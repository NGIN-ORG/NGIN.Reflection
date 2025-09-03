#include <NGIN/Reflection/Reflection.hpp>
#include <NGIN/Meta/TypeName.hpp>

#include <iostream>
#include <vector>

namespace Demo {
  struct Foo { int a; };
  template<typename T>
  struct Box { T value; };
}

int main() {
  using NGIN::Reflection::LibraryName;
  std::cout << "Library: " << LibraryName() << "\n";

  // Use the reflection helper implemented in this lib
  std::cout << "TypeName_Int(): " << NGIN::Reflection::TypeName_Int() << "\n";

  // Use NGIN.Base Meta facilities transitively via NGIN::Reflection
  std::cout << "TypeName<Foo>::qualified: "
            << NGIN::Meta::TypeName<Demo::Foo>::qualifiedName << "\n";

  std::cout << "TypeName<std::vector<float>>::unqualified: "
            << NGIN::Meta::TypeName<std::vector<float>>::unqualifiedName << "\n";

  std::cout << "TypeName<Box<int>>::qualified: "
            << NGIN::Meta::TypeName<Demo::Box<int>>::qualifiedName << "\n";

  return 0;
}

