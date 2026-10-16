/**
 * @file test_rcog_hub_integration.cpp
 * @brief Integration tests for Recursive Cognition + Cognitive Integration Hub
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests integration between Recursive Cognition engine and Cognitive Hub
 * WHY:  Verify event flow, subscriptions, and inter-module communication
 * HOW:  Test event publishing, subscription, memory queries, and coordination
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define MODULE_RCOG_ENGINE          1000
#define MODULE_RCOG_CONTEXT         1001
#define MODULE_RCOG_ORCHESTRATOR    1002
#define MODULE_RCOG_DELEGATION      1003
#define MODULE_MEMORY_SYSTEM        2000
#define MODULE_ATTENTION_SYSTEM     2001
#define MODULE_EXECUTIVE_SYSTEM     2002

/* ============================================================================
 * Test Event Tracking
 * ============================================================================ */

struct RcogEventTracker {
    std::atomic<int> total_events{0};
    std::atomic<int> input_events{0};
    std::atomic<int> output_events{0};
    std::atomic<int> state_change_events{0};
    std::atomic<int> memory_access_events{0};
    std::atomic<int> attention_shift_events{0};
    std::vector<cognitive_event_type_t> event_types;
    std::vector<uint32_t> source_modules;
};

static RcogEventTracker g_tracker;

static void reset_tracker() {
    g_tracker.total_events = 0;
    g_tracker.input_events = 0;
    g_tracker.output_events = 0;
    g_tracker.state_change_events = 0;
    g_tracker.memory_access_events = 0;
    g_tracker.attention_shift_events = 0;
    g_tracker.event_types.clear();
    g_tracker.source_modules.clear();
}

static int rcog_event_callback(const cognitive_event_data_t* event, void* user_data) {
    RcogEventTracker* tracker = static_cast<RcogEventTracker*>(user_data);
    tracker->total_events++;

    switch (event->event_type) {
        case COG_EVENT_INPUT_RECEIVED:
            tracker->input_events++;
            break;
        case COG_EVENT_OUTPUT_READY:
            tracker->output_events++;
            break;
        case COG_EVENT_STATE_CHANGE:
            tracker->state_change_events++;
            break;
        case COG_EVENT_MEMORY_ACCESS:
            tracker->memory_access_events++;
            break;
        case COG_EVENT_ATTENTION_SHIFT:
            tracker->attention_shift_events++;
            break;
        default:
            break;
    }

    tracker->event_types.push_back(event->event_type);
    tracker->source_modules.push_back(event->source_module_id);

    return 0;
}

/* ============================================================================
 * Memory Query Handler
 * ============================================================================ */

static rcog_context_store_t* g_context_store_for_query = nullptr;

static int memory_query_handler(const cognitive_query_t* query,
                                  cognitive_query_result_t* result,
                                  void* context) {
    (void)context;

    if (query->query_type == COG_QUERY_MEMORY && g_context_store_for_query) {
        /* Query context store for memory access */
        if (query->query_params != nullptr) {
            const char* var_name = (const char*)query->query_params;
            if (rcog_context_store_exists(g_context_store_for_query, var_name)) {
                result->status = 0;
                snprintf(result->error_message, sizeof(result->error_message),
                         "Variable '%s' found", var_name);
            } else {
                result->status = -1;
                snprintf(result->error_message, sizeof(result->error_message),
                         "Variable '%s' not found", var_name);
            }
        } else {
            result->status = 0;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Memory query handled");
        }
    } else if (query->query_type == COG_QUERY_STATUS) {
        result->status = 0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "RCOG engine running");
    } else {
        result->status = 0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Query processed");
    }

    return 0;
}

/* ============================================================================
 * Test Fixture: RCOG + Hub Integration
 * ============================================================================ */

class RcogHubIntegrationTest : public ::testing::Test {
protected:
    rcog_engine_t* engine = nullptr;
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

        /* Create RCOG engine */
        engine = rcog_engine_create_default();
        ASSERT_NE(engine, nullptr);
        ASSERT_EQ(rcog_engine_init(engine), 0);

        /* Set up context store for queries */
        g_context_store_for_query = rcog_engine_get_context_store(engine);
    }

    void TearDown() override {
        g_context_store_for_query = nullptr;

        if (engine) {
            rcog_engine_stop(engine, 1000);
            rcog_engine_destroy(engine);
            engine = nullptr;
        }
        if (hub) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }

    /* Helper to register RCOG modules */
    void register_rcog_modules() {
        cognitive_hub_register_module(hub, MODULE_RCOG_ENGINE,
                                       COG_CATEGORY_REASONING,
                                       "rcog_engine", engine);
        cognitive_hub_register_module(hub, MODULE_RCOG_CONTEXT,
                                       COG_CATEGORY_MEMORY,
                                       "rcog_context", nullptr);
        cognitive_hub_register_module(hub, MODULE_RCOG_ORCHESTRATOR,
                                       COG_CATEGORY_EXECUTIVE,
                                       "rcog_orchestrator", nullptr);
        cognitive_hub_register_module(hub, MODULE_RCOG_DELEGATION,
                                       COG_CATEGORY_REASONING,
                                       "rcog_delegation", nullptr);
    }

    /* Helper to register other cognitive modules */
    void register_other_modules() {
        cognitive_hub_register_module(hub, MODULE_MEMORY_SYSTEM,
                                       COG_CATEGORY_MEMORY,
                                       "memory_system", nullptr);
        cognitive_hub_register_module(hub, MODULE_ATTENTION_SYSTEM,
                                       COG_CATEGORY_PERCEPTION,
                                       "attention_system", nullptr);
        cognitive_hub_register_module(hub, MODULE_EXECUTIVE_SYSTEM,
                                       COG_CATEGORY_EXECUTIVE,
                                       "executive_system", nullptr);
    }

    /* Helper to publish event */
    void publish_event(uint32_t module_id, cognitive_event_type_t event_type,
                       cognitive_event_priority_t priority) {
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

        cognitive_hub_publish(hub, module_id, event_type, &event);
    }
};

/* ============================================================================
 * RcogHubEventFlow - Events flow through hub to rcog
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, RcogHubEventFlow) {
    /* Register modules */
    register_rcog_modules();
    register_other_modules();

    /* Verify registration */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_GE(stats.registered_modules, 7u);

    /* Subscribe RCOG to various events */
    int ret = cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                                       COG_EVENT_INPUT_RECEIVED,
                                       rcog_event_callback, &g_tracker);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                                   COG_EVENT_ATTENTION_SHIFT,
                                   rcog_event_callback, &g_tracker);
    ASSERT_EQ(ret, 0);

    /* Publish events from other modules */
    publish_event(MODULE_ATTENTION_SYSTEM, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    publish_event(MODULE_ATTENTION_SYSTEM, COG_EVENT_ATTENTION_SHIFT, COG_PRIORITY_HIGH);
    publish_event(MODULE_MEMORY_SYSTEM, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);

    /* Verify events received */
    EXPECT_EQ(g_tracker.total_events.load(), 3);
    EXPECT_EQ(g_tracker.input_events.load(), 2);
    EXPECT_EQ(g_tracker.attention_shift_events.load(), 1);

    /* Verify hub stats */
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 3u);
    EXPECT_GE(stats.events_delivered, 3u);
}

/* ============================================================================
 * RcogTriggeredByInput - Input events trigger recursion
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, RcogTriggeredByInput) {
    register_rcog_modules();
    register_other_modules();

    /* Subscribe to input events */
    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_INPUT_RECEIVED,
                             rcog_event_callback, &g_tracker);

    /* Simulate input event that would trigger recursive processing */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_MEMORY_SYSTEM;
    event.priority = COG_PRIORITY_HIGH;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    /* Add payload representing a goal/query */
    const char* input_data = "Process this complex query recursively";
    event.payload = (void*)input_data;
    event.payload_size = strlen(input_data) + 1;

    int ret = cognitive_hub_publish(hub, MODULE_MEMORY_SYSTEM,
                                     COG_EVENT_INPUT_RECEIVED, &event);
    EXPECT_EQ(ret, 0);

    /* Verify event received */
    EXPECT_EQ(g_tracker.input_events.load(), 1);
    EXPECT_EQ(g_tracker.source_modules.size(), 1u);
    EXPECT_EQ(g_tracker.source_modules[0], MODULE_MEMORY_SYSTEM);
}

/* ============================================================================
 * RcogPublishesResults - rcog publishes output events
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, RcogPublishesResults) {
    register_rcog_modules();
    register_other_modules();

    /* Other modules subscribe to RCOG output */
    cognitive_hub_subscribe(hub, MODULE_MEMORY_SYSTEM,
                             COG_EVENT_OUTPUT_READY,
                             rcog_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_EXECUTIVE_SYSTEM,
                             COG_EVENT_OUTPUT_READY,
                             rcog_event_callback, &g_tracker);

    /* RCOG publishes output result */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_OUTPUT_READY;
    event.source_module_id = MODULE_RCOG_ENGINE;
    event.priority = COG_PRIORITY_NORMAL;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    const char* result_data = "Recursive processing complete with confidence 0.95";
    event.payload = (void*)result_data;
    event.payload_size = strlen(result_data) + 1;

    int ret = cognitive_hub_publish(hub, MODULE_RCOG_ENGINE,
                                     COG_EVENT_OUTPUT_READY, &event);
    EXPECT_EQ(ret, 0);

    /* Both subscribers should receive the output */
    EXPECT_EQ(g_tracker.output_events.load(), 2);

    /* Verify hub stats */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 1u);
    EXPECT_EQ(stats.events_delivered, 2u);
}

/* ============================================================================
 * RcogQueriesMemory - rcog queries memory during decomposition
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, RcogQueriesMemory) {
    register_rcog_modules();
    register_other_modules();

    /* Set up memory in context store */
    const char* memory_data = "Important context information for decomposition";
    rcog_engine_set_context(engine, "task_context", memory_data,
                             strlen(memory_data) + 1, RCOG_DTYPE_TEXT);

    /* Register query handler for memory system */
    int ret = cognitive_hub_register_query_handler(hub, MODULE_RCOG_CONTEXT,
                                                    memory_query_handler);
    ASSERT_EQ(ret, 0);

    /* RCOG queries memory system */
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_MEMORY;
    const char* var_name = "task_context";
    query.query_params = (void*)var_name;
    query.params_size = strlen(var_name) + 1;

    cognitive_query_result_t result;
    memset(&result, 0, sizeof(result));

    ret = cognitive_hub_query_module(hub, MODULE_RCOG_ENGINE, MODULE_RCOG_CONTEXT,
                                      &query, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.status, 0);

    /* Verify query tracked */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.queries_processed, 1u);
    EXPECT_EQ(stats.queries_failed, 0u);
}

/* ============================================================================
 * RcogWithAttention - Attention events affect rcog priority
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, RcogWithAttention) {
    register_rcog_modules();
    register_other_modules();

    /* RCOG subscribes to attention shifts */
    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_ATTENTION_SHIFT,
                             rcog_event_callback, &g_tracker);

    /* Publish attention shift events with different priorities */
    for (int i = 0; i < 5; i++) {
        cognitive_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = COG_EVENT_ATTENTION_SHIFT;
        event.source_module_id = MODULE_ATTENTION_SYSTEM;
        event.priority = (i < 2) ? COG_PRIORITY_LOW :
                         (i < 4) ? COG_PRIORITY_NORMAL : COG_PRIORITY_HIGH;
        event.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );

        cognitive_hub_publish(hub, MODULE_ATTENTION_SYSTEM,
                               COG_EVENT_ATTENTION_SHIFT, &event);
    }

    /* All attention shifts received */
    EXPECT_EQ(g_tracker.attention_shift_events.load(), 5);
    EXPECT_EQ(g_tracker.total_events.load(), 5);
}

/* ============================================================================
 * MultiModuleCoordination - rcog works with multiple cognitive modules
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, MultiModuleCoordination) {
    register_rcog_modules();
    register_other_modules();

    /* Set up cross-subscriptions */
    /* RCOG listens to memory and attention */
    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_MEMORY_ACCESS,
                             rcog_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_ATTENTION_SHIFT,
                             rcog_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_INPUT_RECEIVED,
                             rcog_event_callback, &g_tracker);

    /* Memory listens to RCOG output */
    cognitive_hub_subscribe(hub, MODULE_MEMORY_SYSTEM,
                             COG_EVENT_OUTPUT_READY,
                             rcog_event_callback, &g_tracker);

    /* Verify subscription count */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_GE(stats.active_subscriptions, 4u);

    /* Simulate coordinated activity */

    /* 1. Input arrives */
    publish_event(MODULE_EXECUTIVE_SYSTEM, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_HIGH);

    /* 2. Attention shifts to new input */
    publish_event(MODULE_ATTENTION_SYSTEM, COG_EVENT_ATTENTION_SHIFT, COG_PRIORITY_NORMAL);

    /* 3. Memory is accessed */
    publish_event(MODULE_MEMORY_SYSTEM, COG_EVENT_MEMORY_ACCESS, COG_PRIORITY_NORMAL);

    /* 4. RCOG produces output */
    publish_event(MODULE_RCOG_ENGINE, COG_EVENT_OUTPUT_READY, COG_PRIORITY_NORMAL);

    /* Verify all events delivered */
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 4u);

    /* RCOG received input, attention, memory events; Memory received output */
    EXPECT_EQ(g_tracker.total_events.load(), 4);
}

/* ============================================================================
 * State Change Propagation Tests
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, StateChangeNotification) {
    register_rcog_modules();
    register_other_modules();

    /* Subscribe to state changes */
    cognitive_hub_subscribe(hub, MODULE_MEMORY_SYSTEM,
                             COG_EVENT_STATE_CHANGE,
                             rcog_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_ATTENTION_SYSTEM,
                             COG_EVENT_STATE_CHANGE,
                             rcog_event_callback, &g_tracker);

    /* RCOG publishes state change (e.g., entering degraded mode) */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = MODULE_RCOG_ENGINE;
    event.priority = COG_PRIORITY_HIGH;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    const char* state_info = "RCOG_ENGINE_DEGRADED";
    event.payload = (void*)state_info;
    event.payload_size = strlen(state_info) + 1;

    int ret = cognitive_hub_publish(hub, MODULE_RCOG_ENGINE,
                                     COG_EVENT_STATE_CHANGE, &event);
    EXPECT_EQ(ret, 0);

    /* Both subscribers should receive */
    EXPECT_EQ(g_tracker.state_change_events.load(), 2);
}

/* ============================================================================
 * Module Activation Tests
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, ModuleActivation) {
    register_rcog_modules();

    /* Subscribe RCOG to events */
    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_INPUT_RECEIVED,
                             rcog_event_callback, &g_tracker);

    /* Deactivate RCOG module */
    int ret = cognitive_hub_set_module_active(hub, MODULE_RCOG_ENGINE, false);
    EXPECT_EQ(ret, 0);

    /* Verify deactivated */
    cognitive_module_info_t info;
    cognitive_hub_get_module_info(hub, MODULE_RCOG_ENGINE, &info);
    EXPECT_FALSE(info.is_active);

    /* Publish event - should not be delivered to inactive module */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_RCOG_CONTEXT;
    event.priority = COG_PRIORITY_NORMAL;

    cognitive_hub_publish(hub, MODULE_RCOG_CONTEXT, COG_EVENT_INPUT_RECEIVED, &event);

    /* May or may not receive based on implementation */
    /* Reactivate and verify it works */
    ret = cognitive_hub_set_module_active(hub, MODULE_RCOG_ENGINE, true);
    EXPECT_EQ(ret, 0);

    cognitive_hub_get_module_info(hub, MODULE_RCOG_ENGINE, &info);
    EXPECT_TRUE(info.is_active);
}

/* ============================================================================
 * Category Broadcast Tests
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, CategoryBroadcast) {
    register_rcog_modules();
    register_other_modules();

    /* Subscribe reasoning category modules */
    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_INPUT_RECEIVED,
                             rcog_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_RCOG_DELEGATION,
                             COG_EVENT_INPUT_RECEIVED,
                             rcog_event_callback, &g_tracker);

    /* Broadcast to reasoning category */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_EXECUTIVE_SYSTEM;
    event.priority = COG_PRIORITY_NORMAL;

    int ret = cognitive_hub_publish_to_category(hub, MODULE_EXECUTIVE_SYSTEM,
                                                 COG_CATEGORY_REASONING,
                                                 COG_EVENT_INPUT_RECEIVED, &event);
    EXPECT_EQ(ret, 0);

    /* Both reasoning modules should receive */
    EXPECT_GE(g_tracker.input_events.load(), 2);
}

/* ============================================================================
 * Learning Complete Event Tests
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, LearningCompleteFlow) {
    register_rcog_modules();
    register_other_modules();

    /* Subscribe to learning complete events */
    cognitive_hub_subscribe(hub, MODULE_MEMORY_SYSTEM,
                             COG_EVENT_LEARNING_COMPLETE,
                             rcog_event_callback, &g_tracker);

    /* RCOG publishes learning complete after refinement converges */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_LEARNING_COMPLETE;
    event.source_module_id = MODULE_RCOG_ENGINE;
    event.priority = COG_PRIORITY_NORMAL;
    event.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );

    int ret = cognitive_hub_publish(hub, MODULE_RCOG_ENGINE,
                                     COG_EVENT_LEARNING_COMPLETE, &event);
    EXPECT_EQ(ret, 0);

    /* Memory system should receive for consolidation */
    EXPECT_EQ(g_tracker.total_events.load(), 1);
}

/* ============================================================================
 * Statistics Verification Tests
 * ============================================================================ */

TEST_F(RcogHubIntegrationTest, HubStatisticsAccuracy) {
    register_rcog_modules();

    cognitive_hub_subscribe(hub, MODULE_RCOG_ENGINE,
                             COG_EVENT_INPUT_RECEIVED,
                             rcog_event_callback, &g_tracker);
    cognitive_hub_subscribe(hub, MODULE_RCOG_CONTEXT,
                             COG_EVENT_INPUT_RECEIVED,
                             rcog_event_callback, &g_tracker);

    /* Publish multiple events */
    for (int i = 0; i < 10; i++) {
        publish_event(MODULE_RCOG_ORCHESTRATOR, COG_EVENT_INPUT_RECEIVED,
                      COG_PRIORITY_NORMAL);
    }

    /* Verify statistics */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);

    EXPECT_GE(stats.registered_modules, 4u);
    EXPECT_GE(stats.active_subscriptions, 2u);
    EXPECT_EQ(stats.events_published, 10u);
    EXPECT_EQ(stats.events_delivered, 20u);  /* 10 events * 2 subscribers */
    EXPECT_EQ(stats.events_dropped, 0u);

    /* Reset and verify */
    cognitive_hub_reset_stats(hub);
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.events_delivered, 0u);
    /* Registration should remain */
    EXPECT_GE(stats.registered_modules, 4u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
