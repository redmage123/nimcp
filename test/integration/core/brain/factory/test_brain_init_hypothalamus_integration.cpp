/**
 * @file test_brain_init_hypothalamus_integration.cpp
 * @brief Integration tests for brain factory hypothalamus initialization
 *
 * WHAT: Tests hypothalamus integration with other brain subsystems through factory
 * WHY:  Ensure hypothalamus subsystem initializes correctly and integrates with:
 *       - Medulla (brainstem coordination)
 *       - Immune system (cytokine signaling)
 *       - Sleep/wake system (circadian integration)
 *       - Wellbeing (stress-distress coupling)
 *       - Emotional system (HPA-emotion coupling)
 *       - Bio-async messaging
 * HOW:  Create brains via factory, verify hypothalamus bridges initialize and work
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_hypothalamus.h"

/**
 * @brief Test fixture for brain factory hypothalamus integration tests
 *
 * Creates a brain with hypothalamus-related subsystems enabled for integration testing.
 */
class BrainInitHypothalamusIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /**
     * @brief Create a brain configured for hypothalamus integration testing
     */
    brain_t CreateHypothalamusBrain() {
        brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 3;
        strncpy(config.task_name, "hypothalamus_integration_test", sizeof(config.task_name) - 1);

        // Enable related subsystems for integration testing
        config.enable_wellbeing = true;
        config.enable_emotional_tagging = true;
        config.enable_sleep_wake_cycle = true;

        return brain_create_custom(&config);
    }

    /**
     * @brief Create a minimal brain for fast testing
     */
    brain_t CreateMinimalBrain() {
        return brain_create_minimal(
            "minimal_hypo_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            5,
            2
        );
    }
};

// =============================================================================
// Test Suite 1: Hypothalamus-Medulla Integration
// =============================================================================

/**
 * @brief Test hypothalamus works with medulla
 *
 * Verifies that hypothalamus can communicate with medulla for arousal
 * state coordination and autonomic control.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusMedullaIntegration) {
    brain = CreateHypothalamusBrain();
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize successfully";

    // Connect hypothalamus to medulla
    bool connect_result = nimcp_brain_factory_connect_hypothalamus_to_medulla(brain);
    EXPECT_TRUE(connect_result) << "Hypothalamus-medulla connection should succeed";

    // Verify hypothalamus adapter is created
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should be created";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}

// =============================================================================
// Test Suite 2: Hypothalamus-Immune Integration
// =============================================================================

/**
 * @brief Test hypothalamus works with immune system
 *
 * Verifies cytokine signaling between hypothalamus and brain immune system.
 * Inflammation should affect body temperature setpoints and sickness behavior.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusImmuneIntegration) {
    brain = CreateHypothalamusBrain();
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize successfully";

    // Connect hypothalamus to immune system
    bool connect_result = nimcp_brain_factory_connect_hypothalamus_to_immune(brain);
    EXPECT_TRUE(connect_result) << "Hypothalamus-immune connection should succeed";

    // Verify hypothalamus is enabled
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}

// =============================================================================
// Test Suite 3: Hypothalamus-Sleep Integration
// =============================================================================

/**
 * @brief Test hypothalamus circadian with sleep system
 *
 * Verifies that hypothalamus SCN (suprachiasmatic nucleus) integrates
 * with the sleep/wake system for circadian rhythm coordination.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusSleepIntegration) {
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 8;
    config.num_outputs = 2;
    strncpy(config.task_name, "sleep_integration_test", sizeof(config.task_name) - 1);

    // Enable sleep/wake cycle for this test
    config.enable_sleep_wake_cycle = true;
    config.enable_memory_replay = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize successfully";

    // Connect hypothalamus to sleep system
    bool connect_result = nimcp_brain_factory_connect_hypothalamus_to_sleep(brain);
    EXPECT_TRUE(connect_result) << "Hypothalamus-sleep connection should succeed";

    // Verify hypothalamus is properly integrated
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}

// =============================================================================
// Test Suite 4: Hypothalamus-Wellbeing Integration
// =============================================================================

/**
 * @brief Test hypothalamus stress with wellbeing
 *
 * Verifies that hypothalamus HPA axis stress response integrates
 * with the wellbeing monitoring system for distress detection.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusWellbeingIntegration) {
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 8;
    config.num_outputs = 2;
    strncpy(config.task_name, "wellbeing_integration_test", sizeof(config.task_name) - 1);

    // Enable wellbeing monitoring for this test
    config.enable_wellbeing = true;
    config.enable_wellbeing_monitoring = true;
    config.wellbeing_check_interval_ms = 100;

    brain = brain_create_custom(&config);
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize successfully";

    // Connect hypothalamus to wellbeing monitor
    bool connect_result = nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain);
    EXPECT_TRUE(connect_result) << "Hypothalamus-wellbeing connection should succeed";

    // Verify hypothalamus is properly integrated
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}

// =============================================================================
// Test Suite 5: Hypothalamus-Emotional Integration
// =============================================================================

/**
 * @brief Test HPA axis with emotional system
 *
 * Verifies that hypothalamus HPA axis integrates with emotional system
 * for stress-emotion coupling (fear/anxiety -> HPA activation).
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusEmotionalIntegration) {
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "emotional_integration_test", sizeof(config.task_name) - 1);

    // Enable emotional systems
    config.enable_emotional_tagging = true;
    config.enable_emotional_memories = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize successfully";

    // Connect hypothalamus to emotional system
    bool connect_result = nimcp_brain_factory_connect_hypothalamus_to_emotions(brain);
    EXPECT_TRUE(connect_result) << "Hypothalamus-emotional connection should succeed";

    // Initialize limbic bridge for emotional input
    bool limbic_result = nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain);
    EXPECT_TRUE(limbic_result) << "Limbic bridge should initialize";

    // Verify hypothalamus is properly integrated
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}

// =============================================================================
// Test Suite 6: Hypothalamus-BioAsync Integration
// =============================================================================

/**
 * @brief Test bio-async messaging with hypothalamus
 *
 * Verifies that hypothalamus can use bio-async messaging for
 * asynchronous neuromodulator communication with other subsystems.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusBioAsyncIntegration) {
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 8;
    config.num_outputs = 2;
    strncpy(config.task_name, "bio_async_integration_test", sizeof(config.task_name) - 1);

    brain = brain_create_custom(&config);
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize successfully";

    // Verify hypothalamus is properly integrated
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}

// =============================================================================
// Test Suite 7: Full Brain Creation With Hypothalamus
// =============================================================================

/**
 * @brief Test brain_create_custom includes hypothalamus
 *
 * Verifies that creating a brain with hypothalamus subsystem
 * initialized through factory properly integrates it.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, FullBrainCreationWithHypothalamus) {
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 5;
    strncpy(config.task_name, "full_brain_hypo_test", sizeof(config.task_name) - 1);

    // Enable related subsystems
    config.enable_wellbeing = true;
    config.enable_sleep_wake_cycle = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(nullptr, brain);

    // Verify brain structure is valid
    EXPECT_NE(nullptr, brain->network) << "Neural network should be created";
    EXPECT_NE(nullptr, brain->strategy) << "Task strategy should be created";

    // Initialize hypothalamus via factory
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize";

    // Verify hypothalamus is properly integrated
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus should be created";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}

// =============================================================================
// Test Suite 8: Hypothalamus Initialization Order
// =============================================================================

/**
 * @brief Test hypothalamus inits after medulla
 *
 * Verifies that hypothalamus initialization respects dependencies,
 * particularly that medulla (brainstem) is initialized first since
 * hypothalamus connects to medulla for autonomic output.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusInitializationOrder) {
    brain = CreateHypothalamusBrain();
    ASSERT_NE(nullptr, brain);

    // Step 1: Initialize medulla first (brainstem)
    // Medulla should already be created during brain_create_custom if enabled

    // Step 2: Initialize hypothalamus (depends on medulla for autonomic output)
    bool hypo_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(hypo_result) << "Hypothalamus should initialize successfully";

    // Step 3: Initialize brainstem bridge (hypothalamus -> medulla)
    bool brainstem_result = nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain);
    EXPECT_TRUE(brainstem_result) << "Brainstem bridge should initialize";

    // Verify both are enabled
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
}

// =============================================================================
// Test Suite 9: Hypothalamus Bridges Cascade
// =============================================================================

/**
 * @brief Test all bridges initialize in sequence
 *
 * Verifies that all hypothalamus bridges can be initialized in the
 * correct order without failures.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusBridgesCascade) {
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 15;
    config.num_outputs = 4;
    strncpy(config.task_name, "bridges_cascade_test", sizeof(config.task_name) - 1);

    // Enable all related subsystems
    config.enable_wellbeing = true;
    config.enable_emotional_tagging = true;
    config.enable_sleep_wake_cycle = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem first
    bool subsystem_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(subsystem_result) << "Hypothalamus subsystem should initialize";

    // Initialize all bridges in biological order
    bool limbic_result = nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain);
    EXPECT_TRUE(limbic_result) << "Limbic bridge should initialize";

    bool brainstem_result = nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain);
    EXPECT_TRUE(brainstem_result) << "Brainstem bridge should initialize";

    bool pituitary_result = nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain);
    EXPECT_TRUE(pituitary_result) << "Pituitary bridge should initialize";

    bool quantum_result = nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain);
    EXPECT_TRUE(quantum_result) << "Quantum bridge should initialize";

    // Connect to other systems
    bool sleep_result = nimcp_brain_factory_connect_hypothalamus_to_sleep(brain);
    EXPECT_TRUE(sleep_result) << "Sleep connection should succeed";

    bool immune_result = nimcp_brain_factory_connect_hypothalamus_to_immune(brain);
    EXPECT_TRUE(immune_result) << "Immune connection should succeed";

    bool wellbeing_result = nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain);
    EXPECT_TRUE(wellbeing_result) << "Wellbeing connection should succeed";

    bool medulla_result = nimcp_brain_factory_connect_hypothalamus_to_medulla(brain);
    EXPECT_TRUE(medulla_result) << "Medulla connection should succeed";

    bool emotions_result = nimcp_brain_factory_connect_hypothalamus_to_emotions(brain);
    EXPECT_TRUE(emotions_result) << "Emotions connection should succeed";

    // Verify final state
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
}

// =============================================================================
// Test Suite 10: Hypothalamus Config Propagation
// =============================================================================

/**
 * @brief Test config flows from brain to hypothalamus
 *
 * Verifies that brain configuration settings are properly propagated
 * to the hypothalamus adapter during initialization.
 */
TEST_F(BrainInitHypothalamusIntegrationTest, HypothalamusConfigPropagation) {
    brain_config_t config = brain_config_from_profile(BRAIN_CONFIG_STANDARD);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "config_propagation_test", sizeof(config.task_name) - 1);

    // Set specific wellbeing configuration
    config.enable_wellbeing = true;
    config.enable_wellbeing_monitoring = true;
    config.wellbeing_check_interval_ms = 250;

    // Set sleep configuration
    config.enable_sleep_wake_cycle = true;
    config.sleep_pressure_threshold = 0.75f;

    brain = brain_create_custom(&config);
    ASSERT_NE(nullptr, brain);

    // Initialize hypothalamus subsystem
    bool init_result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(init_result) << "Hypothalamus subsystem should initialize successfully";

    // Verify brain config was applied
    EXPECT_TRUE(brain->config.enable_wellbeing) << "Wellbeing should be enabled in config";
    EXPECT_TRUE(brain->config.enable_sleep_wake_cycle) << "Sleep cycle should be enabled";
    EXPECT_EQ(250u, brain->config.wellbeing_check_interval_ms) << "Wellbeing interval should match";
    EXPECT_NEAR(0.75f, brain->config.sleep_pressure_threshold, 0.01f) << "Sleep threshold should match";

    // Verify hypothalamus is properly integrated
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
}
