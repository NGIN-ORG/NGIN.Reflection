# NGIN.Reflection — Architecture & Implementation Plan

This document describes the architecture, data model, and roadmap for NGIN.Reflection. The README is intentionally concise; this file retains the deeper design details.

## Goals

- No macros in public headers or user code; pure C++23 API.
- Runtime reflection for types, fields, methods, attributes via ADL + fluent Builder.
- Cross‑platform (Windows/Linux/macOS) with a clear cross‑DLL strategy.
- Prefer NGIN.Base primitives/containers over STL; minimal STL (string_view, expected, span, variant).
- Deterministic, allocation‑disciplined, and cache‑friendly.

## Deliverables

- Library target `NGIN::Reflection` (shared/static via `BUILD_SHARED_LIBS`).
- Public headers: descriptors, handles, Builder DSL, ADL hooks.
- Compiled core: registry, Any, optional ABI export stub (gated by `NGIN_REFLECTION_ENABLE_ABI`).

## Dependencies

- `NGIN::Meta::TypeName` for canonical type names and identity.
- `NGIN::Containers::{Vector,FlatHashMap}` for tables and indexes.
- `NGIN::Memory::SystemAllocator` for Any heap fallback.

## Core Model

- Small, trivially copyable handles (indices) for `Type`, `Field`, `Method`.
- Immutable, process‑local registry after construction; strings interned per registry.
- Error handling via `std::expected<T, Error>`; library does not throw.

## ABI Strategy (Phase 3)

- Versioned C entrypoint (name: `NGINReflectionExportV1`) returning registry header and blob pointer.
- Enabled via `NGIN_REFLECTION_ENABLE_ABI` (header declaration and compiled stub when ON).
- Host merges modules by `TypeId`, deduplicating and fixing indices.

## Type Identity

- 64‑bit FNV‑1a over canonical names (`NGIN::Meta::TypeName<T>::qualifiedName`).
- Future: 128‑bit policy in NGIN.Base if needed.

## Data Structures

- `TypeRuntimeDesc`: name, `typeId`, size, align, fields, methods, attributes, overload map.
- `FieldRuntimeDesc`: name, typeId, size, function pointers for get/load/store, attributes.
- `MethodRuntimeDesc`: name, return typeId, param typeIds, invoker FP, attributes.
- `CtorRuntimeDesc` (Phase 2): param typeIds, construct FP, attributes.
- Global `Registry`: vectors of types and hash maps for lookups.

## Builder DSL

- `b.set_name(qualified)`
- `b.field<&T::member>(name)`
- `b.method<sig>(name)` — use explicit member pointer type to disambiguate overloads
- `b.attribute(key, value)` / `b.field_attribute<member>(...)` / `b.method_attribute<sig>(...)`
- `b.constructor<Args...>()` (Phase 2)

## Invocation & Overload Resolution

- Runtime resolution (`ResolveMethod(name, Any*, count)`) with scoring:
  - exact match > promotion > conversion; penalize narrowing/signedness changes.
  - tie‑break by registration order.
- Typed resolution (`ResolveMethod<R, A...>(name)`) — exact param/return IDs.
- Invocation:
  - `Method::invoke(obj, Any*, count)` and `Method::invoke(obj, span<Any>)`.
  - `Method::invoke_as<R>(obj, args...)` builds `Any[]`, invokes, and returns `expected<R, Error>`.
  - `Type::InvokeAs<R, A...>(name, obj, args...)` resolves by types then invokes.

## Any

- 32B small‑buffer optimization; heap fallback via `SystemAllocator`.
- Tracks type_id and size; supports copy/move and raw data access.

## Adapters

- Sequence adapters for `std::vector` and `NGIN::Containers::Vector`.
- Tuple/variant adapters.
- Optional‑like adapter (supports `std::optional` and NGIN‑style `HasValue/Value`).
- Map adapters for `std::map`, `std::unordered_map`, and `NGIN::Containers::FlatHashMap`.

## Implementation Phases

Phase 0 — Bootstrap (done)

- Repo scaffold, CI build, tests, examples, package config.

Phase 1 — MVP (implemented)

- Tags, handles, Builder<T>, type lookup, fields, methods, Any, basic adapters, examples, benches.

Phase 2 — Methods & Invocation (in progress)

- Refined numeric scoring; `span<const Any>` overloads; typed resolve/invoke; constructors; expanded adapters.

Phase 3 — Cross‑DLL Registry

- ABI structs and export; merge/dedup; stable indices; interop tests.

Phase 4 — Attributes & Codegen Hooks

- Attribute storage and scanner prototype.

Phase 5 — Performance & Memory Polish

- Interning/layout tuning; allocator use; lock‑free reads post‑merge.

Phase 6 — Documentation & Samples

- 10+ examples; guides for extending descriptors/adapters; tool integration.

## Acceptance Gates

- v0.1: Type/field basics, Any, single registry, examples/tests.
- v0.2: Overloads, constructors, adapters, error model, perf baselines.
- v0.3: Cross‑DLL merge + handles, loader helpers, diagnostics.
- v0.4: Scanner/codegen preview, warning‑free.
- v1.0: Stabilized API/ABI, documented, benchmarked.

