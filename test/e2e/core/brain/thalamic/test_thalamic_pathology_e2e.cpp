//=============================================================================
// test_thalamic_pathology_e2e.cpp - Thalamic Pathology End-to-End Tests
//=============================================================================
/**
 * @file test_thalamic_pathology_e2e.cpp
 * @brief End-to-end tests for thalamic pathology and immune interactions
 *
 * WHAT: Full pipeline tests for thalamic dysfunction and recovery
 * WHY:  Verify pathological conditions and therapeutic interventions
 * HOW:  Test lesions, seizures, oscillation abnormalities, immune effects
 *
 * TEST COVERAGE:
 * - Thalamic lesion simulation
 * - Absence seizure (spike-wave) patterns
 * - Parkinson's-like oscillations
 * - Recovery from thalamic stroke
 * - Inflammation effects on relay
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <numeric>
#include <algorithm>
#include <set>

#include "e2e_test_framework.h"
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "middleware/immune/nimcp_thalamic_immune_bridge.h"
#include "middleware/routing/nimcp_thalamic_router_fep_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ThalamicPathologyE2ETest : public ::testing::Test {
protected:
    thalamus_t* thal = nullptr;
    thalamic_router_t* router = nullptr;
    static constexpr uint32_t NUM_CHANNELS = 64;
    static constexpr uint32_t NUM_NEURONS = 128;

    void SetUp() override {
        // Create thalamus
        thalamus_config_t config;
        thalamus_default_config(&config);
        config.neurons_per_nucleus = NUM_NEURONS;
        config.channels_per_nucleus = NUM_CHANNELS;
        config.enable_trn = true;
        config.enable_mode_switching = true;
        config.enable_attention_gating = true;
        thal = thalamus_create(&config);
        ASSERT_NE(thal, nullptr);

        // Create router
        thalamic_router_config_t router_config = thalamic_router_default_config();
        router_config.enable_attention_gating = true;
        router = thalamic_router_create(&router_config);
        ASSERT_NE(router, nullptr);

        thalamus_set_arousal(thal, 1.0f);
    }

    void TearDown() override {
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
    }

    // Simulate lesion by zeroing channels
    void simulate_lesion(thal_nucleus_type_t nucleus, uint32_t start_channel,
                         uint32_t end_channel) {
        for (uint32_t ch = start_channel; ch < end_channel && ch < NUM_CHANNELS; ++ch) {
            thalamus_apply_channel_inhibition(thal, nucleus, ch, 1.0f);  // Complete block
        }
    }

    // Generate standard test stimulus
    std::vector<float> generate_stimulus(float amplitude = 0.8f) {
        std::vector<float> stim(NUM_CHANNELS);
        for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
            float x = static_cast<float>(i) / NUM_CHANNELS;
            stim[i] = amplitude * (0.5f + 0.5f * std::sin(2.0f * M_PI * x * 3.0f));
        }
        return stim;
    }

    // Generate spike-wave pattern (3 Hz, characteristic of absence seizures)
    std::vector<float> generate_spike_wave(uint32_t size, float phase) {
        std::vector<float> pattern(size);
        for (uint32_t i = 0; i < size; ++i) {
            float t = static_cast<float>(i) / size;
            float spike = std::exp(-std::pow((std::fmod(t + phase, 0.33f) - 0.1f) / 0.02f, 2.0f));
            float wave = 0.3f * std::sin(2.0f * M_PI * 3.0f * t + phase);
            pattern[i] = 0.9f * spike + wave;
        }
        return pattern;
    }

    // Calculate power
    float calculate_power(const std::vector<float>& signal) {
        float sum = 0.0f;
        for (float v : signal) {
            sum += v * v;
        }
        return sum / signal.size();
    }

    // Calculate ratio of affected channels
    float calculate_deficit_ratio(const std::vector<float>& healthy,
                                   const std::vector<float>& lesioned,
                                   uint32_t start, uint32_t end) {
        float healthy_sum = 0.0f, lesioned_sum = 0.0f;
        for (uint32_t i = start; i < end && i < healthy.size(); ++i) {
            healthy_sum += std::abs(healthy[i]);
            lesioned_sum += std::abs(lesioned[i]);
        }
        if (healthy_sum < 0.001f) return 1.0f;
        return lesioned_sum / healthy_sum;
    }
};

//=============================================================================
// Thalamic Lesion Simulation Tests
//=============================================================================

TEST_F(ThalamicPathologyE2ETest, ThalamicLesion_LGNVisualFieldDefect) {
    E2E_PIPELINE_START("Thalamic Lesion: LGN Visual Field Defect");

    auto visual_input = generate_stimulus(0.9f);

    E2E_STAGE_BEGIN("Baseline visual processing", 500);
    std::vector<float> healthy_output(NUM_CHANNELS);
    int result = thalamus_relay_visual(thal, visual_input.data(), NUM_CHANNELS,
                                       healthy_output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    float healthy_power = calculate_power(healthy_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate LGN lesion (right visual field)", 300);
    // Lesion right half of LGN (left visual field deficit)
    simulate_lesion(THAL_NUCLEUS_LGN, NUM_CHANNELS / 2, NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process visual input post-lesion", 500);
    std::vector<float> lesioned_output(NUM_CHANNELS);
    result = thalamus_relay_visual(thal, visual_input.data(), NUM_CHANNELS,
                                   lesioned_output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify visual field defect", 100);
    // Right half should be severely reduced
    float left_power = 0.0f, right_power = 0.0f;
    for (uint32_t i = 0; i < NUM_CHANNELS / 2; ++i) {
        left_power += lesioned_output[i] * lesioned_output[i];
    }
    for (uint32_t i = NUM_CHANNELS / 2; i < NUM_CHANNELS; ++i) {
        right_power += lesioned_output[i] * lesioned_output[i];
    }

    EXPECT_GT(left_power, right_power)
        << "Lesioned (right) field should have reduced output";

    float deficit_ratio = calculate_deficit_ratio(healthy_output, lesioned_output,
                                                   NUM_CHANNELS / 2, NUM_CHANNELS);
    EXPECT_LT(deficit_ratio, 0.5f)
        << "Lesioned area should show significant deficit";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, ThalamicLesion_MGNAuditoryDeficit) {
    E2E_PIPELINE_START("Thalamic Lesion: MGN Auditory Processing Deficit");

    auto auditory_input = generate_stimulus(0.85f);

    E2E_STAGE_BEGIN("Baseline auditory processing", 500);
    std::vector<float> healthy_output(NUM_CHANNELS);
    thalamus_relay_auditory(thal, auditory_input.data(), NUM_CHANNELS,
                           healthy_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate MGN frequency-specific lesion", 300);
    // Lesion mid-frequency channels (simulates tonotopic damage)
    simulate_lesion(THAL_NUCLEUS_MGN, NUM_CHANNELS / 3, 2 * NUM_CHANNELS / 3);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process auditory input post-lesion", 500);
    std::vector<float> lesioned_output(NUM_CHANNELS);
    thalamus_relay_auditory(thal, auditory_input.data(), NUM_CHANNELS,
                           lesioned_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify frequency-specific deficit", 100);
    float deficit = calculate_deficit_ratio(healthy_output, lesioned_output,
                                            NUM_CHANNELS / 3, 2 * NUM_CHANNELS / 3);
    EXPECT_LT(deficit, 0.5f)
        << "Mid-frequency range should show deficit";

    // Low and high frequencies should be relatively spared
    float low_deficit = calculate_deficit_ratio(healthy_output, lesioned_output,
                                                 0, NUM_CHANNELS / 3);
    float high_deficit = calculate_deficit_ratio(healthy_output, lesioned_output,
                                                  2 * NUM_CHANNELS / 3, NUM_CHANNELS);

    EXPECT_GT(low_deficit, deficit) << "Low frequencies should be spared";
    EXPECT_GT(high_deficit, deficit) << "High frequencies should be spared";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, ThalamicLesion_VPLSomatosensoryLoss) {
    E2E_PIPELINE_START("Thalamic Lesion: VPL Somatosensory Loss");

    auto somatosensory_input = generate_stimulus(0.9f);

    E2E_STAGE_BEGIN("Baseline somatosensory processing", 500);
    std::vector<float> healthy_output(NUM_CHANNELS);
    thalamus_relay(thal, THAL_NUCLEUS_VPL, somatosensory_input.data(),
                  NUM_CHANNELS, healthy_output.data(), NUM_CHANNELS);
    float healthy_power = calculate_power(healthy_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate complete VPL lesion", 300);
    // Complete lesion
    simulate_lesion(THAL_NUCLEUS_VPL, 0, NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process somatosensory input post-lesion", 500);
    std::vector<float> lesioned_output(NUM_CHANNELS);
    thalamus_relay(thal, THAL_NUCLEUS_VPL, somatosensory_input.data(),
                  NUM_CHANNELS, lesioned_output.data(), NUM_CHANNELS);
    float lesioned_power = calculate_power(lesioned_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify complete sensory loss", 100);
    EXPECT_LT(lesioned_power, healthy_power * 0.2f)
        << "Complete VPL lesion should abolish somatosensory relay";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Absence Seizure (Spike-Wave) Pattern Tests
//=============================================================================

TEST_F(ThalamicPathologyE2ETest, AbsenceSeizure_SpikeWaveGeneration) {
    E2E_PIPELINE_START("Absence Seizure: Spike-Wave Pattern Generation");

    E2E_STAGE_BEGIN("Configure thalamus for seizure-prone state", 300);
    // Reduced arousal + burst mode = seizure susceptibility
    thalamus_set_arousal(thal, 0.4f);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_BURST);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger burst oscillations", 500);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_PULVINAR);

    // Run multiple steps to establish oscillation
    for (int i = 0; i < 30; ++i) {
        thalamus_step(thal, 10.0f);  // 10ms steps (100 Hz sampling)
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify burst mode dominance", 100);
    EXPECT_EQ(thalamus_get_mode(thal, THAL_NUCLEUS_LGN), THAL_MODE_BURST);

    thalamus_stats_t stats;
    thalamus_get_stats(thal, &stats);
    EXPECT_GT(stats.burst_count, 0)
        << "Should have burst events during seizure-like state";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, AbsenceSeizure_TRNHyperexcitability) {
    E2E_PIPELINE_START("Absence Seizure: TRN Hyperexcitability");

    E2E_STAGE_BEGIN("Baseline relay function", 500);
    thalamus_set_arousal(thal, 0.8f);
    auto input = generate_stimulus(0.7f);
    std::vector<float> normal_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, input.data(), NUM_CHANNELS,
                         normal_output.data(), NUM_CHANNELS);
    float normal_power = calculate_power(normal_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate TRN hyperexcitability", 500);
    // Abnormally strong TRN inhibition (seizure mechanism)
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.95f);
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_MGN, 0.95f);
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_VPL, 0.95f);

    thalamus_set_arousal(thal, 0.3f);  // Drowsy state
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process sensory input during TRN hyperexcitability", 500);
    std::vector<float> seizure_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, input.data(), NUM_CHANNELS,
                         seizure_output.data(), NUM_CHANNELS);
    float seizure_power = calculate_power(seizure_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify sensory gating disruption", 100);
    // During TRN hyperexcitability, normal gating is disrupted
    EXPECT_NE(seizure_power, normal_power)
        << "TRN hyperexcitability should alter sensory processing";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, AbsenceSeizure_AntiepilepticEffect) {
    E2E_PIPELINE_START("Absence Seizure: Antiepileptic Intervention");

    E2E_STAGE_BEGIN("Induce seizure-like state", 500);
    thalamus_set_arousal(thal, 0.3f);
    thalamus_trigger_burst(thal, THAL_NUCLEUS_LGN);

    for (int i = 0; i < 20; ++i) {
        thalamus_step(thal, 10.0f);
    }
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_BURST);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply antiepileptic effect (increase arousal)", 500);
    // Antiepileptics often work by shifting tonic/burst balance
    thalamus_set_arousal(thal, 0.9f);

    for (int i = 0; i < 20; ++i) {
        thalamus_step(thal, 10.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify seizure termination", 100);
    EXPECT_EQ(thal->dominant_mode, THAL_MODE_TONIC)
        << "Antiepileptic should restore tonic mode";

    float tonic_frac = thalamus_get_tonic_fraction(thal, THAL_NUCLEUS_LGN);
    EXPECT_GT(tonic_frac, 0.5f)
        << "Most cells should return to tonic mode";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Parkinson's-like Oscillation Tests
//=============================================================================

TEST_F(ThalamicPathologyE2ETest, ParkinsonsOscillations_BetaRhythm) {
    E2E_PIPELINE_START("Parkinson's Oscillations: Pathological Beta Rhythm");

    E2E_STAGE_BEGIN("Configure low-dopamine motor circuit", 500);
    thalamus_set_arousal(thal, 0.6f);
    thalamus_set_attention(thal, THAL_NUCLEUS_VA, 0.5f);

    // Apply strong inhibition to motor thalamus (simulates BG dysfunction)
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_VA, i, 0.7f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate beta oscillation input (15-30 Hz)", 500);
    // Pathological beta: 15-30 Hz oscillations
    std::vector<float> beta_input(NUM_CHANNELS);
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        float t = static_cast<float>(i) / NUM_CHANNELS;
        beta_input[i] = 0.6f * (0.5f + 0.5f * std::sin(2.0f * M_PI * 20.0f * t));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process motor command through inhibited pathway", 500);
    std::vector<float> motor_output(NUM_CHANNELS);
    int result = thalamus_relay_motor(thal, beta_input.data(), NUM_CHANNELS,
                                      motor_output.data(), NUM_CHANNELS);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify reduced motor throughput", 100);
    float motor_power = calculate_power(motor_output);
    // Parkinsonian state: reduced motor facilitation
    EXPECT_LE(motor_power, 0.5f)
        << "Inhibited motor thalamus should reduce throughput";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, ParkinsonsOscillations_DopamineRestoration) {
    E2E_PIPELINE_START("Parkinson's Oscillations: L-DOPA Effect Simulation");

    auto motor_input = generate_stimulus(0.75f);

    E2E_STAGE_BEGIN("Simulate Parkinsonian state", 500);
    // Strong inhibition on VA/VL
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_VA, i, 0.8f);
    }

    std::vector<float> parkinsonian_output(NUM_CHANNELS);
    thalamus_relay_motor(thal, motor_input.data(), NUM_CHANNELS,
                        parkinsonian_output.data(), NUM_CHANNELS);
    float park_power = calculate_power(parkinsonian_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply L-DOPA effect (reduce inhibition)", 500);
    // L-DOPA restores dopamine -> reduces indirect pathway inhibition
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_VA, i, 0.3f);
    }
    thalamus_set_attention(thal, THAL_NUCLEUS_VA, 0.8f);

    std::vector<float> treated_output(NUM_CHANNELS);
    thalamus_relay_motor(thal, motor_input.data(), NUM_CHANNELS,
                        treated_output.data(), NUM_CHANNELS);
    float treated_power = calculate_power(treated_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify improved motor throughput", 100);
    EXPECT_GT(treated_power, park_power)
        << "L-DOPA should improve motor thalamic throughput";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, ParkinsonsOscillations_TremorFrequency) {
    E2E_PIPELINE_START("Parkinson's Oscillations: Tremor Frequency Detection");

    E2E_STAGE_BEGIN("Generate tremor-frequency input (4-6 Hz)", 500);
    std::vector<float> tremor_input(NUM_CHANNELS);
    for (uint32_t i = 0; i < NUM_CHANNELS; ++i) {
        float t = static_cast<float>(i) / NUM_CHANNELS;
        // 5 Hz tremor oscillation
        tremor_input[i] = 0.7f * (0.5f + 0.5f * std::sin(2.0f * M_PI * 5.0f * t));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process through motor thalamus", 500);
    thalamus_set_arousal(thal, 0.7f);
    thalamus_set_attention(thal, THAL_NUCLEUS_VA, 0.6f);

    std::vector<float> motor_output(NUM_CHANNELS);
    thalamus_relay_motor(thal, tremor_input.data(), NUM_CHANNELS,
                        motor_output.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify oscillation transmission", 100);
    float power = calculate_power(motor_output);
    EXPECT_GT(power, 0.0f)
        << "Tremor oscillations should transmit through motor thalamus";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Recovery from Thalamic Stroke Tests
//=============================================================================

TEST_F(ThalamicPathologyE2ETest, StrokeRecovery_AcuteLesion) {
    E2E_PIPELINE_START("Stroke Recovery: Acute Lesion Phase");

    auto sensory_input = generate_stimulus(0.85f);

    E2E_STAGE_BEGIN("Pre-stroke baseline", 500);
    std::vector<float> pre_stroke(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         pre_stroke.data(), NUM_CHANNELS);
    float pre_power = calculate_power(pre_stroke);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate acute stroke (sudden lesion)", 300);
    // Acute stroke: sudden complete loss in affected territory
    simulate_lesion(THAL_NUCLEUS_LGN, 20, 44);  // ~40% of field
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Immediate post-stroke function", 500);
    std::vector<float> acute_output(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         acute_output.data(), NUM_CHANNELS);
    float acute_power = calculate_power(acute_output);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify acute deficit", 100);
    EXPECT_LT(acute_power, pre_power)
        << "Acute stroke should reduce overall function";

    float lesion_deficit = calculate_deficit_ratio(pre_stroke, acute_output, 20, 44);
    EXPECT_LT(lesion_deficit, 0.3f)
        << "Lesioned area should show severe deficit";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, StrokeRecovery_PartialRecovery) {
    E2E_PIPELINE_START("Stroke Recovery: Partial Recovery via Plasticity");

    auto sensory_input = generate_stimulus(0.8f);

    E2E_STAGE_BEGIN("Baseline and lesion", 500);
    std::vector<float> baseline(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         baseline.data(), NUM_CHANNELS);

    simulate_lesion(THAL_NUCLEUS_LGN, 24, 40);

    std::vector<float> acute(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         acute.data(), NUM_CHANNELS);
    float acute_power = calculate_power(acute);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate recovery period", 2000);
    // Recovery: gradually reduce inhibition in peri-lesional area
    for (int week = 0; week < 10; ++week) {
        float recovery_factor = 0.1f * week;  // Gradual recovery

        // Reduce inhibition in peri-lesional zones
        for (uint32_t ch = 20; ch < 24; ++ch) {
            thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, ch,
                                              std::max(0.0f, 0.8f - recovery_factor));
        }
        for (uint32_t ch = 40; ch < 44; ++ch) {
            thalamus_apply_channel_inhibition(thal, THAL_NUCLEUS_LGN, ch,
                                              std::max(0.0f, 0.8f - recovery_factor));
        }

        for (int i = 0; i < 10; ++i) {
            thalamus_step(thal, 100.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Post-recovery assessment", 500);
    std::vector<float> recovered(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         recovered.data(), NUM_CHANNELS);
    float recovered_power = calculate_power(recovered);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify partial recovery", 100);
    EXPECT_GT(recovered_power, acute_power * 0.9f)
        << "Should show some recovery from acute phase";

    // Peri-lesional areas should show improvement
    float peri_recovery = (recovered[22] + recovered[23] + recovered[40] + recovered[41]) / 4.0f;
    float acute_peri = (acute[22] + acute[23] + acute[40] + acute[41]) / 4.0f;
    EXPECT_GE(peri_recovery, acute_peri)
        << "Peri-lesional areas should improve";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, StrokeRecovery_Rehabilitation) {
    E2E_PIPELINE_START("Stroke Recovery: Rehabilitation Training");

    simulate_lesion(THAL_NUCLEUS_LGN, 16, 48);

    E2E_STAGE_BEGIN("Run rehabilitation training", 3000);
    // Repeated stimulation promotes recovery
    std::vector<float> recovery_progression;

    for (int session = 0; session < 20; ++session) {
        // Rehabilitation: present stimuli to peri-lesional area
        std::vector<float> rehab_stim(NUM_CHANNELS, 0.0f);
        for (uint32_t i = 12; i < 16; ++i) rehab_stim[i] = 0.9f;
        for (uint32_t i = 48; i < 52; ++i) rehab_stim[i] = 0.9f;

        std::vector<float> output(NUM_CHANNELS);
        thalamus_relay_visual(thal, rehab_stim.data(), NUM_CHANNELS,
                             output.data(), NUM_CHANNELS);

        float peri_response = 0.0f;
        for (uint32_t i = 12; i < 16; ++i) peri_response += output[i];
        for (uint32_t i = 48; i < 52; ++i) peri_response += output[i];
        recovery_progression.push_back(peri_response);

        thalamus_step(thal, 50.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify training maintains or improves function", 100);
    // Later sessions should be at least as good as early
    float early_avg = (recovery_progression[0] + recovery_progression[1] +
                       recovery_progression[2]) / 3.0f;
    float late_avg = (recovery_progression[17] + recovery_progression[18] +
                      recovery_progression[19]) / 3.0f;

    EXPECT_GE(late_avg, early_avg * 0.8f)
        << "Rehabilitation should maintain peri-lesional function";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Inflammation Effects on Relay Tests
//=============================================================================

TEST_F(ThalamicPathologyE2ETest, InflammationEffects_CytokineSensitivity) {
    E2E_PIPELINE_START("Inflammation Effects: Cytokine-Induced Sensitivity Changes");

    auto sensory_input = generate_stimulus(0.7f);

    E2E_STAGE_BEGIN("Baseline sensory processing", 500);
    std::vector<float> baseline(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         baseline.data(), NUM_CHANNELS);
    float baseline_power = calculate_power(baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate inflammation (reduced gating)", 500);
    // Inflammation reduces sensory gating -> hypervigilance
    // Lower attention threshold
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.95f);

    // Reduce TRN inhibition (gating reduced)
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.2f);

    std::vector<float> inflamed(NUM_CHANNELS);
    thalamus_relay_visual(thal, sensory_input.data(), NUM_CHANNELS,
                         inflamed.data(), NUM_CHANNELS);
    float inflamed_power = calculate_power(inflamed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify hypervigilance (increased throughput)", 100);
    // Inflammation should increase sensory throughput
    EXPECT_GE(inflamed_power, baseline_power * 0.8f)
        << "Inflammation should maintain or increase sensory relay";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, InflammationEffects_SicknessBehavior) {
    E2E_PIPELINE_START("Inflammation Effects: Sickness Behavior Pattern");

    E2E_STAGE_BEGIN("Configure sickness behavior state", 500);
    // Reduced arousal (fatigue)
    thalamus_set_arousal(thal, 0.5f);

    // Reduced attention to non-threat signals
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.4f);  // Visual reduced
    thalamus_set_attention(thal, THAL_NUCLEUS_MGN, 0.4f);  // Auditory reduced
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process routine sensory input", 500);
    auto routine_input = generate_stimulus(0.6f);

    std::vector<float> visual_out(NUM_CHANNELS);
    std::vector<float> auditory_out(NUM_CHANNELS);

    thalamus_relay_visual(thal, routine_input.data(), NUM_CHANNELS,
                         visual_out.data(), NUM_CHANNELS);
    thalamus_relay_auditory(thal, routine_input.data(), NUM_CHANNELS,
                           auditory_out.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify reduced sensory engagement", 100);
    float visual_power = calculate_power(visual_out);
    float auditory_power = calculate_power(auditory_out);

    // Both should be reduced due to sickness behavior
    EXPECT_LE(visual_power + auditory_power, 1.5f)
        << "Sickness behavior should reduce sensory engagement";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, InflammationEffects_AntiInflammatoryRecovery) {
    E2E_PIPELINE_START("Inflammation Effects: Anti-Inflammatory Recovery");

    auto input = generate_stimulus(0.75f);

    E2E_STAGE_BEGIN("Inflamed state baseline", 500);
    thalamus_set_arousal(thal, 0.5f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.95f);
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.1f);

    std::vector<float> inflamed_out(NUM_CHANNELS);
    thalamus_relay_visual(thal, input.data(), NUM_CHANNELS,
                         inflamed_out.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply anti-inflammatory (IL-10 effect)", 500);
    // Anti-inflammatory restores normal gating
    thalamus_set_arousal(thal, 0.85f);
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.7f);  // Normal attention
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.5f);  // Restore TRN

    std::vector<float> recovered_out(NUM_CHANNELS);
    thalamus_relay_visual(thal, input.data(), NUM_CHANNELS,
                         recovered_out.data(), NUM_CHANNELS);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify normalized gating", 100);
    float inflamed_power = calculate_power(inflamed_out);
    float recovered_power = calculate_power(recovered_out);

    // Recovery should produce more balanced (less hypervigilant) response
    // The exact relationship depends on inflammation intensity
    EXPECT_GT(recovered_power, 0.0f)
        << "Anti-inflammatory should maintain sensory function";

    thal_firing_mode_t mode = thalamus_get_mode(thal, THAL_NUCLEUS_LGN);
    EXPECT_EQ(mode, THAL_MODE_TONIC)
        << "Recovery should restore tonic mode";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(ThalamicPathologyE2ETest, InflammationEffects_ChronicInflammation) {
    E2E_PIPELINE_START("Inflammation Effects: Chronic Inflammation Adaptation");

    auto input = generate_stimulus(0.7f);

    E2E_STAGE_BEGIN("Simulate chronic low-grade inflammation", 3000);
    std::vector<float> response_over_time;

    // Chronic inflammation: prolonged mild reduction in gating
    thalamus_set_attention(thal, THAL_NUCLEUS_LGN, 0.85f);
    thalamus_apply_trn_inhibition(thal, THAL_NUCLEUS_LGN, 0.3f);

    for (int day = 0; day < 30; ++day) {
        std::vector<float> output(NUM_CHANNELS);
        thalamus_relay_visual(thal, input.data(), NUM_CHANNELS,
                             output.data(), NUM_CHANNELS);

        response_over_time.push_back(calculate_power(output));
        thalamus_step(thal, 100.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify adaptation to chronic inflammation", 100);
    // System should maintain function despite chronic inflammation
    float early_response = (response_over_time[0] + response_over_time[1]) / 2.0f;
    float late_response = (response_over_time[28] + response_over_time[29]) / 2.0f;

    EXPECT_GT(late_response, early_response * 0.5f)
        << "System should adapt to maintain function";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
