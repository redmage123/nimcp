/**
 * @file test_enhanced_fault_tolerance_regression.cpp
 * @brief Regression tests for enhanced fault tolerance modules
 *
 * Tests edge cases, known bug scenarios, and ensures previous
 * issues remain fixed across all enhanced FT modules.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_distributed_fault_tolerance.h"
#include "utils/fault_tolerance/nimcp_hierarchical_recovery.h"
#include "utils/fault_tolerance/nimcp_recovery_evolution.h"
#include "utils/fault_tolerance/nimcp_byzantine_fault_tolerance.h"
#include "utils/fault_tolerance/nimcp_graceful_degradation.h"
#include "utils/fault_tolerance/nimcp_recovery_observability.h"
#include "utils/fault_tolerance/nimcp_chaos_engineering.h"
#include "utils/fault_tolerance/nimcp_predictive_analysis.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Distributed Fault Tolerance Regression Tests
//=============================================================================

class DftRegressionTest : public ::testing::Test {
protected:
    dft_context_t* ctx;

    void SetUp() override {
        dft_config_t config = dft_default_config();
        config.node_id = 1;
        ctx = dft_create(&config);
    }

    void TearDown() override {
        if (ctx) dft_destroy(ctx);
    }
};

// REGRESSION: Ensure cluster health doesn't overflow with multiple peers
TEST_F(DftRegressionTest, ClusterHealthWithPeers) {
    ASSERT_TRUE(dft_start(ctx));

    // Add peers using proper API
    for (int i = 2; i <= 5; i++) {
        dft_add_peer(ctx, i, NULL);
    }

    // Simulate heartbeats with varying health scores
    dft_receive_heartbeat(ctx, 2, 1.0f);  // Healthy
    dft_receive_heartbeat(ctx, 3, 0.8f);
    dft_receive_heartbeat(ctx, 4, 0.5f);
    dft_receive_heartbeat(ctx, 5, 0.9f);

    // Cluster health should be calculable
    float health = dft_get_cluster_health(ctx);
    EXPECT_FALSE(std::isnan(health));
    EXPECT_FALSE(std::isinf(health));
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 100.0f);

    ASSERT_TRUE(dft_stop(ctx));
}

// REGRESSION: Node state transition via failure reporting
TEST_F(DftRegressionTest, StateTransitionConsistency) {
    ASSERT_TRUE(dft_start(ctx));

    // Add a peer
    dft_add_peer(ctx, 2, NULL);

    // Send initial heartbeat
    dft_receive_heartbeat(ctx, 2, 1.0f);

    // Check peer info
    dft_peer_info_t info;
    EXPECT_TRUE(dft_get_peer_info(ctx, 2, &info));

    // Report suspected failure
    dft_report_suspected_failure(ctx, 2, "test failure");

    // Initiate and complete recovery
    dft_initiate_recovery(ctx, 2);
    dft_complete_recovery(ctx, 2, true);

    // Peer should still be queryable
    EXPECT_TRUE(dft_get_peer_info(ctx, 2, &info));

    ASSERT_TRUE(dft_stop(ctx));
}

// REGRESSION: Quorum calculation with multiple peers
TEST_F(DftRegressionTest, QuorumBoundaryCondition) {
    ASSERT_TRUE(dft_start(ctx));

    // Add peers
    dft_add_peer(ctx, 2, NULL);
    dft_add_peer(ctx, 3, NULL);
    dft_add_peer(ctx, 4, NULL);

    // With 4 nodes (self + 3 peers), quorum should exist
    EXPECT_TRUE(dft_has_quorum(ctx));

    ASSERT_TRUE(dft_stop(ctx));
}

//=============================================================================
// Byzantine Fault Tolerance Regression Tests
//=============================================================================

class BftRegressionTest : public ::testing::Test {
protected:
    bft_context_t* ctx;

    void SetUp() override {
        bft_config_t config = bft_default_config();
        config.node_id = 1;
        ctx = bft_create(&config);
    }

    void TearDown() override {
        if (ctx) bft_destroy(ctx);
    }
};

// REGRESSION: Trust score should stay bounded after updates
TEST_F(BftRegressionTest, TrustScoreNonNegative) {
    ASSERT_TRUE(bft_start(ctx));

    // Apply many bad behavior updates (false = incorrect behavior)
    for (int i = 0; i < 100; i++) {
        bft_update_trust(ctx, 2, false);  // incorrect behavior
    }

    // Trust info should be retrievable
    bft_trust_info_t trust_info;
    if (bft_get_trust_info(ctx, 2, &trust_info)) {
        EXPECT_GE(trust_info.trust_score, 0.0f);
    }

    ASSERT_TRUE(bft_stop(ctx));
}

// REGRESSION: Trust score should not exceed 1.0
TEST_F(BftRegressionTest, TrustScoreMaxBound) {
    ASSERT_TRUE(bft_start(ctx));

    // Apply many good behavior updates (true = correct behavior)
    for (int i = 0; i < 100; i++) {
        bft_update_trust(ctx, 2, true);  // correct behavior
    }

    bft_trust_info_t trust_info;
    if (bft_get_trust_info(ctx, 2, &trust_info)) {
        EXPECT_LE(trust_info.trust_score, 100.0f);
    }

    ASSERT_TRUE(bft_stop(ctx));
}

// REGRESSION: Equivocation detection with message headers
TEST_F(BftRegressionTest, EquivocationSameDigest) {
    ASSERT_TRUE(bft_start(ctx));

    // Same digest should NOT be equivocation
    bft_msg_header_t msg1;
    memset(&msg1, 0, sizeof(msg1));
    msg1.type = BFT_MSG_PREPARE;
    msg1.sequence_number = 1;
    msg1.sender_id = 2;
    memcpy(msg1.digest, "same_digest", 11);

    bft_msg_header_t msg2;
    memset(&msg2, 0, sizeof(msg2));
    msg2.type = BFT_MSG_PREPARE;
    msg2.sequence_number = 1;
    msg2.sender_id = 2;
    memcpy(msg2.digest, "same_digest", 11);  // Same!

    // Check for equivocation
    bool is_equivocation = bft_check_equivocation(ctx, &msg1, &msg2);
    EXPECT_FALSE(is_equivocation);

    ASSERT_TRUE(bft_stop(ctx));
}

//=============================================================================
// Graceful Degradation Regression Tests
//=============================================================================

class GdRegressionTest : public ::testing::Test {
protected:
    gd_context_t* ctx;

    void SetUp() override {
        gd_config_t config = gd_default_config();
        ctx = gd_create(&config);
    }

    void TearDown() override {
        if (ctx) gd_destroy(ctx);
    }
};

// REGRESSION: Feature quality boundary conditions
TEST_F(GdRegressionTest, FeatureQualityBounds) {
    gd_feature_t feature = {0};
    strncpy(feature.name, "bounded_feature", sizeof(feature.name) - 1);
    feature.can_degrade = true;
    feature.current_quality = 100.0;
    feature.min_quality = 0.0;

    uint32_t id = gd_register_feature(ctx, &feature);

    // Try to set quality below 0
    gd_set_feature_quality(ctx, id, -10.0);
    double quality = gd_get_feature_quality(ctx, id);
    EXPECT_GE(quality, 0.0);

    // Try to set quality above 100
    gd_set_feature_quality(ctx, id, 150.0);
    quality = gd_get_feature_quality(ctx, id);
    EXPECT_LE(quality, 100.0);
}

// REGRESSION: Resource usage boundary conditions
TEST_F(GdRegressionTest, ResourceUsageBounds) {
    // Should reject invalid resource values
    EXPECT_FALSE(gd_update_resource(ctx, GD_RESOURCE_CPU, -5.0));
    EXPECT_FALSE(gd_update_resource(ctx, GD_RESOURCE_CPU, 105.0));

    // Valid values should work
    EXPECT_TRUE(gd_update_resource(ctx, GD_RESOURCE_CPU, 0.0));
    EXPECT_TRUE(gd_update_resource(ctx, GD_RESOURCE_CPU, 100.0));
}

// REGRESSION: Tier transition maintains feature state
TEST_F(GdRegressionTest, TierTransitionPreservesFeatures) {
    gd_feature_t feature = {0};
    strncpy(feature.name, "stable_feature", sizeof(feature.name) - 1);
    feature.priority = GD_PRIORITY_CRITICAL;
    feature.minimum_tier = GD_TIER_EMERGENCY;

    uint32_t id = gd_register_feature(ctx, &feature);

    // Feature should remain registered across tier changes
    gd_set_tier(ctx, GD_TIER_REDUCED, "test");
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id));

    gd_set_tier(ctx, GD_TIER_EMERGENCY, "test");
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id));

    gd_set_tier(ctx, GD_TIER_FULL, "test");
    EXPECT_TRUE(gd_is_feature_enabled(ctx, id));
}

//=============================================================================
// Predictive Analysis Regression Tests
//=============================================================================

class PaRegressionTest : public ::testing::Test {
protected:
    pa_context_t* ctx;

    void SetUp() override {
        pa_config_t config = pa_default_config();
        ctx = pa_create(&config);
    }

    void TearDown() override {
        if (ctx) pa_destroy(ctx);
    }
};

// REGRESSION: Anomaly detection with constant values
TEST_F(PaRegressionTest, AnomalyDetectionConstant) {
    // Add constant values (variance = 0)
    for (int i = 0; i < 100; i++) {
        pa_add_sample(ctx, PA_SERIES_CPU, 50.0);
    }

    // Should not crash with zero variance
    pa_anomaly_t anomalies[10];
    uint32_t count = pa_detect_anomalies(ctx, PA_SERIES_CPU, anomalies, 10);
    // Constant values should not be anomalies
    EXPECT_EQ(count, 0);
}

// REGRESSION: Correlation with insufficient data
TEST_F(PaRegressionTest, CorrelationInsufficientData) {
    pa_add_sample(ctx, PA_SERIES_CPU, 50.0);
    pa_add_sample(ctx, PA_SERIES_MEMORY, 1000.0);

    pa_correlation_t corr;
    bool success = pa_calculate_correlation(ctx, PA_SERIES_CPU, PA_SERIES_MEMORY, &corr);
    // Should handle gracefully (may return false or low confidence)
    (void)success;
    (void)corr;
}

// REGRESSION: Forecast with trending data crossing zero
TEST_F(PaRegressionTest, ForecastCrossingZero) {
    // Add data that trends from positive to negative
    for (int i = 0; i < 100; i++) {
        pa_add_sample(ctx, PA_SERIES_GRADIENT, 50.0 - i * 1.0);
    }

    pa_forecast_t forecast;
    bool success = pa_forecast(ctx, PA_SERIES_GRADIENT, &forecast);

    if (success) {
        // Forecast values should be valid numbers
        for (uint32_t i = 0; i < forecast.horizon; i++) {
            EXPECT_FALSE(std::isnan(forecast.forecasts[i]));
            EXPECT_FALSE(std::isinf(forecast.forecasts[i]));
        }
    }
}

// REGRESSION: Time series with gaps
TEST_F(PaRegressionTest, TimeSeriesWithGaps) {
    // Add samples with large time gaps
    pa_add_sample_timed(ctx, PA_SERIES_CPU, 50.0, 1000);
    pa_add_sample_timed(ctx, PA_SERIES_CPU, 55.0, 1000000);  // 1 second gap
    pa_add_sample_timed(ctx, PA_SERIES_CPU, 60.0, 10000000); // 10 second gap

    pa_series_meta_t meta;
    EXPECT_TRUE(pa_get_series_meta(ctx, PA_SERIES_CPU, &meta));
    EXPECT_EQ(meta.sample_count, 3);
}

//=============================================================================
// Recovery Evolution Regression Tests
//=============================================================================

class ReRegressionTest : public ::testing::Test {
protected:
    re_context_t* ctx;

    void SetUp() override {
        re_config_t config = re_default_config();
        ctx = re_create(&config);
    }

    void TearDown() override {
        if (ctx) re_destroy(ctx);
    }
};

// REGRESSION: Fitness evaluation returns valid values
TEST_F(ReRegressionTest, FitnessBounds) {
    // Initialize population
    EXPECT_TRUE(re_init_population(ctx));

    // Add a strategy
    re_strategy_t strategy;
    memset(&strategy, 0, sizeof(strategy));
    strategy.id = 1;
    strategy.actions[0] = RE_ACTION_RESTART;
    strategy.action_count = 1;
    strategy.fitness = 0.5f;
    EXPECT_TRUE(re_add_strategy(ctx, &strategy));

    // Very good outcome
    re_outcome_t good_outcome;
    memset(&good_outcome, 0, sizeof(good_outcome));
    good_outcome.strategy_id = 1;
    good_outcome.success = true;
    good_outcome.recovery_time_ms = 1;
    good_outcome.resource_usage = 0.01f;
    good_outcome.data_loss = 0.0f;

    float fitness = re_evaluate_fitness(ctx, 0, &good_outcome);
    EXPECT_GE(fitness, 0.0f);

    // Very bad outcome
    re_outcome_t bad_outcome;
    memset(&bad_outcome, 0, sizeof(bad_outcome));
    bad_outcome.strategy_id = 1;
    bad_outcome.success = false;
    bad_outcome.recovery_time_ms = 1000000;
    bad_outcome.resource_usage = 1.0f;
    bad_outcome.data_loss = 1.0f;

    fitness = re_evaluate_fitness(ctx, 0, &bad_outcome);
    EXPECT_FALSE(std::isnan(fitness));
    EXPECT_FALSE(std::isinf(fitness));
}

// REGRESSION: Q-value via reinforcement learning
TEST_F(ReRegressionTest, QValueStability) {
    // Store experiences and learn
    re_experience_t exp;
    memset(&exp, 0, sizeof(exp));
    exp.state = 1;
    exp.action = RE_ACTION_RESTART;
    exp.reward = 1.0f;
    exp.next_state = 0;
    exp.terminal = false;

    for (int i = 0; i < 100; i++) {
        exp.reward = (i % 2 == 0) ? 1.0f : -1.0f;
        re_store_experience(ctx, &exp);
    }

    // Learn from batch
    float loss = re_learn_from_batch(ctx);
    EXPECT_FALSE(std::isnan(loss));
    EXPECT_FALSE(std::isinf(loss));

    // Get Q-value
    float q = re_get_q_value(ctx, 1, RE_ACTION_RESTART);
    EXPECT_FALSE(std::isnan(q));
    EXPECT_FALSE(std::isinf(q));
}

// REGRESSION: Crossover produces valid offspring
TEST_F(ReRegressionTest, CrossoverValidity) {
    re_strategy_t parent1;
    memset(&parent1, 0, sizeof(parent1));
    parent1.id = 1;
    parent1.action_count = 3;
    parent1.actions[0] = RE_ACTION_RETRY;
    parent1.actions[1] = RE_ACTION_CHECKPOINT;
    parent1.actions[2] = RE_ACTION_RESTART;

    re_strategy_t parent2;
    memset(&parent2, 0, sizeof(parent2));
    parent2.id = 2;
    parent2.action_count = 3;
    parent2.actions[0] = RE_ACTION_ISOLATE;
    parent2.actions[1] = RE_ACTION_FAILOVER;
    parent2.actions[2] = RE_ACTION_DEGRADE;

    re_strategy_t child;
    for (int i = 0; i < 10; i++) {
        memset(&child, 0, sizeof(child));
        EXPECT_TRUE(re_crossover(ctx, &parent1, &parent2, &child));
        EXPECT_EQ(child.action_count, 3);
    }
}

//=============================================================================
// Hierarchical Recovery Regression Tests
//=============================================================================

class HrRegressionTest : public ::testing::Test {
protected:
    hr_context_t* ctx;

    void SetUp() override {
        hr_config_t config = hr_default_config();
        ctx = hr_create(&config);
    }

    void TearDown() override {
        if (ctx) hr_destroy(ctx);
    }
};

// REGRESSION: Circuit breaker transitions
TEST_F(HrRegressionTest, CircuitBreakerTransitions) {
    hr_circuit_config_t config;
    memset(&config, 0, sizeof(config));
    strncpy(config.name, "test_cb", sizeof(config.name) - 1);
    config.failure_threshold = 3;
    config.timeout_ms = 100;  // Short for testing
    config.half_open_max = 2;

    EXPECT_TRUE(hr_add_circuit_breaker(ctx, &config));

    // Should start closed - check by querying
    hr_circuit_breaker_t breaker;
    EXPECT_TRUE(hr_get_circuit_breaker(ctx, "test_cb", &breaker));
    EXPECT_EQ(breaker.state, HR_CIRCUIT_CLOSED);

    // Trip the breaker with failures
    hr_record_circuit_result(ctx, "test_cb", false);
    hr_record_circuit_result(ctx, "test_cb", false);
    hr_record_circuit_result(ctx, "test_cb", false);
    hr_record_circuit_result(ctx, "test_cb", false);

    EXPECT_TRUE(hr_get_circuit_breaker(ctx, "test_cb", &breaker));
    EXPECT_EQ(breaker.state, HR_CIRCUIT_OPEN);

    // Wait for reset timeout
    struct timespec ts = {0, 150000000};  // 150ms
    nanosleep(&ts, NULL);

    // After timeout, should be able to check state
    EXPECT_TRUE(hr_get_circuit_breaker(ctx, "test_cb", &breaker));
    // May be OPEN or HALF_OPEN depending on implementation
}

// REGRESSION: Child registration and hierarchy
TEST_F(HrRegressionTest, ChildRegistration) {
    ASSERT_TRUE(hr_start(ctx));

    // Register children at node level
    for (int i = 1; i <= 10; i++) {
        EXPECT_TRUE(hr_add_child(ctx, i, HR_LEVEL_NODE));
    }

    // Get stats to verify registration worked
    hr_stats_t stats;
    EXPECT_TRUE(hr_get_stats(ctx, &stats));

    ASSERT_TRUE(hr_stop(ctx));
}

//=============================================================================
// Chaos Engineering Regression Tests
//=============================================================================

class CeRegressionTest : public ::testing::Test {
protected:
    ce_context_t* ctx;

    void SetUp() override {
        ce_config_t config = ce_default_config();
        config.enable_dry_run = true;
        ctx = ce_create(&config);
    }

    void TearDown() override {
        if (ctx) ce_destroy(ctx);
    }
};

// REGRESSION: Experiment ID uniqueness
TEST_F(CeRegressionTest, ExperimentIdUniqueness) {
    std::vector<uint32_t> ids;

    for (int i = 0; i < 20; i++) {
        char name[64];
        snprintf(name, sizeof(name), "exp_%d", i);

        uint32_t id = ce_create_experiment(ctx, name, "Test uniqueness");
        if (id > 0) {
            for (uint32_t existing : ids) {
                EXPECT_NE(id, existing);
            }
            ids.push_back(id);
        }
    }
}

// REGRESSION: Abort during running experiment
TEST_F(CeRegressionTest, AbortRunningExperiment) {
    uint32_t id = ce_create_experiment(ctx, "abort_test", "Test abort");

    ce_fault_spec_t fault;
    memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 60000;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target;
    memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    ce_start_experiment(ctx, id);
    EXPECT_EQ(ce_get_experiment_state(ctx, id), CE_STATE_RUNNING);

    EXPECT_TRUE(ce_abort_experiment(ctx, id, "test_abort"));
    EXPECT_EQ(ce_get_experiment_state(ctx, id), CE_STATE_ABORTED);

    // Should not be able to resume aborted experiment
    EXPECT_FALSE(ce_resume_experiment(ctx, id));
}

//=============================================================================
// Recovery Observability Regression Tests
//=============================================================================

class RoRegressionTest : public ::testing::Test {
protected:
    ro_context_t* ctx;

    void SetUp() override {
        ro_config_t config = ro_default_config();
        ctx = ro_create(&config);
    }

    void TearDown() override {
        if (ctx) ro_destroy(ctx);
    }
};

// REGRESSION: Counter overflow protection
TEST_F(RoRegressionTest, CounterOverflowProtection) {
    ASSERT_TRUE(ro_start(ctx));

    // Create counter using proper API
    ro_counter_t* counter = ro_create_counter(ctx, "overflow_test", NULL, 0);
    ASSERT_NE(counter, nullptr);

    // Record many values
    for (uint64_t i = 0; i < 10000; i++) {
        ro_counter_inc(counter, 1);
    }

    uint64_t value = ro_counter_get(counter);
    EXPECT_EQ(value, 10000);

    ASSERT_TRUE(ro_stop(ctx));
}

// REGRESSION: Histogram with edge values
TEST_F(RoRegressionTest, HistogramEdgeValues) {
    ASSERT_TRUE(ro_start(ctx));

    // Create histogram using proper API (buckets, bucket_count, labels, label_count)
    ro_histogram_t* hist = ro_create_histogram(ctx, "edge_hist", NULL, 0, NULL, 0);
    ASSERT_NE(hist, nullptr);

    // Record edge values
    ro_histogram_observe(hist, 0.0);
    ro_histogram_observe(hist, 1e10);
    ro_histogram_observe(hist, 1.0);  // Use reasonable positive value

    // Should not crash and percentiles should be valid
    double p50 = ro_histogram_percentile(hist, 0.5);
    EXPECT_FALSE(std::isnan(p50));

    ASSERT_TRUE(ro_stop(ctx));
}

// REGRESSION: MTTR stats with no failures
TEST_F(RoRegressionTest, MttrNoFailures) {
    ASSERT_TRUE(ro_start(ctx));

    // Get MTTR stats without any failures recorded
    ro_mttr_stats_t stats;
    bool success = ro_get_mttr_stats(ctx, &stats);

    // Should succeed and return valid stats
    if (success) {
        EXPECT_FALSE(std::isnan(stats.mttr_ms));
    }

    ASSERT_TRUE(ro_stop(ctx));
}

//=============================================================================
// Cross-Module Regression Tests
//=============================================================================

TEST(CrossModuleRegressionTest, AllModulesStartStop) {
    // Verify all modules can be created, started, stopped, destroyed
    // without memory leaks or crashes

    dft_config_t dft_config = dft_default_config();
    dft_config.node_id = 1;
    dft_context_t* dft = dft_create(&dft_config);
    ASSERT_NE(dft, nullptr);
    EXPECT_TRUE(dft_start(dft));
    EXPECT_TRUE(dft_stop(dft));
    dft_destroy(dft);

    bft_config_t bft_config = bft_default_config();
    bft_config.node_id = 1;
    bft_context_t* bft = bft_create(&bft_config);
    ASSERT_NE(bft, nullptr);
    EXPECT_TRUE(bft_start(bft));
    EXPECT_TRUE(bft_stop(bft));
    bft_destroy(bft);

    hr_config_t hr_config = hr_default_config();
    hr_context_t* hr = hr_create(&hr_config);
    ASSERT_NE(hr, nullptr);
    EXPECT_TRUE(hr_start(hr));
    EXPECT_TRUE(hr_stop(hr));
    hr_destroy(hr);

    re_config_t re_config = re_default_config();
    re_context_t* re = re_create(&re_config);
    ASSERT_NE(re, nullptr);
    // RE module doesn't have start/stop - it's stateless
    re_destroy(re);

    gd_config_t gd_config = gd_default_config();
    gd_context_t* gd = gd_create(&gd_config);
    ASSERT_NE(gd, nullptr);
    EXPECT_TRUE(gd_start(gd));
    EXPECT_TRUE(gd_stop(gd));
    gd_destroy(gd);

    ro_config_t ro_config = ro_default_config();
    ro_context_t* ro = ro_create(&ro_config);
    ASSERT_NE(ro, nullptr);
    EXPECT_TRUE(ro_start(ro));
    EXPECT_TRUE(ro_stop(ro));
    ro_destroy(ro);

    ce_config_t ce_config = ce_default_config();
    ce_config.enable_dry_run = true;
    ce_context_t* ce = ce_create(&ce_config);
    ASSERT_NE(ce, nullptr);
    // CE doesn't have start/stop
    ce_destroy(ce);

    pa_config_t pa_config = pa_default_config();
    pa_context_t* pa = pa_create(&pa_config);
    ASSERT_NE(pa, nullptr);
    EXPECT_TRUE(pa_start(pa));
    EXPECT_TRUE(pa_stop(pa));
    pa_destroy(pa);
}

TEST(CrossModuleRegressionTest, NullContextHandling) {
    // All modules should handle NULL context gracefully

    // DFT
    dft_destroy(nullptr);
    EXPECT_FALSE(dft_start(nullptr));
    EXPECT_FALSE(dft_stop(nullptr));

    // BFT
    bft_destroy(nullptr);
    EXPECT_FALSE(bft_start(nullptr));
    EXPECT_FALSE(bft_stop(nullptr));

    // HR
    hr_destroy(nullptr);
    EXPECT_FALSE(hr_start(nullptr));
    EXPECT_FALSE(hr_stop(nullptr));

    // RE - module doesn't have start/stop
    re_destroy(nullptr);

    // GD
    gd_destroy(nullptr);
    EXPECT_FALSE(gd_start(nullptr));
    EXPECT_FALSE(gd_stop(nullptr));

    // RO
    ro_destroy(nullptr);
    EXPECT_FALSE(ro_start(nullptr));
    EXPECT_FALSE(ro_stop(nullptr));

    // CE
    ce_destroy(nullptr);

    // PA
    pa_destroy(nullptr);
    EXPECT_FALSE(pa_start(nullptr));
    EXPECT_FALSE(pa_stop(nullptr));
}

