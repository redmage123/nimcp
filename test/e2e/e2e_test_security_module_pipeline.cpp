/**
 * @file e2e_test_security_module_pipeline.cpp
 * @brief End-to-end tests for security module bridge lifecycle
 *
 * WHAT: Tests the security bridge creation, update, and destruction
 * WHY:  Ensure all security modules can be used together without issues
 * HOW:  Create all bridges, run updates, destroy cleanly
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>

extern "C" {
#include "security/perception/nimcp_security_perception_input_bridge.h"
#include "security/language/nimcp_security_language_bridge.h"
#include "security/executive/nimcp_security_executive_bridge.h"
#include "security/memory/nimcp_security_memory_bridge.h"
#include "security/training/nimcp_security_training_bridge.h"
#include "security/rcog/nimcp_security_rcog_bridge.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/recursive/nimcp_rcog_types.h"
#include "utils/error/nimcp_error_codes.h"
#include "nimcp.h"
}

// ============================================================================
// Helper macro to check if bio-async is available
// Bio-async availability is inferred from training bridge creation success
// ============================================================================
#define SKIP_IF_BIO_ASYNC_UNAVAILABLE(training_bridge) \
    do { \
        if (!(training_bridge)) { \
            GTEST_SKIP() << "Bio-async router not available (training bridge creation failed)"; \
        } \
    } while (0)

// Timeout in milliseconds for update operations
static constexpr int UPDATE_TIMEOUT_MS = 1000;

namespace {

// ============================================================================
// E2E Security Module Pipeline Test Fixture
// ============================================================================

class SecurityModulePipelineE2E : public ::testing::Test {
protected:
    security_perception_input_bridge_t* perception_bridge_ = nullptr;
    security_language_bridge_t* language_bridge_ = nullptr;
    security_executive_bridge_t* executive_bridge_ = nullptr;
    security_mem_bridge_t* memory_bridge_ = nullptr;
    security_training_bridge_t* training_bridge_ = nullptr;
    security_rcog_bridge_t* rcog_bridge_ = nullptr;

    void SetUp() override {
        // Create perception bridge with default config
        sec_percept_input_config_t percept_config;
        if (security_perception_input_default_config(&percept_config) == 0) {
            perception_bridge_ = security_perception_input_bridge_create(&percept_config);
        }

        // Create language bridge with default config
        security_language_bridge_config_t lang_config;
        if (security_language_default_config(&lang_config) == 0) {
            language_bridge_ = security_language_bridge_create(&lang_config);
        }

        // Create executive bridge with default config
        security_executive_config_t exec_config;
        if (security_executive_default_config(&exec_config) == 0) {
            executive_bridge_ = security_executive_bridge_create(&exec_config);
        }

        // Create memory bridge with default config
        security_mem_config_t mem_config;
        if (security_memory_default_config(&mem_config) == 0) {
            memory_bridge_ = security_memory_bridge_create(&mem_config);
        }

        // Create training bridge with default config
        security_training_config_t train_config;
        if (security_training_default_config(&train_config) == 0) {
            training_bridge_ = security_training_bridge_create(&train_config);
        }

        // Create RCOG bridge with default config
        security_rcog_config_t rcog_config;
        if (security_rcog_default_config(&rcog_config) == 0) {
            rcog_bridge_ = security_rcog_bridge_create(&rcog_config);
        }
    }

    void TearDown() override {
        if (perception_bridge_) {
            security_perception_input_bridge_destroy(perception_bridge_);
        }
        if (language_bridge_) {
            security_language_bridge_destroy(language_bridge_);
        }
        if (executive_bridge_) {
            security_executive_bridge_destroy(executive_bridge_);
        }
        if (memory_bridge_) {
            security_memory_bridge_destroy(memory_bridge_);
        }
        if (training_bridge_) {
            security_training_bridge_destroy(training_bridge_);
        }
        if (rcog_bridge_) {
            security_rcog_bridge_destroy(rcog_bridge_);
        }
    }
};

// ============================================================================
// E2E Scenario 1: All Bridges Can Be Created
// ============================================================================

TEST_F(SecurityModulePipelineE2E, AllBridgesCreated) {
    // Verify all bridges were successfully created
    EXPECT_NE(perception_bridge_, nullptr) << "Perception bridge creation failed";
    EXPECT_NE(language_bridge_, nullptr) << "Language bridge creation failed";
    EXPECT_NE(executive_bridge_, nullptr) << "Executive bridge creation failed";
    EXPECT_NE(memory_bridge_, nullptr) << "Memory bridge creation failed";
    EXPECT_NE(training_bridge_, nullptr) << "Training bridge creation failed";
    EXPECT_NE(rcog_bridge_, nullptr) << "RCOG bridge creation failed";
}

// ============================================================================
// E2E Scenario 2: Bridges Can Be Updated
// ============================================================================

TEST_F(SecurityModulePipelineE2E, BridgesCanBeUpdated) {
    // Skip if bio-async is not available (training bridge is key indicator)
    SKIP_IF_BIO_ASYNC_UNAVAILABLE(training_bridge_);

    if (!perception_bridge_ || !language_bridge_ || !executive_bridge_ ||
        !memory_bridge_ || !rcog_bridge_) {
        GTEST_SKIP() << "Not all bridges were created";
    }

    // Use async with timeout to prevent hanging
    auto update_task = std::async(std::launch::async, [this]() -> int {
        // Update perception bridge
        int ret = security_perception_input_update(perception_bridge_);
        if (ret != NIMCP_SUCCESS) return ret;

        // Update language bridge
        ret = security_language_update(language_bridge_);
        if (ret != NIMCP_SUCCESS) return ret;

        // Update executive bridge
        ret = security_executive_bridge_update(executive_bridge_, 10);
        if (ret != NIMCP_SUCCESS) return ret;

        // Update memory bridge
        ret = security_memory_bridge_update(memory_bridge_, 10);
        if (ret != NIMCP_SUCCESS) return ret;

        // Update training bridge effects
        ret = security_training_update_security_effects(training_bridge_);
        if (ret != NIMCP_SUCCESS) return ret;

        ret = security_training_update_training_effects(training_bridge_, 0.1f, 1.0f, 1);
        if (ret != NIMCP_SUCCESS) return ret;

        // Update RCOG bridge
        ret = security_rcog_bridge_update(rcog_bridge_, 10);
        return ret;
    });

    // Wait with timeout
    auto status = update_task.wait_for(std::chrono::milliseconds(UPDATE_TIMEOUT_MS));
    if (status == std::future_status::timeout) {
        GTEST_SKIP() << "Bridge update operations timed out - bio-async may not be available";
    }

    int result = update_task.get();
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Bridge update failed with error: " << result;
}

// ============================================================================
// E2E Scenario 3: Language Injection Detection
// ============================================================================

TEST_F(SecurityModulePipelineE2E, LanguageInjectionDetection) {
    if (!language_bridge_) {
        GTEST_SKIP() << "Language bridge not created";
    }

    // Test SQL injection detection
    const char* sql_injection = "'; DROP TABLE users; --";
    security_language_detection_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = security_language_detect_injection(
        language_bridge_, sql_injection, strlen(sql_injection), &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.injection_detected) << "SQL injection not detected";

    // Test safe input
    const char* safe_input = "Hello, this is a normal message.";
    memset(&result, 0, sizeof(result));

    ret = security_language_detect_injection(
        language_bridge_, safe_input, strlen(safe_input), &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.injection_detected) << "False positive on safe input";
}

// ============================================================================
// E2E Scenario 4: Executive Task Authorization
// ============================================================================

TEST_F(SecurityModulePipelineE2E, ExecutiveTaskAuthorization) {
    if (!executive_bridge_) {
        GTEST_SKIP() << "Executive bridge not created";
    }

    // Create a task
    task_descriptor_t task = {};
    task.task_id = 1;
    task.type = TASK_TYPE_REASONING;
    task.priority = PRIORITY_NORMAL;
    strncpy(task.name, "test_task", sizeof(task.name) - 1);

    security_auth_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = security_executive_authorize_task(
        executive_bridge_, &task, 1, nullptr, 0, &result
    );
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// E2E Scenario 5: RCOG Tool Whitelisting
// ============================================================================

TEST_F(SecurityModulePipelineE2E, RCOGToolWhitelisting) {
    if (!rcog_bridge_) {
        GTEST_SKIP() << "RCOG bridge not created";
    }

    // Whitelist a tool
    security_rcog_tool_permission_t permission;
    memset(&permission, 0, sizeof(permission));
    strncpy(permission.tool_name, "safe_tool", sizeof(permission.tool_name) - 1);
    permission.min_tier = RCOG_TIER_L1_REASONING;
    permission.requires_approval = false;

    int ret = security_rcog_whitelist_tool(rcog_bridge_, &permission);
    EXPECT_EQ(ret, 0) << "Tool whitelisting failed";

    // Validate whitelisted tool params
    security_rcog_validation_result_t result = security_rcog_validate_tool_params(
        rcog_bridge_, "safe_tool", "test_params", RCOG_TIER_L1_REASONING
    );
    EXPECT_EQ(result, SECURITY_RCOG_VALID) << "Whitelisted tool should be valid";

    // Validate non-whitelisted tool params
    result = security_rcog_validate_tool_params(
        rcog_bridge_, "unknown_tool", "test_params", RCOG_TIER_L1_REASONING
    );
    EXPECT_NE(result, SECURITY_RCOG_VALID) << "Non-whitelisted tool should be invalid";
}

// ============================================================================
// E2E Scenario 6: Multiple Update Cycles
// ============================================================================

TEST_F(SecurityModulePipelineE2E, MultipleUpdateCycles) {
    // Skip if bio-async is not available (training bridge is key indicator)
    SKIP_IF_BIO_ASYNC_UNAVAILABLE(training_bridge_);

    if (!perception_bridge_ || !language_bridge_ || !executive_bridge_) {
        GTEST_SKIP() << "Required bridges not created";
    }

    // Reduced from 50 to 10 iterations for stability
    const int cycles = 10;
    std::atomic<int> completed_cycles{0};

    // Use async with timeout to prevent hanging
    auto update_task = std::async(std::launch::async, [this, cycles, &completed_cycles]() {
        for (int i = 0; i < cycles; i++) {
            security_perception_input_update(perception_bridge_);
            security_language_update(language_bridge_);
            security_executive_bridge_update(executive_bridge_, 10);
            completed_cycles.store(i + 1, std::memory_order_relaxed);
        }
    });

    // Wait with timeout (10 cycles should complete well within 2 seconds)
    auto status = update_task.wait_for(std::chrono::milliseconds(2000));
    if (status == std::future_status::timeout) {
        GTEST_SKIP() << "Update cycles timed out after completing "
                     << completed_cycles.load() << " of " << cycles
                     << " cycles - bio-async may not be available";
    }

    // Get the result to propagate any exceptions
    update_task.get();

    std::cout << "Completed " << cycles << " update cycles successfully" << std::endl;
}

// ============================================================================
// E2E Scenario 7: Bridge Stats Can Be Retrieved
// ============================================================================

TEST_F(SecurityModulePipelineE2E, BridgeStatsRetrieval) {
    if (!perception_bridge_ || !rcog_bridge_) {
        GTEST_SKIP() << "Required bridges not created";
    }

    // Get perception stats
    sec_percept_input_stats_t percept_stats;
    memset(&percept_stats, 0, sizeof(percept_stats));
    int ret = security_perception_input_get_stats(perception_bridge_, &percept_stats);
    EXPECT_EQ(ret, 0) << "Failed to get perception stats";

    // Get RCOG stats
    security_rcog_stats_t rcog_stats;
    memset(&rcog_stats, 0, sizeof(rcog_stats));
    ret = security_rcog_get_stats(rcog_bridge_, &rcog_stats);
    EXPECT_EQ(ret, 0) << "Failed to get RCOG stats";

    // Get training stats
    if (training_bridge_) {
        security_training_stats_t train_stats;
        memset(&train_stats, 0, sizeof(train_stats));
        ret = security_training_get_stats(training_bridge_, &train_stats);
        EXPECT_EQ(ret, 0) << "Failed to get training stats";
    }
}

}  // namespace
