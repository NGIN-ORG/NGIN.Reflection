#pragma once

#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Meta/TypeId.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <NGIN/Reflection/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace NGIN::Reflection
{
  template <class T>
  struct Tag
  {
    using type = T;
  };

  template <class T>
  class TypeBuilder;

  template <class T>
  struct Describe;

  class AttributeView
  {
  public:
    AttributeView() = default;
    AttributeView(std::string_view name, AttributeValue value)
        : m_name(name),
          m_value(std::move(value)),
          m_valid(true)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept { return m_valid; }
    [[nodiscard]] std::string_view Name() const noexcept { return m_name; }
    [[nodiscard]] const AttributeValue &Value() const noexcept { return m_value; }

  private:
    std::string_view m_name{};
    AttributeValue m_value{};
    bool m_valid{false};
  };

  /// @brief Typed dependency binding attached to an injectable constructor parameter.
  struct ConstructorParameterBinding
  {
    std::string name{};
    bool optional{false};
  };

  class EnumValue
  {
  public:
    EnumValue() = default;
    EnumValue(std::string_view name, std::int64_t signedValue, std::uint64_t unsignedValue)
        : m_name(name),
          m_signedValue(signedValue),
          m_unsignedValue(unsignedValue),
          m_valid(true)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept { return m_valid; }
    [[nodiscard]] std::string_view Name() const noexcept { return m_name; }
    [[nodiscard]] std::int64_t SignedValue() const noexcept { return m_signedValue; }
    [[nodiscard]] std::uint64_t UnsignedValue() const noexcept { return m_unsignedValue; }

  private:
    std::string_view m_name{};
    std::int64_t m_signedValue{0};
    std::uint64_t m_unsignedValue{0};
    bool m_valid{false};
  };

  namespace detail
  {
    inline constexpr std::string_view InjectableConstructorAttribute = "NGIN.Reflection.Injectable";
    inline constexpr std::string_view ConstructorParameterPrefix = "NGIN.Reflection.Parameter.";

    struct TypeReference
    {
      SymbolId qualifiedName{};
      std::uint64_t fastHash{0};
      std::uint64_t reflectedTypeKey{0};
    };

    struct AttributeRecord
    {
      SymbolId name{};
      AttributeValue value{};
    };

    struct EnumRecord
    {
      SymbolId name{};
      std::int64_t signedValue{0};
      std::uint64_t unsignedValue{0};
    };

    struct FieldRecord
    {
      SymbolId name{};
      TypeReference valueType{};
      std::vector<AttributeRecord> attributes;
      std::uint32_t readSlot{0};
      std::uint32_t writeSlot{0};
    };

    struct PropertyRecord
    {
      SymbolId name{};
      TypeReference valueType{};
      std::vector<AttributeRecord> attributes;
      std::uint32_t readSlot{0};
      std::uint32_t writeSlot{0};
    };

    struct MethodRecord
    {
      SymbolId name{};
      TypeReference returnType{};
      std::vector<TypeReference> parameters;
      std::vector<AttributeRecord> attributes;
      std::uint32_t invokeSlot{0};
      bool isConst{false};
    };

    struct ConstructorRecord
    {
      std::vector<TypeReference> parameters;
      std::vector<AttributeRecord> attributes;
      std::uint32_t constructSlot{0};
    };

    struct BaseRecord
    {
      SymbolId baseName{};
      TypeIdentity baseIdentity{};
      std::uint32_t upcastSlot{0};
      std::uint32_t downcastSlot{0};
    };

    struct TypeRecord
    {
      SymbolId qualifiedName{};
      TypeIdentity identity{};
      std::size_t sizeBytes{0};
      std::size_t alignBytes{0};
      std::vector<FieldRecord> fields;
      std::vector<PropertyRecord> properties;
      std::vector<MethodRecord> methods;
      std::vector<ConstructorRecord> constructors;
      std::vector<EnumRecord> enumValues;
      std::vector<BaseRecord> bases;
      std::vector<AttributeRecord> attributes;
      bool isEnum{false};
      bool enumSigned{true};
      TypeReference enumUnderlyingType{};
    };

    struct FunctionRecord
    {
      SymbolId name{};
      TypeReference returnType{};
      std::vector<TypeReference> parameters;
      std::vector<AttributeRecord> attributes;
      std::uint32_t invokeSlot{0};
    };

    struct ModuleCallTables
    {
      std::vector<NGINReflectionFieldReadFn> fieldReaders;
      std::vector<NGINReflectionFieldWriteFn> fieldWriters;
      std::vector<NGINReflectionPropertyReadFn> propertyReaders;
      std::vector<NGINReflectionPropertyWriteFn> propertyWriters;
      std::vector<NGINReflectionMethodInvokeFn> methodInvokers;
      std::vector<NGINReflectionConstructorInvokeFn> constructors;
      std::vector<NGINReflectionFunctionInvokeFn> functionInvokers;
      std::vector<NGINReflectionBaseCastFn> upcasters;
      std::vector<NGINReflectionBaseCastFn> downcasters;
    };

    struct ModuleRecord
    {
      ModuleIdentity identity{};
      std::vector<TypeRecord> types;
      std::vector<FunctionRecord> functions;
      ModuleCallTables tables;
      bool imported{false};
      std::shared_ptr<void> lifetime;
      std::shared_ptr<std::atomic<std::size_t>> liveInstances{
          std::make_shared<std::atomic<std::size_t>>(0u)};
    };

    struct TypeBinding
    {
      std::size_t moduleIndex{0};
      std::size_t typeIndex{0};
    };

    struct FunctionBinding
    {
      std::size_t moduleIndex{0};
      std::size_t functionIndex{0};
    };

    struct RegistryState
    {
      std::uint64_t generation{1};
      std::vector<std::shared_ptr<ModuleRecord>> modules;
      std::vector<TypeBinding> types;
      std::vector<FunctionBinding> functions;
      std::unordered_map<std::uint64_t, std::size_t> typesByKey;
      std::unordered_map<std::uint64_t, std::vector<std::size_t>> typesByNameHash;
      std::unordered_map<std::uint64_t, std::vector<std::size_t>> functionsByNameHash;
      std::unordered_map<std::uint64_t, std::size_t> modulesByKey;
    };

    struct ModuleBuildState
    {
      ModuleIdentity identity{};
      std::vector<TypeRecord> types;
      std::vector<FunctionRecord> functions;
      ModuleCallTables tables;
      std::unordered_map<std::uint64_t, std::size_t> typeNameToIndex;
      bool committed{false};
    };

    struct Handle
    {
      std::size_t typeIndex{static_cast<std::size_t>(-1)};
      std::size_t memberIndex{static_cast<std::size_t>(-1)};
      std::uint64_t generation{0};

      [[nodiscard]] constexpr bool IsValid() const noexcept
      {
        return generation != 0 && typeIndex != static_cast<std::size_t>(-1);
      }
    };

    struct FunctionHandle
    {
      std::size_t functionIndex{static_cast<std::size_t>(-1)};
      std::uint64_t generation{0};

      [[nodiscard]] constexpr bool IsValid() const noexcept
      {
        return generation != 0 && functionIndex != static_cast<std::size_t>(-1);
      }
    };

    struct TypeBuildAnchor
    {
      ModuleBuildState *module{nullptr};
      std::size_t typeIndex{0};
    };

    [[nodiscard]] SymbolId InternSymbol(std::string_view name);
    [[nodiscard]] bool TryGetSymbol(std::string_view name, SymbolId &out) noexcept;
    [[nodiscard]] std::string_view ViewSymbol(SymbolId id) noexcept;

    [[nodiscard]] TypeReference MakeTypeReference(std::string_view qualifiedName);

    [[nodiscard]] Error MakeError(ErrorCode code, std::string message);
    [[nodiscard]] std::expected<NGINReflectionStatus, Error> ImportModuleUnchecked(const NGINReflectionModuleApi &api);
    [[nodiscard]] std::shared_ptr<const RegistryState> CurrentState() noexcept;
    [[nodiscard]] RegistrySnapshot Snapshot() noexcept;
    [[nodiscard]] bool InstallModule(std::shared_ptr<ModuleRecord> module, Error *error) noexcept;
    [[nodiscard]] bool UnloadModule(const ModuleIdentity &identity, Error *error) noexcept;
    [[nodiscard]] std::expected<Handle, Error> FindTypeHandle(std::string_view qualifiedName) noexcept;
    [[nodiscard]] std::expected<Handle, Error> FindTypeHandle(const TypeIdentity &identity) noexcept;
    [[nodiscard]] std::expected<FunctionHandle, Error> FindFunctionHandle(std::string_view name) noexcept;

    [[nodiscard]] std::shared_ptr<ModuleRecord> BuildModuleRecord(const ModuleBuildState &state);

    template <class T>
    concept HasNginReflectWithTypeBuilder = requires(TypeBuilder<T> &builder) {
      { NginReflect(Tag<T>{}, builder) } -> std::same_as<void>;
    };

    template <class, class = void>
    struct HasDescribeImpl : std::false_type
    {
    };

    template <class T>
    struct HasDescribeImpl<T, std::void_t<decltype(Describe<T>::Do(std::declval<TypeBuilder<T> &>()))>>
        : std::true_type
    {
    };

    template <class T>
    concept HasDescribeWithTypeBuilder = HasDescribeImpl<T>::value;
  } // namespace detail

  class Field
  {
  public:
    Field() = default;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] std::string_view ValueTypeName() const;
    [[nodiscard]] ExpectedValue Read(const ConstInstanceRef &instance) const;
    [[nodiscard]] std::expected<void, Error> Write(InstanceRef &instance, const Value &value) const;
    [[nodiscard]] std::size_t AttributeCount() const;
    [[nodiscard]] ExpectedAttribute AttributeAt(std::size_t index) const;

  private:
    explicit Field(detail::Handle handle) noexcept
        : m_handle(handle)
    {
    }

    detail::Handle m_handle{};

    friend class Type;
  };

  class Property
  {
  public:
    Property() = default;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] std::string_view ValueTypeName() const;
    [[nodiscard]] ExpectedValue Read(const ConstInstanceRef &instance) const;
    [[nodiscard]] std::expected<void, Error> Write(InstanceRef &instance, const Value &value) const;
    [[nodiscard]] std::size_t AttributeCount() const;
    [[nodiscard]] ExpectedAttribute AttributeAt(std::size_t index) const;

  private:
    explicit Property(detail::Handle handle) noexcept
        : m_handle(handle)
    {
    }

    detail::Handle m_handle{};

    friend class Type;
  };

  class Method
  {
  public:
    Method() = default;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] std::size_t ParameterCount() const;
    [[nodiscard]] std::string_view ParameterTypeName(std::size_t index) const;
    [[nodiscard]] std::string_view ReturnTypeName() const;
    [[nodiscard]] ExpectedValue Invoke(const InstanceRef &instance, std::span<const Value> arguments) const;
    [[nodiscard]] std::size_t AttributeCount() const;
    [[nodiscard]] ExpectedAttribute AttributeAt(std::size_t index) const;

  private:
    explicit Method(detail::Handle handle) noexcept
        : m_handle(handle)
    {
    }

    detail::Handle m_handle{};

    friend class Type;
  };

  class Constructor
  {
  public:
    Constructor() = default;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::size_t ParameterCount() const;
    [[nodiscard]] std::string_view ParameterTypeName(std::size_t index) const;
    [[nodiscard]] ExpectedInstance Invoke(std::span<const Value> arguments) const;
    [[nodiscard]] bool IsInjectable() const;
    [[nodiscard]] std::expected<ConstructorParameterBinding, Error>
    ParameterBindingAt(std::size_t index) const;
    [[nodiscard]] std::size_t AttributeCount() const;
    [[nodiscard]] ExpectedAttribute AttributeAt(std::size_t index) const;

  private:
    explicit Constructor(detail::Handle handle) noexcept
        : m_handle(handle)
    {
    }

    detail::Handle m_handle{};

    friend class Type;
  };

  class Base
  {
  public:
    Base() = default;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] ExpectedInstance Upcast(const InstanceRef &instance) const;
    [[nodiscard]] ExpectedInstance Downcast(const InstanceRef &instance) const;

  private:
    explicit Base(detail::Handle handle) noexcept
        : m_handle(handle)
    {
    }

    detail::Handle m_handle{};

    friend class Type;
  };

  class Function
  {
  public:
    Function() = default;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::string_view Name() const;
    [[nodiscard]] std::size_t ParameterCount() const;
    [[nodiscard]] std::string_view ParameterTypeName(std::size_t index) const;
    [[nodiscard]] std::string_view ReturnTypeName() const;
    [[nodiscard]] ExpectedValue Invoke(std::span<const Value> arguments) const;
    [[nodiscard]] std::size_t AttributeCount() const;
    [[nodiscard]] ExpectedAttribute AttributeAt(std::size_t index) const;

  private:
    explicit Function(detail::FunctionHandle handle) noexcept
        : m_handle(handle)
    {
    }

    detail::FunctionHandle m_handle{};

    friend ExpectedFunction GetFunction(std::string_view name);
    friend std::optional<Function> FindFunction(std::string_view name);
  };

  class Type
  {
  public:
    Type() = default;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] TypeIdentity Identity() const;
    [[nodiscard]] std::string_view QualifiedName() const;
    [[nodiscard]] std::size_t FieldCount() const;
    [[nodiscard]] std::size_t PropertyCount() const;
    [[nodiscard]] std::size_t MethodCount() const;
    [[nodiscard]] std::size_t ConstructorCount() const;
    [[nodiscard]] std::size_t EnumValueCount() const;
    [[nodiscard]] std::size_t BaseCount() const;
    [[nodiscard]] std::size_t AttributeCount() const;
    [[nodiscard]] bool IsEnum() const;
    [[nodiscard]] ExpectedField FieldAt(std::size_t index) const;
    [[nodiscard]] ExpectedProperty PropertyAt(std::size_t index) const;
    [[nodiscard]] ExpectedMethod MethodAt(std::size_t index) const;
    [[nodiscard]] ExpectedConstructor ConstructorAt(std::size_t index) const;
    [[nodiscard]] ExpectedEnumValue EnumValueAt(std::size_t index) const;
    [[nodiscard]] ExpectedBase BaseAt(std::size_t index) const;
    [[nodiscard]] ExpectedAttribute AttributeAt(std::size_t index) const;
    [[nodiscard]] ExpectedField GetField(std::string_view name) const;
    [[nodiscard]] ExpectedProperty GetProperty(std::string_view name) const;
    [[nodiscard]] ExpectedMethod GetMethod(std::string_view name) const;
    [[nodiscard]] ExpectedMethod ResolveMethod(std::string_view name, std::span<const Value> arguments) const;
    [[nodiscard]] ExpectedConstructor ResolveConstructor(std::span<const Value> arguments) const;
    [[nodiscard]] ExpectedInstance Construct(std::span<const Value> arguments = {}) const;
    [[nodiscard]] ExpectedEnumValue GetEnumValue(std::string_view name) const;

  private:
    explicit Type(detail::Handle handle) noexcept
        : m_handle(handle)
    {
    }

    detail::Handle m_handle{};

    friend ExpectedType GetType(std::string_view qualifiedName);
    friend std::optional<Type> FindType(std::string_view qualifiedName);
  };

  [[nodiscard]] RegistrySnapshot GetRegistrySnapshot() noexcept;
  [[nodiscard]] bool ImportModule(const NGINReflectionModuleApi &api, Error *error = nullptr) noexcept;
  [[nodiscard]] bool UnloadModule(const ModuleIdentity &identity, Error *error = nullptr) noexcept;
  [[nodiscard]] ExpectedType GetType(std::string_view qualifiedName);
  [[nodiscard]] std::optional<Type> FindType(std::string_view qualifiedName);
  [[nodiscard]] ExpectedFunction GetFunction(std::string_view name);
  [[nodiscard]] std::optional<Function> FindFunction(std::string_view name);
  [[nodiscard]] InstanceRef AdoptInstance(
      NGINReflectionInstanceHandle handle,
      std::shared_ptr<void> parentLifetime = {});

  template <class T>
  [[nodiscard]] ExpectedType GetType()
  {
    return GetType(NGIN::Meta::TypeName<std::remove_cvref_t<T>>::qualifiedName);
  }

  template <class T>
  [[nodiscard]] std::optional<Type> FindType()
  {
    return FindType(NGIN::Meta::TypeName<std::remove_cvref_t<T>>::qualifiedName);
  }
} // namespace NGIN::Reflection
