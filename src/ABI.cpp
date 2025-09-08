#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Memory/SystemAllocator.hpp>

#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>

using namespace NGIN::Reflection;
using namespace NGIN::Reflection::detail;

extern "C" bool NGINReflectionExportV1(NGINReflectionRegistryV1 *out)
{
  if (!out)
    return false;

  const auto &reg = GetRegistry();

  // Pass 1: compute counts and gather unique strings
  std::uint64_t typeCount = reg.types.Size();
  std::uint64_t fieldCount = 0, methodCount = 0, ctorCount = 0, attributeCount = 0, paramCount = 0;

  struct HasherStr {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
  };

  // We'll store unique strings in insertion order
  std::vector<std::string> uniqueStrings;
  uniqueStrings.reserve(256);
  std::unordered_map<std::string_view, std::uint64_t, HasherStr, std::equal_to<>> strOffset;
  strOffset.reserve(256);

  auto intern = [&](std::string_view sv) -> NGINReflectionStrRefV1 {
    auto it = strOffset.find(sv);
    if (it != strOffset.end()) {
      return NGINReflectionStrRefV1{it->second, static_cast<std::uint32_t>(sv.size()), 0};
    }
    std::uint64_t off = 0;
    if (!uniqueStrings.empty()) {
      // compute current size
      // defer; we compute offsets later after concatenation to avoid O(n^2)
    }
    // Copy the string to own it; we will compute offsets after building the table
    uniqueStrings.emplace_back(sv);
    // Temporarily store offset as UINT64_MAX to patch later
    strOffset.emplace(sv, UINT64_MAX);
    return NGINReflectionStrRefV1{UINT64_MAX, static_cast<std::uint32_t>(sv.size()), 0};
  };

  // Pre-scan to count and collect strings
  for (NGIN::UIntSize i = 0; i < reg.types.Size(); ++i) {
    const auto &t = reg.types[i];
    fieldCount += t.fields.Size();
    methodCount += t.methods.Size();
    ctorCount += t.constructors.Size();
    attributeCount += t.attributes.Size();
    for (NGIN::UIntSize f = 0; f < t.fields.Size(); ++f) attributeCount += t.fields[f].attributes.Size();
    for (NGIN::UIntSize m = 0; m < t.methods.Size(); ++m) attributeCount += t.methods[m].attributes.Size();
    for (NGIN::UIntSize c = 0; c < t.constructors.Size(); ++c) attributeCount += t.constructors[c].attributes.Size();
    for (NGIN::UIntSize m = 0; m < t.methods.Size(); ++m) paramCount += t.methods[m].paramTypeIds.Size();
    for (NGIN::UIntSize c = 0; c < t.constructors.Size(); ++c) paramCount += t.constructors[c].paramTypeIds.Size();

    // Strings: type name
    (void)intern(t.qualifiedName);
    // Field/method names and attribute keys/values
    for (NGIN::UIntSize f = 0; f < t.fields.Size(); ++f) {
      (void)intern(t.fields[f].name);
      for (NGIN::UIntSize a = 0; a < t.fields[f].attributes.Size(); ++a) {
        (void)intern(t.fields[f].attributes[a].key);
        if (std::holds_alternative<std::string_view>(t.fields[f].attributes[a].value))
          (void)intern(std::get<std::string_view>(t.fields[f].attributes[a].value));
      }
    }
    for (NGIN::UIntSize m = 0; m < t.methods.Size(); ++m) {
      (void)intern(t.methods[m].name);
      for (NGIN::UIntSize a = 0; a < t.methods[m].attributes.Size(); ++a) {
        (void)intern(t.methods[m].attributes[a].key);
        if (std::holds_alternative<std::string_view>(t.methods[m].attributes[a].value))
          (void)intern(std::get<std::string_view>(t.methods[m].attributes[a].value));
      }
    }
    for (NGIN::UIntSize a = 0; a < t.attributes.Size(); ++a) {
      (void)intern(t.attributes[a].key);
      if (std::holds_alternative<std::string_view>(t.attributes[a].value))
        (void)intern(std::get<std::string_view>(t.attributes[a].value));
    }
    for (NGIN::UIntSize c = 0; c < t.constructors.Size(); ++c) {
      for (NGIN::UIntSize a = 0; a < t.constructors[c].attributes.Size(); ++a) {
        (void)intern(t.constructors[c].attributes[a].key);
        if (std::holds_alternative<std::string_view>(t.constructors[c].attributes[a].value))
          (void)intern(std::get<std::string_view>(t.constructors[c].attributes[a].value));
      }
    }
  }

  // Build string table and compute offsets
  std::vector<char> strTable;
  strTable.reserve(1024);
  std::uint64_t running = 0;
  for (auto &owned : uniqueStrings) {
    auto it = strOffset.find(std::string_view{owned});
    if (it != strOffset.end()) {
      it->second = running;
    }
    // append bytes without NUL
    strTable.insert(strTable.end(), owned.data(), owned.data() + owned.size());
    running += static_cast<std::uint64_t>(owned.size());
  }

  // Helper: resolve a string ref using the map
  auto makeSref = [&](std::string_view sv) -> NGINReflectionStrRefV1 {
    auto it = strOffset.find(sv);
    if (it == strOffset.end()) {
      // shouldn't happen; return empty
      return NGINReflectionStrRefV1{0, 0, 0};
    }
    return NGINReflectionStrRefV1{it->second, static_cast<std::uint32_t>(sv.size()), 0};
  };

  // Compute sizes and offsets with 8-byte alignment
  auto align8 = [](std::uint64_t x) { return (x + 7u) & ~std::uint64_t{7u}; };

  const std::uint64_t headerSize = align8(sizeof(NGINReflectionHeaderV1));
  const std::uint64_t typesSize   = align8(typeCount * sizeof(NGINReflectionTypeV1));
  const std::uint64_t fieldsSize  = align8(fieldCount * sizeof(NGINReflectionFieldV1));
  const std::uint64_t methodsSize = align8(methodCount * sizeof(NGINReflectionMethodV1));
  const std::uint64_t ctorsSize   = align8(ctorCount * sizeof(NGINReflectionCtorV1));
  const std::uint64_t attrsSize   = align8(attributeCount * sizeof(NGINReflectionAttrV1));
  const std::uint64_t paramsSize  = align8(paramCount * sizeof(std::uint64_t));
  const std::uint64_t stringsSize = align8(static_cast<std::uint64_t>(strTable.size()));
  const std::uint64_t methodFpSize= align8(methodCount * sizeof(NGINReflectionMethodInvokeFnV1));
  const std::uint64_t ctorFpSize  = align8(ctorCount   * sizeof(NGINReflectionCtorConstructFnV1));

  NGINReflectionHeaderV1 hdr{};
  hdr.version = 1u;
  hdr.flags = 0u;
  hdr.typeCount = typeCount;
  hdr.fieldCount = fieldCount;
  hdr.methodCount = methodCount;
  hdr.ctorCount = ctorCount;
  hdr.attributeCount = attributeCount;
  hdr.paramCount = paramCount;
  hdr.stringBytes = static_cast<std::uint64_t>(strTable.size());

  hdr.typesOff        = headerSize;
  hdr.fieldsOff       = hdr.typesOff + typesSize;
  hdr.methodsOff      = hdr.fieldsOff + fieldsSize;
  hdr.ctorsOff        = hdr.methodsOff + methodsSize;
  hdr.attrsOff        = hdr.ctorsOff + ctorsSize;
  hdr.paramsOff       = hdr.attrsOff + attrsSize;
  hdr.stringsOff      = hdr.paramsOff + paramsSize;
  hdr.methodInvokeOff = hdr.stringsOff + stringsSize;
  hdr.ctorConstructOff= hdr.methodInvokeOff + methodFpSize;
  hdr.totalSize       = hdr.ctorConstructOff + ctorFpSize;

  // Allocate blob and lay out sections
  NGIN::Memory::SystemAllocator alloc{};
  void *mem = alloc.Allocate(static_cast<NGIN::UIntSize>(hdr.totalSize), alignof(std::max_align_t));
  if (!mem) return false;

  auto base = static_cast<std::uint8_t *>(mem);
  // Zero memory for determinism
  std::memset(base, 0, static_cast<std::size_t>(hdr.totalSize));

  // Write arrays
  auto pTypes    = reinterpret_cast<NGINReflectionTypeV1             *>(base + hdr.typesOff);
  auto pFields   = reinterpret_cast<NGINReflectionFieldV1            *>(base + hdr.fieldsOff);
  auto pMethods  = reinterpret_cast<NGINReflectionMethodV1           *>(base + hdr.methodsOff);
  auto pCtors    = reinterpret_cast<NGINReflectionCtorV1             *>(base + hdr.ctorsOff);
  auto pAttrs    = reinterpret_cast<NGINReflectionAttrV1             *>(base + hdr.attrsOff);
  auto pParams   = reinterpret_cast<std::uint64_t                    *>(base + hdr.paramsOff);
  auto pStrings  = reinterpret_cast<std::uint8_t                     *>(base + hdr.stringsOff);
  auto pMethodFp = reinterpret_cast<NGINReflectionMethodInvokeFnV1   *>(base + hdr.methodInvokeOff);
  auto pCtorFp   = reinterpret_cast<NGINReflectionCtorConstructFnV1  *>(base + hdr.ctorConstructOff);

  // Copy string table
  if (!strTable.empty())
    std::memcpy(pStrings, strTable.data(), strTable.size());

  // Fill records
  std::uint32_t fIdx = 0, mIdx = 0, cIdx = 0, aIdx = 0, pIdx = 0;
  for (NGIN::UIntSize i = 0; i < reg.types.Size(); ++i) {
    const auto &t = reg.types[i];
    auto &to = pTypes[i];
    to.typeId = t.typeId;
    to.qualifiedName = makeSref(t.qualifiedName);
    to.sizeBytes = static_cast<std::uint32_t>(t.sizeBytes);
    to.alignBytes = static_cast<std::uint32_t>(t.alignBytes);

    // fields
    to.fieldBegin = fIdx;
    to.fieldCount = static_cast<std::uint32_t>(t.fields.Size());
    for (NGIN::UIntSize f = 0; f < t.fields.Size(); ++f) {
      auto &src = t.fields[f];
      auto &dst = pFields[fIdx++];
      dst.name = makeSref(src.name);
      dst.typeId = src.typeId;
      dst.sizeBytes = static_cast<std::uint32_t>(src.sizeBytes);
      dst.attrBegin = aIdx;
      dst.attrCount = static_cast<std::uint32_t>(src.attributes.Size());
      for (NGIN::UIntSize a = 0; a < src.attributes.Size(); ++a) {
        auto &asrc = src.attributes[a];
        auto &adst = pAttrs[aIdx++];
        adst.key = makeSref(asrc.key);
        if (std::holds_alternative<bool>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Bool; adst.value.b8 = std::get<bool>(asrc.value) ? 1u : 0u;
        } else if (std::holds_alternative<std::int64_t>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Int; adst.value.i64 = std::get<std::int64_t>(asrc.value);
        } else if (std::holds_alternative<double>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Dbl; adst.value.d = std::get<double>(asrc.value);
        } else if (std::holds_alternative<std::string_view>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Str; adst.value.sref = makeSref(std::get<std::string_view>(asrc.value));
        } else /* type id */ {
          adst.kind = NGINReflectionAttrKindV1::Type; adst.value.typeId = std::get<NGIN::UInt64>(asrc.value);
        }
      }
    }

    // methods
    to.methodBegin = mIdx;
    to.methodCount = static_cast<std::uint32_t>(t.methods.Size());
    for (NGIN::UIntSize m = 0; m < t.methods.Size(); ++m) {
      auto &src = t.methods[m];
      auto &dst = pMethods[mIdx++];
      dst.name = makeSref(src.name);
      dst.returnTypeId = src.returnTypeId;
      dst.paramBegin = pIdx;
      dst.paramCount = static_cast<std::uint32_t>(src.paramTypeIds.Size());
      for (NGIN::UIntSize k = 0; k < src.paramTypeIds.Size(); ++k)
        pParams[pIdx++] = src.paramTypeIds[k];
      dst.attrBegin = aIdx;
      dst.attrCount = static_cast<std::uint32_t>(src.attributes.Size());
      for (NGIN::UIntSize a = 0; a < src.attributes.Size(); ++a) {
        auto &asrc = src.attributes[a];
        auto &adst = pAttrs[aIdx++];
        adst.key = makeSref(asrc.key);
        if (std::holds_alternative<bool>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Bool; adst.value.b8 = std::get<bool>(asrc.value) ? 1u : 0u;
        } else if (std::holds_alternative<std::int64_t>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Int; adst.value.i64 = std::get<std::int64_t>(asrc.value);
        } else if (std::holds_alternative<double>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Dbl; adst.value.d = std::get<double>(asrc.value);
        } else if (std::holds_alternative<std::string_view>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Str; adst.value.sref = makeSref(std::get<std::string_view>(asrc.value));
        } else /* type id */ {
          adst.kind = NGINReflectionAttrKindV1::Type; adst.value.typeId = std::get<NGIN::UInt64>(asrc.value);
        }
      }
      // function pointer table entry (parallel to methods array)
      pMethodFp[mIdx - 1] = src.Invoke;
    }

    // ctors
    to.ctorBegin = cIdx;
    to.ctorCount = static_cast<std::uint32_t>(t.constructors.Size());
    for (NGIN::UIntSize c = 0; c < t.constructors.Size(); ++c) {
      auto &src = t.constructors[c];
      auto &dst = pCtors[cIdx++];
      dst.paramBegin = pIdx;
      dst.paramCount = static_cast<std::uint32_t>(src.paramTypeIds.Size());
      for (NGIN::UIntSize k = 0; k < src.paramTypeIds.Size(); ++k)
        pParams[pIdx++] = src.paramTypeIds[k];
      dst.attrBegin = aIdx;
      dst.attrCount = static_cast<std::uint32_t>(src.attributes.Size());
      for (NGIN::UIntSize a = 0; a < src.attributes.Size(); ++a) {
        auto &asrc = src.attributes[a];
        auto &adst = pAttrs[aIdx++];
        adst.key = makeSref(asrc.key);
        if (std::holds_alternative<bool>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Bool; adst.value.b8 = std::get<bool>(asrc.value) ? 1u : 0u;
        } else if (std::holds_alternative<std::int64_t>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Int; adst.value.i64 = std::get<std::int64_t>(asrc.value);
        } else if (std::holds_alternative<double>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Dbl; adst.value.d = std::get<double>(asrc.value);
        } else if (std::holds_alternative<std::string_view>(asrc.value)) {
          adst.kind = NGINReflectionAttrKindV1::Str; adst.value.sref = makeSref(std::get<std::string_view>(asrc.value));
        } else /* type id */ {
          adst.kind = NGINReflectionAttrKindV1::Type; adst.value.typeId = std::get<NGIN::UInt64>(asrc.value);
        }
      }
      // function pointer table entry (parallel to ctors array)
      pCtorFp[cIdx - 1] = src.construct;
    }

    // type-level attributes
    to.attrBegin = aIdx;
    to.attrCount = static_cast<std::uint32_t>(t.attributes.Size());
    for (NGIN::UIntSize a = 0; a < t.attributes.Size(); ++a) {
      auto &asrc = t.attributes[a];
      auto &adst = pAttrs[aIdx++];
      adst.key = makeSref(asrc.key);
      if (std::holds_alternative<bool>(asrc.value)) {
        adst.kind = NGINReflectionAttrKindV1::Bool; adst.value.b8 = std::get<bool>(asrc.value) ? 1u : 0u;
      } else if (std::holds_alternative<std::int64_t>(asrc.value)) {
        adst.kind = NGINReflectionAttrKindV1::Int; adst.value.i64 = std::get<std::int64_t>(asrc.value);
      } else if (std::holds_alternative<double>(asrc.value)) {
        adst.kind = NGINReflectionAttrKindV1::Dbl; adst.value.d = std::get<double>(asrc.value);
      } else if (std::holds_alternative<std::string_view>(asrc.value)) {
        adst.kind = NGINReflectionAttrKindV1::Str; adst.value.sref = makeSref(std::get<std::string_view>(asrc.value));
      } else /* type id */ {
        adst.kind = NGINReflectionAttrKindV1::Type; adst.value.typeId = std::get<NGIN::UInt64>(asrc.value);
      }
    }
  }

  // Sanity: indices should match counts
  (void)fIdx; (void)mIdx; (void)cIdx; (void)aIdx; (void)pIdx;

  // Finally, write header into blob start and publish
  std::memcpy(base, &hdr, sizeof(hdr));

  static NGINReflectionRegistryV1 regOut{}; // stable address for header ptr
  regOut.header = reinterpret_cast<const NGINReflectionHeaderV1 *>(base);
  regOut.blob = base;
  regOut.blobSize = hdr.totalSize;
  *out = regOut;
  return true;
}
