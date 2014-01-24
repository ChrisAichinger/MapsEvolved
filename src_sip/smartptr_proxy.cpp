#include <Python.h>
#include <sip.h>
#include <string>

#define sipRegisterPyType           sipAPI->api_register_py_type
#define sipWrapper_Type             sipAPI->api_wrapper_type
#define sipWrapperType_Type         sipAPI->api_wrappertype_type


static PyObject *SmartptrProxy_new(PyTypeObject *otype, PyObject *args,
                                   PyObject *kwds);
static PyObject* SmartptrProxy_getattro(PyObject *o, PyObject *attr_name);
static int SmartptrProxy_setattro(PyObject *o, PyObject *name, PyObject *val);
static void SmartptrProxy_dealloc(PyObject *self);
static int SmartptrProxy_traverse(PyObject *self, visitproc visit, void *arg);
static int SmartptrProxy_clear(PyObject *self);


typedef struct {
    sipWrapper super;

    // target: The object we delegate calls to.
    // Set to NULL by tp_alloc(), it is initialized on first access.
    // The object is obtained by calling get() of SmartptrProxy's subclass.
    PyObject *target;
} SmartptrProxyObject;

PyDoc_STRVAR(SmartptrProxy_doc,
"Helper class for handling smart pointers with SIP\n\
\n\
To use this class, inherit the smart pointer class from it (via the \n\
SIP SuperType class annotation). Provide a get() function \n\
that returns the base object, which is encapsulated by the smart \n\
pointer. Calls and attribute accesses will then be forwarded to \n\
that object.\n\
\n\
Python code should not directly use this class.\n\
");

static sipWrapperType SmartptrProxyType = { { {
        PyVarObject_HEAD_INIT(0, 0)  /* metacls, set by smartptr_proxy_init */
        "SmartptrProxy",             /* tp_name, set by smartptr_proxy_init */
        sizeof(SmartptrProxyObject), /* tp_basicsize */
        0,                           /* tp_itemsize */
        SmartptrProxy_dealloc,       /* tp_dealloc */
        0,                           /* tp_print */
        0,                           /* tp_getattr */
        0,                           /* tp_setattr */
        0,                           /* tp_reserved */
        0,                           /* tp_repr */
        0,                           /* tp_as_number */
        0,                           /* tp_as_sequence */
        0,                           /* tp_as_mapping */
        0,                           /* tp_hash  */
        0,                           /* tp_call */
        0,                           /* tp_str */
        SmartptrProxy_getattro,      /* tp_getattro */
        SmartptrProxy_setattro,      /* tp_setattro */
        0,                           /* tp_as_buffer */
        (Py_TPFLAGS_DEFAULT |
         Py_TPFLAGS_BASETYPE |
         Py_TPFLAGS_HAVE_GC),        /* tp_flags */
        SmartptrProxy_doc,           /* tp_doc */
        SmartptrProxy_traverse,      /* tp_traverse */
        SmartptrProxy_clear,         /* tp_clear */
        0,                           /* tp_richcompare */
        0,                           /* tp_weaklistoffset */
        0,                           /* tp_iter */
        0,                           /* tp_iternext */
        0,                           /* tp_methods */
        0,                           /* tp_members */
        0,                           /* tp_getset */
        0,                           /* tp_base, set by smartptr_proxy_init */
        0,                           /* tp_dict */
        0,                           /* tp_descr_get */
        0,                           /* tp_descr_set */
        0,                           /* tp_dictoffset */
        0,                           /* tp_init */
        0,                           /* tp_alloc */
        SmartptrProxy_new,           /* tp_new */
        0,                           /* tp_free */
    } },
    0,
    0,
};
static PyTypeObject * const SmartptrProxyPyType =
             reinterpret_cast<PyTypeObject *>(&SmartptrProxyType);


void smartptr_proxy_init(const char* mod_name, const sipAPIDef *sipAPI) {
    static std::string type_name;
    type_name = mod_name;
    type_name += ".SmartptrProxy";

    SmartptrProxyPyType->tp_name = type_name.c_str();
    SmartptrProxyPyType->ob_base.ob_base.ob_type = sipWrapperType_Type;
    SmartptrProxyPyType->tp_base = sipWrapper_Type;

    if (PyType_Ready(SmartptrProxyPyType) < 0)
        Py_FatalError("SmartptrProxy: Failed to initialize type.");

    if (sipRegisterPyType(SmartptrProxyPyType) < 0)
        Py_FatalError("SmartptrProxy: Failed to register type with SIP.");
}

void smartptr_proxy_post_init(PyObject *module_dict) {
    // Add the SmartptrProxy to the module dictionary.
    if (PyDict_SetItemString(module_dict, "SmartptrProxy",
                             &SmartptrProxyPyType->ob_base.ob_base) < 0)
        Py_FatalError("SmartptrProxy: Failed to add to module dict.");
}


static PyObject *SmartptrProxy_new(PyTypeObject *otype, PyObject *args,
                                   PyObject *kwds)
{
    // Ensure SmartptrProxy is not being used directly, it must be subclassed.
    if (otype == SmartptrProxyPyType) {
        PyErr_Format(PyExc_TypeError,
                "the %s type cannot be instantiated or sub-classed",
                otype->tp_name);
        return NULL;
    }
    PyObject* o = SmartptrProxyPyType->tp_base->tp_new(otype, args, kwds);
    return o;
}

static bool populate_target(PyObject *o) {
    if (!PyType_IsSubtype(Py_TYPE(o), SmartptrProxyPyType)) {
        PyErr_Format(PyExc_TypeError,
                     "object's class '%.200s' not a subtype of SmartptrProxy",
                     Py_TYPE(o)->tp_name);
        return false;
    }

    auto spo = reinterpret_cast<SmartptrProxyObject*>(o);
    if (spo->target)
        return true;

    // We obtain the get() function and call it ourselves. We can't use
    // PyObject_CallMethod(), since it calls PyObject_GetAttrString()
    // internally, but we need PyObject_GenericGetAttr() instead.
    PyObject *pyname = PyUnicode_FromString("get");
    if (!pyname) {
        return false;
    }

    PyObject *func = PyObject_GenericGetAttr(o, pyname);
    if (!func) {
        Py_DECREF(pyname);
        return false;
    } else {
        Py_DECREF(pyname);
    }

    PyObject *args = PyTuple_New(0);
    if (!args) {
        Py_DECREF(func);
        return false;
    }

    spo->target = PyObject_Call(func, args, NULL);
    if (!spo->target) {
        Py_DECREF(func);
        Py_DECREF(args);
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_AttributeError,
                         "retrieving wrapped instance via %.200s.get() failed",
                         Py_TYPE(o)->tp_name);
        return false;
    }

    Py_DECREF(func);
    Py_DECREF(args);
    return true;
}

static PyObject* SmartptrProxy_getattro(PyObject *o, PyObject *name) {
    if (!PyUnicode_Check(name)){
        PyErr_Format(PyExc_TypeError,
                     "attribute name must be string, not '%.200s'",
                     Py_TYPE(name)->tp_name);
        return NULL;
    }
    Py_INCREF(name);

    if (PyUnicode_CompareWithASCIIString(name, "get") == 0) {
        PyObject* result = PyObject_GenericGetAttr(o, name);
        Py_DECREF(name);
        return result;
    }

    if (!populate_target(o)) {
        Py_DECREF(name);
        return NULL;
    }

    Py_DECREF(name);
    auto spo = reinterpret_cast<SmartptrProxyObject*>(o);
    return PyObject_GetAttr(spo->target, name);
}

static int SmartptrProxy_setattro(PyObject *o, PyObject *name, PyObject *val) {
    if (!PyUnicode_Check(name)){
        PyErr_Format(PyExc_TypeError,
                     "attribute name must be string, not '%.200s'",
                     Py_TYPE(name)->tp_name);
        return -1;
    }
    Py_INCREF(name);

    if (PyUnicode_CompareWithASCIIString(name, "get") == 0) {
        PyErr_Format(PyExc_AttributeError,
                     "can't set attribute 'get' of SmartptrProxy object");
        Py_DECREF(name);
        return -1;
    }

    if (!populate_target(o)) {
        Py_DECREF(name);
        return -1;
    }

    auto spo = reinterpret_cast<SmartptrProxyObject*>(o);
    int res = PyObject_SetAttr(spo->target, name, val);
    Py_DECREF(name);
    return res;
}

static void SmartptrProxy_dealloc(PyObject *self) {
    PyObject_GC_UnTrack(self);        // must untrack first
    Py_CLEAR(reinterpret_cast<SmartptrProxyObject*>(self)->target);
    return SmartptrProxyPyType->tp_base->tp_dealloc(self);
}

static int SmartptrProxy_traverse(PyObject *self, visitproc visit, void *arg) {
    Py_VISIT(reinterpret_cast<SmartptrProxyObject*>(self)->target);
    return SmartptrProxyPyType->tp_base->tp_traverse(self, visit, arg);
}

static int SmartptrProxy_clear(PyObject *self) {
    Py_CLEAR(reinterpret_cast<SmartptrProxyObject*>(self)->target);
    return SmartptrProxyPyType->tp_base->tp_clear(self);
}
