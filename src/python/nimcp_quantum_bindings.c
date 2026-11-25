/**
 * @file nimcp_quantum_bindings.c
 * @brief Python bindings for quantum walk configuration (Phase C2.1)
 *
 * WHAT: Exposes quantum walk settings to Python API
 * WHY:  Allow Python users to enable/configure quantum neuromodulator diffusion
 * HOW:  Add methods to Brain type for quantum walk configuration
 *
 * @version Phase C2.1
 * @date 2025-11-12
 */

/*
 * BUILD NOTE: this is NOT a standalone translation unit. It is a fragment of
 * the Python extension (target nimcp_python, src/python/CMakeLists.txt) and
 * must be #included by src/bindings/python/nimcp_python.c AFTER the
 * BrainObject typedef, e.g.
 *
 *     #include "../../python/nimcp_quantum_bindings.c"
 *
 * and NIMCP_QUANTUM_BINDINGS_METHODS added inside Brain_methods[] (before the
 * {NULL, NULL, 0, NULL} sentinel). It relies on nimcp_python.c having
 * included api/nimcp_api_internal.h and core/brain/nimcp_brain_internal.h.
 *
 * RUNTIME NOTE: the quantum-walk fields are read when the spatial
 * neuromodulator system is created (nimcp_brain_init_neuromod.c). Changing
 * them after brain creation updates brain->config only; it takes effect on
 * the next brain created from / restored with that config.
 */
#ifndef NIMCP_QUANTUM_BINDINGS_FRAGMENT
#define NIMCP_QUANTUM_BINDINGS_FRAGMENT

#include <Python.h>
#include <math.h>
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain_internal.h"

/* Resolve the internal brain from the Python handle (nimcp_brain_t is the
 * public handle; the internal brain_struct is handle->internal_brain). */
static struct brain_struct* quantum_bindings_brain(BrainObject* self) {
    if (!self || !self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    return (struct brain_struct*)self->brain->internal_brain;
}

// ============================================================================
// Quantum Walk Configuration Methods
// ============================================================================

/**
 * @brief Enable quantum walk diffusion for neuromodulators
 *
 * Python signature: brain.enable_quantum_walks(steps=50, mixing=0.2, coin_type=0, decoherence=0.05)
 */
static PyObject* Brain_enable_quantum_walks(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"steps", "mixing", "coin_type", "decoherence", NULL};

    uint32_t steps = 50;
    float mixing = 0.2f;
    uint32_t coin_type = 0;
    float decoherence = 0.05f;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|IfIf", kwlist,
                                     &steps, &mixing, &coin_type, &decoherence)) {
        return NULL;
    }
    if (!isfinite(mixing) || mixing < 0.0f || mixing > 1.0f ||
        !isfinite(decoherence) || decoherence < 0.0f || decoherence > 1.0f ||
        coin_type > 2 || steps == 0) {
        PyErr_SetString(PyExc_ValueError,
                        "steps>0, mixing/decoherence in [0,1], coin_type in {0,1,2}");
        return NULL;
    }

    struct brain_struct* brain_impl = quantum_bindings_brain(self);
    if (!brain_impl) {
        return NULL;
    }

    // Configure quantum walks
    brain_impl->config.enable_quantum_walk_diffusion = true;
    brain_impl->config.quantum_walk_steps = steps;
    brain_impl->config.quantum_classical_mixing = mixing;
    brain_impl->config.quantum_coin_type = coin_type;
    brain_impl->config.quantum_decoherence_rate = decoherence;

    Py_RETURN_NONE;
}

/**
 * @brief Disable quantum walk diffusion
 *
 * Python signature: brain.disable_quantum_walks()
 */
static PyObject* Brain_disable_quantum_walks(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    struct brain_struct* brain_impl = quantum_bindings_brain(self);
    if (!brain_impl) {
        return NULL;
    }
    brain_impl->config.enable_quantum_walk_diffusion = false;

    Py_RETURN_NONE;
}

/**
 * @brief Get current quantum walk configuration
 *
 * Python signature: config = brain.get_quantum_walk_config()
 * Returns: dict with keys: enabled, steps, mixing, coin_type, decoherence
 */
static PyObject* Brain_get_quantum_walk_config(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    struct brain_struct* brain_impl = quantum_bindings_brain(self);
    if (!brain_impl) {
        return NULL;
    }

    PyObject* dict = PyDict_New();
    if (!dict) {
        return NULL;
    }

    // Helper macro to add item to dict with error checking
    #define ADD_DICT_ITEM(key, value_expr) do { \
        PyObject* _val = (value_expr); \
        if (!_val) { \
            Py_DECREF(dict); \
            return NULL; \
        } \
        if (PyDict_SetItemString(dict, (key), _val) < 0) { \
            Py_DECREF(_val); \
            Py_DECREF(dict); \
            return NULL; \
        } \
        Py_DECREF(_val); \
    } while (0)

    ADD_DICT_ITEM("enabled",
                  PyBool_FromLong(brain_impl->config.enable_quantum_walk_diffusion));
    ADD_DICT_ITEM("steps",
                  PyLong_FromUnsignedLong(brain_impl->config.quantum_walk_steps));
    ADD_DICT_ITEM("mixing",
                  PyFloat_FromDouble(brain_impl->config.quantum_classical_mixing));
    ADD_DICT_ITEM("coin_type",
                  PyLong_FromUnsignedLong(brain_impl->config.quantum_coin_type));
    ADD_DICT_ITEM("decoherence",
                  PyFloat_FromDouble(brain_impl->config.quantum_decoherence_rate));

    #undef ADD_DICT_ITEM

    return dict;
}

// ============================================================================
// Method table entries — splice into Brain_methods[] in nimcp_python.c
// ============================================================================

#define NIMCP_QUANTUM_BINDINGS_METHODS \
    {"enable_quantum_walks", (PyCFunction)(void(*)(void))Brain_enable_quantum_walks, \
     METH_VARARGS | METH_KEYWORDS, \
     "enable_quantum_walks(steps=50, mixing=0.2, coin_type=0, decoherence=0.05) -> None. " \
     "Sets brain->config quantum-walk neuromodulator diffusion (Phase C2.1); read when the " \
     "spatial neuromodulator system is created."}, \
    {"disable_quantum_walks", (PyCFunction)Brain_disable_quantum_walks, METH_NOARGS, \
     "disable_quantum_walks() -> None. Revert config to classical diffusion."}, \
    {"get_quantum_walk_config", (PyCFunction)Brain_get_quantum_walk_config, METH_NOARGS, \
     "get_quantum_walk_config() -> dict(enabled, steps, mixing, coin_type, decoherence)."},

#endif /* NIMCP_QUANTUM_BINDINGS_FRAGMENT */
