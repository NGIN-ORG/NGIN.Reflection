// ABIMerge.hpp — Host-side merge API for ABI V1 blobs (skeleton)
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
  // This skeleton validates header fields and is intended to be extended to
  // deduplicate by type_id and reindex handles.
  bool MergeRegistryV1(const NGINReflectionRegistryV1 &module,
                       MergeStats *stats = nullptr,
                       const char **error = nullptr) noexcept;
} // namespace NGIN::Reflection
