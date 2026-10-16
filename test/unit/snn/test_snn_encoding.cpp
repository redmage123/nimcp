/**
 * @file test_snn_encoding.cpp
 * @brief Unit tests for SNN Spike Encoding/Decoding Module
 *
 * WHAT: Tests for rate, temporal, population, and latency coding
 * WHY:  Verify correct spike encoding and decoding
 * HOW:  GoogleTest fixtures testing all encoding methods
 *
 * TEST CATEGORIES:
 * 1. Default Configurations - Verify sensible defaults
 * 2. Rate Encoding - Stochastic/deterministic rate coding
 * 3. Temporal Encoding - Time-based spike representation
 * 4. Population Encoding - Distributed representation
 * 5. Latency Encoding - Time-to-first-spike
 * 6. Rate Decoding - Spike count to value
 * 7. First-Spike Decoding - Winner-take-all
 * 8. Population Decoding - Weighted reconstruction
 * 9. Statistics - Encoding/decoding metrics
 * 10. Error Handling - NULL checks and invalid args
 *
 * @author NIMCP Team
 * @date 2024
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <ctime>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn_encoding.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SNNEncodingTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand(42);  /* Reproducible randomness */
    }

    void TearDown() override {}
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(SNNEncodingTest, RateEncoderConfigDefault) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);

    EXPECT_GT(config.max_rate, 0.0f);
    EXPECT_GE(config.min_rate, 0.0f);
    EXPECT_LE(config.min_rate, config.max_rate);
    EXPECT_LT(config.value_min, config.value_max);
    EXPECT_TRUE(config.use_poisson);  /* Default is stochastic */
}

TEST_F(SNNEncodingTest, TemporalEncoderConfigDefault) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);

    EXPECT_GE(config.t_min, 0.0f);
    EXPECT_GT(config.t_max, config.t_min);
    EXPECT_LT(config.value_min, config.value_max);
}

TEST_F(SNNEncodingTest, PopulationEncoderConfigDefault) {
    snn_population_encoder_config_t config;
    snn_population_encoder_config_default(&config);

    EXPECT_GT(config.n_neurons, 0u);
    EXPECT_GT(config.sigma, 0.0f);
    EXPECT_LT(config.value_min, config.value_max);
}

TEST_F(SNNEncodingTest, RateDecoderConfigDefault) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);

    EXPECT_GT(config.time_window, 0.0f);
    EXPECT_GT(config.max_rate, 0.0f);
    EXPECT_GT(config.decay_tau, 0.0f);
}

TEST_F(SNNEncodingTest, NullConfigDefaultHandled) {
    /* Should not crash */
    snn_rate_encoder_config_default(nullptr);
    snn_temporal_encoder_config_default(nullptr);
    snn_population_encoder_config_default(nullptr);
    snn_rate_decoder_config_default(nullptr);
}

//=============================================================================
// Rate Encoder Tests
//=============================================================================

TEST_F(SNNEncodingTest, RateEncoderCreate) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);

    snn_encoder_t* encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(encoder, nullptr);
    EXPECT_EQ(encoder->method, SNN_ENCODE_RATE);
    EXPECT_EQ(encoder->n_inputs, 4u);
    EXPECT_EQ(encoder->n_outputs, 4u);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, RateEncoderCreateNullConfig) {
    snn_encoder_t* encoder = snn_encoder_create_rate(4, nullptr);
    EXPECT_EQ(encoder, nullptr);
}

TEST_F(SNNEncodingTest, RateEncoderCreateZeroInputs) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);

    snn_encoder_t* encoder = snn_encoder_create_rate(0, &config);
    EXPECT_EQ(encoder, nullptr);
}

TEST_F(SNNEncodingTest, RateEncodeBasic) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    config.use_poisson = false;  /* Deterministic for testing */

    snn_encoder_t* encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {0.0f, 0.5f, 1.0f, 0.25f};
    uint8_t spikes[4];

    int result = snn_encode_rate(encoder, values, 0.1f, spikes);
    EXPECT_EQ(result, SNN_SUCCESS);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, RateEncodeHighValueMoreSpikes) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    config.use_poisson = true;

    snn_encoder_t* encoder = snn_encoder_create_rate(2, &config);
    ASSERT_NE(encoder, nullptr);

    float high_value[] = {1.0f, 1.0f};
    float low_value[] = {0.0f, 0.0f};
    uint8_t spikes[2];

    /* Run many iterations to verify high values generate more spikes */
    int high_count = 0, low_count = 0;
    for (int i = 0; i < 1000; i++) {
        snn_encode_rate(encoder, high_value, 1.0f, spikes);
        high_count += spikes[0] + spikes[1];

        snn_encode_rate(encoder, low_value, 1.0f, spikes);
        low_count += spikes[0] + spikes[1];
    }

    EXPECT_GT(high_count, low_count);  /* High values should spike more */

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, RateEncodeNullArgs) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);

    snn_encoder_t* encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint8_t spikes[4];

    EXPECT_EQ(snn_encode_rate(nullptr, values, 0.1f, spikes), SNN_ERROR_NULL_POINTER);
    EXPECT_EQ(snn_encode_rate(encoder, nullptr, 0.1f, spikes), SNN_ERROR_NULL_POINTER);
    EXPECT_EQ(snn_encode_rate(encoder, values, 0.1f, nullptr), SNN_ERROR_NULL_POINTER);

    snn_encoder_destroy(encoder);
}

//=============================================================================
// Temporal Encoder Tests
//=============================================================================

TEST_F(SNNEncodingTest, TemporalEncoderCreate) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);

    snn_encoder_t* encoder = snn_encoder_create_temporal(8, &config);
    ASSERT_NE(encoder, nullptr);
    EXPECT_EQ(encoder->method, SNN_ENCODE_TEMPORAL);
    EXPECT_EQ(encoder->n_inputs, 8u);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, TemporalEncodeHighValueEarlySpike) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);
    config.inverse = false;  /* High value = early spike */

    snn_encoder_t* encoder = snn_encoder_create_temporal(2, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {1.0f, 0.0f};
    float spike_times[2];

    int result = snn_encode_temporal(encoder, values, spike_times);
    EXPECT_EQ(result, SNN_SUCCESS);
    EXPECT_LT(spike_times[0], spike_times[1]);  /* High value = earlier spike */

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, TemporalEncodeInverseMode) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);
    config.inverse = true;  /* High value = late spike */

    snn_encoder_t* encoder = snn_encoder_create_temporal(2, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {1.0f, 0.0f};
    float spike_times[2];

    int result = snn_encode_temporal(encoder, values, spike_times);
    EXPECT_EQ(result, SNN_SUCCESS);
    EXPECT_GT(spike_times[0], spike_times[1]);  /* High value = later spike (inverse) */

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, TemporalEncodeBounds) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);

    snn_encoder_t* encoder = snn_encoder_create_temporal(3, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {0.0f, 0.5f, 1.0f};
    float spike_times[3];

    snn_encode_temporal(encoder, values, spike_times);

    /* All spike times should be within bounds */
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(spike_times[i], config.t_min);
        EXPECT_LE(spike_times[i], config.t_max);
    }

    snn_encoder_destroy(encoder);
}

//=============================================================================
// Population Encoder Tests
//=============================================================================

TEST_F(SNNEncodingTest, PopulationEncoderCreate) {
    snn_population_encoder_config_t config;
    snn_population_encoder_config_default(&config);
    config.n_neurons = 8;

    snn_encoder_t* encoder = snn_encoder_create_population(2, &config);
    ASSERT_NE(encoder, nullptr);
    EXPECT_EQ(encoder->method, SNN_ENCODE_POPULATION);
    EXPECT_EQ(encoder->n_inputs, 2u);
    EXPECT_EQ(encoder->n_outputs, 16u);  /* 2 inputs * 8 neurons per input */

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, PopulationEncodeGaussian) {
    snn_population_encoder_config_t config;
    snn_population_encoder_config_default(&config);
    config.n_neurons = 5;
    config.normalize_rates = true;

    snn_encoder_t* encoder = snn_encoder_create_population(1, &config);
    ASSERT_NE(encoder, nullptr);

    float value = 0.5f;  /* Should peak at middle neuron */
    float rates[5];

    int result = snn_encode_population(encoder, &value, rates);
    EXPECT_EQ(result, SNN_SUCCESS);

    /* Middle neuron (index 2) should have highest rate */
    int max_idx = 0;
    for (int i = 1; i < 5; i++) {
        if (rates[i] > rates[max_idx]) max_idx = i;
    }
    EXPECT_EQ(max_idx, 2);

    /* Rates should sum to approximately 1 (normalized) */
    float sum = 0.0f;
    for (int i = 0; i < 5; i++) sum += rates[i];
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, PopulationEncodeEdgeValue) {
    snn_population_encoder_config_t config;
    snn_population_encoder_config_default(&config);
    config.n_neurons = 5;

    snn_encoder_t* encoder = snn_encoder_create_population(1, &config);
    ASSERT_NE(encoder, nullptr);

    float value = 0.0f;  /* Should peak at first neuron */
    float rates[5];

    snn_encode_population(encoder, &value, rates);

    /* First neuron should have highest rate */
    for (int i = 1; i < 5; i++) {
        EXPECT_GE(rates[0], rates[i]);
    }

    snn_encoder_destroy(encoder);
}

//=============================================================================
// Latency Encoder Tests
//=============================================================================

TEST_F(SNNEncodingTest, LatencyEncoderCreate) {
    snn_latency_encoder_config_t config = {
        .tau = 10.0f,
        .threshold = 0.1f,
        .t_max = 50.0f,
        .use_log = false
    };

    snn_encoder_t* encoder = snn_encoder_create_latency(4, &config);
    ASSERT_NE(encoder, nullptr);
    EXPECT_EQ(encoder->method, SNN_ENCODE_LATENCY);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, LatencyEncodeHighValueShortLatency) {
    snn_latency_encoder_config_t config = {
        .tau = 10.0f,
        .threshold = 0.0f,
        .t_max = 50.0f,
        .use_log = false
    };

    snn_encoder_t* encoder = snn_encoder_create_latency(2, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {1.0f, 0.2f};
    float latencies[2];

    int result = snn_encode_latency(encoder, values, latencies);
    EXPECT_EQ(result, SNN_SUCCESS);
    EXPECT_LT(latencies[0], latencies[1]);  /* High value = shorter latency */

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, LatencyEncodeBelowThreshold) {
    snn_latency_encoder_config_t config = {
        .tau = 10.0f,
        .threshold = 0.5f,
        .t_max = 50.0f,
        .use_log = false
    };

    snn_encoder_t* encoder = snn_encoder_create_latency(1, &config);
    ASSERT_NE(encoder, nullptr);

    float value = 0.3f;  /* Below threshold */
    float latency;

    snn_encode_latency(encoder, &value, &latency);
    EXPECT_FLOAT_EQ(latency, config.t_max);  /* No spike = max latency */

    snn_encoder_destroy(encoder);
}

//=============================================================================
// Rate Decoder Tests
//=============================================================================

TEST_F(SNNEncodingTest, RateDecoderCreate) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);

    snn_decoder_t* decoder = snn_decoder_create_rate(10, 10, &config);
    ASSERT_NE(decoder, nullptr);
    EXPECT_EQ(decoder->method, SNN_DECODE_RATE);
    EXPECT_EQ(decoder->n_inputs, 10u);
    EXPECT_EQ(decoder->n_outputs, 10u);

    snn_decoder_destroy(decoder);
}

TEST_F(SNNEncodingTest, RateDecodeBasic) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);
    config.max_rate = 100.0f;
    config.time_window = 100.0f;  /* 100 ms */

    snn_decoder_t* decoder = snn_decoder_create_rate(2, 2, &config);
    ASSERT_NE(decoder, nullptr);

    /* 10 spikes in 100ms = 100 Hz = max rate */
    float spike_counts[] = {10.0f, 5.0f};
    float values[2];

    int result = snn_decode_rate(decoder, spike_counts, values);
    EXPECT_EQ(result, SNN_SUCCESS);
    EXPECT_NEAR(values[0], 1.0f, 0.01f);  /* 100 Hz / 100 Hz = 1.0 */
    EXPECT_NEAR(values[1], 0.5f, 0.01f);  /* 50 Hz / 100 Hz = 0.5 */

    snn_decoder_destroy(decoder);
}

TEST_F(SNNEncodingTest, RateDecodeNullArgs) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);

    snn_decoder_t* decoder = snn_decoder_create_rate(2, 2, &config);
    ASSERT_NE(decoder, nullptr);

    float spike_counts[] = {5.0f, 5.0f};
    float values[2];

    EXPECT_EQ(snn_decode_rate(nullptr, spike_counts, values), SNN_ERROR_NULL_POINTER);
    EXPECT_EQ(snn_decode_rate(decoder, nullptr, values), SNN_ERROR_NULL_POINTER);
    EXPECT_EQ(snn_decode_rate(decoder, spike_counts, nullptr), SNN_ERROR_NULL_POINTER);

    snn_decoder_destroy(decoder);
}

//=============================================================================
// First-Spike Decoder Tests
//=============================================================================

TEST_F(SNNEncodingTest, FirstSpikeDecoderCreate) {
    snn_first_spike_decoder_config_t config = {
        .max_latency = 100.0f,
        .use_softmax = false,
        .temperature = 1.0f
    };

    snn_decoder_t* decoder = snn_decoder_create_first_spike(10, &config);
    ASSERT_NE(decoder, nullptr);
    EXPECT_EQ(decoder->method, SNN_DECODE_FIRST_SPIKE);
    EXPECT_EQ(decoder->n_inputs, 10u);
    EXPECT_EQ(decoder->n_outputs, 1u);  /* Single class output */

    snn_decoder_destroy(decoder);
}

TEST_F(SNNEncodingTest, FirstSpikeDecodeWinner) {
    snn_first_spike_decoder_config_t config = {
        .max_latency = 100.0f,
        .use_softmax = false,
        .temperature = 1.0f
    };

    snn_decoder_t* decoder = snn_decoder_create_first_spike(4, &config);
    ASSERT_NE(decoder, nullptr);

    /* Neuron 2 spikes first */
    float spike_times[] = {20.0f, 15.0f, 5.0f, 25.0f};
    uint32_t winner;
    float confidence;

    int result = snn_decode_first_spike(decoder, spike_times, &winner, &confidence);
    EXPECT_EQ(result, SNN_SUCCESS);
    EXPECT_EQ(winner, 2u);  /* Neuron 2 had earliest spike */
    EXPECT_GT(confidence, 0.0f);

    snn_decoder_destroy(decoder);
}

TEST_F(SNNEncodingTest, FirstSpikeDecodeWithSoftmax) {
    snn_first_spike_decoder_config_t config = {
        .max_latency = 100.0f,
        .use_softmax = true,
        .temperature = 5.0f
    };

    snn_decoder_t* decoder = snn_decoder_create_first_spike(3, &config);
    ASSERT_NE(decoder, nullptr);

    float spike_times[] = {10.0f, 50.0f, 90.0f};
    uint32_t winner;
    float confidence;

    snn_decode_first_spike(decoder, spike_times, &winner, &confidence);
    EXPECT_EQ(winner, 0u);  /* First neuron is fastest */
    EXPECT_GT(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    snn_decoder_destroy(decoder);
}

//=============================================================================
// Population Decoder Tests
//=============================================================================

TEST_F(SNNEncodingTest, PopulationDecoderCreate) {
    snn_population_decoder_config_t config = {
        .time_window = 50.0f,
        .normalize = true,
        .preferred_values = nullptr,
        .n_neurons = 5
    };

    snn_decoder_t* decoder = snn_decoder_create_population(5, 1, &config);
    ASSERT_NE(decoder, nullptr);
    EXPECT_EQ(decoder->method, SNN_DECODE_POPULATION);

    snn_decoder_destroy(decoder);
}

TEST_F(SNNEncodingTest, PopulationDecodeVectorSum) {
    snn_population_decoder_config_t config = {
        .time_window = 50.0f,
        .normalize = true,
        .preferred_values = nullptr,
        .n_neurons = 5
    };

    snn_decoder_t* decoder = snn_decoder_create_population(5, 1, &config);
    ASSERT_NE(decoder, nullptr);

    /* High activity at middle neuron (preferred = 0.5) */
    float activities[] = {0.0f, 0.2f, 1.0f, 0.2f, 0.0f};
    float value;

    int result = snn_decode_population(decoder, activities, &value);
    EXPECT_EQ(result, SNN_SUCCESS);
    EXPECT_NEAR(value, 0.5f, 0.1f);  /* Should be close to middle */

    snn_decoder_destroy(decoder);
}

//=============================================================================
// Generic Encode/Decode Tests
//=============================================================================

TEST_F(SNNEncodingTest, GenericEncodeDispatch) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    config.use_poisson = false;

    snn_encoder_t* encoder = snn_encoder_create_rate(2, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {0.5f, 0.5f};
    uint8_t spikes[2];

    /* Use generic encode function */
    int result = snn_encode(encoder, values, 0.1f, spikes);
    EXPECT_EQ(result, SNN_SUCCESS);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, GenericDecodeDispatch) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);

    snn_decoder_t* decoder = snn_decoder_create_rate(2, 2, &config);
    ASSERT_NE(decoder, nullptr);

    float spike_counts[] = {5.0f, 5.0f};
    float values[2];

    /* Use generic decode function */
    int result = snn_decode(decoder, spike_counts, values);
    EXPECT_EQ(result, SNN_SUCCESS);

    snn_decoder_destroy(decoder);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SNNEncodingTest, EncoderStatisticsTracking) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    config.use_poisson = true;

    snn_encoder_t* encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {1.0f, 1.0f, 1.0f, 1.0f};  /* Max values */
    uint8_t spikes[4];

    /* Initial stats should be zero */
    uint64_t total_spikes, encode_count;
    snn_encoder_get_stats(encoder, &total_spikes, &encode_count);
    EXPECT_EQ(total_spikes, 0u);
    EXPECT_EQ(encode_count, 0u);

    /* Encode multiple times */
    for (int i = 0; i < 10; i++) {
        snn_encode_rate(encoder, values, 1.0f, spikes);
    }

    snn_encoder_get_stats(encoder, &total_spikes, &encode_count);
    EXPECT_EQ(encode_count, 10u);
    EXPECT_GT(total_spikes, 0u);  /* Should have some spikes */

    /* Reset stats */
    snn_encoder_reset_stats(encoder);
    snn_encoder_get_stats(encoder, &total_spikes, &encode_count);
    EXPECT_EQ(total_spikes, 0u);
    EXPECT_EQ(encode_count, 0u);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, DecoderStatisticsTracking) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);

    snn_decoder_t* decoder = snn_decoder_create_rate(4, 4, &config);
    ASSERT_NE(decoder, nullptr);

    float spike_counts[] = {5.0f, 5.0f, 5.0f, 5.0f};
    float values[4];

    /* Initial stats should be zero */
    uint64_t total_outputs, decode_count;
    snn_decoder_get_stats(decoder, &total_outputs, &decode_count);
    EXPECT_EQ(total_outputs, 0u);
    EXPECT_EQ(decode_count, 0u);

    /* Decode multiple times */
    for (int i = 0; i < 5; i++) {
        snn_decode_rate(decoder, spike_counts, values);
    }

    snn_decoder_get_stats(decoder, &total_outputs, &decode_count);
    EXPECT_EQ(decode_count, 5u);
    EXPECT_EQ(total_outputs, 20u);  /* 5 decodes * 4 outputs */

    /* Reset stats */
    snn_decoder_reset_stats(decoder);
    snn_decoder_get_stats(decoder, &total_outputs, &decode_count);
    EXPECT_EQ(total_outputs, 0u);
    EXPECT_EQ(decode_count, 0u);

    snn_decoder_destroy(decoder);
}

TEST_F(SNNEncodingTest, StatsNullHandled) {
    /* Should not crash */
    snn_encoder_get_stats(nullptr, nullptr, nullptr);
    snn_decoder_get_stats(nullptr, nullptr, nullptr);
    snn_encoder_reset_stats(nullptr);
    snn_decoder_reset_stats(nullptr);
}

//=============================================================================
// Destroy Tests
//=============================================================================

TEST_F(SNNEncodingTest, EncoderDestroyNull) {
    /* Should not crash */
    snn_encoder_destroy(nullptr);
}

TEST_F(SNNEncodingTest, DecoderDestroyNull) {
    /* Should not crash */
    snn_decoder_destroy(nullptr);
}

//=============================================================================
// Wrong Method Tests
//=============================================================================

TEST_F(SNNEncodingTest, EncodeWrongMethod) {
    snn_temporal_encoder_config_t config;
    snn_temporal_encoder_config_default(&config);

    snn_encoder_t* encoder = snn_encoder_create_temporal(2, &config);
    ASSERT_NE(encoder, nullptr);

    float values[] = {0.5f, 0.5f};
    uint8_t spikes[2];

    /* Try rate encoding with temporal encoder - should fail */
    int result = snn_encode_rate(encoder, values, 0.1f, spikes);
    EXPECT_EQ(result, SNN_ERROR_INVALID_CONFIG);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, DecodeWrongMethod) {
    snn_first_spike_decoder_config_t config = {
        .max_latency = 100.0f,
        .use_softmax = false,
        .temperature = 1.0f
    };

    snn_decoder_t* decoder = snn_decoder_create_first_spike(4, &config);
    ASSERT_NE(decoder, nullptr);

    float spike_counts[] = {5.0f, 5.0f, 5.0f, 5.0f};
    float values[4];

    /* Try rate decoding with first-spike decoder - should fail */
    int result = snn_decode_rate(decoder, spike_counts, values);
    EXPECT_EQ(result, SNN_ERROR_INVALID_CONFIG);

    snn_decoder_destroy(decoder);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SNNEncodingTest, EncodeClampValues) {
    snn_rate_encoder_config_t config;
    snn_rate_encoder_config_default(&config);
    config.value_min = 0.0f;
    config.value_max = 1.0f;

    snn_encoder_t* encoder = snn_encoder_create_rate(4, &config);
    ASSERT_NE(encoder, nullptr);

    /* Values outside [0, 1] should be clamped */
    float values[] = {-1.0f, 0.5f, 1.5f, 2.0f};
    uint8_t spikes[4];

    /* Should not crash */
    int result = snn_encode_rate(encoder, values, 0.1f, spikes);
    EXPECT_EQ(result, SNN_SUCCESS);

    snn_encoder_destroy(encoder);
}

TEST_F(SNNEncodingTest, DecodeClampOutput) {
    snn_rate_decoder_config_t config;
    snn_rate_decoder_config_default(&config);
    config.max_rate = 100.0f;
    config.time_window = 100.0f;

    snn_decoder_t* decoder = snn_decoder_create_rate(2, 2, &config);
    ASSERT_NE(decoder, nullptr);

    /* Very high spike count */
    float spike_counts[] = {100.0f, 200.0f};  /* Way above max rate */
    float values[2];

    snn_decode_rate(decoder, spike_counts, values);
    EXPECT_LE(values[0], 1.0f);  /* Should be clamped */
    EXPECT_LE(values[1], 1.0f);

    snn_decoder_destroy(decoder);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
