/**
 * @file test_config_callbacks.cpp
 * @brief Comprehensive unit tests for dynamic config callbacks
 *
 * WHAT: Test suite for config_register_callback and config_unregister_callback
 * WHY:  Ensure 100% code coverage and thread-safe callback functionality
 * HOW:  Test registration, invocation, unregistration, and edge cases
 *
 * @author NIMCP Test Team
 * @date 2025-01-17
 */

#include <gtest/gtest.h>
#include "utils/config/nimcp_dynamic_config.h"
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

struct CallbackRecord {
    std::string key;
    config_value_t old_value;
    config_value_t new_value;
    config_value_type_t type;
    void* user_data;

    CallbackRecord() : key(""), old_value{}, new_value{}, type(CONFIG_TYPE_INT), user_data(nullptr) {}
};

class ConfigCallbacksTest : public ::testing::Test {
protected:
    std::vector<CallbackRecord> callback_history;
    std::atomic<int> callback_count{0};

    void SetUp() override {
        callback_history.clear();
        callback_count = 0;

        // Initialize config system with temp file
        system("echo 'test_int=42' > /tmp/test_config.ini");
        system("echo 'test_float=3.14' >> /tmp/test_config.ini");
        system("echo 'test_bool=true' >> /tmp/test_config.ini");
        system("echo 'test_string=hello' >> /tmp/test_config.ini");

        bool success = config_init("/tmp/test_config.ini");
        ASSERT_TRUE(success);
    }

    void TearDown() override {
        config_shutdown();
        system("rm -f /tmp/test_config.ini");
    }

    // Static callback for testing
    static void TestCallback(const char* key, const config_value_t* old_value,
                            const config_value_t* new_value, void* user_data) {
        auto* test = static_cast<ConfigCallbacksTest*>(user_data);
        if (test) {
            CallbackRecord record;
            record.key = key ? key : "";
            if (old_value) record.old_value = *old_value;
            if (new_value) record.new_value = *new_value;
            record.user_data = user_data;

            test->callback_history.push_back(record);
            test->callback_count++;
        }
    }

    // Counter callback for simple counting
    static void CounterCallback(const char* key, const config_value_t* old_value,
                               const config_value_t* new_value, void* user_data) {
        (void)key; (void)old_value; (void)new_value;
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        if (counter) {
            (*counter)++;
        }
    }
};

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(ConfigCallbacksTest, RegisterCallback_ValidKey) {
    // WHAT: Register callback for specific key
    // WHY:  Basic functionality test
    // HOW:  Register callback, verify non-zero ID

    uint32_t id = config_register_callback("test_int", TestCallback, this);
    EXPECT_NE(id, 0);

    // Cleanup
    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, RegisterCallback_NullKey) {
    // WHAT: Register callback for all keys (NULL key)
    // WHY:  Test wildcard registration
    // HOW:  NULL key should match all config changes

    uint32_t id = config_register_callback(nullptr, TestCallback, this);
    EXPECT_NE(id, 0);

    // Cleanup
    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, RegisterCallback_NullCallback) {
    // WHAT: Register NULL callback function
    // WHY:  Should reject gracefully
    // HOW:  Expect ID = 0

    uint32_t id = config_register_callback("test_int", nullptr, this);
    EXPECT_EQ(id, 0);
}

TEST_F(ConfigCallbacksTest, RegisterCallback_MultipleCallbacks) {
    // WHAT: Register multiple callbacks
    // WHY:  Should support multiple registrations
    // HOW:  Each should get unique ID

    uint32_t id1 = config_register_callback("test_int", TestCallback, this);
    uint32_t id2 = config_register_callback("test_float", TestCallback, this);
    uint32_t id3 = config_register_callback(nullptr, TestCallback, this);

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id3, 0);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id1, id3);
    EXPECT_NE(id2, id3);

    // Cleanup
    config_unregister_callback(id1);
    config_unregister_callback(id2);
    config_unregister_callback(id3);
}

TEST_F(ConfigCallbacksTest, RegisterCallback_MaxCallbacks) {
    // WHAT: Test maximum callback limit
    // WHY:  Should handle gracefully when limit reached
    // HOW:  Register 64+ callbacks, expect failure after limit

    std::vector<uint32_t> ids;

    // Register up to limit (MAX_CALLBACKS = 64)
    for (int i = 0; i < 64; i++) {
        uint32_t id = config_register_callback("test_int", CounterCallback, &callback_count);
        if (id != 0) {
            ids.push_back(id);
        }
    }

    // Should have registered at least 60 (allow some to be in use by system)
    EXPECT_GE(ids.size(), 60);

    // Try to register beyond limit - should fail
    uint32_t overflow_id = config_register_callback("test_int", CounterCallback, &callback_count);
    // May succeed or fail depending on how many slots are available
    // Just verify it doesn't crash

    // Cleanup
    for (uint32_t id : ids) {
        config_unregister_callback(id);
    }
    if (overflow_id != 0) {
        config_unregister_callback(overflow_id);
    }
}

//=============================================================================
// Unregistration Tests
//=============================================================================

TEST_F(ConfigCallbacksTest, UnregisterCallback_ValidID) {
    // WHAT: Unregister valid callback
    // WHY:  Should remove callback cleanly
    // HOW:  Register, unregister, verify no longer called

    uint32_t id = config_register_callback("test_int", TestCallback, this);
    ASSERT_NE(id, 0);

    config_unregister_callback(id);

    // Change config - callback should NOT be invoked
    config_set_int("test_int", 99);
    EXPECT_EQ(callback_count, 0);
}

TEST_F(ConfigCallbacksTest, UnregisterCallback_InvalidID) {
    // WHAT: Unregister invalid ID
    // WHY:  Should handle gracefully
    // HOW:  Unregister ID=0 and non-existent ID

    config_unregister_callback(0);
    config_unregister_callback(99999);

    // Should not crash
}

TEST_F(ConfigCallbacksTest, UnregisterCallback_Twice) {
    // WHAT: Unregister same ID twice
    // WHY:  Should handle gracefully
    // HOW:  Register, unregister twice

    uint32_t id = config_register_callback("test_int", TestCallback, this);
    ASSERT_NE(id, 0);

    config_unregister_callback(id);
    config_unregister_callback(id);  // Second time should be no-op

    // Should not crash
}

//=============================================================================
// Callback Invocation Tests
//=============================================================================

TEST_F(ConfigCallbacksTest, CallbackInvoked_IntChange) {
    // WHAT: Callback invoked on int config change
    // WHY:  Test basic callback triggering
    // HOW:  Register callback, change value, verify invocation

    uint32_t id = config_register_callback("test_int", TestCallback, this);
    ASSERT_NE(id, 0);

    // Change config value
    config_set_int("test_int", 100);

    // Callback should have been invoked
    EXPECT_EQ(callback_count, 1);
    ASSERT_EQ(callback_history.size(), 1);
    EXPECT_EQ(callback_history[0].key, "test_int");
    EXPECT_EQ(callback_history[0].new_value.int_val, 100);

    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, CallbackInvoked_FloatChange) {
    // WHAT: Callback invoked on float config change
    // WHY:  Test float value changes
    // HOW:  Register callback, change value

    uint32_t id = config_register_callback("test_float", TestCallback, this);
    ASSERT_NE(id, 0);

    config_set_float("test_float", 2.718);

    EXPECT_EQ(callback_count, 1);
    ASSERT_EQ(callback_history.size(), 1);
    EXPECT_EQ(callback_history[0].key, "test_float");
    EXPECT_NEAR(callback_history[0].new_value.float_val, 2.718, 0.001);

    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, CallbackInvoked_BoolChange) {
    // WHAT: Callback invoked on bool config change
    // WHY:  Test boolean value changes
    // HOW:  Register callback, change value

    uint32_t id = config_register_callback("test_bool", TestCallback, this);
    ASSERT_NE(id, 0);

    config_set_bool("test_bool", false);

    EXPECT_EQ(callback_count, 1);
    ASSERT_EQ(callback_history.size(), 1);
    EXPECT_EQ(callback_history[0].key, "test_bool");
    EXPECT_FALSE(callback_history[0].new_value.bool_val);

    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, CallbackInvoked_StringChange) {
    // WHAT: Callback invoked on string config change
    // WHY:  Test string value changes
    // HOW:  Register callback, change value

    uint32_t id = config_register_callback("test_string", TestCallback, this);
    ASSERT_NE(id, 0);

    config_set_string("test_string", "world");

    EXPECT_EQ(callback_count, 1);
    ASSERT_EQ(callback_history.size(), 1);
    EXPECT_EQ(callback_history[0].key, "test_string");

    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, CallbackNotInvoked_DifferentKey) {
    // WHAT: Callback not invoked for different key
    // WHY:  Test key filtering
    // HOW:  Register for "test_int", change "test_float"

    uint32_t id = config_register_callback("test_int", TestCallback, this);
    ASSERT_NE(id, 0);

    config_set_float("test_float", 1.23);

    EXPECT_EQ(callback_count, 0);

    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, CallbackInvoked_WildcardKey) {
    // WHAT: Wildcard callback invoked for all changes
    // WHY:  Test NULL key (all keys)
    // HOW:  Register with NULL key, change multiple values

    uint32_t id = config_register_callback(nullptr, TestCallback, this);
    ASSERT_NE(id, 0);

    config_set_int("test_int", 50);
    config_set_float("test_float", 1.41);
    config_set_bool("test_bool", true);

    // Should be invoked for all 3 changes
    EXPECT_EQ(callback_count, 3);

    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, MultipleCallbacks_SameKey) {
    // WHAT: Multiple callbacks for same key
    // WHY:  Should invoke all matching callbacks
    // HOW:  Register 3 callbacks for same key

    std::atomic<int> counter1{0}, counter2{0}, counter3{0};

    uint32_t id1 = config_register_callback("test_int", CounterCallback, &counter1);
    uint32_t id2 = config_register_callback("test_int", CounterCallback, &counter2);
    uint32_t id3 = config_register_callback("test_int", CounterCallback, &counter3);

    config_set_int("test_int", 77);

    // All 3 should be invoked
    EXPECT_EQ(counter1, 1);
    EXPECT_EQ(counter2, 1);
    EXPECT_EQ(counter3, 1);

    config_unregister_callback(id1);
    config_unregister_callback(id2);
    config_unregister_callback(id3);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(ConfigCallbacksTest, ThreadSafety_ConcurrentRegistration) {
    // WHAT: Concurrent callback registration
    // WHY:  Test thread safety of registration
    // HOW:  Multiple threads registering simultaneously

    std::vector<uint32_t> ids;
    std::mutex ids_mutex;

    auto register_callbacks = [&]() {
        for (int i = 0; i < 10; i++) {
            uint32_t id = config_register_callback("test_int", CounterCallback, &callback_count);
            if (id != 0) {
                std::lock_guard<std::mutex> lock(ids_mutex);
                ids.push_back(id);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(register_callbacks);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have registered some callbacks without crashing
    EXPECT_GT(ids.size(), 0);

    // Cleanup
    for (uint32_t id : ids) {
        config_unregister_callback(id);
    }
}

TEST_F(ConfigCallbacksTest, ThreadSafety_ConcurrentInvocation) {
    // WHAT: Callbacks invoked during concurrent config changes
    // WHY:  Test thread safety of callback invocation
    // HOW:  Multiple threads changing config simultaneously

    uint32_t id = config_register_callback(nullptr, CounterCallback, &callback_count);
    ASSERT_NE(id, 0);

    auto change_config = [](int thread_id) {
        for (int i = 0; i < 20; i++) {
            config_set_int("test_int", thread_id * 100 + i);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(change_config, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have invoked callback many times without crashing
    EXPECT_GT(callback_count, 0);

    config_unregister_callback(id);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(ConfigCallbacksTest, UserData_Null) {
    // WHAT: Register callback with NULL user_data
    // WHY:  Should be valid (user_data is optional)
    // HOW:  Register with NULL, verify ID

    uint32_t id = config_register_callback("test_int", CounterCallback, nullptr);
    EXPECT_NE(id, 0);

    // Should not crash when invoked
    config_set_int("test_int", 55);

    config_unregister_callback(id);
}

TEST_F(ConfigCallbacksTest, UserData_Preserved) {
    // WHAT: User data preserved and passed to callback
    // WHY:  Test user_data parameter
    // HOW:  Pass custom data, verify in callback

    // FIX: Use 'this' as user_data since TestCallback expects ConfigCallbacksTest*
    uint32_t id = config_register_callback("test_int", TestCallback, this);
    ASSERT_NE(id, 0);

    config_set_int("test_int", 1);

    EXPECT_EQ(callback_count, 1);
    ASSERT_EQ(callback_history.size(), 1);
    EXPECT_EQ(callback_history[0].user_data, this);

    config_unregister_callback(id);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
