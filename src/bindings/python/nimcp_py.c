/**
 * @file nimcp_py.c
 * @brief Python bindings for NIMCP using unified nimcp.h API
 *
 * This is a clean, standalone Python extension that ONLY uses the public
 * nimcp.h API. It does not depend on internal NIMCP headers.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "nimcp.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_learning_constants.h"
//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for py module */
static nimcp_health_agent_t* g_py_health_agent = NULL;

/**
 * @brief Set health agent for py heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
static void py_set_health_agent(nimcp_health_agent_t* agent) {
    g_py_health_agent = agent;
}

/** @brief Send heartbeat from py module */
static inline void py_heartbeat(const char* operation, float progress) {
    if (g_py_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_py_health_agent, operation, progress);
    }
}


//=============================================================================
// Brain Object Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    nimcp_brain_t brain;
} BrainObject;

static void Brain_dealloc(BrainObject* self) {
    if (self->brain) {
        nimcp_brain_destroy(self->brain);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Brain_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    BrainObject* self;
    self = (BrainObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->brain = NULL;
    }
    return (PyObject*)self;
}

static int Brain_init(BrainObject* self, PyObject* args, PyObject* kwds) {
    const char* name;
    int size = NIMCP_BRAIN_SMALL;
    int task = NIMCP_TASK_CLASSIFICATION;
    unsigned int num_inputs = 10;
    unsigned int num_outputs = 10;

    static char* kwlist[] = {"name", "size", "task", "num_inputs", "num_outputs", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|iiII", kwlist,
                                     &name, &size, &task, &num_inputs, &num_outputs)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_init: Invalid arguments");
        return -1;
    }

    self->brain = nimcp_brain_create(name, (nimcp_brain_size_t)size,
                                     (nimcp_brain_task_t)task, num_inputs, num_outputs);

    if (!self->brain) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NOT_INITIALIZED, 0, "python_binding_simple",
                         "Brain_init: Failed to create brain '%s'", name);
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return -1;
    }

    return 0;
}

static PyObject* Brain_learn(BrainObject* self, PyObject* args) {
    PyObject* features_list;
    const char* label;
    float confidence = 1.0f;

    if (!PyArg_ParseTuple(args, "Os|f", &features_list, &label, &confidence)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_learn: Invalid arguments");
        return NULL;
    }

    if (!PyList_Check(features_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_learn: features must be a list");
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(features_list);
    float* features = (float*)nimcp_malloc(num_features * sizeof(float));
    if (!features) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_features * sizeof(float),
                          "Brain_learn: Failed to allocate features array");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate features array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "features is NULL");

        return NULL;
    }

    for (Py_ssize_t i = 0; i < num_features; i++) {
        PyObject* item = PyList_GetItem(features_list, i);
        features[i] = (float)PyFloat_AsDouble(item);
    }

    nimcp_status_t status = nimcp_brain_learn_example(self->brain, features,
                                                       (uint32_t)num_features, label, confidence);
    nimcp_free(features);

    if (status != NIMCP_OK) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "python_binding_simple",
                         "Brain_learn: Failed to learn example with label '%s'", label);
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* Brain_predict(BrainObject* self, PyObject* args) {
    PyObject* features_list;

    if (!PyArg_ParseTuple(args, "O", &features_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict: Invalid arguments");
        return NULL;
    }

    if (!PyList_Check(features_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict: features must be a list");
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(features_list);
    float* features = (float*)nimcp_malloc(num_features * sizeof(float));
    if (!features) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_features * sizeof(float),
                          "Brain_predict: Failed to allocate features array");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate features array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "features is NULL");

        return NULL;
    }

    for (Py_ssize_t i = 0; i < num_features; i++) {
        PyObject* item = PyList_GetItem(features_list, i);
        features[i] = (float)PyFloat_AsDouble(item);
    }

    char label[NIMCP_ID_BUFFER_SIZE];
    float confidence;

    nimcp_status_t status = nimcp_brain_predict(self->brain, features,
                                                (uint32_t)num_features, label, &confidence);
    nimcp_free(features);

    if (status != NIMCP_OK) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "python_binding_simple",
                         "Brain_predict: Prediction failed");
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    return Py_BuildValue("(sf)", label, confidence);
}

static PyObject* Brain_save(BrainObject* self, PyObject* args) {
    const char* filepath;

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_save: Invalid arguments");
        return NULL;
    }

    nimcp_status_t status = nimcp_brain_save(self->brain, filepath);

    if (status != NIMCP_OK) {
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_WRITE, filepath,
                      "Brain_save: Failed to save brain to '%s'", filepath);
        PyErr_SetString(PyExc_IOError, nimcp_get_error());
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* Brain_load(PyTypeObject* type, PyObject* args) {
    const char* filepath;

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_load: Invalid arguments");
        return NULL;
    }

    nimcp_brain_t brain = nimcp_brain_load(filepath);

    if (!brain) {
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_READ, filepath,
                      "Brain_load: Failed to load brain from '%s'", filepath);
        PyErr_SetString(PyExc_IOError, nimcp_get_error());
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");


        return NULL;
    }

    BrainObject* self = (BrainObject*)type->tp_alloc(type, 0);
    if (!self) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(BrainObject),
                          "Brain_load: Failed to allocate BrainObject");
        nimcp_brain_destroy(brain);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate Brain object");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self is NULL");

        return NULL;
    }
    self->brain = brain;

    return (PyObject*)self;
}

static PyMethodDef Brain_methods[] = {
    {"learn", (PyCFunction)Brain_learn, METH_VARARGS, "Learn from example"},
    {"predict", (PyCFunction)Brain_predict, METH_VARARGS, "Make prediction"},
    {"save", (PyCFunction)Brain_save, METH_VARARGS, "Save brain to file"},
    {"load", (PyCFunction)Brain_load, METH_VARARGS | METH_CLASS, "Load brain from file"},
    {NULL}
};

static PyTypeObject BrainType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.Brain",
    .tp_doc = "NIMCP Brain - High-level learning system",
    .tp_basicsize = sizeof(BrainObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Brain_new,
    .tp_init = (initproc)Brain_init,
    .tp_dealloc = (destructor)Brain_dealloc,
    .tp_methods = Brain_methods,
};

//=============================================================================
// Network Object Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    nimcp_network_t network;
} NetworkObject;

static void Network_dealloc(NetworkObject* self) {
    if (self->network) {
        nimcp_network_destroy(self->network);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Network_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    NetworkObject* self;
    self = (NetworkObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->network = NULL;
    }
    return (PyObject*)self;
}

static int Network_init(NetworkObject* self, PyObject* args, PyObject* kwds) {
    unsigned int num_inputs, num_outputs, num_hidden = 100;
    float learning_rate = NIMCP_LEARNING_RATE_DEFAULT;

    static char* kwlist[] = {"num_inputs", "num_outputs", "num_hidden", "learning_rate", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "II|If", kwlist,
                                     &num_inputs, &num_outputs, &num_hidden, &learning_rate)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Network_init: Invalid arguments");
        return -1;
    }

    self->network = nimcp_network_create(num_inputs, num_outputs, num_hidden, learning_rate);

    if (!self->network) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NOT_INITIALIZED, 0, "python_binding_simple",
                         "Network_init: Failed to create network (%u inputs, %u outputs)",
                         num_inputs, num_outputs);
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return -1;
    }

    return 0;
}

static PyObject* Network_forward(NetworkObject* self, PyObject* args) {
    PyObject* inputs_list;

    if (!PyArg_ParseTuple(args, "O", &inputs_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Network_forward: Invalid arguments");
        return NULL;
    }

    if (!PyList_Check(inputs_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Network_forward: inputs must be a list");
        PyErr_SetString(PyExc_TypeError, "inputs must be a list");
        return NULL;
    }

    Py_ssize_t num_inputs = PyList_Size(inputs_list);
    float* inputs = (float*)nimcp_malloc(num_inputs * sizeof(float));
    if (!inputs) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_inputs * sizeof(float),
                          "Network_forward: Failed to allocate inputs array");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate inputs array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inputs is NULL");

        return NULL;
    }

    for (Py_ssize_t i = 0; i < num_inputs; i++) {
        PyObject* item = PyList_GetItem(inputs_list, i);
        inputs[i] = (float)PyFloat_AsDouble(item);
    }

    // Assume outputs same size as inputs for now
    float* outputs = (float*)nimcp_malloc(num_inputs * sizeof(float));
    if (!outputs) {
        nimcp_free(inputs);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_inputs * sizeof(float),
                          "Network_forward: Failed to allocate outputs array");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate outputs array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "outputs is NULL");

        return NULL;
    }

    nimcp_status_t status = nimcp_network_forward(self->network, inputs,
                                                  (uint32_t)num_inputs, outputs, (uint32_t)num_inputs);
    nimcp_free(inputs);

    if (status != NIMCP_OK) {
        nimcp_free(outputs);
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "python_binding_simple",
                         "Network_forward: Forward pass failed");
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    PyObject* result = PyList_New(num_inputs);
    if (!result) {
        nimcp_free(outputs);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                          "Network_forward: Failed to create result list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }
    for (Py_ssize_t i = 0; i < num_inputs; i++) {
        PyList_SetItem(result, i, PyFloat_FromDouble(outputs[i]));
    }

    nimcp_free(outputs);
    return result;
}

static PyMethodDef Network_methods[] = {
    {"forward", (PyCFunction)Network_forward, METH_VARARGS, "Forward pass through network"},
    {NULL}
};

static PyTypeObject NetworkType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.Network",
    .tp_doc = "NIMCP Neural Network - Low-level control",
    .tp_basicsize = sizeof(NetworkObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Network_new,
    .tp_init = (initproc)Network_init,
    .tp_dealloc = (destructor)Network_dealloc,
    .tp_methods = Network_methods,
};

//=============================================================================
// Module Definition
//=============================================================================

static PyObject* nimcp_version(PyObject* self, PyObject* args) {
    return Py_BuildValue("s", nimcp_version());
}

static PyMethodDef module_methods[] = {
    {"version", nimcp_version, METH_NOARGS, "Get NIMCP version"},
    {NULL}
};

static struct PyModuleDef nimcp_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "nimcp",
    .m_doc = "NIMCP - Neural Interface Message Communication Protocol",
    .m_size = -1,
    .m_methods = module_methods
};

PyMODINIT_FUNC PyInit_nimcp(void) {
    PyObject* m;

    LOG_MODULE_INFO("bindings.python.simple", "Initializing simple Python bindings for NIMCP");

    // Initialize NIMCP library
    nimcp_init();

    if (PyType_Ready(&BrainType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }

    if (PyType_Ready(&NetworkType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }

    m = PyModule_Create(&nimcp_module);
    if (m == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Add constants for brain sizes
    PyModule_AddIntConstant(m, "BRAIN_TINY", NIMCP_BRAIN_TINY);
    PyModule_AddIntConstant(m, "BRAIN_SMALL", NIMCP_BRAIN_SMALL);
    PyModule_AddIntConstant(m, "BRAIN_MEDIUM", NIMCP_BRAIN_MEDIUM);
    PyModule_AddIntConstant(m, "BRAIN_LARGE", NIMCP_BRAIN_LARGE);

    // Add constants for task types
    PyModule_AddIntConstant(m, "TASK_CLASSIFICATION", NIMCP_TASK_CLASSIFICATION);
    PyModule_AddIntConstant(m, "TASK_REGRESSION", NIMCP_TASK_REGRESSION);
    PyModule_AddIntConstant(m, "TASK_PATTERN_MATCHING", NIMCP_TASK_PATTERN_MATCHING);
    PyModule_AddIntConstant(m, "TASK_SEQUENCE", NIMCP_TASK_SEQUENCE);
    PyModule_AddIntConstant(m, "TASK_ASSOCIATION", NIMCP_TASK_ASSOCIATION);

    // Add types to module
    Py_INCREF(&BrainType);
    if (PyModule_AddObject(m, "Brain", (PyObject*)&BrainType) < 0) {
        Py_DECREF(&BrainType);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }

    Py_INCREF(&NetworkType);
    if (PyModule_AddObject(m, "Network", (PyObject*)&NetworkType) < 0) {
        Py_DECREF(&NetworkType);
        Py_DECREF(&BrainType);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }

    return m;
}
