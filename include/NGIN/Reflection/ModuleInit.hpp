#pragma once

#include <NGIN/Reflection/TypeBuilder.hpp>

#include <mutex>
#include <unordered_map>
#include <utility>

namespace NGIN::Reflection
{
  namespace detail
  {
    template <class T>
    void RegisterTypeIntoModule(ModuleBuildState &state)
    {
      using U = std::remove_cvref_t<T>;
      const auto qualifiedName = NGIN::Meta::TypeName<U>::qualifiedName;
      const auto nameHash = NGIN::Meta::GetTypeId<U>();
      if (state.typeNameToIndex.contains(nameHash))
        return;

      TypeRecord record{};
      record.qualifiedName = InternSymbol(qualifiedName);
      record.sizeBytes = sizeof(U);
      record.alignBytes = alignof(U);
      state.typeNameToIndex.emplace(nameHash, state.types.size());
      state.types.push_back(std::move(record));

      TypeBuildAnchor anchor{&state, state.types.size() - 1u};
      TypeBuilder<U> builder{anchor};

      if constexpr (HasNginReflectWithTypeBuilder<U>)
        NginReflect(Tag<U>{}, builder);
      else if constexpr (HasDescribeWithTypeBuilder<U>)
        Describe<U>::Do(builder);

      if constexpr (std::is_default_constructible_v<U>)
        builder.template Constructor<>();
    }

    template <auto FunctionPtr>
    void RegisterFunctionIntoModule(ModuleBuildState &state, std::string_view name)
    {
      using Traits = FunctionTraits<decltype(FunctionPtr)>;
      using Return = typename Traits::Return;
      using ArgsTuple = typename Traits::Args;

      FunctionRecord record{};
      record.name = InternSymbol(name);
      if constexpr (!std::is_void_v<Return>)
        record.returnType = MakeTypeReference<Return>();
      FillTypeRefs<ArgsTuple>(record.parameters, std::make_index_sequence<std::tuple_size_v<ArgsTuple>>{});
      record.invokeSlot = static_cast<std::uint32_t>(state.tables.functionInvokers.size());
      state.tables.functionInvokers.push_back(&FunctionInvokeThunk<FunctionPtr>);
      state.functions.push_back(std::move(record));
    }
  } // namespace detail

  class ModuleRegistration
  {
  public:
    explicit ModuleRegistration(std::string_view moduleName,
                                std::uint32_t abiFamily = 1,
                                std::uint32_t abiVersion = 1)
    {
      const auto symbol = detail::InternSymbol(moduleName);
      m_state = std::make_shared<detail::ModuleBuildState>();
      m_state->identity = ModuleIdentity::Create(symbol, abiFamily, abiVersion);
    }

    [[nodiscard]] ModuleIdentity Identity() const noexcept
    {
      return m_state ? m_state->identity : ModuleIdentity{};
    }

    template <class T>
    void RegisterType() const
    {
      detail::RegisterTypeIntoModule<T>(*m_state);
    }

    template <class... T>
    void RegisterTypes() const
    {
      (detail::RegisterTypeIntoModule<T>(*m_state), ...);
    }

    template <auto FunctionPtr>
    void RegisterFunction(std::string_view name) const
    {
      detail::RegisterFunctionIntoModule<FunctionPtr>(*m_state, name);
    }

    [[nodiscard]] bool Commit(Error *error = nullptr) const noexcept
    {
      if (!m_state || m_state->committed)
        return true;
      auto module = detail::BuildModuleRecord(*m_state);
      const bool ok = detail::InstallModule(std::move(module), error);
      if (ok)
        m_state->committed = true;
      return ok;
    }

  private:
    std::shared_ptr<detail::ModuleBuildState> m_state{};
  };

  template <class Fn>
  bool EnsureModuleInitialized(std::string_view moduleName, Fn &&fn)
  {
    static std::mutex mutex;
    static std::unordered_map<std::uint64_t, bool> initialized;

    ModuleRegistration registration{moduleName};
    const auto key = registration.Identity().keyHash;

    {
      std::lock_guard lock{mutex};
      if (initialized[key])
        return true;
    }

    using Result = std::invoke_result_t<Fn, ModuleRegistration &>;
    bool shouldCommit = true;

    if constexpr (std::is_void_v<Result>)
    {
      std::forward<Fn>(fn)(registration);
    }
    else
    {
      auto result = std::forward<Fn>(fn)(registration);
      if constexpr (std::is_convertible_v<Result, bool>)
        shouldCommit = static_cast<bool>(result);
    }

    if (!shouldCommit)
      return false;

    Error error{};
    if (!registration.Commit(&error))
      return false;

    {
      std::lock_guard lock{mutex};
      initialized[key] = true;
    }
    return true;
  }
} // namespace NGIN::Reflection
