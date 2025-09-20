# Potential Moves to NGIN.Base

- `include/NGIN/Reflection/Any.hpp`: `NGIN::Reflection::Any` is a generic small-buffer type erasure box with fallback to `SystemAllocator`. Aside from using `Meta::TypeName` to hash a type id, it is agnostic of the rest of the reflection registry; other runtimes could benefit from an `NGIN::Base::Any` for lightweight invocation, and having it in Base would avoid duplicating the SBO/allocator logic elsewhere.
- `src/Registry.cpp:24`: `detail::StringInterner` builds a reusable, allocator-backed string interner with hashing and page management but is currently private to the reflection registry. Centralizing a string interner in NGIN.Base would let other subsystems intern symbols without rolling their own storage.
- `include/NGIN/Reflection/NameUtils.hpp`: `MemberNameFromPretty` extracts member names from compiler pretty-function strings. The helper is independent of the registry and could live with other meta utilities in NGIN.Base so that non-reflection code needing consistent member naming can share it.
- `include/NGIN/Reflection/Adapters.hpp`: The container/optional/variant adapters mostly wrap standard containers and NGIN containers into `Any`. If `Any` graduates to Base, these generic adapters become multi-module primitives (useful for scripting, serialization, etc.) and would feel at home alongside other container traits in NGIN.Base.

These pieces are broadly reusable primitives with minimal coupling to reflection-specific state, so migrating them to NGIN.Base would reduce duplication and make them available to other libraries.
