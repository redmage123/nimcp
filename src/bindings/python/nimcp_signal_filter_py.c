#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_signal_filter_py.c - Python Bindings for Signal Filter Module
//=============================================================================
/**
 * @file nimcp_signal_filter_py.c
 * @brief Python interface for signal filtering and band-pass operations
 *
 * WHAT: Exposes signal filtering functions to Python for PAC detection
 * WHY: Enable Python scripts to perform band-pass filtering for oscillation analysis
 * HOW: Python Type objects with methods wrapping C API
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#include <Python.h>
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/* Opt into the NumPy 1.7+ API and silence the deprecated-API #warning
 * emitted by npy_1_7_deprecated_api.h when this define is missing. */
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include "utils/signal/nimcp_signal_filter.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for signal_filter_py module */
static nimcp_health_agent_t* g_signal_filter_py_health_agent = NULL;

/**
 * @brief Set health agent for signal_filter_py heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
static void signal_filter_py_set_health_agent(nimcp_health_agent_t* agent) {
    g_signal_filter_py_health_agent = agent;
}

/** @brief Send heartbeat from signal_filter_py module */
static inline void signal_filter_py_heartbeat(const char* operation, float progress) {
    if (g_signal_filter_py_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_signal_filter_py_health_agent, operation, progress);
    }
}


//=============================================================================
// Signal Filter Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    signal_filter_t* filter;
} SignalFilterObject;

static void SignalFilter_dealloc(SignalFilterObject* self) {
    if (self->filter) {
        signal_filter_destroy(self->filter);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* SignalFilter_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    SignalFilterObject* self = (SignalFilterObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->filter = NULL;
    }
    return (PyObject*)self;
}

static int SignalFilter_init(SignalFilterObject* self, PyObject* args, PyObject* kwds) {
    static char* kwlist[] = {"filter_type", "low_freq", "high_freq", "cutoff_freq",
                              "sample_rate", "order", "window", NULL};

    const char* filter_type_str = "bandpass";
    float low_freq = 4.0F;
    float high_freq = 8.0F;
    float cutoff_freq = 0.0F;
    float sample_rate = 1000.0F;
    int order = 64;
    const char* window_str = "hamming";

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sffffis", kwlist,
                                      &filter_type_str, &low_freq, &high_freq,
                                      &cutoff_freq, &sample_rate, &order, &window_str)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "SignalFilter_init: Invalid arguments");
        return -1;
    }

    // Create configuration
    signal_filter_config_t config;

    // Parse filter type
    if (strcmp(filter_type_str, "bandpass") == 0) {
        config = signal_filter_bandpass_config(low_freq, high_freq, sample_rate);
    } else if (strcmp(filter_type_str, "lowpass") == 0) {
        config = signal_filter_lowpass_config(cutoff_freq, sample_rate);
    } else if (strcmp(filter_type_str, "highpass") == 0) {
        config = signal_filter_highpass_config(cutoff_freq, sample_rate);
    } else {
        config = signal_filter_default_config();
        if (strcmp(filter_type_str, "bandstop") == 0) {
            config.type = FILTER_BANDSTOP;
            config.low_freq = low_freq;
            config.high_freq = high_freq;
        }
    }

    config.order = (uint32_t)order;

    // Parse window type
    if (strcmp(window_str, "hamming") == 0) {
        config.window = WINDOW_HAMMING;
    } else if (strcmp(window_str, "hann") == 0) {
        config.window = WINDOW_HANN;
    } else if (strcmp(window_str, "blackman") == 0) {
        config.window = WINDOW_BLACKMAN;
    } else if (strcmp(window_str, "rectangular") == 0) {
        config.window = WINDOW_RECTANGULAR;
    } else {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "SignalFilter_init: Invalid window type '%s'", window_str);
        PyErr_SetString(PyExc_ValueError, "Invalid window type");
        return -1;
    }

    // Validate and create filter
    if (!signal_filter_validate_config(&config)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "SignalFilter_init: Invalid filter configuration (type=%s, order=%d)",
                   filter_type_str, order);
        PyErr_SetString(PyExc_ValueError, "Invalid filter configuration");
        return -1;
    }

    self->filter = signal_filter_create(&config);
    if (!self->filter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                             "SignalFilter_init: Failed to create signal filter");
        PyErr_SetString(PyExc_RuntimeError, "Failed to create signal filter");
        return -1;
    }

    return 0;
}

//=============================================================================
// SignalFilter Methods
//=============================================================================

static PyObject* SignalFilter_apply(SignalFilterObject* self, PyObject* args) {
    PyArrayObject* input_array;

    if (!PyArg_ParseTuple(args, "O!", &PyArray_Type, &input_array)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "SignalFilter_apply: Invalid arguments");
        return NULL;
    }

    // Guard clause
    if (!self->filter) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "SignalFilter_apply: Filter not initialized");
        PyErr_SetString(PyExc_RuntimeError, "Filter not initialized");
        return NULL;
    }

    // Validate input array
    if (PyArray_NDIM(input_array) != 1) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "SignalFilter_apply: Input must be 1D array");
        PyErr_SetString(PyExc_ValueError, "Input must be 1D array");
        return NULL;
    }

    if (PyArray_TYPE(input_array) != NPY_FLOAT32 && PyArray_TYPE(input_array) != NPY_FLOAT64) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "SignalFilter_apply: Input must be float32 or float64 array");
        PyErr_SetString(PyExc_ValueError, "Input must be float32 or float64 array");
        return NULL;
    }

    npy_intp n = PyArray_DIM(input_array, 0);

    // Allocate output array
    npy_intp dims[1] = {n};
    PyArrayObject* output_array = (PyArrayObject*)PyArray_SimpleNew(1, dims, NPY_FLOAT32);
    if (!output_array) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n * sizeof(float),
                          "SignalFilter_apply: Failed to allocate output array");
        return NULL;
    }

    // Get data pointers
    float* input_data = (float*)PyArray_DATA(input_array);
    float* output_data = (float*)PyArray_DATA(output_array);

    // Apply filter
    if (!signal_filter_apply(self->filter, input_data, output_data, (uint32_t)n)) {
        Py_DECREF(output_array);
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED,
                   "SignalFilter_apply: Filter operation failed on %lld samples", (long long)n);
        PyErr_SetString(PyExc_RuntimeError, "Filter operation failed");
        return NULL;
    }

    return (PyObject*)output_array;
}

static PyObject* SignalFilter_reset(SignalFilterObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->filter) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "SignalFilter_reset: Filter not initialized");
        PyErr_SetString(PyExc_RuntimeError, "Filter not initialized");
        return NULL;
    }

    if (!signal_filter_reset(self->filter)) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "SignalFilter_reset: Failed to reset filter");
        PyErr_SetString(PyExc_RuntimeError, "Failed to reset filter");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* SignalFilter_get_delay(SignalFilterObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->filter) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "SignalFilter_get_delay: Filter not initialized");
        PyErr_SetString(PyExc_RuntimeError, "Filter not initialized");
        return NULL;
    }

    uint32_t delay = signal_filter_get_delay(self->filter);
    return PyLong_FromUnsignedLong(delay);
}

static PyObject* SignalFilter_get_response(SignalFilterObject* self, PyObject* args) {
    PyArrayObject* freq_array;

    if (!PyArg_ParseTuple(args, "O!", &PyArray_Type, &freq_array)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "SignalFilter_get_response: Invalid arguments");
        return NULL;
    }

    if (!self->filter) {
        NIMCP_THROW(NIMCP_ERROR_NOT_INITIALIZED, "SignalFilter_get_response: Filter not initialized");
        PyErr_SetString(PyExc_RuntimeError, "Filter not initialized");
        return NULL;
    }

    // Validate input
    if (PyArray_NDIM(freq_array) != 1) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "SignalFilter_get_response: Frequencies must be 1D array");
        PyErr_SetString(PyExc_ValueError, "Frequencies must be 1D array");
        return NULL;
    }

    npy_intp n = PyArray_DIM(freq_array, 0);

    // Allocate output
    npy_intp dims[1] = {n};
    PyArrayObject* response_array = (PyArrayObject*)PyArray_SimpleNew(1, dims, NPY_FLOAT32);
    if (!response_array) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, n * sizeof(float),
                          "SignalFilter_get_response: Failed to allocate response array");
        return NULL;
    }

    float* freqs = (float*)PyArray_DATA(freq_array);
    float* response = (float*)PyArray_DATA(response_array);

    if (!signal_filter_get_response(self->filter, freqs, response, (uint32_t)n)) {
        Py_DECREF(response_array);
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED,
                   "SignalFilter_get_response: Failed to compute frequency response");
        PyErr_SetString(PyExc_RuntimeError, "Failed to compute frequency response");
        return NULL;
    }

    return (PyObject*)response_array;
}

static PyMethodDef SignalFilter_methods[] = {
    {"apply", (PyCFunction)SignalFilter_apply, METH_VARARGS,
     "Apply filter to input signal and return filtered output"},
    {"reset", (PyCFunction)SignalFilter_reset, METH_NOARGS,
     "Reset filter state (clear circular buffer)"},
    {"get_delay", (PyCFunction)SignalFilter_get_delay, METH_NOARGS,
     "Get group delay in samples (order/2 for linear phase FIR)"},
    {"get_response", (PyCFunction)SignalFilter_get_response, METH_VARARGS,
     "Compute frequency response at specified frequencies"},
    {NULL}
};

static PyTypeObject SignalFilterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.SignalFilter",
    .tp_doc = "Signal filter for band-pass, lowpass, highpass, and bandstop filtering",
    .tp_basicsize = sizeof(SignalFilterObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = SignalFilter_new,
    .tp_init = (initproc)SignalFilter_init,
    .tp_dealloc = (destructor)SignalFilter_dealloc,
    .tp_methods = SignalFilter_methods,
};

//=============================================================================
// Module Initialization
//=============================================================================

/* NumPy's import_array() macro expands to `return NUMPY_IMPORT_ARRAY_RETVAL`
 * which is NULL (void*) on Python 3 / modern NumPy. It can't return from an
 * int-returning function, so we wrap it in a void*-returning helper that
 * returns a non-NULL sentinel on success. The caller treats NULL as failure. */
static void* _import_numpy_array_api(void) {
    import_array();  /* on failure: return NULL */
    return (void*)1; /* non-NULL sentinel on success */
}

int init_signal_filter_module(PyObject* module) {
    LOG_MODULE_DEBUG("bindings.python.signal_filter", "Initializing signal filter Python module");

    // Import NumPy array API via wrapper (see comment above)
    if (_import_numpy_array_api() == NULL) {
        LOG_MODULE_ERROR("bindings.python.signal_filter", "NumPy import_array failed");
        return -1;
    }

    // Initialize SignalFilter type
    if (PyType_Ready(&SignalFilterType) < 0) {
        LOG_MODULE_ERROR("bindings.python.signal_filter", "Failed to initialize SignalFilter type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_signal_filter_module: validation failed");
        return -1;
    }

    Py_INCREF(&SignalFilterType);
    if (PyModule_AddObject(module, "SignalFilter", (PyObject*)&SignalFilterType) < 0) {
        Py_DECREF(&SignalFilterType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_signal_filter_module: validation failed");
        return -1;
    }

    // Add filter type constants
    PyModule_AddIntConstant(module, "FILTER_LOWPASS", FILTER_LOWPASS);
    PyModule_AddIntConstant(module, "FILTER_HIGHPASS", FILTER_HIGHPASS);
    PyModule_AddIntConstant(module, "FILTER_BANDPASS", FILTER_BANDPASS);
    PyModule_AddIntConstant(module, "FILTER_BANDSTOP", FILTER_BANDSTOP);

    // Add window type constants
    PyModule_AddIntConstant(module, "WINDOW_RECTANGULAR", WINDOW_RECTANGULAR);
    PyModule_AddIntConstant(module, "WINDOW_HAMMING", WINDOW_HAMMING);
    PyModule_AddIntConstant(module, "WINDOW_HANN", WINDOW_HANN);
    PyModule_AddIntConstant(module, "WINDOW_BLACKMAN", WINDOW_BLACKMAN);

    return 0;
}
