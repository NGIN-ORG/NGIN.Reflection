# NGIN.Reflection — Architecture & Implementation Plan

This document tailors the earlier requirements to the NGIN ecosystem and outlines how we will design and implement NGIN.Reflection as a performant, portable, macro‑free runtime reflection library with first‑class cross‑DLL support and a path to codegen/tooling.

## Goals In The NGIN Ecosystem

- Zero macros in public headers and user code. Public API is pure C++23.
- Runtime reflection for types, fields, methods, attributes; opt‑in registration via ADL/friend + inline `constinit`.
- Works across Windows/Linux/macOS; strong cross‑DLL story using ABI‑stable handles and a versioned C ABI export.
- Minimal reliance on the C++ standard library; prefer NGIN.Base primitives and containers.
- Efficient: predictable memory layout, contiguous registries, cache‑friendly lookups.

## Deliverable Shape

- Compiled library with a tiny runtime plus header‑only public DSL.
  - Public headers: descriptors, handles, builder DSL, ADL hooks.
  - Compiled core: registry blob, string interning, method thunking, cross‑DLL export/merge, Any SBO storage.
- Targets: `NGIN::Reflection` (static/shared via `BUILD_SHARED_LIBS`).
- Options: `NGIN_REFLECTION_BUILD_TESTS`, `NGIN_REFLECTION_BUILD_EXAMPLES`, `NGIN_REFLECTION_ENABLE_CODEGEN` (default OFF).

## Dependencies (NGIN.Base First)

We will lean on NGIN.Base for fundamental types and utilities and only use the standard library where unavoidable (views/expected/span):

- Names/IDs: `NGIN::Meta::TypeName` and `NGIN::Meta::TypeId` (currently 64‑bit FNV‑1a).
- Containers: `NGIN::Containers::Vector`, `NGIN::Containers::FlatHashMap`.
- Traits/Introspection: `NGIN::Meta::TypeTraits`, `NGIN::FunctionTraits`.
- Primitives/Defines: `NGIN::Primitives`, platform defines from `NGIN::Defines`.
- Concurrency/Atomics: `NGIN::Thread`, `NGIN::AtomicCondition` if needed during merge.

Minimal STL usage: `std::string_view`, `std::span`, `std::expected`, `std::variant` for attribute values where beneficial. We avoid `std::string`, dynamic STL containers, and `typeid` in the core.

## Public API Principles (No Macros)

- User declares reflection by providing an ADL friend that returns a descriptor via a fluent builder DSL.
- One‑line inline `constinit` registration attaches the type to the module’s local registry table.
- No macros in public headers or required user code.

Sketch (namespaced to NGIN):

```cpp
#include <NGIN/Reflection/Reflection.hpp>

struct User {
  int id{}; NGIN::Containers::BasicString<char, 64> name{};

  NGIN_ALWAYS_INLINE auto greet(std::string_view to) const { return "hi " + std::string(to); }

  friend consteval auto ngin_reflect(NGIN::Reflection::tag<User>) {
    using NGIN::Reflection::Builder;
    auto b = Builder<User>{ .name = "User", .ns = "example" };
    b.field<&User::id>("id");
    b.field<&User::name>("name");
    b.method<&User::greet>("greet");
    return b.build();
  }
};

inline constinit auto _user_reg = NGIN::Reflection::auto_register<User>();
```

## Core Model & ABI Strategy

- Handles are small, trivially copyable index pairs into immutable tables:
  - `Type`, `Field`, `Method`, `Attribute`, etc. Each is `{uint32 table, uint32 index}`.
  - No raw pointers cross DLL boundaries.
- Registry is a contiguous, read‑only blob after construction; strings are interned once per registry.
- Cross‑DLL export uses a versioned C ABI shim:
  - `extern "C" bool ngin_reflection_export_v1(ngin_refl_registry_v1* out) noexcept;`
  - Host merges module tables; version checked, endianness validated.
- Error model: `std::expected<T, Error>`; library does not throw.

## Type Identity

- v0.1: Use `NGIN::Meta::TypeId<T>::GetId()` (64‑bit FNV‑1a of canonical name) for stable per‑type IDs across modules/compilers.
- v0.3: Introduce 128‑bit IDs behind a type and policy in NGIN.Base (see “Planned NGIN.Base extensions”).
- Canonical names from `NGIN::Meta::TypeName<T>::qualifiedName`; document compiler differences and our normalization.

## Data Structures (NGIN‑first)

- `Vector<T>`: `NGIN::Containers::Vector` for tables and temporary builders.
- `StringInterner`: per‑registry pool storing unique `std::string_view` backed by one big `Vector<char>`; index‑based.
- `HashIndex`: `FlatHashMap` from interned name id → index for O(1) average lookup.
- `Any`: reflection‑local, SBO (e.g., 32 bytes), stores TypeId + destructor/move; does not allocate for small POD.

## Builder DSL → Descriptor Blob

1) During constant‑eval/ODR use, the ADL `ngin_reflect(tag<T>)` returns a constexpr tree describing a type.
2) `auto_register<T>()` stores a compact per‑type record in a module‑local vector.
3) On module export, we pack all per‑type records into a contiguous blob and emit a table of contents.
4) Host merges multiple blobs into the process registry and fixes cross‑module indices.

## Container & Range Adapters

- Provide concepts for sequence/associative/optional/variant/tuple‑like using NGIN.Meta traits where available.
- Implement adapters that expose iteration and element access by handle, backing with NGIN containers.

## Planned NGIN.Base Extensions

- Hashing 128: add `NGIN::Hashing::XXH128` (or a header‑only 128‑bit FNV‑1a variant) and a `Meta::TypeId128<T>` wrapper.
- StringView utilities: small helpers to normalize compiler pretty‑function output (if broadly useful, otherwise keep local).
- Optional: a lightweight `Expected<T,E>` if we decide to eliminate `std::expected` for portability; for now, C++23 is required.

We will keep these extensions minimal and propose them via PRs to NGIN.Base as we cross each phase gate.

## Implementation Phases

Phase 0 — Bootstrap (done)
- Repo scaffold, CI build, tests, examples, package config.

Phase 1 — MVP (single‑module, no DLL)
- Public headers: tags, handles, `Builder<T>`, `type_of<T>()`, queries by name/TypeId.
- Registry: immutable blob; string interning placeholder; name and TypeId → Type lookup.
- Members: reflect public fields; field attributes; get_mut/get_const; `Field::set_any` (type‑checked, size‑checked memcpy for POD); `Field::attribute*`.
- Methods: register const/non‑const member methods; typed param/return ids; invoke via `Any*` → `expected<Any, Error>`; method attributes.
- Any: SBO 32 bytes with heap fallback via `NGIN::Memory::SystemAllocator` for larger types.
- Adapters: basic sequence (std::vector, NGIN::Containers::Vector), tuple, variant adapters (free‑function helpers).
- Tests: Boost.UT suites for fields, methods, attributes, adapters.
- Benchmarks: simple microbenchmarks with NGIN.Base `Benchmark` harness.

Phase 2 — Methods & Invocation
- Overload buckets by name; signature matching and safe conversions.
- Invocation via `std::span<const Any>` → `std::expected<Any, Error>`.
- Constructor descriptors and default construction, where viable.
- Expand adapters (optional, map) and tuple/variant coverage; add registry‑aware container metadata.
- Benchmarks using NGIN.Base benchmarking scaffolding.

Phase 3 — Cross‑DLL Registry
- ABI structs for registry and function pointer table; exported C entrypoint (`ngin_reflection_export_v1`) defined in `include/NGIN/Reflection/ABI.hpp`.
- Merge logic, deduplication by TypeId, conflict diagnostics.
- Stable index handles; ensure no raw pointers leak across modules.
- Interop tests with two shared libraries built and loaded at runtime.

Phase 4 — Attributes & Codegen Hooks
- Attribute storage (bool/int64/double/string/TypeId/blob).
- `[[using ngin: reflect, ...]]` vendor attributes consumed by a separate scanner tool.
- `ngin-reflect-scan` prototype (libclang‑based) generating ADL friend + `auto_register` glue headers.

Phase 5 — Performance & Memory Polish
- Interning and table layout tuning, cache alignment, optional hash indexes.
- Eliminate dynamic allocations in hot paths; leverage NGIN.Base allocators.
- Lock‑free reads after merge; one‑time merge with clear fencing.

Phase 6 — Documentation & Samples
- 10+ examples: quick‑start, fields, methods, adapters, DLL plugins, custom attributes.
- Guides: extending descriptors, writing adapters, integration into tools.

## Overarching Design Details

- Namespaces and headers: `#include <NGIN/Reflection/...>` consistently; no `ngin/` aliases.
- Exceptions: the library itself does not throw; errors returned via `std::expected`.
- RTTI independence: avoid `typeid` in cross‑DLL logic; rely on `TypeId` and base graphs.
- Memory: descriptor tables are POD; strings are indices; handles are small; all read‑only post‑merge. Any uses SBO with heap fallback via NGIN.Base.
- Concurrency: query APIs are thread‑safe after merge; the merge process is controlled via a once‑flag.

## Build & Packaging

- CMake options: `NGIN_REFLECTION_BUILD_TESTS`, `NGIN_REFLECTION_BUILD_EXAMPLES`, `NGIN_REFLECTION_BUILD_BENCHMARKS`, `NGIN_REFLECTION_ENABLE_CODEGEN` (off by default).
- `NGIN::Base` resolved from installed package or sibling source (already wired).
- Static/shared decided by `BUILD_SHARED_LIBS` (both supported); ABI export macros provided.

## Acceptance Gates Per Milestone

- v0.1: Type lookup by name/TypeId; field enumeration + get/set for public fields; Any; single registry; docs + examples.
- v0.2: Overloads, constructors, container adapters, error model throughout; perf baselines.
- v0.3: Cross‑DLL merge + handles; plugin loader helpers; strong diagnostics.
- v0.4: Scanner/codegen preview; generated code compiles warning‑free.
- v1.0: Stabilized API/ABI; benchmarks within targets; full docs and samples.

## Immediate Next Steps

- Implement header stubs: tags/handles, `Builder<T>`, registry interfaces.
- Add string interner and process‑wide registry skeleton using NGIN containers.
- Wire field descriptors and simple get/set for public fields.
- Add Boost.UT tests for Phase 1 and one concrete example mirroring this plan.
