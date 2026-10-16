/**
 * @file test_swarm_consciousness_enhanced.cpp
 * @brief Unit tests for Enhanced Swarm Gestalt Consciousness
 *
 * WHAT: Tests for enhanced collective consciousness features
 * WHY:  Ensure peer callbacks, phi collection, dynamics, binding work correctly
 * HOW:  Test lifecycle, peer events, phi collection, metrics computation
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_consciousness_enhanced.h"
#include "swarm/nimcp_swarm_consciousness.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmConsciousnessEnhancedTest : public ::testing::Test {
protected:
    swarm_consciousness_enhanced_ctx_t* ctx_ = nullptr;

    void SetUp() override {
        // Create with default config
        ctx_ = swarm_consciousness_enhanced_create(nullptr);
    }

    void TearDown() override {
        if (ctx_) {
            swarm_consciousness_enhanced_destroy(ctx_);
            ctx_ = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, DefaultConfigHasValidValues) {
    swarm_consciousness_enhanced_config_t config =
        swarm_consciousness_enhanced_default_config();

    // Base config
    EXPECT_GE(config.base.phi_aggregation_method, PHI_AGGREGATION_SUM);
    EXPECT_LE(config.base.phi_aggregation_method, PHI_AGGREGATION_SYNERGISTIC);
    EXPECT_GE(config.base.integration_weight, 0.0f);
    EXPECT_LE(config.base.integration_weight, 1.0f);

    // Peer event settings
    EXPECT_TRUE(config.enable_peer_callbacks);
    EXPECT_TRUE(config.auto_collect_phi_on_join);
    EXPECT_GT(config.phi_collection_timeout_ms, 0u);

    // Geometry settings
    EXPECT_TRUE(config.enable_geometry);
    EXPECT_GT(config.geometry_history_size, 0u);
    EXPECT_GT(config.entropy_bin_width, 0.0f);

    // Dynamics settings
    EXPECT_TRUE(config.enable_dynamics);
    EXPECT_GT(config.dynamics_window_size, 0u);
    EXPECT_GT(config.critical_variance_threshold, 0.0f);

    // Binding settings
    EXPECT_TRUE(config.enable_binding);
    EXPECT_GT(config.gamma_frequency_hz, 0.0f);
    EXPECT_GT(config.phase_coherence_threshold, 0.0f);
    EXPECT_LE(config.phase_coherence_threshold, 1.0f);

    // Hierarchy settings
    EXPECT_TRUE(config.enable_hierarchy);
    EXPECT_GT(config.squad_size, 0u);
    EXPECT_GT(config.platoon_size, config.squad_size);

    // Resilience settings
    EXPECT_TRUE(config.enable_resilience);
    EXPECT_GT(config.simulated_dropout_rate, 0.0f);
    EXPECT_LT(config.simulated_dropout_rate, 1.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, CreateWithDefaultConfigSucceeds) {
    ASSERT_NE(ctx_, nullptr);
}

TEST_F(SwarmConsciousnessEnhancedTest, CreateWithNullConfigSucceeds) {
    // Already created in SetUp with nullptr
    ASSERT_NE(ctx_, nullptr);
}

TEST_F(SwarmConsciousnessEnhancedTest, CreateWithCustomConfigSucceeds) {
    swarm_consciousness_enhanced_config_t config =
        swarm_consciousness_enhanced_default_config();
    config.enable_geometry = false;
    config.enable_dynamics = false;
    config.enable_binding = false;

    swarm_consciousness_enhanced_ctx_t* custom_ctx =
        swarm_consciousness_enhanced_create(&config);
    ASSERT_NE(custom_ctx, nullptr);

    swarm_consciousness_enhanced_destroy(custom_ctx);
}

TEST_F(SwarmConsciousnessEnhancedTest, DestroyNullIsSafe) {
    // Should not crash
    swarm_consciousness_enhanced_destroy(nullptr);
}

TEST_F(SwarmConsciousnessEnhancedTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        swarm_consciousness_enhanced_ctx_t* temp =
            swarm_consciousness_enhanced_create(nullptr);
        ASSERT_NE(temp, nullptr) << "Iteration " << i;
        swarm_consciousness_enhanced_destroy(temp);
    }
}

//=============================================================================
// Peer Callback Tests
//=============================================================================

static std::atomic<int> g_peer_callback_count{0};
static peer_event_t g_last_peer_event;

static void test_peer_callback(const peer_event_t* event, void* user_data) {
    g_peer_callback_count++;
    if (event) {
        g_last_peer_event = *event;
    }
    if (user_data) {
        int* flag = static_cast<int*>(user_data);
        *flag = 1;
    }
}

TEST_F(SwarmConsciousnessEnhancedTest, RegisterPeerCallbackSucceeds) {
    bool result = swarm_consciousness_register_peer_callback(
        ctx_, test_peer_callback, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(SwarmConsciousnessEnhancedTest, UnregisterPeerCallbackSucceeds) {
    swarm_consciousness_register_peer_callback(ctx_, test_peer_callback, nullptr);
    swarm_consciousness_unregister_peer_callback(ctx_);
    // Should not crash, callback cleared
}

TEST_F(SwarmConsciousnessEnhancedTest, PeerJoinedInvokesCallback) {
    g_peer_callback_count = 0;
    int user_flag = 0;

    swarm_consciousness_register_peer_callback(ctx_, test_peer_callback, &user_flag);

    bool result = swarm_consciousness_on_peer_joined(ctx_, 42);
    EXPECT_TRUE(result);
    EXPECT_EQ(g_peer_callback_count, 1);
    EXPECT_EQ(g_last_peer_event.event_type, PEER_EVENT_JOINED);
    EXPECT_EQ(g_last_peer_event.drone_id, 42);
    EXPECT_EQ(user_flag, 1);
}

TEST_F(SwarmConsciousnessEnhancedTest, PeerLeftInvokesCallback) {
    g_peer_callback_count = 0;

    swarm_consciousness_register_peer_callback(ctx_, test_peer_callback, nullptr);

    // First add the peer
    swarm_consciousness_on_peer_joined(ctx_, 42);

    // Then remove
    g_peer_callback_count = 0;
    bool result = swarm_consciousness_on_peer_left(ctx_, 42, true);
    EXPECT_TRUE(result);
    EXPECT_EQ(g_peer_callback_count, 1);
    EXPECT_EQ(g_last_peer_event.event_type, PEER_EVENT_LEFT);
    EXPECT_EQ(g_last_peer_event.drone_id, 42);
}

TEST_F(SwarmConsciousnessEnhancedTest, PeerTimeoutInvokesCallback) {
    g_peer_callback_count = 0;

    swarm_consciousness_register_peer_callback(ctx_, test_peer_callback, nullptr);
    swarm_consciousness_on_peer_joined(ctx_, 42);

    g_peer_callback_count = 0;
    bool result = swarm_consciousness_on_peer_left(ctx_, 42, false);  // Not graceful
    EXPECT_TRUE(result);
    EXPECT_EQ(g_last_peer_event.event_type, PEER_EVENT_TIMEOUT);
}

TEST_F(SwarmConsciousnessEnhancedTest, NullContextPeerOperationsFail) {
    EXPECT_FALSE(swarm_consciousness_on_peer_joined(nullptr, 42));
    EXPECT_FALSE(swarm_consciousness_on_peer_left(nullptr, 42, true));
}

//=============================================================================
// Phi Collection Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, HandlePhiResponseStoresValue) {
    bool result = swarm_consciousness_handle_phi_response(ctx_, 42, 0.75f);
    EXPECT_TRUE(result);

    // Retrieve stored values
    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    result = swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_TRUE(result);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(drone_ids[0], 42);
    EXPECT_FLOAT_EQ(phi_values[0], 0.75f);
}

TEST_F(SwarmConsciousnessEnhancedTest, HandlePhiResponseRejectsInvalidPhi) {
    // Negative phi
    EXPECT_FALSE(swarm_consciousness_handle_phi_response(ctx_, 1, -1.0f));

    // Phi too high
    EXPECT_FALSE(swarm_consciousness_handle_phi_response(ctx_, 2, 100.0f));

    // NaN phi
    EXPECT_FALSE(swarm_consciousness_handle_phi_response(ctx_, 3, NAN));

    // Inf phi
    EXPECT_FALSE(swarm_consciousness_handle_phi_response(ctx_, 4, INFINITY));
}

TEST_F(SwarmConsciousnessEnhancedTest, HandlePhiResponseUpdatesExisting) {
    // First phi
    swarm_consciousness_handle_phi_response(ctx_, 42, 0.5f);

    // Update phi
    swarm_consciousness_handle_phi_response(ctx_, 42, 0.8f);

    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, 1u);  // Still one entry
    EXPECT_FLOAT_EQ(phi_values[0], 0.8f);  // Updated value
}

TEST_F(SwarmConsciousnessEnhancedTest, HandlePhiResponseInvokesCallback) {
    g_peer_callback_count = 0;
    swarm_consciousness_register_peer_callback(ctx_, test_peer_callback, nullptr);

    swarm_consciousness_handle_phi_response(ctx_, 42, 0.5f);
    EXPECT_EQ(g_peer_callback_count, 1);
    EXPECT_EQ(g_last_peer_event.event_type, PEER_EVENT_PHI_UPDATE);
    EXPECT_EQ(g_last_peer_event.drone_id, 42);
    EXPECT_FLOAT_EQ(g_last_peer_event.phi_value, 0.5f);
}

TEST_F(SwarmConsciousnessEnhancedTest, GetRemotePhiWithNullParametersFails) {
    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    EXPECT_FALSE(swarm_consciousness_get_remote_phi(nullptr, phi_values, drone_ids, &count));
    EXPECT_FALSE(swarm_consciousness_get_remote_phi(ctx_, nullptr, drone_ids, &count));
    EXPECT_FALSE(swarm_consciousness_get_remote_phi(ctx_, phi_values, nullptr, &count));
    EXPECT_FALSE(swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, nullptr));
}

//=============================================================================
// Phase Transition Tests
//=============================================================================

static std::atomic<int> g_phase_callback_count{0};
static consciousness_phase_t g_last_old_phase;
static consciousness_phase_t g_last_new_phase;

static void test_phase_callback(consciousness_phase_t old_phase,
                                 consciousness_phase_t new_phase,
                                 const swarm_consciousness_enhanced_metrics_t* metrics,
                                 void* user_data) {
    g_phase_callback_count++;
    g_last_old_phase = old_phase;
    g_last_new_phase = new_phase;
    (void)metrics;
    (void)user_data;
}

TEST_F(SwarmConsciousnessEnhancedTest, RegisterPhaseCallbackSucceeds) {
    bool result = swarm_consciousness_register_phase_callback(
        ctx_, test_phase_callback, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(SwarmConsciousnessEnhancedTest, GetPhaseReturnsValidPhase) {
    consciousness_phase_t phase = swarm_consciousness_get_phase(ctx_);
    EXPECT_GE(phase, CONSCIOUSNESS_PHASE_CHAOS);
    EXPECT_LE(phase, CONSCIOUSNESS_PHASE_FROZEN);
}

TEST_F(SwarmConsciousnessEnhancedTest, DetectPhaseTransitionReturnsPhase) {
    bool detected = false;
    consciousness_phase_t phase = swarm_consciousness_detect_phase_transition(
        ctx_, &detected);

    EXPECT_GE(phase, CONSCIOUSNESS_PHASE_CHAOS);
    EXPECT_LE(phase, CONSCIOUSNESS_PHASE_FROZEN);
}

TEST_F(SwarmConsciousnessEnhancedTest, NullContextPhaseOperationsReturnDefaults) {
    bool detected = true;
    EXPECT_EQ(swarm_consciousness_detect_phase_transition(nullptr, &detected),
              CONSCIOUSNESS_PHASE_CHAOS);
    EXPECT_EQ(swarm_consciousness_get_phase(nullptr), CONSCIOUSNESS_PHASE_CHAOS);
}

//=============================================================================
// Neural Binding Tests
//=============================================================================

static std::atomic<int> g_binding_callback_count{0};

static void test_binding_callback(const neural_binding_t* binding, void* user_data) {
    g_binding_callback_count++;
    (void)binding;
    (void)user_data;
}

TEST_F(SwarmConsciousnessEnhancedTest, RegisterBindingCallbackSucceeds) {
    bool result = swarm_consciousness_register_binding_callback(
        ctx_, test_binding_callback, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(SwarmConsciousnessEnhancedTest, IsBoundReturnsFalseInitially) {
    EXPECT_FALSE(swarm_consciousness_is_bound(ctx_, 0.0f));
}

TEST_F(SwarmConsciousnessEnhancedTest, GetBindingSucceeds) {
    neural_binding_t binding;
    bool result = swarm_consciousness_get_binding(ctx_, &binding);
    EXPECT_TRUE(result);

    // Initial values should be valid
    EXPECT_GE(binding.phase_coherence, 0.0f);
    EXPECT_LE(binding.phase_coherence, 1.0f);
}

TEST_F(SwarmConsciousnessEnhancedTest, GetBindingWithNullParametersFails) {
    neural_binding_t binding;
    EXPECT_FALSE(swarm_consciousness_get_binding(nullptr, &binding));
    EXPECT_FALSE(swarm_consciousness_get_binding(ctx_, nullptr));
}

//=============================================================================
// Information Geometry Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, ComputeInformationGeometryWithInsufficientData) {
    information_geometry_t geometry;
    // No history yet, should fail or return zeros
    bool result = swarm_compute_information_geometry(ctx_, &geometry);

    // With no history, should fail
    EXPECT_FALSE(result);
}

TEST_F(SwarmConsciousnessEnhancedTest, ComputeInformationGeometryNullParams) {
    information_geometry_t geometry;
    EXPECT_FALSE(swarm_compute_information_geometry(nullptr, &geometry));
    EXPECT_FALSE(swarm_compute_information_geometry(ctx_, nullptr));
}

//=============================================================================
// Consciousness Dynamics Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, ComputeConsciousnessDynamicsWithInsufficientData) {
    consciousness_dynamics_t dynamics;
    bool result = swarm_compute_consciousness_dynamics(ctx_, &dynamics);

    // With no history, should fail
    EXPECT_FALSE(result);
    EXPECT_EQ(dynamics.current_phase, CONSCIOUSNESS_PHASE_CHAOS);
}

TEST_F(SwarmConsciousnessEnhancedTest, ComputeConsciousnessDynamicsNullParams) {
    consciousness_dynamics_t dynamics;
    EXPECT_FALSE(swarm_compute_consciousness_dynamics(nullptr, &dynamics));
    EXPECT_FALSE(swarm_compute_consciousness_dynamics(ctx_, nullptr));
}

//=============================================================================
// Neural Binding Computation Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, ComputeNeuralBindingWithInsufficientData) {
    neural_binding_t binding;
    bool result = swarm_compute_neural_binding(ctx_, &binding);

    // With no history, should fail
    EXPECT_FALSE(result);
}

TEST_F(SwarmConsciousnessEnhancedTest, ComputeNeuralBindingNullParams) {
    neural_binding_t binding;
    EXPECT_FALSE(swarm_compute_neural_binding(nullptr, &binding));
    EXPECT_FALSE(swarm_compute_neural_binding(ctx_, nullptr));
}

//=============================================================================
// Hierarchical Consciousness Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, ComputeHierarchicalConsciousnessNullParams) {
    hierarchical_consciousness_t hierarchy;
    EXPECT_FALSE(swarm_compute_hierarchical_consciousness(nullptr, nullptr, &hierarchy));
    EXPECT_FALSE(swarm_compute_hierarchical_consciousness(ctx_, nullptr, &hierarchy));
    EXPECT_FALSE(swarm_compute_hierarchical_consciousness(ctx_, nullptr, nullptr));
}

//=============================================================================
// Resilience Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, ComputeConsciousnessResilienceNullParams) {
    consciousness_resilience_t resilience;
    EXPECT_FALSE(swarm_compute_consciousness_resilience(nullptr, nullptr, &resilience));
    EXPECT_FALSE(swarm_compute_consciousness_resilience(ctx_, nullptr, &resilience));
    EXPECT_FALSE(swarm_compute_consciousness_resilience(ctx_, nullptr, nullptr));
}

//=============================================================================
// Protocol Message Handling Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, HandleProtocolMessagePhiResponse) {
    // Create a phi response message
    uint8_t data[4];
    float phi = 0.65f;
    memcpy(data, &phi, sizeof(float));

    bool result = swarm_consciousness_handle_protocol_message(
        ctx_, SWARM_MSG_PHI_RESPONSE, data, sizeof(data), 42);
    EXPECT_TRUE(result);

    // Verify phi was stored
    float phi_values[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint16_t drone_ids[SWARM_CONSCIOUSNESS_MAX_DRONES];
    uint32_t count = 0;

    swarm_consciousness_get_remote_phi(ctx_, phi_values, drone_ids, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(phi_values[0], 0.65f);
}

TEST_F(SwarmConsciousnessEnhancedTest, HandleProtocolMessageInvalidType) {
    uint8_t data[4] = {0};
    bool result = swarm_consciousness_handle_protocol_message(
        ctx_, 0xFF, data, sizeof(data), 42);  // Invalid type
    EXPECT_FALSE(result);
}

TEST_F(SwarmConsciousnessEnhancedTest, HandleProtocolMessageShortData) {
    uint8_t data[2] = {0};  // Too short for phi
    bool result = swarm_consciousness_handle_protocol_message(
        ctx_, SWARM_MSG_PHI_RESPONSE, data, sizeof(data), 42);
    EXPECT_FALSE(result);
}

//=============================================================================
// BBB Validation Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, BBBValidateNullMetricsFails) {
    EXPECT_FALSE(swarm_consciousness_enhanced_bbb_validate(nullptr));
}

TEST_F(SwarmConsciousnessEnhancedTest, BBBValidatePhiMessageValid) {
    uint8_t data[4];
    float phi = 0.5f;
    memcpy(data, &phi, sizeof(float));

    EXPECT_TRUE(swarm_consciousness_validate_phi_message(data, sizeof(data), 42));
}

TEST_F(SwarmConsciousnessEnhancedTest, BBBValidatePhiMessageInvalid) {
    // Null data
    EXPECT_FALSE(swarm_consciousness_validate_phi_message(nullptr, 4, 42));

    // Short data
    uint8_t short_data[2] = {0};
    EXPECT_FALSE(swarm_consciousness_validate_phi_message(short_data, 2, 42));

    // Negative phi
    uint8_t neg_data[4];
    float neg_phi = -1.0f;
    memcpy(neg_data, &neg_phi, sizeof(float));
    EXPECT_FALSE(swarm_consciousness_validate_phi_message(neg_data, 4, 42));

    // NaN phi
    uint8_t nan_data[4];
    float nan_phi = NAN;
    memcpy(nan_data, &nan_phi, sizeof(float));
    EXPECT_FALSE(swarm_consciousness_validate_phi_message(nan_data, 4, 42));
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, PhaseNameReturnsValidStrings) {
    EXPECT_STREQ(consciousness_phase_name(CONSCIOUSNESS_PHASE_CHAOS), "CHAOS");
    EXPECT_STREQ(consciousness_phase_name(CONSCIOUSNESS_PHASE_CRITICAL), "CRITICAL");
    EXPECT_STREQ(consciousness_phase_name(CONSCIOUSNESS_PHASE_ORDERED), "ORDERED");
    EXPECT_STREQ(consciousness_phase_name(CONSCIOUSNESS_PHASE_FROZEN), "FROZEN");
    EXPECT_STREQ(consciousness_phase_name((consciousness_phase_t)99), "UNKNOWN");
}

TEST_F(SwarmConsciousnessEnhancedTest, HierarchyNameReturnsValidStrings) {
    EXPECT_STREQ(consciousness_hierarchy_name(HIERARCHY_INDIVIDUAL), "INDIVIDUAL");
    EXPECT_STREQ(consciousness_hierarchy_name(HIERARCHY_SQUAD), "SQUAD");
    EXPECT_STREQ(consciousness_hierarchy_name(HIERARCHY_PLATOON), "PLATOON");
    EXPECT_STREQ(consciousness_hierarchy_name(HIERARCHY_SWARM), "SWARM");
    EXPECT_STREQ(consciousness_hierarchy_name((consciousness_hierarchy_t)99), "UNKNOWN");
}

TEST_F(SwarmConsciousnessEnhancedTest, MetricsFreeNullIsSafe) {
    swarm_consciousness_enhanced_metrics_free(nullptr);
    // Should not crash
}

//=============================================================================
// Swarm Attachment Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, AttachToNullSwarmFails) {
    EXPECT_FALSE(swarm_consciousness_attach_to_swarm(ctx_, nullptr));
}

TEST_F(SwarmConsciousnessEnhancedTest, AttachWithNullContextFails) {
    EXPECT_FALSE(swarm_consciousness_attach_to_swarm(nullptr, nullptr));
}

TEST_F(SwarmConsciousnessEnhancedTest, DetachFromSwarmWhenNotAttached) {
    // Should not crash when detaching without attaching
    swarm_consciousness_detach_from_swarm(ctx_);
}

TEST_F(SwarmConsciousnessEnhancedTest, DetachNullIsSafe) {
    swarm_consciousness_detach_from_swarm(nullptr);
    // Should not crash
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessEnhancedTest, RegisterBioAsyncWithoutInitialization) {
    // Bio-async not initialized, should fail
    bool result = swarm_consciousness_enhanced_register_bio_async(ctx_);
    EXPECT_FALSE(result);
}

TEST_F(SwarmConsciousnessEnhancedTest, RegisterBioAsyncNullContextFails) {
    EXPECT_FALSE(swarm_consciousness_enhanced_register_bio_async(nullptr));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
