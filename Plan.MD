# Hot Reload Refactor Plan (NGIN.Reflection)

## Goals
- Support hot-reload of script modules without stale metadata or dangling function pointers.
- Preserve performance (fast lookup, minimal overhead on steady-state queries).
- Keep usage simple for hosts and module authors.
- Allow a clean, breaking redesign now, then stabilize a single ABI surface.

## Current Issues (Why Hot Reload Is Risky Today)
1) No module ownership tracking
   - The registry has no per-module boundaries, so unload/reload cannot remove or replace entries.
2) Conflict handling skips new data
   - Merge conflicts only increment a counter; updated types are ignored.
3) Function pointer lifetime is tied to module
   - Method/ctor function pointers become invalid after module unload, but registry retains them.
4) No ABI compatibility checks beyond basic bounds
   - A type with the same typeId but different layout/signatures can silently mismatch.
5) No concurrency strategy for live reload
   - Registry updates are not coordinated with reads/invokes.

## Design Principles
- Breaking changes allowed now: prioritize a clean hot-reload model, then stabilize the public surface.
- Keep hot-reload capability behind a clear module API with minimal extra work for users.
- Favor stable handles + indirection over storing raw function pointers in core metadata.

## Proposed Architecture (High-Level)
1) Module-aware registry
   - Track moduleId for each registered type and for any function-pointer slots.
   - Provide APIs to unload or replace a module's contributions.

2) Replace/Update path in merge
   - Introduce a MergeMode: AppendOnly (current behavior), ReplaceOnConflict, or RejectOnConflict.
   - For hot reload, ReplaceOnConflict is default for known moduleId.

3) Indirection table for invocations
   - Store Invoke/Construct pointers in a per-module function table.
   - Runtime method records store an index into that table rather than the raw pointer.
   - On reload, swap out the function table for the module.

4) Compatibility checks
   - Add optional ABI hash/version checks per type (size, align, signature hash, attributes hash).
   - Fail merge if mismatched and ReplaceOnConflict is requested without override.

5) Concurrency strategy
   - Choose a single model: snapshot/RCU-style view (preferred) or RW lock.
   - Reload path produces a new snapshot or uses write lock, then swaps.

6) Handle invalidation
   - Use (index + generation) or epoch-based handles so stale handles fail fast after reload.

7) String ownership
   - Track string ownership per module or ref-counted interning to avoid leaks on unload.

## Detailed Implementation Steps

### Phase 0: Inventory and Baseline
- Identify all registry mutation points and public APIs that expose handles.
- Decide on a stable type/method identifier strategy (stable typeId + signature hash or hash-in-id).
- Add basic test harness for interop module load/unload (Host + PluginA/B).
 - Choose concurrency model (snapshot/RCU vs RW lock) and document invariants.

### Phase 1: Module Ownership Tracking
- Add moduleId to TypeRuntimeDesc (and any internal structures that need ownership).
- Extend ModuleRegistration to carry moduleId and store it in registry entries.
- Update registry lookups to optionally filter by moduleId (for debugging and tooling).
 - Add per-module or ref-counted string interning policy.

### Phase 2: Merge Modes and Replacement
- Add a MergeOptions struct: { mode, allowAbiMismatch, moduleId }.
- Update MergeRegistryV1 to accept options (breaking change allowed).
- On conflict with same moduleId:
  - Remove old entries (including overload maps and name indices).
  - Insert replacement entries.
- On conflict with different moduleId:
  - Keep old entries and count conflicts (current behavior).

### Phase 3: Indirection for Function Pointers
- Introduce a module-scoped function table:
  - Store method/ctor pointers in a per-module array.
  - Replace MethodRuntimeDesc::Invoke and CtorRuntimeDesc::Construct with an index + moduleId.
- Add a fast dispatch helper that resolves (moduleId, index) to a callable pointer.
- Update ABI export to emit function table indices (or a table per module) instead of raw pointers.

### Phase 4: ABI Compatibility Checks
- Add a type signature hash in registry (size, align, return/param types, member names).
- When merging, compare hashes for same typeId if ReplaceOnConflict is requested.
- Provide a MergeOptions flag to allow or reject mismatches.

### Phase 5: Hot-Reload Safe Registry Update
- Implement the chosen concurrency strategy:
  - Snapshot: build new registry state on merge/unload, then atomically swap pointer.
  - RW lock: invocation takes shared lock; merge/unload takes exclusive lock.
 - Add handle generation/epoch checks for invalidation.

### Phase 6: Unload/Reload APIs
- Add public APIs:
  - UnregisterModule(moduleId)
  - ReplaceModule(moduleId, registryBlob)
  - QueryModuleStats(moduleId)
- Ensure ModuleInit provides a stable moduleId (string name -> hashed id).

### Phase 7: Tests and Examples
- Add tests for:
  - ReplaceOnConflict behavior
  - Function pointer validity after reload
  - UnregisterModule cleans indices and overload maps
  - ABI mismatch detection
  - Handle invalidation after unload/reload
  - String interning cleanup after unload
- Add example: hot reload loop that replaces plugin metadata.

## Performance Considerations
- Indirection adds one extra pointer lookup; keep it cache-friendly by using contiguous tables.
- Snapshot swap is O(1) for readers; merge cost is bounded by module size.
- Optional ABI checks can be disabled in release builds or for trusted modules.

## Migration Strategy
- Breaking change now: introduce a single new ABI surface and update all call sites.
- Remove legacy fallback paths to keep the runtime lean.

## Open Questions
- Is moduleId best derived from module name, GUID, or build hash?
- Should typeId remain stable across recompiles, or incorporate signature hash to avoid collisions?
- Do we need versioning per module to prevent stale calls during reload?

## Milestones
- M1: Module ownership + merge modes.
- M2: Function indirection + ABI V2 export/import.
- M3: Unregister/replace APIs + tests.
- M4: Concurrency strategy finalized and documented.
