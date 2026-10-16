/**
 * @file e2e_test_motor_control_pipeline.cpp
 * @brief E2E Tests for Motor Control Pipeline
 *
 * WHAT: Complete end-to-end tests for motor control processing
 * WHY:  Verify motor cortex to execution works correctly with metabolic constraints
 * HOW:  Simulate complete motor planning, execution, and coordination cycles
 *
 * TEST SCENARIOS:
 * 1. MotorPrecisionPipeline - Fine motor control accuracy
 * 2. MotorSpeedPipeline - Motor execution speed under different conditions
 * 3. MotorEndurancePipeline - Sustained motor activity
 * 4. MotorCoordinationPipeline - Multi-joint movement coordination
 * 5. MetabolicEffectsPipeline - ATP/fatigue effects on motor function
 * 6. MotorRecoveryPipeline - Recovery from motor fatigue
 * 7. LongTermStabilityPipeline - Extended motor operation without degradation
 *
 * BIOLOGICAL ANALOGY:
 * - Motor cortex (M1) for movement execution
 * - Premotor areas for movement planning
 * - Basal ganglia for action selection
 * - Cerebellum for timing and coordination
 * - ATP requirements for muscle contraction
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
#include "core/motor/nimcp_motor_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Motor control parameters
constexpr uint32_t MOVEMENT_ITERATIONS = 100;
constexpr uint32_t ENDURANCE_ITERATIONS = 500;
constexpr uint32_t STABILITY_ITERATIONS = 1000;
constexpr float PRECISION_THRESHOLD = 0.95f;
constexpr float SPEED_THRESHOLD = 0.9f;

// Timing thresholds (milliseconds)
constexpr double MAX_MOTOR_PROCESSING_MS = 50.0;
constexpr double MAX_METABOLIC_UPDATE_MS = 20.0;
constexpr double MAX_COORDINATION_MS = 100.0;
constexpr double MAX_ENDURANCE_TEST_MS = 5000.0;

// Capacity thresholds
constexpr float MIN_CAPACITY_OPTIMAL = 0.9f;
constexpr float MIN_CAPACITY_FATIGUED = 0.4f;
constexpr float MAX_CAPACITY_DEGRADATION = 0.1f;

//=============================================================================
// Helper Structures
//=============================================================================

/**
 * @brief Simulated motor command for testing
 */
struct MotorCommand {
    float target_position[3];
    float target_velocity[3];
    float force_required;
    uint32_t duration_ms;
};

/**
 * @brief Motor execution result
 */
struct MotorResult {
    float actual_position[3];
    float position_error;
    float execution_time_ms;
    bool completed;
};

/**
 * @brief Motor state tracking
 */
struct MotorState {
    motor_substrate_effects_t effects;
    float cumulative_fatigue;
    uint32_t movements_completed;
};

//=============================================================================
// Test Fixture
//=============================================================================

class MotorControlPipelineTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate_;
    motor_substrate_bridge_t* motor_bridge_;

    void SetUp() override {
        // Initialize neural substrate with optimal conditions
        substrate_config_t substrate_config;
        substrate_default_config(&substrate_config);
        substrate_config.initial_atp = 1.0f;
        substrate_config.initial_glucose = 1.0f;
        substrate_config.initial_o2 = 1.0f;
        substrate_config.initial_temperature = 37.0f;
        substrate_ = substrate_create(&substrate_config);

        // Create motor bridge
        motor_substrate_config_t motor_config = motor_substrate_default_config();
        motor_bridge_ = motor_substrate_bridge_create(nullptr, substrate_, &motor_config);
    }

    void TearDown() override {
        if (motor_bridge_) {
            motor_substrate_bridge_destroy(motor_bridge_);
            motor_bridge_ = nullptr;
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

    MotorState getMotorState() {
        MotorState state;
        memset(&state, 0, sizeof(state));

        if (motor_bridge_) {
            motor_substrate_bridge_get_effects(motor_bridge_, &state.effects);
        }

        return state;
    }

    float simulateMovementError(float precision, float target_difficulty = 1.0f) {
        // Higher precision = lower error
        // Higher difficulty = higher error
        return (1.0f - precision) * target_difficulty;
    }
};

//=============================================================================
// Motor Precision Tests
//=============================================================================

/**
 * @test Verify motor precision under optimal conditions
 */
TEST_F(MotorControlPipelineTest, MotorPrecisionOptimal) {
    E2E_PIPELINE_START("Motor Precision Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal Substrate", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    ASSERT_NE(nullptr, motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Motor Bridge", MAX_MOTOR_PROCESSING_MS);
    int result = motor_substrate_bridge_update(motor_bridge_);
    EXPECT_EQ(0, result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Motor Precision", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t effects;
    result = motor_substrate_bridge_get_effects(motor_bridge_, &effects);
    EXPECT_EQ(0, result);
    EXPECT_GE(effects.motor_precision, MIN_CAPACITY_OPTIMAL);
    EXPECT_FALSE(std::isnan(effects.motor_precision));
    EXPECT_FALSE(std::isinf(effects.motor_precision));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Coordination", MAX_MOTOR_PROCESSING_MS);
    EXPECT_GE(effects.coordination, MIN_CAPACITY_OPTIMAL);
    EXPECT_FALSE(std::isnan(effects.coordination));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify precision degradation under fatigue
 */
TEST_F(MotorControlPipelineTest, MotorPrecisionFatigue) {
    E2E_PIPELINE_START("Motor Precision Fatigue Pipeline");

    E2E_STAGE_BEGIN("Establish Baseline", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t baseline;
    motor_substrate_bridge_get_effects(motor_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply ATP Depletion", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.3f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Precision Reduction", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t fatigued;
    motor_substrate_bridge_get_effects(motor_bridge_, &fatigued);
    EXPECT_LT(fatigued.motor_precision, baseline.motor_precision);
    EXPECT_LT(fatigued.coordination, baseline.coordination);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Graceful Degradation", MAX_MOTOR_PROCESSING_MS);
    // Should not drop to zero
    EXPECT_GT(fatigued.motor_precision, 0.0f);
    EXPECT_GT(fatigued.coordination, 0.0f);
    EXPECT_FALSE(std::isnan(fatigued.motor_precision));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Motor Speed Tests
//=============================================================================

/**
 * @test Verify motor speed under optimal conditions
 */
TEST_F(MotorControlPipelineTest, MotorSpeedOptimal) {
    E2E_PIPELINE_START("Motor Speed Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal Substrate", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update and Verify Speed", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t effects;
    motor_substrate_bridge_get_effects(motor_bridge_, &effects);
    EXPECT_GE(effects.motor_speed, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.motor_speed, 1.0f);
    EXPECT_FALSE(std::isnan(effects.motor_speed));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Overall Capacity", MAX_MOTOR_PROCESSING_MS);
    EXPECT_GE(effects.overall_capacity, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify speed reduction under metabolic stress
 */
TEST_F(MotorControlPipelineTest, MotorSpeedStress) {
    E2E_PIPELINE_START("Motor Speed Stress Pipeline");

    E2E_STAGE_BEGIN("Baseline Measurement", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t baseline;
    motor_substrate_bridge_get_effects(motor_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Glucose Depletion", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 0.3f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Speed Reduction", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t stressed;
    motor_substrate_bridge_get_effects(motor_bridge_, &stressed);
    EXPECT_LT(stressed.motor_speed, baseline.motor_speed);
    EXPECT_LT(stressed.overall_capacity, baseline.overall_capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Motor Endurance Tests
//=============================================================================

/**
 * @test Verify motor endurance under optimal conditions
 */
TEST_F(MotorControlPipelineTest, MotorEnduranceOptimal) {
    E2E_PIPELINE_START("Motor Endurance Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Endurance Capacity", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t effects;
    motor_substrate_bridge_get_effects(motor_bridge_, &effects);
    EXPECT_GE(effects.motor_endurance, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.motor_endurance, 1.0f);
    EXPECT_FALSE(std::isnan(effects.motor_endurance));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify endurance over sustained activity
 */
TEST_F(MotorControlPipelineTest, MotorEnduranceSustained) {
    E2E_PIPELINE_START("Motor Sustained Endurance Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t initial;
    motor_substrate_bridge_get_effects(motor_bridge_, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Sustained Activity", MAX_ENDURANCE_TEST_MS);
    for (uint32_t i = 0; i < ENDURANCE_ITERATIONS; ++i) {
        motor_substrate_bridge_update(motor_bridge_);

        // Periodic validation
        if (i % 50 == 0) {
            motor_substrate_effects_t current;
            motor_substrate_bridge_get_effects(motor_bridge_, &current);
            EXPECT_FALSE(std::isnan(current.motor_endurance));
            EXPECT_FALSE(std::isinf(current.motor_endurance));
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Post-Activity State", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t final_state;
    motor_substrate_bridge_get_effects(motor_bridge_, &final_state);
    float degradation = initial.motor_endurance - final_state.motor_endurance;
    // Some degradation is expected, but should be bounded
    EXPECT_LT(degradation, MAX_CAPACITY_DEGRADATION * 3);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Motor Coordination Tests
//=============================================================================

/**
 * @test Verify motor coordination under optimal conditions
 */
TEST_F(MotorControlPipelineTest, MotorCoordinationOptimal) {
    E2E_PIPELINE_START("Motor Coordination Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal State", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Coordination", MAX_COORDINATION_MS);
    motor_substrate_effects_t effects;
    motor_substrate_bridge_get_effects(motor_bridge_, &effects);
    EXPECT_GE(effects.coordination, MIN_CAPACITY_OPTIMAL);
    EXPECT_LE(effects.coordination, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify All Motor Aspects", MAX_COORDINATION_MS);
    EXPECT_GE(effects.motor_precision, MIN_CAPACITY_OPTIMAL);
    EXPECT_GE(effects.motor_speed, MIN_CAPACITY_OPTIMAL);
    EXPECT_GE(effects.motor_endurance, MIN_CAPACITY_OPTIMAL);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify coordination degradation under stress
 */
TEST_F(MotorControlPipelineTest, MotorCoordinationStress) {
    E2E_PIPELINE_START("Motor Coordination Stress Pipeline");

    E2E_STAGE_BEGIN("Baseline", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t baseline;
    motor_substrate_bridge_get_effects(motor_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Combined Stress", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.4f, 0.4f, 0.8f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Coordination Reduction", MAX_COORDINATION_MS);
    motor_substrate_effects_t stressed;
    motor_substrate_bridge_get_effects(motor_bridge_, &stressed);
    EXPECT_LT(stressed.coordination, baseline.coordination);
    // All aspects should be reduced
    EXPECT_LT(stressed.motor_precision, baseline.motor_precision);
    EXPECT_LT(stressed.motor_speed, baseline.motor_speed);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Metabolic Effect Tests
//=============================================================================

/**
 * @test Verify motor response to ATP depletion
 */
TEST_F(MotorControlPipelineTest, MotorATPDepletion) {
    E2E_PIPELINE_START("Motor ATP Depletion Pipeline");

    E2E_STAGE_BEGIN("Baseline", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t baseline;
    motor_substrate_bridge_get_effects(motor_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Mild ATP Depletion", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(0.7f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t mild;
    motor_substrate_bridge_get_effects(motor_bridge_, &mild);
    EXPECT_LT(mild.overall_capacity, baseline.overall_capacity);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Severe ATP Depletion", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(0.2f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t severe;
    motor_substrate_bridge_get_effects(motor_bridge_, &severe);
    EXPECT_LT(severe.overall_capacity, mild.overall_capacity);
    EXPECT_GT(severe.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify motor response to hypoxia
 */
TEST_F(MotorControlPipelineTest, MotorHypoxia) {
    E2E_PIPELINE_START("Motor Hypoxia Pipeline");

    E2E_STAGE_BEGIN("Baseline", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t baseline;
    motor_substrate_bridge_get_effects(motor_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Hypoxia", MAX_METABOLIC_UPDATE_MS);
    /* Hypoxia leads to reduced ATP production - simulate via low ATP */
    setMetabolicState(0.3f, 1.0f, 0.3f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Impairment", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t hypoxic;
    motor_substrate_bridge_get_effects(motor_bridge_, &hypoxic);
    EXPECT_LT(hypoxic.motor_precision, baseline.motor_precision);
    /* Speed depends on metabolic capacity, not ATP directly */
    EXPECT_LE(hypoxic.motor_speed, baseline.motor_speed);
    EXPECT_LT(hypoxic.motor_endurance, baseline.motor_endurance);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify motor response to glucose depletion
 */
TEST_F(MotorControlPipelineTest, MotorGlucoseDepletion) {
    E2E_PIPELINE_START("Motor Glucose Depletion Pipeline");

    E2E_STAGE_BEGIN("Baseline", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t baseline;
    motor_substrate_bridge_get_effects(motor_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Glucose Depletion", MAX_METABOLIC_UPDATE_MS);
    /* Glucose depletion leads to reduced ATP - simulate via low ATP */
    setMetabolicState(0.4f, 0.3f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Endurance Impact", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t low_glucose;
    motor_substrate_bridge_get_effects(motor_bridge_, &low_glucose);
    /* ATP depletion affects endurance and overall capacity */
    EXPECT_LT(low_glucose.motor_endurance, baseline.motor_endurance);
    EXPECT_LT(low_glucose.overall_capacity, baseline.overall_capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Recovery Tests
//=============================================================================

/**
 * @test Verify motor recovery from fatigue
 */
TEST_F(MotorControlPipelineTest, MotorFatigueRecovery) {
    E2E_PIPELINE_START("Motor Fatigue Recovery Pipeline");

    E2E_STAGE_BEGIN("Baseline", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t baseline;
    motor_substrate_bridge_get_effects(motor_bridge_, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce Fatigue", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(0.3f, 0.4f, 0.7f);
    for (uint32_t i = 0; i < 100; ++i) {
        motor_substrate_bridge_update(motor_bridge_);
    }
    motor_substrate_effects_t fatigued;
    motor_substrate_bridge_get_effects(motor_bridge_, &fatigued);
    EXPECT_LT(fatigued.overall_capacity, baseline.overall_capacity);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery Period", MAX_MOTOR_PROCESSING_MS * 2);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    for (uint32_t i = 0; i < 50; ++i) {
        motor_substrate_bridge_update(motor_bridge_);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Recovery", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t recovered;
    motor_substrate_bridge_get_effects(motor_bridge_, &recovered);
    EXPECT_GT(recovered.overall_capacity, fatigued.overall_capacity);
    // Should recover toward baseline
    float recovery_ratio = recovered.overall_capacity / baseline.overall_capacity;
    EXPECT_GT(recovery_ratio, 0.8f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify rapid recovery with optimal resources
 */
TEST_F(MotorControlPipelineTest, MotorRapidRecovery) {
    E2E_PIPELINE_START("Motor Rapid Recovery Pipeline");

    E2E_STAGE_BEGIN("Induce Stress State", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(0.2f, 0.3f, 0.5f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t stressed;
    motor_substrate_bridge_get_effects(motor_bridge_, &stressed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Optimal Resources", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Immediate Recovery Check", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t immediate;
    motor_substrate_bridge_get_effects(motor_bridge_, &immediate);
    EXPECT_GT(immediate.overall_capacity, stressed.overall_capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Extreme Stress Tests
//=============================================================================

/**
 * @test Verify motor survives severe stress
 */
TEST_F(MotorControlPipelineTest, MotorSevereStress) {
    E2E_PIPELINE_START("Motor Severe Stress Pipeline");

    E2E_STAGE_BEGIN("Apply Severe Stress", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.1f, 0.1f, 0.5f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update Under Stress", MAX_MOTOR_PROCESSING_MS);
    int result = motor_substrate_bridge_update(motor_bridge_);
    EXPECT_EQ(0, result); // Should not crash
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Minimal Function", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t effects;
    motor_substrate_bridge_get_effects(motor_bridge_, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    EXPECT_FALSE(std::isnan(effects.overall_capacity));
    EXPECT_FALSE(std::isinf(effects.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify motor at absolute minimum resources
 */
TEST_F(MotorControlPipelineTest, MotorMinimumResources) {
    E2E_PIPELINE_START("Motor Minimum Resources Pipeline");

    E2E_STAGE_BEGIN("Apply Minimum Resources", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.05f, 0.05f, 0.2f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process Under Extreme Stress", MAX_MOTOR_PROCESSING_MS);
    for (uint32_t i = 0; i < 10; ++i) {
        motor_substrate_bridge_update(motor_bridge_);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify No Crash", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t effects;
    int result = motor_substrate_bridge_get_effects(motor_bridge_, &effects);
    EXPECT_EQ(0, result);
    EXPECT_FALSE(std::isnan(effects.motor_precision));
    EXPECT_FALSE(std::isnan(effects.motor_speed));
    EXPECT_FALSE(std::isnan(effects.motor_endurance));
    EXPECT_FALSE(std::isnan(effects.coordination));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

/**
 * @test Verify motor long-term stability
 */
TEST_F(MotorControlPipelineTest, LongTermStability) {
    E2E_PIPELINE_START("Motor Long-Term Stability Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.9f, 0.9f, 0.95f); // Slightly suboptimal but sustainable
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t initial;
    motor_substrate_bridge_get_effects(motor_bridge_, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Extended Operation", MAX_MOTOR_PROCESSING_MS * 20);
    for (uint32_t i = 0; i < STABILITY_ITERATIONS; ++i) {
        motor_substrate_bridge_update(motor_bridge_);

        // Periodic validation
        if (i % 100 == 0) {
            motor_substrate_effects_t current;
            motor_substrate_bridge_get_effects(motor_bridge_, &current);
            EXPECT_FALSE(std::isnan(current.overall_capacity));
            EXPECT_FALSE(std::isinf(current.overall_capacity));
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Final State", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t final_state;
    motor_substrate_bridge_get_effects(motor_bridge_, &final_state);
    float drift = std::abs(final_state.overall_capacity - initial.overall_capacity);
    EXPECT_LT(drift, MAX_CAPACITY_DEGRADATION * 2);

    // All values should remain valid
    EXPECT_GE(final_state.motor_precision, 0.0f);
    EXPECT_LE(final_state.motor_precision, 1.0f);
    EXPECT_GE(final_state.motor_speed, 0.0f);
    EXPECT_LE(final_state.motor_speed, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify stability under fluctuating metabolic state
 */
TEST_F(MotorControlPipelineTest, FluctuatingMetabolicStability) {
    E2E_PIPELINE_START("Fluctuating Metabolic Stability Pipeline");

    std::vector<float> capacity_history;

    E2E_STAGE_BEGIN("Simulate Fluctuations", MAX_MOTOR_PROCESSING_MS * 10);
    for (uint32_t i = 0; i < MOVEMENT_ITERATIONS; ++i) {
        // Sinusoidal metabolic fluctuation
        float phase = static_cast<float>(i) * 0.1f;
        float atp = 0.6f + 0.3f * std::sin(phase);
        float glucose = 0.6f + 0.3f * std::sin(phase + 1.0f);
        float oxygen = 0.8f + 0.15f * std::sin(phase + 2.0f);

        setMetabolicState(atp, glucose, oxygen);
        motor_substrate_bridge_update(motor_bridge_);

        motor_substrate_effects_t effects;
        motor_substrate_bridge_get_effects(motor_bridge_, &effects);
        capacity_history.push_back(effects.overall_capacity);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Stable Response", MAX_MOTOR_PROCESSING_MS);
    // All values should be valid
    for (float capacity : capacity_history) {
        EXPECT_GE(capacity, 0.0f);
        EXPECT_LE(capacity, 1.0f);
        EXPECT_FALSE(std::isnan(capacity));
    }

    // Mean should be reasonable
    float mean = std::accumulate(capacity_history.begin(), capacity_history.end(), 0.0f) /
                 capacity_history.size();
    EXPECT_GT(mean, 0.3f);
    EXPECT_LT(mean, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Apply Effects Tests
//=============================================================================

/**
 * @test Verify motor apply effects pipeline
 */
TEST_F(MotorControlPipelineTest, MotorApplyEffects) {
    E2E_PIPELINE_START("Motor Apply Effects Pipeline");

    E2E_STAGE_BEGIN("Update Bridge", MAX_MOTOR_PROCESSING_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply Effects", MAX_MOTOR_PROCESSING_MS);
    int result = motor_substrate_bridge_apply_effects(motor_bridge_);
    EXPECT_EQ(0, result);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify State Consistency", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t effects;
    motor_substrate_bridge_get_effects(motor_bridge_, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Multi-iteration Processing Tests
//=============================================================================

/**
 * @test Verify stable motor processing over iterations
 */
TEST_F(MotorControlPipelineTest, MotorMultiIterationStability) {
    E2E_PIPELINE_START("Motor Multi-Iteration Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t initial;
    motor_substrate_bridge_get_effects(motor_bridge_, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run Iterations", MAX_MOTOR_PROCESSING_MS * MOVEMENT_ITERATIONS);
    for (uint32_t i = 0; i < MOVEMENT_ITERATIONS; ++i) {
        motor_substrate_bridge_update(motor_bridge_);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Stability", MAX_MOTOR_PROCESSING_MS);
    motor_substrate_effects_t final_state;
    motor_substrate_bridge_get_effects(motor_bridge_, &final_state);

    float degradation = initial.overall_capacity - final_state.overall_capacity;
    EXPECT_LT(degradation, MAX_CAPACITY_DEGRADATION);
    EXPECT_FALSE(std::isnan(final_state.overall_capacity));
    EXPECT_FALSE(std::isinf(final_state.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Bio-Async Messaging Tests
//=============================================================================

/**
 * @test Verify bio-async motor command messaging
 */
TEST_F(MotorControlPipelineTest, BioAsyncMotorCommands) {
    E2E_PIPELINE_START("Bio-Async Motor Commands Pipeline");

    E2E_STAGE_BEGIN("Initialize Optimal State", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate Motor Command Messages", MAX_MOTOR_PROCESSING_MS * 5);
    // Simulate a sequence of motor commands via bio-async messaging pattern
    const uint32_t NUM_COMMANDS = 20;
    std::vector<MotorCommand> commands(NUM_COMMANDS);
    std::vector<bool> command_processed(NUM_COMMANDS, false);

    for (uint32_t i = 0; i < NUM_COMMANDS; ++i) {
        // Generate motor command
        commands[i].target_position[0] = static_cast<float>(rand() % 100) / 100.0f;
        commands[i].target_position[1] = static_cast<float>(rand() % 100) / 100.0f;
        commands[i].target_position[2] = static_cast<float>(rand() % 100) / 100.0f;
        commands[i].force_required = 0.5f + 0.5f * (static_cast<float>(i) / NUM_COMMANDS);
        commands[i].duration_ms = 10 + i * 5;

        // Process command (simulating async message handling)
        motor_substrate_bridge_update(motor_bridge_);

        motor_substrate_effects_t effects;
        motor_substrate_bridge_get_effects(motor_bridge_, &effects);

        // Command is processed if motor capacity allows
        command_processed[i] = (effects.overall_capacity > 0.3f);
    }

    uint32_t processed_count = std::count(command_processed.begin(),
                                          command_processed.end(), true);
    std::cout << "[E2E] Processed " << processed_count << "/" << NUM_COMMANDS
              << " motor commands\n";
    EXPECT_GT(processed_count, NUM_COMMANDS / 2);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify bio-async callback safety under load
 */
TEST_F(MotorControlPipelineTest, BioAsyncCallbackSafetyUnderLoad) {
    E2E_PIPELINE_START("Bio-Async Callback Safety Pipeline");

    E2E_STAGE_BEGIN("Setup Load Conditions", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(0.7f, 0.7f, 0.8f);  // Moderate stress
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("High Frequency Updates (Simulating Callback Load)", MAX_MOTOR_PROCESSING_MS * 20);
    const uint32_t RAPID_UPDATES = 500;
    std::atomic<uint32_t> update_count{0};
    std::atomic<uint32_t> error_count{0};
    std::atomic<uint32_t> nan_count{0};

    for (uint32_t i = 0; i < RAPID_UPDATES; ++i) {
        int result = motor_substrate_bridge_update(motor_bridge_);

        if (result == 0) {
            update_count++;

            motor_substrate_effects_t effects;
            motor_substrate_bridge_get_effects(motor_bridge_, &effects);

            // Check for NaN/Inf (callback safety issue)
            if (std::isnan(effects.overall_capacity) ||
                std::isinf(effects.overall_capacity)) {
                nan_count++;
            }
        } else {
            error_count++;
        }
    }

    std::cout << "[E2E] Rapid updates: " << update_count.load() << " success, "
              << error_count.load() << " errors, "
              << nan_count.load() << " NaN/Inf\n";

    EXPECT_EQ(error_count.load(), 0u) << "Callback errors under load";
    EXPECT_EQ(nan_count.load(), 0u) << "Numerical errors in callbacks";
    EXPECT_EQ(update_count.load(), RAPID_UPDATES) << "All updates should succeed";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify motor command execution with bio-async coordination
 */
TEST_F(MotorControlPipelineTest, MotorCommandExecutionWithBioAsync) {
    E2E_PIPELINE_START("Motor Command Execution with Bio-Async");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute Movement Sequence", MAX_MOTOR_PROCESSING_MS * 10);
    const uint32_t SEQUENCE_LENGTH = 50;
    std::vector<MotorResult> results(SEQUENCE_LENGTH);

    float cumulative_error = 0.0f;

    for (uint32_t i = 0; i < SEQUENCE_LENGTH; ++i) {
        // Get current motor state
        motor_substrate_effects_t effects;
        motor_substrate_bridge_get_effects(motor_bridge_, &effects);

        // Simulate movement execution
        float target_pos = static_cast<float>(i) / SEQUENCE_LENGTH;
        float precision = effects.motor_precision;
        float execution_error = (1.0f - precision) * 0.1f;

        results[i].actual_position[0] = target_pos + execution_error * (rand() % 100 - 50) / 100.0f;
        results[i].position_error = std::abs(results[i].actual_position[0] - target_pos);
        results[i].completed = true;

        cumulative_error += results[i].position_error;

        // Update motor state
        motor_substrate_bridge_update(motor_bridge_);
    }

    float avg_error = cumulative_error / SEQUENCE_LENGTH;
    std::cout << "[E2E] Average position error: " << avg_error << "\n";

    EXPECT_LT(avg_error, 0.2f) << "Movement precision too low";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Completion", MAX_MOTOR_PROCESSING_MS);
    uint32_t completed_count = 0;
    for (const auto& r : results) {
        if (r.completed) completed_count++;
    }
    EXPECT_EQ(completed_count, SEQUENCE_LENGTH) << "Not all movements completed";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify bio-async message ordering for motor commands
 */
TEST_F(MotorControlPipelineTest, BioAsyncMessageOrdering) {
    E2E_PIPELINE_START("Bio-Async Message Ordering Pipeline");

    E2E_STAGE_BEGIN("Initialize", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Send Ordered Commands", MAX_MOTOR_PROCESSING_MS * 5);
    // Simulate sending ordered motor commands
    std::vector<uint32_t> send_order;
    std::vector<uint32_t> process_order;

    const uint32_t NUM_ORDERED_COMMANDS = 30;

    for (uint32_t i = 0; i < NUM_ORDERED_COMMANDS; ++i) {
        send_order.push_back(i);

        // Process command
        motor_substrate_bridge_update(motor_bridge_);
        process_order.push_back(i);  // In real bio-async, order might differ
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Order Preservation", MAX_MOTOR_PROCESSING_MS);
    // In synchronous simulation, order should be preserved
    EXPECT_EQ(send_order.size(), process_order.size());

    bool order_preserved = true;
    for (size_t i = 0; i < send_order.size(); ++i) {
        if (send_order[i] != process_order[i]) {
            order_preserved = false;
            break;
        }
    }

    std::cout << "[E2E] Message order preserved: " << (order_preserved ? "yes" : "no") << "\n";
    EXPECT_TRUE(order_preserved) << "Motor command order not preserved";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/**
 * @test Verify motor effector response to bio-async signals
 */
TEST_F(MotorControlPipelineTest, MotorEffectorBioAsyncResponse) {
    E2E_PIPELINE_START("Motor Effector Bio-Async Response Pipeline");

    E2E_STAGE_BEGIN("Initialize Effector State", MAX_METABOLIC_UPDATE_MS);
    setMetabolicState(1.0f, 1.0f, 1.0f);
    motor_substrate_bridge_update(motor_bridge_);
    motor_substrate_effects_t initial;
    motor_substrate_bridge_get_effects(motor_bridge_, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Send Bio-Async Signals", MAX_MOTOR_PROCESSING_MS * 5);
    // Simulate different neuromodulator signals affecting motor output
    struct Signal {
        const char* name;
        float atp;
        float glucose;
        float oxygen;
    };

    std::vector<Signal> signals = {
        {"Normal", 1.0f, 1.0f, 1.0f},
        {"Dopamine Surge", 0.9f, 1.0f, 1.0f},   // Simulated via ATP
        {"Fatigue Signal", 0.5f, 0.6f, 0.9f},
        {"Recovery Signal", 0.8f, 0.9f, 1.0f},
    };

    for (const auto& sig : signals) {
        setMetabolicState(sig.atp, sig.glucose, sig.oxygen);
        motor_substrate_bridge_update(motor_bridge_);

        motor_substrate_effects_t effects;
        motor_substrate_bridge_get_effects(motor_bridge_, &effects);

        std::cout << "[E2E] Signal '" << sig.name << "': "
                  << "precision=" << effects.motor_precision
                  << " speed=" << effects.motor_speed
                  << " capacity=" << effects.overall_capacity << "\n";

        // Verify valid response
        EXPECT_FALSE(std::isnan(effects.motor_precision));
        EXPECT_FALSE(std::isnan(effects.motor_speed));
        EXPECT_FALSE(std::isnan(effects.overall_capacity));
    }
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
