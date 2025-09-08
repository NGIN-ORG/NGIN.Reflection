#include <NGIN/Reflection/Builder.hpp>
#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/ABI.hpp>

namespace Interop
{
  // Same qualified name/typeId as in PluginA to force a conflict
  struct Common { int a{2}; };

  struct Multiplier
  {
    int Mul(int x, int y) const { return x * y; }
  };

  template <class T>
  void ngin_reflect(NGIN::Reflection::Tag<T>, NGIN::Reflection::Builder<T> &);

  template <>
  void ngin_reflect<Common>(NGIN::Reflection::Tag<Common>, NGIN::Reflection::Builder<Common> &b)
  {
    b.field<&Common::a>("a");
  }

  template <>
  void ngin_reflect<Multiplier>(NGIN::Reflection::Tag<Multiplier>, NGIN::Reflection::Builder<Multiplier> &b)
  {
    b.method<&Multiplier::Mul>("Mul");
  }
}

static bool s_registerB = [] {
  using namespace NGIN::Reflection;
  auto_register<Interop::Common>();
  auto_register<Interop::Multiplier>();
  return true;
}();

