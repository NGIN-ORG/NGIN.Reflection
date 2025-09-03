# NGIN.Reflection – CoPilot / LLM Contribution Instructions

Guidance for automated and human contributors to extend the Reflection library safely, consistently, and sustainably.

---

## Purpose & Scope

NGIN.Reflection provides small, composable utilities around type information and reflection‑adjacent facilities that build on NGIN.Base (e.g., `NGIN::Meta::TypeName`). It is intended to be lightweight, mostly header‑first, and safe to consume as a library target `NGIN::Reflection` from CMake.

This doc adapts the conventions from NGIN.Base to this repository’s structure and goals.

---

## Code Philosophy

High‑level principles for this repo:

1. Header‑first APIs: Prefer templates, `constexpr`/`consteval`, and inline functions in headers under `include/NGIN/Reflection/`. Use `.cpp` only when an ABI boundary or platform hook is required.
2. Modern C++: Require C++23 and prefer compile‑time computation where it enhances clarity and performance.
3. ODR / ABI safety: Keep non‑inline symbols minimal. When you must expose non‑inline functions, annotate with `NGIN_REFLECTION_API` (see `include/NGIN/Reflection/Export.hpp`).
4. Determinism: Avoid hidden global state. Favor pure, stateless helpers.
5. Allocation discipline: Avoid heap allocations in core logic.
6. Exception policy: Throw only standard exceptions. Use `noexcept` when accurate. Use assertions for programmer errors.
7. Zero surprise: Follow existing patterns and naming in this repo and in NGIN.Base.
8. Evolvability: Prefer additive changes. Clearly mark experimental bits in comments or separate headers.

---

## Code Style & Namespacing

Namespaces:
- All public code under `NGIN::Reflection`. Use `NGIN::Reflection::detail` for implementation internals in headers. Avoid anonymous namespaces in headers.

Formatting & Layout:
- Respect repository `.clang-format`. Re‑run after edits.
- 4 spaces, no tabs; lines ≤ ~120 chars when feasible.
- Brace on same line: `constexpr auto Foo() {` / `class Bar {`.

Qualifiers & Keywords:
- Prefer `constexpr`/`consteval`/`constinit` when semantically correct.
- Mark non‑throwing functions `noexcept` only when guaranteed.
- Mark derived virtuals `final`/`override` explicitly when present (rare here).

Naming:
- Types/templates: `PascalCase` (e.g., `MemberInfo`, `TypeDescriptor`).
- Functions/methods: `PascalCase` (e.g., `Describe`, `VisitFields`).
- Members: `m_foo`; statics: `s_bar`; locals: `camelCase`.
- Concepts/traits: `...Concept` / `...Traits` when clarifying.

---

## Library Structure

- Public headers: `include/NGIN/Reflection/*.hpp` (e.g., `Reflection.hpp`, `Export.hpp`).
- Implementation `.cpp`: only when crossing ABI boundaries; annotate exports with `NGIN_REFLECTION_API`.
- CMake target: `NGIN::Reflection` (alias for `NGIN.Reflection`).
- Platform defines: injected by `CMakeLists.txt` (`NGIN_PLATFORM` and `NGIN_REFLECTION_*`).

Dependency on NGIN.Base:
- Linked publicly. You may use facilities like `NGIN::Meta::TypeName` in public headers where it does not inflate compile times excessively.
- Do not add new runtime dependencies beyond the standard library and NGIN.Base without prior discussion.

---

## Build & Options

Top‑level options in this repo:
- `NGIN_REFLECTION_BUILD_TESTS` (ON by default)
- `NGIN_REFLECTION_BUILD_EXAMPLES` (OFF by default)

NGIN.Base resolution in `CMakeLists.txt`:
1. Try `find_package(NGINBase CONFIG QUIET)`.
2. Fallback to sibling source `../NGIN.Base` or `NGIN_BASE_SOURCE_DIR` if provided.

Install/export is configured; consumers can use `find_package(NGINReflection)` when installed.

---

## Tests

- Framework: Boost.UT (`boost::ut`) via CPM (see `tests/CMakeLists.txt`).
- Each `.cpp` (excluding `tests/main.cpp`) becomes its own test executable.
- Test discovery: `cmake/BoostUTAddTests.cmake` registers each Boost.UT case with CTest, prefixed by the detected suite.
- Naming: use a suite per file, e.g.:

```cpp
using namespace boost::ut;
suite<"NGIN::Reflection"> suiteName = [] {
  "LibraryName"_test = [] { /* ... */ };
};
```

Minimum coverage for new features:
- Positive and negative cases.
- Boundary conditions (empty, moved‑from, max sizes, alignment), and error paths.
- If performance sensitive, add a benchmark to NGIN.Base or a local `benchmarks/` (kept out of public headers).

Running tests (examples):
- Configure with presets or standard CMake, then `ctest --output-on-failure` in the build tree.

---

## Patterns & APIs (Reflection‑specific)

- Prefer compile‑time descriptions: trait types, constexpr tables, and CTAD helpers over runtime registries.
- If adding utilities like field enumeration or attribute tagging, keep APIs composable and opt‑in (SFINAE / concepts).
- Avoid RTTI‑heavy designs; prefer traits/concepts and constexpr dispatch.
- Keep cross‑module types serializable only when absolutely necessary; prefer exposing metadata accessors.

Export/visibility:
- Use `NGIN_REFLECTION_API` for non‑inline functions intended for external linkage.
- Keep templates and small inline functions in headers; avoid anonymous namespaces in headers.

---

## Performance & Quality

- Move work to compile time when it doesn’t obscure intent.
- Avoid dynamic allocation in hot paths; use SBO/stack.
- Keep trivial functions inline; guard `noexcept` accuracy.
- Minimize includes in public headers; forward declare where possible.

---

## Documentation

- Add brief doxygen‑style summaries for public types/functions.
- Document invariants and non‑obvious rationale rather than restating code.
- If adding a substantial module, include a focused README next to headers, e.g. `include/NGIN/Reflection/README.md` with:
  1) Purpose & Scope, 2) Key Types/Concepts, 3) Examples, 4) Performance Notes, 5) Extension Points, 6) Test Guidance.

---

## Adding New Features (Checklist)

1. Correct namespace and header placement under `include/NGIN/Reflection/`.
2. Keep public headers lean; forward declare where possible.
3. Tests: add positive + negative cases; include edge coverage.
4. Examples: extend `examples/QuickStart` if it improves discoverability.
5. Formatting & static analysis: clang‑format; clang‑tidy as applicable; no new warnings.
6. Ensure `noexcept`/exception‑safety guarantees are correct.
7. Avoid dependency creep beyond NGIN.Base and the standard library.
8. If adding non‑inline functions, annotate with `NGIN_REFLECTION_API` and justify the ABI boundary in PR notes.

---

## Commit / PR Guidance

Title: imperative, concise (e.g., "Add TypeDescriptor for aggregates").

Body should cover:
- Motivation / problem
- Solution summary (algorithms, key types)
- Tests & verification (mention benchmarks if any)
- Follow‑ups / limitations

Diff hygiene:
- Keep changes minimal; avoid mixed refactors with functional changes.
- Prefer introducing helpers over duplicating logic.

---

## AI & Automated Suggestions

- Do not widen or tighten exception specs without proof.
- Preserve observable behavior unless changing it is the goal.
- Keep symbol visibility correct; do not introduce global singletons.
- Align with NGIN.Base conventions for naming and structure.

---

## License

Apache 2.0 (see `LICENSE` in the root of the organization’s repositories where applicable). Preserve license notices and attribution.

---

Adhere to these guidelines to keep NGIN.Reflection consistent, safe, and maintainable.

