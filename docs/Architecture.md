# NGIN.Reflection - Architecture & Implementation Notes

This document describes the core architecture, data model, and design
constraints for NGIN.Reflection. The README is the quick tour; this file keeps
the deeper details and long-term plan.

## Design goals

- No required macros in public headers or user code.
- C++23-first, with compile-time descriptors where they improve clarity or speed.
- Small, trivially copyable handles with cache-friendly lookup tables.
- Deterministic behavior and minimal global state.
- Minimal dynamic allocation in hot paths.
- Clear ABI strategy for cross-DLL metadata import/export.

## Dependencies

- NGIN.Base: `Meta::TypeName`, `Utilities::Any`, `Containers::Vector`,
  `Containers::FlatHashMap`, `Hashing::FNV1a64`, and `Memory::SystemAllocator`.
- STL: `string_view`, `expected`, `span`, `variant`, `optional`, `tuple`,
  `map`, and `unordered_map` (used mainly in adapters and diagnostics).

## High-level model

NGIN.Reflection is a process-local registry of runtime descriptors. Registration
is opt-in and occurs on demand through a single customization point:

- **ADL friend** (preferred): `friend void NginReflect(Tag<T>, TypeBuilder<T>&)`
- **Describe<T>** fallback for types you cannot modify

The TypeBuilder writes descriptors into the registry: fields, properties,
methods, constructors, enums, bases, and attributes. Consumers query by name or
type id and use small handle wrappers (`Type`, `Field`, `Method`, etc.) to access
metadata or invoke functions.

## Registration flow

1) `GetType<T>()` calls `EnsureRegistered<T>()` (once per type), which invokes
   either `NginReflect(Tag<T>{}, builder)` or `Describe<T>::Do(builder)`.
2) `TypeBuilder<T>` records metadata into the registry (name, members,
   attributes, etc.).
3) Handles reference immutable table entries by index + generation.

`TryGetType<T>()` and `FindType(name)` do not register; they only query existing
entries.

## Type identity

Type identity is a 64-bit FNV-1a hash of
`NGIN::Meta::TypeName<T>::qualifiedName`:

- `TypeIdOf<T>()` lives in `include/NGIN/Reflection/Registry.hpp`.
- This is stable across translation units and consistent with the NGIN.Base
  naming strategy.

Future work: optional 128-bit ids (see `Plan.MD`).

## Registry and threading

The registry uses a shared mutex:

- Read operations take a shared lock.
- Registration takes an exclusive lock.
- The lock is re-entrant per thread, but write acquisition while holding a read
  lock terminates. Avoid calling `GetType<T>()` while holding a read lock.

In practice: allow concurrent reads, but keep registration serialized during
startup (registration executes user code).

## Handles and descriptors

Handles are small structs containing indices (and a generation when needed) and
are validated against the current registry state. Key descriptor tables:

- `TypeRuntimeDesc`: name, typeId, size/align, members, attributes, overload
  maps.
- `FieldRuntimeDesc`: name, typeId, get/set thunks, attributes.
- `PropertyRuntimeDesc`: name, typeId, getter/setter thunks, attributes.
- `MethodRuntimeDesc`: name, return typeId, param typeIds, invoker, attributes.
- `FunctionRuntimeDesc`: same as method but for free/static functions.
- `CtorRuntimeDesc`: param typeIds, constructor invoker, attributes.
- `EnumRuntimeDesc`: underlying type id and name/value table.
- `BaseRuntimeDesc`: base type ids with optional upcast/downcast hooks.

## Overload resolution and invocation

- Methods and functions are resolved by name + argument types.
- Resolution scoring favors exact matches, then promotions, then conversions;
  narrowing and signedness changes are penalized.
- Ties resolve by registration order.
- `ResolveMethod` / `ResolveFunction` return a cached plan (`ResolvedMethod`,
  `ResolvedFunction`) that can be invoked repeatedly.

Typed helpers:

- `ResolveMethod<R, A...>()` / `ResolveMethod<R(Args...)>()`
- `Method::InvokeAs<R>(obj, args...)`
- `Type::InvokeAs<R, A...>(name, obj, args...)`
- `Function::InvokeAs<R>(args...)`

## Any

`NGIN::Reflection::Any` is an alias of `NGIN::Utilities::Any<>` (NGIN.Base). It
provides:

- 32-byte small-buffer optimization (default)
- Value boxing/unboxing with `Cast<T>()`
- Type id and size inspection

## Adapters

Adapters offer read-only access to common container shapes via `Any`:

- Sequence: `std::vector`, `NGIN::Containers::Vector`
- Tuple-like: `std::tuple`, `std::pair`
- Variant-like: `std::variant`
- Optional-like: `std::optional` and types with `HasValue/Value`
- Map-like: `std::map`, `std::unordered_map`, `NGIN::Containers::FlatHashMap`

See `include/NGIN/Reflection/Adapters.hpp` for the exact APIs.

## ABI export/merge (V1)

The optional C ABI allows a plugin to export its registry metadata for a host to
merge:

- Entrypoint: `NGINReflectionExportV1` in `include/NGIN/Reflection/ABI.hpp`.
- The blob is pointer-free; strings are stored in a single UTF-8 table.
- Methods/ctors can be invoked across DLLs only if invoke tables are emitted.

Current limitations:

- Field accessors are metadata-only across DLLs (no cross-module get/set).
- The exporter allocates the blob via `NGIN::Memory::SystemAllocator` with no
  free API yet.
- Merge conflicts are tracked but not resolved beyond skipping existing TypeIds.

## Error model

The public surface uses `std::expected<T, Error>` and does not throw on library
errors. The `Error` type carries:

- `ErrorCode` and message
- Overload diagnostics for resolution failures
- Optional closest-match index

Exceptions can still propagate from user code or allocators (e.g., during module
initialization).

## Roadmap

See `Plan.MD` for the hot-reload refactor plan and future ABI evolution. Major
milestones:

- ABI V2 with module ownership and safe hot-reload
- Function-pointer indirection table
- ABI compatibility checks and conflict resolution
- Expanded docs and examples
