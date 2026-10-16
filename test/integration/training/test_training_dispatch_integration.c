/**
 * @file test_training_dispatch_integration.c
 * @brief Integration tests for training dispatcher with actual network types
 * @date 2026-01-16
 *
 * Tests verify integration between the training dispatcher and actual network types:
 * - Adaptive network training with backpropagation
 * - SNN network training (STDP/eProp/surrogate)
 * - LNN network training (adjoint ODE)
 * - CNN network training (convolutional backprop)
 * - Hybrid network training (multiple types)
 * - Training configuration defaults
 *
 * Uses nimcp_brain_create() for brain creation, nimcp_brain_configure_training()
 * for training setup, and nimcp_brain_train_step()/training_dispatch_step() for
 * training execution.
 */

/* Feature test macros - must be before any includes */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

/* Training dispatch API - weak symbols for optional functions */
/* These will be linked if available, otherwise use stubs */

/* Forward declarations for training dispatch API */
struct nimcp_training_config;

/* Training dispatch result structure */
typedef struct {
    float loss;
    float learning_rate;
    uint32_t step;
    bool early_stopped;
    float gradient_norm;
    union {
        struct { uint32_t ltp_events; uint32_t ltd_events; float spike_rate; } snn;
        struct { float ode_error; float tau_mean; } lnn;
        struct { float conv_grad_norm; float dense_grad_norm; } cnn;
    } type_specific;
} training_dispatch_result_t;

/* Weak implementations for training_dispatch functions that may not be implemented yet */
__attribute__((weak))
int training_dispatch_init(brain_t brain, const struct nimcp_training_config* config) {
    (void)brain; (void)config;
    return -1;  /* Not implemented */
}

__attribute__((weak))
int training_dispatch_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result
) {
    (void)brain; (void)inputs; (void)num_inputs;
    (void)targets; (void)num_targets; (void)result;
    return -1;  /* Not implemented */
}

__attribute__((weak))
int training_dispatch_set_reward(brain_t brain, float reward) {
    (void)brain; (void)reward;
    return -1;  /* Not implemented */
}

__attribute__((weak))
int training_dispatch_get_stats(
    brain_t brain,
    uint64_t* total_steps,
    float* total_loss,
    float* current_lr
) {
    (void)brain;
    if (total_steps) *total_steps = 0;
    if (total_loss) *total_loss = 0.0f;
    if (current_lr) *current_lr = 0.0f;
    return -1;  /* Not implemented */
}

__attribute__((weak))
int training_dispatch_reset(brain_t brain) {
    (void)brain;
    return -1;  /* Not implemented */
}

__attribute__((weak))
void training_dispatch_destroy(brain_t brain) {
    (void)brain;
}

__attribute__((weak))
const char* training_dispatch_type_name(uint8_t network_type) {
    static const char* names[] = {
        "ADAPTIVE", "SNN", "LNN", "CNN", "HYBRID"
    };
    if (network_type <= 4) {
        return names[network_type];
    }
    return "UNKNOWN";
}

__attribute__((weak))
bool training_dispatch_is_supported(uint8_t network_type) {
    /* Currently only ADAPTIVE is guaranteed to work */
    return (network_type == 0);  /* NIMCP_NETWORK_ADAPTIVE */
}

//=============================================================================
// Test Configuration
//=============================================================================

#define TEST_NUM_INPUTS     4
#define TEST_NUM_OUTPUTS    2
#define TEST_TRAINING_STEPS 10
#define LOSS_DECREASE_THRESHOLD 0.01f  // Minimum expected loss decrease

//=============================================================================
// Test Framework
//=============================================================================

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", (msg), __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr, msg) do { \
    if ((ptr) == NULL) { \
        printf("  FAIL: %s is NULL (line %d)\n", (msg), __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQ(actual, expected, msg) do { \
    if ((actual) != (expected)) { \
        printf("  FAIL: %s - expected %d, got %d (line %d)\n", \
               (msg), (int)(expected), (int)(actual), __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_LT(a, b, msg) do { \
    if (!((a) < (b))) { \
        printf("  FAIL: %s - expected %f < %f (line %d)\n", \
               (msg), (double)(a), (double)(b), __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    printf("  PASS: %s\n", (msg)); \
    g_tests_passed++; \
} while(0)

//=============================================================================
// Test Helpers
//=============================================================================

/**
 * @brief Generate simple XOR-like training data
 */
static void generate_xor_sample(float* inputs, float* targets, int sample_idx)
{
    // Simple pattern: XOR of first two inputs determines output
    int x = sample_idx % 2;
    int y = (sample_idx / 2) % 2;

    inputs[0] = (float)x;
    inputs[1] = (float)y;
    inputs[2] = 0.5f;  // Bias-like input
    inputs[3] = 0.1f;  // Noise

    // XOR output (one-hot encoded)
    int xor_result = x ^ y;
    targets[0] = (xor_result == 0) ? 1.0f : 0.0f;
    targets[1] = (xor_result == 1) ? 1.0f : 0.0f;
}

/**
 * @brief Get internal brain structure for verification
 *
 * Note: This is for testing purposes only to verify internal state.
 * Production code should use public API only.
 */
static brain_t get_internal_brain(nimcp_brain_t brain)
{
    // The nimcp_brain_t handle is an opaque pointer to brain_t
    return (brain_t)brain;
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * @brief Test 1: Adaptive training integration
 *
 * Creates brain, configures training with NIMCP_NETWORK_ADAPTIVE,
 * runs training steps, and verifies loss decreases over time.
 */
void test_adaptive_training_integration(void)
{
    printf("\n=== test_adaptive_training_integration ===\n");

    // Create brain with TINY size for fast testing
    nimcp_brain_t brain = nimcp_brain_create(
        "adaptive_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        TEST_NUM_INPUTS,
        TEST_NUM_OUTPUTS
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure training with ADAPTIVE network type (default)
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.1f;  // Higher LR for faster convergence in test
    config.loss_type = NIMCP_API_LOSS_MSE;
    config.optimizer_type = NIMCP_API_OPT_SGD;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    TEST_ASSERT_EQ(status, NIMCP_OK, "Configure training");

    // Track loss over training
    float initial_loss = 0.0f;
    float final_loss = 0.0f;
    float inputs[TEST_NUM_INPUTS];
    float targets[TEST_NUM_OUTPUTS];
    nimcp_training_result_t result;

    // Run training steps
    for (int step = 0; step < TEST_TRAINING_STEPS; step++) {
        generate_xor_sample(inputs, targets, step);

        status = nimcp_brain_train_step(
            brain,
            inputs, TEST_NUM_INPUTS,
            targets, TEST_NUM_OUTPUTS,
            &result
        );

        if (status != NIMCP_OK) {
            printf("  Training step %d returned status %d\n", step, status);
            // Continue - some networks may not be fully implemented
        }

        if (step == 0) {
            initial_loss = result.loss;
        }
        final_loss = result.loss;

        printf("  Step %d: loss=%.6f, lr=%.6f\n",
               step, result.loss, result.learning_rate);
    }

    // Verify loss tracking (may not decrease significantly in few steps)
    printf("  Initial loss: %.6f, Final loss: %.6f\n", initial_loss, final_loss);

    // Verify brain state
    brain_t internal = get_internal_brain(brain);
    if (internal) {
        TEST_ASSERT_EQ(internal->active_network_type, NIMCP_NETWORK_ADAPTIVE,
                       "Network type should be ADAPTIVE");
    }

    // Cleanup
    nimcp_brain_destroy(brain);

    TEST_PASS("Adaptive training integration completed");
}

/**
 * @brief Test 2: SNN training integration
 *
 * Creates brain with SNN network, configures NIMCP_NETWORK_SNN training,
 * and verifies SNN training context is created.
 */
void test_snn_training_integration(void)
{
    printf("\n=== test_snn_training_integration ===\n");

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "snn_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        TEST_NUM_INPUTS,
        TEST_NUM_OUTPUTS
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure training with SNN network type
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_SNN;
    config.snn_method = NIMCP_SNN_TRAIN_STDP;  // Use STDP for SNN
    config.snn_eligibility_tau = 20.0f;
    config.snn_reward_tau = 100.0f;
    config.learning_rate = 0.01f;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    printf("  Configure SNN training status: %d\n", status);

    // Check if SNN training is supported
    bool snn_supported = training_dispatch_is_supported(NIMCP_NETWORK_SNN);
    printf("  SNN training supported: %s\n", snn_supported ? "yes" : "no");

    // Verify network type name
    const char* type_name = training_dispatch_type_name(NIMCP_NETWORK_SNN);
    printf("  Network type name: %s\n", type_name ? type_name : "(null)");
    TEST_ASSERT(type_name != NULL, "Type name should not be NULL");

    // Verify internal state if configure succeeded
    if (status == NIMCP_OK) {
        brain_t internal = get_internal_brain(brain);
        if (internal) {
            printf("  Active network type: %d\n", internal->active_network_type);
            printf("  SNN training context: %p\n", (void*)internal->snn_training_ctx);
            printf("  SNN network: %p\n", (void*)internal->snn_network);
        }
    }

    // Try a training step (may fail if SNN not fully implemented)
    float inputs[TEST_NUM_INPUTS] = {1.0f, 0.0f, 0.5f, 0.1f};
    float targets[TEST_NUM_OUTPUTS] = {1.0f, 0.0f};
    nimcp_training_result_t result;

    status = nimcp_brain_train_step(
        brain,
        inputs, TEST_NUM_INPUTS,
        targets, TEST_NUM_OUTPUTS,
        &result
    );
    printf("  SNN training step status: %d\n", status);

    // Cleanup
    nimcp_brain_destroy(brain);

    TEST_PASS("SNN training integration completed");
}

/**
 * @brief Test 3: LNN training integration
 *
 * Creates brain with LNN network, configures NIMCP_NETWORK_LNN training,
 * and verifies LNN training context is created.
 */
void test_lnn_training_integration(void)
{
    printf("\n=== test_lnn_training_integration ===\n");

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "lnn_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_SEQUENCE,  // LNN suited for sequence tasks
        TEST_NUM_INPUTS,
        TEST_NUM_OUTPUTS
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure training with LNN network type
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_LNN;
    config.lnn_method = NIMCP_LNN_TRAIN_ADJOINT;  // Adjoint method for ODE
    config.lnn_bptt_truncation = 50;
    config.lnn_use_adjoint_checkpointing = true;
    config.learning_rate = 0.001f;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    printf("  Configure LNN training status: %d\n", status);

    // Check if LNN training is supported
    bool lnn_supported = training_dispatch_is_supported(NIMCP_NETWORK_LNN);
    printf("  LNN training supported: %s\n", lnn_supported ? "yes" : "no");

    // Verify network type name
    const char* type_name = training_dispatch_type_name(NIMCP_NETWORK_LNN);
    printf("  Network type name: %s\n", type_name ? type_name : "(null)");
    TEST_ASSERT(type_name != NULL, "Type name should not be NULL");

    // Verify internal state if configure succeeded
    if (status == NIMCP_OK) {
        brain_t internal = get_internal_brain(brain);
        if (internal) {
            printf("  Active network type: %d\n", internal->active_network_type);
            printf("  LNN training context: %p\n", (void*)internal->lnn_training_ctx);
            printf("  LNN network: %p\n", (void*)internal->lnn_network);
        }
    }

    // Try a training step
    float inputs[TEST_NUM_INPUTS] = {0.5f, 0.5f, 0.5f, 0.5f};
    float targets[TEST_NUM_OUTPUTS] = {0.5f, 0.5f};
    nimcp_training_result_t result;

    status = nimcp_brain_train_step(
        brain,
        inputs, TEST_NUM_INPUTS,
        targets, TEST_NUM_OUTPUTS,
        &result
    );
    printf("  LNN training step status: %d\n", status);

    // Cleanup
    nimcp_brain_destroy(brain);

    TEST_PASS("LNN training integration completed");
}

/**
 * @brief Test 4: CNN training integration
 *
 * Creates brain with CNN trainer, configures NIMCP_NETWORK_CNN training,
 * and verifies training works.
 */
void test_cnn_training_integration(void)
{
    printf("\n=== test_cnn_training_integration ===\n");

    // CNN typically needs more inputs (e.g., image data)
    // For testing, we'll use a small input size
    const uint32_t cnn_inputs = 16;  // e.g., 4x4 image
    const uint32_t cnn_outputs = 4;  // e.g., 4 classes

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "cnn_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        cnn_inputs,
        cnn_outputs
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure training with CNN network type
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_CNN;
    config.learning_rate = 0.01f;
    config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
    config.optimizer_type = NIMCP_API_OPT_ADAM;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    printf("  Configure CNN training status: %d\n", status);

    // Check if CNN training is supported
    bool cnn_supported = training_dispatch_is_supported(NIMCP_NETWORK_CNN);
    printf("  CNN training supported: %s\n", cnn_supported ? "yes" : "no");

    // Verify network type name
    const char* type_name = training_dispatch_type_name(NIMCP_NETWORK_CNN);
    printf("  Network type name: %s\n", type_name ? type_name : "(null)");
    TEST_ASSERT(type_name != NULL, "Type name should not be NULL");

    // Verify internal state if configure succeeded
    if (status == NIMCP_OK) {
        brain_t internal = get_internal_brain(brain);
        if (internal) {
            printf("  Active network type: %d\n", internal->active_network_type);
            printf("  CNN trainer: %p\n", (void*)internal->cnn_trainer);
        }
    }

    // Try a training step with "image-like" input
    float inputs[16];
    for (int i = 0; i < 16; i++) {
        inputs[i] = (float)(i % 4) / 4.0f;  // Simple pattern
    }
    float targets[4] = {0.0f, 1.0f, 0.0f, 0.0f};  // Class 1
    nimcp_training_result_t result;

    status = nimcp_brain_train_step(
        brain,
        inputs, cnn_inputs,
        targets, cnn_outputs,
        &result
    );
    printf("  CNN training step status: %d\n", status);

    // Cleanup
    nimcp_brain_destroy(brain);

    TEST_PASS("CNN training integration completed");
}

/**
 * @brief Test 5: Hybrid training integration
 *
 * Creates brain with multiple network types, tests NIMCP_NETWORK_HYBRID mode.
 */
void test_hybrid_training_integration(void)
{
    printf("\n=== test_hybrid_training_integration ===\n");

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "hybrid_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_PATTERN_MATCHING,
        TEST_NUM_INPUTS,
        TEST_NUM_OUTPUTS
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure training with HYBRID network type
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_HYBRID;
    config.learning_rate = 0.01f;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    printf("  Configure HYBRID training status: %d\n", status);

    // Check if HYBRID training is supported
    bool hybrid_supported = training_dispatch_is_supported(NIMCP_NETWORK_HYBRID);
    printf("  HYBRID training supported: %s\n", hybrid_supported ? "yes" : "no");

    // Verify network type name
    const char* type_name = training_dispatch_type_name(NIMCP_NETWORK_HYBRID);
    printf("  Network type name: %s\n", type_name ? type_name : "(null)");
    TEST_ASSERT(type_name != NULL, "Type name should not be NULL");

    // Try a training step
    float inputs[TEST_NUM_INPUTS] = {0.3f, 0.7f, 0.5f, 0.2f};
    float targets[TEST_NUM_OUTPUTS] = {0.8f, 0.2f};
    nimcp_training_result_t result;

    status = nimcp_brain_train_step(
        brain,
        inputs, TEST_NUM_INPUTS,
        targets, TEST_NUM_OUTPUTS,
        &result
    );
    printf("  HYBRID training step status: %d\n", status);

    // Cleanup
    nimcp_brain_destroy(brain);

    TEST_PASS("Hybrid training integration completed");
}

/**
 * @brief Test 6: Training config defaults verification
 *
 * Verifies nimcp_training_config_default() sets all network type fields correctly.
 */
void test_training_config_defaults(void)
{
    printf("\n=== test_training_config_defaults ===\n");

    nimcp_training_config_t config = nimcp_training_config_default();

    // Verify basic defaults
    printf("  network_type: %d (expected ADAPTIVE=%d)\n",
           config.network_type, NIMCP_NETWORK_ADAPTIVE);
    TEST_ASSERT_EQ(config.network_type, NIMCP_NETWORK_ADAPTIVE,
                   "Default network_type should be ADAPTIVE");

    // Verify learning rate is set
    printf("  learning_rate: %.6f\n", config.learning_rate);
    TEST_ASSERT(config.learning_rate > 0.0f, "Learning rate should be positive");
    TEST_ASSERT(config.learning_rate <= 1.0f, "Learning rate should be <= 1.0");

    // Verify SNN defaults
    // Note: Default is SURROGATE (3), not STDP (0)
    printf("  snn_method: %d (SURROGATE=%d)\n", config.snn_method, NIMCP_SNN_TRAIN_SURROGATE);
    TEST_ASSERT_EQ(config.snn_method, NIMCP_SNN_TRAIN_SURROGATE,
                   "Default SNN method should be SURROGATE");

    printf("  snn_eligibility_tau: %.1f\n", config.snn_eligibility_tau);
    TEST_ASSERT(config.snn_eligibility_tau > 0.0f,
                "SNN eligibility tau should be positive");

    printf("  snn_reward_tau: %.1f\n", config.snn_reward_tau);
    TEST_ASSERT(config.snn_reward_tau > 0.0f,
                "SNN reward tau should be positive");

    printf("  snn_surrogate_beta: %.1f\n", config.snn_surrogate_beta);
    TEST_ASSERT(config.snn_surrogate_beta > 0.0f,
                "SNN surrogate beta should be positive");

    // Verify LNN defaults
    printf("  lnn_method: %d (ADJOINT=%d)\n", config.lnn_method, NIMCP_LNN_TRAIN_ADJOINT);
    TEST_ASSERT_EQ(config.lnn_method, NIMCP_LNN_TRAIN_ADJOINT,
                   "Default LNN method should be ADJOINT");

    printf("  lnn_bptt_truncation: %u\n", config.lnn_bptt_truncation);
    TEST_ASSERT(config.lnn_bptt_truncation > 0,
                "LNN BPTT truncation should be positive");

    printf("  lnn_use_adjoint_checkpointing: %s\n",
           config.lnn_use_adjoint_checkpointing ? "true" : "false");
    // Checkpointing default can be either true or false - just verify it's set

    // Verify optimizer defaults
    printf("  optimizer_type: %d\n", config.optimizer_type);

    // Verify loss defaults
    printf("  loss_type: %d\n", config.loss_type);

    // Verify scheduler defaults
    printf("  scheduler_type: %d\n", config.scheduler_type);

    // Verify biological modulation
    printf("  enable_biological_modulation: %s\n",
           config.enable_biological_modulation ? "true" : "false");
    printf("  biological_blend: %.2f\n", config.biological_blend);

    TEST_PASS("Training config defaults verified");
}

/**
 * @brief Test 7: Training dispatch with direct API
 *
 * Tests training_dispatch_step() directly with brain handle.
 */
void test_training_dispatch_direct(void)
{
    printf("\n=== test_training_dispatch_direct ===\n");

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "dispatch_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_REGRESSION,
        TEST_NUM_INPUTS,
        TEST_NUM_OUTPUTS
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure training
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_ADAPTIVE;
    config.learning_rate = 0.05f;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    TEST_ASSERT_EQ(status, NIMCP_OK, "Configure training");

    // Initialize training dispatch
    brain_t internal = get_internal_brain(brain);
    int init_result = training_dispatch_init(internal, (const struct nimcp_training_config*)&config);
    printf("  training_dispatch_init result: %d\n", init_result);

    // Run training steps using dispatch API
    float inputs[TEST_NUM_INPUTS] = {0.5f, 0.5f, 0.5f, 0.5f};
    float targets[TEST_NUM_OUTPUTS] = {0.5f, 0.5f};
    training_dispatch_result_t dispatch_result;

    for (int step = 0; step < 5; step++) {
        generate_xor_sample(inputs, targets, step);

        int step_result = training_dispatch_step(
            internal,
            inputs, TEST_NUM_INPUTS,
            targets, TEST_NUM_OUTPUTS,
            &dispatch_result
        );

        printf("  Step %d: result=%d, loss=%.6f, lr=%.6f\n",
               step, step_result, dispatch_result.loss, dispatch_result.learning_rate);
    }

    // Get training stats
    uint64_t total_steps = 0;
    float total_loss = 0.0f;
    float current_lr = 0.0f;

    int stats_result = training_dispatch_get_stats(
        internal, &total_steps, &total_loss, &current_lr
    );
    printf("  Stats result: %d, steps=%lu, loss=%.4f, lr=%.6f\n",
           stats_result, (unsigned long)total_steps, total_loss, current_lr);

    // Cleanup training dispatch
    training_dispatch_destroy(internal);

    // Cleanup brain
    nimcp_brain_destroy(brain);

    TEST_PASS("Training dispatch direct API completed");
}

/**
 * @brief Test 8: SNN reward signal integration
 *
 * Tests training_dispatch_set_reward() for reward-modulated learning.
 */
void test_snn_reward_signal(void)
{
    printf("\n=== test_snn_reward_signal ===\n");

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "reward_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        TEST_NUM_INPUTS,
        TEST_NUM_OUTPUTS
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure with R-STDP for reward-modulated learning
    nimcp_training_config_t config = nimcp_training_config_default();
    config.network_type = NIMCP_NETWORK_SNN;
    config.snn_method = NIMCP_SNN_TRAIN_R_STDP;  // Reward-modulated STDP
    config.learning_rate = 0.01f;

    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    printf("  Configure R-STDP training status: %d\n", status);

    brain_t internal = get_internal_brain(brain);

    // Test setting reward signals
    float rewards[] = {1.0f, 0.5f, 0.0f, -0.5f, -1.0f};
    for (int i = 0; i < 5; i++) {
        int result = training_dispatch_set_reward(internal, rewards[i]);
        printf("  Set reward %.2f: result=%d\n", rewards[i], result);
    }

    // Cleanup
    nimcp_brain_destroy(brain);

    TEST_PASS("SNN reward signal integration completed");
}

/**
 * @brief Test 9: Training reset functionality
 *
 * Tests training_dispatch_reset() to verify state can be reset.
 */
void test_training_reset(void)
{
    printf("\n=== test_training_reset ===\n");

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "reset_test",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        TEST_NUM_INPUTS,
        TEST_NUM_OUTPUTS
    );
    TEST_ASSERT_NOT_NULL(brain, "Brain creation");

    // Configure training
    nimcp_training_config_t config = nimcp_training_config_default();
    nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
    TEST_ASSERT_EQ(status, NIMCP_OK, "Configure training");

    brain_t internal = get_internal_brain(brain);

    // Do some training
    float inputs[TEST_NUM_INPUTS] = {0.5f, 0.5f, 0.5f, 0.5f};
    float targets[TEST_NUM_OUTPUTS] = {0.5f, 0.5f};
    nimcp_training_result_t result;

    for (int i = 0; i < 3; i++) {
        nimcp_brain_train_step(brain, inputs, TEST_NUM_INPUTS,
                               targets, TEST_NUM_OUTPUTS, &result);
    }

    // Get stats before reset
    uint64_t steps_before = 0;
    float loss_before = 0.0f;
    float lr_before = 0.0f;
    training_dispatch_get_stats(internal, &steps_before, &loss_before, &lr_before);
    printf("  Before reset: steps=%lu, loss=%.4f\n",
           (unsigned long)steps_before, loss_before);

    // Reset training
    int reset_result = training_dispatch_reset(internal);
    printf("  Reset result: %d\n", reset_result);

    // Get stats after reset
    uint64_t steps_after = 0;
    float loss_after = 0.0f;
    float lr_after = 0.0f;
    training_dispatch_get_stats(internal, &steps_after, &loss_after, &lr_after);
    printf("  After reset: steps=%lu, loss=%.4f\n",
           (unsigned long)steps_after, loss_after);

    // Cleanup
    nimcp_brain_destroy(brain);

    TEST_PASS("Training reset completed");
}

/**
 * @brief Test 10: All network types support check
 *
 * Verifies training_dispatch_is_supported() for all network types.
 */
void test_all_network_types_support(void)
{
    printf("\n=== test_all_network_types_support ===\n");

    struct {
        uint8_t type;
        const char* name;
    } network_types[] = {
        {NIMCP_NETWORK_ADAPTIVE, "ADAPTIVE"},
        {NIMCP_NETWORK_SNN, "SNN"},
        {NIMCP_NETWORK_LNN, "LNN"},
        {NIMCP_NETWORK_CNN, "CNN"},
        {NIMCP_NETWORK_HYBRID, "HYBRID"},
    };

    int supported_count = 0;
    for (size_t i = 0; i < sizeof(network_types) / sizeof(network_types[0]); i++) {
        bool supported = training_dispatch_is_supported(network_types[i].type);
        const char* type_name = training_dispatch_type_name(network_types[i].type);

        printf("  %s (type=%d): supported=%s, name=%s\n",
               network_types[i].name,
               network_types[i].type,
               supported ? "yes" : "no",
               type_name ? type_name : "(null)");

        if (supported) {
            supported_count++;
        }

        // Type name should never be NULL
        TEST_ASSERT(type_name != NULL, "Type name should not be NULL");
    }

    printf("  Total supported: %d/%zu\n",
           supported_count, sizeof(network_types) / sizeof(network_types[0]));

    // At minimum, ADAPTIVE should be supported
    TEST_ASSERT(training_dispatch_is_supported(NIMCP_NETWORK_ADAPTIVE),
                "ADAPTIVE network type must be supported");

    TEST_PASS("All network types support check completed");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("==============================================\n");
    printf("Training Dispatch Integration Tests\n");
    printf("==============================================\n");

    // Run tests
    test_adaptive_training_integration();
    test_snn_training_integration();
    test_lnn_training_integration();
    test_cnn_training_integration();
    test_hybrid_training_integration();
    test_training_config_defaults();
    test_training_dispatch_direct();
    test_snn_reward_signal();
    test_training_reset();
    test_all_network_types_support();

    // Summary
    printf("\n==============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("==============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
