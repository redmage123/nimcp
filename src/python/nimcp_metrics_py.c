/**
 * @file nimcp_metrics_py.c
 * @brief Python bindings for NIMCP metrics module
 * @version 2.6.1
 * @date 2025-11-04
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stddef.h>  /* for NULL */
#include <string.h>  // For strdup

#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/metrics/nimcp_metrics.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metrics_py)

//=============================================================================
// MetricsCollector Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    nimcp_metrics_collector_t collector;
} MetricsCollectorObject;

static void MetricsCollector_dealloc(MetricsCollectorObject* self) {
    if (self->collector) {
        nimcp_metrics_destroy(self->collector);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* MetricsCollector_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    MetricsCollectorObject* self;
    self = (MetricsCollectorObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->collector = NULL;
    }
    return (PyObject*)self;
}

static int MetricsCollector_init(MetricsCollectorObject* self, PyObject* args, PyObject* kwds) {
    const char* directory = NULL;
    int format = NIMCP_METRICS_FORMAT_CSV;

    static char* kwlist[] = {"directory", "format", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|si", kwlist, &directory, &format)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "MetricsCollector_init: PyArg_ParseTupleAndKeywords is NULL");
        return -1;
    }

    if (directory) {
        nimcp_metrics_config_t config;
        nimcp_metrics_get_default_config(&config);
        strncpy(config.output_directory, directory, NIMCP_METRICS_MAX_PATH - 1);
        config.format = (nimcp_metrics_format_t)format;
        self->collector = nimcp_metrics_create_with_config(&config);
    } else {
        self->collector = nimcp_metrics_create();
    }

    if (!self->collector) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create metrics collector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_init: self->collector is NULL");
        return -1;
    }

    return 0;
}

//=============================================================================
// MetricsCollector Methods
//=============================================================================

static PyObject* MetricsCollector_record_counter(MetricsCollectorObject* self, PyObject* args, PyObject* kwds) {
    const char* name;
    unsigned long long value;
    int category = NIMCP_METRIC_CATEGORY_CUSTOM;

    static char* kwlist[] = {"name", "value", "category", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "sK|i", kwlist, &name, &value, &category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_record_counter: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    bool success = nimcp_metrics_record_counter(self->collector, name, value,
                                                 (nimcp_metric_category_t)category);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to record counter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* MetricsCollector_record_gauge(MetricsCollectorObject* self, PyObject* args, PyObject* kwds) {
    const char* name;
    double value;
    int category = NIMCP_METRIC_CATEGORY_CUSTOM;

    static char* kwlist[] = {"name", "value", "category", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "sd|i", kwlist, &name, &value, &category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_record_gauge: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    bool success = nimcp_metrics_record_gauge(self->collector, name, value,
                                               (nimcp_metric_category_t)category);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to record gauge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* MetricsCollector_record_timer(MetricsCollectorObject* self, PyObject* args, PyObject* kwds) {
    const char* name;
    double duration_ms;
    int category = NIMCP_METRIC_CATEGORY_PERFORMANCE;

    static char* kwlist[] = {"name", "duration_ms", "category", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "sd|i", kwlist, &name, &duration_ms, &category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_record_timer: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    bool success = nimcp_metrics_record_timer(self->collector, name, duration_ms,
                                               (nimcp_metric_category_t)category);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to record timer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* MetricsCollector_record_event(MetricsCollectorObject* self, PyObject* args, PyObject* kwds) {
    const char* name;
    const char* labels = NULL;
    int category = NIMCP_METRIC_CATEGORY_CUSTOM;

    static char* kwlist[] = {"name", "labels", "category", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|si", kwlist, &name, &labels, &category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_record_event: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    bool success = nimcp_metrics_record_event(self->collector, name, labels,
                                               (nimcp_metric_category_t)category);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to record event");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* MetricsCollector_timer_start(MetricsCollectorObject* self, PyObject* args) {
    const char* name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_timer_start: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint64_t start_time = nimcp_metrics_timer_start(self->collector, name);

    return PyLong_FromUnsignedLongLong(start_time);
}

static PyObject* MetricsCollector_timer_stop(MetricsCollectorObject* self, PyObject* args, PyObject* kwds) {
    const char* name;
    unsigned long long start_time;
    int category = NIMCP_METRIC_CATEGORY_PERFORMANCE;

    static char* kwlist[] = {"name", "start_time", "category", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "sK|i", kwlist, &name, &start_time, &category)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_timer_stop: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    bool success = nimcp_metrics_timer_stop(self->collector, name, start_time,
                                             (nimcp_metric_category_t)category);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to stop timer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* MetricsCollector_flush(MetricsCollectorObject* self, PyObject* Py_UNUSED(ignored)) {
    // Release GIL during potentially long disk I/O
    int32_t count;
    Py_BEGIN_ALLOW_THREADS
    count = nimcp_metrics_flush(self->collector);
    Py_END_ALLOW_THREADS

    if (count < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to flush metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_flush: validation failed");
        return NULL;
    }

    return PyLong_FromLong(count);
}

static PyObject* MetricsCollector_export_tableau_csv(MetricsCollectorObject* self, PyObject* args) {
    const char* filename;

    if (!PyArg_ParseTuple(args, "s", &filename)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "MetricsCollector_export_tableau_csv: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Copy filename since we release GIL
    char* filename_copy = strdup(filename);
    if (!filename_copy) {
        return PyErr_NoMemory();
    }

    // Release GIL during file I/O
    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = nimcp_metrics_export_tableau_csv(self->collector, filename_copy);
    Py_END_ALLOW_THREADS

    nimcp_free(filename_copy);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to export Tableau CSV");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* MetricsCollector_export_powerbi_json(MetricsCollectorObject* self, PyObject* args) {
    const char* filename;

    if (!PyArg_ParseTuple(args, "s", &filename)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "MetricsCollector_export_powerbi_json: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Copy filename since we release GIL
    char* filename_copy = strdup(filename);
    if (!filename_copy) {
        return PyErr_NoMemory();
    }

    // Release GIL during file I/O
    bool success;
    Py_BEGIN_ALLOW_THREADS
    success = nimcp_metrics_export_powerbi_json(self->collector, filename_copy);
    Py_END_ALLOW_THREADS

    nimcp_free(filename_copy);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to export PowerBI JSON");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject* MetricsCollector_get_stats(MetricsCollectorObject* self, PyObject* Py_UNUSED(ignored)) {
    char stats_json[NIMCP_PATH_BUFFER_SIZE];
    int32_t written = nimcp_metrics_get_stats(self->collector, stats_json, sizeof(stats_json));

    if (written < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_get_stats: validation failed");
        return NULL;
    }

    return PyUnicode_FromString(stats_json);
}

static PyObject* MetricsCollector_set_directory(MetricsCollectorObject* self, PyObject* args) {
    const char* directory;

    if (!PyArg_ParseTuple(args, "s", &directory)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "MetricsCollector_set_directory: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success = nimcp_metrics_set_directory(self->collector, directory);

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set directory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    Py_RETURN_NONE;
}

//=============================================================================
// MetricsCollector Method Table
//=============================================================================

static PyMethodDef MetricsCollector_methods[] = {
    {"record_counter", (PyCFunction)MetricsCollector_record_counter, METH_VARARGS | METH_KEYWORDS,
     "Record a counter metric\n\n"
     "Args:\n"
     "    name (str): Metric name\n"
     "    value (int): Counter value\n"
     "    category (int, optional): Metric category\n"},

    {"record_gauge", (PyCFunction)MetricsCollector_record_gauge, METH_VARARGS | METH_KEYWORDS,
     "Record a gauge metric\n\n"
     "Args:\n"
     "    name (str): Metric name\n"
     "    value (float): Gauge value\n"
     "    category (int, optional): Metric category\n"},

    {"record_timer", (PyCFunction)MetricsCollector_record_timer, METH_VARARGS | METH_KEYWORDS,
     "Record a timer metric\n\n"
     "Args:\n"
     "    name (str): Metric name\n"
     "    duration_ms (float): Duration in milliseconds\n"
     "    category (int, optional): Metric category\n"},

    {"record_event", (PyCFunction)MetricsCollector_record_event, METH_VARARGS | METH_KEYWORDS,
     "Record an event metric\n\n"
     "Args:\n"
     "    name (str): Event name\n"
     "    labels (str, optional): JSON labels\n"
     "    category (int, optional): Metric category\n"},

    {"timer_start", (PyCFunction)MetricsCollector_timer_start, METH_VARARGS,
     "Start a performance timer\n\n"
     "Args:\n"
     "    name (str): Timer name\n\n"
     "Returns:\n"
     "    int: Start timestamp\n"},

    {"timer_stop", (PyCFunction)MetricsCollector_timer_stop, METH_VARARGS | METH_KEYWORDS,
     "Stop a performance timer\n\n"
     "Args:\n"
     "    name (str): Timer name\n"
     "    start_time (int): Start timestamp from timer_start()\n"
     "    category (int, optional): Metric category\n"},

    {"flush", (PyCFunction)MetricsCollector_flush, METH_NOARGS,
     "Flush buffered metrics to disk\n\n"
     "Returns:\n"
     "    int: Number of metrics flushed\n"},

    {"export_tableau_csv", (PyCFunction)MetricsCollector_export_tableau_csv, METH_VARARGS,
     "Export metrics to Tableau-compatible CSV\n\n"
     "Args:\n"
     "    filename (str): Output filename\n"},

    {"export_powerbi_json", (PyCFunction)MetricsCollector_export_powerbi_json, METH_VARARGS,
     "Export metrics to PowerBI-compatible JSON\n\n"
     "Args:\n"
     "    filename (str): Output filename\n"},

    {"get_stats", (PyCFunction)MetricsCollector_get_stats, METH_NOARGS,
     "Get metrics statistics\n\n"
     "Returns:\n"
     "    str: JSON statistics\n"},

    {"set_directory", (PyCFunction)MetricsCollector_set_directory, METH_VARARGS,
     "Set output directory for metrics\n\n"
     "Args:\n"
     "    directory (str): Metrics directory path\n"},

    {NULL}  // Sentinel
};

//=============================================================================
// MetricsCollector Type Definition
//=============================================================================

static PyTypeObject MetricsCollectorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.MetricsCollector",
    .tp_doc = "NIMCP Metrics Collector for Tableau and PowerBI",
    .tp_basicsize = sizeof(MetricsCollectorObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = MetricsCollector_new,
    .tp_init = (initproc)MetricsCollector_init,
    .tp_dealloc = (destructor)MetricsCollector_dealloc,
    .tp_methods = MetricsCollector_methods,
};

//=============================================================================
// Module Initialization Helper
//=============================================================================

int init_metrics_module(PyObject* module) {
    LOG_MODULE_DEBUG("bindings.python.metrics", "Initializing metrics Python module");

    if (PyType_Ready(&MetricsCollectorType) < 0) {
        LOG_MODULE_ERROR("bindings.python.metrics", "Failed to initialize MetricsCollector type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_metrics_module: validation failed");
        return -1;
    }

    Py_INCREF(&MetricsCollectorType);
    if (PyModule_AddObject(module, "MetricsCollector", (PyObject*)&MetricsCollectorType) < 0) {
        Py_DECREF(&MetricsCollectorType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_metrics_module: validation failed");
        return -1;
    }

    // Add metric format constants
    PyModule_AddIntConstant(module, "METRICS_FORMAT_CSV", NIMCP_METRICS_FORMAT_CSV);
    PyModule_AddIntConstant(module, "METRICS_FORMAT_JSON", NIMCP_METRICS_FORMAT_JSON);
    PyModule_AddIntConstant(module, "METRICS_FORMAT_PARQUET", NIMCP_METRICS_FORMAT_PARQUET);
    PyModule_AddIntConstant(module, "METRICS_FORMAT_TDE", NIMCP_METRICS_FORMAT_TDE);

    // Add metric category constants
    PyModule_AddIntConstant(module, "METRIC_CATEGORY_PERFORMANCE", NIMCP_METRIC_CATEGORY_PERFORMANCE);
    PyModule_AddIntConstant(module, "METRIC_CATEGORY_MEMORY", NIMCP_METRIC_CATEGORY_MEMORY);
    PyModule_AddIntConstant(module, "METRIC_CATEGORY_NETWORK", NIMCP_METRIC_CATEGORY_NETWORK);
    PyModule_AddIntConstant(module, "METRIC_CATEGORY_LEARNING", NIMCP_METRIC_CATEGORY_LEARNING);
    PyModule_AddIntConstant(module, "METRIC_CATEGORY_INFERENCE", NIMCP_METRIC_CATEGORY_INFERENCE);
    PyModule_AddIntConstant(module, "METRIC_CATEGORY_SYSTEM", NIMCP_METRIC_CATEGORY_SYSTEM);
    PyModule_AddIntConstant(module, "METRIC_CATEGORY_CUSTOM", NIMCP_METRIC_CATEGORY_CUSTOM);

    return 0;
}
