/**
 * @file nimcp_swarm_py.c
 * @brief Python bindings for NIMCP Swarm Intelligence System
 *
 * Provides Python types for:
 * - SwarmBrain: Main swarm brain coordinator
 * - SwarmBrainConfig: Configuration type
 * - SwarmStats: Statistics type
 * - PerceptionData: Sensor observation type
 * - ThreatData: Threat warning type
 * - VoteProposal: Consensus vote proposal
 * - NeuromodState: Neuromodulator state
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "swarm/nimcp_swarm_brain.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_py)

/* ============================================================================
 * SwarmStats Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    uint64_t messages_sent;
    uint64_t messages_received;
    uint32_t peers_connected;
    uint32_t emergence_tier_changes;
    uint32_t votes_completed;
    double avg_latency_ms;
    float workspace_coherence;
    uint64_t uptime_ms;
} SwarmStatsObject;

static PyMemberDef SwarmStats_members[] = {
    {"messages_sent", T_ULONGLONG, offsetof(SwarmStatsObject, messages_sent), READONLY, "Total messages sent"},
    {"messages_received", T_ULONGLONG, offsetof(SwarmStatsObject, messages_received), READONLY, "Total messages received"},
    {"peers_connected", T_UINT, offsetof(SwarmStatsObject, peers_connected), READONLY, "Connected peers"},
    {"emergence_tier_changes", T_UINT, offsetof(SwarmStatsObject, emergence_tier_changes), READONLY, "Tier transitions"},
    {"votes_completed", T_UINT, offsetof(SwarmStatsObject, votes_completed), READONLY, "Completed votes"},
    {"avg_latency_ms", T_DOUBLE, offsetof(SwarmStatsObject, avg_latency_ms), READONLY, "Average latency (ms)"},
    {"workspace_coherence", T_FLOAT, offsetof(SwarmStatsObject, workspace_coherence), READONLY, "Workspace coherence"},
    {"uptime_ms", T_ULONGLONG, offsetof(SwarmStatsObject, uptime_ms), READONLY, "Uptime (ms)"},
    {NULL}
};

static PyObject* SwarmStats_repr(SwarmStatsObject* self)
{
    return PyUnicode_FromFormat("SwarmStats(peers=%u, coherence=%.2f, uptime=%llu)",
                                self->peers_connected, self->workspace_coherence,
                                self->uptime_ms);
}

static PyTypeObject SwarmStatsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.SwarmStats",
    .tp_doc = PyDoc_STR("Swarm brain statistics"),
    .tp_basicsize = sizeof(SwarmStatsObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_repr = (reprfunc)SwarmStats_repr,
    .tp_members = SwarmStats_members,
};

/* Helper to create SwarmStats from C struct */
static PyObject* SwarmStats_FromC(const swarm_stats_t* stats)
{
    SwarmStatsObject* obj = PyObject_New(SwarmStatsObject, &SwarmStatsType);
    if (obj == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obj is NULL");

        return NULL;
    }

    obj->messages_sent = stats->messages_sent;
    obj->messages_received = stats->messages_received;
    obj->peers_connected = stats->peers_connected;
    obj->emergence_tier_changes = stats->emergence_tier_changes;
    obj->votes_completed = stats->votes_completed;
    obj->avg_latency_ms = stats->avg_latency_ms;
    obj->workspace_coherence = stats->workspace_coherence;
    obj->uptime_ms = stats->uptime_ms;

    return (PyObject*)obj;
}

/* ============================================================================
 * PerceptionData Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    uint32_t sensor_type;
    PyObject* values;  /* List of floats */
    float confidence;
} PerceptionDataObject;

static void PerceptionData_dealloc(PerceptionDataObject* self)
{
    Py_XDECREF(self->values);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* PerceptionData_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    PerceptionDataObject* self = (PerceptionDataObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->sensor_type = 0;
        self->values = PyList_New(0);
        if (self->values == NULL) {
            Py_DECREF(self);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PerceptionData_new: validation failed");
            return NULL;
        }
        self->confidence = 1.0f;
    }
    return (PyObject*)self;
}

static int PerceptionData_init(PerceptionDataObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"sensor_type", "values", "confidence", NULL};
    unsigned int sensor_type = 0;
    PyObject* values = NULL;
    float confidence = 1.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IO!f", kwlist,
                                      &sensor_type, &PyList_Type, &values, &confidence)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "PerceptionData_init: operation failed");
        return -1;
    }

    self->sensor_type = sensor_type;
    self->confidence = confidence;

    if (values != NULL) {
        Py_XDECREF(self->values);
        Py_INCREF(values);
        self->values = values;
    }

    return 0;
}

static PyMemberDef PerceptionData_members[] = {
    {"sensor_type", T_UINT, offsetof(PerceptionDataObject, sensor_type), 0, "Sensor type ID"},
    {"values", T_OBJECT_EX, offsetof(PerceptionDataObject, values), 0, "Sensor values"},
    {"confidence", T_FLOAT, offsetof(PerceptionDataObject, confidence), 0, "Observation confidence"},
    {NULL}
};

static PyTypeObject PerceptionDataType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.PerceptionData",
    .tp_doc = PyDoc_STR("Perception data - sensor observations for swarm sharing"),
    .tp_basicsize = sizeof(PerceptionDataObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PerceptionData_new,
    .tp_init = (initproc)PerceptionData_init,
    .tp_dealloc = (destructor)PerceptionData_dealloc,
    .tp_members = PerceptionData_members,
};

/* Helper to convert PerceptionData to C struct */
static bool PerceptionData_ToC(PerceptionDataObject* obj, perception_data_t* out)
{
    out->sensor_type = obj->sensor_type;
    out->confidence = obj->confidence;
    out->timestamp_ms = 0;  /* Will be filled by swarm brain */

    Py_ssize_t count = PyList_Size(obj->values);
    if (count > 8) count = 8;
    out->value_count = (uint32_t)count;

    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject* item = PyList_GetItem(obj->values, i);
        if (PyFloat_Check(item) || PyLong_Check(item)) {
            out->values[i] = (float)PyFloat_AsDouble(item);
        } else {
            out->values[i] = 0.0f;
        }
    }

    return true;
}

/* ============================================================================
 * ThreatData Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    uint32_t threat_type;
    float position[3];
    float severity;
    char* description;
} ThreatDataObject;

static void ThreatData_dealloc(ThreatDataObject* self)
{
    if (self->description) {
        nimcp_free(self->description);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* ThreatData_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    ThreatDataObject* self = (ThreatDataObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->threat_type = 0;
        self->position[0] = 0.0f;
        self->position[1] = 0.0f;
        self->position[2] = 0.0f;
        self->severity = 0.0f;
        self->description = NULL;
    }
    return (PyObject*)self;
}

static int ThreatData_init(ThreatDataObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"threat_type", "position", "severity", "description", NULL};
    unsigned int threat_type = 0;
    PyObject* position = NULL;
    float severity = 0.5f;
    const char* description = "";

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IO!fs", kwlist,
                                      &threat_type, &PyList_Type, &position, &severity, &description)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ThreatData_init: operation failed");
        return -1;
    }

    self->threat_type = threat_type;
    self->severity = severity;

    if (description && strlen(description) > 0) {
        self->description = strdup(description);
    }

    if (position != NULL && PyList_Size(position) >= 3) {
        for (int i = 0; i < 3; i++) {
            PyObject* item = PyList_GetItem(position, i);
            if (PyFloat_Check(item) || PyLong_Check(item)) {
                self->position[i] = (float)PyFloat_AsDouble(item);
            }
        }
    }

    return 0;
}

static PyMemberDef ThreatData_members[] = {
    {"threat_type", T_UINT, offsetof(ThreatDataObject, threat_type), 0, "Threat classification"},
    {"severity", T_FLOAT, offsetof(ThreatDataObject, severity), 0, "Threat severity (0-1)"},
    {"description", T_STRING, offsetof(ThreatDataObject, description), READONLY, "Description"},
    {NULL}
};

static PyTypeObject ThreatDataType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.ThreatData",
    .tp_doc = PyDoc_STR("Threat data - urgent warning for swarm broadcast"),
    .tp_basicsize = sizeof(ThreatDataObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = ThreatData_new,
    .tp_init = (initproc)ThreatData_init,
    .tp_dealloc = (destructor)ThreatData_dealloc,
    .tp_members = ThreatData_members,
};

/* ============================================================================
 * VoteProposal Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    uint32_t proposal_id;
    uint32_t action_type;
    PyObject* parameters;  /* List of floats */
} VoteProposalObject;

static void VoteProposal_dealloc(VoteProposalObject* self)
{
    Py_XDECREF(self->parameters);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* VoteProposal_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    VoteProposalObject* self = (VoteProposalObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->proposal_id = 0;
        self->action_type = 0;
        self->parameters = PyList_New(0);
        if (self->parameters == NULL) {
            Py_DECREF(self);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "VoteProposal_new: validation failed");
            return NULL;
        }
    }
    return (PyObject*)self;
}

static int VoteProposal_init(VoteProposalObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"action_type", "parameters", NULL};
    unsigned int action_type = 0;
    PyObject* parameters = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IO!", kwlist,
                                      &action_type, &PyList_Type, &parameters)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "VoteProposal_init: operation failed");
        return -1;
    }

    self->action_type = action_type;

    if (parameters != NULL) {
        Py_XDECREF(self->parameters);
        Py_INCREF(parameters);
        self->parameters = parameters;
    }

    return 0;
}

static PyMemberDef VoteProposal_members[] = {
    {"proposal_id", T_UINT, offsetof(VoteProposalObject, proposal_id), READONLY, "Proposal ID"},
    {"action_type", T_UINT, offsetof(VoteProposalObject, action_type), 0, "Proposed action type"},
    {"parameters", T_OBJECT_EX, offsetof(VoteProposalObject, parameters), 0, "Action parameters"},
    {NULL}
};

static PyTypeObject VoteProposalType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.VoteProposal",
    .tp_doc = PyDoc_STR("Vote proposal for swarm consensus"),
    .tp_basicsize = sizeof(VoteProposalObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = VoteProposal_new,
    .tp_init = (initproc)VoteProposal_init,
    .tp_dealloc = (destructor)VoteProposal_dealloc,
    .tp_members = VoteProposal_members,
};

/* ============================================================================
 * NeuromodState Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    float dopamine;
    float serotonin;
    float norepinephrine;
    float acetylcholine;
} NeuromodStateObject;

static PyObject* NeuromodState_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    NeuromodStateObject* self = (NeuromodStateObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->dopamine = 0.5f;
        self->serotonin = 0.5f;
        self->norepinephrine = 0.5f;
        self->acetylcholine = 0.5f;
    }
    return (PyObject*)self;
}

static int NeuromodState_init(NeuromodStateObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"dopamine", "serotonin", "norepinephrine", "acetylcholine", NULL};
    float dopamine = 0.5f;
    float serotonin = 0.5f;
    float norepinephrine = 0.5f;
    float acetylcholine = 0.5f;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ffff", kwlist,
                                      &dopamine, &serotonin, &norepinephrine, &acetylcholine)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NeuromodState_init: operation failed");
        return -1;
    }

    self->dopamine = dopamine;
    self->serotonin = serotonin;
    self->norepinephrine = norepinephrine;
    self->acetylcholine = acetylcholine;

    return 0;
}

static PyMemberDef NeuromodState_members[] = {
    {"dopamine", T_FLOAT, offsetof(NeuromodStateObject, dopamine), 0, "Dopamine level (0-1)"},
    {"serotonin", T_FLOAT, offsetof(NeuromodStateObject, serotonin), 0, "Serotonin level (0-1)"},
    {"norepinephrine", T_FLOAT, offsetof(NeuromodStateObject, norepinephrine), 0, "Norepinephrine level (0-1)"},
    {"acetylcholine", T_FLOAT, offsetof(NeuromodStateObject, acetylcholine), 0, "Acetylcholine level (0-1)"},
    {NULL}
};

static PyTypeObject NeuromodStateType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.NeuromodState",
    .tp_doc = PyDoc_STR("Neuromodulator state for swarm synchronization"),
    .tp_basicsize = sizeof(NeuromodStateObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = NeuromodState_new,
    .tp_init = (initproc)NeuromodState_init,
    .tp_members = NeuromodState_members,
};

/* ============================================================================
 * SwarmBrain Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    swarm_brain_t* swarm;
    uint16_t drone_id;
    char swarm_name[SWARM_MAX_NAME_LEN];
} SwarmBrainObject;

static void SwarmBrain_dealloc(SwarmBrainObject* self)
{
    if (self->swarm != NULL) {
        swarm_brain_leave(self->swarm);
        swarm_brain_destroy(self->swarm);
        self->swarm = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* SwarmBrain_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    SwarmBrainObject* self = (SwarmBrainObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->swarm = NULL;
        self->drone_id = 0;
        memset(self->swarm_name, 0, sizeof(self->swarm_name));
    }
    return (PyObject*)self;
}

static int SwarmBrain_init(SwarmBrainObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"drone_id", "swarm_name", "heartbeat_ms", "coherence_threshold", NULL};
    unsigned short drone_id = 1;
    const char* swarm_name = "default_swarm";
    unsigned int heartbeat_ms = SWARM_DEFAULT_HEARTBEAT_MS;
    float coherence_threshold = SWARM_DEFAULT_COHERENCE_THRESHOLD;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|HsIf", kwlist,
                                      &drone_id, &swarm_name, &heartbeat_ms, &coherence_threshold)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "SwarmBrain_init: operation failed");
        return -1;
    }

    self->drone_id = drone_id;
    strncpy(self->swarm_name, swarm_name, SWARM_MAX_NAME_LEN - 1);

    swarm_brain_config_t config = swarm_brain_default_config();
    config.drone_id = drone_id;
    strncpy(config.swarm_name, swarm_name, SWARM_MAX_NAME_LEN - 1);
    config.heartbeat_ms = heartbeat_ms;
    config.coherence_threshold = coherence_threshold;

    self->swarm = swarm_brain_create(&config);
    if (self->swarm == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create swarm brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "SwarmBrain_init: validation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief SwarmBrain.join() -> bool
 */
static PyObject* SwarmBrain_join(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = swarm_brain_join(self->swarm);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief SwarmBrain.leave() -> bool
 */
static PyObject* SwarmBrain_leave(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = swarm_brain_leave(self->swarm);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief SwarmBrain.process() -> bool
 */
static PyObject* SwarmBrain_process(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = swarm_brain_process(self->swarm);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief SwarmBrain.broadcast_perception(perception) -> bool
 */
static PyObject* SwarmBrain_broadcast_perception(SwarmBrainObject* self, PyObject* args)
{
    PyObject* perception_obj;

    if (!PyArg_ParseTuple(args, "O!", &PerceptionDataType, &perception_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "SwarmBrain_broadcast_perception: PyArg_ParseTuple is NULL");
        return NULL;
    }

    perception_data_t perception;
    memset(&perception, 0, sizeof(perception));

    if (!PerceptionData_ToC((PerceptionDataObject*)perception_obj, &perception)) {
        PyErr_SetString(PyExc_ValueError, "Invalid perception data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "SwarmBrain_broadcast_perception: PerceptionData_ToC is NULL");
        return NULL;
    }

    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = swarm_brain_broadcast_perception(self->swarm, &perception);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief SwarmBrain.broadcast_threat(threat) -> bool
 */
static PyObject* SwarmBrain_broadcast_threat(SwarmBrainObject* self, PyObject* args)
{
    PyObject* threat_obj;

    if (!PyArg_ParseTuple(args, "O!", &ThreatDataType, &threat_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "SwarmBrain_broadcast_threat: PyArg_ParseTuple is NULL");
        return NULL;
    }

    ThreatDataObject* threat_data = (ThreatDataObject*)threat_obj;

    threat_data_t threat;
    memset(&threat, 0, sizeof(threat));
    threat.threat_type = threat_data->threat_type;
    threat.position[0] = threat_data->position[0];
    threat.position[1] = threat_data->position[1];
    threat.position[2] = threat_data->position[2];
    threat.severity = threat_data->severity;
    if (threat_data->description) {
        strncpy(threat.description, threat_data->description, sizeof(threat.description) - 1);
    }

    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = swarm_brain_broadcast_threat(self->swarm, &threat);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief SwarmBrain.propose_action(proposal) -> bool
 */
static PyObject* SwarmBrain_propose_action(SwarmBrainObject* self, PyObject* args)
{
    PyObject* proposal_obj;

    if (!PyArg_ParseTuple(args, "O!", &VoteProposalType, &proposal_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "SwarmBrain_propose_action: PyArg_ParseTuple is NULL");
        return NULL;
    }

    VoteProposalObject* vp = (VoteProposalObject*)proposal_obj;

    vote_proposal_t proposal;
    memset(&proposal, 0, sizeof(proposal));
    proposal.action_type = vp->action_type;
    proposal.proposer_id = self->drone_id;

    /* Copy parameters */
    Py_ssize_t count = PyList_Size(vp->parameters);
    if (count > 8) count = 8;
    proposal.parameter_count = (uint32_t)count;

    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject* item = PyList_GetItem(vp->parameters, i);
        if (PyFloat_Check(item) || PyLong_Check(item)) {
            proposal.parameters[i] = (float)PyFloat_AsDouble(item);
        }
    }

    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = swarm_brain_propose_action(self->swarm, &proposal);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief SwarmBrain.sync_neuromodulators(state) -> bool
 */
static PyObject* SwarmBrain_sync_neuromodulators(SwarmBrainObject* self, PyObject* args)
{
    PyObject* state_obj;

    if (!PyArg_ParseTuple(args, "O!", &NeuromodStateType, &state_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "SwarmBrain_sync_neuromodulators: PyArg_ParseTuple is NULL");
        return NULL;
    }

    NeuromodStateObject* ns = (NeuromodStateObject*)state_obj;

    neuromod_state_t state;
    state.dopamine = ns->dopamine;
    state.serotonin = ns->serotonin;
    state.norepinephrine = ns->norepinephrine;
    state.acetylcholine = ns->acetylcholine;

    bool result;

    Py_BEGIN_ALLOW_THREADS
    result = swarm_brain_sync_neuromodulators(self->swarm, &state);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(result);
}

/**
 * @brief SwarmBrain.get_emergence_tier() -> int
 */
static PyObject* SwarmBrain_get_emergence_tier(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    swarm_emergence_tier_t tier = swarm_brain_get_emergence_tier(self->swarm);
    return PyLong_FromLong((long)tier);
}

/**
 * @brief SwarmBrain.get_peer_count() -> int
 */
static PyObject* SwarmBrain_get_peer_count(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    uint32_t count = 0;
    const swarm_peer_info_t* peers = swarm_brain_get_peers(self->swarm, &count);
    (void)peers;  /* Just need count */
    return PyLong_FromUnsignedLong(count);
}

/**
 * @brief SwarmBrain.get_stats() -> SwarmStats
 */
static PyObject* SwarmBrain_get_stats(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    swarm_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    bool success = swarm_brain_get_stats(self->swarm, &stats);
    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");

        return NULL;
    }

    return SwarmStats_FromC(&stats);
}

/**
 * @brief SwarmBrain.is_operational() -> bool
 */
static PyObject* SwarmBrain_is_operational(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    bool operational = swarm_brain_is_operational(self->swarm);
    return PyBool_FromLong(operational);
}

/**
 * @brief SwarmBrain.reset_stats() -> bool
 */
static PyObject* SwarmBrain_reset_stats(SwarmBrainObject* self, PyObject* Py_UNUSED(args))
{
    bool result = swarm_brain_reset_stats(self->swarm);
    return PyBool_FromLong(result);
}

static PyObject* SwarmBrain_repr(SwarmBrainObject* self)
{
    swarm_emergence_tier_t tier = swarm_brain_get_emergence_tier(self->swarm);
    uint32_t peer_count = 0;
    swarm_brain_get_peers(self->swarm, &peer_count);

    return PyUnicode_FromFormat("SwarmBrain(id=%u, name='%s', tier=%d, peers=%u)",
                                self->drone_id, self->swarm_name, (int)tier, peer_count);
}

static PyMethodDef SwarmBrain_methods[] = {
    {"join", (PyCFunction)SwarmBrain_join, METH_NOARGS,
     "Join swarm network"},
    {"leave", (PyCFunction)SwarmBrain_leave, METH_NOARGS,
     "Leave swarm network gracefully"},
    {"process", (PyCFunction)SwarmBrain_process, METH_NOARGS,
     "Process swarm brain (call frequently in main loop)"},
    {"broadcast_perception", (PyCFunction)SwarmBrain_broadcast_perception, METH_VARARGS,
     "Broadcast perception data to swarm"},
    {"broadcast_threat", (PyCFunction)SwarmBrain_broadcast_threat, METH_VARARGS,
     "Broadcast urgent threat warning"},
    {"propose_action", (PyCFunction)SwarmBrain_propose_action, METH_VARARGS,
     "Propose action for consensus vote"},
    {"sync_neuromodulators", (PyCFunction)SwarmBrain_sync_neuromodulators, METH_VARARGS,
     "Synchronize neuromodulator state with swarm"},
    {"get_emergence_tier", (PyCFunction)SwarmBrain_get_emergence_tier, METH_NOARGS,
     "Get current emergence tier"},
    {"get_peer_count", (PyCFunction)SwarmBrain_get_peer_count, METH_NOARGS,
     "Get number of connected peers"},
    {"get_stats", (PyCFunction)SwarmBrain_get_stats, METH_NOARGS,
     "Get swarm statistics"},
    {"is_operational", (PyCFunction)SwarmBrain_is_operational, METH_NOARGS,
     "Check if swarm brain is operational"},
    {"reset_stats", (PyCFunction)SwarmBrain_reset_stats, METH_NOARGS,
     "Reset swarm statistics"},
    {NULL}
};

static PyTypeObject SwarmBrainType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.SwarmBrain",
    .tp_doc = PyDoc_STR(
        "Swarm brain coordinator for distributed cognition.\n\n"
        "Integrates local brain, collective workspace, consensus,\n"
        "and neuromodulator synchronization across drone/robot swarms.\n\n"
        "Args:\n"
        "    drone_id (int): Unique drone identifier\n"
        "    swarm_name (str): Swarm network name\n"
        "    heartbeat_ms (int): Heartbeat interval (default: 100)\n"
        "    coherence_threshold (float): Coherence threshold (default: 0.5)\n\n"
        "Emergence Tiers:\n"
        "    TIER_0: Disconnected (solo)\n"
        "    TIER_1: Paired (2-3 drones)\n"
        "    TIER_2: Cluster (4-7 drones)\n"
        "    TIER_3: Swarm (8+ drones)\n"
        "    TIER_4: Superorganism (16+ with high coherence)"
    ),
    .tp_basicsize = sizeof(SwarmBrainObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = SwarmBrain_new,
    .tp_init = (initproc)SwarmBrain_init,
    .tp_dealloc = (destructor)SwarmBrain_dealloc,
    .tp_repr = (reprfunc)SwarmBrain_repr,
    .tp_methods = SwarmBrain_methods,
};

/* ============================================================================
 * Module initialization
 * ============================================================================ */

int init_swarm_module(PyObject* module)
{
    LOG_MODULE_INFO("bindings.python.swarm", "Initializing swarm module");

    /* Ready types */
    if (PyType_Ready(&SwarmStatsType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&PerceptionDataType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&ThreatDataType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&VoteProposalType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&NeuromodStateType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&SwarmBrainType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }

    /* Add types */
    Py_INCREF(&SwarmStatsType);
    if (PyModule_AddObject(module, "SwarmStats", (PyObject*)&SwarmStatsType) < 0) {
        Py_DECREF(&SwarmStatsType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }

    Py_INCREF(&PerceptionDataType);
    if (PyModule_AddObject(module, "PerceptionData", (PyObject*)&PerceptionDataType) < 0) {
        Py_DECREF(&PerceptionDataType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }

    Py_INCREF(&ThreatDataType);
    if (PyModule_AddObject(module, "ThreatData", (PyObject*)&ThreatDataType) < 0) {
        Py_DECREF(&ThreatDataType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }

    Py_INCREF(&VoteProposalType);
    if (PyModule_AddObject(module, "VoteProposal", (PyObject*)&VoteProposalType) < 0) {
        Py_DECREF(&VoteProposalType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }

    Py_INCREF(&NeuromodStateType);
    if (PyModule_AddObject(module, "NeuromodState", (PyObject*)&NeuromodStateType) < 0) {
        Py_DECREF(&NeuromodStateType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }

    Py_INCREF(&SwarmBrainType);
    if (PyModule_AddObject(module, "SwarmBrain", (PyObject*)&SwarmBrainType) < 0) {
        Py_DECREF(&SwarmBrainType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_swarm_module: validation failed");
        return -1;
    }

    /* Add emergence tier constants */
    PyModule_AddIntConstant(module, "SWARM_TIER_INDIVIDUAL", SWARM_TIER_INDIVIDUAL);
    PyModule_AddIntConstant(module, "SWARM_TIER_PAIR", SWARM_TIER_PAIR);
    PyModule_AddIntConstant(module, "SWARM_TIER_SQUAD", SWARM_TIER_SQUAD);
    PyModule_AddIntConstant(module, "SWARM_TIER_PLATOON", SWARM_TIER_PLATOON);
    PyModule_AddIntConstant(module, "SWARM_TIER_COMPANY", SWARM_TIER_COMPANY);
    PyModule_AddIntConstant(module, "SWARM_TIER_BATTALION", SWARM_TIER_BATTALION);

    /* Add message type constants */
    PyModule_AddIntConstant(module, "SWARM_MSG_HEARTBEAT", SWARM_MSG_HEARTBEAT);
    PyModule_AddIntConstant(module, "SWARM_MSG_PERCEPTION", SWARM_MSG_PERCEPTION);
    PyModule_AddIntConstant(module, "SWARM_MSG_THREAT", SWARM_MSG_THREAT);
    PyModule_AddIntConstant(module, "SWARM_MSG_VOTE_PROPOSE", SWARM_MSG_VOTE_PROPOSE);
    PyModule_AddIntConstant(module, "SWARM_MSG_VOTE_CAST", SWARM_MSG_VOTE_CAST);
    PyModule_AddIntConstant(module, "SWARM_MSG_NEUROMOD_SYNC", SWARM_MSG_NEUROMOD_SYNC);
    PyModule_AddIntConstant(module, "SWARM_MSG_WORKSPACE_UPDATE", SWARM_MSG_WORKSPACE_UPDATE);
    PyModule_AddIntConstant(module, "SWARM_MSG_GOODBYE", SWARM_MSG_GOODBYE);

    /* Add vote decision constants */
    PyModule_AddIntConstant(module, "VOTE_ABSTAIN", VOTE_ABSTAIN);
    PyModule_AddIntConstant(module, "VOTE_APPROVE", VOTE_APPROVE);
    PyModule_AddIntConstant(module, "VOTE_REJECT", VOTE_REJECT);

    LOG_MODULE_INFO("bindings.python.swarm", "Swarm module initialized successfully");
    return 0;
}
