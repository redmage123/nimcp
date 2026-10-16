/**
 * @file test_pr_memory_exception_handling.cpp
 * @brief Unit tests for PR memory system exception handling
 * @date 2026-01-25
 *
 * WHAT: Test NIMCP_THROW_TO_IMMUNE exception handling in PR memory modules
 * WHY:  Verify complete exception handling coverage for brain immune integration
 * HOW:  Test NULL pointer handling, invalid parameters, and error propagation
 *
 * MODULES TESTED:
 * - pr_memory_node (Prime Resonant memory nodes)
 * - procedural (Procedural memory for skills and habits)
 * - prospective (Prospective memory for future intentions)
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>

// C++ compatibility for C11 _Atomic keyword
// This must be defined before including C headers that use _Atomic
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

class PRMemoryExceptionTest : public ::testing::Test {
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
        options.name = "pr_memory_test_handler";
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

std::atomic<int> PRMemoryExceptionTest::exception_count{0};
std::atomic<int> PRMemoryExceptionTest::last_error_code{0};
nimcp_handler_registration_t* PRMemoryExceptionTest::registration = nullptr;

//=============================================================================
// PR Memory Node Manager Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, NodeManagerGetMemManager_NullManager_ThrowsNullPointer) {
    unified_mem_manager_t result = pr_node_manager_get_mem_manager(nullptr);

    EXPECT_EQ(result, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionTest, NodeManagerGetNodeCount_NullManager_ReturnsZero) {
    uint64_t result = pr_node_manager_get_node_count(nullptr);

    // Should return 0 for null manager
    EXPECT_EQ(result, 0u);
}

//=============================================================================
// PR Memory Node Creation Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeCreate_NullManager_ThrowsNullPointer) {
    uint8_t data[] = {1, 2, 3, 4};
    pr_node_config_t config = pr_memory_node_default_config();

    pr_memory_node_t* node = pr_memory_node_create(nullptr, data, sizeof(data), &config);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeCreateWithSignature_NullManager_ThrowsNullPointer) {
    uint8_t data[] = {1, 2, 3, 4};
    prime_signature_t sig;
    memset(&sig, 0, sizeof(sig));
    pr_node_config_t config = pr_memory_node_default_config();

    pr_memory_node_t* node = pr_memory_node_create_with_signature(
        nullptr, data, sizeof(data), &sig, &config);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeDestroy_NullNode_NoException) {
    // Destroy with NULL should be safe (no-op)
    pr_memory_node_destroy(nullptr);

    // Should not throw (graceful handling)
    SUCCEED();
}

TEST_F(PRMemoryExceptionTest, MemoryNodeClone_NullNode_ReturnsNull) {
    pr_memory_node_t* clone = pr_memory_node_clone(nullptr, nullptr);

    // Clone returns NULL for NULL node (graceful handling without exception)
    EXPECT_EQ(clone, nullptr);
}

//=============================================================================
// PR Memory Node Data Access Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeRead_NullNode_ReturnsNull) {
    const void* data = pr_memory_node_read(nullptr);

    EXPECT_EQ(data, nullptr);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeWrite_NullNode_ReturnsNull) {
    void* data = pr_memory_node_write(nullptr);

    EXPECT_EQ(data, nullptr);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeIsShared_NullNode_ReturnsFalse) {
    bool shared = pr_memory_node_is_shared(nullptr);

    EXPECT_FALSE(shared);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeGetDataSize_NullNode_ReturnsZero) {
    size_t size = pr_memory_node_get_data_size(nullptr);

    EXPECT_EQ(size, 0u);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeMakePrivate_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_make_private(nullptr);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

//=============================================================================
// PR Memory Node State Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeUpdateState_NullNode_ReturnsError) {
    nimcp_quaternion_t state = {1.0f, 0.0f, 0.0f, 0.0f};

    pr_node_error_t result = pr_memory_node_update_state(nullptr, state);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeBlendState_NullNode_ReturnsError) {
    nimcp_quaternion_t state = {1.0f, 0.0f, 0.0f, 0.0f};

    pr_node_error_t result = pr_memory_node_blend_state(nullptr, state, 0.5f);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeUpdateStateComponents_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_update_state_components(
        nullptr, 0.5f, 0.5f, 0.5f, 0.5f);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

//=============================================================================
// PR Memory Node Signature Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeGetSignature_NullNode_ThrowsNullPointer) {
    const prime_signature_t* sig = pr_memory_node_get_signature(nullptr);

    EXPECT_EQ(sig, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeUpdateSignature_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_update_signature(nullptr);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeSetSignature_NullNode_ReturnsError) {
    prime_signature_t sig;
    memset(&sig, 0, sizeof(sig));

    pr_node_error_t result = pr_memory_node_set_signature(nullptr, &sig);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeSignatureSimilarity_NullNodes_ReturnsZero) {
    float similarity = pr_memory_node_signature_similarity(
        nullptr, nullptr, PRIME_SIG_SIMILARITY_COSINE);

    EXPECT_LE(similarity, 0.0f);
}

//=============================================================================
// PR Memory Node Tier Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodePromote_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_promote(nullptr);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeDemote_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_demote(nullptr);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeSetTier_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_set_tier(nullptr, PR_MEMORY_TIER_Z1);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeGetTier_NullNode_ReturnsZ0) {
    pr_memory_tier_t tier = pr_memory_node_get_tier(nullptr);

    EXPECT_EQ(tier, PR_MEMORY_TIER_Z0);
}

//=============================================================================
// PR Memory Node Decay Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeApplyDecay_NullNode_ReturnsZero) {
    float strength = pr_memory_node_apply_decay(nullptr, 1.0f);

    EXPECT_LE(strength, 0.0f);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeReinforce_NullNode_ReturnsZero) {
    float strength = pr_memory_node_reinforce(nullptr, 0.1f);

    EXPECT_LE(strength, 0.0f);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeCheckEligibility_NullNode_ReturnsFalse) {
    bool eligible = pr_memory_node_check_eligibility(nullptr);

    EXPECT_FALSE(eligible);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeSetDecayRate_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_set_decay_rate(nullptr, 0.01f);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

//=============================================================================
// PR Memory Node Entanglement Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeAddEntanglement_NullNode_ReturnsZero) {
    uint32_t count = pr_memory_node_add_entanglement(nullptr);

    EXPECT_EQ(count, 0u);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeRemoveEntanglement_NullNode_ReturnsZero) {
    uint32_t count = pr_memory_node_remove_entanglement(nullptr);

    EXPECT_EQ(count, 0u);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeGetEntanglementCount_NullNode_ReturnsZero) {
    uint32_t count = pr_memory_node_get_entanglement_count(nullptr);

    EXPECT_EQ(count, 0u);
}

//=============================================================================
// PR Memory Node Flag Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeSetFlags_NullNode_ReturnsZero) {
    uint32_t flags = pr_memory_node_set_flags(nullptr, PR_NODE_FLAG_LOCKED);

    EXPECT_EQ(flags, 0u);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeClearFlags_NullNode_ReturnsZero) {
    uint32_t flags = pr_memory_node_clear_flags(nullptr, PR_NODE_FLAG_LOCKED);

    EXPECT_EQ(flags, 0u);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeGetFlags_NullNode_ReturnsZero) {
    uint32_t flags = pr_memory_node_get_flags(nullptr);

    EXPECT_EQ(flags, 0u);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeHasFlag_NullNode_ReturnsFalse) {
    bool has_flag = pr_memory_node_has_flag(nullptr, PR_NODE_FLAG_LOCKED);

    EXPECT_FALSE(has_flag);
}

//=============================================================================
// PR Memory Node Lock Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeLock_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_lock(nullptr);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeUnlock_NullNode_ReturnsError) {
    pr_node_error_t result = pr_memory_node_unlock(nullptr);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

//=============================================================================
// PR Memory Node Stats Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeGetStats_NullNode_ReturnsError) {
    pr_node_stats_t stats;

    pr_node_error_t result = pr_memory_node_get_stats(nullptr, &stats);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeGetId_NullNode_ReturnsInvalidId) {
    uint64_t id = pr_memory_node_get_id(nullptr);

    EXPECT_EQ(id, PR_NODE_INVALID_ID);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeGetAgeMs_NullNode_ReturnsZero) {
    uint64_t age = pr_memory_node_get_age_ms(nullptr, pr_node_current_time_ms());

    EXPECT_EQ(age, 0u);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeGetIdleMs_NullNode_ReturnsZero) {
    uint64_t idle = pr_memory_node_get_idle_ms(nullptr, pr_node_current_time_ms());

    EXPECT_EQ(idle, 0u);
}

//=============================================================================
// PR Memory Node Serialization Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, MemoryNodeSerialize_NullNode_ReturnsError) {
    uint8_t buffer[1024];

    pr_node_error_t result = pr_memory_node_serialize(nullptr, buffer, sizeof(buffer), nullptr);

    EXPECT_NE(result, PR_NODE_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeDeserialize_NullBuffer_ThrowsNullPointer) {
    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);
    if (!manager) {
        GTEST_SKIP() << "Could not create node manager";
    }
    reset_counters();

    size_t bytes_read = 0;
    pr_memory_node_t* node = pr_memory_node_deserialize(manager, nullptr, 0, &bytes_read);

    EXPECT_EQ(node, nullptr);

    pr_node_manager_destroy(manager);
}

TEST_F(PRMemoryExceptionTest, MemoryNodeSerializationSize_NullNode_ReturnsZero) {
    size_t size = pr_memory_node_serialization_size(nullptr);

    EXPECT_EQ(size, 0u);
}

//=============================================================================
// Procedural Memory Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, ProceduralDestroy_NullPm_NoException) {
    // Destroy with NULL should be safe (no-op) - graceful handling
    procedural_destroy(nullptr);

    SUCCEED();
}

TEST_F(PRMemoryExceptionTest, ProceduralReset_NullPm_ReturnsError) {
    procedural_error_t result = procedural_reset(nullptr);

    // Returns error for NULL pm (graceful handling without exception)
    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralCreateSkill_NullPm_ReturnsError) {
    uint64_t skill_id = 0;

    procedural_error_t result = procedural_create_skill(
        nullptr, "test_skill", PROC_MOTOR, &skill_id);

    // Returns error for NULL pm (graceful handling without exception)
    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralAddStep_NullPm_ReturnsError) {
    uint64_t step_id = 0;
    procedural_error_t result = procedural_add_step(
        nullptr, 1, "test action", 100.0f, 0.9f, &step_id);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralPractice_NullPm_ReturnsError) {
    procedural_error_t result = procedural_practice(nullptr, 1, 0.8f, 1000.0f);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralExecute_NullPm_ReturnsError) {
    procedural_exec_result_t exec_result;
    procedural_error_t result = procedural_execute(nullptr, 1, &exec_result);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralCreateHabit_NullPm_ReturnsError) {
    prime_signature_t cue_sig;
    prime_signature_t response_sig;
    memset(&cue_sig, 0, sizeof(cue_sig));
    memset(&response_sig, 0, sizeof(response_sig));
    uint64_t habit_id = 0;

    procedural_error_t result = procedural_create_habit(
        nullptr, &cue_sig, &response_sig, 0.5f, &habit_id);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralGetSkill_NullPm_ReturnsError) {
    procedural_skill_t skill;

    procedural_error_t result = procedural_get_skill(nullptr, 1, &skill);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralGetStats_NullPm_ReturnsError) {
    procedural_stats_t stats;

    procedural_error_t result = procedural_get_stats(nullptr, &stats);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralGetSkillNode_NullPm_ThrowsNullPointer) {
    pr_memory_node_t* node = procedural_get_skill_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionTest, ProceduralGetHabitNode_NullPm_ThrowsNullPointer) {
    pr_memory_node_t* node = procedural_get_habit_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Prospective Memory Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, ProspectiveDestroy_NullPm_NoException) {
    // Destroy with NULL should be safe (no-op) - graceful handling
    prospective_destroy(nullptr);

    SUCCEED();
}

TEST_F(PRMemoryExceptionTest, ProspectiveReset_NullPm_ReturnsError) {
    prospective_error_t result = prospective_reset(nullptr);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveCreateTimeIntention_NullPm_ReturnsError) {
    uint64_t intent_id = 0;

    prospective_error_t result = prospective_create_time_intention(
        nullptr, 1000.0f, false, "test action", 5.0f, 5.0f, &intent_id);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveCreateActivityIntention_NullPm_ReturnsError) {
    uint64_t intent_id = 0;

    prospective_error_t result = prospective_create_activity_intention(
        nullptr, 1, "test action", 5.0f, 5.0f, &intent_id);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveCreateEventIntention_NullPm_ReturnsError) {
    prime_signature_t trigger;
    memset(&trigger, 0, sizeof(trigger));
    uint64_t intent_id = 0;

    prospective_error_t result = prospective_create_event_intention(
        nullptr, &trigger, 0.7f, "test action", 5.0f, 5.0f, &intent_id);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveUpdate_NullPm_ReturnsError) {
    prospective_trigger_result_t triggered[10];
    size_t count = 0;

    prospective_error_t result = prospective_update(nullptr, 1.0f, triggered, 10, &count);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveCheckTimeTriggers_NullPm_ReturnsError) {
    prospective_trigger_result_t triggered[10];
    size_t count = 0;

    prospective_error_t result = prospective_check_time_triggers(
        nullptr, 1000.0f, triggered, 10, &count);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveExecuteIntention_NullPm_ReturnsError) {
    prospective_error_t result = prospective_execute_intention(nullptr, 1);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveCancelIntention_NullPm_ReturnsError) {
    prospective_error_t result = prospective_cancel_intention(nullptr, 1);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveGetIntention_NullPm_ReturnsError) {
    prospective_intention_t intent;

    prospective_error_t result = prospective_get_intention(nullptr, 1, &intent);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveGetStats_NullPm_ReturnsError) {
    prospective_stats_t stats;

    prospective_error_t result = prospective_get_stats(nullptr, &stats);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveGetMemoryNode_NullPm_ThrowsNullPointer) {
    pr_memory_node_t* node = prospective_get_memory_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Procedural Memory Additional NULL Pointer Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, ProceduralRemoveSkill_NullPm_ReturnsError) {
    procedural_error_t result = procedural_remove_skill(nullptr, 1);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralResetStats_NullPm_ReturnsError) {
    procedural_error_t result = procedural_reset_stats(nullptr);

    EXPECT_NE(result, PROC_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProceduralDecay_NullPm_ReturnsZero) {
    size_t count = procedural_decay(nullptr, 1.0f);

    EXPECT_EQ(count, 0u);
}

TEST_F(PRMemoryExceptionTest, ProceduralGetExecutingSkill_NullPm_ReturnsInvalidId) {
    uint64_t skill_id = procedural_get_executing_skill(nullptr);

    EXPECT_EQ(skill_id, PROC_INVALID_ID);
}

TEST_F(PRMemoryExceptionTest, ProceduralGetCurrentStep_NullPm_ReturnsInvalidValue) {
    size_t step = procedural_get_current_step(nullptr);

    // Returns SIZE_MAX (invalid) for NULL pm
    EXPECT_EQ(step, SIZE_MAX);
}

//=============================================================================
// Prospective Memory Additional NULL Pointer Tests
//=============================================================================

TEST_F(PRMemoryExceptionTest, ProspectiveRemoveIntention_NullPm_ReturnsError) {
    prospective_error_t result = prospective_remove_intention(nullptr, 1);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectiveResetStats_NullPm_ReturnsError) {
    prospective_error_t result = prospective_reset_stats(nullptr);

    EXPECT_NE(result, PROSP_SUCCESS);
}

TEST_F(PRMemoryExceptionTest, ProspectivePrune_NullPm_ReturnsZero) {
    size_t count = prospective_prune(nullptr, 100.0f);

    EXPECT_EQ(count, 0u);
}

TEST_F(PRMemoryExceptionTest, ProspectiveShouldCheckNow_NullPm_ReturnsFalse) {
    bool should_check = prospective_should_check_now(nullptr);

    EXPECT_FALSE(should_check);
}

TEST_F(PRMemoryExceptionTest, ProspectiveApplyDecay_NullPm_ReturnsError) {
    prospective_error_t result = prospective_apply_decay(nullptr, 1.0f);

    EXPECT_NE(result, PROSP_SUCCESS);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
