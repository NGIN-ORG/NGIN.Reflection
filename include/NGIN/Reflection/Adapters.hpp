// Adapters.hpp - Minimal sequence/tuple/variant adapters (Phase 1)
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Reflection/Any.hpp>
#include <NGIN/Containers/Vector.hpp>

#include <type_traits>
#include <tuple>
#include <variant>
#include <vector>

namespace NGIN::Reflection::Adapters {

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
  NGIN::UIntSize size() const {
    if constexpr (requires(Seq& s){ s.size(); }) {
      return static_cast<NGIN::UIntSize>(m_seq->size());
    } else if constexpr (requires(Seq& s){ s.Size(); }) {
      return static_cast<NGIN::UIntSize>(m_seq->Size());
    } else {
      return 0;
    }
  }
  Any element(NGIN::UIntSize i) const { return Any::make((*m_seq)[static_cast<std::size_t>(i)]); }
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
  static constexpr NGIN::UIntSize size() { return static_cast<NGIN::UIntSize>(std::tuple_size<Tup>::value); }
  template<std::size_t I>
  Any get() const { return Any::make(std::get<I>(*m_t)); }
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
  NGIN::UIntSize index() const { return static_cast<NGIN::UIntSize>(m_v->index()); }
  Any get() const {
    Any out = Any::make_void();
    std::visit([&](auto&& val){ out = Any::make(val); }, *m_v);
    return out;
  }
private:
  Var* m_v{};
};

template<class Var>
auto MakeVariantAdapter(Var& v) { return VariantAdapter<Var>{v}; }

} // namespace NGIN::Reflection::Adapters
