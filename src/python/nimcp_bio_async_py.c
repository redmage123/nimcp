/**
 * @file nimcp_bio_async_py.c
 * @brief Python bindings for NIMCP Bio-Async System
 *
 * Provides Python types for biologically-inspired async:
 * - BioPromise: Neuromodulator-based promise
 * - BioFuture: Future with confidence decay
 * - PhaseSync: Phase synchronization coordinator
 * - PredictiveModel: Predictive coding model
 * - GlialWave: Glial signaling wave
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "async/nimcp_bio_async.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bio_async_py)

/* ============================================================================
 * BioPromise Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_bio_promise_t promise;
    size_t result_size;
    int channel;
} BioPromiseObject;

static void BioPromise_dealloc(BioPromiseObject* self)
{
    if (self->promise != NULL) {
        nimcp_bio_promise_destroy(self->promise);
        self->promise = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* BioPromise_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    BioPromiseObject* self = (BioPromiseObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->promise = NULL;
        self->result_size = 0;
        self->channel = BIO_CHANNEL_DOPAMINE;
    }
    return (PyObject*)self;
}

static int BioPromise_init(BioPromiseObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"channel", "result_size", NULL};
    int channel = BIO_CHANNEL_DOPAMINE;
    Py_ssize_t result_size = 0;  /* 0 = use sized completion */

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|in", kwlist, &channel, &result_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BioPromise_init: PyArg_ParseTupleAndKeywords is NULL");
        return -1;
    }

    if (channel < 0 || channel >= BIO_CHANNEL_COUNT) {
        PyErr_SetString(PyExc_ValueError, "Invalid channel type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "BioPromise_init: capacity exceeded");
        return -1;
    }

    if (result_size < 0) {
        PyErr_SetString(PyExc_ValueError, "result_size must be non-negative");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BioPromise_init: validation failed");
        return -1;
    }

    self->channel = channel;
    self->result_size = (size_t)result_size;

    self->promise = nimcp_bio_promise_create((nimcp_bio_channel_type_t)channel, (size_t)result_size);
    if (self->promise == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create bio-promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BioPromise_init: validation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief BioPromise.complete(result_bytes) -> bool
 *
 * Complete the promise with result data.
 */
static PyObject* BioPromise_complete(BioPromiseObject* self, PyObject* args)
{
    Py_buffer buffer;

    if (!PyArg_ParseTuple(args, "y*", &buffer)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioPromise_complete: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_error_t status;

    if (self->result_size > 0) {
        /* Fixed size completion */
        if ((size_t)buffer.len != self->result_size) {
            PyBuffer_Release(&buffer);
            PyErr_Format(PyExc_ValueError, "Expected %zu bytes, got %zd",
                        self->result_size, buffer.len);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BioPromise_complete: validation failed");
            return NULL;
        }
        Py_BEGIN_ALLOW_THREADS
        status = nimcp_bio_promise_complete(self->promise, buffer.buf);
        Py_END_ALLOW_THREADS
    } else {
        /* Variable size completion */
        Py_BEGIN_ALLOW_THREADS
        status = nimcp_bio_promise_complete_sized(self->promise, buffer.buf, (size_t)buffer.len);
        Py_END_ALLOW_THREADS
    }

    PyBuffer_Release(&buffer);

    if (status != NIMCP_SUCCESS) {
        if (status == NIMCP_BIO_ERROR_REFRACTORY) {
            PyErr_SetString(PyExc_RuntimeError, "Channel in refractory period");
        } else if (status == NIMCP_BIO_ERROR_CHANNEL_SATURATED) {
            PyErr_SetString(PyExc_RuntimeError, "Channel saturated");
        } else {
            PyErr_SetString(PyExc_RuntimeError, "Failed to complete promise");
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioPromise_complete: validation failed");
        return NULL;
    }

    Py_RETURN_TRUE;
}

/**
 * @brief BioPromise.fail(error_code) -> bool
 *
 * Fail the promise with an error.
 */
static PyObject* BioPromise_fail(BioPromiseObject* self, PyObject* args)
{
    int error_code;

    if (!PyArg_ParseTuple(args, "i", &error_code)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioPromise_fail: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_bio_promise_fail(self->promise, error_code);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_SUCCESS) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to fail promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioPromise_fail: validation failed");
        return NULL;
    }

    Py_RETURN_TRUE;
}

/* Forward declaration for BioFuture */
static PyTypeObject BioFutureType;

/**
 * @brief BioPromise.get_future() -> BioFuture
 *
 * Get the future associated with this promise.
 */
static PyObject* BioPromise_get_future(BioPromiseObject* self, PyObject* Py_UNUSED(args))
{
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(self->promise);
    if (future == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioPromise_get_future: validation failed");
        return NULL;
    }

    /* Create Python wrapper - note: the C future is owned by the promise */
    PyObject* future_obj = PyObject_CallObject((PyObject*)&BioFutureType, NULL);
    if (future_obj == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "future_obj is NULL");

        return NULL;
    }

    /* Store the future handle - we don't own it, the promise does */
    ((struct {
        PyObject_HEAD
        nimcp_bio_future_t future;
        bool owns_future;
    }*)future_obj)->future = future;
    ((struct {
        PyObject_HEAD
        nimcp_bio_future_t future;
        bool owns_future;
    }*)future_obj)->owns_future = false;

    return future_obj;
}

static PyObject* BioPromise_repr(BioPromiseObject* self)
{
    return PyUnicode_FromFormat("BioPromise(channel=%d, result_size=%zu)",
                                self->channel, self->result_size);
}

static PyMethodDef BioPromise_methods[] = {
    {"complete", (PyCFunction)BioPromise_complete, METH_VARARGS,
     "Complete promise with result bytes"},
    {"fail", (PyCFunction)BioPromise_fail, METH_VARARGS,
     "Fail promise with error code"},
    {"get_future", (PyCFunction)BioPromise_get_future, METH_NOARGS,
     "Get the future for this promise"},
    {NULL}
};

static PyTypeObject BioPromiseType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.BioPromise",
    .tp_doc = PyDoc_STR(
        "Biologically-inspired promise with neuromodulator semantics.\n\n"
        "Args:\n"
        "    channel (int): Neuromodulator channel (BIO_CHANNEL_*)\n"
        "    result_size (int): Size of result (0 for variable)\n\n"
        "Channel semantics:\n"
        "    DOPAMINE: Fast completion, medium decay\n"
        "    SEROTONIN: Slow state change, long decay\n"
        "    NOREPINEPHRINE: Alerting, priority\n"
        "    ACETYLCHOLINE: Fast attention switching"
    ),
    .tp_basicsize = sizeof(BioPromiseObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = BioPromise_new,
    .tp_init = (initproc)BioPromise_init,
    .tp_dealloc = (destructor)BioPromise_dealloc,
    .tp_repr = (reprfunc)BioPromise_repr,
    .tp_methods = BioPromise_methods,
};

/* ============================================================================
 * BioFuture Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_bio_future_t future;
    bool owns_future;
} BioFutureObject;

static void BioFuture_dealloc(BioFutureObject* self)
{
    if (self->future != NULL && self->owns_future) {
        nimcp_bio_future_destroy(self->future);
        self->future = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* BioFuture_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    (void)args;
    (void)kwds;
    BioFutureObject* self = (BioFutureObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->future = NULL;
        self->owns_future = false;
    }
    return (PyObject*)self;
}

/**
 * @brief BioFuture.wait(timeout_ms=0) -> bytes or None
 *
 * Wait for future completion with biological dynamics.
 */
static PyObject* BioFuture_wait(BioFutureObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"timeout_ms", NULL};
    unsigned long long timeout_ms = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|K", kwlist, &timeout_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioFuture_wait: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    if (self->future == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Future not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioFuture_wait: validation failed");
        return NULL;
    }

    /* Allocate buffer for result - we use a reasonable default */
    size_t buffer_size = 4096;
    void* buffer = nimcp_malloc(buffer_size);
    if (buffer == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "BioFuture_wait: validation failed");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_bio_future_wait(self->future, buffer, timeout_ms);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_SUCCESS) {
        nimcp_free(buffer);
        if (status == NIMCP_BIO_ERROR_DECAY_COMPLETE) {
            PyErr_SetString(PyExc_TimeoutError, "Result decayed");
        } else {
            PyErr_SetString(PyExc_RuntimeError, "Wait failed");
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BioFuture_wait: validation failed");
        return NULL;
    }

    /* Return as bytes */
    PyObject* result = PyBytes_FromStringAndSize(buffer, (Py_ssize_t)buffer_size);
    nimcp_free(buffer);
    return result;
}

/**
 * @brief BioFuture.get_confidence() -> float
 *
 * Get current confidence level (decays over time).
 */
static PyObject* BioFuture_get_confidence(BioFutureObject* self, PyObject* Py_UNUSED(args))
{
    if (self->future == NULL) {
        Py_RETURN_NONE;
    }

    float confidence = nimcp_bio_future_get_confidence(self->future);
    return PyFloat_FromDouble((double)confidence);
}

/**
 * @brief BioFuture.is_ready() -> bool
 *
 * Check if future is ready (non-blocking).
 */
static PyObject* BioFuture_is_ready(BioFutureObject* self, PyObject* Py_UNUSED(args))
{
    if (self->future == NULL) {
        Py_RETURN_FALSE;
    }

    bool ready = nimcp_bio_future_is_ready(self->future);
    return PyBool_FromLong(ready);
}

/**
 * @brief BioFuture.state -> int
 *
 * Get current future state.
 */
static PyObject* BioFuture_get_state(BioFutureObject* self, void* closure)
{
    (void)closure;
    if (self->future == NULL) {
        return PyLong_FromLong(BIO_FUTURE_PENDING);
    }
    return PyLong_FromLong(nimcp_bio_future_state(self->future));
}

/**
 * @brief BioFuture.get_age_ms() -> float
 *
 * Get time since completion in ms.
 */
static PyObject* BioFuture_get_age_ms(BioFutureObject* self, PyObject* Py_UNUSED(args))
{
    if (self->future == NULL) {
        return PyFloat_FromDouble(-1.0);
    }

    float age = nimcp_bio_future_get_age_ms(self->future);
    return PyFloat_FromDouble((double)age);
}

/**
 * @brief BioFuture.cancel() -> bool
 *
 * Cancel the future.
 */
static PyObject* BioFuture_cancel(BioFutureObject* self, PyObject* Py_UNUSED(args))
{
    if (self->future == NULL) {
        Py_RETURN_FALSE;
    }

    bool cancelled = nimcp_bio_future_cancel(self->future);
    return PyBool_FromLong(cancelled);
}

static PyObject* BioFuture_repr(BioFutureObject* self)
{
    if (self->future == NULL) {
        return PyUnicode_FromString("BioFuture(uninitialized)");
    }
    const char* state_name = nimcp_bio_future_state_name(nimcp_bio_future_state(self->future));
    float confidence = nimcp_bio_future_get_confidence(self->future);
    return PyUnicode_FromFormat("BioFuture(state=%s, confidence=%.2f)", state_name, confidence);
}

static PyMethodDef BioFuture_methods[] = {
    {"wait", (PyCFunction)BioFuture_wait, METH_VARARGS | METH_KEYWORDS,
     "Wait for completion (timeout_ms=0 means wait for decay)"},
    {"get_confidence", (PyCFunction)BioFuture_get_confidence, METH_NOARGS,
     "Get current confidence level (0.0-1.0, decays over time)"},
    {"is_ready", (PyCFunction)BioFuture_is_ready, METH_NOARGS,
     "Check if future is ready (non-blocking)"},
    {"get_age_ms", (PyCFunction)BioFuture_get_age_ms, METH_NOARGS,
     "Get time since completion in milliseconds"},
    {"cancel", (PyCFunction)BioFuture_cancel, METH_NOARGS,
     "Cancel the future"},
    {NULL}
};

static PyGetSetDef BioFuture_getset[] = {
    {"state", (getter)BioFuture_get_state, NULL,
     "Current future state (BIO_FUTURE_*)", NULL},
    {NULL}
};

static PyTypeObject BioFutureType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.BioFuture",
    .tp_doc = PyDoc_STR(
        "Biologically-inspired future with confidence decay.\n\n"
        "The confidence level decays over time following\n"
        "neuromodulator dynamics.\n\n"
        "Properties:\n"
        "    state (int): Current state (BIO_FUTURE_*)"
    ),
    .tp_basicsize = sizeof(BioFutureObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = BioFuture_new,
    .tp_dealloc = (destructor)BioFuture_dealloc,
    .tp_repr = (reprfunc)BioFuture_repr,
    .tp_methods = BioFuture_methods,
    .tp_getset = BioFuture_getset,
};

/* ============================================================================
 * PhaseSync Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_phase_sync_t sync;
    int band;
} PhaseSyncObject;

static void PhaseSync_dealloc(PhaseSyncObject* self)
{
    if (self->sync != NULL) {
        nimcp_phase_sync_destroy(self->sync);
        self->sync = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PhaseSync_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    PhaseSyncObject* self = (PhaseSyncObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->sync = NULL;
        self->band = BIO_OSC_GAMMA;
    }
    return (PyObject*)self;
}

static int PhaseSync_init(PhaseSyncObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"band", NULL};
    int band = BIO_OSC_GAMMA;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &band)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PhaseSync_init: PyArg_ParseTupleAndKeywords is NULL");
        return -1;
    }

    if (band < 0 || band >= BIO_OSC_BAND_COUNT) {
        PyErr_SetString(PyExc_ValueError, "Invalid oscillation band");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "PhaseSync_init: capacity exceeded");
        return -1;
    }

    self->band = band;
    self->sync = nimcp_phase_sync_create((nimcp_oscillation_band_t)band);

    if (self->sync == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create phase sync");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PhaseSync_init: validation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief PhaseSync.add_future(future) -> bool
 *
 * Add a bio-future to the sync group.
 */
static PyObject* PhaseSync_add_future(PhaseSyncObject* self, PyObject* args)
{
    PyObject* future_obj;

    if (!PyArg_ParseTuple(args, "O!", &BioFutureType, &future_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PhaseSync_add_future: PyArg_ParseTuple is NULL");
        return NULL;
    }

    BioFutureObject* future = (BioFutureObject*)future_obj;
    if (future->future == NULL) {
        PyErr_SetString(PyExc_ValueError, "Future not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PhaseSync_add_future: validation failed");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_phase_sync_add_future(self->sync, future->future);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_SUCCESS) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to add future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PhaseSync_add_future: validation failed");
        return NULL;
    }

    Py_RETURN_TRUE;
}

/**
 * @brief PhaseSync.wait_all(timeout_ms=0) -> bool
 *
 * Wait for all futures to reach phase coherence.
 */
static PyObject* PhaseSync_wait_all(PhaseSyncObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"timeout_ms", NULL};
    unsigned long long timeout_ms = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|K", kwlist, &timeout_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PhaseSync_wait_all: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_phase_sync_wait_all(self->sync, timeout_ms);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_SUCCESS);
}

/**
 * @brief PhaseSync.wait_coherent(threshold, timeout_ms=0) -> bool
 *
 * Wait for specified coherence level.
 */
static PyObject* PhaseSync_wait_coherent(PhaseSyncObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"threshold", "timeout_ms", NULL};
    float threshold = 0.8f;
    unsigned long long timeout_ms = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|fK", kwlist, &threshold, &timeout_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PhaseSync_wait_coherent: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    if (threshold < 0.0f || threshold > 1.0f) {
        PyErr_SetString(PyExc_ValueError, "threshold must be 0.0-1.0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PhaseSync_wait_coherent: validation failed");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_phase_sync_wait_coherent(self->sync, threshold, timeout_ms);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_SUCCESS);
}

/**
 * @brief PhaseSync.get_coherence() -> float
 *
 * Get current phase coherence (order parameter).
 */
static PyObject* PhaseSync_get_coherence(PhaseSyncObject* self, PyObject* Py_UNUSED(args))
{
    float coherence = nimcp_phase_sync_get_coherence(self->sync);
    return PyFloat_FromDouble((double)coherence);
}

/**
 * @brief PhaseSync.get_mean_phase() -> float
 *
 * Get mean phase in radians.
 */
static PyObject* PhaseSync_get_mean_phase(PhaseSyncObject* self, PyObject* Py_UNUSED(args))
{
    float phase = nimcp_phase_sync_get_mean_phase(self->sync);
    return PyFloat_FromDouble((double)phase);
}

/**
 * @brief PhaseSync.count -> int
 *
 * Get number of futures in sync group.
 */
static PyObject* PhaseSync_get_count(PhaseSyncObject* self, void* closure)
{
    (void)closure;
    size_t count = nimcp_phase_sync_get_count(self->sync);
    return PyLong_FromSize_t(count);
}

static PyObject* PhaseSync_repr(PhaseSyncObject* self)
{
    const char* band_name = nimcp_oscillation_band_name((nimcp_oscillation_band_t)self->band);
    size_t count = nimcp_phase_sync_get_count(self->sync);
    float coherence = nimcp_phase_sync_get_coherence(self->sync);
    return PyUnicode_FromFormat("PhaseSync(band=%s, count=%zu, coherence=%.2f)",
                                band_name, count, coherence);
}

static PyMethodDef PhaseSync_methods[] = {
    {"add_future", (PyCFunction)PhaseSync_add_future, METH_VARARGS,
     "Add a bio-future to the sync group"},
    {"wait_all", (PyCFunction)PhaseSync_wait_all, METH_VARARGS | METH_KEYWORDS,
     "Wait for all futures to synchronize"},
    {"wait_coherent", (PyCFunction)PhaseSync_wait_coherent, METH_VARARGS | METH_KEYWORDS,
     "Wait for specified coherence level (0.0-1.0)"},
    {"get_coherence", (PyCFunction)PhaseSync_get_coherence, METH_NOARGS,
     "Get current phase coherence (order parameter)"},
    {"get_mean_phase", (PyCFunction)PhaseSync_get_mean_phase, METH_NOARGS,
     "Get mean phase in radians [0, 2pi]"},
    {NULL}
};

static PyGetSetDef PhaseSync_getset[] = {
    {"count", (getter)PhaseSync_get_count, NULL,
     "Number of futures in sync group", NULL},
    {NULL}
};

static PyTypeObject PhaseSyncType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.PhaseSync",
    .tp_doc = PyDoc_STR(
        "Phase synchronization coordinator using Kuramoto oscillators.\n\n"
        "Args:\n"
        "    band (int): Oscillation band (BIO_OSC_*)\n\n"
        "Band selection:\n"
        "    DELTA: Slow sync, tolerates long delays\n"
        "    THETA: Memory/sequence coordination\n"
        "    ALPHA: Attention-gated sync\n"
        "    BETA: Working memory coordination\n"
        "    GAMMA: Fast binding, tight sync required"
    ),
    .tp_basicsize = sizeof(PhaseSyncObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PhaseSync_new,
    .tp_init = (initproc)PhaseSync_init,
    .tp_dealloc = (destructor)PhaseSync_dealloc,
    .tp_repr = (reprfunc)PhaseSync_repr,
    .tp_methods = PhaseSync_methods,
    .tp_getset = PhaseSync_getset,
};

/* ============================================================================
 * PredictiveModel Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_predictive_model_t model;
    char* signal_name;
} PredictiveModelObject;

static void PredictiveModel_dealloc(PredictiveModelObject* self)
{
    if (self->model != NULL) {
        nimcp_predictive_destroy(self->model);
        self->model = NULL;
    }
    if (self->signal_name) {
        nimcp_free(self->signal_name);
        self->signal_name = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PredictiveModel_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    PredictiveModelObject* self = (PredictiveModelObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->model = NULL;
        self->signal_name = NULL;
    }
    return (PyObject*)self;
}

static int PredictiveModel_init(PredictiveModelObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"signal_name", "initial_prediction", "initial_precision", NULL};
    const char* signal_name;
    float initial_prediction = 0.0f;
    float initial_precision = 1.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|ff", kwlist,
                                      &signal_name, &initial_prediction, &initial_precision)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PredictiveModel_init: operation failed");
        return -1;
    }

    self->signal_name = strdup(signal_name);
    if (self->signal_name == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PredictiveModel_init: validation failed");
        return -1;
    }

    self->model = nimcp_predictive_create(signal_name, initial_prediction, initial_precision);
    if (self->model == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create predictive model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PredictiveModel_init: validation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief PredictiveModel.observe(value) -> float
 *
 * Update model with observed value, returns surprise.
 */
static PyObject* PredictiveModel_observe(PredictiveModelObject* self, PyObject* args)
{
    float value;

    if (!PyArg_ParseTuple(args, "f", &value)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PredictiveModel_observe: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_predictive_observe(self->model, value);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_SUCCESS) {
        PyErr_SetString(PyExc_RuntimeError, "Observation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PredictiveModel_observe: validation failed");
        return NULL;
    }

    /* Return the surprise value */
    float surprise = nimcp_predictive_get_last_surprise(self->model);
    return PyFloat_FromDouble((double)surprise);
}

/**
 * @brief PredictiveModel.set_prediction(prediction, precision=0) -> bool
 *
 * Manually set prediction.
 */
static PyObject* PredictiveModel_set_prediction(PredictiveModelObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"prediction", "precision", NULL};
    float prediction;
    float precision = 0.0f;  /* 0 = keep current */

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "f|f", kwlist, &prediction, &precision)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PredictiveModel_set_prediction: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_predictive_set_prediction(self->model, prediction, precision);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_SUCCESS);
}

/**
 * @brief PredictiveModel.prediction -> float
 */
static PyObject* PredictiveModel_get_prediction(PredictiveModelObject* self, void* closure)
{
    (void)closure;
    float pred = nimcp_predictive_get_prediction(self->model);
    return PyFloat_FromDouble((double)pred);
}

/**
 * @brief PredictiveModel.precision -> float
 */
static PyObject* PredictiveModel_get_precision(PredictiveModelObject* self, void* closure)
{
    (void)closure;
    float prec = nimcp_predictive_get_precision(self->model);
    return PyFloat_FromDouble((double)prec);
}

/**
 * @brief PredictiveModel.last_surprise -> float
 */
static PyObject* PredictiveModel_get_last_surprise(PredictiveModelObject* self, void* closure)
{
    (void)closure;
    float surp = nimcp_predictive_get_last_surprise(self->model);
    return PyFloat_FromDouble((double)surp);
}

static PyObject* PredictiveModel_repr(PredictiveModelObject* self)
{
    float pred = nimcp_predictive_get_prediction(self->model);
    float prec = nimcp_predictive_get_precision(self->model);
    return PyUnicode_FromFormat("PredictiveModel('%s', prediction=%.2f, precision=%.2f)",
                                self->signal_name, pred, prec);
}

static PyMethodDef PredictiveModel_methods[] = {
    {"observe", (PyCFunction)PredictiveModel_observe, METH_VARARGS,
     "Update model with observed value, returns surprise"},
    {"set_prediction", (PyCFunction)PredictiveModel_set_prediction, METH_VARARGS | METH_KEYWORDS,
     "Manually set prediction (precision=0 keeps current)"},
    {NULL}
};

static PyGetSetDef PredictiveModel_getset[] = {
    {"prediction", (getter)PredictiveModel_get_prediction, NULL,
     "Current prediction", NULL},
    {"precision", (getter)PredictiveModel_get_precision, NULL,
     "Current precision (inverse variance)", NULL},
    {"last_surprise", (getter)PredictiveModel_get_last_surprise, NULL,
     "Last calculated surprise value", NULL},
    {NULL}
};

static PyTypeObject PredictiveModelType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.PredictiveModel",
    .tp_doc = PyDoc_STR(
        "Predictive coding model with Bayesian updates.\n\n"
        "Args:\n"
        "    signal_name (str): Unique name for the signal\n"
        "    initial_prediction (float): Initial expected value\n"
        "    initial_precision (float): Initial certainty\n\n"
        "Properties:\n"
        "    prediction (float): Current prediction\n"
        "    precision (float): Current precision\n"
        "    last_surprise (float): Last surprise value"
    ),
    .tp_basicsize = sizeof(PredictiveModelObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PredictiveModel_new,
    .tp_init = (initproc)PredictiveModel_init,
    .tp_dealloc = (destructor)PredictiveModel_dealloc,
    .tp_repr = (reprfunc)PredictiveModel_repr,
    .tp_methods = PredictiveModel_methods,
    .tp_getset = PredictiveModel_getset,
};

/* ============================================================================
 * GlialWave Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_glial_wave_t wave;
    uint32_t source_region;
} GlialWaveObject;

static void GlialWave_dealloc(GlialWaveObject* self)
{
    if (self->wave != NULL) {
        nimcp_glial_wave_destroy(self->wave);
        self->wave = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* GlialWave_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    GlialWaveObject* self = (GlialWaveObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->wave = NULL;
        self->source_region = 0;
    }
    return (PyObject*)self;
}

static int GlialWave_init(GlialWaveObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"source_region", "initial_calcium", NULL};
    unsigned int source_region = 0;
    float initial_calcium = 1.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|f", kwlist,
                                      &source_region, &initial_calcium)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "GlialWave_init: operation failed");
        return -1;
    }

    self->source_region = source_region;
    self->wave = nimcp_glial_wave_initiate(source_region, initial_calcium);

    if (self->wave == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initiate glial wave");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "GlialWave_init: validation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief GlialWave.step(dt_ms=0) -> bool
 *
 * Advance wave propagation.
 */
static PyObject* GlialWave_step(GlialWaveObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"dt_ms", NULL};
    float dt_ms = 0.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|f", kwlist, &dt_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialWave_step: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_glial_wave_step(self->wave, dt_ms);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_SUCCESS);
}

/**
 * @brief GlialWave.get_level_at(region_id) -> float
 *
 * Get calcium level at region.
 */
static PyObject* GlialWave_get_level_at(GlialWaveObject* self, PyObject* args)
{
    unsigned int region_id;

    if (!PyArg_ParseTuple(args, "I", &region_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialWave_get_level_at: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float level = nimcp_glial_wave_get_level_at(self->wave, region_id);
    return PyFloat_FromDouble((double)level);
}

/**
 * @brief GlialWave.has_reached(region_id) -> bool
 *
 * Check if wave has reached region.
 */
static PyObject* GlialWave_has_reached(GlialWaveObject* self, PyObject* args)
{
    unsigned int region_id;

    if (!PyArg_ParseTuple(args, "I", &region_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialWave_has_reached: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool reached = nimcp_glial_wave_has_reached(self->wave, region_id);
    return PyBool_FromLong(reached);
}

/**
 * @brief GlialWave.wait_for_region(region_id, timeout_ms=0) -> bool
 *
 * Wait for wave to reach region.
 */
static PyObject* GlialWave_wait_for_region(GlialWaveObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"region_id", "timeout_ms", NULL};
    unsigned int region_id;
    unsigned long long timeout_ms = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|K", kwlist, &region_id, &timeout_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialWave_wait_for_region: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_glial_wave_wait_for_region(self->wave, region_id, timeout_ms);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_SUCCESS);
}

/**
 * @brief GlialWave.radius -> float
 */
static PyObject* GlialWave_get_radius(GlialWaveObject* self, void* closure)
{
    (void)closure;
    float radius = nimcp_glial_wave_get_radius(self->wave);
    return PyFloat_FromDouble((double)radius);
}

/**
 * @brief GlialWave.is_active -> bool
 */
static PyObject* GlialWave_get_is_active(GlialWaveObject* self, void* closure)
{
    (void)closure;
    bool active = nimcp_glial_wave_is_active(self->wave);
    return PyBool_FromLong(active);
}

static PyObject* GlialWave_repr(GlialWaveObject* self)
{
    float radius = nimcp_glial_wave_get_radius(self->wave);
    bool active = nimcp_glial_wave_is_active(self->wave);
    return PyUnicode_FromFormat("GlialWave(source=%u, radius=%.1f, active=%s)",
                                self->source_region, radius, active ? "True" : "False");
}

static PyMethodDef GlialWave_methods[] = {
    {"step", (PyCFunction)GlialWave_step, METH_VARARGS | METH_KEYWORDS,
     "Advance wave propagation (dt_ms=0 for default)"},
    {"get_level_at", (PyCFunction)GlialWave_get_level_at, METH_VARARGS,
     "Get calcium level at region"},
    {"has_reached", (PyCFunction)GlialWave_has_reached, METH_VARARGS,
     "Check if wave has reached region"},
    {"wait_for_region", (PyCFunction)GlialWave_wait_for_region, METH_VARARGS | METH_KEYWORDS,
     "Wait for wave to reach region"},
    {NULL}
};

static PyGetSetDef GlialWave_getset[] = {
    {"radius", (getter)GlialWave_get_radius, NULL,
     "Current propagation radius (um)", NULL},
    {"is_active", (getter)GlialWave_get_is_active, NULL,
     "Whether wave is still propagating", NULL},
    {NULL}
};

static PyTypeObject GlialWaveType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.GlialWave",
    .tp_doc = PyDoc_STR(
        "Glial calcium wave for system-wide coordination.\n\n"
        "Args:\n"
        "    source_region (int): ID of source region\n"
        "    initial_calcium (float): Initial calcium concentration\n\n"
        "Use cases:\n"
        "    - Global state transitions\n"
        "    - Metabolic resource reallocation\n"
        "    - System-wide synchronization"
    ),
    .tp_basicsize = sizeof(GlialWaveObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = GlialWave_new,
    .tp_init = (initproc)GlialWave_init,
    .tp_dealloc = (destructor)GlialWave_dealloc,
    .tp_repr = (reprfunc)GlialWave_repr,
    .tp_methods = GlialWave_methods,
    .tp_getset = GlialWave_getset,
};

/* ============================================================================
 * Module-level functions
 * ============================================================================ */

/**
 * @brief bio_async_init(config=None) -> bool
 *
 * Initialize the bio-async system.
 */
static PyObject* bio_async_init(PyObject* Py_UNUSED(self), PyObject* args)
{
    /* For now, use default config */
    (void)args;

    nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
    nimcp_error_t status = nimcp_bio_async_init(&config);

    return PyBool_FromLong(status == NIMCP_SUCCESS);
}

/**
 * @brief bio_async_shutdown() -> None
 */
static PyObject* bio_async_shutdown(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    nimcp_bio_async_shutdown();
    Py_RETURN_NONE;
}

/**
 * @brief bio_async_is_initialized() -> bool
 */
static PyObject* bio_async_is_initialized(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    bool init = nimcp_bio_async_is_initialized();
    return PyBool_FromLong(init);
}

/**
 * @brief bio_async_step(dt_ms=0) -> bool
 *
 * Advance simulation by one timestep.
 */
static PyObject* bio_async_step(PyObject* Py_UNUSED(self), PyObject* args)
{
    float dt_ms = 0.0f;

    if (!PyArg_ParseTuple(args, "|f", &dt_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio_async_step: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_error_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_bio_async_step(dt_ms);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_SUCCESS);
}

/**
 * @brief bio_async_reset_stats() -> None
 */
static PyObject* bio_async_reset_stats(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    nimcp_bio_async_reset_stats();
    Py_RETURN_NONE;
}

static PyMethodDef bio_async_module_methods[] = {
    {"bio_async_init", bio_async_init, METH_VARARGS,
     "Initialize the bio-async system"},
    {"bio_async_shutdown", bio_async_shutdown, METH_NOARGS,
     "Shutdown the bio-async system"},
    {"bio_async_is_initialized", bio_async_is_initialized, METH_NOARGS,
     "Check if bio-async is initialized"},
    {"bio_async_step", bio_async_step, METH_VARARGS,
     "Advance simulation by one timestep"},
    {"bio_async_reset_stats", bio_async_reset_stats, METH_NOARGS,
     "Reset statistics counters"},
    {NULL}
};

/* ============================================================================
 * Module initialization
 * ============================================================================ */

int init_bio_async_module(PyObject* module)
{
    LOG_MODULE_INFO("bindings.python.bio_async", "Initializing bio-async module");

    /* Ready types */
    if (PyType_Ready(&BioPromiseType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&BioFutureType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&PhaseSyncType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&PredictiveModelType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&GlialWaveType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }

    /* Add types */
    Py_INCREF(&BioPromiseType);
    if (PyModule_AddObject(module, "BioPromise", (PyObject*)&BioPromiseType) < 0) {
        Py_DECREF(&BioPromiseType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }

    Py_INCREF(&BioFutureType);
    if (PyModule_AddObject(module, "BioFuture", (PyObject*)&BioFutureType) < 0) {
        Py_DECREF(&BioFutureType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }

    Py_INCREF(&PhaseSyncType);
    if (PyModule_AddObject(module, "PhaseSync", (PyObject*)&PhaseSyncType) < 0) {
        Py_DECREF(&PhaseSyncType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }

    Py_INCREF(&PredictiveModelType);
    if (PyModule_AddObject(module, "PredictiveModel", (PyObject*)&PredictiveModelType) < 0) {
        Py_DECREF(&PredictiveModelType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }

    Py_INCREF(&GlialWaveType);
    if (PyModule_AddObject(module, "GlialWave", (PyObject*)&GlialWaveType) < 0) {
        Py_DECREF(&GlialWaveType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
        return -1;
    }

    /* Add module functions */
    for (PyMethodDef* meth = bio_async_module_methods; meth->ml_name != NULL; meth++) {
        PyObject* func = PyCFunction_New(meth, NULL);
        if (func == NULL) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
            return -1;
        }
        if (PyModule_AddObject(module, meth->ml_name, func) < 0) {
            Py_DECREF(func);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_bio_async_module: validation failed");
            return -1;
        }
    }

    /* Add channel constants */
    PyModule_AddIntConstant(module, "BIO_CHANNEL_DOPAMINE", BIO_CHANNEL_DOPAMINE);
    PyModule_AddIntConstant(module, "BIO_CHANNEL_SEROTONIN", BIO_CHANNEL_SEROTONIN);
    PyModule_AddIntConstant(module, "BIO_CHANNEL_NOREPINEPHRINE", BIO_CHANNEL_NOREPINEPHRINE);
    PyModule_AddIntConstant(module, "BIO_CHANNEL_ACETYLCHOLINE", BIO_CHANNEL_ACETYLCHOLINE);

    /* Add oscillation band constants */
    PyModule_AddIntConstant(module, "BIO_OSC_DELTA", BIO_OSC_DELTA);
    PyModule_AddIntConstant(module, "BIO_OSC_THETA", BIO_OSC_THETA);
    PyModule_AddIntConstant(module, "BIO_OSC_ALPHA", BIO_OSC_ALPHA);
    PyModule_AddIntConstant(module, "BIO_OSC_BETA", BIO_OSC_BETA);
    PyModule_AddIntConstant(module, "BIO_OSC_GAMMA", BIO_OSC_GAMMA);

    /* Add future state constants */
    PyModule_AddIntConstant(module, "BIO_FUTURE_PENDING", BIO_FUTURE_PENDING);
    PyModule_AddIntConstant(module, "BIO_FUTURE_COMPLETED", BIO_FUTURE_COMPLETED);
    PyModule_AddIntConstant(module, "BIO_FUTURE_FAILED", BIO_FUTURE_FAILED);
    PyModule_AddIntConstant(module, "BIO_FUTURE_CANCELLED", BIO_FUTURE_CANCELLED);
    PyModule_AddIntConstant(module, "BIO_FUTURE_DECAYED", BIO_FUTURE_DECAYED);
    PyModule_AddIntConstant(module, "BIO_FUTURE_REFRACTORY", BIO_FUTURE_REFRACTORY);

    LOG_MODULE_INFO("bindings.python.bio_async", "Bio-async module initialized successfully");
    return 0;
}
