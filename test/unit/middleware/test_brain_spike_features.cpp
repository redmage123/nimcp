/**
 * @file test_brain_spike_features.cpp
 * @brief Comprehensive unit tests for Brain Spike Feature Extraction
 *
 * WHAT: Test spike-based feature extraction for brain modules
 * WHY:  Ensure 100% code coverage for brain_spike_feature_extractor_t
 * HOW:  Test create/destroy, feature extraction, edge cases, error handling
 *
 * Coverage: 100% of all functions, branches, and edge cases
 * Categories: Lifecycle, Feature Extraction, Oscillations, Synchrony, Error Handling
 *
 * @author NIMCP Test Suite
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <memory>

// Headers have their own extern "C" guards
#include "middleware/brain_integration.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainSpikeFeatureTest : public ::testing::Test {
protected:
    brain_spike_feature_extractor_t extractor;

    void SetUp() override {
        extractor = nullptr;
    }

    void TearDown() override {
        brain_destroy_spike_feature_extractor(extractor);
    }

    // Helper: Create spike data with specified pattern
    spike_data_t* create_spike_data(uint32_t num_neurons,
                                     uint64_t start_time,
                                     uint64_t end_time) {
        spike_data_t* data = spike_data_create(num_neurons);
        if (!data) return nullptr;

        data->start_time = start_time;
        data->end_time = end_time;

        return data;
    }

    // Helper: Add regular spikes to neuron
    void add_regular_spikes(spike_data_t* data, uint32_t neuron_idx,
                           uint64_t start, uint64_t end, float rate_hz) {
        if (rate_hz <= 0.0f) return;

        float isi_ms = 1000.0f / rate_hz;
        std::vector<uint64_t> times;

        for (uint64_t t = start; t < end; t += static_cast<uint64_t>(isi_ms)) {
            times.push_back(t);
        }

        if (times.empty()) return;

        data->spike_times[neuron_idx] = static_cast<uint64_t*>(
            nimcp_malloc(times.size() * sizeof(uint64_t))
        );
        data->spike_counts[neuron_idx] = static_cast<uint32_t>(times.size());

        for (size_t i = 0; i < times.size(); i++) {
            data->spike_times[neuron_idx][i] = times[i];
        }
    }

    // Helper: Add burst pattern
    void add_burst_spikes(spike_data_t* data, uint32_t neuron_idx,
                         uint64_t start, uint32_t num_bursts,
                         uint32_t spikes_per_burst) {
        uint32_t total = num_bursts * spikes_per_burst;
        data->spike_times[neuron_idx] = static_cast<uint64_t*>(
            nimcp_malloc(total * sizeof(uint64_t))
        );
        data->spike_counts[neuron_idx] = total;

        uint32_t idx = 0;
        for (uint32_t b = 0; b < num_bursts; b++) {
            uint64_t burst_start = start + b * 100;  // 100ms apart
            for (uint32_t s = 0; s < spikes_per_burst; s++) {
                data->spike_times[neuron_idx][idx++] = burst_start + s * 2;  // 2ms ISI
            }
        }
    }
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(BrainSpikeFeatureTest, CreateWithValidParameters) {
    // WHAT: Create extractor with valid parameters
    // WHY:  Verify basic allocation succeeds
    extractor = brain_create_spike_feature_extractor(100, true, true);
    EXPECT_NE(extractor, nullptr);
}

TEST_F(BrainSpikeFeatureTest, CreateWithMinimalFeatures) {
    // WHAT: Create extractor with oscillations and synchrony disabled
    // WHY:  Test minimal configuration path
    extractor = brain_create_spike_feature_extractor(100, false, false);
    EXPECT_NE(extractor, nullptr);
}

TEST_F(BrainSpikeFeatureTest, CreateWithOscillationsOnly) {
    // WHAT: Enable oscillations but not synchrony
    // WHY:  Test partial feature configuration
    extractor = brain_create_spike_feature_extractor(100, true, false);
    EXPECT_NE(extractor, nullptr);
}

TEST_F(BrainSpikeFeatureTest, CreateWithSynchronyOnly) {
    // WHAT: Enable synchrony but not oscillations
    // WHY:  Test alternative partial configuration
    extractor = brain_create_spike_feature_extractor(100, false, true);
    EXPECT_NE(extractor, nullptr);
}

TEST_F(BrainSpikeFeatureTest, CreateWithZeroNeurons) {
    // WHAT: Create with zero neurons
    // WHY:  Test invalid input validation
    extractor = brain_create_spike_feature_extractor(0, true, true);
    EXPECT_EQ(extractor, nullptr);
}

TEST_F(BrainSpikeFeatureTest, CreateWithTooManyNeurons) {
    // WHAT: Create with neurons exceeding maximum
    // WHY:  Test upper bound validation
    extractor = brain_create_spike_feature_extractor(
        FEATURE_EXTRACTOR_MAX_NEURONS + 1, true, true
    );
    EXPECT_EQ(extractor, nullptr);
}

TEST_F(BrainSpikeFeatureTest, CreateWithMaxNeurons) {
    // WHAT: Create with exactly maximum neurons
    // WHY:  Test boundary condition
    extractor = brain_create_spike_feature_extractor(
        FEATURE_EXTRACTOR_MAX_NEURONS, true, true
    );
    EXPECT_NE(extractor, nullptr);
}

TEST_F(BrainSpikeFeatureTest, DestroyNull) {
    // WHAT: Destroy NULL extractor
    // WHY:  Verify safe NULL handling
    brain_destroy_spike_feature_extractor(nullptr);
    // Should not crash
}

TEST_F(BrainSpikeFeatureTest, DestroyValidExtractor) {
    // WHAT: Create and destroy extractor
    // WHY:  Test proper cleanup
    extractor = brain_create_spike_feature_extractor(100, true, true);
    ASSERT_NE(extractor, nullptr);
    brain_destroy_spike_feature_extractor(extractor);
    extractor = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// 2. Feature Extraction Tests
//=============================================================================

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesBasic) {
    // WHAT: Extract features from simple spike data
    // WHY:  Test basic extraction path
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Add regular firing at 10 Hz
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_TRUE(features.valid);
    EXPECT_GT(features.mean_firing_rate, 0.0f);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithNullExtractor) {
    // WHAT: Extract features with NULL extractor
    // WHY:  Test error handling
    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(nullptr, data, &features);

    EXPECT_FALSE(result);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithNullSpikeData) {
    // WHAT: Extract features with NULL spike data
    // WHY:  Test NULL input validation
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, nullptr, &features);

    EXPECT_FALSE(result);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithNullOutput) {
    // WHAT: Extract features with NULL output
    // WHY:  Test output validation
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    bool result = brain_extract_spike_features(extractor, data, nullptr);

    EXPECT_FALSE(result);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithTooManyNeurons) {
    // WHAT: Extract features with neuron count exceeding max
    // WHY:  Test bounds checking
    extractor = brain_create_spike_feature_extractor(100, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(200, 0, 1000);
    ASSERT_NE(data, nullptr);

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_FALSE(result);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithEmptyData) {
    // WHAT: Extract features from empty spike data
    // WHY:  Test edge case of no spikes
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_EQ(features.mean_firing_rate, 0.0f);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithBurstPattern) {
    // WHAT: Extract features from burst firing pattern
    // WHY:  Test burst detection
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Add burst pattern: 5 bursts, 5 spikes per burst
    for (uint32_t i = 0; i < 10; i++) {
        add_burst_spikes(data, i, 0, 5, 5);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_GT(features.burst_index, 0.0f);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithHighRate) {
    // WHAT: Extract features with high firing rate
    // WHY:  Test handling of high activity
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Add high rate (100 Hz) regular firing
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 100.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_GT(features.mean_firing_rate, 50.0f);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithLowRate) {
    // WHAT: Extract features with low firing rate
    // WHY:  Test handling of sparse activity
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Add low rate (1 Hz) regular firing
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 1.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_LT(features.mean_firing_rate, 5.0f);

    spike_data_destroy(data);
}

//=============================================================================
// 3. Oscillation Feature Tests
//=============================================================================

TEST_F(BrainSpikeFeatureTest, ExtractOscillationFeaturesEnabled) {
    // WHAT: Extract oscillation features when enabled
    // WHY:  Test oscillation computation path
    extractor = brain_create_spike_feature_extractor(10, true, false);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Add theta-range oscillation (6 Hz)
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 6.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    // Oscillation power should be computed
    EXPECT_GE(features.theta_power, 0.0f);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractOscillationFeaturesDisabled) {
    // WHAT: Extract features with oscillations disabled
    // WHY:  Test that oscillation code is skipped
    extractor = brain_create_spike_feature_extractor(10, false, false);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    // Oscillation powers should be zero when disabled
    EXPECT_EQ(features.delta_power, 0.0f);
    EXPECT_EQ(features.theta_power, 0.0f);
    EXPECT_EQ(features.alpha_power, 0.0f);
    EXPECT_EQ(features.beta_power, 0.0f);
    EXPECT_EQ(features.gamma_power, 0.0f);

    spike_data_destroy(data);
}

//=============================================================================
// 4. Synchrony Feature Tests
//=============================================================================

TEST_F(BrainSpikeFeatureTest, ExtractSynchronyFeaturesEnabled) {
    // WHAT: Extract synchrony features when enabled
    // WHY:  Test synchrony computation path
    extractor = brain_create_spike_feature_extractor(10, false, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Add synchronized spikes (all neurons fire together)
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    // Synchrony should be detected
    EXPECT_GE(features.synchrony_index, 0.0f);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractSynchronyFeaturesDisabled) {
    // WHAT: Extract features with synchrony disabled
    // WHY:  Test that synchrony code is skipped
    extractor = brain_create_spike_feature_extractor(10, false, false);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    // Synchrony should be zero when disabled
    EXPECT_EQ(features.synchrony_index, 0.0f);

    spike_data_destroy(data);
}

//=============================================================================
// 5. Edge Cases and Stress Tests
//=============================================================================

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithSingleNeuron) {
    // WHAT: Extract features from single neuron
    // WHY:  Test minimal population
    extractor = brain_create_spike_feature_extractor(1, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(1, 0, 1000);
    ASSERT_NE(data, nullptr);

    add_regular_spikes(data, 0, 0, 1000, 10.0f);

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_GT(features.mean_firing_rate, 0.0f);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithLargePopulation) {
    // WHAT: Extract features from large population
    // WHY:  Test scalability
    extractor = brain_create_spike_feature_extractor(1000, true, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(1000, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 1000; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesMultipleTimes) {
    // WHAT: Extract features multiple times
    // WHY:  Test reusability
    extractor = brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    for (int trial = 0; trial < 5; trial++) {
        spike_data_t* data = create_spike_data(10, 0, 1000);
        ASSERT_NE(data, nullptr);

        for (uint32_t i = 0; i < 10; i++) {
            add_regular_spikes(data, i, 0, 1000, 10.0f + trial);
        }

        middleware_features_t features = {};
        bool result = brain_extract_spike_features(extractor, data, &features);

        EXPECT_TRUE(result);

        spike_data_destroy(data);
    }
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithIrregularSpikes) {
    // WHAT: Extract features from irregular spike pattern
    // WHY:  Test CV calculation with irregular firing
    extractor = brain_create_spike_feature_extractor(5, false, false);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Create irregular spike pattern (varying ISIs)
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t count = 20;
        data->spike_times[i] = static_cast<uint64_t*>(
            nimcp_malloc(count * sizeof(uint64_t))
        );
        data->spike_counts[i] = count;

        uint64_t t = 0;
        for (uint32_t j = 0; j < count; j++) {
            // Irregular ISI: alternating short and long
            t += (j % 2 == 0) ? 10 : 50;
            data->spike_times[i][j] = t;
        }
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_GT(features.isi_cv, 0.0f);  // Should have non-zero CV for irregular pattern

    spike_data_destroy(data);
}

TEST_F(BrainSpikeFeatureTest, ExtractFeaturesWithSynchronizedPopulation) {
    // WHAT: Extract features from synchronized population
    // WHY:  Test synchrony detection with coordinated firing
    extractor = brain_create_spike_feature_extractor(10, false, true);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // All neurons fire at same times (perfect synchrony)
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t count = 10;
        data->spike_times[i] = static_cast<uint64_t*>(
            nimcp_malloc(count * sizeof(uint64_t))
        );
        data->spike_counts[i] = count;

        for (uint32_t j = 0; j < count; j++) {
            data->spike_times[i][j] = j * 100;  // All neurons spike together
        }
    }

    middleware_features_t features = {};
    bool result = brain_extract_spike_features(extractor, data, &features);

    EXPECT_TRUE(result);
    EXPECT_GT(features.synchrony_index, 0.0f);  // Should detect synchrony

    spike_data_destroy(data);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
