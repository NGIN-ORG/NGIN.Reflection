#pragma once

#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/Export.hpp>
#include <NGIN/Reflection/ModuleInit.hpp>
#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/TypeBuilder.hpp>
#include <NGIN/Reflection/Types.hpp>

#include <string_view>

namespace NGIN::Reflection
{
  [[nodiscard]] constexpr std::string_view LibraryName() noexcept
  {
    return "NGIN.Reflection";
  }
} // namespace NGIN::Reflection
