// ABIMerge.hpp - Host-side merge API for ABI V1 blobs.
#pragma once

#include <NGIN/Reflection/ABI.hpp>

namespace NGIN::Reflection
{
  struct MergeStats
  {
    std::uint64_t modulesMerged{0};
    std::uint64_t typesAdded{0};
    std::uint64_t typesConflicted{0};
  };

  // Merge an exported ABI registry into the process-local registry.
  // Validates the blob, merges by type_id, and tracks conflicts.
  bool MergeRegistryV1(const NGINReflectionRegistryV1 &module,
                       MergeStats *stats = nullptr,
                       const char **error = nullptr) noexcept;
} // namespace NGIN::Reflection
