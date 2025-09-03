// ABI.hpp — Versioned C ABI for cross-DLL registry export (Phase 3 skeleton)
#pragma once

#include <NGIN/Primitives.hpp>
#include <cstdint>

extern "C" {

struct ngin_refl_header_v1 {
  std::uint32_t version;     // 1
  std::uint32_t reserved;    // alignment
  std::uint64_t type_count;
  std::uint64_t field_count;
  std::uint64_t method_count;
  std::uint64_t attribute_count;
};

// Future: pointers/offsets to contiguous descriptor arrays; indices for handles.
struct ngin_refl_registry_v1 {
  const ngin_refl_header_v1* header;
  const void*                blob;     // base pointer to data blob
};

// Exported entrypoint that a module exposes for host to import
// Returns true on success and fills out registry.
bool ngin_reflection_export_v1(ngin_refl_registry_v1* out);

} // extern "C"

