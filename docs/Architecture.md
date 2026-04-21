# NGIN.Reflection Architecture

This document describes the replacement runtime introduced after the ABI V1 review.

## Design Summary

NGIN.Reflection is now organized around:

1. shared identity primitives from `NGIN.Base`
2. immutable registry snapshots
3. module-owned descriptor and call tables
4. a plain C ABI for cross-module reflection

The runtime does not maintain a separate “local” metadata model and “imported” metadata model. Imported modules are translated into the same descriptor shape used by local registration.

## Identity

Reflection now relies on structured identities:

- `SymbolId`
- `ModuleIdentity`
- `TypeIdentity`

`SymbolId` is backed by a real interner instead of aliasing `std::string_view`.
`TypeIdentity` includes the owning module, canonical qualified name, and a signature hash. Hashes are lookup accelerators, not the sole ownership primitive.

## Registration And Snapshots

Registration flows through `ModuleRegistration` and `TypeBuilder<T>`.

- local registration builds a `ModuleBuildState`
- commit finalizes type identities and call tables
- the registry copies the current snapshot, applies the module contribution, and swaps the new snapshot in

All public handles carry the registry generation. Any later module replace/unload invalidates older handles cleanly.

## Descriptor Surface

The canonical type/function surface includes:

- fields
- properties
- methods
- constructors
- enum entries
- bases
- free functions
- attributes

Imported modules expose the same surface and are queried through the same `Type`, `Field`, `Property`, `Method`, `Constructor`, `Base`, and `Function` wrappers.

## ABI Surface

The cross-module ABI is `NGINReflectionModuleApi`.

Important pieces:

- `NGINReflectionModuleApi`
- `NGINReflectionTypeDesc`
- `NGINReflectionFieldDesc`
- `NGINReflectionPropertyDesc`
- `NGINReflectionMethodDesc`
- `NGINReflectionConstructorDesc`
- `NGINReflectionFunctionDesc`
- `NGINReflectionValue`
- `NGINReflectionInstanceHandle`

The host imports a module with `ImportModule`. Ownership stays explicit:

- exported modules own the descriptor tables they expose
- imported instances are reference-counted or borrowed through the instance vtable
- returned ABI values carry release hooks when they own memory

## Call Dispatch

Descriptors never store raw callable metadata as the public boundary.
Each module owns call tables for:

- field read/write
- property read/write
- method invoke
- constructor invoke
- free function invoke
- base upcast/downcast

Local registration fills these tables with generated thunks.
Imported modules bring their own function tables through `NGINReflectionModuleApi`.

## Replacement Rules

Replacement policy is strict:

- same `ModuleIdentity`: replace the prior contribution
- different `ModuleIdentity` with the same canonical qualified type name: reject as conflict

This keeps name-based tooling deterministic and prevents two modules from silently publishing the same reflected type name.
