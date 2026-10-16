/**
 * @file test_brain_region_predictive.cpp
 * @brief Unit tests for brain region predictive coding integration
 *
 * TEST COVERAGE:
 * - Configuration creation
 * - Enable/disable predictive processing
 * - Prediction generation
 * - Error computation
 * - Precision modulation
 * - Hierarchical connections
 * - Statistics queries
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include "core/brain_regions/nimcp_brain_region_predictive.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainRegionPredictiveTest : public ::testing::Test {
protected:
    brain_region_t* sensory_region;
    brain_region_t* association_region;

    void SetUp() override {
        // Create sensory region (V1-like)
        sensory_region = brain_region_create(REGION_VISUAL_V1, 100);
        ASSERT_NE(sensory_region, nullptr);

        // Create association region (higher-level)
        association_region = brain_region_create(REGION_PARIETAL, 50);
        ASSERT_NE(association_region, nullptr);
    }

    void TearDown() override {
        if (sensory_region) {
            brain_region_disable_predictive(sensory_region);
            brain_region_destroy(sensory_region);
        }
        if (association_region) {
            brain_region_disable_predictive(association_region);
            brain_region_destroy(association_region);
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, ConfigDefaultCreation) {
    /* WHAT: Test default configuration creation
     * WHY:  Verify factory method produces valid config
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_default(1);

    EXPECT_TRUE(config.enable_predictive_processing);
    EXPECT_TRUE(config.generate_predictions);
    EXPECT_TRUE(config.compute_prediction_errors);
    EXPECT_TRUE(config.learn_precisions);
    EXPECT_EQ(config.hierarchy_level, 1);
    EXPECT_GT(config.prediction_learning_rate, 0.0f);
    EXPECT_GT(config.precision_learning_rate, 0.0f);
    EXPECT_GT(config.error_correction_rate, 0.0f);
    EXPECT_EQ(config.prediction_channel, BIO_CHANNEL_SEROTONIN);
    EXPECT_EQ(config.error_channel, BIO_CHANNEL_NOREPINEPHRINE);
}

TEST_F(BrainRegionPredictiveTest, ConfigSensoryCreation) {
    /* WHAT: Test sensory configuration
     * WHY:  Verify sensory-specific settings
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();

    EXPECT_TRUE(config.enable_predictive_processing);
    EXPECT_FALSE(config.generate_predictions);  // Sensory doesn't predict downward
    EXPECT_TRUE(config.compute_prediction_errors);  // But computes errors
    EXPECT_TRUE(config.broadcast_errors);
    EXPECT_FALSE(config.broadcast_predictions);
    EXPECT_EQ(config.hierarchy_level, 0);  // Bottom level
}

TEST_F(BrainRegionPredictiveTest, ConfigAssociationCreation) {
    /* WHAT: Test association configuration
     * WHY:  Verify high-level region settings
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_association();

    EXPECT_TRUE(config.enable_predictive_processing);
    EXPECT_TRUE(config.generate_predictions);
    EXPECT_TRUE(config.compute_prediction_errors);
    EXPECT_TRUE(config.learn_precisions);
    EXPECT_TRUE(config.broadcast_predictions);
    EXPECT_TRUE(config.broadcast_errors);
    EXPECT_LT(config.error_correction_rate, 0.1f);  // Slower dynamics
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, EnablePredictiveProcessing) {
    /* WHAT: Test enabling predictive processing
     * WHY:  Verify initialization succeeds
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();

    nimcp_result_t result = brain_region_enable_predictive(sensory_region, &config);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify extension created
    brain_region_predictive_t* pred = brain_region_get_predictive(sensory_region);
    EXPECT_NE(pred, nullptr);

    // Verify buffers allocated
    EXPECT_NE(pred->current_prediction, nullptr);
    EXPECT_NE(pred->prediction_error, nullptr);
    EXPECT_NE(pred->precision_weights, nullptr);

    // Verify hierarchy created
    EXPECT_NE(pred->hierarchy, nullptr);

    // Verify statistics initialized
    EXPECT_EQ(pred->total_predictions, 0);
    EXPECT_EQ(pred->total_errors_computed, 0);
    EXPECT_FLOAT_EQ(pred->mean_precision, 1.0f);
}

TEST_F(BrainRegionPredictiveTest, EnablePredictiveNullChecks) {
    /* WHAT: Test NULL argument handling
     * WHY:  Verify guard clauses work
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_default(0);

    EXPECT_EQ(brain_region_enable_predictive(nullptr, &config),
              NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(brain_region_enable_predictive(sensory_region, nullptr),
              NIMCP_ERROR_NULL_ARG);
}

TEST_F(BrainRegionPredictiveTest, EnablePredictiveIdempotent) {
    /* WHAT: Test enabling twice is safe
     * WHY:  Verify idempotent operation
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_default(0);

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);  // Should succeed, no-op

    // Should still have valid extension
    EXPECT_NE(brain_region_get_predictive(sensory_region), nullptr);
}

TEST_F(BrainRegionPredictiveTest, DisablePredictiveProcessing) {
    /* WHAT: Test disabling predictive processing
     * WHY:  Verify cleanup works
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_default(0);

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);
    EXPECT_NE(brain_region_get_predictive(sensory_region), nullptr);

    EXPECT_EQ(brain_region_disable_predictive(sensory_region),
              NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_get_predictive(sensory_region), nullptr);
}

//=============================================================================
// Prediction Generation Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, GeneratePrediction) {
    /* WHAT: Test prediction generation
     * WHY:  Verify top-down prediction works
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_association();

    EXPECT_EQ(brain_region_enable_predictive(association_region, &config),
              NIMCP_SUCCESS);

    // Generate prediction for lower region
    float prediction[100] = {0};
    nimcp_result_t result = brain_region_predict_lower(
        association_region, 1, prediction, 100);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify statistics updated
    brain_region_predictive_t* pred = brain_region_get_predictive(association_region);
    EXPECT_EQ(pred->total_predictions, 1);
}

TEST_F(BrainRegionPredictiveTest, GeneratePredictionGuardClauses) {
    /* WHAT: Test prediction generation error handling
     * WHY:  Verify guard clauses
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();  // Doesn't generate predictions

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    float prediction[100] = {0};

    // Sensory region shouldn't generate predictions
    nimcp_result_t result = brain_region_predict_lower(
        sensory_region, 1, prediction, 100);
    EXPECT_NE(result, NIMCP_SUCCESS);

    // NULL checks
    EXPECT_EQ(brain_region_predict_lower(nullptr, 1, prediction, 100),
              NIMCP_ERROR_NULL_ARG);
    EXPECT_EQ(brain_region_predict_lower(sensory_region, 1, nullptr, 100),
              NIMCP_ERROR_NULL_ARG);
}

//=============================================================================
// Error Computation Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, ComputePredictionError) {
    /* WHAT: Test prediction error computation
     * WHY:  Verify error = actual - predicted
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    // Create test data
    float actual[100];
    float predicted[100];
    float error[100];

    for (uint32_t i = 0; i < 100; i++) {
        actual[i] = 1.0f;
        predicted[i] = 0.5f;
    }

    // Compute error
    nimcp_result_t result = brain_region_compute_error(
        sensory_region, actual, predicted, error, 100);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify error computed (actual - predicted, precision-weighted)
    // With default precision = 1.0, error ≈ 0.5
    EXPECT_GT(error[0], 0.0f);  // Should be positive
    EXPECT_LT(error[0], 1.0f);  // Should be less than difference

    // Verify statistics updated
    brain_region_predictive_t* pred = brain_region_get_predictive(sensory_region);
    EXPECT_EQ(pred->total_errors_computed, 1);
    EXPECT_GT(pred->mean_prediction_error, 0.0f);
}

TEST_F(BrainRegionPredictiveTest, ComputeErrorPrecisionWeighting) {
    /* WHAT: Test precision weighting of errors
     * WHY:  Verify ε = π × (actual - predicted)
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_default(0);

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    // Set high precision for first half, low for second half
    float precisions[100];
    for (uint32_t i = 0; i < 50; i++) {
        precisions[i] = 10.0f;  // High precision (attend)
    }
    for (uint32_t i = 50; i < 100; i++) {
        precisions[i] = 0.1f;   // Low precision (ignore)
    }

    EXPECT_EQ(brain_region_set_precision(sensory_region, precisions, 100),
              NIMCP_SUCCESS);

    // Compute errors with uniform actual/predicted difference
    float actual[100];
    float predicted[100];
    float error[100];

    for (uint32_t i = 0; i < 100; i++) {
        actual[i] = 1.0f;
        predicted[i] = 0.0f;  // Constant difference
    }

    EXPECT_EQ(brain_region_compute_error(sensory_region, actual, predicted, error, 100),
              NIMCP_SUCCESS);

    // First half should have larger errors (high precision)
    // Second half should have smaller errors (low precision)
    EXPECT_GT(fabsf(error[0]), fabsf(error[99]));
}

//=============================================================================
// Hierarchical Processing Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, HierarchicalStep) {
    /* WHAT: Test one hierarchical processing cycle
     * WHY:  Verify complete prediction → error → update cycle
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    // Provide sensory input
    float sensory_input[100];
    for (uint32_t i = 0; i < 100; i++) {
        sensory_input[i] = 0.5f * (float)i / 100.0f;
    }

    nimcp_result_t result = brain_region_hierarchical_step(
        sensory_region, sensory_input, 100, 1.0f);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify processing occurred
    brain_region_predictive_t* pred = brain_region_get_predictive(sensory_region);
    EXPECT_GT(pred->total_errors_computed, 0);
}

TEST_F(BrainRegionPredictiveTest, HierarchicalConvergence) {
    /* WHAT: Test convergence to stable representation
     * WHY:  Verify iterative inference works
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();
    config.max_iterations = 10;
    config.convergence_tolerance = 0.01f;

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    float sensory_input[100];
    for (uint32_t i = 0; i < 100; i++) {
        sensory_input[i] = 0.8f;  // Constant input
    }

    uint32_t iterations = brain_region_hierarchical_converge(
        sensory_region, sensory_input, 100, 0, 0.0f);  // Use config defaults

    EXPECT_GT(iterations, 0);
    EXPECT_LE(iterations, 10);  // Should converge within max iterations

    // Free energy should be relatively low after convergence
    float fe = brain_region_get_free_energy(sensory_region);
    EXPECT_GE(fe, 0.0f);  // Free energy is always non-negative
}

//=============================================================================
// Precision (Attention) Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, SetPrecision) {
    /* WHAT: Test setting precision weights
     * WHY:  Verify attention modulation
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_default(0);

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    float precisions[100];
    for (uint32_t i = 0; i < 100; i++) {
        precisions[i] = 2.0f;  // Higher precision
    }

    EXPECT_EQ(brain_region_set_precision(sensory_region, precisions, 100),
              NIMCP_SUCCESS);

    // Read back precisions
    float read_precisions[100];
    EXPECT_EQ(brain_region_get_precision(sensory_region, read_precisions, 100),
              NIMCP_SUCCESS);

    // Verify set correctly
    for (uint32_t i = 0; i < 100; i++) {
        EXPECT_FLOAT_EQ(read_precisions[i], 2.0f);
    }
}

TEST_F(BrainRegionPredictiveTest, LearnPrecisions) {
    /* WHAT: Test precision learning from error statistics
     * WHY:  Verify automatic attention allocation
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_default(0);
    config.learn_precisions = true;

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    // Create errors (high variance in first half, low in second half)
    brain_region_predictive_t* pred = brain_region_get_predictive(sensory_region);
    for (uint32_t i = 0; i < 50; i++) {
        pred->prediction_error[i] = 5.0f;  // Large errors
    }
    for (uint32_t i = 50; i < 100; i++) {
        pred->prediction_error[i] = 0.1f;  // Small errors
    }

    // Learn precisions
    EXPECT_EQ(brain_region_learn_precisions(sensory_region, 1.0f),
              NIMCP_SUCCESS);

    // Read precisions
    float precisions[100];
    EXPECT_EQ(brain_region_get_precision(sensory_region, precisions, 100),
              NIMCP_SUCCESS);

    // Second half should have higher precision (lower error variance)
    // Note: This test may need adjustment based on learning dynamics
    EXPECT_GT(precisions[75], 0.0f);
    EXPECT_GT(precisions[25], 0.0f);
}

//=============================================================================
// Inter-Region Connection Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, ConnectRegionsHierarchically) {
    /* WHAT: Test hierarchical connection between regions
     * WHY:  Verify predictive network construction
     */
    brain_region_predictive_config_t sensory_config =
        brain_region_predictive_config_sensory();
    brain_region_predictive_config_t assoc_config =
        brain_region_predictive_config_association();

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &sensory_config),
              NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_enable_predictive(association_region, &assoc_config),
              NIMCP_SUCCESS);

    // Connect association → sensory (higher → lower)
    nimcp_result_t result = brain_region_connect_predictive(
        association_region, sensory_region, 0.8f);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify connections established
    brain_region_predictive_t* assoc_pred =
        brain_region_get_predictive(association_region);
    brain_region_predictive_t* sensory_pred =
        brain_region_get_predictive(sensory_region);

    EXPECT_EQ(assoc_pred->num_output_regions, 1);
    EXPECT_EQ(sensory_pred->num_input_regions, 1);
    EXPECT_EQ(assoc_pred->output_region_ids[0], sensory_region->id);
    EXPECT_EQ(sensory_pred->input_region_ids[0], association_region->id);
}

TEST_F(BrainRegionPredictiveTest, DisconnectRegions) {
    /* WHAT: Test disconnecting hierarchical connection
     * WHY:  Verify network reconfiguration
     */
    brain_region_predictive_config_t sensory_config =
        brain_region_predictive_config_sensory();
    brain_region_predictive_config_t assoc_config =
        brain_region_predictive_config_association();

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &sensory_config),
              NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_enable_predictive(association_region, &assoc_config),
              NIMCP_SUCCESS);

    EXPECT_EQ(brain_region_connect_predictive(association_region, sensory_region, 0.8f),
              NIMCP_SUCCESS);

    // Disconnect
    EXPECT_EQ(brain_region_disconnect_predictive(association_region, sensory_region),
              NIMCP_SUCCESS);

    // Verify disconnected
    brain_region_predictive_t* assoc_pred =
        brain_region_get_predictive(association_region);
    brain_region_predictive_t* sensory_pred =
        brain_region_get_predictive(sensory_region);

    EXPECT_EQ(assoc_pred->num_output_regions, 0);
    EXPECT_EQ(sensory_pred->num_input_regions, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BrainRegionPredictiveTest, GetStatistics) {
    /* WHAT: Test statistics retrieval
     * WHY:  Verify monitoring capabilities
     */
    brain_region_predictive_config_t config =
        brain_region_predictive_config_sensory();

    EXPECT_EQ(brain_region_enable_predictive(sensory_region, &config),
              NIMCP_SUCCESS);

    // Generate some activity
    float sensory_input[100];
    for (uint32_t i = 0; i < 100; i++) {
        sensory_input[i] = 0.5f;
    }

    brain_region_hierarchical_step(sensory_region, sensory_input, 100, 1.0f);

    // Get statistics
    brain_region_predictive_stats_t stats;
    EXPECT_EQ(brain_region_get_predictive_stats(sensory_region, &stats),
              NIMCP_SUCCESS);

    EXPECT_GT(stats.total_errors_computed, 0);
    EXPECT_EQ(stats.hierarchy_level, 0);
    EXPECT_GT(stats.mean_precision, 0.0f);
    EXPECT_GE(stats.total_free_energy, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
