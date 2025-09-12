#include <NGIN/Reflection/ABIMerge.hpp>
#include <NGIN/Reflection/Registry.hpp>

using namespace NGIN::Reflection;
using namespace NGIN::Reflection::detail;

namespace
{
  static constexpr std::uint32_t kV1 = 1u;
}

bool NGIN::Reflection::MergeRegistryV1(const NGINReflectionRegistryV1 &module,
                                       MergeStats *stats,
                                       const char **error) noexcept
{
  if (!module.header || !module.blob)
  {
    if (error) *error = "null registry";
    return false;
  }
  const auto &h = *module.header;
  if (h.version != kV1)
  {
    if (error) *error = "unsupported version";
    return false;
  }
  // Basic bounds sanity: sections within blob
  auto within = [&](std::uint64_t off, std::uint64_t sz) -> bool {
    if (off == 0 && sz == 0) return true; // allow empty
    const std::uint64_t end = off + sz;
    return end <= module.blobSize && off <= module.blobSize;
  };
  const std::uint64_t typesSize   = h.typeCount    * sizeof(NGINReflectionTypeV1);
  const std::uint64_t fieldsSize  = h.fieldCount   * sizeof(NGINReflectionFieldV1);
  const std::uint64_t methodsSize = h.methodCount  * sizeof(NGINReflectionMethodV1);
  const std::uint64_t ctorsSize   = h.ctorCount    * sizeof(NGINReflectionCtorV1);
  const std::uint64_t attrsSize   = h.attributeCount * sizeof(NGINReflectionAttrV1);
  const std::uint64_t paramsSize  = h.paramCount   * sizeof(std::uint64_t);
  if (!within(h.typesOff, typesSize) ||
      !within(h.fieldsOff, fieldsSize) ||
      !within(h.methodsOff, methodsSize) ||
      !within(h.ctorsOff, ctorsSize) ||
      !within(h.attrsOff, attrsSize) ||
      !within(h.paramsOff, paramsSize) ||
      !within(h.stringsOff, h.stringBytes))
  {
    if (error) *error = "corrupt offsets";
    return false;
  }

  // Map raw pointers to sections
  const auto *base = static_cast<const std::uint8_t *>(module.blob);
  const auto *types   = reinterpret_cast<const NGINReflectionTypeV1  *>(base + h.typesOff);
  const auto *fields  = reinterpret_cast<const NGINReflectionFieldV1 *>(base + h.fieldsOff);
  const auto *methods = reinterpret_cast<const NGINReflectionMethodV1*>(base + h.methodsOff);
  const auto *ctors   = reinterpret_cast<const NGINReflectionCtorV1  *>(base + h.ctorsOff);
  const auto *attrs   = reinterpret_cast<const NGINReflectionAttrV1  *>(base + h.attrsOff);
  const auto *params  = reinterpret_cast<const std::uint64_t         *>(base + h.paramsOff);
  const auto *strings = reinterpret_cast<const char                   *>(base + h.stringsOff);
  const auto *methodFp= (h.methodInvokeOff ? reinterpret_cast<const NGINReflectionMethodInvokeFnV1 *>(base + h.methodInvokeOff) : nullptr);
  const auto *ctorFp  = (h.ctorConstructOff ? reinterpret_cast<const NGINReflectionCtorConstructFnV1 *>(base + h.ctorConstructOff) : nullptr);

  auto view = [&](NGINReflectionStrRefV1 r) -> std::string_view {
    if (r.size == 0) return {};
    return std::string_view{strings + r.offset, r.size};
  };

  auto convertAttr = [&](const NGINReflectionAttrV1 &a) -> AttributeDesc {
    AttributeDesc out{};
    out.key = InternName(view(a.key));
    switch (a.kind)
    {
      case NGINReflectionAttrKindV1::Bool: out.value = static_cast<bool>(a.value.b8 != 0u); break;
      case NGINReflectionAttrKindV1::Int:  out.value = static_cast<std::int64_t>(a.value.i64); break;
      case NGINReflectionAttrKindV1::Dbl:  out.value = static_cast<double>(a.value.d); break;
      case NGINReflectionAttrKindV1::Str:  out.value = InternName(view(a.value.sref)); break;
      case NGINReflectionAttrKindV1::Type: out.value = static_cast<NGIN::UInt64>(a.value.typeId); break;
      default: break;
    }
    return out;
  };

  auto &reg = GetRegistry();
  std::uint64_t added = 0, conflicted = 0;

  std::uint32_t methodGlobalIdx = 0;
  std::uint32_t ctorGlobalIdx = 0;

  for (std::uint64_t i = 0; i < h.typeCount; ++i)
  {
    const auto &ti = types[i];
    const auto typeId = static_cast<NGIN::UInt64>(ti.typeId);
    if (reg.byTypeId.GetPtr(typeId))
    {
      ++conflicted;
      continue;
    }

    TypeRuntimeDesc rec{};
    rec.qualifiedNameId = InternNameId(view(ti.qualifiedName));
    rec.qualifiedName = NameFromId(rec.qualifiedNameId);
    rec.typeId = typeId;
    rec.sizeBytes = ti.sizeBytes;
    rec.alignBytes = ti.alignBytes;

    // Fields
    if (ti.fieldCount)
    {
      rec.fields = {};
      rec.fields.Reserve(ti.fieldCount);
      for (std::uint32_t f = 0; f < ti.fieldCount; ++f)
      {
        const auto &fi = fields[ti.fieldBegin + f];
        FieldRuntimeDesc fd{};
        fd.name = InternName(view(fi.name));
        fd.nameId = InternNameId(fd.name);
        fd.typeId = fi.typeId;
        fd.sizeBytes = fi.sizeBytes;
        if (fi.attrCount)
        {
          fd.attributes.Reserve(fi.attrCount);
          for (std::uint32_t a = 0; a < fi.attrCount; ++a)
            fd.attributes.PushBack(convertAttr(attrs[fi.attrBegin + a]));
        }
        rec.fieldIndex.Insert(fd.nameId, static_cast<NGIN::UInt32>(rec.fields.Size()));
        rec.fields.PushBack(std::move(fd));
      }
    }

    // Methods
    if (ti.methodCount)
    {
      rec.methods.Reserve(ti.methodCount);
      for (std::uint32_t m = 0; m < ti.methodCount; ++m)
      {
        const auto &mi = methods[ti.methodBegin + m];
        MethodRuntimeDesc md{};
        md.name = InternName(view(mi.name));
        const auto nameId = InternNameId(md.name);
        md.nameId = nameId;
        md.returnTypeId = mi.returnTypeId;
        if (mi.paramCount)
        {
          md.paramTypeIds.Reserve(mi.paramCount);
          for (std::uint32_t k = 0; k < mi.paramCount; ++k)
            md.paramTypeIds.PushBack(params[mi.paramBegin + k]);
        }
        if (mi.attrCount)
        {
          md.attributes.Reserve(mi.attrCount);
          for (std::uint32_t a = 0; a < mi.attrCount; ++a)
            md.attributes.PushBack(convertAttr(attrs[mi.attrBegin + a]));
        }
        auto methodIdx = static_cast<NGIN::UInt32>(rec.methods.Size());
        rec.methods.PushBack(std::move(md));
        // Attach function pointer if present
        if (methodFp)
          rec.methods[methodIdx].Invoke = methodFp[methodGlobalIdx];
        ++methodGlobalIdx;
        if (auto *vec = rec.methodOverloads.GetPtr(nameId))
          vec->PushBack(methodIdx);
        else
        {
          NGIN::Containers::Vector<NGIN::UInt32> v;
          v.PushBack(methodIdx);
          rec.methodOverloads.Insert(nameId, std::move(v));
        }
      }
    }

    // Constructors
    if (ti.ctorCount)
    {
      rec.constructors.Reserve(ti.ctorCount);
      for (std::uint32_t c = 0; c < ti.ctorCount; ++c)
      {
        const auto &ci = ctors[ti.ctorBegin + c];
        CtorRuntimeDesc cd{};
        if (ci.paramCount)
        {
          cd.paramTypeIds.Reserve(ci.paramCount);
          for (std::uint32_t k = 0; k < ci.paramCount; ++k)
            cd.paramTypeIds.PushBack(params[ci.paramBegin + k]);
        }
        if (ci.attrCount)
        {
          cd.attributes.Reserve(ci.attrCount);
          for (std::uint32_t a = 0; a < ci.attrCount; ++a)
            cd.attributes.PushBack(convertAttr(attrs[ci.attrBegin + a]));
        }
        rec.constructors.PushBack(std::move(cd));
        if (ctorFp)
          rec.constructors[rec.constructors.Size() - 1].construct = ctorFp[ctorGlobalIdx];
        ++ctorGlobalIdx;
      }
    }

    // Type attributes
    if (ti.attrCount)
    {
      rec.attributes.Reserve(ti.attrCount);
      for (std::uint32_t a = 0; a < ti.attrCount; ++a)
        rec.attributes.PushBack(convertAttr(attrs[ti.attrBegin + a]));
    }

    // Commit to registry
    const auto idx = static_cast<NGIN::UInt32>(reg.types.Size());
    reg.types.PushBack(std::move(rec));
    reg.byTypeId.Insert(typeId, idx);
    reg.byName.Insert(reg.types[idx].qualifiedNameId, idx);
#if defined(_MSC_VER)
    // Add alias without MSVC "class ", "struct ", etc. prefix for portable lookups
    {
      auto qn = reg.types[idx].qualifiedName;
      auto add_alias = [&](std::string_view prefix) {
        if (qn.size() > prefix.size() && qn.substr(0, prefix.size()) == prefix)
        {
          auto trimmed = qn.substr(prefix.size());
          auto aliasId = InternNameId(trimmed);
          reg.byName.Insert(aliasId, idx);
        }
      };
      add_alias("class ");
      add_alias("struct ");
      add_alias("enum ");
      add_alias("union ");
    }
#endif
    ++added;
  }

  if (stats)
  {
    stats->modulesMerged += 1;
    stats->typesAdded += added;
    stats->typesConflicted += conflicted;
  }
  return true;
}
