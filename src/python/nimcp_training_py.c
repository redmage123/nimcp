/**
 * @file nimcp_training_py.c
 * @brief Python bindings for NIMCP Training Pipeline
 *
 * Provides Python types for:
 * - TrainingConfig: Configuration for training pipeline
 * - TrainingResult: Results from training steps
 * - Loss type, optimizer type, and scheduler type constants
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "nimcp.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_py)

/* ============================================================================
 * TrainingConfig Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_training_config_t config;
} TrainingConfigObject;

static void TrainingConfig_dealloc(TrainingConfigObject* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* TrainingConfig_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    TrainingConfigObject* self = (TrainingConfigObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        /* Initialize with defaults */
        self->config = nimcp_training_config_default();
    }
    return (PyObject*)self;
}

static int TrainingConfig_init(TrainingConfigObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {
        "loss_type", "optimizer_type", "scheduler_type",
        "learning_rate", "weight_decay", "momentum",
        "beta1", "beta2", "epsilon",
        "scheduler_step_size", "scheduler_gamma", "warmup_steps",
        "enable_gradient_clipping", "gradient_clip_value",
        "enable_biological_modulation", "biological_blend",
        NULL
    };

    int loss_type = -1;
    int optimizer_type = -1;
    int scheduler_type = -1;
    float learning_rate = -1.0f;
    float weight_decay = -1.0f;
    float momentum = -1.0f;
    float beta1 = -1.0f;
    float beta2 = -1.0f;
    float epsilon = -1.0f;
    unsigned int scheduler_step_size = 0;
    float scheduler_gamma = -1.0f;
    unsigned int warmup_steps = 0;
    int enable_gradient_clipping = -1;
    float gradient_clip_value = -1.0f;
    int enable_biological_modulation = -1;
    float biological_blend = -1.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiifffffIfIpfpf", kwlist,
                                     &loss_type, &optimizer_type, &scheduler_type,
                                     &learning_rate, &weight_decay, &momentum,
                                     &beta1, &beta2, &epsilon,
                                     &scheduler_step_size, &scheduler_gamma, &warmup_steps,
                                     &enable_gradient_clipping, &gradient_clip_value,
                                     &enable_biological_modulation, &biological_blend)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_init: operation failed");
        return -1;
    }

    /* Apply only specified values, keep defaults for others */
    if (loss_type >= 0)
        self->config.loss_type = (nimcp_api_loss_t)loss_type;
    if (optimizer_type >= 0)
        self->config.optimizer_type = (nimcp_api_optimizer_t)optimizer_type;
    if (scheduler_type >= 0)
        self->config.scheduler_type = (nimcp_api_scheduler_t)scheduler_type;
    if (learning_rate >= 0.0f)
        self->config.learning_rate = learning_rate;
    if (weight_decay >= 0.0f)
        self->config.weight_decay = weight_decay;
    if (momentum >= 0.0f)
        self->config.momentum = momentum;
    if (beta1 >= 0.0f)
        self->config.beta1 = beta1;
    if (beta2 >= 0.0f)
        self->config.beta2 = beta2;
    if (epsilon >= 0.0f)
        self->config.epsilon = epsilon;
    if (scheduler_step_size > 0)
        self->config.scheduler_step_size = scheduler_step_size;
    if (scheduler_gamma >= 0.0f)
        self->config.scheduler_gamma = scheduler_gamma;
    if (warmup_steps > 0)
        self->config.warmup_steps = warmup_steps;
    if (enable_gradient_clipping >= 0)
        self->config.enable_gradient_clipping = (bool)enable_gradient_clipping;
    if (gradient_clip_value >= 0.0f)
        self->config.gradient_clip_value = gradient_clip_value;
    if (enable_biological_modulation >= 0)
        self->config.enable_biological_modulation = (bool)enable_biological_modulation;
    if (biological_blend >= 0.0f)
        self->config.biological_blend = biological_blend;

    return 0;
}

/* Getters and setters for TrainingConfig */
static PyObject* TrainingConfig_get_loss_type(TrainingConfigObject* self, void* closure)
{
    return PyLong_FromLong((long)self->config.loss_type);
}

static int TrainingConfig_set_loss_type(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete loss_type attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_loss_type: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "loss_type must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_loss_type: PyLong_Check is NULL");
        return -1;
    }
    self->config.loss_type = (nimcp_api_loss_t)PyLong_AsLong(value);
    return 0;
}

static PyObject* TrainingConfig_get_optimizer_type(TrainingConfigObject* self, void* closure)
{
    return PyLong_FromLong((long)self->config.optimizer_type);
}

static int TrainingConfig_set_optimizer_type(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete optimizer_type attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_optimizer_type: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "optimizer_type must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_optimizer_type: PyLong_Check is NULL");
        return -1;
    }
    self->config.optimizer_type = (nimcp_api_optimizer_t)PyLong_AsLong(value);
    return 0;
}

static PyObject* TrainingConfig_get_scheduler_type(TrainingConfigObject* self, void* closure)
{
    return PyLong_FromLong((long)self->config.scheduler_type);
}

static int TrainingConfig_set_scheduler_type(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete scheduler_type attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_scheduler_type: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "scheduler_type must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_scheduler_type: PyLong_Check is NULL");
        return -1;
    }
    self->config.scheduler_type = (nimcp_api_scheduler_t)PyLong_AsLong(value);
    return 0;
}

static PyObject* TrainingConfig_get_learning_rate(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.learning_rate);
}

static int TrainingConfig_set_learning_rate(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete learning_rate attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_learning_rate: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "learning_rate must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_learning_rate: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.learning_rate = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_weight_decay(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.weight_decay);
}

static int TrainingConfig_set_weight_decay(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete weight_decay attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_weight_decay: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "weight_decay must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_weight_decay: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.weight_decay = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_momentum(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.momentum);
}

static int TrainingConfig_set_momentum(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete momentum attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_momentum: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "momentum must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_momentum: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.momentum = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_beta1(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.beta1);
}

static int TrainingConfig_set_beta1(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete beta1 attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_beta1: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "beta1 must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_beta1: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.beta1 = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_beta2(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.beta2);
}

static int TrainingConfig_set_beta2(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete beta2 attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_beta2: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "beta2 must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_beta2: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.beta2 = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_epsilon(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.epsilon);
}

static int TrainingConfig_set_epsilon(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete epsilon attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_epsilon: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "epsilon must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_epsilon: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.epsilon = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_scheduler_step_size(TrainingConfigObject* self, void* closure)
{
    return PyLong_FromUnsignedLong((unsigned long)self->config.scheduler_step_size);
}

static int TrainingConfig_set_scheduler_step_size(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete scheduler_step_size attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_scheduler_step_size: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "scheduler_step_size must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_scheduler_step_size: PyLong_Check is NULL");
        return -1;
    }
    self->config.scheduler_step_size = (uint32_t)PyLong_AsUnsignedLong(value);
    return 0;
}

static PyObject* TrainingConfig_get_scheduler_gamma(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.scheduler_gamma);
}

static int TrainingConfig_set_scheduler_gamma(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete scheduler_gamma attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_scheduler_gamma: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "scheduler_gamma must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_scheduler_gamma: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.scheduler_gamma = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_warmup_steps(TrainingConfigObject* self, void* closure)
{
    return PyLong_FromUnsignedLong((unsigned long)self->config.warmup_steps);
}

static int TrainingConfig_set_warmup_steps(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete warmup_steps attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_warmup_steps: validation failed");
        return -1;
    }
    if (!PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "warmup_steps must be an integer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_warmup_steps: PyLong_Check is NULL");
        return -1;
    }
    self->config.warmup_steps = (uint32_t)PyLong_AsUnsignedLong(value);
    return 0;
}

static PyObject* TrainingConfig_get_enable_gradient_clipping(TrainingConfigObject* self, void* closure)
{
    return PyBool_FromLong((long)self->config.enable_gradient_clipping);
}

static int TrainingConfig_set_enable_gradient_clipping(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete enable_gradient_clipping attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_enable_gradient_clipping: validation failed");
        return -1;
    }
    self->config.enable_gradient_clipping = PyObject_IsTrue(value);
    return 0;
}

static PyObject* TrainingConfig_get_gradient_clip_value(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.gradient_clip_value);
}

static int TrainingConfig_set_gradient_clip_value(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete gradient_clip_value attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_gradient_clip_value: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "gradient_clip_value must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_gradient_clip_value: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.gradient_clip_value = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* TrainingConfig_get_enable_biological_modulation(TrainingConfigObject* self, void* closure)
{
    return PyBool_FromLong((long)self->config.enable_biological_modulation);
}

static int TrainingConfig_set_enable_biological_modulation(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete enable_biological_modulation attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_enable_biological_modulation: validation failed");
        return -1;
    }
    self->config.enable_biological_modulation = PyObject_IsTrue(value);
    return 0;
}

static PyObject* TrainingConfig_get_biological_blend(TrainingConfigObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->config.biological_blend);
}

static int TrainingConfig_set_biological_blend(TrainingConfigObject* self, PyObject* value, void* closure)
{
    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete biological_blend attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_biological_blend: validation failed");
        return -1;
    }
    if (!PyFloat_Check(value) && !PyLong_Check(value)) {
        PyErr_SetString(PyExc_TypeError, "biological_blend must be a number");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "TrainingConfig_set_biological_blend: required parameter is NULL (PyFloat_Check, PyLong_Check)");
        return -1;
    }
    self->config.biological_blend = (float)PyFloat_AsDouble(value);
    return 0;
}

/* Class method: default() */
static PyObject* TrainingConfig_default(PyTypeObject* cls, PyObject* args)
{
    TrainingConfigObject* self = (TrainingConfigObject*)cls->tp_alloc(cls, 0);
    if (self != NULL) {
        self->config = nimcp_training_config_default();
    }
    return (PyObject*)self;
}

/* __repr__ method */
static PyObject* TrainingConfig_repr(TrainingConfigObject* self)
{
    return PyUnicode_FromFormat(
        "TrainingConfig(loss_type=%d, optimizer_type=%d, scheduler_type=%d, "
        "learning_rate=%.6f, weight_decay=%.6f)",
        (int)self->config.loss_type,
        (int)self->config.optimizer_type,
        (int)self->config.scheduler_type,
        self->config.learning_rate,
        self->config.weight_decay
    );
}

static PyGetSetDef TrainingConfig_getsetters[] = {
    {"loss_type", (getter)TrainingConfig_get_loss_type, (setter)TrainingConfig_set_loss_type,
     "Loss function type (LOSS_MSE, LOSS_CROSS_ENTROPY, etc.)", NULL},
    {"optimizer_type", (getter)TrainingConfig_get_optimizer_type, (setter)TrainingConfig_set_optimizer_type,
     "Optimizer type (OPT_SGD, OPT_ADAM, etc.)", NULL},
    {"scheduler_type", (getter)TrainingConfig_get_scheduler_type, (setter)TrainingConfig_set_scheduler_type,
     "Learning rate scheduler type", NULL},
    {"learning_rate", (getter)TrainingConfig_get_learning_rate, (setter)TrainingConfig_set_learning_rate,
     "Initial learning rate", NULL},
    {"weight_decay", (getter)TrainingConfig_get_weight_decay, (setter)TrainingConfig_set_weight_decay,
     "L2 regularization coefficient", NULL},
    {"momentum", (getter)TrainingConfig_get_momentum, (setter)TrainingConfig_set_momentum,
     "Momentum coefficient for SGD", NULL},
    {"beta1", (getter)TrainingConfig_get_beta1, (setter)TrainingConfig_set_beta1,
     "Adam beta1 parameter", NULL},
    {"beta2", (getter)TrainingConfig_get_beta2, (setter)TrainingConfig_set_beta2,
     "Adam beta2 parameter", NULL},
    {"epsilon", (getter)TrainingConfig_get_epsilon, (setter)TrainingConfig_set_epsilon,
     "Adam epsilon parameter", NULL},
    {"scheduler_step_size", (getter)TrainingConfig_get_scheduler_step_size, (setter)TrainingConfig_set_scheduler_step_size,
     "Steps between LR updates", NULL},
    {"scheduler_gamma", (getter)TrainingConfig_get_scheduler_gamma, (setter)TrainingConfig_set_scheduler_gamma,
     "LR decay factor", NULL},
    {"warmup_steps", (getter)TrainingConfig_get_warmup_steps, (setter)TrainingConfig_set_warmup_steps,
     "Number of warmup steps", NULL},
    {"enable_gradient_clipping", (getter)TrainingConfig_get_enable_gradient_clipping, (setter)TrainingConfig_set_enable_gradient_clipping,
     "Enable gradient clipping", NULL},
    {"gradient_clip_value", (getter)TrainingConfig_get_gradient_clip_value, (setter)TrainingConfig_set_gradient_clip_value,
     "Maximum gradient norm", NULL},
    {"enable_biological_modulation", (getter)TrainingConfig_get_enable_biological_modulation, (setter)TrainingConfig_set_enable_biological_modulation,
     "Enable biological plasticity modulation", NULL},
    {"biological_blend", (getter)TrainingConfig_get_biological_blend, (setter)TrainingConfig_set_biological_blend,
     "Biological modulation strength (0-1)", NULL},
    {NULL}
};

static PyMethodDef TrainingConfig_methods[] = {
    {"default", (PyCFunction)TrainingConfig_default, METH_NOARGS | METH_CLASS,
     "Create a TrainingConfig with default values.\n\n"
     "Returns:\n"
     "    TrainingConfig: Default configuration with:\n"
     "        - loss_type: LOSS_CROSS_ENTROPY\n"
     "        - optimizer_type: OPT_ADAM\n"
     "        - scheduler_type: SCHED_COSINE\n"
     "        - learning_rate: 0.001\n"
     "        - biological_modulation: enabled at 50%"},
    {NULL}
};

PyTypeObject TrainingConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.TrainingConfig",
    .tp_doc = PyDoc_STR(
        "Training configuration for NIMCP brain training pipeline.\n\n"
        "Attributes:\n"
        "    loss_type: Loss function type (LOSS_MSE, LOSS_CROSS_ENTROPY, etc.)\n"
        "    optimizer_type: Optimizer type (OPT_SGD, OPT_ADAM, etc.)\n"
        "    scheduler_type: Learning rate scheduler type\n"
        "    learning_rate: Initial learning rate\n"
        "    weight_decay: L2 regularization coefficient\n"
        "    momentum: Momentum coefficient for SGD\n"
        "    beta1, beta2, epsilon: Adam parameters\n"
        "    scheduler_step_size: Steps between LR updates\n"
        "    scheduler_gamma: LR decay factor\n"
        "    warmup_steps: Number of warmup steps\n"
        "    enable_gradient_clipping: Enable gradient clipping\n"
        "    gradient_clip_value: Maximum gradient norm\n"
        "    enable_biological_modulation: Enable biological plasticity modulation\n"
        "    biological_blend: Biological modulation strength (0-1)\n\n"
        "Example:\n"
        "    >>> config = nimcp.TrainingConfig.default()\n"
        "    >>> config.learning_rate = 0.01\n"
        "    >>> config.optimizer_type = nimcp.OPT_ADAMW"
    ),
    .tp_basicsize = sizeof(TrainingConfigObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = TrainingConfig_new,
    .tp_init = (initproc)TrainingConfig_init,
    .tp_dealloc = (destructor)TrainingConfig_dealloc,
    .tp_repr = (reprfunc)TrainingConfig_repr,
    .tp_methods = TrainingConfig_methods,
    .tp_getset = TrainingConfig_getsetters,
};

/* ============================================================================
 * TrainingResult Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_training_result_t result;
} TrainingResultObject;

static void TrainingResult_dealloc(TrainingResultObject* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* TrainingResult_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    TrainingResultObject* self = (TrainingResultObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        memset(&self->result, 0, sizeof(nimcp_training_result_t));
    }
    return (PyObject*)self;
}

/* Read-only getters for TrainingResult */
static PyObject* TrainingResult_get_loss(TrainingResultObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->result.loss);
}

static PyObject* TrainingResult_get_learning_rate(TrainingResultObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->result.learning_rate);
}

static PyObject* TrainingResult_get_step(TrainingResultObject* self, void* closure)
{
    return PyLong_FromUnsignedLong((unsigned long)self->result.step);
}

static PyObject* TrainingResult_get_early_stopped(TrainingResultObject* self, void* closure)
{
    return PyBool_FromLong((long)self->result.early_stopped);
}

static PyObject* TrainingResult_get_gradient_norm(TrainingResultObject* self, void* closure)
{
    return PyFloat_FromDouble((double)self->result.gradient_norm);
}

/* __repr__ method */
static PyObject* TrainingResult_repr(TrainingResultObject* self)
{
    return PyUnicode_FromFormat(
        "TrainingResult(step=%u, loss=%.6f, learning_rate=%.6f, early_stopped=%s)",
        self->result.step,
        self->result.loss,
        self->result.learning_rate,
        self->result.early_stopped ? "True" : "False"
    );
}

static PyGetSetDef TrainingResult_getsetters[] = {
    {"loss", (getter)TrainingResult_get_loss, NULL, "Training loss value", NULL},
    {"learning_rate", (getter)TrainingResult_get_learning_rate, NULL, "Current learning rate", NULL},
    {"step", (getter)TrainingResult_get_step, NULL, "Training step number", NULL},
    {"early_stopped", (getter)TrainingResult_get_early_stopped, NULL, "True if early stopping was triggered", NULL},
    {"gradient_norm", (getter)TrainingResult_get_gradient_norm, NULL, "Gradient norm (if clipping enabled)", NULL},
    {NULL}
};

PyTypeObject TrainingResultType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.TrainingResult",
    .tp_doc = PyDoc_STR(
        "Result from a training step.\n\n"
        "Read-only attributes:\n"
        "    loss: Training loss value\n"
        "    learning_rate: Current learning rate\n"
        "    step: Training step number\n"
        "    early_stopped: True if early stopping was triggered\n"
        "    gradient_norm: Gradient norm (if clipping enabled)\n\n"
        "Example:\n"
        "    >>> result = brain.train_step(features, targets)\n"
        "    >>> print(f'Step {result.step}: loss={result.loss:.4f}')"
    ),
    .tp_basicsize = sizeof(TrainingResultObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = TrainingResult_new,
    .tp_dealloc = (destructor)TrainingResult_dealloc,
    .tp_repr = (reprfunc)TrainingResult_repr,
    .tp_getset = TrainingResult_getsetters,
};

/* ============================================================================
 * Helper function to create TrainingResult from C struct
 * ============================================================================ */

PyObject* TrainingResult_FromC(const nimcp_training_result_t* result)
{
    TrainingResultObject* obj = (TrainingResultObject*)TrainingResultType.tp_alloc(&TrainingResultType, 0);
    if (obj != NULL) {
        obj->result = *result;
    }
    return (PyObject*)obj;
}

/* ============================================================================
 * Module initialization
 * ============================================================================ */

int init_training_module(PyObject* module)
{
    LOG_MODULE_INFO("bindings.python.training", "Initializing training module");

    /* Initialize types */
    if (PyType_Ready(&TrainingConfigType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&TrainingResultType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }

    /* Add types to module */
    Py_INCREF(&TrainingConfigType);
    if (PyModule_AddObject(module, "TrainingConfig", (PyObject*)&TrainingConfigType) < 0) {
        Py_DECREF(&TrainingConfigType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }

    Py_INCREF(&TrainingResultType);
    if (PyModule_AddObject(module, "TrainingResult", (PyObject*)&TrainingResultType) < 0) {
        Py_DECREF(&TrainingResultType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }

    /* Add loss type constants */
    if (PyModule_AddIntConstant(module, "LOSS_MSE", NIMCP_API_LOSS_MSE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "LOSS_CROSS_ENTROPY", NIMCP_API_LOSS_CROSS_ENTROPY) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "LOSS_BINARY_CE", NIMCP_API_LOSS_BINARY_CE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "LOSS_HUBER", NIMCP_API_LOSS_HUBER) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "LOSS_MAE", NIMCP_API_LOSS_MAE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "LOSS_FOCAL", NIMCP_API_LOSS_FOCAL) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "LOSS_KL_DIV", NIMCP_API_LOSS_KL_DIV) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }

    /* Add optimizer type constants */
    if (PyModule_AddIntConstant(module, "OPT_SGD", NIMCP_API_OPT_SGD) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "OPT_MOMENTUM", NIMCP_API_OPT_MOMENTUM) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "OPT_ADAM", NIMCP_API_OPT_ADAM) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "OPT_ADAMW", NIMCP_API_OPT_ADAMW) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "OPT_RMSPROP", NIMCP_API_OPT_RMSPROP) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "OPT_ADAGRAD", NIMCP_API_OPT_ADAGRAD) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }

    /* Add scheduler type constants */
    if (PyModule_AddIntConstant(module, "SCHED_CONSTANT", NIMCP_API_SCHED_CONSTANT) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "SCHED_STEP", NIMCP_API_SCHED_STEP) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "SCHED_EXPONENTIAL", NIMCP_API_SCHED_EXPONENTIAL) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "SCHED_COSINE", NIMCP_API_SCHED_COSINE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "SCHED_WARMUP_COSINE", NIMCP_API_SCHED_WARMUP_COSINE) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "SCHED_REDUCE_ON_PLATEAU", NIMCP_API_SCHED_REDUCE_ON_PLATEAU) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }
    if (PyModule_AddIntConstant(module, "SCHED_CYCLIC", NIMCP_API_SCHED_CYCLIC) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_training_module: validation failed");
        return -1;
    }

    LOG_MODULE_INFO("bindings.python.training", "Training module initialized successfully");
    return 0;
}
