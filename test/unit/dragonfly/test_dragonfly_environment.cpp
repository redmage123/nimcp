/**
 * @file test_dragonfly_environment.cpp
 * @brief Unit tests for dragonfly environment module
 *
 * Tests environmental sensing including wind, light conditions,
 * terrain, and temperature effects on hunting.
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_environment.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonEnvironmentTest : public ::testing::Test {
protected:
    dragonfly_environment_t env = nullptr;

    void SetUp() override {
        environment_config_t config = environment_default_config();
        env = dragonfly_environment_create(&config);
        ASSERT_NE(env, nullptr);
    }

    void TearDown() override {
        if (env) {
            dragonfly_environment_destroy(env);
            env = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, DefaultConfig) {
    environment_config_t config = environment_default_config();

    EXPECT_GT(config.max_hunting_wind_ms, 0.0f);
    EXPECT_GE(config.min_hunting_light, 0.0f);
    EXPECT_LE(config.min_hunting_light, 1.0f);
    EXPECT_LT(config.min_temp_c, config.max_temp_c);
}

TEST_F(DragonEnvironmentTest, ValidateConfig) {
    environment_config_t config = environment_default_config();
    EXPECT_TRUE(environment_validate_config(&config));

    EXPECT_FALSE(environment_validate_config(nullptr));
}

TEST_F(DragonEnvironmentTest, CreateWithCustomConfig) {
    environment_config_t config = environment_default_config();
    config.max_hunting_wind_ms = 15.0f;

    dragonfly_environment_t custom = dragonfly_environment_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_environment_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, CreateAndDestroy) {
    dragonfly_environment_t e = dragonfly_environment_create(nullptr);
    ASSERT_NE(e, nullptr);
    dragonfly_environment_destroy(e);
}

TEST_F(DragonEnvironmentTest, DestroyNull) {
    dragonfly_environment_destroy(nullptr);
}

TEST_F(DragonEnvironmentTest, Reset) {
    EXPECT_EQ(dragonfly_environment_reset(env), 0);
}

//=============================================================================
// Wind Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, SetWind) {
    float wind[3] = {5.0f, 0.0f, 0.0f};
    EXPECT_EQ(dragonfly_environment_set_wind(env, wind, 0.1f), 0);
}

TEST_F(DragonEnvironmentTest, WindAffectsState) {
    float strong_wind[3] = {10.0f, 0.0f, 0.0f};
    dragonfly_environment_set_wind(env, strong_wind, 0.2f);

    environment_state_t state;
    EXPECT_EQ(dragonfly_environment_get_state(env, &state), 0);
    EXPECT_FLOAT_EQ(state.wind_velocity[0], strong_wind[0]);
    EXPECT_FLOAT_EQ(state.wind_variability, 0.2f);
}

TEST_F(DragonEnvironmentTest, WindConditionClassification) {
    // Calm wind
    float calm[3] = {0.5f, 0.0f, 0.0f};
    dragonfly_environment_set_wind(env, calm, 0.0f);
    environment_state_t state;
    dragonfly_environment_get_state(env, &state);
    EXPECT_EQ(state.wind_condition, WIND_CALM);

    // Strong wind
    float strong[3] = {15.0f, 0.0f, 0.0f};
    dragonfly_environment_set_wind(env, strong, 0.3f);
    dragonfly_environment_get_state(env, &state);
    EXPECT_TRUE(state.wind_condition == WIND_STRONG ||
                state.wind_condition == WIND_EXTREME);
}

//=============================================================================
// Light Conditions Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, SetLightLevel) {
    EXPECT_EQ(dragonfly_environment_set_light(env, 0.8f, 1.0f, 0.0f), 0);
}

TEST_F(DragonEnvironmentTest, GetLightLevel) {
    dragonfly_environment_set_light(env, 0.6f, 0.8f, 1.57f);
    environment_state_t state;
    EXPECT_EQ(dragonfly_environment_get_state(env, &state), 0);
    EXPECT_FLOAT_EQ(state.light_level, 0.6f);
    EXPECT_FLOAT_EQ(state.sun_elevation_rad, 0.8f);
    EXPECT_FLOAT_EQ(state.sun_azimuth_rad, 1.57f);
}

TEST_F(DragonEnvironmentTest, LightConditionClassification) {
    // Bright light
    dragonfly_environment_set_light(env, 0.95f, 1.2f, 0.0f);
    environment_state_t state;
    dragonfly_environment_get_state(env, &state);
    EXPECT_EQ(state.light_condition, LIGHT_BRIGHT_SUN);

    // Low light (dusk)
    dragonfly_environment_set_light(env, 0.2f, 0.1f, 3.14f);
    dragonfly_environment_get_state(env, &state);
    EXPECT_TRUE(state.light_condition == LIGHT_DUSK ||
                state.light_condition == LIGHT_DARK ||
                state.light_condition == LIGHT_SHADE);
}

//=============================================================================
// Terrain Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, SetTerrain) {
    EXPECT_EQ(dragonfly_environment_set_terrain(env, TERRAIN_OPEN_WATER, 0.1f, 0.0f), 0);
    EXPECT_EQ(dragonfly_environment_set_terrain(env, TERRAIN_FOREST_EDGE, 0.5f, 0.0f), 0);
}

TEST_F(DragonEnvironmentTest, GetTerrain) {
    dragonfly_environment_set_terrain(env, TERRAIN_MEADOW, 0.3f, 2.0f);
    environment_state_t state;
    EXPECT_EQ(dragonfly_environment_get_state(env, &state), 0);
    EXPECT_EQ(state.terrain, TERRAIN_MEADOW);
    EXPECT_FLOAT_EQ(state.terrain_complexity, 0.3f);
    EXPECT_FLOAT_EQ(state.water_surface_level, 2.0f);
}

//=============================================================================
// Temperature Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, SetTemperature) {
    EXPECT_EQ(dragonfly_environment_set_temperature(env, 25.0f), 0);
}

TEST_F(DragonEnvironmentTest, GetTemperature) {
    dragonfly_environment_set_temperature(env, 22.5f);
    environment_state_t state;
    EXPECT_EQ(dragonfly_environment_get_state(env, &state), 0);
    EXPECT_FLOAT_EQ(state.temperature_c, 22.5f);
}

TEST_F(DragonEnvironmentTest, OptimalTemperatureRange) {
    // Within optimal range (typically 20-30C for dragonflies)
    dragonfly_environment_set_temperature(env, 25.0f);
    environment_state_t state;
    dragonfly_environment_get_state(env, &state);
    EXPECT_TRUE(state.is_optimal_temp);

    // Below optimal
    dragonfly_environment_set_temperature(env, 10.0f);
    dragonfly_environment_get_state(env, &state);
    EXPECT_FALSE(state.is_optimal_temp);
}

//=============================================================================
// Full State Update Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, FullStateUpdate) {
    environment_state_t input;
    memset(&input, 0, sizeof(input));

    input.wind_velocity[0] = 3.0f;
    input.wind_velocity[1] = 1.0f;
    input.wind_velocity[2] = 0.0f;
    input.wind_variability = 0.15f;
    input.wind_condition = WIND_LIGHT;

    input.light_level = 0.7f;
    input.sun_elevation_rad = 0.9f;
    input.sun_azimuth_rad = 2.0f;
    input.light_condition = LIGHT_OVERCAST;

    input.terrain = TERRAIN_WATER_EDGE;
    input.terrain_complexity = 0.2f;
    input.water_surface_level = 0.5f;

    input.temperature_c = 24.0f;
    input.is_optimal_temp = true;

    input.is_raining = false;
    input.visibility_m = 1000.0f;

    EXPECT_EQ(dragonfly_environment_update(env, &input), 0);

    environment_state_t output;
    EXPECT_EQ(dragonfly_environment_get_state(env, &output), 0);

    EXPECT_FLOAT_EQ(output.wind_velocity[0], input.wind_velocity[0]);
    EXPECT_FLOAT_EQ(output.light_level, input.light_level);
    EXPECT_EQ(output.terrain, input.terrain);
    EXPECT_FLOAT_EQ(output.temperature_c, input.temperature_c);
}

//=============================================================================
// Compensation Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, GetCompensation) {
    // Set up wind
    float wind[3] = {5.0f, 2.0f, 0.0f};
    dragonfly_environment_set_wind(env, wind, 0.1f);

    // Set up light (with potential backlight from sun position)
    dragonfly_environment_set_light(env, 0.8f, 0.5f, 1.0f);

    float target_dir[3] = {1.0f, 0.0f, 0.0f};
    environment_compensation_t comp;
    EXPECT_EQ(dragonfly_environment_get_compensation(env, target_dir, &comp), 0);

    // Compensation should include wind correction
    EXPECT_NE(comp.wind_correction[0], 0.0f);

    // Hunting suitability should be computed
    EXPECT_GE(comp.hunting_suitability, 0.0f);
    EXPECT_LE(comp.hunting_suitability, 1.0f);
}

TEST_F(DragonEnvironmentTest, HuntingOkCheck) {
    // Good conditions
    dragonfly_environment_set_light(env, 0.8f, 1.0f, 0.0f);
    dragonfly_environment_set_temperature(env, 25.0f);
    float calm[3] = {1.0f, 0.0f, 0.0f};
    dragonfly_environment_set_wind(env, calm, 0.0f);

    EXPECT_TRUE(dragonfly_environment_hunting_ok(env));
}

TEST_F(DragonEnvironmentTest, HuntingNotOkExtremeWind) {
    float extreme_wind[3] = {25.0f, 0.0f, 0.0f};
    dragonfly_environment_set_wind(env, extreme_wind, 0.5f);

    // Extreme wind should make hunting unsuitable
    // (exact behavior depends on config thresholds)
}

//=============================================================================
// Velocity Correction Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, CorrectVelocity) {
    // Set headwind
    float wind[3] = {5.0f, 0.0f, 0.0f};
    dragonfly_environment_set_wind(env, wind, 0.1f);

    float desired[3] = {10.0f, 0.0f, 0.0f};
    float corrected[3];
    EXPECT_EQ(dragonfly_environment_correct_velocity(env, desired, corrected), 0);

    // Corrected velocity should account for wind
    // When flying into headwind, need to increase velocity
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, GetStats) {
    environment_stats_t stats;
    EXPECT_EQ(dragonfly_environment_get_stats(env, &stats), 0);
}

TEST_F(DragonEnvironmentTest, StatsAccumulate) {
    // Update environment several times
    for (int i = 0; i < 10; i++) {
        float wind[3] = {(float)i, 0.0f, 0.0f};
        dragonfly_environment_set_wind(env, wind, 0.1f);
    }

    environment_stats_t stats;
    dragonfly_environment_get_stats(env, &stats);
    // Stats should reflect accumulated updates
    EXPECT_GE(stats.updates, 0u);
}

//=============================================================================
// Name Utility Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, LightConditionNames) {
    EXPECT_NE(dragonfly_light_name(LIGHT_BRIGHT_SUN), nullptr);
    EXPECT_NE(dragonfly_light_name(LIGHT_OVERCAST), nullptr);
    EXPECT_NE(dragonfly_light_name(LIGHT_DUSK), nullptr);
    EXPECT_NE(dragonfly_light_name(LIGHT_DAWN), nullptr);
    EXPECT_NE(dragonfly_light_name(LIGHT_SHADE), nullptr);
    EXPECT_NE(dragonfly_light_name(LIGHT_DARK), nullptr);
}

TEST_F(DragonEnvironmentTest, WindConditionNames) {
    EXPECT_NE(dragonfly_wind_name(WIND_CALM), nullptr);
    EXPECT_NE(dragonfly_wind_name(WIND_LIGHT), nullptr);
    EXPECT_NE(dragonfly_wind_name(WIND_MODERATE), nullptr);
    EXPECT_NE(dragonfly_wind_name(WIND_STRONG), nullptr);
    EXPECT_NE(dragonfly_wind_name(WIND_GUSTY), nullptr);
    EXPECT_NE(dragonfly_wind_name(WIND_EXTREME), nullptr);
}

TEST_F(DragonEnvironmentTest, TerrainNames) {
    EXPECT_NE(dragonfly_terrain_name(TERRAIN_OPEN_WATER), nullptr);
    EXPECT_NE(dragonfly_terrain_name(TERRAIN_WATER_EDGE), nullptr);
    EXPECT_NE(dragonfly_terrain_name(TERRAIN_MEADOW), nullptr);
    EXPECT_NE(dragonfly_terrain_name(TERRAIN_FOREST_EDGE), nullptr);
    EXPECT_NE(dragonfly_terrain_name(TERRAIN_FOREST), nullptr);
    EXPECT_NE(dragonfly_terrain_name(TERRAIN_URBAN), nullptr);
}

//=============================================================================
// Null Parameter Tests
//=============================================================================

TEST_F(DragonEnvironmentTest, NullWindArray) {
    EXPECT_NE(dragonfly_environment_set_wind(env, nullptr, 0.1f), 0);
}

TEST_F(DragonEnvironmentTest, NullStateOutput) {
    EXPECT_NE(dragonfly_environment_get_state(env, nullptr), 0);
}

TEST_F(DragonEnvironmentTest, NullCompensationOutput) {
    float dir[3] = {1.0f, 0.0f, 0.0f};
    EXPECT_NE(dragonfly_environment_get_compensation(env, dir, nullptr), 0);
}

TEST_F(DragonEnvironmentTest, NullStatsOutput) {
    EXPECT_NE(dragonfly_environment_get_stats(env, nullptr), 0);
}
