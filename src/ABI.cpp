#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/Registry.hpp>

using namespace NGIN::Reflection;
using namespace NGIN::Reflection::detail;

extern "C" bool NGINReflectionExportV1(NGINReflectionRegistryV1 *out)
{
  if (!out)
    return false;
  static NGINReflectionHeaderV1 header{};
  const auto &reg = GetRegistry();
  header.version = 1u;
  header.type_count = reg.types.Size();
  std::uint64_t fields = 0, methods = 0, attrs = 0;
  for (NGIN::UIntSize i = 0; i < reg.types.Size(); ++i)
  {
    fields += reg.types[i].fields.Size();
    methods += reg.types[i].methods.Size();
    attrs += reg.types[i].attributes.Size();
  }
  header.field_count = fields;
  header.method_count = methods;
  header.attribute_count = attrs;
  out->header = &header;
  out->blob = nullptr; // TODO: attach contiguous blob when implemented
  return true;
}
