#ifndef PROXY_EXPORT_H
#define PROXY_EXPORT_H

// Export symbols
#if defined _WIN32 || defined __CYGWIN__ || defined __MINGW32__
#ifdef MICRO_PROXY_BUILD_LIBRARY
#ifdef __GNUC__
#define OVERRIDE_EXPORT __attribute__ ((dllexport))
#else
#define OVERRIDE_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define OVERRIDE_EXPORT __attribute__ ((dllimport))
#else
#define OVERRIDE_EXPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#else
#if __GNUC__ >= 4
#define OVERRIDE_EXPORT __attribute__ ((visibility ("default")))
#else
#define OVERRIDE_EXPORT
#endif
#endif


#ifdef __cplusplus
#define MICRO_EXTERN_C extern "C"
#else
#define MICRO_EXTERN_C
#endif

#endif
