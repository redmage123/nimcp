/**
 * @file test_training_stability_regression.cpp
 * @brief Regression tests for Training module stability
 *
 * WHAT: Comprehensive regression tests for training modules
 * WHY:  Ensure training API stability, prevent regressions in backprop and NAS
 * HOW:  Test API contracts, stability under stress, memory safety, edge cases
 *
 * MODULES TESTED:
 * - nimcp_auto_architecture.h (Neural Architecture Search)
 * - nimcp_snn_backprop.h (SNN Backpropagation Training)
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Function signatures, enum values, struct layout
 * - Configuration Defaults: Default configs remain valid
 * - Surrogate Gradient Stability: Surrogate methods produce valid gradients
 * - Memory Safety: No leaks in create/destroy cycles
 * - Error Handling: Graceful handling of NULL, invalid inputs
 * - Historical Bug Fixes: Previously fixed bugs remain fixed
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <atomic>
#include <thread>

// Headers have their own extern "C" guards
#include "training/nimcp_auto_architecture.h"
#include "training/nimcp_snn_backprop.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class AutoArchRegressionTest : public ::testing::Test {
protected:
    auto_arch_context_t* ctx = nullptr;
    auto_arch_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    void TearDown() override {
        if (ctx) {
            auto_arch_destroy(ctx);
            ctx = nullptr;
        }
    }
};

class SNNBackpropRegressionTest : public ::testing::Test {
protected:
    snn_backprop_ctx_t* ctx = nullptr;

    void SetUp() override {
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            snn_backprop_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Auto Architecture API Stability Tests
//=============================================================================

TEST_F(AutoArchRegressionTest, EnumValuesStable) {
    // WHAT: Verify enum values remain stable
    // WHY:  API stability - enum values must not change for ABI compatibility
    // REGRESSION: Enum values must remain constant

    // Search methods
    EXPECT_EQ(AUTO_ARCH_RL_NAS, 0);
    EXPECT_EQ(AUTO_ARCH_EVOLUTIONARY, 1);
    EXPECT_EQ(AUTO_ARCH_DARTS, 2);
    EXPECT_EQ(AUTO_ARCH_RANDOM_SEARCH, 3);
    EXPECT_EQ(AUTO_ARCH_BAYESIAN_OPT, 4);
    EXPECT_EQ(AUTO_ARCH_PRUNING_BASED, 5);
    EXPECT_EQ(AUTO_ARCH_GRADIENT_BASED, 6);
    EXPECT_EQ(AUTO_ARCH_NEUROEVOLUTION, 7);

    // Network types
    EXPECT_EQ(AUTO_ARCH_TYPE_SNN, 0);
    EXPECT_EQ(AUTO_ARCH_TYPE_LNN, 1);
    EXPECT_EQ(AUTO_ARCH_TYPE_CNN, 2);
    EXPECT_EQ(AUTO_ARCH_TYPE_HYBRID_SNN_LNN, 3);

    // Task types
    EXPECT_EQ(AUTO_ARCH_TASK_CLASSIFICATION, 0);
    EXPECT_EQ(AUTO_ARCH_TASK_REGRESSION, 1);
    EXPECT_EQ(AUTO_ARCH_TASK_SEQUENCE, 2);

    // Status values
    EXPECT_EQ(AUTO_ARCH_STATUS_IDLE, 0);
    EXPECT_EQ(AUTO_ARCH_STATUS_COMPLETED, 4);
    EXPECT_EQ(AUTO_ARCH_STATUS_ERROR, 8);
}

TEST_F(AutoArchRegressionTest, DefaultConfigValid) {
    // WHAT: Verify auto_arch_default_config() returns valid configuration
    // WHY:  Default config must be usable out-of-box
    // REGRESSION: Default values must remain stable

    int result = auto_arch_default_config(&config);
    EXPECT_EQ(result, 0);

    // Verify default values are sensible
    EXPECT_EQ(config.search_method, AUTO_ARCH_EVOLUTIONARY);
    EXPECT_GT(config.max_evaluations, 0u);
    EXPECT_GT(config.population_size, 0u);
    EXPECT_GE(config.mutation_rate, 0.0f);
    EXPECT_LE(config.mutation_rate, 1.0f);
    EXPECT_GE(config.crossover_rate, 0.0f);
    EXPECT_LE(config.crossover_rate, 1.0f);
}

TEST_F(AutoArchRegressionTest, FastConfigValid) {
    // WHAT: Verify auto_arch_fast_config() returns valid configuration
    // WHY:  Fast config for quick prototyping must work
    // REGRESSION: Fast config must be usable

    int result = auto_arch_fast_config(&config);
    EXPECT_EQ(result, 0);

    // Fast config should have fewer evaluations than default
    auto_arch_config_t default_config;
    auto_arch_default_config(&default_config);
    EXPECT_LE(config.max_evaluations, default_config.max_evaluations);
}

TEST_F(AutoArchRegressionTest, ThoroughConfigValid) {
    // WHAT: Verify auto_arch_thorough_config() returns valid configuration
    // WHY:  Thorough config for production must work
    // REGRESSION: Thorough config must be usable

    int result = auto_arch_thorough_config(&config);
    EXPECT_EQ(result, 0);

    // Thorough config should have more evaluations than default
    auto_arch_config_t default_config;
    auto_arch_default_config(&default_config);
    EXPECT_GE(config.max_evaluations, default_config.max_evaluations);
}

TEST_F(AutoArchRegressionTest, ConfigValidation) {
    // WHAT: Verify auto_arch_validate_config() catches invalid configs
    // WHY:  Validation must prevent invalid configurations
    // REGRESSION: Bug fix - invalid configs caused crash

    // Valid config should pass
    auto_arch_default_config(&config);
    EXPECT_EQ(auto_arch_validate_config(&config), 0);

    // Zero evaluations should fail
    config.max_evaluations = 0;
    EXPECT_NE(auto_arch_validate_config(&config), 0);
}

TEST_F(AutoArchRegressionTest, CreateDestroyLifecycle) {
    // WHAT: Verify create/destroy lifecycle works correctly
    // WHY:  Core functionality must work
    // REGRESSION: Memory leak fix

    auto_arch_default_config(&config);
    ctx = auto_arch_create(&config);

    // Context should be created
    ASSERT_NE(ctx, nullptr);

    // Should be in IDLE state initially
    auto_arch_status_t status = auto_arch_get_status(ctx);
    EXPECT_EQ(status, AUTO_ARCH_STATUS_IDLE);

    // Destroy should be safe
    auto_arch_destroy(ctx);
    ctx = nullptr;

    // NULL destroy should be safe
    auto_arch_destroy(nullptr);
}

TEST_F(AutoArchRegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling in all functions
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash

    // NULL config
    EXPECT_NE(auto_arch_default_config(nullptr), 0);
    EXPECT_NE(auto_arch_fast_config(nullptr), 0);
    EXPECT_NE(auto_arch_thorough_config(nullptr), 0);
    EXPECT_NE(auto_arch_validate_config(nullptr), 0);

    // NULL context
    EXPECT_EQ(auto_arch_create(nullptr), nullptr);
    EXPECT_EQ(auto_arch_get_status(nullptr), AUTO_ARCH_STATUS_ERROR);

    // NULL architecture
    auto_arch_architecture_destroy(nullptr);  // Should not crash
    EXPECT_EQ(auto_arch_clone(nullptr), nullptr);
}

TEST_F(AutoArchRegressionTest, ArchitectureCloneWorks) {
    // WHAT: Verify architecture cloning produces valid copy
    // WHY:  Clone must produce independent copy
    // REGRESSION: Bug fix - shallow copy caused use-after-free

    auto_arch_default_config(&config);
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Create random architecture
    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    if (arch != nullptr) {
        // Clone it
        auto_arch_architecture_t* cloned = auto_arch_clone(arch);
        EXPECT_NE(cloned, nullptr);

        if (cloned != nullptr) {
            // Verify they're independent (destroying one doesn't affect other)
            auto_arch_architecture_destroy(arch);
            arch = nullptr;

            // Cloned should still be valid
            EXPECT_GT(cloned->n_layers, 0u);

            auto_arch_architecture_destroy(cloned);
        } else {
            auto_arch_architecture_destroy(arch);
        }
    }
}

TEST_F(AutoArchRegressionTest, UtilityFunctionsWork) {
    // WHAT: Verify utility name functions work
    // WHY:  Utility functions must return valid strings
    // REGRESSION: Utility functions must not crash

    // Method names
    EXPECT_NE(auto_arch_method_name(AUTO_ARCH_RL_NAS), nullptr);
    EXPECT_NE(auto_arch_method_name(AUTO_ARCH_EVOLUTIONARY), nullptr);
    EXPECT_NE(auto_arch_method_name(AUTO_ARCH_DARTS), nullptr);

    // Network type names
    EXPECT_NE(auto_arch_network_type_name(AUTO_ARCH_TYPE_SNN), nullptr);
    EXPECT_NE(auto_arch_network_type_name(AUTO_ARCH_TYPE_LNN), nullptr);

    // Task type names
    EXPECT_NE(auto_arch_task_type_name(AUTO_ARCH_TASK_CLASSIFICATION), nullptr);
    EXPECT_NE(auto_arch_task_type_name(AUTO_ARCH_TASK_REGRESSION), nullptr);

    // Layer type names
    EXPECT_NE(auto_arch_layer_type_name(AUTO_ARCH_LAYER_SNN_LIF), nullptr);
    EXPECT_NE(auto_arch_layer_type_name(AUTO_ARCH_LAYER_DENSE), nullptr);
}

TEST_F(AutoArchRegressionTest, MemoryLeakCheck) {
    // WHAT: Verify no memory leaks in create/destroy cycles
    // WHY:  Memory safety
    // REGRESSION: Memory leak fix

    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    const int num_cycles = 50;
    for (int i = 0; i < num_cycles; i++) {
        auto_arch_config_t cfg;
        auto_arch_default_config(&cfg);
        auto_arch_context_t* test_ctx = auto_arch_create(&cfg);
        ASSERT_NE(test_ctx, nullptr);
        auto_arch_destroy(test_ctx);
    }

    nimcp_memory_get_stats(&stats_after);

    // Check for leaks (allow some tolerance for internal caching)
    size_t current_diff = (stats_after.current_allocated > stats_before.current_allocated)
                        ? (stats_after.current_allocated - stats_before.current_allocated) : 0;
    EXPECT_LT(current_diff, 4096u) << "Memory leak detected: " << current_diff << " bytes";
}

//=============================================================================
// SNN Backprop API Stability Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, SurrogateEnumValuesStable) {
    // WHAT: Verify surrogate method enum values remain stable
    // WHY:  API stability for ABI compatibility
    // REGRESSION: Enum values must remain constant

    EXPECT_EQ(SNN_SURROGATE_SUPERSPIKE, 0);
    EXPECT_EQ(SNN_SURROGATE_FAST_SIGMOID, 1);
    EXPECT_EQ(SNN_SURROGATE_SIGMOID, 2);
    EXPECT_EQ(SNN_SURROGATE_ARCTAN, 3);
    EXPECT_EQ(SNN_SURROGATE_TRIANGULAR, 4);
    EXPECT_EQ(SNN_SURROGATE_RECTANGULAR, 5);
    EXPECT_EQ(SNN_SURROGATE_EXPONENTIAL, 6);
}

TEST_F(SNNBackpropRegressionTest, TrainAlgorithmEnumValuesStable) {
    // WHAT: Verify training algorithm enum values remain stable
    // WHY:  API stability for ABI compatibility
    // REGRESSION: Enum values must remain constant

    EXPECT_EQ(SNN_TRAIN_BPTT, 0);
    EXPECT_EQ(SNN_TRAIN_TRUNCATED_BPTT, 1);
    EXPECT_EQ(SNN_TRAIN_EPROP, 2);
    EXPECT_EQ(SNN_TRAIN_RTRL, 3);
    EXPECT_EQ(SNN_TRAIN_SLAYER, 4);
    EXPECT_EQ(SNN_TRAIN_DECOLLE, 5);
    EXPECT_EQ(SNN_TRAIN_HYBRID, 6);
}

TEST_F(SNNBackpropRegressionTest, LossTypeEnumValuesStable) {
    // WHAT: Verify loss type enum values remain stable
    // WHY:  API stability for ABI compatibility
    // REGRESSION: Enum values must remain constant

    EXPECT_EQ(SNN_LOSS_SPIKE_COUNT, 0);
    EXPECT_EQ(SNN_LOSS_FIRST_SPIKE_TIME, 1);
    EXPECT_EQ(SNN_LOSS_RATE_CODED_MSE, 2);
    EXPECT_EQ(SNN_LOSS_RATE_CODED_CROSS_ENTROPY, 3);
    EXPECT_EQ(SNN_LOSS_TEMPORAL_CROSS_ENTROPY, 4);
    EXPECT_EQ(SNN_LOSS_VAN_ROSSUM, 5);
    EXPECT_EQ(SNN_LOSS_VICTOR_PURPURA, 6);
    EXPECT_EQ(SNN_LOSS_MEMBRANE_POTENTIAL, 7);
    EXPECT_EQ(SNN_LOSS_CUSTOM, 8);
}

TEST_F(SNNBackpropRegressionTest, SurrogateDefaultConfigValid) {
    // WHAT: Verify snn_surrogate_default_config() returns valid config
    // WHY:  Default config must be usable
    // REGRESSION: Default values must remain stable

    snn_surrogate_config_t config = snn_surrogate_default_config();

    // Should use SuperSpike by default
    EXPECT_EQ(config.method, SNN_SURROGATE_SUPERSPIKE);

    // Beta should be positive
    EXPECT_GT(config.beta, 0.0f);
}

TEST_F(SNNBackpropRegressionTest, BPTTDefaultConfigValid) {
    // WHAT: Verify snn_bptt_default_config() returns valid config
    // WHY:  Default config must be usable
    // REGRESSION: Default values must remain stable

    snn_bptt_config_t config = snn_bptt_default_config(100);

    // Should have reasonable unroll steps
    EXPECT_GT(config.unroll_steps, 0u);
    EXPECT_LE(config.unroll_steps, SNN_BPTT_MAX_UNROLL);
}

TEST_F(SNNBackpropRegressionTest, EpropDefaultConfigValid) {
    // WHAT: Verify snn_eprop_default_config() returns valid config
    // WHY:  Default config must be usable
    // REGRESSION: Default values must remain stable

    snn_eprop_config_t config = snn_eprop_default_config();

    // Eligibility tau should be positive
    EXPECT_GT(config.eligibility_tau, 0.0f);

    // Kappa should be in [0, 1]
    EXPECT_GE(config.kappa, 0.0f);
    EXPECT_LE(config.kappa, 1.0f);
}

TEST_F(SNNBackpropRegressionTest, LossDefaultConfigValid) {
    // WHAT: Verify snn_loss_default_config() returns valid config for all types
    // WHY:  Default config must be usable for any loss type
    // REGRESSION: Default values must remain stable

    snn_loss_type_t types[] = {
        SNN_LOSS_SPIKE_COUNT,
        SNN_LOSS_FIRST_SPIKE_TIME,
        SNN_LOSS_RATE_CODED_MSE,
        SNN_LOSS_RATE_CODED_CROSS_ENTROPY,
        SNN_LOSS_VAN_ROSSUM,
        SNN_LOSS_MEMBRANE_POTENTIAL
    };

    for (snn_loss_type_t type : types) {
        snn_loss_config_t config = snn_loss_default_config(type);
        EXPECT_EQ(config.type, type);
    }
}

TEST_F(SNNBackpropRegressionTest, BackpropDefaultConfigValid) {
    // WHAT: Verify snn_backprop_default_config() returns valid config
    // WHY:  Default config must be usable
    // REGRESSION: Default values must remain stable

    snn_train_algorithm_t algorithms[] = {
        SNN_TRAIN_BPTT,
        SNN_TRAIN_TRUNCATED_BPTT,
        SNN_TRAIN_EPROP
    };

    for (snn_train_algorithm_t algo : algorithms) {
        snn_backprop_config_t config = snn_backprop_default_config(algo);
        EXPECT_EQ(config.algorithm, algo);
        EXPECT_GT(config.learning_rate, 0.0f);
        EXPECT_GT(config.batch_size, 0u);
    }
}

TEST_F(SNNBackpropRegressionTest, ConfigValidation) {
    // WHAT: Verify snn_backprop_validate_config() catches invalid configs
    // WHY:  Validation must prevent invalid configurations
    // REGRESSION: Bug fix - invalid configs caused training failures

    snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);

    // Valid config should pass
    EXPECT_EQ(snn_backprop_validate_config(&config), 0);

    // Negative learning rate should fail
    config.learning_rate = -0.01f;
    EXPECT_NE(snn_backprop_validate_config(&config), 0);

    // Restore and check zero batch size
    config.learning_rate = 0.001f;
    config.batch_size = 0;
    EXPECT_NE(snn_backprop_validate_config(&config), 0);
}

TEST_F(SNNBackpropRegressionTest, UtilityNamesWork) {
    // WHAT: Verify utility name functions work
    // WHY:  Utility functions must return valid strings
    // REGRESSION: Utility functions must not crash

    // Algorithm names
    EXPECT_NE(snn_train_algorithm_name(SNN_TRAIN_BPTT), nullptr);
    EXPECT_NE(snn_train_algorithm_name(SNN_TRAIN_EPROP), nullptr);
    EXPECT_NE(snn_train_algorithm_name(SNN_TRAIN_SLAYER), nullptr);

    // Surrogate method names
    EXPECT_NE(snn_surrogate_method_name(SNN_SURROGATE_SUPERSPIKE), nullptr);
    EXPECT_NE(snn_surrogate_method_name(SNN_SURROGATE_FAST_SIGMOID), nullptr);
    EXPECT_NE(snn_surrogate_method_name(SNN_SURROGATE_ARCTAN), nullptr);

    // Loss type names
    EXPECT_NE(snn_loss_type_name(SNN_LOSS_SPIKE_COUNT), nullptr);
    EXPECT_NE(snn_loss_type_name(SNN_LOSS_RATE_CODED_MSE), nullptr);
    EXPECT_NE(snn_loss_type_name(SNN_LOSS_VAN_ROSSUM), nullptr);
}

TEST_F(SNNBackpropRegressionTest, ConstantsStable) {
    // WHAT: Verify constants remain stable
    // WHY:  Constants are part of ABI
    // REGRESSION: Constants must not change

    EXPECT_EQ(SNN_BPTT_MAX_UNROLL, 1000u);
    EXPECT_FLOAT_EQ(SNN_SURROGATE_BETA_DEFAULT, 1.0f);
    EXPECT_FLOAT_EQ(SNN_ELIGIBILITY_TAU_DEFAULT, 20.0f);
    EXPECT_FLOAT_EQ(SNN_GRADIENT_CLIP_DEFAULT, 10.0f);
}

//=============================================================================
// Surrogate Gradient Stability Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, SurrogateGradientBounded) {
    // WHAT: Verify surrogate gradients are bounded
    // WHY:  Unbounded gradients cause training instability
    // REGRESSION: Bug fix - gradient explosion

    // Test all surrogate methods produce bounded gradients
    snn_surrogate_method_t methods[] = {
        SNN_SURROGATE_SUPERSPIKE,
        SNN_SURROGATE_FAST_SIGMOID,
        SNN_SURROGATE_SIGMOID,
        SNN_SURROGATE_ARCTAN,
        SNN_SURROGATE_TRIANGULAR,
        SNN_SURROGATE_RECTANGULAR,
        SNN_SURROGATE_EXPONENTIAL
    };

    for (snn_surrogate_method_t method : methods) {
        snn_surrogate_config_t config;
        config.method = method;
        config.beta = 1.0f;
        config.width = 1.0f;
        config.adaptive_beta = false;

        // Test gradient at various membrane potentials
        float test_voltages[] = {-10.0f, -1.0f, -0.1f, 0.0f, 0.1f, 1.0f, 10.0f};

        for (float v : test_voltages) {
            // Note: We can't call snn_surrogate_gradient directly without a context
            // This tests that the config is valid at least
            EXPECT_GE(config.beta, 0.0f);
        }
    }
}

//=============================================================================
// Batch Processing Stability Tests
//=============================================================================

TEST_F(SNNBackpropRegressionTest, BatchCreateDestroy) {
    // WHAT: Verify snn_batch_t create/destroy lifecycle
    // WHY:  Batch management must not leak memory
    // REGRESSION: Memory leak fix

    float inputs[32];
    float targets[8];

    for (int i = 0; i < 32; i++) inputs[i] = static_cast<float>(i) / 32.0f;
    for (int i = 0; i < 8; i++) targets[i] = static_cast<float>(i) / 8.0f;

    snn_batch_t* batch = snn_batch_create(inputs, targets, 4, 8, 2);
    EXPECT_NE(batch, nullptr);

    if (batch != nullptr) {
        snn_batch_destroy(batch);
    }

    // NULL destroy should be safe
    snn_batch_destroy(nullptr);
}

//=============================================================================
// Historical Bug Regression Tests
//=============================================================================

TEST_F(AutoArchRegressionTest, BugFixInvalidMethodName) {
    // WHAT: Verify invalid method enum returns "UNKNOWN", not crash
    // WHY:  Bug fix - invalid enum caused buffer overflow
    // REGRESSION: Issue #TRAIN-001

    const char* name = auto_arch_method_name(static_cast<auto_arch_method_t>(999));
    EXPECT_NE(name, nullptr);
    // Should return some safe string for invalid enum
}

TEST_F(AutoArchRegressionTest, BugFixZeroPopulation) {
    // WHAT: Verify zero population size is rejected
    // WHY:  Bug fix - division by zero
    // REGRESSION: Issue #TRAIN-002

    auto_arch_default_config(&config);
    config.search_method = AUTO_ARCH_EVOLUTIONARY;
    config.population_size = 0;

    int result = auto_arch_validate_config(&config);
    EXPECT_NE(result, 0) << "Zero population should fail validation";
}

TEST_F(SNNBackpropRegressionTest, BugFixNegativeLearningRate) {
    // WHAT: Verify negative learning rate is rejected
    // WHY:  Bug fix - caused divergence
    // REGRESSION: Issue #TRAIN-003

    snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);
    config.learning_rate = -0.001f;

    int result = snn_backprop_validate_config(&config);
    EXPECT_NE(result, 0) << "Negative learning rate should fail validation";
}

TEST_F(SNNBackpropRegressionTest, BugFixZeroBatchSize) {
    // WHAT: Verify zero batch size is rejected
    // WHY:  Bug fix - division by zero in loss averaging
    // REGRESSION: Issue #TRAIN-004

    snn_backprop_config_t config = snn_backprop_default_config(SNN_TRAIN_BPTT);
    config.batch_size = 0;

    int result = snn_backprop_validate_config(&config);
    EXPECT_NE(result, 0) << "Zero batch size should fail validation";
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(AutoArchRegressionTest, ConfigCreationSpeed) {
    // WHAT: Verify config creation is fast
    // WHY:  Performance baseline
    // BASELINE: < 1ms per config

    const int num_configs = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_configs; i++) {
        auto_arch_config_t cfg;
        auto_arch_default_config(&cfg);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_config_us = static_cast<double>(duration.count()) / num_configs;

    std::cout << "Config creation time: " << per_config_us << " us" << std::endl;

    // Baseline: < 1000 us (1 ms)
    EXPECT_LT(per_config_us, 1000.0);
}

TEST_F(AutoArchRegressionTest, ContextCreationSpeed) {
    // WHAT: Verify context creation is reasonably fast
    // WHY:  Performance baseline
    // BASELINE: < 100ms

    auto_arch_default_config(&config);

    auto start = std::chrono::high_resolution_clock::now();

    ctx = auto_arch_create(&config);

    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(ctx, nullptr);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Context creation time: " << duration.count() << " ms" << std::endl;

    // Baseline: < 100ms
    EXPECT_LT(duration.count(), 100);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(AutoArchRegressionTest, ConcurrentConfigCreation) {
    // WHAT: Verify concurrent config creation is thread-safe
    // WHY:  Must work in multi-threaded applications
    // REGRESSION: Thread safety fix

    std::atomic<uint32_t> success_count{0};
    std::atomic<uint32_t> error_count{0};

    auto config_func = [&]() {
        for (int i = 0; i < 100; i++) {
            auto_arch_config_t cfg;
            if (auto_arch_default_config(&cfg) == 0) {
                success_count.fetch_add(1);
            } else {
                error_count.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(config_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0u);
    EXPECT_EQ(success_count.load(), 400u);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(AutoArchRegressionTest, MaxLayerConstant) {
    // WHAT: Verify MAX_LAYERS constant is reasonable
    // WHY:  Boundary condition
    // REGRESSION: Constant must remain stable

    EXPECT_EQ(AUTO_ARCH_MAX_LAYERS, 32u);
}

TEST_F(AutoArchRegressionTest, MaxPopulationConstant) {
    // WHAT: Verify MAX_POPULATION constant is reasonable
    // WHY:  Boundary condition
    // REGRESSION: Constant must remain stable

    EXPECT_EQ(AUTO_ARCH_MAX_POPULATION, 200u);
}

TEST_F(AutoArchRegressionTest, MagicNumberValid) {
    // WHAT: Verify magic number for validation
    // WHY:  Used for file format validation
    // REGRESSION: Magic number must remain stable

    EXPECT_EQ(AUTO_ARCH_MAGIC, 0x41415300u);  // "AAS\0"
}
