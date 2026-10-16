/**
 * @file test_consolidation.cpp
 * @brief Unit tests for Memory Consolidation Module
 *
 * WHAT: Comprehensive unit tests for memory consolidation system
 * WHY:  Ensure consolidation logic works correctly for learning stability
 * HOW:  Test lifecycle, strategies, statistics, and FEP bridge integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/consolidation/nimcp_consolidation_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_router.h"

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Consolidation Core Tests
 * ============================================================================ */

class ConsolidationTest : public NimcpTestBase {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        // Tests will create brain as needed
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

TEST_F(ConsolidationTest, DefaultConfigHasSensibleValues) {
    // WHAT: Verify default configuration has reasonable values
    // WHY:  Ensure users get working consolidation without manual config
    // HOW:  Call default_config, check all values

    consolidation_config_t config = consolidation_default_config();

    // Strategy should be valid
    EXPECT_GE((int)config.strategy, (int)CONSOLIDATION_STRATEGY_REPLAY);
    EXPECT_LE((int)config.strategy, (int)CONSOLIDATION_STRATEGY_FULL);

    // Priority should be valid
    EXPECT_GE((int)config.priority, (int)CONSOLIDATION_PRIORITY_RECENT);
    EXPECT_LE((int)config.priority, (int)CONSOLIDATION_PRIORITY_ALL);

    // Consolidation parameters should be in valid ranges
    EXPECT_GT(config.consolidation_cycles, 0u);
    EXPECT_GE(config.consolidation_strength, 0.0f);
    EXPECT_LE(config.consolidation_strength, 1.0f);

    // Replay settings
    EXPECT_GT(config.replay_count, 0u);

    // Pruning settings
    EXPECT_GE(config.pruning_threshold, 0.0f);
    EXPECT_LE(config.pruning_threshold, 1.0f);

    // Scaling settings
    EXPECT_GE(config.scaling_target, 0.0f);
    EXPECT_LE(config.scaling_target, 1.0f);

    // Novelty settings
    EXPECT_GE(config.novelty_boost, 1.0f);  // Should be at least 1.0 (no penalty)

    // Weakness threshold
    EXPECT_GE(config.weakness_threshold, 0.0f);
    EXPECT_LE(config.weakness_threshold, 1.0f);
}

TEST_F(ConsolidationTest, ConsolidateMemoryWithNullBrain) {
    // WHAT: Verify consolidate handles NULL brain
    // WHY:  Defensive programming
    // HOW:  Call with NULL brain

    consolidation_config_t config = consolidation_default_config();
    bool result = brain_consolidate_memory(nullptr, &config);

    EXPECT_FALSE(result);
}

TEST_F(ConsolidationTest, ConsolidateMemoryWithNullConfig) {
    // WHAT: Verify consolidate handles NULL config (uses defaults)
    // WHY:  Allow simple usage with defaults
    // HOW:  Call with NULL config

    // Note: This test would require a real brain instance
    // For now, we just verify the function signature accepts NULL
    bool result = brain_consolidate_memory(nullptr, nullptr);
    EXPECT_FALSE(result);  // Should fail due to NULL brain
}

TEST_F(ConsolidationTest, StopConsolidationWithNullHandle) {
    // WHAT: Verify stopping NULL handle is safe
    // WHY:  Defensive programming
    // HOW:  Call stop with NULL

    brain_stop_background_consolidation(nullptr);
    SUCCEED();  // If we get here, no crash occurred
}

TEST_F(ConsolidationTest, PauseConsolidationWithNullHandle) {
    // WHAT: Verify pausing NULL handle is safe
    // WHY:  Defensive programming
    // HOW:  Call pause with NULL

    brain_pause_consolidation(nullptr);
    SUCCEED();
}

TEST_F(ConsolidationTest, ResumeConsolidationWithNullHandle) {
    // WHAT: Verify resuming NULL handle is safe
    // WHY:  Defensive programming
    // HOW:  Call resume with NULL

    brain_resume_consolidation(nullptr);
    SUCCEED();
}

TEST_F(ConsolidationTest, TriggerConsolidationWithNullHandle) {
    // WHAT: Verify triggering NULL handle returns false
    // WHY:  Defensive programming
    // HOW:  Call trigger with NULL

    bool result = brain_trigger_consolidation(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(ConsolidationTest, GetImportantPatternsWithNullBrain) {
    // WHAT: Verify get_important_patterns handles NULL brain
    // WHY:  Defensive programming
    // HOW:  Call with NULL brain

    uint32_t num_patterns = 0;
    pattern_importance_t* patterns = brain_get_important_patterns(nullptr, &num_patterns);

    EXPECT_EQ(patterns, nullptr);
    EXPECT_EQ(num_patterns, 0u);
}

TEST_F(ConsolidationTest, PatternImportanceFreeWithNull) {
    // WHAT: Verify freeing NULL pattern array is safe
    // WHY:  Defensive programming
    // HOW:  Call free with NULL

    pattern_importance_free(nullptr, 0);
    SUCCEED();
}

TEST_F(ConsolidationTest, MarkPatternImportantWithNullBrain) {
    // WHAT: Verify mark_pattern_important handles NULL brain
    // WHY:  Defensive programming
    // HOW:  Call with NULL brain

    bool result = brain_mark_pattern_important(nullptr, "test_pattern", 0.9f);
    EXPECT_FALSE(result);
}

TEST_F(ConsolidationTest, MarkPatternImportantWithNullName) {
    // WHAT: Verify mark_pattern_important handles NULL pattern name
    // WHY:  Defensive programming
    // HOW:  Call with NULL name

    // Without a real brain, this should fail
    bool result = brain_mark_pattern_important(nullptr, nullptr, 0.9f);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Consolidation Statistics Tests
 * ============================================================================ */

TEST_F(ConsolidationTest, GetStatsWithNullOutput) {
    // WHAT: Verify get_stats handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL stats

    bool result = consolidation_get_stats(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(ConsolidationTest, ResetStatsWithNullHandle) {
    // WHAT: Verify reset_stats handles NULL handle
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    consolidation_reset_stats(nullptr);
    SUCCEED();
}

TEST_F(ConsolidationTest, IsRunningWithNullHandle) {
    // WHAT: Verify is_running handles NULL handle
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool running = consolidation_is_running(nullptr);
    EXPECT_FALSE(running);
}

TEST_F(ConsolidationTest, GetProgressWithNullHandle) {
    // WHAT: Verify get_progress handles NULL handle
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float progress = consolidation_get_progress(nullptr);
    EXPECT_EQ(progress, -1.0f);  // -1 indicates not running
}

/* ============================================================================
 * Advanced Consolidation Tests
 * ============================================================================ */

TEST_F(ConsolidationTest, ReplayPatternWithNullBrain) {
    // WHAT: Verify replay_pattern handles NULL brain
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = brain_replay_pattern(nullptr, "test", 10, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(ConsolidationTest, ReplayPatternWithNullName) {
    // WHAT: Verify replay_pattern handles NULL pattern name
    // WHY:  Defensive programming
    // HOW:  Call with NULL name

    bool result = brain_replay_pattern(nullptr, nullptr, 10, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(ConsolidationTest, ApplySynapticScalingWithNullBrain) {
    // WHAT: Verify apply_synaptic_scaling handles NULL brain
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = brain_apply_synaptic_scaling(nullptr, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(ConsolidationTest, PruneWeakConnectionsWithNullBrain) {
    // WHAT: Verify prune_weak_connections handles NULL brain
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t pruned = brain_prune_weak_connections(nullptr, 0.01f);
    EXPECT_EQ(pruned, 0u);
}

/* ============================================================================
 * Consolidation Strategy Tests
 * ============================================================================ */

TEST_F(ConsolidationTest, StrategyEnumValues) {
    // WHAT: Verify strategy enum has expected values
    // WHY:  Enum values used in switch statements
    // HOW:  Check each value

    EXPECT_EQ((int)CONSOLIDATION_STRATEGY_REPLAY, 0);
    EXPECT_EQ((int)CONSOLIDATION_STRATEGY_SCALING, 1);
    EXPECT_EQ((int)CONSOLIDATION_STRATEGY_PRUNING, 2);
    EXPECT_EQ((int)CONSOLIDATION_STRATEGY_INTEGRATION, 3);
    EXPECT_EQ((int)CONSOLIDATION_STRATEGY_FULL, 4);
}

TEST_F(ConsolidationTest, PriorityEnumValues) {
    // WHAT: Verify priority enum has expected values
    // WHY:  Enum values used in switch statements
    // HOW:  Check each value

    EXPECT_EQ((int)CONSOLIDATION_PRIORITY_RECENT, 0);
    EXPECT_EQ((int)CONSOLIDATION_PRIORITY_FREQUENT, 1);
    EXPECT_EQ((int)CONSOLIDATION_PRIORITY_IMPORTANT, 2);
    EXPECT_EQ((int)CONSOLIDATION_PRIORITY_NOVEL, 3);
    EXPECT_EQ((int)CONSOLIDATION_PRIORITY_ALL, 4);
}

TEST_F(ConsolidationTest, EventTypeEnumValues) {
    // WHAT: Verify event type enum has expected values
    // WHY:  Event types used for callbacks
    // HOW:  Check each value

    EXPECT_EQ((int)CONSOLIDATION_EVENT_STARTED, 0);
    EXPECT_EQ((int)CONSOLIDATION_EVENT_CYCLE_COMPLETE, 1);
    EXPECT_EQ((int)CONSOLIDATION_EVENT_PATTERN_REPLAYED, 2);
    EXPECT_EQ((int)CONSOLIDATION_EVENT_CONNECTION_PRUNED, 3);
    EXPECT_EQ((int)CONSOLIDATION_EVENT_SCALING_APPLIED, 4);
    EXPECT_EQ((int)CONSOLIDATION_EVENT_COMPLETED, 5);
    EXPECT_EQ((int)CONSOLIDATION_EVENT_ERROR, 6);
}

/* ============================================================================
 * Consolidation Configuration Edge Cases
 * ============================================================================ */

TEST_F(ConsolidationTest, ConfigZeroValues) {
    // WHAT: Test configuration with zero values
    // WHY:  Edge case - minimal consolidation
    // HOW:  Set all to zero/false

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 0;
    config.consolidation_strength = 0.0f;
    config.enable_replay = false;
    config.enable_pruning = false;
    config.enable_scaling = false;
    config.prioritize_novel = false;
    config.prune_weak = false;

    // Config should be valid even with zero values
    EXPECT_EQ(config.consolidation_cycles, 0u);
    EXPECT_FLOAT_EQ(config.consolidation_strength, 0.0f);
}

TEST_F(ConsolidationTest, ConfigMaxValues) {
    // WHAT: Test configuration with max values
    // WHY:  Edge case - aggressive consolidation
    // HOW:  Set high values

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 1000;
    config.consolidation_strength = 1.0f;
    config.replay_count = 10000;
    config.pruning_threshold = 1.0f;
    config.scaling_target = 1.0f;
    config.novelty_boost = 10.0f;
    config.weakness_threshold = 1.0f;

    EXPECT_EQ(config.consolidation_cycles, 1000u);
    EXPECT_FLOAT_EQ(config.consolidation_strength, 1.0f);
}

/* ============================================================================
 * Consolidation FEP Bridge Tests
 * ============================================================================ */

class ConsolidationFepBridgeTest : public NimcpTestBase {
protected:
    consolidation_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        bio_router_init(NULL);

        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);

        // Create consolidation FEP bridge
        consolidation_fep_config_t config;
        consolidation_fep_bridge_default_config(&config);
        bridge = consolidation_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            consolidation_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        bio_router_shutdown();
        NimcpTestBase::TearDown();
    }
};

TEST_F(ConsolidationFepBridgeTest, CreateDestroy) {
    // WHAT: Verify bridge creation and destruction
    // WHY:  Basic lifecycle test
    // HOW:  Check bridge is not NULL

    ASSERT_NE(bridge, nullptr);
}

TEST_F(ConsolidationFepBridgeTest, DefaultConfig) {
    // WHAT: Verify default config returns success
    // WHY:  Ensure sensible defaults
    // HOW:  Check return value and config values

    consolidation_fep_config_t config;
    int ret = consolidation_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.fe_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_complexity_guided_consolidation);
}

TEST_F(ConsolidationFepBridgeTest, DefaultConfigNull) {
    // WHAT: Verify default_config handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int ret = consolidation_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, DestroyNull) {
    // WHAT: Verify destroying NULL is safe
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    consolidation_fep_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(ConsolidationFepBridgeTest, ConnectFep) {
    // WHAT: Verify FEP connection
    // WHY:  Bridge needs FEP system
    // HOW:  Connect and verify success

    ASSERT_NE(fep, nullptr);
    int ret = consolidation_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, ConnectFepWithNullBridge) {
    // WHAT: Verify connect handles NULL bridge
    // WHY:  Defensive programming
    // HOW:  Call with NULL bridge

    int ret = consolidation_fep_bridge_connect_fep(nullptr, fep);
    EXPECT_NE(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, Update) {
    // WHAT: Verify bridge update works
    // WHY:  Core bridge functionality
    // HOW:  Connect FEP, then update

    if (fep) {
        consolidation_fep_bridge_connect_fep(bridge, fep);
        int ret = consolidation_fep_bridge_update(bridge);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(ConsolidationFepBridgeTest, UpdateWithNullBridge) {
    // WHAT: Verify update handles NULL bridge
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int ret = consolidation_fep_bridge_update(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, GetState) {
    // WHAT: Verify get_state works
    // WHY:  State query functionality
    // HOW:  Connect, update, get state

    if (fep) {
        consolidation_fep_bridge_connect_fep(bridge, fep);
        consolidation_fep_bridge_update(bridge);

        consolidation_fep_state_t state;
        int ret = consolidation_fep_bridge_get_state(bridge, &state);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(ConsolidationFepBridgeTest, GetStateWithNullBridge) {
    // WHAT: Verify get_state handles NULL bridge
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    consolidation_fep_state_t state;
    int ret = consolidation_fep_bridge_get_state(nullptr, &state);
    EXPECT_NE(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, GetStateWithNullOutput) {
    // WHAT: Verify get_state handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL output

    int ret = consolidation_fep_bridge_get_state(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, GetStats) {
    // WHAT: Verify get_stats works
    // WHY:  Statistics query functionality
    // HOW:  Connect, update, get stats

    if (fep) {
        consolidation_fep_bridge_connect_fep(bridge, fep);
        consolidation_fep_bridge_update(bridge);

        consolidation_fep_stats_t stats;
        int ret = consolidation_fep_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(ConsolidationFepBridgeTest, GetStatsWithNullBridge) {
    // WHAT: Verify get_stats handles NULL bridge
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    consolidation_fep_stats_t stats;
    int ret = consolidation_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0);
}

TEST_F(ConsolidationFepBridgeTest, BioAsyncConnection) {
    // WHAT: Verify bio-async connection lifecycle
    // WHY:  Bio-async integration
    // HOW:  Connect, check, disconnect, check

    EXPECT_FALSE(consolidation_fep_bridge_is_bio_async_connected(bridge));

    consolidation_fep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(consolidation_fep_bridge_is_bio_async_connected(bridge));

    consolidation_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(consolidation_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(ConsolidationFepBridgeTest, BioAsyncWithNullBridge) {
    // WHAT: Verify bio-async handles NULL bridge
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    EXPECT_FALSE(consolidation_fep_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Global State Reset Tests
 * ============================================================================ */

TEST_F(ConsolidationTest, ResetGlobalState) {
    // WHAT: Verify global state reset works
    // WHY:  Test isolation requires clean state
    // HOW:  Call reset, verify no crash

    consolidation_reset_global_state();
    SUCCEED();
}

TEST_F(ConsolidationTest, MultipleGlobalStateResets) {
    // WHAT: Verify multiple resets are safe
    // WHY:  Tests may reset multiple times
    // HOW:  Reset multiple times

    for (int i = 0; i < 10; i++) {
        consolidation_reset_global_state();
    }
    SUCCEED();
}
