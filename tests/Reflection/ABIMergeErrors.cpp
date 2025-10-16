#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/ABIMerge.hpp>
#include <NGIN/Reflection/Reflection.hpp>
#include <NGIN/Reflection/Registry.hpp>

#include <string_view>

#if defined(NGIN_REFLECTION_ENABLE_ABI)

using namespace NGIN::Reflection;

TEST_CASE("RejectsNullRegistryPayload", "[reflection][ABIMerge]")
{
  NGINReflectionRegistryV1 mod{};
  MergeStats stats{};
  const char *err = nullptr;

  const bool ok = MergeRegistryV1(mod, &stats, &err);

  CHECK_FALSE(ok);
  CHECK(stats.modulesMerged == std::uint64_t{0});
  REQUIRE(err != nullptr);
  CHECK(std::string_view{err} == "null registry");
}

TEST_CASE("RejectsUnsupportedVersion", "[reflection][ABIMerge]")
{
  NGINReflectionHeaderV1 hdr{};
  hdr.version = 42u;
  std::uint8_t blob[16]{};

  NGINReflectionRegistryV1 mod{};
  mod.header = &hdr;
  mod.blob = blob;
  mod.blobSize = sizeof(blob);

  MergeStats stats{};
  const char *err = nullptr;

  const bool ok = MergeRegistryV1(mod, &stats, &err);

  CHECK_FALSE(ok);
  CHECK(stats.modulesMerged == std::uint64_t{0});
  REQUIRE(err != nullptr);
  CHECK(std::string_view{err} == "unsupported version");
}

TEST_CASE("RejectsCorruptOffsets", "[reflection][ABIMerge]")
{
  NGINReflectionHeaderV1 hdr{};
  hdr.version = 1u;
  hdr.typeCount = 1;
  hdr.typesOff = 32; // intentionally beyond blob length

  std::uint8_t blob[16]{};
  NGINReflectionRegistryV1 mod{};
  mod.header = &hdr;
  mod.blob = blob;
  mod.blobSize = sizeof(blob);

  MergeStats stats{};
  const char *err = nullptr;

  const bool ok = MergeRegistryV1(mod, &stats, &err);

  CHECK_FALSE(ok);
  CHECK(stats.modulesMerged == std::uint64_t{0});
  REQUIRE(err != nullptr);
  CHECK(std::string_view{err}.starts_with("corrupt offsets"));
}

TEST_CASE("ReportsDuplicateTypeConflicts", "[reflection][ABIMerge]")
{
  (void)TypeOf<int>();

  NGINReflectionRegistryV1 mod{};
  REQUIRE(NGINReflectionExportV1(&mod));

  MergeStats stats{};
  const char *errFirst = nullptr;
  MergeDiagnostics diag{};
  REQUIRE(MergeRegistryV1(mod, &stats, &errFirst, &diag));
  if (diag.HasConflicts())
  {
    REQUIRE(errFirst != nullptr);
    CHECK(std::string_view{errFirst}.starts_with("duplicate typeId"));
    CHECK(diag.typeConflicts.Size() >= NGIN::UIntSize{1});
  }
  else
  {
    CHECK(errFirst == nullptr);
    CHECK(diag.typeConflicts.Size() == NGIN::UIntSize{0});
  }

  const char *errSecond = nullptr;
  REQUIRE(MergeRegistryV1(mod, &stats, &errSecond, &diag));
  REQUIRE(errSecond != nullptr);
  CHECK(std::string_view{errSecond}.starts_with("duplicate typeId"));
  CHECK(stats.typesConflicted >= std::uint64_t{1});
  CHECK(diag.HasConflicts());
  CHECK(diag.typeConflicts.Size() >= NGIN::UIntSize{1});
}

TEST_CASE("CopiesRegistryBlobForHostOwnership", "[reflection][ABIMerge]")
{
  (void)TypeOf<double>();

  NGINReflectionRegistryV1 mod{};
  REQUIRE(NGINReflectionExportV1(&mod));

  RegistryBlobCopy copy{};
  REQUIRE(CopyRegistryBlob(mod, copy));

  auto view = copy.AsRegistry();
  CHECK(view.blob != nullptr);
  CHECK(view.header != nullptr);
  CHECK(view.blob != mod.blob);
  CHECK(view.blobSize == mod.blobSize);
  REQUIRE(view.header != nullptr);
  CHECK(view.header->typeCount == mod.header->typeCount);

  copy.Reset();
  CHECK(copy.sizeBytes == 0);
  CHECK(copy.headerOffset == 0);
  CHECK(copy.data == nullptr);
}

TEST_CASE("VerifyProcessRegistryDefaults", "[reflection][ABIMerge]")
{
  (void)TypeOf<char>();
  const char *err = nullptr;
  VerifyRegistryOptions opts{};
  CHECK(VerifyProcessRegistry(opts, &err));
  CHECK(err == nullptr);
}

namespace
{
  struct VerifyFieldType
  {
    int value{};
    friend void ngin_reflect(Tag<VerifyFieldType>, Builder<VerifyFieldType> &b)
    {
      b.field<&VerifyFieldType::value>("value");
    }
  };

  struct VerifyMethodType
  {
    int Multiply(int v) const { return value * v; }
    friend void ngin_reflect(Tag<VerifyMethodType>, Builder<VerifyMethodType> &b)
    {
      b.method<&VerifyMethodType::Multiply>("Multiply");
    }
    int value{2};
  };
} // namespace

TEST_CASE("VerifyProcessRegistryDetectsInvalidFieldIndex", "[reflection][ABIMerge]")
{
  using namespace NGIN::Reflection::detail;
  (void)TypeOf<VerifyFieldType>();

  auto &reg = GetRegistry();
  const auto typeId = detail::TypeIdOf<VerifyFieldType>();
  auto *idxPtr = reg.byTypeId.GetPtr(typeId);
  REQUIRE(idxPtr != nullptr);
  auto &runtime = reg.types[*idxPtr];

  const auto invalidId = InternNameId("Verify.InvalidField");
  runtime.fieldIndex.Insert(invalidId, static_cast<NGIN::UInt32>(runtime.fields.Size() + 1));

  const char *err = nullptr;
  VerifyRegistryOptions opts{};
  opts.checkFieldIndex = true;
  opts.checkMethodOverloads = false;
  CHECK_FALSE(VerifyProcessRegistry(opts, &err));
  REQUIRE(err != nullptr);
  CHECK(std::string_view{err}.starts_with("field index overflow"));

  runtime.fieldIndex.Remove(invalidId);
}

TEST_CASE("VerifyProcessRegistryDetectsInvalidMethodOverload", "[reflection][ABIMerge]")
{
  using namespace NGIN::Reflection::detail;
  (void)TypeOf<VerifyMethodType>();

  auto &reg = GetRegistry();
  const auto typeId = detail::TypeIdOf<VerifyMethodType>();
  auto *idxPtr = reg.byTypeId.GetPtr(typeId);
  REQUIRE(idxPtr != nullptr);
  auto &runtime = reg.types[*idxPtr];

  const auto overloadId = InternNameId("Verify.InvalidMethod");
  NGIN::Containers::Vector<NGIN::UInt32> indices;
  indices.PushBack(static_cast<NGIN::UInt32>(runtime.methods.Size() + 5));
  runtime.methodOverloads.Insert(overloadId, std::move(indices));

  const char *err = nullptr;
  VerifyRegistryOptions opts{};
  opts.checkFieldIndex = false;
  opts.checkMethodOverloads = true;
  CHECK_FALSE(VerifyProcessRegistry(opts, &err));
  REQUIRE(err != nullptr);
  CHECK(std::string_view{err}.starts_with("method overload index overflow"));

  runtime.methodOverloads.Remove(overloadId);
}

#else
TEST_CASE("ABIMergeErrorsNoopWhenDisabled", "[reflection][ABIMerge]")
{
  CHECK(true);
}
#endif
