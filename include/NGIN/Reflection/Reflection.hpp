#pragma once

#include <string_view>

#include <NGIN/Reflection/Export.hpp>
#include <NGIN/Reflection/Types.hpp>
#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/NameUtils.hpp>
#include <NGIN/Reflection/TypeBuilder.hpp>
#include <NGIN/Reflection/ModuleInit.hpp>
#include <NGIN/Meta/TypeName.hpp>

namespace NGIN::Reflection
{

    // For quick sanity checks / examples.
    [[nodiscard]] NGIN_REFLECTION_API constexpr std::string_view LibraryName() noexcept { return "NGIN.Reflection"; }

} // namespace NGIN::Reflection
