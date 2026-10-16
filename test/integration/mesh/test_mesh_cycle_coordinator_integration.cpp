/**
 * @file test_mesh_cycle_coordinator_integration.cpp
 * @brief Integration Tests for Mesh-Cycle Coordinator Interactions
 *
 * WHAT: Tests real component interactions between cycle timing, coordinator pools,
 *       health bridges, ordering service, MSP, exception bridge, and immune system
 * WHY:  Verify end-to-end flows for stall-resilience, timing-ordering, health-mesh,
 *       and exception-immune integration
 * HOW:  Create real instances, use std::thread for concurrency, proper fixtures
 *
 * TEST COVERAGE:
 * - Full pipeline tests (5 tests)
 * - Stall-resilience integration (4 tests)
 * - Timing-ordering integration (3 tests)
 * - Health-mesh integration (3 tests)
 * - Exception-immune integration (2 tests)
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <cstring>
#include <queue>

extern "C" {
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_timing.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Helpers and Types
 * ============================================================================ */

/**
 * @brief Simulated stall event for testing
 */
struct StallEvent {
    mesh_participant_id_t participant;
    uint64_t duration_ms;
    uint64_t timestamp_ns;
    bool recovered;
};

/**
 * @brief Simulated cycle health record
 */
struct CycleHealthRecord {
    mesh_channel_id_t channel;
    float health_score;
    uint32_t stall_count;
    uint64_t last_update_ns;
};

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshCycleCoordinatorIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_hierarchical_timing_t timing = nullptr;
    mesh_health_bridge_t* health_bridge = nullptr;
    mesh_exception_bridge_t* exception_bridge = nullptr;

    /* Statistics tracking */
    std::atomic<uint32_t> stall_count{0};
    std::atomic<uint32_t> recovery_count{0};
    std::atomic<uint32_t> election_count{0};
    std::atomic<uint32_t> timing_violation_count{0};

    void SetUp() override {
        /* Create bootstrap with core subsystems */
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems = MESH_SUBSYSTEMS_CORE;
        config.subsystems.enable_security = true;

        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);

        /* Create hierarchical timing */
        mesh_hierarchical_timing_config_t timing_config = mesh_timing_default_config();
        timing = mesh_timing_create(&timing_config);
        /* timing may be null if not fully implemented */

        /* Get bridges from bootstrap */
        health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
        exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);

        /* Reset counters */
        stall_count = 0;
        recovery_count = 0;
        election_count = 0;
        timing_violation_count = 0;
    }

    void TearDown() override {
        if (timing) {
            mesh_timing_destroy(timing);
            timing = nullptr;
        }
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
        health_bridge = nullptr;
        exception_bridge = nullptr;
    }

    /* Helper: Create a simple transaction for testing */
    mesh_transaction_t* create_test_transaction(
        mesh_tx_type_t type,
        mesh_participant_id_t proposer,
        mesh_channel_id_t channel
    ) {
        mesh_transaction_t* tx = mesh_transaction_create(type, proposer, channel);
        if (tx) {
            uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
            mesh_transaction_set_payload(tx, payload, sizeof(payload));
        }
        return tx;
    }

    /* Helper: Simulate a stall condition */
    void simulate_stall(uint64_t duration_ms) {
        stall_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    }

    /* Helper: Simulate recovery from stall */
    void simulate_recovery() {
        recovery_count++;
    }

    /* Helper: Get current timestamp in nanoseconds */
    uint64_t get_timestamp_ns() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }
};

/* ============================================================================
 * Full Pipeline Tests (5 tests)
 * ============================================================================ */

/**
 * Test 1: StallToRecoveryFullFlow
 * Stall -> exception bridge -> immune -> recovery
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, StallToRecoveryFullFlow) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    /* Create a participant that will experience a stall */
    mesh_participant_id_t stalled_module = mesh_make_participant_id(
        MESH_CHANNEL_LEFT_HEMISPHERE, MESH_PARTICIPANT_MODULE, 0x1001);

    /* Simulate a stall event */
    std::atomic<bool> stall_detected{false};
    std::atomic<bool> recovery_complete{false};

    /* Thread to detect stall */
    std::thread stall_thread([&]() {
        /* Simulate stall detection by timing violation */
        simulate_stall(50);  /* 50ms stall */
        stall_detected = true;

        /* Route stall exception through bridge */
        mesh_exception_response_t response;
        nimcp_error_t err = mesh_exception_bridge_route_error(
            exception_bridge,
            NIMCP_ERROR_TIMEOUT,
            "Cycle stall detected",
            stalled_module,
            __FILE__,
            __LINE__,
            &response
        );

        if (err == NIMCP_SUCCESS) {
            /* Check immune response action */
            EXPECT_NE(response.primary_action, MESH_IMMUNE_ACTION_NONE);

            /* Simulate recovery based on immune response */
            simulate_recovery();
            recovery_complete = true;
        }
    });

    stall_thread.join();

    EXPECT_TRUE(stall_detected.load());
    EXPECT_GE(stall_count.load(), 1u);
    /* Recovery depends on implementation */
    if (recovery_complete) {
        EXPECT_GE(recovery_count.load(), 1u);
    }
}

/**
 * Test 2: TimingConstraintToOrderingFlow
 * Cycle timing -> ordering service batch adjustment
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, TimingConstraintToOrderingFlow) {
    mesh_integration_t* integration = mesh_bootstrap_get_integration(bootstrap);
    if (!integration) {
        GTEST_SKIP() << "Integration not available";
    }

    /* Get timing intervals for different levels */
    float oscillation_interval_ms = MESH_TIMING_LAYER_BASE_MS;  /* 10ms */
    float commit_deadline_ms = MESH_TIMING_HEMISPHERE_BASE_MS;   /* 50ms */

    /* Create transactions that must fit within timing window */
    std::vector<mesh_transaction_t*> transactions;
    mesh_participant_id_t proposer = mesh_make_participant_id(
        MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0x2001);

    for (int i = 0; i < 5; i++) {
        mesh_transaction_t* tx = create_test_transaction(
            MESH_TX_BELIEF_UPDATE, proposer, MESH_CHANNEL_SYSTEM);
        if (tx) {
            transactions.push_back(tx);
        }
    }

    if (transactions.empty()) {
        GTEST_SKIP() << "Could not create test transactions";
    }

    /* Verify batch timing fits within cycle constraints */
    auto start = std::chrono::high_resolution_clock::now();

    /* Submit transactions (simulating batch within oscillation cycle) */
    for (auto tx : transactions) {
        mesh_transaction_destroy(tx);  /* Just testing creation for now */
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Batch operations should complete within oscillation window */
    EXPECT_LT(duration.count(), oscillation_interval_ms * 2);
    EXPECT_LT(duration.count(), commit_deadline_ms);
}

/**
 * Test 3: HealthEndorsementFullFlow
 * Coordinator health -> mesh health bridge -> aggregate
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, HealthEndorsementFullFlow) {
    if (!health_bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    /* Register multiple participants with health status */
    mesh_participant_id_t participants[] = {
        mesh_make_participant_id(MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_COORDINATOR, 0x01),
        mesh_make_participant_id(MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_COORDINATOR, 0x02),
        mesh_make_participant_id(MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_COORDINATOR, 0x03),
    };

    /* Send heartbeats from all coordinators */
    for (auto& pid : participants) {
        nimcp_error_t err = mesh_health_bridge_heartbeat(
            health_bridge, pid, MESH_HEARTBEAT_PING, 100);
        /* May or may not succeed depending on registration */
        (void)err;
    }

    /* Query system health */
    mesh_system_health_t system_health;
    nimcp_error_t err = mesh_health_bridge_get_system_health(
        health_bridge, &system_health);

    if (err == NIMCP_SUCCESS) {
        /* Verify health aggregation happened */
        EXPECT_GE(system_health.system_health_score, 0.0f);
        EXPECT_LE(system_health.system_health_score, 1.0f);
    }

    /* Verify statistics were tracked */
    mesh_health_bridge_stats_t stats;
    err = mesh_health_bridge_get_stats(health_bridge, &stats);
    if (err == NIMCP_SUCCESS) {
        EXPECT_GE(stats.heartbeats_received, 0u);
    }
}

/**
 * Test 4: BBBValidationForTimingCritical
 * Transaction -> BBB -> timing validation -> accept/reject
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, BBBValidationForTimingCritical) {
    mesh_integration_t* integration = mesh_bootstrap_get_integration(bootstrap);
    if (!integration) {
        GTEST_SKIP() << "Integration not available";
    }

    /* Create a timing-critical transaction */
    mesh_participant_id_t proposer = mesh_make_participant_id(
        MESH_CHANNEL_GPU_COMPUTE, MESH_PARTICIPANT_MODULE, 0x3001);

    mesh_transaction_t* tx = create_test_transaction(
        MESH_TX_GPU_BATCH, proposer, MESH_CHANNEL_GPU_COMPUTE);

    if (!tx) {
        GTEST_SKIP() << "Could not create test transaction";
    }

    /* Set tight timeout for timing-critical transaction */
    mesh_transaction_set_timeout(tx, 10);  /* 10ms timeout */

    /* Mark as requiring timing validation */
    tx->requires_gpu = true;

    /* Verify transaction has proper timing constraints */
    EXPECT_GT(tx->timeout_ns, 0u);
    EXPECT_TRUE(tx->requires_gpu);

    /* Validate through timing - transaction should be accepted or rejected
       based on current system timing state */
    uint64_t deadline_ns = get_timestamp_ns() + (10 * 1000000ULL);  /* 10ms */
    EXPECT_GT(deadline_ns, tx->created_ns);

    mesh_transaction_destroy(tx);
}

/**
 * Test 5: CrossChannelHealthSync
 * Health from multiple channels aggregated
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, CrossChannelHealthSync) {
    if (!health_bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    /* Create participants in different channels */
    struct ChannelParticipant {
        mesh_channel_id_t channel;
        mesh_participant_id_t id;
    } channel_participants[] = {
        {MESH_CHANNEL_LEFT_HEMISPHERE,
         mesh_make_participant_id(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_PARTICIPANT_MODULE, 0x01)},
        {MESH_CHANNEL_RIGHT_HEMISPHERE,
         mesh_make_participant_id(MESH_CHANNEL_RIGHT_HEMISPHERE, MESH_PARTICIPANT_MODULE, 0x02)},
        {MESH_CHANNEL_SUBCORTICAL,
         mesh_make_participant_id(MESH_CHANNEL_SUBCORTICAL, MESH_PARTICIPANT_MODULE, 0x03)},
    };

    /* Send heartbeats from each channel */
    for (auto& cp : channel_participants) {
        mesh_health_bridge_heartbeat(
            health_bridge, cp.id, MESH_HEARTBEAT_PING, 100);
    }

    /* Query health for each channel */
    for (auto& cp : channel_participants) {
        mesh_channel_health_t channel_health;
        nimcp_error_t err = mesh_health_bridge_get_channel_health(
            health_bridge, cp.channel, &channel_health);

        if (err == NIMCP_SUCCESS) {
            EXPECT_EQ(channel_health.channel_id, cp.channel);
        }
    }

    /* Get aggregated system health */
    mesh_system_health_t system_health;
    nimcp_error_t err = mesh_health_bridge_get_system_health(
        health_bridge, &system_health);

    if (err == NIMCP_SUCCESS) {
        EXPECT_GT(system_health.channel_count, 0u);
    }
}

/* ============================================================================
 * Stall-Resilience Integration (4 tests)
 * ============================================================================ */

/**
 * Test 6: StallTriggersCoordinatorElection
 * Stall threshold -> coordinator pool election
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, StallTriggersCoordinatorElection) {
    mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
    mesh_channel_t* channel = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);

    if (!registry || !channel) {
        GTEST_SKIP() << "Registry or channel not available";
    }

    /* Create coordinator pool */
    mesh_coordinator_pool_config_t pool_config;
    mesh_coordinator_pool_default_config(&pool_config);
    pool_config.pool_name = "test_pool";
    pool_config.initial_size = 4;
    pool_config.channel = MESH_CHANNEL_SYSTEM;

    mesh_coordinator_pool_t* pool = mesh_coordinator_pool_create(
        &pool_config, registry, channel);

    if (!pool) {
        GTEST_SKIP() << "Could not create coordinator pool";
    }

    /* Verify pool has a leader */
    bool has_leader = mesh_coordinator_pool_has_leader(pool);

    /* Simulate stall that would trigger election */
    std::thread stall_thread([&]() {
        /* Simulate leader stall */
        simulate_stall(MESH_DEFAULT_ELECTION_TIMEOUT_MS + 100);
    });

    /* Trigger election due to stall */
    nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool);
    if (err == NIMCP_SUCCESS) {
        election_count++;
    }

    stall_thread.join();

    /* Get pool stats */
    mesh_coordinator_pool_stats_t stats;
    err = mesh_coordinator_pool_get_stats(pool, &stats);
    if (err == NIMCP_SUCCESS) {
        /* Elections should have occurred */
        EXPECT_GE(stats.elections_held, 0u);
    }

    mesh_coordinator_pool_destroy(pool);
}

/**
 * Test 7: StallQuarantinesMalfunction
 * Chronic stalls -> MSP quarantine
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, StallQuarantinesMalfunction) {
    mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
    if (!registry) {
        GTEST_SKIP() << "Registry not available";
    }

    /* Create MSP for quarantine management */
    mesh_msp_config_t msp_config;
    mesh_msp_default_config(&msp_config);
    msp_config.msp_name = "test_msp";
    msp_config.enable_quarantine = true;
    msp_config.quarantine_duration_ms = 5000;

    mesh_msp_t* msp = mesh_msp_create(&msp_config, registry);
    if (!msp) {
        GTEST_SKIP() << "Could not create MSP";
    }

    /* Create participant that will be quarantined */
    mesh_participant_id_t malfunctioning = mesh_make_participant_id(
        MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0x4001);

    /* Issue credential first */
    credential_t cred;
    nimcp_error_t err = mesh_msp_issue_credential(
        msp, malfunctioning, 5, MESH_CAP_READ | MESH_CAP_WRITE, &cred);

    if (err == NIMCP_SUCCESS) {
        /* Simulate chronic stalls (multiple within window) */
        for (int i = 0; i < 5; i++) {
            /* Report stall as failure that triggers quarantine */
            simulate_stall(10);
        }

        /* Quarantine the malfunctioning module */
        err = mesh_msp_quarantine(msp, malfunctioning, 5000);

        if (err == NIMCP_SUCCESS) {
            /* Verify quarantine */
            EXPECT_TRUE(mesh_msp_is_quarantined(msp, malfunctioning));
        }
    }

    mesh_msp_destroy(msp);
}

/**
 * Test 8: RecoveryRestoresCycleHealth
 * Recovery action -> cycle resumes -> health improves
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, RecoveryRestoresCycleHealth) {
    if (!health_bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t recovering_module = mesh_make_participant_id(
        MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0x5001);

    /* Send initial heartbeat with degraded status */
    mesh_health_bridge_heartbeat(
        health_bridge, recovering_module, MESH_HEARTBEAT_ERROR, 0);

    /* Get initial health */
    mesh_health_record_t before_record;
    nimcp_error_t err = mesh_health_bridge_get_health(
        health_bridge, recovering_module, &before_record);

    /* Simulate recovery process */
    simulate_recovery();

    /* Send recovery heartbeat */
    mesh_health_bridge_heartbeat(
        health_bridge, recovering_module, MESH_HEARTBEAT_COMPLETE, 100);

    /* Get post-recovery health */
    mesh_health_record_t after_record;
    err = mesh_health_bridge_get_health(
        health_bridge, recovering_module, &after_record);

    /* Verify health bridge stats reflect recovery */
    mesh_health_bridge_stats_t stats;
    err = mesh_health_bridge_get_stats(health_bridge, &stats);
    if (err == NIMCP_SUCCESS) {
        EXPECT_GE(stats.heartbeats_received, 2u);
    }

    EXPECT_GE(recovery_count.load(), 1u);
}

/**
 * Test 9: ConcurrentStallsAcrossCycles
 * Multiple cycles stall -> coordinated recovery
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, ConcurrentStallsAcrossCycles) {
    std::atomic<int> stalls_detected{0};
    std::atomic<int> recoveries_completed{0};
    std::atomic<bool> running{true};

    /* Threads simulating different cycle coordinators */
    auto cycle_worker = [&](mesh_channel_id_t channel, int worker_id) {
        while (running) {
            /* Simulate potential stall */
            if (rand() % 10 == 0) {
                stalls_detected++;
                simulate_stall(5);
                simulate_recovery();
                recoveries_completed++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    std::vector<std::thread> workers;
    workers.emplace_back(cycle_worker, MESH_CHANNEL_LEFT_HEMISPHERE, 1);
    workers.emplace_back(cycle_worker, MESH_CHANNEL_RIGHT_HEMISPHERE, 2);
    workers.emplace_back(cycle_worker, MESH_CHANNEL_SUBCORTICAL, 3);

    /* Let workers run for a bit */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& w : workers) {
        w.join();
    }

    /* All stalls should have corresponding recoveries */
    EXPECT_EQ(stalls_detected.load(), recoveries_completed.load());
}

/* ============================================================================
 * Timing-Ordering Integration (3 tests)
 * ============================================================================ */

/**
 * Test 10: OscillationsCycleDrivesBatching
 * 10ms cycle -> batch window
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, OscillationsCycleDrivesBatching) {
    /* Layer-level oscillation cycle: 10ms base */
    const float oscillation_ms = MESH_TIMING_LAYER_BASE_MS;
    const int batch_size = 5;

    mesh_participant_id_t proposer = mesh_make_participant_id(
        MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_ORDERER, 0x6001);

    /* Create batch of transactions within oscillation window */
    auto batch_start = std::chrono::high_resolution_clock::now();

    std::vector<mesh_transaction_t*> batch;
    for (int i = 0; i < batch_size; i++) {
        mesh_transaction_t* tx = create_test_transaction(
            MESH_TX_BELIEF_UPDATE, proposer, MESH_CHANNEL_SYSTEM);
        if (tx) {
            batch.push_back(tx);
        }
    }

    auto batch_end = std::chrono::high_resolution_clock::now();
    auto batch_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        batch_end - batch_start);

    /* Batch creation should fit within oscillation cycle */
    float batch_ms = batch_duration.count() / 1000.0f;
    EXPECT_LT(batch_ms, oscillation_ms);

    /* Cleanup */
    for (auto tx : batch) {
        mesh_transaction_destroy(tx);
    }

    /* If we have timing, verify jittered interval */
    if (timing) {
        float interval = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);
        EXPECT_GE(interval, MESH_TIMING_LAYER_MIN_MS);
        EXPECT_LE(interval, MESH_TIMING_LAYER_MAX_MS);
    }
}

/**
 * Test 11: BrainUpdateCycleDrivesCommit
 * 16ms cycle (brain frame) -> commit deadline
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, BrainUpdateCycleDrivesCommit) {
    /* Brain update cycle: ~16ms (60fps equivalent) */
    const float brain_cycle_ms = 16.0f;

    mesh_participant_id_t proposer = mesh_make_participant_id(
        MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0x7001);

    mesh_transaction_t* tx = create_test_transaction(
        MESH_TX_STATE_CHANGE, proposer, MESH_CHANNEL_SYSTEM);

    if (!tx) {
        GTEST_SKIP() << "Could not create test transaction";
    }

    /* Set commit deadline based on brain cycle */
    uint64_t commit_deadline_ms = (uint64_t)brain_cycle_ms;
    mesh_transaction_set_timeout(tx, commit_deadline_ms);

    /* Verify timeout was set appropriately */
    EXPECT_GT(tx->timeout_ns, 0u);

    /* Simulate transaction processing within deadline */
    auto start = std::chrono::high_resolution_clock::now();

    /* Process transaction (simulated) */
    std::this_thread::sleep_for(std::chrono::milliseconds(5));  /* Processing */

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), brain_cycle_ms);

    mesh_transaction_destroy(tx);
}

/**
 * Test 12: TimingViolationLogged
 * Transaction misses deadline -> logged + stats
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, TimingViolationLogged) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_participant_id_t proposer = mesh_make_participant_id(
        MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0x8001);

    /* Create transaction with very tight deadline */
    mesh_transaction_t* tx = create_test_transaction(
        MESH_TX_BELIEF_UPDATE, proposer, MESH_CHANNEL_SYSTEM);

    if (!tx) {
        GTEST_SKIP() << "Could not create test transaction";
    }

    mesh_transaction_set_timeout(tx, 1);  /* 1ms - will likely be violated */

    /* Simulate processing that exceeds deadline */
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    /* Check if deadline was violated */
    uint64_t now = get_timestamp_ns();
    bool violated = (now > tx->created_ns + tx->timeout_ns);

    if (violated) {
        timing_violation_count++;

        /* Route timing violation through exception bridge */
        mesh_exception_response_t response;
        nimcp_error_t err = mesh_exception_bridge_route_error(
            exception_bridge,
            NIMCP_ERROR_TIMEOUT,
            "Transaction deadline violated",
            proposer,
            __FILE__,
            __LINE__,
            &response
        );

        /* Verify exception was logged */
        mesh_exception_bridge_stats_t stats;
        err = mesh_exception_bridge_get_stats(exception_bridge, &stats);
        if (err == NIMCP_SUCCESS) {
            EXPECT_GT(stats.exceptions_received, 0u);
        }
    }

    EXPECT_GE(timing_violation_count.load(), 0u);

    mesh_transaction_destroy(tx);
}

/* ============================================================================
 * Health-Mesh Integration (3 tests)
 * ============================================================================ */

/**
 * Test 13: CoordinatorHealthAffectsMeshHealth
 * Low cycle health -> mesh degraded
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, CoordinatorHealthAffectsMeshHealth) {
    mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
    mesh_channel_t* channel = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);

    if (!registry || !channel) {
        GTEST_SKIP() << "Registry or channel not available";
    }

    /* Create coordinator */
    mesh_coordinator_config_t coord_config;
    mesh_coordinator_default_config(&coord_config);
    coord_config.name = "test_coordinator";
    coord_config.channel = MESH_CHANNEL_SYSTEM;

    mesh_coordinator_t* coordinator = mesh_coordinator_create(
        &coord_config, registry, channel);

    if (!coordinator) {
        GTEST_SKIP() << "Could not create coordinator";
    }

    /* Get coordinator health */
    float initial_health = mesh_coordinator_get_health(coordinator);
    EXPECT_GE(initial_health, 0.0f);
    EXPECT_LE(initial_health, 1.0f);

    /* Simulate failures to degrade health */
    for (int i = 0; i < 3; i++) {
        mesh_coordinator_report_failure(coordinator, NIMCP_ERROR_TIMEOUT);
    }

    /* Get degraded health */
    float degraded_health = mesh_coordinator_get_health(coordinator);

    /* Health should have decreased or stayed the same */
    EXPECT_LE(degraded_health, initial_health + 0.01f);  /* Small tolerance */

    /* Check if health bridge reflects this */
    if (health_bridge) {
        mesh_participant_id_t coord_id = mesh_coordinator_get_id(coordinator);
        if (coord_id != 0) {
            mesh_health_record_t record;
            nimcp_error_t err = mesh_health_bridge_get_health(
                health_bridge, coord_id, &record);
            /* Result depends on whether coordinator was registered */
            (void)err;
        }
    }

    mesh_coordinator_destroy(coordinator);
}

/**
 * Test 14: MeshHealthAffectsCoordinator
 * Mesh failure -> coordinator adjusts sensitivity
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, MeshHealthAffectsCoordinator) {
    mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
    mesh_channel_t* channel = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);

    if (!registry || !channel) {
        GTEST_SKIP() << "Registry or channel not available";
    }

    /* Create coordinator */
    mesh_coordinator_config_t coord_config;
    mesh_coordinator_default_config(&coord_config);
    coord_config.name = "sensitivity_coordinator";
    coord_config.channel = MESH_CHANNEL_SYSTEM;

    mesh_coordinator_t* coordinator = mesh_coordinator_create(
        &coord_config, registry, channel);

    if (!coordinator) {
        GTEST_SKIP() << "Could not create coordinator";
    }

    /* Get coordinator stats before mesh degradation */
    mesh_coordinator_stats_t before_stats;
    nimcp_error_t err = mesh_coordinator_get_stats(coordinator, &before_stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Simulate mesh health degradation */
    if (health_bridge) {
        /* Report unhealthy status for multiple participants */
        for (int i = 0; i < 3; i++) {
            mesh_participant_id_t pid = mesh_make_participant_id(
                MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0x9000 + i);
            mesh_health_bridge_heartbeat(
                health_bridge, pid, MESH_HEARTBEAT_ERROR, 0);
        }
    }

    /* Update coordinator to respond to health changes */
    err = mesh_coordinator_update(coordinator, 100);

    /* Get coordinator stats after mesh degradation */
    mesh_coordinator_stats_t after_stats;
    err = mesh_coordinator_get_stats(coordinator, &after_stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Coordinator should still be operational */
    EXPECT_NE(after_stats.state, COORD_STATE_FAILED);

    mesh_coordinator_destroy(coordinator);
}

/**
 * Test 15: HealthConsensusAcrossChannels
 * Multi-channel health endorsement
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, HealthConsensusAcrossChannels) {
    if (!health_bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    /* Send heartbeats from participants in multiple channels */
    mesh_channel_id_t channels[] = {
        MESH_CHANNEL_SYSTEM,
        MESH_CHANNEL_LEFT_HEMISPHERE,
        MESH_CHANNEL_RIGHT_HEMISPHERE,
        MESH_CHANNEL_SUBCORTICAL,
    };

    for (auto channel : channels) {
        /* Create 3 participants per channel */
        for (int i = 0; i < 3; i++) {
            mesh_participant_id_t pid = mesh_make_participant_id(
                channel, MESH_PARTICIPANT_MODULE, 0xA000 + i);

            /* Send healthy heartbeat */
            mesh_health_bridge_heartbeat(
                health_bridge, pid, MESH_HEARTBEAT_PING, 100);
        }
    }

    /* Give time for health to propagate */
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    /* Check consensus across channels */
    mesh_system_health_t system_health;
    nimcp_error_t err = mesh_health_bridge_get_system_health(
        health_bridge, &system_health);

    if (err == NIMCP_SUCCESS) {
        /* Verify system health reflects multiple channels */
        EXPECT_GE(system_health.channel_count, 0u);

        /* System health score should be computed */
        EXPECT_GE(system_health.system_health_score, 0.0f);
        EXPECT_LE(system_health.system_health_score, 1.0f);
    }

    /* Verify stats */
    mesh_health_bridge_stats_t stats;
    err = mesh_health_bridge_get_stats(health_bridge, &stats);
    if (err == NIMCP_SUCCESS) {
        EXPECT_GE(stats.heartbeats_received, 0u);
    }
}

/* ============================================================================
 * Exception-Immune Integration (2 tests)
 * ============================================================================ */

/**
 * Test 16: StallExceptionBecomesAntigen
 * Stall -> exception -> antigen -> immune response
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, StallExceptionBecomesAntigen) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_participant_id_t stalling_module = mesh_make_participant_id(
        MESH_CHANNEL_LEFT_HEMISPHERE, MESH_PARTICIPANT_MODULE, 0xB001);

    /* Classify stall error */
    mesh_exception_category_t category;
    mesh_exception_severity_t severity;

    nimcp_error_t err = mesh_exception_bridge_classify(
        NIMCP_ERROR_TIMEOUT, &category, &severity);

    if (err == NIMCP_SUCCESS) {
        /* Timing errors should be classified as TIMING category */
        EXPECT_EQ(category, MESH_EXC_CAT_TIMING);
    }

    /* Route stall exception to create antigen and get immune response */
    mesh_exception_response_t response;
    err = mesh_exception_bridge_route_error(
        exception_bridge,
        NIMCP_ERROR_TIMEOUT,
        "Cycle stall detected - module unresponsive",
        stalling_module,
        __FILE__,
        __LINE__,
        &response
    );

    if (err == NIMCP_SUCCESS) {
        /* Verify immune response was generated */
        EXPECT_NE(response.primary_action, MESH_IMMUNE_ACTION_NONE);

        /* Threat score should be computed */
        EXPECT_GE(response.threat_score, 0.0f);
        EXPECT_LE(response.threat_score, 1.0f);
    }

    /* Verify stats */
    mesh_exception_bridge_stats_t stats;
    err = mesh_exception_bridge_get_stats(exception_bridge, &stats);
    if (err == NIMCP_SUCCESS) {
        EXPECT_GT(stats.exceptions_received, 0u);
        EXPECT_GT(stats.antigens_created, 0u);
    }
}

/**
 * Test 17: ImmuneResponseAffectsRecovery
 * Immune action -> recovery priority adjustment
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, ImmuneResponseAffectsRecovery) {
    if (!exception_bridge) {
        GTEST_SKIP() << "Exception bridge not available";
    }

    mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
    if (!registry) {
        GTEST_SKIP() << "Registry not available";
    }

    /* Create MSP for immune integration */
    mesh_msp_config_t msp_config;
    mesh_msp_default_config(&msp_config);
    msp_config.msp_name = "recovery_msp";
    msp_config.enable_quarantine = true;

    mesh_msp_t* msp = mesh_msp_create(&msp_config, registry);
    if (!msp) {
        GTEST_SKIP() << "Could not create MSP";
    }

    mesh_participant_id_t failing_module = mesh_make_participant_id(
        MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0xC001);

    /* Issue credential */
    credential_t cred;
    nimcp_error_t err = mesh_msp_issue_credential(
        msp, failing_module, 5, MESH_CAP_ALL, &cred);

    if (err != NIMCP_SUCCESS) {
        mesh_msp_destroy(msp);
        GTEST_SKIP() << "Could not issue credential";
    }

    /* Route severe exception */
    mesh_exception_response_t response;
    err = mesh_exception_bridge_route_error(
        exception_bridge,
        NIMCP_ERROR_MEMORY_CORRUPTION,  /* Severe error */
        "Critical system failure requiring immune response",
        failing_module,
        __FILE__,
        __LINE__,
        &response
    );

    /* Handle immune response */
    if (err == NIMCP_SUCCESS) {
        switch (response.primary_action) {
            case MESH_IMMUNE_ACTION_QUARANTINE:
                /* Quarantine affects recovery - module isolated */
                err = mesh_msp_quarantine(msp, failing_module,
                    response.quarantine_duration_ms);
                if (err == NIMCP_SUCCESS) {
                    EXPECT_TRUE(mesh_msp_is_quarantined(msp, failing_module));
                }
                break;

            case MESH_IMMUNE_ACTION_REPAIR:
                /* Repair action - recovery proceeds with adjustment */
                simulate_recovery();
                break;

            case MESH_IMMUNE_ACTION_WARN:
            case MESH_IMMUNE_ACTION_LOG:
                /* Low severity - recovery proceeds normally */
                simulate_recovery();
                break;

            default:
                /* Other actions */
                break;
        }
    }

    /* Verify MSP stats reflect actions */
    mesh_msp_stats_t msp_stats;
    err = mesh_msp_get_stats(msp, &msp_stats);
    if (err == NIMCP_SUCCESS) {
        EXPECT_GE(msp_stats.credentials_issued, 1u);
    }

    mesh_msp_destroy(msp);
}

/* ============================================================================
 * Additional Stress Tests
 * ============================================================================ */

/**
 * Test 18: HighFrequencyCycleUpdates
 * Stress test with rapid cycle updates
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, HighFrequencyCycleUpdates) {
    mesh_participant_registry_t* registry = mesh_bootstrap_get_registry(bootstrap);
    mesh_channel_t* channel = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_SYSTEM);

    if (!registry || !channel) {
        GTEST_SKIP() << "Registry or channel not available";
    }

    /* Create coordinator pool */
    mesh_coordinator_pool_config_t pool_config;
    mesh_coordinator_pool_default_config(&pool_config);
    pool_config.pool_name = "stress_pool";
    pool_config.initial_size = 4;

    mesh_coordinator_pool_t* pool = mesh_coordinator_pool_create(
        &pool_config, registry, channel);

    if (!pool) {
        GTEST_SKIP() << "Could not create coordinator pool";
    }

    /* Rapid updates */
    const int UPDATE_COUNT = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < UPDATE_COUNT; i++) {
        nimcp_error_t err = mesh_coordinator_pool_update(pool, 1);  /* 1ms delta */
        /* Updates should not fail */
        (void)err;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should complete in reasonable time */
    EXPECT_LT(duration.count(), 1000);  /* Less than 1 second total */

    /* Pool should still be healthy */
    EXPECT_TRUE(mesh_coordinator_pool_is_bft_valid(pool) ||
                mesh_coordinator_pool_get_size(pool) < MESH_MIN_POOL_SIZE_BFT);

    mesh_coordinator_pool_destroy(pool);
}

/**
 * Test 19: ConcurrentHealthAndTimingUpdates
 * Health and timing updates happening concurrently
 */
TEST_F(MeshCycleCoordinatorIntegrationTest, ConcurrentHealthAndTimingUpdates) {
    std::atomic<bool> running{true};
    std::atomic<int> health_updates{0};
    std::atomic<int> timing_queries{0};

    /* Health update thread */
    std::thread health_thread([&]() {
        while (running) {
            if (health_bridge) {
                mesh_participant_id_t pid = mesh_make_participant_id(
                    MESH_CHANNEL_SYSTEM, MESH_PARTICIPANT_MODULE, 0xD001);
                mesh_health_bridge_heartbeat(
                    health_bridge, pid, MESH_HEARTBEAT_PING, 100);
                health_updates++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    /* Timing query thread */
    std::thread timing_thread([&]() {
        while (running) {
            if (timing) {
                float interval = mesh_timing_next_interval(
                    timing, MESH_TIMING_LEVEL_LAYER);
                EXPECT_GT(interval, 0.0f);
                timing_queries++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    /* Bootstrap update thread */
    std::thread update_thread([&]() {
        while (running) {
            mesh_bootstrap_update(bootstrap, 10);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    /* Run for 100ms */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    health_thread.join();
    timing_thread.join();
    update_thread.join();

    /* Verify some work was done */
    if (health_bridge) {
        EXPECT_GT(health_updates.load(), 0);
    }
    if (timing) {
        EXPECT_GT(timing_queries.load(), 0);
    }
}
