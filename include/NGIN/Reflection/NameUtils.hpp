// NameUtils.hpp
// Utilities to derive member names from pointer-to-member constants using compiler signatures.
#pragma once

#include <string_view>

namespace NGIN::Reflection::detail {

template<auto MemberPtr>
consteval std::string_view MemberNameFromPretty() noexcept {
#if defined(_MSC_VER)
  constexpr std::string_view sig = __FUNCSIG__;
  // Example: "std::string_view __cdecl NGIN::Reflection::detail::MemberNameFromPretty< &Class::member >(void) noexcept"
  constexpr std::string_view key = "< &"; // start before Class::member
  auto kpos = sig.find(key);
  if (kpos == std::string_view::npos) return {};
  auto start = kpos + key.size();
  auto end = sig.find(" >", start);
  if (end == std::string_view::npos || end <= start) return {};
  auto full = sig.substr(start, end - start); // Class::member
#elif defined(__clang__)
  constexpr std::string_view sig = __PRETTY_FUNCTION__;
  // Example: "std::string_view NGIN::Reflection::detail::MemberNameFromPretty() [MemberPtr = &Class::member]"
  constexpr std::string_view key = "[MemberPtr = &";
  auto kpos = sig.find(key);
  if (kpos == std::string_view::npos) return {};
  auto start = kpos + key.size();
  auto end = sig.find(']', start);
  if (end == std::string_view::npos || end <= start) return {};
  auto full = sig.substr(start, end - start); // Class::member
#elif defined(__GNUC__)
  constexpr std::string_view sig = __PRETTY_FUNCTION__;
  // Example: "std::string_view NGIN::Reflection::detail::MemberNameFromPretty() [with auto MemberPtr = &Class::member]"
  constexpr std::string_view key = "[with auto MemberPtr = &";
  auto kpos = sig.find(key);
  if (kpos == std::string_view::npos) return {};
  auto start = kpos + key.size();
  auto end = sig.find(']', start);
  if (end == std::string_view::npos || end <= start) return {};
  auto full = sig.substr(start, end - start); // Class::member
#else
  return {};
#endif
  // Strip the Class:: prefix to keep only the member identifier.
  auto dc = full.rfind("::");
  if (dc == std::string_view::npos) return full;
  return full.substr(dc + 2);
}

} // namespace NGIN::Reflection::detail

