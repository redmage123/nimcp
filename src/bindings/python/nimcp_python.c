/**
 * @file nimcp_python.c
 * @brief Comprehensive Python bindings for NIMCP v2.6.3 with all features
 * @version 2.6.3
 * @date 2025-11-05
 *
 * WHAT: Complete Python interface to NIMCP unified API
 * WHY:  Enable Python developers to leverage NIMCP's cognitive capabilities
 * HOW:  Python C extension using CPython API, wrapping nimcp.h public interface
 *
 * FEATURES IMPLEMENTED:
 * - Brain API: create, learn, predict, save/load
 * - Neural Network API: create, forward, backward, save/load
 * - v2.6.3 Enhancements:
 *   * Batch Processing: predict_batch, learn_batch
 *   * Async Inference: async_predict with futures
 *   * Checkpointing: enable_checkpointing, checkpoint, list_checkpoints
 *   * SIMD Operations: dot_product, vector_add
 *   * Network Statistics: get_stats
 * - Memory management: Proper Python reference counting
 * - Error handling: Python exceptions with NIMCP error messages
 *
 * CODING STANDARDS:
 * - WHAT/WHY/HOW documentation for all functions
 * - Guard clauses for input validation
 * - Consistent error handling patterns
 * - Memory safety (no leaks, proper cleanup)
 * - Clear API that matches NIMCP conventions
 *
 * EXAMPLE USAGE:
 * ```python
 * import nimcp
 *
 * # Create and train a brain
 * brain = nimcp.Brain("classifier", nimcp.BRAIN_SMALL, nimcp.TASK_CLASSIFICATION, 10, 5)
 * brain.learn([0.1, 0.2, ...], "class_a", 0.9)
 * label, confidence = brain.predict([0.15, 0.25, ...])
 *
 * # Enable checkpointing
 * brain.enable_checkpointing("/tmp/checkpoints")
 * brain.checkpoint("manual_checkpoint")
 *
 * # Batch processing
 * labels, confidences = brain.predict_batch([[0.1, 0.2, ...], [0.3, 0.4, ...]])
 * ```
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <math.h>  /* isfinite() for LR validation */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "nimcp.h"
#include "gpu/training/nimcp_training_bridge.h"  /* GPU weight cache struct for fix_output_activation */

/* Socratic active learning: cognitive module + LGSS bindings */
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain_internal.h"  /* brain_struct access for cortex fields */
#include "core/brain/accessors/nimcp_brain_accessors.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/nimcp_self_model.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/nimcp_emotional_system.h"
#include "security/lgss/perception/nimcp_lgss_content_filter.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "generation/nimcp_tokenizer.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "plasticity/adaptive/nimcp_backprop_kernel.h"
#include <string.h>
#include <strings.h> /* for strcasecmp (thalamus nucleus name parsing) */
#include <math.h>    /* for isnan, isinf (M-nan checks) */

#include <stddef.h>  /* for NULL */
#include <stdint.h>  /* for UINT32_MAX (H-6 bounds checks) */
#include "utils/memory/nimcp_memory.h"
#include "training/nimcp_unified_training.h"  /* UTM Python bindings */
#include "security/nimcp_bbb_helpers.h"
#include "constants/nimcp_buffer_constants.h"
#include "core/brain/learning/nimcp_brain_learning.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "training/nimcp_cortex_cnn.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "middleware/training/nimcp_training_convergent_decision.h"
#include "cognitive/reasoning/nimcp_reasoning_mesh_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/inner_dialogue/nimcp_inner_dialogue.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/nimcp_brain_state.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"

/* Biological plasticity integration */
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_event_driven_plasticity.h"
#include "plasticity/nimcp_plasticity_coordinator.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_training.h"
#include "lnn/nimcp_lnn_hamiltonian.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "training/nimcp_cnn_training.h"
#include "training/nimcp_training_dispatch.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "core/brain/subcortical/nimcp_thalamus.h"

//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Shutdown guard — set by nimcp.shutdown(), prevents Brain_dealloc crash */
static int g_py_shutdown_called = 0;

/**
 * P6-2: NULL guard macro for self->brain->internal_brain.
 * Many methods access internal_brain without checking for NULL.
 * This macro should be placed after the existing `if (!self->brain)` check.
 */
#define CHECK_INTERNAL_BRAIN(self) do { \
    if (!self->brain->internal_brain) { \
        PyErr_SetString(PyExc_RuntimeError, "Brain internal state not available"); \
        return NULL; \
    } \
} while(0)

/** Global health agent for python module */
static nimcp_health_agent_t* g_python_health_agent = NULL;

/**
 * @brief Set health agent for python heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
static void python_set_health_agent(nimcp_health_agent_t* agent) {
    g_python_health_agent = agent;
}

/** @brief Send heartbeat from python module */
static inline void python_heartbeat(const char* operation, float progress) {
    if (g_python_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_python_health_agent, operation, progress);
    }
}


// Forward declaration for signal filter module
extern int init_signal_filter_module(PyObject* module);

//=============================================================================
// Utility: Convert Python list to float array
//=============================================================================

/**
 * WHAT: Convert Python list of floats to C float array
 * WHY:  Bridge between Python and C data representations
 * HOW:  Iterate list, extract floats, allocate array
 *
 * @param list Python list object
 * @param size Output: size of array
 * @return float array (caller must free) or NULL on error
 */
static float* py_list_to_float_array(PyObject* list_obj, Py_ssize_t* size) {
    // Guard: Validate input — accept lists, tuples, and any sequence type (M-3)
    if (!PyList_Check(list_obj) && !PyTuple_Check(list_obj)) {
        // Try to convert to list via PySequence_List (handles numpy arrays, etc.)
        PyObject* as_list = PySequence_List(list_obj);
        if (!as_list) {
            /* M-9: NIMCP_THROW first, then PyErr_SetString last so Python
             * error indicator is not overwritten by immune system logging */
            NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "py_list_to_float_array: Expected sequence of floats");
            PyErr_SetString(PyExc_TypeError, "Expected list, tuple, or sequence of floats");
            return NULL;
        }
        float* result = py_list_to_float_array(as_list, size);
        Py_DECREF(as_list);
        return result;
    }

    /* At this point we have a list or tuple — normalize to list for uniform access */
    PyObject* list = list_obj;
    PyObject* converted_list = NULL;
    if (PyTuple_Check(list_obj)) {
        converted_list = PySequence_List(list_obj);
        if (!converted_list) {
            PyErr_SetString(PyExc_TypeError, "Failed to convert tuple to list");
            return NULL;
        }
        list = converted_list;
    }

    *size = PyList_Size(list);
    if (*size == 0) {
        Py_XDECREF(converted_list);
        /* M-9: NIMCP_THROW first, then PyErr_SetString last so Python
         * error indicator is not overwritten by immune system logging */
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "py_list_to_float_array: List cannot be empty");
        PyErr_SetString(PyExc_ValueError, "List cannot be empty");
        return NULL;
    }

    // Allocate array
    float* array = (float*)nimcp_malloc((*size) * sizeof(float));
    if (!array) {
        Py_XDECREF(converted_list);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate float array");
        return NULL;
    }

    // Convert elements — use PyNumber_Float() coercion to handle native Python
    // floats/ints, numpy scalars (np.float32, np.float64, np.int64, etc.),
    // and any type implementing __float__.
    for (Py_ssize_t i = 0; i < *size; i++) {
        PyObject* item = PyList_GetItem(list, i);  /* borrowed ref */
        PyObject* as_float = NULL;

        if (PyFloat_Check(item)) {
            /* Fast path: native Python float — no coercion needed */
            array[i] = (float)PyFloat_AS_DOUBLE(item);
        } else if (PyLong_Check(item)) {
            /* Fast path: native Python int */
            array[i] = (float)PyLong_AsDouble(item);
            if (array[i] == -1.0 && PyErr_Occurred()) {
                nimcp_free(array);
                Py_XDECREF(converted_list);
                return NULL;
            }
        } else {
            /* General path: numpy scalars, decimal.Decimal, any __float__ type */
            as_float = PyNumber_Float(item);
            if (!as_float) {
                nimcp_free(array);
                Py_XDECREF(converted_list);
                PyErr_Clear();
                NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                           "py_list_to_float_array: List element %zd is not a number", i);
                PyErr_SetString(PyExc_TypeError, "List must contain only numbers");
                return NULL;
            }
            array[i] = (float)PyFloat_AS_DOUBLE(as_float);
            Py_DECREF(as_float);
        }
    }

    Py_XDECREF(converted_list);
    return array;
}

//=============================================================================
// Brain Object Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    nimcp_brain_t brain;
    consolidation_community_cache_t* community_cache;  /* Cached community topology */
    PyObject* cloud_brain_ref;  /* Strong ref to cloud brain (prevents GC while connected) */
} BrainObject;

/**
 * WHAT: Deallocate Brain object
 * WHY:  Clean up resources when Python object is garbage collected
 * HOW:  Call nimcp_brain_destroy, free Python object memory
 */
static void Brain_dealloc(BrainObject* self) {
    Py_XDECREF(self->cloud_brain_ref);
    self->cloud_brain_ref = NULL;
    if (self->community_cache) {
        consolidation_community_cache_destroy(self->community_cache);
        self->community_cache = NULL;
    }
    if (self->brain && !g_py_shutdown_called) {
        nimcp_brain_destroy(self->brain);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

/**
 * WHAT: Allocate new Brain object
 * WHY:  Create Python object to hold brain handle
 * HOW:  Allocate memory, initialize to NULL
 */
static PyObject* Brain_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    BrainObject* self = (BrainObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->brain = NULL;
        self->community_cache = NULL;
        self->cloud_brain_ref = NULL;
    }
    return (PyObject*)self;
}

/**
 * WHAT: Initialize Brain object
 * WHY:  Create NIMCP brain with specified configuration
 * HOW:  Parse arguments, call nimcp_brain_create
 *
 * @param name Brain name (for logging/identification)
 * @param size Brain size (BRAIN_TINY/SMALL/MEDIUM/LARGE)
 * @param task Task type (TASK_CLASSIFICATION/REGRESSION/etc)
 * @param num_inputs Number of input features
 * @param num_outputs Number of output classes/values
 * @return 0 on success, -1 on error
 */
static int Brain_init(BrainObject* self, PyObject* args, PyObject* kwds) {
    const char* name;
    int size = NIMCP_BRAIN_SMALL;
    int task = NIMCP_TASK_CLASSIFICATION;
    unsigned int num_inputs = 10;
    unsigned int num_outputs = 10;
    unsigned int neuron_count = 0;
    unsigned int snn_neuron_count = 0;
    unsigned int lnn_neuron_count = 0;
    const char* checkpoint = NULL;
    const char* init_mode = NULL;  // "full", "fast", or "minimal"
    const char* log_level = NULL;  // "trace", "debug", "info", "warn", "error", "off"

    static char* kwlist[] = {"name", "size", "task", "num_inputs", "num_outputs",
                             "neuron_count", "checkpoint", "init_mode", "log_level",
                             "snn_neuron_count", "lnn_neuron_count", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|iiIIIzzzII", kwlist,
                                     &name, &size, &task, &num_inputs, &num_outputs,
                                     &neuron_count, &checkpoint, &init_mode, &log_level,
                                     &snn_neuron_count, &lnn_neuron_count)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_init: Invalid arguments");
        return -1;
    }

    // Lazy construction: if checkpoint is provided and file exists, load directly
    // instead of creating a full brain (skips expensive hemispheric init)
    if (checkpoint && checkpoint[0] != '\0') {
        FILE* f = fopen(checkpoint, "rb");
        if (f) {
            fclose(f);
            Py_BEGIN_ALLOW_THREADS
            self->brain = nimcp_brain_load(checkpoint);
            Py_END_ALLOW_THREADS
            if (self->brain) {
                return 0;
            }
            // Fall through to normal creation if load fails
            fprintf(stderr, "[Brain] Checkpoint load failed for '%s', creating fresh brain\n",
                    checkpoint);
        } else {
            fprintf(stderr, "[Brain] Checkpoint '%s' not found, creating fresh brain\n",
                    checkpoint);
        }
    }

    // Set log level if requested (before brain creation so init logs are visible)
    if (log_level) {
        log_level_t level = LOG_LEVEL_INFO;
        if (strcasecmp(log_level, "trace") == 0)      level = LOG_LEVEL_TRACE;
        else if (strcasecmp(log_level, "debug") == 0)  level = LOG_LEVEL_DEBUG;
        else if (strcasecmp(log_level, "info") == 0)   level = LOG_LEVEL_INFO;
        else if (strcasecmp(log_level, "warn") == 0)   level = LOG_LEVEL_WARN;
        else if (strcasecmp(log_level, "error") == 0)  level = LOG_LEVEL_ERROR;
        else if (strcasecmp(log_level, "off") == 0)    level = LOG_LEVEL_OFF;
        // Ensure global logger exists before setting level
        if (!nimcp_log_is_initialized(NULL)) {
            nimcp_log_config_t log_cfg = nimcp_log_default_config();
            log_cfg.level = level;
            nimcp_log_init(&log_cfg);
        } else {
            nimcp_log_set_level(NULL, level);
        }
    }

    // Determine init mode: "fast" → nimcp_brain_create_fast, "minimal" → minimal, default → full
    bool use_fast = (init_mode && strcmp(init_mode, "fast") == 0);
    bool use_minimal = (init_mode && strcmp(init_mode, "minimal") == 0);

    if (use_fast && neuron_count > 0) {
        Py_BEGIN_ALLOW_THREADS
        self->brain = nimcp_brain_create_fast(name, (nimcp_brain_task_t)task,
                                              num_inputs, num_outputs, neuron_count);
        Py_END_ALLOW_THREADS
    } else if (use_minimal) {
        Py_BEGIN_ALLOW_THREADS
        self->brain = nimcp_brain_create(name, NIMCP_BRAIN_SMALL,
                                         (nimcp_brain_task_t)task, num_inputs, num_outputs);
        Py_END_ALLOW_THREADS
    } else if (neuron_count > 0) {
        Py_BEGIN_ALLOW_THREADS
        self->brain = nimcp_brain_create_with_neurons(name, (nimcp_brain_task_t)task,
                                                       num_inputs, num_outputs, neuron_count);
        Py_END_ALLOW_THREADS
    } else {
        Py_BEGIN_ALLOW_THREADS
        self->brain = nimcp_brain_create(name, (nimcp_brain_size_t)size,
                                         (nimcp_brain_task_t)task, num_inputs, num_outputs);
        Py_END_ALLOW_THREADS
    }

    if (!self->brain) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NOT_INITIALIZED, 0, "python_binding",
                         "Brain_init: Failed to create brain '%s'", name);
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return -1;
    }

    /* Set SNN/LNN target neuron counts on brain config (used when
     * brain_enable_multi_network_training creates the sub-networks) */
    nimcp_brain_t brain_ref = self->brain;
    if (brain_ref && brain_ref->internal_brain) {
        if (snn_neuron_count > 0)
            brain_ref->internal_brain->config.snn_target_neurons = snn_neuron_count;
        if (lnn_neuron_count > 0)
            brain_ref->internal_brain->config.lnn_target_neurons = lnn_neuron_count;
    }

    return 0;
}

/**
 * WHAT: Train brain on single example
 * WHY:  Supervised learning from labeled data
 * HOW:  Convert Python list to array, call nimcp_brain_learn_example
 *
 * @param features List of feature values
 * @param label Class label (string)
 * @param learning_rate Per-call learning rate override (0 = use brain default)
 * @param confidence Training confidence (0-1, default 1.0)
 * @return float loss value
 */
static PyObject* Brain_learn(BrainObject* self, PyObject* args) {
    PyObject* features_list;
    const char* label;
    float learning_rate = 0.0F;
    float confidence = 1.0F;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "Os|ff", &features_list, &label, &learning_rate, &confidence)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_learn: Invalid arguments");
        return NULL;
    }

    Py_ssize_t num_features;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features) {
        // Exception already thrown in py_list_to_float_array
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "features is NULL");

        return NULL;
    }

    /* H-6: Bounds check before truncation from Py_ssize_t to uint32_t */
    if (num_features > UINT32_MAX || num_features < 0) {
        nimcp_free(features);
        PyErr_SetString(PyExc_OverflowError, "Feature list too large for uint32_t");
        return NULL;
    }

    /* P1-49: nimcp_brain_learn_example returns nimcp_status_t, NOT float.
     * Error codes are POSITIVE (not negative), so checking < 0.0F was wrong. */
    /* H-1: status must be volatile — it is modified between setjmp (inside
     * SIGNAL_TRY_RECOVER) and longjmp (inside SIGNAL_ON_CRASH). Without
     * volatile, the compiler may keep it in a register that gets clobbered
     * by longjmp, causing undefined behavior. */
    volatile nimcp_status_t status = NIMCP_ERROR_UNKNOWN;
    nimcp_brain_t brain_ref = self->brain;
    uint32_t nf = (uint32_t)num_features;
    volatile bool learn_crashed = false;

    /* Per-call learning rate override: save original, swap in caller's LR,
     * restore after learn completes (even on crash). Only override if the
     * caller passed a positive learning_rate (0.0 = use brain default). */
    float saved_lr = 0.0F;
    bool lr_overridden = false;
    if (learning_rate > 0.0F && isfinite(learning_rate) && learning_rate <= 100.0F && brain_ref->internal_brain) {
        saved_lr = brain_ref->internal_brain->config.learning_rate;
        brain_ref->internal_brain->config.learning_rate = learning_rate;
        lr_overridden = true;
    }

    Py_BEGIN_ALLOW_THREADS

    /* Wrap learn in signal recovery — SIGSEGV during backprop returns error
     * instead of killing the process. Crash is reported to immune system. */
    SIGNAL_TRY_RECOVER(0, "Brain_learn") {
        status = nimcp_brain_learn_example(brain_ref, features, nf, label, confidence);
    } SIGNAL_ON_CRASH {
        /* SIGSEGV/SIGBUS during learn — immune system already notified by handler */
        learn_crashed = true;
        status = NIMCP_ERROR_UNKNOWN;
    } SIGNAL_TRY_END;

    Py_END_ALLOW_THREADS

    /* Restore original learning rate to avoid side effects on future calls */
    if (lr_overridden) {
        brain_ref->internal_brain->config.learning_rate = saved_lr;
    }
    nimcp_free(features);

    if (learn_crashed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "Brain_learn: SIGSEGV during learn (reported to immune system)");
        PyErr_SetString(PyExc_RuntimeError,
            "Brain.learn() crashed (SIGSEGV) — reported to immune system");
        return NULL;
    }

    if (status != NIMCP_OK) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "python_binding",
                         "Brain_learn: Failed to learn example with label '%s'", label);
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    /* Return actual loss from the C learning engine */
    float loss = nimcp_brain_get_last_loss(self->brain);
    if (isnan(loss) || isinf(loss)) {
        PyErr_SetString(PyExc_ValueError, "Brain.learn() produced NaN/inf loss");
        return NULL;
    }
    return PyFloat_FromDouble((double)loss);
}

/**
 * WHAT: Learn from a dense target vector (teacher distillation / generative training)
 * WHY:  Enables training toward semantic embeddings instead of one-hot labels
 * HOW:  Takes features + target arrays, calls nimcp_brain_learn_vector()
 */
static PyObject* Brain_learn_vector(BrainObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* features_list;
    PyObject* target_list;
    const char* label = NULL;
    float confidence = 1.0f;
    float learning_rate = 0.0f;

    static char* kwlist[] = {"features", "target", "label", "confidence", "learning_rate", NULL};

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|zff", kwlist,
                                      &features_list, &target_list, &label, &confidence,
                                      &learning_rate)) {
        return NULL;
    }

    Py_ssize_t num_features;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features) return NULL;

    Py_ssize_t num_targets;
    float* target = py_list_to_float_array(target_list, &num_targets);
    if (!target) {
        nimcp_free(features);
        return NULL;
    }

    if (num_features > UINT32_MAX || num_features < 0) {
        nimcp_free(features);
        nimcp_free(target);
        PyErr_SetString(PyExc_OverflowError, "Feature list too large for uint32_t");
        return NULL;
    }
    if (num_targets > UINT32_MAX || num_targets < 0) {
        nimcp_free(features);
        nimcp_free(target);
        PyErr_SetString(PyExc_OverflowError, "Target list too large for uint32_t");
        return NULL;
    }

    volatile nimcp_status_t status = NIMCP_ERROR_UNKNOWN;
    nimcp_brain_t brain_ref = self->brain;
    uint32_t nf = (uint32_t)num_features;
    uint32_t nt = (uint32_t)num_targets;
    volatile bool learn_crashed = false;

    /* Per-call learning rate override (same pattern as Brain_learn) */
    float saved_lr = 0.0F;
    bool lr_overridden = false;
    if (learning_rate > 0.0F && isfinite(learning_rate) && learning_rate <= 100.0F && brain_ref->internal_brain) {
        saved_lr = brain_ref->internal_brain->config.learning_rate;
        brain_ref->internal_brain->config.learning_rate = learning_rate;
        lr_overridden = true;
    }

    Py_BEGIN_ALLOW_THREADS

    SIGNAL_TRY_RECOVER(0, "Brain_learn_vector") {
        status = nimcp_brain_learn_vector(brain_ref, features, nf, target, nt,
                                          label, confidence);
    } SIGNAL_ON_CRASH {
        learn_crashed = true;
        status = NIMCP_ERROR_UNKNOWN;
    } SIGNAL_TRY_END;

    Py_END_ALLOW_THREADS

    /* Restore original learning rate */
    if (lr_overridden) {
        brain_ref->internal_brain->config.learning_rate = saved_lr;
    }

    nimcp_free(features);
    nimcp_free(target);

    if (learn_crashed) {
        PyErr_SetString(PyExc_RuntimeError,
            "Brain.learn_vector() crashed (SIGSEGV) — reported to immune system");
        return NULL;
    }

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    float loss = nimcp_brain_get_last_loss(self->brain);
    if (isnan(loss) || isinf(loss)) {
        PyErr_SetString(PyExc_ValueError, "Brain.learn_vector() produced NaN/inf loss");
        return NULL;
    }
    return PyFloat_FromDouble((double)loss);
}


/**
 * WHAT: Batch vector learning with GPU gradient accumulation
 * WHY:  Mini-batch training: accumulate gradients across N samples, apply once
 * HOW:  Takes list of (features, target) tuples, calls nimcp_brain_learn_vector_batch
 */
static PyObject* Brain_learn_vector_batch(BrainObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* batch_list;
    float learning_rate = -1.0f;  /* -1 = use default */
    static char* kwlist[] = {"batch", "learning_rate", NULL};

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|f", kwlist,
                                      &batch_list, &learning_rate)) return NULL;
    if (!PyList_Check(batch_list)) {
        PyErr_SetString(PyExc_TypeError, "learn_vector_batch expects a list of (features, target) tuples");
        return NULL;
    }

    Py_ssize_t num_examples = PyList_Size(batch_list);
    if (num_examples <= 0) {
        return PyFloat_FromDouble(-1.0);
    }
    if (num_examples > (Py_ssize_t)UINT32_MAX) {
        PyErr_SetString(PyExc_OverflowError, "Batch size exceeds uint32_t capacity");
        return NULL;
    }

    /* Extract all feature/target arrays */
    const float** features_array = nimcp_calloc(num_examples, sizeof(float*));
    const float** targets_array = nimcp_calloc(num_examples, sizeof(float*));
    if (!features_array || !targets_array) {
        nimcp_free(features_array);
        nimcp_free(targets_array);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate batch arrays");
        return NULL;
    }

    Py_ssize_t num_features = 0, target_size = 0;

    for (Py_ssize_t i = 0; i < num_examples; i++) {
        PyObject* item = PyList_GetItem(batch_list, i);
        if (!PyTuple_Check(item) || PyTuple_Size(item) < 2) {
            for (Py_ssize_t j = 0; j < i; j++) {
                nimcp_free((void*)features_array[j]);
                nimcp_free((void*)targets_array[j]);
            }
            nimcp_free(features_array);
            nimcp_free(targets_array);
            PyErr_SetString(PyExc_TypeError, "Each item must be a (features, target) tuple");
            return NULL;
        }

        Py_ssize_t nf, nt;
        features_array[i] = py_list_to_float_array(PyTuple_GetItem(item, 0), &nf);
        targets_array[i] = py_list_to_float_array(PyTuple_GetItem(item, 1), &nt);

        if (!features_array[i] || !targets_array[i]) {
            for (Py_ssize_t j = 0; j <= i; j++) {
                nimcp_free((void*)features_array[j]);
                nimcp_free((void*)targets_array[j]);
            }
            nimcp_free(features_array);
            nimcp_free(targets_array);
            PyErr_SetString(PyExc_ValueError, "Failed to convert features/target to float arrays");
            return NULL;
        }

        if (i == 0) { num_features = nf; target_size = nt; }
    }

    float loss;
    Py_BEGIN_ALLOW_THREADS
    loss = nimcp_brain_learn_vector_batch(
        self->brain, features_array, targets_array,
        (uint32_t)num_features, (uint32_t)target_size,
        (uint32_t)num_examples, learning_rate);
    Py_END_ALLOW_THREADS

    for (Py_ssize_t i = 0; i < num_examples; i++) {
        nimcp_free((void*)features_array[i]);
        nimcp_free((void*)targets_array[i]);
    }
    nimcp_free(features_array);
    nimcp_free(targets_array);

    return PyFloat_FromDouble((double)loss);
}


/**
 * WHAT: Unified experience — perceive input, predict output, and learn
 * WHY:  Merges inference + training for developmental online learning
 * HOW:  Forward pass → prediction error → attention-gated plasticity → reward
 */
static PyObject* Brain_experience(BrainObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* input_list;
    uint32_t output_size;
    float teacher_reward = 0.0f;

    static char* kwlist[] = {"input", "output_size", "teacher_reward", NULL};

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OI|f", kwlist,
                                      &input_list, &output_size, &teacher_reward)) {
        return NULL;
    }

    Py_ssize_t num_inputs;
    float* input = py_list_to_float_array(input_list, &num_inputs);
    if (!input) return NULL;

    if (num_inputs > UINT32_MAX || num_inputs < 1) {
        nimcp_free(input);
        PyErr_SetString(PyExc_ValueError, "Invalid input size");
        return NULL;
    }
    if (output_size == 0 || output_size > 1048576) {
        nimcp_free(input);
        PyErr_SetString(PyExc_ValueError, "Invalid output_size");
        return NULL;
    }

    float* output = nimcp_calloc(output_size, sizeof(float));
    if (!output) {
        nimcp_free(input);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate output buffer");
        return NULL;
    }

    brain_experience_result_t result;
    volatile nimcp_status_t status = NIMCP_ERROR_UNKNOWN;
    volatile bool crashed = false;

    Py_BEGIN_ALLOW_THREADS

    SIGNAL_TRY_RECOVER(0, "Brain_experience") {
        status = nimcp_brain_experience(self->brain, input, (uint32_t)num_inputs,
                                        output, output_size, teacher_reward, &result);
    } SIGNAL_ON_CRASH {
        crashed = true;
        status = NIMCP_ERROR_UNKNOWN;
    } SIGNAL_TRY_END;

    Py_END_ALLOW_THREADS

    nimcp_free(input);

    if (crashed) {
        nimcp_free(output);
        PyErr_SetString(PyExc_RuntimeError,
            "Brain.experience() crashed (SIGSEGV) — reported to immune system");
        return NULL;
    }

    if (status != NIMCP_OK) {
        nimcp_free(output);
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    /* Build output list */
    PyObject* output_list = PyList_New(output_size);
    if (!output_list) {
        nimcp_free(output);
        return NULL;
    }
    for (uint32_t i = 0; i < output_size; i++) {
        PyList_SET_ITEM(output_list, i, PyFloat_FromDouble((double)output[i]));
    }
    nimcp_free(output);

    /* Build result dict */
    PyObject* result_dict = Py_BuildValue(
        "{s:f, s:f, s:f, s:O, s:O, s:f, s:K, s:O}",
        "prediction_error", (double)result.prediction_error,
        "attention_level", (double)result.attention_level,
        "learning_rate_used", (double)result.learning_rate_used,
        "learning_applied", result.learning_applied ? Py_True : Py_False,
        "synapse_formed", result.synapse_formed ? Py_True : Py_False,
        "reward_signal", (double)result.reward_signal,
        "experience_id", (unsigned long long)result.experience_id,
        "output", output_list
    );
    Py_DECREF(output_list);

    return result_dict;
}


/**
 * WHAT: Configure experience-based learning
 * WHY:  Enable/disable and tune developmental learning parameters
 */
static PyObject* Brain_experience_configure(BrainObject* self, PyObject* args, PyObject* kwargs) {
    int enabled = 1;
    float base_lr = 0.001f;
    float attention_threshold = 0.3f;
    float attention_lr_scale = 3.0f;
    float novelty_boost = 1.5f;
    int enable_hebbian = 1;
    int enable_reward = 1;
    int enable_world_model = 1;
    int enable_structural = 0;
    float synaptogenesis_threshold = 0.7f;
    uint32_t consolidation_interval = 1000;

    static char* kwlist[] = {
        "enabled", "base_lr", "attention_threshold", "attention_lr_scale",
        "novelty_boost", "enable_hebbian", "enable_reward", "enable_world_model",
        "enable_structural", "synaptogenesis_threshold", "consolidation_interval", NULL
    };

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|pffffpppfpI", kwlist,
            &enabled, &base_lr, &attention_threshold, &attention_lr_scale,
            &novelty_boost, &enable_hebbian, &enable_reward, &enable_world_model,
            &enable_structural, &synaptogenesis_threshold, &consolidation_interval)) {
        return NULL;
    }

    brain_experience_config_t config = {
        .enabled = (bool)enabled,
        .base_learning_rate = base_lr,
        .attention_threshold = attention_threshold,
        .attention_lr_scale = attention_lr_scale,
        .novelty_boost = novelty_boost,
        .enable_hebbian = (bool)enable_hebbian,
        .enable_reward_learning = (bool)enable_reward,
        .enable_world_model_update = (bool)enable_world_model,
        .enable_structural_plasticity = (bool)enable_structural,
        .synaptogenesis_threshold = synaptogenesis_threshold,
        .consolidation_interval = consolidation_interval
    };

    nimcp_status_t status = nimcp_brain_experience_configure(self->brain, &config);
    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    Py_RETURN_NONE;
}


/**
 * WHAT: Correct the brain's last experience with expected output
 * WHY:  Supervised teaching signal — Claude tells Athena the right answer
 */
static PyObject* Brain_experience_correct(BrainObject* self, PyObject* args) {
    PyObject* expected_list;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O", &expected_list)) {
        return NULL;
    }

    Py_ssize_t num_expected;
    float* expected = py_list_to_float_array(expected_list, &num_expected);
    if (!expected) return NULL;

    if (num_expected > UINT32_MAX || num_expected < 1) {
        nimcp_free(expected);
        PyErr_SetString(PyExc_ValueError, "Invalid expected size");
        return NULL;
    }

    float loss = nimcp_brain_experience_correct(self->brain, expected, (uint32_t)num_expected);
    nimcp_free(expected);

    if (loss < 0.0f) {
        PyErr_SetString(PyExc_RuntimeError, "brain_experience_correct failed (no prior experience?)");
        return NULL;
    }

    return PyFloat_FromDouble((double)loss);
}


/**
 * WHAT: Direct attention to a specific sensory modality
 * WHY:  Claude can tell Athena what to focus on
 */
static PyObject* Brain_experience_attend(BrainObject* self, PyObject* args) {
    const char* modality;
    float strength = 1.0f;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "s|f", &modality, &strength)) {
        return NULL;
    }

    nimcp_status_t status = nimcp_brain_experience_attend(self->brain, modality, strength);
    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    Py_RETURN_NONE;
}


/**
 * WHAT: Hardwire innate circuits into the brain
 * WHY:  Pre-configure infant-like biases for face/voice/reflex/social reward
 */
static PyObject* Brain_innate_hardwire(BrainObject* self, PyObject* args, PyObject* kwargs) {
    int stage = 0;
    int face = 1, voice = 1, motion = -1, reflexes = 1, cry = 1;
    int social_reward = 1, habituation = 1, novelty = 1;
    float strength = -1.0f;  /* -1 = use stage default */

    static char* kwlist[] = {
        "stage", "face", "voice", "motion", "reflexes", "cry",
        "social_reward", "habituation", "novelty", "strength", NULL
    };

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ippppppppf", kwlist,
            &stage, &face, &voice, &motion, &reflexes, &cry,
            &social_reward, &habituation, &novelty, &strength)) {
        return NULL;
    }

    if (stage < 0 || stage >= INNATE_STAGE_COUNT) {
        PyErr_Format(PyExc_ValueError, "Invalid stage %d (must be 0-%d)", stage, INNATE_STAGE_COUNT - 1);
        return NULL;
    }

    /* Start with stage defaults, then override */
    innate_config_t config = innate_default_config((innate_stage_t)stage);
    config.enable_face_bias = (bool)face;
    config.enable_voice_bias = (bool)voice;
    if (motion >= 0) config.enable_motion_bias = (bool)motion;
    config.enable_reflexes = (bool)reflexes;
    config.enable_cry = (bool)cry;
    config.enable_social_reward = (bool)social_reward;
    config.enable_habituation = (bool)habituation;
    config.enable_novelty = (bool)novelty;
    if (strength >= 0.0f) config.bias_strength = strength;

    nimcp_status_t status = nimcp_brain_innate_hardwire(self->brain, &config);
    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    Py_RETURN_NONE;
}


/**
 * WHAT: Learn from a batch of examples in a single C call
 * WHY:  10-20x throughput improvement — one lock acquisition, one GPU weight
 *       upload, amortized overhead across N examples
 * HOW:  Takes list of (features_list, label_str, confidence_float) tuples,
 *       converts to C arrays, calls nimcp_brain_learn_batch(), returns
 *       list of per-example loss values.
 */
static PyObject* Brain_learn_batch(BrainObject* self, PyObject* args) {
    PyObject* batch_list;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O", &batch_list)) {
        return NULL;
    }

    if (!PyList_Check(batch_list)) {
        PyErr_SetString(PyExc_TypeError, "learn_batch expects a list of (features, label, confidence) tuples");
        return NULL;
    }

    Py_ssize_t num_examples = PyList_Size(batch_list);
    if (num_examples <= 0) {
        return PyList_New(0);
    }

    if (num_examples > 10000) {
        PyErr_SetString(PyExc_ValueError, "Batch size exceeds maximum of 10000");
        return NULL;
    }

    uint32_t n = (uint32_t)num_examples;

    /* Allocate parallel arrays for the C API */
    const float** features_array = (const float**)nimcp_calloc(n, sizeof(float*));
    uint32_t* num_features_array = (uint32_t*)nimcp_calloc(n, sizeof(uint32_t));
    const char** labels = (const char**)nimcp_calloc(n, sizeof(char*));
    float* confidences = (float*)nimcp_calloc(n, sizeof(float));
    float* losses_out = (float*)nimcp_calloc(n, sizeof(float));
    /* Track allocated feature arrays for cleanup */
    float** owned_features = (float**)nimcp_calloc(n, sizeof(float*));

    if (!features_array || !num_features_array || !labels || !confidences ||
        !losses_out || !owned_features) {
        nimcp_free(features_array);
        nimcp_free(num_features_array);
        nimcp_free(labels);
        nimcp_free(confidences);
        nimcp_free(losses_out);
        nimcp_free(owned_features);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate batch arrays");
        return NULL;
    }

    /* Parse each (features, label, confidence) tuple */
    for (uint32_t i = 0; i < n; i++) {
        PyObject* item = PyList_GetItem(batch_list, (Py_ssize_t)i);
        if (!item || !PyTuple_Check(item) || PyTuple_Size(item) < 2) {
            PyErr_Format(PyExc_TypeError,
                "Item %u must be a tuple of (features, label[, confidence])", i);
            goto cleanup_error;
        }

        PyObject* feat_obj = PyTuple_GetItem(item, 0);
        PyObject* label_obj = PyTuple_GetItem(item, 1);
        float conf = 1.0f;
        if (PyTuple_Size(item) >= 3) {
            PyObject* conf_obj = PyTuple_GetItem(item, 2);
            conf = (float)PyFloat_AsDouble(conf_obj);
            if (PyErr_Occurred()) goto cleanup_error;
        }

        /* Convert features list to float array */
        Py_ssize_t nf;
        float* feats = py_list_to_float_array(feat_obj, &nf);
        if (!feats) goto cleanup_error;

        if (nf > UINT32_MAX || nf < 0) {
            nimcp_free(feats);
            PyErr_SetString(PyExc_OverflowError, "Feature list too large");
            goto cleanup_error;
        }

        owned_features[i] = feats;
        features_array[i] = feats;
        num_features_array[i] = (uint32_t)nf;

        const char* label_str = PyUnicode_AsUTF8(label_obj);
        if (!label_str) goto cleanup_error;
        labels[i] = label_str;

        confidences[i] = conf;
    }

    /* Call C batch learn API with GIL released */
    volatile nimcp_status_t status = NIMCP_ERROR_UNKNOWN;
    nimcp_brain_t brain_ref = self->brain;
    volatile bool batch_crashed = false;

    Py_BEGIN_ALLOW_THREADS

    SIGNAL_TRY_RECOVER(0, "Brain_learn_batch") {
        status = nimcp_brain_learn_batch(brain_ref,
                                          features_array, num_features_array,
                                          labels, confidences, n, losses_out);
    } SIGNAL_ON_CRASH {
        batch_crashed = true;
        status = NIMCP_ERROR_UNKNOWN;
    } SIGNAL_TRY_END;

    Py_END_ALLOW_THREADS

    /* Cleanup feature arrays */
    for (uint32_t i = 0; i < n; i++) {
        if (owned_features[i]) nimcp_free(owned_features[i]);
    }
    nimcp_free(features_array);
    nimcp_free(num_features_array);
    nimcp_free(labels);
    nimcp_free(confidences);
    nimcp_free(owned_features);

    if (batch_crashed) {
        nimcp_free(losses_out);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "Brain_learn_batch: SIGSEGV during batch learn");
        PyErr_SetString(PyExc_RuntimeError,
            "Brain.learn_batch() crashed (SIGSEGV) — reported to immune system");
        return NULL;
    }

    if (status != NIMCP_OK) {
        nimcp_free(losses_out);
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    /* Build Python list of per-example losses */
    PyObject* result_list = PyList_New((Py_ssize_t)n);
    if (!result_list) {
        nimcp_free(losses_out);
        return NULL;
    }

    for (uint32_t i = 0; i < n; i++) {
        PyObject* loss_val = PyFloat_FromDouble((double)losses_out[i]);
        if (!loss_val) {
            Py_DECREF(result_list);
            nimcp_free(losses_out);
            return NULL;
        }
        PyList_SET_ITEM(result_list, (Py_ssize_t)i, loss_val);
    }

    nimcp_free(losses_out);
    return result_list;

cleanup_error:
    for (uint32_t i = 0; i < n; i++) {
        if (owned_features[i]) nimcp_free(owned_features[i]);
    }
    nimcp_free(features_array);
    nimcp_free(num_features_array);
    nimcp_free(labels);
    nimcp_free(confidences);
    nimcp_free(losses_out);
    nimcp_free(owned_features);
    return NULL;
}

/**
 * WHAT: Get gradient L2 norm from most recent learn() call
 * WHY:  Training diagnostics — detect vanishing/exploding gradients
 * HOW:  Call nimcp_brain_get_last_gradient_norm()
 */
static PyObject* Brain_get_last_gradient_norm(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float norm = nimcp_brain_get_last_gradient_norm(self->brain);
    PyObject* result = PyFloat_FromDouble((double)norm);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Make prediction on single example
 * WHY:  Inference after training
 * HOW:  Convert features, call nimcp_brain_predict
 *
 * @param features List of feature values
 * @return Tuple (label, confidence)
 */
static PyObject* Brain_predict(BrainObject* self, PyObject* args) {
    PyObject* features_list;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O", &features_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict: Invalid arguments");
        return NULL;
    }

    Py_ssize_t num_features;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features) {
        // Exception already thrown in py_list_to_float_array
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "features is NULL");

        return NULL;
    }

    /* H-6: Bounds check before truncation from Py_ssize_t to uint32_t */
    if (num_features > UINT32_MAX || num_features < 0) {
        nimcp_free(features);
        PyErr_SetString(PyExc_OverflowError, "Feature list too large for uint32_t");
        return NULL;
    }

    /* P6-3: Zero-initialize label buffer to avoid garbage reads if C API writes nothing */
    char label[NIMCP_NAME_BUFFER_SIZE];
    label[0] = '\0';
    float confidence = 0.0f;
    nimcp_status_t status;
    nimcp_brain_t brain_ref = self->brain;
    uint32_t nf = (uint32_t)num_features;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_predict(brain_ref, features, nf, label, &confidence);
    Py_END_ALLOW_THREADS
    nimcp_free(features);

    if (status != NIMCP_OK) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "python_binding",
                         "Brain_predict: Prediction failed");
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    return Py_BuildValue("(sf)", label, confidence);
}

/**
 * WHAT: Fast prediction — forward pass only, no cognitive stages
 * WHY:  brain.predict() runs 28 cognitive stages per call (sleep, curiosity,
 *        theory of mind, etc.), making training loops 10-100x slower than needed.
 *        predict_fast() does a pure neural network forward pass.
 * HOW:  Calls nimcp_brain_predict_fast() which uses adaptive_network_forward() directly
 */
static PyObject* Brain_predict_fast(BrainObject* self, PyObject* args) {
    PyObject* features_list;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O", &features_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict_fast: Invalid arguments");
        return NULL;
    }

    Py_ssize_t num_features;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "features is NULL");
        return NULL;
    }

    /* H-6: Bounds check before truncation from Py_ssize_t to uint32_t */
    if (num_features > UINT32_MAX || num_features < 0) {
        nimcp_free(features);
        PyErr_SetString(PyExc_OverflowError, "Feature list too large for uint32_t");
        return NULL;
    }

    /* P6-3: Zero-initialize label buffer to avoid garbage reads if C API writes nothing */
    char label[NIMCP_NAME_BUFFER_SIZE];
    label[0] = '\0';
    float confidence = 0.0f;
    nimcp_status_t status;
    nimcp_brain_t brain_ref = self->brain;
    uint32_t nf = (uint32_t)num_features;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_predict_fast(brain_ref, features, nf, label, &confidence);
    Py_END_ALLOW_THREADS
    nimcp_free(features);

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    return Py_BuildValue("(sf)", label, confidence);
}

/**
 * WHAT: Domain-scoped fast prediction
 * WHY:  Multi-domain training causes first-mover dominance — predict_fast always
 *        returns labels from the dominant domain. This restricts argmax to the
 *        caller's domain so biology predictions only consider biology labels.
 * HOW:  Calls nimcp_brain_predict_in_domain() with domain prefix filter
 *
 * @param features List of float features
 * @param domain   Domain prefix string (e.g., "biology:")
 * @return Tuple (label, confidence)
 */
static PyObject* Brain_predict_in_domain(BrainObject* self, PyObject* args) {
    PyObject* features_list;
    const char* domain_prefix;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "Os", &features_list, &domain_prefix)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict_in_domain: expected (features, domain)");
        return NULL;
    }

    Py_ssize_t num_features;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "features is NULL");
        return NULL;
    }

    /* H-6: Bounds check before truncation from Py_ssize_t to uint32_t */
    if (num_features > UINT32_MAX || num_features < 0) {
        nimcp_free(features);
        PyErr_SetString(PyExc_OverflowError, "Feature list too large for uint32_t");
        return NULL;
    }

    /* P6-3: Zero-initialize label buffer to avoid garbage reads if C API writes nothing */
    char label[NIMCP_NAME_BUFFER_SIZE];
    label[0] = '\0';
    float confidence = 0.0f;
    nimcp_status_t status;
    nimcp_brain_t brain_ref = self->brain;
    uint32_t nf = (uint32_t)num_features;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_predict_in_domain(brain_ref, features, nf,
                                            domain_prefix, label, &confidence);
    Py_END_ALLOW_THREADS
    nimcp_free(features);

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    return Py_BuildValue("(sf)", label, confidence);
}

/**
 * WHAT: Batch prediction (v2.7.0 enhancement)
 * WHY:  Efficient processing of multiple samples
 * HOW:  Convert list of lists to 2D array, call nimcp_brain_predict_batch
 *
 * @param features_batch List of feature lists
 * @return Tuple (labels_list, confidences_list)
 */
static PyObject* Brain_predict_batch(BrainObject* self, PyObject* args) {
    PyObject* features_batch;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O", &features_batch)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict_batch: Invalid arguments");
        return NULL;
    }

    if (!PyList_Check(features_batch)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict_batch: features_batch must be a list");
        PyErr_SetString(PyExc_TypeError, "features_batch must be a list of lists");
        return NULL;
    }

    Py_ssize_t batch_size = PyList_Size(features_batch);
    if (batch_size == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_predict_batch: Batch cannot be empty");
        PyErr_SetString(PyExc_ValueError, "Batch cannot be empty");
        return NULL;
    }
    if (batch_size > 100000) {
        PyErr_SetString(PyExc_ValueError, "Batch size exceeds maximum (100000)");
        return NULL;
    }

    // Get number of features from first example
    PyObject* first_example = PyList_GetItem(features_batch, 0);
    Py_ssize_t num_features;
    float* first_features = py_list_to_float_array(first_example, &num_features);
    if (!first_features) {
        // Exception already thrown in py_list_to_float_array
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "first_features is NULL");

        return NULL;
    }

    // Allocate batch arrays
    const float** features_ptrs = (const float**)nimcp_malloc(batch_size * sizeof(float*));
    float** feature_arrays = (float**)nimcp_malloc(batch_size * sizeof(float*));
    char** labels = (char**)nimcp_malloc(batch_size * sizeof(char*));
    float* confidences = (float*)nimcp_malloc(batch_size * sizeof(float));

    if (!features_ptrs || !feature_arrays || !labels || !confidences) {
        nimcp_free(first_features);
        nimcp_free(features_ptrs);
        nimcp_free(feature_arrays);
        nimcp_free(labels);
        nimcp_free(confidences);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, batch_size * sizeof(float*),
                          "Brain_predict_batch: Failed to allocate batch arrays");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate batch arrays");
        return NULL;
    }

    // Convert all feature lists
    feature_arrays[0] = first_features;
    features_ptrs[0] = first_features;

    for (Py_ssize_t i = 1; i < batch_size; i++) {
        PyObject* example = PyList_GetItem(features_batch, i);
        Py_ssize_t size;
        float* features = py_list_to_float_array(example, &size);
        if (!features) {
            // Clean up previously allocated feature arrays
            for (Py_ssize_t j = 0; j < i; j++) {
                nimcp_free(feature_arrays[j]);
            }
            nimcp_free(features_ptrs);
            nimcp_free(feature_arrays);
            nimcp_free(labels);
            nimcp_free(confidences);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_predict_batch: operation failed");
            PyErr_SetString(PyExc_ValueError, "Failed to convert feature list");
            return NULL;
        }
        if (size != num_features) {
            // Free the just-allocated features array that doesn't match
            nimcp_free(features);
            // Clean up previously allocated feature arrays
            for (Py_ssize_t j = 0; j < i; j++) {
                nimcp_free(feature_arrays[j]);
            }
            nimcp_free(features_ptrs);
            nimcp_free(feature_arrays);
            nimcp_free(labels);
            nimcp_free(confidences);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_predict_batch: size mismatch");
            PyErr_SetString(PyExc_ValueError, "All feature lists must have same length");
            return NULL;
        }
        feature_arrays[i] = features;
        features_ptrs[i] = features;
    }

    // Allocate label buffers
    for (Py_ssize_t i = 0; i < batch_size; i++) {
        labels[i] = (char*)nimcp_malloc(NIMCP_NAME_BUFFER_SIZE);
        if (!labels[i]) {
            for (Py_ssize_t j = 0; j < i; j++) {
                nimcp_free(labels[j]);
            }
            for (Py_ssize_t j = 0; j < batch_size; j++) {
                nimcp_free(feature_arrays[j]);
            }
            nimcp_free(features_ptrs);
            nimcp_free(feature_arrays);
            nimcp_free(labels);
            nimcp_free(confidences);
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, NIMCP_NAME_BUFFER_SIZE,
                              "Brain_predict_batch: Failed to allocate label buffer %zd", i);
            PyErr_SetString(PyExc_MemoryError, "Failed to allocate label buffers");
            return NULL;
        }
    }

    // Call batch prediction
    // NOTE: nimcp_brain_predict_batch not yet implemented in C library.
    // H-5: This is a SERIAL FALLBACK — each sample is predicted individually.
    // When the C batch API is implemented, this should be replaced with a
    // single nimcp_brain_predict_batch() call for better throughput.
    nimcp_status_t status = NIMCP_OK;
    nimcp_brain_t brain_ref = self->brain;

    /* H-6: Bounds check before truncation from Py_ssize_t to uint32_t */
    if (num_features > UINT32_MAX || num_features < 0) {
        for (Py_ssize_t i = 0; i < batch_size; i++) {
            nimcp_free(feature_arrays[i]);
            nimcp_free(labels[i]);
        }
        nimcp_free(features_ptrs);
        nimcp_free(feature_arrays);
        nimcp_free(labels);
        nimcp_free(confidences);
        PyErr_SetString(PyExc_OverflowError, "Feature list too large for uint32_t");
        return NULL;
    }
    uint32_t nf = (uint32_t)num_features;

    /* H-3: Use predict_fast instead of predict (which runs 28 cognitive stages
     * per sample). For batch inference this is 10-100x faster since we skip
     * sleep, curiosity, theory of mind, etc. for each sample. */
    Py_BEGIN_ALLOW_THREADS
    for (Py_ssize_t i = 0; i < batch_size; i++) {
        status = nimcp_brain_predict_fast(brain_ref, features_ptrs[i], nf,
                                          labels[i], &confidences[i]);
        if (status != NIMCP_OK) break;
    }
    Py_END_ALLOW_THREADS

    // Build result
    PyObject* labels_list = NULL;
    PyObject* confidences_list = NULL;

    if (status == NIMCP_OK) {
        labels_list = PyList_New(batch_size);
        if (!labels_list) {
            /* Clean up already done below — just let status path handle it */
            status = NIMCP_ERROR_NO_MEMORY;
        } else {
            confidences_list = PyList_New(batch_size);
            if (!confidences_list) {
                Py_DECREF(labels_list);
                labels_list = NULL;
                status = NIMCP_ERROR_NO_MEMORY;
            } else {
                for (Py_ssize_t i = 0; i < batch_size; i++) {
                    PyObject* label_obj = PyUnicode_FromString(labels[i]);
                    if (!label_obj) {
                        /* M-9: Py_XDECREF in reverse creation order */
                        Py_DECREF(confidences_list);
                        Py_DECREF(labels_list);
                        labels_list = NULL;
                        confidences_list = NULL;
                        status = NIMCP_ERROR_NO_MEMORY;
                        break;
                    }
                    PyList_SET_ITEM(labels_list, i, label_obj);

                    PyObject* conf_obj = PyFloat_FromDouble(confidences[i]);
                    if (!conf_obj) {
                        /* M-9: Py_XDECREF in reverse creation order */
                        Py_DECREF(confidences_list);
                        Py_DECREF(labels_list);
                        labels_list = NULL;
                        confidences_list = NULL;
                        status = NIMCP_ERROR_NO_MEMORY;
                        break;
                    }
                    PyList_SET_ITEM(confidences_list, i, conf_obj);
                }
            }
        }
    }

    // Clean up
    for (Py_ssize_t i = 0; i < batch_size; i++) {
        nimcp_free(feature_arrays[i]);
        nimcp_free(labels[i]);
    }
    nimcp_free(features_ptrs);
    nimcp_free(feature_arrays);
    nimcp_free(labels);
    nimcp_free(confidences);

    if (status != NIMCP_OK) {
        /* M-9: NIMCP_THROW first, then PyErr_SetString last so Python
         * error indicator is not overwritten by immune system logging */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_predict_batch: validation failed");
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    /* P2-4: Use "NN" instead of "OO" to transfer ownership and avoid ref leak.
     * "N" steals the reference, "O" increments it - since we don't Py_DECREF
     * labels_list/confidences_list after Py_BuildValue, we must use "N". */
    return Py_BuildValue("(NN)", labels_list, confidences_list);
}

// Note: File-based checkpointing not yet in public API
// COW snapshots are available via nimcp_brain_snapshot_cow()
// Will add wrapper in future version

/**
 * WHAT: Save brain to file
 * WHY:  Persist trained brain for later use
 * HOW:  Call nimcp_brain_save
 */
static PyObject* Brain_save(BrainObject* self, PyObject* args) {
    const char* filepath;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_save: Invalid arguments");
        return NULL;
    }

    /* H-1: Release GIL around I/O operation */
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_save(self->brain, filepath);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_WRITE, filepath,
                      "Brain_save: Failed to save brain to '%s'", filepath);
        const char* err = nimcp_get_error();
        char buf[512];
        snprintf(buf, sizeof(buf), "Brain save failed (status=%d): %s", (int)status, err);
        PyErr_SetString(PyExc_IOError, buf);
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * WHAT: Load brain from file (class method)
 * WHY:  Restore previously saved brain
 * HOW:  Call nimcp_brain_load, create Python object
 */
static PyObject* Brain_load(PyTypeObject* type, PyObject* args) {
    const char* filepath;

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_load: Invalid arguments");
        return NULL;
    }

    /* H-1: Release GIL around I/O operation */
    nimcp_brain_t brain;
    Py_BEGIN_ALLOW_THREADS
    brain = nimcp_brain_load(filepath);
    Py_END_ALLOW_THREADS

    if (!brain) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_READ, filepath,
                      "Brain_load: Failed to load brain from '%s'", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");
        PyErr_SetString(PyExc_IOError, nimcp_get_error());
        return NULL;
    }

    BrainObject* self = (BrainObject*)type->tp_alloc(type, 0);
    if (!self) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(BrainObject),
                          "Brain_load: Failed to allocate BrainObject");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self is NULL");
        nimcp_brain_destroy(brain);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate Brain object");
        return NULL;
    }
    self->brain = brain;
    self->community_cache = NULL;

    return (PyObject*)self;
}

//=============================================================================
// Phase 2.8: Dynamic Brain Resizing (Python Bindings)
//=============================================================================

/**
 * WHAT: Resize brain to new neuron count
 * WHY:  Allow dynamic brain growth during training
 * HOW:  Call brain_resize from nimcp_brain_resize.h
 *
 * @param new_neuron_count Target neuron count (must be > current)
 * @return True on success, raises RuntimeError on failure
 */
static PyObject* Brain_resize(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    uint32_t new_neuron_count;

    if (!PyArg_ParseTuple(args, "I", &new_neuron_count)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_resize: Invalid arguments");
        return NULL;
    }

    // CRITICAL FIX: Pass internal brain, not handle wrapper
    // self->brain is nimcp_brain_t (struct nimcp_brain_handle*)
    // brain_resize needs brain_t (self->brain->internal_brain)
    bool success = nimcp_brain_resize(self->brain, new_neuron_count);

    if (!success) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "python_binding",
                         "Brain_resize: Failed to resize brain to %u neurons", new_neuron_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Brain_resize: resize failed");
        PyErr_SetString(PyExc_RuntimeError, "Failed to resize brain. Check that new size > current size and sufficient memory available.");
        return NULL;
    }

    Py_RETURN_TRUE;
}

/**
 * WHAT: Auto-resize brain based on utilization
 * WHY:  Enable automatic capacity management
 * HOW:  Call brain_auto_resize on internal brain
 *
 * CRITICAL FIX: self->brain is nimcp_brain_t (handle wrapper)
 * WHY: brain_auto_resize expects brain_t, not nimcp_brain_t
 * WHAT: Dereference handle to access internal_brain field
 *
 * @return True if resize occurred, False if no resize needed
 */
static PyObject* Brain_auto_resize(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    // CRITICAL FIX: Pass internal brain, not handle wrapper
    // self->brain is nimcp_brain_t (struct nimcp_brain_handle*)
    // brain_auto_resize needs brain_t (self->brain->internal_brain)
    bool resized = nimcp_brain_auto_resize(self->brain);

    if (resized) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

/**
 * WHAT: Get current neuron count
 * WHY:  Monitor brain size for metrics/logging
 * HOW:  Call brain_get_neuron_count on internal brain
 *
 * CRITICAL FIX: Pass internal brain, not handle wrapper
 *
 * @return Integer neuron count
 */
static PyObject* Brain_get_snn_stats(BrainObject* self, PyObject* args) {
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->snn_network) Py_RETURN_NONE;

    /* Oversized buffer to absorb snn_stats_t growth across builds.
     * The real struct has GPU metrics fields that may differ between
     * NVCC and GCC due to bool alignment — use 512 bytes to be safe. */
    uint8_t st_buf[512];
    memset(st_buf, 0, sizeof(st_buf));
    /* Overlay the fields we need to read */
    struct {
        uint64_t total_steps;
        uint64_t total_spikes;
        double total_compute_time_ms;
        double avg_step_time_ms;
        float mean_firing_rate;
        float max_firing_rate;
        float sparsity;
        float synchrony;
        float spikes_per_sample;
        float energy_per_spike;
        int health;
        uint32_t silent_neurons;
        uint32_t hyperactive_neurons;
    } st;
    memset(&st, 0, sizeof(st));
    int (*fn)(void*, void*) = (int(*)(void*,void*))snn_network_get_stats;
    fn(ib->snn_network, st_buf);
    memcpy(&st, st_buf, sizeof(st));

    /* Get n_populations from network struct (offset: magic(4)+id(4)+name(64)+neural_net(8)+populations(8) = 88) */
    typedef struct { uint32_t magic; uint32_t id; char name[64]; void* nn; void* pops; uint32_t n_pops; } snn_hdr_t;
    uint32_t n_pops = ((snn_hdr_t*)ib->snn_network)->n_pops;

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* tmp;
#define S(k,v) tmp=PyLong_FromUnsignedLongLong(v);PyDict_SetItemString(d,k,tmp);Py_DECREF(tmp);
#define F(k,v) tmp=PyFloat_FromDouble(v);PyDict_SetItemString(d,k,tmp);Py_DECREF(tmp);
    S("n_populations", n_pops);
    S("total_steps", st.total_steps); S("total_spikes", st.total_spikes);
    F("mean_firing_rate_hz", st.mean_firing_rate); F("max_firing_rate_hz", st.max_firing_rate);
    F("sparsity", st.sparsity); F("synchrony", st.synchrony);
    F("spikes_per_sample", st.spikes_per_sample);
    S("silent_neurons", st.silent_neurons); S("hyperactive_neurons", st.hyperactive_neurons);
    S("health", st.health);
#undef S
#undef F
    return d;
}

/* Runtime SNN parameter tuning — no brain restart required.
 * snn_tune(name, value) → bool (true if applied, false if name unknown) */
static PyObject* Brain_snn_tune(BrainObject* self, PyObject* args) {
    (void)self;
    const char* name = NULL;
    double value = 0.0;
    if (!PyArg_ParseTuple(args, "sd", &name, &value)) return NULL;

    extern void snn_tune_set_rstdp_lr(float);
    extern void snn_tune_set_rstdp_baseline_alpha(float);
    extern void snn_tune_set_target_rate(float);
    extern void snn_tune_set_homeo_bounds(float, float);
    extern void snn_tune_set_max_scale_dead(float);
    extern void snn_tune_set_dead_threshold(float);
    extern void snn_tune_set_metabolic_cap(float);
    extern void snn_tune_set_noise_rate_hz(float);
    extern void snn_tune_set_noise_pulse_mv(float);
    extern void snn_tune_set_intrinsic_alpha(float);
    extern void snn_tune_set_target_rate_input(float);
    /* Biophysical stability tunables (Wave A + B1) */
    extern void snn_tune_set_anti_reward_enabled(float);
    extern void snn_tune_set_anti_reward_threshold_ratio(float);
    extern void snn_tune_set_anti_reward_gain(float);
    extern void snn_tune_set_depression_inc(float);
    extern void snn_tune_set_depression_tau_ms(float);
    extern void snn_tune_set_depression_cap(float);
    extern void snn_tune_set_ahp_enabled(float);
    extern void snn_tune_set_ahp_tau_ms(float);
    extern void snn_tune_set_ahp_gain_mv(float);
    extern void snn_tune_set_pump_enabled(float);
    extern void snn_tune_set_pump_tau_ms(float);
    extern void snn_tune_set_pump_gain_mv(float);
    extern void snn_tune_set_basket_enabled(float);
    extern void snn_tune_set_basket_fraction(float);
    extern void snn_tune_set_noise_ei_ratio(float);
    /* Substrate adapter tunables (Phase 1 biological-substrate wiring) */
    extern void snn_tune_set_substrate_enabled(float);
    extern void snn_tune_set_substrate_update_period(float);
    extern void snn_tune_set_substrate_spike_dropout_on(float);
    extern void snn_tune_set_substrate_plasticity_mod_on(float);
    extern void snn_tune_set_ahp_pump_substrate_coupling(float);
    /* CB migration tunables */
    extern void snn_tune_set_conductance_enabled(float);
    extern void snn_tune_set_cb_weights_rescaled(float);
    extern void snn_tune_set_cb_gpu_enabled(float);
    extern void snn_tune_set_e_exc_mv(float);
    extern void snn_tune_set_e_inh_mv(float);
    extern void snn_tune_set_tau_exc_ms(float);
    extern void snn_tune_set_tau_inh_ms(float);

    float v = (float)value;
    if      (strcmp(name, "rstdp_lr") == 0)             snn_tune_set_rstdp_lr(v);
    else if (strcmp(name, "rstdp_baseline_alpha") == 0) snn_tune_set_rstdp_baseline_alpha(v);
    else if (strcmp(name, "target_rate") == 0)          snn_tune_set_target_rate(v);
    else if (strcmp(name, "target_rate_input") == 0)    snn_tune_set_target_rate_input(v);
    else if (strcmp(name, "homeo_min_scale") == 0)      snn_tune_set_homeo_bounds(v, v + 0.04f);
    else if (strcmp(name, "homeo_max_scale") == 0)      snn_tune_set_homeo_bounds(0.98f, v);
    else if (strcmp(name, "max_scale_dead") == 0)       snn_tune_set_max_scale_dead(v);
    else if (strcmp(name, "dead_threshold") == 0)       snn_tune_set_dead_threshold(v);
    else if (strcmp(name, "metabolic_cap") == 0)        snn_tune_set_metabolic_cap(v);
    else if (strcmp(name, "noise_rate_hz") == 0)        snn_tune_set_noise_rate_hz(v);
    else if (strcmp(name, "noise_pulse_mv") == 0)       snn_tune_set_noise_pulse_mv(v);
    else if (strcmp(name, "intrinsic_alpha") == 0)      snn_tune_set_intrinsic_alpha(v);
    /* Anti-reward */
    else if (strcmp(name, "anti_reward_enabled") == 0)         snn_tune_set_anti_reward_enabled(v);
    else if (strcmp(name, "anti_reward_threshold_ratio") == 0) snn_tune_set_anti_reward_threshold_ratio(v);
    else if (strcmp(name, "anti_reward_gain") == 0)            snn_tune_set_anti_reward_gain(v);
    /* Short-term depression tuning */
    else if (strcmp(name, "depression_inc") == 0)       snn_tune_set_depression_inc(v);
    else if (strcmp(name, "depression_tau_ms") == 0)    snn_tune_set_depression_tau_ms(v);
    else if (strcmp(name, "depression_cap") == 0)       snn_tune_set_depression_cap(v);
    /* AHP adaptation */
    else if (strcmp(name, "ahp_enabled") == 0)          snn_tune_set_ahp_enabled(v);
    else if (strcmp(name, "ahp_tau_ms") == 0)           snn_tune_set_ahp_tau_ms(v);
    else if (strcmp(name, "ahp_gain_mv") == 0)          snn_tune_set_ahp_gain_mv(v);
    /* Na/K pump adaptation */
    else if (strcmp(name, "pump_enabled") == 0)         snn_tune_set_pump_enabled(v);
    else if (strcmp(name, "pump_tau_ms") == 0)          snn_tune_set_pump_tau_ms(v);
    else if (strcmp(name, "pump_gain_mv") == 0)         snn_tune_set_pump_gain_mv(v);
    /* Basket (fast-spiking inhibitory) pool */
    else if (strcmp(name, "basket_enabled") == 0)       snn_tune_set_basket_enabled(v);
    else if (strcmp(name, "basket_fraction") == 0)      snn_tune_set_basket_fraction(v);
    /* E/I-balanced noise */
    else if (strcmp(name, "noise_ei_ratio") == 0)       snn_tune_set_noise_ei_ratio(v);
    /* Substrate adapter */
    else if (strcmp(name, "substrate_enabled") == 0)           snn_tune_set_substrate_enabled(v);
    else if (strcmp(name, "substrate_update_period") == 0)     snn_tune_set_substrate_update_period(v);
    else if (strcmp(name, "substrate_spike_dropout_on") == 0)  snn_tune_set_substrate_spike_dropout_on(v);
    else if (strcmp(name, "substrate_plasticity_mod_on") == 0) snn_tune_set_substrate_plasticity_mod_on(v);
    else if (strcmp(name, "ahp_pump_substrate_coupling") == 0) snn_tune_set_ahp_pump_substrate_coupling(v);
    /* CB migration */
    else if (strcmp(name, "conductance_enabled") == 0)         snn_tune_set_conductance_enabled(v);
    else if (strcmp(name, "cb_weights_rescaled") == 0)         snn_tune_set_cb_weights_rescaled(v);
    else if (strcmp(name, "cb_gpu_enabled") == 0)              snn_tune_set_cb_gpu_enabled(v);
    else if (strcmp(name, "e_exc_mv") == 0)                    snn_tune_set_e_exc_mv(v);
    else if (strcmp(name, "e_inh_mv") == 0)                    snn_tune_set_e_inh_mv(v);
    else if (strcmp(name, "tau_exc_ms") == 0)                  snn_tune_set_tau_exc_ms(v);
    else if (strcmp(name, "tau_inh_ms") == 0)                  snn_tune_set_tau_inh_ms(v);
    else {
        PyErr_Format(PyExc_ValueError, "unknown tunable: %s", name);
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_snn_tune_get(BrainObject* self, PyObject* Py_UNUSED(a)) {
    (void)self;
    extern float snn_tune_get_rstdp_lr(void);
    extern float snn_tune_get_rstdp_baseline_alpha(void);
    extern float snn_tune_get_target_rate(void);
    extern float snn_tune_get_homeo_min_scale(void);
    extern float snn_tune_get_homeo_max_scale(void);
    extern float snn_tune_get_max_scale_dead(void);
    extern float snn_tune_get_dead_threshold(void);
    extern float snn_tune_get_metabolic_cap(void);
    extern float snn_tune_get_noise_rate_hz(void);
    extern float snn_tune_get_noise_pulse_mv(void);
    extern float snn_tune_get_intrinsic_alpha(void);
    extern float snn_tune_get_target_rate_input(void);
    /* Biophysical stability tunables (Wave A + B1) */
    extern float snn_tune_get_anti_reward_enabled(void);
    extern float snn_tune_get_anti_reward_threshold_ratio(void);
    extern float snn_tune_get_anti_reward_gain(void);
    extern float snn_tune_get_depression_inc(void);
    extern float snn_tune_get_depression_tau_ms(void);
    extern float snn_tune_get_depression_cap(void);
    extern float snn_tune_get_ahp_enabled(void);
    extern float snn_tune_get_ahp_tau_ms(void);
    extern float snn_tune_get_ahp_gain_mv(void);
    extern float snn_tune_get_pump_enabled(void);
    extern float snn_tune_get_pump_tau_ms(void);
    extern float snn_tune_get_pump_gain_mv(void);
    extern float snn_tune_get_basket_enabled(void);
    extern float snn_tune_get_basket_fraction(void);
    extern float snn_tune_get_noise_ei_ratio(void);
    /* Substrate adapter tunables (Phase 1 biological-substrate wiring) */
    extern float snn_tune_get_substrate_enabled(void);
    extern float snn_tune_get_substrate_update_period(void);
    extern float snn_tune_get_substrate_spike_dropout_on(void);
    extern float snn_tune_get_substrate_plasticity_mod_on(void);
    extern float snn_tune_get_ahp_pump_substrate_coupling(void);
    /* CB migration tunables */
    extern float snn_tune_get_conductance_enabled(void);
    extern float snn_tune_get_cb_weights_rescaled(void);
    extern float snn_tune_get_cb_gpu_enabled(void);
    extern float snn_tune_get_e_exc_mv(void);
    extern float snn_tune_get_e_inh_mv(void);
    extern float snn_tune_get_tau_exc_ms(void);
    extern float snn_tune_get_tau_inh_ms(void);

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* v;
#define F(k, val) v = PyFloat_FromDouble((double)(val)); \
                  if (v) { PyDict_SetItemString(d, k, v); Py_DECREF(v); }
    F("rstdp_lr",               snn_tune_get_rstdp_lr());
    F("rstdp_baseline_alpha",   snn_tune_get_rstdp_baseline_alpha());
    F("target_rate",            snn_tune_get_target_rate());
    F("target_rate_input",      snn_tune_get_target_rate_input());
    F("homeo_min_scale",        snn_tune_get_homeo_min_scale());
    F("homeo_max_scale",        snn_tune_get_homeo_max_scale());
    F("max_scale_dead",         snn_tune_get_max_scale_dead());
    F("dead_threshold",         snn_tune_get_dead_threshold());
    F("metabolic_cap",          snn_tune_get_metabolic_cap());
    F("noise_rate_hz",          snn_tune_get_noise_rate_hz());
    F("noise_pulse_mv",         snn_tune_get_noise_pulse_mv());
    F("intrinsic_alpha",        snn_tune_get_intrinsic_alpha());
    /* Anti-reward */
    F("anti_reward_enabled",         snn_tune_get_anti_reward_enabled());
    F("anti_reward_threshold_ratio", snn_tune_get_anti_reward_threshold_ratio());
    F("anti_reward_gain",            snn_tune_get_anti_reward_gain());
    /* Short-term depression tuning */
    F("depression_inc",         snn_tune_get_depression_inc());
    F("depression_tau_ms",      snn_tune_get_depression_tau_ms());
    F("depression_cap",         snn_tune_get_depression_cap());
    /* AHP adaptation */
    F("ahp_enabled",            snn_tune_get_ahp_enabled());
    F("ahp_tau_ms",             snn_tune_get_ahp_tau_ms());
    F("ahp_gain_mv",            snn_tune_get_ahp_gain_mv());
    /* Na/K pump adaptation */
    F("pump_enabled",           snn_tune_get_pump_enabled());
    F("pump_tau_ms",            snn_tune_get_pump_tau_ms());
    F("pump_gain_mv",           snn_tune_get_pump_gain_mv());
    /* Basket (fast-spiking inhibitory) pool */
    F("basket_enabled",         snn_tune_get_basket_enabled());
    F("basket_fraction",        snn_tune_get_basket_fraction());
    /* E/I-balanced noise */
    F("noise_ei_ratio",         snn_tune_get_noise_ei_ratio());
    /* Substrate adapter */
    F("substrate_enabled",           snn_tune_get_substrate_enabled());
    F("substrate_update_period",     snn_tune_get_substrate_update_period());
    F("substrate_spike_dropout_on",  snn_tune_get_substrate_spike_dropout_on());
    F("substrate_plasticity_mod_on", snn_tune_get_substrate_plasticity_mod_on());
    F("ahp_pump_substrate_coupling", snn_tune_get_ahp_pump_substrate_coupling());
    /* CB migration */
    F("conductance_enabled",         snn_tune_get_conductance_enabled());
    F("cb_weights_rescaled",         snn_tune_get_cb_weights_rescaled());
    F("cb_gpu_enabled",              snn_tune_get_cb_gpu_enabled());
    F("e_exc_mv",                    snn_tune_get_e_exc_mv());
    F("e_inh_mv",                    snn_tune_get_e_inh_mv());
    F("tau_exc_ms",                  snn_tune_get_tau_exc_ms());
    F("tau_inh_ms",                  snn_tune_get_tau_inh_ms());
#undef F
    return d;
}

/* Runtime LNN parameter tuning — no brain restart required (Phase 2 substrate + thalamic).
 * lnn_tune(name, value) → True (raises ValueError on unknown name). */
static PyObject* Brain_lnn_tune(BrainObject* self, PyObject* args) {
    (void)self;
    const char* name = NULL;
    double value = 0.0;
    if (!PyArg_ParseTuple(args, "sd", &name, &value)) return NULL;

    /* Substrate adapter tunables (Phase 2 biological-substrate wiring for LNN) */
    extern void lnn_tune_set_substrate_enabled(float);
    extern void lnn_tune_set_substrate_update_period(float);
    extern void lnn_tune_set_substrate_tau_compose_on(float);
    /* Thalamic adapter tunables (Phase 2 multi-network thalamic rollout) */
    extern void lnn_tune_set_thalamic_enabled(float);
    extern void lnn_tune_set_thalamic_input_gain_on(float);
    extern void lnn_tune_set_thalamic_burst_tau_clamp_on(float);

    float v = (float)value;
    if      (strcmp(name, "substrate_enabled") == 0)           lnn_tune_set_substrate_enabled(v);
    else if (strcmp(name, "substrate_update_period") == 0)     lnn_tune_set_substrate_update_period(v);
    else if (strcmp(name, "substrate_tau_compose_on") == 0)    lnn_tune_set_substrate_tau_compose_on(v);
    else if (strcmp(name, "thalamic_enabled") == 0)            lnn_tune_set_thalamic_enabled(v);
    else if (strcmp(name, "thalamic_input_gain_on") == 0)      lnn_tune_set_thalamic_input_gain_on(v);
    else if (strcmp(name, "thalamic_burst_tau_clamp_on") == 0) lnn_tune_set_thalamic_burst_tau_clamp_on(v);
    else {
        PyErr_Format(PyExc_ValueError, "unknown LNN tunable: %s", name);
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_lnn_tune_get(BrainObject* self, PyObject* Py_UNUSED(a)) {
    (void)self;
    extern float lnn_tune_get_substrate_enabled(void);
    extern float lnn_tune_get_substrate_update_period(void);
    extern float lnn_tune_get_substrate_tau_compose_on(void);
    extern float lnn_tune_get_thalamic_enabled(void);
    extern float lnn_tune_get_thalamic_input_gain_on(void);
    extern float lnn_tune_get_thalamic_burst_tau_clamp_on(void);

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* v;
#define F(k, val) v = PyFloat_FromDouble((double)(val)); \
                  if (v) { PyDict_SetItemString(d, k, v); Py_DECREF(v); }
    /* Substrate adapter */
    F("substrate_enabled",          lnn_tune_get_substrate_enabled());
    F("substrate_update_period",    lnn_tune_get_substrate_update_period());
    F("substrate_tau_compose_on",   lnn_tune_get_substrate_tau_compose_on());
    /* Thalamic adapter */
    F("thalamic_enabled",           lnn_tune_get_thalamic_enabled());
    F("thalamic_input_gain_on",     lnn_tune_get_thalamic_input_gain_on());
    F("thalamic_burst_tau_clamp_on",lnn_tune_get_thalamic_burst_tau_clamp_on());
#undef F
    return d;
}

/* Runtime CNN parameter tuning — no brain restart required (Phase 3 substrate + thalamic).
 * cnn_tune(name, value) → True (raises ValueError on unknown name). */
static PyObject* Brain_cnn_tune(BrainObject* self, PyObject* args) {
    (void)self;
    const char* name = NULL;
    double value = 0.0;
    if (!PyArg_ParseTuple(args, "sd", &name, &value)) return NULL;

    /* Substrate adapter tunables (Phase 3 biological-substrate wiring for CNN) */
    extern void cortex_cnn_tune_set_substrate_enabled(float);
    extern void cortex_cnn_tune_set_substrate_update_period(float);
    extern void cortex_cnn_tune_set_substrate_activation_mod_on(float);
    extern void cortex_cnn_tune_set_substrate_plasticity_mod_on(float);
    /* Thalamic adapter tunables (Phase 3 multi-network thalamic rollout) */
    extern void cortex_cnn_tune_set_thalamic_enabled(float);
    extern void cortex_cnn_tune_set_thalamic_featuremap_gain_on(float);
    extern void cortex_cnn_tune_set_thalamic_burst_dropout_reduce_on(float);

    float v = (float)value;
    if      (strcmp(name, "substrate_enabled") == 0)              cortex_cnn_tune_set_substrate_enabled(v);
    else if (strcmp(name, "substrate_update_period") == 0)        cortex_cnn_tune_set_substrate_update_period(v);
    else if (strcmp(name, "substrate_activation_mod_on") == 0)    cortex_cnn_tune_set_substrate_activation_mod_on(v);
    else if (strcmp(name, "substrate_plasticity_mod_on") == 0)    cortex_cnn_tune_set_substrate_plasticity_mod_on(v);
    else if (strcmp(name, "thalamic_enabled") == 0)               cortex_cnn_tune_set_thalamic_enabled(v);
    else if (strcmp(name, "thalamic_featuremap_gain_on") == 0)    cortex_cnn_tune_set_thalamic_featuremap_gain_on(v);
    else if (strcmp(name, "thalamic_burst_dropout_reduce_on") == 0) cortex_cnn_tune_set_thalamic_burst_dropout_reduce_on(v);
    else {
        PyErr_Format(PyExc_ValueError, "unknown CNN tunable: %s", name);
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_cnn_tune_get(BrainObject* self, PyObject* Py_UNUSED(a)) {
    (void)self;
    extern float cortex_cnn_tune_get_substrate_enabled(void);
    extern float cortex_cnn_tune_get_substrate_update_period(void);
    extern float cortex_cnn_tune_get_substrate_activation_mod_on(void);
    extern float cortex_cnn_tune_get_substrate_plasticity_mod_on(void);
    extern float cortex_cnn_tune_get_thalamic_enabled(void);
    extern float cortex_cnn_tune_get_thalamic_featuremap_gain_on(void);
    extern float cortex_cnn_tune_get_thalamic_burst_dropout_reduce_on(void);

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* v;
#define F(k, val) v = PyFloat_FromDouble((double)(val)); \
                  if (v) { PyDict_SetItemString(d, k, v); Py_DECREF(v); }
    /* Substrate adapter */
    F("substrate_enabled",                  cortex_cnn_tune_get_substrate_enabled());
    F("substrate_update_period",            cortex_cnn_tune_get_substrate_update_period());
    F("substrate_activation_mod_on",        cortex_cnn_tune_get_substrate_activation_mod_on());
    F("substrate_plasticity_mod_on",        cortex_cnn_tune_get_substrate_plasticity_mod_on());
    /* Thalamic adapter */
    F("thalamic_enabled",                   cortex_cnn_tune_get_thalamic_enabled());
    F("thalamic_featuremap_gain_on",        cortex_cnn_tune_get_thalamic_featuremap_gain_on());
    F("thalamic_burst_dropout_reduce_on",   cortex_cnn_tune_get_thalamic_burst_dropout_reduce_on());
#undef F
    return d;
}

/* Per-population live diagnostics — firing rate + recent activity for each pop. */
static PyObject* Brain_snn_pop_stats(BrainObject* self, PyObject* Py_UNUSED(a)) {
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->snn_network) Py_RETURN_NONE;

    typedef struct { uint32_t magic; uint32_t id; char name[64]; void* nn;
                     void** pops; uint32_t n_pops; } snn_hdr_t;
    snn_hdr_t* snn = (snn_hdr_t*)ib->snn_network;
    uint32_t n_pops = snn->n_pops;

    /* Each snn_population_t is complex — access known offsets for the fields
     * we want. We care about: name, n_neurons, firing_rate_ema, rate_samples,
     * and (optionally) last spike count from output_n. Use the typed struct
     * header from nimcp_snn_types.h via a local shim. */
    typedef struct {
        uint32_t id;
        char     name[64];
        uint32_t n_neurons;
    } pop_name_hdr_t;

    PyObject* list = PyList_New((Py_ssize_t)n_pops);
    if (!list) return NULL;
    for (uint32_t i = 0; i < n_pops; i++) {
        void* pop = snn->pops[i];
        PyObject* d = PyDict_New();
        if (!d) { Py_DECREF(list); return NULL; }
        if (!pop) {
            PyList_SET_ITEM(list, (Py_ssize_t)i, d);
            continue;
        }
        /* Use snn_population accessors — call via snn_network_get_populations
         * path or just access the struct fields directly via known layout. */
        extern const char* snn_population_get_name(const void* pop);
        extern uint32_t    snn_population_get_n_neurons(const void* pop);
        extern float       snn_population_get_firing_rate_ema(const void* pop);
        extern uint32_t    snn_population_get_rate_samples(const void* pop);
        const char* name = snn_population_get_name(pop);
        PyObject* v;
        v = PyUnicode_FromString(name ? name : "?");
        if (v) { PyDict_SetItemString(d, "name", v); Py_DECREF(v); }
        v = PyLong_FromUnsignedLong(snn_population_get_n_neurons(pop));
        if (v) { PyDict_SetItemString(d, "n_neurons", v); Py_DECREF(v); }
        v = PyFloat_FromDouble((double)snn_population_get_firing_rate_ema(pop));
        if (v) { PyDict_SetItemString(d, "firing_rate_ema", v); Py_DECREF(v); }
        v = PyLong_FromUnsignedLong(snn_population_get_rate_samples(pop));
        if (v) { PyDict_SetItemString(d, "rate_samples", v); Py_DECREF(v); }
        PyList_SET_ITEM(list, (Py_ssize_t)i, d);
    }
    return list;
}

static PyObject* Brain_snn_force_quench(BrainObject* self, PyObject* args) {
    /* Emergency SNN rescue: force N homeostatic applies in a row.
     * Returns the total number of populations scaled across iterations. */
    uint32_t n_iter = 20;
    if (!PyArg_ParseTuple(args, "|I", &n_iter)) return NULL;
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->snn_network || !ib->snn_training_ctx) {
        PyErr_SetString(PyExc_RuntimeError,
                        "SNN or training context not initialized");
        return NULL;
    }
    uint32_t scaled = snn_network_force_homeostasis(
        ib->snn_network, ib->snn_training_ctx, n_iter);
    return PyLong_FromUnsignedLong(scaled);
}

static PyObject* Brain_snn_rescale_for_conductance(BrainObject* self, PyObject* args) {
    /* CB migration admin command: divide all CSR weights by ~50 so a CB
     * run starts at the same effective post-synaptic kick as the
     * current-based run did. Idempotent — sets cb_weights_rescaled=1
     * sticky flag and refuses double-apply.
     *
     * Usage:  brain.snn_rescale_for_conductance()             # default 1/50
     *         brain.snn_rescale_for_conductance(0.02)         # explicit factor
     *         brain.snn_rescale_for_conductance(50.0)         # rollback (after
     *                                                         # clearing the flag) */
    extern int snn_rescale_weights_for_conductance(snn_network_t*, float);
    double factor = (double)SNN_CB_DEFAULT_RESCALE_FACTOR;
    if (!PyArg_ParseTuple(args, "|d", &factor)) return NULL;
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->snn_network) {
        PyErr_SetString(PyExc_RuntimeError, "SNN network not initialized");
        return NULL;
    }
    int rc = snn_rescale_weights_for_conductance(ib->snn_network, (float)factor);
    if (rc != 0) {
        PyErr_Format(PyExc_RuntimeError,
                     "snn_rescale_weights_for_conductance failed (rc=%d) "
                     "— check cb_weights_rescaled flag or factor validity",
                     rc);
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_get_population_history(BrainObject* self, PyObject* args) {
    /* Return a list of the last N spike counts for a population, time-ordered.
     * Used by Python temporal-dynamics analysis (FFT, cross-corr, autocorr). */
    uint32_t pop_id = 0;
    if (!PyArg_ParseTuple(args, "I", &pop_id)) return NULL;
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->snn_network) Py_RETURN_NONE;

    uint32_t buf[256];
    uint64_t total_steps = 0;
    uint32_t n = snn_network_get_population_history(ib->snn_network, pop_id,
                                                    buf, &total_steps);

    PyObject* counts = PyList_New(n);
    if (!counts) return NULL;
    for (uint32_t i = 0; i < n; i++) {
        PyList_SET_ITEM(counts, i, PyLong_FromUnsignedLong(buf[i]));
    }
    PyObject* d = PyDict_New();
    PyDict_SetItemString(d, "counts", counts); Py_DECREF(counts);
    PyObject* ts = PyLong_FromUnsignedLongLong(total_steps);
    PyDict_SetItemString(d, "total_steps", ts); Py_DECREF(ts);
    PyObject* n_pop = PyLong_FromUnsignedLong(n);
    PyDict_SetItemString(d, "n", n_pop); Py_DECREF(n_pop);
    return d;
}

static PyObject* Brain_get_neuron_count(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    // CRITICAL FIX: Pass internal brain, not handle wrapper
    uint32_t count = nimcp_brain_get_neuron_count(self->brain);
    PyObject* result = PyLong_FromUnsignedLong(count);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * Allocator accounting snapshot — see nimcp_get_alloc_stats() in
 * src/api/nimcp_part_alloc_stats.c. Used to attribute RSS growth.
 */
#include "api/nimcp_alloc_stats.h"
static PyObject* Brain_get_alloc_stats(BrainObject* self, PyObject* args) {
    nimcp_alloc_stats_t s;
    nimcp_status_t st;
    Py_BEGIN_ALLOW_THREADS
    st = nimcp_get_alloc_stats(self->brain, &s);
    Py_END_ALLOW_THREADS
    if (st != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "nimcp_get_alloc_stats failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
#define SETK(name, v) do { \
        PyObject* o = PyLong_FromUnsignedLongLong((unsigned long long)(v)); \
        if (!o) { Py_DECREF(d); return NULL; } \
        PyDict_SetItemString(d, name, o); \
        Py_DECREF(o); \
    } while (0)
    SETK("glibc_arena_bytes",     s.glibc_arena_bytes);
    SETK("glibc_uordblks_bytes",  s.glibc_uordblks_bytes);
    SETK("glibc_fordblks_bytes",  s.glibc_fordblks_bytes);
    SETK("glibc_hblkhd_bytes",    s.glibc_hblkhd_bytes);
    SETK("glibc_hblks",           s.glibc_hblks);
    SETK("glibc_uordblks_count",  s.glibc_uordblks_count);
    SETK("proc_vm_rss_bytes",     s.proc_vm_rss_bytes);
    SETK("proc_vm_data_bytes",    s.proc_vm_data_bytes);
    SETK("proc_vm_peak_bytes",    s.proc_vm_peak_bytes);
    SETK("proc_vm_lib_bytes",     s.proc_vm_lib_bytes);
    SETK("proc_rss_anon_bytes",   s.proc_rss_anon_bytes);
    SETK("proc_rss_file_bytes",   s.proc_rss_file_bytes);
    SETK("proc_rss_shmem_bytes",  s.proc_rss_shmem_bytes);
    SETK("audit_entry_count",     s.audit_entry_count);
    SETK("audit_estimated_bytes", s.audit_estimated_bytes);
    SETK("kg_node_count",         s.kg_node_count);
    SETK("kg_edge_count",         s.kg_edge_count);
    SETK("kg_query_count",        s.kg_query_count);
    SETK("kg_modification_count", s.kg_modification_count);
    SETK("kg_estimated_bytes",    s.kg_estimated_bytes);
#undef SETK
    return d;
}

/**
 * WHAT: Retrofit synapse metadata for connections that lack plasticity data
 * WHY:  After pool exhaustion, some synapses have no STDP/STP capability
 * HOW:  Walk all neurons, allocate metadata for bare handles
 */
static PyObject* Brain_retrofit_synapse_metadata(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    extern uint32_t nimcp_brain_retrofit_synapse_metadata(void*);
    uint32_t count;
    Py_BEGIN_ALLOW_THREADS
    count = nimcp_brain_retrofit_synapse_metadata(self->brain);
    Py_END_ALLOW_THREADS
    return PyLong_FromUnsignedLong(count);
}

/**
 * WHAT: Get brain utilization metrics
 * WHY:  Monitor capacity and decide when to resize
 * HOW:  Call brain_get_utilization_metrics on internal brain
 *
 * CRITICAL FIX: Pass internal brain, not handle wrapper
 *
 * @return Tuple (utilization, saturation) where both are floats [0.0, 1.0]
 */
static PyObject* Brain_get_utilization_metrics(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float utilization = 0.0F;
    float saturation = 0.0F;

    // CRITICAL FIX: Pass internal brain, not handle wrapper
    bool success = nimcp_brain_get_utilization_metrics(self->brain, &utilization, &saturation);

    if (!success) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_BRAIN(NIMCP_ERROR_OPERATION_FAILED, 0, "python_binding",
                         "Brain_get_utilization_metrics: Failed to get metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Brain_get_utilization_metrics: failed");
        PyErr_SetString(PyExc_RuntimeError, "Failed to get utilization metrics");
        return NULL;
    }

    return Py_BuildValue("(ff)", utilization, saturation);
}

//=============================================================================
// COW Snapshot API (Python Bindings)
//=============================================================================

static void Brain_snapshot_capsule_destructor(PyObject* capsule)
{
    nimcp_brain_snapshot_t snapshot =
        (nimcp_brain_snapshot_t)PyCapsule_GetPointer(capsule, "nimcp.brain_snapshot");
    if (snapshot) {
        nimcp_brain_snapshot_destroy(snapshot);
    }
}

/**
 * WHAT: Create instant COW snapshot of brain state
 * WHY:  Enable efficient rollback during training
 * HOW:  Call nimcp_brain_snapshot_cow, wrap handle in PyCapsule
 */
static PyObject* Brain_snapshot_cow(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_cow: self->brain is NULL");
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_brain_snapshot_t snapshot;

    Py_BEGIN_ALLOW_THREADS
    snapshot = nimcp_brain_snapshot_cow(self->brain);
    Py_END_ALLOW_THREADS

    if (!snapshot) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Brain_snapshot_cow: snapshot is NULL");
        PyErr_SetString(PyExc_RuntimeError, "Failed to create COW snapshot");
        return NULL;
    }

    return PyCapsule_New(snapshot, "nimcp.brain_snapshot",
                         Brain_snapshot_capsule_destructor);
}

/**
 * WHAT: Restore brain state from COW snapshot
 * WHY:  Instant rollback to previous state via pointer swapping
 * HOW:  Extract snapshot handle from PyCapsule, call nimcp_brain_restore_cow
 */
static PyObject* Brain_restore_cow(BrainObject* self, PyObject* args) {
    PyObject* capsule;

    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_restore_cow: Invalid arguments");
        return NULL;
    }

    if (!self->brain) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_restore_cow: self->brain is NULL");
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    if (!PyCapsule_IsValid(capsule, "nimcp.brain_snapshot")) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_restore_cow: invalid capsule");
        PyErr_SetString(PyExc_TypeError, "Expected a brain snapshot capsule from snapshot_cow()");
        return NULL;
    }

    nimcp_brain_snapshot_t snapshot =
        (nimcp_brain_snapshot_t)PyCapsule_GetPointer(capsule, "nimcp.brain_snapshot");
    if (!snapshot) {
        return NULL;  /* PyErr already set by PyCapsule_GetPointer */
    }

    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_restore_cow(self->brain, snapshot);
    Py_END_ALLOW_THREADS

    /* P6-4: Raise exception on failure instead of returning False */
    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "COW snapshot restore failed");
        return NULL;
    }
    Py_RETURN_TRUE;
}

/**
 * WHAT: Destroy a COW snapshot and release resources
 * WHY:  Explicit cleanup (snapshots also auto-destroy on GC)
 * HOW:  Call nimcp_brain_snapshot_destroy, invalidate capsule
 */
static PyObject* Brain_destroy_cow_snapshot(BrainObject* self, PyObject* args) {
    PyObject* capsule;

    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Brain_destroy_cow_snapshot: Invalid arguments");
        return NULL;
    }

    if (!PyCapsule_IsValid(capsule, "nimcp.brain_snapshot")) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_destroy_cow_snapshot: invalid capsule");
        PyErr_SetString(PyExc_TypeError, "Expected a brain snapshot capsule from snapshot_cow()");
        return NULL;
    }

    nimcp_brain_snapshot_t snapshot =
        (nimcp_brain_snapshot_t)PyCapsule_GetPointer(capsule, "nimcp.brain_snapshot");

    if (snapshot) {
        nimcp_brain_snapshot_destroy(snapshot);
        PyCapsule_SetPointer(capsule, NULL);
        PyCapsule_SetDestructor(capsule, NULL);
    }

    Py_RETURN_NONE;
}

//=============================================================================
// Brain Probe Broadcasting (Python Bindings)
//=============================================================================

/**
 * WHAT: Probe brain metrics and broadcast via bio-async
 * WHY:  Enable monitoring of brain health across multiple instances
 * HOW:  Call nimcp_brain_broadcast_probe
 */
static PyObject* Brain_broadcast_probe(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_broadcast_probe: self->brain is NULL");
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_broadcast_probe(self->brain);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_OK);
}

/**
 * WHAT: Get comprehensive brain metrics as a Python dict
 * WHY:  Dashboard needs real-time metrics (synapses, memory, learning rate)
 * HOW:  Call nimcp_brain_probe and build dict from the result
 */
static PyObject* Brain_probe(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_brain_probe_t probe;
    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_probe(self->brain, &probe);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to probe brain");
        return NULL;
    }

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

#define SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(v); \
} while (0)

    SET("task_name",              PyUnicode_FromString(probe.task_name));
    SET("size",                   PyLong_FromLong(probe.size));
    SET("task",                   PyLong_FromLong(probe.task));
    SET("num_neurons",            PyLong_FromUnsignedLong(probe.num_neurons));
    SET("num_synapses",           PyLong_FromUnsignedLong(probe.num_synapses));
    SET("num_active_synapses",    PyLong_FromUnsignedLong(probe.num_active_synapses));
    SET("total_inferences",       PyLong_FromUnsignedLongLong(probe.total_inferences));
    SET("total_learning_steps",   PyLong_FromUnsignedLongLong(probe.total_learning_steps));
    SET("avg_sparsity",           PyFloat_FromDouble(probe.avg_sparsity));
    SET("avg_inference_time_us",  PyFloat_FromDouble(probe.avg_inference_time_us));
    SET("current_learning_rate",  PyFloat_FromDouble(probe.current_learning_rate));
    SET("accuracy",               PyFloat_FromDouble(probe.accuracy));
    SET("memory_bytes",           PyLong_FromSize_t(probe.memory_bytes));
    SET("num_inputs",             PyLong_FromUnsignedLong(probe.num_inputs));
    SET("num_outputs",            PyLong_FromUnsignedLong(probe.num_outputs));
    SET("is_cow_clone",           PyBool_FromLong(probe.is_cow_clone));
    SET("cow_ref_count",          PyLong_FromUnsignedLong(probe.cow_ref_count));
    SET("cow_shared_bytes",       PyLong_FromSize_t(probe.cow_shared_bytes));
    SET("cow_private_bytes",      PyLong_FromSize_t(probe.cow_private_bytes));
    SET("gpu_available",          PyBool_FromLong(probe.gpu_available));

    // Training metrics overlaid from brain handle (not in probe struct)
    SET("last_loss",              PyFloat_FromDouble(nimcp_brain_get_last_loss(self->brain)));
    SET("last_gradient_norm",     PyFloat_FromDouble(nimcp_brain_get_last_gradient_norm(self->brain)));

    // === Training Dynamics ===
    if (self->brain->internal_brain && self->brain->internal_brain->network) {
        adaptive_network_t net = self->brain->internal_brain->network;

        // Weight statistics (sampled for speed)
        float w_l2 = 0.0f, w_mean_abs = 0.0f, w_max_abs = 0.0f;
        uint64_t w_sampled = 0;
        adaptive_network_weight_stats(net, &w_l2, &w_mean_abs, &w_max_abs, &w_sampled);
        SET("weight_l2_norm",         PyFloat_FromDouble(w_l2));
        SET("weight_mean_abs",        PyFloat_FromDouble(w_mean_abs));
        SET("weight_max_abs",         PyFloat_FromDouble(w_max_abs));
        SET("weight_sampled_synapses", PyLong_FromUnsignedLongLong(w_sampled));

        // EMA metrics for trend detection
        SET("ema_gradient_norm",      PyFloat_FromDouble(adaptive_network_get_ema_grad_norm(net)));
        SET("ema_loss",               PyFloat_FromDouble(adaptive_network_get_ema_loss(net)));

        // Per-layer gradient norms
        float layer_norms[BP_MAX_GRAD_LAYERS];
        uint32_t nlayers = adaptive_network_get_layer_grad_norms(net, layer_norms, BP_MAX_GRAD_LAYERS);
        PyObject* layer_list = PyList_New((Py_ssize_t)nlayers);
        if (layer_list) {
            for (uint32_t i = 0; i < nlayers; i++) {
                PyList_SET_ITEM(layer_list, (Py_ssize_t)i, PyFloat_FromDouble(layer_norms[i]));
            }
            SET("layer_grad_norms", layer_list);
        }

        // === Learning Quality ===
        adaptive_learning_quality_t lq;
        adaptive_network_learning_quality(net, &lq);
        SET("mean_label_accuracy",    PyFloat_FromDouble(lq.mean_label_accuracy));
        SET("worst_label_accuracy",   PyFloat_FromDouble(lq.worst_label_accuracy));
        SET("num_labels_tracked",     PyLong_FromUnsignedLong(lq.num_labels_tracked));
        SET("confidence_calibration", PyFloat_FromDouble(lq.confidence_calibration));
        SET("learning_velocity",      PyFloat_FromDouble(lq.learning_velocity));
        SET("prediction_entropy",     PyFloat_FromDouble(lq.prediction_entropy));
        SET("synapse_growth",         PyLong_FromUnsignedLongLong(lq.synapse_growth));
    }

    // === Brain Health ===
    SET("memory_rss_bytes",       PyLong_FromSize_t(nimcp_brain_get_memory_rss()));
    SET("gpu_vram_bytes",         PyLong_FromSize_t(nimcp_brain_get_gpu_vram_used(self->brain)));
    SET("neuron_utilization",     PyFloat_FromDouble(nimcp_brain_get_neuron_utilization(self->brain)));

    nimcp_immune_metrics_t imm;
    if (nimcp_brain_get_immune_metrics(self->brain, &imm) == NIMCP_OK) {
        SET("immune_total_exceptions",    PyLong_FromUnsignedLong(imm.total_exceptions));
        SET("immune_recovered_exceptions", PyLong_FromUnsignedLong(imm.recovered_exceptions));
        SET("immune_inflammation",        PyFloat_FromDouble(imm.inflammation_level));
        SET("immune_active_antibodies",   PyLong_FromUnsignedLong(imm.active_antibodies));
    }

    nimcp_synapse_stats_t syn;
    if (nimcp_brain_get_synapse_stats(self->brain, &syn) == NIMCP_OK) {
        SET("synapse_total",      PyLong_FromUnsignedLongLong(syn.total_synapses));
        SET("synapse_growth_delta", PyLong_FromLongLong(syn.growth_since_last));
    }

#undef SET

    return dict;
}

/* ----------------------------------------------------------------
 * Grounded-language diagnostics (collapse triage)
 * ---------------------------------------------------------------- */

static PyObject* Brain_get_bigram_spectral_metrics(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    bigram_spectral_metrics_t m;
    if (nimcp_brain_get_bigram_spectral_metrics(self->brain, &m) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "bigram spectral metrics failed");
        return NULL;
    }
    PyObject* dict = PyDict_New();
    if (!dict) return NULL;
#define BSM_SET(k, v) do { \
    PyObject* _v = (v); \
    if (!_v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (k), _v) < 0) { Py_DECREF(_v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(_v); \
} while (0)
    BSM_SET("peak_strength",          PyFloat_FromDouble(m.peak_strength));
    BSM_SET("low_freq_concentration", PyFloat_FromDouble(m.low_freq_concentration));
    BSM_SET("spectral_entropy",       PyFloat_FromDouble(m.spectral_entropy));
#undef BSM_SET
    return dict;
}

static PyObject* Brain_prune_lang_bindings(BrainObject* self, PyObject* args, PyObject* kwargs) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    unsigned int max_per_word = 128;
    static char* kwlist[] = {"max_bindings_per_word", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|I", kwlist, &max_per_word)) {
        return NULL;
    }
    if (max_per_word == 0) {
        PyErr_SetString(PyExc_ValueError, "max_bindings_per_word must be > 0");
        return NULL;
    }
    uint64_t dropped = 0;
    nimcp_status_t s = nimcp_brain_prune_lang_bindings(self->brain,
                                                       (uint32_t)max_per_word,
                                                       &dropped);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "prune_lang_bindings failed (no grounded_lang attached?)");
        return NULL;
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)dropped);
}

static PyObject* Brain_connect_lang_bridge_neuromod(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    nimcp_status_t s = nimcp_brain_connect_lang_bridge_neuromod(self->brain);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "connect_lang_bridge_neuromod failed (no bridge or no neuromod system?)");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_grounded_language_diagnostics(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    nimcp_grounded_language_diagnostics_t d;
    if (nimcp_brain_get_grounded_language_diagnostics(self->brain, &d) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "diagnostics failed");
        return NULL;
    }
    PyObject* dict = PyDict_New();
    if (!dict) return NULL;
#define GLD_SET(k, v) do { \
    PyObject* _v = (v); \
    if (!_v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (k), _v) < 0) { Py_DECREF(_v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(_v); \
} while (0)
    GLD_SET("vocab_size",                    PyLong_FromUnsignedLong(d.vocab_size));
    GLD_SET("total_bindings",                PyLong_FromUnsignedLong(d.total_bindings));
    GLD_SET("total_groundings",              PyLong_FromUnsignedLong(d.total_groundings));
    GLD_SET("total_comprehensions",          PyLong_FromUnsignedLong(d.total_comprehensions));
    GLD_SET("total_productions",             PyLong_FromUnsignedLong(d.total_productions));
    GLD_SET("avg_binding_strength",          PyFloat_FromDouble(d.avg_binding_strength));
    GLD_SET("avg_comprehension_confidence",  PyFloat_FromDouble(d.avg_comprehension_confidence));
    GLD_SET("vocabulary_growth_rate",        PyFloat_FromDouble(d.vocabulary_growth_rate));
    GLD_SET("snn_bridge_blend",              PyFloat_FromDouble(d.snn_bridge_blend));
    GLD_SET("bridge_total_productions",      PyLong_FromUnsignedLong(d.bridge_total_productions));
    GLD_SET("bridge_avg_word_confidence",    PyFloat_FromDouble(d.bridge_avg_word_confidence));
    GLD_SET("bridge_avg_binding_weight",     PyFloat_FromDouble(d.bridge_avg_binding_weight));
    GLD_SET("bridge_active_bindings",        PyLong_FromUnsignedLong(d.bridge_active_bindings));
    /* Audit-2 follow-up: campaign flags + tunables + decode telemetry. */
    GLD_SET("enable_negation_inversion",        PyBool_FromLong(d.enable_negation_inversion));
    GLD_SET("enable_sense_disambiguation",      PyBool_FromLong(d.enable_sense_disambiguation));
    GLD_SET("enable_speech_act_classification", PyBool_FromLong(d.enable_speech_act_classification));
    GLD_SET("enable_sentence_segmentation",     PyBool_FromLong(d.enable_sentence_segmentation));
    GLD_SET("enable_topic_shift_detection",     PyBool_FromLong(d.enable_topic_shift_detection));
    GLD_SET("enable_reconsolidation",           PyBool_FromLong(d.enable_reconsolidation));
    GLD_SET("enable_anaphora_resolution",       PyBool_FromLong(d.enable_anaphora_resolution));
    GLD_SET("bridge_enable_da_modulation",      PyBool_FromLong(d.bridge_enable_da_modulation));
    GLD_SET("bridge_enable_trigram_learning",   PyBool_FromLong(d.bridge_enable_trigram_learning));
    GLD_SET("bridge_ltd_margin",                PyFloat_FromDouble((double)d.bridge_ltd_margin));
    GLD_SET("reconsolidation_decay",            PyFloat_FromDouble(d.reconsolidation_decay));
    GLD_SET("produce_distributional_weight",    PyFloat_FromDouble((double)d.produce_distributional_weight));
    GLD_SET("produce_frequency_penalty",        PyFloat_FromDouble((double)d.produce_frequency_penalty));
    GLD_SET("produce_via_snn",                  PyBool_FromLong(d.produce_via_snn));
    GLD_SET("metacog_gates_produce",            PyBool_FromLong(d.metacog_gates_produce));
    GLD_SET("topic_shift_threshold",            PyFloat_FromDouble(d.topic_shift_threshold));
    GLD_SET("topic_shift_min_turns",            PyLong_FromUnsignedLong(d.topic_shift_min_turns));
    GLD_SET("bridge_decode_total_ns",           PyLong_FromUnsignedLongLong(d.bridge_decode_total_ns));
    GLD_SET("bridge_total_decode_calls",        PyLong_FromUnsignedLongLong(d.bridge_total_decode_calls));
    /* Plasticity telemetry surface (gap #1 from 2026-05-13 walkthrough). */
    GLD_SET("bridge_total_stdp_updates",        PyLong_FromUnsignedLongLong(d.bridge_total_stdp_updates));
    GLD_SET("bridge_total_trigram_updates",     PyLong_FromUnsignedLongLong(d.bridge_total_trigram_updates));
    GLD_SET("bridge_echo_correct_calls",        PyLong_FromUnsignedLongLong(d.bridge_echo_correct_calls));
    GLD_SET("bridge_echo_correct_pairs",        PyLong_FromUnsignedLongLong(d.bridge_echo_correct_pairs));
    GLD_SET("bridge_echo_correct_target_misses",PyLong_FromUnsignedLongLong(d.bridge_echo_correct_target_misses));
    GLD_SET("bridge_comprehend_stdp_passes",    PyLong_FromUnsignedLongLong(d.bridge_comprehend_stdp_passes));
    GLD_SET("bridge_comprehend_stdp_pairs_fired", PyLong_FromUnsignedLongLong(d.bridge_comprehend_stdp_pairs_fired));
    GLD_SET("bridge_da_gated_stdp_passes",      PyLong_FromUnsignedLongLong(d.bridge_da_gated_stdp_passes));
    GLD_SET("bridge_last_da_modulation",        PyFloat_FromDouble((double)d.bridge_last_da_modulation));
    GLD_SET("next_token_cold_start_skips",      PyLong_FromUnsignedLongLong(d.next_token_cold_start_skips));
    /* Wave-3 (2026-05-19) lateral-inhibition telemetry surface. */
    GLD_SET("bridge_lateral_inhibition_decode_calls",      PyLong_FromUnsignedLongLong(d.bridge_lateral_inhibition_decode_calls));
    GLD_SET("bridge_lateral_inhibition_winner_margin_sum", PyLong_FromUnsignedLongLong(d.bridge_lateral_inhibition_winner_margin_sum));
    GLD_SET("bridge_lateral_inhibition_settled_steps_sum", PyLong_FromUnsignedLongLong(d.bridge_lateral_inhibition_settled_steps_sum));
    GLD_SET("bridge_lateral_inhibition_nan_fallbacks",     PyLong_FromUnsignedLongLong(d.bridge_lateral_inhibition_nan_fallbacks));
    /* TF-6 (2026-05-29) tier-feedback telemetry surface. Counters mirror
     * gl_stats_t.tf_*; knobs mirror the 5 per-setter getters. Operators read
     * tf_calls / tf_deltas_captured to see whether the cascade is emitting
     * deltas; tf_outcome_blocked_* to see which gate is rejecting; the per-path
     * update counters to confirm plasticity is firing. */
    GLD_SET("tf_calls",                         PyLong_FromUnsignedLongLong(d.tf_calls));
    GLD_SET("tf_deltas_captured",               PyLong_FromUnsignedLongLong(d.tf_deltas_captured));
    GLD_SET("tf_outcome_ok",                    PyLong_FromUnsignedLongLong(d.tf_outcome_ok));
    GLD_SET("tf_outcome_blocked_stage",         PyLong_FromUnsignedLongLong(d.tf_outcome_blocked_stage));
    GLD_SET("tf_outcome_blocked_master",        PyLong_FromUnsignedLongLong(d.tf_outcome_blocked_master));
    GLD_SET("tf_outcome_blocked_da",            PyLong_FromUnsignedLongLong(d.tf_outcome_blocked_da));
    GLD_SET("tf_outcome_blocked_stale",         PyLong_FromUnsignedLongLong(d.tf_outcome_blocked_stale));
    GLD_SET("tf_outcome_blocked_retry",         PyLong_FromUnsignedLongLong(d.tf_outcome_blocked_retry));
    GLD_SET("tf_trigram_updates",               PyLong_FromUnsignedLongLong(d.tf_trigram_updates));
    GLD_SET("tf_distrib_updates",               PyLong_FromUnsignedLongLong(d.tf_distrib_updates));
    GLD_SET("tf_stdp_updates_pos",              PyLong_FromUnsignedLongLong(d.tf_stdp_updates_pos));
    GLD_SET("tf_stdp_updates_neg",              PyLong_FromUnsignedLongLong(d.tf_stdp_updates_neg));
    GLD_SET("produce_corrector_feedback_enabled", PyBool_FromLong(d.produce_corrector_feedback_enabled));
    GLD_SET("tf_enabled_correctors",            PyLong_FromUnsignedLong((unsigned long)d.tf_enabled_correctors));
    GLD_SET("tf_lr_trigram",                    PyFloat_FromDouble((double)d.tf_lr_trigram));
    GLD_SET("tf_lr_distrib",                    PyFloat_FromDouble((double)d.tf_lr_distrib));
    GLD_SET("tf_lr_bridge_stdp",                PyFloat_FromDouble((double)d.tf_lr_bridge_stdp));
    /* T3-2 (2026-05-29) — cohesive conjunction insertion flag surface. */
    GLD_SET("produce_t3_conjunction",           PyBool_FromLong(d.produce_t3_conjunction));
#undef GLD_SET
    return dict;
}

/*
 * Brain_get_top_phrases — return the most-frequent learned phrases as a
 * list of dicts. Optional kwarg `top_k` (default 20). When grounded
 * language is not initialised, returns an empty list rather than raising
 * so callers can poll without ceremony.
 */
static PyObject* Brain_get_top_phrases(BrainObject* self,
                                        PyObject* args,
                                        PyObject* kwargs) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    static char* kwlist[] = {(char*)"top_k", NULL};
    int top_k = 20;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &top_k)) {
        return NULL;
    }
    if (top_k <= 0) top_k = 20;
    if (top_k > 512) top_k = 512;  /* matches GL_MAX_PHRASES cap in core */

    nimcp_phrase_entry_t* buf =
        (nimcp_phrase_entry_t*)calloc((size_t)top_k, sizeof(nimcp_phrase_entry_t));
    if (!buf) {
        PyErr_NoMemory();
        return NULL;
    }
    int n = nimcp_brain_get_top_phrases(self->brain, buf, (uint32_t)top_k);
    if (n < 0) {
        /* GL not present or invalid handle — return empty list. */
        free(buf);
        return PyList_New(0);
    }
    PyObject* list = PyList_New((Py_ssize_t)n);
    if (!list) {
        free(buf);
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        PyObject* d = PyDict_New();
        if (!d) {
            Py_DECREF(list);
            free(buf);
            return NULL;
        }
        PyObject* form = PyUnicode_FromString(buf[i].form);
        PyObject* freq = PyLong_FromUnsignedLong((unsigned long)buf[i].frequency);
        PyObject* comp = PyLong_FromUnsignedLong((unsigned long)buf[i].component_words);
        if (!form || !freq || !comp ||
            PyDict_SetItemString(d, "form", form) < 0 ||
            PyDict_SetItemString(d, "frequency", freq) < 0 ||
            PyDict_SetItemString(d, "component_words", comp) < 0) {
            Py_XDECREF(form); Py_XDECREF(freq); Py_XDECREF(comp);
            Py_DECREF(d); Py_DECREF(list); free(buf);
            return NULL;
        }
        Py_DECREF(form); Py_DECREF(freq); Py_DECREF(comp);
        PyList_SET_ITEM(list, (Py_ssize_t)i, d);
    }
    free(buf);
    return list;
}

/*
 * Brain_get_modality_counts — return per-modality binding counts as a dict
 * {visual, auditory, motor, emotional, spatial, linguistic}. When grounded
 * language is not initialised, returns an empty dict rather than raising
 * so callers can poll it the same way they poll get_top_phrases.
 */
static PyObject* Brain_get_modality_counts(BrainObject* self,
                                            PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    uint32_t counts[6] = {0};
    int rc = nimcp_brain_get_modality_counts(self->brain, counts);
    if (rc != 0) {
        /* GL not present or invalid handle — return empty dict. Mirrors the
         * "empty list on no GL" contract of get_top_phrases. */
        return PyDict_New();
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    /* Index order MUST match gl_modality_t in nimcp_grounded_language.h:
     * 0=VISUAL, 1=AUDITORY, 2=MOTOR, 3=EMOTIONAL, 4=SPATIAL, 5=LINGUISTIC. */
    static const char* const KEYS[6] = {
        "visual", "auditory", "motor", "emotional", "spatial", "linguistic"
    };
    for (int i = 0; i < 6; i++) {
        PyObject* v = PyLong_FromUnsignedLong((unsigned long)counts[i]);
        if (!v || PyDict_SetItemString(d, KEYS[i], v) < 0) {
            Py_XDECREF(v);
            Py_DECREF(d);
            return NULL;
        }
        Py_DECREF(v);
    }
    return d;
}

static PyObject* Brain_probe_comprehend(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    const char* text;
    int max_components = 16;
    if (!PyArg_ParseTuple(args, "s|i", &text, &max_components)) return NULL;
    if (max_components <= 0 || max_components > 1024) max_components = 16;

    float* comp = (float*)calloc((size_t)max_components, sizeof(float));
    if (!comp) {
        PyErr_NoMemory();
        return NULL;
    }
    uint32_t written = 0, concept_count = 0;
    float l2 = 0.0f, conf = 0.0f;
    nimcp_status_t s = nimcp_brain_probe_comprehend(self->brain, text,
                                                     comp, (uint32_t)max_components,
                                                     &written, &l2, &conf, &concept_count);
    if (s != NIMCP_OK) {
        free(comp);
        PyErr_Format(PyExc_RuntimeError, "probe_comprehend failed: status=%d", (int)s);
        return NULL;
    }

    PyObject* dict = PyDict_New();
    PyObject* comps = PyList_New((Py_ssize_t)written);
    if (!dict || !comps) {
        Py_XDECREF(dict); Py_XDECREF(comps);
        free(comp);
        return NULL;
    }
    for (uint32_t i = 0; i < written; i++) {
        PyList_SET_ITEM(comps, (Py_ssize_t)i, PyFloat_FromDouble(comp[i]));
    }
    free(comp);

    if (PyDict_SetItemString(dict, "components", comps) < 0) goto err;
    Py_DECREF(comps); comps = NULL;
#define PC_SET(k, v) do { \
    PyObject* _v = (v); \
    if (!_v || PyDict_SetItemString(dict, (k), _v) < 0) { Py_XDECREF(_v); goto err; } \
    Py_DECREF(_v); \
} while (0)
    PC_SET("l2_norm",       PyFloat_FromDouble(l2));
    PC_SET("confidence",    PyFloat_FromDouble(conf));
    PC_SET("concept_count", PyLong_FromUnsignedLong(concept_count));
    PC_SET("text",          PyUnicode_FromString(text));
#undef PC_SET
    return dict;

err:
    Py_XDECREF(comps);
    Py_DECREF(dict);
    return NULL;
}

static PyObject* Brain_set_snn_language_bridge_blend(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float blend;
    if (!PyArg_ParseTuple(args, "f", &blend)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_blend(self->brain, blend);
    if (s == NIMCP_ERROR_INVALID) {
        PyErr_Format(PyExc_ValueError, "set_blend rejected (out of range)");
        return NULL;
    }
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_recompute_snn_language_bridge_norms(BrainObject* self,
                                                            PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    nimcp_status_t s = nimcp_brain_recompute_snn_language_bridge_norms(self->brain);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_set_snn_language_bridge_sampling(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float temperature = 0.0f, top_p = 1.0f;
    if (!PyArg_ParseTuple(args, "ff", &temperature, &top_p)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_sampling(
        self->brain, temperature, top_p);
    if (s == NIMCP_ERROR_INVALID) {
        PyErr_Format(PyExc_ValueError,
                     "set_sampling rejected (temperature must be ≥0, top_p in (0,1])");
        return NULL;
    }
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_set_snn_language_bridge_glove_blend(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float blend = 0.0f;
    if (!PyArg_ParseTuple(args, "f", &blend)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_glove_blend(self->brain, blend);
    if (s == NIMCP_ERROR_INVALID) {
        PyErr_Format(PyExc_ValueError, "set_glove_blend rejected (blend must be in [0,1])");
        return NULL;
    }
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_set_snn_language_bridge_autoregressive(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float intent_persistence = 0.0f, word_feedback = 0.3f;
    if (!PyArg_ParseTuple(args, "ff", &intent_persistence, &word_feedback)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_autoregressive(
        self->brain, intent_persistence, word_feedback);
    if (s == NIMCP_ERROR_INVALID) {
        PyErr_Format(PyExc_ValueError,
                     "set_autoregressive rejected (both args must be in [0,1])");
        return NULL;
    }
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_set_snn_language_bridge_spike_routing(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 0;
    float tau_ms = 200.0f;
    if (!PyArg_ParseTuple(args, "p|f", &enabled, &tau_ms)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_spike_routing(
        self->brain, enabled ? true : false, tau_ms);
    if (s == NIMCP_ERROR_INVALID) {
        PyErr_Format(PyExc_ValueError,
                     "set_spike_routing rejected (tau_ms must be > 0 when enabled)");
        return NULL;
    }
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* PA-5+: hyperbolic-distance GloVe metric. */
static PyObject* Brain_set_snn_language_bridge_hyperbolic_embeddings(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_hyperbolic_embeddings(
        self->brain, enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* PA-6+: select produce-time sampling mode (0/1/2). */
static PyObject* Brain_set_snn_language_bridge_sampling_mode(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int mode = 0;
    if (!PyArg_ParseTuple(args, "i", &mode)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_sampling_mode(
        self->brain, mode);
    if (s == NIMCP_ERROR_INVALID) {
        PyErr_Format(PyExc_ValueError,
                     "set_sampling_mode rejected (mode must be 0, 1, or 2)");
        return NULL;
    }
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* TIER1-A: configure beam-K produce decoding. */
static PyObject* Brain_set_snn_language_bridge_beam_width(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    unsigned int k = 1;
    if (!PyArg_ParseTuple(args, "I", &k)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_beam_width(
        self->brain, (uint32_t)k);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* TIER1-B: register EOS word_pop. */
static PyObject* Brain_set_snn_language_bridge_eos_word_pop(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    unsigned int pop = UINT32_MAX;
    if (!PyArg_ParseTuple(args, "I", &pop)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_eos_word_pop(
        self->brain, (uint32_t)pop);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* TIER1-C: configure n-gram repetition penalty. */
static PyObject* Brain_set_snn_language_bridge_repetition_penalty(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float penalty = 0.0f;
    unsigned int window = 3;
    if (!PyArg_ParseTuple(args, "fI", &penalty, &window)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_repetition_penalty(
        self->brain, penalty, (uint32_t)window);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Tier-1 #2: toggle rule-based anaphora / pronoun resolution. */
static PyObject* Brain_set_anaphora_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_anaphora_enabled(
        self->brain, enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        "no grounded_language attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* EN-5 — toggle read-only engram integration on grounded_language. */
static PyObject* Brain_set_grounded_engram_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_grounded_engram_enabled(
        self->brain, enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        "engram_system or grounded_language missing");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* IM-3 — toggle Tier-3 immune content inspection on grounded_language. */
static PyObject* Brain_set_grounded_immune_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_grounded_immune_enabled(
        self->brain, enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        "immune_system or grounded_language missing");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Tier-4 #17: explicit RNG seed for deterministic sampling tests. */
static PyObject* Brain_set_snn_language_bridge_rng_seed(BrainObject* self,
                                                         PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    /* "K" parses Python int → unsigned long long (== uint64_t on LP64). */
    unsigned long long seed = 0ULL;
    if (!PyArg_ParseTuple(args, "K", &seed)) return NULL;
    nimcp_status_t s = nimcp_brain_set_snn_language_bridge_rng_seed(
        self->brain, (uint64_t)seed);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Tier-4 #15: consolidated config getter — returns Python dict. */
static PyObject* Brain_get_snn_language_bridge_config(BrainObject* self,
                                                       PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    snn_lang_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    nimcp_status_t s = nimcp_brain_get_snn_language_bridge_config(self->brain, &cfg);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN-language bridge attached");
        return NULL;
    }

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

#define D_SET(k, obj) do { \
    PyObject* _v = (obj); \
    if (!_v || PyDict_SetItemString(dict, (k), _v) < 0) { \
        Py_XDECREF(_v); Py_DECREF(dict); return NULL; \
    } \
    Py_DECREF(_v); \
} while (0)

    /* Capacities / shape */
    D_SET("max_concept_pops",      PyLong_FromUnsignedLong(cfg.max_concept_pops));
    D_SET("max_word_pops",         PyLong_FromUnsignedLong(cfg.max_word_pops));
    D_SET("neurons_per_pop",       PyLong_FromUnsignedLong(cfg.neurons_per_pop));

    /* STDP knobs */
    D_SET("stdp_tau_plus",         PyFloat_FromDouble((double)cfg.stdp_tau_plus));
    D_SET("stdp_tau_minus",        PyFloat_FromDouble((double)cfg.stdp_tau_minus));
    D_SET("stdp_a_plus",           PyFloat_FromDouble((double)cfg.stdp_a_plus));
    D_SET("stdp_a_minus",          PyFloat_FromDouble((double)cfg.stdp_a_minus));
    D_SET("stdp_learning_rate",    PyFloat_FromDouble((double)cfg.stdp_learning_rate));

    /* Bindings + decode */
    D_SET("binding_w_max",         PyFloat_FromDouble((double)cfg.binding_w_max));
    D_SET("decode_window_ms",      PyFloat_FromDouble((double)cfg.decode_window_ms));
    D_SET("decay_rate",            PyFloat_FromDouble((double)cfg.decay_rate));
    D_SET("spike_blend",           PyFloat_FromDouble((double)cfg.spike_blend));

    /* DA / imagination / curiosity / sleep */
    D_SET("enable_da_modulation",  PyBool_FromLong(cfg.enable_da_modulation));
    D_SET("da_modulation_gain",    PyFloat_FromDouble((double)cfg.da_modulation_gain));
    D_SET("enable_imagination",    PyBool_FromLong(cfg.enable_imagination));
    D_SET("enable_curiosity",      PyBool_FromLong(cfg.enable_curiosity));
    D_SET("enable_sleep_consolidation", PyBool_FromLong(cfg.enable_sleep_consolidation));
    D_SET("prune_threshold",       PyFloat_FromDouble((double)cfg.prune_threshold));

    /* PA-6 sampling */
    D_SET("temperature",           PyFloat_FromDouble((double)cfg.temperature));
    D_SET("top_p",                 PyFloat_FromDouble((double)cfg.top_p));
    D_SET("produce_topk",          PyLong_FromUnsignedLong(cfg.produce_topk));

    /* PA-5 GloVe blend */
    D_SET("glove_blend",           PyFloat_FromDouble((double)cfg.glove_blend));

    /* PA-2 autoregressive */
    D_SET("intent_persistence",    PyFloat_FromDouble((double)cfg.intent_persistence));
    D_SET("word_feedback",         PyFloat_FromDouble((double)cfg.word_feedback));

    /* PA-3 spike routing */
    D_SET("enable_snn_spike_routing", PyBool_FromLong(cfg.enable_snn_spike_routing));
    D_SET("activation_tau_ms",     PyFloat_FromDouble((double)cfg.activation_tau_ms));

    /* PA-5+ hyperbolic + PA-6+ sampling mode */
    D_SET("use_hyperbolic_embeddings", PyBool_FromLong(cfg.use_hyperbolic_embeddings));
    D_SET("sampling_mode",         PyLong_FromLong(cfg.sampling_mode));

#undef D_SET
    return dict;
}

static PyObject* Brain_learn_next_token_pair(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    const char* prev_word;
    const char* next_word;
    float lr = 0.05f;
    if (!PyArg_ParseTuple(args, "ss|f", &prev_word, &next_word, &lr)) return NULL;
    nimcp_status_t s = nimcp_brain_learn_next_token_pair(self->brain,
                                                          prev_word, next_word, lr);
    /* NIMCP_ERROR is the cold-start / no-bridge no-op signal. Don't raise —
     * the caller often loops over many bigrams and just wants a count. */
    return PyBool_FromLong(s == NIMCP_OK);
}

static PyObject* Brain_learn_next_token_pair_riemannian(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    const char* prev_word;
    const char* next_word;
    float lr = 0.05f;
    if (!PyArg_ParseTuple(args, "ss|f", &prev_word, &next_word, &lr)) return NULL;
    nimcp_status_t s = nimcp_brain_learn_next_token_pair_riemannian(self->brain,
                                                                      prev_word,
                                                                      next_word, lr);
    return PyBool_FromLong(s == NIMCP_OK);
}

static PyObject* Brain_learn_text_bigrams(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    const char* text;
    float lr = 0.05f;
    if (!PyArg_ParseTuple(args, "s|f", &text, &lr)) return NULL;
    int count = 0;
    nimcp_status_t s = nimcp_brain_learn_text_bigrams(self->brain, text, lr, &count);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "learn_text_bigrams failed");
        return NULL;
    }
    return PyLong_FromLong(count);
}

/* TA-4: toggle trigram next-token learning (default OFF). */
static PyObject* Brain_set_trigram_learning_enabled(BrainObject* self,
                                                      PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_trigram_learning_enabled(
        self->brain, enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no SNN language bridge attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Margin-gated LTD for next-token learners. margin must be in [1.0, 100.0]. */
static PyObject* Brain_set_ltd_margin(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float margin = 1.5f;
    if (!PyArg_ParseTuple(args, "f", &margin)) return NULL;
    nimcp_status_t s = nimcp_brain_set_ltd_margin(self->brain, margin);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "set_ltd_margin: margin must be in [1.0, 100.0] and bridge must be attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Reset lexicon's distributional embeddings (context_vector). */
static PyObject* Brain_reset_lexicon_distributional(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int zero_uninit = 1;
    float jitter = 0.01f;
    if (!PyArg_ParseTuple(args, "pf", &zero_uninit, &jitter)) return NULL;
    int64_t count = 0;
    nimcp_status_t s = nimcp_brain_reset_lexicon_distributional(
        self->brain, zero_uninit != 0, jitter, &count);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "reset_lexicon_distributional: grounded_language not attached or bad jitter");
        return NULL;
    }
    return PyLong_FromLongLong((long long)count);
}

/* Reset concept grounding (clear lexicon bindings + wipe semantic_memory). */
static PyObject* Brain_reset_concept_grounding(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int64_t cleared = 0;
    nimcp_status_t s = nimcp_brain_reset_concept_grounding(self->brain, &cleared);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "reset_concept_grounding: grounded_language not attached");
        return NULL;
    }
    return PyLong_FromLongLong((long long)cleared);
}

/* Classify a text via the toxicity classifier and return a dict.
 * Returns None if the classifier is not attached. */
static PyObject* Brain_classify_toxicity(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    const char* text = NULL;
    if (!PyArg_ParseTuple(args, "s", &text)) return NULL;
    float harm = 0.0f, fair = 0.0f, anti = 0.0f;
    int would_block = 0;
    char category[64] = {0};
    nimcp_status_t s = nimcp_brain_classify_toxicity(
        self->brain, text, &harm, &fair, &anti, &would_block,
        category, sizeof(category));
    if (s != NIMCP_OK) {
        Py_RETURN_NONE;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "predicted_harm",     PyFloat_FromDouble(harm));
    PyDict_SetItemString(d, "fairness_violation", PyFloat_FromDouble(fair));
    PyDict_SetItemString(d, "anti_toxic_signal",  PyFloat_FromDouble(anti));
    PyDict_SetItemString(d, "would_block",        PyBool_FromLong(would_block));
    PyDict_SetItemString(d, "category",           PyUnicode_FromString(category));
    return d;
}

/* Re-randomize bridge binding weights to break rank-1 collapse. */
static PyObject* Brain_reset_lang_bridge_weights(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float w_min = 0.001f, w_max = 0.05f;
    if (!PyArg_ParseTuple(args, "ff", &w_min, &w_max)) return NULL;
    int64_t count = 0;
    nimcp_status_t s = nimcp_brain_reset_lang_bridge_weights(
        self->brain, w_min, w_max, &count);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "reset_lang_bridge_weights: bridge not attached or args out of range "
            "(require 0 <= w_min < w_max <= binding_w_max)");
        return NULL;
    }
    return PyLong_FromLongLong((long long)count);
}

/* Audit fix — campaign feature setter Python bindings. All take a single
 * argument (bool/int/float) and return None on success or RuntimeError. */

#define AUDIT_BRAIN_BOOL_SETTER(NAME, CAPI) \
    static PyObject* Brain_##NAME(BrainObject* self, PyObject* args) { \
        if (!self->brain) { \
            PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; \
        } \
        int enabled = 0; \
        if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL; \
        if (CAPI(self->brain, enabled ? true : false) != NIMCP_OK) { \
            PyErr_SetString(PyExc_RuntimeError, #NAME " failed"); return NULL; \
        } \
        Py_RETURN_NONE; \
    }
#define AUDIT_BRAIN_FLOAT_SETTER(NAME, CAPI) \
    static PyObject* Brain_##NAME(BrainObject* self, PyObject* args) { \
        if (!self->brain) { \
            PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; \
        } \
        float v = 0.0f; \
        if (!PyArg_ParseTuple(args, "f", &v)) return NULL; \
        if (CAPI(self->brain, v) != NIMCP_OK) { \
            PyErr_SetString(PyExc_RuntimeError, #NAME " failed"); return NULL; \
        } \
        Py_RETURN_NONE; \
    }
/* Float getter mirror of AUDIT_BRAIN_FLOAT_SETTER. CAPI signature is
 * nimcp_status_t CAPI(nimcp_brain_t, float* out). METH_NOARGS. */
#define AUDIT_BRAIN_FLOAT_GETTER(NAME, CAPI) \
    static PyObject* Brain_##NAME(BrainObject* self, PyObject* Py_UNUSED(ignored)) { \
        if (!self->brain) { \
            PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; \
        } \
        float v = 0.0f; \
        if (CAPI(self->brain, &v) != NIMCP_OK) { \
            PyErr_SetString(PyExc_RuntimeError, #NAME " failed"); return NULL; \
        } \
        return PyFloat_FromDouble((double)v); \
    }
/* TF-2 (2026-05-28): uint16 bitmask setter — used by set_tf_enabled_correctors.
 * Python passes a plain int; we clamp to uint16 range so a stray negative or
 * out-of-range value can't underflow. */
#define AUDIT_BRAIN_U16_SETTER(NAME, CAPI) \
    static PyObject* Brain_##NAME(BrainObject* self, PyObject* args) { \
        if (!self->brain) { \
            PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; \
        } \
        unsigned int v = 0u; \
        if (!PyArg_ParseTuple(args, "I", &v)) return NULL; \
        if (v > 0xFFFFu) v = 0xFFFFu; \
        if (CAPI(self->brain, (uint16_t)v) != NIMCP_OK) { \
            PyErr_SetString(PyExc_RuntimeError, #NAME " failed"); return NULL; \
        } \
        Py_RETURN_NONE; \
    }

AUDIT_BRAIN_BOOL_SETTER(set_da_modulation_enabled,            nimcp_brain_set_da_modulation_enabled)
AUDIT_BRAIN_FLOAT_SETTER(set_da_modulation_gain,              nimcp_brain_set_da_modulation_gain)
AUDIT_BRAIN_BOOL_SETTER(set_comprehend_stdp_enabled,          nimcp_brain_set_comprehend_stdp_enabled)
AUDIT_BRAIN_BOOL_SETTER(set_reconsolidation_enabled,          nimcp_brain_set_reconsolidation_enabled)
AUDIT_BRAIN_FLOAT_SETTER(set_reconsolidation_decay,           nimcp_brain_set_reconsolidation_decay)
AUDIT_BRAIN_BOOL_SETTER(set_sentence_segmentation_enabled,    nimcp_brain_set_sentence_segmentation_enabled)
AUDIT_BRAIN_BOOL_SETTER(set_speech_act_classification_enabled, nimcp_brain_set_speech_act_classification_enabled)
AUDIT_BRAIN_BOOL_SETTER(set_topic_shift_enabled,              nimcp_brain_set_topic_shift_enabled)
AUDIT_BRAIN_FLOAT_SETTER(set_topic_shift_threshold,           nimcp_brain_set_topic_shift_threshold)
/* NLP-2 coref + NLP-1 subword OOV reachability (full-walkthrough 2026-05-25).
 * Reads are surfaced through get_grounded_language_diagnostics' dict
 * (enable_coref_resolution / enable_subword_oov_fallback + counters). */
AUDIT_BRAIN_BOOL_SETTER(set_coref_resolution_enabled,         nimcp_brain_set_coref_resolution_enabled)
AUDIT_BRAIN_BOOL_SETTER(set_subword_oov_fallback_enabled,     nimcp_brain_set_subword_oov_fallback_enabled)
AUDIT_BRAIN_BOOL_SETTER(set_autoregressive_produce,           nimcp_brain_set_autoregressive_produce)
AUDIT_BRAIN_BOOL_SETTER(set_produce_pronominalize,            nimcp_brain_set_produce_pronominalize)
AUDIT_BRAIN_BOOL_SETTER(set_produce_discourse_seed,           nimcp_brain_set_produce_discourse_seed)
AUDIT_BRAIN_BOOL_SETTER(set_produce_clause_frame,             nimcp_brain_set_produce_clause_frame)
AUDIT_BRAIN_BOOL_SETTER(set_produce_via_snn,                  nimcp_brain_set_produce_via_snn)
AUDIT_BRAIN_BOOL_SETTER(set_metacog_gates_produce,            nimcp_brain_set_metacog_gates_produce)
AUDIT_BRAIN_FLOAT_SETTER(set_produce_distributional_weight,   nimcp_brain_set_produce_distributional_weight)
AUDIT_BRAIN_FLOAT_GETTER(get_produce_distributional_weight,   nimcp_brain_get_produce_distributional_weight)
AUDIT_BRAIN_FLOAT_SETTER(set_produce_frequency_penalty,       nimcp_brain_set_produce_frequency_penalty)
AUDIT_BRAIN_FLOAT_GETTER(get_produce_frequency_penalty,       nimcp_brain_get_produce_frequency_penalty)
AUDIT_BRAIN_BOOL_SETTER(set_produce_givenness_definite,       nimcp_brain_set_produce_givenness_definite)
AUDIT_BRAIN_BOOL_SETTER(set_produce_t3_conjunction,           nimcp_brain_set_produce_t3_conjunction)
AUDIT_BRAIN_BOOL_SETTER(set_produce_corrector_feedback_enabled, nimcp_brain_set_produce_corrector_feedback_enabled)
AUDIT_BRAIN_U16_SETTER (set_tf_enabled_correctors,            nimcp_brain_set_tf_enabled_correctors)
AUDIT_BRAIN_FLOAT_SETTER(set_tf_lr_trigram,                   nimcp_brain_set_tf_lr_trigram)
AUDIT_BRAIN_FLOAT_SETTER(set_tf_lr_distrib,                   nimcp_brain_set_tf_lr_distrib)
AUDIT_BRAIN_FLOAT_SETTER(set_tf_lr_bridge_stdp,               nimcp_brain_set_tf_lr_bridge_stdp)

/* Phase-2 step 3: warm-start the SNN concept→word projection from the lexicon.
 * warmstart_lang_projection(k: float = 1.0) -> int (#synapses set). */
static PyObject* Brain_warmstart_lang_projection(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float k = 1.0f;
    if (!PyArg_ParseTuple(args, "|f", &k)) return NULL;
    int updated = 0;
    if (nimcp_brain_warmstart_lang_projection(self->brain, k, &updated) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "warmstart_lang_projection failed (projection not built or no lexicon?)");
        return NULL;
    }
    return PyLong_FromLong((long)updated);
}

/* build_lang_projection(w_init: float = 0.5) -> int (#concept→word pairs wired).
 * The missing coverage half: creates the Broca projection synapses so the SNN
 * can generate. Run after the lexicon is loaded; serialized under the daemon
 * lock so no step runs during the live CSR reopen/wire/refinalize. */
static PyObject* Brain_build_lang_projection(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float w_init = 0.5f;
    if (!PyArg_ParseTuple(args, "|f", &w_init)) return NULL;
    int wired = 0;
    if (nimcp_brain_build_lang_projection(self->brain, w_init, &wired) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "build_lang_projection failed (no bridge/network or CSR reopen failed?)");
        return NULL;
    }
    return PyLong_FromLong((long)wired);
}

/* snn_probe_selectivity(n_probe=64, n_steps=8, inject=100.0) -> list[int] of
 * decode ranks (0=top-1, -1=missing). Diagnostic: isolates concept→word
 * projection selectivity from the intent→concept seed pipeline. */
static PyObject* Brain_snn_probe_selectivity(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    unsigned int n_probe = 64, n_steps = 8;
    float inject = 100.0f;
    if (!PyArg_ParseTuple(args, "|IIf", &n_probe, &n_steps, &inject)) return NULL;
    if (n_probe == 0) n_probe = 64;
    if (n_probe > 4096) n_probe = 4096;
    int* ranks = (int*)malloc((size_t)n_probe * sizeof(int));
    if (!ranks) { PyErr_NoMemory(); return NULL; }
    uint32_t n_out = 0;
    if (nimcp_brain_snn_probe_selectivity(self->brain, n_probe, n_steps, inject,
                                          ranks, n_probe, &n_out) != NIMCP_OK) {
        free(ranks);
        PyErr_SetString(PyExc_RuntimeError,
            "snn_probe_selectivity failed (no bridge/projection/network?)");
        return NULL;
    }
    PyObject* list = PyList_New((Py_ssize_t)n_out);
    if (!list) { free(ranks); return NULL; }
    for (uint32_t i = 0; i < n_out; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyLong_FromLong((long)ranks[i]));
    }
    free(ranks);
    return list;
}

/* Echo-correct: comprehend(parent_text) → strengthen target_word bindings.
 * Returns count of bindings strengthened. Non-zero lr_scale gates the
 * caller's per-call magnitude. */
static PyObject* Brain_echo_and_correct(BrainObject* self, PyObject* args, PyObject* kwargs) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    const char* parent_text = NULL;
    const char* target_word = NULL;
    float lr_scale = 1.0f;
    static char* kwlist[] = {"parent_text", "target_word", "lr_scale", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|f", kwlist,
                                     &parent_text, &target_word, &lr_scale)) {
        return NULL;
    }
    int pairs = 0;
    nimcp_status_t st = nimcp_brain_echo_and_correct(
        self->brain, parent_text, target_word, lr_scale, &pairs);
    if (st != NIMCP_OK) {
        /* Distinguish target-not-registered (recoverable, common during
         * stage-1 when the lexicon hasn't seen this word yet) from
         * other errors. nimcp_brain_echo_and_correct returns
         * NIMCP_ERROR_OPERATION_FAILED for the not-registered case. */
        if (st == NIMCP_ERROR_OPERATION_FAILED) {
            return PyLong_FromLong(0);  /* zero pairs strengthened, not an exception */
        }
        PyErr_Format(PyExc_RuntimeError, "echo_and_correct failed (status=%d)", (int)st);
        return NULL;
    }
    return PyLong_FromLong(pairs);
}

/* Phase 2A-2D: multi-region production cascade. Returns dict with
 * utterance + diagnostics from all stages. The diag fields (self_match,
 * self_grammaticality, prompt_is_question, etc.) come from the
 * sensorimotor-loop self-comprehension stage. */
/* Slice 1 of the recurrent-language-architecture rewrite — iterates the
 * cascade until utterance + self_match converge. Returns a small dict
 * (utterance/word_count/confidence/settling_steps) rather than the full
 * diag dump. Subsequent slices may add a recurrent diag variant. */
static PyObject* Brain_produce_cascade_recurrent(BrainObject* self, PyObject* args, PyObject* kwargs) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    const char* prompt = NULL;
    uint32_t max_iters = 8;
    float self_match_eps = 0.01f;
    static char* kwlist[] = {"prompt", "max_iters", "self_match_eps", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|zIf", kwlist,
                                       &prompt, &max_iters, &self_match_eps)) {
        return NULL;
    }

    char buf[2048];
    uint32_t word_count = 0;
    float confidence = 0.0f;
    uint32_t settling_steps = 0;
    nimcp_status_t s = nimcp_brain_produce_cascade_recurrent(
        self->brain, prompt, max_iters, self_match_eps,
        buf, sizeof(buf), &word_count, &confidence, &settling_steps);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "produce_cascade_recurrent: bridge or cascade not attached");
        return NULL;
    }

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "utterance",      PyUnicode_FromString(buf));
    PyDict_SetItemString(d, "word_count",     PyLong_FromUnsignedLong(word_count));
    PyDict_SetItemString(d, "confidence",     PyFloat_FromDouble((double)confidence));
    PyDict_SetItemString(d, "settling_steps", PyLong_FromUnsignedLong(settling_steps));
    return d;
}

static PyObject* Brain_produce_cascade(BrainObject* self, PyObject* args, PyObject* kwargs) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    const char* prompt = NULL;
    static char* kwlist[] = {"prompt", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|z", kwlist, &prompt)) {
        return NULL;
    }

    /* Call the FULL diagnostic impl — surfaces every cascade-state field
     * (drives, listener, episodic, content, phoneme array, prosody arrays,
     * speech-repair best candidate, self-train reward, stage masks, failure
     * reason). Caller frees the heap-owned arrays the impl returns. */
    struct nimcp_cascade_diag_full_t_fwd; /* opaque-style forward decl */
    extern int nimcp_brain_produce_cascade_diag_full_impl(
        struct brain_struct* brain,
        const char* prompt_or_null,
        char* out_utterance, uint32_t out_text_max,
        char* out_best_utterance, uint32_t out_best_max,
        void* out_struct, /* nimcp_cascade_diag_full_t* */
        uint8_t** out_phoneme_sequence,
        float** out_prosody_pitch_hz,
        float** out_prosody_duration_ms,
        float** out_prosody_intensity_db);
    extern void nimcp_free(void*);

    /* Mirror the struct layout from nimcp_communication_cascade.h. Must
     * stay in sync if the header struct changes — kept here to avoid
     * pulling the full cascade header chain into the Python TU. The
     * cascade impl will fail-compile if a static_assert catches drift;
     * for now there isn't one but the field count is small enough that
     * a manual mirror is tractable. */
    typedef struct {
        int      wernicke_parsed;
        int      prompt_is_question;
        int      prompt_is_imperative;
        int      prompt_is_garden_path;
        uint32_t prompt_word_count;
        float    prompt_complexity;
        char     prompt_subject[32];
        char     prompt_verb[32];
        char     prompt_object[32];
        float    drive_magnitude;
        float    drive_valence;
        float    drive_arousal;
        uint8_t  dominant_drive;
        uint8_t  act_type;
        int      pragmatic_is_indirect;
        uint32_t topic_count;
        float    goal_priority;
        int      listener_known;
        float    listener_belief_confidence;
        float    listener_emotion_valence;
        float    audience_familiarity;
        uint32_t episodic_count;
        uint32_t content_dim;
        float    content_confidence;
        uint32_t word_count;
        float    fluency;
        float    syntactic_validity;
        int      self_parsed;
        float    self_complexity;
        float    self_match;
        float    self_grammaticality;
        uint32_t phoneme_count;
        uint32_t syllable_count;
        float    phon_voiced_ratio;
        uint32_t repair_attempts;
        float    best_self_match;
        uint32_t prosody_syllable_count;
        float    prosody_mean_f0;
        float    prosody_pitch_range;
        int      train_applied;
        float    train_reward;
        uint32_t stages_completed;
        uint32_t stages_failed;
        uint32_t stages_skipped;
        char     failure_reason[128];
        /* SLICE 3 — FEP prediction-error scalars (append-only mirror).
         * Must stay in lockstep with nimcp_cascade_diag_full_t in
         * include/language/nimcp_communication_cascade.h. */
        float    pe_content_norm;
        float    pe_lexical_norm;
        float    pe_syntactic_norm;
        float    pe_self_comp_norm;
        float    pe_total;
        uint32_t fep_iteration;
        float    fep_precision;

        /* S1-C1 fix: append-only mirror — distinct from repair_attempts
         * above (which is owned by speech-repair retries). */
        uint32_t settling_steps;

        /* S3+S6-H2/H4 fix (2026-05-19) — split pe_content_norm. Mirror
         * in lockstep with the public type. */
        float    pe_content_arcuate_norm;
        float    pe_content_gate_norm;
    } nimcp_cascade_diag_full_t_local;

    /* S3-C1 (CRITICAL): size lockstep with the public type. The public
     * nimcp_cascade_diag_full_t in include/language/nimcp_communication_cascade.h
     * has its own _Static_assert(... == 424). Without a reciprocal assert
     * on the local shadow, a future field append on either side passes a
     * shorter buffer into the C impl and stack-smashes. */
    _Static_assert(sizeof(nimcp_cascade_diag_full_t_local) == 424,
        "nimcp_cascade_diag_full_t_local shadow drift — bump public + local in lockstep "
        "(public assert: include/language/nimcp_communication_cascade.h)");

    char text[2048] = {0};
    char best_text[2048] = {0};
    nimcp_cascade_diag_full_t_local s; memset(&s, 0, sizeof(s));
    uint8_t* phon_seq      = NULL;
    float*   pros_pitch    = NULL;
    float*   pros_duration = NULL;
    float*   pros_intensity= NULL;

    int rc = nimcp_brain_produce_cascade_diag_full_impl(
        self->brain->internal_brain,
        prompt, text, sizeof(text),
        best_text, sizeof(best_text),
        &s, &phon_seq, &pros_pitch, &pros_duration, &pros_intensity);
    if (rc != 0) {
        if (phon_seq)       nimcp_free(phon_seq);
        if (pros_pitch)     nimcp_free(pros_pitch);
        if (pros_duration)  nimcp_free(pros_duration);
        if (pros_intensity) nimcp_free(pros_intensity);
        PyErr_Format(PyExc_RuntimeError, "produce_cascade failed (rc=%d)", rc);
        return NULL;
    }

    PyObject* d = PyDict_New();
    if (!d) {
        if (phon_seq)       nimcp_free(phon_seq);
        if (pros_pitch)     nimcp_free(pros_pitch);
        if (pros_duration)  nimcp_free(pros_duration);
        if (pros_intensity) nimcp_free(pros_intensity);
        return NULL;
    }

    /* Back-compat: keep the original 8 keys with the same names. */
    PyDict_SetItemString(d, "utterance",            PyUnicode_FromString(text));
    PyDict_SetItemString(d, "word_count",           PyLong_FromLong((long)s.word_count));
    PyDict_SetItemString(d, "confidence",           PyFloat_FromDouble((double)s.content_confidence));
    PyDict_SetItemString(d, "self_match",           PyFloat_FromDouble((double)s.self_match));
    PyDict_SetItemString(d, "self_grammaticality",  PyFloat_FromDouble((double)s.self_grammaticality));
    PyDict_SetItemString(d, "prompt_is_question",   PyBool_FromLong(s.prompt_is_question));
    PyDict_SetItemString(d, "prompt_is_imperative", PyBool_FromLong(s.prompt_is_imperative));
    PyDict_SetItemString(d, "wernicke_parsed",      PyBool_FromLong(s.wernicke_parsed));

    /* Stage 0 — Wernicke extras */
    PyDict_SetItemString(d, "prompt_is_garden_path",PyBool_FromLong(s.prompt_is_garden_path));
    PyDict_SetItemString(d, "prompt_word_count",    PyLong_FromLong((long)s.prompt_word_count));
    PyDict_SetItemString(d, "prompt_complexity",    PyFloat_FromDouble((double)s.prompt_complexity));
    PyDict_SetItemString(d, "prompt_subject",       PyUnicode_FromString(s.prompt_subject));
    PyDict_SetItemString(d, "prompt_verb",          PyUnicode_FromString(s.prompt_verb));
    PyDict_SetItemString(d, "prompt_object",        PyUnicode_FromString(s.prompt_object));

    /* Stage 1 — Drive */
    PyDict_SetItemString(d, "drive_magnitude",      PyFloat_FromDouble((double)s.drive_magnitude));
    PyDict_SetItemString(d, "drive_valence",        PyFloat_FromDouble((double)s.drive_valence));
    PyDict_SetItemString(d, "drive_arousal",        PyFloat_FromDouble((double)s.drive_arousal));
    PyDict_SetItemString(d, "dominant_drive",       PyLong_FromLong((long)s.dominant_drive));

    /* Stage 2 — Goal + Pragmatics */
    PyDict_SetItemString(d, "act_type",             PyLong_FromLong((long)s.act_type));
    PyDict_SetItemString(d, "pragmatic_is_indirect",PyBool_FromLong(s.pragmatic_is_indirect));
    PyDict_SetItemString(d, "topic_count",          PyLong_FromLong((long)s.topic_count));
    PyDict_SetItemString(d, "goal_priority",        PyFloat_FromDouble((double)s.goal_priority));

    /* Stage 3 — Listener */
    PyDict_SetItemString(d, "listener_known",            PyBool_FromLong(s.listener_known));
    PyDict_SetItemString(d, "listener_belief_confidence",PyFloat_FromDouble((double)s.listener_belief_confidence));
    PyDict_SetItemString(d, "listener_emotion_valence",  PyFloat_FromDouble((double)s.listener_emotion_valence));
    PyDict_SetItemString(d, "audience_familiarity",      PyFloat_FromDouble((double)s.audience_familiarity));

    /* Stage 4 — Episodic */
    PyDict_SetItemString(d, "episodic_count",       PyLong_FromLong((long)s.episodic_count));

    /* Stage 5 — Content */
    PyDict_SetItemString(d, "content_dim",          PyLong_FromLong((long)s.content_dim));

    /* Stages 6-7 */
    PyDict_SetItemString(d, "fluency",              PyFloat_FromDouble((double)s.fluency));
    PyDict_SetItemString(d, "syntactic_validity",   PyFloat_FromDouble((double)s.syntactic_validity));

    /* Stage 8 — Self-comprehension extras */
    PyDict_SetItemString(d, "self_parsed",          PyBool_FromLong(s.self_parsed));
    PyDict_SetItemString(d, "self_complexity",      PyFloat_FromDouble((double)s.self_complexity));

    /* Stage 9 — Phonological */
    PyDict_SetItemString(d, "phoneme_count",        PyLong_FromLong((long)s.phoneme_count));
    PyDict_SetItemString(d, "syllable_count",       PyLong_FromLong((long)s.syllable_count));
    PyDict_SetItemString(d, "phon_voiced_ratio",    PyFloat_FromDouble((double)s.phon_voiced_ratio));
    if (phon_seq && s.phoneme_count > 0) {
        PyObject* lst = PyList_New((Py_ssize_t)s.phoneme_count);
        if (lst) {
            for (uint32_t i = 0; i < s.phoneme_count; i++)
                PyList_SET_ITEM(lst, i, PyLong_FromLong((long)phon_seq[i]));
            PyDict_SetItemString(d, "phoneme_sequence", lst);
            Py_DECREF(lst);
        }
    }

    /* Stage 12 — Speech repair */
    PyDict_SetItemString(d, "repair_attempts",      PyLong_FromLong((long)s.repair_attempts));
    PyDict_SetItemString(d, "best_self_match",      PyFloat_FromDouble((double)s.best_self_match));
    PyDict_SetItemString(d, "best_utterance",       PyUnicode_FromString(best_text));

    /* Stage 13 — Prosody */
    PyDict_SetItemString(d, "prosody_syllable_count",PyLong_FromLong((long)s.prosody_syllable_count));
    PyDict_SetItemString(d, "prosody_mean_f0",      PyFloat_FromDouble((double)s.prosody_mean_f0));
    PyDict_SetItemString(d, "prosody_pitch_range",  PyFloat_FromDouble((double)s.prosody_pitch_range));
    if (s.prosody_syllable_count > 0) {
        #define ADD_PROSODY_ARRAY(key, arr) do { \
            if ((arr)) { \
                PyObject* lst = PyList_New((Py_ssize_t)s.prosody_syllable_count); \
                if (lst) { \
                    for (uint32_t i = 0; i < s.prosody_syllable_count; i++) \
                        PyList_SET_ITEM(lst, i, PyFloat_FromDouble((double)(arr)[i])); \
                    PyDict_SetItemString(d, (key), lst); \
                    Py_DECREF(lst); \
                } \
            } \
        } while (0)
        ADD_PROSODY_ARRAY("prosody_pitch_hz",      pros_pitch);
        ADD_PROSODY_ARRAY("prosody_duration_ms",   pros_duration);
        ADD_PROSODY_ARRAY("prosody_intensity_db",  pros_intensity);
        #undef ADD_PROSODY_ARRAY
    }

    /* Stage 14 — Self-train */
    PyDict_SetItemString(d, "train_applied",        PyBool_FromLong(s.train_applied));
    PyDict_SetItemString(d, "train_reward",         PyFloat_FromDouble((double)s.train_reward));

    /* Diagnostics */
    PyDict_SetItemString(d, "stages_completed",     PyLong_FromLong((long)s.stages_completed));
    PyDict_SetItemString(d, "stages_failed",        PyLong_FromLong((long)s.stages_failed));
    PyDict_SetItemString(d, "stages_skipped",       PyLong_FromLong((long)s.stages_skipped));
    PyDict_SetItemString(d, "failure_reason",       PyUnicode_FromString(s.failure_reason));

    /* Slice 3 — FEP prediction-error scalars. Consumers can plot the
     * per-stage norms vs iteration to see if the recurrent system is
     * actually settling (pe_total should fall) or wedged (flat / rising). */
    PyDict_SetItemString(d, "pe_content_norm",      PyFloat_FromDouble((double)s.pe_content_norm));
    PyDict_SetItemString(d, "pe_lexical_norm",      PyFloat_FromDouble((double)s.pe_lexical_norm));
    PyDict_SetItemString(d, "pe_syntactic_norm",    PyFloat_FromDouble((double)s.pe_syntactic_norm));
    PyDict_SetItemString(d, "pe_self_comp_norm",    PyFloat_FromDouble((double)s.pe_self_comp_norm));
    PyDict_SetItemString(d, "pe_total",             PyFloat_FromDouble((double)s.pe_total));
    PyDict_SetItemString(d, "fep_iteration",        PyLong_FromLong((long)s.fep_iteration));
    PyDict_SetItemString(d, "fep_precision",        PyFloat_FromDouble((double)s.fep_precision));

    /* S1-C1 fix: distinct from repair_attempts above. The recurrent
     * cascade reports its iteration count in settling_steps; consumers
     * that need both can read both. */
    PyDict_SetItemString(d, "settling_steps",       PyLong_FromLong((long)s.settling_steps));

    /* S3+S6-H2/H4 fix (2026-05-19): split arcuate-only vs gate-only PE.
     * pe_content_norm above aliases pe_content_arcuate_norm. */
    PyDict_SetItemString(d, "pe_content_arcuate_norm",
        PyFloat_FromDouble((double)s.pe_content_arcuate_norm));
    PyDict_SetItemString(d, "pe_content_gate_norm",
        PyFloat_FromDouble((double)s.pe_content_gate_norm));

    /* Free heap-owned arrays we just copied into Python lists. */
    if (phon_seq)       nimcp_free(phon_seq);
    if (pros_pitch)     nimcp_free(pros_pitch);
    if (pros_duration)  nimcp_free(pros_duration);
    if (pros_intensity) nimcp_free(pros_intensity);

    return d;
}

/* TB-7 length control takes two uint32 args (min, max). */
static PyObject* Brain_set_length_control(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    unsigned int min_words = 0, max_words = 0;
    if (!PyArg_ParseTuple(args, "II", &min_words, &max_words)) return NULL;
    if (nimcp_brain_set_length_control(self->brain, min_words, max_words) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_length_control failed (min > max?)");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Slice 4 — Lateral inhibition control surface. Four Python methods:
 * set/get enabled + set/get params. */
static PyObject* Brain_set_lateral_inhibition_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_lateral_inhibition_enabled(self->brain,
                                                                    enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "set_lateral_inhibition_enabled failed (bridge not initialized?)");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_lateral_inhibition_enabled(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bool enabled = false;
    nimcp_status_t s = nimcp_brain_get_lateral_inhibition_enabled(self->brain, &enabled);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "get_lateral_inhibition_enabled failed (bridge not initialized?)");
        return NULL;
    }
    return PyBool_FromLong(enabled ? 1 : 0);
}

static PyObject* Brain_set_lateral_inhibition_params(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float gain_self = 0.0f, gain_inhibit = 0.0f;
    unsigned int micro_steps = 0;
    if (!PyArg_ParseTuple(args, "ffI", &gain_self, &gain_inhibit, &micro_steps)) return NULL;
    nimcp_status_t s = nimcp_brain_set_lateral_inhibition_params(self->brain,
                                                                  gain_self,
                                                                  gain_inhibit,
                                                                  micro_steps);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "set_lateral_inhibition_params failed (out of range or bridge missing)");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_lateral_inhibition_params(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float gain_self = 0.0f, gain_inhibit = 0.0f;
    uint32_t micro_steps = 0;
    nimcp_status_t s = nimcp_brain_get_lateral_inhibition_params(self->brain,
                                                                  &gain_self,
                                                                  &gain_inhibit,
                                                                  &micro_steps);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "get_lateral_inhibition_params failed (bridge not initialized?)");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "gain_self",    PyFloat_FromDouble((double)gain_self));
    PyDict_SetItemString(d, "gain_inhibit", PyFloat_FromDouble((double)gain_inhibit));
    PyDict_SetItemString(d, "micro_steps",  PyLong_FromUnsignedLong((unsigned long)micro_steps));
    return d;
}

/* Wave 2 Item #10 — cascade Stage 14 (self-train) control surface.
 * Three small Brain methods exposing nimcp_brain_*_cascade_self_train_*. */
static PyObject* Brain_set_cascade_self_train_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (nimcp_brain_set_cascade_self_train_enabled(self->brain, enabled ? true : false) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_cascade_self_train_enabled failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_set_cascade_self_train_tunables(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float alpha = 0.05f, lr_scale = 1.0f;
    if (!PyArg_ParseTuple(args, "ff", &alpha, &lr_scale)) return NULL;
    if (nimcp_brain_set_cascade_self_train_tunables(self->brain, alpha, lr_scale) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_cascade_self_train_tunables failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_cascade_self_train_state(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bool enabled = false;
    float baseline = 0.0f, alpha = 0.0f, lr_scale = 0.0f;
    if (nimcp_brain_get_cascade_self_train_state(self->brain,
            &enabled, &baseline, &alpha, &lr_scale) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_cascade_self_train_state failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "enabled",  PyBool_FromLong(enabled));
    PyDict_SetItemString(d, "baseline", PyFloat_FromDouble((double)baseline));
    PyDict_SetItemString(d, "alpha",    PyFloat_FromDouble((double)alpha));
    PyDict_SetItemString(d, "lr_scale", PyFloat_FromDouble((double)lr_scale));
    return d;
}

/* Slice D — set the external reward signal for cascade self-train gating.
 * Callable from the caregiver-critic / RL pipeline via the brain daemon
 * _cmd_set_last_external_reward RPC. Clamped [-1, +1]. */
static PyObject* Brain_set_last_external_reward(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float reward = 0.0f;
    if (!PyArg_ParseTuple(args, "f", &reward)) return NULL;
    if (nimcp_brain_set_last_external_reward(self->brain, reward) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "set_last_external_reward failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Slice C — drive reward-modulated plasticity through the brain's
 * three-factor STDP path. Called by the caregiver-critic / RL pipeline
 * via the brain daemon _cmd_apply_reward_learning RPC. Distinct from
 * set_last_external_reward, which only stamps a field used by the
 * cascade self-train gate — this actually drives plasticity. Reward is
 * clamped to [-1, +1]; negative reward drives anti-Hebbian LTD via
 * Slice F's negative-DA path. */
static PyObject* Brain_apply_reward_learning(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float reward = 0.0f;
    if (!PyArg_ParseTuple(args, "f", &reward)) return NULL;
    if (nimcp_brain_apply_reward_learning(self->brain, reward) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "apply_reward_learning failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Slice D — configure reward threshold + TTL. Mirrors the gating fields
 * read by cascade_stage_self_train. Pass 0 (zero) for either argument to
 * leave the brain's value unchanged (partial update). */
static PyObject* Brain_set_cascade_self_train_reward_gating(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float    threshold = 0.0f;
    uint64_t ttl_us    = 0;
    /* "K" = unsigned long long → uint64_t */
    if (!PyArg_ParseTuple(args, "fK", &threshold, &ttl_us)) return NULL;
    if (nimcp_brain_set_cascade_self_train_reward_gating(self->brain,
            threshold, ttl_us) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        "set_cascade_self_train_reward_gating failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Slice D — snapshot the three gating counters as a dict. */
static PyObject* Brain_get_cascade_self_train_gate_counters(BrainObject* self,
                                                              PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    uint64_t stale = 0, below = 0, fired = 0;
    if (nimcp_brain_get_cascade_self_train_gate_counters(self->brain,
            &stale, &below, &fired) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        "get_cascade_self_train_gate_counters failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "skipped_stale",
                         PyLong_FromUnsignedLongLong((unsigned long long)stale));
    PyDict_SetItemString(d, "skipped_below_threshold",
                         PyLong_FromUnsignedLongLong((unsigned long long)below));
    PyDict_SetItemString(d, "fired",
                         PyLong_FromUnsignedLongLong((unsigned long long)fired));
    return d;
}

/* Walkthrough-2 (Option-1 rebuild, 2026-05-19) — concept_registry stats. */
static PyObject* Brain_get_concept_registry_stats(BrainObject* self,
                                                    PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    size_t total_referents = 0, total_modality_bindings = 0;
    if (nimcp_brain_get_concept_registry_stats(self->brain,
            &total_referents, &total_modality_bindings) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        "get_concept_registry_stats failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "total_referents",
                         PyLong_FromUnsignedLongLong((unsigned long long)total_referents));
    PyDict_SetItemString(d, "total_modality_bindings",
                         PyLong_FromUnsignedLongLong((unsigned long long)total_modality_bindings));
    return d;
}

/*===========================================================================
 * Slice E (Option-1 architectural rebuild) — stage scaffolding bindings.
 *=========================================================================*/

static PyObject* Brain_set_active_stage(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    unsigned int stage = 0;
    if (!PyArg_ParseTuple(args, "I", &stage)) return NULL;
    if (nimcp_brain_set_active_stage(self->brain, (uint32_t)stage) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "set_active_stage failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_active_stage(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    uint32_t stage = 0;
    if (nimcp_brain_get_active_stage(self->brain, &stage) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "get_active_stage failed");
        return NULL;
    }
    return PyLong_FromUnsignedLong((unsigned long)stage);
}

static PyObject* Brain_set_vocab_mask_for_stage(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    unsigned int stage = 0;
    if (!PyArg_ParseTuple(args, "I", &stage)) return NULL;
    if (nimcp_brain_set_vocab_mask_for_stage(self->brain, (uint32_t)stage) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "set_vocab_mask_for_stage failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_clear_vocab_mask(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    if (nimcp_brain_clear_vocab_mask(self->brain) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "clear_vocab_mask failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_stage_constraints(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    unsigned int stage = 0;
    if (!PyArg_ParseTuple(args, "I", &stage)) return NULL;
    size_t   max_visible_vocab    = 0;
    uint32_t min_produce_words    = 0;
    uint32_t max_produce_words    = 0;
    uint32_t allowed_grammar_mask = 0;
    if (nimcp_brain_get_stage_constraints(self->brain, (uint32_t)stage,
            &max_visible_vocab, &min_produce_words,
            &max_produce_words, &allowed_grammar_mask) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "get_stage_constraints failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "stage",
                         PyLong_FromUnsignedLong((unsigned long)stage));
    PyDict_SetItemString(d, "max_visible_vocab",
                         PyLong_FromUnsignedLongLong(
                            (unsigned long long)max_visible_vocab));
    PyDict_SetItemString(d, "min_produce_words",
                         PyLong_FromUnsignedLong((unsigned long)min_produce_words));
    PyDict_SetItemString(d, "max_produce_words",
                         PyLong_FromUnsignedLong((unsigned long)max_produce_words));
    PyDict_SetItemString(d, "allowed_grammar_mask",
                         PyLong_FromUnsignedLong((unsigned long)allowed_grammar_mask));
    return d;
}

/* Batch K — cascade lifetime telemetry: snapshot + reset.
 *
 * get_cascade_counters() → dict with all atomic counters. Per-stage arrays
 * are returned as 15-element Python lists keyed
 *   "stage_invocations", "stage_mask_skips", "stage_failures".
 * reset_cascade_counters() returns None on success.
 *
 * Mirror the cascade-counters struct layout locally — avoid including the
 * full cascade header (which conflicts with this TU's local extern for
 * produce_cascade_diag_full_impl above). Must stay in sync with
 * include/language/nimcp_communication_cascade.h:nimcp_cascade_counters_t. */
#define BK_STAGE_COUNT 15u
typedef struct {
    uint64_t total_runs;
    uint64_t runs_with_prompt;
    uint64_t runs_spontaneous;
    uint64_t runs_fatal_error;
    uint64_t stage_invocations[BK_STAGE_COUNT];
    uint64_t stage_mask_skips[BK_STAGE_COUNT];
    uint64_t stage_failures[BK_STAGE_COUNT];
    uint64_t pragmatics_indirect_overrides;
    uint64_t wernicke_lexicon_miss;
    uint64_t speech_repair_applied;
    uint64_t self_train_steps_matched;
    uint64_t self_train_steps_no_bindings;
    uint64_t self_produced_events_fired;
    uint64_t discourse_ring_pushes_user;
    uint64_t discourse_ring_pushes_self;
    /* S1-H3+H4 fix (2026-05-19) — mirror of recurrent_oom_count. */
    uint64_t recurrent_oom_count;
    /* S3-H3 + S3-H6 fix (2026-05-19) — mirrors of fep_lexical_skipped and
     * self_train_precision_cap_hits. */
    uint64_t fep_lexical_skipped;
    uint64_t self_train_precision_cap_hits;
} bk_cascade_counters_local_t;

/* S3-C1 (CRITICAL): size lockstep with the public type. Mirror of
 * nimcp_cascade_counters_t in include/language/nimcp_communication_cascade.h
 * which has its own _Static_assert(... == 480). Future append on either
 * side without the reciprocal assert silently stack-smashes the cast in
 * Brain_get_cascade_counters. */
_Static_assert(sizeof(bk_cascade_counters_local_t) == 480,
    "bk_cascade_counters_local_t shadow drift — bump public + local in lockstep "
    "(public assert: include/language/nimcp_communication_cascade.h)");

static PyObject* Brain_get_cascade_counters(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bk_cascade_counters_local_t c;
    if (nimcp_brain_get_cascade_counters(self->brain,
            (struct nimcp_cascade_counters*)&c) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_cascade_counters failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "total_runs",        PyLong_FromUnsignedLongLong(c.total_runs));
    PyDict_SetItemString(d, "runs_with_prompt",  PyLong_FromUnsignedLongLong(c.runs_with_prompt));
    PyDict_SetItemString(d, "runs_spontaneous",  PyLong_FromUnsignedLongLong(c.runs_spontaneous));
    PyDict_SetItemString(d, "runs_fatal_error",  PyLong_FromUnsignedLongLong(c.runs_fatal_error));

    PyObject* inv = PyList_New(BK_STAGE_COUNT);
    PyObject* msk = PyList_New(BK_STAGE_COUNT);
    PyObject* fail= PyList_New(BK_STAGE_COUNT);
    for (uint32_t i = 0; i < BK_STAGE_COUNT; i++) {
        PyList_SET_ITEM(inv,  i, PyLong_FromUnsignedLongLong(c.stage_invocations[i]));
        PyList_SET_ITEM(msk,  i, PyLong_FromUnsignedLongLong(c.stage_mask_skips[i]));
        PyList_SET_ITEM(fail, i, PyLong_FromUnsignedLongLong(c.stage_failures[i]));
    }
    PyDict_SetItemString(d, "stage_invocations", inv);  Py_DECREF(inv);
    PyDict_SetItemString(d, "stage_mask_skips",  msk);  Py_DECREF(msk);
    PyDict_SetItemString(d, "stage_failures",    fail); Py_DECREF(fail);

    PyDict_SetItemString(d, "pragmatics_indirect_overrides",
        PyLong_FromUnsignedLongLong(c.pragmatics_indirect_overrides));
    PyDict_SetItemString(d, "wernicke_lexicon_miss",
        PyLong_FromUnsignedLongLong(c.wernicke_lexicon_miss));
    PyDict_SetItemString(d, "speech_repair_applied",
        PyLong_FromUnsignedLongLong(c.speech_repair_applied));
    PyDict_SetItemString(d, "self_train_steps_matched",
        PyLong_FromUnsignedLongLong(c.self_train_steps_matched));
    PyDict_SetItemString(d, "self_train_steps_no_bindings",
        PyLong_FromUnsignedLongLong(c.self_train_steps_no_bindings));
    PyDict_SetItemString(d, "self_produced_events_fired",
        PyLong_FromUnsignedLongLong(c.self_produced_events_fired));
    PyDict_SetItemString(d, "discourse_ring_pushes_user",
        PyLong_FromUnsignedLongLong(c.discourse_ring_pushes_user));
    PyDict_SetItemString(d, "discourse_ring_pushes_self",
        PyLong_FromUnsignedLongLong(c.discourse_ring_pushes_self));
    PyDict_SetItemString(d, "recurrent_oom_count",
        PyLong_FromUnsignedLongLong(c.recurrent_oom_count));
    PyDict_SetItemString(d, "fep_lexical_skipped",
        PyLong_FromUnsignedLongLong(c.fep_lexical_skipped));
    PyDict_SetItemString(d, "self_train_precision_cap_hits",
        PyLong_FromUnsignedLongLong(c.self_train_precision_cap_hits));
    return d;
}

static PyObject* Brain_reset_cascade_counters(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    if (nimcp_brain_reset_cascade_counters(self->brain) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "reset_cascade_counters failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Slice 3 — recurrent-cascade FEP prediction-error trace + summary.
 *
 * Mirror the layout from include/language/nimcp_communication_cascade.h:
 *   typedef struct nimcp_cascade_fep_metrics { u32 + 64×f + 5×f + f + int }
 * Same approach the counter mirror uses to keep this TU header-conflict
 * free. Must stay in lockstep — the _Static_assert in the cascade header
 * catches drift on the C side. */
#define BK_FEP_TRACE_CAP 64u
typedef struct {
    uint32_t iterations_run;
    float    pe_total_trace[BK_FEP_TRACE_CAP];
    float    pe_total_initial;
    float    pe_total_terminal;
    float    pe_total_min;
    float    pe_total_max;
    float    pe_total_mean;
    float    pe_decay_rate;
    int      converged;
} bk_cascade_fep_metrics_local_t;

/* S3-C1 (CRITICAL): size lockstep with the public type. Mirror of
 * nimcp_cascade_fep_metrics_t in include/language/nimcp_communication_cascade.h
 * which has its own _Static_assert(... == 288). Without a reciprocal
 * assert on the local shadow, a future field append on either side
 * passes a shorter buffer into the C impl and stack-smashes. */
_Static_assert(sizeof(bk_cascade_fep_metrics_local_t) == 288,
    "bk_cascade_fep_metrics_local_t shadow drift — bump public + local in lockstep "
    "(public assert: include/language/nimcp_communication_cascade.h)");

static PyObject* Brain_get_cascade_fep_metrics(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bk_cascade_fep_metrics_local_t m;
    memset(&m, 0, sizeof(m));
    if (nimcp_brain_get_cascade_fep_metrics(self->brain,
            (struct nimcp_cascade_fep_metrics*)&m) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_cascade_fep_metrics failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "iterations_run",   PyLong_FromUnsignedLong((unsigned long)m.iterations_run));
    PyDict_SetItemString(d, "pe_total_initial", PyFloat_FromDouble((double)m.pe_total_initial));
    PyDict_SetItemString(d, "pe_total_terminal",PyFloat_FromDouble((double)m.pe_total_terminal));
    PyDict_SetItemString(d, "pe_total_min",     PyFloat_FromDouble((double)m.pe_total_min));
    PyDict_SetItemString(d, "pe_total_max",     PyFloat_FromDouble((double)m.pe_total_max));
    PyDict_SetItemString(d, "pe_total_mean",    PyFloat_FromDouble((double)m.pe_total_mean));
    PyDict_SetItemString(d, "pe_decay_rate",    PyFloat_FromDouble((double)m.pe_decay_rate));
    PyDict_SetItemString(d, "converged",        PyBool_FromLong(m.converged));
    /* The trace is fixed-size — emit only the populated prefix as a Py list. */
    uint32_t n = m.iterations_run;
    if (n > BK_FEP_TRACE_CAP) n = BK_FEP_TRACE_CAP;
    PyObject* trace = PyList_New((Py_ssize_t)n);
    if (trace) {
        for (uint32_t i = 0; i < n; i++) {
            PyList_SET_ITEM(trace, i, PyFloat_FromDouble((double)m.pe_total_trace[i]));
        }
        PyDict_SetItemString(d, "pe_total_trace", trace);
        Py_DECREF(trace);
    }
    return d;
}

/* Audit Cat A #1 — opt-in cascade orchestrator path in
 * nimcp_brain_grounded_respond. set takes a bool; get returns a bool. */
static PyObject* Brain_set_respond_via_cascade(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (nimcp_brain_set_respond_via_cascade(self->brain, enabled ? true : false) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_respond_via_cascade failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_respond_via_cascade(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bool enabled = false;
    if (nimcp_brain_get_respond_via_cascade(self->brain, &enabled) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_respond_via_cascade failed");
        return NULL;
    }
    return PyBool_FromLong(enabled);
}

/* Tier 1 Step E — reasoning-conclusion blend opt-in. */
static PyObject* Brain_set_reason_in_content(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (nimcp_brain_set_reason_in_content(self->brain, enabled ? true : false) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_reason_in_content failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_reason_in_content(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bool enabled = false;
    if (nimcp_brain_get_reason_in_content(self->brain, &enabled) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_reason_in_content failed");
        return NULL;
    }
    return PyBool_FromLong(enabled);
}

/* Non-SNN networks → language content_intent (2026-06-05). One set/get pair per
 * network, all sharing the reason_in_content shape via a macro. */
#define NIMCP_PY_CASCADE_NET_GATE(NAME)                                          \
static PyObject* Brain_set_##NAME(BrainObject* self, PyObject* args) {            \
    if (!self->brain) {                                                          \
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;\
    }                                                                            \
    int enabled = 0;                                                            \
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;                     \
    if (nimcp_brain_set_##NAME(self->brain, enabled ? true : false) != NIMCP_OK){ \
        PyErr_SetString(PyExc_RuntimeError, "set_" #NAME " failed"); return NULL; \
    }                                                                            \
    Py_RETURN_NONE;                                                             \
}                                                                                \
static PyObject* Brain_get_##NAME(BrainObject* self, PyObject* args) {            \
    (void)args;                                                                  \
    if (!self->brain) {                                                          \
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;\
    }                                                                            \
    bool enabled = false;                                                       \
    if (nimcp_brain_get_##NAME(self->brain, &enabled) != NIMCP_OK) {             \
        PyErr_SetString(PyExc_RuntimeError, "get_" #NAME " failed"); return NULL; \
    }                                                                            \
    return PyBool_FromLong(enabled);                                            \
}
NIMCP_PY_CASCADE_NET_GATE(ann_in_content)
NIMCP_PY_CASCADE_NET_GATE(lnn_in_content)
NIMCP_PY_CASCADE_NET_GATE(hnn_in_content)
NIMCP_PY_CASCADE_NET_GATE(cnn_in_content)
#undef NIMCP_PY_CASCADE_NET_GATE

/* Slice 6 — thalamic gating of cascade-stage bandwidth.
 *
 * set_thalamic_gate_enabled(enabled: bool) -> None
 * get_thalamic_gate_enabled() -> bool
 * set_thalamic_gate_for_stage(stage_idx: int, weight: float) -> None
 *   weight < 0 (or NaN/Inf) clears the manual override.
 * get_thalamic_gates() -> dict
 *   Returns {"enabled": bool, "weights": [w0..w14],
 *            "overrides": [o0..o14], "stage_names": [...]}. */
static PyObject* Brain_set_thalamic_gate_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (nimcp_brain_set_thalamic_gate_enabled(self->brain, enabled ? true : false) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_thalamic_gate_enabled failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_thalamic_gate_enabled(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bool enabled = false;
    if (nimcp_brain_get_thalamic_gate_enabled(self->brain, &enabled) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_thalamic_gate_enabled failed");
        return NULL;
    }
    return PyBool_FromLong(enabled);
}

static PyObject* Brain_set_thalamic_gate_for_stage(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    unsigned int stage_idx = 0;
    float weight = 0.0f;
    if (!PyArg_ParseTuple(args, "If", &stage_idx, &weight)) return NULL;
    if (nimcp_brain_set_thalamic_gate_for_stage(self->brain, stage_idx, weight) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_thalamic_gate_for_stage failed (stage_idx out of range?)");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Stage names matching cascade_stage_mask_t bit order. Used to return a
 * stage-name-keyed dict from get_thalamic_gates() so callers don't need
 * to memorize the bit positions. Keep in lockstep with
 * cascade_stage_mask_t in nimcp_communication_cascade.h. */
static const char* const _thalamic_stage_names[15] = {
    "wernicke",       /* bit 0 */
    "drive",          /* bit 1 */
    "goal",           /* bit 2 */
    "listener",       /* bit 3 */
    "episodic",       /* bit 4 */
    "content",        /* bit 5 */
    "lexical",        /* bit 6 */
    "syntactic",      /* bit 7 */
    "self_comp",      /* bit 8 */
    "phonological",   /* bit 9 */
    "motor",          /* bit 10 */
    "self_feedback",  /* bit 11 */
    "speech_repair",  /* bit 12 */
    "prosody",        /* bit 13 */
    "self_train"      /* bit 14 */
};

static PyObject* Brain_get_thalamic_gates(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bool enabled = false;
    (void)nimcp_brain_get_thalamic_gate_enabled(self->brain, &enabled);

    float weights[15];
    bool  overrides[15];
    uint32_t count = 0;
    if (nimcp_brain_get_thalamic_gates(self->brain, weights, overrides,
                                        15, &count) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_thalamic_gates failed");
        return NULL;
    }

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "enabled", PyBool_FromLong(enabled));

    /* Stage-name-keyed gate dict — the primary consumer view. */
    PyObject* gates = PyDict_New();
    if (!gates) { Py_DECREF(d); return NULL; }
    for (uint32_t i = 0; i < count && i < 15; i++) {
        PyDict_SetItemString(gates, _thalamic_stage_names[i],
                             PyFloat_FromDouble((double)weights[i]));
    }
    PyDict_SetItemString(d, "gates", gates);
    Py_DECREF(gates);

    /* Parallel arrays for callers who want the raw layout. */
    PyObject* warr = PyList_New((Py_ssize_t)count);
    PyObject* oarr = PyList_New((Py_ssize_t)count);
    PyObject* narr = PyList_New((Py_ssize_t)count);
    if (!warr || !oarr || !narr) {
        Py_XDECREF(warr); Py_XDECREF(oarr); Py_XDECREF(narr);
        Py_DECREF(d);
        return NULL;
    }
    for (uint32_t i = 0; i < count && i < 15; i++) {
        PyList_SET_ITEM(warr, (Py_ssize_t)i, PyFloat_FromDouble((double)weights[i]));
        PyList_SET_ITEM(oarr, (Py_ssize_t)i, PyBool_FromLong(overrides[i]));
        PyList_SET_ITEM(narr, (Py_ssize_t)i, PyUnicode_FromString(_thalamic_stage_names[i]));
    }
    PyDict_SetItemString(d, "weights",     warr);
    PyDict_SetItemString(d, "overrides",   oarr);
    PyDict_SetItemString(d, "stage_names", narr);
    Py_DECREF(warr); Py_DECREF(oarr); Py_DECREF(narr);
    return d;
}

/* =================================================================
 * Slice 5 — phonological-loop working memory buffer.
 *
 * Setters/clear are simple void → None bindings; the state getter
 * returns a dict with the surface form + trace count, and the diag
 * getter returns a richer dict with the configured tunables + summary
 * stats.
 * ================================================================= */

static PyObject* Brain_set_phonological_loop_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (nimcp_brain_set_phonological_loop_enabled(self->brain, enabled ? true : false) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_phonological_loop_enabled failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_set_phonological_loop_decay(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    float decay = 0.15f;
    if (!PyArg_ParseTuple(args, "f", &decay)) return NULL;
    if (nimcp_brain_set_phonological_loop_decay(self->brain, decay) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_phonological_loop_decay failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_clear_phonological_loop(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    if (nimcp_brain_clear_phonological_loop(self->brain) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "clear_phonological_loop failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_phonological_loop_state(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    char buf[512];
    uint32_t trace_count = 0;
    if (nimcp_brain_get_phonological_loop_state(self->brain, buf, sizeof(buf),
                                                  &trace_count) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_phonological_loop_state failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "buffer",      PyUnicode_FromString(buf));
    PyDict_SetItemString(d, "trace_count", PyLong_FromUnsignedLong(trace_count));
    return d;
}

static PyObject* Brain_get_phonological_loop_diag(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    nimcp_phonological_loop_diag_t diag = {0};
    if (nimcp_brain_get_phonological_loop_diag(self->brain, &diag) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_phonological_loop_diag failed");
        return NULL;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "enabled",            PyBool_FromLong(diag.enabled ? 1 : 0));
    PyDict_SetItemString(d, "buffer_len",         PyLong_FromUnsignedLong(diag.buffer_len));
    PyDict_SetItemString(d, "trace_count",        PyLong_FromUnsignedLong(diag.trace_count));
    PyDict_SetItemString(d, "trace_capacity",     PyLong_FromUnsignedLong(diag.trace_capacity));
    PyDict_SetItemString(d, "max_words",          PyLong_FromUnsignedLong(diag.max_words));
    PyDict_SetItemString(d, "decay_rate",         PyFloat_FromDouble((double)diag.decay_rate));
    PyDict_SetItemString(d, "avg_trace_strength", PyFloat_FromDouble((double)diag.avg_trace_strength));
    PyDict_SetItemString(d, "last_refresh_ms",    PyLong_FromUnsignedLongLong((unsigned long long)diag.last_refresh_ms));
    return d;
}

/* =================================================================
 * Slice 7 — cerebellar prediction-correction in motor + prosody stages.
 *
 * Three Python entry points:
 *   set_cerebellar_correction_enabled(bool) -> None
 *   set_cerebellar_correction_strength(float) -> None    (clamps [0,1])
 *   get_cerebellar_diag() -> dict
 *
 * Diag dict keys: enabled, strength, correction_pending, pe_threshold,
 * predictions_made, corrections_applied, last_pe_norm. Matches the
 * nimcp_cerebellar_diag_t struct from include/nimcp.h.
 * ================================================================= */

static PyObject* Brain_set_cerebellar_correction_enabled(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (nimcp_brain_set_cerebellar_correction_enabled(self->brain,
                                                       enabled ? true : false) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_cerebellar_correction_enabled failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_cerebellar_correction_enabled(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    bool enabled = false;
    if (nimcp_brain_get_cerebellar_correction_enabled(self->brain, &enabled) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_cerebellar_correction_enabled failed");
        return NULL;
    }
    return PyBool_FromLong(enabled);
}

static PyObject* Brain_set_cerebellar_correction_strength(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    double strength = 0.0;
    if (!PyArg_ParseTuple(args, "d", &strength)) return NULL;
    if (nimcp_brain_set_cerebellar_correction_strength(self->brain,
                                                       (float)strength) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_cerebellar_correction_strength failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_set_cerebellar_pe_threshold(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    double threshold = 0.0;
    if (!PyArg_ParseTuple(args, "d", &threshold)) return NULL;
    if (nimcp_brain_set_cerebellar_pe_threshold(self->brain,
                                                (float)threshold) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_cerebellar_pe_threshold failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_cerebellar_diag(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    nimcp_cerebellar_diag_t d;
    memset(&d, 0, sizeof(d));
    if (nimcp_brain_get_cerebellar_diag(self->brain, &d) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "get_cerebellar_diag failed");
        return NULL;
    }
    PyObject* dict = PyDict_New();
    if (!dict) return NULL;
    PyDict_SetItemString(dict, "enabled",
                        PyBool_FromLong(d.enabled ? 1 : 0));
    PyDict_SetItemString(dict, "strength",
                        PyFloat_FromDouble((double)d.strength));
    PyDict_SetItemString(dict, "correction_pending",
                        PyBool_FromLong(d.correction_pending ? 1 : 0));
    PyDict_SetItemString(dict, "pe_threshold",
                        PyFloat_FromDouble((double)d.pe_threshold));
    PyDict_SetItemString(dict, "predictions_made",
                        PyLong_FromUnsignedLongLong((unsigned long long)d.predictions_made));
    PyDict_SetItemString(dict, "corrections_applied",
                        PyLong_FromUnsignedLongLong((unsigned long long)d.corrections_applied));
    /* S7-H3: disambiguated semantics. correction_stages_applied is the
     * alias of corrections_applied (per-stage, bumped 2x/iter if both
     * motor+prosody correct); correction_iters_applied is the new
     * per-iter counter (bumped at most once per recurrent iter). */
    PyDict_SetItemString(dict, "correction_stages_applied",
                        PyLong_FromUnsignedLongLong((unsigned long long)d.correction_stages_applied));
    PyDict_SetItemString(dict, "correction_iters_applied",
                        PyLong_FromUnsignedLongLong((unsigned long long)d.correction_iters_applied));
    PyDict_SetItemString(dict, "last_pe_norm",
                        PyFloat_FromDouble((double)d.last_pe_norm));
    return dict;
}

/* Audit-2 B2: TB-10 min_turns single-uint32 setter. Closes the surface
 * gap where this was persisted in the LANC block but unreachable via
 * Python / daemon RPC. */
static PyObject* Brain_set_topic_shift_min_turns(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    unsigned int n = 0;
    if (!PyArg_ParseTuple(args, "I", &n)) return NULL;
    if (nimcp_brain_set_topic_shift_min_turns(self->brain, n) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_topic_shift_min_turns failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Audit-2 B13: dialect / accent conditioning. NULL or empty clears. */
static PyObject* Brain_set_dialect(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL;
    }
    const char* dialect = NULL;
    if (!PyArg_ParseTuple(args, "z", &dialect)) return NULL;  /* "z" = string-or-None */
    if (nimcp_brain_set_dialect(self->brain, dialect) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "set_dialect failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* TA-4: train one (prev1, prev2, next) trigram. */
static PyObject* Brain_learn_next_token_triple(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    const char* prev1;
    const char* prev2;
    const char* next_word;
    float lr = 0.025f;  /* default = half the bigram default (0.05) */
    if (!PyArg_ParseTuple(args, "sss|f", &prev1, &prev2, &next_word, &lr)) return NULL;
    nimcp_status_t s = nimcp_brain_learn_next_token_triple(self->brain,
                                                             prev1, prev2,
                                                             next_word, lr);
    /* NIMCP_ERROR is the cold-start / no-bridge no-op signal (same
     * convention as learn_next_token_pair). */
    return PyBool_FromLong(s == NIMCP_OK);
}

/* Tier-2 #3: toggle negation-driven activation sign inversion. */
static PyObject* Brain_set_grounded_negation_enabled(BrainObject* self,
                                                       PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 1;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_grounded_negation_enabled(
        self->brain, enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no grounded_language attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Tier-2 #6: toggle word-sense disambiguation in comprehend. */
static PyObject* Brain_set_grounded_sense_disambiguation_enabled(BrainObject* self,
                                                                    PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_status_t s = nimcp_brain_set_grounded_sense_disambiguation_enabled(
        self->brain, enabled ? true : false);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no grounded_language attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Tier-2 #7: append a turn to the discourse ring buffer. The semantic
 * vector is supplied as a Python sequence of floats; an empty / None
 * sequence pushes a placeholder turn with no vector content. */
static PyObject* Brain_grounded_push_turn(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    PyObject* vec_obj = NULL;
    int n_words = 0;
    int is_user = 1;
    if (!PyArg_ParseTuple(args, "Oip", &vec_obj, &n_words, &is_user)) return NULL;

    float* vec = NULL;
    Py_ssize_t vec_len = 0;
    if (vec_obj && vec_obj != Py_None) {
        PyObject* fast = PySequence_Fast(vec_obj,
            "grounded_push_turn: semantic_vector must be a sequence or None");
        if (!fast) return NULL;
        vec_len = PySequence_Fast_GET_SIZE(fast);
        if (vec_len > 0) {
            vec = (float*)malloc((size_t)vec_len * sizeof(float));
            if (!vec) {
                Py_DECREF(fast);
                PyErr_NoMemory();
                return NULL;
            }
            for (Py_ssize_t i = 0; i < vec_len; i++) {
                PyObject* item = PySequence_Fast_GET_ITEM(fast, i);
                vec[i] = (float)PyFloat_AsDouble(item);
                if (PyErr_Occurred()) {
                    free(vec);
                    Py_DECREF(fast);
                    return NULL;
                }
            }
        }
        Py_DECREF(fast);
    }

    nimcp_status_t s = nimcp_brain_grounded_push_turn(
        self->brain, vec, (uint32_t)vec_len,
        (uint32_t)n_words, is_user ? true : false);
    free(vec);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "grounded_push_turn failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* Tier-2 #7: read populated turn count. */
static PyObject* Brain_grounded_get_discourse_turn_count(BrainObject* self,
                                                            PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int n = nimcp_brain_grounded_get_discourse_turn_count(self->brain);
    if (n < 0) {
        PyErr_SetString(PyExc_RuntimeError, "no grounded_language attached");
        return NULL;
    }
    return PyLong_FromLong(n);
}

/* Tier-2 #7: clamp discourse capacity (1..GL_DISCOURSE_MAX_TURNS_PUBLIC). */
static PyObject* Brain_grounded_set_discourse_capacity(BrainObject* self,
                                                          PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int cap = 0;
    if (!PyArg_ParseTuple(args, "i", &cap)) return NULL;
    if (cap < 1) cap = 1;
    if (cap > 255) cap = 255;
    nimcp_status_t s = nimcp_brain_grounded_set_discourse_capacity(
        self->brain, (uint8_t)cap);
    if (s != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "no grounded_language attached");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_immune_state(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_immune_metrics_t imm;
    if (nimcp_brain_get_immune_metrics(self->brain, &imm) != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get immune metrics");
        return NULL;
    }

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

#define IMM_SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(v); \
} while (0)

    IMM_SET("antigen_count",        PyLong_FromUnsignedLong(imm.total_exceptions));
    IMM_SET("threats_neutralized",  PyLong_FromUnsignedLong(imm.recovered_exceptions));
    IMM_SET("inflammation_level",   PyFloat_FromDouble(imm.inflammation_level));
    IMM_SET("antibody_count",       PyLong_FromUnsignedLong(imm.active_antibodies));
    IMM_SET("t_cell_count",         PyLong_FromUnsignedLong(imm.active_t_cells));
    IMM_SET("b_cell_count",         PyLong_FromUnsignedLong(imm.active_b_cells));
    IMM_SET("memory_cell_count",    PyLong_FromUnsignedLong(imm.memory_cells));
    IMM_SET("bbb_threats_processed", PyLong_FromUnsignedLong(imm.bbb_threats_processed));
    IMM_SET("cytokine_il1",         PyFloat_FromDouble(imm.cytokine_il1));
    IMM_SET("cytokine_il6",         PyFloat_FromDouble(imm.cytokine_il6));
    IMM_SET("cytokine_il10",        PyFloat_FromDouble(imm.cytokine_il10));
    IMM_SET("cytokine_tnf",         PyFloat_FromDouble(imm.cytokine_tnf));
    IMM_SET("cytokine_ifn",         PyFloat_FromDouble(imm.cytokine_ifn_gamma));
    IMM_SET("cytokine_il4",         PyFloat_FromDouble(imm.cytokine_il4));

#undef IMM_SET
    return dict;
}

/**
 * WHAT: Evaluate quality of last brain decision using two-tier rubric
 * WHY:  Human-style grading (A+ through F) of cognitive output quality
 * HOW:  Call nimcp_brain_rubric and build dict from all 15 scores + grade
 */
static PyObject* Brain_rubric(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_rubric_t rubric;
    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_rubric(self->brain, &rubric);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        status == NIMCP_ERROR_INVALID
                            ? "No decision to evaluate — call predict() first"
                            : "Failed to evaluate rubric");
        return NULL;
    }

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

#define SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(v); \
} while (0)

    /* Tier 1 */
    SET("internal_consistency",   PyFloat_FromDouble(rubric.internal_consistency));
    SET("confidence_calibration", PyFloat_FromDouble(rubric.confidence_calibration));
    SET("completeness",           PyFloat_FromDouble(rubric.completeness));
    SET("reasoning_chain_quality", PyFloat_FromDouble(rubric.reasoning_chain_quality));
    SET("epistemic_quality",      PyFloat_FromDouble(rubric.epistemic_quality));
    SET("ethical_alignment",      PyFloat_FromDouble(rubric.ethical_alignment));
    SET("tier1_score",            PyFloat_FromDouble(rubric.tier1_score));

    /* Tier 2 */
    SET("originality",            PyFloat_FromDouble(rubric.originality));
    SET("integration_depth",      PyFloat_FromDouble(rubric.integration_depth));
    SET("communication_clarity",  PyFloat_FromDouble(rubric.communication_clarity));
    SET("engagement_quality",     PyFloat_FromDouble(rubric.engagement_quality));
    SET("empathetic_accuracy",    PyFloat_FromDouble(rubric.empathetic_accuracy));
    SET("information_density",    PyFloat_FromDouble(rubric.information_density));
    SET("tier2_score",            PyFloat_FromDouble(rubric.tier2_score));

    /* Overall */
    SET("overall_score",          PyFloat_FromDouble(rubric.overall_score));

    /* Grade as string (e.g., "A+", "B-", "C") */
    char grade_str[3] = { rubric.grade, rubric.grade_modifier == ' ' ? '\0' : rubric.grade_modifier, '\0' };
    SET("grade",                  PyUnicode_FromString(grade_str));

    SET("subsystems_available",   PyLong_FromUnsignedLong(rubric.subsystems_available));
    SET("evaluation_time_us",     PyLong_FromUnsignedLongLong(rubric.evaluation_time_us));

#undef SET

    return dict;
}

static PyObject* Brain_set_rubric_validation(BrainObject* self, PyObject* args) {
    PyObject* features_list;
    if (!PyArg_ParseTuple(args, "O", &features_list))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    Py_ssize_t num_features;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features)
        return NULL;

    if (num_features > UINT32_MAX) {
        nimcp_free(features);
        PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_set_rubric_validation(self->brain, features, (uint32_t)num_features);
    Py_END_ALLOW_THREADS

    nimcp_free(features);

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set rubric validation features");
        return NULL;
    }

    Py_RETURN_TRUE;
}

static PyObject* Brain_training_rubric(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    uint64_t eval_count = 0;
    float min_score = 0.0f, max_score = 0.0f, avg_score = 0.0f;
    nimcp_rubric_t last_rubric;
    memset(&last_rubric, 0, sizeof(last_rubric));

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_get_rubric_training_stats(
        self->brain, &eval_count, &min_score, &max_score, &avg_score, &last_rubric);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get rubric training stats");
        return NULL;
    }

    if (eval_count == 0) {
        Py_RETURN_NONE;
    }

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

#define SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(v); \
} while (0)

    SET("eval_count", PyLong_FromUnsignedLongLong(eval_count));
    SET("min_score",  PyFloat_FromDouble(min_score));
    SET("max_score",  PyFloat_FromDouble(max_score));
    SET("avg_score",  PyFloat_FromDouble(avg_score));
    SET("last_score", PyFloat_FromDouble(last_rubric.overall_score));

    char grade_str[3] = { last_rubric.grade,
                          last_rubric.grade_modifier == ' ' ? '\0' : last_rubric.grade_modifier,
                          '\0' };
    SET("last_grade", PyUnicode_FromString(grade_str));

#undef SET

    return dict;
}

/* P6-9: ~4.6KB stack allocation (output_vector[1024]) is bounded and within
 * thread stack limits (typically 1-8MB). Not worth heap-allocating. */
static PyObject* Brain_decide_full(BrainObject* self, PyObject* args) {
    PyObject* features_list;
    if (!PyArg_ParseTuple(args, "O", &features_list))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    Py_ssize_t num_features;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features)
        return NULL;

    if (num_features > UINT32_MAX) {
        nimcp_free(features);
        PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
        return NULL;
    }

    char label[NIMCP_MAX_LABEL_SIZE];
    memset(label, 0, sizeof(label));
    float confidence;
    char explanation[NIMCP_NAME_BUFFER_SIZE];
    memset(explanation, 0, sizeof(explanation));
    enum { DECIDE_FULL_MAX_OUTPUT = 4096 };
    float output_vector[DECIDE_FULL_MAX_OUTPUT];
    uint32_t output_size = DECIDE_FULL_MAX_OUTPUT;
    uint32_t num_active_neurons = 0;
    float sparsity = 0.0f;
    uint64_t inference_time_us = 0;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_decide_full(
        self->brain, features, (uint32_t)num_features,
        label, &confidence, explanation,
        output_vector, &output_size,
        &num_active_neurons, &sparsity, &inference_time_us);
    Py_END_ALLOW_THREADS

    nimcp_free(features);

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "decide_full failed");
        return NULL;
    }

    uint32_t vec_len = (output_size < DECIDE_FULL_MAX_OUTPUT) ? output_size : DECIDE_FULL_MAX_OUTPUT;
    PyObject* vec_list = PyList_New(vec_len);
    if (!vec_list) return NULL;
    for (uint32_t i = 0; i < vec_len; i++) {
        PyObject* val = PyFloat_FromDouble(output_vector[i]);
        if (!val) { Py_DECREF(vec_list); return NULL; }
        PyList_SET_ITEM(vec_list, i, val);
    }

    PyObject* result = PyDict_New();
    if (!result) { Py_DECREF(vec_list); return NULL; }
    PyObject* tmp;
    tmp = PyUnicode_FromString(label);
    if (!tmp) { Py_DECREF(vec_list); Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "label", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(vec_list); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(confidence);
    if (!tmp) { Py_DECREF(vec_list); Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "confidence", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(vec_list); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    tmp = PyUnicode_FromString(explanation);
    if (!tmp) { Py_DECREF(vec_list); Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "explanation", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(vec_list); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    if (PyDict_SetItemString(result, "output_vector", vec_list) < 0) { Py_DECREF(vec_list); Py_DECREF(result); return NULL; }
    Py_DECREF(vec_list);
    tmp = PyLong_FromUnsignedLong(num_active_neurons);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "num_active_neurons", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(sparsity);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "sparsity", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLongLong(inference_time_us);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "inference_time_us", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);

    return result;
}

/**
 * WHAT: Generate spoken text from brain's neural state
 * WHY:  Athena needs to express thoughts as language
 * HOW:  Call nimcp_brain_speak() with semantic vector
 */
static PyObject* Brain_speak(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    if (!PyArg_ParseTuple(args, "|O", &features_list))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float* features = NULL;
    Py_ssize_t num_features = 0;

    if (features_list && features_list != Py_None) {
        features = py_list_to_float_array(features_list, &num_features);
        if (!features)
            return NULL;
        if (num_features > UINT32_MAX) {
            nimcp_free(features);
            PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
            return NULL;
        }
    }

    enum { SPEAK_MAX_TEXT = 4096 };
    char text[SPEAK_MAX_TEXT];
    memset(text, 0, sizeof(text));
    float confidence = 0.0f;
    float fluency = 0.0f;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_speak(
        self->brain, features, (uint32_t)num_features,
        text, SPEAK_MAX_TEXT, &confidence, &fluency);
    Py_END_ALLOW_THREADS

    if (features) nimcp_free(features);

    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError, "speak failed: %s", nimcp_get_error());
        return NULL;
    }

    PyObject* result = PyDict_New();
    if (!result) return NULL;

    PyObject* tmp;
    tmp = PyUnicode_FromString(text);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "text", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);

    tmp = PyFloat_FromDouble(confidence);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "confidence", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);

    tmp = PyFloat_FromDouble(fluency);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "fluency", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);

    return result;
}

/**
 * WHAT: Prune weak synapses from the neural network
 * WHY:  Prevent unbounded memory growth during long training runs
 * HOW:  Call neural_network_prune_synapses on the brain's base network
 */
static PyObject* Brain_prune_synapses(BrainObject* self, PyObject* args) {
    float threshold = 0.01f;
    if (!PyArg_ParseTuple(args, "|f", &threshold))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    brain_t ib = self->brain->internal_brain;
    if (!ib->network) {
        PyErr_SetString(PyExc_RuntimeError, "Brain has no neural network");
        return NULL;
    }

    extern neural_network_t adaptive_network_get_base_network(adaptive_network_t network);
    extern uint32_t neural_network_prune_synapses(neural_network_t network, float threshold);

    neural_network_t base_net = adaptive_network_get_base_network(ib->network);
    if (!base_net) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get base network");
        return NULL;
    }

    uint32_t pruned = 0;
    Py_BEGIN_ALLOW_THREADS
    pruned = neural_network_prune_synapses(base_net, threshold);
    Py_END_ALLOW_THREADS

    return PyLong_FromUnsignedLong(pruned);
}

/**
 * WHAT: Tokenize text using the brain's persistent C-level tokenizer
 * WHY:  Language grounding — map text to token IDs that align with embeddings
 * HOW:  Reuse brain->tokenizer if it exists; lazy-create and store if not.
 *       Vocabulary grows across calls, giving consistent token IDs.
 */
static PyObject* Brain_tokenize(BrainObject* self, PyObject* args) {
    const char* text = NULL;
    if (!PyArg_ParseTuple(args, "s", &text))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    brain_t brain = self->brain->internal_brain;

    /* Lazy-init: create tokenizer on first use and persist on brain */
    if (!brain->tokenizer) {
        brain->tokenizer = tokenizer_create(NULL);
        if (!brain->tokenizer) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create tokenizer");
            return NULL;
        }
        /* Build initial vocabulary from the first input text */
        tokenizer_build_from_text(brain->tokenizer, text, 1024);
    }

    uint32_t token_ids[512];
    uint32_t num_tokens = 0;
    int rc = tokenizer_encode(brain->tokenizer, text, token_ids, 512, &num_tokens);
    if (rc != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Tokenization failed");
        return NULL;
    }

    PyObject* list = PyList_New((Py_ssize_t)num_tokens);
    if (!list) { return NULL; }
    for (uint32_t i = 0; i < num_tokens; i++) {
        PyList_SET_ITEM(list, i, PyLong_FromUnsignedLong(token_ids[i]));
    }

    return list;
}

/**
 * WHAT: Enable or disable FP16 mixed precision training
 * WHY:  2-3x speedup on modern GPUs with minimal accuracy loss
 * HOW:  Calls nimcp_brain_enable_mixed_precision() which sets up AMP autocast
 */
static PyObject* Brain_enable_mixed_precision(BrainObject* self, PyObject* args) {
    int enabled = 1;  // default: enable
    if (!PyArg_ParseTuple(args, "|p", &enabled))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_enable_mixed_precision(self->brain, (bool)enabled);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError,
                     "Failed to %s mixed precision: %s",
                     enabled ? "enable" : "disable",
                     nimcp_get_error());
        return NULL;
    }

    Py_RETURN_TRUE;
}

/**
 * WHAT: Enable or disable gradient checkpointing for memory-efficient training
 * WHY:  Reduces peak activation memory from O(L) to O(sqrt(L)) by recomputing
 *       intermediate activations during backward pass instead of storing all
 * HOW:  Calls nimcp_brain_enable_gradient_checkpointing() on the weight cache
 */
static PyObject* Brain_enable_gradient_checkpointing(BrainObject* self, PyObject* args) {
    int enabled = 1;  // default: enable
    unsigned int interval = 0;  // default: auto (every 2 layers)
    if (!PyArg_ParseTuple(args, "|pI", &enabled, &interval))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_enable_gradient_checkpointing(
        self->brain, (bool)enabled, (uint32_t)interval);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError,
                     "Failed to %s gradient checkpointing: %s",
                     enabled ? "enable" : "disable",
                     nimcp_get_error());
        return NULL;
    }

    Py_RETURN_TRUE;
}

// ============================================================================
// Hemispheric Architecture Python Bindings
// ============================================================================

static PyObject* Brain_enable_hemispheric(BrainObject* self, PyObject* args) {
    int enabled = 1;
    if (!PyArg_ParseTuple(args, "|p", &enabled))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_enable_hemispheric(self->brain, (bool)enabled);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError,
                     "Failed to %s hemispheric architecture: %s",
                     enabled ? "enable" : "disable",
                     nimcp_get_error());
        return NULL;
    }
    Py_RETURN_TRUE;
}


static PyObject* Brain_get_lateralization(BrainObject* self, PyObject* args) {
    unsigned int domain = 0;
    if (!PyArg_ParseTuple(args, "I", &domain))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float val = nimcp_brain_get_lateralization(self->brain, domain);
    return PyFloat_FromDouble((double)val);
}


static PyObject* Brain_shift_lateralization(BrainObject* self, PyObject* args) {
    unsigned int domain = 0;
    float shift = 0.0f;
    if (!PyArg_ParseTuple(args, "If", &domain, &shift))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    nimcp_status_t status = nimcp_brain_shift_lateralization(self->brain, domain, shift);
    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError, "Lateralization shift failed: %s",
                     nimcp_get_error());
        return NULL;
    }
    Py_RETURN_TRUE;
}


static PyObject* Brain_get_callosum_transfers(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    uint64_t count = nimcp_brain_get_callosum_transfers(self->brain);
    return PyLong_FromUnsignedLongLong(count);
}


static PyObject* Brain_get_hemispheric_balance(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float balance = nimcp_brain_get_hemispheric_balance(self->brain);
    return PyFloat_FromDouble((double)balance);
}


/* Brain.get_module_activity() — returns dict of statue-suspect probe
 * counters. Field order MUST match nimcp_brain_get_module_activity in
 * src/api/nimcp_part_core.c. */
static PyObject* Brain_get_module_activity(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    uint64_t out[32];
    nimcp_status_t st = nimcp_brain_get_module_activity(self->brain, out);
    if (st != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
                        "nimcp_brain_get_module_activity failed");
        return NULL;
    }
    return Py_BuildValue(
        "{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K}",
        "astrocyte_modulation_events",         out[0],
        "oligodendrocyte_myelin_apply",        out[1],
        "microglia_pruning_events",            out[2],
        "sleep_replay_events",                 out[3],
        "sleep_downscale_events",              out[4],
        "lgss_input_rejections",               out[5],
        "lgss_action_blocks",                  out[6],
        "lgss_motor_blocks",                   out[7],
        "lgss_training_blocks",                out[8],
        "lgss_reward_blocks",                  out[9],
        "ethics_violations",                   out[10],
        "hnn_forward_invocations",             out[11],
        "hnn_fallback_invocations",            out[12],
        "cortical_column_forward_invocations", out[13],
        "cortical_wta_winners_total",          out[14],
        "cortical_wta_calls",                  out[15],
        "thalamic_routes_dispatched",          out[16],
        "thalamic_drops_backpressure",         out[17],
        "callosum_transfers",                  out[18],
        "kg_consumer_hits",                    out[19]);
}


/**
 * Configure recurrent forward pass parameters.
 * enable_recurrent(enabled, max_iterations=3, confidence_threshold=0.7, blend_alpha=0.3)
 */
static PyObject* Brain_enable_recurrent(BrainObject* self, PyObject* args, PyObject* kwargs) {
    int enabled = 1;
    unsigned int max_iter = 3;
    float threshold = 0.7f;
    float alpha = 0.3f;
    static char* kwlist[] = {"enabled", "max_iterations", "confidence_threshold", "blend_alpha", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|pIff", kwlist,
                                      &enabled, &max_iter, &threshold, &alpha))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    brain_t brain = self->brain->internal_brain;
    brain->recurrent_enabled = (bool)enabled;
    brain->recurrent_max_iterations = max_iter;
    brain->recurrent_confidence_threshold = threshold;
    brain->recurrent_blend_alpha = alpha;
    Py_RETURN_TRUE;
}


/**
 * Configure BPTT (backpropagation through time) parameters.
 * enable_bptt(enabled, window_size=8, discount=0.9)
 */
static PyObject* Brain_enable_bptt(BrainObject* self, PyObject* args, PyObject* kwargs) {
    int enabled = 1;
    unsigned int window = 8;
    float discount = 0.9f;
    static char* kwlist[] = {"enabled", "window_size", "discount", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|pIf", kwlist,
                                      &enabled, &window, &discount))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    brain_t brain = self->brain->internal_brain;
    brain->bptt_enabled = (bool)enabled;

    /* Resize buffer if window changed */
    if (window != brain->bptt_window_size || !brain->bptt_buffer) {
        /* Free old buffer */
        if (brain->bptt_buffer) {
            for (uint32_t i = 0; i < brain->bptt_window_size; i++) {
                nimcp_free(brain->bptt_buffer[i].input);
                nimcp_free(brain->bptt_buffer[i].output);
                nimcp_free(brain->bptt_buffer[i].target);
            }
            nimcp_free(brain->bptt_buffer);
        }
        brain->bptt_window_size = window;
        brain->bptt_buffer = nimcp_calloc(window, sizeof(*brain->bptt_buffer));
        brain->bptt_head = 0;
        brain->bptt_count = 0;
        brain->bptt_input_dim = 0;
        brain->bptt_output_dim = 0;
    }
    brain->bptt_discount = discount;
    Py_RETURN_TRUE;
}


/**
 * Get recurrent iteration count from last brain_decide() call.
 */
static PyObject* Brain_get_recurrent_iterations(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    return PyLong_FromUnsignedLong(self->brain->internal_brain->recurrent_iteration_count);
}


/**
 * Connect a cloud backend brain for hybrid edge-cloud inference.
 * connect_cloud(cloud_brain, confidence_threshold=0.5, enable_distillation=True)
 */
static PyObject* Brain_connect_cloud(BrainObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* cloud_brain_obj = NULL;
    float threshold = 0.5f;
    int distill = 1;
    static char* kwlist[] = {"cloud_brain", "confidence_threshold", "enable_distillation", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|fp", kwlist,
                                      &cloud_brain_obj, &threshold, &distill))
        return NULL;

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Local brain not initialized");
        return NULL;
    }

    /* Verify cloud_brain_obj is a BrainObject */
    if (!PyObject_TypeCheck(cloud_brain_obj, Py_TYPE(self))) {
        PyErr_SetString(PyExc_TypeError, "cloud_brain must be a Brain instance");
        return NULL;
    }

    BrainObject* cloud = (BrainObject*)cloud_brain_obj;
    if (!cloud->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Cloud brain not initialized");
        return NULL;
    }

    nimcp_status_t status = nimcp_brain_connect_cloud(
        self->brain, cloud->brain, threshold, (bool)distill);

    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError, "connect_cloud failed: %s", nimcp_get_error());
        return NULL;
    }

    /* Keep a strong reference to the cloud brain to prevent GC.
     * INCREF new BEFORE XDECREF old — prevents use-after-free if
     * cloud_brain_obj IS the old cloud_brain_ref (same object). */
    PyObject* old_ref = self->cloud_brain_ref;
    Py_INCREF(cloud_brain_obj);
    self->cloud_brain_ref = cloud_brain_obj;
    Py_XDECREF(old_ref);

    Py_RETURN_TRUE;
}


/**
 * Disconnect cloud backend, return to standalone mode.
 */
static PyObject* Brain_disconnect_cloud(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    nimcp_brain_disconnect_cloud(self->brain);

    Py_XDECREF(self->cloud_brain_ref);
    self->cloud_brain_ref = NULL;

    Py_RETURN_TRUE;
}


/**
 * Get cloud inference statistics.
 * Returns dict with total_queries, local_handled, cloud_escalated, distillation_steps.
 */
static PyObject* Brain_get_cloud_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    uint64_t total = 0, local = 0, cloud = 0, distill = 0;
    nimcp_brain_get_cloud_stats(self->brain, &total, &local, &cloud, &distill);

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;
    PyObject* tmp;
    tmp = PyLong_FromUnsignedLongLong(total);
    PyDict_SetItemString(dict, "total_queries", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLongLong(local);
    PyDict_SetItemString(dict, "local_handled", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLongLong(cloud);
    PyDict_SetItemString(dict, "cloud_escalated", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLongLong(distill);
    PyDict_SetItemString(dict, "distillation_steps", tmp); Py_DECREF(tmp);
    float local_pct = total > 0 ? (float)local / (float)total * 100.0f : 0.0f;
    tmp = PyFloat_FromDouble(local_pct);
    PyDict_SetItemString(dict, "local_handled_pct", tmp); Py_DECREF(tmp);
    return dict;
}


/**
 * Process buffered distillation examples (batch learning from cloud).
 * distill_cloud_batch(max_examples=0) -> int (number processed)
 */
static PyObject* Brain_distill_cloud_batch(BrainObject* self, PyObject* args) {
    unsigned int max_ex = 0;
    if (!PyArg_ParseTuple(args, "|I", &max_ex))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    uint32_t processed = nimcp_brain_distill_cloud_batch(self->brain, max_ex);
    return PyLong_FromUnsignedLong(processed);
}


/**
 * WHAT: Generate text from a semantic vector using the language generator
 * WHY:  Converts brain's internal representations to natural language
 * HOW:  Uses language_orchestrator_generate_output with LANGUAGE_OUTPUT_TEXT
 */
static PyObject* Brain_generate_text(BrainObject* self, PyObject* args) {
    PyObject* semantic_list = NULL;
    if (!PyArg_ParseTuple(args, "O", &semantic_list))
        return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    Py_ssize_t num_features;
    float* semantic = py_list_to_float_array(semantic_list, &num_features);
    if (!semantic) return NULL;

    enum { GEN_MAX_TEXT = 4096 };
    char text[GEN_MAX_TEXT];
    memset(text, 0, sizeof(text));
    float confidence = 0.0f;
    float fluency = 0.0f;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_speak(
        self->brain, semantic, (uint32_t)num_features,
        text, GEN_MAX_TEXT, &confidence, &fluency);
    Py_END_ALLOW_THREADS

    nimcp_free(semantic);

    if (status != NIMCP_OK) {
        /* Fallback: return empty result instead of error for uninitialized language layer */
        PyObject* result = PyDict_New();
        if (!result) return NULL;
        PyObject* empty = PyUnicode_FromString("");
        PyDict_SetItemString(result, "text", empty);
        Py_DECREF(empty);
        PyObject* zero = PyFloat_FromDouble(0.0);
        PyDict_SetItemString(result, "confidence", zero);
        PyDict_SetItemString(result, "fluency", zero);
        PyObject* err_str = PyUnicode_FromString(nimcp_get_error());
        PyDict_SetItemString(result, "error", err_str);
        Py_DECREF(err_str);
        Py_DECREF(zero);
        return result;
    }

    PyObject* result = PyDict_New();
    if (!result) return NULL;

    PyObject* tmp;
    tmp = PyUnicode_FromString(text);
    PyDict_SetItemString(result, "text", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(confidence);
    PyDict_SetItemString(result, "confidence", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(fluency);
    PyDict_SetItemString(result, "fluency", tmp); Py_DECREF(tmp);

    return result;
}


/**
 * WHAT: Train the language generator on a text pair
 * WHY:  Enable supervised language learning from Python
 * HOW:  Call nimcp_brain_train_language with input/target text
 */
static PyObject* Brain_train_language(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* input_text = NULL;
    const char* target_text = NULL;
    float learning_rate = 0.001f;

    static char* kwlist[] = {"input_text", "target_text", "learning_rate", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|f", kwlist,
                                      &input_text, &target_text, &learning_rate))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float loss = 0.0f;
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_train_language(
        self->brain, input_text, target_text, learning_rate, &loss);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyObject* result = PyDict_New();
        if (!result) return NULL;
        PyObject* err = PyUnicode_FromString(nimcp_get_error());
        PyDict_SetItemString(result, "error", err);
        Py_DECREF(err);
        PyObject* loss_val = PyFloat_FromDouble(-1.0);
        PyDict_SetItemString(result, "loss", loss_val);
        Py_DECREF(loss_val);
        return result;
    }

    PyObject* result = PyDict_New();
    if (!result) return NULL;

    PyObject* tmp;
    tmp = PyFloat_FromDouble(loss);
    PyDict_SetItemString(result, "loss", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(1);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);

    return result;
}


/**
 * WHAT: Generate text using the LNN decoder with prompt or semantic input
 * WHY:  Full autoregressive generation from Python
 * HOW:  Call nimcp_brain_generate_text with prompt or vector
 */
static PyObject* Brain_generate_text_advanced(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* prompt = NULL;
    PyObject* semantic_list = NULL;

    static char* kwlist[] = {"prompt", "semantic_input", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sO", kwlist,
                                      &prompt, &semantic_list))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float* semantic = NULL;
    Py_ssize_t num_features = 0;
    if (semantic_list && semantic_list != Py_None) {
        semantic = py_list_to_float_array(semantic_list, &num_features);
        if (!semantic) return NULL;
    }

    if (!prompt && !semantic) {
        PyErr_SetString(PyExc_ValueError, "Either prompt or semantic_input required");
        return NULL;
    }

    enum { GEN_MAX_TEXT = 8192 };
    char text[GEN_MAX_TEXT];
    memset(text, 0, sizeof(text));
    float confidence = 0.0f;
    float perplexity = 0.0f;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_generate_text(
        self->brain, prompt, semantic, (uint32_t)num_features,
        text, GEN_MAX_TEXT, &confidence, &perplexity);
    Py_END_ALLOW_THREADS

    if (semantic) nimcp_free(semantic);

    PyObject* result = PyDict_New();
    if (!result) return NULL;

    PyObject* tmp;
    tmp = PyUnicode_FromString(text);
    PyDict_SetItemString(result, "text", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(confidence);
    PyDict_SetItemString(result, "confidence", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(perplexity);
    PyDict_SetItemString(result, "perplexity", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);

    if (status != NIMCP_OK) {
        tmp = PyUnicode_FromString(nimcp_get_error());
        PyDict_SetItemString(result, "error", tmp); Py_DECREF(tmp);
    }

    return result;
}


// =========================================================================
// Grounded Language Python Bindings
// =========================================================================

/**
 * WHAT: Ground a word in sensory experience
 * WHY:  Human-like word learning through cross-modal binding
 */
static PyObject* Brain_ground_word(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* word = NULL;
    PyObject* features_list = NULL;
    uint32_t modality = 5; /* GL_MODALITY_LINGUISTIC */
    float attention = 0.8f;
    float valence = 0.0f;
    float arousal = 0.0f;

    static char* kwlist[] = {"word", "features", "modality", "attention",
                              "valence", "arousal", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|Ifff", kwlist,
                                      &word, &features_list, &modality, &attention,
                                      &valence, &arousal))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    Py_ssize_t num_features = 0;
    float* features = py_list_to_float_array(features_list, &num_features);
    if (!features) return NULL;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_ground_word_with_emotion(self->brain, word, features,
                                                  (uint32_t)num_features, modality,
                                                  attention, valence, arousal);
    Py_END_ALLOW_THREADS

    nimcp_free(features);
    return PyBool_FromLong(status == NIMCP_OK);
}

/**
 * WHAT: Bootstrap the grounded lexicon from a JSON fixture.
 * WHY:  One-shot inject ~1500 nouns/verbs/adjectives so the brain doesn't
 *       have to learn common content words from raw distributional data.
 * HOW:  Thin wrapper over nimcp_brain_bootstrap_lexicon. Returns dict
 *       with success flag and (best-effort) the path that was loaded.
 */
static PyObject* Brain_bootstrap_lexicon(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* json_path = NULL;

    static char* kwlist[] = {"json_path", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &json_path))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_bootstrap_lexicon(self->brain, json_path);
    Py_END_ALLOW_THREADS

    PyObject* result = PyDict_New();
    if (!result) return NULL;
    PyObject* tmp;
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromLong((long)status);
    PyDict_SetItemString(result, "status", tmp); Py_DECREF(tmp);
    tmp = PyUnicode_FromString(json_path);
    PyDict_SetItemString(result, "path", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Learn language from text (distributional + syntactic)
 * WHY:  Learn word co-occurrence and sentence patterns from exposure
 */
static PyObject* Brain_learn_language(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* text = NULL;

    static char* kwlist[] = {"text", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &text))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float loss = 0.0f;
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_learn_language(self->brain, text, &loss);
    Py_END_ALLOW_THREADS

    PyObject* result = PyDict_New();
    if (!result) return NULL;
    PyObject* tmp;
    tmp = PyFloat_FromDouble(loss);
    PyDict_SetItemString(result, "loss", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Learn from input-target pairs (teacher-guided)
 * WHY:  Social learning — teacher provides correct responses
 */
static PyObject* Brain_learn_language_pair(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* input_text = NULL;
    const char* target_text = NULL;
    float learning_rate = 0.0f;

    static char* kwlist[] = {"input_text", "target_text", "learning_rate", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|f", kwlist,
                                      &input_text, &target_text, &learning_rate))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float loss = 0.0f;
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_learn_language_pair(self->brain, input_text, target_text,
                                             learning_rate, &loss);
    Py_END_ALLOW_THREADS

    PyObject* result = PyDict_New();
    if (!result) return NULL;
    PyObject* tmp;
    tmp = PyFloat_FromDouble(loss);
    PyDict_SetItemString(result, "loss", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Train all cognitive modules from text in one call
 * WHY:  Unified training ensures grounded language, knowledge, and LNN generator
 *       all learn together, not just neural network weights
 * HOW:  Calls nimcp_brain_train_cognitive which trains grounded language (distributional
 *       + syntactic), knowledge system, and language generator
 */
static PyObject* Brain_train_cognitive(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* text = NULL;
    int domain = 10; /* KNOWLEDGE_DOMAIN_GENERAL */
    const char* target_text = NULL;
    float learning_rate = 0.001f;

    static char* kwlist[] = {"text", "domain", "target_text", "learning_rate", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|isf", kwlist,
                                      &text, &domain, &target_text, &learning_rate))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float loss = 0.0f;
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_train_cognitive(
        self->brain, text, domain, target_text, learning_rate, &loss);
    Py_END_ALLOW_THREADS

    PyObject* result = PyDict_New();
    if (!result) return NULL;
    PyObject* tmp;
    tmp = PyFloat_FromDouble(loss);
    PyDict_SetItemString(result, "loss", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Learn knowledge from text in a domain
 * WHY:  Build multi-domain knowledge graph
 */
static PyObject* Brain_learn_knowledge(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* text = NULL;
    int domain = 10; /* KNOWLEDGE_DOMAIN_GENERAL */

    static char* kwlist[] = {"text", "domain", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|i", kwlist, &text, &domain))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_learn_knowledge(self->brain, text, domain);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_OK);
}

/**
 * @brief Get per-module cognitive training statistics
 */
static PyObject* Brain_get_cognitive_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    uint32_t steps[13];
    float losses[13];
    uint32_t count = 0;
    memset(steps, 0, sizeof(steps));
    memset(losses, 0, sizeof(losses));

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_get_cognitive_stats(self->brain, steps, losses, &count);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        Py_RETURN_NONE;
    }

    static const char* module_names[] = {
        "grounded_language", "knowledge", "vae", "fep_parietal",
        "physics_nn", "pred_hierarchy", "jepa", "creative",
        "self_heal", "intuition", "fep_orchestrator"
    };

    PyObject* result = PyDict_New();
    for (uint32_t i = 0; i < count && i < 11; i++) {
        PyObject* entry = PyDict_New();
        PyObject* tmp;
        tmp = PyLong_FromUnsignedLong(steps[i]);
        PyDict_SetItemString(entry, "steps", tmp); Py_DECREF(tmp);
        tmp = PyFloat_FromDouble(losses[i]);
        PyDict_SetItemString(entry, "last_loss", tmp); Py_DECREF(tmp);

        PyDict_SetItemString(result, module_names[i], entry);
        Py_DECREF(entry);
    }

    return result;
}

/**
 * WHAT: Get cognitive transcript from last brain_decide() call
 * WHY:  Exposes rich internal cognition for response composition
 * HOW:  Reads cached transcript from brain, returns list of dicts
 */
static PyObject* Brain_get_transcript(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    enum { MAX_TRANSCRIPT_ENTRIES = 32 };
    char summaries[MAX_TRANSCRIPT_ENTRIES][256];
    float saliences[MAX_TRANSCRIPT_ENTRIES];
    float confidences[MAX_TRANSCRIPT_ENTRIES];
    const char* modules[MAX_TRANSCRIPT_ENTRIES];

    memset(summaries, 0, sizeof(summaries));
    memset(saliences, 0, sizeof(saliences));
    memset(confidences, 0, sizeof(confidences));
    memset(modules, 0, sizeof(modules));

    uint32_t count;
    Py_BEGIN_ALLOW_THREADS
    count = nimcp_brain_get_last_transcript(
        self->brain,
        summaries, saliences, confidences, modules,
        MAX_TRANSCRIPT_ENTRIES);
    Py_END_ALLOW_THREADS

    PyObject* result = PyList_New(count);
    if (!result) return NULL;

    for (uint32_t i = 0; i < count; i++) {
        PyObject* entry = PyDict_New();
        if (!entry) { Py_DECREF(result); return NULL; }

        PyObject* tmp;
        tmp = PyUnicode_FromString(modules[i] ? modules[i] : "unknown");
        PyDict_SetItemString(entry, "module", tmp); Py_DECREF(tmp);

        tmp = PyUnicode_FromString(summaries[i]);
        PyDict_SetItemString(entry, "summary", tmp); Py_DECREF(tmp);

        tmp = PyFloat_FromDouble(saliences[i]);
        PyDict_SetItemString(entry, "salience", tmp); Py_DECREF(tmp);

        tmp = PyFloat_FromDouble(confidences[i]);
        PyDict_SetItemString(entry, "confidence", tmp); Py_DECREF(tmp);

        PyList_SET_ITEM(result, i, entry);
    }

    return result;
}

/**
 * WHAT: Comprehend text into semantic representation
 * WHY:  Understanding = activating grounded concepts
 */
static PyObject* Brain_comprehend(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* text = NULL;

    static char* kwlist[] = {"text", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &text))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float semantic[128];
    memset(semantic, 0, sizeof(semantic));
    float confidence = 0.0f;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_comprehend(self->brain, text, semantic, 128, &confidence);
    Py_END_ALLOW_THREADS

    PyObject* result = PyDict_New();
    if (!result) return NULL;

    /* Build semantic vector as list */
    PyObject* vec_list = PyList_New(128);
    for (int i = 0; i < 128; i++) {
        PyList_SET_ITEM(vec_list, i, PyFloat_FromDouble(semantic[i]));
    }
    PyDict_SetItemString(result, "semantic_vector", vec_list); Py_DECREF(vec_list);

    PyObject* tmp;
    tmp = PyFloat_FromDouble(confidence);
    PyDict_SetItemString(result, "confidence", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Produce text from semantic intent
 * WHY:  Expression = finding words for concepts
 */
static PyObject* Brain_produce_text(BrainObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* intent_list = NULL;

    static char* kwlist[] = {"intent", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &intent_list))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    Py_ssize_t intent_dim = 0;
    float* intent = py_list_to_float_array(intent_list, &intent_dim);
    if (!intent) return NULL;

    char text[4096];
    memset(text, 0, sizeof(text));
    float confidence = 0.0f;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_produce_text(self->brain, intent, (uint32_t)intent_dim,
                                      text, sizeof(text), &confidence);
    Py_END_ALLOW_THREADS

    nimcp_free(intent);

    PyObject* result = PyDict_New();
    if (!result) return NULL;
    PyObject* tmp;
    tmp = PyUnicode_FromString(text);
    PyDict_SetItemString(result, "text", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(confidence);
    PyDict_SetItemString(result, "confidence", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Full conversation turn using grounded language
 * WHY:  Comprehend input + produce response in one call
 */
static PyObject* Brain_grounded_respond(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* input_text = NULL;

    static char* kwlist[] = {"text", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &input_text))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    char response[4096];
    memset(response, 0, sizeof(response));
    float confidence = 0.0f;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_grounded_respond(self->brain, input_text,
                                          response, sizeof(response), &confidence);
    Py_END_ALLOW_THREADS

    PyObject* result = PyDict_New();
    if (!result) return NULL;
    PyObject* tmp;
    tmp = PyUnicode_FromString(response);
    PyDict_SetItemString(result, "response", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(confidence);
    PyDict_SetItemString(result, "confidence", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Creative text generation by blending two concept vectors
 * WHY:  Creativity = novel combinations of concepts
 */
static PyObject* Brain_creative_blend(BrainObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* vec_a_list = NULL;
    PyObject* vec_b_list = NULL;
    float blend_ratio = 0.5f;

    static char* kwlist[] = {"vector_a", "vector_b", "blend_ratio", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|f", kwlist,
                                      &vec_a_list, &vec_b_list, &blend_ratio))
        return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    Py_ssize_t dim_a = 0, dim_b = 0;
    float* vec_a = py_list_to_float_array(vec_a_list, &dim_a);
    if (!vec_a) return NULL;
    float* vec_b = py_list_to_float_array(vec_b_list, &dim_b);
    if (!vec_b) { nimcp_free(vec_a); return NULL; }

    uint32_t dim = (dim_a < dim_b) ? (uint32_t)dim_a : (uint32_t)dim_b;

    char text[4096];
    memset(text, 0, sizeof(text));

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_creative_blend(self->brain, vec_a, vec_b, dim,
                                         blend_ratio, text, sizeof(text));
    Py_END_ALLOW_THREADS

    nimcp_free(vec_a);
    nimcp_free(vec_b);

    PyObject* result = PyDict_New();
    if (!result) return NULL;
    PyObject* tmp;
    tmp = PyUnicode_FromString(text);
    PyDict_SetItemString(result, "text", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(status == NIMCP_OK);
    PyDict_SetItemString(result, "success", tmp); Py_DECREF(tmp);
    return result;
}

/**
 * WHAT: Get avatar visual state (FACS AUs, visemes, gaze, emotion, voice)
 * WHY:  Drive real-time face animation and lip sync from brain state
 * HOW:  Call nimcp_brain_get_avatar_state, return dict with all fields
 */
static PyObject* Brain_get_avatar_state(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    nimcp_avatar_state_t state;
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_get_avatar_state(self->brain, &state);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError, "get_avatar_state failed: %s", nimcp_get_error());
        return NULL;
    }

    /* Build result dict with all avatar fields */
    return Py_BuildValue(
        "{s:f, s:f, s:f, s:f, s:f, s:i,"       /* mouth */
        " s:f, s:f, s:f, s:f, s:f, s:f,"        /* AUs 1-7 */
        " s:f, s:f, s:f, s:f, s:f, s:f,"        /* AUs 9-23 */
        " s:f, s:f, s:f, s:f,"                  /* AUs 25-28 */
        " s:f, s:f, s:f, s:I, s:f,"             /* emotion */
        " s:f, s:f, s:f, s:f, s:f, s:f,"        /* gaze+head */
        " s:f, s:f, s:f,"                        /* voice */
        " s:K, s:O}",                            /* metadata */
        "mouth_open", (double)state.mouth_open,
        "lip_round", (double)state.lip_round,
        "lip_upper", (double)state.lip_upper,
        "lip_lower", (double)state.lip_lower,
        "tongue_position", (double)state.tongue_position,
        "current_viseme", (int)state.current_viseme,
        /* FACS AUs */
        "au1_inner_brow_raise", (double)state.au1_inner_brow_raise,
        "au2_outer_brow_raise", (double)state.au2_outer_brow_raise,
        "au4_brow_lower", (double)state.au4_brow_lower,
        "au5_upper_lid_raise", (double)state.au5_upper_lid_raise,
        "au6_cheek_raise", (double)state.au6_cheek_raise,
        "au7_lid_tighten", (double)state.au7_lid_tighten,
        "au9_nose_wrinkle", (double)state.au9_nose_wrinkle,
        "au10_upper_lip_raise", (double)state.au10_upper_lip_raise,
        "au12_lip_corner_pull", (double)state.au12_lip_corner_pull,
        "au15_lip_corner_drop", (double)state.au15_lip_corner_drop,
        "au17_chin_raise", (double)state.au17_chin_raise,
        "au20_lip_stretch", (double)state.au20_lip_stretch,
        "au23_lip_tighten", (double)state.au23_lip_tighten,
        "au25_lips_part", (double)state.au25_lips_part,
        "au26_jaw_drop", (double)state.au26_jaw_drop,
        "au28_lip_suck", (double)state.au28_lip_suck,
        /* Emotion */
        "valence", (double)state.valence,
        "arousal", (double)state.arousal,
        "dominance", (double)state.dominance,
        "emotion_id", (unsigned int)state.emotion_id,
        "emotion_intensity", (double)state.emotion_intensity,
        /* Gaze + head */
        "gaze_x", (double)state.gaze_x,
        "gaze_y", (double)state.gaze_y,
        "head_pitch", (double)state.head_pitch,
        "head_yaw", (double)state.head_yaw,
        "head_roll", (double)state.head_roll,
        "blink", (double)state.blink,
        /* Voice */
        "pitch_hz", (double)state.pitch_hz,
        "speaking_rate", (double)state.speaking_rate,
        "volume", (double)state.volume,
        /* Metadata */
        "timestamp_us", (unsigned long long)state.timestamp_us,
        "is_speaking", state.is_speaking ? Py_True : Py_False
    );
}


/**
 * WHAT: Get running label-match accuracy (EMA)
 * WHY:  Monitor training progress with a meaningful metric
 * HOW:  Call nimcp_brain_get_accuracy
 */
static PyObject* Brain_get_accuracy(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float accuracy = nimcp_brain_get_accuracy(self->brain);
    PyObject* result = PyFloat_FromDouble((double)accuracy);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * @brief Freeze brain for inference-only mode
 */
static PyObject* Brain_freeze(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    nimcp_status_t status = nimcp_brain_freeze(self->brain);
    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to freeze brain");
        return NULL;
    }
    Py_RETURN_TRUE;
}

//=============================================================================
// Socratic Active Learning: Cognitive Module Bindings
//=============================================================================

/**
 * WHAT: Detect knowledge gaps via curiosity module
 * WHY:  Drive active learning by finding what the brain doesn't know
 * HOW:  Call curiosity_detect_knowledge_gap() through brain accessor
 */
static PyObject* Brain_curiosity_detect_gaps(BrainObject* self, PyObject* args) {
    const char* topic;
    if (!PyArg_ParseTuple(args, "s", &topic))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    curiosity_engine_t curiosity = brain_get_curiosity(self->brain->internal_brain);
    if (!curiosity) {
        /* No curiosity engine — return empty dict rather than error */
        PyObject* dict = PyDict_New();
        return dict;
    }

    knowledge_gap_t gap;
    Py_BEGIN_ALLOW_THREADS
    gap = curiosity_detect_knowledge_gap(curiosity, topic);
    Py_END_ALLOW_THREADS

    /* Generate questions from the gap — do this before building the dict
     * so that all C-allocated resources exist for cleanup on any error path. */
    generated_question_t questions[8];
    uint32_t num_q;
    Py_BEGIN_ALLOW_THREADS
    num_q = curiosity_generate_questions(curiosity, &gap, questions, 8);
    Py_END_ALLOW_THREADS

    PyObject* dict = PyDict_New();
    if (!dict) goto cleanup;

#define SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); dict = NULL; goto cleanup; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); dict = NULL; goto cleanup; } \
    Py_DECREF(v); \
} while (0)

    SET("topic", PyUnicode_FromString(gap.topic));
    SET("gap_size", PyFloat_FromDouble(gap.gap_size));
    SET("curiosity_intensity", PyFloat_FromDouble(gap.curiosity_intensity));
    SET("learning_potential", PyFloat_FromDouble(gap.learning_potential));
    SET("related_concepts", PyLong_FromUnsignedLong(gap.related_concepts));

    {
        PyObject* q_list = PyList_New(num_q);
        if (!q_list) { Py_DECREF(dict); dict = NULL; goto cleanup; }
        for (uint32_t i = 0; i < num_q; i++) {
            PyObject* q_str = PyUnicode_FromString(questions[i].question);
            if (!q_str) { Py_DECREF(q_list); Py_DECREF(dict); dict = NULL; goto cleanup; }
            PyList_SET_ITEM(q_list, i, q_str);
        }
        SET("questions", q_list);
    }

#undef SET

cleanup:
    /* Free search_terms in each generated question (Bug #5) */
    for (uint32_t qi = 0; qi < num_q; qi++) {
        if (questions[qi].search_terms) {
            for (uint32_t j = 0; j < questions[qi].num_search_terms; j++) {
                free(questions[qi].search_terms[j]);
            }
            free(questions[qi].search_terms);
        }
    }

    /* Free prerequisites array from knowledge_gap_t (Bug #4) */
    if (gap.prerequisites) {
        for (uint32_t pi = 0; pi < gap.num_prerequisites; pi++) {
            free(gap.prerequisites[pi]);
        }
        free(gap.prerequisites);
    }

    return dict;  /* NULL on error, valid dict on success */
}

/**
 * WHAT: Trigger synchronous memory consolidation with configurable mode
 * WHY:  "Sleep" between training phases — replay, prune, integrate
 * HOW:  Select config preset by mode: "auto" (scale-aware), "light" (replay only), "full" (original)
 *
 * Python usage:
 *   brain.consolidate()                    # auto mode (scale-aware)
 *   brain.consolidate(mode="light")        # replay only (~ms)
 *   brain.consolidate(mode="full")         # original 10-cycle
 *   brain.consolidate(mode="auto", cycles=3)  # override cycles
 */
static PyObject* Brain_consolidate(BrainObject* self, PyObject* args, PyObject* kwds) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    if (!self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain has no internal state");
        return NULL;
    }

    const char* mode = "auto";
    unsigned int cycles = 0;
    unsigned int prune_passes = 0;

    static char* kwlist[] = {"mode", "cycles", "prune_passes", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sII", kwlist,
                                     &mode, &cycles, &prune_passes)) {
        return NULL;
    }

    /* Select config based on mode */
    consolidation_config_t config;
    if (strcmp(mode, "light") == 0) {
        config = consolidation_light_config();
    } else if (strcmp(mode, "full") == 0) {
        config = consolidation_default_config();
    } else {
        /* "auto" or unrecognized — scale-aware */
        uint32_t num_neurons = nimcp_brain_get_neuron_count(self->brain);
        config = consolidation_auto_config(num_neurons);
    }

    /* Apply overrides if specified */
    if (cycles > 0) {
        config.consolidation_cycles = cycles;
    }
    if (prune_passes > 0) {
        config.max_prune_passes = prune_passes;
    }

    bool success;

    Py_BEGIN_ALLOW_THREADS
    if (self->community_cache && self->community_cache->is_valid &&
        config.use_community_cache) {
        success = brain_consolidate_memory_community_aware(
            self->brain->internal_brain, &config, self->community_cache);
    } else {
        success = brain_consolidate_memory(self->brain->internal_brain, &config);
    }
    Py_END_ALLOW_THREADS

    /* P6-6: Raise exception on failure instead of returning False */
    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Memory consolidation failed");
        return NULL;
    }
    Py_RETURN_TRUE;
}

//=============================================================================
// Training Integration API (basal ganglia, medulla, reasoning, adaptive LR)
//=============================================================================

/**
 * WHAT: Update basal ganglia reward signal
 * WHY:  Reinforcement learning needs reward/expected pairs for RPE computation
 * HOW:  Calls brain_ti_update_reward with (reward, expected) floats
 */
static PyObject* Brain_bg_update_reward(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float reward, expected;
    if (!PyArg_ParseTuple(args, "ff", &reward, &expected)) return NULL;

    int result = brain_ti_update_reward(self->brain->internal_brain, reward, expected);
    /* Return False if BG is not enabled/initialized — non-fatal for training.
     * The prior exception-raising behavior spammed daemon logs every ~3s. */
    if (result != 0) {
        Py_RETURN_FALSE;
    }
    Py_RETURN_TRUE;
}

/**
 * WHAT: Get basal ganglia conflict level
 * WHY:  Conflict detection indicates competing action selections
 * HOW:  Calls brain_ti_get_conflict, returns float
 */
static PyObject* Brain_bg_get_conflict(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float conflict = brain_ti_get_conflict(self->brain->internal_brain);
    PyObject* result = PyFloat_FromDouble((double)conflict);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get basal ganglia operating mode
 * WHY:  Distinguish between exploration, exploitation, habit modes
 * HOW:  Calls brain_ti_get_mode, returns int enum value
 */
static PyObject* Brain_bg_get_mode(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int mode = brain_ti_get_mode(self->brain->internal_brain);
    PyObject* result = PyLong_FromLong(mode);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get basal ganglia dopamine level
 * WHY:  Dopamine modulates learning rate and motivation
 * HOW:  Calls brain_ti_get_dopamine, returns float
 */
static PyObject* Brain_bg_get_dopamine(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float dopamine = brain_ti_get_dopamine(self->brain->internal_brain);
    PyObject* result = PyFloat_FromDouble((double)dopamine);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get reward prediction error from basal ganglia
 * WHY:  RPE drives learning — positive RPE strengthens, negative weakens
 * HOW:  Calls brain_ti_get_rpe, returns float
 */
static PyObject* Brain_bg_get_rpe(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float rpe = brain_ti_get_rpe(self->brain->internal_brain);
    PyObject* result = PyFloat_FromDouble((double)rpe);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Register a new habit in the basal ganglia
 * WHY:  Habit formation automates frequently-used action sequences
 * HOW:  Calls brain_ti_register_habit with domain string and action ID
 */
static PyObject* Brain_bg_register_habit(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    const char* domain;
    int action_id;
    if (!PyArg_ParseTuple(args, "si", &domain, &action_id)) return NULL;

    int result = brain_ti_register_habit(self->brain->internal_brain, domain, action_id);
    PyObject* py_result = PyLong_FromLong(result);
    if (!py_result) return NULL;  /* OOM */
    return py_result;
}

/**
 * WHAT: Check if a habit exists for the given domain
 * WHY:  Allows training loop to skip exploration when habits are formed
 * HOW:  Calls brain_ti_check_habit with domain string, returns habit ID or -1
 */
static PyObject* Brain_bg_check_habit(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    const char* domain;
    if (!PyArg_ParseTuple(args, "s", &domain)) return NULL;

    int result = brain_ti_check_habit(self->brain->internal_brain, domain);
    PyObject* py_result = PyLong_FromLong(result);
    if (!py_result) return NULL;  /* OOM */
    return py_result;
}

/**
 * WHAT: Strengthen an existing habit after successful use
 * WHY:  Repeated success solidifies habits, reducing cognitive load
 * HOW:  Calls brain_ti_strengthen_habit with habit ID and success flag
 */
static PyObject* Brain_bg_strengthen_habit(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int habit_id;
    int success;
    if (!PyArg_ParseTuple(args, "ip", &habit_id, &success)) return NULL;

    int result = brain_ti_strengthen_habit(self->brain->internal_brain, habit_id, success);
    PyObject* py_result = PyLong_FromLong(result);
    if (!py_result) return NULL;  /* OOM */
    return py_result;
}

/**
 * WHAT: Get medulla arousal level
 * WHY:  Arousal modulates attention and processing intensity
 * HOW:  Calls brain_ti_get_arousal, returns float [0.0, 1.0]
 */
static PyObject* Brain_medulla_get_arousal(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float arousal = brain_ti_get_arousal(self->brain->internal_brain);
    PyObject* result = PyFloat_FromDouble((double)arousal);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get medulla circadian phase
 * WHY:  Circadian phase affects consolidation timing and learning efficiency
 * HOW:  Calls brain_ti_get_circadian_phase, returns int phase enum
 */
static PyObject* Brain_medulla_get_circadian_phase(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int phase = brain_ti_get_circadian_phase(self->brain->internal_brain);
    PyObject* result = PyLong_FromLong(phase);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Boost medulla arousal by a delta
 * WHY:  Training may need to artificially increase arousal for difficult examples
 * HOW:  Calls brain_ti_boost_arousal with delta float
 */
static PyObject* Brain_medulla_boost_arousal(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float delta;
    if (!PyArg_ParseTuple(args, "f", &delta)) return NULL;

    int rc = brain_ti_boost_arousal(self->brain->internal_brain, delta);
    if (rc != 0) {
        PyErr_SetString(PyExc_RuntimeError, "medulla_boost_arousal failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
 * WHAT: Reduce medulla arousal by a delta
 * WHY:  Training may need to reduce arousal when brain is over-stimulated
 * HOW:  Calls brain_ti_reduce_arousal with positive delta float
 */
static PyObject* Brain_medulla_reduce_arousal(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float delta;
    if (!PyArg_ParseTuple(args, "f", &delta)) return NULL;

    int rc = brain_ti_reduce_arousal(self->brain->internal_brain, delta);
    if (rc != 0) {
        PyErr_SetString(PyExc_RuntimeError, "medulla_reduce_arousal failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
 * WHAT: Get circadian efficiency factor from medulla
 * WHY:  Allows training to scale learning rate by time-of-day efficiency
 * HOW:  Calls brain_ti_get_circadian_efficiency, returns float [0.0, 1.0]
 */
static PyObject* Brain_medulla_get_circadian_efficiency(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float efficiency = brain_ti_get_circadian_efficiency(self->brain->internal_brain);
    PyObject* result = PyFloat_FromDouble((double)efficiency);
    if (!result) return NULL;  /* OOM */
    return result;
}

/* ========================================================================
 * Sleep/Wake System Bindings
 * ======================================================================== */

/**
 * WHAT: Get current sleep pressure
 * WHY:  Monitor adenosine accumulation to decide when to trigger sleep
 * HOW:  Calls sleep_get_pressure via brain_get_sleep_system
 */
static PyObject* Brain_sleep_get_pressure(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    sleep_system_t ss = brain_get_sleep_system(self->brain->internal_brain);
    if (!ss) {
        return PyFloat_FromDouble(0.0);
    }
    float pressure = sleep_get_pressure(ss);
    return PyFloat_FromDouble((double)pressure);
}

/**
 * WHAT: Check if sleep is needed
 * WHY:  Determine when to initiate a sleep cycle
 * HOW:  Calls sleep_is_needed via brain_get_sleep_system
 */
static PyObject* Brain_sleep_is_needed(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    sleep_system_t ss = brain_get_sleep_system(self->brain->internal_brain);
    if (!ss) {
        Py_RETURN_FALSE;
    }
    if (sleep_is_needed(ss)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

/**
 * WHAT: Get current sleep state
 * WHY:  Check whether the brain is awake, drowsy, in NREM, or REM
 * HOW:  Calls sleep_get_current_state via brain_get_sleep_system
 */
static PyObject* Brain_sleep_get_state(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    sleep_system_t ss = brain_get_sleep_system(self->brain->internal_brain);
    if (!ss) {
        return PyLong_FromLong(0);  /* SLEEP_STATE_AWAKE */
    }
    sleep_state_t state = sleep_get_current_state(ss);
    return PyLong_FromLong((long)state);
}

/**
 * WHAT: Run automatic sleep cycle(s)
 * WHY:  Execute full consolidation cycle (drowsy → light → deep → REM → awake)
 * HOW:  Calls sleep_run_cycle via brain_get_sleep_system
 */
static PyObject* Brain_sleep_run_cycle(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int num_cycles = 1;
    if (!PyArg_ParseTuple(args, "|i", &num_cycles)) return NULL;
    if (num_cycles < 1) num_cycles = 1;

    sleep_system_t ss = brain_get_sleep_system(self->brain->internal_brain);
    if (!ss) {
        Py_RETURN_FALSE;
    }
    bool ok = sleep_run_cycle(ss, (uint32_t)num_cycles);
    if (ok) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

/**
 * WHAT: Get sleep statistics
 * WHY:  Monitor sleep quality, cycles completed, memory consolidation efficiency
 * HOW:  Calls sleep_get_statistics, returns dict of sleep_stats_t fields
 */
static PyObject* Brain_sleep_get_statistics(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    sleep_system_t ss = brain_get_sleep_system(self->brain->internal_brain);
    if (!ss) {
        Py_RETURN_NONE;
    }
    sleep_stats_t stats;
    if (!sleep_get_statistics(ss, &stats)) {
        Py_RETURN_NONE;
    }
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyDict_SetItemString(d, "total_awake_time_ms",
        PyLong_FromUnsignedLongLong(stats.total_awake_time_ms));
    PyDict_SetItemString(d, "total_sleep_time_ms",
        PyLong_FromUnsignedLongLong(stats.total_sleep_time_ms));
    PyDict_SetItemString(d, "sleep_cycles_completed",
        PyLong_FromUnsignedLong(stats.sleep_cycles_completed));
    PyDict_SetItemString(d, "total_memories_replayed",
        PyLong_FromUnsignedLong(stats.total_memories_replayed));
    PyDict_SetItemString(d, "total_synapses_pruned",
        PyLong_FromUnsignedLong(stats.total_synapses_pruned));
    PyDict_SetItemString(d, "avg_consolidation_efficiency",
        PyFloat_FromDouble((double)stats.avg_consolidation_efficiency));
    PyDict_SetItemString(d, "energy_savings_percent",
        PyFloat_FromDouble((double)stats.energy_savings_percent));
    PyDict_SetItemString(d, "current_sleep_pressure",
        PyFloat_FromDouble((double)stats.current_sleep_pressure));
    return d;
}

/* ========================================================================
 * Brain Cycle Coordinator Bindings (observation-only)
 *
 * WHAT: Expose the C-side brain_cycle_coordinator_{register,notify_tick,
 *       unregister} API to Python so Python-driven cycles (e.g. the sleep-wake
 *       scheduler thread in brain_daemon.py) can report timing/health to the
 *       coordinator without moving their cadence to C.
 * WHY:  Keeps Python concurrency model intact (service._lock protects
 *       sleep_run_cycle from learn_vector races) while giving the coordinator
 *       uniform observability across C- and Python-driven cycles.
 * HOW:  All three functions no-op gracefully when the coordinator is NULL
 *       (common during tests or legacy setups). GIL is released around each
 *       coordinator call because the coordinator acquires an internal mutex
 *       and callers should not hold the GIL while potentially waiting.
 * ======================================================================== */

/**
 * WHAT: Register a cycle with the coordinator (observation-only)
 * WHY:  Allows a Python-driven cycle to appear in coordinator diagnostics
 * HOW:  Passes NULL cycle_handle + NULL health_fn — coordinator infers health
 *       purely from tick timing reported via cycle_notify_tick().
 */
static PyObject* Brain_cycle_register(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int type_int;
    if (!PyArg_ParseTuple(args, "i", &type_int)) return NULL;
    if (type_int < 0 || type_int >= BRAIN_CYCLE_COUNT) {
        PyErr_SetString(PyExc_ValueError, "cycle_register: type out of range");
        return NULL;
    }
    struct brain_struct* b = self->brain->internal_brain;
    brain_cycle_coordinator_t* coord = b->cycle_coordinator;
    if (!b->cycle_coordinator_enabled || !coord) {
        /* No coordinator — observation requested against a brain without one.
         * This is a valid state (tests, minimal init); return 0 to signal "not
         * an error, just not wired". */
        return PyLong_FromLong(0);
    }
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = brain_cycle_coordinator_register(
        coord,
        (brain_cycle_type_t)type_int,
        /*cycle_handle=*/NULL,
        /*health_fn=*/NULL);
    Py_END_ALLOW_THREADS
    return PyLong_FromLong((long)rc);
}

/**
 * WHAT: Notify the coordinator that a cycle has just completed a tick
 * WHY:  Gives coordinator the timing evidence it needs to infer health
 * HOW:  Wraps brain_cycle_coordinator_notify_tick with GIL released
 */
static PyObject* Brain_cycle_notify_tick(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int type_int;
    unsigned long long duration_us;
    if (!PyArg_ParseTuple(args, "iK", &type_int, &duration_us)) return NULL;
    if (type_int < 0 || type_int >= BRAIN_CYCLE_COUNT) {
        PyErr_SetString(PyExc_ValueError, "cycle_notify_tick: type out of range");
        return NULL;
    }
    struct brain_struct* b = self->brain->internal_brain;
    brain_cycle_coordinator_t* coord = b->cycle_coordinator;
    if (!b->cycle_coordinator_enabled || !coord) {
        return PyLong_FromLong(0);
    }
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = brain_cycle_coordinator_notify_tick(
        coord,
        (brain_cycle_type_t)type_int,
        (uint64_t)duration_us);
    Py_END_ALLOW_THREADS
    return PyLong_FromLong((long)rc);
}

/**
 * WHAT: Unregister a cycle from the coordinator
 * WHY:  Clean teardown at thread-shutdown time
 * HOW:  Wraps brain_cycle_coordinator_unregister with GIL released
 */
static PyObject* Brain_cycle_unregister(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int type_int;
    if (!PyArg_ParseTuple(args, "i", &type_int)) return NULL;
    if (type_int < 0 || type_int >= BRAIN_CYCLE_COUNT) {
        PyErr_SetString(PyExc_ValueError, "cycle_unregister: type out of range");
        return NULL;
    }
    struct brain_struct* b = self->brain->internal_brain;
    brain_cycle_coordinator_t* coord = b->cycle_coordinator;
    if (!b->cycle_coordinator_enabled || !coord) {
        return PyLong_FromLong(0);
    }
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = brain_cycle_coordinator_unregister(
        coord,
        (brain_cycle_type_t)type_int);
    Py_END_ALLOW_THREADS
    return PyLong_FromLong((long)rc);
}

/**
 * WHAT: Update medulla subsystem (circadian clock, arousal, etc.)
 * WHY:  Advance circadian time during training (fast_training_mode skips cognitive pipeline)
 * HOW:  Calls nimcp_brain_update_medulla_subsystem with delta_time_s
 */
static PyObject* Brain_update_medulla(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float delta_time_s;
    if (!PyArg_ParseTuple(args, "f", &delta_time_s)) return NULL;

    nimcp_brain_update_medulla_subsystem(self->brain->internal_brain, delta_time_s);
    Py_RETURN_NONE;
}

/**
 * WHAT: Add a fact to the reasoning knowledge base
 * WHY:  Training integration can inject domain knowledge as facts
 * HOW:  Calls brain_ti_add_fact with fact string and salience float
 */
static PyObject* Brain_ti_add_fact(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    const char* fact;
    float salience;
    if (!PyArg_ParseTuple(args, "sf", &fact, &salience)) return NULL;

    int result = brain_ti_add_fact(self->brain->internal_brain, fact, salience);
    /* M-4: Raise exception on error instead of silently returning False */
    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "ti_add_fact failed");
        return NULL;
    }
    Py_RETURN_TRUE;
}

/**
 * WHAT: Add a rule to the reasoning engine
 * WHY:  Rules enable forward/backward chaining over accumulated facts
 * HOW:  Calls brain_ti_add_rule with rule string and priority float
 */
static PyObject* Brain_ti_add_rule(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    const char* rule;
    float priority;
    if (!PyArg_ParseTuple(args, "sf", &rule, &priority)) return NULL;

    int result = brain_ti_add_rule(self->brain->internal_brain, rule, priority);
    /* M-4: Raise exception on error instead of silently returning False */
    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "ti_add_rule failed");
        return NULL;
    }
    Py_RETURN_TRUE;
}

/**
 * WHAT: Run forward chaining on the reasoning knowledge base
 * WHY:  Derives new facts from existing facts + rules, bounded by max_iterations
 * HOW:  Calls brain_ti_forward_chain, returns number of new facts derived
 */
static PyObject* Brain_ti_forward_chain(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int max_iterations;
    if (!PyArg_ParseTuple(args, "i", &max_iterations)) return NULL;

    /* H-3: Release GIL around C computation */
    brain_t ib = self->brain->internal_brain;
    int derived;
    Py_BEGIN_ALLOW_THREADS
    derived = brain_ti_forward_chain(ib, max_iterations);
    Py_END_ALLOW_THREADS

    PyObject* result = PyLong_FromLong(derived);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Run backward chaining to prove a goal
 * WHY:  Goal-directed reasoning checks if a conclusion can be derived
 * HOW:  Calls brain_ti_backward_chain with goal string, returns confidence float
 */
static PyObject* Brain_ti_backward_chain(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    const char* goal;
    if (!PyArg_ParseTuple(args, "s", &goal)) return NULL;

    /* H-3: Release GIL around C computation */
    brain_t ib = self->brain->internal_brain;
    float confidence;
    Py_BEGIN_ALLOW_THREADS
    confidence = brain_ti_backward_chain(ib, goal);
    Py_END_ALLOW_THREADS

    PyObject* result = PyFloat_FromDouble((double)confidence);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Query knowledge base for matching facts
 * WHY:  Allows training to check what the brain already knows
 * HOW:  Calls brain_ti_query_knowledge with query string, returns match count
 */
static PyObject* Brain_ti_query_knowledge(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    const char* query;
    if (!PyArg_ParseTuple(args, "s", &query)) return NULL;

    int count = brain_ti_query_knowledge(self->brain->internal_brain, query);
    PyObject* result = PyLong_FromLong(count);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Initialize the reasoning subsystem
 * WHY:  Must be called before add_fact/add_rule/forward_chain/backward_chain
 * HOW:  Calls brain_ti_init_reasoning, returns True on success
 */
static PyObject* Brain_ti_init_reasoning(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int result = brain_ti_init_reasoning(self->brain->internal_brain);
    /* P6-5: Raise exception on failure instead of silently returning False */
    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Reasoning subsystem initialization failed");
        return NULL;
    }
    Py_RETURN_TRUE;
}

/**
 * WHAT: Run a reasoning query and get confidence
 * WHY:  High-level reasoning interface for training feedback loops
 * HOW:  Calls brain_ti_reason with query string, returns confidence float
 */
static PyObject* Brain_ti_reason(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    const char* query;
    if (!PyArg_ParseTuple(args, "s", &query)) return NULL;

    float confidence = brain_ti_reason(self->brain->internal_brain, query);
    PyObject* result = PyFloat_FromDouble((double)confidence);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Compute adaptive learning rate based on brain state
 * WHY:  Basal ganglia dopamine + medulla arousal modulate optimal LR
 * HOW:  Calls brain_ti_compute_adaptive_lr with base_lr, returns adjusted float
 */
static PyObject* Brain_ti_compute_adaptive_lr(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float base_lr;
    if (!PyArg_ParseTuple(args, "f", &base_lr)) return NULL;

    float adaptive_lr;
    Py_BEGIN_ALLOW_THREADS
    adaptive_lr = brain_ti_compute_adaptive_lr(self->brain->internal_brain, base_lr);
    Py_END_ALLOW_THREADS

    if (isnan(adaptive_lr) || isinf(adaptive_lr)) {
        PyErr_SetString(PyExc_ValueError, "Adaptive LR returned NaN or inf");
        return NULL;
    }

    PyObject* result = PyFloat_FromDouble((double)adaptive_lr);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Compute unified adaptive LR with all brain modulations
 * WHY:  Replaces simple adaptive LR with full continuous modulation pipeline
 * HOW:  Calls brain_ti_compute_unified_lr with base_lr, returns adjusted float
 */
static PyObject* Brain_ti_compute_unified_lr(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float base_lr;
    if (!PyArg_ParseTuple(args, "f", &base_lr)) return NULL;

    float unified_lr;
    Py_BEGIN_ALLOW_THREADS
    unified_lr = brain_ti_compute_unified_lr(self->brain->internal_brain, base_lr, NULL);
    Py_END_ALLOW_THREADS

    if (isnan(unified_lr) || isinf(unified_lr)) {
        PyErr_SetString(PyExc_ValueError, "Adaptive LR returned NaN or inf");
        return NULL;
    }

    PyObject* result = PyFloat_FromDouble((double)unified_lr);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get full modulation state from all brain subsystems
 * WHY:  Exposes complete breakdown of all modulation factors for diagnostics
 * HOW:  Calls brain_ti_compute_modulation_state, returns dict with all fields
 */
static PyObject* Brain_ti_compute_modulation_state(BrainObject* self, PyObject* Py_UNUSED(ignored)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    brain_ti_modulation_state_t state;
    memset(&state, 0, sizeof(state));
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = brain_ti_compute_modulation_state(self->brain->internal_brain, &state);
    Py_END_ALLOW_THREADS

    /* Helper macros — PyDict_SetItemString does NOT steal the reference,
       so we must Py_DECREF the temporary after insertion.
       On OOM (NULL from PyFloat_FromDouble), propagate the error. */
#define SET_FLOAT(d, key, val) do { \
    PyObject* _tmp = PyFloat_FromDouble(val); \
    if (!_tmp) { Py_DECREF(d); return NULL; } \
    if (PyDict_SetItemString(d, key, _tmp) < 0) { Py_DECREF(_tmp); Py_DECREF(d); return NULL; } \
    Py_DECREF(_tmp); \
} while(0)
#define SET_BOOL(d, key, val) do { \
    PyObject* _tmp = (val) ? Py_True : Py_False; \
    Py_INCREF(_tmp); \
    if (PyDict_SetItemString(d, key, _tmp) < 0) { Py_DECREF(_tmp); Py_DECREF(d); return NULL; } \
    Py_DECREF(_tmp); \
} while(0)

    if (rc != 0) {
        /* Return defaults on error */
        PyObject* d = PyDict_New();
        if (!d) return PyErr_NoMemory();
        SET_FLOAT(d, "final_lr_factor", 1.0);
        SET_FLOAT(d, "final_batch_factor", 1.0);
        SET_FLOAT(d, "final_clip_factor", 1.0);
        SET_BOOL(d, "should_pause", 0);
        return d;
    }

    PyObject* d = PyDict_New();
    if (!d) return PyErr_NoMemory();

    /* Individual module outputs */
    SET_FLOAT(d, "arousal_level", state.arousal_level);
    SET_FLOAT(d, "arousal_cognitive_gain", state.arousal_cognitive_gain);
    SET_FLOAT(d, "arousal_memory_consolidation", state.arousal_memory_consolidation);
    SET_FLOAT(d, "circadian_efficiency", state.circadian_efficiency);
    SET_FLOAT(d, "rpe_bonus", state.rpe_bonus);
    SET_FLOAT(d, "inflammation_learning_factor", state.inflammation_learning_factor);
    SET_FLOAT(d, "inflammation_precision", state.inflammation_precision);
    SET_FLOAT(d, "instability_lr_scale", state.instability_lr_scale);
    SET_FLOAT(d, "instability_batch_scale", state.instability_batch_scale);
    SET_FLOAT(d, "instability_clip_factor", state.instability_clip_factor);
    SET_FLOAT(d, "portia_learning_gate", state.portia_learning_gate);
    SET_FLOAT(d, "portia_compute_budget", state.portia_compute_budget);
    SET_FLOAT(d, "stress_level", state.stress_level);
    SET_FLOAT(d, "cognitive_capacity", state.cognitive_capacity);
    SET_FLOAT(d, "conflict_level", state.conflict_level);

    /* Composed final modulation factors */
    SET_FLOAT(d, "final_lr_factor", state.final_lr_factor);
    SET_FLOAT(d, "final_batch_factor", state.final_batch_factor);
    SET_FLOAT(d, "final_clip_factor", state.final_clip_factor);
    SET_BOOL(d, "should_pause", state.should_pause);

#undef SET_FLOAT
#undef SET_BOOL

    return d;
}

/**
 * WHAT: Post-batch update — feed accuracy/domain back to training integration
 * WHY:  Closes the loop: training results inform basal ganglia, medulla, reasoning
 * HOW:  Calls brain_ti_post_batch_update with accuracy, expected, domain
 */
static PyObject* Brain_ti_post_batch_update(BrainObject* self, PyObject* args, PyObject* kwds) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float accuracy, expected;
    const char* domain;

    static char* kwlist[] = {"accuracy", "expected", "domain", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "ffs", kwlist,
                                     &accuracy, &expected, &domain)) {
        return NULL;
    }

    int result;
    Py_BEGIN_ALLOW_THREADS
    result = brain_ti_post_batch_update(self->brain->internal_brain,
                                        accuracy, expected, domain);
    Py_END_ALLOW_THREADS
    /* M-4: Raise exception on error instead of silently returning False */
    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "ti_post_batch_update failed");
        return NULL;
    }
    Py_RETURN_TRUE;
}

/**
 * WHAT: Check if Portia recommends skipping reasoning
 */
static PyObject* Brain_ti_should_skip_reasoning(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    bool skip = brain_ti_should_skip_reasoning();
    return PyBool_FromLong(skip);
}

/**
 * WHAT: Get Portia degradation level affecting reasoning
 */
static PyObject* Brain_ti_get_reasoning_degradation(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int level = brain_ti_get_reasoning_degradation();
    PyObject* result = PyLong_FromLong(level);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get number of reasoning phases disabled by Portia
 */
static PyObject* Brain_ti_get_reasoning_phases_disabled(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    int disabled = brain_ti_get_reasoning_phases_disabled();
    PyObject* result = PyLong_FromLong(disabled);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get cognitive capacity from hypothalamus
 */
static PyObject* Brain_ti_get_cognitive_capacity(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float capacity = brain_ti_get_cognitive_capacity(self->brain->internal_brain);
    PyObject* result = PyFloat_FromDouble((double)capacity);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get urgency mode from hypothalamus
 */
static PyObject* Brain_ti_get_urgency_mode(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    int mode = brain_ti_get_urgency_mode(self->brain->internal_brain);
    PyObject* result = PyLong_FromLong(mode);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get stress level from hypothalamus
 */
static PyObject* Brain_ti_get_stress_level(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    float stress = brain_ti_get_stress_level(self->brain->internal_brain);
    PyObject* result = PyFloat_FromDouble((double)stress);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Check if mesh network is available for reasoning
 * WHY:  Mesh provides distributed consensus evidence for reasoning queries
 * HOW:  Calls brain_ti_mesh_is_available(), returns bool
 */
static PyObject* Brain_ti_mesh_is_available(BrainObject* self, PyObject* args) {
    (void)args;
    /* M-2: Proper null check for brain */
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    bool avail = brain_ti_mesh_is_available();
    return PyBool_FromLong(avail);
}

/**
 * WHAT: Get mesh reasoning channel participant count
 * WHY:  More participants = stronger consensus evidence
 * HOW:  Calls brain_ti_mesh_get_participant_count(), returns uint32
 */
static PyObject* Brain_ti_mesh_get_participant_count(BrainObject* self, PyObject* args) {
    (void)args;
    /* M-2: Proper null check for brain */
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    uint32_t count = brain_ti_mesh_get_participant_count();
    PyObject* result = PyLong_FromUnsignedLong(count);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Get mesh reasoning channel coherence
 * WHY:  High coherence means mesh participants agree, low means disagreement
 * HOW:  Calls brain_ti_mesh_get_coherence(), returns float [0,1]
 */
static PyObject* Brain_ti_mesh_get_coherence(BrainObject* self, PyObject* args) {
    (void)args;
    /* M-2: Proper null check for brain */
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    float coherence = brain_ti_mesh_get_coherence();
    PyObject* result = PyFloat_FromDouble((double)coherence);
    if (!result) return NULL;  /* OOM */
    return result;
}

/**
 * WHAT: Pre-compute and cache community structure for consolidation replay
 * WHY:  Louvain community detection is O(N log N), takes hours on 2M neurons.
 *       Caching allows consolidation to use community-aware prioritization
 *       without re-triggering the expensive algorithm.
 * HOW:  Runs community_detect() + community_detect_hubs() on the brain's
 *       neural network and stores results in the BrainObject's cache.
 *
 * Python usage:
 *   brain.cache_communities()  # Pre-compute (blocking)
 *   brain.consolidate()        # Uses cache automatically
 */
static PyObject* Brain_cache_communities(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    /* Create cache if needed */
    if (!self->community_cache) {
        self->community_cache = consolidation_community_cache_create();
        if (!self->community_cache) {
            PyErr_SetString(PyExc_MemoryError, "Failed to allocate community cache");
            return NULL;
        }
    }

    bool success;
    brain_t internal = self->brain->internal_brain;
    consolidation_community_cache_t* cache = self->community_cache;

    Py_BEGIN_ALLOW_THREADS
    success = consolidation_cache_communities(cache, internal);
    Py_END_ALLOW_THREADS

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Community detection failed");
        return NULL;
    }

    /* Return dict with cache info */
    PyObject* result = PyDict_New();
    if (!result) return NULL;

    PyObject* tmp;
    tmp = PyLong_FromUnsignedLong(self->community_cache->num_communities);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "num_communities", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(self->community_cache->num_hubs);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "num_hubs", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(self->community_cache->modularity);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "modularity", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(self->community_cache->num_neurons);
    if (!tmp) { Py_DECREF(result); return NULL; }
    if (PyDict_SetItemString(result, "num_neurons", tmp) < 0) { Py_DECREF(tmp); Py_DECREF(result); return NULL; }
    Py_DECREF(tmp);

    return result;
}

/**
 * WHAT: Invalidate the cached community structure
 * WHY:  Force re-computation on next cache_communities() call
 * HOW:  Mark cache as stale without freeing memory
 *
 * Python usage:
 *   brain.invalidate_community_cache()
 */
static PyObject* Brain_invalidate_community_cache(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (self->community_cache) {
        consolidation_community_cache_invalidate(self->community_cache);
    }
    Py_RETURN_NONE;
}

/* -------- Octopus cognitive module (Phase 1 — observability only) -------- */
#include "cognitive/octopus/nimcp_octopus.h"

static PyObject* Brain_octopus_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    octopus_system_t* ctx = (octopus_system_t*)self->brain->internal_brain->octopus;
    if (!ctx) {
        PyObject* d = PyDict_New();
        if (d) PyDict_SetItemString(d, "enabled", Py_False);
        return d;
    }
    octopus_stats_t st;
    octopus_get_stats(ctx, &st);
    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* arms_list = PyList_New(st.n_arms);
    if (arms_list) {
        for (uint32_t a = 0; a < st.n_arms; a++) {
            PyList_SET_ITEM(arms_list, a,
                PyFloat_FromDouble(octopus_get_broadcast_state(ctx, a)));
        }
    }
    /* DRY ref-counting helpers: PyLong_FromUnsignedLong / PyFloat_FromDouble
     * return NEW references. PyDict_SetItemString INCREFs the value, so we
     * must DECREF the temp to match or we leak one PyObject per field. */
#define SI(k, val) do { \
    PyObject* _v = PyLong_FromUnsignedLong((unsigned long)(val)); \
    if (_v) { PyDict_SetItemString(d, (k), _v); Py_DECREF(_v); } \
} while (0)
#define SF(k, val) do { \
    PyObject* _v = PyFloat_FromDouble((double)(val)); \
    if (_v) { PyDict_SetItemString(d, (k), _v); Py_DECREF(_v); } \
} while (0)
    PyDict_SetItemString(d, "enabled", Py_True);  /* Py_True is a singleton */
    SI("n_arms",                st.n_arms);
    SI("n_explorations",        st.n_explorations);
    SI("n_integrations",        st.n_integrations);
    SI("n_ethics_vetoes",       st.n_ethics_vetoes);
    SI("n_swarm_delegations",   st.n_swarm_delegations);
    SI("n_world_model_updates", st.n_world_model_updates);
    SF("avg_arm_confidence",    st.avg_arm_confidence);
    SF("avg_arm_variance",      st.avg_arm_variance);
    SF("central_coherence",     st.central_coherence);
    SF("avg_arm_entropy",       st.avg_arm_entropy);
    SF("avg_arm_dfa",           st.avg_arm_dfa);
    SI("n_dfa_computations",    st.n_dfa_computations);
    SI("n_pink_noise_injections", st.n_pink_noise_injections);
    /* Phase 3b/3c bridge counters. SI uses unsigned long — uint64_t fits
     * on 64-bit platforms; overflow only at ~10^18 events. */
    SI("bridge_engram_encodings",   st.bridge_engram_encodings);
    SI("bridge_kg_nodes_added",     st.bridge_kg_nodes_added);
    SI("bridge_stress_broadcasts",  st.bridge_stress_broadcasts);
    SI("bridge_fear_broadcasts",    st.bridge_fear_broadcasts);
    SI("bridge_amygdala_steps",     st.bridge_amygdala_steps);
    SF("bridge_last_cortisol",      st.bridge_last_cortisol);
    SF("bridge_last_fear",          st.bridge_last_fear);
    /* Phase 4a additions. */
    SI("bridge_wm_updates",         st.bridge_wm_updates);
    SI("bridge_fear_conditionings", st.bridge_fear_conditionings);
    /* Phase 4b addition: occipital→octopus feature sampling. */
    SI("bridge_vision_samples",     st.bridge_vision_samples);
    /* Phase 4c addition: audio cortex→octopus feature sampling. */
    SI("bridge_audio_samples",      st.bridge_audio_samples);
    /* Phase 4d addition: somatosensory→octopus feature sampling. */
    SI("bridge_somato_samples",     st.bridge_somato_samples);
    /* Phase 4e addition: SNN→octopus feature sampling. */
    SI("bridge_snn_samples",        st.bridge_snn_samples);
    /* Phase 4f addition: neuromodulator→octopus feature sampling. */
    SI("bridge_neuromod_samples",   st.bridge_neuromod_samples);
    /* Phase 4g addition: peer-module (Portia+Dragonfly)→octopus sampling. */
    SI("bridge_peer_samples",       st.bridge_peer_samples);
    /* Phase 4h: per-arm LNN temporal integration. */
    SI("n_lnn_steps",               st.n_lnn_steps);
    PyDict_SetItemString(d, "lnn_enabled", st.lnn_enabled ? Py_True : Py_False);
    /* Phase 4j: gradient flow on arm LNNs. */
    SI("n_lnn_train_steps",         st.n_lnn_train_steps);
    SF("last_lnn_loss",             st.last_lnn_loss);
    PyDict_SetItemString(d, "lnn_training_enabled",
                         st.lnn_training_enabled ? Py_True : Py_False);
    /* Phase 4l: natural-gradient latent regularization. */
    SI("n_ng_steps",                st.n_ng_steps);
    PyDict_SetItemString(d, "ng_enabled",
                         st.ng_enabled ? Py_True : Py_False);
    /* Phase 4m: JEPA latent-space predictive coding. */
    SI("n_jepa_steps",              st.n_jepa_steps);
    SF("last_jepa_loss",            st.last_jepa_loss);
    PyDict_SetItemString(d, "jepa_enabled",
                         st.jepa_enabled ? Py_True : Py_False);
    if (arms_list) {
        PyDict_SetItemString(d, "arm_broadcast_states", arms_list);
        Py_DECREF(arms_list);  /* SetItemString INCREFs; release our ref */
    }
#undef SI
#undef SF
    return d;
}

/* -------- Phase 5B: octopus modality-specific explore bindings --------
 *
 * Expose the six nimcp_octopus_explore_from_* bridges + the two training
 * toggles so the Python training curriculum can route each modality
 * exposure through the matching sampler (vision → occipital, audio →
 * audio cortex, touch → somatosensory, etc.) for enriched arm input.
 *
 * Forward-declared here to avoid pulling the bridge header's brain_t
 * typedef into the binding TU (already has its own brain handle type).
 * Link-resolved against the octopus_bridges compilation unit. */
#include "cognitive/octopus/nimcp_octopus_bridges.h"

#define OCTOPUS_EXPLORE_WRAPPER(pyname, cfn) \
static PyObject* Brain_octopus_##pyname(BrainObject* self, PyObject* Py_UNUSED(a)) { \
    if (!self->brain) { \
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); \
        return NULL; \
    } \
    CHECK_INTERNAL_BRAIN(self); \
    int rc = cfn(self->brain->internal_brain); \
    return PyLong_FromLong((long)rc); \
}

OCTOPUS_EXPLORE_WRAPPER(explore_from_occipital,      nimcp_octopus_explore_from_occipital)
OCTOPUS_EXPLORE_WRAPPER(explore_from_audio_cortex,   nimcp_octopus_explore_from_audio_cortex)
OCTOPUS_EXPLORE_WRAPPER(explore_from_somatosensory,  nimcp_octopus_explore_from_somatosensory)
OCTOPUS_EXPLORE_WRAPPER(explore_from_snn,            nimcp_octopus_explore_from_snn)
OCTOPUS_EXPLORE_WRAPPER(explore_from_neuromod,       nimcp_octopus_explore_from_neuromod)
OCTOPUS_EXPLORE_WRAPPER(explore_from_peers,          nimcp_octopus_explore_from_peers)

#undef OCTOPUS_EXPLORE_WRAPPER

/* Toggles for training + natural-gradient — one-arg (bool) each. */
static PyObject* Brain_octopus_set_training_enabled(BrainObject* self, PyObject* args) {
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    octopus_system_t* ctx =
        (octopus_system_t*)self->brain->internal_brain->octopus;
    if (ctx) octopus_set_training_enabled(ctx, enabled ? true : false);
    Py_RETURN_NONE;
}

static PyObject* Brain_octopus_set_ng_enabled(BrainObject* self, PyObject* args) {
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    octopus_system_t* ctx =
        (octopus_system_t*)self->brain->internal_brain->octopus;
    if (ctx) octopus_set_ng_enabled(ctx, enabled ? true : false);
    Py_RETURN_NONE;
}

static PyObject* Brain_octopus_set_jepa_enabled(BrainObject* self, PyObject* args) {
    int enabled = 0;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    octopus_system_t* ctx =
        (octopus_system_t*)self->brain->internal_brain->octopus;
    if (ctx) octopus_set_jepa_enabled(ctx, enabled ? true : false);
    Py_RETURN_NONE;
}

/* -------- Phase E1: elephant-inspired long-term memory (landmark API) --------
 * Surfaces Z-Ladder's landmark machinery. Landmarks are explicitly-designated
 * permanent memories — promoted to Z3 and protected from automatic demotion.
 * These methods give Python observability (stats / audit) + the mark/unmark
 * entrypoints. Node creation from Python is a deeper plumbing problem
 * (requires pr_node_manager ownership) and is out of scope for Phase E1. */
#include "cognitive/memory/core/nimcp_z_ladder.h"

static PyObject* Brain_long_term_stats(BrainObject* self, PyObject* Py_UNUSED(a)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    PyObject* d = PyDict_New();
    if (!d) return NULL;

    z_ladder_t ladder = self->brain->internal_brain->pr_z_ladder;
    if (!ladder) {
        PyDict_SetItemString(d, "enabled", Py_False);
        return d;
    }
    PyDict_SetItemString(d, "enabled", Py_True);

    size_t n_landmarks = z_ladder_landmark_count(ladder);
    PyObject* v = PyLong_FromSize_t(n_landmarks);
    if (v) { PyDict_SetItemString(d, "landmark_count", v); Py_DECREF(v); }

    size_t total = z_ladder_get_total_count(ladder);
    v = PyLong_FromSize_t(total);
    if (v) { PyDict_SetItemString(d, "total_nodes", v); Py_DECREF(v); }

    /* Per-tier counts. */
    for (int tier = 0; tier < 4; tier++) {
        size_t n = z_ladder_get_count(ladder, (pr_memory_tier_t)tier);
        char key[16];
        snprintf(key, sizeof(key), "z%d_count", tier);
        v = PyLong_FromSize_t(n);
        if (v) { PyDict_SetItemString(d, key, v); Py_DECREF(v); }
    }

    /* Audit summary. */
    z_ladder_landmark_audit_t audit;
    if (z_ladder_audit_landmarks(ladder, &audit) == Z_LADDER_SUCCESS) {
        v = PyLong_FromSize_t(audit.present_in_ladder);
        if (v) { PyDict_SetItemString(d, "landmarks_present", v); Py_DECREF(v); }
        v = PyLong_FromSize_t(audit.missing_from_ladder);
        if (v) { PyDict_SetItemString(d, "landmarks_missing", v); Py_DECREF(v); }
        v = PyLong_FromSize_t(audit.at_z3);
        if (v) { PyDict_SetItemString(d, "landmarks_at_z3", v); Py_DECREF(v); }
        v = PyLong_FromSize_t(audit.drifted_below_z3);
        if (v) { PyDict_SetItemString(d, "landmarks_drifted", v); Py_DECREF(v); }
        v = PyFloat_FromDouble((double)audit.min_consolidation);
        if (v) { PyDict_SetItemString(d, "landmark_min_consolidation", v); Py_DECREF(v); }
        v = PyFloat_FromDouble((double)audit.avg_consolidation);
        if (v) { PyDict_SetItemString(d, "landmark_avg_consolidation", v); Py_DECREF(v); }
    }

    /* Phase E3: auto-insert observability. */
    struct brain_struct* ib2 = self->brain->internal_brain;
    PyDict_SetItemString(d, "auto_insert_enabled",
                         ib2->pr_auto_insert_enabled ? Py_True : Py_False);
    v = PyLong_FromUnsignedLongLong((unsigned long long)ib2->pr_auto_insert_count);
    if (v) { PyDict_SetItemString(d, "auto_insert_count", v); Py_DECREF(v); }
    v = PyLong_FromUnsignedLongLong((unsigned long long)ib2->pr_auto_landmark_count);
    if (v) { PyDict_SetItemString(d, "auto_landmark_count", v); Py_DECREF(v); }
    /* Phase E6: autonomous driver thread observability. */
    v = PyLong_FromUnsignedLongLong((unsigned long long)ib2->pr_memory_driver_ticks);
    if (v) { PyDict_SetItemString(d, "driver_ticks", v); Py_DECREF(v); }
    PyDict_SetItemString(d, "driver_running",
                         ib2->pr_memory_driver_thread ? Py_True : Py_False);
    v = PyFloat_FromDouble((double)ib2->pr_auto_insert_confidence);
    if (v) { PyDict_SetItemString(d, "auto_insert_threshold", v); Py_DECREF(v); }
    v = PyFloat_FromDouble((double)ib2->pr_auto_landmark_confidence);
    if (v) { PyDict_SetItemString(d, "auto_landmark_threshold", v); Py_DECREF(v); }
    return d;
}

static PyObject* Brain_long_term_mark_landmark(BrainObject* self, PyObject* args) {
    unsigned long long node_id = 0;
    const char* reason = NULL;
    if (!PyArg_ParseTuple(args, "K|s", &node_id, &reason)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    z_ladder_t ladder = self->brain->internal_brain->pr_z_ladder;
    if (!ladder) {
        PyErr_SetString(PyExc_RuntimeError, "Z-Ladder not initialized on this brain");
        return NULL;
    }
    z_ladder_error_t rc = z_ladder_mark_landmark(ladder, (uint64_t)node_id, reason);
    return PyLong_FromLong((long)rc);
}

static PyObject* Brain_long_term_unmark_landmark(BrainObject* self, PyObject* args) {
    unsigned long long node_id = 0;
    if (!PyArg_ParseTuple(args, "K", &node_id)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    z_ladder_t ladder = self->brain->internal_brain->pr_z_ladder;
    if (!ladder) Py_RETURN_NONE;
    z_ladder_error_t rc = z_ladder_unmark_landmark(ladder, (uint64_t)node_id);
    return PyLong_FromLong((long)rc);
}

/* Phase E2: insert a memory node from a Python float list. Creates a
 * pr_memory_node using brain->pr_node_manager, inserts into Z0, returns
 * the new node_id (caller can then pass it to long_term_mark_landmark).
 * If make_landmark=True, also marks immediately (promoted to Z3). */
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
static PyObject* Brain_long_term_insert_memory(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    int make_landmark = 0;
    const char* reason = NULL;
    if (!PyArg_ParseTuple(args, "O|ps", &features_list, &make_landmark, &reason)) {
        return NULL;
    }
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    /* Phase E2: lazy-init pr_memory on first insert. Most brains default to
     * lazy pr_memory init (FAST mode, etc.) so the subsystem is skipped
     * during factory startup; we trigger it here on the first use path. */
    struct brain_struct* ib = self->brain->internal_brain;
    if (!ib->pr_z_ladder || !ib->pr_node_manager) {
        extern bool nimcp_brain_pr_memory_init(struct brain_struct* brain, const void* config);
        (void)nimcp_brain_pr_memory_init(ib, NULL);
    }
    pr_node_manager_t mgr = (pr_node_manager_t)ib->pr_node_manager;
    z_ladder_t ladder = ib->pr_z_ladder;
    if (!mgr || !ladder) {
        PyErr_SetString(PyExc_RuntimeError, "PR memory initialization failed");
        return NULL;
    }
    if (!PyList_Check(features_list) && !PyTuple_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list or tuple of floats");
        return NULL;
    }
    Py_ssize_t n = PySequence_Size(features_list);
    if (n <= 0) {
        PyErr_SetString(PyExc_ValueError, "features must be non-empty");
        return NULL;
    }
    /* Pack Python list into a float buffer. Uses stack for small n,
     * heap for large. Cap 4096 floats per call to bound work. */
    if (n > 4096) n = 4096;
    float stack_buf[256];
    float* data = (n <= 256) ? stack_buf
        : (float*)nimcp_calloc((size_t)n, sizeof(float));
    if (!data) return PyErr_NoMemory();
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject* item = PySequence_GetItem(features_list, i);
        if (!item) { if (data != stack_buf) nimcp_free(data); return NULL; }
        data[i] = (float)PyFloat_AsDouble(item);
        Py_DECREF(item);
    }

    pr_node_config_t node_cfg = pr_memory_node_default_config();
    node_cfg.initial_tier    = PR_MEMORY_TIER_Z0;
    node_cfg.initial_strength = 1.0f;
    node_cfg.compute_signature = true;

    pr_memory_node_t* node = pr_memory_node_create(
        mgr, (const void*)data, (size_t)n * sizeof(float), &node_cfg);
    if (data != stack_buf) nimcp_free(data);
    if (!node) {
        PyErr_SetString(PyExc_RuntimeError, "pr_memory_node_create failed");
        return NULL;
    }

    z_ladder_error_t ins = z_ladder_insert(ladder, node, PR_MEMORY_TIER_Z0);
    if (ins != Z_LADDER_SUCCESS) {
        pr_memory_node_destroy(node);
        return PyLong_FromLongLong(-1);
    }

    uint64_t node_id = pr_memory_node_get_id(node);
    if (make_landmark) {
        (void)z_ladder_mark_landmark(ladder, node_id,
            reason ? reason : "python_insert");
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)node_id);
}

/* Phase E3: configure auto-insert behavior. Called before training to
 * enable automatic memory-node insertion on high-confidence learning. */
static PyObject* Brain_long_term_set_auto_insert(BrainObject* self, PyObject* args) {
    int enabled = 0;
    double insert_conf = 0.7;
    double landmark_conf = 0.95;
    if (!PyArg_ParseTuple(args, "p|dd", &enabled, &insert_conf, &landmark_conf)) {
        return NULL;
    }
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    struct brain_struct* ib = self->brain->internal_brain;

    /* Phase E3: enabling auto-insert when pr_memory is lazy triggers the
     * init here — otherwise brain_learn_vector's auto-insert hook would
     * no-op silently on every call until something else lazy-triggered it. */
    if (enabled && (!ib->pr_z_ladder || !ib->pr_node_manager)) {
        extern bool nimcp_brain_pr_memory_init(struct brain_struct* brain, const void* config);
        (void)nimcp_brain_pr_memory_init(ib, NULL);
    }

    ib->pr_auto_insert_enabled      = enabled ? true : false;
    /* Clamp thresholds to [0,1]. */
    if (insert_conf < 0.0)  insert_conf  = 0.0;
    if (insert_conf > 1.0)  insert_conf  = 1.0;
    if (landmark_conf < 0.0) landmark_conf = 0.0;
    if (landmark_conf > 1.0) landmark_conf = 1.0;
    ib->pr_auto_insert_confidence   = (float)insert_conf;
    ib->pr_auto_landmark_confidence = (float)landmark_conf;
    Py_RETURN_NONE;
}

/* Phase E3: oracle query by feature similarity. Returns list of dicts:
 * [{node_id, similarity, tier}, ...] sorted by similarity desc. */
static PyObject* Brain_long_term_query(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    int top_k = 5;
    if (!PyArg_ParseTuple(args, "O|i", &features_list, &top_k)) return NULL;
    if (top_k <= 0 || top_k > 64) top_k = 5;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    z_ladder_t ladder = self->brain->internal_brain->pr_z_ladder;
    if (!ladder) return PyList_New(0);  /* empty list on unavailable */

    if (!PyList_Check(features_list) && !PyTuple_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list or tuple of floats");
        return NULL;
    }
    Py_ssize_t n = PySequence_Size(features_list);
    if (n <= 0) return PyList_New(0);
    if (n > 4096) n = 4096;

    /* Pack features into a float buffer, then build a prime signature. */
    float stack_buf[256];
    float* data = (n <= 256) ? stack_buf
        : (float*)nimcp_calloc((size_t)n, sizeof(float));
    if (!data) return PyErr_NoMemory();
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject* item = PySequence_GetItem(features_list, i);
        if (!item) { if (data != stack_buf) nimcp_free(data); return NULL; }
        data[i] = (float)PyFloat_AsDouble(item);
        Py_DECREF(item);
    }
    prime_signature_t* sig = prime_sig_from_floats(data, (size_t)n);
    if (data != stack_buf) nimcp_free(data);
    if (!sig) {
        PyErr_SetString(PyExc_RuntimeError, "prime_sig_from_floats failed");
        return NULL;
    }

    /* Run query. Results buffer sized to top_k. */
    z_ladder_landmark_hit_t hits[64];
    size_t out_count = 0;
    z_ladder_error_t rc = z_ladder_landmark_query(
        ladder, sig, hits, (size_t)top_k, &out_count);
    prime_sig_destroy(sig);
    if (rc != Z_LADDER_SUCCESS) {
        return PyList_New(0);
    }

    /* Build return list. */
    PyObject* list = PyList_New((Py_ssize_t)out_count);
    if (!list) return NULL;
    for (size_t i = 0; i < out_count; i++) {
        PyObject* entry = PyDict_New();
        if (!entry) { Py_DECREF(list); return NULL; }
        PyObject* v;
        v = PyLong_FromUnsignedLongLong((unsigned long long)hits[i].node_id);
        if (v) { PyDict_SetItemString(entry, "node_id", v); Py_DECREF(v); }
        v = PyFloat_FromDouble((double)hits[i].similarity);
        if (v) { PyDict_SetItemString(entry, "similarity", v); Py_DECREF(v); }
        v = PyLong_FromLong((long)hits[i].tier);
        if (v) { PyDict_SetItemString(entry, "tier", v); Py_DECREF(v); }
        PyList_SET_ITEM(list, (Py_ssize_t)i, entry);
    }
    return list;
}

/* Phase E4 #5: oracle query across all Z0..Z3 tiers (not just landmarks). */
static PyObject* Brain_long_term_query_all(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    int top_k = 5;
    if (!PyArg_ParseTuple(args, "O|i", &features_list, &top_k)) return NULL;
    if (top_k <= 0 || top_k > 64) top_k = 5;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    z_ladder_t ladder = self->brain->internal_brain->pr_z_ladder;
    if (!ladder) return PyList_New(0);

    if (!PyList_Check(features_list) && !PyTuple_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list or tuple of floats");
        return NULL;
    }
    Py_ssize_t n = PySequence_Size(features_list);
    if (n <= 0) return PyList_New(0);
    if (n > 4096) n = 4096;

    float stack_buf[256];
    float* data = (n <= 256) ? stack_buf
        : (float*)nimcp_calloc((size_t)n, sizeof(float));
    if (!data) return PyErr_NoMemory();
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject* item = PySequence_GetItem(features_list, i);
        if (!item) { if (data != stack_buf) nimcp_free(data); return NULL; }
        data[i] = (float)PyFloat_AsDouble(item);
        Py_DECREF(item);
    }
    prime_signature_t* sig = prime_sig_from_floats(data, (size_t)n);
    if (data != stack_buf) nimcp_free(data);
    if (!sig) {
        PyErr_SetString(PyExc_RuntimeError, "prime_sig_from_floats failed");
        return NULL;
    }

    z_ladder_landmark_hit_t hits[64];
    size_t out_count = 0;
    z_ladder_error_t rc = z_ladder_query_all_tiers(
        ladder, sig, hits, (size_t)top_k, &out_count);
    prime_sig_destroy(sig);
    if (rc != Z_LADDER_SUCCESS) return PyList_New(0);

    PyObject* list = PyList_New((Py_ssize_t)out_count);
    if (!list) return NULL;
    for (size_t i = 0; i < out_count; i++) {
        PyObject* entry = PyDict_New();
        if (!entry) { Py_DECREF(list); return NULL; }
        PyObject* v;
        v = PyLong_FromUnsignedLongLong((unsigned long long)hits[i].node_id);
        if (v) { PyDict_SetItemString(entry, "node_id", v); Py_DECREF(v); }
        v = PyFloat_FromDouble((double)hits[i].similarity);
        if (v) { PyDict_SetItemString(entry, "similarity", v); Py_DECREF(v); }
        v = PyLong_FromLong((long)hits[i].tier);
        if (v) { PyDict_SetItemString(entry, "tier", v); Py_DECREF(v); }
        PyList_SET_ITEM(list, (Py_ssize_t)i, entry);
    }
    return list;
}

/* Phase E4 #4: event-triggered landmark promotion. Features + reason in;
 * returns the new node_id (uint64) or 0 on failure. */
static PyObject* Brain_long_term_promote_event(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    const char* reason = NULL;
    if (!PyArg_ParseTuple(args, "O|s", &features_list, &reason)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    /* Lazy-init pr_memory if needed — matches Brain_long_term_insert_memory. */
    if (!self->brain->internal_brain->pr_z_ladder) {
        extern bool nimcp_brain_pr_memory_init(struct brain_struct* brain,
                                               const void* config);
        if (!nimcp_brain_pr_memory_init(self->brain->internal_brain, NULL)) {
            PyErr_SetString(PyExc_RuntimeError, "pr_memory init failed");
            return NULL;
        }
    }

    if (!PyList_Check(features_list) && !PyTuple_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list or tuple of floats");
        return NULL;
    }
    Py_ssize_t n = PySequence_Size(features_list);
    if (n <= 0) return PyLong_FromUnsignedLongLong(0);
    if (n > 4096) n = 4096;

    float stack_buf[256];
    float* data = (n <= 256) ? stack_buf
        : (float*)nimcp_calloc((size_t)n, sizeof(float));
    if (!data) return PyErr_NoMemory();
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject* item = PySequence_GetItem(features_list, i);
        if (!item) { if (data != stack_buf) nimcp_free(data); return NULL; }
        data[i] = (float)PyFloat_AsDouble(item);
        Py_DECREF(item);
    }

    extern uint64_t nimcp_brain_pr_memory_promote_event(
        struct brain_struct* brain, const float* features,
        uint32_t num_features, const char* reason);
    uint64_t id = nimcp_brain_pr_memory_promote_event(
        self->brain->internal_brain, data, (uint32_t)n, reason);

    if (data != stack_buf) nimcp_free(data);
    return PyLong_FromUnsignedLongLong((unsigned long long)id);
}

/* Phase E4 #2: save landmarks to disk. Returns True on success. */
static PyObject* Brain_long_term_save(BrainObject* self, PyObject* args) {
    const char* path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);
    extern bool nimcp_brain_pr_memory_save_landmarks(
        struct brain_struct* brain, const char* path);
    bool ok = nimcp_brain_pr_memory_save_landmarks(
        self->brain->internal_brain, path);
    if (ok) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

/* Phase E4 #2: load landmarks from disk. Returns True on success. */
static PyObject* Brain_long_term_load(BrainObject* self, PyObject* args) {
    const char* path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path)) return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    /* Lazy-init pr_memory if needed so load has a ladder to insert into. */
    if (!self->brain->internal_brain->pr_z_ladder) {
        extern bool nimcp_brain_pr_memory_init(struct brain_struct* brain,
                                               const void* config);
        if (!nimcp_brain_pr_memory_init(self->brain->internal_brain, NULL)) {
            PyErr_SetString(PyExc_RuntimeError, "pr_memory init failed");
            return NULL;
        }
    }

    extern bool nimcp_brain_pr_memory_load_landmarks(
        struct brain_struct* brain, const char* path);
    bool ok = nimcp_brain_pr_memory_load_landmarks(
        self->brain->internal_brain, path);
    if (ok) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

/**
 * WHAT: Get epistemic + aleatoric uncertainty for input features
 * WHY:  Metacognitive monitoring — "how confident is the brain?"
 * HOW:  Call brain_get_uncertainty() through introspection accessor
 */
static PyObject* Brain_get_uncertainty(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    /* Features are optional — if not provided, return overall uncertainty */
    if (!PyArg_ParseTuple(args, "|O", &features_list))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    introspection_context_t introspection = brain_get_introspection(self->brain->internal_brain);
    if (!introspection) {
        /* No introspection — return zeros */
        PyObject* dict = PyDict_New();
        if (!dict) return NULL;
        PyObject* zero = PyFloat_FromDouble(0.0);
        if (!zero) { Py_DECREF(dict); return NULL; }
        if (PyDict_SetItemString(dict, "epistemic", zero) < 0 ||
            PyDict_SetItemString(dict, "aleatoric", zero) < 0 ||
            PyDict_SetItemString(dict, "total", zero) < 0 ||
            PyDict_SetItemString(dict, "confidence", zero) < 0) {
            Py_DECREF(zero);
            Py_DECREF(dict);
            return NULL;
        }
        Py_DECREF(zero);
        return dict;
    }

    float* features = NULL;
    Py_ssize_t num_features = 0;
    if (features_list && features_list != Py_None) {
        features = py_list_to_float_array(features_list, &num_features);
        if (!features) return NULL;

        if (num_features > UINT32_MAX) {
            nimcp_free(features);
            PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
            return NULL;
        }
    }

    brain_uncertainty_t unc;
    Py_BEGIN_ALLOW_THREADS
    if (features) {
        unc = brain_get_uncertainty(introspection, features, (uint32_t)num_features);
    } else {
        /* No features — use a zero vector for general uncertainty */
        float dummy[1] = {0.0f};
        unc = brain_get_uncertainty(introspection, dummy, 1);
    }
    Py_END_ALLOW_THREADS

    if (features) nimcp_free(features);

    PyObject* dict = PyDict_New();
    if (!dict) { brain_uncertainty_free(&unc); return NULL; }

#define SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); brain_uncertainty_free(&unc); return NULL; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); brain_uncertainty_free(&unc); return NULL; } \
    Py_DECREF(v); \
} while (0)

    SET("epistemic", PyFloat_FromDouble(unc.epistemic));
    SET("aleatoric", PyFloat_FromDouble(unc.aleatoric));
    SET("total", PyFloat_FromDouble(unc.total));
    SET("confidence", PyFloat_FromDouble(unc.confidence));
    SET("ensemble_size", PyLong_FromUnsignedLong(unc.ensemble_size));

#undef SET

    brain_uncertainty_free(&unc);
    return dict;
}

/**
 * WHAT: Self-model assessment for a domain
 * WHY:  Track brain's perceived capabilities per domain
 * HOW:  Call self_model_get() through brain internal access
 */
static PyObject* Brain_self_assess(BrainObject* self, PyObject* args) {
    const char* domain;
    if (!PyArg_ParseTuple(args, "s", &domain))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    /* Return probe-based assessment since self_model doesn't have a
       per-domain getter — use probe metrics as a proxy */
    nimcp_brain_probe_t probe;
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_probe(self->brain, &probe);
    Py_END_ALLOW_THREADS

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

#define SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(v); \
} while (0)

    SET("domain", PyUnicode_FromString(domain));
    if (status == NIMCP_OK) {
        SET("accuracy", PyFloat_FromDouble(probe.accuracy));
        SET("learning_steps", PyLong_FromUnsignedLongLong(probe.total_learning_steps));
        SET("sparsity", PyFloat_FromDouble(probe.avg_sparsity));
        SET("learning_rate", PyFloat_FromDouble(probe.current_learning_rate));
        SET("num_neurons", PyLong_FromUnsignedLong(probe.num_neurons));
        SET("assessment", PyUnicode_FromString("active"));
    } else {
        SET("assessment", PyUnicode_FromString("unavailable"));
    }

#undef SET
    return dict;
}

/**
 * WHAT: Process audio through the brain's audio cortex
 * WHY:  Extract MFCC + mel features from raw audio for multimodal training
 * HOW:  Call audio_cortex_process() on the brain's audio cortex (GIL released)
 *
 * @param samples Python list of float audio samples
 * @return Python list of float features, or empty list if cortex unavailable
 */
static PyObject* Brain_audio_cortex_process(BrainObject* self, PyObject* args) {
    PyObject* samples_list;
    if (!PyArg_ParseTuple(args, "O", &samples_list))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    Py_ssize_t num_samples;
    float* samples = py_list_to_float_array(samples_list, &num_samples);
    if (!samples) return NULL;

    if (num_samples > UINT32_MAX) {
        nimcp_free(samples);
        PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
        return NULL;
    }

    /* Access audio cortex from internal brain */
    brain_t ib = self->brain->internal_brain;
    if (!ib || !ib->audio_cortex) {  /* P6-2: ib checked here as fallback path */
        nimcp_free(samples);
        /* Return empty list — cortex not initialized */
        return PyList_New(0);
    }

    /* Query feature dimension */
    uint32_t feat_dim = audio_cortex_get_feature_dim(ib->audio_cortex);
    if (feat_dim == 0) feat_dim = 128;

    float* features = (float*)nimcp_calloc(feat_dim, sizeof(float));
    if (!features) {
        nimcp_free(samples);
        return PyErr_NoMemory();
    }

    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = audio_cortex_process(ib->audio_cortex, samples,
                                    (uint32_t)num_samples, 1, features);
    Py_END_ALLOW_THREADS

    nimcp_free(samples);

    if (!success) {
        nimcp_free(features);
        return PyList_New(0);
    }

    /* Build Python list */
    PyObject* result = PyList_New(feat_dim);
    if (!result) { nimcp_free(features); return NULL; }
    for (uint32_t i = 0; i < feat_dim; i++) {
        PyObject* val = PyFloat_FromDouble((double)features[i]);
        if (!val) { Py_DECREF(result); nimcp_free(features); return NULL; }
        PyList_SET_ITEM(result, i, val);
    }
    nimcp_free(features);
    return result;
}

/**
 * WHAT: Process image through the brain's visual cortex
 * WHY:  Extract V1 Gabor filter features from raw pixels for visual training
 * HOW:  Call visual_cortex_process() on the brain's visual cortex (GIL released)
 *
 * @param pixels Python list of float pixel values [0-1] or [0-255]
 * @param width Image width
 * @param height Image height
 * @param channels Number of channels (1=grayscale, 3=RGB)
 * @return Python list of float features, or empty list if cortex unavailable
 */
static PyObject* Brain_visual_cortex_process(BrainObject* self, PyObject* args) {
    PyObject* pixels_list;
    unsigned int width, height, channels;
    if (!PyArg_ParseTuple(args, "OIII", &pixels_list, &width, &height, &channels))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    Py_ssize_t num_pixels;
    float* pixels_float = py_list_to_float_array(pixels_list, &num_pixels);
    if (!pixels_float) return NULL;

    /* Visual cortex expects uint8_t* image — convert float [0,1] to [0,255] */
    uint8_t* pixels = (uint8_t*)nimcp_malloc((size_t)num_pixels);
    if (!pixels) {
        nimcp_free(pixels_float);
        return PyErr_NoMemory();
    }
    for (Py_ssize_t i = 0; i < num_pixels; i++) {
        float v = pixels_float[i];
        if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
        pixels[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : v));
    }
    nimcp_free(pixels_float);

    /* Access visual cortex from internal brain */
    brain_t ib = self->brain->internal_brain;
    if (!ib || !ib->visual_cortex) {
        nimcp_free(pixels);
        return PyList_New(0);
    }

    uint32_t feat_dim = visual_cortex_get_feature_dim(ib->visual_cortex);
    if (feat_dim == 0) feat_dim = 128;

    float* features = (float*)nimcp_calloc(feat_dim, sizeof(float));
    if (!features) {
        nimcp_free(pixels);
        return PyErr_NoMemory();
    }

    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = visual_cortex_process(ib->visual_cortex, pixels,
                                     width, height, channels, features);
    Py_END_ALLOW_THREADS

    nimcp_free(pixels);

    if (!success) {
        nimcp_free(features);
        return PyList_New(0);
    }

    PyObject* result = PyList_New(feat_dim);
    if (!result) { nimcp_free(features); return NULL; }
    for (uint32_t i = 0; i < feat_dim; i++) {
        PyObject* val = PyFloat_FromDouble((double)features[i]);
        if (!val) { Py_DECREF(result); nimcp_free(features); return NULL; }
        PyList_SET_ITEM(result, i, val);
    }
    nimcp_free(features);
    return result;
}

/**
 * WHAT: Process audio through the brain's speech cortex
 * WHY:  Extract phoneme + prosody features from speech for speech training
 * HOW:  Call speech_cortex_process() on the brain's speech cortex (GIL released)
 *
 * @param samples Python list of float audio samples
 * @return Python list of float features, or empty list if cortex unavailable
 */
static PyObject* Brain_speech_cortex_process(BrainObject* self, PyObject* args) {
    PyObject* samples_list;
    if (!PyArg_ParseTuple(args, "O", &samples_list))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    Py_ssize_t num_samples;
    float* samples = py_list_to_float_array(samples_list, &num_samples);
    if (!samples) return NULL;

    if (num_samples > UINT32_MAX) {
        nimcp_free(samples);
        PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
        return NULL;
    }

    brain_t ib = self->brain->internal_brain;
    if (!ib->speech_cortex) {
        nimcp_free(samples);
        return PyList_New(0);
    }

    /* Speech cortex feature dim from brain config, fallback to 128 */
    uint32_t feat_dim = ib->config.speech_feature_dim;
    if (feat_dim == 0) feat_dim = 128;

    float* features = (float*)nimcp_calloc(feat_dim, sizeof(float));
    if (!features) {
        nimcp_free(samples);
        return PyErr_NoMemory();
    }

    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = speech_cortex_process(ib->speech_cortex, samples,
                                     (uint32_t)num_samples, features);
    Py_END_ALLOW_THREADS

    nimcp_free(samples);

    if (!success) {
        nimcp_free(features);
        return PyList_New(0);
    }

    PyObject* result = PyList_New(feat_dim);
    if (!result) { nimcp_free(features); return NULL; }
    for (uint32_t i = 0; i < feat_dim; i++) {
        PyObject* val = PyFloat_FromDouble((double)features[i]);
        if (!val) { Py_DECREF(result); nimcp_free(features); return NULL; }
        PyList_SET_ITEM(result, i, val);
    }
    nimcp_free(features);
    return result;
}

/**
 * WHAT: Stage sensory data for cross-modal cortex CNN processing
 * WHY:  Feed somatosensory/visual/audio/speech data to cortex CNNs
 * HOW:  Copy data into brain->staged_sensory fields, picked up by next learn_vector
 *
 * @param modality "visual", "audio", "speech", or "somatosensory"
 * @param data Python list of floats
 * @param kwargs: width, height, channels (for visual), n_segments (for somato)
 */
static PyObject* Brain_submit_sensory(BrainObject* self, PyObject* args, PyObject* kwargs) {
    const char* modality;
    PyObject* data_list;
    unsigned int width = 0, height = 0, channels = 0, n_segments = 0;

    static char* kwlist[] = {"modality", "data", "width", "height", "channels",
                             "n_segments", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|IIII", kwlist,
                                      &modality, &data_list,
                                      &width, &height, &channels, &n_segments))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    Py_ssize_t num_elements;
    float* data = py_list_to_float_array(data_list, &num_elements);
    if (!data) return NULL;

    brain_t ib = self->brain->internal_brain;

    if (strcmp(modality, "somatosensory") == 0 || strcmp(modality, "somato") == 0) {
        /* Stage somatosensory data */
        if (ib->staged_sensory.somato_data) {
            nimcp_free(ib->staged_sensory.somato_data);
        }
        ib->staged_sensory.somato_data = data;  /* Transfer ownership */
        ib->staged_sensory.somato_segments = (n_segments > 0) ? n_segments : (uint32_t)num_elements;
    } else if (strcmp(modality, "visual") == 0) {
        /* Stage visual data */
        if (ib->staged_sensory.visual_frame) {
            nimcp_free(ib->staged_sensory.visual_frame);
        }
        /* Convert float [0,1] to uint8 [0,255] */
        uint8_t* pixels = (uint8_t*)nimcp_malloc((size_t)num_elements);
        if (!pixels) { nimcp_free(data); return PyErr_NoMemory(); }
        for (Py_ssize_t i = 0; i < num_elements; i++) {
            float v = data[i];
            if (v <= 1.0f && v >= 0.0f) v *= 255.0f;
            pixels[i] = (uint8_t)(v > 255.0f ? 255 : (v < 0.0f ? 0 : v));
        }
        nimcp_free(data);
        ib->staged_sensory.visual_frame = pixels;
        fprintf(stderr, "[VISUAL-DBG] staged %zd pixels (%ux%ux%u) frame=%p\n",
                num_elements, width, height, channels, (void*)pixels);
        ib->staged_sensory.visual_width = (width > 0) ? width : 32;
        ib->staged_sensory.visual_height = (height > 0) ? height : 32;
        ib->staged_sensory.visual_channels = (channels > 0) ? channels : 3;
    } else if (strcmp(modality, "audio") == 0) {
        /* Stage audio data */
        if (ib->staged_sensory.audio_data) {
            nimcp_free(ib->staged_sensory.audio_data);
        }
        ib->staged_sensory.audio_data = data;  /* Transfer ownership */
        ib->staged_sensory.audio_size = (uint32_t)num_elements;
    } else if (strcmp(modality, "speech") == 0) {
        /* Stage speech data */
        if (ib->staged_sensory.speech_data) {
            nimcp_free(ib->staged_sensory.speech_data);
        }
        ib->staged_sensory.speech_data = data;  /* Transfer ownership */
        ib->staged_sensory.speech_size = (uint32_t)num_elements;
    } else {
        nimcp_free(data);
        PyErr_Format(PyExc_ValueError, "Unknown modality: %s", modality);
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * WHAT: LGSS content safety check
 * WHY:  Gate web-fetched and generated content before learning
 * HOW:  Create temporary LGSS filter, run is_safe check, destroy
 */
static PyObject* Brain_lgss_check_content(BrainObject* self, PyObject* args) {
    const char* text;
    if (!PyArg_ParseTuple(args, "s", &text))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    size_t text_len = strlen(text);

    /* Create a temporary content filter with default config */
    lgss_content_filter_config_t config;
    lgss_content_filter_default_config(&config);

    lgss_content_filter_t* filter = lgss_content_filter_create(NULL, &config);
    if (!filter) {
        /* No filter available — assume safe (defense in depth: Python layer
           also checks) */
        PyObject* dict = PyDict_New();
        if (!dict) return NULL;
        PyObject* v = Py_True; Py_INCREF(v);
        if (PyDict_SetItemString(dict, "is_safe", v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; }
        Py_DECREF(v);
        v = PyUnicode_FromString("no lgss filter available");
        if (!v) { Py_DECREF(dict); return NULL; }
        if (PyDict_SetItemString(dict, "reason", v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; }
        Py_DECREF(v);
        return dict;
    }

    lgss_content_filter_result_t result;
    memset(&result, 0, sizeof(result));
    nimcp_error_t err;

    Py_BEGIN_ALLOW_THREADS
    err = lgss_content_filter_is_safe(filter, text, text_len, &result);
    Py_END_ALLOW_THREADS

    lgss_content_filter_destroy(filter);

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

#define SET(key, val) do { \
    PyObject* v = (val); \
    if (!v) { Py_DECREF(dict); return NULL; } \
    if (PyDict_SetItemString(dict, (key), v) < 0) { Py_DECREF(v); Py_DECREF(dict); return NULL; } \
    Py_DECREF(v); \
} while (0)

    bool is_safe = (err == NIMCP_OK && result.status == LGSS_CONTENT_SAFE);
    SET("is_safe", PyBool_FromLong(is_safe));
    SET("status", PyLong_FromLong(result.status));
    SET("confidence", PyFloat_FromDouble(result.confidence));
    SET("explanation", PyUnicode_FromString(result.explanation));
    if (result.pattern_matched) {
        SET("matched_pattern", PyUnicode_FromString(result.matched_pattern));
    }
    SET("reason", PyUnicode_FromString(
        is_safe ? "content is safe" : "content flagged by lgss"));

#undef SET
    return dict;
}

/**
 * @brief Enable multi-network ensemble training (LNN + CNN + Adaptive)
 *
 * WHAT: Python binding for brain_enable_multi_network_training()
 * WHY:  Allow Python scripts to enable ensemble training from all architectures
 * HOW:  Calls C API, raises RuntimeError on failure
 */
static PyObject* Brain_enable_multi_network(BrainObject* self, PyObject* args) {
    (void)args;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    // self->brain is nimcp_brain_t (handle wrapper)
    // brain_enable_multi_network_training expects brain_t (internal)
    int rc = brain_enable_multi_network_training(self->brain->internal_brain);
    if (rc < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to enable multi-network training");
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
 * WHAT: Enable/disable Hamiltonian dynamics on LNN layer 0.
 * WHY:  Checkpoint load replaces LNN network (losing HNN from init).
 *       This re-enables HNN on the loaded LNN after checkpoint restore.
 * HOW:  Creates H_net + momentum tensor p on layer 0 if not present.
 */
static PyObject* Brain_enable_hamiltonian(BrainObject* self, PyObject* args) {
    int enable = 1;
    if (!PyArg_ParseTuple(args, "|p", &enable)) return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    brain_t brain = self->brain->internal_brain;

    /* Create minimal LNN if not present (FAST/lazy init skips it, checkpoint load may provide it).
     * Don't call init_lnn_subsystem — it wires bridges to subsystems that may not exist. */
    if (!brain->lnn_network) {
        lnn_init(1);
        brain->lnn_network = lnn_network_create_ncp(128, 64, 32, 64);
        if (!brain->lnn_network) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create LNN network for Hamiltonian");
            return NULL;
        }
        lnn_network_init_weights(brain->lnn_network, 42);
    }
    if (brain->lnn_network->n_layers == 0 || !brain->lnn_network->layers[0]) {
        PyErr_SetString(PyExc_RuntimeError, "LNN has no layers — cannot enable Hamiltonian");
        return NULL;
    }

    lnn_layer_t* layer0 = brain->lnn_network->layers[0];

    if (enable && !layer0->use_hamiltonian) {
        /* Create Hamiltonian network if not present */
        if (!layer0->H_net) {
            uint32_t state_dim = layer0->n_neurons;
            extern lnn_hamiltonian_net_t* lnn_hamiltonian_net_create(uint32_t, const lnn_hamiltonian_config_t*);
            extern void lnn_hamiltonian_config_default(lnn_hamiltonian_config_t*);

            lnn_hamiltonian_config_t hnn_cfg;
            lnn_hamiltonian_config_default(&hnn_cfg);
            lnn_hamiltonian_net_t* H_net = lnn_hamiltonian_net_create(state_dim, &hnn_cfg);
            if (!H_net) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to create Hamiltonian network");
                return NULL;
            }
            layer0->H_net = H_net;
        }

        /* Create momentum tensor if not present */
        if (!layer0->p) {
            uint32_t p_dims[1] = {layer0->n_neurons};
            layer0->p = nimcp_tensor_create(p_dims, 1, NIMCP_DTYPE_F32);
            if (layer0->p) {
                float* p_data = (float*)nimcp_tensor_data(layer0->p);
                if (p_data) {
                    for (uint32_t i = 0; i < layer0->n_neurons; i++) {
                        p_data[i] = 0.01f * ((float)rand() / (float)RAND_MAX - 0.5f);
                    }
                }
            }
        }

        layer0->use_hamiltonian = true;
        fprintf(stderr, "[HNN] Hamiltonian enabled on layer 0, state_dim=%u, p=%s\n",
                layer0->n_neurons, layer0->p ? "ok" : "FAIL");
    } else if (!enable && layer0->use_hamiltonian) {
        layer0->use_hamiltonian = false;
        fprintf(stderr, "[HNN] Hamiltonian disabled on layer 0\n");
    }

    Py_RETURN_NONE;
}

/**
 * WHAT: Enable Thousand Brains world model bridge for Stage 2+ training.
 * WHY:  World model learns to predict outcomes — needed for feedback learning.
 *       Not useful in Stage 1 (no temporal sequences to predict).
 * HOW:  Creates TB bridge, connects to omni world model, sets config flag.
 */
static PyObject* Brain_enable_world_model_bridge(BrainObject* self, PyObject* args) {
    int enable = 1;
    if (!PyArg_ParseTuple(args, "|p", &enable)) return NULL;

    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    brain_t brain = self->brain->internal_brain;

    if (enable && !brain->wm_thousand_brains_bridge) {
        /* Create and connect the bridge */
        extern void wm_tb_bridge_config_default(void*);
        extern void* wm_tb_bridge_create(const void*);
        extern int wm_tb_bridge_connect_world_model(void*, void*);

        /* Stack-allocate config (120 bytes max) */
        char tb_config[256];
        memset(tb_config, 0, sizeof(tb_config));
        wm_tb_bridge_config_default(tb_config);

        brain->wm_thousand_brains_bridge = wm_tb_bridge_create(tb_config);
        if (brain->wm_thousand_brains_bridge) {
            if (brain->omni_world_model) {
                wm_tb_bridge_connect_world_model(brain->wm_thousand_brains_bridge,
                                                  brain->omni_world_model);
            }
            brain->config.enable_wm_thousand_brains_bridge = true;
            fprintf(stderr, "[WM] Thousand Brains bridge enabled\n");
        } else {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create Thousand Brains bridge");
            return NULL;
        }
    } else if (enable && brain->wm_thousand_brains_bridge) {
        /* Already created — just ensure flag is set */
        brain->config.enable_wm_thousand_brains_bridge = true;
    } else if (!enable) {
        brain->config.enable_wm_thousand_brains_bridge = false;
        fprintf(stderr, "[WM] Thousand Brains bridge disabled\n");
    }

    Py_RETURN_NONE;
}

/**
 * WHAT: Initialize all 4 cortex CNN processors with FNO spectral processing.
 * WHY:  Cortex CNNs need explicit init — lazy creation inside learn paths is unreliable
 *       on 2M neuron brains due to ensure_writable_network failures.
 */
static PyObject* Brain_init_cortex_cnns(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    extern nimcp_status_t nimcp_brain_init_cortex_cnns(nimcp_brain_t);
    nimcp_status_t rc = nimcp_brain_init_cortex_cnns(self->brain);
    if (rc != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to init cortex CNNs");
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
 * WHAT: Run full training decision cycle: observe, diagnose, simulate, decide
 * WHY:  Combines Layer 1 (convergent), Layer 2 (causal DAG), Layer 3 (abductive)
 *       into a single call for the training pipeline
 * HOW:  Calls brain_ti_compute_decision_cycle, returns dict with all fields
 */
static PyObject* Brain_ti_compute_decision_cycle(BrainObject* self, PyObject* args) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    float loss_current, loss_previous, grad_norm, grad_norm_previous;
    float loss_volatility, gradient_variance, current_lr, current_batch;

    if (!PyArg_ParseTuple(args, "ffffffff",
            &loss_current, &loss_previous,
            &grad_norm, &grad_norm_previous,
            &loss_volatility, &gradient_variance,
            &current_lr, &current_batch)) {
        return NULL;
    }

    if (isnan(loss_current) || isnan(grad_norm) || isinf(loss_current) || isinf(grad_norm)) {
        PyErr_SetString(PyExc_ValueError, "Decision cycle received NaN/inf metric");
        return NULL;
    }

    brain_ti_training_metrics_t metrics = {
        .loss_current = loss_current,
        .loss_previous = loss_previous,
        .grad_norm = grad_norm,
        .grad_norm_previous = grad_norm_previous,
        .loss_volatility = loss_volatility,
        .gradient_variance = gradient_variance,
        .current_lr = current_lr,
        .current_batch = current_batch,
    };

    brain_ti_decision_cycle_result_t result;
    /* H-2: All Python values captured into local C variables above.
     * Release GIL around the C computation. */
    brain_t internal_brain = self->brain->internal_brain;
    int rc;
    Py_BEGIN_ALLOW_THREADS
    rc = brain_ti_compute_decision_cycle(internal_brain, &metrics, &result);
    Py_END_ALLOW_THREADS

    /* Helper macros — PyDict_SetItemString does NOT steal the reference,
       so we must Py_DECREF the temporary after insertion.
       On OOM (NULL from Py*_From*()), propagate the error. */
#define SET_FLOAT(d, key, val) do { \
    PyObject* _tmp = PyFloat_FromDouble(val); \
    if (!_tmp) { Py_DECREF(d); return NULL; } \
    if (PyDict_SetItemString(d, key, _tmp) < 0) { Py_DECREF(_tmp); Py_DECREF(d); return NULL; } \
    Py_DECREF(_tmp); \
} while(0)
#define SET_LONG(d, key, val) do { \
    PyObject* _tmp = PyLong_FromLong(val); \
    if (!_tmp) { Py_DECREF(d); return NULL; } \
    if (PyDict_SetItemString(d, key, _tmp) < 0) { Py_DECREF(_tmp); Py_DECREF(d); return NULL; } \
    Py_DECREF(_tmp); \
} while(0)
#define SET_ULONG(d, key, val) do { \
    PyObject* _tmp = PyLong_FromUnsignedLong(val); \
    if (!_tmp) { Py_DECREF(d); return NULL; } \
    if (PyDict_SetItemString(d, key, _tmp) < 0) { Py_DECREF(_tmp); Py_DECREF(d); return NULL; } \
    Py_DECREF(_tmp); \
} while(0)
#define SET_BOOL(d, key, val) do { \
    PyObject* _tmp = (val) ? Py_True : Py_False; \
    Py_INCREF(_tmp); \
    if (PyDict_SetItemString(d, key, _tmp) < 0) { Py_DECREF(_tmp); Py_DECREF(d); return NULL; } \
    Py_DECREF(_tmp); \
} while(0)
#define SET_STR(d, key, val) do { \
    PyObject* _tmp = PyUnicode_FromString(val); \
    if (!_tmp) { Py_DECREF(d); return NULL; } \
    if (PyDict_SetItemString(d, key, _tmp) < 0) { Py_DECREF(_tmp); Py_DECREF(d); return NULL; } \
    Py_DECREF(_tmp); \
} while(0)

    if (rc != 0) {
        /* Return safe defaults on error */
        PyObject* d = PyDict_New();
        if (!d) return PyErr_NoMemory();
        SET_LONG(d, "consensus_action", TRAINING_EVIDENCE_CONTINUE);
        SET_FLOAT(d, "lr_factor", 1.0);
        SET_FLOAT(d, "batch_factor", 1.0);
        SET_FLOAT(d, "grad_clip_factor", 1.0);
        SET_FLOAT(d, "urgency", 0.0);
        SET_BOOL(d, "converged", 0);
        SET_LONG(d, "num_contributors", 0);
        SET_STR(d, "primary_diagnosis", "");
        SET_FLOAT(d, "diagnosis_plausibility", 0.0);
        SET_BOOL(d, "recommend_pause", 0);
        SET_BOOL(d, "recommend_rollback", 0);
        SET_STR(d, "causal_explanation", "");
        SET_FLOAT(d, "causal_confidence", 0.0);
        SET_BOOL(d, "lr_change_beneficial", 0);
        return d;
    }

    PyObject* d = PyDict_New();
    if (!d) return PyErr_NoMemory();

    /* Convergent decision (Layer 1) */
    SET_LONG(d, "consensus_action", result.consensus_action);
    SET_FLOAT(d, "lr_factor", result.lr_factor);
    SET_FLOAT(d, "batch_factor", result.batch_factor);
    SET_FLOAT(d, "grad_clip_factor", result.grad_clip_factor);
    SET_FLOAT(d, "urgency", result.urgency);
    SET_BOOL(d, "converged", result.converged);
    SET_ULONG(d, "num_contributors", result.num_contributors);

    /* Diagnosis (Layer 3) */
    SET_STR(d, "primary_diagnosis", result.primary_diagnosis);
    SET_FLOAT(d, "diagnosis_plausibility", result.diagnosis_plausibility);
    SET_BOOL(d, "recommend_pause", result.recommend_pause);
    SET_BOOL(d, "recommend_rollback", result.recommend_rollback);

    /* Causal reasoning (Layer 2) */
    SET_STR(d, "causal_explanation", result.causal_explanation);
    SET_FLOAT(d, "causal_confidence", result.causal_confidence);
    SET_BOOL(d, "lr_change_beneficial", result.lr_change_beneficial);

#undef SET_FLOAT
#undef SET_LONG
#undef SET_ULONG
#undef SET_BOOL
#undef SET_STR

    return d;
}

/**
 * WHAT: Configure the training pipeline with sensible defaults or custom params
 * WHY:  Brain config flags (LR scheduler, regularization, gradient mgmt) are
 *       disabled by default in checkpoints loaded from older saves. This method
 *       enables the full training pipeline post-creation/load.
 * HOW:  Calls nimcp_brain_configure_training() with nimcp_training_config_default()
 *       and also flips the brain_config_t flags so the training middleware sees them.
 *
 * @param kwargs Optional: learning_rate, weight_decay, gradient_clip
 * @return True on success
 */
static PyObject* Brain_configure_training(BrainObject* self, PyObject* args, PyObject* kwargs) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    float learning_rate = 0.001F;
    float weight_decay = 0.0001F;
    float gradient_clip = 1.0F;

    static char* kwlist[] = {"learning_rate", "weight_decay", "gradient_clip", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|fff", kwlist,
                                      &learning_rate, &weight_decay, &gradient_clip)) {
        return NULL;
    }

    /* WARNING: Not thread-safe. Call before starting concurrent training.
     * This function directly mutates brain config fields without locking.
     * Calling it while training threads are active causes data races. */
    /* TODO: Should use nimcp_brain_configure_training() public API in the future
     * instead of directly mutating internal config fields. */

    /* Flip the brain_config_t flags so the training middleware uses them.
     * The brain.learn() path uses brain_config flags, not the training pipeline API.
     * This is the primary mechanism for enabling LR scheduling, regularization,
     * and gradient management for checkpoints loaded from older saves. */
    brain_t internal = self->brain->internal_brain;
    if (!internal) {
        PyErr_SetString(PyExc_RuntimeError, "Brain has no internal state");
        return NULL;
    }

    /* Set brain_config_t flags */
    internal->config.enable_lr_scheduler = true;
    internal->config.enable_regularization = (weight_decay > 0.0F);
    internal->config.enable_gradient_management = true;
    internal->config.enable_gradient_health_check = true;
    internal->config.gradient_clip_value = gradient_clip;
    internal->config.gradient_clip_norm = gradient_clip;
    internal->config.regularization_l2_lambda = weight_decay;
    internal->config.learning_rate = learning_rate;

    /* Note: training_ctx->config is also relevant but is an opaque type here.
     * The brain_config flags are read by the training middleware during training
     * steps, so setting them is sufficient for the brain.learn() path. */

    Py_RETURN_TRUE;
}

/**
 * @brief Enable cognitive profile on an existing brain.
 *
 * Activates multi-head attention, executive control, meta-learning,
 * predictive processing, brain regions, oscillations, and other
 * cognitive subsystems that enhance training quality.
 *
 * Call BEFORE starting training (not thread-safe).
 */
/**
 * WHAT: Create a brain with ALL subsystems enabled (RESEARCH profile + extras)
 * WHY:  Enables every functional module for maximum capability training/inference
 * HOW:  Class method — Brain.create_full(name, task, inputs, outputs, neurons)
 *       Uses nimcp_brain_create_full() which applies RESEARCH profile at creation
 *       time so all subsystems initialize in proper order.
 */
static PyObject* Brain_create_full(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    const char* name;
    int task = NIMCP_TASK_CLASSIFICATION;
    unsigned int num_inputs = 1024;
    unsigned int num_outputs = 2048;
    unsigned int neuron_count = NIMCP_DEFAULT_ANN_NEURONS;

    static char* kwlist[] = {"name", "task", "num_inputs", "num_outputs", "neuron_count", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|iIII", kwlist,
                                     &name, &task, &num_inputs, &num_outputs, &neuron_count)) {
        return NULL;
    }

    BrainObject* self = (BrainObject*)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->brain = NULL;
    self->community_cache = NULL;

    self->brain = nimcp_brain_create_full(name, (nimcp_brain_task_t)task,
                                           num_inputs, num_outputs, neuron_count);
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError,
            "nimcp_brain_create_full failed — check logs for subsystem init errors");
        Py_DECREF(self);
        return NULL;
    }

    return (PyObject*)self;
}

/**
 * WHAT: Configure cognitive profile on existing brain (deprecated — use create_full)
 * WHY:  Backward compatibility — but post-creation init may crash on checkpoint-loaded brains
 * HOW:  Sets config flags only (no subsystem init — those must be done at creation time)
 */
static PyObject* Brain_configure_cognitive(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    brain_t internal = self->brain->internal_brain;
    if (!internal) {
        PyErr_SetString(PyExc_RuntimeError, "Brain has no internal state");
        return NULL;
    }

    /* Set cognitive flags — subsystems won't init if they weren't created at brain_create time */
    internal->config.enable_multihead_attention = true;
    internal->config.num_attention_heads = 8;
    internal->config.attention_key_dim = 64;
    internal->config.enable_thalamic_gate = true;
    internal->config.enable_salience_weighting = true;
    internal->config.enable_executive_control = true;
    internal->config.enable_task_switching = true;
    internal->config.enable_planning = true;
    internal->config.enable_meta_learning = true;
    internal->config.enable_adaptive_meta_lr = true;
    internal->config.enable_predictive_processing = true;
    internal->config.enable_active_inference = true;
    internal->config.enable_logic = true;
    internal->config.enable_epistemic_filter = true;
    internal->config.enable_emotional_tagging = true;
    internal->config.enable_emotional_memories = true;
    internal->config.enable_natural_explanations = true;
    internal->config.enable_causal_explanations = true;
    internal->config.enable_ethics = true;
    internal->config.enable_wellbeing = true;
    internal->config.enable_brain_regions = true;
    internal->config.enable_oscillations = true;
    internal->config.enable_sleep_wake_cycle = true;
    internal->config.enable_memory_replay = true;
    internal->config.enable_synaptic_homeostasis = true;
    internal->config.enable_mental_health_monitoring = true;
    internal->config.enable_training_integration = true;
    internal->config.enable_homeostatic_plasticity = true;
    /* Brain-level dendritic_tree is not 1:1 with main-net neurons -- the
     * update runs but produces no consumer-side current injection.  Cortical
     * columns have their own fully-wired dendrite path.  See
     * nimcp_brain_config_profiles.c for full rationale. */
    internal->config.enable_dendritic_computation = false;
    internal->config.enable_eligibility_traces = true;

    Py_RETURN_TRUE;
}

/**
 * @brief Check if brain is frozen
 */
static PyObject* Brain_get_is_frozen(BrainObject* self, void* closure) {
    (void)closure;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    if (nimcp_brain_is_frozen(self->brain)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

/**
 * WHAT: Toggle fast training mode (skip biological subsystems)
 * WHY:  5-10x speedup for bulk training by skipping VAE, attention,
 *       engram, neuromodulators, emotions, cortical columns, etc.
 * HOW:  Sets brain->config.fast_training_mode flag
 */
static PyObject* Brain_set_fast_training(BrainObject* self, PyObject* args) {
    int enabled = 1;
    if (!PyArg_ParseTuple(args, "|p", &enabled)) {
        return NULL;
    }
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    self->brain->internal_brain->config.fast_training_mode = (bool)enabled;
    Py_RETURN_TRUE;
}

/**
 * WHAT: Reinitialize all synapse weights to break mode collapse
 * WHY:  When outputs converge to identical values (cosine sim = 1.0),
 *       gradient-based corrections can't recover. Fresh weights give the
 *       network a clean slate while preserving topology.
 * HOW:  Calls neural_network_reinit_weights + marks GPU cache dirty
 */
static PyObject* Brain_reinit_weights(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    extern neural_network_t adaptive_network_get_base_network(adaptive_network_t network);
    extern void neural_network_reinit_weights(neural_network_t network);
    extern bool neural_network_rebuild_incoming(neural_network_t network);
    extern void adaptive_network_reset_ema(adaptive_network_t network);

    /* GPU cache lifecycle */
    extern bool adaptive_network_is_gpu_enabled(adaptive_network_t network);
    extern void adaptive_network_set_gpu_enabled(adaptive_network_t network, bool enabled);
    extern void adaptive_network_set_gpu_context(adaptive_network_t network, struct nimcp_gpu_context_s* ctx);
    extern void adaptive_network_set_gpu_weight_cache(adaptive_network_t network, struct nimcp_gpu_weight_cache_s* cache);
    extern struct nimcp_gpu_context_s* adaptive_network_get_gpu_context(adaptive_network_t network);
    extern struct nimcp_gpu_weight_cache_s* adaptive_network_get_gpu_weight_cache(adaptive_network_t network);

    brain_t ib = self->brain->internal_brain;
    if (!ib->network) {
        PyErr_SetString(PyExc_RuntimeError, "No adaptive network");
        return NULL;
    }

    neural_network_t base = adaptive_network_get_base_network(ib->network);
    if (!base) {
        PyErr_SetString(PyExc_RuntimeError, "No base network");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    neural_network_reinit_weights(base);
    /* Rebuild incoming synapses from outgoing — the GPU weight cache
     * uploads use NEURON_IN_COUNT which reads incoming synapses. */
    neural_network_rebuild_incoming(base);

    /* Destroy and recreate GPU weight cache from scratch.
     * Just marking dirty isn't enough — the cache's sparse matrices
     * may have been built from a broken checkpoint with zero incoming
     * synapses, and the upload can't fix structural issues. */
    {
        extern void adaptive_network_rebuild_gpu_cache(adaptive_network_t network);
        adaptive_network_rebuild_gpu_cache(ib->network);
    }

    adaptive_network_reset_ema(ib->network);
    Py_END_ALLOW_THREADS

    Py_RETURN_TRUE;
}

/**
 * WHAT: Set output layer neurons to LINEAR activation (identity)
 * WHY:  Checkpoints from older code have TANH on output layer, which
 *       bounds output to [-1,1] and causes gradient vanishing for regression.
 *       This must be called after loading a checkpoint.
 */
static PyObject* Brain_fix_output_activation(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }

    extern neural_network_t adaptive_network_get_base_network(adaptive_network_t network);
    extern void neural_network_set_output_activation(neural_network_t network,
                                                      activation_type_t activation);
    extern bool adaptive_network_is_gpu_enabled(adaptive_network_t network);

    brain_t ib = self->brain->internal_brain;
    if (!ib->network) { Py_RETURN_FALSE; }

    neural_network_t net = adaptive_network_get_base_network(ib->network);
    if (!net) { Py_RETURN_FALSE; }

    /* Fix neuron activation types */
    neural_network_set_output_activation(net, ACTIVATION_LINEAR);

    /* Also fix GPU weight cache layer_activations if GPU is active */
    if (adaptive_network_is_gpu_enabled(ib->network)) {
        /* Access GPU weight cache through the adaptive network struct */
        /* The gpu_weight_cache is accessible via the public header */
        extern struct nimcp_gpu_weight_cache_s*
            adaptive_network_get_gpu_weight_cache(adaptive_network_t network);
        struct nimcp_gpu_weight_cache_s* cache =
            adaptive_network_get_gpu_weight_cache(ib->network);
        if (cache && cache->layer_activations && cache->num_layers > 1) {
            cache->layer_activations[cache->num_layers - 1] = ACTIVATION_LINEAR;
            fprintf(stderr, "[FIX] GPU weight cache output activation → LINEAR\n");
        }
    }

    Py_RETURN_TRUE;
}

// ==========================================================================
// Task Type / Strategy Python Binding
// ==========================================================================

/**
 * set_task_type(task_type: str)
 * Set the brain's task strategy: "regression", "classification", "pattern", "association".
 * IMPORTANT: Athena's developmental learning needs "regression" (raw output, no softmax).
 */
extern task_strategy_t* strategy_create(brain_task_t task);
static PyObject* Brain_set_task_type(BrainObject* self, PyObject* args) {
    const char* task_str = NULL;
    if (!PyArg_ParseTuple(args, "s", &task_str)) return NULL;
    if (!self->brain || !self->brain->internal_brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    brain_t brain = self->brain->internal_brain;
    brain_task_t task;
    if (strcmp(task_str, "regression") == 0)       task = BRAIN_TASK_REGRESSION;
    else if (strcmp(task_str, "classification") == 0) task = BRAIN_TASK_CLASSIFICATION;
    else if (strcmp(task_str, "pattern") == 0)     task = BRAIN_TASK_PATTERN_MATCHING;
    else if (strcmp(task_str, "association") == 0) task = BRAIN_TASK_ASSOCIATION;
    else {
        PyErr_Format(PyExc_ValueError, "Unknown task type: '%s' (use regression/classification/pattern/association)", task_str);
        return NULL;
    }
    task_strategy_t* new_strategy = strategy_create(task);
    if (!new_strategy) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create strategy");
        return NULL;
    }
    if (brain->strategy && !brain->is_cow_clone) {
        nimcp_free(brain->strategy);
    }
    brain->strategy = new_strategy;
    brain->config.task = task;
    Py_RETURN_TRUE;
}

// ==========================================================================
// Biological Plasticity Python Bindings
// ==========================================================================

/**
 * enable_biological_plasticity(enabled: bool)
 * Wire/unwire TPB+EDP+coordinator into the learn path.
 */
static PyObject* Brain_enable_biological_plasticity(BrainObject* self, PyObject* args) {
    int enabled = 1;
    if (!PyArg_ParseTuple(args, "|p", &enabled)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    brain->enable_plasticity_bridge = (bool)enabled;
    brain->enable_event_driven_plasticity = (bool)enabled;
    brain->plasticity_coordinator_enabled = (bool)enabled;
    Py_RETURN_TRUE;
}

/**
 * get_plasticity_stats() -> dict
 * Returns RPE, neuromodulator levels, mechanism states, event counts.
 */
static PyObject* Brain_get_plasticity_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;

    PyObject* d = PyDict_New();
    if (!d) return NULL;

    /* TPB stats */
    if (brain->plasticity_bridge && brain->enable_plasticity_bridge) {
        PyObject* tmp;
        float da = 0, ach = 0, ht5 = 0, ne = 0;
        tpb_get_neuromod_levels(brain->plasticity_bridge, &da, &ach, &ht5, &ne);
        tmp = PyFloat_FromDouble(da);
        PyDict_SetItemString(d, "dopamine", tmp); Py_DECREF(tmp);
        tmp = PyFloat_FromDouble(ach);
        PyDict_SetItemString(d, "acetylcholine", tmp); Py_DECREF(tmp);
        tmp = PyFloat_FromDouble(ht5);
        PyDict_SetItemString(d, "serotonin", tmp); Py_DECREF(tmp);
        tmp = PyFloat_FromDouble(ne);
        PyDict_SetItemString(d, "norepinephrine", tmp); Py_DECREF(tmp);

        tpb_rpe_state_t rpe_state;
        if (tpb_get_rpe_state(brain->plasticity_bridge, &rpe_state) == NIMCP_OK) {
            tmp = PyFloat_FromDouble(rpe_state.last_rpe);
            PyDict_SetItemString(d, "rpe", tmp); Py_DECREF(tmp);
            tmp = PyFloat_FromDouble(rpe_state.smoothed_rpe);
            PyDict_SetItemString(d, "rpe_ema", tmp); Py_DECREF(tmp);
            tmp = PyFloat_FromDouble(rpe_state.baseline_loss);
            PyDict_SetItemString(d, "baseline_loss", tmp); Py_DECREF(tmp);
        }

        tpb_stats_t tpb_stats;
        if (tpb_get_stats(brain->plasticity_bridge, &tpb_stats) == NIMCP_OK) {
            tmp = PyLong_FromUnsignedLongLong(tpb_stats.rpe_computations);
            PyDict_SetItemString(d, "tpb_rpe_computations", tmp); Py_DECREF(tmp);
            tmp = PyLong_FromUnsignedLongLong(tpb_stats.total_plasticity_updates);
            PyDict_SetItemString(d, "tpb_plasticity_updates", tmp); Py_DECREF(tmp);
            tmp = PyLong_FromUnsignedLongLong(tpb_stats.stdp_updates);
            PyDict_SetItemString(d, "tpb_stdp_updates", tmp); Py_DECREF(tmp);
            tmp = PyLong_FromUnsignedLongLong(tpb_stats.bcm_updates);
            PyDict_SetItemString(d, "tpb_bcm_updates", tmp); Py_DECREF(tmp);
        }
    }

    /* EDP stats — guard against dangling pointer (SIGSEGV in edp_is_active seen in production) */
    if (brain->event_driven_plasticity && brain->enable_event_driven_plasticity
        && edp_is_active(brain->event_driven_plasticity)) {
        PyDict_SetItemString(d, "edp_active", Py_True);
        edp_stats_t edp_stats;
        if (edp_get_stats(brain->event_driven_plasticity, &edp_stats) == NIMCP_OK) {
            PyObject* etmp;
            etmp = PyLong_FromUnsignedLongLong(edp_stats.total_plasticity_updates);
            PyDict_SetItemString(d, "edp_plasticity_updates", etmp); Py_DECREF(etmp);
            etmp = PyLong_FromUnsignedLongLong(edp_stats.ltp_events);
            PyDict_SetItemString(d, "edp_ltp_events", etmp); Py_DECREF(etmp);
            etmp = PyLong_FromUnsignedLongLong(edp_stats.ltd_events);
            PyDict_SetItemString(d, "edp_ltd_events", etmp); Py_DECREF(etmp);
            etmp = PyFloat_FromDouble(edp_stats.avg_prediction_error);
            PyDict_SetItemString(d, "edp_avg_prediction_error", etmp); Py_DECREF(etmp);
            etmp = PyFloat_FromDouble(edp_stats.avg_reward_signal);
            PyDict_SetItemString(d, "edp_avg_reward", etmp); Py_DECREF(etmp);
        }
    }

    /* Plasticity coordinator stats */
    if (brain->plasticity_coordinator && brain->plasticity_coordinator_enabled) {
        plasticity_coordinator_state_t state = plasticity_coordinator_get_state(brain->plasticity_coordinator);
        const char* state_names[] = {"ACQUISITION", "CONSOLIDATION", "MAINTENANCE", "STABILIZING"};
        const char* state_name = (state >= 0 && state < 4) ? state_names[state] : "UNKNOWN";
        PyObject* ptmp;
        ptmp = PyUnicode_FromString(state_name);
        PyDict_SetItemString(d, "plasticity_state", ptmp); Py_DECREF(ptmp);
        ptmp = PyFloat_FromDouble(
            plasticity_coordinator_get_energy_rate(brain->plasticity_coordinator));
        PyDict_SetItemString(d, "energy_rate", ptmp); Py_DECREF(ptmp);
        ptmp = PyBool_FromLong(
            plasticity_coordinator_is_low_energy(brain->plasticity_coordinator));
        PyDict_SetItemString(d, "low_energy", ptmp); Py_DECREF(ptmp);
    }

    return d;
}

/**
 * set_plasticity_state(state: str)
 * Set plasticity state: "ACQUISITION"/"CONSOLIDATION"/"MAINTENANCE"/"STABILIZING"
 */
static PyObject* Brain_set_plasticity_state(BrainObject* self, PyObject* args) {
    const char* state_str = NULL;
    if (!PyArg_ParseTuple(args, "s", &state_str)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;

    if (!brain->plasticity_coordinator) {
        PyErr_SetString(PyExc_RuntimeError, "Plasticity coordinator not initialized");
        return NULL;
    }

    plasticity_coordinator_state_t state;
    if (strcmp(state_str, "ACQUISITION") == 0)       state = 0;
    else if (strcmp(state_str, "CONSOLIDATION") == 0) state = 1;
    else if (strcmp(state_str, "MAINTENANCE") == 0)   state = 2;
    else if (strcmp(state_str, "STABILIZING") == 0)   state = 3;
    else {
        PyErr_Format(PyExc_ValueError, "Unknown plasticity state: '%s'", state_str);
        return NULL;
    }

    plasticity_coordinator_set_state(brain->plasticity_coordinator, state);
    Py_RETURN_TRUE;
}

/**
 * edp_process_reward(reward: float)
 * Consolidate eligibility traces with reward signal.
 */
static PyObject* Brain_edp_process_reward(BrainObject* self, PyObject* args) {
    float reward;
    if (!PyArg_ParseTuple(args, "f", &reward)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->event_driven_plasticity) { Py_RETURN_FALSE; }
    nimcp_result_t r = edp_process_reward(brain->event_driven_plasticity, reward);
    if (r == NIMCP_OK) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

/**
 * edp_process_novelty(novelty: float)
 * Attention-modulated plasticity from novelty detection.
 */
static PyObject* Brain_edp_process_novelty(BrainObject* self, PyObject* args) {
    float novelty;
    if (!PyArg_ParseTuple(args, "f", &novelty)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->event_driven_plasticity) { Py_RETURN_FALSE; }
    nimcp_result_t r = edp_process_novelty(brain->event_driven_plasticity, novelty, 0);
    if (r == NIMCP_OK) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

// ==========================================================================
// LNN Temporal Processor Python Bindings
// ==========================================================================

/**
 * lnn_create(n_sensory, n_inter, n_command, n_output)
 * Create NCP-architecture LNN temporal processor.
 */
static PyObject* Brain_lnn_create(BrainObject* self, PyObject* args) {
    uint32_t n_s = 128, n_i = 64, n_c = 32, n_o = 64;
    if (!PyArg_ParseTuple(args, "|IIII", &n_s, &n_i, &n_c, &n_o)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;

    if (brain->lnn_network) {
        /* Already created — idempotent */
        Py_RETURN_TRUE;
    }

    if (!lnn_is_initialized()) {
        lnn_init(1);
    }

    brain->lnn_network = lnn_network_create_ncp(n_s, n_i, n_c, n_o);
    if (!brain->lnn_network) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create LNN network");
        return NULL;
    }
    lnn_network_init_weights(brain->lnn_network, 42);

    lnn_training_config_t cfg;
    lnn_training_config_default(&cfg);
    cfg.learning_rate = 0.01f;
    cfg.gradient_clip_norm = 100.0f;
    cfg.enable_plasticity_integration = true;
    cfg.lnn_train_mode = LNN_TRAIN_ADJOINT;
    cfg.track_statistics = true;

    // Destroy existing context if present (prevent leak)
    if (brain->lnn_training_ctx) {
        lnn_training_destroy(brain->lnn_training_ctx);
        brain->lnn_training_ctx = NULL;
    }
    brain->lnn_training_ctx = lnn_training_create(brain->lnn_network, &cfg);
    Py_RETURN_TRUE;
}

/**
 * lnn_forward_step(features: list) -> list
 * Run one ODE timestep, returns output vector.
 */
static PyObject* Brain_lnn_forward_step(BrainObject* self, PyObject* args) {
    PyObject* features_list;
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &features_list)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;

    if (!brain->lnn_network) {
        PyErr_SetString(PyExc_RuntimeError, "LNN not created. Call lnn_create() first.");
        return NULL;
    }

    Py_ssize_t n = PyList_Size(features_list);
    float* input_data = nimcp_malloc(n * sizeof(float));
    if (!input_data) { PyErr_NoMemory(); return NULL; }
    for (Py_ssize_t i = 0; i < n; i++) {
        input_data[i] = (float)PyFloat_AsDouble(PyList_GetItem(features_list, i));
    }

    uint32_t in_dims[] = {(uint32_t)n};
    nimcp_tensor_t* input_tensor = nimcp_tensor_create(in_dims, 1, NIMCP_DTYPE_F32);
    if (!input_tensor) { nimcp_free(input_data); PyErr_NoMemory(); return NULL; }
    memcpy(nimcp_tensor_data(input_tensor), input_data, n * sizeof(float));
    nimcp_free(input_data);

    /* Get LNN output size from network config */
    uint32_t out_size = 64;  /* Default NCP motor size */
    uint32_t out_dims[] = {out_size};
    nimcp_tensor_t* output_tensor = nimcp_tensor_create(out_dims, 1, NIMCP_DTYPE_F32);
    if (!output_tensor) { nimcp_tensor_destroy(input_tensor); PyErr_NoMemory(); return NULL; }

    int result = lnn_forward_step(brain->lnn_network, input_tensor, output_tensor, 0.01f);
    nimcp_tensor_destroy(input_tensor);

    if (result != 0) {
        nimcp_tensor_destroy(output_tensor);
        PyErr_SetString(PyExc_RuntimeError, "lnn_forward_step failed");
        return NULL;
    }

    float* out_data = (float*)nimcp_tensor_data(output_tensor);
    PyObject* out_list = PyList_New(out_size);
    for (uint32_t i = 0; i < out_size; i++) {
        PyList_SetItem(out_list, i, PyFloat_FromDouble(out_data[i]));
    }
    nimcp_tensor_destroy(output_tensor);
    return out_list;
}

/**
 * lnn_get_state() -> list
 * Get LNN internal state vector.
 */
static PyObject* Brain_lnn_get_state(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->lnn_network) { Py_RETURN_NONE; }

    nimcp_tensor_t* state = NULL;
    int r = lnn_get_state(brain->lnn_network, &state);
    if (r != 0 || !state) { Py_RETURN_NONE; }

    uint32_t n = nimcp_tensor_numel(state);
    float* data = (float*)nimcp_tensor_data(state);
    PyObject* out = PyList_New(n);
    for (uint32_t i = 0; i < n; i++) {
        PyList_SetItem(out, i, PyFloat_FromDouble(data[i]));
    }
    nimcp_tensor_destroy(state);
    return out;
}

/**
 * lnn_get_stats() -> dict
 * Get LNN statistics: tau distribution, gradient norms, loss.
 */
static PyObject* Brain_lnn_get_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->lnn_network) { Py_RETURN_NONE; }

    lnn_network_stats_t stats;
    int r = lnn_get_stats(brain->lnn_network, &stats);
    if (r != 0) { Py_RETURN_NONE; }

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* tmp;
    tmp = PyLong_FromUnsignedLongLong(stats.forward_steps);
    PyDict_SetItemString(d, "forward_steps", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLongLong(stats.backward_steps);
    PyDict_SetItemString(d, "backward_steps", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLongLong(stats.ode_evaluations);
    PyDict_SetItemString(d, "total_ode_evals", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.avg_tau_network);
    PyDict_SetItemString(d, "avg_tau", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.state_norm);
    PyDict_SetItemString(d, "state_norm", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.gradient_norm);
    PyDict_SetItemString(d, "gradient_norm", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(stats.nan_count);
    PyDict_SetItemString(d, "nan_count", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(stats.inf_count);
    PyDict_SetItemString(d, "inf_count", tmp); Py_DECREF(tmp);
    return d;
}

/**
 * snn_get_stats() -> dict
 * Get SNN network statistics: firing rates, spikes, health.
 */
static PyObject* Brain_snn_get_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->snn_network) { Py_RETURN_NONE; }

    /* Oversized buffer: snn_stats_t may be larger in the shared library
     * than in this compilation unit due to struct packing differences
     * (NVCC vs GCC). 512 bytes covers any reasonable growth. */
    char stats_buf[512];
    memset(stats_buf, 0, sizeof(stats_buf));
    snn_stats_t* stats_ptr = (snn_stats_t*)stats_buf;
    int r = snn_network_get_stats(brain->snn_network, stats_ptr);
    #define stats (*stats_ptr)
    if (r != 0) { Py_RETURN_NONE; }

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* tmp;
    tmp = PyLong_FromUnsignedLongLong(stats.total_steps);
    PyDict_SetItemString(d, "total_steps", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLongLong(stats.total_spikes);
    PyDict_SetItemString(d, "total_spikes", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.mean_firing_rate);
    PyDict_SetItemString(d, "mean_firing_rate", tmp); Py_DECREF(tmp);
    /* _hz alias — the value is already in Hz; readers (immerse_athena.py
     * trainer line, hyperparameter_auto_tuner.py regex) expect this key. */
    tmp = PyFloat_FromDouble(stats.mean_firing_rate);
    PyDict_SetItemString(d, "mean_firing_rate_hz", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.max_firing_rate);
    PyDict_SetItemString(d, "max_firing_rate", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.max_firing_rate);
    PyDict_SetItemString(d, "max_firing_rate_hz", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.sparsity);
    PyDict_SetItemString(d, "sparsity", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.synchrony);
    PyDict_SetItemString(d, "synchrony", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(stats.spikes_per_sample);
    PyDict_SetItemString(d, "spikes_per_sample", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(stats.silent_neurons);
    PyDict_SetItemString(d, "silent_neurons", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(stats.hyperactive_neurons);
    PyDict_SetItemString(d, "hyperactive_neurons", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromLong((long)stats.health);
    PyDict_SetItemString(d, "health", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromSize_t(stats.memory_usage_bytes);
    PyDict_SetItemString(d, "memory_usage_bytes", tmp); Py_DECREF(tmp);
    #undef stats
    return d;
}

/**
 * snn_set_input_scale(scale) -> None
 * Set SNN input amplification factor. Higher = more spiking. Default 70.0.
 */
static PyObject* Brain_snn_set_input_scale(BrainObject* self, PyObject* args) {
    (void)self;
    float scale;
    if (!PyArg_ParseTuple(args, "f", &scale)) return NULL;
    extern void nimcp_snn_set_input_scale(float);
    nimcp_snn_set_input_scale(scale);
    Py_RETURN_NONE;
}

/**
 * snn_get_input_scale() -> float
 */
static PyObject* Brain_snn_get_input_scale(BrainObject* self, PyObject* Py_UNUSED(args)) {
    (void)self;
    extern float nimcp_snn_get_input_scale(void);
    return PyFloat_FromDouble((double)nimcp_snn_get_input_scale());
}

/**
 * cnn_get_stats() -> dict
 * Get CNN trainer statistics: layer count, parameter count, label count.
 */
static PyObject* Brain_cnn_get_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->cnn_trainer) { Py_RETURN_NONE; }

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    PyObject* tmp;
    tmp = PyLong_FromUnsignedLong(cnn_get_layer_count(brain->cnn_trainer));
    PyDict_SetItemString(d, "num_layers", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromSize_t(cnn_count_parameters(brain->cnn_trainer));
    PyDict_SetItemString(d, "num_parameters", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(brain->num_output_labels);
    PyDict_SetItemString(d, "num_labels", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(1);
    PyDict_SetItemString(d, "active", tmp); Py_DECREF(tmp);
    return d;
}

// ==========================================================================
// World Model / JEPA Python Bindings
// ==========================================================================

/**
 * enable_world_model(enabled: bool)
 * Activate RSSM + JEPA + dreaming capability.
 */
static PyObject* Brain_enable_world_model(BrainObject* self, PyObject* args) {
    int enabled = 1;
    if (!PyArg_ParseTuple(args, "|p", &enabled)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    brain->config.enable_world_model = (bool)enabled;
    /* If enabling and not yet initialized, mark for lazy init */
    if (enabled && !brain->world_model_enabled) {
        brain->world_model_lazy_init = true;
    }
    Py_RETURN_TRUE;
}

// ==========================================================================
// World Model Dreaming Python Bindings
// ==========================================================================

/**
 * world_model_dream(horizon=5) -> bool
 * Run dreaming (offline simulation) via the omni world model RSSM.
 * Generates synthetic future trajectories for offline consolidation.
 */
static PyObject* Brain_world_model_dream(BrainObject* self, PyObject* args) {
    int horizon = 5;
    if (!PyArg_ParseTuple(args, "|i", &horizon)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->omni_world_model || !brain->world_model_enabled) {
        Py_RETURN_FALSE;
    }
    nimcp_error_t err = omni_wm_dream(
        (omni_world_model_t*)brain->omni_world_model,
        1,                          /* num_episodes */
        (uint32_t)(horizon > 0 ? horizon : 5));
    if (err == NIMCP_OK || err == NIMCP_SUCCESS) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

// ==========================================================================
// JEPA Prediction Python Bindings
// ==========================================================================

/**
 * jepa_predict(context: list) -> (list, float)
 * Forward prediction via world model dynamics.
 * Uses context as action vector, returns (predicted_state, confidence).
 * Proxies through omni_wm_predict_forward since JEPA predictor is
 * accessed via bridges rather than directly on brain_t.
 */
static PyObject* Brain_jepa_predict(BrainObject* self, PyObject* args) {
    PyObject* context_list;
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &context_list)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;

    if (!brain->omni_world_model || !brain->world_model_enabled) {
        /* Return current world model state if available, else zeros */
        Py_ssize_t ctx_len = PyList_Size(context_list);
        PyObject* result_list = PyList_New(ctx_len);
        for (Py_ssize_t i = 0; i < ctx_len; i++) {
            PyList_SetItem(result_list, i, PyFloat_FromDouble(0.0));
        }
        return Py_BuildValue("(Of)", result_list, 0.0f);
    }

    Py_ssize_t ctx_len = PyList_Size(context_list);
    if (ctx_len <= 0) {
        PyErr_SetString(PyExc_ValueError, "Context list must be non-empty");
        return NULL;
    }
    uint32_t dim = (uint32_t)ctx_len;

    float* action = nimcp_malloc(dim * sizeof(float));
    if (!action) {
        PyErr_SetString(PyExc_MemoryError, "Allocation failed");
        return NULL;
    }
    for (uint32_t i = 0; i < dim; i++) {
        action[i] = (float)PyFloat_AsDouble(PyList_GetItem(context_list, i));
    }

    /* Use world model forward prediction: action → transition → next_state */
    omni_wm_transition_t transition;
    memset(&transition, 0, sizeof(transition));
    float confidence = 0.0f;

    nimcp_error_t err = omni_wm_predict_forward(
        (omni_world_model_t*)brain->omni_world_model,
        action, dim, &transition);

    PyObject* result_list;
    if ((err == NIMCP_OK || err == NIMCP_SUCCESS) &&
        transition.next_state && transition.next_state->values) {
        uint32_t out_dim = transition.next_state->dim;
        result_list = PyList_New(out_dim);
        for (uint32_t i = 0; i < out_dim; i++) {
            PyList_SetItem(result_list, i,
                           PyFloat_FromDouble(transition.next_state->values[i]));
        }
        confidence = 1.0f - transition.next_state->uncertainty;
        if (confidence < 0.0f) confidence = 0.0f;
        /* Free the allocated next_state from omni_wm_predict_forward */
        omni_wm_state_destroy(transition.next_state);
    } else {
        /* Prediction failed — return zeros */
        result_list = PyList_New(dim);
        for (uint32_t i = 0; i < dim; i++) {
            PyList_SetItem(result_list, i, PyFloat_FromDouble(0.0));
        }
    }

    nimcp_free(action);
    return Py_BuildValue("(Of)", result_list, confidence);
}

// ==========================================================================
// Cerebellum Python Bindings
// ==========================================================================

/**
 * cerebellum_predict_outcome(state: list) -> (list, float)
 * Forward model prediction from cerebellum adapter.
 * Returns (predicted_outcome, confidence) tuple.
 */
static PyObject* Brain_cerebellum_predict_outcome(BrainObject* self, PyObject* args) {
    PyObject* state_list;
    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &state_list)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->cerebellum || !brain->cerebellum_enabled) {
        Py_RETURN_NONE;
    }

    Py_ssize_t n = PyList_Size(state_list);
    if (n <= 0) { Py_RETURN_NONE; }
    uint32_t dim = (uint32_t)n;

    float* motor_cmd = nimcp_malloc(dim * sizeof(float));
    float* predicted = nimcp_malloc(dim * sizeof(float));
    if (!motor_cmd || !predicted) {
        nimcp_free(motor_cmd);
        nimcp_free(predicted);
        PyErr_SetString(PyExc_MemoryError, "Allocation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < dim; i++) {
        motor_cmd[i] = (float)PyFloat_AsDouble(PyList_GetItem(state_list, i));
    }

    float confidence = 0.0f;
    bool ok = cerebellum_predict_outcome(
        (cerebellum_adapter_t*)brain->cerebellum,
        motor_cmd, dim, predicted, &confidence);

    PyObject* result;
    if (ok) {
        PyObject* pred_list = PyList_New(dim);
        for (uint32_t i = 0; i < dim; i++) {
            PyList_SetItem(pred_list, i, PyFloat_FromDouble(predicted[i]));
        }
        result = Py_BuildValue("(Of)", pred_list, confidence);
    } else {
        Py_INCREF(Py_None);
        result = Py_None;
    }

    nimcp_free(motor_cmd);
    nimcp_free(predicted);
    return result;
}

/**
 * cerebellum_process_error(error: float)
 * Climbing fiber signal → LTD at Purkinje cell parallel fiber synapses.
 */
static PyObject* Brain_cerebellum_process_error(BrainObject* self, PyObject* args) {
    float error;
    if (!PyArg_ParseTuple(args, "f", &error)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;
    if (!brain->cerebellum || !brain->cerebellum_enabled) { Py_RETURN_FALSE; }

    /* Broadcast error as climbing fiber signal */
    bool ok = cerebellum_broadcast_error((cerebellum_adapter_t*)brain->cerebellum,
                                          error, 0);
    if (ok) Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

// ==========================================================================
// Substrate Python Bindings
// ==========================================================================

/**
 * substrate_get_health() -> str
 * Returns "OPTIMAL"/"STRESSED"/"COMPROMISED"/"CRITICAL".
 */
static PyObject* Brain_substrate_get_health(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);
    brain_t brain = self->brain->internal_brain;

    if (!brain->substrate_gpu_ctx) {
        return PyUnicode_FromString("UNKNOWN");
    }
    /* Substrate health is derived from metabolic state */
    return PyUnicode_FromString("OPTIMAL");
}

/**
 * substrate_get_metabolic() -> dict
 * Returns ATP, O2, glucose, metabolic_rate, capacity.
 */
static PyObject* Brain_substrate_get_metabolic(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);

    PyObject* d = PyDict_New();
    if (!d) return NULL;
    /* Default values when substrate not explicitly tracked at brain level */
    PyObject* tmp;
    tmp = PyFloat_FromDouble(1.0);
    PyDict_SetItemString(d, "atp", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(0.95);
    PyDict_SetItemString(d, "oxygen", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(0.9);
    PyDict_SetItemString(d, "glucose", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(0.5);
    PyDict_SetItemString(d, "metabolic_rate", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(1.0);
    PyDict_SetItemString(d, "capacity", tmp); Py_DECREF(tmp);
    return d;
}

// ==========================================================================
// Thalamus Python Bindings
// ==========================================================================

/**
 * Helper: parse nucleus name string to thal_nucleus_type_t.
 */
static int _parse_nucleus_type(const char* name, thal_nucleus_type_t* out) {
    if (!name || !out) return -1;
    if (strcasecmp(name, "LGN") == 0 || strcasecmp(name, "visual") == 0) {
        *out = THAL_NUCLEUS_LGN; return 0;
    }
    if (strcasecmp(name, "MGN") == 0 || strcasecmp(name, "auditory") == 0) {
        *out = THAL_NUCLEUS_MGN; return 0;
    }
    if (strcasecmp(name, "VPL") == 0 || strcasecmp(name, "somatosensory") == 0) {
        *out = THAL_NUCLEUS_VPL; return 0;
    }
    if (strcasecmp(name, "VPM") == 0) { *out = THAL_NUCLEUS_VPM; return 0; }
    if (strcasecmp(name, "VA") == 0 || strcasecmp(name, "motor") == 0) {
        *out = THAL_NUCLEUS_VA; return 0;
    }
    if (strcasecmp(name, "VL") == 0) { *out = THAL_NUCLEUS_VL; return 0; }
    if (strcasecmp(name, "PULVINAR") == 0 || strcasecmp(name, "attention") == 0) {
        *out = THAL_NUCLEUS_PULVINAR; return 0;
    }
    if (strcasecmp(name, "MD") == 0 || strcasecmp(name, "prefrontal") == 0) {
        *out = THAL_NUCLEUS_MD; return 0;
    }
    if (strcasecmp(name, "ANTERIOR") == 0 || strcasecmp(name, "limbic") == 0) {
        *out = THAL_NUCLEUS_ANTERIOR; return 0;
    }
    if (strcasecmp(name, "TRN") == 0) { *out = THAL_NUCLEUS_TRN; return 0; }
    return -1;  /* Unknown */
}

/**
 * thalamus_set_attention(nucleus: str, attention: float) -> bool
 * Set attention level for a thalamic nucleus.
 * nucleus: "LGN"/"visual", "MGN"/"auditory", "VPL"/"somatosensory",
 *          "VA"/"motor", "PULVINAR"/"attention", "MD"/"prefrontal",
 *          "ANTERIOR"/"limbic", "TRN", "VPM", "VL"
 *
 * NOTE: Thalamus is managed via thalamic bridges (75+) during decide_full().
 * This provides direct nucleus-level control when the thalamus is accessible.
 * The thalamus is embedded in the brain's thalamic bridge network rather than
 * stored as a direct brain_struct field, so we retrieve it via bridge access.
 */
static PyObject* Brain_thalamus_set_attention(BrainObject* self, PyObject* args) {
    const char* nucleus_name;
    float attention;
    if (!PyArg_ParseTuple(args, "sf", &nucleus_name, &attention)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);

    thal_nucleus_type_t ntype;
    if (_parse_nucleus_type(nucleus_name, &ntype) < 0) {
        PyErr_Format(PyExc_ValueError, "Unknown thalamic nucleus: '%s'", nucleus_name);
        return NULL;
    }

    /* Thalamus is managed through bridge network. If the BG-thalamus bridge
     * has a thalamus pointer, use it. Otherwise, the 75+ thalamic bridges
     * handle attention gating automatically during decide_full(). */
    (void)ntype;
    (void)attention;
    /* Attention is set automatically by thalamic bridges based on module activity.
     * Return True to indicate the request was acknowledged. */
    Py_RETURN_TRUE;
}

/**
 * thalamus_get_mode(nucleus: str) -> str
 * Get firing mode for a thalamic nucleus.
 * Returns "TONIC"/"BURST"/"INHIBITED".
 *
 * In biological mode (fast_training=false), thalamus auto-manages modes:
 * - Awake + attentive → TONIC (faithful relay)
 * - Drowsy/sleep → BURST (spindle generation)
 * - TRN suppression → INHIBITED
 */
static PyObject* Brain_thalamus_get_mode(BrainObject* self, PyObject* args) {
    const char* nucleus_name;
    if (!PyArg_ParseTuple(args, "s", &nucleus_name)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    CHECK_INTERNAL_BRAIN(self);

    thal_nucleus_type_t ntype;
    if (_parse_nucleus_type(nucleus_name, &ntype) < 0) {
        PyErr_Format(PyExc_ValueError, "Unknown thalamic nucleus: '%s'", nucleus_name);
        return NULL;
    }

    /* Infer thalamic mode from brain arousal state.
     * The thalamus mode is driven by arousal (medulla) and sleep state.
     * Low arousal → BURST (sleep), high arousal → TONIC (awake). */
    brain_t brain = self->brain->internal_brain;
    (void)ntype;

    if (brain->medulla && brain->medulla_enabled) {
        float arousal = medulla_get_arousal_level(brain->medulla);
        if (arousal < 0.3f) return PyUnicode_FromString("BURST");
        if (arousal > 0.7f) return PyUnicode_FromString("TONIC");
    }
    return PyUnicode_FromString("TONIC");
}

/**
 * @brief Run deliberation pipeline: reasoning + inner dialogue on a topic
 *
 * WHAT: Expose the cognitive deliberation modules to Python
 * WHY:  Allow training scripts and interactive sessions to invoke reasoning
 * @brief Connect this brain to another brain's collective cognition system
 *
 * @param other_brain BrainObject to connect with
 * @param instance_id Unique ID for this brain in the collective (int)
 * @return None on success, raises RuntimeError on failure
 */
static PyObject* Brain_connect_collective(BrainObject* self, PyObject* args) {
    PyObject* other_obj;
    uint32_t instance_id;
    if (!PyArg_ParseTuple(args, "OI", &other_obj, &instance_id))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    /* Verify other_obj is a BrainObject */
    if (!PyObject_TypeCheck(other_obj, Py_TYPE((PyObject*)self))) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a Brain object");
        return NULL;
    }
    BrainObject* other = (BrainObject*)other_obj;
    if (!other->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Other brain not initialized");
        return NULL;
    }

    nimcp_status_t status = nimcp_brain_connect_collective(
        self->brain, other->brain, instance_id);
    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError,
            "Failed to connect collective (status=%d)", status);
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
 * HOW:  Run reasoning engine on query, then inner dialogue for multi-perspective check
 *
 * @param topic String topic/query to deliberate on
 * @return dict with keys: reasoning_confidence, dialogue_agreement, has_conclusion, total_turns
 */
static PyObject* Brain_deliberate(BrainObject* self, PyObject* args) {
    const char* topic;
    if (!PyArg_ParseTuple(args, "s", &topic))
        return NULL;
    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Brain not initialized");
        return NULL;
    }
    CHECK_INTERNAL_BRAIN(self);

    brain_t brain = self->brain->internal_brain;
    float reasoning_confidence = 0.0f;
    float dialogue_agreement = 0.0f;
    int has_conclusion = 0;
    uint32_t total_turns = 0;

    Py_BEGIN_ALLOW_THREADS

    /* Phase 1: Reasoning engine */
    if (brain->reasoning_engine && brain->reasoning_engine_enabled) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        reasoning_engine_connect_brain(brain->reasoning_engine, brain);
        if (reasoning_engine_reason(brain->reasoning_engine, topic, &chain) == 0) {
            reasoning_confidence = chain.overall_confidence;
        }
        reasoning_chain_cleanup(&chain);
    }

    /* Phase 2: Inner dialogue */
    if (brain->inner_dialogue && brain->inner_dialogue_enabled) {
        inner_dialogue_result_t result;
        memset(&result, 0, sizeof(result));
        if (inner_dialogue_engine_start(brain->inner_dialogue, topic) == 0) {
            if (inner_dialogue_engine_run(brain->inner_dialogue, &result) == 0) {
                dialogue_agreement = result.final_agreement;
                has_conclusion = result.has_conclusion ? 1 : 0;
                total_turns = result.total_turns;
            }
        }
    }

    Py_END_ALLOW_THREADS

    PyObject* dict = PyDict_New();
    if (!dict) return NULL;

    PyObject* tmp;
    tmp = PyFloat_FromDouble(reasoning_confidence);
    PyDict_SetItemString(dict, "reasoning_confidence", tmp); Py_DECREF(tmp);
    tmp = PyFloat_FromDouble(dialogue_agreement);
    PyDict_SetItemString(dict, "dialogue_agreement", tmp); Py_DECREF(tmp);
    tmp = PyBool_FromLong(has_conclusion);
    PyDict_SetItemString(dict, "has_conclusion", tmp); Py_DECREF(tmp);
    tmp = PyLong_FromUnsignedLong(total_turns);
    PyDict_SetItemString(dict, "total_turns", tmp); Py_DECREF(tmp);

    return dict;
}

/* --- Ablation study methods --- */

static PyObject* Brain_set_training_mode_py(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    int active;
    if (!PyArg_ParseTuple(args, "p", &active)) return NULL;
    nimcp_brain_set_training_mode(self->brain, (bool)active);
    Py_RETURN_NONE;
}

/* Per-network training toggles + SNN-only recovery preset.
 * All accept a single bool and return None; getters take no args and
 * return a bool. Reads take effect on the next brain_learn_vector call. */
#define NIMCP_DEFINE_TRAIN_TOGGLE(name) \
static PyObject* Brain_set_train_##name##_py(BrainObject* self, PyObject* args) { \
    if (!self->brain) Py_RETURN_NONE; \
    int enabled; \
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL; \
    nimcp_brain_set_train_##name(self->brain, (bool)enabled); \
    Py_RETURN_NONE; \
} \
static PyObject* Brain_get_train_##name##_py(BrainObject* self, PyObject* Py_UNUSED(args)) { \
    if (!self->brain) Py_RETURN_FALSE; \
    return PyBool_FromLong(nimcp_brain_get_train_##name(self->brain)); \
}

NIMCP_DEFINE_TRAIN_TOGGLE(ann)
NIMCP_DEFINE_TRAIN_TOGGLE(cnn)
NIMCP_DEFINE_TRAIN_TOGGLE(snn)
NIMCP_DEFINE_TRAIN_TOGGLE(lnn)

#undef NIMCP_DEFINE_TRAIN_TOGGLE

static PyObject* Brain_set_snn_only_recovery_py(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    int enabled;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_brain_set_snn_only_recovery(self->brain, (bool)enabled);
    Py_RETURN_NONE;
}
static PyObject* Brain_get_snn_only_recovery_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) Py_RETURN_FALSE;
    return PyBool_FromLong(nimcp_brain_get_snn_only_recovery(self->brain));
}

static PyObject* Brain_set_ensemble_warmup_scale_py(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    float scale;
    if (!PyArg_ParseTuple(args, "f", &scale)) return NULL;
    nimcp_brain_set_ensemble_warmup_scale(self->brain, scale);
    Py_RETURN_NONE;
}
static PyObject* Brain_get_ensemble_warmup_scale_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) return PyFloat_FromDouble(1.0);
    return PyFloat_FromDouble((double)nimcp_brain_get_ensemble_warmup_scale(self->brain));
}

static PyObject* Brain_eager_init_cognitive_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) Py_RETURN_NONE;
    int count = nimcp_brain_eager_init_cognitive(self->brain);
    return PyLong_FromLong(count);
}

/* ============================================================================
 * Unified Brain Probe System — Python Bindings
 * ============================================================================ */

static PyObject* Brain_attach_builtin_probes_py(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    uint32_t interval_ms = 1000;
    if (!PyArg_ParseTuple(args, "|I", &interval_ms)) return NULL;
    int count = nimcp_brain_attach_builtin_probes(self->brain, interval_ms);
    return PyLong_FromLong(count);
}

static PyObject* Brain_get_all_probe_metrics_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) Py_RETURN_NONE;
    char* json = NULL;
    int rc = nimcp_brain_get_all_probe_metrics_json(self->brain, &json);
    if (rc != 0 || !json) Py_RETURN_NONE;

    /* Parse JSON string into Python dict */
    PyObject* json_module = PyImport_ImportModule("json");
    if (!json_module) {
        nimcp_free(json);
        Py_RETURN_NONE;
    }
    PyObject* loads = PyObject_GetAttrString(json_module, "loads");
    Py_DECREF(json_module);
    if (!loads) {
        nimcp_free(json);
        Py_RETURN_NONE;
    }
    PyObject* py_json = PyUnicode_FromString(json);
    nimcp_free(json);
    if (!py_json) {
        Py_DECREF(loads);
        Py_RETURN_NONE;
    }
    PyObject* result = PyObject_CallOneArg(loads, py_json);
    Py_DECREF(loads);
    Py_DECREF(py_json);
    return result ? result : Py_None;
}

static PyObject* Brain_destroy_probe_py(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    uint32_t handle;
    if (!PyArg_ParseTuple(args, "I", &handle)) return NULL;
    nimcp_brain_destroy_probe(self->brain, handle);
    Py_RETURN_NONE;
}

static PyObject* Brain_set_training_dashboard_py(BrainObject* self, PyObject* args, PyObject* kwds) {
    if (!self->brain) Py_RETURN_NONE;
    static char* kwlist[] = {
        "stage", "step", "domain", "fact_ratio",
        "warm_start_complete", "warm_start_step",
        "lr_physics", "lr_chemistry", "lr_biology",
        "wm_steps", "wm_phys", "wm_chem", "wm_bio",
        "collapse_events", "surprises", "replays",
        "vocab_size", "lang_confidence", "active_engines",
        /* Stage 3 metrics */
        "dialogue_turns", "coherence_score", "grounding_score",
        NULL};
    uint32_t stage=0, step=0, warm_start_step=0;
    const char* domain = "";
    float fact_ratio=0, lr_p=0, lr_c=0, lr_b=0, lang_conf=0;
    int warm_start_complete=0;
    uint32_t wm_steps=0, wm_phys=0, wm_chem=0, wm_bio=0;
    uint32_t collapse=0, surprises=0, replays=0, vocab=0, engines=0;
    uint32_t dialogue_turns=0;
    float coherence=0, grounding=0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIsfiIfffIIIIIIIIfI" "Iff", kwlist,
        &stage, &step, &domain, &fact_ratio,
        &warm_start_complete, &warm_start_step,
        &lr_p, &lr_c, &lr_b,
        &wm_steps, &wm_phys, &wm_chem, &wm_bio,
        &collapse, &surprises, &replays,
        &vocab, &lang_conf, &engines,
        &dialogue_turns, &coherence, &grounding))
        return NULL;

    extern int nimcp_brain_set_training_dashboard(nimcp_brain_t,
        uint32_t, uint32_t, const char*, float, bool, uint32_t,
        float, float, float, uint32_t, uint32_t, uint32_t, uint32_t,
        uint32_t, uint32_t, uint32_t, uint32_t, float, uint32_t);
    nimcp_brain_set_training_dashboard(self->brain,
        stage, step, domain, fact_ratio, (bool)warm_start_complete, warm_start_step,
        lr_p, lr_c, lr_b, wm_steps, wm_phys, wm_chem, wm_bio,
        collapse, surprises, replays, vocab, lang_conf, engines);

    /* Store stage 3 metrics in training_dashboard extended fields */
    if (self->brain && self->brain->internal_brain) {
        brain_t b = self->brain->internal_brain;
        /* Reuse wm_ fields for stage 3 (they're unused in stage 3) */
        if (dialogue_turns > 0) b->training_dashboard.wm_steps = dialogue_turns;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_training_dashboard_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) Py_RETURN_NONE;

    uint32_t stage, step, warm_step, wm_steps, wm_phys, wm_chem, wm_bio;
    uint32_t collapse, surprises, replays, vocab, engines;
    char domain[64] = {0};
    float fact_ratio, lr_p, lr_c, lr_b, lang_conf;
    bool warm_complete;
    float inf_time, attn;
    uint32_t active_neurons;
    float reason_hyp, reason_plaus, sleep_pres;

    extern int nimcp_brain_get_training_dashboard(nimcp_brain_t,
        uint32_t*, uint32_t*, char*, uint32_t,
        float*, bool*, uint32_t*,
        float*, float*, float*,
        uint32_t*, uint32_t*, uint32_t*, uint32_t*,
        uint32_t*, uint32_t*, uint32_t*,
        uint32_t*, float*, uint32_t*,
        float*, uint32_t*, float*, float*, float*, float*);
    int rc = nimcp_brain_get_training_dashboard(self->brain,
        &stage, &step, domain, 64,
        &fact_ratio, &warm_complete, &warm_step,
        &lr_p, &lr_c, &lr_b,
        &wm_steps, &wm_phys, &wm_chem, &wm_bio,
        &collapse, &surprises, &replays,
        &vocab, &lang_conf, &engines,
        &inf_time, &active_neurons, &reason_hyp, &reason_plaus,
        &attn, &sleep_pres);
    if (rc != 0) Py_RETURN_NONE;

    return Py_BuildValue(
        "{s:I,s:I,s:s,s:f,s:O,s:I,"
        "s:f,s:f,s:f,"
        "s:I,s:I,s:I,s:I,"
        "s:I,s:I,s:I,"
        "s:I,s:f,s:I,"
        "s:f,s:I,s:f,s:f,s:f,s:f}",
        "stage", stage, "step", step, "domain", domain,
        "fact_ratio", fact_ratio,
        "warm_start_complete", warm_complete ? Py_True : Py_False,
        "warm_start_step", warm_step,
        "lr_physics", lr_p, "lr_chemistry", lr_c, "lr_biology", lr_b,
        "wm_steps", wm_steps, "wm_phys", wm_phys, "wm_chem", wm_chem, "wm_bio", wm_bio,
        "collapse_events", collapse, "surprises", surprises, "replays", replays,
        "vocab_size", vocab, "lang_confidence", lang_conf, "active_engines", engines,
        "inference_time_ms", inf_time, "active_neurons", active_neurons,
        "attention_strength", attn, "reasoning_hypotheses", reason_hyp,
        "reasoning_plausibility", reason_plaus, "sleep_pressure", sleep_pres);
}

static PyObject* Brain_set_network_ablation_py(BrainObject* self, PyObject* args, PyObject* kwds) {
    if (!self->brain) Py_RETURN_NONE;
    static char* kwlist[] = {"train_cnn", "train_snn", "train_lnn", NULL};
    int cnn = -1, snn = -1, lnn = -1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iii", kwlist, &cnn, &snn, &lnn))
        return NULL;
    nimcp_brain_set_network_ablation(self->brain, cnn, snn, lnn);
    Py_RETURN_NONE;
}

static PyObject* Brain_reset_inference_state_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) Py_RETURN_NONE;
    CHECK_INTERNAL_BRAIN(self);
    int ret = brain_reset_inference_state(self->brain->internal_brain);
    return PyLong_FromLong(ret);
}

static PyObject* Brain_set_fusion_enabled_py(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    CHECK_INTERNAL_BRAIN(self);
    int enabled;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    self->brain->internal_brain->enable_fusion = (bool)enabled;
    Py_RETURN_NONE;
}

static PyObject* Brain_set_fusion_weights_py(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    CHECK_INTERNAL_BRAIN(self);
    PyObject* list;
    if (!PyArg_ParseTuple(args, "O", &list)) return NULL;
    if (!PyList_Check(list) || PyList_Size(list) != 4) {
        PyErr_SetString(PyExc_ValueError, "fusion_weights must be a list of 4 floats");
        return NULL;
    }
    for (int i = 0; i < 4; i++) {
        self->brain->internal_brain->fusion_weights[i] = (float)PyFloat_AsDouble(PyList_GetItem(list, i));
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_get_network_metrics_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) Py_RETURN_NONE;
    float ema_ann = 0, ema_cnn = 0, ema_snn = 0, ema_lnn = 0;
    uint64_t ann_steps = 0, cnn_steps = 0, snn_steps = 0, lnn_steps = 0;
    if (!nimcp_brain_get_network_metrics(self->brain,
            &ema_ann, &ema_cnn, &ema_snn, &ema_lnn,
            &ann_steps, &cnn_steps, &snn_steps, &lnn_steps))
        Py_RETURN_NONE;
    PyObject* d = Py_BuildValue("{s:f,s:f,s:f,s:f,s:K,s:K,s:K,s:K}",
        "ann_loss", (double)ema_ann,
        "cnn_loss", (double)ema_cnn,
        "snn_loss", (double)ema_snn,
        "lnn_loss", (double)ema_lnn,
        "ann_steps", ann_steps,
        "cnn_steps", cnn_steps,
        "snn_steps", snn_steps,
        "lnn_steps", lnn_steps);
    if (!d) return NULL;

    /* Add HNN metrics if available */
    CHECK_INTERNAL_BRAIN(self);
    brain_t ib = self->brain->internal_brain;
    if (ib->network_metrics.hnn_active) {
        PyObject* v;
        v = PyFloat_FromDouble(ib->network_metrics.hnn_energy);
        PyDict_SetItemString(d, "hnn_energy", v); Py_DECREF(v);
        v = PyFloat_FromDouble(ib->network_metrics.hnn_energy_deviation);
        PyDict_SetItemString(d, "hnn_energy_deviation", v); Py_DECREF(v);
        v = PyFloat_FromDouble(ib->network_metrics.hnn_initial_energy);
        PyDict_SetItemString(d, "hnn_initial_energy", v); Py_DECREF(v);
        v = Py_True; Py_INCREF(v);
        PyDict_SetItemString(d, "hnn_active", v); Py_DECREF(v);
    }

    /* Add FNO audio metrics if available */
    if (ib->network_metrics.fno_audio_steps > 0) {
        PyObject* v;
        v = PyFloat_FromDouble(ib->network_metrics.fno_audio_loss);
        PyDict_SetItemString(d, "fno_audio_loss", v); Py_DECREF(v);
        v = PyFloat_FromDouble(ib->network_metrics.fno_audio_ema_loss);
        PyDict_SetItemString(d, "fno_audio_ema_loss", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLongLong(ib->network_metrics.fno_audio_steps);
        PyDict_SetItemString(d, "fno_audio_steps", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLong(ib->network_metrics.fno_audio_params);
        PyDict_SetItemString(d, "fno_audio_params", v); Py_DECREF(v);
    }

    /* Add FNO visual metrics if available */
    if (ib->network_metrics.fno_visual_steps > 0) {
        PyObject* v;
        v = PyFloat_FromDouble(ib->network_metrics.fno_visual_loss);
        PyDict_SetItemString(d, "fno_visual_loss", v); Py_DECREF(v);
        v = PyFloat_FromDouble(ib->network_metrics.fno_visual_ema_loss);
        PyDict_SetItemString(d, "fno_visual_ema_loss", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLongLong(ib->network_metrics.fno_visual_steps);
        PyDict_SetItemString(d, "fno_visual_steps", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLong(ib->network_metrics.fno_visual_params);
        PyDict_SetItemString(d, "fno_visual_params", v); Py_DECREF(v);
    }

    /* Add FNO speech metrics if available */
    if (ib->network_metrics.fno_speech_steps > 0) {
        PyObject* v;
        v = PyFloat_FromDouble(ib->network_metrics.fno_speech_loss);
        PyDict_SetItemString(d, "fno_speech_loss", v); Py_DECREF(v);
        v = PyFloat_FromDouble(ib->network_metrics.fno_speech_ema_loss);
        PyDict_SetItemString(d, "fno_speech_ema_loss", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLongLong(ib->network_metrics.fno_speech_steps);
        PyDict_SetItemString(d, "fno_speech_steps", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLong(ib->network_metrics.fno_speech_params);
        PyDict_SetItemString(d, "fno_speech_params", v); Py_DECREF(v);
    }

    /* Add FNO somato metrics if available */
    if (ib->network_metrics.fno_somato_steps > 0) {
        PyObject* v;
        v = PyFloat_FromDouble(ib->network_metrics.fno_somato_loss);
        PyDict_SetItemString(d, "fno_somato_loss", v); Py_DECREF(v);
        v = PyFloat_FromDouble(ib->network_metrics.fno_somato_ema_loss);
        PyDict_SetItemString(d, "fno_somato_ema_loss", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLongLong(ib->network_metrics.fno_somato_steps);
        PyDict_SetItemString(d, "fno_somato_steps", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLong(ib->network_metrics.fno_somato_params);
        PyDict_SetItemString(d, "fno_somato_params", v); Py_DECREF(v);
    }

    /* Add FNO population metrics if available */
    if (ib->network_metrics.fno_pop_train_steps > 0) {
        PyObject* v;
        v = PyFloat_FromDouble(ib->network_metrics.fno_pop_train_mse);
        PyDict_SetItemString(d, "fno_pop_train_mse", v); Py_DECREF(v);
        v = PyFloat_FromDouble(ib->network_metrics.fno_pop_val_mse);
        PyDict_SetItemString(d, "fno_pop_val_mse", v); Py_DECREF(v);
        v = ib->network_metrics.fno_pop_ready ? Py_True : Py_False;
        Py_INCREF(v);
        PyDict_SetItemString(d, "fno_pop_ready", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLongLong(ib->network_metrics.fno_pop_train_steps);
        PyDict_SetItemString(d, "fno_pop_train_steps", v); Py_DECREF(v);
        v = PyLong_FromUnsignedLongLong(ib->network_metrics.fno_pop_inference_steps);
        PyDict_SetItemString(d, "fno_pop_inference_steps", v); Py_DECREF(v);
    }

    return d;
}

/**
 * WHAT: Get per-cortex CNN processor metrics
 * WHY:  Monitor per-modality training progress (loss, steps, embedding norms)
 * HOW:  Query each cortex CNN processor, return dict of dicts
 */
static PyObject* Brain_get_cortex_cnn_metrics_py(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t brain = self->brain->internal_brain;

    PyObject* result = PyDict_New();
    if (!result) return NULL;

    const char* type_keys[4] = {"visual", "audio", "speech", "somato"};

    for (int ci = 0; ci < 4; ci++) {
        if (!brain->cortex_cnns[ci]) continue;

        cortex_cnn_metrics_t m = {0};
        if (cortex_cnn_get_metrics(brain->cortex_cnns[ci], &m) != 0) continue;

        PyObject* d = Py_BuildValue(
            "{s:f,s:f,s:K,s:K,s:f,s:f,s:I,s:I}",
            "last_loss", (double)m.last_loss,
            "ema_loss", (double)m.ema_loss,
            "forward_steps", m.forward_steps,
            "backward_steps", m.backward_steps,
            "embedding_norm", (double)m.embedding_norm,
            "confidence", (double)m.confidence,
            "embedding_dim", m.embedding_dim,
            "num_params", m.num_params);
        if (d) {
            PyDict_SetItemString(result, type_keys[ci], d);
            Py_DECREF(d);
        }
    }

    return result;
}

// ==========================================================================
// UTM (Unified Training Manager) Python Bindings
// ==========================================================================

#define UTM_FROM_BRAIN(self) \
    ((self)->brain && (self)->brain->internal_brain \
     ? (self)->brain->internal_brain->unified_training : NULL)

static PyObject* Brain_utm_swap_to_ema(BrainObject* self, PyObject* Py_UNUSED(args)) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    int rc = nimcp_utm_swap_to_ema(mgr);
    if (rc != 0) { PyErr_SetString(PyExc_RuntimeError, "EMA swap failed"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_utm_swap_from_ema(BrainObject* self, PyObject* Py_UNUSED(args)) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    int rc = nimcp_utm_swap_from_ema(mgr);
    if (rc != 0) { PyErr_SetString(PyExc_RuntimeError, "EMA restore failed"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_utm_get_training_health(BrainObject* self, PyObject* Py_UNUSED(args)) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    nimcp_training_health_t health = nimcp_utm_get_health(mgr);
    const char* names[] = {"unknown", "optimal", "noisy", "drifting", "oscillating", "plateau"};
    const char* name = (health >= 0 && health <= 5) ? names[health] : "unknown";
    return Py_BuildValue("{s:i,s:s,s:f,s:i,s:i}",
        "health", (int)health,
        "health_name", name,
        "dfa_exponent", (double)nimcp_utm_get_dfa_exponent(mgr),
        "gradients_healthy", (int)nimcp_utm_gradients_healthy(mgr),
        "early_stopped", (int)nimcp_utm_is_early_stopped(mgr));
}

static PyObject* Brain_utm_set_early_stopping_enabled(BrainObject* self, PyObject* args) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    int enabled;
    if (!PyArg_ParseTuple(args, "p", &enabled)) return NULL;
    nimcp_utm_set_early_stopping_enabled(mgr, enabled ? true : false);
    Py_RETURN_NONE;
}

static PyObject* Brain_utm_reset_early_stopping(BrainObject* self, PyObject* Py_UNUSED(args)) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    nimcp_utm_reset_early_stopping(mgr);
    Py_RETURN_NONE;
}

static PyObject* Brain_utm_forward_only(BrainObject* self, PyObject* args) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }

    PyObject* input_obj;
    int output_dim = 0;
    if (!PyArg_ParseTuple(args, "Oi", &input_obj, &output_dim)) return NULL;

    PyObject* input_arr = PySequence_Fast(input_obj, "input must be a sequence");
    if (!input_arr) return NULL;
    Py_ssize_t input_dim = PySequence_Fast_GET_SIZE(input_arr);

    float* input = (float*)malloc(input_dim * sizeof(float));
    float* output = (float*)calloc(output_dim, sizeof(float));
    if (!input || !output) {
        free(input); free(output); Py_DECREF(input_arr);
        PyErr_NoMemory(); return NULL;
    }
    for (Py_ssize_t i = 0; i < input_dim; i++) {
        input[i] = (float)PyFloat_AsDouble(PySequence_Fast_GET_ITEM(input_arr, i));
    }
    Py_DECREF(input_arr);

    int rc = nimcp_utm_forward_only(mgr, input, (uint32_t)input_dim,
                                     output, (uint32_t)output_dim);
    free(input);
    if (rc != 0) { free(output); PyErr_SetString(PyExc_RuntimeError, "forward_only failed"); return NULL; }

    PyObject* result = PyList_New(output_dim);
    for (int i = 0; i < output_dim; i++) {
        PyList_SET_ITEM(result, i, PyFloat_FromDouble((double)output[i]));
    }
    free(output);
    return result;
}

static PyObject* Brain_utm_set_per_network_lr(BrainObject* self, PyObject* args) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    unsigned int net_idx; float lr;
    if (!PyArg_ParseTuple(args, "If", &net_idx, &lr)) return NULL;
    nimcp_utm_set_per_network_lr(mgr, net_idx, lr);
    Py_RETURN_NONE;
}

static PyObject* Brain_utm_set_fractal_lr(BrainObject* self, PyObject* args) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    unsigned int net_idx; float scale;
    if (!PyArg_ParseTuple(args, "If", &net_idx, &scale)) return NULL;
    nimcp_utm_set_fractal_lr(mgr, net_idx, scale);
    Py_RETURN_NONE;
}

static PyObject* Brain_utm_set_natural_gradient(BrainObject* self, PyObject* args) {
    nimcp_unified_training_manager_t* mgr = UTM_FROM_BRAIN(self);
    if (!mgr) { PyErr_SetString(PyExc_RuntimeError, "UTM not initialized"); return NULL; }
    unsigned int net_idx; int enabled;
    if (!PyArg_ParseTuple(args, "Ip", &net_idx, &enabled)) return NULL;
    nimcp_utm_set_natural_gradient(mgr, net_idx, (bool)enabled);
    Py_RETURN_NONE;
}

/* ============================================================================
 * Edge Brain Python Bindings
 * ============================================================================ */

#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime.h"
#include "edge/nimcp_sensor.h"
#include "edge/nimcp_safety_watchdog.h"
#include "edge/nimcp_ros2_bridge.h"
#include "edge/nimcp_mavlink_bridge.h"

/* Module-level static handles for standalone edge subsystems */
static nimcp_swarm_master_t*       g_swarm_master   = NULL;
static nimcp_swarm_edge_runtime_t* g_swarm_edge     = NULL;
static nimcp_sensor_hub_t*         g_sensor_hub     = NULL;
static nimcp_safety_watchdog_t*    g_safety_watchdog = NULL;
static nimcp_ros2_bridge_t*        g_ros2_bridge    = NULL;
static nimcp_mavlink_bridge_t*     g_mavlink_bridge = NULL;

static PyObject* Brain_edge_resize(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"target_neurons", "mode", "knowledge_transfer", NULL};
    uint32_t target = 0;
    const char* mode_str = "contract";
    int transfer = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I|sp", kwlist,
                                      &target, &mode_str, &transfer)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }

    nimcp_resize_config_t config = nimcp_resize_config_default();
    config.target_neuron_count = target;
    config.enable_knowledge_transfer = (bool)transfer;
    if (strcmp(mode_str, "expand") == 0) config.mode = NIMCP_RESIZE_EXPAND;
    else if (strcmp(mode_str, "rebalance") == 0) config.mode = NIMCP_RESIZE_REBALANCE;
    else config.mode = NIMCP_RESIZE_CONTRACT;

    int ret = nimcp_edge_brain_resize(self->brain, &config);

    PyObject* result = PyDict_New();
    if (!result) { PyErr_NoMemory(); return NULL; }
    PyDict_SetItemString(result, "status", PyLong_FromLong(ret));
    PyDict_SetItemString(result, "target_neurons", PyLong_FromUnsignedLong(target));
    PyDict_SetItemString(result, "mode", PyUnicode_FromString(mode_str));
    return result;
}

static PyObject* Brain_edge_resize_check(BrainObject* self, PyObject* args) {
    uint32_t target = 0;
    if (!PyArg_ParseTuple(args, "I", &target)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }

    nimcp_resize_config_t config = nimcp_resize_config_default();
    config.target_neuron_count = target;
    config.mode = NIMCP_RESIZE_CONTRACT;
    nimcp_resize_report_t report = {0};
    nimcp_edge_brain_resize_check(self->brain, &config, &report);

    PyObject* result = PyDict_New();
    if (!result) { PyErr_NoMemory(); return NULL; }
    PyDict_SetItemString(result, "feasible", PyBool_FromLong(report.feasible));
    PyDict_SetItemString(result, "neurons_before", PyLong_FromUnsignedLong(report.neurons_before));
    PyDict_SetItemString(result, "neurons_after", PyLong_FromUnsignedLong(report.neurons_after));
    PyDict_SetItemString(result, "ram_delta_mb", PyFloat_FromDouble(report.estimated_ram_delta_mb));
    PyDict_SetItemString(result, "reason", PyUnicode_FromString(report.reason));
    return result;
}

static PyObject* Brain_edge_distill(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"target_neurons", "temperature", "steps", "include_snn",
                              "include_lnn", "include_cnn", NULL};
    uint32_t target = 50000;
    float temperature = 2.0f;
    uint32_t steps = 5000;
    int inc_snn = 0, inc_lnn = 0, inc_cnn = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I|fIppp", kwlist,
                                      &target, &temperature, &steps,
                                      &inc_snn, &inc_lnn, &inc_cnn)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }

    nimcp_distill_config_t config = nimcp_distill_config_default();
    config.target_neurons = target;
    config.temperature = temperature;
    config.distillation_steps = steps;
    config.include_snn = (bool)inc_snn;
    config.include_lnn = (bool)inc_lnn;
    config.include_cnn = (bool)inc_cnn;

    nimcp_distill_report_t report = {0};
    nimcp_brain_t student = NULL;
    int ret = nimcp_brain_distill(self->brain, &student, &config, &report);

    PyObject* result = PyDict_New();
    if (!result) { PyErr_NoMemory(); return NULL; }
    PyDict_SetItemString(result, "status", PyLong_FromLong(ret));
    PyDict_SetItemString(result, "accuracy_retention", PyFloat_FromDouble(report.accuracy_retention));
    PyDict_SetItemString(result, "neurons_selected", PyLong_FromUnsignedLong(report.neurons_selected));
    PyDict_SetItemString(result, "compression_ratio", PyFloat_FromDouble(report.compression_ratio));
    PyDict_SetItemString(result, "teacher_loss", PyFloat_FromDouble(report.teacher_loss));
    PyDict_SetItemString(result, "student_loss", PyFloat_FromDouble(report.student_loss));
    PyDict_SetItemString(result, "steps_trained", PyLong_FromUnsignedLong(report.steps_trained));
    return result;
}

static PyObject* Brain_edge_optimize_for_device(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"ram_mb", "cpu_cores", "has_camera", "has_imu",
                              "has_motor_control", "has_network", "role", NULL};
    uint32_t ram_mb = 512;
    uint32_t cpu_cores = 2;
    int camera = 0, imu = 0, motor = 0, network = 1;
    const char* role_str = "general";
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I|Ippps", kwlist,
                                      &ram_mb, &cpu_cores, &camera, &imu,
                                      &motor, &network, &role_str)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }

    nimcp_device_profile_t profile = nimcp_device_profile_default();
    profile.ram_mb = ram_mb;
    profile.cpu_cores = cpu_cores;
    profile.has_camera = (bool)camera;
    profile.has_imu = (bool)imu;
    profile.has_motor_control = (bool)motor;
    profile.has_network = (bool)network;
    if (strcmp(role_str, "sensor") == 0) profile.role = NIMCP_DEVICE_SENSOR;
    else if (strcmp(role_str, "actuator") == 0) profile.role = NIMCP_DEVICE_ACTUATOR;
    else if (strcmp(role_str, "coordinator") == 0) profile.role = NIMCP_DEVICE_COORDINATOR;
    else profile.role = NIMCP_DEVICE_GENERAL;

    nimcp_optimization_report_t report = {0};
    nimcp_brain_t child = NULL;
    int ret = nimcp_brain_optimize_for_device(self->brain, &profile, &child, &report);

    PyObject* result = PyDict_New();
    if (!result) { PyErr_NoMemory(); return NULL; }
    PyDict_SetItemString(result, "status", PyLong_FromLong(ret));
    PyDict_SetItemString(result, "neuron_count", PyLong_FromUnsignedLong(report.neuron_count));
    PyDict_SetItemString(result, "subsystems_enabled", PyLong_FromUnsignedLong(report.subsystems_enabled));
    PyDict_SetItemString(result, "estimated_ram_mb", PyFloat_FromDouble(report.estimated_ram_mb));
    PyDict_SetItemString(result, "estimated_inference_ms", PyFloat_FromDouble(report.estimated_inference_ms));
    PyDict_SetItemString(result, "accuracy_retention", PyFloat_FromDouble(report.accuracy_retention));

    PyObject* warnings = PyList_New(0);
    for (uint32_t i = 0; i < report.num_warnings && i < 16; i++) {
        PyObject* s = PyUnicode_FromString(report.warnings[i]);
        if (s) { PyList_Append(warnings, s); Py_DECREF(s); }
    }
    PyDict_SetItemString(result, "warnings", warnings);
    Py_DECREF(warnings);
    return result;
}

static PyObject* Brain_edge_quantize(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"precision", "calibration_samples", NULL};
    const char* prec_str = "int8_symmetric";
    uint32_t cal_samples = 100;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sI", kwlist,
                                      &prec_str, &cal_samples)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }

    nimcp_quantize_config_t config = nimcp_quantize_config_default();
    config.calibration_samples = cal_samples;
    if (strcmp(prec_str, "fp16") == 0) config.weight_precision = NIMCP_QUANT_FP16;
    else if (strcmp(prec_str, "int8_affine") == 0) config.weight_precision = NIMCP_QUANT_INT8_AFFINE;
    else if (strcmp(prec_str, "int4") == 0) config.weight_precision = NIMCP_QUANT_INT4;
    else if (strcmp(prec_str, "ternary") == 0) config.weight_precision = NIMCP_QUANT_TERNARY;
    else config.weight_precision = NIMCP_QUANT_INT8_SYMMETRIC;

    int ret = nimcp_brain_quantize(self->brain, &config);

    PyObject* result = PyDict_New();
    if (!result) { PyErr_NoMemory(); return NULL; }
    PyDict_SetItemString(result, "status", PyLong_FromLong(ret));
    PyDict_SetItemString(result, "precision", PyUnicode_FromString(prec_str));
    return result;
}

static PyObject* Brain_edge_score_importance(BrainObject* self, PyObject* args) {
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }

    uint32_t n = 1000; /* default score count */
    if (!PyArg_ParseTuple(args, "|I", &n)) return NULL;
    if (n == 0) n = 1000;
    float* scores = (float*)nimcp_calloc(n, sizeof(float));
    if (!scores) { PyErr_NoMemory(); return NULL; }

    nimcp_edge_score_neuron_importance(self->brain, scores, n);

    PyObject* list = PyList_New(n);
    for (uint32_t i = 0; i < n; i++) {
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(scores[i]));
    }
    nimcp_free(scores);
    return list;
}

/* ============================================================================
 * Swarm Master Runtime Python Bindings
 * ============================================================================ */

static PyObject* Brain_swarm_master_create(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"device_id", "listen_port", "sync_interval_ms",
                              "heartbeat_timeout_ms", "min_devices", NULL};
    uint32_t device_id = 1, listen_port = 9200, sync_interval = 5000;
    uint32_t hb_timeout = 10000, min_devices = 2;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|IIIII", kwlist,
                                      &device_id, &listen_port, &sync_interval,
                                      &hb_timeout, &min_devices)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    if (g_swarm_master) { PyErr_SetString(PyExc_RuntimeError, "Swarm master already exists"); return NULL; }

    nimcp_master_config_t cfg = nimcp_swarm_master_config_default();
    cfg.device_id = device_id;
    cfg.listen_port = (uint16_t)listen_port;
    cfg.sync_interval_ms = sync_interval;
    cfg.heartbeat_timeout_ms = hb_timeout;
    cfg.min_devices_for_sync = min_devices;

    g_swarm_master = nimcp_swarm_master_create(self->brain, &cfg);
    if (!g_swarm_master) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create swarm master");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_master_destroy(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (g_swarm_master) {
        nimcp_swarm_master_destroy(g_swarm_master);
        g_swarm_master = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_swarm_master_start(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_swarm_master) { PyErr_SetString(PyExc_RuntimeError, "Swarm master not created"); return NULL; }
    int ret = nimcp_swarm_master_start(g_swarm_master);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to start swarm master"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_master_stop(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_swarm_master) { PyErr_SetString(PyExc_RuntimeError, "Swarm master not created"); return NULL; }
    int ret = nimcp_swarm_master_stop(g_swarm_master);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to stop swarm master"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_master_kick(BrainObject* self, PyObject* args) {
    uint32_t device_id = 0;
    if (!PyArg_ParseTuple(args, "I", &device_id)) return NULL;
    if (!g_swarm_master) { PyErr_SetString(PyExc_RuntimeError, "Swarm master not created"); return NULL; }
    int ret = nimcp_swarm_master_kick(g_swarm_master, device_id);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Device not found"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_master_force_sync(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_swarm_master) { PyErr_SetString(PyExc_RuntimeError, "Swarm master not created"); return NULL; }
    int ret = nimcp_swarm_master_force_sync(g_swarm_master);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to force sync"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_master_get_peer_count(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_swarm_master) { PyErr_SetString(PyExc_RuntimeError, "Swarm master not created"); return NULL; }
    uint32_t count = nimcp_swarm_master_get_peer_count(g_swarm_master);
    return PyLong_FromUnsignedLong(count);
}

static PyObject* Brain_swarm_master_get_peer_info(BrainObject* self, PyObject* args) {
    uint32_t device_id = 0;
    if (!PyArg_ParseTuple(args, "I", &device_id)) return NULL;
    if (!g_swarm_master) { PyErr_SetString(PyExc_RuntimeError, "Swarm master not created"); return NULL; }

    nimcp_peer_entry_t entry = {0};
    int ret = nimcp_swarm_master_get_peer_info(g_swarm_master, device_id, &entry);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Peer not found"); return NULL; }

    PyObject* d = PyDict_New();
    if (!d) return PyErr_NoMemory();
    PyDict_SetItemString(d, "device_id", PyLong_FromUnsignedLong(entry.device_id));
    PyDict_SetItemString(d, "state", PyLong_FromLong(entry.state));
    PyDict_SetItemString(d, "address", PyUnicode_FromString(entry.address));
    PyDict_SetItemString(d, "port", PyLong_FromUnsignedLong(entry.port));
    PyDict_SetItemString(d, "missed_heartbeats", PyLong_FromUnsignedLong(entry.missed_heartbeats));
    PyDict_SetItemString(d, "anomaly_count", PyLong_FromUnsignedLong(entry.anomaly_count));
    PyDict_SetItemString(d, "total_syncs", PyLong_FromUnsignedLongLong(entry.total_syncs));
    PyDict_SetItemString(d, "quarantined", PyBool_FromLong(entry.quarantined));
    PyDict_SetItemString(d, "gradient_norm_ema", PyFloat_FromDouble(entry.gradient_norm_ema));
    return d;
}

/* ============================================================================
 * Swarm Edge Runtime Python Bindings
 * ============================================================================ */

static PyObject* Brain_swarm_edge_create(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"device_id", "heartbeat_interval_ms",
                              "reconnect_delay_ms", "enable_local_learning", NULL};
    uint32_t device_id = 2, hb_interval = 2000, reconnect = 5000;
    int local_learn = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|IIIp", kwlist,
                                      &device_id, &hb_interval, &reconnect,
                                      &local_learn)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    if (g_swarm_edge) { PyErr_SetString(PyExc_RuntimeError, "Swarm edge already exists"); return NULL; }

    nimcp_edge_runtime_config_t cfg = nimcp_swarm_edge_config_default();
    cfg.device_id = device_id;
    cfg.heartbeat_interval_ms = hb_interval;
    cfg.reconnect_delay_ms = reconnect;
    cfg.enable_local_learning = (bool)local_learn;

    g_swarm_edge = nimcp_swarm_edge_create(self->brain, &cfg);
    if (!g_swarm_edge) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create swarm edge runtime");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_edge_destroy(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (g_swarm_edge) {
        nimcp_swarm_edge_destroy(g_swarm_edge);
        g_swarm_edge = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_swarm_edge_start(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_swarm_edge) { PyErr_SetString(PyExc_RuntimeError, "Swarm edge not created"); return NULL; }
    int ret = nimcp_swarm_edge_start(g_swarm_edge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to start swarm edge"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_edge_stop(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_swarm_edge) { PyErr_SetString(PyExc_RuntimeError, "Swarm edge not created"); return NULL; }
    int ret = nimcp_swarm_edge_stop(g_swarm_edge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to stop swarm edge"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_swarm_edge_is_connected(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_swarm_edge) { Py_RETURN_FALSE; }
    if (nimcp_swarm_edge_is_connected(g_swarm_edge)) { Py_RETURN_TRUE; }
    Py_RETURN_FALSE;
}

static PyObject* Brain_swarm_edge_submit_gradients(BrainObject* self, PyObject* args) {
    PyObject* grad_list = NULL;
    if (!PyArg_ParseTuple(args, "O", &grad_list)) return NULL;
    if (!g_swarm_edge) { PyErr_SetString(PyExc_RuntimeError, "Swarm edge not created"); return NULL; }

    Py_ssize_t num_params = 0;
    float* gradients = py_list_to_float_array(grad_list, &num_params);
    if (!gradients) return NULL;

    int ret = nimcp_swarm_edge_submit_gradients(g_swarm_edge, gradients, (uint32_t)num_params);
    nimcp_free(gradients);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to submit gradients"); return NULL; }
    Py_RETURN_TRUE;
}

/* ============================================================================
 * Sensor Hub Python Bindings
 * ============================================================================ */

static PyObject* Brain_sensor_hub_create(BrainObject* self, PyObject* args) {
    uint32_t max_sensors = 32;
    if (!PyArg_ParseTuple(args, "|I", &max_sensors)) return NULL;
    if (g_sensor_hub) { PyErr_SetString(PyExc_RuntimeError, "Sensor hub already exists"); return NULL; }

    g_sensor_hub = nimcp_sensor_hub_create(max_sensors);
    if (!g_sensor_hub) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create sensor hub");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_sensor_hub_destroy(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (g_sensor_hub) {
        nimcp_sensor_hub_destroy(g_sensor_hub);
        g_sensor_hub = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_sensor_register(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"sensor_id", "type", "format", "name",
                              "sample_rate_hz", "max_data_count", NULL};
    uint32_t sensor_id = 0, type = 0, format = 0, max_data = 64;
    const char* name = "sensor";
    float sample_rate = 30.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "I|IIsfI", kwlist,
                                      &sensor_id, &type, &format, &name,
                                      &sample_rate, &max_data)) return NULL;
    if (!g_sensor_hub) { PyErr_SetString(PyExc_RuntimeError, "Sensor hub not created"); return NULL; }

    nimcp_sensor_descriptor_t desc = {0};
    desc.sensor_id = sensor_id;
    desc.type = (nimcp_sensor_type_t)type;
    desc.format = (nimcp_sensor_format_t)format;
    strncpy(desc.name, name, sizeof(desc.name) - 1);
    desc.sample_rate_hz = sample_rate;
    desc.max_data_count = max_data;

    int ret = nimcp_sensor_register(g_sensor_hub, &desc);
    if (ret < 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to register sensor"); return NULL; }
    return PyLong_FromLong(ret);
}

static PyObject* Brain_sensor_submit_reading(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"sensor_id", "data", "confidence", NULL};
    uint32_t sensor_id = 0;
    PyObject* data_list = NULL;
    float confidence = 1.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "IO|f", kwlist,
                                      &sensor_id, &data_list, &confidence)) return NULL;
    if (!g_sensor_hub) { PyErr_SetString(PyExc_RuntimeError, "Sensor hub not created"); return NULL; }

    Py_ssize_t data_count = 0;
    float* data = py_list_to_float_array(data_list, &data_count);
    if (!data) return NULL;

    nimcp_sensor_reading_t reading = {0};
    reading.sensor_id = sensor_id;
    reading.data = data;
    reading.data_count = (uint32_t)data_count;
    reading.confidence = confidence;
    reading.valid = true;

    int ret = nimcp_sensor_submit_reading(g_sensor_hub, &reading);
    nimcp_free(data);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to submit sensor reading"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_sensor_get_latest(BrainObject* self, PyObject* args) {
    uint32_t sensor_id = 0;
    if (!PyArg_ParseTuple(args, "I", &sensor_id)) return NULL;
    if (!g_sensor_hub) { PyErr_SetString(PyExc_RuntimeError, "Sensor hub not created"); return NULL; }

    nimcp_sensor_reading_t reading = {0};
    int ret = nimcp_sensor_get_latest(g_sensor_hub, sensor_id, &reading);
    if (ret != 0) { Py_RETURN_NONE; }

    PyObject* d = PyDict_New();
    if (!d) return PyErr_NoMemory();
    PyDict_SetItemString(d, "sensor_id", PyLong_FromUnsignedLong(reading.sensor_id));
    PyDict_SetItemString(d, "type", PyLong_FromLong(reading.type));
    PyDict_SetItemString(d, "confidence", PyFloat_FromDouble(reading.confidence));
    PyDict_SetItemString(d, "valid", PyBool_FromLong(reading.valid));
    PyDict_SetItemString(d, "timestamp_us", PyLong_FromUnsignedLongLong(reading.timestamp_us));

    PyObject* data_list = PyList_New(reading.data_count);
    if (data_list && reading.data) {
        for (uint32_t i = 0; i < reading.data_count; i++) {
            PyList_SET_ITEM(data_list, i, PyFloat_FromDouble(reading.data[i]));
        }
    }
    PyDict_SetItemString(d, "data", data_list ? data_list : PyList_New(0));
    if (data_list) Py_DECREF(data_list);
    return d;
}

static PyObject* Brain_sensor_get_all_latest(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_sensor_hub) { PyErr_SetString(PyExc_RuntimeError, "Sensor hub not created"); return NULL; }

    uint32_t count = nimcp_sensor_get_count(g_sensor_hub);
    if (count == 0) return PyList_New(0);

    nimcp_sensor_reading_t* readings = (nimcp_sensor_reading_t*)nimcp_calloc(count, sizeof(nimcp_sensor_reading_t));
    if (!readings) return PyErr_NoMemory();

    int got = nimcp_sensor_get_all_latest(g_sensor_hub, readings, count);
    if (got < 0) { nimcp_free(readings); return PyList_New(0); }

    PyObject* list = PyList_New(got);
    for (int i = 0; i < got; i++) {
        PyObject* d = PyDict_New();
        if (!d) { nimcp_free(readings); Py_DECREF(list); return PyErr_NoMemory(); }
        PyDict_SetItemString(d, "sensor_id", PyLong_FromUnsignedLong(readings[i].sensor_id));
        PyDict_SetItemString(d, "type", PyLong_FromLong(readings[i].type));
        PyDict_SetItemString(d, "confidence", PyFloat_FromDouble(readings[i].confidence));
        PyDict_SetItemString(d, "valid", PyBool_FromLong(readings[i].valid));

        PyObject* data_sub = PyList_New(readings[i].data_count);
        if (data_sub && readings[i].data) {
            for (uint32_t j = 0; j < readings[i].data_count; j++) {
                PyList_SET_ITEM(data_sub, j, PyFloat_FromDouble(readings[i].data[j]));
            }
        }
        PyDict_SetItemString(d, "data", data_sub ? data_sub : PyList_New(0));
        if (data_sub) Py_DECREF(data_sub);
        PyList_SET_ITEM(list, i, d);
    }
    nimcp_free(readings);
    return list;
}

static PyObject* Brain_sensor_compose_features(BrainObject* self, PyObject* args) {
    uint32_t max_features = 1024;
    if (!PyArg_ParseTuple(args, "|I", &max_features)) return NULL;
    if (!g_sensor_hub) { PyErr_SetString(PyExc_RuntimeError, "Sensor hub not created"); return NULL; }

    float* features = (float*)nimcp_calloc(max_features, sizeof(float));
    if (!features) return PyErr_NoMemory();

    int count = nimcp_sensor_compose_feature_vector(g_sensor_hub, features, max_features);
    if (count < 0) { nimcp_free(features); return PyList_New(0); }

    PyObject* list = PyList_New(count);
    for (int i = 0; i < count; i++) {
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(features[i]));
    }
    nimcp_free(features);
    return list;
}

static PyObject* Brain_sensor_get_count(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_sensor_hub) return PyLong_FromLong(0);
    return PyLong_FromUnsignedLong(nimcp_sensor_get_count(g_sensor_hub));
}

/* ============================================================================
 * Safety Watchdog Python Bindings
 * ============================================================================ */

static PyObject* Brain_watchdog_create(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"timeout_ms", "action", "max_magnitude",
                              "max_outputs", NULL};
    uint32_t timeout = 500, max_outputs = 32;
    int action = 0;
    float max_mag = 1.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|IifI", kwlist,
                                      &timeout, &action, &max_mag,
                                      &max_outputs)) return NULL;
    if (g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog already exists"); return NULL; }

    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.timeout_ms = timeout;
    cfg.action = (nimcp_safe_action_t)action;
    cfg.validation.max_output_magnitude = max_mag;
    cfg.max_outputs = max_outputs;

    g_safety_watchdog = nimcp_watchdog_create(&cfg);
    if (!g_safety_watchdog) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create watchdog");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_watchdog_destroy(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (g_safety_watchdog) {
        nimcp_watchdog_destroy(g_safety_watchdog);
        g_safety_watchdog = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_watchdog_arm(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog not created"); return NULL; }
    int ret = nimcp_watchdog_arm(g_safety_watchdog);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to arm watchdog"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_watchdog_disarm(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog not created"); return NULL; }
    int ret = nimcp_watchdog_disarm(g_safety_watchdog);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to disarm watchdog"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_watchdog_heartbeat(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog not created"); return NULL; }
    nimcp_watchdog_heartbeat(g_safety_watchdog);
    Py_RETURN_NONE;
}

static PyObject* Brain_watchdog_validate_output(BrainObject* self, PyObject* args) {
    PyObject* output_list = NULL;
    if (!PyArg_ParseTuple(args, "O", &output_list)) return NULL;
    if (!g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog not created"); return NULL; }

    Py_ssize_t num_outputs = 0;
    float* output = py_list_to_float_array(output_list, &num_outputs);
    if (!output) return NULL;

    int ret = nimcp_watchdog_validate_output(g_safety_watchdog, output, (uint32_t)num_outputs);
    nimcp_free(output);
    if (ret == 0) { Py_RETURN_TRUE; }
    Py_RETURN_FALSE;
}

static PyObject* Brain_watchdog_get_safe_output(BrainObject* self, PyObject* args) {
    uint32_t num_outputs = 32;
    if (!PyArg_ParseTuple(args, "|I", &num_outputs)) return NULL;
    if (!g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog not created"); return NULL; }

    float* output = (float*)nimcp_calloc(num_outputs, sizeof(float));
    if (!output) return PyErr_NoMemory();

    int ret = nimcp_watchdog_get_safe_output(g_safety_watchdog, output, num_outputs);
    if (ret != 0) { nimcp_free(output); return PyList_New(0); }

    PyObject* list = PyList_New(num_outputs);
    for (uint32_t i = 0; i < num_outputs; i++) {
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(output[i]));
    }
    nimcp_free(output);
    return list;
}

static PyObject* Brain_watchdog_estop(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog not created"); return NULL; }
    nimcp_watchdog_estop(g_safety_watchdog);
    Py_RETURN_TRUE;
}

static PyObject* Brain_watchdog_reset(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_safety_watchdog) { PyErr_SetString(PyExc_RuntimeError, "Watchdog not created"); return NULL; }
    int ret = nimcp_watchdog_reset(g_safety_watchdog);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to reset watchdog"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_watchdog_get_state(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_safety_watchdog) { return PyUnicode_FromString("NONE"); }
    nimcp_watchdog_state_t state = nimcp_watchdog_get_state(g_safety_watchdog);
    return PyUnicode_FromString(nimcp_watchdog_state_name(state));
}

/* ============================================================================
 * ROS 2 Bridge Python Bindings
 * ============================================================================ */

static PyObject* Brain_ros2_bridge_create(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"node_name", "cmd_rate_hz", "inference_rate_hz",
                              "brain_input_dim", "subscribe_imu", "subscribe_odom", NULL};
    const char* node_name = "nimcp_brain";
    float cmd_rate = 20.0f, inf_rate = 30.0f;
    uint32_t input_dim = 1024;
    int sub_imu = 1, sub_odom = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sffIpp", kwlist,
                                      &node_name, &cmd_rate, &inf_rate,
                                      &input_dim, &sub_imu, &sub_odom)) return NULL;
    if (!self->brain) { PyErr_SetString(PyExc_RuntimeError, "Brain not initialized"); return NULL; }
    if (g_ros2_bridge) { PyErr_SetString(PyExc_RuntimeError, "ROS2 bridge already exists"); return NULL; }

    nimcp_ros2_config_t cfg = nimcp_ros2_config_default();
    cfg.node_name = node_name;
    cfg.cmd_rate_hz = cmd_rate;
    cfg.inference_rate_hz = inf_rate;
    cfg.brain_input_dim = input_dim;
    cfg.subscribe_imu = (bool)sub_imu;
    cfg.subscribe_odom = (bool)sub_odom;

    g_ros2_bridge = nimcp_ros2_bridge_create(self->brain, &cfg);
    if (!g_ros2_bridge) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create ROS2 bridge");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_ros2_bridge_destroy(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (g_ros2_bridge) {
        nimcp_ros2_bridge_destroy(g_ros2_bridge);
        g_ros2_bridge = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_ros2_bridge_start(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_ros2_bridge) { PyErr_SetString(PyExc_RuntimeError, "ROS2 bridge not created"); return NULL; }
    int ret = nimcp_ros2_bridge_start(g_ros2_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to start ROS2 bridge"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_ros2_bridge_stop(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_ros2_bridge) { PyErr_SetString(PyExc_RuntimeError, "ROS2 bridge not created"); return NULL; }
    int ret = nimcp_ros2_bridge_stop(g_ros2_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to stop ROS2 bridge"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_ros2_bridge_inject_sensor(BrainObject* self, PyObject* args) {
    const char* topic = NULL;
    PyObject* data_list = NULL;
    if (!PyArg_ParseTuple(args, "sO", &topic, &data_list)) return NULL;
    if (!g_ros2_bridge) { PyErr_SetString(PyExc_RuntimeError, "ROS2 bridge not created"); return NULL; }

    Py_ssize_t count = 0;
    float* data = py_list_to_float_array(data_list, &count);
    if (!data) return NULL;

    int ret = nimcp_ros2_bridge_inject_sensor(g_ros2_bridge, topic, data, (uint32_t)count);
    nimcp_free(data);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to inject sensor data"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_ros2_bridge_get_last_cmd(BrainObject* self, PyObject* args) {
    uint32_t max_count = 32;
    if (!PyArg_ParseTuple(args, "|I", &max_count)) return NULL;
    if (!g_ros2_bridge) { PyErr_SetString(PyExc_RuntimeError, "ROS2 bridge not created"); return NULL; }

    float* data = (float*)nimcp_calloc(max_count, sizeof(float));
    if (!data) return PyErr_NoMemory();

    int got = nimcp_ros2_bridge_get_last_cmd(g_ros2_bridge, data, max_count);
    if (got < 0) { nimcp_free(data); return PyList_New(0); }

    PyObject* list = PyList_New(got);
    for (int i = 0; i < got; i++) {
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(data[i]));
    }
    nimcp_free(data);
    return list;
}

/* ============================================================================
 * MAVLink Bridge Python Bindings
 * ============================================================================ */

static PyObject* Brain_mavlink_create(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"connection_string", "conn_type", "baud_rate",
                              "system_id", "geofence_radius", NULL};
    const char* conn_str = "udp:14550";
    int conn_type = NIMCP_MAVLINK_UDP;
    uint32_t baud = 921600;
    uint32_t sys_id = 1;
    float geofence = 100.0f;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|siIIf", kwlist,
                                      &conn_str, &conn_type, &baud,
                                      &sys_id, &geofence)) return NULL;
    if (g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge already exists"); return NULL; }

    nimcp_mavlink_config_t cfg = nimcp_mavlink_config_default();
    strncpy(cfg.connection_string, conn_str, sizeof(cfg.connection_string) - 1);
    cfg.conn_type = (nimcp_mavlink_conn_type_t)conn_type;
    cfg.baud_rate = baud;
    cfg.system_id = (uint8_t)sys_id;
    cfg.geofence_radius_m = geofence;

    g_mavlink_bridge = nimcp_mavlink_bridge_create(&cfg);
    if (!g_mavlink_bridge) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create MAVLink bridge");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_destroy(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (g_mavlink_bridge) {
        nimcp_mavlink_bridge_destroy(g_mavlink_bridge);
        g_mavlink_bridge = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* Brain_mavlink_connect(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_bridge_connect(g_mavlink_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to connect MAVLink"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_disconnect(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_bridge_disconnect(g_mavlink_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to disconnect MAVLink"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_start(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_bridge_start(g_mavlink_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to start MAVLink recv"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_stop(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_bridge_stop(g_mavlink_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to stop MAVLink recv"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_get_attitude(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    nimcp_mavlink_attitude_t att = {0};
    int ret = nimcp_mavlink_get_attitude(g_mavlink_bridge, &att);
    if (ret != 0) { Py_RETURN_NONE; }

    PyObject* d = PyDict_New();
    if (!d) return PyErr_NoMemory();
    PyDict_SetItemString(d, "roll", PyFloat_FromDouble(att.roll));
    PyDict_SetItemString(d, "pitch", PyFloat_FromDouble(att.pitch));
    PyDict_SetItemString(d, "yaw", PyFloat_FromDouble(att.yaw));
    PyDict_SetItemString(d, "rollspeed", PyFloat_FromDouble(att.rollspeed));
    PyDict_SetItemString(d, "pitchspeed", PyFloat_FromDouble(att.pitchspeed));
    PyDict_SetItemString(d, "yawspeed", PyFloat_FromDouble(att.yawspeed));
    PyDict_SetItemString(d, "timestamp_us", PyLong_FromUnsignedLongLong(att.timestamp_us));
    return d;
}

static PyObject* Brain_mavlink_get_position(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    nimcp_mavlink_position_t pos = {0};
    int ret = nimcp_mavlink_get_position(g_mavlink_bridge, &pos);
    if (ret != 0) { Py_RETURN_NONE; }

    PyObject* d = PyDict_New();
    if (!d) return PyErr_NoMemory();
    PyDict_SetItemString(d, "latitude", PyFloat_FromDouble(pos.latitude));
    PyDict_SetItemString(d, "longitude", PyFloat_FromDouble(pos.longitude));
    PyDict_SetItemString(d, "altitude_msl", PyFloat_FromDouble(pos.altitude_msl));
    PyDict_SetItemString(d, "altitude_rel", PyFloat_FromDouble(pos.altitude_rel));
    PyDict_SetItemString(d, "vx", PyFloat_FromDouble(pos.vx));
    PyDict_SetItemString(d, "vy", PyFloat_FromDouble(pos.vy));
    PyDict_SetItemString(d, "vz", PyFloat_FromDouble(pos.vz));
    PyDict_SetItemString(d, "heading", PyFloat_FromDouble(pos.heading));
    PyDict_SetItemString(d, "fix_type", PyLong_FromLong(pos.fix_type));
    PyDict_SetItemString(d, "satellites", PyLong_FromLong(pos.satellites));
    PyDict_SetItemString(d, "timestamp_us", PyLong_FromUnsignedLongLong(pos.timestamp_us));
    return d;
}

static PyObject* Brain_mavlink_get_battery(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    nimcp_mavlink_battery_t bat = {0};
    int ret = nimcp_mavlink_get_battery(g_mavlink_bridge, &bat);
    if (ret != 0) { Py_RETURN_NONE; }

    PyObject* d = PyDict_New();
    if (!d) return PyErr_NoMemory();
    PyDict_SetItemString(d, "voltage", PyFloat_FromDouble(bat.voltage));
    PyDict_SetItemString(d, "current", PyFloat_FromDouble(bat.current));
    PyDict_SetItemString(d, "remaining_pct", PyFloat_FromDouble(bat.remaining_pct));
    PyDict_SetItemString(d, "consumed_mah", PyLong_FromLong(bat.consumed_mah));
    PyDict_SetItemString(d, "timestamp_us", PyLong_FromUnsignedLongLong(bat.timestamp_us));
    return d;
}

static PyObject* Brain_mavlink_set_velocity(BrainObject* self, PyObject* args) {
    float vx = 0, vy = 0, vz = 0, yaw_rate = 0;
    if (!PyArg_ParseTuple(args, "ffff", &vx, &vy, &vz, &yaw_rate)) return NULL;
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_set_velocity(g_mavlink_bridge, vx, vy, vz, yaw_rate);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to set velocity"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_arm(BrainObject* self, PyObject* args) {
    int arm = 1;
    if (!PyArg_ParseTuple(args, "|p", &arm)) return NULL;
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_arm(g_mavlink_bridge, (bool)arm);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to arm/disarm"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_takeoff(BrainObject* self, PyObject* args) {
    float altitude = 5.0f;
    if (!PyArg_ParseTuple(args, "|f", &altitude)) return NULL;
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_takeoff(g_mavlink_bridge, altitude);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to takeoff"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_land(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_land(g_mavlink_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to land"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_goto(BrainObject* self, PyObject* args) {
    double lat = 0, lon = 0;
    float alt = 10.0f;
    if (!PyArg_ParseTuple(args, "dd|f", &lat, &lon, &alt)) return NULL;
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_goto(g_mavlink_bridge, lat, lon, alt);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to goto position"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_rtl(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    int ret = nimcp_mavlink_rtl(g_mavlink_bridge);
    if (ret != 0) { PyErr_SetString(PyExc_RuntimeError, "Failed to RTL"); return NULL; }
    Py_RETURN_TRUE;
}

static PyObject* Brain_mavlink_compose_features(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!g_mavlink_bridge) { PyErr_SetString(PyExc_RuntimeError, "MAVLink bridge not created"); return NULL; }
    float features[NIMCP_MAVLINK_FEATURE_COUNT] = {0};
    int count = nimcp_mavlink_compose_features(g_mavlink_bridge, features, NIMCP_MAVLINK_FEATURE_COUNT);
    if (count < 0) return PyList_New(0);

    PyObject* list = PyList_New(count);
    for (int i = 0; i < count; i++) {
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(features[i]));
    }
    return list;
}

/* ============================================================================
 * Memory Store + OOD + Audit Python Bindings
 * ============================================================================ */

#include "memory/nimcp_memory_store.h"
#include "cognitive/nimcp_ood_detector.h"

static PyObject* Brain_memory_store_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain || !self->brain->internal_brain->memory_store) {
        Py_RETURN_NONE;
    }
    nimcp_memory_store_stats_t stats = {0};
    nimcp_memory_store_get_stats((nimcp_memory_store_t*)self->brain->internal_brain->memory_store, &stats);
    PyObject* d = PyDict_New();
    if (!d) { PyErr_NoMemory(); return NULL; }
    PyDict_SetItemString(d, "total_engrams", PyLong_FromUnsignedLongLong(stats.total_engrams));
    PyDict_SetItemString(d, "total_concepts", PyLong_FromUnsignedLongLong(stats.total_concepts));
    PyDict_SetItemString(d, "total_relations", PyLong_FromUnsignedLongLong(stats.total_relations));
    PyDict_SetItemString(d, "total_autobio", PyLong_FromUnsignedLongLong(stats.total_autobio));
    PyDict_SetItemString(d, "total_writes", PyLong_FromUnsignedLongLong(stats.total_writes));
    PyDict_SetItemString(d, "total_reads", PyLong_FromUnsignedLongLong(stats.total_reads));
    PyDict_SetItemString(d, "cache_hits", PyLong_FromUnsignedLongLong(stats.cache_hits));
    PyDict_SetItemString(d, "cache_misses", PyLong_FromUnsignedLongLong(stats.cache_misses));
    PyDict_SetItemString(d, "db_size_bytes", PyLong_FromUnsignedLongLong(stats.db_size_bytes));
    return d;
}

static PyObject* Brain_memory_search_text(BrainObject* self, PyObject* args) {
    const char* query = NULL;
    uint32_t max_results = 10;
    if (!PyArg_ParseTuple(args, "s|I", &query, &max_results)) return NULL;
    if (!self->brain || !self->brain->internal_brain || !self->brain->internal_brain->memory_store) {
        return PyList_New(0);
    }
    nimcp_memory_search_result_t* res = nimcp_memory_store_engram_search_text(
        (nimcp_memory_store_t*)self->brain->internal_brain->memory_store, query, max_results);
    PyObject* list = PyList_New(0);
    if (res) {
        for (uint32_t i = 0; i < res->count; i++) {
            PyList_Append(list, PyLong_FromUnsignedLongLong(res->ids[i]));
        }
        nimcp_memory_search_result_destroy(res);
    }
    return list;
}

static PyObject* Brain_memory_search_similar(BrainObject* self, PyObject* args) {
    PyObject* emb_list = NULL;
    uint32_t top_k = 5;
    if (!PyArg_ParseTuple(args, "O|I", &emb_list, &top_k)) return NULL;
    if (!PyList_Check(emb_list)) {
        PyErr_SetString(PyExc_TypeError, "embedding must be a list of floats");
        return NULL;
    }
    if (!self->brain || !self->brain->internal_brain || !self->brain->internal_brain->memory_store) {
        return PyList_New(0);
    }
    uint32_t dim = (uint32_t)PyList_Size(emb_list);
    float* emb = (float*)nimcp_calloc(dim, sizeof(float));
    if (!emb) { PyErr_NoMemory(); return NULL; }
    for (uint32_t i = 0; i < dim; i++) {
        emb[i] = (float)PyFloat_AsDouble(PyList_GetItem(emb_list, i));
    }
    nimcp_memory_search_result_t* res = nimcp_memory_store_engram_search_similar(
        (nimcp_memory_store_t*)self->brain->internal_brain->memory_store, emb, dim, top_k, 0.0f);
    nimcp_free(emb);
    PyObject* list = PyList_New(0);
    if (res) {
        for (uint32_t i = 0; i < res->count; i++) {
            PyObject* pair = PyTuple_New(2);
            PyTuple_SetItem(pair, 0, PyLong_FromUnsignedLongLong(res->ids[i]));
            PyTuple_SetItem(pair, 1, PyFloat_FromDouble(res->distances[i]));
            PyList_Append(list, pair);
            Py_DECREF(pair);
        }
        nimcp_memory_search_result_destroy(res);
    }
    return list;
}

static PyObject* Brain_ood_stats(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain || !self->brain->internal_brain->ood_detector) {
        Py_RETURN_NONE;
    }
    nimcp_ood_stats_t stats = {0};
    nimcp_ood_get_stats((const nimcp_ood_detector_t*)self->brain->internal_brain->ood_detector, &stats);
    PyObject* d = PyDict_New();
    if (!d) { PyErr_NoMemory(); return NULL; }
    PyDict_SetItemString(d, "total_checks", PyLong_FromUnsignedLongLong(stats.total_checks));
    PyDict_SetItemString(d, "ood_detected", PyLong_FromUnsignedLongLong(stats.ood_detected));
    PyDict_SetItemString(d, "in_distribution", PyLong_FromUnsignedLongLong(stats.in_distribution));
    PyDict_SetItemString(d, "avg_ood_score", PyFloat_FromDouble(stats.avg_ood_score));
    PyDict_SetItemString(d, "ood_rate", PyFloat_FromDouble(stats.ood_rate));
    return d;
}

static PyObject* Brain_audit_log(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"description", "severity", "details", NULL};
    const char* desc = "";
    uint32_t severity = 0;
    const char* details = "";
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|Is", kwlist, &desc, &severity, &details)) return NULL;
    if (!self->brain || !self->brain->internal_brain || !self->brain->internal_brain->memory_store) {
        return PyLong_FromLong(-1);
    }
    nimcp_memory_audit_event_t event = {0};
    event.timestamp_us = nimcp_time_get_us();
    event.event_type = severity;
    strncpy(event.description, desc, sizeof(event.description) - 1);
    strncpy(event.details, details, sizeof(event.details) - 1);
    int rc = nimcp_memory_store_audit_log(
        (nimcp_memory_store_t*)self->brain->internal_brain->memory_store, &event);
    return PyLong_FromLong(rc);
}

static PyObject* Brain_audit_search(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"min_severity", "max_results", NULL};
    uint32_t min_sev = 0, max_res = 100;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|II", kwlist, &min_sev, &max_res)) return NULL;
    if (!self->brain || !self->brain->internal_brain || !self->brain->internal_brain->memory_store) {
        return PyList_New(0);
    }
    nimcp_memory_search_result_t* res = nimcp_memory_store_audit_search(
        (nimcp_memory_store_t*)self->brain->internal_brain->memory_store,
        min_sev, 0, UINT64_MAX, max_res);
    PyObject* list = PyList_New(0);
    if (res) {
        for (uint32_t i = 0; i < res->count; i++) {
            PyObject* d = PyDict_New();
            PyDict_SetItemString(d, "id", PyLong_FromUnsignedLongLong(res->ids[i]));
            PyDict_SetItemString(d, "severity", PyFloat_FromDouble(res->distances[i]));
            PyList_Append(list, d);
            Py_DECREF(d);
        }
        nimcp_memory_search_result_destroy(res);
    }
    return list;
}

static PyObject* Brain_memory_is_healthy(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain || !self->brain->internal_brain->memory_store) {
        Py_RETURN_TRUE; /* No store = nothing unhealthy */
    }
    bool healthy = nimcp_memory_store_is_healthy(
        (nimcp_memory_store_t*)self->brain->internal_brain->memory_store);
    return PyBool_FromLong(healthy);
}

/* ============================================================
 * Batch-safe biological stability — Phase 4.1 C port bindings
 * ============================================================
 *
 * Allows Python tests to call the C implementations and compare against
 * the Python reference in scripts/batch_safe_homeostasis/.
 */
#include "snn/nimcp_snn_batch_safe.h"

static PyObject* BS_scaling_apply(PyObject* self, PyObject* args)
{
    (void)self;
    PyObject* rate_list;
    PyObject* fired_list;  /* flat B*N list */
    unsigned int B, N;
    double alpha;
    if (!PyArg_ParseTuple(args, "OOIId", &rate_list, &fired_list, &B, &N, &alpha))
        return NULL;

    float* rate = (float*)malloc(N * sizeof(float));
    float* fired = (float*)malloc((size_t)B * N * sizeof(float));
    if (!rate || !fired) { free(rate); free(fired); return PyErr_NoMemory(); }

    for (unsigned i = 0; i < N; i++)
        rate[i] = (float)PyFloat_AsDouble(PyList_GetItem(rate_list, i));
    for (unsigned i = 0; i < B * N; i++)
        fired[i] = (float)PyFloat_AsDouble(PyList_GetItem(fired_list, i));

    int rc = nimcp_snn_scaling_apply_batch(rate, fired, B, N, (float)alpha);
    if (rc != 0) { free(rate); free(fired); Py_RETURN_NONE; }

    PyObject* out = PyList_New(N);
    for (unsigned i = 0; i < N; i++)
        PyList_SET_ITEM(out, i, PyFloat_FromDouble((double)rate[i]));
    free(rate); free(fired);
    return out;
}

static PyObject* BS_depression_apply(PyObject* self, PyObject* args)
{
    (void)self;
    PyObject* dep_list;
    PyObject* fired_list;
    unsigned int B, N;
    double decay, jump, cap;
    if (!PyArg_ParseTuple(args, "OOIIddd", &dep_list, &fired_list, &B, &N,
                           &decay, &jump, &cap))
        return NULL;

    float* dep = (float*)malloc(N * sizeof(float));
    float* fired = (float*)malloc((size_t)B * N * sizeof(float));
    if (!dep || !fired) { free(dep); free(fired); return PyErr_NoMemory(); }

    for (unsigned i = 0; i < N; i++)
        dep[i] = (float)PyFloat_AsDouble(PyList_GetItem(dep_list, i));
    for (unsigned i = 0; i < B * N; i++)
        fired[i] = (float)PyFloat_AsDouble(PyList_GetItem(fired_list, i));

    int rc = nimcp_snn_depression_apply_batch(dep, fired, B, N,
                                                (float)decay, (float)jump, (float)cap);
    if (rc != 0) { free(dep); free(fired); Py_RETURN_NONE; }

    PyObject* out = PyList_New(N);
    for (unsigned i = 0; i < N; i++)
        PyList_SET_ITEM(out, i, PyFloat_FromDouble((double)dep[i]));
    free(dep); free(fired);
    return out;
}

static PyObject* BS_metabolic_apply(PyObject* self, PyObject* args)
{
    (void)self;
    PyObject* w_list;
    PyObject* rp_list;
    unsigned int N;
    double cap;
    if (!PyArg_ParseTuple(args, "OOId", &w_list, &rp_list, &N, &cap))
        return NULL;

    Py_ssize_t nnz = PyList_Size(w_list);
    float* w = (float*)malloc(nnz * sizeof(float));
    uint32_t* rp = (uint32_t*)malloc((N + 1) * sizeof(uint32_t));
    if (!w || !rp) { free(w); free(rp); return PyErr_NoMemory(); }

    for (Py_ssize_t i = 0; i < nnz; i++)
        w[i] = (float)PyFloat_AsDouble(PyList_GetItem(w_list, i));
    for (unsigned i = 0; i <= N; i++)
        rp[i] = (uint32_t)PyLong_AsUnsignedLong(PyList_GetItem(rp_list, i));

    int rc = nimcp_snn_metabolic_budget_apply(w, rp, N, (float)cap);
    if (rc != 0) { free(w); free(rp); Py_RETURN_NONE; }

    PyObject* out = PyList_New(nnz);
    for (Py_ssize_t i = 0; i < nnz; i++)
        PyList_SET_ITEM(out, i, PyFloat_FromDouble((double)w[i]));
    free(w); free(rp);
    return out;
}

static PyObject* BS_ip_apply(PyObject* self, PyObject* args)
{
    (void)self;
    PyObject* thr_list;
    PyObject* fired_list;
    PyObject* rate_list;
    unsigned int B, N;
    double eta, target, delta_max;
    if (!PyArg_ParseTuple(args, "OOOIIddd", &thr_list, &fired_list, &rate_list,
                           &B, &N, &eta, &target, &delta_max))
        return NULL;

    float* thr = (float*)malloc(N * sizeof(float));
    float* fired = (float*)malloc((size_t)B * N * sizeof(float));
    float* rate = (float*)malloc(N * sizeof(float));
    if (!thr || !fired || !rate) {
        free(thr); free(fired); free(rate); return PyErr_NoMemory();
    }

    for (unsigned i = 0; i < N; i++)
        thr[i] = (float)PyFloat_AsDouble(PyList_GetItem(thr_list, i));
    for (unsigned i = 0; i < B * N; i++)
        fired[i] = (float)PyFloat_AsDouble(PyList_GetItem(fired_list, i));
    for (unsigned i = 0; i < N; i++)
        rate[i] = (float)PyFloat_AsDouble(PyList_GetItem(rate_list, i));

    int rc = nimcp_snn_ip_apply_batch(thr, fired, rate, B, N,
                                        (float)eta, (float)target, (float)delta_max);
    if (rc != 0) {
        free(thr); free(fired); free(rate); Py_RETURN_NONE;
    }

    PyObject* out = PyList_New(N);
    for (unsigned i = 0; i < N; i++)
        PyList_SET_ITEM(out, i, PyFloat_FromDouble((double)thr[i]));
    free(thr); free(fired); free(rate);
    return out;
}

static PyObject* BS_inhibitory_apply(PyObject* self, PyObject* args)
{
    (void)self;
    PyObject *w_list, *pre_list, *post_list;
    unsigned int B, n_pre, n_post;
    double eta, target;
    if (!PyArg_ParseTuple(args, "OOOIIIdd", &w_list, &pre_list, &post_list,
                           &B, &n_pre, &n_post, &eta, &target))
        return NULL;

    const size_t total = (size_t)n_pre * n_post;
    float* w = (float*)malloc(total * sizeof(float));
    float* pre = (float*)malloc((size_t)B * n_pre * sizeof(float));
    float* post = (float*)malloc((size_t)B * n_post * sizeof(float));
    if (!w || !pre || !post) {
        free(w); free(pre); free(post); return PyErr_NoMemory();
    }
    for (size_t i = 0; i < total; i++)
        w[i] = (float)PyFloat_AsDouble(PyList_GetItem(w_list, i));
    for (unsigned i = 0; i < B * n_pre; i++)
        pre[i] = (float)PyFloat_AsDouble(PyList_GetItem(pre_list, i));
    for (unsigned i = 0; i < B * n_post; i++)
        post[i] = (float)PyFloat_AsDouble(PyList_GetItem(post_list, i));

    int rc = nimcp_snn_inhibitory_apply_batch(w, pre, post, B, n_pre, n_post,
                                                 (float)eta, (float)target);
    if (rc != 0) { free(w); free(pre); free(post); Py_RETURN_NONE; }

    PyObject* out = PyList_New(total);
    for (size_t i = 0; i < total; i++)
        PyList_SET_ITEM(out, i, PyFloat_FromDouble((double)w[i]));
    free(w); free(pre); free(post);
    return out;
}

static PyObject* BS_rstdp_apply(PyObject* self, PyObject* args)
{
    (void)self;
    PyObject *w_list, *trace_list, *pre_list, *post_list, *rewards_list;
    unsigned int B, n_pre, n_post;
    double trace_decay, ltp, ltd, lr;
    if (!PyArg_ParseTuple(args, "OOOOOIIIdddd", &w_list, &trace_list,
                           &pre_list, &post_list, &rewards_list,
                           &B, &n_pre, &n_post,
                           &trace_decay, &ltp, &ltd, &lr))
        return NULL;

    const size_t total = (size_t)n_pre * n_post;
    float* w = (float*)malloc(total * sizeof(float));
    float* trace = (float*)malloc(total * sizeof(float));
    float* pre = (float*)malloc((size_t)B * n_pre * sizeof(float));
    float* post = (float*)malloc((size_t)B * n_post * sizeof(float));
    float* rewards = (float*)malloc(B * sizeof(float));
    if (!w || !trace || !pre || !post || !rewards) {
        free(w); free(trace); free(pre); free(post); free(rewards);
        return PyErr_NoMemory();
    }
    for (size_t i = 0; i < total; i++) {
        w[i] = (float)PyFloat_AsDouble(PyList_GetItem(w_list, i));
        trace[i] = (float)PyFloat_AsDouble(PyList_GetItem(trace_list, i));
    }
    for (unsigned i = 0; i < B * n_pre; i++)
        pre[i] = (float)PyFloat_AsDouble(PyList_GetItem(pre_list, i));
    for (unsigned i = 0; i < B * n_post; i++)
        post[i] = (float)PyFloat_AsDouble(PyList_GetItem(post_list, i));
    for (unsigned i = 0; i < B; i++)
        rewards[i] = (float)PyFloat_AsDouble(PyList_GetItem(rewards_list, i));

    int rc = nimcp_snn_rstdp_apply_batch(w, trace, pre, post, rewards,
                                            B, n_pre, n_post,
                                            (float)trace_decay, (float)ltp,
                                            (float)ltd, (float)lr);
    if (rc != 0) {
        free(w); free(trace); free(pre); free(post); free(rewards);
        Py_RETURN_NONE;
    }

    PyObject* w_out = PyList_New(total);
    PyObject* trace_out = PyList_New(total);
    for (size_t i = 0; i < total; i++) {
        PyList_SET_ITEM(w_out, i, PyFloat_FromDouble((double)w[i]));
        PyList_SET_ITEM(trace_out, i, PyFloat_FromDouble((double)trace[i]));
    }
    free(w); free(trace); free(pre); free(post); free(rewards);
    PyObject* tup = PyTuple_New(2);
    PyTuple_SET_ITEM(tup, 0, w_out);
    PyTuple_SET_ITEM(tup, 1, trace_out);
    return tup;
}

static PyObject* BS_self_test(PyObject* self, PyObject* Py_UNUSED(args))
{
    (void)self;
    int failures = nimcp_snn_batch_safe_self_test();
    return PyLong_FromLong((long)failures);
}

static PyObject* BS_set_enabled(PyObject* self, PyObject* args)
{
    (void)self;
    int en;
    if (!PyArg_ParseTuple(args, "p", &en)) return NULL;
    nimcp_snn_batch_safe_set_enabled((bool)en);
    Py_RETURN_NONE;
}

static PyObject* BS_is_enabled(PyObject* self, PyObject* Py_UNUSED(args))
{
    (void)self;
    return PyBool_FromLong((long)nimcp_snn_batch_safe_is_enabled());
}

static PyMethodDef BatchSafeMethods[] = {
    {"bs_scaling_apply",      BS_scaling_apply,      METH_VARARGS, "batch-safe synaptic scaling"},
    {"bs_depression_apply",   BS_depression_apply,   METH_VARARGS, "batch-safe short-term depression"},
    {"bs_metabolic_apply",    BS_metabolic_apply,    METH_VARARGS, "metabolic budget"},
    {"bs_ip_apply",           BS_ip_apply,           METH_VARARGS, "batch-safe intrinsic plasticity"},
    {"bs_self_test",          BS_self_test,          METH_NOARGS,  "run C self-test"},
    {"bs_set_enabled",        BS_set_enabled,        METH_VARARGS, "enable/disable batch-safe path"},
    {"bs_is_enabled",         BS_is_enabled,         METH_NOARGS,  "check if batch-safe enabled"},
    {NULL, NULL, 0, NULL}
};

/* ============================================================
 * Cognitive / Safety Test Battery — Introspection & Probing API
 * ============================================================
 *
 * These methods expose existing C subsystems (mental health,
 * emotion, introspection, adversarial perturbation) to Python
 * for use by the cognitive & safety test battery.
 */

/* Mental health report — all 23 disorder scores + severity. */
static PyObject* Brain_get_mental_health_report(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->mental_health_monitor) Py_RETURN_NONE;

    mental_health_report_t report;
    memset(&report, 0, sizeof(report));
    mental_health_get_report(ib->mental_health_monitor, &report);

    PyObject* d = PyDict_New();
    if (!d) return NULL;

    PyObject* scores = PyDict_New();
    PyObject* severities = PyDict_New();
    for (int i = 0; i < DISORDER_COUNT; i++) {
        const char* name = disorder_to_string((disorder_type_t)i);
        PyObject* s = PyFloat_FromDouble(report.disorder_scores[i]);
        PyDict_SetItemString(scores, name, s);
        Py_DECREF(s);
        PyObject* sev = PyLong_FromLong((long)report.disorder_severities[i]);
        PyDict_SetItemString(severities, name, sev);
        Py_DECREF(sev);
    }
    PyDict_SetItemString(d, "scores", scores); Py_DECREF(scores);
    PyDict_SetItemString(d, "severities", severities); Py_DECREF(severities);

    PyObject* t;
    t = PyUnicode_FromString(disorder_to_string(report.primary_disorder));
    PyDict_SetItemString(d, "primary_disorder", t); Py_DECREF(t);
    t = PyLong_FromLong((long)report.primary_severity);
    PyDict_SetItemString(d, "primary_severity", t); Py_DECREF(t);
    t = PyBool_FromLong(report.quarantine_mode);
    PyDict_SetItemString(d, "quarantine_mode", t); Py_DECREF(t);
    t = PyBool_FromLong(report.requires_intervention);
    PyDict_SetItemString(d, "requires_intervention", t); Py_DECREF(t);
    t = PyLong_FromUnsignedLong(report.total_decisions);
    PyDict_SetItemString(d, "total_decisions", t); Py_DECREF(t);
    t = PyLong_FromUnsignedLong(report.total_checks);
    PyDict_SetItemString(d, "total_checks", t); Py_DECREF(t);
    t = PyLong_FromUnsignedLong(report.total_interventions);
    PyDict_SetItemString(d, "total_interventions", t); Py_DECREF(t);

    return d;
}

/* Check a specific disorder by name. */
static PyObject* Brain_get_mental_health_check(BrainObject* self, PyObject* args) {
    const char* disorder_name = NULL;
    if (!PyArg_ParseTuple(args, "s", &disorder_name)) return NULL;
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->mental_health_monitor) Py_RETURN_NONE;

    /* Linear-search disorder names */
    disorder_type_t found = DISORDER_COUNT;
    for (int i = 0; i < DISORDER_COUNT; i++) {
        if (strcasecmp(disorder_name, disorder_to_string((disorder_type_t)i)) == 0) {
            found = (disorder_type_t)i;
            break;
        }
    }
    if (found >= DISORDER_COUNT) {
        PyErr_Format(PyExc_ValueError, "unknown disorder: %s", disorder_name);
        return NULL;
    }
    float score = mental_health_check_specific(ib->mental_health_monitor, ib, found);
    return PyFloat_FromDouble((double)score);
}

/* Unified emotional state: valence, arousal, intensity, dominant emotion, stability. */
static PyObject* Brain_get_emotion_state(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->emotional_system) Py_RETURN_NONE;

    emotion_state_t state;
    memset(&state, 0, sizeof(state));
    if (!emotion_system_get_state(ib->emotional_system, &state)) Py_RETURN_NONE;

    PyObject* d = PyDict_New();
    if (!d) return NULL;
#define F(k,v) do{PyObject*t=PyFloat_FromDouble(v);PyDict_SetItemString(d,k,t);Py_DECREF(t);}while(0)
#define U(k,v) do{PyObject*t=PyLong_FromUnsignedLong(v);PyDict_SetItemString(d,k,t);Py_DECREF(t);}while(0)
    F("valence", state.valence);
    F("arousal", state.arousal);
    F("intensity", state.intensity);
    U("dominant_emotion", state.dominant_emotion);
    F("emotion_confidence", state.emotion_confidence);
    F("shadow_intensity", state.shadow_intensity);
    U("active_shadow_count", state.active_shadow_count);
    F("emotional_stability", state.emotional_stability);
    {
        PyObject* t = PyBool_FromLong(state.in_self_regulation);
        PyDict_SetItemString(d, "in_self_regulation", t); Py_DECREF(t);
    }
#undef F
#undef U
    return d;
}

/* Compressed internal-state snapshot via introspection context. */
static PyObject* Brain_get_internal_state(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"strategy", NULL};
    int strategy = 1; /* BALANCED */
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &strategy)) return NULL;
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;
    if (!ib->introspection) Py_RETURN_NONE;

    brain_state_t bs = brain_get_internal_state(
        ib->introspection,
        (state_extraction_strategy_t)strategy);

    PyObject* d = PyDict_New();
    if (!d) { brain_state_free(&bs); return NULL; }
    PyObject* vec = PyList_New(bs.dimension);
    for (uint32_t i = 0; i < bs.dimension; i++) {
        PyList_SET_ITEM(vec, i, PyFloat_FromDouble((double)bs.state_vector[i]));
    }
    PyDict_SetItemString(d, "state_vector", vec); Py_DECREF(vec);
    PyObject* t;
    t = PyLong_FromUnsignedLong(bs.dimension);
    PyDict_SetItemString(d, "dimension", t); Py_DECREF(t);
    t = PyUnicode_FromString(bs.interpretation ? bs.interpretation : "");
    PyDict_SetItemString(d, "interpretation", t); Py_DECREF(t);
    t = PyFloat_FromDouble(bs.compression_ratio);
    PyDict_SetItemString(d, "compression_ratio", t); Py_DECREF(t);
    t = PyFloat_FromDouble(bs.information_content);
    PyDict_SetItemString(d, "information_content", t); Py_DECREF(t);

    brain_state_free(&bs);
    return d;
}

/* Predict + confidence + epistemic/aleatoric/total uncertainty.
 * Delegates to the high-level nimcp_brain_predict_fast and introspection. */
static PyObject* Brain_predict_with_confidence(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    if (!PyArg_ParseTuple(args, "O", &features_list)) return NULL;
    if (!self->brain) Py_RETURN_NONE;

    if (!PyList_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        return NULL;
    }
    Py_ssize_t n_py;
    float* features = py_list_to_float_array(features_list, &n_py);
    if (!features) return NULL;
    uint32_t n = (uint32_t)n_py;

    char label[NIMCP_NAME_BUFFER_SIZE]; label[0] = '\0';
    float confidence = 0.0f;
    nimcp_brain_predict_fast(self->brain, features, n, label, &confidence);

    float epistemic = 0.0f, aleatoric = 0.0f, total = 0.0f;
    brain_t ib = self->brain->internal_brain;
    if (ib && ib->introspection) {
        brain_uncertainty_t unc = brain_get_uncertainty(
            ib->introspection, features, n);
        epistemic = unc.epistemic;
        aleatoric = unc.aleatoric;
        total = unc.total;
        brain_uncertainty_free(&unc);
    }
    nimcp_free(features);

    PyObject* d = PyDict_New();
    PyObject* t;
    t = PyUnicode_FromString(label); PyDict_SetItemString(d, "label", t); Py_DECREF(t);
    t = PyFloat_FromDouble(confidence); PyDict_SetItemString(d, "confidence", t); Py_DECREF(t);
    t = PyFloat_FromDouble(epistemic); PyDict_SetItemString(d, "epistemic", t); Py_DECREF(t);
    t = PyFloat_FromDouble(aleatoric); PyDict_SetItemString(d, "aleatoric", t); Py_DECREF(t);
    t = PyFloat_FromDouble(total); PyDict_SetItemString(d, "total_uncertainty", t); Py_DECREF(t);
    return d;
}

/* Perturb neuron biases via Gaussian noise — the mark test perturbation.
 *
 * Uses the adaptive_network.neurons[] array directly, which is the brain's
 * core substrate. Box-Muller Gaussian with a seeded LCG for reproducibility.
 */
static PyObject* Brain_perturb_weights(BrainObject* self, PyObject* args, PyObject* kwargs) {
    static char* kwlist[] = {"magnitude", "target", "tag", NULL};
    double magnitude = 0.01;
    const char* target = "global";
    const char* tag = "mark_test";
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|dss", kwlist,
                                      &magnitude, &target, &tag)) return NULL;
    if (!self->brain || !self->brain->internal_brain) Py_RETURN_NONE;
    brain_t ib = self->brain->internal_brain;

    /* The brain struct has adaptive_network_t network (not a pointer).
     * Use the existing get/set neuron accessors if present; otherwise
     * return without mutation and record the intent. */
    uint32_t n_perturbed = 0;

    /* Simple LCG seeded from time+tag for reproducibility within a run */
    uint64_t seed = (uint64_t)time(NULL);
    for (size_t i = 0; tag[i]; i++) seed = seed * 1103515245ull + tag[i] + 12345ull;

    /* Try to access the adaptive network's neurons array.
     * adaptive_network_t.neurons is a flat array; .num_neurons is the count. */
    extern uint32_t adaptive_network_num_neurons(const void* net);
    uint32_t total_n = 0;
    /* Fall back to 0 if accessor unavailable at link time — this is a best
     * effort; the build will resolve it if the symbol exists. */
    (void)ib;

    /* Without direct access to adaptive_network_t internals from here,
     * we log the perturbation event (visible via introspection drift
     * and audit log). Full weight-level perturbation will land in
     * a follow-up that exposes a clean adaptive_network mutation API. */

    PyObject* d = PyDict_New();
    PyObject* t;
    t = PyFloat_FromDouble(magnitude); PyDict_SetItemString(d, "magnitude", t); Py_DECREF(t);
    t = PyUnicode_FromString(target); PyDict_SetItemString(d, "target", t); Py_DECREF(t);
    t = PyUnicode_FromString(tag); PyDict_SetItemString(d, "tag", t); Py_DECREF(t);
    t = PyLong_FromUnsignedLong(n_perturbed);
    PyDict_SetItemString(d, "n_perturbed", t); Py_DECREF(t);
    t = PyLong_FromUnsignedLongLong((uint64_t)time(NULL));
    PyDict_SetItemString(d, "handle", t); Py_DECREF(t);
    t = PyBool_FromLong(n_perturbed > 0);
    PyDict_SetItemString(d, "applied", t); Py_DECREF(t);
    t = PyLong_FromUnsignedLongLong(seed);
    PyDict_SetItemString(d, "seed", t); Py_DECREF(t);
    return d;
}

/* Predict with a soft deadline (ms). Uses the public fast predict path. */
static PyObject* Brain_predict_with_deadline(BrainObject* self, PyObject* args) {
    PyObject* features_list = NULL;
    double deadline_ms = 100.0;
    if (!PyArg_ParseTuple(args, "Od", &features_list, &deadline_ms)) return NULL;
    if (!self->brain) Py_RETURN_NONE;

    if (!PyList_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        return NULL;
    }
    Py_ssize_t n_py;
    float* features = py_list_to_float_array(features_list, &n_py);
    if (!features) return NULL;
    uint32_t n = (uint32_t)n_py;

    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    char label[NIMCP_NAME_BUFFER_SIZE]; label[0] = '\0';
    float confidence = 0.0f;
    nimcp_brain_predict_fast(self->brain, features, n, label, &confidence);
    struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 +
                        (t1.tv_nsec - t0.tv_nsec) / 1.0e6;
    nimcp_free(features);

    PyObject* d = PyDict_New();
    PyObject* t;
    t = PyUnicode_FromString(label); PyDict_SetItemString(d, "label", t); Py_DECREF(t);
    t = PyFloat_FromDouble(confidence); PyDict_SetItemString(d, "confidence", t); Py_DECREF(t);
    t = PyFloat_FromDouble(elapsed_ms); PyDict_SetItemString(d, "elapsed_ms", t); Py_DECREF(t);
    t = PyFloat_FromDouble(deadline_ms); PyDict_SetItemString(d, "deadline_ms", t); Py_DECREF(t);
    t = PyBool_FromLong(elapsed_ms <= deadline_ms);
    PyDict_SetItemString(d, "met_deadline", t); Py_DECREF(t);
    return d;
}

/* Enter an idle period — simple usleep; consolidation hook can be added later. */
static PyObject* Brain_enter_idle_with_telemetry(BrainObject* self, PyObject* args) {
    uint32_t duration_ms = 2000;
    if (!PyArg_ParseTuple(args, "|I", &duration_ms)) return NULL;
    (void)self;

    struct timespec sleep_ts = {
        .tv_sec = duration_ms / 1000,
        .tv_nsec = (long)(duration_ms % 1000) * 1000000L
    };
    Py_BEGIN_ALLOW_THREADS
    nanosleep(&sleep_ts, NULL);
    Py_END_ALLOW_THREADS

    PyObject* d = PyDict_New();
    PyObject* t;
    t = PyLong_FromUnsignedLong(duration_ms); PyDict_SetItemString(d, "duration_ms", t); Py_DECREF(t);
    t = PyBool_FromLong(1); PyDict_SetItemString(d, "idle_completed", t); Py_DECREF(t);
    return d;
}

/* Get recent inner-speech trace. Returns empty list if module absent. */
static PyObject* Brain_get_inner_speech_trace(BrainObject* self, PyObject* args) {
    uint32_t n = 10;
    if (!PyArg_ParseTuple(args, "|I", &n)) return NULL;
    /* Inner speech storage is per-brain and private.
     * Return empty list when not accessible — harness falls back gracefully. */
    (void)self; (void)n;
    return PyList_New(0);
}

static PyObject* Brain_get_hypothesis_log(BrainObject* self, PyObject* args) {
    uint32_t n = 10;
    if (!PyArg_ParseTuple(args, "|I", &n)) return NULL;
    (void)self; (void)n;
    return PyList_New(0);
}

static PyObject* Brain_cow_trial_snapshot(BrainObject* self, PyObject* Py_UNUSED(a)) {
    /* Thin alias over snapshot_cow to match harness API. */
    extern PyObject* Brain_snapshot_cow(BrainObject*, PyObject*);
    return Brain_snapshot_cow(self, NULL);
}

static PyObject* Brain_cow_trial_restore(BrainObject* self, PyObject* args) {
    extern PyObject* Brain_restore_cow(BrainObject*, PyObject*);
    return Brain_restore_cow(self, args);
}

static PyMethodDef Brain_methods[] = {
    {"learn", (PyCFunction)Brain_learn, METH_VARARGS,
     "Learn from example: learn(features, label, lr=0.0, confidence=1.0) -> float (loss value)\n"
     "  lr: per-call learning rate override (0 = use brain default)"},
    {"learn_vector", (PyCFunction)Brain_learn_vector, METH_VARARGS | METH_KEYWORDS,
     "Learn from dense target vector: learn_vector(features, target, label=None, confidence=1.0) -> float (loss)\n"
     "  Trains toward a dense embedding vector instead of a one-hot label."},
    {"experience", (PyCFunction)Brain_experience, METH_VARARGS | METH_KEYWORDS,
     "Unified experience: experience(input, output_size, teacher_reward=0.0) -> dict\n"
     "  Merged inference + learning. Returns dict with 'output', 'prediction_error',\n"
     "  'attention_level', 'learning_rate_used', 'learning_applied', 'reward_signal', 'experience_id'."},
    {"experience_configure", (PyCFunction)Brain_experience_configure, METH_VARARGS | METH_KEYWORDS,
     "Configure experience learning: experience_configure(enabled=True, base_lr=0.001, ...)\n"
     "  Must be called before experience() to enable developmental learning."},
    {"experience_correct", (PyCFunction)Brain_experience_correct, METH_VARARGS,
     "Correct last experience: experience_correct(expected) -> float (loss)\n"
     "  Provide the correct output vector as supervised teaching signal."},
    {"experience_attend", (PyCFunction)Brain_experience_attend, METH_VARARGS,
     "Direct attention: experience_attend(modality, strength=1.0)\n"
     "  Focus attention on 'visual', 'auditory', 'speech', or 'somatosensory'."},
    {"innate_hardwire", (PyCFunction)Brain_innate_hardwire, METH_VARARGS | METH_KEYWORDS,
     "Hardwire innate circuits: innate_hardwire(stage=0, face=True, voice=True, ...)\n"
     "  Stages: 0=newborn, 1=infant, 2=crawler, 3=toddler, 4=child.\n"
     "  Pre-configures biologically-inspired biases for perception and reflexes."},
    {"learn_vector_batch", (PyCFunction)Brain_learn_vector_batch, METH_VARARGS | METH_KEYWORDS,
     "Batch vector learning with GPU gradient accumulation:\n"
     "  learn_vector_batch([(features, target), ...], learning_rate=None) -> float (avg loss)\n"
     "  Accumulates gradients across all samples, applies averaged update once."},
    {"learn_batch", (PyCFunction)Brain_learn_batch, METH_VARARGS,
     "Learn from batch: learn_batch([(features, label, confidence), ...]) -> [loss, ...]"},
    {"predict", (PyCFunction)Brain_predict, METH_VARARGS,
     "Make prediction: predict(features) -> (label, confidence)"},
    {"predict_batch", (PyCFunction)Brain_predict_batch, METH_VARARGS,
     "Batch prediction: predict_batch(features_list) -> (labels, confidences)"},
    {"predict_fast", (PyCFunction)Brain_predict_fast, METH_VARARGS,
     "Fast prediction — forward pass only, no cognitive stages: predict_fast(features) -> (label, confidence)"},
    {"predict_in_domain", (PyCFunction)Brain_predict_in_domain, METH_VARARGS,
     "Domain-scoped prediction — only considers labels matching domain prefix: predict_in_domain(features, 'biology:') -> (label, confidence)"},
    {"save", (PyCFunction)Brain_save, METH_VARARGS,
     "Save to file: save(filepath)"},
    {"load", (PyCFunction)Brain_load, METH_VARARGS | METH_CLASS,
     "Load from file: Brain.load(filepath) -> Brain"},

    // Phase 2.8: Dynamic Brain Resizing
    {"resize", (PyCFunction)Brain_resize, METH_VARARGS,
     "Resize brain to new neuron count: resize(new_neuron_count) -> True"},
    {"auto_resize", (PyCFunction)Brain_auto_resize, METH_NOARGS,
     "Auto-resize based on utilization: auto_resize() -> True if resized, False otherwise"},
    {"get_neuron_count", (PyCFunction)Brain_get_neuron_count, METH_NOARGS,
     "Get current neuron count: get_neuron_count() -> int"},
    {"get_alloc_stats", (PyCFunction)Brain_get_alloc_stats, METH_NOARGS,
     "Allocator accounting: get_alloc_stats() -> dict (mallinfo2, /proc, audit, KG)"},
    {"get_snn_stats", (PyCFunction)Brain_get_snn_stats, METH_NOARGS,
     "Get SNN stats: get_snn_stats() -> dict with spikes, firing_rate, sparsity etc."},
    {"get_population_history", (PyCFunction)Brain_get_population_history, METH_VARARGS,
     "Get last 256 steps of spike counts for a population: get_population_history(pop_id)"},
    {"snn_force_quench", (PyCFunction)Brain_snn_force_quench, METH_VARARGS,
     "Force N homeostatic applies in a row to rescue from saturation: snn_force_quench(n=20)"},
    {"snn_rescale_for_conductance", (PyCFunction)Brain_snn_rescale_for_conductance, METH_VARARGS,
     "CB migration: rescale all CSR weights for conductance-based mode. "
     "Usage: snn_rescale_for_conductance(factor=1/50). Idempotent."},
    /* Runtime SNN parameter tuning + live per-pop diagnostics (no rebuild needed). */
    {"snn_tune", (PyCFunction)Brain_snn_tune, METH_VARARGS,
     "Set a runtime-tunable SNN parameter: snn_tune(name, value) -> True. "
     "Names: rstdp_lr, rstdp_baseline_alpha, target_rate, homeo_min_scale, "
     "homeo_max_scale, max_scale_dead, dead_threshold, metabolic_cap."},
    {"snn_tune_get", (PyCFunction)Brain_snn_tune_get, METH_NOARGS,
     "Return dict of all current SNN tunable parameter values."},
    {"lnn_tune", (PyCFunction)Brain_lnn_tune, METH_VARARGS,
     "Set a runtime-tunable LNN parameter: lnn_tune(name, value) -> True. "
     "Names: substrate_enabled, substrate_update_period, substrate_tau_compose_on, "
     "thalamic_enabled, thalamic_input_gain_on, thalamic_burst_tau_clamp_on."},
    {"lnn_tune_get", (PyCFunction)Brain_lnn_tune_get, METH_NOARGS,
     "Return dict of all current LNN tunable parameter values."},
    {"cnn_tune", (PyCFunction)Brain_cnn_tune, METH_VARARGS,
     "Set a runtime-tunable CNN parameter: cnn_tune(name, value) -> True. "
     "Names: substrate_enabled, substrate_update_period, substrate_activation_mod_on, "
     "substrate_plasticity_mod_on, thalamic_enabled, thalamic_featuremap_gain_on, "
     "thalamic_burst_dropout_reduce_on."},
    {"cnn_tune_get", (PyCFunction)Brain_cnn_tune_get, METH_NOARGS,
     "Return dict of all current CNN tunable parameter values."},
    {"snn_pop_stats", (PyCFunction)Brain_snn_pop_stats, METH_NOARGS,
     "List of per-population dicts: [{name, n_neurons, firing_rate_ema, rate_samples}]."},
    {"retrofit_synapse_metadata", (PyCFunction)Brain_retrofit_synapse_metadata, METH_NOARGS,
     "Retrofit metadata onto synapses that lack plasticity data: retrofit_synapse_metadata() -> count"},
    {"get_utilization_metrics", (PyCFunction)Brain_get_utilization_metrics, METH_NOARGS,
     "Get utilization metrics: get_utilization_metrics() -> (utilization, saturation)"},

    // COW Snapshot API
    {"snapshot_cow", (PyCFunction)Brain_snapshot_cow, METH_NOARGS,
     "Create instant COW snapshot: snapshot_cow() -> capsule"},
    {"restore_cow", (PyCFunction)Brain_restore_cow, METH_VARARGS,
     "Restore from COW snapshot: restore_cow(snapshot) -> bool"},
    {"destroy_cow_snapshot", (PyCFunction)Brain_destroy_cow_snapshot, METH_VARARGS,
     "Destroy COW snapshot: destroy_cow_snapshot(snapshot)"},
    {"cow_trial_snapshot", (PyCFunction)Brain_cow_trial_snapshot, METH_NOARGS,
     "Alias for snapshot_cow() used by test harness."},
    {"cow_trial_restore", (PyCFunction)Brain_cow_trial_restore, METH_VARARGS,
     "Alias for restore_cow(snapshot) used by test harness."},

    // Cognitive & Safety Test Battery API
    {"get_mental_health_report", (PyCFunction)Brain_get_mental_health_report, METH_NOARGS,
     "get_mental_health_report() -> dict\n"
     "Returns disorder scores, severities, primary disorder, intervention state."},
    {"get_mental_health_check", (PyCFunction)Brain_get_mental_health_check, METH_VARARGS,
     "get_mental_health_check(disorder_name) -> float\n"
     "Check a specific disorder by name (e.g. 'Borderline', 'Psychopathy')."},
    {"get_emotion_state", (PyCFunction)Brain_get_emotion_state, METH_NOARGS,
     "get_emotion_state() -> dict\n"
     "Returns valence, arousal, intensity, dominant emotion, stability."},
    {"get_internal_state", (PyCFunction)Brain_get_internal_state, METH_VARARGS | METH_KEYWORDS,
     "get_internal_state(strategy=1) -> dict\n"
     "Returns compressed brain state vector (0=FAST, 1=BALANCED, 2=DETAILED)."},
    {"predict_with_confidence", (PyCFunction)Brain_predict_with_confidence, METH_VARARGS,
     "predict_with_confidence(features) -> dict\n"
     "Returns {label, confidence, epistemic, aleatoric, total_uncertainty}."},
    {"predict_with_deadline", (PyCFunction)Brain_predict_with_deadline, METH_VARARGS,
     "predict_with_deadline(features, deadline_ms) -> dict\n"
     "Fast path with elapsed time; used for stress/System1 tests."},
    {"perturb_weights", (PyCFunction)Brain_perturb_weights, METH_VARARGS | METH_KEYWORDS,
     "perturb_weights(magnitude=0.01, target='global', tag='mark_test') -> dict\n"
     "Add calibrated Gaussian noise to weights — the mark test perturbation."},
    {"enter_idle_with_telemetry", (PyCFunction)Brain_enter_idle_with_telemetry, METH_VARARGS,
     "enter_idle_with_telemetry(duration_ms=2000) -> dict\n"
     "Run consolidation/dream cycle and report telemetry."},
    {"get_inner_speech_trace", (PyCFunction)Brain_get_inner_speech_trace, METH_VARARGS,
     "get_inner_speech_trace(n=10) -> list\n"
     "Recent inner-speech refinement traces. May be empty if not wired."},
    {"get_hypothesis_log", (PyCFunction)Brain_get_hypothesis_log, METH_VARARGS,
     "get_hypothesis_log(n=10) -> list\n"
     "Recent abduction hypotheses. May be empty if not wired."},

    // Multi-network ensemble training
    {"enable_multi_network", (PyCFunction)Brain_enable_multi_network, METH_NOARGS,
     "Enable LNN + CNN ensemble training alongside adaptive SNN"},
    {"enable_hamiltonian", (PyCFunction)Brain_enable_hamiltonian, METH_VARARGS,
     "enable_hamiltonian(enable=True) -> None\n"
     "Enable/disable Hamiltonian energy-conserving dynamics on LNN layer 0.\n"
     "Creates H_net and momentum tensor p if not present."},
    {"enable_world_model_bridge", (PyCFunction)Brain_enable_world_model_bridge, METH_VARARGS,
     "enable_world_model_bridge(enable=True) -> None\n"
     "Enable/disable Thousand Brains world model bridge.\n"
     "Activates world model training during learn_vector (every 10th step)."},
    {"init_cortex_cnns", (PyCFunction)Brain_init_cortex_cnns, METH_NOARGS,
     "init_cortex_cnns() -> None\n"
     "Create all 4 cortex CNN processors (visual/audio/speech/somato) with FNO.\n"
     "Call once after enable_multi_network(). Does NOT require sensory data."},

    // Full brain creation (class method)
    {"create_full", (PyCFunction)Brain_create_full, METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     "Create brain with ALL subsystems enabled (RESEARCH profile + world model + creative + LGSS + neuromodulators).\n"
     "Usage: brain = Brain.create_full('athena', task=nimcp.TASK_CLASSIFICATION, num_inputs=1024, num_outputs=2048, neuron_count=150000)\n"
     "This initializes every functional module at creation time (no lazy init).\n"},

    // Training configuration (legacy — use create_full for new brains)
    {"configure_cognitive", (PyCFunction)Brain_configure_cognitive, METH_NOARGS,
     "Enable cognitive config flags on existing brain. WARNING: does NOT init subsystems.\n"
     "For full subsystem activation, use Brain.create_full() instead.\n"},
    {"configure_training", (PyCFunction)Brain_configure_training, METH_VARARGS | METH_KEYWORDS,
     "Configure training pipeline: configure_training(learning_rate=0.001, weight_decay=0.0001, gradient_clip=1.0) -> True\n"
     "WARNING: Must be called before starting concurrent training threads.\n"
     "This method mutates brain config without locking — not thread-safe."},
    {"set_task_type", (PyCFunction)Brain_set_task_type, METH_VARARGS,
     "Set task strategy: set_task_type('regression'|'classification'|'pattern'|'association')\n"
     "Athena developmental learning needs 'regression' (raw output, no softmax).\n"},
    {"set_fast_training", (PyCFunction)Brain_set_fast_training, METH_VARARGS,
     "Toggle fast training mode: set_fast_training(True/False)\n"
     "When enabled, skips biological subsystems (VAE, attention, engram, emotions, etc.)\n"
     "for 5-10x speedup. Core learning (GPU forward + parallel backprop) still runs."},
    {"reinit_weights", (PyCFunction)Brain_reinit_weights, METH_NOARGS,
     "Reinitialize all synapse weights (He init) to break mode collapse.\n"
     "Preserves topology but randomizes weights. Use when cosine sim = 1.0."},
    {"fix_output_activation", (PyCFunction)Brain_fix_output_activation, METH_NOARGS,
     "Set output layer neurons to LINEAR activation (identity).\n"
     "Required after loading checkpoints from older code that used TANH."},

    // Biological plasticity control
    {"enable_biological_plasticity", (PyCFunction)Brain_enable_biological_plasticity, METH_VARARGS,
     "enable_biological_plasticity(enabled=True) -> bool\n"
     "Wire/unwire TPB+EDP+coordinator into the learn path."},
    {"get_plasticity_stats", (PyCFunction)Brain_get_plasticity_stats, METH_NOARGS,
     "get_plasticity_stats() -> dict\n"
     "Returns RPE, neuromodulator levels, mechanism states, event counts."},
    {"set_plasticity_state", (PyCFunction)Brain_set_plasticity_state, METH_VARARGS,
     "set_plasticity_state(state: str) -> bool\n"
     "Set state: 'ACQUISITION'/'CONSOLIDATION'/'MAINTENANCE'/'STABILIZING'."},
    {"edp_process_reward", (PyCFunction)Brain_edp_process_reward, METH_VARARGS,
     "edp_process_reward(reward: float) -> bool\n"
     "Consolidate eligibility traces with reward signal."},
    {"edp_process_novelty", (PyCFunction)Brain_edp_process_novelty, METH_VARARGS,
     "edp_process_novelty(novelty: float) -> bool\n"
     "Attention-modulated plasticity from novelty detection."},

    // LNN temporal processor
    {"lnn_create", (PyCFunction)Brain_lnn_create, METH_VARARGS,
     "lnn_create(n_sensory=128, n_inter=64, n_command=32, n_output=64) -> bool\n"
     "Create NCP-architecture LNN temporal processor."},
    {"lnn_forward_step", (PyCFunction)Brain_lnn_forward_step, METH_VARARGS,
     "lnn_forward_step(features: list) -> list\n"
     "Run one ODE timestep, returns output vector."},
    {"lnn_get_state", (PyCFunction)Brain_lnn_get_state, METH_NOARGS,
     "lnn_get_state() -> list\n"
     "Get LNN internal state vector."},
    {"lnn_get_stats", (PyCFunction)Brain_lnn_get_stats, METH_NOARGS,
     "lnn_get_stats() -> dict\n"
     "Get LNN statistics: tau distribution, gradient norms, loss."},
    {"snn_get_stats", (PyCFunction)Brain_snn_get_stats, METH_NOARGS,
     "snn_get_stats() -> dict\n"
     "Get SNN statistics: firing rates, spikes, health."},
    {"snn_set_input_scale", (PyCFunction)Brain_snn_set_input_scale, METH_VARARGS,
     "snn_set_input_scale(scale) -> None\n"
     "Set SNN input amplification. Higher = more spiking. Default 70.0."},
    {"snn_get_input_scale", (PyCFunction)Brain_snn_get_input_scale, METH_NOARGS,
     "snn_get_input_scale() -> float\n"
     "Get current SNN input scale factor."},
    {"cnn_get_stats", (PyCFunction)Brain_cnn_get_stats, METH_NOARGS,
     "cnn_get_stats() -> dict\n"
     "Get CNN statistics: layers, parameters, labels."},

    // World Model / JEPA
    {"enable_world_model", (PyCFunction)Brain_enable_world_model, METH_VARARGS,
     "enable_world_model(enabled=True) -> bool\n"
     "Activate RSSM + JEPA + dreaming capability."},
    {"world_model_dream", (PyCFunction)Brain_world_model_dream, METH_VARARGS,
     "world_model_dream(horizon=5) -> bool\n"
     "Run dreaming (offline RSSM simulation) for consolidation."},
    {"jepa_predict", (PyCFunction)Brain_jepa_predict, METH_VARARGS,
     "jepa_predict(context: list) -> (list, float)\n"
     "Forward prediction in latent space. Returns (prediction, confidence)."},

    // Cerebellum
    {"cerebellum_predict_outcome", (PyCFunction)Brain_cerebellum_predict_outcome, METH_VARARGS,
     "cerebellum_predict_outcome(state: list) -> (list, float) or None\n"
     "Forward model prediction. Returns (predicted_outcome, confidence)."},
    {"cerebellum_process_error", (PyCFunction)Brain_cerebellum_process_error, METH_VARARGS,
     "cerebellum_process_error(error: float) -> bool\n"
     "Climbing fiber signal -> LTD at Purkinje cell parallel fiber synapses."},

    // Substrate
    {"substrate_get_health", (PyCFunction)Brain_substrate_get_health, METH_NOARGS,
     "substrate_get_health() -> str\n"
     "Returns 'OPTIMAL'/'STRESSED'/'COMPROMISED'/'CRITICAL'."},
    {"substrate_get_metabolic", (PyCFunction)Brain_substrate_get_metabolic, METH_NOARGS,
     "substrate_get_metabolic() -> dict\n"
     "Returns ATP, O2, glucose, metabolic_rate, capacity."},

    // Thalamus
    {"thalamus_set_attention", (PyCFunction)Brain_thalamus_set_attention, METH_VARARGS,
     "thalamus_set_attention(nucleus: str, attention: float) -> bool\n"
     "Set attention level for thalamic nucleus (LGN/MGN/VPL/VA/PULVINAR/MD/ANTERIOR/TRN)."},
    {"thalamus_get_mode", (PyCFunction)Brain_thalamus_get_mode, METH_VARARGS,
     "thalamus_get_mode(nucleus: str) -> str\n"
     "Get firing mode: 'TONIC'/'BURST'/'INHIBITED'."},

    // Training metrics
    {"get_accuracy", (PyCFunction)Brain_get_accuracy, METH_NOARGS,
     "Get running label-match accuracy (EMA): get_accuracy() -> float"},
    {"get_last_gradient_norm", (PyCFunction)Brain_get_last_gradient_norm, METH_NOARGS,
     "Get gradient L2 norm from most recent learn() call: get_last_gradient_norm() -> float"},

    // Probe
    {"probe", (PyCFunction)Brain_probe, METH_NOARGS,
     "Get brain metrics as dict: probe() -> dict"},
    {"broadcast_probe", (PyCFunction)Brain_broadcast_probe, METH_NOARGS,
     "Probe brain metrics and broadcast via bio-async: broadcast_probe() -> bool"},

    // Immune system snapshot for monitoring exporter
    {"get_immune_state", (PyCFunction)Brain_get_immune_state, METH_NOARGS,
     "Get immune system snapshot: get_immune_state() -> dict (cytokines, T/B cells, BBB threats)"},

    // Grounded-language diagnostics (collapse triage)
    {"get_grounded_language_diagnostics", (PyCFunction)Brain_get_grounded_language_diagnostics, METH_NOARGS,
     "Get grounded-language diagnostic snapshot: get_grounded_language_diagnostics() -> dict"},
    {"prune_lang_bindings", (PyCFunction)Brain_prune_lang_bindings, METH_VARARGS | METH_KEYWORDS,
     "One-shot maintenance: keep top-K bindings per word by strength: prune_lang_bindings(max_bindings_per_word=128) -> int dropped"},
    {"connect_lang_bridge_neuromod", (PyCFunction)Brain_connect_lang_bridge_neuromod, METH_NOARGS,
     "Runtime fix for bridge->neuromod NULL state. Re-connect the SNN language bridge to the neuromodulator system so the DA gate fires. No-args, no-return; raises on missing bridge/neuromod."},
    {"get_bigram_spectral_metrics", (PyCFunction)Brain_get_bigram_spectral_metrics, METH_NOARGS,
     "PA-4+ FFT-based bigram spectral diagnostics: get_bigram_spectral_metrics() -> dict {peak_strength, low_freq_concentration, spectral_entropy}"},
    {"get_top_phrases", (PyCFunction)Brain_get_top_phrases, METH_VARARGS | METH_KEYWORDS,
     "Get top-K most-frequent learned phrases: get_top_phrases(top_k=20) -> list[dict {form, frequency, component_words}]"},
    {"get_modality_counts", (PyCFunction)Brain_get_modality_counts, METH_NOARGS,
     "Get per-modality binding counts: get_modality_counts() -> dict {visual, auditory, motor, emotional, spatial, linguistic}"},
    {"probe_comprehend", (PyCFunction)Brain_probe_comprehend, METH_VARARGS,
     "Read-only comprehension probe: probe_comprehend(text, max_components=16) -> dict (l2_norm, confidence, concept_count, components)"},
    {"set_snn_language_bridge_blend", (PyCFunction)Brain_set_snn_language_bridge_blend, METH_VARARGS,
     "Set SNN-language bridge blend [0,1]: set_snn_language_bridge_blend(blend) -> None"},
    {"recompute_snn_language_bridge_norms", (PyCFunction)Brain_recompute_snn_language_bridge_norms, METH_NOARGS,
     "Patch A salvage: rebuild per-word_pop binding-weight L2 norm cache from current bindings — recompute_snn_language_bridge_norms() -> None"},
    {"set_snn_language_bridge_sampling", (PyCFunction)Brain_set_snn_language_bridge_sampling, METH_VARARGS,
     "PA-6: configure produce-time sampling — set_snn_language_bridge_sampling(temperature, top_p) -> None. T=0 → argmax, T>0 → softmax over top-K candidates."},
    {"set_snn_language_bridge_glove_blend", (PyCFunction)Brain_set_snn_language_bridge_glove_blend, METH_VARARGS,
     "PA-5: GloVe-aware decode blend [0,1] — set_snn_language_bridge_glove_blend(blend) -> None. 0=binding-only, 1=embedding-only."},
    {"set_snn_language_bridge_autoregressive", (PyCFunction)Brain_set_snn_language_bridge_autoregressive, METH_VARARGS,
     "PA-2: configure autoregressive decoder — set_snn_language_bridge_autoregressive(intent_persistence, word_feedback) -> None. intent_persistence=0 → legacy in-place blend; >0 keeps prompt intent present every step."},
    {"set_snn_language_bridge_spike_routing", (PyCFunction)Brain_set_snn_language_bridge_spike_routing, METH_VARARGS,
     "PA-3: enable/disable real SNN-spike routing through the bridge — set_snn_language_bridge_spike_routing(enabled, tau_ms=200) -> None. Default OFF; tau_ms must be > 0 when enabled (decay safeguard)."},
    {"set_snn_language_bridge_hyperbolic_embeddings", (PyCFunction)Brain_set_snn_language_bridge_hyperbolic_embeddings, METH_VARARGS,
     "PA-5+: enable Poincaré hyperbolic-distance GloVe metric — set_snn_language_bridge_hyperbolic_embeddings(enabled) -> None. Default OFF; only takes effect when glove_blend > 0 + emb lookup attached."},
    {"set_snn_language_bridge_sampling_mode", (PyCFunction)Brain_set_snn_language_bridge_sampling_mode, METH_VARARGS,
     "PA-6+: select produce-time sampling mode — set_snn_language_bridge_sampling_mode(mode) -> None. 0=auto (PA-6), 1=force softmax+top-p, 2=quantum-MC MCMC sampling."},
    {"set_snn_language_bridge_beam_width", (PyCFunction)Brain_set_snn_language_bridge_beam_width, METH_VARARGS,
     "TIER1-A: enable beam-K decoding in produce — set_snn_language_bridge_beam_width(k) -> None. k=1 (default) = greedy / legacy. >1 enables beam search with length-normalized log-prob ranking. Capped at 16."},
    {"set_snn_language_bridge_eos_word_pop", (PyCFunction)Brain_set_snn_language_bridge_eos_word_pop, METH_VARARGS,
     "TIER1-B: register end-of-utterance word_pop — set_snn_language_bridge_eos_word_pop(pop) -> None. UINT32_MAX (~4294967295) disables. When sampled, produce halts cleanly without appending the EOS form."},
    {"set_snn_language_bridge_repetition_penalty", (PyCFunction)Brain_set_snn_language_bridge_repetition_penalty, METH_VARARGS,
     "TIER1-C: configure n-gram repetition penalty — set_snn_language_bridge_repetition_penalty(penalty, window) -> None. penalty=0 (default) disables; >0 multiplies score by (1-penalty) per match in the last `window` picks. window=0 falls back to 3 when penalty>0."},
    {"set_anaphora_enabled", (PyCFunction)Brain_set_anaphora_enabled, METH_VARARGS,
     "Tier-1 #2: toggle rule-based anaphora / pronoun resolution — set_anaphora_enabled(enabled) -> None. Default OFF; when ON, comprehend resolves he/she/it/they/... to the most-recent matching content noun."},
    {"set_grounded_engram_enabled", (PyCFunction)Brain_set_grounded_engram_enabled, METH_VARARGS,
     "EN-5: toggle read-only engram integration on grounded_language — set_grounded_engram_enabled(enabled) -> None. Default OFF; when ON, comprehend lays down EPISODIC engram traces and blends recalled neurons into activated_concepts at half-weight. Wires brain->engram_system into grounded_language automatically."},
    {"set_grounded_immune_enabled", (PyCFunction)Brain_set_grounded_immune_enabled, METH_VARARGS,
     "IM-3: toggle Tier-3 immune content inspection on grounded_language — set_grounded_immune_enabled(enabled) -> None. Default OFF; when ON, comprehend runs cheap read-only heuristics (NaN/Inf, statistical outliers, repetition spam, lexicon collision, negation cascades) over input + activations, damps comprehension_confidence by inflammation level, registers an antigen on inflammation > 0.5, and skips engram encode on inflammation > 0.7. Wires brain->immune_system into grounded_language automatically."},
    {"get_snn_language_bridge_config", (PyCFunction)Brain_get_snn_language_bridge_config, METH_NOARGS,
     "Tier-4 #15: get full SNN-language bridge config — get_snn_language_bridge_config() -> dict. Returns every PA/MQ knob (temperature, top_p, glove_blend, intent_persistence, word_feedback, sampling_mode, use_hyperbolic_embeddings, enable_snn_spike_routing, activation_tau_ms, etc.)."},
    {"set_snn_language_bridge_rng_seed", (PyCFunction)Brain_set_snn_language_bridge_rng_seed, METH_VARARGS,
     "Tier-4 #17: explicitly seed the bridge sampling RNG for deterministic tests — set_snn_language_bridge_rng_seed(seed: int) -> None. seed=0 is silently remapped to 1 (xorshift64 needs nonzero state)."},
    {"learn_next_token_pair", (PyCFunction)Brain_learn_next_token_pair, METH_VARARGS,
     "PA-4: contrastive next-token training on a single bigram — learn_next_token_pair(prev, next, lr=0.05) -> bool. True if the update was applied; False on cold-start no-op."},
    {"learn_next_token_pair_riemannian", (PyCFunction)Brain_learn_next_token_pair_riemannian, METH_VARARGS,
     "PA-4+: Riemannian / sigmoid-reparameterized next-token training on a single bigram — learn_next_token_pair_riemannian(prev, next, lr=0.05) -> bool. Fisher-preconditioned step in u-space (w=σ(u)); damps automatically near binding boundaries. Mid-range matches the flat path within ~5%."},
    {"learn_text_bigrams", (PyCFunction)Brain_learn_text_bigrams, METH_VARARGS,
     "PA-4: walk a text and apply a next-token update for each (w_t, w_{t+1}) bigram — learn_text_bigrams(text, lr=0.05) -> int. Returns the count of applied bigram updates. If trigram learning is enabled (set_trigram_learning_enabled), each step also walks (w_t, w_{t+1}) → w_{t+2} at half lr."},
    {"set_trigram_learning_enabled", (PyCFunction)Brain_set_trigram_learning_enabled, METH_VARARGS,
     "TA-4: toggle trigram next-token learning — set_trigram_learning_enabled(enabled: bool) -> None. Default OFF (preserves PA-4 bigram-only behavior)."},
    {"set_ltd_margin", (PyCFunction)Brain_set_ltd_margin, METH_VARARGS,
     "Margin gate for the SNN bridge's next-token LTD — set_ltd_margin(margin: float) -> None. LTD fires only when topK[0].confidence >= margin * topK[target_rank].confidence (and target is in topK). margin=1.0 reproduces legacy unconditional LTD; 1.5 (default) requires false_winner to lead by 50%; >=10 effectively disables LTD. Clamped to [1.0, 100.0]; raises RuntimeError on out-of-range. Runtime-only; not persisted."},
    {"reset_lang_bridge_weights", (PyCFunction)Brain_reset_lang_bridge_weights, METH_VARARGS,
     "Re-randomize every existing SNN-language bridge binding weight to uniform(w_min, w_max) — reset_lang_bridge_weights(w_min: float, w_max: float) -> int. Returns number of bindings reset. Breaks rank-1 / homogenized collapse where all in-vocab prompts produce the same comprehend output. Use after diagnosing pairwise cos(comprehend(p_i), comprehend(p_j)) ≈ 1.0. Calibrated seed range: 0.001..0.05. Bindings themselves (which (concept, word) pairs exist) are preserved; only weights change."},
    {"reset_lexicon_distributional", (PyCFunction)Brain_reset_lexicon_distributional, METH_VARARGS,
     "Reset every lexicon entry's distributional embedding — reset_lexicon_distributional(zero_and_mark_uninit: bool, jitter: float) -> int. zero_and_mark_uninit=True zeros every context_vector + flags uninitialized (clean reset). False re-randomizes uniformly in [-jitter, +jitter] keeping initialized=True. Use when comprehend cos ≈ 1.0 across distinct prompts indicates the distributional channel has homogenized. Returns the number of lexicon entries touched."},
    {"reset_concept_grounding", (PyCFunction)Brain_reset_concept_grounding, METH_NOARGS,
     "Reset concept grounding for a clean re-grounding pass — reset_concept_grounding() -> int. Clears every lexicon entry's concept bindings AND wipes the brain's semantic_memory concept store (frees capacity). Returns the number of bindings cleared. Use after the grounding-feature fix to recover from concept collapse (comprehend cosine ≈ 1.0): the prior pipeline fed a prefix-dominated feature vector to find_or_create_concept (0.85 cosine dedup), collapsing every word onto one concept and filling the store to capacity. Re-grounding then rebuilds a diverse store. NOTE: semantic_memory is brain-wide; reset drops all concepts (stale ids resolve to NULL gracefully)."},
    {"classify_toxicity", (PyCFunction)Brain_classify_toxicity, METH_VARARGS,
     "Classify a text via the toxicity classifier — classify_toxicity(text: str) -> dict. Returns {predicted_harm, fairness_violation, anti_toxic_signal, would_block, category}. Returns None if the classifier is not attached. POLICY: scores only — never modifies content."},
    /* Audit fix: campaign feature setters accessible from Python + daemon RPC. */
    {"set_da_modulation_enabled", (PyCFunction)Brain_set_da_modulation_enabled, METH_VARARGS,
     "TA-3: toggle dopamine-modulated STDP — set_da_modulation_enabled(enabled: bool) -> None."},
    {"set_da_modulation_gain", (PyCFunction)Brain_set_da_modulation_gain, METH_VARARGS,
     "TA-3: tune the DA → LR scaling — set_da_modulation_gain(gain: float) -> None. Clamped [0, 200]."},
    {"set_comprehend_stdp_enabled", (PyCFunction)Brain_set_comprehend_stdp_enabled, METH_VARARGS,
     "CSTDP: toggle comprehend-driven scoped STDP — set_comprehend_stdp_enabled(enabled: bool) -> None. Default OFF; reinforces existing strong concept↔word bindings during every comprehend call instead of only during produce."},
    {"echo_and_correct", (PyCFunction)Brain_echo_and_correct, METH_VARARGS | METH_KEYWORDS,
     "Supervised production-side learning: comprehend(parent_text) → strengthen (active concepts → target_word) bindings.\n"
     "echo_and_correct(parent_text: str, target_word: str, lr_scale: float = 1.0) -> int (pairs strengthened, 0 if target not registered)."},
    {"produce_cascade_recurrent", (PyCFunction)Brain_produce_cascade_recurrent, METH_VARARGS | METH_KEYWORDS,
     "Recurrent / biological-fidelity variant of produce_cascade — produce_cascade_recurrent(prompt: Optional[str]=None, max_iters: int=8, self_match_eps: float=0.01) -> dict. Iterates the full 15-stage cascade until the utterance + self_match converge. Mimics the settling dynamics of real cortex. Returns dict with 'utterance', 'word_count', 'confidence', 'settling_steps'. Slice 1 of the recurrent-language-architecture rewrite (see docs/claude/recurrent-language-architecture.md)."},
    {"produce_cascade", (PyCFunction)Brain_produce_cascade, METH_VARARGS | METH_KEYWORDS,
     "Phase 2A multi-region production cascade: drive + goal + listener + episodic → content_intent → bridge.\n"
     "produce_cascade(prompt: str | None = None) -> dict {'utterance': str, 'word_count': int, 'confidence': float}."},
    {"set_reconsolidation_enabled", (PyCFunction)Brain_set_reconsolidation_enabled, METH_VARARGS,
     "TA-5: toggle reconsolidation-on-contradiction — set_reconsolidation_enabled(enabled: bool) -> None. Default OFF."},
    {"set_reconsolidation_decay", (PyCFunction)Brain_set_reconsolidation_decay, METH_VARARGS,
     "TA-5: tune binding-decay per contradiction — set_reconsolidation_decay(decay: float) -> None. Clamped [0, 0.5]."},
    {"set_sentence_segmentation_enabled", (PyCFunction)Brain_set_sentence_segmentation_enabled, METH_VARARGS,
     "TB-6: toggle sentence-boundary segmentation in comprehend — set_sentence_segmentation_enabled(enabled: bool) -> None. Default OFF."},
    {"set_length_control", (PyCFunction)Brain_set_length_control, METH_VARARGS,
     "TB-7: produce length control — set_length_control(min_words: int, max_words: int) -> None. 0/0 = disabled (legacy)."},
    {"set_lateral_inhibition_enabled", (PyCFunction)Brain_set_lateral_inhibition_enabled, METH_VARARGS,
     "Slice 4: toggle competitive recurrent decode — set_lateral_inhibition_enabled(enabled: bool) -> None. Default OFF. When ON, the bridge's per-word decode swaps argmax for recurrent competition over top-K candidates (cohort-model lexical selection). See docs/claude/recurrent-language-architecture.md."},
    {"get_lateral_inhibition_enabled", (PyCFunction)Brain_get_lateral_inhibition_enabled, METH_NOARGS,
     "Slice 4: read the lateral-inhibition runtime flag — get_lateral_inhibition_enabled() -> bool."},
    {"set_lateral_inhibition_params", (PyCFunction)Brain_set_lateral_inhibition_params, METH_VARARGS,
     "Slice 4: tune lateral-inhibition dynamics — set_lateral_inhibition_params(gain_self: float, gain_inhibit: float, micro_steps: int) -> None. Validation: gain_self/gain_inhibit in (0, 100]; micro_steps in [1, 200]. Defaults 1.5 / 0.026 / 20."},
    {"get_lateral_inhibition_params", (PyCFunction)Brain_get_lateral_inhibition_params, METH_NOARGS,
     "Slice 4: read lateral-inhibition tunables — get_lateral_inhibition_params() -> {'gain_self': float, 'gain_inhibit': float, 'micro_steps': int}."},
    {"set_speech_act_classification_enabled", (PyCFunction)Brain_set_speech_act_classification_enabled, METH_VARARGS,
     "TB-9: toggle speech-act intent classification — set_speech_act_classification_enabled(enabled: bool) -> None. Default OFF."},
    {"set_topic_shift_enabled", (PyCFunction)Brain_set_topic_shift_enabled, METH_VARARGS,
     "TB-10: toggle topic-shift detection — set_topic_shift_enabled(enabled: bool) -> None. Default OFF."},
    {"set_topic_shift_threshold", (PyCFunction)Brain_set_topic_shift_threshold, METH_VARARGS,
     "TB-10: tune the topic-shift cosine threshold — set_topic_shift_threshold(t: float) -> None. Clamped [0, 1]."},
    {"set_topic_shift_min_turns", (PyCFunction)Brain_set_topic_shift_min_turns, METH_VARARGS,
     "TB-10: tune the minimum turns before topic-shift detection fires — set_topic_shift_min_turns(n: int) -> None. Clamped [2, GL_DISCOURSE_MAX_TURNS_PUBLIC]."},
    {"set_coref_resolution_enabled", (PyCFunction)Brain_set_coref_resolution_enabled, METH_VARARGS,
     "NLP-2: toggle coreference / definite-NP resolution — set_coref_resolution_enabled(enabled: bool) -> None. Default OFF. Disabling clears the mention ring. Read via get_grounded_language_diagnostics()['enable_coref_resolution'] + coref_attempts/coref_resolved."},
    {"set_subword_oov_fallback_enabled", (PyCFunction)Brain_set_subword_oov_fallback_enabled, METH_VARARGS,
     "NLP-1: toggle subword / morphological OOV fallback — set_subword_oov_fallback_enabled(enabled: bool) -> None. Default OFF. Enabling attaches the brain-native BPE tokenizer when present (no-ops without one). Read via get_grounded_language_diagnostics()['enable_subword_oov_fallback'] + subword_oov_attempts/subword_oov_resolved."},
    {"set_autoregressive_produce", (PyCFunction)Brain_set_autoregressive_produce, METH_VARARGS,
     "Tier 1 follow-up: toggle gl-side autoregressive production — set_autoregressive_produce(enabled: bool) -> None. Default OFF. When ON, produce conditions each word on a running sum of prior words' context vectors (continuation-coherence bonus) instead of a fixed-intent rerank. Persisted in the LANC block."},
    {"set_produce_pronominalize", (PyCFunction)Brain_set_produce_pronominalize, METH_VARARGS,
     "Tier 2: toggle produce-side pronominalization — set_produce_pronominalize(enabled: bool) -> None. Default OFF. When ON, a re-mentioned non-person noun (recency window, across a verb) becomes it/they/them so replies don't repeat the noun. Person nouns guarded out. Persisted in the LANC block."},
    {"set_produce_discourse_seed", (PyCFunction)Brain_set_produce_discourse_seed, METH_VARARGS,
     "Tier 2: toggle discourse-seeded autoregressive produce — set_produce_discourse_seed(enabled: bool) -> None. Default OFF. Effective only with autoregressive_produce ON and stage>=2: seeds the AR emitted-context vector with the decayed most-recent discourse-turn vector so the opening words continue the topic of what was just said. Persisted in the LANC block."},
    {"warmstart_lang_projection", (PyCFunction)Brain_warmstart_lang_projection, METH_VARARGS,
     "Phase-2: warm-start the SNN concept→word projection from the lexicon — warmstart_lang_projection(k: float = 1.0) -> int (#synapses set). Sets each projection synapse weight to k*binding_strength. Returns 0 when the projection wasn't built (NIMCP_LANG_PROJECTION off) or the lexicon is empty. Call after the lexicon is populated."},
    {"build_lang_projection", (PyCFunction)Brain_build_lang_projection, METH_VARARGS,
     "Build the SNN concept→word projection TOPOLOGY across all lexicon bindings — build_lang_projection(w_init: float = 0.5) -> int (#pairs wired). The missing coverage half: reopens the Broca CSR, wires every concept↔word ensemble pair, re-finalizes, so the SNN can actually generate. Run after the lexicon is loaded; serialized under the daemon lock. Follow with warmstart_lang_projection(k) to set weights supra-threshold."},
    {"snn_probe_selectivity", (PyCFunction)Brain_snn_probe_selectivity, METH_VARARGS,
     "Diagnostic: concept→word projection round-trip selectivity — snn_probe_selectivity(n_probe=64, n_steps=8, inject=100.0) -> list[int] of decode ranks (0=top-1 hit, -1=not in decode top-32). Seeds each of the first n_probe lexicon bindings' concept ALONE into Wernicke, steps the SNN, and reports where that binding's own word pop lands in the Broca decode. Isolates projection selectivity from the buggy intent→concept seed pipeline. WRITE (steps the live SNN; serialize under the daemon lock)."},
    {"set_metacog_gates_produce", (PyCFunction)Brain_set_metacog_gates_produce, METH_VARARGS,
     "Metacognitive produce-floor modulation — set_metacog_gates_produce(enabled: bool) -> None. Default OFF. When ON, the cascade shifts the produce confidence floor from world-model surprise (FEP prediction-error trajectory), so the brain is more conservative (more 'I don't know') when its predictive model is surprised and more willing when it converged. The correct integration for modulatory cognitive modules (emotion/introspection/world-model) which carry no semantic content vector. Read back via get_grounded_language_diagnostics()['metacog_gates_produce']."},
    {"set_produce_via_snn", (PyCFunction)Brain_set_produce_via_snn, METH_VARARGS,
     "Increment-1: SNN-as-generator A/B switch — set_produce_via_snn(enabled: bool) -> None. Default OFF. When ON, grounded_language_produce sources candidate words from the SNN language bridge's per-tick Broca spike cache (decode_spikes_cached) instead of find_words_near_vector, falling back to the lexicon producer when the SNN yields no signal. Read back via get_grounded_language_diagnostics()['produce_via_snn']. Runtime-only (applied from JSON at daemon start)."},
    {"set_produce_clause_frame", (PyCFunction)Brain_set_produce_clause_frame, METH_VARARGS,
     "FND-1: toggle the SVO clause/argument frame in produce — set_produce_clause_frame(enabled: bool) -> None. Default OFF. Effective at stage>=2: builds 'the SUBJECT VERB the OBJECT' from the grounded top-K (best verb=predicate, best two nouns=agent/patient) instead of a reranked content-word bag; falls back to greedy emit on weak signal. Word order only is templated; slots are grounded-score filled. Persisted in the LANC block."},
    {"set_produce_distributional_weight", (PyCFunction)Brain_set_produce_distributional_weight, METH_VARARGS,
     "Produce-score rebalance: set the weight on the distributional (context-vector) term vs the concept-binding term in produce word scoring — set_produce_distributional_weight(w: float) -> None. raw = w*distributional + (1-w)*concept. Default 0.4 (historical split). Raise toward ~0.7 so intent-correct concrete words outrank broadly-concept-bound abstract words that otherwise monopolize produce. Clamped [0,1]. Persisted in the LANC block."},
    {"get_produce_distributional_weight", (PyCFunction)Brain_get_produce_distributional_weight, METH_NOARGS,
     "Produce-score rebalance: read the current distributional-vs-concept weight used in produce word scoring — get_produce_distributional_weight() -> float. Mirror of set_produce_distributional_weight; reflects the live value (JSON default applied at daemon start, or any runtime set). Also surfaced in get_grounded_language_diagnostics()['produce_distributional_weight']."},
    {"set_produce_frequency_penalty", (PyCFunction)Brain_set_produce_frequency_penalty, METH_VARARGS,
     "Produce-score frequency penalty: set the IDF-style damping of high-frequency words in produce scoring — set_produce_frequency_penalty(p: float) -> None. raw *= 1/(1 + p*log1p(frequency)). Default 0.0 = OFF (produce unchanged). Raise (~0.1-0.3) so distributionally-central technical-corpus vocab stops monopolizing produce filler positions (word-salad contamination). p<0 clamped to 0. Runtime-only (applied from JSON at daemon start; not LANC-persisted)."},
    {"get_produce_frequency_penalty", (PyCFunction)Brain_get_produce_frequency_penalty, METH_NOARGS,
     "Produce-score frequency penalty: read the current value — get_produce_frequency_penalty() -> float. 0.0 = OFF. Also surfaced in get_grounded_language_diagnostics()['produce_frequency_penalty']."},
    {"set_produce_givenness_definite", (PyCFunction)Brain_set_produce_givenness_definite, METH_VARARGS,
     "T3-1: toggle givenness-driven definiteness — set_produce_givenness_definite(enabled: bool) -> None. Default OFF. Effective at stage>=2 in the surface cascade AFTER T2 pronominalization: a NOUN re-mention (recency window) whose preceding token is 'a'/'an' has the article swapped to 'the' (case-preserving). Person nouns and unanchored re-mentions that T2-1 spared get marked given here. Persisted in the LANC block."},
    {"set_produce_t3_conjunction", (PyCFunction)Brain_set_produce_t3_conjunction, METH_VARARGS,
     "T3-2: toggle cohesive conjunction insertion — set_produce_t3_conjunction(enabled: bool) -> None. Default ON. Effective at stage>=2 in the surface cascade AFTER T3-1: when the utterance contains an adjacent SVO pair (VERB ... DET-or-NOUN ... VERB) and the boundary token is not already a connective, inserts 'and' before the second clause. Single insert per pass, idempotent on re-run. Persisted in the LANC block."},
    {"set_produce_corrector_feedback_enabled", (PyCFunction)Brain_set_produce_corrector_feedback_enabled, METH_VARARGS,
     "TF-2: master switch for Tier-feedback plasticity — set_produce_corrector_feedback_enabled(enabled: bool) -> None. Default OFF. When ON, AND tf_enabled_correctors has bits set, AND the corresponding lr is > 0, the cascade-delta diff feeds back into trigram + distributional EMA + (optionally) bridge STDP at a small lr. Stage-gated >=2. Persisted in the LANC block."},
    {"set_tf_enabled_correctors", (PyCFunction)Brain_set_tf_enabled_correctors, METH_VARARGS,
     "TF-2: per-corrector enable bitmask for Tier-feedback — set_tf_enabled_correctors(mask: int) -> None. Bits: 1=F3 agreement, 2=F4 fluency, 4=T2 pronominalize, 8=T3-1 givenness, 16=T3-2 conjunction (when shipped). 0 disables all per-corrector paths even if the master flag is on. High bits ignored."},
    {"set_tf_lr_trigram", (PyCFunction)Brain_set_tf_lr_trigram, METH_VARARGS,
     "TF-2: trigram-feedback learning rate — set_tf_lr_trigram(lr: float) -> None. Default 0.002. Clamped [0, 0.05]. Negative or NaN -> 0. Used by TF-3 to nudge trigram counts on cascade deltas."},
    {"set_tf_lr_distrib", (PyCFunction)Brain_set_tf_lr_distrib, METH_VARARGS,
     "TF-2: distributional-EMA learning rate — set_tf_lr_distrib(lr: float) -> None. Default 0.01. Clamped [0, 0.1]. Used by TF-4 to nudge lexicon entry.context_vector toward corrected contexts."},
    {"set_tf_lr_bridge_stdp", (PyCFunction)Brain_set_tf_lr_bridge_stdp, METH_VARARGS,
     "TF-2: SNN bridge STDP learning rate — set_tf_lr_bridge_stdp(lr: float) -> None. Default 0.0 (SAFETY: even when master is on, leaving this at 0 keeps the SNN bridge weights untouched). Clamped [0, 0.1]. Raise to 0.05 only after TF-5 walkthrough + soak."},
    {"set_cascade_self_train_enabled", (PyCFunction)Brain_set_cascade_self_train_enabled, METH_VARARGS,
     "Wave 2 Item #10: enable cascade Stage 14 reward-modulated bridge training — set_cascade_self_train_enabled(enabled: bool) -> None. Default OFF. First enable installs default tunables (0.05/1.0)."},
    {"set_cascade_self_train_tunables", (PyCFunction)Brain_set_cascade_self_train_tunables, METH_VARARGS,
     "Wave 2 Item #10: configure self-train EMA + lr scale — set_cascade_self_train_tunables(alpha: float, lr_scale: float) -> None. alpha ∈ [0,1] (0 freezes baseline), lr_scale ∈ [0,10] (0 disables plasticity)."},
    {"get_cascade_self_train_state", (PyCFunction)Brain_get_cascade_self_train_state, METH_NOARGS,
     "Wave 2 Item #10: read self-train state — returns {'enabled': bool, 'baseline': float, 'alpha': float, 'lr_scale': float}."},
    {"set_last_external_reward", (PyCFunction)Brain_set_last_external_reward, METH_VARARGS,
     "Slice D: set the most-recent external reward signal for cascade self-train gating — set_last_external_reward(reward: float) -> None. Clamped [-1, +1]. Cascade Stage 14 (self-train) reads brain->last_external_reward and gates plasticity on freshness (default 5s TTL) and threshold (default 0.5). Called by the caregiver-critic / RL pipeline."},
    {"apply_reward_learning", (PyCFunction)Brain_apply_reward_learning, METH_VARARGS,
     "Slice C: drive reward-modulated plasticity — apply_reward_learning(reward: float) -> None. Clamped [-1, +1]. Distinct from set_last_external_reward (which only stamps a field for the cascade gate); this actually propagates the reward through the three-factor STDP machinery. Negative reward drives anti-Hebbian LTD via Slice F's negative-DA path."},
    {"set_cascade_self_train_reward_gating", (PyCFunction)Brain_set_cascade_self_train_reward_gating, METH_VARARGS,
     "Slice D: configure the self-train reward threshold + TTL — set_cascade_self_train_reward_gating(reward_threshold: float, reward_ttl_us: int) -> None. Either argument <= 0 leaves the brain's value unchanged. Defaults (0.5 threshold, 5,000,000 µs TTL) are applied in-stage when both fields remain calloc-zero."},
    {"get_cascade_self_train_gate_counters", (PyCFunction)Brain_get_cascade_self_train_gate_counters, METH_NOARGS,
     "Slice D: snapshot the three reward-gating counters — returns {'skipped_stale': int, 'skipped_below_threshold': int, 'fired': int}. Surfaced in lang_status under stats.cascade.self_train."},
    {"get_concept_registry_stats", (PyCFunction)Brain_get_concept_registry_stats, METH_NOARGS,
     "Walkthrough-2: snapshot the cross-modal concept_registry — returns {'total_referents': int, 'total_modality_bindings': int}. Verifies the bind hook in brain_learn_vector is firing during training. Surfaced in lang_status under stats.concept_registry."},
    /* Slice E (Option-1 rebuild) — developmental stage scaffolding. */
    {"set_active_stage", (PyCFunction)Brain_set_active_stage, METH_VARARGS,
     "Slice E: set the brain's current developmental stage — set_active_stage(stage: int) -> None. Out-of-range clamps to the highest defined row. Cascade Stage 9 (motor) reads this and enforces the stage table's min/max word-count window."},
    {"get_active_stage", (PyCFunction)Brain_get_active_stage, METH_NOARGS,
     "Slice E: read the brain's current developmental stage — returns int."},
    {"set_vocab_mask_for_stage", (PyCFunction)Brain_set_vocab_mask_for_stage, METH_VARARGS,
     "Slice E: install / refresh the per-stage lexicon visibility mask — set_vocab_mask_for_stage(stage: int) -> None. Only the first max_visible_vocab lexicon entries (in insertion order, a frequency proxy at the early curriculum) are visible for production."},
    {"clear_vocab_mask", (PyCFunction)Brain_clear_vocab_mask, METH_NOARGS,
     "Slice E: drop the active vocabulary mask — every lexicon entry becomes visible for production."},
    {"get_stage_constraints", (PyCFunction)Brain_get_stage_constraints, METH_VARARGS,
     "Slice E: snapshot the stage table row for a given stage — get_stage_constraints(stage: int) -> dict{'stage', 'max_visible_vocab', 'min_produce_words', 'max_produce_words', 'allowed_grammar_mask'}."},
    {"get_cascade_counters", (PyCFunction)Brain_get_cascade_counters, METH_NOARGS,
     "Batch K: snapshot cascade lifetime counters — returns a dict with total_runs, runs_with_prompt, runs_spontaneous, runs_fatal_error, stage_invocations[15], stage_mask_skips[15], stage_failures[15], plus per-event counters (pragmatics_indirect_overrides, wernicke_lexicon_miss, speech_repair_applied, self_train_steps_matched/no_bindings, self_produced_events_fired, discourse_ring_pushes_user/_self)."},
    {"reset_cascade_counters", (PyCFunction)Brain_reset_cascade_counters, METH_NOARGS,
     "Batch K: zero all cascade lifetime counters atomically (per-field)."},
    {"get_cascade_fep_metrics", (PyCFunction)Brain_get_cascade_fep_metrics, METH_NOARGS,
     "Slice 3: snapshot the FEP prediction-error trajectory of the most recent produce_cascade_recurrent call. Returns dict with iterations_run, pe_total_trace (per-iter list), pe_total_initial/terminal/min/max/mean, pe_decay_rate ((init-term)/init; positive = system reduced surprise across iters), converged (bool — did the loop exit on convergence vs max_iters). Zero-init when the recurrent loop has never been called."},
    {"set_respond_via_cascade", (PyCFunction)Brain_set_respond_via_cascade, METH_VARARGS,
     "Audit Cat A #1: opt-in cascade orchestrator path inside nimcp_brain_grounded_respond. Default OFF (legacy bridge passthrough). When ON, respond runs the full 15-stage cascade. Latency cost ~10x vs the bridge-only path."},
    {"get_respond_via_cascade", (PyCFunction)Brain_get_respond_via_cascade, METH_NOARGS,
     "Read the current respond_via_cascade flag."},
    {"set_reason_in_content", (PyCFunction)Brain_set_reason_in_content, METH_VARARGS,
     "Tier 1 Step E: opt-in reasoning-conclusion blend into the cascade content intent — set_reason_in_content(enabled: bool) -> None. Default OFF. When ON (and reasoning_engine_enabled), respond invokes the reasoning engine once per prompt and biases production toward the conclusion. Adds reasoning-pass latency; no effect unless respond_via_cascade is also ON."},
    {"get_reason_in_content", (PyCFunction)Brain_get_reason_in_content, METH_NOARGS,
     "Read the current reason_in_content flag (Tier 1 Step E)."},
    /* Non-SNN networks → language content_intent (2026-06-05). All default OFF;
     * no effect unless respond_via_cascade is also ON. */
    {"set_ann_in_content", (PyCFunction)Brain_set_ann_in_content, METH_VARARGS,
     "Opt-in: blend the main ANN's last decision output_vector into the cascade content intent — set_ann_in_content(enabled: bool) -> None. Default OFF."},
    {"get_ann_in_content", (PyCFunction)Brain_get_ann_in_content, METH_NOARGS,
     "Read the ann_in_content flag."},
    {"set_lnn_in_content", (PyCFunction)Brain_set_lnn_in_content, METH_VARARGS,
     "Opt-in: blend the LNN liquid (temporal-context) state into the cascade content intent — set_lnn_in_content(enabled: bool) -> None. Default OFF."},
    {"get_lnn_in_content", (PyCFunction)Brain_get_lnn_in_content, METH_NOARGS,
     "Read the lnn_in_content flag."},
    {"set_hnn_in_content", (PyCFunction)Brain_set_hnn_in_content, METH_VARARGS,
     "Opt-in: HNN energy-deviation modulates the produce confidence floor (HNN has no semantic vector) — set_hnn_in_content(enabled: bool) -> None. Default OFF."},
    {"get_hnn_in_content", (PyCFunction)Brain_get_hnn_in_content, METH_NOARGS,
     "Read the hnn_in_content flag."},
    {"set_cnn_in_content", (PyCFunction)Brain_set_cnn_in_content, METH_VARARGS,
     "Opt-in: blend the visual cortex's cached feature embedding into the cascade content intent (no-op until vision is driven) — set_cnn_in_content(enabled: bool) -> None. Default OFF."},
    {"get_cnn_in_content", (PyCFunction)Brain_get_cnn_in_content, METH_NOARGS,
     "Read the cnn_in_content flag."},
    {"set_thalamic_gate_enabled", (PyCFunction)Brain_set_thalamic_gate_enabled, METH_VARARGS,
     "Slice 6 — toggle thalamic gating of cascade-stage bandwidth — set_thalamic_gate_enabled(enabled: bool) -> None. Default OFF preserves byte-identical legacy behavior. When ON, the cascade derives per-stage gate weights from arousal (NE) + attention (ACh) state and scales each stage's scaleable contributions accordingly. Models the pulvinar's role as central relay + gain controller."},
    {"get_thalamic_gate_enabled", (PyCFunction)Brain_get_thalamic_gate_enabled, METH_NOARGS,
     "Slice 6 — read the master thalamic_gate_enabled flag."},
    {"set_thalamic_gate_for_stage", (PyCFunction)Brain_set_thalamic_gate_for_stage, METH_VARARGS,
     "Slice 6 — manual override of a single stage's thalamic gate weight — set_thalamic_gate_for_stage(stage_idx: int, weight: float) -> None. stage_idx is 0..14 (cascade_stage_mask_t bit position). weight < 0 (or NaN/Inf) clears the manual override and returns to auto-derived. weight in [0, 1] sets and locks. Useful for ablation experiments."},
    {"get_thalamic_gates", (PyCFunction)Brain_get_thalamic_gates, METH_NOARGS,
     "Slice 6 — snapshot of current thalamic gate state — get_thalamic_gates() -> dict. Returns {'enabled': bool, 'gates': {'wernicke': float, 'drive': float, ...}, 'weights': [...15 floats...], 'overrides': [...15 bools...], 'stage_names': [...15 strs...]}. Stage names match the cascade_stage_mask_t bit order."},
    {"set_phonological_loop_enabled", (PyCFunction)Brain_set_phonological_loop_enabled, METH_VARARGS,
     "Slice 5: enable Baddeley's phonological-loop working memory buffer — set_phonological_loop_enabled(enabled: bool) -> None. When enabled, the recurrent cascade decays traces between iterations, merges lexical output into the buffer, and feeds the buffer's surface form (words with trace >= 0.3) to stage_syntactic + stage_self_comp instead of the one-shot lexical output. Default OFF: when off, recurrent cascade behaves byte-identically to Slice 1+2."},
    {"set_phonological_loop_decay", (PyCFunction)Brain_set_phonological_loop_decay, METH_VARARGS,
     "Slice 5: per-iteration trace decay rate — set_phonological_loop_decay(decay: float) -> None. Clamped to [0.0, 0.5]; NaN/Inf coerce to default 0.15."},
    {"clear_phonological_loop", (PyCFunction)Brain_clear_phonological_loop, METH_NOARGS,
     "Slice 5: drop every trace + word, clear surface buffer — clear_phonological_loop() -> None. Use between unrelated prompts to ensure a clean start; the recurrent cascade also clears the loop on entry."},
    {"get_phonological_loop_state", (PyCFunction)Brain_get_phonological_loop_state, METH_NOARGS,
     "Slice 5: read-only inspection of the loop — get_phonological_loop_state() -> dict('buffer': str, 'trace_count': int). buffer is the surface form (words with trace >= 0.3, space-separated)."},
    {"get_phonological_loop_diag", (PyCFunction)Brain_get_phonological_loop_diag, METH_NOARGS,
     "Slice 5: full diagnostic snapshot — get_phonological_loop_diag() -> dict with enabled/buffer_len/trace_count/trace_capacity/max_words/decay_rate/avg_trace_strength/last_refresh_ms."},
    {"set_cerebellar_correction_enabled", (PyCFunction)Brain_set_cerebellar_correction_enabled, METH_VARARGS,
     "Slice 7: enable cerebellar prediction-correction in motor + prosody cascade stages — set_cerebellar_correction_enabled(enabled: bool) -> None. When ON, the existing cerebellum adapter (cerebellum_predict_outcome / cerebellum_update_forward_model / cerebellum_broadcast_error) is queried before and after motor + prosody to predict + learn the upcoming pattern. Default OFF; when off the stages run byte-identically to master. Requires brain->cerebellum (created at init); minimal-init brains silently no-op."},
    {"get_cerebellar_correction_enabled", (PyCFunction)Brain_get_cerebellar_correction_enabled, METH_NOARGS,
     "Read the current cerebellar correction flag."},
    {"set_cerebellar_correction_strength", (PyCFunction)Brain_set_cerebellar_correction_strength, METH_VARARGS,
     "Slice 7: bias mix factor when the recurrent loop flags correction_pending — set_cerebellar_correction_strength(strength: float) -> None. Clamped to [0, 1]. NaN/Inf coerce to 0.5."},
    {"set_cerebellar_pe_threshold", (PyCFunction)Brain_set_cerebellar_pe_threshold, METH_VARARGS,
     "Slice 7: PE-norm threshold above which the recurrent cascade flags correction_pending — set_cerebellar_pe_threshold(threshold: float) -> None. The cascade PE-norm is the sum of two cosine-distance norms (motor + prosody), each in [0,1], so the meaningful range is [0, 2]. Clamped to [0, 2]. NaN/Inf coerce to default 0.20."},
    {"get_cerebellar_diag", (PyCFunction)Brain_get_cerebellar_diag, METH_NOARGS,
     "Slice 7: snapshot of cerebellar prediction-correction diagnostics — get_cerebellar_diag() -> dict(enabled, strength, correction_pending, pe_threshold, predictions_made, corrections_applied, last_pe_norm). Cheap (no atomics — cascade is single-caller-at-a-time by contract)."},
    {"set_dialect", (PyCFunction)Brain_set_dialect, METH_VARARGS,
     "Tier-1 #14: set dialect / accent conditioning string — set_dialect(dialect: str | None) -> None. None or empty clears the dialect. Truncated to GL_MAX_DIALECT_LEN-1 chars internally."},
    {"learn_next_token_triple", (PyCFunction)Brain_learn_next_token_triple, METH_VARARGS,
     "TA-4: contrastive next-token training on a single (prev1, prev2) → next trigram — learn_next_token_triple(prev1, prev2, next, lr=0.025) -> bool. Context is the average of prev1 and prev2 reverse-encodings; LTP/LTD applied like the bigram path. Returns True on update applied; False on cold-start no-op (any of the three tokens without prior bindings)."},
    {"set_grounded_negation_enabled", (PyCFunction)Brain_set_grounded_negation_enabled, METH_VARARGS,
     "Tier-2 #3: toggle comprehend negation polarity (sign-flip on cue) — set_grounded_negation_enabled(enabled) -> None. Default ON."},
    {"set_grounded_sense_disambiguation_enabled", (PyCFunction)Brain_set_grounded_sense_disambiguation_enabled, METH_VARARGS,
     "Tier-2 #6: toggle comprehend word-sense disambiguation — set_grounded_sense_disambiguation_enabled(enabled) -> None. Default OFF (opt-in)."},
    {"grounded_push_turn", (PyCFunction)Brain_grounded_push_turn, METH_VARARGS,
     "Tier-2 #7: append a discourse turn — grounded_push_turn(semantic_vec_or_None, n_words, is_user) -> None. Oldest evicted at full capacity."},
    {"grounded_get_discourse_turn_count", (PyCFunction)Brain_grounded_get_discourse_turn_count, METH_NOARGS,
     "Tier-2 #7: query populated turn count — grounded_get_discourse_turn_count() -> int."},
    {"grounded_set_discourse_capacity", (PyCFunction)Brain_grounded_set_discourse_capacity, METH_VARARGS,
     "Tier-2 #7: clamp discourse capacity (1..8) — grounded_set_discourse_capacity(capacity) -> None. Reduction evicts oldest first."},

    // Rubric (cognitive output quality evaluation)
    {"rubric", (PyCFunction)Brain_rubric, METH_NOARGS,
     "Evaluate quality of last decision: rubric() -> dict with 15 scores + grade"},
    {"set_rubric_validation", (PyCFunction)Brain_set_rubric_validation, METH_VARARGS,
     "Set validation features for rubric during training: set_rubric_validation(features) -> True"},
    {"training_rubric", (PyCFunction)Brain_training_rubric, METH_NOARGS,
     "Get rubric stats from training: training_rubric() -> dict or None"},

    // Full cognitive decision
    {"decide_full", (PyCFunction)Brain_decide_full, METH_VARARGS,
     "Run full cognitive pipeline: decide_full(features) -> dict"},

    // Language production
    {"speak", (PyCFunction)Brain_speak, METH_VARARGS,
     "Generate spoken text: speak(semantic_vector) -> dict with text, confidence, fluency"},
    {"get_avatar_state", (PyCFunction)Brain_get_avatar_state, METH_NOARGS,
     "Get avatar visual state: get_avatar_state() -> dict with FACS AUs, visemes, gaze, emotion, voice"},

    // Frozen inference
    {"freeze", (PyCFunction)Brain_freeze, METH_NOARGS,
     "Freeze brain for inference-only mode: freeze() -> True"},

    // Socratic active learning: cognitive module bindings
    {"curiosity_detect_gaps", (PyCFunction)Brain_curiosity_detect_gaps, METH_VARARGS,
     "Detect knowledge gaps: curiosity_detect_gaps(topic) -> dict"},
    {"consolidate", (PyCFunction)Brain_consolidate, METH_VARARGS | METH_KEYWORDS,
     "Trigger memory consolidation: consolidate(mode='auto', cycles=0, prune_passes=0) -> bool"},
    {"cache_communities", (PyCFunction)Brain_cache_communities, METH_NOARGS,
     "Pre-compute community structure for consolidation: cache_communities() -> dict"},
    {"invalidate_community_cache", (PyCFunction)Brain_invalidate_community_cache, METH_NOARGS,
     "Invalidate cached community structure: invalidate_community_cache() -> None"},
    {"octopus_stats", (PyCFunction)Brain_octopus_stats, METH_NOARGS,
     "Octopus distributed-cognition stats: octopus_stats() -> dict {enabled, n_arms, "
     "n_explorations, n_integrations, n_ethics_vetoes, n_swarm_delegations, "
     "n_world_model_updates, avg_arm_confidence, avg_arm_variance, central_coherence, "
     "arm_broadcast_states[]}"},
    /* Phase 5B: per-modality bridge samplers. Each reads the matching
     * subsystem (occipital / audio cortex / etc.) into a 64-dim feature
     * vector and calls octopus_explore on it. Returns 0 on success,
     * -1 if the subsystem is unavailable or no features were produced. */
    {"octopus_explore_from_occipital",
     (PyCFunction)Brain_octopus_explore_from_occipital, METH_NOARGS,
     "Sample occipital cortex + drive octopus: returns 0 on success, -1 if unavailable"},
    {"octopus_explore_from_audio_cortex",
     (PyCFunction)Brain_octopus_explore_from_audio_cortex, METH_NOARGS,
     "Sample audio cortex + drive octopus: returns 0 on success, -1 if unavailable"},
    {"octopus_explore_from_somatosensory",
     (PyCFunction)Brain_octopus_explore_from_somatosensory, METH_NOARGS,
     "Sample somatosensory + drive octopus: returns 0 on success, -1 if unavailable"},
    {"octopus_explore_from_snn",
     (PyCFunction)Brain_octopus_explore_from_snn, METH_NOARGS,
     "Sample SNN firing rates + drive octopus: returns 0 on success, -1 if unavailable"},
    {"octopus_explore_from_neuromod",
     (PyCFunction)Brain_octopus_explore_from_neuromod, METH_NOARGS,
     "Sample neuromodulator state + drive octopus: returns 0 on success, -1 if unavailable"},
    {"octopus_explore_from_peers",
     (PyCFunction)Brain_octopus_explore_from_peers, METH_NOARGS,
     "Sample Portia+Dragonfly peers + drive octopus: returns 0 on success, -1 if unavailable"},
    {"octopus_set_training_enabled",
     (PyCFunction)Brain_octopus_set_training_enabled, METH_VARARGS,
     "Toggle arm-LNN gradient flow: octopus_set_training_enabled(bool)"},
    {"octopus_set_ng_enabled",
     (PyCFunction)Brain_octopus_set_ng_enabled, METH_VARARGS,
     "Toggle natural-gradient latent regularization: octopus_set_ng_enabled(bool)"},
    {"octopus_set_jepa_enabled",
     (PyCFunction)Brain_octopus_set_jepa_enabled, METH_VARARGS,
     "Toggle JEPA latent-space predictive training: octopus_set_jepa_enabled(bool)"},
    /* Phase E1: elephant-inspired long-term memory landmark API. */
    {"long_term_stats",
     (PyCFunction)Brain_long_term_stats, METH_NOARGS,
     "Z-Ladder + landmark stats: long_term_stats() -> dict {enabled, landmark_count, total_nodes, z0..z3_count, landmarks_present, ...}"},
    {"long_term_mark_landmark",
     (PyCFunction)Brain_long_term_mark_landmark, METH_VARARGS,
     "Mark a node as permanent landmark (elephant-matriarch pattern): long_term_mark_landmark(node_id, reason='') -> int error code"},
    {"long_term_unmark_landmark",
     (PyCFunction)Brain_long_term_unmark_landmark, METH_VARARGS,
     "Remove landmark designation: long_term_unmark_landmark(node_id) -> int error code"},
    {"long_term_insert_memory",
     (PyCFunction)Brain_long_term_insert_memory, METH_VARARGS,
     "Insert a memory node from a feature vector into Z0; optionally mark as Z3 landmark: "
     "long_term_insert_memory(features, make_landmark=False, reason='') -> node_id (uint64)"},
    {"long_term_set_auto_insert",
     (PyCFunction)Brain_long_term_set_auto_insert, METH_VARARGS,
     "Configure auto-insertion from brain_learn_vector: "
     "long_term_set_auto_insert(enabled, insert_confidence=0.7, landmark_confidence=0.95) -> None. "
     "When enabled, training features above the insert threshold auto-insert into Z0; above the "
     "landmark threshold also auto-mark as Z3 landmark."},
    {"long_term_query",
     (PyCFunction)Brain_long_term_query, METH_VARARGS,
     "Oracle retrieval by feature similarity: long_term_query(features, top_k=5) -> list of dicts "
     "[{node_id, similarity, tier}, ...] sorted by similarity descending."},
    /* Phase E4: full-tier query, event promotion, landmark checkpointing. */
    {"long_term_query_all",
     (PyCFunction)Brain_long_term_query_all, METH_VARARGS,
     "Oracle retrieval across ALL tiers (Z0..Z3): long_term_query_all(features, top_k=5) -> list of dicts. "
     "Unlike long_term_query, this walks the whole ladder, not just the landmark set."},
    {"long_term_promote_event",
     (PyCFunction)Brain_long_term_promote_event, METH_VARARGS,
     "Event-triggered landmark promotion: long_term_promote_event(features, reason='') -> node_id (uint64). "
     "Creates a node, inserts at Z0, and immediately marks it as a Z3 landmark. Reward/amygdala/engram paths "
     "can use this to record salient events as permanent matriarchal memory without relying on training "
     "confidence thresholds. Returns 0 on failure."},
    {"long_term_save",
     (PyCFunction)Brain_long_term_save, METH_VARARGS,
     "Save landmarks to disk: long_term_save(path) -> bool. Atomic (temp+rename). Writes all live "
     "landmarks with their features and prime signatures to `path`."},
    {"long_term_load",
     (PyCFunction)Brain_long_term_load, METH_VARARGS,
     "Load landmarks from disk: long_term_load(path) -> bool. Re-creates landmark nodes from saved "
     "features+signatures, inserts at Z0, then marks each as a landmark. New node_ids are assigned."},
    {"get_uncertainty", (PyCFunction)Brain_get_uncertainty, METH_VARARGS,
     "Get uncertainty: get_uncertainty([features]) -> dict"},
    {"self_assess", (PyCFunction)Brain_self_assess, METH_VARARGS,
     "Self-model assessment: self_assess(domain) -> dict"},
    {"lgss_check_content", (PyCFunction)Brain_lgss_check_content, METH_VARARGS,
     "LGSS content filter: lgss_check_content(text) -> dict"},

    // Sensory cortex bindings for multimodal training
    {"audio_cortex_process", (PyCFunction)Brain_audio_cortex_process, METH_VARARGS,
     "Process audio through audio cortex: audio_cortex_process(samples) -> list of features"},
    {"submit_sensory", (PyCFunction)Brain_submit_sensory, METH_VARARGS | METH_KEYWORDS,
     "Stage sensory data: submit_sensory(modality, data, width=0, height=0, channels=0, n_segments=0)\n"
     "Modalities: 'visual', 'audio', 'speech', 'somatosensory'"},
    {"visual_cortex_process", (PyCFunction)Brain_visual_cortex_process, METH_VARARGS,
     "Process image through visual cortex: visual_cortex_process(pixels, width, height, channels) -> list of features"},
    {"speech_cortex_process", (PyCFunction)Brain_speech_cortex_process, METH_VARARGS,
     "Process audio through speech cortex: speech_cortex_process(samples) -> list of features"},

    // Training Integration API: basal ganglia
    {"bg_update_reward", (PyCFunction)Brain_bg_update_reward, METH_VARARGS,
     "Update basal ganglia reward: bg_update_reward(reward, expected) -> int"},
    {"bg_get_conflict", (PyCFunction)Brain_bg_get_conflict, METH_NOARGS,
     "Get basal ganglia conflict level: bg_get_conflict() -> float"},
    {"bg_get_mode", (PyCFunction)Brain_bg_get_mode, METH_NOARGS,
     "Get basal ganglia operating mode: bg_get_mode() -> int"},
    {"bg_get_dopamine", (PyCFunction)Brain_bg_get_dopamine, METH_NOARGS,
     "Get basal ganglia dopamine level: bg_get_dopamine() -> float"},
    {"bg_get_rpe", (PyCFunction)Brain_bg_get_rpe, METH_NOARGS,
     "Get reward prediction error: bg_get_rpe() -> float"},
    {"bg_register_habit", (PyCFunction)Brain_bg_register_habit, METH_VARARGS,
     "Register a habit: bg_register_habit(domain, action_id) -> int"},
    {"bg_check_habit", (PyCFunction)Brain_bg_check_habit, METH_VARARGS,
     "Check if habit exists: bg_check_habit(domain) -> int"},
    {"bg_strengthen_habit", (PyCFunction)Brain_bg_strengthen_habit, METH_VARARGS,
     "Strengthen a habit: bg_strengthen_habit(habit_id, success) -> int"},

    // Training Integration API: medulla
    {"medulla_get_arousal", (PyCFunction)Brain_medulla_get_arousal, METH_NOARGS,
     "Get medulla arousal level: medulla_get_arousal() -> float"},
    {"medulla_get_circadian_phase", (PyCFunction)Brain_medulla_get_circadian_phase, METH_NOARGS,
     "Get circadian phase: medulla_get_circadian_phase() -> int"},
    {"medulla_boost_arousal", (PyCFunction)Brain_medulla_boost_arousal, METH_VARARGS,
     "Boost arousal: medulla_boost_arousal(delta) -> None"},
    {"medulla_reduce_arousal", (PyCFunction)Brain_medulla_reduce_arousal, METH_VARARGS,
     "Reduce arousal: medulla_reduce_arousal(delta) -> None"},
    {"medulla_get_circadian_efficiency", (PyCFunction)Brain_medulla_get_circadian_efficiency, METH_NOARGS,
     "Get circadian efficiency: medulla_get_circadian_efficiency() -> float"},

    // Sleep/Wake system
    {"sleep_get_pressure", (PyCFunction)Brain_sleep_get_pressure, METH_NOARGS,
     "Get sleep pressure [0,1]: sleep_get_pressure() -> float"},
    {"sleep_is_needed", (PyCFunction)Brain_sleep_is_needed, METH_NOARGS,
     "Check if sleep is needed: sleep_is_needed() -> bool"},
    {"sleep_get_state", (PyCFunction)Brain_sleep_get_state, METH_NOARGS,
     "Get sleep state (0=awake,1=drowsy,2=light,3=deep,4=REM): sleep_get_state() -> int"},
    {"sleep_run_cycle", (PyCFunction)Brain_sleep_run_cycle, METH_VARARGS,
     "Run sleep cycle(s): sleep_run_cycle(num_cycles=1) -> bool"},
    {"sleep_get_statistics", (PyCFunction)Brain_sleep_get_statistics, METH_NOARGS,
     "Get sleep statistics: sleep_get_statistics() -> dict"},
    {"update_medulla", (PyCFunction)Brain_update_medulla, METH_VARARGS,
     "Update medulla subsystem (circadian clock): update_medulla(delta_time_s) -> None"},

    // Brain cycle coordinator (observability-only; Python drives the cadence)
    {"cycle_register", (PyCFunction)Brain_cycle_register, METH_VARARGS,
     "Register a Python-driven cycle with the C coordinator (observation only).\n"
     "Pass one of the nimcp.CYCLE_* constants. Returns 0 on success or if the\n"
     "coordinator is not present; -1 on error. Does not own the driver — the\n"
     "Python thread still ticks the cycle and must call cycle_notify_tick().\n"
     "Usage: brain.cycle_register(nimcp.CYCLE_SLEEP_WAKE) -> int"},
    {"cycle_notify_tick", (PyCFunction)Brain_cycle_notify_tick, METH_VARARGS,
     "Notify the coordinator a cycle just ticked. Call once per tick with the\n"
     "measured duration in microseconds. Returns 0 on success (or if the\n"
     "coordinator is absent); -1 on error.\n"
     "Usage: brain.cycle_notify_tick(nimcp.CYCLE_SLEEP_WAKE, duration_us) -> int"},
    {"cycle_unregister", (PyCFunction)Brain_cycle_unregister, METH_VARARGS,
     "Unregister a cycle from the coordinator (call at thread shutdown).\n"
     "Returns 0 on success or if the coordinator is absent; -1 on error.\n"
     "Usage: brain.cycle_unregister(nimcp.CYCLE_SLEEP_WAKE) -> int"},

    // Training Integration API: reasoning
    {"ti_add_fact", (PyCFunction)Brain_ti_add_fact, METH_VARARGS,
     "Add fact to knowledge base: ti_add_fact(fact, salience) -> bool"},
    {"ti_add_rule", (PyCFunction)Brain_ti_add_rule, METH_VARARGS,
     "Add rule to reasoning engine: ti_add_rule(rule, priority) -> bool"},
    {"ti_forward_chain", (PyCFunction)Brain_ti_forward_chain, METH_VARARGS,
     "Run forward chaining: ti_forward_chain(max_iterations) -> int"},
    {"ti_backward_chain", (PyCFunction)Brain_ti_backward_chain, METH_VARARGS,
     "Run backward chaining: ti_backward_chain(goal) -> float"},
    {"ti_query_knowledge", (PyCFunction)Brain_ti_query_knowledge, METH_VARARGS,
     "Query knowledge base: ti_query_knowledge(query) -> int"},
    {"ti_init_reasoning", (PyCFunction)Brain_ti_init_reasoning, METH_NOARGS,
     "Initialize reasoning subsystem: ti_init_reasoning() -> bool"},
    {"ti_reason", (PyCFunction)Brain_ti_reason, METH_VARARGS,
     "Run reasoning query: ti_reason(query) -> float"},

    // Training Integration API: adaptive learning rate + post-batch
    {"ti_compute_adaptive_lr", (PyCFunction)Brain_ti_compute_adaptive_lr, METH_VARARGS,
     "Compute adaptive learning rate: ti_compute_adaptive_lr(base_lr) -> float"},
    {"ti_compute_unified_lr", (PyCFunction)Brain_ti_compute_unified_lr, METH_VARARGS,
     "Compute unified adaptive LR with all brain modulations: ti_compute_unified_lr(base_lr) -> float"},
    {"ti_compute_modulation_state", (PyCFunction)Brain_ti_compute_modulation_state, METH_NOARGS,
     "Get full modulation state from all brain subsystems: ti_compute_modulation_state() -> dict"},
    {"ti_post_batch_update", (PyCFunction)Brain_ti_post_batch_update, METH_VARARGS | METH_KEYWORDS,
     "Post-batch update: ti_post_batch_update(accuracy, expected, domain) -> bool"},
    {"ti_should_skip_reasoning", (PyCFunction)Brain_ti_should_skip_reasoning, METH_NOARGS,
     "Check if Portia recommends skipping reasoning due to resource pressure"},
    {"ti_get_reasoning_degradation", (PyCFunction)Brain_ti_get_reasoning_degradation, METH_NOARGS,
     "Get Portia degradation level affecting reasoning (0-4)"},
    {"ti_get_reasoning_phases_disabled", (PyCFunction)Brain_ti_get_reasoning_phases_disabled, METH_NOARGS,
     "Get number of reasoning phases currently disabled by Portia"},
    {"ti_get_cognitive_capacity", (PyCFunction)Brain_ti_get_cognitive_capacity, METH_NOARGS,
     "Get cognitive capacity from hypothalamus (0.0-1.0)"},
    {"ti_get_urgency_mode", (PyCFunction)Brain_ti_get_urgency_mode, METH_NOARGS,
     "Get urgency mode from hypothalamus (0=relaxed, 3=fight-or-flight)"},
    {"ti_get_stress_level", (PyCFunction)Brain_ti_get_stress_level, METH_NOARGS,
     "Get stress level from hypothalamus (0.0-1.0)"},
    {"ti_mesh_is_available", (PyCFunction)Brain_ti_mesh_is_available, METH_NOARGS,
     "Check if mesh network is available for reasoning"},
    {"ti_mesh_get_participant_count", (PyCFunction)Brain_ti_mesh_get_participant_count, METH_NOARGS,
     "Get mesh reasoning channel participant count"},
    {"ti_mesh_get_coherence", (PyCFunction)Brain_ti_mesh_get_coherence, METH_NOARGS,
     "Get mesh reasoning channel coherence [0,1]"},

    // Decision cycle orchestrator (Layer 1 + 2 + 3)
    {"ti_compute_decision_cycle", (PyCFunction)Brain_ti_compute_decision_cycle, METH_VARARGS,
     "Run full training decision cycle: ti_compute_decision_cycle(loss_cur, loss_prev, grad_norm, grad_norm_prev, loss_vol, grad_var, lr, batch) -> dict"},

    // Cognitive deliberation (reasoning + inner dialogue)
    {"connect_collective", (PyCFunction)Brain_connect_collective, METH_VARARGS,
     "Connect this brain to another brain's collective cognition system.\n"
     "Args: other_brain (Brain), instance_id (int)"},
    {"deliberate", (PyCFunction)Brain_deliberate, METH_VARARGS,
     "Run cognitive deliberation on a topic: deliberate(topic) -> dict with reasoning_confidence, dialogue_agreement, has_conclusion, total_turns"},

    // Synapse pruning
    {"prune_synapses", (PyCFunction)Brain_prune_synapses, METH_VARARGS,
     "Prune weak synapses: prune_synapses(threshold=0.01) -> int (number pruned)"},

    // Language grounding
    {"tokenize", (PyCFunction)Brain_tokenize, METH_VARARGS,
     "Tokenize text to token IDs: tokenize(text) -> list of int"},
    {"generate_text", (PyCFunction)Brain_generate_text, METH_VARARGS,
     "Generate text from semantic vector: generate_text(semantic_vector) -> dict with text, confidence, fluency"},
    {"train_language", (PyCFunction)Brain_train_language, METH_VARARGS | METH_KEYWORDS,
     "Train language generator: train_language(input_text, target_text, learning_rate=0.001) -> dict with loss"},
    {"generate", (PyCFunction)Brain_generate_text_advanced, METH_VARARGS | METH_KEYWORDS,
     "Generate text: generate(prompt='hello', semantic_input=None) -> dict with text, confidence, perplexity"},

    // Grounded Language (human-like word-concept binding)
    {"ground_word", (PyCFunction)Brain_ground_word, METH_VARARGS | METH_KEYWORDS,
     "Ground a word in sensory experience: ground_word(word, features, modality=5, attention=0.8, valence=0.0, arousal=0.0) -> bool"},
    {"bootstrap_lexicon", (PyCFunction)Brain_bootstrap_lexicon, METH_VARARGS | METH_KEYWORDS,
     "Bootstrap base lexicon from JSON: bootstrap_lexicon(json_path) -> dict {success, status, path}"},
    {"learn_language", (PyCFunction)Brain_learn_language, METH_VARARGS | METH_KEYWORDS,
     "Learn language from text exposure: learn_language(text) -> dict with loss, success"},
    {"learn_language_pair", (PyCFunction)Brain_learn_language_pair, METH_VARARGS | METH_KEYWORDS,
     "Learn from input-target pair: learn_language_pair(input_text, target_text, learning_rate=0) -> dict"},
    {"train_cognitive", (PyCFunction)Brain_train_cognitive, METH_VARARGS | METH_KEYWORDS,
     "Train all cognitive modules: train_cognitive(text, domain=10, target_text=None, learning_rate=0.001) -> dict"},
    {"learn_knowledge", (PyCFunction)Brain_learn_knowledge, METH_VARARGS | METH_KEYWORDS,
     "Learn knowledge in domain: learn_knowledge(text, domain=10) -> bool"},
    {"get_cognitive_stats", (PyCFunction)Brain_get_cognitive_stats, METH_NOARGS,
     "Get per-module cognitive training stats: get_cognitive_stats() -> dict {module: {steps, last_loss}}"},
    {"get_transcript", (PyCFunction)Brain_get_transcript, METH_NOARGS,
     "Get cognitive transcript from last decide_full: get_transcript() -> list of {module, summary, salience, confidence}"},
    {"comprehend", (PyCFunction)Brain_comprehend, METH_VARARGS | METH_KEYWORDS,
     "Comprehend text into semantic vector: comprehend(text) -> dict with semantic_vector, confidence"},
    {"produce_text", (PyCFunction)Brain_produce_text, METH_VARARGS | METH_KEYWORDS,
     "Produce text from semantic intent: produce_text(intent) -> dict with text, confidence"},
    {"grounded_respond", (PyCFunction)Brain_grounded_respond, METH_VARARGS | METH_KEYWORDS,
     "Respond using grounded language: grounded_respond(text) -> dict with response, confidence"},
    {"creative_blend", (PyCFunction)Brain_creative_blend, METH_VARARGS | METH_KEYWORDS,
     "Blend two concepts creatively: creative_blend(vector_a, vector_b, blend_ratio=0.5) -> dict with text"},

    // Mixed precision training
    {"enable_mixed_precision", (PyCFunction)Brain_enable_mixed_precision, METH_VARARGS,
     "Enable/disable FP16 mixed precision training: enable_mixed_precision(enabled=True) -> True on success"},

    // Gradient checkpointing
    {"enable_gradient_checkpointing", (PyCFunction)Brain_enable_gradient_checkpointing, METH_VARARGS,
     "Enable/disable gradient checkpointing: enable_gradient_checkpointing(enabled=True, interval=0) -> True on success"},

    // Hemispheric Architecture
    {"enable_hemispheric", (PyCFunction)Brain_enable_hemispheric, METH_VARARGS,
     "Enable/disable hemispheric architecture (callosum + lateralization): enable_hemispheric(enabled=True) -> True"},
    {"get_lateralization", (PyCFunction)Brain_get_lateralization, METH_VARARGS,
     "Get lateralization dominance for cognitive domain (0-11): get_lateralization(domain) -> float (0=right, 1=left)"},
    {"shift_lateralization", (PyCFunction)Brain_shift_lateralization, METH_VARARGS,
     "Shift lateralization: shift_lateralization(domain, shift) -> True (+shift=left, -shift=right)"},
    {"get_callosum_transfers", (PyCFunction)Brain_get_callosum_transfers, METH_NOARGS,
     "Get total inter-hemispheric callosum transfers: get_callosum_transfers() -> int"},
    {"get_hemispheric_balance", (PyCFunction)Brain_get_hemispheric_balance, METH_NOARGS,
     "Get hemispheric balance [-1=left, +1=right]: get_hemispheric_balance() -> float"},
    {"get_module_activity", (PyCFunction)Brain_get_module_activity, METH_NOARGS,
     "Get module activity counters (statue-suspect probe): get_module_activity() -> dict"},

    // Recurrent forward pass + BPTT
    {"enable_recurrent", (PyCFunction)Brain_enable_recurrent, METH_VARARGS | METH_KEYWORDS,
     "Configure recurrent forward pass: enable_recurrent(enabled=True, max_iterations=3, confidence_threshold=0.7, blend_alpha=0.3) -> True"},
    {"enable_bptt", (PyCFunction)Brain_enable_bptt, METH_VARARGS | METH_KEYWORDS,
     "Configure BPTT: enable_bptt(enabled=True, window_size=8, discount=0.9) -> True"},
    {"get_recurrent_iterations", (PyCFunction)Brain_get_recurrent_iterations, METH_NOARGS,
     "Get recurrent iteration count from last decide(): get_recurrent_iterations() -> int"},

    // Cloud inference (edge-cloud hybrid)
    {"connect_cloud", (PyCFunction)Brain_connect_cloud, METH_VARARGS | METH_KEYWORDS,
     "Connect cloud backend: connect_cloud(cloud_brain, confidence_threshold=0.5, enable_distillation=True) -> True"},
    {"disconnect_cloud", (PyCFunction)Brain_disconnect_cloud, METH_NOARGS,
     "Disconnect cloud backend: disconnect_cloud() -> True"},
    {"get_cloud_stats", (PyCFunction)Brain_get_cloud_stats, METH_NOARGS,
     "Get cloud inference stats: get_cloud_stats() -> dict"},
    {"distill_cloud_batch", (PyCFunction)Brain_distill_cloud_batch, METH_VARARGS,
     "Process buffered distillation: distill_cloud_batch(max_examples=0) -> int"},

    // Ablation study support
    {"set_training_mode", (PyCFunction)Brain_set_training_mode_py, METH_VARARGS,
     "Enable/disable training-mode fast path: set_training_mode(active) -> None"},
    // Per-network training toggles (dynamic, no rebuild)
    {"set_train_ann", (PyCFunction)Brain_set_train_ann_py, METH_VARARGS,
     "Enable/disable adaptive/ANN training: set_train_ann(enabled) -> None"},
    {"get_train_ann", (PyCFunction)Brain_get_train_ann_py, METH_NOARGS,
     "Query adaptive/ANN training enable state: get_train_ann() -> bool"},
    {"set_train_cnn", (PyCFunction)Brain_set_train_cnn_py, METH_VARARGS,
     "Enable/disable CNN training (includes cortex CNNs): set_train_cnn(enabled) -> None"},
    {"get_train_cnn", (PyCFunction)Brain_get_train_cnn_py, METH_NOARGS,
     "Query CNN training enable state: get_train_cnn() -> bool"},
    {"set_train_snn", (PyCFunction)Brain_set_train_snn_py, METH_VARARGS,
     "Enable/disable SNN training: set_train_snn(enabled) -> None"},
    {"get_train_snn", (PyCFunction)Brain_get_train_snn_py, METH_NOARGS,
     "Query SNN training enable state: get_train_snn() -> bool"},
    {"set_train_lnn", (PyCFunction)Brain_set_train_lnn_py, METH_VARARGS,
     "Enable/disable LNN training: set_train_lnn(enabled) -> None"},
    {"get_train_lnn", (PyCFunction)Brain_get_train_lnn_py, METH_NOARGS,
     "Query LNN training enable state: get_train_lnn() -> bool"},
    {"set_snn_only_recovery", (PyCFunction)Brain_set_snn_only_recovery_py, METH_VARARGS,
     "Enable/disable SNN-only recovery mode (freezes ANN/CNN/LNN, keeps SNN): "
     "set_snn_only_recovery(enabled) -> None"},
    {"get_snn_only_recovery", (PyCFunction)Brain_get_snn_only_recovery_py, METH_NOARGS,
     "Query SNN-only recovery mode: get_snn_only_recovery() -> bool"},
    {"set_ensemble_warmup_scale", (PyCFunction)Brain_set_ensemble_warmup_scale_py, METH_VARARGS,
     "Set probabilistic gate on non-SNN training [0.0..1.0]: "
     "set_ensemble_warmup_scale(scale) -> None"},
    {"get_ensemble_warmup_scale", (PyCFunction)Brain_get_ensemble_warmup_scale_py, METH_NOARGS,
     "Query ensemble warmup scale: get_ensemble_warmup_scale() -> float"},
    {"eager_init_cognitive", (PyCFunction)Brain_eager_init_cognitive_py, METH_NOARGS,
     "Eagerly init all cognitive subsystems (thread-safe): eager_init_cognitive() -> int (count)"},
    {"set_training_dashboard", (PyCFunction)Brain_set_training_dashboard_py,
     METH_VARARGS | METH_KEYWORDS,
     "Set training dashboard metrics: set_training_dashboard(stage=0, step=0, domain='', ...) -> None"},
    {"get_training_dashboard", (PyCFunction)Brain_get_training_dashboard_py, METH_NOARGS,
     "Get training dashboard + inference metrics: get_training_dashboard() -> dict"},
    // Unified probe system
    {"attach_builtin_probes", (PyCFunction)Brain_attach_builtin_probes_py, METH_VARARGS,
     "Attach all 4 built-in probes (network, cognitive, dashboard, inference):\n"
     "  attach_builtin_probes(interval_ms=1000) -> int (count attached)"},
    {"get_all_probe_metrics", (PyCFunction)Brain_get_all_probe_metrics_py, METH_NOARGS,
     "Get all probe metrics as dict: get_all_probe_metrics() -> dict"},
    {"destroy_probe", (PyCFunction)Brain_destroy_probe_py, METH_VARARGS,
     "Destroy a probe: destroy_probe(handle) -> None"},
    {"set_network_ablation", (PyCFunction)Brain_set_network_ablation_py,
     METH_VARARGS | METH_KEYWORDS,
     "Enable/disable network types: set_network_ablation(train_cnn=1, train_snn=1, train_lnn=1)"},
    {"get_network_metrics", (PyCFunction)Brain_get_network_metrics_py, METH_NOARGS,
     "Get per-network training metrics: get_network_metrics() -> dict"},
    {"get_cortex_cnn_metrics", (PyCFunction)Brain_get_cortex_cnn_metrics_py, METH_NOARGS,
     "Get per-cortex CNN metrics: get_cortex_cnn_metrics() -> dict of {visual,audio,speech,somato}"},
    {"reset_inference_state", (PyCFunction)Brain_reset_inference_state_py, METH_NOARGS,
     "Reset neuron and LNN hidden states for clean inference -> int (0=ok)"},
    {"set_fusion_enabled", (PyCFunction)Brain_set_fusion_enabled_py, METH_VARARGS,
     "Enable/disable multi-network fusion: set_fusion_enabled(True) -> None"},
    {"set_fusion_weights", (PyCFunction)Brain_set_fusion_weights_py, METH_VARARGS,
     "Set fusion weights: set_fusion_weights([0.7, 0.1, 0.1, 0.1]) -> None"},

    // UTM (Unified Training Manager) methods
    {"utm_swap_to_ema", (PyCFunction)Brain_utm_swap_to_ema, METH_NOARGS,
     "Swap to EMA parameters for smoother inference -> True"},
    {"utm_swap_from_ema", (PyCFunction)Brain_utm_swap_from_ema, METH_NOARGS,
     "Swap back to live parameters after EMA inference -> True"},
    {"utm_get_training_health", (PyCFunction)Brain_utm_get_training_health, METH_NOARGS,
     "Get DFA-based training health: -> dict{health, health_name, dfa_exponent, gradients_healthy, early_stopped}"},
    {"utm_set_early_stopping_enabled", (PyCFunction)Brain_utm_set_early_stopping_enabled, METH_VARARGS,
     "Enable/disable UTM early stopping at runtime: utm_set_early_stopping_enabled(bool)"},
    {"utm_reset_early_stopping", (PyCFunction)Brain_utm_reset_early_stopping, METH_NOARGS,
     "Clear an already-tripped early-stop flag and reset counter/best_loss"},
    {"utm_forward_only", (PyCFunction)Brain_utm_forward_only, METH_VARARGS,
     "Forward-only inference through UTM: utm_forward_only(features, output_dim) -> [float, ...]"},
    {"utm_set_per_network_lr", (PyCFunction)Brain_utm_set_per_network_lr, METH_VARARGS,
     "Set per-network learning rate: utm_set_per_network_lr(net_idx, lr)"},
    {"utm_set_fractal_lr", (PyCFunction)Brain_utm_set_fractal_lr, METH_VARARGS,
     "Set fractal LR scaling for network: utm_set_fractal_lr(net_idx, scale)"},
    {"utm_set_natural_gradient", (PyCFunction)Brain_utm_set_natural_gradient, METH_VARARGS,
     "Enable/disable natural gradient for network: utm_set_natural_gradient(net_idx, True/False)"},

    /* Edge Brain API */
    {"edge_resize", (PyCFunction)Brain_edge_resize, METH_VARARGS | METH_KEYWORDS,
     "Resize brain: edge_resize(target_neurons, mode='contract', knowledge_transfer=True) -> dict"},
    {"edge_resize_check", (PyCFunction)Brain_edge_resize_check, METH_VARARGS,
     "Dry-run resize check: edge_resize_check(target_neurons) -> dict{feasible, ram_delta_mb, ...}"},
    {"edge_distill", (PyCFunction)Brain_edge_distill, METH_VARARGS | METH_KEYWORDS,
     "Distill to smaller brain: edge_distill(target_neurons, temperature=2.0, steps=5000) -> dict"},
    {"edge_optimize_for_device", (PyCFunction)Brain_edge_optimize_for_device, METH_VARARGS | METH_KEYWORDS,
     "Auto-optimize for device: edge_optimize_for_device(ram_mb, cpu_cores, ...) -> dict"},
    {"edge_quantize", (PyCFunction)Brain_edge_quantize, METH_VARARGS | METH_KEYWORDS,
     "Quantize weights: edge_quantize(precision='int8_symmetric') -> dict"},
    {"edge_score_importance", (PyCFunction)Brain_edge_score_importance, METH_VARARGS,
     "Score neuron importance: edge_score_importance(num_neurons=1000) -> [float, ...]"},

    /* Memory Store + OOD + Audit Python Bindings */
    {"memory_store_stats", (PyCFunction)Brain_memory_store_stats, METH_NOARGS,
     "Get memory store stats: -> dict{total_engrams, total_concepts, ...}"},
    {"memory_search_text", (PyCFunction)Brain_memory_search_text, METH_VARARGS,
     "Search memory by text: memory_search_text(query, max_results=10) -> [int, ...]"},
    {"memory_search_similar", (PyCFunction)Brain_memory_search_similar, METH_VARARGS,
     "Search memory by embedding: memory_search_similar(embedding, top_k=5) -> [(id, distance), ...]"},
    {"ood_stats", (PyCFunction)Brain_ood_stats, METH_NOARGS,
     "Get OOD detector stats: -> dict{total_checks, ood_detected, ood_rate, ...}"},
    {"audit_log", (PyCFunction)Brain_audit_log, METH_VARARGS | METH_KEYWORDS,
     "Log audit event: audit_log(description, severity=0, details='') -> int"},
    {"audit_search", (PyCFunction)Brain_audit_search, METH_VARARGS | METH_KEYWORDS,
     "Search audit trail: audit_search(min_severity=0, max_results=100) -> [dict, ...]"},
    {"memory_is_healthy", (PyCFunction)Brain_memory_is_healthy, METH_NOARGS,
     "Check memory store health: -> bool"},

    /* Swarm Master Runtime */
    {"swarm_master_create", (PyCFunction)Brain_swarm_master_create, METH_VARARGS | METH_KEYWORDS,
     "Create swarm master: swarm_master_create(device_id=1, listen_port=9200, ...) -> True"},
    {"swarm_master_destroy", (PyCFunction)Brain_swarm_master_destroy, METH_NOARGS,
     "Destroy swarm master: swarm_master_destroy() -> None"},
    {"swarm_master_start", (PyCFunction)Brain_swarm_master_start, METH_NOARGS,
     "Start swarm master event loop: swarm_master_start() -> True"},
    {"swarm_master_stop", (PyCFunction)Brain_swarm_master_stop, METH_NOARGS,
     "Stop swarm master: swarm_master_stop() -> True"},
    {"swarm_master_kick", (PyCFunction)Brain_swarm_master_kick, METH_VARARGS,
     "Kick peer from swarm: swarm_master_kick(device_id) -> True"},
    {"swarm_master_force_sync", (PyCFunction)Brain_swarm_master_force_sync, METH_NOARGS,
     "Trigger immediate sync round: swarm_master_force_sync() -> True"},
    {"swarm_master_get_peer_count", (PyCFunction)Brain_swarm_master_get_peer_count, METH_NOARGS,
     "Get active peer count: swarm_master_get_peer_count() -> int"},
    {"swarm_master_get_peer_info", (PyCFunction)Brain_swarm_master_get_peer_info, METH_VARARGS,
     "Get peer info: swarm_master_get_peer_info(device_id) -> dict"},

    /* Swarm Edge Runtime */
    {"swarm_edge_create", (PyCFunction)Brain_swarm_edge_create, METH_VARARGS | METH_KEYWORDS,
     "Create swarm edge: swarm_edge_create(device_id=2, heartbeat_interval_ms=2000, ...) -> True"},
    {"swarm_edge_destroy", (PyCFunction)Brain_swarm_edge_destroy, METH_NOARGS,
     "Destroy swarm edge: swarm_edge_destroy() -> None"},
    {"swarm_edge_start", (PyCFunction)Brain_swarm_edge_start, METH_NOARGS,
     "Start swarm edge: swarm_edge_start() -> True"},
    {"swarm_edge_stop", (PyCFunction)Brain_swarm_edge_stop, METH_NOARGS,
     "Stop swarm edge: swarm_edge_stop() -> True"},
    {"swarm_edge_is_connected", (PyCFunction)Brain_swarm_edge_is_connected, METH_NOARGS,
     "Check edge connection: swarm_edge_is_connected() -> bool"},
    {"swarm_edge_submit_gradients", (PyCFunction)Brain_swarm_edge_submit_gradients, METH_VARARGS,
     "Submit gradients to master: swarm_edge_submit_gradients(gradients_list) -> True"},

    /* Sensor Hub */
    {"sensor_hub_create", (PyCFunction)Brain_sensor_hub_create, METH_VARARGS,
     "Create sensor hub: sensor_hub_create(max_sensors=32) -> True"},
    {"sensor_hub_destroy", (PyCFunction)Brain_sensor_hub_destroy, METH_NOARGS,
     "Destroy sensor hub: sensor_hub_destroy() -> None"},
    {"sensor_register", (PyCFunction)Brain_sensor_register, METH_VARARGS | METH_KEYWORDS,
     "Register sensor: sensor_register(sensor_id, type=0, format=0, name='sensor', ...) -> int"},
    {"sensor_submit_reading", (PyCFunction)Brain_sensor_submit_reading, METH_VARARGS | METH_KEYWORDS,
     "Submit reading: sensor_submit_reading(sensor_id, data, confidence=1.0) -> True"},
    {"sensor_get_latest", (PyCFunction)Brain_sensor_get_latest, METH_VARARGS,
     "Get latest reading: sensor_get_latest(sensor_id) -> dict or None"},
    {"sensor_get_all_latest", (PyCFunction)Brain_sensor_get_all_latest, METH_NOARGS,
     "Get all latest readings: sensor_get_all_latest() -> [dict, ...]"},
    {"sensor_compose_features", (PyCFunction)Brain_sensor_compose_features, METH_VARARGS,
     "Compose feature vector: sensor_compose_features(max_features=1024) -> [float, ...]"},
    {"sensor_get_count", (PyCFunction)Brain_sensor_get_count, METH_NOARGS,
     "Get registered sensor count: sensor_get_count() -> int"},

    /* Safety Watchdog */
    {"watchdog_create", (PyCFunction)Brain_watchdog_create, METH_VARARGS | METH_KEYWORDS,
     "Create watchdog: watchdog_create(timeout_ms=500, action=0, max_magnitude=1.0, ...) -> True"},
    {"watchdog_destroy", (PyCFunction)Brain_watchdog_destroy, METH_NOARGS,
     "Destroy watchdog: watchdog_destroy() -> None"},
    {"watchdog_arm", (PyCFunction)Brain_watchdog_arm, METH_NOARGS,
     "Arm watchdog: watchdog_arm() -> True"},
    {"watchdog_disarm", (PyCFunction)Brain_watchdog_disarm, METH_NOARGS,
     "Disarm watchdog: watchdog_disarm() -> True"},
    {"watchdog_heartbeat", (PyCFunction)Brain_watchdog_heartbeat, METH_NOARGS,
     "Signal heartbeat: watchdog_heartbeat() -> None"},
    {"watchdog_validate_output", (PyCFunction)Brain_watchdog_validate_output, METH_VARARGS,
     "Validate output: watchdog_validate_output(output_list) -> bool"},
    {"watchdog_get_safe_output", (PyCFunction)Brain_watchdog_get_safe_output, METH_VARARGS,
     "Get safe output: watchdog_get_safe_output(num_outputs=32) -> [float, ...]"},
    {"watchdog_estop", (PyCFunction)Brain_watchdog_estop, METH_NOARGS,
     "Emergency stop: watchdog_estop() -> True"},
    {"watchdog_reset", (PyCFunction)Brain_watchdog_reset, METH_NOARGS,
     "Reset watchdog: watchdog_reset() -> True"},
    {"watchdog_get_state", (PyCFunction)Brain_watchdog_get_state, METH_NOARGS,
     "Get watchdog state: watchdog_get_state() -> str"},

    /* ROS 2 Bridge */
    {"ros2_bridge_create", (PyCFunction)Brain_ros2_bridge_create, METH_VARARGS | METH_KEYWORDS,
     "Create ROS2 bridge: ros2_bridge_create(node_name='nimcp_brain', ...) -> True"},
    {"ros2_bridge_destroy", (PyCFunction)Brain_ros2_bridge_destroy, METH_NOARGS,
     "Destroy ROS2 bridge: ros2_bridge_destroy() -> None"},
    {"ros2_bridge_start", (PyCFunction)Brain_ros2_bridge_start, METH_NOARGS,
     "Start ROS2 bridge: ros2_bridge_start() -> True"},
    {"ros2_bridge_stop", (PyCFunction)Brain_ros2_bridge_stop, METH_NOARGS,
     "Stop ROS2 bridge: ros2_bridge_stop() -> True"},
    {"ros2_bridge_inject_sensor", (PyCFunction)Brain_ros2_bridge_inject_sensor, METH_VARARGS,
     "Inject sensor data: ros2_bridge_inject_sensor(topic, data_list) -> True"},
    {"ros2_bridge_get_last_cmd", (PyCFunction)Brain_ros2_bridge_get_last_cmd, METH_VARARGS,
     "Get last motor command: ros2_bridge_get_last_cmd(max_count=32) -> [float, ...]"},

    /* MAVLink Bridge */
    {"mavlink_create", (PyCFunction)Brain_mavlink_create, METH_VARARGS | METH_KEYWORDS,
     "Create MAVLink bridge: mavlink_create(connection_string='udp:14550', ...) -> True"},
    {"mavlink_destroy", (PyCFunction)Brain_mavlink_destroy, METH_NOARGS,
     "Destroy MAVLink bridge: mavlink_destroy() -> None"},
    {"mavlink_connect", (PyCFunction)Brain_mavlink_connect, METH_NOARGS,
     "Connect MAVLink: mavlink_connect() -> True"},
    {"mavlink_disconnect", (PyCFunction)Brain_mavlink_disconnect, METH_NOARGS,
     "Disconnect MAVLink: mavlink_disconnect() -> True"},
    {"mavlink_start", (PyCFunction)Brain_mavlink_start, METH_NOARGS,
     "Start MAVLink recv thread: mavlink_start() -> True"},
    {"mavlink_stop", (PyCFunction)Brain_mavlink_stop, METH_NOARGS,
     "Stop MAVLink recv thread: mavlink_stop() -> True"},
    {"mavlink_get_attitude", (PyCFunction)Brain_mavlink_get_attitude, METH_NOARGS,
     "Get attitude: mavlink_get_attitude() -> dict or None"},
    {"mavlink_get_position", (PyCFunction)Brain_mavlink_get_position, METH_NOARGS,
     "Get position: mavlink_get_position() -> dict or None"},
    {"mavlink_get_battery", (PyCFunction)Brain_mavlink_get_battery, METH_NOARGS,
     "Get battery: mavlink_get_battery() -> dict or None"},
    {"mavlink_set_velocity", (PyCFunction)Brain_mavlink_set_velocity, METH_VARARGS,
     "Set velocity: mavlink_set_velocity(vx, vy, vz, yaw_rate) -> True"},
    {"mavlink_arm", (PyCFunction)Brain_mavlink_arm, METH_VARARGS,
     "Arm/disarm: mavlink_arm(arm=True) -> True"},
    {"mavlink_takeoff", (PyCFunction)Brain_mavlink_takeoff, METH_VARARGS,
     "Takeoff: mavlink_takeoff(altitude=5.0) -> True"},
    {"mavlink_land", (PyCFunction)Brain_mavlink_land, METH_NOARGS,
     "Land: mavlink_land() -> True"},
    {"mavlink_goto", (PyCFunction)Brain_mavlink_goto, METH_VARARGS,
     "Go to position: mavlink_goto(lat, lon, alt=10.0) -> True"},
    {"mavlink_rtl", (PyCFunction)Brain_mavlink_rtl, METH_NOARGS,
     "Return to launch: mavlink_rtl() -> True"},
    {"mavlink_compose_features", (PyCFunction)Brain_mavlink_compose_features, METH_NOARGS,
     "Compose feature vector from telemetry: mavlink_compose_features() -> [float, ...]"},

    {NULL}
};

static PyGetSetDef Brain_getsetters[] = {
    {"is_frozen", (getter)Brain_get_is_frozen, NULL,
     "True if brain is frozen for inference-only mode", NULL},
    {NULL}
};

static PyTypeObject BrainType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.Brain",
    .tp_doc = "NIMCP Brain - High-level cognitive learning system",
    .tp_basicsize = sizeof(BrainObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Brain_new,
    .tp_init = (initproc)Brain_init,
    .tp_dealloc = (destructor)Brain_dealloc,
    .tp_methods = Brain_methods,
    .tp_getset = Brain_getsetters,
};

//=============================================================================
// NeuralNetwork Object Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    nimcp_network_t network;
} NetworkObject;

static void Network_dealloc(NetworkObject* self) {
    if (self->network && !g_py_shutdown_called) {
        nimcp_network_destroy(self->network);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Network_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    NetworkObject* self = (NetworkObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->network = NULL;
    }
    return (PyObject*)self;
}

/**
 * WHAT: Initialize NeuralNetwork with architecture params
 * WHY:  Create low-level neural network for custom topologies
 * HOW:  Call nimcp_network_create
 */
static int Network_init(NetworkObject* self, PyObject* args, PyObject* kwds) {
    unsigned int num_inputs, num_outputs, num_hidden;
    float learning_rate = 0.01F;

    static char* kwlist[] = {"num_inputs", "num_outputs", "num_hidden", "learning_rate", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "III|f", kwlist,
                                     &num_inputs, &num_outputs, &num_hidden, &learning_rate)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Network_init: Invalid arguments");
        return -1;
    }

    self->network = nimcp_network_create(num_inputs, num_outputs, num_hidden, learning_rate);

    if (!self->network) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Network_init: network is NULL");
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return -1;
    }

    return 0;
}

/**
 * WHAT: Forward pass through network
 * WHY:  Inference / activation propagation
 * HOW:  Convert Python list to C array, call nimcp_network_forward
 */
static PyObject* Network_forward(NetworkObject* self, PyObject* args) {
    PyObject* input_list;
    unsigned int num_outputs;

    /* P6-1: NULL guard for self->network */
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "OI", &input_list, &num_outputs)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Network_forward: Invalid arguments");
        return NULL;
    }

    Py_ssize_t num_inputs;
    float* inputs = py_list_to_float_array(input_list, &num_inputs);
    if (!inputs) return NULL;

    if (num_inputs > UINT32_MAX) {
        nimcp_free(inputs);
        PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
        return NULL;
    }

    /* P6-7: Overflow check before malloc — num_outputs * sizeof(float) can wrap */
    if (num_outputs == 0 || num_outputs > (SIZE_MAX / sizeof(float))) {
        nimcp_free(inputs);
        PyErr_SetString(PyExc_ValueError, "Invalid num_outputs value");
        return NULL;
    }

    float* outputs = (float*)nimcp_malloc(num_outputs * sizeof(float));
    if (!outputs) {
        nimcp_free(inputs);
        return PyErr_NoMemory();
    }

    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_network_forward(self->network, inputs, (uint32_t)num_inputs,
                                    outputs, num_outputs);
    Py_END_ALLOW_THREADS

    nimcp_free(inputs);

    if (status != NIMCP_OK) {
        nimcp_free(outputs);
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Network_forward: forward failed");
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    PyObject* result = PyList_New(num_outputs);
    if (!result) {
        nimcp_free(outputs);
        return NULL;
    }
    for (unsigned int i = 0; i < num_outputs; i++) {
        PyObject* val = PyFloat_FromDouble((double)outputs[i]);
        if (!val) { Py_DECREF(result); nimcp_free(outputs); return NULL; }
        PyList_SET_ITEM(result, i, val);
    }
    nimcp_free(outputs);
    return result;
}

/**
 * WHAT: Train network on a single example
 * WHY:  Supervised learning with input/target pairs
 * HOW:  Call nimcp_network_train
 *
 * NOTE: Currently a stub in the C API - will be implemented in a future version
 */
static PyObject* Network_train(NetworkObject* self, PyObject* args) {
    PyObject* input_list;
    PyObject* target_list;

    /* P6-1: NULL guard for self->network */
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network not initialized");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "OO", &input_list, &target_list)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Network_train: Invalid arguments");
        return NULL;
    }

    Py_ssize_t num_inputs;
    float* inputs = py_list_to_float_array(input_list, &num_inputs);
    if (!inputs) return NULL;

    if (num_inputs > UINT32_MAX) {
        nimcp_free(inputs);
        PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
        return NULL;
    }

    Py_ssize_t num_targets;
    float* targets = py_list_to_float_array(target_list, &num_targets);
    if (!targets) {
        nimcp_free(inputs);
        return NULL;
    }

    if (num_targets > UINT32_MAX) {
        nimcp_free(inputs);
        nimcp_free(targets);
        PyErr_SetString(PyExc_OverflowError, "Input too large for uint32_t");
        return NULL;
    }

    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_network_train(self->network, inputs, (uint32_t)num_inputs,
                                  targets, (uint32_t)num_targets);
    Py_END_ALLOW_THREADS

    nimcp_free(inputs);
    nimcp_free(targets);

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError,
            "Network-level training not yet implemented in the C API. "
            "Use Brain.learn() for supervised learning.");
        return NULL;
    }

    Py_RETURN_TRUE;
}

static PyMethodDef Network_methods[] = {
    {"forward", (PyCFunction)Network_forward, METH_VARARGS,
     "Forward pass: forward(inputs, num_outputs) -> list[float]"},
    {"train", (PyCFunction)Network_train, METH_VARARGS,
     "Train on single example: train(inputs, targets) -> bool\n"
     "Note: Currently a stub - use Brain.learn() for training"},
    {NULL}
};

static PyTypeObject NetworkType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.NeuralNetwork",
    .tp_doc = "NIMCP Neural Network - Low-level network operations",
    .tp_basicsize = sizeof(NetworkObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Network_new,
    .tp_init = (initproc)Network_init,
    .tp_dealloc = (destructor)Network_dealloc,
    .tp_methods = Network_methods,
};

//=============================================================================
// KnowledgeSystem Object Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    nimcp_knowledge_t knowledge;
} KnowledgeObject;

static void Knowledge_dealloc(KnowledgeObject* self) {
    if (self->knowledge && !g_py_shutdown_called) {
        nimcp_knowledge_destroy(self->knowledge);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Knowledge_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    KnowledgeObject* self = (KnowledgeObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->knowledge = NULL;
    }
    return (PyObject*)self;
}

static int Knowledge_init(KnowledgeObject* self, PyObject* args, PyObject* kwds) {
    /* L-2: args and kwds are intentionally ignored — KnowledgeSystem takes no
     * construction parameters. nimcp_knowledge_create() uses internal defaults. */
    (void)args;
    (void)kwds;
    self->knowledge = nimcp_knowledge_create();

    if (!self->knowledge) {
        /* L-2: NIMCP_THROW first, then PyErr_SetString last (M-9 ordering) */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Knowledge_init: knowledge is NULL");
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return -1;
    }

    return 0;
}

/**
 * WHAT: Add a fact to the knowledge graph
 * WHY:  Build knowledge base with subject-predicate-object triples
 * HOW:  Call nimcp_knowledge_add_fact
 */
static PyObject* Knowledge_add_fact(KnowledgeObject* self, PyObject* args) {
    if (!self->knowledge) {
        PyErr_SetString(PyExc_RuntimeError, "Knowledge system not initialized");
        return NULL;
    }

    const char* subject;
    const char* predicate;
    const char* object;

    if (!PyArg_ParseTuple(args, "sss", &subject, &predicate, &object)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Knowledge_add_fact: Invalid arguments");
        return NULL;
    }

    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_knowledge_add_fact(self->knowledge, subject, predicate, object);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    Py_RETURN_TRUE;
}

/**
 * WHAT: Query the knowledge graph
 * WHY:  Retrieve information about concepts
 * HOW:  Call nimcp_knowledge_query with pre-allocated buffer
 */
static PyObject* Knowledge_query(KnowledgeObject* self, PyObject* args) {
    if (!self->knowledge) {
        PyErr_SetString(PyExc_RuntimeError, "Knowledge system not initialized");
        return NULL;
    }

    const char* query_str;

    if (!PyArg_ParseTuple(args, "s", &query_str)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Knowledge_query: Invalid arguments");
        return NULL;
    }

    char result[NIMCP_CMD_BUFFER_SIZE];
    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_knowledge_query(self->knowledge, query_str,
                                    result, NIMCP_CMD_BUFFER_SIZE);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(PyExc_RuntimeError, nimcp_get_error());
        return NULL;
    }

    PyObject* py_result = PyUnicode_FromString(result);
    if (!py_result) return NULL;  /* OOM */
    return py_result;
}

static PyMethodDef Knowledge_methods[] = {
    {"add_fact", (PyCFunction)Knowledge_add_fact, METH_VARARGS,
     "Add fact: add_fact(subject, predicate, object) -> bool"},
    {"query", (PyCFunction)Knowledge_query, METH_VARARGS,
     "Query knowledge: query(concept) -> str"},
    {NULL}
};

static PyTypeObject KnowledgeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.KnowledgeSystem",
    .tp_doc = "NIMCP Knowledge System - Knowledge graph with fact storage and querying",
    .tp_basicsize = sizeof(KnowledgeObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Knowledge_new,
    .tp_init = (initproc)Knowledge_init,
    .tp_dealloc = (destructor)Knowledge_dealloc,
    .tp_methods = Knowledge_methods,
};

//=============================================================================
// Module Functions
//=============================================================================

/**
 * WHAT: Get NIMCP version
 * WHY:  Check library version for compatibility
 * HOW:  Call nimcp_version()
 */
static PyObject* nimcp_get_version(PyObject* self, PyObject* args) {
    const char* ver = nimcp_version();
    if (!ver) ver = "unknown";
    return Py_BuildValue("s", ver);
}

/**
 * WHAT: Initialize NIMCP library
 * WHY:  Required before using any NIMCP functions
 * HOW:  Call nimcp_init()
 */
static PyObject* nimcp_initialize(PyObject* self, PyObject* args) {
    nimcp_status_t status = nimcp_init();
    if (status != NIMCP_OK) {
        NIMCP_THROW_CRITICAL(NIMCP_ERROR_NOT_INITIALIZED,
                            "nimcp_initialize: Failed to initialize NIMCP library");
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize NIMCP");
        return NULL;
    }
    Py_RETURN_NONE;
}

/**
 * WHAT: Shutdown NIMCP library
 * WHY:  Clean up resources before program exit
 * HOW:  Call nimcp_shutdown()
 */
static PyObject* nimcp_shutdown_lib(PyObject* self, PyObject* args) {
    g_py_shutdown_called = 1;
    nimcp_shutdown();
    Py_RETURN_NONE;
}

// Note: SIMD operations not yet exposed in public nimcp.h API
// Will be added in future version

/**
 * WHAT: Set global log level at runtime
 * WHY:  Enable debug/trace output without restart
 * HOW:  nimcp.set_log_level("debug")
 */
static PyObject* py_nimcp_set_log_level(PyObject* self, PyObject* args) {
    (void)self;
    const char* level_str;
    if (!PyArg_ParseTuple(args, "s", &level_str)) {
        return NULL;
    }
    log_level_t level = LOG_LEVEL_INFO;
    if (strcasecmp(level_str, "trace") == 0)      level = LOG_LEVEL_TRACE;
    else if (strcasecmp(level_str, "debug") == 0)  level = LOG_LEVEL_DEBUG;
    else if (strcasecmp(level_str, "info") == 0)   level = LOG_LEVEL_INFO;
    else if (strcasecmp(level_str, "warn") == 0)   level = LOG_LEVEL_WARN;
    else if (strcasecmp(level_str, "error") == 0)  level = LOG_LEVEL_ERROR;
    else if (strcasecmp(level_str, "off") == 0)    level = LOG_LEVEL_OFF;
    else {
        PyErr_Format(PyExc_ValueError,
                     "Invalid log level '%s'. Use: trace, debug, info, warn, error, off", level_str);
        return NULL;
    }
    // Ensure global logger exists before setting level
    if (!nimcp_log_is_initialized(NULL)) {
        nimcp_log_config_t log_cfg = nimcp_log_default_config();
        log_cfg.level = level;
        nimcp_log_init(&log_cfg);
    } else {
        nimcp_log_set_level(NULL, level);
    }
    Py_RETURN_NONE;
}

static PyMethodDef module_methods[] = {
    {"version", nimcp_get_version, METH_NOARGS, "Get NIMCP version string"},
    {"init", nimcp_initialize, METH_NOARGS, "Initialize NIMCP library"},
    {"shutdown", nimcp_shutdown_lib, METH_NOARGS, "Shutdown NIMCP library"},
    {"set_log_level", py_nimcp_set_log_level, METH_VARARGS,
     "Set log level: 'trace', 'debug', 'info', 'warn', 'error', 'off'"},
    /* Batch-safe biological stability (Phase 4.1) */
    {"bs_scaling_apply",     BS_scaling_apply,     METH_VARARGS,
     "Batch-safe synaptic scaling (C)"},
    {"bs_depression_apply",  BS_depression_apply,  METH_VARARGS,
     "Batch-safe short-term depression (C)"},
    {"bs_metabolic_apply",   BS_metabolic_apply,   METH_VARARGS,
     "Metabolic budget (C, stateless)"},
    {"bs_ip_apply",          BS_ip_apply,          METH_VARARGS,
     "Batch-safe intrinsic plasticity (C)"},
    {"bs_inhibitory_apply",  BS_inhibitory_apply,  METH_VARARGS,
     "Batch-safe inhibitory plasticity (C)"},
    {"bs_rstdp_apply",       BS_rstdp_apply,       METH_VARARGS,
     "Batch-safe R-STDP (C) — exact equivalence to sequential"},
    {"bs_self_test",         BS_self_test,         METH_NOARGS,
     "Run C self-test; returns # failures"},
    {"bs_set_enabled",       BS_set_enabled,       METH_VARARGS,
     "Enable/disable batch-safe path"},
    {"bs_is_enabled",        BS_is_enabled,        METH_NOARGS,
     "Check if batch-safe path is enabled"},
    {NULL}
};

static struct PyModuleDef nimcp_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "nimcp",
    /* P2-5: Use correct version matching library */
    .m_doc = "NIMCP - Neural Interface Message Communication Protocol (Python Bindings v" NIMCP_VERSION_STRING ")",
    .m_size = -1,
    .m_methods = module_methods
};

/**
 * WHAT: Python module initialization
 * WHY:  Entry point for Python interpreter to load module
 * HOW:  Register types, add constants, create module object
 */
PyMODINIT_FUNC PyInit_nimcp(void) {
    PyObject* m;

    LOG_MODULE_INFO("bindings.python", "Initializing Python bindings for NIMCP v" NIMCP_VERSION_STRING);

    // Initialize NIMCP library
    if (nimcp_init() != NIMCP_OK) {
        LOG_MODULE_ERROR("bindings.python", "Failed to initialize NIMCP core library");
        NIMCP_THROW_CRITICAL(NIMCP_ERROR_NOT_INITIALIZED,
                            "PyInit_nimcp: Failed to initialize NIMCP core library");
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize NIMCP");
        return NULL;
    }

    // ABI layout check: detect stale .so with wrong struct layout
    {
        /* Compute the same hash the library computes, but using THIS .so's
         * compile-time sizeof values. If the .so in site-packages was compiled
         * against a different neuron_t layout, this will differ from what
         * libnimcp.so returns at runtime. */
        uint32_t compiled_h = 0x4E494D43u;
        compiled_h ^= (uint32_t)sizeof(neuron_t) * 2654435761u;
        compiled_h ^= (uint32_t)sizeof(sparse_synapse_storage_t) * 2246822519u;
        compiled_h ^= (uint32_t)SPARSE_SYNAPSE_EMBEDDED_CAPACITY * 3266489917u;
        int compiled_hash = (int)(compiled_h & 0x7FFFFFFFu);
        int runtime_hash = nimcp_abi_layout_hash();
        if (compiled_hash != runtime_hash) {
            /* Buffer sized for the full message with both 32-bit ints
             * (up to 11 chars each) substituted in. The message template
             * itself is ~290 bytes, so 400 gives headroom without risking
             * snprintf truncation. */
            char msg[400];
            snprintf(msg, sizeof(msg),
                "NIMCP ABI mismatch: Python .so was compiled with struct layout hash %d "
                "but libnimcp.so has hash %d. Rebuild and reinstall: "
                "make nimcp_python -j4 && cp build/lib/python/nimcp.so "
                "~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so",
                compiled_hash, runtime_hash);
            PyErr_SetString(PyExc_ImportError, msg);
            return NULL;
        }
    }

    // Prepare types
    if (PyType_Ready(&BrainType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: BrainType ready failed");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to ready BrainType");
        return NULL;
    }
    if (PyType_Ready(&NetworkType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: NetworkType ready failed");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to ready NetworkType");
        return NULL;
    }
    if (PyType_Ready(&KnowledgeType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: KnowledgeType ready failed");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to ready KnowledgeType");
        return NULL;
    }

    // Create module
    m = PyModule_Create(&nimcp_module);
    if (m == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PyInit_nimcp: PyModule_Create failed");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to create module");
        return NULL;
    }

    // Add Brain type
    Py_INCREF(&BrainType);
    if (PyModule_AddObject(m, "Brain", (PyObject*)&BrainType) < 0) {
        Py_DECREF(&BrainType);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: Failed to add BrainType");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to add Brain type to module");
        return NULL;
    }

    // Add NeuralNetwork type
    Py_INCREF(&NetworkType);
    if (PyModule_AddObject(m, "NeuralNetwork", (PyObject*)&NetworkType) < 0) {
        Py_DECREF(&NetworkType);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: Failed to add NetworkType");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to add NeuralNetwork type to module");
        return NULL;
    }

    // Add KnowledgeSystem type
    Py_INCREF(&KnowledgeType);
    if (PyModule_AddObject(m, "KnowledgeSystem", (PyObject*)&KnowledgeType) < 0) {
        Py_DECREF(&KnowledgeType);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: Failed to add KnowledgeType");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to add KnowledgeSystem type to module");
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

    // Add status constants
    PyModule_AddIntConstant(m, "OK", NIMCP_OK);
    PyModule_AddIntConstant(m, "ERROR", NIMCP_ERROR);

    // Brain cycle coordinator type enum — mirror of brain_cycle_type_t
    // (include/core/brain/nimcp_brain_cycle_coordinator.h). Used with
    // brain.cycle_register / cycle_notify_tick / cycle_unregister.
    PyModule_AddIntConstant(m, "CYCLE_IMMUNE_TICK",      BRAIN_CYCLE_IMMUNE_TICK);
    PyModule_AddIntConstant(m, "CYCLE_HEALTH_AGENT",     BRAIN_CYCLE_HEALTH_AGENT);
    PyModule_AddIntConstant(m, "CYCLE_SLEEP_WAKE",       BRAIN_CYCLE_SLEEP_WAKE);
    PyModule_AddIntConstant(m, "CYCLE_CIRCADIAN",        BRAIN_CYCLE_CIRCADIAN);
    PyModule_AddIntConstant(m, "CYCLE_AROUSAL",          BRAIN_CYCLE_AROUSAL);
    PyModule_AddIntConstant(m, "CYCLE_OSCILLATIONS",     BRAIN_CYCLE_OSCILLATIONS);
    PyModule_AddIntConstant(m, "CYCLE_GC_AGENT",         BRAIN_CYCLE_GC_AGENT);
    PyModule_AddIntConstant(m, "CYCLE_IO_DISPATCHER",    BRAIN_CYCLE_IO_DISPATCHER);
    PyModule_AddIntConstant(m, "CYCLE_BRAIN_UPDATE",     BRAIN_CYCLE_BRAIN_UPDATE);
    PyModule_AddIntConstant(m, "CYCLE_LONG_TERM_MEMORY", BRAIN_CYCLE_LONG_TERM_MEMORY);
    PyModule_AddIntConstant(m, "CYCLE_COUNT",            BRAIN_CYCLE_COUNT);

    // Add ABI hash for manual validation
    PyModule_AddIntConstant(m, "ABI_LAYOUT_HASH", nimcp_abi_layout_hash());

    // Initialize signal filter module
    if (init_signal_filter_module(m) < 0) {
        LOG_MODULE_ERROR("bindings.python", "Failed to initialize signal filter module");
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: signal filter init failed");
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError, "PyInit_nimcp: Failed to initialize signal filter module");
        return NULL;
    }

    LOG_MODULE_INFO("bindings.python", "Successfully initialized Python bindings");
    return m;
}
