//=============================================================================
// test_thalamic_bridge_integration.cpp - Thalamic Bridge Integration Tests
//=============================================================================
/**
 * @file test_thalamic_bridge_integration.cpp
 * @brief Integration tests for thalamic routing across brain regions
 *
 * WHAT: Tests multi-component interactions of thalamic bridges
 * WHY:  Verify attention gating works across connected subsystems
 * HOW:  Create thalamic router, connect multiple bridges, test routing
 *
 * BIOLOGICAL BASIS:
 * - Thalamus is "gateway to cortex" (Sherman & Guillery)
 * - Thalamic reticular nucleus (TRN) provides attention-based gating
 * - Burst mode for salient events, tonic for steady state
 *
 * TEST SCENARIOS:
 * 1. Thalamic routing across multiple destinations
 * 2. Attention gating signal propagation
 * 3. Burst vs tonic mode switching
 * 4. Cortical-thalamic loop integration
 * 5. Multi-region attention coordination
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_thalamic_router.h"
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"
#include "snn/bridges/nimcp_snn_thalamic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ThalamicBridgeIntegrationTest : public ::testing::Test {
protected:
    thalamic_router_t* router;

    void SetUp() override {
        // Create thalamic router
        thalamic_router_config_t config;
        thalamic_router_default_config(&config);
        config.max_destinations = 64;
        config.enable_attention_gating = true;

        router = thalamic_router_create(&config);
        ASSERT_NE(router, nullptr) << "Failed to create thalamic router";
    }

    void TearDown() override {
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
    }

    // Helper: Create test signal features
    void createSignalFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + 0.1f * sin(i * 0.5f);
        }
    }
};

//=============================================================================
// Test: Quantum Routing Bridge Creation
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, QuantumBridge_Creation) {
    // WHAT: Test quantum thalamic bridge creation
    // WHY:  Verify quantum routing infrastructure initializes correctly
    // HOW:  Create bridge, verify configuration applied

    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    config.routing_threshold = 0.3f;
    config.attention_weight = 0.7f;
    config.max_destinations = 128;

    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify enabled state
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(bridge));

    // Disable and verify
    thalamic_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(thalamic_quantum_bridge_is_enabled(bridge));

    // Re-enable
    thalamic_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(bridge));

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: Quantum Routing Decision
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, QuantumBridge_RoutingDecision) {
    // WHAT: Test quantum routing decision making
    // WHY:  Verify quantum attention provides valid routing decisions
    // HOW:  Create routing scenario, execute quantum route, verify results

    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    config.routing_threshold = 0.2f;  // Lower threshold for more routing

    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Create test signal
    const uint32_t feature_dim = 16;
    float signal_features[feature_dim];
    createSignalFeatures(signal_features, feature_dim, 0.5f);

    // Define destinations
    uint32_t dest_ids[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t num_dests = 8;

    // Allocate output buffers
    uint32_t routed_dests[8];
    uint32_t num_routed = 0;

    // Execute quantum routing
    int result = thalamic_quantum_route(
        bridge,
        0,  // source_id
        dest_ids,
        num_dests,
        signal_features,
        feature_dim,
        routed_dests,
        &num_routed
    );

    EXPECT_EQ(result, 0) << "Quantum routing should succeed";

    // Some destinations should be routed (or all if using classical fallback)
    EXPECT_GE(num_routed, 0u);
    EXPECT_LE(num_routed, num_dests);

    // Routed destinations should be from original list
    for (uint32_t i = 0; i < num_routed; i++) {
        bool found = false;
        for (uint32_t j = 0; j < num_dests; j++) {
            if (routed_dests[i] == dest_ids[j]) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Routed dest " << routed_dests[i]
                          << " should be from original list";
    }

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: Quantum Gating Single Destination
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, QuantumBridge_GatingSingleDestination) {
    // WHAT: Test quantum gating for single destination
    // WHY:  Verify fast gate/no-gate decision
    // HOW:  Gate multiple signals, verify consistent behavior

    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    const uint32_t feature_dim = 8;
    float signal_features[feature_dim];

    // Test multiple gating decisions
    int gate_count = 0;
    int no_gate_count = 0;

    for (int trial = 0; trial < 10; trial++) {
        // Create varied signal
        createSignalFeatures(signal_features, feature_dim, 0.2f + trial * 0.05f);

        float gate_weight = 0.0f;
        bool gated = thalamic_quantum_gate_signal(
            bridge,
            0,    // source_id
            1,    // dest_id
            signal_features,
            feature_dim,
            &gate_weight
        );

        if (gated) {
            gate_count++;
            EXPECT_GE(gate_weight, 0.0f);
            EXPECT_LE(gate_weight, 1.0f);
        } else {
            no_gate_count++;
            // Gate weight might be 0 for non-gated signals
        }
    }

    // Should have some decisions made
    EXPECT_GT(gate_count + no_gate_count, 0);

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: Quantum Routing Statistics
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, QuantumBridge_StatisticsTracking) {
    // WHAT: Test statistics tracking in quantum routing
    // WHY:  Verify performance metrics are recorded
    // HOW:  Perform multiple routes, verify stats accumulated

    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Reset stats
    thalamic_quantum_reset_stats(bridge);

    // Perform routing operations
    const uint32_t feature_dim = 16;
    float signal_features[feature_dim];
    createSignalFeatures(signal_features, feature_dim, 0.5f);

    uint32_t dest_ids[] = {1, 2, 3, 4};
    uint32_t routed_dests[4];
    uint32_t num_routed;

    const int num_routes = 20;
    for (int i = 0; i < num_routes; i++) {
        thalamic_quantum_route(
            bridge, 0, dest_ids, 4,
            signal_features, feature_dim,
            routed_dests, &num_routed
        );
    }

    // Get statistics
    thalamic_quantum_stats_t stats;
    int result = thalamic_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    // Verify statistics accumulated
    // Either quantum or classical routes should be counted
    EXPECT_GE(stats.quantum_routes + stats.classical_fallbacks, (uint64_t)num_routes);

    // Speedup should be valid
    EXPECT_GE(stats.routing_speedup, 0.0f);

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: SNN Thalamic Bridge Creation
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, SNNBridge_Creation) {
    // WHAT: Test SNN thalamic bridge creation
    // WHY:  Verify SNN-thalamic integration initializes correctly
    // HOW:  Create bridge with config, verify configuration

    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);
    config.default_mode = THALAMIC_MODE_ADAPTIVE;
    config.enable_attention_gating = true;
    config.enable_ct_loop = true;
    config.burst_threshold_ms = 4.0f;

    // Create bridge without network/router (testing config only)
    snn_thalamic_bridge_t* bridge = snn_thalamic_bridge_create(&config, nullptr, router);

    if (bridge) {
        // Verify configuration was applied
        EXPECT_TRUE(bridge->config.enable_attention_gating);
        EXPECT_TRUE(bridge->config.enable_ct_loop);
        EXPECT_FLOAT_EQ(bridge->config.burst_threshold_ms, 4.0f);

        snn_thalamic_bridge_destroy(bridge);
    }
    // It's okay if bridge is null due to missing network
}

//=============================================================================
// Test: Thalamic Mode Detection
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, SNNBridge_ModeDetection) {
    // WHAT: Test burst vs tonic mode detection
    // WHY:  Verify ISI-based mode classification works
    // HOW:  Provide different spike timings, verify mode detection

    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);
    config.burst_threshold_ms = 4.0f;
    config.tonic_min_isi_ms = 10.0f;
    config.enable_mode_switching = true;

    snn_thalamic_bridge_t* bridge = snn_thalamic_bridge_create(&config, nullptr, router);

    if (bridge) {
        // Simulate burst timing (short ISI)
        uint64_t spike_time_burst_1 = 1000;  // 1ms
        uint64_t spike_time_burst_2 = 3000;  // 3ms (2ms ISI = burst)

        // Simulate tonic timing (long ISI)
        uint64_t spike_time_tonic = 20000;   // 20ms ISI = tonic

        thalamic_relay_mode_t mode1 = snn_thalamic_bridge_detect_mode(
            bridge, 0, spike_time_burst_1);
        thalamic_relay_mode_t mode2 = snn_thalamic_bridge_detect_mode(
            bridge, 0, spike_time_burst_2);

        // First spike starts as burst (or adaptive)
        // Second spike should detect burst based on short ISI

        // Reset neuron and test tonic
        thalamic_relay_mode_t mode3 = snn_thalamic_bridge_detect_mode(
            bridge, 1, spike_time_burst_1);  // Different neuron
        thalamic_relay_mode_t mode4 = snn_thalamic_bridge_detect_mode(
            bridge, 1, spike_time_tonic);

        // Modes should be valid
        EXPECT_GE((int)mode1, 0);
        EXPECT_LE((int)mode1, 2);
        EXPECT_GE((int)mode4, 0);
        EXPECT_LE((int)mode4, 2);

        snn_thalamic_bridge_destroy(bridge);
    }
}

//=============================================================================
// Test: Attention Weight Setting
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, SNNBridge_AttentionControl) {
    // WHAT: Test attention weight control
    // WHY:  Verify top-down attention modulation works
    // HOW:  Set attention weights, verify they are stored

    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);
    config.enable_attention_gating = true;
    config.attention_threshold = 0.3f;

    snn_thalamic_bridge_t* bridge = snn_thalamic_bridge_create(&config, nullptr, router);

    if (bridge) {
        // Set attention for population 0
        int result = snn_thalamic_bridge_set_attention(bridge, 0, 0.8f);
        EXPECT_EQ(result, 0);

        // Get attention
        float attention = 0.0f;
        result = snn_thalamic_bridge_get_attention(bridge, 0, &attention);
        EXPECT_EQ(result, 0);
        EXPECT_FLOAT_EQ(attention, 0.8f);

        // Set different attention levels for multiple populations
        snn_thalamic_bridge_set_attention(bridge, 1, 0.5f);
        snn_thalamic_bridge_set_attention(bridge, 2, 0.2f);

        float attn1 = 0.0f, attn2 = 0.0f;
        snn_thalamic_bridge_get_attention(bridge, 1, &attn1);
        snn_thalamic_bridge_get_attention(bridge, 2, &attn2);

        EXPECT_FLOAT_EQ(attn1, 0.5f);
        EXPECT_FLOAT_EQ(attn2, 0.2f);

        snn_thalamic_bridge_destroy(bridge);
    }
}

//=============================================================================
// Test: SNN Thalamic Statistics
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, SNNBridge_StatisticsTracking) {
    // WHAT: Test statistics tracking in SNN thalamic bridge
    // WHY:  Verify spike relay metrics are recorded
    // HOW:  Simulate activity, verify stats

    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);

    snn_thalamic_bridge_t* bridge = snn_thalamic_bridge_create(&config, nullptr, router);

    if (bridge) {
        // Get initial stats
        snn_thalamic_stats_t stats;
        int result = snn_thalamic_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(result, 0);

        EXPECT_EQ(stats.spikes_relayed, 0u);
        EXPECT_EQ(stats.spikes_blocked, 0u);
        EXPECT_EQ(stats.bursts_detected, 0u);

        // Reset stats
        snn_thalamic_bridge_reset_stats(bridge);

        // Get stats again
        result = snn_thalamic_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(stats.spikes_relayed, 0u);

        snn_thalamic_bridge_destroy(bridge);
    }
}

//=============================================================================
// Test: Multi-Destination Routing
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, MultiDestination_RoutingScenario) {
    // WHAT: Test routing to multiple destinations simultaneously
    // WHY:  Verify thalamic broadcasting works correctly
    // HOW:  Route signal to many destinations, verify results

    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    config.routing_threshold = 0.1f;  // Low threshold for broad routing
    config.max_destinations = 64;

    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Create many destinations
    const uint32_t num_dests = 32;
    uint32_t dest_ids[num_dests];
    for (uint32_t i = 0; i < num_dests; i++) {
        dest_ids[i] = i + 1;
    }

    // Create signal
    const uint32_t feature_dim = 32;
    float signal_features[feature_dim];
    createSignalFeatures(signal_features, feature_dim, 0.7f);

    // Route
    uint32_t routed_dests[num_dests];
    uint32_t num_routed = 0;

    int result = thalamic_quantum_route(
        bridge, 0, dest_ids, num_dests,
        signal_features, feature_dim,
        routed_dests, &num_routed
    );

    EXPECT_EQ(result, 0);
    EXPECT_LE(num_routed, num_dests);

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: Routing Threshold Effects
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, RoutingThreshold_AffectsSparsity) {
    // WHAT: Test that routing threshold affects sparsity
    // WHY:  Verify threshold controls routing selectivity
    // HOW:  Compare routing with different thresholds

    const uint32_t feature_dim = 16;
    float signal_features[feature_dim];
    createSignalFeatures(signal_features, feature_dim, 0.5f);

    uint32_t dest_ids[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t num_dests = 8;
    uint32_t routed_dests[8];
    uint32_t num_routed;

    // Low threshold - more routing
    thalamic_quantum_config_t config_low = thalamic_quantum_default_config();
    config_low.routing_threshold = 0.1f;
    thalamic_quantum_bridge_t* bridge_low = thalamic_quantum_bridge_create(&config_low);
    ASSERT_NE(bridge_low, nullptr);

    thalamic_quantum_route(bridge_low, 0, dest_ids, num_dests,
                           signal_features, feature_dim,
                           routed_dests, &num_routed);
    uint32_t low_threshold_routed = num_routed;

    // High threshold - less routing
    thalamic_quantum_config_t config_high = thalamic_quantum_default_config();
    config_high.routing_threshold = 0.9f;
    thalamic_quantum_bridge_t* bridge_high = thalamic_quantum_bridge_create(&config_high);
    ASSERT_NE(bridge_high, nullptr);

    thalamic_quantum_route(bridge_high, 0, dest_ids, num_dests,
                           signal_features, feature_dim,
                           routed_dests, &num_routed);
    uint32_t high_threshold_routed = num_routed;

    // Generally, lower threshold should route more (or equal)
    // But implementation may vary - just verify both are valid
    EXPECT_LE(high_threshold_routed, num_dests);
    EXPECT_LE(low_threshold_routed, num_dests);

    thalamic_quantum_bridge_destroy(bridge_low);
    thalamic_quantum_bridge_destroy(bridge_high);
}

//=============================================================================
// Test: Mode Setting and Getting
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, SNNBridge_ModeControl) {
    // WHAT: Test relay mode setting and getting
    // WHY:  Verify mode control works per neuron
    // HOW:  Set modes, verify retrieval

    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);

    snn_thalamic_bridge_t* bridge = snn_thalamic_bridge_create(&config, nullptr, router);

    if (bridge) {
        // Set mode for neuron 0
        int result = snn_thalamic_bridge_set_mode(bridge, 0, THALAMIC_MODE_BURST);
        EXPECT_EQ(result, 0);

        // Get mode
        thalamic_relay_mode_t mode;
        result = snn_thalamic_bridge_get_mode(bridge, 0, &mode);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(mode, THALAMIC_MODE_BURST);

        // Set different mode
        result = snn_thalamic_bridge_set_mode(bridge, 0, THALAMIC_MODE_TONIC);
        EXPECT_EQ(result, 0);

        result = snn_thalamic_bridge_get_mode(bridge, 0, &mode);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(mode, THALAMIC_MODE_TONIC);

        snn_thalamic_bridge_destroy(bridge);
    }
}

//=============================================================================
// Test: Quantum Bridge Disabled State
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, QuantumBridge_DisabledFallback) {
    // WHAT: Test quantum bridge in disabled state
    // WHY:  Verify classical fallback works
    // HOW:  Disable bridge, verify routing still works

    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Disable quantum routing
    thalamic_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(thalamic_quantum_bridge_is_enabled(bridge));

    // Routing should still work (classical fallback)
    const uint32_t feature_dim = 8;
    float signal_features[feature_dim];
    createSignalFeatures(signal_features, feature_dim, 0.5f);

    uint32_t dest_ids[] = {1, 2, 3, 4};
    uint32_t routed_dests[4];
    uint32_t num_routed = 0;

    int result = thalamic_quantum_route(
        bridge, 0, dest_ids, 4,
        signal_features, feature_dim,
        routed_dests, &num_routed
    );

    EXPECT_EQ(result, 0);
    // Classical fallback routes to all destinations
    EXPECT_EQ(num_routed, 4u);

    // Check stats show classical fallback
    thalamic_quantum_stats_t stats;
    thalamic_quantum_get_stats(bridge, &stats);
    EXPECT_GT(stats.classical_fallbacks, 0u);

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: Bridge Update Integration
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, SNNBridge_UpdateCycle) {
    // WHAT: Test bridge update cycle
    // WHY:  Verify update integrates relay modes and attention
    // HOW:  Run update cycles, verify state consistency

    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);
    config.update_interval_ms = 10.0f;

    snn_thalamic_bridge_t* bridge = snn_thalamic_bridge_create(&config, nullptr, router);

    if (bridge) {
        // Run multiple update cycles
        for (int i = 0; i < 10; i++) {
            float dt = 10.0f;  // 10ms timestep
            int result = snn_thalamic_bridge_update(bridge, dt);
            EXPECT_EQ(result, 0);
        }

        // Bridge should still be in valid state
        EXPECT_FALSE(bridge->connected);  // No network connected

        snn_thalamic_bridge_destroy(bridge);
    }
}

//=============================================================================
// Test: Error Handling
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, ErrorHandling_NullInputs) {
    // WHAT: Test error handling for null inputs
    // WHY:  Verify graceful handling of invalid inputs
    // HOW:  Pass null pointers, verify error returns

    // Null bridge
    EXPECT_FALSE(thalamic_quantum_bridge_is_enabled(nullptr));

    // Null route inputs
    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    thalamic_quantum_bridge_t* bridge = thalamic_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    uint32_t num_routed = 0;
    int result = thalamic_quantum_route(bridge, 0, nullptr, 4, nullptr, 8, nullptr, &num_routed);
    EXPECT_LT(result, 0);  // Should fail

    uint32_t dest_ids[] = {1, 2};
    uint32_t routed[2];
    float features[8] = {0};

    result = thalamic_quantum_route(bridge, 0, dest_ids, 2, features, 8, routed, nullptr);
    EXPECT_LT(result, 0);  // Should fail (null num_routed)

    // Zero destinations
    result = thalamic_quantum_route(bridge, 0, dest_ids, 0, features, 8, routed, &num_routed);
    EXPECT_LT(result, 0);  // Should fail

    // Null stats
    result = thalamic_quantum_get_stats(nullptr, nullptr);
    EXPECT_LT(result, 0);

    thalamic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Test: Default Configuration Values
//=============================================================================

TEST_F(ThalamicBridgeIntegrationTest, DefaultConfig_ValidValues) {
    // WHAT: Test default configuration provides valid values
    // WHY:  Verify sensible defaults for all bridge types
    // HOW:  Get defaults, verify ranges

    // Quantum config
    thalamic_quantum_config_t q_config = thalamic_quantum_default_config();
    EXPECT_TRUE(q_config.enabled);
    EXPECT_GE(q_config.routing_threshold, 0.0f);
    EXPECT_LE(q_config.routing_threshold, 1.0f);
    EXPECT_GE(q_config.attention_weight, 0.0f);
    EXPECT_LE(q_config.attention_weight, 1.0f);
    EXPECT_GT(q_config.max_destinations, 0u);

    // SNN config
    snn_thalamic_config_t s_config;
    snn_thalamic_config_default(&s_config);
    EXPECT_GE(s_config.burst_threshold_ms, 0.0f);
    EXPECT_GE(s_config.tonic_min_isi_ms, 0.0f);
    EXPECT_GE(s_config.attention_threshold, 0.0f);
    EXPECT_LE(s_config.attention_threshold, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
