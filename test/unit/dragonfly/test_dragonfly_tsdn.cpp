/**
 * @file test_dragonfly_tsdn.cpp
 * @brief Unit tests for TSDN population vector encoding
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_tsdn.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class TSDNTest : public ::testing::Test {
protected:
    tsdn_population_t* pop = nullptr;

    void SetUp() override {
        pop = tsdn_create(nullptr);  // Use defaults
        ASSERT_NE(pop, nullptr);
    }

    void TearDown() override {
        if (pop) {
            tsdn_destroy(pop);
            pop = nullptr;
        }
    }

    // Helper to check if angle is close to expected (handling wraparound)
    bool angle_close(float actual, float expected, float tolerance = 0.1f) {
        float diff = actual - expected;
        while (diff > M_PI) diff -= 2.0f * M_PI;
        while (diff < -M_PI) diff += 2.0f * M_PI;
        return std::abs(diff) < tolerance;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TSDNTest, CreateWithDefaults) {
    // Already created in SetUp, just verify it's valid
    EXPECT_NE(pop, nullptr);
}

TEST_F(TSDNTest, CreateWithCustomConfig) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    config.tuning_type = TSDN_TUNING_GAUSSIAN;
    config.tuning_width = 0.8f;

    tsdn_population_t* custom_pop = tsdn_create(&config);
    ASSERT_NE(custom_pop, nullptr);
    tsdn_destroy(custom_pop);
}

TEST_F(TSDNTest, CreateWith3DMode) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    config.mode = TSDN_MODE_3D;
    config.elevation_neurons = 8;

    tsdn_population_t* pop_3d = tsdn_create(&config);
    ASSERT_NE(pop_3d, nullptr);
    tsdn_destroy(pop_3d);
}

TEST_F(TSDNTest, CreateWithInvalidConfig) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    config.tuning_width = -1.0f;  // Invalid

    tsdn_population_t* invalid_pop = tsdn_create(&config);
    EXPECT_EQ(invalid_pop, nullptr);
}

TEST_F(TSDNTest, Reset) {
    // Encode a direction to set some state
    tsdn_encode(pop, 1.0f, 0.0f);

    // Reset
    int result = tsdn_reset(pop);
    EXPECT_EQ(result, 0);

    // After reset, all firing rates should be zero
    tsdn_state_t state;
    tsdn_get_state(pop, &state);
    for (int i = 0; i < TSDN_NEURON_COUNT; i++) {
        EXPECT_FLOAT_EQ(state.firing_rate[i], 0.0f);
    }
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(TSDNTest, EncodeForwardDirection) {
    // Target directly ahead (positive X)
    tsdn_vector_t result = tsdn_encode(pop, 1.0f, 0.0f);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, 0.0f));
    EXPECT_GT(result.magnitude, 0.0f);
}

TEST_F(TSDNTest, EncodeRightDirection) {
    // Target to the right (positive Y)
    tsdn_vector_t result = tsdn_encode(pop, 0.0f, 1.0f);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, M_PI / 2.0f));
    EXPECT_GT(result.magnitude, 0.0f);
}

TEST_F(TSDNTest, EncodeBackwardDirection) {
    // Target behind (negative X)
    tsdn_vector_t result = tsdn_encode(pop, -1.0f, 0.0f);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, M_PI) ||
                angle_close(result.direction, -M_PI));
    EXPECT_GT(result.magnitude, 0.0f);
}

TEST_F(TSDNTest, EncodeLeftDirection) {
    // Target to the left (negative Y)
    tsdn_vector_t result = tsdn_encode(pop, 0.0f, -1.0f);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, -M_PI / 2.0f));
    EXPECT_GT(result.magnitude, 0.0f);
}

TEST_F(TSDNTest, EncodeDiagonalDirection) {
    // Target at 45 degrees
    tsdn_vector_t result = tsdn_encode(pop, 1.0f, 1.0f);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, M_PI / 4.0f));
    EXPECT_GT(result.magnitude, 0.0f);
}

TEST_F(TSDNTest, EncodeMultipleDirections) {
    // Test encoding accuracy across 360 degrees
    const int num_angles = 16;
    for (int i = 0; i < num_angles; i++) {
        float angle = (float)i * 2.0f * M_PI / (float)num_angles - M_PI;
        float x = std::cos(angle);
        float y = std::sin(angle);

        tsdn_vector_t result = tsdn_encode(pop, x, y);

        EXPECT_TRUE(result.valid) << "Failed at angle " << angle;
        EXPECT_TRUE(angle_close(result.direction, angle, 0.2f))
            << "Direction mismatch at " << angle
            << ": got " << result.direction;
    }
}

TEST_F(TSDNTest, EncodeDirection) {
    // Test direct direction encoding
    float test_direction = M_PI / 3.0f;  // 60 degrees
    tsdn_vector_t result = tsdn_encode_direction(pop, test_direction);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, test_direction));
}

//=============================================================================
// 3D Encoding Tests
//=============================================================================

TEST_F(TSDNTest, Encode3DForward) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    config.mode = TSDN_MODE_3D;
    config.elevation_neurons = 8;

    tsdn_population_t* pop_3d = tsdn_create(&config);
    ASSERT_NE(pop_3d, nullptr);

    // Target forward and level
    tsdn_vector_t result = tsdn_encode_3d(pop_3d, 1.0f, 0.0f, 0.0f);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, 0.0f));
    EXPECT_TRUE(angle_close(result.elevation, 0.0f, 0.3f));

    tsdn_destroy(pop_3d);
}

TEST_F(TSDNTest, Encode3DUp) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    config.mode = TSDN_MODE_3D;
    config.elevation_neurons = 8;

    tsdn_population_t* pop_3d = tsdn_create(&config);
    ASSERT_NE(pop_3d, nullptr);

    // Target forward and up
    tsdn_vector_t result = tsdn_encode_3d(pop_3d, 1.0f, 0.0f, 1.0f);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, 0.0f));
    EXPECT_GT(result.elevation, 0.0f);  // Should be positive elevation

    tsdn_destroy(pop_3d);
}

//=============================================================================
// Decoding Tests
//=============================================================================

TEST_F(TSDNTest, DecodeAfterEncode) {
    // Encode a direction
    float test_direction = M_PI / 6.0f;
    tsdn_encode_direction(pop, test_direction);

    // Decode should give same direction
    tsdn_vector_t result = tsdn_decode(pop);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, test_direction));
}

TEST_F(TSDNTest, DecodeExternal) {
    // Set up external firing rates - peak at neuron 4 (90 degrees)
    float rates[TSDN_NEURON_COUNT] = {0};
    rates[4] = 1.0f;
    rates[3] = 0.5f;
    rates[5] = 0.5f;

    tsdn_vector_t result = tsdn_decode_external(pop, rates);

    EXPECT_TRUE(result.valid);
    // Neuron 4 is at 4 * 22.5 = 90 degrees = PI/2
    EXPECT_TRUE(angle_close(result.direction, M_PI / 2.0f, 0.3f));
}

//=============================================================================
// State Access Tests
//=============================================================================

TEST_F(TSDNTest, GetState) {
    tsdn_encode(pop, 1.0f, 0.0f);

    tsdn_state_t state;
    int result = tsdn_get_state(pop, &state);

    EXPECT_EQ(result, 0);
    // Should have some non-zero firing rates
    bool has_activity = false;
    for (int i = 0; i < TSDN_NEURON_COUNT; i++) {
        if (state.firing_rate[i] > 0.01f) {
            has_activity = true;
            break;
        }
    }
    EXPECT_TRUE(has_activity);
}

TEST_F(TSDNTest, GetPreferredDirections) {
    // Check that preferred directions are evenly spaced
    float expected_spacing = 2.0f * M_PI / TSDN_NEURON_COUNT;

    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        float direction;
        int result = tsdn_get_preferred_direction(pop, i, &direction);
        EXPECT_EQ(result, 0);
        EXPECT_NEAR(direction, (float)i * expected_spacing, 0.01f);
    }
}

TEST_F(TSDNTest, GetFiringRate) {
    tsdn_encode(pop, 1.0f, 0.0f);

    float rate;
    int result = tsdn_get_firing_rate(pop, 0, &rate);

    EXPECT_EQ(result, 0);
    EXPECT_GE(rate, 0.0f);
    EXPECT_LE(rate, 1.0f);
}

TEST_F(TSDNTest, SetFiringRates) {
    float rates[TSDN_NEURON_COUNT];
    for (int i = 0; i < TSDN_NEURON_COUNT; i++) {
        rates[i] = (float)i / (float)TSDN_NEURON_COUNT;
    }

    int result = tsdn_set_firing_rates(pop, rates);
    EXPECT_EQ(result, 0);

    // Verify rates were set
    for (int i = 0; i < TSDN_NEURON_COUNT; i++) {
        float rate;
        tsdn_get_firing_rate(pop, i, &rate);
        EXPECT_NEAR(rate, rates[i], 0.01f);
    }
}

//=============================================================================
// Gain Control Tests
//=============================================================================

TEST_F(TSDNTest, SetGain) {
    int result = tsdn_set_gain(pop, 2.0f);
    EXPECT_EQ(result, 0);

    float gain;
    tsdn_get_gain(pop, &gain);
    EXPECT_FLOAT_EQ(gain, 2.0f);
}

TEST_F(TSDNTest, GainAffectsOutput) {
    // Encode with default gain
    tsdn_vector_t result1 = tsdn_encode(pop, 1.0f, 0.0f);
    float mag1 = result1.magnitude;

    // Reset and increase gain
    tsdn_reset(pop);
    tsdn_set_gain(pop, 2.0f);

    // Encode again
    tsdn_vector_t result2 = tsdn_encode(pop, 1.0f, 0.0f);
    float mag2 = result2.magnitude;

    // Higher gain should give higher magnitude (up to saturation)
    EXPECT_GE(mag2, mag1);
}

TEST_F(TSDNTest, ApplyFacilitation) {
    float predicted_dir = 0.0f;
    float strength = 0.5f;
    float width = M_PI / 4.0f;

    int result = tsdn_apply_facilitation(pop, predicted_dir, strength, width);
    EXPECT_EQ(result, 0);

    // Clear facilitation
    result = tsdn_clear_facilitation(pop);
    EXPECT_EQ(result, 0);
}

TEST_F(TSDNTest, FacilitationBoostsTargetDirection) {
    // Encode without facilitation
    tsdn_vector_t result1 = tsdn_encode(pop, 1.0f, 0.0f);
    float mag1 = result1.magnitude;

    // Reset and apply facilitation in target direction
    tsdn_reset(pop);
    tsdn_apply_facilitation(pop, 0.0f, 0.5f, M_PI / 4.0f);

    // Encode with facilitation
    tsdn_vector_t result2 = tsdn_encode(pop, 1.0f, 0.0f);
    float mag2 = result2.magnitude;

    // Facilitation should boost magnitude
    EXPECT_GT(mag2, mag1);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(TSDNTest, UpdateWithDt) {
    // Encode to set some activity
    tsdn_encode(pop, 1.0f, 0.0f);

    // Update with time step
    int result = tsdn_update(pop, 0.1f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(TSDNTest, GetStats) {
    tsdn_stats_t stats;
    int result = tsdn_get_stats(pop, &stats);
    EXPECT_EQ(result, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.encode_calls, 0u);
}

TEST_F(TSDNTest, StatsTrackEncodeCalls) {
    // Encode several times
    for (int i = 0; i < 10; i++) {
        tsdn_encode(pop, 1.0f, 0.0f);
    }

    tsdn_stats_t stats;
    tsdn_get_stats(pop, &stats);
    EXPECT_EQ(stats.encode_calls, 10u);
}

TEST_F(TSDNTest, ResetStats) {
    tsdn_encode(pop, 1.0f, 0.0f);

    tsdn_reset_stats(pop);

    tsdn_stats_t stats;
    tsdn_get_stats(pop, &stats);
    EXPECT_EQ(stats.encode_calls, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(TSDNTest, NormalizeAngle) {
    EXPECT_NEAR(tsdn_normalize_angle(0.0f), 0.0f, 0.01f);
    // Note: π and -π are equivalent angles, normalize returns (-π, π]
    // Due to floating point precision, exact π may normalize to -π
    float pi_result = tsdn_normalize_angle(static_cast<float>(M_PI));
    EXPECT_TRUE(std::fabs(pi_result) > M_PI - 0.01f);  // Should be near ±π

    float neg_pi_result = tsdn_normalize_angle(static_cast<float>(-M_PI));
    EXPECT_TRUE(std::fabs(neg_pi_result) > M_PI - 0.01f);  // Should be near ±π

    // 3π should wrap to near ±π
    float three_pi = tsdn_normalize_angle(3.0f * static_cast<float>(M_PI));
    EXPECT_TRUE(std::fabs(three_pi) > M_PI - 0.01f);

    float neg_three_pi = tsdn_normalize_angle(-3.0f * static_cast<float>(M_PI));
    EXPECT_TRUE(std::fabs(neg_three_pi) > M_PI - 0.01f);

    // Values clearly not at boundary
    EXPECT_NEAR(tsdn_normalize_angle(M_PI / 2.0f), M_PI / 2.0f, 0.01f);
    EXPECT_NEAR(tsdn_normalize_angle(-M_PI / 2.0f), -M_PI / 2.0f, 0.01f);
    EXPECT_NEAR(tsdn_normalize_angle(5.0f * M_PI / 4.0f), -3.0f * M_PI / 4.0f, 0.01f);
}

TEST_F(TSDNTest, AngularDiff) {
    EXPECT_NEAR(tsdn_angular_diff(0.0f, 0.0f), 0.0f, 0.01f);
    EXPECT_NEAR(tsdn_angular_diff(M_PI / 2.0f, 0.0f), M_PI / 2.0f, 0.01f);
    EXPECT_NEAR(tsdn_angular_diff(0.0f, M_PI / 2.0f), -M_PI / 2.0f, 0.01f);

    // Wrap-around case
    EXPECT_NEAR(tsdn_angular_diff(M_PI - 0.1f, -M_PI + 0.1f), -0.2f, 0.01f);
}

TEST_F(TSDNTest, TuningResponse) {
    // At preferred direction, response should be 1
    float response = tsdn_tuning_response(0.0f, TSDN_TUNING_COSINE, 1.0f, 2.0f);
    EXPECT_FLOAT_EQ(response, 1.0f);

    // At 90 degrees, cosine response should be 0
    response = tsdn_tuning_response(M_PI / 2.0f, TSDN_TUNING_COSINE, 1.0f, 2.0f);
    EXPECT_FLOAT_EQ(response, 0.0f);

    // Gaussian should be positive at 90 degrees
    response = tsdn_tuning_response(M_PI / 2.0f, TSDN_TUNING_GAUSSIAN, 1.0f, 2.0f);
    EXPECT_GT(response, 0.0f);
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(TSDNTest, NullPointerHandling) {
    EXPECT_EQ(tsdn_reset(nullptr), -1);
    EXPECT_EQ(tsdn_set_gain(nullptr, 1.0f), -1);
    EXPECT_EQ(tsdn_update(nullptr, 0.1f), -1);

    tsdn_vector_t result = tsdn_encode(nullptr, 1.0f, 0.0f);
    EXPECT_FALSE(result.valid);

    result = tsdn_decode(nullptr);
    EXPECT_FALSE(result.valid);
}

//=============================================================================
// Tuning Type Tests
//=============================================================================

TEST_F(TSDNTest, GaussianTuning) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    config.tuning_type = TSDN_TUNING_GAUSSIAN;

    tsdn_population_t* gaussian_pop = tsdn_create(&config);
    ASSERT_NE(gaussian_pop, nullptr);

    tsdn_vector_t result = tsdn_encode(gaussian_pop, 1.0f, 0.0f);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, 0.0f));

    tsdn_destroy(gaussian_pop);
}

TEST_F(TSDNTest, VonMisesTuning) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    config.tuning_type = TSDN_TUNING_VON_MISES;

    tsdn_population_t* vm_pop = tsdn_create(&config);
    ASSERT_NE(vm_pop, nullptr);

    tsdn_vector_t result = tsdn_encode(vm_pop, 1.0f, 0.0f);
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(angle_close(result.direction, 0.0f));

    tsdn_destroy(vm_pop);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
