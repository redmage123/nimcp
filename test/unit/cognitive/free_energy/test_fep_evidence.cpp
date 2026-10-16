/**
 * @file test_fep_evidence.cpp
 * @brief Unit tests for FEP Model Evidence computation module
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/free_energy/nimcp_fep_evidence.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class FEPEvidenceTest : public ::testing::Test {
protected:
    fep_evidence_system_t* evidence = nullptr;
    fep_system_t* fep = nullptr;

    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;

    void SetUp() override {
        // Create evidence system
        fep_evidence_config_t config;
        fep_evidence_default_config(&config);
        evidence = fep_evidence_create(&config);

        // Create simple FEP system for testing
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
    }

    void TearDown() override {
        if (evidence) {
            fep_evidence_destroy(evidence);
            evidence = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, CreateDestroy) {
    ASSERT_NE(evidence, nullptr);
}

TEST_F(FEPEvidenceTest, CreateWithNullConfig) {
    fep_evidence_system_t* sys = fep_evidence_create(nullptr);
    ASSERT_NE(sys, nullptr);
    fep_evidence_destroy(sys);
}

TEST_F(FEPEvidenceTest, DestroyNull) {
    fep_evidence_destroy(nullptr);  // Should not crash
}

TEST_F(FEPEvidenceTest, DefaultConfig) {
    fep_evidence_config_t config;
    fep_evidence_default_config(&config);

    EXPECT_EQ(config.method, EVIDENCE_ELBO);
    EXPECT_EQ(config.num_samples, FEP_EVIDENCE_DEFAULT_SAMPLES);
    EXPECT_FLOAT_EQ(config.temperature_schedule_start, FEP_EVIDENCE_DEFAULT_TEMP_START);
    EXPECT_FLOAT_EQ(config.temperature_schedule_end, FEP_EVIDENCE_DEFAULT_TEMP_END);
}

TEST_F(FEPEvidenceTest, DefaultConfigNullPtr) {
    fep_evidence_default_config(nullptr);  // Should not crash
}

/* ============================================================================
 * Evidence Computation Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, ComputeLogEvidence) {
    ASSERT_NE(fep, nullptr);

    float observations[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    fep_evidence_result_t result;

    int ret = fep_compute_log_evidence(evidence, fep, observations, 1, 8, &result);
    EXPECT_EQ(ret, 0);

    // Log evidence should be finite
    EXPECT_TRUE(std::isfinite(result.log_evidence));
    EXPECT_TRUE(std::isfinite(result.evidence_lower_bound));
}

TEST_F(FEPEvidenceTest, ComputeLogEvidenceMultipleObs) {
    ASSERT_NE(fep, nullptr);

    float observations[24] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f,
        0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f
    };
    fep_evidence_result_t result;

    int ret = fep_compute_log_evidence(evidence, fep, observations, 3, 8, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(result.log_evidence));
}

TEST_F(FEPEvidenceTest, ComputeLogEvidenceNullParams) {
    float observations[8] = {0};
    fep_evidence_result_t result;

    EXPECT_EQ(fep_compute_log_evidence(nullptr, fep, observations, 1, 8, &result), -1);
    EXPECT_EQ(fep_compute_log_evidence(evidence, nullptr, observations, 1, 8, &result), -1);
    EXPECT_EQ(fep_compute_log_evidence(evidence, fep, nullptr, 1, 8, &result), -1);
    EXPECT_EQ(fep_compute_log_evidence(evidence, fep, observations, 1, 8, nullptr), -1);
}

TEST_F(FEPEvidenceTest, ComputeELBO) {
    ASSERT_NE(fep, nullptr);

    float observation[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float elbo;

    int ret = fep_compute_elbo(evidence, fep, observation, 8, &elbo);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(elbo));
    // ELBO should be non-positive (upper bounded by 0 for normalized probs)
    EXPECT_LE(elbo, 0.0f);
}

TEST_F(FEPEvidenceTest, ComputeELBONullParams) {
    float observation[8] = {0};
    float elbo;

    EXPECT_EQ(fep_compute_elbo(nullptr, fep, observation, 8, &elbo), -1);
    EXPECT_EQ(fep_compute_elbo(evidence, nullptr, observation, 8, &elbo), -1);
    EXPECT_EQ(fep_compute_elbo(evidence, fep, nullptr, 8, &elbo), -1);
    EXPECT_EQ(fep_compute_elbo(evidence, fep, observation, 8, nullptr), -1);
}

TEST_F(FEPEvidenceTest, ComputeModelComplexity) {
    ASSERT_NE(fep, nullptr);

    float complexity;
    int ret = fep_compute_model_complexity(evidence, fep, &complexity);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(complexity, 0.0f);  // KL divergence is non-negative
    EXPECT_TRUE(std::isfinite(complexity));
}

TEST_F(FEPEvidenceTest, ComputeModelAccuracy) {
    ASSERT_NE(fep, nullptr);

    float observation[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float accuracy;

    int ret = fep_compute_model_accuracy(evidence, fep, observation, 8, &accuracy);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(accuracy));
    EXPECT_LE(accuracy, 0.0f);  // Log likelihood is non-positive
}

/* ============================================================================
 * Model Comparison Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, ComputeBayesFactor) {
    // Create second FEP model
    fep_config_t fep_config2;
    fep_default_config(&fep_config2);
    fep_system_t* fep2 = fep_create(&fep_config2, OBS_DIM, ACTION_DIM);
    ASSERT_NE(fep2, nullptr);

    float observations[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float log_bf;

    int ret = fep_compute_bayes_factor(evidence, fep, fep2, observations, 1, 8, &log_bf);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(log_bf));

    fep_destroy(fep2);
}

TEST_F(FEPEvidenceTest, CompareModels) {
    // Create multiple FEP models
    fep_system_t* models[3];
    fep_config_t configs[3];

    for (int i = 0; i < 3; i++) {
        fep_default_config(&configs[i]);
        models[i] = fep_create(&configs[i], OBS_DIM, ACTION_DIM);
        ASSERT_NE(models[i], nullptr);
    }

    float observations[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    fep_model_score_t scores[3];

    int ret = fep_compare_models(evidence, models, 3, observations, 1, 8, scores);
    EXPECT_EQ(ret, 0);

    // Check posterior probabilities sum to ~1
    float sum = scores[0].posterior_probability +
                scores[1].posterior_probability +
                scores[2].posterior_probability;
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    for (int i = 0; i < 3; i++) {
        fep_destroy(models[i]);
    }
}

TEST_F(FEPEvidenceTest, SelectBestModel) {
    fep_model_score_t scores[3] = {
        {0, -10.0f, 0.2f, 0.33f},
        {1, -5.0f, 0.6f, 0.33f},   // Best
        {2, -8.0f, 0.2f, 0.33f}
    };

    uint32_t best_id;
    int ret = fep_select_best_model(evidence, scores, 3, &best_id);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(best_id, 1u);
}

TEST_F(FEPEvidenceTest, SelectBestModelNullParams) {
    fep_model_score_t scores[3] = {};
    uint32_t best_id;

    EXPECT_EQ(fep_select_best_model(nullptr, scores, 3, &best_id), -1);
    EXPECT_EQ(fep_select_best_model(evidence, nullptr, 3, &best_id), -1);
    EXPECT_EQ(fep_select_best_model(evidence, scores, 3, nullptr), -1);
    EXPECT_EQ(fep_select_best_model(evidence, scores, 0, &best_id), -1);
}

/* ============================================================================
 * Bayes Factor Interpretation Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, InterpretBayesFactorNone) {
    fep_bf_strength_t strength = fep_interpret_bayes_factor(-1.0f);  // BF < 1
    EXPECT_EQ(strength, BF_STRENGTH_NONE);
}

TEST_F(FEPEvidenceTest, InterpretBayesFactorWeak) {
    fep_bf_strength_t strength = fep_interpret_bayes_factor(0.5f);  // BF ~1.6
    EXPECT_EQ(strength, BF_STRENGTH_WEAK);
}

TEST_F(FEPEvidenceTest, InterpretBayesFactorPositive) {
    fep_bf_strength_t strength = fep_interpret_bayes_factor(1.5f);  // BF ~4.5
    EXPECT_EQ(strength, BF_STRENGTH_POSITIVE);
}

TEST_F(FEPEvidenceTest, InterpretBayesFactorStrong) {
    fep_bf_strength_t strength = fep_interpret_bayes_factor(2.5f);  // BF ~12
    EXPECT_EQ(strength, BF_STRENGTH_STRONG);
}

TEST_F(FEPEvidenceTest, InterpretBayesFactorDecisive) {
    fep_bf_strength_t strength = fep_interpret_bayes_factor(5.0f);  // BF ~148
    EXPECT_EQ(strength, BF_STRENGTH_DECISIVE);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, BFStrengthToString) {
    EXPECT_STREQ(fep_bf_strength_to_string(BF_STRENGTH_NONE), "NONE");
    EXPECT_STREQ(fep_bf_strength_to_string(BF_STRENGTH_WEAK), "WEAK");
    EXPECT_STREQ(fep_bf_strength_to_string(BF_STRENGTH_POSITIVE), "POSITIVE");
    EXPECT_STREQ(fep_bf_strength_to_string(BF_STRENGTH_STRONG), "STRONG");
    EXPECT_STREQ(fep_bf_strength_to_string(BF_STRENGTH_VERY_STRONG), "VERY_STRONG");
    EXPECT_STREQ(fep_bf_strength_to_string(BF_STRENGTH_DECISIVE), "DECISIVE");
}

TEST_F(FEPEvidenceTest, EvidenceMethodToString) {
    EXPECT_STREQ(fep_evidence_method_to_string(EVIDENCE_ELBO), "ELBO");
    EXPECT_STREQ(fep_evidence_method_to_string(EVIDENCE_IMPORTANCE), "IMPORTANCE_SAMPLING");
    EXPECT_STREQ(fep_evidence_method_to_string(EVIDENCE_ANNEALED), "ANNEALED_IMPORTANCE");
    EXPECT_STREQ(fep_evidence_method_to_string(EVIDENCE_BRIDGE), "BRIDGE_SAMPLING");
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, Connect) {
    int ret = fep_evidence_connect(evidence, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPEvidenceTest, ConnectNullParams) {
    EXPECT_EQ(fep_evidence_connect(nullptr, fep), -1);
    EXPECT_EQ(fep_evidence_connect(evidence, nullptr), -1);
}

TEST_F(FEPEvidenceTest, SetReference) {
    float observations[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    int ret = fep_evidence_set_reference(evidence, fep, observations, 1, 8);
    EXPECT_EQ(ret, 0);

    // Now computing evidence should give BF relative to reference
    fep_evidence_result_t result;
    ret = fep_compute_log_evidence(evidence, fep, observations, 1, 8, &result);
    EXPECT_EQ(ret, 0);
    // BF against self should be ~1 (log BF ~0)
    EXPECT_NEAR(result.bayes_factor, 1.0f, 0.5f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, GetStats) {
    fep_evidence_stats_t stats;
    int ret = fep_evidence_get_stats(evidence, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_computations, 0u);
}

TEST_F(FEPEvidenceTest, StatsAfterComputation) {
    float observations[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    fep_evidence_result_t result;

    fep_compute_log_evidence(evidence, fep, observations, 1, 8, &result);

    fep_evidence_stats_t stats;
    fep_evidence_get_stats(evidence, &stats);

    EXPECT_GT(stats.total_computations, 0u);
    EXPECT_GT(stats.elbo_computations, 0u);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, BioAsyncConnectDisconnect) {
    EXPECT_FALSE(fep_evidence_is_bio_async_connected(evidence));

    int ret = fep_evidence_connect_bio_async(evidence);
    EXPECT_EQ(ret, 0);

    ret = fep_evidence_disconnect_bio_async(evidence);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(fep_evidence_is_bio_async_connected(evidence));
}

TEST_F(FEPEvidenceTest, BioAsyncDoubleConnect) {
    fep_evidence_connect_bio_async(evidence);
    int ret = fep_evidence_connect_bio_async(evidence);  // Should be no-op
    EXPECT_EQ(ret, 0);
    fep_evidence_disconnect_bio_async(evidence);
}

TEST_F(FEPEvidenceTest, BioAsyncNullParams) {
    EXPECT_EQ(fep_evidence_connect_bio_async(nullptr), -1);
    EXPECT_EQ(fep_evidence_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(fep_evidence_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Decomposition Tests
 * ============================================================================ */

TEST_F(FEPEvidenceTest, EvidenceDecomposition) {
    float observations[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    fep_evidence_result_t result;

    fep_compute_log_evidence(evidence, fep, observations, 1, 8, &result);

    // ELBO = Accuracy - Complexity
    // Both should be finite
    EXPECT_TRUE(std::isfinite(result.model_accuracy));
    EXPECT_TRUE(std::isfinite(result.model_complexity));

    // Complexity should be non-negative (KL divergence)
    EXPECT_GE(result.model_complexity, 0.0f);
}
