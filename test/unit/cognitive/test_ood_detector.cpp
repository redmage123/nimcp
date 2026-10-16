/**
 * @file test_ood_detector.cpp
 * @brief Unit tests for OOD (Out-of-Distribution) detector
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/nimcp_ood_detector.h"
}

/* ============================================================================
 * Config Tests
 * ============================================================================ */

TEST(OODDetector, ConfigDefault) {
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    /* Weights should sum to ~1.0 */
    float sum = cfg.memory_distance_weight + cfg.energy_score_weight +
                cfg.disagreement_weight + cfg.reconstruction_weight;
    EXPECT_NEAR(sum, 1.0f, 0.01f);
    /* Threshold should be reasonable (0.5-0.9) */
    EXPECT_GT(cfg.ood_threshold, 0.4f);
    EXPECT_LT(cfg.ood_threshold, 0.95f);
    /* Defaults should be enabled */
    EXPECT_TRUE(cfg.enable_memory_check);
    EXPECT_TRUE(cfg.enable_energy_score);
    EXPECT_TRUE(cfg.enable_disagreement);
    EXPECT_TRUE(cfg.enable_reconstruction);
    EXPECT_TRUE(cfg.enable_bloom_precheck);
    /* Confidence reduction should reduce, not amplify */
    EXPECT_GT(cfg.confidence_reduction, 0.0f);
    EXPECT_LT(cfg.confidence_reduction, 1.0f);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST(OODDetector, CreateDestroy) {
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);
    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, CreateWithNullConfig) {
    /* Should use defaults */
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(NULL);
    ASSERT_NE(det, nullptr);
    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, DestroyNull) {
    /* Should not crash */
    nimcp_ood_detector_destroy(NULL);
}

/* ============================================================================
 * Energy Score Tests
 * ============================================================================ */

TEST(OODDetector, EnergyScoreUniform) {
    /* Uniform logits = maximum uncertainty = high energy */
    float logits[10];
    for (int i = 0; i < 10; i++) logits[i] = 0.0f;
    float energy = nimcp_ood_energy_score(logits, 10);
    /* -log(sum(exp(0))) = -log(10) = -2.302...  */
    EXPECT_NEAR(energy, -logf(10.0f), 0.01f);
}

TEST(OODDetector, EnergyScoreConfident) {
    /* One-hot-like logits: one large, rest small = confident = low energy */
    float logits[10];
    for (int i = 0; i < 10; i++) logits[i] = -10.0f;
    logits[3] = 10.0f;  /* Strong prediction on class 3 */
    float energy = nimcp_ood_energy_score(logits, 10);
    /* Should be approximately -10 - log(1 + 9*exp(-20)) ~ -10 */
    EXPECT_LT(energy, -5.0f);  /* Very negative = very confident */
}

TEST(OODDetector, EnergyScoreZeros) {
    /* All-zero logits: moderate energy */
    float logits[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float energy = nimcp_ood_energy_score(logits, 5);
    /* -log(sum(exp(0))) = -log(5) ≈ -1.609 */
    EXPECT_NEAR(energy, -logf(5.0f), 0.01f);
}

TEST(OODDetector, EnergyScoreNullInput) {
    float energy = nimcp_ood_energy_score(NULL, 10);
    EXPECT_EQ(energy, 0.0f);
}

TEST(OODDetector, EnergyScoreZeroDim) {
    float logits[1] = {1.0f};
    float energy = nimcp_ood_energy_score(logits, 0);
    EXPECT_EQ(energy, 0.0f);
}

/* ============================================================================
 * Disagreement Score Tests
 * ============================================================================ */

TEST(OODDetector, DisagreementIdentical) {
    float a[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float b[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float dis = nimcp_ood_disagreement_score(a, b, 5);
    EXPECT_NEAR(dis, 0.0f, 0.001f);
}

TEST(OODDetector, DisagreementOrthogonal) {
    /* Orthogonal vectors: cosine = 0 -> disagreement = 0.5 */
    float a[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float b[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    float dis = nimcp_ood_disagreement_score(a, b, 4);
    EXPECT_NEAR(dis, 0.5f, 0.001f);
}

TEST(OODDetector, DisagreementOpposite) {
    /* Opposite vectors: cosine = -1 -> disagreement = 1.0 */
    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[4] = {-1.0f, -2.0f, -3.0f, -4.0f};
    float dis = nimcp_ood_disagreement_score(a, b, 4);
    EXPECT_NEAR(dis, 1.0f, 0.001f);
}

TEST(OODDetector, DisagreementPartial) {
    /* Partially similar vectors -> mid-range */
    float a[4] = {1.0f, 1.0f, 0.0f, 0.0f};
    float b[4] = {1.0f, 0.0f, 1.0f, 0.0f};
    float dis = nimcp_ood_disagreement_score(a, b, 4);
    /* cosine = 1/(sqrt(2)*sqrt(2)) = 0.5, disagreement = (1-0.5)/2 = 0.25 */
    EXPECT_GT(dis, 0.1f);
    EXPECT_LT(dis, 0.7f);
}

TEST(OODDetector, DisagreementNullInputs) {
    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_EQ(nimcp_ood_disagreement_score(NULL, a, 4), 0.0f);
    EXPECT_EQ(nimcp_ood_disagreement_score(a, NULL, 4), 0.0f);
    EXPECT_EQ(nimcp_ood_disagreement_score(NULL, NULL, 4), 0.0f);
}

TEST(OODDetector, DisagreementZeroDim) {
    float a[1] = {1.0f};
    float b[1] = {2.0f};
    EXPECT_EQ(nimcp_ood_disagreement_score(a, b, 0), 0.0f);
}

/* ============================================================================
 * Reconstruction Error Tests
 * ============================================================================ */

TEST(OODDetector, ReconstructionPerfect) {
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float recon[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float err = nimcp_ood_reconstruction_error(input, recon, 5);
    EXPECT_NEAR(err, 0.0f, 1e-6f);
}

TEST(OODDetector, ReconstructionBad) {
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float recon[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    float err = nimcp_ood_reconstruction_error(input, recon, 4);
    /* MSE = ((9^2 + 18^2 + 27^2 + 36^2) / 4) = (81+324+729+1296)/4 = 607.5 */
    EXPECT_GT(err, 100.0f);
}

TEST(OODDetector, ReconstructionNullInputs) {
    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_EQ(nimcp_ood_reconstruction_error(NULL, a, 4), 0.0f);
    EXPECT_EQ(nimcp_ood_reconstruction_error(a, NULL, 4), 0.0f);
}

/* ============================================================================
 * Detection Tests (no memory store)
 * ============================================================================ */

TEST(OODDetector, DetectEnergyOnly) {
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_disagreement   = false;
    cfg.enable_reconstruction = false;
    /* Only energy active */

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    /* Uniform logits = uncertain = high energy -> OOD-ish */
    float features[8] = {0};
    float logits[10];
    for (int i = 0; i < 10; i++) logits[i] = 0.0f;

    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(det, features, 8, logits, 10, NULL, 0, NULL, 0, NULL, &result);
    EXPECT_EQ(rc, 0);
    /* Energy score should be populated */
    EXPECT_GE(result.energy_score, 0.0f);
    EXPECT_LE(result.energy_score, 1.0f);
    /* OOD score should equal energy_score (only signal active, normalized by its weight) */
    EXPECT_NEAR(result.ood_score, result.energy_score, 0.001f);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, DetectWithDisagreement) {
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_energy_score   = false;
    cfg.enable_reconstruction = false;
    /* Only disagreement active */

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    /* Two completely opposite outputs = max disagreement */
    float primary[4]   = {1.0f, 0.0f, 0.0f, 0.0f};
    float secondary[4] = {-1.0f, 0.0f, 0.0f, 0.0f};

    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(det, features, 4, primary, 4, secondary, 4, NULL, 0, NULL, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(result.disagreement_score, 1.0f, 0.001f);
    /* With only disagreement at 1.0, ood_score should be 1.0 */
    EXPECT_NEAR(result.ood_score, 1.0f, 0.001f);
    EXPECT_TRUE(result.is_ood);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, DetectWithReconstruction) {
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_energy_score   = false;
    cfg.enable_disagreement   = false;
    /* Only reconstruction active */

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    /* Perfect reconstruction -> not OOD */
    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float recon[4]    = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(det, features, 4, NULL, 0, NULL, 0, recon, 4, NULL, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(result.reconstruction_error, 0.0f, 0.001f);
    EXPECT_NEAR(result.ood_score, 0.0f, 0.001f);
    EXPECT_FALSE(result.is_ood);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, DetectWithAllSignalsNoMemory) {
    /* All signals except memory (needs a real store) */
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check = false;

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    /* Uniform logits (uncertain) */
    float logits[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    /* Opposite secondary output (high disagreement) */
    float secondary[4] = {-1.0f, -2.0f, -3.0f, -4.0f};
    /* Bad reconstruction */
    float recon[4] = {10.0f, 20.0f, 30.0f, 40.0f};

    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(det, features, 4, logits, 4, secondary, 4, recon, 4, NULL, &result);
    EXPECT_EQ(rc, 0);

    /* All signals should indicate OOD */
    EXPECT_GT(result.energy_score, 0.0f);
    EXPECT_GT(result.disagreement_score, 0.0f);
    EXPECT_GT(result.reconstruction_error, 0.0f);
    /* Combined should be high -> OOD */
    EXPECT_GT(result.ood_score, 0.3f);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, ConfidenceReduction) {
    /* Force OOD by using only disagreement with opposite vectors */
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_energy_score   = false;
    cfg.enable_reconstruction = false;
    cfg.confidence_reduction  = 0.5f;

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4]  = {1.0f, 2.0f, 3.0f, 4.0f};
    float primary[4]   = {1.0f, 0.0f, 0.0f, 0.0f};
    float secondary[4] = {-1.0f, 0.0f, 0.0f, 0.0f};

    nimcp_ood_result_t result;
    nimcp_ood_detect(det, features, 4, primary, 4, secondary, 4, NULL, 0, NULL, &result);

    EXPECT_TRUE(result.is_ood);
    EXPECT_NEAR(result.confidence_adjustment, 0.5f, 0.001f);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, ConfidenceNoReduction) {
    /* In-distribution: identical vectors = 0 disagreement */
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_energy_score   = false;
    cfg.enable_reconstruction = false;

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4]  = {1.0f, 2.0f, 3.0f, 4.0f};
    float primary[4]   = {1.0f, 2.0f, 3.0f, 4.0f};
    float secondary[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_ood_result_t result;
    nimcp_ood_detect(det, features, 4, primary, 4, secondary, 4, NULL, 0, NULL, &result);

    EXPECT_FALSE(result.is_ood);
    EXPECT_NEAR(result.confidence_adjustment, 1.0f, 0.001f);

    nimcp_ood_detector_destroy(det);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST(OODDetector, StatsTracking) {
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_energy_score   = false;
    cfg.enable_reconstruction = false;
    /* Only disagreement */

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    /* Run 10 detections: 5 OOD (opposite), 5 in-dist (identical) */
    for (int i = 0; i < 10; i++) {
        float primary[4]   = {1.0f, 0.0f, 0.0f, 0.0f};
        float secondary[4];
        if (i < 5) {
            /* Opposite = OOD */
            secondary[0] = -1.0f; secondary[1] = 0.0f;
            secondary[2] = 0.0f;  secondary[3] = 0.0f;
        } else {
            /* Identical = in-dist */
            secondary[0] = 1.0f; secondary[1] = 0.0f;
            secondary[2] = 0.0f; secondary[3] = 0.0f;
        }

        nimcp_ood_result_t result;
        nimcp_ood_detect(det, features, 4, primary, 4, secondary, 4, NULL, 0, NULL, &result);
        nimcp_ood_update_stats(det, &result);
    }

    nimcp_ood_stats_t stats;
    int rc = nimcp_ood_get_stats(det, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_checks, 10u);
    EXPECT_EQ(stats.ood_detected, 5u);
    EXPECT_EQ(stats.in_distribution, 5u);
    EXPECT_NEAR(stats.ood_rate, 0.5f, 0.01f);
    EXPECT_GT(stats.max_ood_score, 0.5f);
    EXPECT_GT(stats.avg_ood_score, 0.0f);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, StatsInitiallyEmpty) {
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(NULL);
    ASSERT_NE(det, nullptr);

    nimcp_ood_stats_t stats;
    nimcp_ood_get_stats(det, &stats);
    EXPECT_EQ(stats.total_checks, 0u);
    EXPECT_EQ(stats.ood_detected, 0u);
    EXPECT_EQ(stats.in_distribution, 0u);
    EXPECT_NEAR(stats.avg_ood_score, 0.0f, 1e-6f);

    nimcp_ood_detector_destroy(det);
}

/* ============================================================================
 * Null / Edge Case Tests
 * ============================================================================ */

TEST(OODDetector, NullDetector) {
    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, &result);
    EXPECT_EQ(rc, -1);
}

TEST(OODDetector, NullResult) {
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(NULL);
    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    int rc = nimcp_ood_detect(det, features, 4, NULL, 0, NULL, 0, NULL, 0, NULL, NULL);
    EXPECT_EQ(rc, -1);
    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, NullFeatures) {
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(NULL);
    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(det, NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, &result);
    EXPECT_EQ(rc, -1);
    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, ZeroDimFeatures) {
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(NULL);
    float features[1] = {1.0f};
    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(det, features, 0, NULL, 0, NULL, 0, NULL, 0, NULL, &result);
    EXPECT_EQ(rc, -1);
    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, NoSignalsEnabled) {
    /* All signals disabled, no memory store -> ood_score = 0 */
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_energy_score   = false;
    cfg.enable_disagreement   = false;
    cfg.enable_reconstruction = false;

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_ood_result_t result;
    int rc = nimcp_ood_detect(det, features, 4, NULL, 0, NULL, 0, NULL, 0, NULL, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(result.ood_score, 0.0f, 0.001f);
    EXPECT_FALSE(result.is_ood);
    EXPECT_NEAR(result.confidence_adjustment, 1.0f, 0.001f);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, UpdateStatsNullDetector) {
    nimcp_ood_result_t result = {};
    EXPECT_EQ(nimcp_ood_update_stats(NULL, &result), -1);
}

TEST(OODDetector, UpdateStatsNullResult) {
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(NULL);
    EXPECT_EQ(nimcp_ood_update_stats(det, NULL), -1);
    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, GetStatsNullDetector) {
    nimcp_ood_stats_t stats;
    EXPECT_EQ(nimcp_ood_get_stats(NULL, &stats), -1);
}

TEST(OODDetector, GetStatsNullStats) {
    nimcp_ood_detector_t* det = nimcp_ood_detector_create(NULL);
    EXPECT_EQ(nimcp_ood_get_stats(det, NULL), -1);
    nimcp_ood_detector_destroy(det);
}

/* ============================================================================
 * Normalization / Boundary Tests
 * ============================================================================ */

TEST(OODDetector, EnergyNormalizationBounds) {
    /* Verify energy normalization stays in [0, 1] with extreme logits */
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_disagreement   = false;
    cfg.enable_reconstruction = false;

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4] = {0};

    /* Very confident logits (low energy) */
    float confident[4] = {-100.0f, -100.0f, -100.0f, 100.0f};
    nimcp_ood_result_t result;
    nimcp_ood_detect(det, features, 4, confident, 4, NULL, 0, NULL, 0, NULL, &result);
    EXPECT_GE(result.energy_score, 0.0f);
    EXPECT_LE(result.energy_score, 1.0f);

    /* Very uncertain logits (high energy) — single logit */
    float uncertain[1] = {0.0f};
    nimcp_ood_detect(det, features, 4, uncertain, 1, NULL, 0, NULL, 0, NULL, &result);
    EXPECT_GE(result.energy_score, 0.0f);
    EXPECT_LE(result.energy_score, 1.0f);

    nimcp_ood_detector_destroy(det);
}

TEST(OODDetector, ReconstructionNormalizationCap) {
    /* Very bad reconstruction should cap at 1.0 after normalization */
    nimcp_ood_config_t cfg = nimcp_ood_config_default();
    cfg.enable_memory_check   = false;
    cfg.enable_energy_score   = false;
    cfg.enable_disagreement   = false;

    nimcp_ood_detector_t* det = nimcp_ood_detector_create(&cfg);
    ASSERT_NE(det, nullptr);

    float features[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float bad_recon[4] = {100.0f, 200.0f, 300.0f, 400.0f};

    nimcp_ood_result_t result;
    nimcp_ood_detect(det, features, 4, NULL, 0, NULL, 0, bad_recon, 4, NULL, &result);
    /* recon_norm = min(mse * 10, 1.0) should be capped at 1.0 */
    EXPECT_NEAR(result.reconstruction_error, 1.0f, 0.001f);

    nimcp_ood_detector_destroy(det);
}
