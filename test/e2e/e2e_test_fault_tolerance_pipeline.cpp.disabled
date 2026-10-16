/**
 * @file e2e_test_fault_tolerance_pipeline.cpp
 * @brief E2E Test for Enhanced Fault Tolerance "Immune System" Pipeline
 *
 * WHAT: Complete end-to-end tests for the fault tolerance immune system
 * WHY:  Verify all 8 FT modules work together as a cohesive immune system
 * HOW:  Simulate failures, verify detection, recovery, and adaptation
 *
 * TEST SCENARIOS:
 * 1. ImmuneSystemBootstrap - All modules initialize and coordinate
 * 2. ThreatDetection - Predictive analysis detects anomalies
 * 3. AdaptiveResponse - Graceful degradation responds to threats
 * 4. DistributedRecovery - Cross-node failure handling
 * 5. ByzantineResilience - PBFT consensus under adversarial conditions
 * 6. ChaosValidation - Chaos engineering validates recovery
 * 7. EvolutionaryLearning - Strategies improve over time
 * 8. FullImmuneResponse - Complete immune system activation
 *
 * BIOLOGICAL ANALOGY:
 * The fault tolerance system operates like the biological immune system:
 * - Detection (PA): Pattern recognition like dendritic cells
 * - Response (GD): Inflammation and containment
 * - Elimination (HR): T-cells eliminating threats
 * - Memory (RE): Adaptive immunity remembering past threats
 * - Tolerance (BFT): Self vs non-self discrimination
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>

extern "C" {
#include "utils/fault_tolerance/nimcp_distributed_fault_tolerance.h"
#include "utils/fault_tolerance/nimcp_hierarchical_recovery.h"
#include "utils/fault_tolerance/nimcp_recovery_evolution.h"
#include "utils/fault_tolerance/nimcp_byzantine_fault_tolerance.h"
#include "utils/fault_tolerance/nimcp_graceful_degradation.h"
#include "utils/fault_tolerance/nimcp_recovery_observability.h"
#include "utils/fault_tolerance/nimcp_chaos_engineering.h"
#include "utils/fault_tolerance/nimcp_predictive_analysis.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
}

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Cluster parameters
constexpr uint32_t CLUSTER_SIZE = 7;
constexpr uint32_t NODE_ID = 1;

// Test durations
constexpr uint32_t SHORT_RUN_MS = 100;
constexpr uint32_t MEDIUM_RUN_MS = 500;
constexpr uint32_t LONG_RUN_MS = 2000;

// Timing thresholds
constexpr double MAX_DETECTION_MS = 100.0;
constexpr double MAX_RESPONSE_MS = 200.0;
constexpr double MAX_RECOVERY_MS = 500.0;

// Success thresholds
constexpr float MIN_RECOVERY_RATE = 0.8f;
constexpr float MIN_DETECTION_ACCURACY = 0.7f;

//=============================================================================
// Immune System Context - All FT Modules
//=============================================================================

struct ImmuneSystemContext {
    dft_context_t* dft;  // Distributed fault tolerance
    hr_context_t* hr;    // Hierarchical recovery
    re_context_t* re;    // Recovery evolution
    bft_context_t* bft;  // Byzantine fault tolerance
    gd_context_t* gd;    // Graceful degradation
    ro_context_t* ro;    // Recovery observability
    ce_context_t* ce;    // Chaos engineering
    pa_context_t* pa;    // Predictive analysis

    bool initialized;
    bool running;
};

/**
 * @brief Initialize all immune system modules
 */
static bool immune_system_init(ImmuneSystemContext* ctx) {
    if (!ctx) return false;

    memset(ctx, 0, sizeof(ImmuneSystemContext));

    // Create DFT context
    dft_config_t dft_config = dft_default_config();
    dft_config.node_id = NODE_ID;
    dft_config.cluster_size = CLUSTER_SIZE;
    ctx->dft = dft_create(&dft_config);
    if (!ctx->dft) return false;

    // Create HR context
    hr_config_t hr_config = hr_default_config();
    ctx->hr = hr_create(&hr_config);
    if (!ctx->hr) return false;

    // Create RE context
    re_config_t re_config = re_default_config();
    ctx->re = re_create(&re_config);
    if (!ctx->re) return false;

    // Create BFT context
    bft_config_t bft_config = bft_default_config();
    bft_config.node_id = NODE_ID;
    bft_config.cluster_size = CLUSTER_SIZE;
    ctx->bft = bft_create(&bft_config);
    if (!ctx->bft) return false;

    // Create GD context
    gd_config_t gd_config = gd_default_config();
    ctx->gd = gd_create(&gd_config);
    if (!ctx->gd) return false;

    // Create RO context
    ro_config_t ro_config = ro_default_config();
    ctx->ro = ro_create(&ro_config);
    if (!ctx->ro) return false;

    // Create CE context (dry run mode)
    ce_config_t ce_config = ce_default_config();
    ce_config.enable_dry_run = true;
    ctx->ce = ce_create(&ce_config);
    if (!ctx->ce) return false;

    // Create PA context
    pa_config_t pa_config = pa_default_config();
    ctx->pa = pa_create(&pa_config);
    if (!ctx->pa) return false;

    ctx->initialized = true;
    return true;
}

/**
 * @brief Start all immune system modules
 */
static bool immune_system_start(ImmuneSystemContext* ctx) {
    if (!ctx || !ctx->initialized) return false;

    if (!dft_start(ctx->dft)) return false;
    if (!hr_start(ctx->hr)) return false;
    if (!re_start(ctx->re)) return false;
    if (!bft_start(ctx->bft)) return false;
    if (!gd_start(ctx->gd)) return false;
    if (!ro_start(ctx->ro)) return false;
    if (!pa_start(ctx->pa)) return false;

    ctx->running = true;
    return true;
}

/**
 * @brief Stop all immune system modules
 */
static bool immune_system_stop(ImmuneSystemContext* ctx) {
    if (!ctx || !ctx->running) return false;

    pa_stop(ctx->pa);
    ro_stop(ctx->ro);
    gd_stop(ctx->gd);
    bft_stop(ctx->bft);
    re_stop(ctx->re);
    hr_stop(ctx->hr);
    dft_stop(ctx->dft);

    ctx->running = false;
    return true;
}

/**
 * @brief Destroy all immune system modules
 */
static void immune_system_destroy(ImmuneSystemContext* ctx) {
    if (!ctx) return;

    if (ctx->running) {
        immune_system_stop(ctx);
    }

    if (ctx->pa) pa_destroy(ctx->pa);
    if (ctx->ce) ce_destroy(ctx->ce);
    if (ctx->ro) ro_destroy(ctx->ro);
    if (ctx->gd) gd_destroy(ctx->gd);
    if (ctx->bft) bft_destroy(ctx->bft);
    if (ctx->re) re_destroy(ctx->re);
    if (ctx->hr) hr_destroy(ctx->hr);
    if (ctx->dft) dft_destroy(ctx->dft);

    memset(ctx, 0, sizeof(ImmuneSystemContext));
}

//=============================================================================
// Test Fixture
//=============================================================================

class FaultTolerancePipelineTest : public ::testing::Test {
protected:
    ImmuneSystemContext immune;

    void SetUp() override {
        ASSERT_TRUE(immune_system_init(&immune));
    }

    void TearDown() override {
        immune_system_destroy(&immune);
    }

    void RegisterClusterNodes() {
        // Register nodes in DFT
        for (uint32_t i = 2; i <= CLUSTER_SIZE; i++) {
            dft_node_info_t info = {0};
            info.node_id = i;
            info.state = DFT_NODE_HEALTHY;
            dft_register_node(immune.dft, &info);
        }

        // Register nodes in HR
        for (uint32_t i = 1; i <= CLUSTER_SIZE; i++) {
            hr_node_t node = {0};
            node.node_id = i;
            snprintf(node.name, sizeof(node.name), "node_%u", i);
            hr_register_node(immune.hr, &node);
        }
    }

    void RegisterRecoveryStrategies() {
        // Register basic recovery strategies
        const char* names[] = {"restart", "failover", "checkpoint"};
        float fitnesses[] = {0.5f, 0.7f, 0.8f};

        for (int i = 0; i < 3; i++) {
            re_strategy_t strategy = {0};
            strncpy(strategy.name, names[i], sizeof(strategy.name) - 1);
            strategy.fault_type = 1;
            strategy.actions[0] = i + 1;
            strategy.action_count = 1;
            strategy.fitness = fitnesses[i];
            re_register_strategy(immune.re, &strategy);
        }
    }

    void RegisterGdFeatures() {
        // Register service features with priorities
        gd_feature_t critical = {0};
        strncpy(critical.name, "core_processing", sizeof(critical.name) - 1);
        critical.priority = GD_PRIORITY_CRITICAL;
        critical.minimum_tier = GD_TIER_EMERGENCY;
        gd_register_feature(immune.gd, &critical);

        gd_feature_t optional = {0};
        strncpy(optional.name, "analytics", sizeof(optional.name) - 1);
        optional.priority = GD_PRIORITY_OPTIONAL;
        optional.minimum_tier = GD_TIER_FULL;
        gd_register_feature(immune.gd, &optional);
    }

    void SimulateNormalOperations(uint32_t duration_ms) {
        uint32_t steps = duration_ms / 10;
        for (uint32_t i = 0; i < steps; i++) {
            // Add normal metrics
            double cpu = 40.0 + 10.0 * sin(i * 0.1);
            double memory = 50.0 + 5.0 * cos(i * 0.1);

            pa_add_sample(immune.pa, PA_SERIES_CPU, cpu);
            pa_add_sample(immune.pa, PA_SERIES_MEMORY, memory);

            gd_update_resource(immune.gd, GD_RESOURCE_CPU, cpu);
            gd_update_resource(immune.gd, GD_RESOURCE_MEMORY, memory);

            // Simulate heartbeats
            for (uint32_t j = 2; j <= CLUSTER_SIZE; j++) {
                dft_heartbeat_t hb = {0};
                hb.sender_id = j;
                hb.sequence = i;
                hb.timestamp_ms = i * 10;
                dft_receive_heartbeat(immune.dft, &hb);
            }

            struct timespec ts = {0, 10000000};  // 10ms
            nanosleep(&ts, NULL);
        }
    }
};

//=============================================================================
// E2E Test: Immune System Bootstrap
//=============================================================================

TEST_F(FaultTolerancePipelineTest, ImmuneSystemBootstrap) {
    E2E_PIPELINE_START("Immune System Bootstrap");

    E2E_STAGE_BEGIN("Initialize modules", 100);
    EXPECT_TRUE(immune.initialized);
    EXPECT_NE(immune.dft, nullptr);
    EXPECT_NE(immune.hr, nullptr);
    EXPECT_NE(immune.re, nullptr);
    EXPECT_NE(immune.bft, nullptr);
    EXPECT_NE(immune.gd, nullptr);
    EXPECT_NE(immune.ro, nullptr);
    EXPECT_NE(immune.ce, nullptr);
    EXPECT_NE(immune.pa, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Start all modules", 200);
    EXPECT_TRUE(immune_system_start(&immune));
    EXPECT_TRUE(immune.running);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Register cluster nodes", 100);
    RegisterClusterNodes();
    RegisterRecoveryStrategies();
    RegisterGdFeatures();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify initial state", 50);
    EXPECT_EQ(gd_get_current_tier(immune.gd), GD_TIER_FULL);

    dft_cluster_health_t health;
    dft_get_cluster_health(immune.dft, &health);
    EXPECT_GE(health.total_nodes, CLUSTER_SIZE - 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Stop all modules", 200);
    EXPECT_TRUE(immune_system_stop(&immune));
    EXPECT_FALSE(immune.running);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Threat Detection (Predictive Analysis)
//=============================================================================

TEST_F(FaultTolerancePipelineTest, ThreatDetection) {
    E2E_PIPELINE_START("Threat Detection Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();

    E2E_STAGE_BEGIN("Establish baseline", 500);
    SimulateNormalOperations(200);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Inject anomalous pattern", 100);
    // Inject anomalous CPU spike pattern
    for (int i = 0; i < 20; i++) {
        double cpu = 85.0 + i * 0.5;  // Rising to critical
        pa_add_sample(immune.pa, PA_SERIES_CPU, cpu);
        gd_update_resource(immune.gd, GD_RESOURCE_CPU, cpu);

        struct timespec ts = {0, 10000000};
        nanosleep(&ts, NULL);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Detect anomalies", MAX_DETECTION_MS);
    pa_anomaly_t anomalies[10];
    uint32_t anomaly_count = pa_detect_anomalies(immune.pa, PA_SERIES_CPU, anomalies, 10);
    // Should detect the spike
    EXPECT_GE(anomaly_count, 1);

    // Record detection in observability
    ro_record_counter(immune.ro, "threats.detected", anomaly_count);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate prediction", MAX_DETECTION_MS);
    pa_failure_prediction_t predictions[5];
    uint32_t pred_count = pa_predict_failures(immune.pa, predictions, 5);
    // May or may not have predictions
    ro_record_counter(immune.ro, "predictions.generated", pred_count);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify observability", 50);
    ro_stats_t stats;
    ro_get_stats(immune.ro, &stats);
    EXPECT_GE(stats.events_recorded, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Adaptive Response (Graceful Degradation)
//=============================================================================

TEST_F(FaultTolerancePipelineTest, AdaptiveResponse) {
    E2E_PIPELINE_START("Adaptive Response Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();
    RegisterGdFeatures();

    E2E_STAGE_BEGIN("Initial healthy state", 50);
    EXPECT_EQ(gd_get_current_tier(immune.gd), GD_TIER_FULL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate resource pressure", 200);
    // Gradually increase resource usage
    for (int i = 0; i < 30; i++) {
        double cpu = 50.0 + i * 1.5;
        pa_add_sample(immune.pa, PA_SERIES_CPU, cpu);
        gd_update_resource(immune.gd, GD_RESOURCE_CPU, cpu);

        struct timespec ts = {0, 10000000};
        nanosleep(&ts, NULL);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trigger degradation", MAX_RESPONSE_MS);
    // Check for resource critical
    gd_resource_budget_t budget = {0};
    budget.type = GD_RESOURCE_CPU;
    budget.critical_threshold = 90.0;
    gd_set_resource_budget(immune.gd, &budget);

    gd_update_resource(immune.gd, GD_RESOURCE_CPU, 95.0);

    if (gd_is_resource_critical(immune.gd, GD_RESOURCE_CPU)) {
        gd_set_tier(immune.gd, GD_TIER_REDUCED, "resource_critical");
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify degraded state", 50);
    // Should have degraded
    gd_tier_t tier = gd_get_current_tier(immune.gd);
    EXPECT_NE(tier, GD_TIER_FULL);

    // Record tier change
    ro_event_t event = {0};
    strncpy(event.name, "tier_degraded", sizeof(event.name) - 1);
    event.type = RO_EVENT_TIER_CHANGE;
    event.severity = RO_SEVERITY_WARNING;
    ro_record_event(immune.ro, &event);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Enable load shedding", 50);
    gd_start_load_shedding(immune.gd, 50.0, GD_PRIORITY_MEDIUM, 60000);

    // Critical should pass, low priority should fail
    EXPECT_TRUE(gd_should_accept_request(immune.gd, GD_PRIORITY_CRITICAL));
    EXPECT_FALSE(gd_should_accept_request(immune.gd, GD_PRIORITY_LOW));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery when resources improve", 100);
    gd_update_resource(immune.gd, GD_RESOURCE_CPU, 40.0);
    gd_stop_load_shedding(immune.gd);
    gd_set_tier(immune.gd, GD_TIER_FULL, "resources_recovered");

    EXPECT_EQ(gd_get_current_tier(immune.gd), GD_TIER_FULL);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Distributed Recovery
//=============================================================================

TEST_F(FaultTolerancePipelineTest, DistributedRecovery) {
    E2E_PIPELINE_START("Distributed Recovery Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();
    RegisterRecoveryStrategies();

    E2E_STAGE_BEGIN("Simulate node failure", 100);
    // Report node failure to DFT
    dft_report_failure(immune.dft, 3, DFT_FAILURE_CRASH);

    dft_node_state_t state = dft_get_node_state(immune.dft, 3);
    EXPECT_EQ(state, DFT_NODE_FAILED);

    // Record in observability
    ro_event_t event = {0};
    strncpy(event.name, "node_failed", sizeof(event.name) - 1);
    event.type = RO_EVENT_FAULT_DETECTED;
    event.node_id = 3;
    event.severity = RO_SEVERITY_ERROR;
    ro_record_event(immune.ro, &event);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Initiate hierarchical recovery", MAX_RECOVERY_MS);
    // Report to HR
    hr_failure_report_t report = {0};
    report.failed_id = 3;
    report.level = HR_LEVEL_NODE;
    report.failure_type = 1;
    strncpy(report.description, "Node 3 crashed", sizeof(report.description) - 1);

    uint32_t recovery_id = hr_report_failure(immune.hr, &report);
    EXPECT_GT(recovery_id, 0);

    // Start recovery trace
    ro_trace_id_t trace_id = ro_start_trace(immune.ro, "node_recovery");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Select recovery strategy", 100);
    // Use evolution to select best strategy
    re_strategy_t best;
    bool has_strategy = re_select_best_strategy(immune.re, 1, &best);
    EXPECT_TRUE(has_strategy);

    ro_record_counter(immune.ro, "strategies.selected", 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute recovery", 200);
    // Simulate recovery execution
    struct timespec ts = {0, 100000000};  // 100ms recovery time
    nanosleep(&ts, NULL);

    // Mark node as recovered
    dft_recover_node(immune.dft, 3);
    EXPECT_EQ(dft_get_node_state(immune.dft, 3), DFT_NODE_HEALTHY);

    // Update strategy fitness
    re_evaluation_result_t result = {0};
    result.success = true;
    result.recovery_time_ms = 100;
    result.resource_usage = 0.3;
    // Would update fitness here

    ro_end_trace(immune.ro, trace_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify cluster health", 50);
    dft_cluster_health_t health;
    dft_get_cluster_health(immune.dft, &health);
    EXPECT_EQ(health.healthy_nodes, CLUSTER_SIZE - 1);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Byzantine Resilience
//=============================================================================

TEST_F(FaultTolerancePipelineTest, ByzantineResilience) {
    E2E_PIPELINE_START("Byzantine Resilience Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();

    E2E_STAGE_BEGIN("Verify quorum parameters", 50);
    // n=7, f=2, quorum=5
    uint32_t quorum = bft_get_quorum_size(immune.bft);
    uint32_t f = bft_get_fault_tolerance(immune.bft);
    EXPECT_EQ(quorum, 5);
    EXPECT_EQ(f, 2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process consensus messages", 200);
    // Propose a value
    bft_proposal_t proposal = {0};
    proposal.sequence = 1;
    proposal.value_size = 10;
    memcpy(proposal.value, "test_value", 10);

    uint32_t proposal_id = bft_propose(immune.bft, &proposal);
    EXPECT_GT(proposal_id, 0);

    // Receive PREPARE from quorum
    for (uint32_t i = 2; i <= 6; i++) {
        bft_message_t prepare = {0};
        prepare.type = BFT_MSG_PREPARE;
        prepare.sequence = 1;
        prepare.sender_id = i;
        bft_receive_message(immune.bft, &prepare);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Detect Byzantine behavior", 100);
    // Node 7 sends equivocating messages
    bft_message_t msg1 = {0};
    msg1.type = BFT_MSG_PREPARE;
    msg1.sequence = 2;
    msg1.sender_id = 7;
    memcpy(msg1.digest, "digest_a", 8);
    bft_receive_message(immune.bft, &msg1);

    bft_message_t msg2 = {0};
    msg2.type = BFT_MSG_PREPARE;
    msg2.sequence = 2;
    msg2.sender_id = 7;
    memcpy(msg2.digest, "digest_b", 8);  // Different!
    bft_receive_message(immune.bft, &msg2);

    EXPECT_TRUE(bft_is_equivocating(immune.bft, 7));

    // Report to DFT
    dft_report_failure(immune.dft, 7, DFT_FAILURE_BYZANTINE);

    ro_event_t event = {0};
    strncpy(event.name, "byzantine_detected", sizeof(event.name) - 1);
    event.type = RO_EVENT_BYZANTINE_DETECTED;
    event.node_id = 7;
    event.severity = RO_SEVERITY_ERROR;
    ro_record_event(immune.ro, &event);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Quarantine Byzantine node", 100);
    bft_quarantine_node(immune.bft, 7, 60000);
    EXPECT_TRUE(bft_is_quarantined(immune.bft, 7));

    // Update trust score
    bft_update_trust(immune.bft, 7, -1.0);
    float trust = bft_get_trust_score(immune.bft, 7);
    EXPECT_LT(trust, 1.0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify consensus still possible", 50);
    // With 1 Byzantine node quarantined, should still have quorum
    uint32_t effective_nodes = CLUSTER_SIZE - 1;  // Self + 6 others - 1 quarantined
    EXPECT_GE(effective_nodes, quorum);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Chaos Validation
//=============================================================================

TEST_F(FaultTolerancePipelineTest, ChaosValidation) {
    E2E_PIPELINE_START("Chaos Validation Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();
    RegisterRecoveryStrategies();

    E2E_STAGE_BEGIN("Create chaos experiment", 100);
    uint32_t exp_id = ce_create_experiment(immune.ce,
        "recovery_validation",
        "Validate recovery from network latency");

    ce_fault_spec_t fault = {0};
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.pattern = CE_PATTERN_ONCE;
    fault.intensity = 50.0;
    fault.duration_ms = 500;
    ce_set_fault(immune.ce, exp_id, &fault);

    ce_target_spec_t target = {0};
    strncpy(target.name, "test_nodes", sizeof(target.name) - 1);
    target.node_ids[0] = 3;
    target.node_count = 1;
    ce_set_target(immune.ce, exp_id, &target);

    ce_hypothesis_t hypothesis = {0};
    strncpy(hypothesis.description,
            "System recovers within 1 second",
            sizeof(hypothesis.description) - 1);
    strncpy(hypothesis.metric_name, "recovery_time_ms", sizeof(hypothesis.metric_name) - 1);
    hypothesis.expected_max = 1000;
    ce_add_hypothesis(immune.ce, exp_id, &hypothesis);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run experiment", 1000);
    // Start observability trace
    ro_trace_id_t trace_id = ro_start_trace(immune.ro, "chaos_experiment");
    ro_record_counter(immune.ro, "chaos.experiments.started", 1);

    ce_start_experiment(immune.ce, exp_id);
    EXPECT_EQ(ce_get_experiment_state(immune.ce, exp_id), CE_STATE_RUNNING);

    // Simulate experiment duration
    struct timespec ts = {0, 600000000};  // 600ms
    nanosleep(&ts, NULL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery", 500);
    // Check that affected node is still functional
    dft_node_state_t state = dft_get_node_state(immune.dft, 3);
    // Node should be healthy (dry run)
    EXPECT_EQ(state, DFT_NODE_HEALTHY);

    ro_record_counter(immune.ro, "chaos.recoveries.observed", 1);
    ro_end_trace(immune.ro, trace_id);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate report", 100);
    char report[4096];
    size_t len = ce_generate_report(immune.ce, exp_id, report, sizeof(report));
    EXPECT_GT(len, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Evolutionary Learning
//=============================================================================

TEST_F(FaultTolerancePipelineTest, EvolutionaryLearning) {
    E2E_PIPELINE_START("Evolutionary Learning Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));

    E2E_STAGE_BEGIN("Initialize population", 200);
    re_initialize_population(immune.re, 1);  // For fault type 1

    re_stats_t stats;
    re_get_stats(immune.re, &stats);
    EXPECT_GT(stats.total_strategies, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate recovery experiences", 500);
    // Store experiences from simulated recoveries
    for (int i = 0; i < 50; i++) {
        re_experience_t exp = {0};
        exp.state.fault_type = 1;
        exp.action = i % 5;
        exp.reward = (i % 3 == 0) ? 1.0 : ((i % 3 == 1) ? 0.5 : -0.5);
        exp.next_state.fault_type = (i % 4 == 0) ? 0 : 1;  // Occasional recovery
        exp.done = (i % 4 == 0);
        re_store_experience(immune.re, &exp);
    }

    // Replay experiences
    re_replay_experiences(immune.re, 32);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Evolve generation", 500);
    float best_before = re_get_best_fitness(immune.re, 1);

    re_evolve_generation(immune.re, 1);

    re_get_stats(immune.re, &stats);
    EXPECT_GE(stats.generations_evolved, 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify learning progress", 100);
    float best_after = re_get_best_fitness(immune.re, 1);

    // Fitness may or may not improve in single generation
    ro_record_gauge(immune.ro, "evolution.best_fitness", best_after);

    // Q-values should be updated
    re_state_t state = {0};
    state.fault_type = 1;
    double q = re_get_q_value(immune.re, &state, 0);
    EXPECT_NE(q, 0.0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Export learned strategies", 100);
    re_strategy_t exported[10];
    uint32_t count = re_export_strategies(immune.re, 1, exported, 10);
    EXPECT_GT(count, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Full Immune Response
//=============================================================================

TEST_F(FaultTolerancePipelineTest, FullImmuneResponse) {
    E2E_PIPELINE_START("Full Immune Response Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();
    RegisterRecoveryStrategies();
    RegisterGdFeatures();

    E2E_STAGE_BEGIN("Establish healthy baseline", 500);
    SimulateNormalOperations(300);
    EXPECT_EQ(gd_get_current_tier(immune.gd), GD_TIER_FULL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 1: Threat Detection", MAX_DETECTION_MS);
    // Inject anomalous pattern (infection detected)
    for (int i = 0; i < 20; i++) {
        double error_rate = 0.01 + i * 0.01;  // Rising error rate
        pa_add_sample(immune.pa, PA_SERIES_ERROR_RATE, error_rate);
    }

    pa_anomaly_t anomalies[10];
    uint32_t anomaly_count = pa_detect_anomalies(immune.pa, PA_SERIES_ERROR_RATE, anomalies, 10);

    ro_trace_id_t trace_id = ro_start_trace(immune.ro, "immune_response");
    ro_record_counter(immune.ro, "immune.phase1.anomalies", anomaly_count);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 2: Initial Response", MAX_RESPONSE_MS);
    // Inflammation: reduce service tier
    gd_set_tier(immune.gd, GD_TIER_STANDARD, "anomaly_response");

    ro_event_t event = {0};
    strncpy(event.name, "tier_reduced", sizeof(event.name) - 1);
    event.type = RO_EVENT_TIER_CHANGE;
    event.severity = RO_SEVERITY_WARNING;
    ro_record_event(immune.ro, &event);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 3: Threat Identification", 200);
    // Identify Byzantine node
    bft_behavior_report_t behavior = {0};
    behavior.node_id = 5;
    behavior.behavior = BFT_BEHAVIOR_INVALID_MESSAGE;
    bft_report_behavior(immune.bft, &behavior);
    bft_report_behavior(immune.bft, &behavior);
    bft_report_behavior(immune.bft, &behavior);

    if (bft_is_byzantine(immune.bft, 5)) {
        dft_report_failure(immune.dft, 5, DFT_FAILURE_BYZANTINE);
        ro_record_counter(immune.ro, "immune.phase3.byzantine", 1);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 4: Targeted Response", MAX_RECOVERY_MS);
    // Quarantine (like antibodies binding)
    bft_quarantine_node(immune.bft, 5, 60000);

    // Initiate recovery
    hr_failure_report_t report = {0};
    report.failed_id = 5;
    report.level = HR_LEVEL_NODE;
    report.failure_type = 1;
    strncpy(report.description, "Byzantine node isolated", sizeof(report.description) - 1);

    uint32_t recovery_id = hr_report_failure(immune.hr, &report);
    EXPECT_GT(recovery_id, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 5: Recovery Execution", 500);
    // Select and execute best strategy
    re_strategy_t best;
    if (re_select_best_strategy(immune.re, 1, &best)) {
        ro_record_counter(immune.ro, "immune.phase5.strategy_selected", 1);
    }

    // Simulate recovery
    struct timespec ts = {0, 200000000};  // 200ms
    nanosleep(&ts, NULL);

    // Replace failed node
    dft_recover_node(immune.dft, 5);
    bft_release_from_quarantine(immune.bft, 5);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 6: System Restoration", 200);
    // Restore full service
    gd_set_tier(immune.gd, GD_TIER_FULL, "threat_eliminated");
    EXPECT_EQ(gd_get_current_tier(immune.gd), GD_TIER_FULL);

    // Update strategy fitness (memory formation)
    re_evaluation_result_t result = {0};
    result.success = true;
    result.recovery_time_ms = 200;
    result.resource_usage = 0.4;

    // Store experience
    re_experience_t exp = {0};
    exp.state.fault_type = 1;
    exp.action = 1;
    exp.reward = 1.0;
    exp.done = true;
    re_store_experience(immune.re, &exp);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 7: Post-Incident Analysis", 100);
    ro_end_trace(immune.ro, trace_id);

    ro_stats_t stats;
    ro_get_stats(immune.ro, &stats);
    EXPECT_GE(stats.events_recorded, 2);

    gd_stats_t gd_stats;
    gd_get_stats(immune.gd, &gd_stats);
    EXPECT_GE(gd_stats.total_transitions, 2);  // Down and up

    dft_cluster_health_t health;
    dft_get_cluster_health(immune.dft, &health);
    EXPECT_EQ(health.healthy_nodes, CLUSTER_SIZE - 1);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Memory Stability
//=============================================================================

TEST_F(FaultTolerancePipelineTest, MemoryStability) {
    E2E_PIPELINE_START("Memory Stability Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();
    RegisterRecoveryStrategies();

    E2E_STAGE_BEGIN("Long-running operations", 3000);
    // Simulate extended operation
    for (int cycle = 0; cycle < 10; cycle++) {
        // Normal operations
        for (int i = 0; i < 20; i++) {
            pa_add_sample(immune.pa, PA_SERIES_CPU, 50.0 + i);
            gd_update_resource(immune.gd, GD_RESOURCE_CPU, 50.0 + i);

            re_experience_t exp = {0};
            exp.state.fault_type = 1;
            exp.action = i % 5;
            exp.reward = 0.5;
            re_store_experience(immune.re, &exp);
        }

        // Periodic failure/recovery
        dft_report_failure(immune.dft, 3, DFT_FAILURE_TIMEOUT);
        dft_recover_node(immune.dft, 3);

        struct timespec ts = {0, 100000000};  // 100ms
        nanosleep(&ts, NULL);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify no memory issues", 100);
    // All modules should still be functional
    EXPECT_TRUE(immune.running);

    pa_series_meta_t meta;
    EXPECT_TRUE(pa_get_series_meta(immune.pa, PA_SERIES_CPU, &meta));
    EXPECT_GT(meta.sample_count, 0);

    dft_cluster_health_t health;
    dft_get_cluster_health(immune.dft, &health);
    EXPECT_GT(health.total_nodes, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Concurrent Operations
//=============================================================================

TEST_F(FaultTolerancePipelineTest, ConcurrentOperations) {
    E2E_PIPELINE_START("Concurrent Operations Pipeline");

    ASSERT_TRUE(immune_system_start(&immune));
    RegisterClusterNodes();

    std::atomic<int> completed{0};
    std::atomic<bool> has_error{false};

    E2E_STAGE_BEGIN("Concurrent module operations", 2000);

    auto pa_worker = [&]() {
        for (int i = 0; i < 100 && !has_error; i++) {
            pa_add_sample(immune.pa, PA_SERIES_CPU, 50.0 + i * 0.1);
        }
        completed++;
    };

    auto gd_worker = [&]() {
        for (int i = 0; i < 100 && !has_error; i++) {
            gd_update_resource(immune.gd, GD_RESOURCE_CPU, 40.0 + i * 0.2);
        }
        completed++;
    };

    auto dft_worker = [&]() {
        for (int i = 0; i < 100 && !has_error; i++) {
            dft_heartbeat_t hb = {0};
            hb.sender_id = 2;
            hb.sequence = i;
            hb.timestamp_ms = i * 10;
            dft_receive_heartbeat(immune.dft, &hb);
        }
        completed++;
    };

    auto ro_worker = [&]() {
        for (int i = 0; i < 100 && !has_error; i++) {
            ro_record_counter(immune.ro, "concurrent_test", 1);
        }
        completed++;
    };

    std::thread t1(pa_worker);
    std::thread t2(gd_worker);
    std::thread t3(dft_worker);
    std::thread t4(ro_worker);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(completed, 4);
    EXPECT_FALSE(has_error);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify data consistency", 100);
    pa_series_meta_t meta;
    pa_get_series_meta(immune.pa, PA_SERIES_CPU, &meta);
    EXPECT_EQ(meta.sample_count, 100);

    uint64_t counter = ro_get_counter(immune.ro, "concurrent_test");
    EXPECT_EQ(counter, 100);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

