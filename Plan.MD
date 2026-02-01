# Phase Plan — NGIN.Reflection

This document tracks forward-looking work. All items are additive and should avoid regressing existing APIs/performance.

---

## Phase 4 — Attributes & Codegen Hooks

### Objective
Deliver a production-quality attribute story and automated codegen pipeline that scales to large codebases and survives DLL boundaries.

### Milestones
1. **Attribute Storage Redesign**
   - Define final `AttributeValue` variant (bool / int64 / double / string_view / TypeId / span<byte>).
   - Add ergonomic query APIs on `Type`, `Field`, `Method` (`HasAttribute(key)`, `GetAttribute(key)`, iteration helpers).
   - Persist attributes through `NGINReflectionExportV1` blob with stable key IDs and ABI documentation.
   - Extend diagnostics/tests covering attribute round-tripping (single module + cross-DLL).

2. **Scanner & Codegen Prototype (`ngin-reflect-scan`)**
   - Draft design doc: input model, expected outputs, integration points, performance goals.
   - Implement libclang-based scanner that discovers `[[using ngin::reflect(... )]]` vendor attributes and type definitions.
   - Generate `*.ngin.reflect.hpp` shims containing `ngin_reflect`/`Describe<T>` bodies, attribute registrations, and optional metadata tables.
   - Provide incremental-safe CMake integration (custom command + target) and sample CI cache strategy.

3. **Authoring Workflow & Docs**
   - Write end-user guide: declaring attributes, overriding generated descriptors, mixing manual + generated code, handling merges.
  - Supply reference examples (small library + DLL) demonstrating scanner usage and attribute queries.
   - Publish troubleshooting/FAQ for scanner diagnostics and attribute schema evolution.

### Work Breakdown
- [ ] Attribute storage RFC (variant shape, ABI layout, migration plan).
- [ ] Implement storage changes + runtime APIs with unit/integration tests.
- [ ] Update ABI writer/reader (`ABI.cpp`, `ABIMerge.cpp`) for attribute payloads.
- [ ] Verify cross-module attribute round-trip via existing interop tests (extend as needed).
- [ ] Scanner prototype (command-line skeleton, libclang traversal).
- [ ] Codegen output format + validation tests (ensuring generated code compiles).
- [ ] CMake integration module (`NGINReflectionScanner.cmake`) with user toggles.
- [ ] Documentation: README/Architecture updates, dedicated `docs/Attributes.md`, sample projects.

### Dependencies / Risks
- Requires agreement on attribute schema before ABI changes ship.
- Scanner depends on libclang availability; plan fallback for non-clang toolchains (skip or stub).
- Must avoid significant compile-time regressions; monitor with sample projects.

---

## Phase 5 — Performance & Memory Polish (Preview)

- Profile registry hot paths post-attribute changes (name lookup, invoke, merge).
- Evaluate alternative hash/allocator strategies (e.g., arena for descriptors).
- Investigate lock-free reader model once merge is a one-time cost per process.

---

## Phase 6 — Documentation & Samples (Preview)

- Expand examples suite (attributes, DLL, codegen integration, tooling).
- Produce migration guide for adopters moving from manual descriptors to scanner-driven workflows.
- Polish API reference (doxygen/DocCA) synced with attribute APIs.

