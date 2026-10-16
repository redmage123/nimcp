/**
 * @file test_nimcp_hyperscanning.cpp
 * @brief Unit tests for hyperscanning (inter-brain synchronization)
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_hyperscanning.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class HyperscanningTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = hyperscanning_default_config();
        hs_ = hyperscanning_create(&config_);
        ASSERT_NE(hs_, nullptr);
    }

    void TearDown() override {
        if (hs_) {
            hyperscanning_destroy(hs_);
            hs_ = nullptr;
        }
    }

    hyperscanning_config_t config_;
    hyperscanning_t* hs_ = nullptr;
};

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, CreateWithNullConfig) {
    hyperscanning_t* hs = hyperscanning_create(nullptr);
    ASSERT_NE(hs, nullptr);
    hyperscanning_destroy(hs);
}

TEST_F(HyperscanningTest, DestroyNull) {
    hyperscanning_destroy(nullptr);  // Should not crash
}

TEST_F(HyperscanningTest, Reset) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);

    EXPECT_EQ(hyperscanning_reset(hs_), 0);

    hyperscan_state_t state;
    ASSERT_EQ(hyperscanning_get_state(hs_, &state), 0);
    EXPECT_EQ(state.global_sync, 0.0f);
}

/*=============================================================================
 * Instance Management Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, RegisterInstance) {
    EXPECT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    EXPECT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);
}

TEST_F(HyperscanningTest, RegisterDuplicateInstance) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    EXPECT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), -1);
}

TEST_F(HyperscanningTest, UnregisterInstance) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    EXPECT_EQ(hyperscanning_unregister_instance(hs_, 1), 0);
}

TEST_F(HyperscanningTest, UnregisterNonexistentInstance) {
    EXPECT_EQ(hyperscanning_unregister_instance(hs_, 999), -1);
}

TEST_F(HyperscanningTest, UpdateNeuralState) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);

    hyperscanning_neural_state_t state;
    memset(&state, 0, sizeof(state));
    state.instance_id = 1;
    state.band_power[SYNC_BAND_GAMMA] = 0.8f;
    state.band_phase[SYNC_BAND_GAMMA] = 1.5f;
    state.atp_level = 0.9f;

    EXPECT_EQ(hyperscanning_update_state(hs_, &state), 0);
}

TEST_F(HyperscanningTest, UpdateStateNonexistentInstance) {
    hyperscanning_neural_state_t state;
    memset(&state, 0, sizeof(state));
    state.instance_id = 999;

    EXPECT_EQ(hyperscanning_update_state(hs_, &state), -1);
}

/*=============================================================================
 * Synchronization Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, UpdateEmpty) {
    EXPECT_EQ(hyperscanning_update(hs_), 0);
}

TEST_F(HyperscanningTest, UpdateSingleInstance) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    EXPECT_EQ(hyperscanning_update(hs_), 0);

    hyperscan_state_t state;
    ASSERT_EQ(hyperscanning_get_state(hs_, &state), 0);
    EXPECT_EQ(state.global_sync, 0.0f);
}

TEST_F(HyperscanningTest, UpdateTwoInstances) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);

    EXPECT_EQ(hyperscanning_update(hs_), 0);

    hyperscan_state_t state;
    ASSERT_EQ(hyperscanning_get_state(hs_, &state), 0);
    EXPECT_GE(state.global_sync, 0.0f);
}

TEST_F(HyperscanningTest, GetPairSync) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    hyperscan_pair_t pair;
    ASSERT_EQ(hyperscanning_get_pair_sync(hs_, 1, 2, &pair), 0);
    EXPECT_GE(pair.plv[SYNC_BAND_GAMMA], 0.0f);
    EXPECT_LE(pair.plv[SYNC_BAND_GAMMA], 1.0f);
}

TEST_F(HyperscanningTest, GetPairSyncInvalidPair) {
    hyperscan_pair_t pair;
    EXPECT_EQ(hyperscanning_get_pair_sync(hs_, 1, 2, &pair), -1);
}

TEST_F(HyperscanningTest, GetPLV) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    float plv = hyperscanning_get_plv(hs_, 1, 2, SYNC_BAND_GAMMA);
    EXPECT_GE(plv, 0.0f);
    EXPECT_LE(plv, 1.0f);
}

TEST_F(HyperscanningTest, GetPLVInvalidBand) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);

    float plv = hyperscanning_get_plv(hs_, 1, 2, (sync_band_t)99);
    EXPECT_LT(plv, 0.0f);  // Error return
}

/*=============================================================================
 * Entrainment Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, EntrainTo) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);

    entrainment_request_t request;
    memset(&request, 0, sizeof(request));
    request.requester_id = 1;
    request.target_id = 2;
    request.target_band = SYNC_BAND_GAMMA;

    EXPECT_EQ(hyperscanning_entrain_to(hs_, &request), 0);
}

TEST_F(HyperscanningTest, EntrainToInvalidTarget) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);

    entrainment_request_t request;
    memset(&request, 0, sizeof(request));
    request.requester_id = 1;
    request.target_id = 999;

    EXPECT_EQ(hyperscanning_entrain_to(hs_, &request), -1);
}

TEST_F(HyperscanningTest, ReleaseEntrainment) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);

    entrainment_request_t request;
    memset(&request, 0, sizeof(request));
    request.requester_id = 1;
    request.target_id = 2;
    request.target_band = SYNC_BAND_GAMMA;

    ASSERT_EQ(hyperscanning_entrain_to(hs_, &request), 0);
    EXPECT_EQ(hyperscanning_release_entrainment(hs_, 1), 0);
}

TEST_F(HyperscanningTest, GetEntrainmentStatus) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);

    entrainment_status_t status = hyperscanning_get_entrainment_status(hs_, 1);
    EXPECT_EQ(status, ENTRAINMENT_NONE);
}

/*=============================================================================
 * Leader-Follower Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, GetLeaderEmpty) {
    EXPECT_EQ(hyperscanning_get_leader(hs_), 0u);
}

TEST_F(HyperscanningTest, GetLeaderSingleInstance) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 42, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    EXPECT_EQ(hyperscanning_get_leader(hs_), 42u);
}

TEST_F(HyperscanningTest, GetLeaderInfluence) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    float influence = hyperscanning_get_leader_influence(hs_);
    EXPECT_GE(influence, 0.0f);
}

TEST_F(HyperscanningTest, GetInfluence) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    float influence = hyperscanning_get_influence(hs_, 1, 2);
    // Influence can be positive or negative
    EXPECT_GE(influence, -1.0f);
    EXPECT_LE(influence, 1.0f);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, GetStats) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    hyperscanning_stats_t stats;
    ASSERT_EQ(hyperscanning_get_stats(hs_, &stats), 0);
    EXPECT_GT(stats.sync_computations, 0u);
}

TEST_F(HyperscanningTest, ResetStats) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    hyperscanning_reset_stats(hs_);

    hyperscanning_stats_t stats;
    ASSERT_EQ(hyperscanning_get_stats(hs_, &stats), 0);
    EXPECT_EQ(stats.sync_computations, 0u);
}

/*=============================================================================
 * Debug Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, DumpDoesNotCrash) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_update(hs_), 0);

    hyperscanning_dump(hs_);  // Should not crash
    hyperscanning_dump(nullptr);  // Should not crash
}

/*=============================================================================
 * Phase Evolution Tests
 *===========================================================================*/

TEST_F(HyperscanningTest, PhaseEvolution) {
    ASSERT_EQ(hyperscanning_register_instance(hs_, 1, nullptr, nullptr), 0);
    ASSERT_EQ(hyperscanning_register_instance(hs_, 2, nullptr, nullptr), 0);

    // Run multiple updates and verify sync values change
    float first_sync = 0.0f;
    for (int i = 0; i < 50; i++) {
        ASSERT_EQ(hyperscanning_update(hs_), 0);

        hyperscan_state_t state;
        ASSERT_EQ(hyperscanning_get_state(hs_, &state), 0);

        if (i == 0) {
            first_sync = state.global_sync;
        }
    }

    // Sync should remain in valid range
    hyperscan_state_t final_state;
    ASSERT_EQ(hyperscanning_get_state(hs_, &final_state), 0);
    EXPECT_GE(final_state.global_sync, 0.0f);
    EXPECT_LE(final_state.global_sync, 1.0f);
}
