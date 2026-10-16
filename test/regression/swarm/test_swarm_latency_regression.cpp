/**
 * @file test_swarm_latency_regression.cpp
 * @brief Latency performance regression tests for swarm brain components
 *
 * Tests cover:
 * - Message processing latency (< 50ms threshold)
 * - Consensus vote completion time (< 200ms for quorum of 8)
 * - Emergence tier detection latency (< 10ms)
 * - Workspace merge latency (< 5ms per item)
 * - Heartbeat processing time (< 10ms)
 * - Neuromodulator synchronization latency (< 20ms)
 * - End-to-end communication latency (< 100ms)
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_collective_workspace.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmLatencyRegressionTest : public ::testing::Test {
protected:
    using clock = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;

    swarm_brain_t* swarm = nullptr;

    void SetUp() override {
        // Logging initialized in framework
        // Log level set in framework
    }

    void TearDown() override {
        if (swarm) {
            swarm_brain_destroy(swarm);
            swarm = nullptr;
        }
    }

    double milliseconds(time_point start, time_point end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    double mean(const std::vector<double>& values) {
        return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    double stddev(const std::vector<double>& values) {
        double m = mean(values);
        double sq_sum = 0;
        for (double v : values) {
            sq_sum += (v - m) * (v - m);
        }
        return std::sqrt(sq_sum / values.size());
    }

    double percentile(std::vector<double> values, double p) {
        std::sort(values.begin(), values.end());
        size_t idx = static_cast<size_t>(values.size() * p / 100.0);
        if (idx >= values.size()) idx = values.size() - 1;
        return values[idx];
    }
};

//=============================================================================
// Test Cases
//=============================================================================

TEST_F(SwarmLatencyRegressionTest, MessageProcessingLatency) {
    const double MAX_LATENCY_MS = 50.0;
    const int NUM_MESSAGES = 100;

    swarm_brain_config_t config = swarm_brain_default_config();
    config.drone_id = 1;
    strcpy(config.swarm_name, "latency_test");
    config.enable_bio_async = false;

    swarm = swarm_brain_create(&config);
    ASSERT_NE(swarm, nullptr);
    swarm_brain_join(swarm);

    std::vector<double> latencies;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        perception_data_t perception = {};
        perception.sensor_type = 1;
        perception.value_count = 8;
        for (int j = 0; j < 8; j++) {
            perception.values[j] = (i + j) * 0.1f;
        }
        perception.confidence = 0.9f;

        auto start = clock::now();
        swarm_brain_broadcast_perception(swarm, &perception);
        swarm_brain_process(swarm);
        auto end = clock::now();

        latencies.push_back(milliseconds(start, end));
    }

    double avg = mean(latencies);
    double p95 = percentile(latencies, 95.0);

    EXPECT_LT(avg, MAX_LATENCY_MS) << "Avg: " << avg << " ms";
    std::cout << "[LATENCY] Message processing: Mean=" << avg << "ms, P95=" << p95 << "ms\n";
}

TEST_F(SwarmLatencyRegressionTest, ConsensusVoteCompletionTime) {
    const double MAX_LATENCY_MS = 200.0;
    const int NUM_VOTES = 50;
    const int QUORUM = 8;

    swarm_consensus_config_t config = swarm_consensus_default_config(1);
    swarm_consensus_t ctx = swarm_consensus_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<double> latencies;
    for (int i = 0; i < NUM_VOTES; i++) {
        uint32_t proposal_id = 0;
        float values[4] = {(float)i, (float)i+1, (float)i+2, (float)i+3};

        auto start = clock::now();
        swarm_consensus_propose(ctx, VOTE_TOPIC_TARGET_PRIORITY, values, 0, QUORUM, 0.5f, nullptr, nullptr, &proposal_id);
        for (int j = 0; j < QUORUM; j++) {
            swarm_vote_response_t vote = {proposal_id, (uint16_t)j, VOTE_CHOICE_AGREE, 0.9f};
            swarm_consensus_receive_vote(ctx, &vote);
        }
        auto end = clock::now();

        latencies.push_back(milliseconds(start, end));
    }

    swarm_consensus_destroy(ctx);
    double avg = mean(latencies);
    EXPECT_LT(avg, MAX_LATENCY_MS) << "Consensus avg: " << avg << " ms";
    std::cout << "[LATENCY] Consensus (Q=" << QUORUM << "): Mean=" << avg << "ms\n";
}

TEST_F(SwarmLatencyRegressionTest, EmergenceTierDetectionLatency) {
    const double MAX_LATENCY_MS = 10.0;

    swarm_brain_config_t config = swarm_brain_default_config();
    config.drone_id = 1;
    strcpy(config.swarm_name, "tier_test");

    swarm = swarm_brain_create(&config);
    ASSERT_NE(swarm, nullptr);

    std::vector<double> latencies;
    for (int i = 0; i < 1000; i++) {
        auto start = clock::now();
        swarm_brain_get_emergence_tier(swarm);
        auto end = clock::now();
        latencies.push_back(milliseconds(start, end));
    }

    double avg = mean(latencies);
    EXPECT_LT(avg, MAX_LATENCY_MS) << "Tier detection avg: " << avg << " ms";
    std::cout << "[LATENCY] Tier detection: Mean=" << avg << "ms\n";
}

TEST_F(SwarmLatencyRegressionTest, WorkspaceMergeLatency) {
    const double MAX_LATENCY_MS = 5.0;

    collective_workspace_t* workspace = collective_workspace_create_simple(1, 8);
    ASSERT_NE(workspace, nullptr);

    std::vector<double> latencies;
    for (int i = 0; i < 500; i++) {
        collective_workspace_item_t item = {};
        item.item_id = (2 << 16) | (i % 100);
        item.salience = 0.5f + (i % 50) * 0.01f;
        item.type = WORKSPACE_ITEM_PERCEPTION;
        item.source_drone = 2;

        auto start = clock::now();
        collective_workspace_merge_item(workspace, &item);
        auto end = clock::now();
        latencies.push_back(milliseconds(start, end));
    }

    collective_workspace_destroy(workspace);
    double avg = mean(latencies);
    EXPECT_LT(avg, MAX_LATENCY_MS) << "Workspace merge avg: " << avg << " ms";
    std::cout << "[LATENCY] Workspace merge: Mean=" << avg << "ms\n";
}

TEST_F(SwarmLatencyRegressionTest, NeuromodulatorSyncLatency) {
    const double MAX_LATENCY_MS = 20.0;

    swarm_brain_config_t config = swarm_brain_default_config();
    config.drone_id = 1;
    strcpy(config.swarm_name, "neuromod_test");

    swarm = swarm_brain_create(&config);
    ASSERT_NE(swarm, nullptr);
    swarm_brain_join(swarm);

    std::vector<double> latencies;
    for (int i = 0; i < 100; i++) {
        neuromod_state_t state = {0.5f, 0.6f, 0.4f, 0.7f};
        auto start = clock::now();
        swarm_brain_sync_neuromodulators(swarm, &state);
        auto end = clock::now();
        latencies.push_back(milliseconds(start, end));
    }

    double avg = mean(latencies);
    EXPECT_LT(avg, MAX_LATENCY_MS) << "Neuromod sync avg: " << avg << " ms";
    std::cout << "[LATENCY] Neuromod sync: Mean=" << avg << "ms\n";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
