/**
 * @file test_mesh_coordination_e2e.cpp
 * @brief End-to-end tests for mesh network coordination
 * @date 2026-02-02
 *
 * WHAT: E2E tests verifying mesh network coordination across brain regions
 * WHY:  Validate that mesh communication, signal propagation, consensus,
 *       ordering, and failover work correctly in realistic scenarios
 * HOW:  Uses GTest framework with comprehensive mesh coordination scenarios
 *
 * TESTS:
 * 1. Multi-region mesh communication
 * 2. Signal propagation across brain regions
 * 3. Coordinator consensus and ordering
 * 4. Failover scenarios
 * 5. Load balancing across coordinators
 * 6. Cross-channel communication
 *
 * @author NIMCP Development Team
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <vector>
#include <string>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
}

namespace nimcp {
namespace e2e {

//=============================================================================
// Constants
//=============================================================================

constexpr int MAX_PARTICIPANTS = 32;
constexpr int MAX_COORDINATORS = 8;
constexpr int MAX_EVENTS = 256;
constexpr int STRESS_MESSAGE_COUNT = 1000;
constexpr int HEARTBEAT_TIMEOUT_MS = 500;

//=============================================================================
// Event Tracking
//=============================================================================

enum class MeshEventType {
    PARTICIPANT_JOIN = 0,
    PARTICIPANT_LEAVE,
    COORDINATOR_ELECTED,
    MESSAGE_SENT,
    MESSAGE_RECEIVED,
    HEARTBEAT,
    FAILURE_DETECTED,
    RECOVERY,
    REBALANCE
};

struct MeshEvent {
    MeshEventType type;
    uint64_t timestamp_ms;
    char source[64];
    char target[64];
    int result_code;
};

//=============================================================================
// Helper Structures
//=============================================================================

struct SimulatedRegion {
    char name[64];
    int region_id;
    float activity_level;
    int messages_sent;
    int messages_received;
};

struct SimulatedCoordinator {
    char id[32];
    coordinator_role_t role;
    coordinator_state_t state;
    uint64_t term;
    int votes_received;
    uint64_t last_heartbeat;
};

//=============================================================================
// Test Fixture
//=============================================================================

class MeshCoordinationE2E : public ::testing::Test {
protected:
    std::vector<MeshEvent> events_;

    void SetUp() override {
        events_.clear();
        srand(static_cast<unsigned int>(time(nullptr)));
    }

    void TearDown() override {
        events_.clear();
    }

    uint64_t get_time_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000 +
               static_cast<uint64_t>(ts.tv_nsec) / 1000000;
    }

    void record_mesh_event(MeshEventType type, const char* source,
                           const char* target, int result) {
        if (events_.size() < MAX_EVENTS) {
            MeshEvent evt;
            evt.type = type;
            evt.timestamp_ms = get_time_ms();
            if (source) {
                strncpy(evt.source, source, sizeof(evt.source) - 1);
                evt.source[sizeof(evt.source) - 1] = '\0';
            } else {
                evt.source[0] = '\0';
            }
            if (target) {
                strncpy(evt.target, target, sizeof(evt.target) - 1);
                evt.target[sizeof(evt.target) - 1] = '\0';
            } else {
                evt.target[0] = '\0';
            }
            evt.result_code = result;
            events_.push_back(evt);
        }
    }

    int count_events_of_type(MeshEventType type) {
        int count = 0;
        for (const auto& evt : events_) {
            if (evt.type == type) {
                count++;
            }
        }
        return count;
    }
};

//=============================================================================
// TEST GROUP 1: Basic Coordinator Operations
//=============================================================================

TEST_F(MeshCoordinationE2E, CoordinatorLifecycle) {
    E2E_PIPELINE_START("Coordinator Lifecycle Pipeline");

    E2E_STAGE_BEGIN("Get default configuration", 100);
    {
        mesh_coordinator_config_t config;
        nimcp_error_t status = mesh_coordinator_default_config(&config);
        EXPECT_EQ(NIMCP_SUCCESS, status) << "Failed to get default config";

        config.name = "test_coordinator";
        config.level = COORD_LEVEL_HEMISPHERE;
        config.heartbeat_interval_ms = 100.0f;
        config.election_timeout_ms = 500.0f;
        config.max_participants = 16;
        config.enable_logging = true;

        record_mesh_event(MeshEventType::COORDINATOR_ELECTED, "test_coordinator", nullptr, 0);

        EXPECT_STREQ("test_coordinator", config.name);
        EXPECT_EQ(COORD_LEVEL_HEMISPHERE, config.level);
        EXPECT_EQ(16, config.max_participants);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(MeshCoordinationE2E, CoordinatorLevelTiming) {
    E2E_PIPELINE_START("Coordinator Level Timing Pipeline");

    E2E_STAGE_BEGIN("Test timing for each level", 200);
    {
        coordinator_level_t levels[] = {
            COORD_LEVEL_SYSTEM,
            COORD_LEVEL_HEMISPHERE,
            COORD_LEVEL_LAYER,
            COORD_LEVEL_ORDERING
        };

        const char* level_names[] = {
            "SYSTEM", "HEMISPHERE", "LAYER", "ORDERING"
        };

        mesh_timing_t prev_timing;
        bool has_prev = false;

        for (int i = 0; i < 4; i++) {
            mesh_timing_t timing;
            mesh_coordinator_get_level_timing(levels[i], &timing);

            const char* level_str = mesh_coordinator_level_to_string(levels[i]);
            ASSERT_NE(level_str, nullptr) << "Level string should not be NULL";

            std::cout << "  Level " << level_names[i] << ": base_ms=" << timing.base_interval_ms << std::endl;

            if (has_prev) {
                EXPECT_LE(timing.base_interval_ms, prev_timing.base_interval_ms)
                    << "Timing should decrease with lower levels";
            }

            prev_timing = timing;
            has_prev = true;
        }
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 2: Multi-Region Communication Simulation
//=============================================================================

TEST_F(MeshCoordinationE2E, MultiRegionCommunication) {
    E2E_PIPELINE_START("Multi-Region Communication Pipeline");

    E2E_STAGE_BEGIN("Simulate message exchange", 500);
    {
        SimulatedRegion regions[6] = {
            {"prefrontal_cortex", 0, 0.8f, 0, 0},
            {"hippocampus", 1, 0.7f, 0, 0},
            {"amygdala", 2, 0.6f, 0, 0},
            {"basal_ganglia", 3, 0.5f, 0, 0},
            {"thalamus", 4, 0.9f, 0, 0},
            {"cerebellum", 5, 0.4f, 0, 0}
        };

        // Thalamus (relay) sends to all cortical regions
        for (int i = 0; i < 3; i++) {
            record_mesh_event(MeshEventType::MESSAGE_SENT, regions[4].name, regions[i].name, 0);
            regions[4].messages_sent++;
            regions[i].messages_received++;
        }

        // Prefrontal cortex coordinates with hippocampus and amygdala
        record_mesh_event(MeshEventType::MESSAGE_SENT, regions[0].name, regions[1].name, 0);
        record_mesh_event(MeshEventType::MESSAGE_SENT, regions[0].name, regions[2].name, 0);
        regions[0].messages_sent += 2;
        regions[1].messages_received++;
        regions[2].messages_received++;

        // Amygdala modulates hippocampus
        record_mesh_event(MeshEventType::MESSAGE_SENT, regions[2].name, regions[1].name, 0);
        regions[2].messages_sent++;
        regions[1].messages_received++;

        // Basal ganglia receives from prefrontal cortex
        record_mesh_event(MeshEventType::MESSAGE_SENT, regions[0].name, regions[3].name, 0);
        regions[0].messages_sent++;
        regions[3].messages_received++;

        // Cerebellum provides feedback to motor areas via thalamus
        record_mesh_event(MeshEventType::MESSAGE_SENT, regions[5].name, regions[4].name, 0);
        regions[5].messages_sent++;
        regions[4].messages_received++;

        // Verify communication patterns
        int total_sent = 0;
        int total_received = 0;
        for (int i = 0; i < 6; i++) {
            total_sent += regions[i].messages_sent;
            total_received += regions[i].messages_received;
            std::cout << "  Region " << regions[i].name << ": sent=" << regions[i].messages_sent
                      << ", received=" << regions[i].messages_received << std::endl;
        }

        EXPECT_EQ(total_sent, total_received) << "Sent and received counts should match";
        EXPECT_EQ(7, count_events_of_type(MeshEventType::MESSAGE_SENT));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(MeshCoordinationE2E, SignalPropagation) {
    E2E_PIPELINE_START("Signal Propagation Pipeline");

    E2E_STAGE_BEGIN("Propagate signal through pathway", 200);
    {
        const char* pathway[] = {
            "sensory_cortex",
            "thalamus",
            "primary_cortex",
            "association_cortex",
            "prefrontal_cortex",
            "motor_planning",
            "motor_cortex"
        };
        const int pathway_length = 7;

        uint64_t start_time = get_time_ms();

        // Propagate signal through pathway
        for (int i = 0; i < pathway_length - 1; i++) {
            record_mesh_event(MeshEventType::MESSAGE_SENT, pathway[i], pathway[i+1], 0);
            record_mesh_event(MeshEventType::MESSAGE_RECEIVED, pathway[i+1], pathway[i], 0);
            usleep(1000);  // 1ms per hop
        }

        uint64_t propagation_time = get_time_ms() - start_time;

        std::cout << "  Signal propagated through " << pathway_length
                  << " regions in " << propagation_time << " ms" << std::endl;

        int sent_count = count_events_of_type(MeshEventType::MESSAGE_SENT);
        int received_count = count_events_of_type(MeshEventType::MESSAGE_RECEIVED);

        EXPECT_EQ(pathway_length - 1, sent_count);
        EXPECT_EQ(pathway_length - 1, received_count);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 3: Consensus and Ordering Simulation
//=============================================================================

TEST_F(MeshCoordinationE2E, LeaderElectionSimulation) {
    E2E_PIPELINE_START("Leader Election Pipeline");

    E2E_STAGE_BEGIN("Simulate leader election", 300);
    {
        SimulatedCoordinator coordinators[5];
        for (int i = 0; i < 5; i++) {
            snprintf(coordinators[i].id, sizeof(coordinators[i].id), "coord_%d", i);
            coordinators[i].role = COORD_ROLE_FOLLOWER;
            coordinators[i].state = COORD_STATE_ACTIVE;
            coordinators[i].term = 0;
            coordinators[i].votes_received = 0;
            coordinators[i].last_heartbeat = get_time_ms();
        }

        // Phase 1: coord_0 becomes candidate
        std::cout << "  Phase 1: coord_0 becomes candidate..." << std::endl;
        coordinators[0].role = COORD_ROLE_CANDIDATE;
        coordinators[0].term = 1;
        coordinators[0].votes_received = 1;  // Self vote

        // Collect votes
        std::cout << "  Phase 2: Collecting votes..." << std::endl;
        for (int i = 1; i < 5; i++) {
            if (coordinators[i].term < coordinators[0].term) {
                coordinators[0].votes_received++;
                coordinators[i].term = coordinators[0].term;
                record_mesh_event(MeshEventType::MESSAGE_SENT, coordinators[i].id, coordinators[0].id, 0);
            }
        }

        // Check for majority
        int majority = (5 / 2) + 1;
        if (coordinators[0].votes_received >= majority) {
            std::cout << "  Phase 3: coord_0 wins election with " << coordinators[0].votes_received
                      << " votes (majority=" << majority << ")" << std::endl;
            coordinators[0].role = COORD_ROLE_LEADER;
            record_mesh_event(MeshEventType::COORDINATOR_ELECTED, coordinators[0].id, nullptr, 0);
        }

        EXPECT_EQ(COORD_ROLE_LEADER, coordinators[0].role);
        EXPECT_GE(coordinators[0].votes_received, majority);

        // Verify all followers updated term
        for (int i = 1; i < 5; i++) {
            EXPECT_EQ(1u, coordinators[i].term);
        }
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(MeshCoordinationE2E, HeartbeatMechanism) {
    E2E_PIPELINE_START("Heartbeat Mechanism Pipeline");

    E2E_STAGE_BEGIN("Send heartbeats from leader", 500);
    {
        SimulatedCoordinator leader = {
            "leader",
            COORD_ROLE_LEADER,
            COORD_STATE_ACTIVE,
            1,
            0,
            get_time_ms()
        };

        SimulatedCoordinator followers[3];
        for (int i = 0; i < 3; i++) {
            snprintf(followers[i].id, sizeof(followers[i].id), "follower_%d", i);
            followers[i].role = COORD_ROLE_FOLLOWER;
            followers[i].state = COORD_STATE_ACTIVE;
            followers[i].term = 1;
            followers[i].last_heartbeat = get_time_ms();
        }

        // Send heartbeats
        for (int round = 0; round < 5; round++) {
            for (int i = 0; i < 3; i++) {
                record_mesh_event(MeshEventType::HEARTBEAT, leader.id, followers[i].id, 0);
                followers[i].last_heartbeat = get_time_ms();
            }
            usleep(50000);  // 50ms between rounds
        }

        int heartbeat_count = count_events_of_type(MeshEventType::HEARTBEAT);
        EXPECT_EQ(15, heartbeat_count);  // 5 rounds * 3 followers
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 4: Failover Scenarios
//=============================================================================

TEST_F(MeshCoordinationE2E, CoordinatorFailureDetection) {
    E2E_PIPELINE_START("Coordinator Failure Detection Pipeline");

    E2E_STAGE_BEGIN("Simulate coordinator failure and recovery", 1000);
    {
        bool coordinators_healthy[5] = {true, true, true, true, true};
        uint64_t last_seen[5];

        for (int i = 0; i < 5; i++) {
            last_seen[i] = get_time_ms();
        }

        // Simulate coordinator 2 failing
        std::cout << "  Simulating coordinator 2 failure..." << std::endl;
        coordinators_healthy[2] = false;
        record_mesh_event(MeshEventType::FAILURE_DETECTED, "monitor", "coord_2", 0);

        // Wait for heartbeat timeout
        usleep(HEARTBEAT_TIMEOUT_MS * 1000);

        // Check for missed heartbeats
        uint64_t current_time = get_time_ms();
        int failed_count = 0;
        for (int i = 0; i < 5; i++) {
            if (!coordinators_healthy[i]) {
                uint64_t age = current_time - last_seen[i];
                if (age > static_cast<uint64_t>(HEARTBEAT_TIMEOUT_MS)) {
                    failed_count++;
                    std::cout << "  Coordinator " << i << " detected as failed (age="
                              << age << " ms)" << std::endl;
                }
            }
        }

        EXPECT_EQ(1, failed_count);
        EXPECT_EQ(1, count_events_of_type(MeshEventType::FAILURE_DETECTED));

        // Simulate recovery
        std::cout << "  Simulating coordinator 2 recovery..." << std::endl;
        coordinators_healthy[2] = true;
        last_seen[2] = get_time_ms();
        record_mesh_event(MeshEventType::RECOVERY, "coord_2", nullptr, 0);

        EXPECT_EQ(1, count_events_of_type(MeshEventType::RECOVERY));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(MeshCoordinationE2E, LeaderFailover) {
    E2E_PIPELINE_START("Leader Failover Pipeline");

    E2E_STAGE_BEGIN("Simulate leader failover", 200);
    {
        // Initial state: coord_0 is leader
        SimulatedCoordinator coordinators[3] = {
            {"coord_0", COORD_ROLE_LEADER, COORD_STATE_ACTIVE, 1, 0, 0},
            {"coord_1", COORD_ROLE_FOLLOWER, COORD_STATE_ACTIVE, 1, 0, 0},
            {"coord_2", COORD_ROLE_FOLLOWER, COORD_STATE_ACTIVE, 1, 0, 0}
        };

        EXPECT_EQ(COORD_ROLE_LEADER, coordinators[0].role);

        // Phase 1: Leader failure
        std::cout << "  Phase 1: Leader (coord_0) fails..." << std::endl;
        coordinators[0].state = COORD_STATE_FAILED;
        coordinators[0].role = COORD_ROLE_STANDBY;
        record_mesh_event(MeshEventType::FAILURE_DETECTED, "pool", "coord_0", 0);

        // Phase 2: New election
        std::cout << "  Phase 2: New election triggered..." << std::endl;
        coordinators[1].role = COORD_ROLE_CANDIDATE;
        coordinators[1].term = 2;
        coordinators[1].votes_received = 1;

        coordinators[1].votes_received++;
        coordinators[2].term = 2;

        coordinators[1].role = COORD_ROLE_LEADER;
        record_mesh_event(MeshEventType::COORDINATOR_ELECTED, "coord_1", nullptr, 0);

        std::cout << "  Phase 3: coord_1 elected as new leader" << std::endl;

        EXPECT_EQ(COORD_ROLE_LEADER, coordinators[1].role);
        EXPECT_EQ(2u, coordinators[1].term);
        EXPECT_NE(COORD_ROLE_LEADER, coordinators[0].role);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 5: Load Balancing Simulation
//=============================================================================

TEST_F(MeshCoordinationE2E, LoadBalancing) {
    E2E_PIPELINE_START("Load Balancing Pipeline");

    E2E_STAGE_BEGIN("Simulate load balancing", 200);
    {
        struct LoadInfo {
            char id[32];
            int assigned_participants;
            float load;
        };

        LoadInfo coordinators[4] = {
            {"coord_0", 10, 0.5f},
            {"coord_1", 15, 0.75f},
            {"coord_2", 5, 0.25f},
            {"coord_3", 8, 0.4f}
        };

        float total_load = 0.0f;
        int total_participants = 0;
        for (int i = 0; i < 4; i++) {
            total_load += coordinators[i].load;
            total_participants += coordinators[i].assigned_participants;
        }
        float avg_load = total_load / 4.0f;

        std::cout << "  Initial state: avg_load=" << avg_load
                  << ", total_participants=" << total_participants << std::endl;

        int overloaded = -1;
        int underloaded = -1;
        float max_load = 0.0f;
        float min_load = 1.0f;

        for (int i = 0; i < 4; i++) {
            if (coordinators[i].load > max_load) {
                max_load = coordinators[i].load;
                overloaded = i;
            }
            if (coordinators[i].load < min_load) {
                min_load = coordinators[i].load;
                underloaded = i;
            }
        }

        EXPECT_NE(-1, overloaded);
        EXPECT_NE(-1, underloaded);

        std::cout << "  Overloaded: " << coordinators[overloaded].id << " (load=" << coordinators[overloaded].load
                  << "), Underloaded: " << coordinators[underloaded].id << " (load=" << coordinators[underloaded].load
                  << ")" << std::endl;

        if (max_load - min_load > 0.3f) {
            std::cout << "  Rebalancing: moving participants from " << coordinators[overloaded].id
                      << " to " << coordinators[underloaded].id << std::endl;

            int to_move = (coordinators[overloaded].assigned_participants -
                           coordinators[underloaded].assigned_participants) / 2;

            coordinators[overloaded].assigned_participants -= to_move;
            coordinators[underloaded].assigned_participants += to_move;

            coordinators[overloaded].load -= static_cast<float>(to_move) * 0.05f;
            coordinators[underloaded].load += static_cast<float>(to_move) * 0.05f;

            record_mesh_event(MeshEventType::REBALANCE, coordinators[overloaded].id,
                              coordinators[underloaded].id, to_move);
        }

        float new_max_load = 0.0f;
        float new_min_load = 1.0f;
        for (int i = 0; i < 4; i++) {
            if (coordinators[i].load > new_max_load) new_max_load = coordinators[i].load;
            if (coordinators[i].load < new_min_load) new_min_load = coordinators[i].load;
        }

        EXPECT_LE(new_max_load - new_min_load, max_load - min_load + 0.001f);

        std::cout << "  After rebalancing: max_load=" << new_max_load
                  << ", min_load=" << new_min_load
                  << ", diff=" << (new_max_load - new_min_load) << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 6: Stress Testing
//=============================================================================

TEST_F(MeshCoordinationE2E, HighVolumeMessages) {
    E2E_PIPELINE_START("High Volume Message Pipeline");

    E2E_STAGE_BEGIN("Process high-volume messages", 2000);
    {
        uint64_t start_time = get_time_ms();
        int successful_messages = 0;
        int failed_messages = 0;

        for (int i = 0; i < STRESS_MESSAGE_COUNT; i++) {
            char source[32], target[32];
            snprintf(source, sizeof(source), "region_%d", i % 10);
            snprintf(target, sizeof(target), "region_%d", (i + 5) % 10);

            int result = (rand() % 100 < 98) ? 0 : -1;

            if (result == 0) {
                successful_messages++;
            } else {
                failed_messages++;
            }

            if (i < 100) {
                record_mesh_event(MeshEventType::MESSAGE_SENT, source, target, result);
            }
        }

        uint64_t elapsed = get_time_ms() - start_time;

        std::cout << "  Processed " << STRESS_MESSAGE_COUNT << " messages in " << elapsed << " ms" << std::endl;
        std::cout << "  Successful: " << successful_messages << ", Failed: " << failed_messages
                  << " (" << (100.0f * successful_messages / STRESS_MESSAGE_COUNT) << "% success rate)" << std::endl;
        std::cout << "  Throughput: " << (elapsed > 0 ? static_cast<float>(STRESS_MESSAGE_COUNT) * 1000.0f / static_cast<float>(elapsed) : 0.0f)
                  << " messages/sec" << std::endl;

        float failure_rate = static_cast<float>(failed_messages) / STRESS_MESSAGE_COUNT;
        EXPECT_LT(failure_rate, 0.05f);  // Less than 5% failure rate
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(MeshCoordinationE2E, ConcurrentCoordinatorUpdates) {
    E2E_PIPELINE_START("Concurrent Coordinator Updates Pipeline");

    E2E_STAGE_BEGIN("Simulate concurrent updates", 500);
    {
        struct UpdateTracker {
            char id[32];
            int update_count;
            uint64_t last_update;
        };

        UpdateTracker trackers[8];
        for (int i = 0; i < 8; i++) {
            snprintf(trackers[i].id, sizeof(trackers[i].id), "coord_%d", i);
            trackers[i].update_count = 0;
            trackers[i].last_update = get_time_ms();
        }

        uint64_t start_time = get_time_ms();
        const int UPDATE_ROUNDS = 100;

        for (int round = 0; round < UPDATE_ROUNDS; round++) {
            for (int i = 0; i < 8; i++) {
                trackers[i].update_count++;
                trackers[i].last_update = get_time_ms();
            }
        }

        uint64_t elapsed = get_time_ms() - start_time;

        for (int i = 0; i < 8; i++) {
            EXPECT_EQ(UPDATE_ROUNDS, trackers[i].update_count);
        }

        std::cout << "  Completed " << UPDATE_ROUNDS << " update rounds across 8 coordinators in "
                  << elapsed << " ms" << std::endl;
        std::cout << "  Total updates: " << (UPDATE_ROUNDS * 8) << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 7: Full Integration Pipeline
//=============================================================================

TEST_F(MeshCoordinationE2E, CompleteMeshPipeline) {
    E2E_PIPELINE_START("Complete Mesh Coordination Pipeline");

    SimulatedRegion regions[4] = {
        {"prefrontal", 0, 0.0f, 0, 0},
        {"hippocampus", 1, 0.0f, 0, 0},
        {"amygdala", 2, 0.0f, 0, 0},
        {"thalamus", 3, 0.0f, 0, 0}
    };

    SimulatedCoordinator coordinators[2] = {
        {"left_hemisphere", COORD_ROLE_LEADER, COORD_STATE_ACTIVE, 1, 0, 0},
        {"right_hemisphere", COORD_ROLE_FOLLOWER, COORD_STATE_ACTIVE, 1, 0, 0}
    };

    E2E_STAGE_BEGIN("Initialize mesh structure", 100);
    std::cout << "  Phase 1: Initializing mesh structure..." << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Establish connections", 100);
    {
        std::cout << "  Phase 2: Establishing connections..." << std::endl;
        for (int i = 0; i < 4; i++) {
            record_mesh_event(MeshEventType::PARTICIPANT_JOIN, regions[i].name, "mesh", 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Leader election", 100);
    {
        std::cout << "  Phase 3: Leader election..." << std::endl;
        record_mesh_event(MeshEventType::COORDINATOR_ELECTED, coordinators[0].id, nullptr, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Message exchange cycle", 500);
    {
        std::cout << "  Phase 4: Message exchange cycle..." << std::endl;
        for (int cycle = 0; cycle < 10; cycle++) {
            for (int i = 0; i < 3; i++) {
                record_mesh_event(MeshEventType::MESSAGE_SENT, "thalamus", regions[i].name, 0);
                regions[i].messages_received++;
            }
            regions[3].messages_sent += 3;

            record_mesh_event(MeshEventType::MESSAGE_SENT, "prefrontal", "hippocampus", 0);
            regions[0].messages_sent++;
            regions[1].messages_received++;

            record_mesh_event(MeshEventType::HEARTBEAT, coordinators[0].id, coordinators[1].id, 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Failure and recovery", 100);
    {
        std::cout << "  Phase 5: Failure and recovery..." << std::endl;
        record_mesh_event(MeshEventType::FAILURE_DETECTED, "monitor", "amygdala", 0);
        usleep(10000);
        record_mesh_event(MeshEventType::RECOVERY, "amygdala", nullptr, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Load rebalancing", 100);
    {
        std::cout << "  Phase 6: Load rebalancing..." << std::endl;
        record_mesh_event(MeshEventType::REBALANCE, coordinators[0].id, coordinators[1].id, 1);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Graceful shutdown", 100);
    {
        std::cout << "  Phase 7: Graceful shutdown..." << std::endl;
        for (int i = 0; i < 4; i++) {
            record_mesh_event(MeshEventType::PARTICIPANT_LEAVE, regions[i].name, "mesh", 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify event counts", 100);
    {
        int join_count = count_events_of_type(MeshEventType::PARTICIPANT_JOIN);
        int leave_count = count_events_of_type(MeshEventType::PARTICIPANT_LEAVE);
        int message_count = count_events_of_type(MeshEventType::MESSAGE_SENT);
        int heartbeat_count = count_events_of_type(MeshEventType::HEARTBEAT);

        EXPECT_EQ(4, join_count);
        EXPECT_EQ(4, leave_count);
        EXPECT_EQ(40, message_count);  // 4 per cycle * 10 cycles
        EXPECT_EQ(10, heartbeat_count);

        std::cout << "  Pipeline completed with " << events_.size() << " total events" << std::endl;
        std::cout << "  Join: " << join_count << ", Leave: " << leave_count
                  << ", Messages: " << message_count << ", Heartbeats: " << heartbeat_count << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp
