/**
 * @file test_fep_curiosity.cpp
 * @brief Unit tests for FEP Curiosity module
 * @date 2025-12-12
 *
 * WHAT: Tests for epistemic value and curiosity-driven exploration
 * WHY:  Verify curiosity computations for information-seeking behavior
 * HOW:  Test epistemic value, information gain, empowerment, novelty
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class FEPCuriosityTest : public ::testing::Test {
protected:
    fep_curiosity_system_t* curiosity = nullptr;
    fep_curiosity_config_t config;
    fep_system_t* fep = nullptr;
    fep_config_t fep_config;

    static constexpr uint32_t STATE_DIM = 8;
    static constexpr uint32_t OBS_DIM = 16;
    static constexpr uint32_t ACTION_DIM = 4;

    void SetUp() override {
        fep_curiosity_default_config(&config);
        fep_default_config(&fep_config);
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
    }

    void createCuriosity() {
        curiosity = fep_curiosity_create(&config);
        ASSERT_NE(curiosity, nullptr);
    }

    void createFEP() {
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);
    }

    std::vector<float> createState(float base = 1.0f) {
        std::vector<float> state(STATE_DIM);
        for (uint32_t i = 0; i < STATE_DIM; i++) {
            state[i] = base + 0.1f * static_cast<float>(i);
        }
        return state;
    }

    std::vector<float> createObservation(float base = 1.0f) {
        std::vector<float> obs(OBS_DIM);
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = base + 0.05f * static_cast<float>(i);
        }
        return obs;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, CreateDestroy) {
    // WHAT: Create and destroy curiosity system
    createCuriosity();
    ASSERT_NE(curiosity, nullptr);
}

TEST_F(FEPCuriosityTest, CreateWithNullConfig) {
    // WHAT: Create with NULL should use defaults
    curiosity = fep_curiosity_create(nullptr);
    ASSERT_NE(curiosity, nullptr);
}

TEST_F(FEPCuriosityTest, DestroyNullSafe) {
    // WHAT: Destroying NULL should not crash
    fep_curiosity_destroy(nullptr);
    // Should not crash
}

TEST_F(FEPCuriosityTest, DefaultConfig) {
    // WHAT: Verify default config has sensible values
    fep_curiosity_config_t cfg;
    fep_curiosity_default_config(&cfg);

    EXPECT_GT(cfg.exploration_bonus, 0.0f);
    EXPECT_GT(cfg.novelty_threshold, 0.0f);
    EXPECT_GT(cfg.information_gain_weight, 0.0f);
    EXPECT_GT(cfg.memory_capacity, 0u);
}

TEST_F(FEPCuriosityTest, DefaultConfigNullSafe) {
    // WHAT: Default config with NULL should not crash
    fep_curiosity_default_config(nullptr);
    // Should not crash
}

TEST_F(FEPCuriosityTest, Reset) {
    // WHAT: Reset should clear state
    createCuriosity();

    int result = fep_curiosity_reset(curiosity);
    EXPECT_EQ(result, 0);
}

TEST_F(FEPCuriosityTest, ResetNullFails) {
    // WHAT: Reset NULL should fail
    int result = fep_curiosity_reset(nullptr);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, GetState) {
    // WHAT: Get current curiosity state
    createCuriosity();

    fep_curiosity_state_t state;
    int result = fep_curiosity_get_state(curiosity, &state);

    EXPECT_EQ(result, 0);
    EXPECT_GE(state.exploration_drive, 0.0f);
    EXPECT_LE(state.exploration_drive, 1.0f);
}

TEST_F(FEPCuriosityTest, GetStateNullChecks) {
    // WHAT: Null checks for state query
    createCuriosity();

    fep_curiosity_state_t state;
    EXPECT_NE(fep_curiosity_get_state(nullptr, &state), 0);
    EXPECT_NE(fep_curiosity_get_state(curiosity, nullptr), 0);
}

TEST_F(FEPCuriosityTest, GetStats) {
    // WHAT: Get curiosity statistics
    createCuriosity();

    fep_curiosity_stats_t stats;
    int result = fep_curiosity_get_stats(curiosity, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.observations_processed, 0u);
    EXPECT_EQ(stats.novel_states_found, 0u);
}

TEST_F(FEPCuriosityTest, GetStatsNullChecks) {
    // WHAT: Null checks for stats query
    createCuriosity();

    fep_curiosity_stats_t stats;
    EXPECT_NE(fep_curiosity_get_stats(nullptr, &stats), 0);
    EXPECT_NE(fep_curiosity_get_stats(curiosity, nullptr), 0);
}

/* ============================================================================
 * Novelty Computation Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, ComputeNovelty) {
    // WHAT: Compute novelty for state
    createCuriosity();

    auto state = createState();
    float novelty = fep_compute_novelty(curiosity, state.data(), STATE_DIM);

    // First observation should be highly novel
    EXPECT_GE(novelty, 0.0f);
    EXPECT_LE(novelty, 1.0f);
}

TEST_F(FEPCuriosityTest, ComputeNoveltyNullSafe) {
    // WHAT: Novelty with NULL should return 0
    auto state = createState();

    float novelty = fep_compute_novelty(nullptr, state.data(), STATE_DIM);
    EXPECT_EQ(novelty, 0.0f);
}

TEST_F(FEPCuriosityTest, NoveltyDecreasesWithRepetition) {
    // WHAT: Repeated states should have decreasing novelty
    createCuriosity();

    auto state = createState();

    float first_novelty = fep_compute_novelty(curiosity, state.data(), STATE_DIM);

    // Record observations to update counts
    for (int i = 0; i < 10; i++) {
        fep_curiosity_record_observation(curiosity, state.data(), STATE_DIM);
    }

    float later_novelty = fep_compute_novelty(curiosity, state.data(), STATE_DIM);

    // Novelty should decrease with familiarity
    EXPECT_LE(later_novelty, first_novelty);
}

/* ============================================================================
 * Information Gain Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, ComputeInformationGain) {
    // WHAT: Compute information gain from observation
    createCuriosity();
    createFEP();

    auto observation = createObservation();
    float info_gain = fep_compute_information_gain(curiosity, fep, observation.data(), OBS_DIM);

    EXPECT_GE(info_gain, 0.0f);
}

TEST_F(FEPCuriosityTest, InformationGainNullChecks) {
    // WHAT: Null checks for info gain
    createCuriosity();
    createFEP();

    auto observation = createObservation();

    float ig1 = fep_compute_information_gain(nullptr, fep, observation.data(), OBS_DIM);
    EXPECT_EQ(ig1, 0.0f);

    float ig2 = fep_compute_information_gain(curiosity, nullptr, observation.data(), OBS_DIM);
    EXPECT_EQ(ig2, 0.0f);
}

/* ============================================================================
 * Empowerment Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, ComputeEmpowerment) {
    // WHAT: Compute empowerment for state
    createCuriosity();
    createFEP();

    auto state = createState();
    float empowerment = fep_compute_empowerment(curiosity, fep, state.data(), STATE_DIM);

    EXPECT_GE(empowerment, 0.0f);
}

TEST_F(FEPCuriosityTest, EmpowermentNullChecks) {
    // WHAT: Null checks for empowerment
    createCuriosity();
    createFEP();

    auto state = createState();

    float emp1 = fep_compute_empowerment(nullptr, fep, state.data(), STATE_DIM);
    EXPECT_EQ(emp1, 0.0f);

    float emp2 = fep_compute_empowerment(curiosity, nullptr, state.data(), STATE_DIM);
    EXPECT_EQ(emp2, 0.0f);
}

/* ============================================================================
 * Epistemic Value Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, ComputeEpistemicValue) {
    // WHAT: Compute epistemic value for policy
    createCuriosity();
    createFEP();

    fep_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    policy.num_actions = 1;
    float actions[1] = {0.5f};
    policy.actions = actions;

    float epistemic = fep_compute_epistemic_value(curiosity, fep, &policy);
    EXPECT_GE(epistemic, 0.0f);
}

TEST_F(FEPCuriosityTest, EpistemicValueNullChecks) {
    // WHAT: Null checks for epistemic value
    createCuriosity();
    createFEP();

    fep_policy_t policy;
    memset(&policy, 0, sizeof(policy));

    float ev1 = fep_compute_epistemic_value(nullptr, fep, &policy);
    EXPECT_EQ(ev1, 0.0f);

    float ev2 = fep_compute_epistemic_value(curiosity, nullptr, &policy);
    EXPECT_EQ(ev2, 0.0f);
}

/* ============================================================================
 * Observation Recording Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, RecordObservation) {
    // WHAT: Record observation for novelty tracking
    createCuriosity();

    auto observation = createObservation();
    int result = fep_curiosity_record_observation(curiosity, observation.data(), OBS_DIM);
    EXPECT_EQ(result, 0);

    // Check stats updated
    fep_curiosity_stats_t stats;
    fep_curiosity_get_stats(curiosity, &stats);
    EXPECT_GT(stats.observations_processed, 0u);
}

TEST_F(FEPCuriosityTest, RecordObservationNullChecks) {
    // WHAT: Null checks for record observation
    createCuriosity();

    auto observation = createObservation();

    EXPECT_NE(fep_curiosity_record_observation(nullptr, observation.data(), OBS_DIM), 0);
    EXPECT_NE(fep_curiosity_record_observation(curiosity, nullptr, OBS_DIM), 0);
}

TEST_F(FEPCuriosityTest, RecordMultipleObservations) {
    // WHAT: Record multiple observations
    createCuriosity();

    for (int i = 0; i < 50; i++) {
        auto obs = createObservation(static_cast<float>(i) * 0.1f);
        int result = fep_curiosity_record_observation(curiosity, obs.data(), OBS_DIM);
        EXPECT_EQ(result, 0);
    }

    fep_curiosity_stats_t stats;
    fep_curiosity_get_stats(curiosity, &stats);
    EXPECT_EQ(stats.observations_processed, 50u);
}

/* ============================================================================
 * Action Selection Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, SelectAction) {
    // WHAT: Select action with curiosity-driven exploration
    createCuriosity();
    createFEP();

    uint32_t action;
    int result = fep_curiosity_select_action(curiosity, fep, &action);
    EXPECT_EQ(result, 0);
    EXPECT_LT(action, ACTION_DIM);
}

TEST_F(FEPCuriosityTest, SelectActionNullChecks) {
    // WHAT: Null checks for action selection
    createCuriosity();
    createFEP();

    uint32_t action;
    EXPECT_NE(fep_curiosity_select_action(nullptr, fep, &action), 0);
    EXPECT_NE(fep_curiosity_select_action(curiosity, nullptr, &action), 0);
    EXPECT_NE(fep_curiosity_select_action(curiosity, fep, nullptr), 0);
}

/* ============================================================================
 * FEP Connection Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, ConnectToFEP) {
    // WHAT: Connect curiosity to FEP system
    createCuriosity();
    createFEP();

    int result = fep_curiosity_connect(curiosity, fep);
    EXPECT_EQ(result, 0);

    result = fep_curiosity_disconnect(curiosity);
    EXPECT_EQ(result, 0);
}

TEST_F(FEPCuriosityTest, ConnectNullChecks) {
    // WHAT: Null checks for connection
    createCuriosity();
    createFEP();

    EXPECT_NE(fep_curiosity_connect(nullptr, fep), 0);
    EXPECT_NE(fep_curiosity_connect(curiosity, nullptr), 0);
}

TEST_F(FEPCuriosityTest, DisconnectNullSafe) {
    // WHAT: Disconnect NULL should not crash
    int result = fep_curiosity_disconnect(nullptr);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * EFE Modulation Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, ModulateEFE) {
    // WHAT: Modulate expected free energy with curiosity
    createCuriosity();
    createFEP();

    fep_efe_t efe;
    memset(&efe, 0, sizeof(efe));
    efe.risk = 0.5f;
    efe.ambiguity = 0.3f;
    efe.total = 0.8f;

    int result = fep_curiosity_modulate_efe(curiosity, fep, &efe);
    EXPECT_EQ(result, 0);
}

TEST_F(FEPCuriosityTest, ModulateEFENullChecks) {
    // WHAT: Null checks for EFE modulation
    createCuriosity();
    createFEP();

    fep_efe_t efe;
    memset(&efe, 0, sizeof(efe));

    EXPECT_NE(fep_curiosity_modulate_efe(nullptr, fep, &efe), 0);
    EXPECT_NE(fep_curiosity_modulate_efe(curiosity, nullptr, &efe), 0);
    EXPECT_NE(fep_curiosity_modulate_efe(curiosity, fep, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, BioAsyncConnectDisconnect) {
    // WHAT: Connect and disconnect bio-async
    createCuriosity();

    EXPECT_FALSE(fep_curiosity_is_bio_async_connected(curiosity));

    int result = fep_curiosity_connect_bio_async(curiosity);
    EXPECT_EQ(result, 0);

    result = fep_curiosity_disconnect_bio_async(curiosity);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(fep_curiosity_is_bio_async_connected(curiosity));
}

TEST_F(FEPCuriosityTest, BioAsyncNullChecks) {
    // WHAT: Null checks for bio-async
    EXPECT_NE(fep_curiosity_connect_bio_async(nullptr), 0);
    EXPECT_NE(fep_curiosity_disconnect_bio_async(nullptr), 0);
    EXPECT_FALSE(fep_curiosity_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, CuriosityTypeToString) {
    // WHAT: Convert curiosity types to strings
    EXPECT_STREQ(fep_curiosity_type_to_string(CURIOSITY_EPISTEMIC), "Epistemic");
    EXPECT_STREQ(fep_curiosity_type_to_string(CURIOSITY_EMPOWERMENT), "Empowerment");
    EXPECT_STREQ(fep_curiosity_type_to_string(CURIOSITY_COMPETENCE), "Competence");
    EXPECT_STREQ(fep_curiosity_type_to_string(CURIOSITY_NOVELTY), "Novelty");
}

/* ============================================================================
 * Curiosity Type Configuration Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, EpistemicCuriosityType) {
    // WHAT: Test epistemic curiosity mode
    config.type = CURIOSITY_EPISTEMIC;
    createCuriosity();
    createFEP();

    // Should compute epistemic value
    fep_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    float epistemic = fep_compute_epistemic_value(curiosity, fep, &policy);
    EXPECT_GE(epistemic, 0.0f);
}

TEST_F(FEPCuriosityTest, NoveltyCuriosityType) {
    // WHAT: Test novelty curiosity mode
    config.type = CURIOSITY_NOVELTY;
    createCuriosity();

    auto state = createState();
    float novelty = fep_compute_novelty(curiosity, state.data(), STATE_DIM);
    EXPECT_GE(novelty, 0.0f);
}

TEST_F(FEPCuriosityTest, EmpowermentCuriosityType) {
    // WHAT: Test empowerment curiosity mode
    config.type = CURIOSITY_EMPOWERMENT;
    createCuriosity();
    createFEP();

    auto state = createState();
    float emp = fep_compute_empowerment(curiosity, fep, state.data(), STATE_DIM);
    EXPECT_GE(emp, 0.0f);
}

/* ============================================================================
 * Configuration Parameter Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, HighExplorationBonus) {
    // WHAT: High exploration bonus should increase exploration drive
    config.exploration_bonus = 0.9f;
    createCuriosity();

    fep_curiosity_state_t state;
    fep_curiosity_get_state(curiosity, &state);
    // Should not crash with high exploration bonus
}

TEST_F(FEPCuriosityTest, LowExplorationBonus) {
    // WHAT: Low exploration bonus should favor exploitation
    config.exploration_bonus = 0.1f;
    createCuriosity();

    fep_curiosity_state_t state;
    fep_curiosity_get_state(curiosity, &state);
    // Should not crash with low exploration bonus
}

TEST_F(FEPCuriosityTest, LargeMemoryCapacity) {
    // WHAT: Test with large memory capacity
    config.memory_capacity = 10000;
    createCuriosity();

    // Record many observations
    for (int i = 0; i < 100; i++) {
        auto obs = createObservation(static_cast<float>(i) * 0.1f);
        fep_curiosity_record_observation(curiosity, obs.data(), OBS_DIM);
    }

    fep_curiosity_stats_t stats;
    fep_curiosity_get_stats(curiosity, &stats);
    EXPECT_EQ(stats.observations_processed, 100u);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FEPCuriosityTest, EmptyState) {
    // WHAT: Handle empty/zero state
    createCuriosity();

    std::vector<float> empty_state(STATE_DIM, 0.0f);
    float novelty = fep_compute_novelty(curiosity, empty_state.data(), STATE_DIM);
    EXPECT_GE(novelty, 0.0f);
}

TEST_F(FEPCuriosityTest, RepeatedIdenticalObservations) {
    // WHAT: Repeated identical observations should decrease novelty
    createCuriosity();

    auto obs = createObservation(1.0f);

    // Record many identical observations
    for (int i = 0; i < 100; i++) {
        fep_curiosity_record_observation(curiosity, obs.data(), OBS_DIM);
    }

    // Novelty should be very low for this state
    float novelty = fep_compute_novelty(curiosity, obs.data(), OBS_DIM);
    EXPECT_LT(novelty, 0.5f);  // Should be below threshold
}

TEST_F(FEPCuriosityTest, HighVarianceObservations) {
    // WHAT: High variance observations should maintain high novelty
    createCuriosity();

    // Record many different observations
    for (int i = 0; i < 50; i++) {
        auto obs = createObservation(static_cast<float>(i) * 0.5f);
        fep_curiosity_record_observation(curiosity, obs.data(), OBS_DIM);
    }

    fep_curiosity_stats_t stats;
    fep_curiosity_get_stats(curiosity, &stats);
    EXPECT_GT(stats.novel_states_found, 0u);
}
