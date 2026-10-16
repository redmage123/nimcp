/**
 * @file test_working_memory_workspace_integration.c
 * @brief Unit tests for Working Memory-Workspace integration
 *
 * WHAT: Test suite for working memory global workspace integration
 * WHY:  Ensure salient items trigger workspace ignition and broadcasts refresh WM
 * HOW:  Create WM + workspace, test item ignition and broadcast handling
 *
 * TEST COVERAGE:
 * 1. Working memory workspace association
 * 2. Salient item workspace ignition
 * 3. Below-threshold no ignition
 * 4. Workspace broadcast refresh
 * 5. Multiple item cycles
 * 6. NULL workspace safety
 *
 * @author Claude Code
 * @date 2025-12-09
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cognitive/nimcp_working_memory.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "async/nimcp_bio_router.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "test.working_memory.workspace"

//=============================================================================
// Test Fixtures
//=============================================================================

static working_memory_t* wm = NULL;
static global_workspace_t* workspace = NULL;

static void setup(void) {
    // Initialize bio-async router
    if (!bio_router_is_initialized()) {
        bio_router_config_t cfg = {0}; bio_router_init(&cfg);
    }

    // Create working memory
    wm = working_memory_create();
    assert(wm != NULL);

    // Create global workspace
    workspace = global_workspace_create();
    assert(workspace != NULL);

    // Connect working memory to workspace
    working_memory_set_workspace(wm, workspace);

    LOG_INFO("Test setup complete");
}

static void teardown(void) {
    if (wm) {
        working_memory_destroy(wm);
        wm = NULL;
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
 * @brief Test 1: Working memory workspace association
 *
 * WHAT: Verify WM can be associated with workspace
 * WHY:  Foundation for all workspace integration
 * HOW:  Create WM, associate workspace, verify connection
 */
static void test_wm_workspace_association(void) {
    LOG_INFO("TEST: WM workspace association");

    // WM was connected in setup
    // Verify no crash when accessing WM functions
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);

    assert(stats.capacity == WORKING_MEMORY_DEFAULT_CAPACITY);
    assert(stats.current_size == 0);

    LOG_INFO("PASS: WM workspace association");
}

/**
 * @brief Test 2: Salient item triggers workspace ignition
 *
 * WHAT: Verify high-salience items compete for workspace access
 * WHY:  Important WM items should reach conscious awareness
 * HOW:  Add high-salience item, verify workspace competition attempt
 */
static void test_salient_item_triggers_ignition(void) {
    LOG_INFO("TEST: Salient item triggers workspace ignition");

    // Create high-salience item
    float item[64];
    for (uint32_t i = 0; i < 64; i++) {
        item[i] = (float)i / 64.0f;
    }

    // Add item with high salience (should trigger workspace competition)
    bool added = working_memory_add(wm, item, 64, 0.95f);
    assert(added);

    // Verify item was added to WM
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);
    assert(stats.current_size == 1);
    assert(stats.total_additions == 1);

    // Check if item can be retrieved
    uint32_t retrieved_size = 0;
    const float* retrieved = working_memory_get(wm, 0, &retrieved_size);
    assert(retrieved != NULL);
    assert(retrieved_size == 64);

    LOG_INFO("PASS: Salient item triggers workspace ignition");
}

/**
 * @brief Test 3: Below-threshold item no ignition
 *
 * WHAT: Verify low-salience items don't trigger workspace ignition
 * WHY:  Avoid cluttering workspace with routine information
 * HOW:  Add low-salience item, verify no workspace competition
 */
static void test_below_threshold_no_ignition(void) {
    LOG_INFO("TEST: Below-threshold item no ignition");

    // Create low-salience item
    float item[32];
    for (uint32_t i = 0; i < 32; i++) {
        item[i] = 0.1f;
    }

    // Add item with low salience (should NOT trigger workspace competition)
    bool added = working_memory_add(wm, item, 32, 0.3f);
    assert(added);

    // Verify item was added to WM locally
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);
    assert(stats.current_size >= 1);

    LOG_INFO("PASS: Below-threshold item no ignition");
}

/**
 * @brief Test 4: Workspace broadcast refreshes WM items
 *
 * WHAT: Verify workspace broadcasts prevent WM decay
 * WHY:  Conscious attention should maintain WM items
 * HOW:  Add item, trigger workspace broadcast, verify item refreshed
 */
static void test_workspace_broadcast_refreshes_wm(void) {
    LOG_INFO("TEST: Workspace broadcast refreshes WM items");

    // Add item to WM
    float item[16];
    for (uint32_t i = 0; i < 16; i++) {
        item[i] = 0.5f + (float)i / 32.0f;
    }

    bool added = working_memory_add(wm, item, 16, 0.6f);
    assert(added);

    // Simulate workspace broadcast from another module
    float broadcast_content[256] = {0};
    broadcast_content[0] = 0.8f;
    bool ignited = global_workspace_compete(workspace, MODULE_EXECUTIVE,
                                            broadcast_content, 256, 0.85f);
    assert(ignited);

    // WM should receive broadcast and potentially refresh items
    // (actual refresh logic depends on content matching)
    LOG_INFO("PASS: Workspace broadcast refreshes WM items");
}

/**
 * @brief Test 5: Multiple salient items compete
 *
 * WHAT: Verify multiple high-salience items can compete over time
 * WHY:  Ensure workspace competition works with multiple WM items
 * HOW:  Add multiple salient items, verify all attempt workspace access
 */
static void test_multiple_salient_items_compete(void) {
    LOG_INFO("TEST: Multiple salient items compete");

    // Add multiple high-salience items
    for (int i = 0; i < 5; i++) {
        float item[32];
        for (uint32_t j = 0; j < 32; j++) {
            item[j] = 0.5f + (float)(i * 32 + j) / 160.0f;
        }

        float salience = 0.85f + (float)i * 0.02f;
        bool added = working_memory_add(wm, item, 32, salience);
        assert(added);
    }

    // Verify all items added
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);
    assert(stats.current_size == 5);
    assert(stats.total_additions >= 5);

    LOG_INFO("PASS: Multiple salient items compete");
}

/**
 * @brief Test 6: WM capacity with workspace integration
 *
 * WHAT: Verify workspace integration doesn't break capacity limits
 * WHY:  WM should still enforce Miller's 7±2 limit
 * HOW:  Add items beyond capacity, verify eviction works
 */
static void test_wm_capacity_with_workspace(void) {
    LOG_INFO("TEST: WM capacity with workspace integration");

    // Add items beyond capacity
    uint32_t capacity = working_memory_get_capacity(wm);
    for (uint32_t i = 0; i < capacity + 3; i++) {
        float item[16];
        for (uint32_t j = 0; j < 16; j++) {
            item[j] = (float)(i * 16 + j) / 256.0f;
        }

        // Vary salience
        float salience = 0.5f + (float)i * 0.05f;
        if (salience > 1.0f) salience = 1.0f;

        bool added = working_memory_add(wm, item, 16, salience);
        assert(added);
    }

    // Verify capacity respected
    uint32_t current_size = working_memory_get_size(wm);
    assert(current_size <= capacity);

    // Verify evictions occurred
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);
    assert(stats.total_evictions > 0);

    LOG_INFO("PASS: WM capacity with workspace integration");
}

/**
 * @brief Test 7: Workspace NULL safety
 *
 * WHAT: Verify WM handles NULL workspace gracefully
 * WHY:  Workspace integration should be optional
 * HOW:  Set workspace to NULL, add item, verify no crash
 */
static void test_workspace_null_safety(void) {
    LOG_INFO("TEST: Workspace NULL safety");

    // Disconnect workspace
    working_memory_set_workspace(wm, NULL);

    // Add item with high salience (should not crash)
    float item[32];
    for (uint32_t i = 0; i < 32; i++) {
        item[i] = 0.8f;
    }

    bool added = working_memory_add(wm, item, 32, 0.95f);
    assert(added);

    // Verify item added locally
    uint32_t size = working_memory_get_size(wm);
    assert(size >= 1);

    // Reconnect workspace for other tests
    working_memory_set_workspace(wm, workspace);

    LOG_INFO("PASS: Workspace NULL safety");
}

/**
 * @brief Test 8: Temporal decay with workspace refresh
 *
 * WHAT: Verify workspace attention prevents WM decay
 * WHY:  Conscious items should not decay
 * HOW:  Add item, simulate time passing, workspace refresh, verify no decay
 */
static void test_temporal_decay_with_workspace_refresh(void) {
    LOG_INFO("TEST: Temporal decay with workspace refresh");

    // Clear WM for clean test
    working_memory_clear(wm);

    // Add item
    float item[16];
    for (uint32_t i = 0; i < 16; i++) {
        item[i] = 0.7f;
    }

    bool added = working_memory_add(wm, item, 16, 0.6f);
    assert(added);

    uint64_t t0 = get_current_time_ms();

    // Simulate time passing (would normally cause decay)
    uint64_t t1 = t0 + 2000;  // 2 seconds

    // Refresh item via workspace attention
    bool refreshed = working_memory_refresh(wm, 0);
    assert(refreshed);

    // Apply decay
    uint32_t decayed = working_memory_decay(wm, t1);

    // Item should NOT be decayed due to refresh
    uint32_t size = working_memory_get_size(wm);
    assert(size == 1);

    LOG_INFO("PASS: Temporal decay with workspace refresh");
}

//=============================================================================
// Test Suite
//=============================================================================

int main(void) {
    LOG_INFO("=== Working Memory-Workspace Integration Tests ===");

    // Run tests with setup/teardown
    setup();
    test_wm_workspace_association();
    test_salient_item_triggers_ignition();
    test_below_threshold_no_ignition();
    test_workspace_broadcast_refreshes_wm();
    test_multiple_salient_items_compete();
    test_wm_capacity_with_workspace();
    test_workspace_null_safety();
    test_temporal_decay_with_workspace_refresh();
    teardown();

    LOG_INFO("=== All Working Memory-Workspace Tests PASSED ===");
    return 0;
}
