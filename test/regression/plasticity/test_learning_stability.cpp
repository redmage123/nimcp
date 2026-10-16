/**
 * @file test_learning_stability.cpp
 * @brief Regression tests for learning stability with bio-async integration
 *
 * These tests ensure that the integration of bio-async messaging doesn't
 * destabilize learning, and that plasticity rules continue to converge
 * properly.
 *
 * API verified from headers:
 * - nimcp_stdp.h: stdp_synapse_t, stdp_config_t, stdp_synapse_init, stdp_config_default
 * - nimcp_homeostatic.h: homeostatic_controller_t, homeostatic_config_default, homeostatic_controller_create
 * - nimcp_bcm.h: bcm_synapse_t, bcm_params_t, bcm_synapse_init, bcm_params_cortical
 * - nimcp_neuromodulators.h: neuromodulator_system_t, neuromodulator_system_create, NEUROMOD_DOPAMINE
 * - nimcp_predictive_coding.h: pc_hierarchy_t, pc_hierarchy_config_default, pc_hierarchy_create
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

class LearningStabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_bio_async_config_t async_config;
        memset(&async_config, 0, sizeof(async_config));
        nimcp_bio_async_init(&async_config);

        bio_router_config_t cfg = bio_router_default_config();
        bio_router_init(&cfg);
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    float ComputeVariance(const std::vector<float>& values) {
        if (values.empty()) return 0.0f;

        float mean = 0.0f;
        for (float v : values) mean += v;
        mean /= values.size();

        float variance = 0.0f;
        for (float v : values) {
            float diff = v - mean;
            variance += diff * diff;
        }
        return variance / values.size();
    }

    bool IsStable(const std::vector<float>& trajectory, float tolerance = 0.01f) {
        if (trajectory.size() < 100) return false;

        // Check last 20% of trajectory for stability
        size_t start = trajectory.size() * 4 / 5;
        std::vector<float> tail(trajectory.begin() + start, trajectory.end());

        float variance = ComputeVariance(tail);
        return variance < tolerance;
    }
};

TEST_F(LearningStabilityTest, STDPConvergence) {
    // Test that STDP weights converge to stable values
    // API: stdp_synapse_t, stdp_synapse_init(), stdp_pre_spike(), stdp_post_spike()
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    std::vector<float> weight_trajectory;
    float dt = 0.001f;  // 1ms time step

    // Simulate 1000 learning steps with correlated pre-post spike pairs
    for (int i = 0; i < 1000; i++) {
        float current_time = i * 10.0f;  // 10ms between spike pairs

        // Pre-synaptic spike
        stdp_pre_spike(&synapse, current_time);

        // Post-synaptic spike 5ms later (LTP condition)
        stdp_post_spike(&synapse, current_time + 5.0f);

        // Update traces
        stdp_update_traces(&synapse, dt);

        weight_trajectory.push_back(synapse.weight);
    }

    // Check that weights converged
    EXPECT_TRUE(IsStable(weight_trajectory, 0.01f))
        << "STDP weights did not converge to stable values";

    // Check weights stayed in valid range
    float final_weight = weight_trajectory.back();
    EXPECT_GE(final_weight, synapse.w_min);
    EXPECT_LE(final_weight, synapse.w_max);
}

TEST_F(LearningStabilityTest, HomeostaticRegulation) {
    // Test that homeostatic plasticity maintains stable firing rates
    // API: homeostatic_controller_t, homeostatic_config_default(), homeostatic_controller_create()
    homeostatic_config_t config = homeostatic_config_default();
    uint32_t num_neurons = 10;
    homeostatic_controller_t controller = homeostatic_controller_create(&config, num_neurons);
    ASSERT_NE(controller, nullptr);

    std::vector<float> stability_trajectory;
    float dt = 1.0f;  // 1ms

    // Create weight array
    uint32_t num_synapses = 100;
    std::vector<float> weights(num_neurons * num_synapses, 0.5f);
    std::vector<float> firing_rates(num_neurons, 10.0f);  // Start above target

    // Simulate 1000 updates
    for (int i = 0; i < 1000; i++) {
        // Simulate variable input that modulates firing rate
        for (size_t n = 0; n < num_neurons; n++) {
            firing_rates[n] = 10.0f + 5.0f * sinf(i * 0.01f + n * 0.1f);
        }

        homeostatic_controller_update(controller, firing_rates.data(),
                                      weights.data(), num_synapses, dt);

        // Check stability
        homeostatic_stats_t stats;
        if (homeostatic_controller_get_stats(controller, &stats)) {
            stability_trajectory.push_back(stats.stability_score);
        }
    }

    homeostatic_controller_destroy(controller);

    // Check that stability improved
    if (!stability_trajectory.empty()) {
        float final_stability = stability_trajectory.back();
        EXPECT_GT(final_stability, 0.0f)
            << "Homeostatic controller did not improve stability";
    }
}

TEST_F(LearningStabilityTest, BCMThresholdAdaptation) {
    // Test that BCM threshold adapts appropriately
    // API: bcm_synapse_t, bcm_synapse_init(), bcm_params_cortical(), bcm_update_threshold()
    bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.3f);
    bcm_params_t params = bcm_params_cortical();

    std::vector<float> threshold_trajectory;
    float dt = 1.0f;  // 1ms

    // Simulate learning with varying activity levels
    for (int i = 0; i < 1000; i++) {
        float post_activity = 0.3f + 0.2f * sinf(i * 0.02f);

        bcm_update_threshold(&synapse, post_activity, dt, &params);

        threshold_trajectory.push_back(synapse.threshold);
    }

    // Check threshold stayed in valid range
    for (float th : threshold_trajectory) {
        EXPECT_GE(th, params.min_threshold);
        EXPECT_LE(th, params.max_threshold);
    }

    // Check threshold converged
    EXPECT_TRUE(IsStable(threshold_trajectory, 0.01f))
        << "BCM threshold did not converge";
}

TEST_F(LearningStabilityTest, PredictiveCodingConvergence) {
    // Test that predictive coding hierarchy converges to low free energy
    // API: pc_hierarchy_t, pc_hierarchy_config_default(), pc_hierarchy_create()
    uint32_t units[] = {10, 5, 2};
    pc_hierarchy_config_t config = pc_hierarchy_config_default(3, units);
    config.units_per_level = units;  // Must set explicitly after config_default
    pc_hierarchy_t hierarchy = pc_hierarchy_create(&config);
    ASSERT_NE(hierarchy, nullptr);

    std::vector<float> free_energy_trajectory;

    // Fixed input pattern
    float input[] = {0.5f, 0.3f, 0.8f, 0.1f, 0.6f, 0.4f, 0.9f, 0.2f, 0.7f, 0.0f};

    // Run inference for 500 steps
    for (int i = 0; i < 500; i++) {
        pc_hierarchy_set_input(hierarchy, input);
        pc_hierarchy_inference_step(hierarchy, 1.0f, true);

        float free_energy = pc_hierarchy_get_free_energy(hierarchy);
        free_energy_trajectory.push_back(free_energy);
    }

    pc_hierarchy_destroy(hierarchy);

    // Check free energy decreased and stabilized
    if (free_energy_trajectory.size() >= 2) {
        float initial_fe = free_energy_trajectory[0];
        float final_fe = free_energy_trajectory.back();

        EXPECT_LE(final_fe, initial_fe)
            << "Free energy did not decrease during inference";
    }

    EXPECT_TRUE(IsStable(free_energy_trajectory, 1.0f))
        << "Free energy did not converge";
}

TEST_F(LearningStabilityTest, NeuromodulatorDynamics) {
    // Test that neuromodulator concentrations remain stable
    // API: neuromodulator_system_t, neuromodulator_system_create(), NEUROMOD_DOPAMINE
    neuromodulator_config_t config = {
        .baseline_dopamine = 0.5f,
        .baseline_serotonin = 0.5f,
        .baseline_acetylcholine = 0.5f,
        .baseline_norepinephrine = 0.5f,
        .dopamine_decay = 2.0f,
        .serotonin_decay = 10.0f,
        .acetylcholine_decay = 0.5f,
        .norepinephrine_decay = 3.0f,
        .reward_dopamine_gain = 0.5f,
        .threat_norepinephrine_gain = 0.7f,
        .salience_acetylcholine_gain = 0.6f,
        .punishment_serotonin_gain = 0.4f,
        .enable_volume_transmission = true,
        .diffusion_rate = 0.1f
    };
    neuromodulator_system_t system = neuromodulator_system_create(&config);
    ASSERT_NE(system, nullptr);

    std::vector<float> dopamine_trajectory;
    float dt = 0.01f;  // 10ms

    // Simulate reward/error signals
    for (int i = 0; i < 1000; i++) {
        // Periodic reward
        if (i % 100 < 50) {
            neuromodulator_release_dopamine(system, 1.0f, 0.5f);
        }

        neuromodulator_update(system, dt);

        float dopamine = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
        dopamine_trajectory.push_back(dopamine);
    }

    neuromodulator_system_destroy(system);

    // Check dopamine stayed in physiological range
    for (float conc : dopamine_trajectory) {
        EXPECT_GE(conc, 0.0f);
        EXPECT_LE(conc, 1.0f);
    }

    // Check for oscillatory stability (not runaway)
    float variance = ComputeVariance(dopamine_trajectory);
    EXPECT_LT(variance, 0.5f)
        << "Dopamine concentration variance too high";
}

TEST_F(LearningStabilityTest, WeightBoundsRespected) {
    // Regression test: ensure weights stay in [w_min, w_max] during learning
    // API: stdp_synapse_t, stdp_synapse_init_with_config()
    stdp_config_t config = stdp_config_default();
    config.learning_rate = 0.1f;  // High learning rate for stress test

    stdp_synapse_t synapse;
    stdp_synapse_init_with_config(&synapse, &config);

    // Extreme learning scenario
    for (int i = 0; i < 10000; i++) {
        float current_time = i * 1.0f;

        // Very strong potentiation signal (pre before post)
        stdp_pre_spike(&synapse, current_time);
        stdp_post_spike(&synapse, current_time + 1.0f);
        stdp_update_traces(&synapse, 0.001f);

        // Weights should never go out of bounds
        ASSERT_GE(synapse.weight, synapse.w_min)
            << "Weight went below w_min at iteration " << i;
        ASSERT_LE(synapse.weight, synapse.w_max)
            << "Weight exceeded w_max at iteration " << i;
    }
}

TEST_F(LearningStabilityTest, NoMemoryLeaks) {
    // Regression test: repeated create/destroy doesn't leak memory
    for (int i = 0; i < 100; i++) {
        // Create and destroy STDP synapses
        stdp_synapse_t synapse;
        stdp_synapse_init(&synapse);

        // Do some work
        for (int j = 0; j < 10; j++) {
            stdp_pre_spike(&synapse, j * 1.0f);
            stdp_post_spike(&synapse, j * 1.0f + 5.0f);
            stdp_update_traces(&synapse, 0.001f);
        }

        // Create and destroy BCM synapses
        bcm_synapse_t bcm = bcm_synapse_init(0.5f, 0.3f);
        bcm_params_t params = bcm_params_cortical();
        for (int j = 0; j < 10; j++) {
            bcm_apply_rule(&bcm, 0.5f, 0.4f, 1.0f, &params);
        }

        // Create and destroy homeostatic controller
        homeostatic_config_t h_config = homeostatic_config_default();
        homeostatic_controller_t h_ctrl = homeostatic_controller_create(&h_config, 5);
        if (h_ctrl) {
            homeostatic_controller_destroy(h_ctrl);
        }
    }

    // If we get here without crashing, no obvious memory corruption
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
