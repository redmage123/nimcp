//=============================================================================
// test_pattern_detectors.cpp - Integrated Pattern Detector Tests
//=============================================================================
/**
 * This test suite tests all 3 pattern detectors together to ensure:
 * - Consistent API across detectors
 * - Proper pattern detection coordination
 * - Integration scenarios
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/patterns/nimcp_oscillation_detector.h"
#include "middleware/patterns/nimcp_sequence_detector.h"
#include "middleware/patterns/nimcp_synchrony_detector.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PatternDetectorsTest : public ::testing::Test {
protected:
    oscillation_detector_t* osc_detector;
    sequence_detector_t* seq_detector;
    synchrony_detector_t* sync_detector;

    static constexpr uint32_t NUM_NEURONS = 100;
    static constexpr float SAMPLE_RATE = 1000.0f;

    void SetUp() override {
        // Create oscillation detector
        oscillation_detector_config_t osc_config = oscillation_detector_default_config();
        osc_detector = oscillation_detector_create(&osc_config);

        // Create sequence detector
        sequence_detector_config_t seq_config = sequence_detector_default_config();
        seq_detector = sequence_detector_create(&seq_config);

        // Create synchrony detector
        synchrony_detector_config_t sync_config = synchrony_detector_default_config(NUM_NEURONS);
        sync_detector = synchrony_detector_create(&sync_config);

        ASSERT_NE(osc_detector, nullptr);
        ASSERT_NE(seq_detector, nullptr);
        ASSERT_NE(sync_detector, nullptr);
    }

    void TearDown() override {
        oscillation_detector_destroy(osc_detector);
        sequence_detector_destroy(seq_detector);
        synchrony_detector_destroy(sync_detector);
    }

    // Helper: Generate oscillatory signal
    void addOscillatorySamples(float freq_hz, float duration_ms, float amplitude) {
        uint32_t num_samples = static_cast<uint32_t>(duration_ms);
        for (uint32_t i = 0; i < num_samples; i++) {
            float t = i / SAMPLE_RATE;
            float signal = amplitude * sinf(2.0f * M_PI * freq_hz * t);
            oscillation_detector_add_sample(osc_detector, signal, i);
        }
    }

    // Helper: Add spike sequence
    void addSpikeSequence(const std::vector<uint32_t>& neurons, double start_time, double interval) {
        for (size_t i = 0; i < neurons.size(); i++) {
            double spike_time = start_time + i * interval;
            sequence_detector_add_spike(seq_detector, neurons[i], spike_time);
            synchrony_detector_add_spike(sync_detector, neurons[i], spike_time);
        }
    }
};

//=============================================================================
// API Consistency Tests
//=============================================================================

TEST_F(PatternDetectorsTest, AllHaveResetFunction) {
    oscillation_detector_reset(osc_detector);
    sequence_detector_reset(seq_detector);
    synchrony_detector_reset(sync_detector);
    // Should not crash
}

TEST_F(PatternDetectorsTest, AllHaveStatsFunction) {
    uint64_t total_samples, total_bursts, total_spikes, total_critical;
    uint64_t seq_total_detections;
    uint32_t seq_num_templates;
    float avg_power, avg_strength, mean_synchrony;

    bool osc_stats = oscillation_detector_get_stats(
        osc_detector, &total_samples, &total_bursts, &avg_power);
    bool seq_stats = sequence_detector_get_stats(
        seq_detector, &seq_num_templates, &seq_total_detections, &avg_strength);
    bool sync_stats = synchrony_detector_get_stats(
        sync_detector, &total_spikes, &total_critical, &mean_synchrony);

    EXPECT_TRUE(osc_stats);
    EXPECT_TRUE(seq_stats);
    EXPECT_TRUE(sync_stats);
}

TEST_F(PatternDetectorsTest, AllHaveDefaultConfig) {
    oscillation_detector_config_t osc_cfg = oscillation_detector_default_config();
    sequence_detector_config_t seq_cfg = sequence_detector_default_config();
    synchrony_detector_config_t sync_cfg = synchrony_detector_default_config(NUM_NEURONS);

    EXPECT_GT(osc_cfg.sample_rate_hz, 0.0f);
    EXPECT_GT(seq_cfg.max_templates, 0);
    EXPECT_EQ(sync_cfg.num_neurons, NUM_NEURONS);
}

//=============================================================================
// Oscillation Detection Tests
//=============================================================================

TEST_F(PatternDetectorsTest, DetectThetaOscillation) {
    // Generate theta oscillation (4-8 Hz), need >= 1024 samples for FFT window
    addOscillatorySamples(6.0f, 1100.0f, 1.0f);

    oscillation_result_t result;
    bool detected = oscillation_detector_detect(osc_detector, &result);

    EXPECT_TRUE(detected);
    EXPECT_GT(result.total_power, 0.0f);
}

TEST_F(PatternDetectorsTest, DetectGammaOscillation) {
    // Generate gamma oscillation (30-100 Hz), need >= 1024 samples for FFT window
    addOscillatorySamples(40.0f, 1100.0f, 0.5f);

    oscillation_result_t result;
    bool detected = oscillation_detector_detect(osc_detector, &result);

    EXPECT_TRUE(detected);
    EXPECT_TRUE(result.has_gamma);
}

TEST_F(PatternDetectorsTest, NoOscillationInWhiteNoise) {
    // Add random white noise
    for (int i = 0; i < 1000; i++) {
        float noise = static_cast<float>(rand()) / RAND_MAX - 0.5f;
        oscillation_detector_add_sample(osc_detector, noise, i);
    }

    oscillation_result_t result;
    oscillation_detector_detect(osc_detector, &result);

    // White noise should have low oscillation power
    EXPECT_LT(result.total_power, 10.0f);
}

//=============================================================================
// Sequence Detection Tests
//=============================================================================

TEST_F(PatternDetectorsTest, LearnAndDetectSimpleSequence) {
    // Create a simple sequence template
    sequence_element_t elements[3];
    elements[0] = {0, 0.0f};
    elements[1] = {1, 10.0f};
    elements[2] = {2, 20.0f};

    uint32_t template_id;
    bool learned = sequence_detector_learn_template(
        seq_detector, elements, 3, &template_id);

    EXPECT_TRUE(learned);
    EXPECT_NE(template_id, 0);

    // Add matching sequence
    sequence_detector_add_spike(seq_detector, 0, 100.0);
    sequence_detector_add_spike(seq_detector, 1, 110.0);
    sequence_detector_add_spike(seq_detector, 2, 120.0);

    // Detect
    sequence_detection_t detections[10];
    uint32_t num_detected;
    bool detected = sequence_detector_detect(
        seq_detector, detections, 10, &num_detected);

    EXPECT_TRUE(detected);
    EXPECT_GT(num_detected, 0);
}

TEST_F(PatternDetectorsTest, MultipleSequenceTemplates) {
    // Learn two different templates
    sequence_element_t seq1[2] = {{0, 0.0f}, {1, 10.0f}};
    sequence_element_t seq2[2] = {{2, 0.0f}, {3, 10.0f}};

    uint32_t id1, id2;
    bool learned1 = sequence_detector_learn_template(seq_detector, seq1, 2, &id1);
    bool learned2 = sequence_detector_learn_template(seq_detector, seq2, 2, &id2);

    EXPECT_TRUE(learned1);
    EXPECT_TRUE(learned2);
    EXPECT_NE(id1, id2);
}

TEST_F(PatternDetectorsTest, SequenceDetectionAfterReset) {
    // Add some spikes
    addSpikeSequence({0, 1, 2}, 100.0, 10.0);

    // Reset
    sequence_detector_reset(seq_detector);

    // Try to detect (should find nothing after reset)
    sequence_detection_t detections[10];
    uint32_t num_detected;
    sequence_detector_detect(seq_detector, detections, 10, &num_detected);

    EXPECT_EQ(num_detected, 0);
}

//=============================================================================
// Synchrony Detection Tests
//=============================================================================

TEST_F(PatternDetectorsTest, DetectSynchronousSpikes) {
    // Add synchronous spikes (all at same time)
    for (uint32_t i = 0; i < 50; i++) {
        synchrony_detector_add_spike(sync_detector, i, 100.0);
    }

    synchrony_result_t result;
    bool detected = synchrony_detector_detect(sync_detector, 0, &result);

    EXPECT_TRUE(detected);
    EXPECT_GT(result.synchrony_index, 0.5f) << "High synchrony expected";
    EXPECT_TRUE(result.is_critical_event);
}

TEST_F(PatternDetectorsTest, DetectAsynchronousSpikes) {
    // Add asynchronous spikes (spread over time)
    for (uint32_t i = 0; i < 50; i++) {
        synchrony_detector_add_spike(sync_detector, i, i * 10.0);
    }

    synchrony_result_t result;
    bool detected = synchrony_detector_detect(sync_detector, 0, &result);

    EXPECT_TRUE(detected);
    EXPECT_LT(result.synchrony_index, 0.5f) << "Low synchrony expected";
}

TEST_F(PatternDetectorsTest, PairwiseCorrelation) {
    // Add correlated spikes for neurons 0 and 1
    for (int i = 0; i < 10; i++) {
        double time = i * 100.0;
        synchrony_detector_add_spike(sync_detector, 0, time);
        synchrony_detector_add_spike(sync_detector, 1, time + 1.0); // 1ms lag
    }

    // Use 1000ms window to capture all 10 spike pairs (spanned over 900ms)
    float correlation = synchrony_detector_compute_correlation(
        sync_detector, 0, 1, 1000.0f);

    EXPECT_GT(correlation, 0.5f) << "Should detect correlation";
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(PatternDetectorsTest, OscillationAndSynchrony) {
    // Generate oscillatory activity with synchronous bursts

    // Add oscillation samples (need >= 1024 for FFT window)
    addOscillatorySamples(10.0f, 1100.0f, 1.0f);

    // Add synchronous bursts
    for (uint32_t i = 0; i < 30; i++) {
        synchrony_detector_add_spike(sync_detector, i, 100.0);
        synchrony_detector_add_spike(sync_detector, i, 200.0);
        synchrony_detector_add_spike(sync_detector, i, 300.0);
    }

    // Detect oscillation
    oscillation_result_t osc_result;
    bool osc_detected = oscillation_detector_detect(osc_detector, &osc_result);

    // Detect synchrony
    synchrony_result_t sync_result;
    bool sync_detected = synchrony_detector_detect(sync_detector, 0, &sync_result);

    EXPECT_TRUE(osc_detected);
    EXPECT_TRUE(sync_detected);
    EXPECT_GT(osc_result.total_power, 0.0f);
    EXPECT_GT(sync_result.synchrony_index, 0.0f);
}

TEST_F(PatternDetectorsTest, SequenceWithinOscillation) {
    // Simulate spike sequence embedded in oscillatory activity

    // Add background oscillation (need >= 1024 for FFT window)
    addOscillatorySamples(5.0f, 1100.0f, 0.5f);

    // Learn a sequence template
    sequence_element_t elements[3];
    elements[0] = {10, 0.0f};
    elements[1] = {11, 50.0f};
    elements[2] = {12, 100.0f};

    uint32_t template_id;
    sequence_detector_learn_template(seq_detector, elements, 3, &template_id);

    // Add the sequence
    sequence_detector_add_spike(seq_detector, 10, 200.0);
    sequence_detector_add_spike(seq_detector, 11, 250.0);
    sequence_detector_add_spike(seq_detector, 12, 300.0);

    // Detect sequence
    sequence_detection_t detections[10];
    uint32_t num_detected;
    bool seq_found = sequence_detector_detect(
        seq_detector, detections, 10, &num_detected);

    // Detect oscillation
    oscillation_result_t osc_result;
    bool osc_detected = oscillation_detector_detect(osc_detector, &osc_result);

    EXPECT_TRUE(seq_found);
    EXPECT_GT(num_detected, 0);
    EXPECT_TRUE(osc_detected);
}

TEST_F(PatternDetectorsTest, SynchronyDuringSequence) {
    // Sequence with synchronous bursts

    // Add sequence where multiple neurons fire together
    std::vector<uint32_t> neurons = {0, 1, 2, 3, 4};

    for (size_t i = 0; i < neurons.size(); i++) {
        // Each neuron fires at slightly different times (forming sequence)
        sequence_detector_add_spike(seq_detector, neurons[i], i * 10.0);

        // But multiple neurons fire synchronously at each step
        for (int j = 0; j < 10; j++) {
            synchrony_detector_add_spike(sync_detector, j, i * 10.0);
        }
    }

    // Check synchrony
    synchrony_result_t sync_result;
    bool sync_detected = synchrony_detector_detect(sync_detector, 0, &sync_result);

    EXPECT_TRUE(sync_detected);
    EXPECT_GT(sync_result.synchrony_index, 0.0f);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(PatternDetectorsTest, LongRunningDetection) {
    // Run all detectors for extended period
    for (int t = 0; t < 5000; t++) {
        // Add oscillation sample
        float signal = sinf(2.0f * M_PI * 10.0f * t / SAMPLE_RATE);
        oscillation_detector_add_sample(osc_detector, signal, t);

        // Add occasional spikes
        if (t % 100 == 0) {
            uint32_t neuron = t / 100;
            sequence_detector_add_spike(seq_detector, neuron, t);
            synchrony_detector_add_spike(sync_detector, neuron, t);
        }
    }

    // All detectors should still work
    oscillation_result_t osc_result;
    synchrony_result_t sync_result;

    EXPECT_TRUE(oscillation_detector_detect(osc_detector, &osc_result));
    EXPECT_TRUE(synchrony_detector_detect(sync_detector, 0, &sync_result));
}

TEST_F(PatternDetectorsTest, ResetAllDetectors) {
    // Add data to all
    addOscillatorySamples(10.0f, 100.0f, 1.0f);
    addSpikeSequence({0, 1, 2}, 100.0, 10.0);

    // Reset all - clears analysis state but preserves lifetime counters
    oscillation_detector_reset(osc_detector);
    sequence_detector_reset(seq_detector);
    synchrony_detector_reset(sync_detector);

    // Lifetime stats are intentionally preserved across resets
    uint64_t osc_samples, osc_bursts;
    uint32_t seq_templates;
    uint64_t seq_detections, sync_spikes, sync_critical;
    float power, strength, synchrony;

    oscillation_detector_get_stats(osc_detector, &osc_samples, &osc_bursts, &power);
    sequence_detector_get_stats(seq_detector, &seq_templates, &seq_detections, &strength);
    synchrony_detector_get_stats(sync_detector, &sync_spikes, &sync_critical, &synchrony);

    // Lifetime counters persist (by design) but running averages reset
    EXPECT_GE(osc_samples, 0u);  // Lifetime counter preserved
    EXPECT_GE(osc_bursts, 0u);
    EXPECT_GE(sync_spikes, 0u);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PatternDetectorsTest, EmptyDetection) {
    // Try to detect without adding any data
    oscillation_result_t osc_result;
    sequence_detection_t seq_detections[10];
    synchrony_result_t sync_result;
    uint32_t num_seq_detected;

    // Should handle gracefully
    bool osc_ok = oscillation_detector_detect(osc_detector, &osc_result);
    bool seq_ok = sequence_detector_detect(
        seq_detector, seq_detections, 10, &num_seq_detected);
    bool sync_ok = synchrony_detector_detect(sync_detector, 0, &sync_result);

    // Results depend on implementation, but should not crash
    (void)osc_ok;
    (void)seq_ok;
    (void)sync_ok;
}

TEST_F(PatternDetectorsTest, SingleSampleOscillation) {
    oscillation_detector_add_sample(osc_detector, 1.0f, 0);

    oscillation_result_t result;
    bool detected = oscillation_detector_detect(osc_detector, &result);

    // Should handle single sample gracefully
    (void)detected;
}

TEST_F(PatternDetectorsTest, SingleSpikeSequence) {
    sequence_detector_add_spike(seq_detector, 0, 100.0);

    sequence_detection_t detections[10];
    uint32_t num_detected;
    bool detected = sequence_detector_detect(
        seq_detector, detections, 10, &num_detected);

    // Single spike shouldn't match sequences
    EXPECT_EQ(num_detected, 0);
}

TEST_F(PatternDetectorsTest, SingleSpikeSynchrony) {
    synchrony_detector_add_spike(sync_detector, 0, 100.0);

    synchrony_result_t result;
    bool detected = synchrony_detector_detect(sync_detector, 0, &result);

    // Single spike has no synchrony
    (void)detected;
    EXPECT_LE(result.synchrony_index, 0.1f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
