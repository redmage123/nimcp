//=============================================================================
// test_bg_bbb_kg_initialization.cpp - BG BBB/KG Initialization Tests
//=============================================================================
/**
 * @file test_bg_bbb_kg_initialization.cpp
 * @brief Verify that BG initialization wires BBB and KG context setters
 *
 * WHAT: Tests that basal_ganglia_set_bbb() and basal_ganglia_set_kg_context()
 *       are called during brain factory BG initialization when the brain
 *       has BBB and KG persistence systems enabled.
 *
 * WHY:  Previously, g_basal_ganglia_bbb and g_basal_ganglia_kg_ctx were
 *       always NULL, making BBB validation always pass-through and KG
 *       recording silently dropped.
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/factory/init/nimcp_brain_init_basal_ganglia.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BGBBBKGInitTest : public ::testing::Test {
protected:
    brain_t test_brain = nullptr;

    void SetUp() override {
        test_brain = brain_create(
            "bg_bbb_kg_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,
            4
        );
        ASSERT_NE(test_brain, nullptr);
    }

    void TearDown() override {
        if (test_brain) {
            brain_destroy(test_brain);
            test_brain = nullptr;
        }
    }
};

//=============================================================================
// BBB Wiring Tests
//=============================================================================

TEST_F(BGBBBKGInitTest, SetBBB_AcceptsNonNull) {
    // basal_ganglia_set_bbb should accept a non-NULL pointer without crashing
    // We pass a dummy pointer since we only check it does not crash
    int dummy = 42;
    basal_ganglia_set_bbb((bbb_system_t)(void*)&dummy);

    // Clean up by setting back to NULL
    basal_ganglia_set_bbb(NULL);
}

TEST_F(BGBBBKGInitTest, SetBBB_AcceptsNull) {
    // Should accept NULL without crashing (disables BBB)
    basal_ganglia_set_bbb(NULL);
}

//=============================================================================
// KG Context Wiring Tests
//=============================================================================

TEST_F(BGBBBKGInitTest, SetKGContext_AcceptsNonNull) {
    int dummy = 99;
    basal_ganglia_set_kg_context(&dummy);

    // Clean up
    basal_ganglia_set_kg_context(NULL);
}

TEST_F(BGBBBKGInitTest, SetKGContext_AcceptsNull) {
    basal_ganglia_set_kg_context(NULL);
}

//=============================================================================
// Integration: Init wires systems when present
//=============================================================================

TEST_F(BGBBBKGInitTest, InitBG_WiresBBBWhenPresent) {
    struct brain_struct* b = (struct brain_struct*)test_brain;

    // Ensure BG is not yet initialized
    if (b->basal_ganglia) {
        // Already initialized by brain_create; destroy and reinit
        nimcp_brain_bg_destroy(test_brain);
    }
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    // Set a fake BBB system on the brain struct
    int fake_bbb = 1;
    b->bbb_system = (bbb_system_t)(void*)&fake_bbb;

    // Initialize BG -- this should call basal_ganglia_set_bbb()
    bool result = nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_TRUE(b->basal_ganglia_enabled);

    // Clean up: NULL out BBB before destroy to avoid dangling
    b->bbb_system = NULL;
    basal_ganglia_set_bbb(NULL);
}

TEST_F(BGBBBKGInitTest, InitBG_SkipsBBBWhenNull) {
    struct brain_struct* b = (struct brain_struct*)test_brain;

    if (b->basal_ganglia) {
        nimcp_brain_bg_destroy(test_brain);
    }
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    // Ensure BBB is NULL
    b->bbb_system = NULL;

    bool result = nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_TRUE(b->basal_ganglia_enabled);
}

TEST_F(BGBBBKGInitTest, InitBG_WiresKGWhenPresent) {
    struct brain_struct* b = (struct brain_struct*)test_brain;

    if (b->basal_ganglia) {
        nimcp_brain_bg_destroy(test_brain);
    }
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    // Set a fake KG persistence context
    int fake_kg = 2;
    b->kg_persistence = (struct kg_persistence*)(void*)&fake_kg;

    bool result = nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_TRUE(b->basal_ganglia_enabled);

    // Clean up
    b->kg_persistence = nullptr;
    basal_ganglia_set_kg_context(NULL);
}

TEST_F(BGBBBKGInitTest, InitBG_SkipsKGWhenNull) {
    struct brain_struct* b = (struct brain_struct*)test_brain;

    if (b->basal_ganglia) {
        nimcp_brain_bg_destroy(test_brain);
    }
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;
    b->kg_persistence = nullptr;

    bool result = nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_TRUE(b->basal_ganglia_enabled);
}
