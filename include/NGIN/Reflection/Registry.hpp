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
#include <type_traits>
#include <array>
#include <string>
#include <memory>

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
  class Builder;

  // Optional external customization point for types you cannot modify
  // Specialize in namespace NGIN::Reflection: template<> struct Describe<MyType> { static void Do(Builder<MyType>&); };
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
  class Any;

  namespace detail
  {
    struct StringInterner
    {
      StringInterner() = default;
      ~StringInterner();

      NameId InsertOrGet(std::string_view s) noexcept;
      bool FindId(std::string_view s, NameId &out) const noexcept;
      std::string_view View(NameId id) const noexcept;
      std::string_view InternView(std::string_view s) noexcept;

    private:
      struct Page
      {
        char *data{nullptr};
        NGIN::UInt32 used{0};
        NGIN::UInt32 capacity{0};
      };
      struct Entry
      {
        NGIN::UInt32 page{0};
        NGIN::UInt32 offset{0};
        NGIN::UInt32 length{0};
        NGIN::UInt64 hash{0};
      };
      NGIN::Containers::Vector<Page> pages;
      NGIN::Containers::Vector<Entry> entries;
      NGIN::Containers::FlatHashMap<NGIN::UInt64, NGIN::Containers::Vector<NGIN::UInt32>> buckets;

      void *AllocateBytes(NGIN::UInt32 n) noexcept;
    };

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
      class Any (*load)(const void *){nullptr};
      std::expected<void, Error> (*store)(void *, const class Any &){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct MethodRuntimeDesc
    {
      std::string_view name;
      NameId nameId{static_cast<NameId>(-1)};
      NGIN::UInt64 returnTypeId;
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<class Any, Error> (*Invoke)(void *, const class Any *, NGIN::UIntSize){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct CtorRuntimeDesc
    {
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<class Any, Error> (*construct)(const class Any *, NGIN::UIntSize){nullptr};
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
    concept HasNginReflectWithBuilder = requires(Builder<T> &b) {
      // ADL friend should be declared as: friend void ngin_reflect(tag<T>, Builder<T>&)
      { ngin_reflect(Tag<T>{}, b) } -> std::same_as<void>;
    };

    // Detection for Describe<T>::Do(Builder<T>&)
    template <class, class = void>
    struct HasDescribeImpl : std::false_type
    {
    };
    template <class T>
    struct HasDescribeImpl<T, std::void_t<decltype(NGIN::Reflection::Describe<T>::Do(std::declval<Builder<T> &>()))>>
        : std::true_type
    {
    };
    template <class T>
    concept HasDescribeWithBuilder = HasDescribeImpl<T>::value;

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
      (c->*MemberPtr) = value.As<M>();
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
        c.construct = [](const class Any *, NGIN::UIntSize cnt) -> std::expected<class Any, Error>
        {
          if (cnt != 0)
            return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
          return Any::make(U{});
        };
        rec.constructors.PushBack(std::move(c));
      }

      const auto idx = static_cast<NGIN::UInt32>(reg.types.Size());
      reg.types.PushBack(std::move(rec));
      reg.byTypeId.Insert(tid, idx);
      reg.byName.Insert(reg.types[idx].qualifiedNameId, idx);

      if constexpr (HasNginReflectWithBuilder<U>)
      {
        Builder<U> b{idx};
        ngin_reflect(Tag<U>{}, b); // ADL — user describes fields/methods/etc.
      }
      else if constexpr (HasDescribeWithBuilder<U>)
      {
        Builder<U> b{idx};
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
    [[nodiscard]] class Any GetAny(const void *obj) const;
    [[nodiscard]] std::expected<void, Error> SetAny(void *obj, const class Any &value) const;

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
    [[nodiscard]] std::expected<class Any, Error> Invoke(void *obj, const class Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<class Any, Error> Invoke(void *obj, std::span<const class Any> args) const
    {
      return Invoke(obj, args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }
    template <NGIN::UIntSize N>
    [[nodiscard]] std::expected<class Any, Error> Invoke(void *obj, const class Any (&args)[N], NGIN::UIntSize count) const
    {
      const class Any *p = args;
      return Invoke(obj, p, count);
    }
    template <NGIN::UIntSize N>
    [[nodiscard]] std::expected<class Any, Error> Invoke(void *obj, class Any (&args)[N], NGIN::UIntSize count) const
    {
      const class Any *p = args;
      return Invoke(obj, p, count);
    }
    template <class R, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(void *obj, A &&...a) const
    {
      std::array<class Any, sizeof...(A)> tmp{Any::make(std::forward<A>(a))...};
      auto r = Invoke(obj, tmp.data(), static_cast<NGIN::UIntSize>(tmp.size()));
      if (!r.has_value())
        return std::unexpected(r.error());
      if constexpr (std::is_void_v<R>)
      {
        return {};
      }
      else
      {
        return r->template As<R>();
      }
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

    [[nodiscard]] NGIN::UIntSize MethodCount() const;
    [[nodiscard]] Method MethodAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<Method, Error> GetMethod(std::string_view name) const;
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name, const class Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name, std::span<const class Any> args) const
    {
      return ResolveMethod(name, args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }

    // Resolve by compile-time signature (exact match on parameter and, if non-void, return type)
    template <class R = void, class... A>
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

    // Directly resolve and Invoke by compile-time signature
    template <class R = void, class... A>
    [[nodiscard]] std::expected<class Any, Error> Invoke(std::string_view name, void *obj, A &&...a) const
    {
      auto m = ResolveMethod<R, A...>(name);
      if (!m.has_value())
        return std::unexpected(m.error());
      std::array<class Any, sizeof...(A)> tmp{Any::make(std::forward<A>(a))...};
      return m->Invoke(obj, tmp.data(), static_cast<NGIN::UIntSize>(tmp.size()));
    }

    template <class R, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(std::string_view name, void *obj, A &&...a) const
    {
      auto m = ResolveMethod<R, A...>(name);
      if (!m)
        return std::unexpected(m.error());
      return m->template InvokeAs<R>(obj, std::forward<A>(a)...);
    }

    // Constructors
    [[nodiscard]] NGIN::UIntSize ConstructorCount() const;
    [[nodiscard]] std::expected<class Any, Error> Construct(const class Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<class Any, Error> Construct(std::span<const class Any> args) const
    {
      return Construct(args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }
    [[nodiscard]] std::expected<class Any, Error> DefaultConstruct() const
    {
      const class Any *none = nullptr;
      return Construct(none, 0);
    }

    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    TypeHandle m_h{};
  };

  // Queries
  ExpectedType GetType(std::string_view name);

  template <class T>
  Type TypeOf()
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
