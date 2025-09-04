// Builder.hpp
// Public Builder<T> used inside ADL friend to describe fields (Phase 1)
#pragma once

#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/NameUtils.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Meta/TypeTraits.hpp>
#include <NGIN/Reflection/Any.hpp>
#include <string_view>

namespace NGIN::Reflection {

template<class T>
class Builder {
public:
  // Note: constructed by the registry when invoking ADL reflect; binds to a specific type index.
  explicit Builder(NGIN::UInt32 typeIndex) : m_index(typeIndex) {}

  // Optional name overrides (qualified or unqualified). If not set, defaults to Meta::TypeName<T>.
  Builder& set_name(std::string_view qualified) {
    auto& reg = detail::GetRegistry();
    reg.types[m_index].qualifiedName = detail::InternName(qualified);
    // Update name index as well
    reg.byName.Insert(reg.types[m_index].qualifiedName, m_index);
    return *this;
  }

  // Add a public data member as a field; name optional and auto-derived if omitted.
  template<auto MemberPtr>
  Builder& field(std::string_view name = {}) {
    using MemberT = detail::MemberTypeT<MemberPtr>;
    auto& reg = detail::GetRegistry();
    detail::FieldRuntimeDesc f{};
    f.name       = detail::InternName(name.empty() ? detail::MemberNameFromPretty<MemberPtr>() : name);
    {
      auto sv    = NGIN::Meta::TypeName<MemberT>::qualifiedName;
      f.typeId   = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
    }
    f.sizeBytes  = sizeof(MemberT);
    f.get_mut    = &detail::FieldGetterMut<MemberPtr>;
    f.get_const  = &detail::FieldGetterConst<MemberPtr>;
    f.load       = &detail::FieldLoad<MemberPtr>;
    f.store      = &detail::FieldStore<MemberPtr>;
    reg.types[m_index].fields.PushBack(std::move(f));
    return *this;
  }

  // Add a const/non-const member method. Name required.
  template<auto MemFn>
  Builder& method(std::string_view name);

  // Attach a typed attribute (type-level)
  Builder& attribute(std::string_view key, const AttrValue& value) {
    auto& reg = detail::GetRegistry();
    reg.types[m_index].attributes.PushBack(AttributeDesc{key, value});
    return *this;
  }

  // Attach attribute to a specific field by member pointer.
  template<auto MemberPtr>
  Builder& field_attribute(std::string_view key, const AttrValue& value) {
    auto& reg = detail::GetRegistry();
    auto* fn = &detail::FieldGetterMut<MemberPtr>;
    auto& fields = reg.types[m_index].fields;
    for (auto i = NGIN::UIntSize{0}; i < fields.Size(); ++i) {
      if (fields[i].get_mut == reinterpret_cast<void*(*)(void*)>(fn)) {
        fields[i].attributes.PushBack(AttributeDesc{key, value});
        break;
      }
    }
    return *this;
  }

  // Attach attribute to a specific method by member function pointer.
  template<auto MemFn>
  Builder& method_attribute(std::string_view key, const AttrValue& value);

  // No-op in Phase 1; present for API symmetry.
  constexpr void build() const noexcept {}

private:
  NGIN::UInt32 m_index {0};
};

// ==== Method registration machinery ====
namespace detail {
  template<typename> struct MethodTraits;
  
  template<class T>
  inline constexpr bool is_numeric_v = std::is_arithmetic_v<std::remove_cv_t<std::remove_reference_t<T>>>;

  // Try to convert Any -> To (supports exact match, arithmetic conversions)
  template<class To>
  inline std::expected<std::remove_cv_t<std::remove_reference_t<To>>, Error>
  ConvertAny(const class Any& src)
  {
    using Dest = std::remove_cv_t<std::remove_reference_t<To>>;
    const auto tid = src.type_id();
    if (tid == TypeIdOf<Dest>()) {
      return src.template as<Dest>();
    }
    if constexpr (is_numeric_v<Dest>) {
      if (tid == TypeIdOf<bool>())   return static_cast<Dest>(src.template as<bool>());
      if (tid == TypeIdOf<int>())    return static_cast<Dest>(src.template as<int>());
      if (tid == TypeIdOf<unsigned int>()) return static_cast<Dest>(src.template as<unsigned int>());
      if (tid == TypeIdOf<long long>())    return static_cast<Dest>(src.template as<long long>());
      if (tid == TypeIdOf<unsigned long long>()) return static_cast<Dest>(src.template as<unsigned long long>());
      if (tid == TypeIdOf<float>())  return static_cast<Dest>(src.template as<float>());
      if (tid == TypeIdOf<double>()) return static_cast<Dest>(src.template as<double>());
    }
    return std::unexpected(Error{ErrorCode::InvalidArgument, "argument type not convertible"});
  }
  
  template<std::size_t I, class Tuple>
  inline NGIN::UInt64 param_type_id() {
    using Arg = std::remove_cv_t<std::remove_reference_t<std::tuple_element_t<I, Tuple>>>;
    auto psv = NGIN::Meta::TypeName<Arg>::qualifiedName;
    return NGIN::Hashing::FNV1a64(psv.data(), psv.size());
  }

  template<class Tuple, std::size_t... I>
  inline void push_param_ids(MethodRuntimeDesc& m, std::index_sequence<I...>) {
    (m.paramTypeIds.PushBack(param_type_id<I, Tuple>()), ...);
  }

  template<class C, class R, class... A>
  struct MethodTraits<R(C::*)(A...)> {
    using Class = C; using Ret = R; static constexpr bool IsConst = false;
    static constexpr NGIN::UIntSize Arity = sizeof...(A);
    using Args = std::tuple<A...>;
    template<auto MemFn>
    static std::expected<class Any, Error> Invoke(void* obj, const class Any* args, NGIN::UIntSize count) {
      if (count != Arity) return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
      auto* c = static_cast<C*>(obj);
      return call<MemFn>(c, args, std::index_sequence_for<A...>{});
    }
  private:
    template<auto MemFn, std::size_t... I>
    static std::expected<class Any, Error> call(C* c, const class Any* args, std::index_sequence<I...>) {
      if (((ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).has_value()) && ...)) {
        if constexpr (std::is_void_v<R>) {
          (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
          return Any::make_void();
        } else {
          auto r = (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
          return Any::make(std::move(r));
        }
      }
      return std::unexpected(Error{ErrorCode::InvalidArgument, "argument conversion failed"});
    }
  };

  template<class C, class R, class... A>
  struct MethodTraits<R(C::*)(A...) const> {
    using Class = C; using Ret = R; static constexpr bool IsConst = true;
    static constexpr NGIN::UIntSize Arity = sizeof...(A);
    using Args = std::tuple<A...>;
    template<auto MemFn>
    static std::expected<class Any, Error> Invoke(void* obj, const class Any* args, NGIN::UIntSize count) {
      if (count != Arity) return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
      auto* c = static_cast<const C*>(obj);
      return call<MemFn>(c, args, std::index_sequence_for<A...>{});
    }
  private:
    template<auto MemFn, std::size_t... I>
    static std::expected<class Any, Error> call(const C* c, const class Any* args, std::index_sequence<I...>) {
      if (((ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).has_value()) && ...)) {
        if constexpr (std::is_void_v<R>) {
          (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
          return Any::make_void();
        } else {
          auto r = (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
          return Any::make(std::move(r));
        }
      }
      return std::unexpected(Error{ErrorCode::InvalidArgument, "argument conversion failed"});
    }
  };
} // namespace detail

template<class T>
template<auto MemFn>
inline Builder<T>& Builder<T>::method(std::string_view name) {
  using Traits = detail::MethodTraits<decltype(MemFn)>;
  static_assert(std::is_same_v<typename Traits::Class, T>, "Method must belong to T");
  auto& reg = detail::GetRegistry();
  detail::MethodRuntimeDesc m{};
  m.name = detail::InternName(name);
  // Return type id
  if constexpr (std::is_void_v<typename Traits::Ret>) {
    m.returnTypeId = 0;
  } else {
    auto rsv = NGIN::Meta::TypeName<typename Traits::Ret>::qualifiedName;
    m.returnTypeId = NGIN::Hashing::FNV1a64(rsv.data(), rsv.size());
  }
  // Param type ids
  constexpr auto N = Traits::Arity;
  if constexpr (N > 0) {
    using Tuple = typename Traits::Args;
    detail::push_param_ids<Tuple>(m, std::make_index_sequence<N>{});
  }
  // Invoker
  m.invoke = &Traits::template Invoke<MemFn>;
  reg.types[m_index].methods.PushBack(std::move(m));
  // Add to overload set map
  auto& tdesc = reg.types[m_index];
  const auto newIndex = static_cast<NGIN::UInt32>(tdesc.methods.Size() - 1);
  auto* vecPtr = tdesc.methodOverloads.GetPtr(tdesc.methods[newIndex].name);
  if (!vecPtr) {
    NGIN::Containers::Vector<NGIN::UInt32> v; v.PushBack(newIndex);
    tdesc.methodOverloads.Insert(tdesc.methods[newIndex].name, std::move(v));
  } else {
    vecPtr->PushBack(newIndex);
  }
  return *this;
}

// Implement method_attribute after MethodTraits are defined
template<class T>
template<auto MemFn>
inline Builder<T>& Builder<T>::method_attribute(std::string_view key, const AttrValue& value) {
  using Traits = detail::MethodTraits<decltype(MemFn)>;
  auto& reg = detail::GetRegistry();
  auto inv = &Traits::template Invoke<MemFn>;
  auto& methods = reg.types[m_index].methods;
  for (auto i = NGIN::UIntSize{0}; i < methods.Size(); ++i) {
    if (methods[i].invoke == inv) {
      methods[i].attributes.PushBack(AttributeDesc{key, value});
      break;
    }
  }
  return *this;
}

// Removed obsolete add_param_ids helper


} // namespace NGIN::Reflection
