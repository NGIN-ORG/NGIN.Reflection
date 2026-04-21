# NGIN.Reflection

NGIN.Reflection is a runtime reflection system for modern C++23 with one model for both local and imported modules.

The current library is built around three ideas:

- reflection is explicit and authored, not discovered
- local and imported modules expose the same descriptor surface
- the plugin boundary is a plain C ABI with explicit ownership

## What It Provides

NGIN.Reflection supports:

- fields
- properties
- methods
- constructors
- enums
- base relationships
- free functions
- attributes
- module import, unload, and replace

The public C++ layer uses `Value`, `ValueView`, `ConstValueView`, `InstanceRef`, and `ConstInstanceRef` instead of exposing raw ABI structs directly.

## Authoring Model

Types are still described with an explicit `TypeBuilder<T>` customization point:

```cpp
#include <NGIN/Reflection/Reflection.hpp>

namespace Demo
{
  struct User
  {
    int id{1};
    std::string name{"Ada"};

    friend void NginReflect(NGIN::Reflection::Tag<User>,
                            NGIN::Reflection::TypeBuilder<User> &builder)
    {
      builder.SetName("Demo::User");
      builder.Field<&User::id>("id");
      builder.Field<&User::name>("name");
    }
  };
}

int main()
{
  using namespace NGIN::Reflection;

  ModuleRegistration module{"Demo.Reflection"};
  module.RegisterType<Demo::User>();
  module.Commit();

  auto type = GetType("Demo::User").value();
  auto instance = type.Construct().value();
  auto field = type.GetField("name").value();
  auto value = field.Read(instance).value();
  return value.TryAs<std::string>() ? 0 : 1;
}
```

## ABI Model

The plugin/runtime boundary is now `NGINReflectionModuleApi`.

- Export entrypoint: `NGINReflectionGetModuleApi`
- Value transport: `NGINReflectionValue`
- Object transport: `NGINReflectionInstanceHandle`
- Import path: `ImportModule`
- Unload path: `UnloadModule`

The removed `NGINReflectionExportV1` / `MergeRegistryV1` blob path is no longer part of the supported API.

## Build Options

Main CMake options:

- `NGIN_REFLECTION_BUILD_TESTS` default `ON`
- `NGIN_REFLECTION_BUILD_EXAMPLES` default `OFF`
- `NGIN_REFLECTION_BUILD_BENCHMARKS` default `OFF`
- `NGIN_REFLECTION_ENABLE_ABI` default `ON`

## Typical Local Build

```bash
cmake -S . -B build \
  -DNGIN_REFLECTION_BUILD_TESTS=ON \
  -DNGIN_REFLECTION_BUILD_EXAMPLES=ON \
  -DNGIN_REFLECTION_BUILD_BENCHMARKS=OFF \
  -DNGIN_REFLECTION_ENABLE_ABI=ON

cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Read Next

- [Contribution Guide](AGENTS.md)
- [Architecture Notes](docs/Architecture.md)
- `include/NGIN/Reflection/`
