#include <NGIN/Reflection/Adapters.hpp>
#include <iostream>
#include <tuple>
#include <variant>
#include <vector>

int main()
{
  using namespace NGIN::Reflection::Adapters;

  std::vector<int> v{1, 2, 3};
  auto sv = MakeSequenceAdapter(v);
  std::cout << "std::vector size=" << (unsigned)sv.Size() << ", elem1=" << sv.Element(1).Cast<int>() << "\n";

  NGIN::Containers::Vector<int> nv;
  nv.PushBack(4);
  nv.PushBack(5);
  auto nvA = MakeSequenceAdapter(nv);
  std::cout << "NGIN::Vector size=" << (unsigned)nvA.Size() << ", elem0=" << nvA.Element(0).Cast<int>() << "\n";

  auto t = std::make_tuple(7, 8.5f);
  auto ta = MakeTupleAdapter(t);
  std::cout << "tuple size=" << (unsigned)decltype(ta)::Size() << ", get<0>=" << ta.Get<0>().Cast<int>() << "\n";

  std::variant<int, float> var{42};
  auto va = MakeVariantAdapter(var);
  std::cout << "variant index=" << (unsigned)va.Index() << ", value=" << va.Get().Cast<int>() << "\n";

  return 0;
}
