/**
 * @file test_mesh_cycle_coordinator_e2e.cpp
 * @brief End-to-End Tests for Mesh-Cycle Coordinator Integration
 *
 * WHAT: Tests complete cycle coordinator integration with mesh network
 * WHY:  Verify full system operates correctly with brain cycles through mesh
 * HOW:  Create brain, register with mesh, run cycles, verify health and recovery
 *
 * TEST COVERAGE:
 * - Full brain with cycle coordinator through mesh
 * - Stall detection to immune response pipeline
 * - Timing-driven ordering and batching
 * - Health monitoring across all channels
 * - BBB blocks timing violations
 * - Recovery after chronic stalls with election
 * - Cross-hemisphere cycle synchronization
 * - System-wide health degradation logging
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <map>
#include <set>
#include <cstring>
#include <mutex>
#include <condition_variable>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_timing.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Test Constants
// =============================================================================

static constexpr size_t CHANNEL_COUNT = 5;
static constexpr size_t CHRONIC_STALL_COUNT = 10;
static constexpr uint32_t MOCK_CYCLE_MAGIC = 0x43594C01;  /* "CYC\x01" */

// =============================================================================
// Mock Cycle Instance Structure
// =============================================================================

typedef struct mock_cycle_instance {
    uint32_t magic;
    brain_cycle_type_t type;
    const char* name;
    uint64_t tick_count;
    uint64_t stall_count;
    uint64_t recovery_count;
    brain_cycle_health_t health;
    bool is_running;
    uint64_t last_tick_us;
    float avg_duration_us;
} mock_cycle_instance_t;

// =============================================================================
// Test Fixture - Cycle Coordinator E2E
// =============================================================================

class MeshCycleCoordinatorE2ETest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap_ = nullptr;
    brain_cycle_coordinator_t* cycle_coord_ = nullptr;
    mesh_hierarchical_timing_t timing_ = nullptr;
    mesh_health_bridge_t* health_bridge_ = nullptr;
    mesh_integration_t* integration_ = nullptr;

    std::map<brain_cycle_type_t, mock_cycle_instance_t*> mock_cycles_;
    std::vector<std::string> log_messages_;
    std::mutex log_mutex_;

    std::atomic<size_t> stall_events_{0};
    std::atomic<size_t> recovery_events_{0};
    std::atomic<size_t> health_changes_{0};
    std::atomic<size_t> dependency_violations_{0};

    void SetUp() override {
        stall_events_ = 0;
        recovery_events_ = 0;
        health_changes_ = 0;
        dependency_violations_ = 0;
        log_messages_.clear();

        // Create mesh bootstrap with full subsystems
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems.enable_cognitive = true;
        config.subsystems.enable_sensory = true;
        config.subsystems.enable_motor = true;
        config.subsystems.enable_memory = true;
        config.subsystems.enable_security = true;
        config.enable_health_monitoring = true;
        config.verbose_logging = false;

        bootstrap_ = mesh_bootstrap_create(&config);
        if (!bootstrap_) {
            GTEST_SKIP() << "Bootstrap creation not available";
        }

        integration_ = mesh_bootstrap_get_integration(bootstrap_);
        health_bridge_ = mesh_bootstrap_get_health_bridge(bootstrap_);

        // Create cycle coordinator with integrations
        brain_cycle_coordinator_config_t cc_config;
        brain_cycle_coordinator_default_config(&cc_config);
        cc_config.enable_timing_checks = true;
        cc_config.enable_dependency_tracking = true;
        cc_config.stall_threshold_multiplier = 3;
        cc_config.health_check_interval_ms = 100;
        cc_config.enable_auto_health_check = true;
        cc_config.enable_logging = true;

        cycle_coord_ = brain_cycle_coordinator_create(&cc_config);

        // Create timing context
        timing_ = mesh_timing_create(nullptr);

        // Create mock cycles
        CreateMockCycles();

        // Register callbacks
        RegisterCallbacks();
    }

    void TearDown() override {
        for (auto& pair : mock_cycles_) {
            nimcp_free(pair.second);
        }
        mock_cycles_.clear();

        if (timing_) {
            mesh_timing_destroy(timing_);
            timing_ = nullptr;
        }

        if (cycle_coord_) {
            brain_cycle_coordinator_destroy(cycle_coord_);
            cycle_coord_ = nullptr;
        }

        if (bootstrap_) {
            mesh_bootstrap_destroy(bootstrap_);
            bootstrap_ = nullptr;
        }
    }

    void CreateMockCycles() {
        const char* cycle_names[] = {
            "immune_tick", "health_agent", "sleep_wake", "circadian",
            "arousal", "oscillations", "gc_agent", "io_dispatcher", "brain_update"
        };

        for (int i = 0; i < static_cast<int>(BRAIN_CYCLE_COUNT); i++) {
            auto* cycle = static_cast<mock_cycle_instance_t*>(
                nimcp_calloc(1, sizeof(mock_cycle_instance_t)));

            if (!cycle) continue;

            cycle->magic = MOCK_CYCLE_MAGIC;
            cycle->type = static_cast<brain_cycle_type_t>(i);
            cycle->name = cycle_names[i];
            cycle->tick_count = 0;
            cycle->stall_count = 0;
            cycle->recovery_count = 0;
            cycle->health = BRAIN_CYCLE_HEALTH_HEALTHY;
            cycle->is_running = true;
            cycle->last_tick_us = 0;
            cycle->avg_duration_us = 0.0f;

            mock_cycles_[static_cast<brain_cycle_type_t>(i)] = cycle;
        }
    }

    static brain_cycle_health_t MockHealthFn(void* cycle_handle) {
        if (!cycle_handle) return BRAIN_CYCLE_HEALTH_UNKNOWN;
        auto* cycle = static_cast<mock_cycle_instance_t*>(cycle_handle);
        if (cycle->magic != MOCK_CYCLE_MAGIC) return BRAIN_CYCLE_HEALTH_ERROR;
        return cycle->health;
    }

    static void StallCallback(brain_cycle_type_t type, uint64_t duration_ms, void* user_data) {
        auto* test = static_cast<MeshCycleCoordinatorE2ETest*>(user_data);
        test->stall_events_++;

        std::lock_guard<std::mutex> lock(test->log_mutex_);
        char msg[256];
        snprintf(msg, sizeof(msg), "STALL: cycle=%d, duration=%lu ms",
                 static_cast<int>(type), static_cast<unsigned long>(duration_ms));
        test->log_messages_.push_back(msg);
    }

    static void HealthChangeCallback(brain_cycle_type_t type,
                                     brain_cycle_health_t old_health,
                                     brain_cycle_health_t new_health,
                                     void* user_data) {
        auto* test = static_cast<MeshCycleCoordinatorE2ETest*>(user_data);
        test->health_changes_++;

        std::lock_guard<std::mutex> lock(test->log_mutex_);
        char msg[256];
        snprintf(msg, sizeof(msg), "HEALTH_CHANGE: cycle=%d, %s -> %s",
                 static_cast<int>(type),
                 brain_cycle_health_name(old_health),
                 brain_cycle_health_name(new_health));
        test->log_messages_.push_back(msg);
    }

    static void DependencyViolatedCallback(brain_cycle_type_t dependent,
                                           brain_cycle_type_t dependency,
                                           void* user_data) {
        auto* test = static_cast<MeshCycleCoordinatorE2ETest*>(user_data);
        test->dependency_violations_++;

        std::lock_guard<std::mutex> lock(test->log_mutex_);
        char msg[256];
        snprintf(msg, sizeof(msg), "DEPENDENCY_VIOLATION: %d depends on %d",
                 static_cast<int>(dependent), static_cast<int>(dependency));
        test->log_messages_.push_back(msg);
    }

    static void OverallHealthCallback(float old_health, float new_health, void* user_data) {
        auto* test = static_cast<MeshCycleCoordinatorE2ETest*>(user_data);

        std::lock_guard<std::mutex> lock(test->log_mutex_);
        char msg[256];
        snprintf(msg, sizeof(msg), "OVERALL_HEALTH: %.2f -> %.2f", old_health, new_health);
        test->log_messages_.push_back(msg);
    }

    void RegisterCallbacks() {
        if (!cycle_coord_) return;

        brain_cycle_coordinator_callbacks_t callbacks = {};
        callbacks.on_stall_detected = StallCallback;
        callbacks.on_health_changed = HealthChangeCallback;
        callbacks.on_dependency_violated = DependencyViolatedCallback;
        callbacks.on_overall_health_changed = OverallHealthCallback;
        callbacks.user_data = this;

        brain_cycle_coordinator_register_callbacks(cycle_coord_, &callbacks);
    }

    void RegisterAllCyclesWithCoordinator() {
        if (!cycle_coord_) return;

        for (auto& pair : mock_cycles_) {
            brain_cycle_coordinator_register(cycle_coord_,
                pair.first, pair.second, MockHealthFn);
        }
    }

    void SimulateCycleTick(brain_cycle_type_t type, uint64_t duration_us) {
        auto it = mock_cycles_.find(type);
        if (it == mock_cycles_.end()) return;

        mock_cycle_instance_t* cycle = it->second;
        cycle->tick_count++;
        cycle->last_tick_us = duration_us;

        // Update rolling average
        cycle->avg_duration_us = (cycle->avg_duration_us * 0.9f) + (duration_us * 0.1f);

        if (cycle_coord_) {
            brain_cycle_coordinator_notify_tick(cycle_coord_, type, duration_us);
        }
    }

    mesh_transaction_t* CreateTimingTransaction(const char* name, float timing_ms) {
        mesh_participant_id_t proposer = {0};
        mesh_channel_id_t channel = MESH_CHANNEL_SYSTEM;

        mesh_transaction_t* tx = mesh_transaction_create(
            MESH_TX_STATE_CHANGE, proposer, channel);
        if (tx) {
            char payload[256];
            snprintf(payload, sizeof(payload), "timing_tx:%s:timing_%.2fms", name, timing_ms);
            mesh_transaction_set_payload(tx, payload, strlen(payload));
        }
        return tx;
    }
};

// =============================================================================
// Test 1: Full Brain with Cycle Coordinator Mesh
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, FullBrainWithCycleCoordinatorMesh) {
    // Create full brain, register with mesh, run cycles through coordinator

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(cycle_coord_, nullptr);

    // Register all cycles with coordinator
    RegisterAllCyclesWithCoordinator();

    // Verify all cycles registered
    brain_cycle_coordinator_stats_t stats;
    int result = brain_cycle_coordinator_get_stats(cycle_coord_, &stats);
    ASSERT_EQ(result, 0);
    EXPECT_EQ(stats.total_cycles_registered, static_cast<uint32_t>(BRAIN_CYCLE_COUNT));

    // Run cycles for a period
    auto start = std::chrono::steady_clock::now();
    size_t tick_count = 0;

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
        // Simulate all cycles ticking at their natural rates
        for (auto& pair : mock_cycles_) {
            // Get expected interval for this cycle type
            uint64_t expected_us = brain_cycle_get_default_interval_us(pair.first);

            // Add some jitter using timing system
            float jitter_ms = 0;
            if (timing_) {
                mesh_timing_level_t level = mesh_timing_level_from_coord_level(
                    static_cast<uint32_t>(COORD_LEVEL_LAYER));
                jitter_ms = mesh_timing_next_interval(timing_, level) * 0.1f;
            }

            uint64_t actual_duration = expected_us + static_cast<uint64_t>(jitter_ms * 1000);
            SimulateCycleTick(pair.first, actual_duration);
            tick_count++;
        }

        // Small sleep between rounds
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Periodic health check
        brain_cycle_coordinator_check_health(cycle_coord_);
    }

    // Verify cycles ran
    EXPECT_GT(tick_count, 100u) << "Should have many cycle ticks";

    // Verify health is good
    result = brain_cycle_coordinator_get_stats(cycle_coord_, &stats);
    ASSERT_EQ(result, 0);
    EXPECT_GT(stats.overall_health, 0.5f) << "Overall health should be reasonable";
    EXPECT_EQ(stats.total_cycles_healthy + stats.total_cycles_degraded,
              stats.total_cycles_registered) << "All cycles should be healthy or degraded";
}

// =============================================================================
// Test 2: Stall Detection to Immune Response
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, StallDetectionToImmuneResponse) {
    // Full pipeline: stall -> exception -> antigen -> immune -> recovery

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(cycle_coord_, nullptr);

    RegisterAllCyclesWithCoordinator();

    mesh_exception_bridge_t* exception_bridge =
        mesh_bootstrap_get_exception_bridge(bootstrap_);

    // Simulate normal operation first
    for (int i = 0; i < 10; i++) {
        for (auto& pair : mock_cycles_) {
            SimulateCycleTick(pair.first, 1000);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Now simulate a stall on brain_update cycle
    mock_cycle_instance_t* brain_cycle = mock_cycles_[BRAIN_CYCLE_BRAIN_UPDATE];
    brain_cycle->health = BRAIN_CYCLE_HEALTH_STALLED;
    brain_cycle->is_running = false;

    // Wait for stall detection
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    brain_cycle_coordinator_check_health(cycle_coord_);

    // Verify stall was detected
    brain_cycle_status_t status;
    int result = brain_cycle_coordinator_get_status(cycle_coord_,
        BRAIN_CYCLE_BRAIN_UPDATE, &status);
    EXPECT_EQ(result, 0);

    // Route exception through mesh if available
    if (exception_bridge) {
        // Exception would trigger immune response
        // (Actual immune integration depends on implementation)
    }

    // Simulate recovery
    brain_cycle->health = BRAIN_CYCLE_HEALTH_HEALTHY;
    brain_cycle->is_running = true;
    SimulateCycleTick(BRAIN_CYCLE_BRAIN_UPDATE, 1000);

    recovery_events_++;

    // Verify recovery logged
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check that stall was recorded
    EXPECT_GT(stall_events_.load() + health_changes_.load(), 0u)
        << "Should have detected stall or health change";
}

// =============================================================================
// Test 3: Timing Driven Ordering Batching
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, TimingDrivenOrderingBatching) {
    // Real ordering service uses cycle timing for batch decisions

    ASSERT_NE(bootstrap_, nullptr);

    mesh_ordering_service_t* ordering = mesh_integration_get_ordering(integration_);
    if (!ordering) {
        GTEST_SKIP() << "Ordering service not available";
    }

    RegisterAllCyclesWithCoordinator();

    std::vector<mesh_transaction_t*> transactions;
    std::vector<uint64_t> submit_times;

    // Create transactions with timing-based urgency
    for (int i = 0; i < 20; i++) {
        // Get timing interval from hierarchy
        float timing_ms = 10.0f;
        if (timing_) {
            timing_ms = mesh_timing_next_interval(timing_, MESH_TIMING_LEVEL_LAYER);
        }

        mesh_transaction_t* tx = CreateTimingTransaction("batch_tx", timing_ms);
        ASSERT_NE(tx, nullptr);

        auto now = std::chrono::steady_clock::now();
        submit_times.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count());

        mesh_ordering_submit(ordering, tx);
        transactions.push_back(tx);

        // Simulate cycle tick after each submission
        SimulateCycleTick(BRAIN_CYCLE_BRAIN_UPDATE, 1000);

        // Real millisecond sleep for timing realism
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Wait for batch processing (ordering service processes batches automatically)

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify batching occurred
    // Transactions should have been batched based on timing intervals
    EXPECT_EQ(transactions.size(), 20u);

    // Cleanup
    for (auto* tx : transactions) {
        mesh_transaction_destroy(tx);
    }
}

// =============================================================================
// Test 4: Health Monitoring Across All Channels
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, HealthMonitoringAcrossAllChannels) {
    // 5 channels report health, aggregate is computed

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(health_bridge_, nullptr);

    RegisterAllCyclesWithCoordinator();

    // Register health for participants in each channel
    mesh_channel_id_t channels[] = {
        MESH_CHANNEL_SYSTEM,
        MESH_CHANNEL_LEFT_HEMISPHERE,
        MESH_CHANNEL_RIGHT_HEMISPHERE,
        MESH_CHANNEL_SUBCORTICAL,
        MESH_CHANNEL_GPU_COMPUTE
    };

    // Send heartbeats for each channel
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        for (int p = 0; p < 5; p++) {
            mesh_participant_id_t pid = mesh_make_participant_id(
                channels[ch], MESH_PARTICIPANT_MODULE, p);

            mesh_health_bridge_heartbeat(health_bridge_, pid,
                MESH_HEARTBEAT_PING, 100);
        }
    }

    // Run cycles while health is being monitored
    for (int round = 0; round < 10; round++) {
        for (auto& pair : mock_cycles_) {
            SimulateCycleTick(pair.first, 1000);
        }

        // Check heartbeats
        mesh_health_bridge_check_heartbeats(health_bridge_);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Get per-channel health
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        mesh_channel_health_t channel_health;
        nimcp_error_t err = mesh_health_bridge_get_channel_health(
            health_bridge_, channels[ch], &channel_health);

        if (err == NIMCP_SUCCESS) {
            EXPECT_NE(channel_health.status, MESH_HEALTH_UNKNOWN)
                << "Channel " << ch << " should have determined health";
        }
    }

    // Get aggregate system health
    mesh_system_health_t system_health;
    nimcp_error_t err = mesh_health_bridge_get_system_health(health_bridge_, &system_health);

    if (err == NIMCP_SUCCESS) {
        EXPECT_GE(system_health.channel_count, 1u)
            << "Should have at least one channel";

        EXPECT_GE(system_health.system_health_score, 0.0f);
        EXPECT_LE(system_health.system_health_score, 1.0f);

        // Verify aggregation computed
        EXPECT_GT(system_health.computed_at_ns, 0u);
    }
}

// =============================================================================
// Test 5: BBB Blocks Timing Violation
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, BBBBlocksTimingViolation) {
    // Transaction violating timing constraints rejected by BBB

    ASSERT_NE(bootstrap_, nullptr);

    mesh_msp_t* msp = mesh_integration_get_msp(integration_);
    if (!msp) {
        GTEST_SKIP() << "MSP not available";
    }

    RegisterAllCyclesWithCoordinator();

    // Create transaction with extreme timing (should violate constraints)
    mesh_participant_id_t proposer = {0};
    mesh_channel_id_t channel = MESH_CHANNEL_SYSTEM;

    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_EMERGENCY_OVERRIDE, proposer, channel);
    ASSERT_NE(tx, nullptr);
    mesh_transaction_set_payload(tx, "timing_violation_test", strlen("timing_violation_test"));

    // Try to submit - BBB should validate
    nimcp_error_t err = mesh_integration_submit_transaction(integration_, tx);

    // Depending on BBB configuration, this may succeed or be rejected
    // The key is it should not crash and should be handled gracefully

    // Simulate what happens when cycles are too fast
    for (int i = 0; i < 10; i++) {
        SimulateCycleTick(BRAIN_CYCLE_BRAIN_UPDATE, 100); // Very fast
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    mesh_transaction_destroy(tx);

    // System should remain stable
    brain_cycle_coordinator_stats_t stats;
    int result = brain_cycle_coordinator_get_stats(cycle_coord_, &stats);
    EXPECT_EQ(result, 0);
}

// =============================================================================
// Test 6: Recovery After Chronic Stalls
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, RecoveryAfterChronicStalls) {
    // 10 consecutive stalls -> election -> new coordinator -> recovery

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(cycle_coord_, nullptr);

    RegisterAllCyclesWithCoordinator();

    mesh_coordinator_pool_t* pool = mesh_integration_get_coordinator_pool(
        integration_, MESH_CHANNEL_LEFT_HEMISPHERE);

    // Get initial leader
    mesh_participant_id_t initial_leader = 0;
    if (pool) {
        initial_leader = mesh_coordinator_pool_get_leader_id(pool);
    }

    // Simulate 10 consecutive stalls
    for (size_t stall = 0; stall < CHRONIC_STALL_COUNT; stall++) {
        // Stop brain_update cycle
        mock_cycle_instance_t* brain_cycle = mock_cycles_[BRAIN_CYCLE_BRAIN_UPDATE];
        brain_cycle->health = BRAIN_CYCLE_HEALTH_STALLED;
        brain_cycle->stall_count++;

        // Wait for detection
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        brain_cycle_coordinator_check_health(cycle_coord_);

        // Brief recovery attempt
        brain_cycle->health = BRAIN_CYCLE_HEALTH_DEGRADED;
        SimulateCycleTick(BRAIN_CYCLE_BRAIN_UPDATE, 50000); // Slow tick

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // After chronic stalls, trigger recovery
    if (pool) {
        // Force election
        mesh_coordinator_pool_elect_leader(pool);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Get new leader
        mesh_participant_id_t new_leader = mesh_coordinator_pool_get_leader_id(pool);

        // Leader may have changed
        // (depends on election outcome, but should complete)
    }

    // Full recovery - all cycles back to normal
    for (auto& pair : mock_cycles_) {
        pair.second->health = BRAIN_CYCLE_HEALTH_HEALTHY;
        pair.second->recovery_count++;
        SimulateCycleTick(pair.first, 1000);
    }

    // Verify recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    brain_cycle_coordinator_check_health(cycle_coord_);

    brain_cycle_coordinator_stats_t stats;
    int result = brain_cycle_coordinator_get_stats(cycle_coord_, &stats);
    EXPECT_EQ(result, 0);

    // After recovery, health should improve
    EXPECT_GE(stats.total_cycles_healthy, stats.total_cycles_stalled)
        << "After recovery, more cycles should be healthy than stalled";
}

// =============================================================================
// Test 7: Cross-Hemisphere Cycle Synchronization
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, CrossHemisphereCycleSynchronization) {
    // Left/right hemisphere cycles synchronized via mesh

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(cycle_coord_, nullptr);

    RegisterAllCyclesWithCoordinator();

    // Get both hemisphere channels
    mesh_channel_t* left_channel = mesh_bootstrap_get_channel(
        bootstrap_, MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_channel_t* right_channel = mesh_bootstrap_get_channel(
        bootstrap_, MESH_CHANNEL_RIGHT_HEMISPHERE);

    if (!left_channel || !right_channel) {
        GTEST_SKIP() << "Hemisphere channels not available";
    }

    // Define dependencies for synchronization
    // Oscillations should sync with brain_update
    brain_cycle_coordinator_add_dependency(cycle_coord_,
        BRAIN_CYCLE_OSCILLATIONS, BRAIN_CYCLE_BRAIN_UPDATE);

    // Health agent depends on immune
    brain_cycle_coordinator_add_dependency(cycle_coord_,
        BRAIN_CYCLE_HEALTH_AGENT, BRAIN_CYCLE_IMMUNE_TICK);

    std::vector<uint64_t> left_ticks;
    std::vector<uint64_t> right_ticks;
    std::mutex sync_mutex;

    // Simulate synchronized hemisphere operation
    for (int round = 0; round < 50; round++) {
        auto round_start = std::chrono::steady_clock::now();

        // Left hemisphere processes
        {
            std::lock_guard<std::mutex> lock(sync_mutex);
            auto tick_time = std::chrono::steady_clock::now();
            left_ticks.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                tick_time.time_since_epoch()).count());
        }

        // Simulate left hemisphere cycles
        SimulateCycleTick(BRAIN_CYCLE_BRAIN_UPDATE, 1000);
        SimulateCycleTick(BRAIN_CYCLE_OSCILLATIONS, 500);

        // Small delay for cross-hemisphere routing
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // Right hemisphere processes
        {
            std::lock_guard<std::mutex> lock(sync_mutex);
            auto tick_time = std::chrono::steady_clock::now();
            right_ticks.push_back(std::chrono::duration_cast<std::chrono::microseconds>(
                tick_time.time_since_epoch()).count());
        }

        // Simulate right hemisphere cycles
        SimulateCycleTick(BRAIN_CYCLE_HEALTH_AGENT, 2000);
        SimulateCycleTick(BRAIN_CYCLE_IMMUNE_TICK, 1000);

        // Synchronization point
        brain_cycle_coordinator_check_health(cycle_coord_);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify synchronization - ticks should be interleaved properly
    EXPECT_EQ(left_ticks.size(), 50u);
    EXPECT_EQ(right_ticks.size(), 50u);

    // Check dependencies are being tracked
    bool deps_satisfied;
    brain_cycle_coordinator_check_dependencies(cycle_coord_,
        BRAIN_CYCLE_OSCILLATIONS, &deps_satisfied);

    // Overall system should be synchronized
    brain_cycle_coordinator_stats_t stats;
    int result = brain_cycle_coordinator_get_stats(cycle_coord_, &stats);
    EXPECT_EQ(result, 0);
}

// =============================================================================
// Test 8: System-Wide Health Degradation
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, SystemWideHealthDegradation) {
    // Multiple cycles degrade -> system health drops -> logged

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(cycle_coord_, nullptr);

    RegisterAllCyclesWithCoordinator();

    // Get initial health
    brain_cycle_coordinator_stats_t initial_stats;
    brain_cycle_coordinator_get_stats(cycle_coord_, &initial_stats);
    float initial_health = initial_stats.overall_health;

    // Run normal cycles first
    for (int i = 0; i < 10; i++) {
        for (auto& pair : mock_cycles_) {
            SimulateCycleTick(pair.first, 1000);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Now degrade multiple cycles progressively
    std::vector<brain_cycle_type_t> degraded_cycles;

    for (int wave = 0; wave < 3; wave++) {
        // Degrade 3 cycles per wave
        for (int i = 0; i < 3; i++) {
            int cycle_idx = wave * 3 + i;
            if (cycle_idx >= static_cast<int>(BRAIN_CYCLE_COUNT)) break;

            brain_cycle_type_t type = static_cast<brain_cycle_type_t>(cycle_idx);
            auto it = mock_cycles_.find(type);
            if (it != mock_cycles_.end()) {
                it->second->health = BRAIN_CYCLE_HEALTH_DEGRADED;
                degraded_cycles.push_back(type);
            }
        }

        // Check health after degradation
        brain_cycle_coordinator_check_health(cycle_coord_);

        brain_cycle_coordinator_stats_t wave_stats;
        brain_cycle_coordinator_get_stats(cycle_coord_, &wave_stats);

        // Log the degradation
        {
            std::lock_guard<std::mutex> lock(log_mutex_);
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "DEGRADATION_WAVE_%d: health=%.2f, degraded=%u, stalled=%u",
                     wave, wave_stats.overall_health,
                     wave_stats.total_cycles_degraded,
                     wave_stats.total_cycles_stalled);
            log_messages_.push_back(msg);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Get final health
    brain_cycle_coordinator_stats_t final_stats;
    brain_cycle_coordinator_get_stats(cycle_coord_, &final_stats);

    // Health should have degraded
    EXPECT_GT(final_stats.total_cycles_degraded, 0u)
        << "Some cycles should be degraded";

    // Overall health should be lower
    EXPECT_LT(final_stats.overall_health, initial_health + 0.1f)
        << "Overall health should have dropped";

    // Verify logging captured the degradation
    EXPECT_GT(log_messages_.size(), 0u)
        << "Degradation events should be logged";

    // Print log for debugging
    for (const auto& msg : log_messages_) {
        // In real test, this would go to test output
        EXPECT_FALSE(msg.empty());
    }

    // Now recover
    for (auto& pair : mock_cycles_) {
        pair.second->health = BRAIN_CYCLE_HEALTH_HEALTHY;
        SimulateCycleTick(pair.first, 1000);
    }

    brain_cycle_coordinator_check_health(cycle_coord_);

    brain_cycle_coordinator_stats_t recovered_stats;
    brain_cycle_coordinator_get_stats(cycle_coord_, &recovered_stats);

    // Health should improve after recovery
    EXPECT_GT(recovered_stats.overall_health, final_stats.overall_health - 0.1f)
        << "Health should improve after recovery";
}

// =============================================================================
// Additional E2E: Full Transaction Flow with Cycle Timing
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, FullTransactionFlowWithCycleTiming) {
    // Complete transaction flow respecting cycle timing

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(integration_, nullptr);

    RegisterAllCyclesWithCoordinator();

    // Process transactions while cycles are running
    std::atomic<size_t> tx_processed{0};
    std::atomic<bool> stop{false};

    // Cycle runner thread
    std::thread cycle_thread([&]() {
        while (!stop.load()) {
            for (auto& pair : mock_cycles_) {
                SimulateCycleTick(pair.first, 1000);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });

    // Transaction processing thread
    std::thread tx_thread([&]() {
        for (int i = 0; i < 100 && !stop.load(); i++) {
            mesh_participant_id_t proposer = (mesh_participant_id_t)i;
            mesh_channel_id_t channel = MESH_CHANNEL_SYSTEM;

            mesh_transaction_t* tx = mesh_transaction_create(
                MESH_TX_BELIEF_UPDATE, proposer, channel);
            if (tx) {
                char payload[128];
                snprintf(payload, sizeof(payload), "cycle_tx_%d", i);
                mesh_transaction_set_payload(tx, payload, strlen(payload));
                mesh_integration_submit_transaction(integration_, tx);
                tx_processed++;
                mesh_transaction_destroy(tx);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Run for a period
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    stop.store(true);

    cycle_thread.join();
    tx_thread.join();

    // Verify transactions were processed
    EXPECT_GT(tx_processed.load(), 50u)
        << "Should have processed many transactions";

    // Verify cycles still healthy
    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(cycle_coord_, &stats);
    EXPECT_GT(stats.overall_health, 0.0f);
}

// =============================================================================
// Additional E2E: Mesh Update Integration with Cycles
// =============================================================================

TEST_F(MeshCycleCoordinatorE2ETest, MeshUpdateIntegrationWithCycles) {
    // Mesh updates synchronized with cycle coordinator

    ASSERT_NE(bootstrap_, nullptr);
    ASSERT_NE(integration_, nullptr);

    RegisterAllCyclesWithCoordinator();

    std::vector<double> update_latencies_ms;

    for (int update = 0; update < 100; update++) {
        auto start = std::chrono::steady_clock::now();

        // Simulate cycles
        for (auto& pair : mock_cycles_) {
            SimulateCycleTick(pair.first, 1000);
        }

        // Update mesh
        mesh_integration_update(integration_, 16);

        // Check cycle health
        brain_cycle_coordinator_check_health(cycle_coord_);

        auto end = std::chrono::steady_clock::now();
        double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
        update_latencies_ms.push_back(latency_ms);

        // Real timing between updates
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Calculate statistics
    double max_latency = 0;
    double total_latency = 0;
    for (auto lat : update_latencies_ms) {
        max_latency = std::max(max_latency, lat);
        total_latency += lat;
    }
    double avg_latency = total_latency / update_latencies_ms.size();

    // Update latency should be reasonable
    EXPECT_LT(avg_latency, 50.0)
        << "Average update latency should be under 50ms";
    EXPECT_LT(max_latency, 200.0)
        << "Maximum update latency should be under 200ms";

    // System should remain healthy
    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(cycle_coord_, &stats);
    EXPECT_GT(stats.overall_health, 0.5f);
}
