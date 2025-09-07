#include <iostream>
#include <NGIN/Benchmark.hpp>
#include <NGIN/Reflection/Registry.hpp>
#include <NGIN/Containers/String.hpp>

using namespace NGIN;

int main()
{
  using namespace NGIN::Reflection;
  using namespace NGIN::Reflection::detail;

  constexpr int N = 10000;
  NGIN::Containers::Vector<NGIN::Containers::String> names;
  names.Reserve(N);
  for (int i = 0; i < N; ++i)
  {
    NGIN::Containers::String s;
    s.Append("bench::Name_");
    s.Append(std::to_string(i));
    names.PushBack(std::move(s));
  }

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        for (int i = 0; i < N; ++i)
                        {
                          (void)InternNameId(std::string_view{names[i].CStr(), names[i].GetSize()});
                        }
                        ctx.stop(); }, "Interner: InsertOrGet 10k unique");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        for (int i = 0; i < N; ++i)
                        {
                          (void)InternNameId(std::string_view{names[i].CStr(), names[i].GetSize()});
                        }
                        ctx.stop(); }, "Interner: InsertOrGet 10k duplicates");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        for (int i = 0; i < N; ++i)
                        {
                          NameId id{};
                          (void)FindNameId(std::string_view{names[i].CStr(), names[i].GetSize()}, id);
                        }
                        ctx.stop(); }, "Interner: FindId 10k hits");

  // Miss strings
  NGIN::Containers::Vector<NGIN::Containers::String> miss;
  miss.Reserve(N);
  for (int i = 0; i < N; ++i)
  {
    NGIN::Containers::String s;
    s.Append("bench::Miss_");
    s.Append(std::to_string(i));
    miss.PushBack(std::move(s));
  }

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
                        ctx.start();
                        int found = 0;
                        for (int i = 0; i < N; ++i)
                        {
                          NameId id{};
                          if (FindNameId(std::string_view{miss[i].CStr(), miss[i].GetSize()}, id))
                            ++found;
                        }
                        ctx.doNotOptimize(found);
                        ctx.stop(); }, "Interner: FindId 10k misses");

  // Print summary
  auto results = Benchmark::RunAll<Milliseconds>();
  NGIN::Benchmark::PrintSummaryTable(std::cout, results);
  return 0;
}
