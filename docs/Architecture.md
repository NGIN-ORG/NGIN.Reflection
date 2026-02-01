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
- Compiled core: registry, direct integration with shared utilities (e.g., `NGIN::Utilities::Any<>`), optional ABI export stub (gated by `NGIN_REFLECTION_ENABLE_ABI`).

## Dependencies

- `NGIN::Meta::TypeName` for canonical type names and identity.
- `NGIN::Containers::{Vector,FlatHashMap}` for tables and indexes.
- `NGIN::Utilities::{Any,StringInterner}` (via NGIN.Base) for runtime boxing and interned names, along with `NGIN::Memory::SystemAllocator` as their default allocator.

## Core Model

- Small, trivially copyable handles (indices) for `Type`, `Field`, `Method`.
- Immutable, process‑local registry after construction; strings interned per registry.
- Error handling via `std::expected<T, Error>`; library does not throw.

## ABI Strategy (Phase 3)

- Versioned C entrypoint (`NGINReflectionExportV1`) publishes a compact, POD-only blob describing all reflected entities in the module.
- Layout:
  - `NGINReflectionHeaderV1` at the front of the blob encodes counts and byte offsets for every section, plus optional tables for method/ctor function pointers.
  - Fixed-width arrays follow in this order: types → fields → methods → ctors → attributes → parameter type-ids → UTF‑8 string table → optional function-pointer tables.
  - All cross-record references are indices into those arrays (no raw pointers); strings use `(offset,size)` pairs into the shared table.
- Ownership & lifetime:
  - The exporter allocates the blob once per process using `NGIN::Memory::SystemAllocator`; the memory stays owned by the exporting module.
  - Consumers must treat the blob as read-only and either copy it or ensure they stop dereferencing once the module unloads.
  - `CopyRegistryBlob` (see `NGIN::Reflection::RegistryBlobCopy`) clones the payload into host-owned memory and reconstitutes a stable `NGINReflectionRegistryV1` view via `AsRegistry()`.
  - No free function is exposed in V1; destroying the module implicitly reclaims the blob.
- Error handling:
  - `MergeRegistryV1` rejects null payloads, version mismatches, and range violations with descriptive strings (`"null registry"`, `"unsupported version"`, `"corrupt offsets"`).
  - Stats counters only advance on successful merges; callers should reset/inspect `MergeStats` per module load.
  - Optional `MergeDiagnostics` captures per-type conflicts without affecting the default fast path.
- Enabled via `NGIN_REFLECTION_ENABLE_ABI` (header declaration and compiled stub when ON); disabled builds define no-op tests and omit export symbols.
- Host merges modules by `TypeId`, deduplicating duplicates, and layering alias lookups (including MSVC `class`/`struct` prefix stripping).
- Post-merge verification runs opt-in via `VerifyProcessRegistry(options)`, ensuring overload tables remain consistent without imposing costs unless invoked.

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

- Uses `NGIN::Utilities::Any<>` from NGIN.Base (32B small-buffer + `SystemAllocator` fallback).
- Reflection code relies on `Cast`, `TryCast`, `GetTypeId`, `Size`, and `Data` provided by the shared implementation.

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

Phase 2 — Methods & Invocation (implemented)

- Refined numeric scoring (promotion vs conversion tiers and signedness handling).
- Added `span<const Any>` overloads, typed resolve/invoke helpers, and constructor descriptors.
- Expanded adapters (map/optional/FlatHashMap) and constructor metadata coverage.

Phase 3 — Cross‑DLL Registry (implemented)

- ABI V1 export surface implemented (`NGINReflectionExportV1`) with contiguous blob writer.
- Merge path ingests module payloads, dedupes by TypeId, and is validated via dual-plugin interop test plus targeted negative fixtures.
- Optional diagnostics (`MergeDiagnostics`) and `VerifyProcessRegistry` helper provide opt-in conflict details and consistency checks.
- Usage pattern:
  ```cpp
  MergeDiagnostics diag{};
  auto handler = [](MergeEvent ev, const MergeEventInfo &info) {
    if (ev == MergeEvent::TypeConflict) {
      // Log or record the conflict for diagnostics.
    } else if (ev == MergeEvent::ModuleComplete) {
      // Observe per-module statistics if desired.
    }
  };
  auto callbacks = MakeMergeCallbacks(handler);
  const char *err = nullptr;
  if (!MergeRegistryV1(mod, &stats, &err, &diag, &callbacks) && diag.HasConflicts()) {
    for (const auto &conflict : diag.typeConflicts) {
      // host logging or conflict resolution
    }
  }
  VerifyRegistryOptions verify{};
  verify.checkMethodOverloads = true;
  (void)VerifyProcessRegistry(verify); // optional integrity check
  ```
- Future: additional host-side logging helpers and advanced validation toggles can be layered via `MergeCallbacks`.

Phase 4 — Attributes & Codegen Hooks

- Attribute storage: finalize `AttributeValue` variant (bool/int64/double/string_view/TypeId/blob) and expose indexed + keyed lookups across type/field/method descriptors.
- ABI integration: persist attributes inside exported blobs with stable key IDs so cross-module consumers see consistent metadata.
- Codegen pipeline: `[[using ngin::reflect(... )]]` vendor attributes mark types/functions; `ngin-reflect-scan` (libclang) parses sources, emits `*.ngin.reflect.hpp` files that contain generated ADL/Describe stubs and attribute registrations, and drops CMake targets for incremental rebuilds.
- Tooling deliverables: CLI options for include/exclude filters, diagnostic reporting for unsupported constructs, and sample CI configuration for cache reuse.
- Authoring docs: guidelines for custom attribute schemas, naming conventions, and strategies for mixing handwritten and generated descriptors.

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
