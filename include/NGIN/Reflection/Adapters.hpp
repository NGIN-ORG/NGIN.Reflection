// Adapters.hpp - Minimal sequence/tuple/variant adapters (Phase 1)
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

#include <NGIN/Reflection/Convert.hpp>

namespace NGIN::Reflection::Adapters {

using Any = NGIN::Utilities::Any<>;

// Sequence detection (std::vector, NGIN::Containers::Vector)
template<class T>
struct is_sequence : std::false_type {};

template<class T, class A>
struct is_sequence<std::vector<T, A>> : std::true_type {};

template<class T, class Alloc>
struct is_sequence<NGIN::Containers::Vector<T, Alloc>> : std::true_type {};

template<class T>
inline constexpr bool is_sequence_v = is_sequence<T>::value;

template<class Seq>
class SequenceAdapter {
public:
  using Elem = std::remove_reference_t<decltype(std::declval<Seq&>()[0])>;
  explicit SequenceAdapter(Seq& s) : m_seq(&s) {}
  NGIN::UIntSize Size() const {
    if constexpr (requires(Seq& s){ s.size(); }) {
      return static_cast<NGIN::UIntSize>(m_seq->size());
    } else if constexpr (requires(Seq& s){ s.Size(); }) {
      return static_cast<NGIN::UIntSize>(m_seq->Size());
    } else {
      return 0;
    }
  }
  Any Element(NGIN::UIntSize i) const { return Any{(*m_seq)[static_cast<std::size_t>(i)]}; }
private:
  Seq* m_seq{};
};

template<class Seq>
auto MakeSequenceAdapter(Seq& s) { return SequenceAdapter<Seq>{s}; }

// Tuple-like detection
template<class T, class = void>
struct is_tuple_like : std::false_type {};

template<class T>
struct is_tuple_like<T, std::void_t<decltype(std::tuple_size<T>::value)>> : std::true_type {};

template<class T>
inline constexpr bool is_tuple_like_v = is_tuple_like<T>::value;

template<class Tup>
class TupleAdapter {
public:
  explicit TupleAdapter(Tup& t) : m_t(&t) {}
  static constexpr NGIN::UIntSize Size() { return static_cast<NGIN::UIntSize>(std::tuple_size<Tup>::value); }
  template<std::size_t I>
  Any Get() const { return Any{std::get<I>(*m_t)}; }
private:
  Tup* m_t{};
};

template<class Tup>
auto MakeTupleAdapter(Tup& t) { return TupleAdapter<Tup>{t}; }

// Variant-like
template<class T>
struct is_variant_like : std::false_type {};

template<class... Ts>
struct is_variant_like<std::variant<Ts...>> : std::true_type {};

template<class T>
inline constexpr bool is_variant_like_v = is_variant_like<T>::value;

template<class Var>
class VariantAdapter {
public:
  explicit VariantAdapter(Var& v) : m_v(&v) {}
  NGIN::UIntSize Index() const { return static_cast<NGIN::UIntSize>(m_v->index()); }
  Any Get() const {
    Any out = Any::MakeVoid();
    std::visit([&](auto&& val){ out = Any{val}; }, *m_v);
    return out;
  }
private:
  Var* m_v{};
};

template<class Var>
auto MakeVariantAdapter(Var& v) { return VariantAdapter<Var>{v}; }

// Optional-like
template<class T>
struct is_optional : std::false_type {};

template<class T>
struct is_optional<std::optional<T>> : std::true_type {};

template<class T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template<class Opt>
class OptionalAdapter {
public:
  using Elem = typename Opt::value_type;
  explicit OptionalAdapter(Opt& o) : m_opt(&o) {}
  bool HasValue() const { return m_opt->has_value(); }
  Any Value() const { return m_opt->has_value() ? Any{m_opt->value()} : Any::MakeVoid(); }
private:
  Opt* m_opt{};
};

template<class Opt>
auto MakeOptionalAdapter(Opt& o) { return OptionalAdapter<Opt>{o}; }

// Map-like (std::map / std::unordered_map)
template<class T>
struct is_map : std::false_type {};

template<class K, class V, class C, class A>
struct is_map<std::map<K, V, C, A>> : std::true_type {};

template<class K, class V, class H, class E, class A>
struct is_map<std::unordered_map<K, V, H, E, A>> : std::true_type {};

template<class T>
inline constexpr bool is_map_v = is_map<T>::value;

template<class Map>
class MapAdapter {
public:
  using Key = typename Map::key_type;
  using Mapped = typename Map::mapped_type;
  explicit MapAdapter(Map& m) : m_map(&m) {}
  NGIN::UIntSize Size() const { return static_cast<NGIN::UIntSize>(m_map->size()); }
  bool ContainsKey(const Any& k) const {
    auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
    if (!key) return false;
    return m_map->find(key.value()) != m_map->end();
  }
  Any FindValue(const Any& k) const {
    auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
    if (!key) return Any::MakeVoid();
    auto it = m_map->find(key.value());
    if (it == m_map->end()) return Any::MakeVoid();
    return Any{it->second};
  }
private:
  Map* m_map{};
};

template<class Map>
auto MakeMapAdapter(Map& m) { return MapAdapter<Map>{m}; }

// NGIN::Containers::FlatHashMap adapter (uses GetPtr/Size API)
template<class Map> class FlatHashMapAdapter;

template<class K, class V>
class FlatHashMapAdapter<NGIN::Containers::FlatHashMap<K, V>> {
public:
  using Key = K;
  using Mapped = V;
  explicit FlatHashMapAdapter(NGIN::Containers::FlatHashMap<K, V>& m) : m_map(&m) {}
  NGIN::UIntSize Size() const { return m_map->Size(); }
  bool ContainsKey(const Any& k) const {
    auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
    if (!key) return false;
    return m_map->GetPtr(key.value()) != nullptr;
  }
  Any FindValue(const Any& k) const {
    auto key = NGIN::Reflection::detail::ConvertAny<Key>(k);
    if (!key) return Any::MakeVoid();
    if (auto* p = m_map->GetPtr(key.value())) return Any{*p};
    return Any::MakeVoid();
  }
private:
  NGIN::Containers::FlatHashMap<K, V>* m_map{};
};

template<class Map>
auto MakeFlatHashMapAdapter(Map& m) { return FlatHashMapAdapter<Map>{m}; }

// Optional-like adapter supporting std::optional and NGIN-style APIs
template<class Opt>
class OptionalLikeAdapter {
public:
  using Elem = typename Opt::value_type;
  explicit OptionalLikeAdapter(Opt& o) : m_opt(&o) {}
  bool HasValue() const {
    if constexpr (requires(const Opt& o){ o.has_value(); }) return m_opt->has_value();
    else if constexpr (requires(const Opt& o){ o.HasValue(); }) return m_opt->HasValue();
    else return false;
  }
  Any Value() const {
    if (!HasValue()) return Any::MakeVoid();
    if constexpr (requires(const Opt& o){ o.value(); }) return Any{m_opt->value()};
    else if constexpr (requires(const Opt& o){ o.Value(); }) return Any{m_opt->Value()};
    else return Any::MakeVoid();
  }
private:
  Opt* m_opt{};
};

template<class Opt>
auto MakeOptionalLikeAdapter(Opt& o) { return OptionalLikeAdapter<Opt>{o}; }

} // namespace NGIN::Reflection::Adapters
