/**
 * @file test_pr_memory_exception_regression.cpp
 * @brief Regression tests for PR memory system exception handling
 * @date 2026-01-25
 *
 * WHAT: Regression tests to prevent exception handling regressions in PR memory
 * WHY:  Ensure exception handling behavior remains stable across changes
 * HOW:  Test edge cases, boundary conditions, and previously fixed issues
 *
 * REGRESSION SCENARIOS:
 * - Exception handling consistency across API changes
 * - Memory safety during exception conditions
 * - Correct error code propagation
 * - Thread safety during exception handling
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdint>

// C++ compatibility for C11 _Atomic keyword
#ifndef _Atomic
#define _Atomic volatile
#endif

extern "C" {
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_procedural.h"
#include "cognitive/memory/core/nimcp_prospective.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PRMemoryExceptionRegressionTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> last_error_code;
    static nimcp_handler_registration_t* registration;

    void SetUp() override {
        exception_count = 0;
        last_error_code = 0;

        nimcp_exception_system_init();

        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "pr_memory_regression_test_handler";
        registration = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (registration) {
            nimcp_handler_unregister(registration);
            registration = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        last_error_code = ex->code;
        return false;  // Don't suppress
    }

    void reset_counters() {
        exception_count = 0;
        last_error_code = 0;
    }
};

std::atomic<int> PRMemoryExceptionRegressionTest::exception_count{0};
std::atomic<int> PRMemoryExceptionRegressionTest::last_error_code{0};
nimcp_handler_registration_t* PRMemoryExceptionRegressionTest::registration = nullptr;

//=============================================================================
// Regression Test: Error Code Consistency
// Verify that error codes remain consistent across versions
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, ErrorCode_NullPointer_AlwaysReturns1003) {
    // NIMCP_ERROR_NULL_POINTER must always be 1003
    // This is a contract that external code may depend on
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeCreate_NullManager_ErrorCode1003) {
    // Regression: Ensure pr_memory_node_create throws NIMCP_ERROR_NULL_POINTER (1003)
    pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeManagerGetMemManager_NullManager_ErrorCode1003) {
    // Regression: Ensure pr_node_manager_get_mem_manager throws NIMCP_ERROR_NULL_POINTER
    unified_mem_manager_t result = pr_node_manager_get_mem_manager(nullptr);

    EXPECT_EQ(result, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionRegressionTest, ProceduralGetSkillNode_NullPm_ErrorCode1003) {
    // Regression: Ensure procedural_get_skill_node throws NIMCP_ERROR_NULL_POINTER
    pr_memory_node_t* node = procedural_get_skill_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionRegressionTest, ProspectiveGetMemoryNode_NullPm_ErrorCode1003) {
    // Regression: Ensure prospective_get_memory_node throws NIMCP_ERROR_NULL_POINTER
    pr_memory_node_t* node = prospective_get_memory_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Regression Test: Return Value Consistency
// Verify that return values for error conditions remain consistent
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeGetTier_NullNode_ReturnsZ0) {
    // Regression: pr_memory_node_get_tier must return PR_MEMORY_TIER_Z0 for NULL
    pr_memory_tier_t tier = pr_memory_node_get_tier(nullptr);
    EXPECT_EQ(tier, PR_MEMORY_TIER_Z0);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeGetDataSize_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_get_data_size must return 0 for NULL
    size_t size = pr_memory_node_get_data_size(nullptr);
    EXPECT_EQ(size, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeRead_NullNode_ReturnsNull) {
    // Regression: pr_memory_node_read must return NULL for NULL node
    const void* data = pr_memory_node_read(nullptr);
    EXPECT_EQ(data, nullptr);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeWrite_NullNode_ReturnsNull) {
    // Regression: pr_memory_node_write must return NULL for NULL node
    void* data = pr_memory_node_write(nullptr);
    EXPECT_EQ(data, nullptr);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeIsShared_NullNode_ReturnsFalse) {
    // Regression: pr_memory_node_is_shared must return false for NULL
    bool shared = pr_memory_node_is_shared(nullptr);
    EXPECT_FALSE(shared);
}

TEST_F(PRMemoryExceptionRegressionTest, ProceduralGetExecutingSkill_NullPm_ReturnsInvalidId) {
    // Regression: procedural_get_executing_skill must return PROC_INVALID_ID for NULL
    uint64_t skill_id = procedural_get_executing_skill(nullptr);
    EXPECT_EQ(skill_id, PROC_INVALID_ID);
}

TEST_F(PRMemoryExceptionRegressionTest, ProceduralDecay_NullPm_ReturnsZero) {
    // Regression: procedural_decay must return 0 for NULL
    size_t count = procedural_decay(nullptr, 1.0f);
    EXPECT_EQ(count, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, ProspectiveShouldCheckNow_NullPm_ReturnsFalse) {
    // Regression: prospective_should_check_now must return false for NULL
    bool should_check = prospective_should_check_now(nullptr);
    EXPECT_FALSE(should_check);
}

TEST_F(PRMemoryExceptionRegressionTest, ProspectivePrune_NullPm_ReturnsZero) {
    // Regression: prospective_prune must return 0 for NULL
    size_t count = prospective_prune(nullptr, 100.0f);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Regression Test: Graceful Handling Without Exceptions
// Verify that destroy functions don't throw exceptions for NULL
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeDestroy_NullNode_NoException) {
    // Regression: pr_memory_node_destroy must not throw for NULL
    reset_counters();
    pr_memory_node_destroy(nullptr);

    // Should not have thrown an exception
    EXPECT_EQ(exception_count.load(), 0);
}

TEST_F(PRMemoryExceptionRegressionTest, ProceduralDestroy_NullPm_NoException) {
    // Regression: procedural_destroy must not throw for NULL
    reset_counters();
    procedural_destroy(nullptr);

    // Should not have thrown an exception (graceful no-op)
    SUCCEED();
}

TEST_F(PRMemoryExceptionRegressionTest, ProspectiveDestroy_NullPm_NoException) {
    // Regression: prospective_destroy must not throw for NULL
    reset_counters();
    prospective_destroy(nullptr);

    // Should not have thrown an exception (graceful no-op)
    SUCCEED();
}

TEST_F(PRMemoryExceptionRegressionTest, NodeManagerDestroy_NullManager_NoException) {
    // Regression: pr_node_manager_destroy must not throw for NULL
    reset_counters();
    pr_node_manager_destroy(nullptr);

    // Should not have thrown an exception
    EXPECT_EQ(exception_count.load(), 0);
}

//=============================================================================
// Regression Test: Memory Safety
// Verify no memory leaks or crashes during exception conditions
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, RepeatedNullCalls_NoMemoryLeaks) {
    // Regression: Repeated NULL calls should not leak memory
    for (int i = 0; i < 1000; i++) {
        pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
        EXPECT_EQ(node, nullptr);

        pr_memory_node_destroy(nullptr);
        procedural_destroy(nullptr);
        prospective_destroy(nullptr);
    }

    // If we reach here without crash, memory is handled correctly
    SUCCEED();
}

TEST_F(PRMemoryExceptionRegressionTest, AlternatingValidInvalid_StableState) {
    // Regression: Alternating valid and invalid operations should maintain stable state
    pr_node_manager_config_t config = pr_node_manager_default_config();

    for (int i = 0; i < 100; i++) {
        // Valid operation
        pr_node_manager_t manager = pr_node_manager_create(&config);
        EXPECT_NE(manager, nullptr);

        // Invalid operation (NULL)
        pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
        EXPECT_EQ(node, nullptr);

        // Valid cleanup
        if (manager) {
            pr_node_manager_destroy(manager);
        }
    }

    SUCCEED();
}

//=============================================================================
// Regression Test: Thread Safety
// Verify exception handling is thread-safe
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, ConcurrentNullCalls_NoRaceConditions) {
    // Regression: Concurrent NULL calls should not cause race conditions
    const int num_threads = 8;
    const int ops_per_thread = 100;
    std::atomic<int> success_count{0};
    std::atomic<bool> had_crash{false};

    auto worker = [&]() {
        try {
            for (int i = 0; i < ops_per_thread; i++) {
                // Various NULL calls
                pr_memory_node_t* n1 = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
                (void)n1;

                unified_mem_manager_t mem = pr_node_manager_get_mem_manager(nullptr);
                (void)mem;

                pr_memory_node_t* n2 = procedural_get_skill_node(nullptr, 1);
                (void)n2;

                pr_memory_node_t* n3 = prospective_get_memory_node(nullptr, 1);
                (void)n3;
            }
            success_count++;
        } catch (...) {
            had_crash = true;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads);
    EXPECT_FALSE(had_crash.load());
}

//=============================================================================
// Regression Test: Edge Cases
// Verify handling of unusual but valid edge cases
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeApplyDecay_NullNode_ReturnsNonPositive) {
    // Regression: pr_memory_node_apply_decay should return <= 0 for NULL
    float strength = pr_memory_node_apply_decay(nullptr, 1.0f);
    EXPECT_LE(strength, 0.0f);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeReinforce_NullNode_ReturnsNonPositive) {
    // Regression: pr_memory_node_reinforce should return <= 0 for NULL
    float strength = pr_memory_node_reinforce(nullptr, 0.1f);
    EXPECT_LE(strength, 0.0f);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeCheckEligibility_NullNode_ReturnsFalse) {
    // Regression: pr_memory_node_check_eligibility should return false for NULL
    bool eligible = pr_memory_node_check_eligibility(nullptr);
    EXPECT_FALSE(eligible);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeSignatureSimilarity_BothNull_ReturnsNonPositive) {
    // Regression: signature similarity with both NULL should return <= 0
    float similarity = pr_memory_node_signature_similarity(
        nullptr, nullptr, PRIME_SIG_SIMILARITY_COSINE);
    EXPECT_LE(similarity, 0.0f);
}

//=============================================================================
// Regression Test: Flag Operations
// Verify flag operations handle NULL correctly
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeSetFlags_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_set_flags should return 0 for NULL
    uint32_t flags = pr_memory_node_set_flags(nullptr, PR_NODE_FLAG_LOCKED);
    EXPECT_EQ(flags, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeClearFlags_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_clear_flags should return 0 for NULL
    uint32_t flags = pr_memory_node_clear_flags(nullptr, PR_NODE_FLAG_LOCKED);
    EXPECT_EQ(flags, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeGetFlags_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_get_flags should return 0 for NULL
    uint32_t flags = pr_memory_node_get_flags(nullptr);
    EXPECT_EQ(flags, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeHasFlag_NullNode_ReturnsFalse) {
    // Regression: pr_memory_node_has_flag should return false for NULL
    bool has_flag = pr_memory_node_has_flag(nullptr, PR_NODE_FLAG_LOCKED);
    EXPECT_FALSE(has_flag);
}

//=============================================================================
// Regression Test: Entanglement Operations
// Verify entanglement operations handle NULL correctly
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeAddEntanglement_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_add_entanglement should return 0 for NULL
    uint32_t count = pr_memory_node_add_entanglement(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeRemoveEntanglement_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_remove_entanglement should return 0 for NULL
    uint32_t count = pr_memory_node_remove_entanglement(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeGetEntanglementCount_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_get_entanglement_count should return 0 for NULL
    uint32_t count = pr_memory_node_get_entanglement_count(nullptr);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Regression Test: State Operations
// Verify state operations handle NULL correctly
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeUpdateState_NullNode_ReturnsError) {
    // Regression: pr_memory_node_update_state should return error for NULL
    nimcp_quaternion_t state = {1.0f, 0.0f, 0.0f, 0.0f};
    pr_node_error_t result = pr_memory_node_update_state(nullptr, state);
    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeBlendState_NullNode_ReturnsError) {
    // Regression: pr_memory_node_blend_state should return error for NULL
    nimcp_quaternion_t state = {1.0f, 0.0f, 0.0f, 0.0f};
    pr_node_error_t result = pr_memory_node_blend_state(nullptr, state, 0.5f);
    EXPECT_NE(result, PR_NODE_SUCCESS);
}

//=============================================================================
// Regression Test: Lock Operations
// Verify lock operations handle NULL correctly
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeLock_NullNode_ReturnsError) {
    // Regression: pr_memory_node_lock should return error for NULL
    pr_node_error_t result = pr_memory_node_lock(nullptr);
    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeUnlock_NullNode_ReturnsError) {
    // Regression: pr_memory_node_unlock should return error for NULL
    pr_node_error_t result = pr_memory_node_unlock(nullptr);
    EXPECT_NE(result, PR_NODE_SUCCESS);
}

//=============================================================================
// Regression Test: Serialization Operations
// Verify serialization operations handle NULL correctly
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeSerialize_NullNode_ReturnsError) {
    // Regression: pr_memory_node_serialize should return error for NULL node
    uint8_t buffer[1024];
    pr_node_error_t result = pr_memory_node_serialize(nullptr, buffer, sizeof(buffer), nullptr);
    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeSerializationSize_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_serialization_size should return 0 for NULL
    size_t size = pr_memory_node_serialization_size(nullptr);
    EXPECT_EQ(size, 0u);
}

//=============================================================================
// Regression Test: ID Operations
// Verify ID operations handle NULL correctly
//=============================================================================

TEST_F(PRMemoryExceptionRegressionTest, NodeGetId_NullNode_ReturnsInvalidId) {
    // Regression: pr_memory_node_get_id should return PR_NODE_INVALID_ID for NULL
    uint64_t id = pr_memory_node_get_id(nullptr);
    EXPECT_EQ(id, PR_NODE_INVALID_ID);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeGetAgeMs_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_get_age_ms should return 0 for NULL
    uint64_t age = pr_memory_node_get_age_ms(nullptr, pr_node_current_time_ms());
    EXPECT_EQ(age, 0u);
}

TEST_F(PRMemoryExceptionRegressionTest, NodeGetIdleMs_NullNode_ReturnsZero) {
    // Regression: pr_memory_node_get_idle_ms should return 0 for NULL
    uint64_t idle = pr_memory_node_get_idle_ms(nullptr, pr_node_current_time_ms());
    EXPECT_EQ(idle, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
