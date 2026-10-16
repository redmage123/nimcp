/**
 * @file test_rcog_tool_router.cpp
 * @brief Unit tests for Recursive Cognition Tool Router
 *
 * WHAT: Comprehensive tests for RCOG tool router functionality
 * WHY:  Router controls tool access by tier - security critical
 * HOW:  Unit tests for lifecycle, registration, access control, invocation
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <string.h>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"

//=============================================================================
// Test Helpers
//=============================================================================

static rcog_error_t test_tool_handler(
    const void* input,
    size_t input_size,
    void* tool_context,
    void** output,
    size_t* output_size)
{
    (void)input;
    (void)input_size;
    (void)tool_context;
    *output = nullptr;
    *output_size = 0;
    return RCOG_OK;
}

static rcog_error_t echo_tool_handler(
    const void* input,
    size_t input_size,
    void* tool_context,
    void** output,
    size_t* output_size)
{
    (void)tool_context;
    if (input && input_size > 0) {
        *output = malloc(input_size);
        if (*output) {
            memcpy(*output, input, input_size);
            *output_size = input_size;
        }
    }
    return RCOG_OK;
}

static rcog_error_t failing_tool_handler(
    const void* input,
    size_t input_size,
    void* tool_context,
    void** output,
    size_t* output_size)
{
    (void)input;
    (void)input_size;
    (void)tool_context;
    *output = nullptr;
    *output_size = 0;
    return RCOG_ERROR_SUBTASK_FAILED;  // Tool execution error
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Basic tool router test fixture
 */
class RcogToolRouterTest : public ::testing::Test {
protected:
    rcog_tool_router_t* router;

    void SetUp() override
    {
        router = rcog_tool_router_create_default();
        ASSERT_NE(router, nullptr);
    }

    void TearDown() override
    {
        if (router) {
            rcog_tool_router_destroy(router);
            router = nullptr;
        }
    }
};

/**
 * @brief Tool router with registered tools
 */
class RcogToolRouterWithToolsTest : public ::testing::Test {
protected:
    rcog_tool_router_t* router;

    void SetUp() override
    {
        router = rcog_tool_router_create_default();
        ASSERT_NE(router, nullptr);

        // Register test tools at different tiers
        rcog_tool_def_t l1_tool = rcog_tool_def_create("l1_test", test_tool_handler,
                                                        RCOG_TIER_L1_REASONING);
        rcog_tool_router_register(router, &l1_tool);

        rcog_tool_def_t l2_tool = rcog_tool_def_create("l2_test", test_tool_handler,
                                                        RCOG_TIER_L2_PERCEPTION);
        rcog_tool_router_register(router, &l2_tool);

        rcog_tool_def_t l3_tool = rcog_tool_def_create("l3_test", test_tool_handler,
                                                        RCOG_TIER_L3_ACTION);
        rcog_tool_router_register(router, &l3_tool);
    }

    void TearDown() override
    {
        if (router) {
            rcog_tool_router_destroy(router);
            router = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(RcogToolRouterLifecycleTest, DefaultConfig)
{
    rcog_tool_router_config_t config = rcog_tool_router_default_config();

    EXPECT_GT(config.default_timeout_ms, 0u);
    EXPECT_TRUE(config.enable_metrics);
}

TEST(RcogToolRouterLifecycleTest, CreateDefault)
{
    rcog_tool_router_t* router = rcog_tool_router_create_default();
    ASSERT_NE(router, nullptr);

    rcog_tool_router_destroy(router);
}

TEST(RcogToolRouterLifecycleTest, CreateWithConfig)
{
    rcog_tool_router_config_t config = rcog_tool_router_default_config();
    config.default_timeout_ms = 5000;
    config.enable_async = true;
    config.enable_streaming = false;

    rcog_tool_router_t* router = rcog_tool_router_create(&config);
    ASSERT_NE(router, nullptr);

    rcog_tool_router_destroy(router);
}

TEST(RcogToolRouterLifecycleTest, DestroyNull)
{
    // Should not crash
    rcog_tool_router_destroy(nullptr);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(RcogToolRouterTest, ConnectContextStore)
{
    rcog_context_store_t* store = rcog_context_store_create_default();
    ASSERT_NE(store, nullptr);

    int result = rcog_tool_router_connect_context_store(router, store);
    EXPECT_EQ(result, 0);

    rcog_context_store_destroy(store);
}

TEST(RcogToolRouterConnectionTest, ConnectNullRouter)
{
    rcog_context_store_t* store = rcog_context_store_create_default();

    int result = rcog_tool_router_connect_context_store(nullptr, store);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_context_store_destroy(store);
}

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(RcogToolRouterTest, RegisterTool)
{
    rcog_tool_def_t def = rcog_tool_def_create("test_tool", test_tool_handler,
                                                RCOG_TIER_L1_REASONING);

    int result = rcog_tool_router_register(router, &def);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(rcog_tool_router_has_tool(router, "test_tool"));
}

TEST_F(RcogToolRouterTest, RegisterMultipleTools)
{
    rcog_tool_def_t def1 = rcog_tool_def_create("tool_a", test_tool_handler,
                                                 RCOG_TIER_L1_REASONING);
    rcog_tool_def_t def2 = rcog_tool_def_create("tool_b", test_tool_handler,
                                                 RCOG_TIER_L2_PERCEPTION);

    rcog_tool_router_register(router, &def1);
    rcog_tool_router_register(router, &def2);

    EXPECT_TRUE(rcog_tool_router_has_tool(router, "tool_a"));
    EXPECT_TRUE(rcog_tool_router_has_tool(router, "tool_b"));
}

TEST_F(RcogToolRouterTest, RegisterBatch)
{
    rcog_tool_def_t defs[3];
    defs[0] = rcog_tool_def_create("batch_a", test_tool_handler, RCOG_TIER_L1_REASONING);
    defs[1] = rcog_tool_def_create("batch_b", test_tool_handler, RCOG_TIER_L2_PERCEPTION);
    defs[2] = rcog_tool_def_create("batch_c", test_tool_handler, RCOG_TIER_L3_ACTION);

    size_t registered = rcog_tool_router_register_batch(router, defs, 3);
    EXPECT_EQ(registered, 3u);
}

TEST_F(RcogToolRouterTest, UnregisterTool)
{
    rcog_tool_def_t def = rcog_tool_def_create("temp_tool", test_tool_handler,
                                                RCOG_TIER_L1_REASONING);
    rcog_tool_router_register(router, &def);

    EXPECT_TRUE(rcog_tool_router_has_tool(router, "temp_tool"));

    int result = rcog_tool_router_unregister(router, "temp_tool");
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(rcog_tool_router_has_tool(router, "temp_tool"));
}

TEST_F(RcogToolRouterTest, UnregisterNonexistent)
{
    int result = rcog_tool_router_unregister(router, "nonexistent");
    EXPECT_EQ(result, RCOG_ERROR_TOOL_NOT_FOUND);
}

TEST(RcogToolRouterRegistrationTest, RegisterNull)
{
    rcog_tool_router_t* router = rcog_tool_router_create_default();

    int result = rcog_tool_router_register(router, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_tool_def_t def = rcog_tool_def_create("test", test_tool_handler,
                                                RCOG_TIER_L1_REASONING);
    result = rcog_tool_router_register(nullptr, &def);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_tool_router_destroy(router);
}

//=============================================================================
// Tool Definition Tests
//=============================================================================

TEST(RcogToolDefTest, CreateToolDef)
{
    rcog_tool_def_t def = rcog_tool_def_create("my_tool", test_tool_handler,
                                                RCOG_TIER_L2_PERCEPTION);

    EXPECT_STREQ(def.name, "my_tool");
    EXPECT_EQ(def.handler, test_tool_handler);
    EXPECT_EQ(def.min_tier, RCOG_TIER_L2_PERCEPTION);
}

TEST_F(RcogToolRouterTest, GetToolDef)
{
    rcog_tool_def_t orig = rcog_tool_def_create("retrieval_test", test_tool_handler,
                                                 RCOG_TIER_L1_REASONING);
    strncpy(orig.description, "Test tool description", RCOG_ROUTER_MAX_TOOL_DESC - 1);

    rcog_tool_router_register(router, &orig);

    rcog_tool_def_t retrieved;
    int result = rcog_tool_router_get_tool(router, "retrieval_test", &retrieved);
    EXPECT_EQ(result, 0);

    EXPECT_STREQ(retrieved.name, "retrieval_test");
    EXPECT_STREQ(retrieved.description, "Test tool description");
}

TEST_F(RcogToolRouterTest, GetNonexistentTool)
{
    rcog_tool_def_t def;
    int result = rcog_tool_router_get_tool(router, "nonexistent", &def);
    EXPECT_EQ(result, RCOG_ERROR_TOOL_NOT_FOUND);
}

//=============================================================================
// Access Control Tests
//=============================================================================

TEST_F(RcogToolRouterWithToolsTest, CanAccessSameTier)
{
    bool can_access = rcog_tool_router_can_access(router, "l1_test",
                                                   RCOG_TIER_L1_REASONING);
    EXPECT_TRUE(can_access);
}

TEST_F(RcogToolRouterWithToolsTest, CanAccessHigherTier)
{
    // Higher tier can access lower tier tools
    bool can_access = rcog_tool_router_can_access(router, "l1_test",
                                                   RCOG_TIER_L2_PERCEPTION);
    EXPECT_TRUE(can_access);

    can_access = rcog_tool_router_can_access(router, "l1_test",
                                              RCOG_TIER_L3_ACTION);
    EXPECT_TRUE(can_access);
}

TEST_F(RcogToolRouterWithToolsTest, CannotAccessLowerTier)
{
    // Lower tier cannot access higher tier tools
    bool can_access = rcog_tool_router_can_access(router, "l3_test",
                                                   RCOG_TIER_L1_REASONING);
    EXPECT_FALSE(can_access);

    can_access = rcog_tool_router_can_access(router, "l2_test",
                                              RCOG_TIER_L1_REASONING);
    EXPECT_FALSE(can_access);
}

TEST_F(RcogToolRouterWithToolsTest, RootHasNoAccess)
{
    // ROOT tier has NO tool access
    bool can_access = rcog_tool_router_can_access(router, "l1_test",
                                                   RCOG_TIER_ROOT);
    EXPECT_FALSE(can_access);

    can_access = rcog_tool_router_can_access(router, "l2_test",
                                              RCOG_TIER_ROOT);
    EXPECT_FALSE(can_access);

    can_access = rcog_tool_router_can_access(router, "l3_test",
                                              RCOG_TIER_ROOT);
    EXPECT_FALSE(can_access);
}

TEST_F(RcogToolRouterWithToolsTest, GetAccessibleTools)
{
    char tools[20][RCOG_ROUTER_MAX_TOOL_NAME];
    size_t count = 0;

    int result = rcog_tool_router_get_accessible_tools(
        router, RCOG_TIER_L3_ACTION, tools, 20, &count);
    EXPECT_EQ(result, 0);

    // L3 should see all tools (L1, L2, L3)
    EXPECT_GE(count, 3u);
}

TEST_F(RcogToolRouterWithToolsTest, GetMinTier)
{
    rcog_capability_tier_t tier;

    int result = rcog_tool_router_get_min_tier(router, "l1_test", &tier);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(tier, RCOG_TIER_L1_REASONING);

    result = rcog_tool_router_get_min_tier(router, "l2_test", &tier);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(tier, RCOG_TIER_L2_PERCEPTION);
}

//=============================================================================
// Category Tests
//=============================================================================

TEST_F(RcogToolRouterTest, RegisterCategory)
{
    int result = rcog_tool_router_register_category(
        router, "reasoning", "Reasoning and logic tools",
        RCOG_TIER_L1_REASONING);
    EXPECT_EQ(result, 0);
}

TEST_F(RcogToolRouterTest, ListCategories)
{
    rcog_tool_router_register_category(router, "cat_a", "Category A",
                                        RCOG_TIER_L1_REASONING);
    rcog_tool_router_register_category(router, "cat_b", "Category B",
                                        RCOG_TIER_L2_PERCEPTION);

    char categories[10][RCOG_ROUTER_MAX_CATEGORY_NAME];
    size_t count = 0;

    int result = rcog_tool_router_list_categories(router, categories, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 2u);
}

TEST_F(RcogToolRouterTest, GetCategoryTools)
{
    // Register a category and tools
    rcog_tool_router_register_category(router, "test_cat", "Test",
                                        RCOG_TIER_L1_REASONING);

    rcog_tool_def_t def = rcog_tool_def_create("cat_tool", test_tool_handler,
                                                RCOG_TIER_L1_REASONING);
    strncpy(def.category, "test_cat", RCOG_ROUTER_MAX_CATEGORY_NAME - 1);
    rcog_tool_router_register(router, &def);

    char tools[10][RCOG_ROUTER_MAX_TOOL_NAME];
    size_t count = 0;

    int result = rcog_tool_router_get_category_tools(router, "test_cat",
                                                      tools, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 1u);
}

//=============================================================================
// Invocation Tests
//=============================================================================

TEST_F(RcogToolRouterWithToolsTest, InvokeTool)
{
    rcog_tool_request_t request = rcog_tool_request_create(
        "l1_test", nullptr, 0, RCOG_TIER_L1_REASONING);

    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, &request, &result);
    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);

    rcog_tool_router_free_result(&result);
}

TEST_F(RcogToolRouterWithToolsTest, InvokeWithHigherTier)
{
    // L3 can access L1 tools
    rcog_tool_request_t request = rcog_tool_request_create(
        "l1_test", nullptr, 0, RCOG_TIER_L3_ACTION);

    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, &request, &result);
    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);

    rcog_tool_router_free_result(&result);
}

TEST_F(RcogToolRouterWithToolsTest, InvokeAccessDenied)
{
    // L1 cannot access L3 tools
    rcog_tool_request_t request = rcog_tool_request_create(
        "l3_test", nullptr, 0, RCOG_TIER_L1_REASONING);

    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, &request, &result);
    EXPECT_EQ(err, RCOG_ERROR_TOOL_ACCESS_DENIED);
    EXPECT_FALSE(result.success);
}

TEST_F(RcogToolRouterWithToolsTest, InvokeNonexistent)
{
    rcog_tool_request_t request = rcog_tool_request_create(
        "nonexistent", nullptr, 0, RCOG_TIER_L1_REASONING);

    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, &request, &result);
    EXPECT_EQ(err, RCOG_ERROR_TOOL_NOT_FOUND);
}

TEST_F(RcogToolRouterTest, InvokeEchoTool)
{
    rcog_tool_def_t def = rcog_tool_def_create("echo", echo_tool_handler,
                                                RCOG_TIER_L1_REASONING);
    rcog_tool_router_register(router, &def);

    const char* input = "Hello, World!";
    rcog_tool_request_t request = rcog_tool_request_create(
        "echo", input, strlen(input) + 1, RCOG_TIER_L1_REASONING);

    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, &request, &result);
    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output_size, strlen(input) + 1);
    EXPECT_STREQ((char*)result.output, input);

    rcog_tool_router_free_result(&result);
}

TEST_F(RcogToolRouterTest, InvokeFailingTool)
{
    rcog_tool_def_t def = rcog_tool_def_create("failing", failing_tool_handler,
                                                RCOG_TIER_L1_REASONING);
    rcog_tool_router_register(router, &def);

    rcog_tool_request_t request = rcog_tool_request_create(
        "failing", nullptr, 0, RCOG_TIER_L1_REASONING);

    rcog_tool_result_t result;
    int err = rcog_tool_router_invoke(router, &request, &result);
    EXPECT_NE(err, 0);  // Should fail
    EXPECT_FALSE(result.success);
}

TEST(RcogToolRouterInvokeTest, InvokeNullParams)
{
    rcog_tool_router_t* router = rcog_tool_router_create_default();
    rcog_tool_request_t request = rcog_tool_request_create(
        "test", nullptr, 0, RCOG_TIER_L1_REASONING);
    rcog_tool_result_t result;

    int err = rcog_tool_router_invoke(nullptr, &request, &result);
    EXPECT_EQ(err, RCOG_ERROR_NULL_POINTER);

    err = rcog_tool_router_invoke(router, nullptr, &result);
    EXPECT_EQ(err, RCOG_ERROR_NULL_POINTER);

    err = rcog_tool_router_invoke(router, &request, nullptr);
    EXPECT_EQ(err, RCOG_ERROR_NULL_POINTER);

    rcog_tool_router_destroy(router);
}

//=============================================================================
// Discovery Tests
//=============================================================================

TEST_F(RcogToolRouterWithToolsTest, ListTools)
{
    char tools[20][RCOG_ROUTER_MAX_TOOL_NAME];
    size_t count = 0;

    int result = rcog_tool_router_list_tools(router, tools, 20, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 3u);  // At least our 3 test tools
}

TEST_F(RcogToolRouterWithToolsTest, ListToolsByTier)
{
    char tools[20][RCOG_ROUTER_MAX_TOOL_NAME];
    size_t count = 0;

    int result = rcog_tool_router_list_tools_by_tier(
        router, RCOG_TIER_L1_REASONING, tools, 20, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 1u);
}

TEST_F(RcogToolRouterWithToolsTest, SearchTools)
{
    char tools[20][RCOG_ROUTER_MAX_TOOL_NAME];
    size_t count = 0;

    /* Search uses substring match, not glob patterns */
    int result = rcog_tool_router_search_tools(router, "_test",
                                                tools, 20, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GE(count, 3u);  // l1_test, l2_test, l3_test
}

TEST_F(RcogToolRouterWithToolsTest, GetToolCount)
{
    size_t count = rcog_tool_router_get_tool_count(router);
    EXPECT_GE(count, 3u);
}

//=============================================================================
// Builtin Tools Tests
//=============================================================================

TEST_F(RcogToolRouterTest, RegisterL1Builtins)
{
    size_t count = rcog_tool_router_register_l1_builtins(router);
    EXPECT_GT(count, 0u);

    // Check for known L1 tools
    EXPECT_TRUE(rcog_tool_router_has_tool(router, "memory_read"));
    EXPECT_TRUE(rcog_tool_router_has_tool(router, "memory_write"));
}

TEST_F(RcogToolRouterTest, RegisterL2Builtins)
{
    size_t count = rcog_tool_router_register_l2_builtins(router);
    EXPECT_GT(count, 0u);

    EXPECT_TRUE(rcog_tool_router_has_tool(router, "feature_extract"));
    EXPECT_TRUE(rcog_tool_router_has_tool(router, "pattern_match"));
}

TEST_F(RcogToolRouterTest, RegisterL3Builtins)
{
    size_t count = rcog_tool_router_register_l3_builtins(router);
    EXPECT_GT(count, 0u);

    EXPECT_TRUE(rcog_tool_router_has_tool(router, "output_text"));
}

TEST_F(RcogToolRouterTest, RegisterAllBuiltins)
{
    size_t count = rcog_tool_router_register_all_builtins(router);
    EXPECT_GT(count, 3u);  // At least 3 levels of builtins
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(RcogToolRouterWithToolsTest, GetRouterStats)
{
    rcog_router_stats_t stats;
    int result = rcog_tool_router_get_stats(router, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_GE(stats.tools_registered, 3u);
}

TEST_F(RcogToolRouterWithToolsTest, GetToolStats)
{
    // Invoke tool first
    rcog_tool_request_t request = rcog_tool_request_create(
        "l1_test", nullptr, 0, RCOG_TIER_L1_REASONING);
    rcog_tool_result_t result;
    rcog_tool_router_invoke(router, &request, &result);
    rcog_tool_router_free_result(&result);

    rcog_tool_stats_t stats;
    int err = rcog_tool_router_get_tool_stats(router, "l1_test", &stats);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(stats.invocations, 1u);
    EXPECT_EQ(stats.successes, 1u);
}

TEST_F(RcogToolRouterTest, ResetStats)
{
    rcog_tool_router_reset_stats(router);

    rcog_router_stats_t stats;
    rcog_tool_router_get_stats(router, &stats);
    EXPECT_EQ(stats.total_invocations, 0u);
}

TEST(RcogToolRouterStatsTest, StatsNullParams)
{
    rcog_router_stats_t stats;
    int result = rcog_tool_router_get_stats(nullptr, &stats);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_tool_router_t* router = rcog_tool_router_create_default();
    result = rcog_tool_router_get_stats(router, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_tool_router_destroy(router);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST(RcogToolRouterUtilityTest, TierName)
{
    EXPECT_STREQ(rcog_tool_tier_name(RCOG_TIER_ROOT), "root");
    EXPECT_STREQ(rcog_tool_tier_name(RCOG_TIER_L1_REASONING), "L1_reasoning");
    EXPECT_STREQ(rcog_tool_tier_name(RCOG_TIER_L2_PERCEPTION), "L2_perception");
    EXPECT_STREQ(rcog_tool_tier_name(RCOG_TIER_L3_ACTION), "L3_action");
    EXPECT_STREQ(rcog_tool_tier_name(RCOG_TIER_L4_SPECIALIZED), "L4_specialized");
}

TEST(RcogToolRouterUtilityTest, IOTypeName)
{
    EXPECT_STREQ(rcog_tool_io_type_name(RCOG_TOOL_IO_ANY), "any");
    EXPECT_STREQ(rcog_tool_io_type_name(RCOG_TOOL_IO_TEXT), "text");
    EXPECT_STREQ(rcog_tool_io_type_name(RCOG_TOOL_IO_JSON), "json");
    EXPECT_STREQ(rcog_tool_io_type_name(RCOG_TOOL_IO_BINARY), "binary");
    EXPECT_STREQ(rcog_tool_io_type_name(RCOG_TOOL_IO_TENSOR), "tensor");
}

TEST(RcogToolRouterUtilityTest, CreateToolRequest)
{
    const char* data = "test input";
    rcog_tool_request_t request = rcog_tool_request_create(
        "my_tool", data, strlen(data) + 1, RCOG_TIER_L2_PERCEPTION);

    EXPECT_STREQ(request.tool_name, "my_tool");
    EXPECT_EQ(request.input, data);
    EXPECT_EQ(request.input_size, strlen(data) + 1);
    EXPECT_EQ(request.caller_tier, RCOG_TIER_L2_PERCEPTION);
}

//=============================================================================
// Access Policy Tests
//=============================================================================

TEST_F(RcogToolRouterTest, SetAccessPolicy)
{
    rcog_access_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    policy.allow_cross_tier = false;
    policy.audit_all_calls = true;
    policy.max_concurrent = 10;

    int result = rcog_tool_router_set_access_policy(router, &policy);
    EXPECT_EQ(result, 0);
}

TEST(RcogToolRouterPolicyTest, SetAccessPolicyNull)
{
    rcog_access_policy_t policy;

    int result = rcog_tool_router_set_access_policy(nullptr, &policy);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_tool_router_t* router = rcog_tool_router_create_default();
    result = rcog_tool_router_set_access_policy(router, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_tool_router_destroy(router);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
