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

#include <Python.h>
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "core/brain/nimcp_brain.h"
#include <stddef.h>  /* for NULL */
#include "core/brain/nimcp_brain_internal.h"  // Internal header for brain_struct access
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantum_bindings)

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

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Ifif", kwlist,
                                     &steps, &mixing, &coin_type, &decoherence)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_enable_quantum_walks: operation failed");
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_enable_quantum_walks: self->brain is NULL");
        return NULL;
    }

    // Cast from opaque handle to internal struct (using proper internal header)
    struct brain_struct* brain_impl = (struct brain_struct*)self->brain;

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
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_disable_quantum_walks: self->brain is NULL");
        return NULL;
    }

    // Cast from opaque handle to internal struct (using proper internal header)
    struct brain_struct* brain_impl = (struct brain_struct*)self->brain;
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
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_quantum_walk_config: self->brain is NULL");
        return NULL;
    }

    // Cast from opaque handle to internal struct (using proper internal header)
    struct brain_struct* brain_impl = (struct brain_struct*)self->brain;

    // Create dictionary
    PyObject* dict = PyDict_New();
    if (!dict) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dict is NULL");

        return NULL;
    }

    // Helper macro to add item to dict with error checking
    #define ADD_DICT_ITEM(key, value_expr) do { \
        PyObject* _val = (value_expr); \
        if (!_val) { \
            Py_DECREF(dict); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_quantum_walk_config: _val is NULL"); \
            return NULL; \
        } \
        if (PyDict_SetItemString(dict, (key), _val) < 0) { \
            Py_DECREF(_val); \
            Py_DECREF(dict); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_quantum_walk_config: validation failed"); \
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
// Method Definitions to Add to Brain_methods
// ============================================================================

/**
 * Add these entries to Brain_methods array in nimcp_types.c:
 */

/*
static PyMethodDef Brain_quantum_methods[] = {
    {"enable_quantum_walks", (PyCFunction)Brain_enable_quantum_walks, METH_VARARGS | METH_KEYWORDS,
     "Enable quantum walk diffusion for neuromodulators (Phase C2.1)\n\n"
     "Quantum walks provide O(√N) speedup for neuromodulator propagation.\n"
     "Best for large networks (>500 neurons) where quantum advantage outweighs overhead.\n\n"
     "Args:\n"
     "    steps (int): Number of quantum evolution steps (default: 50)\n"
     "    mixing (float): Quantum-classical mixing ratio [0=pure quantum, 1=classical] (default: 0.2)\n"
     "    coin_type (int): Coin operator [0=Hadamard, 1=Grover, 2=Fourier] (default: 0)\n"
     "    decoherence (float): Decoherence rate [0=none, 1=instant classical] (default: 0.05)\n\n"
     "Returns:\n"
     "    None\n\n"
     "Example:\n"
     "    brain = nimcp.Brain('model', size=2, task=0, inputs=10, outputs=3)\n"
     "    brain.enable_quantum_walks(steps=100, mixing=0.1)  # 90% quantum, 10% classical"},

    {"disable_quantum_walks", (PyCFunction)Brain_disable_quantum_walks, METH_NOARGS,
     "Disable quantum walk diffusion, revert to classical\n\n"
     "Returns:\n"
     "    None\n\n"
     "Example:\n"
     "    brain.disable_quantum_walks()"},

    {"get_quantum_walk_config", (PyCFunction)Brain_get_quantum_walk_config, METH_NOARGS,
     "Get current quantum walk configuration\n\n"
     "Returns:\n"
     "    dict: Configuration with keys: enabled, steps, mixing, coin_type, decoherence\n\n"
     "Example:\n"
     "    config = brain.get_quantum_walk_config()\n"
     "    print(f\"Quantum walks enabled: {config['enabled']}\")"},

    // ... (add before the final {NULL, NULL, 0, NULL} entry)
};
*/
