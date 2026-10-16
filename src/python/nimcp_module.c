#include "common/nimcp_module.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(module)

// Forward declarations for sub-modules
extern int init_metrics_module(PyObject* module);
extern int init_training_module(PyObject* module);
extern int init_callbacks_module(PyObject* module);
extern int init_ethics_module(PyObject* module);
extern int init_knowledge_module(PyObject* module);
extern int init_bio_async_module(PyObject* module);
extern int init_brain_immune_module(PyObject* module);
extern int init_swarm_module(PyObject* module);
extern int init_health_agent_module(PyObject* module);

// Define exception types
// These are initialized in PyInit_nimcp()
PyObject* NIMCPError = NULL;
PyObject* NetworkError = NULL;
PyObject* ProtocolError = NULL;
PyObject* NodeError = NULL;

// Module-level function: nimcp.version() -> str
static PyObject* nimcp_py_version(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    const char* ver = nimcp_version();
    return PyUnicode_FromString(ver ? ver : "unknown");
}

// Module-level function: nimcp.version_int() -> int
static PyObject* nimcp_py_version_int(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    return PyLong_FromLong(nimcp_version_int());
}

// Module-level function: nimcp.init() -> None
static PyObject* nimcp_py_init(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    nimcp_status_t status = nimcp_init();
    if (status != NIMCP_OK) {
        PyErr_Format(PyExc_RuntimeError, "nimcp_init() failed with status %d", (int)status);
        return NULL;
    }
    Py_RETURN_NONE;
}

// Module-level function: nimcp.shutdown() -> None
static PyObject* nimcp_py_shutdown(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    Py_BEGIN_ALLOW_THREADS
    nimcp_shutdown();
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

// Module-level function: nimcp.get_error() -> str
static PyObject* nimcp_py_get_error(PyObject* Py_UNUSED(self), PyObject* Py_UNUSED(args))
{
    const char* err = nimcp_get_error();
    return PyUnicode_FromString(err ? err : "");
}

// Module method definitions
static PyMethodDef nimcp_methods[] = {
    {"version", (PyCFunction)nimcp_py_version, METH_NOARGS,
     "Get NIMCP version string\n\nReturns:\n    str: Version (e.g., '2.6.3')"},
    {"version_int", (PyCFunction)nimcp_py_version_int, METH_NOARGS,
     "Get NIMCP version as integer\n\nReturns:\n    int: Version (e.g., 20603)"},
    {"init", (PyCFunction)nimcp_py_init, METH_NOARGS,
     "Initialize NIMCP library (call once at startup)\n\nRaises:\n    RuntimeError: If initialization fails"},
    {"shutdown", (PyCFunction)nimcp_py_shutdown, METH_NOARGS,
     "Shutdown NIMCP library (call once at cleanup)"},
    {"get_error", (PyCFunction)nimcp_py_get_error, METH_NOARGS,
     "Get error message for last error\n\nReturns:\n    str: Error message"},
    {NULL, NULL, 0, NULL}
};

// Module definition
static struct PyModuleDef nimcp_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "nimcp",
    .m_doc = "Neural Interface Message Communication Protocol",
    .m_size = -1,
    .m_methods = nimcp_methods,
    .m_slots = NULL
};

// Module initialization
PyMODINIT_FUNC PyInit_nimcp(void)
{
    PyObject* m;

    LOG_MODULE_INFO("bindings.python.module", "Initializing full Python module for NIMCP");

    // Initialize types
    if (PyType_Ready(&BrainType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }
    if (PyType_Ready(&BrainConfigType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }
    if (PyType_Ready(&NeuralNetworkType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }
    if (PyType_Ready(&P2PNodeType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }
    if (PyType_Ready(&NetworkConfigType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }
    if (PyType_Ready(&NodeConfigType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }
    if (PyType_Ready(&GlialIntegrationType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }

    m = PyModule_Create(&nimcp_module);
    if (m == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "PyInit_nimcp: parameter is NULL");
        return NULL;
    }

    // Create exception hierarchy with proper error handling
    NIMCPError = PyErr_NewException("nimcp.NIMCPError", NULL, NULL);
    if (NIMCPError == NULL) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PyInit_nimcp: validation failed");
        return NULL;
    }

    NetworkError = PyErr_NewException("nimcp.NetworkError", NIMCPError, NULL);
    if (NetworkError == NULL) {
        Py_DECREF(NIMCPError);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PyInit_nimcp: validation failed");
        return NULL;
    }

    ProtocolError = PyErr_NewException("nimcp.ProtocolError", NIMCPError, NULL);
    if (ProtocolError == NULL) {
        Py_DECREF(NetworkError);
        Py_DECREF(NIMCPError);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PyInit_nimcp: validation failed");
        return NULL;
    }

    NodeError = PyErr_NewException("nimcp.NodeError", NIMCPError, NULL);
    if (NodeError == NULL) {
        Py_DECREF(ProtocolError);
        Py_DECREF(NetworkError);
        Py_DECREF(NIMCPError);
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Add types to module with proper error handling
    // Note: PyModule_AddObject steals a reference on success, so we must Py_INCREF first
    Py_INCREF(&BrainType);
    if (PyModule_AddObject(m, "Brain", (PyObject*) &BrainType) < 0) {
        Py_DECREF(&BrainType);
        goto error_cleanup;
    }

    Py_INCREF(&BrainConfigType);
    if (PyModule_AddObject(m, "BrainConfig", (PyObject*) &BrainConfigType) < 0) {
        Py_DECREF(&BrainConfigType);
        goto error_cleanup;
    }

    Py_INCREF(&NeuralNetworkType);
    if (PyModule_AddObject(m, "NeuralNetwork", (PyObject*) &NeuralNetworkType) < 0) {
        Py_DECREF(&NeuralNetworkType);
        goto error_cleanup;
    }

    Py_INCREF(&P2PNodeType);
    if (PyModule_AddObject(m, "P2PNode", (PyObject*) &P2PNodeType) < 0) {
        Py_DECREF(&P2PNodeType);
        goto error_cleanup;
    }

    Py_INCREF(&NetworkConfigType);
    if (PyModule_AddObject(m, "NetworkConfig", (PyObject*) &NetworkConfigType) < 0) {
        Py_DECREF(&NetworkConfigType);
        goto error_cleanup;
    }

    Py_INCREF(&NodeConfigType);
    if (PyModule_AddObject(m, "NodeConfig", (PyObject*) &NodeConfigType) < 0) {
        Py_DECREF(&NodeConfigType);
        goto error_cleanup;
    }

    Py_INCREF(&GlialIntegrationType);
    if (PyModule_AddObject(m, "GlialIntegration", (PyObject*) &GlialIntegrationType) < 0) {
        Py_DECREF(&GlialIntegrationType);
        goto error_cleanup;
    }

    // Add exceptions to module (PyModule_AddObject steals reference on success)
    Py_INCREF(NIMCPError);
    if (PyModule_AddObject(m, "NIMCPError", NIMCPError) < 0) {
        Py_DECREF(NIMCPError);
        goto error_cleanup;
    }

    Py_INCREF(NetworkError);
    if (PyModule_AddObject(m, "NetworkError", NetworkError) < 0) {
        Py_DECREF(NetworkError);
        goto error_cleanup;
    }

    Py_INCREF(ProtocolError);
    if (PyModule_AddObject(m, "ProtocolError", ProtocolError) < 0) {
        Py_DECREF(ProtocolError);
        goto error_cleanup;
    }

    Py_INCREF(NodeError);
    if (PyModule_AddObject(m, "NodeError", NodeError) < 0) {
        Py_DECREF(NodeError);
        goto error_cleanup;
    }

    // Add brain size constants
    PyModule_AddIntConstant(m, "BRAIN_TINY", NIMCP_BRAIN_TINY);
    PyModule_AddIntConstant(m, "BRAIN_SMALL", NIMCP_BRAIN_SMALL);
    PyModule_AddIntConstant(m, "BRAIN_MEDIUM", NIMCP_BRAIN_MEDIUM);
    PyModule_AddIntConstant(m, "BRAIN_LARGE", NIMCP_BRAIN_LARGE);

    // Add task type constants
    PyModule_AddIntConstant(m, "TASK_CLASSIFICATION", NIMCP_TASK_CLASSIFICATION);
    PyModule_AddIntConstant(m, "TASK_REGRESSION", NIMCP_TASK_REGRESSION);
    PyModule_AddIntConstant(m, "TASK_PATTERN_MATCHING", NIMCP_TASK_PATTERN_MATCHING);
    PyModule_AddIntConstant(m, "TASK_SEQUENCE", NIMCP_TASK_SEQUENCE);
    PyModule_AddIntConstant(m, "TASK_ASSOCIATION", NIMCP_TASK_ASSOCIATION);

    // Add status code constants
    PyModule_AddIntConstant(m, "OK", NIMCP_OK);
    PyModule_AddIntConstant(m, "ERROR", NIMCP_ERROR);
    PyModule_AddIntConstant(m, "ERROR_NULL_ARG", NIMCP_ERROR_NULL_ARG);
    PyModule_AddIntConstant(m, "ERROR_INVALID", NIMCP_ERROR_INVALID);
    PyModule_AddIntConstant(m, "ERROR_MEMORY", NIMCP_ERROR_MEMORY);
    PyModule_AddIntConstant(m, "ERROR_IO", NIMCP_ERROR_IO);

    // Add network type constants
    PyModule_AddIntConstant(m, "NETWORK_ADAPTIVE", NIMCP_NETWORK_ADAPTIVE);
    PyModule_AddIntConstant(m, "NETWORK_SNN", NIMCP_NETWORK_SNN);
    PyModule_AddIntConstant(m, "NETWORK_LNN", NIMCP_NETWORK_LNN);
    PyModule_AddIntConstant(m, "NETWORK_CNN", NIMCP_NETWORK_CNN);
    PyModule_AddIntConstant(m, "NETWORK_HYBRID", NIMCP_NETWORK_HYBRID);

    // Initialize metrics module
    if (init_metrics_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize topology module
    if (init_topology_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize pink noise module
    if (init_pink_noise_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize community detection module
    if (init_community_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize training module (TrainingConfig, TrainingResult, constants)
    if (init_training_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize callbacks module (CallbackConfig, CallbackMetrics, constants)
    if (init_callbacks_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize ethics module (Ethics class)
    if (init_ethics_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize knowledge module (KnowledgeSystem, KnowledgeItem, DomainKnowledge)
    if (init_knowledge_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize bio-async module (BioPromise, BioFuture, PhaseSync, PredictiveModel, GlialWave)
    if (init_bio_async_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize brain immune module (BrainImmuneSystem, BrainAntigen, BrainImmuneStats)
    if (init_brain_immune_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize swarm module (SwarmBrain, SwarmBrainConfig)
    if (init_swarm_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    // Initialize health agent module (HealthAgent, HealthAgentStats, USE functions)
    if (init_health_agent_module(m) < 0) {
        Py_DECREF(m);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "PyInit_nimcp: validation failed");
        return NULL;
    }

    return m;

error_cleanup:
    // P1-14 fix: Properly rollback all partial type/exception additions.
    // PyModule_AddObject steals a reference on success, so successfully-added
    // objects are owned by the module and will be cleaned up when 'm' is decref'd.
    // We only need to decref the exception objects that were created but may not
    // have been added to the module yet.
    Py_XDECREF(NodeError);
    NodeError = NULL;
    Py_XDECREF(ProtocolError);
    ProtocolError = NULL;
    Py_XDECREF(NetworkError);
    NetworkError = NULL;
    Py_XDECREF(NIMCPError);
    NIMCPError = NULL;
    Py_DECREF(m);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "PyInit_nimcp: module initialization failed during type registration");
    return NULL;
}
