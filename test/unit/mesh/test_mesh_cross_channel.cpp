/**
 * @file test_mesh_cross_channel.cpp
 * @brief Unit Tests for Cross-Channel Router and System Coordinator
 *
 * Tests: Configuration, lifecycle, channel registration, cross-channel transactions,
 *        conflict resolution, system health, and statistics.
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_cross_channel.h"
#include "mesh/nimcp_mesh_types.h"
}

/* ============================================================================
 * System Coordinator Test Fixture
 * ============================================================================ */

class MeshSystemCoordTest : public ::testing::Test {
protected:
    mesh_system_coordinator_t coord;

    void SetUp() override {
        coord = mesh_system_coord_create(nullptr, nullptr, nullptr);
        ASSERT_NE(coord, nullptr);
    }

    void TearDown() override {
        mesh_system_coord_destroy(coord);
        coord = nullptr;
    }
};

/* ============================================================================
 * Cross-Channel Router Test Fixture
 * ============================================================================ */

class MeshCrossRouterTest : public ::testing::Test {
protected:
    mesh_system_coordinator_t coord;
    mesh_cross_router_t router;

    void SetUp() override {
        coord = mesh_system_coord_create(nullptr, nullptr, nullptr);
        ASSERT_NE(coord, nullptr);

        /* Register some channels */
        ASSERT_EQ(mesh_system_coord_register_channel(coord, MESH_CHANNEL_LEFT_HEMISPHERE, "Left Hemisphere"),
                  NIMCP_SUCCESS);
        ASSERT_EQ(mesh_system_coord_register_channel(coord, MESH_CHANNEL_RIGHT_HEMISPHERE, "Right Hemisphere"),
                  NIMCP_SUCCESS);

        router = mesh_cross_router_create(nullptr, coord);
        ASSERT_NE(router, nullptr);
    }

    void TearDown() override {
        mesh_cross_router_destroy(router);
        router = nullptr;
        mesh_system_coord_destroy(coord);
        coord = nullptr;
    }

    /* Helper: Create simple cross-channel transaction */
    mesh_cross_transaction_t* create_cross_tx() {
        uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
        return mesh_cross_transaction_create(
            MESH_CHANNEL_LEFT_HEMISPHERE,
            MESH_CHANNEL_RIGHT_HEMISPHERE,
            mesh_make_participant_id(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_PARTICIPANT_MODULE, 1),
            MESH_TX_STATE_CHANGE,
            payload,
            sizeof(payload)
        );
    }
};

/* ============================================================================
 * System Coordinator Configuration Tests
 * ============================================================================ */

TEST_F(MeshSystemCoordTest, DefaultConfigHasSensibleValues) {
    mesh_system_coord_config_t config = mesh_system_coord_default_config();

    EXPECT_GT(config.arbitration_timeout_ms, 0.0f);
    EXPECT_GT(config.health_check_interval_ms, 0.0f);
    EXPECT_TRUE(config.enable_fep_arbitration);
    EXPECT_GT(config.fe_threshold, 0.0f);
    EXPECT_GT(config.max_pending_conflicts, 0u);
}

TEST_F(MeshSystemCoordTest, CreateWithCustomConfig) {
    mesh_system_coord_config_t config = mesh_system_coord_default_config();
    config.enable_fep_arbitration = false;
    config.fe_threshold = 0.1f;

    mesh_system_coordinator_t custom = mesh_system_coord_create(&config, nullptr, nullptr);
    ASSERT_NE(custom, nullptr);

    mesh_system_coord_destroy(custom);
}

/* ============================================================================
 * Channel Registration Tests
 * ============================================================================ */

TEST_F(MeshSystemCoordTest, RegisterChannel) {
    EXPECT_EQ(mesh_system_coord_register_channel(coord, 1, "Test Channel"), NIMCP_SUCCESS);
}

TEST_F(MeshSystemCoordTest, RegisterMultipleChannels) {
    for (uint16_t i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Channel %u", i);
        EXPECT_EQ(mesh_system_coord_register_channel(coord, i, name), NIMCP_SUCCESS);
    }
}

TEST_F(MeshSystemCoordTest, RegisterDuplicateIsIdempotent) {
    EXPECT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1"), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1 Again"), NIMCP_SUCCESS);
}

TEST_F(MeshSystemCoordTest, UnregisterChannel) {
    ASSERT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1"), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_system_coord_unregister_channel(coord, 1), NIMCP_SUCCESS);
}

TEST_F(MeshSystemCoordTest, UnregisterNonexistent) {
    EXPECT_EQ(mesh_system_coord_unregister_channel(coord, 999), NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshSystemCoordTest, RegisterWithNullName) {
    EXPECT_EQ(mesh_system_coord_register_channel(coord, 1, nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Cross-Channel Router Configuration Tests
 * ============================================================================ */

TEST_F(MeshCrossRouterTest, DefaultConfigHasSensibleValues) {
    mesh_cross_router_config_t config = mesh_cross_router_default_config();

    EXPECT_GT(config.endorsement_timeout_ms, 0.0f);
    EXPECT_GT(config.transaction_timeout_ms, 0.0f);
    EXPECT_GT(config.max_pending, 0u);
    EXPECT_TRUE(config.require_both_endorsements);
}

TEST_F(MeshCrossRouterTest, CreateWithCustomConfig) {
    mesh_cross_router_config_t config = mesh_cross_router_default_config();
    config.max_pending = 100;
    config.enable_parallel_endorsement = false;

    mesh_cross_router_t custom = mesh_cross_router_create(&config, coord);
    ASSERT_NE(custom, nullptr);

    mesh_cross_router_destroy(custom);
}

/* ============================================================================
 * Router Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshCrossRouterTest, StartAndStop) {
    EXPECT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_cross_router_stop(router, false), NIMCP_SUCCESS);
}

TEST_F(MeshCrossRouterTest, StartIdempotent) {
    EXPECT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);
}

TEST_F(MeshCrossRouterTest, StopWithDrain) {
    EXPECT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_cross_router_stop(router, true), NIMCP_SUCCESS);
}

/* ============================================================================
 * Cross-Channel Transaction Tests
 * ============================================================================ */

TEST_F(MeshCrossRouterTest, CreateTransaction) {
    mesh_cross_transaction_t* tx = create_cross_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(tx->source_channel, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(tx->target_channel, MESH_CHANNEL_RIGHT_HEMISPHERE);
    EXPECT_EQ(tx->tx_type, MESH_TX_STATE_CHANGE);
    EXPECT_EQ(tx->status, MESH_CROSS_TX_PENDING);
    EXPECT_NE(tx->payload, nullptr);
    EXPECT_EQ(tx->payload_size, 4u);

    mesh_cross_transaction_destroy(tx);
}

TEST_F(MeshCrossRouterTest, CreateTransactionNoPayload) {
    mesh_cross_transaction_t* tx = mesh_cross_transaction_create(
        MESH_CHANNEL_LEFT_HEMISPHERE,
        MESH_CHANNEL_RIGHT_HEMISPHERE,
        mesh_make_participant_id(MESH_CHANNEL_LEFT_HEMISPHERE, MESH_PARTICIPANT_MODULE, 1),
        MESH_TX_CONSENSUS_VOTE,
        nullptr,
        0
    );
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(tx->payload, nullptr);
    EXPECT_EQ(tx->payload_size, 0u);

    mesh_cross_transaction_destroy(tx);
}

TEST_F(MeshCrossRouterTest, DestroyNullTransaction) {
    mesh_cross_transaction_destroy(nullptr);  /* Should not crash */
}

TEST_F(MeshCrossRouterTest, SubmitRequiresStart) {
    mesh_cross_transaction_t* tx = create_cross_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(mesh_cross_router_submit(router, tx), NIMCP_ERROR_NOT_READY);

    mesh_cross_transaction_destroy(tx);
}

TEST_F(MeshCrossRouterTest, SubmitTransaction) {
    ASSERT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);

    mesh_cross_transaction_t* tx = create_cross_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(mesh_cross_router_submit(router, tx), NIMCP_SUCCESS);
    EXPECT_EQ(tx->status, MESH_CROSS_TX_COMMITTED);

    mesh_cross_transaction_destroy(tx);
}

TEST_F(MeshCrossRouterTest, SubmitWithCallback) {
    ASSERT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);

    static bool callback_called = false;
    static mesh_tx_status_t callback_status = MESH_TX_STATUS_NONE;

    auto callback = [](const mesh_result_t* result, void* ctx) {
        (void)ctx;
        callback_called = true;
        callback_status = result->status;
    };

    mesh_cross_transaction_t* tx = create_cross_tx();
    ASSERT_NE(tx, nullptr);

    EXPECT_EQ(mesh_cross_router_submit_async(router, tx, callback, nullptr), NIMCP_SUCCESS);
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_status, MESH_TX_STATUS_COMMITTED);

    mesh_cross_transaction_destroy(tx);
}

TEST_F(MeshCrossRouterTest, SubmitMultipleTransactions) {
    ASSERT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);

    for (int i = 0; i < 10; i++) {
        mesh_cross_transaction_t* tx = create_cross_tx();
        ASSERT_NE(tx, nullptr);
        EXPECT_EQ(mesh_cross_router_submit(router, tx), NIMCP_SUCCESS);
        mesh_cross_transaction_destroy(tx);
    }
}

/* ============================================================================
 * Conflict Resolution Tests
 * ============================================================================ */

TEST_F(MeshSystemCoordTest, ArbitrateConflict) {
    /* Register channels first */
    mesh_system_coord_register_channel(coord, 1, "Channel 1");
    mesh_system_coord_register_channel(coord, 2, "Channel 2");

    /* Create two conflicting transactions */
    mesh_cross_transaction_t* tx1 = mesh_cross_transaction_create(
        1, 2,
        mesh_make_participant_id(1, MESH_PARTICIPANT_MODULE, 1),
        MESH_TX_STATE_CHANGE, nullptr, 0
    );
    mesh_cross_transaction_t* tx2 = mesh_cross_transaction_create(
        1, 2,
        mesh_make_participant_id(1, MESH_PARTICIPANT_MODULE, 2),
        MESH_TX_STATE_CHANGE, nullptr, 0
    );
    ASSERT_NE(tx1, nullptr);
    ASSERT_NE(tx2, nullptr);

    mesh_cross_transaction_t* winner = nullptr;
    EXPECT_EQ(mesh_system_coord_arbitrate(coord, tx1, tx2, &winner), NIMCP_SUCCESS);
    ASSERT_NE(winner, nullptr);

    /* One should win, one should lose */
    EXPECT_TRUE(
        (tx1->conflict_result == MESH_CONFLICT_WINNER && tx2->conflict_result == MESH_CONFLICT_LOSER) ||
        (tx2->conflict_result == MESH_CONFLICT_WINNER && tx1->conflict_result == MESH_CONFLICT_LOSER)
    );

    mesh_cross_transaction_destroy(tx1);
    mesh_cross_transaction_destroy(tx2);
}

TEST_F(MeshSystemCoordTest, ComputeFreeEnergy) {
    mesh_cross_transaction_t* tx = mesh_cross_transaction_create(
        1, 2,
        mesh_make_participant_id(1, MESH_PARTICIPANT_MODULE, 1),
        MESH_TX_STATE_CHANGE, nullptr, 0
    );
    ASSERT_NE(tx, nullptr);

    float fe;
    EXPECT_EQ(mesh_system_coord_compute_free_energy(coord, tx, &fe), NIMCP_SUCCESS);
    EXPECT_GE(fe, 0.0f);
    EXPECT_TRUE(tx->fe_computed);

    mesh_cross_transaction_destroy(tx);
}

TEST_F(MeshSystemCoordTest, TransactionsConflict) {
    mesh_cross_transaction_t* tx1 = mesh_cross_transaction_create(
        1, 2,
        mesh_make_participant_id(1, MESH_PARTICIPANT_MODULE, 1),
        MESH_TX_STATE_CHANGE, nullptr, 0
    );
    mesh_cross_transaction_t* tx2 = mesh_cross_transaction_create(
        1, 2,
        mesh_make_participant_id(1, MESH_PARTICIPANT_MODULE, 2),
        MESH_TX_STATE_CHANGE, nullptr, 0
    );
    ASSERT_NE(tx1, nullptr);
    ASSERT_NE(tx2, nullptr);

    /* Same target channel, same type -> conflict */
    EXPECT_TRUE(mesh_cross_transactions_conflict(tx1, tx2));

    mesh_cross_transaction_destroy(tx1);
    mesh_cross_transaction_destroy(tx2);
}

TEST_F(MeshSystemCoordTest, TransactionsDontConflict) {
    mesh_cross_transaction_t* tx1 = mesh_cross_transaction_create(
        1, 2,
        mesh_make_participant_id(1, MESH_PARTICIPANT_MODULE, 1),
        MESH_TX_STATE_CHANGE, nullptr, 0
    );
    mesh_cross_transaction_t* tx2 = mesh_cross_transaction_create(
        1, 3,  /* Different target channel */
        mesh_make_participant_id(1, MESH_PARTICIPANT_MODULE, 2),
        MESH_TX_STATE_CHANGE, nullptr, 0
    );
    ASSERT_NE(tx1, nullptr);
    ASSERT_NE(tx2, nullptr);

    /* Different target channels -> no conflict */
    EXPECT_FALSE(mesh_cross_transactions_conflict(tx1, tx2));

    mesh_cross_transaction_destroy(tx1);
    mesh_cross_transaction_destroy(tx2);
}

/* ============================================================================
 * System Health Tests
 * ============================================================================ */

TEST_F(MeshSystemCoordTest, InitiallyHealthy) {
    /* With no channels, system is healthy */
    EXPECT_TRUE(mesh_system_coord_is_healthy(coord));
}

TEST_F(MeshSystemCoordTest, ChannelHealthTracking) {
    ASSERT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1"), NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_system_coord_channel_healthy(coord, 1));
    EXPECT_TRUE(mesh_system_coord_is_healthy(coord));
}

TEST_F(MeshSystemCoordTest, MarkUnhealthy) {
    ASSERT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1"), NIMCP_SUCCESS);

    EXPECT_EQ(mesh_system_coord_mark_unhealthy(coord, 1, "Test unhealthy"), NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_system_coord_channel_healthy(coord, 1));
    EXPECT_FALSE(mesh_system_coord_is_healthy(coord));
}

TEST_F(MeshSystemCoordTest, MarkHealthy) {
    ASSERT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1"), NIMCP_SUCCESS);
    mesh_system_coord_mark_unhealthy(coord, 1, "Test");

    EXPECT_EQ(mesh_system_coord_mark_healthy(coord, 1), NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_system_coord_channel_healthy(coord, 1));
}

TEST_F(MeshSystemCoordTest, MarkNonexistentChannel) {
    EXPECT_EQ(mesh_system_coord_mark_unhealthy(coord, 999, "Test"), NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(mesh_system_coord_mark_healthy(coord, 999), NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshSystemCoordTest, ChannelHealthyNonexistent) {
    EXPECT_FALSE(mesh_system_coord_channel_healthy(coord, 999));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshSystemCoordTest, GetStats) {
    ASSERT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1"), NIMCP_SUCCESS);

    mesh_system_coord_stats_t stats;
    EXPECT_EQ(mesh_system_coord_get_stats(coord, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.channel_count, 1u);
    ASSERT_NE(stats.channel_stats, nullptr);
    EXPECT_EQ(stats.channel_stats[0].channel_id, 1u);

    mesh_system_coord_stats_free(&stats);
}

TEST_F(MeshSystemCoordTest, ResetStats) {
    ASSERT_EQ(mesh_system_coord_register_channel(coord, 1, "Channel 1"), NIMCP_SUCCESS);

    EXPECT_EQ(mesh_system_coord_reset_stats(coord), NIMCP_SUCCESS);

    mesh_system_coord_stats_t stats;
    EXPECT_EQ(mesh_system_coord_get_stats(coord, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_cross_transactions, 0u);

    mesh_system_coord_stats_free(&stats);
}

TEST_F(MeshSystemCoordTest, StatsFreeHandlesNull) {
    mesh_system_coord_stats_free(nullptr);  /* Should not crash */

    mesh_system_coord_stats_t stats = {0};
    mesh_system_coord_stats_free(&stats);  /* Should handle empty */
}

TEST_F(MeshCrossRouterTest, PendingCount) {
    EXPECT_EQ(mesh_cross_router_pending_count(router), 0u);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST(MeshCrossChannelUtils, CrossTxStatusToString) {
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_PENDING), "PENDING");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_VALIDATING), "VALIDATING");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_ENDORSING_SOURCE), "ENDORSING_SOURCE");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_ENDORSING_TARGET), "ENDORSING_TARGET");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_ORDERING), "ORDERING");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_COMMITTING), "COMMITTING");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_COMMITTED), "COMMITTED");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_FAILED), "FAILED");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_CONFLICT), "CONFLICT");
    EXPECT_STREQ(mesh_cross_tx_status_to_string(MESH_CROSS_TX_ACCESS_DENIED), "ACCESS_DENIED");
    EXPECT_STREQ(mesh_cross_tx_status_to_string((mesh_cross_tx_status_t)999), "UNKNOWN");
}

TEST(MeshCrossChannelUtils, ConflictResultToString) {
    EXPECT_STREQ(mesh_conflict_result_to_string(MESH_CONFLICT_NONE), "NONE");
    EXPECT_STREQ(mesh_conflict_result_to_string(MESH_CONFLICT_WINNER), "WINNER");
    EXPECT_STREQ(mesh_conflict_result_to_string(MESH_CONFLICT_LOSER), "LOSER");
    EXPECT_STREQ(mesh_conflict_result_to_string(MESH_CONFLICT_MERGED), "MERGED");
    EXPECT_STREQ(mesh_conflict_result_to_string(MESH_CONFLICT_DEFERRED), "DEFERRED");
    EXPECT_STREQ(mesh_conflict_result_to_string((mesh_conflict_result_t)999), "UNKNOWN");
}

/* ============================================================================
 * Debug Output Tests
 * ============================================================================ */

TEST_F(MeshSystemCoordTest, PrintDebugDoesNotCrash) {
    mesh_system_coord_register_channel(coord, 1, "Channel 1");
    mesh_system_coord_print_debug(coord);  /* Should not crash */
    mesh_system_coord_print_debug(nullptr);  /* Should handle NULL */
}

TEST_F(MeshCrossRouterTest, PrintDebugDoesNotCrash) {
    mesh_cross_router_print_debug(router);  /* Should not crash */
    mesh_cross_router_print_debug(nullptr);  /* Should handle NULL */
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

TEST(MeshCrossChannelNull, SystemCoordNullHandling) {
    EXPECT_EQ(mesh_system_coord_register_channel(nullptr, 1, "Test"), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_system_coord_unregister_channel(nullptr, 1), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_FALSE(mesh_system_coord_is_healthy(nullptr));
    EXPECT_FALSE(mesh_system_coord_channel_healthy(nullptr, 1));
    EXPECT_EQ(mesh_system_coord_mark_unhealthy(nullptr, 1, "Test"), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_system_coord_mark_healthy(nullptr, 1), NIMCP_ERROR_INVALID_PARAMETER);

    mesh_system_coord_stats_t stats;
    EXPECT_EQ(mesh_system_coord_get_stats(nullptr, &stats), NIMCP_ERROR_INVALID_PARAMETER);
}

TEST(MeshCrossChannelNull, RouterNullHandling) {
    EXPECT_EQ(mesh_cross_router_start(nullptr), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_cross_router_stop(nullptr, false), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_cross_router_submit(nullptr, nullptr), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_cross_router_pending_count(nullptr), 0u);
}

TEST_F(MeshCrossRouterTest, NullTransactionHandling) {
    ASSERT_EQ(mesh_cross_router_start(router), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_cross_router_submit(router, nullptr), NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
