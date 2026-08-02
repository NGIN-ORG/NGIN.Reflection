#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

#include <array>
#include <string>

#if defined(_WIN32)
#include <windows.h>
using LibHandle = HMODULE;
static LibHandle OpenLib(const char *path) { return LoadLibraryA(path); }
static void *GetSym(LibHandle h, const char *name) { return reinterpret_cast<void *>(GetProcAddress(h, name)); }
static void CloseLib(LibHandle h)
{
  if (h)
    FreeLibrary(h);
}
static std::string GetExeDir()
{
  char buf[MAX_PATH];
  DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  std::string s(buf, buf + n);
  auto p = s.find_last_of("/\\");
  return (p == std::string::npos) ? std::string{"."} : s.substr(0, p);
}
static const char *ABase = "InteropPluginA.dll";
static const char *BBase = "InteropPluginB.dll";
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
using LibHandle = void *;
static LibHandle OpenLib(const char *path) { return dlopen(path, RTLD_LAZY); }
static void *GetSym(LibHandle h, const char *name) { return dlsym(h, name); }
static void CloseLib(LibHandle h)
{
  if (h)
    dlclose(h);
}
static std::string GetExeDir()
{
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string tmp(size, '\0');
  _NSGetExecutablePath(tmp.data(), &size);
  auto p = tmp.find_last_of('/');
  return (p == std::string::npos) ? std::string{"."} : tmp.substr(0, p);
}
static const char *ABase = "libInteropPluginA.dylib";
static const char *BBase = "libInteropPluginB.dylib";
#else
#include <dlfcn.h>
#include <unistd.h>
using LibHandle = void *;
static LibHandle OpenLib(const char *path) { return dlopen(path, RTLD_LAZY); }
static void *GetSym(LibHandle h, const char *name) { return dlsym(h, name); }
static void CloseLib(LibHandle h)
{
  if (h)
    dlclose(h);
}
static std::string GetExeDir()
{
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf));
  if (n <= 0)
    return ".";
  std::string s(buf, buf + n);
  auto p = s.find_last_of('/');
  return (p == std::string::npos) ? std::string{"."} : s.substr(0, p);
}
static const char *ABase = "libInteropPluginA.so";
static const char *BBase = "libInteropPluginB.so";
#endif

using InitFn = bool (*)();
using ApiFn = bool (*)(NGINReflectionModuleApi *);

namespace
{
  struct LibGuard
  {
    explicit LibGuard(LibHandle lib = nullptr) : handle(lib) {}
    ~LibGuard() { CloseLib(handle); }
    LibGuard(const LibGuard &) = delete;
    LibGuard &operator=(const LibGuard &) = delete;
    LibGuard(LibGuard &&other) noexcept : handle(other.handle) { other.handle = nullptr; }
    LibGuard &operator=(LibGuard &&other) noexcept
    {
      if (this != &other)
      {
        CloseLib(handle);
        handle = other.handle;
        other.handle = nullptr;
      }
      return *this;
    }
    LibHandle handle{nullptr};
  };
} // namespace

TEST_CASE("Imported module API exposes the same callable surface as local registration", "[reflection][interop]")
{
  using namespace NGIN::Reflection;

  auto dir = GetExeDir();
  LibGuard a{OpenLib((dir + "/" + ABase).c_str())};
  LibGuard b{OpenLib((dir + "/" + BBase).c_str())};
  REQUIRE(a.handle != nullptr);
  REQUIRE(b.handle != nullptr);

  auto initA = reinterpret_cast<InitFn>(GetSym(a.handle, "NGINReflectionModuleInit"));
  auto initB = reinterpret_cast<InitFn>(GetSym(b.handle, "NGINReflectionModuleInit"));
  auto apiA = reinterpret_cast<ApiFn>(GetSym(a.handle, "NGINReflectionGetModuleApi"));
  auto apiB = reinterpret_cast<ApiFn>(GetSym(b.handle, "NGINReflectionGetModuleApi"));
  REQUIRE(initA != nullptr);
  REQUIRE(initB != nullptr);
  REQUIRE(apiA != nullptr);
  REQUIRE(apiB != nullptr);

  REQUIRE(initA());
  REQUIRE(initB());

  NGINReflectionModuleApi moduleA{};
  NGINReflectionModuleApi moduleB{};
  REQUIRE(apiA(&moduleA));
  REQUIRE(apiB(&moduleB));

  Error error{};
  REQUIRE(ImportModule(moduleA, &error));
  REQUIRE(ImportModule(moduleB, &error));

  auto offsetterType = GetType("Interop::Offsetter");
  REQUIRE(offsetterType.has_value());
  REQUIRE(offsetterType->ConstructorCount() == 1);
  auto offsetterConstructor = offsetterType->ConstructorAt(0);
  REQUIRE(offsetterConstructor.has_value());
  REQUIRE(offsetterConstructor->IsInjectable());
  auto offsetBinding = offsetterConstructor->ParameterBindingAt(0);
  REQUIRE(offsetBinding.has_value());
  CHECK(offsetBinding->name == "offset");

  auto offset = NGIN::Memory::MakeShared<int>(9);
  std::array<Value, 1> offsetArguments{MakeInstanceValue(offset)};
  auto offsetter = offsetterConstructor->Invoke(offsetArguments);
  REQUIRE(offsetter.has_value());
  auto addOffset = offsetterType->GetMethod("AddOffset");
  REQUIRE(addOffset.has_value());
  std::array<Value, 1> offsetInput{Value{std::int64_t{4}}};
  auto offsetResult = addOffset->Invoke(*offsetter, offsetInput);
  REQUIRE(offsetResult.has_value());
  REQUIRE(offsetResult->TryAs<std::int64_t>() != nullptr);
  CHECK(*offsetResult->TryAs<std::int64_t>() == 13);

  Error unloadError{};
  CHECK_FALSE(UnloadModule(offsetterType->Identity().module, &unloadError));
  CHECK(unloadError.code == ErrorCode::Conflict);

  auto adderType = GetType("Interop::Adder");
  REQUIRE(adderType.has_value());
  auto adder = adderType->Construct();
  REQUIRE(adder.has_value());
  auto add = adderType->GetMethod("Add");
  REQUIRE(add.has_value());
  std::array<Value, 2> addArgs{Value{std::int64_t{3}}, Value{std::int64_t{4}}};
  auto sum = add->Invoke(*adder, addArgs);
  REQUIRE(sum.has_value());
  REQUIRE(sum->TryAs<std::int64_t>() != nullptr);
  CHECK((*sum->TryAs<std::int64_t>()) == 7);

  auto multiplierType = GetType("Interop::Multiplier");
  REQUIRE(multiplierType.has_value());
  auto multiplier = multiplierType->Construct();
  REQUIRE(multiplier.has_value());
  auto mul = multiplierType->GetMethod("Mul");
  REQUIRE(mul.has_value());
  std::array<Value, 2> mulArgs{Value{std::int64_t{3}}, Value{std::int64_t{4}}};
  auto product = mul->Invoke(*multiplier, mulArgs);
  REQUIRE(product.has_value());
  REQUIRE(product->TryAs<std::int64_t>() != nullptr);
  CHECK((*product->TryAs<std::int64_t>()) == 12);

  const auto pluginAIdentity = offsetterType->Identity().module;
  const auto pluginBIdentity = multiplierType->Identity().module;
  offsetter = InstanceRef{};
  adder = InstanceRef{};
  multiplier = InstanceRef{};
  REQUIRE(UnloadModule(pluginAIdentity, &unloadError));
  REQUIRE(UnloadModule(pluginBIdentity, &unloadError));
}
