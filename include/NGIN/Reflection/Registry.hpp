// Registry.hpp
// Single-module immutable registry and query API (Phase 1)
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Containers/HashMap.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <shared_mutex>
#include <mutex>

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
  using NameId = std::string_view;

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
  class Property;
  class EnumValue;
  class Type;
  class Method;
  class Constructor;
  class Member;
  class Base;
  class Function;
  class ResolvedFunction;
  class AttributeView;

  namespace detail
  {
    struct StringPool
    {
      std::string_view Intern(std::string_view s) noexcept;
      void Clear() noexcept;

      NGIN::Containers::FlatHashMap<std::string_view, std::string_view> entries;
      NGIN::Containers::Vector<std::unique_ptr<char[]>> allocations;
    };

    struct ModuleStrings
    {
      ModuleId moduleId{0};
      NGIN::UInt32 typeCount{0};
      StringPool pool{};
    };

    // Convenience wrappers using module-owned string storage.
    NameId InternNameId(ModuleId moduleId, std::string_view s) noexcept;
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

    // Intern a string into module-owned storage and return a stable view.
    std::string_view InternName(ModuleId moduleId, std::string_view s) noexcept;
    std::string_view InternName(std::string_view s) noexcept;

    AttrValue InternAttrValue(ModuleId moduleId, const AttrValue &value) noexcept;

    struct FieldDescriptor
    {
      std::string_view name;
      NameId nameId{};
      NGIN::UInt64 typeId;
      NGIN::UIntSize sizeBytes{0};
      void *(*GetMut)(void *){nullptr};
      const void *(*GetConst)(const void *){nullptr};
      Any (*Load)(const void *){nullptr};
      std::expected<void, Error> (*Store)(void *, const Any &){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct PropertyDescriptor
    {
      std::string_view name;
      NameId nameId{};
      NGIN::UInt64 typeId;
      Any (*Get)(const void *){nullptr};
      std::expected<void, Error> (*Set)(void *, const Any &){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct MethodDescriptor
    {
      std::string_view name;
      NameId nameId{};
      NGIN::UInt64 returnTypeId;
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<Any, Error> (*Invoke)(void *, const Any *, NGIN::UIntSize){nullptr};
      std::expected<Any, Error> (*InvokeExact)(void *, const Any *, NGIN::UIntSize){nullptr};
      bool isConst{false};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct FunctionDescriptor
    {
      std::string_view name;
      NameId nameId{};
      NGIN::UInt64 returnTypeId;
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<Any, Error> (*Invoke)(const Any *, NGIN::UIntSize){nullptr};
      std::expected<Any, Error> (*InvokeExact)(const Any *, NGIN::UIntSize){nullptr};
      ModuleId moduleId{0};
      bool alive{true};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct EnumValueDescriptor
    {
      std::string_view name;
      NameId nameId{};
      Any value{};
      std::int64_t svalue{0};
      std::uint64_t uvalue{0};
    };

    struct EnumDescriptor
    {
      bool isEnum{false};
      bool isSigned{true};
      NGIN::UInt64 underlyingTypeId{0};
      NGIN::Containers::Vector<EnumValueDescriptor> values;
      NGIN::Containers::FlatHashMap<NameId, NGIN::UInt32> valueIndex;
      std::expected<std::uint64_t, Error> (*ToUnsigned)(const Any &){nullptr};
      std::expected<std::int64_t, Error> (*ToSigned)(const Any &){nullptr};
    };

    struct BaseDescriptor
    {
      NGIN::UInt32 baseTypeIndex{static_cast<NGIN::UInt32>(-1)};
      NGIN::UInt64 baseTypeId{0};
      void *(*Upcast)(void *){nullptr};
      const void *(*UpcastConst)(const void *){nullptr};
      void *(*Downcast)(void *){nullptr};
      const void *(*DowncastConst)(const void *){nullptr};
    };

    struct ConstructorDescriptor
    {
      NGIN::Containers::Vector<NGIN::UInt64> paramTypeIds;
      std::expected<Any, Error> (*Construct)(const Any *, NGIN::UIntSize){nullptr};
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
    };

    struct TypeDescriptor
    {
      std::string_view qualifiedName;
      NameId qualifiedNameId{};
      NGIN::UInt64 typeId;
      ModuleId moduleId{0};
      NGIN::UInt32 generation{0};
      NGIN::UIntSize sizeBytes;
      NGIN::UIntSize alignBytes;
      NGIN::Containers::Vector<FieldDescriptor> fields;
      NGIN::Containers::FlatHashMap<NameId, NGIN::UInt32> fieldIndex;
      NGIN::Containers::Vector<PropertyDescriptor> properties;
      NGIN::Containers::FlatHashMap<NameId, NGIN::UInt32> propertyIndex;
      EnumDescriptor enumInfo;
      NGIN::Containers::Vector<BaseDescriptor> bases;
      NGIN::Containers::FlatHashMap<NGIN::UInt64, NGIN::UInt32> baseIndex;
      NGIN::Containers::Vector<MethodDescriptor> methods;
      NGIN::Containers::Vector<ConstructorDescriptor> constructors;
      NGIN::Containers::Vector<NGIN::Reflection::AttributeDesc> attributes;
      NGIN::Containers::FlatHashMap<NameId, NGIN::Containers::Vector<NGIN::UInt32>> methodOverloads;
    };

    struct Registry
    {
      NGIN::Containers::Vector<TypeDescriptor> types;
      NGIN::Containers::FlatHashMap<NGIN::UInt64, NGIN::UInt32> byTypeId;
      NGIN::Containers::FlatHashMap<NameId, NGIN::UInt32> byName;
      NGIN::Containers::Vector<FunctionDescriptor> functions;
      NGIN::Containers::FlatHashMap<NameId, NGIN::Containers::Vector<NGIN::UInt32>> functionOverloads;
      NGIN::Containers::Vector<ModuleStrings> modules;
      NGIN::Containers::FlatHashMap<ModuleId, NGIN::UInt32> moduleIndex;
      mutable std::shared_mutex mutex;
    };

    Registry &GetRegistry() noexcept;

    class RegistryReadLock
    {
    public:
      RegistryReadLock() noexcept;
      RegistryReadLock(RegistryReadLock &&other) noexcept;
      RegistryReadLock &operator=(RegistryReadLock &&other) noexcept;
      ~RegistryReadLock() noexcept;

    private:
      bool m_active{false};
    };

    class RegistryWriteLock
    {
    public:
      RegistryWriteLock() noexcept;
      RegistryWriteLock(RegistryWriteLock &&other) noexcept;
      RegistryWriteLock &operator=(RegistryWriteLock &&other) noexcept;
      ~RegistryWriteLock() noexcept;

    private:
      bool m_active{false};
    };

    RegistryReadLock LockRegistryRead() noexcept;
    RegistryWriteLock LockRegistryWrite() noexcept;
    bool BeginModuleInitialization(ModuleId moduleId) noexcept;
    void FinishModuleInitialization(ModuleId moduleId, bool success) noexcept;

    bool IsTypeAlive(const Registry &reg, NGIN::UInt32 index, NGIN::UInt32 generation) noexcept;
    bool IsTypeAlive(const Registry &reg, TypeHandle h) noexcept;
    bool IsFieldAlive(const Registry &reg, FieldHandle h) noexcept;
    bool IsPropertyAlive(const Registry &reg, PropertyHandle h) noexcept;
    bool IsEnumValueAlive(const Registry &reg, EnumValueHandle h) noexcept;
    bool IsCtorAlive(const Registry &reg, ConstructorHandle h) noexcept;
    bool IsBaseAlive(const Registry &reg, BaseHandle h) noexcept;
    bool IsMethodAlive(const Registry &reg, NGIN::UInt32 typeIndex, NGIN::UInt32 typeGeneration, NGIN::UInt32 methodIndex) noexcept;
    bool IsFunctionAlive(const Registry &reg, FunctionHandle h) noexcept;
    void IncrementModuleTypeCount(ModuleId moduleId) noexcept;
    void DecrementModuleTypeCount(ModuleId moduleId) noexcept;

    template <class T>
    concept HasNginReflectWithTypeBuilder = requires(TypeBuilder<T> &b) {
      // ADL friend should be declared as: friend void NginReflect(Tag<T>, TypeBuilder<T>&)
      { NginReflect(Tag<T>{}, b) } -> std::same_as<void>;
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

    template <class>
    struct IsFunctionPtr : std::false_type
    {
    };
    template <class R, class... A>
    struct IsFunctionPtr<R (*)(A...)> : std::true_type
    {
    };
    template <class T>
    inline constexpr bool IsFunctionPtrV = IsFunctionPtr<T>::value;

    template <class T>
    inline bool ArgMatchesExact(const Any &arg)
    {
      using U = std::remove_cv_t<std::remove_reference_t<T>>;
      return arg.GetTypeId() == TypeIdOf<U>();
    }

    enum class ConversionKind : NGIN::UInt8
    {
      Exact = 0,
      Convert = 1,
    };

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

    // Ensure a type is present; returns the type index. Caller must hold a write lock.
    template <class T>
    NGIN::UInt32 EnsureRegistered(ModuleId moduleId = ModuleId{0})
    {
      using U = std::remove_cvref_t<T>;
      auto &reg = GetRegistry();
      const auto sv = NGIN::Meta::TypeName<U>::qualifiedName;
      const auto tid = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
      if (auto *p = reg.byTypeId.GetPtr(tid))
        return *p;

      // Create a new record with defaults
      TypeDescriptor rec{};
      rec.qualifiedNameId = InternNameId(moduleId, NGIN::Meta::TypeName<U>::qualifiedName);
      rec.qualifiedName = NameFromId(rec.qualifiedNameId); // default name derived; user override optional
      rec.typeId = tid;
      rec.moduleId = moduleId;
      rec.sizeBytes = sizeof(U);
      rec.alignBytes = alignof(U);

      // Default constructor descriptor (if available)
      if constexpr (std::is_default_constructible_v<U>)
      {
        ConstructorDescriptor c{};
        c.Construct = [](const Any *, NGIN::UIntSize cnt) -> std::expected<Any, Error>
        {
          if (cnt != 0)
            return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
          return Any{U{}};
        };
        rec.constructors.PushBack(std::move(c));
      }

      const auto idx = static_cast<NGIN::UInt32>(reg.types.Size());
      reg.types.PushBack(std::move(rec));
      IncrementModuleTypeCount(moduleId);
      reg.byTypeId.Insert(tid, idx);
      reg.byName.Insert(reg.types[idx].qualifiedNameId, idx);
      // MSVC sometimes prefixes qualified names with "class ", "struct ", etc.
      // Add trimmed aliases to support portable GetType("Namespace::Type") lookups.
#if defined(_MSC_VER)
      {
        auto qn = reg.types[idx].qualifiedName;
        auto add_alias = [&](std::string_view prefix)
        {
          if (qn.size() > prefix.size() && qn.substr(0, prefix.size()) == prefix)
          {
            auto trimmed = qn.substr(prefix.size());
            auto aliasId = InternNameId(moduleId, trimmed);
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
        NginReflect(Tag<U>{}, b); // ADL — user describes fields/methods/etc.
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

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (!m_h.IsValid())
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsFieldAlive(reg, m_h);
    }
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] NGIN::UInt64 TypeId() const;

    [[nodiscard]] void *GetMut(void *obj) const;
    [[nodiscard]] const void *GetConst(const void *obj) const;

    // Any helpers
    [[nodiscard]] Any GetAny(const void *obj) const;
    [[nodiscard]] std::expected<void, Error> SetAny(void *obj, const Any &value) const;

    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] Any GetAny(const Obj &obj) const
    {
      return GetAny(static_cast<const void *>(&obj));
    }

    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<void, Error> SetAny(Obj &obj, const Any &value) const
    {
      return SetAny(static_cast<void *>(&obj), value);
    }

    template <class T, class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<std::remove_cvref_t<T>, Error> Get(const Obj &obj) const
    {
      using U = std::remove_cvref_t<T>;
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsFieldAlive(reg, m_h))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
      const auto want = detail::TypeIdOf<U>();
      if (f.typeId != want)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      const auto *ptr = static_cast<const U *>(f.GetConst(&obj));
      return *ptr;
    }

    template <class T, class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<void, Error> Set(Obj &obj, T &&value) const
    {
      using U = std::remove_cvref_t<T>;
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsFieldAlive(reg, m_h))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &f = reg.types[m_h.typeIndex].fields[m_h.fieldIndex];
      const auto want = detail::TypeIdOf<U>();
      if (f.typeId != want)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      auto *ptr = static_cast<U *>(f.GetMut(&obj));
      *ptr = static_cast<U>(std::forward<T>(value));
      return {};
    }

    // Attributes
    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    FieldHandle m_h{};
    friend class Type;
  };

  class Property
  {
  public:
    constexpr Property() = default;
    explicit constexpr Property(PropertyHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (!m_h.IsValid())
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsPropertyAlive(reg, m_h);
    }
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] NGIN::UInt64 TypeId() const;

    [[nodiscard]] Any GetAny(const void *obj) const;
    [[nodiscard]] std::expected<void, Error> SetAny(void *obj, const Any &value) const;

    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] Any GetAny(const Obj &obj) const
    {
      return GetAny(static_cast<const void *>(&obj));
    }

    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<void, Error> SetAny(Obj &obj, const Any &value) const
    {
      return SetAny(static_cast<void *>(&obj), value);
    }

    template <class T, class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<std::remove_cvref_t<T>, Error> Get(const Obj &obj) const
    {
      using U = std::remove_cvref_t<T>;
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsPropertyAlive(reg, m_h))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &p = reg.types[m_h.typeIndex].properties[m_h.propertyIndex];
      const auto want = detail::TypeIdOf<U>();
      if (p.typeId != want)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      Any any = Any::MakeVoid();
      if (p.Get)
        any = p.Get(static_cast<const void *>(std::addressof(obj)));
      if (any.GetTypeId() != want)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "type-id mismatch"});
      return any.template Cast<U>();
    }

    template <class T, class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<void, Error> Set(Obj &obj, T &&value) const
    {
      return SetAny(obj, Any{std::forward<T>(value)});
    }

    // Attributes
    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    PropertyHandle m_h{};
    friend class Type;
  };

  class EnumValue
  {
  public:
    constexpr EnumValue() = default;
    explicit constexpr EnumValue(EnumValueHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (!m_h.IsValid())
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsEnumValueAlive(reg, m_h);
    }
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] Any Value() const;

  private:
    EnumValueHandle m_h{};
    friend class Type;
  };

  class Method
  {
  public:
    constexpr Method() = default;
    explicit constexpr Method(NGIN::UInt32 typeIdx, NGIN::UInt32 methodIdx, NGIN::UInt32 typeGen)
        : m_typeIndex(typeIdx), m_methodIndex(methodIdx), m_typeGeneration(typeGen)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (m_typeIndex == static_cast<NGIN::UInt32>(-1))
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsMethodAlive(reg, m_typeIndex, m_typeGeneration, m_methodIndex);
    }
    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] NGIN::UIntSize GetParameterCount() const;
    [[nodiscard]] NGIN::UInt64 GetTypeId() const;
    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, const Any *args, NGIN::UIntSize count) const;

    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, std::span<const Any> args) const
    {
      return Invoke(obj, args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }
    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<Any, Error> Invoke(Obj &obj, std::span<const Any> args) const
    {
      return Invoke(static_cast<void *>(std::addressof(obj)), args);
    }
    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<Any, Error> Invoke(const Obj &obj, std::span<const Any> args) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsMethodAlive(reg, m_typeIndex, m_typeGeneration, m_methodIndex))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &m = reg.types[m_typeIndex].methods[m_methodIndex];
      if (!m.isConst)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "const object requires const method"});
      return Invoke(const_cast<void *>(static_cast<const void *>(std::addressof(obj))), args);
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
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<R, Error> InvokeAs(Obj &obj, A &&...a) const
    {
      return InvokeAs<R>(static_cast<void *>(std::addressof(obj)), std::forward<A>(a)...);
    }
    template <class R, class Obj, class... A>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<R, Error> InvokeAs(const Obj &obj, A &&...a) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsMethodAlive(reg, m_typeIndex, m_typeGeneration, m_methodIndex))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &m = reg.types[m_typeIndex].methods[m_methodIndex];
      if (!m.isConst)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "const object requires const method"});
      return InvokeAs<R>(const_cast<void *>(static_cast<const void *>(std::addressof(obj))),
                         std::forward<A>(a)...);
    }

    // Attributes
    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    NGIN::UInt32 m_typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 m_methodIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 m_typeGeneration{0};
  };

  class Function
  {
  public:
    constexpr Function() = default;
    explicit constexpr Function(FunctionHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      return detail::IsFunctionAlive(reg, m_h);
    }
    [[nodiscard]] std::string_view GetName() const;
    [[nodiscard]] NGIN::UIntSize GetParameterCount() const;
    [[nodiscard]] NGIN::UInt64 GetTypeId() const;

    [[nodiscard]] std::expected<Any, Error> Invoke(const Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<Any, Error> Invoke(std::span<const Any> args) const
    {
      return Invoke(args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }

    template <class R, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(A &&...a) const
    {
      std::array<Any, sizeof...(A)> tmp{Any{std::forward<A>(a)}...};
      auto r = Invoke(tmp.data(), static_cast<NGIN::UIntSize>(tmp.size()));
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

    // Attributes
    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    FunctionHandle m_h{};
  };

  class ResolvedFunction
  {
  public:
    ResolvedFunction() = default;
    ResolvedFunction(NGIN::UInt32 functionIndex,
                     NGIN::Containers::Vector<NGIN::UInt64> argTypeIds,
                     NGIN::Containers::Vector<detail::ConversionKind> conversions)
        : m_functionIndex(functionIndex), m_argTypeIds(std::move(argTypeIds)), m_conversions(std::move(conversions))
    {
    }

    [[nodiscard]] bool IsValid() const noexcept { return m_functionIndex != static_cast<NGIN::UInt32>(-1); }
    [[nodiscard]] Function FunctionHandle() const { return Function{NGIN::Reflection::FunctionHandle{m_functionIndex}}; }
    [[nodiscard]] NGIN::UIntSize ArgumentCount() const { return m_argTypeIds.Size(); }

    [[nodiscard]] std::expected<Any, Error> Invoke(std::span<const Any> args) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsFunctionAlive(reg, NGIN::Reflection::FunctionHandle{m_functionIndex}))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      if (args.size() != m_argTypeIds.Size())
        return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
      for (NGIN::UIntSize i = 0; i < m_argTypeIds.Size(); ++i)
      {
        if (args[i].GetTypeId() != m_argTypeIds[i])
          return std::unexpected(Error{ErrorCode::InvalidArgument, "argument type mismatch"});
      }
      const auto &f = reg.functions[m_functionIndex];
      bool needsConvert = false;
      for (NGIN::UIntSize i = 0; i < m_conversions.Size(); ++i)
      {
        if (m_conversions[i] != detail::ConversionKind::Exact)
        {
          needsConvert = true;
          break;
        }
      }
      if (!needsConvert && f.InvokeExact)
        return f.InvokeExact(args.data(), static_cast<NGIN::UIntSize>(args.size()));
      return f.Invoke(args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }

    [[nodiscard]] std::expected<Any, Error> Invoke(const Any *args, NGIN::UIntSize count) const
    {
      return Invoke(std::span<const Any>{args, count});
    }

    template <class R, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(A &&...a) const
    {
      std::array<Any, sizeof...(A)> tmp{Any{std::forward<A>(a)}...};
      auto r = Invoke(std::span<const Any>{tmp.data(), tmp.size()});
      if (!r.has_value())
        return std::unexpected(r.error());
      if constexpr (std::is_void_v<R>)
        return {};
      return r->template Cast<R>();
    }

  private:
    NGIN::UInt32 m_functionIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::Containers::Vector<NGIN::UInt64> m_argTypeIds;
    NGIN::Containers::Vector<detail::ConversionKind> m_conversions;
  };

  class ResolvedMethod
  {
  public:
    ResolvedMethod() = default;
    ResolvedMethod(NGIN::UInt32 typeIndex,
                   NGIN::UInt32 typeGeneration,
                   NGIN::UInt32 methodIndex,
                   NGIN::Containers::Vector<NGIN::UInt64> argTypeIds,
                   NGIN::Containers::Vector<detail::ConversionKind> conversions)
        : m_typeIndex(typeIndex),
          m_typeGeneration(typeGeneration),
          m_methodIndex(methodIndex),
          m_argTypeIds(std::move(argTypeIds)),
          m_conversions(std::move(conversions))
    {
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (m_typeIndex == static_cast<NGIN::UInt32>(-1))
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsMethodAlive(reg, m_typeIndex, m_typeGeneration, m_methodIndex);
    }
    [[nodiscard]] Method MethodHandle() const { return Method{m_typeIndex, m_methodIndex, m_typeGeneration}; }
    [[nodiscard]] NGIN::UIntSize ArgumentCount() const { return m_argTypeIds.Size(); }

    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, std::span<const Any> args) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsMethodAlive(reg, m_typeIndex, m_typeGeneration, m_methodIndex))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      if (args.size() != m_argTypeIds.Size())
        return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
      for (NGIN::UIntSize i = 0; i < m_argTypeIds.Size(); ++i)
      {
        if (args[i].GetTypeId() != m_argTypeIds[i])
          return std::unexpected(Error{ErrorCode::InvalidArgument, "argument type mismatch"});
      }
      const auto &m = reg.types[m_typeIndex].methods[m_methodIndex];
      bool needsConvert = false;
      for (NGIN::UIntSize i = 0; i < m_conversions.Size(); ++i)
      {
        if (m_conversions[i] != detail::ConversionKind::Exact)
        {
          needsConvert = true;
          break;
        }
      }
      if (!needsConvert && m.InvokeExact)
        return m.InvokeExact(obj, args.data(), static_cast<NGIN::UIntSize>(args.size()));
      return m.Invoke(obj, args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }

    [[nodiscard]] std::expected<Any, Error> Invoke(void *obj, const Any *args, NGIN::UIntSize count) const
    {
      return Invoke(obj, std::span<const Any>{args, count});
    }

    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<Any, Error> Invoke(Obj &obj, std::span<const Any> args) const
    {
      return Invoke(static_cast<void *>(std::addressof(obj)), args);
    }
    template <class Obj>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<Any, Error> Invoke(const Obj &obj, std::span<const Any> args) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsMethodAlive(reg, m_typeIndex, m_typeGeneration, m_methodIndex))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &m = reg.types[m_typeIndex].methods[m_methodIndex];
      if (!m.isConst)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "const object requires const method"});
      return Invoke(const_cast<void *>(static_cast<const void *>(std::addressof(obj))), args);
    }

    template <class R, class... A>
    [[nodiscard]] std::expected<R, Error> InvokeAs(void *obj, A &&...a) const
    {
      std::array<Any, sizeof...(A)> tmp{Any{std::forward<A>(a)}...};
      auto r = Invoke(obj, std::span<const Any>{tmp.data(), tmp.size()});
      if (!r.has_value())
        return std::unexpected(r.error());
      if constexpr (std::is_void_v<R>)
        return {};
      return r->template Cast<R>();
    }

    template <class R, class Obj, class... A>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<R, Error> InvokeAs(Obj &obj, A &&...a) const
    {
      return InvokeAs<R>(static_cast<void *>(std::addressof(obj)), std::forward<A>(a)...);
    }
    template <class R, class Obj, class... A>
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<R, Error> InvokeAs(const Obj &obj, A &&...a) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsMethodAlive(reg, m_typeIndex, m_typeGeneration, m_methodIndex))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &m = reg.types[m_typeIndex].methods[m_methodIndex];
      if (!m.isConst)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "const object requires const method"});
      return InvokeAs<R>(const_cast<void *>(static_cast<const void *>(std::addressof(obj))),
                         std::forward<A>(a)...);
    }

  private:
    NGIN::UInt32 m_typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 m_typeGeneration{0};
    NGIN::UInt32 m_methodIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::Containers::Vector<NGIN::UInt64> m_argTypeIds;
    NGIN::Containers::Vector<detail::ConversionKind> m_conversions;
  };

  class Constructor
  {
  public:
    constexpr Constructor() = default;
    explicit constexpr Constructor(ConstructorHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (!m_h.IsValid())
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsCtorAlive(reg, m_h);
    }
    [[nodiscard]] NGIN::UIntSize ParameterCount() const;
    [[nodiscard]] std::expected<Any, Error> Construct(const Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<Any, Error> Construct(std::span<const Any> args) const
    {
      return Construct(args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }

    // Attributes
    [[nodiscard]] NGIN::UIntSize AttributeCount() const;
    [[nodiscard]] AttributeView AttributeAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<AttributeView, Error> Attribute(std::string_view key) const;

  private:
    ConstructorHandle m_h{};
  };

  class AttributeView
  {
  public:
    AttributeView() = default;
    AttributeView(std::string_view k, const AttrValue *v) : m_key(k), m_val(v) {}
    [[nodiscard]] std::string_view Key() const { return m_key; }
    [[nodiscard]] const AttrValue &Value() const
    {
      static const AttrValue kEmpty{};
      return m_val ? *m_val : kEmpty;
    }

  private:
    std::string_view m_key{};
    const AttrValue *m_val{nullptr};
  };

  class MethodOverloads
  {
  public:
    MethodOverloads() = default;
    MethodOverloads(NGIN::UInt32 typeIndex,
                    NGIN::UInt32 typeGeneration,
                    NameId nameId)
        : m_typeIndex(typeIndex), m_typeGeneration(typeGeneration), m_nameId(nameId)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (m_typeIndex == static_cast<NGIN::UInt32>(-1))
        return false;
      const auto &reg = detail::GetRegistry();
      if (!detail::IsTypeAlive(reg, m_typeIndex, m_typeGeneration))
        return false;
      return reg.types[m_typeIndex].methodOverloads.GetPtr(m_nameId) != nullptr;
    }
    [[nodiscard]] NGIN::UIntSize Size() const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsTypeAlive(reg, m_typeIndex, m_typeGeneration))
        return 0;
      const auto *vec = reg.types[m_typeIndex].methodOverloads.GetPtr(m_nameId);
      return vec ? vec->Size() : 0;
    }
    [[nodiscard]] Method MethodAt(NGIN::UIntSize i) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsTypeAlive(reg, m_typeIndex, m_typeGeneration))
        return Method{};
      const auto *vec = reg.types[m_typeIndex].methodOverloads.GetPtr(m_nameId);
      if (!vec || i >= vec->Size())
        return Method{};
      return Method{m_typeIndex, (*vec)[i], m_typeGeneration};
    }

  private:
    NGIN::UInt32 m_typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 m_typeGeneration{0};
    NameId m_nameId{};
  };

  class FunctionOverloads
  {
  public:
    FunctionOverloads() = default;
    explicit FunctionOverloads(NameId nameId) : m_nameId(nameId) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      auto *vec = reg.functionOverloads.GetPtr(m_nameId);
      if (!vec)
        return false;
      for (NGIN::UIntSize i = 0; i < vec->Size(); ++i)
      {
        if (detail::IsFunctionAlive(reg, FunctionHandle{(*vec)[i]}))
          return true;
      }
      return false;
    }
    [[nodiscard]] NGIN::UIntSize Size() const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      const auto *vec = reg.functionOverloads.GetPtr(m_nameId);
      if (!vec)
        return 0;
      NGIN::UIntSize count = 0;
      for (NGIN::UIntSize i = 0; i < vec->Size(); ++i)
      {
        if (detail::IsFunctionAlive(reg, FunctionHandle{(*vec)[i]}))
          ++count;
      }
      return count;
    }
    [[nodiscard]] Function FunctionAt(NGIN::UIntSize i) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      const auto *vec = reg.functionOverloads.GetPtr(m_nameId);
      if (!vec)
        return Function{};
      NGIN::UIntSize seen = 0;
      for (NGIN::UIntSize idx = 0; idx < vec->Size(); ++idx)
      {
        auto handle = FunctionHandle{(*vec)[idx]};
        if (!detail::IsFunctionAlive(reg, handle))
          continue;
        if (seen == i)
          return Function{handle};
        ++seen;
      }
      return Function{};
    }

  private:
    NameId m_nameId{};
  };

  class Type
  {
  public:
    constexpr Type() = default;
    explicit constexpr Type(TypeHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (!m_h.IsValid())
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsTypeAlive(reg, m_h);
    }
    [[nodiscard]] std::string_view QualifiedName() const;
    [[nodiscard]] NGIN::UInt64 GetTypeId() const;
    [[nodiscard]] NGIN::UIntSize Size() const;
    [[nodiscard]] NGIN::UIntSize Alignment() const;

    [[nodiscard]] NGIN::UIntSize FieldCount() const;
    [[nodiscard]] Field FieldAt(NGIN::UIntSize i) const;
    [[nodiscard]] ExpectedField GetField(std::string_view name) const;
    [[nodiscard]] std::optional<Field> FindField(std::string_view name) const;

    [[nodiscard]] NGIN::UIntSize PropertyCount() const;
    [[nodiscard]] Property PropertyAt(NGIN::UIntSize i) const;
    [[nodiscard]] ExpectedProperty GetProperty(std::string_view name) const;
    [[nodiscard]] std::optional<Property> FindProperty(std::string_view name) const;

    [[nodiscard]] bool IsEnum() const;
    [[nodiscard]] NGIN::UInt64 EnumUnderlyingTypeId() const;
    [[nodiscard]] NGIN::UIntSize EnumValueCount() const;
    [[nodiscard]] EnumValue EnumValueAt(NGIN::UIntSize i) const;
    [[nodiscard]] ExpectedEnumValue GetEnumValue(std::string_view name) const;
    [[nodiscard]] std::optional<EnumValue> FindEnumValue(std::string_view name) const;
    [[nodiscard]] std::expected<Any, Error> ParseEnum(std::string_view name) const;
    [[nodiscard]] std::optional<std::string_view> EnumName(const Any &value) const;

    [[nodiscard]] NGIN::UIntSize MethodCount() const;
    [[nodiscard]] Method MethodAt(NGIN::UIntSize i) const;
    [[nodiscard]] std::expected<Method, Error> GetMethod(std::string_view name) const;
    [[nodiscard]] std::optional<Method> FindMethod(std::string_view name) const;
    [[nodiscard]] MethodOverloads FindMethods(std::string_view name) const;
    [[nodiscard]] std::expected<ResolvedMethod, Error> ResolveMethod(std::string_view name, const Any *args, NGIN::UIntSize count) const;
    [[nodiscard]] std::expected<ResolvedMethod, Error> ResolveMethod(std::string_view name, std::span<const Any> args) const
    {
      return ResolveMethod(name, args.data(), static_cast<NGIN::UIntSize>(args.size()));
    }

    // Resolve by compile-time signature (exact match on parameter and, if non-void, return type)
    template <class R = void, class... A>
      requires(!detail::FunctionSignature<R>)
    [[nodiscard]] std::expected<Method, Error> ResolveMethod(std::string_view name) const
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      const auto &reg = detail::GetRegistry();
      if (!detail::IsTypeAlive(reg, m_h))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "stale handle"});
      const auto &tdesc = reg.types[m_h.index];
      NameId nid{};
      (void)detail::FindNameId(name, nid);
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
          return Method{m_h.index, mi, m_h.generation};
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
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<Any, Error> Invoke(std::string_view name, Obj &&obj, A &&...a) const
    {
      using ObjT = std::remove_reference_t<Obj>;
      if constexpr (std::is_const_v<ObjT>)
      {
        auto m = ResolveMethod<R, A...>(name);
        if (!m.has_value())
          return std::unexpected(m.error());
        std::array<Any, sizeof...(A)> tmp{Any{std::forward<A>(a)}...};
        return m->Invoke(obj, std::span<const Any>{tmp.data(), tmp.size()});
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
      requires(!std::is_pointer_v<std::remove_reference_t<Obj>>)
    [[nodiscard]] std::expected<R, Error> InvokeAs(std::string_view name, Obj &&obj, A &&...a) const
    {
      using ObjT = std::remove_reference_t<Obj>;
      if constexpr (std::is_const_v<ObjT>)
      {
        auto m = ResolveMethod<R, A...>(name);
        if (!m)
          return std::unexpected(m.error());
        return m->template InvokeAs<R>(obj, std::forward<A>(a)...);
      }
      else
      {
        return InvokeAs<R>(name, static_cast<void *>(std::addressof(obj)), std::forward<A>(a)...);
      }
    }

    // Constructors
    [[nodiscard]] NGIN::UIntSize ConstructorCount() const;
    [[nodiscard]] Constructor ConstructorAt(NGIN::UIntSize i) const;
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

    // Unified member enumeration
    [[nodiscard]] NGIN::UIntSize MemberCount() const;
    [[nodiscard]] Member MemberAt(NGIN::UIntSize i) const;

    // Base-class metadata
    [[nodiscard]] NGIN::UIntSize BaseCount() const;
    [[nodiscard]] Base BaseAt(NGIN::UIntSize i) const;
    [[nodiscard]] ExpectedBase GetBase(const Type &base) const;
    [[nodiscard]] std::optional<Base> FindBase(const Type &base) const;
    [[nodiscard]] bool IsDerivedFrom(const Type &base) const;

  private:
    template <class Sig, std::size_t... I>
    [[nodiscard]] std::expected<Method, Error> ResolveMethodBySignature(std::string_view name, std::index_sequence<I...>) const
    {
      using Traits = detail::SignatureTraits<Sig>;
      return ResolveMethod<typename Traits::Ret, std::tuple_element_t<I, typename Traits::Args>...>(name);
    }

    TypeHandle m_h{};
  };

  class Member
  {
  public:
    constexpr Member() = default;
    explicit constexpr Member(MemberHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (!m_h.IsValid())
        return false;
      const auto &reg = detail::GetRegistry();
      if (!detail::IsTypeAlive(reg, m_h.typeIndex, m_h.typeGeneration))
        return false;
      const auto &t = reg.types[m_h.typeIndex];
      switch (m_h.kind)
      {
      case MemberKind::Field:
        return m_h.memberIndex < t.fields.Size();
      case MemberKind::Property:
        return m_h.memberIndex < t.properties.Size();
      case MemberKind::Method:
        return m_h.memberIndex < t.methods.Size();
      case MemberKind::Constructor:
        return m_h.memberIndex < t.constructors.Size();
      default:
        break;
      }
      return false;
    }
    [[nodiscard]] MemberKind Kind() const noexcept { return m_h.kind; }

    [[nodiscard]] bool IsField() const noexcept { return m_h.kind == MemberKind::Field; }
    [[nodiscard]] bool IsProperty() const noexcept { return m_h.kind == MemberKind::Property; }
    [[nodiscard]] bool IsMethod() const noexcept { return m_h.kind == MemberKind::Method; }
    [[nodiscard]] bool IsConstructor() const noexcept { return m_h.kind == MemberKind::Constructor; }

    [[nodiscard]] Field AsField() const { return Field{FieldHandle{m_h.typeIndex, m_h.memberIndex, m_h.typeGeneration}}; }
    [[nodiscard]] Property AsProperty() const { return Property{PropertyHandle{m_h.typeIndex, m_h.memberIndex, m_h.typeGeneration}}; }
    [[nodiscard]] Method AsMethod() const { return Method{m_h.typeIndex, m_h.memberIndex, m_h.typeGeneration}; }
    [[nodiscard]] Constructor AsConstructor() const { return Constructor{ConstructorHandle{m_h.typeIndex, m_h.memberIndex, m_h.typeGeneration}}; }

  private:
    MemberHandle m_h{};
  };

  class Base
  {
  public:
    constexpr Base() = default;
    explicit constexpr Base(BaseHandle h) : m_h(h) {}

    [[nodiscard]] bool IsValid() const noexcept
    {
      [[maybe_unused]] auto lock = detail::LockRegistryRead();
      if (!m_h.IsValid())
        return false;
      const auto &reg = detail::GetRegistry();
      return detail::IsBaseAlive(reg, m_h);
    }
    [[nodiscard]] Type BaseType() const;
    [[nodiscard]] void *Upcast(void *obj) const;
    [[nodiscard]] const void *Upcast(const void *obj) const;
    [[nodiscard]] void *Downcast(void *obj) const;
    [[nodiscard]] const void *Downcast(const void *obj) const;
    [[nodiscard]] bool CanDowncast() const;

  private:
    BaseHandle m_h{};
  };

  // Function registry queries
  [[nodiscard]] NGIN::UIntSize FunctionCount();
  [[nodiscard]] Function FunctionAt(NGIN::UIntSize i);
  [[nodiscard]] ExpectedFunction GetFunction(std::string_view name);
  [[nodiscard]] std::optional<Function> FindFunction(std::string_view name);
  [[nodiscard]] FunctionOverloads FindFunctions(std::string_view name);
  [[nodiscard]] ExpectedResolvedFunction ResolveFunction(std::string_view name, const Any *args, NGIN::UIntSize count);
  [[nodiscard]] inline ExpectedResolvedFunction ResolveFunction(std::string_view name, std::span<const Any> args)
  {
    return ResolveFunction(name, args.data(), static_cast<NGIN::UIntSize>(args.size()));
  }

  template <class R, class... A>
    requires(!detail::FunctionSignature<R>)
  [[nodiscard]] ExpectedFunction ResolveFunction(std::string_view name);

  template <class Sig, std::size_t... I>
  [[nodiscard]] ExpectedFunction ResolveFunctionBySignature(std::string_view name, std::index_sequence<I...>)
  {
    using Traits = detail::SignatureTraits<Sig>;
    return ResolveFunction<typename Traits::Ret, std::tuple_element_t<I, typename Traits::Args>...>(name);
  }

  // Resolve by compile-time signature (exact match on parameter and, if non-void, return type)
  template <class R = void, class... A>
    requires(!detail::FunctionSignature<R>)
  [[nodiscard]] ExpectedFunction ResolveFunction(std::string_view name)
  {
    [[maybe_unused]] auto lock = detail::LockRegistryRead();
    const auto &reg = detail::GetRegistry();
    NameId nid{};
    (void)detail::FindNameId(name, nid);
    auto *vec = reg.functionOverloads.GetPtr(nid);
    if (!vec)
      return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
    constexpr NGIN::UIntSize N = sizeof...(A);
    NGIN::UInt64 desired[N == 0 ? 1 : N] = {detail::TypeIdOf<std::remove_cv_t<std::remove_reference_t<A>>>()...};
    const NGIN::UInt64 desiredRet = []
    {
      if constexpr (std::is_void_v<R>)
        return NGIN::UInt64{0};
      else
        return detail::TypeIdOf<std::remove_cv_t<std::remove_reference_t<R>>>();
    }();
    bool anyAlive = false;
    for (NGIN::UIntSize k = 0; k < vec->Size(); ++k)
    {
      auto fi = (*vec)[k];
      if (!detail::IsFunctionAlive(reg, FunctionHandle{fi}))
        continue;
      anyAlive = true;
      const auto &f = reg.functions[fi];
      if constexpr (!std::is_void_v<R>)
      {
        if (f.returnTypeId != desiredRet)
          continue;
      }
      else
      {
        if (f.returnTypeId != 0)
          continue;
      }
      if (f.paramTypeIds.Size() != N)
        continue;
      bool all = true;
      for (NGIN::UIntSize i = 0; i < N; ++i)
      {
        if (f.paramTypeIds[i] != desired[i])
        {
          all = false;
          break;
        }
      }
      if (all)
        return Function{FunctionHandle{fi}};
    }
    if (!anyAlive)
      return std::unexpected(Error{ErrorCode::NotFound, "no overloads"});
    return std::unexpected(Error{ErrorCode::InvalidArgument, "no exact match"});
  }

  template <class Sig>
    requires detail::FunctionSignature<Sig>
  [[nodiscard]] ExpectedFunction ResolveFunction(std::string_view name)
  {
    using Traits = detail::SignatureTraits<Sig>;
    constexpr auto N = std::tuple_size_v<typename Traits::Args>;
    return ResolveFunctionBySignature<Sig>(name, std::make_index_sequence<N>{});
  }

  // Register a free or static function in the global registry
  template <auto Fn>
  Function RegisterFunction(std::string_view name);

  // Queries
  ExpectedType GetType(std::string_view name);
  std::optional<Type> FindType(std::string_view name);
  bool UnregisterModule(ModuleId moduleId);

  template <class T>
  Type GetType()
  {
    [[maybe_unused]] auto lock = detail::LockRegistryWrite();
    auto idx = detail::EnsureRegistered<T>();
    const auto &reg = detail::GetRegistry();
    return Type{TypeHandle{idx, reg.types[idx].generation}};
  }

  template <class T>
  std::optional<Type> TryGetType()
  {
    using U = std::remove_cvref_t<T>;
    [[maybe_unused]] auto lock = detail::LockRegistryRead();
    auto &reg = detail::GetRegistry();
    const auto tid = detail::TypeIdOf<U>();
    if (auto *p = reg.byTypeId.GetPtr(tid))
      return Type{TypeHandle{*p, reg.types[*p].generation}};
    return std::nullopt;
  }

  // Optional eager registration helper
  template <class T>
  inline bool AutoRegister()
  {
    [[maybe_unused]] auto lock = detail::LockRegistryWrite();
    (void)detail::EnsureRegistered<T>();
    return true;
  }

} // namespace NGIN::Reflection
