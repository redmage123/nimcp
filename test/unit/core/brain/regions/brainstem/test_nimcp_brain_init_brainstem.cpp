/**
 * @file test_nimcp_brain_init_brainstem.cpp
 * @brief Unit tests for nimcp_brain_init_brainstem.c
 *
 * WHAT: Unit tests for the brainstem factory initialization
 * WHY:  Ensure correct brain integration of brainstem subsystem
 * HOW:  Test initialization, destruction, and update functions
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>

// Headers have their own extern "C" guards
#include "core/brain/factory/init/nimcp_brain_init_brainstem.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class BrainstemInitTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Create a minimal brain for testing
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.num_inputs = 4;
        config.num_outputs = 2;
        config.size = static_cast<brain_size_t>(BRAIN_SIZE_MICRO);
        config.task = static_cast<brain_task_t>(BRAIN_TASK_CLASSIFICATION);
        config.minimal_mode = true;

        brain = brain_create_custom(&config);
        // Note: brain_create_custom may or may not succeed depending on full initialization
        // For isolated testing, we may need a simpler approach
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// ============================================================================
// BASIC FUNCTION TESTS
// ============================================================================

TEST(BrainstemInitBasicTest, InitNullReturnsFalse) {
    EXPECT_FALSE(nimcp_brain_init_brainstem(NULL));
}

TEST(BrainstemInitBasicTest, DestroyNullDoesNotCrash) {
    nimcp_brain_destroy_brainstem(NULL);
    // Should not crash
}

TEST(BrainstemInitBasicTest, IsEnabledNullReturnsFalse) {
    EXPECT_FALSE(nimcp_brain_is_brainstem_enabled(NULL));
}

TEST(BrainstemInitBasicTest, ConnectThalamosNullReturnsFalse) {
    EXPECT_FALSE(nimcp_brain_connect_brainstem_thalamus(NULL));
}

TEST(BrainstemInitBasicTest, ConnectCerebellumNullReturnsFalse) {
    EXPECT_FALSE(nimcp_brain_connect_brainstem_cerebellum(NULL));
}

TEST(BrainstemInitBasicTest, UpdateNullReturnsTrue) {
    // NULL brain should return true (not an error if disabled)
    EXPECT_TRUE(nimcp_brain_update_brainstem(NULL, 0.1f));
}

// ============================================================================
// ISOLATED STRUCTURE TESTS
// ============================================================================

// Test with a manually created brain_struct to avoid full brain initialization
class BrainstemInitIsolatedTest : public ::testing::Test {
protected:
    struct brain_struct* brain;

    void SetUp() override {
        brain = (struct brain_struct*)nimcp_calloc(1, sizeof(struct brain_struct));
        ASSERT_NE(nullptr, brain);

        // Initialize minimal fields needed for brainstem init
        brain->brainstem = NULL;
        brain->brainstem_quantum_bridge = NULL;
        brain->brainstem_enabled = false;
        brain->last_brainstem_update_us = 0;

        // Initialize medulla (may be used by brainstem)
        brain->medulla = NULL;
        brain->medulla_enabled = false;

        // Initialize other fields that brainstem init may check
        brain->bio_async_enabled = false;
        brain->enable_event_broadcasting = false;
        brain->quantum_reasoning_enabled = false;
        brain->quantum_reasoner = NULL;
        brain->quantum_annealer = NULL;
        brain->current_time_us = 0;
    }

    void TearDown() override {
        if (brain) {
            // Clean up brainstem if it was created
            if (brain->brainstem_enabled) {
                nimcp_brain_destroy_brainstem(brain);
            }
            nimcp_free(brain);
            brain = nullptr;
        }
    }
};

TEST_F(BrainstemInitIsolatedTest, InitSuccess) {
    EXPECT_TRUE(nimcp_brain_init_brainstem(brain));
    EXPECT_TRUE(brain->brainstem_enabled);
    EXPECT_NE(nullptr, brain->brainstem);
}

TEST_F(BrainstemInitIsolatedTest, InitTwiceSucceeds) {
    EXPECT_TRUE(nimcp_brain_init_brainstem(brain));
    // Second init should succeed (already initialized)
    EXPECT_TRUE(nimcp_brain_init_brainstem(brain));
    EXPECT_TRUE(brain->brainstem_enabled);
}

TEST_F(BrainstemInitIsolatedTest, DestroyAfterInit) {
    ASSERT_TRUE(nimcp_brain_init_brainstem(brain));

    nimcp_brain_destroy_brainstem(brain);

    EXPECT_FALSE(brain->brainstem_enabled);
    EXPECT_EQ(nullptr, brain->brainstem);
    EXPECT_EQ(nullptr, brain->brainstem_quantum_bridge);
}

TEST_F(BrainstemInitIsolatedTest, IsEnabledAfterInit) {
    EXPECT_FALSE(nimcp_brain_is_brainstem_enabled(brain));

    ASSERT_TRUE(nimcp_brain_init_brainstem(brain));

    EXPECT_TRUE(nimcp_brain_is_brainstem_enabled(brain));
}

TEST_F(BrainstemInitIsolatedTest, UpdateAfterInit) {
    ASSERT_TRUE(nimcp_brain_init_brainstem(brain));

    // Update should succeed
    EXPECT_TRUE(nimcp_brain_update_brainstem(brain, 0.1f));
}

TEST_F(BrainstemInitIsolatedTest, UpdateWhenDisabled) {
    // Should return true (not an error when disabled)
    EXPECT_TRUE(nimcp_brain_update_brainstem(brain, 0.1f));
}

TEST_F(BrainstemInitIsolatedTest, ConnectThalamosAfterInit) {
    ASSERT_TRUE(nimcp_brain_init_brainstem(brain));
    EXPECT_TRUE(nimcp_brain_connect_brainstem_thalamus(brain));
}

TEST_F(BrainstemInitIsolatedTest, ConnectCerebellumAfterInit) {
    ASSERT_TRUE(nimcp_brain_init_brainstem(brain));
    EXPECT_TRUE(nimcp_brain_connect_brainstem_cerebellum(brain));
}

TEST_F(BrainstemInitIsolatedTest, ConnectThalamosBeforeInit) {
    // Should fail when brainstem not initialized
    EXPECT_FALSE(nimcp_brain_connect_brainstem_thalamus(brain));
}

TEST_F(BrainstemInitIsolatedTest, ConnectCerebellumBeforeInit) {
    EXPECT_FALSE(nimcp_brain_connect_brainstem_cerebellum(brain));
}

// ============================================================================
// MULTIPLE UPDATE TESTS
// ============================================================================

TEST_F(BrainstemInitIsolatedTest, MultipleUpdates) {
    ASSERT_TRUE(nimcp_brain_init_brainstem(brain));

    // Multiple updates should succeed
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(nimcp_brain_update_brainstem(brain, 0.01f));
        brain->current_time_us += 10000; // 10ms
    }
}

// ============================================================================
// INTEGRATION WITH EXISTING MEDULLA
// ============================================================================

TEST_F(BrainstemInitIsolatedTest, InitWithExistingMedulla) {
    // Create a medulla first
    brain->medulla = medulla_create(NULL);
    brain->medulla_enabled = (brain->medulla != NULL);

    if (brain->medulla_enabled) {
        medulla_start(brain->medulla);

        // Now init brainstem - it should use existing medulla
        EXPECT_TRUE(nimcp_brain_init_brainstem(brain));
        EXPECT_TRUE(brain->brainstem_enabled);

        // Clean up
        nimcp_brain_destroy_brainstem(brain);

        // Medulla should still exist (brainstem doesn't own it in this case)
        // Note: This depends on implementation - if brainstem uses external medulla,
        // it should not destroy it

        medulla_stop(brain->medulla);
        medulla_destroy(brain->medulla);
        brain->medulla = NULL;
        brain->medulla_enabled = false;
    }
}

// ============================================================================
// DESTROY IDEMPOTENCY TESTS
// ============================================================================

TEST_F(BrainstemInitIsolatedTest, DestroyTwice) {
    ASSERT_TRUE(nimcp_brain_init_brainstem(brain));

    nimcp_brain_destroy_brainstem(brain);
    nimcp_brain_destroy_brainstem(brain); // Should not crash

    EXPECT_FALSE(brain->brainstem_enabled);
}

TEST_F(BrainstemInitIsolatedTest, DestroyWithoutInit) {
    nimcp_brain_destroy_brainstem(brain);
    // Should not crash
    EXPECT_FALSE(brain->brainstem_enabled);
}
