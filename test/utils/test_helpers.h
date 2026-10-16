#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#ifdef __cplusplus
    // C++ includes
    #include <gtest/gtest.h>
#endif

// test_helpers.h
#include <gtest/gtest.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <cmath>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "networking/protocol/nimcp_protocol.h"
}

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Tolerance for floating point comparisons
#ifdef FLOAT_TOLERANCE
#undef FLOAT_TOLERANCE
#endif
#define FLOAT_TOLERANCE 1e-6f

// Test ports for P2P networking
#define TEST_PORT_BASE 8000
#define TEST_PORT_1 (TEST_PORT_BASE + 1)
#define TEST_PORT_2 (TEST_PORT_BASE + 2)
#define TEST_PORT_3 (TEST_PORT_BASE + 3)

// Test IP addresses
#define TEST_IP_LOCALHOST "127.0.0.1"
#define TEST_IP_INVALID "256.256.256.256"

// Test timeout values (in microseconds)
#define TEST_SHORT_TIMEOUT 10000    // 10ms
#define TEST_MEDIUM_TIMEOUT 100000  // 100ms
#define TEST_LONG_TIMEOUT 500000    // 500ms

// Test data sizes
#define TEST_SMALL_PAYLOAD 64
#define TEST_MEDIUM_PAYLOAD 512
#define TEST_LARGE_PAYLOAD 4096

//-----------------------------------------------------------------------------
// Neural Network Test Helpers
//-----------------------------------------------------------------------------

// Helper function to create a default network configuration
static network_config_t create_test_network_config(void)
{
    network_config_t config = {};
    config.num_neurons = 10;
    config.input_size = 10;
    config.output_size = 10;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.hebbian_rate = 0.1f;
    config.stdp_window = 20.0f;
    config.homeostatic_rate = 0.001f;
    config.target_activity = 0.1f;
    config.adaptation_rate = 0.1f;
    config.refractory_period = 5.0f;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.update_interval = 1000;
    return config;
}

// Helper function to compare floating point values
static bool float_equals(float a, float b)
{
    return fabsf(a - b) < FLOAT_TOLERANCE;
}

//-----------------------------------------------------------------------------
// Protocol Test Helpers
//-----------------------------------------------------------------------------

// Helper function to create a test protocol message
static void create_test_message(msg_header_t* header, msg_type_t type, const void* payload,
                                uint32_t payload_len)
{
    if (!header)
        return;

    header->magic = PROTOCOL_MAGIC;
    header->version = PROTOCOL_VERSION;
    header->type = type;
    header->length = payload_len;
    header->sequence = 0;
    header->checksum = 0;
}

// Helper function to verify protocol message fields
static bool verify_message_header(const msg_header_t* header, msg_type_t expected_type,
                                  uint32_t expected_length)
{
    if (!header)
        return false;

    return (header->magic == PROTOCOL_MAGIC && header->version == PROTOCOL_VERSION &&
            header->type == expected_type && header->length == expected_length);
}

// Helper function to generate test payload
static void generate_test_payload(uint8_t* buffer, size_t size)
{
    if (!buffer)
        return;

    for (size_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t) (i & 0xFF);
    }
}

// Helper function to verify test payload
static bool verify_test_payload(const uint8_t* buffer, size_t size)
{
    if (!buffer)
        return false;

    for (size_t i = 0; i < size; i++) {
        if (buffer[i] != (uint8_t) (i & 0xFF)) {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// P2P Node Test Helpers
//-----------------------------------------------------------------------------

// Helper function to create a default P2P node configuration
static node_config_t create_test_node_config(uint16_t port)
{
    node_config_t config = {.listen_port = port,
                            .max_peers = 10,
                            .keepalive_interval = 1000,
                            .discovery_interval = 5000,
                            .reconnect_interval = 3000,
                            .max_retries = 3,
                            .ping_interval = 2000};
    return config;
}

// Helper function to create a test peer info structure
static peer_info_t create_test_peer(const char* ip, uint16_t port)
{
    peer_info_t peer;
    memset(&peer, 0, sizeof(peer_info_t));

    if (ip) {
        strncpy(peer.ip, ip, sizeof(peer.ip) - 1);
        peer.ip[sizeof(peer.ip) - 1] = '\0';
    }

    peer.port = port;
    peer.connected = false;
    /*    peer.last_seen = 0;
        peer.retry_count = 0;
    */

    return peer;
}

// Helper function to compare peer info structures
static bool compare_peers(const peer_info_t* p1, const peer_info_t* p2)
{
    if (!p1 || !p2)
        return false;

    return (strcmp(p1->ip, p2->ip) == 0 && p1->port == p2->port && p1->connected == p2->connected);
}

// Helper function to wait for node connection
static bool wait_for_connection(p2p_node_t node, const char* peer_ip, uint16_t peer_port,
                                uint32_t timeout_us)
{
    if (!node || !peer_ip)
        return false;

    uint32_t elapsed = 0;
    uint32_t step = 1000;  // 1ms steps

    while (elapsed < timeout_us) {
        if (p2p_node_is_peer_connected(node, peer_ip, peer_port)) {
            return true;
        }

        usleep(step);
        elapsed += step;
    }

    return false;
}

//-----------------------------------------------------------------------------
// Test Classes
//-----------------------------------------------------------------------------

static network_config_t create_test_config(void)
{
    network_config_t config;
    memset(&config, 0, sizeof(config));

    // Set required test values
    config.num_neurons = 10;
    config.input_size = 3;     // Required for validation
    config.output_size = 2;    // Required for validation
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.hebbian_rate = 0.1f;
    config.stdp_window = 20.0f;
    config.homeostatic_rate = 0.001f;
    config.target_activity = 0.1f;
    config.adaptation_rate = 0.1f;
    config.refractory_period = 5.0f;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.update_interval = 1000;
    config.num_layers = 0;
    config.layer_sizes = NULL;
    config.enable_stdp = true;
    config.enable_hebbian = true;
    config.enable_oja = true;
    config.enable_homeostasis = true;
    config.neuron_model = NEURON_MODEL_LIF;
    config.model_params = NULL;

    return config;
}

// Helper class for managing test node lifecycle
class TestNode {
   public:
    TestNode(uint16_t port)
    {
        config = create_test_node_config(port);
        node = p2p_node_create(&config);
    }

    ~TestNode()
    {
        if (node) {
            p2p_node_stop(node);
            p2p_node_destroy(node);
        }
    }

    bool start()
    {
        return node && p2p_node_start(node);
    }

    bool stop()
    {
        return node && p2p_node_stop(node);
    }

    p2p_node_t get()
    {
        return node;
    }

    uint16_t port() const
    {
        return config.listen_port;
    }

   private:
    node_config_t config;
    p2p_node_t node;
};

//-----------------------------------------------------------------------------
// Brain Test Helper
//-----------------------------------------------------------------------------

/**
 * @brief Helper class for managing test brain lifecycle
 *
 * WHAT: Creates a minimal brain_t for testing cognitive modules
 * WHY:  Many cognitive modules require brain_t but don't need full functionality
 * HOW:  Creates tiny brain with minimal neurons for fast setup/teardown
 *
 * USAGE:
 *   TestBrain brain("test_brain");
 *   ASSERT_NE(brain.get(), nullptr);
 *   network_analyzer_t* analyzer = network_analyzer_create(brain.get());
 */
class TestBrain {
   public:
    /**
     * @brief Create test brain with default parameters
     * @param name Brain name for identification
     */
    explicit TestBrain(const char* name = "test_brain")
    {
        brain_ = brain_create(name, BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 10);
    }

    /**
     * @brief Create test brain with custom parameters
     * @param name Brain name
     * @param size Brain size enum
     * @param task Brain task type
     * @param num_inputs Number of input neurons
     * @param num_outputs Number of output neurons
     */
    TestBrain(const char* name, brain_size_t size, brain_task_t task,
              uint32_t num_inputs, uint32_t num_outputs)
    {
        brain_ = brain_create(name, size, task, num_inputs, num_outputs);
    }

    ~TestBrain()
    {
        if (brain_) {
            brain_destroy(brain_);
        }
    }

    // Prevent copying
    TestBrain(const TestBrain&) = delete;
    TestBrain& operator=(const TestBrain&) = delete;

    // Allow moving
    TestBrain(TestBrain&& other) noexcept : brain_(other.brain_)
    {
        other.brain_ = nullptr;
    }

    TestBrain& operator=(TestBrain&& other) noexcept
    {
        if (this != &other) {
            if (brain_) {
                brain_destroy(brain_);
            }
            brain_ = other.brain_;
            other.brain_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Get the brain pointer
     * @return brain_t pointer (nullptr if creation failed)
     */
    brain_t get() const
    {
        return brain_;
    }

    /**
     * @brief Check if brain was created successfully
     * @return true if brain is valid
     */
    bool is_valid() const
    {
        return brain_ != nullptr;
    }

   private:
    brain_t brain_;
};

// Helper function to create a minimal test brain
static inline brain_t create_test_brain(const char* name = "test_brain")
{
    return brain_create(name, BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 10);
}

//-----------------------------------------------------------------------------
// Test Assertion Macros
//-----------------------------------------------------------------------------

// Assert valid message header
#define ASSERT_VALID_MESSAGE_HEADER(header, type, len) \
    ASSERT_TRUE(verify_message_header(header, type, len))

// Assert peer connection
#define ASSERT_PEER_CONNECTED(node, ip, port) \
    ASSERT_TRUE(wait_for_connection(node, ip, port, TEST_MEDIUM_TIMEOUT))

// Assert peer disconnection
#define ASSERT_PEER_DISCONNECTED(node, ip, port) \
    ASSERT_FALSE(p2p_node_is_peer_connected(node, ip, port))

// Setup test nodes
#define SETUP_TEST_NODES(node1, node2) \
    TestNode node1(TEST_PORT_1);       \
    TestNode node2(TEST_PORT_2);       \
    ASSERT_NE(node1.get(), nullptr);   \
    ASSERT_NE(node2.get(), nullptr);   \
    ASSERT_TRUE(node1.start());        \
    ASSERT_TRUE(node2.start());        \
    usleep(TEST_SHORT_TIMEOUT)

// Verify test payload
#define ASSERT_VALID_TEST_PAYLOAD(buffer, size) \
    ASSERT_TRUE(verify_test_payload((const uint8_t*) buffer, size))

#endif  // TEST_HELPERS_H
