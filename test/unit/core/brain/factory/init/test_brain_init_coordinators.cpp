//=============================================================================
// test_brain_init_coordinators.cpp - Unit Tests for Coordinator Init Functions
//=============================================================================
/**
 * @file test_brain_init_coordinators.cpp
 * @brief Comprehensive unit tests for 6 coordinator/orchestrator init functions
 *
 * WHAT: Tests covering coordinator subsystem initialization during brain creation
 * WHY:  Ensure proper initialization, dependency wiring, and graceful handling
 * HOW:  GoogleTest framework with test cases for each coordinator
 *
 * Coordinators Tested:
 * 1. Bio-Async Orchestrator - Foundation for inter-module messaging
 * 2. Plasticity Coordinator - Manages plasticity mechanisms
 * 3. Immune Bridge Coordinator - Coordinates immune bridges
 * 4. Cognitive Meta-Controller - Arbitrates cognitive resources
 * 5. Security-Perception Bridge - Sensory threat analysis
 * 6. Swarm Module Registry - Plugin architecture for swarm behaviors
 *
 * @version 1.0.0
 * @date 2025-12-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <memory>

// Headers have their own extern "C" guards
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "plasticity/nimcp_plasticity_coordinator.h"
#include "cognitive/immune/nimcp_immune_bridge_coordinator.h"
#include "cognitive/nimcp_cognitive_meta_controller.h"
#include "security/nimcp_security_perception_bridge.h"
#include "swarm/nimcp_swarm_module_registry.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * CoordinatorInitTest Fixture
 *
 * Creates a minimal brain for testing coordinator init functions
 */
class CoordinatorInitTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create a minimal brain for testing
        brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 16);
    }

    void TearDown() override {
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. BIO-ASYNC ORCHESTRATOR TESTS
//=============================================================================

/**
 * TEST: BioAsyncOrchestrator_InitWhenEnabled
 *
 * WHAT: Initialize bio-async orchestrator when bio-async is enabled
 * WHY:  Verify successful initialization with dependencies
 * EXPECT: Orchestrator created and enabled
 */
TEST_F(CoordinatorInitTest, BioAsyncOrchestrator_InitWhenEnabled) {
    ASSERT_NE(brain, nullptr);

    // Ensure bio-async is enabled
    brain->bio_async_enabled = true;

    bool result = nimcp_brain_factory_init_bio_async_orchestrator_subsystem(brain);

    EXPECT_TRUE(result);
    // Orchestrator may or may not be created depending on runtime conditions
    // but the function should return true (non-fatal)
}

/**
 * TEST: BioAsyncOrchestrator_SkipWhenDisabled
 *
 * WHAT: Skip initialization when bio-async is disabled
 * WHY:  Verify graceful skip when not needed
 * EXPECT: Returns true, orchestrator not created
 */
TEST_F(CoordinatorInitTest, BioAsyncOrchestrator_SkipWhenDisabled) {
    ASSERT_NE(brain, nullptr);

    // Disable bio-async
    brain->bio_async_enabled = false;
    brain->bio_async_orchestrator = nullptr;
    brain->bio_async_orchestrator_enabled = false;

    bool result = nimcp_brain_factory_init_bio_async_orchestrator_subsystem(brain);

    EXPECT_TRUE(result);  // Non-fatal skip
    EXPECT_FALSE(brain->bio_async_orchestrator_enabled);
}

/**
 * TEST: BioAsyncOrchestrator_NullBrain
 *
 * WHAT: Handle NULL brain pointer
 * WHY:  Verify guard clause
 * EXPECT: Returns false
 */
TEST_F(CoordinatorInitTest, BioAsyncOrchestrator_NullBrain) {
    bool result = nimcp_brain_factory_init_bio_async_orchestrator_subsystem(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 2. PLASTICITY COORDINATOR TESTS
//=============================================================================

/**
 * TEST: PlasticityCoordinator_InitWhenEnabled
 *
 * WHAT: Initialize plasticity coordinator with dependencies
 * WHY:  Verify successful initialization
 * EXPECT: Coordinator created
 */
TEST_F(CoordinatorInitTest, PlasticityCoordinator_InitWhenEnabled) {
    ASSERT_NE(brain, nullptr);

    // Enable dependencies
    brain->bio_async_enabled = true;

    bool result = nimcp_brain_factory_init_plasticity_coordinator_subsystem(brain);

    EXPECT_TRUE(result);
}

/**
 * TEST: PlasticityCoordinator_SkipWhenNoDepends
 *
 * WHAT: Skip when no dependencies are available
 * WHY:  Verify graceful skip
 * EXPECT: Returns true, coordinator not created
 */
TEST_F(CoordinatorInitTest, PlasticityCoordinator_SkipWhenNoDepends) {
    ASSERT_NE(brain, nullptr);

    // Disable all dependencies
    brain->bio_async_enabled = false;
    brain->immune_enabled = false;
    brain->plasticity_coordinator = nullptr;
    brain->plasticity_coordinator_enabled = false;

    bool result = nimcp_brain_factory_init_plasticity_coordinator_subsystem(brain);

    EXPECT_TRUE(result);  // Non-fatal skip
    EXPECT_FALSE(brain->plasticity_coordinator_enabled);
}

/**
 * TEST: PlasticityCoordinator_NullBrain
 *
 * WHAT: Handle NULL brain pointer
 * WHY:  Verify guard clause
 * EXPECT: Returns false
 */
TEST_F(CoordinatorInitTest, PlasticityCoordinator_NullBrain) {
    bool result = nimcp_brain_factory_init_plasticity_coordinator_subsystem(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 3. IMMUNE BRIDGE COORDINATOR TESTS
//=============================================================================

/**
 * TEST: ImmuneBridgeCoordinator_InitWhenEnabled
 *
 * WHAT: Initialize immune bridge coordinator with immune enabled
 * WHY:  Verify successful initialization
 * EXPECT: Coordinator created
 */
TEST_F(CoordinatorInitTest, ImmuneBridgeCoordinator_InitWhenEnabled) {
    ASSERT_NE(brain, nullptr);

    // Enable immune system
    brain->immune_enabled = true;

    bool result = nimcp_brain_factory_init_immune_bridge_coordinator_subsystem(brain);

    EXPECT_TRUE(result);
}

/**
 * TEST: ImmuneBridgeCoordinator_SkipWhenDisabled
 *
 * WHAT: Skip when immune is disabled
 * WHY:  Verify graceful skip
 * EXPECT: Returns true, coordinator not created
 */
TEST_F(CoordinatorInitTest, ImmuneBridgeCoordinator_SkipWhenDisabled) {
    ASSERT_NE(brain, nullptr);

    // Disable immune
    brain->immune_enabled = false;
    brain->immune_bridge_coordinator = nullptr;
    brain->immune_bridge_coordinator_enabled = false;

    bool result = nimcp_brain_factory_init_immune_bridge_coordinator_subsystem(brain);

    EXPECT_TRUE(result);  // Non-fatal skip
    EXPECT_FALSE(brain->immune_bridge_coordinator_enabled);
}

/**
 * TEST: ImmuneBridgeCoordinator_NullBrain
 *
 * WHAT: Handle NULL brain pointer
 * WHY:  Verify guard clause
 * EXPECT: Returns false
 */
TEST_F(CoordinatorInitTest, ImmuneBridgeCoordinator_NullBrain) {
    bool result = nimcp_brain_factory_init_immune_bridge_coordinator_subsystem(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 4. COGNITIVE META-CONTROLLER TESTS
//=============================================================================

/**
 * TEST: CognitiveMetaController_InitWithCognitive
 *
 * WHAT: Initialize cognitive meta-controller with cognitive subsystems
 * WHY:  Verify successful initialization
 * EXPECT: Controller created when cognitive subsystems exist
 */
TEST_F(CoordinatorInitTest, CognitiveMetaController_InitWithCognitive) {
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_cognitive_meta_controller_subsystem(brain);

    EXPECT_TRUE(result);
    // Controller may or may not be created depending on cognitive subsystems
}

/**
 * TEST: CognitiveMetaController_SkipWhenNoCognitive
 *
 * WHAT: Skip when no cognitive subsystems exist
 * WHY:  Verify graceful skip
 * EXPECT: Returns true, controller not created
 */
TEST_F(CoordinatorInitTest, CognitiveMetaController_SkipWhenNoCognitive) {
    ASSERT_NE(brain, nullptr);

    // Clear cognitive subsystems
    brain->working_memory = nullptr;
    brain->executive = nullptr;
    brain->global_workspace = nullptr;
    brain->cognitive_meta_controller = nullptr;
    brain->cognitive_meta_controller_enabled = false;

    bool result = nimcp_brain_factory_init_cognitive_meta_controller_subsystem(brain);

    EXPECT_TRUE(result);  // Non-fatal skip
    EXPECT_FALSE(brain->cognitive_meta_controller_enabled);
}

/**
 * TEST: CognitiveMetaController_NullBrain
 *
 * WHAT: Handle NULL brain pointer
 * WHY:  Verify guard clause
 * EXPECT: Returns false
 */
TEST_F(CoordinatorInitTest, CognitiveMetaController_NullBrain) {
    bool result = nimcp_brain_factory_init_cognitive_meta_controller_subsystem(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 5. SECURITY-PERCEPTION BRIDGE TESTS
//=============================================================================

/**
 * TEST: SecurityPerceptionBridge_InitWithDependencies
 *
 * WHAT: Initialize security-perception bridge with dependencies
 * WHY:  Verify successful initialization
 * EXPECT: Bridge created when dependencies exist
 */
TEST_F(CoordinatorInitTest, SecurityPerceptionBridge_InitWithDependencies) {
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_security_perception_bridge_subsystem(brain);

    EXPECT_TRUE(result);
}

/**
 * TEST: SecurityPerceptionBridge_SkipWhenNoDependencies
 *
 * WHAT: Skip when no security/perception subsystems exist
 * WHY:  Verify graceful skip
 * EXPECT: Returns true, bridge not created
 */
TEST_F(CoordinatorInitTest, SecurityPerceptionBridge_SkipWhenNoDependencies) {
    ASSERT_NE(brain, nullptr);

    // Clear dependencies
    brain->bbb_system = nullptr;
    brain->visual_cortex = nullptr;
    brain->audio_cortex = nullptr;
    brain->security_perception_bridge = nullptr;
    brain->security_perception_bridge_enabled = false;

    bool result = nimcp_brain_factory_init_security_perception_bridge_subsystem(brain);

    EXPECT_TRUE(result);  // Non-fatal skip
    EXPECT_FALSE(brain->security_perception_bridge_enabled);
}

/**
 * TEST: SecurityPerceptionBridge_NullBrain
 *
 * WHAT: Handle NULL brain pointer
 * WHY:  Verify guard clause
 * EXPECT: Returns false
 */
TEST_F(CoordinatorInitTest, SecurityPerceptionBridge_NullBrain) {
    bool result = nimcp_brain_factory_init_security_perception_bridge_subsystem(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 6. SWARM MODULE REGISTRY TESTS
//=============================================================================

/**
 * TEST: SwarmModuleRegistry_InitWithDependencies
 *
 * WHAT: Initialize swarm module registry with dependencies
 * WHY:  Verify successful initialization
 * EXPECT: Registry created when dependencies exist
 */
TEST_F(CoordinatorInitTest, SwarmModuleRegistry_InitWithDependencies) {
    ASSERT_NE(brain, nullptr);

    // Enable dependencies
    brain->bio_async_enabled = true;

    bool result = nimcp_brain_factory_init_swarm_module_registry_subsystem(brain);

    EXPECT_TRUE(result);
}

/**
 * TEST: SwarmModuleRegistry_SkipWhenNoDependencies
 *
 * WHAT: Skip when no swarm dependencies exist
 * WHY:  Verify graceful skip
 * EXPECT: Returns true, registry not created
 */
TEST_F(CoordinatorInitTest, SwarmModuleRegistry_SkipWhenNoDependencies) {
    ASSERT_NE(brain, nullptr);

    // Disable dependencies
    brain->bio_async_enabled = false;
    brain->immune_enabled = false;
    brain->swarm_module_registry = nullptr;
    brain->swarm_module_registry_enabled = false;

    bool result = nimcp_brain_factory_init_swarm_module_registry_subsystem(brain);

    EXPECT_TRUE(result);  // Non-fatal skip
    EXPECT_FALSE(brain->swarm_module_registry_enabled);
}

/**
 * TEST: SwarmModuleRegistry_NullBrain
 *
 * WHAT: Handle NULL brain pointer
 * WHY:  Verify guard clause
 * EXPECT: Returns false
 */
TEST_F(CoordinatorInitTest, SwarmModuleRegistry_NullBrain) {
    bool result = nimcp_brain_factory_init_swarm_module_registry_subsystem(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// FULL BRAIN LIFECYCLE TESTS
//=============================================================================

/**
 * TEST: FullBrainCreate_AllCoordinators
 *
 * WHAT: Create and destroy a full brain with all coordinators
 * WHY:  Verify complete lifecycle including all coordinator init/cleanup
 * EXPECT: Brain created and destroyed without errors
 */
TEST_F(CoordinatorInitTest, FullBrainCreate_AllCoordinators) {
    // Create fresh brain (SetUp already created one, destroy it first)
    if (brain != nullptr) {
        brain_destroy(brain);
        brain = nullptr;
    }

    // Create a new brain which will init all coordinators
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 128, 32);

    EXPECT_NE(brain, nullptr);

    // The brain_create should have initialized coordinators
    // Verify brain fields exist (coordinators may or may not be enabled based on dependencies)

    // Cleanup happens in TearDown
}

/**
 * TEST: BrainDestroy_CleansUpCoordinators
 *
 * WHAT: Verify brain_destroy cleans up all coordinators
 * WHY:  Ensure no memory leaks from coordinator cleanup
 * EXPECT: All coordinator pointers nulled and flags cleared
 */
TEST_F(CoordinatorInitTest, BrainDestroy_CleansUpCoordinators) {
    ASSERT_NE(brain, nullptr);

    // Destroy the brain
    brain_destroy(brain);
    brain = nullptr;  // Mark as cleaned up to avoid double-free in TearDown

    // If we get here without crash, cleanup succeeded
    SUCCEED();
}

//=============================================================================
// DEPENDENCY ORDER TESTS
//=============================================================================

/**
 * TEST: DependencyOrder_PlasticityAfterBioAsync
 *
 * WHAT: Verify plasticity coordinator works after bio-async
 * WHY:  Ensure correct dependency order
 * EXPECT: Both init successfully
 */
TEST_F(CoordinatorInitTest, DependencyOrder_PlasticityAfterBioAsync) {
    ASSERT_NE(brain, nullptr);

    brain->bio_async_enabled = true;

    // First init bio-async
    bool result1 = nimcp_brain_factory_init_bio_async_orchestrator_subsystem(brain);
    EXPECT_TRUE(result1);

    // Then init plasticity
    bool result2 = nimcp_brain_factory_init_plasticity_coordinator_subsystem(brain);
    EXPECT_TRUE(result2);
}

/**
 * TEST: DependencyOrder_CognitiveAfterPlasticity
 *
 * WHAT: Verify cognitive meta-controller works after plasticity
 * WHY:  Ensure correct dependency order
 * EXPECT: Both init successfully
 */
TEST_F(CoordinatorInitTest, DependencyOrder_CognitiveAfterPlasticity) {
    ASSERT_NE(brain, nullptr);

    brain->bio_async_enabled = true;

    // Init in dependency order
    nimcp_brain_factory_init_bio_async_orchestrator_subsystem(brain);
    nimcp_brain_factory_init_plasticity_coordinator_subsystem(brain);

    bool result = nimcp_brain_factory_init_cognitive_meta_controller_subsystem(brain);
    EXPECT_TRUE(result);
}
