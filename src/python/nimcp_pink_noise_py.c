#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_py.c - Python Bindings for Pink Noise Generator
//=============================================================================
/**
 * @file nimcp_pink_noise_py.c
 * @brief Python interface for 1/f noise generation
 *
 * WHAT: Exposes pink noise generation to Python
 * WHY: Enable Python scripts to add biologically realistic noise to networks
 * HOW: Python Type object with methods wrapping C API
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 */

#include <Python.h>
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "plasticity/noise/nimcp_pink_noise.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_py)

//=============================================================================
// PinkNoiseGenerator Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    pink_noise_generator_t generator;
} PinkNoiseGeneratorObject;

static void PinkNoiseGenerator_dealloc(PinkNoiseGeneratorObject* self) {
    if (self->generator) {
        pink_noise_destroy(self->generator);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PinkNoiseGenerator_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    PinkNoiseGeneratorObject* self = (PinkNoiseGeneratorObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->generator = NULL;
    }
    return (PyObject*)self;
}

static int PinkNoiseGenerator_init(PinkNoiseGeneratorObject* self, PyObject* args, PyObject* kwds) {
    static char* kwlist[] = {"alpha", "amplitude", "min_frequency", "max_frequency",
                              "sample_rate", "method", "seed", NULL};

    float alpha = 1.0F;
    float amplitude = 0.05F;
    float min_frequency = 0.1F;
    float max_frequency = 100.0F;
    float sample_rate = 1000.0F;
    const char* method_str = "voss";
    unsigned int seed = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ffffffsI", kwlist,
                                      &alpha, &amplitude, &min_frequency,
                                      &max_frequency, &sample_rate,
                                      &method_str, &seed)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PinkNoiseGenerator_init: operation failed");
        return -1;
    }

    // Parse method
    pink_noise_method_t method = PINK_NOISE_VOSS;
    if (strcmp(method_str, "voss") == 0) {
        method = PINK_NOISE_VOSS;
    } else if (strcmp(method_str, "iir") == 0) {
        method = PINK_NOISE_IIR;
    } else if (strcmp(method_str, "fft") == 0) {
        method = PINK_NOISE_FFT;
    } else if (strcmp(method_str, "white") == 0) {
        method = PINK_NOISE_WHITE;
    } else {
        PyErr_SetString(PyExc_ValueError, "Invalid method. Use 'voss', 'iir', 'fft', or 'white'");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PinkNoiseGenerator_init: operation failed");
        return -1;
    }

    // Create configuration
    pink_noise_config_t config = {
        .alpha = alpha,
        .amplitude = amplitude,
        .min_frequency = min_frequency,
        .max_frequency = max_frequency,
        .sample_rate = sample_rate,
        .method = method,
        .seed = seed
    };

    // Create generator
    self->generator = pink_noise_create(&config);
    if (!self->generator) {
        const char* error = pink_noise_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Failed to create generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_init: self->generator is NULL");
        return -1;
    }

    return 0;
}

// PinkNoiseGenerator.generate(num_samples) -> list
static PyObject* PinkNoiseGenerator_generate(PinkNoiseGeneratorObject* self, PyObject* args) {
    int num_samples;

    if (!PyArg_ParseTuple(args, "i", &num_samples)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_generate: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (num_samples <= 0) {
        PyErr_SetString(PyExc_ValueError, "Number of samples must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_generate: validation failed");
        return NULL;
    }

    // Allocate buffer
    float* samples = (float*)nimcp_malloc(sizeof(float) * (size_t)num_samples);
    if (!samples) {
        return PyErr_NoMemory();
    }

    // Release GIL during potentially long generation
    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = pink_noise_generate(self->generator, samples, (uint32_t)num_samples);
    Py_END_ALLOW_THREADS

    if (!success) {
        nimcp_free(samples);
        const char* error = pink_noise_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Generation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_generate: success is NULL");
        return NULL;
    }

    // Convert to Python list
    PyObject* result = PyList_New(num_samples);
    if (!result) {
        nimcp_free(samples);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PinkNoiseGenerator_generate: result is NULL");
        return NULL;
    }

    for (int i = 0; i < num_samples; i++) {
        PyObject* item = PyFloat_FromDouble(samples[i]);
        if (!item) {
            Py_DECREF(result);
            nimcp_free(samples);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_generate: item is NULL");
            return NULL;
        }
        // PyList_SetItem steals reference on success
        if (PyList_SetItem(result, i, item) < 0) {
            // On failure, item is NOT stolen
            Py_DECREF(item);
            Py_DECREF(result);
            nimcp_free(samples);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_generate: validation failed");
            return NULL;
        }
    }

    nimcp_free(samples);
    return result;
}

// PinkNoiseGenerator.generate_sample() -> float
static PyObject* PinkNoiseGenerator_generate_sample(PinkNoiseGeneratorObject* self, PyObject* args) {
    float sample;
    bool success = pink_noise_generate_sample(self->generator, &sample);

    if (!success) {
        const char* error = pink_noise_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Generation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_generate_sample: success is NULL");
        return NULL;
    }

    return PyFloat_FromDouble(sample);
}

// PinkNoiseGenerator.reset(seed) -> None
static PyObject* PinkNoiseGenerator_reset(PinkNoiseGeneratorObject* self, PyObject* args) {
    unsigned int seed = 0;

    if (!PyArg_ParseTuple(args, "|I", &seed)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_reset: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = pink_noise_reset(self->generator, seed);

    if (!success) {
        const char* error = pink_noise_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Reset failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_reset: success is NULL");
        return NULL;
    }

    Py_RETURN_NONE;
}

// PinkNoiseGenerator.modulate(base_level) -> float
static PyObject* PinkNoiseGenerator_modulate(PinkNoiseGeneratorObject* self, PyObject* args) {
    float base_level;

    if (!PyArg_ParseTuple(args, "f", &base_level)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_modulate: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float output;
    bool success = pink_noise_modulate(self->generator, base_level, &output);

    if (!success) {
        const char* error = pink_noise_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Modulation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_modulate: success is NULL");
        return NULL;
    }

    return PyFloat_FromDouble(output);
}

// PinkNoiseGenerator.modulate_multiplicative(value, strength) -> float
static PyObject* PinkNoiseGenerator_modulate_multiplicative(PinkNoiseGeneratorObject* self, PyObject* args) {
    float value, strength;

    if (!PyArg_ParseTuple(args, "ff", &value, &strength)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_modulate_multiplicative: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float output;
    bool success = pink_noise_modulate_multiplicative(self->generator, value, strength, &output);

    if (!success) {
        const char* error = pink_noise_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Modulation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PinkNoiseGenerator_modulate_multiplicative: success is NULL");
        return NULL;
    }

    return PyFloat_FromDouble(output);
}

static PyMethodDef PinkNoiseGenerator_methods[] = {
    {"generate", (PyCFunction)PinkNoiseGenerator_generate, METH_VARARGS,
     "Generate batch of pink noise samples\n\n"
     "Args:\n"
     "    num_samples (int): Number of samples to generate\n\n"
     "Returns:\n"
     "    list: Pink noise samples\n"},

    {"generate_sample", (PyCFunction)PinkNoiseGenerator_generate_sample, METH_NOARGS,
     "Generate single pink noise sample\n\n"
     "Returns:\n"
     "    float: Pink noise sample\n"},

    {"reset", (PyCFunction)PinkNoiseGenerator_reset, METH_VARARGS,
     "Reset generator to initial state\n\n"
     "Args:\n"
     "    seed (int, optional): New random seed (0 = time-based)\n"},

    {"modulate", (PyCFunction)PinkNoiseGenerator_modulate, METH_VARARGS,
     "Apply additive pink noise modulation\n\n"
     "Args:\n"
     "    base_level (float): Base value to modulate\n\n"
     "Returns:\n"
     "    float: Modulated value\n"},

    {"modulate_multiplicative", (PyCFunction)PinkNoiseGenerator_modulate_multiplicative, METH_VARARGS,
     "Apply multiplicative pink noise modulation\n\n"
     "Args:\n"
     "    value (float): Value to modulate\n"
     "    strength (float): Modulation strength (0-1)\n\n"
     "Returns:\n"
     "    float: Modulated value\n"},

    {NULL}
};

static PyTypeObject PinkNoiseGeneratorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.PinkNoiseGenerator",
    .tp_doc = "Pink noise (1/f) generator for neuromodulation\n\n"
              "Examples:\n"
              "    >>> gen = PinkNoiseGenerator(alpha=1.0, amplitude=0.05)\n"
              "    >>> noise = gen.generate(1000)\n"
              "    >>> modulated_dopamine = gen.modulate(0.5)\n",
    .tp_basicsize = sizeof(PinkNoiseGeneratorObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PinkNoiseGenerator_new,
    .tp_init = (initproc)PinkNoiseGenerator_init,
    .tp_dealloc = (destructor)PinkNoiseGenerator_dealloc,
    .tp_methods = PinkNoiseGenerator_methods,
};

//=============================================================================
// Module-Level Functions
//=============================================================================

// compute_pink_noise_stats(samples, sample_rate) -> dict
static PyObject* py_compute_pink_noise_stats(PyObject* self, PyObject* args) {
    PyObject* sample_list;
    float sample_rate;

    if (!PyArg_ParseTuple(args, "Of", &sample_list, &sample_rate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_compute_pink_noise_stats: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Convert Python list to C array
    if (!PyList_Check(sample_list)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_compute_pink_noise_stats: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t num_samples = PyList_Size(sample_list);
    if (num_samples < 64) {
        PyErr_SetString(PyExc_ValueError, "Too few samples (minimum 64)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_compute_pink_noise_stats: validation failed");
        return NULL;
    }

    float* samples = (float*)nimcp_malloc(sizeof(float) * (size_t)num_samples);
    if (!samples) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_samples; i++) {
        PyObject* item = PyList_GetItem(sample_list, i);
        samples[i] = (float)PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            nimcp_free(samples);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_compute_pink_noise_stats: validation failed");
            return NULL;
        }
    }

    // Release GIL during potentially long stats computation
    pink_noise_stats_t stats;
    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = pink_noise_compute_stats(samples, (uint32_t)num_samples, sample_rate, &stats);
    Py_END_ALLOW_THREADS

    nimcp_free(samples);

    if (!success) {
        const char* error = pink_noise_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Stats computation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_compute_pink_noise_stats: success is NULL");
        return NULL;
    }

    // Build result dictionary
    PyObject* result = PyDict_New();
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    // Helper macro to add item to dict with error checking
    #define ADD_DICT_ITEM(key, value_expr) do { \
        PyObject* _val = (value_expr); \
        if (!_val) { \
            Py_DECREF(result); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_compute_pink_noise_stats: _val is NULL"); \
            return NULL; \
        } \
        if (PyDict_SetItemString(result, (key), _val) < 0) { \
            Py_DECREF(_val); \
            Py_DECREF(result); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_compute_pink_noise_stats: validation failed"); \
            return NULL; \
        } \
        Py_DECREF(_val); \
    } while (0)

    ADD_DICT_ITEM("measured_alpha", PyFloat_FromDouble(stats.measured_alpha));
    ADD_DICT_ITEM("measured_amplitude", PyFloat_FromDouble(stats.measured_amplitude));
    ADD_DICT_ITEM("spectral_fit_r2", PyFloat_FromDouble(stats.spectral_fit_r2));
    ADD_DICT_ITEM("mean", PyFloat_FromDouble(stats.mean));
    ADD_DICT_ITEM("std_dev", PyFloat_FromDouble(stats.std_dev));
    ADD_DICT_ITEM("min_value", PyFloat_FromDouble(stats.min_value));
    ADD_DICT_ITEM("max_value", PyFloat_FromDouble(stats.max_value));

    #undef ADD_DICT_ITEM

    return result;
}

static PyMethodDef pink_noise_methods[] = {
    {"compute_pink_noise_stats", py_compute_pink_noise_stats, METH_VARARGS,
     "Compute statistics for pink noise samples\n\n"
     "Args:\n"
     "    samples (list): Noise samples to analyze\n"
     "    sample_rate (float): Sampling rate (Hz)\n\n"
     "Returns:\n"
     "    dict: Statistics including measured_alpha, amplitude, etc.\n"},

    {NULL, NULL, 0, NULL}
};

//=============================================================================
// Module Initialization
//=============================================================================

int init_pink_noise_module(PyObject* module) {
    LOG_MODULE_DEBUG("bindings.python.pink_noise", "Initializing pink noise Python module");

    // Prepare type
    if (PyType_Ready(&PinkNoiseGeneratorType) < 0) {
        LOG_MODULE_ERROR("bindings.python.pink_noise", "Failed to initialize PinkNoiseGenerator type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_pink_noise_module: validation failed");
        return -1;
    }

    // Add type to module
    Py_INCREF(&PinkNoiseGeneratorType);
    if (PyModule_AddObject(module, "PinkNoiseGenerator", (PyObject*)&PinkNoiseGeneratorType) < 0) {
        Py_DECREF(&PinkNoiseGeneratorType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_pink_noise_module: validation failed");
        return -1;
    }

    // Add methods to module
    for (int i = 0; pink_noise_methods[i].ml_name != NULL; i++) {
        PyObject* func = PyCFunction_New(&pink_noise_methods[i], NULL);
        if (func == NULL) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_pink_noise_module: validation failed");
            return -1;
        }
        if (PyModule_AddObject(module, pink_noise_methods[i].ml_name, func) < 0) {
            Py_DECREF(func);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_pink_noise_module: validation failed");
            return -1;
        }
    }

    return 0;
}
