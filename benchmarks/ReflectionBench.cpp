#include <iostream>

#include <NGIN/Benchmark.hpp>
#include <NGIN/Reflection/Reflection.hpp>

using namespace NGIN;

namespace BenchDemo
{
  struct Vec2
  {
    float x, y;
  };
  struct Obj
  {
    int n{0};
    Vec2 p{1.0f, 2.0f};
    int add(int v) const { return n + v; }
    friend void NginReflect(Reflection::Tag<Obj>, Reflection::TypeBuilder<Obj> &b)
    {
      b.Field<&Obj::n>("n");
      b.Field<&Obj::p>("p");
      b.Method<&Obj::add>("add");
    }
  };
}

int main()
{
  using namespace NGIN::Reflection;
  using BenchDemo::Obj;

  auto t = GetType<Obj>();
  auto m_add = t.GetMethod("add").value();

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
    Obj o{5};
    Any arg{7};
    ctx.start();
    int sum = 0;
    for (int i=0;i<10000;++i) {
      auto out = m_add.Invoke(&o, &arg, 1).value();
      sum += out.Cast<int>();
    }
    ctx.doNotOptimize(sum);
    ctx.stop(); }, "Method Invoke add(int) 10k");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
    Obj o{5};
    ctx.start();
    int sum = 0;
    for (int i=0;i<10000;++i) {
      sum += o.add(7);
    }
    ctx.doNotOptimize(sum);
    ctx.stop(); }, "Direct add(int) 10k");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
    Obj o{0};
    Any val{42};
    auto f = t.GetField("n").value();
    ctx.start();
    for (int i=0;i<20000;++i) {
      (void)f.SetAny(&o, val);
    }
    ctx.stop(); }, "Field SetAny int 20k");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
    Obj o{0};
    ctx.start();
    for (int i=0;i<20000;++i) {
      o.n = 42;
    }
    ctx.stop(); }, "Direct set int 20k");

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
    Obj o{5};
    Any arg{7.0}; // conversion from double to int
    ctx.start();
    int sum = 0;
    for (int i=0;i<10000;++i) {
      auto out = m_add.Invoke(&o, &arg, 1).value();
      sum += out.Cast<int>();
    }
    ctx.doNotOptimize(sum);
    ctx.stop(); }, "Method Invoke add(conv double->int) 10k");

  auto results = Benchmark::RunAll<Milliseconds>();
  Benchmark::PrintSummaryTable(std::cout, results);
  return 0;
}
