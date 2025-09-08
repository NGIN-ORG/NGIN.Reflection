// Convert.hpp — Shared Any -> T conversion helpers and type-id utilities
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Reflection/Registry.hpp>

#include <type_traits>
#include <expected>

#include <NGIN/Reflection/Any.hpp>
#include <NGIN/Reflection/Types.hpp>

namespace NGIN::Reflection::detail
{

  template <class T>
  inline constexpr bool is_numeric_v = std::is_arithmetic_v<std::remove_cv_t<std::remove_reference_t<T>>>;

  // Try to convert Any -> To (supports exact match, arithmetic conversions)
  template <class To>
  inline std::expected<std::remove_cv_t<std::remove_reference_t<To>>, Error>
  ConvertAny(const class Any &src)
  {
    using Dest = std::remove_cv_t<std::remove_reference_t<To>>;
    const auto tid = src.type_id();
    if (tid == TypeIdOf<Dest>())
    {
      return src.template As<Dest>();
    }
    if constexpr (is_numeric_v<Dest>)
    {
      if (tid == TypeIdOf<bool>())
        return static_cast<Dest>(src.template As<bool>());
      if (tid == TypeIdOf<signed char>())
        return static_cast<Dest>(src.template As<signed char>());
      if (tid == TypeIdOf<unsigned char>())
        return static_cast<Dest>(src.template As<unsigned char>());
      if (tid == TypeIdOf<char>())
        return static_cast<Dest>(src.template As<char>());
      if (tid == TypeIdOf<short>())
        return static_cast<Dest>(src.template As<short>());
      if (tid == TypeIdOf<unsigned short>())
        return static_cast<Dest>(src.template As<unsigned short>());
      if (tid == TypeIdOf<int>())
        return static_cast<Dest>(src.template As<int>());
      if (tid == TypeIdOf<unsigned int>())
        return static_cast<Dest>(src.template As<unsigned int>());
      if (tid == TypeIdOf<long>())
        return static_cast<Dest>(src.template As<long>());
      if (tid == TypeIdOf<unsigned long>())
        return static_cast<Dest>(src.template As<unsigned long>());
      if (tid == TypeIdOf<long long>())
        return static_cast<Dest>(src.template As<long long>());
      if (tid == TypeIdOf<unsigned long long>())
        return static_cast<Dest>(src.template As<unsigned long long>());
      if (tid == TypeIdOf<float>())
        return static_cast<Dest>(src.template As<float>());
      if (tid == TypeIdOf<double>())
        return static_cast<Dest>(src.template As<double>());
      if (tid == TypeIdOf<long double>())
        return static_cast<Dest>(src.template As<long double>());
    }
    return std::unexpected(Error{ErrorCode::InvalidArgument, "argument type not convertible"});
  }

} // namespace NGIN::Reflection::detail
