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
  std::cout << "std::vector size=" << (unsigned)sv.size() << ", elem1=" << sv.element(1).Cast<int>() << "\n";

  NGIN::Containers::Vector<int> nv;
  nv.PushBack(4);
  nv.PushBack(5);
  auto nvA = MakeSequenceAdapter(nv);
  std::cout << "NGIN::Vector size=" << (unsigned)nvA.size() << ", elem0=" << nvA.element(0).Cast<int>() << "\n";

  auto t = std::make_tuple(7, 8.5f);
  auto ta = MakeTupleAdapter(t);
  std::cout << "tuple size=" << (unsigned)decltype(ta)::size() << ", get<0>=" << ta.get<0>().Cast<int>() << "\n";

  std::variant<int, float> var{42};
  auto va = MakeVariantAdapter(var);
  std::cout << "variant index=" << (unsigned)va.index() << ", value=" << va.get().Cast<int>() << "\n";

  return 0;
}
