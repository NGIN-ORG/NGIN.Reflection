// Phase1Basic.cpp — Basic smoke tests for Phase 1 reflection

#include <boost/ut.hpp>

#include <NGIN/Reflection/Reflection.hpp>

using namespace boost::ut;

namespace DemoPhase1
{
  struct User
  {
    int id{};
    float score{};

    // ADL friend using builder; omit names to auto-derive from pointer-to-member
    friend void ngin_reflect(NGIN::Reflection::Tag<User>, NGIN::Reflection::Builder<User> &b)
    {
      b.field<&User::id>("id");
      b.field<&User::score>("score");
    }
  };

  struct Named
  {
    int value{};
    friend void ngin_reflect(NGIN::Reflection::Tag<Named>, NGIN::Reflection::Builder<Named> &b)
    {
      b.set_name("My::Named");
      b.field<&Named::value>("v");
    }
  };
}

suite<"NGIN::Reflection::Phase1"> reflPhase1 = []
{
  using namespace NGIN::Reflection;
  using DemoPhase1::User;
  using DemoPhase1::Named;

  "Type_Of_InferName_And_Fields"_test = []
  {
    auto t = TypeOf<User>();
    expect(t.IsValid()) << "TypeOf<User> must be IsValid";
    expect(eq(t.QualifiedName(), std::string_view{NGIN::Meta::TypeName<User>::qualifiedName}))
        << "qualified name should default from Meta::TypeName";
    expect(eq(t.FieldCount(), NGIN::UIntSize{2})) << "two fields registered";

    expect(eq(t.FieldAt(0).name(), std::string_view{"id"}));
    expect(eq(t.FieldAt(1).name(), std::string_view{"score"}));

    User u{};
    *static_cast<int *>(t.FieldAt(0).GetMut(&u)) = 42;
    expect(eq(u.id, 42)) << "mutable access via handle";
  };

  "Type_SetName_And_FieldAlias"_test = []
  {
    auto t = TypeOf<Named>();
    expect(eq(t.QualifiedName(), std::string_view{"My::Named"}));
    auto f = t.GetField("v");
    expect(f.has_value());
  };
};
