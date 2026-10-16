/**
 * @file test_triplet_stdp_regression.cpp
 * @brief Regression tests for Triplet STDP
 * @version 1.0.0
 * @date 2025-12-19
 *
 * Tests ensuring triplet STDP behavior remains stable across changes
 */

#include <gtest/gtest.h>
#include <vector>
#include "plasticity/stdp/nimcp_triplet_stdp.h"

class TripletSTDPRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        synapse = nullptr;
    }

    void TearDown() override {
        if (synapse) {
            triplet_stdp_synapse_destroy(synapse);
            synapse = nullptr;
        }
    }

    triplet_stdp_synapse_t* synapse;
};

TEST_F(TripletSTDPRegressionTest, PfisterGerstnerVisualCortexParams) {
    /* Verify default parameters match Pfister & Gerstner (2006) paper */
    triplet_stdp_config_t config = triplet_stdp_config_default();

    EXPECT_FLOAT_EQ(config.A2_plus, 0.005f);
    EXPECT_FLOAT_EQ(config.A3_plus, 0.0062f);
    EXPECT_FLOAT_EQ(config.A2_minus, 0.007f);
    EXPECT_FLOAT_EQ(config.A3_minus, 0.00023f);
    EXPECT_FLOAT_EQ(config.tau_plus, 16.8f);
    EXPECT_FLOAT_EQ(config.tau_minus, 33.7f);
    EXPECT_FLOAT_EQ(config.tau_x, 101.0f);
    EXPECT_FLOAT_EQ(config.tau_y, 125.0f);
}

TEST_F(TripletSTDPRegressionTest, StandardPairingProtocol) {
    /* Canonical pre-before-post pairing at 10ms interval */
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    std::vector<float> weight_history;
    weight_history.push_back(synapse->weight);

    /* 20 pairings at 10ms delta */
    for (int i = 0; i < 20; i++) {
        float t = i * 100.0f;  /* 10 Hz */
        triplet_stdp_pre_spike(synapse, t);
        triplet_stdp_post_spike(synapse, t + 10.0f);
        triplet_stdp_update_traces(synapse, 90.0f);
        weight_history.push_back(synapse->weight);
    }

    /* Weight should monotonically increase */
    for (size_t i = 1; i < weight_history.size(); i++) {
        EXPECT_GE(weight_history[i], weight_history[i-1]);
    }

    /* Final weight should be predictable (regression baseline) */
    EXPECT_GT(synapse->weight, 0.5f);
    EXPECT_LT(synapse->weight, 1.0f);
}

TEST_F(TripletSTDPRegressionTest, FrequencyDependenceCurve) {
    /* Test frequency dependence matches expected behavior */
    struct FreqResult {
        float freq_hz;
        float final_weight;
    };

    std::vector<FreqResult> results;

    float frequencies[] = {5.0f, 10.0f, 20.0f, 40.0f, 80.0f};

    for (float freq : frequencies) {
        synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
        ASSERT_NE(synapse, nullptr);

        float isi = 1000.0f / freq;  /* Inter-spike interval in ms */

        for (int i = 0; i < 10; i++) {
            float t = i * isi;
            triplet_stdp_pre_spike(synapse, t);
            triplet_stdp_post_spike(synapse, t + 5.0f);
            if (i < 9) {
                triplet_stdp_update_traces(synapse, isi - 5.0f);
            }
        }

        results.push_back({freq, synapse->weight});
        triplet_stdp_synapse_destroy(synapse);
        synapse = nullptr;
    }

    /* Higher frequencies should produce stronger LTP (triplet accumulation) */
    for (size_t i = 1; i < results.size(); i++) {
        if (results[i].freq_hz > 20.0f && results[i-1].freq_hz > 20.0f) {
            EXPECT_GE(results[i].final_weight, results[i-1].final_weight * 0.95f);
        }
    }
}

TEST_F(TripletSTDPRegressionTest, TraceDecayTimeConstants) {
    /* Verify exponential decay matches expected time constants */
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    /* Set all traces to 1.0 */
    synapse->r1_pre = 1.0f;
    synapse->r2_pre = 1.0f;
    synapse->o1_post = 1.0f;
    synapse->o2_post = 1.0f;

    /* After 1 time constant, should decay to ~1/e */
    triplet_stdp_update_traces(synapse, synapse->tau_plus);

    EXPECT_NEAR(synapse->r1_pre, 1.0f / M_E, 0.01f);

    /* Slow trace should decay less after same time */
    EXPECT_GT(synapse->r2_pre, synapse->r1_pre);
}

TEST_F(TripletSTDPRegressionTest, WeightBounds) {
    /* Ensure weights always stay within [w_min, w_max] */
    triplet_stdp_config_t config = triplet_stdp_config_default();
    config.w_min = 0.0f;
    config.w_max = 1.0f;
    config.A2_plus = 0.1f;  /* Large amplitude */
    config.A2_minus = 0.1f;

    synapse = triplet_stdp_synapse_create(&config, 0.5f);
    ASSERT_NE(synapse, nullptr);

    /* Try to drive weight above max */
    for (int i = 0; i < 100; i++) {
        triplet_stdp_pre_spike(synapse, i * 10.0f);
        triplet_stdp_post_spike(synapse, i * 10.0f + 2.0f);
    }

    EXPECT_LE(synapse->weight, config.w_max);
    EXPECT_GE(synapse->weight, config.w_min);

    /* Try to drive weight below min */
    for (int i = 0; i < 100; i++) {
        triplet_stdp_post_spike(synapse, i * 10.0f);
        triplet_stdp_pre_spike(synapse, i * 10.0f + 2.0f);
    }

    EXPECT_LE(synapse->weight, config.w_max);
    EXPECT_GE(synapse->weight, config.w_min);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
