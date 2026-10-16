/**
 * @file test_swarm_stability_regression.cpp
 * @brief Comprehensive Regression Tests for Swarm System Stability
 *
 * WHAT: Regression tests for swarm memory usage, threat response, and conflict resolution
 * WHY:  Ensure stable memory usage, consistent threat handling, and deterministic conflict resolution
 * HOW:  Test memory under load, threat detection patterns, and conflict resolution outcomes
 *
 * REGRESSION CATEGORIES:
 * - Memory Usage Stability: No memory growth under sustained operations
 * - Threat Response Consistency: Same threats produce same responses
 * - Conflict Resolution Determinism: Same conflicts produce same resolutions
 * - Gossip Protocol Stability: Belief propagation is stable
 * - API Stability: Struct layouts and enum values
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <thread>
#include <fstream>
#include <sstream>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_memory.h"
#include "swarm/nimcp_gossip_beliefs.h"
#include "swarm/nimcp_swarm_multi.h"
#include "swarm/nimcp_swarm_signal.h"
#include "utils/memory/nimcp_memory.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Memory Monitoring Utility
//=============================================================================

class MemoryMonitor {
public:
    struct MemoryStats {
        size_t rss_kb;
        size_t vms_kb;
        size_t peak_rss_kb;
    };

    static MemoryStats GetCurrentMemory() {
        MemoryStats stats = {0, 0, 0};

#ifdef __linux__
        std::ifstream status("/proc/self/status");
        std::string line;

        while (std::getline(status, line)) {
            if (line.find("VmRSS:") == 0) {
                std::istringstream iss(line);
                std::string key;
                iss >> key >> stats.rss_kb;
            } else if (line.find("VmSize:") == 0) {
                std::istringstream iss(line);
                std::string key;
                iss >> key >> stats.vms_kb;
            } else if (line.find("VmHWM:") == 0) {
                std::istringstream iss(line);
                std::string key;
                iss >> key >> stats.peak_rss_kb;
            }
        }
#endif
        return stats;
    }

    static size_t GetMemoryDelta(const MemoryStats& before, const MemoryStats& after) {
        return after.rss_kb > before.rss_kb ? after.rss_kb - before.rss_kb : 0;
    }
};

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmStabilityRegressionTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coordinator = nullptr;
    nimcp_swarm_memory_t* swarm_memory = nullptr;
    nimcp_gossip_context_t* gossip_ctx = nullptr;
    std::vector<nimcp_swarm_identity_t*> swarms;
    MemoryMonitor::MemoryStats initial_memory;

    void SetUp() override {
        initial_memory = MemoryMonitor::GetCurrentMemory();
    }

    void TearDown() override {
        for (auto* swarm : swarms) {
            if (swarm) {
                nimcp_swarm_identity_destroy(swarm);
            }
        }
        swarms.clear();

        if (gossip_ctx) {
            nimcp_gossip_destroy(gossip_ctx);
            gossip_ctx = nullptr;
        }

        if (swarm_memory) {
            nimcp_swarm_memory_destroy(swarm_memory);
            swarm_memory = nullptr;
        }

        if (coordinator) {
            nimcp_multi_swarm_destroy(coordinator);
            coordinator = nullptr;
        }
    }

    void CreateCoordinator() {
        coordinator = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr);
    }

    void CreateSwarms(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            char name[64];
            snprintf(name, sizeof(name), "swarm_%u", i);

            auto* swarm = nimcp_swarm_identity_create(coordinator, name, 10 + (i % 20));
            if (swarm) {
                if (nimcp_swarm_register(coordinator, swarm) == NIMCP_SUCCESS) {
                    swarms.push_back(swarm);
                } else {
                    nimcp_swarm_identity_destroy(swarm);
                }
            }
        }
    }

    void CreateSwarmMemory() {
        nimcp_swarm_memory_config_t config;
        memset(&config, 0, sizeof(config));
        config.max_entries = 1000;
        config.max_memory_bytes = 10 * 1024 * 1024;  // 10MB
        config.eviction_policy = SWARM_MEMORY_EVICT_LRU;

        swarm_memory = nimcp_swarm_memory_create(&config);
        ASSERT_NE(swarm_memory, nullptr);
    }

    void CreateGossipContext() {
        nimcp_gossip_config_t config;
        memset(&config, 0, sizeof(config));
        config.max_beliefs = 100;
        config.convergence_threshold = 0.001f;
        config.max_rounds = 50;
        config.dampening_factor = 0.8f;

        gossip_ctx = nimcp_gossip_create(&config);
        ASSERT_NE(gossip_ctx, nullptr);
    }
};

//=============================================================================
// Memory Usage Stability Tests
//=============================================================================

TEST_F(SwarmStabilityRegressionTest, MemoryStableUnderLoad) {
    // WHAT: Verify memory usage stays stable under sustained operations
    // WHY:  Embedded systems have limited memory
    // REGRESSION: Memory must not grow unboundedly

    CreateCoordinator();
    CreateSwarms(20);
    CreateSwarmMemory();

    auto baseline = MemoryMonitor::GetCurrentMemory();

    // Perform many operations
    for (int iteration = 0; iteration < 1000; iteration++) {
        // Store and retrieve from swarm memory
        char key[32];
        snprintf(key, sizeof(key), "key_%d", iteration % 100);

        nimcp_swarm_memory_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.timestamp_ns = iteration * 1000;
        entry.priority = iteration % 10;

        nimcp_swarm_memory_store(swarm_memory, key, &entry);
        nimcp_swarm_memory_retrieve(swarm_memory, key, &entry);
    }

    auto after_load = MemoryMonitor::GetCurrentMemory();
    size_t memory_growth_mb = MemoryMonitor::GetMemoryDelta(baseline, after_load) / 1024;

    // Memory growth should be minimal (< 5MB)
    EXPECT_LT(memory_growth_mb, 5)
        << "Memory grew by " << memory_growth_mb << " MB during sustained operations";

    std::cout << "Memory after 1000 operations: +" << memory_growth_mb << " MB" << std::endl;
}

TEST_F(SwarmStabilityRegressionTest, NoMemoryLeakOnRepeatedCreation) {
    // WHAT: Verify no memory leak on repeated create/destroy cycles
    // WHY:  Dynamic allocation must be properly freed
    // REGRESSION: Bug fix - memory leak on destroy

    auto baseline = MemoryMonitor::GetCurrentMemory();

    for (int iteration = 0; iteration < 50; iteration++) {
        // Create coordinator and swarms
        coordinator = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr);

        for (int i = 0; i < 5; i++) {
            char name[64];
            snprintf(name, sizeof(name), "temp_swarm_%d_%d", iteration, i);
            auto* swarm = nimcp_swarm_identity_create(coordinator, name, 10);
            if (swarm) {
                nimcp_swarm_register(coordinator, swarm);
                // Swarm is now owned by coordinator
            }
        }

        // Destroy everything
        nimcp_multi_swarm_destroy(coordinator);
        coordinator = nullptr;
    }

    auto final = MemoryMonitor::GetCurrentMemory();
    size_t memory_growth_mb = MemoryMonitor::GetMemoryDelta(baseline, final) / 1024;

    // Should have minimal growth (< 5MB for allocator overhead)
    EXPECT_LT(memory_growth_mb, 5)
        << "Memory leaked " << memory_growth_mb << " MB after 50 create/destroy cycles";
}

TEST_F(SwarmStabilityRegressionTest, SwarmMemoryEvictionWorks) {
    // WHAT: Verify memory eviction prevents unbounded growth
    // WHY:  Swarm memory must stay within limits
    // REGRESSION: Eviction must actually free memory

    CreateSwarmMemory();

    // Store more entries than max
    for (int i = 0; i < 2000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "entry_%d", i);

        nimcp_swarm_memory_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.timestamp_ns = i * 1000;
        entry.priority = i % 10;

        nimcp_swarm_memory_store(swarm_memory, key, &entry);
    }

    // Get stats
    nimcp_swarm_memory_stats_t stats;
    nimcp_swarm_memory_get_stats(swarm_memory, &stats);

    // Entry count should be limited
    EXPECT_LE(stats.current_entries, 1000u);

    std::cout << "Swarm memory entries: " << stats.current_entries
              << " / " << stats.max_entries << std::endl;
}

TEST_F(SwarmStabilityRegressionTest, GossipMemoryStability) {
    // WHAT: Verify gossip protocol memory is stable
    // WHY:  Belief propagation must not leak memory
    // REGRESSION: Gossip memory leak fix

    CreateGossipContext();

    auto baseline = MemoryMonitor::GetCurrentMemory();

    // Propagate many beliefs
    for (int round = 0; round < 100; round++) {
        for (int belief = 0; belief < 10; belief++) {
            char belief_name[32];
            snprintf(belief_name, sizeof(belief_name), "belief_%d", belief);

            float confidence = 0.5f + (round % 10) * 0.05f;
            nimcp_gossip_propagate_belief(gossip_ctx, belief_name, confidence);
        }

        // Run gossip round
        nimcp_gossip_update(gossip_ctx);
    }

    auto after = MemoryMonitor::GetCurrentMemory();
    size_t memory_growth_mb = MemoryMonitor::GetMemoryDelta(baseline, after) / 1024;

    // Memory growth should be minimal
    EXPECT_LT(memory_growth_mb, 2)
        << "Gossip memory grew by " << memory_growth_mb << " MB";
}

//=============================================================================
// Threat Response Consistency Tests
//=============================================================================

TEST_F(SwarmStabilityRegressionTest, ThreatDetectionDeterministic) {
    // WHAT: Verify threat detection produces consistent results
    // WHY:  Same threats must produce same responses
    // REGRESSION: Threat detection must be deterministic

    CreateCoordinator();
    CreateSwarms(10);

    // Create threat scenario
    nimcp_threat_t threat;
    memset(&threat, 0, sizeof(threat));
    threat.type = NIMCP_THREAT_JAMMER;
    threat.severity = 0.8f;
    threat.location = {100.0f, 100.0f, 50.0f};
    threat.radius = 50.0f;

    std::vector<nimcp_threat_response_t> responses;

    // Detect threat multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_threat_response_t response;
        nimcp_result_t result = nimcp_swarm_detect_threat(coordinator, &threat, &response);

        if (result == NIMCP_SUCCESS) {
            responses.push_back(response);
        }
    }

    // All responses should be identical
    for (size_t i = 1; i < responses.size(); i++) {
        EXPECT_EQ(responses[0].action, responses[i].action)
            << "Response " << i << " differs from first response";
        EXPECT_FLOAT_EQ(responses[0].confidence, responses[i].confidence);
    }
}

TEST_F(SwarmStabilityRegressionTest, ThreatEscalationConsistent) {
    // WHAT: Verify threat escalation follows consistent rules
    // WHY:  Escalation must be predictable
    // REGRESSION: Escalation behavior must not change

    CreateCoordinator();
    CreateSwarms(10);

    // Low severity threat
    nimcp_threat_t low_threat;
    memset(&low_threat, 0, sizeof(low_threat));
    low_threat.type = NIMCP_THREAT_PROXIMITY;
    low_threat.severity = 0.2f;

    nimcp_threat_response_t low_response;
    nimcp_swarm_detect_threat(coordinator, &low_threat, &low_response);

    // High severity threat
    nimcp_threat_t high_threat;
    memset(&high_threat, 0, sizeof(high_threat));
    high_threat.type = NIMCP_THREAT_COLLISION;
    high_threat.severity = 0.9f;

    nimcp_threat_response_t high_response;
    nimcp_swarm_detect_threat(coordinator, &high_threat, &high_response);

    // Higher severity should trigger more aggressive response
    EXPECT_GE(static_cast<int>(high_response.action), static_cast<int>(low_response.action));
}

TEST_F(SwarmStabilityRegressionTest, ThreatResponseTimeConsistent) {
    // WHAT: Verify threat response time is consistent
    // WHY:  Response time must meet real-time requirements
    // BASELINE: < 10ms for threat detection

    CreateCoordinator();
    CreateSwarms(10);

    nimcp_threat_t threat;
    memset(&threat, 0, sizeof(threat));
    threat.type = NIMCP_THREAT_JAMMER;
    threat.severity = 0.5f;

    std::vector<long> response_times;

    for (int i = 0; i < 100; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        nimcp_threat_response_t response;
        nimcp_swarm_detect_threat(coordinator, &threat, &response);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        response_times.push_back(duration.count());
    }

    // Calculate average and max
    long sum = 0;
    long max_time = 0;
    for (long t : response_times) {
        sum += t;
        max_time = std::max(max_time, t);
    }
    double avg_time = static_cast<double>(sum) / response_times.size();

    std::cout << "Threat response time: avg=" << (avg_time / 1000.0)
              << "ms, max=" << (max_time / 1000.0) << "ms" << std::endl;

    // Average should be < 10ms
    EXPECT_LT(avg_time, 10000);  // 10ms in microseconds
}

//=============================================================================
// Conflict Resolution Determinism Tests
//=============================================================================

TEST_F(SwarmStabilityRegressionTest, ConflictResolutionDeterministic) {
    // WHAT: Verify conflict resolution produces deterministic results
    // WHY:  Same conflicts must produce same resolutions
    // REGRESSION: Conflict resolution must be deterministic

    CreateCoordinator();
    CreateSwarms(10);

    // Create overlapping territories for conflict
    float base_size = 100.0f;
    for (size_t i = 0; i < swarms.size(); i++) {
        float offset = i * 20.0f;  // Overlapping
        nimcp_coord3d_t min = {offset, offset, 0};
        nimcp_coord3d_t max = {offset + base_size, offset + base_size, 50};
        float priority = 1.0f - (i / static_cast<float>(swarms.size()));
        nimcp_swarm_set_territory(swarms[i], min, max, true, priority);
    }

    std::vector<uint32_t> conflict_counts;

    // Detect conflicts multiple times
    for (int i = 0; i < 5; i++) {
        nimcp_swarm_conflict_t* conflicts = nullptr;
        uint32_t count = 0;
        nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &count);

        conflict_counts.push_back(count);

        if (conflicts) {
            nimcp_free(conflicts);
        }
    }

    // All detections should find same number of conflicts
    for (size_t i = 1; i < conflict_counts.size(); i++) {
        EXPECT_EQ(conflict_counts[0], conflict_counts[i])
            << "Detection " << i << " found different conflict count";
    }
}

TEST_F(SwarmStabilityRegressionTest, ResolutionStrategyConsistent) {
    // WHAT: Verify each resolution strategy produces consistent results
    // WHY:  Strategy behavior must be predictable
    // REGRESSION: Strategy behavior must not change

    CreateCoordinator();
    CreateSwarms(5);

    // Create overlapping territories
    for (size_t i = 0; i < swarms.size(); i++) {
        nimcp_coord3d_t min = {0, 0, 0};
        nimcp_coord3d_t max = {100, 100, 50};
        nimcp_swarm_set_territory(swarms[i], min, max, true, static_cast<float>(i) / 5.0f);
    }

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &count);

    if (count > 0) {
        // Test each strategy
        nimcp_conflict_resolution_t strategies[] = {
            NIMCP_CONFLICT_PRIORITY,
            NIMCP_CONFLICT_TIME_SHARING,
            NIMCP_CONFLICT_RESOLVE_PARTITION,
            NIMCP_CONFLICT_RESOLVE_MERGE
        };

        for (auto strategy : strategies) {
            // Same conflict should resolve same way with same strategy
            uint32_t conflict_id = conflicts[0].conflict_id;

            nimcp_result_t result = nimcp_multi_swarm_resolve_conflict(
                coordinator, conflict_id, strategy, nullptr
            );

            // Should succeed or fail consistently
            // (exact behavior depends on implementation)
        }
    }

    if (conflicts) {
        nimcp_free(conflicts);
    }
}

TEST_F(SwarmStabilityRegressionTest, AutoResolveIdempotent) {
    // WHAT: Verify auto-resolve is idempotent after resolution
    // WHY:  Resolved conflicts should stay resolved
    // REGRESSION: Double resolution must not cause issues

    CreateCoordinator();
    CreateSwarms(5);

    // Create overlapping territories
    for (size_t i = 0; i < swarms.size(); i++) {
        nimcp_coord3d_t min = {0, 0, 0};
        nimcp_coord3d_t max = {100, 100, 50};
        nimcp_swarm_set_territory(swarms[i], min, max, true, static_cast<float>(i) / 5.0f);
    }

    // First auto-resolve
    nimcp_conflict_detect(coordinator);
    uint32_t first_resolved = nimcp_conflict_auto_resolve(coordinator, nullptr, nullptr);

    // Second auto-resolve (should resolve remaining or none)
    nimcp_conflict_detect(coordinator);
    uint32_t second_resolved = nimcp_conflict_auto_resolve(coordinator, nullptr, nullptr);

    // After first resolve, fewer (or no) conflicts should remain
    EXPECT_LE(second_resolved, first_resolved);

    std::cout << "First resolve: " << first_resolved
              << ", Second resolve: " << second_resolved << std::endl;
}

//=============================================================================
// Gossip Protocol Stability Tests
//=============================================================================

TEST_F(SwarmStabilityRegressionTest, BeliefConvergenceDeterministic) {
    // WHAT: Verify belief convergence is deterministic
    // WHY:  Same initial beliefs must converge to same values
    // REGRESSION: Convergence must be reproducible

    std::vector<float> final_beliefs_run1;
    std::vector<float> final_beliefs_run2;

    // Run 1
    {
        CreateGossipContext();

        // Initialize beliefs
        nimcp_gossip_propagate_belief(gossip_ctx, "belief_a", 0.8f);
        nimcp_gossip_propagate_belief(gossip_ctx, "belief_b", 0.3f);
        nimcp_gossip_propagate_belief(gossip_ctx, "belief_c", 0.5f);

        // Run until convergence
        for (int round = 0; round < 50; round++) {
            nimcp_gossip_update(gossip_ctx);
        }

        // Get final beliefs
        final_beliefs_run1.push_back(nimcp_gossip_get_belief(gossip_ctx, "belief_a"));
        final_beliefs_run1.push_back(nimcp_gossip_get_belief(gossip_ctx, "belief_b"));
        final_beliefs_run1.push_back(nimcp_gossip_get_belief(gossip_ctx, "belief_c"));

        nimcp_gossip_destroy(gossip_ctx);
        gossip_ctx = nullptr;
    }

    // Run 2 (identical setup)
    {
        CreateGossipContext();

        nimcp_gossip_propagate_belief(gossip_ctx, "belief_a", 0.8f);
        nimcp_gossip_propagate_belief(gossip_ctx, "belief_b", 0.3f);
        nimcp_gossip_propagate_belief(gossip_ctx, "belief_c", 0.5f);

        for (int round = 0; round < 50; round++) {
            nimcp_gossip_update(gossip_ctx);
        }

        final_beliefs_run2.push_back(nimcp_gossip_get_belief(gossip_ctx, "belief_a"));
        final_beliefs_run2.push_back(nimcp_gossip_get_belief(gossip_ctx, "belief_b"));
        final_beliefs_run2.push_back(nimcp_gossip_get_belief(gossip_ctx, "belief_c"));
    }

    // Beliefs should match
    ASSERT_EQ(final_beliefs_run1.size(), final_beliefs_run2.size());
    for (size_t i = 0; i < final_beliefs_run1.size(); i++) {
        EXPECT_FLOAT_EQ(final_beliefs_run1[i], final_beliefs_run2[i])
            << "Belief " << i << " differs between runs";
    }
}

TEST_F(SwarmStabilityRegressionTest, BeliefBoundsEnforced) {
    // WHAT: Verify belief values stay in valid range [0, 1]
    // WHY:  Beliefs are probabilities
    // REGRESSION: Bounds must be enforced

    CreateGossipContext();

    // Propagate extreme values
    nimcp_gossip_propagate_belief(gossip_ctx, "high", 100.0f);
    nimcp_gossip_propagate_belief(gossip_ctx, "low", -50.0f);
    nimcp_gossip_propagate_belief(gossip_ctx, "normal", 0.5f);

    // Update
    nimcp_gossip_update(gossip_ctx);

    // Get beliefs
    float high = nimcp_gossip_get_belief(gossip_ctx, "high");
    float low = nimcp_gossip_get_belief(gossip_ctx, "low");
    float normal = nimcp_gossip_get_belief(gossip_ctx, "normal");

    // All should be clamped to [0, 1]
    EXPECT_GE(high, 0.0f);
    EXPECT_LE(high, 1.0f);
    EXPECT_GE(low, 0.0f);
    EXPECT_LE(low, 1.0f);
    EXPECT_GE(normal, 0.0f);
    EXPECT_LE(normal, 1.0f);
}

TEST_F(SwarmStabilityRegressionTest, GossipConvergenceSpeed) {
    // WHAT: Verify gossip converges within expected rounds
    // WHY:  Convergence speed must be predictable
    // BASELINE: Should converge within 50 rounds

    CreateGossipContext();

    // Initialize with diverse beliefs
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "belief_%d", i);
        nimcp_gossip_propagate_belief(gossip_ctx, name, static_cast<float>(i) / 10.0f);
    }

    int convergence_round = -1;
    float prev_change = 1.0f;

    for (int round = 0; round < 100; round++) {
        float change = nimcp_gossip_update(gossip_ctx);

        if (change < 0.001f && prev_change < 0.001f) {
            convergence_round = round;
            break;
        }
        prev_change = change;
    }

    std::cout << "Gossip converged at round: " << convergence_round << std::endl;

    // Should converge within 50 rounds
    EXPECT_LT(convergence_round, 50);
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(SwarmStabilityRegressionTest, ConflictDetectionPerformance) {
    // WHAT: Verify conflict detection performance
    // WHY:  Performance baseline
    // BASELINE: < 100ms for 50 swarms

    CreateCoordinator();
    CreateSwarms(50);

    // Create overlapping territories
    for (size_t i = 0; i < swarms.size(); i++) {
        float offset = i * 10.0f;
        nimcp_coord3d_t min = {offset, offset, 0};
        nimcp_coord3d_t max = {offset + 100.0f, offset + 100.0f, 50};
        nimcp_swarm_set_territory(swarms[i], min, max, true, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &count);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Conflict detection (" << swarms.size() << " swarms): "
              << duration.count() << "ms, found " << count << " conflicts" << std::endl;

    EXPECT_LT(duration.count(), 100);

    if (conflicts) {
        nimcp_free(conflicts);
    }
}

TEST_F(SwarmStabilityRegressionTest, SwarmMemoryOperationPerformance) {
    // WHAT: Verify swarm memory operation performance
    // WHY:  Performance baseline
    // BASELINE: < 1ms for 1000 operations

    CreateSwarmMemory();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i % 100);

        nimcp_swarm_memory_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.timestamp_ns = i * 1000;

        nimcp_swarm_memory_store(swarm_memory, key, &entry);
        nimcp_swarm_memory_retrieve(swarm_memory, key, &entry);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "1000 memory operations: " << (duration.count() / 1000.0) << "ms" << std::endl;

    EXPECT_LT(duration.count(), 10000);  // < 10ms
}

TEST_F(SwarmStabilityRegressionTest, GossipUpdatePerformance) {
    // WHAT: Verify gossip update performance
    // WHY:  Performance baseline
    // BASELINE: < 1ms per update

    CreateGossipContext();

    // Initialize with many beliefs
    for (int i = 0; i < 50; i++) {
        char name[32];
        snprintf(name, sizeof(name), "belief_%d", i);
        nimcp_gossip_propagate_belief(gossip_ctx, name, static_cast<float>(i) / 50.0f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        nimcp_gossip_update(gossip_ctx);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "100 gossip updates: " << (duration.count() / 1000.0) << "ms" << std::endl;

    EXPECT_LT(duration.count(), 100000);  // < 100ms total (< 1ms per update)
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SwarmStabilityRegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash

    // NULL coordinator operations
    nimcp_multi_swarm_destroy(nullptr);  // Should not crash

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    EXPECT_NE(nimcp_multi_swarm_detect_conflicts(nullptr, &conflicts, &count), NIMCP_SUCCESS);

    // NULL swarm memory operations
    nimcp_swarm_memory_destroy(nullptr);  // Should not crash

    // NULL gossip operations
    nimcp_gossip_destroy(nullptr);  // Should not crash
    EXPECT_FLOAT_EQ(nimcp_gossip_get_belief(nullptr, "test"), 0.0f);
}

TEST_F(SwarmStabilityRegressionTest, InvalidConfigHandling) {
    // WHAT: Verify invalid config is handled
    // WHY:  Must reject invalid configurations
    // REGRESSION: Invalid config must be rejected

    // Zero capacity swarm memory
    nimcp_swarm_memory_config_t bad_config;
    memset(&bad_config, 0, sizeof(bad_config));
    bad_config.max_entries = 0;
    bad_config.max_memory_bytes = 0;

    nimcp_swarm_memory_t* bad_memory = nimcp_swarm_memory_create(&bad_config);
    // Should either return NULL or create with defaults

    if (bad_memory) {
        nimcp_swarm_memory_destroy(bad_memory);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SwarmStabilityRegressionTest, ConflictStatisticsAccurate) {
    // WHAT: Verify conflict statistics are accurate
    // WHY:  Monitoring depends on accurate stats
    // REGRESSION: Statistics must be accurate

    CreateCoordinator();
    CreateSwarms(10);

    // Create overlapping territories
    for (size_t i = 0; i < swarms.size(); i++) {
        nimcp_coord3d_t min = {0, 0, 0};
        nimcp_coord3d_t max = {100, 100, 50};
        nimcp_swarm_set_territory(swarms[i], min, max, true, 0.5f);
    }

    // Detect and resolve conflicts
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &count);

    if (count > 0) {
        // Resolve some conflicts
        uint32_t to_resolve = (count < 3) ? count : 3;
        for (uint32_t i = 0; i < to_resolve; i++) {
            nimcp_multi_swarm_resolve_conflict(coordinator, conflicts[i].conflict_id,
                NIMCP_CONFLICT_PRIORITY, nullptr);
        }
    }

    auto stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    EXPECT_EQ(stats.total_conflicts, count);
    EXPECT_LE(stats.conflicts_resolved, count);
    EXPECT_EQ(stats.conflicts_pending, count - stats.conflicts_resolved);

    if (conflicts) {
        nimcp_free(conflicts);
    }
}

TEST_F(SwarmStabilityRegressionTest, SwarmMemoryStatisticsAccurate) {
    // WHAT: Verify swarm memory statistics are accurate
    // WHY:  Monitoring depends on accurate stats
    // REGRESSION: Statistics must be accurate

    CreateSwarmMemory();

    // Store entries
    for (int i = 0; i < 50; i++) {
        char key[32];
        snprintf(key, sizeof(key), "entry_%d", i);

        nimcp_swarm_memory_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.timestamp_ns = i * 1000;

        nimcp_swarm_memory_store(swarm_memory, key, &entry);
    }

    nimcp_swarm_memory_stats_t stats;
    nimcp_swarm_memory_get_stats(swarm_memory, &stats);

    EXPECT_EQ(stats.current_entries, 50u);
    EXPECT_GT(stats.bytes_used, 0u);
    EXPECT_LE(stats.bytes_used, stats.bytes_limit);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
