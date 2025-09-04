// Registry.hpp
// Single-module immutable registry and query API (Phase 1)
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Containers/HashMap.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Reflection/Any.hpp>

#include <string_view>
#include <expected>
#include <span>
#include <variant>

#include <NGIN/Reflection/Types.hpp>

namespace NGIN::Reflection
{

  // Forward decls
  template <class T>
  struct tag
  {
    using type = T;
  };
  template <class T>
  class Builder;

  // Public attribute value type
  using AttrValue = std::variant<bool, std::int64_t, double, std::string_view, NGIN::UInt64>;

  struct AttributeDesc
  {
    std::string_view key;
    AttrValue value;
  };

  // Runtime wrappers
  class Field;
  class Type;
  class Method;
  class AttributeView;
  class Any;

  namespace detail
  {
    // Compute FNV-based type id for a type
    template<class T>
    inline NGIN::UInt64 TypeIdOf() {
      auto sv = NGIN::Meta::TypeName<std::remove_cv_t<std::remove_reference_t<T>>>::qualifiedName;
      return NGIN::Hashing::FNV1a64(sv.data(), sv.size());
    }

    inline std::string_view InternName(std::string_view s) noexcept { return s; }

    struct FieldRuntimeDesc
    {
      std::string_view name;
      NGIN::UInt64 typeId;
      NGIN::UIntSize sizeBytes{0};
      void *(*get_mut)(void *){nullptr};
      const void *(*get_const)(const void *){nullptr};
      class Any (*load)(const void *){nullptr};
      std::expected<void, Error> (*store)(void *, const class Any &){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct MethodRuntimeDesc
    {
      std::string_view name;
      NGIN::UInt64 returnTypeId;
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<class Any, Error> (*invoke)(void *, const class Any *, NGIN::UIntSize){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct TypeRuntimeDesc
    {
      std::string_view qualifiedName;
      NGIN::UInt64 typeId;
      NGIN::UIntSize sizeBytes;
      NGIN::UIntSize alignBytes;
      NGIN::Containers::Vector<FieldRuntimeDesc> fields;
      NGIN::Containers::Vector<MethodRuntimeDesc> methods;
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
      NGIN::Containers::FlatHashMap<std::string_view, NGIN::Containers::Vector<NGIN::UInt32>> methodOverloads;
    };

    struct Registry
    {
      NGIN::Containers::Vector<TypeRuntimeDesc> types;
      NGIN::Containers::FlatHashMap<NGIN::UInt64, NGIN::UInt32> byTypeId;
      NGIN::Containers::FlatHashMap<std::string_view, NGIN::UInt32> byName;
    };

    Registry &GetRegistry() noexcept;

    template <class T>
    concept HasNginReflectWithBuilder = requires(Builder<T> &b) {
      // ADL friend should be declared as: friend void ngin_reflect(tag<T>, Builder<T>&)
      { ngin_reflect(tag<T>{}, b) } -> std::same_as<void>;
    };

    // Traits for pointer-to-member decomposition
    template <class M>
    struct MemberPtrTraits;
    template <class C, class M>
    struct MemberPtrTraits<M C::*>
    {
      using Class = C;
      using Member = M;
    };

    template <auto MemberPtr>
    using MemberClassT = typename MemberPtrTraits<decltype(MemberPtr)>::Class;

    template <auto MemberPtr>
    using MemberTypeT = typename MemberPtrTraits<decltype(MemberPtr)>::Member;

    // Function pointers for field accessors
    template <auto MemberPtr>
    static void *FieldGetterMut(void *obj)
    {
      using C = MemberClassT<MemberPtr>;
      auto *c = static_cast<C *>(obj);
      return static_cast<void *>(&(c->*MemberPtr));
    }

    template <auto MemberPtr>
    static const void *FieldGetterConst(const void *obj)
    {
      using C = MemberClassT<MemberPtr>;
      auto *c = static_cast<const C *>(obj);
      return static_cast<const void *>(&(c->*MemberPtr));
    }

    template <auto MemberPtr>
    static Any FieldLoad(const void *obj)
    {
      using C = MemberClassT<MemberPtr>;
      using M = MemberTypeT<MemberPtr>;
      auto *c = static_cast<const C *>(obj);
      return Any::make(static_cast<const M &>(c->*MemberPtr));
    }

    template <auto MemberPtr>
    static std::expected<void, Error> FieldStore(void *obj, const class Any &value)
    {
      using C = MemberClassT<MemberPtr>;
      using M = MemberTypeT<MemberPtr>;
      const auto sv = NGIN::Meta::TypeName<M>::qualifiedName;
      const auto tid = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
      if (value.type_id() != tid)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      auto *c = static_cast<C *>(obj);
      (c->*MemberPtr) = value.as<M>();
      return {};
    }

    // Ensure a type is present; returns the type index
    template <class T>
    NGIN::UInt32 EnsureRegistered()
    {
      auto &reg = GetRegistry();
      const auto sv = NGIN::Meta::TypeName<T>::qualifiedName;
      const auto tid = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
      if (auto *p = reg.byTypeId.GetPtr(tid))
        return *p;

      // Create a new record with defaults
      TypeRuntimeDesc rec{};
      rec.qualifiedName = InternName(NGIN::Meta::TypeName<T>::qualifiedName); // default name derived; user override optional
      rec.typeId = tid;
      rec.sizeBytes = sizeof(T);
      rec.alignBytes = alignof(T);

      const auto idx = static_cast<NGIN::UInt32>(reg.types.Size());
      reg.types.PushBack(std::move(rec));
      reg.byTypeId.Insert(tid, idx);
      reg.byName.Insert(reg.types[idx].qualifiedName, idx);

      if constexpr (HasNginReflectWithBuilder<T>)
      {
        Builder<T> b{idx};
        ngin_reflect(tag<T>{}, b); // ADL — user describes fields/methods/etc.
      }
      return idx;
    }

  } // namespace detail

  // Public wrappers
  class Field
  {
  public:
    constexpr Field() = default;
    explicit constexpr Field(FieldHandle h) : m_h(h) {}

    [[nodiscard]] bool valid() const noexcept { return m_h.valid(); }
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] NGIN::UInt64 type_id() const;

    [[nodiscard]] void *get_mut(void *obj) const;
    [[nodiscard]] const void *get_const(const void *obj) const;

    // Any helpers
    [[nodiscard]] class Any get_any(const void *obj) const;
    [[nodiscard]] std::expected<void, Error> set_any(void *obj, const class Any &value) const;

    // Attributes
    [[nodiscard]] NGIN::UIntSize attribute_count() const;
    [[nodiscard]] AttributeView attribute_at(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> attribute(std::string_view key) const;

  private:
    FieldHandle m_h{};
    friend class Type;
  };

  class Method
  {
  public:
    constexpr Method() = default;
    explicit constexpr Method(NGIN::UInt32 typeIdx, NGIN::UInt32 methodIdx) : m_typeIndex(typeIdx), m_methodIndex(methodIdx) {}

    [[nodiscard]] bool valid() const noexcept { return m_typeIndex != static_cast<NGIN::UInt32>(-1); }
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] NGIN::UIntSize param_count() const;
    [[nodiscard]] NGIN::UInt64 return_type_id() const;
    [[nodiscard]] std::expected<class Any, Error> invoke(void *obj, const class Any *args, NGIN::UIntSize count) const;

    // Attributes
    [[nodiscard]] NGIN::UIntSize attribute_count() const;
    [[nodiscard]] AttributeView attribute_at(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> attribute(std::string_view key) const;

  private:
    NGIN::UInt32 m_typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 m_methodIndex{static_cast<NGIN::UInt32>(-1)};
  };

  class AttributeView
  {
  public:
    AttributeView() = default;
    AttributeView(std::string_view k, const AttrValue *v) : m_key(k), m_val(v) {}
    [[nodiscard]] std::string_view key() const { return m_key; }
    [[nodiscard]] const AttrValue &value() const { return *m_val; }

  private:
    std::string_view m_key{};
    const AttrValue *m_val{nullptr};
  };

  class Type
  {
  public:
    constexpr Type() = default;
    explicit constexpr Type(TypeHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept { return m_h.valid(); }
    [[nodiscard]] std::string_view QualifiedName() const;
    [[nodiscard]] NGIN::UInt64 GetTypeId() const;
    [[nodiscard]] NGIN::UIntSize Size() const;
    [[nodiscard]] NGIN::UIntSize Alignment() const;

    [[nodiscard]] NGIN::UIntSize FieldCount() const;
    [[nodiscard]] Field FieldAt(NGIN::UIntSize i) const;
    [[nodiscard]] ExpectedField GetField(std::string_view name) const;

    [[nodiscard]] NGIN::UIntSize MethodCount() const;
    [[nodiscard]] Method MethodAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<Method, Error> GetMethod(std::string_view name) const;
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name, const class Any* args, NGIN::UIntSize count) const;

    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    TypeHandle m_h{};
  };

  // Queries
  ExpectedType type(std::string_view qualified_name);

  template <class T>
  Type type_of()
  {
    auto idx = detail::EnsureRegistered<T>();
    return Type{TypeHandle{idx}};
  }

  // Optional eager registration helper
  template <class T>
  inline bool auto_register()
  {
    (void)detail::EnsureRegistered<T>();
    return true;
  }

} // namespace NGIN::Reflection
