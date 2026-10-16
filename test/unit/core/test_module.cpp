/**
 * @file test_nimcp_module.cpp
 * @brief Unit tests for NIMCP Python module implementation
 */

#include "common/nimcp_module.h"
#include "test_helpers.h"

// Ensure we're using C linkage for the Python/NIMCP C headers
#define PY_SSIZE_T_CLEAN
#include <Python.h>

namespace {

class NIMCPModuleTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        Py_Initialize();
        module = PyInit_nimcp();
    }

    void TearDown() override
    {
        if (module) {
            Py_DECREF(module);
        }
        Py_Finalize();
    }

    // Helper to create neural network object
    PyObject* CreateNeuralNetwork()
    {
        network_config_t config = create_test_network_config();
        PyObject* nn_type = PyObject_GetAttrString(module, "NeuralNetwork");
        PyObject* args = Py_BuildValue("(i)", config.num_neurons);
        PyObject* nn = PyObject_CallObject(nn_type, args);
        Py_DECREF(nn_type);
        Py_DECREF(args);
        return nn;
    }

    // Helper to create P2P node object
    PyObject* CreateP2PNode()
    {
        node_config_t config = create_test_node_config(TEST_PORT_1);
        PyObject* node_type = PyObject_GetAttrString(module, "P2PNode");
        PyObject* args = Py_BuildValue("(H)", config.listen_port);
        PyObject* node = PyObject_CallObject(node_type, args);
        Py_DECREF(node_type);
        Py_DECREF(args);
        return node;
    }

    PyObject* module;
};

// Module Initialization Tests

TEST_F(NIMCPModuleTest, ModuleInitialization)
{
    ASSERT_NE(module, nullptr);
    EXPECT_TRUE(PyModule_Check(module));
}

TEST_F(NIMCPModuleTest, TypeRegistration)
{
    PyObject* neural_network = PyObject_GetAttrString(module, "NeuralNetwork");
    ASSERT_NE(neural_network, nullptr);
    EXPECT_TRUE(PyType_Check(neural_network));
    Py_DECREF(neural_network);

    PyObject* p2p_node = PyObject_GetAttrString(module, "P2PNode");
    ASSERT_NE(p2p_node, nullptr);
    EXPECT_TRUE(PyType_Check(p2p_node));
    Py_DECREF(p2p_node);

    PyObject* network_config = PyObject_GetAttrString(module, "NetworkConfig");
    ASSERT_NE(network_config, nullptr);
    EXPECT_TRUE(PyType_Check(network_config));
    Py_DECREF(network_config);

    PyObject* node_config = PyObject_GetAttrString(module, "NodeConfig");
    ASSERT_NE(node_config, nullptr);
    EXPECT_TRUE(PyType_Check(node_config));
    Py_DECREF(node_config);
}

// Neural Network Tests

TEST_F(NIMCPModuleTest, NeuralNetworkCreation)
{
    PyObject* nn = CreateNeuralNetwork();
    ASSERT_NE(nn, nullptr);
    EXPECT_TRUE(PyObject_TypeCheck(nn, &NeuralNetworkType));

    NeuralNetworkObject* nn_obj = reinterpret_cast<NeuralNetworkObject*>(nn);
    ASSERT_NE(nn_obj, nullptr);
    ASSERT_NE(nn_obj->network, nullptr);  // neural_network_t is an opaque pointer type

    // Get network stats to verify creation
    network_stats_t stats;
    EXPECT_TRUE(neural_network_get_stats(nn_obj->network, &stats));
    EXPECT_EQ(stats.num_neurons, create_test_network_config().num_neurons);

    Py_DECREF(nn);
}

// P2P Node Tests
TEST_F(NIMCPModuleTest, P2PNodeCreation)
{
    PyObject* node = CreateP2PNode();
    ASSERT_NE(node, nullptr);
    EXPECT_TRUE(PyObject_TypeCheck(node, &P2PNodeType));

    P2PNodeObject* node_obj = reinterpret_cast<P2PNodeObject*>(node);
    ASSERT_NE(node_obj, nullptr);
    ASSERT_NE(node_obj->node, nullptr);  // p2p_node_t is an opaque pointer type

    // Verify node status
    EXPECT_EQ(p2p_node_get_status(node_obj->node), NODE_STATUS_INIT);

    Py_DECREF(node);
}


// Exception Tests

TEST_F(NIMCPModuleTest, ExceptionHierarchy)
{
    // Test base exception
    PyObject* nimcp_error = PyObject_GetAttrString(module, "NIMCPError");
    ASSERT_NE(nimcp_error, nullptr);
    EXPECT_TRUE(PyExceptionClass_Check(nimcp_error));

    // Test derived exceptions and their inheritance
    PyObject* network_error = PyObject_GetAttrString(module, "NetworkError");
    ASSERT_NE(network_error, nullptr);
    EXPECT_TRUE(PyExceptionClass_Check(network_error));
    EXPECT_TRUE(PyObject_IsSubclass(network_error, nimcp_error));

    PyObject* protocol_error = PyObject_GetAttrString(module, "ProtocolError");
    ASSERT_NE(protocol_error, nullptr);
    EXPECT_TRUE(PyExceptionClass_Check(protocol_error));
    EXPECT_TRUE(PyObject_IsSubclass(protocol_error, nimcp_error));

    PyObject* node_error = PyObject_GetAttrString(module, "NodeError");
    ASSERT_NE(node_error, nullptr);
    EXPECT_TRUE(PyExceptionClass_Check(node_error));
    EXPECT_TRUE(PyObject_IsSubclass(node_error, nimcp_error));

    Py_DECREF(nimcp_error);
    Py_DECREF(network_error);
    Py_DECREF(protocol_error);
    Py_DECREF(node_error);
}

// Configuration Tests

TEST_F(NIMCPModuleTest, NetworkConfigCreation)
{
    network_config_t test_config = create_test_network_config();

    PyObject* config_type = PyObject_GetAttrString(module, "NetworkConfig");
    PyObject* args = Py_BuildValue("(ifffffff)", test_config.num_neurons, test_config.ei_ratio,
                                   test_config.learning_rate, test_config.hebbian_rate,
                                   test_config.stdp_window, test_config.homeostatic_rate,
                                   test_config.target_activity, test_config.adaptation_rate);

    PyObject* config = PyObject_CallObject(config_type, args);
    ASSERT_NE(config, nullptr);

    NetworkConfigObject* config_obj = reinterpret_cast<NetworkConfigObject*>(config);
    EXPECT_TRUE(float_equals(config_obj->config.ei_ratio, test_config.ei_ratio));
    EXPECT_TRUE(float_equals(config_obj->config.learning_rate, test_config.learning_rate));

    Py_DECREF(config_type);
    Py_DECREF(args);
    Py_DECREF(config);
}

TEST_F(NIMCPModuleTest, NodeConfigCreation)
{
    node_config_t test_config = create_test_node_config(TEST_PORT_1);

    PyObject* config_type = PyObject_GetAttrString(module, "NodeConfig");
    PyObject* args = Py_BuildValue("(Hiiiii)", test_config.listen_port, test_config.max_peers,
                                   test_config.keepalive_interval, test_config.discovery_interval,
                                   test_config.reconnect_interval, test_config.max_retries);

    PyObject* config = PyObject_CallObject(config_type, args);
    ASSERT_NE(config, nullptr);

    NodeConfigObject* config_obj = reinterpret_cast<NodeConfigObject*>(config);
    EXPECT_EQ(config_obj->config.listen_port, test_config.listen_port);
    EXPECT_EQ(config_obj->config.max_peers, test_config.max_peers);

    Py_DECREF(config_type);
    Py_DECREF(args);
    Py_DECREF(config);
}

// Reference Counting Tests

TEST_F(NIMCPModuleTest, ReferenceCounting)
{
    PyObject* nn = CreateNeuralNetwork();
    Py_ssize_t initial_refcount = Py_REFCNT(nn);

    // Create a new reference
    Py_INCREF(nn);
    EXPECT_EQ(Py_REFCNT(nn), initial_refcount + 1);

    // Remove references
    Py_DECREF(nn);
    EXPECT_EQ(Py_REFCNT(nn), initial_refcount);

    Py_DECREF(nn);
}

}  // anonymous namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
