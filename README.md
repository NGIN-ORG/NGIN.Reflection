# NGIN.Reflection

Runtime reflection for modern C++23: zero required macros, no RTTI in the core, and a
compact registry designed for fast lookups. You describe types once (via an ADL
friend or a traits specialization) and then query fields, properties, methods,
constructors, enums, and attributes at runtime.

## At a glance

- Header-first, template-friendly API with minimal compiled core.
- Process-local registry with interned names and small, cheap handles.
- Overload resolution with promotions/conversions + typed invoke helpers.
- Any-based boxing with 32-byte SBO (from NGIN.Base).
- Optional cross-DLL ABI export/merge (metadata always; method/ctor invoke when
  tables are present).

## Quick start

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
                          NGIN::Reflection::TypeBuilder<User> &b)
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

  auto idField = t.GetField("id").value();
  idField.Set(u, 42).value();

  auto scoreProp = t.GetProperty("Score").value();
  scoreProp.Set(u, 7).value();

  auto add = t.ResolveMethod<int, int, int>("Add").value();
  auto sum = add.InvokeAs<int>(&u, 3, 4).value();

  (void)sum;
}
```

## Registering types

There are two supported customization points:

1) **ADL friend** (preferred, grants private access)

```cpp
struct Secret
{
  int value{};

  friend void NginReflect(NGIN::Reflection::Tag<Secret>,
                          NGIN::Reflection::TypeBuilder<Secret> &b)
  {
    b.Field<&Secret::value>("value");
  }
};
```

2) **Trait fallback** for external types

```cpp
namespace NGIN::Reflection
{
  template <>
  struct Describe<std::pair<int, int>>
  {
    static void Do(TypeBuilder<std::pair<int, int>> &b)
    {
      b.SetName("std::pair<int,int>");
      b.Field<&std::pair<int, int>::first>("first");
      b.Field<&std::pair<int, int>::second>("second");
    }
  };
}
```

Notes:

- `GetType<T>()` registers (if needed). `TryGetType<T>()` and `FindType(name)`
  do not.
- Overload disambiguation: explicitly cast overloaded member pointers to their
  exact signatures.

## Working with metadata

### Fields & properties

```cpp
auto t = NGIN::Reflection::GetType<User>();
User u{};

auto id = t.GetField("id").value();
id.Set(u, 123).value();
int v = id.Get<int>(u).value();

auto score = t.GetProperty("Score").value();
score.Set(u, 10).value();
int s = score.Get<int>(u).value();
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
                          NGIN::Reflection::TypeBuilder<Widget> &b)
  {
    b.Constructor<int, float>();
  }
};

auto tw = NGIN::Reflection::GetType<Widget>();
NGIN::Reflection::Any args[2] = {NGIN::Reflection::Any{1},
                                 NGIN::Reflection::Any{2.0f}};
auto anyWidget = tw.Construct(args, 2).value();
Widget w = anyWidget.Cast<Widget>();
```

### Free/static functions

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
                          NGIN::Reflection::TypeBuilder<Config> &b)
  {
    b.EnumValue("Fast", Mode::Fast);
    b.EnumValue("Safe", Mode::Safe);
    b.Field<&Config::mode>("mode");
    b.Attribute("category", std::string_view{"settings"});
  }
};
```

Attribute values are `std::variant<bool, int64_t, double, string_view, NGIN::UInt64>`.

## Adapters

Adapters provide read-only access to common container shapes through `Any`:

- Sequence: `std::vector`, `NGIN::Containers::Vector`
- Tuple-like: `std::tuple`, `std::pair`, and other tuple-like types
- Variant-like: `std::variant`
- Optional-like: `std::optional` and types with `HasValue/Value`
- Map-like: `std::map`, `std::unordered_map`, `NGIN::Containers::FlatHashMap`

See `include/NGIN/Reflection/Adapters.hpp` for the exact adapter APIs.

## Error model

Most APIs return `std::expected<T, Error>` with structured overload diagnostics
on resolution failures. Errors do **not** throw; exceptions only propagate from
user code or allocators.

```cpp
auto method = t.ResolveMethod("Add", args, 2);
if (!method)
{
  const auto &err = method.error();
  // err.code, err.message, err.diagnostics
}
```

## Threading & registration

Reads are guarded by a shared lock; registration uses an exclusive lock. The
registry is safe for concurrent reads, but **avoid concurrent registration**
during startup (registration executes user code and is intentionally serialized).

## Cross-DLL ABI (optional)

Enable with `-DNGIN_REFLECTION_ENABLE_ABI=ON` (default ON).

- Exported symbol: `NGINReflectionExportV1` in `include/NGIN/Reflection/ABI.hpp`.
- The exported blob is pointer-free and self-contained; strings are stored in a
  table.
- Methods/ctors are invocable across DLLs **only** when invoke tables are
  present.
- Current limitation: field accessors are metadata-only across DLLs.
- The exporter allocates the blob with `NGIN::Memory::SystemAllocator` and does
  not yet expose a free API (treat as module-owned or copy).

## Build & install

Requirements: C++23 compiler and NGIN.Base (found via package or sibling source).

```bash
cmake -S . -B build \
  -DNGIN_REFLECTION_BUILD_TESTS=ON \
  -DNGIN_REFLECTION_BUILD_EXAMPLES=ON \
  -DNGIN_REFLECTION_BUILD_BENCHMARKS=OFF \
  -DNGIN_REFLECTION_ENABLE_ABI=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Options:

- `NGIN_REFLECTION_BUILD_TESTS` (default ON)
- `NGIN_REFLECTION_BUILD_EXAMPLES` (default OFF)
- `NGIN_REFLECTION_BUILD_BENCHMARKS` (default OFF)
- `NGIN_REFLECTION_ENABLE_ABI` (default ON)

## Docs & examples

- Architecture and implementation notes: `docs/Architecture.md`
- Examples: `examples/QuickStart`, `examples/Methods`, `examples/Adapters`

## Status

Phase 1-2 features are implemented (fields, methods, constructors, adapters,
Any, error model). Cross-DLL export/merge exists but has limitations; see the ABI
section and `docs/Architecture.md` for details.
