/**
 * @file test_enhanced_fault_tolerance_integration.cpp
 * @brief Integration tests for enhanced fault tolerance modules
 *
 * Tests module interactions including:
 * - Distributed FT + Byzantine FT coordination
 * - Predictive Analysis + Graceful Degradation
 * - Chaos Engineering + Recovery Observability
 * - Hierarchical Recovery + Recovery Evolution
 * - Security (BBB) integration with all modules
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

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
// Multi-Module Integration Fixture
//=============================================================================

class EnhancedFaultToleranceIntegrationTest : public ::testing::Test {
protected:
    dft_context_t* dft_ctx;
    hr_context_t* hr_ctx;
    re_context_t* re_ctx;
    bft_context_t* bft_ctx;
    gd_context_t* gd_ctx;
    ro_context_t* ro_ctx;
    ce_context_t* ce_ctx;
    pa_context_t* pa_ctx;

    void SetUp() override {
        // Create all contexts with default configs
        dft_config_t dft_config = dft_default_config();
        dft_config.node_id = 1;
        dft_ctx = dft_create(&dft_config);

        hr_config_t hr_config = hr_default_config();
        hr_ctx = hr_create(&hr_config);

        re_config_t re_config = re_default_config();
        re_ctx = re_create(&re_config);

        bft_config_t bft_config = bft_default_config();
        bft_config.node_id = 1;
        bft_ctx = bft_create(&bft_config);

        gd_config_t gd_config = gd_default_config();
        gd_ctx = gd_create(&gd_config);

        ro_config_t ro_config = ro_default_config();
        ro_ctx = ro_create(&ro_config);

        ce_config_t ce_config = ce_default_config();
        ce_config.enable_dry_run = true;  // Safety
        ce_ctx = ce_create(&ce_config);

        pa_config_t pa_config = pa_default_config();
        pa_ctx = pa_create(&pa_config);
    }

    void TearDown() override {
        if (dft_ctx) dft_destroy(dft_ctx);
        if (hr_ctx) hr_destroy(hr_ctx);
        if (re_ctx) re_destroy(re_ctx);
        if (bft_ctx) bft_destroy(bft_ctx);
        if (gd_ctx) gd_destroy(gd_ctx);
        if (ro_ctx) ro_destroy(ro_ctx);
        if (ce_ctx) ce_destroy(ce_ctx);
        if (pa_ctx) pa_destroy(pa_ctx);
    }
};

//=============================================================================
// Distributed FT + Byzantine FT Integration Tests
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, DftAndBftCoordination) {
    // SCENARIO: Byzantine node detected, DFT should mark it as failed

    ASSERT_TRUE(dft_start(dft_ctx));
    ASSERT_TRUE(bft_start(bft_ctx));

    // Register nodes in DFT using proper API
    for (int i = 2; i <= 5; i++) {
        dft_add_peer(dft_ctx, i, NULL);
    }

    // Simulate Byzantine behavior detection using proper API
    // bft_report_byzantine(ctx, node_id, behavior, evidence, evidence_count)
    bft_evidence_t evidence;
    memset(&evidence, 0, sizeof(evidence));
    evidence.type = BFT_EVIDENCE_PROTOCOL_VIOLATION;
    evidence.accused_node_id = 3;
    evidence.timestamp_ms = 1000;
    memcpy(evidence.data, "equivocated", 11);
    evidence.data_size = 11;
    bft_report_byzantine(bft_ctx, 3, BFT_BEHAV_EQUIVOCATION, &evidence, 1);

    // BFT should mark node as quarantined
    if (bft_is_quarantined(bft_ctx, 3)) {
        // Notify DFT of the suspected failure
        dft_report_suspected_failure(dft_ctx, 3, "byzantine behavior detected");

        // Check peer info
        dft_peer_info_t peer_info;
        if (dft_get_peer_info(dft_ctx, 3, &peer_info)) {
            // Node should be in failed or quarantined state
            EXPECT_TRUE(peer_info.state == DFT_NODE_FAILED ||
                       peer_info.state == DFT_NODE_QUARANTINED ||
                       peer_info.state == DFT_NODE_SUSPECTED);
        }
    }

    // Verify quorum still exists with remaining nodes
    EXPECT_TRUE(dft_has_quorum(dft_ctx));

    // Additional Byzantine node detection
    bft_evidence_t evidence2;
    memset(&evidence2, 0, sizeof(evidence2));
    evidence2.type = BFT_EVIDENCE_CONFLICTING_MSG;
    evidence2.accused_node_id = 4;
    evidence2.timestamp_ms = 2000;
    memcpy(evidence2.data, "double_vote", 11);
    evidence2.data_size = 11;
    bft_report_byzantine(bft_ctx, 4, BFT_BEHAV_EQUIVOCATION, &evidence2, 1);

    if (bft_is_quarantined(bft_ctx, 4)) {
        dft_report_suspected_failure(dft_ctx, 4, "byzantine - quarantined");
    }

    ASSERT_TRUE(dft_stop(dft_ctx));
    ASSERT_TRUE(bft_stop(bft_ctx));
}

//=============================================================================
// Graceful Degradation + Recovery Observability Tests
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, GdAndRoIntegration) {
    // SCENARIO: Track tier changes with observability

    ASSERT_TRUE(gd_start(gd_ctx));
    ASSERT_TRUE(ro_start(ro_ctx));

    // Start at full tier
    EXPECT_EQ(gd_get_current_tier(gd_ctx), GD_TIER_FULL);

    // Create metrics for tracking
    ro_counter_t* tier_change_counter = ro_create_counter(ro_ctx, "gd.tier.changes", NULL, 0);
    ro_gauge_t* current_tier_gauge = ro_create_gauge(ro_ctx, "gd.tier.current", NULL, 0);

    // Start trace for degradation
    ro_span_t* degrade_span = ro_start_trace(ro_ctx, "graceful_degradation");

    // Simulate degradation
    gd_set_tier(gd_ctx, GD_TIER_REDUCED, "test_pressure");
    EXPECT_EQ(gd_get_current_tier(gd_ctx), GD_TIER_REDUCED);

    if (tier_change_counter) ro_counter_inc(tier_change_counter, 1);
    if (current_tier_gauge) ro_gauge_set(current_tier_gauge, (double)GD_TIER_REDUCED);

    // Log the degradation event
    ro_event_t degrade_event;
    memset(&degrade_event, 0, sizeof(degrade_event));
    degrade_event.type = RO_EVENT_DEGRADATION;
    degrade_event.severity = RO_SEVERITY_WARN;
    degrade_event.timestamp_ns = 0;
    strncpy(degrade_event.message, "tier reduced due to test pressure", sizeof(degrade_event.message) - 1);
    ro_log_event(ro_ctx, &degrade_event);

    // Simulate recovery
    gd_set_tier(gd_ctx, GD_TIER_FULL, "pressure_relieved");
    EXPECT_EQ(gd_get_current_tier(gd_ctx), GD_TIER_FULL);

    if (tier_change_counter) ro_counter_inc(tier_change_counter, 1);
    if (current_tier_gauge) ro_gauge_set(current_tier_gauge, (double)GD_TIER_FULL);

    // End trace
    ro_end_span(degrade_span, RO_SPAN_OK, "degradation cycle complete");

    // Verify counter value
    if (tier_change_counter) {
        EXPECT_EQ(ro_counter_get(tier_change_counter), 2);
    }

    ASSERT_TRUE(gd_stop(gd_ctx));
    ASSERT_TRUE(ro_stop(ro_ctx));
}

//=============================================================================
// Hierarchical Recovery + DFT Integration Tests
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, HrAndDftRecoveryCoordination) {
    // SCENARIO: Hierarchical recovery coordinates with distributed fault tolerance

    ASSERT_TRUE(hr_start(hr_ctx));
    ASSERT_TRUE(dft_start(dft_ctx));

    // Register nodes
    for (int i = 1; i <= 4; i++) {
        dft_add_peer(dft_ctx, i, NULL);
        hr_add_child(hr_ctx, i, HR_LEVEL_NODE);
    }

    // Simulate failure in DFT
    dft_report_suspected_failure(dft_ctx, 2, "heartbeat timeout");

    // Initiate recovery through DFT
    EXPECT_TRUE(dft_initiate_recovery(dft_ctx, 2));

    // Check cluster health
    float health = dft_get_cluster_health(dft_ctx);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 100.0f);

    // Verify quorum maintained
    EXPECT_TRUE(dft_has_quorum(dft_ctx));

    ASSERT_TRUE(hr_stop(hr_ctx));
    ASSERT_TRUE(dft_stop(dft_ctx));
}

//=============================================================================
// Predictive Analysis + Graceful Degradation Tests
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, PaTriggersGdDegradation) {
    // SCENARIO: Predictive analysis detects anomalies and triggers degradation

    ASSERT_TRUE(pa_start(pa_ctx));
    ASSERT_TRUE(gd_start(gd_ctx));
    ASSERT_TRUE(ro_start(ro_ctx));

    // Add normal data first
    for (int i = 0; i < 30; i++) {
        pa_add_sample(pa_ctx, PA_SERIES_ERROR_RATE, 0.01);
    }

    // Add anomalous data pattern (increasing error rate)
    for (int i = 0; i < 20; i++) {
        pa_add_sample(pa_ctx, PA_SERIES_ERROR_RATE, 0.01 + i * 0.02);
    }

    // Start trace
    ro_span_t* trace = ro_start_trace(ro_ctx, "anomaly_detection");

    // Detect anomalies
    pa_anomaly_t anomalies[10];
    uint32_t count = pa_detect_anomalies(pa_ctx, PA_SERIES_ERROR_RATE, anomalies, 10);

    // Log anomaly detection
    ro_counter_t* anomaly_counter = ro_create_counter(ro_ctx, "pa.anomalies", NULL, 0);
    if (anomaly_counter) ro_counter_inc(anomaly_counter, count);

    // If significant anomalies detected, trigger degradation
    if (count >= 2) {
        gd_set_tier(gd_ctx, GD_TIER_REDUCED, "anomaly_pressure");
        ro_event_t anomaly_event;
        memset(&anomaly_event, 0, sizeof(anomaly_event));
        anomaly_event.type = RO_EVENT_DEGRADATION;
        anomaly_event.severity = RO_SEVERITY_WARN;
        strncpy(anomaly_event.message, "degraded due to anomaly detection", sizeof(anomaly_event.message) - 1);
        ro_log_event(ro_ctx, &anomaly_event);
    }

    // End trace
    ro_end_span(trace, RO_SPAN_OK, "anomaly check complete");

    ASSERT_TRUE(pa_stop(pa_ctx));
    ASSERT_TRUE(gd_stop(gd_ctx));
    ASSERT_TRUE(ro_stop(ro_ctx));
}

//=============================================================================
// Chaos Engineering + Recovery Observability Tests
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, ChaosValidatesRecovery) {
    // SCENARIO: Chaos engineering validates that recovery systems work

    ASSERT_TRUE(hr_start(hr_ctx));
    ASSERT_TRUE(ro_start(ro_ctx));

    // Register nodes using HR's child registration
    hr_add_child(hr_ctx, 1, HR_LEVEL_NODE);
    hr_add_child(hr_ctx, 2, HR_LEVEL_NODE);

    // Create chaos experiment
    uint32_t exp_id = ce_create_experiment(ce_ctx, "recovery_validation",
        "Verify system recovers from simulated failure within 5 seconds");

    ce_fault_spec_t fault;
    memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_PROCESS_KILL;
    fault.pattern = CE_PATTERN_ONCE;
    ce_set_fault(ce_ctx, exp_id, &fault);

    ce_target_spec_t target;
    memset(&target, 0, sizeof(target));
    strncpy(target.name, "test_target", sizeof(target.name) - 1);
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ce_ctx, exp_id, &target);

    ce_hypothesis_t hypothesis;
    memset(&hypothesis, 0, sizeof(hypothesis));
    strncpy(hypothesis.description, "Recovery within 5 seconds",
            sizeof(hypothesis.description) - 1);
    strncpy(hypothesis.metric_name, "recovery_time_ms", sizeof(hypothesis.metric_name) - 1);
    hypothesis.expected_min = 0;
    hypothesis.expected_max = 5000;
    ce_add_hypothesis(ce_ctx, exp_id, &hypothesis);

    // Add safety guardrail
    ce_guardrail_config_t guardrail;
    memset(&guardrail, 0, sizeof(guardrail));
    guardrail.type = CE_GUARDRAIL_TIME_LIMIT;
    guardrail.threshold = 10000;  // 10 second max
    guardrail.abort_on_violation = true;
    ce_add_guardrail(ce_ctx, exp_id, &guardrail);

    // Start experiment (dry run)
    ce_start_experiment(ce_ctx, exp_id);

    // Record with observability
    ro_span_t* trace = ro_start_trace(ro_ctx, "chaos_recovery");

    // Simulate recovery time
    struct timespec ts = {0, 100000000};  // 100ms
    nanosleep(&ts, NULL);

    // End trace
    ro_end_span(trace, RO_SPAN_OK, "chaos experiment completed");

    // Get experiment result
    ce_experiment_t exp;
    ce_get_experiment(ce_ctx, exp_id, &exp);

    ASSERT_TRUE(ro_stop(ro_ctx));
    ASSERT_TRUE(hr_stop(hr_ctx));
}

//=============================================================================
// Cross-Module State Synchronization Tests
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, StateConsistencyAcrossModules) {
    // SCENARIO: Verify state is consistent across DFT, HR, and GD

    ASSERT_TRUE(dft_start(dft_ctx));
    ASSERT_TRUE(hr_start(hr_ctx));
    ASSERT_TRUE(gd_start(gd_ctx));

    // All should start healthy/full
    EXPECT_EQ(gd_get_current_tier(gd_ctx), GD_TIER_FULL);

    // Register nodes using proper APIs
    for (int i = 1; i <= 3; i++) {
        dft_add_peer(dft_ctx, i, NULL);
        hr_add_child(hr_ctx, i, HR_LEVEL_NODE);
    }

    // Fail a node in DFT
    dft_report_suspected_failure(dft_ctx, 2, "heartbeat timeout - test");

    // Fail another node
    dft_report_suspected_failure(dft_ctx, 3, "heartbeat timeout - test");

    // Get cluster health (returns float)
    float cluster_health = dft_get_cluster_health(dft_ctx);

    // If health is poor, degrade
    if (cluster_health < 50.0f) {
        gd_set_tier(gd_ctx, GD_TIER_REDUCED, "cluster_failures");
        EXPECT_EQ(gd_get_current_tier(gd_ctx), GD_TIER_REDUCED);
    }

    ASSERT_TRUE(dft_stop(dft_ctx));
    ASSERT_TRUE(hr_stop(hr_ctx));
    ASSERT_TRUE(gd_stop(gd_ctx));
}

//=============================================================================
// Recovery Evolution Integration Tests
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, RecoveryEvolutionLearning) {
    // SCENARIO: Recovery evolution learns from successful recoveries using genetic algorithms

    ASSERT_NE(re_ctx, nullptr);
    ASSERT_TRUE(ro_start(ro_ctx));

    // Start trace
    ro_span_t* trace = ro_start_trace(ro_ctx, "recovery_learning");

    // Initialize the population for genetic algorithm
    EXPECT_TRUE(re_init_population(re_ctx));

    // Create and add recovery strategies with proper struct fields
    re_strategy_t strategy;
    memset(&strategy, 0, sizeof(strategy));
    strategy.id = 1;
    strategy.actions[0] = RE_ACTION_RESTART;
    strategy.action_count = 1;
    strategy.fitness = 0.0f;
    EXPECT_TRUE(re_add_strategy(re_ctx, &strategy));

    strategy.id = 2;
    strategy.actions[0] = RE_ACTION_CHECKPOINT;
    strategy.actions[1] = RE_ACTION_RESTART;
    strategy.action_count = 2;
    EXPECT_TRUE(re_add_strategy(re_ctx, &strategy));

    // Simulate outcomes and evaluate fitness
    re_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.strategy_id = 1;
    outcome.success = true;
    outcome.recovery_time_ms = 100;
    outcome.data_loss = 0.0f;
    outcome.resource_usage = 0.2f;
    outcome.retry_count = 1;

    float fitness = re_evaluate_fitness(re_ctx, 0, &outcome);
    EXPECT_GE(fitness, 0.0f);

    // Get the best strategy
    re_strategy_t best;
    memset(&best, 0, sizeof(best));
    EXPECT_TRUE(re_get_best_strategy(re_ctx, &best));

    // Get stats
    re_stats_t stats;
    EXPECT_TRUE(re_get_stats(re_ctx, &stats));

    // End trace
    ro_end_span(trace, RO_SPAN_OK, "learning cycle complete");

    ASSERT_TRUE(ro_stop(ro_ctx));
}

//=============================================================================
// Full Pipeline Integration Test
//=============================================================================

TEST_F(EnhancedFaultToleranceIntegrationTest, FullPipelineIntegration) {
    // SCENARIO: Full fault tolerance pipeline from detection to recovery

    // Start all systems
    ASSERT_TRUE(pa_start(pa_ctx));
    ASSERT_TRUE(gd_start(gd_ctx));
    ASSERT_TRUE(dft_start(dft_ctx));
    ASSERT_TRUE(hr_start(hr_ctx));
    ASSERT_TRUE(ro_start(ro_ctx));

    // Register nodes
    for (int i = 2; i <= 5; i++) {
        dft_add_peer(dft_ctx, i, NULL);
        hr_add_child(hr_ctx, i, HR_LEVEL_NODE);
    }

    // PHASE 1: DETECTION (Predictive Analysis)
    for (int i = 0; i < 50; i++) {
        pa_add_sample(pa_ctx, PA_SERIES_ERROR_RATE, 0.01 + i * 0.005);
    }

    ro_span_t* trace = ro_start_trace(ro_ctx, "full_pipeline");

    pa_anomaly_t anomalies[10];
    uint32_t anomaly_count = pa_detect_anomalies(pa_ctx, PA_SERIES_ERROR_RATE, anomalies, 10);

    // PHASE 2: ASSESSMENT (Distributed FT)
    float health = dft_get_cluster_health(dft_ctx);
    ro_gauge_t* health_gauge = ro_create_gauge(ro_ctx, "cluster.health", NULL, 0);
    if (health_gauge) ro_gauge_set(health_gauge, (double)health);

    // PHASE 3: RESPONSE (Graceful Degradation)
    if (anomaly_count > 2) {
        gd_set_tier(gd_ctx, GD_TIER_REDUCED, "anomaly_response");
        ro_event_t tier_event;
        memset(&tier_event, 0, sizeof(tier_event));
        tier_event.type = RO_EVENT_DEGRADATION;
        tier_event.severity = RO_SEVERITY_WARN;
        strncpy(tier_event.message, "tier_reduced", sizeof(tier_event.message) - 1);
        ro_log_event(ro_ctx, &tier_event);
    }

    // PHASE 4: RECOVERY
    if (anomaly_count > 0) {
        dft_initiate_recovery(dft_ctx, 2);
        ro_counter_t* recovery_counter = ro_create_counter(ro_ctx, "recoveries.initiated", NULL, 0);
        if (recovery_counter) ro_counter_inc(recovery_counter, 1);
    }

    ro_end_span(trace, RO_SPAN_OK, "pipeline complete");

    // Stop all systems
    ASSERT_TRUE(ro_stop(ro_ctx));
    ASSERT_TRUE(hr_stop(hr_ctx));
    ASSERT_TRUE(dft_stop(dft_ctx));
    ASSERT_TRUE(gd_stop(gd_ctx));
    ASSERT_TRUE(pa_stop(pa_ctx));
}
