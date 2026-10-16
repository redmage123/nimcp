#ifndef NIMCP_MODULE_H
#define NIMCP_MODULE_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>  // Use CMake-provided Python includes
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "networking/protocol/nimcp_protocol.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "nimcp.h"

// Exception types
// WHY WEAK: Allow ODR-safe linking when test binaries include Python module
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) extern PyObject* NIMCPError;
__attribute__((weak)) extern PyObject* NetworkError;
__attribute__((weak)) extern PyObject* ProtocolError;
__attribute__((weak)) extern PyObject* NodeError;
#else
extern PyObject* NIMCPError;
extern PyObject* NetworkError;
extern PyObject* ProtocolError;
extern PyObject* NodeError;
#endif

// Type objects
extern PyTypeObject BrainType;
extern PyTypeObject BrainConfigType;
extern PyTypeObject NeuralNetworkType;
extern PyTypeObject P2PNodeType;
extern PyTypeObject NetworkConfigType;
extern PyTypeObject NodeConfigType;
extern PyTypeObject GlialIntegrationType;

// Custom types definitions
typedef struct {
    PyObject_HEAD nimcp_brain_t brain;
} BrainObject;

typedef struct {
    PyObject_HEAD
    uint32_t num_inputs;
    uint32_t num_outputs;
    PyObject* hidden_layers;    // List of layer sizes
    char task_name[64];         // Task type name
    nimcp_brain_size_t size;    // Brain size preset
    nimcp_brain_task_t task;    // Task type enum
} BrainConfigObject;

typedef struct {
    PyObject_HEAD neural_network_t network;
} NeuralNetworkObject;

typedef struct {
    PyObject_HEAD p2p_node_t node;
} P2PNodeObject;

typedef struct {
    PyObject_HEAD network_config_t config;
} NetworkConfigObject;

typedef struct {
    PyObject_HEAD node_config_t config;
} NodeConfigObject;

typedef struct {
    PyObject_HEAD
    glial_integration_t* integration;
    PyObject* network;  // Reference to owning network
} GlialIntegrationObject;

// Module initialization functions
PyMODINIT_FUNC PyInit_nimcp(void);
extern int init_topology_module(PyObject* module);
extern int init_pink_noise_module(PyObject* module);
extern int init_community_module(PyObject* module);

#endif  // NIMCP_MODULE_H
