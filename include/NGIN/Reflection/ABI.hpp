#pragma once

#include <NGIN/Reflection/Export.hpp>

#include <cstddef>
#include <cstdint>

extern "C"
{
  enum NGINReflectionStatusCode : std::uint32_t
  {
    NGINReflectionStatus_Ok = 0,
    NGINReflectionStatus_NotFound = 1,
    NGINReflectionStatus_InvalidArgument = 2,
    NGINReflectionStatus_StaleHandle = 3,
    NGINReflectionStatus_AbiMismatch = 4,
    NGINReflectionStatus_Conflict = 5,
    NGINReflectionStatus_CorruptModule = 6,
    NGINReflectionStatus_InternalError = 7,
  };

  struct NGINReflectionStringView
  {
    const char *data;
    std::uint64_t size;
  };

  struct NGINReflectionSpan
  {
    const void *data;
    std::uint64_t count;
  };

  struct NGINReflectionStatus
  {
    std::uint32_t code;
    NGINReflectionStringView message;
  };

  struct NGINReflectionAbiHeader
  {
    std::uint32_t family;
    std::uint32_t version;
  };

  struct NGINReflectionModuleIdentity
  {
    std::uint32_t moduleNameSymbol;
    std::uint32_t abiFamily;
    std::uint32_t abiVersion;
    std::uint32_t reserved;
    std::uint64_t keyHash;
  };

  struct NGINReflectionTypeIdentity
  {
    NGINReflectionModuleIdentity module;
    std::uint32_t qualifiedNameSymbol;
    std::uint32_t reserved;
    std::uint64_t signatureHash;
    std::uint64_t keyHash;
  };

  struct NGINReflectionTypeRef
  {
    std::uint32_t qualifiedNameSymbol;
    std::uint32_t reserved;
    std::uint64_t fastHash;
    std::uint64_t reflectedTypeKey;
  };

  struct NGINReflectionInstanceHandle;

  struct NGINReflectionInstanceVTable
  {
    void (*retain)(NGINReflectionInstanceHandle *instance);
    void (*release)(NGINReflectionInstanceHandle *instance);
    const void *(*getConst)(const NGINReflectionInstanceHandle *instance);
    void *(*getMut)(const NGINReflectionInstanceHandle *instance);
  };

  struct NGINReflectionInstanceHandle
  {
    void *object;
    void *userData;
    const NGINReflectionInstanceVTable *vtable;
    NGINReflectionTypeIdentity identity;
  };

  enum NGINReflectionValueKind : std::uint32_t
  {
    NGINReflectionValue_Empty = 0,
    NGINReflectionValue_Bool = 1,
    NGINReflectionValue_Int64 = 2,
    NGINReflectionValue_UInt64 = 3,
    NGINReflectionValue_Float64 = 4,
    NGINReflectionValue_String = 5,
    NGINReflectionValue_Symbol = 6,
    NGINReflectionValue_TypeIdentity = 7,
    NGINReflectionValue_Instance = 8,
  };

  struct NGINReflectionValue
  {
    std::uint32_t kind;
    std::uint32_t flags;
    union
    {
      std::uint8_t boolValue;
      std::int64_t intValue;
      std::uint64_t uintValue;
      double floatValue;
      NGINReflectionStringView stringValue;
      std::uint32_t symbolValue;
      NGINReflectionTypeIdentity typeIdentity;
      NGINReflectionInstanceHandle instanceValue;
    };
    void (*release)(NGINReflectionValue *value);
    void *releaseUserData;
  };

  struct NGINReflectionAttributeDesc
  {
    std::uint32_t nameSymbol;
    std::uint32_t reserved;
    NGINReflectionValue value;
  };

  struct NGINReflectionFieldDesc
  {
    std::uint32_t nameSymbol;
    std::uint32_t reserved;
    NGINReflectionTypeRef valueType;
    std::uint32_t attributeBegin;
    std::uint32_t attributeCount;
    std::uint32_t readSlot;
    std::uint32_t writeSlot;
  };

  struct NGINReflectionPropertyDesc
  {
    std::uint32_t nameSymbol;
    std::uint32_t reserved;
    NGINReflectionTypeRef valueType;
    std::uint32_t attributeBegin;
    std::uint32_t attributeCount;
    std::uint32_t readSlot;
    std::uint32_t writeSlot;
  };

  struct NGINReflectionMethodDesc
  {
    std::uint32_t nameSymbol;
    std::uint32_t reserved;
    NGINReflectionTypeRef returnType;
    std::uint32_t parameterBegin;
    std::uint32_t parameterCount;
    std::uint32_t attributeBegin;
    std::uint32_t attributeCount;
    std::uint32_t invokeSlot;
    std::uint32_t isConst;
  };

  struct NGINReflectionConstructorDesc
  {
    std::uint32_t parameterBegin;
    std::uint32_t parameterCount;
    std::uint32_t attributeBegin;
    std::uint32_t attributeCount;
    std::uint32_t constructSlot;
    std::uint32_t reserved;
  };

  struct NGINReflectionEnumValueDesc
  {
    std::uint32_t nameSymbol;
    std::uint32_t reserved;
    std::int64_t signedValue;
    std::uint64_t unsignedValue;
  };

  struct NGINReflectionBaseDesc
  {
    NGINReflectionTypeIdentity baseIdentity;
    std::uint32_t upcastSlot;
    std::uint32_t downcastSlot;
  };

  struct NGINReflectionTypeDesc
  {
    NGINReflectionTypeIdentity identity;
    std::uint64_t sizeBytes;
    std::uint64_t alignBytes;
    std::uint32_t fieldBegin;
    std::uint32_t fieldCount;
    std::uint32_t propertyBegin;
    std::uint32_t propertyCount;
    std::uint32_t methodBegin;
    std::uint32_t methodCount;
    std::uint32_t ctorBegin;
    std::uint32_t ctorCount;
    std::uint32_t enumBegin;
    std::uint32_t enumCount;
    std::uint32_t baseBegin;
    std::uint32_t baseCount;
    std::uint32_t attributeBegin;
    std::uint32_t attributeCount;
  };

  struct NGINReflectionFunctionDesc
  {
    std::uint32_t nameSymbol;
    std::uint32_t reserved;
    NGINReflectionTypeRef returnType;
    std::uint32_t parameterBegin;
    std::uint32_t parameterCount;
    std::uint32_t attributeBegin;
    std::uint32_t attributeCount;
    std::uint32_t invokeSlot;
    std::uint32_t reserved2;
  };

  typedef NGINReflectionStatus (*NGINReflectionFieldReadFn)(const NGINReflectionInstanceHandle *instance,
                                                            NGINReflectionValue *outValue);
  typedef NGINReflectionStatus (*NGINReflectionFieldWriteFn)(NGINReflectionInstanceHandle *instance,
                                                             const NGINReflectionValue *value);
  typedef NGINReflectionStatus (*NGINReflectionPropertyReadFn)(const NGINReflectionInstanceHandle *instance,
                                                               NGINReflectionValue *outValue);
  typedef NGINReflectionStatus (*NGINReflectionPropertyWriteFn)(NGINReflectionInstanceHandle *instance,
                                                                const NGINReflectionValue *value);
  typedef NGINReflectionStatus (*NGINReflectionMethodInvokeFn)(const NGINReflectionInstanceHandle *instance,
                                                               const NGINReflectionValue *arguments,
                                                               std::uint64_t argumentCount,
                                                               NGINReflectionValue *outValue);
  typedef NGINReflectionStatus (*NGINReflectionConstructorInvokeFn)(const NGINReflectionValue *arguments,
                                                                    std::uint64_t argumentCount,
                                                                    NGINReflectionInstanceHandle *outInstance);
  typedef NGINReflectionStatus (*NGINReflectionFunctionInvokeFn)(const NGINReflectionValue *arguments,
                                                                 std::uint64_t argumentCount,
                                                                 NGINReflectionValue *outValue);
  typedef NGINReflectionStatus (*NGINReflectionBaseCastFn)(const NGINReflectionInstanceHandle *instance,
                                                           NGINReflectionInstanceHandle *outInstance);

  struct NGINReflectionModuleApi
  {
    NGINReflectionAbiHeader header;
    NGINReflectionModuleIdentity identity;

    const NGINReflectionStringView *symbols;
    std::uint64_t symbolCount;

    const NGINReflectionAttributeDesc *attributes;
    std::uint64_t attributeCount;

    const NGINReflectionTypeRef *parameters;
    std::uint64_t parameterCount;

    const NGINReflectionFieldDesc *fields;
    std::uint64_t fieldCount;

    const NGINReflectionPropertyDesc *properties;
    std::uint64_t propertyCount;

    const NGINReflectionMethodDesc *methods;
    std::uint64_t methodCount;

    const NGINReflectionConstructorDesc *constructors;
    std::uint64_t constructorCount;

    const NGINReflectionEnumValueDesc *enumValues;
    std::uint64_t enumValueCount;

    const NGINReflectionBaseDesc *bases;
    std::uint64_t baseCount;

    const NGINReflectionTypeDesc *types;
    std::uint64_t typeCount;

    const NGINReflectionFunctionDesc *functions;
    std::uint64_t functionCount;

    const NGINReflectionFieldReadFn *fieldReaders;
    std::uint64_t fieldReaderCount;

    const NGINReflectionFieldWriteFn *fieldWriters;
    std::uint64_t fieldWriterCount;

    const NGINReflectionPropertyReadFn *propertyReaders;
    std::uint64_t propertyReaderCount;

    const NGINReflectionPropertyWriteFn *propertyWriters;
    std::uint64_t propertyWriterCount;

    const NGINReflectionMethodInvokeFn *methodInvokers;
    std::uint64_t methodInvokerCount;

    const NGINReflectionConstructorInvokeFn *constructorsInvokers;
    std::uint64_t constructorInvokerCount;

    const NGINReflectionFunctionInvokeFn *functionInvokers;
    std::uint64_t functionInvokerCount;

    const NGINReflectionBaseCastFn *upcasters;
    std::uint64_t upcasterCount;

    const NGINReflectionBaseCastFn *downcasters;
    std::uint64_t downcasterCount;

    void (*release)(NGINReflectionModuleApi *api);
    void *userData;
  };

  NGIN_REFLECTION_API bool NGINReflectionGetModuleApi(NGINReflectionModuleApi *out);
}
