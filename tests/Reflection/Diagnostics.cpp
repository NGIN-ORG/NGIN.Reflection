// Diagnostics.cpp — tests for overload diagnostics and closest match

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

#include <string>

namespace DiagDemo
{
  struct D
  {
    int f(int) const { return 1; }
    double f(double) const { return 2.0; }
    friend void ngin_reflect(NGIN::Reflection::Tag<D>, NGIN::Reflection::TypeBuilder<D> &b)
    {
      b.method<static_cast<int (D::*)(int) const>(&D::f)>("f");
      b.method<static_cast<double (D::*)(double) const>(&D::f)>("f");
    }
  };
} // namespace DiagDemo

TEST_CASE("ResolveDiagnosticsReportNonConvertible", "[reflection][Diagnostics]")
{
  using namespace NGIN::Reflection;
  using DiagDemo::D;

  auto t = GetType<D>();
  Any bad[1] = {Any{std::string{"x"}}};
  auto r = t.ResolveMethod("f", bad, 1);
  CHECK_FALSE(r.has_value());
  auto err = r.error();
  CHECK(err.diagnostics.Size() == 2);
  for (NGIN::UIntSize i = 0; i < err.diagnostics.Size(); ++i)
  {
    CHECK(err.diagnostics[i].code == DiagnosticCode::NonConvertible);
    CHECK(err.diagnostics[i].argIndex == 0);
  }
  CHECK(err.closestMethodIndex.has_value());
}

TEST_CASE("ResolveDiagnosticsReportArityMismatch", "[reflection][Diagnostics]")
{
  using namespace NGIN::Reflection;
  using DiagDemo::D;

  auto t = GetType<D>();
  Any two[2] = {Any{1}, Any{2}};
  auto r = t.ResolveMethod("f", two, 2);
  CHECK_FALSE(r.has_value());
  auto err = r.error();
  CHECK(err.diagnostics.Size() == 2);
  for (NGIN::UIntSize i = 0; i < err.diagnostics.Size(); ++i)
  {
    CHECK(err.diagnostics[i].code == DiagnosticCode::ArityMismatch);
    CHECK(err.diagnostics[i].arity == 1);
  }
  CHECK(err.closestMethodIndex.has_value());
}
