//=============================================================================
// test_brain_init_hypothalamus.cpp - Hypothalamus Brain Init Unit Tests
//=============================================================================
/**
 * @file test_brain_init_hypothalamus.cpp
 * @brief GoogleTest unit tests for brain factory hypothalamus initialization
 *
 * Tests the hypothalamus brain initialization functions:
 * - nimcp_brain_factory_init_hypothalamus_subsystem
 * - nimcp_brain_factory_init_hypothalamus_limbic_bridge
 * - nimcp_brain_factory_init_hypothalamus_brainstem_bridge
 * - nimcp_brain_factory_init_hypothalamus_pituitary_bridge
 * - nimcp_brain_factory_init_hypothalamus_quantum_bridge
 * - nimcp_brain_factory_connect_hypothalamus_to_sleep
 * - nimcp_brain_factory_connect_hypothalamus_to_immune
 * - nimcp_brain_factory_connect_hypothalamus_to_wellbeing
 * - nimcp_brain_factory_connect_hypothalamus_to_medulla
 * - nimcp_brain_factory_connect_hypothalamus_to_emotions
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/factory/init/nimcp_brain_init_hypothalamus.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainInitHypothalamusTest : public ::testing::Test {
protected:
    brain_t test_brain = nullptr;

    void SetUp() override {
        // Create a minimal test brain
        test_brain = brain_create(
            "hypothalamus_test_brain",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,
            4
        );
        ASSERT_NE(test_brain, nullptr);
    }

    void TearDown() override {
        if (test_brain) {
            brain_destroy(test_brain);
            test_brain = nullptr;
        }
    }
};

//=============================================================================
// Subsystem Initialization Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, InitHypothalamusSubsystem_NullBrain_ReturnsFalse) {
    bool result = nimcp_brain_factory_init_hypothalamus_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitHypothalamusTest, InitHypothalamusSubsystem_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->hypothalamus_enabled = false;

    bool result = nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(b->hypothalamus, nullptr);
    EXPECT_TRUE(b->hypothalamus_enabled);
}

TEST_F(BrainInitHypothalamusTest, InitHypothalamusSubsystem_AlreadyInitialized_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // First initialization
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);
    void* first_ptr = b->hypothalamus;

    // Second initialization should be idempotent
    bool result = nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(b->hypothalamus, first_ptr);
}

//=============================================================================
// Limbic Bridge Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, InitLimbicBridge_NullBrain_ReturnsFalse) {
    bool result = nimcp_brain_factory_init_hypothalamus_limbic_bridge(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitHypothalamusTest, InitLimbicBridge_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_init_hypothalamus_limbic_bridge(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, InitLimbicBridge_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Should return true even without hypothalamus (deferred initialization)
    bool result = nimcp_brain_factory_init_hypothalamus_limbic_bridge(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Brainstem Bridge Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, InitBrainstemBridge_NullBrain_ReturnsFalse) {
    bool result = nimcp_brain_factory_init_hypothalamus_brainstem_bridge(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitHypothalamusTest, InitBrainstemBridge_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_init_hypothalamus_brainstem_bridge(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, InitBrainstemBridge_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Should return true even without hypothalamus (deferred initialization)
    bool result = nimcp_brain_factory_init_hypothalamus_brainstem_bridge(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Pituitary Bridge Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, InitPituitaryBridge_NullBrain_ReturnsFalse) {
    bool result = nimcp_brain_factory_init_hypothalamus_pituitary_bridge(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitHypothalamusTest, InitPituitaryBridge_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_init_hypothalamus_pituitary_bridge(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, InitPituitaryBridge_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Should return true even without hypothalamus (deferred initialization)
    bool result = nimcp_brain_factory_init_hypothalamus_pituitary_bridge(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Quantum Bridge Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, InitQuantumBridge_NullBrain_ReturnsFalse) {
    bool result = nimcp_brain_factory_init_hypothalamus_quantum_bridge(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitHypothalamusTest, InitQuantumBridge_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->hypothalamus_quantum_bridge = nullptr;
    b->quantum_reasoning_enabled = true;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_init_hypothalamus_quantum_bridge(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, InitQuantumBridge_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->hypothalamus_quantum_bridge = nullptr;

    // Should return true even without hypothalamus (deferred initialization)
    bool result = nimcp_brain_factory_init_hypothalamus_quantum_bridge(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, InitQuantumBridge_AlreadyInitialized_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->quantum_reasoning_enabled = true;

    // Initialize hypothalamus and quantum bridge
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);
    nimcp_brain_factory_init_hypothalamus_quantum_bridge(test_brain);
    void* first_ptr = b->hypothalamus_quantum_bridge;

    // Second initialization should be idempotent
    bool result = nimcp_brain_factory_init_hypothalamus_quantum_bridge(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(b->hypothalamus_quantum_bridge, first_ptr);
}

TEST_F(BrainInitHypothalamusTest, InitQuantumBridge_QuantumDisabled_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->hypothalamus_quantum_bridge = nullptr;
    b->quantum_reasoning_enabled = false;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    // Should return true but not create bridge when quantum is disabled
    bool result = nimcp_brain_factory_init_hypothalamus_quantum_bridge(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Connect to Sleep Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, ConnectToSleep_NullBrain_ReturnsTrue) {
    // Connect functions return true for null brain (non-fatal)
    bool result = nimcp_brain_factory_connect_hypothalamus_to_sleep(nullptr);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToSleep_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_connect_hypothalamus_to_sleep(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToSleep_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    bool result = nimcp_brain_factory_connect_hypothalamus_to_sleep(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Connect to Immune Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, ConnectToImmune_NullBrain_ReturnsTrue) {
    // Connect functions return true for null brain (non-fatal)
    bool result = nimcp_brain_factory_connect_hypothalamus_to_immune(nullptr);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToImmune_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_connect_hypothalamus_to_immune(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToImmune_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    bool result = nimcp_brain_factory_connect_hypothalamus_to_immune(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Connect to Wellbeing Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, ConnectToWellbeing_NullBrain_ReturnsTrue) {
    // Connect functions return true for null brain (non-fatal)
    bool result = nimcp_brain_factory_connect_hypothalamus_to_wellbeing(nullptr);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToWellbeing_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_connect_hypothalamus_to_wellbeing(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToWellbeing_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    bool result = nimcp_brain_factory_connect_hypothalamus_to_wellbeing(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Connect to Medulla Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, ConnectToMedulla_NullBrain_ReturnsTrue) {
    // Connect functions return true for null brain (non-fatal)
    bool result = nimcp_brain_factory_connect_hypothalamus_to_medulla(nullptr);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToMedulla_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_connect_hypothalamus_to_medulla(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToMedulla_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    bool result = nimcp_brain_factory_connect_hypothalamus_to_medulla(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// Connect to Emotions Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, ConnectToEmotions_NullBrain_ReturnsTrue) {
    // Connect functions return true for null brain (non-fatal)
    bool result = nimcp_brain_factory_connect_hypothalamus_to_emotions(nullptr);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToEmotions_ValidBrain_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // Initialize hypothalamus first
    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    bool result = nimcp_brain_factory_connect_hypothalamus_to_emotions(test_brain);
    EXPECT_TRUE(result);
}

TEST_F(BrainInitHypothalamusTest, ConnectToEmotions_NoHypothalamus_ReturnsTrue) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    bool result = nimcp_brain_factory_connect_hypothalamus_to_emotions(test_brain);
    EXPECT_TRUE(result);
}

//=============================================================================
// State Verification Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, HypothalamusEnabledAfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->hypothalamus_enabled = false;

    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    EXPECT_TRUE(b->hypothalamus_enabled);
}

TEST_F(BrainInitHypothalamusTest, HypothalamusAdapterCreatedAfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    EXPECT_NE(b->hypothalamus, nullptr);
}

TEST_F(BrainInitHypothalamusTest, TimestampInitializedAfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->last_hypothalamus_update_us = 12345;

    nimcp_brain_factory_init_hypothalamus_subsystem(test_brain);

    EXPECT_EQ(b->last_hypothalamus_update_us, 0u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BrainInitHypothalamusTest, FullLifecycle) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->hypothalamus_enabled = false;

    // Initialize subsystem
    ASSERT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(test_brain));
    EXPECT_NE(b->hypothalamus, nullptr);
    EXPECT_TRUE(b->hypothalamus_enabled);

    // All bridges should succeed (called internally by subsystem init)
    // Re-calling them should also succeed (idempotent)
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_limbic_bridge(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_brainstem_bridge(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_pituitary_bridge(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_quantum_bridge(test_brain));

    // All connect functions should succeed
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_sleep(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_immune(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_wellbeing(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_medulla(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(test_brain));
}

TEST_F(BrainInitHypothalamusTest, MultipleInitializationsIdempotent) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;

    // First init
    ASSERT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(test_brain));
    void* first_ptr = b->hypothalamus;

    // Multiple subsequent inits should be idempotent
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(test_brain));
        EXPECT_EQ(b->hypothalamus, first_ptr);
        EXPECT_TRUE(b->hypothalamus_enabled);
    }
}

TEST_F(BrainInitHypothalamusTest, BridgesBeforeSubsystemInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->hypothalamus = nullptr;
    b->hypothalamus_enabled = false;

    // All bridge/connect functions should succeed even before subsystem init
    // (they defer or return non-fatal success)
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_limbic_bridge(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_brainstem_bridge(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_pituitary_bridge(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_quantum_bridge(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_sleep(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_immune(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_wellbeing(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_medulla(test_brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(test_brain));
}
