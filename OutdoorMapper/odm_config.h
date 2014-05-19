#ifndef ODM__CONFIG_H
#define ODM__CONFIG_H


#ifdef _MSC_VER

#define ODM_OS WINDOWS
#define ODM_INTERFACE __declspec(novtable)
#define PACKED_STRUCT(...) __pragma(pack(push, 1)); \
                           __VA_ARGS__;             \
                           __pragma(pack(pop));
#ifdef ODM_SHARED_LIB
#  define EXPORT __declspec(dllexport)
#elif ODM_EXECUTABLE
#  define EXPORT
#else
#  define EXPORT __declspec(dllimport)
#endif
#pragma warning( disable : 4251 )

#elif defined(__GNUC__)
#  error missing implementation for gcc
#else
#  error missing implementation
#endif

#endif
