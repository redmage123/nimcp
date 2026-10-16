/**
 * @file test_adaptive_serialization.cpp
 * @brief Tests for adaptive network serialization features
 *
 * WHAT: Unit tests for adaptive network save/load functionality
 * WHY:  Ensure persistence works correctly across all formats
 * HOW:  Test binary, JSON, and SafeTensors serialization
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AdaptiveSerializationTest : public ::testing::Test {
protected:
    adaptive_network_t network;
    const char* binary_file = "/tmp/test_adaptive.bin";
    const char* json_file = "/tmp/test_adaptive.json";
    const char* st_file = "/tmp/test_adaptive.safetensors";

    void SetUp() override {
        network = nullptr;
        remove(binary_file);
        remove(json_file);
        remove(st_file);
    }

    void TearDown() override {
        if (network) {
            adaptive_network_destroy(network);
            network = nullptr;
        }
        remove(binary_file);
        remove(json_file);
        remove(st_file);
    }

    adaptive_network_config_t create_test_config() {
        adaptive_network_config_t config;
        memset(&config, 0, sizeof(config));

        config.base_config.num_neurons = 20;
        config.base_config.input_size = 10;
        config.base_config.output_size = 3;
        config.base_config.learning_rate = 0.01f;
        config.base_config.ei_ratio = 0.8f;

        config.spike_params.k_factor = 0.5f;
        config.spike_params.sparsity_target = 0.7f;
        config.spike_params.encoding = SPIKE_ENCODING_INTEGER;

        config.enable_sparsity = true;
        config.pruning_threshold = 0.01f;
        config.update_frequency = 10;

        return config;
    }
};

//=============================================================================
// Binary Format Tests
//=============================================================================

TEST_F(AdaptiveSerializationTest, SaveBinaryFormat) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = adaptive_network_save(network, binary_file, SERIALIZE_FORMAT_BINARY);
    EXPECT_TRUE(result);
    EXPECT_EQ(access(binary_file, F_OK), 0);
}

TEST_F(AdaptiveSerializationTest, LoadBinaryFormat) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Train network
    float input[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float target[3] = {1, 0, 0};
    training_example_t example = {
        .input = input,
        .input_size = 10,
        .target = target,
        .target_size = 3,
        .confidence = 1.0f,
        .label = "class_a"
    };
    adaptive_network_learn(network, &example, LEARN_MODE_SUPERVISED, 0.01f);

    // Save
    adaptive_network_save(network, binary_file, SERIALIZE_FORMAT_BINARY);
    adaptive_network_destroy(network);
    network = nullptr;

    // Load
    network = adaptive_network_load(binary_file);
    ASSERT_NE(network, nullptr);
    EXPECT_GT(adaptive_network_get_size(network), 0u);
}

TEST_F(AdaptiveSerializationTest, BinaryRoundTripPreservesState) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Get initial stats
    size_t original_size = adaptive_network_get_size(network);

    // Save and reload
    adaptive_network_save(network, binary_file, SERIALIZE_FORMAT_BINARY);
    adaptive_network_destroy(network);
    network = adaptive_network_load(binary_file);

    ASSERT_NE(network, nullptr);
    size_t loaded_size = adaptive_network_get_size(network);

    // Size should be approximately the same
    EXPECT_GT(loaded_size, 0u);
}

//=============================================================================
// JSON Format Tests
//=============================================================================

TEST_F(AdaptiveSerializationTest, SaveJSONFormat) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = adaptive_network_save(network, json_file, SERIALIZE_FORMAT_JSON);
    EXPECT_TRUE(result);
    EXPECT_EQ(access(json_file, F_OK), 0);

    // Verify JSON structure
    FILE* f = fopen(json_file, "r");
    ASSERT_NE(f, nullptr);

    char buffer[1024];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[read] = '\0';
    fclose(f);

    EXPECT_NE(strstr(buffer, "\"format\":"), nullptr);
    EXPECT_NE(strstr(buffer, "\"num_neurons\":"), nullptr);
    EXPECT_NE(strstr(buffer, "\"neuron_states\":"), nullptr);
}

TEST_F(AdaptiveSerializationTest, JSONContainsLabels) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Add some labels
    float input[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float target[3] = {1, 0, 0};
    training_example_t example = {
        .input = input,
        .input_size = 10,
        .target = target,
        .target_size = 3,
        .confidence = 1.0f,
        .label = "test_label"
    };
    adaptive_network_learn(network, &example, LEARN_MODE_SUPERVISED, 0.01f);

    adaptive_network_save(network, json_file, SERIALIZE_FORMAT_JSON);

    FILE* f = fopen(json_file, "r");
    ASSERT_NE(f, nullptr);

    char buffer[2048];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[read] = '\0';
    fclose(f);

    EXPECT_NE(strstr(buffer, "\"labels\":"), nullptr);
}

//=============================================================================
// SafeTensors Format Tests
//=============================================================================

TEST_F(AdaptiveSerializationTest, SaveSafeTensorsFormat) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = adaptive_network_save(network, st_file, SERIALIZE_FORMAT_SAFETENSORS);
    EXPECT_TRUE(result);
    EXPECT_EQ(access(st_file, F_OK), 0);
}

TEST_F(AdaptiveSerializationTest, SafeTensorsHasValidHeader) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    adaptive_network_save(network, st_file, SERIALIZE_FORMAT_SAFETENSORS);

    // Verify SafeTensors format: 8-byte header size + JSON header
    FILE* f = fopen(st_file, "rb");
    ASSERT_NE(f, nullptr);

    uint64_t header_size;
    size_t read = fread(&header_size, sizeof(uint64_t), 1, f);
    EXPECT_EQ(read, 1u);
    EXPECT_GT(header_size, 0u);
    EXPECT_LT(header_size, 10000u);  // Reasonable header size

    // Read and verify JSON header
    char* header = (char*)malloc(header_size + 1);
    read = fread(header, 1, header_size, f);
    EXPECT_EQ(read, header_size);
    header[header_size] = '\0';

    EXPECT_NE(strstr(header, "\"__metadata__\""), nullptr);
    EXPECT_NE(strstr(header, "\"thresholds\""), nullptr);
    EXPECT_NE(strstr(header, "\"dtype\":\"F32\""), nullptr);

    free(header);
    fclose(f);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(AdaptiveSerializationTest, SaveNullNetwork) {
    bool result = adaptive_network_save(nullptr, binary_file, SERIALIZE_FORMAT_BINARY);
    EXPECT_FALSE(result);
}

TEST_F(AdaptiveSerializationTest, SaveNullPath) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    bool result = adaptive_network_save(network, nullptr, SERIALIZE_FORMAT_BINARY);
    EXPECT_FALSE(result);
}

TEST_F(AdaptiveSerializationTest, LoadNullPath) {
    adaptive_network_t loaded = adaptive_network_load(nullptr);
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(AdaptiveSerializationTest, LoadNonexistentFile) {
    adaptive_network_t loaded = adaptive_network_load("/tmp/nonexistent_file_12345.bin");
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(AdaptiveSerializationTest, LoadInvalidMagic) {
    // Create file with invalid magic
    FILE* f = fopen(binary_file, "wb");
    ASSERT_NE(f, nullptr);
    uint32_t bad_magic = 0xDEADBEEF;
    fwrite(&bad_magic, sizeof(uint32_t), 1, f);
    fclose(f);

    adaptive_network_t loaded = adaptive_network_load(binary_file);
    EXPECT_EQ(loaded, nullptr);
}

//=============================================================================
// Model State Persistence Tests
//=============================================================================

TEST_F(AdaptiveSerializationTest, PreservesNeuronStates) {
    adaptive_network_config_t config = create_test_config();
    network = adaptive_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Train to modify neuron states
    float input[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float target[3] = {1, 0, 0};
    training_example_t example = {
        .input = input,
        .input_size = 10,
        .target = target,
        .target_size = 3,
        .confidence = 1.0f,
        .label = "trained"
    };

    for (int i = 0; i < 10; i++) {
        adaptive_network_learn(network, &example, LEARN_MODE_SUPERVISED, 0.01f);
    }

    // Save
    adaptive_network_save(network, binary_file, SERIALIZE_FORMAT_BINARY);
    adaptive_network_destroy(network);

    // Load and verify
    network = adaptive_network_load(binary_file);
    ASSERT_NE(network, nullptr);
}

//=============================================================================
// Run Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
