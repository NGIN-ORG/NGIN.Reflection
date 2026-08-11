#pragma once

#include <NGIN/Memory/SmartPointers.hpp>
#include <NGIN/Reflection/Registry.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace NGIN::Reflection
{
  template <std::size_t N>
  struct FixedString
  {
    char value[N]{};

    constexpr FixedString(const char (&text)[N])
    {
      std::copy_n(text, N, value);
    }

    [[nodiscard]] constexpr std::string_view View() const noexcept
    {
      return {value, N - 1u};
    }
  };

  template <class T>
  struct ConstructorDependency
  {
    using Type = T;
  };

  template <class T>
  struct OptionalConstructorDependency
  {
    using Type = T;
  };

  template <class T, FixedString Name>
  struct NamedConstructorDependency
  {
    using Type = T;
    inline static constexpr auto name = Name;
  };

  template <class T, FixedString Name>
  struct NamedOptionalConstructorDependency
  {
    using Type = T;
    inline static constexpr auto name = Name;
  };

  namespace detail
  {
    template <class T>
    struct ConstructorDependencyTraits
    {
      using Type = T;
      static constexpr bool optional = false;
      [[nodiscard]] static constexpr std::string_view Name() noexcept { return {}; }
    };

    template <class T>
    struct ConstructorDependencyTraits<ConstructorDependency<T>> : ConstructorDependencyTraits<T>
    {
    };

    template <class T>
    struct ConstructorDependencyTraits<OptionalConstructorDependency<T>>
    {
      using Type = T;
      static constexpr bool optional = true;
      [[nodiscard]] static constexpr std::string_view Name() noexcept { return {}; }
    };

    template <class T, FixedString NameValue>
    struct ConstructorDependencyTraits<NamedConstructorDependency<T, NameValue>>
    {
      using Type = T;
      static constexpr bool optional = false;
      [[nodiscard]] static constexpr std::string_view Name() noexcept { return NameValue.View(); }
    };

    template <class T, FixedString NameValue>
    struct ConstructorDependencyTraits<NamedOptionalConstructorDependency<T, NameValue>>
    {
      using Type = T;
      static constexpr bool optional = true;
      [[nodiscard]] static constexpr std::string_view Name() noexcept { return NameValue.View(); }
    };

    inline constexpr NGINReflectionStatus OkStatus() noexcept
    {
      return NGINReflectionStatus{NGINReflectionStatus_Ok, {nullptr, 0}};
    }

    inline constexpr NGINReflectionStatus MakeStatus(NGINReflectionStatusCode code, const char *message) noexcept
    {
      return NGINReflectionStatus{code, {message, message ? std::strlen(message) : 0u}};
    }

    inline NGINReflectionModuleIdentity ToAbiModuleIdentity(const ModuleIdentity &identity) noexcept
    {
      return NGINReflectionModuleIdentity{
          identity.moduleName.value,
          identity.abiFamily,
          identity.abiVersion,
          0u,
          identity.keyHash};
    }

    inline NGINReflectionTypeIdentity ToAbiTypeIdentity(const TypeIdentity &identity) noexcept
    {
      return NGINReflectionTypeIdentity{
          ToAbiModuleIdentity(identity.module),
          identity.qualifiedName.value,
          0u,
          identity.signatureHash,
          identity.keyHash};
    }

    template <class T>
    [[nodiscard]] inline TypeIdentity SyntheticTypeIdentity(const ModuleIdentity &module)
    {
      const auto symbol = InternSymbol(NGIN::Meta::TypeName<std::remove_cvref_t<T>>::qualifiedName);
      const auto signature = NGIN::Meta::GetTypeId<std::remove_cvref_t<T>>();
      return TypeIdentity::Create(module, symbol, signature == 0 ? 1u : signature);
    }

    [[nodiscard]] inline ModuleIdentity RuntimeModuleIdentity()
    {
      return ModuleIdentity::Create(InternSymbol("NGIN.Reflection.Runtime"));
    }

    template <class T>
    [[nodiscard]] inline TypeReference MakeTypeReference()
    {
      return MakeTypeReference(NGIN::Meta::TypeName<std::remove_cvref_t<T>>::qualifiedName);
    }

    template <class T>
    struct IsSharedPointer : std::false_type
    {
    };

    template <class T, NGIN::Memory::AllocatorConcept Alloc>
    struct IsSharedPointer<NGIN::Memory::Shared<T, Alloc>> : std::true_type
    {
    };

    template <class T>
    [[nodiscard]] inline std::expected<T, const char *> ConvertScalarValue(const NGINReflectionValue &value)
    {
      using U = std::remove_cvref_t<T>;
      if constexpr (std::is_same_v<U, bool>)
      {
        if (value.kind != NGINReflectionValue_Bool)
          return std::unexpected("expected bool");
        return static_cast<bool>(value.boolValue != 0u);
      }
      else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && !std::is_same_v<U, bool>)
      {
        if (value.kind == NGINReflectionValue_Int64)
          return static_cast<U>(value.intValue);
        if (value.kind == NGINReflectionValue_UInt64)
          return static_cast<U>(value.uintValue);
        return std::unexpected("expected signed integer");
      }
      else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>)
      {
        if (value.kind == NGINReflectionValue_UInt64)
          return static_cast<U>(value.uintValue);
        if (value.kind == NGINReflectionValue_Int64)
          return static_cast<U>(value.intValue);
        return std::unexpected("expected unsigned integer");
      }
      else if constexpr (std::is_floating_point_v<U>)
      {
        if (value.kind == NGINReflectionValue_Float64)
          return static_cast<U>(value.floatValue);
        if (value.kind == NGINReflectionValue_Int64)
          return static_cast<U>(value.intValue);
        if (value.kind == NGINReflectionValue_UInt64)
          return static_cast<U>(value.uintValue);
        return std::unexpected("expected floating-point value");
      }
      else if constexpr (std::is_enum_v<U>)
      {
        using Under = std::underlying_type_t<U>;
        auto converted = ConvertScalarValue<Under>(value);
        if (!converted.has_value())
          return std::unexpected(converted.error());
        return static_cast<U>(*converted);
      }
      else if constexpr (std::is_same_v<U, std::string>)
      {
        if (value.kind != NGINReflectionValue_String)
          return std::unexpected("expected string");
        return std::string(value.stringValue.data, static_cast<std::size_t>(value.stringValue.size));
      }
      else if constexpr (std::is_same_v<U, std::string_view>)
      {
        if (value.kind != NGINReflectionValue_String)
          return std::unexpected("expected string");
        return std::string_view(value.stringValue.data, static_cast<std::size_t>(value.stringValue.size));
      }
      else
      {
        if constexpr (IsSharedPointer<U>::value)
        {
          if (value.kind == NGINReflectionValue_Empty)
            return U{};
        }
        if (value.kind == NGINReflectionValue_Instance && value.instanceValue.vtable)
        {
          const auto *instance = static_cast<const U *>(value.instanceValue.vtable->getConst(&value.instanceValue));
          if (instance)
            return *instance;
        }
        return std::unexpected("unsupported argument type");
      }
    }

    template <class T>
    [[nodiscard]] inline std::expected<std::remove_cvref_t<T>, const char *> ConvertValue(const NGINReflectionValue &value)
    {
      using U = std::remove_cvref_t<T>;
      if constexpr (std::is_reference_v<T>)
      {
        auto converted = ConvertScalarValue<U>(value);
        if (!converted.has_value())
          return std::unexpected(converted.error());
        return *converted;
      }
      else
      {
        return ConvertScalarValue<U>(value);
      }
    }

    struct LocalSharedBoxHeader
    {
      std::atomic<std::uint32_t> refs{1};
    };

    template <class T>
    struct LocalSharedBox final : LocalSharedBoxHeader
    {
      template <class... Args>
      explicit LocalSharedBox(Args &&...args)
          : object(std::forward<Args>(args)...)
      {
      }

      T object;
    };

    inline void BorrowRetain(NGINReflectionInstanceHandle *) {}
    inline void BorrowRelease(NGINReflectionInstanceHandle *) {}
    inline const void *BorrowGetConst(const NGINReflectionInstanceHandle *instance) { return instance ? instance->object : nullptr; }
    inline void *BorrowGetMut(const NGINReflectionInstanceHandle *instance) { return instance ? instance->object : nullptr; }

    inline const NGINReflectionInstanceVTable *BorrowedInstanceVTable() noexcept
    {
      static const NGINReflectionInstanceVTable table{
          &BorrowRetain,
          &BorrowRelease,
          &BorrowGetConst,
          &BorrowGetMut};
      return &table;
    }

    template <class T>
    void OwnedRetain(NGINReflectionInstanceHandle *instance)
    {
      if (!instance || !instance->userData)
        return;
      static_cast<LocalSharedBoxHeader *>(instance->userData)->refs.fetch_add(1u, std::memory_order_relaxed);
    }

    template <class T>
    void OwnedRelease(NGINReflectionInstanceHandle *instance)
    {
      if (!instance || !instance->userData)
        return;
      auto *header = static_cast<LocalSharedBoxHeader *>(instance->userData);
      if (header->refs.fetch_sub(1u, std::memory_order_acq_rel) == 1u)
        delete static_cast<LocalSharedBox<T> *>(instance->userData);
      instance->object = nullptr;
      instance->userData = nullptr;
    }

    template <class T>
    const NGINReflectionInstanceVTable *OwnedInstanceVTable() noexcept
    {
      static const NGINReflectionInstanceVTable table{
          &OwnedRetain<T>,
          &OwnedRelease<T>,
          &BorrowGetConst,
          &BorrowGetMut};
      return &table;
    }

    template <class T>
    [[nodiscard]] inline NGINReflectionInstanceHandle MakeBorrowedHandle(T *instance,
                                                                         const TypeIdentity &identity) noexcept
    {
      return NGINReflectionInstanceHandle{
          instance,
          nullptr,
          BorrowedInstanceVTable(),
          ToAbiTypeIdentity(identity)};
    }

    template <class BaseT, class DerivedT>
    [[nodiscard]] inline NGINReflectionStatus UpcastThunk(const NGINReflectionInstanceHandle *instance,
                                                          NGINReflectionInstanceHandle *outInstance)
    {
      if (!instance || !outInstance || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid upcast");
      auto *derived = static_cast<DerivedT *>(instance->vtable->getMut(instance));
      if (!derived)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      *outInstance = *instance;
      if (outInstance->vtable && outInstance->vtable->retain)
        outInstance->vtable->retain(outInstance);
      outInstance->object = static_cast<BaseT *>(derived);
      const auto module = ModuleIdentity::Create(SymbolId{instance->identity.module.moduleNameSymbol},
                                                 instance->identity.module.abiFamily,
                                                 instance->identity.module.abiVersion);
      outInstance->identity = ToAbiTypeIdentity(SyntheticTypeIdentity<BaseT>(module));
      return OkStatus();
    }

    template <class DerivedT, class BaseT>
    [[nodiscard]] inline NGINReflectionStatus DowncastThunk(const NGINReflectionInstanceHandle *instance,
                                                            NGINReflectionInstanceHandle *outInstance)
    {
      if (!instance || !outInstance || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid downcast");
      auto *base = static_cast<BaseT *>(instance->vtable->getMut(instance));
      if (!base)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      *outInstance = *instance;
      if (outInstance->vtable && outInstance->vtable->retain)
        outInstance->vtable->retain(outInstance);
      outInstance->object = static_cast<DerivedT *>(base);
      const auto module = ModuleIdentity::Create(SymbolId{instance->identity.module.moduleNameSymbol},
                                                 instance->identity.module.abiFamily,
                                                 instance->identity.module.abiVersion);
      outInstance->identity = ToAbiTypeIdentity(SyntheticTypeIdentity<DerivedT>(module));
      return OkStatus();
    }

    template <class DerivedT, class BaseT, auto DowncastFn>
    [[nodiscard]] inline NGINReflectionStatus CustomDowncastThunk(const NGINReflectionInstanceHandle *instance,
                                                                  NGINReflectionInstanceHandle *outInstance)
    {
      if (!instance || !outInstance || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid downcast");
      auto *base = static_cast<BaseT *>(instance->vtable->getMut(instance));
      if (!base)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      auto *derived = DowncastFn(base);
      if (!derived)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "downcast failed");
      *outInstance = *instance;
      if (outInstance->vtable && outInstance->vtable->retain)
        outInstance->vtable->retain(outInstance);
      outInstance->object = derived;
      const auto module = ModuleIdentity::Create(SymbolId{instance->identity.module.moduleNameSymbol},
                                                 instance->identity.module.abiFamily,
                                                 instance->identity.module.abiVersion);
      outInstance->identity = ToAbiTypeIdentity(SyntheticTypeIdentity<DerivedT>(module));
      return OkStatus();
    }

    template <class T>
    [[nodiscard]] inline NGINReflectionInstanceHandle MakeOwnedHandle(T value,
                                                                      const TypeIdentity &identity)
    {
      auto *box = new LocalSharedBox<std::remove_cvref_t<T>>(std::move(value));
      return NGINReflectionInstanceHandle{
          &box->object,
          box,
          OwnedInstanceVTable<std::remove_cvref_t<T>>(),
          ToAbiTypeIdentity(identity)};
    }

    inline void ReleaseOwnedString(NGINReflectionValue *value)
    {
      if (!value || !value->releaseUserData)
        return;
      delete static_cast<std::string *>(value->releaseUserData);
      value->release = nullptr;
      value->releaseUserData = nullptr;
      value->kind = NGINReflectionValue_Empty;
    }

    template <class T>
    [[nodiscard]] inline NGINReflectionValue ToAbiValue(const ModuleIdentity &moduleIdentity, T &&value)
    {
      using U = std::remove_cvref_t<T>;
      if constexpr (std::is_same_v<U, bool>)
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Bool;
        out.boolValue = value ? 1u : 0u;
        return out;
      }
      else if constexpr (std::is_integral_v<U> && std::is_signed_v<U> && !std::is_same_v<U, bool>)
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Int64;
        out.intValue = static_cast<std::int64_t>(value);
        return out;
      }
      else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>)
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_UInt64;
        out.uintValue = static_cast<std::uint64_t>(value);
        return out;
      }
      else if constexpr (std::is_floating_point_v<U>)
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Float64;
        out.floatValue = static_cast<double>(value);
        return out;
      }
      else if constexpr (std::is_enum_v<U>)
      {
        using Under = std::underlying_type_t<U>;
        return ToAbiValue(moduleIdentity, static_cast<Under>(value));
      }
      else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>)
      {
        auto *owned = new std::string(value);
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_String;
        out.stringValue = NGINReflectionStringView{owned->data(), static_cast<std::uint64_t>(owned->size())};
        out.release = &ReleaseOwnedString;
        out.releaseUserData = owned;
        return out;
      }
      else if constexpr (std::is_same_v<U, const char *>)
      {
        std::string_view view = value ? std::string_view{value} : std::string_view{};
        return ToAbiValue(moduleIdentity, view);
      }
      else
      {
        NGINReflectionValue out{};
        out.kind = NGINReflectionValue_Instance;
        out.instanceValue = MakeOwnedHandle<U>(std::forward<T>(value), SyntheticTypeIdentity<U>(moduleIdentity));
        return out;
      }
    }

    template <class T>
    struct MemberPointerTraits;

    template <class C, class M>
    struct MemberPointerTraits<M C::*>
    {
      using Class = C;
      using Member = M;
    };

    template <class>
    struct MethodTraits;

    template <class C, class R, class... A>
    struct MethodTraits<R (C::*)(A...)>
    {
      using Class = C;
      using Return = R;
      using Args = std::tuple<A...>;
      static constexpr bool IsConst = false;
    };

    template <class C, class R, class... A>
    struct MethodTraits<R (C::*)(A...) const>
    {
      using Class = C;
      using Return = R;
      using Args = std::tuple<A...>;
      static constexpr bool IsConst = true;
    };

    template <class>
    struct FunctionTraits;

    template <class R, class... A>
    struct FunctionTraits<R (*)(A...)>
    {
      using Return = R;
      using Args = std::tuple<A...>;
    };

    template <class Tuple, std::size_t... I>
    inline void FillTypeRefs(std::vector<TypeReference> &out, std::index_sequence<I...>)
    {
      (out.push_back(MakeTypeReference<std::tuple_element_t<I, Tuple>>()), ...);
    }

    inline TypeRecord &GetTypeRecord(TypeBuildAnchor anchor)
    {
      return anchor.module->types[anchor.typeIndex];
    }

    template <class T>
    [[nodiscard]] inline NGINReflectionStatus ReadField(const NGINReflectionInstanceHandle *instance,
                                                        NGINReflectionValue *outValue,
                                                        T *fieldPtr,
                                                        const ModuleIdentity &moduleIdentity)
    {
      if (!instance || !outValue || !fieldPtr)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid field read");
      *outValue = ToAbiValue(moduleIdentity, *fieldPtr);
      return OkStatus();
    }

    template <auto MemberPtr>
    NGINReflectionStatus FieldReadThunk(const NGINReflectionInstanceHandle *instance,
                                        NGINReflectionValue *outValue)
    {
      using Traits = MemberPointerTraits<decltype(MemberPtr)>;
      using Class = typename Traits::Class;

      if (!instance || !outValue || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid field read");
      auto *self = static_cast<const Class *>(instance->vtable->getConst(instance));
      if (!self)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      const auto module = ModuleIdentity::Create(SymbolId{instance->identity.module.moduleNameSymbol},
                                                 instance->identity.module.abiFamily,
                                                 instance->identity.module.abiVersion);
      *outValue = ToAbiValue(module, self->*MemberPtr);
      return OkStatus();
    }

    template <auto MemberPtr>
    NGINReflectionStatus FieldWriteThunk(NGINReflectionInstanceHandle *instance,
                                         const NGINReflectionValue *value)
    {
      using Traits = MemberPointerTraits<decltype(MemberPtr)>;
      using Class = typename Traits::Class;
      using Member = typename Traits::Member;

      if (!instance || !value || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid field write");
      auto *self = static_cast<Class *>(instance->vtable->getMut(instance));
      if (!self)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      auto converted = ConvertValue<Member>(*value);
      if (!converted.has_value())
        return MakeStatus(NGINReflectionStatus_InvalidArgument, converted.error());
      self->*MemberPtr = *converted;
      return OkStatus();
    }

    template <auto Getter>
    NGINReflectionStatus PropertyReadThunk(const NGINReflectionInstanceHandle *instance,
                                           NGINReflectionValue *outValue)
    {
      using Traits = MethodTraits<decltype(Getter)>;
      using Class = typename Traits::Class;

      if (!instance || !outValue || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid property read");
      auto *self = static_cast<const Class *>(instance->vtable->getConst(instance));
      if (!self)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      const auto module = ModuleIdentity::Create(SymbolId{instance->identity.module.moduleNameSymbol},
                                                 instance->identity.module.abiFamily,
                                                 instance->identity.module.abiVersion);
      *outValue = ToAbiValue(module, (self->*Getter)());
      return OkStatus();
    }

    template <auto Setter>
    NGINReflectionStatus PropertyWriteThunk(NGINReflectionInstanceHandle *instance,
                                            const NGINReflectionValue *value)
    {
      using Traits = MethodTraits<decltype(Setter)>;
      using Class = typename Traits::Class;
      using Arg = std::tuple_element_t<0, typename Traits::Args>;

      if (!instance || !value || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid property write");
      auto *self = static_cast<Class *>(instance->vtable->getMut(instance));
      if (!self)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      auto converted = ConvertValue<Arg>(*value);
      if (!converted.has_value())
        return MakeStatus(NGINReflectionStatus_InvalidArgument, converted.error());
      (self->*Setter)(*converted);
      return OkStatus();
    }

    template <auto Getter>
    NGINReflectionStatus PropertyWriteByReferenceThunk(NGINReflectionInstanceHandle *instance,
                                                       const NGINReflectionValue *value)
    {
      using Traits = MethodTraits<decltype(Getter)>;
      using Class = typename Traits::Class;
      using Return = typename Traits::Return;
      using ValueType = std::remove_reference_t<Return>;

      if (!instance || !value || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid property write");
      auto *self = static_cast<Class *>(instance->vtable->getMut(instance));
      if (!self)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing object");
      auto converted = ConvertValue<ValueType>(*value);
      if (!converted.has_value())
        return MakeStatus(NGINReflectionStatus_InvalidArgument, converted.error());
      (self->*Getter)() = *converted;
      return OkStatus();
    }

    template <class Class, class Return, class Tuple, std::size_t... I>
    [[nodiscard]] inline NGINReflectionStatus InvokeMethodChecked(const NGINReflectionInstanceHandle *instance,
                                                                  const NGINReflectionValue *arguments,
                                                                  NGINReflectionValue *outValue,
                                                                  Return (Class::*fn)(std::tuple_element_t<I, Tuple>...),
                                                                  std::index_sequence<I...>)
    {
      if constexpr (std::is_void_v<Return>)
      {
        (static_cast<Class *>(instance->vtable->getMut(instance))->*fn)(*ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I])...);
        if (outValue)
          *outValue = {};
        return OkStatus();
      }
      else
      {
        const auto module = ModuleIdentity::Create(SymbolId{instance->identity.module.moduleNameSymbol},
                                                   instance->identity.module.abiFamily,
                                                   instance->identity.module.abiVersion);
        auto result = (static_cast<Class *>(instance->vtable->getMut(instance))->*fn)(*ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I])...);
        if (outValue)
          *outValue = ToAbiValue(module, std::move(result));
        return OkStatus();
      }
    }

    template <class Class, class Return, class Tuple, std::size_t... I>
    [[nodiscard]] inline NGINReflectionStatus InvokeConstMethodChecked(const NGINReflectionInstanceHandle *instance,
                                                                       const NGINReflectionValue *arguments,
                                                                       NGINReflectionValue *outValue,
                                                                       Return (Class::*fn)(std::tuple_element_t<I, Tuple>...) const,
                                                                       std::index_sequence<I...>)
    {
      if constexpr (std::is_void_v<Return>)
      {
        (static_cast<const Class *>(instance->vtable->getConst(instance))->*fn)(*ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I])...);
        if (outValue)
          *outValue = {};
        return OkStatus();
      }
      else
      {
        const auto module = ModuleIdentity::Create(SymbolId{instance->identity.module.moduleNameSymbol},
                                                   instance->identity.module.abiFamily,
                                                   instance->identity.module.abiVersion);
        auto result = (static_cast<const Class *>(instance->vtable->getConst(instance))->*fn)(*ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I])...);
        if (outValue)
          *outValue = ToAbiValue(module, std::move(result));
        return OkStatus();
      }
    }

    template <auto MethodPtr, class Tuple, std::size_t... I>
    [[nodiscard]] inline bool ValidateArgs(const NGINReflectionValue *arguments,
                                           std::uint64_t argumentCount,
                                           std::index_sequence<I...>)
    {
      if (argumentCount != sizeof...(I))
        return false;
      bool valid = true;
      (((void)(valid = valid && ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I]).has_value())), ...);
      return valid;
    }

    template <auto MethodPtr>
    NGINReflectionStatus MethodInvokeThunk(const NGINReflectionInstanceHandle *instance,
                                           const NGINReflectionValue *arguments,
                                           std::uint64_t argumentCount,
                                           NGINReflectionValue *outValue)
    {
      using Traits = MethodTraits<decltype(MethodPtr)>;
      using Class = typename Traits::Class;
      using Return = typename Traits::Return;
      using ArgsTuple = typename Traits::Args;
      constexpr auto N = std::tuple_size_v<ArgsTuple>;

      if (!instance || !instance->vtable)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "invalid instance");
      if (!ValidateArgs<MethodPtr, ArgsTuple>(arguments, argumentCount, std::make_index_sequence<N>{}))
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "argument mismatch");

      if constexpr (Traits::IsConst)
        return InvokeConstMethodChecked<Class, Return, ArgsTuple>(instance, arguments, outValue, MethodPtr, std::make_index_sequence<N>{});
      else
        return InvokeMethodChecked<Class, Return, ArgsTuple>(instance, arguments, outValue, MethodPtr, std::make_index_sequence<N>{});
    }

    template <class T, class Tuple, std::size_t... I>
    NGINReflectionStatus ConstructorInvokeThunk(const NGINReflectionValue *arguments,
                                                std::uint64_t argumentCount,
                                                NGINReflectionInstanceHandle *outInstance,
                                                std::index_sequence<I...>)
    {
      if (!outInstance)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "missing instance output");
      if (argumentCount != sizeof...(I))
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "constructor arity mismatch");
      bool valid = true;
      (((void)(valid = valid && ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I]).has_value())), ...);
      if (!valid)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "constructor argument mismatch");

      const auto identity = SyntheticTypeIdentity<T>(RuntimeModuleIdentity());
      *outInstance = MakeOwnedHandle<T>(
          T{*ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I])...},
          identity);
      return OkStatus();
    }

    template <class T, class... Args>
    NGINReflectionStatus ConstructorThunk(const NGINReflectionValue *arguments,
                                          std::uint64_t argumentCount,
                                          NGINReflectionInstanceHandle *outInstance)
    {
      return ConstructorInvokeThunk<T, std::tuple<Args...>>(arguments, argumentCount, outInstance, std::index_sequence_for<Args...>{});
    }

    template <class T, class... TBindings>
    void AppendInjectableConstructor(TypeBuildAnchor anchor)
    {
      auto &record = GetTypeRecord(anchor);
      ConstructorRecord ctor{};
      ctor.constructSlot = static_cast<std::uint32_t>(anchor.module->tables.constructors.size());
      anchor.module->tables.constructors.push_back(
          &ConstructorThunk<T, typename ConstructorDependencyTraits<TBindings>::Type...>);
      (ctor.parameters.push_back(MakeTypeReference<typename ConstructorDependencyTraits<TBindings>::Type>()), ...);
      ctor.attributes.push_back(AttributeRecord{InternSymbol(InjectableConstructorAttribute), true});

      std::size_t index = 0;
      ([&]
       {
         using Traits = ConstructorDependencyTraits<TBindings>;
         const auto prefix = std::string(ConstructorParameterPrefix) + std::to_string(index++);
         if (!Traits::Name().empty())
           ctor.attributes.push_back(AttributeRecord{InternSymbol(prefix + ".Name"), std::string(Traits::Name())});
         if constexpr (Traits::optional)
           ctor.attributes.push_back(AttributeRecord{InternSymbol(prefix + ".Optional"), true});
       }(), ...);
      record.constructors.push_back(std::move(ctor));
    }

    template <class Return, class Tuple, std::size_t... I>
    NGINReflectionStatus FunctionInvokeChecked(const ModuleIdentity &moduleIdentity,
                                               const NGINReflectionValue *arguments,
                                               NGINReflectionValue *outValue,
                                               Return (*fn)(std::tuple_element_t<I, Tuple>...),
                                               std::index_sequence<I...>)
    {
      if constexpr (std::is_void_v<Return>)
      {
        fn(*ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I])...);
        if (outValue)
          *outValue = {};
        return OkStatus();
      }
      else
      {
        if (outValue)
          *outValue = ToAbiValue(moduleIdentity, fn(*ConvertValue<std::tuple_element_t<I, Tuple>>(arguments[I])...));
        return OkStatus();
      }
    }

    template <auto FunctionPtr>
    NGINReflectionStatus FunctionInvokeThunk(const NGINReflectionValue *arguments,
                                             std::uint64_t argumentCount,
                                             NGINReflectionValue *outValue)
    {
      using Traits = FunctionTraits<decltype(FunctionPtr)>;
      using Return = typename Traits::Return;
      using ArgsTuple = typename Traits::Args;
      constexpr auto N = std::tuple_size_v<ArgsTuple>;

      if (argumentCount != N)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "function arity mismatch");
      bool valid = true;
      [&]<std::size_t... I>(std::index_sequence<I...>) {
        (((void)(valid = valid && ConvertValue<std::tuple_element_t<I, ArgsTuple>>(arguments[I]).has_value())), ...);
      }(std::make_index_sequence<N>{});
      if (!valid)
        return MakeStatus(NGINReflectionStatus_InvalidArgument, "function argument mismatch");

      return FunctionInvokeChecked<Return, ArgsTuple>(RuntimeModuleIdentity(),
                                                      arguments,
                                                      outValue,
                                                      FunctionPtr,
                                                      std::make_index_sequence<N>{});
    }
  } // namespace detail

  /// @brief Box a typed constructor argument in an ABI-owned reflection value.
  template <class T>
  [[nodiscard]] Value MakeInstanceValue(T value)
  {
    using ValueType = std::remove_cvref_t<T>;
    auto handle = detail::MakeOwnedHandle<ValueType>(
        std::move(value), detail::SyntheticTypeIdentity<ValueType>(detail::RuntimeModuleIdentity()));
    return Value::FromInstance(AdoptInstance(handle));
  }

  template <class T>
  class TypeBuilder
  {
  public:
    explicit TypeBuilder(detail::TypeBuildAnchor anchor) noexcept
        : m_anchor(anchor)
    {
    }

    TypeBuilder &SetName(std::string_view qualifiedName)
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      record.qualifiedName = detail::InternSymbol(qualifiedName);
      return *this;
    }

    template <auto MemberPtr>
    TypeBuilder &Field(std::string_view name)
    {
      using Traits = detail::MemberPointerTraits<decltype(MemberPtr)>;
      using Member = typename Traits::Member;

      auto &record = detail::GetTypeRecord(m_anchor);
      detail::FieldRecord field{};
      field.name = detail::InternSymbol(name);
      field.valueType = detail::MakeTypeReference<Member>();
      field.readSlot = static_cast<std::uint32_t>(m_anchor.module->tables.fieldReaders.size());
      m_anchor.module->tables.fieldReaders.push_back(&detail::FieldReadThunk<MemberPtr>);
      field.writeSlot = static_cast<std::uint32_t>(m_anchor.module->tables.fieldWriters.size());
      m_anchor.module->tables.fieldWriters.push_back(&detail::FieldWriteThunk<MemberPtr>);
      record.fields.push_back(std::move(field));
      return *this;
    }

    template <auto Getter>
    TypeBuilder &Property(std::string_view name)
    {
      using Traits = detail::MethodTraits<decltype(Getter)>;
      using Return = typename Traits::Return;
      using ValueType = std::remove_reference_t<Return>;

      auto &record = detail::GetTypeRecord(m_anchor);
      detail::PropertyRecord property{};
      property.name = detail::InternSymbol(name);
      property.valueType = detail::MakeTypeReference<ValueType>();
      property.readSlot = static_cast<std::uint32_t>(m_anchor.module->tables.propertyReaders.size());
      m_anchor.module->tables.propertyReaders.push_back(&detail::PropertyReadThunk<Getter>);
      property.writeSlot = static_cast<std::uint32_t>(m_anchor.module->tables.propertyWriters.size());
      if constexpr (std::is_lvalue_reference_v<Return> && !std::is_const_v<ValueType>)
        m_anchor.module->tables.propertyWriters.push_back(&detail::PropertyWriteByReferenceThunk<Getter>);
      else
        m_anchor.module->tables.propertyWriters.push_back(nullptr);
      record.properties.push_back(std::move(property));
      return *this;
    }

    template <auto Getter, auto Setter>
    TypeBuilder &Property(std::string_view name)
    {
      using GetTraits = detail::MethodTraits<decltype(Getter)>;
      using Return = typename GetTraits::Return;
      using ValueType = std::remove_reference_t<Return>;

      auto &record = detail::GetTypeRecord(m_anchor);
      detail::PropertyRecord property{};
      property.name = detail::InternSymbol(name);
      property.valueType = detail::MakeTypeReference<ValueType>();
      property.readSlot = static_cast<std::uint32_t>(m_anchor.module->tables.propertyReaders.size());
      m_anchor.module->tables.propertyReaders.push_back(&detail::PropertyReadThunk<Getter>);
      property.writeSlot = static_cast<std::uint32_t>(m_anchor.module->tables.propertyWriters.size());
      m_anchor.module->tables.propertyWriters.push_back(&detail::PropertyWriteThunk<Setter>);
      record.properties.push_back(std::move(property));
      return *this;
    }

    template <auto MethodPtr>
    TypeBuilder &Method(std::string_view name)
    {
      using Traits = detail::MethodTraits<decltype(MethodPtr)>;
      using Return = typename Traits::Return;
      using ArgsTuple = typename Traits::Args;

      auto &record = detail::GetTypeRecord(m_anchor);
      detail::MethodRecord method{};
      method.name = detail::InternSymbol(name);
      if constexpr (!std::is_void_v<Return>)
        method.returnType = detail::MakeTypeReference<Return>();
      method.invokeSlot = static_cast<std::uint32_t>(m_anchor.module->tables.methodInvokers.size());
      method.isConst = Traits::IsConst;
      m_anchor.module->tables.methodInvokers.push_back(&detail::MethodInvokeThunk<MethodPtr>);
      detail::FillTypeRefs<ArgsTuple>(method.parameters,
                                      std::make_index_sequence<std::tuple_size_v<ArgsTuple>>{});
      record.methods.push_back(std::move(method));
      return *this;
    }

    template <class... Args>
    TypeBuilder &Constructor()
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      detail::ConstructorRecord ctor{};
      ctor.constructSlot = static_cast<std::uint32_t>(m_anchor.module->tables.constructors.size());
      m_anchor.module->tables.constructors.push_back(&detail::ConstructorThunk<T, Args...>);
      (ctor.parameters.push_back(detail::MakeTypeReference<Args>()), ...);
      record.constructors.push_back(std::move(ctor));
      return *this;
    }

    /// @brief Register the one constructor intended for dependency injection.
    template <class... TBindings>
    TypeBuilder &InjectableConstructor()
    {
      detail::AppendInjectableConstructor<T, TBindings...>(m_anchor);
      return *this;
    }

    TypeBuilder &EnumValue(std::string_view name, T value)
    {
      static_assert(std::is_enum_v<T>, "EnumValue requires enum T");
      auto &record = detail::GetTypeRecord(m_anchor);
      record.isEnum = true;
      record.enumSigned = std::is_signed_v<std::underlying_type_t<T>>;
      record.enumUnderlyingType = detail::MakeTypeReference<std::underlying_type_t<T>>();
      record.enumValues.push_back(detail::EnumRecord{
          detail::InternSymbol(name),
          static_cast<std::int64_t>(static_cast<std::underlying_type_t<T>>(value)),
          static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<std::underlying_type_t<T>>>(static_cast<std::underlying_type_t<T>>(value)))});
      return *this;
    }

    template <class BaseT>
    TypeBuilder &Base()
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      detail::BaseRecord base{};
      base.baseName = detail::InternSymbol(NGIN::Meta::TypeName<BaseT>::qualifiedName);
      base.upcastSlot = static_cast<std::uint32_t>(m_anchor.module->tables.upcasters.size());
      m_anchor.module->tables.upcasters.push_back(&detail::UpcastThunk<BaseT, T>);
      base.downcastSlot = static_cast<std::uint32_t>(m_anchor.module->tables.downcasters.size());
      m_anchor.module->tables.downcasters.push_back(&detail::DowncastThunk<T, BaseT>);
      record.bases.push_back(std::move(base));
      return *this;
    }

    template <class BaseT, auto Downcast>
    TypeBuilder &Base()
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      detail::BaseRecord base{};
      base.baseName = detail::InternSymbol(NGIN::Meta::TypeName<BaseT>::qualifiedName);
      base.upcastSlot = static_cast<std::uint32_t>(m_anchor.module->tables.upcasters.size());
      m_anchor.module->tables.upcasters.push_back(&detail::UpcastThunk<BaseT, T>);
      base.downcastSlot = static_cast<std::uint32_t>(m_anchor.module->tables.downcasters.size());
      m_anchor.module->tables.downcasters.push_back(&detail::CustomDowncastThunk<T, BaseT, Downcast>);
      record.bases.push_back(std::move(base));
      return *this;
    }

    TypeBuilder &Attribute(std::string_view key, const AttributeValue &value)
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      record.attributes.push_back(detail::AttributeRecord{detail::InternSymbol(key), value});
      return *this;
    }

    template <auto MemberPtr>
    TypeBuilder &FieldAttribute(std::string_view key, const AttributeValue &value)
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      if (!record.fields.empty())
        record.fields.back().attributes.push_back(detail::AttributeRecord{detail::InternSymbol(key), value});
      return *this;
    }

    template <auto Getter>
    TypeBuilder &PropertyAttribute(std::string_view key, const AttributeValue &value)
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      if (!record.properties.empty())
        record.properties.back().attributes.push_back(detail::AttributeRecord{detail::InternSymbol(key), value});
      return *this;
    }

    template <auto MethodPtr>
    TypeBuilder &MethodAttribute(std::string_view key, const AttributeValue &value)
    {
      auto &record = detail::GetTypeRecord(m_anchor);
      if (!record.methods.empty())
        record.methods.back().attributes.push_back(detail::AttributeRecord{detail::InternSymbol(key), value});
      return *this;
    }

    constexpr void Build() const noexcept {}

  private:
    detail::TypeBuildAnchor m_anchor{};
  };
} // namespace NGIN::Reflection
