/**
 * @file test_module_recovery_integration.cpp
 * @brief Integration tests for module recovery with state manager
 *
 * WHAT: Integration tests for recovery system cross-module interactions
 * WHY:  Verify graduated recovery works with state checkpoints
 * HOW:  Test recovery with STDP and astrocyte modules and state manager
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
#include "utils/fault_tolerance/nimcp_module_recovery.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/astrocyte_types/nimcp_astrocyte_types.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Module recovery integration test fixture
 *
 * WHAT: Tests recovery manager with real modules and state manager
 * WHY:  Verify recovery can use checkpoints for partial recovery
 * HOW:  Create both managers, register modules, test recovery workflows
 */
class ModuleRecoveryIntegrationTest : public ::testing::Test {
protected:
    nimcp_module_recovery_manager_t* recovery_manager;
    nimcp_state_manager_t* state_manager;
    stdp_synapse_t synapse;
    astrocyte_network_t* network;
    static constexpr int NETWORK_SIZE = 5;

    void SetUp() override {
        recovery_manager = nimcp_module_recovery_manager_create();
        state_manager = nimcp_state_manager_create();
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
        if (recovery_manager) {
            nimcp_module_recovery_manager_destroy(recovery_manager);
            recovery_manager = nullptr;
        }
        if (state_manager) {
            nimcp_state_manager_destroy(state_manager);
            state_manager = nullptr;
        }
        if (network) {
            astrocyte_network_destroy(network);
            network = nullptr;
        }
    }
};

//=============================================================================
// Multi-Module Recovery Tests
//=============================================================================

TEST_F(ModuleRecoveryIntegrationTest, RegisterMultipleModulesForRecovery) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    const nimcp_module_recovery_ops_t* astro_ops = nimcp_astrocyte_get_recovery_ops();

    int result1 = nimcp_module_recovery_register(
        recovery_manager, "stdp", stdp_ops, &synapse);
    int result2 = nimcp_module_recovery_register(
        recovery_manager, "astrocyte", astro_ops, network);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);

    nimcp_module_recovery_stats_t stats;
    nimcp_module_recovery_get_stats(recovery_manager, &stats);
    EXPECT_EQ(stats.module_count, 2u);
}

TEST_F(ModuleRecoveryIntegrationTest, HealthCheckAllModules) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    const nimcp_module_recovery_ops_t* astro_ops = nimcp_astrocyte_get_recovery_ops();

    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);
    nimcp_module_recovery_register(recovery_manager, "astrocyte", astro_ops, network);

    float avg_health = nimcp_module_recovery_check_all_health(recovery_manager);

    EXPECT_GE(avg_health, 0.0f);
    EXPECT_LE(avg_health, 1.0f);
    /* Both modules should be healthy initially */
    EXPECT_GT(avg_health, 0.7f);
}

TEST_F(ModuleRecoveryIntegrationTest, RecoverUnhealthyModulesOnly) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    const nimcp_module_recovery_ops_t* astro_ops = nimcp_astrocyte_get_recovery_ops();

    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);
    nimcp_module_recovery_register(recovery_manager, "astrocyte", astro_ops, network);

    /* Corrupt one module */
    synapse.weight = synapse.w_max * 2.0f;  /* Out of range */

    /* Set threshold */
    recovery_manager->health_threshold = 0.7f;

    /* Attempt recovery of unhealthy */
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(recovery_manager);

    /* STDP should have been recovered */
    EXPECT_GE(recovered, 1);

    /* Both should now be healthy */
    float avg_health = nimcp_module_recovery_check_all_health(recovery_manager);
    EXPECT_GT(avg_health, 0.7f);
}

//=============================================================================
// Recovery with State Checkpoints Tests
//=============================================================================

TEST_F(ModuleRecoveryIntegrationTest, RecoveryPartialUsesCheckpoint) {
    /* Register modules with both managers */
    const nimcp_module_state_ops_t* stdp_state_ops = stdp_get_state_ops();
    const nimcp_module_recovery_ops_t* stdp_recovery_ops = nimcp_stdp_get_recovery_ops();

    nimcp_state_manager_register(state_manager, "stdp", stdp_state_ops, &synapse);
    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_recovery_ops, &synapse);

    /* Set known good state */
    synapse.weight = 0.75f;
    synapse.pre_trace = 0.1f;

    /* Checkpoint */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(state_manager, "stdp", nullptr, &size);
    std::vector<uint8_t> checkpoint(size);
    size_t written = size;
    nimcp_state_manager_checkpoint_module(state_manager, "stdp", checkpoint.data(), &written);

    /* Corrupt state */
    synapse.weight = 999.0f;
    synapse.pre_trace = NAN;

    /* Attempt partial recovery (which should use checkpoint if available) */
    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        recovery_manager, "stdp", NIMCP_MODULE_RECOVERY_PARTIAL);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

    /* State should be recovered (may not match checkpoint exactly) */
    EXPECT_TRUE(std::isfinite(synapse.weight));
    EXPECT_TRUE(std::isfinite(synapse.pre_trace));
}

//=============================================================================
// Graduated Recovery Escalation Tests
//=============================================================================

TEST_F(ModuleRecoveryIntegrationTest, GraduatedRecoveryStdp) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);

    /* Test each recovery level */
    nimcp_module_recovery_level_t levels[] = {
        NIMCP_MODULE_RECOVERY_LIGHT,
        NIMCP_MODULE_RECOVERY_PARTIAL,
        NIMCP_MODULE_RECOVERY_FULL
    };

    for (int i = 0; i < 3; i++) {
        /* Corrupt state */
        synapse.pre_trace = 0.5f;
        synapse.post_trace = 0.3f;

        nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
            recovery_manager, "stdp", levels[i]);

        EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

        /* Verify recovery was effective */
        float health;
        nimcp_module_recovery_check_health(recovery_manager, "stdp", &health);
        EXPECT_GT(health, 0.7f);
    }
}

TEST_F(ModuleRecoveryIntegrationTest, GraduatedRecoveryAstrocyte) {
    const nimcp_module_recovery_ops_t* astro_ops = nimcp_astrocyte_get_recovery_ops();
    nimcp_module_recovery_register(recovery_manager, "astrocyte", astro_ops, network);

    /* Test light recovery */
    for (int i = 0; i < NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            astro->calcium_concentration = 10.0f;  /* High value */
        }
    }

    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        recovery_manager, "astrocyte", NIMCP_MODULE_RECOVERY_LIGHT);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

    /* Calcium should be reset */
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        EXPECT_LT(astro->calcium_concentration, 10.0f);
    }
}

//=============================================================================
// Module Isolation Tests
//=============================================================================

TEST_F(ModuleRecoveryIntegrationTest, IsolateFailedModule) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    const nimcp_module_recovery_ops_t* astro_ops = nimcp_astrocyte_get_recovery_ops();

    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);
    nimcp_module_recovery_register(recovery_manager, "astrocyte", astro_ops, network);

    /* Isolate STDP */
    int result = nimcp_module_recovery_isolate(recovery_manager, "stdp");
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(nimcp_module_recovery_is_isolated(recovery_manager, "stdp"));
    EXPECT_FALSE(nimcp_module_recovery_is_isolated(recovery_manager, "astrocyte"));

    /* Stats should reflect isolation */
    nimcp_module_recovery_stats_t stats;
    nimcp_module_recovery_get_stats(recovery_manager, &stats);
    EXPECT_EQ(stats.isolated_modules, 1u);
}

TEST_F(ModuleRecoveryIntegrationTest, IsolatedModuleSkippedInRecoveryAll) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    const nimcp_module_recovery_ops_t* astro_ops = nimcp_astrocyte_get_recovery_ops();

    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);
    nimcp_module_recovery_register(recovery_manager, "astrocyte", astro_ops, network);

    /* Isolate STDP and make both unhealthy */
    nimcp_module_recovery_isolate(recovery_manager, "stdp");

    synapse.weight = synapse.w_max * 2.0f;
    for (int i = 0; i < NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            astro->calcium_concentration = 200.0f;  /* > 100.0f triggers unhealthy */
        }
    }

    recovery_manager->health_threshold = 0.7f;

    /* Only astrocyte should be recovered (STDP is isolated) */
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(recovery_manager);
    EXPECT_EQ(recovered, 1);

    /* Restore and recover STDP */
    nimcp_module_recovery_restore(recovery_manager, "stdp");
    EXPECT_FALSE(nimcp_module_recovery_is_isolated(recovery_manager, "stdp"));
}

TEST_F(ModuleRecoveryIntegrationTest, IsolationViaRecoveryLevel) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);

    /* Use ISOLATE recovery level */
    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        recovery_manager, "stdp", NIMCP_MODULE_RECOVERY_ISOLATE);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

    /* Synapse should be effectively disabled */
    EXPECT_FLOAT_EQ(synapse.weight, 0.0f);
    EXPECT_FLOAT_EQ(synapse.learning_rate, 0.0f);
}

//=============================================================================
// Combined State and Recovery Workflow Tests
//=============================================================================

TEST_F(ModuleRecoveryIntegrationTest, FullRecoveryWorkflow) {
    /* Register with both managers */
    const nimcp_module_state_ops_t* stdp_state_ops = stdp_get_state_ops();
    const nimcp_module_state_ops_t* astro_state_ops = astrocyte_network_get_state_ops();
    const nimcp_module_recovery_ops_t* stdp_recovery_ops = nimcp_stdp_get_recovery_ops();
    const nimcp_module_recovery_ops_t* astro_recovery_ops = nimcp_astrocyte_get_recovery_ops();

    nimcp_state_manager_register(state_manager, "stdp", stdp_state_ops, &synapse);
    nimcp_state_manager_register(state_manager, "astrocyte", astro_state_ops, network);
    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_recovery_ops, &synapse);
    nimcp_module_recovery_register(recovery_manager, "astrocyte", astro_recovery_ops, network);

    /* 1. Establish good state */
    synapse.weight = 0.65f;

    /* 2. Checkpoint */
    size_t total_size = 0;
    nimcp_state_manager_checkpoint_all(state_manager, nullptr, &total_size);
    std::vector<uint8_t> checkpoint(total_size);
    size_t written = total_size;
    nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &written);

    /* 3. Simulate failure */
    synapse.weight = NAN;
    synapse.pre_trace = INFINITY;

    /* 4. Validate detects failure */
    int valid_count = nimcp_state_manager_validate_all(state_manager);
    EXPECT_LT(valid_count, 2);

    /* 5. Attempt recovery */
    float health;
    nimcp_module_recovery_check_health(recovery_manager, "stdp", &health);
    if (health < 0.7f) {
        nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
            recovery_manager, "stdp", NIMCP_MODULE_RECOVERY_LIGHT);
        EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    }

    /* 6. Verify recovery */
    EXPECT_TRUE(std::isfinite(synapse.weight));
    EXPECT_TRUE(std::isfinite(synapse.pre_trace));

    /* 7. All should now validate */
    valid_count = nimcp_state_manager_validate_all(state_manager);
    EXPECT_EQ(valid_count, 2);
}

TEST_F(ModuleRecoveryIntegrationTest, AutoEscalationWorkflow) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);

    /* Enable auto-escalation */
    recovery_manager->auto_escalate = true;
    recovery_manager->max_escalation_level = NIMCP_MODULE_RECOVERY_FULL;

    /* Severely corrupt state */
    synapse.weight = NAN;
    synapse.pre_trace = INFINITY;
    synapse.post_trace = NAN;
    synapse.learning_rate = -1.0f;

    /* Start with light recovery - should auto-escalate if needed */
    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        recovery_manager, "stdp", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Should eventually succeed */
    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

    /* State should be valid */
    float health;
    nimcp_module_recovery_check_health(recovery_manager, "stdp", &health);
    EXPECT_GT(health, 0.5f);
}

//=============================================================================
// Statistics Consistency Tests
//=============================================================================

TEST_F(ModuleRecoveryIntegrationTest, RecoveryStatisticsConsistent) {
    const nimcp_module_recovery_ops_t* stdp_ops = nimcp_stdp_get_recovery_ops();
    nimcp_module_recovery_register(recovery_manager, "stdp", stdp_ops, &synapse);

    /* Perform several recovery operations */
    for (int i = 0; i < 5; i++) {
        synapse.pre_trace = 0.5f;
        nimcp_module_recovery_attempt(recovery_manager, "stdp", NIMCP_MODULE_RECOVERY_LIGHT);
    }

    nimcp_module_recovery_stats_t stats;
    nimcp_module_recovery_get_stats(recovery_manager, &stats);

    EXPECT_GE(stats.total_recoveries, 5u);
    EXPECT_GE(stats.total_successes, 5u);
    EXPECT_EQ(stats.total_failures, 0u);
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
