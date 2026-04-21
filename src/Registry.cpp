#include <NGIN/Reflection/Reflection.hpp>
#include <NGIN/Utilities/SymbolTable.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_set>

namespace NGIN::Reflection
{
  namespace
  {
    constexpr std::uint32_t kAbiFamily = 1;
    constexpr std::uint32_t kAbiVersion = 1;

    struct ExportedModuleApiState
    {
      std::deque<std::string> ownedStrings;
      std::vector<NGINReflectionStringView> symbols;
      std::vector<NGINReflectionAttributeDesc> attributes;
      std::vector<NGINReflectionTypeRef> parameters;
      std::vector<NGINReflectionFieldDesc> fields;
      std::vector<NGINReflectionPropertyDesc> properties;
      std::vector<NGINReflectionMethodDesc> methods;
      std::vector<NGINReflectionConstructorDesc> constructors;
      std::vector<NGINReflectionEnumValueDesc> enumValues;
      std::vector<NGINReflectionBaseDesc> bases;
      std::vector<NGINReflectionTypeDesc> types;
      std::vector<NGINReflectionFunctionDesc> functions;
      std::vector<NGINReflectionFieldReadFn> fieldReaders;
      std::vector<NGINReflectionFieldWriteFn> fieldWriters;
      std::vector<NGINReflectionPropertyReadFn> propertyReaders;
      std::vector<NGINReflectionPropertyWriteFn> propertyWriters;
      std::vector<NGINReflectionMethodInvokeFn> methodInvokers;
      std::vector<NGINReflectionConstructorInvokeFn> constructorInvokers;
      std::vector<NGINReflectionFunctionInvokeFn> functionInvokers;
      std::vector<NGINReflectionBaseCastFn> upcasters;
      std::vector<NGINReflectionBaseCastFn> downcasters;
      std::unordered_map<std::uint32_t, std::uint32_t> localSymbols;
    };

    struct GlobalRegistry
    {
      NGIN::Utilities::SymbolTable<NGIN::Memory::SystemAllocator, std::mutex> symbols;
      std::mutex mutationMutex;
      std::atomic<std::shared_ptr<const detail::RegistryState>> state{std::make_shared<detail::RegistryState>()};
    };

    GlobalRegistry &Registry()
    {
      static GlobalRegistry registry{};
      return registry;
    }

    [[nodiscard]] inline std::uint64_t HashNameId(SymbolId id) noexcept
    {
      return static_cast<std::uint64_t>(id.value);
    }

    template <class T>
    [[nodiscard]] inline std::uint64_t HashVector(const std::vector<T> &items, std::uint64_t seed) noexcept
    {
      std::uint64_t hash = seed;
      for (const auto &item : items)
      {
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(&item);
        for (std::size_t i = 0; i < sizeof(T); ++i)
          hash = (hash ^ bytes[i]) * 1099511628211ull;
      }
      return hash;
    }

    [[nodiscard]] inline std::uint64_t CombineHash(std::uint64_t seed, std::uint64_t value) noexcept
    {
      const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
      std::uint64_t hash = seed;
      for (std::size_t i = 0; i < sizeof(value); ++i)
        hash = (hash ^ bytes[i]) * 1099511628211ull;
      return hash;
    }

    [[nodiscard]] Error MakeRangeError(std::string what)
    {
      return Error{ErrorCode::CorruptModule, std::move(what)};
    }

    [[nodiscard]] NGINReflectionStatusCode ToStatusCode(ErrorCode code) noexcept
    {
      return static_cast<NGINReflectionStatusCode>(code);
    }

    [[nodiscard]] NGINReflectionStatus OkStatus() noexcept
    {
      return NGINReflectionStatus{NGINReflectionStatus_Ok, {nullptr, 0}};
    }

    [[nodiscard]] NGINReflectionStatus MakeStatus(const Error &error) noexcept
    {
      return NGINReflectionStatus{static_cast<std::uint32_t>(ToStatusCode(error.code)),
                                  {error.message.data(), static_cast<std::uint64_t>(error.message.size())}};
    }

    [[nodiscard]] std::string ToOwnedString(std::string_view view)
    {
      return std::string(view.data(), view.size());
    }

    [[nodiscard]] std::expected<AttributeValue, Error> ImportAttributeValue(const NGINReflectionValue &value,
                                                                            const std::vector<SymbolId> &symbols);

    void ReleaseAbiValue(NGINReflectionValue &value)
    {
      if (value.release)
        value.release(&value);
    }

    [[nodiscard]] Value FromAbiValue(const NGINReflectionValue &value);

    [[nodiscard]] std::expected<NGINReflectionValue, Error> ToAbiValue(const Value &value)
    {
      if (value.IsEmpty())
        return NGINReflectionValue{};

      if (auto instance = value.TryAsInstance(); instance.has_value())
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Instance;
        out.instanceValue = *instance->AbiHandle();
        if (out.instanceValue.vtable && out.instanceValue.vtable->retain)
          out.instanceValue.vtable->retain(&out.instanceValue);
        out.release = [](NGINReflectionValue *value) {
          if (value && value->kind == NGINReflectionValue_Instance &&
              value->instanceValue.vtable && value->instanceValue.vtable->release)
          {
            value->instanceValue.vtable->release(&value->instanceValue);
          }
          value->kind = NGINReflectionValue_Empty;
          value->release = nullptr;
          value->releaseUserData = nullptr;
        };
        return out;
      }

      const Any *boxed = value.TryAny();
      if (!boxed)
        return std::unexpected(Error{ErrorCode::InvalidArgument, "value does not contain data"});

      if (const auto *b = boxed->TryCast<bool>())
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Bool;
        out.boolValue = *b ? 1u : 0u;
        return out;
      }
      if (const auto *i = boxed->TryCast<std::int64_t>())
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Int64;
        out.intValue = *i;
        return out;
      }
      if (const auto *u = boxed->TryCast<std::uint64_t>())
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_UInt64;
        out.uintValue = *u;
        return out;
      }
      if (const auto *d = boxed->TryCast<double>())
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Float64;
        out.floatValue = *d;
        return out;
      }
      if (const auto *s = boxed->TryCast<std::string>())
      {
        auto *owned = new std::string(*s);
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_String;
        out.stringValue = {owned->data(), static_cast<std::uint64_t>(owned->size())};
        out.release = [](NGINReflectionValue *value) {
          delete static_cast<std::string *>(value->releaseUserData);
          value->release = nullptr;
          value->releaseUserData = nullptr;
        };
        out.releaseUserData = owned;
        return out;
      }
      if (const auto *symbol = boxed->TryCast<SymbolId>())
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Symbol;
        out.symbolValue = symbol->value;
        return out;
      }
      if (const auto *identity = boxed->TryCast<TypeIdentity>())
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_TypeIdentity;
        out.typeIdentity = NGINReflectionTypeIdentity{
            NGINReflectionModuleIdentity{
                identity->module.moduleName.value,
                identity->module.abiFamily,
                identity->module.abiVersion,
                0u,
                identity->module.keyHash},
            identity->qualifiedName.value,
            0u,
            identity->signatureHash,
            identity->keyHash};
        return out;
      }

      return std::unexpected(Error{ErrorCode::InvalidArgument, "unsupported value conversion"});
    }

    [[nodiscard]] std::expected<std::shared_ptr<const detail::ModuleRecord>, Error> SelectExportModule()
    {
      auto state = Registry().state.load();
      if (!state || state->modules.empty())
        return std::unexpected(Error{ErrorCode::NotFound, "no registered reflection module"});

      for (const auto &module : state->modules)
      {
        if (module && !module->imported)
          return module;
      }
      return state->modules.front();
    }
  } // namespace

  namespace detail
  {
    struct InstanceStorage
    {
      explicit InstanceStorage(NGINReflectionInstanceHandle sourceHandle)
          : handle(sourceHandle)
      {
      }

      ~InstanceStorage()
      {
        if (handle.vtable && handle.vtable->release)
          handle.vtable->release(&handle);
      }

      NGINReflectionInstanceHandle handle{};
    };

    SymbolId InternSymbol(std::string_view name)
    {
      return Registry().symbols.Intern(name);
    }

    bool TryGetSymbol(std::string_view name, SymbolId &out) noexcept
    {
      return Registry().symbols.TryGet(name, out);
    }

    std::string_view ViewSymbol(SymbolId id) noexcept
    {
      return Registry().symbols.View(id);
    }

    TypeReference MakeTypeReference(std::string_view qualifiedName)
    {
      SymbolId symbol = InternSymbol(qualifiedName);
      return TypeReference{symbol, NGIN::Hashing::FNV1a64(qualifiedName), 0};
    }

    Error MakeError(ErrorCode code, std::string message)
    {
      return Error{code, std::move(message)};
    }

    std::shared_ptr<const RegistryState> CurrentState() noexcept
    {
      return Registry().state.load();
    }

    RegistrySnapshot Snapshot() noexcept
    {
      auto state = CurrentState();
      if (!state)
        return {};
      return RegistrySnapshot{state->generation, state->modules.size(), state->types.size(), state->functions.size()};
    }

    namespace
    {
      std::uint64_t ComputeSignatureHash(const TypeRecord &record)
      {
        std::uint64_t hash = 14695981039346656037ull;
        hash = CombineHash(hash, record.qualifiedName.value);
        hash = CombineHash(hash, record.sizeBytes);
        hash = CombineHash(hash, record.alignBytes);

        for (const auto &field : record.fields)
        {
          hash = CombineHash(hash, field.name.value);
          hash = CombineHash(hash, field.valueType.qualifiedName.value);
        }
        for (const auto &property : record.properties)
        {
          hash = CombineHash(hash, property.name.value);
          hash = CombineHash(hash, property.valueType.qualifiedName.value);
        }
        for (const auto &method : record.methods)
        {
          hash = CombineHash(hash, method.name.value);
          hash = CombineHash(hash, method.returnType.qualifiedName.value);
          for (const auto &parameter : method.parameters)
            hash = CombineHash(hash, parameter.qualifiedName.value);
        }
        for (const auto &ctor : record.constructors)
          for (const auto &parameter : ctor.parameters)
            hash = CombineHash(hash, parameter.qualifiedName.value);
        for (const auto &base : record.bases)
          hash = CombineHash(hash, base.baseName.value);
        for (const auto &entry : record.enumValues)
        {
          hash = CombineHash(hash, entry.name.value);
          hash = CombineHash(hash, static_cast<std::uint64_t>(entry.signedValue));
          hash = CombineHash(hash, entry.unsignedValue);
        }
        return hash == 0 ? 1u : hash;
      }

      void ResolveReflectedTypeKeys(std::vector<TypeRecord> &types)
      {
        std::unordered_map<std::uint32_t, TypeIdentity> byName;
        for (const auto &type : types)
          byName.emplace(type.qualifiedName.value, type.identity);

        auto resolve = [&](TypeReference &reference) {
          if (reference.qualifiedName.IsValid())
          {
            if (auto it = byName.find(reference.qualifiedName.value); it != byName.end())
              reference.reflectedTypeKey = it->second.keyHash;
          }
        };

        for (auto &type : types)
        {
          resolve(type.enumUnderlyingType);
          for (auto &field : type.fields)
            resolve(field.valueType);
          for (auto &property : type.properties)
            resolve(property.valueType);
          for (auto &method : type.methods)
          {
            resolve(method.returnType);
            for (auto &parameter : method.parameters)
              resolve(parameter);
          }
          for (auto &ctor : type.constructors)
            for (auto &parameter : ctor.parameters)
              resolve(parameter);
        }
      }

      std::shared_ptr<ModuleRecord> CloneModuleWithResolvedIdentity(const ModuleBuildState &state)
      {
        auto module = std::make_shared<ModuleRecord>();
        module->identity = state.identity;
        module->tables = state.tables;
        module->types = state.types;
        module->functions = state.functions;

        for (auto &type : module->types)
        {
          const auto signature = ComputeSignatureHash(type);
          type.identity = TypeIdentity::Create(module->identity, type.qualifiedName, signature);
        }

        ResolveReflectedTypeKeys(module->types);

        std::unordered_map<std::uint32_t, TypeIdentity> byName;
        for (const auto &type : module->types)
          byName.emplace(type.qualifiedName.value, type.identity);

        for (auto &type : module->types)
        {
          for (auto &base : type.bases)
          {
            if (auto it = byName.find(base.baseName.value); it != byName.end())
              base.baseIdentity = it->second;
            else
              base.baseIdentity = TypeIdentity::Create(module->identity, base.baseName, base.baseName.value);
          }
        }

        for (auto &function : module->functions)
        {
          if (function.returnType.qualifiedName.IsValid())
          {
            if (auto it = byName.find(function.returnType.qualifiedName.value); it != byName.end())
              function.returnType.reflectedTypeKey = it->second.keyHash;
          }
          for (auto &parameter : function.parameters)
          {
            if (auto it = byName.find(parameter.qualifiedName.value); it != byName.end())
              parameter.reflectedTypeKey = it->second.keyHash;
          }
        }

        return module;
      }

      void RebuildIndices(RegistryState &state)
      {
        state.types.clear();
        state.functions.clear();
        state.typesByKey.clear();
        state.typesByNameHash.clear();
        state.functionsByNameHash.clear();
        state.modulesByKey.clear();

        for (std::size_t moduleIndex = 0; moduleIndex < state.modules.size(); ++moduleIndex)
        {
          const auto &module = state.modules[moduleIndex];
          if (!module)
            continue;
          state.modulesByKey[module->identity.keyHash] = moduleIndex;

          for (std::size_t typeIndex = 0; typeIndex < module->types.size(); ++typeIndex)
          {
            const auto &type = module->types[typeIndex];
            const auto bindingIndex = state.types.size();
            state.types.push_back(TypeBinding{moduleIndex, typeIndex});
            state.typesByKey[type.identity.keyHash] = bindingIndex;
            state.typesByNameHash[HashNameId(type.qualifiedName)].push_back(bindingIndex);
          }

          for (std::size_t functionIndex = 0; functionIndex < module->functions.size(); ++functionIndex)
          {
            const auto &function = module->functions[functionIndex];
            const auto bindingIndex = state.functions.size();
            state.functions.push_back(FunctionBinding{moduleIndex, functionIndex});
            state.functionsByNameHash[HashNameId(function.name)].push_back(bindingIndex);
          }
        }
      }

      std::expected<NGINReflectionTypeIdentity, Error> TranslateTypeIdentity(const NGINReflectionTypeIdentity &identity,
                                                                             const std::vector<SymbolId> &symbols)
      {
        if (identity.module.moduleNameSymbol >= symbols.size() || identity.qualifiedNameSymbol >= symbols.size())
          return std::unexpected(MakeRangeError("type identity uses invalid symbol index"));

        const auto moduleIdentity = ModuleIdentity::Create(symbols[identity.module.moduleNameSymbol],
                                                           identity.module.abiFamily,
                                                           identity.module.abiVersion);
        const auto typeIdentity = TypeIdentity::Create(moduleIdentity,
                                                       symbols[identity.qualifiedNameSymbol],
                                                       identity.signatureHash);
        return NGINReflectionTypeIdentity{
            NGINReflectionModuleIdentity{
                typeIdentity.module.moduleName.value,
                typeIdentity.module.abiFamily,
                typeIdentity.module.abiVersion,
                0u,
                typeIdentity.module.keyHash},
            typeIdentity.qualifiedName.value,
            0u,
            typeIdentity.signatureHash,
            typeIdentity.keyHash};
      }
    } // namespace

    std::shared_ptr<ModuleRecord> BuildModuleRecord(const ModuleBuildState &state)
    {
      return CloneModuleWithResolvedIdentity(state);
    }

    bool InstallModule(std::shared_ptr<ModuleRecord> module, Error *error) noexcept
    {
      if (!module || !module->identity.IsValid())
      {
        if (error)
          *error = Error{ErrorCode::InvalidArgument, "invalid module contribution"};
        return false;
      }

      auto &registry = Registry();
      std::lock_guard lock{registry.mutationMutex};

      auto current = std::const_pointer_cast<RegistryState>(registry.state.load());
      auto next = std::make_shared<RegistryState>(*current);
      next->generation += 1u;

      for (std::size_t i = 0; i < next->modules.size(); ++i)
      {
        const auto &existing = next->modules[i];
        if (!existing)
          continue;

        if (existing->identity.keyHash == module->identity.keyHash)
        {
          next->modules[i] = std::move(module);
          RebuildIndices(*next);
          registry.state.store(std::static_pointer_cast<const RegistryState>(next));
          return true;
        }

        for (const auto &candidateType : module->types)
        {
          for (const auto &existingType : existing->types)
          {
            if (candidateType.qualifiedName == existingType.qualifiedName)
            {
              if (error)
                *error = Error{ErrorCode::Conflict, "qualified type name already owned by another module"};
              return false;
            }
            if (candidateType.identity.keyHash == existingType.identity.keyHash)
            {
              if (error)
                *error = Error{ErrorCode::Conflict, "type identity collision between modules"};
              return false;
            }
          }
        }
      }

      next->modules.push_back(std::move(module));
      RebuildIndices(*next);
      registry.state.store(std::static_pointer_cast<const RegistryState>(next));
      return true;
    }

    bool UnloadModule(const ModuleIdentity &identity, Error *error) noexcept
    {
      auto &registry = Registry();
      std::lock_guard lock{registry.mutationMutex};

      auto current = std::const_pointer_cast<RegistryState>(registry.state.load());
      auto next = std::make_shared<RegistryState>(*current);
      next->generation += 1u;

      auto oldSize = next->modules.size();
      next->modules.erase(
          std::remove_if(next->modules.begin(), next->modules.end(), [&](const auto &module) {
            return module && module->identity.keyHash == identity.keyHash;
          }),
          next->modules.end());

      if (next->modules.size() == oldSize)
      {
        if (error)
          *error = Error{ErrorCode::NotFound, "module not found"};
        return false;
      }

      RebuildIndices(*next);
      registry.state.store(std::static_pointer_cast<const RegistryState>(next));
      return true;
    }

    std::expected<Handle, Error> FindTypeHandle(std::string_view qualifiedName) noexcept
    {
      SymbolId symbol{};
      if (!TryGetSymbol(qualifiedName, symbol))
        return std::unexpected(Error{ErrorCode::NotFound, "type not found"});
      auto state = CurrentState();
      auto it = state->typesByNameHash.find(HashNameId(symbol));
      if (it == state->typesByNameHash.end() || it->second.empty())
        return std::unexpected(Error{ErrorCode::NotFound, "type not found"});
      return Handle{it->second.front(), static_cast<std::size_t>(-1), state->generation};
    }

    std::expected<Handle, Error> FindTypeHandle(const TypeIdentity &identity) noexcept
    {
      auto state = CurrentState();
      auto it = state->typesByKey.find(identity.keyHash);
      if (it == state->typesByKey.end())
        return std::unexpected(Error{ErrorCode::NotFound, "type not found"});
      return Handle{it->second, static_cast<std::size_t>(-1), state->generation};
    }

    std::expected<FunctionHandle, Error> FindFunctionHandle(std::string_view name) noexcept
    {
      SymbolId symbol{};
      if (!TryGetSymbol(name, symbol))
        return std::unexpected(Error{ErrorCode::NotFound, "function not found"});
      auto state = CurrentState();
      auto it = state->functionsByNameHash.find(HashNameId(symbol));
      if (it == state->functionsByNameHash.end() || it->second.empty())
        return std::unexpected(Error{ErrorCode::NotFound, "function not found"});
      return FunctionHandle{it->second.front(), state->generation};
    }
  } // namespace detail

  namespace
  {
    const detail::TypeRecord *GetTypeRecord(const std::shared_ptr<const detail::RegistryState> &state,
                                            const detail::Handle &handle,
                                            Error *error = nullptr)
    {
      if (!state || handle.generation != state->generation)
      {
        if (error)
          *error = Error{ErrorCode::StaleHandle, "stale reflection handle"};
        return nullptr;
      }
      if (handle.typeIndex >= state->types.size())
      {
        if (error)
          *error = Error{ErrorCode::NotFound, "type handle out of range"};
        return nullptr;
      }
      const auto binding = state->types[handle.typeIndex];
      return &state->modules[binding.moduleIndex]->types[binding.typeIndex];
    }

    const detail::ModuleRecord *GetTypeModule(const std::shared_ptr<const detail::RegistryState> &state,
                                              const detail::Handle &handle) noexcept
    {
      if (!state || handle.generation != state->generation || handle.typeIndex >= state->types.size())
        return nullptr;
      const auto binding = state->types[handle.typeIndex];
      return state->modules[binding.moduleIndex].get();
    }

    const detail::FunctionRecord *GetFunctionRecord(const std::shared_ptr<const detail::RegistryState> &state,
                                                    const detail::FunctionHandle &handle,
                                                    Error *error = nullptr)
    {
      if (!state || handle.generation != state->generation)
      {
        if (error)
          *error = Error{ErrorCode::StaleHandle, "stale reflection handle"};
        return nullptr;
      }
      if (handle.functionIndex >= state->functions.size())
      {
        if (error)
          *error = Error{ErrorCode::NotFound, "function handle out of range"};
        return nullptr;
      }
      const auto binding = state->functions[handle.functionIndex];
      return &state->modules[binding.moduleIndex]->functions[binding.functionIndex];
    }

    const detail::ModuleRecord *GetFunctionModule(const std::shared_ptr<const detail::RegistryState> &state,
                                                  const detail::FunctionHandle &handle) noexcept
    {
      if (!state || handle.generation != state->generation || handle.functionIndex >= state->functions.size())
        return nullptr;
      const auto binding = state->functions[handle.functionIndex];
      return state->modules[binding.moduleIndex].get();
    }

    std::expected<std::size_t, Error> FindNamedMethod(const detail::TypeRecord &record, std::string_view name)
    {
      for (std::size_t i = 0; i < record.methods.size(); ++i)
      {
        if (detail::ViewSymbol(record.methods[i].name) == name)
          return i;
      }
      return std::unexpected(Error{ErrorCode::NotFound, "method not found"});
    }

    std::expected<std::size_t, Error> FindNamedField(const detail::TypeRecord &record, std::string_view name)
    {
      for (std::size_t i = 0; i < record.fields.size(); ++i)
      {
        if (detail::ViewSymbol(record.fields[i].name) == name)
          return i;
      }
      return std::unexpected(Error{ErrorCode::NotFound, "field not found"});
    }

    std::expected<std::size_t, Error> FindNamedProperty(const detail::TypeRecord &record, std::string_view name)
    {
      for (std::size_t i = 0; i < record.properties.size(); ++i)
      {
        if (detail::ViewSymbol(record.properties[i].name) == name)
          return i;
      }
      return std::unexpected(Error{ErrorCode::NotFound, "property not found"});
    }

    std::expected<std::size_t, Error> FindNamedEnum(const detail::TypeRecord &record, std::string_view name)
    {
      for (std::size_t i = 0; i < record.enumValues.size(); ++i)
      {
        if (detail::ViewSymbol(record.enumValues[i].name) == name)
          return i;
      }
      return std::unexpected(Error{ErrorCode::NotFound, "enum value not found"});
    }

    AttributeView MakeAttributeView(const detail::AttributeRecord &record)
    {
      return AttributeView{detail::ViewSymbol(record.name), record.value};
    }

    Value FromAbiValue(const NGINReflectionValue &value)
    {
      switch (value.kind)
      {
      case NGINReflectionValue_Bool:
        return Value(static_cast<bool>(value.boolValue != 0u));
      case NGINReflectionValue_Int64:
        return Value(static_cast<std::int64_t>(value.intValue));
      case NGINReflectionValue_UInt64:
        return Value(static_cast<std::uint64_t>(value.uintValue));
      case NGINReflectionValue_Float64:
        return Value(static_cast<double>(value.floatValue));
      case NGINReflectionValue_String:
        return Value(std::string(value.stringValue.data, static_cast<std::size_t>(value.stringValue.size)));
      case NGINReflectionValue_Symbol:
        return Value(Any{SymbolId{static_cast<SymbolId::ValueType>(value.symbolValue)}});
      case NGINReflectionValue_TypeIdentity:
      {
        const auto module = ModuleIdentity::Create(SymbolId{static_cast<SymbolId::ValueType>(value.typeIdentity.module.moduleNameSymbol)},
                                                   value.typeIdentity.module.abiFamily,
                                                   value.typeIdentity.module.abiVersion);
        return Value(Any{TypeIdentity::Create(module,
                                              SymbolId{static_cast<SymbolId::ValueType>(value.typeIdentity.qualifiedNameSymbol)},
                                              value.typeIdentity.signatureHash)});
      }
      case NGINReflectionValue_Instance:
      {
        auto storage = std::make_shared<detail::InstanceStorage>(value.instanceValue);
        return Value::FromInstance(InstanceRef{storage});
      }
      default:
        return {};
      }
    }
  } // namespace

  bool ConstInstanceRef::IsValid() const noexcept
  {
    return m_storage && m_storage->handle.object != nullptr;
  }

  TypeIdentity ConstInstanceRef::Identity() const noexcept
  {
    if (!IsValid())
      return {};
    const auto &identity = m_storage->handle.identity;
    const auto module = ModuleIdentity::Create(SymbolId{static_cast<SymbolId::ValueType>(identity.module.moduleNameSymbol)},
                                               identity.module.abiFamily,
                                               identity.module.abiVersion);
    return TypeIdentity::Create(module,
                                SymbolId{static_cast<SymbolId::ValueType>(identity.qualifiedNameSymbol)},
                                identity.signatureHash);
  }

  const void *ConstInstanceRef::Data() const noexcept
  {
    if (!IsValid())
      return nullptr;
    return m_storage->handle.vtable ? m_storage->handle.vtable->getConst(&m_storage->handle) : m_storage->handle.object;
  }

  const NGINReflectionInstanceHandle *ConstInstanceRef::AbiHandle() const noexcept
  {
    return IsValid() ? &m_storage->handle : nullptr;
  }

  void *InstanceRef::Data() noexcept
  {
    if (!IsValid())
      return nullptr;
    return m_storage->handle.vtable ? m_storage->handle.vtable->getMut(&m_storage->handle) : m_storage->handle.object;
  }

  NGINReflectionInstanceHandle *InstanceRef::AbiHandle() noexcept
  {
    return IsValid() ? &m_storage->handle : nullptr;
  }

  const NGINReflectionInstanceHandle *InstanceRef::AbiHandle() const noexcept
  {
    return IsValid() ? &m_storage->handle : nullptr;
  }

  Value::Value(bool value)
      : m_storage(Any{value})
  {
  }

  Value::Value(std::int64_t value)
      : m_storage(Any{value})
  {
  }

  Value::Value(std::uint64_t value)
      : m_storage(Any{value})
  {
  }

  Value::Value(double value)
      : m_storage(Any{value})
  {
  }

  Value::Value(std::string value)
      : m_storage(Any{std::move(value)})
  {
  }

  Value::Value(const char *value)
      : m_storage(Any{std::string(value ? value : "")})
  {
  }

  Value::Value(Any value)
      : m_storage(std::move(value))
  {
  }

  Value Value::FromInstance(const InstanceRef &instance)
  {
    Value value;
    value.m_storage = instance.m_storage;
    return value;
  }

  bool Value::IsEmpty() const noexcept
  {
    return std::holds_alternative<std::monostate>(m_storage);
  }

  bool Value::IsInstance() const noexcept
  {
    return std::holds_alternative<std::shared_ptr<detail::InstanceStorage>>(m_storage);
  }

  const Any *Value::TryAny() const noexcept
  {
    return std::get_if<Any>(&m_storage);
  }

  Any *Value::TryAny() noexcept
  {
    return std::get_if<Any>(&m_storage);
  }

  std::optional<ConstInstanceRef> Value::TryAsInstance() const noexcept
  {
    if (const auto *storage = std::get_if<std::shared_ptr<detail::InstanceStorage>>(&m_storage))
      return ConstInstanceRef{*storage};
    return std::nullopt;
  }

  std::optional<InstanceRef> Value::TryAsInstance() noexcept
  {
    if (const auto *storage = std::get_if<std::shared_ptr<detail::InstanceStorage>>(&m_storage))
      return InstanceRef{*storage};
    return std::nullopt;
  }

  ConstValueView Value::View() const noexcept
  {
    return ConstValueView{this};
  }

  ValueView Value::View() noexcept
  {
    return ValueView{this};
  }

  bool ConstValueView::IsValid() const noexcept
  {
    return m_value != nullptr;
  }

  bool ConstValueView::IsEmpty() const noexcept
  {
    return !m_value || m_value->IsEmpty();
  }

  bool ConstValueView::IsInstance() const noexcept
  {
    return m_value && m_value->IsInstance();
  }

  const Any *ConstValueView::TryAny() const noexcept
  {
    return m_value ? m_value->TryAny() : nullptr;
  }

  std::optional<ConstInstanceRef> ConstValueView::TryAsInstance() const noexcept
  {
    return m_value ? m_value->TryAsInstance() : std::nullopt;
  }

  std::optional<ConstAnyView> ConstValueView::TryAsAnyView() const noexcept
  {
    if (const auto *boxed = TryAny())
      return boxed->MakeView();
    return std::nullopt;
  }

  bool ValueView::IsValid() const noexcept
  {
    return m_value != nullptr;
  }

  bool ValueView::IsInstance() const noexcept
  {
    return m_value && m_value->IsInstance();
  }

  Any *ValueView::TryAny() noexcept
  {
    return m_value ? m_value->TryAny() : nullptr;
  }

  std::optional<InstanceRef> ValueView::TryAsInstance() noexcept
  {
    return m_value ? m_value->TryAsInstance() : std::nullopt;
  }

  std::optional<AnyView> ValueView::TryAsAnyView() noexcept
  {
    if (auto *boxed = TryAny())
      return boxed->MakeView();
    return std::nullopt;
  }

  RegistrySnapshot GetRegistrySnapshot() noexcept
  {
    return detail::Snapshot();
  }

  bool ImportModule(const NGINReflectionModuleApi &api, Error *error) noexcept
  {
    if (api.header.family != kAbiFamily || api.header.version != kAbiVersion)
    {
      if (error)
        *error = Error{ErrorCode::AbiMismatch, "unsupported reflection ABI"};
      return false;
    }

    std::vector<SymbolId> symbols;
    symbols.reserve(api.symbolCount);
    symbols.push_back(SymbolId::Invalid());
    for (std::uint64_t i = 1; i < api.symbolCount; ++i)
    {
      const auto &symbol = api.symbols[i];
      symbols.push_back(detail::InternSymbol(std::string_view{symbol.data, static_cast<std::size_t>(symbol.size)}));
    }

    auto module = std::make_shared<detail::ModuleRecord>();
    if (api.identity.moduleNameSymbol >= symbols.size())
    {
      if (error)
        *error = Error{ErrorCode::CorruptModule, "module identity uses invalid symbol index"};
      return false;
    }

    module->identity = ModuleIdentity::Create(symbols[api.identity.moduleNameSymbol],
                                              api.identity.abiFamily,
                                              api.identity.abiVersion);
    module->imported = true;
    module->tables.fieldReaders.assign(api.fieldReaders, api.fieldReaders + api.fieldReaderCount);
    module->tables.fieldWriters.assign(api.fieldWriters, api.fieldWriters + api.fieldWriterCount);
    module->tables.propertyReaders.assign(api.propertyReaders, api.propertyReaders + api.propertyReaderCount);
    module->tables.propertyWriters.assign(api.propertyWriters, api.propertyWriters + api.propertyWriterCount);
    module->tables.methodInvokers.assign(api.methodInvokers, api.methodInvokers + api.methodInvokerCount);
    module->tables.constructors.assign(api.constructorsInvokers, api.constructorsInvokers + api.constructorInvokerCount);
    module->tables.functionInvokers.assign(api.functionInvokers, api.functionInvokers + api.functionInvokerCount);
    module->tables.upcasters.assign(api.upcasters, api.upcasters + api.upcasterCount);
    module->tables.downcasters.assign(api.downcasters, api.downcasters + api.downcasterCount);

    auto convertTypeRef = [&](const NGINReflectionTypeRef &ref) -> std::expected<detail::TypeReference, Error> {
      if (ref.qualifiedNameSymbol >= symbols.size())
        return std::unexpected(Error{ErrorCode::CorruptModule, "type reference uses invalid symbol index"});
      return detail::TypeReference{symbols[ref.qualifiedNameSymbol], ref.fastHash, ref.reflectedTypeKey};
    };

    auto convertAttributes = [&](std::uint32_t begin, std::uint32_t count) -> std::expected<std::vector<detail::AttributeRecord>, Error> {
      if (static_cast<std::uint64_t>(begin) + static_cast<std::uint64_t>(count) > api.attributeCount)
        return std::unexpected(Error{ErrorCode::CorruptModule, "attribute range is out of bounds"});
      std::vector<detail::AttributeRecord> result;
      result.reserve(count);
      for (std::uint32_t i = 0; i < count; ++i)
      {
        const auto &attribute = api.attributes[begin + i];
        if (attribute.nameSymbol >= symbols.size())
          return std::unexpected(Error{ErrorCode::CorruptModule, "attribute uses invalid symbol index"});
        auto converted = ImportAttributeValue(attribute.value, symbols);
        if (!converted.has_value())
          return std::unexpected(converted.error());
        result.push_back(detail::AttributeRecord{symbols[attribute.nameSymbol], std::move(*converted)});
      }
      return result;
    };

    auto convertParameters = [&](std::uint32_t begin, std::uint32_t count) -> std::expected<std::vector<detail::TypeReference>, Error> {
      if (static_cast<std::uint64_t>(begin) + static_cast<std::uint64_t>(count) > api.parameterCount)
        return std::unexpected(Error{ErrorCode::CorruptModule, "parameter range is out of bounds"});
      std::vector<detail::TypeReference> result;
      result.reserve(count);
      for (std::uint32_t i = 0; i < count; ++i)
      {
        auto converted = convertTypeRef(api.parameters[begin + i]);
        if (!converted.has_value())
          return std::unexpected(converted.error());
        result.push_back(*converted);
      }
      return result;
    };

    for (std::uint64_t typeIndex = 0; typeIndex < api.typeCount; ++typeIndex)
    {
      const auto &source = api.types[typeIndex];
      if (source.identity.qualifiedNameSymbol >= symbols.size())
      {
        if (error)
          *error = Error{ErrorCode::CorruptModule, "type uses invalid symbol index"};
        return false;
      }

      detail::TypeRecord record{};
      record.qualifiedName = symbols[source.identity.qualifiedNameSymbol];
      record.identity = TypeIdentity::Create(module->identity, record.qualifiedName, source.identity.signatureHash);
      record.sizeBytes = static_cast<std::size_t>(source.sizeBytes);
      record.alignBytes = static_cast<std::size_t>(source.alignBytes);

      if (static_cast<std::uint64_t>(source.fieldBegin) + static_cast<std::uint64_t>(source.fieldCount) > api.fieldCount ||
          static_cast<std::uint64_t>(source.propertyBegin) + static_cast<std::uint64_t>(source.propertyCount) > api.propertyCount ||
          static_cast<std::uint64_t>(source.methodBegin) + static_cast<std::uint64_t>(source.methodCount) > api.methodCount ||
          static_cast<std::uint64_t>(source.ctorBegin) + static_cast<std::uint64_t>(source.ctorCount) > api.constructorCount ||
          static_cast<std::uint64_t>(source.enumBegin) + static_cast<std::uint64_t>(source.enumCount) > api.enumValueCount ||
          static_cast<std::uint64_t>(source.baseBegin) + static_cast<std::uint64_t>(source.baseCount) > api.baseCount)
      {
        if (error)
          *error = Error{ErrorCode::CorruptModule, "type member range is out of bounds"};
        return false;
      }

      auto attributes = convertAttributes(source.attributeBegin, source.attributeCount);
      if (!attributes.has_value())
      {
        if (error)
          *error = attributes.error();
        return false;
      }
      record.attributes = std::move(*attributes);

      for (std::uint32_t i = 0; i < source.fieldCount; ++i)
      {
        const auto &field = api.fields[source.fieldBegin + i];
        if (field.nameSymbol >= symbols.size())
        {
          if (error)
            *error = Error{ErrorCode::CorruptModule, "field uses invalid symbol index"};
          return false;
        }
        auto valueType = convertTypeRef(field.valueType);
        auto fieldAttributes = convertAttributes(field.attributeBegin, field.attributeCount);
        if (!valueType.has_value() || !fieldAttributes.has_value())
        {
          if (error)
            *error = valueType.has_value() ? fieldAttributes.error() : valueType.error();
          return false;
        }
        record.fields.push_back(detail::FieldRecord{
            symbols[field.nameSymbol],
            *valueType,
            std::move(*fieldAttributes),
            field.readSlot,
            field.writeSlot});
      }

      for (std::uint32_t i = 0; i < source.propertyCount; ++i)
      {
        const auto &property = api.properties[source.propertyBegin + i];
        if (property.nameSymbol >= symbols.size())
        {
          if (error)
            *error = Error{ErrorCode::CorruptModule, "property uses invalid symbol index"};
          return false;
        }
        auto valueType = convertTypeRef(property.valueType);
        auto propertyAttributes = convertAttributes(property.attributeBegin, property.attributeCount);
        if (!valueType.has_value() || !propertyAttributes.has_value())
        {
          if (error)
            *error = valueType.has_value() ? propertyAttributes.error() : valueType.error();
          return false;
        }
        record.properties.push_back(detail::PropertyRecord{
            symbols[property.nameSymbol],
            *valueType,
            std::move(*propertyAttributes),
            property.readSlot,
            property.writeSlot});
      }

      for (std::uint32_t i = 0; i < source.methodCount; ++i)
      {
        const auto &method = api.methods[source.methodBegin + i];
        if (method.nameSymbol >= symbols.size())
        {
          if (error)
            *error = Error{ErrorCode::CorruptModule, "method uses invalid symbol index"};
          return false;
        }
        auto returnType = convertTypeRef(method.returnType);
        auto parameters = convertParameters(method.parameterBegin, method.parameterCount);
        auto methodAttributes = convertAttributes(method.attributeBegin, method.attributeCount);
        if (!returnType.has_value() || !parameters.has_value() || !methodAttributes.has_value())
        {
          if (error)
            *error = !returnType.has_value() ? returnType.error() : (!parameters.has_value() ? parameters.error() : methodAttributes.error());
          return false;
        }
        record.methods.push_back(detail::MethodRecord{
            symbols[method.nameSymbol],
            *returnType,
            std::move(*parameters),
            std::move(*methodAttributes),
            method.invokeSlot,
            method.isConst != 0u});
      }

      for (std::uint32_t i = 0; i < source.ctorCount; ++i)
      {
        const auto &ctor = api.constructors[source.ctorBegin + i];
        auto parameters = convertParameters(ctor.parameterBegin, ctor.parameterCount);
        auto ctorAttributes = convertAttributes(ctor.attributeBegin, ctor.attributeCount);
        if (!parameters.has_value() || !ctorAttributes.has_value())
        {
          if (error)
            *error = !parameters.has_value() ? parameters.error() : ctorAttributes.error();
          return false;
        }
        record.constructors.push_back(detail::ConstructorRecord{
            std::move(*parameters),
            std::move(*ctorAttributes),
            ctor.constructSlot});
      }

      for (std::uint32_t i = 0; i < source.enumCount; ++i)
      {
        const auto &entry = api.enumValues[source.enumBegin + i];
        if (entry.nameSymbol >= symbols.size())
        {
          if (error)
            *error = Error{ErrorCode::CorruptModule, "enum value uses invalid symbol index"};
          return false;
        }
        record.isEnum = true;
        record.enumValues.push_back(detail::EnumRecord{
            symbols[entry.nameSymbol],
            entry.signedValue,
            entry.unsignedValue});
      }

      for (std::uint32_t i = 0; i < source.baseCount; ++i)
      {
        const auto &entry = api.bases[source.baseBegin + i];
        if (entry.baseIdentity.qualifiedNameSymbol >= symbols.size())
        {
          if (error)
            *error = Error{ErrorCode::CorruptModule, "base uses invalid symbol index"};
          return false;
        }
        const auto baseIdentity = TypeIdentity::Create(module->identity,
                                                       symbols[entry.baseIdentity.qualifiedNameSymbol],
                                                       entry.baseIdentity.signatureHash);
        record.bases.push_back(detail::BaseRecord{
            symbols[entry.baseIdentity.qualifiedNameSymbol],
            baseIdentity,
            entry.upcastSlot,
            entry.downcastSlot});
      }

      module->types.push_back(std::move(record));
    }

    for (std::uint64_t functionIndex = 0; functionIndex < api.functionCount; ++functionIndex)
    {
      const auto &source = api.functions[functionIndex];
      if (source.nameSymbol >= symbols.size())
      {
        if (error)
          *error = Error{ErrorCode::CorruptModule, "function uses invalid symbol index"};
        return false;
      }
      auto returnType = convertTypeRef(source.returnType);
      auto parameters = convertParameters(source.parameterBegin, source.parameterCount);
      auto functionAttributes = convertAttributes(source.attributeBegin, source.attributeCount);
      if (!returnType.has_value() || !parameters.has_value() || !functionAttributes.has_value())
      {
        if (error)
          *error = !returnType.has_value() ? returnType.error() : (!parameters.has_value() ? parameters.error() : functionAttributes.error());
        return false;
      }
      module->functions.push_back(detail::FunctionRecord{
          symbols[source.nameSymbol],
          *returnType,
          std::move(*parameters),
          std::move(*functionAttributes),
          source.invokeSlot});
    }

    if (api.release)
    {
      auto holder = api.userData ? api.userData : reinterpret_cast<void *>(1);
      module->lifetime = std::shared_ptr<void>(holder, [api](void *) mutable {
        auto copy = api;
        copy.release(&copy);
      });
    }

    return detail::InstallModule(std::move(module), error);
  }

  bool UnloadModule(const ModuleIdentity &identity, Error *error) noexcept
  {
    return detail::UnloadModule(identity, error);
  }

  ExpectedType GetType(std::string_view qualifiedName)
  {
    auto handle = detail::FindTypeHandle(qualifiedName);
    if (!handle.has_value())
      return std::unexpected(handle.error());
    return Type{*handle};
  }

  std::optional<Type> FindType(std::string_view qualifiedName)
  {
    auto handle = detail::FindTypeHandle(qualifiedName);
    if (!handle.has_value())
      return std::nullopt;
    return Type{*handle};
  }

  ExpectedFunction GetFunction(std::string_view name)
  {
    auto handle = detail::FindFunctionHandle(name);
    if (!handle.has_value())
      return std::unexpected(handle.error());
    return Function{*handle};
  }

  std::optional<Function> FindFunction(std::string_view name)
  {
    auto handle = detail::FindFunctionHandle(name);
    if (!handle.has_value())
      return std::nullopt;
    return Function{*handle};
  }

  bool Field::IsValid() const noexcept
  {
    Error error{};
    auto state = detail::CurrentState();
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record && m_handle.memberIndex < record->fields.size();
  }

  std::string_view Field::Name() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->fields.size())
      return {};
    return detail::ViewSymbol(record->fields[m_handle.memberIndex].name);
  }

  std::string_view Field::ValueTypeName() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->fields.size())
      return {};
    return detail::ViewSymbol(record->fields[m_handle.memberIndex].valueType.qualifiedName);
  }

  ExpectedValue Field::Read(const ConstInstanceRef &instance) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->fields.size())
      return std::unexpected(error);
    NGINReflectionValue value{};
    const auto &field = record->fields[m_handle.memberIndex];
    if (field.readSlot >= module->tables.fieldReaders.size() || !module->tables.fieldReaders[field.readSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing field reader"});
    const auto status = module->tables.fieldReaders[field.readSlot](instance.AbiHandle(), &value);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    Value out = FromAbiValue(value);
    ReleaseAbiValue(value);
    return out;
  }

  std::expected<void, Error> Field::Write(InstanceRef &instance, const Value &value) const
  {
    auto abi = ToAbiValue(value);
    if (!abi.has_value())
      return std::unexpected(abi.error());
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->fields.size())
      return std::unexpected(error);
    const auto &field = record->fields[m_handle.memberIndex];
    if (field.writeSlot >= module->tables.fieldWriters.size() || !module->tables.fieldWriters[field.writeSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing field writer"});
    const auto status = module->tables.fieldWriters[field.writeSlot](instance.AbiHandle(), &*abi);
    ReleaseAbiValue(*abi);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    return {};
  }

  std::size_t Field::AttributeCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->fields.size())
      return 0;
    return record->fields[m_handle.memberIndex].attributes.size();
  }

  ExpectedAttribute Field::AttributeAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->fields.size())
      return std::unexpected(error);
    const auto &attributes = record->fields[m_handle.memberIndex].attributes;
    if (index >= attributes.size())
      return std::unexpected(Error{ErrorCode::NotFound, "attribute index out of range"});
    return MakeAttributeView(attributes[index]);
  }

  bool Property::IsValid() const noexcept
  {
    Error error{};
    auto state = detail::CurrentState();
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record && m_handle.memberIndex < record->properties.size();
  }

  std::string_view Property::Name() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->properties.size())
      return {};
    return detail::ViewSymbol(record->properties[m_handle.memberIndex].name);
  }

  std::string_view Property::ValueTypeName() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->properties.size())
      return {};
    return detail::ViewSymbol(record->properties[m_handle.memberIndex].valueType.qualifiedName);
  }

  ExpectedValue Property::Read(const ConstInstanceRef &instance) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->properties.size())
      return std::unexpected(error);
    NGINReflectionValue value{};
    const auto &property = record->properties[m_handle.memberIndex];
    if (property.readSlot >= module->tables.propertyReaders.size() || !module->tables.propertyReaders[property.readSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing property reader"});
    const auto status = module->tables.propertyReaders[property.readSlot](instance.AbiHandle(), &value);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    Value out = FromAbiValue(value);
    ReleaseAbiValue(value);
    return out;
  }

  std::expected<void, Error> Property::Write(InstanceRef &instance, const Value &value) const
  {
    auto abi = ToAbiValue(value);
    if (!abi.has_value())
      return std::unexpected(abi.error());
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->properties.size())
      return std::unexpected(error);
    const auto &property = record->properties[m_handle.memberIndex];
    if (property.writeSlot >= module->tables.propertyWriters.size() || !module->tables.propertyWriters[property.writeSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing property writer"});
    const auto status = module->tables.propertyWriters[property.writeSlot](instance.AbiHandle(), &*abi);
    ReleaseAbiValue(*abi);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    return {};
  }

  std::size_t Property::AttributeCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->properties.size())
      return 0;
    return record->properties[m_handle.memberIndex].attributes.size();
  }

  ExpectedAttribute Property::AttributeAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->properties.size())
      return std::unexpected(error);
    const auto &attributes = record->properties[m_handle.memberIndex].attributes;
    if (index >= attributes.size())
      return std::unexpected(Error{ErrorCode::NotFound, "attribute index out of range"});
    return MakeAttributeView(attributes[index]);
  }

  bool Method::IsValid() const noexcept
  {
    Error error{};
    auto state = detail::CurrentState();
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record && m_handle.memberIndex < record->methods.size();
  }

  std::string_view Method::Name() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->methods.size())
      return {};
    return detail::ViewSymbol(record->methods[m_handle.memberIndex].name);
  }

  std::size_t Method::ParameterCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->methods.size())
      return 0;
    return record->methods[m_handle.memberIndex].parameters.size();
  }

  std::string_view Method::ParameterTypeName(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->methods.size())
      return {};
    const auto &parameters = record->methods[m_handle.memberIndex].parameters;
    if (index >= parameters.size())
      return {};
    return detail::ViewSymbol(parameters[index].qualifiedName);
  }

  std::string_view Method::ReturnTypeName() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->methods.size())
      return {};
    return detail::ViewSymbol(record->methods[m_handle.memberIndex].returnType.qualifiedName);
  }

  ExpectedValue Method::Invoke(const InstanceRef &instance, std::span<const Value> arguments) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->methods.size())
      return std::unexpected(error);
    const auto &method = record->methods[m_handle.memberIndex];
    if (method.invokeSlot >= module->tables.methodInvokers.size() || !module->tables.methodInvokers[method.invokeSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing method invoker"});

    std::vector<NGINReflectionValue> abiArguments;
    abiArguments.reserve(arguments.size());
    for (const auto &argument : arguments)
    {
      auto abi = ToAbiValue(argument);
      if (!abi.has_value())
        return std::unexpected(abi.error());
      abiArguments.push_back(*abi);
    }

    NGINReflectionValue result{};
    const auto status = module->tables.methodInvokers[method.invokeSlot](instance.AbiHandle(),
                                                                         abiArguments.data(),
                                                                         abiArguments.size(),
                                                                         &result);
    for (auto &value : abiArguments)
      ReleaseAbiValue(value);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    Value out = FromAbiValue(result);
    ReleaseAbiValue(result);
    return out;
  }

  std::size_t Method::AttributeCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->methods.size())
      return 0;
    return record->methods[m_handle.memberIndex].attributes.size();
  }

  ExpectedAttribute Method::AttributeAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->methods.size())
      return std::unexpected(error);
    const auto &attributes = record->methods[m_handle.memberIndex].attributes;
    if (index >= attributes.size())
      return std::unexpected(Error{ErrorCode::NotFound, "attribute index out of range"});
    return MakeAttributeView(attributes[index]);
  }

  bool Constructor::IsValid() const noexcept
  {
    Error error{};
    auto state = detail::CurrentState();
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record && m_handle.memberIndex < record->constructors.size();
  }

  std::size_t Constructor::ParameterCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->constructors.size())
      return 0;
    return record->constructors[m_handle.memberIndex].parameters.size();
  }

  std::string_view Constructor::ParameterTypeName(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->constructors.size())
      return {};
    const auto &parameters = record->constructors[m_handle.memberIndex].parameters;
    if (index >= parameters.size())
      return {};
    return detail::ViewSymbol(parameters[index].qualifiedName);
  }

  ExpectedInstance Constructor::Invoke(std::span<const Value> arguments) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->constructors.size())
      return std::unexpected(error);
    const auto &ctor = record->constructors[m_handle.memberIndex];
    if (ctor.constructSlot >= module->tables.constructors.size() || !module->tables.constructors[ctor.constructSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing constructor invoker"});

    std::vector<NGINReflectionValue> abiArguments;
    abiArguments.reserve(arguments.size());
    for (const auto &argument : arguments)
    {
      auto abi = ToAbiValue(argument);
      if (!abi.has_value())
        return std::unexpected(abi.error());
      abiArguments.push_back(*abi);
    }

    NGINReflectionInstanceHandle instance{};
    const auto status = module->tables.constructors[ctor.constructSlot](abiArguments.data(),
                                                                        abiArguments.size(),
                                                                        &instance);
    for (auto &value : abiArguments)
      ReleaseAbiValue(value);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    return InstanceRef{std::make_shared<detail::InstanceStorage>(instance)};
  }

  std::size_t Constructor::AttributeCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->constructors.size())
      return 0;
    return record->constructors[m_handle.memberIndex].attributes.size();
  }

  ExpectedAttribute Constructor::AttributeAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->constructors.size())
      return std::unexpected(error);
    const auto &attributes = record->constructors[m_handle.memberIndex].attributes;
    if (index >= attributes.size())
      return std::unexpected(Error{ErrorCode::NotFound, "attribute index out of range"});
    return MakeAttributeView(attributes[index]);
  }

  bool Base::IsValid() const noexcept
  {
    Error error{};
    auto state = detail::CurrentState();
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record && m_handle.memberIndex < record->bases.size();
  }

  std::string_view Base::Name() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record || m_handle.memberIndex >= record->bases.size())
      return {};
    return detail::ViewSymbol(record->bases[m_handle.memberIndex].baseName);
  }

  ExpectedInstance Base::Upcast(const InstanceRef &instance) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->bases.size())
      return std::unexpected(error);
    const auto &base = record->bases[m_handle.memberIndex];
    if (base.upcastSlot >= module->tables.upcasters.size() || !module->tables.upcasters[base.upcastSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing upcaster"});
    NGINReflectionInstanceHandle out{};
    const auto status = module->tables.upcasters[base.upcastSlot](instance.AbiHandle(), &out);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    return InstanceRef{std::make_shared<detail::InstanceStorage>(out)};
  }

  ExpectedInstance Base::Downcast(const InstanceRef &instance) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    const auto *module = GetTypeModule(state, m_handle);
    if (!record || !module || m_handle.memberIndex >= record->bases.size())
      return std::unexpected(error);
    const auto &base = record->bases[m_handle.memberIndex];
    if (base.downcastSlot >= module->tables.downcasters.size() || !module->tables.downcasters[base.downcastSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing downcaster"});
    NGINReflectionInstanceHandle out{};
    const auto status = module->tables.downcasters[base.downcastSlot](instance.AbiHandle(), &out);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    return InstanceRef{std::make_shared<detail::InstanceStorage>(out)};
  }

  bool Function::IsValid() const noexcept
  {
    Error error{};
    auto state = detail::CurrentState();
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    return record != nullptr;
  }

  std::string_view Function::Name() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    return record ? detail::ViewSymbol(record->name) : std::string_view{};
  }

  std::size_t Function::ParameterCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    return record ? record->parameters.size() : 0;
  }

  std::string_view Function::ParameterTypeName(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    if (!record || index >= record->parameters.size())
      return {};
    return detail::ViewSymbol(record->parameters[index].qualifiedName);
  }

  std::string_view Function::ReturnTypeName() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    return record ? detail::ViewSymbol(record->returnType.qualifiedName) : std::string_view{};
  }

  ExpectedValue Function::Invoke(std::span<const Value> arguments) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    const auto *module = GetFunctionModule(state, m_handle);
    if (!record || !module)
      return std::unexpected(error);
    if (record->invokeSlot >= module->tables.functionInvokers.size() || !module->tables.functionInvokers[record->invokeSlot])
      return std::unexpected(Error{ErrorCode::InternalError, "missing function invoker"});

    std::vector<NGINReflectionValue> abiArguments;
    abiArguments.reserve(arguments.size());
    for (const auto &argument : arguments)
    {
      auto abi = ToAbiValue(argument);
      if (!abi.has_value())
        return std::unexpected(abi.error());
      abiArguments.push_back(*abi);
    }

    NGINReflectionValue result{};
    const auto status = module->tables.functionInvokers[record->invokeSlot](abiArguments.data(),
                                                                            abiArguments.size(),
                                                                            &result);
    for (auto &value : abiArguments)
      ReleaseAbiValue(value);
    if (status.code != NGINReflectionStatus_Ok)
      return std::unexpected(Error{static_cast<ErrorCode>(status.code), ToOwnedString({status.message.data, static_cast<std::size_t>(status.message.size)})});
    Value out = FromAbiValue(result);
    ReleaseAbiValue(result);
    return out;
  }

  std::size_t Function::AttributeCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    return record ? record->attributes.size() : 0;
  }

  ExpectedAttribute Function::AttributeAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetFunctionRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->attributes.size())
      return std::unexpected(Error{ErrorCode::NotFound, "attribute index out of range"});
    return MakeAttributeView(record->attributes[index]);
  }

  bool Type::IsValid() const noexcept
  {
    Error error{};
    auto state = detail::CurrentState();
    return GetTypeRecord(state, m_handle, &error) != nullptr;
  }

  TypeIdentity Type::Identity() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->identity : TypeIdentity{};
  }

  std::string_view Type::QualifiedName() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? detail::ViewSymbol(record->qualifiedName) : std::string_view{};
  }

  std::size_t Type::FieldCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->fields.size() : 0;
  }

  std::size_t Type::PropertyCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->properties.size() : 0;
  }

  std::size_t Type::MethodCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->methods.size() : 0;
  }

  std::size_t Type::ConstructorCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->constructors.size() : 0;
  }

  std::size_t Type::EnumValueCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->enumValues.size() : 0;
  }

  std::size_t Type::BaseCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->bases.size() : 0;
  }

  std::size_t Type::AttributeCount() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->attributes.size() : 0;
  }

  bool Type::IsEnum() const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    return record ? record->isEnum : false;
  }

  ExpectedField Type::FieldAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->fields.size())
      return std::unexpected(Error{ErrorCode::NotFound, "field index out of range"});
    return Field{detail::Handle{m_handle.typeIndex, index, m_handle.generation}};
  }

  ExpectedProperty Type::PropertyAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->properties.size())
      return std::unexpected(Error{ErrorCode::NotFound, "property index out of range"});
    return Property{detail::Handle{m_handle.typeIndex, index, m_handle.generation}};
  }

  ExpectedMethod Type::MethodAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->methods.size())
      return std::unexpected(Error{ErrorCode::NotFound, "method index out of range"});
    return Method{detail::Handle{m_handle.typeIndex, index, m_handle.generation}};
  }

  ExpectedConstructor Type::ConstructorAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->constructors.size())
      return std::unexpected(Error{ErrorCode::NotFound, "constructor index out of range"});
    return Constructor{detail::Handle{m_handle.typeIndex, index, m_handle.generation}};
  }

  ExpectedEnumValue Type::EnumValueAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->enumValues.size())
      return std::unexpected(Error{ErrorCode::NotFound, "enum index out of range"});
    const auto &entry = record->enumValues[index];
    return EnumValue{detail::ViewSymbol(entry.name), entry.signedValue, entry.unsignedValue};
  }

  ExpectedBase Type::BaseAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->bases.size())
      return std::unexpected(Error{ErrorCode::NotFound, "base index out of range"});
    return Base{detail::Handle{m_handle.typeIndex, index, m_handle.generation}};
  }

  ExpectedAttribute Type::AttributeAt(std::size_t index) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    if (index >= record->attributes.size())
      return std::unexpected(Error{ErrorCode::NotFound, "attribute index out of range"});
    return MakeAttributeView(record->attributes[index]);
  }

  ExpectedField Type::GetField(std::string_view name) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    auto index = FindNamedField(*record, name);
    if (!index.has_value())
      return std::unexpected(index.error());
    return Field{detail::Handle{m_handle.typeIndex, *index, m_handle.generation}};
  }

  ExpectedProperty Type::GetProperty(std::string_view name) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    auto index = FindNamedProperty(*record, name);
    if (!index.has_value())
      return std::unexpected(index.error());
    return Property{detail::Handle{m_handle.typeIndex, *index, m_handle.generation}};
  }

  ExpectedMethod Type::GetMethod(std::string_view name) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    auto index = FindNamedMethod(*record, name);
    if (!index.has_value())
      return std::unexpected(index.error());
    return Method{detail::Handle{m_handle.typeIndex, *index, m_handle.generation}};
  }

  ExpectedMethod Type::ResolveMethod(std::string_view name, std::span<const Value> arguments) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    for (std::size_t i = 0; i < record->methods.size(); ++i)
    {
      const auto &method = record->methods[i];
      if (detail::ViewSymbol(method.name) == name && method.parameters.size() == arguments.size())
        return Method{detail::Handle{m_handle.typeIndex, i, m_handle.generation}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "no matching overload"});
  }

  ExpectedConstructor Type::ResolveConstructor(std::span<const Value> arguments) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    for (std::size_t i = 0; i < record->constructors.size(); ++i)
    {
      if (record->constructors[i].parameters.size() == arguments.size())
        return Constructor{detail::Handle{m_handle.typeIndex, i, m_handle.generation}};
    }
    return std::unexpected(Error{ErrorCode::NotFound, "no matching constructor"});
  }

  ExpectedInstance Type::Construct(std::span<const Value> arguments) const
  {
    auto ctor = ResolveConstructor(arguments);
    if (!ctor.has_value())
      return std::unexpected(ctor.error());
    return ctor->Invoke(arguments);
  }

  ExpectedEnumValue Type::GetEnumValue(std::string_view name) const
  {
    auto state = detail::CurrentState();
    Error error{};
    const auto *record = GetTypeRecord(state, m_handle, &error);
    if (!record)
      return std::unexpected(error);
    auto index = FindNamedEnum(*record, name);
    if (!index.has_value())
      return std::unexpected(index.error());
    const auto &entry = record->enumValues[*index];
    return EnumValue{detail::ViewSymbol(entry.name), entry.signedValue, entry.unsignedValue};
  }

  namespace
  {
    std::expected<AttributeValue, Error> ImportAttributeValue(const NGINReflectionValue &value,
                                                             const std::vector<SymbolId> &symbols)
    {
      switch (value.kind)
      {
      case NGINReflectionValue_Bool:
        return AttributeValue(static_cast<bool>(value.boolValue != 0u));
      case NGINReflectionValue_Int64:
        return AttributeValue(static_cast<std::int64_t>(value.intValue));
      case NGINReflectionValue_UInt64:
        return AttributeValue(static_cast<std::uint64_t>(value.uintValue));
      case NGINReflectionValue_Float64:
        return AttributeValue(static_cast<double>(value.floatValue));
      case NGINReflectionValue_String:
        return AttributeValue(std::string(value.stringValue.data, static_cast<std::size_t>(value.stringValue.size)));
      case NGINReflectionValue_Symbol:
        if (value.symbolValue >= symbols.size())
          return std::unexpected(Error{ErrorCode::CorruptModule, "attribute symbol is out of bounds"});
        return AttributeValue(symbols[value.symbolValue]);
      case NGINReflectionValue_TypeIdentity:
      {
        if (value.typeIdentity.module.moduleNameSymbol >= symbols.size() ||
            value.typeIdentity.qualifiedNameSymbol >= symbols.size())
          return std::unexpected(Error{ErrorCode::CorruptModule, "attribute type identity is out of bounds"});
        const auto moduleIdentity = ModuleIdentity::Create(symbols[value.typeIdentity.module.moduleNameSymbol],
                                                           value.typeIdentity.module.abiFamily,
                                                           value.typeIdentity.module.abiVersion);
        return AttributeValue(TypeIdentity::Create(moduleIdentity,
                                                   symbols[value.typeIdentity.qualifiedNameSymbol],
                                                   value.typeIdentity.signatureHash));
      }
      default:
        return std::unexpected(Error{ErrorCode::CorruptModule, "unsupported attribute value kind"});
      }
    }

    std::uint32_t ExportSymbol(ExportedModuleApiState &state, SymbolId symbol)
    {
      if (!symbol.IsValid())
        return 0u;
      if (auto it = state.localSymbols.find(symbol.value); it != state.localSymbols.end())
        return it->second;

      const auto local = static_cast<std::uint32_t>(state.symbols.size());
      state.localSymbols.emplace(symbol.value, local);
      state.ownedStrings.push_back(std::string(detail::ViewSymbol(symbol)));
      const auto &owned = state.ownedStrings.back();
      state.symbols.push_back({owned.data(), static_cast<std::uint64_t>(owned.size())});
      return local;
    }

    NGINReflectionTypeIdentity ExportTypeIdentity(ExportedModuleApiState &state, const TypeIdentity &identity)
    {
      return NGINReflectionTypeIdentity{
          NGINReflectionModuleIdentity{
              ExportSymbol(state, identity.module.moduleName),
              identity.module.abiFamily,
              identity.module.abiVersion,
              0u,
              identity.module.keyHash},
          ExportSymbol(state, identity.qualifiedName),
          0u,
          identity.signatureHash,
          identity.keyHash};
    }

    NGINReflectionTypeRef ExportTypeRef(ExportedModuleApiState &state, const detail::TypeReference &reference)
    {
      return NGINReflectionTypeRef{
          ExportSymbol(state, reference.qualifiedName),
          0u,
          reference.fastHash,
          reference.reflectedTypeKey};
    }

    NGINReflectionValue ExportAttributeValue(ExportedModuleApiState &state, const AttributeValue &value)
    {
      if (const auto *b = std::get_if<bool>(&value))
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Bool;
        out.boolValue = *b ? 1u : 0u;
        return out;
      }
      if (const auto *i = std::get_if<std::int64_t>(&value))
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Int64;
        out.intValue = *i;
        return out;
      }
      if (const auto *u = std::get_if<std::uint64_t>(&value))
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_UInt64;
        out.uintValue = *u;
        return out;
      }
      if (const auto *d = std::get_if<double>(&value))
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Float64;
        out.floatValue = *d;
        return out;
      }
      if (const auto *s = std::get_if<std::string>(&value))
      {
        auto *owned = new std::string(*s);
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_String;
        out.stringValue = {owned->data(), static_cast<std::uint64_t>(owned->size())};
        out.release = [](NGINReflectionValue *value) {
          delete static_cast<std::string *>(value->releaseUserData);
          value->release = nullptr;
          value->releaseUserData = nullptr;
        };
        out.releaseUserData = owned;
        return out;
      }
      if (const auto *symbol = std::get_if<SymbolId>(&value))
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Symbol;
        out.symbolValue = ExportSymbol(state, *symbol);
        return out;
      }
      const auto &identity = std::get<TypeIdentity>(value);
      NGINReflectionValue out{};
      out.kind = NGINReflectionValue_TypeIdentity;
      out.typeIdentity = ExportTypeIdentity(state, identity);
      return out;
    }
  } // namespace

  extern "C" NGIN_REFLECTION_API bool NGINReflectionGetModuleApi(NGINReflectionModuleApi *out)
  {
    if (!out)
      return false;

    auto selected = SelectExportModule();
    if (!selected.has_value())
      return false;

    auto state = std::make_unique<ExportedModuleApiState>();
    state->ownedStrings.emplace_back();
    state->symbols.push_back({nullptr, 0u});

    const auto &module = **selected;
    for (const auto &type : module.types)
    {
      ExportSymbol(*state, type.qualifiedName);
      for (const auto &field : type.fields)
      {
        ExportSymbol(*state, field.name);
        ExportSymbol(*state, field.valueType.qualifiedName);
      }
      for (const auto &property : type.properties)
      {
        ExportSymbol(*state, property.name);
        ExportSymbol(*state, property.valueType.qualifiedName);
      }
      for (const auto &method : type.methods)
      {
        ExportSymbol(*state, method.name);
        ExportSymbol(*state, method.returnType.qualifiedName);
        for (const auto &parameter : method.parameters)
          ExportSymbol(*state, parameter.qualifiedName);
      }
      for (const auto &ctor : type.constructors)
        for (const auto &parameter : ctor.parameters)
          ExportSymbol(*state, parameter.qualifiedName);
      for (const auto &entry : type.enumValues)
        ExportSymbol(*state, entry.name);
      for (const auto &base : type.bases)
      {
        ExportSymbol(*state, base.baseName);
        ExportSymbol(*state, base.baseIdentity.qualifiedName);
      }
      for (const auto &attribute : type.attributes)
        ExportSymbol(*state, attribute.name);
    }
    for (const auto &function : module.functions)
    {
      ExportSymbol(*state, function.name);
      ExportSymbol(*state, function.returnType.qualifiedName);
      for (const auto &parameter : function.parameters)
        ExportSymbol(*state, parameter.qualifiedName);
      for (const auto &attribute : function.attributes)
        ExportSymbol(*state, attribute.name);
    }

    state->fieldReaders = module.tables.fieldReaders;
    state->fieldWriters = module.tables.fieldWriters;
    state->propertyReaders = module.tables.propertyReaders;
    state->propertyWriters = module.tables.propertyWriters;
    state->methodInvokers = module.tables.methodInvokers;
    state->constructorInvokers = module.tables.constructors;
    state->functionInvokers = module.tables.functionInvokers;
    state->upcasters = module.tables.upcasters;
    state->downcasters = module.tables.downcasters;

    auto appendAttributes = [&](const std::vector<detail::AttributeRecord> &source, std::uint32_t &begin, std::uint32_t &count) {
      begin = static_cast<std::uint32_t>(state->attributes.size());
      count = static_cast<std::uint32_t>(source.size());
      for (const auto &attribute : source)
      {
        state->attributes.push_back(NGINReflectionAttributeDesc{
            ExportSymbol(*state, attribute.name),
            0u,
            ExportAttributeValue(*state, attribute.value)});
      }
    };

    auto appendParameters = [&](const std::vector<detail::TypeReference> &source, std::uint32_t &begin, std::uint32_t &count) {
      begin = static_cast<std::uint32_t>(state->parameters.size());
      count = static_cast<std::uint32_t>(source.size());
      for (const auto &parameter : source)
        state->parameters.push_back(ExportTypeRef(*state, parameter));
    };

    for (const auto &type : module.types)
    {
      std::uint32_t fieldBegin = static_cast<std::uint32_t>(state->fields.size());
      for (const auto &field : type.fields)
      {
        std::uint32_t attributeBegin = 0, attributeCount = 0;
        appendAttributes(field.attributes, attributeBegin, attributeCount);
        state->fields.push_back(NGINReflectionFieldDesc{
            ExportSymbol(*state, field.name),
            0u,
            ExportTypeRef(*state, field.valueType),
            attributeBegin,
            attributeCount,
            field.readSlot,
            field.writeSlot});
      }

      std::uint32_t propertyBegin = static_cast<std::uint32_t>(state->properties.size());
      for (const auto &property : type.properties)
      {
        std::uint32_t attributeBegin = 0, attributeCount = 0;
        appendAttributes(property.attributes, attributeBegin, attributeCount);
        state->properties.push_back(NGINReflectionPropertyDesc{
            ExportSymbol(*state, property.name),
            0u,
            ExportTypeRef(*state, property.valueType),
            attributeBegin,
            attributeCount,
            property.readSlot,
            property.writeSlot});
      }

      std::uint32_t methodBegin = static_cast<std::uint32_t>(state->methods.size());
      for (const auto &method : type.methods)
      {
        std::uint32_t parameterBegin = 0, parameterCount = 0;
        std::uint32_t attributeBegin = 0, attributeCount = 0;
        appendParameters(method.parameters, parameterBegin, parameterCount);
        appendAttributes(method.attributes, attributeBegin, attributeCount);
        state->methods.push_back(NGINReflectionMethodDesc{
            ExportSymbol(*state, method.name),
            0u,
            ExportTypeRef(*state, method.returnType),
            parameterBegin,
            parameterCount,
            attributeBegin,
            attributeCount,
            method.invokeSlot,
            method.isConst ? 1u : 0u});
      }

      std::uint32_t ctorBegin = static_cast<std::uint32_t>(state->constructors.size());
      for (const auto &ctor : type.constructors)
      {
        std::uint32_t parameterBegin = 0, parameterCount = 0;
        std::uint32_t attributeBegin = 0, attributeCount = 0;
        appendParameters(ctor.parameters, parameterBegin, parameterCount);
        appendAttributes(ctor.attributes, attributeBegin, attributeCount);
        state->constructors.push_back(NGINReflectionConstructorDesc{
            parameterBegin,
            parameterCount,
            attributeBegin,
            attributeCount,
            ctor.constructSlot,
            0u});
      }

      std::uint32_t enumBegin = static_cast<std::uint32_t>(state->enumValues.size());
      for (const auto &entry : type.enumValues)
      {
        state->enumValues.push_back(NGINReflectionEnumValueDesc{
            ExportSymbol(*state, entry.name),
            0u,
            entry.signedValue,
            entry.unsignedValue});
      }

      std::uint32_t baseBegin = static_cast<std::uint32_t>(state->bases.size());
      for (const auto &base : type.bases)
      {
        state->bases.push_back(NGINReflectionBaseDesc{
            ExportTypeIdentity(*state, base.baseIdentity),
            base.upcastSlot,
            base.downcastSlot});
      }

      std::uint32_t attributeBegin = 0, attributeCount = 0;
      appendAttributes(type.attributes, attributeBegin, attributeCount);

      state->types.push_back(NGINReflectionTypeDesc{
          ExportTypeIdentity(*state, type.identity),
          static_cast<std::uint64_t>(type.sizeBytes),
          static_cast<std::uint64_t>(type.alignBytes),
          fieldBegin,
          static_cast<std::uint32_t>(type.fields.size()),
          propertyBegin,
          static_cast<std::uint32_t>(type.properties.size()),
          methodBegin,
          static_cast<std::uint32_t>(type.methods.size()),
          ctorBegin,
          static_cast<std::uint32_t>(type.constructors.size()),
          enumBegin,
          static_cast<std::uint32_t>(type.enumValues.size()),
          baseBegin,
          static_cast<std::uint32_t>(type.bases.size()),
          attributeBegin,
          attributeCount});
    }

    for (const auto &function : module.functions)
    {
      std::uint32_t parameterBegin = 0, parameterCount = 0;
      std::uint32_t attributeBegin = 0, attributeCount = 0;
      appendParameters(function.parameters, parameterBegin, parameterCount);
      appendAttributes(function.attributes, attributeBegin, attributeCount);
      state->functions.push_back(NGINReflectionFunctionDesc{
          ExportSymbol(*state, function.name),
          0u,
          ExportTypeRef(*state, function.returnType),
          parameterBegin,
          parameterCount,
          attributeBegin,
          attributeCount,
          function.invokeSlot,
          0u});
    }

    out->header = {kAbiFamily, kAbiVersion};
    out->identity = {
        ExportSymbol(*state, module.identity.moduleName),
        module.identity.abiFamily,
        module.identity.abiVersion,
        0u,
        module.identity.keyHash};
    out->symbols = state->symbols.data();
    out->symbolCount = state->symbols.size();
    out->attributes = state->attributes.data();
    out->attributeCount = state->attributes.size();
    out->parameters = state->parameters.data();
    out->parameterCount = state->parameters.size();
    out->fields = state->fields.data();
    out->fieldCount = state->fields.size();
    out->properties = state->properties.data();
    out->propertyCount = state->properties.size();
    out->methods = state->methods.data();
    out->methodCount = state->methods.size();
    out->constructors = state->constructors.data();
    out->constructorCount = state->constructors.size();
    out->enumValues = state->enumValues.data();
    out->enumValueCount = state->enumValues.size();
    out->bases = state->bases.data();
    out->baseCount = state->bases.size();
    out->types = state->types.data();
    out->typeCount = state->types.size();
    out->functions = state->functions.data();
    out->functionCount = state->functions.size();
    out->fieldReaders = state->fieldReaders.data();
    out->fieldReaderCount = state->fieldReaders.size();
    out->fieldWriters = state->fieldWriters.data();
    out->fieldWriterCount = state->fieldWriters.size();
    out->propertyReaders = state->propertyReaders.data();
    out->propertyReaderCount = state->propertyReaders.size();
    out->propertyWriters = state->propertyWriters.data();
    out->propertyWriterCount = state->propertyWriters.size();
    out->methodInvokers = state->methodInvokers.data();
    out->methodInvokerCount = state->methodInvokers.size();
    out->constructorsInvokers = state->constructorInvokers.data();
    out->constructorInvokerCount = state->constructorInvokers.size();
    out->functionInvokers = state->functionInvokers.data();
    out->functionInvokerCount = state->functionInvokers.size();
    out->upcasters = state->upcasters.data();
    out->upcasterCount = state->upcasters.size();
    out->downcasters = state->downcasters.data();
    out->downcasterCount = state->downcasters.size();
    out->userData = state.release();
    out->release = [](NGINReflectionModuleApi *api) {
      delete static_cast<ExportedModuleApiState *>(api->userData);
      api->userData = nullptr;
      api->release = nullptr;
    };
    return true;
  }
} // namespace NGIN::Reflection
