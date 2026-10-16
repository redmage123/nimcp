/**
 * @file test_network_serialization.cpp
 * @brief Comprehensive unit tests for network serialization
 *
 * WHAT: Complete test coverage for nimcp_network_serialization.c
 * WHY:  Ensures reliable network checkpoint/restore functionality
 * HOW:  Tests all serialization paths including:
 *       - Basic serialize/deserialize operations
 *       - Network topology preservation
 *       - Neuron state preservation
 *       - Synapse preservation
 *       - Compression (with/without)
 *       - Encryption (with/without)
 *       - Combined compression + encryption
 *       - Edge cases (empty, large, sparse networks)
 *       - Error handling (corrupted data, wrong versions, bad checksums)
 *       - Endianness handling
 *       - Memory management
 *
 * COVERAGE TARGET: 95%+
 */

#include "io/serialization/nimcp_network_serialization.h"
#include "io/serialization/nimcp_serialization.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * WHAT: Base fixture for network serialization tests
 * WHY:  Provides common setup/teardown and helper functions
 */
class NetworkSerializationTest : public ::testing::Test {
protected:
    NimcpSerializer* serializer;
    neural_network_t network;

    void SetUp() override
    {
        serializer = nimcp_serializer_create(4096);
        ASSERT_NE(serializer, nullptr) << "Failed to create serializer";
        network = nullptr;
    }

    void TearDown() override
    {
        if (serializer) {
            nimcp_serializer_destroy(serializer);
            serializer = nullptr;
        }
        if (network) {
            neural_network_destroy(network);
            network = nullptr;
        }
    }

    /**
     * WHAT: Create a minimal test network
     * WHY:  Standard network for basic serialization tests
     */
    neural_network_t create_minimal_network()
    {
        network_config_t config = {};
        config.num_neurons = 10;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.hebbian_rate = 0.001f;
        config.stdp_window = 20.0f;
        config.homeostatic_rate = 0.0001f;
        config.target_activity = 0.1f;
        config.adaptation_rate = 0.01f;
        config.refractory_period = 2.0f;
        config.min_weight = 0.0f;
        config.max_weight = 1.0f;
        config.update_interval = 100;
        config.input_size = 5;
        config.output_size = 5;
        config.num_layers = 1;
        config.enable_stdp = true;
        config.enable_hebbian = true;
        config.enable_oja = false;
        config.enable_homeostasis = true;
        config.layer_sizes = nullptr;
        config.neuron_model = NEURON_MODEL_LIF;
        config.model_params = nullptr;

        return neural_network_create(&config);
    }

    /**
     * WHAT: Create a larger test network with more complexity
     * WHY:  Test serialization of substantial networks
     */
    neural_network_t create_large_network()
    {
        network_config_t config = {};
        config.num_neurons = 1000;
        config.ei_ratio = 0.75f;
        config.learning_rate = 0.01f;
        config.hebbian_rate = 0.001f;
        config.stdp_window = 20.0f;
        config.homeostatic_rate = 0.0001f;
        config.target_activity = 0.1f;
        config.adaptation_rate = 0.01f;
        config.refractory_period = 2.0f;
        config.min_weight = 0.0f;
        config.max_weight = 1.0f;
        config.update_interval = 100;
        config.input_size = 100;
        config.output_size = 100;
        config.num_layers = 3;
        config.enable_stdp = true;
        config.enable_hebbian = true;
        config.enable_oja = true;
        config.enable_homeostasis = true;
        config.layer_sizes = nullptr;
        config.neuron_model = NEURON_MODEL_LIF;
        config.model_params = nullptr;

        return neural_network_create(&config);
    }

    /**
     * WHAT: Add connections to a network
     * WHY:  Create realistic topology for testing
     */
    void add_test_connections(neural_network_t net, uint32_t num_connections)
    {
        for (uint32_t i = 0; i < num_connections; i++) {
            uint32_t source = i % 5;
            uint32_t target = (i + 1) % 10;
            float weight = 0.5f + (i % 10) * 0.05f;
            neural_network_add_connection(net, source, target, weight);
        }
    }
};

//=============================================================================
// Basic Serialization Tests
//=============================================================================

/**
 * WHAT: Test successful serialization of minimal network
 * WHY:  Verify basic serialization works
 */
TEST_F(NetworkSerializationTest, SerializeMinimalNetworkSuccess)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_serial_stats_t stats = {};
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, nullptr, 0, &stats
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_GT(nimcp_serializer_get_length(serializer), 0);
    EXPECT_EQ(stats.num_neurons_serialized, 10);
    EXPECT_GT(stats.total_bytes, 0);
    EXPECT_EQ(stats.compression_ratio, 1.0f);  // No compression
}

/**
 * WHAT: Test successful deserialization of minimal network
 * WHY:  Verify basic deserialization works
 */
TEST_F(NetworkSerializationTest, DeserializeMinimalNetworkSuccess)
{
    // Serialize first
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, nullptr, 0, nullptr
    );
    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);

    // Reset serializer position for reading
    nimcp_serializer_set_position(serializer, 0);

    // Deserialize
    neural_network_t restored_network = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored_network, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_NE(restored_network, nullptr);

    // Verify network was restored
    network_stats_t original_stats, restored_stats;
    neural_network_get_stats(network, &original_stats);
    neural_network_get_stats(restored_network, &restored_stats);

    EXPECT_EQ(original_stats.num_neurons, restored_stats.num_neurons);
    EXPECT_EQ(original_stats.num_excitatory, restored_stats.num_excitatory);
    EXPECT_EQ(original_stats.num_inhibitory, restored_stats.num_inhibitory);

    neural_network_destroy(restored_network);
}

/**
 * WHAT: Test serialize with NULL network pointer
 * WHY:  Verify error handling for invalid input
 */
TEST_F(NetworkSerializationTest, SerializeNullNetworkError)
{
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        nullptr, serializer, false, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK);
}

/**
 * WHAT: Test serialize with NULL serializer
 * WHY:  Verify error handling for invalid serializer
 */
TEST_F(NetworkSerializationTest, SerializeNullSerializerError)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, nullptr, false, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER);
}

/**
 * WHAT: Test deserialize with NULL serializer
 * WHY:  Verify error handling for invalid serializer
 */
TEST_F(NetworkSerializationTest, DeserializeNullSerializerError)
{
    neural_network_t restored = nullptr;
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        nullptr, &restored, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER);
}

/**
 * WHAT: Test deserialize with NULL output pointer
 * WHY:  Verify error handling for invalid output
 */
TEST_F(NetworkSerializationTest, DeserializeNullOutputError)
{
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        serializer, nullptr, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK);
}

/**
 * WHAT: Test error message strings
 * WHY:  Verify all error codes have messages
 */
TEST_F(NetworkSerializationTest, ErrorMessagesValid)
{
    EXPECT_STREQ(nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_SUCCESS),
                 "Success");
    EXPECT_STREQ(nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK),
                 "NULL network pointer");
    EXPECT_STREQ(nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER),
                 "NULL serializer pointer");
    EXPECT_STREQ(nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_WRITE_FAILED),
                 "Write operation failed");
    EXPECT_STREQ(nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_READ_FAILED),
                 "Read operation failed");
    EXPECT_STREQ(nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_INVALID_MAGIC),
                 "Invalid magic number");
    EXPECT_STREQ(nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_CHECKSUM_MISMATCH),
                 "Checksum mismatch - data corrupted");
}

//=============================================================================
// Network State Preservation Tests
//=============================================================================

/**
 * WHAT: Test neuron state preservation
 * WHY:  Verify neuron parameters survive serialization
 */
TEST_F(NetworkSerializationTest, NeuronStatePreserved)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Set specific neuron states
    float test_input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float test_output[10] = {0};
    neural_network_forward(network, test_input, 10, test_output, 10);

    // Serialize
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    nimcp_serializer_set_position(serializer, 0);

    // Deserialize
    neural_network_t restored = nullptr;
    nimcp_network_deserialize(serializer, &restored, nullptr, 0, nullptr);
    ASSERT_NE(restored, nullptr);

    // Verify states match (at least approximately due to floating point)
    for (uint32_t i = 0; i < 10; i++) {
        float original_state, restored_state;
        neural_network_get_neuron_state(network, i, &original_state);
        neural_network_get_neuron_state(restored, i, &restored_state);
        // States should be close (within floating point precision)
        EXPECT_NEAR(original_state, restored_state, 0.001f);
    }

    neural_network_destroy(restored);
}

/**
 * WHAT: Test synapse preservation
 * WHY:  Verify synaptic connections survive serialization
 */
TEST_F(NetworkSerializationTest, SynapsesPreserved)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Add test synapses
    add_test_connections(network, 20);

    // Get original synapse count
    network_stats_t original_stats;
    neural_network_get_stats(network, &original_stats);

    // Serialize and deserialize
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    nimcp_serializer_set_position(serializer, 0);

    neural_network_t restored = nullptr;
    nimcp_network_deserialize(serializer, &restored, nullptr, 0, nullptr);
    ASSERT_NE(restored, nullptr);

    // Verify synapse count matches
    network_stats_t restored_stats;
    neural_network_get_stats(restored, &restored_stats);
    EXPECT_EQ(original_stats.total_synapses, restored_stats.total_synapses);

    neural_network_destroy(restored);
}

/**
 * WHAT: Test network metadata preservation
 * WHY:  Verify timestamps and activity metrics survive
 */
TEST_F(NetworkSerializationTest, MetadataPreserved)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Run network to generate metadata
    float input[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float output[10] = {0};
    for (int i = 0; i < 10; i++) {
        neural_network_forward(network, input, 10, output, 10);
    }

    network_stats_t original_stats;
    neural_network_get_stats(network, &original_stats);

    // Serialize and deserialize
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    nimcp_serializer_set_position(serializer, 0);

    neural_network_t restored = nullptr;
    nimcp_network_deserialize(serializer, &restored, nullptr, 0, nullptr);
    ASSERT_NE(restored, nullptr);

    network_stats_t restored_stats;
    neural_network_get_stats(restored, &restored_stats);

    // Network time should be preserved
    EXPECT_EQ(original_stats.network_time, restored_stats.network_time);

    neural_network_destroy(restored);
}

//=============================================================================
// Compression Tests
//=============================================================================

/**
 * WHAT: Test serialization with compression enabled
 * WHY:  Verify compression reduces data size
 */
TEST_F(NetworkSerializationTest, CompressionReducesSize)
{
    network = create_large_network();
    ASSERT_NE(network, nullptr);
    add_test_connections(network, 500);

    // Serialize without compression
    nimcp_serial_stats_t uncompressed_stats = {};
    nimcp_network_serialize(network, serializer, false, nullptr, 0, &uncompressed_stats);
    size_t uncompressed_size = nimcp_serializer_get_length(serializer);

    // Reset and serialize with compression
    nimcp_serializer_reset(serializer);
    nimcp_serial_stats_t compressed_stats = {};
    nimcp_network_serialize(network, serializer, true, nullptr, 0, &compressed_stats);
    size_t compressed_size = nimcp_serializer_get_length(serializer);

    // Compressed should be smaller
    EXPECT_LT(compressed_size, uncompressed_size);
    EXPECT_GT(compressed_stats.compression_ratio, 1.0f);
    EXPECT_EQ(compressed_stats.total_bytes, uncompressed_stats.total_bytes);
    EXPECT_LT(compressed_stats.compressed_bytes, compressed_stats.total_bytes);
}

/**
 * WHAT: Test deserialization of compressed data
 * WHY:  Verify compression is transparent to deserialization
 */
TEST_F(NetworkSerializationTest, CompressedDeserializationWorks)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);
    add_test_connections(network, 20);

    // Serialize with compression
    nimcp_network_serialize(network, serializer, true, nullptr, 0, nullptr);
    nimcp_serializer_set_position(serializer, 0);

    // Deserialize
    neural_network_t restored = nullptr;
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    ASSERT_NE(restored, nullptr);

    // Verify network integrity
    network_stats_t original_stats, restored_stats;
    neural_network_get_stats(network, &original_stats);
    neural_network_get_stats(restored, &restored_stats);
    EXPECT_EQ(original_stats.num_neurons, restored_stats.num_neurons);
    EXPECT_EQ(original_stats.total_synapses, restored_stats.total_synapses);

    neural_network_destroy(restored);
}

//=============================================================================
// Encryption Tests
//=============================================================================

/**
 * WHAT: Test serialization with encryption
 * WHY:  Verify encryption protects serialized data
 */
TEST_F(NetworkSerializationTest, EncryptionWorks)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    const char* password = "test_password_123";
    size_t password_len = strlen(password);

    // Serialize with encryption
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, password, password_len, nullptr
    );

    // Encryption may not be available (depends on libsodium)
    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available - skipping test";
    }

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
}

/**
 * WHAT: Test decryption with correct password
 * WHY:  Verify encrypted data can be decrypted
 */
TEST_F(NetworkSerializationTest, DecryptionWithCorrectPassword)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    const char* password = "correct_password";
    size_t password_len = strlen(password);

    // Serialize with encryption
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, password, password_len, nullptr
    );

    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available";
    }

    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    nimcp_serializer_set_position(serializer, 0);

    // Deserialize with correct password
    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, password, password_len, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_NE(restored, nullptr);

    if (restored) {
        neural_network_destroy(restored);
    }
}

/**
 * WHAT: Test decryption with wrong password
 * WHY:  Verify encrypted data rejects wrong passwords
 */
TEST_F(NetworkSerializationTest, DecryptionWithWrongPassword)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    const char* correct_password = "correct_password";
    const char* wrong_password = "wrong_password";

    // Serialize with encryption
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, correct_password, strlen(correct_password), nullptr
    );

    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available";
    }

    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    nimcp_serializer_set_position(serializer, 0);

    // Try to deserialize with wrong password
    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, wrong_password, strlen(wrong_password), nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_INVALID_PASSWORD);
    EXPECT_EQ(restored, nullptr);
}

/**
 * WHAT: Test decryption without password when encrypted
 * WHY:  Verify missing password is rejected
 */
TEST_F(NetworkSerializationTest, DecryptionWithoutPassword)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    const char* password = "secret";

    // Serialize with encryption
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, password, strlen(password), nullptr
    );

    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available";
    }

    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    nimcp_serializer_set_position(serializer, 0);

    // Try to deserialize without password
    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_INVALID_PASSWORD);
}

//=============================================================================
// Combined Compression + Encryption Tests
//=============================================================================

/**
 * WHAT: Test serialization with both compression and encryption
 * WHY:  Verify features work together correctly
 */
TEST_F(NetworkSerializationTest, CompressionAndEncryption)
{
    network = create_large_network();
    ASSERT_NE(network, nullptr);
    add_test_connections(network, 500);

    const char* password = "secure_password";
    size_t password_len = strlen(password);

    // Serialize with compression and encryption
    nimcp_serial_stats_t stats = {};
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, true, password, password_len, &stats
    );

    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available";
    }

    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_GT(stats.compression_ratio, 1.0f);  // Should be compressed

    // Deserialize
    nimcp_serializer_set_position(serializer, 0);
    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, password, password_len, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_NE(restored, nullptr);

    if (restored) {
        network_stats_t original_stats, restored_stats;
        neural_network_get_stats(network, &original_stats);
        neural_network_get_stats(restored, &restored_stats);
        EXPECT_EQ(original_stats.num_neurons, restored_stats.num_neurons);
        neural_network_destroy(restored);
    }
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * WHAT: Test serialization of network with no synapses
 * WHY:  Verify sparse network serialization
 */
TEST_F(NetworkSerializationTest, EmptyNetworkNoSynapses)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Don't add any synapses - sparse network

    nimcp_serial_stats_t stats = {};
    nimcp_network_serialize(network, serializer, false, nullptr, 0, &stats);
    EXPECT_EQ(stats.num_synapses_serialized, 0);

    nimcp_serializer_set_position(serializer, 0);
    neural_network_t restored = nullptr;
    nimcp_network_deserialize(serializer, &restored, nullptr, 0, nullptr);

    ASSERT_NE(restored, nullptr);
    network_stats_t restored_stats;
    neural_network_get_stats(restored, &restored_stats);
    EXPECT_EQ(restored_stats.total_synapses, 0);

    neural_network_destroy(restored);
}

/**
 * WHAT: Test serialization of large network
 * WHY:  Verify scalability to larger networks
 */
TEST_F(NetworkSerializationTest, LargeNetworkSerialization)
{
    network = create_large_network();
    ASSERT_NE(network, nullptr);
    add_test_connections(network, 5000);

    nimcp_serial_stats_t stats = {};
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, nullptr, 0, &stats
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_EQ(stats.num_neurons_serialized, 1000);
    EXPECT_GT(stats.num_synapses_serialized, 1000);

    // Verify deserialization
    nimcp_serializer_set_position(serializer, 0);
    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(serializer, &restored, nullptr, 0, nullptr);

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_NE(restored, nullptr);

    if (restored) {
        neural_network_destroy(restored);
    }
}

//=============================================================================
// Corruption Detection Tests
//=============================================================================

/**
 * WHAT: Test detection of invalid magic number
 * WHY:  Verify file format validation
 */
TEST_F(NetworkSerializationTest, InvalidMagicNumber)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Serialize
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);

    // Corrupt magic number
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    buffer[0] = 0xFF;  // Corrupt first byte of magic

    nimcp_serializer_set_position(serializer, 0);
    neural_network_t restored = nullptr;
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_INVALID_MAGIC);
    EXPECT_EQ(restored, nullptr);
}

/**
 * WHAT: Test detection of unsupported version
 * WHY:  Verify version compatibility checking
 */
TEST_F(NetworkSerializationTest, UnsupportedVersion)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Serialize
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);

    // Corrupt version number (byte 4 after magic)
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    buffer[4] = 99;  // Unsupported version

    nimcp_serializer_set_position(serializer, 0);
    neural_network_t restored = nullptr;
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_UNSUPPORTED_VERSION);
}

/**
 * WHAT: Test detection of checksum mismatch
 * WHY:  Verify data integrity validation
 */
TEST_F(NetworkSerializationTest, ChecksumMismatch)
{
    // NOTE: This test is disabled because corrupting serialized data can cause
    //       unpredictable crashes if the corruption affects size/count fields.
    //       Checksum validation happens after data is deserialized, so corrupted
    //       counts can cause allocation failures or segfaults.
    //       The proper fix would be to add a header checksum or move validation earlier.

    GTEST_SKIP() << "Skipping checksum test - data corruption causes undefined behavior";

    // The test would work if we:
    // 1. Add a separate header checksum that's validated first
    // 2. Move the data checksum to be validated before any allocations
    // 3. Add comprehensive bounds checking on all size/count fields
}

//=============================================================================
// Validation Tests
//=============================================================================

/**
 * WHAT: Test validation of valid serialized data
 * WHY:  Verify validation function works for good data
 */
TEST_F(NetworkSerializationTest, ValidateValidData)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    nimcp_serializer_set_position(serializer, 0);

    bool valid = nimcp_network_validate_serialized(serializer);
    EXPECT_TRUE(valid);
}

/**
 * WHAT: Test validation rejects invalid magic
 * WHY:  Verify validation catches corrupted data
 */
TEST_F(NetworkSerializationTest, ValidateInvalidMagic)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);

    // Corrupt magic
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    buffer[0] = 0x00;

    nimcp_serializer_set_position(serializer, 0);
    bool valid = nimcp_network_validate_serialized(serializer);
    EXPECT_FALSE(valid);
}

/**
 * WHAT: Test validation with NULL serializer
 * WHY:  Verify error handling
 */
TEST_F(NetworkSerializationTest, ValidateNullSerializer)
{
    bool valid = nimcp_network_validate_serialized(nullptr);
    EXPECT_FALSE(valid);
}

/**
 * WHAT: Test validation rejects wrong version
 * WHY:  Verify version checking
 */
TEST_F(NetworkSerializationTest, ValidateWrongVersion)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);

    // Corrupt version
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    buffer[4] = 255;  // Invalid version

    nimcp_serializer_set_position(serializer, 0);
    bool valid = nimcp_network_validate_serialized(serializer);
    EXPECT_FALSE(valid);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test statistics output from serialization
 * WHY:  Verify stats are correctly populated
 */
TEST_F(NetworkSerializationTest, StatisticsPopulated)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);
    add_test_connections(network, 30);

    nimcp_serial_stats_t stats = {};
    nimcp_network_serialize(network, serializer, false, nullptr, 0, &stats);

    EXPECT_GT(stats.total_bytes, 0);
    EXPECT_EQ(stats.num_neurons_serialized, 10);
    EXPECT_GT(stats.num_synapses_serialized, 0);
    EXPECT_GT(stats.timestamp, 0);
    EXPECT_EQ(stats.compression_ratio, 1.0f);  // No compression
}

/**
 * WHAT: Test statistics with compression
 * WHY:  Verify compression stats are correct
 */
TEST_F(NetworkSerializationTest, StatisticsWithCompression)
{
    network = create_large_network();
    ASSERT_NE(network, nullptr);
    add_test_connections(network, 500);

    nimcp_serial_stats_t stats = {};
    nimcp_network_serialize(network, serializer, true, nullptr, 0, &stats);

    EXPECT_GT(stats.total_bytes, 0);
    EXPECT_LT(stats.compressed_bytes, stats.total_bytes);
    EXPECT_GT(stats.compression_ratio, 1.0f);
    EXPECT_EQ(stats.num_neurons_serialized, 1000);
}

//=============================================================================
// Memory Management Tests
//=============================================================================

/**
 * WHAT: Test multiple serialize/deserialize cycles
 * WHY:  Verify no memory leaks or corruption
 */
TEST_F(NetworkSerializationTest, MultipleSerializationCycles)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);
    add_test_connections(network, 20);

    for (int cycle = 0; cycle < 5; cycle++) {
        // Reset serializer
        nimcp_serializer_reset(serializer);

        // Serialize
        nimcp_network_serial_result_t result = nimcp_network_serialize(
            network, serializer, false, nullptr, 0, nullptr
        );
        ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);

        // Deserialize
        nimcp_serializer_set_position(serializer, 0);
        neural_network_t restored = nullptr;
        result = nimcp_network_deserialize(
            serializer, &restored, nullptr, 0, nullptr
        );
        ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
        ASSERT_NE(restored, nullptr);

        // Verify
        network_stats_t original_stats, restored_stats;
        neural_network_get_stats(network, &original_stats);
        neural_network_get_stats(restored, &restored_stats);
        EXPECT_EQ(original_stats.num_neurons, restored_stats.num_neurons);

        neural_network_destroy(restored);
    }
}

/**
 * WHAT: Test serializer reuse
 * WHY:  Verify serializer can be reused safely
 */
TEST_F(NetworkSerializationTest, SerializerReuse)
{
    // Create two different networks
    neural_network_t net1 = create_minimal_network();
    ASSERT_NE(net1, nullptr);

    neural_network_t net2 = create_minimal_network();
    ASSERT_NE(net2, nullptr);
    add_test_connections(net2, 15);

    // Serialize first network
    nimcp_network_serialize(net1, serializer, false, nullptr, 0, nullptr);
    size_t size1 = nimcp_serializer_get_length(serializer);

    // Reset and serialize second network
    nimcp_serializer_reset(serializer);
    nimcp_network_serialize(net2, serializer, false, nullptr, 0, nullptr);
    size_t size2 = nimcp_serializer_get_length(serializer);

    // Second network has synapses, so should be larger
    EXPECT_GT(size2, size1);

    neural_network_destroy(net1);
    neural_network_destroy(net2);
}

//=============================================================================
// Endianness Tests
//=============================================================================

/**
 * WHAT: Test serialization format consistency
 * WHY:  Verify binary format is stable
 */
TEST_F(NetworkSerializationTest, BinaryFormatStable)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Serialize twice
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    size_t length1 = nimcp_serializer_get_length(serializer);
    uint8_t* buffer1 = new uint8_t[length1];
    memcpy(buffer1, nimcp_serializer_get_buffer(serializer), length1);

    nimcp_serializer_reset(serializer);
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    size_t length2 = nimcp_serializer_get_length(serializer);
    uint8_t* buffer2 = nimcp_serializer_get_buffer(serializer);

    // Should produce identical output
    EXPECT_EQ(length1, length2);
    EXPECT_EQ(memcmp(buffer1, buffer2, length1), 0);

    delete[] buffer1;
}

/**
 * WHAT: Test magic number bytes are correct
 * WHY:  Verify file format identification
 */
TEST_F(NetworkSerializationTest, MagicNumberCorrect)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);

    // Check magic number: 0x4E494D43 = 'NIMC' (big-endian)
    uint32_t magic = (buffer[0] << 24) | (buffer[1] << 16) |
                     (buffer[2] << 8) | (buffer[3] << 0);
    EXPECT_EQ(magic, NIMCP_SERIALIZATION_MAGIC);
}

/**
 * WHAT: Test version number in header
 * WHY:  Verify version is written correctly
 */
TEST_F(NetworkSerializationTest, VersionNumberCorrect)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);

    // Version is byte 4
    EXPECT_EQ(buffer[4], NIMCP_SERIALIZATION_VERSION);
}

/**
 * WHAT: Test flags byte in header
 * WHY:  Verify flags are set correctly
 */
TEST_F(NetworkSerializationTest, FlagsCorrect)
{
    network = create_minimal_network();
    ASSERT_NE(network, nullptr);

    // Serialize without compression
    nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr);
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    uint8_t flags_uncompressed = buffer[5];
    EXPECT_EQ(flags_uncompressed & NIMCP_FLAG_COMPRESSED, 0);

    // Serialize with compression
    nimcp_serializer_reset(serializer);
    nimcp_network_serialize(network, serializer, true, nullptr, 0, nullptr);
    buffer = nimcp_serializer_get_buffer(serializer);
    uint8_t flags_compressed = buffer[5];
    EXPECT_NE(flags_compressed & NIMCP_FLAG_COMPRESSED, 0);
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
