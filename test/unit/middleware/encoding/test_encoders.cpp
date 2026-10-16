//=============================================================================
// test_encoders.cpp - Comprehensive Encoder Integration Tests
//=============================================================================
/**
 * This test suite tests all 3 encoders together to ensure:
 * - Consistent API across encoders
 * - Proper encoding/decoding roundtrip
 * - Integration between different coding schemes
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <memory>
#include <algorithm>

// Headers have their own extern "C" guards
#include "middleware/encoding/nimcp_rate_coding.h"
#include "middleware/encoding/nimcp_temporal_coding.h"
#include "middleware/encoding/nimcp_population_coding.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EncodersTest : public ::testing::Test {
protected:
    rate_coding_encoder_t rate_encoder;
    temporal_coding_encoder_t temporal_encoder;
    population_coding_encoder_t population_encoder;

    static constexpr size_t NUM_NEURONS = 100;
    static constexpr float EPSILON = 1e-3f;

    void SetUp() override {
        // Create rate encoder
        rate_coding_config_t rate_config = rate_coding_default_config();
        rate_encoder = rate_coding_create(&rate_config);

        // Create temporal encoder
        temporal_coding_config_t temporal_config = temporal_coding_default_config();
        temporal_encoder = temporal_coding_create(&temporal_config);

        // Create population encoder
        population_coding_config_t pop_config = population_coding_default_config();
        population_encoder = population_coding_create(&pop_config);

        ASSERT_NE(rate_encoder, nullptr);
        ASSERT_NE(temporal_encoder, nullptr);
        ASSERT_NE(population_encoder, nullptr);
    }

    void TearDown() override {
        rate_coding_destroy(rate_encoder);
        temporal_coding_destroy(temporal_encoder);
        population_coding_destroy(population_encoder);
    }

    // Helper: Create spike train with regular spikes
    spike_train_t* createRegularSpikeTrain(float rate_hz, float duration_ms) {
        spike_train_t* train = rate_coding_spike_train_create(1000);
        if (!train) return nullptr;

        float isi_ms = 1000.0f / rate_hz;
        uint64_t time = 0;

        while (time < duration_ms) {
            spike_train_add_spike(train, time);
            time += static_cast<uint64_t>(isi_ms);
        }

        return train;
    }

    // Helper: Create Poisson spike train
    spike_train_t* createPoissonSpikeTrain(float rate_hz, float duration_ms) {
        spike_train_t* train = rate_coding_spike_train_create(1000);
        if (!train) return nullptr;

        bool success = rate_coding_decode(
            rate_encoder, rate_hz, duration_ms, true, train);

        return success ? train : nullptr;
    }
};

//=============================================================================
// Spike Train Utility Tests
//=============================================================================

TEST_F(EncodersTest, CreateAndDestroySpikeTrain) {
    spike_train_t* train = rate_coding_spike_train_create(100);
    ASSERT_NE(train, nullptr);

    EXPECT_EQ(train->num_spikes, 0);
    EXPECT_GE(train->capacity, 100);

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, AddSpikesToTrain) {
    spike_train_t* train = rate_coding_spike_train_create(10);
    ASSERT_NE(train, nullptr);

    // Add spikes
    for (uint64_t i = 0; i < 5; i++) {
        bool added = spike_train_add_spike(train, i * 100);
        EXPECT_TRUE(added);
    }

    EXPECT_EQ(train->num_spikes, 5);

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, ClearSpikeTrain) {
    spike_train_t* train = rate_coding_spike_train_create(10);
    ASSERT_NE(train, nullptr);

    // Add spikes
    for (int i = 0; i < 5; i++) {
        spike_train_add_spike(train, i * 100);
    }

    // Clear
    rate_coding_spike_train_clear(train);

    EXPECT_EQ(train->num_spikes, 0);

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, CopySpikeTrain) {
    spike_train_t* original = rate_coding_spike_train_create(10);
    ASSERT_NE(original, nullptr);

    for (int i = 0; i < 5; i++) {
        spike_train_add_spike(original, i * 100);
    }

    spike_train_t* copy = spike_train_copy(original);
    ASSERT_NE(copy, nullptr);

    EXPECT_EQ(copy->num_spikes, original->num_spikes);

    for (uint32_t i = 0; i < original->num_spikes; i++) {
        EXPECT_EQ(copy->spike_times[i], original->spike_times[i]);
    }

    rate_coding_spike_train_destroy(original);
    rate_coding_spike_train_destroy(copy);
}

//=============================================================================
// Rate Coding Tests
//=============================================================================

TEST_F(EncodersTest, RateCodingEncodeBasic) {
    spike_train_t* train = createRegularSpikeTrain(50.0f, 1000.0f);
    ASSERT_NE(train, nullptr);

    float rate_out;
    bool success = rate_coding_encode(
        rate_encoder, train, 1000, &rate_out);

    EXPECT_TRUE(success);
    EXPECT_NEAR(rate_out, 50.0f, 10.0f);

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, RateCodingDecodeBasic) {
    spike_train_t* train = rate_coding_spike_train_create(1000);
    ASSERT_NE(train, nullptr);

    bool success = rate_coding_decode(
        rate_encoder, 50.0f, 1000.0f, false, train);

    EXPECT_TRUE(success);
    EXPECT_GT(train->num_spikes, 0);

    // Verify average rate
    float actual_rate = (train->num_spikes * 1000.0f) / 1000.0f;
    EXPECT_NEAR(actual_rate, 50.0f, 5.0f);

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, RateCodingRoundtrip) {
    float original_rate = 30.0f;

    // Decode to spikes
    spike_train_t* train = rate_coding_spike_train_create(1000);
    rate_coding_decode(rate_encoder, original_rate, 1000.0f, false, train);

    // Encode back to rate
    float decoded_rate;
    rate_coding_encode(rate_encoder, train, 1000, &decoded_rate);

    EXPECT_NEAR(decoded_rate, original_rate, 5.0f);

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, RateCodingMultiscale) {
    // Use 200Hz rate (ISI=5ms) so even the 10ms window captures spikes
    spike_train_t* train = createRegularSpikeTrain(200.0f, 1000.0f);
    ASSERT_NE(train, nullptr);

    float windows_ms[] = {10.0f, 100.0f, 1000.0f};
    float rates[3];

    uint32_t encoded = rate_coding_encode_multiscale(
        rate_encoder, train, 1000, windows_ms, 3, rates);

    EXPECT_EQ(encoded, 3);

    // All rates should be positive
    for (int i = 0; i < 3; i++) {
        EXPECT_GT(rates[i], 0.0f);
    }

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, RateCodingPopulation) {
    std::vector<spike_train_t*> trains(10);

    // Create trains with different rates
    for (int i = 0; i < 10; i++) {
        trains[i] = createRegularSpikeTrain((i + 1) * 10.0f, 1000.0f);
    }

    std::vector<float> rates(10);

    // Create array of spike_train_t (not pointers) for the API
    std::vector<spike_train_t> train_array(10);
    for (int i = 0; i < 10; i++) {
        if (trains[i]) {
            train_array[i] = *trains[i];
        }
    }

    uint32_t encoded = rate_coding_encode_population(
        rate_encoder, train_array.data(), 10, 1000, rates.data());

    EXPECT_EQ(encoded, 10);

    // Check rates are increasing
    for (int i = 1; i < 10; i++) {
        EXPECT_GT(rates[i], rates[i-1]);
    }

    for (auto* train : trains) {
        rate_coding_spike_train_destroy(train);
    }
}

//=============================================================================
// Temporal Coding Tests
//=============================================================================

TEST_F(EncodersTest, TemporalCodingEncodeBasic) {
    spike_train_t* train = createRegularSpikeTrain(50.0f, 1000.0f);
    ASSERT_NE(train, nullptr);

    temporal_features_t* features = temporal_features_create(10);
    ASSERT_NE(features, nullptr);

    bool success = temporal_coding_encode(
        temporal_encoder, train, 0, features);

    EXPECT_TRUE(success);
    EXPECT_GT(features->num_spikes, 0);
    EXPECT_GT(features->mean_isi, 0.0f);

    temporal_features_destroy(features);
    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, TemporalCodingDecodeBasic) {
    temporal_features_t* features = temporal_features_create(10);
    ASSERT_NE(features, nullptr);

    // Set up features
    features->first_spike_latency = 10.0f;
    features->mean_isi = 20.0f;
    features->isi_std = 2.0f;
    features->num_spikes = 50;

    spike_train_t* train = rate_coding_spike_train_create(1000);
    ASSERT_NE(train, nullptr);

    bool success = temporal_coding_decode(
        temporal_encoder, features, 1000.0f, train);

    EXPECT_TRUE(success);
    EXPECT_GT(train->num_spikes, 0);

    temporal_features_destroy(features);
    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, TemporalCodingFirstSpikeLatency) {
    spike_train_t* train = rate_coding_spike_train_create(100);
    spike_train_add_spike(train, 50);  // First spike at 50ms
    spike_train_add_spike(train, 100);
    spike_train_add_spike(train, 150);

    temporal_features_t* features = temporal_features_create(10);
    temporal_coding_encode(temporal_encoder, train, 0, features);

    EXPECT_NEAR(features->first_spike_latency, 50.0f, 1.0f);

    temporal_features_destroy(features);
    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, TemporalCodingISIStatistics) {
    spike_train_t* train = createRegularSpikeTrain(50.0f, 1000.0f);
    ASSERT_NE(train, nullptr);

    temporal_features_t* features = temporal_features_create(10);
    temporal_coding_encode(temporal_encoder, train, 0, features);

    // Regular spikes should have low ISI CV
    EXPECT_LT(features->isi_cv, 0.1f);

    temporal_features_destroy(features);
    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, TemporalCodingCorrelation) {
    spike_train_t* train1 = createRegularSpikeTrain(50.0f, 1000.0f);
    spike_train_t* train2 = createRegularSpikeTrain(50.0f, 1000.0f);
    ASSERT_NE(train1, nullptr);
    ASSERT_NE(train2, nullptr);

    float correlation;
    // Use max_lag = 500ms to capture enough spike pairs for high correlation
    // (with ISI=20ms and max_lag=500ms, ~25 of 50 spikes per pair match => ~0.5)
    bool success = temporal_coding_compute_correlation(
        temporal_encoder, train1, train2, 500.0f, &correlation);

    EXPECT_TRUE(success);
    EXPECT_GT(correlation, 0.4f) << "Identical trains should correlate";

    rate_coding_spike_train_destroy(train1);
    rate_coding_spike_train_destroy(train2);
}

//=============================================================================
// Population Coding Tests
//=============================================================================

TEST_F(EncodersTest, PopulationCodingVectorSum) {
    std::vector<float> rates = {1.0f, 0.5f, 0.2f, 0.8f};
    std::vector<tuning_curve_t> tuning(4);

    // Set up tuning curves with different directions
    tuning[0].preferred_direction = population_coding_vector3d_make(1.0f, 0.0f, 0.0f);
    tuning[1].preferred_direction = population_coding_vector3d_make(0.0f, 1.0f, 0.0f);
    tuning[2].preferred_direction = population_coding_vector3d_make(0.0f, 0.0f, 1.0f);
    tuning[3].preferred_direction = population_coding_vector3d_make(1.0f, 1.0f, 0.0f);

    for (auto& curve : tuning) {
        curve.tuning_width = 0.5f;
        curve.max_rate = 100.0f;
        population_coding_vector3d_normalize(&curve.preferred_direction);
    }

    vector3d_t result;
    bool success = population_coding_encode_vector_sum(
        population_encoder, rates.data(), tuning.data(), 4, &result);

    EXPECT_TRUE(success);
    EXPECT_GT(result.magnitude, 0.0f);
}

TEST_F(EncodersTest, PopulationCodingCenterOfMass) {
    std::vector<float> rates = {1.0f, 0.5f, 0.8f, 0.2f};
    std::vector<vector3d_t> positions(4);

    positions[0] = population_coding_vector3d_make(0.0f, 0.0f, 0.0f);
    positions[1] = population_coding_vector3d_make(1.0f, 0.0f, 0.0f);
    positions[2] = population_coding_vector3d_make(0.0f, 1.0f, 0.0f);
    positions[3] = population_coding_vector3d_make(1.0f, 1.0f, 0.0f);

    vector3d_t center;
    bool success = population_coding_encode_center_of_mass(
        population_encoder, rates.data(), positions.data(), 4, &center);

    EXPECT_TRUE(success);
    // Center should be somewhere in the middle
    EXPECT_GE(center.x, 0.0f);
    EXPECT_LE(center.x, 1.0f);
    EXPECT_GE(center.y, 0.0f);
    EXPECT_LE(center.y, 1.0f);
}

TEST_F(EncodersTest, PopulationCodingSparseEncoding) {
    std::vector<float> rates(100);
    for (int i = 0; i < 100; i++) {
        rates[i] = static_cast<float>(i);
    }

    // Use unique_ptr for bool array since std::vector<bool> doesn't have data()
    std::unique_ptr<bool[]> sparse_code(new bool[100]);
    uint32_t active = population_coding_encode_sparse(
        population_encoder, rates.data(), 100, sparse_code.get());

    EXPECT_GT(active, 0);
    EXPECT_LT(active, 100);

    // Count active neurons
    uint32_t count = 0;
    for (int i = 0; i < 100; i++) {
        if (sparse_code[i]) count++;
    }

    EXPECT_EQ(count, active);
}

TEST_F(EncodersTest, PopulationCodingSparseOverlap) {
    // Use unique_ptr for bool arrays since std::vector<bool> doesn't have data()
    std::unique_ptr<bool[]> code1(new bool[100]);
    std::unique_ptr<bool[]> code2(new bool[100]);

    // Initialize to false
    std::fill_n(code1.get(), 100, false);
    std::fill_n(code2.get(), 100, false);

    // Set some overlapping bits
    for (int i = 0; i < 10; i++) {
        code1[i] = true;
        code2[i] = (i % 2 == 0); // 5 overlap
    }

    float overlap = population_coding_sparse_overlap(
        code1.get(), code2.get(), 100);

    EXPECT_GE(overlap, 0.0f);
    EXPECT_LE(overlap, 1.0f);
}

//=============================================================================
// Cross-Encoder Integration Tests
//=============================================================================

TEST_F(EncodersTest, RateToTemporalConsistency) {
    // Create spikes at known rate
    spike_train_t* train = createRegularSpikeTrain(50.0f, 1000.0f);

    // Encode with rate encoder
    float rate;
    rate_coding_encode(rate_encoder, train, 1000, &rate);

    // Encode with temporal encoder
    temporal_features_t* features = temporal_features_create(10);
    temporal_coding_encode(temporal_encoder, train, 0, features);

    // Temporal mean ISI should match rate
    float isi_rate = 1000.0f / features->mean_isi;
    EXPECT_NEAR(rate, isi_rate, 10.0f);

    temporal_features_destroy(features);
    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, RateToPopulationConsistency) {
    std::vector<spike_train_t*> trains(10);
    std::vector<float> expected_rates;

    // Create trains
    for (int i = 0; i < 10; i++) {
        float rate = (i + 1) * 10.0f;
        expected_rates.push_back(rate);
        trains[i] = createRegularSpikeTrain(rate, 1000.0f);
    }

    // Encode with rate encoder - need array of structs
    std::vector<spike_train_t> train_array(10);
    for (int i = 0; i < 10; i++) {
        if (trains[i]) {
            train_array[i] = *trains[i];
        }
    }

    std::vector<float> rates(10);
    rate_coding_encode_population(
        rate_encoder, train_array.data(), 10, 1000, rates.data());

    // Use for population vector
    std::vector<tuning_curve_t> tuning(10);
    for (int i = 0; i < 10; i++) {
        float angle = (i / 10.0f) * 2.0f * M_PI;
        tuning[i].preferred_direction = population_coding_vector3d_make(
            cosf(angle), sinf(angle), 0.0f);
        tuning[i].max_rate = 100.0f;
        population_coding_vector3d_normalize(&tuning[i].preferred_direction);
    }

    vector3d_t pop_vector;
    population_coding_encode_vector_sum(
        population_encoder, rates.data(), tuning.data(), 10, &pop_vector);

    EXPECT_GT(pop_vector.magnitude, 0.0f);

    for (auto* train : trains) {
        rate_coding_spike_train_destroy(train);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EncodersTest, ComputeCV) {
    spike_train_t* regular = createRegularSpikeTrain(50.0f, 1000.0f);
    spike_train_t* poisson = createPoissonSpikeTrain(50.0f, 1000.0f);

    float cv_regular, cv_poisson;
    rate_coding_compute_cv(regular, &cv_regular);
    rate_coding_compute_cv(poisson, &cv_poisson);

    // Regular spikes should have low CV
    EXPECT_LT(cv_regular, 0.5f);

    // Poisson spikes should have CV around 1
    EXPECT_NEAR(cv_poisson, 1.0f, 0.5f);

    rate_coding_spike_train_destroy(regular);
    rate_coding_spike_train_destroy(poisson);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(EncodersTest, EmptySpikeTrain) {
    spike_train_t* train = rate_coding_spike_train_create(10);

    float rate;
    bool success = rate_coding_encode(rate_encoder, train, 1000, &rate);

    // Should handle empty train
    (void)success;

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, SingleSpike) {
    spike_train_t* train = rate_coding_spike_train_create(10);
    spike_train_add_spike(train, 950);  // Within 100ms window ending at 1000

    float rate;
    bool success = rate_coding_encode(rate_encoder, train, 1000, &rate);

    EXPECT_TRUE(success);
    EXPECT_GT(rate, 0.0f);

    rate_coding_spike_train_destroy(train);
}

TEST_F(EncodersTest, ZeroRate) {
    spike_train_t* train = rate_coding_spike_train_create(10);

    bool success = rate_coding_decode(
        rate_encoder, 0.0f, 1000.0f, false, train);

    EXPECT_TRUE(success);
    EXPECT_EQ(train->num_spikes, 0);

    rate_coding_spike_train_destroy(train);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
