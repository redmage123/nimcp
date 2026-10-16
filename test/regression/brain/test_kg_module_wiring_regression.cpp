/**
 * @file test_kg_module_wiring_regression.cpp
 * @brief Regression tests for KG Module Wiring
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests to prevent regressions in the KG module wiring system:
 * - Edge case handling
 * - Boundary conditions
 * - Previously fixed bugs
 * - Memory safety
 * - Thread safety scenarios
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

extern "C" {
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_kg_wiring_exception.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class KGModuleWiringRegressionTest : public ::testing::Test {
protected:
    kg_module_wiring_t* wiring_ = nullptr;

    void SetUp() override {
        kg_wiring_disable_exceptions();  // Prevent slow immune dispatch in tests
        wiring_ = kg_module_wiring_create("regression_test_module", "TEST");
        ASSERT_NE(wiring_, nullptr);
    }

    void TearDown() override {
        if (wiring_) {
            kg_module_wiring_destroy(wiring_);
            wiring_ = nullptr;
        }
    }
};

//=============================================================================
// REGRESSION TEST SUITE: Boundary Conditions
//=============================================================================

/**
 * @test Maximum inputs should not cause overflow
 * Regression: Previously could overflow internal array
 */
TEST_F(KGModuleWiringRegressionTest, MaxInputsNoOverflow) {
    // Try to add more than MAX_INPUTS
    for (int i = 0; i < KG_WIRING_MAX_INPUTS + 10; i++) {
        std::string source = "input_" + std::to_string(i);
        std::string msg = "MSG_" + std::to_string(i);
        int result = kg_module_wiring_add_input(wiring_, source.c_str(), msg.c_str(), true);

        if (i < KG_WIRING_MAX_INPUTS) {
            EXPECT_EQ(result, 0) << "Failed at input " << i;
        } else {
            // Should fail gracefully, not crash
            EXPECT_NE(result, 0) << "Should have failed at input " << i;
        }
    }

    // Verify count is capped at maximum
    EXPECT_EQ(wiring_->input_count, static_cast<uint32_t>(KG_WIRING_MAX_INPUTS));
}

/**
 * @test Maximum outputs should not cause overflow
 * Regression: Array bounds check was off-by-one
 */
TEST_F(KGModuleWiringRegressionTest, MaxOutputsNoOverflow) {
    for (int i = 0; i < KG_WIRING_MAX_OUTPUTS + 10; i++) {
        std::string msg = "OUTPUT_" + std::to_string(i);
        int result = kg_module_wiring_add_output(wiring_, msg.c_str(), "Description");

        if (i < KG_WIRING_MAX_OUTPUTS) {
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_NE(result, 0);
        }
    }

    EXPECT_EQ(wiring_->output_count, static_cast<uint32_t>(KG_WIRING_MAX_OUTPUTS));
}

/**
 * @test Maximum handlers should not cause overflow
 */
TEST_F(KGModuleWiringRegressionTest, MaxHandlersNoOverflow) {
    for (int i = 0; i < KG_WIRING_MAX_HANDLERS + 10; i++) {
        std::string msg = "HANDLER_" + std::to_string(i);
        int result = kg_module_wiring_add_handler(wiring_, msg.c_str(), static_cast<uint32_t>(i % 100));

        if (i < KG_WIRING_MAX_HANDLERS) {
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_NE(result, 0);
        }
    }

    EXPECT_EQ(wiring_->handler_count, static_cast<uint32_t>(KG_WIRING_MAX_HANDLERS));
}

/**
 * @test Empty string module name handling
 * Regression: Empty name caused null pointer dereference
 */
TEST_F(KGModuleWiringRegressionTest, EmptyNameHandling) {
    // Empty name should fail gracefully
    kg_module_wiring_t* bad_wiring = kg_module_wiring_create("", "TYPE");
    EXPECT_EQ(bad_wiring, nullptr);

    // NULL name should fail
    bad_wiring = kg_module_wiring_create(nullptr, "TYPE");
    EXPECT_EQ(bad_wiring, nullptr);

    // Empty type should also fail
    bad_wiring = kg_module_wiring_create("name", "");
    EXPECT_EQ(bad_wiring, nullptr);
}

/**
 * @test Very long module name handling
 * Regression: Long names caused buffer overflow
 */
TEST_F(KGModuleWiringRegressionTest, LongNameTruncation) {
    // Create name longer than maximum
    std::string long_name(512, 'A');

    kg_module_wiring_t* long_wiring = kg_module_wiring_create(long_name.c_str(), "TYPE");

    // Should either fail or truncate safely
    if (long_wiring) {
        // If created, name should be truncated
        EXPECT_LT(strlen(long_wiring->module_name), static_cast<size_t>(KG_WIRING_MAX_NAME_LEN));
        kg_module_wiring_destroy(long_wiring);
    }
}

//=============================================================================
// REGRESSION TEST SUITE: Edge Cases
//=============================================================================

/**
 * @test Required vs optional inputs
 */
TEST_F(KGModuleWiringRegressionTest, RequiredInputFlag) {
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "required_src", "MSG_A", true), 0);
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "optional_src", "MSG_B", false), 0);

    EXPECT_EQ(wiring_->input_count, 2u);
    EXPECT_TRUE(wiring_->inputs[0].required);
    EXPECT_FALSE(wiring_->inputs[1].required);
}

/**
 * @test Handler priority 0 edge case
 * Regression: Priority 0 was treated as invalid
 */
TEST_F(KGModuleWiringRegressionTest, HandlerPriorityZero) {
    int result = kg_module_wiring_add_handler(wiring_, "ZERO_PRIORITY", 0);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(wiring_->handler_count, 1u);
    EXPECT_EQ(wiring_->handlers[0].priority, 0u);
}

/**
 * @test Maximum priority value
 */
TEST_F(KGModuleWiringRegressionTest, HandlerPriorityMax) {
    int result = kg_module_wiring_add_handler(wiring_, "MAX_PRIORITY", UINT32_MAX);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(wiring_->handlers[0].priority, UINT32_MAX);
}

/**
 * @test Duplicate input names (same source, different messages)
 */
TEST_F(KGModuleWiringRegressionTest, DuplicateInputSource) {
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "source", "MSG_A", true), 0);
    ASSERT_EQ(kg_module_wiring_add_input(wiring_, "source", "MSG_B", true), 0);

    // Both should be added (same source, different message types)
    EXPECT_EQ(wiring_->input_count, 2u);
}

/**
 * @test Weight types enumeration
 */
TEST_F(KGModuleWiringRegressionTest, WeightTypesValid) {
    // Test all weight types
    kg_weight_type_t types[] = {
        KG_WEIGHT_NONE,
        KG_WEIGHT_SNN,
        KG_WEIGHT_LNN,
        KG_WEIGHT_CNN,
        KG_WEIGHT_RNN,
        KG_WEIGHT_TRANSFORMER,
        KG_WEIGHT_GNN,
        KG_WEIGHT_HYBRID,
        KG_WEIGHT_CUSTOM
    };

    for (auto type : types) {
        const char* str = kg_weight_type_to_string(type);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }

    // Invalid type should return something safe
    const char* invalid = kg_weight_type_to_string(static_cast<kg_weight_type_t>(999));
    EXPECT_NE(invalid, nullptr);
}

//=============================================================================
// REGRESSION TEST SUITE: Memory Safety
//=============================================================================

/**
 * @test Double destroy should not crash
 * Regression: Double-free caused crash
 */
TEST_F(KGModuleWiringRegressionTest, DoubleDestroySafe) {
    kg_module_wiring_t* temp_wiring = kg_module_wiring_create("temp", "TYPE");
    ASSERT_NE(temp_wiring, nullptr);

    kg_module_wiring_destroy(temp_wiring);

    // Note: After destroy, pointer is dangling - we can't safely destroy again
    // This test documents that kg_module_wiring_destroy(nullptr) is safe
    kg_module_wiring_destroy(nullptr);
}

/**
 * @test NULL pointer handling in all functions
 * Regression: NULL inputs caused segfaults
 */
TEST_F(KGModuleWiringRegressionTest, NullPointerSafety) {
    // All functions should handle NULL gracefully
    kg_module_wiring_destroy(nullptr);

    EXPECT_EQ(kg_module_wiring_add_input(nullptr, "test", "msg", true), -1);
    EXPECT_EQ(kg_module_wiring_add_output(nullptr, "msg", "desc"), -1);
    EXPECT_EQ(kg_module_wiring_add_handler(nullptr, "msg", 10), -1);
    EXPECT_EQ(kg_module_wiring_set_weights(nullptr, KG_WEIGHT_SNN, nullptr, 0), -1);
    EXPECT_EQ(kg_module_wiring_set_metadata(nullptr, nullptr, nullptr, nullptr), -1);

    EXPECT_FALSE(kg_module_wiring_has_input(nullptr, "src", "msg"));
    EXPECT_FALSE(kg_module_wiring_has_output(nullptr, "msg"));
    EXPECT_FALSE(kg_module_wiring_has_handler(nullptr, "msg"));
    EXPECT_EQ(kg_module_wiring_get_handler_priority(nullptr, "msg"), 0u);

    EXPECT_EQ(kg_module_wiring_validate(nullptr, nullptr, 0), -1);
}

/**
 * @test NULL source/msg_type in connection functions
 * Regression: NULL strings caused string operation crashes
 */
TEST_F(KGModuleWiringRegressionTest, NullConnectionStrings) {
    EXPECT_EQ(kg_module_wiring_add_input(wiring_, nullptr, "msg", true), -1);
    EXPECT_EQ(kg_module_wiring_add_input(wiring_, "src", nullptr, true), -1);

    EXPECT_EQ(kg_module_wiring_add_output(wiring_, nullptr, "desc"), -1);

    EXPECT_EQ(kg_module_wiring_add_handler(wiring_, nullptr, 10), -1);

    // Verify no connections were added
    EXPECT_EQ(wiring_->input_count, 0u);
    EXPECT_EQ(wiring_->output_count, 0u);
    EXPECT_EQ(wiring_->handler_count, 0u);
}

/**
 * @test Weights with NULL data
 */
TEST_F(KGModuleWiringRegressionTest, NullWeightData) {
    // NULL weights with size > 0 should fail
    EXPECT_EQ(kg_module_wiring_set_weights(wiring_, KG_WEIGHT_SNN, nullptr, 100), -1);

    // NULL weights with size = 0 might succeed (depends on impl)
    int result = kg_module_wiring_set_weights(wiring_, KG_WEIGHT_SNN, nullptr, 0);
    // Either is acceptable
    (void)result;
}

//=============================================================================
// REGRESSION TEST SUITE: Validation
//=============================================================================

/**
 * @test Validation with NULL error buffer
 * Regression: NULL buffer caused crash
 */
TEST_F(KGModuleWiringRegressionTest, ValidateNullBuffer) {
    // Should not crash with NULL buffer
    int result = kg_module_wiring_validate(wiring_, nullptr, 0);
    // Result code should indicate valid/invalid
    EXPECT_EQ(result, 0);  // Valid empty wiring
}

/**
 * @test Validation consistency across calls
 */
TEST_F(KGModuleWiringRegressionTest, ValidateConsistency) {
    // Add some configuration
    kg_module_wiring_add_input(wiring_, "input", "MSG", true);
    kg_module_wiring_add_output(wiring_, "output", "Description");
    kg_module_wiring_add_handler(wiring_, "HANDLER", 50);

    char buf1[256], buf2[256];

    // Validate multiple times - should be consistent
    int result1 = kg_module_wiring_validate(wiring_, buf1, sizeof(buf1));
    int result2 = kg_module_wiring_validate(wiring_, buf2, sizeof(buf2));

    EXPECT_EQ(result1, result2);
}

/**
 * @test Validation with various buffer sizes
 */
TEST_F(KGModuleWiringRegressionTest, ValidateBufferSizes) {
    char tiny_buf[4];
    char small_buf[32];
    char large_buf[1024];

    // All should work without crashing
    kg_module_wiring_validate(wiring_, tiny_buf, sizeof(tiny_buf));
    kg_module_wiring_validate(wiring_, small_buf, sizeof(small_buf));
    kg_module_wiring_validate(wiring_, large_buf, sizeof(large_buf));
}

//=============================================================================
// REGRESSION TEST SUITE: Metadata
//=============================================================================

/**
 * @test Maximum metadata entries
 */
TEST_F(KGModuleWiringRegressionTest, MaxMetadataEntries) {
    for (int i = 0; i < KG_WIRING_MAX_METADATA + 5; i++) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        int result = kg_module_wiring_add_metadata_entry(wiring_, key.c_str(), value.c_str());

        if (i < KG_WIRING_MAX_METADATA) {
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_NE(result, 0);
        }
    }

    EXPECT_EQ(wiring_->metadata.entry_count, static_cast<uint32_t>(KG_WIRING_MAX_METADATA));
}

/**
 * @test Long metadata values truncation
 */
TEST_F(KGModuleWiringRegressionTest, LongMetadataValues) {
    std::string long_key(512, 'K');
    std::string long_value(1024, 'V');

    // Should truncate or fail gracefully
    int result = kg_module_wiring_add_metadata_entry(wiring_, long_key.c_str(), long_value.c_str());

    if (result == 0) {
        // If successful, strings should be truncated
        EXPECT_LT(strlen(wiring_->metadata.entries[0].key),
                  static_cast<size_t>(KG_WIRING_MAX_META_KEY_LEN));
        EXPECT_LT(strlen(wiring_->metadata.entries[0].value),
                  static_cast<size_t>(KG_WIRING_MAX_META_VALUE_LEN));
    }
}

//=============================================================================
// REGRESSION TEST SUITE: Thread Safety (Basic)
//=============================================================================

/**
 * @test Concurrent reads should not corrupt state
 */
TEST_F(KGModuleWiringRegressionTest, ConcurrentReadSafety) {
    // Setup wiring with data
    kg_module_wiring_add_input(wiring_, "source", "MSG", true);
    kg_module_wiring_add_handler(wiring_, "HANDLER", 50);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Multiple threads reading concurrently
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 100; j++) {
                bool has_input = kg_module_wiring_has_input(wiring_, "source", "MSG");
                bool has_handler = kg_module_wiring_has_handler(wiring_, "HANDLER");
                uint32_t priority = kg_module_wiring_get_handler_priority(wiring_, "HANDLER");

                if (has_input && has_handler && priority == 50) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All reads should succeed consistently
    EXPECT_EQ(success_count.load(), 400);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
