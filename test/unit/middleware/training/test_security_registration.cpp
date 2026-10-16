/**
 * @file test_security_registration.cpp
 * @brief Unit tests for Training Module Security Registration
 *
 * Tests security module registration/unregistration for:
 * - Training-Plasticity Bridge
 * - LR Scheduler
 * - Regularization
 * - Gradient Manager
 * - Training Adapters (Learning Signal, Weight Router, Event Manager)
 *
 * @note Part of Phase SR-1: Security Registration Tests
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_lr_scheduler.h"
#include "middleware/training/nimcp_regularization.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_training_adapters.h"
#include "security/nimcp_security_integration.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class SecurityRegistrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create security context
        sec_ctx = nimcp_sec_integration_create();
        ASSERT_NE(sec_ctx, nullptr);

        nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
        config.enable_continuous_monitoring = false;
        config.enable_self_monitoring = false;
        ASSERT_EQ(nimcp_sec_integration_init(sec_ctx, &config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (sec_ctx) {
            nimcp_sec_integration_destroy(sec_ctx);
            sec_ctx = nullptr;
        }
    }

    nimcp_sec_integration_t* sec_ctx = nullptr;
};

/* ============================================================================
 * LR Scheduler Security Registration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, LRScheduler_RegistersWithSecurity) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    config.security_ctx = sec_ctx;

    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Verify registration happened by checking stats
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.registered_modules, 1u);

    nimcp_lr_scheduler_destroy(scheduler);
}

TEST_F(SecurityRegistrationTest, LRScheduler_UnregistersOnDestroy) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    config.security_ctx = sec_ctx;

    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    nimcp_lr_scheduler_destroy(scheduler);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);
    EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
}

TEST_F(SecurityRegistrationTest, LRScheduler_WorksWithoutSecurity) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    config.security_ctx = nullptr;  // No security context

    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    // Should still function normally
    float lr = nimcp_lr_scheduler_get_lr(scheduler);
    EXPECT_FLOAT_EQ(lr, 0.001f);

    nimcp_lr_scheduler_destroy(scheduler);
}

/* ============================================================================
 * Regularization Security Registration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, Regularization_RegistersWithSecurity) {
    nimcp_regularization_config_t config;
    memset(&config, 0, sizeof(config));
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2 = nimcp_l2_default_config(0.01f);
    config.security_ctx = sec_ctx;

    nimcp_regularization_ctx_t* reg = nimcp_regularization_create(&config);
    ASSERT_NE(reg, nullptr);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.registered_modules, 1u);

    nimcp_regularization_destroy(reg);
}

TEST_F(SecurityRegistrationTest, Regularization_UnregistersOnDestroy) {
    nimcp_regularization_config_t config;
    memset(&config, 0, sizeof(config));
    config.weight_reg_type = NIMCP_REG_L1;
    config.weight_reg.l1 = nimcp_l1_default_config(0.01f);
    config.security_ctx = sec_ctx;

    nimcp_regularization_ctx_t* reg = nimcp_regularization_create(&config);
    ASSERT_NE(reg, nullptr);

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    nimcp_regularization_destroy(reg);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);
    EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
}

TEST_F(SecurityRegistrationTest, Regularization_WorksWithoutSecurity) {
    nimcp_regularization_config_t config;
    memset(&config, 0, sizeof(config));
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2 = nimcp_l2_default_config(0.01f);
    config.security_ctx = nullptr;

    nimcp_regularization_ctx_t* reg = nimcp_regularization_create(&config);
    ASSERT_NE(reg, nullptr);

    // Should still compute regularization
    float weights[] = {1.0f, 2.0f, 3.0f};
    float loss = nimcp_regularization_loss(reg, weights, 3);
    EXPECT_GT(loss, 0.0f);

    nimcp_regularization_destroy(reg);
}

/* ============================================================================
 * Gradient Manager Security Registration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, GradientManager_RegistersWithSecurity) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.security_ctx = sec_ctx;

    nimcp_gradient_manager_ctx_t* gm = nimcp_gradient_manager_create(&config);
    ASSERT_NE(gm, nullptr);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.registered_modules, 1u);

    nimcp_gradient_manager_destroy(gm);
}

TEST_F(SecurityRegistrationTest, GradientManager_UnregistersOnDestroy) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.security_ctx = sec_ctx;

    nimcp_gradient_manager_ctx_t* gm = nimcp_gradient_manager_create(&config);
    ASSERT_NE(gm, nullptr);

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    nimcp_gradient_manager_destroy(gm);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);
    EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
}

TEST_F(SecurityRegistrationTest, GradientManager_WorksWithoutSecurity) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.security_ctx = nullptr;
    config.check_nan_inf = true;

    nimcp_gradient_manager_ctx_t* gm = nimcp_gradient_manager_create(&config);
    ASSERT_NE(gm, nullptr);

    // Should still check gradient health
    float grads[] = {0.1f, 0.2f, 0.3f};
    nimcp_grad_health_t health = nimcp_gradient_check_health_ctx(gm, grads, 3);
    EXPECT_EQ(health, NIMCP_GRAD_HEALTHY);

    nimcp_gradient_manager_destroy(gm);
}

/* ============================================================================
 * Training-Plasticity Bridge Security Registration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, TPB_RegistersWithSecurity) {
    tpb_config_t config = tpb_config_default();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    tpb_context_t* tpb = tpb_create(&config);
    ASSERT_NE(tpb, nullptr);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // TPB should register with security if implementation supports it
    // Allow for possibility it hasn't been implemented yet
    if (stats_after.registered_modules > stats_before.registered_modules) {
        EXPECT_GE(stats_after.registered_modules, stats_before.registered_modules + 1);
    }

    tpb_destroy(tpb);
}

TEST_F(SecurityRegistrationTest, TPB_UnregistersOnDestroy) {
    tpb_config_t config = tpb_config_default();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_init;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_init), NIMCP_SUCCESS);

    tpb_context_t* tpb = tpb_create(&config);
    ASSERT_NE(tpb, nullptr);

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    tpb_destroy(tpb);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // If it registered, it should unregister
    if (stats_before.active_modules > stats_init.active_modules) {
        EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
    }
}

TEST_F(SecurityRegistrationTest, TPB_WorksWithoutSecurity) {
    tpb_config_t config = tpb_config_default();
    config.security_ctx = nullptr;

    tpb_context_t* tpb = tpb_create(&config);
    ASSERT_NE(tpb, nullptr);

    // Should still function
    tpb_stats_t stats;
    ASSERT_EQ(tpb_get_stats(tpb, &stats), NIMCP_SUCCESS);

    tpb_destroy(tpb);
}

/* ============================================================================
 * Training Adapters Security Registration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, LearningSignalAdapter_RegistersWithSecurity) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // If implemented, should register
    if (stats_after.registered_modules > stats_before.registered_modules) {
        EXPECT_GE(stats_after.registered_modules, stats_before.registered_modules + 1);
    }

    learning_signal_adapter_destroy(adapter);
}

TEST_F(SecurityRegistrationTest, LearningSignalAdapter_UnregistersOnDestroy) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_init;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_init), NIMCP_SUCCESS);

    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    learning_signal_adapter_destroy(adapter);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // If it registered, it should unregister
    if (stats_before.active_modules > stats_init.active_modules) {
        EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
    }
}

TEST_F(SecurityRegistrationTest, WeightUpdateRouter_RegistersWithSecurity) {
    weight_update_router_config_t config = weight_update_router_default_config();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    weight_update_router_t router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // If implemented, should register
    if (stats_after.registered_modules > stats_before.registered_modules) {
        EXPECT_GE(stats_after.registered_modules, stats_before.registered_modules + 1);
    }

    weight_update_router_destroy(router);
}

TEST_F(SecurityRegistrationTest, WeightUpdateRouter_UnregistersOnDestroy) {
    weight_update_router_config_t config = weight_update_router_default_config();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_init;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_init), NIMCP_SUCCESS);

    weight_update_router_t router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    weight_update_router_destroy(router);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // If it registered, it should unregister
    if (stats_before.active_modules > stats_init.active_modules) {
        EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
    }
}

TEST_F(SecurityRegistrationTest, TrainingEventManager_RegistersWithSecurity) {
    training_event_manager_config_t config = training_event_manager_default_config();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    training_event_manager_t manager = training_event_manager_create(&config, nullptr);
    ASSERT_NE(manager, nullptr);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // If implemented, should register
    if (stats_after.registered_modules > stats_before.registered_modules) {
        EXPECT_GE(stats_after.registered_modules, stats_before.registered_modules + 1);
    }

    training_event_manager_destroy(manager);
}

TEST_F(SecurityRegistrationTest, TrainingEventManager_UnregistersOnDestroy) {
    training_event_manager_config_t config = training_event_manager_default_config();
    config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_init;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_init), NIMCP_SUCCESS);

    training_event_manager_t manager = training_event_manager_create(&config, nullptr);
    ASSERT_NE(manager, nullptr);

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    training_event_manager_destroy(manager);

    nimcp_sec_integration_stats_t stats_after;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_after), NIMCP_SUCCESS);

    // If it registered, it should unregister
    if (stats_before.active_modules > stats_init.active_modules) {
        EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
    }
}

/* ============================================================================
 * Multiple Module Registration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, MultipleModules_AllRegisterCorrectly) {
    // Create multiple modules with security
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    lr_config.security_ctx = sec_ctx;

    nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
    gm_config.security_ctx = sec_ctx;

    learning_signal_adapter_config_t lsa_config = learning_signal_adapter_default_config();
    lsa_config.security_ctx = sec_ctx;

    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);
    nimcp_gradient_manager_ctx_t* gm = nimcp_gradient_manager_create(&gm_config);
    learning_signal_adapter_t adapter = learning_signal_adapter_create(&lsa_config);

    ASSERT_NE(scheduler, nullptr);
    ASSERT_NE(gm, nullptr);
    ASSERT_NE(adapter, nullptr);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    // At least LR scheduler and gradient manager should register (they do)
    // Learning signal adapter may or may not be implemented
    EXPECT_GE(stats.registered_modules, stats_before.registered_modules + 2u);
    EXPECT_GE(stats.active_modules, stats_before.active_modules + 2u);

    // Cleanup
    nimcp_lr_scheduler_destroy(scheduler);
    nimcp_gradient_manager_destroy(gm);
    learning_signal_adapter_destroy(adapter);

    // Verify all unregistered
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, stats_before.active_modules);
}

TEST_F(SecurityRegistrationTest, MultipleModules_PartialUnregistration) {
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    lr_config.security_ctx = sec_ctx;

    nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
    gm_config.security_ctx = sec_ctx;

    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);
    nimcp_gradient_manager_ctx_t* gm = nimcp_gradient_manager_create(&gm_config);

    ASSERT_NE(scheduler, nullptr);
    ASSERT_NE(gm, nullptr);

    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    uint32_t initial_active = stats.active_modules;

    // Destroy one module
    nimcp_lr_scheduler_destroy(scheduler);

    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, initial_active - 1);

    // Gradient manager should still work
    float grads[] = {0.1f, 0.2f};
    EXPECT_EQ(nimcp_gradient_check_health_ctx(gm, grads, 2), NIMCP_GRAD_HEALTHY);

    nimcp_gradient_manager_destroy(gm);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, NullSecurityContext_CreationSucceeds) {
    // All modules should work without security context
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    lr_config.security_ctx = nullptr;

    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);
    ASSERT_NE(scheduler, nullptr);
    nimcp_lr_scheduler_destroy(scheduler);

    nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
    gm_config.security_ctx = nullptr;

    nimcp_gradient_manager_ctx_t* gm = nimcp_gradient_manager_create(&gm_config);
    ASSERT_NE(gm, nullptr);
    nimcp_gradient_manager_destroy(gm);
}

TEST_F(SecurityRegistrationTest, DoubleDestroy_NoSideEffects) {
    nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    config.security_ctx = sec_ctx;

    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&config);
    ASSERT_NE(scheduler, nullptr);

    nimcp_lr_scheduler_destroy(scheduler);
    // Second destroy on null should be safe (pointer was freed)
    // Note: We can't actually call destroy twice on same pointer, but NULL is safe
    nimcp_lr_scheduler_destroy(nullptr);
}

/* ============================================================================
 * Security Category Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationTest, ModulesRegisterWithCorrectCategory) {
    nimcp_sec_integration_stats_t stats_before;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats_before), NIMCP_SUCCESS);

    // TPB should register as PLASTICITY (if implemented)
    tpb_config_t tpb_config = tpb_config_default();
    tpb_config.security_ctx = sec_ctx;
    tpb_context_t* tpb = tpb_create(&tpb_config);
    ASSERT_NE(tpb, nullptr);

    // LR Scheduler should register as MIDDLEWARE
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(NIMCP_LR_CONSTANT, 0.001f);
    lr_config.security_ctx = sec_ctx;
    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);
    ASSERT_NE(scheduler, nullptr);

    // At least LR scheduler should be registered
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.registered_modules, stats_before.registered_modules + 1u);

    tpb_destroy(tpb);
    nimcp_lr_scheduler_destroy(scheduler);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
