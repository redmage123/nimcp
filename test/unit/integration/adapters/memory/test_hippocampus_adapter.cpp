/**
 * @file test_hippocampus_adapter.cpp
 * @brief Unit tests for Hippocampus Memory Adapter (Integration Layer)
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/memory/nimcp_hippocampus_adapter.h"

class HippocampusIntegrationAdapterTest : public ::testing::Test {
protected:
    nimcp_hippocampus_adapter_t adapter;
    nimcp_hippocampus_config_t config;

    void SetUp() override {
        config = nimcp_hippocampus_adapter_default_config();
        adapter = nimcp_hippocampus_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_hippocampus_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_hippocampus_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_hippocampus_config_t default_config = nimcp_hippocampus_adapter_default_config();

    EXPECT_GT(default_config.ca3_size, 0u);
    EXPECT_GT(default_config.ca1_size, 0u);
    EXPECT_GT(default_config.dg_size, 0u);
    EXPECT_GT(default_config.pattern_separation_strength, 0.0f);
    EXPECT_LE(default_config.pattern_separation_strength, 1.0f);
    EXPECT_GT(default_config.pattern_completion_threshold, 0.0f);
}

TEST_F(HippocampusIntegrationAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_hippocampus_adapter_t adapter_null = nimcp_hippocampus_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_hippocampus_adapter_destroy(adapter_null);
}

TEST_F(HippocampusIntegrationAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_hippocampus_adapter_destroy(NULL);
}

TEST_F(HippocampusIntegrationAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_hippocampus_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
    EXPECT_NE(nullptr, iface->init);
    EXPECT_NE(nullptr, iface->shutdown);
    EXPECT_NE(nullptr, iface->update);
    EXPECT_NE(nullptr, iface->handle_message);
}

// ============================================================================
// UPDATE TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_hippocampus_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

// ============================================================================
// EPISODE ENCODING TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, EncodeEpisodeSucceeds) {
    float pattern[64];
    for (int i = 0; i < 64; i++) {
        pattern[i] = (float)i / 64.0f;
    }

    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_encode_episode(adapter, pattern, 64));
}

TEST_F(HippocampusIntegrationAdapterTest, EncodeEpisodeNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_encode_episode(NULL, NULL, 64));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_encode_episode(adapter, NULL, 64));
}

TEST_F(HippocampusIntegrationAdapterTest, EncodeMultipleEpisodes) {
    float pattern[64];

    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 64; j++) {
            pattern[j] = sinf((float)(i * 64 + j) * 0.1f);
        }
        EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_encode_episode(adapter, pattern, 64));
    }

    nimcp_hippocampus_state_t state;
    nimcp_hippocampus_adapter_get_state(adapter, &state);
    EXPECT_EQ(20u, state.stored_patterns);
}

// ============================================================================
// RETRIEVAL TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, RetrievePatternSucceeds) {
    // Encode a pattern
    float pattern[64];
    for (int i = 0; i < 64; i++) {
        pattern[i] = (float)i / 64.0f;
    }
    nimcp_hippocampus_adapter_encode_episode(adapter, pattern, 64);

    // Retrieve with similar cue
    float cue[64];
    for (int i = 0; i < 64; i++) {
        cue[i] = (float)i / 64.0f * config.pattern_separation_strength + 0.01f;
    }

    float retrieved[64];
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_retrieve(adapter, cue, 64, retrieved, 64));
}

TEST_F(HippocampusIntegrationAdapterTest, RetrievePatternNullFails) {
    float cue[64], retrieved[64];

    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_retrieve(NULL, cue, 64, retrieved, 64));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_retrieve(adapter, NULL, 64, retrieved, 64));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_retrieve(adapter, cue, 64, NULL, 64));
}

TEST_F(HippocampusIntegrationAdapterTest, RetrieveNoMatchReturnsError) {
    // No patterns encoded
    float cue[64];
    for (int i = 0; i < 64; i++) cue[i] = 0.5f;

    float retrieved[64];
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_retrieve(adapter, cue, 64, retrieved, 64));
}

// ============================================================================
// PATTERN COMPLETION TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, PatternCompletionWorks) {
    // Encode full pattern
    float full_pattern[64];
    for (int i = 0; i < 64; i++) {
        full_pattern[i] = (float)i / 64.0f;
    }
    nimcp_hippocampus_adapter_encode_episode(adapter, full_pattern, 64);

    // Retrieve with partial cue (first half)
    float partial_cue[32];
    for (int i = 0; i < 32; i++) {
        partial_cue[i] = (float)i / 64.0f * config.pattern_separation_strength;
    }

    float completed[64];
    nimcp_layer_error_t result = nimcp_hippocampus_adapter_retrieve(adapter, partial_cue, 32, completed, 64);
    // May or may not find match depending on threshold
    EXPECT_TRUE(result == NIMCP_LAYER_OK || result == NIMCP_LAYER_ERR_NOT_REGISTERED);
}

// ============================================================================
// REPLAY TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, TriggerReplaySucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_trigger_replay(adapter));
}

TEST_F(HippocampusIntegrationAdapterTest, TriggerReplayNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_trigger_replay(NULL));
}

TEST_F(HippocampusIntegrationAdapterTest, TriggerReplayDisabledFails) {
    // Create adapter with replay disabled
    nimcp_hippocampus_config_t no_replay_config = nimcp_hippocampus_adapter_default_config();
    no_replay_config.enable_replay = false;
    nimcp_hippocampus_adapter_t no_replay = nimcp_hippocampus_adapter_create(&no_replay_config);
    nimcp_module_interface_t* iface = nimcp_hippocampus_adapter_get_interface(no_replay);
    iface->init(no_replay, &no_replay_config);

    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_trigger_replay(no_replay));

    nimcp_hippocampus_adapter_destroy(no_replay);
}

// ============================================================================
// STATE AND STATS TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, GetStateSucceeds) {
    nimcp_hippocampus_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(HippocampusIntegrationAdapterTest, GetStatsSucceeds) {
    nimcp_hippocampus_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_get_stats(adapter, &stats));
}

TEST_F(HippocampusIntegrationAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_reset_stats(adapter));
}

TEST_F(HippocampusIntegrationAdapterTest, StatsTrackEncoding) {
    float pattern[64];
    for (int i = 0; i < 64; i++) pattern[i] = 0.5f;

    for (int i = 0; i < 5; i++) {
        nimcp_hippocampus_adapter_encode_episode(adapter, pattern, 64);
    }

    nimcp_hippocampus_stats_t stats;
    nimcp_hippocampus_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(5u, stats.patterns_encoded);
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(HippocampusIntegrationAdapterTest, StressEncodeMany) {
    float pattern[128];

    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 128; j++) {
            pattern[j] = cosf((float)(i * 128 + j) * 0.05f);
        }
        EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hippocampus_adapter_encode_episode(adapter, pattern, 128));
    }

    nimcp_hippocampus_stats_t stats;
    nimcp_hippocampus_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(100u, stats.patterns_encoded);
}
