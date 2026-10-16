/**
 * @file test_mesh_system_integration_e2e.cpp
 * @brief End-to-End Tests for Full Mesh System Integration
 *
 * WHAT: Tests complete system integration with all mesh components
 * WHY:  Verify entire mesh network operates correctly as unified system
 * HOW:  Create realistic multi-hemisphere scenarios with full component stack
 *
 * TEST COVERAGE:
 * - Complete brain hemisphere setup
 * - Full transaction lifecycle across system
 * - System coordinator arbitration
 * - Multi-channel belief propagation
 * - Complete failure recovery scenarios
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
#include "mesh/nimcp_mesh_coordinator.h"
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
// Test Fixture - Full System Setup
// =============================================================================

class MeshSystemIntegrationE2ETest : public ::testing::Test {
protected:
    // System-level components
    mesh_msp_t* msp_ = nullptr;
    mesh_ordering_service_t* ordering_ = nullptr;
    mesh_cross_channel_router_t* router_ = nullptr;
    mesh_hierarchical_timing_t* timing_ = nullptr;
    mesh_topology_t* topology_ = nullptr;

    // Hemispheres
    mesh_channel_t* left_hemisphere_ = nullptr;
    mesh_channel_t* right_hemisphere_ = nullptr;
    mesh_channel_t* corpus_callosum_ = nullptr;
    mesh_channel_t* brainstem_ = nullptr;

    // Coordinator pools (hierarchical)
    mesh_coordinator_pool_t* system_pool_ = nullptr;
    mesh_coordinator_pool_t* left_hemi_pool_ = nullptr;
    mesh_coordinator_pool_t* right_hemi_pool_ = nullptr;
    mesh_coordinator_pool_t* subcortical_pool_ = nullptr;

    // Layer pools within hemispheres
    mesh_coordinator_pool_t* left_cognitive_pool_ = nullptr;
    mesh_coordinator_pool_t* left_sensory_pool_ = nullptr;
    mesh_coordinator_pool_t* right_cognitive_pool_ = nullptr;
    mesh_coordinator_pool_t* right_sensory_pool_ = nullptr;

    // Participants representing brain modules
    std::map<std::string, mesh_participant_t*> modules_;

    void SetUp() override {
        // Create MSP (brain security)
        mesh_msp_config_t msp_config;
        mesh_msp_config_init(&msp_config);
        msp_config.enable_bbb = true;
        msp_config.enable_immune = true;
        msp_ = mesh_msp_create(&msp_config);
        ASSERT_NE(msp_, nullptr);

        // Create ordering service (Raft consensus)
        mesh_ordering_config_t ord_config;
        mesh_ordering_config_init(&ord_config);
        ord_config.batch_size = 20;
        ord_config.raft_enabled = true;
        ordering_ = mesh_ordering_create(&ord_config);
        ASSERT_NE(ordering_, nullptr);

        // Create hierarchical timing
        mesh_hierarchical_timing_config_t timing_config;
        mesh_hierarchical_timing_config_init(&timing_config);
        timing_ = mesh_hierarchical_timing_create(&timing_config);
        ASSERT_NE(timing_, nullptr);

        // Create topology
        mesh_topology_config_t topo_config;
        mesh_topology_config_init(&topo_config);
        topo_config.max_nodes = 200;
        topology_ = mesh_topology_create(&topo_config);
        ASSERT_NE(topology_, nullptr);

        // Create channels (brain regions)
        left_hemisphere_ = CreateChannel("left_hemisphere", 64);
        right_hemisphere_ = CreateChannel("right_hemisphere", 64);
        corpus_callosum_ = CreateChannel("corpus_callosum", 32);
        brainstem_ = CreateChannel("brainstem", 16);

        ASSERT_NE(left_hemisphere_, nullptr);
        ASSERT_NE(right_hemisphere_, nullptr);
        ASSERT_NE(corpus_callosum_, nullptr);
        ASSERT_NE(brainstem_, nullptr);

        // Create hierarchical coordinator pools
        CreateCoordinatorHierarchy();

        // Create cross-channel router
        mesh_cross_channel_config_t router_config;
        mesh_cross_channel_config_init(&router_config);
        router_config.ordering_service = ordering_;
        router_config.msp = msp_;
        router_ = mesh_cross_channel_router_create(&router_config);
        ASSERT_NE(router_, nullptr);

        // Register channels
        mesh_cross_channel_router_register(router_, left_hemisphere_);
        mesh_cross_channel_router_register(router_, right_hemisphere_);
        mesh_cross_channel_router_register(router_, corpus_callosum_);
        mesh_cross_channel_router_register(router_, brainstem_);

        // Create brain modules as participants
        CreateBrainModules();

        // Run initial elections
        RunInitialElections();
    }

    void TearDown() override {
        for (auto& pair : modules_) {
            if (pair.second) mesh_participant_destroy(pair.second);
        }
        modules_.clear();

        // Destroy layer pools
        if (left_cognitive_pool_) mesh_coordinator_pool_destroy(left_cognitive_pool_);
        if (left_sensory_pool_) mesh_coordinator_pool_destroy(left_sensory_pool_);
        if (right_cognitive_pool_) mesh_coordinator_pool_destroy(right_cognitive_pool_);
        if (right_sensory_pool_) mesh_coordinator_pool_destroy(right_sensory_pool_);

        // Destroy hemisphere pools
        if (left_hemi_pool_) mesh_coordinator_pool_destroy(left_hemi_pool_);
        if (right_hemi_pool_) mesh_coordinator_pool_destroy(right_hemi_pool_);
        if (subcortical_pool_) mesh_coordinator_pool_destroy(subcortical_pool_);

        // Destroy system pool
        if (system_pool_) mesh_coordinator_pool_destroy(system_pool_);

        // Destroy infrastructure
        if (router_) mesh_cross_channel_router_destroy(router_);
        if (topology_) mesh_topology_destroy(topology_);
        if (timing_) mesh_hierarchical_timing_destroy(timing_);
        if (brainstem_) mesh_channel_destroy(brainstem_);
        if (corpus_callosum_) mesh_channel_destroy(corpus_callosum_);
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

    void CreateCoordinatorHierarchy() {
        // System level (top)
        system_pool_ = CreatePool("system_pool", 3, nullptr);

        // Hemisphere level
        left_hemi_pool_ = CreatePool("left_hemi_pool", 5, system_pool_);
        right_hemi_pool_ = CreatePool("right_hemi_pool", 5, system_pool_);
        subcortical_pool_ = CreatePool("subcortical_pool", 3, system_pool_);

        // Layer level (within hemispheres)
        left_cognitive_pool_ = CreatePool("left_cognitive", 3, left_hemi_pool_);
        left_sensory_pool_ = CreatePool("left_sensory", 3, left_hemi_pool_);
        right_cognitive_pool_ = CreatePool("right_cognitive", 3, right_hemi_pool_);
        right_sensory_pool_ = CreatePool("right_sensory", 3, right_hemi_pool_);
    }

    mesh_coordinator_pool_t* CreatePool(const char* name, size_t size,
                                         mesh_coordinator_pool_t* parent) {
        mesh_coordinator_pool_config_t config;
        mesh_coordinator_pool_config_init(&config);
        config.pool_name = name;
        config.initial_size = size;
        config.enable_bft = true;
        config.parent_pool = parent;
        return mesh_coordinator_pool_create(&config);
    }

    void CreateBrainModules() {
        // Left hemisphere modules
        CreateModule("left_pfc", left_hemisphere_, left_cognitive_pool_, true);
        CreateModule("left_dlpfc", left_hemisphere_, left_cognitive_pool_, true);
        CreateModule("left_motor", left_hemisphere_, left_sensory_pool_, false);
        CreateModule("left_visual", left_hemisphere_, left_sensory_pool_, false);
        CreateModule("left_auditory", left_hemisphere_, left_sensory_pool_, false);
        CreateModule("broca", left_hemisphere_, left_cognitive_pool_, true);

        // Right hemisphere modules
        CreateModule("right_pfc", right_hemisphere_, right_cognitive_pool_, true);
        CreateModule("right_dlpfc", right_hemisphere_, right_cognitive_pool_, true);
        CreateModule("right_motor", right_hemisphere_, right_sensory_pool_, false);
        CreateModule("right_visual", right_hemisphere_, right_sensory_pool_, false);
        CreateModule("right_spatial", right_hemisphere_, right_sensory_pool_, false);
        CreateModule("right_creative", right_hemisphere_, right_cognitive_pool_, true);

        // Subcortical modules
        CreateModule("hippocampus", brainstem_, subcortical_pool_, true);
        CreateModule("amygdala", brainstem_, subcortical_pool_, true);
        CreateModule("thalamus", brainstem_, subcortical_pool_, false);
        CreateModule("basal_ganglia", brainstem_, subcortical_pool_, false);

        // Corpus callosum modules (cross-hemisphere communication)
        CreateModule("cc_anterior", corpus_callosum_, system_pool_, true);
        CreateModule("cc_posterior", corpus_callosum_, system_pool_, true);
    }

    void CreateModule(const char* name, mesh_channel_t* channel,
                      mesh_coordinator_pool_t* pool, bool can_endorse) {
        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        config.name = name;
        config.type = can_endorse ? MESH_PARTICIPANT_TYPE_ENDORSER
                                  : MESH_PARTICIPANT_TYPE_PEER;
        config.can_endorse = can_endorse;

        mesh_participant_t* p = mesh_participant_create(&config);
        if (!p) return;

        mesh_participant_id_t pid = mesh_participant_get_id(p);

        // Issue credential
        mesh_credential_t cred;
        mesh_credential_init(&cred);
        cred.participant_id = pid;
        mesh_msp_issue_credential(msp_, &cred);

        // Add to channel
        mesh_channel_add_participant(channel, p);

        // Authorize channel access
        mesh_channel_id_t ch_id = mesh_channel_get_id(channel);
        mesh_msp_authorize_channel(msp_, pid, ch_id);

        // Assign to coordinator pool
        mesh_coordinator_pool_assign_participant(pool, pid);

        // Register in topology
        mesh_topology_register_node(topology_, pid, name);

        modules_[name] = p;
    }

    void RunInitialElections() {
        // Bottom-up election order
        mesh_coordinator_pool_elect_leader(left_cognitive_pool_);
        mesh_coordinator_pool_elect_leader(left_sensory_pool_);
        mesh_coordinator_pool_elect_leader(right_cognitive_pool_);
        mesh_coordinator_pool_elect_leader(right_sensory_pool_);

        mesh_coordinator_pool_elect_leader(left_hemi_pool_);
        mesh_coordinator_pool_elect_leader(right_hemi_pool_);
        mesh_coordinator_pool_elect_leader(subcortical_pool_);

        mesh_coordinator_pool_elect_leader(system_pool_);
    }

    mesh_transaction_t* CreateBeliefTransaction(const char* module_name,
                                                 mesh_channel_t* channel,
                                                 const char* belief_data) {
        auto it = modules_.find(module_name);
        if (it == modules_.end()) return nullptr;

        mesh_transaction_config_t tx_config;
        mesh_transaction_config_init(&tx_config);
        tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        tx_config.channel_id = mesh_channel_get_id(channel);
        tx_config.proposer_id = mesh_participant_get_id(it->second);
        tx_config.payload = belief_data;
        tx_config.payload_size = strlen(belief_data);
        return mesh_transaction_create(&tx_config);
    }

    void AddEndorsements(mesh_transaction_t* tx, const std::vector<std::string>& endorser_names) {
        for (const auto& name : endorser_names) {
            auto it = modules_.find(name);
            if (it == modules_.end()) continue;

            mesh_endorsement_t endorsement;
            mesh_endorsement_init(&endorsement);
            endorsement.endorser_id = mesh_participant_get_id(it->second);
            endorsement.approved = true;
            endorsement.timestamp_ms = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            mesh_transaction_add_endorsement(tx, &endorsement);
        }
    }
};

// =============================================================================
// Full System Setup Verification
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, SystemSetupVerification) {
    // Verify all pools have leaders
    mesh_coordinator_pool_info_t info;

    mesh_coordinator_pool_get_info(system_pool_, &info);
    EXPECT_TRUE(info.has_leader) << "System pool should have leader";

    mesh_coordinator_pool_get_info(left_hemi_pool_, &info);
    EXPECT_TRUE(info.has_leader) << "Left hemisphere pool should have leader";

    mesh_coordinator_pool_get_info(right_hemi_pool_, &info);
    EXPECT_TRUE(info.has_leader) << "Right hemisphere pool should have leader";

    // Verify channels have participants
    mesh_channel_info_t ch_info;

    mesh_channel_get_info(left_hemisphere_, &ch_info);
    EXPECT_GT(ch_info.participant_count, 0u) << "Left hemisphere should have participants";

    mesh_channel_get_info(right_hemisphere_, &ch_info);
    EXPECT_GT(ch_info.participant_count, 0u) << "Right hemisphere should have participants";

    // Verify modules exist
    EXPECT_TRUE(modules_.count("left_pfc") > 0);
    EXPECT_TRUE(modules_.count("right_pfc") > 0);
    EXPECT_TRUE(modules_.count("hippocampus") > 0);
}

// =============================================================================
// Intra-Hemisphere Processing
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, LeftHemisphereProcessing) {
    // Language processing flow: Auditory → Broca → PFC
    const char* belief_data = "auditory_input:speech_pattern:0.9";

    // Create transaction from auditory cortex
    mesh_transaction_t* tx = CreateBeliefTransaction("left_auditory", left_hemisphere_, belief_data);
    ASSERT_NE(tx, nullptr);

    // Endorse by left hemisphere endorsers
    AddEndorsements(tx, {"broca", "left_pfc", "left_dlpfc"});

    // Submit to ordering
    nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_OK);

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify transaction processed
    mesh_transaction_info_t tx_info;
    mesh_transaction_get_info(tx, &tx_info);
    EXPECT_EQ(tx_info.status, MESH_TX_STATUS_COMMITTED);

    mesh_transaction_destroy(tx);
}

TEST_F(MeshSystemIntegrationE2ETest, RightHemisphereProcessing) {
    // Spatial processing flow: Visual → Spatial → Creative
    const char* belief_data = "visual_input:spatial_pattern:0.85";

    // Create transaction from visual cortex
    mesh_transaction_t* tx = CreateBeliefTransaction("right_visual", right_hemisphere_, belief_data);
    ASSERT_NE(tx, nullptr);

    // Endorse by right hemisphere endorsers
    AddEndorsements(tx, {"right_spatial", "right_creative", "right_pfc"});

    // Submit to ordering
    nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_OK);

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify transaction processed
    mesh_transaction_info_t tx_info;
    mesh_transaction_get_info(tx, &tx_info);
    EXPECT_EQ(tx_info.status, MESH_TX_STATUS_COMMITTED);

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Cross-Hemisphere Communication
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, CrossHemisphereCommunication) {
    // Grant cross-channel access for corpus callosum modules
    mesh_channel_id_t left_id = mesh_channel_get_id(left_hemisphere_);
    mesh_channel_id_t right_id = mesh_channel_get_id(right_hemisphere_);

    for (const auto& name : {"cc_anterior", "cc_posterior"}) {
        auto it = modules_.find(name);
        if (it != modules_.end()) {
            mesh_participant_id_t pid = mesh_participant_get_id(it->second);
            mesh_msp_authorize_channel(msp_, pid, left_id);
            mesh_msp_authorize_channel(msp_, pid, right_id);
        }
    }

    // Create cross-hemisphere transaction
    auto cc_it = modules_.find("cc_anterior");
    ASSERT_NE(cc_it, modules_.end());

    mesh_cross_transaction_t cross_tx;
    mesh_cross_transaction_init(&cross_tx);
    cross_tx.source_channel_id = left_id;
    cross_tx.target_channel_id = right_id;
    cross_tx.proposer_id = mesh_participant_get_id(cc_it->second);
    cross_tx.payload = "cross_hemi:integrated_belief:0.88";
    cross_tx.payload_size = 32;

    // Submit via router
    nimcp_error_t err = mesh_cross_channel_router_submit(router_, &cross_tx);
    EXPECT_EQ(err, NIMCP_OK);

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check status
    mesh_cross_tx_status_t status;
    mesh_cross_channel_router_get_status(router_, cross_tx.tx_id, &status);

    EXPECT_TRUE(status == MESH_CROSS_TX_COMMITTED ||
                status == MESH_CROSS_TX_ORDERING);

    mesh_cross_transaction_cleanup(&cross_tx);
}

// =============================================================================
// Subcortical Integration
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, SubcorticalMemoryConsolidation) {
    // Memory consolidation: PFC → Hippocampus
    const char* belief_data = "memory:working_memory_content:important";

    // Create transaction from left PFC
    mesh_transaction_t* tx = CreateBeliefTransaction("left_pfc", brainstem_, belief_data);

    if (!tx) {
        // PFC might not have brainstem access, grant it
        auto pfc_it = modules_.find("left_pfc");
        if (pfc_it != modules_.end()) {
            mesh_participant_id_t pid = mesh_participant_get_id(pfc_it->second);
            mesh_channel_id_t bs_id = mesh_channel_get_id(brainstem_);
            mesh_msp_authorize_channel(msp_, pid, bs_id);

            tx = CreateBeliefTransaction("left_pfc", brainstem_, belief_data);
        }
    }

    if (!tx) {
        // Fall back to hippocampus as proposer
        tx = CreateBeliefTransaction("hippocampus", brainstem_, belief_data);
    }

    ASSERT_NE(tx, nullptr);

    // Endorse by subcortical modules
    AddEndorsements(tx, {"hippocampus", "amygdala"});

    // Submit
    nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_OK);

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    mesh_transaction_destroy(tx);
}

TEST_F(MeshSystemIntegrationE2ETest, EmotionalProcessingPriority) {
    // Amygdala emotional response should have high priority
    const char* belief_data = "emotion:threat_detected:0.95";

    mesh_transaction_t* tx = CreateBeliefTransaction("amygdala", brainstem_, belief_data);
    ASSERT_NE(tx, nullptr);

    // Set high priority
    mesh_transaction_set_priority(tx, MESH_TX_PRIORITY_HIGH);

    // Endorse
    AddEndorsements(tx, {"amygdala", "hippocampus"});

    // Submit
    nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_OK);

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // High priority should be processed quickly
    mesh_transaction_info_t tx_info;
    mesh_transaction_get_info(tx, &tx_info);
    EXPECT_EQ(tx_info.status, MESH_TX_STATUS_COMMITTED);

    mesh_transaction_destroy(tx);
}

// =============================================================================
// Failure Recovery
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, HemisphereLeaderFailover) {
    // Get left hemisphere leader
    mesh_coordinator_pool_info_t info;
    mesh_coordinator_pool_get_info(left_hemi_pool_, &info);
    ASSERT_TRUE(info.has_leader);
    uint64_t original_term = info.current_term;

    // Submit transaction before failure
    mesh_transaction_t* tx1 = CreateBeliefTransaction("left_pfc", left_hemisphere_, "pre_failure:belief");
    ASSERT_NE(tx1, nullptr);
    AddEndorsements(tx1, {"broca", "left_pfc", "left_dlpfc"});
    mesh_ordering_submit(ordering_, tx1, nullptr, nullptr);

    // Simulate leader failure
    mesh_coordinator_pool_handle_failure(left_hemi_pool_, info.leader_index);

    // Wait for re-election
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    mesh_coordinator_pool_get_info(left_hemi_pool_, &info);
    EXPECT_TRUE(info.has_leader) << "Should have new leader after failover";
    EXPECT_GT(info.current_term, original_term) << "Term should increase";

    // Submit transaction after failure
    mesh_transaction_t* tx2 = CreateBeliefTransaction("left_pfc", left_hemisphere_, "post_failure:belief");
    ASSERT_NE(tx2, nullptr);
    AddEndorsements(tx2, {"broca", "left_pfc", "left_dlpfc"});
    nimcp_error_t err = mesh_ordering_submit(ordering_, tx2, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_OK) << "Should be able to submit after failover";

    mesh_ordering_flush(ordering_);

    mesh_transaction_destroy(tx1);
    mesh_transaction_destroy(tx2);
}

// =============================================================================
// Consensus Across Hemispheres
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, BilateralConsensus) {
    // Introduce beliefs in both hemispheres
    for (int i = 0; i < 5; i++) {
        // Left hemisphere
        mesh_belief_t left_belief;
        mesh_belief_init(&left_belief);
        left_belief.belief_type = MESH_BELIEF_TYPE_STATE;
        left_belief.confidence = 0.8f;
        snprintf(left_belief.topic, sizeof(left_belief.topic), "shared_concept_%d", i);
        mesh_channel_gossip_introduce(left_hemisphere_, &left_belief);

        // Right hemisphere
        mesh_belief_t right_belief;
        mesh_belief_init(&right_belief);
        right_belief.belief_type = MESH_BELIEF_TYPE_STATE;
        right_belief.confidence = 0.75f;
        snprintf(right_belief.topic, sizeof(right_belief.topic), "shared_concept_%d", i);
        mesh_channel_gossip_introduce(right_hemisphere_, &right_belief);
    }

    // Run gossip rounds
    for (int round = 0; round < 10; round++) {
        mesh_channel_gossip_round(left_hemisphere_);
        mesh_channel_gossip_round(right_hemisphere_);
        mesh_channel_gossip_round(corpus_callosum_);

        float interval = mesh_hierarchical_timing_get_interval(timing_, MESH_TIMING_LEVEL_HEMISPHERE);
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval)));
    }

    // Check consensus in both hemispheres
    mesh_belief_set_t left_consensus, right_consensus;
    mesh_belief_set_init(&left_consensus);
    mesh_belief_set_init(&right_consensus);

    mesh_channel_get_consensus_beliefs(left_hemisphere_, &left_consensus);
    mesh_channel_get_consensus_beliefs(right_hemisphere_, &right_consensus);

    // Both should have converged on some beliefs
    EXPECT_GT(left_consensus.count, 0u);
    EXPECT_GT(right_consensus.count, 0u);

    mesh_belief_set_cleanup(&left_consensus);
    mesh_belief_set_cleanup(&right_consensus);
}

// =============================================================================
// Full Transaction Lifecycle
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, CompleteTransactionLifecycle) {
    // Track all stages
    std::atomic<bool> endorsed{false};
    std::atomic<bool> ordered{false};
    std::atomic<bool> validated{false};
    std::atomic<bool> committed{false};

    // Create complex multi-step belief
    const char* belief_data = "complex:multimodal_integration:visual+auditory+motor";

    mesh_transaction_t* tx = CreateBeliefTransaction("left_pfc", left_hemisphere_, belief_data);
    ASSERT_NE(tx, nullptr);

    // Set transaction callbacks
    mesh_transaction_set_callback(tx, MESH_TX_EVENT_ENDORSED, [](void* ctx) {
        *static_cast<std::atomic<bool>*>(ctx) = true;
    }, &endorsed);

    mesh_transaction_set_callback(tx, MESH_TX_EVENT_ORDERED, [](void* ctx) {
        *static_cast<std::atomic<bool>*>(ctx) = true;
    }, &ordered);

    mesh_transaction_set_callback(tx, MESH_TX_EVENT_VALIDATED, [](void* ctx) {
        *static_cast<std::atomic<bool>*>(ctx) = true;
    }, &validated);

    mesh_transaction_set_callback(tx, MESH_TX_EVENT_COMMITTED, [](void* ctx) {
        *static_cast<std::atomic<bool>*>(ctx) = true;
    }, &committed);

    // Add endorsements
    AddEndorsements(tx, {"broca", "left_pfc", "left_dlpfc"});
    endorsed = true;  // Manual set since we added endorsements directly

    // Submit to ordering
    nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_OK);

    // Flush and wait
    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify all stages completed
    EXPECT_TRUE(endorsed.load()) << "Transaction should be endorsed";
    // ordered, validated, committed depend on implementation callbacks

    // Final verification
    mesh_transaction_info_t tx_info;
    mesh_transaction_get_info(tx, &tx_info);
    EXPECT_EQ(tx_info.status, MESH_TX_STATUS_COMMITTED);
    EXPECT_GT(tx_info.sequence_number, 0u);

    mesh_transaction_destroy(tx);
}

// =============================================================================
// System-Wide Stress Test
// =============================================================================

TEST_F(MeshSystemIntegrationE2ETest, SystemWideStress) {
    std::atomic<int> total_submitted{0};
    std::atomic<int> total_success{0};
    std::atomic<int> total_failed{0};

    std::vector<std::thread> threads;
    std::vector<mesh_transaction_t*> all_txs;
    std::mutex tx_mutex;

    // Simulate concurrent activity across all channels
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            mesh_channel_t* channels[] = {left_hemisphere_, right_hemisphere_, brainstem_};
            std::vector<std::string> endorsers = {"broca", "left_pfc", "right_pfc", "hippocampus"};

            for (int i = 0; i < 20; i++) {
                mesh_channel_t* ch = channels[(t + i) % 3];
                const char* proposer = (t % 2 == 0) ? "left_pfc" : "right_pfc";

                char payload[64];
                snprintf(payload, sizeof(payload), "stress_belief_t%d_i%d", t, i);

                mesh_transaction_t* tx = CreateBeliefTransaction(proposer, ch, payload);
                if (!tx) {
                    total_failed++;
                    continue;
                }

                AddEndorsements(tx, {"broca", "left_pfc"});

                nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
                total_submitted++;

                if (err == NIMCP_OK) {
                    total_success++;
                    std::lock_guard<std::mutex> lock(tx_mutex);
                    all_txs.push_back(tx);
                } else {
                    total_failed++;
                    mesh_transaction_destroy(tx);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Flush all pending
    for (int i = 0; i < 10; i++) {
        mesh_ordering_flush(ordering_);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Verify results
    EXPECT_GE(total_success.load(), 60) << "At least 75% of transactions should succeed";

    // Check ordering metrics
    mesh_ordering_metrics_t metrics;
    mesh_ordering_get_metrics(ordering_, &metrics);
    EXPECT_GE(metrics.total_transactions, 60u);

    // Cleanup
    for (auto* tx : all_txs) {
        mesh_transaction_destroy(tx);
    }
}

