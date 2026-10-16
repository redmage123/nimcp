/**
 * @file test_trigeminal_oral_bridge.cpp
 * @brief Unit tests for Trigeminal-Oral Sensory Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * Tests all components of the trigeminal-oral bridge including:
 * - Lifecycle management
 * - Chemesthesis detection (spicy, cooling, tingling)
 * - Texture analysis
 * - Temperature-taste interactions
 * - Mouthfeel computation
 * - Mastication tracking
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/regions/sensory_integration/nimcp_trigeminal_oral_bridge.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class TrigeminalOralBridgeTest : public ::testing::Test {
protected:
    trigeminal_oral_bridge_t* bridge = nullptr;
    nimcp_somatosensory_t* soma = nullptr;
    nimcp_gustatory_t* gust = nullptr;

    void SetUp() override {
        /* Create somatosensory module */
        soma_config_t soma_cfg = soma_default_config();
        soma = soma_create(&soma_cfg);

        /* Create gustatory module */
        gust_config_t gust_cfg = gust_default_config();
        gust = gust_create(&gust_cfg);

        /* Create trigeminal bridge */
        trigeminal_oral_config_t config;
        trigeminal_oral_default_config(&config);
        bridge = trigeminal_oral_bridge_create(&config);

        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            trigeminal_oral_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (soma) {
            soma_destroy(soma);
            soma = nullptr;
        }
        if (gust) {
            gust_destroy(gust);
            gust = nullptr;
        }
    }

    /* Helper to create oral input */
    oral_soma_input_t createOralInput(oral_region_t region, float pressure,
                                       float roughness, float hardness,
                                       float viscosity, float temp_c) {
        oral_soma_input_t input;
        memset(&input, 0, sizeof(input));
        input.region = region;
        input.pressure = pressure;
        input.texture_roughness = roughness;
        input.hardness = hardness;
        input.viscosity = viscosity;
        input.temperature_c = temp_c;
        input.temp_category = (temp_c < 20.0f) ? TEMP_PERCEPTION_COLD :
                              (temp_c > 45.0f) ? TEMP_PERCEPTION_HOT :
                              TEMP_PERCEPTION_NEUTRAL;
        return input;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, DefaultConfig) {
    trigeminal_oral_config_t config;
    EXPECT_EQ(trigeminal_oral_default_config(&config), 0);

    EXPECT_TRUE(config.enable_chemesthesis);
    EXPECT_TRUE(config.enable_texture);
    EXPECT_TRUE(config.enable_temp_taste);
    EXPECT_TRUE(config.enable_mouthfeel);
    EXPECT_GT(config.spice_sensitivity, 0.0f);
    EXPECT_GT(config.cold_sensitivity, 0.0f);
    EXPECT_GT(config.integration_window_ms, 0u);
}

TEST_F(TrigeminalOralBridgeTest, DefaultConfigNullPointer) {
    EXPECT_EQ(trigeminal_oral_default_config(nullptr), -1);
}

TEST_F(TrigeminalOralBridgeTest, CreateWithDefaultConfig) {
    trigeminal_oral_bridge_t* b = trigeminal_oral_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(trigeminal_oral_get_status(b), TRIGEMINAL_STATUS_IDLE);
    trigeminal_oral_bridge_destroy(b);
}

TEST_F(TrigeminalOralBridgeTest, CreateWithCustomConfig) {
    trigeminal_oral_config_t config;
    trigeminal_oral_default_config(&config);
    config.spice_sensitivity = 0.8f;
    config.spice_tolerance = 0.6f;

    trigeminal_oral_bridge_t* b = trigeminal_oral_bridge_create(&config);
    ASSERT_NE(b, nullptr);
    trigeminal_oral_bridge_destroy(b);
}

TEST_F(TrigeminalOralBridgeTest, DestroyNull) {
    trigeminal_oral_bridge_destroy(nullptr);  /* Should not crash */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, Connect) {
    EXPECT_FALSE(trigeminal_oral_is_connected(bridge));
    EXPECT_EQ(trigeminal_oral_connect(bridge, soma, gust), 0);
    EXPECT_TRUE(trigeminal_oral_is_connected(bridge));
}

TEST_F(TrigeminalOralBridgeTest, ConnectNullBridge) {
    EXPECT_EQ(trigeminal_oral_connect(nullptr, soma, gust), -1);
}

TEST_F(TrigeminalOralBridgeTest, ConnectNullModules) {
    /* Should succeed even with null modules */
    EXPECT_EQ(trigeminal_oral_connect(bridge, nullptr, nullptr), 0);
    EXPECT_TRUE(trigeminal_oral_is_connected(bridge));
}

TEST_F(TrigeminalOralBridgeTest, Disconnect) {
    trigeminal_oral_connect(bridge, soma, gust);
    EXPECT_TRUE(trigeminal_oral_is_connected(bridge));

    EXPECT_EQ(trigeminal_oral_disconnect(bridge), 0);
    EXPECT_FALSE(trigeminal_oral_is_connected(bridge));
}

TEST_F(TrigeminalOralBridgeTest, DisconnectNullBridge) {
    EXPECT_EQ(trigeminal_oral_disconnect(nullptr), -1);
}

TEST_F(TrigeminalOralBridgeTest, GetStatusNull) {
    EXPECT_EQ(trigeminal_oral_get_status(nullptr), TRIGEMINAL_STATUS_ERROR);
}

/* ============================================================================
 * Oral Input Processing Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, ProcessInput) {
    trigeminal_oral_connect(bridge, soma, gust);

    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_TIP,
                                               0.5f, 0.3f, 0.4f, 0.2f, 37.0f);
    EXPECT_EQ(trigeminal_oral_process_input(bridge, &input), 0);

    trigeminal_oral_stats_t stats;
    trigeminal_oral_get_stats(bridge, &stats);
    EXPECT_EQ(stats.oral_inputs_processed, 1u);
}

TEST_F(TrigeminalOralBridgeTest, ProcessInputNullBridge) {
    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_TIP,
                                               0.5f, 0.3f, 0.4f, 0.2f, 37.0f);
    EXPECT_EQ(trigeminal_oral_process_input(nullptr, &input), -1);
}

TEST_F(TrigeminalOralBridgeTest, ProcessInputNullInput) {
    trigeminal_oral_connect(bridge, soma, gust);
    EXPECT_EQ(trigeminal_oral_process_input(bridge, nullptr), -1);
}

TEST_F(TrigeminalOralBridgeTest, ProcessInputInvalidRegion) {
    trigeminal_oral_connect(bridge, soma, gust);
    oral_soma_input_t input;
    memset(&input, 0, sizeof(input));
    input.region = (oral_region_t)999;  /* Invalid */
    EXPECT_EQ(trigeminal_oral_process_input(bridge, &input), -1);
}

TEST_F(TrigeminalOralBridgeTest, ProcessMultiInput) {
    trigeminal_oral_connect(bridge, soma, gust);

    oral_soma_input_t inputs[3];
    inputs[0] = createOralInput(ORAL_REGION_TONGUE_TIP, 0.5f, 0.3f, 0.4f, 0.2f, 37.0f);
    inputs[1] = createOralInput(ORAL_REGION_TONGUE_BODY, 0.6f, 0.4f, 0.5f, 0.3f, 37.0f);
    inputs[2] = createOralInput(ORAL_REGION_PALATE_HARD, 0.4f, 0.2f, 0.3f, 0.1f, 37.0f);

    EXPECT_EQ(trigeminal_oral_process_multi(bridge, inputs, 3), 0);

    trigeminal_oral_stats_t stats;
    trigeminal_oral_get_stats(bridge, &stats);
    EXPECT_EQ(stats.oral_inputs_processed, 3u);
}

TEST_F(TrigeminalOralBridgeTest, Update) {
    trigeminal_oral_connect(bridge, soma, gust);
    EXPECT_EQ(trigeminal_oral_update(bridge, 0.016f), 0);
}

TEST_F(TrigeminalOralBridgeTest, UpdateNullBridge) {
    EXPECT_EQ(trigeminal_oral_update(nullptr, 0.016f), -1);
}

/* ============================================================================
 * Chemesthesis Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, DetectSpicyHeat) {
    trigeminal_oral_connect(bridge, soma, gust);

    chemesthesis_t result;
    EXPECT_EQ(trigeminal_oral_detect_chemesthesis(bridge, CHEMESTHESIS_SPICY_HEAT,
                                                   0.7f, ORAL_REGION_TONGUE_TIP, &result), 0);

    EXPECT_EQ(result.type, CHEMESTHESIS_SPICY_HEAT);
    EXPECT_GT(result.intensity, 0.0f);
    EXPECT_GT(result.scoville_equiv, 0.0f);
    EXPECT_EQ(result.primary_region, ORAL_REGION_TONGUE_TIP);

    trigeminal_oral_stats_t stats;
    trigeminal_oral_get_stats(bridge, &stats);
    EXPECT_EQ(stats.chemesthesis_detected, 1u);
}

TEST_F(TrigeminalOralBridgeTest, DetectCooling) {
    trigeminal_oral_connect(bridge, soma, gust);

    chemesthesis_t result;
    EXPECT_EQ(trigeminal_oral_detect_chemesthesis(bridge, CHEMESTHESIS_COOLING,
                                                   0.6f, ORAL_REGION_TONGUE_BODY, &result), 0);

    EXPECT_EQ(result.type, CHEMESTHESIS_COOLING);
    EXPECT_GT(result.cooling_intensity, 0.0f);
    EXPECT_TRUE(trigeminal_oral_is_cooling_active(bridge));
    EXPECT_GT(trigeminal_oral_get_cooling_intensity(bridge), 0.0f);
}

TEST_F(TrigeminalOralBridgeTest, DetectCarbonation) {
    trigeminal_oral_connect(bridge, soma, gust);

    chemesthesis_t result;
    EXPECT_EQ(trigeminal_oral_detect_chemesthesis(bridge, CHEMESTHESIS_CARBONATION,
                                                   0.8f, ORAL_REGION_TONGUE_TIP, &result), 0);

    EXPECT_EQ(result.type, CHEMESTHESIS_CARBONATION);
    /* Carbonation should affect multiple regions */
    EXPECT_GT(result.affected_regions, (1u << ORAL_REGION_TONGUE_TIP));
}

TEST_F(TrigeminalOralBridgeTest, DetectAstringent) {
    trigeminal_oral_connect(bridge, soma, gust);

    chemesthesis_t result;
    EXPECT_EQ(trigeminal_oral_detect_chemesthesis(bridge, CHEMESTHESIS_ASTRINGENT,
                                                   0.5f, ORAL_REGION_INNER_CHEEK, &result), 0);

    EXPECT_EQ(result.type, CHEMESTHESIS_ASTRINGENT);
    EXPECT_GT(result.duration_s, 0.0f);
}

TEST_F(TrigeminalOralBridgeTest, DetectChemesthesisNullResult) {
    trigeminal_oral_connect(bridge, soma, gust);
    EXPECT_EQ(trigeminal_oral_detect_chemesthesis(bridge, CHEMESTHESIS_SPICY_HEAT,
                                                   0.5f, ORAL_REGION_TONGUE_TIP, nullptr), -1);
}

TEST_F(TrigeminalOralBridgeTest, DetectChemesthesisDisabled) {
    trigeminal_oral_config_t config;
    trigeminal_oral_default_config(&config);
    config.enable_chemesthesis = false;

    trigeminal_oral_bridge_t* b = trigeminal_oral_bridge_create(&config);
    trigeminal_oral_connect(b, soma, gust);

    chemesthesis_t result;
    EXPECT_EQ(trigeminal_oral_detect_chemesthesis(b, CHEMESTHESIS_SPICY_HEAT,
                                                   0.5f, ORAL_REGION_TONGUE_TIP, &result), -1);

    trigeminal_oral_bridge_destroy(b);
}

TEST_F(TrigeminalOralBridgeTest, GetSpiciness) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Trigger spiciness first */
    chemesthesis_t chem;
    trigeminal_oral_detect_chemesthesis(bridge, CHEMESTHESIS_SPICY_HEAT,
                                        0.7f, ORAL_REGION_TONGUE_TIP, &chem);

    spiciness_perception_t spicy;
    EXPECT_EQ(trigeminal_oral_get_spiciness(bridge, &spicy), 0);
    EXPECT_GT(spicy.heat_level, 0.0f);
    EXPECT_GT(spicy.scoville_estimate, 0.0f);
}

TEST_F(TrigeminalOralBridgeTest, UpdateSpiceTolerance) {
    trigeminal_oral_connect(bridge, soma, gust);

    trigeminal_oral_stats_t stats_before;
    trigeminal_oral_get_stats(bridge, &stats_before);

    EXPECT_EQ(trigeminal_oral_update_spice_tolerance(bridge, 0.8f, 30.0f), 0);

    trigeminal_oral_stats_t stats_after;
    trigeminal_oral_get_stats(bridge, &stats_after);
    EXPECT_GT(stats_after.spice_tolerance_updates, stats_before.spice_tolerance_updates);
}

TEST_F(TrigeminalOralBridgeTest, CoolingDecay) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Activate cooling */
    chemesthesis_t result;
    trigeminal_oral_detect_chemesthesis(bridge, CHEMESTHESIS_COOLING,
                                        0.5f, ORAL_REGION_TONGUE_TIP, &result);

    EXPECT_TRUE(trigeminal_oral_is_cooling_active(bridge));
    float initial = trigeminal_oral_get_cooling_intensity(bridge);

    /* Update to decay */
    for (int i = 0; i < 100; i++) {
        trigeminal_oral_update(bridge, 0.1f);
    }

    /* Cooling should have decayed */
    float final_intensity = trigeminal_oral_get_cooling_intensity(bridge);
    EXPECT_LT(final_intensity, initial);
}

/* ============================================================================
 * Texture Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, AnalyzeTextureCrunchy) {
    trigeminal_oral_connect(bridge, soma, gust);

    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                               0.7f, 0.6f, 0.9f, 0.1f, 37.0f);

    texture_perception_t texture;
    EXPECT_EQ(trigeminal_oral_analyze_texture(bridge, &input, &texture), 0);

    EXPECT_EQ(texture.primary, TEXTURE_CRUNCHY);
    EXPECT_GT(texture.crunchiness, 0.5f);

    trigeminal_texture_free(&texture);
}

TEST_F(TrigeminalOralBridgeTest, AnalyzeTextureSmooth) {
    trigeminal_oral_connect(bridge, soma, gust);

    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                               0.3f, 0.1f, 0.2f, 0.5f, 37.0f);

    texture_perception_t texture;
    EXPECT_EQ(trigeminal_oral_analyze_texture(bridge, &input, &texture), 0);

    EXPECT_EQ(texture.primary, TEXTURE_SMOOTH);
    EXPECT_GT(texture.smoothness, 0.5f);

    trigeminal_texture_free(&texture);
}

TEST_F(TrigeminalOralBridgeTest, AnalyzeTextureLiquid) {
    trigeminal_oral_connect(bridge, soma, gust);

    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                               0.2f, 0.1f, 0.1f, 0.9f, 37.0f);

    texture_perception_t texture;
    EXPECT_EQ(trigeminal_oral_analyze_texture(bridge, &input, &texture), 0);

    EXPECT_EQ(texture.primary, TEXTURE_LIQUID);

    trigeminal_texture_free(&texture);
}

TEST_F(TrigeminalOralBridgeTest, AnalyzeTextureChewy) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* CHEWY requires: hardness > 0.5 && roughness < 0.2, but NOT hardness > 0.6 (which triggers CRISPY first) */
    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                               0.6f, 0.15f, 0.55f, 0.2f, 37.0f);

    texture_perception_t texture;
    EXPECT_EQ(trigeminal_oral_analyze_texture(bridge, &input, &texture), 0);

    EXPECT_EQ(texture.primary, TEXTURE_CHEWY);
    EXPECT_GT(texture.chewiness, 0.3f);

    trigeminal_texture_free(&texture);
}

TEST_F(TrigeminalOralBridgeTest, ClassifyTexture) {
    trigeminal_oral_connect(bridge, soma, gust);

    float features[3] = {0.6f, 0.8f, 0.1f};  /* roughness, hardness, viscosity */
    texture_category_t category;
    float confidence;

    EXPECT_EQ(trigeminal_oral_classify_texture(bridge, features, 3, &category, &confidence), 0);
    EXPECT_EQ(category, TEXTURE_CRUNCHY);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(TrigeminalOralBridgeTest, ClassifyTextureInsufficientFeatures) {
    trigeminal_oral_connect(bridge, soma, gust);

    float features[2] = {0.5f, 0.5f};
    texture_category_t category;
    float confidence;

    EXPECT_EQ(trigeminal_oral_classify_texture(bridge, features, 2, &category, &confidence), -1);
}

TEST_F(TrigeminalOralBridgeTest, GetTexture) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Analyze texture first */
    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                               0.5f, 0.4f, 0.6f, 0.3f, 37.0f);
    texture_perception_t tex;
    trigeminal_oral_analyze_texture(bridge, &input, &tex);
    trigeminal_texture_free(&tex);

    /* Now get current texture */
    texture_perception_t current;
    EXPECT_EQ(trigeminal_oral_get_texture(bridge, &current), 0);
}

/* ============================================================================
 * Temperature-Taste Interaction Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, ComputeTempTasteCold) {
    trigeminal_oral_connect(bridge, soma, gust);

    temp_taste_interaction_t interaction;
    EXPECT_EQ(trigeminal_oral_compute_temp_taste(bridge, 10.0f, &interaction), 0);

    EXPECT_EQ(interaction.temperature, TEMP_PERCEPTION_COLD);
    /* Cold suppresses sweet */
    EXPECT_LT(interaction.sweet_modulation, 1.0f);
    /* Cold enhances bitter */
    EXPECT_GT(interaction.bitter_modulation, 1.0f);
}

TEST_F(TrigeminalOralBridgeTest, ComputeTempTasteWarm) {
    trigeminal_oral_connect(bridge, soma, gust);

    temp_taste_interaction_t interaction;
    EXPECT_EQ(trigeminal_oral_compute_temp_taste(bridge, 50.0f, &interaction), 0);

    EXPECT_EQ(interaction.temperature, TEMP_PERCEPTION_HOT);
    /* Warm enhances sweet */
    EXPECT_GT(interaction.sweet_modulation, 1.0f);
}

TEST_F(TrigeminalOralBridgeTest, ComputeTempTasteNeutral) {
    trigeminal_oral_connect(bridge, soma, gust);

    temp_taste_interaction_t interaction;
    EXPECT_EQ(trigeminal_oral_compute_temp_taste(bridge, 35.0f, &interaction), 0);

    EXPECT_EQ(interaction.temperature, TEMP_PERCEPTION_NEUTRAL);
    /* Neutral should have modulations close to 1.0 */
    EXPECT_NEAR(interaction.sweet_modulation, 1.0f, 0.2f);
}

TEST_F(TrigeminalOralBridgeTest, ModulateTaste) {
    trigeminal_oral_connect(bridge, soma, gust);

    float sweet = 0.8f, salty = 0.3f, sour = 0.2f, bitter = 0.1f, umami = 0.4f;
    float orig_sweet = sweet;

    /* Cold temperature */
    EXPECT_EQ(trigeminal_oral_modulate_taste(bridge, 5.0f,
                                              &sweet, &salty, &sour, &bitter, &umami), 0);

    /* Sweet should be reduced at cold temp */
    EXPECT_LT(sweet, orig_sweet);
}

TEST_F(TrigeminalOralBridgeTest, OptimalTemperature) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Create sweet-dominant taste profile */
    taste_perception_t taste;
    memset(&taste, 0, sizeof(taste));
    taste.perceived_sweet = 0.9f;
    taste.perceived_bitter = 0.1f;

    float optimal_temp;
    EXPECT_EQ(trigeminal_oral_optimal_temperature(bridge, &taste, &optimal_temp), 0);

    /* Sweet foods should be served warmer */
    EXPECT_GT(optimal_temp, 30.0f);
}

TEST_F(TrigeminalOralBridgeTest, OptimalTemperatureBitter) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Create bitter-dominant taste profile */
    taste_perception_t taste;
    memset(&taste, 0, sizeof(taste));
    taste.perceived_sweet = 0.1f;
    taste.perceived_bitter = 0.8f;

    float optimal_temp;
    EXPECT_EQ(trigeminal_oral_optimal_temperature(bridge, &taste, &optimal_temp), 0);

    /* Bitter foods should be served cooler */
    EXPECT_LT(optimal_temp, 30.0f);
}

/* ============================================================================
 * Mouthfeel Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, ComputeMouthfeel) {
    trigeminal_oral_connect(bridge, soma, gust);

    oral_soma_input_t soma_input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                                    0.5f, 0.3f, 0.4f, 0.6f, 37.0f);

    taste_perception_t taste;
    memset(&taste, 0, sizeof(taste));
    taste.perceived_sweet = 0.7f;
    taste.palatability = 0.8f;

    mouthfeel_t mouthfeel;
    EXPECT_EQ(trigeminal_oral_compute_mouthfeel(bridge, &soma_input, &taste, &mouthfeel), 0);

    EXPECT_GT(mouthfeel.pleasantness, -1.0f);
    EXPECT_LT(mouthfeel.pleasantness, 1.0f);
    EXPECT_GT(mouthfeel.onset_time_ms, 0.0f);

    trigeminal_mouthfeel_free(&mouthfeel);
}

TEST_F(TrigeminalOralBridgeTest, ComputeMouthfeelThick) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* High viscosity (0.7f) + low roughness (0.2f) -> CREAMY */
    oral_soma_input_t soma_input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                                    0.5f, 0.2f, 0.3f, 0.7f, 37.0f);

    mouthfeel_t mouthfeel;
    EXPECT_EQ(trigeminal_oral_compute_mouthfeel(bridge, &soma_input, nullptr, &mouthfeel), 0);

    EXPECT_EQ(mouthfeel.primary_quality, MOUTHFEEL_CREAMY);

    trigeminal_mouthfeel_free(&mouthfeel);
}

TEST_F(TrigeminalOralBridgeTest, ComputeMouthfeelNeutral) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Low viscosity, moderate roughness -> NEUTRAL */
    oral_soma_input_t soma_input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                                    0.2f, 0.3f, 0.1f, 0.1f, 37.0f);

    mouthfeel_t mouthfeel;
    EXPECT_EQ(trigeminal_oral_compute_mouthfeel(bridge, &soma_input, nullptr, &mouthfeel), 0);

    EXPECT_EQ(mouthfeel.primary_quality, MOUTHFEEL_NEUTRAL);

    trigeminal_mouthfeel_free(&mouthfeel);
}

TEST_F(TrigeminalOralBridgeTest, GetMouthfeel) {
    trigeminal_oral_connect(bridge, soma, gust);

    oral_soma_input_t soma_input = createOralInput(ORAL_REGION_TONGUE_BODY,
                                                    0.5f, 0.3f, 0.4f, 0.5f, 37.0f);
    mouthfeel_t mf;
    trigeminal_oral_compute_mouthfeel(bridge, &soma_input, nullptr, &mf);
    trigeminal_mouthfeel_free(&mf);

    mouthfeel_t current;
    EXPECT_EQ(trigeminal_oral_get_mouthfeel(bridge, &current), 0);
}

TEST_F(TrigeminalOralBridgeTest, PredictPleasantness) {
    trigeminal_oral_connect(bridge, soma, gust);

    mouthfeel_t mouthfeel;
    memset(&mouthfeel, 0, sizeof(mouthfeel));
    mouthfeel.pleasantness = 0.7f;

    float predicted;
    EXPECT_EQ(trigeminal_oral_predict_pleasantness(bridge, &mouthfeel, &predicted), 0);
    EXPECT_FLOAT_EQ(predicted, 0.7f);
}

TEST_F(TrigeminalOralBridgeTest, FlavorTextureSynergy) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Crunchy + salty = high synergy */
    taste_perception_t taste;
    memset(&taste, 0, sizeof(taste));
    taste.perceived_salty = 0.8f;

    texture_perception_t texture;
    memset(&texture, 0, sizeof(texture));
    texture.primary = TEXTURE_CRUNCHY;

    float synergy;
    EXPECT_EQ(trigeminal_oral_flavor_texture_synergy(bridge, &taste, &texture, &synergy), 0);
    EXPECT_GT(synergy, 0.5f);  /* Should be high synergy */
}

TEST_F(TrigeminalOralBridgeTest, FlavorTextureSynergyLow) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Grainy + bitter = low synergy */
    taste_perception_t taste;
    memset(&taste, 0, sizeof(taste));
    taste.perceived_bitter = 0.8f;

    texture_perception_t texture;
    memset(&texture, 0, sizeof(texture));
    texture.primary = TEXTURE_GRAINY;

    float synergy;
    EXPECT_EQ(trigeminal_oral_flavor_texture_synergy(bridge, &taste, &texture, &synergy), 0);
    EXPECT_LT(synergy, 0.5f);  /* Should be lower synergy */
}

/* ============================================================================
 * Mastication Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, StartMastication) {
    trigeminal_oral_connect(bridge, soma, gust);

    EXPECT_EQ(trigeminal_oral_start_mastication(bridge, 0.7f), 0);

    uint32_t chew_count;
    float breakdown;
    bool ready;
    EXPECT_EQ(trigeminal_oral_get_mastication_state(bridge, &chew_count, &breakdown, &ready), 0);
    EXPECT_EQ(chew_count, 0u);
    EXPECT_FLOAT_EQ(breakdown, 0.0f);
    EXPECT_FALSE(ready);
}

TEST_F(TrigeminalOralBridgeTest, UpdateMastication) {
    trigeminal_oral_connect(bridge, soma, gust);

    trigeminal_oral_start_mastication(bridge, 0.5f);

    /* Simulate chewing cycles */
    for (int i = 0; i < 10; i++) {
        trigeminal_oral_update_mastication(bridge, 0.8f, 0.5f);  /* Bite */
        trigeminal_oral_update_mastication(bridge, 0.1f, 0.8f);  /* Release */
    }

    uint32_t chew_count;
    float breakdown;
    bool ready;
    trigeminal_oral_get_mastication_state(bridge, &chew_count, &breakdown, &ready);

    EXPECT_GT(chew_count, 0u);
    EXPECT_GT(breakdown, 0.0f);
}

TEST_F(TrigeminalOralBridgeTest, MasticationReadyToSwallow) {
    trigeminal_oral_connect(bridge, soma, gust);

    trigeminal_oral_start_mastication(bridge, 0.3f);  /* Soft food */

    /* Simulate many chewing cycles */
    for (int i = 0; i < 50; i++) {
        trigeminal_oral_update_mastication(bridge, 0.9f, 0.5f);
        trigeminal_oral_update_mastication(bridge, 0.1f, 0.8f);
    }

    uint32_t chew_count;
    float breakdown;
    bool ready;
    trigeminal_oral_get_mastication_state(bridge, &chew_count, &breakdown, &ready);

    EXPECT_GT(breakdown, 0.5f);
}

TEST_F(TrigeminalOralBridgeTest, EndMastication) {
    trigeminal_oral_connect(bridge, soma, gust);

    trigeminal_oral_start_mastication(bridge, 0.5f);
    EXPECT_EQ(trigeminal_oral_end_mastication(bridge), 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, GetStats) {
    trigeminal_oral_connect(bridge, soma, gust);

    trigeminal_oral_stats_t stats;
    EXPECT_EQ(trigeminal_oral_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.oral_inputs_processed, 0u);
}

TEST_F(TrigeminalOralBridgeTest, GetStatsNull) {
    EXPECT_EQ(trigeminal_oral_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(trigeminal_oral_get_stats(nullptr, nullptr), -1);
}

TEST_F(TrigeminalOralBridgeTest, ResetStats) {
    trigeminal_oral_connect(bridge, soma, gust);

    /* Generate some stats */
    oral_soma_input_t input = createOralInput(ORAL_REGION_TONGUE_TIP,
                                               0.5f, 0.3f, 0.4f, 0.2f, 37.0f);
    trigeminal_oral_process_input(bridge, &input);

    trigeminal_oral_stats_t stats;
    trigeminal_oral_get_stats(bridge, &stats);
    EXPECT_GT(stats.oral_inputs_processed, 0u);

    EXPECT_EQ(trigeminal_oral_reset_stats(bridge), 0);

    trigeminal_oral_get_stats(bridge, &stats);
    EXPECT_EQ(stats.oral_inputs_processed, 0u);
}

TEST_F(TrigeminalOralBridgeTest, PrintSummary) {
    trigeminal_oral_connect(bridge, soma, gust);
    trigeminal_oral_print_summary(bridge);
    trigeminal_oral_print_summary(nullptr);  /* Should not crash */
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, OralRegionName) {
    EXPECT_STREQ(trigeminal_oral_region_name(ORAL_REGION_TONGUE_TIP), "Tongue Tip");
    EXPECT_STREQ(trigeminal_oral_region_name(ORAL_REGION_PALATE_HARD), "Hard Palate");
    EXPECT_STREQ(trigeminal_oral_region_name(ORAL_REGION_LIPS), "Lips");
    EXPECT_STREQ(trigeminal_oral_region_name((oral_region_t)999), "Unknown");
}

TEST_F(TrigeminalOralBridgeTest, ChemesthesisName) {
    EXPECT_STREQ(trigeminal_chemesthesis_name(CHEMESTHESIS_SPICY_HEAT), "Spicy Heat");
    EXPECT_STREQ(trigeminal_chemesthesis_name(CHEMESTHESIS_COOLING), "Cooling");
    EXPECT_STREQ(trigeminal_chemesthesis_name(CHEMESTHESIS_CARBONATION), "Carbonation");
    EXPECT_STREQ(trigeminal_chemesthesis_name((chemesthesis_type_t)999), "Unknown");
}

TEST_F(TrigeminalOralBridgeTest, TextureName) {
    EXPECT_STREQ(trigeminal_texture_name(TEXTURE_CRUNCHY), "Crunchy");
    EXPECT_STREQ(trigeminal_texture_name(TEXTURE_SMOOTH), "Smooth");
    EXPECT_STREQ(trigeminal_texture_name(TEXTURE_CHEWY), "Chewy");
    EXPECT_STREQ(trigeminal_texture_name((texture_category_t)999), "Unknown");
}

TEST_F(TrigeminalOralBridgeTest, MouthfeelName) {
    EXPECT_STREQ(trigeminal_mouthfeel_name(MOUTHFEEL_NEUTRAL), "Neutral");
    EXPECT_STREQ(trigeminal_mouthfeel_name(MOUTHFEEL_CREAMY), "Creamy");
    EXPECT_STREQ(trigeminal_mouthfeel_name(MOUTHFEEL_COOLING), "Cooling");
    EXPECT_STREQ(trigeminal_mouthfeel_name(MOUTHFEEL_BURNING), "Burning");
    EXPECT_STREQ(trigeminal_mouthfeel_name((mouthfeel_quality_t)999), "Unknown");
}

TEST_F(TrigeminalOralBridgeTest, ScovilleConversion) {
    /* Test conversion roundtrip */
    float scoville = 50000.0f;  /* Cayenne pepper level */
    float normalized = trigeminal_scoville_to_normalized(scoville);

    EXPECT_GT(normalized, 0.0f);
    EXPECT_LT(normalized, 1.0f);

    float back = trigeminal_normalized_to_scoville(normalized);
    EXPECT_NEAR(back, scoville, scoville * 0.1f);
}

TEST_F(TrigeminalOralBridgeTest, ScovilleZero) {
    EXPECT_FLOAT_EQ(trigeminal_scoville_to_normalized(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(trigeminal_normalized_to_scoville(0.0f), 0.0f);
}

TEST_F(TrigeminalOralBridgeTest, ScovilleMax) {
    float normalized = trigeminal_scoville_to_normalized(TRIGEMINAL_SCOVILLE_SCALE);
    EXPECT_LE(normalized, 1.0f);
}

/* ============================================================================
 * Cleanup Tests
 * ============================================================================ */

TEST_F(TrigeminalOralBridgeTest, TextureFree) {
    texture_perception_t texture;
    memset(&texture, 0, sizeof(texture));
    texture.texture_profile = (float*)malloc(8 * sizeof(float));

    trigeminal_texture_free(&texture);
    EXPECT_EQ(texture.texture_profile, nullptr);

    trigeminal_texture_free(nullptr);  /* Should not crash */
}

TEST_F(TrigeminalOralBridgeTest, MouthfeelFree) {
    mouthfeel_t mouthfeel;
    memset(&mouthfeel, 0, sizeof(mouthfeel));
    mouthfeel.mouthfeel_profile = (float*)malloc(12 * sizeof(float));
    mouthfeel.texture.texture_profile = (float*)malloc(8 * sizeof(float));

    trigeminal_mouthfeel_free(&mouthfeel);
    EXPECT_EQ(mouthfeel.mouthfeel_profile, nullptr);

    trigeminal_mouthfeel_free(nullptr);  /* Should not crash */
}

/* Main entry point */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
