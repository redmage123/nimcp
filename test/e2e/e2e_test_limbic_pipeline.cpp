/**
 * @file e2e_test_limbic_pipeline.cpp
 * @brief E2E Tests for Limbic System Integration Pipeline
 *
 * WHAT: Complete end-to-end tests for integrated limbic processing
 * WHY:  Verify cingulate-insula-amygdala circuit works correctly together
 * HOW:  Simulate complete emotional-interoceptive-conflict processing
 *
 * TEST SCENARIOS:
 * 1. CingulatePipeline - Error detection, conflict resolution, emotional regulation
 * 2. InsulaPipeline - Interoception, emotional awareness, disgust, empathy
 * 3. LimbicIntegrationPipeline - Cingulate-insula-amygdala coordination
 * 4. MetabolicEffectsPipeline - ATP/fatigue effects on limbic function
 * 5. ConflictEmotionPipeline - Error monitoring with emotional integration
 * 6. InteroceptiveEmotionPipeline - Body signals affecting emotional processing
 * 7. LongTermStabilityPipeline - Extended operation without degradation
 *
 * BIOLOGICAL ANALOGY:
 * - Anterior cingulate cortex (ACC) for error detection and conflict
 * - Insula for interoception and emotional awareness
 * - Amygdala for threat detection and fear processing
 * - ACC-insula-amygdala circuit for emotional salience
 * - Metabolic effects on limbic sensitivity
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 * @version 1.0.0
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
#include "core/cingulate/nimcp_cingulate_substrate_bridge.h"
#include "core/insula/nimcp_insula_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Processing parameters
constexpr uint32_t PROCESSING_ITERATIONS = 100;
constexpr uint32_t STABILITY_ITERATIONS = 1000;
constexpr float CONFLICT_THRESHOLD = 0.5f;
constexpr float ERROR_THRESHOLD = 0.6f;

// Timing thresholds (milliseconds)
constexpr double MAX_CINGULATE_PROCESSING_MS = 50.0;
constexpr double MAX_INSULA_PROCESSING_MS = 50.0;
constexpr double MAX_INTEGRATION_PROCESSING_MS = 100.0;
constexpr double MAX_METABOLIC_UPDATE_MS = 20.0;

// Capacity thresholds
constexpr float MIN_CAPACITY_OPTIMAL = 0.9f;
constexpr float MIN_CAPACITY_STRESSED = 0.3f;
constexpr float MAX_CAPACITY_DEGRADATION = 0.1f;

//=============================================================================
// Helper Structures
//=============================================================================

/**
 * @brief Simulated cingulate context for testing
 */
struct CingulateContext {
    float conflict_level;
    float error_signal;
    float emotional_input;
    float pain_input;
};

/**
 * @brief Simulated insula context for testing
 */
struct InsulaContext {
    float interoceptive_signal;
    float emotional_input;
    float disgust_stimulus;
    float empathy_stimulus;
};

/**
 * @brief Limbic integration state
 */
struct LimbicState {
    cingulate_substrate_effects_t cingulate_effects;
    insula_substrate_effects_t insula_effects;
    float overall_limbic_capacity;
    float emotional_salience;
};

//=============================================================================
// Test Fixture
//=============================================================================

class LimbicPipelineTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate_;
    cingulate_substrate_bridge_t* cingulate_bridge_;
    insula_substrate_bridge_t* insula_bridge_;

    void SetUp() override {
        // Initialize neural substrate with optimal conditions
        substrate_config_t substrate_config;
        substrate_default_config(&substrate_config);
        substrate_config.initial_atp = 1.0f;
        substrate_config.initial_glucose = 1.0f;
        substrate_config.initial_o2 = 1.0f;
        substrate_config.initial_temperature = 37.0f;
        substrate_ = substrate_create(&substrate_config);

        // Create cingulate bridge
        cingulate_substrate_config_t cingulate_config = cingulate_substrate_default_config();
        cingulate_bridge_ = cingulate_substrate_bridge_create(nullptr, substrate_, &cingulate_config);

        // Create insula bridge
        insula_substrate_config_t insula_config = insula_substrate_default_config();
        insula_bridge_ = insula_substrate_bridge_create(nullptr, substrate_, &insula_config);
    }

    void TearDown() override {
        if (cingulate_bridge_) {
            cingulate_substrate_bridge_destroy(cingulate_bridge_);
            cingulate_bridge_ = nullptr;
        }
        if (insula_bridge_) {
            insula_substrate_bridge_destroy(insula_bridge_);
            insula_bridge_ = nullptr;
        }
        if (substrate_) {
            substrate_destroy(substrate_);
            substrate_ = nullptr;
        }
    }

    void setMetabolicState(float atp, float glucose, float oxygen) {
        if (substrate_) {
            substrate_set_atp(substrate_, atp);
            substrate_set_glucose(substrate_, glucose);
            substrate_set_oxygen(substrate_, oxygen);
        }
    }

    LimbicState getLimbicState() {
        LimbicState state;
        memset(&state, 0, sizeof(state));

        if (cingulate_bridge_) {
            cingulate_substrate_bridge_get_effects(cingulate_bridge_, &state.cingulate_effects);
        }
        if (insula_bridge_) {
            insula_substrate_bridge_get_effects(insula_bridge_, &state.insula_effects);
        }

        // Compute combined limbic capacity
        state.overall_limbic_capacity = (state.cingulate_effects.overall_capacity +
                                         state.insula_effects.overall_capacity) / 2.0f;

        // Emotional salience combines conflict detection and interoception
        state.emotional_salience = (state.cingulate_effects.error_detection +
                                    state.insula_effects.emotional_awareness) / 2.0f;

        return state;
    }
};

//=============================================================================
// Cingulate Processing Tests
//=============================================================================

/**
 * @test Verify cingulate error detection pipeline under optimal conditions
 */
TEST_F(LimbicPipelineTest, CingulateErrorDetectionOptimal) {
    E2E_PIPELINE_START("Cingulate Error Detection Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal Substrate", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    ASSERT_NE(nullptr, cingulate_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Cingulate Bridge", MAX_CINGULATE_PROCESSING_MS);
    int result = cingulate_substrate_bridge_update(cingulate_bridge_);
    EXPECT_EQ(0, result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Error Detection Capacity", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_effects_t effects;
    result = cingulate_substrate_bridge_get_effects(cingulate_bridge_, &effects);
    EXPECT_EQ(0, result);
    EXPECT_GE(effects.error_detection, MIN_CAPACITY_OPTIMAL);
    EXPECT_FALSE(std::isnan(effects.error_detection));
    EXPECT_FALSE(std::isinf(effects.error_detection));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Conflict Resolution", MAX_CINGULATE_PROCESSING_MS);
    EXPECT_GE(effects.conflict_resolution, MIN_CAPACITY_OPTIMAL);
    EXPECT_FALSE(std::isnan(effects.conflict_resolution));
    EXPECT_FALSE(std::isinf(effects.conflict_resolution));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify cingulate emotional regulation under optimal conditions
 */
TEST_F(LimbicPipelineTest, CingulateEmotionalRegulationOptimal) {
    E2E_PIPELINE_START("Cingulate Emotional Regulation Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal Substrate", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update and Verify Emotional Regulation", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    cingulate_substrate_effects_t effects;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &effects);
    EXPECT_GE(effects.emotional_regulation, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.emotional_regulation, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Pain Processing", MAX_CINGULATE_PROCESSING_MS);
    EXPECT_GE(effects.pain_processing, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.pain_processing, 1.0f);
    EXPECT_FALSE(std::isnan(effects.pain_processing));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify cingulate degradation under metabolic stress
 */
TEST_F(LimbicPipelineTest, CingulateMetabolicStress) {
    E2E_PIPELINE_START("Cingulate Metabolic Stress Pipeline");

    E2E_STAGE_BEGIN("Establish Baseline", MAX_CINGULATE_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    cingulate_substrate_effects_t baseline;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply ATP Depletion", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.3f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Capacity Reduction", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_effects_t stressed;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &stressed);
    EXPECT_LT(stressed.error_detection, baseline.error_detection);
    EXPECT_LT(stressed.conflict_resolution, baseline.conflict_resolution);
    EXPECT_LT(stressed.overall_capacity, baseline.overall_capacity);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Graceful Degradation", MAX_CINGULATE_PROCESSING_MS);
    // Should not drop to zero
    EXPECT_GT(stressed.error_detection, 0.0f);
    EXPECT_GT(stressed.conflict_resolution, 0.0f);
    EXPECT_FALSE(std::isnan(stressed.error_detection));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Insula Processing Tests
//=============================================================================

/**
 * @test Verify insula interoceptive processing under optimal conditions
 */
TEST_F(LimbicPipelineTest, InsulaInteroceptionOptimal) {
    E2E_PIPELINE_START("Insula Interoception Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal Substrate", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    ASSERT_NE(nullptr, insula_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Insula Bridge", MAX_INSULA_PROCESSING_MS);
    int result = insula_substrate_bridge_update(insula_bridge_);
    EXPECT_EQ(0, result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Interoceptive Accuracy", MAX_INSULA_PROCESSING_MS);
    insula_substrate_effects_t effects;
    result = insula_substrate_bridge_get_effects(insula_bridge_, &effects);
    EXPECT_EQ(0, result);
    EXPECT_GE(effects.interoceptive_accuracy, MIN_CAPACITY_OPTIMAL);
    EXPECT_FALSE(std::isnan(effects.interoceptive_accuracy));
    EXPECT_FALSE(std::isinf(effects.interoceptive_accuracy));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Emotional Awareness", MAX_INSULA_PROCESSING_MS);
    EXPECT_GE(effects.emotional_awareness, MIN_CAPACITY_OPTIMAL);
    EXPECT_FALSE(std::isnan(effects.emotional_awareness));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify insula disgust and empathy processing
 */
TEST_F(LimbicPipelineTest, InsulaDisgustEmpathyOptimal) {
    E2E_PIPELINE_START("Insula Disgust/Empathy Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal Substrate", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update and Verify Disgust Sensitivity", MAX_INSULA_PROCESSING_MS);
    insula_substrate_bridge_update(insula_bridge_);
    insula_substrate_effects_t effects;
    insula_substrate_bridge_get_effects(insula_bridge_, &effects);
    EXPECT_GE(effects.disgust_sensitivity, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.disgust_sensitivity, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Empathic Response", MAX_INSULA_PROCESSING_MS);
    EXPECT_GE(effects.empathic_response, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.empathic_response, 1.0f);
    EXPECT_FALSE(std::isnan(effects.empathic_response));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify insula degradation under metabolic stress
 */
TEST_F(LimbicPipelineTest, InsulaMetabolicStress) {
    E2E_PIPELINE_START("Insula Metabolic Stress Pipeline");

    E2E_STAGE_BEGIN("Establish Baseline", MAX_INSULA_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    insula_substrate_bridge_update(insula_bridge_);
    insula_substrate_effects_t baseline;
    insula_substrate_bridge_get_effects(insula_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Glucose Depletion", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 0.3f, 1.0f);
    insula_substrate_bridge_update(insula_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Capacity Reduction", MAX_INSULA_PROCESSING_MS);
    insula_substrate_effects_t stressed;
    insula_substrate_bridge_get_effects(insula_bridge_, &stressed);
    EXPECT_LT(stressed.interoceptive_accuracy, baseline.interoceptive_accuracy);
    EXPECT_LT(stressed.emotional_awareness, baseline.emotional_awareness);
    EXPECT_LT(stressed.overall_capacity, baseline.overall_capacity);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Graceful Degradation", MAX_INSULA_PROCESSING_MS);
    EXPECT_GT(stressed.interoceptive_accuracy, 0.0f);
    EXPECT_GT(stressed.emotional_awareness, 0.0f);
    EXPECT_FALSE(std::isnan(stressed.interoceptive_accuracy));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Limbic Integration Tests
//=============================================================================

/**
 * @test Verify cingulate-insula integration under optimal conditions
 */
TEST_F(LimbicPipelineTest, LimbicIntegrationOptimal) {
    E2E_PIPELINE_START("Limbic Integration Pipeline");

    E2E_STAGE_BEGIN("Initialize Both Systems", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    ASSERT_NE(nullptr, cingulate_bridge_);
    ASSERT_NE(nullptr, insula_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Cingulate", MAX_CINGULATE_PROCESSING_MS);
    int cingulate_result = cingulate_substrate_bridge_update(cingulate_bridge_);
    EXPECT_EQ(0, cingulate_result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Insula", MAX_INSULA_PROCESSING_MS);
    int insula_result = insula_substrate_bridge_update(insula_bridge_);
    EXPECT_EQ(0, insula_result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Integrated Limbic State", MAX_INTEGRATION_PROCESSING_MS);
    LimbicState state = getLimbicState();
    EXPECT_GE(state.overall_limbic_capacity, MIN_CAPACITY_OPTIMAL);
    EXPECT_GE(state.emotional_salience, MIN_CAPACITY_OPTIMAL);
    EXPECT_FALSE(std::isnan(state.overall_limbic_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify conflict-emotion integration
 */
TEST_F(LimbicPipelineTest, ConflictEmotionIntegration) {
    E2E_PIPELINE_START("Conflict-Emotion Integration Pipeline");

    E2E_STAGE_BEGIN("Setup Integration", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process Conflict Detection", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    cingulate_substrate_effects_t cingulate;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &cingulate);
    EXPECT_GE(cingulate.error_detection, MIN_CAPACITY_OPTIMAL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process Emotional Awareness", MAX_INSULA_PROCESSING_MS);
    insula_substrate_bridge_update(insula_bridge_);
    insula_substrate_effects_t insula;
    insula_substrate_bridge_get_effects(insula_bridge_, &insula);
    EXPECT_GE(insula.emotional_awareness, MIN_CAPACITY_OPTIMAL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Correlation", MAX_INTEGRATION_PROCESSING_MS);
    // Error detection and emotional awareness should be correlated under same metabolic state
    float diff = std::abs(cingulate.error_detection - insula.emotional_awareness);
    EXPECT_LT(diff, 0.2f); // Should be within 20% of each other
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify integrated limbic response to metabolic stress
 */
TEST_F(LimbicPipelineTest, IntegratedMetabolicStress) {
    E2E_PIPELINE_START("Integrated Metabolic Stress Pipeline");

    E2E_STAGE_BEGIN("Establish Baseline", MAX_INTEGRATION_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    insula_substrate_bridge_update(insula_bridge_);
    LimbicState baseline = getLimbicState();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Combined Stress", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.4f, 0.4f, 0.8f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    insula_substrate_bridge_update(insula_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Coordinated Degradation", MAX_INTEGRATION_PROCESSING_MS);
    LimbicState stressed = getLimbicState();
    EXPECT_LT(stressed.overall_limbic_capacity, baseline.overall_limbic_capacity);
    EXPECT_LT(stressed.cingulate_effects.overall_capacity, baseline.cingulate_effects.overall_capacity);
    EXPECT_LT(stressed.insula_effects.overall_capacity, baseline.insula_effects.overall_capacity);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Recovery", MAX_INTEGRATION_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    insula_substrate_bridge_update(insula_bridge_);
    LimbicState recovered = getLimbicState();
    EXPECT_GT(recovered.overall_limbic_capacity, stressed.overall_limbic_capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Multi-iteration Processing Tests
//=============================================================================

/**
 * @test Verify stable cingulate processing over multiple iterations
 */
TEST_F(LimbicPipelineTest, CingulateMultiIterationStability) {
    E2E_PIPELINE_START("Cingulate Multi-Iteration Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    cingulate_substrate_effects_t initial;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run Iterations", MAX_CINGULATE_PROCESSING_MS * PROCESSING_ITERATIONS);
    for (uint32_t i = 0; i < PROCESSING_ITERATIONS; ++i) {
        cingulate_substrate_bridge_update(cingulate_bridge_);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Stability", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_effects_t final_state;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &final_state);

    float degradation = initial.overall_capacity - final_state.overall_capacity;
    EXPECT_LT(degradation, MAX_CAPACITY_DEGRADATION);
    EXPECT_FALSE(std::isnan(final_state.overall_capacity));
    EXPECT_FALSE(std::isinf(final_state.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify stable insula processing over multiple iterations
 */
TEST_F(LimbicPipelineTest, InsulaMultiIterationStability) {
    E2E_PIPELINE_START("Insula Multi-Iteration Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    insula_substrate_bridge_update(insula_bridge_);
    insula_substrate_effects_t initial;
    insula_substrate_bridge_get_effects(insula_bridge_, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run Iterations", MAX_INSULA_PROCESSING_MS * PROCESSING_ITERATIONS);
    for (uint32_t i = 0; i < PROCESSING_ITERATIONS; ++i) {
        insula_substrate_bridge_update(insula_bridge_);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Stability", MAX_INSULA_PROCESSING_MS);
    insula_substrate_effects_t final_state;
    insula_substrate_bridge_get_effects(insula_bridge_, &final_state);

    float degradation = initial.overall_capacity - final_state.overall_capacity;
    EXPECT_LT(degradation, MAX_CAPACITY_DEGRADATION);
    EXPECT_FALSE(std::isnan(final_state.overall_capacity));
    EXPECT_FALSE(std::isinf(final_state.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Oxygen Stress Tests
//=============================================================================

/**
 * @test Verify cingulate response to hypoxia
 */
TEST_F(LimbicPipelineTest, CingulateHypoxiaResponse) {
    E2E_PIPELINE_START("Cingulate Hypoxia Pipeline");

    E2E_STAGE_BEGIN("Baseline Measurement", MAX_CINGULATE_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    cingulate_substrate_effects_t baseline;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Hypoxia", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 0.3f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Impairment", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_effects_t hypoxic;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &hypoxic);
    EXPECT_LT(hypoxic.error_detection, baseline.error_detection);
    EXPECT_LT(hypoxic.conflict_resolution, baseline.conflict_resolution);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify insula response to hypoxia
 */
TEST_F(LimbicPipelineTest, InsulaHypoxiaResponse) {
    E2E_PIPELINE_START("Insula Hypoxia Pipeline");

    E2E_STAGE_BEGIN("Baseline Measurement", MAX_INSULA_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    insula_substrate_bridge_update(insula_bridge_);
    insula_substrate_effects_t baseline;
    insula_substrate_bridge_get_effects(insula_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Hypoxia", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 0.3f);
    insula_substrate_bridge_update(insula_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Impairment", MAX_INSULA_PROCESSING_MS);
    insula_substrate_effects_t hypoxic;
    insula_substrate_bridge_get_effects(insula_bridge_, &hypoxic);
    EXPECT_LT(hypoxic.interoceptive_accuracy, baseline.interoceptive_accuracy);
    EXPECT_LT(hypoxic.emotional_awareness, baseline.emotional_awareness);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Extreme Stress Tests
//=============================================================================

/**
 * @test Verify cingulate survives severe metabolic stress
 */
TEST_F(LimbicPipelineTest, CingulateSevereStress) {
    E2E_PIPELINE_START("Cingulate Severe Stress Pipeline");

    E2E_STAGE_BEGIN("Apply Severe Stress", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.1f, 0.1f, 0.5f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Under Stress", MAX_CINGULATE_PROCESSING_MS);
    int result = cingulate_substrate_bridge_update(cingulate_bridge_);
    EXPECT_EQ(0, result); // Should not crash
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Minimal Function", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_effects_t effects;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    EXPECT_FALSE(std::isnan(effects.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify insula survives severe metabolic stress
 */
TEST_F(LimbicPipelineTest, InsulaSevereStress) {
    E2E_PIPELINE_START("Insula Severe Stress Pipeline");

    E2E_STAGE_BEGIN("Apply Severe Stress", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.1f, 0.1f, 0.5f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Under Stress", MAX_INSULA_PROCESSING_MS);
    int result = insula_substrate_bridge_update(insula_bridge_);
    EXPECT_EQ(0, result); // Should not crash
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Minimal Function", MAX_INSULA_PROCESSING_MS);
    insula_substrate_effects_t effects;
    insula_substrate_bridge_get_effects(insula_bridge_, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    EXPECT_FALSE(std::isnan(effects.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

/**
 * @test Verify limbic system long-term stability
 */
TEST_F(LimbicPipelineTest, LongTermStability) {
    E2E_PIPELINE_START("Limbic Long-Term Stability Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.9f, 0.9f, 0.95f); // Slightly suboptimal but sustainable
    cingulate_substrate_bridge_update(cingulate_bridge_);
    insula_substrate_bridge_update(insula_bridge_);
    LimbicState initial = getLimbicState();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Extended Operation", MAX_INTEGRATION_PROCESSING_MS * 10);
    for (uint32_t i = 0; i < STABILITY_ITERATIONS; ++i) {
        cingulate_substrate_bridge_update(cingulate_bridge_);
        insula_substrate_bridge_update(insula_bridge_);

        // Periodic validation
        if (i % 100 == 0) {
            LimbicState current = getLimbicState();
            EXPECT_FALSE(std::isnan(current.overall_limbic_capacity));
            EXPECT_FALSE(std::isinf(current.overall_limbic_capacity));
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Final State", MAX_INTEGRATION_PROCESSING_MS);
    LimbicState final_state = getLimbicState();
    float drift = std::abs(final_state.overall_limbic_capacity - initial.overall_limbic_capacity);
    EXPECT_LT(drift, MAX_CAPACITY_DEGRADATION * 2); // Allow some tolerance for long runs

    // All values should remain valid
    EXPECT_GE(final_state.cingulate_effects.error_detection, 0.0f);
    EXPECT_LE(final_state.cingulate_effects.error_detection, 1.0f);
    EXPECT_GE(final_state.insula_effects.interoceptive_accuracy, 0.0f);
    EXPECT_LE(final_state.insula_effects.interoceptive_accuracy, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify recovery from prolonged stress
 */
TEST_F(LimbicPipelineTest, ProlongedStressRecovery) {
    E2E_PIPELINE_START("Prolonged Stress Recovery Pipeline");

    E2E_STAGE_BEGIN("Baseline", MAX_INTEGRATION_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    insula_substrate_bridge_update(insula_bridge_);
    LimbicState baseline = getLimbicState();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Prolonged Stress Period", MAX_INTEGRATION_PROCESSING_MS * 5);
    setMetabolicState(0.4f, 0.4f, 0.7f);
    for (uint32_t i = 0; i < 500; ++i) {
        cingulate_substrate_bridge_update(cingulate_bridge_);
        insula_substrate_bridge_update(insula_bridge_);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery Period", MAX_INTEGRATION_PROCESSING_MS * 5);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    for (uint32_t i = 0; i < 100; ++i) {
        cingulate_substrate_bridge_update(cingulate_bridge_);
        insula_substrate_bridge_update(insula_bridge_);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Recovery", MAX_INTEGRATION_PROCESSING_MS);
    LimbicState recovered = getLimbicState();
    // Should recover to near baseline
    float recovery_ratio = recovered.overall_limbic_capacity / baseline.overall_limbic_capacity;
    EXPECT_GT(recovery_ratio, 0.85f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Apply Effects Tests
//=============================================================================

/**
 * @test Verify cingulate apply effects pipeline
 */
TEST_F(LimbicPipelineTest, CingulateApplyEffects) {
    E2E_PIPELINE_START("Cingulate Apply Effects Pipeline");

    E2E_STAGE_BEGIN("Update Bridge", MAX_CINGULATE_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    cingulate_substrate_bridge_update(cingulate_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Effects", MAX_CINGULATE_PROCESSING_MS);
    int result = cingulate_substrate_bridge_apply_effects(cingulate_bridge_);
    EXPECT_EQ(0, result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify State Consistency", MAX_CINGULATE_PROCESSING_MS);
    cingulate_substrate_effects_t effects;
    cingulate_substrate_bridge_get_effects(cingulate_bridge_, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify insula apply effects pipeline
 */
TEST_F(LimbicPipelineTest, InsulaApplyEffects) {
    E2E_PIPELINE_START("Insula Apply Effects Pipeline");

    E2E_STAGE_BEGIN("Update Bridge", MAX_INSULA_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    insula_substrate_bridge_update(insula_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Effects", MAX_INSULA_PROCESSING_MS);
    int result = insula_substrate_bridge_apply_effects(insula_bridge_);
    EXPECT_EQ(0, result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify State Consistency", MAX_INSULA_PROCESSING_MS);
    insula_substrate_effects_t effects;
    insula_substrate_bridge_get_effects(insula_bridge_, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Fluctuating Metabolic State Tests
//=============================================================================

/**
 * @test Verify limbic system handles fluctuating metabolic state
 */
TEST_F(LimbicPipelineTest, FluctuatingMetabolicState) {
    E2E_PIPELINE_START("Fluctuating Metabolic State Pipeline");

    std::vector<float> capacity_history;

    E2E_STAGE_BEGIN("Simulate Fluctuations", MAX_INTEGRATION_PROCESSING_MS * 5);
    for (uint32_t i = 0; i < PROCESSING_ITERATIONS; ++i) {
        // Sinusoidal metabolic fluctuation
        float phase = static_cast<float>(i) * 0.1f;
        float atp = 0.6f + 0.3f * std::sin(phase);
        float glucose = 0.6f + 0.3f * std::sin(phase + 1.0f);
        float oxygen = 0.8f + 0.15f * std::sin(phase + 2.0f);

        setMetabolicState(atp, glucose, oxygen);
        cingulate_substrate_bridge_update(cingulate_bridge_);
        insula_substrate_bridge_update(insula_bridge_);

        LimbicState state = getLimbicState();
        capacity_history.push_back(state.overall_limbic_capacity);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Stable Response", MAX_INTEGRATION_PROCESSING_MS);
    // All values should be valid
    for (float capacity : capacity_history) {
        EXPECT_GE(capacity, 0.0f);
        EXPECT_LE(capacity, 1.0f);
        EXPECT_FALSE(std::isnan(capacity));
    }

    // Variance should be bounded
    float mean = std::accumulate(capacity_history.begin(), capacity_history.end(), 0.0f) /
                 capacity_history.size();
    EXPECT_GT(mean, 0.3f);
    EXPECT_LT(mean, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
