/**
 * @file test_recovery_pool_integration.cpp
 * @brief Integration tests for recovery pool (12+ tests)
 *
 * TEST COVERAGE:
 * - OOM recovery scenarios
 * - Integration with checkpoint system
 * - Integration with diagnostics
 * - Multi-threaded stress tests
 * - Real-world recovery patterns
 * - Pool exhaustion handling
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_recovery_pool.h"
#include "utils/fault_tolerance/nimcp_checkpoint.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#include <vector>
#include <thread>
#include <atomic>
#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryPoolIntegrationTest : public ::testing::Test {
protected:
    recovery_pool_t* pool;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_thread_init();
        pool = nullptr;
    }

    void TearDown() override {
        if (pool) {
            recovery_pool_destroy(pool);
            pool = nullptr;
        }
        recovery_pool_set_global(nullptr);
        recovery_pool_clear_error();
    }
};

//=============================================================================
// Test Group 1: OOM Recovery Scenarios (4 tests)
//=============================================================================

TEST_F(RecoveryPoolIntegrationTest, OOMRecoveryWorkflow) {
    // Create pool
    pool = recovery_pool_create(1024 * 1024);  // 1MB
    ASSERT_NE(pool, nullptr);

    // Simulate OOM detection
    recovery_pool_enter_emergency_mode(pool);
    EXPECT_TRUE(recovery_pool_is_emergency_mode(pool));

    // Allocate recovery data structures
    struct recovery_data {
        char checkpoint_path[256];
        char diagnostics_log[512];
        uint8_t temp_buffer[1024];
    };

    recovery_data* data = (recovery_data*)recovery_pool_alloc(pool, sizeof(recovery_data));
    ASSERT_NE(data, nullptr);

    // Use the data
    strncpy(data->checkpoint_path, "/tmp/emergency_checkpoint.ckpt", sizeof(data->checkpoint_path) - 1);
    strncpy(data->diagnostics_log, "OOM detected, initiating recovery...", sizeof(data->diagnostics_log) - 1);

    // Verify data intact
    EXPECT_STREQ(data->checkpoint_path, "/tmp/emergency_checkpoint.ckpt");

    // Recovery complete
    recovery_pool_exit_emergency_mode(pool);
    recovery_pool_reset(pool);

    // Verify pool ready for next emergency
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.current_used_bytes, 0);
    EXPECT_EQ(stats.emergency_activations, 1);
}

TEST_F(RecoveryPoolIntegrationTest, SequentialOOMRecoveries) {
    pool = recovery_pool_create(64 * 1024);  // 64KB
    ASSERT_NE(pool, nullptr);

    recovery_pool_set_global(pool);

    // Simulate 5 sequential OOM events
    for (int oom_event = 1; oom_event <= 5; oom_event++) {
        // Enter emergency mode
        ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

        // Allocate recovery structures
        void* checkpoint_buffer = recovery_pool_alloc(pool, 4096);
        void* diagnostics_buffer = recovery_pool_alloc(pool, 2048);
        void* temp_buffer = recovery_pool_alloc(pool, 1024);

        ASSERT_NE(checkpoint_buffer, nullptr);
        ASSERT_NE(diagnostics_buffer, nullptr);
        ASSERT_NE(temp_buffer, nullptr);

        // Simulate recovery operations
        memset(checkpoint_buffer, 0, 4096);
        memset(diagnostics_buffer, 0, 2048);
        memset(temp_buffer, 0, 1024);

        // Exit emergency mode and reset
        ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
        ASSERT_TRUE(recovery_pool_reset(pool));

        // Verify activation count
        recovery_pool_stats_t stats;
        ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
        EXPECT_EQ(stats.emergency_activations, oom_event);
    }

    recovery_pool_set_global(nullptr);
}

TEST_F(RecoveryPoolIntegrationTest, OOMRecoveryWithExhaustion) {
    pool = recovery_pool_create(4096);  // Small pool
    ASSERT_NE(pool, nullptr);

    // Enter emergency mode
    ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

    // Allocate until exhausted
    std::vector<void*> allocations;
    void* ptr;
    while ((ptr = recovery_pool_alloc(pool, 512)) != nullptr) {
        allocations.push_back(ptr);
        if (allocations.size() > 100) {
            break;  // Safety limit
        }
    }

    // Pool should be exhausted
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_TRUE(stats.pool_exhausted);
    EXPECT_GT(stats.failed_allocations, 0);

    // Should not be able to allocate more
    void* extra = recovery_pool_alloc(pool, 512);
    EXPECT_EQ(extra, nullptr);

    // Reset and try again
    ASSERT_TRUE(recovery_pool_reset(pool));

    // Should be able to allocate again
    void* new_ptr = recovery_pool_alloc(pool, 512);
    EXPECT_NE(new_ptr, nullptr);

    // Exit emergency mode
    ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
}

TEST_F(RecoveryPoolIntegrationTest, OOMRecoveryReallocation) {
    pool = recovery_pool_create(16 * 1024);  // 16KB
    ASSERT_NE(pool, nullptr);

    // Multiple allocations of varying sizes
    const int num_cycles = 3;
    for (int cycle = 0; cycle < num_cycles; cycle++) {
        ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

        // Allocate different size blocks
        void* small = recovery_pool_alloc(pool, 256);
        void* medium = recovery_pool_alloc(pool, 1024);
        void* large = recovery_pool_alloc(pool, 4096);

        ASSERT_NE(small, nullptr);
        ASSERT_NE(medium, nullptr);
        ASSERT_NE(large, nullptr);

        // Use the memory
        memset(small, 0xAA, 256);
        memset(medium, 0xBB, 1024);
        memset(large, 0xCC, 4096);

        ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
        ASSERT_TRUE(recovery_pool_reset(pool));
    }

    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.emergency_activations, num_cycles);
    EXPECT_EQ(stats.reset_count, num_cycles);
}

//=============================================================================
// Test Group 2: Integration with Other Systems (3 tests)
//=============================================================================

TEST_F(RecoveryPoolIntegrationTest, GlobalPoolCheckpointIntegration) {
    // Create and set global pool
    pool = recovery_pool_create(1024 * 1024);
    ASSERT_NE(pool, nullptr);
    recovery_pool_set_global(pool);

    // Verify global pool accessible
    recovery_pool_t* global = recovery_pool_get_global();
    EXPECT_EQ(global, pool);

    // Simulate checkpoint using global pool
    ASSERT_TRUE(recovery_pool_enter_emergency_mode(global));

    // Allocate checkpoint data structure
    struct checkpoint_data {
        char path[256];
        uint8_t header[64];
        uint8_t metadata[512];
    };

    checkpoint_data* ckpt = (checkpoint_data*)recovery_pool_alloc(global, sizeof(checkpoint_data));
    ASSERT_NE(ckpt, nullptr);

    // Use checkpoint data
    strncpy(ckpt->path, "/tmp/emergency.ckpt", sizeof(ckpt->path) - 1);
    memset(ckpt->header, 0, sizeof(ckpt->header));
    memset(ckpt->metadata, 0, sizeof(ckpt->metadata));

    // Cleanup
    ASSERT_TRUE(recovery_pool_exit_emergency_mode(global));
    ASSERT_TRUE(recovery_pool_reset(global));

    recovery_pool_set_global(nullptr);
}

TEST_F(RecoveryPoolIntegrationTest, DiagnosticsIntegration) {
    pool = recovery_pool_create(256 * 1024);  // 256KB
    ASSERT_NE(pool, nullptr);

    recovery_pool_set_global(pool);

    // Simulate diagnostics using pool
    ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

    // Allocate diagnostic structures
    struct diagnostic_context {
        char error_message[512];
        char stack_trace[1024];
        uint8_t system_state[2048];
    };

    diagnostic_context* ctx = (diagnostic_context*)recovery_pool_calloc(pool, 1, sizeof(diagnostic_context));
    ASSERT_NE(ctx, nullptr);

    // Verify zero-initialized
    bool all_zeros = true;
    uint8_t* bytes = (uint8_t*)ctx;
    for (size_t i = 0; i < sizeof(diagnostic_context); i++) {
        if (bytes[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_TRUE(all_zeros);

    // Use diagnostic context
    strncpy(ctx->error_message, "Segmentation fault detected", sizeof(ctx->error_message) - 1);
    strncpy(ctx->stack_trace, "main() -> process() -> crash()", sizeof(ctx->stack_trace) - 1);

    // Cleanup
    ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
    ASSERT_TRUE(recovery_pool_reset(pool));

    recovery_pool_set_global(nullptr);
}

TEST_F(RecoveryPoolIntegrationTest, MultiSubsystemCoordination) {
    pool = recovery_pool_create(512 * 1024);  // 512KB
    ASSERT_NE(pool, nullptr);

    recovery_pool_set_global(pool);

    // Simulate multiple subsystems using pool simultaneously
    ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

    // Checkpoint subsystem allocation
    void* checkpoint_buf = recovery_pool_alloc(pool, 64 * 1024);
    ASSERT_NE(checkpoint_buf, nullptr);

    // Diagnostics subsystem allocation
    void* diagnostics_buf = recovery_pool_alloc(pool, 32 * 1024);
    ASSERT_NE(diagnostics_buf, nullptr);

    // Recovery subsystem allocation
    void* recovery_buf = recovery_pool_alloc(pool, 16 * 1024);
    ASSERT_NE(recovery_buf, nullptr);

    // Verify all allocations valid
    EXPECT_NE(checkpoint_buf, diagnostics_buf);
    EXPECT_NE(checkpoint_buf, recovery_buf);
    EXPECT_NE(diagnostics_buf, recovery_buf);

    // Get statistics
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.allocation_count, 3);

    // Cleanup
    ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
    ASSERT_TRUE(recovery_pool_reset(pool));

    recovery_pool_set_global(nullptr);
}

//=============================================================================
// Test Group 3: Multi-Threaded Stress Tests (3 tests)
//=============================================================================

TEST_F(RecoveryPoolIntegrationTest, ConcurrentEmergencyMode) {
    pool = recovery_pool_create(256 * 1024);
    ASSERT_NE(pool, nullptr);

    const int num_threads = 8;
    std::atomic<int> enter_success{0};
    std::atomic<int> exit_success{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, &enter_success, &exit_success]() {
            // Enter emergency mode
            if (recovery_pool_enter_emergency_mode(pool)) {
                enter_success++;
            }

            // Allocate something
            void* ptr = recovery_pool_alloc(pool, 1024);
            if (ptr) {
                memset(ptr, 0, 1024);
            }

            // Exit emergency mode
            if (recovery_pool_exit_emergency_mode(pool)) {
                exit_success++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All threads should succeed
    EXPECT_EQ(enter_success, num_threads);
    EXPECT_EQ(exit_success, num_threads);

    // Pool should still be valid
    EXPECT_TRUE(recovery_pool_validate(pool));
}

TEST_F(RecoveryPoolIntegrationTest, ConcurrentAllocationStress) {
    pool = recovery_pool_create(64 * 1024);  // 64KB - smaller to cause exhaustion
    ASSERT_NE(pool, nullptr);

    ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

    const int num_threads = 16;
    const int allocs_per_thread = 100;  // Increased to force failures
    std::atomic<int> total_allocations{0};
    std::atomic<int> failed_allocations{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, &total_allocations, &failed_allocations, allocs_per_thread]() {
            for (int j = 0; j < allocs_per_thread; j++) {
                void* ptr = recovery_pool_alloc(pool, 1024);  // Larger allocations
                if (ptr) {
                    total_allocations++;
                    // Use the memory
                    memset(ptr, j & 0xFF, 1024);
                } else {
                    failed_allocations++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Some allocations should succeed
    EXPECT_GT(total_allocations, 0);

    // Eventually pool should exhaust (16 threads * 100 allocs * 1KB = 1.6MB > 64KB pool)
    EXPECT_GT(failed_allocations, 0);

    // Verify pool integrity
    EXPECT_TRUE(recovery_pool_validate(pool));

    ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
}

TEST_F(RecoveryPoolIntegrationTest, ConcurrentResetStress) {
    pool = recovery_pool_create(128 * 1024);
    ASSERT_NE(pool, nullptr);

    const int num_cycles = 100;
    std::atomic<int> successful_cycles{0};

    std::thread allocator([this, &successful_cycles, num_cycles]() {
        for (int i = 0; i < num_cycles; i++) {
            void* ptr = recovery_pool_alloc(pool, 512);
            if (ptr) {
                successful_cycles++;
            }
        }
    });

    std::thread resetter([this, num_cycles]() {
        for (int i = 0; i < num_cycles / 10; i++) {
            recovery_pool_reset(pool);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    allocator.join();
    resetter.join();

    // Pool should still be valid after concurrent access
    EXPECT_TRUE(recovery_pool_validate(pool));
}

//=============================================================================
// Test Group 4: Real-World Recovery Patterns (2 tests)
//=============================================================================

TEST_F(RecoveryPoolIntegrationTest, CompleteRecoverySequence) {
    pool = recovery_pool_create(1024 * 1024);
    ASSERT_NE(pool, nullptr);

    recovery_pool_set_global(pool);

    // Step 1: Detect OOM
    ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

    // Step 2: Allocate diagnostic context
    struct diagnostic_data {
        char error_type[128];
        char timestamp[64];
        uint8_t system_snapshot[4096];
    };

    diagnostic_data* diag = (diagnostic_data*)recovery_pool_calloc(pool, 1, sizeof(diagnostic_data));
    ASSERT_NE(diag, nullptr);

    strncpy(diag->error_type, "OUT_OF_MEMORY", sizeof(diag->error_type) - 1);
    strncpy(diag->timestamp, "2025-11-20T12:00:00", sizeof(diag->timestamp) - 1);

    // Step 3: Allocate checkpoint buffer
    uint8_t* checkpoint = (uint8_t*)recovery_pool_alloc(pool, 256 * 1024);
    ASSERT_NE(checkpoint, nullptr);

    // Step 4: Allocate recovery metadata
    struct recovery_metadata {
        char recovery_strategy[256];
        uint32_t retry_count;
        bool fallback_enabled;
    };

    recovery_metadata* meta = (recovery_metadata*)recovery_pool_calloc(pool, 1, sizeof(recovery_metadata));
    ASSERT_NE(meta, nullptr);

    strncpy(meta->recovery_strategy, "RELOAD_FROM_CHECKPOINT", sizeof(meta->recovery_strategy) - 1);
    meta->retry_count = 3;
    meta->fallback_enabled = true;

    // Step 5: Verify allocations
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.allocation_count, 3);

    // Step 6: Complete recovery
    ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
    ASSERT_TRUE(recovery_pool_reset(pool));

    // Step 7: Verify pool ready for next emergency
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.current_used_bytes, 0);
    EXPECT_EQ(stats.emergency_activations, 1);
    EXPECT_FALSE(stats.is_emergency_mode);

    recovery_pool_set_global(nullptr);
}

TEST_F(RecoveryPoolIntegrationTest, PartialRecoveryScenario) {
    pool = recovery_pool_create(32 * 1024);  // Small pool
    ASSERT_NE(pool, nullptr);

    ASSERT_TRUE(recovery_pool_enter_emergency_mode(pool));

    // Allocate critical structures first
    void* critical = recovery_pool_alloc(pool, 8 * 1024);
    ASSERT_NE(critical, nullptr);

    // Allocate important structures
    void* important = recovery_pool_alloc(pool, 8 * 1024);
    ASSERT_NE(important, nullptr);

    // Allocate optional structures
    void* optional1 = recovery_pool_alloc(pool, 8 * 1024);
    void* optional2 = recovery_pool_alloc(pool, 8 * 1024);

    // Pool may be exhausted
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));

    // At minimum, critical and important should succeed
    EXPECT_NE(critical, nullptr);
    EXPECT_NE(important, nullptr);

    // Optional may fail (pool exhaustion)
    if (optional1 == nullptr || optional2 == nullptr) {
        EXPECT_TRUE(stats.pool_exhausted);
    }

    // Even with partial failure, critical operations should succeed
    memset(critical, 0xAA, 8 * 1024);
    memset(important, 0xBB, 8 * 1024);

    ASSERT_TRUE(recovery_pool_exit_emergency_mode(pool));
    ASSERT_TRUE(recovery_pool_reset(pool));
}

//=============================================================================
// Summary
//=============================================================================

// Total tests: 12+
// Coverage:
// - OOM recovery scenarios: 4 tests
// - Integration with other systems: 3 tests
// - Multi-threaded stress tests: 3 tests
// - Real-world recovery patterns: 2 tests
