/**
 * @file test_hub_module_integration.cpp
 * @brief Integration tests for cognitive hub with multiple modules
 * @version 1.0.0
 * @date 2025-01-08
 *
 * WHAT: Integration tests for cognitive hub multi-module scenarios
 * WHY:  Verify hub correctly manages multiple modules, cross-category events,
 *       cascading events, and module query chains
 * HOW:  Test module registration, event routing, and query forwarding
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define MODULE_PERCEPTION_1     1001
#define MODULE_PERCEPTION_2     1002
#define MODULE_MEMORY_1         2001
#define MODULE_REASONING_1      3001
#define MODULE_EXECUTIVE_1      4001
#define MODULE_EMOTIONAL_1      5001

/* ============================================================================
 * Test Context Structure
 * ============================================================================ */

struct TestCallbackContext {
    std::atomic<int> events_received{0};
    cognitive_event_type_t last_event_type{COG_EVENT_STATE_CHANGE};
    uint32_t last_source_module{0};
    cognitive_integration_hub_t hub{nullptr};  /* For cascading events */
    uint32_t publisher_id{0};                  /* For cascading events */
    bool should_cascade{false};
};

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

static int test_event_callback(const cognitive_event_data_t* event, void* user_data) {
    TestCallbackContext* ctx = static_cast<TestCallbackContext*>(user_data);
    ctx->events_received++;
    ctx->last_event_type = event->event_type;
    ctx->last_source_module = event->source_module_id;
    return 0;
}

static int cascading_event_callback(const cognitive_event_data_t* event, void* user_data) {
    TestCallbackContext* ctx = static_cast<TestCallbackContext*>(user_data);
    ctx->events_received++;
    ctx->last_event_type = event->event_type;
    ctx->last_source_module = event->source_module_id;

    /* If cascading enabled, publish a new event */
    if (ctx->should_cascade && ctx->hub && ctx->publisher_id != 0) {
        cognitive_event_data_t cascade_event;
        memset(&cascade_event, 0, sizeof(cascade_event));
        cascade_event.event_type = COG_EVENT_OUTPUT_READY;
        cascade_event.source_module_id = ctx->publisher_id;
        cascade_event.priority = COG_PRIORITY_NORMAL;

        /* Only cascade once to avoid infinite loop */
        ctx->should_cascade = false;
        cognitive_hub_publish(ctx->hub, ctx->publisher_id, COG_EVENT_OUTPUT_READY, &cascade_event);
    }

    return 0;
}

/* ============================================================================
 * Query Handler for Chained Queries
 * ============================================================================ */

struct QueryChainContext {
    cognitive_integration_hub_t hub{nullptr};
    uint32_t my_module_id{0};
    uint32_t next_module_id{0};
    int query_count{0};
};

static int chained_query_handler(const cognitive_query_t* query,
                                  cognitive_query_result_t* result,
                                  void* context) {
    QueryChainContext* ctx = static_cast<QueryChainContext*>(context);
    ctx->query_count++;

    /* If there's a next module, forward the query */
    if (ctx->next_module_id != 0 && ctx->hub != nullptr) {
        cognitive_query_result_t forward_result;
        memset(&forward_result, 0, sizeof(forward_result));

        int ret = cognitive_hub_query_module(ctx->hub, ctx->my_module_id,
                                              ctx->next_module_id, query, &forward_result);
        if (ret == 0) {
            result->status = 0;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Forwarded to module %u", ctx->next_module_id);
        } else {
            result->status = 0;
            snprintf(result->error_message, sizeof(result->error_message),
                     "End of chain at module %u", ctx->my_module_id);
        }
    } else {
        result->status = 0;
        snprintf(result->error_message, sizeof(result->error_message),
                 "End of chain at module %u", ctx->my_module_id);
    }

    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HubModuleIntegrationTest : public ::testing::Test {
protected:
    cognitive_integration_hub_t hub;
    cognitive_hub_config_t config;

    void SetUp() override {
        hub = nullptr;

        /* Get default config */
        config = cognitive_hub_default_config();
        config.max_modules = 32;
        config.max_subscriptions = 128;
        config.enable_async = false;  /* Synchronous for deterministic tests */

        /* Create hub */
        hub = cognitive_hub_create(&config);
        ASSERT_NE(hub, nullptr);
    }

    void TearDown() override {
        if (hub) {
            cognitive_hub_destroy(hub);
        }
    }
};

/* ============================================================================
 * Multi-Module Registration Tests
 * ============================================================================ */

TEST_F(HubModuleIntegrationTest, MultiModuleRegistration) {
    /* Register 5+ modules from different categories */
    int ret;

    /* Register perception modules */
    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "visual_processor", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_2,
                                         COG_CATEGORY_PERCEPTION,
                                         "auditory_processor", nullptr);
    ASSERT_EQ(ret, 0);

    /* Register memory module */
    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "working_memory", nullptr);
    ASSERT_EQ(ret, 0);

    /* Register reasoning module */
    ret = cognitive_hub_register_module(hub, MODULE_REASONING_1,
                                         COG_CATEGORY_REASONING,
                                         "logical_reasoner", nullptr);
    ASSERT_EQ(ret, 0);

    /* Register executive module */
    ret = cognitive_hub_register_module(hub, MODULE_EXECUTIVE_1,
                                         COG_CATEGORY_EXECUTIVE,
                                         "executive_control", nullptr);
    ASSERT_EQ(ret, 0);

    /* Register emotional module */
    ret = cognitive_hub_register_module(hub, MODULE_EMOTIONAL_1,
                                         COG_CATEGORY_EMOTIONAL,
                                         "emotion_processor", nullptr);
    ASSERT_EQ(ret, 0);

    /* Verify all modules registered by checking stats */
    cognitive_hub_stats_t stats;
    ret = cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.registered_modules, 6u);

    /* Verify module info can be retrieved */
    cognitive_module_info_t info;
    ret = cognitive_hub_get_module_info(hub, MODULE_PERCEPTION_1, &info);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(info.module_id, MODULE_PERCEPTION_1);
    EXPECT_EQ(info.category, COG_CATEGORY_PERCEPTION);
    EXPECT_STREQ(info.name, "visual_processor");
    EXPECT_TRUE(info.is_active);

    ret = cognitive_hub_get_module_info(hub, MODULE_EMOTIONAL_1, &info);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(info.category, COG_CATEGORY_EMOTIONAL);
}

TEST_F(HubModuleIntegrationTest, DuplicateModuleRegistrationFails) {
    int ret;

    /* Register first module */
    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "visual_processor", nullptr);
    ASSERT_EQ(ret, 0);

    /* Try to register same ID again - should fail */
    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_MEMORY,
                                         "duplicate_module", nullptr);
    EXPECT_EQ(ret, -1);

    /* Verify only one module registered */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.registered_modules, 1u);
}

/* ============================================================================
 * Cross-Category Event Tests
 * ============================================================================ */

TEST_F(HubModuleIntegrationTest, CrossCategoryEvents) {
    /* Register modules from different categories */
    int ret;

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "visual_processor", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "working_memory", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_EMOTIONAL_1,
                                         COG_CATEGORY_EMOTIONAL,
                                         "emotion_processor", nullptr);
    ASSERT_EQ(ret, 0);

    /* Set up callback contexts */
    TestCallbackContext memory_ctx;
    TestCallbackContext emotion_ctx;

    /* Memory and emotion modules subscribe to perception events */
    ret = cognitive_hub_subscribe(hub, MODULE_MEMORY_1, COG_EVENT_INPUT_RECEIVED,
                                   test_event_callback, &memory_ctx);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_subscribe(hub, MODULE_EMOTIONAL_1, COG_EVENT_INPUT_RECEIVED,
                                   test_event_callback, &emotion_ctx);
    ASSERT_EQ(ret, 0);

    /* Perception module publishes event */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PERCEPTION_1;
    event.priority = COG_PRIORITY_NORMAL;

    ret = cognitive_hub_publish(hub, MODULE_PERCEPTION_1, COG_EVENT_INPUT_RECEIVED, &event);
    EXPECT_EQ(ret, 0);

    /* Verify both subscribers received the event */
    EXPECT_EQ(memory_ctx.events_received.load(), 1);
    EXPECT_EQ(memory_ctx.last_event_type, COG_EVENT_INPUT_RECEIVED);
    EXPECT_EQ(memory_ctx.last_source_module, MODULE_PERCEPTION_1);

    EXPECT_EQ(emotion_ctx.events_received.load(), 1);
    EXPECT_EQ(emotion_ctx.last_event_type, COG_EVENT_INPUT_RECEIVED);
    EXPECT_EQ(emotion_ctx.last_source_module, MODULE_PERCEPTION_1);

    /* Verify stats */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 1u);
    EXPECT_EQ(stats.events_delivered, 2u);
}

TEST_F(HubModuleIntegrationTest, CategoryBroadcast) {
    /* Register multiple modules in same category */
    int ret;

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "visual_processor", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_2,
                                         COG_CATEGORY_PERCEPTION,
                                         "auditory_processor", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_EXECUTIVE_1,
                                         COG_CATEGORY_EXECUTIVE,
                                         "executive_control", nullptr);
    ASSERT_EQ(ret, 0);

    /* Subscribe both perception modules to state change events */
    TestCallbackContext perception1_ctx;
    TestCallbackContext perception2_ctx;

    ret = cognitive_hub_subscribe(hub, MODULE_PERCEPTION_1, COG_EVENT_STATE_CHANGE,
                                   test_event_callback, &perception1_ctx);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_subscribe(hub, MODULE_PERCEPTION_2, COG_EVENT_STATE_CHANGE,
                                   test_event_callback, &perception2_ctx);
    ASSERT_EQ(ret, 0);

    /* Publish to perception category */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = MODULE_EXECUTIVE_1;
    event.priority = COG_PRIORITY_HIGH;

    ret = cognitive_hub_publish_to_category(hub, MODULE_EXECUTIVE_1,
                                             COG_CATEGORY_PERCEPTION,
                                             COG_EVENT_STATE_CHANGE, &event);
    EXPECT_EQ(ret, 0);

    /* Both perception modules should receive */
    EXPECT_EQ(perception1_ctx.events_received.load(), 1);
    EXPECT_EQ(perception2_ctx.events_received.load(), 1);
}

/* ============================================================================
 * Cascading Events Tests
 * ============================================================================ */

TEST_F(HubModuleIntegrationTest, CascadingEvents) {
    /* Register modules */
    int ret;

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "visual_processor", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "working_memory", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_REASONING_1,
                                         COG_CATEGORY_REASONING,
                                         "logical_reasoner", nullptr);
    ASSERT_EQ(ret, 0);

    /* Set up cascading callback context for memory module */
    TestCallbackContext cascade_ctx;
    cascade_ctx.hub = hub;
    cascade_ctx.publisher_id = MODULE_MEMORY_1;
    cascade_ctx.should_cascade = true;

    /* Memory subscribes to input and will cascade to output */
    ret = cognitive_hub_subscribe(hub, MODULE_MEMORY_1, COG_EVENT_INPUT_RECEIVED,
                                   cascading_event_callback, &cascade_ctx);
    ASSERT_EQ(ret, 0);

    /* Reasoning subscribes to output (should receive cascade) */
    TestCallbackContext reasoning_ctx;
    ret = cognitive_hub_subscribe(hub, MODULE_REASONING_1, COG_EVENT_OUTPUT_READY,
                                   test_event_callback, &reasoning_ctx);
    ASSERT_EQ(ret, 0);

    /* Perception publishes initial event */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PERCEPTION_1;
    event.priority = COG_PRIORITY_NORMAL;

    ret = cognitive_hub_publish(hub, MODULE_PERCEPTION_1, COG_EVENT_INPUT_RECEIVED, &event);
    EXPECT_EQ(ret, 0);

    /* Memory should have received input event */
    EXPECT_EQ(cascade_ctx.events_received.load(), 1);
    EXPECT_EQ(cascade_ctx.last_event_type, COG_EVENT_INPUT_RECEIVED);

    /* Reasoning should have received cascaded output event */
    EXPECT_EQ(reasoning_ctx.events_received.load(), 1);
    EXPECT_EQ(reasoning_ctx.last_event_type, COG_EVENT_OUTPUT_READY);
    EXPECT_EQ(reasoning_ctx.last_source_module, MODULE_MEMORY_1);

    /* Verify chain was: perception->memory->reasoning */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 2u);  /* Original + cascade */
    EXPECT_EQ(stats.events_delivered, 2u);  /* Memory + Reasoning */
}

/* ============================================================================
 * Module Query Chain Tests
 * ============================================================================ */

TEST_F(HubModuleIntegrationTest, ModuleQueryChain) {
    /* Register three modules for A -> B -> C query chain */
    int ret;

    /* Set up query handler contexts for B and C */
    QueryChainContext ctx_b;
    ctx_b.hub = hub;
    ctx_b.my_module_id = MODULE_MEMORY_1;
    ctx_b.next_module_id = MODULE_REASONING_1;  /* B forwards to C */
    ctx_b.query_count = 0;

    QueryChainContext ctx_c;
    ctx_c.hub = hub;
    ctx_c.my_module_id = MODULE_REASONING_1;
    ctx_c.next_module_id = 0;  /* C is end of chain */
    ctx_c.query_count = 0;

    /* Register modules - pass context for B and C so query handler can use it */
    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "module_a", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "module_b", &ctx_b);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_REASONING_1,
                                         COG_CATEGORY_REASONING,
                                         "module_c", &ctx_c);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_query_handler(hub, MODULE_MEMORY_1, chained_query_handler);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_query_handler(hub, MODULE_REASONING_1, chained_query_handler);
    ASSERT_EQ(ret, 0);

    /* Module A queries B (which will forward to C) */
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATUS;

    cognitive_query_result_t result;
    memset(&result, 0, sizeof(result));

    /* Query from A to B - B will forward to C using its context */
    ret = cognitive_hub_query_module(hub, MODULE_PERCEPTION_1, MODULE_MEMORY_1,
                                      &query, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.status, 0);

    /* Verify query was processed */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_GE(stats.queries_processed, 1u);
}

TEST_F(HubModuleIntegrationTest, QueryToInactiveModuleFails) {
    /* Register module and make it inactive */
    int ret;

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "source_module", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "target_module", nullptr);
    ASSERT_EQ(ret, 0);

    /* Make target inactive */
    ret = cognitive_hub_set_module_active(hub, MODULE_MEMORY_1, false);
    EXPECT_EQ(ret, 0);

    /* Query should fail */
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATUS;

    cognitive_query_result_t result;
    memset(&result, 0, sizeof(result));

    ret = cognitive_hub_query_module(hub, MODULE_PERCEPTION_1, MODULE_MEMORY_1,
                                      &query, &result);
    EXPECT_EQ(ret, -1);

    /* Stats should show failed query */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_GE(stats.queries_failed, 1u);
}

/* ============================================================================
 * Module State Management Tests
 * ============================================================================ */

TEST_F(HubModuleIntegrationTest, ModuleActivationDeactivation) {
    int ret;

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "test_module", nullptr);
    ASSERT_EQ(ret, 0);

    /* Initially active */
    cognitive_module_info_t info;
    ret = cognitive_hub_get_module_info(hub, MODULE_PERCEPTION_1, &info);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(info.is_active);

    /* Deactivate */
    ret = cognitive_hub_set_module_active(hub, MODULE_PERCEPTION_1, false);
    EXPECT_EQ(ret, 0);

    ret = cognitive_hub_get_module_info(hub, MODULE_PERCEPTION_1, &info);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(info.is_active);

    /* Reactivate */
    ret = cognitive_hub_set_module_active(hub, MODULE_PERCEPTION_1, true);
    EXPECT_EQ(ret, 0);

    ret = cognitive_hub_get_module_info(hub, MODULE_PERCEPTION_1, &info);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(info.is_active);
}

TEST_F(HubModuleIntegrationTest, UnregisterModuleCleansUpSubscriptions) {
    int ret;

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "publisher", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "subscriber", nullptr);
    ASSERT_EQ(ret, 0);

    /* Subscribe */
    TestCallbackContext ctx;
    ret = cognitive_hub_subscribe(hub, MODULE_MEMORY_1, COG_EVENT_INPUT_RECEIVED,
                                   test_event_callback, &ctx);
    ASSERT_EQ(ret, 0);

    /* Unregister subscriber */
    ret = cognitive_hub_unregister_module(hub, MODULE_MEMORY_1);
    EXPECT_EQ(ret, 0);

    /* Publish should succeed but deliver to no one */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PERCEPTION_1;

    ret = cognitive_hub_publish(hub, MODULE_PERCEPTION_1, COG_EVENT_INPUT_RECEIVED, &event);
    EXPECT_EQ(ret, 0);

    /* No events delivered */
    EXPECT_EQ(ctx.events_received.load(), 0);
}

/* ============================================================================
 * Category Query Tests
 * ============================================================================ */

TEST_F(HubModuleIntegrationTest, GetModulesByCategory) {
    int ret;

    /* Register multiple modules in different categories */
    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "visual", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_2,
                                         COG_CATEGORY_PERCEPTION,
                                         "auditory", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "memory", nullptr);
    ASSERT_EQ(ret, 0);

    /* Query perception modules */
    uint32_t module_ids[10];
    uint32_t count = 0;
    ret = cognitive_hub_get_modules_by_category(hub, COG_CATEGORY_PERCEPTION,
                                                 module_ids, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 2u);

    /* Verify both perception modules found */
    bool found_1 = false, found_2 = false;
    for (uint32_t i = 0; i < count; i++) {
        if (module_ids[i] == MODULE_PERCEPTION_1) found_1 = true;
        if (module_ids[i] == MODULE_PERCEPTION_2) found_2 = true;
    }
    EXPECT_TRUE(found_1);
    EXPECT_TRUE(found_2);

    /* Query memory modules */
    ret = cognitive_hub_get_modules_by_category(hub, COG_CATEGORY_MEMORY,
                                                 module_ids, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(module_ids[0], MODULE_MEMORY_1);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HubModuleIntegrationTest, StatisticsAccumulation) {
    int ret;

    /* Register modules */
    ret = cognitive_hub_register_module(hub, MODULE_PERCEPTION_1,
                                         COG_CATEGORY_PERCEPTION,
                                         "publisher", nullptr);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_register_module(hub, MODULE_MEMORY_1,
                                         COG_CATEGORY_MEMORY,
                                         "subscriber", nullptr);
    ASSERT_EQ(ret, 0);

    /* Subscribe */
    TestCallbackContext ctx;
    ret = cognitive_hub_subscribe(hub, MODULE_MEMORY_1, COG_EVENT_INPUT_RECEIVED,
                                   test_event_callback, &ctx);
    ASSERT_EQ(ret, 0);

    /* Publish multiple events */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PERCEPTION_1;

    for (int i = 0; i < 5; i++) {
        cognitive_hub_publish(hub, MODULE_PERCEPTION_1, COG_EVENT_INPUT_RECEIVED, &event);
    }

    /* Check stats */
    cognitive_hub_stats_t stats;
    ret = cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.registered_modules, 2u);
    EXPECT_EQ(stats.active_subscriptions, 1u);
    EXPECT_EQ(stats.events_published, 5u);
    EXPECT_EQ(stats.events_delivered, 5u);

    /* Reset and verify */
    ret = cognitive_hub_reset_stats(hub);
    EXPECT_EQ(ret, 0);

    ret = cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.events_delivered, 0u);
    /* Modules and subscriptions should remain */
    EXPECT_EQ(stats.registered_modules, 2u);
    EXPECT_EQ(stats.active_subscriptions, 1u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
