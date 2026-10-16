/**
 * @file test_nimcp_extended_mind.cpp
 * @brief Unit tests for extended mind (external cognitive extensions)
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_extended_mind.h"

/*=============================================================================
 * Mock Extension Functions
 *===========================================================================*/

static int mock_query_success(
    const void* query,
    size_t query_size,
    void* response,
    size_t* response_size,
    void* user_data
) {
    const char* resp = "mock_response";
    size_t len = strlen(resp);
    if (*response_size >= len) {
        memcpy(response, resp, len);
        *response_size = len;
        return 0;
    }
    return -1;
}

static int mock_query_fail(
    const void* query,
    size_t query_size,
    void* response,
    size_t* response_size,
    void* user_data
) {
    return -1;
}

static ext_health_t mock_status_healthy(void* user_data) {
    return EXT_HEALTH_HEALTHY;
}

static ext_health_t mock_status_degraded(void* user_data) {
    return EXT_HEALTH_DEGRADED;
}

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class ExtendedMindTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = extended_mind_default_config();
        em_ = extended_mind_create(&config_);
        ASSERT_NE(em_, nullptr);
    }

    void TearDown() override {
        if (em_) {
            extended_mind_destroy(em_);
            em_ = nullptr;
        }
    }

    cognitive_extension_t CreateMockExtension(
        extension_type_t type,
        const char* name,
        ext_query_fn query_fn = mock_query_success
    ) {
        cognitive_extension_t ext;
        memset(&ext, 0, sizeof(ext));
        ext.type = type;
        strncpy(ext.name, name, sizeof(ext.name) - 1);
        ext.query_fn = query_fn;
        ext.status_fn = mock_status_healthy;
        ext.reliability = 1.0f;
        ext.trust_level = 0.8f;
        ext.health = EXT_HEALTH_HEALTHY;
        return ext;
    }

    extended_mind_config_t config_;
    extended_mind_t* em_ = nullptr;
};

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, CreateWithNullConfig) {
    extended_mind_t* em = extended_mind_create(nullptr);
    ASSERT_NE(em, nullptr);
    extended_mind_destroy(em);
}

TEST_F(ExtendedMindTest, DestroyNull) {
    extended_mind_destroy(nullptr);  // Should not crash
}

TEST_F(ExtendedMindTest, Reset) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "test_memory");
    ASSERT_NE(extended_mind_register_extension(em_, &ext), 0u);

    EXPECT_EQ(extended_mind_reset(em_), 0);
    EXPECT_EQ(extended_mind_extension_count(em_), 0u);
}

/*=============================================================================
 * Extension Management Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, RegisterExtension) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "test_memory");
    uint32_t id = extended_mind_register_extension(em_, &ext);
    EXPECT_NE(id, 0u);
    EXPECT_EQ(extended_mind_extension_count(em_), 1u);
}

TEST_F(ExtendedMindTest, RegisterMultipleExtensions) {
    cognitive_extension_t ext1 = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    cognitive_extension_t ext2 = CreateMockExtension(EXT_TYPE_REASONING, "llm");
    cognitive_extension_t ext3 = CreateMockExtension(EXT_TYPE_PERCEPTION, "camera");

    EXPECT_NE(extended_mind_register_extension(em_, &ext1), 0u);
    EXPECT_NE(extended_mind_register_extension(em_, &ext2), 0u);
    EXPECT_NE(extended_mind_register_extension(em_, &ext3), 0u);

    EXPECT_EQ(extended_mind_extension_count(em_), 3u);
}

TEST_F(ExtendedMindTest, UnregisterExtension) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "test_memory");
    uint32_t id = extended_mind_register_extension(em_, &ext);
    ASSERT_NE(id, 0u);

    EXPECT_EQ(extended_mind_unregister_extension(em_, id), 0);
    EXPECT_EQ(extended_mind_extension_count(em_), 0u);
}

TEST_F(ExtendedMindTest, UnregisterNonexistentExtension) {
    EXPECT_EQ(extended_mind_unregister_extension(em_, 999), -1);
}

TEST_F(ExtendedMindTest, GetExtension) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "test_memory");
    uint32_t id = extended_mind_register_extension(em_, &ext);
    ASSERT_NE(id, 0u);

    cognitive_extension_t retrieved;
    ASSERT_EQ(extended_mind_get_extension(em_, id, &retrieved), 0);
    EXPECT_EQ(retrieved.type, EXT_TYPE_MEMORY);
    EXPECT_STREQ(retrieved.name, "test_memory");
}

TEST_F(ExtendedMindTest, CountByType) {
    cognitive_extension_t ext1 = CreateMockExtension(EXT_TYPE_MEMORY, "memory1");
    cognitive_extension_t ext2 = CreateMockExtension(EXT_TYPE_MEMORY, "memory2");
    cognitive_extension_t ext3 = CreateMockExtension(EXT_TYPE_REASONING, "llm");

    extended_mind_register_extension(em_, &ext1);
    extended_mind_register_extension(em_, &ext2);
    extended_mind_register_extension(em_, &ext3);

    EXPECT_EQ(extended_mind_count_by_type(em_, EXT_TYPE_MEMORY), 2u);
    EXPECT_EQ(extended_mind_count_by_type(em_, EXT_TYPE_REASONING), 1u);
    EXPECT_EQ(extended_mind_count_by_type(em_, EXT_TYPE_ACTION), 0u);
}

/*=============================================================================
 * Query Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, QuerySync) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    const char* query = "test_query";
    char response[256];
    size_t response_size = sizeof(response);

    int result = extended_mind_query_sync(em_, EXT_TYPE_MEMORY,
                                          query, strlen(query),
                                          response, &response_size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(response_size, 0u);
}

TEST_F(ExtendedMindTest, QuerySyncNoExtension) {
    const char* query = "test_query";
    char response[256];
    size_t response_size = sizeof(response);

    int result = extended_mind_query_sync(em_, EXT_TYPE_MEMORY,
                                          query, strlen(query),
                                          response, &response_size);
    EXPECT_EQ(result, -1);
}

TEST_F(ExtendedMindTest, QueryAsync) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    ext_query_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = EXT_TYPE_MEMORY;
    request.timeout_ms = 1000;
    request.priority = 0.5f;

    const char* query = "test_query";
    uint32_t query_id = extended_mind_query(em_, &request, query, strlen(query));
    EXPECT_NE(query_id, 0u);
}

TEST_F(ExtendedMindTest, CancelQuery) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    ext_query_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = EXT_TYPE_MEMORY;

    const char* query = "test";
    uint32_t query_id = extended_mind_query(em_, &request, query, strlen(query));
    ASSERT_NE(query_id, 0u);

    EXPECT_EQ(extended_mind_cancel_query(em_, query_id), 0);
}

/*=============================================================================
 * State Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, GetState) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    extended_mind_state_t state;
    ASSERT_EQ(extended_mind_get_state(em_, &state), 0);
    EXPECT_EQ(state.active_extensions, 1u);
    EXPECT_GE(state.total_cognitive_capacity, 1.0f);
}

TEST_F(ExtendedMindTest, GetCapacity) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    ext.integration_depth = 0.5f;
    extended_mind_register_extension(em_, &ext);

    float capacity = extended_mind_get_capacity(em_, EXT_TYPE_MEMORY);
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(ExtendedMindTest, BestExtension) {
    cognitive_extension_t ext1 = CreateMockExtension(EXT_TYPE_MEMORY, "memory1");
    ext1.reliability = 0.5f;
    cognitive_extension_t ext2 = CreateMockExtension(EXT_TYPE_MEMORY, "memory2");
    ext2.reliability = 0.9f;

    uint32_t id1 = extended_mind_register_extension(em_, &ext1);
    uint32_t id2 = extended_mind_register_extension(em_, &ext2);

    // Higher reliability should be chosen
    uint32_t best = extended_mind_best_extension(em_, EXT_TYPE_MEMORY);
    EXPECT_EQ(best, id2);
}

TEST_F(ExtendedMindTest, BestExtensionNoneAvailable) {
    uint32_t best = extended_mind_best_extension(em_, EXT_TYPE_MEMORY);
    EXPECT_EQ(best, 0u);
}

/*=============================================================================
 * Update Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, Update) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    EXPECT_EQ(extended_mind_update(em_), 0);
}

TEST_F(ExtendedMindTest, UpdateExtensionStats) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    ext.reliability = 1.0f;
    uint32_t id = extended_mind_register_extension(em_, &ext);

    // Record some failures
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(extended_mind_update_extension_stats(em_, id, false, 100.0f), 0);
    }

    cognitive_extension_t updated;
    ASSERT_EQ(extended_mind_get_extension(em_, id, &updated), 0);
    EXPECT_LT(updated.reliability, 1.0f);
    EXPECT_EQ(updated.failed_queries, 5u);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, GetStats) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    const char* query = "test";
    char response[256];
    size_t response_size = sizeof(response);
    extended_mind_query_sync(em_, EXT_TYPE_MEMORY, query, strlen(query),
                             response, &response_size);

    extended_mind_stats_t stats;
    ASSERT_EQ(extended_mind_get_stats(em_, &stats), 0);
    EXPECT_GT(stats.successful_queries, 0u);
}

TEST_F(ExtendedMindTest, ResetStats) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    const char* query = "test";
    char response[256];
    size_t response_size = sizeof(response);
    extended_mind_query_sync(em_, EXT_TYPE_MEMORY, query, strlen(query),
                             response, &response_size);

    extended_mind_reset_stats(em_);

    extended_mind_stats_t stats;
    ASSERT_EQ(extended_mind_get_stats(em_, &stats), 0);
    EXPECT_EQ(stats.successful_queries, 0u);
}

/*=============================================================================
 * Offload Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, Offload) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_REASONING, "llm");
    extended_mind_register_extension(em_, &ext);

    ext_offload_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = EXT_TYPE_REASONING;
    request.source_instance = 1;
    request.estimated_load = 0.5f;

    const char* task = "compute_something";
    uint32_t id = extended_mind_offload(em_, &request, task, strlen(task));
    EXPECT_NE(id, 0u);
}

TEST_F(ExtendedMindTest, CheckOffload) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_REASONING, "llm");
    extended_mind_register_extension(em_, &ext);

    ext_offload_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = EXT_TYPE_REASONING;

    const char* task = "task";
    uint32_t id = extended_mind_offload(em_, &request, task, strlen(task));
    ASSERT_NE(id, 0u);

    char result[256];
    size_t result_size = sizeof(result);
    int status = extended_mind_check_offload(em_, id, result, &result_size);
    // 0 = complete, 1 = pending
    EXPECT_TRUE(status == 0 || status == 1);
}

/*=============================================================================
 * Debug Tests
 *===========================================================================*/

TEST_F(ExtendedMindTest, DumpDoesNotCrash) {
    cognitive_extension_t ext = CreateMockExtension(EXT_TYPE_MEMORY, "memory");
    extended_mind_register_extension(em_, &ext);

    extended_mind_dump(em_);  // Should not crash
    extended_mind_dump(nullptr);  // Should not crash
}
