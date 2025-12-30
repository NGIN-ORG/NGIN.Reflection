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

  struct MergeOptions
  {
    ModuleId moduleId{0};
    enum class MergeMode : std::uint8_t
    {
      AppendOnly = 0,
      ReplaceOnConflict = 1,
      RejectOnConflict = 2
    };
    MergeMode mode{MergeMode::AppendOnly};
  };

  // Merge an exported ABI registry into the process-local registry.
  // Validates the blob, merges by type_id, and tracks conflicts.
  bool MergeRegistryV1(const NGINReflectionRegistryV1 &module,
                       MergeStats *stats = nullptr,
                       const char **error = nullptr) noexcept;

  // Merge with explicit options (module ownership, etc).
  bool MergeRegistryV1(const NGINReflectionRegistryV1 &module,
                       const MergeOptions &options,
                       MergeStats *stats = nullptr,
                       const char **error = nullptr) noexcept;
} // namespace NGIN::Reflection
