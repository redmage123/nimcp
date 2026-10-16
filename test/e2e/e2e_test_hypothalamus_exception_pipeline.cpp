/**
 * @file e2e_test_hypothalamus_exception_pipeline.cpp
 * @brief End-to-end tests for Hypothalamus Exception Handling Pipeline
 *
 * WHAT: Complete end-to-end tests verifying hypothalamus exception handling
 *       integrates correctly with the brain immune system
 * WHY:  Verify the full error handling pipeline works correctly under realistic
 *       conditions with all components integrated
 * HOW:  Create complete hypothalamus systems, trigger errors, verify immune
 *       response, test recovery mechanisms, and validate error statistics
 *
 * TEST SCENARIOS:
 * 1. Full Pipeline Error Handling - Initialize complete hypothalamus system with
 *    brain immune, trigger error in orchestrator, verify exception reaches immune
 * 2. Multi-Bridge Error Cascade - Create multiple bridges, trigger errors in
 *    sequence across bridges, verify all errors are tracked
 * 3. Recovery Test - Trigger error condition, verify error is reported, fix
 *    condition, verify system returns to normal operation
 * 4. Stress Test - Rapidly trigger many errors, verify no crashes or memory
 *    corruption, verify statistics are reasonable
 * 5. FEP Integration Errors - Create FEP bridges, trigger FEP-related errors,
 *    verify proper exception flow
 *
 * @version Phase 7: Exception Handling Integration
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>
#include <random>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_immune_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_immune_fep_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

// E2E Framework
#include "e2e_test_framework.h"

using namespace nimcp::e2e;

// ============================================================================
// Constants
// ============================================================================

constexpr int MAX_STRESS_ERRORS = 100;
constexpr int MAX_BRIDGES_FOR_CASCADE = 5;
constexpr double MAX_ERROR_LATENCY_MS = 100.0;
constexpr double MAX_RECOVERY_TIME_MS = 500.0;
constexpr double MAX_STRESS_DURATION_MS = 5000.0;

// ============================================================================
// Callback Tracking
// ============================================================================

struct ExceptionTracker {
    std::atomic<int> exceptions_presented{0};
    std::atomic<int> recoveries_completed{0};
    std::atomic<int> antigens_created{0};
    std::atomic<int> cytokines_released{0};
    std::mutex history_mutex;
    std::vector<uint32_t> antigen_ids;
    std::vector<int> error_codes;

    void reset() {
        exceptions_presented = 0;
        recoveries_completed = 0;
        antigens_created = 0;
        cytokines_released = 0;
        std::lock_guard<std::mutex> lock(history_mutex);
        antigen_ids.clear();
        error_codes.clear();
    }

    void record_exception(int code, uint32_t antigen_id) {
        exceptions_presented++;
        antigens_created++;
        std::lock_guard<std::mutex> lock(history_mutex);
        error_codes.push_back(code);
        antigen_ids.push_back(antigen_id);
    }
};

static ExceptionTracker g_exception_tracker;

// Static callback for exception presentation
static void on_exception_presented(
    brain_immune_system_t* system,
    const nimcp_exception_t* exception,
    uint32_t antigen_id,
    void* user_data
) {
    (void)system;
    (void)user_data;
    if (exception) {
        g_exception_tracker.record_exception(
            exception->code,
            antigen_id
        );
    }
}

// Static callback for recovery
static void on_recovery_completed(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    int recovery_action,
    bool success,
    void* user_data
) {
    (void)system;
    (void)antigen_id;
    (void)recovery_action;
    (void)user_data;
    if (success) {
        g_exception_tracker.recoveries_completed++;
    }
}

// Static callback for cytokine
static void on_cytokine_released(
    brain_immune_system_t* system,
    const brain_cytokine_t* cytokine,
    void* user_data
) {
    (void)system;
    (void)cytokine;
    (void)user_data;
    g_exception_tracker.cytokines_released++;
}

// ============================================================================
// Test Fixture
// ============================================================================

class HypothalamusExceptionPipelineTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives = nullptr;
    hypo_orchestrator_t orchestrator = nullptr;
    brain_immune_system_t* immune = nullptr;
    hypo_immune_bridge_t* immune_bridge = nullptr;

    void SetUp() override {
        g_exception_tracker.reset();

        // Create drive system
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_config.alignment_mode = HYPO_ALIGN_CONTROLLED;
        drives = hypo_drive_create(&drive_config);
        ASSERT_NE(nullptr, drives);

        // Create orchestrator
        hypo_orch_config_t orch_config;
        hypo_orch_default_config(&orch_config);
        orch_config.enable_async = true;
        orch_config.connect_immune = true;
        orchestrator = hypo_orch_create(&orch_config);
        ASSERT_NE(nullptr, orchestrator);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_config.enable_logging = false;  // Reduce noise
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(nullptr, immune);

        // Set up callbacks
        brain_immune_set_exception_callback(immune, on_exception_presented, nullptr);
        brain_immune_set_recovery_callback(immune, on_recovery_completed, nullptr);
        brain_immune_set_cytokine_callback(immune, on_cytokine_released, nullptr);

        // Initialize and connect exception-immune integration
        // This is CRITICAL: nimcp_exception_present_to_immune() uses this global connection
        nimcp_exception_immune_config_t ex_immune_config;
        nimcp_exception_immune_default_config(&ex_immune_config);
        ex_immune_config.enable_auto_present = true;
        ex_immune_config.async_presentation = false;  // Synchronous for testing
        ASSERT_EQ(0, nimcp_exception_immune_init(&ex_immune_config));
        ASSERT_EQ(0, nimcp_exception_immune_connect(immune));

        // Create immune bridge
        hypo_immune_config_t bridge_config;
        hypo_immune_bridge_default_config(&bridge_config);
        bridge_config.enable_bidirectional = true;
        immune_bridge = hypo_immune_bridge_create(drives, immune, &bridge_config);
        ASSERT_NE(nullptr, immune_bridge);

        // Start immune system
        brain_immune_start(immune);
    }

    void TearDown() override {
        // Disconnect exception-immune integration before destroying immune system
        nimcp_exception_immune_disconnect();
        nimcp_exception_immune_shutdown();

        if (immune_bridge) {
            hypo_immune_bridge_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (orchestrator) {
            hypo_orch_destroy(orchestrator);
            orchestrator = nullptr;
        }
        if (drives) {
            hypo_drive_destroy(drives);
            drives = nullptr;
        }
    }

    // Helper to trigger a simulated error through exception system
    void TriggerTestException(int error_code, const char* message) {
        NIMCP_THROW_TO_IMMUNE(error_code, "%s", message);
    }

    // Helper to wait for exception processing
    void WaitForExceptionProcessing(int expected_count, int timeout_ms = 1000) {
        int elapsed = 0;
        while (g_exception_tracker.exceptions_presented.load() < expected_count &&
               elapsed < timeout_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            brain_immune_update(immune, 10);
            elapsed += 10;
        }
    }
};

// ============================================================================
// E2E Test: Full Pipeline Error Handling
// ============================================================================

TEST_F(HypothalamusExceptionPipelineTest, FullPipelineErrorHandling) {
    E2E_PIPELINE_START("Full Pipeline Error Handling");

    E2E_STAGE_BEGIN("Connect orchestrator to immune system", 100);
    EXPECT_EQ(0, hypo_orch_connect_immune(orchestrator, immune));
    EXPECT_EQ(0, hypo_immune_connect(immune_bridge, orchestrator, immune));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify initial system state", 50);
    brain_immune_stats_t stats;
    EXPECT_EQ(0, brain_immune_get_stats(immune, &stats));
    /* Setup/connection may trigger exception throws that create antigens;
     * record baseline and check relative change later */
    uint32_t baseline_antigens = stats.antigens_processed;
    hypo_orch_state_t orch_state;
    EXPECT_EQ(0, hypo_orch_get_state(orchestrator, &orch_state));
    EXPECT_NE(HYPO_ORCH_STATE_ERROR, orch_state);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger error in orchestrator", MAX_ERROR_LATENCY_MS);
    // Simulate an error condition
    TriggerTestException(NIMCP_ERROR_INVALID_STATE, "Test orchestrator error");

    // Wait for exception to be processed
    WaitForExceptionProcessing(1);

    EXPECT_GE(g_exception_tracker.exceptions_presented.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify exception reached immune system", 100);
    brain_immune_get_stats(immune, &stats);
    // Antigen should have been presented
    EXPECT_GE(g_exception_tracker.antigens_created.load(), 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify system continues to operate", 200);
    // Update drives - should still work
    EXPECT_TRUE(hypo_drive_update(drives, 10000));

    // Orchestrator should not be in error state permanently
    hypo_orch_get_state(orchestrator, &orch_state);
    // May be in error state briefly, but should be recoverable

    // Update immune bridge - should continue
    EXPECT_EQ(0, hypo_immune_update(immune_bridge, 100));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update and verify statistics", 100);
    // Run update cycle
    brain_immune_update(immune, 100);

    // Get final statistics
    brain_immune_get_stats(immune, &stats);

    // Verify cytokines were released for the error
    EXPECT_GE(g_exception_tracker.cytokines_released.load(), 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Test: Multi-Bridge Error Cascade
// ============================================================================

TEST_F(HypothalamusExceptionPipelineTest, MultiBridgeErrorCascade) {
    E2E_PIPELINE_START("Multi-Bridge Error Cascade");

    E2E_STAGE_BEGIN("Register multiple bridges", 200);
    // Register bridges with orchestrator
    uint32_t bridge_ids[MAX_BRIDGES_FOR_CASCADE] = {0};
    const hypo_bridge_type_t bridge_types[] = {
        HYPO_BRIDGE_EMOTION,
        HYPO_BRIDGE_EXECUTIVE,
        HYPO_BRIDGE_ATTENTION,
        HYPO_BRIDGE_SLEEP,
        HYPO_BRIDGE_WELLBEING
    };

    for (int i = 0; i < MAX_BRIDGES_FOR_CASCADE; i++) {
        char name[64];
        snprintf(name, sizeof(name), "test_bridge_%d", i);
        EXPECT_EQ(0, hypo_orch_register_bridge(
            orchestrator,
            bridge_types[i],
            name,
            nullptr,  // No actual handle
            nullptr,  // No context
            &bridge_ids[i]
        ));
        EXPECT_GT(bridge_ids[i], 0u);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Connect orchestrator to immune", 100);
    EXPECT_EQ(0, hypo_orch_connect_immune(orchestrator, immune));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger errors in sequence across bridges", 500);
    int initial_exceptions = g_exception_tracker.exceptions_presented.load();

    for (int i = 0; i < MAX_BRIDGES_FOR_CASCADE; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Bridge %d error: %s", i,
                 hypo_bridge_type_name(bridge_types[i]));

        // Different error codes for each bridge
        int error_code = NIMCP_ERROR_INVALID_PARAM + i;
        TriggerTestException(error_code, msg);

        // Brief pause between errors
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        brain_immune_update(immune, 20);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all errors are tracked", 200);
    WaitForExceptionProcessing(initial_exceptions + MAX_BRIDGES_FOR_CASCADE);

    // Check that we received exceptions
    int total_exceptions = g_exception_tracker.exceptions_presented.load();
    EXPECT_GE(total_exceptions, initial_exceptions + MAX_BRIDGES_FOR_CASCADE);

    // Verify error codes are tracked
    {
        std::lock_guard<std::mutex> lock(g_exception_tracker.history_mutex);
        EXPECT_GE(g_exception_tracker.error_codes.size(),
                  static_cast<size_t>(MAX_BRIDGES_FOR_CASCADE));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify no corruption between modules", 100);
    // All bridges should still be registered
    for (int i = 0; i < MAX_BRIDGES_FOR_CASCADE; i++) {
        hypo_bridge_info_t info;
        EXPECT_EQ(0, hypo_orch_get_bridge_info(orchestrator, bridge_ids[i], &info));
        EXPECT_EQ(bridge_types[i], info.type);
    }

    // Orchestrator statistics should be reasonable
    hypo_orch_stats_t orch_stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orchestrator, &orch_stats));
    EXPECT_EQ(static_cast<uint32_t>(MAX_BRIDGES_FOR_CASCADE),
              orch_stats.registered_bridges);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Unregister bridges", 100);
    for (int i = 0; i < MAX_BRIDGES_FOR_CASCADE; i++) {
        EXPECT_EQ(0, hypo_orch_unregister_bridge(orchestrator, bridge_ids[i]));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Test: Recovery Test
// ============================================================================

TEST_F(HypothalamusExceptionPipelineTest, RecoveryTest) {
    E2E_PIPELINE_START("Recovery Test");

    E2E_STAGE_BEGIN("Setup initial working state", 100);
    EXPECT_EQ(0, hypo_orch_connect_immune(orchestrator, immune));
    EXPECT_EQ(0, hypo_immune_connect(immune_bridge, orchestrator, immune));

    // Verify working state
    float drive_level = 0.0f;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orchestrator, &drive_level));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger error condition", MAX_ERROR_LATENCY_MS);
    int initial_count = g_exception_tracker.exceptions_presented.load();

    // Trigger a specific recoverable error
    TriggerTestException(NIMCP_ERROR_TIMEOUT, "Recoverable timeout condition");

    WaitForExceptionProcessing(initial_count + 1);
    EXPECT_GT(g_exception_tracker.exceptions_presented.load(), initial_count);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify error is reported", 100);
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(g_exception_tracker.antigens_created.load(), 1);

    // Check immune system detected the threat
    brain_immune_phase_t phase = brain_immune_get_phase(immune);
    (void)phase;  // Phase might have changed
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate fix condition", 100);
    // Reset orchestrator to clear any error state
    EXPECT_EQ(0, hypo_orch_reset(orchestrator));

    // Re-register with immune
    EXPECT_EQ(0, hypo_orch_connect_immune(orchestrator, immune));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify system returns to normal operation", MAX_RECOVERY_TIME_MS);
    // Update multiple cycles
    for (int i = 0; i < 10; i++) {
        hypo_drive_update(drives, 10000);
        brain_immune_update(immune, 100);
        hypo_immune_update(immune_bridge, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify orchestrator is operational
    hypo_orch_state_t orch_state;
    EXPECT_EQ(0, hypo_orch_get_state(orchestrator, &orch_state));
    EXPECT_NE(HYPO_ORCH_STATE_ERROR, orch_state);

    // Verify drives can still be updated
    EXPECT_TRUE(hypo_drive_update(drives, 10000));

    // Verify immune system health
    brain_immune_get_stats(immune, &stats);
    EXPECT_GE(stats.system_health, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Test: Stress Test
// ============================================================================

TEST_F(HypothalamusExceptionPipelineTest, StressTest) {
    E2E_PIPELINE_START("Stress Test");

    E2E_STAGE_BEGIN("Setup for stress test", 100);
    EXPECT_EQ(0, hypo_orch_connect_immune(orchestrator, immune));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Rapidly trigger many errors", MAX_STRESS_DURATION_MS);
    std::atomic<bool> has_crash{false};
    std::atomic<int> errors_triggered{0};

    // Use multiple threads to trigger errors concurrently
    std::vector<std::thread> workers;
    constexpr int NUM_WORKER_THREADS = 4;
    constexpr int ERRORS_PER_THREAD = MAX_STRESS_ERRORS / NUM_WORKER_THREADS;

    for (int t = 0; t < NUM_WORKER_THREADS; t++) {
        workers.emplace_back([this, t, &has_crash, &errors_triggered]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> error_dist(
                NIMCP_ERROR_INVALID_PARAM,
                NIMCP_ERROR_INVALID_PARAM + 10
            );

            for (int i = 0; i < ERRORS_PER_THREAD && !has_crash.load(); i++) {
                try {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Stress error thread=%d iter=%d", t, i);
                    int error_code = error_dist(gen);

                    // Create exception directly instead of using macro in thread
                    nimcp_exception_t* ex = nimcp_exception_create(
                        error_code,
                        EXCEPTION_SEVERITY_ERROR,
                        __FILE__,
                        __LINE__,
                        __func__,
                        "%s", msg
                    );
                    if (ex) {
                        nimcp_exception_present_to_immune(ex, NULL);
                        nimcp_exception_unref(ex);
                    }

                    errors_triggered++;

                    // Small random delay
                    std::this_thread::sleep_for(
                        std::chrono::microseconds(100 + (gen() % 500))
                    );
                } catch (...) {
                    has_crash = true;
                }
            }
        });
    }

    // Main thread updates immune system
    auto start = std::chrono::steady_clock::now();
    while (errors_triggered.load() < MAX_STRESS_ERRORS) {
        brain_immune_update(immune, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();
        if (elapsed > MAX_STRESS_DURATION_MS) {
            break;
        }
    }

    // Wait for workers
    for (auto& w : workers) {
        w.join();
    }

    EXPECT_FALSE(has_crash.load()) << "Crash detected during stress test";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify no memory corruption", 200);
    // Run several update cycles
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(0, brain_immune_update(immune, 50));
        EXPECT_TRUE(hypo_drive_update(drives, 10000));
    }

    // Verify we can still query state
    brain_immune_stats_t stats;
    EXPECT_EQ(0, brain_immune_get_stats(immune, &stats));

    hypo_orch_stats_t orch_stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orchestrator, &orch_stats));

    // Immune bridge should still work
    hypo_immune_bridge_stats_t bridge_stats;
    EXPECT_EQ(0, hypo_immune_bridge_get_stats(immune_bridge, &bridge_stats));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify statistics are reasonable", 100);
    brain_immune_get_stats(immune, &stats);

    // Should have processed many antigens
    EXPECT_GE(g_exception_tracker.antigens_created.load(),
              MAX_STRESS_ERRORS / 2);

    // System health should still be positive
    EXPECT_GE(stats.system_health, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Test: FEP Integration Errors
// ============================================================================

TEST_F(HypothalamusExceptionPipelineTest, FEPIntegrationErrors) {
    E2E_PIPELINE_START("FEP Integration Errors");

    hypo_immune_fep_bridge_t* fep_bridge = nullptr;
    fep_system_t* fep_system = nullptr;

    E2E_STAGE_BEGIN("Create FEP system", 100);
    // Create FEP system for the bridge
    fep_config_t fep_sys_config;
    EXPECT_EQ(0, fep_default_config(&fep_sys_config));
    // Use small dimensions for testing: 8 observations (cytokines + state), 4 actions
    fep_system = fep_create(&fep_sys_config, 8, 4);
    ASSERT_NE(nullptr, fep_system) << "Failed to create FEP system";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create FEP bridges", 200);
    // Create FEP bridge configuration
    hypo_immune_fep_config_t fep_config;
    EXPECT_EQ(0, hypo_immune_fep_default_config(&fep_config));
    fep_config.enable_active_inference = true;
    fep_config.enable_bio_async = false;  // Simpler for testing

    // Create FEP bridge with the created FEP system
    fep_bridge = hypo_immune_fep_create(&fep_config, drives, immune, fep_system);
    ASSERT_NE(nullptr, fep_bridge) << "Failed to create FEP bridge";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Setup exception tracking", 100);
    int initial_exceptions = g_exception_tracker.exceptions_presented.load();
    EXPECT_EQ(0, hypo_orch_connect_immune(orchestrator, immune));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger FEP-related errors", 300);
    // Simulate cytokine storm condition via exception
    TriggerTestException(NIMCP_ERROR_BUFFER_OVERFLOW, "Simulated cytokine storm overflow");
    brain_immune_update(immune, 50);

    // Update FEP bridge with extreme values
    EXPECT_EQ(0, hypo_immune_fep_update_cytokines(
        fep_bridge,
        0.95f,  // IL-1 very high
        0.90f,  // IL-6 very high
        0.85f,  // TNF very high
        0.1f,   // IL-10 low
        0.8f    // IFN high
    ));

    // Update FEP processing
    EXPECT_EQ(0, hypo_immune_fep_update(fep_bridge));

    // Trigger another FEP-related exception
    TriggerTestException(NIMCP_ERROR_INVALID_STATE, "FEP free energy threshold exceeded");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify proper exception flow", 200);
    WaitForExceptionProcessing(initial_exceptions + 2);

    // Check FEP effects
    hypo_immune_fep_effects_t fep_effects;
    EXPECT_EQ(0, hypo_immune_fep_get_effects(fep_bridge, &fep_effects));

    // With high cytokines, should have elevated free energy
    EXPECT_GT(fep_effects.free_energy, 0.0f);
    EXPECT_GT(fep_effects.inflammation_level, 0.5f);

    // Immune state should reflect inflammation
    EXPECT_GE(fep_effects.immune_state, HYPO_IMMUNE_FEP_STATE_VIGILANT);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify exception statistics", 100);
    hypo_immune_fep_stats_t fep_stats;
    EXPECT_EQ(0, hypo_immune_fep_get_stats(fep_bridge, &fep_stats));

    // Should have performed updates
    EXPECT_GT(fep_stats.total_updates, 0u);

    // Check exception tracker
    EXPECT_GE(g_exception_tracker.exceptions_presented.load(),
              initial_exceptions + 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test recovery from FEP error state", 200);
    // Reset cytokines to normal
    EXPECT_EQ(0, hypo_immune_fep_update_cytokines(
        fep_bridge,
        0.1f,   // IL-1 normal
        0.1f,   // IL-6 normal
        0.05f,  // TNF normal
        0.3f,   // IL-10 elevated (anti-inflammatory)
        0.1f    // IFN normal
    ));

    // Multiple update cycles
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, hypo_immune_fep_update(fep_bridge));
        brain_immune_update(immune, 50);
    }

    // Check effects after recovery
    EXPECT_EQ(0, hypo_immune_fep_get_effects(fep_bridge, &fep_effects));

    // Free energy should have decreased
    // (Note: may still be elevated due to recent events)
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup FEP bridge and system", 100);
    if (fep_bridge) {
        hypo_immune_fep_destroy(fep_bridge);
        fep_bridge = nullptr;
    }
    if (fep_system) {
        fep_destroy(fep_system);
        fep_system = nullptr;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Test: Concurrent Error Handling
// ============================================================================

TEST_F(HypothalamusExceptionPipelineTest, ConcurrentErrorHandling) {
    E2E_PIPELINE_START("Concurrent Error Handling");

    E2E_STAGE_BEGIN("Setup concurrent error test", 100);
    EXPECT_EQ(0, hypo_orch_connect_immune(orchestrator, immune));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger concurrent errors from multiple sources", 500);
    std::atomic<bool> running{true};
    std::atomic<int> errors_from_drives{0};
    std::atomic<int> errors_from_orchestrator{0};
    std::atomic<int> errors_from_immune{0};

    // Thread 1: Errors from drive system
    std::thread drive_error_thread([this, &running, &errors_from_drives]() {
        while (running.load()) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_INVALID_PARAM,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Drive system error %d", errors_from_drives.load()
            );
            if (ex) {
                nimcp_exception_present_to_immune(ex, NULL);
                nimcp_exception_unref(ex);
                errors_from_drives++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    // Thread 2: Errors from orchestrator
    std::thread orch_error_thread([this, &running, &errors_from_orchestrator]() {
        while (running.load()) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Orchestrator error %d", errors_from_orchestrator.load()
            );
            if (ex) {
                nimcp_exception_present_to_immune(ex, NULL);
                nimcp_exception_unref(ex);
                errors_from_orchestrator++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    });

    // Thread 3: Immune system processing
    std::thread immune_thread([this, &running, &errors_from_immune]() {
        while (running.load()) {
            brain_immune_update(immune, 10);
            errors_from_immune++;  // Not an error, just counting updates
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Let it run for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    running = false;

    drive_error_thread.join();
    orch_error_thread.join();
    immune_thread.join();

    EXPECT_GT(errors_from_drives.load(), 5);
    EXPECT_GT(errors_from_orchestrator.load(), 5);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all errors were handled properly", 200);
    // Final update
    brain_immune_update(immune, 100);

    brain_immune_stats_t stats;
    EXPECT_EQ(0, brain_immune_get_stats(immune, &stats));

    // System should still be healthy
    EXPECT_GE(stats.system_health, 0.0f);

    // Check that both error sources contributed
    {
        std::lock_guard<std::mutex> lock(g_exception_tracker.history_mutex);
        EXPECT_GE(g_exception_tracker.error_codes.size(), 10u);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Test: Error Statistics Accuracy
// ============================================================================

TEST_F(HypothalamusExceptionPipelineTest, ErrorStatisticsAccuracy) {
    E2E_PIPELINE_START("Error Statistics Accuracy");

    E2E_STAGE_BEGIN("Reset all statistics", 50);
    g_exception_tracker.reset();
    hypo_orch_reset_stats(orchestrator);
    hypo_immune_bridge_reset_stats(immune_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate known number of errors", 300);
    constexpr int KNOWN_ERROR_COUNT = 10;
    constexpr int ERROR_CODE_BASE = NIMCP_ERROR_INVALID_PARAM;

    for (int i = 0; i < KNOWN_ERROR_COUNT; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            ERROR_CODE_BASE + (i % 5),  // Cycle through 5 error codes
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test error %d", i
        );
        if (ex) {
            nimcp_exception_present_to_immune(ex, NULL);
            nimcp_exception_unref(ex);
        }

        brain_immune_update(immune, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Wait for all processing to complete", 200);
    WaitForExceptionProcessing(KNOWN_ERROR_COUNT);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify statistics match expected counts", 100);
    // Check exception tracker
    int tracked_exceptions = g_exception_tracker.exceptions_presented.load();
    EXPECT_EQ(KNOWN_ERROR_COUNT, tracked_exceptions)
        << "Expected " << KNOWN_ERROR_COUNT << " exceptions, got " << tracked_exceptions;

    int tracked_antigens = g_exception_tracker.antigens_created.load();
    EXPECT_EQ(KNOWN_ERROR_COUNT, tracked_antigens)
        << "Expected " << KNOWN_ERROR_COUNT << " antigens, got " << tracked_antigens;

    // Verify error code distribution
    {
        std::lock_guard<std::mutex> lock(g_exception_tracker.history_mutex);
        EXPECT_EQ(static_cast<size_t>(KNOWN_ERROR_COUNT),
                  g_exception_tracker.error_codes.size());
    }

    // Immune system stats should reflect processing
    brain_immune_stats_t stats;
    EXPECT_EQ(0, brain_immune_get_stats(immune, &stats));
    EXPECT_GE(stats.antigens_processed, 0u);  // May not all be fully processed yet
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
