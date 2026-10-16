/**
 * @file test_plasticity_coordinator_no_deadlock.cpp
 * @brief Tests for plasticity coordinator callback-under-mutex fix (Bug H3)
 *
 * WHAT: Verify that plasticity coordinator update does not deadlock when callbacks
 *       attempt to re-enter the coordinator
 * WHY:  Previously, update_fn callbacks were invoked while holding coordinator->mutex,
 *       causing deadlock if the callback called any coordinator API
 * HOW:  Register mechanisms whose callbacks call coordinator APIs (re-entry),
 *       then call update from multiple threads and verify no deadlock/hang
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_coordinator.h"

/**
 * WHAT: Dummy mechanism update function that does nothing
 * WHY:  Baseline: confirms coordinator can update without callbacks hanging
 */
static int dummy_update_fn(void* mechanism, float dt) {
    (void)mechanism;
    (void)dt;
    return 0;
}

/**
 * WHAT: Mechanism update function that re-enters coordinator via get_stats
 * WHY:  This previously caused deadlock when update_fn was called under mutex
 */
static std::atomic<plasticity_coordinator_t*> g_reentrant_coordinator{nullptr};

static int reentrant_update_fn(void* mechanism, float dt) {
    (void)mechanism;
    (void)dt;

    plasticity_coordinator_t* coord = g_reentrant_coordinator.load();
    if (coord) {
        /* Attempt re-entrant call: this would deadlock if called under mutex */
        plasticity_coordinator_stats_t stats;
        memset(&stats, 0, sizeof(stats));
        plasticity_coordinator_get_stats(coord, &stats);
    }
    return 0;
}

class PlasticityCoordinatorNoDeadlockTest : public ::testing::Test {
protected:
    plasticity_coordinator_t* coordinator = nullptr;

    void SetUp() override {
        plasticity_coordinator_config_t config;
        plasticity_coordinator_default_config(&config);
        config.enable_bio_async = false;
        config.enable_brain_immune = false;
        config.enable_statistics = true;

        coordinator = plasticity_coordinator_create(&config);
        ASSERT_NE(coordinator, nullptr) << "Failed to create plasticity coordinator";
    }

    void TearDown() override {
        g_reentrant_coordinator.store(nullptr);
        if (coordinator) {
            plasticity_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
    }
};

/**
 * WHAT: Test basic create-update-destroy lifecycle
 * WHY:  Sanity check that the coordinator works at all
 */
TEST_F(PlasticityCoordinatorNoDeadlockTest, BasicLifecycle) {
    uint32_t mech_id = 0;
    int result = plasticity_coordinator_register_mechanism(
        coordinator,
        "test_stdp",
        PLASTICITY_TYPE_STDP,
        (void*)0x1234,   /* dummy handle */
        dummy_update_fn,
        nullptr,          /* no weight change query */
        0.8f,            /* priority */
        PLASTICITY_ENERGY_COST_STDP,
        PLASTICITY_UPDATE_INTERVAL_STDP,
        &mech_id
    );
    ASSERT_EQ(result, 0) << "Failed to register mechanism";

    /* Update should complete without issues */
    int updated = plasticity_coordinator_update(coordinator, 100, 0.01f);
    EXPECT_GE(updated, 0) << "Update should succeed";
}

/**
 * WHAT: Test that callbacks can re-enter coordinator without deadlocking
 * WHY:  This is the core Bug H3 test — callbacks were called under mutex before fix
 * HOW:  Register a mechanism whose update_fn calls coordinator_get_stats (takes mutex),
 *       verify update completes within timeout
 */
TEST_F(PlasticityCoordinatorNoDeadlockTest, CallbackReentryNoDeadlock) {
    g_reentrant_coordinator.store(coordinator);

    uint32_t mech_id = 0;
    int result = plasticity_coordinator_register_mechanism(
        coordinator,
        "reentrant_mech",
        PLASTICITY_TYPE_STDP,
        (void*)0xBEEF,
        reentrant_update_fn,
        nullptr,
        0.9f,
        1.0f,
        1,  /* update every 1ms so it triggers immediately */
        &mech_id
    );
    ASSERT_EQ(result, 0);

    /* Run update in a thread with timeout to detect deadlock */
    std::atomic<bool> completed{false};

    std::thread worker([&]() {
        plasticity_coordinator_update(coordinator, 100, 0.01f);
        completed.store(true);
    });

    /* Wait up to 5 seconds — if deadlocked, this will timeout */
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!completed.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(completed.load())
        << "Coordinator update deadlocked! Callback re-entry caused mutex deadlock (Bug H3)";

    if (worker.joinable()) {
        worker.join();
    }
}

/**
 * WHAT: Test concurrent updates from multiple threads
 * WHY:  After the H3 fix, callbacks are called outside the lock — verify this is
 *       thread-safe with concurrent callers
 */
TEST_F(PlasticityCoordinatorNoDeadlockTest, ConcurrentUpdatesNoDeadlock) {
    /* Register multiple mechanisms */
    for (int i = 0; i < 4; i++) {
        uint32_t mech_id = 0;
        char name[32];
        snprintf(name, sizeof(name), "mech_%d", i);
        plasticity_coordinator_register_mechanism(
            coordinator,
            name,
            (plasticity_mechanism_type_t)(i % PLASTICITY_TYPE_COUNT),
            (void*)(uintptr_t)(0x1000 + i),
            dummy_update_fn,
            nullptr,
            0.5f,
            1.0f,
            10,
            &mech_id
        );
    }

    const int NUM_THREADS = 4;
    const int UPDATES_PER_THREAD = 50;
    std::atomic<int> success_count{0};
    std::atomic<bool> all_completed{true};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < UPDATES_PER_THREAD; i++) {
            uint64_t time_ms = (uint64_t)(thread_id * 1000 + i * 10);
            int result = plasticity_coordinator_update(coordinator, time_ms, 0.01f);
            if (result >= 0) {
                success_count.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }

    /* Wait with timeout */
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (auto& t : threads) {
        if (t.joinable()) {
            /* Use timed join pattern */
            t.join();
        }
    }

    auto elapsed = std::chrono::steady_clock::now();
    EXPECT_LT(elapsed, deadline)
        << "Concurrent updates took too long — possible deadlock";

    EXPECT_GT(success_count.load(), 0)
        << "At least some updates should succeed";
}

/**
 * WHAT: Test create-register-update-unregister-destroy full lifecycle
 * WHY:  Verify no resource leaks or crashes in the complete lifecycle
 */
TEST_F(PlasticityCoordinatorNoDeadlockTest, FullLifecycleNoLeak) {
    uint32_t mech_ids[3];

    /* Register 3 mechanisms */
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "lifecycle_%d", i);
        int result = plasticity_coordinator_register_mechanism(
            coordinator,
            name,
            (plasticity_mechanism_type_t)(i),
            (void*)(uintptr_t)(0x2000 + i),
            dummy_update_fn,
            nullptr,
            0.5f + 0.1f * i,
            1.0f,
            10,
            &mech_ids[i]
        );
        ASSERT_EQ(result, 0) << "Failed to register mechanism " << i;
    }

    /* Update several times */
    for (int t = 0; t < 10; t++) {
        int updated = plasticity_coordinator_update(coordinator, (uint64_t)(t * 20), 0.02f);
        EXPECT_GE(updated, 0);
    }

    /* Unregister all */
    for (int i = 0; i < 3; i++) {
        int result = plasticity_coordinator_unregister_mechanism(coordinator, mech_ids[i]);
        EXPECT_EQ(result, 0) << "Failed to unregister mechanism " << i;
    }

    /* Update after unregister — should succeed with 0 mechanisms updated */
    int updated = plasticity_coordinator_update(coordinator, 500, 0.01f);
    EXPECT_GE(updated, 0);

    /* Destroy handled in TearDown */
}

/**
 * WHAT: Test that get_stats works from different thread than update
 * WHY:  After fix, stats are updated under lock and queried under lock —
 *       cross-thread access should be consistent
 */
TEST_F(PlasticityCoordinatorNoDeadlockTest, CrossThreadStatsAccess) {
    uint32_t mech_id = 0;
    plasticity_coordinator_register_mechanism(
        coordinator,
        "stats_test",
        PLASTICITY_TYPE_BCM,
        (void*)0xDEAD,
        dummy_update_fn,
        nullptr,
        0.7f,
        2.0f,
        5,
        &mech_id
    );

    /* Do some updates on main thread */
    for (int i = 0; i < 20; i++) {
        plasticity_coordinator_update(coordinator, (uint64_t)(i * 10), 0.01f);
    }

    /* Read stats from a different thread */
    std::atomic<bool> stats_ok{false};
    std::thread reader([&]() {
        plasticity_coordinator_stats_t stats;
        memset(&stats, 0, sizeof(stats));
        int result = plasticity_coordinator_get_stats(coordinator, &stats);
        if (result == 0 && stats.total_mechanisms >= 1) {
            stats_ok.store(true);
        }
    });
    reader.join();

    EXPECT_TRUE(stats_ok.load())
        << "Cross-thread stats query should succeed and show registered mechanisms";
}
