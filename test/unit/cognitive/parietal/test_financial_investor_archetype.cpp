/**
 * @file test_financial_investor_archetype.cpp
 * @brief Unit tests for NIMCP Financial Investor Archetype module
 *
 * Tests 10 investor archetypes, heuristic evaluation, archetype blending,
 * adaptive selection, emotional modulation, mirror learning,
 * fuzzy decision scoring, and NULL safety.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/parietal/nimcp_financial_investor_archetype.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class FinancialArchetypeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fin_archetype_config_t cfg = financial_investor_archetype_default_config();
        arch = financial_investor_archetype_create(&cfg);
        ASSERT_NE(arch, nullptr);
    }

    void TearDown() override
    {
        if (arch) {
            financial_investor_archetype_destroy(arch);
            arch = nullptr;
        }
    }

    /** Helper: populate a heuristic input with sample data */
    fin_heuristic_input_t make_input()
    {
        fin_heuristic_input_t input{};
        input.current_price = 100.0f;
        input.intrinsic_value = 130.0f;
        input.book_value = 80.0f;
        input.earnings_per_share = 5.0f;
        input.earnings_growth_rate = 0.15f;
        input.dividend_yield = 0.02f;
        input.peg_ratio = 1.2f;
        input.fear_greed_index = 35.0f;
        input.market_consensus_strength = 0.6f;
        input.sector_distance = 0.2f;
        input.market_share_stability = 0.8f;
        input.pricing_power = 0.7f;
        input.switching_cost = 0.6f;
        input.brand_strength = 0.9f;
        input.rsi = 45.0f;
        input.pivot_price = 98.0f;
        input.pivot_tolerance = 2.0f;
        input.breakout_confirmation = 0.7f;
        input.unrealized_profit_pct = 0.10f;
        input.z_score = -0.5f;
        input.management_quality = 0.85f;
        input.rd_effectiveness = 0.7f;
        input.competitive_position = 0.75f;
        for (int i = 0; i < FIN_ARCH_FISHER_CHECKLIST_SIZE; i++) {
            input.fisher_checklist_scores[i] = 0.7f + 0.02f * (float)i;
        }
        input.risk_contribution_count = 3;
        input.risk_contributions[0] = 0.3f;
        input.risk_contributions[1] = 0.4f;
        input.risk_contributions[2] = 0.3f;
        input.leverage_ratio = 1.5f;
        input.position_concentration = 0.15f;
        input.mental_model_count = 4;
        input.mental_model_activations[0] = 0.8f;
        input.mental_model_activations[1] = 0.6f;
        input.mental_model_activations[2] = 0.7f;
        input.mental_model_activations[3] = 0.5f;
        input.price_momentum = 0.3f;
        input.sentiment_divergence = 0.4f;
        input.volume_trend = 0.2f;

        // Market regime: mildly bullish
        input.market_condition.bull_degree = 0.6f;
        input.market_condition.bear_degree = 0.1f;
        input.market_condition.sideways_degree = 0.3f;
        input.market_condition.dominant = FIN_MKT_BULL;

        // Sentiment: fearful (contrarian opportunity)
        input.market_sentiment.fear_degree = 0.6f;
        input.market_sentiment.neutral_degree = 0.2f;
        input.market_sentiment.greed_degree = 0.1f;
        input.market_sentiment.dominant = FIN_MKT_SENTIMENT_FEAR;

        return input;
    }

    financial_investor_archetype_t* arch = nullptr;
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, DefaultConfig)
{
    fin_archetype_config_t cfg = financial_investor_archetype_default_config();
    EXPECT_GT(cfg.max_blend_archetypes, 0u);
    EXPECT_GT(cfg.mirror_learning_rate, 0.0f);
}

TEST_F(FinancialArchetypeTest, CreateWithConfig)
{
    EXPECT_NE(arch, nullptr);
}

TEST_F(FinancialArchetypeTest, DestroyNullSafe)
{
    financial_investor_archetype_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// 2. Archetype Profile Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, GetProfileGraham)
{
    fin_archetype_profile_t profile{};
    int rc = financial_investor_archetype_get_profile(FIN_ARCH_GRAHAM, &profile);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(profile.id, FIN_ARCH_GRAHAM);
    EXPECT_GT(profile.heuristic_count, 0u);
}

TEST_F(FinancialArchetypeTest, GetProfileBuffett)
{
    fin_archetype_profile_t profile{};
    int rc = financial_investor_archetype_get_profile(FIN_ARCH_BUFFETT, &profile);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(profile.id, FIN_ARCH_BUFFETT);
}

TEST_F(FinancialArchetypeTest, GetProfileAllArchetypes)
{
    for (int i = 0; i < FIN_ARCH_COUNT; i++) {
        fin_archetype_profile_t profile{};
        int rc = financial_investor_archetype_get_profile(
            (fin_archetype_id_t)i, &profile);
        EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
        EXPECT_EQ(profile.id, (fin_archetype_id_t)i);
        EXPECT_GT(strlen(profile.name), 0u);
    }
}

TEST_F(FinancialArchetypeTest, ArchetypeName)
{
    const char* name = financial_investor_archetype_name(FIN_ARCH_GRAHAM);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(FinancialArchetypeTest, ArchetypeNameAll)
{
    for (int i = 0; i < FIN_ARCH_COUNT; i++) {
        const char* name = financial_investor_archetype_name(
            (fin_archetype_id_t)i);
        EXPECT_NE(name, nullptr);
    }
}

//=============================================================================
// 3. Heuristic Evaluation Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, EvaluateMarginOfSafety)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_MARGIN_OF_SAFETY, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(result.type, FIN_HEURISTIC_MARGIN_OF_SAFETY);
    // Price 100 vs intrinsic 130 => 23% margin of safety
    EXPECT_GT(result.crisp_score, 0.0f);
    EXPECT_GE(result.fuzzy_membership, 0.0f);
    EXPECT_LE(result.fuzzy_membership, 1.0f);
}

TEST_F(FinancialArchetypeTest, EvaluateEconomicMoat)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_ECONOMIC_MOAT, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_GE(result.fuzzy_membership, 0.0f);
    EXPECT_LE(result.fuzzy_membership, 1.0f);
}

TEST_F(FinancialArchetypeTest, EvaluatePEGRatio)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_PEG_RATIO, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    // PEG of 1.2 is reasonable
    EXPECT_GT(result.crisp_score, 0.0f);
}

TEST_F(FinancialArchetypeTest, EvaluateReflexivity)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_REFLEXIVITY, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, EvaluateMaximumPessimism)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_MAXIMUM_PESSIMISM, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, EvaluateRiskParity)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_RISK_PARITY, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, EvaluateStatisticalEdge)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_STATISTICAL_EDGE, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, EvaluatePivotalPoint)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_PIVOTAL_POINT, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, EvaluateMentalModelConvergence)
{
    fin_heuristic_input_t input = make_input();
    fin_heuristic_result_t result{};

    int rc = financial_investor_archetype_evaluate_heuristic(arch,
        FIN_HEURISTIC_MENTAL_MODEL_CONVERGENCE, &input, &result);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

//=============================================================================
// 4. Single Archetype Evaluation Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, EvaluateGraham)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    int rc = financial_investor_archetype_evaluate(arch,
        FIN_ARCH_GRAHAM, &input, &decision);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(decision.archetype, FIN_ARCH_GRAHAM);
    EXPECT_GE(decision.conviction, 0.0f);
    EXPECT_LE(decision.conviction, 1.0f);
}

TEST_F(FinancialArchetypeTest, EvaluateBuffett)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    int rc = financial_investor_archetype_evaluate(arch,
        FIN_ARCH_BUFFETT, &input, &decision);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(decision.archetype, FIN_ARCH_BUFFETT);
}

TEST_F(FinancialArchetypeTest, EvaluateLynch)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    int rc = financial_investor_archetype_evaluate(arch,
        FIN_ARCH_LYNCH, &input, &decision);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(decision.archetype, FIN_ARCH_LYNCH);
}

TEST_F(FinancialArchetypeTest, EvaluateSoros)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    int rc = financial_investor_archetype_evaluate(arch,
        FIN_ARCH_SOROS, &input, &decision);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(decision.archetype, FIN_ARCH_SOROS);
}

TEST_F(FinancialArchetypeTest, EvaluateSimons)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    int rc = financial_investor_archetype_evaluate(arch,
        FIN_ARCH_SIMONS, &input, &decision);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, EvaluateDalio)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    int rc = financial_investor_archetype_evaluate(arch,
        FIN_ARCH_DALIO, &input, &decision);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

//=============================================================================
// 5. Blending Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, BlendTwoArchetypes)
{
    fin_heuristic_input_t input = make_input();

    fin_archetype_id_t archetypes[] = {FIN_ARCH_GRAHAM, FIN_ARCH_BUFFETT};
    float weights[] = {0.5f, 0.5f};

    fin_blend_result_t blend{};
    int rc = financial_investor_archetype_evaluate_blend(arch,
        archetypes, weights, 2, &input, &blend);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(blend.archetype_count, 2u);
    EXPECT_GE(blend.blended_conviction, 0.0f);
    EXPECT_LE(blend.blended_conviction, 1.0f);
}

TEST_F(FinancialArchetypeTest, BlendThreeArchetypes)
{
    fin_heuristic_input_t input = make_input();

    fin_archetype_id_t archetypes[] = {
        FIN_ARCH_GRAHAM, FIN_ARCH_SOROS, FIN_ARCH_DALIO
    };
    float weights[] = {0.4f, 0.3f, 0.3f};

    fin_blend_result_t blend{};
    int rc = financial_investor_archetype_evaluate_blend(arch,
        archetypes, weights, 3, &input, &blend);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_EQ(blend.archetype_count, 3u);
    EXPECT_GT(blend.blend_entropy, 0.0f);
}

TEST_F(FinancialArchetypeTest, BlendDifferentWeights)
{
    fin_heuristic_input_t input = make_input();

    fin_archetype_id_t archetypes[] = {FIN_ARCH_BUFFETT, FIN_ARCH_SIMONS};
    float weights[] = {0.8f, 0.2f};

    fin_blend_result_t blend{};
    int rc = financial_investor_archetype_evaluate_blend(arch,
        archetypes, weights, 2, &input, &blend);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

//=============================================================================
// 6. Adaptive Selection Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, SelectBullMarket)
{
    fin_fuzzy_market_condition_t market{};
    market.bull_degree = 0.8f;
    market.bear_degree = 0.05f;
    market.sideways_degree = 0.15f;
    market.dominant = FIN_MKT_BULL;

    fin_fuzzy_sentiment_t sentiment{};
    sentiment.greed_degree = 0.6f;
    sentiment.neutral_degree = 0.3f;
    sentiment.dominant = FIN_MKT_SENTIMENT_GREED;

    fin_archetype_suitability_t suit{};
    int rc = financial_investor_archetype_select(arch,
        &market, &sentiment, &suit);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_GE(suit.best_suitability, 0.0f);
    EXPECT_LE(suit.best_suitability, 1.0f);
    EXPECT_GE((int)suit.best_archetype, 0);
    EXPECT_LT((int)suit.best_archetype, FIN_ARCH_COUNT);
}

TEST_F(FinancialArchetypeTest, SelectBearMarket)
{
    fin_fuzzy_market_condition_t market{};
    market.bull_degree = 0.05f;
    market.bear_degree = 0.8f;
    market.crisis_degree = 0.3f;
    market.dominant = FIN_MKT_BEAR;

    fin_fuzzy_sentiment_t sentiment{};
    sentiment.extreme_fear_degree = 0.7f;
    sentiment.fear_degree = 0.2f;
    sentiment.dominant = FIN_MKT_SENTIMENT_EXTREME_FEAR;

    fin_archetype_suitability_t suit{};
    int rc = financial_investor_archetype_select(arch,
        &market, &sentiment, &suit);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    // In bear + extreme fear, contrarian archetypes may rank higher
    EXPECT_GE(suit.selection_confidence, 0.0f);
}

TEST_F(FinancialArchetypeTest, SelectSidewaysMarket)
{
    fin_fuzzy_market_condition_t market{};
    market.sideways_degree = 0.7f;
    market.bull_degree = 0.15f;
    market.bear_degree = 0.15f;
    market.dominant = FIN_MKT_SIDEWAYS;

    fin_fuzzy_sentiment_t sentiment{};
    sentiment.neutral_degree = 0.6f;
    sentiment.dominant = FIN_MKT_SENTIMENT_NEUTRAL;

    fin_archetype_suitability_t suit{};
    int rc = financial_investor_archetype_select(arch,
        &market, &sentiment, &suit);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

//=============================================================================
// 7. Emotional Modulation Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, ComputeEmotion)
{
    fin_fuzzy_market_condition_t market{};
    market.bear_degree = 0.6f;
    market.crisis_degree = 0.3f;
    market.dominant = FIN_MKT_BEAR;

    fin_fuzzy_sentiment_t sentiment{};
    sentiment.fear_degree = 0.7f;
    sentiment.dominant = FIN_MKT_SENTIMENT_FEAR;

    fin_emotional_state_t emotion{};
    int rc = financial_investor_archetype_compute_emotion(arch,
        &market, &sentiment, 0.6f, 0.5f, &emotion);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_GT(emotion.fear_level, 0.0f);
    EXPECT_GE(emotion.fuzzy_position_scale, 0.0f);
    EXPECT_LE(emotion.fuzzy_position_scale, 2.0f);
}

TEST_F(FinancialArchetypeTest, ApplyEmotion)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};
    financial_investor_archetype_evaluate(arch,
        FIN_ARCH_BUFFETT, &input, &decision);

    float original_position = decision.position_size_pct;

    fin_emotional_state_t emotion{};
    emotion.fear_level = 0.8f;
    emotion.stress_level = 0.7f;
    emotion.confidence_level = 0.3f;
    emotion.fuzzy_position_scale = 0.5f;
    emotion.fuzzy_conservatism = 0.8f;

    int rc = financial_investor_archetype_apply_emotion(arch,
        &emotion, &decision);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    // Fear should reduce position size
    EXPECT_LE(decision.position_size_pct, original_position + 0.01f);
}

TEST_F(FinancialArchetypeTest, FearReducesPosition)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};
    financial_investor_archetype_evaluate(arch,
        FIN_ARCH_GRAHAM, &input, &decision);
    float base_size = decision.position_size_pct;

    fin_emotional_state_t fear_state{};
    fear_state.fear_level = 0.9f;
    fear_state.stress_level = 0.8f;
    fear_state.fuzzy_position_scale = 0.3f;
    fear_state.fuzzy_conservatism = 0.9f;

    financial_investor_archetype_apply_emotion(arch, &fear_state, &decision);
    EXPECT_LE(decision.position_size_pct, base_size + 0.01f);
}

TEST_F(FinancialArchetypeTest, StressIncreasesConservatism)
{
    fin_fuzzy_market_condition_t market{};
    market.crisis_degree = 0.8f;
    market.dominant = FIN_MKT_CRISIS;

    fin_fuzzy_sentiment_t sentiment{};
    sentiment.extreme_fear_degree = 0.8f;
    sentiment.dominant = FIN_MKT_SENTIMENT_EXTREME_FEAR;

    fin_emotional_state_t emotion{};
    financial_investor_archetype_compute_emotion(arch,
        &market, &sentiment, 0.9f, 0.8f, &emotion);
    EXPECT_GT(emotion.fuzzy_conservatism, 0.0f);
}

//=============================================================================
// 8. Mirror Learning Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, MirrorRecordCorrect)
{
    fin_mirror_record_t record{};
    record.archetype = FIN_ARCH_BUFFETT;
    record.decision_made = FIN_DECISION_BUY;
    record.outcome_return = 0.15f;
    record.prediction_error = 0.02f;
    record.timestamp_us = 1000000ULL;
    record.was_correct = true;

    int rc = financial_investor_archetype_mirror_record(arch, &record);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, MirrorRecordIncorrect)
{
    fin_mirror_record_t record{};
    record.archetype = FIN_ARCH_SOROS;
    record.decision_made = FIN_DECISION_BUY;
    record.outcome_return = -0.10f;
    record.prediction_error = 0.15f;
    record.timestamp_us = 2000000ULL;
    record.was_correct = false;

    int rc = financial_investor_archetype_mirror_record(arch, &record);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, SelfReflect)
{
    // Record some outcomes first
    for (int i = 0; i < 5; i++) {
        fin_mirror_record_t record{};
        record.archetype = FIN_ARCH_GRAHAM;
        record.decision_made = FIN_DECISION_BUY;
        record.outcome_return = (i % 2 == 0) ? 0.10f : -0.05f;
        record.was_correct = (i % 2 == 0);
        record.timestamp_us = (uint64_t)(i + 1) * 1000000ULL;
        financial_investor_archetype_mirror_record(arch, &record);
    }

    float accuracy = 0.0f, calibration = 0.0f;
    int rc = financial_investor_archetype_self_reflect(arch,
        &accuracy, &calibration);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
    EXPECT_GE(accuracy, 0.0f);
    EXPECT_LE(accuracy, 1.0f);
    EXPECT_GE(calibration, 0.0f);
    EXPECT_LE(calibration, 1.0f);
}

//=============================================================================
// 9. Fuzzy Decision Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, FuzzyDecisionVector)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    financial_investor_archetype_evaluate(arch,
        FIN_ARCH_GRAHAM, &input, &decision);

    // Check 7-term membership vector is valid
    fin_fuzzy_decision_t& fd = decision.fuzzy_decision;
    EXPECT_GE(fd.strong_buy_degree, 0.0f);
    EXPECT_LE(fd.strong_buy_degree, 1.0f);
    EXPECT_GE(fd.buy_degree, 0.0f);
    EXPECT_LE(fd.buy_degree, 1.0f);
    EXPECT_GE(fd.hold_degree, 0.0f);
    EXPECT_LE(fd.hold_degree, 1.0f);
    EXPECT_GE(fd.reduce_degree, 0.0f);
    EXPECT_LE(fd.reduce_degree, 1.0f);
    EXPECT_GE(fd.sell_degree, 0.0f);
    EXPECT_LE(fd.sell_degree, 1.0f);
    EXPECT_GE(fd.strong_sell_degree, 0.0f);
    EXPECT_LE(fd.strong_sell_degree, 1.0f);
    EXPECT_GE(fd.no_action_degree, 0.0f);
    EXPECT_LE(fd.no_action_degree, 1.0f);
}

TEST_F(FinancialArchetypeTest, FuzzyDecisionEntropy)
{
    fin_heuristic_input_t input = make_input();
    fin_archetype_decision_t decision{};

    financial_investor_archetype_evaluate(arch,
        FIN_ARCH_BUFFETT, &input, &decision);

    // Entropy should be non-negative
    EXPECT_GE(decision.fuzzy_decision.decision_entropy, 0.0f);
}

//=============================================================================
// 10. Subsystem Setter Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, SetEthics)
{
    int rc = financial_investor_archetype_set_ethics(arch, nullptr);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, SetLGSS)
{
    int rc = financial_investor_archetype_set_lgss(arch, nullptr);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, SetImmune)
{
    int rc = financial_investor_archetype_set_immune(arch, nullptr);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, SetHealthAgent)
{
    int rc = financial_investor_archetype_set_health_agent(arch, nullptr);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, SetFuzzy)
{
    int rc = financial_investor_archetype_set_fuzzy(arch, nullptr);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

//=============================================================================
// 11. Modulation Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, SetInflammation)
{
    int rc = financial_investor_archetype_set_inflammation(arch, 0.5f);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, SetFatigue)
{
    int rc = financial_investor_archetype_set_fatigue(arch, 0.7f);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

//=============================================================================
// 12. Statistics Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, GetStats)
{
    fin_archetype_stats_t stats{};
    int rc = financial_investor_archetype_get_stats(arch, &stats);
    EXPECT_EQ(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, ResetStats)
{
    financial_investor_archetype_reset_stats(arch);
    fin_archetype_stats_t stats{};
    financial_investor_archetype_get_stats(arch, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(FinancialArchetypeTest, GetLastError)
{
    const char* err = financial_investor_archetype_get_last_error();
    EXPECT_NE(err, nullptr);
}

//=============================================================================
// 13. NULL Safety Tests
//=============================================================================

TEST_F(FinancialArchetypeTest, NullCreate)
{
    financial_investor_archetype_t* a =
        financial_investor_archetype_create(nullptr);
    // Should either return valid with defaults or null
    if (a) financial_investor_archetype_destroy(a);
}

TEST_F(FinancialArchetypeTest, NullGetProfile)
{
    int rc = financial_investor_archetype_get_profile(FIN_ARCH_GRAHAM, nullptr);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, NullEvaluate)
{
    int rc = financial_investor_archetype_evaluate(nullptr,
        FIN_ARCH_GRAHAM, nullptr, nullptr);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, NullEvaluateHeuristic)
{
    int rc = financial_investor_archetype_evaluate_heuristic(nullptr,
        FIN_HEURISTIC_MARGIN_OF_SAFETY, nullptr, nullptr);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, NullBlend)
{
    int rc = financial_investor_archetype_evaluate_blend(nullptr,
        nullptr, nullptr, 0, nullptr, nullptr);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, NullSelect)
{
    int rc = financial_investor_archetype_select(nullptr,
        nullptr, nullptr, nullptr);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, NullMirrorRecord)
{
    int rc = financial_investor_archetype_mirror_record(nullptr, nullptr);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, NullSetInflammation)
{
    int rc = financial_investor_archetype_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

TEST_F(FinancialArchetypeTest, NullGetStats)
{
    int rc = financial_investor_archetype_get_stats(nullptr, nullptr);
    EXPECT_NE(rc, FIN_ARCH_ERR_OK);
}

} // namespace
