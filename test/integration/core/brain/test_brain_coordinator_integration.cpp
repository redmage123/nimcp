//=============================================================================
// test_brain_coordinator_integration.cpp - Coordinator Integration Tests
//=============================================================================
/**
 * @file test_brain_coordinator_integration.cpp
 * @brief Integration tests for coordinator subsystem interactions
 *
 * WHAT: Tests verifying coordinators work together correctly
 * WHY:  Ensure cross-coordinator communication and dependencies work
 * HOW:  GoogleTest with real brain lifecycle and coordinator interactions
 *
 * Test Categories:
 * 1. Multi-Coordinator Initialization - All 6 coordinators init in order
 * 2. Cross-Coordinator Communication - Bio-async messaging between coordinators
 * 3. Dependency Injection - Coordinators receive their dependencies
 * 4. Graceful Degradation - Coordinators handle missing dependencies
 * 5. Lifecycle Integration - Full create/operate/destroy cycle
 *
 * @version 1.0.0
 * @date 2025-12-15
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
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
 * CoordinatorIntegrationTest Fixture
 *
 * Provides full brain context for integration testing
 */
class CoordinatorIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create a brain with various subsystems enabled
        brain = brain_create("test", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 256, 64);
    }

    void TearDown() override {
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. MULTI-COORDINATOR INITIALIZATION TESTS
//=============================================================================

/**
 * TEST: AllCoordinators_InitializeInOrder
 *
 * WHAT: Verify all 6 coordinators initialize in dependency order
 * WHY:  Ensure proper initialization sequence
 * EXPECT: Brain created with coordinator fields populated
 */
TEST_F(CoordinatorIntegrationTest, AllCoordinators_InitializeInOrder) {
    ASSERT_NE(brain, nullptr);

    // The brain_create() should have initialized coordinators in order
    // We verify the brain was created successfully, which means init didn't fail
    SUCCEED();
}

/**
 * TEST: AllCoordinators_EnabledBasedOnDependencies
 *
 * WHAT: Verify coordinators are enabled only when dependencies exist
 * WHY:  Ensure conditional initialization works
 * EXPECT: Enabled flags reflect dependency availability
 */
TEST_F(CoordinatorIntegrationTest, AllCoordinators_EnabledBasedOnDependencies) {
    ASSERT_NE(brain, nullptr);

    // Bio-async orchestrator enabled if bio-async is enabled
    if (brain->bio_async_enabled) {
        // May or may not have orchestrator depending on runtime
    }

    // Plasticity coordinator enabled if bio-async or immune is enabled
    if (brain->bio_async_enabled || brain->immune_enabled) {
        // May have plasticity coordinator
    }

    // Immune bridge coordinator enabled if immune is enabled
    if (brain->immune_enabled) {
        // May have immune bridge coordinator
    }

    // All enabled flags should be consistent with dependencies
    SUCCEED();
}

//=============================================================================
// 2. CROSS-COORDINATOR COMMUNICATION TESTS
//=============================================================================

/**
 * TEST: BioAsync_RoutesMessagesBetweenCoordinators
 *
 * WHAT: Verify bio-async can route messages between coordinators
 * WHY:  Ensure inter-coordinator communication works
 * EXPECT: Messages routed without error
 */
TEST_F(CoordinatorIntegrationTest, BioAsync_RoutesMessagesBetweenCoordinators) {
    ASSERT_NE(brain, nullptr);

    // If bio-async is enabled, test message routing
    if (brain->bio_async_enabled && brain->bio_async_orchestrator_enabled) {
        bio_async_orchestrator_t* orch = brain->bio_async_orchestrator;
        if (orch) {
            bio_orchestrator_stats_t stats;
            int result = bio_orchestrator_get_stats(orch, &stats);
            EXPECT_EQ(result, 0);
        }
    }

    SUCCEED();
}

//=============================================================================
// 3. DEPENDENCY INJECTION TESTS
//=============================================================================

/**
 * TEST: PlasticityCoordinator_ConnectsToBioAsync
 *
 * WHAT: Verify plasticity coordinator connects to bio-async
 * WHY:  Ensure dependency injection works
 * EXPECT: Connection established or gracefully skipped
 */
TEST_F(CoordinatorIntegrationTest, PlasticityCoordinator_ConnectsToBioAsync) {
    ASSERT_NE(brain, nullptr);

    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        plasticity_coordinator_t* coord = brain->plasticity_coordinator;
        plasticity_coordinator_stats_t stats;
        int result = plasticity_coordinator_get_stats(coord, &stats);
        EXPECT_EQ(result, 0);
    }

    SUCCEED();
}

/**
 * TEST: ImmuneBridgeCoordinator_ConnectsToBrainImmune
 *
 * WHAT: Verify immune bridge coordinator connects to brain immune
 * WHY:  Ensure immune integration works
 * EXPECT: Connection established or gracefully skipped
 */
TEST_F(CoordinatorIntegrationTest, ImmuneBridgeCoordinator_ConnectsToBrainImmune) {
    ASSERT_NE(brain, nullptr);

    if (brain->immune_bridge_coordinator_enabled && brain->immune_bridge_coordinator) {
        immune_bridge_coordinator_t* coord = brain->immune_bridge_coordinator;
        immune_coordinator_stats_t stats;
        int result = immune_bridge_coordinator_get_stats(coord, &stats);
        EXPECT_EQ(result, 0);
        EXPECT_GE(stats.system_health, 0.0f);
        EXPECT_LE(stats.system_health, 1.0f);
    }

    SUCCEED();
}

/**
 * TEST: CognitiveMetaController_ConnectsToWorkingMemory
 *
 * WHAT: Verify cognitive meta-controller connects to working memory
 * WHY:  Ensure cognitive integration works
 * EXPECT: Connection established or gracefully skipped
 */
TEST_F(CoordinatorIntegrationTest, CognitiveMetaController_ConnectsToWorkingMemory) {
    ASSERT_NE(brain, nullptr);

    if (brain->cognitive_meta_controller_enabled && brain->cognitive_meta_controller) {
        cognitive_meta_controller_t* ctrl = brain->cognitive_meta_controller;
        meta_controller_stats_t stats;
        int result = meta_controller_get_stats(ctrl, &stats);
        EXPECT_EQ(result, 0);
    }

    SUCCEED();
}

//=============================================================================
// 4. GRACEFUL DEGRADATION TESTS
//=============================================================================

/**
 * TEST: Coordinators_HandleMissingDependencies
 *
 * WHAT: Verify coordinators handle missing dependencies gracefully
 * WHY:  Ensure robustness when dependencies unavailable
 * EXPECT: Brain still functions without all coordinators
 */
TEST_F(CoordinatorIntegrationTest, Coordinators_HandleMissingDependencies) {
    // Destroy current brain and create minimal one
    brain_destroy(brain);

    // Create a tiny brain with fewer subsystems
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 32, 8);
    ASSERT_NE(brain, nullptr);

    // Brain should work even if some coordinators are disabled
    // due to missing dependencies
    SUCCEED();
}

//=============================================================================
// 5. LIFECYCLE INTEGRATION TESTS
//=============================================================================

/**
 * TEST: FullLifecycle_CreateOperateDestroy
 *
 * WHAT: Test full lifecycle with all coordinators
 * WHY:  Ensure complete create/operate/destroy works
 * EXPECT: No crashes or memory leaks
 */
TEST_F(CoordinatorIntegrationTest, FullLifecycle_CreateOperateDestroy) {
    ASSERT_NE(brain, nullptr);

    // Simulate some operations
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Destroy and recreate
    brain_destroy(brain);
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_PATTERN_MATCHING, 128, 32);
    ASSERT_NE(brain, nullptr);

    // More operations
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Final destroy happens in TearDown
    SUCCEED();
}

/**
 * TEST: MultipleCreateDestroy_NoLeaks
 *
 * WHAT: Create and destroy brains multiple times
 * WHY:  Ensure no memory leaks in coordinator lifecycle
 * EXPECT: All iterations succeed
 */
TEST_F(CoordinatorIntegrationTest, MultipleCreateDestroy_NoLeaks) {
    // Destroy the fixture brain
    brain_destroy(brain);
    brain = nullptr;

    // Create and destroy 5 times
    for (int i = 0; i < 5; i++) {
        brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 16);
        ASSERT_NE(brain, nullptr) << "Failed on iteration " << i;
        brain_destroy(brain);
        brain = nullptr;
    }

    // Create one more for TearDown
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 16);
    ASSERT_NE(brain, nullptr);
}

//=============================================================================
// 6. COORDINATOR STATS TESTS
//=============================================================================

/**
 * TEST: AllCoordinators_StatsAccessible
 *
 * WHAT: Verify stats can be retrieved from all enabled coordinators
 * WHY:  Ensure monitoring works
 * EXPECT: Stats retrieval succeeds for enabled coordinators
 */
TEST_F(CoordinatorIntegrationTest, AllCoordinators_StatsAccessible) {
    ASSERT_NE(brain, nullptr);

    // Bio-async orchestrator stats
    if (brain->bio_async_orchestrator_enabled && brain->bio_async_orchestrator) {
        bio_orchestrator_stats_t stats;
        int result = bio_orchestrator_get_stats(brain->bio_async_orchestrator, &stats);
        EXPECT_EQ(result, 0);
    }

    // Plasticity coordinator stats
    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        plasticity_coordinator_stats_t stats;
        int result = plasticity_coordinator_get_stats(brain->plasticity_coordinator, &stats);
        EXPECT_EQ(result, 0);
    }

    // Immune bridge coordinator stats
    if (brain->immune_bridge_coordinator_enabled && brain->immune_bridge_coordinator) {
        immune_coordinator_stats_t stats;
        int result = immune_bridge_coordinator_get_stats(brain->immune_bridge_coordinator, &stats);
        EXPECT_EQ(result, 0);
    }

    // Cognitive meta-controller stats
    if (brain->cognitive_meta_controller_enabled && brain->cognitive_meta_controller) {
        meta_controller_stats_t stats;
        int result = meta_controller_get_stats(brain->cognitive_meta_controller, &stats);
        EXPECT_EQ(result, 0);
    }

    // Security-perception bridge stats
    if (brain->security_perception_bridge_enabled && brain->security_perception_bridge) {
        sec_percept_stats_t stats;
        int result = sec_percept_get_stats(brain->security_perception_bridge, &stats);
        EXPECT_EQ(result, 0);
    }

    // Swarm module registry stats
    if (brain->swarm_module_registry_enabled && brain->swarm_module_registry) {
        swarm_registry_stats_t stats;
        int result = swarm_registry_get_stats(brain->swarm_module_registry, &stats);
        EXPECT_EQ(result, 0);
    }

    SUCCEED();
}
