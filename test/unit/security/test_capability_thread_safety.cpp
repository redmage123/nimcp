/**
 * @file test_capability_thread_safety.cpp
 * @brief Thread safety tests for capability-based access control system
 *
 * WHAT: Tests concurrent access to the capability system mutex protection
 * WHY:  Verify thread-safe operations under high contention for security-critical code
 *
 * DESIGN PATTERNS:
 * - Builder Pattern: Construct complex concurrent test scenarios
 * - Template Method: Reusable concurrent test harness
 * - Strategy Pattern: Different contention strategies
 *
 * TEST PHILOSOPHY:
 * - TDD: Tests written before implementation verification
 * - Concurrent stress testing: Many threads, many operations
 * - Race detection: Use thread sanitizer to catch bugs
 * - Correctness: Verify final state matches expected behavior
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 *
 * @author NIMCP Development Team
 * @date 2025-01-15
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <random>
#include <mutex>

extern "C" {
#include "security/nimcp_capability.h"
#include "utils/validation/nimcp_common.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Thread safety test fixture for capability system
 *
 * WHAT: Provides concurrent testing infrastructure for capability operations
 * WHY:  Setup/teardown for multi-threaded capability tests
 */
class CapabilityThreadSafetyTest : public ::testing::Test {
protected:
    nimcp_capability_system_t* caps;
    static constexpr uint32_t NUM_THREADS = 8;
    static constexpr uint32_t OPS_PER_THREAD = 1000;

    void SetUp() override {
        /* WHAT: Create and initialize capability system
         * WHY:  Clean state for each test
         */
        caps = nimcp_capability_system_create();
        ASSERT_NE(caps, nullptr) << "Failed to create capability system";

        nimcp_result_t result = nimcp_capability_system_init(caps);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to initialize capability system";
    }

    void TearDown() override {
        /* WHAT: Clean up capability system
         * WHY:  Prevent memory leaks and state pollution between tests
         */
        if (caps) {
            nimcp_capability_system_destroy(caps);
            caps = nullptr;
        }
    }
};

//=============================================================================
// Test 1: Concurrent Capability Grant/Revoke Operations
//=============================================================================

/**
 * @brief Test concurrent capability creation and revocation
 *
 * WHAT: Multiple threads create and revoke capabilities simultaneously
 * WHY:  Verify mutex protection for grant/revoke operations under contention
 *
 * PATTERN: Template Method (concurrent test harness)
 */
TEST_F(CapabilityThreadSafetyTest, ConcurrentGrantRevoke) {
    /* WHAT: Launch multiple threads granting and revoking capabilities
     * WHY:  Test for race conditions in capability table modifications
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint32_t> grant_success{0};
    std::atomic<uint32_t> revoke_success{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, &ready_count, &start, &grant_success, &revoke_success]() {
        /* WHAT: Signal ready and wait for start
         * WHY:  Synchronize all threads to maximize contention
         */
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* WHAT: Perform many concurrent grant/revoke operations
         * WHY:  Stress test mutex protection
         */
        std::vector<nimcp_capability_t> created_caps;
        created_caps.reserve(OPS_PER_THREAD / 2);

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            if (i % 2 == 0) {
                // Create capability
                nimcp_capability_t cap;
                nimcp_result_t result = nimcp_capability_create(
                    caps,
                    NIMCP_RES_GENERIC,
                    nullptr,
                    NIMCP_PERM_READ | NIMCP_PERM_WRITE,
                    &cap
                );
                if (result == NIMCP_SUCCESS) {
                    grant_success.fetch_add(1, std::memory_order_relaxed);
                    created_caps.push_back(cap);
                }
            } else if (!created_caps.empty()) {
                // Revoke a previously created capability
                nimcp_capability_t cap_to_revoke = created_caps.back();
                created_caps.pop_back();

                nimcp_result_t result = nimcp_capability_revoke(caps, cap_to_revoke);
                if (result == NIMCP_SUCCESS) {
                    revoke_success.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        // Clean up remaining capabilities
        for (const auto& cap : created_caps) {
            nimcp_capability_revoke(caps, cap);
        }
    };

    /* WHAT: Create thread pool
     * WHY:  Test concurrent access from multiple threads
     */
    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    /* WHAT: Wait for all threads ready, then start
     * WHY:  Maximize concurrent access
     */
    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    /* WHAT: Join all threads
     * WHY:  Wait for completion before verification
     */
    for (auto& t : threads) {
        t.join();
    }

    /* WHAT: Verify operations succeeded
     * WHY:  Ensure no deadlocks or crashes occurred
     */
    EXPECT_GT(grant_success.load(), 0u) << "No capabilities were successfully granted";
    EXPECT_GT(revoke_success.load(), 0u) << "No capabilities were successfully revoked";

    /* WHAT: Verify system is still in valid state
     * WHY:  Race conditions would corrupt state
     */
    nimcp_cap_stats_t stats;
    nimcp_result_t result = nimcp_capability_get_stats(caps, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to get stats after concurrent operations";
}

//=============================================================================
// Test 2: Concurrent Capability Check Operations
//=============================================================================

/**
 * @brief Test concurrent capability permission checks
 *
 * WHAT: Multiple threads checking permissions on shared capabilities
 * WHY:  Verify read-side mutex protection doesn't corrupt state
 */
TEST_F(CapabilityThreadSafetyTest, ConcurrentCheck) {
    /* WHAT: Pre-create some capabilities to check
     * WHY:  Need valid capabilities for concurrent checking
     */
    std::vector<nimcp_capability_t> test_caps;
    const uint32_t NUM_CAPS = 50;

    for (uint32_t i = 0; i < NUM_CAPS; i++) {
        nimcp_capability_t cap;
        uint32_t perms = NIMCP_PERM_READ;
        if (i % 2 == 0) perms |= NIMCP_PERM_WRITE;
        if (i % 3 == 0) perms |= NIMCP_PERM_EXECUTE;

        nimcp_result_t result = nimcp_capability_create(
            caps,
            NIMCP_RES_GENERIC,
            nullptr,
            perms,
            &cap
        );
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to create test capability " << i;
        test_caps.push_back(cap);
    }

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> checks_performed{0};
    std::atomic<uint64_t> valid_checks{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, &test_caps, &ready_count, &start, &checks_performed, &valid_checks]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* WHAT: Perform many concurrent capability checks
         * WHY:  Stress test read-side locking
         */
        std::mt19937 rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::uniform_int_distribution<uint32_t> cap_dist(0, test_caps.size() - 1);

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            uint32_t cap_idx = cap_dist(rng);
            nimcp_capability_t& cap = test_caps[cap_idx];

            // Check various permissions
            bool is_valid = nimcp_capability_is_valid(caps, cap);
            bool has_read = nimcp_capability_check(caps, cap, NIMCP_PERM_READ);
            bool has_write = nimcp_capability_check(caps, cap, NIMCP_PERM_WRITE);
            uint32_t perms = nimcp_capability_get_permissions(caps, cap);

            checks_performed.fetch_add(4, std::memory_order_relaxed);
            if (is_valid && has_read) {
                valid_checks.fetch_add(1, std::memory_order_relaxed);
            }

            // Verify consistency: if valid, should have READ permission
            if (is_valid) {
                EXPECT_TRUE(has_read) << "Valid capability should have READ permission";
                EXPECT_EQ((perms & NIMCP_PERM_READ), NIMCP_PERM_READ)
                    << "Permissions inconsistent with check result";
            }
        }
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    /* WHAT: Verify all capabilities are still valid
     * WHY:  Read operations should not corrupt state
     */
    for (const auto& cap : test_caps) {
        EXPECT_TRUE(nimcp_capability_is_valid(caps, cap))
            << "Capability became invalid after concurrent reads";
    }

    /* WHAT: Verify check count matches expected
     * WHY:  No checks should be lost
     */
    uint64_t expected_checks = NUM_THREADS * OPS_PER_THREAD * 4;
    EXPECT_EQ(checks_performed.load(), expected_checks)
        << "Some checks were not performed";

    /* WHAT: Verify stats tracking
     * WHY:  Statistics should reflect all checks
     */
    nimcp_cap_stats_t stats;
    nimcp_result_t result = nimcp_capability_get_stats(caps, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.checks_performed, 0u) << "No checks recorded in statistics";
}

//=============================================================================
// Test 3: Thread Safety of Create/Destroy
//=============================================================================

/**
 * @brief Test thread safety of capability system create and destroy
 *
 * WHAT: Multiple threads creating and destroying separate capability systems
 * WHY:  Verify system lifecycle is thread-safe
 *
 * NOTE: Each thread uses its own capability system to avoid UB on shared destroy
 */
TEST_F(CapabilityThreadSafetyTest, CreateDestroyThreadSafety) {
    /* WHAT: Multiple threads creating/destroying independent systems
     * WHY:  Test allocation and deallocation thread safety
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint32_t> create_success{0};
    std::atomic<uint32_t> init_success{0};
    std::vector<std::thread> threads;

    auto thread_func = [&ready_count, &start, &create_success, &init_success]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* WHAT: Repeatedly create and destroy capability systems
         * WHY:  Test for memory corruption or race conditions in lifecycle
         */
        for (uint32_t i = 0; i < 100; i++) {
            nimcp_capability_system_t* local_caps = nimcp_capability_system_create();
            if (local_caps != nullptr) {
                create_success.fetch_add(1, std::memory_order_relaxed);

                nimcp_result_t result = nimcp_capability_system_init(local_caps);
                if (result == NIMCP_SUCCESS) {
                    init_success.fetch_add(1, std::memory_order_relaxed);

                    // Do some operations
                    nimcp_capability_t cap;
                    nimcp_capability_create(local_caps, NIMCP_RES_GENERIC, nullptr,
                                           NIMCP_PERM_READ, &cap);
                    nimcp_capability_is_valid(local_caps, cap);
                }

                nimcp_capability_system_destroy(local_caps);
            }
        }
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    /* WHAT: Verify operations succeeded
     * WHY:  All create/init should succeed
     */
    uint32_t expected = NUM_THREADS * 100;
    EXPECT_EQ(create_success.load(), expected) << "Some creates failed";
    EXPECT_EQ(init_success.load(), expected) << "Some inits failed";
}

//=============================================================================
// Test 4: Multiple Threads Accessing Different Capabilities
//=============================================================================

/**
 * @brief Test multiple threads accessing different capabilities simultaneously
 *
 * WHAT: Each thread operates on a disjoint set of capabilities
 * WHY:  Verify mutex doesn't cause false sharing or unnecessary contention
 */
TEST_F(CapabilityThreadSafetyTest, DisjointCapabilityAccess) {
    /* WHAT: Create capabilities partitioned by thread
     * WHY:  Test concurrent access to different entries
     */
    const uint32_t CAPS_PER_THREAD = 20;
    std::vector<std::vector<nimcp_capability_t>> thread_caps(NUM_THREADS);

    // Pre-create capabilities for each thread
    for (uint32_t t = 0; t < NUM_THREADS; t++) {
        for (uint32_t i = 0; i < CAPS_PER_THREAD; i++) {
            nimcp_capability_t cap;
            nimcp_result_t result = nimcp_capability_create(
                caps,
                NIMCP_RES_MEMORY,
                reinterpret_cast<void*>(static_cast<uintptr_t>((t * CAPS_PER_THREAD + i + 1))),
                NIMCP_PERM_READ | NIMCP_PERM_WRITE,
                &cap
            );
            ASSERT_EQ(result, NIMCP_SUCCESS);
            thread_caps[t].push_back(cap);
        }
    }

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> total_ops{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, &thread_caps, &ready_count, &start, &total_ops](uint32_t tid) {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* WHAT: Each thread operates only on its own capabilities
         * WHY:  Test disjoint access pattern efficiency
         */
        uint64_t ops = 0;
        for (uint32_t iter = 0; iter < OPS_PER_THREAD; iter++) {
            for (auto& cap : thread_caps[tid]) {
                // Check validity and permissions
                bool valid = nimcp_capability_is_valid(caps, cap);
                if (valid) {
                    nimcp_capability_check(caps, cap, NIMCP_PERM_READ);
                    nimcp_capability_get_permissions(caps, cap);
                    ops += 3;
                }
            }
        }
        total_ops.fetch_add(ops, std::memory_order_relaxed);
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, i);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    /* WHAT: Verify all operations completed
     * WHY:  No operations should be lost
     */
    uint64_t expected_ops = NUM_THREADS * OPS_PER_THREAD * CAPS_PER_THREAD * 3;
    EXPECT_EQ(total_ops.load(), expected_ops) << "Some operations were lost";

    /* WHAT: All capabilities should still be valid
     * WHY:  Disjoint access should not corrupt shared state
     */
    for (const auto& cap_vec : thread_caps) {
        for (const auto& cap : cap_vec) {
            EXPECT_TRUE(nimcp_capability_is_valid(caps, cap));
        }
    }

    // Performance note
    std::cout << "Disjoint access time: " << duration_ms << " ms for "
              << total_ops.load() << " operations" << std::endl;
}

//=============================================================================
// Test 5: Stress Test with Many Concurrent Operations
//=============================================================================

/**
 * @brief Stress test with many concurrent mixed operations
 *
 * WHAT: High volume of mixed operations: create, check, revoke, delegate
 * WHY:  Verify system stability under extreme load
 */
TEST_F(CapabilityThreadSafetyTest, StressTestMixedOperations) {
    /* WHAT: Create root capability for delegation tests
     * WHY:  Need delegatable parent capability
     */
    nimcp_capability_t root_cap;
    nimcp_result_t result = nimcp_capability_create_root(caps, &root_cap);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> create_ops{0};
    std::atomic<uint64_t> check_ops{0};
    std::atomic<uint64_t> revoke_ops{0};
    std::atomic<uint64_t> delegate_ops{0};
    std::atomic<uint64_t> errors{0};
    std::vector<std::thread> threads;

    // Shared capabilities (thread-safe access via mutex in cap system)
    std::mutex shared_caps_mutex;
    std::vector<nimcp_capability_t> shared_caps;
    shared_caps.reserve(1000);

    auto thread_func = [this, &root_cap, &ready_count, &start, &stop,
                       &create_ops, &check_ops, &revoke_ops, &delegate_ops, &errors,
                       &shared_caps_mutex, &shared_caps]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        std::mt19937 rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::uniform_int_distribution<int> op_dist(0, 3);

        /* WHAT: Perform random mixed operations until stop signal
         * WHY:  Realistic mixed workload
         */
        while (!stop.load(std::memory_order_acquire)) {
            int op = op_dist(rng);

            switch (op) {
                case 0: {
                    // Create capability
                    nimcp_capability_t cap;
                    nimcp_result_t result = nimcp_capability_create(
                        caps,
                        NIMCP_RES_GENERIC,
                        nullptr,
                        NIMCP_PERM_READ | NIMCP_PERM_WRITE,
                        &cap
                    );
                    if (result == NIMCP_SUCCESS) {
                        create_ops.fetch_add(1, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(shared_caps_mutex);
                        if (shared_caps.size() < 1000) {
                            shared_caps.push_back(cap);
                        }
                    } else if (result != NIMCP_BUFFER_TOO_SMALL) {
                        errors.fetch_add(1, std::memory_order_relaxed);
                    }
                    break;
                }
                case 1: {
                    // Check capability
                    nimcp_capability_t cap_to_check;
                    {
                        std::lock_guard<std::mutex> lock(shared_caps_mutex);
                        if (shared_caps.empty()) break;
                        std::uniform_int_distribution<size_t> idx_dist(0, shared_caps.size() - 1);
                        cap_to_check = shared_caps[idx_dist(rng)];
                    }
                    nimcp_capability_is_valid(caps, cap_to_check);
                    nimcp_capability_check(caps, cap_to_check, NIMCP_PERM_READ);
                    check_ops.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
                case 2: {
                    // Revoke capability
                    nimcp_capability_t cap_to_revoke;
                    {
                        std::lock_guard<std::mutex> lock(shared_caps_mutex);
                        if (shared_caps.empty()) break;
                        std::uniform_int_distribution<size_t> idx_dist(0, shared_caps.size() - 1);
                        size_t idx = idx_dist(rng);
                        cap_to_revoke = shared_caps[idx];
                        // Remove from list (may already be invalid)
                        shared_caps.erase(shared_caps.begin() + idx);
                    }
                    nimcp_capability_revoke(caps, cap_to_revoke);
                    revoke_ops.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
                case 3: {
                    // Delegate capability
                    nimcp_capability_t child;
                    nimcp_result_t result = nimcp_capability_delegate(
                        caps,
                        root_cap,
                        NIMCP_PERM_READ,
                        &child
                    );
                    if (result == NIMCP_SUCCESS) {
                        delegate_ops.fetch_add(1, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(shared_caps_mutex);
                        if (shared_caps.size() < 1000) {
                            shared_caps.push_back(child);
                        }
                    } else if (result != NIMCP_BUFFER_TOO_SMALL) {
                        // Delegation may fail if root was revoked
                    }
                    break;
                }
            }
        }
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    /* WHAT: Let run for 500ms (high stress)
     * WHY:  Sufficient time to detect race conditions
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    /* WHAT: Report operation counts
     * WHY:  Verify operations completed
     */
    uint64_t total = create_ops.load() + check_ops.load() +
                    revoke_ops.load() + delegate_ops.load();

    std::cout << "Stress test results:" << std::endl
              << "  Creates:   " << create_ops.load() << std::endl
              << "  Checks:    " << check_ops.load() << std::endl
              << "  Revokes:   " << revoke_ops.load() << std::endl
              << "  Delegates: " << delegate_ops.load() << std::endl
              << "  Total:     " << total << std::endl
              << "  Errors:    " << errors.load() << std::endl;

    EXPECT_GT(total, 0u) << "No operations were performed";
    EXPECT_EQ(errors.load(), 0u) << "Unexpected errors occurred";

    /* WHAT: Verify system is still valid
     * WHY:  No corruption should occur
     */
    nimcp_cap_stats_t stats;
    result = nimcp_capability_get_stats(caps, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Test 6: Concurrent Holder Management
//=============================================================================

/**
 * @brief Test concurrent holder registration and capability assignment
 *
 * WHAT: Multiple threads registering holders and assigning capabilities
 * WHY:  Verify holder management is thread-safe
 */
TEST_F(CapabilityThreadSafetyTest, ConcurrentHolderManagement) {
    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint32_t> register_success{0};
    std::atomic<uint32_t> assign_success{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, &ready_count, &start, &register_success, &assign_success](uint32_t tid) {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* WHAT: Register holder and assign capabilities
         * WHY:  Test holder management thread safety
         */
        for (uint32_t i = 0; i < 50; i++) {
            char name[64];
            snprintf(name, sizeof(name), "holder_%u_%u", tid, i);

            uint32_t holder_id;
            nimcp_result_t result = nimcp_capability_register_holder(caps, name, &holder_id);
            if (result == NIMCP_SUCCESS) {
                register_success.fetch_add(1, std::memory_order_relaxed);

                // Create and assign capability to holder
                nimcp_capability_t cap;
                result = nimcp_capability_create(caps, NIMCP_RES_GENERIC, nullptr,
                                                NIMCP_PERM_READ, &cap);
                if (result == NIMCP_SUCCESS) {
                    result = nimcp_capability_assign(caps, holder_id, cap);
                    if (result == NIMCP_SUCCESS) {
                        assign_success.fetch_add(1, std::memory_order_relaxed);
                    }
                }

                // Clean up
                nimcp_capability_remove_holder(caps, holder_id);
            }
        }
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func, i);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(register_success.load(), 0u) << "No holders were registered";
    EXPECT_GT(assign_success.load(), 0u) << "No capabilities were assigned";
}

//=============================================================================
// Test 7: Concurrent Statistics Access
//=============================================================================

/**
 * @brief Test concurrent statistics reads with concurrent writes
 *
 * WHAT: Threads reading stats while others modify the system
 * WHY:  Verify stats snapshot is consistent
 */
TEST_F(CapabilityThreadSafetyTest, ConcurrentStatsAccess) {
    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> stats_reads{0};
    std::atomic<uint64_t> write_ops{0};
    std::vector<std::thread> threads;

    auto reader_func = [this, &ready_count, &start, &stop, &stats_reads]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        while (!stop.load(std::memory_order_acquire)) {
            nimcp_cap_stats_t stats;
            nimcp_result_t result = nimcp_capability_get_stats(caps, &stats);
            if (result == NIMCP_SUCCESS) {
                stats_reads.fetch_add(1, std::memory_order_relaxed);

                // Verify stats are internally consistent
                EXPECT_GE(stats.total_capabilities, stats.active_capabilities);
                EXPECT_GE(stats.checks_performed, stats.checks_passed);
            }
        }
    };

    auto writer_func = [this, &ready_count, &start, &stop, &write_ops]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        while (!stop.load(std::memory_order_acquire)) {
            // Create capability
            nimcp_capability_t cap;
            nimcp_result_t result = nimcp_capability_create(caps, NIMCP_RES_GENERIC,
                                                           nullptr, NIMCP_PERM_READ, &cap);
            if (result == NIMCP_SUCCESS) {
                write_ops.fetch_add(1, std::memory_order_relaxed);

                // Check and revoke
                nimcp_capability_check(caps, cap, NIMCP_PERM_READ);
                nimcp_capability_revoke(caps, cap);
            }
        }
    };

    // 6 readers, 2 writers
    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < 6; i++) {
        threads.emplace_back(reader_func);
    }
    for (uint32_t i = 0; i < 2; i++) {
        threads.emplace_back(writer_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Stats reads: " << stats_reads.load()
              << ", Write ops: " << write_ops.load() << std::endl;

    EXPECT_GT(stats_reads.load(), 100u) << "Too few stats reads";
    EXPECT_GT(write_ops.load(), 10u) << "Too few write operations";
}

//=============================================================================
// Test 8: Concurrent Resource Revocation
//=============================================================================

/**
 * @brief Test concurrent revocation by resource pointer
 *
 * WHAT: Multiple threads revoking capabilities for same resource
 * WHY:  Verify resource-based revocation is atomic
 */
TEST_F(CapabilityThreadSafetyTest, ConcurrentResourceRevocation) {
    /* WHAT: Create multiple capabilities pointing to same "resource"
     * WHY:  Test revoke_for_resource thread safety
     */
    int fake_resource = 42;
    void* resource_ptr = &fake_resource;
    const uint32_t CAPS_PER_RESOURCE = 100;

    for (uint32_t i = 0; i < CAPS_PER_RESOURCE; i++) {
        nimcp_capability_t cap;
        nimcp_result_t result = nimcp_capability_create(
            caps,
            NIMCP_RES_MEMORY,
            resource_ptr,
            NIMCP_PERM_READ,
            &cap
        );
        ASSERT_EQ(result, NIMCP_SUCCESS);
    }

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> total_revoked{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, resource_ptr, &ready_count, &start, &total_revoked]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // All threads try to revoke for the same resource
        uint32_t revoked = nimcp_capability_revoke_for_resource(caps, resource_ptr);
        total_revoked.fetch_add(revoked, std::memory_order_relaxed);
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    /* WHAT: Total revoked should equal capabilities created
     * WHY:  Each capability should be revoked exactly once
     */
    EXPECT_EQ(total_revoked.load(), CAPS_PER_RESOURCE)
        << "Revocation count mismatch (possible race condition)";

    /* WHAT: Verify all capabilities are now invalid
     * WHY:  Double-check revocation worked
     */
    nimcp_cap_stats_t stats;
    nimcp_result_t result = nimcp_capability_get_stats(caps, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.revoked_capabilities, CAPS_PER_RESOURCE);
}

//=============================================================================
// Test 9: Delegation Chain Thread Safety
//=============================================================================

/**
 * @brief Test concurrent delegation chain operations
 *
 * WHAT: Multiple threads creating delegation chains
 * WHY:  Verify parent-child relationship integrity under concurrency
 */
TEST_F(CapabilityThreadSafetyTest, DelegationChainThreadSafety) {
    /* WHAT: Create root capability with delegation permission
     * WHY:  Need delegatable parent
     */
    nimcp_capability_t root_cap;
    nimcp_result_t result = nimcp_capability_create(
        caps,
        NIMCP_RES_GENERIC,
        nullptr,
        NIMCP_PERM_ALL,
        &root_cap
    );
    ASSERT_EQ(result, NIMCP_SUCCESS);

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> delegation_success{0};
    std::atomic<uint64_t> chain_revokes{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, &root_cap, &ready_count, &start,
                       &delegation_success, &chain_revokes]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* WHAT: Create delegation chains and revoke them
         * WHY:  Test cascading revocation thread safety
         */
        for (uint32_t i = 0; i < 50; i++) {
            /*
             * Note: The NIMCP capability system uses a single-level delegation model.
             * NIMCP_PERM_DELEGATE cannot be passed to children (security design).
             * This test verifies: delegate from root, verify validity, then revoke.
             */
            nimcp_capability_t child;

            nimcp_result_t r1 = nimcp_capability_delegate(
                caps, root_cap,
                NIMCP_PERM_READ | NIMCP_PERM_WRITE,
                &child
            );

            if (r1 == NIMCP_SUCCESS) {
                delegation_success.fetch_add(1, std::memory_order_relaxed);

                // Verify child is valid
                bool valid_before = nimcp_capability_is_valid(caps, child);
                if (valid_before) {
                    // Revoke the child capability
                    nimcp_capability_revoke(caps, child);
                    chain_revokes.fetch_add(1, std::memory_order_relaxed);

                    // Verify child is now invalid after revocation
                    bool valid_after = nimcp_capability_is_valid(caps, child);
                    EXPECT_FALSE(valid_after) << "Revoked capability should be invalid";
                }
            }
        }
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(delegation_success.load(), 0u) << "No delegations succeeded";
    EXPECT_GT(chain_revokes.load(), 0u) << "No chain revokes occurred";

    std::cout << "Delegation chain test: " << delegation_success.load()
              << " delegations, " << chain_revokes.load() << " chain revokes" << std::endl;
}

//=============================================================================
// Test 10: High Contention Single Capability
//=============================================================================

/**
 * @brief Test high contention on a single capability
 *
 * WHAT: All threads operate on the same capability token
 * WHY:  Maximum contention scenario for mutex testing
 */
TEST_F(CapabilityThreadSafetyTest, HighContentionSingleCapability) {
    /* WHAT: Create single capability for all threads to access
     * WHY:  Maximum contention on same entry
     */
    nimcp_capability_t shared_cap;
    nimcp_result_t result = nimcp_capability_create(
        caps,
        NIMCP_RES_GENERIC,
        nullptr,
        NIMCP_PERM_READ | NIMCP_PERM_WRITE | NIMCP_PERM_EXECUTE,
        &shared_cap
    );
    ASSERT_EQ(result, NIMCP_SUCCESS);

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> check_count{0};
    std::atomic<uint64_t> valid_count{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, &shared_cap, &ready_count, &start,
                       &check_count, &valid_count]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        /* WHAT: Hammer the same capability with checks
         * WHY:  Stress test single-entry contention
         */
        for (uint32_t i = 0; i < OPS_PER_THREAD * 10; i++) {
            bool valid = nimcp_capability_is_valid(caps, shared_cap);
            nimcp_capability_check(caps, shared_cap, NIMCP_PERM_READ);
            uint32_t perms = nimcp_capability_get_permissions(caps, shared_cap);

            check_count.fetch_add(3, std::memory_order_relaxed);
            if (valid && perms != 0) {
                valid_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    /* WHAT: All checks should succeed
     * WHY:  Capability was never revoked
     */
    uint64_t expected_valid = NUM_THREADS * OPS_PER_THREAD * 10;
    EXPECT_EQ(valid_count.load(), expected_valid)
        << "Some validity checks failed unexpectedly";

    /* WHAT: Capability should still be valid
     * WHY:  No thread should have corrupted it
     */
    EXPECT_TRUE(nimcp_capability_is_valid(caps, shared_cap));

    std::cout << "High contention test: " << check_count.load()
              << " checks in " << duration_ms << " ms" << std::endl;
}

//=============================================================================
// Test 11: Stats Reset Thread Safety
//=============================================================================

/**
 * @brief Test concurrent stats reset with operations
 *
 * WHAT: One thread resets stats while others operate
 * WHY:  Verify stats reset is atomic
 */
TEST_F(CapabilityThreadSafetyTest, StatsResetThreadSafety) {
    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> reset_count{0};
    std::vector<std::thread> threads;

    auto operation_func = [this, &ready_count, &start, &stop]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        while (!stop.load(std::memory_order_acquire)) {
            nimcp_capability_t cap;
            nimcp_result_t result = nimcp_capability_create(
                caps, NIMCP_RES_GENERIC, nullptr, NIMCP_PERM_READ, &cap);
            if (result == NIMCP_SUCCESS) {
                nimcp_capability_check(caps, cap, NIMCP_PERM_READ);
                nimcp_capability_revoke(caps, cap);
            }
        }
    };

    auto reset_func = [this, &ready_count, &start, &stop, &reset_count]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        while (!stop.load(std::memory_order_acquire)) {
            nimcp_result_t result = nimcp_capability_reset_stats(caps);
            if (result == NIMCP_SUCCESS) {
                reset_count.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    // 7 operation threads, 1 reset thread
    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < 7; i++) {
        threads.emplace_back(operation_func);
    }
    threads.emplace_back(reset_func);

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(reset_count.load(), 0u) << "No stats resets occurred";

    /* WHAT: Final stats should be valid (may be partially reset)
     * WHY:  No corruption should occur from concurrent reset
     */
    nimcp_cap_stats_t stats;
    nimcp_result_t result = nimcp_capability_get_stats(caps, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Test 12: Access Check with Resource Pointer
//=============================================================================

/**
 * @brief Test concurrent check_access operations
 *
 * WHAT: Multiple threads checking access with resource pointer matching
 * WHY:  Verify resource pointer comparison is thread-safe
 */
TEST_F(CapabilityThreadSafetyTest, ConcurrentCheckAccess) {
    /* WHAT: Create capabilities for different resources
     * WHY:  Test resource-specific access checks
     */
    int resources[10];
    std::vector<nimcp_capability_t> resource_caps;

    for (int i = 0; i < 10; i++) {
        resources[i] = i;
        nimcp_capability_t cap;
        nimcp_result_t result = nimcp_capability_create(
            caps,
            NIMCP_RES_MEMORY,
            &resources[i],
            NIMCP_PERM_READ | NIMCP_PERM_WRITE,
            &cap
        );
        ASSERT_EQ(result, NIMCP_SUCCESS);
        resource_caps.push_back(cap);
    }

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint64_t> access_checks{0};
    std::atomic<uint64_t> access_granted{0};
    std::atomic<uint64_t> access_denied{0};
    std::vector<std::thread> threads;

    auto thread_func = [this, &resources, &resource_caps, &ready_count, &start,
                       &access_checks, &access_granted, &access_denied]() {
        ready_count.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        std::mt19937 rng(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::uniform_int_distribution<int> idx_dist(0, 9);

        for (uint32_t i = 0; i < OPS_PER_THREAD; i++) {
            int cap_idx = idx_dist(rng);
            int resource_idx = idx_dist(rng);

            bool granted = nimcp_capability_check_access(
                caps,
                resource_caps[cap_idx],
                &resources[resource_idx],
                NIMCP_PERM_READ
            );

            access_checks.fetch_add(1, std::memory_order_relaxed);

            if (granted) {
                access_granted.fetch_add(1, std::memory_order_relaxed);
                // Should only be granted when cap_idx == resource_idx
                EXPECT_EQ(cap_idx, resource_idx);
            } else {
                access_denied.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    threads.reserve(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(thread_func);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    uint64_t expected_checks = NUM_THREADS * OPS_PER_THREAD;
    EXPECT_EQ(access_checks.load(), expected_checks);
    EXPECT_EQ(access_granted.load() + access_denied.load(), expected_checks);

    // Statistically, ~10% should be granted (when indices match)
    double grant_rate = static_cast<double>(access_granted.load()) / expected_checks;
    EXPECT_GT(grant_rate, 0.05) << "Grant rate suspiciously low";
    EXPECT_LT(grant_rate, 0.20) << "Grant rate suspiciously high";
}

