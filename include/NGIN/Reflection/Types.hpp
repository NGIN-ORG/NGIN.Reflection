#pragma once

#include <NGIN/Meta/ReflectionIdentity.hpp>
#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Utilities/Any.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace NGIN::Reflection
{
  using SymbolId = NGIN::Meta::SymbolId;
  using ModuleIdentity = NGIN::Meta::ModuleIdentity;
  using TypeIdentity = NGIN::Meta::TypeIdentity;
  using Any = NGIN::Utilities::Any<>;
  using AnyView = Any::View;
  using ConstAnyView = Any::ConstView;

  namespace detail
  {
    struct InstanceStorage;
  }

  enum class ErrorCode : std::uint32_t
  {
    NotFound = NGINReflectionStatus_NotFound,
    InvalidArgument = NGINReflectionStatus_InvalidArgument,
    StaleHandle = NGINReflectionStatus_StaleHandle,
    AbiMismatch = NGINReflectionStatus_AbiMismatch,
    Conflict = NGINReflectionStatus_Conflict,
    CorruptModule = NGINReflectionStatus_CorruptModule,
    InternalError = NGINReflectionStatus_InternalError,
  };

  struct Error
  {
    ErrorCode code{ErrorCode::InternalError};
    std::string message;
  };

  using AttributeValue = std::variant<bool, std::int64_t, std::uint64_t, double, std::string, SymbolId, TypeIdentity>;

  class ConstInstanceRef
  {
  public:
    ConstInstanceRef() = default;
    explicit ConstInstanceRef(std::shared_ptr<detail::InstanceStorage> storage) noexcept
        : m_storage(std::move(storage))
    {
    }

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] TypeIdentity Identity() const noexcept;
    [[nodiscard]] const void *Data() const noexcept;
    [[nodiscard]] const NGINReflectionInstanceHandle *AbiHandle() const noexcept;
    [[nodiscard]] std::shared_ptr<void> LifetimeToken() const noexcept;

    template <class T>
    [[nodiscard]] const T *TryAs() const noexcept
    {
      return static_cast<const T *>(Data());
    }

  protected:
    std::shared_ptr<detail::InstanceStorage> m_storage{};

    friend class InstanceRef;
    friend class Value;
  };

  class InstanceRef final : public ConstInstanceRef
  {
  public:
    InstanceRef() = default;
    explicit InstanceRef(std::shared_ptr<detail::InstanceStorage> storage) noexcept
        : ConstInstanceRef(std::move(storage))
    {
    }

    [[nodiscard]] void *Data() noexcept;
    [[nodiscard]] const NGINReflectionInstanceHandle *AbiHandle() const noexcept;
    [[nodiscard]] NGINReflectionInstanceHandle *AbiHandle() noexcept;

    template <class T>
    [[nodiscard]] T *TryAs() noexcept
    {
      return static_cast<T *>(Data());
    }

  private:
    friend class Value;
    friend class Field;
    friend class Property;
    friend class Method;
    friend class Constructor;
    friend class Base;
    friend class Function;
    friend class ModuleRegistration;
  };

  class Value;

  class ConstValueView
  {
  public:
    ConstValueView() = default;
    explicit ConstValueView(const Value *value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] bool IsInstance() const noexcept;
    [[nodiscard]] const Any *TryAny() const noexcept;
    [[nodiscard]] std::optional<ConstInstanceRef> TryAsInstance() const noexcept;
    [[nodiscard]] std::optional<ConstAnyView> TryAsAnyView() const noexcept;

    template <class T>
    [[nodiscard]] const T *TryAs() const noexcept
    {
      if (const auto *boxed = TryAny())
        return boxed->TryCast<T>();
      return nullptr;
    }

  private:
    const Value *m_value{nullptr};
  };

  class ValueView
  {
  public:
    ValueView() = default;
    explicit ValueView(Value *value) noexcept
        : m_value(value)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool IsInstance() const noexcept;
    [[nodiscard]] Any *TryAny() noexcept;
    [[nodiscard]] std::optional<InstanceRef> TryAsInstance() noexcept;
    [[nodiscard]] std::optional<AnyView> TryAsAnyView() noexcept;

    template <class T>
    [[nodiscard]] T *TryAs() noexcept
    {
      if (auto *boxed = TryAny())
        return boxed->TryCast<T>();
      return nullptr;
    }

  private:
    Value *m_value{nullptr};
  };

  class Value
  {
  public:
    Value() = default;
    explicit Value(bool value);
    explicit Value(std::int64_t value);
    explicit Value(std::uint64_t value);
    explicit Value(double value);
    explicit Value(std::string value);
    explicit Value(const char *value);
    explicit Value(Any value);

    [[nodiscard]] static Value FromInstance(const InstanceRef &instance);

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] bool IsInstance() const noexcept;
    [[nodiscard]] const Any *TryAny() const noexcept;
    [[nodiscard]] Any *TryAny() noexcept;
    [[nodiscard]] std::optional<ConstInstanceRef> TryAsInstance() const noexcept;
    [[nodiscard]] std::optional<InstanceRef> TryAsInstance() noexcept;
    [[nodiscard]] ConstValueView View() const noexcept;
    [[nodiscard]] ValueView View() noexcept;

    template <class T>
    [[nodiscard]] const T *TryAs() const noexcept
    {
      if (const auto *boxed = TryAny())
        return boxed->TryCast<T>();
      return nullptr;
    }

    template <class T>
    [[nodiscard]] T *TryAs() noexcept
    {
      if (auto *boxed = TryAny())
        return boxed->TryCast<T>();
      return nullptr;
    }

  private:
    std::variant<std::monostate, Any, std::shared_ptr<detail::InstanceStorage>> m_storage{};

    friend class ConstValueView;
    friend class ValueView;
    friend class Field;
    friend class Property;
    friend class Method;
    friend class Constructor;
    friend class Function;
    friend class Base;
    friend class Type;
  };

  class Type;
  class Field;
  class Property;
  class Method;
  class Constructor;
  class EnumValue;
  class Base;
  class Function;
  class AttributeView;

  using ExpectedType = std::expected<Type, Error>;
  using ExpectedField = std::expected<Field, Error>;
  using ExpectedProperty = std::expected<Property, Error>;
  using ExpectedMethod = std::expected<Method, Error>;
  using ExpectedConstructor = std::expected<Constructor, Error>;
  using ExpectedEnumValue = std::expected<EnumValue, Error>;
  using ExpectedBase = std::expected<Base, Error>;
  using ExpectedFunction = std::expected<Function, Error>;
  using ExpectedValue = std::expected<Value, Error>;
  using ExpectedInstance = std::expected<InstanceRef, Error>;
  using ExpectedAttribute = std::expected<AttributeView, Error>;

  struct RegistrySnapshot
  {
    std::uint64_t generation{0};
    std::size_t moduleCount{0};
    std::size_t typeCount{0};
    std::size_t functionCount{0};
  };
} // namespace NGIN::Reflection
