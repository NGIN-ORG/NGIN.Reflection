// BaseMetadata.cpp - tests for base-class metadata

#include <catch2/catch_test_macros.hpp>

#include <NGIN/Reflection/Reflection.hpp>

namespace BaseDemo
{
  struct Base
  {
    int id{};
  };

  struct Derived : Base
  {
    int value{};

    static Derived *Downcast(Base *b) { return static_cast<Derived *>(b); }

    friend void ngin_reflect(NGIN::Reflection::Tag<Derived>, NGIN::Reflection::TypeBuilder<Derived> &b)
    {
      b.set_name("BaseDemo::Derived");
      b.field<&Derived::value>("value");
      b.base<Base, &Downcast>();
    }
  };

  inline void ngin_reflect(NGIN::Reflection::Tag<Base>, NGIN::Reflection::TypeBuilder<Base> &b)
  {
    b.set_name("BaseDemo::Base");
    b.field<&Base::id>("id");
  }
} // namespace BaseDemo

TEST_CASE("BaseMetadataProvidesUpcastAndDowncast", "[reflection][Base]")
{
  using namespace NGIN::Reflection;
  using BaseDemo::Base;
  using BaseDemo::Derived;

  auto t = GetType<Derived>();
  auto bt = GetType<Base>();
  CHECK(t.BaseCount() == 1);
  CHECK(t.IsDerivedFrom(bt));

  auto base = t.BaseAt(0);
  Derived d{};
  d.id = 7;
  d.value = 11;
  auto *bp = static_cast<Base *>(base.Upcast(&d));
  CHECK(bp != nullptr);
  CHECK(bp->id == 7);
  CHECK(base.CanDowncast());
  auto *dp = static_cast<Derived *>(base.Downcast(bp));
  CHECK(dp != nullptr);
  CHECK(dp->value == 11);
}
