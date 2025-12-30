#pragma once

#include <string_view>
#include <type_traits>
#include <utility>

#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Hashing/FNV.hpp>

namespace NGIN::Reflection
{

  /**
   * Helper used by plugin or module authors to register reflection metadata in a
   * predictable, explicit fashion. Constructed with a module identifier so
   * diagnostics can attribute registrations to a specific binary.
   */
  class ModuleRegistration
  {
  public:
    constexpr explicit ModuleRegistration(std::string_view moduleName) noexcept
        : m_moduleName(moduleName),
          m_moduleId(NGIN::Hashing::FNV1a64(moduleName.data(), moduleName.size()))
    {
    }

    [[nodiscard]] constexpr std::string_view ModuleName() const noexcept
    {
      return m_moduleName;
    }

    [[nodiscard]] constexpr ModuleId GetModuleId() const noexcept
    {
      return m_moduleId;
    }

    /** Register a single reflected type. */
    template <class T>
    void RegisterType() const
    {
      (void)detail::EnsureRegistered<T>(m_moduleId);
    }

    /** Register multiple reflected types in one call. */
    template <class... T>
    void RegisterTypes() const
    {
      (detail::EnsureRegistered<T>(m_moduleId), ...);
    }

    /** Invoke a callable with direct access to the backing registry. */
    template <class Fn>
    decltype(auto) WithRegistry(Fn &&fn) const
    {
      return std::forward<Fn>(fn)(detail::GetRegistry());
    }

  private:
    std::string_view m_moduleName;
    ModuleId m_moduleId{0};
  };

  /**
   * Runs `fn` exactly once per module and only marks the module as initialized
   * when the callable succeeds. The callable receives a `ModuleRegistration`
   * helper. If it returns a `bool`, that value controls whether initialization
   * is considered successful.
   */
  template <class Fn>
  bool EnsureModuleInitialized(std::string_view moduleName, Fn &&fn)
  {
    static bool initialized = false;
    if (initialized)
      return true;

    ModuleRegistration registration{moduleName};

    using Result = std::invoke_result_t<Fn, ModuleRegistration &>;

    if constexpr (std::is_void_v<Result>)
    {
      std::forward<Fn>(fn)(registration);
      initialized = true;
      return true;
    }
    else
    {
      Result result = std::forward<Fn>(fn)(registration);
      if constexpr (std::is_convertible_v<Result, bool>)
      {
        if (!static_cast<bool>(result))
          return false;
        initialized = true;
        return true;
      }
      else
      {
        initialized = true;
        return true;
      }
    }
  }

} // namespace NGIN::Reflection

