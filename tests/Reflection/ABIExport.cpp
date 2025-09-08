#include <boost/ut.hpp>

#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/ABIMerge.hpp>

using namespace boost::ut;
using namespace NGIN::Reflection;

#if defined(NGIN_REFLECTION_ENABLE_ABI)
suite<"ABI.Export"> abiExportSuite = [] {
  "Exports V1 blob with arrays"_test = [] {
    // Ensure at least one type is present
    (void)TypeOf<int>();

    NGINReflectionRegistryV1 mod{};
    expect(NGINReflectionExportV1(&mod)) << "export returns true";
    expect(mod.header != nullptr) << "header present";
    expect(mod.blob != nullptr) << "blob present";
    expect(mod.blobSize > 0_u64) << "blob non-empty";

    const auto &h = *mod.header;
    expect(h.version == 1_u32);
    // Sections must lie within the blob
    auto within = [&](std::uint64_t off, std::uint64_t sz) {
      if (sz == 0) return true;
      return off + sz <= mod.blobSize;
    };
    expect(within(h.typesOff, h.typeCount * sizeof(NGINReflectionTypeV1)));
    expect(within(h.fieldsOff, h.fieldCount * sizeof(NGINReflectionFieldV1)));
    expect(within(h.methodsOff, h.methodCount * sizeof(NGINReflectionMethodV1)));
    expect(within(h.ctorsOff, h.ctorCount * sizeof(NGINReflectionCtorV1)));
    expect(within(h.attrsOff, h.attributeCount * sizeof(NGINReflectionAttrV1)));
    expect(within(h.paramsOff, h.paramCount * sizeof(std::uint64_t)));
    expect(within(h.stringsOff, h.stringBytes));

    // Try skeleton merge
    MergeStats stats{};
    const char *err = nullptr;
    expect(MergeRegistryV1(mod, &stats, &err)) << (err ? err : "");
    expect(stats.modulesMerged == 1_u64);
  };
};
#else
suite<"ABI.Export"> abiExportSuite = [] {
  "ABI disabled"_test = [] {
    expect(true);
  };
};
#endif
