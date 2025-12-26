// Registry.hpp
// Single-module immutable registry and query API (Phase 1)
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Containers/HashMap.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Utilities/StringInterner.hpp>

#include <string_view>
#include <expected>
#include <span>
#include <variant>
#include <type_traits>
#include <array>
#include <tuple>
#include <string>
#include <optional>
#include <memory>
#include <utility>

#include <NGIN/Reflection/Types.hpp>

namespace NGIN::Reflection
{
  using NameId = NGIN::UInt32;

  // Forward decls
  template <class T>
  struct Tag
  {
    using type = T;
  };
  template <class T>
  class TypeBuilder;

  // Optional external customization point for types you cannot modify
  // Specialize in namespace NGIN::Reflection: template<> struct Describe<MyType> { static void Do(TypeBuilder<MyType>&); };
  template <class T>
  struct Describe;

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
  class Constructor;
  class AttributeView;

  namespace detail
  {
    using StringInterner = NGIN::Utilities::StringInterner<>;

    // Convenience wrappers using the global registry interner
    NameId InternNameId(std::string_view s) noexcept;
    bool FindNameId(std::string_view s, NameId &out) noexcept;
    std::string_view NameFromId(NameId id) noexcept;
    // Compute FNV-based type id for a type
    template <class T>
    inline NGIN::UInt64 TypeIdOf()
    {
      auto sv = NGIN::Meta::TypeName<std::remove_cv_t<std::remove_reference_t<T>>>::qualifiedName;
      return NGIN::Hashing::FNV1a64(sv.data(), sv.size());
    }

    // Intern a string into the registry's string storage and return a stable view
    std::string_view InternName(std::string_view s) noexcept;

    struct FieldRuntimeDesc
    {
      std::string_view name;
      NameId nameId{static_cast<NameId>(-1)};
      NGIN::UInt64 typeId;
      NGIN::UIntSize sizeBytes{0};
      void *(*GetMut)(void *){nullptr};
      const void *(*GetConst)(const void *){nullptr};
      Any (*load)(const void *){nullptr};
      std::expected<void, Error> (*store)(void *, const Any &){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct MethodRuntimeDesc
    {
      std::string_view name;
      NameId nameId{static_cast<NameId>(-1)};
      NGIN::UInt64 returnTypeId;
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<Any, Error> (*Invoke)(void *, const Any *, NGIN::UIntSize){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct CtorRuntimeDesc
    {
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<Any, Error> (*construct)(const Any *, NGIN::UIntSize){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct TypeRuntimeDesc
    {
      std::string_view qualifiedName;
      NameId qualifiedNameId{static_cast<NameId>(-1)};
      NGIN::UInt64 typeId;
      NGIN::UIntSize sizeBytes;
      NGIN::UIntSize alignBytes;
      NGIN::Containers::Vector<FieldRuntimeDesc> fields;
      NGIN::Containers::FlatHashMap<NameId, NGIN::UInt32> fieldIndex;
      NGIN::Containers::Vector<MethodRuntimeDesc> methods;
      NGIN::Containers::Vector<CtorRuntimeDesc> constructors;
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
      NGIN::Containers::FlatHashMap<NameId, NGIN::Containers::Vector<NGIN::UInt32>> methodOverloads;
    };

    struct Registry
    {
      NGIN::Containers::Vector<TypeRuntimeDesc> types;
      NGIN::Containers::FlatHashMap<NGIN::UInt64, NGIN::UInt32> byTypeId;
      NGIN::Containers::FlatHashMap<NameId, NGIN::UInt32> byName;

      StringInterner names;
    };

    Registry &GetRegistry() noexcept;

    template <class T>
    concept HasNginReflectWithTypeBuilder = requires(TypeBuilder<T> &b) {
      // ADL friend should be declared as: friend void ngin_reflect(Tag<T>, TypeBuilder<T>&)
      { ngin_reflect(Tag<T>{}, b) } -> std::same_as<void>;
    };

    // Detection for Describe<T>::Do(TypeBuilder<T>&)
    template <class, class = void>
    struct HasDescribeImpl : std::false_type
    {
    };
    template <class T>
    struct HasDescribeImpl<T, std::void_t<decltype(NGIN::Reflection::Describe<T>::Do(std::declval<TypeBuilder<T> &>()))>>
        : std::true_type
    {
    };
    template <class T>
    concept HasDescribeWithTypeBuilder = HasDescribeImpl<T>::value;

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

    template <class Sig>
    struct SignatureTraits;

    template <class R, class... A>
    struct SignatureTraits<R(A...)>
    {
      using Ret = R;
      using Args = std::tuple<A...>;
    };

    template <class Sig, class = void>
    struct IsFunctionSignature : std::false_type
    {
    };

    template <class Sig>
    struct IsFunctionSignature<Sig, std::void_t<typename SignatureTraits<Sig>::Ret>> : std::true_type
    {
    };

    template <class Sig>
    concept FunctionSignature = IsFunctionSignature<Sig>::value;

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
      return Any{static_cast<const M &>(c->*MemberPtr)};
    }

    template <auto MemberPtr>
    static std::expected<void, Error> FieldStore(void *obj, const Any &value)
    {
      using C = MemberClassT<MemberPtr>;
      using M = MemberTypeT<MemberPtr>;
      const auto sv = NGIN::Meta::TypeName<M>::qualifiedName;
      const auto tid = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
      if (value.GetTypeId() != tid)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      auto *c = static_cast<C *>(obj);
      (c->*MemberPtr) = value.Cast<M>();
      return {};
    }

    // Ensure a type is present; returns the type index
    template <class T>
    NGIN::UInt32 EnsureRegistered()
    {
      using U = std::remove_cvref_t<T>;
      auto &reg = GetRegistry();
      const auto sv = NGIN::Meta::TypeName<U>::qualifiedName;
      const auto tid = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
      if (auto *p = reg.byTypeId.GetPtr(tid))
        return *p;

      // Create a new record with defaults
      TypeRuntimeDesc rec{};
      rec.qualifiedNameId = InternNameId(NGIN::Meta::TypeName<U>::qualifiedName);
      rec.qualifiedName = NameFromId(rec.qualifiedNameId); // default name derived; user override optional
      rec.typeId = tid;
      rec.sizeBytes = sizeof(U);
      rec.alignBytes = alignof(U);

      // Default constructor descriptor (if available)
      if constexpr (std::is_default_constructible_v<U>)
      {
        CtorRuntimeDesc c{};
        c.construct = [](const Any *, NGIN::UIntSize cnt) -> std::expected<Any, Error>
        {
          if (cnt != 0)
            return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
          return Any{U{}};
        };
        rec.constructors.PushBack(std::move(c));
      }

      const auto idx = static_cast<NGIN::UInt32>(reg.types.Size());
      reg.types.PushBack(std::move(rec));
      reg.byTypeId.Insert(tid, idx);
      reg.byName.Insert(reg.types[idx].qualifiedNameId, idx);
      // MSVC sometimes prefixes qualified names with "class ", "struct ", etc.
      // Add trimmed aliases to support portable GetType("Namespace::Type") lookups.
#if defined(_MSC_VER)
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

      if constexpr (HasNginReflectWithTypeBuilder<U>)
      {
        TypeBuilder<U> b{idx};
        ngin_reflect(Tag<U>{}, b); // ADL — user describes fields/methods/etc.
      }
      else if constexpr (HasDescribeWithTypeBuilder<U>)
      {
        TypeBuilder<U> b{idx};
        NGIN::Reflection::Describe<U>::Do(b); // Trait fallback — public access only
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

    [[nodiscard]] bool IsValid() const noexcept { return m_h.IsValid(); }
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] NGIN::UInt64 type_id() const;

    [[nodiscard]] void *GetMut(void *obj) const;
    [[nodiscard]] const void *GetConst(const void *obj) const;

    // Any helpers
    [[nodiscard]] Any GetAny(const void *obj) const;
    [[nodiscard]] std::expected<void, Error> SetAny(void *obj, const Any &value) const;

    template <class Obj>
    requires (!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] Any GetAny(const Obj &obj) const
    {
      return GetAny(static_cast<const void *>(&obj));
    }

    template <class Obj>
    requires (!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<void, Error> SetAny(Obj &obj, const Any &value) const
    {
      return SetAny(static_cast<void *>(&obj), value);
    }

    template <class T, class Obj>
    requires (!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<std::remove_cvref_t<T>, Error> Get(const Obj &obj) const
    {
      using U = std::remove_cvref_t<T>;
      const auto &reg = detail::GetRegistry();
      const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
      const auto want = detail::TypeIdOf<U>();
      if (f.typeId != want)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      const auto *ptr = static_cast<const U *>(GetConst(&obj));
      return *ptr;
    }

    template <class T, class Obj>
    requires (!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<void, Error> Set(Obj &obj, T &&value) const
    {
      using U = std::remove_cvref_t<T>;
      const auto &reg = detail::GetRegistry();
      const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
      const auto want = detail::TypeIdOf<U>();
      if (f.typeId != want)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      auto *ptr = static_cast<U *>(GetMut(&obj));
      *ptr = static_cast<U>(std::forward<T>(value));
      return {};
    }

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

    [[nodiscard]] bool IsValid() const noexcept { return m_typeIndex != static_cast<NGIN::UInt32>(-1); }
    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] NGIN::UIntSize GetParameterCount() const;
    [[nodiscard]] NGIN::UInt64 GetTypeId() const;
    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, const Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, std::span<const Any> args) const
    {
      return Invoke(obj, args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }
    template <class Obj>
    [[nodiscard]] std::expected<Any, Error> Invoke(Obj &obj, std::span<const Any> args) const
    {
      return Invoke(static_cast<void *>(&obj), args);
    }
    template <class Obj>
    [[nodiscard]] std::expected<Any, Error> Invoke(const Obj &obj, std::span<const Any> args) const
    {
      return Invoke(const_cast<void *>(static_cast<const void *>(&obj)), args);
    }
    template <NGIN::UIntSize N>
    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, const Any (&args)[N], NGIN::UIntSize count) const
    {
      const Any *p = args;
      return Invoke(obj, p, count);
    }
    template <NGIN::UIntSize N>
    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, Any (&args)[N], NGIN::UIntSize count) const
    {
      const Any *p = args;
      return Invoke(obj, p, count);
    }
    template <class R, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(void *obj, A &&...a) const
    {
      std::array<Any, sizeof...(A)> tmp{Any{std::forward<A>(a)}...};
      auto r = Invoke(obj, tmp.data(), static_cast<NGIN::UIntSize>(tmp.size()));
      if (!r.has_value())
        return std::unexpected(r.error());
      if constexpr (std::is_void_v<R>)
      {
        return {};
      }
      else
      {
        return r->template Cast<R>();
      }
    }
    template <class R, class Obj, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(Obj &obj, A &&...a) const
    {
      return InvokeAs<R>(static_cast<void *>(&obj), std::forward<A>(a)...);
    }
    template <class R, class Obj, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(const Obj &obj, A &&...a) const
    {
      return InvokeAs<R>(const_cast<void *>(static_cast<const void *>(&obj)), std::forward<A>(a)...);
    }

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

  class MethodOverloads
  {
  public:
    MethodOverloads() = default;
    MethodOverloads(NGIN::UInt32 typeIndex, const NGIN::Containers::Vector<NGIN::UInt32> *indices)
        : m_typeIndex(typeIndex), m_indices(indices)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept { return m_indices != nullptr; }
    [[nodiscard]] NGIN::UIntSize Size() const { return m_indices ? m_indices->Size() : 0; }
    [[nodiscard]] Method MethodAt(NGIN::UIntSize i) const { return Method{m_typeIndex, (*m_indices)[i]}; }

  private:
    NGIN::UInt32 m_typeIndex{static_cast<NGIN::UInt32>(-1)};
    const NGIN::Containers::Vector<NGIN::UInt32> *m_indices{nullptr};
  };

  class Type
  {
  public:
    constexpr Type() = default;
    explicit constexpr Type(TypeHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept { return m_h.IsValid(); }
    [[nodiscard]] std::string_view QualifiedName() const;
    [[nodiscard]] NGIN::UInt64 GetTypeId() const;
    [[nodiscard]] NGIN::UIntSize Size() const;
    [[nodiscard]] NGIN::UIntSize Alignment() const;

    [[nodiscard]] NGIN::UIntSize FieldCount() const;
    [[nodiscard]] Field FieldAt(NGIN::UIntSize i) const;
    [[nodiscard]] ExpectedField GetField(std::string_view name) const;
    [[nodiscard]] std::optional<Field> FindField(std::string_view name) const;

    [[nodiscard]] NGIN::UIntSize MethodCount() const;
    [[nodiscard]] Method MethodAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<Method, Error> GetMethod(std::string_view name) const;
    [[nodiscard]] std::optional<Method> FindMethod(std::string_view name) const;
    [[nodiscard]] MethodOverloads FindMethods(std::string_view name) const;
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name, const Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name, std::span<const Any> args) const
    {
      return ResolveMethod(name, args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }

    // Resolve by compile-time signature (exact match on parameter and, if non-void, return type)
    template <class R = void, class... A>
    requires (!detail::FunctionSignature<R>)
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name) const
    {
      const auto &reg = detail::GetRegistry();
      const auto &tdesc = reg.types[m_h.index];
      NameId nid{};
      if (!detail::FindNameId(name, nid))
        return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
      auto *vec = tdesc.methodOverloads.GetPtr(nid);
      if (!vec)
        return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
      // Desired param type ids
      constexpr NGIN::UIntSize N = sizeof...(A);
      NGIN::UInt64 desired[N == 0 ? 1 : N] = {detail::TypeIdOf<std::remove_cv_t<std::remove_reference_t<A>>>()...};
      const NGIN::UInt64 desiredRet = []
      {
        if constexpr (std::is_void_v<R>)
          return NGIN::UInt64{0};
        else
          return detail::TypeIdOf<std::remove_cv_t<std::remove_reference_t<R>>>();
      }();
      for (NGIN::UIntSize k = 0; k < vec->Size(); ++k)
      {
        auto mi = (*vec)[k];
        const auto &m = tdesc.methods[mi];
        if constexpr (!std::is_void_v<R>)
        {
          if (m.returnTypeId != desiredRet)
            continue;
        }
        else
        {
          if (m.returnTypeId != 0)
            continue;
        }
        if (m.paramTypeIds.Size() != N)
          continue;
        bool all = true;
        for (NGIN::UIntSize i = 0; i < N; ++i)
        {
          if (m.paramTypeIds[i] != desired[i])
          {
            all = false;
            break;
          }
        }
        if (all)
          return Method{m_h.index, mi};
      }
      return std::unexpected(Error{ErrorCode::InvalidArgument, "no exact match"});
    }

    template <class Sig>
    requires detail::FunctionSignature<Sig>
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name) const
    {
      using Traits = detail::SignatureTraits<Sig>;
      constexpr auto N = std::tuple_size_v<typename Traits::Args>;
      return ResolveMethodBySignature<Sig>(name, std::make_index_sequence<N>{});
    }

    // Directly resolve and Invoke by compile-time signature
    template <class R = void, class... A>
    [[nodiscard]] std::expected<Any, Error> Invoke(std::string_view name, void *obj, A &&...a) const
    {
      auto m = ResolveMethod<R, A...>(name);
      if (!m.has_value())
        return std::unexpected(m.error());
      std::array<Any, sizeof...(A)> tmp{Any{std::forward<A>(a)}...};
      return m->Invoke(obj, tmp.data(), static_cast<NGIN::UIntSize>(tmp.size()));
    }
    template <class R = void, class... A, class Obj>
    requires (!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<Any, Error> Invoke(std::string_view name, Obj &&obj, A &&...a) const
    {
      using ObjT = std::remove_reference_t<Obj>;
      if constexpr (std::is_const_v<ObjT>)
      {
        return Invoke<R>(name,
                         const_cast<void *>(static_cast<const void *>(std::addressof(obj))),
                         std::forward<A>(a)...);
      }
      else
      {
        return Invoke<R>(name, static_cast<void *>(std::addressof(obj)), std::forward<A>(a)...);
      }
    }

    template <class R, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(std::string_view name, void *obj, A &&...a) const
    {
      auto m = ResolveMethod<R, A...>(name);
      if (!m)
        return std::unexpected(m.error());
      return m->template InvokeAs<R>(obj, std::forward<A>(a)...);
    }
    template <class R, class... A, class Obj>
    requires (!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<R, Error> InvokeAs(std::string_view name, Obj &&obj, A &&...a) const
    {
      using ObjT = std::remove_reference_t<Obj>;
      if constexpr (std::is_const_v<ObjT>)
      {
        return InvokeAs<R>(name,
                           const_cast<void *>(static_cast<const void *>(std::addressof(obj))),
                           std::forward<A>(a)...);
      }
      else
      {
        return InvokeAs<R>(name, static_cast<void *>(std::addressof(obj)), std::forward<A>(a)...);
      }
    }

    // Constructors
    [[nodiscard]] NGIN::UIntSize ConstructorCount() const;
    [[nodiscard]] std::expected<Any, Error> Construct(const Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<Any, Error> Construct(std::span<const Any> args) const
    {
      return Construct(args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }
    [[nodiscard]] std::expected<Any, Error> DefaultConstruct() const
    {
      const Any *none = nullptr;
      return Construct(none, 0);
    }

    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    template <class Sig, std::size_t... I>
    [[nodiscard]] std::expected<Method, Error> ResolveMethodBySignature(std::string_view name, std::index_sequence<I...>) const
    {
      using Traits = detail::SignatureTraits<Sig>;
      return ResolveMethod<typename Traits::Ret, std::tuple_element_t<I, typename Traits::Args>...>(name);
    }

    TypeHandle m_h{};
  };

  // Queries
  ExpectedType GetType(std::string_view name);
  std::optional<Type> FindType(std::string_view name);

  template <class T>
  Type GetType()
  {
    auto idx = detail::EnsureRegistered<T>();
    return Type{TypeHandle{idx}};
  }

  template <class T>
  std::optional<Type> TryGetType()
  {
    using U = std::remove_cvref_t<T>;
    auto &reg = detail::GetRegistry();
    const auto tid = detail::TypeIdOf<U>();
    if (auto *p = reg.byTypeId.GetPtr(tid))
      return Type{TypeHandle{*p}};
    return std::nullopt;
  }

  // Optional eager registration helper
  template <class T>
  inline bool auto_register()
  {
    (void)detail::EnsureRegistered<T>();
    return true;
  }

} // namespace NGIN::Reflection
