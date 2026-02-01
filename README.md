# NGIN.Reflection

Portable, high‑performance runtime reflection for modern C++ (C++23). Inspect and manipulate types, fields, methods, and attributes at runtime — with zero macros. Cross‑DLL support is planned via a gated C ABI.

## Why NGIN.Reflection

- Pure C++23 API, no required macros or RTTI in the core.
- Stateless, deterministic registry with small handles and cache‑friendly lookups.
- Composable adapters for common containers and tuple/variant types.
- Clear path to cross‑DLL scenarios (ABI export option + planned merge).

```cpp
#include <NGIN/Reflection/Reflection.hpp>

struct User {
  int id{};
  float score{};

  friend void ngin_reflect(NGIN::Reflection::tag<User>, NGIN::Reflection::Builder<User>& b) {
    b.set_name("Demo::User");
    b.field<&User::id>("id");
    b.field<&User::score>("score");
  }
};

// Query and use
auto t = NGIN::Reflection::TypeOf<User>();
auto f = t.GetField("id").value();
User u{};
(void)f.SetAny(&u, NGIN::Reflection::Any{42});
auto v = f.GetAny(&u).Cast<int>(); // 42
```

### Methods (overloads, conversions, and typed invocations)

```cpp
struct Math {
  int    mul(int,int) const { /*...*/ }
  double mul(int,double) const { /*...*/ }
  float  mul(float,float) const { /*...*/ }
  friend void ngin_reflect(NGIN::Reflection::tag<Math>, NGIN::Reflection::Builder<Math>& b) {
    b.method<static_cast<int    (Math::*)(int,int)    const>(&Math::mul)>("mul");
    b.method<static_cast<double (Math::*)(int,double) const>(&Math::mul)>("mul");
    b.method<static_cast<float  (Math::*)(float,float)const>(&Math::mul)>("mul");
  }
};

using namespace NGIN::Reflection;
auto tm = TypeOf<Math>(); Math m{};

// Resolve by runtime args (promotions/conversions applied)
Any args[2] = { Any{3}, Any{2.5} };
auto mr = tm.ResolveMethod("mul", args, 2).value();
auto out = mr.invoke(&m, args, 2).value().Cast<double>();

// Resolve by compile-time signature (exact match)
auto mi = tm.ResolveMethod<int,int,int>("mul").value();
auto sum = mi.invoke_as<int>(&m, 3, 4).value(); // builds Any[] internally

// Or: resolve + call in one step
auto sum2 = tm.InvokeAs<int,int,int>("mul", &m, 5, 6).value();
```

## Features

- Types: lookup by qualified name or TypeId; size/alignment.
- Fields: enumerate/find; GetMut/GetConst; `GetAny`/`SetAny`; field attributes.
- Methods: enumerate/find; overload resolution with promotions/conversions; method attributes.
- Typed APIs: `ResolveMethod<R, A...>()`, `Method::invoke_as<R>(obj, args...)`, `Type::InvokeAs<R, A...>(name, obj, args...)`.
- Constructors: default construction and registered parameterized constructors via `Builder::constructor<Args...>()` + `Type::Construct(...)` and `DefaultConstruct()`.
- Attributes: on type/field/method with `std::variant<bool, int64_t, double, string_view, UInt64>`.
- Any: 32B SBO, heap fallback, copy/move, `as<T>()`, `type_id()`, `size()`.
- Adapters: sequence (std::vector, `NGIN::Containers::Vector`), tuple, variant, optional‑like, std::map/unordered_map, and `NGIN::Containers::FlatHashMap`.
- Error model: `std::expected<T, Error>` throughout; library does not throw.

## Customization Points

- ADL friend (primary): define an inline friend in the class to grant private access.

  ```cpp
  struct Foo {
    int x{};
    friend void ngin_reflect(NGIN::Reflection::tag<Foo>, NGIN::Reflection::Builder<Foo>& b) {
      b.field<&Foo::x>("x");
    }
  };
  ```

- Trait fallback (for external/3rd‑party types): specialize `NGIN::Reflection::Describe<T>` when you can’t modify the type. Only public members are accessible.

  ```cpp
  namespace NGIN::Reflection {
  template<> struct Describe<std::pair<int,int>> {
    static void Do(Builder<std::pair<int,int>>& b) {
      b.set_name("std::pair<int,int>");
      b.field<&std::pair<int,int>::first>("first");
      b.field<&std::pair<int,int>::second>("second");
    }
  };
  } // namespace NGIN::Reflection
  ```

**Overloads & Access**

- Overload disambiguation: explicitly cast overloaded member pointers to the exact signature when registering.

  ```cpp
  b.method<static_cast<int (Math::*)(int,int) const>(&Math::mul)>("mul");
  b.method<static_cast<double (Math::*)(int,double) const>(&Math::mul)>("mul");
  b.method<static_cast<float (Math::*)(float,float) const>(&Math::mul)>("mul");
  ```

- Const vs non-const: register each variant explicitly; cv-qualification is part of the type.
- Access rules: inline friend grants private/protected access; free `ngin_reflect` and `Describe<T>` only see public members.

Notes:

- Cv/ref normalization: `T`, `const T&`, and `T&&` map to one canonical type record.
- Name interning: names passed to `set_name()` and Builder APIs are interned; passing temporaries or literals is safe.

## Non‑Goals

- No macros in public API or required user code.
- No exceptions in the library core (use `std::expected`).
- No reliance on RTTI for identity across modules.

## Status & Roadmap

| Phase | Status | Highlights |
|---|---|---|
| 1 — MVP | ✅ done | Fields, methods, Any, basic adapters, examples |
| 2 — Methods | ✅ done | Overload scoring, span invoke, typed resolve/invoke, constructors, adapters |
| 3 — DLL | ✅ done | Export blob, merge path, diagnostics/verification, interop coverage |
| 4 — Attributes/Codegen | 🚧 planned | Attributes storage, scanner prototype |
| 5 — Perf/Memory | 🚧 planned | Interning/layout tuning, allocator use |
| 6 — Docs/Samples | 🚧 planned | Guides and additional samples |

See docs/Architecture.md for full details.

## Data Structures (NGIN‑first)

- `Vector<T>`: `NGIN::Containers::Vector` for tables and temporary builders.
- `StringInterner`: registry-local instance of `NGIN::Utilities::StringInterner<>` (from NGIN.Base) storing unique `std::string_view` handles.
- `HashIndex`: `FlatHashMap` from interned name id → index for O(1) average lookup.
- `Any`: alias of `NGIN::Utilities::Any<>` (from NGIN.Base); 32B SBO + allocator-configurable, with `Cast`, `GetTypeId`, and `Data` used throughout the registry.

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

Phase 1 — MVP (single‑module, no DLL) — Implemented

- Public headers: tags, handles (`Type`, `Field`, `Method`), `Builder<T>`, `TypeOf<T>()`, `GetType(name)`.
- Registry: process‑local immutable tables; fast name/TypeId → Type lookup.
- Fields: reflect public data members; load/store thunks; `GetMut/GetConst`; `Field::GetAny/SetAny`; field attributes.
- Methods: register const member functions (explicit member pointer signature for overloads); `GetMethod`, `ResolveMethod` (overload set + scoring); method attributes.
- Overload resolution: prefers exact > promotion > conversion, penalizes narrowing and signedness changes. Tie‑break by registration order.
- Any: SBO 32B + heap fallback; copy supported; raw data access; type_id/size tracking.
- Adapters: basic sequence (std::vector, `NGIN::Containers::Vector`), tuple, variant.
- Examples: QuickStart, Adapters, Methods (overload+conversion demo).
- Benchmarks: invoke vs direct call; SetAny vs direct set; conversion invoke.
- ABI stub: `NGINReflectionExportV1` returns registry counts (blob TBD).

Phase 2 — Methods & Invocation (implemented)

- Refined overload scoring (promotion vs conversion tiers; improved signedness handling; better specificity tie-breaks).
- Added `std::span<const Any>` invoke overloads plus typed resolve/invoke helpers.
- Introduced constructor descriptors, default construction helpers, and runtime construction APIs.
- Expanded adapters (map/optional/FlatHashMap) and registry-aware metadata.
- Added benchmarks covering method invocation, conversions, and field access.

Phase 3 — Cross‑DLL Registry (implemented)

- Implemented ABI V1 export (`NGINReflectionExportV1`) and binary blob layout generation.
- Landed module merge path with type dedupe/aliasing, optional `MergeDiagnostics` collection, `MergeCallbacks` logging hook, `VerifyProcessRegistry` helper, and dual-plugin interop + negative fixtures.
- Host usage (opt-in):
  ```cpp
  NGINReflectionRegistryV1 mod{};
  MergeDiagnostics diag{};
  auto handler = [](MergeEvent ev, const MergeEventInfo &info) {
    if (ev == MergeEvent::TypeConflict) {
      std::fprintf(stderr, "conflict on %.*s\n",
                   static_cast<int>(info.incomingName.size()),
                   info.incomingName.data());
    } else if (ev == MergeEvent::ModuleComplete) {
      std::fprintf(stderr, "merge stats: +%llu / conflicts=%llu\n",
                   static_cast<unsigned long long>(info.typesAdded),
                   static_cast<unsigned long long>(info.typesConflicted));
    }
  };
  auto cb = MakeMergeCallbacks(handler);
  const char *err = nullptr;
  if (!MergeRegistryV1(mod, nullptr, &err, &diag, &cb) && diag.HasConflicts()) {
    // inspect diag.typeConflicts for each duplicate TypeId
  }
  VerifyRegistryOptions opts{};
  opts.checkMethodOverloads = true;
  (void)VerifyProcessRegistry(opts); // optional post-merge consistency check
  ```
- Future refinements: richer logging hooks and cross-module diagnostics as consumers request them.

Phase 4 — Attributes & Codegen Hooks

- Extend attribute storage to cover structured values (bool/int64/double/string_view/TypeId/blob) with lightweight iteration/query helpers on `Type`, `Field`, and `Method`.
- Stabilize attribute export in the ABI blob so metadata survives module boundaries.
- Introduce vendor attributes (`[[using ngin::reflect(... )]]`) collected by a libclang-based `ngin-reflect-scan` tool.
- Ship a prototype codegen pipeline: scan translation units, emit `*.ngin.reflect.hpp` shims that forward to `Builder<T>` and register attributes, wire it into CMake presets/CI.
- Document authoring guidance (naming, attribute schema, opt-in strategy) for teams adopting the scanner.

Phase 5 — Performance & Memory Polish

- Interning and table layout tuning, cache alignment, optional hash indexes.
- Eliminate dynamic allocations in hot paths; leverage NGIN.Base allocators.
- Lock‑free reads after merge; one‑time merge with clear fencing.

Phase 6 — Documentation & Samples

- 10+ examples: quick‑start, fields, methods, adapters, DLL plugins, custom attributes.
- Guides: extending descriptors, writing adapters, integration into tools.

## Build & Install

Requirements: C++23 compiler; NGIN.Base (found via package or sibling source).

```bash
cmake -S . -B build -DNGIN_REFLECTION_BUILD_TESTS=ON \
                 -DNGIN_REFLECTION_BUILD_EXAMPLES=ON \
                 -DNGIN_REFLECTION_BUILD_BENCHMARKS=OFF \
                 -DNGIN_REFLECTION_ENABLE_ABI=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### ABI (V1) — Exporting a Registry Blob

- Enable with `-DNGIN_REFLECTION_ENABLE_ABI=ON`.
- Header: `#include <NGIN/Reflection/ABI.hpp>`.
- Symbol: `extern "C" bool NGINReflectionExportV1(NGINReflectionRegistryV1* out);`
- Optional init hook: `extern "C" bool NGINReflectionModuleInit();` returning `true` once module registration succeeds.
- Layout: see `include/NGIN/Reflection/ABI.hpp` for `NGINReflectionHeaderV1` and record structs. The blob contains no raw pointers; string data is referenced via offsets.
- Host-side merge (skeleton): `#include <NGIN/Reflection/ABIMerge.hpp>` then `MergeRegistryV1(mod, &stats, &err, &diag, &callbacks)` with diagnostics/callbacks optional (pass `nullptr` to skip). The initial implementation validates and records basic stats; full dedup/reindexing follows.

Options:

- `NGIN_REFLECTION_BUILD_TESTS`: build Catch2 tests (default ON)
- `NGIN_REFLECTION_BUILD_EXAMPLES`: build examples (default OFF)
- `NGIN_REFLECTION_BUILD_BENCHMARKS`: build microbenchmarks (default OFF)
- `NGIN_REFLECTION_ENABLE_ABI`: build and export the C ABI entrypoint (default OFF)

ABI export:

- Header: `#include <NGIN/Reflection/ABI.hpp>` (function declaration is visible only when `NGIN_REFLECTION_ENABLE_ABI` is defined)
- Symbol: `extern "C" bool NGINReflectionExportV1(NGINReflectionRegistryV1* out);`

### Module Initialization Helpers

- Header: `#include <NGIN/Reflection/ModuleInit.hpp>`.
- Use `EnsureModuleInitialized("ModuleName", fn)` to guard registration logic and run it once per module instance.
- `fn` receives a `ModuleRegistration &` helper so you can call `RegisterTypes<T...>()` or access the registry directly.
- Recommended DLL entrypoint pattern:

```cpp
extern "C" NGIN_REFLECTION_API bool NGINReflectionModuleInit()
{
  return NGIN::Reflection::EnsureModuleInitialized("MyPlugin", [](auto &module) {
    module.RegisterTypes<MyComponent, AnotherComponent>();
  });
}
```

## Examples & Docs

- Examples: `examples/QuickStart`, `examples/Methods`, `examples/Adapters`.
- Extended docs: `docs/Architecture.md` (design, phases, data structures).
