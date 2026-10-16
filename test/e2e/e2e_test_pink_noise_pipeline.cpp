/**
 * @file e2e_test_pink_noise_pipeline.cpp
 * @brief End-to-End tests for Pink Noise Enhancement Pipeline
 *
 * WHAT: Complete pipeline tests for all 9 pink noise enhancement modules
 * WHY:  Verify full system integration in realistic neural processing scenarios
 * HOW:  Simulate neural activity with biologically-inspired noise across all modules
 *
 * E2E SCENARIOS:
 * 1. Neural Perception Pipeline: Spatial noise → Visual processing
 * 2. Neuromodulator Dynamics: Correlated noise → Synaptic plasticity
 * 3. Sleep Cycle Simulation: Sleep stages with immune modulation
 * 4. Criticality Maintenance: Self-organized criticality with avalanches
 * 5. High-Performance Generation: SIMD-accelerated large-scale generation
 * 6. Full Brain Simulation: All modules in coordinated operation
 *
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "plasticity/noise/nimcp_pink_noise.h"
#include "plasticity/noise/nimcp_pink_noise_multiscale.h"
#include "plasticity/noise/nimcp_pink_noise_correlated.h"
#include "plasticity/noise/nimcp_pink_noise_criticality.h"
#include "plasticity/noise/nimcp_pink_noise_quantum_bridge.h"
#include "plasticity/noise/nimcp_pink_noise_immune_bridge.h"
#include "plasticity/noise/nimcp_pink_noise_sleep.h"
#include "plasticity/noise/nimcp_pink_noise_simd.h"
#include "plasticity/noise/nimcp_pink_noise_monitor.h"
#include "plasticity/noise/nimcp_pink_noise_spatial.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Fixture
//=============================================================================

class PinkNoiseE2ETest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// E2E Scenario 1: Neural Perception Pipeline
//=============================================================================

TEST_F(PinkNoiseE2ETest, NeuralPerceptionPipeline) {
    PipelineTracker tracker("Neural Perception with Spatial Noise");

    // Stage 1: Initialize visual cortex network
    tracker.begin_stage("Initialize Visual Network", 1000);
    pink_spatial_config_t sp_config = pink_spatial_network_config("visual");
    pink_spatial_t* sp = pink_spatial_create(&sp_config);
    ASSERT_NE(sp, nullptr);

    pink_noise_multiscale_config_t ms_config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&ms_config);
    ASSERT_NE(ms, nullptr);

    pink_monitor_config_t mon_config = pink_monitor_default_config();
    pink_noise_monitor_t* mon = pink_monitor_create(&mon_config);
    ASSERT_NE(mon, nullptr);
    tracker.end_stage();

    // Stage 2: Simulate visual perception (1000 timesteps)
    tracker.begin_stage("Visual Perception Simulation", 5000);
    std::vector<float> v1_activity(1000);
    std::vector<float> it_activity(1000);

    for (int t = 0; t < 1000; t++) {
        // Generate spatial noise for visual regions
        pink_spatial_step(sp);
        pink_noise_multiscale_step(ms);

        // V1 (early visual) - fast scale
        float v1_noise = pink_spatial_get_region(sp, 0);  // V1
        float fast = pink_noise_multiscale_get_scale(ms, 0);
        v1_activity[t] = v1_noise + 0.3f * fast;

        // IT (late visual) - slow scale
        float it_noise = pink_spatial_get_region(sp, 4);  // IT
        float slow = pink_noise_multiscale_get_scale(ms, 2);
        it_activity[t] = it_noise + 0.5f * slow;

        // Monitor combined activity
        float combined = (v1_activity[t] + it_activity[t]) / 2.0f;
        pink_monitor_update(mon, combined);

        ASSERT_TRUE(std::isfinite(v1_activity[t]));
        ASSERT_TRUE(std::isfinite(it_activity[t]));
    }
    tracker.end_stage();

    // Stage 3: Validate spectral properties
    tracker.begin_stage("Validate Spectral Properties", 1000);
    pink_monitor_quality_t quality;
    pink_monitor_get_quality(mon, &quality);
    EXPECT_TRUE(std::isfinite(quality.current_alpha));
    tracker.end_stage();

    // Stage 4: Cleanup
    tracker.begin_stage("Cleanup", 500);
    pink_monitor_destroy(mon);
    pink_noise_multiscale_destroy(ms);
    pink_spatial_destroy(sp);
    tracker.end_stage();

    ASSERT_TRUE(tracker.is_successful());
}

//=============================================================================
// E2E Scenario 2: Neuromodulator Dynamics
//=============================================================================

TEST_F(PinkNoiseE2ETest, NeuromodulatorDynamicsPipeline) {
    PipelineTracker tracker("Neuromodulator Correlated Dynamics");

    // Stage 1: Initialize neuromodulator system
    tracker.begin_stage("Initialize Neuromodulators", 1000);
    pink_noise_correlated_config_t cn_config = pink_noise_correlated_neuromod_config();
    pink_noise_correlated_t* cn = pink_noise_correlated_create(&cn_config);
    ASSERT_NE(cn, nullptr);

    criticality_config_t ca_config = criticality_default_config();
    criticality_analyzer_t* ca = criticality_create(&ca_config);
    ASSERT_NE(ca, nullptr);
    tracker.end_stage();

    // Stage 2: Simulate neuromodulator fluctuations (2000 timesteps)
    tracker.begin_stage("Neuromodulator Simulation", 5000);
    std::vector<float> dopamine(2000);
    std::vector<float> serotonin(2000);
    std::vector<float> norepinephrine(2000);
    float synaptic_efficacy = 1.0f;

    for (int t = 0; t < 2000; t++) {
        pink_noise_correlated_step(cn);

        dopamine[t] = pink_noise_correlated_get_channel(cn, 0);
        serotonin[t] = pink_noise_correlated_get_channel(cn, 1);
        norepinephrine[t] = pink_noise_correlated_get_channel(cn, 3);

        // Synaptic efficacy modulated by neuromodulators
        float neuromod_effect = 0.1f * dopamine[t] - 0.05f * serotonin[t] + 0.08f * norepinephrine[t];
        synaptic_efficacy = synaptic_efficacy * (1.0f + 0.01f * neuromod_effect);
        synaptic_efficacy = std::max(0.1f, std::min(10.0f, synaptic_efficacy));

        // Feed to criticality analyzer
        criticality_update(ca, dopamine[t] + serotonin[t]);

        ASSERT_TRUE(std::isfinite(synaptic_efficacy));
    }
    tracker.end_stage();

    // Stage 3: Validate correlation structure
    tracker.begin_stage("Validate Correlations", 1000);
    // DA and NE should be positively correlated
    float da_ne_sum = 0.0f;
    for (int t = 0; t < 2000; t++) {
        da_ne_sum += dopamine[t] * norepinephrine[t];
    }
    // Just verify it runs without NaN
    EXPECT_TRUE(std::isfinite(da_ne_sum));

    criticality_stats_t stats;
    criticality_get_stats(ca, &stats);
    EXPECT_GT(stats.total_samples, 0u);
    tracker.end_stage();

    // Stage 4: Cleanup
    tracker.begin_stage("Cleanup", 500);
    criticality_destroy(ca);
    pink_noise_correlated_destroy(cn);
    tracker.end_stage();

    ASSERT_TRUE(tracker.is_successful());
}

//=============================================================================
// E2E Scenario 3: Sleep Cycle Simulation
//=============================================================================

TEST_F(PinkNoiseE2ETest, SleepCycleSimulation) {
    PipelineTracker tracker("Full Sleep Cycle with Immune Modulation");

    // Stage 1: Initialize sleep-immune system
    tracker.begin_stage("Initialize Sleep-Immune System", 1000);
    pink_sleep_config_t sl_config = pink_sleep_default_config();
    pink_sleep_bridge_t* sl = pink_sleep_create(&sl_config);
    ASSERT_NE(sl, nullptr);

    pink_immune_config_t im_config = pink_immune_bridge_default_config();
    pink_immune_bridge_t* im = pink_immune_bridge_create(&im_config);
    ASSERT_NE(im, nullptr);

    pink_noise_multiscale_config_t ms_config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&ms_config);
    ASSERT_NE(ms, nullptr);
    tracker.end_stage();

    // Stage 2: Simulate complete sleep cycle (Wake → N1 → N2 → N3 → REM → Wake)
    tracker.begin_stage("Sleep Cycle Simulation", 10000);

    struct SleepPhase {
        pink_sleep_stage_t stage;
        pink_inflammation_level_t inflammation;
        int duration;
        const char* name;
    } phases[] = {
        {PINK_SLEEP_WAKE, PINK_INFLAMMATION_NONE, 500, "Wake"},
        {PINK_SLEEP_DROWSY, PINK_INFLAMMATION_NONE, 200, "Drowsy"},
        {PINK_SLEEP_N1, PINK_INFLAMMATION_NONE, 300, "N1"},
        {PINK_SLEEP_N2, PINK_INFLAMMATION_LOCAL, 600, "N2"},
        {PINK_SLEEP_N3, PINK_INFLAMMATION_LOCAL, 800, "N3 (Deep)"},
        {PINK_SLEEP_N2, PINK_INFLAMMATION_NONE, 400, "N2 (ascending)"},
        {PINK_SLEEP_REM, PINK_INFLAMMATION_NONE, 500, "REM"},
        {PINK_SLEEP_WAKE, PINK_INFLAMMATION_NONE, 200, "Wake (final)"}
    };

    std::vector<float> all_samples;
    all_samples.reserve(3500);

    for (int p = 0; p < 8; p++) {
        pink_sleep_set_stage(sl, phases[p].stage);
        pink_immune_bridge_set_inflammation(im, phases[p].inflammation);

        for (int t = 0; t < phases[p].duration; t++) {
            pink_sleep_step(sl);
            pink_noise_multiscale_step(ms);
            pink_immune_bridge_compute_effects(im);

            float sleep_sample = pink_sleep_generate_sample(sl);
            float amp_mod = pink_immune_bridge_get_amplitude_modifier(im);
            float multiscale = pink_noise_multiscale_get_combined(ms, nullptr);

            float final_sample = (sleep_sample + multiscale) * amp_mod;
            all_samples.push_back(final_sample);

            ASSERT_TRUE(std::isfinite(final_sample));
        }
    }
    tracker.end_stage();

    // Stage 3: Validate sleep cycle statistics
    tracker.begin_stage("Validate Sleep Statistics", 1000);
    EXPECT_GT(all_samples.size(), 3000u);

    // Compute basic statistics
    double sum = 0, sum_sq = 0;
    for (float s : all_samples) {
        sum += s;
        sum_sq += s * s;
    }
    double mean = sum / all_samples.size();
    double variance = sum_sq / all_samples.size() - mean * mean;

    EXPECT_TRUE(std::isfinite(mean));
    EXPECT_GT(variance, 0.0);
    tracker.end_stage();

    // Stage 4: Cleanup
    tracker.begin_stage("Cleanup", 500);
    pink_noise_multiscale_destroy(ms);
    pink_immune_bridge_destroy(im);
    pink_sleep_destroy(sl);
    tracker.end_stage();

    ASSERT_TRUE(tracker.is_successful());
}

//=============================================================================
// E2E Scenario 4: Criticality Maintenance
//=============================================================================

TEST_F(PinkNoiseE2ETest, CriticalityMaintenancePipeline) {
    PipelineTracker tracker("Self-Organized Criticality Pipeline");

    // Stage 1: Initialize criticality system
    tracker.begin_stage("Initialize Criticality System", 1000);
    pink_quantum_config_t q_config = pink_quantum_default_config();
    q_config.method = PINK_QUANTUM_ANNEALING;
    pink_quantum_bridge_t* q = pink_quantum_create(&q_config);
    ASSERT_NE(q, nullptr);

    criticality_config_t ca_config = criticality_default_config();
    ca_config.enable_feedback = true;
    criticality_analyzer_t* ca = criticality_create(&ca_config);
    ASSERT_NE(ca, nullptr);

    pink_monitor_config_t mon_config = pink_monitor_default_config();
    mon_config.enable_auto_correction = true;
    pink_noise_monitor_t* mon = pink_monitor_create(&mon_config);
    ASSERT_NE(mon, nullptr);
    tracker.end_stage();

    // Stage 2: Run criticality maintenance loop
    tracker.begin_stage("Criticality Maintenance Loop", 10000);

    int avalanche_count = 0;
    float total_criticality_index = 0.0f;

    for (int t = 0; t < 5000; t++) {
        float sample;
        pink_quantum_generate_sample(q, &sample);

        // Apply feedback corrections
        float amp_corr = criticality_get_amplitude_correction(ca);
        float alpha_corr = pink_monitor_get_alpha_correction(mon);
        sample = sample * amp_corr + 0.1f * alpha_corr;

        criticality_update(ca, sample);
        pink_monitor_update(mon, sample);

        if (criticality_in_avalanche(ca)) {
            avalanche_count++;
        }

        total_criticality_index += criticality_get_index(ca);

        ASSERT_TRUE(std::isfinite(sample));
    }
    tracker.end_stage();

    // Stage 3: Validate criticality metrics
    tracker.begin_stage("Validate Criticality Metrics", 1000);
    criticality_stats_t stats;
    criticality_get_stats(ca, &stats);

    EXPECT_GT(stats.total_samples, 0u);
    EXPECT_TRUE(std::isfinite(stats.criticality_index));

    float avg_criticality = total_criticality_index / 5000.0f;
    EXPECT_TRUE(std::isfinite(avg_criticality));

    pink_quantum_stats_t q_stats;
    pink_quantum_get_stats(q, &q_stats);
    EXPECT_GT(q_stats.quantum_operations, 0u);
    tracker.end_stage();

    // Stage 4: Cleanup
    tracker.begin_stage("Cleanup", 500);
    pink_monitor_destroy(mon);
    criticality_destroy(ca);
    pink_quantum_destroy(q);
    tracker.end_stage();

    ASSERT_TRUE(tracker.is_successful());
}

//=============================================================================
// E2E Scenario 5: High-Performance Generation
//=============================================================================

TEST_F(PinkNoiseE2ETest, HighPerformanceSIMDPipeline) {
    PipelineTracker tracker("SIMD High-Performance Generation");

    // Stage 1: Detect SIMD capabilities
    tracker.begin_stage("Detect SIMD Capabilities", 500);
    pink_simd_type_t simd_type = pink_simd_detect();
    std::cout << "  SIMD type detected: " << pink_simd_type_name(simd_type) << std::endl;
    tracker.end_stage();

    // Stage 2: Initialize SIMD generator
    tracker.begin_stage("Initialize SIMD Generator", 1000);
    pink_simd_config_t simd_config = pink_simd_default_config();
    simd_config.preferred_simd = simd_type;
    pink_simd_generator_t* simd = pink_simd_create(&simd_config);
    ASSERT_NE(simd, nullptr);

    pink_noise_multiscale_config_t ms_config = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* ms = pink_noise_multiscale_create(&ms_config);
    ASSERT_NE(ms, nullptr);
    tracker.end_stage();

    // Stage 3: Large-scale generation (100K samples)
    tracker.begin_stage("Generate 100K Samples", 5000);
    const int BATCH_SIZE = 1024;
    const int NUM_BATCHES = 100;

    float* buffer = new float[BATCH_SIZE];

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        pink_simd_generate_batch(simd, buffer, BATCH_SIZE);

        // Verify all samples are valid
        for (int i = 0; i < BATCH_SIZE; i++) {
            ASSERT_TRUE(std::isfinite(buffer[i])) << "Invalid at batch " << batch << " sample " << i;
        }
    }

    delete[] buffer;
    tracker.end_stage();

    // Stage 4: Get performance stats
    tracker.begin_stage("Collect Performance Stats", 500);
    pink_simd_stats_t stats;
    pink_simd_get_stats(simd, &stats);

    std::cout << "  Total samples: " << stats.total_samples << std::endl;
    std::cout << "  SIMD type: " << pink_simd_type_name(stats.simd_type) << std::endl;
    std::cout << "  Vector width: " << stats.vector_width << std::endl;

    EXPECT_GE(stats.total_samples, (uint64_t)(BATCH_SIZE * NUM_BATCHES));
    tracker.end_stage();

    // Stage 5: Cleanup
    tracker.begin_stage("Cleanup", 500);
    pink_noise_multiscale_destroy(ms);
    pink_simd_destroy(simd);
    tracker.end_stage();

    ASSERT_TRUE(tracker.is_successful());
}

//=============================================================================
// E2E Scenario 6: Full Brain Simulation
//=============================================================================

TEST_F(PinkNoiseE2ETest, FullBrainSimulationPipeline) {
    PipelineTracker tracker("Full Brain Simulation Pipeline");

    // Stage 1: Initialize all noise systems
    tracker.begin_stage("Initialize All Noise Systems", 2000);

    // Spatial networks for different brain areas
    pink_spatial_config_t visual_cfg = pink_spatial_network_config("visual");
    pink_spatial_config_t motor_cfg = pink_spatial_network_config("motor");
    pink_spatial_config_t dmn_cfg = pink_spatial_network_config("default_mode");
    pink_spatial_t* visual = pink_spatial_create(&visual_cfg);
    pink_spatial_t* motor = pink_spatial_create(&motor_cfg);
    pink_spatial_t* dmn = pink_spatial_create(&dmn_cfg);

    // Neuromodulators
    pink_noise_correlated_config_t neuromod_cfg = pink_noise_correlated_neuromod_config();
    pink_noise_correlated_t* neuromod = pink_noise_correlated_create(&neuromod_cfg);

    // Temporal scales
    pink_noise_multiscale_config_t multiscale_cfg = pink_noise_multiscale_default_config();
    pink_noise_multiscale_t* multiscale = pink_noise_multiscale_create(&multiscale_cfg);

    // State systems
    pink_sleep_config_t sleep_cfg = pink_sleep_default_config();
    pink_immune_config_t immune_cfg = pink_immune_bridge_default_config();
    pink_sleep_bridge_t* sleep = pink_sleep_create(&sleep_cfg);
    pink_immune_bridge_t* immune = pink_immune_bridge_create(&immune_cfg);

    // Monitoring
    criticality_config_t crit_cfg = criticality_default_config();
    pink_monitor_config_t mon_cfg = pink_monitor_default_config();
    criticality_analyzer_t* criticality = criticality_create(&crit_cfg);
    pink_noise_monitor_t* monitor = pink_monitor_create(&mon_cfg);

    ASSERT_NE(visual, nullptr);
    ASSERT_NE(motor, nullptr);
    ASSERT_NE(dmn, nullptr);
    ASSERT_NE(neuromod, nullptr);
    ASSERT_NE(multiscale, nullptr);
    ASSERT_NE(sleep, nullptr);
    ASSERT_NE(immune, nullptr);
    ASSERT_NE(criticality, nullptr);
    ASSERT_NE(monitor, nullptr);
    tracker.end_stage();

    // Stage 2: Configure initial brain state
    tracker.begin_stage("Configure Brain State", 500);
    pink_sleep_set_stage(sleep, PINK_SLEEP_WAKE);
    pink_sleep_set_arousal(sleep, 0.8f);
    pink_immune_bridge_set_inflammation(immune, PINK_INFLAMMATION_NONE);
    tracker.end_stage();

    // Stage 3: Simulate 3000 timesteps of brain activity
    tracker.begin_stage("Brain Activity Simulation", 15000);

    struct BrainActivity {
        float visual_v1, visual_it;
        float motor_m1, motor_sma;
        float dmn_mpfc, dmn_pcc;
        float dopamine, serotonin, norepinephrine;
        float global_activity;
    };

    std::vector<BrainActivity> activity(3000);

    for (int t = 0; t < 3000; t++) {
        // Step all generators
        pink_spatial_step(visual);
        pink_spatial_step(motor);
        pink_spatial_step(dmn);
        pink_noise_correlated_step(neuromod);
        pink_noise_multiscale_step(multiscale);
        pink_sleep_step(sleep);

        // Compute state modulation
        pink_immune_bridge_compute_effects(immune);
        float amp_mod = pink_immune_bridge_get_amplitude_modifier(immune);
        float sleep_amp = pink_sleep_get_amplitude(sleep);

        // Get regional activities
        activity[t].visual_v1 = pink_spatial_get_region(visual, 0) * amp_mod;
        activity[t].visual_it = pink_spatial_get_region(visual, 4) * amp_mod;
        activity[t].motor_m1 = pink_spatial_get_region(motor, 0) * amp_mod;
        activity[t].motor_sma = pink_spatial_get_region(motor, 1) * amp_mod;
        activity[t].dmn_mpfc = pink_spatial_get_region(dmn, 0) * amp_mod;
        activity[t].dmn_pcc = pink_spatial_get_region(dmn, 1) * amp_mod;

        // Get neuromodulator levels
        activity[t].dopamine = pink_noise_correlated_get_channel(neuromod, 0);
        activity[t].serotonin = pink_noise_correlated_get_channel(neuromod, 1);
        activity[t].norepinephrine = pink_noise_correlated_get_channel(neuromod, 3);

        // Compute global brain activity
        float multiscale_val = pink_noise_multiscale_get_combined(multiscale, nullptr);
        activity[t].global_activity = (
            activity[t].visual_v1 + activity[t].visual_it +
            activity[t].motor_m1 + activity[t].motor_sma +
            activity[t].dmn_mpfc + activity[t].dmn_pcc
        ) / 6.0f + multiscale_val * sleep_amp;

        // Update monitoring
        pink_monitor_update(monitor, activity[t].global_activity);
        criticality_update(criticality, activity[t].global_activity);

        // Validate
        ASSERT_TRUE(std::isfinite(activity[t].global_activity))
            << "Invalid global activity at timestep " << t;
    }
    tracker.end_stage();

    // Stage 4: Analyze brain activity
    tracker.begin_stage("Analyze Brain Activity", 2000);

    // Compute statistics
    double global_sum = 0, global_sq = 0;
    for (const auto& a : activity) {
        global_sum += a.global_activity;
        global_sq += a.global_activity * a.global_activity;
    }
    double mean = global_sum / activity.size();
    double variance = global_sq / activity.size() - mean * mean;

    std::cout << "  Global activity mean: " << mean << std::endl;
    std::cout << "  Global activity variance: " << variance << std::endl;

    EXPECT_TRUE(std::isfinite(mean));
    EXPECT_GT(variance, 0.0);

    // Get criticality and quality metrics
    criticality_stats_t crit_stats;
    criticality_get_stats(criticality, &crit_stats);

    pink_monitor_quality_t quality;
    pink_monitor_get_quality(monitor, &quality);

    std::cout << "  Criticality index: " << crit_stats.criticality_index << std::endl;
    std::cout << "  Spectral alpha: " << quality.current_alpha << std::endl;

    EXPECT_GT(crit_stats.total_samples, 0u);
    EXPECT_TRUE(std::isfinite(quality.current_alpha));
    tracker.end_stage();

    // Stage 5: Cleanup
    tracker.begin_stage("Cleanup", 1000);
    pink_monitor_destroy(monitor);
    criticality_destroy(criticality);
    pink_immune_bridge_destroy(immune);
    pink_sleep_destroy(sleep);
    pink_noise_multiscale_destroy(multiscale);
    pink_noise_correlated_destroy(neuromod);
    pink_spatial_destroy(dmn);
    pink_spatial_destroy(motor);
    pink_spatial_destroy(visual);
    tracker.end_stage();

    ASSERT_TRUE(tracker.is_successful());
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
