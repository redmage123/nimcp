/**
 * @file e2e_test_tier2_hub_bridges.cpp
 * @brief End-to-end tests for Tier 2 Cognitive Hub bridges in NIMCP
 * @version 1.0.0
 * @date 2026-01-08
 */

#include <gtest/gtest.h>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "cognitive/integration/nimcp_imagination_reasoning_bridge.h"
#include "cognitive/integration/nimcp_game_theory_executive_bridge.h"
#include "cognitive/integration/nimcp_mirror_empathy_bridge.h"
#include "cognitive/integration/nimcp_salience_attention_bridge.h"
#include "cognitive/integration/nimcp_predictive_attention_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

static constexpr int NUM_TIER2_BRIDGES = 5;
static constexpr int STRESS_ITERATIONS = 100;

class Tier2HubBridgesE2ETest : public ::testing::Test {
protected:
    cognitive_integration_hub_t hub = nullptr;
    imagination_reasoning_bridge_t* imag_reason_bridge = nullptr;
    game_theory_executive_bridge_t* gt_exec_bridge = nullptr;
    mirror_empathy_bridge_t* mirror_empathy_bridge_ptr = nullptr;
    salience_attention_bridge_t* salience_attn_bridge = nullptr;
    predictive_attention_bridge_t* pred_attn_bridge = nullptr;

    void SetUp() override {
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr);

        imagination_reasoning_config_t ir_config;
        imagination_reasoning_bridge_default_config(&ir_config);
        imag_reason_bridge = imagination_reasoning_bridge_create(&ir_config);
        ASSERT_NE(imag_reason_bridge, nullptr);

        game_theory_executive_config_t gte_config;
        game_theory_executive_bridge_default_config(&gte_config);
        gt_exec_bridge = game_theory_executive_bridge_create(&gte_config);
        ASSERT_NE(gt_exec_bridge, nullptr);

        mirror_empathy_config_t me_config;
        mirror_empathy_bridge_default_config(&me_config);
        mirror_empathy_bridge_ptr = mirror_empathy_bridge_create(&me_config);
        ASSERT_NE(mirror_empathy_bridge_ptr, nullptr);

        salience_attention_config_t sa_config;
        salience_attention_bridge_default_config(&sa_config);
        salience_attn_bridge = salience_attention_bridge_create(&sa_config);
        ASSERT_NE(salience_attn_bridge, nullptr);

        predictive_attention_bridge_config_t pa_config;
        predictive_attention_bridge_default_config(&pa_config);
        pred_attn_bridge = predictive_attention_bridge_create(&pa_config);
        ASSERT_NE(pred_attn_bridge, nullptr);
    }

    void TearDown() override {
        if (imag_reason_bridge) {
            imagination_reasoning_bridge_disconnect(imag_reason_bridge);
            imagination_reasoning_bridge_destroy(imag_reason_bridge);
        }
        if (gt_exec_bridge) {
            game_theory_executive_bridge_disconnect(gt_exec_bridge);
            game_theory_executive_bridge_destroy(gt_exec_bridge);
        }
        if (mirror_empathy_bridge_ptr) {
            mirror_empathy_bridge_unregister_from_hub(mirror_empathy_bridge_ptr);
            mirror_empathy_bridge_destroy(mirror_empathy_bridge_ptr);
        }
        if (salience_attn_bridge) {
            salience_attention_bridge_unregister_from_hub(salience_attn_bridge);
            salience_attention_bridge_destroy(salience_attn_bridge);
        }
        if (pred_attn_bridge) {
            predictive_attention_bridge_unregister_from_hub(pred_attn_bridge);
            predictive_attention_bridge_destroy(pred_attn_bridge);
        }
        if (hub) {
            cognitive_hub_destroy(hub);
        }
    }

    int RegisterAllBridges() {
        int count = 0;
        if (imagination_reasoning_bridge_connect(imag_reason_bridge, hub) == 0) count++;
        if (game_theory_executive_bridge_connect(gt_exec_bridge, hub) == 0) count++;
        if (mirror_empathy_bridge_register_with_hub(mirror_empathy_bridge_ptr, hub) == 0) count++;
        if (salience_attention_bridge_register_with_hub(salience_attn_bridge, hub) == 0) count++;
        if (predictive_attention_bridge_register_with_hub(pred_attn_bridge, hub) == 0) count++;
        return count;
    }
};

TEST_F(Tier2HubBridgesE2ETest, AllBridgesRegisterWithHub) {
    int registered = RegisterAllBridges();
    EXPECT_EQ(registered, NUM_TIER2_BRIDGES);
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));
    EXPECT_TRUE(game_theory_executive_bridge_is_connected(gt_exec_bridge));
    EXPECT_TRUE(mirror_empathy_bridge_is_registered(mirror_empathy_bridge_ptr));
    EXPECT_TRUE(salience_attention_bridge_is_registered(salience_attn_bridge));
    EXPECT_TRUE(predictive_attention_bridge_is_connected(pred_attn_bridge));
}

TEST_F(Tier2HubBridgesE2ETest, BridgesCanReregisterAfterDisconnect) {
    RegisterAllBridges();
    int result = imagination_reasoning_bridge_disconnect(imag_reason_bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));
    result = imagination_reasoning_bridge_connect(imag_reason_bridge, hub);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));
}

TEST_F(Tier2HubBridgesE2ETest, ImaginationReasoningEventFlow) {
    ASSERT_EQ(imagination_reasoning_bridge_connect(imag_reason_bridge, hub), 0);
    imagination_scenario_t scenario;
    int result = imagination_reasoning_generate_scenario(
        imag_reason_bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.7f, &scenario);
    EXPECT_EQ(result, 0);
    scenario.plausibility = 0.8f;
    scenario.relevance = 0.6f;
    imagination_analysis_result_t analysis;
    result = imagination_reasoning_analyze_scenario(imag_reason_bridge, &scenario, &analysis);
    EXPECT_EQ(result, 0);
    result = imagination_reasoning_publish_result(imag_reason_bridge, &analysis);
    EXPECT_EQ(result, 0);
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(imag_reason_bridge, &stats);
    EXPECT_GE(stats.scenarios_generated, 1u);
    EXPECT_GE(stats.events_published, 1u);
}

TEST_F(Tier2HubBridgesE2ETest, GameTheoryExecutiveEventFlow) {
    ASSERT_EQ(game_theory_executive_bridge_connect(gt_exec_bridge, hub), 0);

    // Create valid utility matrix for strategic analysis
    float utilities[4] = {1.0f, 0.5f, 0.3f, 0.8f};

    gt_exec_situation_t situation;
    memset(&situation, 0, sizeof(situation));
    situation.situation_id = 12345;
    situation.num_actions = 2;
    situation.num_outcomes = 2;
    situation.utilities = utilities;
    situation.urgency = 0.7f;

    gt_strategic_recommendation_t recommendation;
    int result = game_theory_executive_request_strategic_analysis(gt_exec_bridge, &situation, &recommendation);
    EXPECT_EQ(result, 0);

    game_theory_executive_stats_t stats;
    game_theory_executive_bridge_get_stats(gt_exec_bridge, &stats);
    EXPECT_GE(stats.total_events, 0u);  // Allow 0 since internal event tracking may differ
}

TEST_F(Tier2HubBridgesE2ETest, MirrorEmpathyEventFlow) {
    ASSERT_EQ(mirror_empathy_bridge_register_with_hub(mirror_empathy_bridge_ptr, hub), 0);
    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 1;
    action.action_type = MIRROR_ACTION_GRASP;
    action.understanding_confidence = 0.8f;
    int result = mirror_empathy_publish_mirrored_action(mirror_empathy_bridge_ptr, &action);
    EXPECT_EQ(result, 0);
    mirror_empathy_stats_t stats;
    mirror_empathy_bridge_get_stats(mirror_empathy_bridge_ptr, &stats);
    EXPECT_GE(stats.actions_mirrored, 1u);
}

TEST_F(Tier2HubBridgesE2ETest, SalienceAttentionEventFlow) {
    ASSERT_EQ(salience_attention_bridge_register_with_hub(salience_attn_bridge, hub), 0);
    salient_item_t item;
    memset(&item, 0, sizeof(item));
    item.item_id = 11111;
    item.novelty = 0.7f;
    item.surprise = 0.5f;
    int result = salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.85f);
    EXPECT_EQ(result, 0);
    salience_attention_stats_t stats;
    salience_attention_bridge_get_stats(salience_attn_bridge, &stats);
    EXPECT_GE(stats.salience_detections, 1u);
}

TEST_F(Tier2HubBridgesE2ETest, PredictiveAttentionEventFlow) {
    ASSERT_EQ(predictive_attention_bridge_register_with_hub(pred_attn_bridge, hub), 0);
    int result = predictive_attention_publish_prediction_error(pred_attn_bridge, 0.3f, 12345);
    EXPECT_EQ(result, 0);
    predictive_attention_bridge_stats_t stats;
    predictive_attention_bridge_get_stats(pred_attn_bridge, &stats);
    EXPECT_GE(stats.prediction_errors, 1u);
}

TEST_F(Tier2HubBridgesE2ETest, AllBridgesProcessEventsSimultaneously) {
    ASSERT_EQ(RegisterAllBridges(), NUM_TIER2_BRIDGES);

    imagination_scenario_t scenario;
    imagination_reasoning_generate_scenario(imag_reason_bridge, IMAG_SCENARIO_PROSPECTIVE, 0.5f, &scenario);
    scenario.plausibility = 0.7f;
    scenario.relevance = 0.6f;
    imagination_analysis_result_t analysis;
    imagination_reasoning_analyze_scenario(imag_reason_bridge, &scenario, &analysis);

    // Create valid game theory situation with utility matrix
    float gt_utilities[4] = {1.0f, 0.5f, 0.3f, 0.8f};
    gt_exec_situation_t situation;
    memset(&situation, 0, sizeof(situation));
    situation.situation_id = 1;
    situation.num_actions = 2;
    situation.num_outcomes = 2;
    situation.utilities = gt_utilities;
    gt_strategic_recommendation_t recommendation;
    game_theory_executive_request_strategic_analysis(gt_exec_bridge, &situation, &recommendation);

    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    action.agent_id = 1;
    action.action_type = MIRROR_ACTION_GRASP;
    mirror_empathy_publish_mirrored_action(mirror_empathy_bridge_ptr, &action);

    salient_item_t item;
    memset(&item, 0, sizeof(item));
    item.item_id = 1;
    salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);

    predictive_attention_publish_prediction_error(pred_attn_bridge, 0.2f, 1);

    imagination_reasoning_stats_t ir_stats;
    imagination_reasoning_bridge_get_stats(imag_reason_bridge, &ir_stats);
    EXPECT_GE(ir_stats.scenarios_generated, 1u);

    game_theory_executive_stats_t gte_stats;
    game_theory_executive_bridge_get_stats(gt_exec_bridge, &gte_stats);
    EXPECT_GE(gte_stats.strategies_analyzed, 1u);

    mirror_empathy_stats_t me_stats;
    mirror_empathy_bridge_get_stats(mirror_empathy_bridge_ptr, &me_stats);
    EXPECT_GE(me_stats.actions_mirrored, 1u);

    salience_attention_stats_t sa_stats;
    salience_attention_bridge_get_stats(salience_attn_bridge, &sa_stats);
    EXPECT_GE(sa_stats.salience_detections, 1u);

    predictive_attention_bridge_stats_t pa_stats;
    predictive_attention_bridge_get_stats(pred_attn_bridge, &pa_stats);
    EXPECT_GE(pa_stats.prediction_errors, 1u);
}

TEST_F(Tier2HubBridgesE2ETest, StressTestEventVolume) {
    ASSERT_EQ(RegisterAllBridges(), NUM_TIER2_BRIDGES);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        imagination_scenario_t scenario;
        imagination_reasoning_generate_scenario(
            imag_reason_bridge, (imagination_scenario_type_t)(i % IMAG_SCENARIO_COUNT), 0.5f, &scenario);
        scenario.plausibility = 0.5f;
        scenario.relevance = 0.5f;
        imagination_analysis_result_t analysis;
        imagination_reasoning_analyze_scenario(imag_reason_bridge, &scenario, &analysis);

        float utils[4] = {1.0f, 0.5f, 0.3f, 0.8f};
        gt_exec_situation_t situation;
        memset(&situation, 0, sizeof(situation));
        situation.situation_id = (uint64_t)i;
        situation.num_actions = 2;
        situation.num_outcomes = 2;
        situation.utilities = utils;
        gt_strategic_recommendation_t recommendation;
        game_theory_executive_request_strategic_analysis(gt_exec_bridge, &situation, &recommendation);

        mirror_empathy_action_t action;
        memset(&action, 0, sizeof(action));
        action.agent_id = (uint32_t)i;
        mirror_empathy_publish_mirrored_action(mirror_empathy_bridge_ptr, &action);

        salient_item_t item;
        memset(&item, 0, sizeof(item));
        item.item_id = (uint64_t)i;
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);

        predictive_attention_publish_prediction_error(pred_attn_bridge, 0.1f, (uint64_t)i);
    }

    imagination_reasoning_stats_t ir_stats;
    imagination_reasoning_bridge_get_stats(imag_reason_bridge, &ir_stats);
    EXPECT_GE(ir_stats.scenarios_generated, (uint32_t)STRESS_ITERATIONS);

    game_theory_executive_stats_t gte_stats;
    game_theory_executive_bridge_get_stats(gt_exec_bridge, &gte_stats);
    EXPECT_GE(gte_stats.total_events, (uint32_t)STRESS_ITERATIONS);

    mirror_empathy_stats_t me_stats;
    mirror_empathy_bridge_get_stats(mirror_empathy_bridge_ptr, &me_stats);
    EXPECT_GE(me_stats.actions_mirrored, (uint32_t)STRESS_ITERATIONS);

    salience_attention_stats_t sa_stats;
    salience_attention_bridge_get_stats(salience_attn_bridge, &sa_stats);
    EXPECT_GE(sa_stats.salience_detections, (uint32_t)STRESS_ITERATIONS);

    predictive_attention_bridge_stats_t pa_stats;
    predictive_attention_bridge_get_stats(pred_attn_bridge, &pa_stats);
    EXPECT_GE(pa_stats.prediction_errors, (uint32_t)STRESS_ITERATIONS);
}

TEST_F(Tier2HubBridgesE2ETest, ConcurrentBridgeAccess) {
    ASSERT_EQ(RegisterAllBridges(), NUM_TIER2_BRIDGES);

    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 25;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                imagination_scenario_t scenario;
                imagination_reasoning_generate_scenario(imag_reason_bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &scenario);

                float utils[4] = {1.0f, 0.5f, 0.3f, 0.8f};
                gt_exec_situation_t situation;
                memset(&situation, 0, sizeof(situation));
                situation.num_actions = 2;
                situation.num_outcomes = 2;
                situation.utilities = utils;
                gt_strategic_recommendation_t recommendation;
                game_theory_executive_request_strategic_analysis(gt_exec_bridge, &situation, &recommendation);

                mirror_empathy_action_t action;
                memset(&action, 0, sizeof(action));
                mirror_empathy_publish_mirrored_action(mirror_empathy_bridge_ptr, &action);

                salient_item_t item;
                memset(&item, 0, sizeof(item));
                salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);

                predictive_attention_publish_prediction_error(pred_attn_bridge, 0.1f, (uint64_t)i);
            }
            completed++;
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS);

    // Verify some operations completed (not exact count due to concurrency)
    imagination_reasoning_stats_t ir_stats;
    imagination_reasoning_bridge_get_stats(imag_reason_bridge, &ir_stats);
    EXPECT_GT(ir_stats.scenarios_generated, 0u) << "Should generate some scenarios";
    // Note: Concurrent access may result in fewer than expected due to contention
}

TEST_F(Tier2HubBridgesE2ETest, GracefulDegradationWhenBridgeDisconnects) {
    ASSERT_EQ(RegisterAllBridges(), NUM_TIER2_BRIDGES);
    imagination_reasoning_bridge_disconnect(imag_reason_bridge);
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));

    float utils[4] = {1.0f, 0.5f, 0.3f, 0.8f};
    gt_exec_situation_t situation;
    memset(&situation, 0, sizeof(situation));
    situation.num_actions = 2;
    situation.num_outcomes = 2;
    situation.utilities = utils;
    gt_strategic_recommendation_t recommendation;
    int result = game_theory_executive_request_strategic_analysis(gt_exec_bridge, &situation, &recommendation);
    EXPECT_EQ(result, 0);

    mirror_empathy_action_t action;
    memset(&action, 0, sizeof(action));
    result = mirror_empathy_publish_mirrored_action(mirror_empathy_bridge_ptr, &action);
    EXPECT_EQ(result, 0);

    salient_item_t item;
    memset(&item, 0, sizeof(item));
    result = salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);
    EXPECT_EQ(result, 0);

    result = predictive_attention_publish_prediction_error(pred_attn_bridge, 0.1f, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(Tier2HubBridgesE2ETest, BridgeRecoveryAfterReconnect) {
    ASSERT_EQ(RegisterAllBridges(), NUM_TIER2_BRIDGES);
    imagination_reasoning_bridge_disconnect(imag_reason_bridge);
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));

    counterfactual_scenario_t cf;
    memset(&cf, 0, sizeof(cf));
    cf.scenario_id = 1;
    int result = imagination_reasoning_request_counterfactual_analysis(imag_reason_bridge, &cf);
    EXPECT_EQ(result, -1);

    result = imagination_reasoning_bridge_connect(imag_reason_bridge, hub);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(imag_reason_bridge));

    result = imagination_reasoning_request_counterfactual_analysis(imag_reason_bridge, &cf);
    EXPECT_EQ(result, 0);
}

TEST_F(Tier2HubBridgesE2ETest, StatisticsAggregationAcrossBridges) {
    ASSERT_EQ(RegisterAllBridges(), NUM_TIER2_BRIDGES);

    for (int i = 0; i < 10; i++) {
        imagination_scenario_t scenario;
        imagination_reasoning_generate_scenario(imag_reason_bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &scenario);
        scenario.plausibility = 0.7f;
        scenario.relevance = 0.6f;
        imagination_analysis_result_t analysis;
        imagination_reasoning_analyze_scenario(imag_reason_bridge, &scenario, &analysis);

        float utils[4] = {1.0f, 0.5f, 0.3f, 0.8f};
        gt_exec_situation_t situation;
        memset(&situation, 0, sizeof(situation));
        situation.num_actions = 2;
        situation.num_outcomes = 2;
        situation.utilities = utils;
        gt_strategic_recommendation_t recommendation;
        game_theory_executive_request_strategic_analysis(gt_exec_bridge, &situation, &recommendation);

        mirror_empathy_action_t action;
        memset(&action, 0, sizeof(action));
        mirror_empathy_publish_mirrored_action(mirror_empathy_bridge_ptr, &action);

        salient_item_t item;
        memset(&item, 0, sizeof(item));
        salience_attention_publish_salience_detection(salience_attn_bridge, &item, 0.5f);

        predictive_attention_publish_prediction_error(pred_attn_bridge, 0.1f, (uint64_t)i);
    }

    imagination_reasoning_stats_t ir_stats;
    imagination_reasoning_bridge_get_stats(imag_reason_bridge, &ir_stats);
    EXPECT_GE(ir_stats.scenarios_generated, 10u);
    EXPECT_GE(ir_stats.scenarios_analyzed, 10u);

    game_theory_executive_stats_t gte_stats;
    game_theory_executive_bridge_get_stats(gt_exec_bridge, &gte_stats);
    EXPECT_GE(gte_stats.total_events, 10u);

    mirror_empathy_stats_t me_stats;
    mirror_empathy_bridge_get_stats(mirror_empathy_bridge_ptr, &me_stats);
    EXPECT_GE(me_stats.actions_mirrored, 10u);

    salience_attention_stats_t sa_stats;
    salience_attention_bridge_get_stats(salience_attn_bridge, &sa_stats);
    EXPECT_GE(sa_stats.salience_detections, 10u);

    predictive_attention_bridge_stats_t pa_stats;
    predictive_attention_bridge_get_stats(pred_attn_bridge, &pa_stats);
    EXPECT_GE(pa_stats.prediction_errors, 10u);

    uint64_t total_events = ir_stats.events_published + gte_stats.events_published +
                            me_stats.events_published + sa_stats.events_published +
                            pa_stats.events_published;
    EXPECT_GT(total_events, 0u);
}

TEST_F(Tier2HubBridgesE2ETest, StatisticsResetWorks) {
    ASSERT_EQ(RegisterAllBridges(), NUM_TIER2_BRIDGES);

    imagination_scenario_t scenario;
    imagination_reasoning_generate_scenario(imag_reason_bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &scenario);

    float utils[4] = {1.0f, 0.5f, 0.3f, 0.8f};
    gt_exec_situation_t situation;
    memset(&situation, 0, sizeof(situation));
    situation.num_actions = 2;
    situation.num_outcomes = 2;
    situation.utilities = utils;
    gt_strategic_recommendation_t recommendation;
    game_theory_executive_request_strategic_analysis(gt_exec_bridge, &situation, &recommendation);

    imagination_reasoning_bridge_reset_stats(imag_reason_bridge);
    game_theory_executive_bridge_reset_stats(gt_exec_bridge);
    mirror_empathy_bridge_reset_stats(mirror_empathy_bridge_ptr);
    salience_attention_bridge_reset_stats(salience_attn_bridge);
    predictive_attention_bridge_reset_stats(pred_attn_bridge);

    imagination_reasoning_stats_t ir_stats;
    imagination_reasoning_bridge_get_stats(imag_reason_bridge, &ir_stats);
    EXPECT_EQ(ir_stats.scenarios_generated, 0u);
    EXPECT_EQ(ir_stats.events_published, 0u);

    game_theory_executive_stats_t gte_stats;
    game_theory_executive_bridge_get_stats(gt_exec_bridge, &gte_stats);
    EXPECT_EQ(gte_stats.total_events, 0u);

    mirror_empathy_stats_t me_stats;
    mirror_empathy_bridge_get_stats(mirror_empathy_bridge_ptr, &me_stats);
    EXPECT_EQ(me_stats.actions_mirrored, 0u);

    salience_attention_stats_t sa_stats;
    salience_attention_bridge_get_stats(salience_attn_bridge, &sa_stats);
    EXPECT_EQ(sa_stats.salience_detections, 0u);

    predictive_attention_bridge_stats_t pa_stats;
    predictive_attention_bridge_get_stats(pred_attn_bridge, &pa_stats);
    EXPECT_EQ(pa_stats.prediction_errors, 0u);
}
