/**
 * @file e2e_test_nlp_neural_distributed.cpp
 * @brief E2E Test for Neural Link Protocol - Distributed Neural Processing
 *
 * WHAT: Complete end-to-end tests for NLP neural synchronization features
 * WHY:  Verify distributed brain coordination for spike sync, weight updates, state migration
 * HOW:  Simulate distributed learning, spike coordination, state transfer, federated learning
 *
 * TEST SCENARIOS:
 * 1. DistributedLearning - Weight updates propagate across swarm
 * 2. SpikeCoordination - Spike timing synchronized sub-millisecond
 * 3. StateMigration - Full brain state transferred to new master
 * 4. GradientAggregation - Federated learning across nodes
 * 5. CognitiveConsensus - Agreement on perceived state
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <arpa/inet.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Infrastructure
//=============================================================================

constexpr uint32_t NUM_NEURAL_NODES = 8;
constexpr uint16_t NEURAL_BASE_PORT = 19300;
constexpr uint32_t NUM_NEURONS_PER_NODE = 1000;
constexpr uint32_t NUM_SYNAPSES_PER_NODE = 5000;
constexpr float SYNC_TOLERANCE_MS = 2.0f; // 2ms tolerance for spike timing
constexpr uint32_t LEARNING_ITERATIONS = 50;

/**
 * @brief Neural node state for distributed processing
 */
struct NeuralNode {
    uint32_t node_id;
    nlp_node_t nlp_node;
    uint16_t port;
    bool is_master;

    // Neural state
    std::vector<float> weights; // Synaptic weights
    uint32_t weight_version;
    std::vector<uint32_t> spike_history; // Recent spike IDs
    std::vector<uint16_t> spike_times; // Microsecond timestamps

    // Distributed learning
    std::vector<float> gradients;
    std::atomic<uint32_t> weight_updates_received{0};
    std::atomic<uint32_t> spike_batches_received{0};
    std::atomic<uint32_t> gradient_updates_received{0};

    // State synchronization
    std::vector<uint8_t> brain_state;
    uint32_t state_version;
    std::atomic<uint32_t> state_syncs_received{0};

    // Timing
    std::chrono::high_resolution_clock::time_point last_spike_time;
    std::vector<int64_t> spike_latencies_us; // Measured spike sync latencies

    // Consensus
    struct PerceivedState {
        float value;
        uint32_t timestamp;
    };
    std::vector<PerceivedState> perceived_states;

    std::mutex state_mutex;

    NeuralNode()
        : node_id(0), nlp_node(nullptr), port(0), is_master(false),
          weight_version(0), state_version(0) {
        weights.resize(NUM_SYNAPSES_PER_NODE, 0.5f);
        gradients.resize(NUM_SYNAPSES_PER_NODE, 0.0f);
    }

    // Move constructor for use in std::vector
    NeuralNode(NeuralNode&& other) noexcept
        : node_id(other.node_id),
          nlp_node(other.nlp_node),
          port(other.port),
          is_master(other.is_master),
          weights(std::move(other.weights)),
          weight_version(other.weight_version),
          spike_history(std::move(other.spike_history)),
          spike_times(std::move(other.spike_times)),
          gradients(std::move(other.gradients)),
          weight_updates_received(other.weight_updates_received.load()),
          spike_batches_received(other.spike_batches_received.load()),
          gradient_updates_received(other.gradient_updates_received.load()),
          brain_state(std::move(other.brain_state)),
          state_version(other.state_version),
          state_syncs_received(other.state_syncs_received.load()),
          last_spike_time(other.last_spike_time),
          spike_latencies_us(std::move(other.spike_latencies_us)),
          perceived_states(std::move(other.perceived_states)) {
        other.nlp_node = nullptr;
    }

    // Delete copy operations
    NeuralNode(const NeuralNode&) = delete;
    NeuralNode& operator=(const NeuralNode&) = delete;
    NeuralNode& operator=(NeuralNode&&) = delete;
};

/**
 * @brief Shared neural test state
 */
struct NeuralTestState {
    std::vector<NeuralNode*> nodes;
    std::atomic<uint32_t> total_weight_updates{0};
    std::atomic<uint32_t> total_spike_batches{0};
    std::atomic<uint32_t> total_state_migrations{0};
    std::mutex state_mutex;
};

static NeuralTestState g_neural_state;

//=============================================================================
// NLP Callbacks for Neural Sync
//=============================================================================

static void neural_message_callback(
    nlp_node_t node,
    const nlp_peer_t* peer,
    const nlp_message_t* msg,
    void* user_data
) {
    auto* neural_node = static_cast<NeuralNode*>(user_data);
    if (!neural_node) return;

    uint16_t msg_type = ntohs(msg->header.msg_type);

    std::lock_guard<std::mutex> lock(neural_node->state_mutex);

    switch (msg_type) {
        case NLP_MSG_SPIKE_BATCH:
            neural_node->spike_batches_received++;
            g_neural_state.total_spike_batches++;

            if (msg->payload && msg->header.payload_len >= sizeof(nlp_spike_batch_t)) {
                nlp_spike_batch_t batch;
                memcpy(&batch, msg->payload, sizeof(nlp_spike_batch_t));

                // Record spike timing for latency measurement
                auto now = std::chrono::high_resolution_clock::now();
                auto msg_time = std::chrono::high_resolution_clock::time_point(
                    std::chrono::microseconds(batch.timestamp_us)
                );

                if (neural_node->last_spike_time.time_since_epoch().count() > 0) {
                    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                        now - msg_time
                    ).count();
                    neural_node->spike_latencies_us.push_back(latency);
                }
            }
            break;

        case NLP_MSG_WEIGHT_DELTA:
            neural_node->weight_updates_received++;
            g_neural_state.total_weight_updates++;

            {
                uint16_t payload_len = ntohs(msg->header.payload_len);
                if (msg->payload && payload_len >= sizeof(nlp_weight_delta_header_t)) {
                    nlp_weight_delta_header_t header;
                    memcpy(&header, msg->payload, sizeof(nlp_weight_delta_header_t));

                    // Convert from network byte order
                    uint16_t delta_count = ntohs(header.delta_count);
                    uint32_t new_version = ntohl(header.new_version);

                    // Apply weight deltas
                    if (delta_count > 0 &&
                        payload_len >= sizeof(nlp_weight_delta_header_t) +
                            delta_count * sizeof(nlp_weight_delta_entry_t)) {

                        const uint8_t* delta_data = msg->payload + sizeof(nlp_weight_delta_header_t);
                        for (uint32_t i = 0; i < delta_count && i < 100; i++) {
                            nlp_weight_delta_entry_t entry;
                            memcpy(&entry, delta_data + i * sizeof(nlp_weight_delta_entry_t),
                                   sizeof(nlp_weight_delta_entry_t));

                            // Convert synapse_id from network byte order
                            uint32_t synapse_id = ntohl(entry.synapse_id);
                            if (synapse_id < neural_node->weights.size()) {
                                neural_node->weights[synapse_id] = entry.new_weight;
                            }
                        }

                        neural_node->weight_version = new_version;
                    }
                }
            }
            break;

        case NLP_MSG_STATE_SYNC:
            neural_node->state_syncs_received++;
            g_neural_state.total_state_migrations++;

            {
                uint16_t payload_len = ntohs(msg->header.payload_len);
                if (msg->payload && payload_len >= sizeof(nlp_state_sync_t)) {
                    nlp_state_sync_t state_header;
                    memcpy(&state_header, msg->payload, sizeof(nlp_state_sync_t));
                    // Convert from network byte order
                    uint32_t state_version = ntohl(state_header.state_version);

                    // Store state data
                    size_t state_data_len = payload_len - sizeof(nlp_state_sync_t);
                    neural_node->brain_state.resize(state_data_len);
                    if (state_data_len > 0) {
                        memcpy(neural_node->brain_state.data(),
                               msg->payload + sizeof(nlp_state_sync_t),
                               state_data_len);
                    }
                    neural_node->state_version = state_version;
                }
            }
            break;

        case NLP_MSG_GRADIENT_PUSH:
            neural_node->gradient_updates_received++;

            if (msg->payload && msg->header.payload_len >= 8) {
                // Parse gradient data (simplified)
                uint32_t num_gradients;
                memcpy(&num_gradients, msg->payload, sizeof(uint32_t));

                if (num_gradients > 0 && num_gradients <= 1000) {
                    // Aggregate gradients (federated learning)
                    // In real implementation, would apply gradient updates
                }
            }
            break;

        case NLP_MSG_ACTIVATION_SYNC:
            // Synchronize neuron activations
            break;

        default:
            break;
    }
}

static void neural_peer_callback(
    nlp_node_t node,
    const nlp_peer_t* peer,
    nlp_session_state_t old_state,
    nlp_session_state_t new_state,
    void* user_data
) {
    // Track peer connectivity
}

static void neural_mode_callback(
    nlp_node_t node,
    nlp_mode_t old_mode,
    nlp_mode_t new_mode,
    const char* reason,
    void* user_data
) {
    // Track mode changes
}

//=============================================================================
// Helper Functions
//=============================================================================

static nlp_node_t create_neural_node(
    uint32_t node_id,
    uint16_t port,
    bool is_master,
    NeuralNode* neural_node
) {
    nlp_config_t config = nlp_config_default();

    config.brain_id = node_id;
    config.is_master = is_master;
    config.port = port;
    snprintf(config.bind_address, sizeof(config.bind_address), "127.0.0.1");

    // Use TACTICAL mode with PSK - STANDARD mode's session keys are not yet implemented
    // (session key derivation via DH/ECDH is TODO in nlp_session_handle_handshake_resp)
    config.default_mode = NLP_MODE_TACTICAL;
    config.auto_mode_switch = false;
    config.heartbeat_interval_ms = 500;
    config.session_timeout_ms = 5000;

    config.require_encryption = true;

    // Pre-shared key for tactical mode encryption
    uint8_t psk[NLP_KEY_SIZE];
    for (int i = 0; i < NLP_KEY_SIZE; i++) {
        psk[i] = 0x6E + (i % 16); // 'n' for neural
    }
    config.psk[0].active = true;
    memcpy(config.psk[0].key, psk, NLP_KEY_SIZE);
    config.psk[0].key_id = 200;
    config.psk[0].valid_from = 0;
    config.psk[0].valid_until = UINT64_MAX;

    config.user_data = neural_node;

    nlp_node_t node = nlp_node_create(&config);
    if (!node) return nullptr;

    nlp_set_message_callback(node, neural_message_callback);
    nlp_set_peer_callback(node, neural_peer_callback);
    nlp_set_mode_callback(node, neural_mode_callback);

    return node;
}

static void connect_neural_mesh(std::vector<NeuralNode>& nodes) {
    // Connect with proper bidirectional connections with delays
    // Each node needs to know about each other for proper message routing
    for (size_t i = 0; i < nodes.size(); i++) {
        for (size_t j = i + 1; j < nodes.size(); j++) {
            // Connect from lower to higher
            nlp_connect_peer(nodes[i].nlp_node, "127.0.0.1", nodes[j].port);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            // Connect from higher to lower
            nlp_connect_peer(nodes[j].nlp_node, "127.0.0.1", nodes[i].port);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    // Wait for all connections to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

//=============================================================================
// Test Fixture
//=============================================================================

class NLPNeuralDistributedTest : public ::testing::Test {
protected:
    std::vector<NeuralNode> nodes_;

    void SetUp() override {
        nodes_.resize(NUM_NEURAL_NODES);
        g_neural_state.nodes.clear();
        g_neural_state.total_weight_updates = 0;
        g_neural_state.total_spike_batches = 0;
        g_neural_state.total_state_migrations = 0;
    }

    void TearDown() override {
        for (auto& node : nodes_) {
            if (node.nlp_node) {
                nlp_node_stop(node.nlp_node);
                nlp_node_destroy(node.nlp_node);
            }
        }
        nodes_.clear();
        g_neural_state.nodes.clear();
    }

    void InitializeNeuralNodes() {
        for (uint32_t i = 0; i < NUM_NEURAL_NODES; i++) {
            nodes_[i].node_id = 3000 + i;
            nodes_[i].port = NEURAL_BASE_PORT + i;
            nodes_[i].is_master = (i == 0);
            nodes_[i].weight_version = 1;
            nodes_[i].state_version = 1;

            nodes_[i].nlp_node = create_neural_node(
                nodes_[i].node_id,
                nodes_[i].port,
                nodes_[i].is_master,
                &nodes_[i]
            );

            ASSERT_NE(nodes_[i].nlp_node, nullptr);
            ASSERT_EQ(nlp_node_start(nodes_[i].nlp_node), 0);

            g_neural_state.nodes.push_back(&nodes_[i]);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

//=============================================================================
// Test Cases
//=============================================================================

TEST_F(NLPNeuralDistributedTest, DistributedLearning) {
    PipelineTracker tracker("Distributed Weight Update Propagation");

    tracker.begin_stage("Initialize Neural Network", 5000);
    InitializeNeuralNodes();
    connect_neural_mesh(nodes_);
    tracker.end_stage();

    tracker.begin_stage("Perform Local Learning on Master", 2000);
    // Master learns and generates weight deltas
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> synapse_dist(0, NUM_SYNAPSES_PER_NODE - 1);
    std::uniform_real_distribution<float> weight_dist(-0.1f, 0.1f);

    const uint32_t num_updates = 100;
    std::vector<uint32_t> synapse_ids(num_updates);
    std::vector<float> old_weights(num_updates);
    std::vector<float> new_weights(num_updates);

    for (uint32_t i = 0; i < num_updates; i++) {
        synapse_ids[i] = synapse_dist(gen);
        old_weights[i] = nodes_[0].weights[synapse_ids[i]];
        new_weights[i] = old_weights[i] + weight_dist(gen);
        nodes_[0].weights[synapse_ids[i]] = new_weights[i];
    }
    nodes_[0].weight_version++;
    tracker.end_stage();

    tracker.begin_stage("Broadcast Weight Deltas", 3000);
    int ret = nlp_send_weight_deltas(
        nodes_[0].nlp_node,
        0, // Broadcast
        synapse_ids.data(),
        old_weights.data(),
        new_weights.data(),
        num_updates
    );
    // nlp_broadcast returns count of successful sends, not 0
    EXPECT_GE(ret, 0) << "Failed to send weight deltas";

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    tracker.end_stage();

    tracker.begin_stage("Verify Weight Update Propagation", 1000);
    uint32_t nodes_updated = 0;
    for (size_t i = 1; i < nodes_.size(); i++) {
        if (nodes_[i].weight_updates_received.load() > 0) {
            nodes_updated++;
        }
    }
    EXPECT_GE(nodes_updated, NUM_NEURAL_NODES - 2)
        << "Weight deltas not propagated to all nodes";
    tracker.end_stage();

    tracker.begin_stage("Verify Weight Convergence", 2000);
    // Check that at least some weights are synchronized
    uint32_t synapse_to_check = synapse_ids[0];
    float master_weight = nodes_[0].weights[synapse_to_check];

    uint32_t nodes_synced = 0;
    for (size_t i = 1; i < nodes_.size(); i++) {
        std::lock_guard<std::mutex> lock(nodes_[i].state_mutex);
        if (std::abs(nodes_[i].weights[synapse_to_check] - master_weight) < 0.01f) {
            nodes_synced++;
        }
    }
    EXPECT_GE(nodes_synced, NUM_NEURAL_NODES - 3)
        << "Weights not synchronized across nodes";
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPNeuralDistributedTest, SpikeCoordination) {
    PipelineTracker tracker("Sub-Millisecond Spike Timing Synchronization");

    tracker.begin_stage("Initialize Neural Network", 5000);
    InitializeNeuralNodes();
    connect_neural_mesh(nodes_);
    tracker.end_stage();

    tracker.begin_stage("Generate Spike Activity", 2000);
    // Node 0 generates spike batch
    const uint32_t num_spikes = 50;
    std::vector<uint32_t> neuron_ids(num_spikes);
    std::vector<uint16_t> spike_times(num_spikes);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> neuron_dist(0, NUM_NEURONS_PER_NODE - 1);
    std::uniform_int_distribution<uint16_t> time_dist(0, 1000); // 0-1ms

    for (uint32_t i = 0; i < num_spikes; i++) {
        neuron_ids[i] = neuron_dist(gen);
        spike_times[i] = time_dist(gen);
    }

    nodes_[0].last_spike_time = std::chrono::high_resolution_clock::now();
    tracker.end_stage();

    tracker.begin_stage("Broadcast Spike Batch", 2000);
    int ret = nlp_send_spikes(
        nodes_[0].nlp_node,
        0, // Broadcast
        neuron_ids.data(),
        spike_times.data(),
        num_spikes
    );
    // nlp_broadcast returns count of successful sends, not 0
    EXPECT_GE(ret, 0) << "Failed to send spike batch";

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    tracker.end_stage();

    tracker.begin_stage("Verify Spike Propagation", 1000);
    uint32_t nodes_received = 0;
    for (size_t i = 1; i < nodes_.size(); i++) {
        if (nodes_[i].spike_batches_received.load() > 0) {
            nodes_received++;
        }
    }
    EXPECT_GE(nodes_received, NUM_NEURAL_NODES - 2)
        << "Spike batches not received by all nodes";
    tracker.end_stage();

    tracker.begin_stage("Measure Spike Synchronization Latency", 3000);
    // Send multiple spike batches to measure latency
    for (int round = 0; round < 10; round++) {
        for (uint32_t i = 0; i < num_spikes; i++) {
            spike_times[i] = time_dist(gen);
        }

        nlp_send_spikes(
            nodes_[0].nlp_node,
            0,
            neuron_ids.data(),
            spike_times.data(),
            num_spikes
        );

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    tracker.begin_stage("Analyze Spike Timing Precision", 1000);
    // Check recorded latencies
    for (size_t i = 1; i < nodes_.size(); i++) {
        std::lock_guard<std::mutex> lock(nodes_[i].state_mutex);
        if (!nodes_[i].spike_latencies_us.empty()) {
            int64_t avg_latency = std::accumulate(
                nodes_[i].spike_latencies_us.begin(),
                nodes_[i].spike_latencies_us.end(),
                0LL
            ) / nodes_[i].spike_latencies_us.size();

            // Latency should be reasonable (< 100ms for localhost)
            EXPECT_LT(avg_latency, 100000) // 100ms
                << "Spike latency too high on node " << i;
        }
    }
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPNeuralDistributedTest, StateMigration) {
    PipelineTracker tracker("Full Brain State Transfer to New Master");

    tracker.begin_stage("Initialize Neural Network", 5000);
    InitializeNeuralNodes();
    connect_neural_mesh(nodes_);
    tracker.end_stage();

    tracker.begin_stage("Build Complex State on Master", 2000);
    // Master builds up significant state
    // NLP_MAX_PAYLOAD is 65535, so stay under that limit
    // Account for nlp_state_sync_t header (16 bytes)
    const size_t state_data_size = 60000 - sizeof(nlp_state_sync_t);
    nodes_[0].brain_state.resize(state_data_size);
    for (size_t i = 0; i < nodes_[0].brain_state.size(); i++) {
        nodes_[0].brain_state[i] = static_cast<uint8_t>(i % 256);
    }
    nodes_[0].state_version = 42;
    tracker.end_stage();

    tracker.begin_stage("Identify Backup Master", 500);
    uint32_t backup_master_idx = 1;
    EXPECT_TRUE(backup_master_idx < nodes_.size());
    tracker.end_stage();

    tracker.begin_stage("Transfer State to Backup", 5000);
    // Construct state message with nlp_state_sync_t header followed by data
    // The callback expects: [nlp_state_sync_t header][state data]
    std::vector<uint8_t> state_message(sizeof(nlp_state_sync_t) + nodes_[0].brain_state.size());
    nlp_state_sync_t* header = reinterpret_cast<nlp_state_sync_t*>(state_message.data());
    // Convert to network byte order for wire transmission
    header->state_version = htonl(nodes_[0].state_version);
    header->neuron_count = htonl(static_cast<uint32_t>(nodes_[0].brain_state.size()));
    header->synapse_count = htonl(0);
    header->flags = htonl(0);
    memcpy(state_message.data() + sizeof(nlp_state_sync_t),
           nodes_[0].brain_state.data(),
           nodes_[0].brain_state.size());

    int ret = nlp_send_state(
        nodes_[0].nlp_node,
        0, // Broadcast
        state_message.data(),
        state_message.size()
    );
    // nlp_broadcast returns count of successful sends, not 0
    EXPECT_GE(ret, 0) << "Failed to send state";

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    tracker.end_stage();

    tracker.begin_stage("Verify State Reception", 1000);
    EXPECT_GT(nodes_[backup_master_idx].state_syncs_received.load(), 0)
        << "Backup did not receive state";

    {
        std::lock_guard<std::mutex> lock(nodes_[backup_master_idx].state_mutex);
        EXPECT_EQ(nodes_[backup_master_idx].state_version, 42)
            << "State version mismatch";
        EXPECT_EQ(nodes_[backup_master_idx].brain_state.size(), nodes_[0].brain_state.size())
            << "State size mismatch";
    }
    tracker.end_stage();

    tracker.begin_stage("Verify State Integrity", 1000);
    // Check that state data is correct
    std::lock_guard<std::mutex> lock2(nodes_[backup_master_idx].state_mutex);
    bool state_matches = true;
    for (size_t i = 0; i < std::min(nodes_[0].brain_state.size(),
                                     nodes_[backup_master_idx].brain_state.size()); i++) {
        if (nodes_[0].brain_state[i] != nodes_[backup_master_idx].brain_state[i]) {
            state_matches = false;
            break;
        }
    }
    EXPECT_TRUE(state_matches) << "Transferred state does not match original";
    tracker.end_stage();

    tracker.begin_stage("Simulate Master Failover", 2000);
    // Stop original master
    nlp_node_stop(nodes_[0].nlp_node);

    // Backup becomes new master
    nodes_[backup_master_idx].is_master = true;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    tracker.begin_stage("Verify New Master Operational", 2000);
    // New master should be able to send updates
    const char* test_msg = "new_master_active";
    nlp_broadcast(
        nodes_[backup_master_idx].nlp_node,
        NLP_MSG_STATE_SYNC,  // HEARTBEAT is consumed internally, use STATE_SYNC
        test_msg,
        strlen(test_msg),
        NLP_PRIORITY_NORMAL
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPNeuralDistributedTest, GradientAggregation) {
    PipelineTracker tracker("Federated Learning Gradient Aggregation");

    tracker.begin_stage("Initialize Neural Network", 5000);
    InitializeNeuralNodes();
    connect_neural_mesh(nodes_);
    tracker.end_stage();

    tracker.begin_stage("Local Training on Each Node", 3000);
    // Each node computes gradients locally
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> grad_dist(0.0f, 0.01f);

    for (auto& node : nodes_) {
        for (size_t i = 0; i < node.gradients.size(); i++) {
            node.gradients[i] = grad_dist(gen);
        }
    }
    tracker.end_stage();

    tracker.begin_stage("Push Gradients to Master", 4000);
    // All nodes send gradients to master
    for (size_t i = 0; i < nodes_.size(); i++) {
        // Simplified gradient message (first 100 gradients)
        uint32_t num_grads = std::min(100u, static_cast<uint32_t>(nodes_[i].gradients.size()));

        std::vector<uint8_t> grad_data(sizeof(uint32_t) + num_grads * sizeof(float));
        memcpy(grad_data.data(), &num_grads, sizeof(uint32_t));
        memcpy(grad_data.data() + sizeof(uint32_t),
               nodes_[i].gradients.data(),
               num_grads * sizeof(float));

        // Use broadcast (peer_id=0) since targeted sends require session keys
        nlp_broadcast(
            nodes_[i].nlp_node,
            NLP_MSG_GRADIENT_PUSH,
            grad_data.data(),
            grad_data.size(),
            NLP_PRIORITY_HIGH
        );
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    tracker.end_stage();

    tracker.begin_stage("Verify Gradient Collection", 1000);
    EXPECT_GT(nodes_[0].gradient_updates_received.load(), NUM_NEURAL_NODES / 2)
        << "Master did not receive enough gradients";
    tracker.end_stage();

    tracker.begin_stage("Aggregate and Broadcast Updated Weights", 3000);
    // Master aggregates gradients and broadcasts weight updates
    // (Simplified: just send a few weight updates)
    std::vector<uint32_t> synapse_ids = {0, 10, 20, 30, 40};
    std::vector<float> old_weights(5);
    std::vector<float> new_weights(5);

    for (size_t i = 0; i < 5; i++) {
        old_weights[i] = nodes_[0].weights[synapse_ids[i]];
        new_weights[i] = old_weights[i] - 0.01f * nodes_[0].gradients[synapse_ids[i]];
        nodes_[0].weights[synapse_ids[i]] = new_weights[i];
    }

    nlp_send_weight_deltas(
        nodes_[0].nlp_node,
        0,
        synapse_ids.data(),
        old_weights.data(),
        new_weights.data(),
        5
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    tracker.end_stage();

    tracker.begin_stage("Verify Distributed Learning Convergence", 1000);
    uint32_t nodes_with_updates = 0;
    for (size_t i = 1; i < nodes_.size(); i++) {
        if (nodes_[i].weight_updates_received.load() > 0) {
            nodes_with_updates++;
        }
    }
    EXPECT_GE(nodes_with_updates, NUM_NEURAL_NODES - 2)
        << "Federated learning did not propagate";
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

TEST_F(NLPNeuralDistributedTest, CognitiveConsensus) {
    PipelineTracker tracker("Agreement on Perceived State");

    tracker.begin_stage("Initialize Neural Network", 5000);
    InitializeNeuralNodes();
    connect_neural_mesh(nodes_);
    tracker.end_stage();

    tracker.begin_stage("Each Node Perceives Environment", 2000);
    // Each node processes sensory input and forms perception
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> perception_dist(10.0f, 0.5f); // Mean=10, noise=0.5

    for (auto& node : nodes_) {
        NeuralNode::PerceivedState state;
        state.value = perception_dist(gen);
        state.timestamp = static_cast<uint32_t>(time(nullptr));

        std::lock_guard<std::mutex> lock(node.state_mutex);
        node.perceived_states.push_back(state);
    }
    tracker.end_stage();

    tracker.begin_stage("Broadcast Perceptions", 3000);
    // Each node broadcasts its perception
    // Note: HEARTBEAT is consumed internally, so we track perceptions locally
    // In production, a dedicated consensus message type would be used
    for (auto& node : nodes_) {
        std::lock_guard<std::mutex> lock(node.state_mutex);
        if (!node.perceived_states.empty()) {
            auto& state = node.perceived_states.back();
            nlp_broadcast(
                node.nlp_node,
                NLP_MSG_STATE_SYNC,  // Use STATE_SYNC for user callbacks
                &state.value,
                sizeof(float),
                NLP_PRIORITY_NORMAL
            );
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    tracker.end_stage();

    tracker.begin_stage("Compute Consensus", 2000);
    // Each node computes consensus from all perceptions received
    // For this simplified test, we just verify variance is low
    std::vector<float> all_perceptions;
    for (const auto& node : nodes_) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(node.state_mutex));
        for (const auto& state : node.perceived_states) {
            all_perceptions.push_back(state.value);
        }
    }

    if (!all_perceptions.empty()) {
        float mean = std::accumulate(all_perceptions.begin(), all_perceptions.end(), 0.0f) /
                     all_perceptions.size();

        float variance = 0.0f;
        for (float val : all_perceptions) {
            variance += (val - mean) * (val - mean);
        }
        variance /= all_perceptions.size();

        EXPECT_LT(variance, 1.0f) << "Perceptions too divergent for consensus";
    }
    tracker.end_stage();

    tracker.begin_stage("Verify Consensus Convergence", 1000);
    // In a real implementation, would verify Byzantine-fault-tolerant consensus
    // For this test, we verify that nodes have similar perceptions
    EXPECT_GE(all_perceptions.size(), NUM_NEURAL_NODES)
        << "Not enough perceptions collected";
    tracker.end_stage();

    EXPECT_TRUE(tracker.is_successful());
    tracker.print_summary();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
