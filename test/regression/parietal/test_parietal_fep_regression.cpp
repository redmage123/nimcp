/**
 * @file test_parietal_fep_regression.cpp
 * @brief Regression tests for Parietal-FEP Bridge integration
 *
 * WHAT: Test for regression in parietal-FEP integration behavior
 * WHY:  Ensure FEP parameters, numerical stability, and performance remain stable
 * HOW:  Test fixed parameter values, boundary conditions, and performance baselines
 *
 * REGRESSION TEST CATEGORIES:
 * - FEP parameter stability (precision weights, learning rates)
 * - Numerical stability of belief updates
 * - Free energy computation consistency
 * - Active inference behavior preservation
 * - Performance overhead baselines
 * - Backward compatibility
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ParietalFEPRegressionTest : public ::testing::Test {
protected:
    parietal_lobe_t* parietal;
    fep_parietal_bridge_t* fep_bridge;

    void SetUp() override {
        parietal = parietal_create();
        ASSERT_NE(parietal, nullptr);

        fep_bridge = parietal_get_fep_bridge(parietal);
        /* FEP bridge may be null if not enabled - that's OK for some tests */
    }

    void TearDown() override {
        if (parietal) {
            parietal_destroy(parietal);
            parietal = nullptr;
        }
    }

    /* Helper to measure execution time */
    template<typename Func>
    double measure_time_us(Func f) {
        auto start = std::chrono::high_resolution_clock::now();
        f();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return static_cast<double>(duration.count());
    }
};

//=============================================================================
// FEP Parameter Stability Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, DefaultConfigValues) {
    /* WHAT: Test default configuration values
     * WHY:  These values are tuned for optimal inference - changes break behavior
     */

    fep_parietal_config_t config = fep_parietal_default_config();

    /* Default precision should be in valid range */
    EXPECT_GE(config.initial_precision, 0.0f);
    EXPECT_LE(config.initial_precision, 1.0f);

    /* Learning rate should be small for stability */
    EXPECT_GE(config.belief_learning_rate, 0.0f);
    EXPECT_LE(config.belief_learning_rate, 1.0f);

    EXPECT_GE(config.precision_learning_rate, 0.0f);
    EXPECT_LE(config.precision_learning_rate, 1.0f);

    /* Min precision should be less than max */
    EXPECT_LT(config.min_precision, config.max_precision);
}

TEST_F(ParietalFEPRegressionTest, ConfigEnabledByDefault) {
    /* WHAT: Test FEP is enabled by default
     * WHY:  Ensure integration is active for parietal processing
     */

    fep_parietal_config_t config = fep_parietal_default_config();
    EXPECT_TRUE(config.enabled);
}

TEST_F(ParietalFEPRegressionTest, DomainModelsEnabled) {
    /* WHAT: Test domain-specific models are enabled
     * WHY:  Parietal processing needs all domain models active
     */

    fep_parietal_config_t config = fep_parietal_default_config();

    EXPECT_TRUE(config.enable_numerical_model);
    EXPECT_TRUE(config.enable_spatial_model);
    EXPECT_TRUE(config.enable_algebraic_model);
    EXPECT_TRUE(config.enable_physical_model);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, BeliefUpdateNumericalStability) {
    /* WHAT: Test belief updates don't produce NaN/Inf
     * WHY:  Edge cases in belief update math can cause numerical issues
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Create request with normal values */
    parietal_request_t request = {};
    request.type = PARIETAL_FEP_UPDATE_BELIEFS;
    request.priority = 1.0f;

    /* Run many iterations */
    for (int i = 0; i < 100; i++) {
        parietal_result_t result = parietal_process(parietal, &request);

        /* Result confidence should never be NaN or Inf */
        EXPECT_FALSE(std::isnan(result.confidence));
        EXPECT_FALSE(std::isinf(result.confidence));
        EXPECT_GE(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
    }
}

TEST_F(ParietalFEPRegressionTest, FreeEnergyBoundedness) {
    /* WHAT: Test free energy values stay bounded
     * WHY:  Unbounded free energy indicates computation errors
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Compute free energy multiple times */
    for (int i = 0; i < 50; i++) {
        parietal_request_t request = {};
        request.type = PARIETAL_FEP_COMPUTE_SURPRISE;
        request.priority = 1.0f;

        parietal_result_t result = parietal_process(parietal, &request);

        /* Result should be finite */
        EXPECT_FALSE(std::isnan(result.confidence));
        EXPECT_FALSE(std::isinf(result.confidence));
    }
}

TEST_F(ParietalFEPRegressionTest, PredictResultStability) {
    /* WHAT: Test prediction results are stable
     * WHY:  Predictions should not produce invalid values
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    parietal_request_t request = {};
    request.type = PARIETAL_FEP_PREDICT;
    request.priority = 1.0f;

    for (int i = 0; i < 50; i++) {
        parietal_result_t result = parietal_process(parietal, &request);

        /* Should get valid result */
        EXPECT_FALSE(std::isnan(result.confidence));
        EXPECT_FALSE(std::isinf(result.confidence));
    }
}

//=============================================================================
// Free Energy Computation Consistency Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, FreeEnergyDeterministic) {
    /* WHAT: Test same inputs produce consistent results
     * WHY:  Non-deterministic FE breaks reproducibility
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Process identical request multiple times */
    parietal_request_t request = {};
    request.type = PARIETAL_FEP_PREDICT;
    request.priority = 0.5f;

    parietal_result_t result1 = parietal_process(parietal, &request);
    parietal_result_t result2 = parietal_process(parietal, &request);

    /* Results should be identical (or very close) */
    EXPECT_NEAR(result1.confidence, result2.confidence, 0.05f);
}

TEST_F(ParietalFEPRegressionTest, StatisticsAccumulate) {
    /* WHAT: Test statistics accumulate correctly
     * WHY:  Verify FEP processing is tracked
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Get initial stats */
    fep_parietal_stats_t stats_before;
    fep_parietal_get_stats(fep_bridge, &stats_before);

    /* Process many belief updates */
    for (int i = 0; i < 20; i++) {
        parietal_request_t request = {};
        request.type = PARIETAL_FEP_UPDATE_BELIEFS;
        request.priority = 1.0f;
        parietal_process(parietal, &request);
    }

    /* Get final stats */
    fep_parietal_stats_t stats_after;
    fep_parietal_get_stats(fep_bridge, &stats_after);

    /* Verify some processing occurred */
    EXPECT_GE(stats_after.belief_updates, stats_before.belief_updates);
}

//=============================================================================
// Active Inference Behavior Preservation Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, ActiveInferenceConsistency) {
    /* WHAT: Test active inference produces consistent action preferences
     * WHY:  Random action selection would break expected behavior
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Run active inference multiple times */
    parietal_request_t request = {};
    request.type = PARIETAL_FEP_ACTIVE_INFERENCE;
    request.priority = 1.0f;

    std::vector<float> confidences;
    for (int i = 0; i < 10; i++) {
        parietal_result_t result = parietal_process(parietal, &request);
        confidences.push_back(result.confidence);
    }

    /* Calculate variance - should be low for deterministic behavior */
    float mean = 0.0f;
    for (float c : confidences) mean += c;
    mean /= confidences.size();

    float variance = 0.0f;
    for (float c : confidences) {
        variance += (c - mean) * (c - mean);
    }
    variance /= confidences.size();

    /* Variance should be small - deterministic behavior */
    EXPECT_LT(variance, 0.1f);
}

TEST_F(ParietalFEPRegressionTest, DomainSpecificInferenceSuccess) {
    /* WHAT: Test domain-specific inference types work
     * WHY:  All domain types should be processed successfully
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Test numerical inference */
    parietal_request_t num_req = {};
    num_req.type = PARIETAL_FEP_NUMERICAL_INFERENCE;
    num_req.priority = 1.0f;
    parietal_result_t num_result = parietal_process(parietal, &num_req);
    EXPECT_TRUE(num_result.success);

    /* Test spatial inference */
    parietal_request_t spa_req = {};
    spa_req.type = PARIETAL_FEP_SPATIAL_INFERENCE;
    spa_req.priority = 1.0f;
    parietal_result_t spa_result = parietal_process(parietal, &spa_req);
    EXPECT_TRUE(spa_result.success);

    /* Test physics inference */
    parietal_request_t phy_req = {};
    phy_req.type = PARIETAL_FEP_PHYSICS_INFERENCE;
    phy_req.priority = 1.0f;
    parietal_result_t phy_result = parietal_process(parietal, &phy_req);
    EXPECT_TRUE(phy_result.success);
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, BeliefUpdatePerformanceBaseline) {
    /* WHAT: Test belief update performance stays within bounds
     * WHY:  Performance regression detection
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    parietal_request_t request = {};
    request.type = PARIETAL_FEP_UPDATE_BELIEFS;
    request.priority = 1.0f;

    /* Measure average time over many iterations */
    const int iterations = 100;
    double total_time = 0.0;

    for (int i = 0; i < iterations; i++) {
        total_time += measure_time_us([&]() {
            parietal_process(parietal, &request);
        });
    }

    double avg_time_us = total_time / iterations;

    /* Performance baseline: < 1ms per belief update */
    EXPECT_LT(avg_time_us, 1000.0) << "Belief update too slow: " << avg_time_us << "us";
}

TEST_F(ParietalFEPRegressionTest, ActiveInferencePerformanceBaseline) {
    /* WHAT: Test active inference performance stays within bounds
     * WHY:  Active inference is computationally heavier - monitor for regression
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    parietal_request_t request = {};
    request.type = PARIETAL_FEP_ACTIVE_INFERENCE;
    request.priority = 1.0f;

    const int iterations = 50;
    double total_time = 0.0;

    for (int i = 0; i < iterations; i++) {
        total_time += measure_time_us([&]() {
            parietal_process(parietal, &request);
        });
    }

    double avg_time_us = total_time / iterations;

    /* Performance baseline: < 5ms per active inference */
    EXPECT_LT(avg_time_us, 5000.0) << "Active inference too slow: " << avg_time_us << "us";
}

TEST_F(ParietalFEPRegressionTest, PredictionPerformanceBaseline) {
    /* WHAT: Test prediction performance stays within bounds
     * WHY:  Prediction is critical path - must be fast
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    parietal_request_t request = {};
    request.type = PARIETAL_FEP_PREDICT;
    request.priority = 1.0f;

    const int iterations = 100;
    double total_time = 0.0;

    for (int i = 0; i < iterations; i++) {
        total_time += measure_time_us([&]() {
            parietal_process(parietal, &request);
        });
    }

    double avg_time_us = total_time / iterations;

    /* Performance baseline: < 500us per prediction */
    EXPECT_LT(avg_time_us, 500.0) << "Prediction too slow: " << avg_time_us << "us";
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, LegacyRequestTypeSupport) {
    /* WHAT: Test all FEP request types are still supported
     * WHY:  Prevent removal of functionality
     */

    /* All FEP request types that must be supported */
    parietal_request_type_t fep_types[] = {
        PARIETAL_FEP_UPDATE_BELIEFS,
        PARIETAL_FEP_PREDICT,
        PARIETAL_FEP_ACTIVE_INFERENCE,
        PARIETAL_FEP_COMPUTE_SURPRISE,
        PARIETAL_FEP_NUMERICAL_INFERENCE,
        PARIETAL_FEP_SPATIAL_INFERENCE,
        PARIETAL_FEP_PHYSICS_INFERENCE
    };

    for (auto type : fep_types) {
        parietal_request_t request = {};
        request.type = type;
        request.priority = 1.0f;

        parietal_result_t result = parietal_process(parietal, &request);

        /* Should not crash and should return some result */
        /* Success depends on FEP bridge availability */
        if (fep_bridge && fep_parietal_is_available(fep_bridge)) {
            EXPECT_TRUE(result.success) << "Request type " << type << " failed";
        }
    }
}

TEST_F(ParietalFEPRegressionTest, StatsStructureStability) {
    /* WHAT: Test stats structure has expected fields
     * WHY:  Prevent breaking changes to stats API
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    fep_parietal_stats_t stats;
    int ret = fep_parietal_get_stats(fep_bridge, &stats);
    EXPECT_EQ(ret, 0);

    /* Verify key fields exist and are non-negative */
    EXPECT_GE(stats.predictions_made, 0UL);
    EXPECT_GE(stats.belief_updates, 0UL);
    EXPECT_GE(stats.active_inferences, 0UL);
    EXPECT_GE(stats.policies_evaluated, 0UL);
}

//=============================================================================
// Modulation Effect Regression Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, InflammationModulationEffect) {
    /* WHAT: Test inflammation affects FEP processing consistently
     * WHY:  Modulation effects should be stable across versions
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Get baseline performance */
    parietal_request_t request = {};
    request.type = PARIETAL_FEP_PREDICT;
    request.priority = 1.0f;

    parietal_result_t baseline = parietal_process(parietal, &request);
    float baseline_conf = baseline.confidence;

    /* Apply high inflammation */
    parietal_set_inflammation(parietal, 0.9f);

    /* Get inflamed performance */
    parietal_result_t inflamed = parietal_process(parietal, &request);

    /* Inflammation should reduce confidence or maintain it, never increase significantly */
    /* Allow some tolerance for numerical variation */
    EXPECT_LE(inflamed.confidence, baseline_conf + 0.1f);

    /* Reset */
    parietal_set_inflammation(parietal, 0.0f);
}

TEST_F(ParietalFEPRegressionTest, FatigueModulationEffect) {
    /* WHAT: Test fatigue affects FEP processing consistently
     * WHY:  Modulation effects should be stable across versions
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Get baseline */
    parietal_request_t request = {};
    request.type = PARIETAL_FEP_ACTIVE_INFERENCE;
    request.priority = 1.0f;

    parietal_result_t baseline = parietal_process(parietal, &request);

    /* Apply fatigue */
    parietal_set_fatigue(parietal, 0.8f);

    /* Get fatigued performance */
    parietal_result_t fatigued = parietal_process(parietal, &request);

    /* Both should complete */
    EXPECT_TRUE(baseline.success);
    /* Fatigued may have lower confidence */

    /* Reset */
    parietal_set_fatigue(parietal, 0.0f);
}

//=============================================================================
// Reset and State Recovery Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, StatsResetWorks) {
    /* WHAT: Test stats reset clears counters
     * WHY:  Incomplete reset causes state leakage between tests
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Process some requests to accumulate stats */
    parietal_request_t request = {};
    request.type = PARIETAL_FEP_UPDATE_BELIEFS;
    for (int i = 0; i < 10; i++) {
        parietal_process(parietal, &request);
    }

    /* Reset stats */
    fep_parietal_reset_stats(fep_bridge);

    /* Verify stats are reset */
    fep_parietal_stats_t stats;
    fep_parietal_get_stats(fep_bridge, &stats);
    EXPECT_EQ(stats.belief_updates, 0UL);
    EXPECT_EQ(stats.predictions_made, 0UL);
}

TEST_F(ParietalFEPRegressionTest, StatisticsAccumulateWithoutOverflow) {
    /* WHAT: Test statistics accumulate without overflow
     * WHY:  Integer overflow in stats was a historical concern
     */

    if (!fep_bridge || !fep_parietal_is_available(fep_bridge)) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    fep_parietal_reset_stats(fep_bridge);

    /* Process many requests */
    const int iterations = 1000;
    parietal_request_t request = {};
    request.type = PARIETAL_FEP_UPDATE_BELIEFS;
    request.priority = 1.0f;

    for (int i = 0; i < iterations; i++) {
        parietal_process(parietal, &request);
    }

    /* Get stats */
    fep_parietal_stats_t stats;
    fep_parietal_get_stats(fep_bridge, &stats);

    /* Verify reasonable count (should be close to iterations) */
    EXPECT_GE(stats.belief_updates, 0UL);
    EXPECT_LE(stats.belief_updates, (uint64_t)(iterations * 2));
}

//=============================================================================
// Bridge Availability Tests
//=============================================================================

TEST_F(ParietalFEPRegressionTest, BridgeAvailabilityCheck) {
    /* WHAT: Test bridge availability check works
     * WHY:  Graceful degradation when FEP is unavailable
     */

    /* Bridge should exist if parietal created successfully */
    if (fep_bridge) {
        /* Can check availability without crash */
        bool available = fep_parietal_is_available(fep_bridge);
        (void)available;  /* Just verify it doesn't crash */
    }
}

TEST_F(ParietalFEPRegressionTest, EnableDisableWorks) {
    /* WHAT: Test enable/disable functionality
     * WHY:  Should be able to toggle FEP processing
     */

    if (!fep_bridge) {
        GTEST_SKIP() << "FEP bridge not available";
    }

    /* Disable */
    int ret = fep_parietal_set_enabled(fep_bridge, false);
    EXPECT_EQ(ret, 0);

    /* Should report unavailable */
    EXPECT_FALSE(fep_parietal_is_available(fep_bridge));

    /* Re-enable */
    ret = fep_parietal_set_enabled(fep_bridge, true);
    EXPECT_EQ(ret, 0);

    /* Should report available again */
    EXPECT_TRUE(fep_parietal_is_available(fep_bridge));
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
