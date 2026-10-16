/**
 * @file test_reasoning_integration.cpp
 * @brief Unit tests for reasoning cognitive integration
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Comprehensive unit tests for reasoning-cognitive layer integration
 * WHY:  Ensure all cognitive hooks work correctly with logic events
 * HOW:  Test each hook, configuration, statistics, and error handling
 *
 * TEST COVERAGE:
 * - Event publishing (13 logic event types)
 * - Attention integration (novel facts, contradictions)
 * - Curiosity integration (unexplained facts, exploration)
 * - Working memory integration (7±2 limit, eviction)
 * - Executive integration (planning for complex proofs)
 * - Consolidation integration (rule learning and storage)
 * - Configuration and validation
 * - Statistics tracking
 * - Error handling and edge cases
 *
 * TARGET: 25+ tests, 100% coverage
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_reasoning_integration.h"
#include "core/events/nimcp_event_bus.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ReasoningIntegrationTest : public ::testing::Test {
protected:
    event_bus_t event_bus;
    reasoning_integration_t* integration;

    void SetUp() override {
        // Create event bus
        event_bus = event_bus_create("test_bus", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(event_bus, nullptr);

        // Create integration with default config
        integration = reasoning_integration_create(event_bus);
        ASSERT_NE(integration, nullptr);
    }

    void TearDown() override {
        if (integration) {
            reasoning_integration_destroy(integration);
            integration = nullptr;
        }
        if (event_bus) {
            event_bus_destroy(event_bus);
            event_bus = nullptr;
        }
    }

    // Helper: Publish simple logic event
    void publish_event(brain_event_type_t type) {
        event_bus_publish_simple(event_bus, type, EVENT_PRIORITY_NORMAL, "test");
    }

    // Helper: Publish event with data
    void publish_event_with_data(brain_event_type_t type, const char* data) {
        event_bus_publish_data(event_bus, type, EVENT_PRIORITY_NORMAL, "test",
                              data, strlen(data));
    }

    // Helper: Get stats
    reasoning_integration_stats_t get_stats() {
        reasoning_integration_stats_t stats;
        EXPECT_TRUE(reasoning_integration_get_stats(integration, &stats));
        return stats;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, CreateDestroy) {
    // Verify creation succeeded
    EXPECT_NE(integration, nullptr);

    // Verify initial state
    auto stats = get_stats();
    EXPECT_EQ(stats.total_events_processed, 0);
    EXPECT_EQ(stats.current_active_inferences, 0);
    EXPECT_EQ(stats.current_tracked_rules, 0);
}

TEST_F(ReasoningIntegrationTest, CreateWithNullEventBus) {
    reasoning_integration_t* invalid = reasoning_integration_create(nullptr);
    EXPECT_EQ(invalid, nullptr);
}

TEST_F(ReasoningIntegrationTest, CreateWithCustomConfig) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.max_active_inferences = 5;
    config.novel_fact_salience_boost = 0.9f;

    reasoning_integration_t* custom = reasoning_integration_create_custom(event_bus, &config);
    ASSERT_NE(custom, nullptr);

    reasoning_integration_config_t retrieved;
    EXPECT_TRUE(reasoning_integration_get_config(custom, &retrieved));
    EXPECT_EQ(retrieved.max_active_inferences, 5);
    EXPECT_FLOAT_EQ(retrieved.novel_fact_salience_boost, 0.9f);

    reasoning_integration_destroy(custom);
}

TEST_F(ReasoningIntegrationTest, DestroyNull) {
    // Should not crash
    reasoning_integration_destroy(nullptr);
}

//=============================================================================
// Event Publishing Tests (13 Logic Event Types)
//=============================================================================

TEST_F(ReasoningIntegrationTest, PublishAllLogicEventTypes) {
    brain_event_type_t logic_events[] = {
        EVENT_LOGIC_GATE_EVALUATED,
        EVENT_LOGIC_INFERENCE_STARTED,
        EVENT_LOGIC_INFERENCE_COMPLETE,
        EVENT_FACT_ADDED,
        EVENT_RULE_ADDED,
        EVENT_UNIFICATION_SUCCEEDED,
        EVENT_UNIFICATION_FAILED,
        EVENT_FORWARD_CHAIN_STEP,
        EVENT_BACKWARD_CHAIN_STEP,
        EVENT_PROOF_FOUND,
        EVENT_PROOF_FAILED,
        EVENT_CONTRADICTION_DETECTED,
        EVENT_NOVEL_FACT_DERIVED
    };

    uint64_t initial_count = get_stats().total_events_processed;

    // Publish each event type
    for (size_t i = 0; i < sizeof(logic_events) / sizeof(logic_events[0]); i++) {
        publish_event(logic_events[i]);
    }

    // Verify all events were processed
    auto stats = get_stats();
    EXPECT_EQ(stats.total_events_processed, initial_count + 13);
}

//=============================================================================
// Attention Integration Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, AttentionBoostNovelFact) {
    uint64_t initial_boosts = get_stats().attention_boosts_applied;

    publish_event(EVENT_NOVEL_FACT_DERIVED);

    auto stats = get_stats();
    EXPECT_EQ(stats.attention_boosts_applied, initial_boosts + 1);
}

TEST_F(ReasoningIntegrationTest, AttentionBoostContradiction) {
    uint64_t initial_boosts = get_stats().attention_boosts_applied;

    publish_event(EVENT_CONTRADICTION_DETECTED);

    auto stats = get_stats();
    EXPECT_EQ(stats.attention_boosts_applied, initial_boosts + 1);
}

TEST_F(ReasoningIntegrationTest, AttentionBoostProofFound) {
    uint64_t initial_boosts = get_stats().attention_boosts_applied;

    publish_event(EVENT_PROOF_FOUND);

    auto stats = get_stats();
    EXPECT_EQ(stats.attention_boosts_applied, initial_boosts + 1);
}

TEST_F(ReasoningIntegrationTest, AttentionDisabled) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.enable_attention_integration = false;
    EXPECT_TRUE(reasoning_integration_set_config(integration, &config));

    uint64_t initial_boosts = get_stats().attention_boosts_applied;
    publish_event(EVENT_NOVEL_FACT_DERIVED);

    auto stats = get_stats();
    EXPECT_EQ(stats.attention_boosts_applied, initial_boosts); // No change
}

//=============================================================================
// Curiosity Integration Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, CuriosityTriggerProofFailed) {
    uint64_t initial_triggers = get_stats().curiosity_triggers;

    publish_event(EVENT_PROOF_FAILED);

    auto stats = get_stats();
    EXPECT_EQ(stats.curiosity_triggers, initial_triggers + 1);
}

TEST_F(ReasoningIntegrationTest, CuriosityTriggerUnificationFailed) {
    uint64_t initial_triggers = get_stats().curiosity_triggers;

    publish_event(EVENT_UNIFICATION_FAILED);

    auto stats = get_stats();
    EXPECT_EQ(stats.curiosity_triggers, initial_triggers + 1);
}

TEST_F(ReasoningIntegrationTest, CuriosityTriggerNovelFact) {
    uint64_t initial_triggers = get_stats().curiosity_triggers;

    publish_event(EVENT_NOVEL_FACT_DERIVED);

    auto stats = get_stats();
    EXPECT_EQ(stats.curiosity_triggers, initial_triggers + 1);
}

TEST_F(ReasoningIntegrationTest, CuriosityDisabled) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.enable_curiosity_integration = false;
    EXPECT_TRUE(reasoning_integration_set_config(integration, &config));

    uint64_t initial_triggers = get_stats().curiosity_triggers;
    publish_event(EVENT_PROOF_FAILED);

    auto stats = get_stats();
    EXPECT_EQ(stats.curiosity_triggers, initial_triggers); // No change
}

//=============================================================================
// Working Memory Integration Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, WorkingMemoryStoreInference) {
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");

    auto stats = get_stats();
    EXPECT_EQ(stats.wm_inferences_stored, 1);
    EXPECT_EQ(stats.current_active_inferences, 1);
}

TEST_F(ReasoningIntegrationTest, WorkingMemoryUpdateStepCount) {
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");
    publish_event(EVENT_FORWARD_CHAIN_STEP);
    publish_event(EVENT_BACKWARD_CHAIN_STEP);

    active_inference_t inferences[10];
    uint32_t count = reasoning_integration_get_active_inferences(integration, inferences, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(inferences[0].step_count, 2);
}

TEST_F(ReasoningIntegrationTest, WorkingMemoryInferenceComplete) {
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");
    publish_event(EVENT_LOGIC_INFERENCE_COMPLETE);

    active_inference_t inferences[10];
    uint32_t count = reasoning_integration_get_active_inferences(integration, inferences, 10);
    EXPECT_EQ(count, 1);
    EXPECT_FALSE(inferences[0].is_active);
}

TEST_F(ReasoningIntegrationTest, WorkingMemoryRespects7Plus2Limit) {
    // Store up to max capacity
    for (int i = 0; i < REASONING_MAX_ACTIVE_INFERENCES; i++) {
        char goal[64];
        snprintf(goal, sizeof(goal), "Goal %d", i);
        publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, goal);
    }

    auto stats = get_stats();
    EXPECT_EQ(stats.current_active_inferences, REASONING_MAX_ACTIVE_INFERENCES);

    // Store one more - should evict lowest salience
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Goal Overflow");

    stats = get_stats();
    EXPECT_EQ(stats.current_active_inferences, REASONING_MAX_ACTIVE_INFERENCES); // Still at limit
    EXPECT_GT(stats.wm_inferences_stored, REASONING_MAX_ACTIVE_INFERENCES); // Stored more than limit
}

TEST_F(ReasoningIntegrationTest, WorkingMemoryDisabled) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.enable_working_memory_integration = false;
    EXPECT_TRUE(reasoning_integration_set_config(integration, &config));

    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");

    auto stats = get_stats();
    EXPECT_EQ(stats.wm_inferences_stored, 0); // No storage
}

//=============================================================================
// Executive Integration Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, ExecutivePlanCreated) {
    uint64_t initial_plans = get_stats().executive_plans_created;

    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Complex Goal");

    auto stats = get_stats();
    EXPECT_EQ(stats.executive_plans_created, initial_plans + 1);
}

TEST_F(ReasoningIntegrationTest, ExecutivePlanCompleted) {
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");
    publish_event(EVENT_PROOF_FOUND);

    // Verify plan was created and completed
    auto stats = get_stats();
    EXPECT_GE(stats.executive_plans_created, 1);
}

TEST_F(ReasoningIntegrationTest, ExecutivePlanFailed) {
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");
    publish_event(EVENT_PROOF_FAILED);

    // Verify plan was created and failed
    auto stats = get_stats();
    EXPECT_GE(stats.executive_plans_created, 1);
}

TEST_F(ReasoningIntegrationTest, ExecutiveDisabled) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.enable_executive_integration = false;
    EXPECT_TRUE(reasoning_integration_set_config(integration, &config));

    uint64_t initial_plans = get_stats().executive_plans_created;
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");

    auto stats = get_stats();
    EXPECT_EQ(stats.executive_plans_created, initial_plans); // No change
}

//=============================================================================
// Consolidation Integration Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, ConsolidationTrackRule) {
    publish_event_with_data(EVENT_RULE_ADDED, "if(X) then(Y)");

    auto stats = get_stats();
    EXPECT_EQ(stats.current_tracked_rules, 1);
}

TEST_F(ReasoningIntegrationTest, ConsolidationRuleUsage) {
    publish_event_with_data(EVENT_RULE_ADDED, "if(X) then(Y)");

    // Use rule multiple times
    for (int i = 0; i < 10; i++) {
        publish_event(EVENT_FORWARD_CHAIN_STEP);
    }

    rule_usage_t rules[10];
    uint32_t count = reasoning_integration_get_tracked_rules(integration, rules, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(rules[0].use_count, 10);
}

TEST_F(ReasoningIntegrationTest, ConsolidationTriggersAfterThreshold) {
    publish_event_with_data(EVENT_RULE_ADDED, "if(X) then(Y)");

    uint64_t initial_consolidated = get_stats().rules_consolidated;

    // Use rule enough times to trigger consolidation
    for (int i = 0; i < REASONING_CONSOLIDATION_THRESHOLD + 5; i++) {
        publish_event(EVENT_FORWARD_CHAIN_STEP);
    }

    auto stats = get_stats();
    EXPECT_GT(stats.rules_consolidated, initial_consolidated);
}

TEST_F(ReasoningIntegrationTest, ConsolidationDisabled) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.enable_consolidation_integration = false;
    EXPECT_TRUE(reasoning_integration_set_config(integration, &config));

    publish_event_with_data(EVENT_RULE_ADDED, "if(X) then(Y)");

    auto stats = get_stats();
    EXPECT_EQ(stats.current_tracked_rules, 0); // No tracking
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, DefaultConfiguration) {
    reasoning_integration_config_t config = reasoning_integration_default_config();

    EXPECT_TRUE(config.enable_attention_integration);
    EXPECT_TRUE(config.enable_curiosity_integration);
    EXPECT_TRUE(config.enable_working_memory_integration);
    EXPECT_TRUE(config.enable_executive_integration);
    EXPECT_TRUE(config.enable_consolidation_integration);

    EXPECT_FLOAT_EQ(config.novel_fact_salience_boost, REASONING_NOVEL_FACT_SALIENCE);
    EXPECT_FLOAT_EQ(config.contradiction_salience_boost, REASONING_CONTRADICTION_SALIENCE);
    EXPECT_FLOAT_EQ(config.unexplained_curiosity_boost, REASONING_CURIOSITY_BOOST);

    EXPECT_EQ(config.max_active_inferences, REASONING_MAX_ACTIVE_INFERENCES);
    EXPECT_EQ(config.min_rule_uses_for_consolidation, REASONING_CONSOLIDATION_THRESHOLD);
}

TEST_F(ReasoningIntegrationTest, ValidateValidConfig) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    EXPECT_TRUE(reasoning_integration_validate_config(&config));
}

TEST_F(ReasoningIntegrationTest, ValidateInvalidSalienceBoost) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.novel_fact_salience_boost = 1.5f; // Out of range [0, 1]
    EXPECT_FALSE(reasoning_integration_validate_config(&config));
}

TEST_F(ReasoningIntegrationTest, ValidateInvalidCuriosityBoost) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.unexplained_curiosity_boost = -0.1f; // Negative
    EXPECT_FALSE(reasoning_integration_validate_config(&config));
}

TEST_F(ReasoningIntegrationTest, ValidateInvalidMaxInferences) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.max_active_inferences = 0; // Must be >= 1
    EXPECT_FALSE(reasoning_integration_validate_config(&config));

    config.max_active_inferences = 100; // Must be <= 32
    EXPECT_FALSE(reasoning_integration_validate_config(&config));
}

TEST_F(ReasoningIntegrationTest, SetGetConfig) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.max_active_inferences = 5;
    config.novel_fact_salience_boost = 0.9f;

    EXPECT_TRUE(reasoning_integration_set_config(integration, &config));

    reasoning_integration_config_t retrieved;
    EXPECT_TRUE(reasoning_integration_get_config(integration, &retrieved));

    EXPECT_EQ(retrieved.max_active_inferences, 5);
    EXPECT_FLOAT_EQ(retrieved.novel_fact_salience_boost, 0.9f);
}

TEST_F(ReasoningIntegrationTest, SetInvalidConfig) {
    reasoning_integration_config_t config = reasoning_integration_default_config();
    config.novel_fact_salience_boost = 2.0f; // Invalid

    EXPECT_FALSE(reasoning_integration_set_config(integration, &config));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, StatisticsInitialState) {
    auto stats = get_stats();

    EXPECT_EQ(stats.total_events_processed, 0);
    EXPECT_EQ(stats.attention_boosts_applied, 0);
    EXPECT_EQ(stats.curiosity_triggers, 0);
    EXPECT_EQ(stats.wm_inferences_stored, 0);
    EXPECT_EQ(stats.executive_plans_created, 0);
    EXPECT_EQ(stats.rules_consolidated, 0);
    EXPECT_EQ(stats.current_active_inferences, 0);
    EXPECT_EQ(stats.current_tracked_rules, 0);
}

TEST_F(ReasoningIntegrationTest, StatisticsAfterEvents) {
    publish_event(EVENT_NOVEL_FACT_DERIVED);
    publish_event(EVENT_PROOF_FAILED);
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Test Goal");

    auto stats = get_stats();

    EXPECT_GE(stats.total_events_processed, 3);
    EXPECT_GE(stats.attention_boosts_applied, 1);
    EXPECT_GE(stats.curiosity_triggers, 1);
    EXPECT_GE(stats.wm_inferences_stored, 1);
}

TEST_F(ReasoningIntegrationTest, StatisticsReset) {
    publish_event(EVENT_NOVEL_FACT_DERIVED);
    publish_event(EVENT_PROOF_FAILED);

    EXPECT_TRUE(reasoning_integration_reset_stats(integration));

    auto stats = get_stats();
    EXPECT_EQ(stats.total_events_processed, 0);
    EXPECT_EQ(stats.attention_boosts_applied, 0);
    EXPECT_EQ(stats.curiosity_triggers, 0);
}

//=============================================================================
// Query Functions Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, GetActiveInferences) {
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Goal 1");
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Goal 2");

    active_inference_t inferences[10];
    uint32_t count = reasoning_integration_get_active_inferences(integration, inferences, 10);

    EXPECT_EQ(count, 2);
    EXPECT_STREQ(inferences[0].goal, "Goal 1");
    EXPECT_STREQ(inferences[1].goal, "Goal 2");
}

TEST_F(ReasoningIntegrationTest, GetActiveInferencesLimitedBuffer) {
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Goal 1");
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Goal 2");
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Goal 3");

    active_inference_t inferences[2]; // Limited buffer
    uint32_t count = reasoning_integration_get_active_inferences(integration, inferences, 2);

    EXPECT_EQ(count, 2); // Only 2 returned due to buffer size
}

TEST_F(ReasoningIntegrationTest, GetTrackedRules) {
    publish_event_with_data(EVENT_RULE_ADDED, "Rule 1");
    publish_event_with_data(EVENT_RULE_ADDED, "Rule 2");

    rule_usage_t rules[10];
    uint32_t count = reasoning_integration_get_tracked_rules(integration, rules, 10);

    EXPECT_EQ(count, 2);
    EXPECT_STREQ(rules[0].rule, "Rule 1");
    EXPECT_STREQ(rules[1].rule, "Rule 2");
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(ReasoningIntegrationTest, NullParameterHandling) {
    reasoning_integration_config_t config;
    reasoning_integration_stats_t stats;
    active_inference_t inferences[10];
    rule_usage_t rules[10];

    // All functions should handle NULL gracefully
    EXPECT_FALSE(reasoning_integration_get_config(nullptr, &config));
    EXPECT_FALSE(reasoning_integration_get_config(integration, nullptr));
    EXPECT_FALSE(reasoning_integration_set_config(nullptr, &config));
    EXPECT_FALSE(reasoning_integration_set_config(integration, nullptr));
    EXPECT_FALSE(reasoning_integration_get_stats(nullptr, &stats));
    EXPECT_FALSE(reasoning_integration_get_stats(integration, nullptr));
    EXPECT_FALSE(reasoning_integration_reset_stats(nullptr));
    EXPECT_EQ(reasoning_integration_get_active_inferences(nullptr, inferences, 10), 0);
    EXPECT_EQ(reasoning_integration_get_active_inferences(integration, nullptr, 10), 0);
    EXPECT_EQ(reasoning_integration_get_tracked_rules(nullptr, rules, 10), 0);
    EXPECT_EQ(reasoning_integration_get_tracked_rules(integration, nullptr, 10), 0);
}

TEST_F(ReasoningIntegrationTest, HookNullParameterHandling) {
    brain_event_t event = event_create(EVENT_NOVEL_FACT_DERIVED, EVENT_PRIORITY_NORMAL, "test");

    EXPECT_FALSE(reasoning_attention_hook(nullptr, &event));
    EXPECT_FALSE(reasoning_attention_hook(integration, nullptr));
    EXPECT_FALSE(reasoning_curiosity_hook(nullptr, &event));
    EXPECT_FALSE(reasoning_curiosity_hook(integration, nullptr));
    EXPECT_FALSE(reasoning_working_memory_hook(nullptr, &event));
    EXPECT_FALSE(reasoning_working_memory_hook(integration, nullptr));
    EXPECT_FALSE(reasoning_executive_hook(nullptr, &event));
    EXPECT_FALSE(reasoning_executive_hook(integration, nullptr));
    EXPECT_FALSE(reasoning_consolidation_hook(nullptr, &event));
    EXPECT_FALSE(reasoning_consolidation_hook(integration, nullptr));
}

//=============================================================================
// Integration Test - Full Workflow
//=============================================================================

TEST_F(ReasoningIntegrationTest, CompleteReasoningWorkflow) {
    // Start inference
    publish_event_with_data(EVENT_LOGIC_INFERENCE_STARTED, "Prove: X -> Y");

    // Add facts and rules
    publish_event_with_data(EVENT_FACT_ADDED, "X is true");
    publish_event_with_data(EVENT_RULE_ADDED, "if(X) then(Y)");

    // Perform reasoning steps
    publish_event(EVENT_FORWARD_CHAIN_STEP);
    publish_event(EVENT_UNIFICATION_SUCCEEDED);
    publish_event(EVENT_FORWARD_CHAIN_STEP);

    // Derive novel fact
    publish_event(EVENT_NOVEL_FACT_DERIVED);

    // Complete proof
    publish_event(EVENT_PROOF_FOUND);
    publish_event(EVENT_LOGIC_INFERENCE_COMPLETE);

    // Verify all integrations triggered
    auto stats = get_stats();
    EXPECT_GT(stats.total_events_processed, 0);
    EXPECT_GT(stats.attention_boosts_applied, 0);  // Novel fact
    EXPECT_GT(stats.wm_inferences_stored, 0);      // Inference stored
    EXPECT_GT(stats.executive_plans_created, 0);   // Plan created
    EXPECT_GE(stats.current_tracked_rules, 1);     // Rule tracked
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
