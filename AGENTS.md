# NGIN.Reflection Agent Guide

NGIN.Reflection is a C++23 **runtime** reflection library built on NGIN.Base.
Types opt in explicitly through `TypeBuilder<T>` (hand-written `NginReflect`
or code emitted by `NGIN.Reflection.MetaGen`), register into immutable registry
snapshots, and can be exported/imported across modules through a plain C ABI.

It is consumed by the NGIN workspace through `Packages/NGIN.Reflection` and is
also a standalone repository (a git submodule of NGIN). When working inside the
NGIN workspace, the root `AGENTS.md` also applies.

## Source of Truth

- `docs/Architecture.md` — identity model, snapshots, descriptor surface, ABI,
  call dispatch, and module replacement rules. Read it before changing the
  registry, ABI, or descriptor shapes.
- Public headers in `include/NGIN/Reflection/`; implementation in `src/`.
- Tests in `tests/Reflection/` (behavior) and `tests/Interop/` (host + plugin
  modules across the ABI).

## Invariants to Preserve

- One descriptor model: imported modules are translated into the same shape as
  local registration. Don't add a separate "imported" metadata path.
- Registry snapshots are immutable; commits copy, apply, and swap. Public handles
  carry the registry generation and must be invalidated by module replace/unload.
- Same `ModuleIdentity` replaces its prior contribution; a different module
  publishing the same canonical qualified type name is a conflict and is rejected.
- Unloading is rejected while reflected instances from that module are alive.
- The cross-module boundary is `NGINReflectionModuleApi` (`ABI.hpp`): plain C,
  module-owned descriptor/call tables, explicit release hooks for owned values.
  Changing ABI structs or semantics needs tests in `tests/Interop/` and a note
  in `docs/Architecture.md`. Binary compatibility is not promised before 1.0.
- Lookups report failure through `std::expected<..., Error>` (`ExpectedType`,
  `ExpectedMethod`, …); keep that pattern for new queries.
- Generated MetaGen output must keep compiling against the public
  `TypeBuilder` API; changes there affect `Examples/Hello.Reflection`.

## Code Style

No local `.clang-format`; match the existing files: 2-space indent, braces on
their own line, `PascalCase` types and functions, `m_`/`s_` members, `camelCase`
locals, internals in `NGIN::Reflection::detail`. Export non-inline symbols with
`NGIN_REFLECTION_API` (`Export.hpp`). Mark `noexcept` only when guaranteed.

## Dependencies

Standard library and NGIN.Base only. NGIN.Base is found via
`find_package(NGINBase)` or falls back to `NGIN_BASE_SOURCE_DIR` / the sibling
`../NGIN.Base` source tree. Tests use Catch2 via CPM.

## Verification

```bash
cmake --preset tests
cmake --build --preset tests-debug
ctest --test-dir build/tests -C Debug --output-on-failure
```

Options: `NGIN_REFLECTION_BUILD_TESTS` (ON), `NGIN_REFLECTION_BUILD_EXAMPLES`,
`NGIN_REFLECTION_BUILD_BENCHMARKS` (OFF). Benchmarks live in `benchmarks/`.

- New behavior needs success and failure tests (missing names, conflicts,
  stale handles, overload ambiguity, unload with live instances).
- ABI or module lifecycle changes must exercise `tests/Interop/`.
- Changes visible to NGIN consumers or MetaGen: also validate
  `Examples/Hello.Reflection/` from the workspace per the root `AGENTS.md`.
