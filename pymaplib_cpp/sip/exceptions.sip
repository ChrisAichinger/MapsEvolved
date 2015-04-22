// SIP exception handling:
// Only one exception can be "Default" in each module.
// A catch block is generated for it where its RaiseCode is invoked.
// If the function is annotated with throw(...) exception specifications,
// further catch clause are generated for each of them.
// SIP also adds a catch-all clause and generates an UnknownException
// if no user-defined handler matched.
// Unfortunately, if a throw() specification is present, SIP also exposes
// it to the C++ side. This means that if a not-listed exception is thrown,
// the program will terminate.

%ModuleHeaderCode
// Use RTTI to get as much information from the exception as possible.
#include <typeinfo>
#define DO_RAISE(exc, cls)                                      \
    do {                                                        \
        const char *exc_name = typeid(exc).name();              \
        const char *detail = exc.what();                        \
        SIP_BLOCK_THREADS                                       \
        PyErr_Format(cls, "<%s>: %s", exc_name, detail);        \
        SIP_UNBLOCK_THREADS                                     \
    } while(0)
%End

// Make std::runtime_error the default exception to catch
// if no throw(...) exception specification was given.
%Exception std::exception(SIP_Exception) /Default,PyName=StdException/ {
%TypeHeaderCode
#include <exception>
%End
%RaiseCode
    DO_RAISE(sipExceptionRef, sipException_std_exception);
%End
};

%Exception std::runtime_error(SIP_Exception) /PyName=StdRuntimeError/ {
%TypeHeaderCode
#include <stdexcept>
%End
%RaiseCode
    DO_RAISE(sipExceptionRef, sipException_std_runtime_error);
%End
};

