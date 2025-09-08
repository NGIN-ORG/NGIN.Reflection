#include <boost/ut.hpp>

#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Reflection/ABI.hpp>
#include <NGIN/Reflection/ABIMerge.hpp>
#include <vector>
#include <string>

#if defined(_WIN32)
#include <windows.h>
using LibHandle = HMODULE;
static LibHandle OpenLib(const char *path) { return LoadLibraryA(path); }
static void *GetSym(LibHandle h, const char *name) { return (void *)GetProcAddress(h, name); }
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

using namespace boost::ut;
using namespace NGIN::Reflection;

suite<"Interop.Host"> interopHostSuite = []
{
  "Loads plugins, merges, and invokes"_test = []
  {
    auto dir = GetExeDir();
    auto aPath = dir + "/" + ABase;
    auto bPath = dir + "/" + BBase;
    LibHandle a = OpenLib(aPath.c_str());
    LibHandle b = OpenLib(bPath.c_str());
    expect(a != nullptr) << "load A";
    expect(b != nullptr) << "load B";

    auto symA = reinterpret_cast<bool (*)(NGINReflectionRegistryV1 *)>(GetSym(a, "NGINReflectionExportV1"));
    auto symB = reinterpret_cast<bool (*)(NGINReflectionRegistryV1 *)>(GetSym(b, "NGINReflectionExportV1"));
    expect(symA != nullptr) << "sym A";
    expect(symB != nullptr) << "sym B";

    NGINReflectionRegistryV1 modA{}, modB{};
    bool okA = symA && symA(&modA);
    bool okB = symB && symB(&modB);
    expect(okA) << "export A";
    expect(okB) << "export B";

    MergeStats stats{};
    const char *err = nullptr;
    expect(MergeRegistryV1(modA, &stats, &err)) << (err ? err : "");
    expect(MergeRegistryV1(modB, &stats, &err)) << (err ? err : "");
    expect(stats.modulesMerged == 2_u64);
    expect(stats.typesAdded >= 2_u64);
    expect(stats.typesConflicted >= 1_u64);

    // Invoke Adder.Add(2,3) == 5
    auto tAdder = GetType("Interop::Adder");
    expect(bool(tAdder)) << "type Adder";
    auto mAdd = tAdder->ResolveMethod<int, int, int>("Add");
    expect(bool(mAdd)) << "method Add";
    auto anyObj = tAdder->DefaultConstruct();
    expect(bool(anyObj)) << "construct";
    // Invoke using the function pointer table bound thunks
    auto result = mAdd->InvokeAs<int>(const_cast<void *>(anyObj->raw_data()), 2, 3);
    expect(bool(result)) << "invoke add";
    expect(*result == 5_i) << "2+3=5";

    // Invoke Multiplier.Mul(2,3) == 6
    auto tMul = GetType("Interop::Multiplier");
    expect(bool(tMul)) << "type Multiplier";
    auto mMul = tMul->ResolveMethod<int, int, int>("Mul");
    expect(bool(mMul)) << "method Mul";
    auto anyObj2 = tMul->DefaultConstruct();
    expect(bool(anyObj2)) << "construct";
    auto result2 = mMul->InvokeAs<int>(const_cast<void *>(anyObj2->raw_data()), 2, 3);
    expect(bool(result2)) << "invoke mul";
    expect(*result2 == 6_i) << "2*3=6";

    CloseLib(b);
    CloseLib(a);
  };
};
