/**
 * @file test_ensemble_uncertainty.cpp
 * @brief Unit tests for ensemble-based uncertainty quantification
 *
 * TEST COVERAGE:
 * - Ensemble creation and destruction
 * - Ensemble prediction aggregation
 * - Epistemic uncertainty computation
 * - Aleatoric uncertainty computation
 * - Utility functions (entropy, variance, mean)
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/introspection/nimcp_ensemble_uncertainty.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EnsembleUncertaintyTest : public ::testing::Test {
protected:
    brain_t brain;
    adaptive_network_t network;
    ensemble_context_t ensemble;

    void SetUp() override {
        // Create minimal brain for testing
        brain = brain_create(
            "test_ensemble",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,  // inputs
            3    // outputs
        );

        // Get network from brain
        network = nullptr;
        if (brain != nullptr) {
            network = brain_get_network(brain);
        }

        ensemble = nullptr;
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
// 1. Configuration Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, DefaultConfigValid) {
    ensemble_config_t config = ensemble_default_config();

    // Check defaults match documented values
    EXPECT_EQ(config.num_models, 5u);
    EXPECT_FLOAT_EQ(config.weight_noise_sigma, 0.1f);
    EXPECT_FLOAT_EQ(config.dropout_rate, 0.2f);
    EXPECT_FALSE(config.use_bootstrap);
    EXPECT_FALSE(config.use_snapshot_ensemble);
    EXPECT_EQ(config.snapshot_interval, 10u);
    EXPECT_EQ(config.max_models, 20u);
}

TEST_F(EnsembleUncertaintyTest, DefaultConfigRangesValid) {
    ensemble_config_t config = ensemble_default_config();

    // Validate ranges
    EXPECT_GT(config.num_models, 0u);
    EXPECT_LE(config.num_models, config.max_models);
    EXPECT_GE(config.weight_noise_sigma, 0.0f);
    EXPECT_LE(config.weight_noise_sigma, 1.0f);
    EXPECT_GE(config.dropout_rate, 0.0f);
    EXPECT_LE(config.dropout_rate, 1.0f);
}

//=============================================================================
// 2. Ensemble Creation Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, CreateEnsembleWithNullNetwork) {
    ensemble_context_t ens = ensemble_create(nullptr, nullptr);
    EXPECT_EQ(ens, nullptr);
}

TEST_F(EnsembleUncertaintyTest, CreateEnsembleWithValidNetwork) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    // Check ensemble size
    uint32_t size = ensemble_get_size(ensemble);
    EXPECT_GT(size, 0u);
    EXPECT_LE(size, 5u); // Default config creates up to 5 models
}

TEST_F(EnsembleUncertaintyTest, CreateEnsembleWithCustomConfig) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble_config_t config = ensemble_default_config();
    config.num_models = 3;
    config.weight_noise_sigma = 0.05f;

    ensemble = ensemble_create(network, &config);
    ASSERT_NE(ensemble, nullptr);

    uint32_t size = ensemble_get_size(ensemble);
    EXPECT_GT(size, 0u);
    EXPECT_LE(size, 3u);
}

TEST_F(EnsembleUncertaintyTest, DestroyNullEnsemble) {
    // Should not crash
    ensemble_destroy(nullptr);
}

TEST_F(EnsembleUncertaintyTest, GetSizeOfNullEnsemble) {
    uint32_t size = ensemble_get_size(nullptr);
    EXPECT_EQ(size, 0u);
}

//=============================================================================
// 3. Model Management Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, AddModelToEnsemble) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    // Create ensemble with capacity for more models
    ensemble_config_t config = ensemble_default_config();
    config.num_models = 2;  // Start with 2
    config.max_models = 10; // Allow adding more

    ensemble = ensemble_create(network, &config);
    ASSERT_NE(ensemble, nullptr);

    uint32_t initial_size = ensemble_get_size(ensemble);

    // Add another model (would need a real network, so this may fail)
    // Just test the API
    bool result = ensemble_add_model(ensemble, nullptr, 10);
    EXPECT_FALSE(result); // Should fail with NULL network
}

TEST_F(EnsembleUncertaintyTest, AddModelWithNullEnsemble) {
    bool result = ensemble_add_model(nullptr, network, 0);
    EXPECT_FALSE(result);
}

//=============================================================================
// 4. Prediction Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, PredictWithNullEnsemble) {
    float features[10] = {0};
    ensemble_prediction_t* predictions = nullptr;

    uint32_t count = ensemble_predict(nullptr, features, 10, &predictions);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(predictions, nullptr);
}

TEST_F(EnsembleUncertaintyTest, PredictWithNullFeatures) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    ensemble_prediction_t* predictions = nullptr;
    uint32_t count = ensemble_predict(ensemble, nullptr, 10, &predictions);
    EXPECT_EQ(count, 0u);
}

TEST_F(EnsembleUncertaintyTest, PredictWithValidInput) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                         0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    ensemble_prediction_t* predictions = nullptr;

    uint32_t count = ensemble_predict(ensemble, features, 10, &predictions);

    if (count > 0 && predictions != nullptr) {
        // Validate predictions
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_NE(predictions[i].prediction, nullptr);
            EXPECT_GT(predictions[i].size, 0u);
            EXPECT_GE(predictions[i].confidence, 0.0f);
            EXPECT_LE(predictions[i].confidence, 1.0f);
            EXPECT_GE(predictions[i].entropy, 0.0f);
        }

        ensemble_predictions_free(predictions, count);
    }
}

//=============================================================================
// 5. Uncertainty Computation Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, ComputeUncertaintyWithNullEnsemble) {
    float features[10] = {0};
    ensemble_uncertainty_result_t result =
        ensemble_compute_uncertainty(nullptr, features, 10);

    EXPECT_EQ(result.num_models, 0u);
    EXPECT_EQ(result.mean_prediction, nullptr);
}

TEST_F(EnsembleUncertaintyTest, ComputeUncertaintyWithValidInput) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                         0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    ensemble_uncertainty_result_t result =
        ensemble_compute_uncertainty(ensemble, features, 10);

    if (result.num_models > 0) {
        // Validate uncertainty metrics
        EXPECT_GE(result.epistemic, 0.0f);
        EXPECT_LE(result.epistemic, 1.0f);
        EXPECT_GE(result.aleatoric, 0.0f);
        EXPECT_LE(result.aleatoric, 10.0f); // Entropy can exceed 1.0
        EXPECT_GE(result.total, 0.0f);
        EXPECT_LE(result.total, 1.0f);
        EXPECT_GE(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
        EXPECT_FLOAT_EQ(result.confidence, 1.0f - result.total);

        // Validate arrays allocated
        EXPECT_NE(result.mean_prediction, nullptr);
        EXPECT_NE(result.prediction_variance, nullptr);
        EXPECT_GT(result.prediction_size, 0u);

        ensemble_uncertainty_free(&result);
    }
}

TEST_F(EnsembleUncertaintyTest, UncertaintyRangeValidation) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                         0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    ensemble_uncertainty_result_t result =
        ensemble_compute_uncertainty(ensemble, features, 10);

    if (result.num_models > 0) {
        // Total uncertainty should be sum of components
        float sum = result.epistemic + result.aleatoric;
        EXPECT_GE(sum, 0.0f);
        // Total is clamped to [0,1], so it may be less than sum
        EXPECT_LE(result.total, sum + 0.01f); // Allow small tolerance

        ensemble_uncertainty_free(&result);
    }
}

//=============================================================================
// 6. Utility Function Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, ComputeEntropyUniform) {
    // Uniform distribution should have maximum entropy
    float uniform[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    float entropy = ensemble_compute_entropy(uniform, 4);

    // Max entropy for 4 states = log2(4) = 2 bits
    EXPECT_NEAR(entropy, 2.0f, 0.01f);
}

TEST_F(EnsembleUncertaintyTest, ComputeEntropyDeterministic) {
    // Deterministic distribution should have zero entropy
    float deterministic[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float entropy = ensemble_compute_entropy(deterministic, 4);

    EXPECT_NEAR(entropy, 0.0f, 0.01f);
}

TEST_F(EnsembleUncertaintyTest, ComputeEntropyWithNull) {
    float entropy = ensemble_compute_entropy(nullptr, 4);
    EXPECT_EQ(entropy, 0.0f);

    float data[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    entropy = ensemble_compute_entropy(data, 0);
    EXPECT_EQ(entropy, 0.0f);
}

TEST_F(EnsembleUncertaintyTest, ComputeMean) {
    const float* predictions[3];
    float pred1[2] = {0.1f, 0.2f};
    float pred2[2] = {0.3f, 0.4f};
    float pred3[2] = {0.5f, 0.6f};
    predictions[0] = pred1;
    predictions[1] = pred2;
    predictions[2] = pred3;

    float mean[2] = {0};
    ensemble_compute_mean(predictions, 3, 2, mean);

    EXPECT_FLOAT_EQ(mean[0], 0.3f); // (0.1 + 0.3 + 0.5) / 3
    EXPECT_FLOAT_EQ(mean[1], 0.4f); // (0.2 + 0.4 + 0.6) / 3
}

TEST_F(EnsembleUncertaintyTest, ComputeVariance) {
    const float* predictions[3];
    float pred1[2] = {0.1f, 0.2f};
    float pred2[2] = {0.3f, 0.4f};
    float pred3[2] = {0.5f, 0.6f};
    predictions[0] = pred1;
    predictions[1] = pred2;
    predictions[2] = pred3;

    float variance[2] = {0};
    float total_var = ensemble_compute_variance(predictions, 3, 2, variance);

    // Mean is [0.3, 0.4]
    // Variance[0] = ((0.1-0.3)^2 + (0.3-0.3)^2 + (0.5-0.3)^2) / 3
    //             = (0.04 + 0 + 0.04) / 3 = 0.0267
    EXPECT_NEAR(variance[0], 0.0267f, 0.001f);
    EXPECT_NEAR(variance[1], 0.0267f, 0.001f);
    EXPECT_GT(total_var, 0.0f);
}

//=============================================================================
// 7. Statistics Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, GetStatsWithNullEnsemble) {
    ensemble_stats_t stats;
    bool result = ensemble_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(EnsembleUncertaintyTest, GetStatsWithValidEnsemble) {
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    ensemble_stats_t stats;
    bool result = ensemble_get_stats(ensemble, &stats);

    if (result) {
        EXPECT_GT(stats.num_models, 0u);
        EXPECT_LE(stats.num_models, stats.max_models);
        EXPECT_GE(stats.total_predictions, 0u);
        EXPECT_GE(stats.memory_used_bytes, 0u);
    }
}

//=============================================================================
// 8. Introspection Integration Tests
//=============================================================================

TEST_F(EnsembleUncertaintyTest, SetEnsembleToIntrospection) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain not available";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Create ensemble
    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    // Attach to introspection
    bool result = introspection_set_ensemble(intro, ensemble);
    EXPECT_TRUE(result);

    // Retrieve ensemble
    ensemble_context_t retrieved = introspection_get_ensemble(intro);
    EXPECT_EQ(retrieved, ensemble);
}

TEST_F(EnsembleUncertaintyTest, GetUncertaintyWithEnsemble) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain not available";
    }

    introspection_context_t intro = brain_get_introspection(brain);
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    if (network == nullptr) {
        GTEST_SKIP() << "Network not available";
    }

    // Create and attach ensemble
    ensemble = ensemble_create(network, nullptr);
    ASSERT_NE(ensemble, nullptr);

    introspection_set_ensemble(intro, ensemble);

    // Get uncertainty (should use real ensemble)
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                         0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    brain_uncertainty_t uncertainty = brain_get_uncertainty(intro, features, 10);

    // Should have valid uncertainty values
    EXPECT_GE(uncertainty.epistemic, 0.0f);
    EXPECT_LE(uncertainty.epistemic, 1.0f);
    EXPECT_GE(uncertainty.aleatoric, 0.0f);
    EXPECT_GE(uncertainty.total, 0.0f);
    EXPECT_LE(uncertainty.total, 1.0f);
    EXPECT_GT(uncertainty.ensemble_size, 0u);

    brain_uncertainty_free(&uncertainty);
}

//=============================================================================
// 9. Edge Cases and Error Handling
//=============================================================================

TEST_F(EnsembleUncertaintyTest, FreeNullUncertaintyResult) {
    // Should not crash
    ensemble_uncertainty_free(nullptr);
}

TEST_F(EnsembleUncertaintyTest, FreePredictionsWithNull) {
    // Should not crash
    ensemble_predictions_free(nullptr, 0);
}

TEST_F(EnsembleUncertaintyTest, FreePredictionsWithZeroCount) {
    // Allocate with nimcp_malloc so nimcp_free can properly handle it
    ensemble_prediction_t* predictions =
        (ensemble_prediction_t*)nimcp_malloc(sizeof(ensemble_prediction_t));
    predictions[0].prediction = nullptr;
    predictions[0].size = 0;

    // Should not crash - passes 0 count so predictions[i] loop doesn't run
    // but the array itself is still freed
    ensemble_predictions_free(predictions, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
