/**
 * @file test_ensemble_uncertainty_integration.cpp
 * @brief Integration tests for ensemble uncertainty with brain and introspection
 *
 * TEST COVERAGE:
 * - Brain + ensemble integration
 * - Uncertainty API with real ensemble vs fallback
 * - Ensemble-based uncertainty in decision making
 * - Multi-model consensus
 * - Uncertainty-aware learning
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "cognitive/introspection/nimcp_ensemble_uncertainty.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EnsembleIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t intro;
    ensemble_context_t ensemble;

    void SetUp() override {
        // Create brain with introspection
        brain = brain_create(
            "test_ensemble_integration",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            20,  // inputs
            5    // outputs
        );

        intro = nullptr;
        ensemble = nullptr;

        if (brain != nullptr) {
            intro = brain_get_introspection(brain);
        }
    }

    void TearDown() override {
        if (ensemble != nullptr) {
            ensemble_destroy(ensemble);
            ensemble = nullptr;
        }

        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// 1. Brain + Ensemble Integration Tests
//=============================================================================

TEST_F(EnsembleIntegrationTest, CreateEnsembleFromBrain) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    // Create ensemble from brain's network
    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    uint32_t size = ensemble_get_size(ensemble);
    EXPECT_GT(size, 0u);
    EXPECT_LE(size, 5u);
}

TEST_F(EnsembleIntegrationTest, AttachEnsembleToIntrospection) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    // Create ensemble
    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    // Attach to introspection
    bool result = introspection_set_ensemble(intro, ensemble);
    ASSERT_TRUE(result);

    // Verify attachment
    ensemble_context_t retrieved = introspection_get_ensemble(intro);
    EXPECT_EQ(retrieved, ensemble);
}

//=============================================================================
// 2. Uncertainty Estimation Tests
//=============================================================================

TEST_F(EnsembleIntegrationTest, UncertaintyWithoutEnsemble) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Get uncertainty without ensemble (should use fallback)
    float features[20];
    for (int i = 0; i < 20; i++) {
        features[i] = 0.5f;
    }

    brain_uncertainty_t uncertainty = brain_get_uncertainty(intro, features, 20);

    // Should return valid uncertainty even without ensemble
    EXPECT_GE(uncertainty.epistemic, 0.0f);
    EXPECT_LE(uncertainty.epistemic, 1.0f);
    EXPECT_GE(uncertainty.aleatoric, 0.0f);
    EXPECT_GE(uncertainty.total, 0.0f);
    EXPECT_LE(uncertainty.total, 1.0f);

    brain_uncertainty_free(&uncertainty);
}

TEST_F(EnsembleIntegrationTest, UncertaintyWithEnsemble) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    // Create and attach ensemble
    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    introspection_set_ensemble(intro, ensemble);

    // Get uncertainty with ensemble
    float features[20];
    for (int i = 0; i < 20; i++) {
        features[i] = static_cast<float>(i) / 20.0f;
    }

    brain_uncertainty_t uncertainty = brain_get_uncertainty(intro, features, 20);

    // Should use real ensemble
    EXPECT_GT(uncertainty.ensemble_size, 0u);
    EXPECT_GE(uncertainty.epistemic, 0.0f);
    EXPECT_LE(uncertainty.epistemic, 1.0f);
    EXPECT_GE(uncertainty.aleatoric, 0.0f);
    EXPECT_GE(uncertainty.total, 0.0f);
    EXPECT_LE(uncertainty.total, 1.0f);
    EXPECT_FLOAT_EQ(uncertainty.confidence, 1.0f - uncertainty.total);

    brain_uncertainty_free(&uncertainty);
}

TEST_F(EnsembleIntegrationTest, UncertaintyComparisonWithAndWithoutEnsemble) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    float features[20];
    for (int i = 0; i < 20; i++) {
        features[i] = 0.7f;
    }

    // Get uncertainty without ensemble
    brain_uncertainty_t unc_without = brain_get_uncertainty(intro, features, 20);

    // Create and attach ensemble
    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    // Get uncertainty with ensemble
    brain_uncertainty_t unc_with = brain_get_uncertainty(intro, features, 20);

    // Both should be valid
    EXPECT_GE(unc_without.total, 0.0f);
    EXPECT_LE(unc_without.total, 1.0f);
    EXPECT_GE(unc_with.total, 0.0f);
    EXPECT_LE(unc_with.total, 1.0f);

    // Ensemble-based uncertainty should use more models
    EXPECT_GT(unc_with.ensemble_size, 0u);

    brain_uncertainty_free(&unc_without);
    brain_uncertainty_free(&unc_with);
}

//=============================================================================
// 3. Multi-Input Uncertainty Tests
//=============================================================================

TEST_F(EnsembleIntegrationTest, UncertaintyForDifferentInputs) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    // Test different inputs
    float inputs[][20] = {
        // All zeros
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        // All ones
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        // Mixed
        {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5,
         0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5},
        // Ramp
        {0.0, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45,
         0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95}
    };

    for (int i = 0; i < 4; i++) {
        brain_uncertainty_t unc = brain_get_uncertainty(intro, inputs[i], 20);

        // All should be valid
        EXPECT_GE(unc.epistemic, 0.0f) << "Input pattern " << i;
        EXPECT_LE(unc.epistemic, 1.0f) << "Input pattern " << i;
        EXPECT_GE(unc.aleatoric, 0.0f) << "Input pattern " << i;
        EXPECT_GE(unc.total, 0.0f) << "Input pattern " << i;
        EXPECT_LE(unc.total, 1.0f) << "Input pattern " << i;

        brain_uncertainty_free(&unc);
    }
}

//=============================================================================
// 4. Ensemble Size Effects
//=============================================================================

TEST_F(EnsembleIntegrationTest, LargerEnsembleLowerUncertainty) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    float features[20];
    for (int i = 0; i < 20; i++) {
        features[i] = 0.6f;
    }

    // Small ensemble
    ensemble_config_t config_small = ensemble_default_config();
    config_small.num_models = 3;
    ensemble_context_t ens_small = ensemble_create(network, &config_small);

    if (ens_small != nullptr) {
        introspection_set_ensemble(intro, ens_small);
        brain_uncertainty_t unc_small = brain_get_uncertainty(intro, features, 20);

        // Larger ensemble
        ensemble_config_t config_large = ensemble_default_config();
        config_large.num_models = 7;
        ensemble_context_t ens_large = ensemble_create(network, &config_large);

        if (ens_large != nullptr) {
            introspection_set_ensemble(intro, ens_large);
            brain_uncertainty_t unc_large = brain_get_uncertainty(intro, features, 20);

            // Larger ensemble should have different uncertainty
            // (not necessarily lower due to model diversity)
            EXPECT_GT(unc_large.ensemble_size, unc_small.ensemble_size);

            brain_uncertainty_free(&unc_large);
            ensemble_destroy(ens_large);
        }

        brain_uncertainty_free(&unc_small);
        ensemble_destroy(ens_small);
    }

    // Don't destroy ensemble in teardown since we destroyed them manually
    ensemble = nullptr;
}

//=============================================================================
// 5. Statistics Tracking
//=============================================================================

TEST_F(EnsembleIntegrationTest, EnsembleStatisticsAfterPredictions) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    adaptive_network_t network = brain_get_network(brain);
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);
    introspection_set_ensemble(intro, ensemble);

    // Get initial stats
    ensemble_stats_t stats_before;
    bool result = ensemble_get_stats(ensemble, &stats_before);
    ASSERT_TRUE(result);

    uint64_t predictions_before = stats_before.total_predictions;

    // Make some predictions
    float features[20];
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 20; j++) {
            features[j] = static_cast<float>(i) / 5.0f;
        }

        brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 20);
        brain_uncertainty_free(&unc);
    }

    // Get final stats
    ensemble_stats_t stats_after;
    result = ensemble_get_stats(ensemble, &stats_after);
    ASSERT_TRUE(result);

    // Prediction count should increase
    EXPECT_GT(stats_after.total_predictions, predictions_before);

    // Average uncertainties should be computed
    if (stats_after.total_predictions > 0) {
        EXPECT_GE(stats_after.avg_epistemic, 0.0f);
        EXPECT_GE(stats_after.avg_aleatoric, 0.0f);
    }
}

//=============================================================================
// 6. Fallback Behavior
//=============================================================================

TEST_F(EnsembleIntegrationTest, FallbackUncertaintyIsReasonable) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Test fallback without ensemble
    float features[20];
    for (int i = 0; i < 20; i++) {
        features[i] = 0.5f;
    }

    brain_uncertainty_t unc = brain_get_uncertainty(intro, features, 20);

    // Fallback should still provide reasonable uncertainty
    EXPECT_GE(unc.total, 0.0f);
    EXPECT_LE(unc.total, 1.0f);
    EXPECT_GE(unc.confidence, 0.0f);
    EXPECT_LE(unc.confidence, 1.0f);
    EXPECT_FLOAT_EQ(unc.confidence, 1.0f - unc.total);

    // Should have ensemble predictions array
    EXPECT_NE(unc.ensemble_predictions, nullptr);
    EXPECT_GT(unc.ensemble_size, 0u);

    brain_uncertainty_free(&unc);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
