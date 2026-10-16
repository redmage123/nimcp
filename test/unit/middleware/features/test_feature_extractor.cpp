/**
 * @file test_feature_extractor.cpp
 * @brief Comprehensive unit tests for Feature Extractor module
 *
 * Coverage: 100% of all functions and edge cases
 * Categories: Create/Destroy, Rate Features, Temporal Features, Population Features,
 *             Oscillation Analysis, Entropy, Integration, Thread Safety, Performance
 *
 * @author NIMCP Test Suite
 * @date 2025-01-19
 */

#include <gtest/gtest.h>
#include <pthread.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/features/nimcp_feature_extractor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FeatureExtractorTest : public ::testing::Test {
protected:
    feature_extractor_t extractor;
    feature_extractor_config_t config;

    void SetUp() override {
        config = feature_extractor_default_config();
        extractor = feature_extractor_create(&config);
        ASSERT_NE(extractor, nullptr);
    }

    void TearDown() override {
        feature_extractor_destroy(extractor);
    }

    // Helper: Create spike data with specified patterns
    spike_data_t* create_spike_data(uint32_t num_neurons, uint64_t start_time, uint64_t end_time) {
        spike_data_t* data = spike_data_create(num_neurons);
        if (!data) return nullptr;

        data->start_time = start_time;
        data->end_time = end_time;

        return data;
    }

    // Helper: Add regular spikes to a neuron
    void add_regular_spikes(spike_data_t* data, uint32_t neuron_idx,
                           uint64_t start, uint64_t end, float rate_hz) {
        if (rate_hz <= 0.0f) return;

        float isi_ms = 1000.0f / rate_hz;
        uint32_t count = 0;

        for (uint64_t t = start; t < end; t += (uint64_t)isi_ms) {
            count++;
        }

        data->spike_times[neuron_idx] = (uint64_t*)nimcp_malloc(count * sizeof(uint64_t));
        data->spike_counts[neuron_idx] = count;

        uint32_t idx = 0;
        for (uint64_t t = start; t < end; t += (uint64_t)isi_ms) {
            data->spike_times[neuron_idx][idx++] = t;
        }
    }

    // Helper: Add burst pattern spikes
    void add_burst_spikes(spike_data_t* data, uint32_t neuron_idx,
                         uint64_t start, uint32_t num_bursts, uint32_t spikes_per_burst) {
        uint32_t total = num_bursts * spikes_per_burst;
        data->spike_times[neuron_idx] = (uint64_t*)nimcp_malloc(total * sizeof(uint64_t));
        data->spike_counts[neuron_idx] = total;

        uint32_t idx = 0;
        for (uint32_t b = 0; b < num_bursts; b++) {
            uint64_t burst_start = start + b * 100;  // Bursts 100ms apart
            for (uint32_t s = 0; s < spikes_per_burst; s++) {
                data->spike_times[neuron_idx][idx++] = burst_start + s * 2;  // 2ms ISI in burst
            }
        }
    }

    // Helper: Add random Poisson spikes
    void add_poisson_spikes(spike_data_t* data, uint32_t neuron_idx,
                           uint64_t start, uint64_t end, float rate_hz) {
        std::vector<uint64_t> times;
        float lambda = rate_hz / 1000.0f;  // Rate per ms

        for (uint64_t t = start; t < end; t++) {
            float r = (float)rand() / RAND_MAX;
            if (r < lambda) {
                times.push_back(t);
            }
        }

        data->spike_counts[neuron_idx] = times.size();
        if (times.size() > 0) {
            data->spike_times[neuron_idx] = (uint64_t*)nimcp_malloc(times.size() * sizeof(uint64_t));
            memcpy(data->spike_times[neuron_idx], times.data(), times.size() * sizeof(uint64_t));
        }
    }
};

//=============================================================================
// 1. Create/Destroy Tests
//=============================================================================

TEST_F(FeatureExtractorTest, CreateWithDefaultConfig) {
    EXPECT_NE(extractor, nullptr);
}

TEST_F(FeatureExtractorTest, CreateWithCustomConfig) {
    feature_extractor_config_t custom = {
        .window_ms = 200.0f,
        .synchrony_window_ms = 10.0f,
        .burst_isi_threshold_ms = 5.0f,
        .min_burst_spikes = 5,
        .entropy_bins = 30,
        .compute_oscillations = false,
        .compute_entropy = false,
        .compute_synchrony = false
    };

    feature_extractor_t custom_ext = feature_extractor_create(&custom);
    EXPECT_NE(custom_ext, nullptr);
    feature_extractor_destroy(custom_ext);
}

TEST_F(FeatureExtractorTest, CreateWithNullConfig) {
    feature_extractor_t ext = feature_extractor_create(nullptr);
    EXPECT_NE(ext, nullptr);  // Should use defaults
    feature_extractor_destroy(ext);
}

TEST_F(FeatureExtractorTest, DestroyNull) {
    feature_extractor_destroy(nullptr);  // Should not crash
    SUCCEED();
}

TEST_F(FeatureExtractorTest, MultipleCreateDestroy) {
    for (int i = 0; i < 10; i++) {
        feature_extractor_t ext = feature_extractor_create(&config);
        ASSERT_NE(ext, nullptr);
        feature_extractor_destroy(ext);
    }
}

TEST_F(FeatureExtractorTest, DefaultConfigValues) {
    feature_extractor_config_t defaults = feature_extractor_default_config();

    EXPECT_FLOAT_EQ(defaults.window_ms, 100.0f);
    EXPECT_FLOAT_EQ(defaults.synchrony_window_ms, 5.0f);
    EXPECT_FLOAT_EQ(defaults.burst_isi_threshold_ms, 10.0f);
    EXPECT_EQ(defaults.min_burst_spikes, 3);
    EXPECT_EQ(defaults.entropy_bins, 20);
    EXPECT_TRUE(defaults.compute_oscillations);
    EXPECT_TRUE(defaults.compute_entropy);
    EXPECT_TRUE(defaults.compute_synchrony);
}

//=============================================================================
// 2. Mean Firing Rate Tests
//=============================================================================

TEST_F(FeatureExtractorTest, MeanFiringRateZeroSpikes) {
    spike_data_t* data = create_spike_data(5, 0, 100);
    ASSERT_NE(data, nullptr);

    // No spikes for any neuron
    for (uint32_t i = 0; i < 5; i++) {
        data->spike_counts[i] = 0;
        data->spike_times[i] = nullptr;
    }

    float rate;
    bool success = feature_extractor_compute_mean_firing_rate(extractor, data, &rate);

    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(rate, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, MeanFiringRateRegularFiring) {
    spike_data_t* data = create_spike_data(3, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Each neuron fires at 10 Hz (regular)
    for (uint32_t i = 0; i < 3; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    float rate;
    bool success = feature_extractor_compute_mean_firing_rate(extractor, data, &rate);

    EXPECT_TRUE(success);
    EXPECT_NEAR(rate, 10.0f, 1.0f);  // Should be ~10 Hz

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, MeanFiringRateBurstFiring) {
    spike_data_t* data = create_spike_data(2, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Burst firing pattern
    add_burst_spikes(data, 0, 0, 5, 10);  // 5 bursts, 10 spikes each
    add_burst_spikes(data, 1, 50, 5, 10);

    float rate;
    bool success = feature_extractor_compute_mean_firing_rate(extractor, data, &rate);

    EXPECT_TRUE(success);
    EXPECT_GT(rate, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, MeanFiringRateMixedPopulation) {
    spike_data_t* data = create_spike_data(4, 0, 1000);
    ASSERT_NE(data, nullptr);

    add_regular_spikes(data, 0, 0, 1000, 5.0f);   // 5 Hz
    add_regular_spikes(data, 1, 0, 1000, 10.0f);  // 10 Hz
    add_regular_spikes(data, 2, 0, 1000, 20.0f);  // 20 Hz
    add_regular_spikes(data, 3, 0, 1000, 30.0f);  // 30 Hz

    float rate;
    bool success = feature_extractor_compute_mean_firing_rate(extractor, data, &rate);

    EXPECT_TRUE(success);
    EXPECT_NEAR(rate, 16.25f, 2.0f);  // Average of 5, 10, 20, 30

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, MeanFiringRateNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    float rate;

    EXPECT_FALSE(feature_extractor_compute_mean_firing_rate(nullptr, data, &rate));
    EXPECT_FALSE(feature_extractor_compute_mean_firing_rate(extractor, nullptr, &rate));
    EXPECT_FALSE(feature_extractor_compute_mean_firing_rate(extractor, data, nullptr));

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, MeanFiringRateEdgeCases) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    ASSERT_NE(data, nullptr);

    // Single spike
    data->spike_counts[0] = 1;
    data->spike_times[0] = (uint64_t*)nimcp_malloc(sizeof(uint64_t));
    data->spike_times[0][0] = 50;

    float rate;
    bool success = feature_extractor_compute_mean_firing_rate(extractor, data, &rate);

    EXPECT_TRUE(success);
    EXPECT_GT(rate, 0.0f);

    spike_data_destroy(data);
}

//=============================================================================
// 3. CV Computation Tests
//=============================================================================

TEST_F(FeatureExtractorTest, CVPerfectlyRegular) {
    spike_data_t* data = create_spike_data(3, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Perfectly regular firing
    for (uint32_t i = 0; i < 3; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    float cv;
    bool success = feature_extractor_compute_population_cv(extractor, data, &cv);

    EXPECT_TRUE(success);
    EXPECT_NEAR(cv, 0.0f, 0.1f);  // CV should be near 0 for regular firing

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, CVPoissonLike) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    srand(42);  // Fixed seed for reproducibility
    for (uint32_t i = 0; i < 5; i++) {
        add_poisson_spikes(data, i, 0, 1000, 20.0f);
    }

    float cv;
    bool success = feature_extractor_compute_population_cv(extractor, data, &cv);

    EXPECT_TRUE(success);
    EXPECT_GT(cv, 0.5f);  // Poisson process has CV ~ 1.0
    EXPECT_LT(cv, 2.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, CVBursty) {
    spike_data_t* data = create_spike_data(3, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 3; i++) {
        add_burst_spikes(data, i, 0, 5, 10);
    }

    float cv;
    bool success = feature_extractor_compute_population_cv(extractor, data, &cv);

    EXPECT_TRUE(success);
    EXPECT_GT(cv, 1.0f);  // Bursty firing has CV > 1

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, CVInsufficientSpikes) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    ASSERT_NE(data, nullptr);

    // Only one spike - can't compute ISI
    data->spike_counts[0] = 1;
    data->spike_times[0] = (uint64_t*)nimcp_malloc(sizeof(uint64_t));
    data->spike_times[0][0] = 50;

    float cv;
    bool success = feature_extractor_compute_population_cv(extractor, data, &cv);

    EXPECT_FALSE(success);  // Need at least 2 spikes for ISI

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, CVNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    float cv;

    EXPECT_FALSE(feature_extractor_compute_population_cv(nullptr, data, &cv));
    EXPECT_FALSE(feature_extractor_compute_population_cv(extractor, nullptr, &cv));
    EXPECT_FALSE(feature_extractor_compute_population_cv(extractor, data, nullptr));

    spike_data_destroy(data);
}

//=============================================================================
// 4. Fano Factor Tests
//=============================================================================

TEST_F(FeatureExtractorTest, FanoFactorRegular) {
    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // All neurons fire at same regular rate
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    float fano;
    bool success = feature_extractor_compute_fano_factor(extractor, data, &fano);

    EXPECT_TRUE(success);
    EXPECT_LT(fano, 0.5f);  // Sub-Poisson (very regular)

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, FanoFactorPoisson) {
    spike_data_t* data = create_spike_data(20, 0, 1000);
    ASSERT_NE(data, nullptr);

    srand(123);
    for (uint32_t i = 0; i < 20; i++) {
        add_poisson_spikes(data, i, 0, 1000, 15.0f);
    }

    float fano;
    bool success = feature_extractor_compute_fano_factor(extractor, data, &fano);

    EXPECT_TRUE(success);
    EXPECT_GT(fano, 0.5f);  // Poisson: Fano ~ 1.0
    EXPECT_LT(fano, 2.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, FanoFactorHighVariability) {
    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Mixed rates - high variance
    for (uint32_t i = 0; i < 10; i++) {
        float rate = 5.0f + i * 10.0f;  // 5, 15, 25, ..., 95 Hz
        add_regular_spikes(data, i, 0, 1000, rate);
    }

    float fano;
    bool success = feature_extractor_compute_fano_factor(extractor, data, &fano);

    EXPECT_TRUE(success);
    EXPECT_GT(fano, 1.0f);  // High variance

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, FanoFactorZeroSpikes) {
    spike_data_t* data = create_spike_data(5, 0, 100);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        data->spike_counts[i] = 0;
        data->spike_times[i] = nullptr;
    }

    float fano;
    bool success = feature_extractor_compute_fano_factor(extractor, data, &fano);

    EXPECT_FALSE(success);  // Can't compute with no spikes

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, FanoFactorNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    float fano;

    EXPECT_FALSE(feature_extractor_compute_fano_factor(nullptr, data, &fano));
    EXPECT_FALSE(feature_extractor_compute_fano_factor(extractor, nullptr, &fano));
    EXPECT_FALSE(feature_extractor_compute_fano_factor(extractor, data, nullptr));

    spike_data_destroy(data);
}

//=============================================================================
// 5. Burst Index Tests
//=============================================================================

TEST_F(FeatureExtractorTest, BurstIndexNoBursts) {
    spike_data_t* data = create_spike_data(3, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Regular tonic firing - no bursts
    for (uint32_t i = 0; i < 3; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    float burst_index;
    bool success = feature_extractor_compute_burst_index(extractor, data, &burst_index);

    EXPECT_TRUE(success);
    EXPECT_NEAR(burst_index, 0.0f, 0.1f);  // No bursts

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, BurstIndexAllBursts) {
    spike_data_t* data = create_spike_data(3, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Pure burst firing
    for (uint32_t i = 0; i < 3; i++) {
        add_burst_spikes(data, i, 0, 10, 5);
    }

    float burst_index;
    bool success = feature_extractor_compute_burst_index(extractor, data, &burst_index);

    EXPECT_TRUE(success);
    EXPECT_GT(burst_index, 0.8f);  // Most/all spikes in bursts

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, BurstIndexMixed) {
    spike_data_t* data = create_spike_data(2, 0, 1000);
    ASSERT_NE(data, nullptr);

    add_regular_spikes(data, 0, 0, 1000, 10.0f);  // Tonic
    add_burst_spikes(data, 1, 0, 5, 10);          // Bursts

    float burst_index;
    bool success = feature_extractor_compute_burst_index(extractor, data, &burst_index);

    EXPECT_TRUE(success);
    EXPECT_GT(burst_index, 0.0f);
    EXPECT_LT(burst_index, 1.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, BurstIndexInsufficientSpikes) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    ASSERT_NE(data, nullptr);

    // Too few spikes to form burst
    data->spike_counts[0] = 2;
    data->spike_times[0] = (uint64_t*)nimcp_malloc(2 * sizeof(uint64_t));
    data->spike_times[0][0] = 10;
    data->spike_times[0][1] = 12;

    float burst_index;
    bool success = feature_extractor_compute_burst_index(extractor, data, &burst_index);

    // May succeed with index of 0
    if (success) {
        EXPECT_FLOAT_EQ(burst_index, 0.0f);
    }

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, BurstIndexNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    float burst_index;

    EXPECT_FALSE(feature_extractor_compute_burst_index(nullptr, data, &burst_index));
    EXPECT_FALSE(feature_extractor_compute_burst_index(extractor, nullptr, &burst_index));
    EXPECT_FALSE(feature_extractor_compute_burst_index(extractor, data, nullptr));

    spike_data_destroy(data);
}

//=============================================================================
// 6. Synchrony Tests
//=============================================================================

TEST_F(FeatureExtractorTest, SynchronyPerfectSync) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    // All neurons fire at exact same times
    for (uint32_t i = 0; i < 5; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    float synchrony;
    bool success = feature_extractor_compute_synchrony_index(extractor, data, &synchrony);

    EXPECT_TRUE(success);
    EXPECT_GT(synchrony, 0.8f);  // High synchrony

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, SynchronyNoSync) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Each neuron at different offset
    for (uint32_t i = 0; i < 5; i++) {
        add_regular_spikes(data, i, i * 20, 1000, 10.0f);
    }

    float synchrony;
    bool success = feature_extractor_compute_synchrony_index(extractor, data, &synchrony);

    EXPECT_TRUE(success);
    EXPECT_LT(synchrony, 0.5f);  // Low synchrony

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, SynchronyPartial) {
    spike_data_t* data = create_spike_data(4, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Two pairs of synchronized neurons
    add_regular_spikes(data, 0, 0, 1000, 10.0f);
    add_regular_spikes(data, 1, 0, 1000, 10.0f);  // Sync with 0
    add_regular_spikes(data, 2, 50, 1000, 10.0f);
    add_regular_spikes(data, 3, 50, 1000, 10.0f);  // Sync with 2

    float synchrony;
    bool success = feature_extractor_compute_synchrony_index(extractor, data, &synchrony);

    EXPECT_TRUE(success);
    EXPECT_GT(synchrony, 0.3f);
    EXPECT_LT(synchrony, 0.9f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, SynchronyNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    float synchrony;

    EXPECT_FALSE(feature_extractor_compute_synchrony_index(nullptr, data, &synchrony));
    EXPECT_FALSE(feature_extractor_compute_synchrony_index(extractor, nullptr, &synchrony));
    EXPECT_FALSE(feature_extractor_compute_synchrony_index(extractor, data, nullptr));

    spike_data_destroy(data);
}

//=============================================================================
// 7. Oscillation Power Tests
//=============================================================================

TEST_F(FeatureExtractorTest, OscillationPowerDeltaBand) {
    spike_data_t* data = create_spike_data(10, 0, 2000);
    ASSERT_NE(data, nullptr);

    // Slow oscillation pattern (delta range: 0.5-4 Hz)
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 2000, 2.0f);  // 2 Hz
    }

    float delta, theta, alpha, beta, gamma;
    bool success = feature_extractor_compute_oscillation_power(
        extractor, data, &delta, &theta, &alpha, &beta, &gamma
    );

    EXPECT_TRUE(success);
    EXPECT_GT(delta, 0.0f);
    // Delta should be strongest
    EXPECT_GT(delta, theta);
    EXPECT_GT(delta, alpha);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, OscillationPowerThetaBand) {
    spike_data_t* data = create_spike_data(10, 0, 2000);
    ASSERT_NE(data, nullptr);

    // Theta oscillation (4-8 Hz)
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 2000, 6.0f);  // 6 Hz
    }

    float delta, theta, alpha, beta, gamma;
    bool success = feature_extractor_compute_oscillation_power(
        extractor, data, &delta, &theta, &alpha, &beta, &gamma
    );

    EXPECT_TRUE(success);
    EXPECT_GT(theta, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, OscillationPowerAlphaBand) {
    spike_data_t* data = create_spike_data(10, 0, 2000);
    ASSERT_NE(data, nullptr);

    // Alpha oscillation (8-13 Hz)
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 2000, 10.0f);  // 10 Hz
    }

    float delta, theta, alpha, beta, gamma;
    bool success = feature_extractor_compute_oscillation_power(
        extractor, data, &delta, &theta, &alpha, &beta, &gamma
    );

    EXPECT_TRUE(success);
    EXPECT_GT(alpha, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, OscillationPowerBetaBand) {
    spike_data_t* data = create_spike_data(10, 0, 2000);
    ASSERT_NE(data, nullptr);

    // Beta oscillation (13-30 Hz)
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 2000, 20.0f);  // 20 Hz
    }

    float delta, theta, alpha, beta, gamma;
    bool success = feature_extractor_compute_oscillation_power(
        extractor, data, &delta, &theta, &alpha, &beta, &gamma
    );

    EXPECT_TRUE(success);
    EXPECT_GT(beta, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, OscillationPowerGammaBand) {
    spike_data_t* data = create_spike_data(10, 0, 2000);
    ASSERT_NE(data, nullptr);

    // Gamma oscillation (30-100 Hz)
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 2000, 40.0f);  // 40 Hz
    }

    float delta, theta, alpha, beta, gamma;
    bool success = feature_extractor_compute_oscillation_power(
        extractor, data, &delta, &theta, &alpha, &beta, &gamma
    );

    EXPECT_TRUE(success);
    EXPECT_GT(gamma, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, OscillationPowerMultipleBands) {
    spike_data_t* data = create_spike_data(20, 0, 2000);
    ASSERT_NE(data, nullptr);

    // Mix of different oscillation frequencies
    for (uint32_t i = 0; i < 5; i++) {
        add_regular_spikes(data, i, 0, 2000, 2.0f);   // Delta
    }
    for (uint32_t i = 5; i < 10; i++) {
        add_regular_spikes(data, i, 0, 2000, 6.0f);   // Theta
    }
    for (uint32_t i = 10; i < 15; i++) {
        add_regular_spikes(data, i, 0, 2000, 10.0f);  // Alpha
    }
    for (uint32_t i = 15; i < 20; i++) {
        add_regular_spikes(data, i, 0, 2000, 40.0f);  // Gamma
    }

    float delta, theta, alpha, beta, gamma;
    bool success = feature_extractor_compute_oscillation_power(
        extractor, data, &delta, &theta, &alpha, &beta, &gamma
    );

    EXPECT_TRUE(success);
    EXPECT_GT(delta, 0.0f);
    EXPECT_GT(theta, 0.0f);
    EXPECT_GT(alpha, 0.0f);
    EXPECT_GT(gamma, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, OscillationPowerNoActivity) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        data->spike_counts[i] = 0;
        data->spike_times[i] = nullptr;
    }

    float delta, theta, alpha, beta, gamma;
    bool success = feature_extractor_compute_oscillation_power(
        extractor, data, &delta, &theta, &alpha, &beta, &gamma
    );

    EXPECT_FALSE(success);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, OscillationPowerNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    float delta, theta, alpha, beta, gamma;

    EXPECT_FALSE(feature_extractor_compute_oscillation_power(nullptr, data, &delta, &theta, &alpha, &beta, &gamma));
    EXPECT_FALSE(feature_extractor_compute_oscillation_power(extractor, nullptr, &delta, &theta, &alpha, &beta, &gamma));
    EXPECT_FALSE(feature_extractor_compute_oscillation_power(extractor, data, nullptr, &theta, &alpha, &beta, &gamma));
    EXPECT_FALSE(feature_extractor_compute_oscillation_power(extractor, data, &delta, nullptr, &alpha, &beta, &gamma));
    EXPECT_FALSE(feature_extractor_compute_oscillation_power(extractor, data, &delta, &theta, nullptr, &beta, &gamma));
    EXPECT_FALSE(feature_extractor_compute_oscillation_power(extractor, data, &delta, &theta, &alpha, nullptr, &gamma));
    EXPECT_FALSE(feature_extractor_compute_oscillation_power(extractor, data, &delta, &theta, &alpha, &beta, nullptr));

    spike_data_destroy(data);
}

//=============================================================================
// 8. Entropy Tests
//=============================================================================

TEST_F(FeatureExtractorTest, EntropyUniformDistribution) {
    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Uniform spike counts
    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    float entropy;
    bool success = feature_extractor_compute_spike_entropy(extractor, data, &entropy);

    EXPECT_TRUE(success);
    EXPECT_GT(entropy, 0.0f);  // Should have positive entropy

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, EntropyNonUniform) {
    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Non-uniform distribution
    for (uint32_t i = 0; i < 10; i++) {
        float rate = 5.0f + i * 5.0f;  // 5, 10, 15, ..., 50 Hz
        add_regular_spikes(data, i, 0, 1000, rate);
    }

    float entropy;
    bool success = feature_extractor_compute_spike_entropy(extractor, data, &entropy);

    EXPECT_TRUE(success);
    EXPECT_GT(entropy, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, EntropyZero) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Only one neuron fires
    add_regular_spikes(data, 0, 0, 1000, 10.0f);
    for (uint32_t i = 1; i < 5; i++) {
        data->spike_counts[i] = 0;
        data->spike_times[i] = nullptr;
    }

    float entropy;
    bool success = feature_extractor_compute_spike_entropy(extractor, data, &entropy);

    EXPECT_TRUE(success);
    EXPECT_NEAR(entropy, 0.0f, 0.1f);  // Low entropy

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, EntropyMaximum) {
    spike_data_t* data = create_spike_data(20, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Perfectly uniform - maximum entropy
    for (uint32_t i = 0; i < 20; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    float entropy;
    bool success = feature_extractor_compute_spike_entropy(extractor, data, &entropy);

    EXPECT_TRUE(success);
    EXPECT_GT(entropy, 3.0f);  // log2(20) ≈ 4.32, should be close

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, EntropyNoSpikes) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        data->spike_counts[i] = 0;
        data->spike_times[i] = nullptr;
    }

    float entropy;
    bool success = feature_extractor_compute_spike_entropy(extractor, data, &entropy);

    EXPECT_FALSE(success);  // Can't compute entropy with no spikes

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, EntropyNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    float entropy;

    EXPECT_FALSE(feature_extractor_compute_spike_entropy(nullptr, data, &entropy));
    EXPECT_FALSE(feature_extractor_compute_spike_entropy(extractor, nullptr, &entropy));
    EXPECT_FALSE(feature_extractor_compute_spike_entropy(extractor, data, nullptr));

    spike_data_destroy(data);
}

//=============================================================================
// 9. Integration Tests
//=============================================================================

TEST_F(FeatureExtractorTest, UpdateExtractAllFeatures) {
    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    // Create diverse population
    for (uint32_t i = 0; i < 5; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f + i * 5.0f);
    }
    for (uint32_t i = 5; i < 10; i++) {
        add_burst_spikes(data, i, 0, 5, 8);
    }

    middleware_features_t features;
    bool success = feature_extractor_update(extractor, data, &features);

    EXPECT_TRUE(success);
    EXPECT_TRUE(features.valid);
    EXPECT_GT(features.timestamp, 0);

    // Verify all features are computed
    EXPECT_GT(features.mean_firing_rate, 0.0f);
    EXPECT_GE(features.population_rate_std, 0.0f);
    EXPECT_GT(features.mean_isi, 0.0f);
    EXPECT_GE(features.isi_cv, 0.0f);
    EXPECT_GE(features.synchrony_index, 0.0f);
    EXPECT_GE(features.burst_index, 0.0f);
    EXPECT_GE(features.fano_factor, 0.0f);

    // Oscillation powers
    EXPECT_GE(features.delta_power, 0.0f);
    EXPECT_GE(features.theta_power, 0.0f);
    EXPECT_GE(features.alpha_power, 0.0f);
    EXPECT_GE(features.beta_power, 0.0f);
    EXPECT_GE(features.gamma_power, 0.0f);

    EXPECT_GE(features.spike_entropy, 0.0f);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, UpdateEmptyData) {
    spike_data_t* data = create_spike_data(5, 0, 100);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        data->spike_counts[i] = 0;
        data->spike_times[i] = nullptr;
    }

    middleware_features_t features;
    bool success = feature_extractor_update(extractor, data, &features);

    // Empty data is valid input - just produces default/zero feature values
    EXPECT_TRUE(success);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, UpdateNullPointers) {
    spike_data_t* data = create_spike_data(1, 0, 100);
    middleware_features_t features;

    EXPECT_FALSE(feature_extractor_update(nullptr, data, &features));
    EXPECT_FALSE(feature_extractor_update(extractor, nullptr, &features));
    EXPECT_FALSE(feature_extractor_update(extractor, data, nullptr));

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, UpdateFeatureValidity) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    middleware_features_t features;
    features.valid = false;

    bool success = feature_extractor_update(extractor, data, &features);

    EXPECT_TRUE(success);
    EXPECT_TRUE(features.valid);  // Should be marked valid

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, UpdateTimestamp) {
    spike_data_t* data = create_spike_data(3, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 3; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    middleware_features_t features;
    bool success = feature_extractor_update(extractor, data, &features);

    EXPECT_TRUE(success);
    EXPECT_EQ(features.timestamp, data->end_time);

    spike_data_destroy(data);
}

//=============================================================================
// 10. Utility Function Tests
//=============================================================================

TEST_F(FeatureExtractorTest, FeaturesCreateDestroy) {
    middleware_features_t* features = middleware_features_create();

    EXPECT_NE(features, nullptr);
    EXPECT_FALSE(features->valid);

    middleware_features_destroy(features);
}

TEST_F(FeatureExtractorTest, FeaturesDestroyNull) {
    middleware_features_destroy(nullptr);  // Should not crash
    SUCCEED();
}

TEST_F(FeatureExtractorTest, FeaturesReset) {
    middleware_features_t features;
    features.valid = true;
    features.mean_firing_rate = 50.0f;

    middleware_features_reset(&features);

    EXPECT_FALSE(features.valid);
}

TEST_F(FeatureExtractorTest, FeaturesResetNull) {
    middleware_features_reset(nullptr);  // Should not crash
    SUCCEED();
}

TEST_F(FeatureExtractorTest, SpikeDataCreate) {
    spike_data_t* data = spike_data_create(10);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->num_neurons, 10);
    EXPECT_NE(data->spike_times, nullptr);
    EXPECT_NE(data->spike_counts, nullptr);

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, SpikeDataCreateZero) {
    spike_data_t* data = spike_data_create(0);
    EXPECT_EQ(data, nullptr);
}

TEST_F(FeatureExtractorTest, SpikeDataDestroyNull) {
    spike_data_destroy(nullptr);  // Should not crash
    SUCCEED();
}

TEST_F(FeatureExtractorTest, SpikeDataDestroyWithSpikes) {
    spike_data_t* data = create_spike_data(5, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f);
    }

    spike_data_destroy(data);  // Should free all spike arrays
    SUCCEED();
}

//=============================================================================
// 11. Thread Safety Tests
//=============================================================================

struct FeatureThreadData {
    feature_extractor_t extractor;
    spike_data_t* data;
    int num_iterations;
    bool success;
};

void* concurrent_feature_extraction_thread(void* arg) {
    FeatureThreadData* thread_data = static_cast<FeatureThreadData*>(arg);
    thread_data->success = true;

    for (int i = 0; i < thread_data->num_iterations; i++) {
        middleware_features_t features;
        bool ok = feature_extractor_update(
            thread_data->extractor,
            thread_data->data,
            &features
        );

        if (!ok) {
            thread_data->success = false;
            break;
        }
    }

    return nullptr;
}

TEST_F(FeatureExtractorTest, ThreadSafetyConcurrentExtraction) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;

    spike_data_t* data = create_spike_data(10, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 10; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f + i * 2.0f);
    }

    pthread_t threads[NUM_THREADS];
    FeatureThreadData thread_data[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].extractor = extractor;
        thread_data[i].data = data;
        thread_data[i].num_iterations = ITERATIONS;
        thread_data[i].success = false;

        pthread_create(&threads[i], nullptr, concurrent_feature_extraction_thread, &thread_data[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
        EXPECT_TRUE(thread_data[i].success);
    }

    spike_data_destroy(data);
}

void* concurrent_rate_computation_thread(void* arg) {
    FeatureThreadData* thread_data = static_cast<FeatureThreadData*>(arg);
    thread_data->success = true;

    for (int i = 0; i < thread_data->num_iterations; i++) {
        float rate;
        bool ok = feature_extractor_compute_mean_firing_rate(
            thread_data->extractor,
            thread_data->data,
            &rate
        );

        if (!ok) {
            thread_data->success = false;
            break;
        }
    }

    return nullptr;
}

TEST_F(FeatureExtractorTest, ThreadSafetyRaceConditions) {
    const int NUM_THREADS = 8;
    const int ITERATIONS = 50;

    spike_data_t* data = create_spike_data(20, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 20; i++) {
        add_regular_spikes(data, i, 0, 1000, 15.0f);
    }

    pthread_t threads[NUM_THREADS];
    FeatureThreadData thread_data[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].extractor = extractor;
        thread_data[i].data = data;
        thread_data[i].num_iterations = ITERATIONS;
        thread_data[i].success = false;

        pthread_create(&threads[i], nullptr, concurrent_rate_computation_thread, &thread_data[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
        EXPECT_TRUE(thread_data[i].success);
    }

    spike_data_destroy(data);
}

//=============================================================================
// 12. Performance Tests
//=============================================================================

TEST_F(FeatureExtractorTest, Performance100Neurons) {
    spike_data_t* data = create_spike_data(100, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 100; i++) {
        add_regular_spikes(data, i, 0, 1000, 10.0f + i * 0.5f);
    }

    middleware_features_t features;

    auto start = std::chrono::high_resolution_clock::now();

    bool success = feature_extractor_update(extractor, data, &features);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_TRUE(success);
    EXPECT_LT(duration.count(), 100000);  // Should complete in < 100ms (relaxed for CI/parallel execution)

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, Performance1000Neurons) {
    spike_data_t* data = create_spike_data(1000, 0, 1000);
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < 1000; i++) {
        add_regular_spikes(data, i, 0, 1000, 5.0f + (i % 50));
    }

    middleware_features_t features;

    auto start = std::chrono::high_resolution_clock::now();

    bool success = feature_extractor_update(extractor, data, &features);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(success);
    EXPECT_LT(duration.count(), 5000);  // Should complete in < 5000ms (relaxed for CI/parallel execution)

    spike_data_destroy(data);
}

TEST_F(FeatureExtractorTest, PerformanceHighSpikeRate) {
    spike_data_t* data = create_spike_data(50, 0, 1000);
    ASSERT_NE(data, nullptr);

    // High firing rates
    for (uint32_t i = 0; i < 50; i++) {
        add_regular_spikes(data, i, 0, 1000, 100.0f);  // 100 Hz
    }

    middleware_features_t features;

    auto start = std::chrono::high_resolution_clock::now();

    bool success = feature_extractor_update(extractor, data, &features);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_TRUE(success);
    EXPECT_LT(duration.count(), 100000);  // Should complete in < 100ms

    spike_data_destroy(data);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
