#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/NameUtils.hpp>
#include <NGIN/Reflection/Any.hpp>
#include <cstring>
#include <memory>

namespace NGIN::Reflection::detail
{

  static Registry g_registry{};

  Registry &GetRegistry() noexcept { return g_registry; }

  // Minimal string interner (prototype): stores unique strings with stable lifetime
  std::string_view InternName(std::string_view s) noexcept
  {
    auto &reg = GetRegistry();
    const auto h = NGIN::Hashing::FNV1a64(s.data(), s.size());
    if (auto bucket = reg.internBuckets.GetPtr(h))
    {
      for (NGIN::UIntSize i = 0; i < bucket->Size(); ++i)
      {
        std::string_view v = (*bucket)[i];
        if (v.size() == s.size() && std::memcmp(v.data(), s.data(), s.size()) == 0)
          return v; // already interned
      }
    }
    // Not found: store and index
    auto up = std::make_unique<std::string>(s);
    std::string_view view{up->data(), up->size()};
    reg.stringStore.PushBack(std::move(up));
    if (auto bucket = reg.internBuckets.GetPtr(h))
    {
      bucket->PushBack(view);
    }
    else
    {
      NGIN::Containers::Vector<std::string_view> v;
      v.PushBack(view);
      reg.internBuckets.Insert(h, std::move(v));
    }
    return view;
  }

} // namespace NGIN::Reflection::detail

namespace NGIN::Reflection
{

  using detail::GetRegistry;

  // Type
  std::string_view Type::QualifiedName() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].qualifiedName;
  }

  NGIN::UInt64 Type::GetTypeId() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].typeId;
  }

  NGIN::UIntSize Type::Size() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].sizeBytes;
  }

  NGIN::UIntSize Type::Alignment() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].alignBytes;
  }

  NGIN::UIntSize Type::FieldCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].fields.Size();
  }

  Field Type::FieldAt(NGIN::UIntSize i) const
  {
    return Field{FieldHandle{m_h.index, static_cast<NGIN::UInt32>(i)}};
  }

  ExpectedField Type::GetField(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.index].fields;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
    {
      if (v[i].name == name)
        return Field{FieldHandle{m_h.index, static_cast<NGIN::UInt32>(i)}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "field not found"});
  }

  // Field
  std::string_view Field::name() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].name;
  }

  NGIN::UInt64 Field::type_id() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].typeId;
  }

  void *Field::GetMut(void *obj) const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].GetMut(obj);
  }

  const void *Field::GetConst(const void *obj) const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].GetConst(obj);
  }

  Any Field::GetAny(const void *obj) const
  {
    const auto &reg = GetRegistry();
    const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
    if (f.load)
      return f.load(obj);
    return Any::make_void();
  }

  std::expected<void, Error> Field::SetAny(void *obj, const Any &value) const
  {
    const auto &reg = GetRegistry();
    const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
    if (f.store)
      return f.store(obj, value);
    if (value.type_id() != f.typeId)
    {
      return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
    }
    void *dst = f.GetMut(obj);
    if (value.size() != f.sizeBytes)
    {
      return std::unexpected(Error{ErrorCode::InvalidArgument, "size mismatch"});
    }
    std::memcpy(dst, value.raw_data(), f.sizeBytes);
    return {};
  }

  NGIN::UIntSize Field::attribute_count() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].attributes.Size();
  }

  AttributeView Field::attribute_at(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.typeIndex].fields[m_h.fieldIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Field::attribute(std::string_view key) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.typeIndex].fields[m_h.fieldIndex].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  // Method
  std::string_view Method::GetName() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_typeIndex].methods[m_methodIndex].name;
  }

  NGIN::UIntSize Method::GetParameterCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_typeIndex].methods[m_methodIndex].paramTypeIds.Size();
  }

  NGIN::UInt64 Method::GetTypeId() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_typeIndex].methods[m_methodIndex].returnTypeId;
  }

  std::expected<class Any, Error> Method::Invoke(void *obj, const class Any *args, NGIN::UIntSize count) const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_typeIndex].methods[m_methodIndex].Invoke(obj, args, count);
  }

  // span-based convenience overloads are defined inline in the header

  NGIN::UIntSize Method::attribute_count() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_typeIndex].methods[m_methodIndex].attributes.Size();
  }

  AttributeView Method::attribute_at(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_typeIndex].methods[m_methodIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Method::attribute(std::string_view key) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_typeIndex].methods[m_methodIndex].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  // Queries
  ExpectedType type(std::string_view qualified_name)
  {
    auto &reg = GetRegistry();
    if (auto *p = reg.byName.GetPtr(qualified_name))
      return Type{TypeHandle{*p}};
    return std::unexpected(Error{ErrorCode::NotFound, "type not found"});
  }

  // Type: methods and attributes
  NGIN::UIntSize Type::MethodCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].methods.Size();
  }

  Method Type::MethodAt(NGIN::UIntSize i) const
  {
    return Method{m_h.index, static_cast<NGIN::UInt32>(i)};
  }

  std::expected<Method, Error> Type::GetMethod(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.index].methods;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].name == name)
        return Method{m_h.index, static_cast<NGIN::UInt32>(i)};
    return std::unexpected(Error{ErrorCode::NotFound, "method not found"});
  }

  enum class NumKind
  {
    None,
    Int,
    UInt,
    Float
  };
  struct NumInfo
  {
    NumKind kind;
    int rank;
  };
  static inline NumInfo NumInfoFromTid(NGIN::UInt64 tid)
  {
    if (tid == detail::TypeIdOf<bool>())
      return {NumKind::UInt, 0};
    if (tid == detail::TypeIdOf<signed char>())
      return {NumKind::Int, 1};
    if (tid == detail::TypeIdOf<unsigned char>())
      return {NumKind::UInt, 1};
    if (tid == detail::TypeIdOf<char>())
      return {NumKind::Int, 1};
    if (tid == detail::TypeIdOf<short>())
      return {NumKind::Int, 2};
    if (tid == detail::TypeIdOf<unsigned short>())
      return {NumKind::UInt, 2};
    if (tid == detail::TypeIdOf<int>())
      return {NumKind::Int, 3};
    if (tid == detail::TypeIdOf<unsigned int>())
      return {NumKind::UInt, 3};
    if (tid == detail::TypeIdOf<long>())
      return {NumKind::Int, 4};
    if (tid == detail::TypeIdOf<unsigned long>())
      return {NumKind::UInt, 4};
    if (tid == detail::TypeIdOf<long long>())
      return {NumKind::Int, 5};
    if (tid == detail::TypeIdOf<unsigned long long>())
      return {NumKind::UInt, 5};
    if (tid == detail::TypeIdOf<float>())
      return {NumKind::Float, 1};
    if (tid == detail::TypeIdOf<double>())
      return {NumKind::Float, 2};
    if (tid == detail::TypeIdOf<long double>())
      return {NumKind::Float, 3};
    return {NumKind::None, -1};
  }

  struct ScoreDims
  {
    int cost;
    int narrow;
    int conv;
  };
  static inline ScoreDims ParamScore(NGIN::UInt64 have, NGIN::UInt64 want)
  {
    if (have == want)
      return {0, 0, 0};
    auto h = NumInfoFromTid(have);
    auto w = NumInfoFromTid(want);
    if (h.kind == NumKind::None || w.kind == NumKind::None)
      return {1000, 0, 0};
    // Promotions: same kind, rank increases
    if (h.kind == w.kind && h.rank <= w.rank)
      return {1, 0, 0};
    // Float <- Int/UInt: conversion
    if (w.kind == NumKind::Float && (h.kind == NumKind::Int || h.kind == NumKind::UInt))
      return {3, 0, 1};
    // Int/UInt <- Float: narrowing conversion
    if ((w.kind == NumKind::Int || w.kind == NumKind::UInt) && h.kind == NumKind::Float)
      return {5, 1, 1};
    // Signedness change or rank decrease: conversion, possibly narrowing
    int narrow = 0;
    if (h.kind != w.kind)
    {
      narrow = (w.kind == NumKind::Int || w.kind == NumKind::UInt) ? 1 : 0;
      return {4, narrow, 1};
    }
    if (h.rank > w.rank)
    {
      narrow = 1;
      return {4, narrow, 1};
    }
    return {3, 0, 1};
  }

  std::expected<Method, Error> Type::ResolveMethod(std::string_view name, const class Any *args, NGIN::UIntSize count) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    auto *vec = tdesc.methodOverloads.GetPtr(name);
    if (!vec)
      return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
    NGIN::UInt32 bestIdx = static_cast<NGIN::UInt32>(-1);
    struct Key
    {
      int total;
      int nar;
      int conv;
      NGIN::UInt32 idx;
    };
    Key best{INT_MAX, INT_MAX, INT_MAX, 0};
    for (NGIN::UIntSize k = 0; k < vec->Size(); ++k)
    {
      auto mi = (*vec)[k];
      const auto &m = tdesc.methods[mi];
      if (m.paramTypeIds.Size() != count)
        continue;
      int total = 0;
      int nar = 0;
      int conv = 0;
      bool ok = true;
      for (NGIN::UIntSize i = 0; i < count; ++i)
      {
        auto want = m.paramTypeIds[i];
        auto have = args[i].type_id();
        auto d = ParamScore(have, want);
        if (d.cost >= 1000)
        {
          ok = false;
          break;
        }
        total += d.cost;
        nar += d.narrow;
        conv += d.conv;
      }
      if (ok)
      {
        Key cur{total, nar, conv, static_cast<NGIN::UInt32>(k)};
        if (std::tuple{cur.total, cur.nar, cur.conv, cur.idx} < std::tuple{best.total, best.nar, best.conv, best.idx})
        {
          best = cur;
          bestIdx = mi;
        }
      }
    }
    if (bestIdx == static_cast<NGIN::UInt32>(-1))
      return std::unexpected(Error{ErrorCode::InvalidArgument, "no viable overload"});
    return Method{m_h.index, bestIdx};
  }

  // Constructors
  NGIN::UIntSize Type::ConstructorCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].constructors.Size();
  }

  std::expected<class Any, Error> Type::Construct(const class Any *args, NGIN::UIntSize count) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    // Fast-path: default constructor
    if (count == 0)
    {
      for (NGIN::UIntSize i = 0; i < tdesc.constructors.Size(); ++i)
      {
        const auto &c = tdesc.constructors[i];
        if (c.paramTypeIds.Size() == 0 && c.construct)
          return c.construct(nullptr, 0);
      }
      return std::unexpected(Error{ErrorCode::NotFound, "no default constructor"});
    }

    // Overload selection (same scoring as methods)
    NGIN::UInt32 bestIdx = static_cast<NGIN::UInt32>(-1);
    struct Key
    {
      int total;
      int nar;
      int conv;
      NGIN::UInt32 idx;
    };
    Key best{INT_MAX, INT_MAX, INT_MAX, 0};
    for (NGIN::UIntSize i = 0; i < tdesc.constructors.Size(); ++i)
    {
      const auto &c = tdesc.constructors[i];
      if (c.paramTypeIds.Size() != count)
        continue;
      int total = 0;
      int nar = 0;
      int conv = 0;
      bool ok = true;
      for (NGIN::UIntSize k = 0; k < count; ++k)
      {
        auto want = c.paramTypeIds[k];
        auto have = args[k].type_id();
        auto d = ParamScore(have, want);
        if (d.cost >= 1000)
        {
          ok = false;
          break;
        }
        total += d.cost;
        nar += d.narrow;
        conv += d.conv;
      }
      if (ok)
      {
        Key cur{total, nar, conv, static_cast<NGIN::UInt32>(i)};
        if (std::tuple{cur.total, cur.nar, cur.conv, cur.idx} < std::tuple{best.total, best.nar, best.conv, best.idx})
        {
          best = cur;
          bestIdx = static_cast<NGIN::UInt32>(i);
        }
      }
    }
    if (bestIdx == static_cast<NGIN::UInt32>(-1))
      return std::unexpected(Error{ErrorCode::InvalidArgument, "no viable constructor"});
    return tdesc.constructors[bestIdx].construct(args, count);
  }

  NGIN::UIntSize Type::AttributeCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].attributes.Size();
  }

  AttributeView Type::AttributeAt(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.index].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Type::Attribute(std::string_view key) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.index].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

} // namespace NGIN::Reflection
