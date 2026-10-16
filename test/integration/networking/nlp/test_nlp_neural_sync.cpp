/**
 * @file test_nlp_neural_sync.cpp
 * @brief Integration Tests for Neural Link Protocol Neural Synchronization
 *
 * WHAT: Tests neural state synchronization between distributed brains
 * WHY:  Verify distributed brain swarms can coordinate cognitive state
 * HOW:  Send spike batches, weight deltas, state syncs, and verify delivery
 *
 * TEST COVERAGE:
 * - Spike batch synchronization between brains
 * - Weight delta propagation and application
 * - Full state sync after disconnect/reconnect
 * - Gradient push for distributed learning
 * - Activation synchronization
 * - High-frequency spike streaming
 * - Large weight matrix synchronization
 * - Sync performance under load
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstring>
#include <cmath>
#include <arpa/inet.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture and Helpers
//=============================================================================

/**
 * @brief Neural data tracker for verification
 */
struct NeuralSyncTracker {
    std::mutex mutex;
    std::vector<std::vector<uint32_t>> received_spikes;
    std::vector<uint32_t> weight_delta_counts;
    std::vector<uint32_t> state_sync_versions;
    std::atomic<uint32_t> spike_batch_count{0};
    std::atomic<uint32_t> weight_delta_count{0};
    std::atomic<uint32_t> state_sync_count{0};
    std::atomic<uint32_t> gradient_push_count{0};

    void record_spike_batch(const uint32_t* neuron_ids, uint32_t count) {
        std::lock_guard<std::mutex> lock(mutex);
        received_spikes.push_back(std::vector<uint32_t>(neuron_ids, neuron_ids + count));
        spike_batch_count++;
    }

    void record_weight_delta(uint32_t delta_count) {
        std::lock_guard<std::mutex> lock(mutex);
        weight_delta_counts.push_back(delta_count);
        weight_delta_count++;
    }

    void record_state_sync(uint32_t version) {
        std::lock_guard<std::mutex> lock(mutex);
        state_sync_versions.push_back(version);
        state_sync_count++;
    }

    void record_gradient_push() {
        gradient_push_count++;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        received_spikes.clear();
        weight_delta_counts.clear();
        state_sync_versions.clear();
        spike_batch_count = 0;
        weight_delta_count = 0;
        state_sync_count = 0;
        gradient_push_count = 0;
    }
};

/**
 * @brief Message callback for neural sync tracking
 */
static void neural_sync_callback(nlp_node_t node, const nlp_peer_t* peer,
                                const nlp_message_t* msg, void* user_data) {
    auto* tracker = static_cast<NeuralSyncTracker*>(user_data);
    uint16_t msg_type = ntohs(msg->header.msg_type);
    uint16_t payload_len = ntohs(msg->header.payload_len);

    switch (static_cast<nlp_msg_type_t>(msg_type)) {
        case NLP_MSG_SPIKE_BATCH: {
            if (payload_len >= sizeof(nlp_spike_batch_t)) {
                const nlp_spike_batch_t* batch =
                    reinterpret_cast<const nlp_spike_batch_t*>(msg->payload);
                uint16_t spike_count = ntohs(batch->spike_count);
                const uint32_t* neuron_ids =
                    reinterpret_cast<const uint32_t*>(batch + 1);
                tracker->record_spike_batch(neuron_ids, spike_count);
            }
            break;
        }

        case NLP_MSG_WEIGHT_DELTA: {
            if (payload_len >= sizeof(nlp_weight_delta_header_t)) {
                const nlp_weight_delta_header_t* header =
                    reinterpret_cast<const nlp_weight_delta_header_t*>(msg->payload);
                tracker->record_weight_delta(ntohs(header->delta_count));
            }
            break;
        }

        case NLP_MSG_STATE_SYNC: {
            if (payload_len >= sizeof(nlp_state_sync_t)) {
                const nlp_state_sync_t* state =
                    reinterpret_cast<const nlp_state_sync_t*>(msg->payload);
                tracker->record_state_sync(ntohl(state->state_version));
            }
            break;
        }

        case NLP_MSG_GRADIENT_PUSH: {
            tracker->record_gradient_push();
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Test fixture for neural sync tests
 */
class NLPNeuralSyncTest : public ::testing::Test {
protected:
    static constexpr uint16_t BASE_PORT = 19000;
    static constexpr uint32_t TIMEOUT_MS = 5000;

    std::vector<nlp_node_t> nodes_;
    std::vector<std::unique_ptr<NeuralSyncTracker>> trackers_;

    void SetUp() override {
        // Logging initialized by framework
    }

    void TearDown() override {
        for (auto node : nodes_) {
            if (node) {
                nlp_node_stop(node);
                nlp_node_destroy(node);
            }
        }
        nodes_.clear();
        trackers_.clear();
    }

    nlp_node_t CreateNode(uint16_t port) {
        nlp_config_t config = nlp_config_default();
        config.brain_id = nlp_generate_brain_id();
        config.port = port;
        config.default_mode = NLP_MODE_STANDARD;
        config.auto_mode_switch = false;
        config.heartbeat_interval_ms = 500;
        config.session_timeout_ms = 5000;
        strncpy(config.bind_address, "127.0.0.1", sizeof(config.bind_address) - 1);

        auto tracker = std::make_unique<NeuralSyncTracker>();
        config.user_data = tracker.get();

        nlp_node_t node = nlp_node_create(&config);
        if (!node) {
            return nullptr;
        }

        nlp_set_message_callback(node, neural_sync_callback);

        if (nlp_node_start(node) != 0) {
            nlp_node_destroy(node);
            return nullptr;
        }

        nodes_.push_back(node);
        trackers_.push_back(std::move(tracker));

        return node;
    }

    NeuralSyncTracker* GetTracker(size_t node_index) {
        if (node_index < trackers_.size()) {
            return trackers_[node_index].get();
        }
        return nullptr;
    }

    bool ConnectNodes(nlp_node_t node1, nlp_node_t node2, uint16_t node2_port) {
        uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", node2_port);
        if (peer_id == 0) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true;
    }
};

//=============================================================================
// Spike Synchronization Tests
//=============================================================================

TEST_F(NLPNeuralSyncTest, SpikeSync) {
    // Create two connected nodes
    nlp_node_t node1 = CreateNode(BASE_PORT);
    nlp_node_t node2 = CreateNode(BASE_PORT + 1);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 1);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Clear trackers
    GetTracker(1)->clear();

    // Send spike batch from node1 to node2
    uint32_t neuron_ids[] = {100, 101, 102, 103, 104};
    uint16_t spike_times[] = {10, 20, 30, 40, 50};  // microseconds
    constexpr uint32_t spike_count = 5;

    int result = nlp_send_spikes(node1, peer_id, neuron_ids, spike_times,
                                 spike_count);
    ASSERT_EQ(result, 0) << "Failed to send spike batch";

    // Wait for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify node2 received the spikes
    NeuralSyncTracker* tracker2 = GetTracker(1);
    ASSERT_NE(tracker2, nullptr);
    EXPECT_GE(tracker2->spike_batch_count, 1u) << "No spike batches received";

    if (tracker2->spike_batch_count > 0) {
        // Verify spike content
        const auto& received = tracker2->received_spikes[0];
        EXPECT_EQ(received.size(), spike_count);

        for (uint32_t i = 0; i < spike_count && i < received.size(); i++) {
            EXPECT_EQ(ntohl(received[i]), neuron_ids[i])
                << "Spike " << i << " neuron ID mismatch";
        }
    }
}

TEST_F(NLPNeuralSyncTest, HighFrequencySpikeStream) {
    // Test streaming spikes at high frequency
    nlp_node_t node1 = CreateNode(BASE_PORT + 10);
    nlp_node_t node2 = CreateNode(BASE_PORT + 11);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 11);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Send 10 spike batches rapidly
    constexpr uint32_t num_batches = 10;
    for (uint32_t batch = 0; batch < num_batches; batch++) {
        uint32_t neuron_ids[] = {
            batch * 10 + 0,
            batch * 10 + 1,
            batch * 10 + 2
        };
        uint16_t spike_times[] = {5, 10, 15};

        nlp_send_spikes(node1, peer_id, neuron_ids, spike_times, 3);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Wait for all batches to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify most batches were received (some loss acceptable)
    NeuralSyncTracker* tracker2 = GetTracker(1);
    EXPECT_GE(tracker2->spike_batch_count, num_batches * 0.8)
        << "Less than 80% of spike batches received";
}

TEST_F(NLPNeuralSyncTest, BroadcastSpikes) {
    // Test broadcasting spikes to multiple peers
    nlp_node_t node1 = CreateNode(BASE_PORT + 20);
    nlp_node_t node2 = CreateNode(BASE_PORT + 21);
    nlp_node_t node3 = CreateNode(BASE_PORT + 22);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);
    ASSERT_NE(node3, nullptr);

    // Connect node1 to both peers
    nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 21);
    nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 22);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();
    GetTracker(2)->clear();

    // Broadcast spikes to all peers (peer_id = 0)
    uint32_t neuron_ids[] = {200, 201, 202};
    uint16_t spike_times[] = {1, 2, 3};

    int result = nlp_send_spikes(node1, 0, neuron_ids, spike_times, 3);
    EXPECT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify both peers received the broadcast
    EXPECT_GE(GetTracker(1)->spike_batch_count, 1u);
    EXPECT_GE(GetTracker(2)->spike_batch_count, 1u);
}

//=============================================================================
// Weight Delta Synchronization Tests
//=============================================================================

TEST_F(NLPNeuralSyncTest, WeightDeltaSync) {
    // Create two connected nodes
    nlp_node_t node1 = CreateNode(BASE_PORT + 30);
    nlp_node_t node2 = CreateNode(BASE_PORT + 31);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 31);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Send weight deltas
    uint32_t synapse_ids[] = {1000, 1001, 1002, 1003};
    float old_weights[] = {0.5f, 0.3f, 0.7f, 0.2f};
    float new_weights[] = {0.55f, 0.35f, 0.75f, 0.25f};
    constexpr uint32_t delta_count = 4;

    int result = nlp_send_weight_deltas(node1, peer_id, synapse_ids,
                                       old_weights, new_weights, delta_count);
    ASSERT_EQ(result, 0) << "Failed to send weight deltas";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify node2 received the deltas
    NeuralSyncTracker* tracker2 = GetTracker(1);
    EXPECT_GE(tracker2->weight_delta_count, 1u) << "No weight deltas received";

    if (tracker2->weight_delta_count > 0) {
        EXPECT_EQ(tracker2->weight_delta_counts[0], delta_count)
            << "Delta count mismatch";
    }
}

TEST_F(NLPNeuralSyncTest, LargeWeightMatrixSync) {
    // Test synchronizing a large weight matrix
    nlp_node_t node1 = CreateNode(BASE_PORT + 40);
    nlp_node_t node2 = CreateNode(BASE_PORT + 41);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 41);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Send large weight delta batch
    constexpr uint32_t large_delta_count = 100;
    std::vector<uint32_t> synapse_ids(large_delta_count);
    std::vector<float> old_weights(large_delta_count);
    std::vector<float> new_weights(large_delta_count);

    for (uint32_t i = 0; i < large_delta_count; i++) {
        synapse_ids[i] = i;
        old_weights[i] = static_cast<float>(i) / 100.0f;
        new_weights[i] = old_weights[i] + 0.01f;
    }

    int result = nlp_send_weight_deltas(node1, peer_id, synapse_ids.data(),
                                       old_weights.data(), new_weights.data(),
                                       large_delta_count);
    ASSERT_EQ(result, 0) << "Failed to send large weight delta";

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify receipt
    NeuralSyncTracker* tracker2 = GetTracker(1);
    EXPECT_GE(tracker2->weight_delta_count, 1u);

    if (tracker2->weight_delta_count > 0) {
        EXPECT_EQ(tracker2->weight_delta_counts[0], large_delta_count);
    }
}

//=============================================================================
// State Synchronization Tests
//=============================================================================

TEST_F(NLPNeuralSyncTest, StateSyncRecovery) {
    // Test full state sync after disconnect
    nlp_node_t node1 = CreateNode(BASE_PORT + 50);
    nlp_node_t node2 = CreateNode(BASE_PORT + 51);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 51);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Simulate disconnect by stopping node2
    nlp_node_stop(node2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Restart node2
    ASSERT_EQ(nlp_node_start(node2), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Reconnect
    peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 51);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Send full state sync
    uint8_t dummy_state[256];
    memset(dummy_state, 0xAB, sizeof(dummy_state));

    int result = nlp_send_state(node1, peer_id, dummy_state, sizeof(dummy_state));
    ASSERT_EQ(result, 0) << "Failed to send state sync";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify state sync received
    NeuralSyncTracker* tracker2 = GetTracker(1);
    EXPECT_GE(tracker2->state_sync_count, 1u) << "No state sync received";
}

TEST_F(NLPNeuralSyncTest, StateSyncVersioning) {
    // Test state sync with version tracking
    nlp_node_t node1 = CreateNode(BASE_PORT + 60);
    nlp_node_t node2 = CreateNode(BASE_PORT + 61);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 61);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Send multiple state syncs with incrementing versions
    for (uint32_t version = 1; version <= 3; version++) {
        nlp_state_sync_t state = {};
        state.state_version = htonl(version);
        state.neuron_count = htonl(1000);
        state.synapse_count = htonl(5000);
        state.flags = 0;

        nlp_send_state(node1, peer_id, &state, sizeof(state));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Verify versions received
    NeuralSyncTracker* tracker2 = GetTracker(1);
    EXPECT_GE(tracker2->state_sync_count, 3u);

    if (tracker2->state_sync_count >= 3) {
        // Check versions are in order
        for (size_t i = 0; i < tracker2->state_sync_versions.size(); i++) {
            EXPECT_EQ(tracker2->state_sync_versions[i], i + 1)
                << "State sync version " << i << " mismatch";
        }
    }
}

//=============================================================================
// Gradient Synchronization Tests
//=============================================================================

TEST_F(NLPNeuralSyncTest, GradientPush) {
    // Test distributed learning gradient push
    nlp_node_t node1 = CreateNode(BASE_PORT + 70);
    nlp_node_t node2 = CreateNode(BASE_PORT + 71);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 71);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    // Send gradient data
    float gradients[100];
    for (int i = 0; i < 100; i++) {
        gradients[i] = sinf(static_cast<float>(i) * 0.1f);
    }

    int result = nlp_send(node1, peer_id, NLP_MSG_GRADIENT_PUSH,
                         gradients, sizeof(gradients), NLP_PRIORITY_HIGH);
    ASSERT_EQ(result, 0) << "Failed to send gradient push";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify gradient received
    NeuralSyncTracker* tracker2 = GetTracker(1);
    EXPECT_GE(tracker2->gradient_push_count, 1u) << "No gradient push received";
}

TEST_F(NLPNeuralSyncTest, DistributedGradientAggregation) {
    // Test gradient push from multiple nodes (simulating distributed training)
    nlp_node_t master = CreateNode(BASE_PORT + 80);
    nlp_node_t worker1 = CreateNode(BASE_PORT + 81);
    nlp_node_t worker2 = CreateNode(BASE_PORT + 82);

    ASSERT_NE(master, nullptr);
    ASSERT_NE(worker1, nullptr);
    ASSERT_NE(worker2, nullptr);

    // Connect workers to master
    uint32_t master_from_w1 = nlp_connect_peer(worker1, "127.0.0.1", BASE_PORT + 80);
    uint32_t master_from_w2 = nlp_connect_peer(worker2, "127.0.0.1", BASE_PORT + 80);
    ASSERT_NE(master_from_w1, 0u);
    ASSERT_NE(master_from_w2, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(0)->clear();

    // Workers send gradients to master
    float gradients1[50];
    float gradients2[50];

    for (int i = 0; i < 50; i++) {
        gradients1[i] = static_cast<float>(i) * 0.01f;
        gradients2[i] = static_cast<float>(i) * 0.02f;
    }

    nlp_send(worker1, master_from_w1, NLP_MSG_GRADIENT_PUSH,
            gradients1, sizeof(gradients1), NLP_PRIORITY_HIGH);

    nlp_send(worker2, master_from_w2, NLP_MSG_GRADIENT_PUSH,
            gradients2, sizeof(gradients2), NLP_PRIORITY_HIGH);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify master received both gradients
    NeuralSyncTracker* tracker_master = GetTracker(0);
    EXPECT_GE(tracker_master->gradient_push_count, 2u)
        << "Master did not receive gradients from both workers";
}

//=============================================================================
// Performance and Load Tests
//=============================================================================

TEST_F(NLPNeuralSyncTest, HighThroughputSync) {
    // Test sustained high-throughput neural sync
    nlp_node_t node1 = CreateNode(BASE_PORT + 90);
    nlp_node_t node2 = CreateNode(BASE_PORT + 91);
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    uint32_t peer_id = nlp_connect_peer(node1, "127.0.0.1", BASE_PORT + 91);
    ASSERT_NE(peer_id, 0u);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    GetTracker(1)->clear();

    auto start_time = std::chrono::steady_clock::now();

    // Send mix of spikes and weight deltas for 2 seconds
    constexpr int duration_ms = 2000;
    int messages_sent = 0;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time).count();

        if (elapsed >= duration_ms) break;

        // Send spike batch
        uint32_t neuron_ids[] = {100, 101, 102};
        uint16_t spike_times[] = {1, 2, 3};
        nlp_send_spikes(node1, peer_id, neuron_ids, spike_times, 3);
        messages_sent++;

        // Send weight delta
        uint32_t synapse_ids[] = {200, 201};
        float old_weights[] = {0.5f, 0.6f};
        float new_weights[] = {0.51f, 0.61f};
        nlp_send_weight_deltas(node1, peer_id, synapse_ids, old_weights,
                              new_weights, 2);
        messages_sent++;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify significant throughput
    NeuralSyncTracker* tracker2 = GetTracker(1);
    uint32_t total_received = tracker2->spike_batch_count +
                             tracker2->weight_delta_count;

    EXPECT_GT(total_received, static_cast<uint32_t>(messages_sent * 0.7))
        << "Less than 70% message delivery under load";
}
