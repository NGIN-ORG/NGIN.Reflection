# NGIN.Reflection — Architecture & Implementation Notes

This document describes the internal architecture, data model, and design
constraints of NGIN.Reflection. The README is the user-facing tour; this file
covers the mechanics and long-term direction.

---

## Design goals

- No required macros in public headers or user code
- C++23‑first design
- Small, trivially copyable handles
- Deterministic behavior with minimal global state
- Minimal dynamic allocation in hot paths
- Explicit and debuggable ABI strategy

---

## Dependencies

- **NGIN.Base**
  - `Meta::TypeName`
  - `Utilities::Any`
  - `Containers::Vector`
  - `Containers::FlatHashMap`
  - `Hashing::FNV1a64`
  - `Memory::SystemAllocator`
- **STL**
  - `string_view`, `expected`, `span`, `variant`, `optional`, `tuple`
  - `map`, `unordered_map` (primarily adapters and diagnostics)

---

## High‑level model

NGIN.Reflection is a **process‑local registry** of runtime descriptors.
Registration is explicit and opt‑in via one of two customization points:

- ADL friend:
  `friend void NginReflect(Tag<T>, TypeBuilder<T>&)`
- Trait fallback:
  `Describe<T>::Do(TypeBuilder<T>&)`

`TypeBuilder<T>` writes immutable descriptor tables into the registry.
Consumers query metadata by name or type id and interact via lightweight handles
(`Type`, `Field`, `Method`, etc.).

---

## Registration flow

1. `GetType<T>()` calls `EnsureRegistered<T>()` once per type.
2. `NginReflect` or `Describe<T>::Do` is invoked.
3. `TypeBuilder<T>` records metadata into registry tables.
4. Handles reference descriptors by index (+ generation when required).

`TryGetType<T>()` and `FindType(name)` only query existing entries.

---

## Type identity

Type identity is a 64‑bit FNV‑1a hash of
`NGIN::Meta::TypeName<T>::qualifiedName`.

Properties:

- Stable across translation units
- Consistent with NGIN.Base naming
- Not guaranteed stable across different compilers, standard libraries, or ABI modes

Future work includes optional 128‑bit identifiers.

---

## Registry & threading

The registry uses a shared mutex:

- Reads acquire a shared lock
- Registration acquires an exclusive lock

⚠️ Acquiring a write lock while holding a read lock results in `std::terminate`.
Do not call `GetType<T>()` from inside reflection read callbacks.

In practice:

- Allow concurrent reads freely
- Serialize registration during startup

---

## Handles & descriptor tables

Handles are small value types validated against registry generations.

Key tables:

- `TypeRuntimeDesc`
- `FieldRuntimeDesc`
- `PropertyRuntimeDesc`
- `MethodRuntimeDesc`
- `FunctionRuntimeDesc`
- `CtorRuntimeDesc`
- `EnumRuntimeDesc`
- `BaseRuntimeDesc`

All tables are append‑only after registration.

---

## Overload resolution

Resolution considers:

1. Exact matches
2. Promotions
3. Conversions

Conversions are limited to the numeric conversions implemented in
`NGIN::Reflection::detail::ConvertAny` (exact match + arithmetic conversions).
No user-defined conversions are attempted.

Narrowing and signedness changes are penalized.
Ties resolve by registration order.

Resolution produces a cached plan (`ResolvedMethod` / `ResolvedFunction`) that
can be reused across invocations.

---

## Any

`NGIN::Reflection::Any` aliases `NGIN::Utilities::Any<>` and provides:

- 32‑byte small‑buffer optimization
- Type‑safe boxing/unboxing
- Runtime type inspection
- Copying an `Any` that holds a non‑copyable, non‑trivially‑copyable type will throw

---

## Adapters

Adapters provide read‑only access to common container shapes via `Any`:

- Sequence
- Tuple‑like
- Variant‑like
- Optional‑like
- Map‑like

Adapters never mutate containers. `*View()` APIs return non‑owning views; `Any`
returning APIs copy values and may allocate depending on the stored type.

---

## ABI export / merge (V1)

The optional C ABI allows a module to export its registry metadata:

- Entrypoint: `NGINReflectionExportV1`
- Pointer‑free blob with interned UTF‑8 strings
- Import merges metadata by TypeId

Conflict behavior:

- `AppendOnly` (default): conflicting TypeIds are skipped and counted
- `ReplaceOnConflict`: replaces only when `moduleId` matches (or moduleId is 0)
- `RejectOnConflict`: merge fails with an error string

Merge diagnostics are exposed via `MergeStats` and the optional error string.

Limitations:

- Field accessors are metadata‑only across modules
- No allocator‑neutral free API yet
- Conflicting TypeIds are skipped, not resolved

Hosts should treat exported blobs as **module‑owned** and copy immediately.

---

## Error model

Public APIs return `std::expected<T, Error>`.

`Error` contains:

- Error code
- Human‑readable message
- Overload diagnostics
- Optional closest‑match index

The library itself does not throw.

---

## Roadmap

See `Plan.MD` for planned work, including:

- ABI V2 with ownership and hot‑reload safety
- Invocation indirection tables
- ABI compatibility validation
- Expanded documentation and examples
