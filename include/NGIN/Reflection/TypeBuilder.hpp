// TypeBuilder.hpp
// Public TypeBuilder<T> used inside ADL friend to describe fields (Phase 1)
#pragma once

#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/NameUtils.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Meta/TypeTraits.hpp>
#include <NGIN/Reflection/Convert.hpp>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace NGIN::Reflection
{

  namespace detail
  {
    template <auto Getter>
    Any PropertyGet(const void *obj);
  }

  template <class T>
  class TypeBuilder
  {
  public:
    // Note: constructed by the registry when invoking ADL reflect; binds to a specific type index.
    explicit TypeBuilder(NGIN::UInt32 typeIndex) : m_index(typeIndex) {}

    // Optional name overrides (qualified or unqualified). If not set, defaults to Meta::TypeName<T>.
    TypeBuilder &set_name(std::string_view qualified)
    {
      auto &reg = detail::GetRegistry();
      auto id = detail::InternNameId(qualified);
      reg.types[m_index].qualifiedNameId = id;
      reg.types[m_index].qualifiedName = detail::NameFromId(id);
      // Update name index as well
      reg.byName.Insert(id, m_index);
      return *this;
    }

    // Add a public data member as a field; name optional and auto-derived if omitted.
    template <auto MemberPtr>
    TypeBuilder &field(std::string_view name = {})
    {
      using MemberT = detail::MemberTypeT<MemberPtr>;
      auto &reg = detail::GetRegistry();
      detail::FieldRuntimeDesc f{};
      {
        auto svName = name.empty() ? detail::MemberNameFromPretty<MemberPtr>() : name;
        auto id = detail::InternNameId(svName);
        f.nameId = id;
        f.name = detail::NameFromId(id);
      }
      {
        auto sv = NGIN::Meta::TypeName<MemberT>::qualifiedName;
        f.typeId = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
      }
      f.sizeBytes = sizeof(MemberT);
      f.GetMut = &detail::FieldGetterMut<MemberPtr>;
      f.GetConst = &detail::FieldGetterConst<MemberPtr>;
      f.load = &detail::FieldLoad<MemberPtr>;
      f.store = &detail::FieldStore<MemberPtr>;
      reg.types[m_index].fields.PushBack(std::move(f));
      // update field index map
      const auto newIdx = static_cast<NGIN::UInt32>(reg.types[m_index].fields.Size() - 1);
      reg.types[m_index].fieldIndex.Insert(reg.types[m_index].fields[newIdx].nameId, newIdx);
      return *this;
    }

    // Add a const/non-const member method. Name required.
    template <auto MemFn>
    TypeBuilder &method(std::string_view name);

    // Add a property using getter and optional setter.
    template <auto Getter>
    TypeBuilder &property(std::string_view name);

    template <auto Getter, auto Setter>
    TypeBuilder &property(std::string_view name);

    // Add a constructor descriptor for T with parameter types A...
    template <class... A>
    TypeBuilder &constructor();

    // Attach a typed attribute (type-level)
    TypeBuilder &attribute(std::string_view key, const AttrValue &value)
    {
      auto &reg = detail::GetRegistry();
      reg.types[m_index].attributes.PushBack(AttributeDesc{key, value});
      return *this;
    }

    // Attach attribute to a specific field by member pointer.
    template <auto MemberPtr>
    TypeBuilder &field_attribute(std::string_view key, const AttrValue &value)
    {
      auto &reg = detail::GetRegistry();
      auto *fn = &detail::FieldGetterMut<MemberPtr>;
      auto &fields = reg.types[m_index].fields;
      for (auto i = NGIN::UIntSize{0}; i < fields.Size(); ++i)
      {
        if (fields[i].GetMut == reinterpret_cast<void *(*)(void *)>(fn))
        {
          fields[i].attributes.PushBack(AttributeDesc{key, value});
          break;
        }
      }
      return *this;
    }

    // Attach attribute to a specific property by getter.
    template <auto Getter>
    TypeBuilder &property_attribute(std::string_view key, const AttrValue &value)
    {
      auto &reg = detail::GetRegistry();
      auto *fn = &detail::PropertyGet<Getter>;
      auto &props = reg.types[m_index].properties;
      for (auto i = NGIN::UIntSize{0}; i < props.Size(); ++i)
      {
        if (props[i].get == fn)
        {
          props[i].attributes.PushBack(AttributeDesc{key, value});
          break;
        }
      }
      return *this;
    }

    // Attach attribute to a specific method by member function pointer.
    template <auto MemFn>
    TypeBuilder &method_attribute(std::string_view key, const AttrValue &value);

    // No-op in Phase 1; present for API symmetry.
    constexpr void build() const noexcept {}

  private:
    NGIN::UInt32 m_index{0};
  };

  // ==== Method registration machinery ====
  namespace detail
  {
    template <typename>
    struct MethodTraits;

    using NGIN::Reflection::detail::ConvertAny; // reuse shared conversion

    template <typename>
    struct GetterTraits;

    template <class C, class R>
    struct GetterTraits<R (C::*)()>
    {
      using Class = C;
      using Ret = R;
      static constexpr bool IsConst = false;
      static constexpr bool IsMember = true;
    };

    template <class C, class R>
    struct GetterTraits<R (C::*)() const>
    {
      using Class = C;
      using Ret = R;
      static constexpr bool IsConst = true;
      static constexpr bool IsMember = true;
    };

    template <class C, class R>
    struct GetterTraits<R (*)(C &)>
    {
      using Class = C;
      using Ret = R;
      static constexpr bool IsConst = false;
      static constexpr bool IsMember = false;
    };

    template <class C, class R>
    struct GetterTraits<R (*)(const C &)>
    {
      using Class = C;
      using Ret = R;
      static constexpr bool IsConst = true;
      static constexpr bool IsMember = false;
    };

    template <typename>
    struct SetterTraits;

    template <class C, class A>
    struct SetterTraits<void (C::*)(A)>
    {
      using Class = C;
      using Arg = A;
      static constexpr bool IsMember = true;
    };

    template <class C, class A>
    struct SetterTraits<void (C::*)(A) const>
    {
      using Class = C;
      using Arg = A;
      static constexpr bool IsMember = true;
    };

    template <class C, class A>
    struct SetterTraits<void (*)(C &, A)>
    {
      using Class = C;
      using Arg = A;
      static constexpr bool IsMember = false;
    };

    template <auto Getter>
    static Any PropertyGet(const void *obj)
    {
      using Traits = GetterTraits<decltype(Getter)>;
      using C = typename Traits::Class;
      if constexpr (Traits::IsMember)
      {
        if constexpr (Traits::IsConst)
        {
          auto *c = static_cast<const C *>(obj);
          return Any{(c->*Getter)()};
        }
        else
        {
          auto *c = const_cast<C *>(static_cast<const C *>(obj));
          return Any{(c->*Getter)()};
        }
      }
      else
      {
        if constexpr (Traits::IsConst)
        {
          const auto &c = *static_cast<const C *>(obj);
          return Any{Getter(c)};
        }
        else
        {
          auto &c = *const_cast<C *>(static_cast<const C *>(obj));
          return Any{Getter(c)};
        }
      }
    }

    template <auto Setter>
    static std::expected<void, Error> PropertySet(void *obj, const Any &value)
    {
      using Traits = SetterTraits<decltype(Setter)>;
      using C = typename Traits::Class;
      using Arg = std::remove_cv_t<std::remove_reference_t<typename Traits::Arg>>;
      auto conv = ConvertAny<Arg>(value);
      if (!conv.has_value())
        return std::unexpected(Error{ErrorCode::InvalidArgument, "argument conversion failed"});
      if constexpr (Traits::IsMember)
      {
        auto *c = static_cast<C *>(obj);
        (c->*Setter)(conv.value());
      }
      else
      {
        auto &c = *static_cast<C *>(obj);
        Setter(c, conv.value());
      }
      return {};
    }

    template <auto Getter>
    static std::expected<void, Error> PropertySetFromGetter(void *obj, const Any &value)
    {
      using Traits = GetterTraits<decltype(Getter)>;
      using Ret = typename Traits::Ret;
      using Arg = std::remove_cv_t<std::remove_reference_t<Ret>>;
      static_assert(std::is_lvalue_reference_v<Ret> && !std::is_const_v<std::remove_reference_t<Ret>>,
                    "Getter must return non-const lvalue reference to enable implicit setter");
      auto conv = ConvertAny<Arg>(value);
      if (!conv.has_value())
        return std::unexpected(Error{ErrorCode::InvalidArgument, "argument conversion failed"});
      if constexpr (Traits::IsMember)
      {
        auto *c = static_cast<typename Traits::Class *>(obj);
        (c->*Getter)() = conv.value();
      }
      else
      {
        auto &c = *static_cast<typename Traits::Class *>(obj);
        Getter(c) = conv.value();
      }
      return {};
    }

    template <std::size_t I, class Tuple>
    inline NGIN::UInt64 param_type_id()
    {
      using Arg = std::remove_cv_t<std::remove_reference_t<std::tuple_element_t<I, Tuple>>>;
      auto psv = NGIN::Meta::TypeName<Arg>::qualifiedName;
      return NGIN::Hashing::FNV1a64(psv.data(), psv.size());
    }

    template <class Tuple, std::size_t... I>
    inline void push_param_ids(MethodRuntimeDesc &m, std::index_sequence<I...>)
    {
      (m.paramTypeIds.PushBack(param_type_id<I, Tuple>()), ...);
    }

    template <class Tuple, std::size_t... I>
    inline void push_ctor_param_ids(NGIN::Containers::Vector<NGIN::UInt64> &v, std::index_sequence<I...>)
    {
      (v.PushBack(param_type_id<I, Tuple>()), ...);
    }

    template <class C, class R, class... A>
    struct MethodTraits<R (C::*)(A...)>
    {
      using Class = C;
      using Ret = R;
      static constexpr bool IsConst = false;
      static constexpr NGIN::UIntSize Arity = sizeof...(A);
      using Args = std::tuple<A...>;
      template <auto MemFn>
      static std::expected<Any, Error> Invoke(void *obj, const Any *args, NGIN::UIntSize count)
      {
        if (count != Arity)
          return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
        auto *c = static_cast<C *>(obj);
        return call<MemFn>(c, args, std::index_sequence_for<A...>{});
      }

    private:
      template <auto MemFn, std::size_t... I>
      static std::expected<Any, Error> call(C *c, const Any *args, std::index_sequence<I...>)
      {
        if (((ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).has_value()) && ...))
        {
          if constexpr (std::is_void_v<R>)
          {
            (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
            return Any::MakeVoid();
          }
          else
          {
            auto r = (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
            return Any{std::move(r)};
          }
        }
        return std::unexpected(Error{ErrorCode::InvalidArgument, "argument conversion failed"});
      }
    };

    template <class C, class R, class... A>
    struct MethodTraits<R (C::*)(A...) const>
    {
      using Class = C;
      using Ret = R;
      static constexpr bool IsConst = true;
      static constexpr NGIN::UIntSize Arity = sizeof...(A);
      using Args = std::tuple<A...>;
      template <auto MemFn>
      static std::expected<Any, Error> Invoke(void *obj, const Any *args, NGIN::UIntSize count)
      {
        if (count != Arity)
          return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
        auto *c = static_cast<const C *>(obj);
        return call<MemFn>(c, args, std::index_sequence_for<A...>{});
      }

    private:
      template <auto MemFn, std::size_t... I>
      static std::expected<Any, Error> call(const C *c, const Any *args, std::index_sequence<I...>)
      {
        if (((ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).has_value()) && ...))
        {
          if constexpr (std::is_void_v<R>)
          {
            (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
            return Any::MakeVoid();
          }
          else
          {
            auto r = (c->*MemFn)(ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[I]).value()...);
            return Any{std::move(r)};
          }
        }
        return std::unexpected(Error{ErrorCode::InvalidArgument, "argument conversion failed"});
      }
    };
  } // namespace detail

  template <class T>
  template <auto MemFn>
  inline TypeBuilder<T> &TypeBuilder<T>::method(std::string_view name)
  {
    using Traits = detail::MethodTraits<decltype(MemFn)>;
    static_assert(std::is_same_v<typename Traits::Class, T>, "Method must belong to T");
    auto &reg = detail::GetRegistry();
    detail::MethodRuntimeDesc m{};
    auto nameId = detail::InternNameId(name);
    m.name = detail::NameFromId(nameId);
    m.nameId = nameId;
    // Return type id
    if constexpr (std::is_void_v<typename Traits::Ret>)
    {
      m.returnTypeId = 0;
    }
    else
    {
      auto rsv = NGIN::Meta::TypeName<typename Traits::Ret>::qualifiedName;
      m.returnTypeId = NGIN::Hashing::FNV1a64(rsv.data(), rsv.size());
    }
    // Param type ids
    constexpr auto N = Traits::Arity;
    if constexpr (N > 0)
    {
      using Tuple = typename Traits::Args;
      detail::push_param_ids<Tuple>(m, std::make_index_sequence<N>{});
    }
    // Invoker
    m.Invoke = &Traits::template Invoke<MemFn>;
    reg.types[m_index].methods.PushBack(std::move(m));
    // Add to overload set map
    auto &tdesc = reg.types[m_index];
    const auto newIndex = static_cast<NGIN::UInt32>(tdesc.methods.Size() - 1);
    auto *vecPtr = tdesc.methodOverloads.GetPtr(tdesc.methods[newIndex].nameId);
    if (!vecPtr)
    {
      NGIN::Containers::Vector<NGIN::UInt32> v;
      v.PushBack(newIndex);
      tdesc.methodOverloads.Insert(tdesc.methods[newIndex].nameId, std::move(v));
    }
    else
    {
      vecPtr->PushBack(newIndex);
    }
    return *this;
  }

  // ==== Property registration ====
  template <class T>
  template <auto Getter>
  inline TypeBuilder<T> &TypeBuilder<T>::property(std::string_view name)
  {
    using Traits = detail::GetterTraits<decltype(Getter)>;
    static_assert(std::is_same_v<typename Traits::Class, T>, "Property getter must belong to T");
    auto &reg = detail::GetRegistry();
    detail::PropertyRuntimeDesc p{};
    auto nameId = detail::InternNameId(name);
    p.nameId = nameId;
    p.name = detail::NameFromId(nameId);
    using Ret = typename Traits::Ret;
    using Value = std::remove_cv_t<std::remove_reference_t<Ret>>;
    auto sv = NGIN::Meta::TypeName<Value>::qualifiedName;
    p.typeId = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
    p.get = &detail::PropertyGet<Getter>;
    if constexpr (std::is_lvalue_reference_v<Ret> && !std::is_const_v<std::remove_reference_t<Ret>>)
    {
      p.set = &detail::PropertySetFromGetter<Getter>;
    }
    reg.types[m_index].properties.PushBack(std::move(p));
    const auto newIdx = static_cast<NGIN::UInt32>(reg.types[m_index].properties.Size() - 1);
    reg.types[m_index].propertyIndex.Insert(reg.types[m_index].properties[newIdx].nameId, newIdx);
    return *this;
  }

  template <class T>
  template <auto Getter, auto Setter>
  inline TypeBuilder<T> &TypeBuilder<T>::property(std::string_view name)
  {
    using GetTraits = detail::GetterTraits<decltype(Getter)>;
    using SetTraits = detail::SetterTraits<decltype(Setter)>;
    static_assert(std::is_same_v<typename GetTraits::Class, T>, "Property getter must belong to T");
    static_assert(std::is_same_v<typename SetTraits::Class, T>, "Property setter must belong to T");
    auto &reg = detail::GetRegistry();
    detail::PropertyRuntimeDesc p{};
    auto nameId = detail::InternNameId(name);
    p.nameId = nameId;
    p.name = detail::NameFromId(nameId);
    using Ret = typename GetTraits::Ret;
    using Value = std::remove_cv_t<std::remove_reference_t<Ret>>;
    auto sv = NGIN::Meta::TypeName<Value>::qualifiedName;
    p.typeId = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
    p.get = &detail::PropertyGet<Getter>;
    p.set = &detail::PropertySet<Setter>;
    reg.types[m_index].properties.PushBack(std::move(p));
    const auto newIdx = static_cast<NGIN::UInt32>(reg.types[m_index].properties.Size() - 1);
    reg.types[m_index].propertyIndex.Insert(reg.types[m_index].properties[newIdx].nameId, newIdx);
    return *this;
  }

  // ==== Constructor registration ====
  template <class T>
  template <class... A>
  inline TypeBuilder<T> &TypeBuilder<T>::constructor()
  {
    auto &reg = detail::GetRegistry();
    detail::CtorRuntimeDesc c{};
    if constexpr (sizeof...(A) > 0)
    {
      using Tuple = std::tuple<A...>;
      detail::push_ctor_param_ids<Tuple>(c.paramTypeIds, std::make_index_sequence<sizeof...(A)>{});
    }
    c.construct = [](const Any *args, NGIN::UIntSize count) -> std::expected<Any, Error>
    {
      if (count != sizeof...(A))
        return std::unexpected(Error{ErrorCode::InvalidArgument, "bad arity"});
      // Convert then construct
      auto convert_and_make = [&](auto &&...unpacked) -> std::expected<Any, Error>
      {
        if (((detail::ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[unpacked]).has_value()) && ...))
        {
          if constexpr (sizeof...(A) == 0)
          {
            return Any{T{}};
          }
          else
          {
            T obj{detail::ConvertAny<std::remove_cv_t<std::remove_reference_t<A>>>(args[unpacked]).value()...};
            return Any{std::move(obj)};
          }
        }
        return std::unexpected(Error{ErrorCode::InvalidArgument, "argument conversion failed"});
      };
      return [&]<std::size_t... I>(std::index_sequence<I...>)
      {
        return convert_and_make(I...);
      }(std::make_index_sequence<sizeof...(A)>{});
    };
    reg.types[m_index].constructors.PushBack(std::move(c));
    return *this;
  }

  // Implement method_attribute after MethodTraits are defined
  template <class T>
  template <auto MemFn>
  inline TypeBuilder<T> &TypeBuilder<T>::method_attribute(std::string_view key, const AttrValue &value)
  {
    using Traits = detail::MethodTraits<decltype(MemFn)>;
    auto &reg = detail::GetRegistry();
    auto inv = &Traits::template Invoke<MemFn>;
    auto &methods = reg.types[m_index].methods;
    for (auto i = NGIN::UIntSize{0}; i < methods.Size(); ++i)
    {
      if (methods[i].Invoke == inv)
      {
        methods[i].attributes.PushBack(AttributeDesc{key, value});
        break;
      }
    }
    return *this;
  }

  // Removed obsolete add_param_ids helper

} // namespace NGIN::Reflection
