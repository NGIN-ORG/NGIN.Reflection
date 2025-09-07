// ABI.hpp — Versioned C ABI for cross-DLL registry export (Phase 3 skeleton)
#pragma once

#include <NGIN/Primitives.hpp>
#include <cstdint>

extern "C" {

struct NGINReflectionHeaderV1 {
  std::uint32_t version;  // 1
  std::uint32_t reserved; // alignment
  std::uint64_t type_count;
  std::uint64_t field_count;
  std::uint64_t method_count;
  std::uint64_t attribute_count;
};

// Future: pointers/offsets to contiguous descriptor arrays; indices for handles.
struct NGINReflectionRegistryV1 {
  const NGINReflectionHeaderV1 *header;
  const void *blob; // base pointer to data blob
};

#if defined(NGIN_REFLECTION_ENABLE_ABI)
// Exported entrypoint that a module exposes for host to import
// Returns true on success and fills out registry.
bool NGINReflectionExportV1(NGINReflectionRegistryV1 *out);
#endif

} // extern "C"
