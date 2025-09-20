# Potential Moves to NGIN.Base

- `include/NGIN/Reflection/Any.hpp`: `NGIN::Reflection::Any` is a generic small-buffer type erasure box with fallback to `SystemAllocator`. Aside from using `Meta::TypeName` to hash a type id, it is agnostic of the rest of the reflection registry; other runtimes could benefit from an `NGIN::Base::Any` for lightweight invocation, and having it in Base would avoid duplicating the SBO/allocator logic elsewhere.
- `src/Registry.cpp:24`: `detail::StringInterner` builds a reusable, allocator-backed string interner with hashing and page management but is currently private to the reflection registry. Centralizing a string interner in NGIN.Base would let other subsystems intern symbols without rolling their own storage.
- `include/NGIN/Reflection/NameUtils.hpp`: `MemberNameFromPretty` extracts member names from compiler pretty-function strings. The helper is independent of the registry and could live with other meta utilities in NGIN.Base so that non-reflection code needing consistent member naming can share it.
- `include/NGIN/Reflection/Adapters.hpp`: The container/optional/variant adapters mostly wrap standard containers and NGIN containers into `Any`. If `Any` graduates to Base, these generic adapters become multi-module primitives (useful for scripting, serialization, etc.) and would feel at home alongside other container traits in NGIN.Base.

These pieces are broadly reusable primitives with minimal coupling to reflection-specific state, so migrating them to NGIN.Base would reduce duplication and make them available to other libraries.

These adapters give the reflection layer a tiny, uniform way to poke at common STL / NGIN container & value patterns (sequence, tuple, variant, optional, map) without hard‑coding their concrete types.

Core ideas:
- Type traits (is_sequence_v, is_tuple_like_v, etc.) detect category at compile time.
- An adapter class wraps an existing object reference and exposes a minimal, generic surface (size(), element(i), get<I>(), index(), has_value(), value(), contains_key(), find_value()).
- Returned values are wrapped in Any so higher‑level reflective code can treat heterogeneous element types uniformly.
- No ownership or allocation: just lightweight views (store a raw pointer).
- Keeps everything inline / header‑only; enables generic algorithms like: “If object is sequence‑like, iterate elements reflectively”.

Quick mental model:
- SequenceAdapter: random/indexed access containers (std::vector, NGIN Vector).
- TupleAdapter: fixed compile‑time field count (std::tuple, pair, structured bindings).
- VariantAdapter: access active alternative (index + value).
- OptionalAdapter / OptionalLikeAdapter: presence + contained value.
- MapAdapter / FlatHashMapAdapter: size + key lookup with dynamic key (Any -> Key conversion).
- ConvertAny<Key> tries to turn an Any into the needed key/value type.

Example sketch:

````cpp
auto showFirstIfSequence = [](auto& obj) {
  using T = std::remove_reference_t<decltype(obj)>;
  if constexpr (NGIN::Reflection::Adapters::is_sequence_v<T>) {
    auto a = NGIN::Reflection::Adapters::MakeSequenceAdapter(obj);
    if (a.size()) {
      Any first = a.element(0);
      // ... do something generic with first ...
    }
  }
};
````

Benefit: Higher layers can write one reflective path instead of a pile of container‑specific overloads.

That’s it: they are thin category adapters bridging concrete containers to a uniform reflection API via Any.