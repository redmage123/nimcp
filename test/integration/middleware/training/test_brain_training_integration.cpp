/**
 * @file test_brain_training_integration.cpp
 * @brief Integration tests for Brain-Training Integration Module
 *
 * Phase TM-3: Tests integration of training modules with brain and security systems
 *
 * Test Categories:
 * - Context Lifecycle: Create, init, destroy
 * - Security Registration: Register/unregister with security system
 * - Loss Function Management: Create, get, destroy loss contexts
 * - Optimizer Management: Create, get, destroy optimizer contexts
 * - Training Operations: Compute loss, optimize, training step
 * - Statistics and Monitoring: Get stats, convergence detection
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_brain_training_integration.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "middleware/training/nimcp_optimizers.h"
#include "security/nimcp_security_integration.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class BrainTrainingIntegrationTest : public ::testing::Test {
protected:
    nimcp_brain_training_ctx_t* ctx = nullptr;
    nimcp_sec_integration_t* security_ctx = nullptr;

    void SetUp() override {
        // Create security context for integration tests
        security_ctx = nimcp_sec_integration_create();
        if (security_ctx) {
            nimcp_sec_integration_init(security_ctx, nullptr);
        }
    }

    void TearDown() override {
        if (ctx) {
            nimcp_brain_training_destroy(ctx);
            ctx = nullptr;
        }
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    void createDefaultContext() {
        ctx = nimcp_brain_training_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void createAndInitContext() {
        ctx = nimcp_brain_training_create(nullptr);
        ASSERT_NE(ctx, nullptr);
        ASSERT_EQ(nimcp_brain_training_init(ctx, security_ctx, nullptr), NIMCP_SUCCESS);
    }
};

/* ============================================================================
 * Context Lifecycle Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, CreateWithDefaultConfig) {
    ctx = nimcp_brain_training_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(BrainTrainingIntegrationTest, CreateWithCustomConfig) {
    nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
    config.default_loss_type = NIMCP_LOSS_CROSS_ENTROPY;
    config.default_optimizer = NIMCP_OPTIMIZER_ADAMW;
    config.default_learning_rate = 0.0001f;

    ctx = nimcp_brain_training_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(BrainTrainingIntegrationTest, CreateWithInvalidConfig) {
    nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
    config.default_learning_rate = -1.0f; // Invalid

    ctx = nimcp_brain_training_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(BrainTrainingIntegrationTest, InitWithNullContext) {
    nimcp_result_t result = nimcp_brain_training_init(nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(BrainTrainingIntegrationTest, InitWithSecurityContext) {
    createDefaultContext();
    nimcp_result_t result = nimcp_brain_training_init(ctx, security_ctx, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should be registered with security
    EXPECT_TRUE(nimcp_brain_training_is_security_registered(ctx));
}

TEST_F(BrainTrainingIntegrationTest, InitWithoutSecurityContext) {
    createDefaultContext();
    nimcp_result_t result = nimcp_brain_training_init(ctx, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should not be registered
    EXPECT_FALSE(nimcp_brain_training_is_security_registered(ctx));
}

TEST_F(BrainTrainingIntegrationTest, DestroyNullContext) {
    // Should not crash
    nimcp_brain_training_destroy(nullptr);
}

/* ============================================================================
 * Security Registration Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, SecurityRegistration) {
    createDefaultContext();
    ASSERT_NE(security_ctx, nullptr);

    nimcp_result_t result = nimcp_brain_training_register_security(ctx, security_ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(nimcp_brain_training_is_security_registered(ctx));
}

TEST_F(BrainTrainingIntegrationTest, SecurityUnregistration) {
    createAndInitContext();
    ASSERT_TRUE(nimcp_brain_training_is_security_registered(ctx));

    nimcp_result_t result = nimcp_brain_training_unregister_security(ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(nimcp_brain_training_is_security_registered(ctx));
}

TEST_F(BrainTrainingIntegrationTest, GetSecurityIds) {
    createAndInitContext();

    uint32_t loss_id = 0, opt_id = 0;
    nimcp_result_t result = nimcp_brain_training_get_security_ids(ctx, &loss_id, &opt_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(loss_id, 0u);
    EXPECT_GT(opt_id, 0u);
    EXPECT_NE(loss_id, opt_id);
}

TEST_F(BrainTrainingIntegrationTest, GetSecurityIdsNotRegistered) {
    createDefaultContext();
    // Don't init with security

    uint32_t loss_id = 0, opt_id = 0;
    nimcp_result_t result = nimcp_brain_training_get_security_ids(ctx, &loss_id, &opt_id);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Loss Function Management Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, CreateMSELoss) {
    createAndInitContext();

    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;

    nimcp_result_t result = nimcp_brain_training_create_loss(ctx, &config, &loss_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(loss_id, 0u);
}

TEST_F(BrainTrainingIntegrationTest, CreateMultipleLossFunctions) {
    createAndInitContext();

    uint32_t loss_ids[4];
    nimcp_loss_type_t types[] = {
        NIMCP_LOSS_MSE,
        NIMCP_LOSS_MAE,
        NIMCP_LOSS_CROSS_ENTROPY,
        NIMCP_LOSS_HUBER
    };

    for (int i = 0; i < 4; i++) {
        nimcp_loss_config_t config = nimcp_loss_default_config(types[i]);
        nimcp_result_t result = nimcp_brain_training_create_loss(ctx, &config, &loss_ids[i]);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_GT(loss_ids[i], 0u);
    }

    // All IDs should be unique
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            EXPECT_NE(loss_ids[i], loss_ids[j]);
        }
    }
}

TEST_F(BrainTrainingIntegrationTest, GetLossContext) {
    createAndInitContext();

    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &config, &loss_id), NIMCP_SUCCESS);

    nimcp_loss_context_t* loss_ctx = nimcp_brain_training_get_loss(ctx, loss_id);
    EXPECT_NE(loss_ctx, nullptr);
}

TEST_F(BrainTrainingIntegrationTest, GetNonExistentLossContext) {
    createAndInitContext();

    nimcp_loss_context_t* loss_ctx = nimcp_brain_training_get_loss(ctx, 999);
    EXPECT_EQ(loss_ctx, nullptr);
}

TEST_F(BrainTrainingIntegrationTest, DestroyLossContext) {
    createAndInitContext();

    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &config, &loss_id), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_brain_training_destroy_loss(ctx, loss_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should no longer be accessible
    EXPECT_EQ(nimcp_brain_training_get_loss(ctx, loss_id), nullptr);
}

/* ============================================================================
 * Optimizer Management Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, CreateAdamOptimizer) {
    createAndInitContext();

    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    uint32_t opt_id = 0;

    nimcp_result_t result = nimcp_brain_training_create_optimizer(ctx, &config, &opt_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(opt_id, 0u);
}

TEST_F(BrainTrainingIntegrationTest, CreateMultipleOptimizers) {
    createAndInitContext();

    uint32_t opt_ids[4];
    nimcp_optimizer_type_t types[] = {
        NIMCP_OPTIMIZER_SGD,
        NIMCP_OPTIMIZER_ADAM,
        NIMCP_OPTIMIZER_ADAMW,
        NIMCP_OPTIMIZER_RMSPROP
    };

    for (int i = 0; i < 4; i++) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(types[i]);
        nimcp_result_t result = nimcp_brain_training_create_optimizer(ctx, &config, &opt_ids[i]);
        EXPECT_EQ(result, NIMCP_SUCCESS);
        EXPECT_GT(opt_ids[i], 0u);
    }

    // All IDs should be unique
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            EXPECT_NE(opt_ids[i], opt_ids[j]);
        }
    }
}

TEST_F(BrainTrainingIntegrationTest, GetOptimizerContext) {
    createAndInitContext();

    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    uint32_t opt_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_optimizer(ctx, &config, &opt_id), NIMCP_SUCCESS);

    nimcp_optimizer_context_t* opt_ctx = nimcp_brain_training_get_optimizer(ctx, opt_id);
    EXPECT_NE(opt_ctx, nullptr);
}

TEST_F(BrainTrainingIntegrationTest, DestroyOptimizerContext) {
    createAndInitContext();

    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    uint32_t opt_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_optimizer(ctx, &config, &opt_id), NIMCP_SUCCESS);

    nimcp_result_t result = nimcp_brain_training_destroy_optimizer(ctx, opt_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should no longer be accessible
    EXPECT_EQ(nimcp_brain_training_get_optimizer(ctx, opt_id), nullptr);
}

/* ============================================================================
 * Training Mode Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, SetTrainingMode) {
    createAndInitContext();

    EXPECT_EQ(nimcp_brain_training_get_mode(ctx), NIMCP_TRAINING_MODE_TRAIN);

    EXPECT_EQ(nimcp_brain_training_set_mode(ctx, NIMCP_TRAINING_MODE_EVAL), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_training_get_mode(ctx), NIMCP_TRAINING_MODE_EVAL);

    EXPECT_EQ(nimcp_brain_training_set_mode(ctx, NIMCP_TRAINING_MODE_INFERENCE), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_training_get_mode(ctx), NIMCP_TRAINING_MODE_INFERENCE);
}

/* ============================================================================
 * Training Operations Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, ComputeMSELoss) {
    createAndInitContext();

    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &config, &loss_id), NIMCP_SUCCESS);

    float predictions[] = {0.5f, 0.7f, 0.3f, 0.9f};
    float targets[] = {1.0f, 0.0f, 0.5f, 0.8f};
    float gradients[4] = {0};
    float loss_value = 0.0f;

    nimcp_result_t result = nimcp_brain_training_compute_loss(
        ctx, loss_id, predictions, targets, 4, 1, &loss_value, gradients
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(loss_value, 0.0f);
}

TEST_F(BrainTrainingIntegrationTest, OptimizeWithAdam) {
    createAndInitContext();

    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    uint32_t opt_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_optimizer(ctx, &config, &opt_id), NIMCP_SUCCESS);

    // Initialize optimizer state
    nimcp_optimizer_context_t* opt_ctx = nimcp_brain_training_get_optimizer(ctx, opt_id);
    ASSERT_NE(opt_ctx, nullptr);
    nimcp_optimizer_init_params(opt_ctx, 4);

    float params[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float gradients[] = {0.1f, -0.1f, 0.2f, -0.05f};
    float original_params[4];
    memcpy(original_params, params, sizeof(params));

    nimcp_result_t result = nimcp_brain_training_optimize(ctx, opt_id, params, gradients, 4);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Params should have changed
    bool params_changed = false;
    for (int i = 0; i < 4; i++) {
        if (fabsf(params[i] - original_params[i]) > 1e-6f) {
            params_changed = true;
            break;
        }
    }
    EXPECT_TRUE(params_changed);
}

TEST_F(BrainTrainingIntegrationTest, FullTrainingStep) {
    createAndInitContext();

    // Create loss function
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &loss_config, &loss_id), NIMCP_SUCCESS);

    // Create optimizer
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    uint32_t opt_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_optimizer(ctx, &opt_config, &opt_id), NIMCP_SUCCESS);

    // Initialize optimizer
    nimcp_optimizer_context_t* opt_ctx = nimcp_brain_training_get_optimizer(ctx, opt_id);
    ASSERT_NE(opt_ctx, nullptr);
    nimcp_optimizer_init_params(opt_ctx, 4);

    float params[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float predictions[] = {0.5f, 0.7f, 0.3f, 0.9f};
    float targets[] = {1.0f, 0.0f, 0.5f, 0.8f};
    float loss_value = 0.0f;

    // batch_size=4, output_size=1 (scalar outputs), param_count=4
    nimcp_result_t result = nimcp_brain_training_step(
        ctx, loss_id, opt_id, params, predictions, targets, 4, 1, 4, &loss_value
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(loss_value, 0.0f);
}

TEST_F(BrainTrainingIntegrationTest, MultiEpochTraining) {
    createAndInitContext();

    // Create MSE loss
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &loss_config, &loss_id), NIMCP_SUCCESS);

    // Create Adam optimizer with higher learning rate
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    opt_config.params.adam.learning_rate = 0.1f;
    uint32_t opt_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_optimizer(ctx, &opt_config, &opt_id), NIMCP_SUCCESS);

    // Initialize optimizer
    nimcp_optimizer_context_t* opt_ctx = nimcp_brain_training_get_optimizer(ctx, opt_id);
    nimcp_optimizer_init_params(opt_ctx, 4);

    float params[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float targets[] = {1.0f, 0.5f, 0.25f, 0.75f};
    float first_loss = 0.0f, last_loss = 0.0f;

    for (int epoch = 0; epoch < 100; epoch++) {
        // Use params as predictions for simple test
        // batch_size=4, output_size=1, param_count=4
        float loss_value = 0.0f;
        nimcp_result_t result = nimcp_brain_training_step(
            ctx, loss_id, opt_id, params, params, targets, 4, 1, 4, &loss_value
        );
        ASSERT_EQ(result, NIMCP_SUCCESS);

        if (epoch == 0) first_loss = loss_value;
        last_loss = loss_value;
    }

    // Loss should decrease
    EXPECT_LT(last_loss, first_loss);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, GetStatistics) {
    createAndInitContext();

    nimcp_training_session_stats_t stats;
    nimcp_result_t result = nimcp_brain_training_get_stats(ctx, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_epochs, 0u);
    EXPECT_EQ(stats.total_batches, 0u);
}

TEST_F(BrainTrainingIntegrationTest, StatisticsAfterTraining) {
    createAndInitContext();

    // Create loss and optimizer
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &loss_config, &loss_id), NIMCP_SUCCESS);

    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    uint32_t opt_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_optimizer(ctx, &opt_config, &opt_id), NIMCP_SUCCESS);

    nimcp_optimizer_context_t* opt_ctx = nimcp_brain_training_get_optimizer(ctx, opt_id);
    nimcp_optimizer_init_params(opt_ctx, 4);

    float params[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float predictions[] = {0.5f, 0.7f, 0.3f, 0.9f};
    float targets[] = {1.0f, 0.0f, 0.5f, 0.8f};

    // Run 10 training steps
    // batch_size=4, output_size=1, param_count=4
    for (int i = 0; i < 10; i++) {
        float loss_value = 0.0f;
        nimcp_brain_training_step(
            ctx, loss_id, opt_id, params, predictions, targets, 4, 1, 4, &loss_value
        );
    }

    nimcp_training_session_stats_t stats;
    ASSERT_EQ(nimcp_brain_training_get_stats(ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_batches, 10u);
    EXPECT_EQ(stats.weight_updates, 10u);
    EXPECT_GT(stats.total_loss, 0.0);
}

TEST_F(BrainTrainingIntegrationTest, ResetStatistics) {
    createAndInitContext();

    // Create loss and do some training
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &loss_config, &loss_id), NIMCP_SUCCESS);

    float predictions[] = {0.5f};
    float targets[] = {1.0f};
    float loss_value = 0.0f;
    nimcp_brain_training_compute_loss(ctx, loss_id, predictions, targets, 1, 1, &loss_value, nullptr);

    // Reset
    nimcp_brain_training_reset_stats(ctx);

    nimcp_training_session_stats_t stats;
    nimcp_brain_training_get_stats(ctx, &stats);
    EXPECT_EQ(stats.total_batches, 0u);
    EXPECT_EQ(stats.total_loss, 0.0);
}

/* ============================================================================
 * Event Callback Tests
 * ============================================================================ */

static int g_event_count = 0;
static nimcp_training_event_type_t g_last_event_type = NIMCP_TRAINING_EVENT_NONE;

static void test_event_callback(const nimcp_training_event_t* event, void* user_data) {
    g_event_count++;
    g_last_event_type = event->type;
}

TEST_F(BrainTrainingIntegrationTest, EventCallback) {
    createAndInitContext();

    g_event_count = 0;
    g_last_event_type = NIMCP_TRAINING_EVENT_NONE;

    nimcp_brain_training_register_callback(ctx, test_event_callback, nullptr);

    // Create loss and compute
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &loss_config, &loss_id), NIMCP_SUCCESS);

    float predictions[] = {0.5f};
    float targets[] = {1.0f};
    float loss_value = 0.0f;
    nimcp_brain_training_compute_loss(ctx, loss_id, predictions, targets, 1, 1, &loss_value, nullptr);

    EXPECT_GT(g_event_count, 0);
    EXPECT_EQ(g_last_event_type, NIMCP_TRAINING_EVENT_LOSS_COMPUTED);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, EventTypeNames) {
    EXPECT_STREQ(nimcp_training_event_type_name(NIMCP_TRAINING_EVENT_NONE), "NONE");
    EXPECT_STREQ(nimcp_training_event_type_name(NIMCP_TRAINING_EVENT_EPOCH_START), "EPOCH_START");
    EXPECT_STREQ(nimcp_training_event_type_name(NIMCP_TRAINING_EVENT_LOSS_COMPUTED), "LOSS_COMPUTED");
    EXPECT_STREQ(nimcp_training_event_type_name(NIMCP_TRAINING_EVENT_CONVERGENCE), "CONVERGENCE");
}

TEST_F(BrainTrainingIntegrationTest, ModeNames) {
    EXPECT_STREQ(nimcp_training_mode_name(NIMCP_TRAINING_MODE_TRAIN), "TRAIN");
    EXPECT_STREQ(nimcp_training_mode_name(NIMCP_TRAINING_MODE_EVAL), "EVAL");
    EXPECT_STREQ(nimcp_training_mode_name(NIMCP_TRAINING_MODE_INFERENCE), "INFERENCE");
}

TEST_F(BrainTrainingIntegrationTest, ConfigValidation) {
    nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
    EXPECT_EQ(nimcp_brain_training_validate_config(&config), NIMCP_SUCCESS);

    config.default_learning_rate = -1.0f;
    EXPECT_NE(nimcp_brain_training_validate_config(&config), NIMCP_SUCCESS);

    config.default_learning_rate = 0.001f;
    config.convergence_threshold = -1.0f;
    EXPECT_NE(nimcp_brain_training_validate_config(&config), NIMCP_SUCCESS);
}

/* ============================================================================
 * Convergence/Divergence Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, InitialConvergenceState) {
    createAndInitContext();
    EXPECT_FALSE(nimcp_brain_training_is_converged(ctx));
    EXPECT_FALSE(nimcp_brain_training_is_diverged(ctx));
}

/* ============================================================================
 * Integration with Loss and Optimizer Module Tests
 * ============================================================================ */

TEST_F(BrainTrainingIntegrationTest, LossOptimizerIntegration) {
    createAndInitContext();

    // Create cross-entropy loss for classification
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_BINARY_CROSS_ENTROPY);
    uint32_t loss_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_loss(ctx, &loss_config, &loss_id), NIMCP_SUCCESS);

    // Create AdamW optimizer
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAMW);
    opt_config.params.adamw.weight_decay = 0.01f;
    uint32_t opt_id = 0;
    ASSERT_EQ(nimcp_brain_training_create_optimizer(ctx, &opt_config, &opt_id), NIMCP_SUCCESS);

    // Verify both are accessible
    EXPECT_NE(nimcp_brain_training_get_loss(ctx, loss_id), nullptr);
    EXPECT_NE(nimcp_brain_training_get_optimizer(ctx, opt_id), nullptr);

    // Clean up one
    EXPECT_EQ(nimcp_brain_training_destroy_loss(ctx, loss_id), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_training_get_loss(ctx, loss_id), nullptr);

    // Other should still exist
    EXPECT_NE(nimcp_brain_training_get_optimizer(ctx, opt_id), nullptr);
}

/* ============================================================================
 * Main Function
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
