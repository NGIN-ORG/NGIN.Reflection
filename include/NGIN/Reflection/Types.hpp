// Types.hpp
// Public-facing error codes and small ABI-stable handle types
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Utilities/Any.hpp>
#include <string_view>
#include <expected>

namespace NGIN::Reflection
{

  using Any = NGIN::Utilities::Any<>;

  enum class ErrorCode : unsigned
  {
    NotFound = 1,
    InvalidArgument = 2,
  };

  struct Error
  {
    ErrorCode code{ErrorCode::InvalidArgument};
    std::string_view message{};
  };

  // Small opaque handles (indices into immutable tables). Intentionally trivial.
  struct TypeHandle
  {
    NGIN::UInt32 index{static_cast<NGIN::UInt32>(-1)};
    constexpr bool IsValid() const noexcept { return index != static_cast<NGIN::UInt32>(-1); }
  };

  struct FieldHandle
  {
    NGIN::UInt32 typeIndex{static_cast<NGIN::UInt32>(-1)};
    NGIN::UInt32 fieldIndex{static_cast<NGIN::UInt32>(-1)};
    constexpr bool IsValid() const noexcept { return typeIndex != static_cast<NGIN::UInt32>(-1) && fieldIndex != static_cast<NGIN::UInt32>(-1); }
  };

  // Forward decls of high-level wrappers
  class Type;
  class Field;

  using ExpectedType = std::expected<Type, Error>;
  using ExpectedField = std::expected<Field, Error>;

} // namespace NGIN::Reflection
