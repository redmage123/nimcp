/**
 * @file test_rcog_context_store.cpp
 * @brief Unit tests for Recursive Cognition Context Store
 *
 * WHAT: Comprehensive tests for RLM-style context store functionality
 * WHY:  Context store implements "environment as variable" pattern - must work correctly
 * HOW:  Unit tests for variable storage, queries, access patterns, statistics
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <string.h>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * WHAT: Test fixture for context store tests
 * WHY:  Set up/tear down context store for each test
 */
class ContextStoreTest : public ::testing::Test {
protected:
    rcog_context_store_t* store;

    void SetUp() override
    {
        store = rcog_context_store_create_default();
        ASSERT_NE(store, nullptr);
    }

    void TearDown() override
    {
        if (store) {
            rcog_context_store_destroy(store);
            store = nullptr;
        }
    }
};

/**
 * WHAT: Test fixture with custom configuration
 * WHY:  Test non-default configurations
 */
class ContextStoreCustomConfigTest : public ::testing::Test {
protected:
    rcog_context_store_t* store;

    void SetUp() override
    {
        rcog_context_store_config_t config = rcog_context_store_default_config();
        config.max_variables = 32;
        config.max_variable_size = 4096;
        config.output_limit_per_query = 1024;

        store = rcog_context_store_create(&config);
        ASSERT_NE(store, nullptr);
    }

    void TearDown() override
    {
        if (store) {
            rcog_context_store_destroy(store);
            store = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test default configuration
 * WHY:  Verify sensible defaults are provided
 */
TEST(ContextStoreLifecycleTest, DefaultConfig)
{
    rcog_context_store_config_t config = rcog_context_store_default_config();

    EXPECT_GT(config.max_variables, 0u);
    EXPECT_GT(config.max_variable_size, 0u);
    EXPECT_GT(config.max_total_size, 0u);
    EXPECT_GT(config.output_limit_per_query, 0u);
}

/**
 * WHAT: Test context store creation with defaults
 * WHY:  Verify basic creation works
 */
TEST(ContextStoreLifecycleTest, CreateDefault)
{
    rcog_context_store_t* store = rcog_context_store_create_default();
    ASSERT_NE(store, nullptr);

    rcog_context_store_destroy(store);
}

/**
 * WHAT: Test context store creation with custom config
 * WHY:  Verify custom configuration is applied
 */
TEST(ContextStoreLifecycleTest, CreateWithConfig)
{
    rcog_context_store_config_t config = rcog_context_store_default_config();
    config.max_variables = 128;

    rcog_context_store_t* store = rcog_context_store_create(&config);
    ASSERT_NE(store, nullptr);

    rcog_context_store_destroy(store);
}

/**
 * WHAT: Test context store creation with NULL config
 * WHY:  Verify NULL config uses defaults
 */
TEST(ContextStoreLifecycleTest, CreateWithNullConfig)
{
    rcog_context_store_t* store = rcog_context_store_create(NULL);
    ASSERT_NE(store, nullptr);

    rcog_context_store_destroy(store);
}

/**
 * WHAT: Test destroy with NULL (should be safe)
 * WHY:  Verify NULL-safe destruction
 */
TEST(ContextStoreLifecycleTest, DestroyNull)
{
    // Should not crash
    rcog_context_store_destroy(NULL);
}

//=============================================================================
// Variable Storage Tests
//=============================================================================

/**
 * WHAT: Test storing text variable
 * WHY:  Basic text storage is core functionality
 */
TEST_F(ContextStoreTest, StoreText)
{
    const char* text = "Hello, recursive cognition!";

    rcog_error_t err = rcog_context_store_set_text(store, "greeting", text);
    EXPECT_EQ(err, RCOG_OK);

    // Verify it exists
    EXPECT_TRUE(rcog_context_store_exists(store, "greeting"));
}

/**
 * WHAT: Test storing binary data
 * WHY:  Binary data storage for embeddings, tensors
 */
TEST_F(ContextStoreTest, StoreBinary)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    rcog_error_t err = rcog_context_store_set(store, "binary_data", data, sizeof(data), RCOG_DTYPE_BINARY);
    EXPECT_EQ(err, RCOG_OK);

    EXPECT_TRUE(rcog_context_store_exists(store, "binary_data"));
}

/**
 * WHAT: Test storing with NULL store
 * WHY:  Verify proper error handling
 */
TEST_F(ContextStoreTest, StoreNullStore)
{
    const char* text = "test";
    rcog_error_t err = rcog_context_store_set_text(NULL, "var", text);
    EXPECT_EQ(err, RCOG_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test storing with NULL name
 * WHY:  Verify proper error handling
 */
TEST_F(ContextStoreTest, StoreNullName)
{
    const char* text = "test";
    rcog_error_t err = rcog_context_store_set_text(store, NULL, text);
    EXPECT_EQ(err, RCOG_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test storing with NULL data
 * WHY:  Verify proper error handling
 */
TEST_F(ContextStoreTest, StoreNullData)
{
    rcog_error_t err = rcog_context_store_set(store, "var", NULL, 100, RCOG_DTYPE_BINARY);
    EXPECT_EQ(err, RCOG_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test overwriting existing variable
 * WHY:  Verify update semantics work
 */
TEST_F(ContextStoreTest, OverwriteVariable)
{
    rcog_context_store_set_text(store, "var", "first value");
    rcog_error_t err = rcog_context_store_set_text(store, "var", "second value");
    EXPECT_EQ(err, RCOG_OK);

    // Query to verify new value
    rcog_query_result_t result = {0};
    err = rcog_context_store_query(store, "var", RCOG_ACCESS_FULL, NULL, &result);
    EXPECT_EQ(err, RCOG_OK);

    if (result.data) {
        EXPECT_STREQ((const char*)result.data, "second value");
        rcog_query_result_free(&result);
    }
}

/**
 * WHAT: Test removing variable
 * WHY:  Verify variable removal works
 */
TEST_F(ContextStoreTest, RemoveVariable)
{
    rcog_context_store_set_text(store, "temp", "temporary");
    EXPECT_TRUE(rcog_context_store_exists(store, "temp"));

    rcog_error_t err = rcog_context_store_remove(store, "temp");
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_FALSE(rcog_context_store_exists(store, "temp"));
}

/**
 * WHAT: Test removing non-existent variable
 * WHY:  Verify proper error handling
 */
TEST_F(ContextStoreTest, RemoveNonExistent)
{
    rcog_error_t err = rcog_context_store_remove(store, "nonexistent");
    EXPECT_EQ(err, RCOG_ERROR_CONTEXT_NOT_FOUND);
}

/**
 * WHAT: Test exists for non-existent variable
 * WHY:  Verify false return for missing variables
 */
TEST_F(ContextStoreTest, ExistsNonExistent)
{
    EXPECT_FALSE(rcog_context_store_exists(store, "does_not_exist"));
}

/**
 * WHAT: Test clearing all variables
 * WHY:  Verify bulk clear works
 */
TEST_F(ContextStoreTest, ClearAll)
{
    rcog_context_store_set_text(store, "var1", "value1");
    rcog_context_store_set_text(store, "var2", "value2");
    rcog_context_store_set_text(store, "var3", "value3");

    rcog_error_t err = rcog_context_store_clear(store);
    EXPECT_EQ(err, RCOG_OK);

    EXPECT_FALSE(rcog_context_store_exists(store, "var1"));
    EXPECT_FALSE(rcog_context_store_exists(store, "var2"));
    EXPECT_FALSE(rcog_context_store_exists(store, "var3"));
}

//=============================================================================
// Query Tests (Core RLM Pattern)
//=============================================================================

/**
 * WHAT: Test full access query
 * WHY:  Basic query returns entire variable
 */
TEST_F(ContextStoreTest, QueryFull)
{
    const char* text = "The quick brown fox jumps over the lazy dog.";
    rcog_context_store_set_text(store, "sentence", text);

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "sentence", RCOG_ACCESS_FULL, NULL, &result);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_NE(result.data, nullptr);
    EXPECT_GT(result.size, 0u);

    if (result.data) {
        EXPECT_STREQ((const char*)result.data, text);
        rcog_query_result_free(&result);
    }
}

/**
 * WHAT: Test head access query
 * WHY:  Get first N characters
 */
TEST_F(ContextStoreTest, QueryHead)
{
    const char* text = "The quick brown fox jumps over the lazy dog.";
    rcog_context_store_set_text(store, "sentence", text);

    rcog_query_params_t params = {0};
    params.count = 9;  // "The quick"

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "sentence", RCOG_ACCESS_HEAD, &params, &result);
    EXPECT_EQ(err, RCOG_OK);

    if (result.data) {
        EXPECT_EQ(result.size, 9u);
        EXPECT_EQ(strncmp((const char*)result.data, "The quick", 9), 0);
        rcog_query_result_free(&result);
    }
}

/**
 * WHAT: Test tail access query
 * WHY:  Get last N characters
 */
TEST_F(ContextStoreTest, QueryTail)
{
    const char* text = "The quick brown fox jumps over the lazy dog.";
    rcog_context_store_set_text(store, "sentence", text);

    rcog_query_params_t params = {0};
    params.count = 9;  // "lazy dog."

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "sentence", RCOG_ACCESS_TAIL, &params, &result);
    EXPECT_EQ(err, RCOG_OK);

    if (result.data) {
        EXPECT_EQ(result.size, 9u);
        EXPECT_EQ(strncmp((const char*)result.data, "lazy dog.", 9), 0);
        rcog_query_result_free(&result);
    }
}

/**
 * WHAT: Test slice access query
 * WHY:  Get range [start:end]
 */
TEST_F(ContextStoreTest, QuerySlice)
{
    const char* text = "The quick brown fox";
    rcog_context_store_set_text(store, "sentence", text);

    rcog_query_params_t params = {0};
    params.start = 4;   // Start at "quick"
    params.end = 9;     // End before "brown"

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "sentence", RCOG_ACCESS_SLICE, &params, &result);
    EXPECT_EQ(err, RCOG_OK);

    if (result.data) {
        EXPECT_EQ(result.size, 5u);  // "quick"
        EXPECT_EQ(strncmp((const char*)result.data, "quick", 5), 0);
        rcog_query_result_free(&result);
    }
}

/**
 * WHAT: Test search access query
 * WHY:  Find pattern in variable
 */
TEST_F(ContextStoreTest, QuerySearch)
{
    const char* text = "The quick brown fox jumps over the lazy dog.";
    rcog_context_store_set_text(store, "sentence", text);

    rcog_query_params_t params = {0};
    params.pattern = "fox";

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "sentence", RCOG_ACCESS_SEARCH, &params, &result);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_TRUE(result.found);

    rcog_query_result_free(&result);
}

/**
 * WHAT: Test search with no match
 * WHY:  Verify found=false when pattern not found
 */
TEST_F(ContextStoreTest, QuerySearchNoMatch)
{
    const char* text = "The quick brown fox";
    rcog_context_store_set_text(store, "sentence", text);

    rcog_query_params_t params = {0};
    params.pattern = "elephant";

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "sentence", RCOG_ACCESS_SEARCH, &params, &result);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_FALSE(result.found);

    rcog_query_result_free(&result);
}

/**
 * WHAT: Test metadata query
 * WHY:  Get info without data transfer
 */
TEST_F(ContextStoreTest, QueryMetadata)
{
    const char* text = "Sample text for metadata test";
    rcog_context_store_set_text(store, "sample", text);

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "sample", RCOG_ACCESS_METADATA, NULL, &result);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_EQ(result.data, nullptr);  // Metadata only, no data
    EXPECT_TRUE(result.found);

    rcog_query_result_free(&result);
}

/**
 * WHAT: Test query non-existent variable
 * WHY:  Verify proper error handling
 */
TEST_F(ContextStoreTest, QueryNonExistent)
{
    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "nonexistent", RCOG_ACCESS_FULL, NULL, &result);
    EXPECT_EQ(err, RCOG_ERROR_CONTEXT_NOT_FOUND);
}

//=============================================================================
// Metadata Tests
//=============================================================================

/**
 * WHAT: Test getting variable metadata
 * WHY:  Verify metadata is tracked correctly
 */
TEST_F(ContextStoreTest, GetMetadata)
{
    const char* text = "Test data for metadata";
    rcog_context_store_set_text(store, "meta_test", text);

    rcog_variable_metadata_t metadata = {0};
    rcog_error_t err = rcog_context_store_get_metadata(store, "meta_test", &metadata);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_EQ(metadata.dtype, RCOG_DTYPE_TEXT);
    EXPECT_EQ(metadata.size, strlen(text) + 1);  /* set_text stores strlen+1 (includes NUL) */
}

/**
 * WHAT: Test listing all variables
 * WHY:  Verify variable enumeration works
 */
TEST_F(ContextStoreTest, ListVariables)
{
    rcog_context_store_set_text(store, "var_a", "value a");
    rcog_context_store_set_text(store, "var_b", "value b");
    rcog_context_store_set_text(store, "var_c", "value c");

    char* names[10] = {0};
    size_t count = 0;

    rcog_error_t err = rcog_context_store_list(store, names, 10, &count);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_EQ(count, 3u);

    // Clean up allocated names
    for (size_t i = 0; i < count; i++) {
        if (names[i]) {
            free(names[i]);
        }
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test getting store statistics
 * WHY:  Verify statistics tracking works
 */
TEST_F(ContextStoreTest, GetStats)
{
    rcog_context_store_set_text(store, "stats_test", "Some data for stats");

    rcog_query_result_t result = {0};
    rcog_context_store_query(store, "stats_test", RCOG_ACCESS_FULL, NULL, &result);
    rcog_query_result_free(&result);

    rcog_context_store_stats_t stats = {0};
    rcog_error_t err = rcog_context_store_get_stats(store, &stats);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_GE(stats.variable_count, 1u);
    EXPECT_GE(stats.query_count, 1u);
}

/**
 * WHAT: Test reset statistics
 * WHY:  Verify statistics can be cleared
 */
TEST_F(ContextStoreTest, ResetStats)
{
    rcog_context_store_set_text(store, "test", "data");

    rcog_query_result_t result = {0};
    rcog_context_store_query(store, "test", RCOG_ACCESS_FULL, NULL, &result);
    rcog_query_result_free(&result);

    rcog_context_store_reset_stats(store);

    rcog_context_store_stats_t stats = {0};
    rcog_context_store_get_stats(store, &stats);
    EXPECT_EQ(stats.query_count, 0u);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test lock/unlock
 * WHY:  Verify thread safety mechanisms work
 */
TEST_F(ContextStoreTest, LockUnlock)
{
    rcog_error_t err = rcog_context_store_lock(store);
    EXPECT_EQ(err, RCOG_OK);

    // Can still do operations while locked
    rcog_context_store_set_text(store, "locked_var", "locked value");

    err = rcog_context_store_unlock(store);
    EXPECT_EQ(err, RCOG_OK);
}

//=============================================================================
// Custom Configuration Tests
//=============================================================================

/**
 * WHAT: Test with smaller output limit
 * WHY:  Verify output limit is respected
 */
TEST_F(ContextStoreCustomConfigTest, OutputLimitRespected)
{
    // Create text larger than output limit (1024 in custom config)
    char large_text[2048];
    memset(large_text, 'A', sizeof(large_text) - 1);
    large_text[sizeof(large_text) - 1] = '\0';

    rcog_context_store_set_text(store, "large", large_text);

    rcog_query_result_t result = {0};
    rcog_error_t err = rcog_context_store_query(store, "large", RCOG_ACCESS_FULL, NULL, &result);
    EXPECT_EQ(err, RCOG_OK);

    // Result should be truncated to output limit
    EXPECT_LE(result.size, 1024u);
    EXPECT_TRUE(result.truncated);

    rcog_query_result_free(&result);
}

//=============================================================================
// Swarm Sharing Tests
//=============================================================================

/**
 * WHAT: Test setting salience for swarm sharing
 * WHY:  Verify salience tracking works
 */
TEST_F(ContextStoreTest, SetSalience)
{
    rcog_context_store_set_text(store, "salient", "important data");

    rcog_error_t err = rcog_context_store_set_salience(store, "salient", 0.9f);
    EXPECT_EQ(err, RCOG_OK);
}

/**
 * WHAT: Test marking variable as shared
 * WHY:  Verify sharing flag works
 */
TEST_F(ContextStoreTest, SetShared)
{
    rcog_context_store_set_text(store, "shared_var", "shared data");

    rcog_error_t err = rcog_context_store_set_shared(store, "shared_var", true);
    EXPECT_EQ(err, RCOG_OK);

    // Verify we can get shared variables
    char* names[10] = {0};
    size_t count = 0;
    err = rcog_context_store_get_shared(store, names, 10, &count);
    EXPECT_EQ(err, RCOG_OK);
    EXPECT_GE(count, 1u);

    // Clean up
    for (size_t i = 0; i < count; i++) {
        if (names[i]) free(names[i]);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
