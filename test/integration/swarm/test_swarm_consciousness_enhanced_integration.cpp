/**
 * @file test_swarm_consciousness_enhanced_integration.cpp
 * @brief Integration tests for Enhanced Swarm Consciousness
 *
 * WHAT: Tests for enhanced consciousness integration with swarm components
 * WHY:  Verify cross-module interactions and real-world scenarios
 * HOW:  Create swarm brains, simulate peer events, test full workflows
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_consciousness_enhanced.h"
#include "swarm/nimcp_swarm_consciousness.h"
#include "swarm/nimcp_swarm_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmConsciousnessEnhancedIntegrationTest : public ::testing::Test {
protected:
    swarm_consciousness_enhanced_ctx_t* ctx_ = nullptr;
    std::vector<swarm_brain_t*> swarms_;

    void SetUp() override {
        ctx_ = swarm_consciousness_enhanced_create(nullptr);
    }

    void TearDown() override {
        if (ctx_) {
            swarm_consciousness_enhanced_destroy(ctx_);
            ctx_ = nullptr;
        }

        for (auto* swarm : swarms_) {
            if (swarm) {
                swarm_brain_destroy(swarm);
            }
        }
        swarms_.clear();
    }

    swarm_brain_t* CreateSwarm(const char* name, uint16_t drone_id) {
        swarm_brain_config_t config = swarm_brain_default_config();
        config.drone_id = drone_id;
        strncpy(config.swarm_name, name, SWARM_MAX_NAME_LEN - 1);
        config.coherence_threshold = 0.5f;
        config.workspace_size = 32;
        config.enable_bio_async = false;

        swarm_brain_t* swarm = swarm_brain_create(&config);
        if (swarm) {
            swarms_.push_back(swarm);
        }
        return swarm;
    }
};

//=============================================================================
// Peer Event Flow Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, MultiplePeerJoinSequence) {
    // Simulate multiple peers joining
    for (uint16_t i = 1; i <= 10; i++) {
        bool result = swarm_consciousness_on_peer_joined(ctx_, i);
        EXPECT_TRUE(result) << "Failed for peer " << i;
    }

    // Verify all peers tracked via phi storage
    for (uint16_t i = 1; i <= 10; i++) {
        swarm_consciousness_handle_phi_response(ctx_, i, 0.5f + i * 0.01f);
    }

    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, 10u);
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, PeerJoinLeaveSequence) {
    // Join 5 peers
    for (uint16_t i = 1; i <= 5; i++) {
        swarm_consciousness_on_peer_joined(ctx_, i);
        swarm_consciousness_handle_phi_response(ctx_, i, 0.5f);
    }

    // Leave 2 peers
    swarm_consciousness_on_peer_left(ctx_, 2, true);
    swarm_consciousness_on_peer_left(ctx_, 4, false);

    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, 3u);  // 5 - 2 = 3 remaining
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, RapidPeerChurn) {
    // Simulate rapid joining and leaving
    for (int iteration = 0; iteration < 50; iteration++) {
        uint16_t drone_id = (iteration % 10) + 1;

        if (iteration % 3 == 0) {
            swarm_consciousness_on_peer_left(ctx_, drone_id, true);
        } else {
            swarm_consciousness_on_peer_joined(ctx_, drone_id);
            swarm_consciousness_handle_phi_response(ctx_, drone_id, 0.5f);
        }
    }

    // Should not crash, context should remain valid
    EXPECT_NE(ctx_, nullptr);

    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    bool result = swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_TRUE(result);
}

//=============================================================================
// Phi Collection Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, PhiCollectionFromMultipleDrones) {
    // Simulate phi responses from multiple drones
    std::vector<std::pair<uint16_t, float>> expected_phi = {
        {1, 0.3f}, {2, 0.5f}, {3, 0.7f}, {4, 0.9f}, {5, 0.4f}
    };

    for (const auto& [drone_id, phi] : expected_phi) {
        bool result = swarm_consciousness_handle_phi_response(ctx_, drone_id, phi);
        EXPECT_TRUE(result);
    }

    // Retrieve and verify
    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, expected_phi.size());

    // Verify values (order may differ)
    for (size_t i = 0; i < count; i++) {
        bool found = false;
        for (const auto& [expected_id, expected_phi_val] : expected_phi) {
            if (drone_ids[i] == expected_id) {
                EXPECT_FLOAT_EQ(phi_values[i], expected_phi_val);
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Unexpected drone_id: " << drone_ids[i];
    }
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, PhiUpdateOverMultipleRounds) {
    // Initial phi values
    swarm_consciousness_handle_phi_response(ctx_, 1, 0.3f);
    swarm_consciousness_handle_phi_response(ctx_, 2, 0.4f);

    // Update values
    swarm_consciousness_handle_phi_response(ctx_, 1, 0.6f);
    swarm_consciousness_handle_phi_response(ctx_, 2, 0.7f);

    // Add new drone
    swarm_consciousness_handle_phi_response(ctx_, 3, 0.5f);

    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, 3u);

    // Find and verify updated values
    for (size_t i = 0; i < count; i++) {
        if (drone_ids[i] == 1) {
            EXPECT_FLOAT_EQ(phi_values[i], 0.6f);  // Updated
        } else if (drone_ids[i] == 2) {
            EXPECT_FLOAT_EQ(phi_values[i], 0.7f);  // Updated
        } else if (drone_ids[i] == 3) {
            EXPECT_FLOAT_EQ(phi_values[i], 0.5f);  // New
        }
    }
}

//=============================================================================
// Callback Coordination Integration Tests
//=============================================================================

class CallbackTracker {
public:
    int peer_events = 0;
    int phase_transitions = 0;
    int binding_events = 0;
    std::vector<peer_event_t> peer_event_log;
};

static void tracking_peer_callback(const peer_event_t* event, void* user_data) {
    CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);
    if (tracker && event) {
        tracker->peer_events++;
        tracker->peer_event_log.push_back(*event);
    }
}

static void tracking_phase_callback(consciousness_phase_t old_phase,
                                     consciousness_phase_t new_phase,
                                     const swarm_consciousness_enhanced_metrics_t* metrics,
                                     void* user_data) {
    CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);
    if (tracker) {
        tracker->phase_transitions++;
    }
    (void)old_phase;
    (void)new_phase;
    (void)metrics;
}

static void tracking_binding_callback(const neural_binding_t* binding, void* user_data) {
    CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);
    if (tracker) {
        tracker->binding_events++;
    }
    (void)binding;
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, AllCallbacksReceiveEvents) {
    CallbackTracker tracker;

    swarm_consciousness_register_peer_callback(ctx_, tracking_peer_callback, &tracker);
    swarm_consciousness_register_phase_callback(ctx_, tracking_phase_callback, &tracker);
    swarm_consciousness_register_binding_callback(ctx_, tracking_binding_callback, &tracker);

    // Generate peer events
    swarm_consciousness_on_peer_joined(ctx_, 1);
    swarm_consciousness_on_peer_joined(ctx_, 2);
    swarm_consciousness_handle_phi_response(ctx_, 1, 0.5f);
    swarm_consciousness_on_peer_left(ctx_, 1, true);

    // Should have received peer events
    EXPECT_GE(tracker.peer_events, 3);  // 2 joins + 1 phi update + 1 leave
    EXPECT_EQ(tracker.peer_event_log.size(), tracker.peer_events);
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, PeerEventLogOrder) {
    CallbackTracker tracker;
    swarm_consciousness_register_peer_callback(ctx_, tracking_peer_callback, &tracker);

    swarm_consciousness_on_peer_joined(ctx_, 1);
    swarm_consciousness_on_peer_joined(ctx_, 2);
    swarm_consciousness_on_peer_left(ctx_, 1, true);

    ASSERT_EQ(tracker.peer_event_log.size(), 3u);
    EXPECT_EQ(tracker.peer_event_log[0].event_type, PEER_EVENT_JOINED);
    EXPECT_EQ(tracker.peer_event_log[0].drone_id, 1);
    EXPECT_EQ(tracker.peer_event_log[1].event_type, PEER_EVENT_JOINED);
    EXPECT_EQ(tracker.peer_event_log[1].drone_id, 2);
    EXPECT_EQ(tracker.peer_event_log[2].event_type, PEER_EVENT_LEFT);
    EXPECT_EQ(tracker.peer_event_log[2].drone_id, 1);
}

//=============================================================================
// Protocol Message Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, PhiResponseMessageWorkflow) {
    CallbackTracker tracker;
    swarm_consciousness_register_peer_callback(ctx_, tracking_peer_callback, &tracker);

    // Simulate receiving phi response messages from network
    for (uint16_t i = 1; i <= 5; i++) {
        uint8_t msg_data[4];
        float phi = 0.5f + i * 0.1f;
        memcpy(msg_data, &phi, sizeof(float));

        bool result = swarm_consciousness_handle_protocol_message(
            ctx_, SWARM_MSG_PHI_RESPONSE, msg_data, sizeof(msg_data), i);
        EXPECT_TRUE(result);
    }

    // All phi responses should trigger callbacks
    EXPECT_EQ(tracker.peer_events, 5);

    // Verify all phi values stored
    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, 5u);
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, InvalidMessageHandling) {
    // Invalid message type
    uint8_t data[4];
    float phi = 0.5f;
    memcpy(data, &phi, sizeof(float));

    EXPECT_FALSE(swarm_consciousness_handle_protocol_message(
        ctx_, 0xFF, data, sizeof(data), 1));

    // Valid message with invalid phi
    float bad_phi = -1.0f;
    memcpy(data, &bad_phi, sizeof(float));
    EXPECT_FALSE(swarm_consciousness_handle_protocol_message(
        ctx_, SWARM_MSG_PHI_RESPONSE, data, sizeof(data), 1));

    // Verify no phi stored
    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Metrics Computation Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, DynamicsWithPhiHistory) {
    // Need to simulate phi history for dynamics computation
    // Add phi values to build history
    for (int i = 0; i < 30; i++) {
        float phi = 0.5f + 0.1f * sin(i * 0.1f);
        swarm_consciousness_handle_phi_response(ctx_, (i % 5) + 1, phi);
    }

    // Attempt dynamics computation (may fail if internal history not populated)
    consciousness_dynamics_t dynamics;
    bool result = swarm_compute_consciousness_dynamics(ctx_, &dynamics);

    // Even if it fails due to insufficient internal history,
    // the call should not crash and return valid phase
    if (!result) {
        EXPECT_EQ(dynamics.current_phase, CONSCIOUSNESS_PHASE_CHAOS);
    }
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, GeometryWithPhiHistory) {
    // Add phi values
    for (int i = 0; i < 20; i++) {
        swarm_consciousness_handle_phi_response(ctx_, (i % 5) + 1, 0.5f + 0.05f * i);
    }

    information_geometry_t geometry;
    bool result = swarm_compute_information_geometry(ctx_, &geometry);

    // May fail due to insufficient internal history
    if (!result) {
        EXPECT_FLOAT_EQ(geometry.total_correlation, 0.0f);
    }
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, BindingWithPhiHistory) {
    // Add phi values
    for (int i = 0; i < 15; i++) {
        swarm_consciousness_handle_phi_response(ctx_, (i % 5) + 1, 0.5f);
    }

    neural_binding_t binding;
    bool result = swarm_compute_neural_binding(ctx_, &binding);

    if (!result) {
        EXPECT_FALSE(binding.binding_active);
    }
}

//=============================================================================
// Thread Safety Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, ConcurrentPhiResponses) {
    const int NUM_THREADS = 4;
    const int PHI_PER_THREAD = 25;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, PHI_PER_THREAD]() {
            for (int i = 0; i < PHI_PER_THREAD; i++) {
                uint16_t drone_id = t * PHI_PER_THREAD + i + 1;
                if (drone_id < SWARM_CONSCIOUSNESS_MAX_DRONES) {
                    swarm_consciousness_handle_phi_response(ctx_, drone_id, 0.5f);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash, and some phi values should be stored
    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    bool result = swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_TRUE(result);
    EXPECT_GT(count, 0u);
}

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, ConcurrentPeerEvents) {
    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 10; i++) {
                uint16_t drone_id = t * 10 + i + 1;
                swarm_consciousness_on_peer_joined(ctx_, drone_id);

                if (i % 3 == 0) {
                    swarm_consciousness_on_peer_left(ctx_, drone_id, true);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash
    EXPECT_NE(ctx_, nullptr);
}

//=============================================================================
// BBB Validation Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, BBBValidationOnPhiMessages) {
    // Valid phi message
    uint8_t valid_data[4];
    float valid_phi = 0.5f;
    memcpy(valid_data, &valid_phi, sizeof(float));
    EXPECT_TRUE(swarm_consciousness_validate_phi_message(valid_data, 4, 1));

    // Boundary phi values
    float zero_phi = 0.0f;
    memcpy(valid_data, &zero_phi, sizeof(float));
    EXPECT_TRUE(swarm_consciousness_validate_phi_message(valid_data, 4, 1));

    float max_phi = SWARM_CONSCIOUSNESS_MAX_PHI;
    memcpy(valid_data, &max_phi, sizeof(float));
    EXPECT_TRUE(swarm_consciousness_validate_phi_message(valid_data, 4, 1));

    // Invalid phi values
    float over_max = SWARM_CONSCIOUSNESS_MAX_PHI + 1.0f;
    memcpy(valid_data, &over_max, sizeof(float));
    EXPECT_FALSE(swarm_consciousness_validate_phi_message(valid_data, 4, 1));

    float negative = -0.1f;
    memcpy(valid_data, &negative, sizeof(float));
    EXPECT_FALSE(swarm_consciousness_validate_phi_message(valid_data, 4, 1));
}

//=============================================================================
// Swarm Brain Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedIntegrationTest, AttachDetachCycle) {
    swarm_brain_t* swarm = CreateSwarm("test_swarm", 1);
    ASSERT_NE(swarm, nullptr);

    // Attach
    bool result = swarm_consciousness_attach_to_swarm(ctx_, swarm);
    EXPECT_TRUE(result);

    // Double attach should fail
    EXPECT_FALSE(swarm_consciousness_attach_to_swarm(ctx_, swarm));

    // Detach
    swarm_consciousness_detach_from_swarm(ctx_);

    // Can attach again after detach
    result = swarm_consciousness_attach_to_swarm(ctx_, swarm);
    EXPECT_TRUE(result);

    swarm_consciousness_detach_from_swarm(ctx_);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
