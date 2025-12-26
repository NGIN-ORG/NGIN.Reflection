#include <iostream>
#include <NGIN/Benchmark.hpp>
#include <NGIN/Reflection/Reflection.hpp>

using namespace NGIN;

namespace LookupBench
{
  struct ManyFields
  {
    int a0{};
    int a1{};
    int a2{};
    int a3{};
    int a4{};
    int a5{};
    int a6{};
    int a7{};
    int a8{};
    int a9{};
    int a10{};
    int a11{};
    int a12{};
    int a13{};
    int a14{};
    int a15{};
    int a16{};
    int a17{};
    int a18{};
    int a19{};

    friend void NginReflect(NGIN::Reflection::Tag<ManyFields>, NGIN::Reflection::TypeBuilder<ManyFields> &b)
    {
      b.Field<&ManyFields::a0>("a0");
      b.Field<&ManyFields::a1>("a1");
      b.Field<&ManyFields::a2>("a2");
      b.Field<&ManyFields::a3>("a3");
      b.Field<&ManyFields::a4>("a4");
      b.Field<&ManyFields::a5>("a5");
      b.Field<&ManyFields::a6>("a6");
      b.Field<&ManyFields::a7>("a7");
      b.Field<&ManyFields::a8>("a8");
      b.Field<&ManyFields::a9>("a9");
      b.Field<&ManyFields::a10>("a10");
      b.Field<&ManyFields::a11>("a11");
      b.Field<&ManyFields::a12>("a12");
      b.Field<&ManyFields::a13>("a13");
      b.Field<&ManyFields::a14>("a14");
      b.Field<&ManyFields::a15>("a15");
      b.Field<&ManyFields::a16>("a16");
      b.Field<&ManyFields::a17>("a17");
      b.Field<&ManyFields::a18>("a18");
      b.Field<&ManyFields::a19>("a19");
    }
  };
}

int main()
{
  using namespace NGIN::Reflection;
  using LookupBench::ManyFields;

  auto t = GetType<ManyFields>();

  // Warmup lookup (ensure interning of the names used below)
  (void)t.GetField("a0");

  constexpr int N = 10000;

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        for (int i = 0; i < N; ++i)
                        {
                          (void)t.GetField("a15");
                        }
                        ctx.stop(); }, "GetField(name) 10k hits");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        int misses = 0;
                        for (int i = 0; i < N; ++i)
                        {
                          auto f = t.GetField("does_not_exist");
                          misses += f.has_value() ? 0 : 1;
                        }
                        ctx.doNotOptimize(misses);
                        ctx.stop(); }, "GetField(name) 10k misses");

  auto results = NGIN::Benchmark::RunAll<Milliseconds>();
  NGIN::Benchmark::PrintSummaryTable(std::cout, results);
  return 0;
}
