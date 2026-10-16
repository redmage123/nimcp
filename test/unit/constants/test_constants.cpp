/**
 * @file test_constants.cpp
 * @brief Unit tests for NIMCP timing, buffer, and frequency constants
 *
 * Tests that all constants defined across the codebase are:
 * - Positive (where appropriate)
 * - Within reasonable bounds
 * - Consistent with biological plausibility constraints
 *
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cmath>
#include <climits>
#include <cfloat>

extern "C" {
/* Core constants from mesh timing */
#include "mesh/nimcp_mesh_timing.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"

/* Health agent constants */
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* Common error codes and validation */
#include "utils/validation/nimcp_common.h"

/* Bio-async constants */
#include "async/nimcp_bio_async.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ConstantsTest : public ::testing::Test {
protected:
    /* Maximum reasonable values for sanity checks */
    static constexpr uint32_t MAX_REASONABLE_TIMEOUT_MS = 3600000;  /* 1 hour */
    static constexpr uint32_t MAX_REASONABLE_BUFFER_SIZE = 1024 * 1024 * 100; /* 100 MB */
    static constexpr float MAX_REASONABLE_FREQUENCY_HZ = 10000.0f; /* 10 kHz */
    static constexpr float MIN_REASONABLE_TIMING_MS = 0.001f; /* 1 microsecond */
};

/* ============================================================================
 * Mesh Timing Constants Tests
 * ============================================================================ */

TEST_F(ConstantsTest, MeshTimingNumLevelsPositive) {
    EXPECT_GT(MESH_TIMING_NUM_LEVELS, 0)
        << "MESH_TIMING_NUM_LEVELS must be positive";
    EXPECT_LE(MESH_TIMING_NUM_LEVELS, 16)
        << "MESH_TIMING_NUM_LEVELS should be reasonable (<=16)";
}

TEST_F(ConstantsTest, MeshTimingSystemLevelConstantsPositive) {
    EXPECT_GT(MESH_TIMING_SYSTEM_BASE_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_SYSTEM_JITTER_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_SYSTEM_MIN_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_SYSTEM_MAX_MS, 0.0f);

    /* Verify ordering: min <= base <= max */
    EXPECT_LE(MESH_TIMING_SYSTEM_MIN_MS, MESH_TIMING_SYSTEM_BASE_MS);
    EXPECT_LE(MESH_TIMING_SYSTEM_BASE_MS, MESH_TIMING_SYSTEM_MAX_MS);
}

TEST_F(ConstantsTest, MeshTimingHemisphereLevelConstantsPositive) {
    EXPECT_GT(MESH_TIMING_HEMISPHERE_BASE_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_HEMISPHERE_JITTER_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_HEMISPHERE_MIN_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_HEMISPHERE_MAX_MS, 0.0f);

    EXPECT_LE(MESH_TIMING_HEMISPHERE_MIN_MS, MESH_TIMING_HEMISPHERE_BASE_MS);
    EXPECT_LE(MESH_TIMING_HEMISPHERE_BASE_MS, MESH_TIMING_HEMISPHERE_MAX_MS);
}

TEST_F(ConstantsTest, MeshTimingLayerLevelConstantsPositive) {
    EXPECT_GT(MESH_TIMING_LAYER_BASE_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_LAYER_JITTER_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_LAYER_MIN_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_LAYER_MAX_MS, 0.0f);

    EXPECT_LE(MESH_TIMING_LAYER_MIN_MS, MESH_TIMING_LAYER_BASE_MS);
    EXPECT_LE(MESH_TIMING_LAYER_BASE_MS, MESH_TIMING_LAYER_MAX_MS);
}

TEST_F(ConstantsTest, MeshTimingOrderingLevelConstantsPositive) {
    EXPECT_GT(MESH_TIMING_ORDERING_BASE_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_ORDERING_JITTER_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_ORDERING_MIN_MS, 0.0f);
    EXPECT_GT(MESH_TIMING_ORDERING_MAX_MS, 0.0f);

    EXPECT_LE(MESH_TIMING_ORDERING_MIN_MS, MESH_TIMING_ORDERING_BASE_MS);
    EXPECT_LE(MESH_TIMING_ORDERING_BASE_MS, MESH_TIMING_ORDERING_MAX_MS);
}

TEST_F(ConstantsTest, MeshTimingHierarchyDecreasing) {
    /*
     * Timing constants should decrease through hierarchy levels:
     * System (slowest) > Hemisphere > Layer > Ordering (fastest)
     */
    EXPECT_GT(MESH_TIMING_SYSTEM_BASE_MS, MESH_TIMING_HEMISPHERE_BASE_MS);
    EXPECT_GT(MESH_TIMING_HEMISPHERE_BASE_MS, MESH_TIMING_LAYER_BASE_MS);
    EXPECT_GT(MESH_TIMING_LAYER_BASE_MS, MESH_TIMING_ORDERING_BASE_MS);
}

TEST_F(ConstantsTest, MeshTimingPinkAlphaValid) {
    /* Pink noise exponent should be around 1.0 */
    EXPECT_GT(MESH_TIMING_PINK_ALPHA, 0.0f);
    EXPECT_LE(MESH_TIMING_PINK_ALPHA, 2.0f)
        << "Pink alpha should be in range (0, 2] for realistic noise";
}

TEST_F(ConstantsTest, MeshTimingConstantsReasonable) {
    /* All timing values should be reasonable (< 1 hour) */
    EXPECT_LT(MESH_TIMING_SYSTEM_MAX_MS, static_cast<float>(MAX_REASONABLE_TIMEOUT_MS));
    EXPECT_LT(MESH_TIMING_HEMISPHERE_MAX_MS, static_cast<float>(MAX_REASONABLE_TIMEOUT_MS));
    EXPECT_LT(MESH_TIMING_LAYER_MAX_MS, static_cast<float>(MAX_REASONABLE_TIMEOUT_MS));
    EXPECT_LT(MESH_TIMING_ORDERING_MAX_MS, static_cast<float>(MAX_REASONABLE_TIMEOUT_MS));
}

/* ============================================================================
 * Mesh Types Buffer Constants Tests
 * ============================================================================ */

TEST_F(ConstantsTest, MeshMaxParticipantsPositive) {
    EXPECT_GT(MESH_MAX_PARTICIPANTS_PER_CHANNEL, 0u);
    EXPECT_LE(MESH_MAX_PARTICIPANTS_PER_CHANNEL, MAX_REASONABLE_BUFFER_SIZE);
}

TEST_F(ConstantsTest, MeshMaxChannelsPositive) {
    EXPECT_GT(MESH_MAX_CHANNELS, 0u);
    EXPECT_LE(MESH_MAX_CHANNELS, 1024u)
        << "MESH_MAX_CHANNELS should be reasonable";
}

TEST_F(ConstantsTest, MeshMaxCoordinatorsPositive) {
    EXPECT_GT(MESH_MAX_COORDINATORS_PER_POOL, 0u);
    EXPECT_LE(MESH_MAX_COORDINATORS_PER_POOL, 128u)
        << "Too many coordinators per pool is inefficient";
}

TEST_F(ConstantsTest, MeshMaxEndorsersPositive) {
    EXPECT_GT(MESH_MAX_ENDORSERS_PER_POLICY, 0u);
    EXPECT_LE(MESH_MAX_ENDORSERS_PER_POLICY, 256u);
}

TEST_F(ConstantsTest, MeshMaxPendingTransactionsPositive) {
    EXPECT_GT(MESH_MAX_PENDING_TRANSACTIONS, 0u);
    EXPECT_LE(MESH_MAX_PENDING_TRANSACTIONS, 1000000u);
}

TEST_F(ConstantsTest, MeshMaxBatchSizePositive) {
    EXPECT_GT(MESH_MAX_BATCH_SIZE, 0u);
    EXPECT_LE(MESH_MAX_BATCH_SIZE, MESH_MAX_PENDING_TRANSACTIONS)
        << "Batch size should not exceed max pending transactions";
}

TEST_F(ConstantsTest, MeshMaxPayloadSizePositive) {
    EXPECT_GT(MESH_MAX_PAYLOAD_SIZE, 0u);
    EXPECT_LE(MESH_MAX_PAYLOAD_SIZE, MAX_REASONABLE_BUFFER_SIZE);
}

TEST_F(ConstantsTest, MeshBeliefVectorDimPositive) {
    EXPECT_GT(MESH_BELIEF_VECTOR_DIM, 0u);
    EXPECT_LE(MESH_BELIEF_VECTOR_DIM, 4096u)
        << "Belief vector dimension should be reasonable";
}

TEST_F(ConstantsTest, MeshMaxNameLenPositive) {
    EXPECT_GT(MESH_MAX_NAME_LEN, 0u);
    EXPECT_LE(MESH_MAX_NAME_LEN, 1024u);
}

TEST_F(ConstantsTest, MeshSignatureSizePositive) {
    EXPECT_GT(MESH_SIGNATURE_SIZE, 0u);
    /* SHA256-based signature should be at least 32 bytes */
    EXPECT_GE(MESH_SIGNATURE_SIZE, 32u);
}

TEST_F(ConstantsTest, MeshCredentialIdSizePositive) {
    EXPECT_GT(MESH_CREDENTIAL_ID_SIZE, 0u);
}

/* ============================================================================
 * Mesh Timeout Constants Tests
 * ============================================================================ */

TEST_F(ConstantsTest, MeshDefaultTxTimeoutPositive) {
    EXPECT_GT(MESH_DEFAULT_TX_TIMEOUT_MS, 0u);
    EXPECT_LT(MESH_DEFAULT_TX_TIMEOUT_MS, MAX_REASONABLE_TIMEOUT_MS);
}

TEST_F(ConstantsTest, MeshDefaultEndorsementTimeoutPositive) {
    EXPECT_GT(MESH_DEFAULT_ENDORSEMENT_TIMEOUT_MS, 0u);
    EXPECT_LT(MESH_DEFAULT_ENDORSEMENT_TIMEOUT_MS, MESH_DEFAULT_TX_TIMEOUT_MS)
        << "Endorsement timeout should be less than transaction timeout";
}

TEST_F(ConstantsTest, MeshDefaultBatchTimeoutPositive) {
    EXPECT_GT(MESH_DEFAULT_BATCH_TIMEOUT_MS, 0u);
    EXPECT_LT(MESH_DEFAULT_BATCH_TIMEOUT_MS, MESH_DEFAULT_TX_TIMEOUT_MS);
}

/* ============================================================================
 * Health Agent Constants Tests
 * ============================================================================ */

TEST_F(ConstantsTest, HealthAgentMaxQueueDepthPositive) {
    EXPECT_GT(HEALTH_AGENT_MAX_QUEUE_DEPTH, 0u);
    EXPECT_LE(HEALTH_AGENT_MAX_QUEUE_DEPTH, 1000000u);
}

TEST_F(ConstantsTest, HealthAgentDefaultHeartbeatPositive) {
    EXPECT_GT(HEALTH_AGENT_DEFAULT_HEARTBEAT_MS, 0u);
    EXPECT_LT(HEALTH_AGENT_DEFAULT_HEARTBEAT_MS, 60000u)
        << "Default heartbeat should be < 1 minute";
}

TEST_F(ConstantsTest, HealthAgentDefaultWatchdogPositive) {
    EXPECT_GT(HEALTH_AGENT_DEFAULT_WATCHDOG_MS, 0u);
    EXPECT_GT(HEALTH_AGENT_DEFAULT_WATCHDOG_MS, HEALTH_AGENT_DEFAULT_HEARTBEAT_MS)
        << "Watchdog should be longer than heartbeat";
}

TEST_F(ConstantsTest, HealthAgentDefaultCheckIntervalPositive) {
    EXPECT_GT(HEALTH_AGENT_DEFAULT_CHECK_MS, 0u);
    EXPECT_LE(HEALTH_AGENT_DEFAULT_CHECK_MS, HEALTH_AGENT_DEFAULT_HEARTBEAT_MS)
        << "Check interval should be <= heartbeat interval";
}

TEST_F(ConstantsTest, HealthAgentMagicNonZero) {
    EXPECT_NE(HEALTH_AGENT_MAGIC, 0u)
        << "Magic number should not be zero";
}

TEST_F(ConstantsTest, HealthAgentCanaryNonZero) {
    EXPECT_NE(HEALTH_AGENT_CANARY, 0ULL)
        << "Canary value should not be zero";
    EXPECT_NE(HEALTH_AGENT_CANARY, 0xFFFFFFFFFFFFFFFFULL)
        << "Canary value should not be all 1s";
}

TEST_F(ConstantsTest, HealthAgentMaxCapacityManagersPositive) {
    EXPECT_GT(HEALTH_AGENT_MAX_CAPACITY_MANAGERS, 0u);
    EXPECT_LE(HEALTH_AGENT_MAX_CAPACITY_MANAGERS, 1024u);
}

/* ============================================================================
 * Mesh Channel Constants Tests
 * ============================================================================ */

TEST_F(ConstantsTest, MeshMaxPrivateCollectionsPositive) {
    EXPECT_GT(MESH_MAX_PRIVATE_COLLECTIONS, 0u);
    EXPECT_LE(MESH_MAX_PRIVATE_COLLECTIONS, 1024u);
}

TEST_F(ConstantsTest, MeshMaxCollectionNameLenPositive) {
    EXPECT_GT(MESH_MAX_COLLECTION_NAME_LEN, 0u);
    EXPECT_LE(MESH_MAX_COLLECTION_NAME_LEN, 1024u);
}

TEST_F(ConstantsTest, MeshMaxPrivateKeyLenPositive) {
    EXPECT_GT(MESH_MAX_PRIVATE_KEY_LEN, 0u);
    EXPECT_LE(MESH_MAX_PRIVATE_KEY_LEN, 4096u);
}

TEST_F(ConstantsTest, MeshMaxPrivateValueLenPositive) {
    EXPECT_GT(MESH_MAX_PRIVATE_VALUE_LEN, 0u);
    EXPECT_LE(MESH_MAX_PRIVATE_VALUE_LEN, MAX_REASONABLE_BUFFER_SIZE);
}

TEST_F(ConstantsTest, MeshDefaultGossipRoundsPositive) {
    EXPECT_GT(MESH_DEFAULT_GOSSIP_ROUNDS, 0u);
    EXPECT_LE(MESH_DEFAULT_GOSSIP_ROUNDS, 100u);
}

TEST_F(ConstantsTest, MeshDefaultConvergenceThresholdValid) {
    EXPECT_GT(MESH_DEFAULT_CONVERGENCE_THRESHOLD, 0.0f);
    EXPECT_LT(MESH_DEFAULT_CONVERGENCE_THRESHOLD, 1.0f)
        << "Convergence threshold should be in (0, 1)";
}

/* ============================================================================
 * Well-Known Channel IDs Tests
 * ============================================================================ */

TEST_F(ConstantsTest, WellKnownChannelIdsUnique) {
    /* All well-known channel IDs should be distinct */
    EXPECT_NE(MESH_CHANNEL_SYSTEM, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_NE(MESH_CHANNEL_SYSTEM, MESH_CHANNEL_RIGHT_HEMISPHERE);
    EXPECT_NE(MESH_CHANNEL_SYSTEM, MESH_CHANNEL_SUBCORTICAL);
    EXPECT_NE(MESH_CHANNEL_SYSTEM, MESH_CHANNEL_GPU_COMPUTE);

    EXPECT_NE(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_CHANNEL_RIGHT_HEMISPHERE);
    EXPECT_NE(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_CHANNEL_SUBCORTICAL);
    EXPECT_NE(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_CHANNEL_GPU_COMPUTE);

    EXPECT_NE(MESH_CHANNEL_RIGHT_HEMISPHERE, MESH_CHANNEL_SUBCORTICAL);
    EXPECT_NE(MESH_CHANNEL_RIGHT_HEMISPHERE, MESH_CHANNEL_GPU_COMPUTE);

    EXPECT_NE(MESH_CHANNEL_SUBCORTICAL, MESH_CHANNEL_GPU_COMPUTE);
}

TEST_F(ConstantsTest, WellKnownChannelIdsInRange) {
    /* Channel IDs should be less than max channels */
    EXPECT_LT(MESH_CHANNEL_SYSTEM, MESH_MAX_CHANNELS);
    EXPECT_LT(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_MAX_CHANNELS);
    EXPECT_LT(MESH_CHANNEL_RIGHT_HEMISPHERE, MESH_MAX_CHANNELS);
    EXPECT_LT(MESH_CHANNEL_SUBCORTICAL, MESH_MAX_CHANNELS);
    EXPECT_LT(MESH_CHANNEL_GPU_COMPUTE, MESH_MAX_CHANNELS);
}

/* ============================================================================
 * Mesh Magic Number Tests
 * ============================================================================ */

TEST_F(ConstantsTest, MeshMagicNonZero) {
    EXPECT_NE(NIMCP_MESH_MAGIC, 0u);
}

TEST_F(ConstantsTest, MeshMagicNotTrivial) {
    /* Magic should not be trivially guessable patterns */
    EXPECT_NE(NIMCP_MESH_MAGIC, 0xFFFFFFFFu);
    EXPECT_NE(NIMCP_MESH_MAGIC, 0x12345678u);
    EXPECT_NE(NIMCP_MESH_MAGIC, 0xDEADBEEFu);
}

/* ============================================================================
 * Consistency Tests
 * ============================================================================ */

TEST_F(ConstantsTest, TimingConstantsNotNaN) {
    EXPECT_FALSE(std::isnan(MESH_TIMING_SYSTEM_BASE_MS));
    EXPECT_FALSE(std::isnan(MESH_TIMING_HEMISPHERE_BASE_MS));
    EXPECT_FALSE(std::isnan(MESH_TIMING_LAYER_BASE_MS));
    EXPECT_FALSE(std::isnan(MESH_TIMING_ORDERING_BASE_MS));
    EXPECT_FALSE(std::isnan(MESH_TIMING_PINK_ALPHA));
    EXPECT_FALSE(std::isnan(MESH_DEFAULT_CONVERGENCE_THRESHOLD));
}

TEST_F(ConstantsTest, TimingConstantsNotInf) {
    EXPECT_FALSE(std::isinf(MESH_TIMING_SYSTEM_BASE_MS));
    EXPECT_FALSE(std::isinf(MESH_TIMING_HEMISPHERE_BASE_MS));
    EXPECT_FALSE(std::isinf(MESH_TIMING_LAYER_BASE_MS));
    EXPECT_FALSE(std::isinf(MESH_TIMING_ORDERING_BASE_MS));
    EXPECT_FALSE(std::isinf(MESH_TIMING_PINK_ALPHA));
    EXPECT_FALSE(std::isinf(MESH_DEFAULT_CONVERGENCE_THRESHOLD));
}

TEST_F(ConstantsTest, BufferSizesNotOverflow) {
    /* Verify buffer sizes won't cause overflow when used in calculations */
    uint64_t participants_times_channels =
        static_cast<uint64_t>(MESH_MAX_PARTICIPANTS_PER_CHANNEL) *
        static_cast<uint64_t>(MESH_MAX_CHANNELS);
    EXPECT_LT(participants_times_channels, UINT32_MAX);

    uint64_t tx_times_payload =
        static_cast<uint64_t>(MESH_MAX_PENDING_TRANSACTIONS) *
        static_cast<uint64_t>(MESH_MAX_PAYLOAD_SIZE);
    EXPECT_LT(tx_times_payload, SIZE_MAX / 2)
        << "Transaction * payload should not risk overflow";
}
