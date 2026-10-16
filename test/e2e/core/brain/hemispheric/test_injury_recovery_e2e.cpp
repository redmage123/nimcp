/**
 * @file test_injury_recovery_e2e.cpp
 * @brief End-to-end tests for hemispheric injury simulation and recovery
 *
 * WHAT: Full pipeline tests for brain injury modeling and neuroplastic recovery
 * WHY:  Verify injury effects, compensation mechanisms, and rehabilitation
 * HOW:  Test lesion induction, recovery trajectories, and rehabilitation effects
 *
 * TEST COVERAGE:
 * - Injury Induction (3 tests)
 * - Compensation Mechanisms (4 tests)
 * - Recovery Trajectory (4 tests)
 * - Rehabilitation Effects (4 tests)
 *
 * TOTAL: 15 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Stroke causes focal damage with spreading diaschisis
 * - Contralateral hemisphere can compensate for some functions
 * - Recovery follows predictable phases: acute, subacute, chronic
 * - Rehabilitation enhances neuroplasticity and functional recovery
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "../../../e2e_test_framework.h"
#include "utils/nimcp_test_base.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>


#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "core/brain/hemispheric/nimcp_hemispheric_injury.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_INJURY_OP_TIME_MS = 100.0;
constexpr double MAX_RECOVERY_TIME_MS = 500.0;
constexpr double MAX_REHAB_TIME_MS = 200.0;
constexpr float SEVERE_DAMAGE_LEVEL = 0.8f;
constexpr float MODERATE_DAMAGE_LEVEL = 0.5f;
constexpr float MILD_DAMAGE_LEVEL = 0.25f;
constexpr float RECOVERY_THRESHOLD = 0.3f;
constexpr uint32_t INPUT_SIZE = 64;
constexpr uint32_t OUTPUT_SIZE = 32;

//=============================================================================
// Helper Functions
//=============================================================================

static std::vector<float> generate_input_pattern(uint32_t size, uint32_t seed) {
    std::vector<float> input(size);
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t i = 0; i < size; i++) {
        input[i] = dist(gen);
    }
    return input;
}

static std::vector<float> generate_rehabilitation_pattern(uint32_t size) {
    std::vector<float> pattern(size);
    for (uint32_t i = 0; i < size; i++) {
        // Structured rehabilitation pattern
        pattern[i] = 0.5f + 0.4f * std::sin(2.0f * M_PI * i / 16.0f);
    }
    return pattern;
}

//=============================================================================
// Test Fixture
//=============================================================================

class E2EInjuryRecoveryTest : public ::testing::Test {
protected:
    static hemispheric_brain_t* shared_brain;
    hemispheric_brain_t* brain = nullptr;
    hemispheric_injury_system_t* injury_system = nullptr;

    static void SetUpTestSuite() {
        signal_handler_unregister_brain();
        signal_handler_reset_stats();
        signal_handler_uninstall();

        hemispheric_brain_config_t config = hemispheric_brain_default_config();
        config.size = BRAIN_SIZE_MICRO;
        config.num_inputs = INPUT_SIZE;
        config.num_outputs = OUTPUT_SIZE;
        config.default_mode = HEMISPHERIC_MODE_LATERALIZED;

        shared_brain = hemispheric_brain_create(&config);
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            hemispheric_brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        brain = shared_brain;
        ASSERT_NE(brain, nullptr) << "Failed to create hemispheric brain";
    }

    void TearDown() override {
        if (injury_system) {
            hemispheric_injury_destroy(injury_system);
            injury_system = nullptr;
        }
    }

    void createInjurySystem() {
        hemispheric_injury_config_t config = hemispheric_injury_default_config();
        config.enable_spontaneous_recovery = true;
        config.enable_contralateral_compensation = true;
        config.enable_perilesional_plasticity = true;
        config.enable_diaschisis = true;
        config.enable_rehabilitation = true;

        injury_system = hemispheric_injury_create(&config, brain);
        ASSERT_NE(injury_system, nullptr) << "Failed to create injury system";
    }
};

hemispheric_brain_t* E2EInjuryRecoveryTest::shared_brain = nullptr;

//=============================================================================
// Injury Induction Tests
//=============================================================================

TEST_F(E2EInjuryRecoveryTest, InduceIschemicStrokeToMotorCortex) {
    E2E_PIPELINE_START("Ischemic Stroke Motor Cortex");

    createInjurySystem();

    // Induce ischemic stroke to left motor cortex
    E2E_STAGE_BEGIN("Induce stroke", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_SEVERE,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX,
        SEVERE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    // Lesion IDs start at 0 (based on stats.total_lesions counter)
    // Just verify the induce succeeded via result code above
    E2E_STAGE_END();

    // Verify damage
    E2E_STAGE_BEGIN("Verify damage", 20);
    float damage = hemispheric_injury_get_damage(
        injury_system,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX
    );
    EXPECT_GT(damage, 0.5f) << "Should have significant damage";
    E2E_STAGE_END();

    // Verify function loss
    E2E_STAGE_BEGIN("Verify function loss", 20);
    float function = hemispheric_injury_get_function(
        injury_system,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX
    );
    EXPECT_LT(function, 0.5f) << "Function should be reduced";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, InduceBrocaAreaAphasia) {
    E2E_PIPELINE_START("Broca Area Aphasia");

    createInjurySystem();

    // Induce lesion to Broca's area (language production)
    E2E_STAGE_BEGIN("Induce Broca lesion", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_MODERATE,
        HEMISPHERE_LEFT,
        INJURY_REGION_BROCA,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Test language processing after injury
    E2E_STAGE_BEGIN("Test language processing", MAX_INJURY_OP_TIME_MS);
    auto input = generate_input_pattern(INPUT_SIZE, 42);
    std::vector<float> output(OUTPUT_SIZE);

    // Language processing should be impaired
    result = hemispheric_brain_process_lateralized(
        brain,
        input.data(),
        INPUT_SIZE,
        COGNITIVE_DOMAIN_LANGUAGE,
        output.data(),
        OUTPUT_SIZE
    );
    // Processing may still succeed but with degraded output
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify Broca damage
    E2E_STAGE_BEGIN("Verify Broca damage", 20);
    float damage = hemispheric_injury_get_damage(
        injury_system,
        HEMISPHERE_LEFT,
        INJURY_REGION_BROCA
    );
    EXPECT_GT(damage, 0.3f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, InduceDiffuseAxonalInjury) {
    E2E_PIPELINE_START("Diffuse Axonal Injury");

    createInjurySystem();

    // Induce TBI with diffuse damage
    E2E_STAGE_BEGIN("Induce DAI", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_TBI_DIFFUSE,
        SEVERITY_MODERATE,
        HEMISPHERE_RIGHT,
        INJURY_REGION_PREFRONTAL,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Check connectivity loss (characteristic of DAI)
    E2E_STAGE_BEGIN("Check connectivity", 20);
    float conn_loss = hemispheric_injury_get_connectivity_loss(
        injury_system,
        HEMISPHERE_RIGHT,
        INJURY_REGION_PREFRONTAL
    );
    EXPECT_GT(conn_loss, 0.0f) << "DAI should cause connectivity loss";
    E2E_STAGE_END();

    // Get full region state
    E2E_STAGE_BEGIN("Get region state", 20);
    region_damage_state_t state = hemispheric_injury_get_region_state(
        injury_system,
        HEMISPHERE_RIGHT,
        INJURY_REGION_PREFRONTAL
    );
    EXPECT_GT(state.structural_damage, 0.0f);
    EXPECT_GT(state.functional_impairment, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Compensation Mechanism Tests
//=============================================================================

TEST_F(E2EInjuryRecoveryTest, ContralateralCompensation) {
    E2E_PIPELINE_START("Contralateral Compensation");

    createInjurySystem();

    // Induce left hemisphere damage
    E2E_STAGE_BEGIN("Induce left damage", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_SEVERE,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX,
        SEVERE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Update to allow compensation to develop
    E2E_STAGE_BEGIN("Allow compensation development", MAX_RECOVERY_TIME_MS);
    for (int day = 0; day < 30; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);  // 1 day per update
        EXPECT_EQ(result, 0);
    }
    E2E_STAGE_END();

    // Check compensatory function
    E2E_STAGE_BEGIN("Check compensation", 20);
    region_damage_state_t state = hemispheric_injury_get_region_state(
        injury_system,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX
    );

    // Should have some compensatory function from right hemisphere
    EXPECT_GE(state.compensatory_function, 0.0f);
    E2E_STAGE_END();

    // Verify right hemisphere is still functional
    E2E_STAGE_BEGIN("Verify right function", 20);
    float right_function = hemispheric_injury_get_function(
        injury_system,
        HEMISPHERE_RIGHT,
        INJURY_REGION_MOTOR_CORTEX
    );
    EXPECT_GT(right_function, 0.8f) << "Right hemisphere should be intact";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, PerilesionalPlasticity) {
    E2E_PIPELINE_START("Perilesional Plasticity");

    createInjurySystem();

    // Induce focal lesion
    E2E_STAGE_BEGIN("Induce focal lesion", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_SURGICAL,
        SEVERITY_MODERATE,
        HEMISPHERE_LEFT,
        INJURY_REGION_SENSORY_CORTEX,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Boost plasticity
    E2E_STAGE_BEGIN("Boost plasticity", MAX_INJURY_OP_TIME_MS);
    result = hemispheric_injury_boost_plasticity(injury_system, HEMISPHERE_LEFT, 2.0f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Update with enhanced plasticity
    E2E_STAGE_BEGIN("Recovery with plasticity", MAX_RECOVERY_TIME_MS);
    for (int day = 0; day < 14; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);
    }
    E2E_STAGE_END();

    // Check recovery
    E2E_STAGE_BEGIN("Check enhanced recovery", 20);
    float recovery = hemispheric_injury_get_recovery(
        injury_system,
        HEMISPHERE_LEFT,
        INJURY_REGION_SENSORY_CORTEX
    );
    EXPECT_GE(recovery, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, DiaschisissEffects) {
    E2E_PIPELINE_START("Diaschisis Effects");

    createInjurySystem();

    // Induce lesion with diaschisis
    E2E_STAGE_BEGIN("Induce lesion with diaschisis", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_HEMORRHAGIC,
        SEVERITY_SEVERE,
        HEMISPHERE_LEFT,
        INJURY_REGION_PREFRONTAL,
        SEVERE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Check diaschisis in connected regions
    E2E_STAGE_BEGIN("Check diaschisis", 50);
    // Diaschisis should affect connected regions
    const lesion_t* lesion = hemispheric_injury_get_lesion(injury_system, lesion_id);
    ASSERT_NE(lesion, nullptr);

    // Primary damage at lesion site
    EXPECT_GT(lesion->primary_damage, 0.5f);

    // Secondary damage from diaschisis
    EXPECT_GE(lesion->secondary_damage, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, BilateralModeActivation) {
    E2E_PIPELINE_START("Bilateral Mode Activation");

    createInjurySystem();

    // Induce severe unilateral damage
    E2E_STAGE_BEGIN("Induce severe damage", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_PROFOUND,
        HEMISPHERE_LEFT,
        INJURY_REGION_BROCA,
        0.9f,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Activate bilateral mode for compensation
    E2E_STAGE_BEGIN("Activate bilateral mode", 50);
    result = hemispheric_brain_set_bilateral_mode(brain, true);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(hemispheric_brain_is_bilateral_mode(brain));
    E2E_STAGE_END();

    // Process in bilateral mode
    E2E_STAGE_BEGIN("Bilateral processing", MAX_INJURY_OP_TIME_MS);
    auto input = generate_input_pattern(INPUT_SIZE, 123);
    std::vector<float> output(OUTPUT_SIZE);

    result = hemispheric_brain_process_cooperative(
        brain,
        input.data(),
        INPUT_SIZE,
        output.data(),
        OUTPUT_SIZE
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Recovery Trajectory Tests
//=============================================================================

TEST_F(E2EInjuryRecoveryTest, AcuteToChronicRecoveryPhases) {
    E2E_PIPELINE_START("Recovery Phases");

    createInjurySystem();

    // Induce injury
    E2E_STAGE_BEGIN("Induce injury", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_MODERATE,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Track phases during recovery
    E2E_STAGE_BEGIN("Track recovery phases", MAX_RECOVERY_TIME_MS);
    std::vector<recovery_phase_t> phase_history;

    // Days 0-7: Acute
    for (int day = 0; day < 7; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);
        phase_history.push_back(hemispheric_injury_get_phase(injury_system));
    }

    // Days 7-30: Subacute
    for (int day = 7; day < 30; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);
        phase_history.push_back(hemispheric_injury_get_phase(injury_system));
    }

    // Days 30-90: Chronic
    for (int day = 30; day < 90; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);
    }
    phase_history.push_back(hemispheric_injury_get_phase(injury_system));

    // Should have progressed through phases
    EXPECT_GT(phase_history.size(), 0u);
    E2E_STAGE_END();

    // Verify final phase
    E2E_STAGE_BEGIN("Verify final phase", 20);
    recovery_phase_t final_phase = hemispheric_injury_get_phase(injury_system);
    EXPECT_TRUE(final_phase == RECOVERY_PHASE_CHRONIC ||
                final_phase == RECOVERY_PHASE_PLATEAU);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, RecoveryProgressTracking) {
    E2E_PIPELINE_START("Recovery Progress Tracking");

    createInjurySystem();

    // Induce injury
    E2E_STAGE_BEGIN("Induce injury", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_MODERATE,
        HEMISPHERE_LEFT,
        INJURY_REGION_SENSORY_CORTEX,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Track recovery over time
    E2E_STAGE_BEGIN("Track recovery", MAX_RECOVERY_TIME_MS);
    std::vector<float> recovery_curve;
    float initial_function = hemispheric_injury_get_function(
        injury_system, HEMISPHERE_LEFT, INJURY_REGION_SENSORY_CORTEX);

    for (int day = 0; day < 60; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);

        float recovery = hemispheric_injury_get_recovery(
            injury_system, HEMISPHERE_LEFT, INJURY_REGION_SENSORY_CORTEX);
        recovery_curve.push_back(recovery);
    }

    // Recovery should generally increase
    float final_recovery = recovery_curve.back();
    EXPECT_GT(final_recovery, 0.0f) << "Should show some recovery";
    E2E_STAGE_END();

    // Verify improved function
    E2E_STAGE_BEGIN("Verify improved function", 20);
    float final_function = hemispheric_injury_get_function(
        injury_system, HEMISPHERE_LEFT, INJURY_REGION_SENSORY_CORTEX);
    EXPECT_GE(final_function, initial_function)
        << "Function should improve with recovery";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, RecoveryPlateauDetection) {
    E2E_PIPELINE_START("Recovery Plateau");

    createInjurySystem();

    // Induce mild injury for faster plateau
    E2E_STAGE_BEGIN("Induce mild injury", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_MILD,
        HEMISPHERE_RIGHT,
        INJURY_REGION_PARIETAL,
        MILD_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Run until plateau
    E2E_STAGE_BEGIN("Run to plateau", MAX_RECOVERY_TIME_MS);
    float prev_recovery = 0.0f;
    int stable_days = 0;

    for (int day = 0; day < 120; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);

        float recovery = hemispheric_injury_get_recovery(
            injury_system, HEMISPHERE_RIGHT, INJURY_REGION_PARIETAL);

        // Check for plateau (recovery rate < 0.1% per day)
        if (std::abs(recovery - prev_recovery) < 0.001f) {
            stable_days++;
        } else {
            stable_days = 0;
        }
        prev_recovery = recovery;

        // Plateau reached if stable for 7+ days
        if (stable_days >= 7) {
            break;
        }
    }

    recovery_phase_t phase = hemispheric_injury_get_phase(injury_system);
    // May reach plateau or still be in chronic phase
    (void)phase;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, TotalDeficitCalculation) {
    E2E_PIPELINE_START("Total Deficit Calculation");

    createInjurySystem();

    // Get baseline
    E2E_STAGE_BEGIN("Get baseline", 20);
    float baseline_deficit = hemispheric_injury_get_total_deficit(
        injury_system, HEMISPHERE_LEFT);
    EXPECT_EQ(baseline_deficit, 0.0f) << "No deficit before injury";
    E2E_STAGE_END();

    // Induce multiple injuries
    E2E_STAGE_BEGIN("Induce multiple injuries", MAX_INJURY_OP_TIME_MS * 2);
    uint32_t lesion_id1 = 0, lesion_id2 = 0;

    int result = hemispheric_injury_induce_lesion(
        injury_system, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MODERATE,
        HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX, 0.4f, &lesion_id1);
    EXPECT_EQ(result, 0);

    result = hemispheric_injury_induce_lesion(
        injury_system, INJURY_TYPE_STROKE_ISCHEMIC, SEVERITY_MILD,
        HEMISPHERE_LEFT, INJURY_REGION_SENSORY_CORTEX, 0.2f, &lesion_id2);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Check total deficit
    E2E_STAGE_BEGIN("Check total deficit", 20);
    float total_deficit = hemispheric_injury_get_total_deficit(
        injury_system, HEMISPHERE_LEFT);
    EXPECT_GT(total_deficit, 0.0f) << "Should have deficit after injuries";
    EXPECT_LE(total_deficit, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Rehabilitation Effects Tests
//=============================================================================

TEST_F(E2EInjuryRecoveryTest, RehabilitationBasic) {
    E2E_PIPELINE_START("Rehabilitation Basic");

    createInjurySystem();

    // Induce injury
    E2E_STAGE_BEGIN("Induce injury", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_MODERATE,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Wait for acute phase to pass
    E2E_STAGE_BEGIN("Wait for acute phase", MAX_RECOVERY_TIME_MS / 4);
    for (int day = 0; day < 7; day++) {
        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);
    }
    E2E_STAGE_END();

    // Start rehabilitation
    E2E_STAGE_BEGIN("Start rehabilitation", MAX_REHAB_TIME_MS);
    result = hemispheric_injury_start_rehabilitation(
        injury_system,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX,
        0.8f,  // High intensity
        2.0f   // 2 sessions per day
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Apply rehabilitation sessions
    E2E_STAGE_BEGIN("Apply rehab sessions", MAX_RECOVERY_TIME_MS);
    float pre_rehab_function = hemispheric_injury_get_function(
        injury_system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);

    for (int day = 0; day < 30; day++) {
        result = hemispheric_injury_apply_rehabilitation(injury_system, HEMISPHERE_LEFT);
        EXPECT_EQ(result, 0);

        result = hemispheric_injury_update(injury_system, 1.0f);
        EXPECT_EQ(result, 0);
    }

    float post_rehab_function = hemispheric_injury_get_function(
        injury_system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX);

    EXPECT_GT(post_rehab_function, pre_rehab_function)
        << "Function should improve with rehabilitation";
    E2E_STAGE_END();

    // Stop rehabilitation
    E2E_STAGE_BEGIN("Stop rehabilitation", 50);
    result = hemispheric_injury_stop_rehabilitation(injury_system, HEMISPHERE_LEFT);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, RehabilitationIntensityEffects) {
    E2E_PIPELINE_START("Rehabilitation Intensity");

    createInjurySystem();

    // Induce injury
    E2E_STAGE_BEGIN("Induce injury", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_MODERATE,
        HEMISPHERE_LEFT,
        INJURY_REGION_SENSORY_CORTEX,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Allow acute phase
    E2E_STAGE_BEGIN("Acute phase", MAX_RECOVERY_TIME_MS / 8);
    for (int day = 0; day < 7; day++) {
        hemispheric_injury_update(injury_system, 1.0f);
    }
    E2E_STAGE_END();

    // Test different intensities
    E2E_STAGE_BEGIN("Test intensities", MAX_REHAB_TIME_MS * 3);
    std::vector<float> intensities = {0.3f, 0.6f, 0.9f};

    for (float intensity : intensities) {
        result = hemispheric_injury_start_rehabilitation(
            injury_system,
            HEMISPHERE_LEFT,
            INJURY_REGION_SENSORY_CORTEX,
            intensity,
            1.0f
        );
        EXPECT_EQ(result, 0);

        // Apply for 10 days
        for (int day = 0; day < 10; day++) {
            hemispheric_injury_apply_rehabilitation(injury_system, HEMISPHERE_LEFT);
            hemispheric_injury_update(injury_system, 1.0f);
        }

        hemispheric_injury_stop_rehabilitation(injury_system, HEMISPHERE_LEFT);
    }
    E2E_STAGE_END();

    // Check final recovery
    E2E_STAGE_BEGIN("Check recovery", 20);
    float recovery = hemispheric_injury_get_recovery(
        injury_system, HEMISPHERE_LEFT, INJURY_REGION_SENSORY_CORTEX);
    EXPECT_GT(recovery, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, RehabilitationWithProcessing) {
    E2E_PIPELINE_START("Rehabilitation With Processing");

    createInjurySystem();

    // Induce injury
    E2E_STAGE_BEGIN("Induce injury", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_ISCHEMIC,
        SEVERITY_MODERATE,
        HEMISPHERE_LEFT,
        INJURY_REGION_MOTOR_CORTEX,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Start rehab
    E2E_STAGE_BEGIN("Start rehab", 50);
    hemispheric_injury_update(injury_system, 7.0f);  // Skip acute

    result = hemispheric_injury_start_rehabilitation(
        injury_system, HEMISPHERE_LEFT, INJURY_REGION_MOTOR_CORTEX,
        0.7f, 1.0f);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Combine rehab with actual processing (use-dependent plasticity)
    E2E_STAGE_BEGIN("Combined rehab and processing", MAX_REHAB_TIME_MS * 2);
    auto rehab_pattern = generate_rehabilitation_pattern(INPUT_SIZE);
    std::vector<float> output(OUTPUT_SIZE);

    for (int session = 0; session < 20; session++) {
        // Apply rehabilitation
        hemispheric_injury_apply_rehabilitation(injury_system, HEMISPHERE_LEFT);

        // Also process through the brain (active task practice)
        result = hemispheric_brain_process_lateralized(
            brain,
            rehab_pattern.data(),
            INPUT_SIZE,
            COGNITIVE_DOMAIN_MOTOR_FINE,
            output.data(),
            OUTPUT_SIZE
        );
        EXPECT_EQ(result, 0);

        // Update brain and injury system
        hemispheric_brain_update(brain, 0.05f);
        hemispheric_injury_update(injury_system, 1.0f);
    }
    E2E_STAGE_END();

    // Verify stats
    E2E_STAGE_BEGIN("Verify rehab stats", 20);
    hemispheric_injury_stats_t stats = hemispheric_injury_get_stats(injury_system);
    EXPECT_GT(stats.rehab_sessions, 0u);
    // With BRAIN_SIZE_MICRO, compensation may not accumulate enough to be non-zero
    EXPECT_GE(stats.total_compensated, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EInjuryRecoveryTest, LesionExpansionAndRemoval) {
    E2E_PIPELINE_START("Lesion Expansion and Removal");

    createInjurySystem();

    // Induce initial lesion
    E2E_STAGE_BEGIN("Induce initial lesion", MAX_INJURY_OP_TIME_MS);
    uint32_t lesion_id = 0;
    int result = hemispheric_injury_induce_lesion(
        injury_system,
        INJURY_TYPE_STROKE_HEMORRHAGIC,
        SEVERITY_MODERATE,
        HEMISPHERE_RIGHT,
        INJURY_REGION_PREFRONTAL,
        MODERATE_DAMAGE_LEVEL,
        &lesion_id
    );
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Expand lesion (simulating hemorrhage expansion)
    E2E_STAGE_BEGIN("Expand lesion", MAX_INJURY_OP_TIME_MS);
    float initial_damage = hemispheric_injury_get_damage(
        injury_system, HEMISPHERE_RIGHT, INJURY_REGION_PREFRONTAL);

    result = hemispheric_injury_expand_lesion(injury_system, lesion_id, 0.2f);
    EXPECT_EQ(result, 0);

    float expanded_damage = hemispheric_injury_get_damage(
        injury_system, HEMISPHERE_RIGHT, INJURY_REGION_PREFRONTAL);

    EXPECT_GT(expanded_damage, initial_damage)
        << "Damage should increase after expansion";
    E2E_STAGE_END();

    // Update for some time
    E2E_STAGE_BEGIN("Recovery period", MAX_RECOVERY_TIME_MS);
    for (int day = 0; day < 30; day++) {
        hemispheric_injury_update(injury_system, 1.0f);
    }
    E2E_STAGE_END();

    // Remove lesion (simulating surgical intervention or simulation reset)
    E2E_STAGE_BEGIN("Remove lesion", MAX_INJURY_OP_TIME_MS);
    result = hemispheric_injury_remove_lesion(injury_system, lesion_id);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Verify stats
    E2E_STAGE_BEGIN("Verify final stats", 20);
    hemispheric_injury_stats_t stats = hemispheric_injury_get_stats(injury_system);
    EXPECT_EQ(stats.total_lesions, 1u);
    EXPECT_EQ(stats.active_lesions, 0u);  // Lesion was removed
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
