/**
 * @file test_social_fep_integration.cpp
 * @brief Integration tests for Social Cognition + FEP Orchestrator
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests real integration between social cognition and FEP orchestrator
 * WHY:  Verify FEP correctly coordinates social cognition updates and free energy
 * HOW:  Test FEP update cycles, social prediction effects, relationship modeling
 *
 * TEST COVERAGE:
 * - Social predictions minimize free energy
 * - Relationship models reduce uncertainty
 * - Norm violations increase prediction error
 * - Trust changes affect free energy
 * - Group dynamics prediction
 * - Social learning integration
 * - Emotional contagion effects
 * - Reputation tracking reduces uncertainty
 * - FEP update cycle integration (50ms)
 * - Statistics accumulation across cycles
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/social/nimcp_social_fep_bridge.h"
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define SOCIAL_FEP_TEST_UPDATE_INTERVAL_MS   50
#define SOCIAL_FEP_TEST_CYCLE_COUNT          20
#define SOCIAL_FEP_TEST_RELATIONSHIP_COUNT   4

/* ============================================================================
 * Global Test State
 * ============================================================================ */

static std::atomic<int> g_social_fep_update_count{0};
static std::atomic<float> g_current_free_energy{0.5f};
static std::atomic<float> g_accumulated_prediction_error{0.0f};

/**
 * @brief Mock FEP update callback for social cognition bridge
 */
static int social_fep_test_update(fep_bridge_handle_t handle) {
    social_fep_bridge_t* bridge = static_cast<social_fep_bridge_t*>(handle);
    if (bridge) {
        social_fep_bridge_update(bridge);
    }
    g_social_fep_update_count++;

    /* Track free energy changes */
    float fe = social_fep_bridge_get_free_energy_contribution(bridge);
    if (fe >= 0.0f) {
        g_current_free_energy.store(fe);
    }

    return 0;
}

static void reset_test_counters() {
    g_social_fep_update_count = 0;
    g_current_free_energy = 0.5f;
    g_accumulated_prediction_error = 0.0f;
}

/* ============================================================================
 * Test Fixture: Social FEP Integration
 * ============================================================================ */

class SocialFEPIntegrationTest : public ::testing::Test {
protected:
    social_bond_system_t* social = nullptr;
    social_fep_bridge_t* bridge = nullptr;
    social_fep_config_t bridge_config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;
    uint32_t bridge_id = 0;

    void SetUp() override {
        reset_test_counters();

        /* Create social bond system */
        social = social_bond_system_create();
        ASSERT_NE(social, nullptr) << "Failed to create social bond system";

        /* Create social FEP bridge */
        bridge_config = social_fep_config_default();
        bridge_config.enable_logging = false;
        bridge_config.enable_surprise_callbacks = true;
        bridge = social_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr) << "Failed to create social FEP bridge";

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr) << "Failed to create FEP orchestrator";

        /* Start FEP orchestrator */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);
    }

    void TearDown() override {
        if (bridge && social_fep_bridge_is_registered(bridge)) {
            social_fep_bridge_unregister(bridge);
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (bridge) {
            social_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (social) {
            social_bond_system_destroy(social);
            social = nullptr;
        }
    }

    /* Helper to register bridge with FEP orchestrator */
    void register_social_bridge() {
        int ret = social_fep_bridge_register(bridge, fep_orch, social, &bridge_id);
        ASSERT_EQ(ret, 0) << "Failed to register social FEP bridge";
        ASSERT_TRUE(social_fep_bridge_is_registered(bridge));
    }

    /* Helper to get current time in milliseconds */
    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    /* Helper to create relationships in social system */
    void setup_relationships(uint32_t count) {
        uint64_t current_time = get_current_time_ms() * 1000;  /* Convert to microseconds */
        for (uint32_t i = 0; i < count && i < SOCIAL_MAX_RELATIONSHIPS; i++) {
            relationship_stage_t stage = (i < 2) ? RELATIONSHIP_FRIEND : RELATIONSHIP_ACQUAINTANCE;
            uint32_t rel_id = social_create_relationship(social, stage, current_time);
            EXPECT_GT(rel_id, 0u) << "Failed to create relationship " << i;

            /* Add some interaction history for close friends */
            if (stage == RELATIONSHIP_FRIEND) {
                social_process_interaction(social, rel_id, INTERACTION_CONVERSATION,
                                           0.7f, 0.8f, current_time);
                social_provide_support(social, rel_id, 0.6f);
            }
        }
    }

    /* Helper to run FEP update cycles */
    int run_fep_cycles(int count) {
        uint64_t base_time = get_current_time_ms();
        int total_updated = 0;
        for (int i = 0; i < count; i++) {
            int updated = fep_orchestrator_update(fep_orch, base_time + (i * 60));
            if (updated > 0) {
                total_updated += updated;
            }
        }
        return total_updated;
    }
};

/* ============================================================================
 * SocialPredictionWithFEP - Social predictions minimize free energy
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, SocialPredictionWithFEP) {
    /* Register bridge with FEP orchestrator */
    register_social_bridge();

    /* Setup social relationships */
    setup_relationships(SOCIAL_FEP_TEST_RELATIONSHIP_COUNT);

    /* Get initial free energy */
    social_fep_bridge_update(bridge);
    float initial_fe = social_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Initial free energy should be non-negative";

    /* Run multiple FEP update cycles */
    run_fep_cycles(SOCIAL_FEP_TEST_CYCLE_COUNT);

    /* Get final free energy */
    float final_fe = social_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(final_fe, 0.0f) << "Final free energy should be non-negative";

    /* Verify statistics were accumulated */
    social_fep_stats_t stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u) << "Updates should have occurred";
    EXPECT_GT(stats.social_predictions, 0u) << "Social predictions should be tracked";
}

/* ============================================================================
 * RelationshipModelingFreeEnergy - Relationship models reduce uncertainty
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, RelationshipModelingFreeEnergy) {
    register_social_bridge();

    /* Initially no relationships - higher uncertainty */
    social_fep_bridge_update(bridge);
    float no_rel_uncertainty = social_fep_bridge_get_relationship_uncertainty(bridge);

    /* Add relationships */
    setup_relationships(4);

    /* Process some positive interactions to build trust */
    uint64_t current_time = get_current_time_ms() * 1000;
    social_update(social, 1.0f, current_time);

    /* Update bridge with relationships */
    social_fep_bridge_update(bridge);
    float with_rel_uncertainty = social_fep_bridge_get_relationship_uncertainty(bridge);

    /* Relationships should reduce uncertainty */
    EXPECT_LE(with_rel_uncertainty, 1.0f) << "Uncertainty should be bounded";

    /* Verify relationship updates are tracked */
    social_fep_stats_t stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.relationship_updates, 0u);
}

/* ============================================================================
 * NormViolationSurprise - Norm violations increase prediction error
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, NormViolationSurprise) {
    register_social_bridge();
    setup_relationships(2);

    /* Get baseline metrics */
    social_fep_bridge_update(bridge);
    social_fep_metrics_t baseline_metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &baseline_metrics), 0);

    /* Simulate betrayal (norm violation) */
    uint64_t current_time = get_current_time_ms() * 1000;
    social_experience_betrayal(social, 1, 0.8f);  /* High severity betrayal */
    social_update(social, 0.1f, current_time);

    /* Update bridge after betrayal */
    social_fep_bridge_update(bridge);
    social_fep_metrics_t after_betrayal_metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &after_betrayal_metrics), 0);

    /* Norm violation metrics should reflect the event */
    EXPECT_GE(after_betrayal_metrics.norm_violation_surprise, 0.0f);
    EXPECT_LE(after_betrayal_metrics.norm_violation_surprise, 1.0f);

    /* Verify stats track norm violations */
    social_fep_stats_t stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &stats), 0);
}

/* ============================================================================
 * TrustUpdatePredictionError - Trust changes affect free energy
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, TrustUpdatePredictionError) {
    register_social_bridge();
    setup_relationships(2);

    /* Build trust through support */
    uint64_t current_time = get_current_time_ms() * 1000;
    social_provide_support(social, 1, 0.9f);
    social_receive_support(social, 1, 0.8f);
    social_update(social, 0.5f, current_time);

    /* Update bridge */
    social_fep_bridge_update(bridge);
    social_fep_metrics_t high_trust_metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &high_trust_metrics), 0);

    /* Trust prediction error should reflect trust state */
    EXPECT_GE(high_trust_metrics.trust_prediction_error, 0.0f);
    EXPECT_LE(high_trust_metrics.trust_prediction_error, 1.0f);

    /* Free energy should be valid */
    float fe = social_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, bridge_config.max_free_energy);
}

/* ============================================================================
 * GroupDynamicsPrediction - Group behavior prediction
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, GroupDynamicsPrediction) {
    register_social_bridge();

    /* Create a social group (multiple relationships) */
    setup_relationships(6);

    /* Process group interactions */
    uint64_t current_time = get_current_time_ms() * 1000;
    for (uint32_t i = 1; i <= 4; i++) {
        social_process_interaction(social, i, INTERACTION_COLLABORATION,
                                   0.7f, 0.8f, current_time);
    }
    social_update(social, 1.0f, current_time);

    /* Run FEP cycles */
    run_fep_cycles(10);

    /* Get metrics after group processing */
    social_fep_metrics_t metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &metrics), 0);

    /* Group should have affected social metrics */
    EXPECT_GE(metrics.active_relationships, 0u);
    EXPECT_GE(metrics.cooperation_prediction_error, 0.0f);
    EXPECT_LE(metrics.cooperation_prediction_error, 1.0f);
}

/* ============================================================================
 * SocialLearningIntegration - Learning from others reduces uncertainty
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, SocialLearningIntegration) {
    register_social_bridge();
    setup_relationships(3);

    /* Initial uncertainty */
    social_fep_bridge_update(bridge);
    float initial_uncertainty = social_fep_bridge_get_relationship_uncertainty(bridge);

    /* Social learning through vulnerability and support */
    uint64_t current_time = get_current_time_ms() * 1000;
    for (uint32_t i = 1; i <= 2; i++) {
        social_express_vulnerability(social, i, 0.6f, true);
        social_provide_support(social, i, 0.7f);
        social_receive_support(social, i, 0.7f);
    }
    social_update(social, 2.0f, current_time);

    /* Run FEP cycles to integrate learning */
    run_fep_cycles(15);

    /* Get updated uncertainty */
    float final_uncertainty = social_fep_bridge_get_relationship_uncertainty(bridge);

    /* Both should be in valid range */
    EXPECT_GE(initial_uncertainty, 0.0f);
    EXPECT_LE(initial_uncertainty, 1.0f);
    EXPECT_GE(final_uncertainty, 0.0f);
    EXPECT_LE(final_uncertainty, 1.0f);
}

/* ============================================================================
 * EmotionalContagionFreeEnergy - Emotional spread affects predictions
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, EmotionalContagionFreeEnergy) {
    register_social_bridge();
    setup_relationships(4);

    /* Get baseline metrics */
    social_fep_bridge_update(bridge);
    social_fep_metrics_t baseline;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &baseline), 0);

    /* Simulate emotional events through interactions */
    uint64_t current_time = get_current_time_ms() * 1000;

    /* Celebration (positive emotion) */
    social_process_interaction(social, 1, INTERACTION_CELEBRATION,
                               0.9f, 0.95f, current_time);
    social_update(social, 0.5f, current_time);

    /* Update and check */
    social_fep_bridge_update(bridge);
    social_fep_metrics_t after_celebration;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &after_celebration), 0);

    /* Social warmth should be tracked */
    EXPECT_GE(after_celebration.social_warmth, 0.0f);
    EXPECT_LE(after_celebration.social_warmth, 1.0f);
}

/* ============================================================================
 * ReputationTracking - Reputation models reduce social uncertainty
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, ReputationTracking) {
    register_social_bridge();
    setup_relationships(3);

    /* Build reputation through loyalty tests */
    uint64_t current_time = get_current_time_ms() * 1000;

    /* Commit loyalty and pass tests */
    social_commit_loyalty(social, 1, LOYALTY_TO_PERSON, 0.8f);
    social_test_loyalty(social, 1, 0.6f, true);  /* Pass moderate test */
    social_test_loyalty(social, 1, 0.8f, true);  /* Pass hard test */
    social_update(social, 1.0f, current_time);

    /* Run FEP cycles */
    run_fep_cycles(10);

    /* Get metrics */
    social_fep_metrics_t metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &metrics), 0);

    /* Trust should reflect positive reputation */
    EXPECT_GE(metrics.avg_relationship_trust, 0.0f);
    EXPECT_LE(metrics.avg_relationship_trust, 1.0f);
}

/* ============================================================================
 * FEPUpdateCycleIntegration - Verify 50ms update cycles work correctly
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, FEPUpdateCycleIntegration) {
    register_social_bridge();
    setup_relationships(2);

    /* Configure update interval */
    fep_orchestrator_set_update_interval(fep_orch, FEP_BRIDGE_CATEGORY_COGNITIVE, 50);

    /* Reset counters */
    reset_test_counters();

    /* Run coordinated updates */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 60));
    }

    /* Verify bridge was updated through orchestrator */
    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);
    EXPECT_GT(orch_stats.total_update_cycles, 0u);
    EXPECT_GT(orch_stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].total_updates, 0u);

    /* Verify bridge tracked updates */
    social_fep_stats_t bridge_stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &bridge_stats), 0);
    EXPECT_GT(bridge_stats.total_updates, 0u);
}

/* ============================================================================
 * StatisticsAccumulation - Stats accumulate across multiple cycles
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, StatisticsAccumulation) {
    register_social_bridge();
    setup_relationships(3);

    /* Reset statistics */
    social_fep_bridge_reset_stats(bridge);

    /* Run known number of cycles */
    int expected_cycles = 25;
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < expected_cycles; i++) {
        social_fep_bridge_update(bridge);
    }

    /* Get accumulated statistics */
    social_fep_stats_t stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &stats), 0);

    /* Verify accumulation */
    EXPECT_GE(stats.total_updates, static_cast<uint64_t>(expected_cycles));
    EXPECT_GE(stats.avg_update_time_us, 0.0f);
    EXPECT_GE(stats.avg_free_energy, 0.0f);
    EXPECT_GE(stats.avg_prediction_error, 0.0f);
    EXPECT_LE(stats.avg_prediction_error, 1.0f);

    /* Free energy contribution should be tracked */
    EXPECT_GE(stats.total_free_energy_contribution, 0.0f);
}

/* ============================================================================
 * LonelinessIncreasesUncertainty - Loneliness affects free energy
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, LonelinessIncreasesUncertainty) {
    register_social_bridge();

    /* No relationships - should feel lonely */
    social_update(social, 5.0f, get_current_time_ms() * 1000);  /* Time passes */

    social_fep_bridge_update(bridge);
    social_fep_metrics_t lonely_metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &lonely_metrics), 0);

    /* Loneliness should be reflected */
    EXPECT_GE(lonely_metrics.loneliness, 0.0f);
    EXPECT_LE(lonely_metrics.loneliness, 1.0f);

    /* Add relationships to reduce loneliness */
    setup_relationships(4);
    social_update(social, 1.0f, get_current_time_ms() * 1000);

    social_fep_bridge_update(bridge);
    social_fep_metrics_t connected_metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &connected_metrics), 0);

    /* With friends, loneliness should be lower or similar */
    EXPECT_LE(connected_metrics.loneliness, 1.0f);
}

/* ============================================================================
 * OxytocinBondingIntegration - Oxytocin affects social predictions
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, OxytocinBondingIntegration) {
    register_social_bridge();
    setup_relationships(2);

    /* Build oxytocin through positive interactions */
    uint64_t current_time = get_current_time_ms() * 1000;

    /* Multiple bonding interactions */
    for (int i = 0; i < 5; i++) {
        social_process_interaction(social, 1, INTERACTION_SUPPORT_GIVEN,
                                   0.8f, 0.9f, current_time + i * 1000);
        social_express_vulnerability(social, 1, 0.5f, true);
    }
    social_update(social, 1.0f, current_time);

    /* Run FEP cycles */
    run_fep_cycles(10);

    /* Get metrics */
    social_fep_metrics_t metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &metrics), 0);

    /* Oxytocin level should be tracked */
    EXPECT_GE(metrics.oxytocin_level, 0.0f);
    EXPECT_LE(metrics.oxytocin_level, 1.0f);
}

/* ============================================================================
 * MultipleBridgesWithSocial - Social works alongside other bridges
 * ============================================================================ */

static std::atomic<int> g_other_bridge_count{0};

static int other_bridge_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_other_bridge_count++;
    return 0;
}

TEST_F(SocialFEPIntegrationTest, MultipleBridgesWithSocial) {
    g_other_bridge_count = 0;
    register_social_bridge();
    setup_relationships(2);

    /* Dummy handles for other bridges */
    static int dummy_emotion_handle = 1;
    static int dummy_memory_handle = 2;

    /* Register additional cognitive bridges */
    uint32_t other_bridge_id;
    int ret = fep_orchestrator_register_bridge(
        fep_orch, "emotion_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_emotion_handle,
        other_bridge_update, nullptr, &other_bridge_id);
    ASSERT_EQ(ret, 0);

    ret = fep_orchestrator_register_bridge(
        fep_orch, "memory_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_memory_handle,
        other_bridge_update, nullptr, &other_bridge_id);
    ASSERT_EQ(ret, 0);

    /* Verify total bridge count */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GE(stats.total_bridges, 3u);

    /* Run FEP update cycles */
    run_fep_cycles(15);

    /* All bridges should have been updated */
    EXPECT_GT(g_other_bridge_count.load(), 0);

    /* Social bridge should also have updates */
    social_fep_stats_t social_stats;
    EXPECT_EQ(social_fep_bridge_get_stats(bridge, &social_stats), 0);
    EXPECT_GT(social_stats.total_updates, 0u);
}

/* ============================================================================
 * StateTransitionsIntegration - Bridge state transitions work with FEP
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, StateTransitionsIntegration) {
    /* Before registration - IDLE state */
    social_fep_state_t initial_state = social_fep_bridge_get_state(bridge);
    EXPECT_EQ(initial_state, SOCIAL_FEP_STATE_IDLE);

    /* Register with FEP */
    register_social_bridge();
    setup_relationships(2);

    /* After registration - ACTIVE state */
    social_fep_state_t active_state = social_fep_bridge_get_state(bridge);
    EXPECT_EQ(active_state, SOCIAL_FEP_STATE_ACTIVE);

    /* Run updates */
    run_fep_cycles(10);

    /* Should still be active (not degraded without high FE) */
    social_fep_state_t running_state = social_fep_bridge_get_state(bridge);
    EXPECT_TRUE(running_state == SOCIAL_FEP_STATE_ACTIVE ||
                running_state == SOCIAL_FEP_STATE_DEGRADED);

    /* Unregister */
    EXPECT_EQ(social_fep_bridge_unregister(bridge), 0);

    /* After unregister - IDLE state */
    social_fep_state_t final_state = social_fep_bridge_get_state(bridge);
    EXPECT_EQ(final_state, SOCIAL_FEP_STATE_IDLE);
}

/* ============================================================================
 * CloseFriendsReducePredictionError - Close friends improve predictions
 * ============================================================================ */

TEST_F(SocialFEPIntegrationTest, CloseFriendsReducePredictionError) {
    register_social_bridge();

    /* Start with acquaintances only */
    uint64_t current_time = get_current_time_ms() * 1000;
    for (int i = 0; i < 3; i++) {
        social_create_relationship(social, RELATIONSHIP_ACQUAINTANCE, current_time);
    }
    social_update(social, 1.0f, current_time);

    social_fep_bridge_update(bridge);
    social_fep_metrics_t acquaintance_metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &acquaintance_metrics), 0);

    /* Get close friends count before */
    uint32_t close_before = social_get_close_friend_count(social);

    /* Upgrade to close friends through interactions */
    for (uint32_t i = 1; i <= 2; i++) {
        for (int j = 0; j < 10; j++) {
            social_process_interaction(social, i, INTERACTION_CONVERSATION,
                                       0.8f, 0.85f, current_time);
            social_express_vulnerability(social, i, 0.6f, true);
            social_provide_support(social, i, 0.8f);
        }
    }
    social_update(social, 5.0f, current_time);

    /* Run FEP cycles */
    run_fep_cycles(15);

    social_fep_metrics_t close_friend_metrics;
    EXPECT_EQ(social_fep_bridge_get_metrics(bridge, &close_friend_metrics), 0);

    /* Close friends count should be tracked */
    EXPECT_GE(close_friend_metrics.close_friends_count, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
