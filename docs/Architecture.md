# NGIN.Reflection — Architecture & Implementation Plan

This document describes the architecture, data model, and roadmap for NGIN.Reflection. The README is intentionally concise; this file retains the deeper design details.

## Goals

- No macros in public headers or user code; pure C++23 API.
- Runtime reflection for types, fields, properties, methods, attributes via ADL + fluent TypeBuilder.
- Cross‑platform (Windows/Linux/macOS) with a clear cross‑DLL strategy.
- Prefer NGIN.Base primitives/containers over STL; minimal STL (string_view, expected, span, variant).
- Deterministic, allocation‑disciplined, and cache‑friendly.

## Deliverables

- Library target `NGIN::Reflection` (shared/static via `BUILD_SHARED_LIBS`).
- Public headers: descriptors, handles, TypeBuilder DSL, ADL hooks, module init helper.
- Compiled core: registry, direct integration with shared utilities (e.g., `NGIN::Utilities::Any<>`), ABI export + merge (gated by `NGIN_REFLECTION_ENABLE_ABI`).

## Dependencies

- `NGIN::Meta::TypeName` for canonical type names and identity.
- `NGIN::Containers::{Vector,FlatHashMap}` for tables and indexes.
- `NGIN::Utilities::{Any,StringInterner}` (via NGIN.Base) for runtime boxing and interned names, along with `NGIN::Memory::SystemAllocator` as their default allocator.

## Core Model

- Small, trivially copyable handles (indices) for `Type`, `Field`, `Method`.
- Process-local registry populated on demand; strings interned per registry.
- Registration is not currently thread-safe (concurrent `GetType<T>()` should be avoided during startup).
- Error handling via `std::expected<T, Error>`; library does not throw.

## Lookup APIs

- `GetType<T>()` ensures registration; `TryGetType<T>()` and `FindType(name)` do not register.
- `GetField`/`GetProperty`/`GetMethod` return `std::expected` with errors; `FindField`/`FindProperty`/`FindMethod` return `std::optional`.
- `FindMethods(name)` returns an overload view (size + index access).
- Fields expose typed `Get<T>(obj)`/`Set(obj, value)` helpers in addition to Any-based access.
- `ResolveMethod(name, args)` returns a `ResolvedMethod` that caches the argument shape.

## ABI Strategy (Phase 3, partial)

- Versioned C entrypoint (name: `NGINReflectionExportV1`) returning a registry header and blob pointer.
- Enabled via `NGIN_REFLECTION_ENABLE_ABI` (default ON when building the library).
- Export blob contains a string table and optional method/ctor invoke tables; no raw pointers.
- `MergeRegistryV1` validates the blob and appends types by `TypeId`, recording conflicts.
- Limitations: field accessors are not exported; merge does not resolve conflicts beyond skipping existing TypeIds; blob allocation uses `SystemAllocator` with no public free API yet.

## Type Identity

- 64‑bit FNV‑1a over canonical names (`NGIN::Meta::TypeName<T>::qualifiedName`).
- Future: 128‑bit policy in NGIN.Base if needed.

## Data Structures

- `TypeRuntimeDesc`: name, `typeId`, size, align, fields, properties, methods, attributes, overload map.
- `FieldRuntimeDesc`: name, typeId, size, function pointers for get/load/store, attributes.
- `PropertyRuntimeDesc`: name, typeId, get/set thunks, attributes.
- `MethodRuntimeDesc`: name, return typeId, param typeIds, invoker FP, attributes.
- `CtorRuntimeDesc`: param typeIds, construct FP, attributes.
- Global `Registry`: vectors of types and hash maps for lookups.

## TypeBuilder DSL

- `b.set_name(qualified)`
- `b.field<&T::member>(name)`
- `b.property<Getter, Setter>(name)` or `b.property<Getter>(name)` (getter-only)
- `b.method<sig>(name)` — use explicit member pointer type to disambiguate overloads
- `b.attribute(key, value)` / `b.field_attribute<member>(...)` / `b.property_attribute<getter>(...)` / `b.method_attribute<sig>(...)`
- `b.constructor<Args...>()`

## Invocation & Overload Resolution

- Runtime resolution (`ResolveMethod(name, Any*, count)`) with scoring:
  - exact match > promotion > conversion; penalize narrowing/signedness changes.
  - tie‑break by registration order.
- Typed resolution (`ResolveMethod<R, A...>(name)` or `ResolveMethod<R(Args...)>(name)`) — exact param/return IDs.
- Invocation:
  - `Method::Invoke(obj, Any*, count)` and `Method::Invoke(obj, span<const Any>)`.
  - `Method::InvokeAs<R>(obj, args...)` builds `Any[]`, invokes, and returns `expected<R, Error>`.
  - `Type::InvokeAs<R, A...>(name, obj, args...)` resolves by types then invokes.

## Any

- Uses `NGIN::Utilities::Any<>` from NGIN.Base (32B small-buffer + `SystemAllocator` fallback).
- Reflection code relies on `Cast`, `GetTypeId`, `Size`, `Data`, `HasValue`, and `MakeVoid` provided by the shared implementation.

## Adapters

- Sequence adapters for `std::vector` and `NGIN::Containers::Vector`.
- Tuple/variant adapters.
- Optional‑like adapter (supports `std::optional` and NGIN‑style `HasValue/Value`).
- Map adapters for `std::map`, `std::unordered_map`, and `NGIN::Containers::FlatHashMap`.

## Implementation Phases

Phase 0 — Bootstrap (done)

- Repo scaffold, CI build, tests, examples, package config.

Phase 1 — MVP (implemented)

- Tags, handles, TypeBuilder<T>, type lookup, fields, methods, Any, attributes, basic adapters, examples, benches.

Phase 2 — Methods & Invocation (implemented)

- Refined numeric scoring; `span<const Any>` overloads; typed resolve/invoke; constructors; expanded adapters.

Phase 3 — Cross-DLL Registry (partial)

- ABI structs and export/merge; interop tests; conflict tracking. Field accessors are not exported yet.

Phase 4 — Attributes & Codegen Hooks

- Scanner/codegen prototype (attribute storage is already implemented in runtime).

Phase 5 — Performance & Memory Polish

- Interning/layout tuning; allocator use; lock‑free reads post‑merge.

Phase 6 — Documentation & Samples

- 10+ examples; guides for extending descriptors/adapters; tool integration.

## Acceptance Gates

- v0.1 (done): Type/field basics, Any, single registry, examples/tests.
- v0.2 (done): Overloads, constructors, adapters, error model, perf baselines.
- v0.3 (partial): Cross-DLL merge + handles, loader helpers, diagnostics.
- v0.4: Scanner/codegen preview, warning‑free.
- v1.0: Stabilized API/ABI, documented, benchmarked.
