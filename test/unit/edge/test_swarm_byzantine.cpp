/**
 * @file test_swarm_byzantine.cpp
 * @brief Unit tests for Byzantine fault detection — gradient and telemetry anomaly checks.
 *
 * WHAT: Test gradient NaN/Inf detection, norm outlier detection, anomaly
 *       accumulation, quarantine, telemetry validation, and anomaly scoring.
 * WHY:  Byzantine detection prevents poisoned gradients from corrupting the
 *       aggregated model in federated swarm learning.
 * HOW:  Google Test, using types from nimcp_swarm_runtime_types.h with
 *       forward-declared internal functions from nimcp_swarm_byzantine.c.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime_types.h"

/* Forward-declare internal byzantine functions.
 * These are defined in nimcp_swarm_byzantine.c and use the types from
 * nimcp_swarm_runtime_types.h. */
int   nimcp_byzantine_check_gradient(nimcp_peer_entry_t* peer,
                                      const float* gradients,
                                      uint32_t num_params);
int   nimcp_byzantine_check_telemetry(const nimcp_peer_entry_t* peer,
                                       const nimcp_device_telemetry_t* telemetry);
void  nimcp_byzantine_reset_peer(nimcp_peer_entry_t* peer);
float nimcp_byzantine_get_anomaly_score(const nimcp_peer_entry_t* peer);
}

// Helper: create a zeroed peer entry for testing
static nimcp_peer_entry_t make_test_peer(uint32_t id) {
    nimcp_peer_entry_t peer;
    memset(&peer, 0, sizeof(peer));
    peer.device_id = id;
    peer.state = NIMCP_PEER_ACTIVE;
    peer.gradient_norm_ema = 0.0f;
    peer.anomaly_count = 0;
    peer.total_syncs = 0;
    peer.quarantined = false;
    return peer;
}

// ============================================================================
// Normal Gradient
// ============================================================================

TEST(SwarmByzantine, NormalGradientPasses) {
    nimcp_peer_entry_t peer = make_test_peer(1);

    std::vector<float> grads(100, 0.01f);
    int rc = nimcp_byzantine_check_gradient(&peer, grads.data(), 100);
    EXPECT_EQ(rc, 0) << "Normal gradient should pass";
    EXPECT_EQ(peer.anomaly_count, 0u);
    EXPECT_FALSE(peer.quarantined);
}

// ============================================================================
// NaN / Inf Detection
// ============================================================================

TEST(SwarmByzantine, NaNGradientFlagged) {
    nimcp_peer_entry_t peer = make_test_peer(2);

    std::vector<float> grads(100, 0.01f);
    grads[50] = NAN;
    int rc = nimcp_byzantine_check_gradient(&peer, grads.data(), 100);
    EXPECT_EQ(rc, 1) << "NaN gradient should be flagged";
    EXPECT_GT(peer.anomaly_count, 0u);
}

TEST(SwarmByzantine, InfGradientFlagged) {
    nimcp_peer_entry_t peer = make_test_peer(3);

    std::vector<float> grads(100, 0.01f);
    grads[0] = INFINITY;
    int rc = nimcp_byzantine_check_gradient(&peer, grads.data(), 100);
    EXPECT_EQ(rc, 1) << "Inf gradient should be flagged";
    EXPECT_GT(peer.anomaly_count, 0u);
}

// ============================================================================
// Extreme Norm Triggers Anomaly
// ============================================================================

TEST(SwarmByzantine, ExtremeNormTriggers) {
    nimcp_peer_entry_t peer = make_test_peer(4);

    // First submission: establishes EMA
    std::vector<float> normal_grads(100, 0.01f);
    nimcp_byzantine_check_gradient(&peer, normal_grads.data(), 100);
    peer.total_syncs = 1;

    // Second submission: extreme norm (100x the first)
    std::vector<float> extreme_grads(100, 10.0f);
    int rc = nimcp_byzantine_check_gradient(&peer, extreme_grads.data(), 100);
    EXPECT_EQ(rc, 1) << "Extreme norm deviation should be flagged";
    EXPECT_GT(peer.anomaly_count, 0u);
}

// ============================================================================
// Repeated Anomalies Lead to Quarantine
// ============================================================================

TEST(SwarmByzantine, RepeatedAnomaliesQuarantine) {
    nimcp_peer_entry_t peer = make_test_peer(5);

    // Submit NaN gradients repeatedly to exceed anomaly threshold (5)
    std::vector<float> bad_grads(10, NAN);
    for (int i = 0; i < 10; i++) {
        nimcp_byzantine_check_gradient(&peer, bad_grads.data(), 10);
    }

    EXPECT_TRUE(peer.quarantined) << "Should be quarantined after repeated anomalies";
    EXPECT_EQ(peer.state, NIMCP_PEER_BYZANTINE);
}

// ============================================================================
// All-Zero Gradient
// ============================================================================

TEST(SwarmByzantine, AllZeroGradientNoCrash) {
    nimcp_peer_entry_t peer = make_test_peer(6);

    std::vector<float> zero_grads(100, 0.0f);
    int rc = nimcp_byzantine_check_gradient(&peer, zero_grads.data(), 100);
    // Zero gradient is valid (not NaN/Inf), but norm is zero
    // First submission — no anomaly since it initializes EMA
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(peer.quarantined);
}

// ============================================================================
// Reset
// ============================================================================

TEST(SwarmByzantine, ResetClearsState) {
    nimcp_peer_entry_t peer = make_test_peer(7);
    peer.anomaly_count = 3;
    peer.gradient_norm_ema = 5.0f;
    peer.quarantined = true;
    peer.state = NIMCP_PEER_BYZANTINE;

    nimcp_byzantine_reset_peer(&peer);

    EXPECT_EQ(peer.anomaly_count, 0u);
    EXPECT_FLOAT_EQ(peer.gradient_norm_ema, 0.0f);
    EXPECT_FALSE(peer.quarantined);
    EXPECT_EQ(peer.state, NIMCP_PEER_ACTIVE);
}

TEST(SwarmByzantine, ResetNull) {
    nimcp_byzantine_reset_peer(NULL);
    SUCCEED() << "nimcp_byzantine_reset_peer(NULL) did not crash";
}

// ============================================================================
// Telemetry Validation
// ============================================================================

TEST(SwarmByzantine, TelemetryNegativeBattery) {
    nimcp_peer_entry_t peer = make_test_peer(8);
    nimcp_device_telemetry_t tel;
    memset(&tel, 0, sizeof(tel));
    tel.battery_pct = -10.0f;

    int rc = nimcp_byzantine_check_telemetry(&peer, &tel);
    EXPECT_EQ(rc, 1) << "Negative battery should be flagged";
}

TEST(SwarmByzantine, TelemetryExtremeTemperature) {
    nimcp_peer_entry_t peer = make_test_peer(9);
    nimcp_device_telemetry_t tel;
    memset(&tel, 0, sizeof(tel));
    tel.temperature_c = 200.0f; // Above 150 max

    int rc = nimcp_byzantine_check_telemetry(&peer, &tel);
    EXPECT_EQ(rc, 1) << "Extreme temperature should be flagged";
}

TEST(SwarmByzantine, TelemetryNullArgs) {
    nimcp_peer_entry_t peer = make_test_peer(10);
    nimcp_device_telemetry_t tel;
    memset(&tel, 0, sizeof(tel));

    EXPECT_LT(nimcp_byzantine_check_telemetry(NULL, &tel), 0);
    EXPECT_LT(nimcp_byzantine_check_telemetry(&peer, NULL), 0);
    EXPECT_LT(nimcp_byzantine_check_telemetry(NULL, NULL), 0);
}

// ============================================================================
// Anomaly Score
// ============================================================================

TEST(SwarmByzantine, AnomalyScore) {
    nimcp_peer_entry_t peer = make_test_peer(11);
    peer.anomaly_count = 3;
    peer.total_syncs = 10;

    float score = nimcp_byzantine_get_anomaly_score(&peer);
    EXPECT_NEAR(score, 0.3f, 0.01f);
}

TEST(SwarmByzantine, AnomalyScoreNull) {
    float score = nimcp_byzantine_get_anomaly_score(NULL);
    EXPECT_FLOAT_EQ(score, 0.0f);
}

TEST(SwarmByzantine, AnomalyScoreZeroSyncs) {
    nimcp_peer_entry_t peer = make_test_peer(12);
    peer.anomaly_count = 2;
    peer.total_syncs = 0;

    float score = nimcp_byzantine_get_anomaly_score(&peer);
    // total_syncs=0 should be treated as 1 to avoid division by zero
    EXPECT_NEAR(score, 2.0f, 0.01f);
}

// ============================================================================
// NULL Safety for Gradient Check
// ============================================================================

TEST(SwarmByzantine, GradientCheckNullArgs) {
    nimcp_peer_entry_t peer = make_test_peer(13);
    std::vector<float> grads(10, 0.01f);

    EXPECT_LT(nimcp_byzantine_check_gradient(NULL, grads.data(), 10), 0);
    EXPECT_LT(nimcp_byzantine_check_gradient(&peer, NULL, 10), 0);
    EXPECT_LT(nimcp_byzantine_check_gradient(&peer, grads.data(), 0), 0);
}
