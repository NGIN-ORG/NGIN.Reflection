// ABIMerge.hpp — Host-side merge API for ABI V1 blobs (skeleton)
#pragma once

#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Containers/Vector.hpp>

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <string_view>
#include <type_traits>

namespace NGIN::Reflection
{
  enum class MergeEvent : std::uint8_t
  {
    BeginModule = 1,
    TypeAdded = 2,
    TypeConflict = 3,
    Error = 4,
    ModuleComplete = 5,
  };

  struct MergeEventInfo
  {
    const NGINReflectionRegistryV1 *module{nullptr};
    NGIN::UInt64 typeId{0};
    std::string_view existingName{};
    std::string_view incomingName{};
    std::string_view message{};
    std::uint64_t typesAdded{0};
    std::uint64_t typesConflicted{0};
  };

  struct MergeCallbacks
  {
    using EventFn = void (*)(MergeEvent, const MergeEventInfo &, void *);
    EventFn onEvent{nullptr};
    void *userData{nullptr};

    [[nodiscard]] bool HasListener() const noexcept { return onEvent != nullptr; }
  };

  template <class Fn>
  [[nodiscard]] inline MergeCallbacks MakeMergeCallbacks(Fn &fn) noexcept
  {
    using Callable = std::remove_reference_t<Fn>;
    static_assert(std::is_invocable_r_v<void, Fn &, MergeEvent, const MergeEventInfo &>,
                  "Fn must be callable with (MergeEvent, MergeEventInfo)");
    return MergeCallbacks{
        [](MergeEvent ev, const MergeEventInfo &info, void *user) {
          auto *callable = static_cast<Callable *>(user);
          std::invoke(*callable, ev, info);
        },
        const_cast<Callable *>(std::addressof(fn))};
  }

  struct MergeStats
  {
    std::uint64_t modulesMerged{0};
    std::uint64_t typesAdded{0};
    std::uint64_t typesConflicted{0};
  };

  struct MergeConflict
  {
    NGIN::UInt64 typeId{};
    std::string_view existingName{};
    std::string_view incomingName{};
  };

  struct MergeDiagnostics
  {
    NGIN::Containers::Vector<MergeConflict> typeConflicts{};

    void Reset() noexcept { typeConflicts.Clear(); }
    [[nodiscard]] bool HasConflicts() const noexcept { return typeConflicts.Size() != 0; }
  };

  struct RegistryBlobCopy
  {
    std::unique_ptr<std::byte[]> data{};
    std::uint64_t sizeBytes{0};
    std::uint64_t headerOffset{0};

    [[nodiscard]] NGINReflectionRegistryV1 AsRegistry() const noexcept
    {
      if (!data || sizeBytes == 0)
        return {};
      auto *base = data.get();
      auto *header = reinterpret_cast<const NGINReflectionHeaderV1 *>(base + headerOffset);
      return NGINReflectionRegistryV1{header, base, sizeBytes};
    }

    void Reset() noexcept
    {
      data.reset();
      sizeBytes = 0;
      headerOffset = 0;
    }
  };

  inline bool CopyRegistryBlob(const NGINReflectionRegistryV1 &src, RegistryBlobCopy &dst) noexcept
  {
    if (!src.blob || !src.header || src.blobSize == 0)
      return false;

    const auto *blobBase = static_cast<const std::uint8_t *>(src.blob);
    const auto *headerPtr = reinterpret_cast<const std::uint8_t *>(src.header);
    if (headerPtr < blobBase)
      return false;
    const auto offset = static_cast<std::uint64_t>(headerPtr - blobBase);
    if (offset >= src.blobSize)
      return false;

    auto copy = std::unique_ptr<std::byte[]>(new (std::nothrow) std::byte[src.blobSize]);
    if (!copy)
      return false;

    std::memcpy(copy.get(), src.blob, static_cast<std::size_t>(src.blobSize));
    dst.data = std::move(copy);
    dst.sizeBytes = src.blobSize;
    dst.headerOffset = offset;
    return true;
  }

  // Merge an exported ABI registry into the process-local registry.
  // This skeleton validates header fields and is intended to be extended to
  // deduplicate by type_id and reindex handles.
  bool MergeRegistryV1(const NGINReflectionRegistryV1 &module,
                       MergeStats *stats = nullptr,
                       const char **error = nullptr,
                       MergeDiagnostics *diagnostics = nullptr,
                       const MergeCallbacks *callbacks = nullptr) noexcept;

  struct VerifyRegistryOptions
  {
    bool checkFieldIndex{true};
    bool checkMethodOverloads{true};
    bool checkConstructorRanges{false};
  };

  bool VerifyProcessRegistry(const VerifyRegistryOptions &options = {},
                             const char **error = nullptr) noexcept;
} // namespace NGIN::Reflection
