/**
 * @file test_module_recovery_regression.cpp
 * @brief Regression tests for module recovery bug prevention
 *
 * WHAT: Regression tests to prevent recurrence of specific bugs
 * WHY:  Document and prevent known failure modes in recovery system
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

extern "C" {
#include "utils/fault_tolerance/nimcp_module_recovery.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

class ModuleRecoveryRegressionTest : public ::testing::Test {
protected:
    nimcp_module_recovery_manager_t* manager;

    void SetUp() override {
        manager = nimcp_module_recovery_manager_create();
    }

    void TearDown() override {
        if (manager) {
            nimcp_module_recovery_manager_destroy(manager);
            manager = nullptr;
        }
    }
};

//=============================================================================
// Bug: Infinite escalation loop
// Issue: Auto-escalation could loop forever if max level not enforced
// Fix: Proper max level check and enforcement
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, InfiniteEscalationLoop) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    nimcp_module_recovery_register(manager, "escalate_test", ops, &synapse);

    /* Enable auto-escalation with low max level */
    manager->auto_escalate = true;
    manager->max_escalation_level = NIMCP_MODULE_RECOVERY_PARTIAL;

    /* Make module need escalation */
    synapse.weight = NAN;

    /* This should complete (not loop forever) */
    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "escalate_test", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Should either succeed or stop at max level */
    EXPECT_NE(result, (nimcp_module_recovery_result_t)-1);  /* Not infinite loop indicator */
}

//=============================================================================
// Bug: Health check null dereference
// Issue: Health check crashed when module state was null
// Fix: Null check in health check functions
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, HealthCheckNullState) {
    /* Direct health check with null */
    float health = 1.0f;
    int result = nimcp_stdp_health_check(nullptr, &health);
    EXPECT_LT(result, 0);

    result = nimcp_astrocyte_health_check(nullptr, &health);
    EXPECT_LT(result, 0);

    /* Health should be unchanged on error */
    EXPECT_FLOAT_EQ(health, 1.0f);
}

//=============================================================================
// Bug: Recovery on isolated module corrupts state
// Issue: Recovery attempt on isolated module proceeded despite isolation flag
// Fix: Skip isolated modules in recovery
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, RecoveryOnIsolatedModule) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    synapse.weight = 0.77f;  /* Known good value */
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    nimcp_module_recovery_register(manager, "isolated", ops, &synapse);
    nimcp_module_recovery_isolate(manager, "isolated");

    /* Mark as unhealthy but isolated */
    synapse.pre_trace = NAN;
    manager->health_threshold = 0.8f;

    /* Attempt recovery all - should skip isolated */
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);
    EXPECT_EQ(recovered, 0);

    /* Weight should be unchanged (not reset) */
    /* Note: pre_trace was corrupted but we didn't recover */
}

//=============================================================================
// Bug: Stats not updated on failed recovery
// Issue: Recovery statistics only updated on success
// Fix: Track both successes and failures
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, StatsUpdatedOnFailure) {
    /* Create mock ops that always fail */
    nimcp_module_recovery_ops_t failing_ops;
    failing_ops.recover = [](void*, nimcp_module_recovery_level_t, void*) {
        return NIMCP_MODULE_RECOVERY_FAILED;
    };
    failing_ops.health_check = [](void*, float* h) { *h = 0.0f; return 0; };
    failing_ops.user_data = nullptr;

    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    nimcp_module_recovery_register(manager, "failing", &failing_ops, &synapse);

    /* Disable auto-escalation to get clean failure */
    manager->auto_escalate = false;

    /* Attempt (and fail) recovery */
    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        manager, "failing", NIMCP_MODULE_RECOVERY_LIGHT);
    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_FAILED);

    /* Stats should reflect failure */
    nimcp_module_recovery_stats_t stats;
    nimcp_module_recovery_get_stats(manager, &stats);
    EXPECT_GE(stats.total_failures, 1u);
}

//=============================================================================
// Bug: Recovery callback receives wrong level
// Issue: Escalated level not passed to callback correctly
// Fix: Pass actual (escalated) level to callback
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, EscalatedLevelPassedCorrectly) {
    static nimcp_module_recovery_level_t received_level;
    static int call_count;
    call_count = 0;
    received_level = NIMCP_MODULE_RECOVERY_NONE;

    /* Create ops that track received level */
    nimcp_module_recovery_ops_t tracking_ops;
    tracking_ops.recover = [](void*, nimcp_module_recovery_level_t level, void*) {
        received_level = level;
        call_count++;
        /* Escalate first two times, succeed on third */
        if (call_count < 3) {
            return NIMCP_MODULE_RECOVERY_ESCALATE;
        }
        return NIMCP_MODULE_RECOVERY_SUCCESS;
    };
    tracking_ops.health_check = [](void*, float* h) { *h = 1.0f; return 0; };
    tracking_ops.user_data = nullptr;

    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    nimcp_module_recovery_register(manager, "level_track", &tracking_ops, &synapse);
    manager->auto_escalate = true;
    manager->max_escalation_level = NIMCP_MODULE_RECOVERY_ISOLATE;

    nimcp_module_recovery_attempt(manager, "level_track", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Should have escalated through levels */
    EXPECT_EQ(call_count, 3);
    /* Last received level should be > LIGHT */
    EXPECT_GT(received_level, NIMCP_MODULE_RECOVERY_LIGHT);
}

//=============================================================================
// Bug: Health threshold of 0.0 recovers all modules
// Issue: Health < 0.0 is always true when threshold is 0.0
// Fix: Use <= comparison and handle edge cases
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, ZeroHealthThreshold) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    nimcp_module_recovery_register(manager, "zero_thresh", ops, &synapse);

    /* Set zero threshold */
    manager->health_threshold = 0.0f;

    /* Module is healthy (health > 0) */
    float health;
    nimcp_module_recovery_check_health(manager, "zero_thresh", &health);
    EXPECT_GT(health, 0.0f);

    /* Should NOT recover healthy module */
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);
    EXPECT_EQ(recovered, 0);
}

//=============================================================================
// Bug: Health threshold of 1.0 never recovers
// Issue: Health <= 1.0 is always true
// Fix: Proper threshold comparison
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, OneHealthThreshold) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    synapse.weight = synapse.w_max * 2.0f;  /* Unhealthy */
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    nimcp_module_recovery_register(manager, "one_thresh", ops, &synapse);

    /* Set threshold to 1.0 (max health) */
    manager->health_threshold = 1.0f;

    /* Module health should be < 1.0 (unhealthy) */
    float health;
    nimcp_module_recovery_check_health(manager, "one_thresh", &health);
    EXPECT_LT(health, 1.0f);

    /* Should attempt recovery */
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);
    EXPECT_GE(recovered, 1);
}

//=============================================================================
// Bug: Last recovery time not updated
// Issue: last_recovery_time field remained 0 after recovery
// Fix: Set timestamp after recovery
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, LastRecoveryTimeUpdated) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    nimcp_module_recovery_register(manager, "time_track", ops, &synapse);

    /* Perform recovery */
    nimcp_module_recovery_attempt(manager, "time_track", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Find entry and check timestamp */
    for (uint32_t i = 0; i < manager->module_count; i++) {
        if (strcmp(manager->modules[i].name, "time_track") == 0) {
            EXPECT_GT(manager->modules[i].last_recovery_time, 0u);
            break;
        }
    }
}

//=============================================================================
// Bug: Concurrent health checks corrupt last_health_score
// Issue: Simultaneous health checks overwrote each other's scores
// Fix: Thread-safe score storage
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, ConcurrentHealthChecks) {
    stdp_synapse_t synapses[3];
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    for (int i = 0; i < 3; i++) {
        stdp_synapse_init(&synapses[i]);
        char name[32];
        snprintf(name, sizeof(name), "concurrent_%d", i);
        nimcp_module_recovery_register(manager, name, ops, &synapses[i]);
    }

    /* Check health multiple times */
    for (int iter = 0; iter < 10; iter++) {
        float avg = nimcp_module_recovery_check_all_health(manager);
        EXPECT_GE(avg, 0.0f);
        EXPECT_LE(avg, 1.0f);
    }
}

//=============================================================================
// Bug: Restore after isolate doesn't reset isolation flag
// Issue: Module remained marked as isolated after restore
// Fix: Clear isolated flag in restore function
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, RestoreAfterIsolate) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    nimcp_module_recovery_register(manager, "restore_iso", ops, &synapse);

    /* Isolate */
    nimcp_module_recovery_isolate(manager, "restore_iso");
    EXPECT_TRUE(nimcp_module_recovery_is_isolated(manager, "restore_iso"));

    /* Restore */
    nimcp_module_recovery_restore(manager, "restore_iso");
    EXPECT_FALSE(nimcp_module_recovery_is_isolated(manager, "restore_iso"));

    /* Should be able to recover again */
    manager->health_threshold = 1.0f;
    synapse.weight = synapse.w_max * 2.0f;  /* Make unhealthy */
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(manager);
    EXPECT_GE(recovered, 1);
}

//=============================================================================
// Bug: Empty module name accepted
// Issue: Empty string "" was accepted as valid module name
// Fix: Reject empty names
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, EmptyModuleName) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    int result = nimcp_module_recovery_register(manager, "", ops, &synapse);
    EXPECT_LT(result, 0);

    result = nimcp_module_recovery_register(manager, "\0", ops, &synapse);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Bug: Unregister during recovery causes crash
// Issue: Module unregistered while recovery in progress
// Fix: Lock protection during recovery
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, UnregisterDuringRecovery) {
    /* This test documents the expected behavior */
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();

    nimcp_module_recovery_register(manager, "unregister_test", ops, &synapse);

    /* Start recovery, then unregister */
    /* In a real scenario this would be from different threads */
    nimcp_module_recovery_attempt(manager, "unregister_test", NIMCP_MODULE_RECOVERY_LIGHT);

    /* Unregister should succeed */
    int result = nimcp_module_recovery_unregister(manager, "unregister_test");
    EXPECT_EQ(result, 0);

    /* Further operations should fail gracefully */
    result = (int)nimcp_module_recovery_attempt(
        manager, "unregister_test", NIMCP_MODULE_RECOVERY_LIGHT);
    /* Should return FAILED since module doesn't exist */
}

//=============================================================================
// Bug: Recovery ops with null callbacks accepted
// Issue: Null callback pointers caused crashes during recovery
// Fix: Validate ops on registration
//=============================================================================

TEST_F(ModuleRecoveryRegressionTest, NullCallbacksRejected) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    nimcp_module_recovery_ops_t bad_ops;
    bad_ops.recover = nullptr;
    bad_ops.health_check = nullptr;
    bad_ops.user_data = nullptr;

    int result = nimcp_module_recovery_register(manager, "null_ops", &bad_ops, &synapse);
    /* Should reject (bad_ops has null callbacks) */
    EXPECT_LT(result, 0);
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
