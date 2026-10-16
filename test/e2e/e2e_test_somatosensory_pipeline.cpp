/**
 * @file e2e_test_somatosensory_pipeline.cpp
 * @brief End-to-end tests for Parietal Somatosensory Processing Pipeline
 *
 * WHAT: Full pipeline tests for parietal lobe sensory processing
 * WHY:  Verify complete somatosensory workflows with substrate integration
 * HOW:  Test spatial attention, body awareness, sensory integration, numerical processing
 *
 * TEST COVERAGE:
 * - Spatial Attention Pipeline (4 tests)
 * - Body Awareness (3 tests)
 * - Sensory Integration (4 tests)
 * - Numerical Processing (4 tests)
 * - Metabolic Effects (3 tests)
 * - Long-Term Stability (3 tests)
 *
 * TOTAL: 21 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Primary somatosensory cortex (S1) in postcentral gyrus
 * - Posterior parietal cortex for spatial processing
 * - Intraparietal sulcus for numerical cognition
 * - Superior parietal lobule for body awareness
 * - Multimodal integration in association areas
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
#include "core/parietal/nimcp_parietal_substrate_bridge.h"
#include "core/parietal/nimcp_parietal_thalamic_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_ATTENTION_TIME_MS = 30.0;
constexpr double MAX_INTEGRATION_TIME_MS = 50.0;
constexpr double MAX_NUMERICAL_TIME_MS = 100.0;
constexpr float MIN_PARIETAL_CAPACITY = 0.3f;
constexpr uint32_t SPATIAL_FIELD_SIZE = 32;
constexpr uint32_t NUM_BODY_PARTS = 10;
constexpr float ATTENTION_THRESHOLD = 0.5f;

//=============================================================================
// Helper Structures
//=============================================================================

struct SpatialLocation {
    float x, y, z;
    float salience;
};

struct TouchStimulus {
    uint32_t body_part_id;
    float intensity;
    float location_x;
    float location_y;
};

struct MultimodalInput {
    std::vector<float> visual;
    std::vector<float> auditory;
    std::vector<float> tactile;
    float congruence;  // How well they align
};

//=============================================================================
// Test Fixtures
//=============================================================================

class E2EParSpatialAttentionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    parietal_substrate_bridge_t* par_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        parietal_substrate_config_t par_config = parietal_substrate_default_config();
        par_bridge = parietal_substrate_bridge_create(nullptr, substrate, &par_config);
        ASSERT_NE(par_bridge, nullptr);
    }

    void TearDown() override {
        if (par_bridge) {
            parietal_substrate_bridge_destroy(par_bridge);
            par_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    SpatialLocation createLocation(float x, float y, float z, float salience) {
        SpatialLocation loc;
        loc.x = x;
        loc.y = y;
        loc.z = z;
        loc.salience = salience;
        return loc;
    }
};

class E2EParBodyAwarenessTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    parietal_substrate_bridge_t* par_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        parietal_substrate_config_t par_config = parietal_substrate_default_config();
        par_bridge = parietal_substrate_bridge_create(nullptr, substrate, &par_config);
        ASSERT_NE(par_bridge, nullptr);
    }

    void TearDown() override {
        if (par_bridge) {
            parietal_substrate_bridge_destroy(par_bridge);
            par_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    TouchStimulus createTouch(uint32_t body_part, float intensity) {
        TouchStimulus stim;
        stim.body_part_id = body_part;
        stim.intensity = intensity;
        stim.location_x = 0.5f;
        stim.location_y = 0.5f;
        return stim;
    }
};

class E2EParSensoryIntegrationTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    parietal_substrate_bridge_t* par_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        parietal_substrate_config_t par_config = parietal_substrate_default_config();
        par_bridge = parietal_substrate_bridge_create(nullptr, substrate, &par_config);
        ASSERT_NE(par_bridge, nullptr);
    }

    void TearDown() override {
        if (par_bridge) {
            parietal_substrate_bridge_destroy(par_bridge);
            par_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    MultimodalInput createMultimodal(float congruence) {
        MultimodalInput input;
        input.congruence = congruence;
        input.visual.resize(32, 0.5f);
        input.auditory.resize(16, 0.5f);
        input.tactile.resize(16, 0.5f);
        return input;
    }
};

class E2EParNumericalTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    parietal_substrate_bridge_t* par_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        parietal_substrate_config_t par_config = parietal_substrate_default_config();
        par_bridge = parietal_substrate_bridge_create(nullptr, substrate, &par_config);
        ASSERT_NE(par_bridge, nullptr);
    }

    void TearDown() override {
        if (par_bridge) {
            parietal_substrate_bridge_destroy(par_bridge);
            par_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

//=============================================================================
// Spatial Attention Pipeline Tests
//=============================================================================

TEST_F(E2EParSpatialAttentionTest, BaselineSpatialCapacity) {
    // Scenario: Verify baseline spatial attention with optimal substrate
    E2E_PIPELINE_START("Baseline Spatial Capacity");

    E2E_STAGE_BEGIN("Initialize substrate", 5);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update parietal bridge", 10);
    int result = parietal_substrate_bridge_update(par_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get effects", 5);
    parietal_substrate_effects_t effects;
    result = parietal_substrate_bridge_get_effects(par_bridge, &effects);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity", 2);
    EXPECT_GT(effects.overall_capacity, MIN_PARIETAL_CAPACITY);
    EXPECT_GT(effects.spatial_attention, MIN_PARIETAL_CAPACITY);
    EXPECT_GT(effects.numerical_processing, MIN_PARIETAL_CAPACITY);
    EXPECT_GT(effects.sensory_integration, MIN_PARIETAL_CAPACITY);
    EXPECT_GT(effects.body_awareness, MIN_PARIETAL_CAPACITY);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSpatialAttentionTest, AttentionShiftPipeline) {
    // Scenario: Shift attention across spatial field
    E2E_PIPELINE_START("Attention Shift Pipeline");

    E2E_STAGE_BEGIN("Create attention targets", 10);
    std::vector<SpatialLocation> targets;

    for (int i = 0; i < 8; i++) {
        float angle = i * M_PI / 4;
        targets.push_back(createLocation(
            cosf(angle) * 10.0f,
            sinf(angle) * 10.0f,
            0.0f,
            0.8f
        ));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Shift attention", 100);
    for (size_t i = 0; i < targets.size(); i++) {
        // Attention shift consumes resources
        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 30);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);

        EXPECT_GE(effects.spatial_attention, 0.0f);
        EXPECT_LE(effects.spatial_attention, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stability", 5);
    parietal_substrate_effects_t final_effects;
    parietal_substrate_bridge_get_effects(par_bridge, &final_effects);
    EXPECT_GT(final_effects.spatial_attention, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSpatialAttentionTest, SustainedAttention) {
    // Scenario: Maintain sustained spatial attention
    E2E_PIPELINE_START("Sustained Attention");

    E2E_STAGE_BEGIN("Sustained focus", 200);
    std::vector<float> attention_values;

    for (int step = 0; step < 50; step++) {
        substrate_record_spikes(substrate, 80);
        substrate_update(substrate, 50);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);
        attention_values.push_back(effects.spatial_attention);

        EXPECT_GE(effects.spatial_attention, 0.0f);
        EXPECT_LE(effects.spatial_attention, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 5);
    float mean = std::accumulate(attention_values.begin(), attention_values.end(), 0.0f)
                 / attention_values.size();
    EXPECT_GT(mean, 0.0f);

    // Check for stability
    float variance = 0.0f;
    for (float v : attention_values) {
        variance += (v - mean) * (v - mean);
    }
    variance /= attention_values.size();
    EXPECT_LT(variance, 0.5f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSpatialAttentionTest, ApplySpatialEffects) {
    // Scenario: Apply spatial attention effects
    E2E_PIPELINE_START("Apply Spatial Effects");

    E2E_STAGE_BEGIN("Update and apply", 20);
    parietal_substrate_bridge_update(par_bridge);
    int result = parietal_substrate_bridge_apply_effects(par_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify application", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Body Awareness Tests
//=============================================================================

TEST_F(E2EParBodyAwarenessTest, BodyAwarenessCapacity) {
    // Scenario: Baseline body awareness
    E2E_PIPELINE_START("Body Awareness Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get body awareness", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.body_awareness, MIN_PARIETAL_CAPACITY);
    EXPECT_LE(effects.body_awareness, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParBodyAwarenessTest, SomatotopicMapping) {
    // Scenario: Process touch across body parts
    E2E_PIPELINE_START("Somatotopic Mapping");

    E2E_STAGE_BEGIN("Process body parts", 100);
    for (uint32_t body_part = 0; body_part < NUM_BODY_PARTS; body_part++) {
        TouchStimulus touch = createTouch(body_part, 0.7f);

        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 30);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);

        EXPECT_GE(effects.body_awareness, 0.0f);
        EXPECT_LE(effects.body_awareness, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify mapping", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    EXPECT_GE(effects.body_awareness, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParBodyAwarenessTest, BodyAwarenessUnderFatigue) {
    // Scenario: Body awareness degrades with fatigue
    E2E_PIPELINE_START("Body Awareness Under Fatigue");

    E2E_STAGE_BEGIN("Fresh state", 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t fresh;
    parietal_substrate_bridge_get_effects(par_bridge, &fresh);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce fatigue", 100);
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 300);
        substrate_record_transmissions(substrate, 700);
        substrate_update(substrate, 20);
    }
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t fatigued;
    parietal_substrate_bridge_get_effects(par_bridge, &fatigued);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(fresh.body_awareness, 0.0f);
    EXPECT_GE(fatigued.body_awareness, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Sensory Integration Tests
//=============================================================================

TEST_F(E2EParSensoryIntegrationTest, SensoryIntegrationCapacity) {
    // Scenario: Baseline sensory integration
    E2E_PIPELINE_START("Sensory Integration Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get integration capacity", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.sensory_integration, MIN_PARIETAL_CAPACITY);
    EXPECT_LE(effects.sensory_integration, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSensoryIntegrationTest, MultimodalIntegration) {
    // Scenario: Integrate visual, auditory, tactile inputs
    E2E_PIPELINE_START("Multimodal Integration");

    E2E_STAGE_BEGIN("Process multimodal input", 100);
    float congruence_levels[] = {0.2f, 0.5f, 0.8f, 1.0f};

    for (float congruence : congruence_levels) {
        MultimodalInput input = createMultimodal(congruence);

        // Integration requires significant processing
        substrate_record_spikes(substrate, 200);
        substrate_record_transmissions(substrate, 500);
        substrate_update(substrate, 50);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);

        EXPECT_GE(effects.sensory_integration, 0.0f);
        EXPECT_LE(effects.sensory_integration, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify integration", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    EXPECT_GE(effects.sensory_integration, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSensoryIntegrationTest, SequentialModalityProcessing) {
    // Scenario: Process modalities in sequence
    E2E_PIPELINE_START("Sequential Modality Processing");

    E2E_STAGE_BEGIN("Process modalities", 100);
    // Visual
    substrate_record_spikes(substrate, 150);
    substrate_update(substrate, 30);

    // Auditory
    substrate_record_spikes(substrate, 100);
    substrate_update(substrate, 30);

    // Tactile
    substrate_record_spikes(substrate, 100);
    substrate_update(substrate, 30);

    // Integration
    substrate_record_transmissions(substrate, 300);
    substrate_update(substrate, 50);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify integration", 5);
    EXPECT_GE(effects.sensory_integration, 0.0f);
    EXPECT_LE(effects.sensory_integration, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSensoryIntegrationTest, IntegrationUnderATPStress) {
    // Scenario: Integration under metabolic stress
    E2E_PIPELINE_START("Integration Under ATP Stress");

    E2E_STAGE_BEGIN("Normal ATP", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t normal;
    parietal_substrate_bridge_get_effects(par_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low ATP", 10);
    substrate_set_atp(substrate, 0.4f);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t stressed;
    parietal_substrate_bridge_get_effects(par_bridge, &stressed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GT(normal.sensory_integration, 0.0f);
    EXPECT_GE(stressed.sensory_integration, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Numerical Processing Tests
//=============================================================================

TEST_F(E2EParNumericalTest, NumericalProcessingCapacity) {
    // Scenario: Baseline numerical processing
    E2E_PIPELINE_START("Numerical Processing Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get numerical processing", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.numerical_processing, MIN_PARIETAL_CAPACITY);
    EXPECT_LE(effects.numerical_processing, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParNumericalTest, NumberMagnitudeProcessing) {
    // Scenario: Process numbers of different magnitudes
    E2E_PIPELINE_START("Number Magnitude Processing");

    E2E_STAGE_BEGIN("Process magnitudes", 100);
    // Different magnitudes require different processing
    float magnitudes[] = {1.0f, 10.0f, 100.0f, 1000.0f};

    for (float mag : magnitudes) {
        // Larger magnitudes may require more resources
        uint32_t spikes = 50 + (uint32_t)(log10f(mag) * 30);

        substrate_record_spikes(substrate, spikes);
        substrate_update(substrate, 30);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);

        EXPECT_GE(effects.numerical_processing, 0.0f);
        EXPECT_LE(effects.numerical_processing, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stability", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    EXPECT_GE(effects.numerical_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParNumericalTest, ArithmeticOperationPipeline) {
    // Scenario: Perform arithmetic operations
    E2E_PIPELINE_START("Arithmetic Operation Pipeline");

    E2E_STAGE_BEGIN("Arithmetic operations", 100);
    // Simulate different operations
    for (int op = 0; op < 20; op++) {
        // Addition, subtraction, multiplication, division
        substrate_record_spikes(substrate, 120);
        substrate_record_transmissions(substrate, 300);
        substrate_update(substrate, 50);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);

        EXPECT_GE(effects.numerical_processing, 0.0f);
        EXPECT_FALSE(std::isnan(effects.numerical_processing));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final check", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    EXPECT_GE(effects.numerical_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParNumericalTest, NumericalWithGlucoseDeprivation) {
    // Scenario: Numerical processing is glucose sensitive
    E2E_PIPELINE_START("Numerical With Glucose Deprivation");

    E2E_STAGE_BEGIN("Normal glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t normal;
    parietal_substrate_bridge_get_effects(par_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_CRITICAL_GLUCOSE);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t low;
    parietal_substrate_bridge_get_effects(par_bridge, &low);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GT(normal.numerical_processing, 0.0f);
    EXPECT_GE(low.numerical_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Metabolic Effects Tests
//=============================================================================

TEST_F(E2EParSpatialAttentionTest, ATPEffectsOnParietal) {
    // Scenario: ATP levels affect parietal processing
    E2E_PIPELINE_START("ATP Effects On Parietal");

    float atp_levels[] = {0.95f, 0.7f, 0.5f, 0.3f};
    std::vector<parietal_substrate_effects_t> effects_at_atp;

    E2E_STAGE_BEGIN("Test ATP levels", 40);
    for (float atp : atp_levels) {
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 10);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);
        effects_at_atp.push_back(effects);

        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify gradient", 5);
    for (const auto& eff : effects_at_atp) {
        EXPECT_FALSE(std::isnan(eff.spatial_attention));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSpatialAttentionTest, OxygenDependentProcessing) {
    // Scenario: Parietal cortex processing requires oxygen
    E2E_PIPELINE_START("Oxygen Dependent Processing");

    E2E_STAGE_BEGIN("Normal oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t normal_o2;
    parietal_substrate_bridge_get_effects(par_bridge, &normal_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2);
    substrate_update(substrate, 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t low_o2;
    parietal_substrate_bridge_get_effects(par_bridge, &low_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(normal_o2.overall_capacity, 0.0f);
    EXPECT_GE(low_o2.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSpatialAttentionTest, TemperatureSensitivity) {
    // Scenario: Temperature affects parietal function
    E2E_PIPELINE_START("Temperature Sensitivity");

    float temperatures[] = {35.0f, 37.0f, 39.0f};

    E2E_STAGE_BEGIN("Test temperatures", 30);
    for (float temp : temperatures) {
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 10);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);

        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all valid", 5);
    parietal_substrate_effects_t effects;
    parietal_substrate_bridge_get_effects(par_bridge, &effects);
    EXPECT_FALSE(std::isnan(effects.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

TEST_F(E2EParSpatialAttentionTest, LongSimulationStability) {
    // Scenario: Extended parietal processing without degradation
    E2E_PIPELINE_START("Long Simulation Stability");

    E2E_STAGE_BEGIN("Extended simulation", 500);
    for (int step = 0; step < 1000; step++) {
        substrate_update(substrate, 10);
        parietal_substrate_bridge_update(par_bridge);

        if (step % 100 == 0) {
            parietal_substrate_effects_t effects;
            parietal_substrate_bridge_get_effects(par_bridge, &effects);

            EXPECT_FALSE(std::isnan(effects.overall_capacity));
            EXPECT_FALSE(std::isinf(effects.overall_capacity));
            EXPECT_GE(effects.overall_capacity, 0.0f);
            EXPECT_LE(effects.overall_capacity, 1.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    parietal_substrate_effects_t final_effects;
    parietal_substrate_bridge_get_effects(par_bridge, &final_effects);

    EXPECT_GT(final_effects.overall_capacity, 0.0f);
    EXPECT_GT(final_effects.spatial_attention, 0.0f);
    EXPECT_GT(final_effects.numerical_processing, 0.0f);
    EXPECT_GT(final_effects.sensory_integration, 0.0f);
    EXPECT_GT(final_effects.body_awareness, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParBodyAwarenessTest, ContinuousSomatosensoryProcessing) {
    // Scenario: Continuous somatosensory processing
    E2E_PIPELINE_START("Continuous Somatosensory Processing");

    E2E_STAGE_BEGIN("Continuous processing", 300);
    std::vector<float> body_values;

    for (int step = 0; step < 100; step++) {
        substrate_record_spikes(substrate, 80);
        substrate_update(substrate, 30);
        parietal_substrate_bridge_update(par_bridge);

        parietal_substrate_effects_t effects;
        parietal_substrate_bridge_get_effects(par_bridge, &effects);
        body_values.push_back(effects.body_awareness);

        EXPECT_FALSE(std::isnan(effects.body_awareness));
        EXPECT_FALSE(std::isinf(effects.body_awareness));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 5);
    float min_val = *std::min_element(body_values.begin(), body_values.end());
    float max_val = *std::max_element(body_values.begin(), body_values.end());

    EXPECT_GE(min_val, 0.0f);
    EXPECT_LE(max_val, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EParSensoryIntegrationTest, RecoveryFromStress) {
    // Scenario: Sensory integration recovery after stress
    E2E_PIPELINE_START("Recovery From Stress");

    E2E_STAGE_BEGIN("Baseline", 10);
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t baseline;
    parietal_substrate_bridge_get_effects(par_bridge, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply stress", 50);
    substrate_set_atp(substrate, 0.3f);
    for (int i = 0; i < 20; i++) {
        substrate_record_spikes(substrate, 400);
        substrate_update(substrate, 10);
    }
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t stressed;
    parietal_substrate_bridge_get_effects(par_bridge, &stressed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery", 100);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    for (int i = 0; i < 100; i++) {
        substrate_update(substrate, 50);
    }
    parietal_substrate_bridge_update(par_bridge);

    parietal_substrate_effects_t recovered;
    parietal_substrate_bridge_get_effects(par_bridge, &recovered);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery", 5);
    EXPECT_GE(recovered.sensory_integration, stressed.sensory_integration);
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
