// Adapters.hpp - Sequence/tuple/variant/optional/map adapters
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Utilities/Any.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Containers/HashMap.hpp>

#include <type_traits>
#include <tuple>
#include <variant>
#include <vector>
#include <optional>
#include <map>
#include <unordered_map>
#include <expected>
#include <concepts>

#include <NGIN/Reflection/Convert.hpp>

namespace NGIN::Reflection::Adapters
{

using Any = NGIN::Utilities::Any<>;
using AnyView = Any::View;
using ConstAnyView = Any::ConstView;

  // Sequence detection (std::vector, NGIN::Containers::Vector)
  template <class T>
  struct is_sequence : std::false_type
  {
  };

  template <class T, class A>
  struct is_sequence<std::vector<T, A>> : std::true_type
  {
  };

  template <class T, class Alloc>
  struct is_sequence<NGIN::Containers::Vector<T, Alloc>> : std::true_type
  {
  };

  template <class T>
  inline constexpr bool is_sequence_v = is_sequence<T>::value;

  template <class Seq>
  class SequenceAdapter
  {
  public:
    using ElemRef = decltype(std::declval<const Seq &>()[0]);
    static_assert(std::is_lvalue_reference_v<ElemRef>,
                  "SequenceAdapter requires operator[] to return lvalue reference.");
    using Elem = std::remove_reference_t<ElemRef>;
    explicit SequenceAdapter(const Seq &s) : m_seq(&s) {}
    NGIN::UIntSize Size() const
    {
      if constexpr (requires(const Seq &s) { s.size(); })
      {
        return static_cast<NGIN::UIntSize>(m_seq->size());
      }
      else if constexpr (requires(const Seq &s) { s.Size(); })
      {
        return static_cast<NGIN::UIntSize>(m_seq->Size());
      }
      else
      {
        return 0;
      }
    }
    Any Element(NGIN::UIntSize i) const { return Any{(*m_seq)[static_cast<std::size_t>(i)]}; }
    ConstAnyView ElementView(NGIN::UIntSize i) const
    {
      return Any::FromConstRef((*m_seq)[static_cast<std::size_t>(i)]);
    }

  private:
    const Seq *m_seq{};
  };

  template <class Seq>
  auto MakeSequenceAdapter(const Seq &s) { return SequenceAdapter<Seq>{s}; }

  // Tuple-like detection
  template <class T, class = void>
  struct is_tuple_like : std::false_type
  {
  };

  template <class T>
  struct is_tuple_like<T, std::void_t<decltype(std::tuple_size<T>::value)>>
  {
    static constexpr bool value = []()
    {
      if constexpr (std::tuple_size_v<T> == 0)
        return true;
      else
        return requires(T &t) { std::get<0>(t); };
    }();
  };

  template <class T>
  inline constexpr bool is_tuple_like_v = is_tuple_like<T>::value;

  template <class Tup>
  class TupleAdapter
  {
  public:
    explicit TupleAdapter(const Tup &t) : m_t(&t) {}
    static constexpr NGIN::UIntSize Size()
    {
      return static_cast<NGIN::UIntSize>(std::tuple_size<Tup>::value);
    }
    template <std::size_t I>
    Any Get() const { return Any{std::get<I>(*m_t)}; }
    template <std::size_t I>
    ConstAnyView GetView() const { return Any::FromConstRef(std::get<I>(*m_t)); }
    ConstAnyView ElementView(NGIN::UIntSize i) const
    {
      return ElementViewImpl(i, std::make_index_sequence<std::tuple_size_v<Tup>>{});
    }
    Any ElementCopy(NGIN::UIntSize i) const
    {
      return ElementCopyImpl(i, std::make_index_sequence<std::tuple_size_v<Tup>>{});
    }

  private:
    template <std::size_t... I>
    ConstAnyView ElementViewImpl(NGIN::UIntSize i, std::index_sequence<I...>) const
    {
      ConstAnyView out{};
      (void)((i == static_cast<NGIN::UIntSize>(I) ? (out = Any::FromConstRef(std::get<I>(*m_t)), true) : false) || ...);
      return out;
    }
    template <std::size_t... I>
    Any ElementCopyImpl(NGIN::UIntSize i, std::index_sequence<I...>) const
    {
      Any out = Any::MakeVoid();
      (void)((i == static_cast<NGIN::UIntSize>(I) ? (out = Any{std::get<I>(*m_t)}, true) : false) || ...);
      return out;
    }
    const Tup *m_t{};
  };

  template <class Tup>
  auto MakeTupleAdapter(const Tup &t) { return TupleAdapter<Tup>{t}; }

  // Variant-like
  template <class T>
  struct is_variant_like : std::false_type
  {
  };

  template <class... Ts>
  struct is_variant_like<std::variant<Ts...>> : std::true_type
  {
  };

  template <class T>
  inline constexpr bool is_variant_like_v = is_variant_like<T>::value;

  template <class Var>
  class VariantAdapter
  {
  public:
    explicit VariantAdapter(const Var &v) : m_v(&v) {}
    NGIN::UIntSize Index() const
    {
      return static_cast<NGIN::UIntSize>(m_v->index());
    }
    Any Get() const
    {
      Any out = Any::MakeVoid();
      std::visit([&](const auto &val)
                 { out = Any{val}; }, *m_v);
      return out;
    }
    ConstAnyView GetView() const
    {
      return std::visit([](const auto &val)
                        { return Any::FromConstRef(val); }, *m_v);
    }

  private:
    const Var *m_v{};
  };

  template <class Var>
  auto MakeVariantAdapter(const Var &v) { return VariantAdapter<Var>{v}; }

  // Optional-like detection
  template <class T>
  struct is_optional : std::false_type
  {
  };

  template <class T>
  struct is_optional<std::optional<T>> : std::true_type
  {
  };

  template <class T>
  inline constexpr bool is_optional_v = is_optional<T>::value;

  // Optional-like adapter supporting std::optional and NGIN-style APIs
  template <class Opt>
  class OptionalLikeAdapter
  {
  public:
    using Elem = typename Opt::value_type;
    explicit OptionalLikeAdapter(const Opt &o) : m_opt(&o) {}
    bool HasValue() const
    {
      if constexpr (requires(const Opt &o) { o.has_value(); })
        return m_opt->has_value();
      else if constexpr (requires(const Opt &o) { o.HasValue(); })
        return m_opt->HasValue();
      else
        return false;
    }
    Any Value() const
    {
      if (!HasValue())
        return Any::MakeVoid();
      if constexpr (requires(const Opt &o) { o.value(); })
        return Any{m_opt->value()};
      else if constexpr (requires(const Opt &o) { o.Value(); })
        return Any{m_opt->Value()};
      else
        return Any::MakeVoid();
    }
    ConstAnyView ValueView() const
    {
      if (!HasValue())
        return ConstAnyView{};
      if constexpr (requires(const Opt &o) { o.value(); })
        return Any::FromConstRef(m_opt->value());
      else if constexpr (requires(const Opt &o) { o.Value(); })
        return Any::FromConstRef(m_opt->Value());
      else
        return ConstAnyView{};
    }

  private:
    const Opt *m_opt{};
  };

  template <class Opt>
  using OptionalAdapter = OptionalLikeAdapter<Opt>;

  template <class Opt>
    requires is_optional_v<Opt>
  auto MakeOptionalAdapter(const Opt &o)
  {
    return OptionalLikeAdapter<Opt>{o};
  }

  template <class Opt>
  auto MakeOptionalLikeAdapter(const Opt &o) { return OptionalLikeAdapter<Opt>{o}; }

  // Map-like (std::map / std::unordered_map)
  template <class T>
  struct is_map : std::false_type
  {
  };

  template <class K, class V, class C, class A>
  struct is_map<std::map<K, V, C, A>> : std::true_type
  {
  };

  template <class K, class V, class H, class E, class A>
  struct is_map<std::unordered_map<K, V, H, E, A>> : std::true_type
  {
  };

  template <class T>
  inline constexpr bool is_map_v = is_map<T>::value;

  template <class Map>
  class MapAdapter
  {
  public:
    using Key = typename Map::key_type;
    using Mapped = typename Map::mapped_type;
    explicit MapAdapter(const Map &m) : m_map(&m) {}
    NGIN::UIntSize Size() const { return static_cast<NGIN::UIntSize>(m_map->size()); }
    bool ContainsKey(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return false;
      return m_map->find(key.value()) != m_map->end();
    }
    Any FindValue(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return Any::MakeVoid();
      auto it = m_map->find(key.value());
      if (it == m_map->end())
        return Any::MakeVoid();
      return Any{it->second};
    }
    ConstAnyView FindValueView(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return ConstAnyView{};
      auto it = m_map->find(key.value());
      if (it == m_map->end())
        return ConstAnyView{};
      return Any::FromConstRef(it->second);
    }
    std::expected<ConstAnyView, Error> TryFindValueView(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return std::unexpected(key.error());
      auto it = m_map->find(key.value());
      if (it == m_map->end())
        return std::unexpected(Error{ErrorCode::NotFound, "key not found"});
      return Any::FromConstRef(it->second);
    }
    std::expected<Any, Error> TryFindValueCopy(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return std::unexpected(key.error());
      auto it = m_map->find(key.value());
      if (it == m_map->end())
        return std::unexpected(Error{ErrorCode::NotFound, "key not found"});
      return Any{it->second};
    }

  private:
    const Map *m_map{};
  };

  template <class Map>
  auto MakeMapAdapter(const Map &m) { return MapAdapter<Map>{m}; }

  // FlatHashMap-like adapter (uses Size() and GetPtr())
  template <class Map>
  concept FlatHashMapLike = requires(const Map &m, const typename Map::key_type &key) {
    typename Map::key_type;
    typename Map::mapped_type;
    { m.Size() } -> std::convertible_to<NGIN::UIntSize>;
    { m.GetPtr(key) } -> std::same_as<const typename Map::mapped_type *>;
  };

  template <class Map>
    requires FlatHashMapLike<Map>
  class FlatHashMapAdapter
  {
  public:
    using Key = typename Map::key_type;
    using Mapped = typename Map::mapped_type;
    explicit FlatHashMapAdapter(const Map &m) : m_map(&m) {}
    NGIN::UIntSize Size() const { return m_map->Size(); }
    bool ContainsKey(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return false;
      return m_map->GetPtr(key.value()) != nullptr;
    }
    Any FindValue(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return Any::MakeVoid();
      if (auto *p = m_map->GetPtr(key.value()))
        return Any{*p};
      return Any::MakeVoid();
    }
    ConstAnyView FindValueView(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return ConstAnyView{};
      if (auto *p = m_map->GetPtr(key.value()))
        return Any::FromConstRef(*p);
      return ConstAnyView{};
    }
    std::expected<ConstAnyView, Error> TryFindValueView(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return std::unexpected(key.error());
      if (auto *p = m_map->GetPtr(key.value()))
        return Any::FromConstRef(*p);
      return std::unexpected(Error{ErrorCode::NotFound, "key not found"});
    }
    std::expected<Any, Error> TryFindValueCopy(const Any &k) const
    {
      auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
      if (!key)
        return std::unexpected(key.error());
      if (auto *p = m_map->GetPtr(key.value()))
        return Any{*p};
      return std::unexpected(Error{ErrorCode::NotFound, "key not found"});
    }

  private:
    const Map *m_map{};
  };

  template <class Map>
    requires FlatHashMapLike<Map>
  auto MakeFlatHashMapAdapter(const Map &m)
  {
    return FlatHashMapAdapter<Map>{m};
  }

} // namespace NGIN::Reflection::Adapters
