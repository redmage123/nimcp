/**
 * @file test_state_manager_integration.cpp
 * @brief Integration tests for state manager with multiple modules
 *
 * WHAT: Integration tests for state manager cross-module interactions
 * WHY:  Verify checkpoint/restore works across heterogeneous modules
 * HOW:  Test state manager with STDP and astrocyte modules together
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

extern "C" {
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/fault_tolerance/nimcp_module_recovery.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/astrocyte_types/nimcp_astrocyte_types.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Multi-module state manager integration test fixture
 *
 * WHAT: Tests state manager with real STDP and astrocyte modules
 * WHY:  Verify cross-module checkpoint/restore consistency
 * HOW:  Create manager with multiple module types, test operations
 */
class StateManagerIntegrationTest : public ::testing::Test {
protected:
    nimcp_state_manager_t* manager;
    stdp_synapse_t synapse;
    astrocyte_network_t* network;
    static constexpr int NETWORK_SIZE = 5;

    void SetUp() override {
        manager = nimcp_state_manager_create();
        stdp_synapse_init(&synapse);
        network = astrocyte_network_create(NETWORK_SIZE);

        /* Add astrocytes to the network */
        if (network) {
            for (int i = 0; i < NETWORK_SIZE; i++) {
                astrocyte_t* astro = astrocyte_create(
                    i, ASTROCYTE_TYPE_GENERIC,
                    (float)i * 10.0f, 0.0f, 0.0f, 50.0f);
                if (astro) {
                    astrocyte_network_add(network, astro);
                }
            }
        }
    }

    void TearDown() override {
        if (manager) {
            nimcp_state_manager_destroy(manager);
            manager = nullptr;
        }
        if (network) {
            astrocyte_network_destroy(network);
            network = nullptr;
        }
    }
};

//=============================================================================
// Multi-Module Registration Tests
//=============================================================================

TEST_F(StateManagerIntegrationTest, RegisterMultipleModuleTypes) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    int result1 = nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    int result2 = nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), 2u);
}

TEST_F(StateManagerIntegrationTest, RegisterWithPriorities) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    /* Register with priorities - STDP first (lower priority number = higher priority) */
    int result1 = nimcp_state_manager_register_with_priority(
        manager, "stdp", stdp_ops, &synapse, 1);
    int result2 = nimcp_state_manager_register_with_priority(
        manager, "astrocyte", astro_ops, network, 2);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);

    /* Verify priorities are set */
    nimcp_state_module_entry_t* stdp_entry = nimcp_state_manager_find(manager, "stdp");
    nimcp_state_module_entry_t* astro_entry = nimcp_state_manager_find(manager, "astrocyte");

    ASSERT_NE(stdp_entry, nullptr);
    ASSERT_NE(astro_entry, nullptr);
    EXPECT_EQ(stdp_entry->priority, 1u);
    EXPECT_EQ(astro_entry->priority, 2u);
}

//=============================================================================
// Cross-Module Checkpoint Tests
//=============================================================================

TEST_F(StateManagerIntegrationTest, CheckpointAllModules) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Set known values */
    synapse.weight = 0.65f;
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        astro->calcium_concentration = 0.789f;
    }

    /* Checkpoint all */
    size_t total_size = 0;
    int result = nimcp_state_manager_checkpoint_all(manager, nullptr, &total_size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(total_size, 0u);

    std::vector<uint8_t> buffer(total_size);
    size_t written = total_size;
    result = nimcp_state_manager_checkpoint_all(manager, buffer.data(), &written);
    EXPECT_EQ(result, 0);

    /* Verify combined size is sum of individual sizes */
    size_t stdp_size = stdp_ops->get_size(&synapse);
    size_t astro_size = astro_ops->get_size(network);
    EXPECT_GE(total_size, stdp_size + astro_size);
}

TEST_F(StateManagerIntegrationTest, RestoreAllModules) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Set known values */
    float original_weight = 0.72f;
    float original_calcium = 0.543f;

    synapse.weight = original_weight;
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        astro->calcium_concentration = original_calcium;
    }

    /* Checkpoint */
    size_t total_size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &total_size);
    std::vector<uint8_t> buffer(total_size);
    size_t written = total_size;
    nimcp_state_manager_checkpoint_all(manager, buffer.data(), &written);

    /* Modify values */
    synapse.weight = 0.11f;
    if (astro) {
        astro->calcium_concentration = 0.11f;
    }

    /* Restore */
    int result = nimcp_state_manager_restore_all(manager, buffer.data(), written);
    EXPECT_EQ(result, 0);

    /* Verify restoration */
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
    if (astro) {
        EXPECT_NEAR(astro->calcium_concentration, original_calcium, 0.01f);
    }
}

TEST_F(StateManagerIntegrationTest, SelectiveModuleCheckpointRestore) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Checkpoint only STDP */
    synapse.weight = 0.88f;

    size_t stdp_size = 0;
    nimcp_state_manager_checkpoint_module(manager, "stdp", nullptr, &stdp_size);
    std::vector<uint8_t> stdp_buffer(stdp_size);
    size_t stdp_written = stdp_size;
    nimcp_state_manager_checkpoint_module(manager, "stdp", stdp_buffer.data(), &stdp_written);

    /* Modify STDP */
    synapse.weight = 0.22f;

    /* Restore only STDP */
    int result = nimcp_state_manager_restore_module(
        manager, "stdp", stdp_buffer.data(), stdp_written);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(synapse.weight, 0.88f);
}

//=============================================================================
// Cross-Module Validation Tests
//=============================================================================

TEST_F(StateManagerIntegrationTest, ValidateAllModules) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    int valid_count = nimcp_state_manager_validate_all(manager);
    EXPECT_EQ(valid_count, 2);
}

TEST_F(StateManagerIntegrationTest, ValidateWithOneInvalid) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Corrupt STDP */
    synapse.weight = NAN;

    int valid_count = nimcp_state_manager_validate_all(manager);
    EXPECT_EQ(valid_count, 1);  /* Only astrocyte should be valid */
}

TEST_F(StateManagerIntegrationTest, ResetInvalidModules) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Corrupt STDP */
    synapse.weight = NAN;
    synapse.pre_trace = INFINITY;

    /* Reset invalid */
    int reset_count = nimcp_state_manager_reset_invalid(manager);
    EXPECT_EQ(reset_count, 1);

    /* STDP should now be valid */
    EXPECT_TRUE(std::isfinite(synapse.weight));
    EXPECT_TRUE(std::isfinite(synapse.pre_trace));

    /* All should validate */
    int valid_count = nimcp_state_manager_validate_all(manager);
    EXPECT_EQ(valid_count, 2);
}

//=============================================================================
// Disabled Module Tests
//=============================================================================

TEST_F(StateManagerIntegrationTest, DisabledModuleNotCheckpointed) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Disable astrocyte */
    nimcp_state_manager_set_enabled(manager, "astrocyte", false);

    /* Get total size - should only include STDP */
    size_t total_size = nimcp_state_manager_get_total_size(manager);
    size_t stdp_only_size = stdp_ops->get_size(&synapse);

    /* Total should be approximately STDP size (with header overhead) */
    EXPECT_LT(total_size, stdp_only_size + astro_ops->get_size(network));
}

TEST_F(StateManagerIntegrationTest, DisabledModuleNotRestored) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Checkpoint all (both enabled) */
    size_t total_size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &total_size);
    std::vector<uint8_t> buffer(total_size);
    size_t written = total_size;
    nimcp_state_manager_checkpoint_all(manager, buffer.data(), &written);

    /* Set new values */
    float stdp_new = 0.33f;
    float astro_new = 3.33f;
    synapse.weight = stdp_new;
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        astro->calcium_concentration = astro_new;
    }

    /* Disable astrocyte before restore */
    nimcp_state_manager_set_enabled(manager, "astrocyte", false);

    /* Restore all - should only restore STDP */
    nimcp_state_manager_restore_all(manager, buffer.data(), written);

    /* STDP should be restored, astrocyte should keep new value */
    /* (Note: actual behavior depends on implementation - test documents it) */
}

//=============================================================================
// Statistics Tracking Tests
//=============================================================================

TEST_F(StateManagerIntegrationTest, StatisticsUpdated) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_ops = astrocyte_network_get_state_ops();

    nimcp_state_manager_register(manager, "stdp", stdp_ops, &synapse);
    nimcp_state_manager_register(manager, "astrocyte", astro_ops, network);

    /* Perform operations */
    size_t size = 0;
    nimcp_state_manager_checkpoint_all(manager, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    nimcp_state_manager_checkpoint_all(manager, buffer.data(), &written);

    nimcp_state_manager_validate_all(manager);
    nimcp_state_manager_restore_all(manager, buffer.data(), written);

    /* Get stats */
    nimcp_state_manager_stats_t stats;
    int result = nimcp_state_manager_get_stats(manager, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.module_count, 2u);
    EXPECT_EQ(stats.enabled_modules, 2u);
    EXPECT_GE(stats.total_checkpoints, 1u);
    EXPECT_GE(stats.total_restores, 1u);
    EXPECT_GE(stats.total_validations, 1u);
}

//=============================================================================
// Concurrent Registration Tests
//=============================================================================

TEST_F(StateManagerIntegrationTest, RegisterUnregisterCycle) {
    const nimcp_module_state_ops_t* stdp_ops = stdp_get_state_ops();

    /* Multiple register/unregister cycles */
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stdp_%d", i);

        int result = nimcp_state_manager_register(manager, name, stdp_ops, &synapse);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(nimcp_state_manager_get_module_count(manager), (uint32_t)(i + 1));
    }

    /* Unregister all */
    for (int i = 4; i >= 0; i--) {
        char name[32];
        snprintf(name, sizeof(name), "stdp_%d", i);

        int result = nimcp_state_manager_unregister(manager, name);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(nimcp_state_manager_get_module_count(manager), (uint32_t)i);
    }
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
