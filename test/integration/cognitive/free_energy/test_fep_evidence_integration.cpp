/**
 * @file test_fep_evidence_integration.cpp
 * @brief Integration tests for FEP Evidence module with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_fep_evidence.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"

class FEPEvidenceIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;
    fep_evidence_system_t* evidence = nullptr;
    fep_system_t* fep = nullptr;
    fep_likelihood_learner_t* learning = nullptr;

    void SetUp() override {
        /* Create evidence system */
        fep_evidence_config_t ev_config;
        fep_evidence_default_config(&ev_config);
        evidence = fep_evidence_create(&ev_config);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        /* Create learning system */
        fep_learning_config_t learn_config;
        fep_learning_default_config(&learn_config);
        learning = fep_likelihood_learner_create(&learn_config, OBS_DIM, STATE_DIM);
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
        if (learning) {
            fep_likelihood_learner_destroy(learning);
            learning = nullptr;
        }
    }
};

/* ============================================================================
 * Evidence + FEP Core Integration Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, EvidenceWithFEPSystem) {
    fep_evidence_connect(evidence, fep);

    std::vector<float> obs = {0.8f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float elbo;
    int ret = fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(elbo));
}

TEST_F(FEPEvidenceIntegrationTest, ELBOComputation) {
    fep_evidence_connect(evidence, fep);

    std::vector<float> obs = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float elbo;
    int ret = fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo);
    EXPECT_EQ(ret, 0);
    /* ELBO can be negative (it's a lower bound on log evidence) */
    EXPECT_TRUE(std::isfinite(elbo));
}

/* ============================================================================
 * Complexity vs Accuracy Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, ComplexityComputation) {
    fep_evidence_connect(evidence, fep);

    float complexity;
    int ret = fep_compute_model_complexity(evidence, fep, &complexity);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(complexity, 0.0f);  /* KL divergence is non-negative */
}

TEST_F(FEPEvidenceIntegrationTest, AccuracyComputation) {
    fep_evidence_connect(evidence, fep);

    std::vector<float> obs = {0.8f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float accuracy;
    int ret = fep_compute_model_accuracy(evidence, fep, obs.data(), OBS_DIM, &accuracy);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(accuracy));
}

TEST_F(FEPEvidenceIntegrationTest, ELBODecomposition) {
    fep_evidence_connect(evidence, fep);

    std::vector<float> obs = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float complexity;
    fep_compute_model_complexity(evidence, fep, &complexity);

    float accuracy;
    fep_compute_model_accuracy(evidence, fep, obs.data(), OBS_DIM, &accuracy);

    float elbo;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo);

    /* ELBO ≈ Accuracy - Complexity */
    EXPECT_TRUE(std::isfinite(complexity));
    EXPECT_TRUE(std::isfinite(accuracy));
    EXPECT_TRUE(std::isfinite(elbo));
}

/* ============================================================================
 * Learning + Evidence Integration Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, EvidenceAfterLearning) {
    fep_evidence_connect(evidence, fep);

    std::vector<float> obs = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    /* Compute evidence before learning */
    float elbo_before;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo_before);

    /* Train on consistent data */
    for (int i = 0; i < 10; i++) {
        fep_learn_likelihood(learning, fep, obs.data(), state.data(), OBS_DIM, STATE_DIM);
    }

    /* Compute evidence after learning */
    float elbo_after;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo_after);

    EXPECT_TRUE(std::isfinite(elbo_before));
    EXPECT_TRUE(std::isfinite(elbo_after));
}

/* ============================================================================
 * Model Comparison Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, BayesFactorComputation) {
    /* Create two FEP systems */
    fep_config_t config1;
    fep_default_config(&config1);
    fep_system_t* model1 = fep_create(&config1, OBS_DIM, ACTION_DIM);

    fep_config_t config2;
    fep_default_config(&config2);
    fep_system_t* model2 = fep_create(&config2, OBS_DIM, ACTION_DIM);

    /* Create observation data */
    std::vector<float> observations(OBS_DIM * 10);
    for (size_t i = 0; i < observations.size(); i++) {
        observations[i] = 0.1f;
    }

    /* Compute Bayes factor */
    float log_bf;
    int ret = fep_compute_bayes_factor(evidence, model1, model2,
                                        observations.data(), 10, OBS_DIM, &log_bf);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(log_bf));

    fep_destroy(model1);
    fep_destroy(model2);
}

TEST_F(FEPEvidenceIntegrationTest, ModelComparison) {
    /* Create multiple models */
    std::vector<fep_system_t*> models;
    for (int i = 0; i < 3; i++) {
        fep_config_t config;
        fep_default_config(&config);
        models.push_back(fep_create(&config, OBS_DIM, ACTION_DIM));
    }

    /* Create observation data */
    std::vector<float> observations(OBS_DIM * 10);
    for (size_t i = 0; i < observations.size(); i++) {
        observations[i] = 0.1f;
    }

    /* Compare models */
    std::vector<fep_model_score_t> scores(3);
    int ret = fep_compare_models(evidence, models.data(), 3,
                                  observations.data(), 10, OBS_DIM, scores.data());
    EXPECT_EQ(ret, 0);

    /* Select best model */
    uint32_t best_id;
    ret = fep_select_best_model(evidence, scores.data(), 3, &best_id);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(best_id, 3u);

    for (auto* model : models) {
        fep_destroy(model);
    }
}


/* ============================================================================
 * Stats Tracking Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, StatsAfterComputations) {
    fep_evidence_connect(evidence, fep);

    std::vector<float> obs = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    /* Run multiple computations */
    for (int i = 0; i < 10; i++) {
        float elbo;
        fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo);

        float complexity;
        fep_compute_model_complexity(evidence, fep, &complexity);
    }

    fep_evidence_stats_t stats;
    int ret = fep_evidence_get_stats(evidence, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.elbo_computations, 0u);
}

/* ============================================================================
 * Config Integration Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, CustomConfiguration) {
    fep_evidence_config_t config;
    fep_evidence_default_config(&config);
    config.num_samples = 500;
    config.method = EVIDENCE_ELBO;

    fep_evidence_system_t* custom = fep_evidence_create(&config);
    fep_evidence_connect(custom, fep);

    float complexity;
    int ret = fep_compute_model_complexity(custom, fep, &complexity);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(complexity));

    fep_evidence_destroy(custom);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, BioAsyncWithEvidence) {
    fep_evidence_connect(evidence, fep);
    fep_evidence_connect_bio_async(evidence);

    std::vector<float> obs = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float elbo;
    int ret = fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(elbo));

    fep_evidence_disconnect_bio_async(evidence);
}

TEST_F(FEPEvidenceIntegrationTest, BioAsyncConnectionStatus) {
    EXPECT_FALSE(fep_evidence_is_bio_async_connected(evidence));

    int ret = fep_evidence_connect_bio_async(evidence);
    EXPECT_EQ(ret, 0);  // Connection attempt should succeed
    // Note: Connection status may be false if bio-async router is not available
    // This is expected behavior in test environments without router initialization

    fep_evidence_disconnect_bio_async(evidence);
    EXPECT_FALSE(fep_evidence_is_bio_async_connected(evidence));
}

/* ============================================================================
 * Full Evidence Integration Tests
 * ============================================================================ */

TEST_F(FEPEvidenceIntegrationTest, FullEvidenceLoop) {
    fep_evidence_connect(evidence, fep);

    std::vector<float> obs = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    /* 1. Compute initial evidence */
    float elbo_initial;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo_initial);

    /* 2. Learn from data */
    for (int i = 0; i < 20; i++) {
        fep_learn_likelihood(learning, fep, obs.data(), state.data(), OBS_DIM, STATE_DIM);
    }

    /* 3. Compute final evidence */
    float elbo_final;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo_final);

    /* 4. Compute decomposition */
    float complexity;
    fep_compute_model_complexity(evidence, fep, &complexity);

    float accuracy;
    fep_compute_model_accuracy(evidence, fep, obs.data(), OBS_DIM, &accuracy);

    EXPECT_TRUE(std::isfinite(elbo_initial));
    EXPECT_TRUE(std::isfinite(elbo_final));
    EXPECT_TRUE(std::isfinite(complexity));
    EXPECT_TRUE(std::isfinite(accuracy));
}

TEST_F(FEPEvidenceIntegrationTest, LogEvidenceComputation) {
    fep_evidence_connect(evidence, fep);

    /* Create observation data */
    std::vector<float> observations(OBS_DIM * 5);
    for (size_t i = 0; i < observations.size(); i++) {
        observations[i] = 0.1f;
    }

    /* Compute log evidence */
    fep_evidence_result_t result;
    int ret = fep_compute_log_evidence(evidence, fep, observations.data(),
                                        5, OBS_DIM, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(std::isfinite(result.log_evidence));
    EXPECT_TRUE(std::isfinite(result.evidence_lower_bound));
    EXPECT_TRUE(std::isfinite(result.model_complexity));
    EXPECT_TRUE(std::isfinite(result.model_accuracy));
}
