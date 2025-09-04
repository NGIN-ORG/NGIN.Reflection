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
    friend void ngin_reflect(Reflection::tag<Obj>, Reflection::Builder<Obj> &b)
    {
      b.field<&Obj::n>("n");
      b.field<&Obj::p>("p");
      b.method<&Obj::add>("add");
    }
  };
}

int main()
{
  using namespace NGIN::Reflection;
  using BenchDemo::Obj;

  auto t = type_of<Obj>();
  auto m_add = t.GetMethod("add").value();

  Benchmark::Register([&](BenchmarkContext &ctx)
                      {
    Obj o{5};
    Any arg = Any::make(7);
    ctx.start();
    int sum = 0;
    for (int i=0;i<10000;++i) {
      auto out = m_add.invoke(&o, &arg, 1).value();
      sum += out.as<int>();
    }
    ctx.doNotOptimize(sum);
    ctx.stop(); }, "Method invoke add(int) 10k");

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
    Any val = Any::make(42);
    auto f = t.GetField("n").value();
    ctx.start();
    for (int i=0;i<20000;++i) {
      (void)f.set_any(&o, val);
    }
    ctx.stop(); }, "Field set_any int 20k");

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
    Any arg = Any::make(7.0); // conversion from double to int
    ctx.start();
    int sum = 0;
    for (int i=0;i<10000;++i) {
      auto out = m_add.invoke(&o, &arg, 1).value();
      sum += out.as<int>();
    }
    ctx.doNotOptimize(sum);
    ctx.stop(); }, "Method invoke add(conv double->int) 10k");

  auto results = Benchmark::RunAll<Milliseconds>();
  Benchmark::PrintSummaryTable(std::cout, results);
  return 0;
}
