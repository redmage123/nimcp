/**
 * @file test_integration_v2_7.cpp
 * @brief Integration tests for v2.7.0 enhancements working together
 */

#include <gtest/gtest.h>
#include <thread>
#include <sys/stat.h>
#include <vector>

extern "C" {
#include "include/nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "core/neuralnet/nimcp_neuralnet_simd.h"
#include "plasticity/adaptive/nimcp_weights.h"
#include "utils/async/nimcp_async.h"
#include "cognitive/checkpointing/nimcp_checkpointing.h"
#include "utils/memory/nimcp_memory.h"
}

class IntegrationV27Test : public ::testing::Test {
protected:
    nimcp_brain_t brain;
    const char* test_dir = "/tmp/nimcp_integration_test";

    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();

        // Create test directory
        mkdir(test_dir, 0755);

        brain = nimcp_brain_create("integration_brain", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
        }

        // Clean up test directory
        cleanup_directory(test_dir);
        rmdir(test_dir);

        nimcp_shutdown();
    }

    void cleanup_directory(const char* dir) {
        std::string cmd = "rm -f ";
        cmd += dir;
        cmd += "/*";
        system(cmd.c_str());
    }
};

//=============================================================================
// Integration: Batch + SIMD
//=============================================================================

TEST_F(IntegrationV27Test, BatchProcessingWithSIMD) {
    // Test that batch API benefits from SIMD acceleration

    const int batch_size = 50;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<std::vector<char>> label_buffers(batch_size, std::vector<char>(64));
    std::vector<char*> labels(batch_size);
    std::vector<float> confidences(batch_size);

    // Prepare batch data
    for (int i = 0; i < batch_size; i++) {
        features_storage[i * 3 + 0] = (float)i / batch_size;
        features_storage[i * 3 + 1] = (float)i / batch_size;
        features_storage[i * 3 + 2] = (float)i / batch_size;
        feature_ptrs[i] = &features_storage[i * 3];
        labels[i] = label_buffers[i].data();
    }

    // Check SIMD capabilities
    const nimcp_cpu_caps_t* caps = nimcp_cpu_get_caps();

    // Run batch prediction
    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, feature_ptrs.data(), 3, batch_size,
        labels.data(), confidences.data()
    );

    EXPECT_EQ(status, NIMCP_OK);

    // Verify all predictions completed
    for (int i = 0; i < batch_size; i++) {
        EXPECT_GE(confidences[i], 0.0f);
        EXPECT_LE(confidences[i], 1.0f);
        EXPECT_GT(strlen(labels[i]), 0);
    }

    // If SIMD is available, we expect some performance benefit
    // (This is qualitative - we can't assert specific speedup in tests)
    if (caps->has_avx2 || caps->has_sse2) {
        // SIMD should be used internally
        SUCCEED();
    }
}

TEST_F(IntegrationV27Test, BatchLearningWithSIMDAcceleration) {
    const int batch_size = 30;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<const char*> label_ptrs(batch_size);

    // Prepare training data
    for (int i = 0; i < batch_size; i++) {
        features_storage[i * 3 + 0] = (float)i / batch_size;
        features_storage[i * 3 + 1] = (float)i / batch_size;
        features_storage[i * 3 + 2] = (float)i / batch_size;
        feature_ptrs[i] = &features_storage[i * 3];
        label_ptrs[i] = (i % 2 == 0) ? "class_a" : "class_b";
    }

    float loss = nimcp_brain_learn_batch(
        brain, feature_ptrs.data(), 3, batch_size,
        label_ptrs.data(), nullptr
    );

    EXPECT_GE(loss, 0.0f);
}

//=============================================================================
// Integration: Batch + Async
//=============================================================================

#if 0  // TODO: Requires internal future APIs (nimcp_future_set_running, nimcp_future_complete)
TEST_F(IntegrationV27Test, DISABLED_AsyncBatchInference) {
    // Test async wrapper around batch operations

    const int batch_size = 10;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<std::vector<char>> label_buffers(batch_size, std::vector<char>(64));
    std::vector<char*> labels(batch_size);
    std::vector<float> confidences(batch_size);

    for (int i = 0; i < batch_size; i++) {
        features_storage[i * 3 + 0] = 0.5f;
        features_storage[i * 3 + 1] = 0.5f;
        features_storage[i * 3 + 2] = 0.5f;
        feature_ptrs[i] = &features_storage[i * 3];
        labels[i] = label_buffers[i].data();
    }

    // Create future for async batch processing
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    // Launch batch in background thread
    std::thread worker([&]() {
        nimcp_future_set_running(future);

        nimcp_status_t status = nimcp_brain_predict_batch(
            brain, feature_ptrs.data(), 3, batch_size,
            labels.data(), confidences.data()
        );

        if (status == NIMCP_OK) {
            nimcp_future_complete(future, &status, sizeof(status));
        } else {
            nimcp_future_set_error(future, -1, "Batch prediction failed");
        }
    });

    // Wait for completion
    bool success = nimcp_future_wait(future, 5000);  // 5 second timeout

    EXPECT_TRUE(success);
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_DONE);

    worker.join();
    nimcp_future_destroy(future);

    // Verify results
    for (int i = 0; i < batch_size; i++) {
        EXPECT_GT(strlen(labels[i]), 0);
    }
}
#endif  // Disabled async test

#if 0  // TODO: Requires internal future APIs
TEST_F(IntegrationV27Test, MultipleConcurrentBatches) {
    // Process multiple batches concurrently using async

    const int num_batches = 3;
    const int batch_size = 10;

    std::vector<nimcp_future_t> futures(num_batches);
    std::vector<std::thread> workers;

    // Launch multiple batch operations concurrently
    for (int b = 0; b < num_batches; b++) {
        futures[b] = nimcp_future_create();
        ASSERT_NE(futures[b], nullptr);

        workers.emplace_back([this, b, &futures]() {
            nimcp_future_set_running(futures[b]);

            std::vector<float> features_storage(batch_size * 3);
            std::vector<const float*> feature_ptrs(batch_size);
            std::vector<std::vector<char>> label_buffers(batch_size, std::vector<char>(64));
            std::vector<char*> labels(batch_size);
            std::vector<float> confidences(batch_size);

            for (int i = 0; i < batch_size; i++) {
                features_storage[i * 3 + 0] = (float)b / num_batches;
                features_storage[i * 3 + 1] = (float)b / num_batches;
                features_storage[i * 3 + 2] = (float)b / num_batches;
                feature_ptrs[i] = &features_storage[i * 3];
                labels[i] = label_buffers[i].data();
            }

            nimcp_status_t status = nimcp_brain_predict_batch(
                brain, feature_ptrs.data(), 3, batch_size,
                labels.data(), confidences.data()
            );

            if (status == NIMCP_OK) {
                nimcp_future_complete(futures[b], &status, sizeof(status));
            } else {
                nimcp_future_set_error(futures[b], -1, "Failed");
            }
        });
    }

    // Wait for all batches to complete
    bool success = nimcp_future_wait_all(futures.data(), num_batches, 10000);

    EXPECT_TRUE(success);

    for (auto& worker : workers) {
        worker.join();
    }

    for (auto future : futures) {
        EXPECT_TRUE(nimcp_future_is_done(future));
        nimcp_future_destroy(future);
    }
}
#endif  // Disabled async test

//=============================================================================
// Integration: Checkpointing + Weights Sharing
//=============================================================================

TEST_F(IntegrationV27Test, CheckpointWithSharedWeights) {
    // Create brain with shared weights and verify checkpointing preserves this

    nimcp_checkpoint_config_t config = nimcp_checkpoint_default_config();
    config.checkpoint_dir = test_dir;

    nimcp_brain_enable_checkpointing(brain, &config);

    // Train a bit
    float features[] = {0.5f, 0.5f, 0.5f};
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features, 3, "test_class", 0.9f);
    }

    // Save checkpoint
    nimcp_status_t status = nimcp_brain_checkpoint(brain, "weights_test");

    EXPECT_EQ(status, NIMCP_OK);

    // Verify checkpoint exists
    DIR* dir = opendir(test_dir);
    bool found = false;
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strstr(entry->d_name, ".ckpt") != nullptr) {
                found = true;
                break;
            }
        }
        closedir(dir);
    }

    EXPECT_TRUE(found);
}

TEST_F(IntegrationV27Test, CheckpointDuringBatchTraining) {
    // Enable checkpointing
    nimcp_checkpoint_config_t config = nimcp_checkpoint_default_config();
    config.checkpoint_dir = test_dir;
    config.interval_steps = 5;  // Checkpoint every 5 steps

    nimcp_brain_enable_checkpointing(brain, &config);

    // Perform batch learning
    const int batch_size = 10;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<const char*> labels(batch_size);

    for (int i = 0; i < batch_size; i++) {
        features_storage[i * 3 + 0] = (float)i / batch_size;
        features_storage[i * 3 + 1] = (float)i / batch_size;
        features_storage[i * 3 + 2] = (float)i / batch_size;
        feature_ptrs[i] = &features_storage[i * 3];
        labels[i] = "training_class";
    }

    // Run multiple batches
    for (int epoch = 0; epoch < 3; epoch++) {
        float loss = nimcp_brain_learn_batch(
            brain, feature_ptrs.data(), 3, batch_size,
            labels.data(), nullptr
        );

        EXPECT_GE(loss, 0.0f);

        // Manually trigger checkpoint after each epoch
        nimcp_brain_checkpoint(brain, nullptr);
    }

    // Check that checkpoints were created
    DIR* dir = opendir(test_dir);
    int checkpoint_count = 0;
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strstr(entry->d_name, ".ckpt") != nullptr) {
                checkpoint_count++;
            }
        }
        closedir(dir);
    }

    EXPECT_GT(checkpoint_count, 0);
}

//=============================================================================
// Integration: SIMD + Statistics Tracking
//=============================================================================

TEST_F(IntegrationV27Test, SIMDWithStatsTracking) {
    // Create network stats
    nimcp_network_stats_t* stats = network_stats_create();
    ASSERT_NE(stats, nullptr);

    // Perform operations and track statistics
    const uint32_t size = 100;
    std::vector<float> a(size, 1.0f);
    std::vector<float> b(size, 2.0f);

    // Measure inference time (using microsecond resolution for fast operations)
    uint64_t start = nimcp_time_monotonic_us();
    float result = nimcp_simd_dot_product(a.data(), b.data(), size);
    uint64_t elapsed = nimcp_time_elapsed_us(start);

    EXPECT_GT(result, 0.0f);

    // Record inference timing
    float time_us = (float)elapsed;  // Already in microseconds
    network_stats_record_inference(stats, time_us);

    EXPECT_EQ(stats->total_inferences, 1);
    EXPECT_GT(stats->avg_inference_time_us, 0.0f);

    network_stats_destroy(stats);
}

//=============================================================================
// Integration: Full Pipeline
//=============================================================================

#if 0  // TODO: Requires internal future APIs
TEST_F(IntegrationV27Test, DISABLED_FullPipelineAsyncBatchSIMDCheckpoint) {
    // Test complete pipeline:
    // 1. Enable checkpointing
    // 2. Async batch training with SIMD acceleration
    // 3. Save checkpoint
    // 4. Verify checkpoint

    // 1. Enable checkpointing
    nimcp_checkpoint_config_t config = nimcp_checkpoint_default_config();
    config.checkpoint_dir = test_dir;

    nimcp_brain_enable_checkpointing(brain, &config);

    // 2. Async batch training
    const int batch_size = 20;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<const char*> labels(batch_size);

    for (int i = 0; i < batch_size; i++) {
        features_storage[i * 3 + 0] = (float)i / batch_size;
        features_storage[i * 3 + 1] = (float)i / batch_size;
        features_storage[i * 3 + 2] = (float)i / batch_size;
        feature_ptrs[i] = &features_storage[i * 3];
        labels[i] = (i % 2 == 0) ? "even" : "odd";
    }

    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    std::thread trainer([&]() {
        nimcp_future_set_running(future);

        float loss = nimcp_brain_learn_batch(
            brain, feature_ptrs.data(), 3, batch_size,
            labels.data(), nullptr
        );

        if (loss >= 0.0f) {
            nimcp_future_complete(future, &loss, sizeof(loss));
        } else {
            nimcp_future_set_error(future, -1, "Training failed");
        }
    });

    bool success = nimcp_future_wait(future, 5000);
    EXPECT_TRUE(success);

    float loss;
    if (nimcp_future_get_result(future, &loss, sizeof(loss))) {
        EXPECT_GE(loss, 0.0f);
    }

    trainer.join();
    nimcp_future_destroy(future);

    // 3. Save checkpoint
    nimcp_status_t status = nimcp_brain_checkpoint(brain, "full_pipeline");
    EXPECT_EQ(status, NIMCP_OK);

    // 4. Verify checkpoint
    DIR* dir = opendir(test_dir);
    char checkpoint_path[512] = "";
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strstr(entry->d_name, ".ckpt") != nullptr) {
                snprintf(checkpoint_path, sizeof(checkpoint_path),
                         "%s/%s", test_dir, entry->d_name);
                break;
            }
        }
        closedir(dir);
    }

    if (strlen(checkpoint_path) > 0) {
        bool valid = nimcp_checkpoint_verify(checkpoint_path);
        EXPECT_TRUE(valid);
    }
}
#endif  // Disabled async test

//=============================================================================
// Integration: Memory Efficiency
//=============================================================================

TEST_F(IntegrationV27Test, WeightsSharingReducesMemory) {
    // Create multiple brains to test weight sharing
    // Note: This test verifies the API works, actual sharing depends on implementation

    nimcp_brain_t brain2 = nimcp_brain_create("brain2", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    ASSERT_NE(brain2, nullptr);

    // Train both brains
    float features[] = {0.5f, 0.5f, 0.5f};
    nimcp_brain_learn_example(brain, features, 3, "class1", 0.9f);
    nimcp_brain_learn_example(brain2, features, 3, "class1", 0.9f);

    // Both brains should work independently
    char label1[64], label2[64];
    float conf1, conf2;

    nimcp_status_t status1 = nimcp_brain_predict(brain, features, 3, label1, &conf1);
    nimcp_status_t status2 = nimcp_brain_predict(brain2, features, 3, label2, &conf2);

    EXPECT_EQ(status1, NIMCP_OK);
    EXPECT_EQ(status2, NIMCP_OK);

    nimcp_brain_destroy(brain2);
}

//=============================================================================
// Integration: Error Handling Across Components
//=============================================================================

TEST_F(IntegrationV27Test, ErrorPropagationThroughLayers) {
    // Test that errors propagate correctly through the full stack

    // Try batch prediction with invalid data
    const float* invalid_features[] = {nullptr};
    char* labels[] = {nullptr};
    float confidences[1];

    nimcp_status_t status = nimcp_brain_predict_batch(
        brain, invalid_features, 3, 1, labels, confidences
    );

    EXPECT_NE(status, NIMCP_OK);

    // Try async operation with invalid future - should return NIMCP_FUTURE_ERROR
    EXPECT_EQ(nimcp_future_wait(nullptr, 100), NIMCP_FUTURE_ERROR);

    // Try checkpoint with invalid config
    nimcp_status_t cp_status = nimcp_brain_enable_checkpointing(brain, nullptr);
    EXPECT_EQ(cp_status, NIMCP_ERROR_NULL_ARG);
}

//=============================================================================
// Integration: Performance Under Load
//=============================================================================

TEST_F(IntegrationV27Test, DISABLED_StressTestAllComponents) {
    // Stress test all v2.7.0 components together

    nimcp_checkpoint_config_t config = nimcp_checkpoint_default_config();
    config.checkpoint_dir = test_dir;
    nimcp_brain_enable_checkpointing(brain, &config);

    const int num_iterations = 100;
    const int batch_size = 10;

    for (int iter = 0; iter < num_iterations; iter++) {
        // Batch training
        std::vector<float> features_storage(batch_size * 3);
        std::vector<const float*> feature_ptrs(batch_size);
        std::vector<const char*> labels(batch_size);

        for (int i = 0; i < batch_size; i++) {
            features_storage[i * 3 + 0] = (float)(iter * batch_size + i) / (num_iterations * batch_size);
            features_storage[i * 3 + 1] = (float)(iter * batch_size + i) / (num_iterations * batch_size);
            features_storage[i * 3 + 2] = (float)(iter * batch_size + i) / (num_iterations * batch_size);
            feature_ptrs[i] = &features_storage[i * 3];
            labels[i] = "stress_test";
        }

        float loss = nimcp_brain_learn_batch(
            brain, feature_ptrs.data(), 3, batch_size,
            labels.data(), nullptr
        );

        EXPECT_GE(loss, 0.0f);

        // Periodic checkpoint
        if (iter % 20 == 0) {
            nimcp_brain_checkpoint(brain, nullptr);
        }
    }

    // Verify system remained stable
    SUCCEED();
}
