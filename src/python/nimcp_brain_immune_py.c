/**
 * @file nimcp_brain_immune_py.c
 * @brief Python bindings for NIMCP Brain Immune System
 *
 * Provides Python types for:
 * - BrainImmuneSystem: Adaptive defense coordination layer
 * - BrainAntigen: Threat representation
 * - BrainImmuneStats: System statistics
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_immune_py)

/* ============================================================================
 * BrainAntigen Type (read-only container)
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    uint32_t id;
    int source;
    uint32_t severity;
    float confidence;
    float danger_signal;
    uint32_t source_node_id;
    bool processed;
    bool neutralized;
} BrainAntigenObject;

static PyMemberDef BrainAntigen_members[] = {
    {"id", T_UINT, offsetof(BrainAntigenObject, id), READONLY, "Antigen ID"},
    {"source", T_INT, offsetof(BrainAntigenObject, source), READONLY, "Detection source"},
    {"severity", T_UINT, offsetof(BrainAntigenObject, severity), READONLY, "Severity (1-10)"},
    {"confidence", T_FLOAT, offsetof(BrainAntigenObject, confidence), READONLY, "Detection confidence"},
    {"danger_signal", T_FLOAT, offsetof(BrainAntigenObject, danger_signal), READONLY, "Cumulative danger"},
    {"source_node_id", T_UINT, offsetof(BrainAntigenObject, source_node_id), READONLY, "Source node"},
    {"processed", T_BOOL, offsetof(BrainAntigenObject, processed), READONLY, "Processed by T cells"},
    {"neutralized", T_BOOL, offsetof(BrainAntigenObject, neutralized), READONLY, "Threat neutralized"},
    {NULL}
};

static PyObject* BrainAntigen_repr(BrainAntigenObject* self)
{
    return PyUnicode_FromFormat("BrainAntigen(id=%u, severity=%u, neutralized=%s)",
                                self->id, self->severity,
                                self->neutralized ? "True" : "False");
}

static PyTypeObject BrainAntigenType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.BrainAntigen",
    .tp_doc = PyDoc_STR("Antigen - processed threat for immune response"),
    .tp_basicsize = sizeof(BrainAntigenObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_repr = (reprfunc)BrainAntigen_repr,
    .tp_members = BrainAntigen_members,
};

/* Helper to create BrainAntigen from C struct */
static PyObject* BrainAntigen_FromC(const brain_antigen_t* antigen)
{
    BrainAntigenObject* obj = PyObject_New(BrainAntigenObject, &BrainAntigenType);
    if (obj == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obj is NULL");

        return NULL;
    }

    obj->id = antigen->id;
    obj->source = (int)antigen->source;
    obj->severity = antigen->severity;
    obj->confidence = antigen->confidence;
    obj->danger_signal = antigen->danger_signal;
    obj->source_node_id = antigen->source_node_id;
    obj->processed = antigen->processed;
    obj->neutralized = antigen->neutralized;

    return (PyObject*)obj;
}

/* ============================================================================
 * BrainImmuneStats Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    uint32_t active_b_cells;
    uint32_t active_t_cells;
    uint32_t active_antibodies;
    uint32_t memory_cells;
    uint64_t antigens_processed;
    uint64_t threats_neutralized;
    uint64_t responses_generated;
    uint64_t cytokines_released;
    uint32_t inflammation_sites;
    float avg_response_time_ms;
    float system_health;
    int inflammation_level;
    float cytokine_il1;
    float cytokine_il6;
    float cytokine_il10;
    float cytokine_tnf;
    float cytokine_ifn_gamma;
} BrainImmuneStatsObject;

static PyMemberDef BrainImmuneStats_members[] = {
    {"active_b_cells", T_UINT, offsetof(BrainImmuneStatsObject, active_b_cells), READONLY, "Active B cells"},
    {"active_t_cells", T_UINT, offsetof(BrainImmuneStatsObject, active_t_cells), READONLY, "Active T cells"},
    {"active_antibodies", T_UINT, offsetof(BrainImmuneStatsObject, active_antibodies), READONLY, "Active antibodies"},
    {"memory_cells", T_UINT, offsetof(BrainImmuneStatsObject, memory_cells), READONLY, "Memory cells"},
    {"antigens_processed", T_ULONGLONG, offsetof(BrainImmuneStatsObject, antigens_processed), READONLY, "Total antigens processed"},
    {"threats_neutralized", T_ULONGLONG, offsetof(BrainImmuneStatsObject, threats_neutralized), READONLY, "Threats neutralized"},
    {"responses_generated", T_ULONGLONG, offsetof(BrainImmuneStatsObject, responses_generated), READONLY, "Responses generated"},
    {"cytokines_released", T_ULONGLONG, offsetof(BrainImmuneStatsObject, cytokines_released), READONLY, "Cytokines released"},
    {"inflammation_sites", T_UINT, offsetof(BrainImmuneStatsObject, inflammation_sites), READONLY, "Active inflammation sites"},
    {"avg_response_time_ms", T_FLOAT, offsetof(BrainImmuneStatsObject, avg_response_time_ms), READONLY, "Average response time (ms)"},
    {"system_health", T_FLOAT, offsetof(BrainImmuneStatsObject, system_health), READONLY, "Overall health (0-1)"},
    {"inflammation_level", T_INT, offsetof(BrainImmuneStatsObject, inflammation_level), READONLY, "Current inflammation level"},
    {"cytokine_il1", T_FLOAT, offsetof(BrainImmuneStatsObject, cytokine_il1), READONLY, "IL-1β level"},
    {"cytokine_il6", T_FLOAT, offsetof(BrainImmuneStatsObject, cytokine_il6), READONLY, "IL-6 level"},
    {"cytokine_il10", T_FLOAT, offsetof(BrainImmuneStatsObject, cytokine_il10), READONLY, "IL-10 level"},
    {"cytokine_tnf", T_FLOAT, offsetof(BrainImmuneStatsObject, cytokine_tnf), READONLY, "TNF-α level"},
    {"cytokine_ifn_gamma", T_FLOAT, offsetof(BrainImmuneStatsObject, cytokine_ifn_gamma), READONLY, "IFN-γ level"},
    {NULL}
};

static PyObject* BrainImmuneStats_repr(BrainImmuneStatsObject* self)
{
    return PyUnicode_FromFormat("BrainImmuneStats(health=%.2f, b=%u, t=%u, neutralized=%llu)",
                                self->system_health, self->active_b_cells,
                                self->active_t_cells, self->threats_neutralized);
}

static PyTypeObject BrainImmuneStatsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.BrainImmuneStats",
    .tp_doc = PyDoc_STR("Brain immune system statistics"),
    .tp_basicsize = sizeof(BrainImmuneStatsObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_repr = (reprfunc)BrainImmuneStats_repr,
    .tp_members = BrainImmuneStats_members,
};

/* Helper to create BrainImmuneStats from C struct */
static PyObject* BrainImmuneStats_FromC(const brain_immune_stats_t* stats)
{
    BrainImmuneStatsObject* obj = PyObject_New(BrainImmuneStatsObject, &BrainImmuneStatsType);
    if (obj == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obj is NULL");

        return NULL;
    }

    obj->active_b_cells = stats->active_b_cells;
    obj->active_t_cells = stats->active_t_cells;
    obj->active_antibodies = stats->active_antibodies;
    obj->memory_cells = stats->memory_cells;
    obj->antigens_processed = stats->antigens_processed;
    obj->threats_neutralized = stats->threats_neutralized;
    obj->responses_generated = stats->responses_generated;
    obj->cytokines_released = stats->cytokines_released;
    obj->inflammation_sites = stats->inflammation_sites;
    obj->avg_response_time_ms = stats->avg_response_time_ms;
    obj->system_health = stats->system_health;
    obj->inflammation_level = (int)stats->inflammation_level;
    obj->cytokine_il1 = stats->cytokine_il1;
    obj->cytokine_il6 = stats->cytokine_il6;
    obj->cytokine_il10 = stats->cytokine_il10;
    obj->cytokine_tnf = stats->cytokine_tnf;
    obj->cytokine_ifn_gamma = stats->cytokine_ifn_gamma;

    return (PyObject*)obj;
}

/* ============================================================================
 * BrainImmuneSystem Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    brain_immune_system_t* system;
} BrainImmuneSystemObject;

static void BrainImmuneSystem_dealloc(BrainImmuneSystemObject* self)
{
    if (self->system != NULL) {
        brain_immune_stop(self->system);
        brain_immune_destroy(self->system);
        self->system = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* BrainImmuneSystem_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    BrainImmuneSystemObject* self = (BrainImmuneSystemObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->system = NULL;
    }
    return (PyObject*)self;
}

static int BrainImmuneSystem_init(BrainImmuneSystemObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"enable_bbb", "enable_bft", "enable_swarm", NULL};
    int enable_bbb = 1;
    int enable_bft = 1;
    int enable_swarm = 1;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ppp", kwlist,
                                      &enable_bbb, &enable_bft, &enable_swarm)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BrainImmuneSystem_init: operation failed");
        return -1;
    }

    brain_immune_config_t config;
    if (brain_immune_default_config(&config) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get default config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BrainImmuneSystem_init: validation failed");
        return -1;
    }

    config.enable_bbb_integration = (bool)enable_bbb;
    config.enable_bft_integration = (bool)enable_bft;
    config.enable_swarm_integration = (bool)enable_swarm;
    config.enable_bio_async = true;
    config.enable_logging = true;

    self->system = brain_immune_create(&config);
    if (self->system == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create brain immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "BrainImmuneSystem_init: validation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief BrainImmuneSystem.start() -> bool
 */
static PyObject* BrainImmuneSystem_start(BrainImmuneSystemObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_start(self->system);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.stop() -> bool
 */
static PyObject* BrainImmuneSystem_stop(BrainImmuneSystemObject* self, PyObject* Py_UNUSED(args))
{
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_stop(self->system);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.update(delta_ms) -> bool
 */
static PyObject* BrainImmuneSystem_update(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned long long delta_ms;

    if (!PyArg_ParseTuple(args, "K", &delta_ms)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_update: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_update(self->system, delta_ms);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.get_stats() -> BrainImmuneStats
 */
static PyObject* BrainImmuneSystem_get_stats(BrainImmuneSystemObject* self, PyObject* Py_UNUSED(args))
{
    brain_immune_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_get_stats(self->system, &stats);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_get_stats: validation failed");
        return NULL;
    }

    return BrainImmuneStats_FromC(&stats);
}

/**
 * @brief BrainImmuneSystem.present_antigen(epitope, severity, source_node) -> int
 *
 * Present a threat signature as an antigen.
 */
static PyObject* BrainImmuneSystem_present_antigen(BrainImmuneSystemObject* self, PyObject* args)
{
    Py_buffer epitope;
    unsigned int severity;
    unsigned int source_node;
    int source = ANTIGEN_SOURCE_MANUAL;

    if (!PyArg_ParseTuple(args, "y*II|i", &epitope, &severity, &source_node, &source)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_present_antigen: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (severity < 1 || severity > 10) {
        PyBuffer_Release(&epitope);
        PyErr_SetString(PyExc_ValueError, "severity must be 1-10");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_present_antigen: validation failed");
        return NULL;
    }

    uint32_t antigen_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_present_antigen(
        self->system,
        (brain_antigen_source_t)source,
        epitope.buf,
        (size_t)epitope.len,
        severity,
        source_node,
        &antigen_id
    );
    Py_END_ALLOW_THREADS

    PyBuffer_Release(&epitope);

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to present antigen");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_present_antigen: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(antigen_id);
}

/**
 * @brief BrainImmuneSystem.get_antigen(antigen_id) -> BrainAntigen or None
 */
static PyObject* BrainImmuneSystem_get_antigen(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;

    if (!PyArg_ParseTuple(args, "I", &antigen_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_get_antigen: PyArg_ParseTuple is NULL");
        return NULL;
    }

    brain_antigen_t antigen_copy;
    if (brain_immune_get_antigen_copy(self->system, antigen_id, &antigen_copy) != 0) {
        Py_RETURN_NONE;
    }

    return BrainAntigen_FromC(&antigen_copy);
}

/**
 * @brief BrainImmuneSystem.activate_b_cell(antigen_id) -> int
 */
static PyObject* BrainImmuneSystem_activate_b_cell(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;

    if (!PyArg_ParseTuple(args, "I", &antigen_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_activate_b_cell: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t b_cell_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_activate_b_cell(self->system, antigen_id, &b_cell_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to activate B cell");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_activate_b_cell: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(b_cell_id);
}

/**
 * @brief BrainImmuneSystem.activate_helper_t(antigen_id) -> int
 */
static PyObject* BrainImmuneSystem_activate_helper_t(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;

    if (!PyArg_ParseTuple(args, "I", &antigen_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_activate_helper_t: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t t_cell_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_activate_helper_t(self->system, antigen_id, &t_cell_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to activate helper T cell");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_activate_helper_t: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(t_cell_id);
}

/**
 * @brief BrainImmuneSystem.activate_killer_t(antigen_id) -> int
 */
static PyObject* BrainImmuneSystem_activate_killer_t(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;

    if (!PyArg_ParseTuple(args, "I", &antigen_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_activate_killer_t: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t t_cell_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_activate_killer_t(self->system, antigen_id, &t_cell_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to activate killer T cell");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_activate_killer_t: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(t_cell_id);
}

/**
 * @brief BrainImmuneSystem.t_cell_kill(t_cell_id, target_node) -> bool
 */
static PyObject* BrainImmuneSystem_t_cell_kill(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int t_cell_id;
    unsigned int target_node;

    if (!PyArg_ParseTuple(args, "II", &t_cell_id, &target_node)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_t_cell_kill: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_t_cell_kill(self->system, t_cell_id, target_node);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.produce_antibody(b_cell_id, ab_class) -> int
 */
static PyObject* BrainImmuneSystem_produce_antibody(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int b_cell_id;
    int ab_class = ANTIBODY_IGG;

    if (!PyArg_ParseTuple(args, "I|i", &b_cell_id, &ab_class)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_produce_antibody: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t antibody_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_produce_antibody(self->system, b_cell_id,
                                           (brain_antibody_class_t)ab_class, &antibody_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to produce antibody");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_produce_antibody: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(antibody_id);
}

/**
 * @brief BrainImmuneSystem.execute_antibody(antibody_id) -> bool
 */
static PyObject* BrainImmuneSystem_execute_antibody(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antibody_id;

    if (!PyArg_ParseTuple(args, "I", &antibody_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_execute_antibody: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_execute_antibody(self->system, antibody_id);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.neutralize(antigen_id, antibody_id) -> bool
 */
static PyObject* BrainImmuneSystem_neutralize(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;
    unsigned int antibody_id;

    if (!PyArg_ParseTuple(args, "II", &antigen_id, &antibody_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_neutralize: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_neutralize(self->system, antigen_id, antibody_id);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.is_neutralized(antigen_id) -> bool
 */
static PyObject* BrainImmuneSystem_is_neutralized(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;

    if (!PyArg_ParseTuple(args, "I", &antigen_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_is_neutralized: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool neutralized = brain_immune_is_neutralized(self->system, antigen_id);
    return PyBool_FromLong(neutralized);
}

/**
 * @brief BrainImmuneSystem.release_cytokine(type, source_cell, concentration, target_region) -> int
 */
static PyObject* BrainImmuneSystem_release_cytokine(BrainImmuneSystemObject* self, PyObject* args)
{
    int type;
    unsigned int source_cell;
    float concentration;
    unsigned int target_region = 0;

    if (!PyArg_ParseTuple(args, "iIf|I", &type, &source_cell, &concentration, &target_region)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_release_cytokine: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (concentration < 0.0f || concentration > 1.0f) {
        PyErr_SetString(PyExc_ValueError, "concentration must be 0.0-1.0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_release_cytokine: validation failed");
        return NULL;
    }

    uint32_t cytokine_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_release_cytokine(
        self->system,
        (brain_cytokine_type_t)type,
        source_cell,
        concentration,
        target_region,
        &cytokine_id
    );
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to release cytokine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_release_cytokine: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(cytokine_id);
}

/**
 * @brief BrainImmuneSystem.get_cytokine_level(type) -> float
 */
static PyObject* BrainImmuneSystem_get_cytokine_level(BrainImmuneSystemObject* self, PyObject* args)
{
    int type;

    if (!PyArg_ParseTuple(args, "i", &type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_get_cytokine_level: PyArg_ParseTuple is NULL");
        return NULL;
    }

    float level = brain_immune_get_cytokine_level(self->system, (brain_cytokine_type_t)type);
    return PyFloat_FromDouble((double)level);
}

/**
 * @brief BrainImmuneSystem.initiate_inflammation(region_id, antigen_id) -> int
 */
static PyObject* BrainImmuneSystem_initiate_inflammation(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int region_id;
    unsigned int antigen_id;

    if (!PyArg_ParseTuple(args, "II", &region_id, &antigen_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_initiate_inflammation: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t site_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_initiate_inflammation(self->system, region_id, antigen_id, &site_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initiate inflammation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_initiate_inflammation: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(site_id);
}

/**
 * @brief BrainImmuneSystem.escalate_inflammation(site_id) -> bool
 */
static PyObject* BrainImmuneSystem_escalate_inflammation(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int site_id;

    if (!PyArg_ParseTuple(args, "I", &site_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_escalate_inflammation: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_escalate_inflammation(self->system, site_id);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.resolve_inflammation(site_id) -> bool
 */
static PyObject* BrainImmuneSystem_resolve_inflammation(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int site_id;

    if (!PyArg_ParseTuple(args, "I", &site_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_resolve_inflammation: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_resolve_inflammation(self->system, site_id);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.get_inflammation_level() -> int
 */
static PyObject* BrainImmuneSystem_get_inflammation_level(BrainImmuneSystemObject* self, PyObject* Py_UNUSED(args))
{
    brain_inflammation_level_t level = brain_immune_get_inflammation_level(self->system);
    return PyLong_FromLong((long)level);
}

/**
 * @brief BrainImmuneSystem.check_memory(antigen_id) -> int or None
 */
static PyObject* BrainImmuneSystem_check_memory(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;

    if (!PyArg_ParseTuple(args, "I", &antigen_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_check_memory: PyArg_ParseTuple is NULL");
        return NULL;
    }

    uint32_t b_cell_id = 0;
    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_check_memory(self->system, antigen_id, &b_cell_id);
    Py_END_ALLOW_THREADS

    if (result != 0) {
        Py_RETURN_NONE;  /* No memory match */
    }

    return PyLong_FromUnsignedLong(b_cell_id);
}

/**
 * @brief BrainImmuneSystem.secondary_response(antigen_id, memory_b_cell_id) -> bool
 */
static PyObject* BrainImmuneSystem_secondary_response(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int antigen_id;
    unsigned int memory_b_cell_id;

    if (!PyArg_ParseTuple(args, "II", &antigen_id, &memory_b_cell_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "BrainImmuneSystem_secondary_response: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_secondary_response(self->system, antigen_id, memory_b_cell_id);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.b_cell_to_memory(b_cell_id) -> bool
 */
static PyObject* BrainImmuneSystem_b_cell_to_memory(BrainImmuneSystemObject* self, PyObject* args)
{
    unsigned int b_cell_id;

    if (!PyArg_ParseTuple(args, "I", &b_cell_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "BrainImmuneSystem_b_cell_to_memory: PyArg_ParseTuple is NULL");
        return NULL;
    }

    int result;

    Py_BEGIN_ALLOW_THREADS
    result = brain_immune_b_cell_to_memory(self->system, b_cell_id);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result == 0);
}

/**
 * @brief BrainImmuneSystem.get_phase() -> int
 */
static PyObject* BrainImmuneSystem_get_phase(BrainImmuneSystemObject* self, PyObject* Py_UNUSED(args))
{
    brain_immune_phase_t phase = brain_immune_get_phase(self->system);
    return PyLong_FromLong((long)phase);
}

static PyObject* BrainImmuneSystem_repr(BrainImmuneSystemObject* self)
{
    brain_immune_phase_t phase = brain_immune_get_phase(self->system);
    return PyUnicode_FromFormat("BrainImmuneSystem(phase=%d)", (int)phase);
}

static PyMethodDef BrainImmuneSystem_methods[] = {
    {"start", (PyCFunction)BrainImmuneSystem_start, METH_NOARGS,
     "Start immune system monitoring"},
    {"stop", (PyCFunction)BrainImmuneSystem_stop, METH_NOARGS,
     "Stop immune system"},
    {"update", (PyCFunction)BrainImmuneSystem_update, METH_VARARGS,
     "Update immune system state (delta_ms)"},
    {"get_stats", (PyCFunction)BrainImmuneSystem_get_stats, METH_NOARGS,
     "Get immune system statistics"},
    {"present_antigen", (PyCFunction)BrainImmuneSystem_present_antigen, METH_VARARGS,
     "Present threat signature as antigen (epitope, severity, source_node)"},
    {"get_antigen", (PyCFunction)BrainImmuneSystem_get_antigen, METH_VARARGS,
     "Get antigen by ID"},
    {"activate_b_cell", (PyCFunction)BrainImmuneSystem_activate_b_cell, METH_VARARGS,
     "Activate B cell for antigen"},
    {"activate_helper_t", (PyCFunction)BrainImmuneSystem_activate_helper_t, METH_VARARGS,
     "Activate helper T cell"},
    {"activate_killer_t", (PyCFunction)BrainImmuneSystem_activate_killer_t, METH_VARARGS,
     "Activate killer T cell"},
    {"t_cell_kill", (PyCFunction)BrainImmuneSystem_t_cell_kill, METH_VARARGS,
     "Execute killer T cell action"},
    {"produce_antibody", (PyCFunction)BrainImmuneSystem_produce_antibody, METH_VARARGS,
     "Produce antibody from B cell"},
    {"execute_antibody", (PyCFunction)BrainImmuneSystem_execute_antibody, METH_VARARGS,
     "Execute antibody response"},
    {"neutralize", (PyCFunction)BrainImmuneSystem_neutralize, METH_VARARGS,
     "Mark antigen as neutralized"},
    {"is_neutralized", (PyCFunction)BrainImmuneSystem_is_neutralized, METH_VARARGS,
     "Check if antigen is neutralized"},
    {"release_cytokine", (PyCFunction)BrainImmuneSystem_release_cytokine, METH_VARARGS,
     "Release cytokine signal"},
    {"get_cytokine_level", (PyCFunction)BrainImmuneSystem_get_cytokine_level, METH_VARARGS,
     "Get cytokine concentration"},
    {"initiate_inflammation", (PyCFunction)BrainImmuneSystem_initiate_inflammation, METH_VARARGS,
     "Initiate inflammation at region"},
    {"escalate_inflammation", (PyCFunction)BrainImmuneSystem_escalate_inflammation, METH_VARARGS,
     "Escalate inflammation severity"},
    {"resolve_inflammation", (PyCFunction)BrainImmuneSystem_resolve_inflammation, METH_VARARGS,
     "Resolve inflammation"},
    {"get_inflammation_level", (PyCFunction)BrainImmuneSystem_get_inflammation_level, METH_NOARGS,
     "Get current inflammation level"},
    {"check_memory", (PyCFunction)BrainImmuneSystem_check_memory, METH_VARARGS,
     "Check for memory cell match"},
    {"secondary_response", (PyCFunction)BrainImmuneSystem_secondary_response, METH_VARARGS,
     "Trigger secondary (memory) response"},
    {"b_cell_to_memory", (PyCFunction)BrainImmuneSystem_b_cell_to_memory, METH_VARARGS,
     "Convert B cell to memory cell"},
    {"get_phase", (PyCFunction)BrainImmuneSystem_get_phase, METH_NOARGS,
     "Get current immune phase"},
    {NULL}
};

static PyTypeObject BrainImmuneSystemType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.BrainImmuneSystem",
    .tp_doc = PyDoc_STR(
        "Brain immune system - adaptive defense coordination layer.\n\n"
        "Maps biological immune concepts to security modules:\n"
        "- B cells -> Swarm immune memory cells\n"
        "- T cells -> BFT quarantine actions\n"
        "- Antibodies -> Coordinated response strategies\n"
        "- Cytokines -> Bio-async signaling\n"
        "- Inflammation -> Hierarchical recovery\n\n"
        "Args:\n"
        "    enable_bbb (bool): Enable BBB integration\n"
        "    enable_bft (bool): Enable BFT integration\n"
        "    enable_swarm (bool): Enable swarm immune integration"
    ),
    .tp_basicsize = sizeof(BrainImmuneSystemObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = BrainImmuneSystem_new,
    .tp_init = (initproc)BrainImmuneSystem_init,
    .tp_dealloc = (destructor)BrainImmuneSystem_dealloc,
    .tp_repr = (reprfunc)BrainImmuneSystem_repr,
    .tp_methods = BrainImmuneSystem_methods,
};

/* ============================================================================
 * Module initialization
 * ============================================================================ */

int init_brain_immune_module(PyObject* module)
{
    LOG_MODULE_INFO("bindings.python.brain_immune", "Initializing brain immune module");

    /* Ready types */
    if (PyType_Ready(&BrainAntigenType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_brain_immune_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&BrainImmuneStatsType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_brain_immune_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&BrainImmuneSystemType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_brain_immune_module: validation failed");
        return -1;
    }

    /* Add types */
    Py_INCREF(&BrainAntigenType);
    if (PyModule_AddObject(module, "BrainAntigen", (PyObject*)&BrainAntigenType) < 0) {
        Py_DECREF(&BrainAntigenType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_brain_immune_module: validation failed");
        return -1;
    }

    Py_INCREF(&BrainImmuneStatsType);
    if (PyModule_AddObject(module, "BrainImmuneStats", (PyObject*)&BrainImmuneStatsType) < 0) {
        Py_DECREF(&BrainImmuneStatsType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_brain_immune_module: validation failed");
        return -1;
    }

    Py_INCREF(&BrainImmuneSystemType);
    if (PyModule_AddObject(module, "BrainImmuneSystem", (PyObject*)&BrainImmuneSystemType) < 0) {
        Py_DECREF(&BrainImmuneSystemType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_brain_immune_module: validation failed");
        return -1;
    }

    /* Add B cell state constants */
    PyModule_AddIntConstant(module, "B_CELL_NAIVE", B_CELL_NAIVE);
    PyModule_AddIntConstant(module, "B_CELL_ACTIVATED", B_CELL_ACTIVATED);
    PyModule_AddIntConstant(module, "B_CELL_PLASMA", B_CELL_PLASMA);
    PyModule_AddIntConstant(module, "B_CELL_MEMORY", B_CELL_MEMORY);
    PyModule_AddIntConstant(module, "B_CELL_APOPTOTIC", B_CELL_APOPTOTIC);

    /* Add T cell type constants */
    PyModule_AddIntConstant(module, "T_CELL_NAIVE", T_CELL_NAIVE);
    PyModule_AddIntConstant(module, "T_CELL_HELPER", T_CELL_HELPER);
    PyModule_AddIntConstant(module, "T_CELL_KILLER", T_CELL_KILLER);
    PyModule_AddIntConstant(module, "T_CELL_REGULATORY", T_CELL_REGULATORY);
    PyModule_AddIntConstant(module, "T_CELL_MEMORY", T_CELL_MEMORY);

    /* Add antibody class constants */
    PyModule_AddIntConstant(module, "ANTIBODY_IGM", ANTIBODY_IGM);
    PyModule_AddIntConstant(module, "ANTIBODY_IGG", ANTIBODY_IGG);
    PyModule_AddIntConstant(module, "ANTIBODY_IGE", ANTIBODY_IGE);

    /* Add cytokine type constants */
    PyModule_AddIntConstant(module, "BRAIN_CYTOKINE_IL1", BRAIN_CYTOKINE_IL1);
    PyModule_AddIntConstant(module, "BRAIN_CYTOKINE_IL6", BRAIN_CYTOKINE_IL6);
    PyModule_AddIntConstant(module, "BRAIN_CYTOKINE_IL10", BRAIN_CYTOKINE_IL10);
    PyModule_AddIntConstant(module, "BRAIN_CYTOKINE_TNF", BRAIN_CYTOKINE_TNF);
    PyModule_AddIntConstant(module, "BRAIN_CYTOKINE_IFN_GAMMA", BRAIN_CYTOKINE_IFN_GAMMA);

    /* Add antigen source constants */
    PyModule_AddIntConstant(module, "ANTIGEN_SOURCE_BBB", ANTIGEN_SOURCE_BBB);
    PyModule_AddIntConstant(module, "ANTIGEN_SOURCE_BFT", ANTIGEN_SOURCE_BFT);
    PyModule_AddIntConstant(module, "ANTIGEN_SOURCE_ANOMALY", ANTIGEN_SOURCE_ANOMALY);
    PyModule_AddIntConstant(module, "ANTIGEN_SOURCE_SWARM", ANTIGEN_SOURCE_SWARM);
    PyModule_AddIntConstant(module, "ANTIGEN_SOURCE_MANUAL", ANTIGEN_SOURCE_MANUAL);

    /* Add inflammation level constants */
    PyModule_AddIntConstant(module, "INFLAMMATION_NONE", INFLAMMATION_NONE);
    PyModule_AddIntConstant(module, "INFLAMMATION_LOCAL", INFLAMMATION_LOCAL);
    PyModule_AddIntConstant(module, "INFLAMMATION_REGIONAL", INFLAMMATION_REGIONAL);
    PyModule_AddIntConstant(module, "INFLAMMATION_SYSTEMIC", INFLAMMATION_SYSTEMIC);
    PyModule_AddIntConstant(module, "INFLAMMATION_STORM", INFLAMMATION_STORM);

    /* Add immune phase constants */
    PyModule_AddIntConstant(module, "IMMUNE_PHASE_SURVEILLANCE", IMMUNE_PHASE_SURVEILLANCE);
    PyModule_AddIntConstant(module, "IMMUNE_PHASE_RECOGNITION", IMMUNE_PHASE_RECOGNITION);
    PyModule_AddIntConstant(module, "IMMUNE_PHASE_ACTIVATION", IMMUNE_PHASE_ACTIVATION);
    PyModule_AddIntConstant(module, "IMMUNE_PHASE_EFFECTOR", IMMUNE_PHASE_EFFECTOR);
    PyModule_AddIntConstant(module, "IMMUNE_PHASE_RESOLUTION", IMMUNE_PHASE_RESOLUTION);
    PyModule_AddIntConstant(module, "IMMUNE_PHASE_MEMORY", IMMUNE_PHASE_MEMORY);

    LOG_MODULE_INFO("bindings.python.brain_immune", "Brain immune module initialized successfully");
    return 0;
}
