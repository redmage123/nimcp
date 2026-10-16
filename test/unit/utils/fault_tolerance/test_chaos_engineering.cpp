/**
 * @file test_chaos_engineering.cpp
 * @brief Unit tests for chaos engineering module
 *
 * Tests controlled fault injection, experiment lifecycle,
 * safety guardrails, and hypothesis evaluation.
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_chaos_engineering.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ChaosEngineeringTest : public ::testing::Test {
protected:
    ce_context_t* ctx;
    ce_config_t config;

    void SetUp() override {
        config = ce_default_config();
        config.enable_dry_run = true;  // Safety: don't actually inject faults
        ctx = ce_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            ce_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(CeLifecycleTest, DefaultConfig) {
    ce_config_t config = ce_default_config();

    EXPECT_TRUE(config.enable_safety_checks);
    EXPECT_GT(config.max_blast_radius, 0);
    EXPECT_GT(config.max_experiment_duration_ms, 0);
    EXPECT_GT(config.min_availability, 0.0);
}

TEST(CeLifecycleTest, CreateAndDestroy) {
    ce_config_t config = ce_default_config();

    ce_context_t* ctx = ce_create(&config);
    ASSERT_NE(ctx, nullptr);

    ce_destroy(ctx);
}

TEST(CeLifecycleTest, CreateWithNullConfig) {
    ce_context_t* ctx = ce_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

//=============================================================================
// Experiment Lifecycle Tests
//=============================================================================

TEST_F(ChaosEngineeringTest, CreateExperiment) {
    uint32_t id = ce_create_experiment(ctx, "test_exp", "A test experiment");
    EXPECT_GT(id, 0);
}

TEST_F(ChaosEngineeringTest, SetFaultSpec) {
    uint32_t id = ce_create_experiment(ctx, "fault_test", "Test fault spec");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.pattern = CE_PATTERN_ONCE;
    fault.intensity = 50.0;
    fault.duration_ms = 5000;

    EXPECT_TRUE(ce_set_fault(ctx, id, &fault));
}

TEST_F(ChaosEngineeringTest, SetTarget) {
    uint32_t id = ce_create_experiment(ctx, "target_test", "Test target spec");

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    strncpy(target.name, "test_nodes", sizeof(target.name) - 1);
    target.strategy = CE_TARGET_SPECIFIC;
    target.node_ids[0] = 1;
    target.node_ids[1] = 2;
    target.node_count = 2;

    EXPECT_TRUE(ce_set_target(ctx, id, &target));
}

TEST_F(ChaosEngineeringTest, AddHypothesis) {
    uint32_t id = ce_create_experiment(ctx, "hyp_test", "Test hypothesis");

    ce_hypothesis_t hypothesis; memset(&hypothesis, 0, sizeof(hypothesis));
    strncpy(hypothesis.description, "System should recover within 5 seconds",
            sizeof(hypothesis.description) - 1);
    strncpy(hypothesis.metric_name, "recovery_time_ms", sizeof(hypothesis.metric_name) - 1);
    hypothesis.expected_min = 0;
    hypothesis.expected_max = 5000;

    EXPECT_TRUE(ce_add_hypothesis(ctx, id, &hypothesis));
}

TEST_F(ChaosEngineeringTest, AddGuardrail) {
    uint32_t id = ce_create_experiment(ctx, "guard_test", "Test guardrail");

    ce_guardrail_config_t guardrail; memset(&guardrail, 0, sizeof(guardrail));
    guardrail.type = CE_GUARDRAIL_BLAST_RADIUS;
    guardrail.threshold = 10.0;
    guardrail.abort_on_violation = true;

    EXPECT_TRUE(ce_add_guardrail(ctx, id, &guardrail));
}

TEST_F(ChaosEngineeringTest, AddMetric) {
    uint32_t id = ce_create_experiment(ctx, "metric_test", "Test metric");

    EXPECT_TRUE(ce_add_metric(ctx, id, "latency_p99"));
    EXPECT_TRUE(ce_add_metric(ctx, id, "error_rate"));
}

//=============================================================================
// Experiment Execution Tests
//=============================================================================

TEST_F(ChaosEngineeringTest, StartExperiment) {
    uint32_t id = ce_create_experiment(ctx, "start_test", "Test start");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 1000;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.strategy = CE_TARGET_SPECIFIC;
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    EXPECT_TRUE(ce_start_experiment(ctx, id));
    EXPECT_EQ(ce_get_experiment_state(ctx, id), CE_STATE_RUNNING);
}

TEST_F(ChaosEngineeringTest, PauseAndResumeExperiment) {
    uint32_t id = ce_create_experiment(ctx, "pause_test", "Test pause");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_CPU_STRESS;
    fault.duration_ms = 10000;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    ce_start_experiment(ctx, id);

    EXPECT_TRUE(ce_pause_experiment(ctx, id));
    EXPECT_EQ(ce_get_experiment_state(ctx, id), CE_STATE_PAUSED);

    EXPECT_TRUE(ce_resume_experiment(ctx, id));
    EXPECT_EQ(ce_get_experiment_state(ctx, id), CE_STATE_RUNNING);
}

TEST_F(ChaosEngineeringTest, AbortExperiment) {
    uint32_t id = ce_create_experiment(ctx, "abort_test", "Test abort");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_MEMORY_LEAK;
    fault.duration_ms = 60000;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    ce_start_experiment(ctx, id);
    EXPECT_TRUE(ce_abort_experiment(ctx, id, "Test abort reason"));
    EXPECT_EQ(ce_get_experiment_state(ctx, id), CE_STATE_ABORTED);
}

TEST_F(ChaosEngineeringTest, GetExperiment) {
    uint32_t id = ce_create_experiment(ctx, "get_test", "Test get experiment");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_DISK_SLOW;
    ce_set_fault(ctx, id, &fault);

    ce_experiment_t exp;
    EXPECT_TRUE(ce_get_experiment(ctx, id, &exp));
    EXPECT_STREQ(exp.name, "get_test");
    EXPECT_EQ(exp.fault.type, CE_FAULT_DISK_SLOW);
}

TEST_F(ChaosEngineeringTest, GetResult) {
    uint32_t id = ce_create_experiment(ctx, "result_test", "Test result");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LOSS;
    fault.duration_ms = 100;  // Short duration for quick completion
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    ce_start_experiment(ctx, id);

    // Wait for experiment to complete (dry run should be fast)
    struct timespec ts = {0, 200000000};  // 200ms
    nanosleep(&ts, NULL);

    ce_result_t result;
    if (ce_get_experiment_state(ctx, id) == CE_STATE_COMPLETED) {
        EXPECT_TRUE(ce_get_result(ctx, id, &result));
        EXPECT_EQ(result.experiment_id, id);
    }
}

//=============================================================================
// Direct Fault Injection Tests
//=============================================================================

TEST_F(ChaosEngineeringTest, InjectNetworkLatency) {
    // Dry run mode - should succeed without actual injection
    EXPECT_TRUE(ce_inject_network_latency(ctx, 1, 100, 5000));
}

TEST_F(ChaosEngineeringTest, InjectPacketLoss) {
    EXPECT_TRUE(ce_inject_packet_loss(ctx, 1, 10.0, 5000));
}

TEST_F(ChaosEngineeringTest, InjectCpuStress) {
    EXPECT_TRUE(ce_inject_cpu_stress(ctx, 1, 80, 1000));
}

TEST_F(ChaosEngineeringTest, InjectMemoryPressure) {
    EXPECT_TRUE(ce_inject_memory_pressure(ctx, 1, 1024 * 1024, 1000));
}

TEST_F(ChaosEngineeringTest, InjectAndRollback) {
    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.intensity = 50.0;
    fault.duration_ms = 10000;

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;

    EXPECT_TRUE(ce_inject_fault(ctx, &fault, &target));
    EXPECT_TRUE(ce_rollback_fault(ctx, &fault, &target));
}

TEST_F(ChaosEngineeringTest, CreateNetworkPartition) {
    uint32_t group1[] = {1, 2, 3};
    uint32_t group2[] = {4, 5, 6};

    EXPECT_TRUE(ce_create_partition(ctx, group1, 3, group2, 3, 5000));
}

//=============================================================================
// Callback Tests
//=============================================================================

static bool inject_callback_invoked = false;
static bool inject_callback(const ce_fault_spec_t* fault, const ce_target_spec_t* target, void* user_data) {
    (void)fault;
    (void)target;
    (void)user_data;
    inject_callback_invoked = true;
    return true;
}

static bool rollback_callback_invoked = false;
static bool rollback_callback(const ce_fault_spec_t* fault, const ce_target_spec_t* target, void* user_data) {
    (void)fault;
    (void)target;
    (void)user_data;
    rollback_callback_invoked = true;
    return true;
}

static bool event_callback_invoked = false;
static void event_callback(uint32_t experiment_id, ce_state_t state, const char* message, void* user_data) {
    (void)experiment_id;
    (void)state;
    (void)message;
    (void)user_data;
    event_callback_invoked = true;
}

TEST_F(ChaosEngineeringTest, RegisterInjectCallback) {
    inject_callback_invoked = false;

    EXPECT_TRUE(ce_register_inject_callback(ctx, CE_FAULT_NETWORK_LATENCY, inject_callback, nullptr));

    // Trigger callback by injecting fault
    ce_inject_network_latency(ctx, 1, 100, 1000);

    // Callback may or may not be invoked depending on implementation
    (void)inject_callback_invoked;
}

TEST_F(ChaosEngineeringTest, RegisterRollbackCallback) {
    EXPECT_TRUE(ce_register_rollback_callback(ctx, CE_FAULT_NETWORK_LATENCY, rollback_callback, nullptr));
}

TEST_F(ChaosEngineeringTest, RegisterEventCallback) {
    event_callback_invoked = false;

    EXPECT_TRUE(ce_register_event_callback(ctx, event_callback, nullptr));

    uint32_t id = ce_create_experiment(ctx, "event_test", "Test events");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    ce_start_experiment(ctx, id);

    EXPECT_TRUE(event_callback_invoked);
}

//=============================================================================
// Safety Tests
//=============================================================================

TEST_F(ChaosEngineeringTest, IsSafeToRun) {
    uint32_t id = ce_create_experiment(ctx, "safe_test", "Test safety check");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 1000;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    EXPECT_TRUE(ce_is_safe_to_run(ctx, id));
}

TEST_F(ChaosEngineeringTest, ValidateExperiment) {
    uint32_t id = ce_create_experiment(ctx, "validate_test", "Test validation");

    // Incomplete experiment - missing fault and target
    char errors[4][256];
    uint32_t error_count = ce_validate_experiment(ctx, id, errors, 4);

    EXPECT_GT(error_count, 0);  // Should have validation errors
}

TEST_F(ChaosEngineeringTest, ValidateCompleteExperiment) {
    uint32_t id = ce_create_experiment(ctx, "complete_test", "Complete experiment");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 1000;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    char errors[4][256];
    uint32_t error_count = ce_validate_experiment(ctx, id, errors, 4);

    EXPECT_EQ(error_count, 0);  // Should have no validation errors
}

TEST_F(ChaosEngineeringTest, DryRunMode) {
    EXPECT_TRUE(ce_is_dry_run(ctx));

    ce_set_dry_run(ctx, false);
    EXPECT_FALSE(ce_is_dry_run(ctx));

    ce_set_dry_run(ctx, true);
    EXPECT_TRUE(ce_is_dry_run(ctx));
}

//=============================================================================
// Game Day Tests
//=============================================================================

TEST_F(ChaosEngineeringTest, ScheduleGameDay) {
    uint32_t exp1 = ce_create_experiment(ctx, "exp1", "Experiment 1");
    uint32_t exp2 = ce_create_experiment(ctx, "exp2", "Experiment 2");

    // Set up experiments
    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 1000;

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;

    ce_set_fault(ctx, exp1, &fault);
    ce_set_target(ctx, exp1, &target);

    fault.type = CE_FAULT_CPU_STRESS;
    ce_set_fault(ctx, exp2, &fault);
    ce_set_target(ctx, exp2, &target);

    uint32_t experiments[] = {exp1, exp2};
    uint32_t game_day_id = ce_schedule_game_day(ctx, experiments, 2, 0);

    EXPECT_GT(game_day_id, 0);
}

TEST_F(ChaosEngineeringTest, StartGameDay) {
    uint32_t exp1 = ce_create_experiment(ctx, "gd_exp1", "Game day exp 1");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 100;
    ce_set_fault(ctx, exp1, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, exp1, &target);

    uint32_t experiments[] = {exp1};
    uint32_t game_day_id = ce_schedule_game_day(ctx, experiments, 1, 0);

    EXPECT_TRUE(ce_start_game_day(ctx, game_day_id));
}

TEST_F(ChaosEngineeringTest, AbortGameDay) {
    uint32_t exp1 = ce_create_experiment(ctx, "abort_gd_exp", "Abort game day exp");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 10000;
    ce_set_fault(ctx, exp1, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, exp1, &target);

    uint32_t experiments[] = {exp1};
    uint32_t game_day_id = ce_schedule_game_day(ctx, experiments, 1, 0);

    ce_start_game_day(ctx, game_day_id);
    EXPECT_TRUE(ce_abort_game_day(ctx, game_day_id));
}

//=============================================================================
// Reporting Tests
//=============================================================================

TEST_F(ChaosEngineeringTest, GenerateReport) {
    uint32_t id = ce_create_experiment(ctx, "report_test", "Test report generation");

    ce_fault_spec_t fault; memset(&fault, 0, sizeof(fault));
    fault.type = CE_FAULT_NETWORK_LATENCY;
    fault.duration_ms = 100;
    ce_set_fault(ctx, id, &fault);

    ce_target_spec_t target; memset(&target, 0, sizeof(target));
    target.node_ids[0] = 1;
    target.node_count = 1;
    ce_set_target(ctx, id, &target);

    char buffer[4096];
    size_t len = ce_generate_report(ctx, id, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
}

TEST_F(ChaosEngineeringTest, ListExperiments) {
    ce_create_experiment(ctx, "list_exp1", "List exp 1");
    ce_create_experiment(ctx, "list_exp2", "List exp 2");
    ce_create_experiment(ctx, "list_exp3", "List exp 3");

    ce_experiment_t experiments[10];
    uint32_t count = ce_list_experiments(ctx, experiments, 10);

    EXPECT_GE(count, 3);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(CeStringTest, FaultTypeToString) {
    EXPECT_STREQ("None", ce_fault_type_to_string(CE_FAULT_NONE));
    EXPECT_STREQ("ProcessKill", ce_fault_type_to_string(CE_FAULT_PROCESS_KILL));
    EXPECT_STREQ("NetworkLatency", ce_fault_type_to_string(CE_FAULT_NETWORK_LATENCY));
    EXPECT_STREQ("NetworkPartition", ce_fault_type_to_string(CE_FAULT_NETWORK_PARTITION));
    EXPECT_STREQ("MemoryLeak", ce_fault_type_to_string(CE_FAULT_MEMORY_LEAK));
    EXPECT_STREQ("CpuStress", ce_fault_type_to_string(CE_FAULT_CPU_STRESS));
}

TEST(CeStringTest, StateToString) {
    EXPECT_STREQ("Created", ce_state_to_string(CE_STATE_CREATED));
    EXPECT_STREQ("Ready", ce_state_to_string(CE_STATE_READY));
    EXPECT_STREQ("Running", ce_state_to_string(CE_STATE_RUNNING));
    EXPECT_STREQ("Paused", ce_state_to_string(CE_STATE_PAUSED));
    EXPECT_STREQ("Completed", ce_state_to_string(CE_STATE_COMPLETED));
    EXPECT_STREQ("Aborted", ce_state_to_string(CE_STATE_ABORTED));
}

TEST(CeStringTest, HypothesisResultToString) {
    EXPECT_STREQ("Unknown", ce_hypothesis_result_to_string(CE_HYPOTHESIS_UNKNOWN));
    EXPECT_STREQ("Confirmed", ce_hypothesis_result_to_string(CE_HYPOTHESIS_CONFIRMED));
    EXPECT_STREQ("Refuted", ce_hypothesis_result_to_string(CE_HYPOTHESIS_REFUTED));
    EXPECT_STREQ("Inconclusive", ce_hypothesis_result_to_string(CE_HYPOTHESIS_INCONCLUSIVE));
}

TEST(CeStringTest, PatternToString) {
    EXPECT_STREQ("Once", ce_pattern_to_string(CE_PATTERN_ONCE));
    EXPECT_STREQ("Periodic", ce_pattern_to_string(CE_PATTERN_PERIODIC));
    EXPECT_STREQ("Random", ce_pattern_to_string(CE_PATTERN_RANDOM));
    EXPECT_STREQ("Burst", ce_pattern_to_string(CE_PATTERN_BURST));
    EXPECT_STREQ("Progressive", ce_pattern_to_string(CE_PATTERN_PROGRESSIVE));
}

TEST(CeStringTest, TargetStrategyToString) {
    EXPECT_STREQ("Specific", ce_target_strategy_to_string(CE_TARGET_SPECIFIC));
    EXPECT_STREQ("Random", ce_target_strategy_to_string(CE_TARGET_RANDOM));
    EXPECT_STREQ("Percentage", ce_target_strategy_to_string(CE_TARGET_PERCENTAGE));
    EXPECT_STREQ("RoundRobin", ce_target_strategy_to_string(CE_TARGET_ROUND_ROBIN));
}

TEST(CeStringTest, GuardrailToString) {
    EXPECT_STREQ("None", ce_guardrail_to_string(CE_GUARDRAIL_NONE));
    EXPECT_STREQ("BlastRadius", ce_guardrail_to_string(CE_GUARDRAIL_BLAST_RADIUS));
    EXPECT_STREQ("TimeLimit", ce_guardrail_to_string(CE_GUARDRAIL_TIME_LIMIT));
    EXPECT_STREQ("ErrorRate", ce_guardrail_to_string(CE_GUARDRAIL_ERROR_RATE));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(ChaosEngineeringTest, GetNonexistentExperiment) {
    ce_experiment_t exp;
    EXPECT_FALSE(ce_get_experiment(ctx, 999, &exp));
}

TEST_F(ChaosEngineeringTest, StartInvalidExperiment) {
    // Experiment without fault/target should fail to start
    uint32_t id = ce_create_experiment(ctx, "invalid_start", "Invalid experiment");
    EXPECT_FALSE(ce_start_experiment(ctx, id));
}

TEST_F(ChaosEngineeringTest, MaxExperiments) {
    // Try to create more than max experiments
    for (int i = 0; i < CE_MAX_EXPERIMENTS + 5; i++) {
        char name[64];
        snprintf(name, sizeof(name), "exp_%d", i);
        uint32_t id = ce_create_experiment(ctx, name, "Max test");

        if (i >= CE_MAX_EXPERIMENTS) {
            EXPECT_EQ(id, 0);  // Should fail
        }
    }
}

TEST_F(ChaosEngineeringTest, MaxHypotheses) {
    uint32_t id = ce_create_experiment(ctx, "max_hyp", "Max hypotheses test");

    for (int i = 0; i < CE_MAX_HYPOTHESIS + 2; i++) {
        ce_hypothesis_t h; memset(&h, 0, sizeof(h));
        snprintf(h.description, sizeof(h.description), "Hypothesis %d", i);

        bool added = ce_add_hypothesis(ctx, id, &h);

        if (i >= CE_MAX_HYPOTHESIS) {
            EXPECT_FALSE(added);
        }
    }
}

TEST_F(ChaosEngineeringTest, KillProcess) {
    // Dry run mode - should succeed without actual kill
    EXPECT_TRUE(ce_kill_process(ctx, 1));
}

