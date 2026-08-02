#pragma once

#if defined(__clang__)
#define NGIN_METAGEN_ANNOTATE(payload) [[clang::annotate(payload)]]
#else
#define NGIN_METAGEN_ANNOTATE(payload)
#endif

#define NGIN_REFLECT(...) NGIN_METAGEN_ANNOTATE("ngin.reflect:" #__VA_ARGS__)
#define NGIN_FIELD(...) NGIN_METAGEN_ANNOTATE("ngin.field:" #__VA_ARGS__)
#define NGIN_PROPERTY(...) NGIN_METAGEN_ANNOTATE("ngin.property:" #__VA_ARGS__)
#define NGIN_METHOD(...) NGIN_METAGEN_ANNOTATE("ngin.method:" #__VA_ARGS__)
#define NGIN_CTOR(...) NGIN_METAGEN_ANNOTATE("ngin.ctor:" #__VA_ARGS__)
#define NGIN_INJECT NGIN_METAGEN_ANNOTATE("ngin.ctor:injectable")
#define NGIN_DEPENDENCY(...) NGIN_METAGEN_ANNOTATE("ngin.dependency:" #__VA_ARGS__)
#define NGIN_ENUM_VALUE(...) NGIN_METAGEN_ANNOTATE("ngin.enum_value:" #__VA_ARGS__)
#define NGIN_BASE(...) NGIN_METAGEN_ANNOTATE("ngin.base:" #__VA_ARGS__)
#define NGIN_IGNORE NGIN_METAGEN_ANNOTATE("ngin.ignore")
