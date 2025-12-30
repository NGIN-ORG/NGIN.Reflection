// Types.hpp
// Public-facing error codes and small ABI-stable handle types
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Utilities/Any.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <string_view>
#include <expected>
#include <utility>
#include <optional>

namespace NGIN::Reflection
{

  using Any = NGIN::Utilities::Any<>;
  using ModuleId = NGIN::UInt64;

  enum class ErrorCode : unsigned
  {
    NotFound = 1,
    InvalidArgument = 2,
  };

  enum class DiagnosticCode : unsigned
  {
    None = 0,
    ArityMismatch = 1,
    NonConvertible = 2,
    NoOverloads = 3,
  };

  struct OverloadDiagnostic
  {
    NGIN::UInt32 methodIndex{static_cast<NGIN::UInt32>(-1)};
    std::string_view name{};
    NGIN::UIntSize arity{0};
    DiagnosticCode code{DiagnosticCode::None};
    NGIN::UIntSize argIndex{static_cast<NGIN::UIntSize>(-1)};
    int totalCost{0};
    int narrow{0};
    int conversions{0};
  };

  struct Error
  {
    ErrorCode code{ErrorCode::InvalidArgument};
    std::string_view message{};
    NGIN::Containers::Vector<OverloadDiagnostic> diagnostics{};
    std::optional<NGIN::UInt32> closestMethodIndex{};

    constexpr Error() = default;
    Error(ErrorCode c, std::string_view m) : code(c), message(m) {}
    Error(ErrorCode c, std::string_view m, NGIN::Containers::Vector<OverloadDiagnostic> d)
        : code(c), message(m), diagnostics(std::move(d))
    {
    }
  };

  // Small opaque handles (indices into immutable tables). Intentionally trivial.
  struct TypeHandle
  {
    NGIN::UInt32 index{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 generation{0};
    constexpr bool IsValid() const noexcept { return index != static_cast<NGIN::UInt32>(-1); }
  };

  struct FieldHandle
  {
    NGIN::UInt32 typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 fieldIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 typeGeneration{0};
    constexpr bool IsValid() const noexcept { return typeIndex != static_cast<NGIN::UInt32>(-1) && fieldIndex != static_cast<NGIN::UInt32>(-1); }
  };

  struct PropertyHandle
  {
    NGIN::UInt32 typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 propertyIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 typeGeneration{0};
    constexpr bool IsValid() const noexcept { return typeIndex != static_cast<NGIN::UInt32>(-1) && propertyIndex != static_cast<NGIN::UInt32>(-1); }
  };

  struct ConstructorHandle
  {
    NGIN::UInt32 typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 ctorIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 typeGeneration{0};
    constexpr bool IsValid() const noexcept { return typeIndex != static_cast<NGIN::UInt32>(-1) && ctorIndex != static_cast<NGIN::UInt32>(-1); }
  };

  struct EnumValueHandle
  {
    NGIN::UInt32 typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 valueIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 typeGeneration{0};
    constexpr bool IsValid() const noexcept { return typeIndex != static_cast<NGIN::UInt32>(-1) && valueIndex != static_cast<NGIN::UInt32>(-1); }
  };

  struct BaseHandle
  {
    NGIN::UInt32 typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 baseIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 typeGeneration{0};
    constexpr bool IsValid() const noexcept { return typeIndex != static_cast<NGIN::UInt32>(-1) && baseIndex != static_cast<NGIN::UInt32>(-1); }
  };

  struct FunctionHandle
  {
    NGIN::UInt32 index{static_cast<NGIN::UInt32>(-1)};
    constexpr bool IsValid() const noexcept { return index != static_cast<NGIN::UInt32>(-1); }
  };

  enum class MemberKind : unsigned char
  {
    Field = 0,
    Property = 1,
    Method = 2,
    Constructor = 3,
  };

  struct MemberHandle
  {
    MemberKind kind{MemberKind::Field};
    NGIN::UInt32 typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 memberIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 typeGeneration{0};
    constexpr bool IsValid() const noexcept { return typeIndex != static_cast<NGIN::UInt32>(-1) && memberIndex != static_cast<NGIN::UInt32>(-1); }
  };

  // Forward decls of high-level wrappers
  class Type;
  class Field;
  class Property;
  class Method;
  class Constructor;
  class Member;
  class EnumValue;
  class Base;
  class Function;
  class ResolvedFunction;

  using ExpectedType = std::expected<Type, Error>;
  using ExpectedField = std::expected<Field, Error>;
  using ExpectedProperty = std::expected<Property, Error>;
  using ExpectedConstructor = std::expected<Constructor, Error>;
  using ExpectedEnumValue = std::expected<EnumValue, Error>;
  using ExpectedBase = std::expected<Base, Error>;
  using ExpectedFunction = std::expected<Function, Error>;
  using ExpectedResolvedFunction = std::expected<ResolvedFunction, Error>;

} // namespace NGIN::Reflection
