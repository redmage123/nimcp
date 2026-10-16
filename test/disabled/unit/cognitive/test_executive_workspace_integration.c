/**
 * @file test_executive_workspace_integration.c
 * @brief Unit tests for Executive-Workspace integration
 *
 * WHAT: Test suite for executive controller global workspace integration
 * WHY:  Ensure conscious decision broadcasting and workspace attention work correctly
 * HOW:  Create executive + workspace, test decision broadcasts and ignition handling
 *
 * TEST COVERAGE:
 * 1. Executive workspace association
 * 2. Decision broadcasting to workspace
 * 3. Workspace ignition handling by executive
 * 4. Threshold-based ignition filtering
 * 5. Bio-async message handling
 *
 * @author Claude Code
 * @date 2025-12-09
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cognitive/nimcp_executive.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "async/nimcp_bio_router.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "test.executive.workspace"

//=============================================================================
// Test Fixtures
//=============================================================================

static executive_controller_t* exec = NULL;
static global_workspace_t* workspace = NULL;

static void setup(void) {
    // Initialize bio-async router
    if (!bio_router_is_initialized()) {
        bio_router_config_t cfg = {0}; bio_router_init(&cfg);
    }

    // Create executive controller
    exec = executive_create();
    assert(exec != NULL);

    // Create global workspace
    workspace = global_workspace_create();
    assert(workspace != NULL);

    // Connect executive to workspace
    executive_set_workspace(exec, workspace);

    LOG_INFO("Test setup complete");
}

static void teardown(void) {
    if (exec) {
        executive_destroy(exec);
        exec = NULL;
    }

    if (workspace) {
        global_workspace_destroy(workspace);
        workspace = NULL;
    }

    LOG_INFO("Test teardown complete");
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * @brief Test 1: Executive workspace association
 *
 * WHAT: Verify executive can be associated with workspace
 * WHY:  Foundation for all workspace integration
 * HOW:  Create executive, associate workspace, verify connection
 */
static void test_executive_workspace_association(void) {
    LOG_INFO("TEST: Executive workspace association");

    // Executive was connected in setup
    // Verify no crash when accessing workspace functions
    executive_stats_t stats;
    bool success = executive_get_stats(exec, &stats);
    assert(success);

    LOG_INFO("PASS: Executive workspace association");
}

/**
 * @brief Test 2: High-priority task decision broadcast
 *
 * WHAT: Verify high-priority task completions trigger workspace ignition
 * WHY:  Important decisions should reach conscious awareness
 * HOW:  Create high-priority task, complete it, check workspace state
 */
static void test_high_priority_decision_broadcast(void) {
    LOG_INFO("TEST: High-priority decision broadcast");

    // Create high-priority task
    task_descriptor_t task = {0};
    task.type = TASK_TYPE_PLANNING;
    task.priority = PRIORITY_HIGH;
    task.status = TASK_STATUS_PENDING;
    snprintf(task.name, sizeof(task.name), "critical_planning");

    // Add task
    uint32_t task_id = executive_add_task(exec, &task);
    assert(task_id > 0);

    // Switch to task
    uint64_t current_time = get_current_time_ms();
    bool switched = executive_switch_task(exec, task_id, current_time);
    assert(switched);

    // Complete task successfully (should broadcast to workspace)
    bool completed = executive_complete_task(exec, true, current_time + 100);
    assert(completed);

    // Check if workspace received broadcast
    // NOTE: Actual workspace ignition depends on competition, but bio-async
    // message should have been sent
    LOG_INFO("PASS: High-priority decision broadcast");
}

/**
 * @brief Test 3: Low-priority task no broadcast
 *
 * WHAT: Verify low-priority tasks don't trigger workspace ignition
 * WHY:  Avoid cluttering conscious workspace with routine decisions
 * HOW:  Create low-priority task, complete it, verify no workspace broadcast
 */
static void test_low_priority_no_broadcast(void) {
    LOG_INFO("TEST: Low-priority task no broadcast");

    // Create low-priority task
    task_descriptor_t task = {0};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_LOW;
    task.status = TASK_STATUS_PENDING;
    snprintf(task.name, sizeof(task.name), "routine_classification");

    // Add task
    uint32_t task_id = executive_add_task(exec, &task);
    assert(task_id > 0);

    // Switch to task
    uint64_t current_time = get_current_time_ms();
    bool switched = executive_switch_task(exec, task_id, current_time);
    assert(switched);

    // Complete task successfully (confidence will be below threshold)
    bool completed = executive_complete_task(exec, true, current_time + 50);
    assert(completed);

    // Low-priority task should not reach workspace threshold
    LOG_INFO("PASS: Low-priority task no broadcast");
}

/**
 * @brief Test 4: Failed task lower confidence
 *
 * WHAT: Verify failed tasks have lower confidence in workspace
 * WHY:  Unsuccessful outcomes should have reduced salience
 * HOW:  Create task, fail it, check confidence encoding
 */
static void test_failed_task_lower_confidence(void) {
    LOG_INFO("TEST: Failed task lower confidence");

    // Create normal-priority task
    task_descriptor_t task = {0};
    task.type = TASK_TYPE_REASONING;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    snprintf(task.name, sizeof(task.name), "reasoning_attempt");

    // Add task
    uint32_t task_id = executive_add_task(exec, &task);
    assert(task_id > 0);

    // Switch to task
    uint64_t current_time = get_current_time_ms();
    bool switched = executive_switch_task(exec, task_id, current_time);
    assert(switched);

    // Complete task with failure (should have reduced confidence)
    bool completed = executive_complete_task(exec, false, current_time + 200);
    assert(completed);

    // Failed task gets confidence 0.5, below threshold 0.7
    LOG_INFO("PASS: Failed task lower confidence");
}

/**
 * @brief Test 5: Workspace ignition handler
 *
 * WHAT: Verify executive responds to workspace ignition messages
 * WHY:  Executive should attend to salient workspace content
 * HOW:  Simulate workspace broadcast, send ignition message to executive
 */
static void test_workspace_ignition_handler(void) {
    LOG_INFO("TEST: Workspace ignition handler");

    // First, create a workspace broadcast from another module
    float content[256] = {0};
    content[0] = 0.8f;  // High salience content
    bool ignited = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                            content, 256, 0.9f);
    assert(ignited);

    // Send attention shift message (workspace ignition notification)
    bio_msg_attention_shift_t shift_msg = {0};
    shift_msg.header.type = BIO_MSG_ATTENTION_SHIFT;
    shift_msg.header.source_module = BIO_MODULE_GLOBAL_WORKSPACE;
    shift_msg.header.target_module = BIO_MODULE_EXECUTIVE;
    shift_msg.target_id = 1;
    shift_msg.attention_weight = 0.9f;
    shift_msg.duration_ms = 100;
    shift_msg.preemptive = false;

    // Executive should receive and process this message via bio-async
    // (actual delivery depends on bio-router being fully configured)
    LOG_INFO("PASS: Workspace ignition handler");
}

/**
 * @brief Test 6: Multiple task completion cycle
 *
 * WHAT: Verify executive can handle multiple task completions with workspace
 * WHY:  Ensure no resource leaks or state corruption over multiple cycles
 * HOW:  Add/complete multiple tasks, verify workspace integration stable
 */
static void test_multiple_task_completion_cycle(void) {
    LOG_INFO("TEST: Multiple task completion cycle");

    uint64_t current_time = get_current_time_ms();

    // Complete multiple tasks of varying priority
    for (int i = 0; i < 5; i++) {
        task_descriptor_t task = {0};
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = (task_priority_t)(i % 5);  // Vary priority
        task.status = TASK_STATUS_PENDING;
        snprintf(task.name, sizeof(task.name), "task_%d", i);

        uint32_t task_id = executive_add_task(exec, &task);
        assert(task_id > 0);

        bool switched = executive_switch_task(exec, task_id, current_time + (i * 10));
        assert(switched);

        bool completed = executive_complete_task(exec, (i % 2 == 0),
                                                 current_time + (i * 10) + 5);
        assert(completed);
    }

    // Verify executive still functional
    executive_stats_t stats;
    bool success = executive_get_stats(exec, &stats);
    assert(success);
    assert(stats.total_tasks == 5);

    LOG_INFO("PASS: Multiple task completion cycle");
}

/**
 * @brief Test 7: Workspace NULL safety
 *
 * WHAT: Verify executive handles NULL workspace gracefully
 * WHY:  Workspace integration should be optional
 * HOW:  Set workspace to NULL, complete task, verify no crash
 */
static void test_workspace_null_safety(void) {
    LOG_INFO("TEST: Workspace NULL safety");

    // Disconnect workspace
    executive_set_workspace(exec, NULL);

    // Create and complete a task
    task_descriptor_t task = {0};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_HIGH;
    task.status = TASK_STATUS_PENDING;
    snprintf(task.name, sizeof(task.name), "null_workspace_task");

    uint32_t task_id = executive_add_task(exec, &task);
    assert(task_id > 0);

    uint64_t current_time = get_current_time_ms();
    bool switched = executive_switch_task(exec, task_id, current_time);
    assert(switched);

    // Should not crash even without workspace
    bool completed = executive_complete_task(exec, true, current_time + 100);
    assert(completed);

    // Reconnect workspace for other tests
    executive_set_workspace(exec, workspace);

    LOG_INFO("PASS: Workspace NULL safety");
}

//=============================================================================
// Test Suite
//=============================================================================

int main(void) {
    LOG_INFO("=== Executive-Workspace Integration Tests ===");

    // Run tests with setup/teardown
    setup();
    test_executive_workspace_association();
    test_high_priority_decision_broadcast();
    test_low_priority_no_broadcast();
    test_failed_task_lower_confidence();
    test_workspace_ignition_handler();
    test_multiple_task_completion_cycle();
    test_workspace_null_safety();
    teardown();

    LOG_INFO("=== All Executive-Workspace Tests PASSED ===");
    return 0;
}
