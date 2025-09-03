#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #if defined(NGIN_REFLECTION_STATIC)
    #define NGIN_REFLECTION_API
  #else
    #if defined(NGIN_REFLECTION_EXPORTS)
      #define NGIN_REFLECTION_API __declspec(dllexport)
    #else
      #define NGIN_REFLECTION_API __declspec(dllimport)
    #endif
  #endif
#else
  #define NGIN_REFLECTION_API
#endif

