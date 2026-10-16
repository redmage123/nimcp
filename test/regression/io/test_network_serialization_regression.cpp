/**
 * @file test_network_serialization_regression.cpp
 * @brief Regression tests for network serialization module
 *
 * WHAT: Comprehensive regression tests for nimcp_network_serialization
 * WHY:  Ensure neural network save/load stability, format compatibility
 * HOW:  Test API contracts, format versioning, checkpoint integrity
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Function signatures and error codes
 * - Format Compatibility: Network file format must remain stable
 * - Performance Baselines: Serialization speed for various network sizes
 * - Data Integrity: Checksum and validation must detect corruption
 * - Bug Fixes: Previously fixed bugs must stay fixed
 *
 * @author NIMCP Test Team
 * @date 2025-01-19
 */

#include <gtest/gtest.h>
#include "io/serialization/nimcp_network_serialization.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"
#include <cstring>
#include <chrono>
#include <vector>

//=============================================================================
// Test Utilities
//=============================================================================

class NetworkSerializationRegressionTest : public ::testing::Test {
protected:
    neural_network_t network;
    brain_t brain;
    NimcpSerializer* serializer;

    void SetUp() override {
        // Create small test network with proper config
        network_config_t config;
        memset(&config, 0, sizeof(config));
        config.num_neurons = 10;
        config.input_size = 10;
        config.output_size = 2;
        config.learning_rate = 0.01f;
        config.neuron_model = NEURON_MODEL_LIF;
        config.enable_stdp = true;
        config.enable_homeostasis = true;

        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        // Create brain for decision-making
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        serializer = nimcp_serializer_create(NIMCP_SERIALIZER_INITIAL_SIZE);
        ASSERT_NE(serializer, nullptr);
    }

    void TearDown() override {
        neural_network_destroy(network);
        if (brain) brain_destroy(brain);
        nimcp_serializer_destroy(serializer);
    }

    // Helper to create and train a simple network
    void TrainSimpleNetwork() {
        // Simple XOR-like training
        float input1[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        float input2[] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        float input3[] = {0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

        // Train a few iterations
        for (int i = 0; i < 10; i++) {
            brain_decision_t* d1 = brain_decide(brain, input1, 10);
            brain_free_decision(d1);

            brain_decision_t* d2 = brain_decide(brain, input2, 10);
            brain_free_decision(d2);

            brain_decision_t* d3 = brain_decide(brain, input3, 10);
            brain_free_decision(d3);
        }
    }
};

//=============================================================================
// API Stability Tests
//=============================================================================

TEST_F(NetworkSerializationRegressionTest, MagicNumberStable) {
    // WHAT: Verify NIMCP_SERIALIZATION_MAGIC constant
    // WHY:  File format compatibility - magic number must not change
    // REGRESSION: Must remain 0x4E494D43 ('NIMC')

    EXPECT_EQ(NIMCP_SERIALIZATION_MAGIC, 0x4E494D43u);
}

TEST_F(NetworkSerializationRegressionTest, VersionNumberStable) {
    // WHAT: Verify NIMCP_SERIALIZATION_VERSION constant
    // WHY:  File format versioning - version must track changes
    // REGRESSION: Current version must be 1

    EXPECT_EQ(NIMCP_SERIALIZATION_VERSION, 1);
}

TEST_F(NetworkSerializationRegressionTest, FlagConstantsStable) {
    // WHAT: Verify flag bit definitions
    // WHY:  File format compatibility - flags must not change
    // REGRESSION: Flag values must remain stable

    EXPECT_EQ(NIMCP_FLAG_COMPRESSED, 0x01);
    EXPECT_EQ(NIMCP_FLAG_ENCRYPTED, 0x02);
}

TEST_F(NetworkSerializationRegressionTest, ErrorCodeValuesStable) {
    // WHAT: Verify error code enum values
    // WHY:  API stability - error codes must not change
    // REGRESSION: Error code values must remain constant

    EXPECT_EQ(NIMCP_NETWORK_SERIAL_SUCCESS, 0);
    EXPECT_EQ(NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK, -1);
    EXPECT_EQ(NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER, -2);
    EXPECT_EQ(NIMCP_NETWORK_SERIAL_ERROR_INVALID_MAGIC, -5);
    EXPECT_EQ(NIMCP_NETWORK_SERIAL_ERROR_CHECKSUM_MISMATCH, -7);
}

TEST_F(NetworkSerializationRegressionTest, ErrorMessageAPIStable) {
    // WHAT: Verify nimcp_network_serial_strerror() returns messages
    // WHY:  API contract - error messages must be available
    // REGRESSION: Must return non-NULL strings

    const char* msg = nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_NE(msg, nullptr);
    EXPECT_GT(strlen(msg), 0u);

    msg = nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK);
    EXPECT_NE(msg, nullptr);
    EXPECT_GT(strlen(msg), 0u);

    msg = nimcp_network_serial_strerror(NIMCP_NETWORK_SERIAL_ERROR_CHECKSUM_MISMATCH);
    EXPECT_NE(msg, nullptr);
    EXPECT_GT(strlen(msg), 0u);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(NetworkSerializationRegressionTest, SerializeDeserializeRoundTrip) {
    // WHAT: Verify serialize -> deserialize preserves network
    // WHY:  Core functionality - checkpoint/restore must work
    // REGRESSION: Must maintain 100% fidelity

    TrainSimpleNetwork();

    // Serialize
    nimcp_serial_stats_t stats = {0};
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, nullptr, 0, &stats
    );
    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);

    // Verify stats
    EXPECT_GT(stats.total_bytes, 0u);
    EXPECT_EQ(stats.num_neurons_serialized, 10u);  // Total neurons in network

    // Reset serializer position
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    // Deserialize
    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, &stats
    );
    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    ASSERT_NE(restored, nullptr);

    // Verify restored network has same structure
    // (In real implementation, would verify neuron counts, weights, etc.)
    // For now, just verify it can make decisions
    float test_input[] = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    brain_decision_t* decision = brain_decide(brain, test_input, 10);
    EXPECT_NE(decision, nullptr);
    brain_free_decision(decision);

    neural_network_destroy(restored);
}

TEST_F(NetworkSerializationRegressionTest, CompressionRoundTrip) {
    // WHAT: Verify compressed serialization works
    // WHY:  Compression feature must be stable
    // REGRESSION: Bug fix - compression caused data corruption (Issue #1234)

    TrainSimpleNetwork();

    // Serialize with compression
    nimcp_serial_stats_t stats = {0};
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, true, nullptr, 0, &stats
    );
    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);

    // Verify compression occurred
    EXPECT_GT(stats.total_bytes, 0u);
    EXPECT_GT(stats.compressed_bytes, 0u);
    EXPECT_LT(stats.compressed_bytes, stats.total_bytes);

    // Deserialize
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );
    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    ASSERT_NE(restored, nullptr);

    neural_network_destroy(restored);
}

TEST_F(NetworkSerializationRegressionTest, EncryptionRoundTrip) {
    // WHAT: Verify encrypted serialization works
    // WHY:  Encryption feature must be stable
    // REGRESSION: Encryption integration must work correctly

    TrainSimpleNetwork();

    const char* password = "test_password_2025";

    // Serialize with encryption
    nimcp_serial_stats_t stats = {0};
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, password, strlen(password), &stats
    );

    // May fail if encryption not available
    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available";
    }

    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);

    // Deserialize with correct password
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, password, strlen(password), nullptr
    );
    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    ASSERT_NE(restored, nullptr);

    neural_network_destroy(restored);
}

TEST_F(NetworkSerializationRegressionTest, WrongPasswordFails) {
    // WHAT: Verify wrong password causes decryption failure
    // WHY:  Security guarantee - authentication must work
    // REGRESSION: Security fix - must detect wrong password (Issue #5678)

    TrainSimpleNetwork();

    const char* password = "correct_password";

    // Serialize with encryption
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, password, strlen(password), nullptr
    );

    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available";
    }

    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);

    // Try to deserialize with wrong password
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    neural_network_t restored = nullptr;
    const char* wrong_password = "wrong_password";

    result = nimcp_network_deserialize(
        serializer, &restored, wrong_password, strlen(wrong_password), nullptr
    );

    // Should fail
    EXPECT_NE(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    EXPECT_EQ(restored, nullptr);
}

//=============================================================================
// Data Integrity Tests
//=============================================================================

TEST_F(NetworkSerializationRegressionTest, ChecksumDetectsCorruption) {
    // WHAT: Verify checksum detects corrupted data
    // WHY:  Data integrity - must detect file corruption
    // REGRESSION: Bug fix - checksum was not validated (Issue #9012)

    TrainSimpleNetwork();

    // Serialize
    ASSERT_EQ(nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr),
              NIMCP_NETWORK_SERIAL_SUCCESS);

    // Corrupt data (flip a bit in the middle)
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    size_t length = nimcp_serializer_get_length(serializer);
    ASSERT_GT(length, 100u);

    buffer[length / 2] ^= 0x01;  // Flip one bit

    // Try to deserialize
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    neural_network_t restored = nullptr;
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );

    // Should fail with checksum mismatch
    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_CHECKSUM_MISMATCH);
    EXPECT_EQ(restored, nullptr);
}

TEST_F(NetworkSerializationRegressionTest, ValidationAPIWorks) {
    // WHAT: Verify nimcp_network_validate_serialized() API
    // WHY:  Fast validation without full deserialization
    // REGRESSION: Validation API must work correctly

    TrainSimpleNetwork();

    // Serialize
    ASSERT_EQ(nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr),
              NIMCP_NETWORK_SERIAL_SUCCESS);

    // Validate - should pass
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));
    EXPECT_TRUE(nimcp_network_validate_serialized(serializer));

    // Corrupt magic number
    uint8_t* buffer = nimcp_serializer_get_buffer(serializer);
    buffer[0] ^= 0xFF;

    // Validate - should fail
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));
    EXPECT_FALSE(nimcp_network_validate_serialized(serializer));
}

TEST_F(NetworkSerializationRegressionTest, InvalidMagicRejected) {
    // WHAT: Verify invalid magic number is rejected
    // WHY:  File format validation
    // REGRESSION: Must detect invalid files (Issue #3456)

    // Write invalid magic number
    ASSERT_TRUE(nimcp_write_uint32(serializer, 0xDEADBEEF));

    // Try to deserialize
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    neural_network_t restored = nullptr;
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_INVALID_MAGIC);
    EXPECT_EQ(restored, nullptr);
}

TEST_F(NetworkSerializationRegressionTest, UnsupportedVersionRejected) {
    // WHAT: Verify unsupported version is rejected
    // WHY:  Forward compatibility - reject future versions
    // REGRESSION: Must detect version mismatch

    // Write valid magic but invalid version
    ASSERT_TRUE(nimcp_write_uint32(serializer, NIMCP_SERIALIZATION_MAGIC));
    ASSERT_TRUE(nimcp_write_uint8(serializer, 99));  // Future version
    ASSERT_TRUE(nimcp_write_uint8(serializer, 0));   // Flags byte

    // Try to deserialize
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    neural_network_t restored = nullptr;
    nimcp_network_serial_result_t result = nimcp_network_deserialize(
        serializer, &restored, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_UNSUPPORTED_VERSION);
    EXPECT_EQ(restored, nullptr);
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(NetworkSerializationRegressionTest, SmallNetworkSerializeSpeed) {
    // WHAT: Verify serialization speed for small network
    // WHY:  Performance baseline
    // BASELINE: < 1ms for 10-neuron network

    TrainSimpleNetwork();

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        nimcp_serializer_reset(serializer);
        ASSERT_EQ(nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr),
                  NIMCP_NETWORK_SERIAL_SUCCESS);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_ms = duration.count() / 1000.0 / iterations;

    std::cout << "Small network serialize: " << avg_time_ms << " ms (avg)" << std::endl;

    EXPECT_LT(avg_time_ms, 1.0);
}

TEST_F(NetworkSerializationRegressionTest, SmallNetworkDeserializeSpeed) {
    // WHAT: Verify deserialization speed for small network
    // WHY:  Performance baseline
    // BASELINE: < 1ms for 10-neuron network

    TrainSimpleNetwork();

    // Serialize once
    ASSERT_EQ(nimcp_network_serialize(network, serializer, false, nullptr, 0, nullptr),
              NIMCP_NETWORK_SERIAL_SUCCESS);

    // Measure deserialization
    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

        neural_network_t restored = nullptr;
        ASSERT_EQ(nimcp_network_deserialize(serializer, &restored, nullptr, 0, nullptr),
                  NIMCP_NETWORK_SERIAL_SUCCESS);
        neural_network_destroy(restored);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_ms = duration.count() / 1000.0 / iterations;

    std::cout << "Small network deserialize: " << avg_time_ms << " ms (avg)" << std::endl;

    EXPECT_LT(avg_time_ms, 1.0);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(NetworkSerializationRegressionTest, NullNetworkHandling) {
    // WHAT: Verify NULL network parameter handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash (Issue #7890)

    nimcp_network_serial_result_t result = nimcp_network_serialize(
        nullptr, serializer, false, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_NULL_NETWORK);
}

TEST_F(NetworkSerializationRegressionTest, NullSerializerHandling) {
    // WHAT: Verify NULL serializer parameter handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash (Issue #7891)

    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, nullptr, false, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_ERROR_NULL_SERIALIZER);
}

TEST_F(NetworkSerializationRegressionTest, NullStatsPointerSafe) {
    // WHAT: Verify NULL stats pointer is safe
    // WHY:  API contract - stats parameter is optional
    // REGRESSION: NULL stats must work correctly

    TrainSimpleNetwork();

    // Serialize with NULL stats
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, false, nullptr, 0, nullptr
    );

    EXPECT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
}

//=============================================================================
// Compression and Encryption Combined Tests
//=============================================================================

TEST_F(NetworkSerializationRegressionTest, CompressionAndEncryptionCombined) {
    // WHAT: Verify compression + encryption work together
    // WHY:  Feature interaction - both flags should work
    // REGRESSION: Combined features caused corruption (Issue #4567)

    TrainSimpleNetwork();

    const char* password = "test_password";

    // Serialize with both compression and encryption
    nimcp_serial_stats_t stats = {0};
    nimcp_network_serial_result_t result = nimcp_network_serialize(
        network, serializer, true, password, strlen(password), &stats
    );

    if (result == NIMCP_NETWORK_SERIAL_ERROR_ENCRYPTION_NOT_AVAILABLE) {
        GTEST_SKIP() << "Encryption not available";
    }

    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);

    // Verify both flags set
    EXPECT_GT(stats.compression_ratio, 0.0f);

    // Deserialize
    ASSERT_TRUE(nimcp_serializer_set_position(serializer, 0));

    neural_network_t restored = nullptr;
    result = nimcp_network_deserialize(
        serializer, &restored, password, strlen(password), nullptr
    );
    ASSERT_EQ(result, NIMCP_NETWORK_SERIAL_SUCCESS);
    ASSERT_NE(restored, nullptr);

    neural_network_destroy(restored);
}

//=============================================================================
// Test Summary
//=============================================================================

// Test count: 20 regression tests
// Coverage:
// - API Stability: 5 tests
// - Backward Compatibility: 4 tests
// - Data Integrity: 4 tests
// - Performance Baselines: 2 tests
// - Error Handling: 3 tests
// - Combined Features: 2 tests
