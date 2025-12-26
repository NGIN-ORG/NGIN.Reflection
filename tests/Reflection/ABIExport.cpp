#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/ABIMerge.hpp>
#include <NGIN/Reflection/Registry.hpp>

#include <cstdint>

using namespace NGIN::Reflection;

#if defined(NGIN_REFLECTION_ENABLE_ABI)
TEST_CASE("YieldsMergeableRegistry", "[reflection][ABIExport]")
{
  (void)GetType<int>();

  NGINReflectionRegistryV1 mod{};
  INFO("export returns true");
  CHECK(NGINReflectionExportV1(&mod));
  INFO("header present");
  REQUIRE(mod.header != nullptr);
  INFO("blob present");
  REQUIRE(mod.blob != nullptr);
  INFO("blob non-empty");
  REQUIRE(mod.blobSize > 0);

  const auto &h = *mod.header;
  CHECK(h.version == std::uint32_t{1});

  auto within = [&](std::uint64_t off, std::uint64_t sz)
  {
    if (sz == 0)
      return true;
    return off + sz <= mod.blobSize;
  };

  CHECK(within(h.typesOff, h.typeCount * sizeof(NGINReflectionTypeV1)));
  CHECK(within(h.fieldsOff, h.fieldCount * sizeof(NGINReflectionFieldV1)));
  CHECK(within(h.methodsOff, h.methodCount * sizeof(NGINReflectionMethodV1)));
  CHECK(within(h.ctorsOff, h.ctorCount * sizeof(NGINReflectionCtorV1)));
  CHECK(within(h.attrsOff, h.attributeCount * sizeof(NGINReflectionAttrV1)));
  CHECK(within(h.paramsOff, h.paramCount * sizeof(std::uint64_t)));
  CHECK(within(h.stringsOff, h.stringBytes));

  MergeStats stats{};
  const char *err = nullptr;
  const bool mergeOk = MergeRegistryV1(mod, &stats, &err);
  INFO((err ? err : ""));
  CHECK(mergeOk);
  CHECK(stats.modulesMerged == std::uint64_t{1});
}
#else
TEST_CASE("DisabledFallback", "[reflection][ABIExport]")
{
  CHECK(true);
}
#endif
