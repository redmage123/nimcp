/**
 * @file nimcp_types.c
 * @brief Python type definitions for NIMCP module
 */

#include <stddef.h>  /* for NULL */
#include <string.h>  // For strdup
#include <structmember.h>  // For PyMemberDef, T_UINT, T_OBJECT_EX

#include "common/nimcp_module.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "core/brain/nimcp_pretrained.h"
#include "io/serialization/nimcp_network_serialization.h"
#include "utils/logging/nimcp_logging.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "api/nimcp_api_internal.h"  /* For internal brain access */
#include "nimcp.h"  /* For training API */
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(types)

/* Forward declaration from nimcp_training_py.c */
extern PyObject* TrainingResult_FromC(const nimcp_training_result_t* result);
extern PyTypeObject TrainingConfigType;

//=============================================================================
// Brain Type
//=============================================================================

/**
 * @brief Initialize Brain object from constructor arguments
 *
 * SIGNATURE: Brain(name, size, task, num_inputs, num_outputs)
 *        OR: Brain(config) where config is a BrainConfig object
 */
static int Brain_init(BrainObject* self, PyObject* args, PyObject* kwds)
{
    // Check if first argument is a BrainConfig
    if (PyTuple_Size(args) == 1) {
        PyObject* first_arg = PyTuple_GetItem(args, 0);
        if (PyObject_TypeCheck(first_arg, &BrainConfigType)) {
            BrainConfigObject* config = (BrainConfigObject*)first_arg;

            // Create brain from config
            self->brain = nimcp_brain_create("from_config", config->size,
                                             config->task,
                                             config->num_inputs, config->num_outputs);

            if (!self->brain) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to create brain from config");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_init: self->brain is NULL");
                return -1;
            }
            return 0;
        }
    }

    // Fall back to original argument parsing
    const char* name;
    int size = -1, task = NIMCP_TASK_CLASSIFICATION;
    unsigned int num_inputs = 10, num_outputs = 10;
    unsigned int neuron_count = 0;
    const char* checkpoint = NULL;
    const char* init_mode = NULL;

    static char* kwlist[] = {"name", "size", "task", "inputs", "outputs",
                             "num_inputs", "num_outputs", "neuron_count",
                             "checkpoint", "init_mode", NULL};

    // Parse constructor arguments — all optional except name
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|iiIIIIIzz", kwlist,
                                     &name, &size, &task,
                                     &num_inputs, &num_outputs,
                                     &num_inputs, &num_outputs,
                                     &neuron_count, &checkpoint, &init_mode)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_init: operation failed");
        return -1;
    }

    // Validate task parameter
    if (task < NIMCP_TASK_CLASSIFICATION || task > NIMCP_TASK_ASSOCIATION) {
        PyErr_SetString(PyExc_ValueError, "task must be 0-4 (CLASSIFICATION=0, REGRESSION=1, PATTERN_MATCHING=2, SEQUENCE=3, ASSOCIATION=4)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_init: validation failed");
        return -1;
    }

    // Choose creation path based on arguments
    if (neuron_count > 0) {
        // Explicit neuron count — use init_mode to pick create function
        if (init_mode && strcmp(init_mode, "full") == 0) {
            self->brain = nimcp_brain_create_full(name, (nimcp_brain_task_t)task,
                                                  num_inputs, num_outputs, neuron_count);
        } else if (init_mode && strcmp(init_mode, "fast") == 0) {
            self->brain = nimcp_brain_create_fast(name, (nimcp_brain_task_t)task,
                                                  num_inputs, num_outputs, neuron_count);
        } else {
            self->brain = nimcp_brain_create_with_neurons(name, (nimcp_brain_task_t)task,
                                                          num_inputs, num_outputs, neuron_count);
        }
    } else if (size >= NIMCP_BRAIN_TINY && size <= NIMCP_BRAIN_LARGE) {
        // Size preset
        self->brain = nimcp_brain_create(name, (nimcp_brain_size_t)size,
                                         (nimcp_brain_task_t)task,
                                         num_inputs, num_outputs);
    } else {
        PyErr_SetString(PyExc_ValueError,
            "Must provide either size (0-3) or neuron_count");
        return -1;
    }

    if (!self->brain) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_init: self->brain is NULL");
        return -1;
    }

    return 0;
}

static void Brain_dealloc(BrainObject* self)
{
    if (self->brain) {
        nimcp_brain_destroy(self->brain);
    }
    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* Brain_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    BrainObject* self = (BrainObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->brain = NULL;
    }
    return (PyObject*) self;
}

// Brain.learn(features, label, confidence=1.0) -> None
static PyObject* Brain_learn(BrainObject* self, PyObject* args, PyObject* kwds)
{
    PyObject* features_list;
    const char* label;
    float confidence = 1.0F;

    static char* kwlist[] = {"features", "label", "confidence", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os|f", kwlist,
                                     &features_list, &label, &confidence)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_learn: operation failed");
        return NULL;
    }

    // Convert Python list to C array
    if (!PyList_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_learn: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(features_list);
    if (num_features <= 0) {
        PyErr_SetString(PyExc_ValueError, "features list must not be empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_learn: validation failed");
        return NULL;
    }

    float* features = (float*)nimcp_malloc(sizeof(float) * (size_t)num_features);
    if (!features) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_features; i++) {
        PyObject* item = PyList_GetItem(features_list, i);
        features[i] = (float)PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            nimcp_free(features);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_learn: validation failed");
            return NULL;
        }
    }

    // Copy label since we release GIL
    char* label_copy = strdup(label);
    if (!label_copy) {
        nimcp_free(features);
        return PyErr_NoMemory();
    }

    // Release GIL during potentially long-running C operation
    // P1-13 fix: features and label_copy MUST be freed AFTER Py_END_ALLOW_THREADS
    // because nimcp_brain_learn_example references them during the C operation.
    // Freeing before would cause use-after-free.
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_learn_example(
        self->brain, features, (uint32_t)num_features, label_copy, confidence);
    Py_END_ALLOW_THREADS

    // Safe to free now - C operation is complete and GIL is reacquired
    nimcp_free(features);
    nimcp_free(label_copy);

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to learn from example");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_learn: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

// Brain.decide(features) -> (label, confidence)
static PyObject* Brain_decide(BrainObject* self, PyObject* args)
{
    PyObject* features_list;

    if (!PyArg_ParseTuple(args, "O", &features_list)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_decide: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Convert Python list to C array
    if (!PyList_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_decide: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(features_list);
    if (num_features <= 0) {
        PyErr_SetString(PyExc_ValueError, "features list must not be empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_decide: validation failed");
        return NULL;
    }

    float* features = (float*)nimcp_malloc(sizeof(float) * (size_t)num_features);
    if (!features) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_features; i++) {
        PyObject* item = PyList_GetItem(features_list, i);
        features[i] = (float)PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            nimcp_free(features);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_decide: validation failed");
            return NULL;
        }
    }

    // Allocate output buffers
    char label[NIMCP_ID_BUFFER_SIZE];
    float confidence;

    // Release GIL during potentially long-running C operation
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_predict(
        self->brain, features, (uint32_t)num_features, label, &confidence);
    Py_END_ALLOW_THREADS

    nimcp_free(features);

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to make prediction");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_decide: validation failed");
        return NULL;
    }

    // Return tuple (label, confidence)
    return Py_BuildValue("(sf)", label, confidence);
}

// Brain.set_active_modalities(flags) -> None
static PyObject* Brain_set_active_modalities(BrainObject* self, PyObject* args)
{
    unsigned int flags;
    if (!PyArg_ParseTuple(args, "I", &flags)) return NULL;
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        return NULL;
    }
    brain_set_active_modalities(self->brain->internal_brain, (uint32_t)flags);
    Py_RETURN_NONE;
}

// Brain.get_active_modalities() -> int
static PyObject* Brain_get_active_modalities(BrainObject* self, PyObject* Py_UNUSED(args))
{
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        return NULL;
    }
    uint32_t flags = brain_get_active_modalities(self->brain->internal_brain);
    return PyLong_FromUnsignedLong(flags);
}

// Brain.submit_sensory(modality, data, width=0, height=0, channels=0) -> None
static PyObject* Brain_submit_sensory(BrainObject* self, PyObject* args, PyObject* kwargs)
{
    static char* kwlist[] = {"modality", "data", "width", "height", "channels", NULL};
    const char* modality_str;
    PyObject* data_obj;
    unsigned int width = 0, height = 0, channels = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|III", kwlist,
                                      &modality_str, &data_obj, &width, &height, &channels))
        return NULL;

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        return NULL;
    }

    // Map string to modality flag
    uint32_t modality_flag;
    if (strcmp(modality_str, "visual") == 0) {
        modality_flag = 0x02u;  // BRAIN_MODALITY_VISUAL
        if (width == 0 || height == 0 || channels == 0) {
            PyErr_SetString(PyExc_ValueError,
                "Visual modality requires width, height, channels kwargs");
            return NULL;
        }
    } else if (strcmp(modality_str, "audio") == 0) {
        modality_flag = 0x04u;  // BRAIN_MODALITY_AUDIO
    } else if (strcmp(modality_str, "somatosensory") == 0) {
        modality_flag = 0x08u;  // BRAIN_MODALITY_SOMATOSENSORY
    } else if (strcmp(modality_str, "speech") == 0) {
        modality_flag = 0x10u;  // BRAIN_MODALITY_SPEECH
    } else {
        PyErr_Format(PyExc_ValueError,
            "Unknown modality '%s'. Use: visual, audio, somatosensory, speech",
            modality_str);
        return NULL;
    }

    // Convert Python list/bytes to C array
    Py_buffer buf;
    uint8_t* raw_data = NULL;
    uint32_t data_size = 0;

    if (PyObject_CheckBuffer(data_obj)) {
        // Bytes / bytearray / numpy array with buffer protocol
        if (PyObject_GetBuffer(data_obj, &buf, PyBUF_SIMPLE) < 0)
            return NULL;
        raw_data = (uint8_t*)buf.buf;
        data_size = (uint32_t)buf.len;
    } else if (PyList_Check(data_obj)) {
        // List of floats → convert to float array
        Py_ssize_t n = PyList_Size(data_obj);
        if (n <= 0) {
            PyErr_SetString(PyExc_ValueError, "Data list is empty");
            return NULL;
        }
        float* float_buf = (float*)malloc(n * sizeof(float));
        if (!float_buf) {
            PyErr_NoMemory();
            return NULL;
        }
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject* item = PyList_GET_ITEM(data_obj, i);
            float_buf[i] = (float)PyFloat_AsDouble(item);
            if (PyErr_Occurred()) {
                free(float_buf);
                return NULL;
            }
        }
        raw_data = (uint8_t*)float_buf;
        data_size = (uint32_t)(n * sizeof(float));
        // Call submit, then free
        int rc = brain_submit_sensory(self->brain->internal_brain, modality_flag,
                                       raw_data, data_size, width, height, channels);
        free(float_buf);
        if (rc != 0) {
            PyErr_SetString(NIMCPError, "brain_submit_sensory failed");
            return NULL;
        }
        Py_RETURN_NONE;
    } else {
        PyErr_SetString(PyExc_TypeError,
            "data must be bytes, bytearray, or list of floats");
        return NULL;
    }

    // Buffer protocol path
    int rc = brain_submit_sensory(self->brain->internal_brain, modality_flag,
                                   raw_data, data_size, width, height, channels);
    PyBuffer_Release(&buf);
    if (rc != 0) {
        PyErr_SetString(NIMCPError, "brain_submit_sensory failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

// Brain.decide_full(features) -> dict with full cognitive decision data
static PyObject* Brain_decide_full(BrainObject* self, PyObject* args)
{
    PyObject* features_list;

    if (!PyArg_ParseTuple(args, "O", &features_list)) {
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        return NULL;
    }

    if (!PyList_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(features_list);
    if (num_features <= 0) {
        PyErr_SetString(PyExc_ValueError, "features list must not be empty");
        return NULL;
    }

    float* features = (float*)nimcp_malloc(sizeof(float) * (size_t)num_features);
    if (!features) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_features; i++) {
        PyObject* item = PyList_GetItem(features_list, i);
        features[i] = (float)PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            nimcp_free(features);
            return NULL;
        }
    }

    // Output buffers
    char label[64];
    float confidence;
    char explanation[256];
    float output_vector[1024];
    uint32_t output_size = 1024;
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
        PyErr_SetString(NIMCPError, "Failed to run decide_full");
        return NULL;
    }

    // Build output vector list
    uint32_t vec_len = (output_size < 1024) ? output_size : 1024;
    PyObject* vec_list = PyList_New(vec_len);
    if (!vec_list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_decide_full: PyList_New failed");
        return NULL;
    }
    for (uint32_t i = 0; i < vec_len; i++) {
        PyList_SetItem(vec_list, i, PyFloat_FromDouble(output_vector[i]));
    }

    // Build result dict — PyDict_SetItemString does NOT steal refs, so we must DECREF temps
    PyObject* result = PyDict_New();
    if (!result) {
        Py_DECREF(vec_list);
        return NULL;
    }
    PyObject* tmp;

    tmp = PyUnicode_FromString(label);
    PyDict_SetItemString(result, "label", tmp);
    Py_DECREF(tmp);

    tmp = PyFloat_FromDouble(confidence);
    PyDict_SetItemString(result, "confidence", tmp);
    Py_DECREF(tmp);

    tmp = PyUnicode_FromString(explanation);
    PyDict_SetItemString(result, "explanation", tmp);
    Py_DECREF(tmp);

    PyDict_SetItemString(result, "output_vector", vec_list);

    tmp = PyLong_FromUnsignedLong(num_active_neurons);
    PyDict_SetItemString(result, "num_active_neurons", tmp);
    Py_DECREF(tmp);

    tmp = PyFloat_FromDouble(sparsity);
    PyDict_SetItemString(result, "sparsity", tmp);
    Py_DECREF(tmp);

    tmp = PyLong_FromUnsignedLongLong(inference_time_us);
    PyDict_SetItemString(result, "inference_time_us", tmp);
    Py_DECREF(tmp);

    Py_DECREF(vec_list);
    return result;
}

// Brain.clone_cow() -> Brain
static PyObject* Brain_clone_cow(BrainObject* self, PyObject* Py_UNUSED(ignored))
{
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_clone_cow: self->brain is NULL");
        return NULL;
    }

    // Call unified API to create COW clone
    nimcp_brain_t clone_brain;
    Py_BEGIN_ALLOW_THREADS
    clone_brain = nimcp_brain_clone_cow(self->brain);
    Py_END_ALLOW_THREADS

    if (!clone_brain) {
        PyErr_SetString(NIMCPError, "Failed to create COW clone");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_clone_cow: clone_brain is NULL");
        return NULL;
    }

    // Create new Python Brain object WITHOUT calling __init__
    BrainObject* clone_obj = (BrainObject*)BrainType.tp_alloc(&BrainType, 0);
    if (!clone_obj) {
        nimcp_brain_destroy(clone_brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_clone_cow: clone_obj is NULL");
        return NULL;
    }

    // Assign the cloned brain
    clone_obj->brain = clone_brain;

    return (PyObject*)clone_obj;
}

// Brain.save(filepath) -> None
static PyObject* Brain_save(BrainObject* self, PyObject* args)
{
    const char* filepath;

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_save: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Copy filepath since we release GIL
    char* filepath_copy = strdup(filepath);
    if (!filepath_copy) {
        return PyErr_NoMemory();
    }

    // Release GIL during file I/O operation
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_save(self->brain, filepath_copy);
    Py_END_ALLOW_THREADS

    nimcp_free(filepath_copy);

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to save brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_save: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

// Brain.load(filepath) -> Brain (classmethod)
static PyObject* Brain_load(PyObject* cls, PyObject* args)
{
    const char* filepath;

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_load: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Copy filepath since we release GIL
    char* filepath_copy = strdup(filepath);
    if (!filepath_copy) {
        return PyErr_NoMemory();
    }

    // Release GIL during file I/O operation
    nimcp_brain_t brain;
    Py_BEGIN_ALLOW_THREADS
    brain = nimcp_brain_load(filepath_copy);
    Py_END_ALLOW_THREADS

    nimcp_free(filepath_copy);

    if (!brain) {
        PyErr_SetString(NIMCPError, "Failed to load brain from file");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_load: brain is NULL");
        return NULL;
    }

    // Create Python Brain object
    PyTypeObject* type = (PyTypeObject*)cls;
    BrainObject* brain_obj = (BrainObject*)type->tp_alloc(type, 0);
    if (!brain_obj) {
        nimcp_brain_destroy(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_load: brain_obj is NULL");
        return NULL;
    }

    brain_obj->brain = brain;

    return (PyObject*)brain_obj;
}

// Brain.from_pretrained(model_name, models_dir=None) -> Brain (classmethod)
static PyObject* Brain_from_pretrained(PyObject* cls, PyObject* args, PyObject* kwds)
{
    const char* model_name;
    const char* models_dir = NULL;

    static char* kwlist[] = {"model_name", "models_dir", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|z", kwlist,
                                     &model_name, &models_dir)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_from_pretrained: operation failed");
        return NULL;
    }

    // Copy strings since we release GIL
    char* model_name_copy = strdup(model_name);
    if (!model_name_copy) {
        return PyErr_NoMemory();
    }

    char* models_dir_copy = NULL;
    if (models_dir) {
        models_dir_copy = strdup(models_dir);
        if (!models_dir_copy) {
            nimcp_free(model_name_copy);
            return PyErr_NoMemory();
        }
    }

    // Release GIL during file I/O operation
    nimcp_brain_t brain;
    Py_BEGIN_ALLOW_THREADS
    brain = (nimcp_brain_t)brain_load_pretrained(model_name_copy, models_dir_copy);
    Py_END_ALLOW_THREADS

    nimcp_free(model_name_copy);
    nimcp_free(models_dir_copy);

    if (!brain) {
        PyErr_Format(NIMCPError, "Failed to load pre-trained model: %s", model_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_from_pretrained: brain is NULL");
        return NULL;
    }

    // Create Python Brain object
    PyTypeObject* type = (PyTypeObject*)cls;
    BrainObject* brain_obj = (BrainObject*)type->tp_alloc(type, 0);
    if (!brain_obj) {
        nimcp_brain_destroy(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_from_pretrained: brain_obj is NULL");
        return NULL;
    }

    brain_obj->brain = brain;

    return (PyObject*)brain_obj;
}

// Brain.finetune(training_data, labels, num_epochs=5, learning_rate=0.001, freeze_sensory=True, freeze_cognitive=True) -> None
static PyObject* Brain_finetune(BrainObject* self, PyObject* args, PyObject* kwds)
{
    PyObject* training_data_list;
    PyObject* labels_list;
    uint32_t num_epochs = 5;
    float learning_rate = 0.001F;
    int freeze_sensory = 1;
    int freeze_cognitive = 1;

    static char* kwlist[] = {"training_data", "labels", "num_epochs", "learning_rate",
                            "freeze_sensory", "freeze_cognitive", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|Ifpp", kwlist,
                                     &training_data_list, &labels_list,
                                     &num_epochs, &learning_rate,
                                     &freeze_sensory, &freeze_cognitive)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_finetune: operation failed");
        return NULL;
    }

    // Validate inputs
    if (!PyList_Check(training_data_list)) {
        PyErr_SetString(PyExc_TypeError, "training_data must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_finetune: PyList_Check is NULL");
        return NULL;
    }
    if (!PyList_Check(labels_list)) {
        PyErr_SetString(PyExc_TypeError, "labels must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_finetune: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t num_samples = PyList_Size(training_data_list);
    if (num_samples != PyList_Size(labels_list)) {
        PyErr_SetString(PyExc_ValueError, "training_data and labels must have same length");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_finetune: validation failed");
        return NULL;
    }

    // For now, just print a message (full implementation would convert data and call brain_finetune)
    printf("Fine-tuning brain with %zd samples, %u epochs, lr=%.4f\n",
           num_samples, num_epochs, learning_rate);
    printf("  freeze_sensory=%d, freeze_cognitive=%d\n", freeze_sensory, freeze_cognitive);

    // TODO: Convert Python lists to C arrays and call brain_finetune()
    // This requires iterating through the nested lists and flattening them

    Py_RETURN_NONE;
}

// Brain.probe() -> dict
static PyObject* Brain_probe(BrainObject* self, PyObject* Py_UNUSED(ignored))
{
    nimcp_brain_probe_t probe;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_probe(self->brain, &probe);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to probe brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_probe: validation failed");
        return NULL;
    }

    PyObject* dict = PyDict_New();
    if (!dict) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dict is NULL");

        return NULL;

    }

    // Helper macro to add item to dict with error checking
    // PyDict_SetItemString steals no reference, but we create temporary objects
    // that need to be decref'd after use
    #define ADD_DICT_ITEM(key, value_expr) do { \
        PyObject* _val = (value_expr); \
        if (!_val) { \
            Py_DECREF(dict); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_probe: _val is NULL"); \
            return NULL; \
        } \
        if (PyDict_SetItemString(dict, (key), _val) < 0) { \
            Py_DECREF(_val); \
            Py_DECREF(dict); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_probe: validation failed"); \
            return NULL; \
        } \
        Py_DECREF(_val); \
    } while (0)

    ADD_DICT_ITEM("task_name", PyUnicode_FromString(probe.task_name));
    ADD_DICT_ITEM("size", PyLong_FromLong(probe.size));
    ADD_DICT_ITEM("task", PyLong_FromLong(probe.task));
    ADD_DICT_ITEM("num_neurons", PyLong_FromUnsignedLong(probe.num_neurons));
    ADD_DICT_ITEM("num_synapses", PyLong_FromUnsignedLong(probe.num_synapses));
    ADD_DICT_ITEM("num_active_synapses", PyLong_FromUnsignedLong(probe.num_active_synapses));
    ADD_DICT_ITEM("total_inferences", PyLong_FromUnsignedLongLong(probe.total_inferences));
    ADD_DICT_ITEM("total_learning_steps", PyLong_FromUnsignedLongLong(probe.total_learning_steps));
    ADD_DICT_ITEM("avg_sparsity", PyFloat_FromDouble(probe.avg_sparsity));
    ADD_DICT_ITEM("avg_inference_time_us", PyFloat_FromDouble(probe.avg_inference_time_us));
    ADD_DICT_ITEM("current_learning_rate", PyFloat_FromDouble(probe.current_learning_rate));
    ADD_DICT_ITEM("accuracy", PyFloat_FromDouble(probe.accuracy));
    ADD_DICT_ITEM("memory_bytes", PyLong_FromSize_t(probe.memory_bytes));
    ADD_DICT_ITEM("num_inputs", PyLong_FromUnsignedLong(probe.num_inputs));
    ADD_DICT_ITEM("num_outputs", PyLong_FromUnsignedLong(probe.num_outputs));
    ADD_DICT_ITEM("is_cow_clone", PyBool_FromLong(probe.is_cow_clone));
    ADD_DICT_ITEM("cow_ref_count", PyLong_FromUnsignedLong(probe.cow_ref_count));
    ADD_DICT_ITEM("cow_shared_bytes", PyLong_FromSize_t(probe.cow_shared_bytes));
    ADD_DICT_ITEM("cow_private_bytes", PyLong_FromSize_t(probe.cow_private_bytes));

    #undef ADD_DICT_ITEM

    return dict;
}

/* ============================================================================
 * Training Pipeline Methods
 * ============================================================================ */

/**
 * @brief Brain.configure_training(config) -> None
 *
 * Configure the brain's training pipeline with the specified configuration.
 * Must be called before using train_step() or train_batch().
 */
static PyObject* Brain_configure_training(BrainObject* self, PyObject* args)
{
    PyObject* config_obj;

    if (!PyArg_ParseTuple(args, "O", &config_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_configure_training: PyArg_ParseTuple is NULL");
        return NULL;
    }

    /* Verify it's a TrainingConfig object */
    if (!PyObject_TypeCheck(config_obj, &TrainingConfigType)) {
        PyErr_SetString(PyExc_TypeError, "config must be a TrainingConfig object");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_configure_training: PyObject_TypeCheck is NULL");
        return NULL;
    }

    /* Access the underlying config struct */
    /* TrainingConfigObject is defined in nimcp_training_py.c */
    typedef struct {
        PyObject_HEAD
        nimcp_training_config_t config;
    } TrainingConfigObject;

    TrainingConfigObject* config = (TrainingConfigObject*)config_obj;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_configure_training(self->brain, &config->config);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to configure training pipeline");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_configure_training: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * @brief Brain.train_step(features, targets) -> TrainingResult
 *
 * Perform a single training step using the configured training pipeline.
 */
static PyObject* Brain_train_step(BrainObject* self, PyObject* args)
{
    PyObject* features_list;
    PyObject* targets_list;

    if (!PyArg_ParseTuple(args, "OO", &features_list, &targets_list)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: PyArg_ParseTuple is NULL");
        return NULL;
    }

    /* Validate inputs */
    if (!PyList_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: PyList_Check is NULL");
        return NULL;
    }
    if (!PyList_Check(targets_list)) {
        PyErr_SetString(PyExc_TypeError, "targets must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(features_list);
    Py_ssize_t num_targets = PyList_Size(targets_list);

    if (num_features <= 0 || num_targets <= 0) {
        PyErr_SetString(PyExc_ValueError, "features and targets must be non-empty lists");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: validation failed");
        return NULL;
    }

    /* Convert features to C array */
    float* features = (float*)nimcp_malloc(sizeof(float) * (size_t)num_features);
    if (features == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: validation failed");
        return NULL;
    }

    for (Py_ssize_t i = 0; i < num_features; i++) {
        PyObject* item = PyList_GetItem(features_list, i);
        if (!PyFloat_Check(item) && !PyLong_Check(item)) {
            nimcp_free(features);
            PyErr_SetString(PyExc_TypeError, "All feature values must be numbers");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: required parameter is NULL (PyFloat_Check, PyLong_Check)");
            return NULL;
        }
        features[i] = (float)PyFloat_AsDouble(item);
    }

    /* Convert targets to C array */
    float* targets = (float*)nimcp_malloc(sizeof(float) * (size_t)num_targets);
    if (targets == NULL) {
        nimcp_free(features);
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: validation failed");
        return NULL;
    }

    for (Py_ssize_t i = 0; i < num_targets; i++) {
        PyObject* item = PyList_GetItem(targets_list, i);
        if (!PyFloat_Check(item) && !PyLong_Check(item)) {
            nimcp_free(features);
            nimcp_free(targets);
            PyErr_SetString(PyExc_TypeError, "All target values must be numbers");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: required parameter is NULL (PyFloat_Check, PyLong_Check)");
            return NULL;
        }
        targets[i] = (float)PyFloat_AsDouble(item);
    }

    /* Perform training step */
    nimcp_training_result_t result;
    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_train_step(self->brain, features, (uint32_t)num_features,
                                    targets, (uint32_t)num_targets, &result);
    Py_END_ALLOW_THREADS

    nimcp_free(features);
    nimcp_free(targets);

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Training step failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_step: validation failed");
        return NULL;
    }

    return TrainingResult_FromC(&result);
}

/**
 * @brief Brain.train_batch(features, targets) -> TrainingResult
 *
 * Train on a batch of examples. Features and targets should be 2D lists.
 */
static PyObject* Brain_train_batch(BrainObject* self, PyObject* args)
{
    PyObject* features_list;
    PyObject* targets_list;

    if (!PyArg_ParseTuple(args, "OO", &features_list, &targets_list)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_batch: PyArg_ParseTuple is NULL");
        return NULL;
    }

    /* Validate inputs are lists of lists */
    if (!PyList_Check(features_list) || !PyList_Check(targets_list)) {
        PyErr_SetString(PyExc_TypeError, "features and targets must be lists of lists");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_batch: required parameter is NULL (PyList_Check, PyList_Check)");
        return NULL;
    }

    Py_ssize_t batch_size = PyList_Size(features_list);
    if (batch_size <= 0 || PyList_Size(targets_list) != batch_size) {
        PyErr_SetString(PyExc_ValueError, "features and targets must have same batch size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_train_batch: validation failed");
        return NULL;
    }

    /* Get dimensions from first example */
    PyObject* first_features = PyList_GetItem(features_list, 0);
    PyObject* first_targets = PyList_GetItem(targets_list, 0);

    if (!PyList_Check(first_features) || !PyList_Check(first_targets)) {
        PyErr_SetString(PyExc_TypeError, "features and targets must be 2D lists");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_batch: required parameter is NULL (PyList_Check, PyList_Check)");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(first_features);
    Py_ssize_t num_targets = PyList_Size(first_targets);

    if (num_features <= 0 || num_targets <= 0) {
        PyErr_SetString(PyExc_ValueError, "feature and target dimensions must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_batch: validation failed");
        return NULL;
    }

    /* Allocate flattened arrays */
    float* features = (float*)nimcp_malloc(sizeof(float) * (size_t)(batch_size * num_features));
    float* targets = (float*)nimcp_malloc(sizeof(float) * (size_t)(batch_size * num_targets));

    if (features == NULL || targets == NULL) {
        nimcp_free(features);
        nimcp_free(targets);
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_batch: validation failed");
        return NULL;
    }

    /* Flatten the 2D lists into 1D arrays */
    for (Py_ssize_t b = 0; b < batch_size; b++) {
        PyObject* feat_row = PyList_GetItem(features_list, b);
        PyObject* targ_row = PyList_GetItem(targets_list, b);

        if (!PyList_Check(feat_row) || PyList_Size(feat_row) != num_features) {
            nimcp_free(features);
            nimcp_free(targets);
            PyErr_SetString(PyExc_ValueError, "All feature rows must have same length");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_train_batch: PyList_Check is NULL");
            return NULL;
        }
        if (!PyList_Check(targ_row) || PyList_Size(targ_row) != num_targets) {
            nimcp_free(features);
            nimcp_free(targets);
            PyErr_SetString(PyExc_ValueError, "All target rows must have same length");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_train_batch: PyList_Check is NULL");
            return NULL;
        }

        for (Py_ssize_t i = 0; i < num_features; i++) {
            PyObject* item = PyList_GetItem(feat_row, i);
            features[b * num_features + i] = (float)PyFloat_AsDouble(item);
        }
        for (Py_ssize_t i = 0; i < num_targets; i++) {
            PyObject* item = PyList_GetItem(targ_row, i);
            targets[b * num_targets + i] = (float)PyFloat_AsDouble(item);
        }
    }

    /* Perform batch training */
    nimcp_training_result_t result;
    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_train_batch(self->brain, features, targets,
                                     (uint32_t)batch_size, (uint32_t)num_features,
                                     (uint32_t)num_targets, &result);
    Py_END_ALLOW_THREADS

    nimcp_free(features);
    nimcp_free(targets);

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Batch training failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_train_batch: validation failed");
        return NULL;
    }

    return TrainingResult_FromC(&result);
}

/**
 * @brief Brain.get_training_stats() -> (total_steps, total_loss, current_lr)
 *
 * Get current training statistics.
 */
static PyObject* Brain_get_training_stats(BrainObject* self, PyObject* Py_UNUSED(args))
{
    uint64_t total_steps;
    float total_loss;
    float current_lr;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_get_training_stats(self->brain, &total_steps, &total_loss, &current_lr);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to get training stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_training_stats: validation failed");
        return NULL;
    }

    return Py_BuildValue("(Kff)", (unsigned long long)total_steps, total_loss, current_lr);
}

/**
 * @brief Brain.step_scheduler(validation_metric) -> float
 *
 * Step the learning rate scheduler. Returns the new learning rate.
 */
static PyObject* Brain_step_scheduler(BrainObject* self, PyObject* args)
{
    float validation_metric = 0.0f;

    if (!PyArg_ParseTuple(args, "|f", &validation_metric)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_step_scheduler: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float new_lr;

    Py_BEGIN_ALLOW_THREADS
    new_lr = nimcp_brain_step_scheduler(self->brain, validation_metric);
    Py_END_ALLOW_THREADS

    return PyFloat_FromDouble((double)new_lr);
}

/* ============================================================================
 * Callback Methods
 * ============================================================================ */

/* Forward declarations from nimcp_callbacks_py.c */
extern PyTypeObject CallbackConfigType;
extern PyTypeObject CallbackMetricsType;

/* Callback context management from nimcp_callbacks_py.c */
typedef struct {
    PyObject* callback;
    PyObject* user_data;
    uint32_t callback_id;
} PyCallbackContext;

extern PyCallbackContext* allocate_callback_context(void);
extern void release_callback_context(uint32_t callback_id);

/* C callback wrapper - defined in nimcp_callbacks_py.c */
extern nimcp_callback_action_t python_callback_wrapper(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t* metrics,
    void* user_data);

/**
 * @brief Brain.enable_callbacks(config) -> None
 */
static PyObject* Brain_enable_callbacks(BrainObject* self, PyObject* args)
{
    PyObject* config_obj = NULL;

    if (!PyArg_ParseTuple(args, "|O", &config_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_enable_callbacks: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_callback_config_t* config_ptr = NULL;
    nimcp_callback_config_t config;

    if (config_obj != NULL && config_obj != Py_None) {
        if (!PyObject_TypeCheck(config_obj, &CallbackConfigType)) {
            PyErr_SetString(PyExc_TypeError, "config must be a CallbackConfig object");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_enable_callbacks: PyObject_TypeCheck is NULL");
            return NULL;
        }

        /* Access the underlying config struct */
        typedef struct {
            PyObject_HEAD
            nimcp_callback_config_t config;
        } CallbackConfigObject;

        CallbackConfigObject* cfg = (CallbackConfigObject*)config_obj;
        config = cfg->config;
        config_ptr = &config;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_enable_callbacks(self->brain, config_ptr);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to enable callbacks");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_enable_callbacks: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * @brief Brain.disable_callbacks() -> None
 */
static PyObject* Brain_disable_callbacks(BrainObject* self, PyObject* Py_UNUSED(args))
{
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_disable_callbacks(self->brain);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to disable callbacks");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_disable_callbacks: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * @brief Brain.register_callback(event, callback, name=None, user_data=None) -> int
 */
static PyObject* Brain_register_callback(BrainObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"event", "callback", "name", "user_data", NULL};

    int event;
    PyObject* callback;
    const char* name = NULL;
    PyObject* user_data = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "iO|zO", kwlist,
                                     &event, &callback, &name, &user_data)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_register_callback: operation failed");
        return NULL;
    }

    /* Validate callback is callable */
    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_register_callback: PyCallable_Check is NULL");
        return NULL;
    }

    /* Allocate context to store Python callback */
    PyCallbackContext* ctx = allocate_callback_context();
    if (ctx == NULL) {
        PyErr_SetString(NIMCPError, "Maximum number of callbacks exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_register_callback: validation failed");
        return NULL;
    }

    /* Store Python objects with increased reference count */
    Py_INCREF(callback);
    ctx->callback = callback;
    if (user_data != NULL) {
        Py_INCREF(user_data);
        ctx->user_data = user_data;
    } else {
        ctx->user_data = NULL;
    }

    /* Register with C API using wrapper function */
    uint32_t callback_id;
    Py_BEGIN_ALLOW_THREADS
    callback_id = nimcp_brain_register_callback(
        self->brain,
        (nimcp_callback_event_t)event,
        python_callback_wrapper,
        ctx,
        name
    );
    Py_END_ALLOW_THREADS

    if (callback_id == 0) {
        Py_DECREF(callback);
        Py_XDECREF(user_data);
        ctx->callback = NULL;
        ctx->user_data = NULL;
        PyErr_SetString(NIMCPError, "Failed to register callback");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_register_callback: callback_id is zero");
        return NULL;
    }

    ctx->callback_id = callback_id;
    return PyLong_FromUnsignedLong((unsigned long)callback_id);
}

/**
 * @brief Brain.unregister_callback(callback_id) -> None
 */
static PyObject* Brain_unregister_callback(BrainObject* self, PyObject* args)
{
    unsigned int callback_id;

    if (!PyArg_ParseTuple(args, "I", &callback_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_unregister_callback: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_unregister_callback(self->brain, callback_id);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to unregister callback");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_unregister_callback: validation failed");
        return NULL;
    }

    /* Release Python references */
    release_callback_context(callback_id);

    Py_RETURN_NONE;
}

/**
 * @brief Brain.get_callback_stats() -> (total_fired, avg_time_us, early_stops)
 */
static PyObject* Brain_get_callback_stats(BrainObject* self, PyObject* Py_UNUSED(args))
{
    uint64_t total_fired;
    float avg_time_us;
    uint32_t early_stops;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_get_callback_stats(self->brain, &total_fired, &avg_time_us, &early_stops);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to get callback stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_callback_stats: validation failed");
        return NULL;
    }

    return Py_BuildValue("(KfI)",
                         (unsigned long long)total_fired,
                         avg_time_us,
                         (unsigned int)early_stops);
}

/* ============================================================================
 * Snapshot Methods
 * ============================================================================ */

/**
 * @brief Brain.snapshot_save(name, description=None) -> None
 */
static PyObject* Brain_snapshot_save(BrainObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"name", "description", NULL};

    const char* name;
    const char* description = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|z", kwlist, &name, &description)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_save: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_snapshot_save(self->brain, name, description);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to save snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_save: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * @brief Brain.snapshot_restore(name) -> Brain
 */
static PyObject* Brain_snapshot_restore(BrainObject* self, PyObject* args)
{
    const char* name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_restore: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_brain_t restored;
    Py_BEGIN_ALLOW_THREADS
    restored = nimcp_brain_snapshot_restore(self->brain, name);
    Py_END_ALLOW_THREADS

    if (restored == NULL) {
        PyErr_SetString(NIMCPError, "Failed to restore snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_restore: validation failed");
        return NULL;
    }

    /* Create new Python Brain object with restored brain */
    BrainObject* new_brain = (BrainObject*)BrainType.tp_alloc(&BrainType, 0);
    if (new_brain == NULL) {
        nimcp_brain_destroy(restored);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_snapshot_restore: validation failed");
        return NULL;
    }
    new_brain->brain = restored;

    return (PyObject*)new_brain;
}

/**
 * @brief Brain.snapshot_list(max_count=100) -> list[dict]
 */
static PyObject* Brain_snapshot_list(BrainObject* self, PyObject* args)
{
    unsigned int max_count = 100;

    if (!PyArg_ParseTuple(args, "|I", &max_count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_list: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (max_count == 0 || max_count > 1000) {
        max_count = 100;
    }

    /* Allocate array for snapshot infos */
    nimcp_brain_snapshot_info_t* infos = (nimcp_brain_snapshot_info_t*)nimcp_malloc(
        sizeof(nimcp_brain_snapshot_info_t) * max_count);
    if (infos == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_list: validation failed");
        return NULL;
    }

    uint32_t out_count = 0;
    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_snapshot_list(self->brain, infos, max_count, &out_count);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        nimcp_free(infos);
        PyErr_SetString(NIMCPError, "Failed to list snapshots");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_list: validation failed");
        return NULL;
    }

    /* Build Python list of dicts */
    PyObject* result_list = PyList_New((Py_ssize_t)out_count);
    if (result_list == NULL) {
        nimcp_free(infos);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_snapshot_list: validation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < out_count; i++) {
        PyObject* dict = PyDict_New();
        if (dict == NULL) {
            Py_DECREF(result_list);
            nimcp_free(infos);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_snapshot_list: validation failed");
            return NULL;
        }

        PyObject* tmp;

        tmp = PyUnicode_FromString(infos[i].name);
        PyDict_SetItemString(dict, "name", tmp);
        Py_DECREF(tmp);

        tmp = PyUnicode_FromString(infos[i].description);
        PyDict_SetItemString(dict, "description", tmp);
        Py_DECREF(tmp);

        tmp = PyLong_FromUnsignedLongLong(infos[i].timestamp);
        PyDict_SetItemString(dict, "timestamp", tmp);
        Py_DECREF(tmp);

        tmp = PyLong_FromUnsignedLong(infos[i].file_size);
        PyDict_SetItemString(dict, "file_size", tmp);
        Py_DECREF(tmp);

        tmp = PyBool_FromLong(infos[i].is_compressed);
        PyDict_SetItemString(dict, "is_compressed", tmp);
        Py_DECREF(tmp);

        tmp = PyBool_FromLong(infos[i].is_encrypted);
        PyDict_SetItemString(dict, "is_encrypted", tmp);
        Py_DECREF(tmp);

        PyList_SET_ITEM(result_list, i, dict);  /* Steals reference */
    }

    nimcp_free(infos);
    return result_list;
}

/**
 * @brief Brain.snapshot_delete(name) -> None
 */
static PyObject* Brain_snapshot_delete(BrainObject* self, PyObject* args)
{
    const char* name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_delete: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_snapshot_delete(self->brain, name);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to delete snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_delete: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/* ============================================================================
 * Working Memory Methods
 * ============================================================================ */

/**
 * @brief Brain.working_memory_add(data, salience) -> None
 */
static PyObject* Brain_working_memory_add(BrainObject* self, PyObject* args)
{
    PyObject* data_list;
    float salience;

    if (!PyArg_ParseTuple(args, "Of", &data_list, &salience)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_working_memory_add: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (!PyList_Check(data_list)) {
        PyErr_SetString(PyExc_TypeError, "data must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_working_memory_add: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t size = PyList_Size(data_list);
    if (size <= 0) {
        PyErr_SetString(PyExc_ValueError, "data must be non-empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_working_memory_add: validation failed");
        return NULL;
    }

    float* data = (float*)nimcp_malloc(sizeof(float) * (size_t)size);
    if (data == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_working_memory_add: validation failed");
        return NULL;
    }

    for (Py_ssize_t i = 0; i < size; i++) {
        PyObject* item = PyList_GetItem(data_list, i);
        data[i] = (float)PyFloat_AsDouble(item);
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_working_memory_add(self->brain, data, (uint32_t)size, salience);
    Py_END_ALLOW_THREADS

    nimcp_free(data);

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to add to working memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_working_memory_add: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * @brief Brain.working_memory_get(index) -> (data, size) or None
 */
static PyObject* Brain_working_memory_get(BrainObject* self, PyObject* args)
{
    unsigned int index;

    if (!PyArg_ParseTuple(args, "I", &index)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "Brain_working_memory_get: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t size_out;
    const float* data;

    Py_BEGIN_ALLOW_THREADS
    data = nimcp_brain_working_memory_get(self->brain, index, &size_out);
    Py_END_ALLOW_THREADS

    if (data == NULL) {
        Py_RETURN_NONE;
    }

    /* Build Python list from data */
    PyObject* result_list = PyList_New((Py_ssize_t)size_out);
    if (result_list == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_working_memory_get: validation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < size_out; i++) {
        PyList_SET_ITEM(result_list, i, PyFloat_FromDouble((double)data[i]));
    }

    PyObject* tmp = PyLong_FromUnsignedLong(size_out);
    PyObject* ret = Py_BuildValue("(ON)", result_list, tmp);
    return ret;
}

/**
 * @brief Brain.working_memory_stats() -> (current_size, capacity)
 */
static PyObject* Brain_working_memory_stats(BrainObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t current_size, capacity;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_working_memory_stats(self->brain, &current_size, &capacity);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to get working memory stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_working_memory_stats: validation failed");
        return NULL;
    }

    return Py_BuildValue("(II)", (unsigned int)current_size, (unsigned int)capacity);
}

/**
 * @brief Brain.working_memory_refresh(index) -> None
 */
static PyObject* Brain_working_memory_refresh(BrainObject* self, PyObject* args)
{
    unsigned int index;

    if (!PyArg_ParseTuple(args, "I", &index)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "Brain_working_memory_refresh: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_working_memory_refresh(self->brain, index);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to refresh working memory item");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_working_memory_refresh: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/* ============================================================================
 * Global Workspace Methods
 * ============================================================================ */

/**
 * @brief Brain.workspace_compete(module, content, strength) -> bool
 */
static PyObject* Brain_workspace_compete(BrainObject* self, PyObject* args)
{
    int module;
    PyObject* content_list;
    float strength;

    if (!PyArg_ParseTuple(args, "iOf", &module, &content_list, &strength)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_compete: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (!PyList_Check(content_list)) {
        PyErr_SetString(PyExc_TypeError, "content must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_compete: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t content_dim = PyList_Size(content_list);
    if (content_dim <= 0) {
        PyErr_SetString(PyExc_ValueError, "content must be non-empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_workspace_compete: validation failed");
        return NULL;
    }

    float* content = (float*)nimcp_malloc(sizeof(float) * (size_t)content_dim);
    if (content == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_compete: validation failed");
        return NULL;
    }

    for (Py_ssize_t i = 0; i < content_dim; i++) {
        PyObject* item = PyList_GetItem(content_list, i);
        content[i] = (float)PyFloat_AsDouble(item);
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_workspace_compete(self->brain, (nimcp_cognitive_module_t)module,
                                           content, (uint32_t)content_dim, strength);
    Py_END_ALLOW_THREADS

    nimcp_free(content);

    /* Return True if won competition, False otherwise */
    return PyBool_FromLong(status == NIMCP_OK);
}

/**
 * @brief Brain.workspace_read(max_dim=256) -> (content, dim, source_module) or None
 */
static PyObject* Brain_workspace_read(BrainObject* self, PyObject* args)
{
    unsigned int max_dim = 256;

    if (!PyArg_ParseTuple(args, "|I", &max_dim)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_workspace_read: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float* content = (float*)nimcp_malloc(sizeof(float) * max_dim);
    if (content == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_read: validation failed");
        return NULL;
    }

    uint32_t actual_dim;
    nimcp_cognitive_module_t source_module;
    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_workspace_read(self->brain, content, max_dim, &actual_dim, &source_module);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        nimcp_free(content);
        Py_RETURN_NONE;
    }

    /* Build result list */
    PyObject* result_list = PyList_New((Py_ssize_t)actual_dim);
    if (result_list == NULL) {
        nimcp_free(content);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_workspace_read: validation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < actual_dim; i++) {
        PyList_SET_ITEM(result_list, i, PyFloat_FromDouble((double)content[i]));
    }

    nimcp_free(content);

    return Py_BuildValue("(OIi)", result_list, (unsigned int)actual_dim, (int)source_module);
}

/**
 * @brief Brain.workspace_subscribe(module) -> None
 */
static PyObject* Brain_workspace_subscribe(BrainObject* self, PyObject* args)
{
    int module;

    if (!PyArg_ParseTuple(args, "i", &module)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_subscribe: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_workspace_subscribe(self->brain, (nimcp_cognitive_module_t)module);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to subscribe to workspace");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_subscribe: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * @brief Brain.workspace_unsubscribe(module) -> None
 */
static PyObject* Brain_workspace_unsubscribe(BrainObject* self, PyObject* args)
{
    int module;

    if (!PyArg_ParseTuple(args, "i", &module)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_unsubscribe: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_workspace_unsubscribe(self->brain, (nimcp_cognitive_module_t)module);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to unsubscribe from workspace");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_unsubscribe: validation failed");
        return NULL;
    }

    Py_RETURN_NONE;
}

/**
 * @brief Brain.workspace_has_broadcast() -> bool
 */
static PyObject* Brain_workspace_has_broadcast(BrainObject* self, PyObject* Py_UNUSED(args))
{
    bool has_broadcast;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_workspace_has_broadcast(self->brain, &has_broadcast);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        return PyBool_FromLong(0);
    }

    return PyBool_FromLong(has_broadcast);
}

/**
 * @brief Brain.workspace_stats() -> (total_broadcasts, total_competitions, avg_strength)
 */
static PyObject* Brain_workspace_stats(BrainObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t total_broadcasts, total_competitions;
    float avg_strength;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_workspace_stats(self->brain, &total_broadcasts, &total_competitions, &avg_strength);
    Py_END_ALLOW_THREADS

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to get workspace stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_workspace_stats: validation failed");
        return NULL;
    }

    return Py_BuildValue("(IIf)",
                         (unsigned int)total_broadcasts,
                         (unsigned int)total_competitions,
                         avg_strength);
}

/* ============================================================================
 * Complex Oscillation Methods
 * ============================================================================ */

/**
 * @brief Brain.enable_complex_oscillations(enable) -> bool
 */
static PyObject* Brain_enable_complex_oscillations(BrainObject* self, PyObject* args)
{
    int enable;

    if (!PyArg_ParseTuple(args, "p", &enable)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_enable_complex_oscillations: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool result;
    Py_BEGIN_ALLOW_THREADS
    result = nimcp_enable_complex_oscillations(self->brain, (bool)enable);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief Brain.is_complex_oscillations_enabled() -> bool
 */
static PyObject* Brain_is_complex_oscillations_enabled(BrainObject* self, PyObject* Py_UNUSED(args))
{
    bool result;
    Py_BEGIN_ALLOW_THREADS
    result = nimcp_is_complex_oscillations_enabled(self->brain);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief Brain.get_oscillation_phasor(neuron_id) -> (amplitude, phase)
 */
static PyObject* Brain_get_oscillation_phasor(BrainObject* self, PyObject* args)
{
    unsigned int neuron_id;

    if (!PyArg_ParseTuple(args, "I", &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_oscillation_phasor: PyArg_ParseTuple is NULL");
        return NULL;
    }

    nimcp_oscillation_phasor_t phasor;
    Py_BEGIN_ALLOW_THREADS
    phasor = nimcp_get_oscillation_phasor(self->brain, neuron_id);
    Py_END_ALLOW_THREADS

    return Py_BuildValue("(ff)", phasor.amplitude, phasor.phase);
}

/**
 * @brief Brain.get_phase_coherence(neuron_ids) -> float
 */
static PyObject* Brain_get_phase_coherence(BrainObject* self, PyObject* args)
{
    PyObject* neuron_ids_list;

    if (!PyArg_ParseTuple(args, "O", &neuron_ids_list)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_phase_coherence: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (!PyList_Check(neuron_ids_list)) {
        PyErr_SetString(PyExc_TypeError, "neuron_ids must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_phase_coherence: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t count = PyList_Size(neuron_ids_list);
    if (count <= 0) {
        PyErr_SetString(PyExc_ValueError, "neuron_ids must be non-empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_phase_coherence: validation failed");
        return NULL;
    }

    uint32_t* neuron_ids = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * (size_t)count);
    if (neuron_ids == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_phase_coherence: validation failed");
        return NULL;
    }

    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject* item = PyList_GetItem(neuron_ids_list, i);
        neuron_ids[i] = (uint32_t)PyLong_AsUnsignedLong(item);
    }

    float coherence;
    Py_BEGIN_ALLOW_THREADS
    coherence = nimcp_get_phase_coherence(self->brain, neuron_ids, (uint32_t)count);
    Py_END_ALLOW_THREADS

    nimcp_free(neuron_ids);

    return PyFloat_FromDouble((double)coherence);
}

// Brain.process(inputs) -> dict
// Simpler interface that returns a dictionary with prediction results
static PyObject* Brain_process(BrainObject* self, PyObject* args)
{
    PyObject* inputs_obj;

    if (!PyArg_ParseTuple(args, "O", &inputs_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_process: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_process: self->brain is NULL");
        return NULL;
    }

    // Convert inputs to float array
    if (!PyList_Check(inputs_obj)) {
        PyErr_SetString(PyExc_TypeError, "inputs must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_process: PyList_Check is NULL");
        return NULL;
    }

    Py_ssize_t num_inputs = PyList_Size(inputs_obj);
    if (num_inputs <= 0) {
        PyErr_SetString(PyExc_ValueError, "inputs list must not be empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_process: validation failed");
        return NULL;
    }

    float* inputs = (float*)nimcp_malloc(sizeof(float) * (size_t)num_inputs);
    if (!inputs) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_inputs; i++) {
        PyObject* item = PyList_GetItem(inputs_obj, i);
        inputs[i] = (float)PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            nimcp_free(inputs);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_process: validation failed");
            return NULL;
        }
    }

    // Get prediction
    char label[NIMCP_ID_BUFFER_SIZE];
    float confidence;

    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_predict(self->brain, inputs, (uint32_t)num_inputs, label, &confidence);
    Py_END_ALLOW_THREADS

    nimcp_free(inputs);

    if (status != NIMCP_OK) {
        PyErr_SetString(NIMCPError, "Failed to process inputs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_process: validation failed");
        return NULL;
    }

    // Return a dict with the results
    PyObject* result = PyDict_New();
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    PyObject* tmp;

    tmp = PyUnicode_FromString(label);
    PyDict_SetItemString(result, "label", tmp);
    Py_DECREF(tmp);

    tmp = PyFloat_FromDouble((double)confidence);
    PyDict_SetItemString(result, "confidence", tmp);
    Py_DECREF(tmp);

    return result;
}

// Brain.release_dopamine(amount, predicted=0.0) -> float
// Release dopamine for reward learning
static PyObject* Brain_release_dopamine(BrainObject* self, PyObject* args, PyObject* kwds)
{
    float amount;
    float predicted = 0.0f;

    static char* kwlist[] = {"amount", "predicted", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "f|f", kwlist, &amount, &predicted)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_release_dopamine: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_release_dopamine: self->brain is NULL");
        return NULL;
    }

    // Get the neuromodulator system from the internal brain
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(self->brain->internal_brain);
    if (!neuromod) {
        PyErr_SetString(NIMCPError, "Neuromodulator system not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_release_dopamine: neuromod is NULL");
        return NULL;
    }

    // Release dopamine
    float released;
    Py_BEGIN_ALLOW_THREADS
    released = neuromodulator_release_dopamine(neuromod, amount, predicted);
    Py_END_ALLOW_THREADS

    return PyFloat_FromDouble((double)released);
}

// Brain.infer(features, num_outputs) -> list[float]
// Raw inference returning output vector
static PyObject* Brain_infer(BrainObject* self, PyObject* args)
{
    PyObject* features_list;
    unsigned int num_outputs;

    if (!PyArg_ParseTuple(args, "OI", &features_list, &num_outputs)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_infer: PyArg_ParseTuple failed");
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_infer: self->brain is NULL");
        return NULL;
    }

    if (!PyList_Check(features_list)) {
        PyErr_SetString(PyExc_TypeError, "features must be a list");
        return NULL;
    }

    Py_ssize_t num_features = PyList_Size(features_list);
    if (num_features <= 0) {
        PyErr_SetString(PyExc_ValueError, "features list must be non-empty");
        return NULL;
    }

    // Convert Python list to C array
    float* features = (float*)nimcp_malloc(sizeof(float) * (size_t)num_features);
    if (!features) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_features; i++) {
        PyObject* item = PyList_GetItem(features_list, i);
        features[i] = (float)PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            nimcp_free(features);
            return NULL;
        }
    }

    // Allocate output array
    float* outputs = (float*)nimcp_malloc(sizeof(float) * num_outputs);
    if (!outputs) {
        nimcp_free(features);
        return PyErr_NoMemory();
    }

    // Call C API with GIL released
    nimcp_status_t status;
    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_infer(self->brain, features, (uint32_t)num_features,
                               outputs, num_outputs);
    Py_END_ALLOW_THREADS

    nimcp_free(features);

    if (status != NIMCP_OK) {
        nimcp_free(outputs);
        PyErr_SetString(NIMCPError, "Brain inference failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_infer: inference failed");
        return NULL;
    }

    // Convert output to Python list
    PyObject* result = PyList_New(num_outputs);
    if (!result) {
        nimcp_free(outputs);
        return NULL;
    }

    for (unsigned int i = 0; i < num_outputs; i++) {
        PyList_SET_ITEM(result, i, PyFloat_FromDouble((double)outputs[i]));
    }

    nimcp_free(outputs);
    return result;
}

// Brain.resize(new_neuron_count) -> bool
static PyObject* Brain_resize(BrainObject* self, PyObject* args)
{
    unsigned int new_neuron_count;

    if (!PyArg_ParseTuple(args, "I", &new_neuron_count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_resize: PyArg_ParseTuple failed");
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_resize: self->brain is NULL");
        return NULL;
    }

    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = nimcp_brain_resize(self->brain, new_neuron_count);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(success);
}

// Brain.auto_resize() -> bool
static PyObject* Brain_auto_resize(BrainObject* self, PyObject* Py_UNUSED(ignored))
{
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_auto_resize: self->brain is NULL");
        return NULL;
    }

    bool resized;
    Py_BEGIN_ALLOW_THREADS
    resized = nimcp_brain_auto_resize(self->brain);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(resized);
}

// Brain.get_neuron_count() -> int
static PyObject* Brain_get_neuron_count(BrainObject* self, PyObject* Py_UNUSED(ignored))
{
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_neuron_count: self->brain is NULL");
        return NULL;
    }

    uint32_t count = nimcp_brain_get_neuron_count(self->brain);
    return PyLong_FromUnsignedLong(count);
}

// Brain.get_utilization_metrics() -> (float, float)
static PyObject* Brain_get_utilization_metrics(BrainObject* self, PyObject* Py_UNUSED(ignored))
{
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_utilization_metrics: self->brain is NULL");
        return NULL;
    }

    float utilization = 0.0f, saturation = 0.0f;
    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = nimcp_brain_get_utilization_metrics(self->brain, &utilization, &saturation);
    Py_END_ALLOW_THREADS

    if (!success) {
        PyErr_SetString(NIMCPError, "Failed to get utilization metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_utilization_metrics: failed");
        return NULL;
    }

    return Py_BuildValue("(ff)", utilization, saturation);
}

// Brain.create_from_config(filepath) -> Brain (classmethod)
static PyObject* Brain_create_from_config(PyObject* cls, PyObject* args)
{
    const char* filepath;

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_create_from_config: PyArg_ParseTuple failed");
        return NULL;
    }

    // Copy filepath since we release GIL
    char* filepath_copy = strdup(filepath);
    if (!filepath_copy) {
        return PyErr_NoMemory();
    }

    nimcp_brain_t brain;
    Py_BEGIN_ALLOW_THREADS
    brain = nimcp_brain_create_from_config(filepath_copy);
    Py_END_ALLOW_THREADS

    free(filepath_copy);

    if (!brain) {
        PyErr_SetString(NIMCPError, "Failed to create brain from config file");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_create_from_config: brain is NULL");
        return NULL;
    }

    // Create Python Brain object
    PyTypeObject* type = (PyTypeObject*)cls;
    BrainObject* brain_obj = (BrainObject*)type->tp_alloc(type, 0);
    if (!brain_obj) {
        nimcp_brain_destroy(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Brain_create_from_config: brain_obj is NULL");
        return NULL;
    }

    brain_obj->brain = brain;
    return (PyObject*)brain_obj;
}

// Brain.get_pac_modulation(theta_freq, gamma_freq) -> float
static PyObject* Brain_get_pac_modulation(BrainObject* self, PyObject* args)
{
    float theta_freq, gamma_freq;

    if (!PyArg_ParseTuple(args, "ff", &theta_freq, &gamma_freq)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_get_pac_modulation: PyArg_ParseTuple failed");
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_get_pac_modulation: self->brain is NULL");
        return NULL;
    }

    float pac = nimcp_get_pac_modulation(self->brain, theta_freq, gamma_freq);
    return PyFloat_FromDouble((double)pac);
}

// PyCapsule destructor for COW snapshots
static void Brain_snapshot_capsule_destructor(PyObject* capsule)
{
    nimcp_brain_snapshot_t snapshot =
        (nimcp_brain_snapshot_t)PyCapsule_GetPointer(capsule, "nimcp.brain_snapshot");
    if (snapshot) {
        nimcp_brain_snapshot_destroy(snapshot);
    }
}

// Brain.snapshot_cow() -> capsule
static PyObject* Brain_snapshot_cow(BrainObject* self, PyObject* Py_UNUSED(args))
{
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_snapshot_cow: self->brain is NULL");
        return NULL;
    }

    nimcp_brain_snapshot_t snapshot;

    Py_BEGIN_ALLOW_THREADS
    snapshot = nimcp_brain_snapshot_cow(self->brain);
    Py_END_ALLOW_THREADS

    if (!snapshot) {
        PyErr_SetString(NIMCPError, "Failed to create COW snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Brain_snapshot_cow: snapshot is NULL");
        return NULL;
    }

    return PyCapsule_New(snapshot, "nimcp.brain_snapshot",
                         Brain_snapshot_capsule_destructor);
}

// Brain.restore_cow(snapshot) -> bool
static PyObject* Brain_restore_cow(BrainObject* self, PyObject* args)
{
    PyObject* capsule;

    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_restore_cow: PyArg_ParseTuple failed");
        return NULL;
    }

    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_restore_cow: self->brain is NULL");
        return NULL;
    }

    if (!PyCapsule_IsValid(capsule, "nimcp.brain_snapshot")) {
        PyErr_SetString(PyExc_TypeError, "Expected a brain snapshot capsule");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_restore_cow: invalid capsule");
        return NULL;
    }

    nimcp_brain_snapshot_t snapshot =
        (nimcp_brain_snapshot_t)PyCapsule_GetPointer(capsule, "nimcp.brain_snapshot");

    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_restore_cow(self->brain, snapshot);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_OK);
}

// Brain.destroy_cow_snapshot(snapshot) -> None
static PyObject* Brain_destroy_cow_snapshot(BrainObject* Py_UNUSED(self), PyObject* args)
{
    PyObject* capsule;

    if (!PyArg_ParseTuple(args, "O", &capsule)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_destroy_cow_snapshot: PyArg_ParseTuple failed");
        return NULL;
    }

    if (!PyCapsule_IsValid(capsule, "nimcp.brain_snapshot")) {
        PyErr_SetString(PyExc_TypeError, "Expected a brain snapshot capsule");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Brain_destroy_cow_snapshot: invalid capsule");
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

// Brain.broadcast_probe() -> bool
static PyObject* Brain_broadcast_probe(BrainObject* self, PyObject* Py_UNUSED(args))
{
    if (!self->brain) {
        PyErr_SetString(NIMCPError, "Brain not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain_broadcast_probe: self->brain is NULL");
        return NULL;
    }

    nimcp_status_t status;

    Py_BEGIN_ALLOW_THREADS
    status = nimcp_brain_broadcast_probe(self->brain);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(status == NIMCP_OK);
}

static PyObject* Brain_set_training_mode(BrainObject* self, PyObject* args) {
    if (!self->brain) Py_RETURN_NONE;
    int active;
    if (!PyArg_ParseTuple(args, "p", &active)) return NULL;
    nimcp_brain_set_training_mode(self->brain, (bool)active);
    Py_RETURN_NONE;
}

static PyObject* Brain_set_network_ablation(BrainObject* self, PyObject* args, PyObject* kwds) {
    if (!self->brain) Py_RETURN_NONE;
    static char* kwlist[] = {"train_cnn", "train_snn", "train_lnn", NULL};
    int cnn = -1, snn = -1, lnn = -1;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iii", kwlist, &cnn, &snn, &lnn))
        return NULL;
    nimcp_brain_set_network_ablation(self->brain, cnn, snn, lnn);
    Py_RETURN_NONE;
}

static PyObject* Brain_get_network_metrics(BrainObject* self, PyObject* Py_UNUSED(args)) {
    if (!self->brain) Py_RETURN_NONE;
    float ema_ann = 0, ema_cnn = 0, ema_snn = 0, ema_lnn = 0;
    uint64_t ann_steps = 0, cnn_steps = 0, snn_steps = 0, lnn_steps = 0;
    if (!nimcp_brain_get_network_metrics(self->brain,
            &ema_ann, &ema_cnn, &ema_snn, &ema_lnn,
            &ann_steps, &cnn_steps, &snn_steps, &lnn_steps))
        Py_RETURN_NONE;
    return Py_BuildValue("{s:f,s:f,s:f,s:f,s:K,s:K,s:K,s:K}",
        "ann_loss", (double)ema_ann,
        "cnn_loss", (double)ema_cnn,
        "snn_loss", (double)ema_snn,
        "lnn_loss", (double)ema_lnn,
        "ann_steps", ann_steps,
        "cnn_steps", cnn_steps,
        "snn_steps", snn_steps,
        "lnn_steps", lnn_steps);
}

static PyMethodDef Brain_methods[] = {
    {"learn", (PyCFunction)Brain_learn, METH_VARARGS | METH_KEYWORDS,
     "Learn from a single example\n\n"
     "Args:\n"
     "    features (list): Input feature vector\n"
     "    label (str): Target label/class\n"
     "    confidence (float): Example confidence (0.0-1.0, default 1.0)\n"
     "Returns:\n"
     "    None\n\n"
     "Example:\n"
     "    brain.learn([0.5, 0.3, 0.8], 'cat', confidence=0.9)"},

    {"decide", (PyCFunction)Brain_decide, METH_VARARGS,
     "Make a prediction/decision\n\n"
     "Args:\n"
     "    features (list): Input feature vector\n"
     "Returns:\n"
     "    tuple: (label, confidence) - predicted label and confidence score\n\n"
     "Example:\n"
     "    label, conf = brain.decide([0.5, 0.3, 0.8])"},

    {"process", (PyCFunction)Brain_process, METH_VARARGS,
     "Process inputs and return prediction as dict\n\n"
     "Args:\n"
     "    inputs (list): Input feature vector\n"
     "Returns:\n"
     "    dict: {'label': str, 'confidence': float}\n\n"
     "Example:\n"
     "    result = brain.process([0.5, 0.3, 0.8])\n"
     "    print(result['label'], result['confidence'])"},

    {"release_dopamine", (PyCFunction)Brain_release_dopamine, METH_VARARGS | METH_KEYWORDS,
     "Release dopamine for reward-based learning\n\n"
     "Triggers dopamine release in the neuromodulator system,\n"
     "enabling reward-based learning and reinforcement.\n\n"
     "Args:\n"
     "    amount (float): Reward magnitude (0.0-1.0)\n"
     "    predicted (float): Predicted reward for TD learning (default: 0.0)\n"
     "Returns:\n"
     "    float: Amount of dopamine actually released\n\n"
     "Example:\n"
     "    brain.release_dopamine(0.8)  # Positive reward\n"
     "    brain.release_dopamine(0.5, predicted=0.3)  # TD error"},

    {"clone_cow", (PyCFunction)Brain_clone_cow, METH_NOARGS,
     "Create a copy-on-write clone of this brain\n\n"
     "Creates a lightweight clone that shares memory with the original brain.\n"
     "The clone uses copy-on-write semantics, providing 86% memory savings\n"
     "for inference-only clones and instant cloning (<10ms vs ~1000ms).\n\n"
     "Memory is only copied when either the original or clone modifies\n"
     "shared data structures. Perfect for:\n"
     "- Parallel inference on multiple inputs\n"
     "- Creating checkpoints before training\n"
     "- A/B testing different training strategies\n\n"
     "Returns:\n"
     "    Brain: A new brain instance sharing memory with the original\n\n"
     "Performance:\n"
     "    - Clone time: <10ms (vs ~1000ms for full copy)\n"
     "    - Memory overhead: ~1MB (vs ~50MB for full copy)\n"
     "    - Memory savings: 86% for read-only use\n\n"
     "Example:\n"
     "    original = nimcp.Brain('model', size=1, task=0, inputs=10, outputs=3)\n"
     "    clone = original.clone_cow()  # Shares network, 86% memory saved\n"
     "    result = clone.decide(features)  # Read-only inference works"},

    {"save", (PyCFunction)Brain_save, METH_VARARGS,
     "Save brain to file\n\n"
     "Args:\n"
     "    filepath (str): Path to save the brain\n"
     "Returns:\n"
     "    None"},

    {"load", (PyCFunction)Brain_load, METH_CLASS | METH_VARARGS,
     "Load brain from file (classmethod)\n\n"
     "Args:\n"
     "    filepath (str): Path to saved brain file\n"
     "Returns:\n"
     "    Brain: Loaded brain instance"},

    {"from_pretrained", (PyCFunction)Brain_from_pretrained, METH_CLASS | METH_VARARGS | METH_KEYWORDS,
     "Load pre-trained NIMCP model (classmethod)\n\n"
     "Loads a pre-trained brain model from the NIMCP model repository.\n"
     "Models are automatically discovered from:\n"
     "  1. NIMCP_MODELS_DIR environment variable\n"
     "  2. Source repository models/ directory\n"
     "  3. User home ~/.nimcp/models/pretrained/\n"
     "  4. System /usr/local/share/nimcp/models/pretrained/\n\n"
     "Available models:\n"
     "  - 'nimcp_foundation_small_v1.0'   - Small general-purpose (2.5K neurons, 5MB)\n"
     "  - 'nimcp_foundation_medium_v1.0'  - Medium general-purpose (10K neurons, 42MB) [RECOMMENDED]\n"
     "  - 'nimcp_foundation_large_v1.0'   - Large high-accuracy (50K neurons, 250MB)\n"
     "  - 'nimcp_ethics_medium_v1.0'      - Ethics-specialized (8K neurons, 35MB)\n"
     "  - 'nimcp_multimodal_large_v1.0'   - Multi-modal (75K neurons, 380MB)\n\n"
     "Args:\n"
     "    model_name (str): Model identifier (e.g., 'nimcp_foundation_medium_v1.0')\n"
     "    models_dir (str, optional): Custom models directory (default: auto-detect)\n"
     "Returns:\n"
     "    Brain: Pre-trained brain ready for inference or fine-tuning\n\n"
     "Example:\n"
     "    # Load pre-trained model\n"
     "    brain = nimcp.Brain.from_pretrained('nimcp_foundation_medium_v1.0')\n\n"
     "    # Use immediately for inference\n"
     "    label, confidence = brain.decide([0.5, 0.3, 0.8])\n\n"
     "    # Or fine-tune on your data\n"
     "    brain.finetune(train_data, train_labels, num_epochs=10)\n"
     "    brain.save('my_finetuned_model.nimcp')"},

    {"finetune", (PyCFunction)Brain_finetune, METH_VARARGS | METH_KEYWORDS,
     "Fine-tune pre-trained model on domain-specific data\n\n"
     "Adapts a pre-trained model to your specific task with minimal data.\n"
     "Uses selective layer freezing and lower learning rates to preserve\n"
     "pre-trained knowledge while adapting to new domain.\n\n"
     "Strategies:\n"
     "  - Quick Adaptation (10-100 examples): freeze_sensory=True, freeze_cognitive=True\n"
     "  - Domain Adaptation (100-1000 examples): freeze_sensory=False, freeze_cognitive=True\n"
     "  - Full Fine-tuning (1000+ examples): freeze_sensory=False, freeze_cognitive=False\n\n"
     "Args:\n"
     "    training_data (list): List of input examples (each example is a list of floats)\n"
     "    labels (list): List of target labels (strings or floats)\n"
     "    num_epochs (int, optional): Number of training epochs (default: 5)\n"
     "    learning_rate (float, optional): Learning rate (default: 0.001)\n"
     "    freeze_sensory (bool, optional): Freeze visual/audio cortices (default: True)\n"
     "    freeze_cognitive (bool, optional): Freeze ethics/logic modules (default: True)\n"
     "Returns:\n"
     "    None\n\n"
     "Example:\n"
     "    brain = nimcp.Brain.from_pretrained('nimcp_foundation_medium_v1.0')\n\n"
     "    # Prepare training data\n"
     "    train_data = [[0.1, 0.2, 0.3], [0.4, 0.5, 0.6], ...]  # 100 examples\n"
     "    train_labels = ['class_a', 'class_b', ...]  # 100 labels\n\n"
     "    # Quick adaptation (only fine-tune output layer)\n"
     "    brain.finetune(train_data, train_labels,\n"
     "                   num_epochs=10, learning_rate=0.001,\n"
     "                   freeze_sensory=True, freeze_cognitive=True)\n\n"
     "    # Save fine-tuned model\n"
     "    brain.save('my_finetuned_model.nimcp')"},

    {"probe", (PyCFunction)Brain_probe, METH_NOARGS,
     "Get comprehensive brain statistics\n\n"
     "Returns detailed information about brain architecture, performance,\n"
     "and resource usage including COW statistics.\n\n"
     "Returns:\n"
     "    dict: Brain statistics including neurons, synapses, accuracy,\n"
     "          memory usage, and COW sharing information"},

    /* Training Pipeline Methods */
    {"configure_training", (PyCFunction)Brain_configure_training, METH_VARARGS,
     "Configure training pipeline\n\n"
     "Sets up the internal training coordinator with specified loss function,\n"
     "optimizer, and learning rate scheduler. Must be called before using\n"
     "train_step() or train_batch().\n\n"
     "Args:\n"
     "    config (TrainingConfig): Training configuration object\n"
     "Returns:\n"
     "    None\n\n"
     "Example:\n"
     "    config = nimcp.TrainingConfig.default()\n"
     "    config.loss_type = nimcp.LOSS_CROSS_ENTROPY\n"
     "    config.optimizer_type = nimcp.OPT_ADAM\n"
     "    config.learning_rate = 0.001\n"
     "    brain.configure_training(config)"},

    {"train_step", (PyCFunction)Brain_train_step, METH_VARARGS,
     "Perform a single training step\n\n"
     "Performs a complete training step using the configured training pipeline:\n"
     "forward pass, loss computation, gradient computation, weight update.\n\n"
     "Args:\n"
     "    features (list): Input feature vector\n"
     "    targets (list): Target output vector (one-hot or continuous)\n"
     "Returns:\n"
     "    TrainingResult: Training result with loss, learning_rate, step, etc.\n\n"
     "Example:\n"
     "    features = [0.1] * 784\n"
     "    targets = [0,0,0,1,0,0,0,0,0,0]  # One-hot for class 3\n"
     "    result = brain.train_step(features, targets)\n"
     "    print(f'Loss: {result.loss:.4f}')"},

    {"train_batch", (PyCFunction)Brain_train_batch, METH_VARARGS,
     "Train on a batch of examples\n\n"
     "Performs training on multiple examples, accumulating gradients before\n"
     "applying weight updates (mini-batch gradient descent).\n\n"
     "Args:\n"
     "    features (list[list]): 2D array of features [batch_size][num_features]\n"
     "    targets (list[list]): 2D array of targets [batch_size][num_targets]\n"
     "Returns:\n"
     "    TrainingResult: Training result with averaged loss\n\n"
     "Example:\n"
     "    features = [[0.1]*784 for _ in range(32)]  # 32 images\n"
     "    targets = [[0]*10 for _ in range(32)]  # 32 one-hot labels\n"
     "    result = brain.train_batch(features, targets)"},

    {"get_training_stats", (PyCFunction)Brain_get_training_stats, METH_NOARGS,
     "Get current training statistics\n\n"
     "Returns:\n"
     "    tuple: (total_steps, total_loss, current_lr)\n\n"
     "Example:\n"
     "    steps, loss, lr = brain.get_training_stats()\n"
     "    print(f'Steps: {steps}, Total Loss: {loss:.4f}, LR: {lr:.6f}')"},

    {"step_scheduler", (PyCFunction)Brain_step_scheduler, METH_VARARGS,
     "Step the learning rate scheduler\n\n"
     "Should be called at the end of each epoch or validation step.\n"
     "For ReduceOnPlateau scheduler, pass the validation metric.\n\n"
     "Args:\n"
     "    validation_metric (float, optional): Validation metric for plateau detection\n"
     "Returns:\n"
     "    float: New learning rate after stepping\n\n"
     "Example:\n"
     "    # At end of epoch\n"
     "    new_lr = brain.step_scheduler(validation_accuracy)\n"
     "    print(f'New learning rate: {new_lr:.6f}')"},

    /* Callback Methods */
    {"enable_callbacks", (PyCFunction)Brain_enable_callbacks, METH_VARARGS,
     "Enable training callbacks\n\n"
     "Must be called after configure_training(). Enables the callback\n"
     "system with the specified configuration.\n\n"
     "Args:\n"
     "    config (CallbackConfig, optional): Callback configuration\n"
     "Returns:\n"
     "    None\n\n"
     "Example:\n"
     "    config = nimcp.CallbackConfig.default()\n"
     "    config.enable_early_stopping = True\n"
     "    config.patience = 10\n"
     "    brain.enable_callbacks(config)"},

    {"disable_callbacks", (PyCFunction)Brain_disable_callbacks, METH_NOARGS,
     "Disable training callbacks\n\n"
     "Returns:\n"
     "    None"},

    {"register_callback", (PyCFunction)Brain_register_callback, METH_VARARGS | METH_KEYWORDS,
     "Register a callback for a specific event type\n\n"
     "Args:\n"
     "    event (int): Event type (CB_STEP_COMPLETE, CB_EPOCH_COMPLETE, etc.)\n"
     "    callback (callable): Callback function(event, metrics) -> action\n"
     "    name (str, optional): Callback name for logging\n"
     "    user_data (any, optional): User data passed to callback\n"
     "Returns:\n"
     "    int: Callback ID for later unregistration\n\n"
     "Example:\n"
     "    def on_step(event, metrics):\n"
     "        print(f'Step {metrics.step}: loss={metrics.loss:.4f}')\n"
     "        if metrics.is_diverging:\n"
     "            return nimcp.CB_ACTION_REDUCE_LR\n"
     "        return nimcp.CB_ACTION_CONTINUE\n\n"
     "    cb_id = brain.register_callback(nimcp.CB_STEP_COMPLETE, on_step, 'logger')"},

    {"unregister_callback", (PyCFunction)Brain_unregister_callback, METH_VARARGS,
     "Unregister a callback\n\n"
     "Args:\n"
     "    callback_id (int): Callback ID from registration\n"
     "Returns:\n"
     "    None"},

    {"get_callback_stats", (PyCFunction)Brain_get_callback_stats, METH_NOARGS,
     "Get callback statistics\n\n"
     "Returns:\n"
     "    tuple: (total_fired, avg_time_us, early_stops)\n\n"
     "Example:\n"
     "    fired, avg_time, stops = brain.get_callback_stats()\n"
     "    print(f'Callbacks fired: {fired}, Early stops: {stops}')"},

    /* Snapshot Methods */
    {"snapshot_save", (PyCFunction)Brain_snapshot_save, METH_VARARGS | METH_KEYWORDS,
     "Save a named snapshot of the brain state\n\n"
     "Snapshots are named, timestamped backups for versioning/backup/A/B testing.\n"
     "Different from checkpoints which are auto-saved for resumption.\n\n"
     "Args:\n"
     "    name (str): Snapshot name (no path, just name)\n"
     "    description (str, optional): Description of the snapshot\n"
     "Returns:\n"
     "    None\n\n"
     "Example:\n"
     "    brain.snapshot_save('before_training', 'Baseline state')\n"
     "    # Train the model...\n"
     "    brain.snapshot_save('after_epoch_1', 'After 1 epoch')"},

    {"snapshot_restore", (PyCFunction)Brain_snapshot_restore, METH_VARARGS,
     "Restore brain from a named snapshot\n\n"
     "Args:\n"
     "    name (str): Snapshot name or full path to snapshot file\n"
     "Returns:\n"
     "    Brain: New brain instance restored from snapshot\n\n"
     "Example:\n"
     "    restored = brain.snapshot_restore('before_training')"},

    {"snapshot_list", (PyCFunction)Brain_snapshot_list, METH_VARARGS,
     "List all available snapshots\n\n"
     "Args:\n"
     "    max_count (int, optional): Maximum snapshots to list (default: 100)\n"
     "Returns:\n"
     "    list[dict]: List of snapshot info dictionaries with:\n"
     "        name, description, timestamp, file_size,\n"
     "        is_compressed, is_encrypted\n\n"
     "Example:\n"
     "    for snap in brain.snapshot_list():\n"
     "        print(f'{snap[\"name\"]}: {snap[\"description\"]}')"},

    {"snapshot_delete", (PyCFunction)Brain_snapshot_delete, METH_VARARGS,
     "Delete a named snapshot\n\n"
     "Args:\n"
     "    name (str): Snapshot name to delete\n"
     "Returns:\n"
     "    None"},

    /* Working Memory Methods */
    {"working_memory_add", (PyCFunction)Brain_working_memory_add, METH_VARARGS,
     "Add item to working memory\n\n"
     "Args:\n"
     "    data (list): Item data (feature vector)\n"
     "    salience (float): Initial salience (0.0-1.0)\n"
     "Returns:\n"
     "    None"},

    {"working_memory_get", (PyCFunction)Brain_working_memory_get, METH_VARARGS,
     "Get item from working memory by index\n\n"
     "Args:\n"
     "    index (int): Item index (0 = highest salience)\n"
     "Returns:\n"
     "    tuple: (data, size) or None if invalid index"},

    {"working_memory_stats", (PyCFunction)Brain_working_memory_stats, METH_NOARGS,
     "Get working memory statistics\n\n"
     "Returns:\n"
     "    tuple: (current_size, capacity)"},

    {"working_memory_refresh", (PyCFunction)Brain_working_memory_refresh, METH_VARARGS,
     "Refresh item in working memory (prevent decay)\n\n"
     "Args:\n"
     "    index (int): Item index to refresh\n"
     "Returns:\n"
     "    None"},

    /* Global Workspace Methods */
    {"workspace_compete", (PyCFunction)Brain_workspace_compete, METH_VARARGS,
     "Compete for conscious access in global workspace\n\n"
     "Args:\n"
     "    module (int): Source module identifier\n"
     "    content (list): Content vector\n"
     "    strength (float): Competition strength (0.0-1.0)\n"
     "Returns:\n"
     "    bool: True if won competition and broadcast"},

    {"workspace_read", (PyCFunction)Brain_workspace_read, METH_VARARGS,
     "Read current global workspace broadcast\n\n"
     "Args:\n"
     "    max_dim (int, optional): Maximum buffer size (default: 256)\n"
     "Returns:\n"
     "    tuple: (content, dim, source_module) or None if no broadcast"},

    {"workspace_subscribe", (PyCFunction)Brain_workspace_subscribe, METH_VARARGS,
     "Subscribe module to workspace broadcasts\n\n"
     "Args:\n"
     "    module (int): Module to subscribe\n"
     "Returns:\n"
     "    None"},

    {"workspace_unsubscribe", (PyCFunction)Brain_workspace_unsubscribe, METH_VARARGS,
     "Unsubscribe module from workspace broadcasts\n\n"
     "Args:\n"
     "    module (int): Module to unsubscribe\n"
     "Returns:\n"
     "    None"},

    {"workspace_has_broadcast", (PyCFunction)Brain_workspace_has_broadcast, METH_NOARGS,
     "Check if workspace has active broadcast\n\n"
     "Returns:\n"
     "    bool: True if broadcast active"},

    {"workspace_stats", (PyCFunction)Brain_workspace_stats, METH_NOARGS,
     "Get workspace statistics\n\n"
     "Returns:\n"
     "    tuple: (total_broadcasts, total_competitions, avg_strength)"},

    /* Complex Oscillation Methods */
    {"enable_complex_oscillations", (PyCFunction)Brain_enable_complex_oscillations, METH_VARARGS,
     "Enable or disable complex oscillation features\n\n"
     "When enabled: neurons track oscillatory phase and amplitude,\n"
     "synapses compute phase-dependent weights, ~15% memory overhead.\n\n"
     "Args:\n"
     "    enable (bool): True to enable, False to disable\n"
     "Returns:\n"
     "    bool: True on success"},

    {"is_complex_oscillations_enabled", (PyCFunction)Brain_is_complex_oscillations_enabled, METH_NOARGS,
     "Check if complex oscillation features are enabled\n\n"
     "Returns:\n"
     "    bool: True if enabled"},

    {"get_oscillation_phasor", (PyCFunction)Brain_get_oscillation_phasor, METH_VARARGS,
     "Get oscillation phasor for a specific neuron\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron identifier\n"
     "Returns:\n"
     "    tuple: (amplitude, phase) in radians"},

    {"get_phase_coherence", (PyCFunction)Brain_get_phase_coherence, METH_VARARGS,
     "Compute phase coherence across multiple neurons\n\n"
     "Args:\n"
     "    neuron_ids (list): List of neuron identifiers\n"
     "Returns:\n"
     "    float: Phase coherence value [0, 1]"},

    {"get_pac_modulation", (PyCFunction)Brain_get_pac_modulation, METH_VARARGS,
     "Compute phase-amplitude coupling (PAC) modulation index\n\n"
     "Measures theta-gamma cross-frequency coupling strength.\n"
     "Requires complex oscillations to be enabled.\n\n"
     "Args:\n"
     "    theta_freq (float): Theta frequency in Hz (typically 4-8)\n"
     "    gamma_freq (float): Gamma frequency in Hz (typically 30-100)\n"
     "Returns:\n"
     "    float: PAC modulation index [0, 1]\n\n"
     "Example:\n"
     "    pac = brain.get_pac_modulation(6.0, 40.0)\n"
     "    if pac > 0.4:\n"
     "        print('Strong theta-gamma coupling')"},

    /* Dynamic Brain Resizing Methods */
    {"resize", (PyCFunction)Brain_resize, METH_VARARGS,
     "Manually resize brain to a specific neuron count\n\n"
     "Args:\n"
     "    new_neuron_count (int): Target number of neurons\n"
     "Returns:\n"
     "    bool: True on success, False on failure\n\n"
     "Example:\n"
     "    brain.resize(5000)  # Resize to 5000 neurons"},

    {"auto_resize", (PyCFunction)Brain_auto_resize, METH_NOARGS,
     "Automatically resize brain based on hardware and utilization\n\n"
     "Returns:\n"
     "    bool: True if resized, False if no resize needed"},

    {"get_neuron_count", (PyCFunction)Brain_get_neuron_count, METH_NOARGS,
     "Get current neuron count\n\n"
     "Returns:\n"
     "    int: Current number of neurons"},

    {"get_utilization_metrics", (PyCFunction)Brain_get_utilization_metrics, METH_NOARGS,
     "Get brain utilization metrics\n\n"
     "Returns:\n"
     "    tuple: (utilization, saturation) - both floats 0.0-1.0\n\n"
     "Example:\n"
     "    util, sat = brain.get_utilization_metrics()\n"
     "    print(f'Utilization: {util:.1%}, Saturation: {sat:.1%}')"},

    /* Raw Inference */
    {"infer", (PyCFunction)Brain_infer, METH_VARARGS,
     "Run raw inference and get output vector\n\n"
     "For numeric predictions, embeddings, or when you don't need\n"
     "label classification. Returns raw network output activations.\n\n"
     "Args:\n"
     "    features (list): Input feature vector\n"
     "    num_outputs (int): Number of outputs expected\n"
     "Returns:\n"
     "    list[float]: Raw output vector\n\n"
     "Example:\n"
     "    outputs = brain.infer([0.5, 0.3, 0.8, 0.1], 3)\n"
     "    print(f'Outputs: {outputs}')"},

    /* Config File Loading */
    {"create_from_config", (PyCFunction)Brain_create_from_config, METH_CLASS | METH_VARARGS,
     "Create brain from YAML or JSON config file (classmethod)\n\n"
     "Supports loading brain configuration from YAML (.yaml, .yml)\n"
     "or JSON (.json) files including architecture, training params,\n"
     "plasticity settings, and ethics.\n\n"
     "Args:\n"
     "    filepath (str): Path to configuration file\n"
     "Returns:\n"
     "    Brain: New brain instance\n\n"
     "Example:\n"
     "    brain = nimcp.Brain.create_from_config('model.yaml')"},

    /* COW Snapshot API */
    {"snapshot_cow", (PyCFunction)Brain_snapshot_cow, METH_NOARGS,
     "Create an instant copy-on-write snapshot\n\n"
     "Creates a lightweight snapshot that shares memory with the brain.\n"
     "Only pages that are modified after the snapshot are copied.\n\n"
     "Returns:\n"
     "    capsule: Opaque snapshot handle (pass to restore_cow/destroy_cow_snapshot)\n\n"
     "Example:\n"
     "    checkpoint = brain.snapshot_cow()\n"
     "    # ... train ...\n"
     "    brain.restore_cow(checkpoint)\n"
     "    brain.destroy_cow_snapshot(checkpoint)"},

    {"restore_cow", (PyCFunction)Brain_restore_cow, METH_VARARGS,
     "Restore brain state from a COW snapshot\n\n"
     "Instantly restores brain to the snapshot state via pointer swapping.\n\n"
     "Args:\n"
     "    snapshot (capsule): Snapshot from snapshot_cow()\n"
     "Returns:\n"
     "    bool: True on success"},

    {"destroy_cow_snapshot", (PyCFunction)Brain_destroy_cow_snapshot, METH_VARARGS,
     "Destroy a COW snapshot and release references\n\n"
     "Frees resources held by the snapshot. The capsule becomes invalid.\n"
     "Note: Snapshots are also auto-destroyed when garbage collected.\n\n"
     "Args:\n"
     "    snapshot (capsule): Snapshot to destroy"},

    /* Probe Broadcasting */
    {"broadcast_probe", (PyCFunction)Brain_broadcast_probe, METH_NOARGS,
     "Probe brain metrics and broadcast via bio-async\n\n"
     "Collects brain health metrics and broadcasts them to all\n"
     "subscribed monitoring systems using the bio-async router.\n\n"
     "Returns:\n"
     "    bool: True on success"},

    {"decide_full", (PyCFunction)Brain_decide_full, METH_VARARGS,
     "Run full cognitive pipeline and return complete decision data\n\n"
     "Args:\n"
     "    features (list): Input feature vector\n"
     "Returns:\n"
     "    dict: {label, confidence, explanation, output_vector,\n"
     "           num_active_neurons, sparsity, inference_time_us}"},

    {"set_active_modalities", (PyCFunction)Brain_set_active_modalities, METH_VARARGS,
     "Set which sensory modalities are active for SNN encoding\n\n"
     "Controls which SNN sensory bridges fire during decide().\n"
     "Text-only input (default=1) skips visual/audio/somatosensory bridges.\n\n"
     "Modality flags (bitmask):\n"
     "    TEXT=0x01, VISUAL=0x02, AUDIO=0x04, SOMATOSENSORY=0x08, SPEECH=0x10\n\n"
     "Args:\n"
     "    flags (int): Bitmask of active modalities\n"
     "Example:\n"
     "    brain.set_active_modalities(0x01)  # text only (default)\n"
     "    brain.set_active_modalities(0x1F)  # all modalities"},

    {"get_active_modalities", (PyCFunction)Brain_get_active_modalities, METH_NOARGS,
     "Get current active modality bitmask\n\n"
     "Returns:\n"
     "    int: Bitmask of active modalities"},

    {"submit_sensory", (PyCFunction)Brain_submit_sensory, METH_VARARGS | METH_KEYWORDS,
     "Stage raw sensory data for the next decide() call\n\n"
     "Data is consumed (freed) after the next decide() cycle.\n"
     "Automatically enables the modality bit.\n\n"
     "Args:\n"
     "    modality (str): 'visual', 'audio', 'somatosensory', or 'speech'\n"
     "    data: bytes, bytearray, or list of floats\n"
     "    width (int): Frame width (visual only)\n"
     "    height (int): Frame height (visual only)\n"
     "    channels (int): Color channels (visual only)\n\n"
     "Example:\n"
     "    brain.submit_sensory('audio', audio_samples)  # list of floats\n"
     "    brain.submit_sensory('visual', pixels, width=64, height=64, channels=3)"},

    {"get_network_metrics", (PyCFunction)Brain_get_network_metrics, METH_NOARGS,
     "Get per-network training metrics for ablation analysis\n\n"
     "Returns:\n"
     "    dict: {ann_loss, cnn_loss, snn_loss, lnn_loss, ann_steps, cnn_steps, ...}"},

    {"set_training_mode", (PyCFunction)Brain_set_training_mode, METH_VARARGS,
     "Enable/disable training-mode fast path in brain_decide()\n\n"
     "Args:\n"
     "    active (bool): True = skip expensive cognitive modules during decide()"},

    {"set_network_ablation", (PyCFunction)Brain_set_network_ablation,
     METH_VARARGS | METH_KEYWORDS,
     "Enable/disable individual network types for ablation studies\n\n"
     "Args:\n"
     "    train_cnn (bool): Enable CNN training\n"
     "    train_snn (bool): Enable SNN training\n"
     "    train_lnn (bool): Enable LNN training"},

    {NULL, NULL, 0, NULL}
};

PyTypeObject BrainType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.Brain",
    .tp_doc = "NIMCP Brain - High-level learning system\n\n"
              "A brain is a complete learning system with neural networks,\n"
              "plasticity mechanisms, and knowledge integration.\n\n"
              "Args:\n"
              "    name (str): Human-readable name (e.g., 'classifier', 'ethics')\n"
              "    size (int): Brain size (0=TINY, 1=SMALL, 2=MEDIUM, 3=LARGE)\n"
              "    task (int): Task type (0=CLASSIFICATION, 1=REGRESSION, 2=PATTERN_MATCHING, 3=SEQUENCE, 4=ASSOCIATION)\n"
              "    inputs (int): Number of input features\n"
              "    outputs (int): Number of output classes/values\n\n"
              "Example:\n"
              "    brain = nimcp.Brain('classifier', size=1, task=0, inputs=10, outputs=3)",
    .tp_basicsize = sizeof(BrainObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = Brain_new,
    .tp_init = (initproc) Brain_init,
    .tp_dealloc = (destructor) Brain_dealloc,
    .tp_methods = Brain_methods,
};

//=============================================================================
// BrainConfig Type
//=============================================================================

/**
 * @brief Map task name string to enum
 */
static nimcp_brain_task_t task_name_to_enum(const char* name)
{
    if (strcmp(name, "classification") == 0) return NIMCP_TASK_CLASSIFICATION;
    if (strcmp(name, "regression") == 0) return NIMCP_TASK_REGRESSION;
    if (strcmp(name, "pattern_matching") == 0) return NIMCP_TASK_PATTERN_MATCHING;
    if (strcmp(name, "sequence") == 0) return NIMCP_TASK_SEQUENCE;
    if (strcmp(name, "association") == 0) return NIMCP_TASK_ASSOCIATION;
    return NIMCP_TASK_CLASSIFICATION;  // Default
}

/**
 * @brief Initialize BrainConfig from constructor arguments
 *
 * SIGNATURE: BrainConfig(num_inputs, num_outputs, hidden_layers=None, task_name="classification")
 */
static int BrainConfig_init(BrainConfigObject* self, PyObject* args, PyObject* kwds)
{
    unsigned int num_inputs, num_outputs;
    PyObject* hidden_layers = NULL;
    const char* task_name = "classification";

    static char* kwlist[] = {"num_inputs", "num_outputs", "hidden_layers", "task_name", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "II|Os", kwlist,
                                     &num_inputs, &num_outputs, &hidden_layers, &task_name)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BrainConfig_init: operation failed");
        return -1;
    }

    self->num_inputs = num_inputs;
    self->num_outputs = num_outputs;
    strncpy(self->task_name, task_name, sizeof(self->task_name) - 1);
    self->task_name[sizeof(self->task_name) - 1] = '\0';
    self->task = task_name_to_enum(task_name);

    // Determine brain size from hidden_layers
    if (hidden_layers && PyList_Check(hidden_layers)) {
        Py_ssize_t num_hidden = PyList_Size(hidden_layers);
        Py_INCREF(hidden_layers);
        self->hidden_layers = hidden_layers;

        // Estimate size from total neurons
        int total_neurons = (int)(num_inputs + num_outputs);
        for (Py_ssize_t i = 0; i < num_hidden; i++) {
            PyObject* item = PyList_GetItem(hidden_layers, i);
            total_neurons += (int)PyLong_AsLong(item);
        }

        if (total_neurons < 100) self->size = NIMCP_BRAIN_TINY;
        else if (total_neurons < 1000) self->size = NIMCP_BRAIN_SMALL;
        else if (total_neurons < 10000) self->size = NIMCP_BRAIN_MEDIUM;
        else self->size = NIMCP_BRAIN_LARGE;
    } else {
        self->hidden_layers = NULL;
        self->size = NIMCP_BRAIN_SMALL;  // Default
    }

    return 0;
}

static void BrainConfig_dealloc(BrainConfigObject* self)
{
    Py_XDECREF(self->hidden_layers);
    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* BrainConfig_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    BrainConfigObject* self = (BrainConfigObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->num_inputs = 0;
        self->num_outputs = 0;
        self->hidden_layers = NULL;
        self->task_name[0] = '\0';
        self->size = NIMCP_BRAIN_SMALL;
        self->task = NIMCP_TASK_CLASSIFICATION;
    }
    return (PyObject*) self;
}

static PyMemberDef BrainConfig_members[] = {
    {"num_inputs", T_UINT, offsetof(BrainConfigObject, num_inputs), 0, "Number of input features"},
    {"num_outputs", T_UINT, offsetof(BrainConfigObject, num_outputs), 0, "Number of output classes/values"},
    {"hidden_layers", T_OBJECT_EX, offsetof(BrainConfigObject, hidden_layers), 0, "List of hidden layer sizes"},
    {NULL}
};

static PyObject* BrainConfig_get_task_name(BrainConfigObject* self, void* closure)
{
    return PyUnicode_FromString(self->task_name);
}

static PyGetSetDef BrainConfig_getset[] = {
    {"task_name", (getter)BrainConfig_get_task_name, NULL, "Task type name", NULL},
    {NULL}
};

PyTypeObject BrainConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.BrainConfig",
    .tp_doc = "NIMCP Brain Configuration\n\n"
              "Configuration object for creating brains with custom parameters.\n\n"
              "Args:\n"
              "    num_inputs (int): Number of input features\n"
              "    num_outputs (int): Number of output classes/values\n"
              "    hidden_layers (list): Optional list of hidden layer sizes\n"
              "    task_name (str): Task type ('classification', 'regression', etc.)\n\n"
              "Example:\n"
              "    config = nimcp.BrainConfig(\n"
              "        num_inputs=10,\n"
              "        num_outputs=5,\n"
              "        hidden_layers=[20, 15],\n"
              "        task_name='classification'\n"
              "    )\n"
              "    brain = nimcp.Brain(config)",
    .tp_basicsize = sizeof(BrainConfigObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = BrainConfig_new,
    .tp_init = (initproc) BrainConfig_init,
    .tp_dealloc = (destructor) BrainConfig_dealloc,
    .tp_members = BrainConfig_members,
    .tp_getset = BrainConfig_getset,
};

//=============================================================================
// NeuralNetwork Type
//=============================================================================

/**
 * @brief Initializes NeuralNetwork object from constructor arguments
 *
 * WHY: Python's type system calls tp_new to allocate, then tp_init to initialize.
 * This function creates the actual C neural network from Python arguments.
 *
 * SIGNATURE: NeuralNetwork(num_neurons)
 */
static int NeuralNetwork_init(NeuralNetworkObject* self, PyObject* args, PyObject* kwds)
{
    int num_neurons;

    // Parse constructor arguments
    if (!PyArg_ParseTuple(args, "i", &num_neurons)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuralNetwork_init: PyArg_ParseTuple is NULL");
        return -1;
    }

    // Create default configuration
    network_config_t config = {.num_neurons = num_neurons,
                               .ei_ratio = 0.8F,
                               .learning_rate = 0.01F,
                               .hebbian_rate = 0.1F,
                               .stdp_window = 20.0F,
                               .homeostatic_rate = 0.001F,
                               .target_activity = 0.1F,
                               .adaptation_rate = 0.1F,
                               .refractory_period = 5.0F,
                               .min_weight = -1.0F,
                               .max_weight = 1.0F,
                               .update_interval = 1000,
                               .input_size = 10,
                               .output_size = 10,
                               .num_layers = 0,
                               .layer_sizes = NULL,
                               .enable_stdp = true,
                               .enable_hebbian = true,
                               .enable_oja = true,
                               .enable_homeostasis = true};

    // Create the neural network
    self->network = neural_network_create(&config);
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create neural network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_init: self->network is NULL");
        return -1;
    }

    return 0;
}

static void NeuralNetwork_dealloc(NeuralNetworkObject* self)
{
    if (self->network) {
        neural_network_destroy(self->network);
    }
    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* NeuralNetwork_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    NeuralNetworkObject* self = (NeuralNetworkObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->network = NULL;
    }
    return (PyObject*) self;
}

// NeuralNetwork.forward(inputs, num_outputs) -> list
static PyObject* NeuralNetwork_forward(NeuralNetworkObject* self, PyObject* args)
{
    PyObject* input_list;
    int num_outputs;

    if (!PyArg_ParseTuple(args, "Oi", &input_list, &num_outputs)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_forward: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Convert Python list to C array
    if (!PyList_Check(input_list)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_forward: PyList_Check is NULL");
        return NULL;
    }

    if (num_outputs <= 0) {
        PyErr_SetString(PyExc_ValueError, "num_outputs must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_forward: validation failed");
        return NULL;
    }

    Py_ssize_t input_size = PyList_Size(input_list);
    if (input_size <= 0) {
        PyErr_SetString(PyExc_ValueError, "Input list must not be empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuralNetwork_forward: validation failed");
        return NULL;
    }

    float* inputs = (float*)nimcp_malloc(sizeof(float) * (size_t)input_size);
    if (!inputs) {
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < input_size; i++) {
        PyObject* item = PyList_GetItem(input_list, i);
        inputs[i] = (float)PyFloat_AsDouble(item);
        if (PyErr_Occurred()) {
            nimcp_free(inputs);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_forward: validation failed");
            return NULL;
        }
    }

    // Allocate output array
    float* outputs = (float*)nimcp_malloc(sizeof(float) * (size_t)num_outputs);
    if (!outputs) {
        nimcp_free(inputs);
        return PyErr_NoMemory();
    }

    // Release GIL during potentially long-running forward pass
    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = neural_network_forward(self->network, inputs, input_size, outputs, num_outputs);
    Py_END_ALLOW_THREADS

    if (!success) {
        nimcp_free(inputs);
        nimcp_free(outputs);
        PyErr_SetString(NetworkError, "Forward pass failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_forward: success is NULL");
        return NULL;
    }

    // Convert output array to Python list
    PyObject* result = PyList_New(num_outputs);
    if (!result) {
        nimcp_free(inputs);
        nimcp_free(outputs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "NeuralNetwork_forward: result is NULL");
        return NULL;
    }

    for (int i = 0; i < num_outputs; i++) {
        PyObject* item = PyFloat_FromDouble(outputs[i]);
        if (!item) {
            Py_DECREF(result);
            nimcp_free(inputs);
            nimcp_free(outputs);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_forward: item is NULL");
            return NULL;
        }
        // PyList_SetItem steals reference on success, so don't decref item
        if (PyList_SetItem(result, i, item) < 0) {
            // On failure, item is NOT stolen, so we must decref it
            Py_DECREF(item);
            Py_DECREF(result);
            nimcp_free(inputs);
            nimcp_free(outputs);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_forward: validation failed");
            return NULL;
        }
    }

    nimcp_free(inputs);
    nimcp_free(outputs);

    return result;
}

// NeuralNetwork.add_connection(from_id, to_id, weight) -> bool
static PyObject* NeuralNetwork_add_connection(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int from_id, to_id;
    float weight;

    if (!PyArg_ParseTuple(args, "IIf", &from_id, &to_id, &weight)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_add_connection: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = neural_network_add_connection(self->network, from_id, to_id, weight);

    if (!success) {
        PyErr_SetString(NetworkError, "Failed to add connection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_add_connection: success is NULL");
        return NULL;
    }

    Py_RETURN_TRUE;
}

// NeuralNetwork.update_neuron(neuron_id, new_state, timestamp) -> bool
static PyObject* NeuralNetwork_update_neuron(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    float new_state;
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "IfK", &neuron_id, &new_state, &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "NeuralNetwork_update_neuron: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = neural_network_update_neuron(self->network, neuron_id, new_state, timestamp);

    if (!success) {
        PyErr_SetString(NetworkError, "Failed to update neuron");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "NeuralNetwork_update_neuron: success is NULL");
        return NULL;
    }

    Py_RETURN_TRUE;
}

// NeuralNetwork.get_neuron_state(neuron_id) -> float
static PyObject* NeuralNetwork_get_neuron_state(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    float state;

    if (!PyArg_ParseTuple(args, "I", &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_get_neuron_state: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = neural_network_get_neuron_state(self->network, neuron_id, &state);

    if (!success) {
        PyErr_SetString(NetworkError, "Failed to get neuron state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_get_neuron_state: success is NULL");
        return NULL;
    }

    return PyFloat_FromDouble(state);
}

// NeuralNetwork.compute_step(timestamp) -> int
static PyObject* NeuralNetwork_compute_step(NeuralNetworkObject* self, PyObject* args)
{
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "K", &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_compute_step: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t active_neurons = neural_network_compute_step(self->network, timestamp);

    return PyLong_FromUnsignedLong(active_neurons);
}

// NeuralNetwork.reset() -> None
static PyObject* NeuralNetwork_reset(NeuralNetworkObject* self, PyObject* Py_UNUSED(ignored))
{
    neural_network_reset(self->network);
    Py_RETURN_NONE;
}

// NeuralNetwork.add_neuron(activation_type) -> int
static PyObject* NeuralNetwork_add_neuron(NeuralNetworkObject* self, PyObject* args)
{
    int activation_type;

    if (!PyArg_ParseTuple(args, "i", &activation_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_add_neuron: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t neuron_id = neural_network_add_neuron(self->network, (activation_type_t)activation_type);
    return PyLong_FromUnsignedLong(neuron_id);
}

// NeuralNetwork.apply_stdp(neuron_id, timestamp) -> int
static PyObject* NeuralNetwork_apply_stdp(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "IK", &neuron_id, &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_apply_stdp: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t count = neural_network_apply_stdp(self->network, neuron_id, timestamp);
    return PyLong_FromUnsignedLong(count);
}

// NeuralNetwork.apply_oja(neuron_id, timestamp) -> int
static PyObject* NeuralNetwork_apply_oja(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "IK", &neuron_id, &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_apply_oja: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t count = neural_network_apply_oja(self->network, neuron_id, timestamp);
    return PyLong_FromUnsignedLong(count);
}

// NeuralNetwork.apply_homeostasis(neuron_id, timestamp) -> bool
static PyObject* NeuralNetwork_apply_homeostasis(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "IK", &neuron_id, &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_apply_homeostasis: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = neural_network_apply_homeostasis(self->network, neuron_id, timestamp);
    if (success) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

// NeuralNetwork.normalize_weights(neuron_id) -> bool
static PyObject* NeuralNetwork_normalize_weights(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;

    if (!PyArg_ParseTuple(args, "I", &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_normalize_weights: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = neural_network_normalize_weights(self->network, neuron_id);
    if (success) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

// NeuralNetwork.record_spike(neuron_id, magnitude, timestamp) -> bool
static PyObject* NeuralNetwork_record_spike(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    float magnitude;
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "IfK", &neuron_id, &magnitude, &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_record_spike: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = neural_network_record_spike(self->network, neuron_id, magnitude, timestamp);
    if (success) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

// NeuralNetwork.get_average_activity(neuron_id) -> float
static PyObject* NeuralNetwork_get_average_activity(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;

    if (!PyArg_ParseTuple(args, "I", &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_get_average_activity: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float activity = neural_network_get_average_activity(self->network, neuron_id);
    return PyFloat_FromDouble(activity);
}

// NeuralNetwork.get_weight_norm(neuron_id) -> float
static PyObject* NeuralNetwork_get_weight_norm(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;

    if (!PyArg_ParseTuple(args, "I", &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_get_weight_norm: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float norm = neural_network_get_weight_norm(self->network, neuron_id);
    return PyFloat_FromDouble(norm);
}

// NeuralNetwork.get_weight_statistics(neuron_id) -> (mean, std_dev)
static PyObject* NeuralNetwork_get_weight_statistics(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    float mean, std_dev;

    if (!PyArg_ParseTuple(args, "I", &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_get_weight_statistics: PyArg_ParseTuple is NULL");
        return NULL;
    }

    neural_network_get_weight_statistics(self->network, neuron_id, &mean, &std_dev);

    return Py_BuildValue("(ff)", mean, std_dev);
}

// NeuralNetwork.maintain_homeostasis(timestamp) -> None
static PyObject* NeuralNetwork_maintain_homeostasis(NeuralNetworkObject* self, PyObject* args)
{
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "K", &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_maintain_homeostasis: PyArg_ParseTuple is NULL");
        return NULL;
    }

    neural_network_maintain_homeostasis(self->network, timestamp);
    Py_RETURN_NONE;
}

// NeuralNetwork.prune_synapses(threshold) -> int
static PyObject* NeuralNetwork_prune_synapses(NeuralNetworkObject* self, PyObject* args)
{
    float threshold;

    if (!PyArg_ParseTuple(args, "f", &threshold)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_prune_synapses: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t pruned = neural_network_prune_synapses(self->network, threshold);
    return PyLong_FromUnsignedLong(pruned);
}

// NeuralNetwork.get_incoming_synapse_count(neuron_id) -> int
static PyObject* NeuralNetwork_get_incoming_synapse_count(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;

    if (!PyArg_ParseTuple(args, "I", &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_get_incoming_synapse_count: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t count = neural_network_get_incoming_synapse_count(self->network, neuron_id);
    return PyLong_FromUnsignedLong(count);
}

// NeuralNetwork.update_plasticity(neuron_id, timestamp) -> int
static PyObject* NeuralNetwork_update_plasticity(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "IK", &neuron_id, &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_update_plasticity: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t updates = neural_network_update_plasticity(self->network, neuron_id, timestamp);
    return PyLong_FromUnsignedLong(updates);
}

// NeuralNetwork.adapt_threshold(neuron_id, timestamp) -> bool
static PyObject* NeuralNetwork_adapt_threshold(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    unsigned long long timestamp;

    if (!PyArg_ParseTuple(args, "IK", &neuron_id, &timestamp)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_adapt_threshold: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = neural_network_adapt_threshold(self->network, neuron_id, timestamp);
    if (success) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

// NeuralNetwork.set_neuron_model(neuron_id, model_type) -> bool
static PyObject* NeuralNetwork_set_neuron_model(NeuralNetworkObject* self, PyObject* args)
{
    unsigned int neuron_id;
    int model_type;

    // Parse arguments: neuron_id (required), model_type (required)
    // Note: params parameter not yet supported, defaults used
    if (!PyArg_ParseTuple(args, "Ii", &neuron_id, &model_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_set_neuron_model: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Validate model_type range
    if (model_type < 0 || model_type > 3) {
        PyErr_SetString(PyExc_ValueError, "Invalid model_type: must be 0-3 (LIF, Izhikevich, AdEx, Hodgkin-Huxley)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_set_neuron_model: validation failed");
        return NULL;
    }

    // Call C function with NULL params (use defaults)
    bool success = neural_network_set_neuron_model(
        self->network,
        neuron_id,
        (neuron_model_type_t)model_type,
        NULL  // Use default parameters for the model
    );

    if (success) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

// NeuralNetwork.serialize(compress=True, password=None) -> bytes
static PyObject* NeuralNetwork_serialize(NeuralNetworkObject* self, PyObject* args, PyObject* kwargs)
{
    int compress = 1;  // Default to compressed
    const char* password = NULL;
    Py_ssize_t password_len = 0;
    static char* kwlist[] = {"compress", "password", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|pz#", kwlist, &compress, &password, &password_len)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuralNetwork_serialize: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    // Create serializer
    NimcpSerializer* serializer = nimcp_serializer_create(0);
    if (!serializer) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create serializer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "NeuralNetwork_serialize: serializer is NULL");
        return NULL;
    }

    // Copy password if provided since we release GIL
    char* password_copy = NULL;
    if (password && password_len > 0) {
        password_copy = (char*)nimcp_malloc((size_t)password_len + 1);
        if (!password_copy) {
            nimcp_serializer_destroy(serializer);
            return PyErr_NoMemory();
        }
        memcpy(password_copy, password, (size_t)password_len);
        password_copy[password_len] = '\0';
    }

    // Release GIL during potentially long serialization
    nimcp_serial_stats_t stats;
    nimcp_network_serial_result_t result;
    Py_BEGIN_ALLOW_THREADS
    result = nimcp_network_serialize(
        self->network,
        serializer,
        compress,
        password_copy,
        password_copy ? (size_t)password_len : 0,
        &stats
    );
    Py_END_ALLOW_THREADS

    nimcp_free(password_copy);

    if (result != NIMCP_NETWORK_SERIAL_SUCCESS) {
        nimcp_serializer_destroy(serializer);
        PyErr_Format(NetworkError, "Serialization failed: %s",
                     nimcp_network_serial_strerror(result));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_serialize: validation failed");
        return NULL;
    }

    // Get serialized data
    size_t data_length = nimcp_serializer_get_length(serializer);
    const uint8_t* data = nimcp_serializer_get_buffer(serializer);

    // Create Python bytes object
    PyObject* bytes_obj = PyBytes_FromStringAndSize((const char*)data, (Py_ssize_t)data_length);

    nimcp_serializer_destroy(serializer);

    return bytes_obj;
}

// NeuralNetwork.deserialize(data, password=None) -> NeuralNetwork (classmethod)
static PyObject* NeuralNetwork_deserialize(PyObject* cls, PyObject* args, PyObject* kwargs)
{
    const char* data;
    Py_ssize_t data_length;
    const char* password = NULL;
    Py_ssize_t password_len = 0;
    static char* kwlist[] = {"data", "password", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "y#|z#", kwlist, &data, &data_length, &password, &password_len)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuralNetwork_deserialize: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    // Create serializer and load data
    NimcpSerializer* serializer = nimcp_serializer_create((size_t)data_length);
    if (!serializer) {
        PyErr_SetString(PyExc_MemoryError, "Failed to create serializer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "NeuralNetwork_deserialize: serializer is NULL");
        return NULL;
    }

    if (!nimcp_serializer_set_buffer(serializer, (uint8_t*)data, (size_t)data_length)) {
        nimcp_serializer_destroy(serializer);
        PyErr_SetString(PyExc_ValueError, "Failed to set serializer buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuralNetwork_deserialize: nimcp_serializer_set_buffer is NULL");
        return NULL;
    }

    // Copy password if provided since we release GIL
    char* password_copy = NULL;
    if (password && password_len > 0) {
        password_copy = (char*)nimcp_malloc((size_t)password_len + 1);
        if (!password_copy) {
            nimcp_serializer_destroy(serializer);
            return PyErr_NoMemory();
        }
        memcpy(password_copy, password, (size_t)password_len);
        password_copy[password_len] = '\0';
    }

    // Release GIL during potentially long deserialization
    neural_network_t network = NULL;
    nimcp_serial_stats_t stats;
    nimcp_network_serial_result_t result;
    Py_BEGIN_ALLOW_THREADS
    result = nimcp_network_deserialize(
        serializer,
        &network,
        password_copy,
        password_copy ? (size_t)password_len : 0,
        &stats
    );
    Py_END_ALLOW_THREADS

    nimcp_serializer_destroy(serializer);
    nimcp_free(password_copy);

    if (result != NIMCP_NETWORK_SERIAL_SUCCESS) {
        PyErr_Format(NetworkError, "Deserialization failed: %s",
                     nimcp_network_serial_strerror(result));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NeuralNetwork_deserialize: validation failed");
        return NULL;
    }

    // Create Python NeuralNetwork object WITHOUT calling __init__
    // Use tp_alloc directly to avoid __init__ validation
    PyTypeObject* type = (PyTypeObject*)cls;
    NeuralNetworkObject* py_network = (NeuralNetworkObject*)type->tp_alloc(type, 0);
    if (!py_network) {
        neural_network_destroy(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "NeuralNetwork_deserialize: py_network is NULL");
        return NULL;
    }

    // Assign the deserialized network
    py_network->network = network;

    return (PyObject*)py_network;
}

// NeuralNetwork.train(inputs, targets) -> bool
static PyObject* NeuralNetwork_train(NeuralNetworkObject* self, PyObject* args)
{
    PyObject* input_list;
    PyObject* target_list;

    if (!PyArg_ParseTuple(args, "OO", &input_list, &target_list)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuralNetwork_train: PyArg_ParseTuple failed");
        return NULL;
    }

    if (!PyList_Check(input_list) || !PyList_Check(target_list)) {
        PyErr_SetString(PyExc_TypeError, "Both inputs and targets must be lists");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuralNetwork_train: non-list argument");
        return NULL;
    }

    Py_ssize_t num_inputs = PyList_Size(input_list);
    Py_ssize_t num_targets = PyList_Size(target_list);

    float* inputs = (float*)nimcp_malloc(num_inputs * sizeof(float));
    float* targets = (float*)nimcp_malloc(num_targets * sizeof(float));
    if (!inputs || !targets) {
        nimcp_free(inputs);
        nimcp_free(targets);
        return PyErr_NoMemory();
    }

    for (Py_ssize_t i = 0; i < num_inputs; i++) {
        inputs[i] = (float)PyFloat_AsDouble(PyList_GET_ITEM(input_list, i));
    }
    for (Py_ssize_t i = 0; i < num_targets; i++) {
        targets[i] = (float)PyFloat_AsDouble(PyList_GET_ITEM(target_list, i));
    }

    if (PyErr_Occurred()) {
        nimcp_free(inputs);
        nimcp_free(targets);
        return NULL;
    }

    // Call the public API via a temporary network handle
    // Note: nimcp_network_train is currently a stub in the C API
    nimcp_status_t status = NIMCP_ERROR;

    // The public API requires nimcp_network_t (opaque wrapper), but Python bindings
    // hold the internal neural_network_t directly. Since the C API stub doesn't
    // actually use the internal network, we report the correct error.
    PyErr_SetString(NIMCPError,
        "Network-level training not yet implemented in the C API. "
        "Use Brain.train_step() or Brain.train_batch() for training.");
    (void)status;

    nimcp_free(inputs);
    nimcp_free(targets);
    return NULL;
}

static PyMethodDef NeuralNetwork_methods[] = {
    {"forward", (PyCFunction)NeuralNetwork_forward, METH_VARARGS,
     "Perform forward pass through the network\n\n"
     "Args:\n"
     "    inputs (list): List of input values\n"
     "    num_outputs (int): Number of output neurons\n"
     "Returns:\n"
     "    list: Output values"},

    {"train", (PyCFunction)NeuralNetwork_train, METH_VARARGS,
     "Train network on a single example\n\n"
     "Note: Low-level network training is not yet implemented in the C API.\n"
     "Use Brain.train_step() or Brain.train_batch() for training.\n\n"
     "Args:\n"
     "    inputs (list): Input values\n"
     "    targets (list): Target output values\n"
     "Raises:\n"
     "    NIMCPError: Always (not yet implemented)"},

    {"add_connection", (PyCFunction)NeuralNetwork_add_connection, METH_VARARGS,
     "Add a synaptic connection between neurons\n\n"
     "Args:\n"
     "    from_id (int): Source neuron ID\n"
     "    to_id (int): Target neuron ID\n"
     "    weight (float): Connection weight\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"update_neuron", (PyCFunction)NeuralNetwork_update_neuron, METH_VARARGS,
     "Update a neuron's state\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "    new_state (float): New state value\n"
     "    timestamp (int): Current timestamp\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"get_neuron_state", (PyCFunction)NeuralNetwork_get_neuron_state, METH_VARARGS,
     "Get a neuron's current state\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "Returns:\n"
     "    float: Neuron state value"},

    {"compute_step", (PyCFunction)NeuralNetwork_compute_step, METH_VARARGS,
     "Compute one simulation step\n\n"
     "Args:\n"
     "    timestamp (int): Current timestamp\n"
     "Returns:\n"
     "    int: Number of active neurons"},

    {"reset", (PyCFunction)NeuralNetwork_reset, METH_NOARGS,
     "Reset the neural network to initial state"},

    {"add_neuron", (PyCFunction)NeuralNetwork_add_neuron, METH_VARARGS,
     "Add a new neuron to the network\n\n"
     "Args:\n"
     "    activation_type (int): Activation function type\n"
     "Returns:\n"
     "    int: New neuron ID"},

    {"apply_stdp", (PyCFunction)NeuralNetwork_apply_stdp, METH_VARARGS,
     "Apply Spike-Timing-Dependent Plasticity\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "    timestamp (int): Current timestamp\n"
     "Returns:\n"
     "    int: Number of synapses updated"},

    {"apply_oja", (PyCFunction)NeuralNetwork_apply_oja, METH_VARARGS,
     "Apply Oja's learning rule\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "    timestamp (int): Current timestamp\n"
     "Returns:\n"
     "    int: Number of synapses updated"},

    {"apply_homeostasis", (PyCFunction)NeuralNetwork_apply_homeostasis, METH_VARARGS,
     "Apply homeostatic plasticity\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "    timestamp (int): Current timestamp\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"normalize_weights", (PyCFunction)NeuralNetwork_normalize_weights, METH_VARARGS,
     "Normalize synaptic weights for a neuron\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"record_spike", (PyCFunction)NeuralNetwork_record_spike, METH_VARARGS,
     "Record a spike event\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "    magnitude (float): Spike magnitude\n"
     "    timestamp (int): Spike timestamp\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"get_average_activity", (PyCFunction)NeuralNetwork_get_average_activity, METH_VARARGS,
     "Get average activity level of a neuron\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "Returns:\n"
     "    float: Average activity"},

    {"get_weight_norm", (PyCFunction)NeuralNetwork_get_weight_norm, METH_VARARGS,
     "Get L2 norm of weights for a neuron\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "Returns:\n"
     "    float: Weight norm"},

    {"get_weight_statistics", (PyCFunction)NeuralNetwork_get_weight_statistics, METH_VARARGS,
     "Get weight statistics (mean and std dev)\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "Returns:\n"
     "    tuple: (mean, std_dev)"},

    {"maintain_homeostasis", (PyCFunction)NeuralNetwork_maintain_homeostasis, METH_VARARGS,
     "Maintain homeostatic balance across network\n\n"
     "Args:\n"
     "    timestamp (int): Current timestamp"},

    {"prune_synapses", (PyCFunction)NeuralNetwork_prune_synapses, METH_VARARGS,
     "Prune weak synapses below threshold\n\n"
     "Args:\n"
     "    threshold (float): Weight threshold\n"
     "Returns:\n"
     "    int: Number of synapses pruned"},

    {"get_incoming_synapse_count", (PyCFunction)NeuralNetwork_get_incoming_synapse_count, METH_VARARGS,
     "Get number of incoming synapses\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "Returns:\n"
     "    int: Number of incoming connections"},

    {"update_plasticity", (PyCFunction)NeuralNetwork_update_plasticity, METH_VARARGS,
     "Update plasticity mechanisms for a neuron\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "    timestamp (int): Current timestamp\n"
     "Returns:\n"
     "    int: Number of updates applied"},

    {"adapt_threshold", (PyCFunction)NeuralNetwork_adapt_threshold, METH_VARARGS,
     "Adapt firing threshold for a neuron\n\n"
     "Args:\n"
     "    neuron_id (int): Neuron ID\n"
     "    timestamp (int): Current timestamp\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"set_neuron_model", (PyCFunction)NeuralNetwork_set_neuron_model, METH_VARARGS,
     "Set neuron model type for a specific neuron\n\n"
     "Args:\n"
     "    neuron_id (int): ID of neuron to modify\n"
     "    model_type (int): Model type (0=LIF, 1=Izhikevich, 2=AdEx, 3=Hodgkin-Huxley)\n"
     "Returns:\n"
     "    bool: True if successful, False otherwise\n\n"
     "Example:\n"
     "    # Switch neuron 5 to Izhikevich model\n"
     "    network.set_neuron_model(5, 1)"},

    {"serialize", (PyCFunction)NeuralNetwork_serialize, METH_VARARGS | METH_KEYWORDS,
     "Serialize network to binary format\n\n"
     "Args:\n"
     "    compress (bool): Enable LZ4 compression (default: True)\n"
     "    password (str, optional): Password for encryption (default: None)\n"
     "Returns:\n"
     "    bytes: Serialized network data\n\n"
     "Example:\n"
     "    # Save network to file\n"
     "    data = network.serialize()\n"
     "    with open('checkpoint.nimcp', 'wb') as f:\n"
     "        f.write(data)"},

    {"deserialize", (PyCFunction)NeuralNetwork_deserialize, METH_CLASS | METH_VARARGS | METH_KEYWORDS,
     "Deserialize network from binary format (classmethod)\n\n"
     "Args:\n"
     "    data (bytes): Serialized network data\n"
     "    password (str, optional): Password for decryption (if encrypted)\n"
     "Returns:\n"
     "    NeuralNetwork: Restored network instance\n\n"
     "Example:\n"
     "    # Load network from file\n"
     "    with open('checkpoint.nimcp', 'rb') as f:\n"
     "        data = f.read()\n"
     "    network = NeuralNetwork.deserialize(data)"},

    {NULL, NULL, 0, NULL}
};

PyTypeObject NeuralNetworkType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "nimcp.NeuralNetwork",
    .tp_doc = "NIMCP Neural Network",
    .tp_basicsize = sizeof(NeuralNetworkObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = NeuralNetwork_new,
    .tp_init = (initproc) NeuralNetwork_init,
    .tp_dealloc = (destructor) NeuralNetwork_dealloc,
    .tp_methods = NeuralNetwork_methods,
};

//=============================================================================
// P2PNode Type
//=============================================================================

/**
 * @brief Initializes P2PNode object from constructor arguments
 *
 * WHY: Creates the actual C P2P node from Python arguments.
 *
 * SIGNATURE: P2PNode(listen_port)
 */
static int P2PNode_init(P2PNodeObject* self, PyObject* args, PyObject* kwds)
{
    unsigned short listen_port;

    // Parse constructor arguments
    if (!PyArg_ParseTuple(args, "H", &listen_port)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "P2PNode_init: PyArg_ParseTuple is NULL");
        return -1;
    }

    // Create default node configuration
    node_config_t config = {.listen_port = listen_port,
                            .max_peers = 10,
                            .keepalive_interval = 1000,
                            .discovery_interval = 5000,
                            .reconnect_interval = 3000,
                            .max_retries = 3,
                            .ping_interval = 2000};

    // Create the P2P node
    self->node = p2p_node_create(&config);
    if (!self->node) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create P2P node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_init: self->node is NULL");
        return -1;
    }

    return 0;
}

static void P2PNode_dealloc(P2PNodeObject* self)
{
    if (self->node) {
        p2p_node_destroy(self->node);
    }
    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* P2PNode_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    P2PNodeObject* self = (P2PNodeObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->node = NULL;
    }
    return (PyObject*) self;
}

// P2PNode.start() -> bool
static PyObject* P2PNode_start(P2PNodeObject* self, PyObject* Py_UNUSED(ignored))
{
    bool success = p2p_node_start(self->node);

    if (!success) {
        PyErr_SetString(NodeError, "Failed to start P2P node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_start: success is NULL");
        return NULL;
    }

    Py_RETURN_TRUE;
}

// P2PNode.stop() -> bool
static PyObject* P2PNode_stop(P2PNodeObject* self, PyObject* Py_UNUSED(ignored))
{
    bool success = p2p_node_stop(self->node);

    if (!success) {
        PyErr_SetString(NodeError, "Failed to stop P2P node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_stop: success is NULL");
        return NULL;
    }

    Py_RETURN_TRUE;
}

// P2PNode.connect_peer(ip, port) -> bool
static PyObject* P2PNode_connect_peer(P2PNodeObject* self, PyObject* args)
{
    const char* peer_ip;
    unsigned short peer_port;

    if (!PyArg_ParseTuple(args, "sH", &peer_ip, &peer_port)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_connect_peer: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = p2p_node_connect_peer(self->node, peer_ip, peer_port);

    if (!success) {
        PyErr_SetString(NodeError, "Failed to connect to peer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_connect_peer: success is NULL");
        return NULL;
    }

    Py_RETURN_TRUE;
}

// P2PNode.disconnect_peer(ip, port) -> bool
static PyObject* P2PNode_disconnect_peer(P2PNodeObject* self, PyObject* args)
{
    const char* peer_ip;
    unsigned short peer_port;

    if (!PyArg_ParseTuple(args, "sH", &peer_ip, &peer_port)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_disconnect_peer: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = p2p_node_disconnect_peer(self->node, peer_ip, peer_port);

    if (!success) {
        PyErr_SetString(NodeError, "Failed to disconnect from peer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_disconnect_peer: success is NULL");
        return NULL;
    }

    Py_RETURN_TRUE;
}

// P2PNode.is_peer_connected(ip, port) -> bool
static PyObject* P2PNode_is_peer_connected(P2PNodeObject* self, PyObject* args)
{
    const char* peer_ip;
    unsigned short peer_port;

    if (!PyArg_ParseTuple(args, "sH", &peer_ip, &peer_port)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_is_peer_connected: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool connected = p2p_node_is_peer_connected(self->node, peer_ip, peer_port);

    if (connected) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

// P2PNode.send_heartbeats() -> int
static PyObject* P2PNode_send_heartbeats(P2PNodeObject* self, PyObject* Py_UNUSED(ignored))
{
    uint32_t sent = p2p_node_send_heartbeats(self->node);
    return PyLong_FromUnsignedLong(sent);
}

// P2PNode.check_peer_health(timeout_ms) -> int
static PyObject* P2PNode_check_peer_health(P2PNodeObject* self, PyObject* args)
{
    unsigned int timeout_ms;

    if (!PyArg_ParseTuple(args, "I", &timeout_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_check_peer_health: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t unhealthy = p2p_node_check_peer_health(self->node, timeout_ms);
    return PyLong_FromUnsignedLong(unhealthy);
}

// P2PNode.reconnect_unhealthy() -> int
static PyObject* P2PNode_reconnect_unhealthy(P2PNodeObject* self, PyObject* Py_UNUSED(ignored))
{
    uint32_t reconnected = p2p_node_reconnect_unhealthy(self->node);
    return PyLong_FromUnsignedLong(reconnected);
}

// P2PNode.process_pong(ip, port) -> bool
static PyObject* P2PNode_process_pong(P2PNodeObject* self, PyObject* args)
{
    const char* peer_ip;
    unsigned short peer_port;

    if (!PyArg_ParseTuple(args, "sH", &peer_ip, &peer_port)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "P2PNode_process_pong: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = p2p_node_process_pong(self->node, peer_ip, peer_port);

    if (success) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyMethodDef P2PNode_methods[] = {
    {"start", (PyCFunction)P2PNode_start, METH_NOARGS,
     "Start the P2P node\n\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"stop", (PyCFunction)P2PNode_stop, METH_NOARGS,
     "Stop the P2P node\n\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"connect_peer", (PyCFunction)P2PNode_connect_peer, METH_VARARGS,
     "Connect to a peer node\n\n"
     "Args:\n"
     "    ip (str): Peer IP address\n"
     "    port (int): Peer port number\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"disconnect_peer", (PyCFunction)P2PNode_disconnect_peer, METH_VARARGS,
     "Disconnect from a peer node\n\n"
     "Args:\n"
     "    ip (str): Peer IP address\n"
     "    port (int): Peer port number\n"
     "Returns:\n"
     "    bool: True if successful"},

    {"is_peer_connected", (PyCFunction)P2PNode_is_peer_connected, METH_VARARGS,
     "Check if a peer is connected\n\n"
     "Args:\n"
     "    ip (str): Peer IP address\n"
     "    port (int): Peer port number\n"
     "Returns:\n"
     "    bool: True if connected"},

    {"send_heartbeats", (PyCFunction)P2PNode_send_heartbeats, METH_NOARGS,
     "Send heartbeats to all connected peers\n\n"
     "Returns:\n"
     "    int: Number of heartbeats sent"},

    {"check_peer_health", (PyCFunction)P2PNode_check_peer_health, METH_VARARGS,
     "Check health of all peers\n\n"
     "Args:\n"
     "    timeout_ms (int): Timeout in milliseconds\n"
     "Returns:\n"
     "    int: Number of unhealthy peers"},

    {"reconnect_unhealthy", (PyCFunction)P2PNode_reconnect_unhealthy, METH_NOARGS,
     "Reconnect to unhealthy peers\n\n"
     "Returns:\n"
     "    int: Number of peers reconnected"},

    {"process_pong", (PyCFunction)P2PNode_process_pong, METH_VARARGS,
     "Process pong message from peer\n\n"
     "Args:\n"
     "    ip (str): Peer IP address\n"
     "    port (int): Peer port number\n"
     "Returns:\n"
     "    bool: True if successful"},

    {NULL, NULL, 0, NULL}
};

PyTypeObject P2PNodeType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "nimcp.P2PNode",
    .tp_doc = "NIMCP P2P Node",
    .tp_basicsize = sizeof(P2PNodeObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = P2PNode_new,
    .tp_init = (initproc) P2PNode_init,
    .tp_dealloc = (destructor) P2PNode_dealloc,
    .tp_methods = P2PNode_methods,
};

//=============================================================================
// NetworkConfig Type
//=============================================================================

/**
 * @brief Initializes NetworkConfig object from constructor arguments
 *
 * WHY: Properly initializes config structure from Python constructor arguments.
 *
 * SIGNATURE: NetworkConfig(num_neurons, ei_ratio, learning_rate, hebbian_rate,
 *                          stdp_window, homeostatic_rate, target_activity, adaptation_rate)
 */
static int NetworkConfig_init(NetworkConfigObject* self, PyObject* args, PyObject* kwds)
{
    int num_neurons;
    float ei_ratio, learning_rate, hebbian_rate, stdp_window;
    float homeostatic_rate, target_activity, adaptation_rate;

    // Parse constructor arguments
    if (!PyArg_ParseTuple(args, "ifffffff", &num_neurons, &ei_ratio, &learning_rate, &hebbian_rate,
                          &stdp_window, &homeostatic_rate, &target_activity, &adaptation_rate)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NetworkConfig_init: operation failed");
        return -1;
    }

    // Initialize configuration
    self->config.num_neurons = num_neurons;
    self->config.ei_ratio = ei_ratio;
    self->config.learning_rate = learning_rate;
    self->config.hebbian_rate = hebbian_rate;
    self->config.stdp_window = stdp_window;
    self->config.homeostatic_rate = homeostatic_rate;
    self->config.target_activity = target_activity;
    self->config.adaptation_rate = adaptation_rate;

    // Set defaults for other fields
    self->config.refractory_period = 5.0F;
    self->config.min_weight = -1.0F;
    self->config.max_weight = 1.0F;
    self->config.update_interval = 1000;
    self->config.input_size = 0;
    self->config.output_size = 0;
    self->config.num_layers = 0;
    self->config.layer_sizes = NULL;
    self->config.enable_stdp = true;
    self->config.enable_hebbian = true;
    self->config.enable_oja = true;
    self->config.enable_homeostasis = true;

    return 0;
}

static void NetworkConfig_dealloc(NetworkConfigObject* self)
{
    // Config cleanup if needed
    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* NetworkConfig_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    NetworkConfigObject* self = (NetworkConfigObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        memset(&self->config, 0, sizeof(network_config_t));
    }
    return (PyObject*) self;
}

PyTypeObject NetworkConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "nimcp.NetworkConfig",
    .tp_doc = "NIMCP Network Configuration",
    .tp_basicsize = sizeof(NetworkConfigObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = NetworkConfig_new,
    .tp_init = (initproc) NetworkConfig_init,
    .tp_dealloc = (destructor) NetworkConfig_dealloc,
};

//=============================================================================
// NodeConfig Type
//=============================================================================

/**
 * @brief Initializes NodeConfig object from constructor arguments
 *
 * WHY: Properly initializes node config structure from Python constructor arguments.
 *
 * SIGNATURE: NodeConfig(listen_port, max_peers, keepalive_interval,
 *                       discovery_interval, reconnect_interval, max_retries)
 */
static int NodeConfig_init(NodeConfigObject* self, PyObject* args, PyObject* kwds)
{
    unsigned short listen_port;
    int max_peers, keepalive_interval, discovery_interval;
    int reconnect_interval, max_retries;

    // Parse constructor arguments
    if (!PyArg_ParseTuple(args, "Hiiiii", &listen_port, &max_peers, &keepalive_interval,
                          &discovery_interval, &reconnect_interval, &max_retries)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NodeConfig_init: operation failed");
        return -1;
    }

    // Initialize configuration
    self->config.listen_port = listen_port;
    self->config.max_peers = max_peers;
    self->config.keepalive_interval = keepalive_interval;
    self->config.discovery_interval = discovery_interval;
    self->config.reconnect_interval = reconnect_interval;
    self->config.max_retries = max_retries;

    // Set default for ping_interval
    self->config.ping_interval = 2000;

    return 0;
}

static void NodeConfig_dealloc(NodeConfigObject* self)
{
    // Config cleanup if needed
    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* NodeConfig_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    NodeConfigObject* self = (NodeConfigObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        memset(&self->config, 0, sizeof(node_config_t));
    }
    return (PyObject*) self;
}

PyTypeObject NodeConfigType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "nimcp.NodeConfig",
    .tp_doc = "NIMCP Node Configuration",
    .tp_basicsize = sizeof(NodeConfigObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = NodeConfig_new,
    .tp_init = (initproc) NodeConfig_init,
    .tp_dealloc = (destructor) NodeConfig_dealloc,
};

//=============================================================================
// GlialIntegration Type
//=============================================================================

/**
 * @brief Initialize GlialIntegration object
 *
 * SIGNATURE: GlialIntegration(network, max_mappings=1000)
 */
static int GlialIntegration_init(GlialIntegrationObject* self, PyObject* args, PyObject* kwds)
{
    PyObject* network_obj;
    uint32_t max_mappings = 1000;
    static char* kwlist[] = {"network", "max_mappings", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|I", kwlist, &network_obj, &max_mappings)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "GlialIntegration_init: PyArg_ParseTupleAndKeywords is NULL");
        return -1;
    }

    // Verify it's a NeuralNetwork object
    if (!PyObject_IsInstance(network_obj, (PyObject*)&NeuralNetworkType)) {
        PyErr_SetString(PyExc_TypeError, "First argument must be a NeuralNetwork");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "GlialIntegration_init: PyObject_IsInstance is NULL");
        return -1;
    }

    NeuralNetworkObject* network = (NeuralNetworkObject*)network_obj;

    // Create glial integration system
    self->integration = glial_integration_create(network->network, max_mappings);
    if (!self->integration) {
        PyErr_SetString(NIMCPError, "Failed to create glial integration system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_init: self->integration is NULL");
        return -1;
    }

    // Keep reference to network
    Py_INCREF(network_obj);
    self->network = network_obj;

    return 0;
}

static void GlialIntegration_dealloc(GlialIntegrationObject* self)
{
    if (self->integration) {
        glial_integration_destroy(self->integration);
    }
    Py_XDECREF(self->network);
    Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* GlialIntegration_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    GlialIntegrationObject* self = (GlialIntegrationObject*) type->tp_alloc(type, 0);
    if (self != NULL) {
        self->integration = NULL;
        self->network = NULL;
    }
    return (PyObject*) self;
}

// GlialIntegration.enable_astrocytes(enable) -> bool
static PyObject* GlialIntegration_enable_astrocytes(GlialIntegrationObject* self, PyObject* args)
{
    int enable;

    if (!PyArg_ParseTuple(args, "p", &enable)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_enable_astrocytes: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (!self->integration) {
        PyErr_SetString(NIMCPError, "Glial integration not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_enable_astrocytes: self->integration is NULL");
        return NULL;
    }

    self->integration->enable_astrocyte_modulation = enable ? true : false;

    Py_RETURN_NONE;
}

// GlialIntegration.enable_oligodendrocytes(enable) -> bool
static PyObject* GlialIntegration_enable_oligodendrocytes(GlialIntegrationObject* self, PyObject* args)
{
    int enable;

    if (!PyArg_ParseTuple(args, "p", &enable)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_enable_oligodendrocytes: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (!self->integration) {
        PyErr_SetString(NIMCPError, "Glial integration not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_enable_oligodendrocytes: self->integration is NULL");
        return NULL;
    }

    self->integration->enable_oligodendrocyte_myelination = enable ? true : false;

    Py_RETURN_NONE;
}

// GlialIntegration.enable_microglia(enable) -> bool
static PyObject* GlialIntegration_enable_microglia(GlialIntegrationObject* self, PyObject* args)
{
    int enable;

    if (!PyArg_ParseTuple(args, "p", &enable)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_enable_microglia: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (!self->integration) {
        PyErr_SetString(NIMCPError, "Glial integration not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_enable_microglia: self->integration is NULL");
        return NULL;
    }

    self->integration->enable_microglia_pruning = enable ? true : false;

    Py_RETURN_NONE;
}

// GlialIntegration.get_stats() -> dict
static PyObject* GlialIntegration_get_stats(GlialIntegrationObject* self, PyObject* Py_UNUSED(ignored))
{
    if (!self->integration) {
        PyErr_SetString(NIMCPError, "Glial integration not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_get_stats: self->integration is NULL");
        return NULL;
    }

    glial_integration_stats_t stats;
    nimcp_result_t result = glial_integration_get_stats(self->integration, &stats);

    if (result != NIMCP_SUCCESS) {
        PyErr_SetString(NIMCPError, "Failed to get glial statistics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_get_stats: validation failed");
        return NULL;
    }

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
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_get_stats: _val is NULL"); \
            return NULL; \
        } \
        if (PyDict_SetItemString(dict, (key), _val) < 0) { \
            Py_DECREF(_val); \
            Py_DECREF(dict); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "GlialIntegration_get_stats: validation failed"); \
            return NULL; \
        } \
        Py_DECREF(_val); \
    } while (0)

    ADD_DICT_ITEM("num_astrocytes", PyLong_FromUnsignedLong(stats.num_astrocytes));
    ADD_DICT_ITEM("num_oligodendrocytes", PyLong_FromUnsignedLong(stats.num_oligodendrocytes));
    ADD_DICT_ITEM("num_microglia", PyLong_FromUnsignedLong(stats.num_microglia));

    ADD_DICT_ITEM("num_tripartite_synapses", PyLong_FromUnsignedLong(stats.num_tripartite_synapses));
    ADD_DICT_ITEM("num_myelinated_neurons", PyLong_FromUnsignedLong(stats.num_myelinated_neurons));
    ADD_DICT_ITEM("num_monitored_synapses", PyLong_FromUnsignedLong(stats.num_monitored_synapses));

    ADD_DICT_ITEM("total_modulations", PyLong_FromUnsignedLongLong(stats.total_modulations));
    ADD_DICT_ITEM("total_myelinations", PyLong_FromUnsignedLongLong(stats.total_myelinations));
    ADD_DICT_ITEM("total_prunings", PyLong_FromUnsignedLongLong(stats.total_prunings));

    ADD_DICT_ITEM("avg_synaptic_modulation", PyFloat_FromDouble(stats.avg_synaptic_modulation));
    ADD_DICT_ITEM("avg_myelination_factor", PyFloat_FromDouble(stats.avg_myelination_factor));
    ADD_DICT_ITEM("avg_pruning_rate", PyFloat_FromDouble(stats.avg_pruning_rate));

    #undef ADD_DICT_ITEM

    return dict;
}

static PyMethodDef GlialIntegration_methods[] = {
    {"enable_astrocytes", (PyCFunction)GlialIntegration_enable_astrocytes, METH_VARARGS,
     "Enable or disable astrocyte modulation\n\n"
     "Args:\n"
     "    enable (bool): True to enable, False to disable\n"},

    {"enable_oligodendrocytes", (PyCFunction)GlialIntegration_enable_oligodendrocytes, METH_VARARGS,
     "Enable or disable oligodendrocyte myelination\n\n"
     "Args:\n"
     "    enable (bool): True to enable, False to disable\n"},

    {"enable_microglia", (PyCFunction)GlialIntegration_enable_microglia, METH_VARARGS,
     "Enable or disable microglia pruning\n\n"
     "Args:\n"
     "    enable (bool): True to enable, False to disable\n"},

    {"get_stats", (PyCFunction)GlialIntegration_get_stats, METH_NOARGS,
     "Get glial cell statistics\n\n"
     "Returns:\n"
     "    dict: Statistics including cell counts, modulations, myelinations, and prunings"},

    {NULL}
};

PyTypeObject GlialIntegrationType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.GlialIntegration",
    .tp_doc = "NIMCP Glial Cell Integration System\n\n"
              "Manages astrocytes, oligodendrocytes, and microglia for\n"
              "biologically-realistic neural network modulation.",
    .tp_basicsize = sizeof(GlialIntegrationObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = GlialIntegration_new,
    .tp_init = (initproc) GlialIntegration_init,
    .tp_dealloc = (destructor) GlialIntegration_dealloc,
    .tp_methods = GlialIntegration_methods,
};
