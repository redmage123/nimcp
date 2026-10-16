/**
 * @file test_imagination_reasoning_bridge.cpp
 * @brief Unit tests for Imagination-Reasoning Cognitive Hub Bridge module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Comprehensive tests for Imagination-Reasoning Hub bidirectional integration
 * WHY:  Ensure imagination and reasoning integrate correctly via cognitive event system
 * HOW:  Test lifecycle, connection, events, counterfactual analysis, creative inference, and statistics
 *
 * TEST COVERAGE:
 * - Bridge Creation/Destruction
 * - Config Validation
 * - Hub Connection/Disconnection
 * - Register/Unregister with Hub
 * - Counterfactual Analysis Requests
 * - Simulation Result Publishing
 * - Creative Inference Requests
 * - Bidirectional Event Flow
 * - Statistics Tracking
 * - Thread Safety
 * - Null Parameter Handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/integration/nimcp_imagination_reasoning_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Global Test Helpers
 * ============================================================================ */

static std::atomic<int> g_counterfactual_callback_count{0};
static std::atomic<int> g_simulation_callback_count{0};
static std::atomic<int> g_creative_callback_count{0};
static std::atomic<int> g_insight_callback_count{0};

static counterfactual_result_t g_last_counterfactual_result;
static simulation_result_t g_last_simulation_result;
static creative_inference_result_t g_last_creative_result;
static imagination_insight_t g_last_insight;

/**
 * Test callback for counterfactual results
 */
static int test_counterfactual_callback(
    const counterfactual_result_t* result,
    void* user_data
) {
    (void)user_data;
    g_counterfactual_callback_count++;
    if (result) {
        g_last_counterfactual_result = *result;
    }
    return 0;
}

/**
 * Test callback for simulation results
 */
static int test_simulation_callback(
    const simulation_result_t* result,
    void* user_data
) {
    (void)user_data;
    g_simulation_callback_count++;
    if (result) {
        g_last_simulation_result = *result;
    }
    return 0;
}

/**
 * Test callback for creative inference results
 */
static int test_creative_callback(
    const creative_inference_result_t* result,
    void* user_data
) {
    (void)user_data;
    g_creative_callback_count++;
    if (result) {
        g_last_creative_result = *result;
    }
    return 0;
}

/**
 * Test callback for imagination insights
 */
static int test_insight_callback(
    const imagination_insight_t* insight,
    void* user_data
) {
    (void)user_data;
    g_insight_callback_count++;
    if (insight) {
        g_last_insight = *insight;
    }
    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImaginationReasoningBridgeTest : public ::testing::Test {
protected:
    imagination_reasoning_bridge_t* bridge = nullptr;
    imagination_reasoning_config_t config;
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        // Reset global state
        g_counterfactual_callback_count = 0;
        g_simulation_callback_count = 0;
        g_creative_callback_count = 0;
        g_insight_callback_count = 0;
        memset(&g_last_counterfactual_result, 0, sizeof(g_last_counterfactual_result));
        memset(&g_last_simulation_result, 0, sizeof(g_last_simulation_result));
        memset(&g_last_creative_result, 0, sizeof(g_last_creative_result));
        memset(&g_last_insight, 0, sizeof(g_last_insight));

        // Get default config
        int result = imagination_reasoning_bridge_default_config(&config);
        ASSERT_EQ(result, 0) << "Default config should succeed";

        // Create bridge
        bridge = imagination_reasoning_bridge_create(&config);

        // Create cognitive hub for connection tests
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            imagination_reasoning_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (hub != nullptr) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ============================================================================
 * Bridge Creation/Destruction Tests
 * ============================================================================ */

/**
 * Test: BridgeCreation
 * Verify bridge can be created successfully
 */
TEST_F(ImaginationReasoningBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr) << "Bridge creation should succeed";

    // Verify not connected initially
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(bridge))
        << "Bridge should not be connected initially";

    // Verify initial state
    EXPECT_EQ(imagination_reasoning_bridge_get_state(bridge), IMAG_REASON_STATE_IDLE)
        << "Initial state should be IDLE";
}

/**
 * Test: BridgeCreationNullConfig
 * Verify bridge can be created with NULL config (uses defaults)
 */
TEST_F(ImaginationReasoningBridgeTest, BridgeCreationNullConfig) {
    imagination_reasoning_bridge_t* br = imagination_reasoning_bridge_create(nullptr);
    ASSERT_NE(br, nullptr) << "Bridge creation with NULL config should succeed";

    // Verify default module ID
    EXPECT_EQ(imagination_reasoning_bridge_get_module_id(br), IMAG_REASON_DEFAULT_MODULE_ID);

    imagination_reasoning_bridge_destroy(br);
}

/**
 * Test: BridgeDestruction
 * Verify bridge destruction is safe and handles NULL
 */
TEST_F(ImaginationReasoningBridgeTest, BridgeDestruction) {
    // Destroy the bridge created in SetUp
    imagination_reasoning_bridge_destroy(bridge);
    bridge = nullptr;

    // Destroying NULL should be safe
    imagination_reasoning_bridge_destroy(nullptr);
    SUCCEED() << "Destroying NULL bridge should be safe";
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * Test: DefaultConfig
 * Verify default configuration has sensible values
 */
TEST_F(ImaginationReasoningBridgeTest, DefaultConfig) {
    imagination_reasoning_config_t default_config;
    int result = imagination_reasoning_bridge_default_config(&default_config);
    EXPECT_EQ(result, 0) << "Default config should succeed";

    // Verify module ID
    EXPECT_EQ(default_config.module_id, IMAG_REASON_DEFAULT_MODULE_ID);

    // Verify weights
    EXPECT_GT(default_config.counterfactual_weight, 0.0f);
    EXPECT_LE(default_config.counterfactual_weight, 1.0f);
    EXPECT_GT(default_config.simulation_reasoning_weight, 0.0f);
    EXPECT_LE(default_config.simulation_reasoning_weight, 1.0f);
    EXPECT_GT(default_config.creative_inference_weight, 0.0f);
    EXPECT_LE(default_config.creative_inference_weight, 1.0f);

    // Verify boolean options
    EXPECT_TRUE(default_config.auto_subscribe_input);
    EXPECT_TRUE(default_config.auto_subscribe_attention);
    EXPECT_TRUE(default_config.enable_counterfactual);
    EXPECT_TRUE(default_config.enable_prospective);
    EXPECT_TRUE(default_config.enable_query_handling);
}

/**
 * Test: ConfigValidation
 * Verify config validation with various values
 */
TEST_F(ImaginationReasoningBridgeTest, ConfigValidation) {
    imagination_reasoning_config_t test_config;
    imagination_reasoning_bridge_default_config(&test_config);

    // Custom config values
    test_config.counterfactual_weight = 0.8f;
    test_config.simulation_reasoning_weight = 0.7f;
    test_config.creative_inference_weight = 0.4f;
    test_config.enable_logging = true;
    test_config.max_concurrent_scenarios = 32;

    imagination_reasoning_bridge_t* br = imagination_reasoning_bridge_create(&test_config);
    ASSERT_NE(br, nullptr) << "Bridge creation with custom config should succeed";

    imagination_reasoning_bridge_destroy(br);
}

/**
 * Test: DefaultConfigNull
 * Verify default_config handles NULL gracefully
 */
TEST_F(ImaginationReasoningBridgeTest, DefaultConfigNull) {
    int result = imagination_reasoning_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1) << "NULL config should fail";
}

/* ============================================================================
 * Hub Registration Tests
 * ============================================================================ */

/**
 * Test: RegisterWithHub
 * Verify bridge can register with cognitive hub
 */
TEST_F(ImaginationReasoningBridgeTest, RegisterWithHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    int result = imagination_reasoning_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "Registration should succeed";

    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(bridge))
        << "Bridge should be connected after registration";
}

/**
 * Test: UnregisterFromHub
 * Verify bridge can unregister from cognitive hub
 */
TEST_F(ImaginationReasoningBridgeTest, UnregisterFromHub) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Register first
    int result = imagination_reasoning_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(result, 0) << "Registration required for unregister test";

    // Unregister
    result = imagination_reasoning_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, 0) << "Unregistration should succeed";

    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(bridge))
        << "Bridge should not be connected after unregistration";
}

/**
 * Test: ConnectDisconnect
 * Verify connect/disconnect aliases work correctly
 */
TEST_F(ImaginationReasoningBridgeTest, ConnectDisconnect) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect (alias)
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0) << "Connect should succeed";
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(bridge));

    // Disconnect (alias)
    result = imagination_reasoning_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0) << "Disconnect should succeed";
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(bridge));
}

/**
 * Test: RegisterWithHubNullParams
 * Verify registration handles NULL parameters gracefully
 */
TEST_F(ImaginationReasoningBridgeTest, RegisterWithHubNullParams) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // NULL bridge
    int result = imagination_reasoning_bridge_register_with_hub(nullptr, hub);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    // NULL hub
    result = imagination_reasoning_bridge_register_with_hub(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL hub should fail";
}

/**
 * Test: DuplicateRegistration
 * Verify registering when already registered is handled
 */
TEST_F(ImaginationReasoningBridgeTest, DuplicateRegistration) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // First registration
    int result = imagination_reasoning_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, 0) << "First registration should succeed";

    // Second registration - should fail
    result = imagination_reasoning_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(result, -1) << "Duplicate registration should fail";
}

/**
 * Test: UnregisterNotRegistered
 * Verify unregistering when not registered is handled
 */
TEST_F(ImaginationReasoningBridgeTest, UnregisterNotRegistered) {
    ASSERT_NE(bridge, nullptr);

    // Unregister without registering first
    int result = imagination_reasoning_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(result, -1) << "Unregister when not registered should fail";
}

/* ============================================================================
 * Counterfactual Analysis Tests
 * ============================================================================ */

/**
 * Test: CounterfactualAnalysisRequest
 * Verify counterfactual analysis can be requested
 */
TEST_F(ImaginationReasoningBridgeTest, CounterfactualAnalysisRequest) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0) << "Connection required for counterfactual test";

    // Create scenario
    counterfactual_scenario_t scenario;
    memset(&scenario, 0, sizeof(scenario));
    scenario.scenario_id = 12345;
    strncpy(scenario.description, "What if gravity were weaker?",
            IMAG_REASON_MAX_SCENARIO_LEN - 1);
    scenario.complexity = 0.7f;
    scenario.variable_count = 3;
    scenario.timestamp_us = 0;  /* Will be set automatically */

    // Request analysis
    result = imagination_reasoning_request_counterfactual_analysis(bridge, &scenario);
    EXPECT_EQ(result, 0) << "Counterfactual request should succeed";

    // Check statistics
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.counterfactual_queries, 1u)
        << "Should have at least 1 counterfactual query";
    EXPECT_GE(stats.events_published, 1u)
        << "Should have published at least 1 event";
}

/**
 * Test: CounterfactualCallback
 * Verify counterfactual callback can be set and cleared
 */
TEST_F(ImaginationReasoningBridgeTest, CounterfactualCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set callback
    int result = imagination_reasoning_set_counterfactual_callback(
        bridge, test_counterfactual_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = imagination_reasoning_set_counterfactual_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/**
 * Test: CounterfactualNotConnected
 * Verify counterfactual request fails when not connected
 */
TEST_F(ImaginationReasoningBridgeTest, CounterfactualNotConnected) {
    ASSERT_NE(bridge, nullptr);

    counterfactual_scenario_t scenario;
    memset(&scenario, 0, sizeof(scenario));
    scenario.scenario_id = 1;

    int result = imagination_reasoning_request_counterfactual_analysis(bridge, &scenario);
    EXPECT_EQ(result, -1) << "Request should fail when not connected";
}

/* ============================================================================
 * Simulation Result Tests
 * ============================================================================ */

/**
 * Test: SimulationResultPublishing
 * Verify simulation results can be published
 */
TEST_F(ImaginationReasoningBridgeTest, SimulationResultPublishing) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0) << "Connection required for simulation test";

    // Create simulation result
    simulation_result_t sim_result;
    memset(&sim_result, 0, sizeof(sim_result));
    sim_result.simulation_id = 54321;
    sim_result.simulation_type = 1;
    sim_result.success = true;
    sim_result.confidence = 0.85f;
    sim_result.predicted_utility = 0.75f;
    sim_result.steps = 100;
    strncpy(sim_result.description, "Mental simulation of action sequence",
            IMAG_REASON_MAX_SCENARIO_LEN - 1);

    // Publish result
    result = imagination_reasoning_publish_simulation_result(bridge, &sim_result);
    EXPECT_EQ(result, 0) << "Simulation result publishing should succeed";

    // Check statistics
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.simulation_results, 1u)
        << "Should have at least 1 simulation result";
}

/**
 * Test: SimulationCallback
 * Verify simulation callback can be set
 */
TEST_F(ImaginationReasoningBridgeTest, SimulationCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set callback
    int result = imagination_reasoning_set_simulation_callback(
        bridge, test_simulation_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = imagination_reasoning_set_simulation_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/* ============================================================================
 * Creative Inference Tests
 * ============================================================================ */

/**
 * Test: CreativeInferenceRequest
 * Verify creative inference can be requested
 */
TEST_F(ImaginationReasoningBridgeTest, CreativeInferenceRequest) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0) << "Connection required for creative inference test";

    // Create request
    creative_inference_request_t request;
    memset(&request, 0, sizeof(request));
    request.request_id = 99999;
    request.premise_count = 2;
    strncpy(request.premises[0], "All birds can fly", 255);
    strncpy(request.premises[1], "Penguins are birds", 255);
    request.novelty_target = 0.6f;
    request.constraint_strictness = 0.4f;

    // Request inference
    result = imagination_reasoning_request_creative_inference(bridge, &request);
    EXPECT_EQ(result, 0) << "Creative inference request should succeed";

    // Verify state changed to integrating
    imagination_reasoning_state_t state = imagination_reasoning_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAG_REASON_STATE_INTEGRATING)
        << "State should be INTEGRATING after creative request";
}

/**
 * Test: CreativeCallback
 * Verify creative inference callback can be set
 */
TEST_F(ImaginationReasoningBridgeTest, CreativeCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set callback
    int result = imagination_reasoning_set_creative_callback(
        bridge, test_creative_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = imagination_reasoning_set_creative_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/* ============================================================================
 * Insight Publication Tests
 * ============================================================================ */

/**
 * Test: InsightPublishing
 * Verify imagination insights can be published
 */
TEST_F(ImaginationReasoningBridgeTest, InsightPublishing) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0) << "Connection required for insight test";

    // Create insight
    imagination_insight_t insight;
    memset(&insight, 0, sizeof(insight));
    insight.insight_id = 77777;
    insight.insight_type = 1;
    strncpy(insight.description, "Novel pattern discovered in simulation",
            IMAG_REASON_MAX_INSIGHT_LEN - 1);
    insight.confidence = 0.9f;
    insight.surprise = 0.8f;
    insight.relevance = 0.7f;

    // Publish insight
    result = imagination_reasoning_publish_insight(bridge, &insight);
    EXPECT_EQ(result, 0) << "Insight publishing should succeed";

    // Check statistics
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.insights_shared, 1u)
        << "Should have at least 1 insight shared";
}

/**
 * Test: InsightCallback
 * Verify insight callback can be set
 */
TEST_F(ImaginationReasoningBridgeTest, InsightCallback) {
    ASSERT_NE(bridge, nullptr);

    int user_data = 42;

    // Set callback
    int result = imagination_reasoning_set_insight_callback(
        bridge, test_insight_callback, &user_data);
    EXPECT_EQ(result, 0) << "Setting callback should succeed";

    // Clear callback
    result = imagination_reasoning_set_insight_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Clearing callback should succeed";
}

/* ============================================================================
 * Scenario Generation Tests
 * ============================================================================ */

/**
 * Test: ScenarioGeneration
 * Verify scenarios can be generated
 */
TEST_F(ImaginationReasoningBridgeTest, ScenarioGeneration) {
    ASSERT_NE(bridge, nullptr);

    imagination_scenario_t scenario;
    int result = imagination_reasoning_generate_scenario(
        bridge,
        IMAG_SCENARIO_COUNTERFACTUAL,
        0.5f,
        &scenario
    );
    EXPECT_EQ(result, 0) << "Scenario generation should succeed";

    // Verify scenario properties
    EXPECT_NE(scenario.scenario_id, 0u) << "Scenario ID should be assigned";
    EXPECT_EQ(scenario.type, IMAG_SCENARIO_COUNTERFACTUAL);
    EXPECT_FLOAT_EQ(scenario.complexity, 0.5f);
    EXPECT_NE(scenario.creation_time, 0u) << "Creation time should be set";

    // Verify state changed
    imagination_reasoning_state_t state = imagination_reasoning_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAG_REASON_STATE_IMAGINING);

    // Verify active scenario count
    uint32_t active = imagination_reasoning_bridge_get_active_scenarios(bridge);
    EXPECT_EQ(active, 1u);
}

/**
 * Test: ScenarioAnalysis
 * Verify scenarios can be analyzed
 */
TEST_F(ImaginationReasoningBridgeTest, ScenarioAnalysis) {
    ASSERT_NE(bridge, nullptr);

    // Generate scenario first
    imagination_scenario_t scenario;
    int result = imagination_reasoning_generate_scenario(
        bridge,
        IMAG_SCENARIO_PROSPECTIVE,
        0.6f,
        &scenario
    );
    ASSERT_EQ(result, 0);

    // Set plausibility and relevance for analysis
    scenario.plausibility = 0.8f;
    scenario.relevance = 0.7f;

    // Analyze scenario
    imagination_analysis_result_t analysis;
    result = imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);
    EXPECT_EQ(result, 0) << "Scenario analysis should succeed";

    // Verify analysis results
    EXPECT_EQ(analysis.scenario_id, scenario.scenario_id);
    EXPECT_GT(analysis.logical_consistency, 0.0f);
    EXPECT_LE(analysis.logical_consistency, 1.0f);
    EXPECT_GT(analysis.utility_estimate, 0.0f);

    // Verify statistics
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.scenarios_generated, 1u);
    EXPECT_GE(stats.scenarios_analyzed, 1u);
}

/**
 * Test: ScenarioResultPublishing
 * Verify analysis results can be published
 */
TEST_F(ImaginationReasoningBridgeTest, ScenarioResultPublishing) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect first
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0);

    // Create analysis result
    imagination_analysis_result_t analysis;
    analysis.scenario_id = 12345;
    analysis.logical_consistency = 0.85f;
    analysis.utility_estimate = 0.7f;
    analysis.confidence = 0.8f;
    analysis.feasible = true;

    // Publish result
    result = imagination_reasoning_publish_result(bridge, &analysis);
    EXPECT_EQ(result, 0) << "Result publishing should succeed";
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * Test: StatisticsTracking
 * Verify statistics are tracked correctly
 */
TEST_F(ImaginationReasoningBridgeTest, StatisticsTracking) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect and perform operations
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0);

    // Generate and analyze a scenario
    imagination_scenario_t scenario;
    imagination_reasoning_generate_scenario(bridge, IMAG_SCENARIO_HYPOTHETICAL, 0.5f, &scenario);
    scenario.plausibility = 0.7f;
    scenario.relevance = 0.6f;

    imagination_analysis_result_t analysis;
    imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);

    // Get stats
    imagination_reasoning_stats_t stats;
    result = imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Get stats should succeed";

    // Verify statistics
    EXPECT_GE(stats.scenarios_generated, 1u);
    EXPECT_GE(stats.scenarios_analyzed, 1u);
    EXPECT_GT(stats.avg_plausibility, 0.0f);
    EXPECT_GT(stats.avg_utility, 0.0f);
}

/**
 * Test: StatisticsReset
 * Verify statistics can be reset
 */
TEST_F(ImaginationReasoningBridgeTest, StatisticsReset) {
    ASSERT_NE(bridge, nullptr);

    // Generate a scenario to create some stats
    imagination_scenario_t scenario;
    imagination_reasoning_generate_scenario(bridge, IMAG_SCENARIO_EPISODIC, 0.5f, &scenario);

    // Reset stats
    int result = imagination_reasoning_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0) << "Reset stats should succeed";

    // Verify reset
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.scenarios_generated, 0u);
    EXPECT_EQ(stats.scenarios_analyzed, 0u);
    EXPECT_EQ(stats.events_published, 0u);
}

/**
 * Test: StatisticsNull
 * Verify get_stats handles NULL parameters
 */
TEST_F(ImaginationReasoningBridgeTest, StatisticsNull) {
    imagination_reasoning_stats_t stats;

    int result = imagination_reasoning_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "NULL bridge should fail";

    result = imagination_reasoning_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1) << "NULL stats output should fail";
}

/* ============================================================================
 * Bidirectional Event Flow Tests
 * ============================================================================ */

/**
 * Test: BidirectionalEventFlow
 * Verify events flow between imagination and reasoning
 */
TEST_F(ImaginationReasoningBridgeTest, BidirectionalEventFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0);

    // Set up callbacks
    imagination_reasoning_set_counterfactual_callback(
        bridge, test_counterfactual_callback, nullptr);
    imagination_reasoning_set_simulation_callback(
        bridge, test_simulation_callback, nullptr);
    imagination_reasoning_set_creative_callback(
        bridge, test_creative_callback, nullptr);
    imagination_reasoning_set_insight_callback(
        bridge, test_insight_callback, nullptr);

    // Perform multiple operations
    counterfactual_scenario_t cf_scenario;
    memset(&cf_scenario, 0, sizeof(cf_scenario));
    cf_scenario.scenario_id = 1;
    cf_scenario.complexity = 0.5f;
    imagination_reasoning_request_counterfactual_analysis(bridge, &cf_scenario);

    simulation_result_t sim;
    memset(&sim, 0, sizeof(sim));
    sim.simulation_id = 2;
    sim.success = true;
    sim.confidence = 0.8f;
    imagination_reasoning_publish_simulation_result(bridge, &sim);

    creative_inference_request_t creative;
    memset(&creative, 0, sizeof(creative));
    creative.request_id = 3;
    creative.novelty_target = 0.5f;
    imagination_reasoning_request_creative_inference(bridge, &creative);

    imagination_insight_t insight;
    memset(&insight, 0, sizeof(insight));
    insight.insight_id = 4;
    insight.confidence = 0.9f;
    imagination_reasoning_publish_insight(bridge, &insight);

    // Check statistics
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.events_published, 4u)
        << "Should have published at least 4 events";
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

/**
 * Test: ThreadSafety
 * Verify bridge operations are thread-safe
 */
TEST_F(ImaginationReasoningBridgeTest, ThreadSafety) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    ASSERT_EQ(result, 0);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 25;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    // Create threads that concurrently access the bridge
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, t, ITERATIONS]() {
            for (int i = 0; i < ITERATIONS; i++) {
                // Read operations
                imagination_reasoning_bridge_is_connected(bridge);
                imagination_reasoning_bridge_get_state(bridge);
                imagination_reasoning_bridge_get_active_scenarios(bridge);

                imagination_reasoning_stats_t stats;
                imagination_reasoning_bridge_get_stats(bridge, &stats);

                // Write operations
                imagination_scenario_t scenario;
                imagination_reasoning_generate_scenario(
                    bridge,
                    (imagination_scenario_type_t)(t % IMAG_SCENARIO_COUNT),
                    0.5f,
                    &scenario
                );

                scenario.plausibility = 0.6f;
                scenario.relevance = 0.7f;

                imagination_analysis_result_t analysis;
                imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);
            }
            completed++;
        });
    }

    // Wait for all threads to complete
    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS)
        << "All threads should complete successfully";
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

/**
 * Test: NullHandling
 * Comprehensive test for NULL parameter handling across all functions
 */
TEST_F(ImaginationReasoningBridgeTest, NullHandling) {
    // Lifecycle
    imagination_reasoning_bridge_destroy(nullptr);
    EXPECT_EQ(imagination_reasoning_bridge_default_config(nullptr), -1);

    // Connection
    EXPECT_EQ(imagination_reasoning_bridge_register_with_hub(nullptr, hub), -1);
    EXPECT_EQ(imagination_reasoning_bridge_register_with_hub(bridge, nullptr), -1);
    EXPECT_EQ(imagination_reasoning_bridge_unregister_from_hub(nullptr), -1);
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(nullptr));

    // Module connections
    EXPECT_EQ(imagination_reasoning_bridge_set_imagination(nullptr, nullptr), -1);
    EXPECT_EQ(imagination_reasoning_bridge_set_reasoning(nullptr, nullptr), -1);

    // Counterfactual
    counterfactual_scenario_t scenario;
    memset(&scenario, 0, sizeof(scenario));
    EXPECT_EQ(imagination_reasoning_request_counterfactual_analysis(nullptr, &scenario), -1);
    EXPECT_EQ(imagination_reasoning_request_counterfactual_analysis(bridge, nullptr), -1);
    EXPECT_EQ(imagination_reasoning_set_counterfactual_callback(nullptr, nullptr, nullptr), -1);

    // Simulation
    simulation_result_t sim;
    memset(&sim, 0, sizeof(sim));
    EXPECT_EQ(imagination_reasoning_publish_simulation_result(nullptr, &sim), -1);
    EXPECT_EQ(imagination_reasoning_publish_simulation_result(bridge, nullptr), -1);
    EXPECT_EQ(imagination_reasoning_set_simulation_callback(nullptr, nullptr, nullptr), -1);

    // Creative inference
    creative_inference_request_t creative;
    memset(&creative, 0, sizeof(creative));
    EXPECT_EQ(imagination_reasoning_request_creative_inference(nullptr, &creative), -1);
    EXPECT_EQ(imagination_reasoning_request_creative_inference(bridge, nullptr), -1);
    EXPECT_EQ(imagination_reasoning_set_creative_callback(nullptr, nullptr, nullptr), -1);

    // Insight
    imagination_insight_t insight;
    memset(&insight, 0, sizeof(insight));
    EXPECT_EQ(imagination_reasoning_publish_insight(nullptr, &insight), -1);
    EXPECT_EQ(imagination_reasoning_publish_insight(bridge, nullptr), -1);
    EXPECT_EQ(imagination_reasoning_set_insight_callback(nullptr, nullptr, nullptr), -1);

    // Scenario generation
    imagination_scenario_t gen_scenario;
    EXPECT_EQ(imagination_reasoning_generate_scenario(nullptr, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &gen_scenario), -1);
    EXPECT_EQ(imagination_reasoning_generate_scenario(bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, nullptr), -1);

    // Scenario analysis
    imagination_analysis_result_t analysis;
    imagination_reasoning_generate_scenario(bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &gen_scenario);
    EXPECT_EQ(imagination_reasoning_analyze_scenario(nullptr, &gen_scenario, &analysis), -1);
    EXPECT_EQ(imagination_reasoning_analyze_scenario(bridge, nullptr, &analysis), -1);
    EXPECT_EQ(imagination_reasoning_analyze_scenario(bridge, &gen_scenario, nullptr), -1);

    // Query
    EXPECT_EQ(imagination_reasoning_bridge_get_state(nullptr), IMAG_REASON_STATE_ERROR);
    EXPECT_EQ(imagination_reasoning_bridge_get_module_id(nullptr), 0u);
    EXPECT_EQ(imagination_reasoning_bridge_get_active_scenarios(nullptr), 0u);

    // Stats
    imagination_reasoning_stats_t stats;
    EXPECT_EQ(imagination_reasoning_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(imagination_reasoning_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(imagination_reasoning_bridge_reset_stats(nullptr), -1);

    // Force update
    EXPECT_EQ(imagination_reasoning_bridge_force_update(nullptr), -1);

    // Event handler
    EXPECT_EQ(imagination_reasoning_on_event(nullptr, bridge), -1);
    EXPECT_EQ(imagination_reasoning_on_event((void*)&stats, nullptr), -1);

    SUCCEED() << "All NULL handling tests passed";
}

/* ============================================================================
 * Event Type String Tests
 * ============================================================================ */

/**
 * Test: EventTypeToString
 * Verify event type string conversion
 */
TEST_F(ImaginationReasoningBridgeTest, EventTypeToString) {
    EXPECT_STREQ(imag_reason_event_type_to_string(IMAG_REASON_EVENT_COUNTERFACTUAL_REQUEST),
                 "COUNTERFACTUAL_REQUEST");
    EXPECT_STREQ(imag_reason_event_type_to_string(IMAG_REASON_EVENT_COUNTERFACTUAL_RESULT),
                 "COUNTERFACTUAL_RESULT");
    EXPECT_STREQ(imag_reason_event_type_to_string(IMAG_REASON_EVENT_SIMULATION_RESULT),
                 "SIMULATION_RESULT");
    EXPECT_STREQ(imag_reason_event_type_to_string(IMAG_REASON_EVENT_CREATIVE_REQUEST),
                 "CREATIVE_REQUEST");
    EXPECT_STREQ(imag_reason_event_type_to_string(IMAG_REASON_EVENT_CREATIVE_RESULT),
                 "CREATIVE_RESULT");
    EXPECT_STREQ(imag_reason_event_type_to_string(IMAG_REASON_EVENT_INSIGHT_PUBLISHED),
                 "INSIGHT_PUBLISHED");

    // Invalid type
    EXPECT_STREQ(imag_reason_event_type_to_string((imag_reason_event_type_t)999),
                 "UNKNOWN");
}

/* ============================================================================
 * Module Connection Tests
 * ============================================================================ */

/**
 * Test: SetEngines
 * Verify engine references can be set
 */
TEST_F(ImaginationReasoningBridgeTest, SetEngines) {
    ASSERT_NE(bridge, nullptr);

    // Set imagination engine (NULL to clear)
    int result = imagination_reasoning_bridge_set_imagination(bridge, nullptr);
    EXPECT_EQ(result, 0) << "Setting imagination engine should succeed";

    // Set reasoning engine (NULL to clear)
    result = imagination_reasoning_bridge_set_reasoning(bridge, nullptr);
    EXPECT_EQ(result, 0) << "Setting reasoning engine should succeed";
}

/* ============================================================================
 * Force Update Tests
 * ============================================================================ */

/**
 * Test: ForceUpdate
 * Verify force update works correctly
 */
TEST_F(ImaginationReasoningBridgeTest, ForceUpdate) {
    ASSERT_NE(bridge, nullptr);

    // Generate scenario to change state
    imagination_scenario_t scenario;
    imagination_reasoning_generate_scenario(bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &scenario);

    // Manually analyze to clear active scenarios
    scenario.plausibility = 0.5f;
    scenario.relevance = 0.5f;
    imagination_analysis_result_t analysis;
    imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);

    // Force update
    int result = imagination_reasoning_bridge_force_update(bridge);
    EXPECT_EQ(result, 0) << "Force update should succeed";

    // State should be idle since no active scenarios
    imagination_reasoning_state_t state = imagination_reasoning_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAG_REASON_STATE_IDLE);
}

/* ============================================================================
 * Reconnection Tests
 * ============================================================================ */

/**
 * Test: ReconnectAfterDisconnect
 * Verify bridge can reconnect after disconnect
 */
TEST_F(ImaginationReasoningBridgeTest, ReconnectAfterDisconnect) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0);

    // Disconnect
    result = imagination_reasoning_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);

    // Reconnect
    result = imagination_reasoning_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0) << "Reconnect should succeed";
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(bridge));
}

/* ============================================================================
 * Full Integration Flow Test
 * ============================================================================ */

/**
 * Test: FullIntegrationFlow
 * Test complete flow: connect, operations, disconnect
 */
TEST_F(ImaginationReasoningBridgeTest, FullIntegrationFlow) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(hub, nullptr);

    // Connect
    int result = imagination_reasoning_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(bridge));

    // Set callbacks
    imagination_reasoning_set_counterfactual_callback(
        bridge, test_counterfactual_callback, nullptr);
    imagination_reasoning_set_simulation_callback(
        bridge, test_simulation_callback, nullptr);
    imagination_reasoning_set_creative_callback(
        bridge, test_creative_callback, nullptr);
    imagination_reasoning_set_insight_callback(
        bridge, test_insight_callback, nullptr);

    // Generate and analyze scenario
    imagination_scenario_t scenario;
    result = imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.7f, &scenario);
    EXPECT_EQ(result, 0);

    scenario.plausibility = 0.8f;
    scenario.relevance = 0.6f;

    imagination_analysis_result_t analysis;
    result = imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);
    EXPECT_EQ(result, 0);

    // Publish result
    result = imagination_reasoning_publish_result(bridge, &analysis);
    EXPECT_EQ(result, 0);

    // Request counterfactual analysis
    counterfactual_scenario_t cf;
    memset(&cf, 0, sizeof(cf));
    cf.scenario_id = 100;
    cf.complexity = 0.5f;
    result = imagination_reasoning_request_counterfactual_analysis(bridge, &cf);
    EXPECT_EQ(result, 0);

    // Publish simulation result
    simulation_result_t sim;
    memset(&sim, 0, sizeof(sim));
    sim.simulation_id = 200;
    sim.success = true;
    sim.confidence = 0.9f;
    result = imagination_reasoning_publish_simulation_result(bridge, &sim);
    EXPECT_EQ(result, 0);

    // Request creative inference
    creative_inference_request_t creative;
    memset(&creative, 0, sizeof(creative));
    creative.request_id = 300;
    creative.novelty_target = 0.7f;
    result = imagination_reasoning_request_creative_inference(bridge, &creative);
    EXPECT_EQ(result, 0);

    // Publish insight
    imagination_insight_t insight;
    memset(&insight, 0, sizeof(insight));
    insight.insight_id = 400;
    insight.confidence = 0.85f;
    result = imagination_reasoning_publish_insight(bridge, &insight);
    EXPECT_EQ(result, 0);

    // Check final stats
    imagination_reasoning_stats_t stats;
    result = imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.events_published, 5u);
    EXPECT_GE(stats.scenarios_generated, 1u);
    EXPECT_GE(stats.scenarios_analyzed, 1u);

    // Disconnect
    result = imagination_reasoning_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(bridge));
}
