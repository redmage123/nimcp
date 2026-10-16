/**
 * @file test_training_convergent_decision.cpp
 * @brief Unit tests for Convergent Training Decision module
 *
 * TEST COVERAGE:
 * - CreateDestroy: lifecycle
 * - DefaultConfig: verify defaults
 * - SubmitSingleEvidence: one contributor
 * - SubmitMultipleEvidence: multiple contributors
 * - ConvergenceDetection: EMA delta falls below threshold
 * - ConsensusAction: majority vote
 * - LrFactorGeometricMean: weighted geometric mean of lr_factors
 * - UrgencyForcePause: high urgency overrides votes
 * - NullSafety: NULL params return errors
 * - ResetClearsState: reset between training steps
 * - PauseVsRollbackPriority: rollback wins when urgency is high
 * - ConfidenceWeighting: high-confidence contributors dominate
 * - AllContinueDecision: all calm -> CONTINUE
 * - MixedEvidence: mixed signals -> convergence resolves
 * - BatchFactorModulation: batch factor correctly composed
 *
 * TOTAL: 15 tests
 *
 * @date 2026-02-26
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "middleware/training/nimcp_training_convergent_decision.h"
}

class TrainingConvergentDecisionTest : public ::testing::Test {
protected:
    training_convergent_session_t* session;
    training_convergent_config_t config;

    void SetUp() override {
        config = training_convergent_default_config();
        session = training_convergent_session_create(&config);
        ASSERT_NE(session, nullptr);
    }

    void TearDown() override {
        if (session) {
            training_convergent_session_destroy(session);
            session = nullptr;
        }
    }

    /** Helper: create a basic evidence struct */
    static training_evidence_t make_evidence(
        const char* source,
        training_evidence_type_t type,
        float lr, float batch, float grad_clip,
        float urgency, float confidence)
    {
        training_evidence_t e;
        memset(&e, 0, sizeof(e));
        e.source_name = source;
        e.type = type;
        e.lr_factor = lr;
        e.batch_factor = batch;
        e.grad_clip_factor = grad_clip;
        e.urgency = urgency;
        e.confidence = confidence;
        return e;
    }
};

/*=============================================================================
 * 1. CreateDestroy — lifecycle
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, CreateDestroy) {
    /* session was created in SetUp, just verify it exists */
    EXPECT_NE(session, nullptr);

    /* Create another and destroy it explicitly */
    training_convergent_session_t* s2 = training_convergent_session_create(nullptr);
    EXPECT_NE(s2, nullptr);
    training_convergent_session_destroy(s2);

    /* Destroy NULL should be safe */
    training_convergent_session_destroy(nullptr);
    SUCCEED();
}

/*=============================================================================
 * 2. DefaultConfig — verify defaults
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, DefaultConfig) {
    training_convergent_config_t cfg = training_convergent_default_config();

    EXPECT_TRUE(cfg.enabled);
    EXPECT_FLOAT_EQ(cfg.convergence_threshold,
                    TRAINING_CONVERGENT_DEFAULT_CONVERGENCE_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.ema_alpha,
                    TRAINING_CONVERGENT_DEFAULT_EMA_ALPHA);
    EXPECT_EQ(cfg.min_submissions,
              TRAINING_CONVERGENT_DEFAULT_MIN_SUBMISSIONS);
    EXPECT_FLOAT_EQ(cfg.pause_urgency_threshold,
                    TRAINING_CONVERGENT_DEFAULT_PAUSE_URGENCY_THRESHOLD);
}

/*=============================================================================
 * 3. SubmitSingleEvidence — one contributor
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, SubmitSingleEvidence) {
    training_evidence_t e = make_evidence("arousal",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.1f, 0.8f);

    int rc = training_convergent_submit_evidence(session, &e);
    EXPECT_EQ(rc, 0);

    /* Compute decision with single contributor */
    training_convergent_decision_t decision;
    rc = training_convergent_compute_decision(session, &decision);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(decision.num_contributors, 1u);
    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_CONTINUE);
}

/*=============================================================================
 * 4. SubmitMultipleEvidence — multiple contributors
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, SubmitMultipleEvidence) {
    training_evidence_t e1 = make_evidence("arousal",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.1f, 0.8f);
    training_evidence_t e2 = make_evidence("instability",
        TRAINING_EVIDENCE_PAUSE, 0.5f, 1.0f, 0.5f, 0.7f, 0.9f);
    training_evidence_t e3 = make_evidence("portia",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.2f, 0.7f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e1), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e2), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e3), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);
    EXPECT_EQ(decision.num_contributors, 3u);
}

/*=============================================================================
 * 5. ConvergenceDetection — EMA delta falls below threshold
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, ConvergenceDetection) {
    /* Submit many similar evidence items to drive EMA delta toward zero.
     * EMA starts at 1.0 to prevent premature convergence, so need ~15+
     * identical submissions for it to decay below threshold 0.01. */
    for (int i = 0; i < 20; i++) {
        training_evidence_t e = make_evidence("module",
            TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.1f, 0.8f);
        EXPECT_EQ(training_convergent_submit_evidence(session, &e), 0);
    }

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    /* With 10 identical confidence=0.8 submissions, EMA delta should converge */
    EXPECT_TRUE(decision.converged);
    EXPECT_LT(decision.ema_delta, config.convergence_threshold);
}

/*=============================================================================
 * 6. ConsensusAction — majority vote
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, ConsensusAction) {
    /* 3 votes for CONTINUE, 1 for PAUSE — CONTINUE should win */
    training_evidence_t e_continue = make_evidence("arousal",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.1f, 0.8f);
    training_evidence_t e_pause = make_evidence("instability",
        TRAINING_EVIDENCE_PAUSE, 0.5f, 1.0f, 0.5f, 0.5f, 0.8f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e_continue), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_continue), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_continue), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_pause), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);
    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_CONTINUE);
    EXPECT_EQ(decision.num_for_continue, 3u);
    EXPECT_EQ(decision.num_for_pause, 1u);
}

/*=============================================================================
 * 7. LrFactorGeometricMean — weighted geometric mean of lr_factors
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, LrFactorGeometricMean) {
    /* Two contributors with equal confidence:
     * lr_factors = 0.5 and 2.0
     * Geometric mean = sqrt(0.5 * 2.0) = 1.0 */
    training_evidence_t e1 = make_evidence("arousal",
        TRAINING_EVIDENCE_LR_MODULATION, 0.5f, 1.0f, 1.0f, 0.1f, 1.0f);
    training_evidence_t e2 = make_evidence("instability",
        TRAINING_EVIDENCE_LR_MODULATION, 2.0f, 1.0f, 1.0f, 0.1f, 1.0f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e1), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e2), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    /* exp((1.0*ln(0.5) + 1.0*ln(2.0)) / (1.0+1.0)) = exp(0) = 1.0 */
    EXPECT_NEAR(decision.lr_factor, 1.0f, 0.01f);
}

/*=============================================================================
 * 8. UrgencyForcePause — high urgency overrides votes
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, UrgencyForcePause) {
    /* All vote CONTINUE but with very high urgency */
    training_evidence_t e = make_evidence("emergency",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.95f, 0.9f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    /* Urgency 0.95 > threshold 0.9, so PAUSE should be forced */
    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_PAUSE);
    EXPECT_GT(decision.urgency, config.pause_urgency_threshold);
}

/*=============================================================================
 * 9. NullSafety — NULL params return errors
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, NullSafety) {
    training_evidence_t e = make_evidence("test",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.0f, 0.5f);
    training_convergent_decision_t decision;

    /* NULL session */
    EXPECT_EQ(training_convergent_submit_evidence(nullptr, &e), -1);
    EXPECT_EQ(training_convergent_compute_decision(nullptr, &decision), -1);
    EXPECT_EQ(training_convergent_session_reset(nullptr), -1);

    /* NULL evidence */
    EXPECT_EQ(training_convergent_submit_evidence(session, nullptr), -1);

    /* NULL decision output */
    EXPECT_EQ(training_convergent_compute_decision(session, nullptr), -1);
}

/*=============================================================================
 * 10. ResetClearsState — reset between training steps
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, ResetClearsState) {
    /* Submit some evidence */
    training_evidence_t e = make_evidence("arousal",
        TRAINING_EVIDENCE_PAUSE, 0.5f, 1.0f, 1.0f, 0.5f, 0.8f);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e), 0);

    /* Reset */
    EXPECT_EQ(training_convergent_session_reset(session), 0);

    /* After reset, decision should have no contributors */
    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);
    EXPECT_EQ(decision.num_contributors, 0u);
    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_CONTINUE);
    EXPECT_FLOAT_EQ(decision.lr_factor, 1.0f);
}

/*=============================================================================
 * 11. PauseVsRollbackPriority — rollback wins when urgency is high
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, PauseVsRollbackPriority) {
    /* 2 votes for ROLLBACK with high confidence, 1 for PAUSE with lower confidence */
    training_evidence_t e_rollback = make_evidence("instability",
        TRAINING_EVIDENCE_ROLLBACK, 0.1f, 1.0f, 0.5f, 0.8f, 0.95f);
    training_evidence_t e_pause = make_evidence("arousal",
        TRAINING_EVIDENCE_PAUSE, 0.3f, 1.0f, 0.5f, 0.5f, 0.5f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e_rollback), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_rollback), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_pause), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    /* Rollback has higher confidence-weighted votes: 2*0.95 = 1.9 vs 1*0.5 = 0.5 */
    EXPECT_EQ(decision.num_for_rollback, 2u);
    EXPECT_EQ(decision.num_for_pause, 1u);
    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_ROLLBACK);
}

/*=============================================================================
 * 12. ConfidenceWeighting — high-confidence contributors dominate
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, ConfidenceWeighting) {
    /* 3 low-confidence votes for CONTINUE (conf=0.1 each = 0.3 total)
     * 1 high-confidence vote for PAUSE (conf=0.95 = 0.95 total)
     * PAUSE should win despite fewer votes */
    training_evidence_t e_continue = make_evidence("module_a",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.0f, 0.1f);
    training_evidence_t e_pause = make_evidence("instability",
        TRAINING_EVIDENCE_PAUSE, 0.3f, 1.0f, 0.5f, 0.5f, 0.95f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e_continue), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_continue), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_continue), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e_pause), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    /* Weighted: CONTINUE = 3*0.1 = 0.3, PAUSE = 1*0.95 = 0.95 */
    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_PAUSE);
}

/*=============================================================================
 * 13. AllContinueDecision — all calm -> CONTINUE
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, AllContinueDecision) {
    /* All modules say training is fine */
    const char* modules[] = {"arousal", "inflammation", "FEP", "instability", "portia"};
    for (int i = 0; i < 5; i++) {
        training_evidence_t e = make_evidence(modules[i],
            TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.05f, 0.85f);
        EXPECT_EQ(training_convergent_submit_evidence(session, &e), 0);
    }

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_CONTINUE);
    EXPECT_EQ(decision.num_for_continue, 5u);
    EXPECT_EQ(decision.num_for_pause, 0u);
    EXPECT_EQ(decision.num_for_rollback, 0u);
    EXPECT_NEAR(decision.lr_factor, 1.0f, 0.01f);
    EXPECT_NEAR(decision.batch_factor, 1.0f, 0.01f);
    EXPECT_LT(decision.urgency, 0.1f);
}

/*=============================================================================
 * 14. MixedEvidence — mixed signals -> convergence resolves
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, MixedEvidence) {
    /* Diverse evidence from multiple modules */
    training_evidence_t e1 = make_evidence("arousal",
        TRAINING_EVIDENCE_LR_MODULATION, 0.8f, 1.0f, 1.0f, 0.3f, 0.7f);
    training_evidence_t e2 = make_evidence("instability",
        TRAINING_EVIDENCE_PAUSE, 0.3f, 1.0f, 0.5f, 0.6f, 0.85f);
    training_evidence_t e3 = make_evidence("FEP",
        TRAINING_EVIDENCE_CHECKPOINT, 1.0f, 1.0f, 1.0f, 0.2f, 0.6f);
    training_evidence_t e4 = make_evidence("portia",
        TRAINING_EVIDENCE_CONTINUE, 1.0f, 1.0f, 1.0f, 0.1f, 0.75f);
    training_evidence_t e5 = make_evidence("emotion",
        TRAINING_EVIDENCE_PAUSE, 0.5f, 1.0f, 0.8f, 0.5f, 0.8f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e1), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e2), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e3), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e4), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e5), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    EXPECT_EQ(decision.num_contributors, 5u);
    /* PAUSE has highest weighted votes: 0.85 + 0.8 = 1.65 */
    EXPECT_EQ(decision.consensus_action, TRAINING_EVIDENCE_PAUSE);
    EXPECT_EQ(decision.num_for_pause, 2u);
    EXPECT_GT(decision.urgency, 0.0f);

    /* LR factor should be pulled below 1.0 due to low lr_factor submissions */
    EXPECT_LT(decision.lr_factor, 1.0f);
}

/*=============================================================================
 * 15. BatchFactorModulation — batch factor correctly composed
 *============================================================================*/

TEST_F(TrainingConvergentDecisionTest, BatchFactorModulation) {
    /* Two contributors with different batch factors, equal confidence:
     * batch_factors = 0.5 and 2.0
     * Geometric mean = sqrt(0.5 * 2.0) = 1.0 */
    training_evidence_t e1 = make_evidence("portia",
        TRAINING_EVIDENCE_BATCH_MODULATION, 1.0f, 0.5f, 1.0f, 0.1f, 0.9f);
    training_evidence_t e2 = make_evidence("BG",
        TRAINING_EVIDENCE_BATCH_MODULATION, 1.0f, 2.0f, 1.0f, 0.1f, 0.9f);

    EXPECT_EQ(training_convergent_submit_evidence(session, &e1), 0);
    EXPECT_EQ(training_convergent_submit_evidence(session, &e2), 0);

    training_convergent_decision_t decision;
    EXPECT_EQ(training_convergent_compute_decision(session, &decision), 0);

    /* Geometric mean with equal weights: exp((ln(0.5)+ln(2.0))/2) = exp(0) = 1.0 */
    EXPECT_NEAR(decision.batch_factor, 1.0f, 0.01f);

    /* Both submitted batch_factor, so grad_clip should be 1.0 (neutral) */
    EXPECT_NEAR(decision.grad_clip_factor, 1.0f, 0.01f);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
