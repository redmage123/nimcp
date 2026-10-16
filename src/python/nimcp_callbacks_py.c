/**
 * @file nimcp_callbacks_py.c
 * @brief Python bindings for NIMCP Training Callbacks
 *
 * Provides Python types for:
 * - CallbackConfig: Configuration for callback system
 * - CallbackMetrics: Read-only metrics passed to callbacks
 * - Callback event and action constants
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "nimcp.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(callbacks_py)

/* Maximum registered callbacks per brain */
#define MAX_PYTHON_CALLBACKS 64

/* ============================================================================
 * Python Callback Context
 *
 * Stores Python callable and user_data to prevent garbage collection
 * and allow invoking Python from the C callback.
 * ============================================================================ */

typedef struct {
    PyObject* callback;      /* Python callable */
    PyObject* user_data;     /* User-provided data */
    uint32_t callback_id;    /* C-side callback ID */
} PyCallbackContext;

/* Global storage for callback contexts (indexed by callback_id) */
static PyCallbackContext g_callback_contexts[MAX_PYTHON_CALLBACKS];
static int g_context_count = 0;

/* ============================================================================
 * CallbackConfig Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_callback_config_t config;
} CallbackConfigObject;

static void CallbackConfig_dealloc(CallbackConfigObject* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* CallbackConfig_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    CallbackConfigObject* self = (CallbackConfigObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->config = nimcp_callback_config_default();
    }
    return (PyObject*)self;
}

static int CallbackConfig_init(CallbackConfigObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {
        "enable_auto_checkpoint", "checkpoint_interval",
        "enable_early_stopping", "patience", "min_delta",
        "divergence_threshold", "log_interval",
        NULL
    };

    int enable_auto_checkpoint = -1;
    unsigned int checkpoint_interval = 0;
    int enable_early_stopping = -1;
    unsigned int patience = 0;
    float min_delta = -1.0f;
    float divergence_threshold = -1.0f;
    unsigned int log_interval = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|pIpIffI", kwlist,
                                     &enable_auto_checkpoint, &checkpoint_interval,
                                     &enable_early_stopping, &patience, &min_delta,
                                     &divergence_threshold, &log_interval)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_init: operation failed");
        return -1;
    }

    /* Apply only specified values */
    if (enable_auto_checkpoint >= 0)
        self->config.enable_auto_checkpoint = (bool)enable_auto_checkpoint;
    if (checkpoint_interval > 0)
        self->config.checkpoint_interval = checkpoint_interval;
    if (enable_early_stopping >= 0)
        self->config.enable_early_stopping = (bool)enable_early_stopping;
    if (patience > 0)
        self->config.patience = patience;
    if (min_delta >= 0.0f)
        self->config.min_delta = min_delta;
    if (divergence_threshold >= 0.0f)
        self->config.divergence_threshold = divergence_threshold;
    if (log_interval > 0)
        self->config.log_interval = log_interval;

    return 0;
}

/* Getters and setters for CallbackConfig */
static PyObject* CallbackConfig_get_enable_auto_checkpoint(CallbackConfigObject* self, void* closure)
{
    return PyBool_FromLong((long)self->config.enable_auto_checkpoint);
}

static int CallbackConfig_set_enable_auto_checkpoint(CallbackConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_enable_auto_checkpoint: validation failed");
        return -1;
    }
    self->config.enable_auto_checkpoint = PyObject_IsTrue(value);
    return 0;
}

static PyObject* CallbackConfig_get_checkpoint_interval(CallbackConfigObject* self, void* closure)
{
    return PyLong_FromUnsignedLong((unsigned long)self->config.checkpoint_interval);
}

static int CallbackConfig_set_checkpoint_interval(CallbackConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_checkpoint_interval: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "checkpoint_interval must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_checkpoint_interval: PyLong_Check is NULL");
        return -1;
    }
    self->config.checkpoint_interval = (uint32_t)PyLong_AsUnsignedLong(value);
    return 0;
}

static PyObject* CallbackConfig_get_enable_early_stopping(CallbackConfigObject* self, void* closure)
{
    return PyBool_FromLong((long)self->config.enable_early_stopping);
}

static int CallbackConfig_set_enable_early_stopping(CallbackConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_enable_early_stopping: validation failed");
        return -1;
    }
    self->config.enable_early_stopping = PyObject_IsTrue(value);
    return 0;
}

static PyObject* CallbackConfig_get_patience(CallbackConfigObject* self, void* closure)
{
    return PyLong_FromUnsignedLong((unsigned long)self->config.patience);
}

static int CallbackConfig_set_patience(CallbackConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_patience: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "patience must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_patience: PyLong_Check is NULL");
        return -1;
    }
    self->config.patience = (uint32_t)PyLong_AsUnsignedLong(value);
    return 0;
}

static PyObject* CallbackConfig_get_min_delta(CallbackConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.min_delta);
}

static int CallbackConfig_set_min_delta(CallbackConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_min_delta: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "min_delta must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_min_delta: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.min_delta = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* CallbackConfig_get_divergence_threshold(CallbackConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.divergence_threshold);
}

static int CallbackConfig_set_divergence_threshold(CallbackConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_divergence_threshold: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "divergence_threshold must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_divergence_threshold: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.divergence_threshold = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* CallbackConfig_get_log_interval(CallbackConfigObject* self, void* closure)
{
    return PyLong_FromUnsignedLong((unsigned long)self->config.log_interval);
}

static int CallbackConfig_set_log_interval(CallbackConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_log_interval: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "log_interval must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "CallbackConfig_set_log_interval: PyLong_Check is NULL");
        return -1;
    }
    self->config.log_interval = (uint32_t)PyLong_AsUnsignedLong(value);
    return 0;
}

/* Class method: default() */
static PyObject* CallbackConfig_default(PyTypeObject* cls, PyObject* args)
{
    CallbackConfigObject* self = (CallbackConfigObject*)cls->tp_alloc(cls, 0);
    if (self != NULL) {
        self->config = nimcp_callback_config_default();
    }
    return (PyObject*)self;
}

static PyObject* CallbackConfig_repr(CallbackConfigObject* self)
{
    return PyUnicode_FromFormat(
        "CallbackConfig(enable_auto_checkpoint=%s, checkpoint_interval=%u, "
        "enable_early_stopping=%s, patience=%u)",
        self->config.enable_auto_checkpoint ? "True" : "False",
        self->config.checkpoint_interval,
        self->config.enable_early_stopping ? "True" : "False",
        self->config.patience
    );
}

static PyGetSetDef CallbackConfig_getsetters[] = {
    {"enable_auto_checkpoint", (getter)CallbackConfig_get_enable_auto_checkpoint,
     (setter)CallbackConfig_set_enable_auto_checkpoint, "Enable automatic checkpointing", NULL},
    {"checkpoint_interval", (getter)CallbackConfig_get_checkpoint_interval,
     (setter)CallbackConfig_set_checkpoint_interval, "Steps between checkpoints", NULL},
    {"enable_early_stopping", (getter)CallbackConfig_get_enable_early_stopping,
     (setter)CallbackConfig_set_enable_early_stopping, "Enable early stopping", NULL},
    {"patience", (getter)CallbackConfig_get_patience,
     (setter)CallbackConfig_set_patience, "Steps without improvement before stop", NULL},
    {"min_delta", (getter)CallbackConfig_get_min_delta,
     (setter)CallbackConfig_set_min_delta, "Minimum improvement to reset patience", NULL},
    {"divergence_threshold", (getter)CallbackConfig_get_divergence_threshold,
     (setter)CallbackConfig_set_divergence_threshold, "Loss increase ratio for divergence", NULL},
    {"log_interval", (getter)CallbackConfig_get_log_interval,
     (setter)CallbackConfig_set_log_interval, "Steps between log outputs", NULL},
    {NULL}
};

static PyMethodDef CallbackConfig_methods[] = {
    {"default", (PyCFunction)CallbackConfig_default, METH_NOARGS | METH_CLASS,
     "Create a CallbackConfig with default values.\n\n"
     "Returns:\n"
     "    CallbackConfig: Default configuration"},
    {NULL}
};

PyTypeObject CallbackConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.CallbackConfig",
    .tp_doc = PyDoc_STR(
        "Configuration for NIMCP training callbacks.\n\n"
        "Attributes:\n"
        "    enable_auto_checkpoint: Enable automatic checkpointing\n"
        "    checkpoint_interval: Steps between checkpoints\n"
        "    enable_early_stopping: Enable early stopping\n"
        "    patience: Steps without improvement before stop\n"
        "    min_delta: Minimum improvement to reset patience\n"
        "    divergence_threshold: Loss increase ratio for divergence\n"
        "    log_interval: Steps between log outputs\n\n"
        "Example:\n"
        "    >>> config = nimcp.CallbackConfig.default()\n"
        "    >>> config.enable_early_stopping = True\n"
        "    >>> config.patience = 10"
    ),
    .tp_basicsize = sizeof(CallbackConfigObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = CallbackConfig_new,
    .tp_init = (initproc)CallbackConfig_init,
    .tp_dealloc = (destructor)CallbackConfig_dealloc,
    .tp_repr = (reprfunc)CallbackConfig_repr,
    .tp_methods = CallbackConfig_methods,
    .tp_getset = CallbackConfig_getsetters,
};

/* ============================================================================
 * CallbackMetrics Type (Read-only wrapper)
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_callback_metrics_t metrics;
} CallbackMetricsObject;

static void CallbackMetrics_dealloc(CallbackMetricsObject* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* CallbackMetrics_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    CallbackMetricsObject* self = (CallbackMetricsObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        memset(&self->metrics, 0, sizeof(nimcp_callback_metrics_t));
    }
    return (PyObject*)self;
}

/* Read-only getters */
static PyObject* CallbackMetrics_get_step(CallbackMetricsObject* self, void* closure)
{
    return PyLong_FromUnsignedLongLong((unsigned long long)self->metrics.step);
}

static PyObject* CallbackMetrics_get_epoch(CallbackMetricsObject* self, void* closure)
{
    return PyLong_FromUnsignedLong((unsigned long)self->metrics.epoch);
}

static PyObject* CallbackMetrics_get_loss(CallbackMetricsObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->metrics.loss);
}

static PyObject* CallbackMetrics_get_learning_rate(CallbackMetricsObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->metrics.learning_rate);
}

static PyObject* CallbackMetrics_get_gradient_norm(CallbackMetricsObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->metrics.gradient_norm);
}

static PyObject* CallbackMetrics_get_step_time_us(CallbackMetricsObject* self, void* closure)
{
    return PyLong_FromUnsignedLongLong((unsigned long long)self->metrics.step_time_us);
}

static PyObject* CallbackMetrics_get_is_converging(CallbackMetricsObject* self, void* closure)
{
    return PyBool_FromLong((long)self->metrics.is_converging);
}

static PyObject* CallbackMetrics_get_is_diverging(CallbackMetricsObject* self, void* closure)
{
    return PyBool_FromLong((long)self->metrics.is_diverging);
}

static PyObject* CallbackMetrics_repr(CallbackMetricsObject* self)
{
    return PyUnicode_FromFormat(
        "CallbackMetrics(step=%llu, epoch=%u, loss=%.6f, lr=%.6f)",
        (unsigned long long)self->metrics.step,
        self->metrics.epoch,
        self->metrics.loss,
        self->metrics.learning_rate
    );
}

static PyGetSetDef CallbackMetrics_getsetters[] = {
    {"step", (getter)CallbackMetrics_get_step, NULL, "Current training step", NULL},
    {"epoch", (getter)CallbackMetrics_get_epoch, NULL, "Current epoch", NULL},
    {"loss", (getter)CallbackMetrics_get_loss, NULL, "Current loss value", NULL},
    {"learning_rate", (getter)CallbackMetrics_get_learning_rate, NULL, "Current learning rate", NULL},
    {"gradient_norm", (getter)CallbackMetrics_get_gradient_norm, NULL, "Gradient norm", NULL},
    {"step_time_us", (getter)CallbackMetrics_get_step_time_us, NULL, "Step time in microseconds", NULL},
    {"is_converging", (getter)CallbackMetrics_get_is_converging, NULL, "Loss trending down", NULL},
    {"is_diverging", (getter)CallbackMetrics_get_is_diverging, NULL, "Loss trending up rapidly", NULL},
    {NULL}
};

PyTypeObject CallbackMetricsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.CallbackMetrics",
    .tp_doc = PyDoc_STR(
        "Training metrics passed to callbacks (read-only).\n\n"
        "Attributes:\n"
        "    step: Current training step\n"
        "    epoch: Current epoch\n"
        "    loss: Current loss value\n"
        "    learning_rate: Current learning rate\n"
        "    gradient_norm: Gradient norm\n"
        "    step_time_us: Step time in microseconds\n"
        "    is_converging: Loss trending down\n"
        "    is_diverging: Loss trending up rapidly"
    ),
    .tp_basicsize = sizeof(CallbackMetricsObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = CallbackMetrics_new,
    .tp_dealloc = (destructor)CallbackMetrics_dealloc,
    .tp_repr = (reprfunc)CallbackMetrics_repr,
    .tp_getset = CallbackMetrics_getsetters,
};

/* ============================================================================
 * Helper functions
 * ============================================================================ */

/**
 * @brief Create CallbackMetrics from C struct
 */
PyObject* CallbackMetrics_FromC(const nimcp_callback_metrics_t* metrics)
{
    CallbackMetricsObject* obj = (CallbackMetricsObject*)CallbackMetricsType.tp_alloc(&CallbackMetricsType, 0);
    if (obj != NULL) {
        obj->metrics = *metrics;
    }
    return (PyObject*)obj;
}

/**
 * @brief C callback wrapper that invokes Python callback
 *
 * This function is registered with the C API and forwards calls to Python.
 */
nimcp_callback_action_t python_callback_wrapper(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t* metrics,
    void* user_data)
{
    PyCallbackContext* ctx = (PyCallbackContext*)user_data;
    if (ctx == NULL || ctx->callback == NULL) {
        return NIMCP_CB_ACTION_CONTINUE;
    }

    /* Acquire GIL for Python callback */
    PyGILState_STATE gstate = PyGILState_Ensure();

    nimcp_callback_action_t action = NIMCP_CB_ACTION_CONTINUE;

    /* Create metrics object */
    PyObject* metrics_obj = CallbackMetrics_FromC(metrics);
    if (metrics_obj == NULL) {
        PyErr_Clear();
        PyGILState_Release(gstate);
        return NIMCP_CB_ACTION_CONTINUE;
    }

    /* Call Python callback: callback(event, metrics) or callback(event, metrics, user_data) */
    PyObject* result;
    if (ctx->user_data != NULL && ctx->user_data != Py_None) {
        result = PyObject_CallFunction(ctx->callback, "iOO",
                                       (int)event, metrics_obj, ctx->user_data);
    } else {
        result = PyObject_CallFunction(ctx->callback, "iO", (int)event, metrics_obj);
    }

    Py_DECREF(metrics_obj);

    if (result == NULL) {
        /* Python exception occurred - log and continue */
        PyErr_Print();
        PyErr_Clear();
    } else if (PyLong_Check(result)) {
        /* Return value is an action code */
        action = (nimcp_callback_action_t)PyLong_AsLong(result);
        Py_DECREF(result);
    } else {
        Py_DECREF(result);
    }

    PyGILState_Release(gstate);
    return action;
}

/**
 * @brief Find or create a callback context slot
 */
PyCallbackContext* allocate_callback_context(void)
{
    if (g_context_count >= MAX_PYTHON_CALLBACKS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_callback_context: capacity exceeded");
        return NULL;
    }
    return &g_callback_contexts[g_context_count++];
}

/**
 * @brief Find callback context by ID
 */
static PyCallbackContext* find_callback_context(uint32_t callback_id)
{
    for (int i = 0; i < g_context_count; i++) {
        if (g_callback_contexts[i].callback_id == callback_id) {
            return &g_callback_contexts[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_callback_context: validation failed");
    return NULL;
}

/**
 * @brief Release callback context
 */
void release_callback_context(uint32_t callback_id)
{
    for (int i = 0; i < g_context_count; i++) {
        if (g_callback_contexts[i].callback_id == callback_id) {
            Py_XDECREF(g_callback_contexts[i].callback);
            Py_XDECREF(g_callback_contexts[i].user_data);
            g_callback_contexts[i].callback = NULL;
            g_callback_contexts[i].user_data = NULL;
            g_callback_contexts[i].callback_id = 0;
            break;
        }
    }
}

/* ============================================================================
 * Module initialization
 * ============================================================================ */

int init_callbacks_module(PyObject* module)
{
    LOG_MODULE_INFO("bindings.python.callbacks", "Initializing callbacks module");

    /* Initialize types */
    if (PyType_Ready(&CallbackConfigType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&CallbackMetricsType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }

    /* Add types to module */
    Py_INCREF(&CallbackConfigType);
    if (PyModule_AddObject(module, "CallbackConfig", (PyObject*)&CallbackConfigType) < 0) {
        Py_DECREF(&CallbackConfigType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }

    Py_INCREF(&CallbackMetricsType);
    if (PyModule_AddObject(module, "CallbackMetrics", (PyObject*)&CallbackMetricsType) < 0) {
        Py_DECREF(&CallbackMetricsType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }

    /* Add callback event constants */
    if (PyModule_AddIntConstant(module, "CB_STEP_COMPLETE", NIMCP_CB_STEP_COMPLETE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_EPOCH_COMPLETE", NIMCP_CB_EPOCH_COMPLETE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_LOSS_COMPUTED", NIMCP_CB_LOSS_COMPUTED) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_WEIGHTS_UPDATED", NIMCP_CB_WEIGHTS_UPDATED) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_LR_CHANGED", NIMCP_CB_LR_CHANGED) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_CONVERGENCE", NIMCP_CB_CONVERGENCE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_DIVERGENCE", NIMCP_CB_DIVERGENCE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_CHECKPOINT", NIMCP_CB_CHECKPOINT) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }

    /* Add callback action constants */
    if (PyModule_AddIntConstant(module, "CB_ACTION_CONTINUE", NIMCP_CB_ACTION_CONTINUE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_ACTION_STOP", NIMCP_CB_ACTION_STOP) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_ACTION_SKIP", NIMCP_CB_ACTION_SKIP) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_ACTION_ROLLBACK", NIMCP_CB_ACTION_ROLLBACK) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_ACTION_REDUCE_LR", NIMCP_CB_ACTION_REDUCE_LR) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "CB_ACTION_INCREASE_LR", NIMCP_CB_ACTION_INCREASE_LR) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_callbacks_module: validation failed");
        return -1;
    }

    /* Initialize callback context storage */
    memset(g_callback_contexts, 0, sizeof(g_callback_contexts));
    g_context_count = 0;

    LOG_MODULE_INFO("bindings.python.callbacks", "Callbacks module initialized successfully");
    return 0;
}
