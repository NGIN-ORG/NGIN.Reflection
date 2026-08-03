# NGIN.Reflection

`NGIN.Reflection` is a C++23 runtime reflection library with one descriptor
model for local and imported modules.

It supports fields, properties, methods, constructors, enums, base
relationships, free functions, attributes, and module import or unload.
Reflection is explicit: types opt in through `TypeBuilder<T>` or generated
builder code.

## Example

```cpp
#include <NGIN/Reflection/Reflection.hpp>

struct User {
    int id{};

    friend void NginReflect(NGIN::Reflection::Tag<User>,
                            NGIN::Reflection::TypeBuilder<User>& builder) {
        builder.SetName("User");
        builder.Field<&User::id>("id");
    }
};

int main() {
    NGIN::Reflection::ModuleRegistration module{"App"};
    module.RegisterType<User>();
    module.Commit();
    return NGIN::Reflection::GetType("User") ? 0 : 1;
}
```

`NGIN.Reflection.MetaGen` can emit the same registration model from annotated
headers. See [Hello.Reflection](../../../Examples/Hello.Reflection).

## Modules and ABI

Imported modules expose a plain C ABI through `NGINReflectionModuleApi` and
`NGINReflectionGetModuleApi`. Reflection values and instances retain their
owning module; unloading is rejected while reflected instances remain alive.

Binary compatibility is not promised before 1.0. Treat plugin modules as
trusted native code built for a compatible ABI.

## Build and test

```bash
cmake -S . -B build \
  -DNGIN_REFLECTION_BUILD_TESTS=ON \
  -DNGIN_REFLECTION_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

See the [architecture notes](docs/Architecture.md) and
[contribution guide](AGENTS.md).
