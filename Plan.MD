# Reflection Overhaul Status

The previous hot-reload refactor plan described the old ABI V1 evolution path.
That path has been replaced by the current runtime documented in:

- `README.md`
- `docs/Architecture.md`

The active implementation now uses:

- structured identities from `NGIN.Base`
- a unified local/imported descriptor surface
- snapshot-based registry replacement
- `NGINReflectionModuleApi` instead of the removed blob export/merge path

Any future planning should start from the current architecture rather than the retired ABI V1 roadmap.
