/**
 * @file test_json.cpp
 * @brief Comprehensive unit tests for nimcp_json utilities
 *
 * WHAT: Tests for JSON parsing, manipulation, and file I/O using Jansson wrapper
 * WHY: Ensure thread-safe JSON operations work correctly with proper error handling
 * HOW: GoogleTest framework with fixture classes and temporary test files
 */

#include <gtest/gtest.h>
#include <jansson.h>
#include <cstring>
#include <fstream>

#define NIMCP_INTERNAL
#include "utils/json/nimcp_json.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture Setup
//=============================================================================

class JsonContextTest : public ::testing::Test {
   protected:
    JsonContext* ctx;

    void SetUp() override
    {
        ctx = nullptr;
        nimcp_memory_init();
        nimcp_memory_enable_tracking(false);  // Disable for cleaner output
    }

    void TearDown() override
    {
        if (ctx) {
            nimcp_json_destroy_context(ctx);
        }
    }

    // Helper to create a temporary JSON file
    void CreateTestJsonFile(const char* filename, const char* content)
    {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    // Helper to cleanup test file
    void RemoveTestFile(const char* filename)
    {
        std::remove(filename);
    }
};

//=============================================================================
// Context Management Tests
//=============================================================================

/**
 * WHAT: Test context creation
 * WHY: Verify basic initialization works
 */
TEST_F(JsonContextTest, CreateContext)
{
    JsonResult result = nimcp_json_create_context(&ctx);
    EXPECT_EQ(result, JSON_SUCCESS);
    ASSERT_NE(ctx, nullptr);
}

/**
 * WHAT: Test context creation with NULL parameter
 * WHY: Verify error handling
 */
TEST_F(JsonContextTest, CreateContextWithNull)
{
    JsonResult result = nimcp_json_create_context(nullptr);
    EXPECT_EQ(result, JSON_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test context destruction
 * WHY: Verify cleanup works without memory leaks
 */
TEST_F(JsonContextTest, DestroyContext)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    nimcp_json_destroy_context(ctx);
    ctx = nullptr;  // Prevent double-free in TearDown

    // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test destroying NULL context
 * WHY: Verify NULL safety
 */
TEST_F(JsonContextTest, DestroyNullContext)
{
    nimcp_json_destroy_context(nullptr);
    SUCCEED();
}

//=============================================================================
// File I/O Tests
//=============================================================================

class JsonFileTest : public JsonContextTest {
   protected:
    const char* test_file = "test_json_temp.json";
};

/**
 * WHAT: Test loading JSON from file
 * WHY: Verify file parsing works
 */
TEST_F(JsonFileTest, LoadValidJsonFile)
{
    CreateTestJsonFile(test_file, R"(
        {
            "name": "test",
            "value": 42,
            "enabled": true
        }
    )");

    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    JsonResult result = nimcp_json_load_file(ctx, test_file, 0);
    EXPECT_EQ(result, JSON_SUCCESS);

    RemoveTestFile(test_file);
}

/**
 * WHAT: Test loading invalid JSON
 * WHY: Verify parse error handling
 */
TEST_F(JsonFileTest, LoadInvalidJsonFile)
{
    CreateTestJsonFile(test_file, "{ invalid json }");

    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    JsonResult result = nimcp_json_load_file(ctx, test_file, 0);
    EXPECT_EQ(result, JSON_ERROR_PARSE);

    RemoveTestFile(test_file);
}

/**
 * WHAT: Test loading non-existent file
 * WHY: Verify file error handling
 */
TEST_F(JsonFileTest, LoadNonexistentFile)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    JsonResult result = nimcp_json_load_file(ctx, "nonexistent_file.json", 0);
    EXPECT_EQ(result, JSON_ERROR_PARSE);
}

/**
 * WHAT: Test dumping JSON to file
 * WHY: Verify file writing works
 */
TEST_F(JsonFileTest, DumpJsonToFile)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    // Set some values
    nimcp_json_set_string_value(ctx, "name", "test");
    nimcp_json_set_integer_value(ctx, "value", 42);

    JsonResult result = nimcp_json_dump_file(ctx, test_file, JSON_INDENT(2));
    EXPECT_EQ(result, JSON_SUCCESS);

    // Verify file exists and contains JSON
    std::ifstream file(test_file);
    EXPECT_TRUE(file.good());
    file.close();

    RemoveTestFile(test_file);
}

/**
 * WHAT: Test dumping with NULL context
 * WHY: Verify error handling
 */
TEST_F(JsonFileTest, DumpWithNullContext)
{
    JsonResult result = nimcp_json_dump_file(nullptr, test_file, 0);
    EXPECT_EQ(result, JSON_ERROR_INVALID_PARAM);
}

//=============================================================================
// Value Access Tests
//=============================================================================

class JsonValueTest : public JsonContextTest {};

/**
 * WHAT: Test getting string values
 * WHY: Verify string extraction works
 */
TEST_F(JsonValueTest, GetStringValue)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    // Set a string value
    nimcp_json_set_string_value(ctx, "name", "TestValue");

    // Retrieve it
    char buffer[256];
    JsonResult result = nimcp_json_get_string_value(ctx, "name", buffer, sizeof(buffer));
    EXPECT_EQ(result, JSON_SUCCESS);
    EXPECT_STREQ(buffer, "TestValue");
}

/**
 * WHAT: Test getting integer values
 * WHY: Verify integer extraction works
 */
TEST_F(JsonValueTest, GetIntegerValue)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    nimcp_json_set_integer_value(ctx, "count", 12345);

    int64_t value = 0;
    JsonResult result = nimcp_json_get_integer_value(ctx, "count", &value);
    EXPECT_EQ(result, JSON_SUCCESS);
    EXPECT_EQ(value, 12345);
}

/**
 * WHAT: Test getting boolean values
 * WHY: Verify boolean extraction works
 */
TEST_F(JsonValueTest, GetBooleanValue)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    nimcp_json_set_boolean_value(ctx, "enabled", true);
    nimcp_json_set_boolean_value(ctx, "disabled", false);

    bool value1 = false, value2 = true;
    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "enabled", &value1), JSON_SUCCESS);
    EXPECT_TRUE(value1);

    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "disabled", &value2), JSON_SUCCESS);
    EXPECT_FALSE(value2);
}

/**
 * WHAT: Test getting number (double) values
 * WHY: Verify floating-point extraction works
 */
TEST_F(JsonValueTest, GetNumberValue)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    nimcp_json_set_number_value(ctx, "pi", 3.14159);

    double value = 0.0;
    JsonResult result = nimcp_json_get_number_value(ctx, "pi", &value);
    EXPECT_EQ(result, JSON_SUCCESS);
    EXPECT_DOUBLE_EQ(value, 3.14159);
}

/**
 * WHAT: Test getting non-existent value
 * WHY: Verify NOT_FOUND error
 */
TEST_F(JsonValueTest, GetNonexistentValue)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    int64_t value = 0;
    JsonResult result = nimcp_json_get_integer_value(ctx, "nonexistent", &value);
    EXPECT_EQ(result, JSON_ERROR_NOT_FOUND);
}

/**
 * WHAT: Test type mismatch errors
 * WHY: Verify type checking works
 */
TEST_F(JsonValueTest, TypeMismatch)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    nimcp_json_set_string_value(ctx, "name", "test");

    // Try to read as integer
    int64_t value = 0;
    JsonResult result = nimcp_json_get_integer_value(ctx, "name", &value);
    EXPECT_EQ(result, JSON_ERROR_TYPE);
}

//=============================================================================
// Path-Based Access Tests
//=============================================================================

class JsonPathTest : public JsonContextTest {};

/**
 * WHAT: Test nested path access
 * WHY: Verify path resolution works for nested objects
 */
TEST_F(JsonPathTest, NestedPathAccess)
{
    const char* json_content = R"(
        {
            "server": {
                "host": "localhost",
                "port": 8080
            }
        }
    )";

    const char* test_file = "test_nested.json";
    CreateTestJsonFile(test_file, json_content);

    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);
    ASSERT_EQ(nimcp_json_load_file(ctx, test_file, 0), JSON_SUCCESS);

    // Access nested values
    char host[256];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "server/host", host, sizeof(host)), JSON_SUCCESS);
    EXPECT_STREQ(host, "localhost");

    int64_t port = 0;
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "server/port", &port), JSON_SUCCESS);
    EXPECT_EQ(port, 8080);

    RemoveTestFile(test_file);
}

/**
 * WHAT: Test setting nested values
 * WHY: Verify path-based setting works
 */
TEST_F(JsonPathTest, SetNestedValue)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    // Create nested structure manually
    json_t* root = json_object();
    json_t* server = json_object();
    json_object_set_new(root, "server", server);

    // Set root in context
    nimcp_json_set_value(ctx, "", root);

    // Now set nested value
    JsonResult result = nimcp_json_set_string_value(ctx, "server/host", "127.0.0.1");
    EXPECT_EQ(result, JSON_SUCCESS);

    // Verify it was set
    char host[256];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "server/host", host, sizeof(host)), JSON_SUCCESS);
    EXPECT_STREQ(host, "127.0.0.1");

    json_decref(root);
}

/**
 * WHAT: Test invalid path
 * WHY: Verify error handling for malformed paths
 */
TEST_F(JsonPathTest, InvalidPath)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    int64_t value = 0;
    JsonResult result = nimcp_json_get_integer_value(ctx, "path/to/nowhere", &value);
    EXPECT_EQ(result, JSON_ERROR_NOT_FOUND);
}

//=============================================================================
// Null Value Tests
//=============================================================================

class JsonNullTest : public JsonContextTest {};

/**
 * WHAT: Test checking for null values
 * WHY: Verify null detection works
 */
TEST_F(JsonNullTest, CheckNullValue)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    // Set a null value
    nimcp_json_set_null_value(ctx, "nullfield");

    // Check if it's null
    bool is_null = false;
    JsonResult result = nimcp_json_is_null_value(ctx, "nullfield", &is_null);
    EXPECT_EQ(result, JSON_SUCCESS);
    EXPECT_TRUE(is_null);
}

/**
 * WHAT: Test null vs non-existent
 * WHY: Verify distinction between null and missing
 */
TEST_F(JsonNullTest, NullVsNonexistent)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    nimcp_json_set_null_value(ctx, "nullfield");

    // Null field exists and is null
    bool is_null = false;
    EXPECT_EQ(nimcp_json_is_null_value(ctx, "nullfield", &is_null), JSON_SUCCESS);
    EXPECT_TRUE(is_null);

    // Non-existent field returns NOT_FOUND
    is_null = false;
    EXPECT_EQ(nimcp_json_is_null_value(ctx, "nonexistent", &is_null), JSON_ERROR_NOT_FOUND);
}

/**
 * WHAT: Test getting null value as string
 * WHY: Verify type error for null values
 */
TEST_F(JsonNullTest, GetNullAsString)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    nimcp_json_set_null_value(ctx, "nullfield");

    char buffer[256];
    JsonResult result = nimcp_json_get_string_value(ctx, "nullfield", buffer, sizeof(buffer));
    EXPECT_EQ(result, JSON_ERROR_NULL_VALUE);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

class JsonErrorTest : public JsonContextTest {};

/**
 * WHAT: Test error message retrieval
 * WHY: Verify error descriptions are available
 */
TEST_F(JsonErrorTest, GetErrorMessages)
{
    EXPECT_STREQ(nimcp_json_get_error(JSON_SUCCESS), "Success");
    EXPECT_STREQ(nimcp_json_get_error(JSON_ERROR_INVALID_PARAM), "Invalid parameter");
    EXPECT_STREQ(nimcp_json_get_error(JSON_ERROR_MEMORY), "Memory allocation failed");
    EXPECT_STREQ(nimcp_json_get_error(JSON_ERROR_PARSE), "JSON parsing failed");
    EXPECT_STREQ(nimcp_json_get_error(JSON_ERROR_FILE), "File operation failed");
    EXPECT_STREQ(nimcp_json_get_error(JSON_ERROR_TYPE), "Type mismatch");
    EXPECT_STREQ(nimcp_json_get_error(JSON_ERROR_NOT_FOUND), "Path not found");
    EXPECT_STREQ(nimcp_json_get_error(JSON_ERROR_NULL_VALUE), "JSON null value");
}

/**
 * WHAT: Test NULL parameter handling across all functions
 * WHY: Verify comprehensive NULL safety
 */
TEST_F(JsonErrorTest, NullParameterHandling)
{
    char buffer[256];
    int64_t int_val;
    double num_val;
    bool bool_val;
    json_t* json_val;

    // All these should return INVALID_PARAM
    EXPECT_EQ(nimcp_json_load_file(nullptr, "file.json", 0), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_dump_file(nullptr, "file.json", 0), JSON_ERROR_INVALID_PARAM);

    EXPECT_EQ(nimcp_json_get_string_value(nullptr, "key", buffer, 256), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_get_integer_value(nullptr, "key", &int_val), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_get_boolean_value(nullptr, "key", &bool_val), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_get_number_value(nullptr, "key", &num_val), JSON_ERROR_INVALID_PARAM);

    EXPECT_EQ(nimcp_json_set_string_value(nullptr, "key", "value"), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_set_integer_value(nullptr, "key", 42), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_set_boolean_value(nullptr, "key", true), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_set_number_value(nullptr, "key", 3.14), JSON_ERROR_INVALID_PARAM);

    EXPECT_EQ(nimcp_json_is_null_value(nullptr, "key", &bool_val), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_set_null_value(nullptr, "key"), JSON_ERROR_INVALID_PARAM);

    EXPECT_EQ(nimcp_json_get_value(nullptr, "key", &json_val), JSON_ERROR_INVALID_PARAM);
    json_t* temp_obj = json_object();
    EXPECT_EQ(nimcp_json_set_value(nullptr, "key", temp_obj), JSON_ERROR_INVALID_PARAM);
    json_decref(temp_obj);  // Free the object to prevent memory leak
}

//=============================================================================
// Integration Tests
//=============================================================================

class JsonIntegrationTest : public JsonContextTest {};

/**
 * WHAT: Test complete workflow: load, modify, save
 * WHY: Verify real-world usage scenario
 */
TEST_F(JsonIntegrationTest, LoadModifySave)
{
    const char* input_file = "test_input.json";
    const char* output_file = "test_output.json";

    // Create initial JSON file
    CreateTestJsonFile(input_file, R"(
        {
            "version": 1,
            "name": "original",
            "count": 100
        }
    )");

    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    // Load
    ASSERT_EQ(nimcp_json_load_file(ctx, input_file, 0), JSON_SUCCESS);

    // Modify
    nimcp_json_set_string_value(ctx, "name", "modified");
    nimcp_json_set_integer_value(ctx, "count", 200);
    nimcp_json_set_boolean_value(ctx, "enabled", true);

    // Save
    EXPECT_EQ(nimcp_json_dump_file(ctx, output_file, JSON_INDENT(2)), JSON_SUCCESS);

    // Verify by loading again
    JsonContext* verify_ctx = nullptr;
    ASSERT_EQ(nimcp_json_create_context(&verify_ctx), JSON_SUCCESS);
    ASSERT_EQ(nimcp_json_load_file(verify_ctx, output_file, 0), JSON_SUCCESS);

    char name[256];
    int64_t count;
    bool enabled;

    EXPECT_EQ(nimcp_json_get_string_value(verify_ctx, "name", name, sizeof(name)), JSON_SUCCESS);
    EXPECT_STREQ(name, "modified");

    EXPECT_EQ(nimcp_json_get_integer_value(verify_ctx, "count", &count), JSON_SUCCESS);
    EXPECT_EQ(count, 200);

    EXPECT_EQ(nimcp_json_get_boolean_value(verify_ctx, "enabled", &enabled), JSON_SUCCESS);
    EXPECT_TRUE(enabled);

    nimcp_json_destroy_context(verify_ctx);
    RemoveTestFile(input_file);
    RemoveTestFile(output_file);
}

/**
 * WHAT: Test complex nested structure
 * WHY: Verify handling of deeply nested JSON
 */
TEST_F(JsonIntegrationTest, ComplexNestedStructure)
{
    const char* complex_json = "{"
                               "\"application\": {"
                               "\"name\": \"TestApp\","
                               "\"version\": \"1.0.0\","
                               "\"config\": {"
                               "\"database\": {"
                               "\"host\": \"localhost\","
                               "\"port\": 5432,"
                               "\"credentials\": {"
                               "\"username\": \"admin\","
                               "\"password\": \"secret\""
                               "}"
                               "},"
                               "\"logging\": {"
                               "\"level\": \"debug\","
                               "\"enabled\": true"
                               "}"
                               "}"
                               "}"
                               "}";

    const char* test_file = "test_complex.json";
    CreateTestJsonFile(test_file, complex_json);

    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);
    ASSERT_EQ(nimcp_json_load_file(ctx, test_file, 0), JSON_SUCCESS);

    // Access deeply nested values
    char host[256];
    int64_t port;
    bool enabled;

    EXPECT_EQ(
        nimcp_json_get_string_value(ctx, "application/config/database/host", host, sizeof(host)),
        JSON_SUCCESS);
    EXPECT_STREQ(host, "localhost");

    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "application/config/database/port", &port),
              JSON_SUCCESS);
    EXPECT_EQ(port, 5432);

    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "application/config/logging/enabled", &enabled),
              JSON_SUCCESS);
    EXPECT_TRUE(enabled);

    RemoveTestFile(test_file);
}

/**
 * WHAT: Test building JSON from scratch
 * WHY: Verify creating JSON without loading from file
 */
TEST_F(JsonIntegrationTest, BuildJsonFromScratch)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    // Build structure
    nimcp_json_set_string_value(ctx, "name", "BuildTest");
    nimcp_json_set_integer_value(ctx, "version", 2);
    nimcp_json_set_boolean_value(ctx, "active", true);
    nimcp_json_set_number_value(ctx, "rating", 4.5);

    // Save and verify
    const char* output_file = "test_built.json";
    EXPECT_EQ(nimcp_json_dump_file(ctx, output_file, JSON_INDENT(2)), JSON_SUCCESS);

    // Load back and verify
    JsonContext* verify_ctx = nullptr;
    ASSERT_EQ(nimcp_json_create_context(&verify_ctx), JSON_SUCCESS);
    ASSERT_EQ(nimcp_json_load_file(verify_ctx, output_file, 0), JSON_SUCCESS);

    char name[256];
    int64_t version;
    bool active;
    double rating;

    EXPECT_EQ(nimcp_json_get_string_value(verify_ctx, "name", name, sizeof(name)), JSON_SUCCESS);
    EXPECT_STREQ(name, "BuildTest");

    EXPECT_EQ(nimcp_json_get_integer_value(verify_ctx, "version", &version), JSON_SUCCESS);
    EXPECT_EQ(version, 2);

    EXPECT_EQ(nimcp_json_get_boolean_value(verify_ctx, "active", &active), JSON_SUCCESS);
    EXPECT_TRUE(active);

    EXPECT_EQ(nimcp_json_get_number_value(verify_ctx, "rating", &rating), JSON_SUCCESS);
    EXPECT_DOUBLE_EQ(rating, 4.5);

    nimcp_json_destroy_context(verify_ctx);
    RemoveTestFile(output_file);
}

/**
 * WHAT: Test handling of various data types
 * WHY: Verify all JSON types are supported
 */
TEST_F(JsonIntegrationTest, AllDataTypes)
{
    ASSERT_EQ(nimcp_json_create_context(&ctx), JSON_SUCCESS);

    // Set various types
    nimcp_json_set_string_value(ctx, "str", "text");
    nimcp_json_set_integer_value(ctx, "int", -42);
    nimcp_json_set_number_value(ctx, "num", 3.14159);
    nimcp_json_set_boolean_value(ctx, "bool_true", true);
    nimcp_json_set_boolean_value(ctx, "bool_false", false);
    nimcp_json_set_null_value(ctx, "null_val");

    // Verify all types
    char str[256];
    int64_t int_val;
    double num_val;
    bool bool_val;
    bool is_null;

    EXPECT_EQ(nimcp_json_get_string_value(ctx, "str", str, sizeof(str)), JSON_SUCCESS);
    EXPECT_STREQ(str, "text");

    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "int", &int_val), JSON_SUCCESS);
    EXPECT_EQ(int_val, -42);

    EXPECT_EQ(nimcp_json_get_number_value(ctx, "num", &num_val), JSON_SUCCESS);
    EXPECT_DOUBLE_EQ(num_val, 3.14159);

    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "bool_true", &bool_val), JSON_SUCCESS);
    EXPECT_TRUE(bool_val);

    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "bool_false", &bool_val), JSON_SUCCESS);
    EXPECT_FALSE(bool_val);

    EXPECT_EQ(nimcp_json_is_null_value(ctx, "null_val", &is_null), JSON_SUCCESS);
    EXPECT_TRUE(is_null);
}
