#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/ABIMerge.hpp>
#include <NGIN/Reflection/Registry.hpp>

#include <cstdint>
#include <string>

#if defined(_WIN32)
#include <windows.h>
using LibHandle = HMODULE;
static LibHandle OpenLib(const char *path) { return LoadLibraryA(path); }
static void *GetSym(LibHandle h, const char *name)
{
  return (void *)GetProcAddress(h, name);
}
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

using namespace NGIN::Reflection;

using ModuleInitFn = bool (*)();

namespace
{
  struct LibGuard
  {
    explicit LibGuard(LibHandle h = nullptr) : handle(h) {}
    ~LibGuard() { CloseLib(handle); }

    LibGuard(const LibGuard &) = delete;
    LibGuard &operator=(const LibGuard &) = delete;

    LibGuard(LibGuard &&other) noexcept : handle(other.handle)
    {
      other.handle = nullptr;
    }
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

TEST_CASE("LoadsPluginsAndExecutesMergedMetadata", "[reflection][Interop]")
{
  auto dir = GetExeDir();
  auto aPath = dir + "/" + ABase;
  auto bPath = dir + "/" + BBase;

  LibGuard a{OpenLib(aPath.c_str())};
  LibGuard b{OpenLib(bPath.c_str())};

  INFO("load A from " << aPath);
  REQUIRE(a.handle != nullptr);
  INFO("load B from " << bPath);
  REQUIRE(b.handle != nullptr);

  auto initA = reinterpret_cast<ModuleInitFn>(GetSym(a.handle, "NGINReflectionModuleInit"));
  auto initB = reinterpret_cast<ModuleInitFn>(GetSym(b.handle, "NGINReflectionModuleInit"));
  INFO("init sym A");
  REQUIRE(initA != nullptr);
  INFO("init sym B");
  REQUIRE(initB != nullptr);

  const bool initAOk = initA();
  INFO("init A" << (initAOk ? "" : " failed"));
  REQUIRE(initAOk);
  const bool initBOk = initB();
  INFO("init B" << (initBOk ? "" : " failed"));
  REQUIRE(initBOk);

  auto symA = reinterpret_cast<bool (*)(NGINReflectionRegistryV1 *)>(
      GetSym(a.handle, "NGINReflectionExportV1"));
  auto symB = reinterpret_cast<bool (*)(NGINReflectionRegistryV1 *)>(
      GetSym(b.handle, "NGINReflectionExportV1"));
  INFO("sym A");
  REQUIRE(symA != nullptr);
  INFO("sym B");
  REQUIRE(symB != nullptr);

  NGINReflectionRegistryV1 modA{}, modB{};
  bool okA = symA(&modA);
  bool okB = symB(&modB);
  INFO("export A");
  REQUIRE(okA);
  INFO("export B");
  REQUIRE(okB);

  MergeStats stats{};
  const char *err = nullptr;
  const bool mergeA = MergeRegistryV1(modA, &stats, &err);
  INFO("merge A" << (err ? err : ""));
  REQUIRE(mergeA);

  err = nullptr;
  const bool mergeB = MergeRegistryV1(modB, &stats, &err);
  INFO("merge B" << (err ? err : ""));
  REQUIRE(mergeB);
  CHECK(stats.modulesMerged == std::uint64_t{2});
  CHECK(stats.typesAdded >= std::uint64_t{2});
  CHECK(stats.typesConflicted >= std::uint64_t{1});

  auto tAdder = GetType("Interop::Adder");
  INFO("type Adder");
  REQUIRE(tAdder.has_value());
  auto mAdd = tAdder->ResolveMethod<int, int, int>("Add");
  INFO("method Add");
  REQUIRE(mAdd.has_value());
  auto anyObj = tAdder->DefaultConstruct();
  INFO("construct");
  REQUIRE(anyObj.has_value());
  auto result =
      mAdd->InvokeAs<int>(const_cast<void *>(anyObj->Data()), 2, 3);
  INFO("invoke add");
  REQUIRE(result.has_value());
  CHECK(*result == 5);

  auto tMul = GetType("Interop::Multiplier");
  INFO("type Multiplier");
  REQUIRE(tMul.has_value());
  auto mMul = tMul->ResolveMethod<int, int, int>("Mul");
  INFO("method Mul");
  REQUIRE(mMul.has_value());
  auto anyObj2 = tMul->DefaultConstruct();
  INFO("construct");
  REQUIRE(anyObj2.has_value());
  auto result2 =
      mMul->InvokeAs<int>(const_cast<void *>(anyObj2->Data()), 2, 3);
  INFO("invoke mul");
  REQUIRE(result2.has_value());
  CHECK(*result2 == 6);
}
