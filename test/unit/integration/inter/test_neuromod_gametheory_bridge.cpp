/**
 * @file test_neuromod_gametheory_bridge.cpp
 * @brief Unit tests for Neuromodulatory-Game Theory Inter-Layer Bridge
 *
 * WHAT: Test suite for neuromod_gametheory_bridge
 * WHY:  Verify correct 5-HT→cooperation, DA→competition/risk, NE→urgency, Hab→loss aversion
 * HOW:  Unit tests for lifecycle, modulation, decision support, strategy classification
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "integration/inter/neuromod_gametheory/nimcp_neuromod_gametheory_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodGameTheoryBridgeTest : public ::testing::Test {
protected:
    neuromod_gametheory_bridge_t* bridge = nullptr;

    void SetUp() override {
        neuromod_gametheory_config_t config = neuromod_gametheory_default_config();
        bridge = neuromod_gametheory_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_gametheory_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(NeuromodGameTheoryCreateTest, CreateWithDefaultConfig) {
    neuromod_gametheory_bridge_t* br = neuromod_gametheory_create(nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(neuromod_gametheory_is_connected(br));
    neuromod_gametheory_destroy(br);
}

TEST(NeuromodGameTheoryCreateTest, CreateWithCustomConfig) {
    neuromod_gametheory_config_t config = neuromod_gametheory_default_config();
    config.ht_cooperation_coupling = 0.9f;
    config.da_competition_coupling = 0.8f;
    config.da_risk_coupling = 0.7f;
    config.enable_strategy_classification = true;

    neuromod_gametheory_bridge_t* br = neuromod_gametheory_create(&config);
    ASSERT_NE(br, nullptr);
    neuromod_gametheory_destroy(br);
}

TEST(NeuromodGameTheoryCreateTest, DestroyNull) {
    neuromod_gametheory_destroy(nullptr);
}

//=============================================================================
// 5-HT Cooperation Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, ApplyHTCooperationLow) {
    float coop;
    int ret = neuromod_gametheory_apply_ht_cooperation(bridge, 0.2f, &coop);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(coop, 0.5f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ApplyHTCooperationHigh) {
    float coop;
    int ret = neuromod_gametheory_apply_ht_cooperation(bridge, 0.9f, &coop);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(coop, 0.5f);
}

//=============================================================================
// DA Competition and Risk Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, ApplyDACompetitionLow) {
    float comp;
    int ret = neuromod_gametheory_apply_da_competition(bridge, 0.2f, &comp);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(comp, 0.5f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ApplyDACompetitionHigh) {
    float comp;
    int ret = neuromod_gametheory_apply_da_competition(bridge, 0.9f, &comp);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(comp, 0.5f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ApplyDARiskLow) {
    float risk;
    int ret = neuromod_gametheory_apply_da_risk(bridge, 0.2f, &risk);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(risk, 0.5f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ApplyDARiskHigh) {
    float risk;
    int ret = neuromod_gametheory_apply_da_risk(bridge, 0.9f, &risk);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(risk, 0.5f);
}

//=============================================================================
// NE Urgency Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, ApplyNEUrgencyLow) {
    float urgency;
    int ret = neuromod_gametheory_apply_ne_urgency(bridge, 0.2f, &urgency);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(urgency, 0.5f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ApplyNEUrgencyHigh) {
    float urgency;
    int ret = neuromod_gametheory_apply_ne_urgency(bridge, 0.9f, &urgency);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(urgency, 0.5f);
}

//=============================================================================
// Habenula Loss Aversion Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, ApplyHabLossAversionLow) {
    float aversion;
    int ret = neuromod_gametheory_apply_hab_loss_aversion(bridge, 0.2f, &aversion);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(aversion, 0.5f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ApplyHabLossAversionHigh) {
    float aversion;
    int ret = neuromod_gametheory_apply_hab_loss_aversion(bridge, 0.9f, &aversion);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(aversion, 0.5f);
}

//=============================================================================
// Decision Support Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, EvaluateFairOffer) {
    /* Set high 5-HT for fairness sensitivity */
    neuromod_gametheory_apply_ht_cooperation(bridge, 0.8f, nullptr);

    float acceptance = neuromod_gametheory_evaluate_offer(bridge, 0.5f);
    EXPECT_GT(acceptance, 0.5f);  /* Fair offer should be accepted */
}

TEST_F(NeuromodGameTheoryBridgeTest, EvaluateUnfairOffer) {
    /* Set high 5-HT for fairness sensitivity */
    neuromod_gametheory_apply_ht_cooperation(bridge, 0.8f, nullptr);

    float acceptance = neuromod_gametheory_evaluate_offer(bridge, 0.1f);
    EXPECT_LT(acceptance, 0.5f);  /* Unfair offer should be rejected */
}

TEST_F(NeuromodGameTheoryBridgeTest, ShouldCooperateWithCooperator) {
    neuromod_gametheory_apply_ht_cooperation(bridge, 0.8f, nullptr);

    /* Opponent has high cooperation history */
    bool cooperate = neuromod_gametheory_should_cooperate(bridge, 0.9f);
    EXPECT_TRUE(cooperate);
}

TEST_F(NeuromodGameTheoryBridgeTest, ShouldNotCooperateWithDefector) {
    neuromod_gametheory_apply_ht_cooperation(bridge, 0.3f, nullptr);
    neuromod_gametheory_apply_da_competition(bridge, 0.8f, nullptr);

    /* Opponent has low cooperation history */
    bool cooperate = neuromod_gametheory_should_cooperate(bridge, 0.1f);
    EXPECT_FALSE(cooperate);
}

TEST_F(NeuromodGameTheoryBridgeTest, ShouldTakeRiskHighDA) {
    neuromod_gametheory_apply_da_risk(bridge, 0.9f, nullptr);
    neuromod_gametheory_apply_hab_loss_aversion(bridge, 0.2f, nullptr);

    /* Positive expected value with moderate variance */
    bool take_risk = neuromod_gametheory_should_take_risk(bridge, 0.3f, 0.3f);
    EXPECT_TRUE(take_risk);
}

TEST_F(NeuromodGameTheoryBridgeTest, ShouldNotTakeRiskHighLossAversion) {
    neuromod_gametheory_apply_da_risk(bridge, 0.3f, nullptr);
    neuromod_gametheory_apply_hab_loss_aversion(bridge, 0.9f, nullptr);

    /* Negative expected value */
    bool take_risk = neuromod_gametheory_should_take_risk(bridge, -0.2f, 0.3f);
    EXPECT_FALSE(take_risk);
}

//=============================================================================
// Strategy Classification Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, ClassifyStrategyCooperative) {
    neuromod_gametheory_compute_modulation(bridge, 0.9f, 0.3f, 0.4f, 0.2f, nullptr);
    gt_strategy_t strategy = neuromod_gametheory_classify_strategy(bridge);
    EXPECT_EQ(strategy, GT_STRATEGY_COOPERATIVE);
}

TEST_F(NeuromodGameTheoryBridgeTest, ClassifyStrategyCompetitive) {
    /* Set low cooperation, high competition directly to avoid risk triggering aggressive */
    neuromod_gametheory_apply_ht_cooperation(bridge, 0.1f, nullptr);  /* Low coop */
    neuromod_gametheory_apply_da_competition(bridge, 0.9f, nullptr);  /* High competition */
    neuromod_gametheory_apply_da_risk(bridge, 0.2f, nullptr);         /* Low risk to avoid aggressive */
    gt_strategy_t strategy = neuromod_gametheory_classify_strategy(bridge);
    EXPECT_EQ(strategy, GT_STRATEGY_COMPETITIVE);
}

TEST_F(NeuromodGameTheoryBridgeTest, ClassifyStrategyCautious) {
    neuromod_gametheory_apply_hab_loss_aversion(bridge, 0.9f, nullptr);
    neuromod_gametheory_apply_da_risk(bridge, 0.2f, nullptr);
    gt_strategy_t strategy = neuromod_gametheory_classify_strategy(bridge);
    EXPECT_EQ(strategy, GT_STRATEGY_CAUTIOUS);
}

TEST_F(NeuromodGameTheoryBridgeTest, ClassifyStrategyUrgent) {
    neuromod_gametheory_apply_ne_urgency(bridge, 0.9f, nullptr);
    gt_strategy_t strategy = neuromod_gametheory_classify_strategy(bridge);
    EXPECT_EQ(strategy, GT_STRATEGY_URGENT);
}

TEST_F(NeuromodGameTheoryBridgeTest, StrategyNameMapping) {
    EXPECT_STREQ(neuromod_gametheory_strategy_name(GT_STRATEGY_BALANCED), "Balanced");
    EXPECT_STREQ(neuromod_gametheory_strategy_name(GT_STRATEGY_COOPERATIVE), "Cooperative");
    EXPECT_STREQ(neuromod_gametheory_strategy_name(GT_STRATEGY_COMPETITIVE), "Competitive");
    EXPECT_STREQ(neuromod_gametheory_strategy_name(GT_STRATEGY_CAUTIOUS), "Cautious");
    EXPECT_STREQ(neuromod_gametheory_strategy_name(GT_STRATEGY_URGENT), "Urgent");
}

//=============================================================================
// Top-Down Feedback Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, ReportFairTreatment) {
    float ht_trigger;
    int ret = neuromod_gametheory_report_fair_treatment(bridge, 0.8f, &ht_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(ht_trigger, 0.0f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ReportGameWon) {
    float vta_trigger;
    int ret = neuromod_gametheory_report_game_won(bridge, 0.7f, &vta_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(vta_trigger, 0.0f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ReportGameLost) {
    float hab_trigger;
    int ret = neuromod_gametheory_report_game_lost(bridge, 0.6f, &hab_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(hab_trigger, 0.0f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ReportBetrayal) {
    float hab_trigger;
    int ret = neuromod_gametheory_report_betrayal(bridge, 0.8f, &hab_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(hab_trigger, 0.0f);

    /* Betrayal should reduce trust */
    neuromod_gametheory_state_t state;
    neuromod_gametheory_get_state(bridge, &state);
    EXPECT_LT(state.trust_level, 0.5f);
}

TEST_F(NeuromodGameTheoryBridgeTest, ReportTimePressure) {
    float lc_trigger;
    int ret = neuromod_gametheory_report_time_pressure(bridge, 0.7f, &lc_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(lc_trigger, 0.0f);
}

//=============================================================================
// Unified Modulation Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, ComputeModulation) {
    neuromod_gametheory_state_t state;
    int ret = neuromod_gametheory_compute_modulation(bridge, 0.5f, 0.5f, 0.5f, 0.3f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.cooperation_tendency, 0.0f);
    EXPECT_LE(state.cooperation_tendency, 1.0f);
    EXPECT_GE(state.strategic_coherence, 0.0f);
    EXPECT_LE(state.strategic_coherence, 1.0f);
}

//=============================================================================
// Update and Stats Tests
//=============================================================================

TEST_F(NeuromodGameTheoryBridgeTest, Update) {
    int ret = neuromod_gametheory_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodGameTheoryBridgeTest, GetStats) {
    neuromod_gametheory_apply_ht_cooperation(bridge, 0.8f, nullptr);
    neuromod_gametheory_should_cooperate(bridge, 0.7f);

    neuromod_gametheory_stats_t stats;
    int ret = neuromod_gametheory_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.cooperation_modulations, 0u);
}

TEST_F(NeuromodGameTheoryBridgeTest, ResetStats) {
    neuromod_gametheory_apply_ht_cooperation(bridge, 0.8f, nullptr);

    int ret = neuromod_gametheory_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    neuromod_gametheory_stats_t stats;
    neuromod_gametheory_get_stats(bridge, &stats);
    EXPECT_EQ(stats.cooperation_modulations, 0u);
}

TEST_F(NeuromodGameTheoryBridgeTest, GetCoherence) {
    float coherence = neuromod_gametheory_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(NeuromodGameTheoryBridgeTest, PrintSummary) {
    neuromod_gametheory_print_summary(bridge);
    neuromod_gametheory_print_summary(nullptr);
}
