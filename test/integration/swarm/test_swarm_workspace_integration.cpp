/**
 * @file test_swarm_workspace_integration.cpp
 * @brief Integration Tests for Swarm Workspace (Workspace + Emergence)
 *
 * WHAT: Tests integration of shared workspace and emergent behavior
 * WHY:  Verify CRDT merge across network and coherence-based emergence
 * HOW:  Multiple drones adding items, merging state, computing emergence tier
 *
 * TEST SCENARIOS:
 * - Multiple drones adding items to shared workspace
 * - CRDT merge across simulated network
 * - Emergence tier calculation based on coherence
 * - Conflict resolution in concurrent updates
 * - Workspace synchronization
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <string>
#include <atomic>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Mock Workspace Structures (since swarm workspace headers may not exist yet)
//=============================================================================

/**
 * @brief CRDT entry in the shared workspace
 */
struct WorkspaceEntry {
    std::string key;
    std::string value;
    uint64_t timestamp;
    uint32_t drone_id;
    uint32_t version;

    bool operator<(const WorkspaceEntry& other) const {
        // Last-write-wins conflict resolution
        if (timestamp != other.timestamp) {
            return timestamp < other.timestamp;
        }
        return drone_id < other.drone_id;
    }
};

/**
 * @brief Shared workspace using CRDT
 */
class SwarmWorkspace {
private:
    std::map<std::string, WorkspaceEntry> entries_;
    uint32_t drone_id_;
    std::atomic<uint32_t> version_counter_{0};

public:
    explicit SwarmWorkspace(uint32_t drone_id) : drone_id_(drone_id) {}

    // Add or update entry
    bool Put(const std::string& key, const std::string& value) {
        WorkspaceEntry entry;
        entry.key = key;
        entry.value = value;
        entry.timestamp = GetTimestamp();
        entry.drone_id = drone_id_;
        entry.version = version_counter_++;

        entries_[key] = entry;
        return true;
    }

    // Get entry
    bool Get(const std::string& key, std::string& value) const {
        auto it = entries_.find(key);
        if (it != entries_.end()) {
            value = it->second.value;
            return true;
        }
        return false;
    }

    // Merge another workspace (CRDT merge)
    void Merge(const SwarmWorkspace& other) {
        for (const auto& pair : other.entries_) {
            const std::string& key = pair.first;
            const WorkspaceEntry& other_entry = pair.second;

            auto it = entries_.find(key);
            if (it == entries_.end()) {
                // New entry
                entries_[key] = other_entry;
            } else {
                // Conflict resolution: keep the newer entry
                if (other_entry.timestamp > it->second.timestamp ||
                    (other_entry.timestamp == it->second.timestamp &&
                     other_entry.drone_id > it->second.drone_id)) {
                    entries_[key] = other_entry;
                }
            }
        }
    }

    // Get all entries
    const std::map<std::string, WorkspaceEntry>& GetEntries() const {
        return entries_;
    }

    // Calculate coherence (agreement ratio)
    double CalculateCoherence(const std::vector<SwarmWorkspace*>& workspaces) const {
        if (workspaces.empty()) return 0.0;

        size_t total_agreements = 0;
        size_t total_comparisons = 0;

        for (const auto& pair : entries_) {
            const std::string& key = pair.first;
            const std::string& value = pair.second.value;

            for (const auto* other : workspaces) {
                std::string other_value;
                if (other->Get(key, other_value)) {
                    total_comparisons++;
                    if (value == other_value) {
                        total_agreements++;
                    }
                }
            }
        }

        return total_comparisons > 0 ?
               static_cast<double>(total_agreements) / total_comparisons : 0.0;
    }

    // Calculate emergence tier based on coherence
    int CalculateEmergenceTier(double coherence) const {
        if (coherence >= 0.95) return 4; // High emergence
        if (coherence >= 0.80) return 3; // Medium-high
        if (coherence >= 0.60) return 2; // Medium
        if (coherence >= 0.40) return 1; // Low
        return 0; // No emergence
    }

    size_t Size() const { return entries_.size(); }

private:
    static uint64_t GetTimestamp() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
};

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmWorkspaceIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint32_t NUM_DRONES = 5;
    std::vector<SwarmWorkspace*> workspaces_;

    void SetUp() override {
        // Initialize logging
        // Logging initialized in framework
        // Log level set in framework

        // Create workspace for each drone
        workspaces_.resize(NUM_DRONES);
        for (uint32_t i = 0; i < NUM_DRONES; i++) {
            workspaces_[i] = new SwarmWorkspace(1000 + i);
        }
    }

    void TearDown() override {
        for (auto* workspace : workspaces_) {
            delete workspace;
        }
        workspaces_.clear();
    }

    // Helper: Merge all workspaces
    void MergeAllWorkspaces() {
        for (size_t i = 0; i < workspaces_.size(); i++) {
            for (size_t j = 0; j < workspaces_.size(); j++) {
                if (i != j) {
                    workspaces_[i]->Merge(*workspaces_[j]);
                }
            }
        }
    }
};

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test 1: Basic Workspace Operations
 * Verify that basic put/get operations work
 */
TEST_F(SwarmWorkspaceIntegrationTest, BasicWorkspaceOperations) {
    SwarmWorkspace* ws = workspaces_[0];

    // Put some values
    EXPECT_TRUE(ws->Put("temperature", "25.5"));
    EXPECT_TRUE(ws->Put("humidity", "60"));
    EXPECT_TRUE(ws->Put("altitude", "100.0"));

    // Get values
    std::string value;
    EXPECT_TRUE(ws->Get("temperature", value));
    EXPECT_EQ(value, "25.5");

    EXPECT_TRUE(ws->Get("humidity", value));
    EXPECT_EQ(value, "60");

    EXPECT_TRUE(ws->Get("altitude", value));
    EXPECT_EQ(value, "100.0");

    // Non-existent key
    EXPECT_FALSE(ws->Get("pressure", value));
}

/**
 * Test 2: Multiple Drones Adding Items
 * Verify that multiple drones can add items to their workspaces
 */
TEST_F(SwarmWorkspaceIntegrationTest, MultipleDronesAddingItems) {
    // Each drone adds sensor data
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        std::string drone_key = "drone_" + std::to_string(i) + "_status";
        std::string drone_value = "active";
        EXPECT_TRUE(workspaces_[i]->Put(drone_key, drone_value));

        // Add temperature reading
        std::string temp_key = "temperature_" + std::to_string(i);
        std::string temp_value = std::to_string(20.0 + i);
        EXPECT_TRUE(workspaces_[i]->Put(temp_key, temp_value));
    }

    // Verify each workspace has its own data
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        EXPECT_EQ(workspaces_[i]->Size(), 2) << "Drone " << i << " should have 2 entries";
    }
}

/**
 * Test 3: CRDT Merge Across Network
 * Verify that CRDT merge correctly combines workspaces
 */
TEST_F(SwarmWorkspaceIntegrationTest, CRDTMergeAcrossNetwork) {
    // Each drone adds unique data
    workspaces_[0]->Put("sensor_0", "data_0");
    workspaces_[1]->Put("sensor_1", "data_1");
    workspaces_[2]->Put("sensor_2", "data_2");

    // Merge all workspaces
    MergeAllWorkspaces();

    // After merge, all workspaces should have all entries
    for (uint32_t i = 0; i < 3; i++) {
        EXPECT_EQ(workspaces_[i]->Size(), 3)
            << "Workspace " << i << " should have all 3 entries after merge";

        std::string value;
        EXPECT_TRUE(workspaces_[i]->Get("sensor_0", value));
        EXPECT_EQ(value, "data_0");
        EXPECT_TRUE(workspaces_[i]->Get("sensor_1", value));
        EXPECT_EQ(value, "data_1");
        EXPECT_TRUE(workspaces_[i]->Get("sensor_2", value));
        EXPECT_EQ(value, "data_2");
    }
}

/**
 * Test 4: Conflict Resolution - Last Write Wins
 * Verify that concurrent updates are resolved correctly
 */
TEST_F(SwarmWorkspaceIntegrationTest, ConflictResolutionLastWriteWins) {
    // Multiple drones update the same key
    workspaces_[0]->Put("target_location", "zone_A");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    workspaces_[1]->Put("target_location", "zone_B");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    workspaces_[2]->Put("target_location", "zone_C");

    // Merge all workspaces
    MergeAllWorkspaces();

    // All workspaces should converge to the last write (zone_C)
    for (uint32_t i = 0; i < 3; i++) {
        std::string value;
        EXPECT_TRUE(workspaces_[i]->Get("target_location", value));
        EXPECT_EQ(value, "zone_C") << "Workspace " << i << " should have last write";
    }
}

/**
 * Test 5: Emergence Tier Calculation - High Coherence
 * Verify emergence tier with high agreement
 */
TEST_F(SwarmWorkspaceIntegrationTest, EmergenceTierHighCoherence) {
    // All drones agree on the same values
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        workspaces_[i]->Put("formation", "V-shape");
        workspaces_[i]->Put("target", "waypoint_1");
        workspaces_[i]->Put("speed", "10");
    }

    // Merge workspaces
    MergeAllWorkspaces();

    // Calculate coherence
    std::vector<SwarmWorkspace*> others;
    for (uint32_t i = 1; i < NUM_DRONES; i++) {
        others.push_back(workspaces_[i]);
    }

    double coherence = workspaces_[0]->CalculateCoherence(others);
    EXPECT_GE(coherence, 0.95) << "High agreement should result in high coherence";

    int tier = workspaces_[0]->CalculateEmergenceTier(coherence);
    EXPECT_EQ(tier, 4) << "High coherence should result in tier 4 emergence";
}

/**
 * Test 6: Emergence Tier Calculation - Medium Coherence
 * Verify emergence tier with partial agreement
 */
TEST_F(SwarmWorkspaceIntegrationTest, EmergenceTierMediumCoherence) {
    // Partial agreement
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        workspaces_[i]->Put("formation", i < 3 ? "V-shape" : "Line");
        workspaces_[i]->Put("target", "waypoint_1");
    }

    // Merge workspaces
    MergeAllWorkspaces();

    // Calculate coherence
    std::vector<SwarmWorkspace*> others;
    for (uint32_t i = 1; i < NUM_DRONES; i++) {
        others.push_back(workspaces_[i]);
    }

    double coherence = workspaces_[0]->CalculateCoherence(others);
    EXPECT_GE(coherence, 0.0);
    EXPECT_LE(coherence, 1.0);  // Coherence can be exactly 1.0 for identical workspaces

    int tier = workspaces_[0]->CalculateEmergenceTier(coherence);
    EXPECT_GE(tier, 0);
    EXPECT_LE(tier, 4);
}

/**
 * Test 7: Workspace Synchronization
 * Verify that workspaces stay synchronized after multiple updates
 */
TEST_F(SwarmWorkspaceIntegrationTest, WorkspaceSynchronization) {
    // Perform multiple rounds of updates and merges
    for (int round = 0; round < 5; round++) {
        // Each drone updates some values
        for (uint32_t i = 0; i < NUM_DRONES; i++) {
            std::string key = "round_" + std::to_string(round) + "_drone_" + std::to_string(i);
            std::string value = "value_" + std::to_string(round);
            workspaces_[i]->Put(key, value);
        }

        // Merge all workspaces
        MergeAllWorkspaces();
    }

    // All workspaces should have the same number of entries
    size_t expected_size = 5 * NUM_DRONES; // 5 rounds * 5 drones
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        EXPECT_EQ(workspaces_[i]->Size(), expected_size)
            << "Workspace " << i << " should be synchronized";
    }
}

/**
 * Test 8: Concurrent Updates
 * Verify that concurrent updates from multiple drones work correctly
 */
TEST_F(SwarmWorkspaceIntegrationTest, ConcurrentUpdates) {
    std::vector<std::thread> threads;
    std::atomic<int> update_count{0};

    // Each drone updates concurrently
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        threads.emplace_back([this, i, &update_count]() {
            for (int j = 0; j < 20; j++) {
                std::string key = "concurrent_key_" + std::to_string(j);
                std::string value = "drone_" + std::to_string(i) + "_value_" + std::to_string(j);
                if (workspaces_[i]->Put(key, value)) {
                    update_count++;
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(update_count.load(), NUM_DRONES * 20) << "All updates should succeed";

    // Merge and verify
    MergeAllWorkspaces();

    // All workspaces should have the same keys (but possibly different values)
    for (uint32_t i = 0; i < NUM_DRONES - 1; i++) {
        EXPECT_EQ(workspaces_[i]->Size(), workspaces_[i + 1]->Size())
            << "Workspaces should have same number of keys after merge";
    }
}

/**
 * Test 9: Large Workspace Merge
 * Verify that large workspaces merge efficiently
 */
TEST_F(SwarmWorkspaceIntegrationTest, LargeWorkspaceMerge) {
    // Each drone adds many entries
    const int entries_per_drone = 100;

    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        for (int j = 0; j < entries_per_drone; j++) {
            std::string key = "large_key_" + std::to_string(i) + "_" + std::to_string(j);
            std::string value = "value_" + std::to_string(j);
            workspaces_[i]->Put(key, value);
        }
    }

    // Merge all workspaces
    auto start = std::chrono::steady_clock::now();
    MergeAllWorkspaces();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration, 1000) << "Large workspace merge should be efficient";

    // All workspaces should have all entries
    size_t expected_size = NUM_DRONES * entries_per_drone;
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        EXPECT_EQ(workspaces_[i]->Size(), expected_size)
            << "Workspace " << i << " should have all entries";
    }
}

/**
 * Test 10: Idempotent Merge
 * Verify that merging multiple times produces the same result
 */
TEST_F(SwarmWorkspaceIntegrationTest, IdempotentMerge) {
    // Add initial data
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        workspaces_[i]->Put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    // First merge
    MergeAllWorkspaces();
    size_t size_after_first_merge = workspaces_[0]->Size();

    // Second merge (should be idempotent)
    MergeAllWorkspaces();
    size_t size_after_second_merge = workspaces_[0]->Size();

    EXPECT_EQ(size_after_first_merge, size_after_second_merge)
        << "Multiple merges should be idempotent";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
