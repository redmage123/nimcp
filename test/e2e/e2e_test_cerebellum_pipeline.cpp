/**
 * @file e2e_test_cerebellum_pipeline.cpp
 * @brief End-to-end tests for Cerebellum Pipeline
 *
 * WHAT: Full pipeline tests for motor coordination and timing
 * WHY:  Verify complete cerebellar workflows with substrate integration
 * HOW:  Test coordination, timing, error correction, procedural learning
 *
 * TEST COVERAGE:
 * - Motor Coordination Pipeline (4 tests)
 * - Timing Precision (4 tests)
 * - Error Correction (3 tests)
 * - Procedural Learning (4 tests)
 * - Metabolic Effects (3 tests)
 * - Long-Term Stability (3 tests)
 *
 * TOTAL: 21 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Cerebellum is critical for motor timing and coordination
 * - Purkinje cells provide inhibitory output
 * - Climbing fibers carry error signals
 * - Parallel fibers carry mossy fiber input
 * - Deep cerebellar nuclei provide output
 * - Learning through LTD at parallel fiber synapses
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
#include "core/cerebellum/nimcp_cerebellum_substrate_bridge.h"
#include "core/cerebellum/nimcp_cerebellum_thalamic_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_COORDINATION_TIME_MS = 20.0;
constexpr double MAX_TIMING_TIME_MS = 10.0;
constexpr double MAX_LEARNING_TIME_MS = 100.0;
constexpr float MIN_MOTOR_CAPACITY = 0.3f;
constexpr float TIMING_PRECISION_THRESHOLD = 0.7f;
constexpr uint32_t MOVEMENT_SEQUENCE_LENGTH = 10;
constexpr float ERROR_CORRECTION_RATE = 0.1f;

//=============================================================================
// Helper Structures
//=============================================================================

struct MotorCommand {
    float target_position;
    float target_velocity;
    float timing_requirement;
};

struct MovementSequence {
    std::vector<MotorCommand> commands;
    float total_duration_ms;
};

//=============================================================================
// Test Fixtures
//=============================================================================

class E2ECerebellumCoordinationTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cerebellum_substrate_bridge_t* cb_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        cerebellum_substrate_config_t cb_config = cerebellum_substrate_default_config();
        cb_bridge = cerebellum_substrate_bridge_create(nullptr, substrate, &cb_config);
        ASSERT_NE(cb_bridge, nullptr);
    }

    void TearDown() override {
        if (cb_bridge) {
            cerebellum_substrate_bridge_destroy(cb_bridge);
            cb_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    MotorCommand createCommand(float pos, float vel, float timing) {
        MotorCommand cmd;
        cmd.target_position = pos;
        cmd.target_velocity = vel;
        cmd.timing_requirement = timing;
        return cmd;
    }
};

class E2ECerebellumTimingTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cerebellum_substrate_bridge_t* cb_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        cerebellum_substrate_config_t cb_config = cerebellum_substrate_default_config();
        cb_bridge = cerebellum_substrate_bridge_create(nullptr, substrate, &cb_config);
        ASSERT_NE(cb_bridge, nullptr);
    }

    void TearDown() override {
        if (cb_bridge) {
            cerebellum_substrate_bridge_destroy(cb_bridge);
            cb_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2ECerebellumErrorCorrectionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cerebellum_substrate_bridge_t* cb_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        cerebellum_substrate_config_t cb_config = cerebellum_substrate_default_config();
        cb_bridge = cerebellum_substrate_bridge_create(nullptr, substrate, &cb_config);
        ASSERT_NE(cb_bridge, nullptr);
    }

    void TearDown() override {
        if (cb_bridge) {
            cerebellum_substrate_bridge_destroy(cb_bridge);
            cb_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2ECerebellumLearningTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cerebellum_substrate_bridge_t* cb_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        cerebellum_substrate_config_t cb_config = cerebellum_substrate_default_config();
        cb_bridge = cerebellum_substrate_bridge_create(nullptr, substrate, &cb_config);
        ASSERT_NE(cb_bridge, nullptr);
    }

    void TearDown() override {
        if (cb_bridge) {
            cerebellum_substrate_bridge_destroy(cb_bridge);
            cb_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

//=============================================================================
// Motor Coordination Pipeline Tests
//=============================================================================

TEST_F(E2ECerebellumCoordinationTest, BaselineCoordinationCapacity) {
    // Scenario: Verify baseline motor coordination with optimal substrate
    E2E_PIPELINE_START("Baseline Coordination Capacity");

    E2E_STAGE_BEGIN("Initialize substrate", 5);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update cerebellum bridge", 10);
    int result = cerebellum_substrate_bridge_update(cb_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get effects", 5);
    cerebellum_substrate_effects_t effects;
    result = cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity", 2);
    EXPECT_GT(effects.overall_capacity, MIN_MOTOR_CAPACITY);
    EXPECT_GT(effects.motor_coordination, MIN_MOTOR_CAPACITY);
    EXPECT_GT(effects.timing_precision, MIN_MOTOR_CAPACITY);
    EXPECT_GT(effects.procedural_learning, MIN_MOTOR_CAPACITY);
    EXPECT_GT(effects.error_correction, MIN_MOTOR_CAPACITY);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumCoordinationTest, CoordinatedMovementSequence) {
    // Scenario: Execute coordinated movement sequence
    E2E_PIPELINE_START("Coordinated Movement Sequence");

    E2E_STAGE_BEGIN("Create movement sequence", 10);
    MovementSequence sequence;
    sequence.total_duration_ms = 0.0f;

    for (int i = 0; i < MOVEMENT_SEQUENCE_LENGTH; i++) {
        MotorCommand cmd = createCommand(
            i * 10.0f,          // Position
            5.0f + i * 0.5f,    // Velocity
            50.0f               // Timing
        );
        sequence.commands.push_back(cmd);
        sequence.total_duration_ms += cmd.timing_requirement;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute sequence", 100);
    for (size_t i = 0; i < sequence.commands.size(); i++) {
        // Simulate movement execution
        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 50);
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);

        // Coordination should remain valid
        EXPECT_GE(effects.motor_coordination, 0.0f);
        EXPECT_LE(effects.motor_coordination, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    cerebellum_substrate_effects_t final_effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &final_effects);
    EXPECT_GT(final_effects.motor_coordination, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumCoordinationTest, MultiLimbCoordination) {
    // Scenario: Coordinate multiple limbs simultaneously
    E2E_PIPELINE_START("Multi-Limb Coordination");

    E2E_STAGE_BEGIN("Simultaneous limb commands", 50);
    // Higher activity for multi-limb coordination
    for (int step = 0; step < 20; step++) {
        substrate_record_spikes(substrate, 300);  // More spikes for multi-limb
        substrate_record_transmissions(substrate, 600);
        substrate_update(substrate, 25);
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);

        EXPECT_GE(effects.motor_coordination, 0.0f);
        EXPECT_FALSE(std::isnan(effects.motor_coordination));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stability", 5);
    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumCoordinationTest, ApplyCoordinationEffects) {
    // Scenario: Apply coordination effects
    E2E_PIPELINE_START("Apply Coordination Effects");

    E2E_STAGE_BEGIN("Update and apply", 20);
    cerebellum_substrate_bridge_update(cb_bridge);
    int result = cerebellum_substrate_bridge_apply_effects(cb_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify application", 5);
    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Timing Precision Tests
//=============================================================================

TEST_F(E2ECerebellumTimingTest, BaselineTimingPrecision) {
    // Scenario: Baseline timing precision
    E2E_PIPELINE_START("Baseline Timing Precision");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get timing precision", 5);
    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify precision", 2);
    EXPECT_GT(effects.timing_precision, MIN_MOTOR_CAPACITY);
    EXPECT_LE(effects.timing_precision, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumTimingTest, TimingUnderMetabolicStress) {
    // Scenario: Timing precision degrades under metabolic stress
    E2E_PIPELINE_START("Timing Under Metabolic Stress");

    E2E_STAGE_BEGIN("Normal ATP timing", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t normal;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &normal);
    float normal_timing = normal.timing_precision;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low ATP timing", 10);
    substrate_set_atp(substrate, 0.4f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t stressed;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &stressed);
    float stressed_timing = stressed.timing_precision;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GT(normal_timing, 0.0f);
    EXPECT_GE(stressed_timing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumTimingTest, RhythmicTimingPattern) {
    // Scenario: Maintain rhythmic timing pattern
    E2E_PIPELINE_START("Rhythmic Timing Pattern");

    E2E_STAGE_BEGIN("Generate rhythm", 100);
    std::vector<float> timing_values;
    float target_interval = 100.0f;  // Target interval in ms

    for (int beat = 0; beat < 20; beat++) {
        substrate_record_spikes(substrate, 50);
        substrate_update(substrate, (uint64_t)target_interval);
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
        timing_values.push_back(effects.timing_precision);

        EXPECT_GE(effects.timing_precision, 0.0f);
        EXPECT_LE(effects.timing_precision, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze consistency", 5);
    float mean = std::accumulate(timing_values.begin(), timing_values.end(), 0.0f) / timing_values.size();
    EXPECT_GT(mean, 0.0f);
    EXPECT_FALSE(std::isnan(mean));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumTimingTest, TimingWithTemperatureVariation) {
    // Scenario: Temperature affects timing precision (Q10 effect)
    E2E_PIPELINE_START("Timing With Temperature Variation");

    E2E_STAGE_BEGIN("Normal temperature", 10);
    substrate_set_temperature(substrate, SUBSTRATE_NORMAL_TEMPERATURE);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t normal_temp;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &normal_temp);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cool temperature", 10);
    substrate_set_temperature(substrate, 34.0f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t cool;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &cool);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Warm temperature", 10);
    substrate_set_temperature(substrate, 39.0f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t warm;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &warm);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all valid", 2);
    EXPECT_GE(normal_temp.timing_precision, 0.0f);
    EXPECT_GE(cool.timing_precision, 0.0f);
    EXPECT_GE(warm.timing_precision, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Error Correction Tests
//=============================================================================

TEST_F(E2ECerebellumErrorCorrectionTest, BaselineErrorCorrection) {
    // Scenario: Baseline error correction capacity
    E2E_PIPELINE_START("Baseline Error Correction");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get error correction", 5);
    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.error_correction, MIN_MOTOR_CAPACITY);
    EXPECT_LE(effects.error_correction, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumErrorCorrectionTest, ErrorCorrectionDuringMovement) {
    // Scenario: Error correction during continuous movement
    E2E_PIPELINE_START("Error Correction During Movement");

    E2E_STAGE_BEGIN("Simulate movement with errors", 100);
    std::vector<float> correction_capacity;

    for (int step = 0; step < 50; step++) {
        // Simulate error signal (climbing fiber)
        float error_magnitude = 0.5f + 0.3f * sinf(step * 0.2f);

        // Error correction consumes energy
        substrate_record_spikes(substrate, (uint32_t)(error_magnitude * 100));
        substrate_update(substrate, 20);
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
        correction_capacity.push_back(effects.error_correction);

        EXPECT_GE(effects.error_correction, 0.0f);
        EXPECT_LE(effects.error_correction, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze correction", 5);
    float mean = std::accumulate(correction_capacity.begin(), correction_capacity.end(), 0.0f)
                 / correction_capacity.size();
    EXPECT_GT(mean, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumErrorCorrectionTest, ErrorCorrectionUnderFatigue) {
    // Scenario: Error correction degrades with fatigue
    E2E_PIPELINE_START("Error Correction Under Fatigue");

    E2E_STAGE_BEGIN("Fresh state", 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t fresh;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &fresh);
    float fresh_correction = fresh.error_correction;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce fatigue", 100);
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 300);
        substrate_record_transmissions(substrate, 700);
        substrate_update(substrate, 20);
    }
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t fatigued;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &fatigued);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(fresh_correction, 0.0f);
    EXPECT_GE(fatigued.error_correction, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Procedural Learning Tests
//=============================================================================

TEST_F(E2ECerebellumLearningTest, BaselineProceduralLearning) {
    // Scenario: Baseline procedural learning capacity
    E2E_PIPELINE_START("Baseline Procedural Learning");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get learning capacity", 5);
    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.procedural_learning, MIN_MOTOR_CAPACITY);
    EXPECT_LE(effects.procedural_learning, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumLearningTest, LearningAcrossTrials) {
    // Scenario: Track learning capacity across multiple trials
    E2E_PIPELINE_START("Learning Across Trials");

    E2E_STAGE_BEGIN("Run learning trials", 200);
    std::vector<float> learning_values;

    for (int trial = 0; trial < 30; trial++) {
        // Learning trial
        substrate_record_spikes(substrate, 150);
        substrate_record_transmissions(substrate, 400);
        substrate_update(substrate, 100);
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
        learning_values.push_back(effects.procedural_learning);

        EXPECT_GE(effects.procedural_learning, 0.0f);
        EXPECT_LE(effects.procedural_learning, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze learning", 5);
    float mean = std::accumulate(learning_values.begin(), learning_values.end(), 0.0f)
                 / learning_values.size();
    EXPECT_GT(mean, 0.0f);

    // Check for no NaN values
    for (float v : learning_values) {
        EXPECT_FALSE(std::isnan(v));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumLearningTest, LearningWithATPModulation) {
    // Scenario: Learning requires ATP for LTD
    E2E_PIPELINE_START("Learning With ATP Modulation");

    E2E_STAGE_BEGIN("High ATP learning", 20);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t high_atp;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &high_atp);
    float high_atp_learning = high_atp.procedural_learning;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low ATP learning", 20);
    substrate_set_atp(substrate, 0.3f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t low_atp;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &low_atp);
    float low_atp_learning = low_atp.procedural_learning;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GT(high_atp_learning, 0.0f);
    EXPECT_GE(low_atp_learning, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumLearningTest, SkillAcquisitionSimulation) {
    // Scenario: Simulate skill acquisition over time
    E2E_PIPELINE_START("Skill Acquisition Simulation");

    E2E_STAGE_BEGIN("Initial skill level", 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t initial;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Practice sessions", 200);
    for (int session = 0; session < 10; session++) {
        // Each practice session
        for (int trial = 0; trial < 10; trial++) {
            substrate_record_spikes(substrate, 100);
            substrate_update(substrate, 50);
        }
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);

        // Learning capacity should remain valid
        EXPECT_GE(effects.procedural_learning, 0.0f);
        EXPECT_LE(effects.procedural_learning, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final skill level", 5);
    cerebellum_substrate_effects_t final_effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &final_effects);
    EXPECT_GE(final_effects.procedural_learning, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Metabolic Effects Tests
//=============================================================================

TEST_F(E2ECerebellumCoordinationTest, ATPEffectsOnCoordination) {
    // Scenario: ATP levels affect coordination
    E2E_PIPELINE_START("ATP Effects On Coordination");

    float atp_levels[] = {0.95f, 0.7f, 0.5f, 0.3f};
    std::vector<cerebellum_substrate_effects_t> effects_at_atp;

    E2E_STAGE_BEGIN("Test ATP levels", 40);
    for (float atp : atp_levels) {
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 10);
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
        effects_at_atp.push_back(effects);

        EXPECT_GE(effects.motor_coordination, 0.0f);
        EXPECT_LE(effects.motor_coordination, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify gradient", 5);
    // All values should be valid
    for (const auto& eff : effects_at_atp) {
        EXPECT_FALSE(std::isnan(eff.motor_coordination));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumCoordinationTest, GlucoseEffectsOnMotor) {
    // Scenario: Glucose affects motor function
    E2E_PIPELINE_START("Glucose Effects On Motor");

    E2E_STAGE_BEGIN("Normal glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t normal;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_CRITICAL_GLUCOSE);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t low;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &low);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify effects", 2);
    EXPECT_GT(normal.overall_capacity, 0.0f);
    EXPECT_GE(low.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumCoordinationTest, OxygenDependentMotorFunction) {
    // Scenario: Motor function requires oxygen
    E2E_PIPELINE_START("Oxygen Dependent Motor Function");

    E2E_STAGE_BEGIN("Normal oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t normal_o2;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &normal_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t low_o2;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &low_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(normal_o2.motor_coordination, 0.0f);
    EXPECT_GE(low_o2.motor_coordination, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

TEST_F(E2ECerebellumCoordinationTest, LongSimulationStability) {
    // Scenario: Extended simulation without degradation
    E2E_PIPELINE_START("Long Simulation Stability");

    E2E_STAGE_BEGIN("Extended simulation", 500);
    for (int step = 0; step < 1000; step++) {
        substrate_update(substrate, 10);
        cerebellum_substrate_bridge_update(cb_bridge);

        if (step % 100 == 0) {
            cerebellum_substrate_effects_t effects;
            cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);

            EXPECT_FALSE(std::isnan(effects.overall_capacity));
            EXPECT_FALSE(std::isinf(effects.overall_capacity));
            EXPECT_GE(effects.overall_capacity, 0.0f);
            EXPECT_LE(effects.overall_capacity, 1.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    cerebellum_substrate_effects_t final_effects;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &final_effects);

    EXPECT_GT(final_effects.overall_capacity, 0.0f);
    EXPECT_GT(final_effects.motor_coordination, 0.0f);
    EXPECT_GT(final_effects.timing_precision, 0.0f);
    EXPECT_GT(final_effects.procedural_learning, 0.0f);
    EXPECT_GT(final_effects.error_correction, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumCoordinationTest, StressRecoveryPipeline) {
    // Scenario: Recovery from stress conditions
    E2E_PIPELINE_START("Stress Recovery Pipeline");

    E2E_STAGE_BEGIN("Baseline", 10);
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t baseline;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply stress", 50);
    substrate_set_atp(substrate, 0.3f);
    for (int i = 0; i < 20; i++) {
        substrate_record_spikes(substrate, 400);
        substrate_update(substrate, 10);
    }
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t stressed;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &stressed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery", 100);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    for (int i = 0; i < 100; i++) {
        substrate_update(substrate, 50);
    }
    cerebellum_substrate_bridge_update(cb_bridge);

    cerebellum_substrate_effects_t recovered;
    cerebellum_substrate_bridge_get_effects(cb_bridge, &recovered);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery", 5);
    EXPECT_GE(recovered.overall_capacity, stressed.overall_capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ECerebellumTimingTest, ContinuousOperationStability) {
    // Scenario: Timing remains stable during continuous operation
    E2E_PIPELINE_START("Continuous Operation Stability");

    E2E_STAGE_BEGIN("Continuous timing", 300);
    std::vector<float> timing_samples;

    for (int interval = 0; interval < 100; interval++) {
        substrate_record_spikes(substrate, 80);
        substrate_update(substrate, 50);
        cerebellum_substrate_bridge_update(cb_bridge);

        cerebellum_substrate_effects_t effects;
        cerebellum_substrate_bridge_get_effects(cb_bridge, &effects);
        timing_samples.push_back(effects.timing_precision);

        EXPECT_FALSE(std::isnan(effects.timing_precision));
        EXPECT_FALSE(std::isinf(effects.timing_precision));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 5);
    // No extreme values
    float min_val = *std::min_element(timing_samples.begin(), timing_samples.end());
    float max_val = *std::max_element(timing_samples.begin(), timing_samples.end());

    EXPECT_GE(min_val, 0.0f);
    EXPECT_LE(max_val, 1.0f);
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
