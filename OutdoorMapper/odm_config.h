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

// Disable warning about exporting STL classes as part of the DLL interface.
// Cf. http://stackoverflow.com/a/5664491
// The gist is: if we compile all our code with the same compiler version and
// options, we can ignore this. We want to do that anyway for other reasons
// (see http://www.virtualdub.org/blog/pivot/entry.php?id=296), so just go
// ahead and suppress the warning.
#pragma warning( disable : 4251 )

#elif defined(__GNUC__)
#  error missing implementation for gcc
#else
#  error missing implementation
#endif

#endif
