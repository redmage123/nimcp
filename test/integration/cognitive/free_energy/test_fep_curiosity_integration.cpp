/**
 * @file test_fep_curiosity_integration.cpp
 * @brief Integration tests for FEP Curiosity module with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "cognitive/free_energy/nimcp_fep_neuromod.h"

class FEPCuriosityIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;

    fep_curiosity_system_t* curiosity = nullptr;
    fep_system_t* fep = nullptr;
    fep_planning_system_t* planning = nullptr;

    void SetUp() override {
        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        // Create curiosity system
        fep_curiosity_config_t cur_config;
        fep_curiosity_default_config(&cur_config);
        curiosity = fep_curiosity_create(&cur_config);

        // Create planning system
        fep_planning_config_t plan_config;
        fep_planning_default_config(&plan_config);
        planning = fep_planning_create(&plan_config);
    }

    void TearDown() override {
        if (curiosity) {
            fep_curiosity_destroy(curiosity);
            curiosity = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (planning) {
            fep_planning_destroy(planning);
            planning = nullptr;
        }
    }
};

/* ============================================================================
 * Curiosity + FEP Core Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, CuriosityWithFEPSystem) {
    ASSERT_NE(curiosity, nullptr);
    ASSERT_NE(fep, nullptr);

    int ret = fep_curiosity_connect(curiosity, fep);
    EXPECT_EQ(ret, 0);

    // Create a policy for epistemic value computation
    fep_policy_t policy = {};
    policy.policy_id = 0;
    policy.num_actions = 1;
    policy.action_dim = ACTION_DIM;
    float epistemic = fep_compute_epistemic_value(curiosity, fep, &policy);
    EXPECT_TRUE(std::isfinite(epistemic));
}

TEST_F(FEPCuriosityIntegrationTest, NoveltyDetectionWithFEP) {
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    std::vector<float> familiar_state(STATE_DIM, 0.5f);
    std::vector<float> novel_state(STATE_DIM, 0.9f);

    float familiar_novelty = fep_compute_novelty(curiosity, familiar_state.data(), STATE_DIM);
    float novel_novelty = fep_compute_novelty(curiosity, novel_state.data(), STATE_DIM);

    EXPECT_GE(familiar_novelty, 0.0f);
    EXPECT_GE(novel_novelty, 0.0f);
}

/* ============================================================================
 * Curiosity + Planning Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, CuriosityGuidedPlanning) {
    ASSERT_NE(curiosity, nullptr);
    ASSERT_NE(planning, nullptr);

    fep_curiosity_connect(curiosity, fep);
    fep_planning_connect(planning, fep);

    std::vector<float> state(STATE_DIM, 0.5f);
    float novelty = fep_compute_novelty(curiosity, state.data(), STATE_DIM);
    EXPECT_GE(novelty, 0.0f);
}

/* ============================================================================
 * EFE Modulation Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, EFEModulation) {
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    // Create EFE structure for modulation
    fep_efe_t efe = {};
    efe.total = -1.0f;
    efe.risk = 0.5f;
    efe.ambiguity = 0.3f;

    int ret = fep_curiosity_modulate_efe(curiosity, fep, &efe);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Information Gain Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, InformationGainComputation) {
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    std::vector<float> state(STATE_DIM, 0.5f);
    float info_gain = fep_compute_information_gain(curiosity, fep, state.data(), STATE_DIM);
    EXPECT_TRUE(std::isfinite(info_gain));
}

/* ============================================================================
 * Empowerment Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, EmpowermentComputation) {
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    std::vector<float> state(STATE_DIM, 0.5f);
    float empowerment = fep_compute_empowerment(curiosity, fep, state.data(), STATE_DIM);
    EXPECT_TRUE(std::isfinite(empowerment));
}

/* ============================================================================
 * Action Selection Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, ActionSelection) {
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    uint32_t action = 0;

    int ret = fep_curiosity_select_action(curiosity, fep, &action);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Stats Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, StatsTracking) {
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    std::vector<float> state(STATE_DIM, 0.5f);
    fep_compute_novelty(curiosity, state.data(), STATE_DIM);

    // Use policy for epistemic value
    fep_policy_t policy = {};
    policy.policy_id = 0;
    fep_compute_epistemic_value(curiosity, fep, &policy);

    fep_curiosity_stats_t stats;
    int ret = fep_curiosity_get_stats(curiosity, &stats);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, BioAsyncConnection) {
    ASSERT_NE(curiosity, nullptr);

    int ret = fep_curiosity_connect_bio_async(curiosity);
    EXPECT_EQ(ret, 0);

    ret = fep_curiosity_disconnect_bio_async(curiosity);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Reset Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, Reset) {
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    std::vector<float> state(STATE_DIM, 0.5f);
    fep_compute_novelty(curiosity, state.data(), STATE_DIM);

    int ret = fep_curiosity_reset(curiosity);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Observation Recording Integration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityIntegrationTest, ObservationRecording) {
    ASSERT_NE(curiosity, nullptr);

    std::vector<float> obs(OBS_DIM, 0.5f);
    int ret = fep_curiosity_record_observation(curiosity, obs.data(), OBS_DIM);
    EXPECT_EQ(ret, 0);
}
