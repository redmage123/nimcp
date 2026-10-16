//=============================================================================
// test_auto_architecture.cpp - Unit Tests for Auto Architecture Search
//=============================================================================
/**
 * @file test_auto_architecture.cpp
 * @brief Comprehensive unit tests for Neural Architecture Search module
 *
 * WHAT: Tests for all auto_architecture.c functionality
 * WHY:  Ensure correctness of architecture search algorithms
 * HOW:  GTest framework with fixture setup for common test state
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "training/nimcp_auto_architecture.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AutoArchitectureTest : public ::testing::Test {
protected:
    auto_arch_config_t config;
    auto_arch_context_t* ctx;
    nimcp_tensor_t* train_data;
    nimcp_tensor_t* train_labels;
    nimcp_tensor_t* val_data;
    nimcp_tensor_t* val_labels;

    void SetUp() override {
        ctx = nullptr;
        train_data = nullptr;
        train_labels = nullptr;
        val_data = nullptr;
        val_labels = nullptr;

        // Initialize with defaults
        ASSERT_EQ(auto_arch_default_config(&config), 0);
    }

    void TearDown() override {
        if (ctx) {
            auto_arch_destroy(ctx);
            ctx = nullptr;
        }
        if (train_data) {
            nimcp_tensor_destroy(train_data);
            train_data = nullptr;
        }
        if (train_labels) {
            nimcp_tensor_destroy(train_labels);
            train_labels = nullptr;
        }
        if (val_data) {
            nimcp_tensor_destroy(val_data);
            val_data = nullptr;
        }
        if (val_labels) {
            nimcp_tensor_destroy(val_labels);
            val_labels = nullptr;
        }
    }

    void CreateSampleData(uint32_t n_samples, uint32_t n_features, uint32_t n_classes) {
        /* P6-11: Seed RNG for reproducibility */
        srand(42);

        // Create training data
        uint32_t data_dims[2] = {n_samples, n_features};
        train_data = nimcp_tensor_create(data_dims, 2, NIMCP_DTYPE_F32);
        ASSERT_NE(train_data, nullptr);

        // Initialize with random data
        float* data_ptr = (float*)nimcp_tensor_data(train_data);
        for (uint32_t i = 0; i < n_samples * n_features; i++) {
            data_ptr[i] = (float)rand() / RAND_MAX;
        }

        // Create labels (one-hot encoded)
        uint32_t label_dims[2] = {n_samples, n_classes};
        train_labels = nimcp_tensor_create(label_dims, 2, NIMCP_DTYPE_F32);
        ASSERT_NE(train_labels, nullptr);

        // Initialize labels
        float* label_ptr = (float*)nimcp_tensor_data(train_labels);
        memset(label_ptr, 0, n_samples * n_classes * sizeof(float));
        for (uint32_t i = 0; i < n_samples; i++) {
            uint32_t class_idx = i % n_classes;
            label_ptr[i * n_classes + class_idx] = 1.0f;
        }
    }

    void ConfigureMinimalSearch() {
        config.search_method = AUTO_ARCH_RANDOM_SEARCH;
        config.max_evaluations = 5;
        config.max_iterations = 10;
        config.population_size = 5;
        config.eval_epochs = 1;
        config.random_seed = 42; // Reproducibility
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(AutoArchitectureTest, DefaultConfigInitialization) {
    auto_arch_config_t cfg;
    int result = auto_arch_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(cfg.search_method, AUTO_ARCH_EVOLUTIONARY);
    EXPECT_GT(cfg.max_evaluations, 0u);
    EXPECT_GT(cfg.population_size, 0u);
    EXPECT_GT(cfg.eval_epochs, 0u);
}

TEST_F(AutoArchitectureTest, FastConfigInitialization) {
    auto_arch_config_t cfg;
    int result = auto_arch_fast_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(cfg.search_method, AUTO_ARCH_RANDOM_SEARCH);
    EXPECT_LE(cfg.max_evaluations, 100u);
    EXPECT_LE(cfg.eval_epochs, 10u);
}

TEST_F(AutoArchitectureTest, ThoroughConfigInitialization) {
    auto_arch_config_t cfg;
    int result = auto_arch_thorough_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GE(cfg.max_evaluations, 1000u);
    EXPECT_GE(cfg.eval_epochs, 20u);
}

TEST_F(AutoArchitectureTest, ConfigValidation) {
    // Valid config
    EXPECT_EQ(auto_arch_validate_config(&config), 0);

    // Invalid: zero evaluations
    auto_arch_config_t invalid_cfg = config;
    invalid_cfg.max_evaluations = 0;
    EXPECT_NE(auto_arch_validate_config(&invalid_cfg), 0);

    // Invalid: evolutionary with zero population
    invalid_cfg = config;
    invalid_cfg.search_method = AUTO_ARCH_EVOLUTIONARY;
    invalid_cfg.population_size = 0;
    EXPECT_NE(auto_arch_validate_config(&invalid_cfg), 0);
}

TEST_F(AutoArchitectureTest, NullConfigHandling) {
    EXPECT_NE(auto_arch_default_config(nullptr), 0);
    EXPECT_NE(auto_arch_fast_config(nullptr), 0);
    EXPECT_NE(auto_arch_thorough_config(nullptr), 0);
    EXPECT_NE(auto_arch_validate_config(nullptr), 0);
}

//=============================================================================
// Context Creation/Destruction Tests
//=============================================================================

TEST_F(AutoArchitectureTest, ContextCreationRandom) {
    config.search_method = AUTO_ARCH_RANDOM_SEARCH;
    config.population_size = 10;
    config.max_evaluations = 10;

    ctx = auto_arch_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(AutoArchitectureTest, ContextCreationEvolutionary) {
    config.search_method = AUTO_ARCH_EVOLUTIONARY;
    config.population_size = 20;
    config.max_evaluations = 50;

    ctx = auto_arch_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(AutoArchitectureTest, ContextCreationRLNAS) {
    config.search_method = AUTO_ARCH_RL_NAS;
    config.population_size = 10;
    config.max_evaluations = 10;

    ctx = auto_arch_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(AutoArchitectureTest, ContextCreationDARTS) {
    config.search_method = AUTO_ARCH_DARTS;
    config.population_size = 10;
    config.max_evaluations = 10;

    ctx = auto_arch_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(AutoArchitectureTest, ContextCreationPruning) {
    config.search_method = AUTO_ARCH_PRUNING_BASED;
    config.population_size = 10;
    config.max_evaluations = 10;
    config.target_sparsity = 0.8f;

    ctx = auto_arch_create(&config);
    EXPECT_NE(ctx, nullptr);
}

TEST_F(AutoArchitectureTest, ContextDestroyNull) {
    // Should not crash
    auto_arch_destroy(nullptr);
}

TEST_F(AutoArchitectureTest, NullConfigCreation) {
    ctx = auto_arch_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

//=============================================================================
// Task Setting Tests
//=============================================================================

TEST_F(AutoArchitectureTest, SetTask) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 784;
    task.n_outputs = 10;
    task.target_accuracy = 0.9f;
    task.max_latency_ms = 10.0f;

    int result = auto_arch_set_task(ctx, &task);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Architecture Manipulation Tests
//=============================================================================

TEST_F(AutoArchitectureTest, RandomArchitectureGeneration) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Set task first
    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 784;
    task.n_outputs = 10;
    auto_arch_set_task(ctx, &task);

    // Generate random architecture
    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    // Verify basic properties
    EXPECT_GT(arch->n_layers, 0u);
    EXPECT_EQ(arch->n_inputs, 784u);
    EXPECT_EQ(arch->n_outputs, 10u);
    EXPECT_GT(arch->n_parameters, 0u);
    EXPECT_EQ(arch->magic, AUTO_ARCH_MAGIC);

    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, ArchitectureClone) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* original = auto_arch_random_architecture(ctx);
    ASSERT_NE(original, nullptr);

    auto_arch_architecture_t* clone = auto_arch_clone(original);
    ASSERT_NE(clone, nullptr);

    // Verify clone matches original
    EXPECT_EQ(clone->n_layers, original->n_layers);
    EXPECT_EQ(clone->n_inputs, original->n_inputs);
    EXPECT_EQ(clone->n_outputs, original->n_outputs);
    EXPECT_EQ(clone->n_parameters, original->n_parameters);
    EXPECT_EQ(clone->magic, original->magic);

    // Verify layers are copied
    for (uint32_t i = 0; i < original->n_layers; i++) {
        EXPECT_EQ(clone->layers[i].n_neurons, original->layers[i].n_neurons);
        EXPECT_EQ(clone->layers[i].type, original->layers[i].type);
    }

    // Verify independence (modify original, check clone unchanged)
    original->n_layers = 999;
    EXPECT_NE(clone->n_layers, original->n_layers);

    auto_arch_architecture_destroy(original);
    auto_arch_architecture_destroy(clone);
}

TEST_F(AutoArchitectureTest, ArchitectureCloneNull) {
    auto_arch_architecture_t* clone = auto_arch_clone(nullptr);
    EXPECT_EQ(clone, nullptr);
}

TEST_F(AutoArchitectureTest, ArchitectureMutation) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    // Store original values
    uint64_t orig_params = arch->n_parameters;

    // Apply mutation (high rate to ensure changes)
    int result = auto_arch_mutate(arch, 1.0f, ctx);
    EXPECT_EQ(result, 0);

    // Architecture should still be valid
    EXPECT_GT(arch->n_layers, 0u);
    EXPECT_EQ(arch->n_inputs, 100u);
    EXPECT_EQ(arch->n_outputs, 5u);

    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, ArchitectureCrossover) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* parent1 = auto_arch_random_architecture(ctx);
    auto_arch_architecture_t* parent2 = auto_arch_random_architecture(ctx);
    ASSERT_NE(parent1, nullptr);
    ASSERT_NE(parent2, nullptr);

    auto_arch_architecture_t* child = auto_arch_crossover(parent1, parent2, ctx);
    ASSERT_NE(child, nullptr);

    // Child should have valid structure
    EXPECT_GT(child->n_layers, 0u);
    EXPECT_EQ(child->n_inputs, 100u);
    EXPECT_EQ(child->n_outputs, 5u);
    EXPECT_EQ(child->magic, AUTO_ARCH_MAGIC);

    auto_arch_architecture_destroy(parent1);
    auto_arch_architecture_destroy(parent2);
    auto_arch_architecture_destroy(child);
}

TEST_F(AutoArchitectureTest, ArchitectureValidation) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    // Validation against default constraints should pass
    int result = auto_arch_validate_architecture(arch, &config.constraints);
    EXPECT_EQ(result, 0);

    auto_arch_architecture_destroy(arch);
}

//=============================================================================
// Evaluation Tests
//=============================================================================

TEST_F(AutoArchitectureTest, ArchitectureEvaluation) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    CreateSampleData(100, 100, 5);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    auto_arch_fitness_t fitness = {};
    int result = auto_arch_evaluate(ctx, arch, train_data, train_labels, nullptr, nullptr, &fitness);

    EXPECT_EQ(result, 0);
    EXPECT_GE(fitness.accuracy, 0.0f);
    EXPECT_LE(fitness.accuracy, 1.0f);
    EXPECT_GT(fitness.n_parameters, 0u);
    EXPECT_GE(fitness.bio_plausibility_score, 0.0f);
    EXPECT_LE(fitness.bio_plausibility_score, 1.0f);

    auto_arch_architecture_destroy(arch);
}

//=============================================================================
// Bio Score Tests
//=============================================================================

TEST_F(AutoArchitectureTest, BioPlausibilityScore) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    float score = auto_arch_compute_bio_score(arch);

    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, BioScoreNull) {
    float score = auto_arch_compute_bio_score(nullptr);
    EXPECT_EQ(score, 0.0f);
}

//=============================================================================
// Export/Import Tests
//=============================================================================

TEST_F(AutoArchitectureTest, ExportSNN) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    snn_config_t* snn_cfg = auto_arch_export_snn(arch);
    ASSERT_NE(snn_cfg, nullptr);

    EXPECT_EQ(snn_cfg->n_inputs, arch->n_inputs);
    EXPECT_EQ(snn_cfg->n_outputs, arch->n_outputs);
    EXPECT_EQ(snn_cfg->n_populations, arch->n_layers);

    nimcp_free(snn_cfg);
    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, ExportLNN) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    lnn_config_t* lnn_cfg = auto_arch_export_lnn(arch);
    ASSERT_NE(lnn_cfg, nullptr);

    EXPECT_EQ(lnn_cfg->n_inputs, arch->n_inputs);
    EXPECT_EQ(lnn_cfg->n_outputs, arch->n_outputs);
    EXPECT_EQ(lnn_cfg->n_layers, arch->n_layers);
    EXPECT_NE(lnn_cfg->layer_configs, nullptr);

    // Free layer configs
    if (lnn_cfg->layer_configs) {
        nimcp_free(lnn_cfg->layer_configs);
    }
    nimcp_free(lnn_cfg);
    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, ImportSNN) {
    snn_config_t snn_cfg = {};
    snn_cfg.n_inputs = 784;
    snn_cfg.n_outputs = 10;
    snn_cfg.n_populations = 3;
    snn_cfg.dt = 1.0f;
    snn_cfg.tau_mem = 20.0f;
    snn_cfg.tau_syn = 5.0f;
    snn_cfg.v_thresh = -55.0f;
    snn_cfg.v_reset = -70.0f;
    snn_cfg.t_ref = 2.0f;

    auto_arch_architecture_t* arch = auto_arch_import_snn(&snn_cfg);
    ASSERT_NE(arch, nullptr);

    EXPECT_EQ(arch->n_inputs, 784u);
    EXPECT_EQ(arch->n_outputs, 10u);
    EXPECT_EQ(arch->n_layers, 3u);
    EXPECT_EQ(arch->network_type, AUTO_ARCH_TYPE_SNN);

    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, ExportNull) {
    EXPECT_EQ(auto_arch_export_snn(nullptr), nullptr);
    EXPECT_EQ(auto_arch_export_lnn(nullptr), nullptr);
}

TEST_F(AutoArchitectureTest, ImportNull) {
    EXPECT_EQ(auto_arch_import_snn(nullptr), nullptr);
    EXPECT_EQ(auto_arch_import_lnn(nullptr), nullptr);
}

//=============================================================================
// JSON Serialization Tests
//=============================================================================

TEST_F(AutoArchitectureTest, SaveLoadJSON) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 100;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* original = auto_arch_random_architecture(ctx);
    ASSERT_NE(original, nullptr);

    const char* filepath = "/tmp/test_auto_arch.json";

    // Save to JSON
    int save_result = auto_arch_save_json(original, filepath);
    EXPECT_EQ(save_result, 0);

    // Load from JSON
    auto_arch_architecture_t* loaded = auto_arch_load_json(filepath);
    ASSERT_NE(loaded, nullptr);

    // Verify loaded matches original
    EXPECT_EQ(loaded->n_layers, original->n_layers);
    EXPECT_EQ(loaded->n_inputs, original->n_inputs);
    EXPECT_EQ(loaded->n_outputs, original->n_outputs);
    EXPECT_EQ(loaded->network_type, original->network_type);

    // Cleanup
    unlink(filepath);
    auto_arch_architecture_destroy(original);
    auto_arch_architecture_destroy(loaded);
}

TEST_F(AutoArchitectureTest, SaveJSONNull) {
    EXPECT_NE(auto_arch_save_json(nullptr, "/tmp/test.json"), 0);

    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.n_inputs = 10;
    task.n_outputs = 2;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    EXPECT_NE(auto_arch_save_json(arch, nullptr), 0);

    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, LoadJSONNull) {
    EXPECT_EQ(auto_arch_load_json(nullptr), nullptr);
}

TEST_F(AutoArchitectureTest, LoadJSONNonexistent) {
    auto_arch_architecture_t* arch = auto_arch_load_json("/nonexistent/path.json");
    EXPECT_EQ(arch, nullptr);
}

//=============================================================================
// Search Tests
//=============================================================================

TEST_F(AutoArchitectureTest, RandomSearch) {
    ConfigureMinimalSearch();
    config.search_method = AUTO_ARCH_RANDOM_SEARCH;

    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 50;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    CreateSampleData(50, 50, 5);

    auto_arch_result_t* result = auto_arch_search(ctx, train_data, train_labels, nullptr, nullptr);
    ASSERT_NE(result, nullptr);

    EXPECT_NE(result->best_arch, nullptr);
    EXPECT_GE(result->best_fitness.accuracy, 0.0f);
    EXPECT_GT(result->stats.total_evaluations, 0u);

    auto_arch_result_destroy(result);
}

TEST_F(AutoArchitectureTest, EvolutionarySearch) {
    ConfigureMinimalSearch();
    config.search_method = AUTO_ARCH_EVOLUTIONARY;
    config.population_size = 5;
    config.max_evaluations = 10;

    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.type = AUTO_ARCH_TASK_CLASSIFICATION;
    task.n_inputs = 50;
    task.n_outputs = 5;
    auto_arch_set_task(ctx, &task);

    CreateSampleData(50, 50, 5);

    auto_arch_result_t* result = auto_arch_search(ctx, train_data, train_labels, nullptr, nullptr);
    ASSERT_NE(result, nullptr);

    EXPECT_NE(result->best_arch, nullptr);

    auto_arch_result_destroy(result);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AutoArchitectureTest, GetStats) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_stats_t stats = {};
    int result = auto_arch_get_stats(ctx, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(AutoArchitectureTest, GetStatus) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_status_t status = auto_arch_get_status(ctx);
    EXPECT_EQ(status, AUTO_ARCH_STATUS_IDLE);
}

TEST_F(AutoArchitectureTest, GetBest) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.n_inputs = 10;
    task.n_outputs = 2;
    auto_arch_set_task(ctx, &task);

    CreateSampleData(10, 10, 2);

    auto_arch_result_t* result = auto_arch_search(ctx, train_data, train_labels, nullptr, nullptr);
    ASSERT_NE(result, nullptr);

    auto_arch_architecture_t* best = auto_arch_get_best(ctx);
    EXPECT_NE(best, nullptr);

    if (best) {
        auto_arch_architecture_destroy(best);
    }
    auto_arch_result_destroy(result);
}

//=============================================================================
// Result Handling Tests
//=============================================================================

TEST_F(AutoArchitectureTest, ResultDestroy) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.n_inputs = 10;
    task.n_outputs = 2;
    auto_arch_set_task(ctx, &task);

    CreateSampleData(10, 10, 2);

    auto_arch_result_t* result = auto_arch_search(ctx, train_data, train_labels, nullptr, nullptr);
    ASSERT_NE(result, nullptr);

    // Should not crash
    auto_arch_result_destroy(result);
}

TEST_F(AutoArchitectureTest, ResultDestroyNull) {
    // Should not crash
    auto_arch_result_destroy(nullptr);
}

TEST_F(AutoArchitectureTest, ResultSaveLoad) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.n_inputs = 10;
    task.n_outputs = 2;
    auto_arch_set_task(ctx, &task);

    CreateSampleData(10, 10, 2);

    auto_arch_result_t* result = auto_arch_search(ctx, train_data, train_labels, nullptr, nullptr);
    ASSERT_NE(result, nullptr);

    const char* filepath = "/tmp/test_auto_arch_result.json";

    // Save result
    int save_result = auto_arch_result_save(result, filepath);
    EXPECT_EQ(save_result, 0);

    // Load result
    auto_arch_result_t* loaded = auto_arch_result_load(filepath);
    ASSERT_NE(loaded, nullptr);

    // Verify loaded matches original
    EXPECT_FLOAT_EQ(loaded->best_fitness.accuracy, result->best_fitness.accuracy);
    EXPECT_EQ(loaded->stats.total_evaluations, result->stats.total_evaluations);

    // Cleanup
    unlink(filepath);
    unlink("/tmp/test_auto_arch_result.json.arch.json");
    auto_arch_result_destroy(result);
    auto_arch_result_destroy(loaded);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(AutoArchitectureTest, MethodName) {
    EXPECT_STREQ(auto_arch_method_name(AUTO_ARCH_RL_NAS), "RL-NAS");
    EXPECT_STREQ(auto_arch_method_name(AUTO_ARCH_EVOLUTIONARY), "Evolutionary");
    EXPECT_STREQ(auto_arch_method_name(AUTO_ARCH_DARTS), "DARTS");
    EXPECT_STREQ(auto_arch_method_name(AUTO_ARCH_RANDOM_SEARCH), "Random");
}

TEST_F(AutoArchitectureTest, NetworkTypeName) {
    EXPECT_STREQ(auto_arch_network_type_name(AUTO_ARCH_TYPE_SNN), "SNN");
    EXPECT_STREQ(auto_arch_network_type_name(AUTO_ARCH_TYPE_LNN), "LNN");
    EXPECT_STREQ(auto_arch_network_type_name(AUTO_ARCH_TYPE_CNN), "CNN");
}

TEST_F(AutoArchitectureTest, TaskTypeName) {
    EXPECT_STREQ(auto_arch_task_type_name(AUTO_ARCH_TASK_CLASSIFICATION), "Classification");
    EXPECT_STREQ(auto_arch_task_type_name(AUTO_ARCH_TASK_REGRESSION), "Regression");
    EXPECT_STREQ(auto_arch_task_type_name(AUTO_ARCH_TASK_SEQUENCE), "Sequence");
}

TEST_F(AutoArchitectureTest, LayerTypeName) {
    EXPECT_STREQ(auto_arch_layer_type_name(AUTO_ARCH_LAYER_SNN_LIF), "LIF");
    EXPECT_STREQ(auto_arch_layer_type_name(AUTO_ARCH_LAYER_LNN_LTC), "LTC");
    EXPECT_STREQ(auto_arch_layer_type_name(AUTO_ARCH_LAYER_DENSE), "Dense");
}

TEST_F(AutoArchitectureTest, PrintArchitecture) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.n_inputs = 10;
    task.n_outputs = 2;
    auto_arch_set_task(ctx, &task);

    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    ASSERT_NE(arch, nullptr);

    // Should not crash
    auto_arch_print(arch);
    auto_arch_print(nullptr);

    auto_arch_architecture_destroy(arch);
}

TEST_F(AutoArchitectureTest, PrintResult) {
    ConfigureMinimalSearch();
    ctx = auto_arch_create(&config);
    ASSERT_NE(ctx, nullptr);

    auto_arch_task_t task = {};
    task.n_inputs = 10;
    task.n_outputs = 2;
    auto_arch_set_task(ctx, &task);

    CreateSampleData(10, 10, 2);

    auto_arch_result_t* result = auto_arch_search(ctx, train_data, train_labels, nullptr, nullptr);
    ASSERT_NE(result, nullptr);

    // Should not crash
    auto_arch_result_print(result);
    auto_arch_result_print(nullptr);

    auto_arch_result_destroy(result);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
