#include <iostream>
#include <string>
#include <vector>

#include <NGIN/Benchmark.hpp>
#include <NGIN/Reflection/Registry.hpp>

using namespace NGIN;

int main()
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::detail;

  constexpr int N = 10000;
  std::vector<std::string> names;
  names.reserve(N);
  for (int i = 0; i < N; ++i)
  {
    names.push_back("bench::Name_" + std::to_string(i));
  }

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        for (int i = 0; i < N; ++i)
                        {
                          (void)InternSymbol(names[i]);
                        }
                        ctx.stop(); }, "Interner: InsertOrGet 10k unique");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        for (int i = 0; i < N; ++i)
                        {
                          (void)InternSymbol(names[i]);
                        }
                        ctx.stop(); }, "Interner: InsertOrGet 10k duplicates");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        for (int i = 0; i < N; ++i)
                        {
                          SymbolId id{};
                          (void)TryGetSymbol(names[i], id);
                        }
                        ctx.stop(); }, "Interner: FindId 10k hits");

  std::vector<std::string> misses;
  misses.reserve(N);
  for (int i = 0; i < N; ++i)
  {
    misses.push_back("bench::Miss_" + std::to_string(i));
  }

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        int found = 0;
                        for (int i = 0; i < N; ++i)
                        {
                          SymbolId id{};
                          if (TryGetSymbol(misses[i], id))
                            ++found;
                        }
                        ctx.doNotOptimize(found);
                        ctx.stop(); }, "Interner: FindId 10k misses");

  // Print summary
  auto results = Benchmark::RunAll<Milliseconds>();
  NGIN::Benchmark::PrintSummaryTable(std::cout, results);
  return 0;
}
