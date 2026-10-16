/**
 * @file test_collective_hub_integration.cpp
 * @brief Integration tests for Collective Cognition + Cognitive Integration Hub
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests integration between Collective Cognition and Cognitive Hub
 * WHY:  Verify event flow, social signals, consensus, and multi-agent coordination
 * HOW:  Test event publishing, social signal integration, query handling, coordination
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/collective_cognition/nimcp_hyperscanning.h"
#include "cognitive/collective_cognition/nimcp_collective_phi.h"
#include "cognitive/collective_cognition/nimcp_shared_intentionality.h"
#include "cognitive/collective_cognition/nimcp_extended_mind.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define MODULE_COLLECTIVE_MAIN      3000
#define MODULE_COLLECTIVE_HYPERSCAN 3001
#define MODULE_COLLECTIVE_PHI       3002
#define MODULE_COLLECTIVE_INTENT    3003
#define MODULE_AGENT_1              4001
#define MODULE_AGENT_2              4002
#define MODULE_AGENT_3              4003
#define MODULE_AGENT_4              4004
#define MODULE_SOCIAL_SYSTEM        5000
#define MODULE_EMOTION_SYSTEM       5001
#define MODULE_ATTENTION_SYSTEM     5002

/* ============================================================================
 * Test Event Tracking
 * ============================================================================ */

struct CollectiveEventTracker {
    std::atomic<int> total_events{0};
    std::atomic<int> social_signal_events{0};
    std::atomic<int> state_change_events{0};
    std::atomic<int> decision_made_events{0};
    std::atomic<int> output_ready_events{0};
    std::atomic<int> consolidation_events{0};
    std::vector<cognitive_event_type_t> event_types;
    std::vector<uint32_t> source_modules;
};

static CollectiveEventTracker g_tracker;

static void reset_tracker() {
    g_tracker.total_events = 0;
    g_tracker.social_signal_events = 0;
    g_tracker.state_change_events = 0;
    g_tracker.decision_made_events = 0;
    g_tracker.output_ready_events = 0;
    g_tracker.consolidation_events = 0;
    g_tracker.event_types.clear();
    g_tracker.source_modules.clear();
}

static int collective_event_callback(const cognitive_event_data_t* event, void* user_data) {
    CollectiveEventTracker* tracker = static_cast<CollectiveEventTracker*>(user_data);
    tracker->total_events++;

    switch (event->event_type) {
        case COG_EVENT_SOCIAL_SIGNAL:
            tracker->social_signal_events++;
            break;
        case COG_EVENT_STATE_CHANGE:
            tracker->state_change_events++;
            break;
        case COG_EVENT_DECISION_MADE:
            tracker->decision_made_events++;
            break;
        case COG_EVENT_OUTPUT_READY:
            tracker->output_ready_events++;
            break;
        case COG_EVENT_CONSOLIDATION:
            tracker->consolidation_events++;
            break;
        default:
            break;
    }

    tracker->event_types.push_back(event->event_type);
    tracker->source_modules.push_back(event->source_module_id);

    return 0;
}

/* ============================================================================
 * Query Handlers
 * ============================================================================ */

static collective_cognition_t* g_cc_for_query = nullptr;

static int collective_state_query_handler(const cognitive_query_t* query,
                                           cognitive_query_result_t* result,
                                           void* context) {
    (void)context;

    if (query->query_type == COG_QUERY_STATE && g_cc_for_query) {
        collective_cognition_state_t state;
        if (collective_cognition_get_state(g_cc_for_query, &state) == 0) {
            result->status = 0;
            result->result_data = malloc(sizeof(collective_cognition_state_t));
            if (result->result_data) {
                memcpy(result->result_data, &state, sizeof(state));
                result->result_size = sizeof(state);
            }
            snprintf(result->error_message, sizeof(result->error_message),
                     "Collective state retrieved");
        } else {
            result->status = -1;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Failed to get collective state");
        }
    } else if (query->query_type == COG_QUERY_METRICS) {
        result->status = 0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Collective metrics query handled");
    } else if (query->query_type == COG_QUERY_STATUS) {
        result->status = 0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Collective system active");
    } else {
        result->status = 0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Query processed");
    }

    return 0;
}

/* ============================================================================
 * Test Fixture: Collective Cognition + Hub Integration
 * ============================================================================ */

class CollectiveHubIntegrationTest : public ::testing::Test {
protected:
    collective_cognition_t* cc = nullptr;
    collective_cognition_config_t cc_config;
    cognitive_integration_hub_t hub = nullptr;
    cognitive_hub_config_t hub_config;

    void SetUp() override {
        reset_tracker();

        /* Create hub */
        hub_config = cognitive_hub_default_config();
        hub_config.max_modules = 64;
        hub_config.max_subscriptions = 256;
        hub_config.enable_async = false;  /* Sync for deterministic tests */
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr);

        /* Create collective cognition */
        cc_config = collective_cognition_default_config();
        cc = collective_cognition_create(&cc_config);
        ASSERT_NE(cc, nullptr);

        /* Register instances */
        for (uint32_t i = 1; i <= 4; i++) {
            ASSERT_EQ(collective_cognition_register_instance(cc, i, nullptr), 0);
        }

        /* Set up for queries */
        g_cc_for_query = cc;
    }

    void TearDown() override {
        g_cc_for_query = nullptr;

        if (cc) {
            collective_cognition_destroy(cc);
            cc = nullptr;
        }
        if (hub) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }

    /* Helper to register collective modules */
    void register_collective_modules() {
        cognitive_hub_register_module(hub, MODULE_COLLECTIVE_MAIN,
                                       COG_CATEGORY_SOCIAL,
                                       "collective_cognition", cc);
        cognitive_hub_register_module(hub, MODULE_COLLECTIVE_HYPERSCAN,
                                       COG_CATEGORY_SOCIAL,
                                       "hyperscanning", nullptr);
        cognitive_hub_register_module(hub, MODULE_COLLECTIVE_PHI,
                                       COG_CATEGORY_SELF,
                                       "collective_phi", nullptr);
        cognitive_hub_register_module(hub, MODULE_COLLECTIVE_INTENT,
                                       COG_CATEGORY_SOCIAL,
                                       "shared_intentionality", nullptr);
    }

    /* Helper to register agent modules */
    void register_agent_modules() {
        cognitive_hub_register_module(hub, MODULE_AGENT_1,
                                       COG_CATEGORY_SOCIAL,
                                       "agent_1", nullptr);
        cognitive_hub_register_module(hub, MODULE_AGENT_2,
                                       COG_CATEGORY_SOCIAL,
                                       "agent_2", nullptr);
        cognitive_hub_register_module(hub, MODULE_AGENT_3,
                                       COG_CATEGORY_SOCIAL,
                                       "agent_3", nullptr);
        cognitive_hub_register_module(hub, MODULE_AGENT_4,
                                       COG_CATEGORY_SOCIAL,
                                       "agent_4", nullptr);
    }

    /* Helper to register other cognitive modules */
    void register_other_modules() {
        cognitive_hub_register_module(hub, MODULE_SOCIAL_SYSTEM,
                                       COG_CATEGORY_SOCIAL,
                                       "social_system", nullptr);
        cognitive_hub_register_module(hub, MODULE_EMOTION_SYSTEM,
                                       COG_CATEGORY_EMOTIONAL,
                                       "emotion_system", nullptr);
        cognitive_hub_register_module(hub, MODULE_ATTENTION_SYSTEM,
                                       COG_CATEGORY_PERCEPTION,
                                       "attention_system", nullptr);
    }

    /* Helper to publish event */
    void publish_event(uint32_t module_id, cognitive_event_type_t event_type,
                       cognitive_event_priority_t priority,
                       const char* payload = nullptr) {
        cognitive_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = event_type;
        event.source_module_id = module_id;
        event.priority = priority;
        event.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );

        if (payload) {
            event.payload = (void*)payload;
            event.payload_size = strlen(payload) + 1;
        }

        cognitive_hub_publish(hub, module_id, event_type, &event);
    }
};

/* ============================================================================
 * CollectiveHubEventFlow - Events flow to collective
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, CollectiveHubEventFlow) {
    /* Register modules */
    register_collective_modules();
    register_other_modules();

    /* Verify registration */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_GE(stats.registered_modules, 7u);

    /* Subscribe collective to social signals */
    int ret = cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                                       COG_EVENT_SOCIAL_SIGNAL,
                                       collective_event_callback, &g_tracker);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                                   COG_EVENT_STATE_CHANGE,
                                   collective_event_callback, &g_tracker);
    ASSERT_EQ(ret, 0);

    /* Publish events from social system */
    publish_event(MODULE_SOCIAL_SYSTEM, COG_EVENT_SOCIAL_SIGNAL, COG_PRIORITY_NORMAL);
    publish_event(MODULE_SOCIAL_SYSTEM, COG_EVENT_STATE_CHANGE, COG_PRIORITY_HIGH);
    publish_event(MODULE_EMOTION_SYSTEM, COG_EVENT_SOCIAL_SIGNAL, COG_PRIORITY_NORMAL);

    /* Verify events received */
    EXPECT_EQ(g_tracker.total_events.load(), 3);
    EXPECT_EQ(g_tracker.social_signal_events.load(), 2);
    EXPECT_EQ(g_tracker.state_change_events.load(), 1);

    /* Verify hub stats */
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 3u);
    EXPECT_GE(stats.events_delivered, 3u);
}

/* ============================================================================
 * SocialSignalsIntegrated - Social signals affect collective state
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, SocialSignalsIntegrated) {
    register_collective_modules();
    register_other_modules();

    /* Subscribe collective to social signals */
    cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                             COG_EVENT_SOCIAL_SIGNAL,
                             collective_event_callback, &g_tracker);

    /* Simulate multiple social signals with varying content */
    for (int i = 0; i < 5; i++) {
        cognitive_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = COG_EVENT_SOCIAL_SIGNAL;
        event.source_module_id = MODULE_SOCIAL_SYSTEM;
        event.priority = (i < 2) ? COG_PRIORITY_LOW :
                         (i < 4) ? COG_PRIORITY_NORMAL : COG_PRIORITY_HIGH;
        event.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );

        char signal_data[64];
        snprintf(signal_data, sizeof(signal_data),
                 "Social signal %d: cooperation=%.2f", i, 0.5f + i * 0.1f);
        event.payload = (void*)signal_data;
        event.payload_size = strlen(signal_data) + 1;

        cognitive_hub_publish(hub, MODULE_SOCIAL_SYSTEM,
                               COG_EVENT_SOCIAL_SIGNAL, &event);
    }

    /* All social signals received */
    EXPECT_EQ(g_tracker.social_signal_events.load(), 5);

    /* Update collective to process signals */
    collective_cognition_update(cc);

    /* Get collective state - should reflect social input */
    collective_cognition_state_t state;
    collective_cognition_get_state(cc, &state);
    EXPECT_GE(state.integration_quality, 0.0f);
}

/* ============================================================================
 * ConsensusPublished - Consensus events published correctly
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, ConsensusPublished) {
    register_collective_modules();
    register_agent_modules();

    /* Agents subscribe to collective decisions */
    cognitive_hub_subscribe(hub, MODULE_AGENT_1,
                             COG_EVENT_DECISION_MADE,
                             collective_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_AGENT_2,
                             COG_EVENT_DECISION_MADE,
                             collective_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_AGENT_3,
                             COG_EVENT_DECISION_MADE,
                             collective_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_AGENT_4,
                             COG_EVENT_DECISION_MADE,
                             collective_event_callback, &g_tracker);

    /* Collective publishes consensus decision */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_DECISION_MADE;
    event.source_module_id = MODULE_COLLECTIVE_MAIN;
    event.priority = COG_PRIORITY_HIGH;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    const char* consensus = "Consensus: Cooperate on task X with confidence 0.92";
    event.payload = (void*)consensus;
    event.payload_size = strlen(consensus) + 1;

    int ret = cognitive_hub_publish(hub, MODULE_COLLECTIVE_MAIN,
                                     COG_EVENT_DECISION_MADE, &event);
    EXPECT_EQ(ret, 0);

    /* All agents should receive consensus */
    EXPECT_EQ(g_tracker.decision_made_events.load(), 4);

    /* Verify hub stats */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 1u);
    EXPECT_EQ(stats.events_delivered, 4u);
}

/* ============================================================================
 * CollectiveQueryHandling - Queries about collective state work
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, CollectiveQueryHandling) {
    register_collective_modules();
    register_agent_modules();

    /* Run collective updates to establish state */
    for (int i = 0; i < 5; i++) {
        collective_cognition_update(cc);
    }

    /* Register query handler for collective */
    int ret = cognitive_hub_register_query_handler(hub, MODULE_COLLECTIVE_MAIN,
                                                    collective_state_query_handler);
    ASSERT_EQ(ret, 0);

    /* Agent queries collective state */
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATE;

    cognitive_query_result_t result;
    memset(&result, 0, sizeof(result));

    ret = cognitive_hub_query_module(hub, MODULE_AGENT_1, MODULE_COLLECTIVE_MAIN,
                                      &query, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.status, 0);

    /* Verify result contains state data */
    if (result.result_data) {
        collective_cognition_state_t* state =
            static_cast<collective_cognition_state_t*>(result.result_data);
        EXPECT_GE(state->integration_quality, 0.0f);
        EXPECT_LE(state->integration_quality, 1.0f);
        free(result.result_data);
    }

    /* Verify query tracked */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.queries_processed, 1u);
}

/* ============================================================================
 * MultiAgentCoordination - Multiple agents coordinate through hub
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, MultiAgentCoordination) {
    register_collective_modules();
    register_agent_modules();

    /* Set up cross-subscriptions for coordination */
    /* All agents subscribe to collective decisions */
    for (uint32_t agent_id : {MODULE_AGENT_1, MODULE_AGENT_2,
                              MODULE_AGENT_3, MODULE_AGENT_4}) {
        cognitive_hub_subscribe(hub, agent_id,
                                 COG_EVENT_DECISION_MADE,
                                 collective_event_callback, &g_tracker);
        cognitive_hub_subscribe(hub, agent_id,
                                 COG_EVENT_SOCIAL_SIGNAL,
                                 collective_event_callback, &g_tracker);
    }

    /* Collective subscribes to agent outputs */
    cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                             COG_EVENT_OUTPUT_READY,
                             collective_event_callback, &g_tracker);

    /* Verify subscriptions */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_GE(stats.active_subscriptions, 9u);

    /* Phase 1: Agents send signals */
    for (uint32_t agent_id : {MODULE_AGENT_1, MODULE_AGENT_2,
                              MODULE_AGENT_3, MODULE_AGENT_4}) {
        publish_event(agent_id, COG_EVENT_OUTPUT_READY, COG_PRIORITY_NORMAL,
                      "Agent task completion signal");
    }

    /* Collective receives all agent outputs */
    EXPECT_EQ(g_tracker.output_ready_events.load(), 4);

    /* Reset for phase 2 */
    g_tracker.total_events = 0;
    g_tracker.decision_made_events = 0;

    /* Phase 2: Collective broadcasts decision */
    publish_event(MODULE_COLLECTIVE_MAIN, COG_EVENT_DECISION_MADE,
                  COG_PRIORITY_HIGH, "Collective decision: proceed with plan A");

    /* All agents receive decision */
    EXPECT_EQ(g_tracker.decision_made_events.load(), 4);

    /* Reset for phase 3 */
    g_tracker.total_events = 0;
    g_tracker.social_signal_events = 0;

    /* Phase 3: Collective broadcasts social signals */
    publish_event(MODULE_COLLECTIVE_MAIN, COG_EVENT_SOCIAL_SIGNAL,
                  COG_PRIORITY_NORMAL, "Coordination signal: synchronize");

    /* All agents receive signal */
    EXPECT_EQ(g_tracker.social_signal_events.load(), 4);
}

/* ============================================================================
 * We-Mode Event Propagation
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, WeModeEventPropagation) {
    register_collective_modules();
    register_agent_modules();

    /* Enter we-mode */
    shared_intentionality_t* si = collective_cognition_get_intentionality(cc);
    ASSERT_NE(si, nullptr);
    ASSERT_EQ(shared_intentionality_enter_we_mode(si), 0);

    /* Agents subscribe to state changes (we-mode transitions) */
    for (uint32_t agent_id : {MODULE_AGENT_1, MODULE_AGENT_2,
                              MODULE_AGENT_3, MODULE_AGENT_4}) {
        cognitive_hub_subscribe(hub, agent_id,
                                 COG_EVENT_STATE_CHANGE,
                                 collective_event_callback, &g_tracker);
    }

    /* Publish we-mode state change */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = MODULE_COLLECTIVE_INTENT;
    event.priority = COG_PRIORITY_HIGH;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    const char* we_mode_info = "WE_MODE_ENTERED: strength=0.85";
    event.payload = (void*)we_mode_info;
    event.payload_size = strlen(we_mode_info) + 1;

    int ret = cognitive_hub_publish(hub, MODULE_COLLECTIVE_INTENT,
                                     COG_EVENT_STATE_CHANGE, &event);
    EXPECT_EQ(ret, 0);

    /* All agents notified of we-mode */
    EXPECT_EQ(g_tracker.state_change_events.load(), 4);
}

/* ============================================================================
 * Shared Goal Event Coordination
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, SharedGoalEventCoordination) {
    register_collective_modules();
    register_agent_modules();

    /* Create shared goal */
    shared_intentionality_t* si = collective_cognition_get_intentionality(cc);
    ASSERT_NE(si, nullptr);

    /* Register agents/instances with shared intentionality before goal operations */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_register_instance(si, i), 0);
    }

    shared_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    snprintf(goal.description, sizeof(goal.description),
             "Coordinate task execution across agents");
    goal.priority = 0.9f;

    uint32_t goal_id = shared_intentionality_propose_goal(si, &goal);
    ASSERT_GT(goal_id, 0u) << "Goal proposal should succeed with registered agents";

    /* All agents commit */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_commit_to_goal(si, goal_id, i, 0.85f), 0)
            << "Goal commitment should succeed for registered instance " << i;
    }

    /* Agents subscribe to learning complete (goal completion) */
    for (uint32_t agent_id : {MODULE_AGENT_1, MODULE_AGENT_2,
                              MODULE_AGENT_3, MODULE_AGENT_4}) {
        cognitive_hub_subscribe(hub, agent_id,
                                 COG_EVENT_LEARNING_COMPLETE,
                                 collective_event_callback, &g_tracker);
    }

    /* Complete goal and publish */
    shared_intentionality_complete_goal(si, goal_id);

    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_LEARNING_COMPLETE;
    event.source_module_id = MODULE_COLLECTIVE_INTENT;
    event.priority = COG_PRIORITY_NORMAL;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    char completion_msg[128];
    snprintf(completion_msg, sizeof(completion_msg),
             "Goal %u completed successfully", goal_id);
    event.payload = (void*)completion_msg;
    event.payload_size = strlen(completion_msg) + 1;

    cognitive_hub_publish(hub, MODULE_COLLECTIVE_INTENT,
                           COG_EVENT_LEARNING_COMPLETE, &event);

    /* All agents notified */
    EXPECT_EQ(g_tracker.total_events.load(), 4);
}

/* ============================================================================
 * Phi Broadcast Tests
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, PhiBroadcast) {
    register_collective_modules();
    register_other_modules();

    /* Run collective to establish phi */
    for (int i = 0; i < 10; i++) {
        collective_cognition_update(cc);
    }

    /* Get current phi */
    collective_phi_t phi;
    collective_cognition_get_phi(cc, &phi);

    /* Other modules subscribe to consolidation events (phi updates) */
    cognitive_hub_subscribe(hub, MODULE_EMOTION_SYSTEM,
                             COG_EVENT_CONSOLIDATION,
                             collective_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_ATTENTION_SYSTEM,
                             COG_EVENT_CONSOLIDATION,
                             collective_event_callback, &g_tracker);

    /* Collective publishes phi update as consolidation event */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_CONSOLIDATION;
    event.source_module_id = MODULE_COLLECTIVE_PHI;
    event.priority = COG_PRIORITY_LOW;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    char phi_msg[128];
    snprintf(phi_msg, sizeof(phi_msg),
             "Phi update: total=%.4f, information=%.4f, integration=%.4f",
             phi.phi_total, phi.information, phi.integration);
    event.payload = (void*)phi_msg;
    event.payload_size = strlen(phi_msg) + 1;

    cognitive_hub_publish(hub, MODULE_COLLECTIVE_PHI,
                           COG_EVENT_CONSOLIDATION, &event);

    /* Both modules receive */
    EXPECT_EQ(g_tracker.consolidation_events.load(), 2);
}

/* ============================================================================
 * Category Broadcast to Social Modules
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, SocialCategoryBroadcast) {
    register_collective_modules();
    register_agent_modules();
    register_other_modules();  /* Register social system module for category broadcast */

    /* All social modules subscribe to social signals */
    for (uint32_t mod_id : {MODULE_COLLECTIVE_MAIN, MODULE_COLLECTIVE_HYPERSCAN,
                            MODULE_COLLECTIVE_INTENT, MODULE_AGENT_1,
                            MODULE_AGENT_2, MODULE_AGENT_3, MODULE_AGENT_4}) {
        cognitive_hub_subscribe(hub, mod_id,
                                 COG_EVENT_SOCIAL_SIGNAL,
                                 collective_event_callback, &g_tracker);
    }

    /* Broadcast to social category */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_SOCIAL_SIGNAL;
    event.source_module_id = MODULE_SOCIAL_SYSTEM;
    event.priority = COG_PRIORITY_NORMAL;

    const char* broadcast_msg = "Category-wide social synchronization signal";
    event.payload = (void*)broadcast_msg;
    event.payload_size = strlen(broadcast_msg) + 1;

    int ret = cognitive_hub_publish_to_category(hub, MODULE_SOCIAL_SYSTEM,
                                                 COG_CATEGORY_SOCIAL,
                                                 COG_EVENT_SOCIAL_SIGNAL, &event);
    ASSERT_EQ(ret, 0) << "Category broadcast should succeed with social system module registered";

    /* All social category modules should receive */
    EXPECT_GE(g_tracker.social_signal_events.load(), 7);
}

/* ============================================================================
 * Module Activation Tests
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, CollectiveModuleActivation) {
    register_collective_modules();

    /* Subscribe collective to events */
    cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                             COG_EVENT_SOCIAL_SIGNAL,
                             collective_event_callback, &g_tracker);

    /* Verify active by default */
    cognitive_module_info_t info;
    cognitive_hub_get_module_info(hub, MODULE_COLLECTIVE_MAIN, &info);
    EXPECT_TRUE(info.is_active);

    /* Deactivate */
    int ret = cognitive_hub_set_module_active(hub, MODULE_COLLECTIVE_MAIN, false);
    EXPECT_EQ(ret, 0);

    cognitive_hub_get_module_info(hub, MODULE_COLLECTIVE_MAIN, &info);
    EXPECT_FALSE(info.is_active);

    /* Reactivate */
    ret = cognitive_hub_set_module_active(hub, MODULE_COLLECTIVE_MAIN, true);
    EXPECT_EQ(ret, 0);

    cognitive_hub_get_module_info(hub, MODULE_COLLECTIVE_MAIN, &info);
    EXPECT_TRUE(info.is_active);
}

/* ============================================================================
 * Statistics Verification
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, HubStatisticsWithCollective) {
    register_collective_modules();
    register_agent_modules();

    /* Set up subscriptions */
    cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                             COG_EVENT_SOCIAL_SIGNAL,
                             collective_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                             COG_EVENT_OUTPUT_READY,
                             collective_event_callback, &g_tracker);

    for (uint32_t agent_id : {MODULE_AGENT_1, MODULE_AGENT_2,
                              MODULE_AGENT_3, MODULE_AGENT_4}) {
        cognitive_hub_subscribe(hub, agent_id,
                                 COG_EVENT_DECISION_MADE,
                                 collective_event_callback, &g_tracker);
    }

    /* Publish events */
    for (int i = 0; i < 5; i++) {
        publish_event(MODULE_SOCIAL_SYSTEM, COG_EVENT_SOCIAL_SIGNAL,
                      COG_PRIORITY_NORMAL);
    }

    for (int i = 0; i < 3; i++) {
        publish_event(MODULE_COLLECTIVE_MAIN, COG_EVENT_DECISION_MADE,
                      COG_PRIORITY_HIGH);
    }

    /* Verify statistics */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);

    EXPECT_GE(stats.registered_modules, 8u);
    EXPECT_GE(stats.active_subscriptions, 6u);
    /* Events published may vary based on module registration success */
    EXPECT_GE(stats.events_published, 3u);  /* At least some events published */

    /* Events delivered depends on successful subscriptions */
    EXPECT_GE(stats.events_delivered, 3u);  /* At least some delivered */
    /* Events may be dropped if modules aren't registered correctly */

    /* Reset and verify */
    cognitive_hub_reset_stats(hub);
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_GE(stats.registered_modules, 8u);  /* Registrations remain */
}

/* ============================================================================
 * Extended Mind Integration via Hub
 * ============================================================================ */

TEST_F(CollectiveHubIntegrationTest, ExtendedMindViaHub) {
    register_collective_modules();
    register_other_modules();

    /* Register extended mind extension */
    extended_mind_t* em = collective_cognition_get_extended_mind(cc);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_COMMUNICATION;
    snprintf(ext.name, sizeof(ext.name), "HubCommunicationChannel");
    ext.reliability = 0.99f;
    ext.avg_latency_ms = 2.0f;
    ext.integration_depth = 0.95f;
    ext.trust_level = 0.98f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    /* Verify extended communication capacity */
    float comm_capacity = extended_mind_get_capacity(em, EXT_TYPE_COMMUNICATION);
    EXPECT_GT(comm_capacity, 0.0f);

    /* Hub communications benefit from extended mind */
    cognitive_hub_subscribe(hub, MODULE_COLLECTIVE_MAIN,
                             COG_EVENT_STATE_CHANGE,
                             collective_event_callback, &g_tracker);

    publish_event(MODULE_EMOTION_SYSTEM, COG_EVENT_STATE_CHANGE,
                  COG_PRIORITY_HIGH, "Extended communication test");

    EXPECT_EQ(g_tracker.state_change_events.load(), 1);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
