#include <Python.h>

#include <apache-ws-common.h>
typedef struct py_apachews_server {
    PyObject_HEAD
    apachews_context *context;
} py_apachews_server;

typedef struct py_apachews_event {
    PyObject_HEAD
    apachews_event *event;
} py_apachews_event;

typedef struct py_apachews_client {
    PyObject_HEAD
    apachews_client *client;
    long int guid;
} py_apachews_client;

#define GUIDFMT "%04x%04x-%04x-%04x-%04x-%04x%04x%04x"

#define __apachews_server_doc__ "Apache WebSocket Module Connection Object"
#define __apachews_event_doc__ "Apache WebSocket Module Event Object"
#define __apachews_client_doc__ "Apache WebSocket Module Client Object"

static PyObject *py_apachews_server_broadcast(PyObject *self, PyObject *args);
static PyObject *py_apachews_server_dequeue(PyObject *self, PyObject *args);
static PyObject *py_apachews_server_language(PyObject *self, PyObject *args);

static PyObject *py_apachews_event_respond(PyObject *self, PyObject *args);
static PyObject *py_apachews_event_read(PyObject *self, PyObject *args);
static PyObject *py_apachews_event_get_type(PyObject *self, PyObject *args);
static PyObject *py_apachews_event_get_client(PyObject *self, PyObject *args);

static PyObject *py_apachews_client_guid(PyObject *self, PyObject *args);
static PyObject *py_apachews_client_close(PyObject *self, PyObject *args);
static PyObject *py_apachews_client_send(PyObject *self, PyObject *args);

static int py_apachews_server_init(PyObject *self, PyObject *args, PyObject *kwds);
static void py_apachews_server_free(PyObject *self);

static int py_apachews_event_init(PyObject *self, PyObject *args, PyObject *kwds);
static void py_apachews_event_free(PyObject *self);

static int py_apachews_client_init(PyObject *self, PyObject *args, PyObject *kwds);
static void py_apachews_client_free(PyObject *self);

typedef struct py_enum {
    const char *name;
    long int value;
} py_enum;

static const py_enum py_apachews_event_types[] = {
    {"Invalid", ApacheWSInvalidEvent},
    {"Accept", ApacheWSAcceptEvent},
    {"IO", ApacheWSIOEvent},
    {"NoData", ApacheWSNoData},
    {"ConnectionClosed", ApacheWSConnectionClosed},
    {"Error", ApacheWSError},
    {NULL, -1}
};

static PyMethodDef py_apachews_server_methods[] = {
    {"dequeue", py_apachews_server_dequeue, METH_NOARGS, NULL},
    {"broadcast", py_apachews_server_broadcast, METH_NOARGS, NULL},
    {"language", py_apachews_server_language, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyMethodDef py_apachews_client_methods[] = {
    {"send", py_apachews_client_send, METH_O, NULL},
    {"guid", py_apachews_client_guid, METH_NOARGS, NULL},
    {"close", py_apachews_client_close, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyMethodDef py_apachews_event_methods[] = {
    {"respond", py_apachews_event_respond, METH_O, NULL},
    {"read", py_apachews_event_read, METH_NOARGS, NULL},
    {"type", py_apachews_event_get_type, METH_NOARGS, NULL},
    {"client", py_apachews_event_get_client, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyMethodDef py_apachews_module_methods[] = {{NULL, NULL, 0, NULL}};
static struct PyModuleDef py_apachews_module = {
   PyModuleDef_HEAD_INIT,
   /* name of module */
   "apachews",
   /* module documentation, may be NULL */
   NULL,
   /* size of per-interpreter state of the module, or -1
    * if the module keeps state in global variables.
    */
   -1,
   py_apachews_module_methods,
   NULL,
   NULL,
   NULL,
   NULL
};

static PyTypeObject py_apachews_server_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Server",                  /* tp_name */
    sizeof(py_apachews_server),/* tp_basicsize */
    0,                         /* tp_itemsize */
    py_apachews_server_free,   /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_as_async */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    __apachews_server_doc__,   /* tp_doc */
    0,                         /* tp_travers */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_next */
    py_apachews_server_methods,/* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    py_apachews_server_init,   /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,         /* tp_new */
    0,                         /* tp_free */
    0,                         /* tp_is_gc */
    0,                         /* tp_bases */
    0,                         /* tp_mro */
    0,                         /* tp_cache */
    0,                         /* tp_subclasses */
    0,                         /* tp_weaklist */
    0,                         /* tp_del */
    0,                         /* tp_version_tag */
    0                          /* tp_finalize */
};

static PyTypeObject py_apachews_event_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Event",                   /* tp_name */
    sizeof(py_apachews_event), /* tp_basicsize */
    0,                         /* tp_itemsize */
    py_apachews_event_free,    /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_as_async */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    __apachews_event_doc__,    /* tp_doc */
    0,                         /* tp_travers */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_next */
    py_apachews_event_methods, /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    py_apachews_event_init,    /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,         /* tp_new */
    0,                         /* tp_free */
    0,                         /* tp_is_gc */
    0,                         /* tp_bases */
    0,                         /* tp_mro */
    0,                         /* tp_cache */
    0,                         /* tp_subclasses */
    0,                         /* tp_weaklist */
    0,                         /* tp_del */
    0,                         /* tp_version_tag */
    0                          /* tp_finalize */
};

static PyTypeObject py_apachews_client_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Client",                  /* tp_name */
    sizeof(py_apachews_client),/* tp_basicsize */
    0,                         /* tp_itemsize */
    py_apachews_client_free,   /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_as_async */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    __apachews_client_doc__,   /* tp_doc */
    0,                         /* tp_travers */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_next */
    py_apachews_client_methods,/* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    py_apachews_client_init,   /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,         /* tp_new */
    0,                         /* tp_free */
    0,                         /* tp_is_gc */
    0,                         /* tp_bases */
    0,                         /* tp_mro */
    0,                         /* tp_cache */
    0,                         /* tp_subclasses */
    0,                         /* tp_weaklist */
    0,                         /* tp_del */
    0,                         /* tp_version_tag */
    0                          /* tp_finalize */
};

static int
py_apachews_ierror(PyObject *exc, const char *const msg, ...)
{
    PyErr_SetString(exc, msg);
    return -1;
}

static PyObject *
py_apachews_error(PyObject *exc, const char *const msg, ...)
{
    PyErr_SetString(exc, msg);
    return NULL;
}

static int
py_apachews_server_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    py_apachews_server *ws;
    char *path;
    // We expect the path to the file that will be used
    // as a Unix Domain Socket address
    if (PyArg_ParseTuple(args, "s", &path) < 1)
        return py_apachews_ierror(PyExc_ConnectionError, "a filesystem path is expected");
    // Cast the object to the appropriate type
    ws = (py_apachews_server *) self;
    if (ws == NULL)
        return -1;
    // Create a new context
    ws->context = apachews_create(path);
    if (ws->context == NULL)
        return py_apachews_ierror(PyExc_ConnectionError, "cannot create the connection");
    (void) kwds;
    return 0;
}

static int
py_apachews_client_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    py_apachews_client *client;
    // Cast the object to the appropriate type
    client = (py_apachews_client *) self;
    if (client == NULL)
        return -1;
    return 0;
}

static PyObject *
py_apachews_event_respond(PyObject *self, PyObject *args)
{
    py_apachews_event *instance;
    PyObject *bytes;
    Py_ssize_t length;
    uint8_t *buffer;
    // We expect a 'bytes' object
    if (PyArg_Parse(args, "S", &bytes) < 1)
        return py_apachews_error(PyExc_TypeError, "expected bytes as argument");
    // Make a pointer of the appropriate type
    instance = (py_apachews_event *) self;
    if (instance == NULL)
        return py_apachews_error(PyExc_Exception, "unknown error");
    // Attempt extracting the raw bytes
    if (PyBytes_AsStringAndSize(bytes, (char **) &buffer, &length) == -1)
        return py_apachews_error(PyExc_ValueError, "invalid parameter detected");
    if ((length = apachews_event_respond(instance->event, buffer, length)) == -1)
        goto error;
    return Py_BuildValue("l", length);
error:
    return py_apachews_error(PyExc_IOError, "I/O error while writing");
}

static PyObject *
py_apachews_event_read(PyObject *self, PyObject *args)
{
    size_t length;
    uint8_t *data;
    py_apachews_event *instance;
    apachews_status status;
    PyObject *result;
    instance = (py_apachews_event *) self;
    if (instance == NULL)
        return py_apachews_error(PyExc_Exception, "unknown error");
    status = apachews_event_read(instance->event, &data, &length);
    switch (status) {
    case ApacheWSConnectionClosed:
        result = Py_BuildValue("l", ApacheWSConnectionClosed);
        break;
    case ApacheWSSuccess:
        result = Py_BuildValue("y#", data, length);
        free(data);
        break;
    default:
        return py_apachews_error(PyExc_IOError, "I/O error while reading");
    }
    return result;
}

static PyObject *
py_apachews_server_broadcast(PyObject *self, PyObject *args)
{
    py_apachews_server *instance;
    PyObject *bytes;
    Py_ssize_t length;
    uint8_t *buffer;
    // We expect a 'bytes' object
    if (PyArg_Parse(args, "S", &bytes) < 1)
        return py_apachews_error(PyExc_TypeError, "expected bytes as argument");
    // Attempt extracting the raw bytes
    if (PyBytes_AsStringAndSize(bytes, (char **) &buffer, &length) == -1)
        return py_apachews_error(PyExc_ValueError, "invalid parameter detected");
    // Make a pointer with the appropriate type
    instance = (py_apachews_server *) self;
    if (instance == NULL)
        return py_apachews_error(PyExc_Exception, "unknown error");
    if ((length = apachews_server_broadcast(instance->context, buffer, length)) == -1)
        goto error;
    return Py_BuildValue("l", length);
error:
    return py_apachews_error(PyExc_IOError, "I/O error while writing");
}

static PyObject *
py_apachews_server_language(PyObject *self, PyObject *args)
{
    return NULL;
}

static PyObject *
py_apachews_server_dequeue(PyObject *self, PyObject *args)
{
    py_apachews_server *instance;
    PyObject *obj;
    py_apachews_event *result;
    // Make a pointer with the appropriate type
    instance = (py_apachews_server *) self;
    // Create a new 'Event' object, calling the init function too
    obj = PyObject_CallObject((PyObject *) &py_apachews_event_type, NULL);
    if (obj == NULL)
        return py_apachews_error(PyExc_SystemError, "cannot create the `Event' object");
    result = (py_apachews_event *) obj;

    // Initialize the object with the next available event
    Py_BEGIN_ALLOW_THREADS // Or otherwise python GIL will block
                           // other threads when we block here!
    result->event = apachews_server_next_event(instance->context);
    Py_END_ALLOW_THREADS // Allos the GIL to be blocked again

    if (result->event == NULL)
        return py_apachews_error(PyExc_SystemError, "returning an invalid event");
    return obj;
}

static int
py_apachews_event_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject  *base;
    py_apachews_event *ev;
    PyTypeObject *type;
    // Make a pointer with the appropriate type
    ev = (py_apachews_event *) self;
    if (ev == NULL)
        return py_apachews_ierror(PyExc_Exception, "unknwon error");
    base = &(ev->ob_base);
    type = base->ob_type;
    if (type == NULL)
        return py_apachews_ierror(PyExc_ValueError, "invalid object");
    // Here, we create all the static properties of the object
    // that will be used as constants in the python script
    for (size_t idx = 0 ; py_apachews_event_types[idx].name != NULL ; ++idx) {
        const char *name;
        long int value;
        // Just for clarity
        name  = py_apachews_event_types[idx].name;
        value = py_apachews_event_types[idx].value;
        // Append the value
        PyDict_SetItemString(type->tp_dict, name, Py_BuildValue("l", value));
    }
    return 0;
}

static PyObject *
py_apachews_event_get_type(PyObject *self, PyObject *args)
{
    py_apachews_event *ev;
    // Make a pointer with the appropriate type
    ev = (py_apachews_event *) self;
    if (ev->event == NULL)
        return PyLong_FromLong(ApacheWSInvalidEvent);
    // Simply send back the event type
    return PyLong_FromLong(apachews_event_get_type(ev->event));
}

static PyObject *
py_apachews_event_get_client(PyObject *self, PyObject *args)
{
    py_apachews_event *instance;
    PyObject *obj;
    py_apachews_client *result;
    // Make a pointer with the appropriate type
    instance = (py_apachews_event *) self;
    // Create a new 'Event' object, calling the init function too
    obj = PyObject_CallObject((PyObject *) &py_apachews_client_type, NULL);
    if (obj == NULL)
        return py_apachews_error(PyExc_SystemError, "cannot create the `Client' object");
    result = (py_apachews_client *) obj;
    // Initialize the object with the next available event
    result->client = apachews_event_get_client(instance->event);
    result->guid = apachews_client_get_socket(result->client);

    Py_INCREF(result);
    return obj;
}

static PyObject *
py_apachews_client_send(PyObject *self, PyObject *args)
{
    py_apachews_client *cl;
    PyObject *bytes;
    Py_ssize_t length;
    uint8_t *buffer;
    // We expect a 'bytes' object
    if (PyArg_Parse(args, "S", &bytes) < 1)
        return py_apachews_error(PyExc_TypeError, "expected bytes as argument");
    // Make a pointer of the appropriate type
    cl = (py_apachews_client *) self;
    if (cl == NULL)
        goto error;
    // Attempt extracting the raw bytes
    if (PyBytes_AsStringAndSize(bytes, (char **) &buffer, &length) == -1)
        return py_apachews_error(PyExc_ValueError, "invalid parameter detected");
    if ((length = apachews_client_send(cl->client, buffer, length)) == -1)
        goto error;
    return Py_BuildValue("l", length);
error:
    return py_apachews_error(PyExc_IOError, "I/O error while writing");
}

static PyObject *
py_apachews_client_guid(PyObject *self, PyObject *args)
{
    py_apachews_client *cl;
    // Make a pointer of the appropriate type
    cl = (py_apachews_client *) self;
    if (cl == NULL)
        return Py_None;
    return Py_BuildValue("l", cl->guid);
}

static PyObject *
py_apachews_client_close(PyObject *self, PyObject *args)
{
    py_apachews_client *cl;
    // Make a pointer of the appropriate type
    cl = (py_apachews_client *) self;
    if (cl == NULL)
        return Py_BuildValue("l", -1);
    apachews_client_close(cl->client);
    Py_DECREF(cl);

    return Py_BuildValue("l", 0);
}

static void
py_apachews_event_free(PyObject *self)
{
    py_apachews_event *py_event;
    if (self == NULL)
        return;
    py_event = (py_apachews_event *) self;

    apachews_event_free(py_event->event);
    PyMem_DEL(self);
}

static void
py_apachews_client_free(PyObject *self)
{
    py_apachews_client *py_client;
    if (self == NULL)
        return;
    py_client = (py_apachews_client *) self;

    apachews_client_free(py_client->client);
    PyMem_DEL(self);
}

static void
py_apachews_server_free(PyObject *self)
{
    py_apachews_server *py_server;
    if (self == NULL)
        return;
    py_server = (py_apachews_server *) self;

    apachews_context_free(py_server->context);
    PyMem_DEL(self);
}

EXPORT PyObject *
PyInit_apachews(void)
{
    PyObject *module;
    apachews_initialize_os();
    srand(time(NULL));
    // Prepare the base classes to add them
    if (PyType_Ready(&py_apachews_server_type) < 0)
        return NULL;
    if (PyType_Ready(&py_apachews_event_type) < 0)
        return NULL;
    if (PyType_Ready(&py_apachews_client_type) < 0)
        return NULL;
    // Create the apache module
    module = PyModule_Create(&py_apachews_module);
    if (module == NULL)
        return NULL;
    // Add the base classes
    PyModule_AddObject(module, "Server", (PyObject *) &py_apachews_server_type);
    PyModule_AddObject(module, "Event", (PyObject *) &py_apachews_event_type);
    PyModule_AddObject(module, "Client", (PyObject *) &py_apachews_client_type);

    (void) py_apachews_server_free;
    (void) py_apachews_event_free;
    (void) py_apachews_client_free;

    return module;
}
