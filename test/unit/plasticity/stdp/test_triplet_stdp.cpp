/**
 * @file test_triplet_stdp.cpp
 * @brief Unit tests for Triplet STDP module
 * @version 1.0.0
 * @date 2025-12-19
 *
 * Comprehensive tests for Pfister & Gerstner (2006) triplet STDP implementation
 */

#include <gtest/gtest.h>
#include <cmath>
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/stdp/nimcp_triplet_stdp_sleep_bridge.h"
#include "plasticity/stdp/nimcp_triplet_stdp_immune_bridge.h"

class TripletSTDPTest : public ::testing::Test {
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

/* ============================================================================
 * Configuration and Initialization Tests
 * ============================================================================ */

TEST_F(TripletSTDPTest, DefaultConfiguration) {
    triplet_stdp_config_t config = triplet_stdp_config_default();

    EXPECT_FLOAT_EQ(config.A2_plus, TRIPLET_STDP_DEFAULT_A2_PLUS);
    EXPECT_FLOAT_EQ(config.A3_plus, TRIPLET_STDP_DEFAULT_A3_PLUS);
    EXPECT_FLOAT_EQ(config.A2_minus, TRIPLET_STDP_DEFAULT_A2_MINUS);
    EXPECT_FLOAT_EQ(config.A3_minus, TRIPLET_STDP_DEFAULT_A3_MINUS);
    EXPECT_FLOAT_EQ(config.tau_plus, TRIPLET_STDP_DEFAULT_TAU_PLUS);
    EXPECT_FLOAT_EQ(config.tau_minus, TRIPLET_STDP_DEFAULT_TAU_MINUS);
    EXPECT_FLOAT_EQ(config.tau_x, TRIPLET_STDP_DEFAULT_TAU_X);
    EXPECT_FLOAT_EQ(config.tau_y, TRIPLET_STDP_DEFAULT_TAU_Y);
}

TEST_F(TripletSTDPTest, HippocampalConfiguration) {
    triplet_stdp_config_t config = triplet_stdp_config_hippocampal();

    /* Hippocampal params should differ from cortical */
    EXPECT_LT(config.tau_plus, TRIPLET_STDP_DEFAULT_TAU_PLUS);
    EXPECT_LT(config.tau_x, TRIPLET_STDP_DEFAULT_TAU_X);
    EXPECT_GT(config.A3_plus, TRIPLET_STDP_DEFAULT_A3_PLUS);
}

TEST_F(TripletSTDPTest, SynapseCreation) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    EXPECT_FLOAT_EQ(synapse->weight, 0.5f);
    EXPECT_FLOAT_EQ(synapse->r1_pre, 0.0f);
    EXPECT_FLOAT_EQ(synapse->r2_pre, 0.0f);
    EXPECT_FLOAT_EQ(synapse->o1_post, 0.0f);
    EXPECT_FLOAT_EQ(synapse->o2_post, 0.0f);
}

TEST_F(TripletSTDPTest, SynapseCreationWithConfig) {
    triplet_stdp_config_t config = triplet_stdp_config_default();
    config.A2_plus = 0.01f;
    config.w_max = 2.0f;

    synapse = triplet_stdp_synapse_create(&config, 1.0f);
    ASSERT_NE(synapse, nullptr);

    EXPECT_FLOAT_EQ(synapse->A2_plus, 0.01f);
    EXPECT_FLOAT_EQ(synapse->w_max, 2.0f);
    EXPECT_FLOAT_EQ(synapse->weight, 1.0f);
}

TEST_F(TripletSTDPTest, WeightClamping) {
    triplet_stdp_config_t config = triplet_stdp_config_default();
    config.w_min = 0.0f;
    config.w_max = 1.0f;

    /* Test initial weight above max */
    synapse = triplet_stdp_synapse_create(&config, 2.0f);
    ASSERT_NE(synapse, nullptr);
    EXPECT_FLOAT_EQ(synapse->weight, 1.0f);

    triplet_stdp_synapse_destroy(synapse);

    /* Test initial weight below min */
    synapse = triplet_stdp_synapse_create(&config, -0.5f);
    ASSERT_NE(synapse, nullptr);
    EXPECT_FLOAT_EQ(synapse->weight, 0.0f);
}

/* ============================================================================
 * Trace Update Tests
 * ============================================================================ */

TEST_F(TripletSTDPTest, TraceDecay) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    /* Manually set traces */
    synapse->r1_pre = 1.0f;
    synapse->r2_pre = 1.0f;
    synapse->o1_post = 1.0f;
    synapse->o2_post = 1.0f;

    /* Update with 10ms timestep */
    ASSERT_EQ(triplet_stdp_update_traces(synapse, 10.0f), 0);

    /* All traces should decay */
    EXPECT_LT(synapse->r1_pre, 1.0f);
    EXPECT_LT(synapse->r2_pre, 1.0f);
    EXPECT_LT(synapse->o1_post, 1.0f);
    EXPECT_LT(synapse->o2_post, 1.0f);

    /* Fast traces decay faster than slow traces */
    float r1_decay = 1.0f - synapse->r1_pre;
    float r2_decay = 1.0f - synapse->r2_pre;
    EXPECT_GT(r1_decay, r2_decay);
}

TEST_F(TripletSTDPTest, TraceDifferentialDecay) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    synapse->r1_pre = 1.0f;
    synapse->r2_pre = 1.0f;

    /* Fast trace (tau_plus ~ 16.8ms) decays much faster than slow trace (tau_x ~ 101ms) */
    triplet_stdp_update_traces(synapse, 50.0f);

    EXPECT_LT(synapse->r1_pre, 0.1f);  /* Fast trace nearly gone */
    EXPECT_GT(synapse->r2_pre, 0.5f);  /* Slow trace still significant */
}

/* ============================================================================
 * Pairwise STDP Tests
 * ============================================================================ */

TEST_F(TripletSTDPTest, PostBeforePreLTD) {
    /* WHAT: Post spike before pre spike should cause LTD (pairwise)
     * WHY:  Classic STDP: post-before-pre → depression
     * HOW:  Fire post, then pre; check weight decreased
     */
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    float initial_weight = synapse->weight;

    /* Post spike at t=0 */
    triplet_stdp_post_spike(synapse, 0.0f);

    /* Pre spike at t=10 (after post) */
    float dw = triplet_stdp_pre_spike(synapse, 10.0f);

    /* Should have LTD (negative weight change) */
    EXPECT_LT(dw, 0.0f);
    EXPECT_LT(synapse->weight, initial_weight);
    EXPECT_GT(synapse->num_ltd_pairwise_events, 0);
}

TEST_F(TripletSTDPTest, PreBeforePostLTP) {
    /* WHAT: Pre spike before post spike should cause LTP (pairwise)
     * WHY:  Classic STDP: pre-before-post → potentiation
     * HOW:  Fire pre, then post; check weight increased
     */
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    float initial_weight = synapse->weight;

    /* Pre spike at t=0 */
    triplet_stdp_pre_spike(synapse, 0.0f);

    /* Post spike at t=10 (after pre) */
    float dw = triplet_stdp_post_spike(synapse, 10.0f);

    /* Should have LTP (positive weight change) */
    EXPECT_GT(dw, 0.0f);
    EXPECT_GT(synapse->weight, initial_weight);
    EXPECT_GT(synapse->num_ltp_pairwise_events, 0);
}

/* ============================================================================
 * Triplet STDP Tests
 * ============================================================================ */

TEST_F(TripletSTDPTest, TripletLTPAccumulation) {
    /* WHAT: Post-pre-pre-post pattern should produce triplet LTP
     * WHY:  Triplet LTP term = A3+ * r2_pre * o1_post requires previous post spike
     *       to generate o1_post > 0 before LTP computation on second post spike
     * HOW:  Fire post-pre-pre-post pattern, check triplet LTP occurred
     *
     * BIOLOGICAL: Pfister & Gerstner (2006) triplet LTP depends on:
     *   r2_pre (slow pre-trace from accumulated pre-spikes)
     *   o1_post (fast post-trace from previous post-spike)
     */
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    /* First post spike at t=0 to initialize o1_post and o2_post */
    triplet_stdp_post_spike(synapse, 0.0f);

    /* Pre spike 1 at t=10 (establishes r1_pre, r2_pre) */
    triplet_stdp_pre_spike(synapse, 10.0f);

    /* Pre spike 2 at t=15 (r2_pre accumulates further) */
    triplet_stdp_pre_spike(synapse, 15.0f);

    /* Second post spike at t=20 - now r2_pre > 0 AND o1_post > 0 */
    triplet_stdp_post_spike(synapse, 20.0f);

    /* Should have both pairwise and triplet LTP from second post spike */
    EXPECT_GT(synapse->num_ltp_pairwise_events, 0);
    EXPECT_GT(synapse->num_ltp_triplet_events, 0);
    EXPECT_GT(synapse->total_ltp_triplet, 0.0f);
}

TEST_F(TripletSTDPTest, FrequencyDependence) {
    /* WHAT: High-frequency stimulation should produce more triplet plasticity
     * WHY:  Triplet terms capture frequency-dependent effects
     * HOW:  Compare low-freq vs high-freq pairing protocols
     */

    /* Low frequency (10 Hz, 100ms ISI) */
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    for (int i = 0; i < 5; i++) {
        float t = i * 100.0f;
        triplet_stdp_pre_spike(synapse, t);
        triplet_stdp_post_spike(synapse, t + 5.0f);
        triplet_stdp_update_traces(synapse, 95.0f);
    }

    float low_freq_triplet_ltp = synapse->total_ltp_triplet;
    triplet_stdp_synapse_destroy(synapse);

    /* High frequency (50 Hz, 20ms ISI) */
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    for (int i = 0; i < 5; i++) {
        float t = i * 20.0f;
        triplet_stdp_pre_spike(synapse, t);
        triplet_stdp_post_spike(synapse, t + 5.0f);
        triplet_stdp_update_traces(synapse, 15.0f);
    }

    float high_freq_triplet_ltp = synapse->total_ltp_triplet;

    /* High frequency should produce MORE triplet LTP due to trace accumulation */
    EXPECT_GT(high_freq_triplet_ltp, low_freq_triplet_ltp);
}

/* ============================================================================
 * Statistics and Query Tests
 * ============================================================================ */

TEST_F(TripletSTDPTest, QueryFunctions) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    EXPECT_FLOAT_EQ(triplet_stdp_get_weight(synapse), 0.5f);
    EXPECT_FLOAT_EQ(triplet_stdp_get_r1_pre(synapse), 0.0f);
    EXPECT_FLOAT_EQ(triplet_stdp_get_r2_pre(synapse), 0.0f);
    EXPECT_FLOAT_EQ(triplet_stdp_get_o1_post(synapse), 0.0f);
    EXPECT_FLOAT_EQ(triplet_stdp_get_o2_post(synapse), 0.0f);

    /* Fire some spikes */
    triplet_stdp_pre_spike(synapse, 0.0f);
    EXPECT_GT(triplet_stdp_get_r1_pre(synapse), 0.0f);
    EXPECT_GT(triplet_stdp_get_r2_pre(synapse), 0.0f);
}

TEST_F(TripletSTDPTest, StatisticsAccumulation) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    /* Pre-before-post pattern (LTP) */
    for (int i = 0; i < 10; i++) {
        triplet_stdp_pre_spike(synapse, i * 20.0f);
        triplet_stdp_post_spike(synapse, i * 20.0f + 5.0f);
    }

    float total_ltp = triplet_stdp_get_total_ltp(synapse);
    EXPECT_GT(total_ltp, 0.0f);
    EXPECT_GT(synapse->num_ltp_pairwise_events, 0);
}

TEST_F(TripletSTDPTest, SynapseReset) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    /* Accumulate some plasticity */
    triplet_stdp_pre_spike(synapse, 0.0f);
    triplet_stdp_post_spike(synapse, 5.0f);

    EXPECT_GT(synapse->r1_pre, 0.0f);
    EXPECT_GT(synapse->total_ltp_pairwise, 0.0f);

    /* Reset */
    ASSERT_EQ(triplet_stdp_synapse_reset(synapse), 0);

    /* Traces and statistics should be zero */
    EXPECT_FLOAT_EQ(synapse->r1_pre, 0.0f);
    EXPECT_FLOAT_EQ(synapse->r2_pre, 0.0f);
    EXPECT_FLOAT_EQ(synapse->total_ltp_pairwise, 0.0f);
    EXPECT_EQ(synapse->num_ltp_pairwise_events, 0);

    /* Weight should be preserved */
    EXPECT_GT(synapse->weight, 0.5f);  /* Increased from LTP */
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(TripletSTDPTest, WeightSaturation) {
    triplet_stdp_config_t config = triplet_stdp_config_default();
    config.w_max = 1.0f;
    config.A2_plus = 0.1f;  /* Large amplitude for quick saturation */

    synapse = triplet_stdp_synapse_create(&config, 0.95f);
    ASSERT_NE(synapse, nullptr);

    /* Repeated pre-before-post should saturate weight at w_max */
    for (int i = 0; i < 20; i++) {
        triplet_stdp_pre_spike(synapse, i * 10.0f);
        triplet_stdp_post_spike(synapse, i * 10.0f + 2.0f);
    }

    EXPECT_FLOAT_EQ(synapse->weight, 1.0f);
    EXPECT_LE(synapse->weight, config.w_max);
}

TEST_F(TripletSTDPTest, NullPointerHandling) {
    EXPECT_EQ(triplet_stdp_synapse_reset(nullptr), -1);
    EXPECT_EQ(triplet_stdp_update_traces(nullptr, 1.0f), -1);
    EXPECT_FLOAT_EQ(triplet_stdp_pre_spike(nullptr, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(triplet_stdp_post_spike(nullptr, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(triplet_stdp_get_weight(nullptr), -1.0f);
}

TEST_F(TripletSTDPTest, ZeroTimestep) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    synapse->r1_pre = 1.0f;

    /* Zero timestep should not change traces */
    ASSERT_EQ(triplet_stdp_update_traces(synapse, 0.0f), 0);
    EXPECT_FLOAT_EQ(synapse->r1_pre, 1.0f);
}

/* ============================================================================
 * Sleep Integration Tests
 * ============================================================================ */

TEST_F(TripletSTDPTest, SleepStateChange) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    EXPECT_EQ(synapse->current_sleep_state, SLEEP_STATE_AWAKE);

    ASSERT_EQ(triplet_stdp_set_sleep_state(synapse, SLEEP_STATE_REM), 0);
    EXPECT_EQ(synapse->current_sleep_state, SLEEP_STATE_REM);
}

/* ============================================================================
 * Print Stats Test
 * ============================================================================ */

TEST_F(TripletSTDPTest, PrintStats) {
    synapse = triplet_stdp_synapse_create(nullptr, 0.5f);
    ASSERT_NE(synapse, nullptr);

    /* Accumulate some data */
    triplet_stdp_pre_spike(synapse, 0.0f);
    triplet_stdp_post_spike(synapse, 5.0f);

    /* Should not crash */
    triplet_stdp_print_stats(synapse);
    triplet_stdp_print_stats(nullptr);  /* Null handling */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
