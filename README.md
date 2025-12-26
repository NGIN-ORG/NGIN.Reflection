# NGIN.Reflection

Portable, high-performance runtime reflection for modern C++ (C++23). Inspect and manipulate types, fields, methods, and attributes at runtime with zero macros. Cross-DLL support is available via a gated C ABI export/merge with current limitations (see ABI section).

## Why NGIN.Reflection

- Pure C++23 API, no required macros or RTTI in the core.
- Process-local registry with small handles and cache-friendly lookups.
- Composable adapters for common containers and tuple/variant/optional/map types.
- Cross-DLL export/merge via ABI, with optional invocation tables.

```cpp
#include <NGIN/Reflection/Reflection.hpp>

struct User {
  int id{};
  float score{};

  friend void ngin_reflect(NGIN::Reflection::Tag<User>, NGIN::Reflection::TypeBuilder<User> &b) {
    b.set_name("Demo::User");
    b.field<&User::id>("id");
    b.field<&User::score>("score");
  }
};

// Query and use
auto t = NGIN::Reflection::GetType<User>();
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
  friend void ngin_reflect(NGIN::Reflection::Tag<Math>, NGIN::Reflection::TypeBuilder<Math> &b) {
    b.method<static_cast<int    (Math::*)(int,int)    const>(&Math::mul)>("mul");
    b.method<static_cast<double (Math::*)(int,double) const>(&Math::mul)>("mul");
    b.method<static_cast<float  (Math::*)(float,float)const>(&Math::mul)>("mul");
  }
};

using namespace NGIN::Reflection;
auto tm = GetType<Math>(); Math m{};

// Resolve by runtime args (promotions/conversions applied)
Any args[2] = { Any{3}, Any{2.5} };
auto mr = tm.ResolveMethod("mul", args, 2).value();
auto out = mr.Invoke(&m, args, 2).value().Cast<double>();

// Resolve by compile-time signature (exact match)
auto mi = tm.ResolveMethod<int,int,int>("mul").value();
auto sum = mi.InvokeAs<int>(&m, 3, 4).value(); // builds Any[] internally

// Or: resolve + call in one step
auto sum2 = tm.InvokeAs<int,int,int>("mul", &m, 5, 6).value();
```

## Features

- Types: lookup by qualified name or TypeId; size/alignment; `GetType<T>()` registers, `TryGetType<T>()` and `FindType(name)` do not.
- Fields: enumerate/find (`GetField`, `FindField`); GetMut/GetConst; typed `Get<T>(obj)`/`Set(obj, value)`; `GetAny`/`SetAny`; field attributes.
- Methods: enumerate/find (`GetMethod`, `FindMethods`); overload resolution with promotions/conversions; method attributes.
- Typed APIs: `ResolveMethod<R, A...>()` or `ResolveMethod<R(Args...)>()`, `Method::InvokeAs<R>(obj, args...)`, `Type::InvokeAs<R, A...>(name, obj, args...)`.
- Constructors: default construction and registered parameterized constructors via `TypeBuilder::constructor<Args...>()` + `Type::Construct(...)` and `DefaultConstruct()`.
- Attributes: on type/field/method with `std::variant<bool, int64_t, double, string_view, UInt64>`.
- Any: 32B SBO, heap fallback, copy/move, `Cast<T>()`, `GetTypeId()`, `Size()`, `Data()`.
- Adapters: sequence (std::vector, `NGIN::Containers::Vector`), tuple, variant, optional‑like, std::map/unordered_map, and `NGIN::Containers::FlatHashMap`.
- Error model: `std::expected<T, Error>` throughout; library does not throw.
- Module init helper for explicit registration in plugins (`ModuleInit.hpp`).
- ABI export/merge (methods/ctors can be invoked when invoke tables are present; fields are metadata-only across DLLs).

## Customization Points

- ADL friend (primary): define an inline friend in the class to grant private access.

  ```cpp
  struct Foo {
    int x{};
    friend void ngin_reflect(NGIN::Reflection::Tag<Foo>, NGIN::Reflection::TypeBuilder<Foo> &b) {
      b.field<&Foo::x>("x");
    }
  };
  ```

- Trait fallback (for external/3rd‑party types): specialize `NGIN::Reflection::Describe<T>` when you can’t modify the type. Only public members are accessible.

  ```cpp
  namespace NGIN::Reflection {
  template<> struct Describe<std::pair<int,int>> {
    static void Do(TypeBuilder<std::pair<int,int>>& b) {
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
- Name interning: names passed to `set_name()` and TypeBuilder APIs are interned; passing temporaries or literals is safe.

## Non‑Goals

- No macros in public API or required user code.
- No exceptions in the library core (use `std::expected`).
- No reliance on RTTI for identity across modules.

## Status & Roadmap

| Phase | Status | Highlights |
|---|---|---|
| 1 — MVP | ✅ done | Fields, methods, Any, basic adapters, examples |
| 2 — Methods | ✅ done | Overload scoring, span invoke, typed resolve/invoke, constructors, adapters |
| 3 — DLL | 🚧 partial | ABI export/merge + interop tests; field accessors not exported yet |
| 4 — Attributes/Codegen | 🚧 partial | Attributes storage done; scanner prototype planned |
| 5 — Perf/Memory | 🚧 planned | Interning/layout tuning, allocator use |
| 6 — Docs/Samples | 🚧 planned | Guides and additional samples |

See docs/Architecture.md for full details.

## Data Structures (NGIN‑first)

- `Vector<T>`: `NGIN::Containers::Vector` for tables and temporary builders.
- `StringInterner`: registry-local instance of `NGIN::Utilities::StringInterner<>` (from NGIN.Base) storing unique `std::string_view` handles.
- `HashIndex`: `FlatHashMap` from interned name id → index for O(1) average lookup.
- `Any`: alias of `NGIN::Utilities::Any<>` (from NGIN.Base); 32B SBO + allocator-configurable, with `Cast`, `GetTypeId`, and `Data` used throughout the registry.

## TypeBuilder DSL and ABI Blob

1) `GetType<T>()` (or `auto_register<T>()`) ensures T is registered, invoking ADL `ngin_reflect(Tag<T>, TypeBuilder<T>&)` or `Describe<T>::Do`; `TryGetType<T>()`/`FindType(name)` do not register.
2) `TypeBuilder<T>` writes runtime descriptors into the process-local registry (types, fields, methods, ctors, attributes).
3) ABI export packs the registry into a contiguous blob with a string table and optional invoke tables.
4) `MergeRegistryV1` validates and appends types by TypeId, reporting conflicts.

## Container & Range Adapters

- Adapters cover sequence (std::vector, `NGIN::Containers::Vector`), tuple-like, variant-like, optional-like, and map-like containers.
- Each adapter exposes size and value access as `Any`, using lightweight trait checks in `NGIN::Reflection::Adapters`.

## Planned NGIN.Base Extensions

- Hashing 128: add `NGIN::Hashing::XXH128` (or a header‑only 128‑bit FNV‑1a variant) and a `Meta::TypeId128<T>` wrapper.
- StringView utilities: small helpers to normalize compiler pretty‑function output (if broadly useful, otherwise keep local).
- Optional: a lightweight `Expected<T,E>` if we decide to eliminate `std::expected` for portability; for now, C++23 is required.

We will keep these extensions minimal and propose them via PRs to NGIN.Base as we cross each phase gate.

## Implementation Phases

Phase 0 — Bootstrap (done)

- Repo scaffold, CI build, tests, examples, package config.

Phase 1 — MVP (single-module, no DLL) — implemented

- Public headers: tags, handles (`Type`, `Field`, `Method`), `TypeBuilder<T>`, `GetType<T>()`, `GetType(name)`.
- Registry: process-local, populated on demand; fast name/TypeId -> Type lookup; name interning.
- Fields: reflect public data members; load/store thunks; `GetMut/GetConst`; `Field::GetAny/SetAny`; field attributes.
- Methods: register const/non-const member functions (explicit member pointer signature for overloads); `GetMethod`, `ResolveMethod` (overload set + scoring); method attributes.
- Attributes: type/field/method storage and lookup.
- Any: SBO 32B + heap fallback; copy supported; raw data access; type_id/size tracking.
- Adapters: basic sequence (std::vector, `NGIN::Containers::Vector`), tuple, variant.
- Examples and benchmarks: QuickStart, Adapters, Methods; reflection benchmarks.

Phase 2 — Methods & Invocation — implemented

- Overload scoring with promotions/conversions and narrowing penalties.
- `std::span<const Any>` resolve/invoke overloads and typed resolve/invoke helpers.
- Constructor descriptors and default construction support.
- Expanded adapters: optional-like, map, FlatHashMap.

Phase 3 — Cross-DLL Registry — partial

- ABI structs for registry and function pointer tables; exported C entrypoint (`NGINReflectionExportV1`) in `include/NGIN/Reflection/ABI.hpp`.
- Export packs the registry into a blob with a string table and optional invoke tables; merge appends by TypeId and reports conflicts.
- Interop tests with two shared libraries built and loaded at runtime.
- Limitations: field accessors are not exported; merge does not resolve conflicts beyond skipping existing TypeIds.

Phase 4 — Attributes & Codegen Hooks

- Scanner and codegen hooks (attribute storage is already implemented in the runtime).
- `[[using ngin: reflect, ...]]` vendor attributes consumed by a separate scanner tool.
- `ngin-reflect-scan` prototype (libclang‑based) generating ADL friend + `auto_register` glue headers.

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
                 -DNGIN_REFLECTION_ENABLE_ABI=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### ABI (V1) - Exporting a Registry Blob

- Enable with `-DNGIN_REFLECTION_ENABLE_ABI=ON` (default ON); set OFF to omit ABI export.
- Header: `#include <NGIN/Reflection/ABI.hpp>`.
- Symbol: `extern "C" bool NGINReflectionExportV1(NGINReflectionRegistryV1* out);`
- Optional init hook: `extern "C" bool NGINReflectionModuleInit();` returning `true` once module registration succeeds.
- Layout: see `include/NGIN/Reflection/ABI.hpp` for `NGINReflectionHeaderV1` and record structs. The blob contains no raw pointers; string data is referenced via offsets.
- Ownership: the exporter allocates the blob via `NGIN::Memory::SystemAllocator` and currently does not expose a free API. Treat the blob as module-owned for process lifetime, or copy it in the host.
- Host-side merge: `#include <NGIN/Reflection/ABIMerge.hpp>` then `MergeRegistryV1(mod, &stats, &err)`. The current implementation validates and appends by TypeId, attaches method/ctor invoke pointers when present, and reports conflicts.

Options:

- `NGIN_REFLECTION_BUILD_TESTS`: build Catch2 tests (default ON)
- `NGIN_REFLECTION_BUILD_EXAMPLES`: build examples (default OFF)
- `NGIN_REFLECTION_BUILD_BENCHMARKS`: build microbenchmarks (default OFF)
- `NGIN_REFLECTION_ENABLE_ABI`: build and export the C ABI entrypoint (default ON)

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
