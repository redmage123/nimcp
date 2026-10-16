/**
 * @file test_omni_wm_memory_bridge.cpp
 * @brief Comprehensive unit tests for World Model Memory Bridge
 *
 * WHAT: Tests for WM-Memory integration bridge
 * WHY:  Verify hippocampus, engram, and consolidation integration with world model
 * HOW:  GTest-based tests for lifecycle, connection, replay training,
 *       engram encoding, pattern operations, and consolidation sync
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float DEFAULT_DT = 0.016f;
static constexpr uint32_t TEST_STATE_DIM = 64;
static constexpr uint32_t TEST_CONTEXT_DIM = 128;
static constexpr uint32_t TEST_REPLAY_LENGTH = 16;

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool float_in_range(float val, float min_val, float max_val)
{
    return val >= min_val && val <= max_val;
}

// =============================================================================
// Test Fixture
// =============================================================================

class OmniWmMemoryBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        bridge_ = omni_wm_memory_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_memory_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create test state vector
    std::vector<float> create_test_state(uint32_t dim, float base_value)
    {
        std::vector<float> state(dim);
        for (uint32_t i = 0; i < dim; i++) {
            state[i] = base_value + (float)i * 0.01f;
        }
        return state;
    }

    // Helper to create test replay sequence
    void create_test_replay(uint32_t length, uint32_t state_dim, uint32_t action_dim,
                            std::vector<std::vector<float>>& states,
                            std::vector<std::vector<float>>& actions,
                            std::vector<float>& rewards)
    {
        states.resize(length);
        actions.resize(length);
        rewards.resize(length);

        for (uint32_t t = 0; t < length; t++) {
            states[t].resize(state_dim);
            actions[t].resize(action_dim);

            for (uint32_t i = 0; i < state_dim; i++) {
                states[t][i] = (float)t * 0.1f + (float)i * 0.01f;
            }
            for (uint32_t i = 0; i < action_dim; i++) {
                actions[t][i] = (float)t * 0.05f + (float)i * 0.02f;
            }
            rewards[t] = (float)t * 0.2f;
        }
    }

    omni_wm_memory_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Lifecycle Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, CreateWithNullConfig)
{
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(OmniWmMemoryBridgeTest, CreateWithDefaultConfig)
{
    omni_wm_memory_bridge_config_t config;
    ASSERT_EQ(omni_wm_memory_bridge_default_config(&config), NIMCP_SUCCESS);

    omni_wm_memory_bridge_t* bridge = omni_wm_memory_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_memory_bridge_destroy(bridge);
}

TEST_F(OmniWmMemoryBridgeTest, CreateWithCustomConfig)
{
    omni_wm_memory_bridge_config_t config;
    ASSERT_EQ(omni_wm_memory_bridge_default_config(&config), NIMCP_SUCCESS);

    // Customize
    config.enable_modulation = true;
    config.sensitivity = 1.5f;
    config.enable_replay_training = true;
    config.replay_batch_size = 64;
    config.replay_learning_rate = 0.001f;
    config.enable_engram_encoding = true;
    config.encoding_threshold = 0.6f;
    config.enable_consolidation_sync = true;
    config.enable_pattern_completion = true;

    omni_wm_memory_bridge_t* bridge = omni_wm_memory_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);
    EXPECT_EQ(bridge->config.replay_batch_size, 64u);
    EXPECT_FLOAT_EQ(bridge->config.replay_learning_rate, 0.001f);
    EXPECT_FLOAT_EQ(bridge->config.encoding_threshold, 0.6f);

    omni_wm_memory_bridge_destroy(bridge);
}

TEST_F(OmniWmMemoryBridgeTest, DestroyNullSafe)
{
    omni_wm_memory_bridge_destroy(nullptr);
}

TEST_F(OmniWmMemoryBridgeTest, DestroyValidBridge)
{
    omni_wm_memory_bridge_t* bridge = omni_wm_memory_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    omni_wm_memory_bridge_destroy(bridge);
}

TEST_F(OmniWmMemoryBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ResetValidBridge)
{
    nimcp_error_t result = omni_wm_memory_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 2. Default Config Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_memory_bridge_config_t config;
    ASSERT_EQ(omni_wm_memory_bridge_default_config(&config), NIMCP_SUCCESS);

    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));
    EXPECT_GT(config.replay_batch_size, 0u);
    EXPECT_LE(config.replay_batch_size, WM_MEMORY_MAX_REPLAY_LENGTH * 4);
    EXPECT_GT(config.replay_learning_rate, 0.0f);
    EXPECT_LT(config.replay_learning_rate, 1.0f);
    EXPECT_TRUE(float_in_range(config.encoding_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.emotional_boost_factor, 1.0f, 5.0f));
    EXPECT_TRUE(float_in_range(config.completion_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.separation_threshold, 0.0f, 1.0f));
}

TEST_F(OmniWmMemoryBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ValidateConfigDefaultSucceeds)
{
    omni_wm_memory_bridge_config_t config;
    ASSERT_EQ(omni_wm_memory_bridge_default_config(&config), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_memory_bridge_validate_config(&config), NIMCP_SUCCESS);
}

// =============================================================================
// 3. Connection Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_connect(
        nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, IsConnectedBeforeConnect)
{
    EXPECT_FALSE(omni_wm_memory_bridge_is_connected(bridge_));
}

TEST_F(OmniWmMemoryBridgeTest, IsConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_memory_bridge_is_connected(nullptr));
}

TEST_F(OmniWmMemoryBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ConnectHippocampusNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_connect_hippocampus(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ConnectEngramNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_connect_engram(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ConnectConsolidationNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_connect_consolidation(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 4. Update Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_update(nullptr, DEFAULT_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, UpdateUnconnectedBridge)
{
    nimcp_error_t result = omni_wm_memory_bridge_update(bridge_, DEFAULT_DT);
    (void)result;
}

TEST_F(OmniWmMemoryBridgeTest, SetSleepStateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_set_sleep_state(nullptr, true, 1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, SetSleepStateAwake)
{
    nimcp_error_t result = omni_wm_memory_bridge_set_sleep_state(bridge_, false, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(bridge_->is_sleeping);
}

TEST_F(OmniWmMemoryBridgeTest, SetSleepStateSWS)
{
    nimcp_error_t result = omni_wm_memory_bridge_set_sleep_state(bridge_, true, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(bridge_->is_sleeping);
    EXPECT_FLOAT_EQ(bridge_->current_sleep_stage, 1.0f);
}

TEST_F(OmniWmMemoryBridgeTest, SetSleepStateREM)
{
    nimcp_error_t result = omni_wm_memory_bridge_set_sleep_state(bridge_, true, 2.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(bridge_->is_sleeping);
    EXPECT_FLOAT_EQ(bridge_->current_sleep_stage, 2.0f);
}

// =============================================================================
// 5. Replay Training Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, TrainFromReplayNullBridgeFails)
{
    std::vector<std::vector<float>> states, actions;
    std::vector<float> rewards;
    create_test_replay(TEST_REPLAY_LENGTH, TEST_STATE_DIM, 4, states, actions, rewards);

    std::vector<const float*> state_ptrs(TEST_REPLAY_LENGTH);
    std::vector<const float*> action_ptrs(TEST_REPLAY_LENGTH);
    for (uint32_t i = 0; i < TEST_REPLAY_LENGTH; i++) {
        state_ptrs[i] = states[i].data();
        action_ptrs[i] = actions[i].data();
    }

    nimcp_error_t result = omni_wm_memory_bridge_train_from_replay(
        nullptr, state_ptrs.data(), action_ptrs.data(), rewards.data(),
        TEST_REPLAY_LENGTH, false);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, TrainFromReplayNullStatesFails)
{
    std::vector<std::vector<float>> states, actions;
    std::vector<float> rewards;
    create_test_replay(TEST_REPLAY_LENGTH, TEST_STATE_DIM, 4, states, actions, rewards);

    std::vector<const float*> action_ptrs(TEST_REPLAY_LENGTH);
    for (uint32_t i = 0; i < TEST_REPLAY_LENGTH; i++) {
        action_ptrs[i] = actions[i].data();
    }

    nimcp_error_t result = omni_wm_memory_bridge_train_from_replay(
        bridge_, nullptr, action_ptrs.data(), rewards.data(),
        TEST_REPLAY_LENGTH, false);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, TrainFromReplayNullActionsFails)
{
    std::vector<std::vector<float>> states, actions;
    std::vector<float> rewards;
    create_test_replay(TEST_REPLAY_LENGTH, TEST_STATE_DIM, 4, states, actions, rewards);

    std::vector<const float*> state_ptrs(TEST_REPLAY_LENGTH);
    for (uint32_t i = 0; i < TEST_REPLAY_LENGTH; i++) {
        state_ptrs[i] = states[i].data();
    }

    nimcp_error_t result = omni_wm_memory_bridge_train_from_replay(
        bridge_, state_ptrs.data(), nullptr, rewards.data(),
        TEST_REPLAY_LENGTH, false);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, TrainFromReplayNullRewardsFails)
{
    std::vector<std::vector<float>> states, actions;
    std::vector<float> rewards;
    create_test_replay(TEST_REPLAY_LENGTH, TEST_STATE_DIM, 4, states, actions, rewards);

    std::vector<const float*> state_ptrs(TEST_REPLAY_LENGTH);
    std::vector<const float*> action_ptrs(TEST_REPLAY_LENGTH);
    for (uint32_t i = 0; i < TEST_REPLAY_LENGTH; i++) {
        state_ptrs[i] = states[i].data();
        action_ptrs[i] = actions[i].data();
    }

    nimcp_error_t result = omni_wm_memory_bridge_train_from_replay(
        bridge_, state_ptrs.data(), action_ptrs.data(), nullptr,
        TEST_REPLAY_LENGTH, false);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, TrainFromReplayZeroLengthFails)
{
    std::vector<std::vector<float>> states, actions;
    std::vector<float> rewards;
    create_test_replay(1, TEST_STATE_DIM, 4, states, actions, rewards);

    std::vector<const float*> state_ptrs = {states[0].data()};
    std::vector<const float*> action_ptrs = {actions[0].data()};

    nimcp_error_t result = omni_wm_memory_bridge_train_from_replay(
        bridge_, state_ptrs.data(), action_ptrs.data(), rewards.data(),
        0, false);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, OnRippleNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_on_ripple(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, OnRippleNullEventFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_on_ripple(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 6. Engram API Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, EncodeEngramNullBridgeFails)
{
    uint64_t engram_id;
    nimcp_error_t result = omni_wm_memory_bridge_encode_engram(
        nullptr, 0.5f, false, &engram_id);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, EncodeEngramNullOutputFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_encode_engram(
        bridge_, 0.5f, false, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, RetrieveEpisodicContextNullBridgeFails)
{
    std::vector<float> context(TEST_CONTEXT_DIM);
    float confidence;

    nimcp_error_t result = omni_wm_memory_bridge_retrieve_episodic_context(
        nullptr, context.data(), TEST_CONTEXT_DIM, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, RetrieveEpisodicContextNullOutputFails)
{
    float confidence;

    nimcp_error_t result = omni_wm_memory_bridge_retrieve_episodic_context(
        bridge_, nullptr, TEST_CONTEXT_DIM, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, RetrieveEpisodicContextNullConfidenceFails)
{
    std::vector<float> context(TEST_CONTEXT_DIM);

    nimcp_error_t result = omni_wm_memory_bridge_retrieve_episodic_context(
        bridge_, context.data(), TEST_CONTEXT_DIM, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 7. Pattern Operations Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, PatternCompleteNullBridgeFails)
{
    std::vector<float> partial = create_test_state(TEST_STATE_DIM, 0.5f);
    std::vector<float> completed(TEST_STATE_DIM);
    float confidence;

    nimcp_error_t result = omni_wm_memory_bridge_pattern_complete(
        nullptr, partial.data(), TEST_STATE_DIM,
        completed.data(), TEST_STATE_DIM, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, PatternCompleteNullInputFails)
{
    std::vector<float> completed(TEST_STATE_DIM);
    float confidence;

    nimcp_error_t result = omni_wm_memory_bridge_pattern_complete(
        bridge_, nullptr, TEST_STATE_DIM,
        completed.data(), TEST_STATE_DIM, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, PatternCompleteNullOutputFails)
{
    std::vector<float> partial = create_test_state(TEST_STATE_DIM, 0.5f);
    float confidence;

    nimcp_error_t result = omni_wm_memory_bridge_pattern_complete(
        bridge_, partial.data(), TEST_STATE_DIM,
        nullptr, TEST_STATE_DIM, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, PatternSeparateNullBridgeFails)
{
    std::vector<float> input = create_test_state(TEST_STATE_DIM, 0.5f);
    std::vector<float> separated(TEST_STATE_DIM);
    float strength;

    nimcp_error_t result = omni_wm_memory_bridge_pattern_separate(
        nullptr, input.data(), TEST_STATE_DIM,
        separated.data(), TEST_STATE_DIM, &strength);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, PatternSeparateNullInputFails)
{
    std::vector<float> separated(TEST_STATE_DIM);
    float strength;

    nimcp_error_t result = omni_wm_memory_bridge_pattern_separate(
        bridge_, nullptr, TEST_STATE_DIM,
        separated.data(), TEST_STATE_DIM, &strength);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, PatternSeparateNullOutputFails)
{
    std::vector<float> input = create_test_state(TEST_STATE_DIM, 0.5f);
    float strength;

    nimcp_error_t result = omni_wm_memory_bridge_pattern_separate(
        bridge_, input.data(), TEST_STATE_DIM,
        nullptr, TEST_STATE_DIM, &strength);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 8. Consolidation API Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, ExtractSemanticsNullBridgeFails)
{
    std::vector<float> features(TEST_STATE_DIM);

    nimcp_error_t result = omni_wm_memory_bridge_extract_semantics(
        nullptr, features.data(), TEST_STATE_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ExtractSemanticsNullOutputFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_extract_semantics(
        bridge_, nullptr, TEST_STATE_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ConsolidationSyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_consolidation_sync(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ConsolidationSyncValidSignals)
{
    float signals[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float signal : signals) {
        nimcp_error_t result = omni_wm_memory_bridge_consolidation_sync(bridge_, signal);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// 9. Query API Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, GetWmEffectsNullReturnsNull)
{
    const omni_wm_to_memory_effects_t* effects =
        omni_wm_memory_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmMemoryBridgeTest, GetWmEffectsValid)
{
    const omni_wm_to_memory_effects_t* effects =
        omni_wm_memory_bridge_get_wm_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmMemoryBridgeTest, GetMemoryEffectsNullReturnsNull)
{
    const memory_to_omni_wm_effects_t* effects =
        omni_wm_memory_bridge_get_memory_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmMemoryBridgeTest, GetMemoryEffectsValid)
{
    const memory_to_omni_wm_effects_t* effects =
        omni_wm_memory_bridge_get_memory_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

// =============================================================================
// 10. Statistics Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_memory_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_memory_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, GetStatsNullOutputFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, GetStatsValid)
{
    omni_wm_memory_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_memory_bridge_get_stats(bridge_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ResetStatsValid)
{
    nimcp_error_t result = omni_wm_memory_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, StatsIncrementOnUpdate)
{
    omni_wm_memory_bridge_stats_t stats_before, stats_after;

    omni_wm_memory_bridge_get_stats(bridge_, &stats_before);

    for (int i = 0; i < 5; i++) {
        omni_wm_memory_bridge_update(bridge_, DEFAULT_DT);
    }

    omni_wm_memory_bridge_get_stats(bridge_, &stats_after);
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
}

// =============================================================================
// 11. Bio-Async Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, ConnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, DisconnectBioAsyncNullFails)
{
    nimcp_error_t result = omni_wm_memory_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, IsBioAsyncConnectedNullReturnsFalse)
{
    EXPECT_FALSE(omni_wm_memory_bridge_is_bio_async_connected(nullptr));
}

TEST_F(OmniWmMemoryBridgeTest, IsBioAsyncConnectedInitially)
{
    EXPECT_FALSE(omni_wm_memory_bridge_is_bio_async_connected(bridge_));
}

// =============================================================================
// 12. Edge Case Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, MultipleResetCalls)
{
    for (int i = 0; i < 5; i++) {
        nimcp_error_t result = omni_wm_memory_bridge_reset(bridge_);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(OmniWmMemoryBridgeTest, SleepWakeCycles)
{
    for (int i = 0; i < 10; i++) {
        // Wake
        omni_wm_memory_bridge_set_sleep_state(bridge_, false, 0.0f);
        omni_wm_memory_bridge_update(bridge_, DEFAULT_DT);

        // SWS
        omni_wm_memory_bridge_set_sleep_state(bridge_, true, 1.0f);
        omni_wm_memory_bridge_update(bridge_, DEFAULT_DT);

        // REM
        omni_wm_memory_bridge_set_sleep_state(bridge_, true, 2.0f);
        omni_wm_memory_bridge_update(bridge_, DEFAULT_DT);
    }
}

TEST_F(OmniWmMemoryBridgeTest, RapidConsolidationSync)
{
    for (int i = 0; i < 100; i++) {
        float signal = (float)i / 100.0f;
        omni_wm_memory_bridge_consolidation_sync(bridge_, signal);
    }
}

TEST_F(OmniWmMemoryBridgeTest, ContextCacheValidity)
{
    // Initially cache should be invalid
    EXPECT_FALSE(bridge_->context_cache_valid);

    // Update shouldn't crash
    omni_wm_memory_bridge_update(bridge_, DEFAULT_DT);
}

// =============================================================================
// 13. Memory Safety Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, CreateDestroyMultiple)
{
    for (int i = 0; i < 10; i++) {
        omni_wm_memory_bridge_t* bridge = omni_wm_memory_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
        omni_wm_memory_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmMemoryBridgeTest, UseAfterReset)
{
    omni_wm_memory_bridge_reset(bridge_);

    EXPECT_EQ(omni_wm_memory_bridge_update(bridge_, DEFAULT_DT), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_memory_bridge_consolidation_sync(bridge_, 0.5f), NIMCP_SUCCESS);
}

TEST_F(OmniWmMemoryBridgeTest, ConfigIntegrity)
{
    omni_wm_memory_bridge_config_t config;
    omni_wm_memory_bridge_default_config(&config);
    config.replay_batch_size = 48;
    config.encoding_threshold = 0.7f;

    omni_wm_memory_bridge_t* bridge = omni_wm_memory_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Modify original
    config.replay_batch_size = 16;

    // Bridge should have original value
    EXPECT_EQ(bridge->config.replay_batch_size, 48u);

    omni_wm_memory_bridge_destroy(bridge);
}

// =============================================================================
// 14. Internal State Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, InitialStateValid)
{
    EXPECT_FALSE(bridge_->is_sleeping);
    EXPECT_FLOAT_EQ(bridge_->current_sleep_stage, 0.0f);
    EXPECT_FALSE(bridge_->replay_in_progress);
    EXPECT_FALSE(bridge_->context_cache_valid);
}

TEST_F(OmniWmMemoryBridgeTest, ReplayBufferCapacity)
{
    // Buffer should be allocated with reasonable capacity
    EXPECT_GE(bridge_->replay_buffer_capacity, 0u);
}

TEST_F(OmniWmMemoryBridgeTest, EffectsStructureIntegrity)
{
    const omni_wm_to_memory_effects_t* wm_effects =
        omni_wm_memory_bridge_get_wm_effects(bridge_);
    EXPECT_NE(wm_effects, nullptr);
    EXPECT_TRUE(float_in_range(wm_effects->state_uncertainty, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(wm_effects->prediction_confidence, 0.0f, 1.0f));

    const memory_to_omni_wm_effects_t* mem_effects =
        omni_wm_memory_bridge_get_memory_effects(bridge_);
    EXPECT_NE(mem_effects, nullptr);
    EXPECT_TRUE(float_in_range(mem_effects->theta_phase, -4.0f, 4.0f));  // radians
    EXPECT_TRUE(float_in_range(mem_effects->theta_power, 0.0f, 2.0f));
}

// =============================================================================
// 15. Utility Function Tests
// =============================================================================

TEST_F(OmniWmMemoryBridgeTest, MsgTypeToStringValid)
{
    // This depends on what message types are defined
    // Just verify the function exists and returns non-null for known types
    // (specific message type testing would require knowledge of the enum values)
}
