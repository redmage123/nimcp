/**
 * @file test_curiosity_fep_bridge_integration.cpp
 * @brief Integration tests for curiosity-FEP bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Integration tests for bidirectional curiosity-FEP coupling
 * WHY:  Verify that epistemic value computation and exploration work in realistic scenarios
 * HOW:  Test full update cycles, learning feedback loops, and bio-async messaging
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/curiosity/nimcp_curiosity_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "core/brain/nimcp_brain.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CuriosityFEPIntegrationTest : public ::testing::Test {
protected:
    curiosity_fep_bridge_t* bridge;
    fep_system_t* fep;
    brain_t brain;
    curiosity_engine_t curiosity;

    void SetUp() override {
        bridge = nullptr;
        fep = nullptr;
        brain = nullptr;
        curiosity = nullptr;

        /* Create bridge */
        curiosity_fep_config_t bridge_config;
        curiosity_fep_bridge_default_config(&bridge_config);
        bridge = curiosity_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_config.num_levels = 3;
        fep = fep_create(&fep_config, 8, 4);
        ASSERT_NE(fep, nullptr);

        /* Create brain and curiosity */
        brain = brain_create("integration_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 8, 4);
        ASSERT_NE(brain, nullptr);
        curiosity = curiosity_engine_create(brain, "integration_learner");
        ASSERT_NE(curiosity, nullptr);

        /* Connect components */
        curiosity_fep_bridge_connect_fep(bridge, fep);
        curiosity_fep_bridge_connect_curiosity(bridge, curiosity);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_fep_bridge_destroy(bridge);
        }
        if (fep) {
            fep_destroy(fep);
        }
        if (curiosity) {
            curiosity_engine_destroy(curiosity);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }
};

/* ============================================================================
 * Full System Integration Tests
 * ============================================================================ */

TEST_F(CuriosityFEPIntegrationTest, FullUpdateCycle) {
    /* Set prediction errors */
    fep->levels[0].errors.magnitude = 0.6f;
    fep->levels[1].errors.magnitude = 0.4f;
    fep->levels[2].errors.magnitude = 0.3f;

    /* Run update */
    int ret = curiosity_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);

    /* Verify epistemic value was computed */
    EXPECT_GT(bridge->state.current_epistemic_value, 0.0f);

    /* Verify stats were updated */
    EXPECT_GT(bridge->stats.total_epistemic_triggers, 0);
}

TEST_F(CuriosityFEPIntegrationTest, HighPredictionErrorTriggersExploration) {
    /* Set very high prediction errors */
    fep->levels[0].errors.magnitude = 1.5f;
    fep->levels[1].errors.magnitude = 1.2f;
    fep->levels[2].errors.magnitude = 1.0f;

    /* Update bridge */
    curiosity_fep_bridge_update(bridge, 100);

    /* Should have high epistemic value */
    EXPECT_GE(bridge->state.current_epistemic_value, 0.5f);

    /* Should have triggered exploration */
    EXPECT_GT(bridge->effects.exploration_motivation, 0.0f);
}

TEST_F(CuriosityFEPIntegrationTest, LowUncertaintyReducesExploration) {
    /* Set high precision (low uncertainty) */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        for (uint32_t i = 0; i < fep->levels[l].errors.dim; i++) {
            fep->levels[l].errors.precision[i] = 10.0f;
        }
    }

    /* Set low prediction errors */
    fep->levels[0].errors.magnitude = 0.1f;
    fep->levels[1].errors.magnitude = 0.05f;
    fep->levels[2].errors.magnitude = 0.02f;

    /* Update bridge */
    curiosity_fep_bridge_update(bridge, 100);

    /* Should have low epistemic value */
    EXPECT_LE(bridge->state.current_epistemic_value, 0.3f);

    /* Should have low exploration motivation */
    EXPECT_LE(bridge->effects.exploration_motivation, 0.5f);
}

TEST_F(CuriosityFEPIntegrationTest, KnowledgeGapDetectionFlow) {
    /* Set low precision to trigger knowledge gap */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        for (uint32_t i = 0; i < fep->levels[l].errors.dim; i++) {
            fep->levels[l].errors.precision[i] = 0.05f;
        }
    }

    /* Update bridge */
    curiosity_fep_bridge_update(bridge, 100);

    /* Should detect high uncertainty */
    EXPECT_GT(bridge->state.current_uncertainty, 0.0f);

    /* Should detect knowledge gap */
    EXPECT_GT(bridge->state.knowledge_gaps_detected, 0);
}

/* ============================================================================
 * Bidirectional Coupling Tests
 * ============================================================================ */

TEST_F(CuriosityFEPIntegrationTest, FEPToCuriosityFlow) {
    /* Start with moderate prediction error */
    fep->levels[0].errors.magnitude = 0.5f;
    fep->levels[1].errors.magnitude = 0.4f;
    fep->levels[2].errors.magnitude = 0.3f;

    /* Update bridge */
    curiosity_fep_bridge_update(bridge, 100);

    /* FEP prediction errors should increase epistemic value */
    float epistemic = bridge->state.current_epistemic_value;
    EXPECT_GT(epistemic, 0.0f);

    /* Epistemic value should trigger exploration in curiosity */
    EXPECT_GT(bridge->effects.curiosity_boost, 0.0f);
}

TEST_F(CuriosityFEPIntegrationTest, CuriosityToFEPFlow) {
    /* Set exploration rate in curiosity */
    curiosity_set_exploration_rate(curiosity, 0.8f);

    /* Simulate information gain */
    curiosity_set_baseline(curiosity, 0.9f);

    /* Update bridge (should propagate learning to FEP) */
    curiosity_fep_bridge_update(bridge, 100);

    /* Should track curiosity level */
    EXPECT_GT(bridge->state.current_curiosity_level, 0.0f);
}

/* ============================================================================
 * Multi-Step Learning Tests
 * ============================================================================ */

TEST_F(CuriosityFEPIntegrationTest, IterativeLearningCycle) {
    uint64_t initial_triggers = bridge->stats.total_epistemic_triggers;

    /* Run multiple update cycles with varying prediction errors */
    for (int i = 0; i < 10; i++) {
        /* Simulate varying prediction errors */
        float pe = 0.3f + 0.05f * sinf((float)i);
        fep->levels[0].errors.magnitude = pe;
        fep->levels[1].errors.magnitude = pe * 0.8f;
        fep->levels[2].errors.magnitude = pe * 0.6f;

        /* Update */
        curiosity_fep_bridge_update(bridge, 100);
    }

    /* Should have triggered epistemic computations */
    EXPECT_GT(bridge->stats.total_epistemic_triggers, initial_triggers);

    /* Should have accumulated average information gain */
    EXPECT_GT(bridge->stats.avg_information_gain, 0.0f);
}

TEST_F(CuriosityFEPIntegrationTest, ExplorationTriggersAccumulate) {
    uint64_t initial_explorations = bridge->stats.total_explorations;

    /* Set high epistemic value multiple times */
    for (int i = 0; i < 5; i++) {
        fep->levels[0].errors.magnitude = 0.8f;
        curiosity_fep_bridge_update(bridge, 100);
    }

    /* Should have triggered multiple explorations */
    EXPECT_GT(bridge->stats.total_explorations, initial_explorations);
}

/* ============================================================================
 * State Tracking Tests
 * ============================================================================ */

TEST_F(CuriosityFEPIntegrationTest, StateConsistency) {
    /* Set prediction errors */
    fep->levels[0].errors.magnitude = 0.5f;

    /* Update */
    curiosity_fep_bridge_update(bridge, 100);

    /* Get state */
    curiosity_fep_state_t state;
    int ret = curiosity_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);

    /* Verify state matches internal state */
    EXPECT_FLOAT_EQ(state.current_epistemic_value,
                    bridge->state.current_epistemic_value);
    EXPECT_EQ(state.knowledge_gaps_detected,
              bridge->state.knowledge_gaps_detected);
}

TEST_F(CuriosityFEPIntegrationTest, StatsAccumulation) {
    /* Run multiple updates */
    for (int i = 0; i < 3; i++) {
        fep->levels[0].errors.magnitude = 0.5f;
        curiosity_fep_bridge_update(bridge, 100);
    }

    /* Get stats */
    curiosity_fep_stats_t stats;
    int ret = curiosity_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    /* Should have accumulated stats */
    EXPECT_GE(stats.total_epistemic_triggers, 3);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(CuriosityFEPIntegrationTest, BioAsyncConnection) {
    int ret = curiosity_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    /* May or may not be connected depending on router */
    bool connected = curiosity_fep_bridge_is_bio_async_connected(bridge);
    (void)connected; /* Suppress unused warning */
}

TEST_F(CuriosityFEPIntegrationTest, BioAsyncDisconnection) {
    curiosity_fep_bridge_connect_bio_async(bridge);
    int ret = curiosity_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    bool connected = curiosity_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CuriosityFEPIntegrationTest, CustomConfigurationEffects) {
    /* Destroy default bridge */
    curiosity_fep_bridge_destroy(bridge);

    /* Create with custom config */
    curiosity_fep_config_t config;
    curiosity_fep_bridge_default_config(&config);
    config.epistemic_value_weight = 2.0f;
    config.exploration_boost = 0.5f;
    config.enable_knowledge_gap_detection = false;

    bridge = curiosity_fep_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    curiosity_fep_bridge_connect_fep(bridge, fep);
    curiosity_fep_bridge_connect_curiosity(bridge, curiosity);

    /* Set prediction error on all levels (FEP has 3 levels;
     * epistemic_value = avg_pe * weight, so need avg_pe > 0.25 for result > 0.5) */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        fep->levels[l].errors.magnitude = 0.5f;
    }

    /* Update */
    curiosity_fep_bridge_update(bridge, 100);

    /* Should use higher weight: avg_pe=0.5 * weight=2.0 = 1.0 > 0.5 */
    EXPECT_GT(bridge->state.current_epistemic_value, 0.5f);

    /* Knowledge gap detection should be disabled */
    EXPECT_EQ(bridge->state.knowledge_gaps_detected, 0);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(CuriosityFEPIntegrationTest, ZeroPredictionError) {
    /* Set zero prediction errors */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        fep->levels[l].errors.magnitude = 0.0f;
    }

    /* Update */
    curiosity_fep_bridge_update(bridge, 100);

    /* Should have zero epistemic value */
    EXPECT_FLOAT_EQ(bridge->state.current_epistemic_value, 0.0f);

    /* Should not trigger exploration */
    EXPECT_LE(bridge->effects.exploration_motivation, 0.5f);
}

TEST_F(CuriosityFEPIntegrationTest, ExtremelyHighPredictionError) {
    /* Set very high prediction errors */
    for (uint32_t l = 0; l < fep->num_levels; l++) {
        fep->levels[l].errors.magnitude = 10.0f;
    }

    /* Update */
    curiosity_fep_bridge_update(bridge, 100);

    /* Should clamp to max */
    EXPECT_LE(bridge->state.current_epistemic_value, EPISTEMIC_VALUE_MAX);

    /* Should trigger exploration */
    EXPECT_GT(bridge->effects.exploration_motivation, 0.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
