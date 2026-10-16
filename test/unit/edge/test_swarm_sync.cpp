/**
 * @file test_swarm_sync.cpp
 * @brief Unit tests for swarm sync round — create, begin, submit, ready, reset.
 *
 * WHAT: Test sync round lifecycle, gradient submission, readiness check,
 *       phase transitions, and NULL safety.
 * WHY:  The sync round coordinates federated learning across the swarm;
 *       incorrect phase management or gradient handling breaks convergence.
 * HOW:  Google Test, using nimcp_sync_round_t from nimcp_swarm_runtime_types.h.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime_types.h"

/* Forward-declare internal sync round functions.
 * These are defined in nimcp_swarm_sync.c and use types from
 * nimcp_swarm_runtime_types.h (which that file includes). */
nimcp_sync_round_t*     nimcp_sync_round_create(uint32_t max_peers,
                            uint32_t num_params);
void                    nimcp_sync_round_destroy(nimcp_sync_round_t* round);
int                     nimcp_sync_round_begin(nimcp_sync_round_t* round,
                            uint64_t round_id, uint32_t expected_peers,
                            uint32_t timeout_ms);
int                     nimcp_sync_round_submit_gradient(nimcp_sync_round_t* round,
                            uint32_t device_id, const float* gradients,
                            uint32_t num_params);
bool                    nimcp_sync_round_is_ready(const nimcp_sync_round_t* round);
void                    nimcp_sync_round_reset(nimcp_sync_round_t* round);
nimcp_sync_phase_t      nimcp_sync_round_get_phase(const nimcp_sync_round_t* round);
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(SwarmSync, CreateDestroy) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 1024);
    ASSERT_NE(round, nullptr);
    EXPECT_EQ(round->phase, NIMCP_SYNC_IDLE);
    EXPECT_EQ(round->num_params, 1024u);
    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, DestroyNull) {
    nimcp_sync_round_destroy(NULL);
    SUCCEED() << "nimcp_sync_round_destroy(NULL) did not crash";
}

TEST(SwarmSync, CreateZeroParams) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 0);
    EXPECT_EQ(round, nullptr) << "Zero params should fail";
}

TEST(SwarmSync, CreateZeroPeers) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(0, 1024);
    EXPECT_EQ(round, nullptr) << "Zero peers should fail";
}

// ============================================================================
// Begin Round
// ============================================================================

TEST(SwarmSync, BeginRound) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 128);
    ASSERT_NE(round, nullptr);

    int rc = nimcp_sync_round_begin(round, 1, 3, 10000);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_sync_round_get_phase(round), NIMCP_SYNC_COLLECTING);
    EXPECT_EQ(round->gradients_expected, 3u);
    EXPECT_EQ(round->gradients_received, 0u);

    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, BeginWhileNotIdle) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 128);
    ASSERT_NE(round, nullptr);

    nimcp_sync_round_begin(round, 1, 2, 5000);
    int rc = nimcp_sync_round_begin(round, 2, 2, 5000);
    EXPECT_LT(rc, 0) << "Cannot begin while already collecting";

    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, BeginNull) {
    EXPECT_LT(nimcp_sync_round_begin(NULL, 1, 2, 5000), 0);
}

TEST(SwarmSync, BeginZeroPeers) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 128);
    ASSERT_NE(round, nullptr);

    int rc = nimcp_sync_round_begin(round, 1, 0, 5000);
    EXPECT_LT(rc, 0) << "Zero expected peers should fail";

    nimcp_sync_round_destroy(round);
}

// ============================================================================
// Submit Gradient
// ============================================================================

TEST(SwarmSync, SubmitGradient) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 64);
    ASSERT_NE(round, nullptr);
    nimcp_sync_round_begin(round, 1, 3, 10000);

    std::vector<float> grads(64, 0.01f);
    int rc = nimcp_sync_round_submit_gradient(round, 42, grads.data(), 64);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(round->gradients_received, 1u);

    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, SubmitWrongPhase) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 64);
    ASSERT_NE(round, nullptr);
    // Still in IDLE — submission should be rejected

    std::vector<float> grads(64, 0.01f);
    int rc = nimcp_sync_round_submit_gradient(round, 1, grads.data(), 64);
    EXPECT_LT(rc, 0) << "Submit while IDLE should be rejected";

    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, SubmitWrongParamCount) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 64);
    ASSERT_NE(round, nullptr);
    nimcp_sync_round_begin(round, 1, 2, 10000);

    std::vector<float> grads(128, 0.01f); // Wrong count
    int rc = nimcp_sync_round_submit_gradient(round, 1, grads.data(), 128);
    EXPECT_LT(rc, 0) << "Wrong param count should be rejected";

    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, SubmitNull) {
    EXPECT_LT(nimcp_sync_round_submit_gradient(NULL, 1, NULL, 0), 0);
}

// ============================================================================
// Ready Check
// ============================================================================

TEST(SwarmSync, ReadyWhenAllReceived) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 32);
    ASSERT_NE(round, nullptr);
    nimcp_sync_round_begin(round, 1, 2, 60000);

    std::vector<float> grads(32, 0.1f);
    nimcp_sync_round_submit_gradient(round, 1, grads.data(), 32);
    EXPECT_FALSE(nimcp_sync_round_is_ready(round));

    nimcp_sync_round_submit_gradient(round, 2, grads.data(), 32);
    EXPECT_TRUE(nimcp_sync_round_is_ready(round));

    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, ReadyNull) {
    EXPECT_FALSE(nimcp_sync_round_is_ready(NULL));
}

TEST(SwarmSync, ReadyWhileIdle) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 32);
    ASSERT_NE(round, nullptr);
    EXPECT_FALSE(nimcp_sync_round_is_ready(round));
    nimcp_sync_round_destroy(round);
}

// ============================================================================
// Reset
// ============================================================================

TEST(SwarmSync, ResetToIdle) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 32);
    ASSERT_NE(round, nullptr);
    nimcp_sync_round_begin(round, 1, 2, 10000);

    std::vector<float> grads(32, 0.1f);
    nimcp_sync_round_submit_gradient(round, 1, grads.data(), 32);

    nimcp_sync_round_reset(round);
    EXPECT_EQ(nimcp_sync_round_get_phase(round), NIMCP_SYNC_IDLE);
    EXPECT_EQ(round->gradients_received, 0u);

    nimcp_sync_round_destroy(round);
}

TEST(SwarmSync, ResetNull) {
    nimcp_sync_round_reset(NULL);
    SUCCEED() << "nimcp_sync_round_reset(NULL) did not crash";
}

// ============================================================================
// Phase Getter
// ============================================================================

TEST(SwarmSync, PhaseGetterNull) {
    EXPECT_EQ(nimcp_sync_round_get_phase(NULL), NIMCP_SYNC_IDLE);
}

TEST(SwarmSync, PhaseTransitions) {
    nimcp_sync_round_t* round = nimcp_sync_round_create(4, 32);
    ASSERT_NE(round, nullptr);

    EXPECT_EQ(nimcp_sync_round_get_phase(round), NIMCP_SYNC_IDLE);

    nimcp_sync_round_begin(round, 1, 1, 10000);
    EXPECT_EQ(nimcp_sync_round_get_phase(round), NIMCP_SYNC_COLLECTING);

    nimcp_sync_round_reset(round);
    EXPECT_EQ(nimcp_sync_round_get_phase(round), NIMCP_SYNC_IDLE);

    nimcp_sync_round_destroy(round);
}
