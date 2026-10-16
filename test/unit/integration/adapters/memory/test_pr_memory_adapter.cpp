/**
 * @file test_pr_memory_adapter.cpp
 * @brief Unit tests for Prime Resonance Memory Adapter
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/memory/nimcp_pr_memory_adapter.h"

class PRMemoryAdapterTest : public ::testing::Test {
protected:
    nimcp_pr_memory_adapter_t adapter;
    nimcp_pr_memory_config_t config;

    void SetUp() override {
        config = nimcp_pr_memory_adapter_default_config();
        adapter = nimcp_pr_memory_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_pr_memory_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_pr_memory_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_pr_memory_config_t default_config = nimcp_pr_memory_adapter_default_config();

    EXPECT_GT(default_config.max_memories, 0u);
    EXPECT_GT(default_config.z_ladder_tiers, 0u);
    EXPECT_GT(default_config.resonance_threshold, 0.0f);
    EXPECT_LE(default_config.resonance_threshold, 1.0f);
    EXPECT_GT(default_config.consolidation_rate, 0.0f);
}

TEST_F(PRMemoryAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_pr_memory_adapter_t adapter_null = nimcp_pr_memory_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_pr_memory_adapter_destroy(adapter_null);
}

TEST_F(PRMemoryAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_pr_memory_adapter_destroy(NULL);
}

TEST_F(PRMemoryAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_pr_memory_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

// ============================================================================
// UPDATE TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_pr_memory_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

// ============================================================================
// ENCODING TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, EncodeMemorySucceeds) {
    float content[32];
    for (int i = 0; i < 32; i++) {
        content[i] = (float)i / 32.0f;
    }

    pr_quaternion_state_t initial_state = {
        .w = 0.5f,  // Consolidation
        .x = 0.2f,  // Emotion
        .y = 0.8f,  // Salience
        .z = 0.9f   // Accessibility
    };

    uint32_t memory_id = 0;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_encode(adapter, content, 32, &initial_state, &memory_id));
    EXPECT_NE(0u, memory_id);
}

TEST_F(PRMemoryAdapterTest, EncodeMemoryNullFails) {
    float content[32];
    pr_quaternion_state_t state = {0.5f, 0.0f, 0.5f, 0.5f};
    uint32_t memory_id;

    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_encode(NULL, content, 32, &state, &memory_id));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_encode(adapter, NULL, 32, &state, &memory_id));
}

TEST_F(PRMemoryAdapterTest, EncodeMultipleMemories) {
    float content[32];
    pr_quaternion_state_t state = {0.5f, 0.0f, 0.5f, 0.5f};

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 32; j++) {
            content[j] = (float)(i * 32 + j) / 320.0f;
        }
        uint32_t memory_id = 0;
        EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_encode(adapter, content, 32, &state, &memory_id));
        EXPECT_NE(0u, memory_id);
    }

    nimcp_pr_memory_state_t adapter_state;
    nimcp_pr_memory_adapter_get_state(adapter, &adapter_state);
    EXPECT_EQ(10u, adapter_state.total_memories);
}

// ============================================================================
// RETRIEVAL TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, RetrieveMemorySucceeds) {
    // First encode a memory
    float content[32];
    for (int i = 0; i < 32; i++) {
        content[i] = (float)i / 32.0f;
    }
    pr_quaternion_state_t state = {0.8f, 0.3f, 0.9f, 0.95f};
    uint32_t memory_id;
    nimcp_pr_memory_adapter_encode(adapter, content, 32, &state, &memory_id);

    // Now retrieve with similar cue
    float cue[32];
    for (int i = 0; i < 32; i++) {
        cue[i] = (float)i / 32.0f + 0.01f;  // Similar with small noise
    }

    float retrieved[32];
    pr_quaternion_state_t retrieved_state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_retrieve(adapter, cue, 32, retrieved, 32, &retrieved_state));
}

TEST_F(PRMemoryAdapterTest, RetrieveMemoryNullFails) {
    float cue[32], retrieved[32];
    pr_quaternion_state_t state;

    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_retrieve(NULL, cue, 32, retrieved, 32, &state));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_retrieve(adapter, NULL, 32, retrieved, 32, &state));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_retrieve(adapter, cue, 32, NULL, 32, &state));
}

// ============================================================================
// QUATERNION STATE TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, GetMemoryStateSucceeds) {
    // Encode a memory
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;
    pr_quaternion_state_t initial = {0.6f, 0.2f, 0.7f, 0.8f};
    uint32_t memory_id;
    nimcp_pr_memory_adapter_encode(adapter, content, 32, &initial, &memory_id);

    // Get state
    pr_quaternion_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_get_memory_state(adapter, memory_id, &state));

    // Should match initial (or close to it)
    EXPECT_NEAR(state.w, initial.w, 0.1f);
    EXPECT_NEAR(state.y, initial.y, 0.1f);
}

TEST_F(PRMemoryAdapterTest, QuaternionStateInValidRange) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;
    pr_quaternion_state_t initial = {0.5f, 0.0f, 0.5f, 0.5f};
    uint32_t memory_id;
    nimcp_pr_memory_adapter_encode(adapter, content, 32, &initial, &memory_id);

    pr_quaternion_state_t state;
    nimcp_pr_memory_adapter_get_memory_state(adapter, memory_id, &state);

    EXPECT_GE(state.w, 0.0f);  // Consolidation [0,1]
    EXPECT_LE(state.w, 1.0f);
    EXPECT_GE(state.x, -1.0f); // Emotion [-1,1]
    EXPECT_LE(state.x, 1.0f);
    EXPECT_GE(state.y, 0.0f);  // Salience [0,1]
    EXPECT_LE(state.y, 1.0f);
    EXPECT_GE(state.z, 0.0f);  // Accessibility [0,1]
    EXPECT_LE(state.z, 1.0f);
}

// ============================================================================
// CONSOLIDATION TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, ConsolidateSucceeds) {
    // Encode some memories
    float content[32];
    pr_quaternion_state_t state = {0.3f, 0.0f, 0.5f, 0.5f};

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 32; j++) content[j] = (float)i / 5.0f;
        uint32_t id;
        nimcp_pr_memory_adapter_encode(adapter, content, 32, &state, &id);
    }

    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_consolidate(adapter, 100.0f));
}

TEST_F(PRMemoryAdapterTest, ConsolidationIncreasesStrength) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;
    pr_quaternion_state_t initial = {0.3f, 0.0f, 0.8f, 0.9f};
    uint32_t memory_id;
    nimcp_pr_memory_adapter_encode(adapter, content, 32, &initial, &memory_id);

    pr_quaternion_state_t before;
    nimcp_pr_memory_adapter_get_memory_state(adapter, memory_id, &before);

    // Run consolidation
    nimcp_pr_memory_adapter_consolidate(adapter, 1000.0f);

    pr_quaternion_state_t after;
    nimcp_pr_memory_adapter_get_memory_state(adapter, memory_id, &after);

    EXPECT_GE(after.w, before.w);  // Consolidation should increase
}

// ============================================================================
// STATE AND STATS TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, GetStateSucceeds) {
    nimcp_pr_memory_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(PRMemoryAdapterTest, GetStatsSucceeds) {
    nimcp_pr_memory_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_get_stats(adapter, &stats));
}

TEST_F(PRMemoryAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_reset_stats(adapter));
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(PRMemoryAdapterTest, StressEncodeMany) {
    float content[32];
    pr_quaternion_state_t state = {0.5f, 0.0f, 0.5f, 0.5f};

    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 32; j++) {
            content[j] = sinf((float)(i * 32 + j) * 0.1f);
        }
        uint32_t memory_id;
        EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pr_memory_adapter_encode(adapter, content, 32, &state, &memory_id));
    }

    nimcp_pr_memory_stats_t stats;
    nimcp_pr_memory_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(50u, stats.memories_encoded);
}
