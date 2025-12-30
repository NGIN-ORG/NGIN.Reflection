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
  namespace
  {
    constexpr std::string_view kStaleHandle = "stale handle";

    bool IsTypeAlive(NGIN::UInt32 index, NGIN::UInt32 generation)
    {
      const auto &reg = GetRegistry();
      return index < reg.types.Size() && reg.types[index].generation == generation;
    }

    bool IsTypeAlive(TypeHandle h)
    {
      if (!h.IsValid())
        return false;
      return IsTypeAlive(h.index, h.generation);
    }

    bool IsFieldAlive(FieldHandle h)
    {
      if (!IsTypeAlive(h.typeIndex, h.typeGeneration))
        return false;
      const auto &reg = GetRegistry();
      return h.fieldIndex < reg.types[h.typeIndex].fields.Size();
    }

    bool IsPropertyAlive(PropertyHandle h)
    {
      if (!IsTypeAlive(h.typeIndex, h.typeGeneration))
        return false;
      const auto &reg = GetRegistry();
      return h.propertyIndex < reg.types[h.typeIndex].properties.Size();
    }

    bool IsEnumValueAlive(EnumValueHandle h)
    {
      if (!IsTypeAlive(h.typeIndex, h.typeGeneration))
        return false;
      const auto &reg = GetRegistry();
      return h.valueIndex < reg.types[h.typeIndex].enumInfo.values.Size();
    }

    bool IsCtorAlive(ConstructorHandle h)
    {
      if (!IsTypeAlive(h.typeIndex, h.typeGeneration))
        return false;
      const auto &reg = GetRegistry();
      return h.ctorIndex < reg.types[h.typeIndex].constructors.Size();
    }

    bool IsBaseAlive(BaseHandle h)
    {
      if (!IsTypeAlive(h.typeIndex, h.typeGeneration))
        return false;
      const auto &reg = GetRegistry();
      return h.baseIndex < reg.types[h.typeIndex].bases.Size();
    }
  } // namespace

  // Type
  std::string_view Type::QualifiedName() const
  {
    if (!IsTypeAlive(m_h))
      return {};
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].qualifiedName;
  }

  NGIN::UInt64 Type::GetTypeId() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].typeId;
  }

  NGIN::UIntSize Type::Size() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].sizeBytes;
  }

  NGIN::UIntSize Type::Alignment() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].alignBytes;
  }

  NGIN::UIntSize Type::FieldCount() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].fields.Size();
  }

  Field Type::FieldAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return Field{};
    return Field{FieldHandle{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
  }

  ExpectedField Type::GetField(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.fieldIndex.GetPtr(nid))
        return Field{FieldHandle{m_h.index, *p, m_h.generation}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "field not found"});
  }

  std::optional<Field> Type::FindField(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::nullopt;
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.fieldIndex.GetPtr(nid))
        return Field{FieldHandle{m_h.index, *p, m_h.generation}};
    }
    return std::nullopt;
  }

  NGIN::UIntSize Type::PropertyCount() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].properties.Size();
  }

  Property Type::PropertyAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return Property{};
    return Property{PropertyHandle{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
  }

  ExpectedProperty Type::GetProperty(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.propertyIndex.GetPtr(nid))
        return Property{PropertyHandle{m_h.index, *p, m_h.generation}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "property not found"});
  }

  std::optional<Property> Type::FindProperty(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::nullopt;
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.propertyIndex.GetPtr(nid))
        return Property{PropertyHandle{m_h.index, *p, m_h.generation}};
    }
    return std::nullopt;
  }

  bool Type::IsEnum() const
  {
    if (!IsTypeAlive(m_h))
      return false;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].enumInfo.isEnum;
  }

  NGIN::UInt64 Type::EnumUnderlyingTypeId() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].enumInfo.underlyingTypeId;
  }

  NGIN::UIntSize Type::EnumValueCount() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].enumInfo.values.Size();
  }

  EnumValue Type::EnumValueAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return EnumValue{};
    return EnumValue{EnumValueHandle{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
  }

  ExpectedEnumValue Type::GetEnumValue(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.enumInfo.valueIndex.GetPtr(nid))
        return EnumValue{EnumValueHandle{m_h.index, *p, m_h.generation}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "enum value not found"});
  }

  std::optional<EnumValue> Type::FindEnumValue(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::nullopt;
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (detail::FindNameId(name, nid))
    {
      if (auto *p = tdesc.enumInfo.valueIndex.GetPtr(nid))
        return EnumValue{EnumValueHandle{m_h.index, *p, m_h.generation}};
    }
    return std::nullopt;
  }

  std::expected<Any, Error> Type::ParseEnum(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    auto v = GetEnumValue(name);
    if (!v.has_value())
      return std::unexpected(v.error());
    return v->Value();
  }

  std::optional<std::string_view> Type::EnumName(const Any &value) const
  {
    if (!IsTypeAlive(m_h))
      return std::nullopt;
    const auto &reg = GetRegistry();
    const auto &info = reg.types[m_h.index].enumInfo;
    if (!info.isEnum)
      return std::nullopt;
    if (info.isSigned)
    {
      if (!info.ToSigned)
        return std::nullopt;
      auto r = info.ToSigned(value);
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
      if (!info.ToUnsigned)
        return std::nullopt;
      auto r = info.ToUnsigned(value);
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
  std::string_view Field::Name() const
  {
    if (!IsFieldAlive(m_h))
      return {};
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].name;
  }

  NGIN::UInt64 Field::TypeId() const
  {
    if (!IsFieldAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].typeId;
  }

  void *Field::GetMut(void *obj) const
  {
    if (!IsFieldAlive(m_h))
      return nullptr;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].GetMut(obj);
  }

  const void *Field::GetConst(const void *obj) const
  {
    if (!IsFieldAlive(m_h))
      return nullptr;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].GetConst(obj);
  }

  Any Field::GetAny(const void *obj) const
  {
    if (!IsFieldAlive(m_h))
      return Any::MakeVoid();
    const auto &reg = GetRegistry();
    const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
    if (f.Load)
      return f.Load(obj);
    return Any::MakeVoid();
  }

  std::expected<void, Error> Field::SetAny(void *obj, const Any &value) const
  {
    if (!IsFieldAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
    if (f.Store)
      return f.Store(obj, value);
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

  NGIN::UIntSize Field::AttributeCount() const
  {
    if (!IsFieldAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].fields[m_h.fieldIndex].attributes.Size();
  }

  AttributeView Field::AttributeAt(NGIN::UIntSize i) const
  {
    if (!IsFieldAlive(m_h))
      return AttributeView{};
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.typeIndex].fields[m_h.fieldIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Field::Attribute(std::string_view key) const
  {
    if (!IsFieldAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.typeIndex].fields[m_h.fieldIndex].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  // Property
  std::string_view Property::Name() const
  {
    if (!IsPropertyAlive(m_h))
      return {};
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].properties[m_h.propertyIndex].name;
  }

  NGIN::UInt64 Property::TypeId() const
  {
    if (!IsPropertyAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].properties[m_h.propertyIndex].typeId;
  }

  Any Property::GetAny(const void *obj) const
  {
    if (!IsPropertyAlive(m_h))
      return Any::MakeVoid();
    const auto &reg = GetRegistry();
    const auto &p = reg.types[m_h.typeIndex].properties[m_h.propertyIndex];
    if (p.Get)
      return p.Get(obj);
    return Any::MakeVoid();
  }

  std::expected<void, Error> Property::SetAny(void *obj, const Any &value) const
  {
    if (!IsPropertyAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &p = reg.types[m_h.typeIndex].properties[m_h.propertyIndex];
    if (!p.Set)
      return std::unexpected(Error{ErrorCode::InvalidArgument, "property is read-only"});
    return p.Set(obj, value);
  }

  NGIN::UIntSize Property::AttributeCount() const
  {
    if (!IsPropertyAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].properties[m_h.propertyIndex].attributes.Size();
  }

  AttributeView Property::AttributeAt(NGIN::UIntSize i) const
  {
    if (!IsPropertyAlive(m_h))
      return AttributeView{};
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.typeIndex].properties[m_h.propertyIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Property::Attribute(std::string_view key) const
  {
    if (!IsPropertyAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.typeIndex].properties[m_h.propertyIndex].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  // EnumValue
  std::string_view EnumValue::Name() const
  {
    if (!IsEnumValueAlive(m_h))
      return {};
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].enumInfo.values[m_h.valueIndex].name;
  }

  Any EnumValue::Value() const
  {
    if (!IsEnumValueAlive(m_h))
      return Any::MakeVoid();
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].enumInfo.values[m_h.valueIndex].value;
  }

  // Method
  std::string_view Method::GetName() const
  {
    if (!IsTypeAlive(m_typeIndex, m_typeGeneration))
      return {};
    const auto &reg = GetRegistry();
    if (m_methodIndex >= reg.types[m_typeIndex].methods.Size())
      return {};
    return reg.types[m_typeIndex].methods[m_methodIndex].name;
  }

  NGIN::UIntSize Method::GetParameterCount() const
  {
    if (!IsTypeAlive(m_typeIndex, m_typeGeneration))
      return 0;
    const auto &reg = GetRegistry();
    if (m_methodIndex >= reg.types[m_typeIndex].methods.Size())
      return 0;
    return reg.types[m_typeIndex].methods[m_methodIndex].paramTypeIds.Size();
  }

  NGIN::UInt64 Method::GetTypeId() const
  {
    if (!IsTypeAlive(m_typeIndex, m_typeGeneration))
      return 0;
    const auto &reg = GetRegistry();
    if (m_methodIndex >= reg.types[m_typeIndex].methods.Size())
      return 0;
    return reg.types[m_typeIndex].methods[m_methodIndex].returnTypeId;
  }

  std::expected<Any, Error> Method::Invoke(void *obj, const Any *args, NGIN::UIntSize count) const
  {
    if (!IsTypeAlive(m_typeIndex, m_typeGeneration))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    if (m_methodIndex >= reg.types[m_typeIndex].methods.Size())
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    return reg.types[m_typeIndex].methods[m_methodIndex].Invoke(obj, args, count);
  }

  // span-based convenience overloads are defined inline in the header

  NGIN::UIntSize Method::AttributeCount() const
  {
    if (!IsTypeAlive(m_typeIndex, m_typeGeneration))
      return 0;
    const auto &reg = GetRegistry();
    if (m_methodIndex >= reg.types[m_typeIndex].methods.Size())
      return 0;
    return reg.types[m_typeIndex].methods[m_methodIndex].attributes.Size();
  }

  AttributeView Method::AttributeAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_typeIndex, m_typeGeneration))
      return AttributeView{};
    const auto &reg = GetRegistry();
    if (m_methodIndex >= reg.types[m_typeIndex].methods.Size())
      return AttributeView{};
    const auto &a = reg.types[m_typeIndex].methods[m_methodIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Method::Attribute(std::string_view key) const
  {
    if (!IsTypeAlive(m_typeIndex, m_typeGeneration))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    if (m_methodIndex >= reg.types[m_typeIndex].methods.Size())
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
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

  NGIN::UIntSize Function::AttributeCount() const
  {
    const auto &reg = GetRegistry();
    return reg.functions[m_h.index].attributes.Size();
  }

  AttributeView Function::AttributeAt(NGIN::UIntSize i) const
  {
    const auto &reg = GetRegistry();
    const auto &a = reg.functions[m_h.index].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Function::Attribute(std::string_view key) const
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
    if (!IsCtorAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].constructors[m_h.ctorIndex].paramTypeIds.Size();
  }

  std::expected<Any, Error> Constructor::Construct(const Any *args, NGIN::UIntSize count) const
  {
    if (!IsCtorAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &c = reg.types[m_h.typeIndex].constructors[m_h.ctorIndex];
    if (!c.Construct)
      return std::unexpected(Error{ErrorCode::NotFound, "constructor not available"});
    return c.Construct(args, count);
  }

  NGIN::UIntSize Constructor::AttributeCount() const
  {
    if (!IsCtorAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.typeIndex].constructors[m_h.ctorIndex].attributes.Size();
  }

  AttributeView Constructor::AttributeAt(NGIN::UIntSize i) const
  {
    if (!IsCtorAlive(m_h))
      return AttributeView{};
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.typeIndex].constructors[m_h.ctorIndex].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Constructor::Attribute(std::string_view key) const
  {
    if (!IsCtorAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
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
        return Type{TypeHandle{*p, reg.types[*p].generation}};
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
        return Type{TypeHandle{*p, reg.types[*p].generation}};
    }
    return std::nullopt;
  }

  // Type: methods and attributes
  NGIN::UIntSize Type::MethodCount() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].methods.Size();
  }

  Method Type::MethodAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return Method{};
    return Method{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation};
  }

  std::expected<Method, Error> Type::GetMethod(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.index].methods;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].name == name)
        return Method{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation};
    return std::unexpected(Error{ErrorCode::NotFound, "method not found"});
  }

  std::optional<Method> Type::FindMethod(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return std::nullopt;
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.index].methods;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].name == name)
        return Method{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation};
    return std::nullopt;
  }

  MethodOverloads Type::FindMethods(std::string_view name) const
  {
    if (!IsTypeAlive(m_h))
      return MethodOverloads{};
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    NameId nid{};
    if (!detail::FindNameId(name, nid))
      return MethodOverloads{};
    const auto *vec = tdesc.methodOverloads.GetPtr(nid);
    if (!vec)
      return MethodOverloads{};
    return MethodOverloads{m_h.index, m_h.generation, vec};
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
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
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
    return ResolvedMethod{m_h.index, m_h.generation, bestIdx, std::move(argTypeIds), std::move(conversions)};
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
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].constructors.Size();
  }

  Constructor Type::ConstructorAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return Constructor{};
    return Constructor{ConstructorHandle{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
  }

  std::expected<Any, Error> Type::Construct(const Any *args, NGIN::UIntSize count) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    // Fast-path: default constructor
    if (count == 0)
    {
      for (NGIN::UIntSize i = 0; i < tdesc.constructors.Size(); ++i)
      {
        const auto &c = tdesc.constructors[i];
        if (c.paramTypeIds.Size() == 0 && c.Construct)
          return c.Construct(nullptr, 0);
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
    return tdesc.constructors[bestIdx].Construct(args, count);
  }

  NGIN::UIntSize Type::AttributeCount() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].attributes.Size();
  }

  AttributeView Type::AttributeAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return AttributeView{};
    const auto &reg = GetRegistry();
    const auto &a = reg.types[m_h.index].attributes[i];
    return AttributeView{a.key, &a.value};
  }

  std::expected<AttributeView, Error> Type::Attribute(std::string_view key) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &v = reg.types[m_h.index].attributes;
    for (NGIN::UIntSize i = 0; i < v.Size(); ++i)
      if (v[i].key == key)
        return AttributeView{v[i].key, &v[i].value};
    return std::unexpected(Error{ErrorCode::NotFound, "attribute not found"});
  }

  NGIN::UIntSize Type::MemberCount() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    const auto &t = reg.types[m_h.index];
    return t.fields.Size() + t.properties.Size() + t.methods.Size() + t.constructors.Size();
  }

  Member Type::MemberAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return Member{};
    const auto &reg = GetRegistry();
    const auto &t = reg.types[m_h.index];
    const auto fCount = t.fields.Size();
    const auto pCount = t.properties.Size();
    const auto mCount = t.methods.Size();
    if (i < fCount)
      return Member{MemberHandle{MemberKind::Field, m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
    i -= fCount;
    if (i < pCount)
      return Member{MemberHandle{MemberKind::Property, m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
    i -= pCount;
    if (i < mCount)
      return Member{MemberHandle{MemberKind::Method, m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
    i -= mCount;
    if (i < t.constructors.Size())
      return Member{MemberHandle{MemberKind::Constructor, m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
    return Member{};
  }

  NGIN::UIntSize Type::BaseCount() const
  {
    if (!IsTypeAlive(m_h))
      return 0;
    const auto &reg = GetRegistry();
    return reg.types[m_h.index].bases.Size();
  }

  Base Type::BaseAt(NGIN::UIntSize i) const
  {
    if (!IsTypeAlive(m_h))
      return Base{};
    return Base{BaseHandle{m_h.index, static_cast<NGIN::UInt32>(i), m_h.generation}};
  }

  ExpectedBase Type::GetBase(const Type &base) const
  {
    if (!IsTypeAlive(m_h))
      return std::unexpected(Error{ErrorCode::InvalidArgument, kStaleHandle});
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    const auto tid = base.GetTypeId();
    if (auto *p = tdesc.baseIndex.GetPtr(tid))
      return Base{BaseHandle{m_h.index, *p, m_h.generation}};
    return std::unexpected(Error{ErrorCode::NotFound, "base type not found"});
  }

  std::optional<Base> Type::FindBase(const Type &base) const
  {
    if (!IsTypeAlive(m_h))
      return std::nullopt;
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    const auto tid = base.GetTypeId();
    if (auto *p = tdesc.baseIndex.GetPtr(tid))
      return Base{BaseHandle{m_h.index, *p, m_h.generation}};
    return std::nullopt;
  }

  bool Type::IsDerivedFrom(const Type &base) const
  {
    if (!IsTypeAlive(m_h))
      return false;
    const auto &reg = GetRegistry();
    const auto &tdesc = reg.types[m_h.index];
    const auto tid = base.GetTypeId();
    return tdesc.baseIndex.GetPtr(tid) != nullptr;
  }

  Type Base::BaseType() const
  {
    if (!IsBaseAlive(m_h))
      return Type{};
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    return Type{TypeHandle{b.baseTypeIndex, reg.types[b.baseTypeIndex].generation}};
  }

  void *Base::Upcast(void *obj) const
  {
    if (!IsBaseAlive(m_h))
      return nullptr;
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.Upcast)
      return nullptr;
    return b.Upcast(obj);
  }

  const void *Base::Upcast(const void *obj) const
  {
    if (!IsBaseAlive(m_h))
      return nullptr;
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.UpcastConst)
      return nullptr;
    return b.UpcastConst(obj);
  }

  void *Base::Downcast(void *obj) const
  {
    if (!IsBaseAlive(m_h))
      return nullptr;
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.Downcast)
      return nullptr;
    return b.Downcast(obj);
  }

  const void *Base::Downcast(const void *obj) const
  {
    if (!IsBaseAlive(m_h))
      return nullptr;
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    if (!b.DowncastConst)
      return nullptr;
    return b.DowncastConst(obj);
  }

  bool Base::CanDowncast() const
  {
    if (!IsBaseAlive(m_h))
      return false;
    const auto &reg = GetRegistry();
    const auto &b = reg.types[m_h.typeIndex].bases[m_h.baseIndex];
    return b.Downcast != nullptr || b.DowncastConst != nullptr;
  }

} // namespace NGIN::Reflection
