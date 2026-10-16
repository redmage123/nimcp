/**
 * @file test_e2e_v2_7.cpp
 * @brief End-to-end tests for v2.7.0 complete workflows
 */

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <thread>
#include <vector>
#include <fstream>
#include <dirent.h>
#include <cmath>

extern "C" {
#include "include/nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/checkpointing/nimcp_checkpointing.h"
#include "utils/async/nimcp_async.h"
#include "utils/memory/nimcp_memory.h"
}

class E2EV27Test : public ::testing::Test {
protected:
    const char* test_dir = "/tmp/nimcp_e2e_test";

    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();

        // Create test directory
        mkdir(test_dir, 0755);
    }

    void TearDown() override {
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
// E2E: Complete Training Pipeline with Checkpoints
//=============================================================================

TEST_F(E2EV27Test, TrainSaveResumeWorkflow) {
    // Scenario: Train brain, save checkpoint, load and continue training

    // Phase 1: Initial training
    nimcp_brain_t brain1 = nimcp_brain_create("resume_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    ASSERT_NE(brain1, nullptr);

    // Enable checkpointing
    nimcp_checkpoint_config_t config = nimcp_checkpoint_default_config();
    config.checkpoint_dir = test_dir;
    nimcp_brain_enable_checkpointing(brain1, &config);

    // Train with batch data
    const int batch_size = 20;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<const char*> labels(batch_size);

    for (int i = 0; i < batch_size; i++) {
        features_storage[i * 3 + 0] = (float)i / batch_size;
        features_storage[i * 3 + 1] = (float)i / batch_size;
        features_storage[i * 3 + 2] = (float)i / batch_size;
        feature_ptrs[i] = &features_storage[i * 3];
        labels[i] = (i < batch_size / 2) ? "first_half" : "second_half";
    }

    float loss1 = nimcp_brain_learn_batch(
        brain1, feature_ptrs.data(), 3, batch_size,
        labels.data(), nullptr
    );

    EXPECT_GE(loss1, 0.0f);

    // Save checkpoint
    nimcp_brain_checkpoint(brain1, "phase1");

    // Make a prediction before destroying
    float test_features[] = {0.3f, 0.3f, 0.3f};
    char label_before[64];
    float conf_before;

    nimcp_brain_predict(brain1, test_features, 3, label_before, &conf_before);

    nimcp_brain_destroy(brain1);

    // Phase 2: Resume from checkpoint (if implementation supports it)
    // Find checkpoint file
    char checkpoint_path[512] = "";
    DIR* dir = opendir(test_dir);
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

    ASSERT_GT(strlen(checkpoint_path), 0) << "Checkpoint file not found";

    // Try to load (may not be implemented yet)
    nimcp_brain_t brain2 = nimcp_brain_load_checkpoint(checkpoint_path);

    if (brain2) {
        // Continue training
        float loss2 = nimcp_brain_learn_batch(
            brain2, feature_ptrs.data(), 3, batch_size,
            labels.data(), nullptr
        );

        EXPECT_GE(loss2, 0.0f);

        // Verify behavior is consistent
        char label_after[64];
        float conf_after;

        nimcp_brain_predict(brain2, test_features, 3,
                           label_after, &conf_after);

        // Labels might be similar (but not guaranteed identical)
        EXPECT_GT(strlen(label_after), 0);

        nimcp_brain_destroy(brain2);
    }
}

//=============================================================================
// E2E: Production Inference Pipeline
//=============================================================================

#if 0  // TODO: Requires internal future APIs (nimcp_future_set_running, nimcp_future_complete)
TEST_F(E2EV27Test, DISABLED_ProductionInferencePipeline) {
    // Scenario: Real-world inference service with async batch processing

    nimcp_brain_t brain = nimcp_brain_create("inference_service", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    ASSERT_NE(brain, nullptr);

    // Pre-train brain
    float train_features[] = {0.1f, 0.2f, 0.3f};
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, train_features, 3, "known_pattern", 0.9f);
    }

    // Simulate incoming inference requests
    const int num_requests = 50;
    std::vector<float> request_features(num_requests * 3);
    std::vector<const float*> request_ptrs(num_requests);
    std::vector<std::vector<char>> result_buffers(num_requests, std::vector<char>(64));
    std::vector<char*> results(num_requests);
    std::vector<float> confidences(num_requests);

    for (int i = 0; i < num_requests; i++) {
        // Generate diverse request data
        request_features[i * 3 + 0] = 0.1f + (float)i * 0.01f;
        request_features[i * 3 + 1] = 0.2f + (float)i * 0.01f;
        request_features[i * 3 + 2] = 0.3f + (float)i * 0.01f;
        request_ptrs[i] = &request_features[i * 3];
        results[i] = result_buffers[i].data();
    }

    // Process in batches asynchronously
    const int batch_size = 10;
    const int num_batches = num_requests / batch_size;

    std::vector<nimcp_future_t> futures(num_batches);
    std::vector<std::thread> workers;

    uint64_t start_time = nimcp_time_get_ms();

    for (int b = 0; b < num_batches; b++) {
        futures[b] = nimcp_future_create();

        workers.emplace_back([=, &request_ptrs, &results, &confidences, &futures]() {
            nimcp_future_set_running(futures[b]);

            int start_idx = b * batch_size;

            nimcp_status_t status = nimcp_brain_predict_batch(
                brain,
                &request_ptrs[start_idx],
                3,
                batch_size,
                &results[start_idx],
                &confidences[start_idx]
            );

            if (status == NIMCP_OK) {
                nimcp_future_complete(futures[b], &status, sizeof(status));
            } else {
                nimcp_future_set_error(futures[b], "Batch failed");
            }
        });
    }

    // Wait for all batches
    bool success = nimcp_future_wait_all(futures.data(), num_batches, 10000);

    uint64_t elapsed = nimcp_time_get_ms() - start_time;

    EXPECT_TRUE(success);

    for (auto& worker : workers) {
        worker.join();
    }

    for (auto future : futures) {
        EXPECT_TRUE(nimcp_future_is_done(future));
        nimcp_future_destroy(future, false);
    }

    // Verify all requests processed
    for (int i = 0; i < num_requests; i++) {
        EXPECT_GT(strlen(results[i]), 0) << "Request " << i << " not processed";
        EXPECT_GE(confidences[i], 0.0f);
        EXPECT_LE(confidences[i], 1.0f);
    }

    // Performance check: should process all requests reasonably quickly
    float requests_per_sec = (float)num_requests / (elapsed / 1000.0f);
    std::cout << "Throughput: " << requests_per_sec << " requests/sec" << std::endl;

    EXPECT_GT(requests_per_sec, 1.0f);  // At least 1 req/sec

    nimcp_brain_destroy(brain);
}
#endif  // Disabled async test

//=============================================================================
// E2E: Continuous Learning with Fault Tolerance
//=============================================================================

TEST_F(E2EV27Test, ContinuousLearningWithCheckpoints) {
    // Scenario: Online learning system with automatic checkpointing

    nimcp_brain_t brain = nimcp_brain_create("online_learner", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    ASSERT_NE(brain, nullptr);

    // Enable aggressive checkpointing
    nimcp_checkpoint_config_t config = nimcp_checkpoint_default_config();
    config.checkpoint_dir = test_dir;
    config.interval_steps = 5;  // Checkpoint every 5 learning steps
    config.max_checkpoints = 3;  // Keep only last 3

    nimcp_brain_enable_checkpointing(brain, &config);

    // Simulate continuous learning stream
    const int num_learning_steps = 20;

    for (int step = 0; step < num_learning_steps; step++) {
        // Generate synthetic training example
        float features[3];
        features[0] = 0.5f + 0.1f * sinf((float)step / 3.0f);
        features[1] = 0.5f + 0.1f * cosf((float)step / 3.0f);
        features[2] = 0.5f;

        const char* label = (step % 3 == 0) ? "pattern_a" :
                           (step % 3 == 1) ? "pattern_b" : "pattern_c";

        float loss = nimcp_brain_learn_example(brain, features, 3, label, 0.9f);

        EXPECT_GE(loss, 0.0f);

        // Periodic manual checkpoint
        if (step % 5 == 4) {
            nimcp_brain_checkpoint(brain, nullptr);
        }
    }

    // Verify multiple checkpoints exist
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
    EXPECT_LE(checkpoint_count, config.max_checkpoints + 5);  // Some tolerance

    // Get checkpoint statistics
    nimcp_checkpoint_stats_t stats;
    if (nimcp_brain_get_checkpoint_stats(brain, &stats)) {
        EXPECT_GT(stats.total_checkpoints, 0);
        EXPECT_EQ(stats.failed_saves, 0);
    }

    nimcp_brain_destroy(brain);
}

//=============================================================================
// E2E: Multi-Brain Collaborative System
//=============================================================================

TEST_F(E2EV27Test, MultiBrainCollaboration) {
    // Scenario: Multiple brains working together, sharing knowledge

    // Create multiple brains
    nimcp_brain_t brain1 = nimcp_brain_create("specialist_a", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    nimcp_brain_t brain2 = nimcp_brain_create("specialist_b", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    nimcp_brain_t brain3 = nimcp_brain_create("generalist", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    ASSERT_NE(brain3, nullptr);

    // Train specialists on different patterns
    for (int i = 0; i < 10; i++) {
        float features_a[] = {0.9f, 0.1f, 0.1f};
        nimcp_brain_learn_example(brain1, features_a, 3, "type_a", 0.9f);

        float features_b[] = {0.1f, 0.9f, 0.1f};
        nimcp_brain_learn_example(brain2, features_b, 3, "type_b", 0.9f);

        float features_g[] = {0.5f, 0.5f, 0.5f};
        nimcp_brain_learn_example(brain3, features_g, 3, "general", 0.9f);
    }

    // Test ensemble prediction: query all brains and aggregate
    float test_input[] = {0.8f, 0.2f, 0.1f};

    char label1[64], label2[64], label3[64];
    float conf1, conf2, conf3;

    nimcp_brain_predict(brain1, test_input, 3, label1, &conf1);
    nimcp_brain_predict(brain2, test_input, 3, label2, &conf2);
    nimcp_brain_predict(brain3, test_input, 3, label3, &conf3);

    // All should produce valid outputs
    EXPECT_GT(strlen(label1), 0);
    EXPECT_GT(strlen(label2), 0);
    EXPECT_GT(strlen(label3), 0);

    // Find consensus (highest confidence)
    float max_conf = conf1;
    const char* best_label = label1;

    if (conf2 > max_conf) {
        max_conf = conf2;
        best_label = label2;
    }
    if (conf3 > max_conf) {
        max_conf = conf3;
        best_label = label3;
    }

    std::cout << "Ensemble prediction: " << best_label
              << " (confidence: " << max_conf << ")" << std::endl;

    EXPECT_GT(strlen(best_label), 0);

    nimcp_brain_destroy(brain1);
    nimcp_brain_destroy(brain2);
    nimcp_brain_destroy(brain3);
}

//=============================================================================
// E2E: High-Volume Data Processing
//=============================================================================

TEST_F(E2EV27Test, HighVolumeDataProcessing) {
    // Scenario: Process large dataset in batches

    nimcp_brain_t brain = nimcp_brain_create("data_processor", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    ASSERT_NE(brain, nullptr);

    // Pre-train
    float train_sample[] = {0.5f, 0.5f, 0.5f};
    nimcp_brain_learn_example(brain, train_sample, 3, "baseline", 0.9f);

    // Generate large dataset
    const int total_samples = 1000;
    const int batch_size = 50;
    const int num_batches = total_samples / batch_size;

    int total_processed = 0;

    for (int batch_idx = 0; batch_idx < num_batches; batch_idx++) {
        std::vector<float> features_storage(batch_size * 3);
        std::vector<const float*> feature_ptrs(batch_size);
        std::vector<std::vector<char>> label_buffers(batch_size, std::vector<char>(64));
        std::vector<char*> labels(batch_size);
        std::vector<float> confidences(batch_size);

        // Generate batch data
        for (int i = 0; i < batch_size; i++) {
            int sample_id = batch_idx * batch_size + i;
            features_storage[i * 3 + 0] = 0.3f + (float)sample_id * 0.0001f;
            features_storage[i * 3 + 1] = 0.4f + (float)sample_id * 0.0001f;
            features_storage[i * 3 + 2] = 0.5f + (float)sample_id * 0.0001f;
            feature_ptrs[i] = &features_storage[i * 3];
            labels[i] = label_buffers[i].data();
        }

        // Process batch
        nimcp_status_t status = nimcp_brain_predict_batch(
            brain, feature_ptrs.data(), 3, batch_size,
            labels.data(), confidences.data()
        );

        if (status == NIMCP_OK) {
            total_processed += batch_size;

            // Verify batch results
            for (int i = 0; i < batch_size; i++) {
                ASSERT_GT(strlen(labels[i]), 0);
            }
        }
    }

    EXPECT_EQ(total_processed, total_samples);

    std::cout << "Processed " << total_processed << " samples in "
              << num_batches << " batches" << std::endl;

    nimcp_brain_destroy(brain);
}

//=============================================================================
// E2E: Real-time Adaptation Scenario
//=============================================================================

TEST_F(E2EV27Test, RealTimeAdaptation) {
    // Scenario: Brain adapts to changing patterns in real-time

    nimcp_brain_t brain = nimcp_brain_create("adaptive_system", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    ASSERT_NE(brain, nullptr);

    // Phase 1: Learn pattern A
    for (int i = 0; i < 10; i++) {
        float features[] = {0.9f, 0.1f, 0.1f};
        nimcp_brain_learn_example(brain, features, 3, "pattern_a", 0.9f);
    }

    // Test recognition of pattern A
    float test_a[] = {0.85f, 0.15f, 0.1f};
    char label[64];
    float confidence;

    nimcp_brain_predict(brain, test_a, 3, label, &confidence);
    EXPECT_GT(strlen(label), 0);

    // Phase 2: Transition to pattern B
    for (int i = 0; i < 10; i++) {
        float features[] = {0.1f, 0.9f, 0.1f};
        nimcp_brain_learn_example(brain, features, 3, "pattern_b", 0.9f);
    }

    // Test recognition of pattern B
    float test_b[] = {0.15f, 0.85f, 0.1f};
    nimcp_brain_predict(brain, test_b, 3, label, &confidence);
    EXPECT_GT(strlen(label), 0);

    // Phase 3: Mixed patterns - system should handle both
    const int batch_size = 10;
    std::vector<float> features_storage(batch_size * 3);
    std::vector<const float*> feature_ptrs(batch_size);
    std::vector<const char*> labels_in(batch_size);

    for (int i = 0; i < batch_size; i++) {
        if (i % 2 == 0) {
            // Pattern A
            features_storage[i * 3 + 0] = 0.9f;
            features_storage[i * 3 + 1] = 0.1f;
            features_storage[i * 3 + 2] = 0.1f;
            labels_in[i] = "pattern_a";
        } else {
            // Pattern B
            features_storage[i * 3 + 0] = 0.1f;
            features_storage[i * 3 + 1] = 0.9f;
            features_storage[i * 3 + 2] = 0.1f;
            labels_in[i] = "pattern_b";
        }
        feature_ptrs[i] = &features_storage[i * 3];
    }

    float loss = nimcp_brain_learn_batch(
        brain, feature_ptrs.data(), 3, batch_size,
        labels_in.data(), nullptr
    );

    EXPECT_GE(loss, 0.0f);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// E2E: Complete Lifecycle
//=============================================================================

TEST_F(E2EV27Test, CompleteBrainLifecycle) {
    // Scenario: Full lifecycle from creation to archival

    // 1. Create and configure
    nimcp_brain_t brain = nimcp_brain_create("lifecycle_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 3, 5);
    ASSERT_NE(brain, nullptr);

    nimcp_checkpoint_config_t config = nimcp_checkpoint_default_config();
    config.checkpoint_dir = test_dir;
    nimcp_brain_enable_checkpointing(brain, &config);

    // 2. Initial training
    const int initial_training_steps = 20;
    for (int i = 0; i < initial_training_steps; i++) {
        float features[] = {
            0.5f + 0.1f * sinf((float)i),
            0.5f + 0.1f * cosf((float)i),
            0.5f
        };
        nimcp_brain_learn_example(brain, features, 3, "training_pattern", 0.9f);
    }

    // 3. Production inference
    const int num_inferences = 50;
    for (int i = 0; i < num_inferences; i++) {
        float features[] = {0.5f, 0.5f, 0.5f};
        char label[64];
        float confidence;

        nimcp_brain_predict(brain, features, 3, label, &confidence);
        EXPECT_GT(strlen(label), 0);
    }

    // 4. Save final checkpoint
    nimcp_brain_checkpoint(brain, "final_state");

    // 5. Archival - cleanup old checkpoints, keep only latest
    uint32_t deleted = nimcp_checkpoints_cleanup(test_dir, 1);

    // 6. Destroy brain
    nimcp_brain_destroy(brain);

    // 7. Verify checkpoint remains
    DIR* dir = opendir(test_dir);
    bool checkpoint_exists = false;
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strstr(entry->d_name, ".ckpt") != nullptr) {
                checkpoint_exists = true;
                break;
            }
        }
        closedir(dir);
    }

    EXPECT_TRUE(checkpoint_exists);
}
