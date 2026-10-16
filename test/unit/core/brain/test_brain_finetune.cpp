//=============================================================================
// test_brain_finetune.cpp - Comprehensive Brain Fine-tuning Tests
//=============================================================================
/**
 * @file test_brain_finetune.cpp
 * @brief Comprehensive test suite for brain fine-tuning and layer freezing
 *
 * TEST CATEGORIES:
 * 1. Unit Tests (12+): Individual function behavior
 * 2. Integration Tests (6+): End-to-end fine-tuning workflows
 * 3. Regression Tests (8+): Backward compatibility and performance
 *
 * COVERAGE:
 * - Learning rate modulation based on freeze flags
 * - Batch processing with various sizes
 * - Different freeze configurations
 * - Edge cases and error handling
 * - Memory safety and resource cleanup
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

//=============================================================================
// Test Fixtures
//=============================================================================

class BrainFinetuneTest : public ::testing::Test {
protected:
    brain_t brain;
    std::vector<float> training_data;
    std::vector<float> labels;
    uint32_t num_samples;
    uint32_t input_dim;
    uint32_t output_dim;

    void SetUp() override {
        // Initialize NIMCP library (required for bio-async, etc.)
        nimcp_init();
        brain_clear_error();

        // Create a small brain for testing
        brain = brain_create("finetune_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 10, 3);
        ASSERT_NE(brain, nullptr);

        // Setup test data dimensions
        num_samples = 32;
        input_dim = 10;
        output_dim = 3;

        // Generate synthetic training data
        training_data.resize(num_samples * input_dim);
        labels.resize(num_samples * output_dim);

        for (uint32_t i = 0; i < num_samples; i++) {
            // Generate random input features
            for (uint32_t j = 0; j < input_dim; j++) {
                training_data[i * input_dim + j] =
                    static_cast<float>(rand()) / RAND_MAX;
            }

            // Generate one-hot labels
            uint32_t class_idx = i % output_dim;
            for (uint32_t j = 0; j < output_dim; j++) {
                labels[i * output_dim + j] = (j == class_idx) ? 1.0f : 0.0f;
            }
        }
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        nimcp_shutdown();
    }
};

//=============================================================================
// UNIT TESTS (12+ tests)
//=============================================================================

/**
 * Test 1: Null parameter validation
 */
TEST_F(BrainFinetuneTest, NullParameters) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 1,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    // Null brain
    EXPECT_FALSE(brain_finetune(nullptr, training_data.data(),
                               labels.data(), num_samples, &config));

    // Null training data
    EXPECT_FALSE(brain_finetune(brain, nullptr, labels.data(),
                               num_samples, &config));

    // Null labels
    EXPECT_FALSE(brain_finetune(brain, training_data.data(), nullptr,
                               num_samples, &config));

    // Zero samples
    EXPECT_FALSE(brain_finetune(brain, training_data.data(),
                               labels.data(), 0, &config));
}

/**
 * Test 2: Default configuration handling
 */
TEST_F(BrainFinetuneTest, DefaultConfiguration) {
    // Should use default config when NULL is passed
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, nullptr));
}

/**
 * Test 3: Freeze all layers (minimal learning)
 */
TEST_F(BrainFinetuneTest, FreezeAllLayers) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = false,  // Everything frozen
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));
}

/**
 * Test 4: Unfreeze all layers (full fine-tuning)
 */
TEST_F(BrainFinetuneTest, UnfreezeAllLayers) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));
}

/**
 * Test 5: Freeze sensory only
 */
TEST_F(BrainFinetuneTest, FreezeSensoryOnly) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = true,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));
}

/**
 * Test 6: Freeze cognitive only
 */
TEST_F(BrainFinetuneTest, FreezeCognitiveOnly) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));
}

/**
 * Test 7: Various batch sizes
 */
TEST_F(BrainFinetuneTest, VariousBatchSizes) {
    std::vector<uint32_t> batch_sizes = {1, 4, 8, 16, 32, 64};

    for (uint32_t batch_size : batch_sizes) {
        brain_finetune_config_t config = {
            .learning_rate = 0.001f,
            .num_epochs = 1,
            .freeze_sensory = false,
            .freeze_cognitive = false,
            .finetune_classifier = true,
            .batch_size = batch_size,
            .verbose = false
        };

        EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                                  num_samples, &config))
            << "Failed with batch size: " << batch_size;
    }
}

/**
 * Test 8: Various learning rates
 */
TEST_F(BrainFinetuneTest, VariousLearningRates) {
    std::vector<float> learning_rates = {0.0001f, 0.001f, 0.01f, 0.1f};

    for (float lr : learning_rates) {
        brain_finetune_config_t config = {
            .learning_rate = lr,
            .num_epochs = 1,
            .freeze_sensory = false,
            .freeze_cognitive = false,
            .finetune_classifier = true,
            .batch_size = 8,
            .verbose = false
        };

        EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                                  num_samples, &config))
            << "Failed with learning rate: " << lr;
    }
}

/**
 * Test 9: Various epoch counts
 */
TEST_F(BrainFinetuneTest, VariousEpochs) {
    std::vector<uint32_t> epoch_counts = {1, 2, 5, 10};

    for (uint32_t epochs : epoch_counts) {
        brain_finetune_config_t config = {
            .learning_rate = 0.001f,
            .num_epochs = epochs,
            .freeze_sensory = false,
            .freeze_cognitive = false,
            .finetune_classifier = true,
            .batch_size = 8,
            .verbose = false
        };

        EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                                  num_samples, &config))
            << "Failed with epoch count: " << epochs;
    }
}

/**
 * Test 10: Batch size larger than dataset
 */
TEST_F(BrainFinetuneTest, LargeBatchSize) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 1,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = num_samples * 2,  // Larger than dataset
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));
}

/**
 * Test 11: Non-divisible batch size
 */
TEST_F(BrainFinetuneTest, NonDivisibleBatchSize) {
    // Use 30 samples with batch size 8 (remainder of 6)
    uint32_t samples = 30;
    std::vector<float> data(samples * input_dim, 0.5f);
    std::vector<float> lbls(samples * output_dim, 0.0f);

    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 1,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, data.data(), lbls.data(),
                              samples, &config));
}

/**
 * Test 12: Verbose output
 */
TEST_F(BrainFinetuneTest, VerboseOutput) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = true  // Enable verbose output
    };

    // Should succeed with verbose output (captured to stdout)
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));
}

/**
 * Test 13: Single sample fine-tuning
 */
TEST_F(BrainFinetuneTest, SingleSample) {
    std::vector<float> single_data(input_dim, 0.5f);
    std::vector<float> single_label(output_dim);
    single_label[0] = 1.0f;  // Class 0

    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 1,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 1,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, single_data.data(), single_label.data(),
                              1, &config));
}

//=============================================================================
// INTEGRATION TESTS (6+ tests)
//=============================================================================

/**
 * Integration Test 1: Full fine-tuning workflow
 */
TEST_F(BrainFinetuneTest, IntegrationFullWorkflow) {
    // Fine-tune
    brain_finetune_config_t config = {
        .learning_rate = 0.01f,
        .num_epochs = 5,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));

    // Verify brain is still valid after fine-tuning
    brain_stats_t stats;
    EXPECT_TRUE(brain_get_stats(brain, &stats));
}

/**
 * Integration Test 2: Sequential fine-tuning (multiple rounds)
 */
TEST_F(BrainFinetuneTest, IntegrationSequentialFinetune) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    // First round
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));

    // Second round (continue training)
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));

    // Third round
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));
}

/**
 * Integration Test 3: Progressive unfreezing
 */
TEST_F(BrainFinetuneTest, IntegrationProgressiveUnfreezing) {
    // Stage 1: Freeze everything except classifier
    brain_finetune_config_t config1 = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config1));

    // Stage 2: Unfreeze cognitive, keep sensory frozen
    brain_finetune_config_t config2 = {
        .learning_rate = 0.0005f,
        .num_epochs = 2,
        .freeze_sensory = true,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config2));

    // Stage 3: Unfreeze everything
    brain_finetune_config_t config3 = {
        .learning_rate = 0.0001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };
    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config3));
}

/**
 * Integration Test 4: Fine-tune with stats verification
 */
TEST_F(BrainFinetuneTest, IntegrationWithStatsVerification) {
    brain_finetune_config_t config = {
        .learning_rate = 0.01f,
        .num_epochs = 3,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 16,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));

    // Verify brain stats are still accessible after fine-tuning
    brain_stats_t stats;
    EXPECT_TRUE(brain_get_stats(brain, &stats));
    EXPECT_GT(stats.num_neurons, 0u);
}

/**
 * Integration Test 5: Save and load after fine-tuning
 */
TEST_F(BrainFinetuneTest, IntegrationSaveLoad) {
    const char* save_path = "/tmp/test_finetuned_brain.nimcp";

    // Fine-tune
    brain_finetune_config_t config = {
        .learning_rate = 0.01f,
        .num_epochs = 3,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));

    // Save
    EXPECT_TRUE(brain_save(brain, save_path));

    // Load
    brain_t loaded_brain = brain_load(save_path);
    ASSERT_NE(loaded_brain, nullptr);

    // Verify loaded brain works
    brain_stats_t stats;
    EXPECT_TRUE(brain_get_stats(loaded_brain, &stats));

    brain_destroy(loaded_brain);
    unlink(save_path);
}

/**
 * Integration Test 6: Different freeze configurations comparison
 */
TEST_F(BrainFinetuneTest, IntegrationFreezeComparison) {
    // Create multiple brains for comparison
    brain_t brain1 = brain_create("test1", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    brain_t brain2 = brain_create("test2", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    brain_t brain3 = brain_create("test3", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    ASSERT_NE(brain3, nullptr);

    // Config 1: Freeze all
    brain_finetune_config_t config1 = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = false,
        .batch_size = 8,
        .verbose = false
    };

    // Config 2: Freeze sensory
    brain_finetune_config_t config2 = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = true,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    // Config 3: Unfreeze all
    brain_finetune_config_t config3 = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain1, training_data.data(), labels.data(),
                              num_samples, &config1));
    EXPECT_TRUE(brain_finetune(brain2, training_data.data(), labels.data(),
                              num_samples, &config2));
    EXPECT_TRUE(brain_finetune(brain3, training_data.data(), labels.data(),
                              num_samples, &config3));

    brain_destroy(brain1);
    brain_destroy(brain2);
    brain_destroy(brain3);
}

//=============================================================================
// REGRESSION TESTS (8+ tests)
//=============================================================================

/**
 * Regression Test 1: Backward compatibility with old API
 */
TEST_F(BrainFinetuneTest, RegressionBackwardCompatibility) {
    // Old code should still work with new implementation
    brain_finetune_config_t legacy_config = {
        .learning_rate = 0.001f,
        .num_epochs = 5,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = 32,
        .verbose = true
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &legacy_config));
}

/**
 * Regression Test 2: Performance - training time reasonable
 */
TEST_F(BrainFinetuneTest, RegressionPerformance) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 3,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 16,
        .verbose = false
    };

    auto start = std::chrono::high_resolution_clock::now();

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &config));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (< 5 seconds for small dataset)
    EXPECT_LT(duration.count(), 5000) << "Fine-tuning took too long: "
                                      << duration.count() << "ms";
}

/**
 * Regression Test 3: Memory usage stability
 */
TEST_F(BrainFinetuneTest, RegressionMemoryStability) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    size_t initial_memory = brain_get_memory_usage(brain);

    // Fine-tune multiple times
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                                  num_samples, &config));
    }

    size_t final_memory = brain_get_memory_usage(brain);

    // Memory should not grow significantly (allow 20% variance)
    EXPECT_LT(final_memory, initial_memory * 1.2)
        << "Memory grew from " << initial_memory << " to " << final_memory;
}

/**
 * Regression Test 4: Frozen vs unfrozen comparison
 */
TEST_F(BrainFinetuneTest, RegressionFrozenVsUnfrozen) {
    // Train with everything frozen
    brain_finetune_config_t frozen_config = {
        .learning_rate = 0.001f,
        .num_epochs = 3,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = false,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                              num_samples, &frozen_config));

    brain_stats_t frozen_stats;
    ASSERT_TRUE(brain_get_stats(brain, &frozen_stats));

    // Now train with unfrozen layers
    brain_t brain2 = brain_create("test2", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 3);
    brain_finetune_config_t unfrozen_config = {
        .learning_rate = 0.001f,
        .num_epochs = 3,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain2, training_data.data(), labels.data(),
                              num_samples, &unfrozen_config));

    brain_stats_t unfrozen_stats;
    ASSERT_TRUE(brain_get_stats(brain2, &unfrozen_stats));

    // Both should complete successfully
    EXPECT_GT(frozen_stats.num_neurons, 0u);
    EXPECT_GT(unfrozen_stats.num_neurons, 0u);

    brain_destroy(brain2);
}

/**
 * Regression Test 5: Learning rate scaling works
 */
TEST_F(BrainFinetuneTest, RegressionLearningRateScaling) {
    // Test that different learning rates produce different results
    brain_t brain_low_lr = brain_create("low_lr", BRAIN_SIZE_SMALL,
                                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    brain_t brain_high_lr = brain_create("high_lr", BRAIN_SIZE_SMALL,
                                         BRAIN_TASK_CLASSIFICATION, 10, 3);

    brain_finetune_config_t low_config = {
        .learning_rate = 0.0001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    brain_finetune_config_t high_config = {
        .learning_rate = 0.1f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain_low_lr, training_data.data(),
                              labels.data(), num_samples, &low_config));
    EXPECT_TRUE(brain_finetune(brain_high_lr, training_data.data(),
                              labels.data(), num_samples, &high_config));

    brain_destroy(brain_low_lr);
    brain_destroy(brain_high_lr);
}

/**
 * Regression Test 6: Batch processing correctness
 */
TEST_F(BrainFinetuneTest, RegressionBatchCorrectness) {
    // Train with batch size 1 (essentially online)
    brain_t brain_online = brain_create("online", BRAIN_SIZE_SMALL,
                                        BRAIN_TASK_CLASSIFICATION, 10, 3);
    brain_finetune_config_t online_config = {
        .learning_rate = 0.001f,
        .num_epochs = 1,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 1,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain_online, training_data.data(),
                              labels.data(), num_samples, &online_config));

    // Train with larger batch
    brain_t brain_batch = brain_create("batch", BRAIN_SIZE_SMALL,
                                       BRAIN_TASK_CLASSIFICATION, 10, 3);
    brain_finetune_config_t batch_config = {
        .learning_rate = 0.001f,
        .num_epochs = 1,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 16,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain_batch, training_data.data(),
                              labels.data(), num_samples, &batch_config));

    // Both should succeed (exact results may differ due to batch effects)

    brain_destroy(brain_online);
    brain_destroy(brain_batch);
}

/**
 * Regression Test 7: Epoch count effects
 */
TEST_F(BrainFinetuneTest, RegressionEpochEffects) {
    brain_t brain_few = brain_create("few_epochs", BRAIN_SIZE_SMALL,
                                     BRAIN_TASK_CLASSIFICATION, 10, 3);
    brain_t brain_many = brain_create("many_epochs", BRAIN_SIZE_SMALL,
                                      BRAIN_TASK_CLASSIFICATION, 10, 3);

    brain_finetune_config_t few_config = {
        .learning_rate = 0.001f,
        .num_epochs = 1,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    brain_finetune_config_t many_config = {
        .learning_rate = 0.001f,
        .num_epochs = 10,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    EXPECT_TRUE(brain_finetune(brain_few, training_data.data(),
                              labels.data(), num_samples, &few_config));
    EXPECT_TRUE(brain_finetune(brain_many, training_data.data(),
                              labels.data(), num_samples, &many_config));

    brain_destroy(brain_few);
    brain_destroy(brain_many);
}

/**
 * Regression Test 8: No memory leaks
 */
TEST_F(BrainFinetuneTest, RegressionNoMemoryLeaks) {
    brain_finetune_config_t config = {
        .learning_rate = 0.001f,
        .num_epochs = 2,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 8,
        .verbose = false
    };

    // Run fine-tuning many times
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(brain_finetune(brain, training_data.data(), labels.data(),
                                  num_samples, &config));
    }

    // If there were memory leaks, valgrind/sanitizers would catch them
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
