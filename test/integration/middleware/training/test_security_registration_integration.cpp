/**
 * @file test_security_registration_integration.cpp
 * @brief Integration tests for Training Module Security Registration
 *
 * Tests security registration across full training pipelines:
 * - Complete training workflow with security
 * - Security context sharing between modules
 * - Event propagation with security monitoring
 * - Module interaction under security supervision
 *
 * @note Part of Phase SR-1: Security Registration Integration Tests
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_lr_scheduler.h"
#include "middleware/training/nimcp_regularization.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_training_adapters.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "middleware/training/nimcp_optimizers.h"
#include "security/nimcp_security_integration.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class SecurityRegistrationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create security context with full monitoring
        sec_ctx = nimcp_sec_integration_create();
        ASSERT_NE(sec_ctx, nullptr);

        nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
        config.enable_continuous_monitoring = false;
        config.enable_self_monitoring = false;  // Disabled to avoid self-counting
        config.enable_event_logging = true;
        ASSERT_EQ(nimcp_sec_integration_init(sec_ctx, &config), NIMCP_SUCCESS);

        // Get baseline module count (security context may register itself)
        nimcp_sec_integration_stats_t baseline;
        if (nimcp_sec_get_stats(sec_ctx, &baseline) == NIMCP_SUCCESS) {
            baseline_active_modules = baseline.active_modules;
        }
    }

    void TearDown() override {
        if (sec_ctx) {
            nimcp_sec_integration_destroy(sec_ctx);
            sec_ctx = nullptr;
        }
    }

    nimcp_sec_integration_t* sec_ctx = nullptr;
    uint32_t baseline_active_modules = 0;
};

/* ============================================================================
 * Full Training Pipeline Integration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationIntegrationTest, FullPipeline_AllModulesRegister) {
    // Create complete training pipeline with shared security context

    // 1. LR Scheduler
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
        NIMCP_LR_COSINE_ANNEALING, 0.01f);
    lr_config.security_ctx = sec_ctx;
    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);
    ASSERT_NE(scheduler, nullptr);

    // 2. Gradient Manager
    nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
    gm_config.security_ctx = sec_ctx;
    gm_config.check_nan_inf = true;
    nimcp_gradient_manager_ctx_t* grad_mgr = nimcp_gradient_manager_create(&gm_config);
    ASSERT_NE(grad_mgr, nullptr);

    // 3. Regularization
    nimcp_regularization_config_t reg_config;
    memset(&reg_config, 0, sizeof(reg_config));
    reg_config.weight_reg_type = NIMCP_REG_L2;
    reg_config.weight_reg.l2 = nimcp_l2_default_config(0.0001f);
    reg_config.security_ctx = sec_ctx;
    nimcp_regularization_ctx_t* regularizer = nimcp_regularization_create(&reg_config);
    ASSERT_NE(regularizer, nullptr);

    // 4. Training Adapters
    learning_signal_adapter_config_t lsa_config = learning_signal_adapter_default_config();
    lsa_config.security_ctx = sec_ctx;
    learning_signal_adapter_t signal_adapter = learning_signal_adapter_create(&lsa_config);
    ASSERT_NE(signal_adapter, nullptr);

    weight_update_router_config_t wur_config = weight_update_router_default_config();
    wur_config.security_ctx = sec_ctx;
    weight_update_router_t weight_router = weight_update_router_create(&wur_config, nullptr);
    ASSERT_NE(weight_router, nullptr);

    training_event_manager_config_t tem_config = training_event_manager_default_config();
    tem_config.security_ctx = sec_ctx;
    training_event_manager_t event_mgr = training_event_manager_create(&tem_config, nullptr);
    ASSERT_NE(event_mgr, nullptr);

    // 5. Training-Plasticity Bridge
    tpb_config_t tpb_config = tpb_config_default();
    tpb_config.security_ctx = sec_ctx;
    tpb_context_t* tpb = tpb_create(&tpb_config);
    ASSERT_NE(tpb, nullptr);

    // Verify modules registered.
    // Note: Some modules may skip registration when bio-async router is unavailable.
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.registered_modules, 3u) << "At least 3 modules should register";
    EXPECT_GE(stats.active_modules, baseline_active_modules + 3u);

    // Cleanup in reverse order
    tpb_destroy(tpb);
    training_event_manager_destroy(event_mgr);
    weight_update_router_destroy(weight_router);
    learning_signal_adapter_destroy(signal_adapter);
    nimcp_regularization_destroy(regularizer);
    nimcp_gradient_manager_destroy(grad_mgr);
    nimcp_lr_scheduler_destroy(scheduler);

    // Verify all unregistered
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);
}

TEST_F(SecurityRegistrationIntegrationTest, Pipeline_ModulesWorkTogether) {
    // Create pipeline
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
        NIMCP_LR_STEP, 0.01f);
    lr_config.security_ctx = sec_ctx;
    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);

    nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
    gm_config.security_ctx = sec_ctx;
    nimcp_gradient_manager_ctx_t* grad_mgr = nimcp_gradient_manager_create(&gm_config);

    nimcp_regularization_config_t reg_config;
    memset(&reg_config, 0, sizeof(reg_config));
    reg_config.weight_reg_type = NIMCP_REG_L2;
    reg_config.weight_reg.l2 = nimcp_l2_default_config(0.01f);
    reg_config.security_ctx = sec_ctx;
    nimcp_regularization_ctx_t* regularizer = nimcp_regularization_create(&reg_config);

    ASSERT_NE(scheduler, nullptr);
    ASSERT_NE(grad_mgr, nullptr);
    ASSERT_NE(regularizer, nullptr);

    // Simulate training step
    float weights[] = {0.5f, -0.3f, 0.8f, -0.2f};
    float gradients[] = {0.01f, -0.02f, 0.03f, -0.01f};

    // Get learning rate
    float lr = nimcp_lr_scheduler_get_lr(scheduler);
    EXPECT_GT(lr, 0.0f);

    // Check gradient health
    nimcp_grad_health_t health = nimcp_gradient_check_health_ctx(grad_mgr, gradients, 4);
    EXPECT_EQ(health, NIMCP_GRAD_HEALTHY);

    // Compute regularization
    float reg_loss = nimcp_regularization_loss(regularizer, weights, 4);
    EXPECT_GT(reg_loss, 0.0f);

    // Step LR scheduler
    nimcp_lr_scheduler_step(scheduler);
    float new_lr = nimcp_lr_scheduler_get_lr(scheduler);
    EXPECT_GT(new_lr, 0.0f);

    // Security should still be tracking
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.active_modules, 3u);

    nimcp_regularization_destroy(regularizer);
    nimcp_gradient_manager_destroy(grad_mgr);
    nimcp_lr_scheduler_destroy(scheduler);
}

/* ============================================================================
 * Security Event Integration Tests
 * ============================================================================ */

static std::atomic<int> g_event_count{0};

static void security_event_callback(const nimcp_sec_event_t* event, void* user_data) {
    (void)user_data;
    if (event) {
        g_event_count++;
    }
}

TEST_F(SecurityRegistrationIntegrationTest, SecurityEvents_ModuleRegistrationEvents) {
    // Destroy existing context and create with callback
    nimcp_sec_integration_destroy(sec_ctx);

    sec_ctx = nimcp_sec_integration_create();
    ASSERT_NE(sec_ctx, nullptr);

    g_event_count = 0;

    nimcp_sec_integration_config_t config = nimcp_sec_integration_default_config();
    config.enable_continuous_monitoring = false;
    config.enable_event_logging = true;
    config.event_callback = security_event_callback;
    config.callback_user_data = nullptr;
    ASSERT_EQ(nimcp_sec_integration_init(sec_ctx, &config), NIMCP_SUCCESS);

    // Register a module - should trigger event
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
        NIMCP_LR_CONSTANT, 0.001f);
    lr_config.security_ctx = sec_ctx;
    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);
    ASSERT_NE(scheduler, nullptr);

    // Small delay for async events
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Should have received registration event
    EXPECT_GE(g_event_count.load(), 1);

    int count_after_create = g_event_count.load();

    // Unregister - should trigger event
    nimcp_lr_scheduler_destroy(scheduler);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Should have received unregistration event
    EXPECT_GT(g_event_count.load(), count_after_create);
}

/* ============================================================================
 * Concurrent Registration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationIntegrationTest, Concurrent_MultipleThreadsRegister) {
    const int NUM_THREADS = 4;
    const int MODULES_PER_THREAD = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    auto register_modules = [&](int thread_id) {
        for (int i = 0; i < MODULES_PER_THREAD; i++) {
            nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
                NIMCP_LR_CONSTANT, 0.001f * (thread_id + 1));
            config.security_ctx = sec_ctx;

            nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&config);
            if (scheduler) {
                success_count++;
                // Brief use
                nimcp_lr_scheduler_get_lr(scheduler);
                nimcp_lr_scheduler_destroy(scheduler);
            }
        }
    };

    // Launch threads
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(register_modules, t);
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * MODULES_PER_THREAD);

    // All should be unregistered now
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);
}

TEST_F(SecurityRegistrationIntegrationTest, Concurrent_MixedModuleTypes) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Thread 1: LR Schedulers
    threads.emplace_back([&]() {
        for (int i = 0; i < 5; i++) {
            nimcp_lr_scheduler_config_t config = nimcp_lr_scheduler_config_from_type(
                NIMCP_LR_CONSTANT, 0.001f);
            config.security_ctx = sec_ctx;
            nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&config);
            if (s) { success_count++; nimcp_lr_scheduler_destroy(s); }
        }
    });

    // Thread 2: Gradient Managers
    threads.emplace_back([&]() {
        for (int i = 0; i < 5; i++) {
            nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
            config.security_ctx = sec_ctx;
            nimcp_gradient_manager_ctx_t* g = nimcp_gradient_manager_create(&config);
            if (g) { success_count++; nimcp_gradient_manager_destroy(g); }
        }
    });

    // Thread 3: Regularizers
    threads.emplace_back([&]() {
        for (int i = 0; i < 5; i++) {
            nimcp_regularization_config_t config;
            memset(&config, 0, sizeof(config));
            config.weight_reg_type = NIMCP_REG_L1;
            config.weight_reg.l1 = nimcp_l1_default_config(0.01f);
            config.security_ctx = sec_ctx;
            nimcp_regularization_ctx_t* r = nimcp_regularization_create(&config);
            if (r) { success_count++; nimcp_regularization_destroy(r); }
        }
    });

    // Thread 4: Learning Signal Adapters
    threads.emplace_back([&]() {
        for (int i = 0; i < 5; i++) {
            learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
            config.security_ctx = sec_ctx;
            learning_signal_adapter_t a = learning_signal_adapter_create(&config);
            if (a) { success_count++; learning_signal_adapter_destroy(a); }
        }
    });

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), 20);
}

/* ============================================================================
 * Shared Context Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationIntegrationTest, SharedContext_MultipleModulesSameContext) {
    // All modules share the same security context
    std::vector<void*> modules;
    std::vector<int> module_types; // 0=lr, 1=gm, 2=reg

    // Create multiple modules
    for (int i = 0; i < 3; i++) {
        nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
            NIMCP_LR_CONSTANT, 0.001f);
        lr_config.security_ctx = sec_ctx;
        nimcp_lr_scheduler_ctx_t* s = nimcp_lr_scheduler_create(&lr_config);
        ASSERT_NE(s, nullptr);
        modules.push_back(s);
        module_types.push_back(0);

        nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
        gm_config.security_ctx = sec_ctx;
        nimcp_gradient_manager_ctx_t* g = nimcp_gradient_manager_create(&gm_config);
        ASSERT_NE(g, nullptr);
        modules.push_back(g);
        module_types.push_back(1);
    }

    // All should be registered
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules + 6u);

    // Destroy in random order
    size_t indices[] = {3, 0, 5, 2, 1, 4};
    for (size_t idx : indices) {
        if (module_types[idx] == 0) {
            nimcp_lr_scheduler_destroy((nimcp_lr_scheduler_ctx_t*)modules[idx]);
        } else {
            nimcp_gradient_manager_destroy((nimcp_gradient_manager_ctx_t*)modules[idx]);
        }
    }

    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules);
}

/* ============================================================================
 * Error Recovery Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationIntegrationTest, ErrorRecovery_ModuleFailureIsolated) {
    // Create several modules
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
        NIMCP_LR_CONSTANT, 0.001f);
    lr_config.security_ctx = sec_ctx;
    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);

    nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
    gm_config.security_ctx = sec_ctx;
    nimcp_gradient_manager_ctx_t* gm = nimcp_gradient_manager_create(&gm_config);

    ASSERT_NE(scheduler, nullptr);
    ASSERT_NE(gm, nullptr);

    // Simulate one module being destroyed (failure scenario)
    nimcp_lr_scheduler_destroy(scheduler);

    // Other module should still work
    float grads[] = {0.1f, 0.2f};
    EXPECT_EQ(nimcp_gradient_check_health_ctx(gm, grads, 2), NIMCP_GRAD_HEALTHY);

    // Security should still track remaining module
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules + 1u);

    nimcp_gradient_manager_destroy(gm);
}

/* ============================================================================
 * Training Loop Integration Tests
 * ============================================================================ */

TEST_F(SecurityRegistrationIntegrationTest, TrainingLoop_SecurityMonitorsDuringTraining) {
    // Create training components
    nimcp_lr_scheduler_config_t lr_config = nimcp_lr_scheduler_config_from_type(
        NIMCP_LR_EXPONENTIAL, 0.01f);
    lr_config.security_ctx = sec_ctx;
    nimcp_lr_scheduler_ctx_t* scheduler = nimcp_lr_scheduler_create(&lr_config);

    nimcp_gradient_manager_config_t gm_config = nimcp_gradient_manager_default_config();
    gm_config.security_ctx = sec_ctx;
    gm_config.check_nan_inf = true;
    nimcp_gradient_manager_ctx_t* grad_mgr = nimcp_gradient_manager_create(&gm_config);

    nimcp_regularization_config_t reg_config;
    memset(&reg_config, 0, sizeof(reg_config));
    reg_config.weight_reg_type = NIMCP_REG_L2;
    reg_config.weight_reg.l2 = nimcp_l2_default_config(0.0001f);
    reg_config.security_ctx = sec_ctx;
    nimcp_regularization_ctx_t* reg = nimcp_regularization_create(&reg_config);

    ASSERT_NE(scheduler, nullptr);
    ASSERT_NE(grad_mgr, nullptr);
    ASSERT_NE(reg, nullptr);

    // Simulate 100 training iterations
    float weights[] = {0.1f, 0.2f, 0.3f, 0.4f};

    for (int iter = 0; iter < 100; iter++) {
        // Get LR
        float lr = nimcp_lr_scheduler_get_lr(scheduler);
        EXPECT_GT(lr, 0.0f);

        // Compute gradients (simulated)
        float gradients[4];
        for (int j = 0; j < 4; j++) {
            gradients[j] = (float)(iter % 10 + 1) * 0.001f * ((j % 2) ? -1 : 1);
        }

        // Check gradient health
        nimcp_grad_health_t health = nimcp_gradient_check_health_ctx(
            grad_mgr, gradients, 4);
        EXPECT_EQ(health, NIMCP_GRAD_HEALTHY);

        // Compute regularization
        float reg_loss = nimcp_regularization_loss(reg, weights, 4);
        EXPECT_GE(reg_loss, 0.0f);

        // Update weights (simulated)
        for (int j = 0; j < 4; j++) {
            weights[j] -= lr * gradients[j];
        }

        // Step scheduler every 10 iterations
        if (iter % 10 == 0) {
            nimcp_lr_scheduler_step_epoch(scheduler);
        }
    }

    // Security should have tracked all modules throughout
    nimcp_sec_integration_stats_t stats;
    ASSERT_EQ(nimcp_sec_get_stats(sec_ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.active_modules, baseline_active_modules + 3u);

    nimcp_regularization_destroy(reg);
    nimcp_gradient_manager_destroy(grad_mgr);
    nimcp_lr_scheduler_destroy(scheduler);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
