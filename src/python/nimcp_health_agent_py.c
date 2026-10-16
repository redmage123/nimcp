/**
 * @file nimcp_health_agent_py.c
 * @brief Python bindings for NIMCP Health Agent USE Functions
 *
 * WHAT: Python interface to health agent USE functions for cognitive module integration
 * WHY:  Enable Python-based monitoring and control of Portia, Dragonfly, Swarm, and Memory systems
 * HOW:  Python C extension wrapping nimcp_health_agent.h USE functions
 *
 * FEATURES IMPLEMENTED:
 * - HealthAgent: Main health monitoring agent object
 * - HealthAgentStats: Statistics container
 * - HealthAgentMessage: Message container for anomaly reporting
 * - Portia USE functions: tier control, degradation, resource queries
 * - Dragonfly USE functions: anomaly tracking, prediction, pursuit
 * - Swarm USE functions: threat detection, response, memory
 * - Memory USE functions: engram encoding/recall, consolidation
 *
 * CODING STANDARDS:
 * - WHAT/WHY/HOW documentation for all functions
 * - Guard clauses for input validation
 * - Consistent error handling patterns
 * - Memory safety (no leaks, proper cleanup)
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * HealthAgentStats Type (read-only container)
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    uint64_t uptime_ms;
    uint64_t checks_performed;
    uint64_t anomalies_detected;
    uint64_t messages_sent;
    uint64_t recoveries_triggered;
    uint64_t recoveries_succeeded;
    uint64_t heartbeats_received;
    uint64_t heartbeat_timeouts;
    uint64_t deadlocks_detected;
    uint64_t nans_detected;
    uint64_t memory_corruptions;
    uint64_t consistency_failures;
    float avg_check_duration_us;
    float avg_message_latency_us;
    uint32_t queue_high_watermark;
    int highest_severity_seen;
} HealthAgentStatsObject;

static PyMemberDef HealthAgentStats_members[] = {
    {"uptime_ms", T_ULONGLONG, offsetof(HealthAgentStatsObject, uptime_ms), READONLY, "Agent uptime in ms"},
    {"checks_performed", T_ULONGLONG, offsetof(HealthAgentStatsObject, checks_performed), READONLY, "Total checks"},
    {"anomalies_detected", T_ULONGLONG, offsetof(HealthAgentStatsObject, anomalies_detected), READONLY, "Anomalies detected"},
    {"messages_sent", T_ULONGLONG, offsetof(HealthAgentStatsObject, messages_sent), READONLY, "Messages sent"},
    {"recoveries_triggered", T_ULONGLONG, offsetof(HealthAgentStatsObject, recoveries_triggered), READONLY, "Recoveries triggered"},
    {"recoveries_succeeded", T_ULONGLONG, offsetof(HealthAgentStatsObject, recoveries_succeeded), READONLY, "Successful recoveries"},
    {"heartbeats_received", T_ULONGLONG, offsetof(HealthAgentStatsObject, heartbeats_received), READONLY, "Heartbeats received"},
    {"heartbeat_timeouts", T_ULONGLONG, offsetof(HealthAgentStatsObject, heartbeat_timeouts), READONLY, "Heartbeat timeouts"},
    {"deadlocks_detected", T_ULONGLONG, offsetof(HealthAgentStatsObject, deadlocks_detected), READONLY, "Deadlocks detected"},
    {"nans_detected", T_ULONGLONG, offsetof(HealthAgentStatsObject, nans_detected), READONLY, "NaN values detected"},
    {"memory_corruptions", T_ULONGLONG, offsetof(HealthAgentStatsObject, memory_corruptions), READONLY, "Memory corruptions"},
    {"consistency_failures", T_ULONGLONG, offsetof(HealthAgentStatsObject, consistency_failures), READONLY, "Consistency failures"},
    {"avg_check_duration_us", T_FLOAT, offsetof(HealthAgentStatsObject, avg_check_duration_us), READONLY, "Avg check duration"},
    {"avg_message_latency_us", T_FLOAT, offsetof(HealthAgentStatsObject, avg_message_latency_us), READONLY, "Avg message latency"},
    {"queue_high_watermark", T_UINT, offsetof(HealthAgentStatsObject, queue_high_watermark), READONLY, "Queue high watermark"},
    {"highest_severity_seen", T_INT, offsetof(HealthAgentStatsObject, highest_severity_seen), READONLY, "Highest severity seen"},
    {NULL}
};

static PyObject* HealthAgentStats_repr(HealthAgentStatsObject* self)
{
    return PyUnicode_FromFormat("HealthAgentStats(uptime=%llu ms, anomalies=%llu, recoveries=%llu/%llu)",
                                self->uptime_ms, self->anomalies_detected,
                                self->recoveries_succeeded, self->recoveries_triggered);
}

static PyTypeObject HealthAgentStatsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.HealthAgentStats",
    .tp_doc = PyDoc_STR("Health agent statistics"),
    .tp_basicsize = sizeof(HealthAgentStatsObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_repr = (reprfunc)HealthAgentStats_repr,
    .tp_members = HealthAgentStats_members,
};

/* Helper to create HealthAgentStats from C struct */
static PyObject* HealthAgentStats_FromC(const health_agent_stats_t* stats)
{
    HealthAgentStatsObject* obj = PyObject_New(HealthAgentStatsObject, &HealthAgentStatsType);
    if (obj == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obj is NULL");

        return NULL;
    }

    obj->uptime_ms = stats->uptime_ms;
    obj->checks_performed = stats->checks_performed;
    obj->anomalies_detected = stats->anomalies_detected;
    obj->messages_sent = stats->messages_sent;
    obj->recoveries_triggered = stats->recoveries_triggered;
    obj->recoveries_succeeded = stats->recoveries_succeeded;
    obj->heartbeats_received = stats->heartbeats_received;
    obj->heartbeat_timeouts = stats->heartbeat_timeouts;
    obj->deadlocks_detected = stats->deadlocks_detected;
    obj->nans_detected = stats->nans_detected;
    obj->memory_corruptions = stats->memory_corruptions;
    obj->consistency_failures = stats->consistency_failures;
    obj->avg_check_duration_us = stats->avg_check_duration_us;
    obj->avg_message_latency_us = stats->avg_message_latency_us;
    obj->queue_high_watermark = stats->queue_high_watermark;
    obj->highest_severity_seen = (int)stats->highest_severity_seen;

    return (PyObject*)obj;
}

/* ============================================================================
 * HealthAgent Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    nimcp_health_agent_t* agent;
} HealthAgentObject;

static void HealthAgent_dealloc(HealthAgentObject* self)
{
    if (self->agent != NULL) {
        nimcp_health_agent_stop(self->agent);
        nimcp_health_agent_destroy(self->agent);
        self->agent = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* HealthAgent_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    HealthAgentObject* self = (HealthAgentObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->agent = NULL;
    }
    return (PyObject*)self;
}

static int HealthAgent_init(HealthAgentObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"name", "heartbeat_ms", "watchdog_ms", "check_ms", NULL};
    const char* name = "python_health_agent";
    unsigned int heartbeat_ms = HEALTH_AGENT_DEFAULT_HEARTBEAT_MS;
    unsigned int watchdog_ms = HEALTH_AGENT_DEFAULT_WATCHDOG_MS;
    unsigned int check_ms = HEALTH_AGENT_DEFAULT_CHECK_MS;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sIII", kwlist,
                                      &name, &heartbeat_ms, &watchdog_ms, &check_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "HealthAgent_init: operation failed");
        return -1;
    }

    health_agent_config_t config;
    nimcp_health_agent_default_config(&config);

    strncpy(config.agent_name, name, sizeof(config.agent_name) - 1);
    config.agent_name[sizeof(config.agent_name) - 1] = '\0';
    config.heartbeat_interval_ms = heartbeat_ms;
    config.watchdog_timeout_ms = watchdog_ms;
    config.check_interval_ms = check_ms;

    self->agent = nimcp_health_agent_create(&config);
    if (self->agent == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create health agent");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "HealthAgent_init: validation failed");
        return -1;
    }

    return 0;
}

/* ============================================================================
 * Basic Agent Methods
 * ============================================================================ */

static PyObject* HealthAgent_start(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_start(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

static PyObject* HealthAgent_stop(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_stop(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

static PyObject* HealthAgent_is_running(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    bool running = nimcp_health_agent_is_running(self->agent);
    return PyBool_FromLong(running);
}

static PyObject* HealthAgent_heartbeat(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    Py_BEGIN_ALLOW_THREADS
    nimcp_health_agent_heartbeat(self->agent);
    Py_END_ALLOW_THREADS

    Py_RETURN_NONE;
}

static PyObject* HealthAgent_get_stats(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    nimcp_health_agent_get_stats(self->agent, &stats);

    return HealthAgentStats_FromC(&stats);
}

static PyObject* HealthAgent_request_check(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_request_check(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/* ============================================================================
 * Portia USE Functions
 * ============================================================================ */

/**
 * @brief HealthAgent.use_portia_set_tier(tier) -> bool
 */
static PyObject* HealthAgent_use_portia_set_tier(HealthAgentObject* self, PyObject* args)
{
    unsigned int tier;

    if (!PyArg_ParseTuple(args, "I", &tier)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_portia_set_tier: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_portia_set_tier(self->agent, tier);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.use_portia_degrade(level) -> bool
 */
static PyObject* HealthAgent_use_portia_degrade(HealthAgentObject* self, PyObject* args)
{
    unsigned int level;

    if (!PyArg_ParseTuple(args, "I", &level)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_portia_degrade: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (level > 4) {
        PyErr_SetString(PyExc_ValueError, "Degradation level must be 0-4");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_portia_degrade: validation failed");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_portia_degrade(self->agent, level);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.use_portia_get_recommended_neurons() -> int
 */
static PyObject* HealthAgent_use_portia_get_recommended_neurons(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t count = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_portia_get_recommended_neurons(self->agent, &count);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get recommended neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_portia_get_recommended_neurons: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(count);
}

/**
 * @brief HealthAgent.use_portia_get_status() -> (power_state, thermal_state, degradation_level)
 */
static PyObject* HealthAgent_use_portia_get_status(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t power_state = 0;
    uint32_t thermal_state = 0;
    uint32_t degradation_level = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_portia_get_status(self->agent, &power_state, &thermal_state, &degradation_level);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get Portia status");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_portia_get_status: validation failed");
        return NULL;
    }

    return Py_BuildValue("(III)", power_state, thermal_state, degradation_level);
}

/* ============================================================================
 * Dragonfly USE Functions
 * ============================================================================ */

/**
 * @brief HealthAgent.use_dragonfly_track_anomaly(msg_type, severity, source, description) -> target_id
 */
static PyObject* HealthAgent_use_dragonfly_track_anomaly(HealthAgentObject* self, PyObject* args)
{
    int msg_type;
    int severity;
    int source;
    const char* description;

    if (!PyArg_ParseTuple(args, "iiis", &msg_type, &severity, &source, &description)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_dragonfly_track_anomaly: PyArg_ParseTuple is NULL");
        return NULL;
    }

    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = (health_agent_msg_type_t)msg_type;
    msg.severity = (health_agent_severity_t)severity;
    msg.source = (health_agent_source_t)source;
    strncpy(msg.description, description, sizeof(msg.description) - 1);

    uint32_t target_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_dragonfly_track_anomaly(self->agent, &msg, &target_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to track anomaly");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_dragonfly_track_anomaly: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(target_id);
}

/**
 * @brief HealthAgent.use_dragonfly_predict(target_id) -> (time_to_failure, confidence)
 */
static PyObject* HealthAgent_use_dragonfly_predict(HealthAgentObject* self, PyObject* args)
{
    unsigned int target_id;

    if (!PyArg_ParseTuple(args, "I", &target_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_dragonfly_predict: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float time_to_failure = 0.0f;
    float confidence = 0.0f;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_dragonfly_predict(self->agent, target_id, &time_to_failure, &confidence);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get prediction");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_dragonfly_predict: validation failed");
        return NULL;
    }

    return Py_BuildValue("(ff)", time_to_failure, confidence);
}

/**
 * @brief HealthAgent.use_dragonfly_pursue() -> bool
 */
static PyObject* HealthAgent_use_dragonfly_pursue(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_dragonfly_pursue(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.use_dragonfly_abort() -> bool
 */
static PyObject* HealthAgent_use_dragonfly_abort(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_dragonfly_abort(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.use_dragonfly_get_mode() -> mode
 */
static PyObject* HealthAgent_use_dragonfly_get_mode(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t mode = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_dragonfly_get_mode(self->agent, &mode);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get Dragonfly mode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_dragonfly_get_mode: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(mode);
}

/* ============================================================================
 * Swarm Immune USE Functions
 * ============================================================================ */

/**
 * @brief HealthAgent.use_swarm_detect_threat(data, source_id) -> (detected, threat_id)
 */
static PyObject* HealthAgent_use_swarm_detect_threat(HealthAgentObject* self, PyObject* args)
{
    Py_buffer data;
    unsigned int source_id;

    if (!PyArg_ParseTuple(args, "y*I", &data, &source_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_detect_threat: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool detected = false;
    uint32_t threat_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_detect_threat(
        self->agent,
        (const uint8_t*)data.buf,
        (size_t)data.len,
        source_id,
        &detected,
        &threat_id
    );
    Py_END_ALLOW_THREADS

    PyBuffer_Release(&data);

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to detect threat");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_detect_threat: validation failed");
        return NULL;
    }

    return Py_BuildValue("(OI)", detected ? Py_True : Py_False, threat_id);
}

/**
 * @brief HealthAgent.use_swarm_generate_response(threat_id) -> response_id
 */
static PyObject* HealthAgent_use_swarm_generate_response(HealthAgentObject* self, PyObject* args)
{
    unsigned int threat_id;

    if (!PyArg_ParseTuple(args, "I", &threat_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_generate_response: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t response_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_generate_response(self->agent, threat_id, &response_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to generate response");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_generate_response: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(response_id);
}

/**
 * @brief HealthAgent.use_swarm_check_behavior(component_id) -> anomaly_score
 */
static PyObject* HealthAgent_use_swarm_check_behavior(HealthAgentObject* self, PyObject* args)
{
    unsigned int component_id;

    if (!PyArg_ParseTuple(args, "I", &component_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_check_behavior: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float anomaly_score = 0.0f;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_check_behavior(self->agent, component_id, &anomaly_score);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to check behavior");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_check_behavior: validation failed");
        return NULL;
    }

    return PyFloat_FromDouble((double)anomaly_score);
}

/**
 * @brief HealthAgent.use_swarm_add_memory_cell(pattern, response_type) -> cell_id
 */
static PyObject* HealthAgent_use_swarm_add_memory_cell(HealthAgentObject* self, PyObject* args)
{
    Py_buffer pattern;
    unsigned int response_type;

    if (!PyArg_ParseTuple(args, "y*I", &pattern, &response_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_add_memory_cell: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t cell_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_add_memory_cell(
        self->agent,
        (const uint8_t*)pattern.buf,
        (size_t)pattern.len,
        response_type,
        &cell_id
    );
    Py_END_ALLOW_THREADS

    PyBuffer_Release(&pattern);

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to add memory cell");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_add_memory_cell: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(cell_id);
}

/* ============================================================================
 * Swarm Memory USE Functions
 * ============================================================================ */

/**
 * @brief HealthAgent.use_swarm_memory_store(data, pattern_type, importance) -> pattern_id
 */
static PyObject* HealthAgent_use_swarm_memory_store(HealthAgentObject* self, PyObject* args)
{
    Py_buffer data;
    unsigned int pattern_type;
    unsigned int importance;

    if (!PyArg_ParseTuple(args, "y*II", &data, &pattern_type, &importance)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_memory_store: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (importance > 3) {
        PyBuffer_Release(&data);
        PyErr_SetString(PyExc_ValueError, "Importance must be 0-3");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_memory_store: validation failed");
        return NULL;
    }

    char pattern_id[65];
    memset(pattern_id, 0, sizeof(pattern_id));
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_memory_store(
        self->agent,
        data.buf,
        (size_t)data.len,
        pattern_type,
        importance,
        pattern_id
    );
    Py_END_ALLOW_THREADS

    PyBuffer_Release(&data);

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to store memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_memory_store: validation failed");
        return NULL;
    }

    return PyUnicode_FromString(pattern_id);
}

/**
 * @brief HealthAgent.use_swarm_memory_replay(count) -> replayed_count
 */
static PyObject* HealthAgent_use_swarm_memory_replay(HealthAgentObject* self, PyObject* args)
{
    unsigned int count;

    if (!PyArg_ParseTuple(args, "I", &count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_memory_replay: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_memory_replay(self->agent, count);
    Py_END_ALLOW_THREADS

    if (result < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to replay memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_memory_replay: validation failed");
        return NULL;
    }

    return PyLong_FromLong(result);
}

/**
 * @brief HealthAgent.use_swarm_memory_consolidate() -> bool
 */
static PyObject* HealthAgent_use_swarm_memory_consolidate(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_memory_consolidate(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.use_swarm_memory_get_stats() -> (total, consolidated, avg_strength)
 */
static PyObject* HealthAgent_use_swarm_memory_get_stats(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    uint64_t total = 0;
    uint64_t consolidated = 0;
    float avg_strength = 0.0f;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_swarm_memory_get_stats(self->agent, &total, &consolidated, &avg_strength);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get memory stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_swarm_memory_get_stats: validation failed");
        return NULL;
    }

    return Py_BuildValue("(KKf)", total, consolidated, avg_strength);
}

/* ============================================================================
 * Engram USE Functions
 * ============================================================================ */

/**
 * @brief HealthAgent.use_engram_encode(msg_type, severity, source, description) -> engram_id
 */
static PyObject* HealthAgent_use_engram_encode(HealthAgentObject* self, PyObject* args)
{
    int msg_type;
    int severity;
    int source;
    const char* description;

    if (!PyArg_ParseTuple(args, "iiis", &msg_type, &severity, &source, &description)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_engram_encode: PyArg_ParseTuple is NULL");
        return NULL;
    }

    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = (health_agent_msg_type_t)msg_type;
    msg.severity = (health_agent_severity_t)severity;
    msg.source = (health_agent_source_t)source;
    strncpy(msg.description, description, sizeof(msg.description) - 1);

    uint64_t engram_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_engram_encode(self->agent, &msg, &engram_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to encode engram");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_engram_encode: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLongLong(engram_id);
}

/**
 * @brief HealthAgent.use_engram_recall(msg_type, severity, source, description, max_recalls) -> list of engram_ids
 */
static PyObject* HealthAgent_use_engram_recall(HealthAgentObject* self, PyObject* args)
{
    int msg_type;
    int severity;
    int source;
    const char* description;
    unsigned int max_recalls = 10;

    if (!PyArg_ParseTuple(args, "iiis|I", &msg_type, &severity, &source, &description, &max_recalls)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_engram_recall: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (max_recalls > 100) {
        max_recalls = 100;
    }

    health_agent_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = (health_agent_msg_type_t)msg_type;
    msg.severity = (health_agent_severity_t)severity;
    msg.source = (health_agent_source_t)source;
    strncpy(msg.description, description, sizeof(msg.description) - 1);

    uint64_t* recalled_ids = (uint64_t*)nimcp_malloc(max_recalls * sizeof(uint64_t));
    if (!recalled_ids) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate recall buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recalled_ids is NULL");

        return NULL;
    }

    uint32_t num_recalled = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_engram_recall(self->agent, &msg, recalled_ids, max_recalls, &num_recalled);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        nimcp_free(recalled_ids);
        PyErr_SetString(PyExc_RuntimeError, "Failed to recall engrams");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_engram_recall: validation failed");
        return NULL;
    }

    PyObject* list = PyList_New(num_recalled);
    for (uint32_t i = 0; i < num_recalled; i++) {
        PyList_SetItem(list, i, PyLong_FromUnsignedLongLong(recalled_ids[i]));
    }

    nimcp_free(recalled_ids);
    return list;
}

/**
 * @brief HealthAgent.use_engram_get_stats() -> (active, consolidated, avg_strength)
 */
static PyObject* HealthAgent_use_engram_get_stats(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t active = 0;
    uint32_t consolidated = 0;
    float avg_strength = 0.0f;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_engram_get_stats(self->agent, &active, &consolidated, &avg_strength);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get engram stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_engram_get_stats: validation failed");
        return NULL;
    }

    return Py_BuildValue("(IIf)", active, consolidated, avg_strength);
}

/* ============================================================================
 * Systems Consolidation USE Functions
 * ============================================================================ */

/**
 * @brief HealthAgent.use_consolidation_replay(count) -> bool
 */
static PyObject* HealthAgent_use_consolidation_replay(HealthAgentObject* self, PyObject* args)
{
    unsigned int count;

    if (!PyArg_ParseTuple(args, "I", &count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_consolidation_replay: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_consolidation_replay(self->agent, count);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.use_consolidation_extract_semantics() -> bool
 */
static PyObject* HealthAgent_use_consolidation_extract_semantics(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_consolidation_extract_semantics(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.use_consolidation_get_stats() -> (cortical_nodes, replays, transfers)
 */
static PyObject* HealthAgent_use_consolidation_get_stats(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t cortical_nodes = 0;
    uint64_t replays = 0;
    uint64_t transfers = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_use_consolidation_get_stats(self->agent, &cortical_nodes, &replays, &transfers);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get consolidation stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_use_consolidation_get_stats: validation failed");
        return NULL;
    }

    return Py_BuildValue("(IKK)", cortical_nodes, replays, transfers);
}

/* ============================================================================
 * Recovery Actions
 * ============================================================================ */

/**
 * @brief HealthAgent.trigger_gc(force) -> bool
 */
static PyObject* HealthAgent_trigger_gc(HealthAgentObject* self, PyObject* args)
{
    int force = 0;

    if (!PyArg_ParseTuple(args, "|p", &force)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_trigger_gc: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_trigger_gc(self->agent, (bool)force);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.create_checkpoint(name) -> bool
 */
static PyObject* HealthAgent_create_checkpoint(HealthAgentObject* self, PyObject* args)
{
    const char* name;

    if (!PyArg_ParseTuple(args, "s", &name)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "HealthAgent_create_checkpoint: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_create_checkpoint(self->agent, name);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.rollback(checkpoint_id=0) -> bool
 *
 * @param checkpoint_id Specific checkpoint ID, or 0 for latest
 */
static PyObject* HealthAgent_rollback(HealthAgentObject* self, PyObject* args)
{
    unsigned long long checkpoint_id = 0;

    if (!PyArg_ParseTuple(args, "|K", &checkpoint_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_rollback: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_rollback(self->agent, (uint64_t)checkpoint_id);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.reduce_load(factor) -> bool
 */
static PyObject* HealthAgent_reduce_load(HealthAgentObject* self, PyObject* args)
{
    float factor;

    if (!PyArg_ParseTuple(args, "f", &factor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_reduce_load: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (factor < 0.0f || factor > 1.0f) {
        PyErr_SetString(PyExc_ValueError, "Factor must be 0.0-1.0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HealthAgent_reduce_load: validation failed");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_reduce_load(self->agent, factor);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief HealthAgent.restore_load() -> bool
 */
static PyObject* HealthAgent_restore_load(HealthAgentObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = nimcp_health_agent_restore_load(self->agent);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

static PyObject* HealthAgent_repr(HealthAgentObject* self)
{
    bool running = nimcp_health_agent_is_running(self->agent);
    return PyUnicode_FromFormat("HealthAgent(running=%s)", running ? "True" : "False");
}

/* ============================================================================
 * Method Table
 * ============================================================================ */

static PyMethodDef HealthAgent_methods[] = {
    /* Basic methods */
    {"start", (PyCFunction)HealthAgent_start, METH_NOARGS,
     "Start the health agent monitoring thread"},
    {"stop", (PyCFunction)HealthAgent_stop, METH_NOARGS,
     "Stop the health agent"},
    {"is_running", (PyCFunction)HealthAgent_is_running, METH_NOARGS,
     "Check if agent is running"},
    {"heartbeat", (PyCFunction)HealthAgent_heartbeat, METH_NOARGS,
     "Send heartbeat to agent"},
    {"get_stats", (PyCFunction)HealthAgent_get_stats, METH_NOARGS,
     "Get agent statistics"},
    {"request_check", (PyCFunction)HealthAgent_request_check, METH_NOARGS,
     "Request immediate health check"},

    /* Portia USE functions */
    {"use_portia_set_tier", (PyCFunction)HealthAgent_use_portia_set_tier, METH_VARARGS,
     "Set platform tier via Portia (tier)"},
    {"use_portia_degrade", (PyCFunction)HealthAgent_use_portia_degrade, METH_VARARGS,
     "Set degradation level (0-4)"},
    {"use_portia_get_recommended_neurons", (PyCFunction)HealthAgent_use_portia_get_recommended_neurons, METH_NOARGS,
     "Get recommended neuron count for current resources"},
    {"use_portia_get_status", (PyCFunction)HealthAgent_use_portia_get_status, METH_NOARGS,
     "Get Portia status -> (power_state, thermal_state, degradation_level)"},

    /* Dragonfly USE functions */
    {"use_dragonfly_track_anomaly", (PyCFunction)HealthAgent_use_dragonfly_track_anomaly, METH_VARARGS,
     "Track anomaly with Dragonfly (msg_type, severity, source, description) -> target_id"},
    {"use_dragonfly_predict", (PyCFunction)HealthAgent_use_dragonfly_predict, METH_VARARGS,
     "Get failure prediction (target_id) -> (time_to_failure, confidence)"},
    {"use_dragonfly_pursue", (PyCFunction)HealthAgent_use_dragonfly_pursue, METH_NOARGS,
     "Start pursuing current tracked anomaly"},
    {"use_dragonfly_abort", (PyCFunction)HealthAgent_use_dragonfly_abort, METH_NOARGS,
     "Abort current pursuit"},
    {"use_dragonfly_get_mode", (PyCFunction)HealthAgent_use_dragonfly_get_mode, METH_NOARGS,
     "Get Dragonfly mode (0=idle, 1=scanning, 2=tracking, 3=pursuing, 4=intercepting)"},

    /* Swarm Immune USE functions */
    {"use_swarm_detect_threat", (PyCFunction)HealthAgent_use_swarm_detect_threat, METH_VARARGS,
     "Detect threat in data (data_bytes, source_id) -> (detected, threat_id)"},
    {"use_swarm_generate_response", (PyCFunction)HealthAgent_use_swarm_generate_response, METH_VARARGS,
     "Generate response to threat (threat_id) -> response_id"},
    {"use_swarm_check_behavior", (PyCFunction)HealthAgent_use_swarm_check_behavior, METH_VARARGS,
     "Check component behavior (component_id) -> anomaly_score"},
    {"use_swarm_add_memory_cell", (PyCFunction)HealthAgent_use_swarm_add_memory_cell, METH_VARARGS,
     "Add memory cell (pattern_bytes, response_type) -> cell_id"},

    /* Swarm Memory USE functions */
    {"use_swarm_memory_store", (PyCFunction)HealthAgent_use_swarm_memory_store, METH_VARARGS,
     "Store pattern (data_bytes, pattern_type, importance) -> pattern_id"},
    {"use_swarm_memory_replay", (PyCFunction)HealthAgent_use_swarm_memory_replay, METH_VARARGS,
     "Trigger replay (count) -> replayed_count"},
    {"use_swarm_memory_consolidate", (PyCFunction)HealthAgent_use_swarm_memory_consolidate, METH_NOARGS,
     "Trigger memory consolidation"},
    {"use_swarm_memory_get_stats", (PyCFunction)HealthAgent_use_swarm_memory_get_stats, METH_NOARGS,
     "Get memory stats -> (total, consolidated, avg_strength)"},

    /* Engram USE functions */
    {"use_engram_encode", (PyCFunction)HealthAgent_use_engram_encode, METH_VARARGS,
     "Encode event as engram (msg_type, severity, source, description) -> engram_id"},
    {"use_engram_recall", (PyCFunction)HealthAgent_use_engram_recall, METH_VARARGS,
     "Recall similar engrams (msg_type, severity, source, description, max) -> [engram_ids]"},
    {"use_engram_get_stats", (PyCFunction)HealthAgent_use_engram_get_stats, METH_NOARGS,
     "Get engram stats -> (active, consolidated, avg_strength)"},

    /* Systems Consolidation USE functions */
    {"use_consolidation_replay", (PyCFunction)HealthAgent_use_consolidation_replay, METH_VARARGS,
     "Trigger consolidation replay (count)"},
    {"use_consolidation_extract_semantics", (PyCFunction)HealthAgent_use_consolidation_extract_semantics, METH_NOARGS,
     "Extract semantic patterns from episodic memories"},
    {"use_consolidation_get_stats", (PyCFunction)HealthAgent_use_consolidation_get_stats, METH_NOARGS,
     "Get consolidation stats -> (cortical_nodes, replays, transfers)"},

    /* Recovery actions */
    {"trigger_gc", (PyCFunction)HealthAgent_trigger_gc, METH_VARARGS,
     "Trigger garbage collection (force=False)"},
    {"create_checkpoint", (PyCFunction)HealthAgent_create_checkpoint, METH_VARARGS,
     "Create checkpoint (name)"},
    {"rollback", (PyCFunction)HealthAgent_rollback, METH_VARARGS,
     "Rollback to checkpoint (checkpoint_id=0 for latest)"},
    {"reduce_load", (PyCFunction)HealthAgent_reduce_load, METH_VARARGS,
     "Reduce system load (factor 0.0-1.0)"},
    {"restore_load", (PyCFunction)HealthAgent_restore_load, METH_NOARGS,
     "Restore system load to normal"},

    {NULL}
};

static PyTypeObject HealthAgentType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.HealthAgent",
    .tp_doc = PyDoc_STR(
        "Health monitoring agent with USE functions for cognitive modules.\n\n"
        "Provides autonomous health monitoring and integration with:\n"
        "- Portia: Platform tier and degradation control\n"
        "- Dragonfly: Predictive anomaly tracking\n"
        "- Swarm Immune: Distributed threat detection\n"
        "- Swarm Memory: Distributed pattern storage\n"
        "- Engram: Memory encoding and recall\n"
        "- Systems Consolidation: Memory transfer\n\n"
        "Args:\n"
        "    name (str): Agent name for logging\n"
        "    heartbeat_ms (int): Heartbeat interval\n"
        "    watchdog_ms (int): Watchdog timeout\n"
        "    check_ms (int): Health check interval"
    ),
    .tp_basicsize = sizeof(HealthAgentObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = HealthAgent_new,
    .tp_init = (initproc)HealthAgent_init,
    .tp_dealloc = (destructor)HealthAgent_dealloc,
    .tp_repr = (reprfunc)HealthAgent_repr,
    .tp_methods = HealthAgent_methods,
};

/* ============================================================================
 * Module initialization
 * ============================================================================ */

int init_health_agent_module(PyObject* module)
{
    LOG_MODULE_INFO("bindings.python.health_agent", "Initializing health agent module");

    /* Ready types */
    if (PyType_Ready(&HealthAgentStatsType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_health_agent_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&HealthAgentType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_health_agent_module: validation failed");
        return -1;
    }

    /* Add types */
    Py_INCREF(&HealthAgentStatsType);
    if (PyModule_AddObject(module, "HealthAgentStats", (PyObject*)&HealthAgentStatsType) < 0) {
        Py_DECREF(&HealthAgentStatsType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_health_agent_module: validation failed");
        return -1;
    }

    Py_INCREF(&HealthAgentType);
    if (PyModule_AddObject(module, "HealthAgent", (PyObject*)&HealthAgentType) < 0) {
        Py_DECREF(&HealthAgentType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_health_agent_module: validation failed");
        return -1;
    }

    /* Add message type constants */
    PyModule_AddIntConstant(module, "HEALTH_MSG_ANOMALY_DETECTED", HEALTH_MSG_ANOMALY_DETECTED);
    PyModule_AddIntConstant(module, "HEALTH_MSG_CYTOKINE_SIGNAL", HEALTH_MSG_CYTOKINE_SIGNAL);
    PyModule_AddIntConstant(module, "HEALTH_MSG_EMERGENCY", HEALTH_MSG_EMERGENCY);
    PyModule_AddIntConstant(module, "HEALTH_MSG_RECOVERY_REQUEST", HEALTH_MSG_RECOVERY_REQUEST);
    PyModule_AddIntConstant(module, "HEALTH_MSG_STATE_CORRUPTION", HEALTH_MSG_STATE_CORRUPTION);
    PyModule_AddIntConstant(module, "HEALTH_MSG_HEARTBEAT_TIMEOUT", HEALTH_MSG_HEARTBEAT_TIMEOUT);
    PyModule_AddIntConstant(module, "HEALTH_MSG_DEADLOCK_DETECTED", HEALTH_MSG_DEADLOCK_DETECTED);
    PyModule_AddIntConstant(module, "HEALTH_MSG_NAN_DETECTED", HEALTH_MSG_NAN_DETECTED);
    PyModule_AddIntConstant(module, "HEALTH_MSG_MEMORY_CORRUPTION", HEALTH_MSG_MEMORY_CORRUPTION);
    PyModule_AddIntConstant(module, "HEALTH_MSG_RESOURCE_EXHAUSTION", HEALTH_MSG_RESOURCE_EXHAUSTION);
    PyModule_AddIntConstant(module, "HEALTH_MSG_STATUS_UPDATE", HEALTH_MSG_STATUS_UPDATE);

    /* Add severity constants */
    PyModule_AddIntConstant(module, "HEALTH_SEVERITY_INFO", HEALTH_SEVERITY_INFO);
    PyModule_AddIntConstant(module, "HEALTH_SEVERITY_WARNING", HEALTH_SEVERITY_WARNING);
    PyModule_AddIntConstant(module, "HEALTH_SEVERITY_ERROR", HEALTH_SEVERITY_ERROR);
    PyModule_AddIntConstant(module, "HEALTH_SEVERITY_CRITICAL", HEALTH_SEVERITY_CRITICAL);
    PyModule_AddIntConstant(module, "HEALTH_SEVERITY_FATAL", HEALTH_SEVERITY_FATAL);

    /* Add source constants */
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_UNKNOWN", HEALTH_SOURCE_UNKNOWN);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_MEMORY", HEALTH_SOURCE_MEMORY);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_THREADING", HEALTH_SOURCE_THREADING);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_NEURAL", HEALTH_SOURCE_NEURAL);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_KG", HEALTH_SOURCE_KG);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_IMMUNE", HEALTH_SOURCE_IMMUNE);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_IO", HEALTH_SOURCE_IO);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_BRAIN_REGION", HEALTH_SOURCE_BRAIN_REGION);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_CHECKPOINT", HEALTH_SOURCE_CHECKPOINT);
    PyModule_AddIntConstant(module, "HEALTH_SOURCE_HEARTBEAT", HEALTH_SOURCE_HEARTBEAT);

    /* Add recovery action constants */
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_NONE", HEALTH_RECOVERY_NONE);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_GC", HEALTH_RECOVERY_GC);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_CHECKPOINT", HEALTH_RECOVERY_CHECKPOINT);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_ROLLBACK", HEALTH_RECOVERY_ROLLBACK);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_RESTART_THREAD", HEALTH_RECOVERY_RESTART_THREAD);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_CLEAR_NAN", HEALTH_RECOVERY_CLEAR_NAN);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_REDUCE_LOAD", HEALTH_RECOVERY_REDUCE_LOAD);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_QUARANTINE", HEALTH_RECOVERY_QUARANTINE);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_EMERGENCY_SAVE", HEALTH_RECOVERY_EMERGENCY_SAVE);
    PyModule_AddIntConstant(module, "HEALTH_RECOVERY_FULL_RESET", HEALTH_RECOVERY_FULL_RESET);

    /* Add Dragonfly mode constants */
    PyModule_AddIntConstant(module, "DRAGONFLY_MODE_IDLE", 0);
    PyModule_AddIntConstant(module, "DRAGONFLY_MODE_SCANNING", 1);
    PyModule_AddIntConstant(module, "DRAGONFLY_MODE_TRACKING", 2);
    PyModule_AddIntConstant(module, "DRAGONFLY_MODE_PURSUING", 3);
    PyModule_AddIntConstant(module, "DRAGONFLY_MODE_INTERCEPTING", 4);

    /* Add pattern type constants for swarm memory */
    PyModule_AddIntConstant(module, "PATTERN_TYPE_EPISODIC", 0);
    PyModule_AddIntConstant(module, "PATTERN_TYPE_SEMANTIC", 1);
    PyModule_AddIntConstant(module, "PATTERN_TYPE_PROCEDURAL", 2);
    PyModule_AddIntConstant(module, "PATTERN_TYPE_THREAT", 3);
    PyModule_AddIntConstant(module, "PATTERN_TYPE_SPATIAL", 4);

    LOG_MODULE_INFO("bindings.python.health_agent", "Health agent module initialized successfully");
    return 0;
}
