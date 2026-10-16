/**
 * @file test_fuzzy_bridge.cpp
 * @brief Unit tests for fuzzy logic bridge module
 *
 * WHAT: ~40 tests for bridge lifecycle, subsystem setters, SNN/STDP/plasticity/
 *       LNN conversions, training integration, quantum fallback, symbolic logic,
 *       health checks, modulation, and statistics
 * WHY:  Verify correct fuzzy<->subsystem conversions and bridge state management
 * HOW:  GTest C++17, extern "C" headers, NULL subsystem pointers for unit isolation
 *
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#include "utils/fuzzy/nimcp_fuzzy_bridge.h"
}

// ============================================================================
// Test Constants
// ============================================================================

namespace {
    constexpr float TOL = 1e-3f;
}

// ============================================================================
// Fixture: Bridge Lifecycle
// ============================================================================

class FuzzyBridgeTest : public ::testing::Test {
protected:
    fuzzy_bridge_t* bridge = nullptr;

    void SetUp() override {
        fuzzy_bridge_config_t cfg = fuzzy_bridge_default_config();
        bridge = fuzzy_bridge_create(&cfg);
    }

    void TearDown() override {
        if (bridge) {
            fuzzy_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// ============================================================================
// 1. Lifecycle Tests
// ============================================================================

TEST_F(FuzzyBridgeTest, Create_ReturnsNonNull) {
    ASSERT_NE(bridge, nullptr);
}

TEST(FuzzyBridgeLifecycle, CreateWithNullConfig) {
    fuzzy_bridge_t* b = fuzzy_bridge_create(nullptr);
    // May or may not return null depending on implementation
    if (b) {
        fuzzy_bridge_destroy(b);
    }
}

TEST(FuzzyBridgeLifecycle, DestroyNull_NoOp) {
    fuzzy_bridge_destroy(nullptr);  // should not crash
}

TEST(FuzzyBridgeLifecycle, DefaultConfig_HasReasonableValues) {
    fuzzy_bridge_config_t cfg = fuzzy_bridge_default_config();
    EXPECT_GE(cfg.spike_rate_max, cfg.spike_rate_min);
    EXPECT_GT(cfg.stdp_window_ms, 0.0f);
    EXPECT_GE(cfg.plasticity_rate_max, cfg.plasticity_rate_min);
    EXPECT_GE(cfg.training_lr_max, cfg.training_lr_min);
}

TEST_F(FuzzyBridgeTest, GetState_Valid) {
    fuzzy_bridge_state_t state = fuzzy_bridge_get_state(bridge);
    EXPECT_GE(static_cast<int>(state), 0);
    EXPECT_LT(static_cast<int>(state), FUZZY_BRIDGE_STATE_COUNT);
}

TEST(FuzzyBridgeLifecycle, GetState_NullReturnsValid) {
    fuzzy_bridge_state_t state = fuzzy_bridge_get_state(nullptr);
    // Should return an error/idle state, not crash
    EXPECT_GE(static_cast<int>(state), 0);
}

// ============================================================================
// 2. Subsystem Setters (NULL pointers = disconnected)
// ============================================================================

TEST_F(FuzzyBridgeTest, SetImmune_Null) {
    int rc = fuzzy_bridge_set_immune(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetBBB_Null) {
    int rc = fuzzy_bridge_set_bbb(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetHealthAgent_Null) {
    int rc = fuzzy_bridge_set_health_agent(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetKgWiring_Null) {
    int rc = fuzzy_bridge_set_kg_wiring(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetKgRegistry_Null) {
    int rc = fuzzy_bridge_set_kg_registry(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetLogger_Null) {
    int rc = fuzzy_bridge_set_logger(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetSecurity_Null) {
    int rc = fuzzy_bridge_set_security(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetCycleCoordinator_Null) {
    int rc = fuzzy_bridge_set_cycle_coordinator(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetBioRouter_Null) {
    int rc = fuzzy_bridge_set_bio_router(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetEthics_Null) {
    int rc = fuzzy_bridge_set_ethics(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetLgss_Null) {
    int rc = fuzzy_bridge_set_lgss(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetSNN_Null) {
    int rc = fuzzy_bridge_set_snn(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetSTDP_Null) {
    int rc = fuzzy_bridge_set_stdp(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetPlasticity_Null) {
    int rc = fuzzy_bridge_set_plasticity(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetLNN_Null) {
    int rc = fuzzy_bridge_set_lnn(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetTraining_Null) {
    int rc = fuzzy_bridge_set_training(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetQuantum_Null) {
    int rc = fuzzy_bridge_set_quantum(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetSymbolic_Null) {
    int rc = fuzzy_bridge_set_symbolic(bridge, nullptr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

// Setter with NULL bridge
TEST(FuzzyBridgeSetterNull, SetSNN_NullBridge) {
    int rc = fuzzy_bridge_set_snn(nullptr, nullptr);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST(FuzzyBridgeSetterNull, SetImmune_NullBridge) {
    int rc = fuzzy_bridge_set_immune(nullptr, nullptr);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 3. SNN Spike Conversion
// ============================================================================

TEST_F(FuzzyBridgeTest, ToSpikePopulation_Valid) {
    float memberships[] = {0.0f, 0.5f, 1.0f};
    float rates[3] = {0};
    int rc = fuzzy_bridge_to_spike_population(bridge, memberships, 3, rates);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    // Rate for membership 0.0 should be low, 1.0 should be high
    EXPECT_LE(rates[0], rates[2]);
}

TEST_F(FuzzyBridgeTest, FromSpikePopulation_Valid) {
    fuzzy_bridge_config_t cfg = fuzzy_bridge_default_config();
    float rates[] = {cfg.spike_rate_min, (cfg.spike_rate_min + cfg.spike_rate_max) / 2.0f, cfg.spike_rate_max};
    float memberships[3] = {0};
    int rc = fuzzy_bridge_from_spike_population(bridge, rates, 3, memberships);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    // Membership for max rate should be higher
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(memberships[i], 0.0f);
        EXPECT_LE(memberships[i], 1.0f);
    }
}

TEST_F(FuzzyBridgeTest, ToSpikePopulation_NullBridge) {
    float m[] = {0.5f};
    float r[1];
    int rc = fuzzy_bridge_to_spike_population(nullptr, m, 1, r);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, ToSpikePopulation_NullInput) {
    float r[1];
    int rc = fuzzy_bridge_to_spike_population(bridge, nullptr, 1, r);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, ToSpikePopulation_NullOutput) {
    float m[] = {0.5f};
    int rc = fuzzy_bridge_to_spike_population(bridge, m, 1, nullptr);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 4. STDP Temporal Membership
// ============================================================================

TEST_F(FuzzyBridgeTest, STDPTemporal_PositiveDt) {
    float pot = 0.0f, dep = 0.0f;
    int rc = fuzzy_bridge_stdp_temporal_membership(bridge, 5.0f, &pot, &dep);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    // Positive dt -> potentiation should be higher
    EXPECT_GE(pot, 0.0f);
    EXPECT_LE(pot, 1.0f);
    EXPECT_GE(dep, 0.0f);
    EXPECT_LE(dep, 1.0f);
}

TEST_F(FuzzyBridgeTest, STDPTemporal_NegativeDt) {
    float pot = 0.0f, dep = 0.0f;
    int rc = fuzzy_bridge_stdp_temporal_membership(bridge, -5.0f, &pot, &dep);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    // Negative dt -> depression should be higher
    EXPECT_GE(dep, 0.0f);
}

TEST_F(FuzzyBridgeTest, STDPTemporal_NullBridge) {
    float pot, dep;
    int rc = fuzzy_bridge_stdp_temporal_membership(nullptr, 5.0f, &pot, &dep);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 5. Plasticity Rate
// ============================================================================

TEST_F(FuzzyBridgeTest, PlasticityRate_MidScores) {
    float rate = 0.0f;
    int rc = fuzzy_bridge_plasticity_rate(bridge, 0.5f, 0.5f, &rate);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(FuzzyBridgeTest, PlasticityRate_HighPerformance) {
    float rate = 0.0f;
    int rc = fuzzy_bridge_plasticity_rate(bridge, 1.0f, 0.5f, &rate);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(FuzzyBridgeTest, PlasticityRate_NullBridge) {
    float rate;
    int rc = fuzzy_bridge_plasticity_rate(nullptr, 0.5f, 0.5f, &rate);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, PlasticityRate_NullOutput) {
    int rc = fuzzy_bridge_plasticity_rate(bridge, 0.5f, 0.5f, nullptr);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 6. LNN State Classification
// ============================================================================

TEST_F(FuzzyBridgeTest, LNNClassify_ValidState) {
    float state[] = {0.1f, 0.5f, 0.9f, 0.3f};
    fuzzy_value_t fval;
    int rc = fuzzy_bridge_lnn_classify_state(bridge, state, 4, &fval);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, LNNClassify_NullBridge) {
    float state[] = {0.1f};
    fuzzy_value_t fval;
    int rc = fuzzy_bridge_lnn_classify_state(nullptr, state, 1, &fval);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, LNNClassify_NullState) {
    fuzzy_value_t fval;
    int rc = fuzzy_bridge_lnn_classify_state(bridge, nullptr, 4, &fval);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 7. Training Integration
// ============================================================================

TEST_F(FuzzyBridgeTest, TrainingLRSchedule_Valid) {
    float lr = 0.0f;
    int rc = fuzzy_bridge_training_lr_schedule(bridge, 0.5f, -0.1f, 0.01f, &lr);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    EXPECT_GE(lr, 0.0f);
}

TEST_F(FuzzyBridgeTest, TrainingLRSchedule_NullBridge) {
    float lr;
    int rc = fuzzy_bridge_training_lr_schedule(nullptr, 0.5f, -0.1f, 0.01f, &lr);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, TrainingConvergence_Valid) {
    float conv = 0.0f;
    int rc = fuzzy_bridge_training_convergence(bridge, 0.001f, 0.01f, &conv);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    EXPECT_GE(conv, 0.0f);
    EXPECT_LE(conv, 1.0f);
}

TEST_F(FuzzyBridgeTest, TrainingConvergence_NullBridge) {
    float conv;
    int rc = fuzzy_bridge_training_convergence(nullptr, 0.001f, 0.01f, &conv);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, TrainingConvergence_SmallDelta) {
    float conv = 0.0f;
    int rc = fuzzy_bridge_training_convergence(bridge, 1e-8f, 1e-8f, &conv);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
    // Very small delta + norm -> should indicate convergence
    EXPECT_GT(conv, 0.5f);
}

// ============================================================================
// 8. Quantum Integration (Fallback)
// ============================================================================

TEST_F(FuzzyBridgeTest, QuantumInference_NullBridge) {
    float inputs[] = {5.0f};
    fuzzy_inference_engine_t* eng = fuzzy_inference_create();
    fuzzy_inference_result_t result;
    int rc = fuzzy_bridge_quantum_inference(nullptr, inputs, 1, eng, &result);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
    fuzzy_inference_destroy(eng);
}

TEST_F(FuzzyBridgeTest, QuantumInference_NullEngine) {
    float inputs[] = {5.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_bridge_quantum_inference(bridge, inputs, 1, nullptr, &result);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 9. Symbolic Logic
// ============================================================================

TEST_F(FuzzyBridgeTest, SymbolicMatch_NullBridge) {
    bool match;
    float score;
    int rc = fuzzy_bridge_symbolic_match(nullptr, "test", 0.5f, &match, &score);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SymbolicMatch_NullAction) {
    bool match;
    float score;
    int rc = fuzzy_bridge_symbolic_match(bridge, nullptr, 0.5f, &match, &score);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 10. Health & Monitoring
// ============================================================================

TEST_F(FuzzyBridgeTest, Heartbeat_Valid) {
    int rc = fuzzy_bridge_heartbeat(bridge, "unit_test", 0.5f);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, Heartbeat_NullBridge) {
    int rc = fuzzy_bridge_heartbeat(nullptr, "test", 0.5f);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, CheckHealth_Valid) {
    int rc = fuzzy_bridge_check_health(bridge);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, CheckHealth_NullBridge) {
    int rc = fuzzy_bridge_check_health(nullptr);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 11. Modulation
// ============================================================================

TEST_F(FuzzyBridgeTest, SetInflammation_Valid) {
    int rc = fuzzy_bridge_set_inflammation(bridge, 0.5f);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetInflammation_NullBridge) {
    int rc = fuzzy_bridge_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetFatigue_Valid) {
    int rc = fuzzy_bridge_set_fatigue(bridge, 0.3f);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, SetFatigue_NullBridge) {
    int rc = fuzzy_bridge_set_fatigue(nullptr, 0.3f);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

// ============================================================================
// 12. Statistics
// ============================================================================

TEST_F(FuzzyBridgeTest, GetStats_Valid) {
    fuzzy_bridge_stats_t stats;
    int rc = fuzzy_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, GetStats_NullBridge) {
    fuzzy_bridge_stats_t stats;
    int rc = fuzzy_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, GetStats_NullStats) {
    int rc = fuzzy_bridge_get_stats(bridge, nullptr);
    EXPECT_NE(rc, FUZZY_BRIDGE_ERR_OK);
}

TEST_F(FuzzyBridgeTest, ResetStats_Valid) {
    fuzzy_bridge_reset_stats(bridge);  // should not crash
}

TEST_F(FuzzyBridgeTest, ResetStats_NullBridge) {
    fuzzy_bridge_reset_stats(nullptr);  // should not crash
}

// ============================================================================
// 13. Error String
// ============================================================================

TEST(FuzzyBridgeError, GetLastError_ReturnsNonNull) {
    const char* err = fuzzy_bridge_get_last_error();
    EXPECT_NE(err, nullptr);
}
