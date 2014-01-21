// github.com/tagged/digsby/blob/master/digsby/ext/src/BuddyList/sip/wstring.sip
%MappedType std::string
{
%TypeHeaderCode
#include <string>
%End

// Convert std::wstring to a python str object.
%ConvertFromTypeCode
    return PyBytes_FromStringAndSize(sipCpp->c_str(), sipCpp->size());
%End

// Convert Python str objects to std::wstring.
%ConvertToTypeCode
    // If sipIsErr is NULL we must only perform a type check.
    if (!sipIsErr)
        return PyBytes_Check((PyUnicodeObject*)sipPy);

    const char *bytes = PyBytes_AsString(sipPy);
    ssize_t size = PyBytes_Size(sipPy);
    *sipCppPtr = new std::string(bytes, size);
    return sipGetState(sipTransferObj);
%End
};


%MappedType std::wstring
{
%TypeHeaderCode
#include <string>
#include <memory>
%End

// Convert std::wstring to a python str object.
%ConvertFromTypeCode
    return PyUnicode_FromWideChar(sipCpp->c_str(), sipCpp->size());
%End

// Convert Python str objects to std::wstring.
%ConvertToTypeCode
    // If sipIsErr is NULL we must only perform a type check.
    if (!sipIsErr)
        return PyUnicode_Check(sipPy);

    const size_t size = PyUnicode_GetSize(sipPy);
    if (!size) {
        *sipCppPtr = new std::wstring();
        return sipGetState(sipTransferObj);
    }

    std::unique_ptr<wchar_t[]> wchar_buf(new wchar_t[size]);
    PyUnicode_AsWideChar(sipPy, wchar_buf.get(), size);
    *sipCppPtr = new std::wstring(wchar_buf.get(), size);
    return sipGetState(sipTransferObj);
%End
};


%MappedType std::vector<std::wstring> {
%TypeHeaderCode
#include <string>
#include <vector>
%End

%ConvertFromTypeCode
    // Handle no list.
    if (!sipCpp)
        return PyList_New(0);

    // Create the list.
    unsigned int size = sipCpp->size();
    PyObject *lst = PyList_New(size);
    if (!lst)
        return NULL;

    // Convert all wstrings to python str's
    for (unsigned int i=0; i < size; ++i) {
        PyObject *tobj = sipConvertFromType(&sipCpp->at(i),
                                            sipType_std_wstring,
                                            sipTransferObj);
        if (!tobj) {
            Py_DECREF(lst);
            return NULL;
        }
        if (PyList_SetItem(lst, i, tobj) < 0) {
            Py_DECREF(tobj);
            Py_DECREF(lst);
            return NULL;
        }
    }
    return lst;
%End

%ConvertToTypeCode
    // Convert a Python list of string to a vector<string> on the heap.
    if (sipIsErr == NULL)
        return PyList_Check(sipPy);

    if (sipPy == Py_None) {
        *sipCppPtr = NULL;
        return 0;
    }

    std::vector<std::wstring> *qvl = new std::vector<std::wstring>;
    PyErr_Clear();

    unsigned int size = PyList_Size(sipPy);
    for (unsigned int i = 0; i < size; ++i) {
        // Use the SIP API to convert the Python object to the
        // corresponding C++ instance.  Note that we apply any ownership
        // transfer to the list itself, not the individual elements.
        std::wstring *t = reinterpret_cast<std::wstring*>(
                sipConvertToType(PyList_GET_ITEM(sipPy, i),
                                 sipType_std_wstring, 0, 0, 0, sipIsErr));

        if (*sipIsErr) {
            delete qvl;
            return 0;
        }

        // Add the pointer to the C++ instance.
        qvl->push_back(*t);
        delete t;
    }
    *sipCppPtr = qvl;
    return 1;
%End
};

%MappedType std::vector<int>
{
%TypeHeaderCode
#include <vector>
%End

%ConvertFromTypeCode
    // Handle no list.
    if (!sipCpp)
        return PyList_New(0);

    // Create the list.
    unsigned int size = sipCpp->size();
    PyObject *lst = PyList_New(size);
    if (!lst)
        return NULL;

    // Convert all wstrings to python str's
    for (unsigned int i=0; i < size; ++i) {
        long j = sipCpp->operator[](i);
        PyObject *tobj = PyLong_FromLong(j);
        if (!tobj) {
            Py_DECREF(lst);
            return NULL;
        }
        if (PyList_SetItem(lst, i, tobj) < 0) {
            Py_DECREF(tobj);
            Py_DECREF(lst);
            return NULL;
        }
    }
    return lst;
%End

%ConvertToTypeCode
    // Convert a Python list of longs to a vector<int> on the heap.
    if (sipIsErr == NULL)
        return PyList_Check(sipPy);

    if (sipPy == Py_None) {
        *sipCppPtr = NULL;
        return 0;
    }

    std::vector<int> *qvl = new std::vector<int>;
    PyErr_Clear();

    unsigned int size = PyList_Size(sipPy);
    for (unsigned int i = 0; i < size; ++i) {
	long j = PyLong_AsLong(PyList_GET_ITEM(sipPy, i));
        if (PyErr_Occurred()) {
            *sipIsErr = 1;
            delete qvl;
            return 0;
        }
        qvl->push_back(j);
    }
    *sipCppPtr = qvl;
    return 1;
%End
};


%MappedType std::vector<unsigned int>
{
%TypeHeaderCode
#include <vector>
%End

%ConvertFromTypeCode
    // Handle no list.
    if (!sipCpp)
        return PyList_New(0);

    // Create the list.
    unsigned int size = sipCpp->size();
    PyObject *lst = PyList_New(size);
    if (!lst)
        return NULL;

    // Convert all wstrings to python str's
    for (unsigned int i=0; i < size; ++i) {
        unsigned int j = sipCpp->operator[](i);
        PyObject *tobj = PyLong_FromUnsignedLong(j);
        if (!tobj) {
            Py_DECREF(lst);
            return NULL;
        }
        if (PyList_SetItem(lst, i, tobj) < 0) {
            Py_DECREF(tobj);
            Py_DECREF(lst);
            return NULL;
        }
    }
    return lst;
%End

%ConvertToTypeCode
    // Convert a Python list of longs to a vector<int> on the heap.
    if (sipIsErr == NULL)
        return PyList_Check(sipPy);

    if (sipPy == Py_None) {
        *sipCppPtr = NULL;
        return 0;
    }

    std::vector<unsigned int> *qvl = new std::vector<unsigned int>;
    PyErr_Clear();
    unsigned int size = PyList_Size(sipPy);
    for (unsigned int i = 0; i < size; ++i) {
	long j = PyLong_AsUnsignedLong(PyList_GET_ITEM(sipPy, i));
        if (PyErr_Occurred()) {
            *sipIsErr = 1;
            delete qvl;
            return 0;
        }
        qvl->push_back(j);
    }
    *sipCppPtr = qvl;
    return 1;
%End
};
