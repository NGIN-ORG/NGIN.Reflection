#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/NameUtils.hpp>
#include <cstring>
#include <optional>

namespace NGIN::Reflection::detail
{

  static Registry g_registry{};

  Registry &GetRegistry() noexcept { return g_registry; }
  namespace
  {
    constexpr NameId InvalidNameId = static_cast<NameId>(StringInterner::INVALID_ID);
  }

  NameId InternNameId(std::string_view s) noexcept
  {
    auto &reg = GetRegistry();
    const auto id = reg.names.InsertOrGet(s);
    if (id == StringInterner::INVALID_ID)
      return InvalidNameId;
    return static_cast<NameId>(id);
  }

  bool FindNameId(std::string_view s, NameId &out) noexcept
  {
    auto &reg = GetRegistry();
    StringInterner::IdType id{};
    if (!reg.names.TryGetId(s, id))
      return false;
    out = static_cast<NameId>(id);
    return true;
  }

  std::string_view NameFromId(NameId id) noexcept
  {
    auto &reg = GetRegistry();
    return reg.names.View(static_cast<StringInterner::IdType>(id));
  }

  std::string_view InternName(std::string_view s) noexcept
  {
    auto &reg = GetRegistry();
    return reg.names.Intern(s);
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
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.fieldIndex.GetPtr(nid))
        return Field{FieldHandle{m_h.index, *p}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "field not found"});
  }

  std::optional<Field> Type::FindField(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.fieldIndex.GetPtr(nid))
        return Field{FieldHandle{m_h.index, *p}};
    }
    return std::nullopt;
  }

  NGIN::UIntSize Type::PropertyCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].properties.Size();
  }

  Property Type::PropertyAt(NGIN::UIntSize i) const
  {
    return Property{PropertyHandle{m_h.index, static_cast<NGIN::UInt32>(i)}};
  }

  ExpectedProperty Type::GetProperty(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.propertyIndex.GetPtr(nid))
        return Property{PropertyHandle{m_h.index, *p}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "property not found"});
  }

  std::optional<Property> Type::FindProperty(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.propertyIndex.GetPtr(nid))
        return Property{PropertyHandle{m_h.index, *p}};
    }
    return std::nullopt;
  }

  bool Type::IsEnum() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].enumInfo.isEnum;
  }

  NGIN::UInt64 Type::EnumUnderlyingTypeId() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].enumInfo.underlyingTypeId;
  }

  NGIN::UIntSize Type::EnumValueCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].enumInfo.values.Size();
  }

  EnumValue Type::EnumValueAt(NGIN::UIntSize i) const
  {
    return EnumValue{EnumValueHandle{m_h.index, static_cast<NGIN::UInt32>(i)}};
  }

  ExpectedEnumValue Type::GetEnumValue(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.enumInfo.valueIndex.GetPtr(nid))
        return EnumValue{EnumValueHandle{m_h.index, *p}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "enum value not found"});
  }

  std::optional<EnumValue> Type::FindEnumValue(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.enumInfo.valueIndex.GetPtr(nid))
        return EnumValue{EnumValueHandle{m_h.index, *p}};
    }
    return std::nullopt;
  }

  std::expected<Any, Error> Type::ParseEnum(std::string_view name) const
  {
    auto v = GetEnumValue(name);
    if (!v.has_value())
      return std::unexpected(v.error());
    return v->Value();
  }

  std::optional<std::string_view> Type::EnumName(const Any &value) const
  {
    const auto &reg = GetRegistry();
    const auto &info = reg.types[m_h.index].enumInfo;
    if (!info.isEnum)
      return std::nullopt;
    if (info.isSigned)
    {
      if (!info.toSigned)
        return std::nullopt;
      auto r = info.toSigned(value);
      if (!r.has_value())
        return std::nullopt;
      for (NGIN::UIntSize i = 0; i < info.values.Size(); ++i)
      {
        if (info.values[i].svalue == r.value())
          return info.values[i].name;
      }
    }
    else
    {
      if (!info.toUnsigned)
        return std::nullopt;
      auto r = info.toUnsigned(value);
      if (!r.has_value())
        return std::nullopt;
      for (NGIN::UIntSize i = 0; i < info.values.Size(); ++i)
      {
        if (info.values[i].uvalue == r.value())
          return info.values[i].name;
      }
    }
    return std::nullopt;
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
    return Any::MakeVoid();
  }

  std::expected<void, Error> Field::SetAny(void *obj, const Any &value) const
  {
    const auto &reg = GetRegistry();
    const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
    if (f.store)
      return f.store(obj, value);
    if (value.GetTypeId() != f.typeId)
    {
      return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
    }
    void *dst = f.GetMut(obj);
    if (value.Size() != f.sizeBytes)
    {
      return std::unexpected(Error{ErrorCode::InvalidArgument, "size mismatch"});
    }
    std::memcpy(dst, value.Data(), f.sizeBytes);
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

  // Property
  std::string_view Property::name() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].properties[m_h.propertyIndex].name;
  }

  NGIN::UInt64 Property::type_id() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].properties[m_h.propertyIndex].typeId;
  }

  Any Property::GetAny(const void *obj) const
  {
    const auto &reg = GetRegistry();
    const auto &p = reg.types[m_h.typeIndex].properties[m_h.propertyIndex];
    if (p.get)
      return p.get(obj);
    return Any::MakeVoid();
  }

  std::expected<void, Error> Property::SetAny(void *obj, const Any &value) const
  {
    const auto &reg = GetRegistry();
    const auto &p = reg.types[m_h.typeIndex].properties[m_h.propertyIndex];
    if (!p.set)
      return std::unexpected(Error{ErrorCode::InvalidArgument, "property is read-only"});
    return p.set(obj, value);
  }

  NGIN::UIntSize Property::attribute_count() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].properties[m_h.propertyIndex].attributes.Size();
  }

  AttributeView Property::attribute_at(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.typeIndex].properties[m_h.propertyIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Property::attribute(std::string_view key) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.typeIndex].properties[m_h.propertyIndex].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  // EnumValue
  std::string_view EnumValue::name() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].enumInfo.values[m_h.valueIndex].name;
  }

  Any EnumValue::Value() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].enumInfo.values[m_h.valueIndex].value;
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

  std::expected<Any, Error> Method::Invoke(void *obj, const Any *args, NGIN::UIntSize count) const
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

  // Function
  std::string_view Function::GetName() const
  {
    const auto &reg = GetRegistry();
    return reg.functions[m_h.index].name;
  }

  NGIN::UIntSize Function::GetParameterCount() const
  {
    const auto &reg = GetRegistry();
    return reg.functions[m_h.index].paramTypeIds.Size();
  }

  NGIN::UInt64 Function::GetTypeId() const
  {
    const auto &reg = GetRegistry();
    return reg.functions[m_h.index].returnTypeId;
  }

  std::expected<Any, Error> Function::Invoke(const Any *args, NGIN::UIntSize count) const
  {
    const auto &reg = GetRegistry();
    return reg.functions[m_h.index].Invoke(args, count);
  }

  NGIN::UIntSize Function::attribute_count() const
  {
    const auto &reg = GetRegistry();
    return reg.functions[m_h.index].attributes.Size();
  }

  AttributeView Function::attribute_at(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &a = reg.functions[m_h.index].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Function::attribute(std::string_view key) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.functions[m_h.index].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  // Constructor
  NGIN::UIntSize Constructor::ParameterCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].constructors[m_h.ctorIndex].paramTypeIds.Size();
  }

  std::expected<Any, Error> Constructor::Construct(const Any *args, NGIN::UIntSize count) const
  {
    const auto &reg = GetRegistry();
    const auto &c = reg.types[m_h.typeIndex].constructors[m_h.ctorIndex];
    if (!c.construct)
      return std::unexpected(Error{ErrorCode::NotFound, "constructor not available"});
    return c.construct(args, count);
  }

  NGIN::UIntSize Constructor::attribute_count() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].constructors[m_h.ctorIndex].attributes.Size();
  }

  AttributeView Constructor::attribute_at(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.typeIndex].constructors[m_h.ctorIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Constructor::attribute(std::string_view key) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.typeIndex].constructors[m_h.ctorIndex].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  // Queries
  NGIN::UIntSize FunctionCount()
  {
    const auto &reg = GetRegistry();
    return reg.functions.Size();
  }

  Function FunctionAt(NGIN::UIntSize i)
  {
    return Function{FunctionHandle{static_cast<NGIN::UInt32>(i)}};
  }

  ExpectedFunction GetFunction(std::string_view name)
  {
    const auto &reg = GetRegistry();
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *vec = reg.functionOverloads.GetPtr(nid))
      {
        if (vec->Size() > 0)
          return Function{FunctionHandle{(*vec)[0]}};
      }
    }
    return std::unexpected(Error{ErrorCode::NotFound, "function not found"});
  }

  std::optional<Function> FindFunction(std::string_view name)
  {
    const auto &reg = GetRegistry();
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *vec = reg.functionOverloads.GetPtr(nid))
      {
        if (vec->Size() > 0)
          return Function{FunctionHandle{(*vec)[0]}};
      }
    }
    return std::nullopt;
  }

  FunctionOverloads FindFunctions(std::string_view name)
  {
    const auto &reg = GetRegistry();
    NameId nid{};
    if (!detail::FindNameId(name, nid))
      return FunctionOverloads{};
    const auto *vec = reg.functionOverloads.GetPtr(nid);
    if (!vec)
      return FunctionOverloads{};
    return FunctionOverloads{vec};
  }

  ExpectedType GetType(std::string_view name)
  {
    auto &reg = GetRegistry();
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = reg.byName.GetPtr(nid))
        return Type{TypeHandle{*p}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "type not found"});
  }

  std::optional<Type> FindType(std::string_view name)
  {
    auto &reg = GetRegistry();
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = reg.byName.GetPtr(nid))
        return Type{TypeHandle{*p}};
    }
    return std::nullopt;
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

  std::optional<Method> Type::FindMethod(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.index].methods;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].name == name)
        return Method{m_h.index, static_cast<NGIN::UInt32>(i)};
    return std::nullopt;
  }

  MethodOverloads Type::FindMethods(std::string_view name) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (!detail::FindNameId(name, nid))
      return MethodOverloads{};
    const auto *vec = tdesc.methodOverloads.GetPtr(nid);
    if (!vec)
      return MethodOverloads{};
    return MethodOverloads{m_h.index, vec};
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

  std::expected<ResolvedMethod, Error> Type::ResolveMethod(std::string_view name, const Any *args, NGIN::UIntSize count) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (!detail::FindNameId(name, nid))
      return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
    auto *vec = tdesc.methodOverloads.GetPtr(nid);
    if (!vec)
      return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
    NGIN::UInt32 bestIdx = static_cast<NGIN::UInt32>(-1);
    NGIN::UInt32 closestIdx = static_cast<NGIN::UInt32>(-1);
    int closestScore = INT_MAX;
    struct Key
    {
      int total;
      int nar;
      int conv;
      NGIN::UInt32 idx;
    };
    Key best{INT_MAX, INT_MAX, INT_MAX, 0};
    NGIN::Containers::Vector<OverloadDiagnostic> diags;
    diags.Reserve(vec->Size());
    for (NGIN::UIntSize k = 0; k < vec->Size(); ++k)
    {
      auto mi = (*vec)[k];
      const auto &m = tdesc.methods[mi];
      OverloadDiagnostic diag{};
      diag.methodIndex = mi;
      diag.name = m.name;
      diag.arity = m.paramTypeIds.Size();
      if (m.paramTypeIds.Size() != count)
      {
        diag.code = DiagnosticCode::ArityMismatch;
        auto diff = m.paramTypeIds.Size() > count ? m.paramTypeIds.Size() - count : count - m.paramTypeIds.Size();
        diag.totalCost = 10000 + static_cast<int>(diff);
        if (diag.totalCost < closestScore)
        {
          closestScore = diag.totalCost;
          closestIdx = mi;
        }
        diags.PushBack(std::move(diag));
        continue;
      }
      int total = 0;
      int nar = 0;
      int conv = 0;
      bool ok = true;
      for (NGIN::UIntSize i = 0; i < count; ++i)
      {
        auto want = m.paramTypeIds[i];
        auto have = args[i].GetTypeId();
        auto d = ParamScore(have, want);
        if (d.cost >= 1000)
        {
          ok = false;
          diag.code = DiagnosticCode::NonConvertible;
          diag.argIndex = i;
          diag.totalCost = 20000 + static_cast<int>(i);
          break;
        }
        total += d.cost;
        nar += d.narrow;
        conv += d.conv;
      }
      if (ok)
      {
        diag.code = DiagnosticCode::None;
        diag.totalCost = total;
        diag.narrow = nar;
        diag.conversions = conv;
        Key cur{total, nar, conv, static_cast<NGIN::UInt32>(k)};
        if (std::tuple{cur.total, cur.nar, cur.conv, cur.idx} < std::tuple{best.total, best.nar, best.conv, best.idx})
        {
          best = cur;
          bestIdx = mi;
        }
      }
      if (diag.code == DiagnosticCode::None && diag.totalCost < closestScore)
      {
        closestScore = diag.totalCost;
        closestIdx = mi;
      }
      if (diag.code == DiagnosticCode::NonConvertible && diag.totalCost < closestScore)
      {
        closestScore = diag.totalCost;
        closestIdx = mi;
      }
      diags.PushBack(std::move(diag));
    }
    if (bestIdx == static_cast<NGIN::UInt32>(-1))
    {
      Error err{ErrorCode::InvalidArgument, "no viable overload", std::move(diags)};
      if (closestIdx != static_cast<NGIN::UInt32>(-1))
        err.closestMethodIndex = closestIdx;
      return std::unexpected(std::move(err));
    }
    NGIN::Containers::Vector<NGIN::UInt64> argTypeIds;
    NGIN::Containers::Vector<detail::ConversionKind> conversions;
    if (count > 0)
    {
      argTypeIds.Reserve(count);
      conversions.Reserve(count);
      const auto &m = tdesc.methods[bestIdx];
      for (NGIN::UIntSize i = 0; i < count; ++i)
      {
        argTypeIds.PushBack(args[i].GetTypeId());
        conversions.PushBack(args[i].GetTypeId() == m.paramTypeIds[i] ? detail::ConversionKind::Exact
                                                                      : detail::ConversionKind::Convert);
      }
    }
    return ResolvedMethod{m_h.index, bestIdx, std::move(argTypeIds), std::move(conversions)};
  }

  std::expected<ResolvedFunction, Error> ResolveFunction(std::string_view name, const Any *args, NGIN::UIntSize count)
  {
    const auto &reg = GetRegistry();
    NameId nid{};
    if (!detail::FindNameId(name, nid))
      return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
    auto *vec = reg.functionOverloads.GetPtr(nid);
    if (!vec)
      return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
    NGIN::UInt32 bestIdx = static_cast<NGIN::UInt32>(-1);
    NGIN::UInt32 closestIdx = static_cast<NGIN::UInt32>(-1);
    int closestScore = INT_MAX;
    struct Key
    {
      int total;
      int nar;
      int conv;
      NGIN::UInt32 idx;
    };
    Key best{INT_MAX, INT_MAX, INT_MAX, 0};
    NGIN::Containers::Vector<OverloadDiagnostic> diags;
    diags.Reserve(vec->Size());
    for (NGIN::UIntSize k = 0; k < vec->Size(); ++k)
    {
      auto fi = (*vec)[k];
      const auto &f = reg.functions[fi];
      OverloadDiagnostic diag{};
      diag.methodIndex = fi;
      diag.name = f.name;
      diag.arity = f.paramTypeIds.Size();
      if (f.paramTypeIds.Size() != count)
      {
        diag.code = DiagnosticCode::ArityMismatch;
        auto diff = f.paramTypeIds.Size() > count ? f.paramTypeIds.Size() - count : count - f.paramTypeIds.Size();
        diag.totalCost = 10000 + static_cast<int>(diff);
        if (diag.totalCost < closestScore)
        {
          closestScore = diag.totalCost;
          closestIdx = fi;
        }
        diags.PushBack(std::move(diag));
        continue;
      }
      int total = 0;
      int nar = 0;
      int conv = 0;
      bool ok = true;
      for (NGIN::UIntSize i = 0; i < count; ++i)
      {
        auto want = f.paramTypeIds[i];
        auto have = args[i].GetTypeId();
        auto d = ParamScore(have, want);
        if (d.cost >= 1000)
        {
          ok = false;
          diag.code = DiagnosticCode::NonConvertible;
          diag.argIndex = i;
          diag.totalCost = 20000 + static_cast<int>(i);
          break;
        }
        total += d.cost;
        nar += d.narrow;
        conv += d.conv;
      }
      if (ok)
      {
        diag.code = DiagnosticCode::None;
        diag.totalCost = total;
        diag.narrow = nar;
        diag.conversions = conv;
        Key cur{total, nar, conv, static_cast<NGIN::UInt32>(k)};
        if (std::tuple{cur.total, cur.nar, cur.conv, cur.idx} < std::tuple{best.total, best.nar, best.conv, best.idx})
        {
          best = cur;
          bestIdx = fi;
        }
      }
      if (diag.code == DiagnosticCode::None && diag.totalCost < closestScore)
      {
        closestScore = diag.totalCost;
        closestIdx = fi;
      }
      if (diag.code == DiagnosticCode::NonConvertible && diag.totalCost < closestScore)
      {
        closestScore = diag.totalCost;
        closestIdx = fi;
      }
      diags.PushBack(std::move(diag));
    }
    if (bestIdx == static_cast<NGIN::UInt32>(-1))
    {
      Error err{ErrorCode::InvalidArgument, "no viable overload", std::move(diags)};
      if (closestIdx != static_cast<NGIN::UInt32>(-1))
        err.closestMethodIndex = closestIdx;
      return std::unexpected(std::move(err));
    }
    NGIN::Containers::Vector<NGIN::UInt64> argTypeIds;
    NGIN::Containers::Vector<detail::ConversionKind> conversions;
    if (count > 0)
    {
      argTypeIds.Reserve(count);
      conversions.Reserve(count);
      const auto &f = reg.functions[bestIdx];
      for (NGIN::UIntSize i = 0; i < count; ++i)
      {
        argTypeIds.PushBack(args[i].GetTypeId());
        conversions.PushBack(args[i].GetTypeId() == f.paramTypeIds[i] ? detail::ConversionKind::Exact
                                                                      : detail::ConversionKind::Convert);
      }
    }
    return ResolvedFunction{bestIdx, std::move(argTypeIds), std::move(conversions)};
  }

  // Constructors
  NGIN::UIntSize Type::ConstructorCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].constructors.Size();
  }

  Constructor Type::ConstructorAt(NGIN::UIntSize i) const
  {
    return Constructor{ConstructorHandle{m_h.index, static_cast<NGIN::UInt32>(i)}};
  }

  std::expected<Any, Error> Type::Construct(const Any *args, NGIN::UIntSize count) const
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
        auto have = args[k].GetTypeId();
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

  NGIN::UIntSize Type::MemberCount() const
  {
    const auto &reg = GetRegistry();
    const auto &t = reg.types[m_h.index];
    return t.fields.Size() + t.properties.Size() + t.methods.Size() + t.constructors.Size();
  }

  Member Type::MemberAt(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &t = reg.types[m_h.index];
    const auto fCount = t.fields.Size();
    const auto pCount = t.properties.Size();
    const auto mCount = t.methods.Size();
    if (i < fCount)
      return Member{MemberHandle{MemberKind::Field, m_h.index, static_cast<NGIN::UInt32>(i)}};
    i -= fCount;
    if (i < pCount)
      return Member{MemberHandle{MemberKind::Property, m_h.index, static_cast<NGIN::UInt32>(i)}};
    i -= pCount;
    if (i < mCount)
      return Member{MemberHandle{MemberKind::Method, m_h.index, static_cast<NGIN::UInt32>(i)}};
    i -= mCount;
    if (i < t.constructors.Size())
      return Member{MemberHandle{MemberKind::Constructor, m_h.index, static_cast<NGIN::UInt32>(i)}};
    return Member{};
  }

  NGIN::UIntSize Type::BaseCount() const
  {
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].bases.Size();
  }

  Base Type::BaseAt(NGIN::UIntSize i) const
  {
    return Base{BaseHandle{m_h.index, static_cast<NGIN::UInt32>(i)}};
  }

  ExpectedBase Type::GetBase(const Type &base) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    const auto tid = base.GetTypeId();
    if (auto *p = tdesc.baseIndex.GetPtr(tid))
      return Base{BaseHandle{m_h.index, *p}};
    return std::unexpected(Error{ErrorCode::NotFound, "base type not found"});
  }

  std::optional<Base> Type::FindBase(const Type &base) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    const auto tid = base.GetTypeId();
    if (auto *p = tdesc.baseIndex.GetPtr(tid))
      return Base{BaseHandle{m_h.index, *p}};
    return std::nullopt;
  }

  bool Type::IsDerivedFrom(const Type &base) const
  {
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    const auto tid = base.GetTypeId();
    return tdesc.baseIndex.GetPtr(tid) != nullptr;
  }

  Type Base::BaseType() const
  {
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    return Type{TypeHandle{b.baseTypeIndex}};
  }

  void *Base::Upcast(void *obj) const
  {
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.upcast)
      return nullptr;
    return b.upcast(obj);
  }

  const void *Base::Upcast(const void *obj) const
  {
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.upcastConst)
      return nullptr;
    return b.upcastConst(obj);
  }

  void *Base::Downcast(void *obj) const
  {
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.downcast)
      return nullptr;
    return b.downcast(obj);
  }

  const void *Base::Downcast(const void *obj) const
  {
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.downcastConst)
      return nullptr;
    return b.downcastConst(obj);
  }

  bool Base::CanDowncast() const
  {
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    return b.downcast != nullptr || b.downcastConst != nullptr;
  }

} // namespace NGIN::Reflection
