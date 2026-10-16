/**
 * @file test_utils_json.cpp
 * @brief Comprehensive unit tests for JSON parser system
 *
 * WHAT: 100% test coverage for nimcp_json.c
 * WHY:  JSON parsing is critical for configuration and serialization - must be bulletproof
 * HOW:  Test all parsing paths, generation, type handling, thread safety, and edge cases
 *
 * TEST COVERAGE:
 * 1. Context creation and destruction
 * 2. Parse valid JSON (objects, arrays, strings, numbers, booleans, null)
 * 3. Parse invalid JSON (syntax errors)
 * 4. Generate JSON from data structures
 * 5. Nested structures (deep paths)
 * 6. Type-specific getters and setters
 * 7. Unicode handling
 * 8. Large JSON documents
 * 9. Thread safety
 * 10. Edge cases (empty, malformed, NULL pointers)
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <fstream>

    #include "utils/json/nimcp_json.h"

//=============================================================================
// Test Fixture
//=============================================================================

class JsonTest : public ::testing::Test {
protected:
    JsonContext* ctx = nullptr;

    void SetUp() override {
        // WHAT: Initialize JSON context before each test
        // WHY:  Ensure clean state for each test
        JsonResult result = nimcp_json_create_context(&ctx);
        ASSERT_EQ(result, JSON_SUCCESS) << "Failed to create JSON context";
        ASSERT_NE(ctx, nullptr) << "Context should not be NULL";
    }

    void TearDown() override {
        // WHAT: Cleanup JSON context after each test
        // WHY:  Prevent memory leaks between tests
        if (ctx) {
            nimcp_json_destroy_context(ctx);
            ctx = nullptr;
        }
    }

    // Helper: Create temporary JSON file
    std::string createTempFile(const std::string& content) {
        std::string filename = "/tmp/nimcp_json_test_" +
                               std::to_string(reinterpret_cast<uintptr_t>(this)) + ".json";
        std::ofstream file(filename);
        file << content;
        file.close();
        return filename;
    }

    // Helper: Delete temporary file
    void deleteTempFile(const std::string& filename) {
        std::remove(filename.c_str());
    }
};

//=============================================================================
// Unit Test 1: Context Creation and Destruction
//=============================================================================

TEST_F(JsonTest, Context_CreateAndDestroy) {
    // WHAT: Verify context creation and destruction
    // WHY:  Core functionality must work
    // HOW:  Create context, verify valid, destroy

    JsonContext* test_ctx = nullptr;
    JsonResult result = nimcp_json_create_context(&test_ctx);

    EXPECT_EQ(result, JSON_SUCCESS) << "Context creation should succeed";
    EXPECT_NE(test_ctx, nullptr) << "Context pointer should be valid";

    // Destroy context (NULL-safe)
    nimcp_json_destroy_context(test_ctx);
    nimcp_json_destroy_context(nullptr);  // Should not crash

    SUCCEED() << "Context creation and destruction work correctly";
}

//=============================================================================
// Unit Test 2: Parse Valid JSON - Objects
//=============================================================================

TEST_F(JsonTest, Parse_ValidJsonObject) {
    // WHAT: Verify parsing of valid JSON object
    // WHY:  Must correctly parse basic object structure
    // HOW:  Load JSON file, query values

    std::string json = R"({
        "name": "NIMCP",
        "version": 1,
        "enabled": true,
        "ratio": 3.14,
        "metadata": null
    })";

    std::string filename = createTempFile(json);
    JsonResult result = nimcp_json_load_file(ctx, filename.c_str(), 0);

    ASSERT_EQ(result, JSON_SUCCESS) << "Should parse valid JSON object";

    // Test string value
    char name[64];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "name", name, sizeof(name)), JSON_SUCCESS);
    EXPECT_STREQ(name, "NIMCP");

    // Test integer value
    int64_t version;
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "version", &version), JSON_SUCCESS);
    EXPECT_EQ(version, 1);

    // Test boolean value
    bool enabled;
    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "enabled", &enabled), JSON_SUCCESS);
    EXPECT_TRUE(enabled);

    // Test number value
    double ratio;
    EXPECT_EQ(nimcp_json_get_number_value(ctx, "ratio", &ratio), JSON_SUCCESS);
    EXPECT_DOUBLE_EQ(ratio, 3.14);

    // Test null value
    bool is_null;
    EXPECT_EQ(nimcp_json_is_null_value(ctx, "metadata", &is_null), JSON_SUCCESS);
    EXPECT_TRUE(is_null);

    deleteTempFile(filename);
    SUCCEED() << "Valid JSON object parsing works correctly";
}

//=============================================================================
// Unit Test 3: Parse Valid JSON - Arrays
//=============================================================================

TEST_F(JsonTest, Parse_ValidJsonArray) {
    // WHAT: Verify parsing of JSON arrays with path-based access
    // WHY:  Must correctly parse and access array elements
    // HOW:  Load JSON with arrays, access by index

    std::string json = R"({
        "numbers": [10, 20, 30],
        "strings": ["apple", "banana", "cherry"],
        "mixed": [42, "text", true, 2.5]
    })";

    std::string filename = createTempFile(json);
    ASSERT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);

    // Access array elements by index
    int64_t num;
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "numbers/0", &num), JSON_SUCCESS);
    EXPECT_EQ(num, 10);

    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "numbers/2", &num), JSON_SUCCESS);
    EXPECT_EQ(num, 30);

    char str[64];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "strings/1", str, sizeof(str)), JSON_SUCCESS);
    EXPECT_STREQ(str, "banana");

    // Mixed array
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "mixed/0", &num), JSON_SUCCESS);
    EXPECT_EQ(num, 42);

    EXPECT_EQ(nimcp_json_get_string_value(ctx, "mixed/1", str, sizeof(str)), JSON_SUCCESS);
    EXPECT_STREQ(str, "text");

    bool flag;
    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "mixed/2", &flag), JSON_SUCCESS);
    EXPECT_TRUE(flag);

    deleteTempFile(filename);
    SUCCEED() << "JSON array parsing and access work correctly";
}

//=============================================================================
// Unit Test 4: Parse Invalid JSON
//=============================================================================

TEST_F(JsonTest, Parse_InvalidJsonSyntax) {
    // WHAT: Verify proper error handling for invalid JSON
    // WHY:  Must detect and report syntax errors
    // HOW:  Try to parse malformed JSON, expect failure

    std::vector<std::string> invalid_jsons = {
        "{invalid}",                    // Missing quotes
        R"({"key": })",                 // Missing value
        R"({"key": "value")",           // Missing closing brace
        R"({"key": "value",})",         // Trailing comma
        R"({key: "value"})",            // Unquoted key
        R"([1, 2, 3,])",                // Trailing comma in array
        R"({"a": "b" "c": "d"})",       // Missing comma
        ""                              // Empty file
    };

    for (size_t i = 0; i < invalid_jsons.size(); i++) {
        std::string filename = createTempFile(invalid_jsons[i]);
        JsonResult result = nimcp_json_load_file(ctx, filename.c_str(), 0);

        EXPECT_NE(result, JSON_SUCCESS)
            << "Should fail on invalid JSON case " << i;

        deleteTempFile(filename);
    }

    SUCCEED() << "Invalid JSON detection works correctly";
}

//=============================================================================
// Unit Test 5: Generate JSON from Data Structures
//=============================================================================

TEST_F(JsonTest, Generate_JsonFromDataStructures) {
    // WHAT: Verify JSON generation by setting values and saving
    // WHY:  Must create valid JSON from programmatic data
    // HOW:  Set various values, save to file, reload and verify

    // Set various types of values
    EXPECT_EQ(nimcp_json_set_string_value(ctx, "name", "TestProject"), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_set_integer_value(ctx, "version", 42), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_set_boolean_value(ctx, "active", true), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_set_number_value(ctx, "pi", 3.14159), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_set_null_value(ctx, "empty"), JSON_SUCCESS);

    // Save to file
    std::string filename = "/tmp/nimcp_json_generated.json";
    JsonResult result = nimcp_json_dump_file(ctx, filename.c_str(), JSON_INDENT(2));
    ASSERT_EQ(result, JSON_SUCCESS) << "Should save JSON to file";

    // Create new context and reload
    JsonContext* verify_ctx = nullptr;
    ASSERT_EQ(nimcp_json_create_context(&verify_ctx), JSON_SUCCESS);
    ASSERT_EQ(nimcp_json_load_file(verify_ctx, filename.c_str(), 0), JSON_SUCCESS);

    // Verify all values
    char name[64];
    EXPECT_EQ(nimcp_json_get_string_value(verify_ctx, "name", name, sizeof(name)), JSON_SUCCESS);
    EXPECT_STREQ(name, "TestProject");

    int64_t version;
    EXPECT_EQ(nimcp_json_get_integer_value(verify_ctx, "version", &version), JSON_SUCCESS);
    EXPECT_EQ(version, 42);

    bool active;
    EXPECT_EQ(nimcp_json_get_boolean_value(verify_ctx, "active", &active), JSON_SUCCESS);
    EXPECT_TRUE(active);

    double pi;
    EXPECT_EQ(nimcp_json_get_number_value(verify_ctx, "pi", &pi), JSON_SUCCESS);
    EXPECT_NEAR(pi, 3.14159, 0.00001);

    bool is_null;
    EXPECT_EQ(nimcp_json_is_null_value(verify_ctx, "empty", &is_null), JSON_SUCCESS);
    EXPECT_TRUE(is_null);

    nimcp_json_destroy_context(verify_ctx);
    deleteTempFile(filename);

    SUCCEED() << "JSON generation from data structures works correctly";
}

//=============================================================================
// Unit Test 6: Nested Structures
//=============================================================================

TEST_F(JsonTest, Nested_DeepPathNavigation) {
    // WHAT: Verify handling of deeply nested JSON structures
    // WHY:  Must support complex hierarchies
    // HOW:  Create nested structure, access via deep paths

    std::string json = R"({
        "server": {
            "host": "localhost",
            "port": 8080,
            "ssl": {
                "enabled": true,
                "cert": "/path/to/cert.pem"
            }
        },
        "database": {
            "connections": [
                {"name": "primary", "url": "db1.example.com"},
                {"name": "replica", "url": "db2.example.com"}
            ]
        }
    })";

    std::string filename = createTempFile(json);
    ASSERT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);

    // Test deep path navigation
    char host[64];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "server/host", host, sizeof(host)), JSON_SUCCESS);
    EXPECT_STREQ(host, "localhost");

    int64_t port;
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "server/port", &port), JSON_SUCCESS);
    EXPECT_EQ(port, 8080);

    bool ssl_enabled;
    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "server/ssl/enabled", &ssl_enabled), JSON_SUCCESS);
    EXPECT_TRUE(ssl_enabled);

    char cert[128];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "server/ssl/cert", cert, sizeof(cert)), JSON_SUCCESS);
    EXPECT_STREQ(cert, "/path/to/cert.pem");

    // Access nested array elements
    char db_name[64];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "database/connections/0/name",
              db_name, sizeof(db_name)), JSON_SUCCESS);
    EXPECT_STREQ(db_name, "primary");

    char db_url[128];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "database/connections/1/url",
              db_url, sizeof(db_url)), JSON_SUCCESS);
    EXPECT_STREQ(db_url, "db2.example.com");

    deleteTempFile(filename);
    SUCCEED() << "Deep nested structure navigation works correctly";
}

//=============================================================================
// Unit Test 7: Type-Specific Getters and Setters
//=============================================================================

TEST_F(JsonTest, TypeSpecific_GettersAndSetters) {
    // WHAT: Verify type-specific functions handle type mismatches correctly
    // WHY:  Must enforce type safety
    // HOW:  Set values of one type, try to read as another

    // Set different types
    EXPECT_EQ(nimcp_json_set_string_value(ctx, "str", "text"), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_set_integer_value(ctx, "num", 123), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_set_boolean_value(ctx, "flag", false), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_set_number_value(ctx, "real", 45.67), JSON_SUCCESS);

    // Try to read with wrong type
    int64_t num_val;
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "str", &num_val), JSON_ERROR_TYPE)
        << "Should fail: string as integer";

    char str_val[64];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "num", str_val, sizeof(str_val)), JSON_ERROR_TYPE)
        << "Should fail: integer as string";

    bool bool_val;
    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "str", &bool_val), JSON_ERROR_TYPE)
        << "Should fail: string as boolean";

    // Note: get_number accepts both integer and real
    double real_val;
    EXPECT_EQ(nimcp_json_get_number_value(ctx, "num", &real_val), JSON_SUCCESS)
        << "Should succeed: integer as number";
    EXPECT_DOUBLE_EQ(real_val, 123.0);

    EXPECT_EQ(nimcp_json_get_number_value(ctx, "real", &real_val), JSON_SUCCESS);
    EXPECT_NEAR(real_val, 45.67, 0.01);

    // NULL value returns special error code
    EXPECT_EQ(nimcp_json_set_null_value(ctx, "null_field"), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "null_field", str_val, sizeof(str_val)),
              JSON_ERROR_NULL_VALUE);
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "null_field", &num_val), JSON_ERROR_NULL_VALUE);
    EXPECT_EQ(nimcp_json_get_boolean_value(ctx, "null_field", &bool_val), JSON_ERROR_NULL_VALUE);
    EXPECT_EQ(nimcp_json_get_number_value(ctx, "null_field", &real_val), JSON_ERROR_NULL_VALUE);

    SUCCEED() << "Type-specific getters and setters enforce type safety";
}

//=============================================================================
// Unit Test 8: Unicode Handling
//=============================================================================

TEST_F(JsonTest, Unicode_HandlesUnicodeStrings) {
    // WHAT: Verify Unicode string handling
    // WHY:  Must support international characters
    // HOW:  Parse and generate JSON with Unicode

    std::string json = R"({
        "english": "Hello World",
        "chinese": "你好世界",
        "japanese": "こんにちは",
        "emoji": "🚀🌟",
        "mixed": "Hello 世界 🌍"
    })";

    std::string filename = createTempFile(json);
    ASSERT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);

    char buffer[256];

    EXPECT_EQ(nimcp_json_get_string_value(ctx, "english", buffer, sizeof(buffer)), JSON_SUCCESS);
    EXPECT_STREQ(buffer, "Hello World");

    EXPECT_EQ(nimcp_json_get_string_value(ctx, "chinese", buffer, sizeof(buffer)), JSON_SUCCESS);
    EXPECT_STREQ(buffer, "你好世界");

    EXPECT_EQ(nimcp_json_get_string_value(ctx, "japanese", buffer, sizeof(buffer)), JSON_SUCCESS);
    EXPECT_STREQ(buffer, "こんにちは");

    EXPECT_EQ(nimcp_json_get_string_value(ctx, "emoji", buffer, sizeof(buffer)), JSON_SUCCESS);
    EXPECT_STREQ(buffer, "🚀🌟");

    EXPECT_EQ(nimcp_json_get_string_value(ctx, "mixed", buffer, sizeof(buffer)), JSON_SUCCESS);
    EXPECT_STREQ(buffer, "Hello 世界 🌍");

    // Test setting Unicode
    EXPECT_EQ(nimcp_json_set_string_value(ctx, "russian", "Привет мир"), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "russian", buffer, sizeof(buffer)), JSON_SUCCESS);
    EXPECT_STREQ(buffer, "Привет мир");

    deleteTempFile(filename);
    SUCCEED() << "Unicode string handling works correctly";
}

//=============================================================================
// Unit Test 9: Large JSON Documents
//=============================================================================

TEST_F(JsonTest, Large_HandlesLargeDocuments) {
    // WHAT: Verify handling of large JSON documents
    // WHY:  Must scale to realistic data sizes
    // HOW:  Create large JSON, parse, query

    // Build large JSON with many fields
    std::string json = "{\n";
    for (int i = 0; i < 1000; i++) {
        json += "  \"field_" + std::to_string(i) + "\": " + std::to_string(i);
        if (i < 999) json += ",";
        json += "\n";
    }
    json += "}";

    std::string filename = createTempFile(json);
    ASSERT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);

    // Query some fields
    int64_t val;
    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "field_0", &val), JSON_SUCCESS);
    EXPECT_EQ(val, 0);

    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "field_500", &val), JSON_SUCCESS);
    EXPECT_EQ(val, 500);

    EXPECT_EQ(nimcp_json_get_integer_value(ctx, "field_999", &val), JSON_SUCCESS);
    EXPECT_EQ(val, 999);

    // Create large nested structure
    std::string nested_json = R"({
        "level1": {
            "level2": {
                "level3": {
                    "level4": {
                        "level5": {
                            "deep_value": 42
                        }
                    }
                }
            }
        }
    })";

    std::string nested_file = createTempFile(nested_json);
    JsonContext* nested_ctx = nullptr;
    ASSERT_EQ(nimcp_json_create_context(&nested_ctx), JSON_SUCCESS);
    ASSERT_EQ(nimcp_json_load_file(nested_ctx, nested_file.c_str(), 0), JSON_SUCCESS);

    EXPECT_EQ(nimcp_json_get_integer_value(nested_ctx,
              "level1/level2/level3/level4/level5/deep_value", &val), JSON_SUCCESS);
    EXPECT_EQ(val, 42);

    nimcp_json_destroy_context(nested_ctx);
    deleteTempFile(filename);
    deleteTempFile(nested_file);

    SUCCEED() << "Large JSON document handling works correctly";
}

//=============================================================================
// Unit Test 10: Thread Safety
//=============================================================================

TEST_F(JsonTest, ThreadSafety_ConcurrentAccess) {
    // WHAT: Verify thread-safe concurrent access
    // WHY:  Must protect against race conditions
    // HOW:  Multiple threads reading/writing same context

    // Setup initial data
    std::string json = R"({
        "counter": 0,
        "text": "initial"
    })";

    std::string filename = createTempFile(json);
    ASSERT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);

    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100;
    std::atomic<int> success_count{0};

    auto reader_worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            char text[64];
            if (nimcp_json_get_string_value(ctx, "text", text, sizeof(text)) == JSON_SUCCESS) {
                success_count++;
            }

            int64_t counter;
            if (nimcp_json_get_integer_value(ctx, "counter", &counter) == JSON_SUCCESS) {
                success_count++;
            }
        }
    };

    auto writer_worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            nimcp_json_set_integer_value(ctx, "counter", i);
            nimcp_json_set_string_value(ctx, "text", "updated");
        }
    };

    std::vector<std::thread> threads;

    // Launch reader threads
    for (int i = 0; i < NUM_THREADS / 2; i++) {
        threads.emplace_back(reader_worker);
    }

    // Launch writer threads
    for (int i = 0; i < NUM_THREADS / 2; i++) {
        threads.emplace_back(writer_worker);
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify no crashes and operations succeeded
    EXPECT_GT(success_count.load(), 0) << "Some operations should succeed";

    deleteTempFile(filename);
    SUCCEED() << "Thread-safe concurrent access works correctly";
}

//=============================================================================
// Unit Test 11: Edge Cases
//=============================================================================

TEST_F(JsonTest, EdgeCases_HandlesEdgeCases) {
    // WHAT: Verify edge case handling
    // WHY:  Must handle unusual inputs gracefully
    // HOW:  Test NULL, empty, malformed, boundary conditions

    // Test 1: NULL parameter validation
    JsonContext* null_ctx = nullptr;
    EXPECT_EQ(nimcp_json_create_context(nullptr), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_load_file(nullptr, "file.json", 0), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_load_file(ctx, nullptr, 0), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_dump_file(nullptr, "file.json", 0), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_dump_file(ctx, nullptr, 0), JSON_ERROR_INVALID_PARAM);

    char buffer[64];
    EXPECT_EQ(nimcp_json_get_string_value(nullptr, "key", buffer, sizeof(buffer)),
              JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_get_string_value(ctx, nullptr, buffer, sizeof(buffer)),
              JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "key", nullptr, sizeof(buffer)),
              JSON_ERROR_INVALID_PARAM);

    // Test 2: Path not found
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "nonexistent", buffer, sizeof(buffer)),
              JSON_ERROR_NOT_FOUND);

    // Test 3: Empty path with set_value (should set root)
    json_t* obj = json_object();
    EXPECT_EQ(nimcp_json_set_value(ctx, "", obj), JSON_SUCCESS);
    json_decref(obj);

    // Test 4: Empty JSON object
    std::string empty_json = "{}";
    std::string filename = createTempFile(empty_json);
    EXPECT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "any", buffer, sizeof(buffer)),
              JSON_ERROR_NOT_FOUND);
    deleteTempFile(filename);

    // Test 5: Empty array
    std::string array_json = R"({"arr": []})";
    filename = createTempFile(array_json);
    EXPECT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "arr/0", buffer, sizeof(buffer)),
              JSON_ERROR_NOT_FOUND);
    deleteTempFile(filename);

    // Test 6: Buffer too small (string truncation)
    EXPECT_EQ(nimcp_json_set_string_value(ctx, "long", "This is a very long string"),
              JSON_SUCCESS);
    char small_buffer[5];
    EXPECT_EQ(nimcp_json_get_string_value(ctx, "long", small_buffer, sizeof(small_buffer)),
              JSON_SUCCESS);
    EXPECT_EQ(strlen(small_buffer), 4u) << "Should truncate to buffer size - 1";

    // Test 7: Invalid array index (negative)
    std::string arr_json = R"({"arr": [1, 2, 3]})";
    filename = createTempFile(arr_json);
    EXPECT_EQ(nimcp_json_load_file(ctx, filename.c_str(), 0), JSON_SUCCESS);
    int64_t val;
    EXPECT_NE(nimcp_json_get_integer_value(ctx, "arr/-1", &val), JSON_SUCCESS);
    deleteTempFile(filename);

    // Test 8: Invalid array index (non-numeric)
    EXPECT_NE(nimcp_json_get_integer_value(ctx, "arr/abc", &val), JSON_SUCCESS);

    // Test 9: Error message retrieval
    const char* err_msg = nimcp_json_get_error(JSON_ERROR_INVALID_PARAM);
    EXPECT_NE(err_msg, nullptr);
    EXPECT_GT(strlen(err_msg), 0u);

    err_msg = nimcp_json_get_error(JSON_ERROR_MEMORY);
    EXPECT_NE(err_msg, nullptr);
    EXPECT_GT(strlen(err_msg), 0u);

    err_msg = nimcp_json_get_error(JSON_SUCCESS);
    EXPECT_NE(err_msg, nullptr);

    // Test 10: NULL destroy is safe
    nimcp_json_destroy_context(nullptr);   // NULL is safe

    // Test 11: File I/O errors
    EXPECT_NE(nimcp_json_load_file(ctx, "/nonexistent/path/file.json", 0), JSON_SUCCESS);
    EXPECT_NE(nimcp_json_dump_file(ctx, "/invalid/path/output.json", 0), JSON_SUCCESS);

    SUCCEED() << "Edge cases handled correctly";
}

//=============================================================================
// Unit Test 12: Get/Set Value with Raw json_t
//=============================================================================

TEST_F(JsonTest, RawValue_GetSetWithJsonT) {
    // WHAT: Verify raw get_value and set_value functions
    // WHY:  Test low-level API for advanced use cases
    // HOW:  Use json_t* directly

    // Create a JSON object
    json_t* obj = json_object();
    json_object_set_new(obj, "key", json_string("value"));

    EXPECT_EQ(nimcp_json_set_value(ctx, "myobj", obj), JSON_SUCCESS);
    json_decref(obj);

    // Get the value back
    json_t* retrieved = nullptr;
    EXPECT_EQ(nimcp_json_get_value(ctx, "myobj", &retrieved), JSON_SUCCESS);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(json_is_object(retrieved));

    // Access nested value
    json_t* key_val = json_object_get(retrieved, "key");
    ASSERT_NE(key_val, nullptr);
    EXPECT_TRUE(json_is_string(key_val));
    EXPECT_STREQ(json_string_value(key_val), "value");

    json_decref(retrieved);

    // Test with NULL value parameter
    EXPECT_EQ(nimcp_json_get_value(ctx, "myobj", nullptr), JSON_ERROR_INVALID_PARAM);
    EXPECT_EQ(nimcp_json_set_value(ctx, "test", nullptr), JSON_ERROR_INVALID_PARAM);

    SUCCEED() << "Raw json_t get/set works correctly";
}

//=============================================================================
// Unit Test 13: Lazy Initialization
//=============================================================================

TEST_F(JsonTest, LazyInit_CreatesRootOnSet) {
    // WHAT: Verify lazy initialization of root on first set
    // WHY:  Convenient API - can set without explicit root creation
    // HOW:  Set value without loading JSON first

    // Create fresh context with no root
    JsonContext* fresh_ctx = nullptr;
    ASSERT_EQ(nimcp_json_create_context(&fresh_ctx), JSON_SUCCESS);

    // Set value should create root object automatically
    EXPECT_EQ(nimcp_json_set_string_value(fresh_ctx, "first", "value"), JSON_SUCCESS);

    // Verify we can read it back
    char buffer[64];
    EXPECT_EQ(nimcp_json_get_string_value(fresh_ctx, "first", buffer, sizeof(buffer)),
              JSON_SUCCESS);
    EXPECT_STREQ(buffer, "value");

    // Can save it
    std::string filename = "/tmp/nimcp_json_lazy.json";
    EXPECT_EQ(nimcp_json_dump_file(fresh_ctx, filename.c_str(), 0), JSON_SUCCESS);

    nimcp_json_destroy_context(fresh_ctx);
    deleteTempFile(filename);

    SUCCEED() << "Lazy initialization works correctly";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
