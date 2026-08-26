#pragma once

namespace NGIN::Reflection::detail {
/// @brief Forward declaration of the descriptor specialized by generated code.
template <class T> struct GeneratedDescriptor;
} // namespace NGIN::Reflection::detail

#if defined(NGIN_METAGEN_SCAN)
#define NGIN_METAGEN_ANNOTATION(kind, payload) [[ngin::kind(payload)]]
/// @brief Mark a class, struct, or enum for reflection generation.
#define NGIN_REFLECT(...) NGIN_METAGEN_ANNOTATION(reflect, #__VA_ARGS__)
/// @brief Customize reflection for a data member.
#define NGIN_FIELD(...) NGIN_METAGEN_ANNOTATION(field, #__VA_ARGS__)
/// @brief Mark a field or accessor as part of a reflected property.
#define NGIN_PROPERTY(...) NGIN_METAGEN_ANNOTATION(property, #__VA_ARGS__)
/// @brief Mark a member function for reflection generation.
#define NGIN_METHOD(...) NGIN_METAGEN_ANNOTATION(method, #__VA_ARGS__)
/// @brief Mark a constructor for reflection generation.
#define NGIN_CTOR(...) NGIN_METAGEN_ANNOTATION(ctor, #__VA_ARGS__)
/// @brief Mark a constructor as the dependency-injection constructor.
#define NGIN_INJECT NGIN_METAGEN_ANNOTATION(ctor, "injectable")
/// @brief Configure one dependency-injection constructor parameter.
#define NGIN_DEPENDENCY(...) NGIN_METAGEN_ANNOTATION(dependency, #__VA_ARGS__)
/// @brief Customize reflection for an enumerator.
#define NGIN_ENUM_VALUE(...) NGIN_METAGEN_ANNOTATION(enum_value, #__VA_ARGS__)
/// @brief Customize reflection for a direct base class.
#define NGIN_BASE(...) NGIN_METAGEN_ANNOTATION(base, #__VA_ARGS__)
/// @brief Exclude the following declaration from reflection generation.
#define NGIN_IGNORE NGIN_METAGEN_ANNOTATION(ignore, "")
/// @brief Grant generated descriptors access to non-public reflected members.
#define NGIN_GENERATED_BODY()                                                  \
  NGIN_METAGEN_ANNOTATION(generated_body, "")                                  \
  template <class>                                                             \
  friend struct ::NGIN::Reflection::detail::GeneratedDescriptor;
#else
#define NGIN_REFLECT(...)
#define NGIN_FIELD(...)
#define NGIN_PROPERTY(...)
#define NGIN_METHOD(...)
#define NGIN_CTOR(...)
#define NGIN_INJECT
#define NGIN_DEPENDENCY(...)
#define NGIN_ENUM_VALUE(...)
#define NGIN_BASE(...)
#define NGIN_IGNORE
#define NGIN_GENERATED_BODY()                                                  \
  template <class>                                                             \
  friend struct ::NGIN::Reflection::detail::GeneratedDescriptor;
#endif
