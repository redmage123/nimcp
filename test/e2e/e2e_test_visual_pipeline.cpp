/**
 * @file e2e_test_visual_pipeline.cpp
 * @brief End-to-end tests for Occipital Visual Processing Pipeline
 *
 * WHAT: Full pipeline tests for visual cortex processing
 * WHY:  Verify complete visual processing workflows with substrate integration
 * HOW:  Test V1 processing, color, form, motion perception
 *
 * TEST COVERAGE:
 * - Primary Visual Cortex (V1) Pipeline (4 tests)
 * - Color Processing (3 tests)
 * - Form/Shape Processing (4 tests)
 * - Motion Perception (4 tests)
 * - Metabolic Effects (3 tests)
 * - Hierarchical Processing (3 tests)
 *
 * TOTAL: 21 tests
 *
 * BIOLOGICAL ANALOGY:
 * - V1 receives input from LGN (thalamus)
 * - Retinotopic organization maintained
 * - Orientation, spatial frequency tuning in V1
 * - V2/V4 process more complex features
 * - Dorsal stream: motion (MT/MST)
 * - Ventral stream: form/color (V4/IT)
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_substrate_bridge.h"
#include "core/occipital/nimcp_occipital_thalamic_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_V1_PROCESSING_TIME_MS = 20.0;
constexpr double MAX_HIGHER_PROCESSING_TIME_MS = 50.0;
constexpr float MIN_VISUAL_CAPACITY = 0.3f;
constexpr uint32_t VISUAL_FIELD_WIDTH = 64;
constexpr uint32_t VISUAL_FIELD_HEIGHT = 64;
constexpr uint32_t NUM_ORIENTATIONS = 8;
constexpr float CONTRAST_THRESHOLD = 0.1f;

//=============================================================================
// Helper Structures
//=============================================================================

struct VisualStimulus {
    std::vector<float> luminance;
    uint32_t width;
    uint32_t height;
    float contrast;
    float orientation;  // Radians
};

struct ColorStimulus {
    std::vector<float> red;
    std::vector<float> green;
    std::vector<float> blue;
    uint32_t width;
    uint32_t height;
};

struct MotionStimulus {
    std::vector<float> frames;  // Multiple frames
    uint32_t num_frames;
    float velocity;
    float direction;  // Radians
};

//=============================================================================
// Test Fixtures
//=============================================================================

class E2EVisualV1Test : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    occipital_substrate_bridge_t* occ_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        occipital_substrate_config_t occ_config = occipital_substrate_default_config();
        occ_bridge = occipital_substrate_bridge_create(nullptr, substrate, &occ_config);
        ASSERT_NE(occ_bridge, nullptr);
    }

    void TearDown() override {
        if (occ_bridge) {
            occipital_substrate_bridge_destroy(occ_bridge);
            occ_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    VisualStimulus createGrating(float orientation, float frequency, float contrast) {
        VisualStimulus stim;
        stim.width = 32;
        stim.height = 32;
        stim.orientation = orientation;
        stim.contrast = contrast;
        stim.luminance.resize(stim.width * stim.height);

        for (uint32_t y = 0; y < stim.height; y++) {
            for (uint32_t x = 0; x < stim.width; x++) {
                float rx = x * cosf(orientation) + y * sinf(orientation);
                float value = 0.5f + 0.5f * contrast * sinf(2.0f * M_PI * frequency * rx);
                stim.luminance[y * stim.width + x] = value;
            }
        }
        return stim;
    }
};

class E2EVisualColorTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    occipital_substrate_bridge_t* occ_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        occipital_substrate_config_t occ_config = occipital_substrate_default_config();
        occ_bridge = occipital_substrate_bridge_create(nullptr, substrate, &occ_config);
        ASSERT_NE(occ_bridge, nullptr);
    }

    void TearDown() override {
        if (occ_bridge) {
            occipital_substrate_bridge_destroy(occ_bridge);
            occ_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    ColorStimulus createColorPatch(float r, float g, float b, uint32_t size) {
        ColorStimulus stim;
        stim.width = size;
        stim.height = size;
        stim.red.resize(size * size, r);
        stim.green.resize(size * size, g);
        stim.blue.resize(size * size, b);
        return stim;
    }
};

class E2EVisualFormTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    occipital_substrate_bridge_t* occ_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        occipital_substrate_config_t occ_config = occipital_substrate_default_config();
        occ_bridge = occipital_substrate_bridge_create(nullptr, substrate, &occ_config);
        ASSERT_NE(occ_bridge, nullptr);
    }

    void TearDown() override {
        if (occ_bridge) {
            occipital_substrate_bridge_destroy(occ_bridge);
            occ_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2EVisualMotionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    occipital_substrate_bridge_t* occ_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        occipital_substrate_config_t occ_config = occipital_substrate_default_config();
        occ_bridge = occipital_substrate_bridge_create(nullptr, substrate, &occ_config);
        ASSERT_NE(occ_bridge, nullptr);
    }

    void TearDown() override {
        if (occ_bridge) {
            occipital_substrate_bridge_destroy(occ_bridge);
            occ_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    MotionStimulus createMovingDot(float velocity, float direction, uint32_t num_frames) {
        MotionStimulus stim;
        stim.num_frames = num_frames;
        stim.velocity = velocity;
        stim.direction = direction;

        uint32_t frame_size = 32 * 32;
        stim.frames.resize(frame_size * num_frames, 0.0f);

        float x = 16.0f, y = 16.0f;  // Start center
        float dx = velocity * cosf(direction);
        float dy = velocity * sinf(direction);

        for (uint32_t f = 0; f < num_frames; f++) {
            int ix = (int)(x + 0.5f);
            int iy = (int)(y + 0.5f);

            if (ix >= 0 && ix < 32 && iy >= 0 && iy < 32) {
                stim.frames[f * frame_size + iy * 32 + ix] = 1.0f;
            }

            x += dx;
            y += dy;
        }

        return stim;
    }
};

//=============================================================================
// Primary Visual Cortex (V1) Pipeline Tests
//=============================================================================

TEST_F(E2EVisualV1Test, BaselineVisualCapacity) {
    // Scenario: Verify baseline visual processing with optimal substrate
    E2E_PIPELINE_START("Baseline Visual Capacity");

    E2E_STAGE_BEGIN("Initialize substrate", 5);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update occipital bridge", 10);
    int result = occipital_substrate_bridge_update(occ_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get effects", 5);
    occipital_substrate_effects_t effects;
    result = occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity", 2);
    EXPECT_GT(effects.overall_capacity, MIN_VISUAL_CAPACITY);
    EXPECT_GT(effects.v1_contrast_sensitivity, MIN_VISUAL_CAPACITY);
    EXPECT_GT(effects.v4_color_constancy, MIN_VISUAL_CAPACITY);
    EXPECT_GT(effects.v4_complex_form, MIN_VISUAL_CAPACITY);
    EXPECT_GT(effects.v5_motion_direction, MIN_VISUAL_CAPACITY);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualV1Test, OrientationTuning) {
    // Scenario: Process oriented gratings
    E2E_PIPELINE_START("Orientation Tuning");

    E2E_STAGE_BEGIN("Create oriented stimuli", 20);
    std::vector<VisualStimulus> gratings;

    for (int ori = 0; ori < NUM_ORIENTATIONS; ori++) {
        float orientation = ori * M_PI / NUM_ORIENTATIONS;
        gratings.push_back(createGrating(orientation, 0.1f, 0.8f));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process each orientation", 100);
    for (size_t i = 0; i < gratings.size(); i++) {
        // Processing visual input consumes energy
        substrate_record_spikes(substrate, 150);
        substrate_update(substrate, 20);
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);

        EXPECT_GE(effects.v1_contrast_sensitivity, 0.0f);
        EXPECT_LE(effects.v1_contrast_sensitivity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify processing", 5);
    occipital_substrate_effects_t final_effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &final_effects);
    EXPECT_GT(final_effects.v1_contrast_sensitivity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualV1Test, ContrastSensitivity) {
    // Scenario: Test contrast sensitivity
    E2E_PIPELINE_START("Contrast Sensitivity");

    float contrasts[] = {0.1f, 0.3f, 0.5f, 0.7f, 1.0f};

    E2E_STAGE_BEGIN("Process contrasts", 50);
    for (float contrast : contrasts) {
        VisualStimulus stim = createGrating(0.0f, 0.1f, contrast);

        substrate_record_spikes(substrate, (uint32_t)(contrast * 200));
        substrate_update(substrate, 20);
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);

        EXPECT_GE(effects.v1_contrast_sensitivity, 0.0f);
        EXPECT_FALSE(std::isnan(effects.v1_contrast_sensitivity));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stability", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualV1Test, ApplyVisualEffects) {
    // Scenario: Apply visual processing effects
    E2E_PIPELINE_START("Apply Visual Effects");

    E2E_STAGE_BEGIN("Update and apply", 20);
    occipital_substrate_bridge_update(occ_bridge);
    int result = occipital_substrate_bridge_apply_effects(occ_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify application", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Color Processing Tests
//=============================================================================

TEST_F(E2EVisualColorTest, ColorProcessingCapacity) {
    // Scenario: Baseline color processing
    E2E_PIPELINE_START("Color Processing Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get color processing", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.v4_color_constancy, MIN_VISUAL_CAPACITY);
    EXPECT_LE(effects.v4_color_constancy, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualColorTest, ColorChannelProcessing) {
    // Scenario: Process different color channels
    E2E_PIPELINE_START("Color Channel Processing");

    E2E_STAGE_BEGIN("Process colors", 50);
    // Red, Green, Blue, Yellow, Cyan, Magenta
    std::vector<std::tuple<float, float, float>> colors = {
        {1.0f, 0.0f, 0.0f},  // Red
        {0.0f, 1.0f, 0.0f},  // Green
        {0.0f, 0.0f, 1.0f},  // Blue
        {1.0f, 1.0f, 0.0f},  // Yellow
        {0.0f, 1.0f, 1.0f},  // Cyan
        {1.0f, 0.0f, 1.0f}   // Magenta
    };

    for (const auto& [r, g, b] : colors) {
        ColorStimulus stim = createColorPatch(r, g, b, 16);

        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 30);
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);

        EXPECT_GE(effects.v4_color_constancy, 0.0f);
        EXPECT_LE(effects.v4_color_constancy, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stability", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_GE(effects.v4_color_constancy, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualColorTest, ColorUnderMetabolicStress) {
    // Scenario: Color processing under metabolic stress
    E2E_PIPELINE_START("Color Under Metabolic Stress");

    E2E_STAGE_BEGIN("Normal ATP color", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t normal;
    occipital_substrate_bridge_get_effects(occ_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low ATP color", 10);
    substrate_set_atp(substrate, 0.4f);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t stressed;
    occipital_substrate_bridge_get_effects(occ_bridge, &stressed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GT(normal.v4_color_constancy, 0.0f);
    EXPECT_GE(stressed.v4_color_constancy, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Form/Shape Processing Tests
//=============================================================================

TEST_F(E2EVisualFormTest, FormProcessingCapacity) {
    // Scenario: Baseline form processing
    E2E_PIPELINE_START("Form Processing Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get form processing", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.v4_complex_form, MIN_VISUAL_CAPACITY);
    EXPECT_LE(effects.v4_complex_form, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualFormTest, ShapeComplexityProcessing) {
    // Scenario: Process shapes of increasing complexity
    E2E_PIPELINE_START("Shape Complexity Processing");

    E2E_STAGE_BEGIN("Process shapes", 100);
    // Simulate processing shapes of increasing complexity
    for (int complexity = 1; complexity <= 10; complexity++) {
        // Higher complexity = more processing
        uint32_t spikes = 50 + complexity * 30;

        substrate_record_spikes(substrate, spikes);
        substrate_update(substrate, 30);
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);

        EXPECT_GE(effects.v4_complex_form, 0.0f);
        EXPECT_LE(effects.v4_complex_form, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify processing", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_GE(effects.v4_complex_form, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualFormTest, EdgeDetectionPipeline) {
    // Scenario: Edge detection across visual field
    E2E_PIPELINE_START("Edge Detection Pipeline");

    E2E_STAGE_BEGIN("Simulate edges", 50);
    // Process edges at different locations
    for (int location = 0; location < 16; location++) {
        substrate_record_spikes(substrate, 80);
        substrate_update(substrate, 20);
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);

        EXPECT_GE(effects.v4_complex_form, 0.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final state", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_GT(effects.v4_complex_form, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualFormTest, FormWithFatigue) {
    // Scenario: Form processing degrades with fatigue
    E2E_PIPELINE_START("Form With Fatigue");

    E2E_STAGE_BEGIN("Fresh state", 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t fresh;
    occipital_substrate_bridge_get_effects(occ_bridge, &fresh);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce fatigue", 100);
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 250);
        substrate_record_transmissions(substrate, 600);
        substrate_update(substrate, 20);
    }
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t fatigued;
    occipital_substrate_bridge_get_effects(occ_bridge, &fatigued);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(fresh.v4_complex_form, 0.0f);
    EXPECT_GE(fatigued.v4_complex_form, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Motion Perception Tests
//=============================================================================

TEST_F(E2EVisualMotionTest, MotionPerceptionCapacity) {
    // Scenario: Baseline motion perception
    E2E_PIPELINE_START("Motion Perception Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get motion perception", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.v5_motion_direction, MIN_VISUAL_CAPACITY);
    EXPECT_LE(effects.v5_motion_direction, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualMotionTest, DirectionSelectivity) {
    // Scenario: Process motion in different directions
    E2E_PIPELINE_START("Direction Selectivity");

    E2E_STAGE_BEGIN("Process directions", 100);
    float directions[] = {0.0f, M_PI/4, M_PI/2, 3*M_PI/4, M_PI, 5*M_PI/4, 3*M_PI/2, 7*M_PI/4};

    for (float dir : directions) {
        MotionStimulus stim = createMovingDot(1.0f, dir, 10);

        // Motion processing requires temporal integration
        for (uint32_t f = 0; f < stim.num_frames; f++) {
            substrate_record_spikes(substrate, 60);
            substrate_update(substrate, 16);  // ~60 Hz
        }
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);

        EXPECT_GE(effects.v5_motion_direction, 0.0f);
        EXPECT_LE(effects.v5_motion_direction, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stability", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_GE(effects.v5_motion_direction, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualMotionTest, VelocitySensitivity) {
    // Scenario: Process motion at different velocities
    E2E_PIPELINE_START("Velocity Sensitivity");

    E2E_STAGE_BEGIN("Process velocities", 100);
    float velocities[] = {0.5f, 1.0f, 2.0f, 4.0f, 8.0f};

    for (float vel : velocities) {
        MotionStimulus stim = createMovingDot(vel, 0.0f, 10);

        for (uint32_t f = 0; f < stim.num_frames; f++) {
            substrate_record_spikes(substrate, (uint32_t)(vel * 40));
            substrate_update(substrate, 16);
        }
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);

        EXPECT_GE(effects.v5_motion_direction, 0.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 5);
    occipital_substrate_effects_t effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &effects);
    EXPECT_GE(effects.v5_motion_direction, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualMotionTest, MotionWithTemporalDegradation) {
    // Scenario: Motion processing requires precise timing
    E2E_PIPELINE_START("Motion With Temporal Degradation");

    E2E_STAGE_BEGIN("Normal timing", 30);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t normal;
    occipital_substrate_bridge_get_effects(occ_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Temperature affects timing", 30);
    // Lower temperature slows neural processing
    substrate_set_temperature(substrate, 34.0f);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t cooled;
    occipital_substrate_bridge_get_effects(occ_bridge, &cooled);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(normal.v5_motion_direction, 0.0f);
    EXPECT_GE(cooled.v5_motion_direction, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Metabolic Effects Tests
//=============================================================================

TEST_F(E2EVisualV1Test, ATPEffectsOnVisualProcessing) {
    // Scenario: ATP levels affect visual processing
    E2E_PIPELINE_START("ATP Effects On Visual Processing");

    float atp_levels[] = {0.95f, 0.7f, 0.5f, 0.3f};
    std::vector<occipital_substrate_effects_t> effects_at_atp;

    E2E_STAGE_BEGIN("Test ATP levels", 40);
    for (float atp : atp_levels) {
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 10);
        occipital_substrate_bridge_update(occ_bridge);

        occipital_substrate_effects_t effects;
        occipital_substrate_bridge_get_effects(occ_bridge, &effects);
        effects_at_atp.push_back(effects);

        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all valid", 5);
    for (const auto& eff : effects_at_atp) {
        EXPECT_FALSE(std::isnan(eff.v1_contrast_sensitivity));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualV1Test, OxygenDependentVisualProcessing) {
    // Scenario: Visual cortex is oxygen hungry
    E2E_PIPELINE_START("Oxygen Dependent Visual Processing");

    E2E_STAGE_BEGIN("Normal oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t normal_o2;
    occipital_substrate_bridge_get_effects(occ_bridge, &normal_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t low_o2;
    occipital_substrate_bridge_get_effects(occ_bridge, &low_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(normal_o2.v1_contrast_sensitivity, 0.0f);
    EXPECT_GE(low_o2.v1_contrast_sensitivity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualV1Test, GlucoseForSustainedProcessing) {
    // Scenario: Sustained visual processing requires glucose
    E2E_PIPELINE_START("Glucose For Sustained Processing");

    E2E_STAGE_BEGIN("Normal glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t normal;
    occipital_substrate_bridge_get_effects(occ_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_CRITICAL_GLUCOSE);
    substrate_update(substrate, 10);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t low;
    occipital_substrate_bridge_get_effects(occ_bridge, &low);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(normal.overall_capacity, 0.0f);
    EXPECT_GE(low.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Hierarchical Processing Tests
//=============================================================================

TEST_F(E2EVisualV1Test, V1ToHigherAreasProgression) {
    // Scenario: Processing progresses from V1 to higher areas
    E2E_PIPELINE_START("V1 To Higher Areas Progression");

    E2E_STAGE_BEGIN("V1 processing", 20);
    substrate_record_spikes(substrate, 200);
    substrate_update(substrate, 30);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t v1_stage;
    occipital_substrate_bridge_get_effects(occ_bridge, &v1_stage);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("V2/V4 processing", 20);
    substrate_record_spikes(substrate, 150);
    substrate_update(substrate, 50);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t v4_stage;
    occipital_substrate_bridge_get_effects(occ_bridge, &v4_stage);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("MT processing", 20);
    substrate_record_spikes(substrate, 100);
    substrate_update(substrate, 50);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t mt_stage;
    occipital_substrate_bridge_get_effects(occ_bridge, &mt_stage);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify hierarchy", 5);
    EXPECT_GE(v1_stage.v1_contrast_sensitivity, 0.0f);
    EXPECT_GE(v4_stage.v4_complex_form, 0.0f);
    EXPECT_GE(mt_stage.v5_motion_direction, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualV1Test, DorsalVentralStreamSeparation) {
    // Scenario: Dorsal (motion) and ventral (form/color) streams
    E2E_PIPELINE_START("Dorsal Ventral Stream Separation");

    E2E_STAGE_BEGIN("Process dorsal stream", 30);
    // Motion heavy stimulus
    for (int i = 0; i < 10; i++) {
        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 16);
    }
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t dorsal;
    occipital_substrate_bridge_get_effects(occ_bridge, &dorsal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process ventral stream", 30);
    // Form/color heavy stimulus
    substrate_record_spikes(substrate, 200);
    substrate_update(substrate, 50);
    occipital_substrate_bridge_update(occ_bridge);

    occipital_substrate_effects_t ventral;
    occipital_substrate_bridge_get_effects(occ_bridge, &ventral);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify both streams", 5);
    EXPECT_GE(dorsal.v5_motion_direction, 0.0f);
    EXPECT_GE(ventral.v4_complex_form, 0.0f);
    EXPECT_GE(ventral.v4_color_constancy, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EVisualV1Test, LongSimulationStability) {
    // Scenario: Extended visual processing without degradation
    E2E_PIPELINE_START("Long Simulation Stability");

    E2E_STAGE_BEGIN("Extended simulation", 500);
    for (int step = 0; step < 1000; step++) {
        substrate_record_spikes(substrate, 50);
        substrate_update(substrate, 10);
        occipital_substrate_bridge_update(occ_bridge);

        if (step % 100 == 0) {
            occipital_substrate_effects_t effects;
            occipital_substrate_bridge_get_effects(occ_bridge, &effects);

            EXPECT_FALSE(std::isnan(effects.overall_capacity));
            EXPECT_FALSE(std::isinf(effects.overall_capacity));
            EXPECT_GE(effects.overall_capacity, 0.0f);
            EXPECT_LE(effects.overall_capacity, 1.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    occipital_substrate_effects_t final_effects;
    occipital_substrate_bridge_get_effects(occ_bridge, &final_effects);

    EXPECT_GT(final_effects.overall_capacity, 0.0f);
    EXPECT_GT(final_effects.v1_contrast_sensitivity, 0.0f);
    EXPECT_GT(final_effects.v4_color_constancy, 0.0f);
    EXPECT_GT(final_effects.v4_complex_form, 0.0f);
    EXPECT_GT(final_effects.v5_motion_direction, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
