/**
 * @file test_interpretability.cpp
 * @brief Unit tests for Interpretability Module
 * @version 1.0.0
 * @date 2026-02-01
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "security/nimcp_interpretability.h"
}

class InterpretabilityTest : public ::testing::Test {
protected:
    interpretability_t* interp = nullptr;

    void SetUp() override { interp = nullptr; }
    void TearDown() override {
        if (interp) { interpretability_destroy(interp); interp = nullptr; }
    }

    interpretability_t* createWithDefaults() {
        interp = interpretability_create(nullptr);
        return interp;
    }

    interp_proposed_action_t makeAction(const char* type, const char* desc) {
        interp_proposed_action_t action;
        memset(&action, 0, sizeof(action));
        strncpy(action.action_type, type, sizeof(action.action_type) - 1);
        strncpy(action.description, desc, sizeof(action.description) - 1);
        action.confidence = 0.8f;
        action.priority = 0.5f;
        return action;
    }
};

TEST_F(InterpretabilityTest, DefaultConfigHasReasonableSettings) {
    interpretability_config_t config = interpretability_default_config();
    EXPECT_GT(config.max_factors_to_extract, 0u);
    EXPECT_GT(config.mc_samples_for_fidelity, 0u);
}

TEST_F(InterpretabilityTest, CreateWithNullConfigUsesDefaults) {
    interp = interpretability_create(nullptr);
    ASSERT_NE(interp, nullptr);
}

TEST_F(InterpretabilityTest, DestroyNullIsNoOp) {
    interpretability_destroy(nullptr);
}

TEST_F(InterpretabilityTest, ExplainDecisionReturnsValidStructure) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);

    interp_proposed_action_t action = makeAction("compute", "Test action");
    interp_decision_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));

    nimcp_error_t err = interpretability_explain_decision(interp, &action, &explanation);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(explanation.factor_count, 0u);
}

TEST_F(InterpretabilityTest, ExplainSummary) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);

    interp_proposed_action_t action = makeAction("compute", "Test action");
    char summary[1024];
    nimcp_error_t err = interpretability_explain_summary(
        interp, &action, summary, sizeof(summary));
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(InterpretabilityTest, ExtractFactors) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);

    interp_proposed_action_t action = makeAction("compute", "Test action");
    decision_factor_t factors[10];
    size_t factor_count = 0;

    nimcp_error_t err = interpretability_extract_factors(
        interp, &action, factors, 10, &factor_count);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(InterpretabilityTest, VerifyFidelity) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);

    interp_proposed_action_t action = makeAction("compute", "Test action");
    interp_decision_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));
    interpretability_explain_decision(interp, &action, &explanation);

    fidelity_result_t result;
    nimcp_error_t err = interpretability_verify_fidelity(
        interp, &explanation, &action, &result);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(result.fidelity_score, 0.0f);
    EXPECT_LE(result.fidelity_score, 1.0f);
}

TEST_F(InterpretabilityTest, GenerateCounterfactual) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);

    interp_proposed_action_t action = makeAction("compute", "Test action");
    counterfactual_query_t query;
    memset(&query, 0, sizeof(query));
    strcpy(query.query, "What if confidence was higher?");

    counterfactual_result_t result;
    nimcp_error_t err = interpretability_counterfactual(
        interp, &action, &query, &result);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(InterpretabilityTest, DecomposeUncertainty) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);

    interp_proposed_action_t action = makeAction("compute", "Test action");
    uncertainty_breakdown_t uncertainty;
    nimcp_error_t err = interpretability_decompose_uncertainty(
        interp, &action, &uncertainty);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(uncertainty.total_uncertainty, 0.0f);
}

TEST_F(InterpretabilityTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);

    interpretability_stats_t stats;
    nimcp_error_t err = interpretability_get_stats(interp, &stats);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.total_explanations_generated, 0u);
}

TEST_F(InterpretabilityTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(interp, nullptr);
    nimcp_error_t err = interpretability_connect_bio_async(interp);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(InterpretabilityTest, NullHandleOperationsReturnErrors) {
    interp_proposed_action_t action;
    memset(&action, 0, sizeof(action));
    interp_decision_explanation_t exp;
    EXPECT_EQ(interpretability_explain_decision(nullptr, &action, &exp),
              NIMCP_ERROR_INVALID_ARGUMENT);
}

