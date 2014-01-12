#ifndef ODM__CONFIG_H
#define ODM__CONFIG_H

#define ODM_INTERFACE __declspec(novtable)

#define ODM_OS WINDOWS
#define ODM_REG_PATH L"Software\\OutdoorMapper"


#ifdef _MSC_VER
#define PACKED_STRUCT(...) __pragma(pack(push, 1)); \
                           __VA_ARGS__;             \
                           __pragma(pack(pop));

#ifdef ODM_SHARED_LIB
#  define EXPORT __declspec(dllexport)
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