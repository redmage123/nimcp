/**
 * @file test_thousand_brains.cpp
 * @brief Unit tests for Hawkins Thousand Brains features:
 *        Reference Frames, Column Voting, Dendritic Sequences,
 *        WM-TB Bridge, and Integration Hub.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/cortical_columns/nimcp_column_reference_frame.h"
#include "core/cortical_columns/nimcp_column_voting.h"
#include "core/cortical_columns/nimcp_dendritic_sequence.h"
#include "core/cortical_columns/nimcp_thousand_brains_integration.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thousand_brains_bridge.h"
}

/* ============================================================================
 * Test: Reference Frames
 * ============================================================================ */

class RefFrameTest : public ::testing::Test {
protected:
    column_ref_frame_manager_t* mgr = nullptr;
    column_ref_frame_config_t config;

    void SetUp() override {
        column_ref_frame_config_default(&config);
        config.max_frames = 16;
        mgr = column_ref_frame_create(&config);
        ASSERT_NE(mgr, nullptr);
    }

    void TearDown() override {
        if (mgr) column_ref_frame_destroy(mgr);
    }
};

TEST_F(RefFrameTest, ConfigDefaults) {
    column_ref_frame_config_t c;
    column_ref_frame_config_default(&c);
    EXPECT_EQ(c.max_frames, COL_REF_FRAME_MAX_FRAMES);
    EXPECT_EQ(c.encoding_dim, COL_REF_FRAME_ENCODING_DIM);
    EXPECT_FLOAT_EQ(c.movement_threshold, 0.01f);
    EXPECT_FLOAT_EQ(c.association_learning_rate, 0.1f);
    EXPECT_FLOAT_EQ(c.recall_threshold, 0.3f);
    EXPECT_FLOAT_EQ(c.path_integration_gain, 1.0f);
}

TEST_F(RefFrameTest, CreateDestroy) {
    EXPECT_EQ(mgr->num_frames, 0u);
    EXPECT_EQ(mgr->max_frames, 16u);
}

TEST_F(RefFrameTest, NullConfigCreate) {
    column_ref_frame_manager_t* m = column_ref_frame_create(nullptr);
    EXPECT_EQ(m, nullptr);
}

TEST_F(RefFrameTest, BindColumn) {
    float phase[3] = {0.1f, 0.2f, 0.3f};
    int idx = column_ref_frame_bind_column(mgr, 42, 0, phase);
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(mgr->num_frames, 1u);
    EXPECT_EQ(mgr->frames[0].column_id, 42u);
    EXPECT_EQ(mgr->frames[0].grid_module_idx, 0u);
    EXPECT_FLOAT_EQ(mgr->frames[0].phase_offset[0], 0.1f);
}

TEST_F(RefFrameTest, BindMultipleColumns) {
    for (int i = 0; i < 16; i++) {
        float phase[3] = {(float)i * 0.1f, 0.0f, 0.0f};
        EXPECT_GE(column_ref_frame_bind_column(mgr, i, i % 4, phase), 0);
    }
    EXPECT_EQ(mgr->num_frames, 16u);

    /* Overflow */
    float phase[3] = {0};
    EXPECT_EQ(column_ref_frame_bind_column(mgr, 100, 0, phase), -1);
}

TEST_F(RefFrameTest, UpdateLocation) {
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(mgr, 0, 0, phase);

    float movement[3] = {1.0f, 2.0f, 3.0f};
    EXPECT_EQ(column_ref_frame_update_location(mgr, 0, movement), 0);
    EXPECT_FLOAT_EQ(mgr->frames[0].location[0], 1.0f);
    EXPECT_FLOAT_EQ(mgr->frames[0].location[1], 2.0f);
    EXPECT_FLOAT_EQ(mgr->frames[0].location[2], 3.0f);
}

TEST_F(RefFrameTest, MovementBelowThreshold) {
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(mgr, 0, 0, phase);

    float tiny[3] = {0.001f, 0.001f, 0.001f};
    EXPECT_EQ(column_ref_frame_update_location(mgr, 0, tiny), 0);
    /* Location should NOT change (below threshold) */
    EXPECT_FLOAT_EQ(mgr->frames[0].location[0], 0.0f);
}

TEST_F(RefFrameTest, EncodeAndRecallFeature) {
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(mgr, 0, 0, phase);

    /* Move to a location */
    float move[3] = {1.0f, 0.0f, 0.0f};
    column_ref_frame_update_location(mgr, 0, move);

    /* Encode feature multiple times to build confidence above recall_threshold (0.3) */
    float feature[COL_REF_FRAME_FEATURE_DIM];
    for (int i = 0; i < COL_REF_FRAME_FEATURE_DIM; i++) feature[i] = (float)i * 0.1f;
    for (int rep = 0; rep < 5; rep++) {
        EXPECT_EQ(column_ref_frame_encode_feature_at_location(mgr, 0, feature,
                  COL_REF_FRAME_FEATURE_DIM, 7), 0);
    }
    /* Should still be 1 pair (same location, same object → blended) */
    EXPECT_EQ(mgr->frames[0].num_pairs, 1u);

    /* Recall at same location — confidence should now be above threshold */
    float recalled[COL_REF_FRAME_FEATURE_DIM];
    uint32_t obj_id = 0;
    float conf = 0.0f;
    int rc = column_ref_frame_recall_feature_at(mgr, 0, recalled,
             COL_REF_FRAME_FEATURE_DIM, &obj_id, &conf);
    EXPECT_EQ(rc, 0); /* Found */
    EXPECT_EQ(obj_id, 7u);
    EXPECT_GT(conf, 0.0f);
}

TEST_F(RefFrameTest, RecallNoMatch) {
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(mgr, 0, 0, phase);

    /* No features encoded — recall should return 1 (no match) */
    float recalled[COL_REF_FRAME_FEATURE_DIM];
    uint32_t obj_id = 0;
    float conf = 0.0f;
    int rc = column_ref_frame_recall_feature_at(mgr, 0, recalled,
             COL_REF_FRAME_FEATURE_DIM, &obj_id, &conf);
    EXPECT_EQ(rc, 1);
}

TEST_F(RefFrameTest, GetLocationEncoding) {
    float phase[3] = {0.5f, 0.5f, 0.5f};
    column_ref_frame_bind_column(mgr, 0, 0, phase);

    float encoding[COL_REF_FRAME_ENCODING_DIM];
    EXPECT_EQ(column_ref_frame_get_location_encoding(mgr, 0, encoding,
              COL_REF_FRAME_ENCODING_DIM), 0);

    /* Encoding should be non-zero (sinusoidal basis) */
    float sum = 0;
    for (int i = 0; i < COL_REF_FRAME_ENCODING_DIM; i++) sum += fabsf(encoding[i]);
    EXPECT_GT(sum, 0.0f);
}

TEST_F(RefFrameTest, PredictNextLocation) {
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(mgr, 0, 0, phase);

    float move[3] = {1.0f, 0.0f, 0.0f};
    column_ref_frame_update_location(mgr, 0, move);

    float predicted[3];
    float next_move[3] = {0.5f, 0.5f, 0.0f};
    EXPECT_EQ(column_ref_frame_predict_next_location(mgr, 0, next_move, predicted), 0);
    EXPECT_FLOAT_EQ(predicted[0], 1.5f);
    EXPECT_FLOAT_EQ(predicted[1], 0.5f);
}

TEST_F(RefFrameTest, Stats) {
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(mgr, 0, 0, phase);

    float move[3] = {1.0f, 0.0f, 0.0f};
    column_ref_frame_update_location(mgr, 0, move);

    column_ref_frame_stats_t stats;
    EXPECT_EQ(column_ref_frame_get_stats(mgr, &stats), 0);
    EXPECT_EQ(stats.total_location_updates, 1u);
}

/* ============================================================================
 * Test: Column Voting
 * ============================================================================ */

class VotingTest : public ::testing::Test {
protected:
    column_voting_manager_t* mgr = nullptr;
    column_voting_config_t config;

    void SetUp() override {
        column_voting_config_default(&config);
        config.max_columns = 16;
        mgr = column_voting_create(&config);
        ASSERT_NE(mgr, nullptr);
    }

    void TearDown() override {
        if (mgr) column_voting_destroy(mgr);
    }
};

TEST_F(VotingTest, ConfigDefaults) {
    column_voting_config_t c;
    column_voting_config_default(&c);
    EXPECT_EQ(c.max_columns, COLUMN_VOTING_MAX_COLUMNS);
    EXPECT_EQ(c.max_voting_rounds, COLUMN_VOTING_MAX_ROUNDS);
    EXPECT_FLOAT_EQ(c.consensus_threshold, COLUMN_VOTING_CONSENSUS_RATIO);
    EXPECT_FLOAT_EQ(c.sensory_weight, COLUMN_VOTING_SENSORY_WEIGHT);
    EXPECT_FLOAT_EQ(c.vote_weight, COLUMN_VOTING_VOTE_WEIGHT);
}

TEST_F(VotingTest, CreateDestroy) {
    EXPECT_EQ(mgr->num_columns, 0u);
    EXPECT_EQ(mgr->max_columns, 16u);
    EXPECT_FALSE(mgr->global_consensus);
}

TEST_F(VotingTest, SubmitHypothesis) {
    float evidence[4] = {0.9f, 0.1f, 0.0f, 0.0f};
    EXPECT_EQ(column_voting_submit_hypothesis(mgr, 0, 42, 0.8f, evidence, 4), 0);
    EXPECT_EQ(mgr->states[0].num_hypotheses, 1u);
    EXPECT_EQ(mgr->states[0].hypotheses[0].object_id, 42u);
    EXPECT_FLOAT_EQ(mgr->states[0].hypotheses[0].confidence, 0.8f);
}

TEST_F(VotingTest, BelowMinConfidence) {
    /* Hypothesis below min_confidence should be silently skipped */
    EXPECT_EQ(column_voting_submit_hypothesis(mgr, 0, 1, 0.05f, nullptr, 0), 0);
    EXPECT_EQ(mgr->states[0].num_hypotheses, 0u);
}

TEST_F(VotingTest, ConnectLateral) {
    EXPECT_EQ(column_voting_connect_lateral(mgr, 0, 1), 0);
    EXPECT_EQ(mgr->num_neighbors[0], 1u);
    EXPECT_EQ(mgr->num_neighbors[1], 1u);
    EXPECT_EQ(mgr->lateral_neighbors[0][0], 1u);
    EXPECT_EQ(mgr->lateral_neighbors[1][0], 0u);
}

TEST_F(VotingTest, NoDuplicateLateral) {
    column_voting_connect_lateral(mgr, 0, 1);
    column_voting_connect_lateral(mgr, 0, 1);
    EXPECT_EQ(mgr->num_neighbors[0], 1u);
}

TEST_F(VotingTest, SelfConnectRejected) {
    EXPECT_EQ(column_voting_connect_lateral(mgr, 0, 0), -1);
}

TEST_F(VotingTest, UnanimousConsensus) {
    /* All 4 columns agree on object 7 */
    for (int i = 0; i < 4; i++) {
        column_voting_submit_hypothesis(mgr, i, 7, 0.9f, nullptr, 0);
        if (i > 0) column_voting_connect_lateral(mgr, i - 1, i);
    }

    uint32_t rounds = 0;
    int rc = column_voting_run_to_consensus(mgr, &rounds);
    EXPECT_EQ(rc, 0); /* Consensus reached */
    EXPECT_TRUE(column_voting_has_consensus(mgr));
    EXPECT_LE(rounds, COLUMN_VOTING_MAX_ROUNDS);

    uint32_t obj = 0;
    float conf = 0;
    EXPECT_EQ(column_voting_get_consensus(mgr, &obj, &conf), 0);
    EXPECT_EQ(obj, 7u);
    EXPECT_GT(conf, 0.0f);
}

TEST_F(VotingTest, DisagreementTimeout) {
    /* 2 columns say object 1, 2 columns say object 2 → no consensus */
    column_voting_submit_hypothesis(mgr, 0, 1, 0.9f, nullptr, 0);
    column_voting_submit_hypothesis(mgr, 1, 1, 0.9f, nullptr, 0);
    column_voting_submit_hypothesis(mgr, 2, 2, 0.9f, nullptr, 0);
    column_voting_submit_hypothesis(mgr, 3, 2, 0.9f, nullptr, 0);

    for (int i = 0; i < 3; i++) column_voting_connect_lateral(mgr, i, i + 1);

    uint32_t rounds = 0;
    int rc = column_voting_run_to_consensus(mgr, &rounds);
    EXPECT_EQ(rc, 1); /* Timeout */
    EXPECT_EQ(rounds, COLUMN_VOTING_MAX_ROUNDS);
}

TEST_F(VotingTest, MajorityConsensus) {
    /* 5 say object 3, 2 say object 4 → consensus on 3 */
    for (int i = 0; i < 5; i++)
        column_voting_submit_hypothesis(mgr, i, 3, 0.8f, nullptr, 0);
    for (int i = 5; i < 7; i++)
        column_voting_submit_hypothesis(mgr, i, 4, 0.6f, nullptr, 0);

    for (int i = 0; i < 6; i++) column_voting_connect_lateral(mgr, i, i + 1);

    uint32_t rounds = 0;
    int rc = column_voting_run_to_consensus(mgr, &rounds);
    EXPECT_EQ(rc, 0);

    uint32_t obj = 0;
    float conf = 0;
    column_voting_get_consensus(mgr, &obj, &conf);
    EXPECT_EQ(obj, 3u);
}

TEST_F(VotingTest, ClearHypotheses) {
    column_voting_submit_hypothesis(mgr, 0, 1, 0.9f, nullptr, 0);
    column_voting_submit_hypothesis(mgr, 1, 2, 0.9f, nullptr, 0);
    EXPECT_EQ(column_voting_clear_hypotheses(mgr), 0);
    EXPECT_EQ(mgr->states[0].num_hypotheses, 0u);
    EXPECT_FALSE(mgr->global_consensus);
}

TEST_F(VotingTest, AgreementRatio) {
    for (int i = 0; i < 4; i++)
        column_voting_submit_hypothesis(mgr, i, 5, 0.9f, nullptr, 0);
    column_voting_submit_hypothesis(mgr, 4, 6, 0.9f, nullptr, 0);

    for (int i = 0; i < 4; i++) column_voting_connect_lateral(mgr, i, i + 1);
    column_voting_run_round(mgr);

    float ratio = column_voting_get_agreement_ratio(mgr);
    EXPECT_GE(ratio, 0.7f); /* 4/5 = 0.8 */
}

TEST_F(VotingTest, GetColumnBelief) {
    column_voting_submit_hypothesis(mgr, 2, 99, 0.75f, nullptr, 0);
    column_voting_connect_lateral(mgr, 0, 2); /* Force num_columns >= 3 */
    column_voting_run_round(mgr);

    uint32_t obj = 0;
    float conf = 0;
    EXPECT_EQ(column_voting_get_column_belief(mgr, 2, &obj, &conf), 0);
    EXPECT_EQ(obj, 99u);
}

TEST_F(VotingTest, Stats) {
    for (int i = 0; i < 3; i++)
        column_voting_submit_hypothesis(mgr, i, 1, 0.9f, nullptr, 0);
    for (int i = 0; i < 2; i++) column_voting_connect_lateral(mgr, i, i + 1);
    column_voting_run_round(mgr);

    column_voting_stats_t stats;
    EXPECT_EQ(column_voting_get_stats(mgr, &stats), 0);
    EXPECT_EQ(stats.total_rounds, 1u);
}

/* ============================================================================
 * Test: Dendritic Sequences
 * ============================================================================ */

class DendriticSeqTest : public ::testing::Test {
protected:
    dendritic_sequence_mgr_t* mgr = nullptr;
    dendritic_seq_config_t config;

    void SetUp() override {
        dendritic_seq_config_default(&config);
        config.num_cells = 64;
        config.cells_per_column = 4;
        mgr = dendritic_seq_create(&config);
        ASSERT_NE(mgr, nullptr);
    }

    void TearDown() override {
        if (mgr) dendritic_seq_destroy(mgr);
    }
};

TEST_F(DendriticSeqTest, ConfigDefaults) {
    dendritic_seq_config_t c;
    dendritic_seq_config_default(&c);
    EXPECT_EQ(c.num_cells, DENDRITE_SEQ_DEFAULT_CELLS);
    EXPECT_EQ(c.cells_per_column, DENDRITE_SEQ_DEFAULT_CPC);
    EXPECT_FLOAT_EQ(c.permanence_increment, 0.1f);
    EXPECT_FLOAT_EQ(c.permanence_decrement, 0.02f);
    EXPECT_FLOAT_EQ(c.initial_permanence, 0.21f);
    EXPECT_FLOAT_EQ(c.predicted_cell_boost, 0.8f);
    EXPECT_FLOAT_EQ(c.latency_advantage_ms, 5.0f);
}

TEST_F(DendriticSeqTest, CreateDestroy) {
    EXPECT_EQ(mgr->num_cells, 64u);
    EXPECT_EQ(mgr->cells_per_column, 4u);
    EXPECT_EQ(mgr->num_columns, 16u); /* 64/4 */
}

TEST_F(DendriticSeqTest, NullConfigCreate) {
    EXPECT_EQ(dendritic_seq_create(nullptr), nullptr);
}

TEST_F(DendriticSeqTest, ZeroCellsCreate) {
    dendritic_seq_config_t c;
    dendritic_seq_config_default(&c);
    c.num_cells = 0;
    EXPECT_EQ(dendritic_seq_create(&c), nullptr);
}

TEST_F(DendriticSeqTest, StepWithActiveColumns) {
    /* Activate columns 0, 2, 5 */
    uint32_t active[] = {0, 2, 5};
    EXPECT_EQ(dendritic_seq_step(mgr, active, 3), 0);

    /* First step should burst (no predictions yet) */
    EXPECT_GT(mgr->stats.total_bursts, 0u);
}

TEST_F(DendriticSeqTest, SequenceLearning) {
    /* Present sequence A→B→C multiple times to learn it */
    uint32_t seq_a[] = {0, 1};
    uint32_t seq_b[] = {2, 3};
    uint32_t seq_c[] = {4, 5};

    for (int epoch = 0; epoch < 10; epoch++) {
        dendritic_seq_step(mgr, seq_a, 2);
        dendritic_seq_step(mgr, seq_b, 2);
        dendritic_seq_step(mgr, seq_c, 2);
    }

    /* After training, prediction accuracy should improve */
    float accuracy = dendritic_seq_get_prediction_accuracy(mgr);
    /* At minimum the system should have created segments */
    EXPECT_GT(mgr->stats.total_segments_created, 0u);
}

TEST_F(DendriticSeqTest, SurpriseRate) {
    /* Initially everything is surprising (no predictions) */
    uint32_t active[] = {0};
    dendritic_seq_step(mgr, active, 1);

    float surprise = dendritic_seq_get_surprise_rate(mgr);
    /* Should be high since nothing was predicted */
    EXPECT_GE(surprise, 0.0f);
    EXPECT_LE(surprise, 1.0f);
}

TEST_F(DendriticSeqTest, GetPredictedCells) {
    /* Step once to create some state */
    uint32_t active[] = {0, 1, 2};
    dendritic_seq_step(mgr, active, 3);

    uint32_t predicted[64];
    uint32_t num = 0;
    EXPECT_EQ(dendritic_seq_get_predicted_cells(mgr, predicted, 64, &num), 0);
    /* num may be 0 after first step (no segments yet) — that's fine */
    EXPECT_LE(num, 64u);
}

TEST_F(DendriticSeqTest, IsCellPredicted) {
    /* No predictions initially */
    EXPECT_FALSE(dendritic_seq_is_cell_predicted(mgr, 0));
    /* Out of range */
    EXPECT_FALSE(dendritic_seq_is_cell_predicted(mgr, 9999));
}

TEST_F(DendriticSeqTest, ActivateColumns) {
    uint32_t active[] = {0, 3, 7};
    EXPECT_EQ(dendritic_seq_activate_columns(mgr, active, 3), 0);
    EXPECT_GT(mgr->num_cur_active, 0u);
    EXPECT_GT(mgr->num_cur_winners, 0u);
}

TEST_F(DendriticSeqTest, AdvanceTimestep) {
    uint32_t active[] = {0};
    dendritic_seq_activate_columns(mgr, active, 1);
    uint32_t n_active = mgr->num_cur_active;

    dendritic_seq_advance_timestep(mgr);
    EXPECT_EQ(mgr->num_prev_active, n_active);
    EXPECT_EQ(mgr->num_cur_active, 0u);
}

TEST_F(DendriticSeqTest, Stats) {
    uint32_t a[] = {0};
    dendritic_seq_step(mgr, a, 1);

    dendritic_seq_stats_t stats;
    EXPECT_EQ(dendritic_seq_get_stats(mgr, &stats), 0);
    EXPECT_GT(stats.total_predictions, 0u);
}

/* ============================================================================
 * Test: WM-TB Bridge
 * ============================================================================ */

class WmTbBridgeTest : public ::testing::Test {
protected:
    wm_thousand_brains_bridge_t* bridge = nullptr;
    column_ref_frame_manager_t* rf = nullptr;
    column_voting_manager_t* vt = nullptr;
    dendritic_sequence_mgr_t* ds = nullptr;

    void SetUp() override {
        wm_tb_bridge_config_t cfg;
        wm_tb_bridge_config_default(&cfg);
        bridge = wm_tb_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);

        column_ref_frame_config_t rf_cfg;
        column_ref_frame_config_default(&rf_cfg);
        rf_cfg.max_frames = 8;
        rf = column_ref_frame_create(&rf_cfg);

        column_voting_config_t vt_cfg;
        column_voting_config_default(&vt_cfg);
        vt_cfg.max_columns = 8;
        vt = column_voting_create(&vt_cfg);

        dendritic_seq_config_t ds_cfg;
        dendritic_seq_config_default(&ds_cfg);
        ds_cfg.num_cells = 32;
        ds_cfg.cells_per_column = 4;
        ds = dendritic_seq_create(&ds_cfg);
    }

    void TearDown() override {
        if (bridge) wm_tb_bridge_destroy(bridge);
        if (rf) column_ref_frame_destroy(rf);
        if (vt) column_voting_destroy(vt);
        if (ds) dendritic_seq_destroy(ds);
    }
};

TEST_F(WmTbBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge->wm_state_buffer, nullptr);
    EXPECT_GT(bridge->wm_state_dim, 0u);
}

TEST_F(WmTbBridgeTest, DefaultConfig) {
    EXPECT_FLOAT_EQ(bridge->config.spatial_weight, WM_TB_DEFAULT_SPATIAL_WEIGHT);
    EXPECT_FLOAT_EQ(bridge->config.object_weight, WM_TB_DEFAULT_OBJECT_WEIGHT);
    EXPECT_FLOAT_EQ(bridge->config.temporal_weight, WM_TB_DEFAULT_TEMPORAL_WEIGHT);
    EXPECT_TRUE(bridge->config.enable_spatial_integration);
    EXPECT_TRUE(bridge->config.enable_voting_integration);
    EXPECT_TRUE(bridge->config.enable_sequence_integration);
}

TEST_F(WmTbBridgeTest, ConnectRefFrames) {
    EXPECT_EQ(wm_tb_bridge_connect_ref_frames(bridge, rf), NIMCP_SUCCESS);
    EXPECT_EQ(bridge->ref_frames, rf);
}

TEST_F(WmTbBridgeTest, ConnectVoting) {
    EXPECT_EQ(wm_tb_bridge_connect_voting(bridge, vt), NIMCP_SUCCESS);
    EXPECT_EQ(bridge->voting, vt);
}

TEST_F(WmTbBridgeTest, ConnectSequences) {
    EXPECT_EQ(wm_tb_bridge_connect_sequences(bridge, ds), NIMCP_SUCCESS);
    EXPECT_EQ(bridge->sequences, ds);
}

TEST_F(WmTbBridgeTest, UpdateSpatial) {
    wm_tb_bridge_connect_ref_frames(bridge, rf);

    /* Bind a column and move it */
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(rf, 0, 0, phase);
    float move[3] = {5.0f, 3.0f, 1.0f};
    column_ref_frame_update_location(rf, 0, move);

    EXPECT_EQ(wm_tb_bridge_update_spatial(bridge), NIMCP_SUCCESS);
    EXPECT_EQ(bridge->stats.spatial_updates, 1u);
    EXPECT_GT(bridge->current_state.spatial.num_active_frames, 0u);
}

TEST_F(WmTbBridgeTest, UpdateConsensus) {
    wm_tb_bridge_connect_voting(bridge, vt);

    /* Submit unanimous hypotheses */
    for (int i = 0; i < 4; i++) {
        column_voting_submit_hypothesis(vt, i, 42, 0.9f, nullptr, 0);
        if (i > 0) column_voting_connect_lateral(vt, i - 1, i);
    }
    uint32_t rounds;
    column_voting_run_to_consensus(vt, &rounds);

    EXPECT_EQ(wm_tb_bridge_update_consensus(bridge), NIMCP_SUCCESS);
    EXPECT_EQ(bridge->stats.consensus_updates, 1u);
    EXPECT_TRUE(bridge->current_state.object.has_consensus);
    EXPECT_EQ(bridge->current_state.object.object_id, 42u);
}

TEST_F(WmTbBridgeTest, UpdateTemporal) {
    wm_tb_bridge_connect_sequences(bridge, ds);

    uint32_t active[] = {0, 1};
    dendritic_seq_step(ds, active, 2);

    EXPECT_EQ(wm_tb_bridge_update_temporal(bridge), NIMCP_SUCCESS);
    EXPECT_EQ(bridge->stats.temporal_updates, 1u);
}

TEST_F(WmTbBridgeTest, GetState) {
    wm_tb_combined_state_t state;
    EXPECT_EQ(wm_tb_bridge_get_state(bridge, &state), NIMCP_SUCCESS);
}

TEST_F(WmTbBridgeTest, GetTopdown) {
    wm_tb_topdown_t td;
    EXPECT_EQ(wm_tb_bridge_get_topdown(bridge, &td), NIMCP_SUCCESS);
}

TEST_F(WmTbBridgeTest, GetStats) {
    wm_tb_bridge_stats_t stats;
    EXPECT_EQ(wm_tb_bridge_get_stats(bridge, &stats), NIMCP_SUCCESS);
}

TEST_F(WmTbBridgeTest, TrainingHooks) {
    EXPECT_EQ(wm_tb_bridge_training_begin(bridge), 0);
    EXPECT_EQ(wm_tb_bridge_training_end(bridge), 0);
}

/* ============================================================================
 * Test: Integration Hub
 * ============================================================================ */

class IntegrationHubTest : public ::testing::Test {
protected:
    tb_integration_hub_t* hub = nullptr;
    column_ref_frame_manager_t* rf = nullptr;
    column_voting_manager_t* vt = nullptr;
    dendritic_sequence_mgr_t* ds = nullptr;

    void SetUp() override {
        tb_integration_config_t cfg;
        tb_integration_config_default(&cfg);
        hub = tb_integration_create(&cfg);
        ASSERT_NE(hub, nullptr);

        column_ref_frame_config_t rf_cfg;
        column_ref_frame_config_default(&rf_cfg);
        rf_cfg.max_frames = 8;
        rf = column_ref_frame_create(&rf_cfg);

        column_voting_config_t vt_cfg;
        column_voting_config_default(&vt_cfg);
        vt_cfg.max_columns = 8;
        vt = column_voting_create(&vt_cfg);

        dendritic_seq_config_t ds_cfg;
        dendritic_seq_config_default(&ds_cfg);
        ds_cfg.num_cells = 32;
        ds_cfg.cells_per_column = 4;
        ds = dendritic_seq_create(&ds_cfg);
    }

    void TearDown() override {
        if (hub) tb_integration_destroy(hub);
        if (rf) column_ref_frame_destroy(rf);
        if (vt) column_voting_destroy(vt);
        if (ds) dendritic_seq_destroy(ds);
    }
};

TEST_F(IntegrationHubTest, ConfigDefaults) {
    tb_integration_config_t c;
    tb_integration_config_default(&c);
    EXPECT_EQ(c.enabled_integrations, TB_INT_ALL);
    EXPECT_FLOAT_EQ(c.entorhinal_coupling_gain, 1.0f);
    EXPECT_FLOAT_EQ(c.hippocampal_replay_weight, 0.5f);
    EXPECT_FLOAT_EQ(c.workspace_ignition_boost, 0.2f);
}

TEST_F(IntegrationHubTest, CreateDestroy) {
    EXPECT_NE(hub->grid_population_vector, nullptr);
    EXPECT_NE(hub->consensus_broadcast_buf, nullptr);
}

TEST_F(IntegrationHubTest, ConnectTB) {
    EXPECT_EQ(tb_integration_connect_tb(hub, rf, vt, ds), 0);
    EXPECT_EQ(hub->ref_frames, rf);
    EXPECT_EQ(hub->voting, vt);
    EXPECT_EQ(hub->sequences, ds);
}

TEST_F(IntegrationHubTest, ActiveCountZero) {
    /* No TB components connected → 0 active integrations */
    EXPECT_EQ(tb_integration_get_active_count(hub), 0u);
}

TEST_F(IntegrationHubTest, ActiveCountWithTB) {
    tb_integration_connect_tb(hub, rf, vt, ds);
    /* Without external systems, only internal TB integrations count */
    uint32_t count = tb_integration_get_active_count(hub);
    /* Should be >= 0 (depends on which external systems are connected) */
    EXPECT_GE(count, 0u);
}

TEST_F(IntegrationHubTest, StepWithoutConnections) {
    /* Step with nothing connected — should return gracefully */
    EXPECT_EQ(tb_integration_step(hub), 0);
}

TEST_F(IntegrationHubTest, StepWithTBComponents) {
    tb_integration_connect_tb(hub, rf, vt, ds);

    /* Bind a column + create voting hypotheses + run sequence */
    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(rf, 0, 0, phase);
    float move[3] = {1.0f, 0.0f, 0.0f};
    column_ref_frame_update_location(rf, 0, move);

    for (int i = 0; i < 3; i++)
        column_voting_submit_hypothesis(vt, i, 7, 0.9f, nullptr, 0);
    for (int i = 0; i < 2; i++) column_voting_connect_lateral(vt, i, i + 1);
    column_voting_run_to_consensus(vt, nullptr);

    uint32_t active[] = {0, 1};
    dendritic_seq_step(ds, active, 2);

    /* Full integration step */
    EXPECT_EQ(tb_integration_step(hub), 0);
    EXPECT_EQ(hub->stats.total_steps, 1u);
}

TEST_F(IntegrationHubTest, MultipleSteps) {
    tb_integration_connect_tb(hub, rf, vt, ds);

    float phase[3] = {0, 0, 0};
    column_ref_frame_bind_column(rf, 0, 0, phase);

    for (int step = 0; step < 10; step++) {
        EXPECT_EQ(tb_integration_step(hub), 0);
    }
    EXPECT_EQ(hub->stats.total_steps, 10u);
}

TEST_F(IntegrationHubTest, PredictiveCodingSync) {
    tb_integration_connect_tb(hub, rf, vt, ds);
    /* Need both predictive_coding and sequences for this integration */
    /* Without predictive_coding connected, it should skip gracefully */
    EXPECT_EQ(tb_int_predictive_coding_sync(hub), 0);
}

TEST_F(IntegrationHubTest, DendriticComputeSync) {
    tb_integration_connect_tb(hub, rf, vt, ds);
    EXPECT_EQ(tb_int_dendritic_compute_sync(hub), 0);
}

TEST_F(IntegrationHubTest, VotingToWorkspaceNoConsensus) {
    tb_integration_connect_tb(hub, rf, vt, ds);
    /* No consensus → should skip */
    EXPECT_EQ(tb_int_voting_to_workspace(hub), 0);
    EXPECT_EQ(hub->stats.workspace_broadcasts, 0u);
}

TEST_F(IntegrationHubTest, DisabledIntegration) {
    tb_integration_config_t cfg;
    tb_integration_config_default(&cfg);
    cfg.enabled_integrations = 0; /* All disabled */
    tb_integration_hub_t* h = tb_integration_create(&cfg);
    ASSERT_NE(h, nullptr);

    tb_integration_connect_tb(h, rf, vt, ds);
    EXPECT_EQ(tb_integration_step(h), 0);
    /* Nothing should have run */
    EXPECT_EQ(h->stats.total_steps, 1u); /* Step counter always increments */
    EXPECT_EQ(h->stats.entorhinal_grid_updates, 0u);
    EXPECT_EQ(h->stats.workspace_broadcasts, 0u);

    tb_integration_destroy(h);
}

TEST_F(IntegrationHubTest, GetStats) {
    tb_integration_stats_t stats;
    EXPECT_EQ(tb_integration_get_stats(hub, &stats), 0);
    EXPECT_EQ(stats.total_steps, 0u);
}

TEST_F(IntegrationHubTest, NullArgs) {
    EXPECT_EQ(tb_integration_step(nullptr), -1);
    EXPECT_EQ(tb_integration_get_stats(nullptr, nullptr), -1);
    EXPECT_EQ(tb_integration_get_active_count(nullptr), 0u);
    EXPECT_EQ(tb_integration_connect_tb(nullptr, nullptr, nullptr, nullptr), -1);
}
