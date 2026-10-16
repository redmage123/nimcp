#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_community_py.c - Python Bindings for Community Detection
//=============================================================================
/**
 * @file nimcp_community_py.c
 * @brief Python interface for community detection and network analysis
 *
 * WHAT: Exposes Louvain algorithm and hub detection to Python
 * WHY: Enable Python training scripts to track modular organization
 * HOW: Python Type objects wrapping C community detection API
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <Python.h>
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "core/topology/nimcp_community_detection.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(community_py)

//=============================================================================
// CommunityStructure Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    community_structure_t* structure;
} CommunityStructureObject;

static void CommunityStructure_dealloc(CommunityStructureObject* self) {
    if (self->structure) {
        topology_community_structure_free(self->structure);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* CommunityStructure_get_num_neurons(CommunityStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(self->structure->num_neurons);
}

static PyObject* CommunityStructure_get_num_communities(CommunityStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(self->structure->num_communities);
}

static PyObject* CommunityStructure_get_modularity(CommunityStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }
    return PyFloat_FromDouble(self->structure->modularity);
}

static PyObject* CommunityStructure_get_community_ids(CommunityStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }

    PyObject* list = PyList_New(self->structure->num_neurons);
    if (!list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;
    }

    for (uint32_t i = 0; i < self->structure->num_neurons; i++) {
        PyObject* val = PyLong_FromUnsignedLong(self->structure->community_ids[i]);
        if (!val) {
            Py_DECREF(list);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "CommunityStructure_get_community_ids: val is NULL");
            return NULL;
        }
        PyList_SET_ITEM(list, i, val);
    }

    return list;
}

static PyObject* CommunityStructure_get_community_sizes(CommunityStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }

    PyObject* list = PyList_New(self->structure->num_communities);
    if (!list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;
    }

    for (uint32_t i = 0; i < self->structure->num_communities; i++) {
        PyObject* val = PyLong_FromUnsignedLong(self->structure->community_sizes[i]);
        if (!val) {
            Py_DECREF(list);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "CommunityStructure_get_community_sizes: val is NULL");
            return NULL;
        }
        PyList_SET_ITEM(list, i, val);
    }

    return list;
}

static PyGetSetDef CommunityStructure_getsetters[] = {
    {"num_neurons", (getter)CommunityStructure_get_num_neurons, NULL, "Number of neurons", NULL},
    {"num_communities", (getter)CommunityStructure_get_num_communities, NULL, "Number of communities", NULL},
    {"modularity", (getter)CommunityStructure_get_modularity, NULL, "Newman's modularity Q", NULL},
    {"community_ids", (getter)CommunityStructure_get_community_ids, NULL, "Community ID for each neuron", NULL},
    {"community_sizes", (getter)CommunityStructure_get_community_sizes, NULL, "Size of each community", NULL},
    {NULL}
};

static PyTypeObject CommunityStructureType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.CommunityStructure",
    .tp_doc = "Community detection results",
    .tp_basicsize = sizeof(CommunityStructureObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)CommunityStructure_dealloc,
    .tp_getset = CommunityStructure_getsetters,
};

//=============================================================================
// HubStructure Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    hub_structure_t* structure;
} HubStructureObject;

static void HubStructure_dealloc(HubStructureObject* self) {
    if (self->structure) {
        hub_structure_free(self->structure);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* HubStructure_get_num_hubs(HubStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(self->structure->num_hubs);
}

static PyObject* HubStructure_get_hub_indices(HubStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }

    PyObject* list = PyList_New(self->structure->num_hubs);
    if (!list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;
    }

    for (uint32_t i = 0; i < self->structure->num_hubs; i++) {
        PyObject* val = PyLong_FromUnsignedLong(self->structure->hub_indices[i]);
        if (!val) {
            Py_DECREF(list);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HubStructure_get_hub_indices: val is NULL");
            return NULL;
        }
        PyList_SET_ITEM(list, i, val);
    }

    return list;
}

static PyObject* HubStructure_get_degree_centrality(HubStructureObject* self, void* closure) {
    if (!self->structure) {
        Py_RETURN_NONE;
    }

    PyObject* list = PyList_New(self->structure->num_hubs);
    if (!list) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "list is NULL");

        return NULL;
    }

    for (uint32_t i = 0; i < self->structure->num_hubs; i++) {
        PyObject* val = PyFloat_FromDouble(self->structure->degree_centrality[i]);
        if (!val) {
            Py_DECREF(list);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "HubStructure_get_degree_centrality: val is NULL");
            return NULL;
        }
        PyList_SET_ITEM(list, i, val);
    }

    return list;
}

static PyGetSetDef HubStructure_getsetters[] = {
    {"num_hubs", (getter)HubStructure_get_num_hubs, NULL, "Number of hub neurons", NULL},
    {"hub_indices", (getter)HubStructure_get_hub_indices, NULL, "Indices of hub neurons", NULL},
    {"degree_centrality", (getter)HubStructure_get_degree_centrality, NULL, "Degree centrality scores", NULL},
    {NULL}
};

static PyTypeObject HubStructureType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.HubStructure",
    .tp_doc = "Hub detection results",
    .tp_basicsize = sizeof(HubStructureObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)HubStructure_dealloc,
    .tp_getset = HubStructure_getsetters,
};

//=============================================================================
// TopologyValidation Type
//=============================================================================

typedef struct {
    PyObject_HEAD
    topology_validation_t validation;
} TopologyValidationObject;

static void TopologyValidation_dealloc(TopologyValidationObject* self) {
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* TopologyValidation_get_is_valid(TopologyValidationObject* self, void* closure) {
    return PyBool_FromLong(self->validation.is_valid);
}

static PyObject* TopologyValidation_get_modularity(TopologyValidationObject* self, void* closure) {
    return PyFloat_FromDouble(self->validation.modularity);
}

static PyObject* TopologyValidation_get_clustering_coefficient(TopologyValidationObject* self, void* closure) {
    return PyFloat_FromDouble(self->validation.clustering_coefficient);
}

static PyObject* TopologyValidation_get_num_communities(TopologyValidationObject* self, void* closure) {
    return PyLong_FromUnsignedLong(self->validation.num_communities);
}

static PyObject* TopologyValidation_get_num_hubs(TopologyValidationObject* self, void* closure) {
    return PyLong_FromUnsignedLong(self->validation.num_hubs);
}

static PyObject* TopologyValidation_get_error_message(TopologyValidationObject* self, void* closure) {
    return PyUnicode_FromString(self->validation.error_message);
}

static PyGetSetDef TopologyValidation_getsetters[] = {
    {"is_valid", (getter)TopologyValidation_get_is_valid, NULL, "Validation passed", NULL},
    {"modularity", (getter)TopologyValidation_get_modularity, NULL, "Modularity Q", NULL},
    {"clustering_coefficient", (getter)TopologyValidation_get_clustering_coefficient, NULL, "Clustering coefficient", NULL},
    {"num_communities", (getter)TopologyValidation_get_num_communities, NULL, "Number of communities", NULL},
    {"num_hubs", (getter)TopologyValidation_get_num_hubs, NULL, "Number of hubs", NULL},
    {"error_message", (getter)TopologyValidation_get_error_message, NULL, "Error message if invalid", NULL},
    {NULL}
};

static PyTypeObject TopologyValidationType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.TopologyValidation",
    .tp_doc = "Topology validation results",
    .tp_basicsize = sizeof(TopologyValidationObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)TopologyValidation_dealloc,
    .tp_getset = TopologyValidation_getsetters,
};

//=============================================================================
// Module-Level Functions
//=============================================================================

// brain_detect_communities(brain) -> CommunityStructure
static PyObject* py_brain_detect_communities(PyObject* self, PyObject* args) {
    PyObject* brain_obj;

    if (!PyArg_ParseTuple(args, "O", &brain_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_communities: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Extract brain's neural network
    // Assuming Brain object has a 'network' attribute accessible as NeuralNetwork
    PyObject* network_obj = PyObject_GetAttrString(brain_obj, "network");
    if (!network_obj) {
        PyErr_SetString(PyExc_TypeError, "Brain object has no 'network' attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_communities: network_obj is NULL");
        return NULL;
    }

    if (!PyObject_TypeCheck(network_obj, &NeuralNetworkType)) {
        Py_DECREF(network_obj);
        PyErr_SetString(PyExc_TypeError, "Brain.network must be NeuralNetwork");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_communities: PyObject_TypeCheck is NULL");
        return NULL;
    }

    NeuralNetworkObject* network_py = (NeuralNetworkObject*)network_obj;

    // Release GIL during potentially long community detection
    community_structure_t* structure;
    Py_BEGIN_ALLOW_THREADS
    structure = community_detect(network_py->network, NULL);
    Py_END_ALLOW_THREADS

    Py_DECREF(network_obj);

    if (!structure) {
        const char* error = community_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Community detection failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_communities: structure is NULL");
        return NULL;
    }

    // Wrap in Python object
    CommunityStructureObject* result = PyObject_New(CommunityStructureObject, &CommunityStructureType);
    if (!result) {
        topology_community_structure_free(structure);
        return PyErr_NoMemory();
    }

    result->structure = structure;
    return (PyObject*)result;
}

// brain_get_modularity(brain) -> float
static PyObject* py_brain_get_modularity(PyObject* self, PyObject* args) {
    PyObject* brain_obj;

    if (!PyArg_ParseTuple(args, "O", &brain_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_get_modularity: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Detect communities first
    PyObject* communities_obj = py_brain_detect_communities(self, args);
    if (!communities_obj) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "communities_obj is NULL");

        return NULL;
    }

    CommunityStructureObject* communities = (CommunityStructureObject*)communities_obj;
    float modularity = communities->structure->modularity;
    Py_DECREF(communities_obj);

    return PyFloat_FromDouble(modularity);
}

// brain_get_num_communities(brain) -> int
static PyObject* py_brain_get_num_communities(PyObject* self, PyObject* args) {
    PyObject* brain_obj;

    if (!PyArg_ParseTuple(args, "O", &brain_obj)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_get_num_communities: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Detect communities first
    PyObject* communities_obj = py_brain_detect_communities(self, args);
    if (!communities_obj) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "communities_obj is NULL");

        return NULL;
    }

    CommunityStructureObject* communities = (CommunityStructureObject*)communities_obj;
    uint32_t num = communities->structure->num_communities;
    Py_DECREF(communities_obj);

    return PyLong_FromUnsignedLong(num);
}

// brain_get_neuron_community(brain, neuron_id) -> int
static PyObject* py_brain_get_neuron_community(PyObject* self, PyObject* args) {
    PyObject* brain_obj;
    uint32_t neuron_id;

    if (!PyArg_ParseTuple(args, "OI", &brain_obj, &neuron_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_get_neuron_community: PyArg_ParseTuple is NULL");
        return NULL;
    }

    // Detect communities first
    PyObject* args_tuple = Py_BuildValue("(O)", brain_obj);
    PyObject* communities_obj = py_brain_detect_communities(self, args_tuple);
    Py_DECREF(args_tuple);

    if (!communities_obj) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "communities_obj is NULL");


        return NULL;
    }

    CommunityStructureObject* communities = (CommunityStructureObject*)communities_obj;
    uint32_t comm_id = community_get_neuron_community(communities->structure, neuron_id);
    Py_DECREF(communities_obj);

    if (comm_id == UINT32_MAX) {
        PyErr_SetString(PyExc_ValueError, "Invalid neuron ID");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_get_neuron_community: validation failed");
        return NULL;
    }

    return PyLong_FromUnsignedLong(comm_id);
}

// brain_detect_hubs(brain, threshold) -> HubStructure
static PyObject* py_brain_detect_hubs(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* brain_obj;
    float threshold = 0.8F;

    static char* kwlist[] = {"brain", "threshold", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|f", kwlist, &brain_obj, &threshold)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_hubs: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    // Extract brain's neural network
    PyObject* network_obj = PyObject_GetAttrString(brain_obj, "network");
    if (!network_obj) {
        PyErr_SetString(PyExc_TypeError, "Brain object has no 'network' attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_hubs: network_obj is NULL");
        return NULL;
    }

    if (!PyObject_TypeCheck(network_obj, &NeuralNetworkType)) {
        Py_DECREF(network_obj);
        PyErr_SetString(PyExc_TypeError, "Brain.network must be NeuralNetwork");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_hubs: PyObject_TypeCheck is NULL");
        return NULL;
    }

    NeuralNetworkObject* network_py = (NeuralNetworkObject*)network_obj;

    // Release GIL during potentially long hub detection
    hub_structure_t* hubs;
    Py_BEGIN_ALLOW_THREADS
    hubs = community_detect_hubs(network_py->network, threshold);
    Py_END_ALLOW_THREADS

    Py_DECREF(network_obj);

    if (!hubs) {
        const char* error = community_get_last_error();
        PyErr_SetString(PyExc_RuntimeError, error ? error : "Hub detection failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_detect_hubs: hubs is NULL");
        return NULL;
    }

    // Wrap in Python object
    HubStructureObject* result = PyObject_New(HubStructureObject, &HubStructureType);
    if (!result) {
        hub_structure_free(hubs);
        return PyErr_NoMemory();
    }

    result->structure = hubs;
    return (PyObject*)result;
}

// brain_validate_topology(brain, min_modularity) -> TopologyValidation
static PyObject* py_brain_validate_topology(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* brain_obj;
    float min_modularity = 0.25F;

    static char* kwlist[] = {"brain", "min_modularity", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|f", kwlist, &brain_obj, &min_modularity)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_validate_topology: PyArg_ParseTupleAndKeywords is NULL");
        return NULL;
    }

    // Extract brain's neural network
    PyObject* network_obj = PyObject_GetAttrString(brain_obj, "network");
    if (!network_obj) {
        PyErr_SetString(PyExc_TypeError, "Brain object has no 'network' attribute");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_validate_topology: network_obj is NULL");
        return NULL;
    }

    if (!PyObject_TypeCheck(network_obj, &NeuralNetworkType)) {
        Py_DECREF(network_obj);
        PyErr_SetString(PyExc_TypeError, "Brain.network must be NeuralNetwork");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "py_brain_validate_topology: PyObject_TypeCheck is NULL");
        return NULL;
    }

    NeuralNetworkObject* network_py = (NeuralNetworkObject*)network_obj;

    // Release GIL during potentially long topology validation
    topology_validation_t validation;
    Py_BEGIN_ALLOW_THREADS
    validation = community_validate_topology(network_py->network, min_modularity);
    Py_END_ALLOW_THREADS

    Py_DECREF(network_obj);

    // Wrap in Python object
    TopologyValidationObject* result = PyObject_New(TopologyValidationObject, &TopologyValidationType);
    if (!result) {
        return PyErr_NoMemory();
    }

    result->validation = validation;
    return (PyObject*)result;
}

//=============================================================================
// Module Method Table
//=============================================================================

static PyMethodDef community_methods[] = {
    {"brain_detect_communities", py_brain_detect_communities, METH_VARARGS,
     "Detect functional modules using Louvain algorithm\n\n"
     "Args:\n"
     "    brain: Brain object\n\n"
     "Returns:\n"
     "    CommunityStructure: Community detection results\n"},

    {"brain_get_modularity", py_brain_get_modularity, METH_VARARGS,
     "Get Newman's modularity Q score\n\n"
     "Args:\n"
     "    brain: Brain object\n\n"
     "Returns:\n"
     "    float: Modularity score (0.0-1.0)\n"},

    {"brain_get_num_communities", py_brain_get_num_communities, METH_VARARGS,
     "Get number of detected communities\n\n"
     "Args:\n"
     "    brain: Brain object\n\n"
     "Returns:\n"
     "    int: Number of communities\n"},

    {"brain_get_neuron_community", py_brain_get_neuron_community, METH_VARARGS,
     "Get community ID for neuron\n\n"
     "Args:\n"
     "    brain: Brain object\n"
     "    neuron_id: Neuron index\n\n"
     "Returns:\n"
     "    int: Community ID\n"},

    {"brain_detect_hubs", (PyCFunction)py_brain_detect_hubs, METH_VARARGS | METH_KEYWORDS,
     "Detect hub neurons via centrality\n\n"
     "Args:\n"
     "    brain: Brain object\n"
     "    threshold: Centrality threshold (default: 0.8)\n\n"
     "Returns:\n"
     "    HubStructure: Hub detection results\n"},

    {"brain_validate_topology", (PyCFunction)py_brain_validate_topology, METH_VARARGS | METH_KEYWORDS,
     "Validate network topology quality\n\n"
     "Args:\n"
     "    brain: Brain object\n"
     "    min_modularity: Minimum acceptable modularity (default: 0.25)\n\n"
     "Returns:\n"
     "    TopologyValidation: Validation results\n"},

    {NULL, NULL, 0, NULL}
};

//=============================================================================
// Module Initialization
//=============================================================================

int init_community_module(PyObject* module) {
    // Prepare types
    if (PyType_Ready(&CommunityStructureType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&HubStructureType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&TopologyValidationType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
        return -1;
    }

    // Add types to module
    Py_INCREF(&CommunityStructureType);
    if (PyModule_AddObject(module, "CommunityStructure", (PyObject*)&CommunityStructureType) < 0) {
        Py_DECREF(&CommunityStructureType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
        return -1;
    }
    Py_INCREF(&HubStructureType);
    if (PyModule_AddObject(module, "HubStructure", (PyObject*)&HubStructureType) < 0) {
        Py_DECREF(&HubStructureType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
        return -1;
    }
    Py_INCREF(&TopologyValidationType);
    if (PyModule_AddObject(module, "TopologyValidation", (PyObject*)&TopologyValidationType) < 0) {
        Py_DECREF(&TopologyValidationType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
        return -1;
    }

    // Add methods to module
    for (int i = 0; community_methods[i].ml_name != NULL; i++) {
        PyObject* func = PyCFunction_New(&community_methods[i], NULL);
        if (func == NULL) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
            return -1;
        }
        if (PyModule_AddObject(module, community_methods[i].ml_name, func) < 0) {
            Py_DECREF(func);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_community_module: validation failed");
            return -1;
        }
    }

    return 0;
}
