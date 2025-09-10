// ABI.hpp — Versioned C ABI for cross-DLL registry export (Phase 3 layout)
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Reflection/Export.hpp>
#include <cstdint>
#include <expected>

namespace NGIN::Reflection
{
  class Any;
  struct Error;
}

extern "C"
{

  //
  // ABI V1 blob format overview
  // - All pointers are encoded as offsets from the blob base (no raw pointers across modules).
  // - Strings are stored in a single UTF-8 byte array; string references are (offset, size).
  // - Types/Fields/Methods/Ctors/Attributes/ParamTypeIds are stored as tightly-packed arrays.
  // - Index ranges in the type record reference slices within the global arrays.
  //

  // String reference within the blob's string table
  struct NGINReflectionStrRefV1
  {
    std::uint64_t offset; // byte offset into string table (from stringsOff), not NUL-terminated
    std::uint32_t size;   // number of bytes
    std::uint32_t reserved{0};
  };

  // Attribute value kind
  enum class NGINReflectionAttrKindV1 : std::uint8_t
  {
    Bool = 1,
    Int = 2,
    Dbl = 3,
    Str = 4,
    Type = 5 // encoded as typeId (uint64)
  };

  // Attribute record
  struct NGINReflectionAttrV1
  {
    NGINReflectionStrRefV1 key;
    NGINReflectionAttrKindV1 kind; // discriminator
    std::uint8_t pad[7]{0, 0, 0, 0, 0, 0, 0};
    union
    {
      std::uint8_t b8;
      std::int64_t i64;
      double d;
      std::uint64_t typeId;
      NGINReflectionStrRefV1 sref; // valid when kind == Str
    } value;
  };

  // Field record (no function pointers)
  struct NGINReflectionFieldV1
  {
    NGINReflectionStrRefV1 name;
    std::uint64_t typeId;
    std::uint32_t sizeBytes;
    std::uint32_t attrBegin; // index into attributes array
    std::uint32_t attrCount;
    std::uint32_t reserved{0};
  };

  // Method record (invocation pointers are not part of the ABI blob)
  struct NGINReflectionMethodV1
  {
    NGINReflectionStrRefV1 name;
    std::uint64_t returnTypeId; // 0 for void
    std::uint32_t paramBegin;   // index into paramTypeIds array
    std::uint32_t paramCount;
    std::uint32_t attrBegin; // index into attributes array
    std::uint32_t attrCount;
  };

  // Constructor record
  struct NGINReflectionCtorV1
  {
    std::uint32_t paramBegin; // index into paramTypeIds array
    std::uint32_t paramCount;
    std::uint32_t attrBegin; // index into attributes array
    std::uint32_t attrCount;
  };

  // Type record aggregates ranges into global arrays
  struct NGINReflectionTypeV1
  {
    std::uint64_t typeId;
    NGINReflectionStrRefV1 qualifiedName;
    std::uint32_t sizeBytes;
    std::uint32_t alignBytes;

    std::uint32_t fieldBegin;
    std::uint32_t fieldCount;

    std::uint32_t methodBegin;
    std::uint32_t methodCount;

    std::uint32_t ctorBegin;
    std::uint32_t ctorCount;

    std::uint32_t attrBegin;
    std::uint32_t attrCount;
  };

  // Header with counts and offsets to each array within the blob
  struct NGINReflectionHeaderV1
  {
    std::uint32_t version; // 1
    std::uint32_t flags;   // reserved for future use

    // Counts
    std::uint64_t typeCount;
    std::uint64_t fieldCount;
    std::uint64_t methodCount;
    std::uint64_t ctorCount;
    std::uint64_t attributeCount;
    std::uint64_t paramCount;  // total entries in paramTypeIds array
    std::uint64_t stringBytes; // total bytes in string table

    // Offsets from the start of the blob (not from the header address)
    std::uint64_t typesOff;
    std::uint64_t fieldsOff;
    std::uint64_t methodsOff;
    std::uint64_t ctorsOff;
    std::uint64_t attrsOff;
    std::uint64_t paramsOff;  // array of uint64_t typeIds
    std::uint64_t stringsOff; // start of the UTF-8 string table

    // Optional function pointer tables (cross-DLL invocation). When zero, not provided.
    // Layout: arrays parallel to methods/ctors arrays (same counts and order)
    std::uint64_t methodInvokeOff;  // array of NGINReflectionMethodInvokeFnV1
    std::uint64_t ctorConstructOff; // array of NGINReflectionCtorConstructFnV1

    // Total blob size (header + arrays + strings). Useful for copying.
    std::uint64_t totalSize;
  };

  // Exported registry surface: header pointer and blob base/size
  struct NGINReflectionRegistryV1
  {
    const NGINReflectionHeaderV1 *header; // points into blob memory
    const void *blob;                     // base pointer to data blob
    std::uint64_t blobSize;               // size in bytes of the blob
  };

#if defined(NGIN_REFLECTION_ENABLE_ABI)
  // Cross-DLL invocation function types for V1
  using NGINReflectionMethodInvokeFnV1 = std::expected<NGIN::Reflection::Any, NGIN::Reflection::Error> (*)(void *, const NGIN::Reflection::Any *, NGIN::UIntSize);
  using NGINReflectionCtorConstructFnV1 = std::expected<NGIN::Reflection::Any, NGIN::Reflection::Error> (*)(const NGIN::Reflection::Any *, NGIN::UIntSize);

  // Exported entrypoint that a module exposes for host to import
  // Returns true on success and fills out registry.
  // Note: On Windows this must be dllexport'ed from the plugin so that the host
  // can locate it via GetProcAddress. We apply NGIN_REFLECTION_API here so
  // plugin builds (defining NGIN_REFLECTION_EXPORTS) export the symbol.
  NGIN_REFLECTION_API bool NGINReflectionExportV1(NGINReflectionRegistryV1 *out);
#endif

} // extern "C"
