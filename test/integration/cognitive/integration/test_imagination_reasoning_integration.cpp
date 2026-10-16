/**
 * @file test_imagination_reasoning_integration.cpp
 * @brief Integration tests for Imagination-Reasoning Cognitive Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Integration tests for full imagination-reasoning bridge workflows
 * WHY:  Validate end-to-end integration scenarios between imagination and reasoning
 * HOW:  Test complete pipelines including counterfactual reasoning, simulation-guided
 *       inference, creative logical leaps, and scenario planning
 *
 * TEST SCENARIOS:
 * - CounterfactualReasoningFlow: Full counterfactual analysis pipeline
 * - SimulationGuidedInference: Reasoning uses simulation results
 * - CreativeLogicalLeaps: Novel inferences from imagination
 * - ScenarioPlanningIntegration: Planning with what-if analysis
 * - AbstractReasoningFromConcrete: Generalize from imagined specifics
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

#include "cognitive/integration/nimcp_imagination_reasoning_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImaginationReasoningIntegrationTest : public ::testing::Test {
protected:
    imagination_reasoning_bridge_t* bridge = nullptr;
    cognitive_integration_hub_t hub = nullptr;

    /* Event tracking */
    std::atomic<int> counterfactual_events{0};
    std::atomic<int> simulation_events{0};
    std::atomic<int> creative_events{0};
    std::atomic<int> insight_events{0};
    std::atomic<int> total_events_published{0};

    /* Last received data */
    counterfactual_result_t last_counterfactual;
    simulation_result_t last_simulation;
    creative_inference_result_t last_creative;
    imagination_insight_t last_insight;

    void SetUp() override {
        /* Reset tracking */
        counterfactual_events = 0;
        simulation_events = 0;
        creative_events = 0;
        insight_events = 0;
        total_events_published = 0;

        memset(&last_counterfactual, 0, sizeof(last_counterfactual));
        memset(&last_simulation, 0, sizeof(last_simulation));
        memset(&last_creative, 0, sizeof(last_creative));
        memset(&last_insight, 0, sizeof(last_insight));

        /* Create cognitive hub */
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub_config.max_modules = 32;
        hub_config.max_subscriptions = 128;
        hub_config.enable_async = true;  // Enable async event delivery for priority handling
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr) << "Hub creation required for integration tests";

        /* Create bridge with custom config */
        imagination_reasoning_config_t bridge_config;
        imagination_reasoning_bridge_default_config(&bridge_config);
        bridge_config.enable_counterfactual = true;
        bridge_config.enable_prospective = true;
        bridge_config.enable_query_handling = true;
        bridge_config.max_concurrent_scenarios = 32;
        bridge_config.counterfactual_weight = 0.6f;
        bridge_config.simulation_reasoning_weight = 0.5f;
        bridge_config.creative_inference_weight = 0.4f;

        bridge = imagination_reasoning_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr) << "Bridge creation required for integration tests";

        /* Connect bridge to hub */
        int result = imagination_reasoning_bridge_connect(bridge, hub);
        ASSERT_EQ(result, 0) << "Bridge connection required for integration tests";
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

    /**
     * Helper: Register callbacks for event tracking
     */
    void RegisterCallbacks() {
        imagination_reasoning_set_counterfactual_callback(
            bridge,
            [](const counterfactual_result_t* result, void* user_data) -> int {
                auto* test = static_cast<ImaginationReasoningIntegrationTest*>(user_data);
                test->counterfactual_events++;
                if (result) test->last_counterfactual = *result;
                return 0;
            },
            this
        );

        imagination_reasoning_set_simulation_callback(
            bridge,
            [](const simulation_result_t* result, void* user_data) -> int {
                auto* test = static_cast<ImaginationReasoningIntegrationTest*>(user_data);
                test->simulation_events++;
                if (result) test->last_simulation = *result;
                return 0;
            },
            this
        );

        imagination_reasoning_set_creative_callback(
            bridge,
            [](const creative_inference_result_t* result, void* user_data) -> int {
                auto* test = static_cast<ImaginationReasoningIntegrationTest*>(user_data);
                test->creative_events++;
                if (result) test->last_creative = *result;
                return 0;
            },
            this
        );

        imagination_reasoning_set_insight_callback(
            bridge,
            [](const imagination_insight_t* insight, void* user_data) -> int {
                auto* test = static_cast<ImaginationReasoningIntegrationTest*>(user_data);
                test->insight_events++;
                if (insight) test->last_insight = *insight;
                return 0;
            },
            this
        );
    }

    /**
     * Helper: Create counterfactual scenario
     */
    counterfactual_scenario_t CreateCounterfactualScenario(
        uint64_t id,
        const char* description,
        float complexity
    ) {
        counterfactual_scenario_t scenario;
        memset(&scenario, 0, sizeof(scenario));
        scenario.scenario_id = id;
        strncpy(scenario.description, description, IMAG_REASON_MAX_SCENARIO_LEN - 1);
        scenario.complexity = complexity;
        scenario.variable_count = (uint32_t)(complexity * 10.0f);
        return scenario;
    }

    /**
     * Helper: Create simulation result
     */
    simulation_result_t CreateSimulationResult(
        uint64_t id,
        bool success,
        float confidence,
        const char* description
    ) {
        simulation_result_t result;
        memset(&result, 0, sizeof(result));
        result.simulation_id = id;
        result.simulation_type = 1;
        result.success = success;
        result.confidence = confidence;
        result.predicted_utility = success ? 0.8f : 0.2f;
        result.steps = 50;
        strncpy(result.description, description, IMAG_REASON_MAX_SCENARIO_LEN - 1);
        return result;
    }

    /**
     * Helper: Create creative inference request
     */
    creative_inference_request_t CreateCreativeRequest(
        uint64_t id,
        float novelty,
        float strictness
    ) {
        creative_inference_request_t request;
        memset(&request, 0, sizeof(request));
        request.request_id = id;
        request.premise_count = 3;
        strncpy(request.premises[0], "Objects have mass", 255);
        strncpy(request.premises[1], "Mass creates gravity", 255);
        strncpy(request.premises[2], "Gravity attracts", 255);
        request.novelty_target = novelty;
        request.constraint_strictness = strictness;
        return request;
    }

    /**
     * Helper: Create imagination insight
     */
    imagination_insight_t CreateInsight(
        uint64_t id,
        float confidence,
        float surprise,
        const char* description
    ) {
        imagination_insight_t insight;
        memset(&insight, 0, sizeof(insight));
        insight.insight_id = id;
        insight.insight_type = 1;
        strncpy(insight.description, description, IMAG_REASON_MAX_INSIGHT_LEN - 1);
        insight.confidence = confidence;
        insight.surprise = surprise;
        insight.relevance = 0.7f;
        return insight;
    }
};

/* ============================================================================
 * Counterfactual Reasoning Flow Tests
 * ============================================================================ */

/**
 * Test: CounterfactualReasoningFlow
 * Full counterfactual analysis pipeline
 */
TEST_F(ImaginationReasoningIntegrationTest, CounterfactualReasoningFlow) {
    RegisterCallbacks();

    /* Phase 1: Generate multiple "what-if" scenarios */
    std::vector<counterfactual_scenario_t> scenarios = {
        CreateCounterfactualScenario(1, "What if the internet was never invented?", 0.8f),
        CreateCounterfactualScenario(2, "What if gravity was 10% stronger?", 0.6f),
        CreateCounterfactualScenario(3, "What if humans could photosynthesize?", 0.7f)
    };

    /* Phase 2: Submit all scenarios for analysis */
    for (const auto& scenario : scenarios) {
        int result = imagination_reasoning_request_counterfactual_analysis(bridge, &scenario);
        EXPECT_EQ(result, 0) << "Counterfactual request should succeed for scenario "
                             << scenario.scenario_id;
    }

    /* Phase 3: Verify statistics reflect submissions */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.counterfactual_queries, 3u)
        << "Should have at least 3 counterfactual queries";
    EXPECT_GE(stats.events_published, 3u)
        << "Should have published events for all scenarios";

    /* Phase 4: Verify bridge state reflects analysis */
    imagination_reasoning_state_t state = imagination_reasoning_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAG_REASON_STATE_ANALYZING)
        << "Bridge should be in ANALYZING state";
}

/**
 * Test: CounterfactualChainAnalysis
 * Test chained counterfactual scenarios
 */
TEST_F(ImaginationReasoningIntegrationTest, CounterfactualChainAnalysis) {
    RegisterCallbacks();

    /* Create a chain of dependent counterfactuals */
    auto scenario1 = CreateCounterfactualScenario(
        100, "What if renewable energy was cheaper?", 0.5f);
    auto scenario2 = CreateCounterfactualScenario(
        101, "Given cheaper renewables, what if all cars were electric?", 0.6f);
    auto scenario3 = CreateCounterfactualScenario(
        102, "Given all electric cars, what if oil production ceased?", 0.7f);

    /* Submit chain */
    imagination_reasoning_request_counterfactual_analysis(bridge, &scenario1);
    imagination_reasoning_request_counterfactual_analysis(bridge, &scenario2);
    imagination_reasoning_request_counterfactual_analysis(bridge, &scenario3);

    /* Verify all were submitted */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.counterfactual_queries, 3u);

    /* Verify complex scenario tracking */
    EXPECT_GE(stats.events_published, 3u);
}

/* ============================================================================
 * Simulation-Guided Inference Tests
 * ============================================================================ */

/**
 * Test: SimulationGuidedInference
 * Reasoning uses simulation results to guide inference
 */
TEST_F(ImaginationReasoningIntegrationTest, SimulationGuidedInference) {
    RegisterCallbacks();

    /* Phase 1: Run mental simulations */
    std::vector<simulation_result_t> simulations = {
        CreateSimulationResult(1, true, 0.9f, "Simulated successful task completion"),
        CreateSimulationResult(2, false, 0.3f, "Simulated failure case"),
        CreateSimulationResult(3, true, 0.7f, "Simulated alternative approach")
    };

    /* Phase 2: Publish all simulation results */
    for (const auto& sim : simulations) {
        int result = imagination_reasoning_publish_simulation_result(bridge, &sim);
        EXPECT_EQ(result, 0) << "Simulation publish should succeed";
    }

    /* Phase 3: Verify simulations were processed */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.simulation_results, 3u)
        << "Should have at least 3 simulation results";

    /* Phase 4: Generate scenario based on simulation insights */
    imagination_scenario_t scenario;
    int result = imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_PROSPECTIVE, 0.6f, &scenario);
    EXPECT_EQ(result, 0);

    /* Phase 5: Analyze with simulation-informed context */
    scenario.plausibility = 0.8f;  /* Higher due to successful simulations */
    scenario.relevance = 0.9f;

    imagination_analysis_result_t analysis;
    result = imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);
    EXPECT_EQ(result, 0);
    EXPECT_GT(analysis.utility_estimate, 0.5f)
        << "Utility should be high given successful simulations";
}

/**
 * Test: MultipleSimulationAggregation
 * Aggregate multiple simulation results for inference
 */
TEST_F(ImaginationReasoningIntegrationTest, MultipleSimulationAggregation) {
    RegisterCallbacks();

    /* Run many simulations with varying outcomes */
    const int NUM_SIMULATIONS = 10;
    int successful = 0;

    for (int i = 0; i < NUM_SIMULATIONS; i++) {
        bool success = (i % 3 != 0);  /* 2/3 success rate */
        if (success) successful++;

        auto sim = CreateSimulationResult(
            (uint64_t)(1000 + i),
            success,
            success ? 0.8f + (i * 0.01f) : 0.3f - (i * 0.01f),
            success ? "Successful simulation" : "Failed simulation"
        );

        imagination_reasoning_publish_simulation_result(bridge, &sim);
    }

    /* Verify aggregation */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.simulation_results, (uint32_t)NUM_SIMULATIONS);

    /* Generate scenario informed by aggregate */
    imagination_scenario_t scenario;
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_PROSPECTIVE, 0.5f, &scenario);

    /* Set plausibility based on success rate */
    scenario.plausibility = (float)successful / (float)NUM_SIMULATIONS;
    scenario.relevance = 0.8f;

    imagination_analysis_result_t analysis;
    imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);

    /* Utility should reflect aggregate success rate */
    EXPECT_GT(analysis.utility_estimate, 0.4f);
}

/* ============================================================================
 * Creative Logical Leaps Tests
 * ============================================================================ */

/**
 * Test: CreativeLogicalLeaps
 * Novel inferences from imagination
 */
TEST_F(ImaginationReasoningIntegrationTest, CreativeLogicalLeaps) {
    RegisterCallbacks();

    /* Phase 1: Prepare creative inference requests with varying parameters */
    std::vector<creative_inference_request_t> requests = {
        CreateCreativeRequest(1, 0.3f, 0.9f),  /* Low novelty, high strictness */
        CreateCreativeRequest(2, 0.7f, 0.5f),  /* High novelty, medium strictness */
        CreateCreativeRequest(3, 0.9f, 0.2f)   /* Very high novelty, low strictness */
    };

    /* Phase 2: Submit creative inference requests */
    for (const auto& req : requests) {
        int result = imagination_reasoning_request_creative_inference(bridge, &req);
        EXPECT_EQ(result, 0) << "Creative request should succeed";
    }

    /* Phase 3: Verify bridge entered integrating state */
    imagination_reasoning_state_t state = imagination_reasoning_bridge_get_state(bridge);
    EXPECT_EQ(state, IMAG_REASON_STATE_INTEGRATING)
        << "Bridge should be in INTEGRATING state for creative inference";

    /* Phase 4: Verify statistics */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.events_published, 3u);
}

/**
 * Test: CreativeInferenceWithPremises
 * Test creative inference with complex premise sets
 */
TEST_F(ImaginationReasoningIntegrationTest, CreativeInferenceWithPremises) {
    RegisterCallbacks();

    /* Create request with multiple premises */
    creative_inference_request_t request;
    memset(&request, 0, sizeof(request));
    request.request_id = 999;
    request.premise_count = 5;
    strncpy(request.premises[0], "All mammals are warm-blooded", 255);
    strncpy(request.premises[1], "Whales are mammals", 255);
    strncpy(request.premises[2], "Whales live in cold water", 255);
    strncpy(request.premises[3], "Cold water absorbs heat quickly", 255);
    strncpy(request.premises[4], "Whales have thick blubber", 255);
    request.novelty_target = 0.6f;
    request.constraint_strictness = 0.7f;

    int result = imagination_reasoning_request_creative_inference(bridge, &request);
    EXPECT_EQ(result, 0);

    /* Verify submission */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.events_published, 1u);
}

/* ============================================================================
 * Scenario Planning Integration Tests
 * ============================================================================ */

/**
 * Test: ScenarioPlanningIntegration
 * Planning with what-if analysis
 */
TEST_F(ImaginationReasoningIntegrationTest, ScenarioPlanningIntegration) {
    RegisterCallbacks();

    /* Phase 1: Generate multiple scenario types for planning */
    imagination_scenario_t scenarios[4];

    /* Counterfactual scenario */
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &scenarios[0]);
    scenarios[0].plausibility = 0.7f;
    scenarios[0].relevance = 0.8f;

    /* Prospective scenario */
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_PROSPECTIVE, 0.6f, &scenarios[1]);
    scenarios[1].plausibility = 0.6f;
    scenarios[1].relevance = 0.9f;

    /* Hypothetical scenario */
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_HYPOTHETICAL, 0.7f, &scenarios[2]);
    scenarios[2].plausibility = 0.5f;
    scenarios[2].relevance = 0.7f;

    /* Episodic scenario */
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_EPISODIC, 0.4f, &scenarios[3]);
    scenarios[3].plausibility = 0.9f;
    scenarios[3].relevance = 0.6f;

    /* Phase 2: Analyze all scenarios */
    std::vector<imagination_analysis_result_t> analyses(4);
    for (int i = 0; i < 4; i++) {
        int result = imagination_reasoning_analyze_scenario(
            bridge, &scenarios[i], &analyses[i]);
        EXPECT_EQ(result, 0);
    }

    /* Phase 3: Find best scenario based on utility */
    float max_utility = 0.0f;
    int best_idx = -1;
    for (int i = 0; i < 4; i++) {
        if (analyses[i].utility_estimate > max_utility) {
            max_utility = analyses[i].utility_estimate;
            best_idx = i;
        }
    }

    EXPECT_GE(best_idx, 0) << "Should find at least one viable scenario";
    EXPECT_GT(max_utility, 0.3f) << "Best scenario should have reasonable utility";

    /* Phase 4: Publish best scenario result */
    if (best_idx >= 0) {
        int result = imagination_reasoning_publish_result(bridge, &analyses[best_idx]);
        EXPECT_EQ(result, 0);
    }

    /* Phase 5: Verify planning statistics */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.scenarios_generated, 4u);
    EXPECT_EQ(stats.scenarios_analyzed, 4u);
    EXPECT_GE(stats.scenarios_accepted, 1u);
}

/**
 * Test: IterativeScenarioRefinement
 * Iteratively refine scenarios based on analysis
 */
TEST_F(ImaginationReasoningIntegrationTest, IterativeScenarioRefinement) {
    RegisterCallbacks();

    const int MAX_ITERATIONS = 5;
    float target_utility = 0.7f;

    imagination_scenario_t scenario;
    imagination_analysis_result_t analysis;
    float current_complexity = 0.3f;

    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        /* Generate scenario with current complexity */
        imagination_reasoning_generate_scenario(
            bridge, IMAG_SCENARIO_PROSPECTIVE, current_complexity, &scenario);

        /* Set parameters based on iteration */
        scenario.plausibility = 0.5f + (iter * 0.08f);
        scenario.relevance = 0.6f + (iter * 0.06f);

        /* Analyze */
        imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);

        /* Check if target reached */
        if (analysis.utility_estimate >= target_utility) {
            break;
        }

        /* Adjust complexity for next iteration */
        current_complexity = std::min(current_complexity + 0.1f, 0.9f);
    }

    /* Verify iterations produced results */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.scenarios_generated, 1u);
    EXPECT_GE(stats.scenarios_analyzed, 1u);
}

/* ============================================================================
 * Abstract Reasoning From Concrete Tests
 * ============================================================================ */

/**
 * Test: AbstractReasoningFromConcrete
 * Generalize from imagined specifics
 */
TEST_F(ImaginationReasoningIntegrationTest, AbstractReasoningFromConcrete) {
    RegisterCallbacks();

    /* Phase 1: Generate specific concrete scenarios (episodic) */
    std::vector<imagination_scenario_t> concrete_scenarios;
    for (int i = 0; i < 5; i++) {
        imagination_scenario_t scenario;
        imagination_reasoning_generate_scenario(
            bridge, IMAG_SCENARIO_EPISODIC, 0.3f + (i * 0.05f), &scenario);
        scenario.plausibility = 0.9f - (i * 0.05f);  /* Concrete = high plausibility */
        scenario.relevance = 0.5f;
        concrete_scenarios.push_back(scenario);
    }

    /* Phase 2: Analyze concrete scenarios */
    std::vector<imagination_analysis_result_t> concrete_analyses;
    for (auto& scenario : concrete_scenarios) {
        imagination_analysis_result_t analysis;
        imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);
        concrete_analyses.push_back(analysis);
    }

    /* Phase 3: Generate abstract scenario (hypothetical) based on patterns */
    imagination_scenario_t abstract_scenario;
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_HYPOTHETICAL, 0.7f, &abstract_scenario);

    /* Set abstract scenario parameters based on concrete findings */
    float avg_plausibility = 0.0f;
    float avg_utility = 0.0f;
    for (const auto& analysis : concrete_analyses) {
        avg_utility += analysis.utility_estimate;
    }
    for (const auto& scenario : concrete_scenarios) {
        avg_plausibility += scenario.plausibility;
    }
    avg_plausibility /= concrete_scenarios.size();
    avg_utility /= concrete_analyses.size();

    abstract_scenario.plausibility = avg_plausibility * 0.8f;  /* Abstract = lower plausibility */
    abstract_scenario.relevance = 0.9f;  /* But higher relevance for generalization */

    /* Phase 4: Analyze abstract scenario */
    imagination_analysis_result_t abstract_analysis;
    imagination_reasoning_analyze_scenario(
        bridge, &abstract_scenario, &abstract_analysis);

    /* Phase 5: Share insight about abstraction */
    auto insight = CreateInsight(
        1001,
        0.85f,
        0.6f,
        "Generalized pattern discovered from concrete instances"
    );
    imagination_reasoning_publish_insight(bridge, &insight);

    /* Verify workflow */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.scenarios_generated, 6u);  /* 5 concrete + 1 abstract */
    EXPECT_EQ(stats.scenarios_analyzed, 6u);
    EXPECT_GE(stats.insights_shared, 1u);
}

/* ============================================================================
 * Multi-Module Coordination Tests
 * ============================================================================ */

/**
 * Test: BidirectionalModuleCoordination
 * Test bidirectional event flow between modules
 */
TEST_F(ImaginationReasoningIntegrationTest, BidirectionalModuleCoordination) {
    RegisterCallbacks();

    /* Simulate imagination -> reasoning -> imagination flow */

    /* Step 1: Imagination generates scenario */
    imagination_scenario_t initial_scenario;
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &initial_scenario);
    initial_scenario.plausibility = 0.7f;
    initial_scenario.relevance = 0.8f;

    /* Step 2: Reasoning analyzes */
    imagination_analysis_result_t analysis;
    imagination_reasoning_analyze_scenario(bridge, &initial_scenario, &analysis);

    /* Step 3: Request counterfactual based on analysis */
    auto cf_scenario = CreateCounterfactualScenario(
        initial_scenario.scenario_id + 1000,
        "Extended counterfactual based on analysis",
        analysis.logical_consistency
    );
    imagination_reasoning_request_counterfactual_analysis(bridge, &cf_scenario);

    /* Step 4: Share insight from coordination */
    auto insight = CreateInsight(
        2001,
        analysis.confidence,
        0.5f,
        "Bidirectional coordination complete"
    );
    imagination_reasoning_publish_insight(bridge, &insight);

    /* Verify full coordination */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.scenarios_generated, 1u);
    EXPECT_GE(stats.scenarios_analyzed, 1u);
    EXPECT_GE(stats.counterfactual_queries, 1u);
    EXPECT_GE(stats.insights_shared, 1u);
}

/* ============================================================================
 * Concurrent Operations Tests
 * ============================================================================ */

/**
 * Test: ConcurrentOperations
 * Test concurrent imagination-reasoning operations
 */
TEST_F(ImaginationReasoningIntegrationTest, ConcurrentOperations) {
    RegisterCallbacks();

    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 10;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &completed, t, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                uint64_t base_id = (uint64_t)(t * 10000 + i);

                /* Different threads do different operations */
                switch (t % 4) {
                    case 0: {
                        /* Thread 0: Counterfactual requests */
                        auto scenario = CreateCounterfactualScenario(
                            base_id, "Concurrent counterfactual", 0.5f);
                        imagination_reasoning_request_counterfactual_analysis(
                            bridge, &scenario);
                        break;
                    }
                    case 1: {
                        /* Thread 1: Simulation results */
                        auto sim = CreateSimulationResult(
                            base_id, true, 0.8f, "Concurrent simulation");
                        imagination_reasoning_publish_simulation_result(bridge, &sim);
                        break;
                    }
                    case 2: {
                        /* Thread 2: Creative inference */
                        auto creative = CreateCreativeRequest(base_id, 0.5f, 0.5f);
                        imagination_reasoning_request_creative_inference(
                            bridge, &creative);
                        break;
                    }
                    case 3: {
                        /* Thread 3: Insight publishing */
                        auto insight = CreateInsight(
                            base_id, 0.7f, 0.5f, "Concurrent insight");
                        imagination_reasoning_publish_insight(bridge, &insight);
                        break;
                    }
                }
            }
            completed++;
        });
    }

    /* Wait for completion */
    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS);

    /* Verify statistics reflect concurrent operations */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.events_published, (uint32_t)(NUM_THREADS * OPS_PER_THREAD));
}

/* ============================================================================
 * Error Recovery Tests
 * ============================================================================ */

/**
 * Test: ErrorRecoveryScenarios
 * Test recovery from various error conditions
 */
TEST_F(ImaginationReasoningIntegrationTest, ErrorRecoveryScenarios) {
    RegisterCallbacks();

    /* Test 1: Operations after disconnect/reconnect */
    imagination_reasoning_bridge_disconnect(bridge);
    EXPECT_FALSE(imagination_reasoning_bridge_is_connected(bridge));

    /* Operations should fail while disconnected */
    auto scenario = CreateCounterfactualScenario(1, "Test", 0.5f);
    int result = imagination_reasoning_request_counterfactual_analysis(bridge, &scenario);
    EXPECT_EQ(result, -1) << "Should fail while disconnected";

    /* Reconnect */
    result = imagination_reasoning_bridge_connect(bridge, hub);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(bridge));

    /* Operations should succeed after reconnect */
    result = imagination_reasoning_request_counterfactual_analysis(bridge, &scenario);
    EXPECT_EQ(result, 0) << "Should succeed after reconnect";

    /* Test 2: Force update during operations */
    imagination_scenario_t gen_scenario;
    imagination_reasoning_generate_scenario(
        bridge, IMAG_SCENARIO_COUNTERFACTUAL, 0.5f, &gen_scenario);

    result = imagination_reasoning_bridge_force_update(bridge);
    EXPECT_EQ(result, 0);

    /* Continue operations */
    gen_scenario.plausibility = 0.7f;
    gen_scenario.relevance = 0.8f;
    imagination_analysis_result_t analysis;
    result = imagination_reasoning_analyze_scenario(bridge, &gen_scenario, &analysis);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Statistics Verification Tests
 * ============================================================================ */

/**
 * Test: ComprehensiveStatisticsVerification
 * Verify all statistics are accurately tracked
 */
TEST_F(ImaginationReasoningIntegrationTest, ComprehensiveStatisticsVerification) {
    RegisterCallbacks();

    /* Reset stats first */
    imagination_reasoning_bridge_reset_stats(bridge);

    /* Perform known number of each operation */
    const int CF_COUNT = 3;
    const int SIM_COUNT = 4;
    const int CREATIVE_COUNT = 2;
    const int INSIGHT_COUNT = 5;
    const int SCENARIO_COUNT = 6;

    /* Counterfactual requests */
    for (int i = 0; i < CF_COUNT; i++) {
        auto scenario = CreateCounterfactualScenario((uint64_t)(i + 1), "CF Test", 0.5f);
        imagination_reasoning_request_counterfactual_analysis(bridge, &scenario);
    }

    /* Simulation results */
    for (int i = 0; i < SIM_COUNT; i++) {
        auto sim = CreateSimulationResult((uint64_t)(i + 100), true, 0.8f, "Sim Test");
        imagination_reasoning_publish_simulation_result(bridge, &sim);
    }

    /* Creative inference requests */
    for (int i = 0; i < CREATIVE_COUNT; i++) {
        auto creative = CreateCreativeRequest((uint64_t)(i + 200), 0.5f, 0.5f);
        imagination_reasoning_request_creative_inference(bridge, &creative);
    }

    /* Insights */
    for (int i = 0; i < INSIGHT_COUNT; i++) {
        auto insight = CreateInsight((uint64_t)(i + 300), 0.7f, 0.5f, "Insight Test");
        imagination_reasoning_publish_insight(bridge, &insight);
    }

    /* Scenario generation and analysis */
    for (int i = 0; i < SCENARIO_COUNT; i++) {
        imagination_scenario_t scenario;
        imagination_reasoning_generate_scenario(
            bridge,
            (imagination_scenario_type_t)(i % IMAG_SCENARIO_COUNT),
            0.5f,
            &scenario
        );
        scenario.plausibility = 0.7f;
        scenario.relevance = 0.6f;

        imagination_analysis_result_t analysis;
        imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);
    }

    /* Verify all statistics */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.counterfactual_queries, (uint32_t)CF_COUNT);
    EXPECT_EQ(stats.simulation_results, (uint32_t)SIM_COUNT);
    EXPECT_EQ(stats.insights_shared, (uint32_t)INSIGHT_COUNT);
    EXPECT_EQ(stats.scenarios_generated, (uint32_t)SCENARIO_COUNT);
    EXPECT_EQ(stats.scenarios_analyzed, (uint32_t)SCENARIO_COUNT);

    /* Verify events published */
    uint32_t expected_events = CF_COUNT + SIM_COUNT + CREATIVE_COUNT + INSIGHT_COUNT;
    EXPECT_GE(stats.events_published, expected_events);

    /* Verify averages are reasonable */
    EXPECT_GT(stats.avg_plausibility, 0.0f);
    EXPECT_LE(stats.avg_plausibility, 1.0f);
    EXPECT_GT(stats.avg_utility, 0.0f);
    EXPECT_LE(stats.avg_utility, 1.0f);
}

/* ============================================================================
 * Full Pipeline Integration Test
 * ============================================================================ */

/**
 * Test: FullPipelineIntegration
 * Complete end-to-end integration test
 */
TEST_F(ImaginationReasoningIntegrationTest, FullPipelineIntegration) {
    RegisterCallbacks();

    /* Step 1: Initialize with simulations to establish baseline */
    std::vector<simulation_result_t> baseline_sims;
    for (int i = 0; i < 5; i++) {
        auto sim = CreateSimulationResult(
            (uint64_t)(i + 1),
            i % 2 == 0,  /* Alternating success/failure */
            0.5f + (i * 0.08f),
            "Baseline simulation"
        );
        baseline_sims.push_back(sim);
        imagination_reasoning_publish_simulation_result(bridge, &sim);
    }

    /* Step 2: Generate and analyze scenarios of each type */
    std::map<imagination_scenario_type_t, imagination_analysis_result_t> analyses;
    for (int type = 0; type < IMAG_SCENARIO_COUNT; type++) {
        imagination_scenario_t scenario;
        imagination_reasoning_generate_scenario(
            bridge,
            (imagination_scenario_type_t)type,
            0.5f,
            &scenario
        );
        scenario.plausibility = 0.6f + (type * 0.05f);
        scenario.relevance = 0.7f;

        imagination_analysis_result_t analysis;
        imagination_reasoning_analyze_scenario(bridge, &scenario, &analysis);
        analyses[(imagination_scenario_type_t)type] = analysis;
    }

    /* Step 3: Request counterfactual analysis for best scenario */
    imagination_scenario_type_t best_type = IMAG_SCENARIO_COUNTERFACTUAL;
    float best_utility = 0.0f;
    for (const auto& pair : analyses) {
        if (pair.second.utility_estimate > best_utility) {
            best_utility = pair.second.utility_estimate;
            best_type = pair.first;
        }
    }

    auto cf_scenario = CreateCounterfactualScenario(
        1000 + (uint64_t)best_type,
        "Deep counterfactual analysis of best scenario",
        analyses[best_type].logical_consistency
    );
    imagination_reasoning_request_counterfactual_analysis(bridge, &cf_scenario);

    /* Step 4: Request creative inference based on findings */
    creative_inference_request_t creative;
    memset(&creative, 0, sizeof(creative));
    creative.request_id = 5000;
    creative.premise_count = 2;
    strncpy(creative.premises[0], "Best scenario type identified", 255);
    strncpy(creative.premises[1], "Utility optimization possible", 255);
    creative.novelty_target = 0.7f;
    creative.constraint_strictness = 0.6f;
    imagination_reasoning_request_creative_inference(bridge, &creative);

    /* Step 5: Share comprehensive insight */
    auto insight = CreateInsight(
        9999,
        0.9f,
        0.7f,
        "Full pipeline integration completed successfully"
    );
    imagination_reasoning_publish_insight(bridge, &insight);

    /* Step 6: Publish best analysis result */
    imagination_reasoning_publish_result(bridge, &analyses[best_type]);

    /* Verify complete pipeline statistics */
    imagination_reasoning_stats_t stats;
    imagination_reasoning_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.simulation_results, 5u);
    EXPECT_EQ(stats.scenarios_generated, (uint32_t)IMAG_SCENARIO_COUNT);
    EXPECT_EQ(stats.scenarios_analyzed, (uint32_t)IMAG_SCENARIO_COUNT);
    EXPECT_GE(stats.counterfactual_queries, 1u);
    EXPECT_GE(stats.insights_shared, 1u);
    EXPECT_GT(stats.avg_utility, 0.0f);

    /* Verify bridge is still functional */
    EXPECT_TRUE(imagination_reasoning_bridge_is_connected(bridge));
    EXPECT_NE(imagination_reasoning_bridge_get_state(bridge), IMAG_REASON_STATE_ERROR);
}
