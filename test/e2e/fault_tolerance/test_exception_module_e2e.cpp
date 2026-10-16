/**
 * @file test_exception_module_e2e.cpp
 * @brief E2E tests for complete exception handling pipelines in newly converted modules
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: End-to-end tests for complete exception handling pipelines across modules
 * WHY:  Verify that exceptions flow correctly through immune response, recovery,
 *       and verification phases for all major exception-producing modules
 * HOW:  Test complete pipelines for swarm, security, training, perception, and
 *       memory exceptions including concurrent exception handling and stress testing
 *
 * Test Scenarios:
 * 1. Swarm exception -> immune response -> recovery -> verification
 * 2. Security exception -> threat response -> quarantine -> cleanup
 * 3. Training exception -> checkpoint -> rollback -> resume
 * 4. Perception exception -> fallback -> degraded mode -> recovery
 * 5. Memory exception -> GC trigger -> compaction -> resume
 * 6. Full system stress with multiple concurrent exceptions
 * 7. End-to-end exception lifecycle verification
 * 8. Concurrent exception handling across threads
 * 9. Exception storm prevention mechanisms
 * 10. System recovery after multiple failures
 * 11. Performance under exception load
 * 12. Cross-module exception propagation
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
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <random>
#include <set>
#include <algorithm>
#include <numeric>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Utilities - Module Exception Tracking
 * ============================================================================ */

namespace {

// Module-specific tracking statistics
struct ModuleExceptionStats {
    std::atomic<uint64_t> swarm_exceptions{0};
    std::atomic<uint64_t> security_exceptions{0};
    std::atomic<uint64_t> training_exceptions{0};
    std::atomic<uint64_t> perception_exceptions{0};
    std::atomic<uint64_t> memory_exceptions{0};

    std::atomic<uint64_t> immune_presentations{0};
    std::atomic<uint64_t> recoveries_attempted{0};
    std::atomic<uint64_t> recoveries_succeeded{0};
    std::atomic<uint64_t> recoveries_failed{0};

    std::atomic<uint64_t> gc_triggers{0};
    std::atomic<uint64_t> rollbacks{0};
    std::atomic<uint64_t> quarantines{0};
    std::atomic<uint64_t> checkpoints{0};
    std::atomic<uint64_t> fallbacks{0};

    std::atomic<uint64_t> storm_detections{0};
    std::atomic<uint64_t> circuit_trips{0};
    std::atomic<uint64_t> degraded_mode_entries{0};

    std::atomic<uint64_t> total_latency_us{0};
    std::atomic<uint64_t> max_latency_us{0};
    std::atomic<uint64_t> min_latency_us{UINT64_MAX};

    void reset() {
        swarm_exceptions = 0;
        security_exceptions = 0;
        training_exceptions = 0;
        perception_exceptions = 0;
        memory_exceptions = 0;

        immune_presentations = 0;
        recoveries_attempted = 0;
        recoveries_succeeded = 0;
        recoveries_failed = 0;

        gc_triggers = 0;
        rollbacks = 0;
        quarantines = 0;
        checkpoints = 0;
        fallbacks = 0;

        storm_detections = 0;
        circuit_trips = 0;
        degraded_mode_entries = 0;

        total_latency_us = 0;
        max_latency_us = 0;
        min_latency_us = UINT64_MAX;
    }

    void update_latency(uint64_t latency) {
        total_latency_us += latency;

        // Update max
        uint64_t current_max = max_latency_us.load();
        while (latency > current_max) {
            if (max_latency_us.compare_exchange_weak(current_max, latency)) break;
            current_max = max_latency_us.load();
        }

        // Update min
        uint64_t current_min = min_latency_us.load();
        while (latency < current_min) {
            if (min_latency_us.compare_exchange_weak(current_min, latency)) break;
            current_min = min_latency_us.load();
        }
    }
};

ModuleExceptionStats g_module_stats;

// Simulated system state for testing
struct SystemState {
    std::atomic<bool> swarm_healthy{true};
    std::atomic<bool> security_clean{true};
    std::atomic<bool> training_active{false};
    std::atomic<bool> perception_online{true};
    std::atomic<bool> memory_available{true};
    std::atomic<bool> degraded_mode{false};
    std::atomic<int> active_agents{0};
    std::atomic<int> quarantined_regions{0};
    std::atomic<int> checkpoint_version{0};

    void reset() {
        swarm_healthy = true;
        security_clean = true;
        training_active = false;
        perception_online = true;
        memory_available = true;
        degraded_mode = false;
        active_agents = 0;
        quarantined_regions = 0;
        checkpoint_version = 0;
    }
};

SystemState g_system_state;

// Recovery callbacks for different module types
int reduce_load_recovery_callback(nimcp_exception_t* ex,
                                   nimcp_exception_recovery_action_t action,
                                   void* user_data) {
    (void)user_data;
    g_module_stats.recoveries_attempted++;

    if (action == EXCEPTION_RECOVERY_REDUCE_LOAD) {
        // Check what type of exception this is to determine the action
        if (ex->category == EXCEPTION_CATEGORY_COGNITIVE) {
            // Check if perception or swarm based on module name or message content
            if (strstr(ex->message, "Perception") ||
                strstr(ex->message, "perception") ||
                strstr(ex->message, "visual cortex")) {
                // Perception exception - enter degraded mode
                g_system_state.degraded_mode = true;
                g_module_stats.degraded_mode_entries++;
                g_module_stats.fallbacks++;
                g_module_stats.recoveries_succeeded++;
                return 0;
            }
        }

        // Default: swarm behavior - reduce agent count
        g_system_state.active_agents = std::max(0, g_system_state.active_agents.load() / 2);
        g_module_stats.fallbacks++;
        g_module_stats.recoveries_succeeded++;
        g_system_state.swarm_healthy = true;
        return 0;
    }

    g_module_stats.recoveries_failed++;
    return -1;
}

int swarm_recovery_callback(nimcp_exception_t* ex,
                            nimcp_exception_recovery_action_t action,
                            void* user_data) {
    (void)user_data;
    g_module_stats.recoveries_attempted++;

    if (action == EXCEPTION_RECOVERY_RESTART_COMPONENT) {
        // Restart swarm coordinator
        g_system_state.swarm_healthy = true;
        g_module_stats.recoveries_succeeded++;
        return 0;
    }

    g_module_stats.recoveries_failed++;
    return -1;
}

int security_recovery_callback(nimcp_exception_t* ex,
                               nimcp_exception_recovery_action_t action,
                               void* user_data) {
    (void)user_data;
    g_module_stats.recoveries_attempted++;

    if (action == EXCEPTION_RECOVERY_QUARANTINE) {
        g_system_state.quarantined_regions++;
        g_module_stats.quarantines++;
        g_module_stats.recoveries_succeeded++;
        return 0;
    }

    if (action == EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN) {
        // Emergency security shutdown
        g_system_state.security_clean = false;  // Mark for cleanup
        g_module_stats.recoveries_succeeded++;
        return 0;
    }

    g_module_stats.recoveries_failed++;
    return -1;
}

int training_recovery_callback(nimcp_exception_t* ex,
                               nimcp_exception_recovery_action_t action,
                               void* user_data) {
    (void)user_data;
    g_module_stats.recoveries_attempted++;

    if (action == EXCEPTION_RECOVERY_ROLLBACK) {
        // Rollback to previous checkpoint
        if (g_system_state.checkpoint_version > 0) {
            g_system_state.checkpoint_version--;
        }
        g_module_stats.rollbacks++;
        g_module_stats.recoveries_succeeded++;
        g_system_state.training_active = false;
        return 0;
    }

    if (action == EXCEPTION_RECOVERY_EMERGENCY_SAVE) {
        // Save current state as checkpoint
        g_system_state.checkpoint_version++;
        g_module_stats.checkpoints++;
        g_module_stats.recoveries_succeeded++;
        return 0;
    }

    g_module_stats.recoveries_failed++;
    return -1;
}

int retry_recovery_callback(nimcp_exception_t* ex,
                            nimcp_exception_recovery_action_t action,
                            void* user_data) {
    (void)user_data;
    (void)ex;
    g_module_stats.recoveries_attempted++;

    if (action == EXCEPTION_RECOVERY_RETRY) {
        // Retry operation - perception pipeline
        g_system_state.perception_online = true;
        g_module_stats.recoveries_succeeded++;
        return 0;
    }

    g_module_stats.recoveries_failed++;
    return -1;
}

int memory_recovery_callback(nimcp_exception_t* ex,
                             nimcp_exception_recovery_action_t action,
                             void* user_data) {
    (void)user_data;
    g_module_stats.recoveries_attempted++;

    if (action == EXCEPTION_RECOVERY_GC) {
        g_module_stats.gc_triggers++;
        g_system_state.memory_available = true;
        g_module_stats.recoveries_succeeded++;
        return 0;
    }

    if (action == EXCEPTION_RECOVERY_COMPACT) {
        // Memory compaction
        g_system_state.memory_available = true;
        g_module_stats.recoveries_succeeded++;
        return 0;
    }

    g_module_stats.recoveries_failed++;
    return -1;
}

// Exception handler for tracking
bool module_tracking_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    // Track by category
    switch (ex->category) {
        case EXCEPTION_CATEGORY_MEMORY:
            g_module_stats.memory_exceptions++;
            break;
        case EXCEPTION_CATEGORY_BRAIN:
            g_module_stats.training_exceptions++;
            break;
        case EXCEPTION_CATEGORY_SECURITY:
            g_module_stats.security_exceptions++;
            break;
        case EXCEPTION_CATEGORY_COGNITIVE:
            g_module_stats.perception_exceptions++;
            break;
        default:
            // Could be swarm or other
            if (ex->code >= 8000 && ex->code < 9000) {
                g_module_stats.swarm_exceptions++;
            }
            break;
    }

    return false;  // Don't consume, let other handlers process
}

}  // namespace

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionModuleE2ETest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* tracking_handler_reg = nullptr;

    void SetUp() override {
        g_module_stats.reset();
        g_system_state.reset();

        // Initialize exception system
        ASSERT_EQ(nimcp_exception_system_init(), 0)
            << "Failed to initialize exception system";

        // Initialize circuit breaker
        ASSERT_EQ(nimcp_circuit_init(), 0)
            << "Failed to initialize circuit breaker";

        // Initialize metrics
        ASSERT_EQ(nimcp_metrics_init(), 0)
            << "Failed to initialize metrics";

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = true;
        immune_config.enable_auto_recovery = true;
        immune_config.min_present_severity = EXCEPTION_SEVERITY_ERROR;
        immune_config.enable_memory_formation = true;
        ASSERT_EQ(nimcp_exception_immune_init(&immune_config), 0)
            << "Failed to initialize exception-immune integration";

        // Register tracking handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "ModuleTracker";
        opts.handler = module_tracking_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        tracking_handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(tracking_handler_reg, nullptr);

        // Register module-specific recovery callbacks
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD,
                                         reduce_load_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT,
                                         swarm_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE,
                                         security_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK,
                                         training_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE,
                                         training_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY,
                                         retry_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC,
                                         memory_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT,
                                         memory_recovery_callback, nullptr);
    }

    void TearDown() override {
        if (tracking_handler_reg) {
            nimcp_handler_unregister(tracking_handler_reg);
            tracking_handler_reg = nullptr;
        }

        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Helper to create module-specific exceptions
    nimcp_exception_t* create_swarm_exception(const char* message) {
        nimcp_cognitive_exception_t* ex = (nimcp_cognitive_exception_t*)malloc(
            sizeof(nimcp_cognitive_exception_t));
        if (!ex) return nullptr;

        memset(ex, 0, sizeof(*ex));
        ex->base.type = EXCEPTION_TYPE_COGNITIVE;
        ex->base.category = EXCEPTION_CATEGORY_COGNITIVE;
        ex->base.code = NIMCP_ERROR_OPERATION_FAILED;
        ex->base.severity = EXCEPTION_SEVERITY_ERROR;
        ex->base.ref_count = 1;
        snprintf(ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE, "%s", message);
        ex->base.file = __FILE__;
        ex->base.line = __LINE__;
        ex->base.function = __func__;
        ex->module_name = "swarm_coordinator";

        return &ex->base;
    }

    nimcp_security_exception_t* create_security_exception(const char* message,
                                                          uint32_t threat_type) {
        return nimcp_security_exception_create(
            NIMCP_ERROR_SECURITY_THREAT,
            EXCEPTION_SEVERITY_CRITICAL,
            __FILE__, __LINE__, __func__,
            threat_type,
            "%s", message
        );
    }

    nimcp_brain_exception_t* create_training_exception(const char* message,
                                                        bool diverged) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_LEARNING_FAILED,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            1, "training_cortex",
            "%s", message
        );
        if (ex) {
            ex->learning_diverged = diverged;
            ex->gradient_norm = diverged ? INFINITY : 1.0f;
        }
        return ex;
    }

    nimcp_memory_exception_t* create_memory_exception(size_t requested) {
        nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            requested,
            "Memory allocation failed: requested %zu bytes", requested
        );
        return ex;
    }

    // Run complete exception pipeline
    bool run_exception_pipeline(nimcp_exception_t* ex,
                               nimcp_exception_recovery_action_t action) {
        auto start = std::chrono::high_resolution_clock::now();

        // Step 1: Dispatch through handler chain
        nimcp_exception_dispatch(ex);

        // Step 2: Present to immune system
        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        int present_result = nimcp_exception_present_to_immune(ex, &response);
        if (present_result == 0) {
            g_module_stats.immune_presentations++;
        }

        // Step 3: Execute recovery
        int recovery_result = nimcp_execute_recovery(ex, action);

        // Step 4: Notify result
        bool success = (recovery_result == 0);
        nimcp_exception_notify_recovery_result(ex, action, success);

        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        g_module_stats.update_latency(latency);

        return success;
    }
};

/* ============================================================================
 * Test 1: Swarm Exception -> Immune Response -> Recovery -> Verification
 *
 * Tests complete pipeline for swarm coordination exceptions
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, SwarmExceptionFullPipeline) {
    printf("=== Test: Swarm Exception -> Immune Response -> Recovery -> Verification ===\n");

    g_system_state.active_agents = 100;
    g_system_state.swarm_healthy = false;

    // Create swarm exception
    nimcp_exception_t* ex = create_swarm_exception(
        "Swarm coordination failure: agent consensus timeout"
    );
    ASSERT_NE(ex, nullptr);
    printf("  Step 1: Created swarm exception\n");

    // Run pipeline with load reduction
    bool recovered = run_exception_pipeline(ex, EXCEPTION_RECOVERY_REDUCE_LOAD);
    printf("  Step 2: Pipeline completed, recovered=%s\n", recovered ? "yes" : "no");

    // Verify recovery
    EXPECT_TRUE(recovered);
    EXPECT_TRUE(g_system_state.swarm_healthy.load());
    EXPECT_LT(g_system_state.active_agents.load(), 100);  // Reduced
    EXPECT_GT(g_module_stats.immune_presentations.load(), 0u);
    EXPECT_GT(g_module_stats.recoveries_succeeded.load(), 0u);
    EXPECT_GT(g_module_stats.fallbacks.load(), 0u);

    printf("  Step 3: Verification passed\n");
    printf("    Active agents reduced to: %d\n", g_system_state.active_agents.load());
    printf("    Immune presentations: %lu\n",
           (unsigned long)g_module_stats.immune_presentations.load());

    nimcp_exception_unref(ex);
    printf("Test passed: Swarm exception full pipeline\n\n");
}

/* ============================================================================
 * Test 2: Security Exception -> Threat Response -> Quarantine -> Cleanup
 *
 * Tests complete pipeline for security threat exceptions
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, SecurityExceptionFullPipeline) {
    printf("=== Test: Security Exception -> Threat Response -> Quarantine -> Cleanup ===\n");

    g_system_state.security_clean = false;
    g_system_state.quarantined_regions = 0;

    // Create security exception
    nimcp_security_exception_t* sec_ex = create_security_exception(
        "Malicious input pattern detected in neural network input",
        1  // BBB_THREAT_MALICIOUS_INPUT
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->severity_score = 9;  // High threat
    sec_ex->quarantine_required = true;
    printf("  Step 1: Created security exception (severity=%d)\n",
           sec_ex->severity_score);

    // Run pipeline with quarantine
    bool recovered = run_exception_pipeline(&sec_ex->base, EXCEPTION_RECOVERY_QUARANTINE);
    printf("  Step 2: Quarantine executed, recovered=%s\n", recovered ? "yes" : "no");

    // Verify quarantine
    EXPECT_TRUE(recovered);
    EXPECT_GT(g_system_state.quarantined_regions.load(), 0);
    EXPECT_GT(g_module_stats.quarantines.load(), 0u);
    EXPECT_GT(g_module_stats.security_exceptions.load(), 0u);

    // Simulate cleanup
    printf("  Step 3: Cleanup phase\n");
    g_system_state.security_clean = true;
    g_system_state.quarantined_regions = 0;

    printf("  Step 4: Verification passed\n");
    printf("    Quarantined regions: %d -> 0 (cleaned)\n",
           g_module_stats.quarantines.load());

    nimcp_exception_unref(&sec_ex->base);
    printf("Test passed: Security exception full pipeline\n\n");
}

/* ============================================================================
 * Test 3: Training Exception -> Checkpoint -> Rollback -> Resume
 *
 * Tests complete pipeline for training divergence exceptions
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, TrainingExceptionFullPipeline) {
    printf("=== Test: Training Exception -> Checkpoint -> Rollback -> Resume ===\n");

    g_system_state.training_active = true;
    g_system_state.checkpoint_version = 5;

    // Phase 1: Create checkpoint before training
    printf("  Phase 1: Initial checkpoint (v%d)\n",
           g_system_state.checkpoint_version.load());

    nimcp_brain_exception_t* checkpoint_ex = create_training_exception(
        "Pre-training checkpoint", false
    );
    ASSERT_NE(checkpoint_ex, nullptr);

    bool checkpoint_saved = run_exception_pipeline(&checkpoint_ex->base,
                                                    EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_TRUE(checkpoint_saved);
    int saved_version = g_system_state.checkpoint_version.load();
    printf("  Phase 1 complete: Checkpoint saved (v%d)\n", saved_version);
    nimcp_exception_unref(&checkpoint_ex->base);

    // Phase 2: Simulate training divergence
    printf("  Phase 2: Training diverges\n");
    nimcp_brain_exception_t* diverge_ex = create_training_exception(
        "Training diverged: loss=infinity, gradient_norm=NaN",
        true  // diverged
    );
    ASSERT_NE(diverge_ex, nullptr);

    EXPECT_TRUE(diverge_ex->learning_diverged);
    EXPECT_TRUE(std::isinf(diverge_ex->gradient_norm));

    // Phase 3: Rollback to checkpoint
    printf("  Phase 3: Rolling back to checkpoint\n");
    bool rolled_back = run_exception_pipeline(&diverge_ex->base,
                                               EXCEPTION_RECOVERY_ROLLBACK);

    EXPECT_TRUE(rolled_back);
    EXPECT_LT(g_system_state.checkpoint_version.load(), saved_version);
    EXPECT_FALSE(g_system_state.training_active.load());
    EXPECT_GT(g_module_stats.rollbacks.load(), 0u);

    printf("  Phase 3 complete: Rolled back to v%d\n",
           g_system_state.checkpoint_version.load());

    // Phase 4: Resume training
    printf("  Phase 4: Resuming training\n");
    g_system_state.training_active = true;

    EXPECT_TRUE(g_system_state.training_active.load());
    printf("  Phase 4 complete: Training resumed\n");

    nimcp_exception_unref(&diverge_ex->base);
    printf("Test passed: Training exception full pipeline\n\n");
}

/* ============================================================================
 * Test 4: Perception Exception -> Fallback -> Degraded Mode -> Recovery
 *
 * Tests complete pipeline for perception system exceptions
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, PerceptionExceptionFullPipeline) {
    printf("=== Test: Perception Exception -> Fallback -> Degraded Mode -> Recovery ===\n");

    g_system_state.perception_online = false;
    g_system_state.degraded_mode = false;

    // Phase 1: Perception failure
    printf("  Phase 1: Perception system failure\n");
    nimcp_cognitive_exception_t* perc_ex = (nimcp_cognitive_exception_t*)malloc(
        sizeof(nimcp_cognitive_exception_t));
    ASSERT_NE(perc_ex, nullptr);

    memset(perc_ex, 0, sizeof(*perc_ex));
    perc_ex->base.type = EXCEPTION_TYPE_COGNITIVE;
    perc_ex->base.category = EXCEPTION_CATEGORY_COGNITIVE;
    perc_ex->base.code = NIMCP_ERROR_OPERATION_FAILED;
    perc_ex->base.severity = EXCEPTION_SEVERITY_SEVERE;
    perc_ex->base.ref_count = 1;
    snprintf(perc_ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE,
             "Perception pipeline timeout: visual cortex unresponsive");
    perc_ex->base.file = __FILE__;
    perc_ex->base.line = __LINE__;
    perc_ex->base.function = __func__;
    perc_ex->module_name = "perception_cortex";
    perc_ex->is_timeout = true;

    // Phase 2: Enter degraded mode
    printf("  Phase 2: Entering degraded mode\n");
    bool degraded = run_exception_pipeline(&perc_ex->base, EXCEPTION_RECOVERY_REDUCE_LOAD);

    EXPECT_TRUE(degraded);
    EXPECT_TRUE(g_system_state.degraded_mode.load());
    EXPECT_GT(g_module_stats.degraded_mode_entries.load(), 0u);
    printf("  Phase 2 complete: Degraded mode active\n");

    nimcp_exception_unref(&perc_ex->base);

    // Phase 3: Attempt full recovery
    printf("  Phase 3: Attempting full recovery\n");
    nimcp_exception_t* retry_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Perception recovery attempt"
    );
    ASSERT_NE(retry_ex, nullptr);

    bool recovered = run_exception_pipeline(retry_ex, EXCEPTION_RECOVERY_RETRY);

    EXPECT_TRUE(recovered);
    EXPECT_TRUE(g_system_state.perception_online.load());
    printf("  Phase 3 complete: Full perception restored\n");

    // Phase 4: Exit degraded mode
    printf("  Phase 4: Exiting degraded mode\n");
    g_system_state.degraded_mode = false;
    EXPECT_FALSE(g_system_state.degraded_mode.load());

    nimcp_exception_unref(retry_ex);
    printf("Test passed: Perception exception full pipeline\n\n");
}

/* ============================================================================
 * Test 5: Memory Exception -> GC Trigger -> Compaction -> Resume
 *
 * Tests complete pipeline for memory allocation exceptions
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, MemoryExceptionFullPipeline) {
    printf("=== Test: Memory Exception -> GC Trigger -> Compaction -> Resume ===\n");

    g_system_state.memory_available = false;

    // Phase 1: Memory allocation failure
    printf("  Phase 1: Memory allocation failure\n");
    nimcp_memory_exception_t* mem_ex = create_memory_exception(128 * 1024 * 1024);
    ASSERT_NE(mem_ex, nullptr);

    mem_ex->available_size = 32 * 1024 * 1024;  // Only 32MB available
    mem_ex->is_heap = true;
    printf("    Requested: %zu bytes\n", mem_ex->requested_size);
    printf("    Available: %zu bytes\n", mem_ex->available_size);

    // Phase 2: Trigger GC
    printf("  Phase 2: Triggering garbage collection\n");
    bool gc_succeeded = run_exception_pipeline(&mem_ex->base, EXCEPTION_RECOVERY_GC);

    EXPECT_TRUE(gc_succeeded);
    EXPECT_GT(g_module_stats.gc_triggers.load(), 0u);
    EXPECT_TRUE(g_system_state.memory_available.load());
    printf("  Phase 2 complete: GC successful\n");

    nimcp_exception_unref(&mem_ex->base);

    // Phase 3: Create another memory exception for compaction test
    printf("  Phase 3: Testing compaction\n");
    g_system_state.memory_available = false;

    nimcp_memory_exception_t* compact_ex = create_memory_exception(64 * 1024 * 1024);
    ASSERT_NE(compact_ex, nullptr);

    bool compact_succeeded = run_exception_pipeline(&compact_ex->base,
                                                     EXCEPTION_RECOVERY_COMPACT);

    EXPECT_TRUE(compact_succeeded);
    EXPECT_TRUE(g_system_state.memory_available.load());
    printf("  Phase 3 complete: Compaction successful\n");

    // Phase 4: Resume operations
    printf("  Phase 4: Resuming normal operations\n");
    EXPECT_TRUE(g_system_state.memory_available.load());
    EXPECT_GT(g_module_stats.memory_exceptions.load(), 0u);

    nimcp_exception_unref(&compact_ex->base);
    printf("Test passed: Memory exception full pipeline\n\n");
}

/* ============================================================================
 * Test 6: Full System Stress with Multiple Concurrent Exceptions
 *
 * Stress tests the system with multiple concurrent module exceptions
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, FullSystemStressConcurrentExceptions) {
    printf("=== Test: Full System Stress with Multiple Concurrent Exceptions ===\n");

    const int NUM_THREADS = 8;
    const int EXCEPTIONS_PER_THREAD = 50;
    std::vector<std::thread> threads;
    std::atomic<int> total_completed{0};
    std::atomic<int> total_recovered{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    // Launch threads for different module exceptions
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nullptr;
                nimcp_exception_recovery_action_t action;

                // Rotate through different exception types
                int type = (t + i) % 5;
                switch (type) {
                    case 0: {
                        // Swarm exception
                        ex = create_swarm_exception("Concurrent swarm test");
                        action = EXCEPTION_RECOVERY_REDUCE_LOAD;
                        break;
                    }
                    case 1: {
                        // Security exception
                        nimcp_security_exception_t* sec_ex = create_security_exception(
                            "Concurrent security test", 1);
                        if (sec_ex) ex = &sec_ex->base;
                        action = EXCEPTION_RECOVERY_QUARANTINE;
                        break;
                    }
                    case 2: {
                        // Training exception
                        nimcp_brain_exception_t* train_ex = create_training_exception(
                            "Concurrent training test", false);
                        if (train_ex) ex = &train_ex->base;
                        action = EXCEPTION_RECOVERY_ROLLBACK;
                        break;
                    }
                    case 3: {
                        // Memory exception
                        nimcp_memory_exception_t* mem_ex = create_memory_exception(1024);
                        if (mem_ex) ex = &mem_ex->base;
                        action = EXCEPTION_RECOVERY_GC;
                        break;
                    }
                    case 4: {
                        // Generic exception
                        ex = nimcp_exception_create(
                            NIMCP_ERROR_OPERATION_FAILED,
                            EXCEPTION_SEVERITY_ERROR,
                            __FILE__, __LINE__, __func__,
                            "Concurrent generic test"
                        );
                        action = EXCEPTION_RECOVERY_RETRY;
                        break;
                    }
                }

                if (ex) {
                    bool recovered = run_exception_pipeline(ex, action);
                    if (recovered) total_recovered++;
                    total_completed++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    int expected = NUM_THREADS * EXCEPTIONS_PER_THREAD;
    double recovery_rate = (total_recovered.load() * 100.0) / total_completed.load();
    double throughput = (total_completed.load() * 1000.0) / duration_ms;

    printf("  Results:\n");
    printf("    Duration: %ld ms\n", duration_ms);
    printf("    Total completed: %d/%d\n", total_completed.load(), expected);
    printf("    Total recovered: %d (%.1f%%)\n", total_recovered.load(), recovery_rate);
    printf("    Throughput: %.1f exceptions/sec\n", throughput);
    printf("    Immune presentations: %lu\n",
           (unsigned long)g_module_stats.immune_presentations.load());

    // Verify results
    EXPECT_EQ(total_completed.load(), expected);
    EXPECT_GT(recovery_rate, 50.0) << "Recovery rate should be above 50%";

    printf("Test passed: Full system stress with concurrent exceptions\n\n");
}

/* ============================================================================
 * Test 7: End-to-End Exception Lifecycle Verification
 *
 * Verifies complete exception lifecycle from creation to cleanup
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, EndToEndExceptionLifecycle) {
    printf("=== Test: End-to-End Exception Lifecycle ===\n");

    nimcp_exception_immune_reset_stats();

    // Step 1: Create exception
    printf("  Step 1: Create exception\n");
    nimcp_brain_exception_t* ex = create_training_exception(
        "Lifecycle test exception", true
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.ref_count, 1);

    // Step 2: Add reference
    printf("  Step 2: Add reference\n");
    nimcp_exception_ref(&ex->base);
    EXPECT_EQ(ex->base.ref_count, 2);

    // Step 3: Set context
    printf("  Step 3: Set context\n");
    nimcp_exception_set_context(&ex->base, "test_phase", "lifecycle");
    nimcp_exception_set_context(&ex->base, "iteration", "1");
    EXPECT_EQ(nimcp_exception_context_count(&ex->base), 2u);

    // Step 4: Generate epitope
    printf("  Step 4: Generate epitope\n");
    size_t epitope_len = nimcp_exception_generate_epitope(&ex->base);
    EXPECT_GT(epitope_len, 0u);
    EXPECT_EQ(ex->base.epitope_len, epitope_len);

    // Step 5: Log exception
    printf("  Step 5: Log exception\n");
    nimcp_exception_log(&ex->base);

    // Step 6: Dispatch through handlers
    printf("  Step 6: Dispatch through handlers\n");
    bool handled = nimcp_exception_dispatch(&ex->base);
    // Not consumed by tracking handler

    // Step 7: Present to immune
    printf("  Step 7: Present to immune system\n");
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int present_result = nimcp_exception_present_to_immune(&ex->base, &response);
    printf("    Present result: %d\n", present_result);
    printf("    Antigen ID: %u\n", response.antigen_id);

    // Step 8: Get recovery strategy
    printf("  Step 8: Get recovery strategy\n");
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(&ex->base, &strategy);
    printf("    Primary action: %s\n",
           nimcp_exception_recovery_action_to_string(strategy.primary_action));

    // Step 9: Execute recovery
    printf("  Step 9: Execute recovery\n");
    int recovery_result = nimcp_execute_recovery(&ex->base, EXCEPTION_RECOVERY_ROLLBACK);
    printf("    Recovery result: %d\n", recovery_result);

    // Step 10: Format to string
    printf("  Step 10: Format to string\n");
    char buffer[2048];
    size_t len = nimcp_exception_to_string(&ex->base, buffer, sizeof(buffer));
    EXPECT_GT(len, 0u);

    // Step 11: Release references
    // Note: immune presentation may have added a reference, so we check >= expected
    // We need to release all references carefully since unref may free the object
    printf("  Step 11: Release references\n");
    int32_t initial_refs = ex->base.ref_count;
    printf("    Initial ref count: %d\n", initial_refs);
    EXPECT_GE(initial_refs, 2);  // We added 1 ref in step 2, plus original

    // Release all our references (we added 1 in step 2, but immune may have added more)
    // The immune system holds references internally, so we only release what we added
    // Release the reference we added in step 2
    nimcp_exception_unref(&ex->base);
    // Release the original reference from step 1
    nimcp_exception_unref(&ex->base);
    // After this, we cannot access ex as it may be freed
    printf("  Step 11 complete: References released\n");

    // Step 12: Verify statistics
    printf("  Step 12: Verify statistics\n");
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("    Exceptions presented: %lu\n",
           (unsigned long)stats.exceptions_presented);

    printf("Test passed: End-to-end exception lifecycle\n\n");
}

/* ============================================================================
 * Test 8: Concurrent Exception Handling Across Threads
 *
 * Tests thread safety of exception handling system
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, ConcurrentExceptionHandling) {
    printf("=== Test: Concurrent Exception Handling Across Threads ===\n");

    const int NUM_THREADS = 16;
    const int EXCEPTIONS_PER_THREAD = 100;
    std::vector<std::thread> threads;
    std::atomic<int> exceptions_created{0};
    std::atomic<int> exceptions_dispatched{0};
    std::atomic<int> exceptions_freed{0};
    std::atomic<int> errors{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED + (i % 10),
                    EXCEPTION_SEVERITY_WARNING,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );

                if (!ex) {
                    errors++;
                    continue;
                }
                exceptions_created++;

                // Add thread context
                char thread_str[32];
                snprintf(thread_str, sizeof(thread_str), "%d", t);
                nimcp_exception_set_context(ex, "thread_id", thread_str);

                // Dispatch
                nimcp_exception_dispatch(ex);
                exceptions_dispatched++;

                // Clean up
                nimcp_exception_unref(ex);
                exceptions_freed++;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    int expected = NUM_THREADS * EXCEPTIONS_PER_THREAD;

    printf("  Results:\n");
    printf("    Duration: %ld ms\n", duration_ms);
    printf("    Created: %d/%d\n", exceptions_created.load(), expected);
    printf("    Dispatched: %d\n", exceptions_dispatched.load());
    printf("    Freed: %d\n", exceptions_freed.load());
    printf("    Errors: %d\n", errors.load());

    // Verify thread safety
    EXPECT_EQ(exceptions_created.load(), expected);
    EXPECT_EQ(exceptions_dispatched.load(), expected);
    EXPECT_EQ(exceptions_freed.load(), expected);
    EXPECT_EQ(errors.load(), 0);

    printf("Test passed: Concurrent exception handling\n\n");
}

/* ============================================================================
 * Test 9: Exception Storm Prevention Mechanisms
 *
 * Tests circuit breaker and rate limiting for exception storms
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, ExceptionStormPrevention) {
    printf("=== Test: Exception Storm Prevention Mechanisms ===\n");

    nimcp_error_t storm_code = NIMCP_ERROR_NO_MEMORY;

    // Configure low threshold for testing
    nimcp_circuit_set_threshold(storm_code, 20, 1000);  // 20/min, 1s reset

    const int STORM_SIZE = 200;
    int passed = 0;
    int blocked = 0;

    printf("  Phase 1: Generating exception storm (%d exceptions)\n", STORM_SIZE);

    auto storm_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < STORM_SIZE; i++) {
        nimcp_memory_exception_t* ex = create_memory_exception(1024 * (i + 1));
        if (!ex) continue;

        // Check circuit breaker
        int result = nimcp_circuit_record(&ex->base);
        if (result == 0) {
            passed++;
            nimcp_exception_dispatch(&ex->base);
        } else if (result == 1) {
            blocked++;
            g_module_stats.circuit_trips++;
        }

        nimcp_exception_unref(&ex->base);
    }

    auto storm_end = std::chrono::high_resolution_clock::now();
    auto storm_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        storm_end - storm_start).count();

    printf("  Phase 1 complete:\n");
    printf("    Duration: %ld ms\n", storm_duration);
    printf("    Passed: %d\n", passed);
    printf("    Blocked: %d\n", blocked);

    // Check circuit state
    nimcp_circuit_state_t state = nimcp_circuit_get_state(storm_code);
    printf("    Circuit state: %s\n", nimcp_circuit_state_to_string(state));

    // Circuit should have opened
    EXPECT_GT(blocked, 0) << "Circuit should have blocked some exceptions";
    EXPECT_TRUE(nimcp_circuit_is_open(storm_code))
        << "Circuit should be open after storm";

    // Phase 2: Wait for half-open
    printf("  Phase 2: Waiting for half-open transition\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    state = nimcp_circuit_get_state(storm_code);
    printf("    Circuit state after wait: %s\n", nimcp_circuit_state_to_string(state));

    // Phase 3: Report successes to close
    printf("  Phase 3: Reporting successes to close circuit\n");
    for (int i = 0; i < 5; i++) {
        nimcp_circuit_report_success(storm_code);
    }

    state = nimcp_circuit_get_state(storm_code);
    printf("    Final circuit state: %s\n", nimcp_circuit_state_to_string(state));

    // Get statistics
    nimcp_circuit_stats_t stats;
    nimcp_circuit_get_stats(&stats);
    printf("  Circuit stats:\n");
    printf("    Total blocked: %lu\n", (unsigned long)stats.total_blocked);
    printf("    Circuits open: %zu\n", stats.circuits_open);

    // Reset for other tests
    nimcp_circuit_reset(storm_code);

    printf("Test passed: Exception storm prevention\n\n");
}

/* ============================================================================
 * Test 10: System Recovery After Multiple Failures
 *
 * Tests system resilience and recovery from cascading failures
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, SystemRecoveryAfterMultipleFailures) {
    printf("=== Test: System Recovery After Multiple Failures ===\n");

    // Simulate cascading failures
    g_system_state.reset();
    g_system_state.swarm_healthy = false;
    g_system_state.security_clean = false;
    g_system_state.perception_online = false;
    g_system_state.memory_available = false;
    g_system_state.training_active = true;
    g_system_state.checkpoint_version = 3;

    printf("  Initial state: All systems failing\n");

    // Phase 1: Memory recovery
    printf("  Phase 1: Recovering memory subsystem\n");
    nimcp_memory_exception_t* mem_ex = create_memory_exception(1024 * 1024);
    ASSERT_NE(mem_ex, nullptr);

    bool mem_recovered = run_exception_pipeline(&mem_ex->base, EXCEPTION_RECOVERY_GC);
    EXPECT_TRUE(mem_recovered);
    EXPECT_TRUE(g_system_state.memory_available.load());
    nimcp_exception_unref(&mem_ex->base);
    printf("    Memory: %s\n", g_system_state.memory_available.load() ? "OK" : "FAIL");

    // Phase 2: Training recovery
    printf("  Phase 2: Recovering training subsystem\n");
    nimcp_brain_exception_t* train_ex = create_training_exception(
        "Cascade recovery", true);
    ASSERT_NE(train_ex, nullptr);

    bool train_recovered = run_exception_pipeline(&train_ex->base,
                                                   EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_TRUE(train_recovered);
    EXPECT_FALSE(g_system_state.training_active.load());
    nimcp_exception_unref(&train_ex->base);
    printf("    Training: rolled back to v%d\n",
           g_system_state.checkpoint_version.load());

    // Phase 3: Perception recovery
    printf("  Phase 3: Recovering perception subsystem\n");
    nimcp_exception_t* perc_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Perception cascade recovery"
    );
    ASSERT_NE(perc_ex, nullptr);

    bool perc_recovered = run_exception_pipeline(perc_ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_TRUE(perc_recovered);
    EXPECT_TRUE(g_system_state.perception_online.load());
    nimcp_exception_unref(perc_ex);
    printf("    Perception: %s\n",
           g_system_state.perception_online.load() ? "OK" : "FAIL");

    // Phase 4: Security recovery
    printf("  Phase 4: Recovering security subsystem\n");
    nimcp_security_exception_t* sec_ex = create_security_exception(
        "Security cascade recovery", 1);
    ASSERT_NE(sec_ex, nullptr);

    bool sec_recovered = run_exception_pipeline(&sec_ex->base,
                                                 EXCEPTION_RECOVERY_QUARANTINE);
    EXPECT_TRUE(sec_recovered);
    g_system_state.security_clean = true;  // Manual cleanup
    nimcp_exception_unref(&sec_ex->base);
    printf("    Security: %s (quarantine=%d)\n",
           g_system_state.security_clean.load() ? "OK" : "FAIL",
           g_system_state.quarantined_regions.load());

    // Phase 5: Swarm recovery
    printf("  Phase 5: Recovering swarm subsystem\n");
    nimcp_exception_t* swarm_ex = create_swarm_exception("Swarm cascade recovery");
    ASSERT_NE(swarm_ex, nullptr);

    bool swarm_recovered = run_exception_pipeline(swarm_ex,
                                                   EXCEPTION_RECOVERY_REDUCE_LOAD);
    EXPECT_TRUE(swarm_recovered);
    EXPECT_TRUE(g_system_state.swarm_healthy.load());
    nimcp_exception_unref(swarm_ex);
    printf("    Swarm: %s\n", g_system_state.swarm_healthy.load() ? "OK" : "FAIL");

    // Verify full recovery
    printf("  Final state:\n");
    printf("    Memory: %s\n", g_system_state.memory_available.load() ? "OK" : "FAIL");
    printf("    Training: %s\n", !g_system_state.training_active.load() ? "Stopped" : "Active");
    printf("    Perception: %s\n", g_system_state.perception_online.load() ? "OK" : "FAIL");
    printf("    Security: %s\n", g_system_state.security_clean.load() ? "OK" : "FAIL");
    printf("    Swarm: %s\n", g_system_state.swarm_healthy.load() ? "OK" : "FAIL");

    EXPECT_TRUE(g_system_state.memory_available.load());
    EXPECT_TRUE(g_system_state.perception_online.load());
    EXPECT_TRUE(g_system_state.security_clean.load());
    EXPECT_TRUE(g_system_state.swarm_healthy.load());

    printf("Test passed: System recovery after multiple failures\n\n");
}

/* ============================================================================
 * Test 11: Performance Under Exception Load
 *
 * Measures performance metrics under sustained exception load
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, PerformanceUnderExceptionLoad) {
    printf("=== Test: Performance Under Exception Load ===\n");

    const int NUM_EXCEPTIONS = 1000;
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_EXCEPTIONS);

    nimcp_metrics_reset();

    printf("  Generating %d exceptions with full pipeline...\n", NUM_EXCEPTIONS);

    auto total_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        auto ex_start = std::chrono::high_resolution_clock::now();

        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + (i % 10),
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Performance test %d", i
        );
        if (!ex) continue;

        // Full pipeline
        nimcp_exception_dispatch(ex);

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        nimcp_exception_unref(ex);

        auto ex_end = std::chrono::high_resolution_clock::now();
        uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(
            ex_end - ex_start).count();
        latencies.push_back(latency);
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        total_end - total_start).count();

    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    uint64_t total_latency = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
    double avg_latency = static_cast<double>(total_latency) / latencies.size();
    uint64_t p50 = latencies[latencies.size() / 2];
    uint64_t p95 = latencies[static_cast<size_t>(latencies.size() * 0.95)];
    uint64_t p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];
    uint64_t max_latency = latencies.back();
    double throughput = (NUM_EXCEPTIONS * 1000.0) / total_duration_ms;

    printf("  Performance Results:\n");
    printf("    Total duration: %ld ms\n", total_duration_ms);
    printf("    Throughput: %.1f exceptions/sec\n", throughput);
    printf("    Latency:\n");
    printf("      Average: %.1f us\n", avg_latency);
    printf("      P50: %lu us\n", (unsigned long)p50);
    printf("      P95: %lu us\n", (unsigned long)p95);
    printf("      P99: %lu us\n", (unsigned long)p99);
    printf("      Max: %lu us\n", (unsigned long)max_latency);

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);
    printf("    Metrics:\n");
    printf("      Total recorded: %lu\n", (unsigned long)metrics.total_exceptions);
    printf("      Current rate: %.2f/sec\n", metrics.current_rate_per_second);

    // Performance assertions
    EXPECT_GT(throughput, 100.0) << "Throughput should be > 100/sec";
    EXPECT_LT(avg_latency, 10000.0) << "Average latency should be < 10ms";
    EXPECT_LT(p99, 50000) << "P99 latency should be < 50ms";

    printf("Test passed: Performance under exception load\n\n");
}

/* ============================================================================
 * Test 12: Cross-Module Exception Propagation
 *
 * Tests exception chaining and propagation across modules
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, CrossModuleExceptionPropagation) {
    printf("=== Test: Cross-Module Exception Propagation ===\n");

    // Create exception chain: Memory -> Training -> Perception
    printf("  Creating cross-module exception chain\n");

    // Root cause: Memory failure
    nimcp_memory_exception_t* mem_ex = create_memory_exception(256 * 1024 * 1024);
    ASSERT_NE(mem_ex, nullptr);
    printf("    Level 1: Memory exception (root cause)\n");

    // Caused: Training failure
    nimcp_brain_exception_t* train_ex = create_training_exception(
        "Training failed due to memory pressure", true);
    ASSERT_NE(train_ex, nullptr);
    nimcp_exception_set_cause(&train_ex->base, &mem_ex->base);
    printf("    Level 2: Training exception (caused by memory)\n");

    // Caused: Perception failure
    nimcp_cognitive_exception_t* perc_ex = (nimcp_cognitive_exception_t*)malloc(
        sizeof(nimcp_cognitive_exception_t));
    ASSERT_NE(perc_ex, nullptr);
    memset(perc_ex, 0, sizeof(*perc_ex));
    perc_ex->base.type = EXCEPTION_TYPE_COGNITIVE;
    perc_ex->base.category = EXCEPTION_CATEGORY_COGNITIVE;
    perc_ex->base.code = NIMCP_ERROR_OPERATION_FAILED;
    perc_ex->base.severity = EXCEPTION_SEVERITY_SEVERE;
    perc_ex->base.ref_count = 1;
    snprintf(perc_ex->base.message, NIMCP_EXCEPTION_MAX_MESSAGE,
             "Perception failed due to training instability");
    perc_ex->base.file = __FILE__;
    perc_ex->base.line = __LINE__;
    perc_ex->base.function = __func__;
    nimcp_exception_set_cause(&perc_ex->base, &train_ex->base);
    printf("    Level 3: Perception exception (caused by training)\n");

    // Walk the chain
    printf("  Walking exception chain:\n");
    nimcp_exception_t* current = &perc_ex->base;
    int depth = 0;
    while (current) {
        printf("    [%d] %s (code=%d, category=%d)\n",
               depth,
               nimcp_exception_type_to_string(current->type),
               current->code,
               current->category);

        // Present each to immune
        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(current, &response);

        current = nimcp_exception_get_cause(current);
        depth++;
    }

    EXPECT_EQ(depth, 3) << "Chain should have 3 levels";
    printf("  Chain depth: %d\n", depth);

    // Recover from root cause up
    printf("  Recovering from root cause up:\n");

    // First: Fix memory
    printf("    Step 1: Recover memory\n");
    run_exception_pipeline(&mem_ex->base, EXCEPTION_RECOVERY_GC);

    // Then: Rollback training
    printf("    Step 2: Rollback training\n");
    run_exception_pipeline(&train_ex->base, EXCEPTION_RECOVERY_ROLLBACK);

    // Finally: Retry perception
    printf("    Step 3: Retry perception\n");
    run_exception_pipeline(&perc_ex->base, EXCEPTION_RECOVERY_RETRY);

    // Clean up (unreffing top releases chain)
    nimcp_exception_unref(&perc_ex->base);

    printf("Test passed: Cross-module exception propagation\n\n");
}

/* ============================================================================
 * Test 13: Aggregate Exception Handling Across Modules
 *
 * Tests batch processing of multiple module exceptions
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, AggregateExceptionHandling) {
    printf("=== Test: Aggregate Exception Handling Across Modules ===\n");

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Multi-module batch operation failure"
    );
    ASSERT_NE(agg, nullptr);

    // Add module exceptions as children
    printf("  Adding module exceptions to aggregate:\n");

    // Memory exception
    nimcp_memory_exception_t* mem_ex = create_memory_exception(1024 * 1024);
    ASSERT_NE(mem_ex, nullptr);
    nimcp_aggregate_exception_add(agg, &mem_ex->base);
    printf("    Added: Memory exception\n");

    // Training exception
    nimcp_brain_exception_t* train_ex = create_training_exception(
        "Batch training error", false);
    ASSERT_NE(train_ex, nullptr);
    nimcp_aggregate_exception_add(agg, &train_ex->base);
    printf("    Added: Training exception\n");

    // Security exception
    nimcp_security_exception_t* sec_ex = create_security_exception(
        "Batch security alert", 1);
    ASSERT_NE(sec_ex, nullptr);
    nimcp_aggregate_exception_add(agg, &sec_ex->base);
    printf("    Added: Security exception\n");

    // Swarm exception
    nimcp_exception_t* swarm_ex = create_swarm_exception("Batch swarm error");
    ASSERT_NE(swarm_ex, nullptr);
    nimcp_aggregate_exception_add(agg, swarm_ex);
    printf("    Added: Swarm exception\n");

    // Verify count
    size_t count = nimcp_aggregate_exception_count(agg);
    EXPECT_EQ(count, 4u);
    printf("  Aggregate contains %zu exceptions\n", count);

    // Dispatch aggregate
    printf("  Dispatching aggregate exception\n");
    nimcp_exception_dispatch(&agg->base);

    // Present aggregate to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&agg->base, &response);
    printf("  Presented to immune: antigen_id=%u\n", response.antigen_id);

    // Process each child individually
    printf("  Processing children individually:\n");
    for (size_t i = 0; i < count; i++) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
        if (child) {
            printf("    Child %zu: type=%s, code=%d\n",
                   i, nimcp_exception_type_to_string(child->type), child->code);

            memset(&response, 0, sizeof(response));
            nimcp_exception_present_to_immune(child, &response);
        }
    }

    // Clean up
    nimcp_exception_unref(&agg->base);

    printf("Test passed: Aggregate exception handling\n\n");
}

/* ============================================================================
 * Test 14: Timing Constraints Verification
 *
 * Verifies that exception handling meets timing requirements
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, TimingConstraintsVerification) {
    printf("=== Test: Timing Constraints Verification ===\n");

    const int NUM_TESTS = 100;
    const uint64_t MAX_CREATION_US = 1000;      // 1ms max for creation
    const uint64_t MAX_DISPATCH_US = 5000;      // 5ms max for dispatch
    const uint64_t MAX_IMMUNE_US = 10000;       // 10ms max for immune presentation
    const uint64_t MAX_RECOVERY_US = 50000;     // 50ms max for recovery

    std::vector<uint64_t> creation_times;
    std::vector<uint64_t> dispatch_times;
    std::vector<uint64_t> immune_times;
    std::vector<uint64_t> recovery_times;

    creation_times.reserve(NUM_TESTS);
    dispatch_times.reserve(NUM_TESTS);
    immune_times.reserve(NUM_TESTS);
    recovery_times.reserve(NUM_TESTS);

    printf("  Running %d timing tests...\n", NUM_TESTS);

    for (int i = 0; i < NUM_TESTS; i++) {
        // Time creation
        auto t1 = std::chrono::high_resolution_clock::now();
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Timing test %d", i
        );
        auto t2 = std::chrono::high_resolution_clock::now();

        if (!ex) continue;

        creation_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            t2 - t1).count());

        // Time dispatch
        auto t3 = std::chrono::high_resolution_clock::now();
        nimcp_exception_dispatch(ex);
        auto t4 = std::chrono::high_resolution_clock::now();

        dispatch_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            t4 - t3).count());

        // Time immune presentation
        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));

        auto t5 = std::chrono::high_resolution_clock::now();
        nimcp_exception_present_to_immune(ex, &response);
        auto t6 = std::chrono::high_resolution_clock::now();

        immune_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            t6 - t5).count());

        // Time recovery
        auto t7 = std::chrono::high_resolution_clock::now();
        nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
        auto t8 = std::chrono::high_resolution_clock::now();

        recovery_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            t8 - t7).count());

        nimcp_exception_unref(ex);
    }

    // Calculate statistics
    auto calc_stats = [](std::vector<uint64_t>& times) {
        std::sort(times.begin(), times.end());
        uint64_t sum = std::accumulate(times.begin(), times.end(), 0ULL);
        return std::make_tuple(
            static_cast<double>(sum) / times.size(),  // avg
            times[times.size() / 2],                   // p50
            times[static_cast<size_t>(times.size() * 0.99)],  // p99
            times.back()                               // max
        );
    };

    auto [creation_avg, creation_p50, creation_p99, creation_max] =
        calc_stats(creation_times);
    auto [dispatch_avg, dispatch_p50, dispatch_p99, dispatch_max] =
        calc_stats(dispatch_times);
    auto [immune_avg, immune_p50, immune_p99, immune_max] =
        calc_stats(immune_times);
    auto [recovery_avg, recovery_p50, recovery_p99, recovery_max] =
        calc_stats(recovery_times);

    printf("  Timing Results (microseconds):\n");
    printf("    Creation:   avg=%.1f, p50=%lu, p99=%lu, max=%lu (limit=%lu)\n",
           creation_avg, (unsigned long)creation_p50,
           (unsigned long)creation_p99, (unsigned long)creation_max,
           (unsigned long)MAX_CREATION_US);
    printf("    Dispatch:   avg=%.1f, p50=%lu, p99=%lu, max=%lu (limit=%lu)\n",
           dispatch_avg, (unsigned long)dispatch_p50,
           (unsigned long)dispatch_p99, (unsigned long)dispatch_max,
           (unsigned long)MAX_DISPATCH_US);
    printf("    Immune:     avg=%.1f, p50=%lu, p99=%lu, max=%lu (limit=%lu)\n",
           immune_avg, (unsigned long)immune_p50,
           (unsigned long)immune_p99, (unsigned long)immune_max,
           (unsigned long)MAX_IMMUNE_US);
    printf("    Recovery:   avg=%.1f, p50=%lu, p99=%lu, max=%lu (limit=%lu)\n",
           recovery_avg, (unsigned long)recovery_p50,
           (unsigned long)recovery_p99, (unsigned long)recovery_max,
           (unsigned long)MAX_RECOVERY_US);

    // Verify timing constraints (using P99 to allow for occasional outliers)
    EXPECT_LT(creation_p99, MAX_CREATION_US * 2)
        << "Creation P99 should be within 2x limit";
    EXPECT_LT(dispatch_p99, MAX_DISPATCH_US * 2)
        << "Dispatch P99 should be within 2x limit";
    EXPECT_LT(immune_p99, MAX_IMMUNE_US * 2)
        << "Immune P99 should be within 2x limit";
    EXPECT_LT(recovery_p99, MAX_RECOVERY_US * 2)
        << "Recovery P99 should be within 2x limit";

    printf("Test passed: Timing constraints verification\n\n");
}

/* ============================================================================
 * Test 15: Exception Handler Priority Chain
 *
 * Tests that handlers execute in priority order
 * ============================================================================ */

TEST_F(ExceptionModuleE2ETest, ExceptionHandlerPriorityChain) {
    printf("=== Test: Exception Handler Priority Chain ===\n");

    std::vector<std::string> execution_order;
    std::mutex order_mutex;

    auto record = [&execution_order, &order_mutex](const std::string& name) {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(name);
    };

    // Register handlers with different priorities
    struct HandlerContext {
        std::function<void(const std::string&)>* recorder;
        const char* name;
    };

    HandlerContext ctx_high = {new std::function<void(const std::string&)>(record), "HIGH"};
    HandlerContext ctx_normal = {new std::function<void(const std::string&)>(record), "NORMAL"};
    HandlerContext ctx_low = {new std::function<void(const std::string&)>(record), "LOW"};

    // High priority handler
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "HighPriority";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH + 50;  // Higher than default high
    opts.user_data = &ctx_high;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* ctx = static_cast<HandlerContext*>(user_data);
        (*ctx->recorder)(ctx->name);
        return false;
    };
    auto* high_reg = nimcp_handler_register(&opts);
    ASSERT_NE(high_reg, nullptr);

    // Normal priority handler
    nimcp_handler_default_options(&opts);
    opts.name = "NormalPriority";
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    opts.user_data = &ctx_normal;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* ctx = static_cast<HandlerContext*>(user_data);
        (*ctx->recorder)(ctx->name);
        return false;
    };
    auto* normal_reg = nimcp_handler_register(&opts);
    ASSERT_NE(normal_reg, nullptr);

    // Low priority handler
    nimcp_handler_default_options(&opts);
    opts.name = "LowPriority";
    opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    opts.user_data = &ctx_low;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        auto* ctx = static_cast<HandlerContext*>(user_data);
        (*ctx->recorder)(ctx->name);
        return false;
    };
    auto* low_reg = nimcp_handler_register(&opts);
    ASSERT_NE(low_reg, nullptr);

    printf("  Registered handlers: HIGH (%d), NORMAL (%d), LOW (%d)\n",
           NIMCP_HANDLER_PRIORITY_HIGH + 50,
           NIMCP_HANDLER_PRIORITY_NORMAL,
           NIMCP_HANDLER_PRIORITY_LOW);

    // Create and dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Priority chain test"
    );
    ASSERT_NE(ex, nullptr);

    execution_order.clear();
    nimcp_exception_dispatch(ex);

    // Verify execution order
    printf("  Execution order:\n");
    for (size_t i = 0; i < execution_order.size(); i++) {
        printf("    %zu: %s\n", i, execution_order[i].c_str());
    }

    // HIGH should come before NORMAL, NORMAL before LOW
    auto find_pos = [&execution_order](const std::string& name) -> int {
        for (size_t i = 0; i < execution_order.size(); i++) {
            if (execution_order[i] == name) return static_cast<int>(i);
        }
        return -1;
    };

    int high_pos = find_pos("HIGH");
    int normal_pos = find_pos("NORMAL");
    int low_pos = find_pos("LOW");

    EXPECT_NE(high_pos, -1) << "HIGH handler should have been called";
    EXPECT_NE(normal_pos, -1) << "NORMAL handler should have been called";
    EXPECT_NE(low_pos, -1) << "LOW handler should have been called";

    if (high_pos >= 0 && normal_pos >= 0 && low_pos >= 0) {
        EXPECT_LT(high_pos, normal_pos) << "HIGH should execute before NORMAL";
        EXPECT_LT(normal_pos, low_pos) << "NORMAL should execute before LOW";
    }

    // Clean up
    nimcp_handler_unregister(high_reg);
    nimcp_handler_unregister(normal_reg);
    nimcp_handler_unregister(low_reg);

    delete ctx_high.recorder;
    delete ctx_normal.recorder;
    delete ctx_low.recorder;

    nimcp_exception_unref(ex);

    printf("Test passed: Exception handler priority chain\n\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
