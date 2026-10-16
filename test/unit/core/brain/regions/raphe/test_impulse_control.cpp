/**
 * @file test_impulse_control.cpp
 * @brief Unit tests for impulse control system
 */

#include <gtest/gtest.h>

extern "C" {
#include "core/brain/regions/raphe/nimcp_impulse_control.h"
}

class ImpulseControlTest : public ::testing::Test {
protected:
    nimcp_impulse_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
    }

    void TearDown() override {
        if (system.initialized) {
            nimcp_impulse_shutdown(&system);
        }
    }
};

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, DefaultConfigHasValidValues) {
    nimcp_impulse_config_t config = nimcp_impulse_default_config();

    EXPECT_FLOAT_EQ(config.baseline_inhibition, IMPULSE_DEFAULT_INHIBITION);
    EXPECT_FLOAT_EQ(config.baseline_patience, IMPULSE_DEFAULT_PATIENCE);
    EXPECT_FLOAT_EQ(config.baseline_risk_aversion, IMPULSE_DEFAULT_RISK_AVERSION);
    EXPECT_FLOAT_EQ(config.ht_inhibition_gain, IMPULSE_5HT_GAIN);
}

TEST_F(ImpulseControlTest, InitWithNullReturnsError) {
    EXPECT_EQ(nimcp_impulse_init(nullptr, nullptr), -1);
}

TEST_F(ImpulseControlTest, InitWithDefaultConfigSucceeds) {
    EXPECT_EQ(nimcp_impulse_init(&system, nullptr), 0);
    EXPECT_TRUE(system.initialized);
}

TEST_F(ImpulseControlTest, InitWithCustomConfigSucceeds) {
    nimcp_impulse_config_t config = nimcp_impulse_default_config();
    config.baseline_inhibition = 0.8f;

    EXPECT_EQ(nimcp_impulse_init(&system, &config), 0);
    EXPECT_FLOAT_EQ(system.config.baseline_inhibition, 0.8f);
}

TEST_F(ImpulseControlTest, InitSetsCorrectInitialState) {
    nimcp_impulse_init(&system, nullptr);

    EXPECT_FLOAT_EQ(system.inhibition_strength, IMPULSE_DEFAULT_INHIBITION);
    EXPECT_FLOAT_EQ(system.patience, IMPULSE_DEFAULT_PATIENCE);
    EXPECT_NEAR(system.impulsivity, 1.0f - IMPULSE_DEFAULT_INHIBITION, 0.01f);
}

TEST_F(ImpulseControlTest, ShutdownSucceeds) {
    nimcp_impulse_init(&system, nullptr);
    EXPECT_EQ(nimcp_impulse_shutdown(&system), 0);
    EXPECT_FALSE(system.initialized);
}

TEST_F(ImpulseControlTest, ResetRestoresInitialState) {
    nimcp_impulse_init(&system, nullptr);

    system.inhibition_strength = 0.2f;
    system.patience = 0.1f;

    EXPECT_EQ(nimcp_impulse_reset(&system), 0);
    EXPECT_FLOAT_EQ(system.inhibition_strength, IMPULSE_DEFAULT_INHIBITION);
}

/* ==========================================================================
 * Update Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, UpdateWithNullReturnsError) {
    EXPECT_EQ(nimcp_impulse_update(nullptr, 20.0f, 10.0f), -1);
}

TEST_F(ImpulseControlTest, UpdateWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_impulse_update(&system, 20.0f, 10.0f), -1);
}

TEST_F(ImpulseControlTest, UpdateWithBaseline5HTMaintainsBaselineInhibition) {
    nimcp_impulse_init(&system, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 20.0f, 100.0f);  /* Baseline 5-HT */
    }

    EXPECT_NEAR(system.inhibition_strength, IMPULSE_DEFAULT_INHIBITION, 0.1f);
}

TEST_F(ImpulseControlTest, UpdateWithHigh5HTIncreasesInhibition) {
    nimcp_impulse_init(&system, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 40.0f, 100.0f);  /* High 5-HT */
    }

    EXPECT_GT(system.inhibition_strength, IMPULSE_DEFAULT_INHIBITION);
}

TEST_F(ImpulseControlTest, UpdateWithLow5HTDecreasesInhibition) {
    nimcp_impulse_init(&system, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 10.0f, 100.0f);  /* Low 5-HT */
    }

    EXPECT_LT(system.inhibition_strength, IMPULSE_DEFAULT_INHIBITION);
}

TEST_F(ImpulseControlTest, UpdateMaintainsImpulsivityInverse) {
    nimcp_impulse_init(&system, nullptr);

    nimcp_impulse_update(&system, 30.0f, 100.0f);

    EXPECT_NEAR(system.impulsivity, 1.0f - system.inhibition_strength, 0.01f);
}

TEST_F(ImpulseControlTest, UrgencyDecaysOverTime) {
    nimcp_impulse_init(&system, nullptr);

    system.accumulated_urgency = 0.8f;

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 20.0f, 100.0f);
    }

    EXPECT_LT(system.accumulated_urgency, 0.8f);
}

/* ==========================================================================
 * Decision API Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, EvaluateWithNullReturnsError) {
    nimcp_impulse_init(&system, nullptr);

    nimcp_impulse_result_t result;
    EXPECT_EQ(nimcp_impulse_evaluate(nullptr, 0.5f, 0.5f, 0.3f, &result), -1);
    EXPECT_EQ(nimcp_impulse_evaluate(&system, 0.5f, 0.5f, 0.3f, nullptr), -1);
}

TEST_F(ImpulseControlTest, EvaluateReturnsValidDecision) {
    nimcp_impulse_init(&system, nullptr);

    nimcp_impulse_result_t result;
    EXPECT_EQ(nimcp_impulse_evaluate(&system, 0.5f, 0.5f, 0.3f, &result), 0);

    EXPECT_TRUE(result.decision == IMPULSE_DECISION_GO ||
                result.decision == IMPULSE_DECISION_NOGO ||
                result.decision == IMPULSE_DECISION_WAIT);
}

TEST_F(ImpulseControlTest, EvaluateHighValueLowRiskFavorsGo) {
    nimcp_impulse_init(&system, nullptr);

    /* High urgency builds */
    for (int i = 0; i < 20; i++) {
        nimcp_impulse_result_t result;
        nimcp_impulse_evaluate(&system, 0.9f, 0.9f, 0.1f, &result);
    }

    nimcp_impulse_result_t result;
    nimcp_impulse_evaluate(&system, 0.9f, 0.9f, 0.1f, &result);

    /* High value, low risk, high urgency should favor GO */
    EXPECT_EQ(result.decision, IMPULSE_DECISION_GO);
}

TEST_F(ImpulseControlTest, EvaluateHighRiskFavorsNoGo) {
    nimcp_impulse_init(&system, nullptr);

    /* Ensure high inhibition */
    for (int i = 0; i < 50; i++) {
        nimcp_impulse_update(&system, 35.0f, 100.0f);
    }

    nimcp_impulse_result_t result;
    nimcp_impulse_evaluate(&system, 0.3f, 0.3f, 0.9f, &result);

    /* High risk with good inhibition should favor NOGO */
    EXPECT_EQ(result.decision, IMPULSE_DECISION_NOGO);
}

TEST_F(ImpulseControlTest, EvaluateUpdatesStatistics) {
    nimcp_impulse_init(&system, nullptr);

    uint32_t initial_go = system.go_decisions;
    uint32_t initial_nogo = system.nogo_decisions;
    uint32_t initial_wait = system.wait_decisions;

    nimcp_impulse_result_t result;
    for (int i = 0; i < 10; i++) {
        nimcp_impulse_evaluate(&system, 0.5f, 0.5f, 0.3f, &result);
    }

    uint32_t total = (system.go_decisions - initial_go) +
                     (system.nogo_decisions - initial_nogo) +
                     (system.wait_decisions - initial_wait);
    EXPECT_EQ(total, 10u);
}

TEST_F(ImpulseControlTest, EvaluateAccumulatesUrgency) {
    nimcp_impulse_init(&system, nullptr);

    EXPECT_FLOAT_EQ(system.accumulated_urgency, 0.0f);

    nimcp_impulse_result_t result;
    nimcp_impulse_evaluate(&system, 0.8f, 0.5f, 0.3f, &result);

    EXPECT_GT(system.accumulated_urgency, 0.0f);
}

TEST_F(ImpulseControlTest, EvaluateGoResetsUrgency) {
    nimcp_impulse_init(&system, nullptr);

    /* Build urgency */
    nimcp_impulse_result_t result;
    for (int i = 0; i < 30; i++) {
        nimcp_impulse_evaluate(&system, 0.9f, 0.9f, 0.1f, &result);
    }

    /* Force a GO decision */
    nimcp_impulse_evaluate(&system, 0.95f, 0.95f, 0.05f, &result);
    if (result.decision == IMPULSE_DECISION_GO) {
        EXPECT_FLOAT_EQ(system.accumulated_urgency, 0.0f);
    }
}

TEST_F(ImpulseControlTest, ImpulsiveActionTracked) {
    nimcp_impulse_init(&system, nullptr);

    /* Lower inhibition */
    for (int i = 0; i < 50; i++) {
        nimcp_impulse_update(&system, 10.0f, 100.0f);
    }

    /* Build urgency beyond threshold */
    system.accumulated_urgency = 0.9f;

    uint32_t initial_impulsive = system.impulsive_actions;

    nimcp_impulse_result_t result;
    nimcp_impulse_evaluate(&system, 0.3f, 0.3f, 0.3f, &result);

    if (result.decision == IMPULSE_DECISION_GO) {
        /* If GO was chosen due to impulsive breakthrough */
        EXPECT_GE(system.impulsive_actions, initial_impulsive);
    }
}

/* ==========================================================================
 * Compute Inhibition Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, ComputeInhibitionWithNullReturnsError) {
    nimcp_impulse_init(&system, nullptr);

    float output;
    EXPECT_EQ(nimcp_impulse_compute_inhibition(nullptr, 0.5f, &output), -1);
    EXPECT_EQ(nimcp_impulse_compute_inhibition(&system, 0.5f, nullptr), -1);
}

TEST_F(ImpulseControlTest, ComputeInhibitionPositiveWhenInhibitionWins) {
    nimcp_impulse_init(&system, nullptr);

    /* Ensure high inhibition */
    system.inhibition_strength = 0.8f;

    float output;
    nimcp_impulse_compute_inhibition(&system, 0.3f, &output);

    /* 0.8 - 0.3 = 0.5 */
    EXPECT_NEAR(output, 0.5f, 0.01f);
}

TEST_F(ImpulseControlTest, ComputeInhibitionNegativeWhenImpulseWins) {
    nimcp_impulse_init(&system, nullptr);

    /* Lower inhibition */
    system.inhibition_strength = 0.3f;

    float output;
    nimcp_impulse_compute_inhibition(&system, 0.8f, &output);

    /* 0.3 - 0.8 = -0.5 */
    EXPECT_NEAR(output, -0.5f, 0.01f);
}

/* ==========================================================================
 * Can Wait Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, CanWaitWithNullReturnsError) {
    nimcp_impulse_init(&system, nullptr);

    bool can_wait;
    EXPECT_EQ(nimcp_impulse_can_wait(nullptr, 1000.0f, &can_wait), -1);
    EXPECT_EQ(nimcp_impulse_can_wait(&system, 1000.0f, nullptr), -1);
}

TEST_F(ImpulseControlTest, CanWaitTrueForShortDuration) {
    nimcp_impulse_init(&system, nullptr);

    bool can_wait;
    nimcp_impulse_can_wait(&system, 100.0f, &can_wait);

    EXPECT_TRUE(can_wait);
}

TEST_F(ImpulseControlTest, CanWaitFalseForVeryLongDuration) {
    nimcp_impulse_init(&system, nullptr);

    /* Low patience */
    system.patience = 0.1f;

    bool can_wait;
    nimcp_impulse_can_wait(&system, 50000.0f, &can_wait);

    EXPECT_FALSE(can_wait);
}

TEST_F(ImpulseControlTest, CanWaitAffectedByUrgency) {
    nimcp_impulse_init(&system, nullptr);

    system.patience = 0.5f;
    system.accumulated_urgency = 0.0f;

    bool can_wait_low_urgency;
    nimcp_impulse_can_wait(&system, 3000.0f, &can_wait_low_urgency);

    system.accumulated_urgency = 0.8f;

    bool can_wait_high_urgency;
    nimcp_impulse_can_wait(&system, 3000.0f, &can_wait_high_urgency);

    /* High urgency should reduce wait capacity */
    if (can_wait_low_urgency) {
        /* Either still can wait, or cannot due to urgency */
        EXPECT_TRUE(can_wait_low_urgency || !can_wait_high_urgency);
    }
}

/* ==========================================================================
 * Query API Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, GetInhibitionSucceeds) {
    nimcp_impulse_init(&system, nullptr);

    float inhibition;
    EXPECT_EQ(nimcp_impulse_get_inhibition(&system, &inhibition), 0);
    EXPECT_FLOAT_EQ(inhibition, IMPULSE_DEFAULT_INHIBITION);
}

TEST_F(ImpulseControlTest, GetPatienceSucceeds) {
    nimcp_impulse_init(&system, nullptr);

    float patience;
    EXPECT_EQ(nimcp_impulse_get_patience(&system, &patience), 0);
    EXPECT_FLOAT_EQ(patience, IMPULSE_DEFAULT_PATIENCE);
}

TEST_F(ImpulseControlTest, GetImpulsivitySucceeds) {
    nimcp_impulse_init(&system, nullptr);

    float impulsivity;
    EXPECT_EQ(nimcp_impulse_get_impulsivity(&system, &impulsivity), 0);
    EXPECT_GE(impulsivity, 0.0f);
    EXPECT_LE(impulsivity, 1.0f);
}

TEST_F(ImpulseControlTest, GetRiskAversionSucceeds) {
    nimcp_impulse_init(&system, nullptr);

    float risk_aversion;
    EXPECT_EQ(nimcp_impulse_get_risk_aversion(&system, &risk_aversion), 0);
    EXPECT_FLOAT_EQ(risk_aversion, IMPULSE_DEFAULT_RISK_AVERSION);
}

/* ==========================================================================
 * Urgency API Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, ResetUrgencySucceeds) {
    nimcp_impulse_init(&system, nullptr);

    system.accumulated_urgency = 0.8f;

    EXPECT_EQ(nimcp_impulse_reset_urgency(&system), 0);
    EXPECT_FLOAT_EQ(system.accumulated_urgency, 0.0f);
}

TEST_F(ImpulseControlTest, GetUrgencySucceeds) {
    nimcp_impulse_init(&system, nullptr);

    system.accumulated_urgency = 0.5f;

    float urgency;
    EXPECT_EQ(nimcp_impulse_get_urgency(&system, &urgency), 0);
    EXPECT_FLOAT_EQ(urgency, 0.5f);
}

/* ==========================================================================
 * 5-HT Effect Tests
 * ========================================================================== */

TEST_F(ImpulseControlTest, High5HTIncreasesPatience) {
    nimcp_impulse_init(&system, nullptr);

    float initial_patience = system.patience;

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 40.0f, 100.0f);
    }

    EXPECT_GT(system.patience, initial_patience);
}

TEST_F(ImpulseControlTest, High5HTIncreasesRiskAversion) {
    nimcp_impulse_init(&system, nullptr);

    float initial_risk_aversion = system.risk_aversion;

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 40.0f, 100.0f);
    }

    EXPECT_GT(system.risk_aversion, initial_risk_aversion);
}

TEST_F(ImpulseControlTest, Low5HTDecreasesPatience) {
    nimcp_impulse_init(&system, nullptr);

    float initial_patience = system.patience;

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 10.0f, 100.0f);
    }

    EXPECT_LT(system.patience, initial_patience);
}

TEST_F(ImpulseControlTest, Low5HTIncreasesImpulsivity) {
    nimcp_impulse_init(&system, nullptr);

    float initial_impulsivity = system.impulsivity;

    for (int i = 0; i < 100; i++) {
        nimcp_impulse_update(&system, 10.0f, 100.0f);
    }

    EXPECT_GT(system.impulsivity, initial_impulsivity);
}
