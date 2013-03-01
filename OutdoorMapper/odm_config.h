#ifndef ODM__CONFIG_H
#define ODM__CONFIG_H

#define ODM_INTERFACE __declspec(novtable)

#define ODM_OS WINDOWS
#define ODM_REG_PATH L"Software\\OutdoorMapper"

#endif


#ifdef _MSC_VER
#define PACKED_STRUCT(...) __pragma(pack(push, 1)); \
                           __VA_ARGS__;             \
                           __pragma(pack(pop));
#elif defined(__GNUC__)
#  error missing implementation
#endif
