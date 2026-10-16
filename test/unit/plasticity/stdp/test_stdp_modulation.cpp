/**
 * @file test_stdp_modulation.cpp
 * @brief Tests for dopamine-modulated STDP
 *
 * NIMCP Phase: Option 2.1 (Integration)
 * Date: 2025-11-12
 */

#include <gtest/gtest.h>

#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

class STDPModulationTest : public ::testing::Test {
protected:
    stdp_synapse_t synapse;
    neuromodulator_system_t neuromod;  /* This is already a pointer type */

    void SetUp() override {
        stdp_synapse_init(&synapse);

        /* Create neuromodulator system with default config */
        neuromod = neuromodulator_system_create(NULL);  /* NULL for defaults */
        ASSERT_NE(neuromod, nullptr);
    }

    void TearDown() override {
        if (neuromod) {
            neuromodulator_system_destroy(neuromod);
        }
    }
};

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST_F(STDPModulationTest, Initialization) {
    EXPECT_EQ(synapse.weight, 0.5f);
    EXPECT_EQ(synapse.w_max, 1.0f);
    EXPECT_EQ(synapse.w_min, 0.0f);
    EXPECT_TRUE(synapse.enable_da_modulation);
    EXPECT_EQ(synapse.da_modulation_gain, 100.0f);
    EXPECT_EQ(synapse.burst_amplification, 3.0f);
}

TEST_F(STDPModulationTest, ConfigurationDefault) {
    stdp_config_t config = stdp_config_default();

    EXPECT_EQ(config.w_max, 1.0f);
    EXPECT_EQ(config.learning_rate, 0.01f);
    EXPECT_EQ(config.a_plus, 0.005f);
    EXPECT_EQ(config.a_minus, 0.00525f);
    EXPECT_TRUE(config.enable_da_modulation);
}

/* ============================================================================
 * Classic STDP (No Neuromodulation)
 * ============================================================================ */

TEST_F(STDPModulationTest, ClassicSTDP_LTP) {
    /* Disable dopamine modulation */
    synapse.enable_da_modulation = false;

    float initial_weight = synapse.weight;

    /* Pre spike → increment pre trace */
    stdp_pre_spike(&synapse, 0.0f);
    EXPECT_GT(synapse.pre_trace, 0.0f);

    /* Post spike shortly after → LTP */
    float weight_change = stdp_post_spike(&synapse, 10.0f);  /* 10 ms later */

    EXPECT_GT(weight_change, 0.0f);  /* Positive weight change */
    EXPECT_GT(synapse.weight, initial_weight);  /* Weight increased */
    EXPECT_EQ(synapse.num_potentiation_events, 1);
}

TEST_F(STDPModulationTest, ClassicSTDP_LTD) {
    synapse.enable_da_modulation = false;

    float initial_weight = synapse.weight;

    /* Post spike → increment post trace */
    stdp_post_spike(&synapse, 0.0f);
    EXPECT_GT(synapse.post_trace, 0.0f);

    /* Pre spike shortly after → LTD */
    float weight_change = stdp_pre_spike(&synapse, 10.0f);  /* 10 ms later */

    EXPECT_LT(weight_change, 0.0f);  /* Negative weight change */
    EXPECT_LT(synapse.weight, initial_weight);  /* Weight decreased */
    EXPECT_EQ(synapse.num_depression_events, 1);
}

TEST_F(STDPModulationTest, TraceDecay) {
    /* Set initial traces */
    synapse.pre_trace = 1.0f;
    synapse.post_trace = 1.0f;

    /* Update traces (20ms, one time constant) */
    float dt = 0.020f;  /* 20 ms in seconds */
    stdp_update_traces(&synapse, dt);

    /* Expect decay by 1/e ≈ 0.368 */
    EXPECT_NEAR(synapse.pre_trace, 1.0f / M_E, 0.01f);
    EXPECT_NEAR(synapse.post_trace, 1.0f / M_E, 0.01f);
}

/* ============================================================================
 * Dopamine Modulation Tests
 * ============================================================================ */

TEST_F(STDPModulationTest, ModulationFactor_Baseline) {
    /* At baseline (no dopamine burst), modulation should be minimal */
    float modulation = stdp_get_da_modulation_factor(&synapse, neuromod);

    /* Baseline DA ≈ 50 nM = 0.00005 µM = 0.05 in [0, 1] range */
    /* With gain 100: 1.0 + 0.05 * 100 = 6.0 */
    EXPECT_NEAR(modulation, 6.0f, 1.0f);  /* Allow some variance */
}

TEST_F(STDPModulationTest, ModulatedSTDP_RewardEnhancesLearning) {
    float initial_weight = synapse.weight;

    /* Trigger dopamine burst via reward delivery */
    neuromodulator_release_dopamine(neuromod, 1.0f, 0.2f);  /* Reward, predicted */
    neuromodulator_update(neuromod, 0.001f);

    /* Pre spike → post spike (LTP with dopamine modulation) */
    stdp_pre_spike_modulated(&synapse, 0.0f, neuromod);
    float weight_change = stdp_post_spike_modulated(&synapse, 10.0f, neuromod);

    /* Expect larger weight change than classic STDP */
    EXPECT_GT(weight_change, 0.0f);
    EXPECT_GT(synapse.weight, initial_weight);

    /* Weight change should be amplified */
    float baseline_change = synapse.a_plus * synapse.learning_rate;
    EXPECT_GT(weight_change, baseline_change * 2.0f);  /* At least 2x */
}

TEST_F(STDPModulationTest, ModulatedSTDP_NoRewardWeakLearning) {
    /* No dopamine burst, just tonic baseline */
    float initial_weight = synapse.weight;

    /* Pre spike → post spike with baseline dopamine */
    stdp_pre_spike_modulated(&synapse, 0.0f, neuromod);
    float weight_change = stdp_post_spike_modulated(&synapse, 10.0f, neuromod);

    /* Expect weight change, but smaller than during burst */
    EXPECT_GT(weight_change, 0.0f);
    EXPECT_GT(synapse.weight, initial_weight);
}

TEST_F(STDPModulationTest, ThreeFactorLearning_RewardTask) {
    /* Simulate reward learning task:
     * 1. Stimulus → neural activity (pre + post spikes)
     * 2. Reward delivered → dopamine burst
     * 3. STDP strengthens stimulus-response association
     */

    float initial_weight = synapse.weight;

    /* Trial 1: Stimulus + Response (no reward yet) */
    stdp_pre_spike_modulated(&synapse, 0.0f, neuromod);
    stdp_post_spike_modulated(&synapse, 10.0f, neuromod);
    float weight_after_trial1 = synapse.weight;

    /* Small weight change (tonic dopamine only) */
    EXPECT_GT(weight_after_trial1, initial_weight);

    /* Trial 2: Stimulus + Response + REWARD */
    neuromodulator_release_dopamine(neuromod, 1.0f, 0.2f);  /* Reward prediction error */
    neuromodulator_update(neuromod, 0.001f);

    stdp_pre_spike_modulated(&synapse, 0.0f, neuromod);
    stdp_post_spike_modulated(&synapse, 10.0f, neuromod);
    float weight_after_trial2 = synapse.weight;

    /* Large weight change (burst amplification) */
    float trial1_change = weight_after_trial1 - initial_weight;
    float trial2_change = weight_after_trial2 - weight_after_trial1;

    EXPECT_GT(trial2_change, trial1_change);  /* More learning with reward */
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(STDPModulationTest, StatisticsTracking) {
    synapse.enable_da_modulation = false;

    /* 10 LTP events */
    for (int i = 0; i < 10; i++) {
        stdp_pre_spike(&synapse, 0.0f);
        stdp_post_spike(&synapse, 5.0f);
    }

    EXPECT_EQ(synapse.num_potentiation_events, 10);
    EXPECT_GT(synapse.total_ltp, 0.0f);

    /* Reset traces and statistics to isolate LTD test phase */
    synapse.pre_trace = 0.0f;
    synapse.post_trace = 0.0f;
    synapse.num_potentiation_events = 0;
    synapse.num_depression_events = 0;
    synapse.total_ltp = 0.0f;
    synapse.total_ltd = 0.0f;

    /* 5 LTD events */
    for (int i = 0; i < 5; i++) {
        stdp_post_spike(&synapse, 100.0f);
        stdp_pre_spike(&synapse, 105.0f);
    }

    EXPECT_EQ(synapse.num_depression_events, 5);
    EXPECT_GT(synapse.total_ltd, 0.0f);
}

TEST_F(STDPModulationTest, Reset) {
    synapse.pre_trace = 1.0f;
    synapse.post_trace = 1.0f;
    synapse.num_potentiation_events = 10;
    synapse.total_ltp = 0.5f;

    stdp_synapse_reset(&synapse);

    EXPECT_EQ(synapse.pre_trace, 0.0f);
    EXPECT_EQ(synapse.post_trace, 0.0f);
    EXPECT_EQ(synapse.num_potentiation_events, 0);
    EXPECT_EQ(synapse.total_ltp, 0.0f);
}

/* ============================================================================
 * Weight Bounds Tests
 * ============================================================================ */

TEST_F(STDPModulationTest, WeightClampingMax) {
    synapse.weight = 0.99f;
    synapse.pre_trace = 300.0f;  /* Large trace to exceed w_max */

    stdp_post_spike(&synapse, 0.0f);

    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_EQ(synapse.weight, synapse.w_max);  /* Should be clamped to 1.0 */
}

TEST_F(STDPModulationTest, WeightClampingMin) {
    synapse.weight = 0.01f;
    synapse.post_trace = 300.0f;  /* Large trace to go below w_min */

    stdp_pre_spike(&synapse, 0.0f);

    EXPECT_GE(synapse.weight, synapse.w_min);
    EXPECT_EQ(synapse.weight, synapse.w_min);  /* Should be clamped to 0.0 */
}

/* ============================================================================
 * Performance Test
 * ============================================================================ */

TEST_F(STDPModulationTest, PerformanceUpdate10000Spikes) {
    auto start = std::chrono::high_resolution_clock::now();

    /* 10,000 spike pairs with dopamine modulation */
    for (int i = 0; i < 10000; i++) {
        stdp_pre_spike_modulated(&synapse, (float)i, neuromod);
        stdp_post_spike_modulated(&synapse, (float)i + 5.0f, neuromod);

        /* Update traces */
        stdp_update_traces(&synapse, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    printf("Performance: 10000 STDP updates in %ld µs\n", duration.count());

    /* Should be fast (<1 ms for 10k updates) */
    EXPECT_LT(duration.count(), 1000);  /* <1 ms */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
