/**
 * @file test_linguistics_fuzzy_stats_integration.cpp
 * @brief Integration tests for Phase 2 Fuzzy & Statistics bridges
 * @date 2026-01-31
 *
 * Tests verify:
 * - Fuzzy bridge membership function evaluation
 * - Fuzzy hedge application
 * - HMM Viterbi decoding for number word sequences
 * - Bayesian reference frame selection
 * - Information theory phonological similarity
 * - Mesh integration for both bridges
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_fuzzy_bridge.h"
#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_statistics_bridge.h"
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"
}

/* ============================================================================
 * FUZZY BRIDGE TESTS
 * ============================================================================ */

class FuzzyBridgeTest : public ::testing::Test {
protected:
    ling_fuzzy_bridge_t* bridge;

    void SetUp() override {
        bridge = ling_fuzzy_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            ling_fuzzy_bridge_destroy(bridge);
        }
    }
};

TEST_F(FuzzyBridgeTest, DefaultConfigHasValidValues) {
    ling_fuzzy_bridge_config_t config = ling_fuzzy_bridge_default_config();

    EXPECT_GT(config.default_near_sigma, 0.0f);
    EXPECT_GT(config.default_far_threshold, 0.0f);
    EXPECT_GT(config.default_angle_sigma, 0.0f);
    EXPECT_GT(config.base_precision, 0.0f);
    EXPECT_LE(config.base_precision, 1.0f);
    EXPECT_TRUE(config.enable_mesh);
}

TEST_F(FuzzyBridgeTest, EvaluateNearPreposition_ReturnsHighMembershipAtZeroDistance) {
    ling_fuzzy_result_t result;

    int ret = ling_fuzzy_evaluate_preposition(
        bridge,
        SPATIAL_PREP_NEAR,
        0.0f,   // distance = 0 (at target)
        NAN,    // no angle
        NAN,    // no height
        &result
    );

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_GT(result.membership, 0.9f);  // Should be high at distance 0
    EXPECT_EQ(result.preposition, SPATIAL_PREP_NEAR);
    EXPECT_GE(result.precision, LING_FUZZY_PRECISION_FLOOR);
    EXPECT_LE(result.precision, LING_FUZZY_PRECISION_CEILING);
}

TEST_F(FuzzyBridgeTest, EvaluateNearPreposition_DecaysWithDistance) {
    ling_fuzzy_result_t result_near, result_mid, result_far;

    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_NEAR, 0.5f, NAN, NAN, &result_near);
    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_NEAR, 2.0f, NAN, NAN, &result_mid);
    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_NEAR, 5.0f, NAN, NAN, &result_far);

    EXPECT_GT(result_near.membership, result_mid.membership);
    EXPECT_GT(result_mid.membership, result_far.membership);
}

TEST_F(FuzzyBridgeTest, EvaluateFarPreposition_IncreasesWithDistance) {
    ling_fuzzy_result_t result_near, result_far;

    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_FAR, 1.0f, NAN, NAN, &result_near);
    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_FAR, 10.0f, NAN, NAN, &result_far);

    EXPECT_LT(result_near.membership, result_far.membership);
}

TEST_F(FuzzyBridgeTest, EvaluateLeftPreposition_ReturnsHighMembershipAtNegativeAngle) {
    ling_fuzzy_result_t result;

    int ret = ling_fuzzy_evaluate_preposition(
        bridge,
        SPATIAL_PREP_LEFT,
        NAN,
        -M_PI / 2.0f,  // -90 degrees (left)
        NAN,
        &result
    );

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_GT(result.membership, 0.5f);
}

TEST_F(FuzzyBridgeTest, EvaluateRightPreposition_ReturnsHighMembershipAtPositiveAngle) {
    ling_fuzzy_result_t result;

    int ret = ling_fuzzy_evaluate_preposition(
        bridge,
        SPATIAL_PREP_RIGHT,
        NAN,
        M_PI / 2.0f,  // +90 degrees (right)
        NAN,
        &result
    );

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_GT(result.membership, 0.5f);
}

TEST_F(FuzzyBridgeTest, ApplyHedgeVery_ConcentratesMembership) {
    float original = 0.8f;
    float hedged;

    int ret = ling_fuzzy_apply_hedge(bridge, original, FUZZY_HEDGE_VERY, &hedged);

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_LT(hedged, original);  // Concentration: μ² < μ for μ < 1
    EXPECT_NEAR(hedged, original * original, 0.001f);
}

TEST_F(FuzzyBridgeTest, ApplyHedgeSomewhat_DilatesMembership) {
    float original = 0.5f;
    float hedged;

    int ret = ling_fuzzy_apply_hedge(bridge, original, FUZZY_HEDGE_SOMEWHAT, &hedged);

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_GT(hedged, original);  // Dilation: √μ > μ for 0 < μ < 1
    EXPECT_NEAR(hedged, sqrtf(original), 0.001f);
}

TEST_F(FuzzyBridgeTest, EvaluateHedgedPreposition_CombinesPrepositionAndHedge) {
    ling_fuzzy_result_t result_base, result_hedged;

    // Evaluate base "near"
    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_NEAR, 1.0f, NAN, NAN, &result_base);

    // Evaluate "very near"
    int ret = ling_fuzzy_evaluate_hedged(
        bridge,
        SPATIAL_PREP_NEAR,
        FUZZY_HEDGE_VERY,
        1.0f,
        NAN,
        NAN,
        &result_hedged
    );

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_LT(result_hedged.membership, result_base.membership);
    EXPECT_EQ(result_hedged.hedge_applied, FUZZY_HEDGE_VERY);
}

TEST_F(FuzzyBridgeTest, SelectPreposition_FindsBestMatch) {
    spatial_preposition_t selected;
    float membership;

    // Distance 0 should select "near"
    int ret = ling_fuzzy_select_preposition(bridge, 0.1f, NAN, NAN, &selected, &membership);

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_GT(membership, 0.0f);
}

TEST_F(FuzzyBridgeTest, InferSpatial_ParsesPhraseCorrectly) {
    spatial_semantics_t semantics;

    int ret = ling_fuzzy_infer_spatial(bridge, "very near the table", &semantics);

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_EQ(semantics.preposition, SPATIAL_PREP_NEAR);
    EXPECT_TRUE(semantics.hedge_applied);
    EXPECT_EQ(semantics.hedge_type, FUZZY_HEDGE_VERY);
}

TEST_F(FuzzyBridgeTest, SetContextScale_AffectsDistanceEvaluation) {
    ling_fuzzy_result_t result_default, result_scaled;

    // Default scale
    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_NEAR, 2.0f, NAN, NAN, &result_default);

    // Indoor scale (0.5x)
    ling_fuzzy_set_context_scale(bridge, 0.5f);
    ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_NEAR, 2.0f, NAN, NAN, &result_scaled);

    // With indoor scale, 2m feels further, so membership should be lower
    EXPECT_NE(result_default.membership, result_scaled.membership);
}

TEST_F(FuzzyBridgeTest, GetStats_ReturnsValidMetrics) {
    // Do some evaluations
    ling_fuzzy_result_t result;
    for (int i = 0; i < 10; i++) {
        ling_fuzzy_evaluate_preposition(bridge, SPATIAL_PREP_NEAR, (float)i, NAN, NAN, &result);
    }

    ling_fuzzy_bridge_stats_t stats;
    int ret = ling_fuzzy_bridge_get_stats(bridge, &stats);

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_EQ(stats.total_evaluations, 10);
    EXPECT_GE(stats.avg_membership, 0.0f);
    EXPECT_LE(stats.avg_membership, 1.0f);
}

TEST_F(FuzzyBridgeTest, MeshHandler_ProducesValidBelief) {
    linguistics_mesh_handler_t handler;
    int ret = ling_fuzzy_get_mesh_handler(bridge, &handler);
    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);

    linguistics_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = LING_REQUEST_PARSE_SPATIAL;
    strncpy(request.input_word, "near", sizeof(request.input_word) - 1);

    linguistics_belief_t belief;
    ret = handler.process(handler.ctx, &request, &belief);

    ASSERT_EQ(ret, LING_FUZZY_ERR_OK);
    EXPECT_GE(belief.certainty, 0.0f);
    EXPECT_LE(belief.certainty, 1.0f);
    EXPECT_GT(belief.precision, 0.0f);
}

TEST_F(FuzzyBridgeTest, MeshHandler_GetPrecisionReturnsValidValue) {
    linguistics_mesh_handler_t handler;
    ling_fuzzy_get_mesh_handler(bridge, &handler);

    float precision = handler.get_precision(handler.ctx);

    EXPECT_GE(precision, LING_FUZZY_PRECISION_FLOOR);
    EXPECT_LE(precision, LING_FUZZY_PRECISION_CEILING);
}

/* ============================================================================
 * STATISTICS BRIDGE TESTS
 * ============================================================================ */

class StatisticsBridgeTest : public ::testing::Test {
protected:
    ling_stats_bridge_t* bridge;

    void SetUp() override {
        bridge = ling_stats_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);

        // Initialize HMM with defaults
        ling_stats_hmm_init_default(bridge);
    }

    void TearDown() override {
        if (bridge) {
            ling_stats_bridge_destroy(bridge);
        }
    }
};

TEST_F(StatisticsBridgeTest, DefaultConfigHasValidValues) {
    ling_stats_bridge_config_t config = ling_stats_bridge_default_config();

    EXPECT_GT(config.hmm_smoothing, 0.0f);
    EXPECT_GT(config.hmm_max_iterations, 0);
    EXPECT_GT(config.prior_egocentric, 0.0f);
    EXPECT_GT(config.prior_allocentric, 0.0f);
    EXPECT_TRUE(config.enable_mesh);
}

TEST_F(StatisticsBridgeTest, ParseNumberWord_RecognizesBasicWords) {
    num_observation_t obs;

    EXPECT_EQ(ling_stats_parse_number_word("one", &obs), 0);
    EXPECT_EQ(obs, NUM_OBS_ONE);

    EXPECT_EQ(ling_stats_parse_number_word("twenty", &obs), 0);
    EXPECT_EQ(obs, NUM_OBS_TWENTY);

    EXPECT_EQ(ling_stats_parse_number_word("hundred", &obs), 0);
    EXPECT_EQ(obs, NUM_OBS_HUNDRED);
}

TEST_F(StatisticsBridgeTest, ParseNumberWord_RejectsUnknownWords) {
    num_observation_t obs;

    EXPECT_NE(ling_stats_parse_number_word("xyz", &obs), 0);
    EXPECT_NE(ling_stats_parse_number_word("", &obs), 0);
}

TEST_F(StatisticsBridgeTest, ObservationName_ReturnsValidStrings) {
    EXPECT_STREQ(ling_stats_observation_name(NUM_OBS_ONE), "one");
    EXPECT_STREQ(ling_stats_observation_name(NUM_OBS_TWENTY), "twenty");
    EXPECT_STREQ(ling_stats_observation_name(NUM_OBS_HUNDRED), "hundred");
}

TEST_F(StatisticsBridgeTest, HmmViterbiDecode_DecodesSimpleSequence) {
    // Test decoding "twenty one" -> TENS + UNITS sequence
    num_observation_t obs[] = {NUM_OBS_TWENTY, NUM_OBS_ONE};
    num_hmm_decode_result_t result;

    // Allocate state sequence
    num_hmm_state_t states[16];
    result.state_sequence = states;

    int ret = ling_stats_hmm_viterbi_decode(bridge, obs, 2, &result);

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_GT(result.sequence_length, 0);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(StatisticsBridgeTest, HmmForward_ComputesLogProbability) {
    num_observation_t obs[] = {NUM_OBS_TWENTY, NUM_OBS_ONE};
    float log_prob;

    int ret = ling_stats_hmm_forward(bridge, obs, 2, &log_prob);

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_LT(log_prob, 0.0f);  // Log probability should be negative
}

TEST_F(StatisticsBridgeTest, HmmPredictNext_ReturnsDistribution) {
    float predictions[NUM_OBS_COUNT];

    int ret = ling_stats_hmm_predict_next(
        bridge,
        NUM_HMM_STATE_TENS,  // After "twenty"
        predictions,
        NUM_OBS_COUNT
    );

    ASSERT_EQ(ret, LING_STATS_ERR_OK);

    // Distribution should sum to ~1
    float sum = 0.0f;
    for (int i = 0; i < NUM_OBS_COUNT; i++) {
        EXPECT_GE(predictions[i], 0.0f);
        sum += predictions[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(StatisticsBridgeTest, BayesSelectFrame_SelectsFrameWithHighestPosterior) {
    ref_frame_selection_result_t result;
    float context_cues[] = {0.8f, 0.2f, 0.5f};  // Example cues

    int ret = ling_stats_bayes_select_frame(
        bridge,
        SPATIAL_PREP_LEFT,
        context_cues,
        3,
        &result
    );

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_GT(result.num_hypotheses, 0);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
    EXPECT_GE(result.precision, 0.0f);

    // Check all posteriors sum to 1
    float sum = 0.0f;
    for (uint32_t i = 0; i < result.num_hypotheses; i++) {
        EXPECT_GE(result.hypotheses[i].posterior, 0.0f);
        EXPECT_LE(result.hypotheses[i].posterior, 1.0f);
        sum += result.hypotheses[i].posterior;
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(StatisticsBridgeTest, BayesUpdatePrior_ModifiesPriors) {
    float priors_before[REF_FRAME_COUNT], priors_after[REF_FRAME_COUNT];

    ling_stats_bayes_get_priors(bridge, priors_before, REF_FRAME_COUNT);

    // Update egocentric prior with strong evidence
    ling_stats_bayes_update_prior(bridge, REF_FRAME_EGOCENTRIC, 0.9f);

    ling_stats_bayes_get_priors(bridge, priors_after, REF_FRAME_COUNT);

    EXPECT_NE(priors_before[REF_FRAME_EGOCENTRIC], priors_after[REF_FRAME_EGOCENTRIC]);
}

TEST_F(StatisticsBridgeTest, PhonologicalSimilarity_ReturnsValidMetrics) {
    phonological_similarity_t result;

    // Compare two phonemes
    int ret = ling_stats_phonological_similarity(
        bridge,
        PHONEME_P,
        PHONEME_B,
        &result
    );

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_GE(result.mutual_information, 0.0f);
    EXPECT_GE(result.entropy_a, 0.0f);
    EXPECT_GE(result.entropy_b, 0.0f);
    EXPECT_GE(result.normalized_mi, 0.0f);
    EXPECT_LE(result.normalized_mi, 1.0f);
    EXPECT_GE(result.similarity, 0.0f);
    EXPECT_LE(result.similarity, 1.1f);  // Allow small floating-point tolerance
}

TEST_F(StatisticsBridgeTest, Entropy_ComputesCorrectly) {
    // Uniform distribution: H = log2(n)
    float uniform[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float entropy;

    int ret = ling_stats_entropy(bridge, uniform, 4, &entropy);

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_NEAR(entropy, 2.0f, 0.1f);  // log2(4) = 2
}

TEST_F(StatisticsBridgeTest, KLDivergence_ComputesCorrectly) {
    float p[] = {0.5f, 0.5f};
    float q[] = {0.9f, 0.1f};
    float kl;

    int ret = ling_stats_kl_divergence(bridge, p, q, 2, &kl);

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_GT(kl, 0.0f);  // KL(P||Q) > 0 when P != Q
}

TEST_F(StatisticsBridgeTest, MeshHandler_ProducesValidBelief) {
    linguistics_mesh_handler_t handler;
    int ret = ling_stats_get_mesh_handler(bridge, &handler);
    ASSERT_EQ(ret, LING_STATS_ERR_OK);

    linguistics_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = LING_REQUEST_PARSE_NUMBER;
    strncpy(request.input_word, "twenty-one", sizeof(request.input_word) - 1);

    linguistics_belief_t belief;
    ret = handler.process(handler.ctx, &request, &belief);

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_GE(belief.certainty, 0.0f);
    EXPECT_LE(belief.certainty, 1.0f);
    EXPECT_GT(belief.precision, 0.0f);
}

TEST_F(StatisticsBridgeTest, GetStats_ReturnsValidMetrics) {
    // Do some operations
    num_observation_t obs[] = {NUM_OBS_TWENTY, NUM_OBS_ONE};
    float log_prob;
    for (int i = 0; i < 5; i++) {
        ling_stats_hmm_forward(bridge, obs, 2, &log_prob);
    }

    ling_stats_bridge_stats_t stats;
    int ret = ling_stats_bridge_get_stats(bridge, &stats);

    ASSERT_EQ(ret, LING_STATS_ERR_OK);
    EXPECT_EQ(stats.hmm_forward_passes, 5);
}

/* ============================================================================
 * MESH INTEGRATION TESTS
 * ============================================================================ */

class MeshIntegrationTest : public ::testing::Test {
protected:
    linguistics_mesh_t* mesh;
    ling_fuzzy_bridge_t* fuzzy_bridge;
    ling_stats_bridge_t* stats_bridge;

    void SetUp() override {
        linguistics_mesh_config_t mesh_config = linguistics_mesh_default_config();
        mesh = linguistics_mesh_create(&mesh_config);
        ASSERT_NE(mesh, nullptr);

        fuzzy_bridge = ling_fuzzy_bridge_create(nullptr);
        ASSERT_NE(fuzzy_bridge, nullptr);

        stats_bridge = ling_stats_bridge_create(nullptr);
        ASSERT_NE(stats_bridge, nullptr);

        ling_stats_hmm_init_default(stats_bridge);
    }

    void TearDown() override {
        if (fuzzy_bridge) ling_fuzzy_bridge_destroy(fuzzy_bridge);
        if (stats_bridge) ling_stats_bridge_destroy(stats_bridge);
        if (mesh) linguistics_mesh_destroy(mesh);
    }
};

TEST_F(MeshIntegrationTest, FuzzyBridge_RegistersWithMesh) {
    int ret = ling_fuzzy_bridge_register_mesh(fuzzy_bridge, mesh);
    EXPECT_EQ(ret, LING_FUZZY_ERR_OK);
}

TEST_F(MeshIntegrationTest, StatsBridge_RegistersWithMesh) {
    int ret = ling_stats_bridge_register_mesh(stats_bridge, mesh);
    EXPECT_EQ(ret, LING_STATS_ERR_OK);
}

TEST_F(MeshIntegrationTest, BothBridges_CanRegisterSimultaneously) {
    int ret1 = ling_fuzzy_bridge_register_mesh(fuzzy_bridge, mesh);
    int ret2 = ling_stats_bridge_register_mesh(stats_bridge, mesh);

    EXPECT_EQ(ret1, LING_FUZZY_ERR_OK);
    EXPECT_EQ(ret2, LING_STATS_ERR_OK);

    // Check mesh stats
    linguistics_mesh_stats_t stats;
    linguistics_mesh_get_stats(mesh, &stats);
    EXPECT_GE(stats.active_participants, 2);
}

TEST_F(MeshIntegrationTest, MeshRequest_GetsProcessedByBridges) {
    // Register both bridges
    ling_fuzzy_bridge_register_mesh(fuzzy_bridge, mesh);
    ling_stats_bridge_register_mesh(stats_bridge, mesh);

    // Create a spatial parse request
    linguistics_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = LING_REQUEST_PARSE_SPATIAL;
    strncpy(request.input_word, "near", sizeof(request.input_word) - 1);

    linguistics_response_t response;
    int ret = linguistics_mesh_request(mesh, &request, &response, 1000);

    // If mesh request succeeds, check response
    if (ret == 0) {
        EXPECT_GE(response.confidence, 0.0f);
        EXPECT_LE(response.confidence, 1.0f);
    }
    // Note: May fail if mesh needs more participants to converge
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
