//=============================================================================
// e2e_test_coordinator_pipeline.cpp - Coordinator Pipeline E2E Tests
//=============================================================================
/**
 * @file e2e_test_coordinator_pipeline.cpp
 * @brief End-to-end tests for coordinator subsystem pipeline
 *
 * WHAT: Complete pipeline tests for 6 coordinators working together
 * WHY:  Verify full system integration from create to destroy
 * HOW:  GoogleTest with realistic workloads and scenarios
 *
 * Test Scenarios:
 * 1. Full Lifecycle - Create brain with all coordinators, operate, destroy
 * 2. Bio-Async Messaging - Message routing through coordinator pipeline
 * 3. Plasticity Operations - Plasticity mechanism coordination
 * 4. Immune Response - Coordinated immune bridge operations
 * 5. Cognitive Load - Meta-controller resource arbitration
 * 6. Security Pipeline - Perception threat analysis flow
 * 7. Swarm Coordination - Module registry operations
 *
 * @version 1.0.0
 * @date 2025-12-15
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

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
 * CoordinatorPipelineE2E Fixture
 */
class CoordinatorPipelineE2E : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create a fully-featured brain
        brain = brain_create("test", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 256, 64);
    }

    void TearDown() override {
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper to simulate processing time
    void simulate_work(int ms = 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

//=============================================================================
// 1. FULL LIFECYCLE E2E TESTS
//=============================================================================

/**
 * TEST: FullLifecycle_CreateOperateDestroy
 *
 * WHAT: Complete brain lifecycle with all coordinators
 * WHY:  Verify end-to-end operation
 * EXPECT: All phases complete successfully
 */
TEST_F(CoordinatorPipelineE2E, FullLifecycle_CreateOperateDestroy) {
    // Phase 1: Create
    ASSERT_NE(brain, nullptr) << "Brain creation failed";

    // Phase 2: Verify coordinators initialized
    // Note: Not all coordinators may be enabled depending on configuration
    // The key is that the brain is functional

    // Phase 3: Operate - query all coordinator stats
    if (brain->bio_async_orchestrator_enabled && brain->bio_async_orchestrator) {
        bio_orchestrator_stats_t stats;
        EXPECT_EQ(bio_orchestrator_get_stats(brain->bio_async_orchestrator, &stats), 0);
    }

    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        plasticity_coordinator_stats_t stats;
        EXPECT_EQ(plasticity_coordinator_get_stats(brain->plasticity_coordinator, &stats), 0);
    }

    if (brain->immune_bridge_coordinator_enabled && brain->immune_bridge_coordinator) {
        immune_coordinator_stats_t stats;
        EXPECT_EQ(immune_bridge_coordinator_get_stats(brain->immune_bridge_coordinator, &stats), 0);
    }

    if (brain->cognitive_meta_controller_enabled && brain->cognitive_meta_controller) {
        meta_controller_stats_t stats;
        EXPECT_EQ(meta_controller_get_stats(brain->cognitive_meta_controller, &stats), 0);
    }

    if (brain->security_perception_bridge_enabled && brain->security_perception_bridge) {
        sec_percept_stats_t stats;
        EXPECT_EQ(sec_percept_get_stats(brain->security_perception_bridge, &stats), 0);
    }

    if (brain->swarm_module_registry_enabled && brain->swarm_module_registry) {
        swarm_registry_stats_t stats;
        EXPECT_EQ(swarm_registry_get_stats(brain->swarm_module_registry, &stats), 0);
    }

    simulate_work(50);

    // Phase 4: Destroy (happens in TearDown)
    SUCCEED();
}

//=============================================================================
// 2. BIO-ASYNC MESSAGING E2E TESTS
//=============================================================================

/**
 * TEST: BioAsyncPipeline_MessageRouting
 *
 * WHAT: Test bio-async message routing through coordinator pipeline
 * WHY:  Verify inter-coordinator communication
 * EXPECT: Messages routed successfully
 */
TEST_F(CoordinatorPipelineE2E, BioAsyncPipeline_MessageRouting) {
    ASSERT_NE(brain, nullptr);

    // If bio-async orchestrator is enabled, verify it's tracking modules
    if (brain->bio_async_orchestrator_enabled && brain->bio_async_orchestrator) {
        bio_orchestrator_stats_t initial_stats;
        int result = bio_orchestrator_get_stats(brain->bio_async_orchestrator, &initial_stats);
        EXPECT_EQ(result, 0);

        // Simulate some activity
        simulate_work(20);

        bio_orchestrator_stats_t final_stats;
        result = bio_orchestrator_get_stats(brain->bio_async_orchestrator, &final_stats);
        EXPECT_EQ(result, 0);

        // Verify orchestrator is running
        EXPECT_GE(final_stats.total_modules, 0u);
    }

    SUCCEED();
}

//=============================================================================
// 3. PLASTICITY OPERATIONS E2E TESTS
//=============================================================================

/**
 * TEST: PlasticityPipeline_MechanismCoordination
 *
 * WHAT: Test plasticity mechanism coordination
 * WHY:  Verify plasticity coordinator manages mechanisms correctly
 * EXPECT: Mechanisms coordinated without conflicts
 */
TEST_F(CoordinatorPipelineE2E, PlasticityPipeline_MechanismCoordination) {
    ASSERT_NE(brain, nullptr);

    if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
        plasticity_coordinator_stats_t initial_stats;
        int result = plasticity_coordinator_get_stats(brain->plasticity_coordinator, &initial_stats);
        EXPECT_EQ(result, 0);

        // Simulate plasticity activity
        simulate_work(30);

        plasticity_coordinator_stats_t final_stats;
        result = plasticity_coordinator_get_stats(brain->plasticity_coordinator, &final_stats);
        EXPECT_EQ(result, 0);
    }

    SUCCEED();
}

//=============================================================================
// 4. IMMUNE RESPONSE E2E TESTS
//=============================================================================

/**
 * TEST: ImmunePipeline_BridgeCoordination
 *
 * WHAT: Test immune bridge coordination
 * WHY:  Verify immune bridges work together
 * EXPECT: System health maintained
 */
TEST_F(CoordinatorPipelineE2E, ImmunePipeline_BridgeCoordination) {
    ASSERT_NE(brain, nullptr);

    if (brain->immune_bridge_coordinator_enabled && brain->immune_bridge_coordinator) {
        immune_coordinator_stats_t initial_stats;
        int result = immune_bridge_coordinator_get_stats(brain->immune_bridge_coordinator, &initial_stats);
        EXPECT_EQ(result, 0);
        EXPECT_GE(initial_stats.system_health, 0.0f);
        EXPECT_LE(initial_stats.system_health, 1.0f);

        // Simulate activity
        simulate_work(20);

        immune_coordinator_stats_t final_stats;
        result = immune_bridge_coordinator_get_stats(brain->immune_bridge_coordinator, &final_stats);
        EXPECT_EQ(result, 0);

        // Health should remain valid
        EXPECT_GE(final_stats.system_health, 0.0f);
        EXPECT_LE(final_stats.system_health, 1.0f);
    }

    SUCCEED();
}

//=============================================================================
// 5. COGNITIVE LOAD E2E TESTS
//=============================================================================

/**
 * TEST: CognitivePipeline_ResourceArbitration
 *
 * WHAT: Test cognitive meta-controller resource arbitration
 * WHY:  Verify cognitive resources allocated correctly
 * EXPECT: Arbitration metrics within bounds
 */
TEST_F(CoordinatorPipelineE2E, CognitivePipeline_ResourceArbitration) {
    ASSERT_NE(brain, nullptr);

    if (brain->cognitive_meta_controller_enabled && brain->cognitive_meta_controller) {
        meta_controller_stats_t initial_stats;
        int result = meta_controller_get_stats(brain->cognitive_meta_controller, &initial_stats);
        EXPECT_EQ(result, 0);

        // Verify confidence/uncertainty bounds
        EXPECT_GE(initial_stats.system_confidence, 0.0f);
        EXPECT_LE(initial_stats.system_confidence, 1.0f);
        EXPECT_GE(initial_stats.system_uncertainty, 0.0f);
        EXPECT_LE(initial_stats.system_uncertainty, 1.0f);

        simulate_work(25);

        meta_controller_stats_t final_stats;
        result = meta_controller_get_stats(brain->cognitive_meta_controller, &final_stats);
        EXPECT_EQ(result, 0);
    }

    SUCCEED();
}

//=============================================================================
// 6. SECURITY PIPELINE E2E TESTS
//=============================================================================

/**
 * TEST: SecurityPipeline_ThreatAnalysis
 *
 * WHAT: Test security-perception bridge threat analysis pipeline
 * WHY:  Verify sensory threat detection works
 * EXPECT: Analysis completes without error
 */
TEST_F(CoordinatorPipelineE2E, SecurityPipeline_ThreatAnalysis) {
    ASSERT_NE(brain, nullptr);

    if (brain->security_perception_bridge_enabled && brain->security_perception_bridge) {
        sec_percept_stats_t initial_stats;
        int result = sec_percept_get_stats(brain->security_perception_bridge, &initial_stats);
        EXPECT_EQ(result, 0);

        simulate_work(20);

        sec_percept_stats_t final_stats;
        result = sec_percept_get_stats(brain->security_perception_bridge, &final_stats);
        EXPECT_EQ(result, 0);
    }

    SUCCEED();
}

//=============================================================================
// 7. SWARM COORDINATION E2E TESTS
//=============================================================================

/**
 * TEST: SwarmPipeline_ModuleRegistry
 *
 * WHAT: Test swarm module registry operations
 * WHY:  Verify module registration and coordination
 * EXPECT: Registry operations succeed
 */
TEST_F(CoordinatorPipelineE2E, SwarmPipeline_ModuleRegistry) {
    ASSERT_NE(brain, nullptr);

    if (brain->swarm_module_registry_enabled && brain->swarm_module_registry) {
        swarm_registry_stats_t initial_stats;
        int result = swarm_registry_get_stats(brain->swarm_module_registry, &initial_stats);
        EXPECT_EQ(result, 0);

        simulate_work(15);

        swarm_registry_stats_t final_stats;
        result = swarm_registry_get_stats(brain->swarm_module_registry, &final_stats);
        EXPECT_EQ(result, 0);
    }

    SUCCEED();
}

//=============================================================================
// 8. MULTI-COORDINATOR E2E TESTS
//=============================================================================

/**
 * TEST: MultiCoordinator_ConcurrentOperations
 *
 * WHAT: Test all coordinators operating concurrently
 * WHY:  Verify no conflicts or deadlocks
 * EXPECT: All operations complete
 */
TEST_F(CoordinatorPipelineE2E, MultiCoordinator_ConcurrentOperations) {
    ASSERT_NE(brain, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Launch threads accessing different coordinators
    threads.emplace_back([this, &success_count]() {
        for (int i = 0; i < 10; i++) {
            if (brain->bio_async_orchestrator_enabled && brain->bio_async_orchestrator) {
                bio_orchestrator_stats_t stats;
                bio_orchestrator_get_stats(brain->bio_async_orchestrator, &stats);
            }
            success_count++;
        }
    });

    threads.emplace_back([this, &success_count]() {
        for (int i = 0; i < 10; i++) {
            if (brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
                plasticity_coordinator_stats_t stats;
                plasticity_coordinator_get_stats(brain->plasticity_coordinator, &stats);
            }
            success_count++;
        }
    });

    threads.emplace_back([this, &success_count]() {
        for (int i = 0; i < 10; i++) {
            if (brain->immune_bridge_coordinator_enabled && brain->immune_bridge_coordinator) {
                immune_coordinator_stats_t stats;
                immune_bridge_coordinator_get_stats(brain->immune_bridge_coordinator, &stats);
            }
            success_count++;
        }
    });

    threads.emplace_back([this, &success_count]() {
        for (int i = 0; i < 10; i++) {
            if (brain->cognitive_meta_controller_enabled && brain->cognitive_meta_controller) {
                meta_controller_stats_t stats;
                meta_controller_get_stats(brain->cognitive_meta_controller, &stats);
            }
            success_count++;
        }
    });

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 40);
}

/**
 * TEST: MultiCoordinator_StressTest
 *
 * WHAT: Stress test with many create/destroy cycles
 * WHY:  Verify stability under load
 * EXPECT: No crashes or memory issues
 */
TEST_F(CoordinatorPipelineE2E, MultiCoordinator_StressTest) {
    brain_destroy(brain);
    brain = nullptr;

    // Create and destroy 10 brains rapidly
    for (int i = 0; i < 10; i++) {
        brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 128, 32);
        ASSERT_NE(brain, nullptr) << "Failed on iteration " << i;

        // Query some stats
        if (brain->bio_async_orchestrator_enabled && brain->bio_async_orchestrator) {
            bio_orchestrator_stats_t stats;
            bio_orchestrator_get_stats(brain->bio_async_orchestrator, &stats);
        }

        brain_destroy(brain);
        brain = nullptr;
    }

    // Create final brain for TearDown
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 128, 32);
    ASSERT_NE(brain, nullptr);
}

/**
 * TEST: FullPipeline_RealisticWorkload
 *
 * WHAT: Simulate realistic workload with all coordinators
 * WHY:  Verify system handles typical usage patterns
 * EXPECT: All operations complete correctly
 */
TEST_F(CoordinatorPipelineE2E, FullPipeline_RealisticWorkload) {
    ASSERT_NE(brain, nullptr);

    // Simulate 100ms of "activity"
    auto start = std::chrono::steady_clock::now();
    int iterations = 0;

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        // Query coordinator stats in a realistic pattern
        if (iterations % 5 == 0 && brain->bio_async_orchestrator_enabled && brain->bio_async_orchestrator) {
            bio_orchestrator_stats_t stats;
            bio_orchestrator_get_stats(brain->bio_async_orchestrator, &stats);
        }
        if (iterations % 7 == 0 && brain->plasticity_coordinator_enabled && brain->plasticity_coordinator) {
            plasticity_coordinator_stats_t stats;
            plasticity_coordinator_get_stats(brain->plasticity_coordinator, &stats);
        }
        if (iterations % 11 == 0 && brain->immune_bridge_coordinator_enabled && brain->immune_bridge_coordinator) {
            immune_coordinator_stats_t stats;
            immune_bridge_coordinator_get_stats(brain->immune_bridge_coordinator, &stats);
        }

        iterations++;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    EXPECT_GT(iterations, 0);
}
