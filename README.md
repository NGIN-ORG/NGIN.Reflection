# NGIN.Reflection

Runtime reflection for modern **C++23** with an explicit, opt‑in model:
**no required macros**, **no RTTI in the core**, and a compact registry designed for fast lookups.

Types are described once (via an **ADL friend** or **traits specialization**) and then queried at runtime for
**fields, properties, methods, constructors, enums, and attributes** — without intrusive base classes or macros.

> Reflection is **opt‑in**: only types you explicitly describe are registered. There is no header scanning or
> automatic discovery.

---

## At a glance

- Header‑first, template‑friendly API with a minimal compiled core
- Process‑local registry with interned names and cheap, cache‑friendly handles
- Overload resolution with promotions and conversions + typed invoke helpers
- Any‑based boxing with **32‑byte SBO** (from `NGIN.Base`)
- Optional **cross‑DLL metadata import/export** (invocation when tables are present)

---

## What this library is (and isn’t)

**NGIN.Reflection is:**

- A runtime metadata and invocation layer for C++
- Suitable as a foundation for tooling, editors, scripting, diagnostics, or glue code

**NGIN.Reflection is not:**

- A serializer or ORM (though it can power one)
- A full dynamic language runtime
- An automatic reflection system — descriptions are authored by you

---

## Contents

- [Quick start](#quick-start)
- [Installation](#installation)
- [Registering types](#registering-types)
- [Working with metadata](#working-with-metadata)
- [Adapters](#adapters)
- [Error model](#error-model)
- [Threading & registration](#threading--registration)
- [Cross-DLL ABI (optional)](#cross-dll-abi-optional)
- [Build options](#build-options)
- [Docs & examples](#docs--examples)
- [Status](#status)

---

## Quick start

Define a type description using an **ADL friend** (preferred: can access private members),
then query it at runtime.

```cpp
#include <NGIN/Reflection/Reflection.hpp>

struct User
{
  int id{};
  int score{};

  int Add(int a, int b) const { return a + b; }
  int Score() const { return score; }
  void SetScore(int v) { score = v; }

  friend void NginReflect(NGIN::Reflection::Tag<User>,
                          NGIN::Reflection::TypeBuilder<User>& b)
  {
    b.SetName("Demo::User");
    b.Field<&User::id>("id");
    b.Field<&User::score>("score");
    b.Property<&User::Score, &User::SetScore>("Score");
    b.Method<&User::Add>("Add");
  }
};

int main()
{
  using namespace NGIN::Reflection;

  auto t = GetType<User>();
  User u{};

  auto id = t.GetField("id").value();
  id.Set(u, 42).value();

  auto add = t.ResolveMethod<int, int, int>("Add").value();
  int sum = add.InvokeAs<int>(&u, 3, 4).value();

  (void)sum;
}
```

---

## Installation

### Requirements

- A **C++23** compiler
- `NGIN.Base` (found via package or sibling source tree)

### Build & test

```bash
cmake -S . -B build \
  -DNGIN_REFLECTION_BUILD_TESTS=ON \
  -DNGIN_REFLECTION_BUILD_EXAMPLES=ON \
  -DNGIN_REFLECTION_BUILD_BENCHMARKS=OFF \
  -DNGIN_REFLECTION_ENABLE_ABI=ON

cmake --build build -j
ctest --test-dir build --output-on-failure
```

---

## Registering types

There are two supported customization points.

### 1) ADL friend (preferred)

```cpp
struct Secret
{
  int value{};

  friend void NginReflect(NGIN::Reflection::Tag<Secret>,
                          NGIN::Reflection::TypeBuilder<Secret>& b)
  {
    b.Field<&Secret::value>("value");
  }
};
```

### 2) Trait fallback (external types)

```cpp
namespace NGIN::Reflection
{
  template <>
  struct Describe<std::pair<int, int>>
  {
    static void Do(TypeBuilder<std::pair<int, int>>& b)
    {
      b.SetName("std::pair<int,int>");
      b.Field<&std::pair<int, int>::first>("first");
      b.Field<&std::pair<int, int>::second>("second");
    }
  };
}
```

### Notes

- `GetType<T>()` registers (if needed)
- `TryGetType<T>()` and `FindType(name)` never register
- Overloaded members must be disambiguated with an explicit cast
- Type identity is based on `NGIN::Meta::TypeName<T>::qualifiedName`: stable
  across translation units, but not guaranteed across different compilers or
  standard libraries

Overload disambiguation example:

```cpp
b.Method<static_cast<int (Math::*)(int,int) const>(&Math::mul)>("mul");
b.Method<static_cast<double (Math::*)(int,double) const>(&Math::mul)>("mul");
```

---

## Working with metadata

### Fields vs properties

- **Field**: direct data member get/set
- **Property**: getter/setter pair, no storage required

```cpp
auto t = GetType<User>();
User u{};

auto id = t.GetField("id").value();
id.Set(u, 123).value();

auto score = t.GetProperty("Score").value();
score.Set(u, 10).value();
```

### Methods

```cpp
auto add = t.ResolveMethod<int, int, int>("Add").value();
int sum = add.InvokeAs<int>(&u, 1, 2).value();
```

### Constructors

```cpp
struct Widget
{
  Widget(int x, float y) {}
  friend void NginReflect(NGIN::Reflection::Tag<Widget>,
                          NGIN::Reflection::TypeBuilder<Widget>& b)
  {
    b.Constructor<int, float>();
  }
};

auto tw = NGIN::Reflection::GetType<Widget>();
auto anyWidget = tw.Construct({1, 2.0f}).value();
Widget w = anyWidget.Cast<Widget>();
```

### Free / static functions

```cpp
int Add(int a, int b) { return a + b; }

NGIN::Reflection::RegisterFunction<&Add>("Add");

auto fn = NGIN::Reflection::ResolveFunction<int, int, int>("Add").value();
int r = fn.InvokeAs<int>(2, 3).value();
```

### Enums & attributes

```cpp
enum class Mode { Fast = 1, Safe = 2 };

struct Config
{
  Mode mode{Mode::Safe};
  friend void NginReflect(NGIN::Reflection::Tag<Config>,
                          NGIN::Reflection::TypeBuilder<Config>& b)
  {
    b.EnumValue("Fast", Mode::Fast);
    b.EnumValue("Safe", Mode::Safe);
    b.Field<&Config::mode>("mode");
    b.Attribute("category", std::string_view{"settings"});
  }
};
```

---

## Adapters

Adapters provide read‑only traversal of common container shapes
(sequence, tuple‑like, variant‑like, optional‑like, and map‑like containers).
Prefer the `*View()` APIs, which return non‑owning `ConstAnyView` and avoid
copies; the `Any`‑returning methods copy values.

See `include/NGIN/Reflection/Adapters.hpp` for details.

Example (sequence + view):

```cpp
std::vector<int> v{1, 2, 3};
auto a = NGIN::Reflection::Adapters::MakeSequenceAdapter(v);
for (NGIN::UIntSize i = 0; i < a.Size(); ++i)
{
  auto view = a.ElementView(i);
  int value = view.Cast<int>();
  (void)value;
}
```

---

## Error model

Most APIs return `std::expected<T, Error>`:

- No exceptions for library errors
- Structured diagnostics for overload resolution failures
- Exceptions only propagate from user code or allocators
- Copying `Any` may throw if the stored type is not copyable/trivially copyable

---

## Threading & registration

- Reads use a shared lock
- Registration uses an exclusive lock

Concurrent reads are safe.
Avoid concurrent registration during startup — registration executes user code.
Calling registration while holding a read lock will `std::terminate`.

---

## Cross-DLL ABI (optional)

Enable with: `-DNGIN_REFLECTION_ENABLE_ABI=ON`

**Advanced / experimental:** the ABI surface is usable but evolving and has
known limitations.

- Metadata import/export via a stable C ABI
- Pointer‑free blob with interned string table
- Cross‑DLL invocation only when invoke tables are emitted
- Field accessors are metadata‑only across DLLs

**Note:** Exported blobs are module‑owned. Hosts should copy the blob immediately.

---

## Build options

- `NGIN_REFLECTION_BUILD_TESTS` (default ON)
- `NGIN_REFLECTION_BUILD_EXAMPLES` (default OFF)
- `NGIN_REFLECTION_BUILD_BENCHMARKS` (default OFF)
- `NGIN_REFLECTION_ENABLE_ABI` (default ON, advanced)

---

## Docs & examples

- Architecture & implementation notes: `docs/Architecture.md`
- Examples:
  - `examples/QuickStart`
  - `examples/Methods`
  - `examples/Adapters`

---

## Status

Core reflection (fields, properties, methods, constructors, adapters, Any,
error model) is implemented and stable.

Cross‑DLL ABI export/merge exists but is **advanced/experimental** and has
documented limitations. See `docs/Architecture.md` for details.
