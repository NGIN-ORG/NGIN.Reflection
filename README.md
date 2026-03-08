# NGIN.Reflection

NGIN.Reflection is a runtime reflection library for modern C++23.

It provides an explicit, opt-in reflection model for describing and querying types at runtime.

The short version is:

- reflection is authored by you
- metadata is explicit, not auto-discovered
- the library is suitable for tooling, editors, scripting glue, diagnostics, and inspection
- it builds on `NGIN.Base`, but is useful on its own

## What NGIN.Reflection Is For

NGIN.Reflection is for situations where code needs runtime knowledge about types, fields, methods, constructors, enums, or attributes.

Typical uses include:

- tooling
- editor integration
- property inspection
- scripting bridges
- diagnostics and metadata-driven utilities

It is not trying to be automatic reflection for all of C++. It gives you a way to describe the types you care about and then query those descriptions at runtime.

## What It Is Not

NGIN.Reflection is not:

- a serializer by itself
- a full scripting runtime
- an automatic source-scanning reflection system

It is a runtime metadata and invocation layer.

## How The Model Works

You describe a type explicitly, then query it at runtime.

The preferred style is an ADL friend customization point:

```cpp
#include <NGIN/Reflection/Reflection.hpp>

struct User
{
    int id {};

    friend void NginReflect(NGIN::Reflection::Tag<User>,
                            NGIN::Reflection::TypeBuilder<User>& b)
    {
        b.SetName("Demo::User");
        b.Field<&User::id>("id");
    }
};

int main()
{
    auto type = NGIN::Reflection::GetType<User>();
    auto field = type.GetField("id").value();
    (void)field;
}
```

That is the core idea: reflection is explicit and opt-in.

## What It Provides

NGIN.Reflection supports metadata for:

- fields
- properties
- methods
- constructors
- enums
- attributes

It also supports runtime lookup and typed invocation helpers on top of that metadata.

## Build Targets

- `NGIN::Reflection`

The repo currently exposes one primary library target rather than separate static/shared aliases in the style of `NGIN.Base` or `NGIN.Log`.

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

## Where It Fits

Within the broader NGIN platform, NGIN.Reflection provides a metadata layer that higher-level tooling and runtime systems can build on.

It is especially relevant for:

- editors
- tooling shells
- diagnostics
- package/module metadata flows that need runtime inspection

## Read Next

- [Contribution Guide](AGENTS.md)
- `include/NGIN/Reflection/`
