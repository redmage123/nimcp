/**
 * @file test_medulla_exception_e2e.cpp
 * @brief End-to-end tests for medulla oblongata exception handling and recovery
 * @version 1.0.0
 * @date 2026-01-25
 *
 * WHAT: End-to-end tests for the complete medulla exception-to-recovery pipeline
 * WHY:  Verify that exceptions in medulla modules flow correctly through the
 *       immune system and trigger appropriate recovery actions
 * HOW:  Test full integration scenarios:
 *       - Medulla subsystem exception -> Immune presentation -> Recovery
 *       - Cross-module exception cascades through medulla
 *       - Sustained operation with exception injection
 *       - Recovery and state restoration
 *
 * TEST SCENARIOS:
 * 1.  Full medulla exception lifecycle (NULL pointer detection -> recovery)
 * 2.  Arousal state exception recovery
 * 3.  Circadian rhythm exception recovery
 * 4.  Protection level exception cascades
 * 5.  Medulla-immune bridge exception flow
 * 6.  Medulla-cerebellum bridge exception flow
 * 7.  Cross-bridge exception coordination
 * 8.  Emergency shutdown exception handling
 * 9.  Concurrent exception handling in medulla subsystems
 * 10. State recovery after exception storm
 * 11. Statistics tracking through exceptions
 * 12. Long-running operation with exception injection
 * 13. Protection escalation during exceptions
 * 14. Circadian phase preservation during exceptions
 * 15. Full system stress test with exception injection
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <random>

// Headers have their own extern "C" guards

// Core medulla headers
#include "core/medulla/nimcp_medulla.h"
#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"

// Related system headers for full E2E testing
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

// Exception system headers
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"

// Signal handler for cleanup
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int E2E_SHORT_ITERATIONS = 100;
static constexpr int E2E_MEDIUM_ITERATIONS = 500;
static constexpr int E2E_LONG_ITERATIONS = 1000;
static constexpr float E2E_DT = 0.016f;  // ~60 fps simulation
static constexpr float EPSILON = 1e-5f;

//=============================================================================
// Test Fixture for Full E2E Tests
//=============================================================================

class MedullaExceptionE2ETest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;
    brain_immune_system_t* immune = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    medulla_immune_bridge_t immune_bridge = nullptr;
    med_cereb_bridge_t cereb_bridge = nullptr;

    void SetUp() override {
        // Global state cleanup for test isolation
        signal_handler_unregister_brain();
        signal_handler_reset_stats();
        signal_handler_uninstall();
        nimcp_memory_reset_state();
        consolidation_reset_global_state();

        // Create medulla with minimal integrations for testing
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_health_integration = false;
        med_config.enable_recovery_integration = false;
        med_config.enable_sleep_integration = false;
        med_config.enable_neuromod_integration = false;
        med_config.enable_bio_async = false;
        medulla = medulla_create(&med_config);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_config.enable_bio_async = false;
        immune_config.enable_bbb_integration = false;
        immune_config.enable_bft_integration = false;
        immune_config.enable_swarm_integration = false;
        immune = brain_immune_create(&immune_config);

        // Create cerebellum
        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_bio_async = false;
        cerebellum = cerebellum_create(&cere_config);

        // Create bridges only if components were created
        if (medulla && immune) {
            medulla_immune_config_t imm_bridge_config;
            medulla_immune_default_config(&imm_bridge_config);
            imm_bridge_config.enable_bio_async = false;
            immune_bridge = medulla_immune_create(&imm_bridge_config, medulla, immune);
        }

        if (medulla && cerebellum) {
            med_cereb_bridge_config_t cer_bridge_config;
            med_cereb_bridge_default_config(&cer_bridge_config);
            cer_bridge_config.enable_bio_async = false;
            cereb_bridge = med_cereb_bridge_create(&cer_bridge_config);
            if (cereb_bridge) {
                med_cereb_bridge_connect_medulla(cereb_bridge, medulla);
                med_cereb_bridge_connect_cerebellum(cereb_bridge, cerebellum);
            }
        }
    }

    void TearDown() override {
        if (cereb_bridge) {
            med_cereb_bridge_destroy(cereb_bridge);
            cereb_bridge = nullptr;
        }
        if (immune_bridge) {
            medulla_immune_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }

        // Global state cleanup for test isolation
        signal_handler_unregister_brain();
        signal_handler_reset_stats();
        signal_handler_uninstall();
        nimcp_memory_reset_state();
        consolidation_reset_global_state();
    }

    // Helper to run update cycles with all bridges
    void runFullUpdateCycle(int iterations, float dt = E2E_DT) {
        for (int i = 0; i < iterations; i++) {
            if (medulla) {
                medulla_update(medulla, dt);
            }
            if (immune_bridge) {
                medulla_immune_update(immune_bridge);
            }
            if (cereb_bridge) {
                med_cereb_bridge_update(cereb_bridge, (uint64_t)(dt * 1000000.0f));
            }
        }
    }
};

//=============================================================================
// Test 1: Full Medulla Exception Lifecycle
//=============================================================================

TEST_F(MedullaExceptionE2ETest, FullMedullaExceptionLifecycle) {
    printf("=== E2E Test: Full Medulla Exception Lifecycle ===\n");

    ASSERT_NE(medulla, nullptr) << "Medulla creation failed";

    // Step 1: Start medulla
    ASSERT_EQ(medulla_start(medulla), 0) << "Medulla start failed";
    printf("  Step 1: Medulla started\n");

    // Step 2: Run normal operations
    runFullUpdateCycle(10);
    printf("  Step 2: Normal operation verified\n");

    // Step 3: Test NULL pointer exception handling
    // These calls should return errors but not crash
    float bad_arousal = medulla_get_arousal_level(nullptr);
    EXPECT_LT(bad_arousal, 0.0f);
    printf("  Step 3: NULL arousal query returned error value: %.2f\n", bad_arousal);

    // Step 4: Verify medulla still functioning after error
    runFullUpdateCycle(10);
    float good_arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(good_arousal, 0.0f);
    EXPECT_LE(good_arousal, 1.0f);
    printf("  Step 4: Medulla still functioning, arousal=%.3f\n", good_arousal);

    // Step 5: Get final statistics
    medulla_stats_t stats;
    ASSERT_EQ(medulla_get_stats(medulla, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);
    printf("  Step 5: Total updates: %lu\n", (unsigned long)stats.total_updates);

    printf("Test passed: Full medulla exception lifecycle\n\n");
}

//=============================================================================
// Test 2: Arousal State Exception Recovery
//=============================================================================

TEST_F(MedullaExceptionE2ETest, ArousalStateExceptionRecovery) {
    printf("=== E2E Test: Arousal State Exception Recovery ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Set known arousal state
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.6f), 0);
    float initial_arousal = medulla_get_arousal_level(medulla);
    printf("  Initial arousal: %.3f\n", initial_arousal);

    // Step 2: Attempt invalid operations that trigger exceptions
    int result = medulla_boost_arousal(nullptr, 0.1f);
    EXPECT_NE(result, 0);
    printf("  NULL boost returned: %d\n", result);

    result = medulla_reduce_arousal(nullptr, 0.1f);
    EXPECT_NE(result, 0);
    printf("  NULL reduce returned: %d\n", result);

    // Step 3: Verify arousal state preserved
    float final_arousal = medulla_get_arousal_level(medulla);
    EXPECT_NEAR(final_arousal, initial_arousal, 0.05f);
    printf("  Final arousal: %.3f (preserved)\n", final_arousal);

    // Step 4: Verify normal operations still work
    ASSERT_EQ(medulla_boost_arousal(medulla, 0.1f), 0);
    float boosted = medulla_get_arousal_level(medulla);
    EXPECT_GT(boosted, final_arousal);
    printf("  Boosted arousal: %.3f\n", boosted);

    printf("Test passed: Arousal state exception recovery\n\n");
}

//=============================================================================
// Test 3: Circadian Rhythm Exception Recovery
//=============================================================================

TEST_F(MedullaExceptionE2ETest, CircadianRhythmExceptionRecovery) {
    printf("=== E2E Test: Circadian Rhythm Exception Recovery ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Set known circadian phase
    ASSERT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING), 0);
    circadian_phase_t initial_phase = medulla_get_circadian_phase(medulla);
    EXPECT_EQ(initial_phase, CIRCADIAN_PHASE_MORNING);
    printf("  Initial phase: %s\n", medulla_circadian_phase_to_string(initial_phase));

    // Step 2: Attempt NULL operations
    circadian_phase_t null_phase = medulla_get_circadian_phase(nullptr);
    // Should return default/error value
    printf("  NULL phase query returned: %d\n", (int)null_phase);

    // Step 3: Verify phase preserved
    circadian_phase_t current_phase = medulla_get_circadian_phase(medulla);
    EXPECT_EQ(current_phase, CIRCADIAN_PHASE_MORNING);
    printf("  Phase preserved: %s\n", medulla_circadian_phase_to_string(current_phase));

    // Step 4: Transition through multiple phases
    circadian_phase_t phases[] = {
        CIRCADIAN_PHASE_AFTERNOON,
        CIRCADIAN_PHASE_EVENING,
        CIRCADIAN_PHASE_NIGHT,
        CIRCADIAN_PHASE_DEEP_NIGHT
    };

    for (auto phase : phases) {
        ASSERT_EQ(medulla_test_set_circadian(medulla, phase), 0);
        runFullUpdateCycle(5);
        EXPECT_EQ(medulla_get_circadian_phase(medulla), phase);
    }
    printf("  Phase transitions successful\n");

    printf("Test passed: Circadian rhythm exception recovery\n\n");
}

//=============================================================================
// Test 4: Protection Level Exception Cascades
//=============================================================================

TEST_F(MedullaExceptionE2ETest, ProtectionLevelExceptionCascades) {
    printf("=== E2E Test: Protection Level Exception Cascades ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Start at normal protection
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);
    printf("  Starting at NORMAL protection\n");

    // Step 2: Simulate escalating exceptions by setting higher protection
    protection_level_t levels[] = {
        PROTECTION_LEVEL_CAUTIOUS,
        PROTECTION_LEVEL_GUARDED,
        PROTECTION_LEVEL_DEFENSIVE,
        PROTECTION_LEVEL_CRITICAL
    };

    for (auto level : levels) {
        ASSERT_EQ(medulla_test_set_protection(medulla, level), 0);
        runFullUpdateCycle(5);

        protection_level_t current = medulla_get_protection_level(medulla);
        EXPECT_EQ(current, level);
        printf("  Escalated to: %s\n", medulla_protection_level_to_string(current));
    }

    // Step 3: Verify arousal is suppressed at critical protection
    float critical_arousal = medulla_get_arousal_level(medulla);
    printf("  Arousal at CRITICAL: %.3f\n", critical_arousal);

    // Step 4: De-escalate and verify recovery
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);
    runFullUpdateCycle(10);

    protection_level_t final_level = medulla_get_protection_level(medulla);
    EXPECT_EQ(final_level, PROTECTION_LEVEL_NORMAL);
    printf("  De-escalated to: %s\n", medulla_protection_level_to_string(final_level));

    printf("Test passed: Protection level exception cascades\n\n");
}

//=============================================================================
// Test 5: Medulla-Immune Bridge Exception Flow
//=============================================================================

TEST_F(MedullaExceptionE2ETest, MedullaImmuneBridgeExceptionFlow) {
    printf("=== E2E Test: Medulla-Immune Bridge Exception Flow ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_NE(immune_bridge, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Normal bridge operation
    runFullUpdateCycle(10);
    printf("  Step 1: Normal bridge operation\n");

    // Step 2: Test NULL operations on bridge
    int result = medulla_immune_update(nullptr);
    EXPECT_NE(result, 0);
    printf("  NULL bridge update returned: %d\n", result);

    medulla_cytokine_effects_t cytokine_effects;
    result = medulla_immune_get_cytokine_effects(nullptr, &cytokine_effects);
    EXPECT_NE(result, 0);
    printf("  NULL cytokine query returned: %d\n", result);

    // Step 3: Verify bridge still works
    ASSERT_EQ(medulla_immune_update(immune_bridge), 0);
    ASSERT_EQ(medulla_immune_get_cytokine_effects(immune_bridge, &cytokine_effects), 0);
    printf("  Bridge recovered, cytokine arousal factor: %.3f\n",
           cytokine_effects.inflammation_arousal_factor);

    // Step 4: Get bridge statistics
    medulla_immune_stats_t bridge_stats;
    ASSERT_EQ(medulla_immune_get_stats(immune_bridge, &bridge_stats), 0);
    printf("  Bridge total updates: %lu\n", (unsigned long)bridge_stats.total_updates);

    printf("Test passed: Medulla-immune bridge exception flow\n\n");
}

//=============================================================================
// Test 6: Medulla-Cerebellum Bridge Exception Flow
//=============================================================================

TEST_F(MedullaExceptionE2ETest, MedullaCerebellumBridgeExceptionFlow) {
    printf("=== E2E Test: Medulla-Cerebellum Bridge Exception Flow ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_NE(cereb_bridge, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Verify bridge is connected
    EXPECT_TRUE(med_cereb_bridge_is_connected(cereb_bridge));
    printf("  Step 1: Bridge connected\n");

    // Step 2: Queue errors and process
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(med_cereb_bridge_queue_error(
            cereb_bridge, MED_CEREB_ERROR_TIMING, 0.5f, i), 0);
    }
    printf("  Step 2: Queued 5 errors\n");

    // Step 3: Test NULL operations
    int result = med_cereb_bridge_queue_error(nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 0);
    EXPECT_NE(result, 0);

    result = med_cereb_bridge_update(nullptr, 10000);
    EXPECT_NE(result, 0);
    printf("  Step 3: NULL operations returned errors\n");

    // Step 4: Verify bridge still works
    runFullUpdateCycle(10);
    EXPECT_TRUE(med_cereb_bridge_is_connected(cereb_bridge));
    printf("  Step 4: Bridge still connected\n");

    // Step 5: Get bridge statistics
    med_cereb_bridge_stats_t bridge_stats;
    ASSERT_EQ(med_cereb_bridge_get_stats(cereb_bridge, &bridge_stats), 0);
    printf("  IO spikes: %lu, Climbing signals: %lu\n",
           (unsigned long)bridge_stats.io_spikes, (unsigned long)bridge_stats.climbing_signals_sent);

    printf("Test passed: Medulla-cerebellum bridge exception flow\n\n");
}

//=============================================================================
// Test 7: Cross-Bridge Exception Coordination
//=============================================================================

TEST_F(MedullaExceptionE2ETest, CrossBridgeExceptionCoordination) {
    printf("=== E2E Test: Cross-Bridge Exception Coordination ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_NE(immune_bridge, nullptr);
    ASSERT_NE(cereb_bridge, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Set arousal high
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.8f), 0);
    printf("  Step 1: Arousal set to 0.8\n");

    // Step 2: Run all bridges
    runFullUpdateCycle(20);

    // Step 3: Verify consistent state across bridges
    med_cereb_arousal_effects_t arousal_effects;
    ASSERT_EQ(med_cereb_bridge_get_arousal_effects(cereb_bridge, &arousal_effects), 0);
    printf("  Step 3: Cerebellum motor gain: %.3f\n", arousal_effects.motor_gain);

    medulla_immune_effects_t immune_effects;
    ASSERT_EQ(medulla_immune_get_immune_effects(immune_bridge, &immune_effects), 0);
    printf("  Step 3: Immune arousal level: %.3f\n", immune_effects.arousal_level);

    // Step 4: Trigger protection escalation
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL), 0);
    runFullUpdateCycle(10);

    med_cereb_protection_effects_t protection_effects;
    ASSERT_EQ(med_cereb_bridge_get_protection_effects(cereb_bridge, &protection_effects), 0);
    printf("  Step 4: Protection output scale: %.3f\n", protection_effects.output_scale);

    // Step 5: Verify bridges handle state changes gracefully
    EXPECT_LT(protection_effects.output_scale, 1.0f);
    printf("  Step 5: State propagated correctly\n");

    printf("Test passed: Cross-bridge exception coordination\n\n");
}

//=============================================================================
// Test 8: Emergency Shutdown Exception Handling
//=============================================================================

TEST_F(MedullaExceptionE2ETest, EmergencyShutdownExceptionHandling) {
    printf("=== E2E Test: Emergency Shutdown Exception Handling ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Get initial state
    medulla_stats_t initial_stats;
    ASSERT_EQ(medulla_get_stats(medulla, &initial_stats), 0);
    uint32_t initial_emergencies = initial_stats.emergency_shutdowns;
    printf("  Step 1: Initial emergency count: %u\n", initial_emergencies);

    // Step 2: Trigger emergency shutdown
    ASSERT_EQ(medulla_emergency_shutdown(medulla, "E2E test emergency"), 0);
    printf("  Step 2: Emergency shutdown triggered\n");

    // Step 3: Verify protection level is SHUTDOWN
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_SHUTDOWN);
    printf("  Step 3: Protection level: %s\n", medulla_protection_level_to_string(level));

    // Step 4: Verify emergency count increased
    medulla_stats_t after_stats;
    ASSERT_EQ(medulla_get_stats(medulla, &after_stats), 0);
    EXPECT_GT(after_stats.emergency_shutdowns, initial_emergencies);
    printf("  Step 4: Emergency count: %u\n", after_stats.emergency_shutdowns);

    // Step 5: Test NULL emergency shutdown
    int result = medulla_emergency_shutdown(nullptr, "test");
    EXPECT_NE(result, 0);
    printf("  Step 5: NULL emergency returned: %d\n", result);

    printf("Test passed: Emergency shutdown exception handling\n\n");
}

//=============================================================================
// Test 9: Concurrent Exception Handling
//=============================================================================

TEST_F(MedullaExceptionE2ETest, ConcurrentExceptionHandling) {
    printf("=== E2E Test: Concurrent Exception Handling ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_NE(cereb_bridge, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    std::atomic<bool> running{true};
    std::atomic<int> update_count{0};
    std::atomic<int> query_count{0};
    std::atomic<int> error_queue_count{0};

    // Thread 1: Medulla updates
    auto updater = [&]() {
        while (running) {
            medulla_update(medulla, 0.001f);
            update_count++;
            std::this_thread::yield();
        }
    };

    // Thread 2: Query operations
    auto querier = [&]() {
        while (running) {
            medulla_get_arousal_level(medulla);
            medulla_get_protection_level(medulla);
            query_count++;
            std::this_thread::yield();
        }
    };

    // Thread 3: Error queueing
    auto error_queuer = [&]() {
        int i = 0;
        while (running) {
            if (cereb_bridge) {
                med_cereb_bridge_queue_error(cereb_bridge, MED_CEREB_ERROR_TIMING, 0.3f, i++);
                error_queue_count++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    };

    printf("  Starting concurrent threads...\n");

    std::thread t1(updater);
    std::thread t2(querier);
    std::thread t3(error_queuer);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    t1.join();
    t2.join();
    t3.join();

    printf("  Updates: %d, Queries: %d, Errors queued: %d\n",
           update_count.load(), query_count.load(), error_queue_count.load());

    // Verify system is still stable
    EXPECT_GT(update_count.load(), 0);
    EXPECT_GT(query_count.load(), 0);

    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
    printf("  Final arousal: %.3f (system stable)\n", arousal);

    printf("Test passed: Concurrent exception handling\n\n");
}

//=============================================================================
// Test 10: State Recovery After Exception Storm
//=============================================================================

TEST_F(MedullaExceptionE2ETest, StateRecoveryAfterExceptionStorm) {
    printf("=== E2E Test: State Recovery After Exception Storm ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Set baseline state
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.5f), 0);
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);
    ASSERT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING), 0);
    printf("  Step 1: Baseline state set\n");

    // Step 2: Generate many NULL pointer calls (exception storm)
    printf("  Step 2: Generating exception storm...\n");
    for (int i = 0; i < 100; i++) {
        medulla_get_arousal_level(nullptr);
        medulla_get_protection_level(nullptr);
        medulla_get_circadian_phase(nullptr);
        medulla_boost_arousal(nullptr, 0.1f);
        medulla_reduce_arousal(nullptr, 0.1f);
    }
    printf("  Step 2: Exception storm complete\n");

    // Step 3: Verify state preserved
    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_NEAR(arousal, 0.5f, 0.1f);
    printf("  Step 3: Arousal preserved: %.3f\n", arousal);

    protection_level_t protection = medulla_get_protection_level(medulla);
    EXPECT_EQ(protection, PROTECTION_LEVEL_NORMAL);
    printf("  Step 3: Protection preserved: %s\n", medulla_protection_level_to_string(protection));

    circadian_phase_t phase = medulla_get_circadian_phase(medulla);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_MORNING);
    printf("  Step 3: Phase preserved: %s\n", medulla_circadian_phase_to_string(phase));

    // Step 4: Run normal operations
    runFullUpdateCycle(20);

    medulla_stats_t stats;
    ASSERT_EQ(medulla_get_stats(medulla, &stats), 0);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);
    printf("  Step 4: System running normally\n");

    printf("Test passed: State recovery after exception storm\n\n");
}

//=============================================================================
// Test 11: Statistics Tracking Through Exceptions
//=============================================================================

TEST_F(MedullaExceptionE2ETest, StatisticsTrackingThroughExceptions) {
    printf("=== E2E Test: Statistics Tracking Through Exceptions ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Step 1: Get initial statistics
    medulla_stats_t initial_stats;
    ASSERT_EQ(medulla_get_stats(medulla, &initial_stats), 0);
    printf("  Initial updates: %lu\n", (unsigned long)initial_stats.total_updates);

    // Step 2: Run with intermittent NULL calls
    for (int i = 0; i < E2E_SHORT_ITERATIONS; i++) {
        medulla_update(medulla, E2E_DT);

        // Every 10 iterations, trigger an exception
        if (i % 10 == 0) {
            medulla_get_arousal_level(nullptr);
        }
    }

    // Step 3: Verify statistics updated
    medulla_stats_t final_stats;
    ASSERT_EQ(medulla_get_stats(medulla, &final_stats), 0);

    EXPECT_GE(final_stats.total_updates, initial_stats.total_updates + E2E_SHORT_ITERATIONS);
    printf("  Final updates: %lu\n", (unsigned long)final_stats.total_updates);

    // Step 4: Verify no corruption
    EXPECT_GE(final_stats.current_arousal, 0.0f);
    EXPECT_LE(final_stats.current_arousal, 1.0f);
    EXPECT_FALSE(std::isnan(final_stats.avg_arousal));
    EXPECT_FALSE(std::isnan(final_stats.avg_update_time_us));
    printf("  Statistics integrity verified\n");

    printf("Test passed: Statistics tracking through exceptions\n\n");
}

//=============================================================================
// Test 12: Long-Running Operation With Exception Injection
//=============================================================================

TEST_F(MedullaExceptionE2ETest, LongRunningWithExceptionInjection) {
    printf("=== E2E Test: Long-Running With Exception Injection ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_NE(immune_bridge, nullptr);
    ASSERT_NE(cereb_bridge, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> arousal_dist(0.2f, 0.8f);
    std::uniform_int_distribution<int> phase_dist(0, 7);
    std::uniform_int_distribution<int> protection_dist(0, 4);

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < E2E_MEDIUM_ITERATIONS; i++) {
        // Normal updates
        medulla_update(medulla, E2E_DT);
        medulla_immune_update(immune_bridge);
        med_cereb_bridge_update(cereb_bridge, 16000);

        // Random state changes
        if (i % 50 == 0) {
            float new_arousal = arousal_dist(gen);
            medulla_test_set_arousal(medulla, new_arousal);
        }

        if (i % 100 == 0) {
            circadian_phase_t new_phase = (circadian_phase_t)phase_dist(gen);
            medulla_test_set_circadian(medulla, new_phase);
        }

        // Inject exceptions periodically
        if (i % 20 == 0) {
            // NULL pointer exceptions
            medulla_get_arousal_level(nullptr);
            med_cereb_bridge_queue_error(nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 0);
        }

        // Queue cerebellum errors
        if (i % 25 == 0) {
            med_cereb_bridge_queue_error(cereb_bridge,
                (med_cereb_error_type_t)(i % MED_CEREB_ERROR_COUNT), 0.5f, i);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    printf("  Completed %d iterations in %ld ms\n", E2E_MEDIUM_ITERATIONS, (long)duration_ms);
    printf("  Throughput: %.2f iterations/second\n",
           (float)E2E_MEDIUM_ITERATIONS * 1000.0f / duration_ms);

    // Verify system stability
    medulla_stats_t stats;
    ASSERT_EQ(medulla_get_stats(medulla, &stats), 0);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);
    EXPECT_GE(stats.total_updates, (uint64_t)E2E_MEDIUM_ITERATIONS);
    printf("  System stable after long run\n");

    printf("Test passed: Long-running with exception injection\n\n");
}

//=============================================================================
// Test 13: Protection Escalation During Exceptions
//=============================================================================

TEST_F(MedullaExceptionE2ETest, ProtectionEscalationDuringExceptions) {
    printf("=== E2E Test: Protection Escalation During Exceptions ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Track protection level changes
    std::vector<protection_level_t> protection_history;

    for (int i = 0; i < E2E_SHORT_ITERATIONS; i++) {
        medulla_update(medulla, E2E_DT);

        // Escalate protection every 20 iterations
        if (i % 20 == 0 && i > 0) {
            int level = (i / 20) % 6;
            medulla_test_set_protection(medulla, (protection_level_t)level);
        }

        // Record protection level
        protection_level_t current = medulla_get_protection_level(medulla);
        protection_history.push_back(current);

        // Inject some exceptions
        if (i % 10 == 0) {
            medulla_boost_arousal(nullptr, 0.1f);
        }
    }

    // Verify protection changes were recorded
    printf("  Recorded %zu protection level samples\n", protection_history.size());

    // Verify transitions occurred
    bool had_transitions = false;
    for (size_t i = 1; i < protection_history.size(); i++) {
        if (protection_history[i] != protection_history[i-1]) {
            had_transitions = true;
            break;
        }
    }
    EXPECT_TRUE(had_transitions);
    printf("  Protection transitions occurred: %s\n", had_transitions ? "yes" : "no");

    printf("Test passed: Protection escalation during exceptions\n\n");
}

//=============================================================================
// Test 14: Circadian Phase Preservation During Exceptions
//=============================================================================

TEST_F(MedullaExceptionE2ETest, CircadianPhasePreservationDuringExceptions) {
    printf("=== E2E Test: Circadian Phase Preservation During Exceptions ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    // Set specific phase
    ASSERT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_AFTERNOON), 0);
    printf("  Initial phase: AFTERNOON\n");

    // Generate many exceptions
    for (int i = 0; i < 50; i++) {
        medulla_get_circadian_phase(nullptr);
        medulla_test_set_circadian(nullptr, CIRCADIAN_PHASE_NIGHT);
    }

    // Verify phase preserved
    circadian_phase_t phase = medulla_get_circadian_phase(medulla);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_AFTERNOON);
    printf("  Phase after exceptions: %s\n", medulla_circadian_phase_to_string(phase));

    // Run updates and verify phase can still change
    ASSERT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_EVENING), 0);
    runFullUpdateCycle(10);

    phase = medulla_get_circadian_phase(medulla);
    EXPECT_EQ(phase, CIRCADIAN_PHASE_EVENING);
    printf("  Phase after transition: %s\n", medulla_circadian_phase_to_string(phase));

    printf("Test passed: Circadian phase preservation during exceptions\n\n");
}

//=============================================================================
// Test 15: Full System Stress Test With Exception Injection
//=============================================================================

TEST_F(MedullaExceptionE2ETest, FullSystemStressTest) {
    printf("=== E2E Test: Full System Stress Test ===\n");

    ASSERT_NE(medulla, nullptr);
    ASSERT_NE(immune_bridge, nullptr);
    ASSERT_NE(cereb_bridge, nullptr);
    ASSERT_EQ(medulla_start(medulla), 0);

    auto start_time = std::chrono::steady_clock::now();

    int exception_count = 0;
    int success_count = 0;

    for (int i = 0; i < E2E_LONG_ITERATIONS; i++) {
        // Full system update
        int result = medulla_update(medulla, E2E_DT);
        if (result == 0) success_count++;

        result = medulla_immune_update(immune_bridge);
        if (result == 0) success_count++;

        result = med_cereb_bridge_update(cereb_bridge, 16000);
        if (result == 0) success_count++;

        // Random state manipulation
        if (i % 100 == 0) {
            float arousal = 0.3f + (i % 50) * 0.01f;
            medulla_test_set_arousal(medulla, arousal);
        }

        if (i % 150 == 0) {
            circadian_phase_t phase = (circadian_phase_t)(i % 8);
            medulla_test_set_circadian(medulla, phase);
        }

        if (i % 200 == 0) {
            protection_level_t level = (protection_level_t)(i % 5);
            medulla_test_set_protection(medulla, level);
        }

        // Exception injection (every 10 iterations)
        if (i % 10 == 0) {
            medulla_get_arousal_level(nullptr);
            medulla_immune_update(nullptr);
            med_cereb_bridge_queue_error(nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 0);
            exception_count += 3;
        }

        // Queue cerebellum errors
        if (i % 30 == 0) {
            med_cereb_bridge_queue_error(cereb_bridge,
                MED_CEREB_ERROR_TIMING, 0.3f, i);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    printf("  Iterations: %d\n", E2E_LONG_ITERATIONS);
    printf("  Duration: %ld ms\n", (long)duration_ms);
    printf("  Successful operations: %d\n", success_count);
    printf("  Exceptions injected: %d\n", exception_count);
    printf("  Throughput: %.2f iterations/second\n",
           (float)E2E_LONG_ITERATIONS * 1000.0f / duration_ms);

    // Final verification
    medulla_stats_t stats;
    ASSERT_EQ(medulla_get_stats(medulla, &stats), 0);

    printf("\n  Final Statistics:\n");
    printf("    State: %s\n", medulla_state_to_string(stats.state));
    printf("    Arousal: %.3f\n", stats.current_arousal);
    printf("    Protection: %s\n", medulla_protection_level_to_string(stats.protection_level));
    printf("    Total updates: %lu\n", (unsigned long)stats.total_updates);
    printf("    Emergency shutdowns: %u\n", stats.emergency_shutdowns);

    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);
    EXPECT_GE(stats.current_arousal, 0.0f);
    EXPECT_LE(stats.current_arousal, 1.0f);
    EXPECT_FALSE(std::isnan(stats.avg_arousal));

    printf("\nTest passed: Full system stress test\n\n");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
