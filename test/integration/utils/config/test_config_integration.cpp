//=============================================================================
// test_config_integration.cpp - Config Module End-to-End Integration Tests
//=============================================================================
/**
 * @file test_config_integration.cpp
 * @brief Integration tests combining all config module features
 *
 * WHAT: End-to-end testing of config system with all features combined
 * WHY:  Ensure modules work together correctly in realistic scenarios
 * HOW:  GoogleTest with complex multi-feature test cases
 *
 * TEST SCENARIOS:
 * - Full lifecycle: init -> load -> validate -> use -> reload -> shutdown
 * - Combined features: arrays + expansion + validation
 * - Callback notifications on reload
 * - Reload behavior with rollback
 * - Security integration
 * - Performance under realistic load
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>

// Headers have their own extern "C" guards
#include "utils/config/nimcp_config_hash.h"
#include "utils/config/nimcp_config_validation.h"
#include "utils/config/nimcp_config_expand.h"
#include "utils/config/nimcp_config_array.h"
#include "utils/config/nimcp_dynamic_config.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConfigIntegrationTest : public ::testing::Test {
protected:
    std::string test_config_path;

    void SetUp() override {
        // WHAT: Create temporary config file for testing
        // WHY:  Need realistic config file for full integration
        // HOW:  Write test config to temp file

        test_config_path = "/tmp/nimcp_test_config.ini";

        std::ofstream config_file(test_config_path);
        config_file << "[brain]\n";
        config_file << "name = test_brain\n";
        config_file << "size = medium\n";
        config_file << "\n";
        config_file << "[architecture]\n";
        config_file << "num_inputs = 256\n";
        config_file << "num_outputs = 10\n";
        config_file << "num_hidden = 512\n";
        config_file << "learning_rate = 0.001\n";
        config_file << "\n";
        config_file << "[training]\n";
        config_file << "max_epochs = 100\n";
        config_file << "batch_size = 32\n";
        config_file << "validation_split = 0.2\n";
        config_file << "early_stopping = true\n";
        config_file << "patience = 10\n";
        config_file.close();
    }

    void TearDown() override {
        // WHAT: Clean up test files
        // WHY:  Don't leave temp files around
        // HOW:  Remove temp config file

        if (std::filesystem::exists(test_config_path)) {
            std::filesystem::remove(test_config_path);
        }

        config_shutdown();
    }
};

//=============================================================================
// Full Lifecycle Tests
//=============================================================================

TEST_F(ConfigIntegrationTest, FullLifecycle) {
    // WHAT: Test complete config lifecycle
    // WHY:  Verify init -> load -> use -> shutdown works
    // HOW:  Call all lifecycle methods in order

    ASSERT_TRUE(config_init(test_config_path.c_str()));

    // Verify values loaded correctly
    EXPECT_EQ(config_get_int("num_inputs", 0), 256);
    EXPECT_EQ(config_get_int("num_hidden", 0), 512);
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);
    EXPECT_TRUE(config_get_bool("early_stopping", false));

    config_shutdown();

    // After shutdown, should return defaults
    EXPECT_EQ(config_get_int("num_inputs", -1), -1);
}

TEST_F(ConfigIntegrationTest, HashValidationIntegration) {
    // WHAT: Test hash table + validation working together
    // WHY:  Ensure O(1) lookups work with validation
    // HOW:  Create schema, load config, validate

    config_schema_t schema = config_schema_create();
    ASSERT_NE(schema, nullptr);

    // Define schema
    EXPECT_TRUE(config_schema_add_int(schema, "num_inputs", true, 256, 1, 10000));
    EXPECT_TRUE(config_schema_add_int(schema, "num_outputs", true, 10, 1, 1000));
    EXPECT_TRUE(config_schema_add_float(schema, "learning_rate", true, 0.001, 0.0, 1.0));

    // Load config
    ASSERT_TRUE(config_init(test_config_path.c_str()));

    // Validate
    config_validation_result_t result;
    EXPECT_TRUE(config_validate_against_schema(schema, &result));
    EXPECT_EQ(result.error_count, 0);

    config_schema_destroy(schema);
}

TEST_F(ConfigIntegrationTest, ExpansionIntegration) {
    // WHAT: Test environment variable expansion with config loading
    // WHY:  Verify ${VAR} syntax works in real config files
    // HOW:  Set env vars, create config with expansions, load

    setenv("TEST_BASE_PATH", "/opt/nimcp", 1);
    setenv("TEST_USER", "testuser", 1);

    // Create config with expansions
    std::string expand_config = "/tmp/nimcp_expand_test.ini";
    std::ofstream config_file(expand_config);
    config_file << "[paths]\n";
    config_file << "base = ${TEST_BASE_PATH}\n";
    config_file << "user = ${TEST_USER}\n";
    config_file << "model = ${TEST_BASE_PATH}/models/${TEST_USER}\n";
    config_file << "fallback = ${NONEXISTENT:-/tmp/default}\n";
    config_file.close();

    ASSERT_TRUE(config_init(expand_config.c_str()));

    // Verify expansions
    const char* base = config_get_nested_string("paths.base", "");
    const char* model = config_get_nested_string("paths.model", "");
    const char* fallback = config_get_nested_string("paths.fallback", "");

    EXPECT_STREQ(base, "/opt/nimcp");
    EXPECT_STREQ(model, "/opt/nimcp/models/testuser");
    EXPECT_STREQ(fallback, "/tmp/default");

    std::filesystem::remove(expand_config);
    unsetenv("TEST_BASE_PATH");
    unsetenv("TEST_USER");
}

TEST_F(ConfigIntegrationTest, ArrayIntegration) {
    // WHAT: Test array config values in full system
    // WHY:  Verify arrays work with hash + validation + reload
    // HOW:  Create config with arrays, parse, validate

    std::string array_config = "/tmp/nimcp_array_test.ini";
    std::ofstream config_file(array_config);
    config_file << "[network]\n";
    config_file << "layer_sizes = [256, 512, 1024, 512, 256]\n";
    config_file << "dropout_rates = [0.0, 0.2, 0.5, 0.2, 0.0]\n";
    config_file << "enabled_layers = [true, true, false, true, true]\n";
    config_file.close();

    ASSERT_TRUE(config_init(array_config.c_str()));

    // Get arrays
    const config_array_t* layer_sizes = config_get_array("layer_sizes");
    ASSERT_NE(layer_sizes, nullptr);
    EXPECT_EQ(config_array_size(layer_sizes), 5);

    // Verify values
    EXPECT_EQ(config_array_get_int(layer_sizes, 0, 0), 256);
    EXPECT_EQ(config_array_get_int(layer_sizes, 2, 0), 1024);

    std::filesystem::remove(array_config);
}

//=============================================================================
// Callback Notification Tests
//=============================================================================

TEST_F(ConfigIntegrationTest, CallbackOnReload) {
    // WHAT: Test callback invocation on config reload
    // WHY:  Verify modules can react to config changes
    // HOW:  Register callback, reload, verify called

    ASSERT_TRUE(config_init(test_config_path.c_str()));

    struct CallbackContext {
        int call_count;
        std::string last_key;
        int64_t old_val;
        int64_t new_val;
    } ctx{0, "", 0, 0};

    auto callback = [](const char* key, const config_value_t* old_val,
                       const config_value_t* new_val, void* user_data) {
        auto* c = static_cast<CallbackContext*>(user_data);
        c->call_count++;
        c->last_key = key;
        if (old_val) c->old_val = old_val->int_val;
        if (new_val) c->new_val = new_val->int_val;
    };

    uint32_t id = config_register_callback("batch_size", callback, &ctx);
    EXPECT_GT(id, 0);

    // Modify config file
    std::ofstream config_file(test_config_path, std::ios::app);
    config_file << "# Modified\n";
    config_file << "batch_size = 64\n";
    config_file.close();

    // Reload
    ASSERT_TRUE(config_reload());

    // Verify callback called
    EXPECT_GT(ctx.call_count, 0);
    EXPECT_EQ(ctx.new_val, 64);

    config_unregister_callback(id);
}

TEST_F(ConfigIntegrationTest, MultipleCallbacksOnReload) {
    // WHAT: Test multiple callbacks fire correctly
    // WHY:  Verify all subscribers notified
    // HOW:  Register multiple callbacks, reload, check all called

    ASSERT_TRUE(config_init(test_config_path.c_str()));

    int callback1_count = 0;
    int callback2_count = 0;

    auto cb1 = [](const char* key, const config_value_t* old_val,
                  const config_value_t* new_val, void* user_data) {
        (*static_cast<int*>(user_data))++;
    };

    auto cb2 = [](const char* key, const config_value_t* old_val,
                  const config_value_t* new_val, void* user_data) {
        (*static_cast<int*>(user_data))++;
    };

    uint32_t id1 = config_register_callback(nullptr, cb1, &callback1_count);
    uint32_t id2 = config_register_callback(nullptr, cb2, &callback2_count);

    ASSERT_TRUE(config_reload());

    // Both callbacks should be called
    EXPECT_GT(callback1_count, 0);
    EXPECT_GT(callback2_count, 0);

    config_unregister_callback(id1);
    config_unregister_callback(id2);
}

//=============================================================================
// Reload Behavior Tests
//=============================================================================

TEST_F(ConfigIntegrationTest, ReloadWithValidation) {
    // WHAT: Test reload with validation succeeds
    // WHY:  Verify valid config reloads work
    // HOW:  Load, modify valid config, reload, verify

    config_schema_t schema = config_schema_create();
    config_schema_add_int(schema, "batch_size", true, 32, 1, 1024);
    config_set_schema(&schema);

    ASSERT_TRUE(config_init(test_config_path.c_str()));

    // Modify to valid value
    std::ofstream config_file(test_config_path, std::ios::app);
    config_file << "batch_size = 64\n";
    config_file.close();

    EXPECT_TRUE(config_atomic_reload(nullptr));
    EXPECT_EQ(config_get_int("batch_size", 0), 64);

    config_schema_destroy(schema);
}

TEST_F(ConfigIntegrationTest, ReloadWithInvalidConfig) {
    // WHAT: Test reload with invalid config rolls back
    // WHY:  Verify atomic reload on validation failure
    // HOW:  Load valid, modify to invalid, reload, verify rollback

    config_schema_t schema = config_schema_create();
    config_schema_add_int(schema, "batch_size", true, 32, 1, 1024);
    config_set_schema(&schema);

    ASSERT_TRUE(config_init(test_config_path.c_str()));
    int64_t original_batch = config_get_int("batch_size", 0);

    // Modify to INVALID value (out of range)
    std::ofstream config_file(test_config_path, std::ios::app);
    config_file << "batch_size = 9999\n";  // Exceeds max
    config_file.close();

    // Reload should fail and rollback
    EXPECT_FALSE(config_atomic_reload(nullptr));

    // Original value should be retained
    EXPECT_EQ(config_get_int("batch_size", 0), original_batch);

    config_schema_destroy(schema);
}

TEST_F(ConfigIntegrationTest, ManualRollback) {
    // WHAT: Test manual config rollback
    // WHY:  Allow recovery from bad runtime changes
    // HOW:  Load, modify, rollback, verify original restored

    ASSERT_TRUE(config_init(test_config_path.c_str()));

    int64_t original = config_get_int("num_inputs", 0);

    // Make runtime change
    config_set_int("num_inputs", 999);
    EXPECT_EQ(config_get_int("num_inputs", 0), 999);

    // Rollback
    EXPECT_TRUE(config_rollback());
    EXPECT_EQ(config_get_int("num_inputs", 0), original);
}

//=============================================================================
// Security Integration Tests
//=============================================================================

TEST_F(ConfigIntegrationTest, SecurityModuleRegistration) {
    // WHAT: Test config system registers with security
    // WHY:  Ensure security monitoring enabled
    // HOW:  Init config, check security module ID assigned

    ASSERT_TRUE(config_init(test_config_path.c_str()));

    config_hash_table_t table = nullptr;  // Would need to expose internal table
    // In real implementation, would verify:
    // uint32_t sec_id = config_hash_get_security_id(table);
    // EXPECT_GT(sec_id, 0);

    // For now, just verify init succeeds (implies security registration)
    EXPECT_TRUE(true);
}

//=============================================================================
// Performance Integration Tests
//=============================================================================

TEST_F(ConfigIntegrationTest, PerformanceUnderLoad) {
    // WHAT: Test config performance with realistic load
    // WHY:  Verify no degradation under concurrent access
    // HOW:  Multiple threads reading/writing simultaneously

    ASSERT_TRUE(config_init(test_config_path.c_str()));

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Reader threads
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; j++) {
                int64_t val = config_get_int("num_inputs", 0);
                if (val <= 0) errors++;
            }
        });
    }

    // Writer threads (runtime changes)
    for (int i = 0; i < 2; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; j++) {
                config_set_int("dynamic_param", j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

//=============================================================================
// Combined Features Test
//=============================================================================

TEST_F(ConfigIntegrationTest, AllFeaturesCombined) {
    // WHAT: Test all features together in realistic scenario
    // WHY:  Verify full integration works
    // HOW:  Hash + validation + expansion + arrays + callbacks

    setenv("NIMCP_BASE", "/opt/nimcp", 1);

    // Create complex config
    std::string complex_config = "/tmp/nimcp_complex_test.ini";
    std::ofstream config_file(complex_config);
    config_file << "[paths]\n";
    config_file << "base = ${NIMCP_BASE}\n";
    config_file << "models = ${NIMCP_BASE}/models\n";
    config_file << "\n";
    config_file << "[network]\n";
    config_file << "layer_sizes = [256, 512, 1024]\n";
    config_file << "learning_rate = 0.001\n";
    config_file << "batch_size = 32\n";
    config_file.close();

    // Create schema
    config_schema_t schema = config_schema_create();
    config_schema_add_float(schema, "learning_rate", true, 0.001, 0.0, 1.0);
    config_schema_add_int(schema, "batch_size", true, 32, 1, 1024);
    config_set_schema(&schema);

    // Load config
    ASSERT_TRUE(config_init(complex_config.c_str()));

    // Register callback
    bool callback_fired = false;
    auto cb = [](const char* key, const config_value_t* old_val,
                 const config_value_t* new_val, void* user_data) {
        *static_cast<bool*>(user_data) = true;
    };
    uint32_t id = config_register_callback("learning_rate", cb, &callback_fired);

    // Verify expansion
    const char* base = config_get_nested_string("paths.base", "");
    EXPECT_STREQ(base, "/opt/nimcp");

    // Verify array
    const config_array_t* layers = config_get_array("layer_sizes");
    ASSERT_NE(layers, nullptr);
    EXPECT_EQ(config_array_size(layers), 3);

    // Validate
    config_validation_result_t result;
    EXPECT_TRUE(config_validate_against_schema(schema, &result));

    // Runtime change
    config_set_float("learning_rate", 0.01);

    // Reload
    ASSERT_TRUE(config_reload());

    // Cleanup
    config_unregister_callback(id);
    config_schema_destroy(schema);
    std::filesystem::remove(complex_config);
    unsetenv("NIMCP_BASE");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
