#include <NGIN/Reflection/TypeBuilder.hpp>
#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/ModuleInit.hpp>

namespace Interop
{
  struct Common
  {
    int a{1};
  };

  struct Adder
  {
    int Add(int x, int y) const { return x + y; }
  };

  // ADL friends for reflection
  template <class T>
  void ngin_reflect(NGIN::Reflection::Tag<T>, NGIN::Reflection::TypeBuilder<T> &);

  template <>
  void ngin_reflect<Common>(NGIN::Reflection::Tag<Common>, NGIN::Reflection::TypeBuilder<Common> &b)
  {
    b.field<&Common::a>("a");
  }

  template <>
  void ngin_reflect<Adder>(NGIN::Reflection::Tag<Adder>, NGIN::Reflection::TypeBuilder<Adder> &b)
  {
    b.method<&Adder::Add>("Add");
  }
}

extern "C" NGIN_REFLECTION_API bool NGINReflectionModuleInit()
{
  using namespace NGIN::Reflection;
  return EnsureModuleInitialized("InteropPluginA", [](ModuleRegistration &module)
                                 { module.RegisterTypes<Interop::Common, Interop::Adder>(); });
}

// The ABI export symbol is implemented by NGIN.Reflection compiled into this module
