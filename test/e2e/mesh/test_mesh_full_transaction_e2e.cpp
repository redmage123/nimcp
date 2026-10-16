/**
 * @file test_mesh_full_transaction_e2e.cpp
 * @brief End-to-End Tests for Complete Mesh Network Transaction Flow
 *
 * WHAT: Tests full transaction flow through all mesh components
 * WHY:  Verify complete system integration from proposal to commit
 * HOW:  Create realistic multi-channel scenarios with all components
 *
 * TEST COVERAGE:
 * - Complete EOV flow with all components
 * - Multi-channel hierarchical coordination
 * - Cross-channel transactions with BBB/MSP
 * - GPU channel batch processing
 * - System coordinator arbitration
 * - Full consensus convergence
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

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_endorsement.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_cross_channel.h"
#include "mesh/nimcp_mesh_topology.h"
#include "mesh/nimcp_mesh_timing.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class MeshFullTransactionE2ETest : public ::testing::Test {
protected:
    // Channels (brain regions)
    mesh_channel_t* left_hemisphere_ = nullptr;
    mesh_channel_t* right_hemisphere_ = nullptr;
    mesh_channel_t* subcortical_ = nullptr;

    // Coordinator pools
    mesh_coordinator_pool_t* system_pool_ = nullptr;
    mesh_coordinator_pool_t* left_pool_ = nullptr;
    mesh_coordinator_pool_t* right_pool_ = nullptr;

    // Ordering service
    mesh_ordering_service_t* ordering_ = nullptr;

    // MSP
    mesh_msp_t* msp_ = nullptr;

    // Cross-channel router
    mesh_cross_channel_router_t* router_ = nullptr;

    // Participants
    std::vector<mesh_participant_t*> participants_;

    // Hierarchical timing
    mesh_hierarchical_timing_t* timing_ = nullptr;

    // Topology
    mesh_topology_t* topology_ = nullptr;

    void SetUp() override {
        // Create MSP
        mesh_msp_config_t msp_config;
        mesh_msp_config_init(&msp_config);
        msp_config.enable_bbb = true;
        msp_config.enable_immune = true;
        msp_ = mesh_msp_create(&msp_config);
        ASSERT_NE(msp_, nullptr);

        // Create ordering service
        mesh_ordering_config_t ord_config;
        mesh_ordering_config_init(&ord_config);
        ord_config.batch_size = 10;
        ord_config.raft_enabled = true;
        ordering_ = mesh_ordering_create(&ord_config);
        ASSERT_NE(ordering_, nullptr);

        // Create channels
        left_hemisphere_ = CreateChannel("left_hemisphere", 32);
        right_hemisphere_ = CreateChannel("right_hemisphere", 32);
        subcortical_ = CreateChannel("subcortical", 16);

        ASSERT_NE(left_hemisphere_, nullptr);
        ASSERT_NE(right_hemisphere_, nullptr);
        ASSERT_NE(subcortical_, nullptr);

        // Create system coordinator pool
        system_pool_ = CreateCoordinatorPool("system_pool", 3, nullptr);
        ASSERT_NE(system_pool_, nullptr);

        // Create hemisphere pools (children of system)
        left_pool_ = CreateCoordinatorPool("left_pool", 5, system_pool_);
        right_pool_ = CreateCoordinatorPool("right_pool", 5, system_pool_);
        ASSERT_NE(left_pool_, nullptr);
        ASSERT_NE(right_pool_, nullptr);

        // Create cross-channel router
        mesh_cross_channel_config_t router_config;
        mesh_cross_channel_config_init(&router_config);
        router_config.ordering_service = ordering_;
        router_config.msp = msp_;
        router_ = mesh_cross_channel_router_create(&router_config);
        ASSERT_NE(router_, nullptr);

        // Register channels with router
        mesh_cross_channel_router_register(router_, left_hemisphere_);
        mesh_cross_channel_router_register(router_, right_hemisphere_);
        mesh_cross_channel_router_register(router_, subcortical_);

        // Create timing
        mesh_hierarchical_timing_config_t timing_config;
        mesh_hierarchical_timing_config_init(&timing_config);
        timing_ = mesh_hierarchical_timing_create(&timing_config);
        ASSERT_NE(timing_, nullptr);

        // Create topology
        mesh_topology_config_t topo_config;
        mesh_topology_config_init(&topo_config);
        topo_config.max_nodes = 100;
        topology_ = mesh_topology_create(&topo_config);
        ASSERT_NE(topology_, nullptr);

        // Create and register participants
        CreateParticipants(30);

        // Initial elections
        mesh_coordinator_pool_elect_leader(system_pool_);
        mesh_coordinator_pool_elect_leader(left_pool_);
        mesh_coordinator_pool_elect_leader(right_pool_);
    }

    void TearDown() override {
        for (auto* p : participants_) {
            if (p) mesh_participant_destroy(p);
        }
        participants_.clear();

        if (topology_) mesh_topology_destroy(topology_);
        if (timing_) mesh_hierarchical_timing_destroy(timing_);
        if (router_) mesh_cross_channel_router_destroy(router_);
        if (left_pool_) mesh_coordinator_pool_destroy(left_pool_);
        if (right_pool_) mesh_coordinator_pool_destroy(right_pool_);
        if (system_pool_) mesh_coordinator_pool_destroy(system_pool_);
        if (subcortical_) mesh_channel_destroy(subcortical_);
        if (right_hemisphere_) mesh_channel_destroy(right_hemisphere_);
        if (left_hemisphere_) mesh_channel_destroy(left_hemisphere_);
        if (ordering_) mesh_ordering_destroy(ordering_);
        if (msp_) mesh_msp_destroy(msp_);
    }

    mesh_channel_t* CreateChannel(const char* name, size_t max_participants) {
        mesh_channel_config_t config;
        mesh_channel_config_init(&config);
        config.name = name;
        config.max_participants = max_participants;
        config.enable_gossip = true;
        config.msp = msp_;
        return mesh_channel_create(&config);
    }

    mesh_coordinator_pool_t* CreateCoordinatorPool(const char* name, size_t size,
                                                    mesh_coordinator_pool_t* parent) {
        mesh_coordinator_pool_config_t config;
        mesh_coordinator_pool_config_init(&config);
        config.pool_name = name;
        config.initial_size = size;
        config.enable_bft = true;
        config.parent_pool = parent;
        return mesh_coordinator_pool_create(&config);
    }

    void CreateParticipants(size_t count) {
        mesh_channel_t* channels[] = {left_hemisphere_, right_hemisphere_, subcortical_};
        size_t num_channels = 3;

        for (size_t i = 0; i < count; i++) {
            mesh_participant_config_t config;
            mesh_participant_config_init(&config);

            char name[32];
            snprintf(name, sizeof(name), "participant_%zu", i);
            config.name = name;
            config.type = (i % 4 == 0) ? MESH_PARTICIPANT_TYPE_ENDORSER
                                       : MESH_PARTICIPANT_TYPE_PEER;
            config.can_endorse = (i % 4 == 0);

            mesh_participant_t* p = mesh_participant_create(&config);
            if (!p) continue;

            // Issue credential
            mesh_participant_id_t pid = mesh_participant_get_id(p);
            mesh_credential_t cred;
            mesh_credential_init(&cred);
            cred.participant_id = pid;
            mesh_msp_issue_credential(msp_, &cred);

            // Assign to channel
            size_t ch_idx = i % num_channels;
            mesh_channel_add_participant(channels[ch_idx], p);

            // Grant channel access
            mesh_channel_id_t ch_id = mesh_channel_get_id(channels[ch_idx]);
            mesh_msp_authorize_channel(msp_, pid, ch_id);

            // Assign to coordinator pool
            mesh_coordinator_pool_t* pool = (ch_idx == 0) ? left_pool_ :
                                            (ch_idx == 1) ? right_pool_ : left_pool_;
            mesh_coordinator_pool_assign_participant(pool, pid);

            // Register in topology
            mesh_topology_register_node(topology_, pid, name);

            participants_.push_back(p);
        }
    }

    // Helper: Wait for condition
    template<typename Pred>
    bool WaitFor(Pred pred, uint32_t timeout_ms) {
        auto start = std::chrono::steady_clock::now();
        while (!pred()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }
};

// =============================================================================
// Full Transaction Flow E2E
// =============================================================================

TEST_F(MeshFullTransactionE2ETest, CompleteBeliefUpdateFlow) {
    // 1. PROPOSE: Create belief update transaction
    mesh_transaction_config_t tx_config;
    mesh_transaction_config_init(&tx_config);
    tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
    tx_config.channel_id = mesh_channel_get_id(left_hemisphere_);
    tx_config.proposer_id = mesh_participant_get_id(participants_[0]);
    tx_config.payload = "belief:visual_pattern:0.85";
    tx_config.payload_size = 26;

    mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
    ASSERT_NE(tx, nullptr);

    // 2. MSP VALIDATION
    nimcp_error_t err = mesh_msp_validate_transaction(msp_, tx);
    ASSERT_EQ(err, NIMCP_OK) << "MSP validation failed";

    // 3. ENDORSEMENT: Collect endorsements from endorser participants
    size_t endorsement_count = 0;
    for (auto* p : participants_) {
        mesh_participant_info_t info;
        mesh_participant_get_info(p, &info);
        if (info.can_endorse && endorsement_count < 3) {
            mesh_endorsement_t endorsement;
            mesh_endorsement_init(&endorsement);
            endorsement.endorser_id = info.participant_id;
            endorsement.approved = true;
            endorsement.timestamp_ms = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            err = mesh_transaction_add_endorsement(tx, &endorsement);
            if (err == NIMCP_OK) endorsement_count++;
        }
    }
    ASSERT_GE(endorsement_count, 3u) << "Not enough endorsements";

    // 4. ORDERING: Submit to ordering service
    std::atomic<bool> committed{false};
    mesh_tx_status_t final_status = MESH_TX_STATUS_PENDING;

    err = mesh_ordering_submit(ordering_, tx, [](const mesh_transaction_t* t,
                                                  mesh_tx_status_t status,
                                                  nimcp_error_t e,
                                                  void* ctx) {
        auto* s = static_cast<mesh_tx_status_t*>(ctx);
        *s = status;
    }, &final_status);
    ASSERT_EQ(err, NIMCP_OK);

    // 5. FLUSH: Force batch processing
    mesh_ordering_flush(ordering_);

    // 6. WAIT for commit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 7. VERIFY: Check transaction was committed
    mesh_transaction_info_t tx_info;
    err = mesh_transaction_get_info(tx, &tx_info);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_EQ(tx_info.status, MESH_TX_STATUS_COMMITTED);
    EXPECT_GT(tx_info.sequence_number, 0u);
    EXPECT_GT(tx_info.block_number, 0u);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshFullTransactionE2ETest, CrossChannelTransactionWithRouter) {
    // Grant cross-channel access
    mesh_participant_id_t pid = mesh_participant_get_id(participants_[0]);
    mesh_channel_id_t left_id = mesh_channel_get_id(left_hemisphere_);
    mesh_channel_id_t right_id = mesh_channel_get_id(right_hemisphere_);

    mesh_msp_authorize_channel(msp_, pid, left_id);
    mesh_msp_authorize_channel(msp_, pid, right_id);

    // Create cross-channel transaction
    mesh_cross_transaction_t cross_tx;
    mesh_cross_transaction_init(&cross_tx);
    cross_tx.source_channel_id = left_id;
    cross_tx.target_channel_id = right_id;
    cross_tx.proposer_id = pid;
    cross_tx.payload = "cross_belief:spatial_visual:0.9";
    cross_tx.payload_size = 31;

    // Submit via router
    nimcp_error_t err = mesh_cross_channel_router_submit(router_, &cross_tx);
    ASSERT_EQ(err, NIMCP_OK);

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check status
    mesh_cross_tx_status_t status;
    err = mesh_cross_channel_router_get_status(router_, cross_tx.tx_id, &status);
    ASSERT_EQ(err, NIMCP_OK);

    // Should be either committed or in valid intermediate state
    EXPECT_TRUE(status == MESH_CROSS_TX_COMMITTED ||
                status == MESH_CROSS_TX_ORDERING ||
                status == MESH_CROSS_TX_ENDORSING);

    mesh_cross_transaction_cleanup(&cross_tx);
}

// =============================================================================
// Multi-Channel Coordination E2E
// =============================================================================

TEST_F(MeshFullTransactionE2ETest, HierarchicalCoordinatorOperation) {
    // Verify hierarchical structure
    mesh_coordinator_pool_info_t system_info, left_info, right_info;

    mesh_coordinator_pool_get_info(system_pool_, &system_info);
    mesh_coordinator_pool_get_info(left_pool_, &left_info);
    mesh_coordinator_pool_get_info(right_pool_, &right_info);

    EXPECT_TRUE(system_info.has_leader);
    EXPECT_TRUE(left_info.has_leader);
    EXPECT_TRUE(right_info.has_leader);
    EXPECT_TRUE(left_info.has_parent);
    EXPECT_TRUE(right_info.has_parent);

    // Process transactions through both hemisphere channels
    for (int i = 0; i < 5; i++) {
        mesh_channel_t* ch = (i % 2 == 0) ? left_hemisphere_ : right_hemisphere_;

        mesh_transaction_config_t tx_config;
        mesh_transaction_config_init(&tx_config);
        tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        tx_config.channel_id = mesh_channel_get_id(ch);
        tx_config.proposer_id = mesh_participant_get_id(participants_[i]);

        char payload[32];
        snprintf(payload, sizeof(payload), "hemisphere_belief_%d", i);
        tx_config.payload = payload;
        tx_config.payload_size = strlen(payload);

        mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
        ASSERT_NE(tx, nullptr);

        // Add minimal endorsements
        for (int e = 0; e < 3 && e < static_cast<int>(participants_.size()); e++) {
            mesh_endorsement_t endorsement;
            mesh_endorsement_init(&endorsement);
            endorsement.endorser_id = mesh_participant_get_id(participants_[e]);
            endorsement.approved = true;
            mesh_transaction_add_endorsement(tx, &endorsement);
        }

        mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
        mesh_transaction_destroy(tx);
    }

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify ordering metrics
    mesh_ordering_metrics_t metrics;
    mesh_ordering_get_metrics(ordering_, &metrics);
    EXPECT_GE(metrics.total_transactions, 5u);
}

// =============================================================================
// Consensus Convergence E2E
// =============================================================================

TEST_F(MeshFullTransactionE2ETest, GossipConsensusConvergence) {
    // Introduce beliefs through gossip in left hemisphere
    for (int i = 0; i < 5; i++) {
        mesh_belief_t belief;
        mesh_belief_init(&belief);
        belief.belief_type = MESH_BELIEF_TYPE_STATE;
        belief.confidence = 0.7f + (i * 0.05f);

        char topic[32];
        snprintf(topic, sizeof(topic), "visual_feature_%d", i);
        strncpy(belief.topic, topic, sizeof(belief.topic) - 1);

        mesh_channel_gossip_introduce(left_hemisphere_, &belief);
    }

    // Run gossip rounds
    for (int round = 0; round < 10; round++) {
        mesh_channel_gossip_round(left_hemisphere_);
        mesh_channel_gossip_round(right_hemisphere_);
        mesh_channel_gossip_round(subcortical_);

        // Use hierarchical timing for inter-round delay
        float interval = mesh_hierarchical_timing_get_interval(
            timing_, MESH_TIMING_LEVEL_LAYER);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(interval)));
    }

    // Check consensus beliefs
    mesh_belief_set_t consensus;
    mesh_belief_set_init(&consensus);

    nimcp_error_t err = mesh_channel_get_consensus_beliefs(left_hemisphere_, &consensus);
    ASSERT_EQ(err, NIMCP_OK);

    // Should have converged on some beliefs
    EXPECT_GT(consensus.count, 0u);

    mesh_belief_set_cleanup(&consensus);
}

// =============================================================================
// Failure Recovery E2E
// =============================================================================

TEST_F(MeshFullTransactionE2ETest, CoordinatorFailureRecovery) {
    // Get initial leader
    mesh_coordinator_pool_info_t info;
    mesh_coordinator_pool_get_info(left_pool_, &info);
    ASSERT_TRUE(info.has_leader);
    size_t original_leader = info.leader_index;
    uint64_t original_term = info.current_term;

    // Submit some transactions
    for (int i = 0; i < 3; i++) {
        mesh_transaction_config_t tx_config;
        mesh_transaction_config_init(&tx_config);
        tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        tx_config.channel_id = mesh_channel_get_id(left_hemisphere_);
        tx_config.proposer_id = mesh_participant_get_id(participants_[i]);
        tx_config.payload = "pre_failure_tx";
        tx_config.payload_size = 14;

        mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
        mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
        mesh_transaction_destroy(tx);
    }

    // Simulate leader failure
    nimcp_error_t err = mesh_coordinator_pool_handle_failure(left_pool_, original_leader);
    ASSERT_EQ(err, NIMCP_OK);

    // Wait for re-election
    bool re_elected = WaitFor([&]() {
        mesh_coordinator_pool_get_info(left_pool_, &info);
        return info.has_leader && info.current_term > original_term;
    }, 1000);

    ASSERT_TRUE(re_elected) << "Re-election did not complete";

    // Submit more transactions - should still work
    for (int i = 0; i < 3; i++) {
        mesh_transaction_config_t tx_config;
        mesh_transaction_config_init(&tx_config);
        tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        tx_config.channel_id = mesh_channel_get_id(left_hemisphere_);
        tx_config.proposer_id = mesh_participant_get_id(participants_[i]);
        tx_config.payload = "post_failure_tx";
        tx_config.payload_size = 15;

        mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
        err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
        EXPECT_EQ(err, NIMCP_OK) << "Post-failure transaction submission failed";
        mesh_transaction_destroy(tx);
    }

    mesh_ordering_flush(ordering_);
}

// =============================================================================
// Topology Integration E2E
// =============================================================================

TEST_F(MeshFullTransactionE2ETest, TopologyHubIdentification) {
    // Add connections between participants in topology
    for (size_t i = 0; i < participants_.size() - 1; i++) {
        mesh_participant_id_t from_id = mesh_participant_get_id(participants_[i]);
        mesh_participant_id_t to_id = mesh_participant_get_id(participants_[i + 1]);
        mesh_topology_add_connection(topology_, from_id, to_id, 1.0f);
    }

    // Create hub connections (participant 0 connects to many)
    mesh_participant_id_t hub_id = mesh_participant_get_id(participants_[0]);
    for (size_t i = 5; i < 15 && i < participants_.size(); i++) {
        mesh_participant_id_t to_id = mesh_participant_get_id(participants_[i]);
        mesh_topology_add_connection(topology_, hub_id, to_id, 1.0f);
    }

    // Compute topology metrics
    mesh_topology_compute_metrics(topology_);

    // Identify hubs
    mesh_topology_hub_result_t hubs;
    mesh_topology_hub_result_init(&hubs);

    nimcp_error_t err = mesh_topology_identify_hubs(topology_, &hubs);
    ASSERT_EQ(err, NIMCP_OK);

    // Participant 0 should be identified as a hub
    bool found_hub = false;
    for (size_t i = 0; i < hubs.count; i++) {
        if (hubs.hub_ids[i] == hub_id) {
            found_hub = true;
            break;
        }
    }

    EXPECT_TRUE(found_hub) << "Hub participant not identified";

    mesh_topology_hub_result_cleanup(&hubs);
}

// =============================================================================
// Full System Stress E2E
// =============================================================================

TEST_F(MeshFullTransactionE2ETest, HighThroughputTransaction) {
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    std::vector<mesh_transaction_t*> all_transactions;
    std::mutex tx_mutex;

    // Submit many transactions across channels
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            mesh_channel_t* channels[] = {left_hemisphere_, right_hemisphere_, subcortical_};

            for (int i = 0; i < 25; i++) {
                mesh_channel_t* ch = channels[(t + i) % 3];

                mesh_transaction_config_t tx_config;
                mesh_transaction_config_init(&tx_config);
                tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
                tx_config.channel_id = mesh_channel_get_id(ch);
                tx_config.proposer_id = mesh_participant_get_id(
                    participants_[(t * 25 + i) % participants_.size()]);

                char payload[64];
                snprintf(payload, sizeof(payload), "stress_tx_t%d_i%d", t, i);
                tx_config.payload = payload;
                tx_config.payload_size = strlen(payload);

                mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
                if (!tx) {
                    fail_count++;
                    continue;
                }

                // Add endorsements
                for (int e = 0; e < 3; e++) {
                    mesh_endorsement_t endorsement;
                    mesh_endorsement_init(&endorsement);
                    endorsement.endorser_id = mesh_participant_get_id(
                        participants_[e % participants_.size()]);
                    endorsement.approved = true;
                    mesh_transaction_add_endorsement(tx, &endorsement);
                }

                nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
                if (err == NIMCP_OK) {
                    success_count++;
                    std::lock_guard<std::mutex> lock(tx_mutex);
                    all_transactions.push_back(tx);
                } else {
                    fail_count++;
                    mesh_transaction_destroy(tx);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Flush and wait
    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify throughput
    EXPECT_GE(success_count.load(), 80) << "Too many failures: " << fail_count.load();

    // Get final metrics
    mesh_ordering_metrics_t metrics;
    mesh_ordering_get_metrics(ordering_, &metrics);

    EXPECT_GE(metrics.total_transactions, 80u);

    // Cleanup
    for (auto* tx : all_transactions) {
        mesh_transaction_destroy(tx);
    }
}
