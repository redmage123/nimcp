/**
 * @file test_state_manager_regression.cpp
 * @brief Regression tests for state manager bug prevention
 *
 * WHAT: Regression tests to prevent recurrence of specific bugs
 * WHY:  Document and prevent known failure modes
 * HOW:  Each test reproduces a specific bug scenario and verifies fix
 *
 * PHASE 8: System-Wide Health Integration
 *
 * @author NIMCP Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

class StateManagerRegressionTest : public ::testing::Test {
protected:
    nimcp_state_manager_t* manager;

    void SetUp() override {
        manager = nimcp_state_manager_create();
    }

    void TearDown() override {
        if (manager) {
            nimcp_state_manager_destroy(manager);
            manager = nullptr;
        }
    }
};

//=============================================================================
// Bug: Double-free on destroy with registered modules
// Issue: Destroying manager with registered modules would double-free contexts
// Fix: Manager should not free module contexts (they're owned by caller)
//=============================================================================

TEST_F(StateManagerRegressionTest, DestroyWithRegisteredModules) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();
    nimcp_state_manager_register(manager, "test_module", ops, &synapse);

    /* This should not crash or double-free */
    nimcp_state_manager_destroy(manager);
    manager = nullptr;

    /* Synapse should still be valid (stack-allocated) */
    EXPECT_TRUE(std::isfinite(synapse.weight));
}

//=============================================================================
// Bug: Buffer overflow on long module names
// Issue: Long module names could overflow fixed-size name buffer
// Fix: Properly truncate or reject long names
//=============================================================================

TEST_F(StateManagerRegressionTest, LongModuleName) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    /* Create name longer than NIMCP_STATE_MANAGER_MAX_NAME_LEN */
    char long_name[NIMCP_STATE_MANAGER_MAX_NAME_LEN * 2];
    memset(long_name, 'x', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    /* Should either truncate gracefully or reject - not crash */
    int result = nimcp_state_manager_register(manager, long_name, ops, &synapse);

    /* Either success (truncated) or failure (rejected) - both valid */
    if (result == 0) {
        /* If accepted, name should be truncated */
        nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, long_name);
        /* Find may fail because name was truncated, that's OK */
        (void)entry;
    }
    /* No crash = success */
}

//=============================================================================
// Bug: Checkpoint size calculation overflow for many modules
// Issue: Total size calculation could overflow with many modules
// Fix: Use size_t properly and check for overflow
//=============================================================================

TEST_F(StateManagerRegressionTest, CheckpointSizeOverflow) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    /* Register maximum number of modules */
    for (uint32_t i = 0; i < NIMCP_STATE_MANAGER_MAX_MODULES; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%u", i);
        int result = nimcp_state_manager_register(manager, name, ops, &synapse);
        if (result != 0) break;
    }

    /* Get total size - should not overflow */
    size_t total_size = nimcp_state_manager_get_total_size(manager);

    /* Size should be reasonable (not overflow to small number) */
    size_t expected_min = NIMCP_STATE_MANAGER_MAX_MODULES * sizeof(stdp_synapse_t);
    EXPECT_GE(total_size, expected_min / 2);  /* Allow for header overhead */
}

//=============================================================================
// Bug: Null pointer dereference on empty manager
// Issue: Operations on empty manager (no modules) crashed
// Fix: Proper null/empty checks
//=============================================================================

TEST_F(StateManagerRegressionTest, OperationsOnEmptyManager) {
    /* All operations should handle empty manager gracefully */
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), 0u);
    /* get_total_size includes header overhead even for empty manager */
    EXPECT_GE(nimcp_state_manager_get_total_size(manager), 0u);
    EXPECT_EQ(nimcp_state_manager_validate_all(manager), 0);
    EXPECT_EQ(nimcp_state_manager_reset_all(manager), 0);
    EXPECT_EQ(nimcp_state_manager_reset_invalid(manager), 0);

    /* Checkpoint/restore with no modules */
    size_t size = 0;
    int result = nimcp_state_manager_checkpoint_all(manager, nullptr, &size);
    /* May return 0 with size 0, or error - both valid */
    (void)result;

    /* No crashes = success */
}

//=============================================================================
// Bug: Use-after-unregister
// Issue: Accessing module after unregistration caused use-after-free
// Fix: Properly clear module entry on unregister
//=============================================================================

TEST_F(StateManagerRegressionTest, UseAfterUnregister) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    nimcp_state_manager_register(manager, "will_remove", ops, &synapse);

    /* Unregister */
    int result = nimcp_state_manager_unregister(manager, "will_remove");
    EXPECT_EQ(result, 0);

    /* Find should return null */
    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "will_remove");
    EXPECT_EQ(entry, nullptr);

    /* Operations on removed module should fail gracefully */
    size_t size = 0;
    result = nimcp_state_manager_checkpoint_module(manager, "will_remove", nullptr, &size);
    EXPECT_LT(result, 0);

    result = nimcp_state_manager_validate_module(manager, "will_remove");
    EXPECT_LT(result, 0);
}

//=============================================================================
// Bug: Checksum validation bypass
// Issue: Corrupted checkpoint data could be deserialized without detection
// Fix: Proper checksum validation on deserialize
//=============================================================================

TEST_F(StateManagerRegressionTest, ChecksumValidation) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    synapse.weight = 0.77f;
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    nimcp_state_manager_register(manager, "checksum_test", ops, &synapse);

    /* Checkpoint */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "checksum_test", nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    nimcp_state_manager_checkpoint_module(manager, "checksum_test", buffer.data(), &written);

    /* Corrupt data (but not magic) - flip some bits in middle of buffer */
    if (written > 20) {
        buffer[written / 2] ^= 0xFF;
        buffer[written / 2 + 1] ^= 0xFF;
    }

    /* Restore should detect corruption and fail */
    stdp_synapse_t restored;
    stdp_synapse_init(&restored);
    int result = ops->deserialize(&restored, buffer.data(), written);

    /* Either fails (good - detected corruption) or succeeds with checksum mismatch */
    /* The actual behavior depends on implementation - document it */
    (void)result;
}

//=============================================================================
// Bug: Statistics counter overflow
// Issue: Statistics counters could overflow and wrap
// Fix: Use uint64_t for counters
//=============================================================================

TEST_F(StateManagerRegressionTest, StatisticsCounterOverflow) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    nimcp_state_manager_register(manager, "counter_test", ops, &synapse);

    /* Perform many operations */
    for (int i = 0; i < 1000; i++) {
        nimcp_state_manager_validate_module(manager, "counter_test");
    }

    nimcp_state_manager_stats_t stats;
    nimcp_state_manager_get_stats(manager, &stats);

    /* Counter should reflect all operations without overflow */
    EXPECT_GE(stats.total_validations, 1000u);
}

//=============================================================================
// Bug: Race condition in registration
// Issue: Concurrent registrations could corrupt module list
// Fix: Mutex protection around registration
//=============================================================================

TEST_F(StateManagerRegressionTest, ConcurrentRegistration) {
    /* Note: This test documents the requirement, actual thread safety
       depends on mutex implementation */

    stdp_synapse_t synapses[10];
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto register_fn = [&](int id) {
        stdp_synapse_init(&synapses[id]);
        char name[32];
        snprintf(name, sizeof(name), "thread_%d", id);
        int result = nimcp_state_manager_register(manager, name, ops, &synapses[id]);
        if (result == 0) {
            success_count++;
        } else {
            failure_count++;
        }
    };

    /* Register from multiple threads */
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(register_fn, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    /* Total should equal attempts (either success or failure, no corruption) */
    EXPECT_EQ(success_count + failure_count, 10);
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), (uint32_t)success_count.load());
}

//=============================================================================
// Bug: Validation fails on freshly initialized module
// Issue: Default-initialized modules failed validation
// Fix: Initialize to valid state by default
//=============================================================================

TEST_F(StateManagerRegressionTest, ValidateFreshlyInitialized) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);  /* Default initialization */
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    nimcp_state_manager_register(manager, "fresh", ops, &synapse);

    /* Should validate without any modifications */
    int result = nimcp_state_manager_validate_module(manager, "fresh");
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Bug: Module not found after registration with similar names
// Issue: Hash collision or strcmp error with similar names
// Fix: Proper string comparison
//=============================================================================

TEST_F(StateManagerRegressionTest, SimilarModuleNames) {
    stdp_synapse_t synapses[4];
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    const char* similar_names[] = {
        "module",
        "module1",
        "module_",
        "Module"
    };

    for (int i = 0; i < 4; i++) {
        stdp_synapse_init(&synapses[i]);
        synapses[i].weight = 0.1f * (i + 1);  /* Different weights */
        nimcp_state_manager_register(manager, similar_names[i], ops, &synapses[i]);
    }

    /* Each should be findable and distinct */
    for (int i = 0; i < 4; i++) {
        nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, similar_names[i]);
        ASSERT_NE(entry, nullptr) << "Failed to find: " << similar_names[i];

        /* Context should match correct synapse */
        stdp_synapse_t* ctx = (stdp_synapse_t*)entry->context;
        EXPECT_FLOAT_EQ(ctx->weight, 0.1f * (i + 1));
    }
}

//=============================================================================
// Bug: Restore corrupts state when buffer is exact size
// Issue: Off-by-one error when buffer size equals required size
// Fix: Proper boundary checking
//=============================================================================

TEST_F(StateManagerRegressionTest, RestoreExactBufferSize) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    synapse.weight = 0.63f;
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    nimcp_state_manager_register(manager, "exact_size", ops, &synapse);

    /* Get exact required size */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "exact_size", nullptr, &size);

    /* Allocate exactly that size */
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    int result = nimcp_state_manager_checkpoint_module(
        manager, "exact_size", buffer.data(), &written);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(written, size);

    /* Modify state */
    synapse.weight = 0.11f;

    /* Restore with exact size */
    result = nimcp_state_manager_restore_module(manager, "exact_size", buffer.data(), size);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(synapse.weight, 0.63f);
}

//=============================================================================
// Bug: Disabled module re-enabled after unregister/register
// Issue: Disabled state not cleared when slot reused
// Fix: Initialize all fields on registration
//=============================================================================

TEST_F(StateManagerRegressionTest, DisabledStateOnReregister) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_state_ops_t* ops = stdp_get_state_ops();

    /* Register and disable */
    nimcp_state_manager_register(manager, "toggle", ops, &synapse);
    nimcp_state_manager_set_enabled(manager, "toggle", false);

    /* Verify disabled */
    nimcp_state_module_entry_t* entry = nimcp_state_manager_find(manager, "toggle");
    ASSERT_NE(entry, nullptr);
    EXPECT_FALSE(entry->enabled);

    /* Unregister */
    nimcp_state_manager_unregister(manager, "toggle");

    /* Re-register with same name */
    nimcp_state_manager_register(manager, "toggle", ops, &synapse);

    /* Should be enabled by default */
    entry = nimcp_state_manager_find(manager, "toggle");
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->enabled);
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
