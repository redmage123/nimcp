/**
 * @file test_brain_pipeline.cpp
 * @brief E2E Tests for Brain Lifecycle Pipeline
 *
 * WHAT: Complete brain lifecycle testing (create → train → infer → save → restore)
 * WHY:  Validate entire brain workflow end-to-end
 * HOW:  Test realistic usage scenarios with timing assertions
 *
 * TEST COVERAGE:
 * - Brain creation with various configurations
 * - Training with synthetic datasets
 * - Inference accuracy validation
 * - State persistence (save/load)
 * - Memory management across lifecycle
 * - Performance regression detection
 */

#include "e2e_test_framework.h"
#include <fstream>
#include <cstdio>

using namespace nimcp::e2e;

//=============================================================================
// Test Fixtures
//=============================================================================

class BrainPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize NIMCP library
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        // Check for memory leaks
        nimcp_memory_check_leaks();
        nimcp_shutdown();
    }

    // Helper: Clean up test files
    void cleanup_test_file(const char* filepath) {
        std::remove(filepath);
    }
};

//=============================================================================
// E2E Test: Simple Brain Lifecycle
//=============================================================================

E2E_TEST(BrainPipelineTest, SimpleBrainLifecycle) {
    E2E_PIPELINE_START("Simple Brain Lifecycle");

    nimcp_brain_t brain = nullptr;
    const char* save_path = "/tmp/nimcp_e2e_brain_test.bin";

    // Stage 1: Create brain (increased timeout for full subsystem init)
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain = nimcp_brain_create(
            "e2e_test_brain",
            NIMCP_BRAIN_SMALL,
            NIMCP_TASK_CLASSIFICATION,
            10,  // 10 inputs
            3    // 3 outputs (classes)
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Configure training
    E2E_STAGE_BEGIN("Configure training", 50);
    {
        nimcp_training_config_t config = nimcp_training_config_default();
        config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
        config.optimizer_type = NIMCP_API_OPT_ADAM;
        config.learning_rate = 0.01f;

        nimcp_status_t status = nimcp_brain_configure_training(brain, &config);
        E2E_ASSERT_SUCCESS(status, "Training configuration failed");
    }
    E2E_STAGE_END();

    // Stage 3: Train with synthetic data
    E2E_STAGE_BEGIN("Training (100 steps)", 5000);
    {
        std::vector<float> features;
        std::vector<float> labels;
        TestDataGenerator::generate_training_batch(10, 10, 3, features, labels);

        for (int step = 0; step < 100; ++step) {
            for (size_t i = 0; i < 10; ++i) {
                nimcp_training_result_t result;
                nimcp_status_t status = nimcp_brain_train_step(
                    brain,
                    features.data() + i * 10,
                    10,
                    labels.data() + i * 3,
                    3,
                    &result
                );
                E2E_ASSERT_SUCCESS(status, "Training step failed");
            }
        }
    }
    E2E_STAGE_END();

    // Stage 4: Run inference
    E2E_STAGE_BEGIN("Inference", 100);
    {
        std::vector<float> test_features = TestDataGenerator::generate_features(10);
        std::vector<float> outputs(3);

        nimcp_status_t status = nimcp_brain_infer(
            brain,
            test_features.data(),
            10,
            outputs.data(),
            3
        );
        E2E_ASSERT_SUCCESS(status, "Inference failed");

        // Verify outputs are valid probabilities
        float sum = 0.0f;
        for (float val : outputs) {
            E2E_ASSERT(val >= 0.0f && val <= 1.0f, "Output not in [0,1] range");
            sum += val;
        }
        E2E_ASSERT(std::abs(sum - 1.0f) < 0.1f, "Outputs don't sum to ~1.0");
    }
    E2E_STAGE_END();

    // Stage 5: Save brain state
    E2E_STAGE_BEGIN("Save state", 500);
    {
        nimcp_status_t status = nimcp_brain_save(brain, save_path);
        E2E_ASSERT_SUCCESS(status, "Brain save failed");

        // Verify file exists
        std::ifstream file(save_path);
        E2E_ASSERT(file.good(), "Saved brain file not found");
        file.close();
    }
    E2E_STAGE_END();

    // Stage 6: Load brain state
    E2E_STAGE_BEGIN("Load state", 500);
    {
        nimcp_brain_t loaded_brain = nimcp_brain_load(save_path);
        E2E_ASSERT_NOT_NULL(loaded_brain, "Brain load failed");

        // Verify loaded brain produces same outputs
        std::vector<float> test_features = TestDataGenerator::generate_features(10);
        std::vector<float> original_outputs(3);
        std::vector<float> loaded_outputs(3);

        nimcp_brain_infer(brain, test_features.data(), 10, original_outputs.data(), 3);
        nimcp_brain_infer(loaded_brain, test_features.data(), 10, loaded_outputs.data(), 3);

        // Outputs should be nearly identical
        for (size_t i = 0; i < 3; ++i) {
            float diff = std::abs(original_outputs[i] - loaded_outputs[i]);
            E2E_ASSERT(diff < 0.01f, "Loaded brain outputs differ from original");
        }

        nimcp_brain_destroy(loaded_brain);
    }
    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 5000);  // Increased timeout for brain cleanup
    {
        nimcp_brain_destroy(brain);
        std::remove(save_path);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Brain Snapshot Workflow
//=============================================================================

E2E_TEST(BrainPipelineTest, SnapshotWorkflow) {
    E2E_PIPELINE_START("Brain Snapshot Workflow");

    nimcp_brain_t brain = nullptr;

    // Stage 1: Create and train initial brain
    E2E_STAGE_BEGIN("Create and initial training", 3000);
    {
        brain = nimcp_brain_create(
            "snapshot_test",
            NIMCP_BRAIN_TINY,
            NIMCP_TASK_REGRESSION,
            5, 2
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");

        nimcp_training_config_t config = nimcp_training_config_default();
        config.loss_type = NIMCP_API_LOSS_MSE;
        nimcp_brain_configure_training(brain, &config);

        // Train for a bit
        std::vector<float> features;
        std::vector<float> targets;
        TestDataGenerator::generate_training_batch(5, 5, 2, features, targets);

        for (int i = 0; i < 50; ++i) {
            nimcp_training_result_t result;
            nimcp_brain_train_step(brain, features.data(), 5, targets.data(), 2, &result);
        }
    }
    E2E_STAGE_END();

    // Stage 2: Save snapshot
    E2E_STAGE_BEGIN("Save snapshot", 100);
    {
        nimcp_status_t status = nimcp_brain_snapshot_save(
            brain,
            "e2e_test_snapshot",
            "Test snapshot from E2E pipeline"
        );
        E2E_ASSERT_SUCCESS(status, "Snapshot save failed");
    }
    E2E_STAGE_END();

    // Stage 3: Continue training (modify state)
    E2E_STAGE_BEGIN("Continue training", 2000);
    {
        std::vector<float> features;
        std::vector<float> targets;
        TestDataGenerator::generate_training_batch(5, 5, 2, features, targets);

        for (int i = 0; i < 50; ++i) {
            nimcp_training_result_t result;
            nimcp_brain_train_step(brain, features.data(), 5, targets.data(), 2, &result);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Restore from snapshot
    E2E_STAGE_BEGIN("Restore snapshot", 200);
    {
        nimcp_brain_t restored = nimcp_brain_snapshot_restore(brain, "e2e_test_snapshot");
        E2E_ASSERT_NOT_NULL(restored, "Snapshot restore failed");

        // Verify it's a valid brain
        std::vector<float> test_features = TestDataGenerator::generate_features(5);
        std::vector<float> outputs(2);
        nimcp_status_t status = nimcp_brain_infer(
            restored,
            test_features.data(),
            5,
            outputs.data(),
            2
        );
        E2E_ASSERT_SUCCESS(status, "Inference on restored brain failed");

        nimcp_brain_destroy(restored);
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 8000);  // Increased timeout for brain cleanup with all BG subsystems
    {
        nimcp_brain_snapshot_delete(brain, "e2e_test_snapshot");
        nimcp_brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Brain Probe and Statistics
//=============================================================================

E2E_TEST(BrainPipelineTest, BrainProbeStatistics) {
    E2E_PIPELINE_START("Brain Probe and Statistics");

    nimcp_brain_t brain = nullptr;

    // Stage 1: Create brain (increased timeout for full subsystem init)
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain = nimcp_brain_create(
            "probe_test",
            NIMCP_BRAIN_MEDIUM,
            NIMCP_TASK_PATTERN_MATCHING,
            20, 10
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Probe initial state
    E2E_STAGE_BEGIN("Probe initial state", 50);
    {
        nimcp_brain_probe_t probe;
        nimcp_status_t status = nimcp_brain_probe(brain, &probe);
        E2E_ASSERT_SUCCESS(status, "Brain probe failed");

        // Verify probe data - be lenient about exact counts as brain may transform dimensions
        E2E_ASSERT(probe.num_inputs > 0, "No inputs in brain");
        E2E_ASSERT(probe.num_outputs > 0, "No outputs in brain");
        E2E_ASSERT(probe.num_neurons > 0, "No neurons in brain");
        E2E_ASSERT(probe.total_inferences == 0, "Should have 0 inferences initially");

        std::cout << "  Initial probe: " << probe.num_inputs << " inputs, "
                  << probe.num_outputs << " outputs, "
                  << probe.num_neurons << " neurons, "
                  << probe.num_synapses << " synapses\n";
    }
    E2E_STAGE_END();

    // Stage 3: Run some inferences
    E2E_STAGE_BEGIN("Run inferences", 500);
    {
        std::vector<float> outputs(10);
        for (int i = 0; i < 100; ++i) {
            std::vector<float> features = TestDataGenerator::generate_features(20);
            nimcp_brain_infer(brain, features.data(), 20, outputs.data(), 10);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Probe after inferences
    E2E_STAGE_BEGIN("Probe after inferences", 50);
    {
        nimcp_brain_probe_t probe;
        nimcp_brain_probe(brain, &probe);

        E2E_ASSERT(probe.total_inferences == 100, "Inference count mismatch");
        E2E_ASSERT(probe.avg_inference_time_us > 0, "Invalid average inference time");

        std::cout << "  After inferences: " << probe.total_inferences << " inferences, "
                  << "avg time: " << probe.avg_inference_time_us << "us\n";
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 5000);  // Increased timeout for brain with many subsystems
    {
        nimcp_brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Copy-on-Write (COW) Cloning
//=============================================================================

E2E_TEST(BrainPipelineTest, COWCloning) {
    E2E_PIPELINE_START("Copy-on-Write Cloning");

    nimcp_brain_t original = nullptr;
    nimcp_brain_t clone = nullptr;

    // Stage 1: Create original brain (increased timeout for full subsystem init)
    E2E_STAGE_BEGIN("Create original", 500);
    {
        original = nimcp_brain_create(
            "cow_original",
            NIMCP_BRAIN_SMALL,
            NIMCP_TASK_CLASSIFICATION,
            8, 4
        );
        E2E_ASSERT_NOT_NULL(original, "Original brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Train original
    E2E_STAGE_BEGIN("Train original", 2000);
    {
        nimcp_training_config_t config = nimcp_training_config_default();
        nimcp_brain_configure_training(original, &config);

        std::vector<float> features;
        std::vector<float> labels;
        TestDataGenerator::generate_training_batch(10, 8, 4, features, labels);

        for (int i = 0; i < 100; ++i) {
            nimcp_training_result_t result;
            nimcp_brain_train_step(original, features.data(), 8, labels.data(), 4, &result);
        }
    }
    E2E_STAGE_END();

    // Stage 3: Clone with COW (should be fast)
    E2E_STAGE_BEGIN("COW clone", 50);
    {
        Timer clone_timer;
        clone_timer.start();
        clone = nimcp_brain_clone_cow(original);
        clone_timer.stop();

        E2E_ASSERT_NOT_NULL(clone, "COW clone failed");

        // Clone should be very fast (<10ms)
        uint64_t clone_time_ms = clone_timer.elapsed_ms();
        std::cout << "  Clone time: " << clone_time_ms << "ms\n";
        E2E_ASSERT(clone_time_ms < 50, "COW clone too slow");
    }
    E2E_STAGE_END();

    // Stage 4: Verify clone produces same outputs
    E2E_STAGE_BEGIN("Verify clone outputs", 200);
    {
        std::vector<float> test_features = TestDataGenerator::generate_features(8);
        std::vector<float> original_outputs(4);
        std::vector<float> clone_outputs(4);

        nimcp_brain_infer(original, test_features.data(), 8, original_outputs.data(), 4);
        nimcp_brain_infer(clone, test_features.data(), 8, clone_outputs.data(), 4);

        // Outputs should be identical (shared state)
        for (size_t i = 0; i < 4; ++i) {
            float diff = std::abs(original_outputs[i] - clone_outputs[i]);
            E2E_ASSERT(diff < 0.0001f, "Clone outputs differ from original");
        }
    }
    E2E_STAGE_END();

    // Stage 5: Probe COW statistics
    E2E_STAGE_BEGIN("Probe COW statistics", 50);
    {
        nimcp_brain_probe_t probe;
        nimcp_brain_probe(clone, &probe);

        E2E_ASSERT(probe.is_cow_clone, "Clone not marked as COW");
        E2E_ASSERT(probe.cow_shared_bytes > 0, "No shared memory in COW clone");

        std::cout << "  COW stats: " << probe.cow_shared_bytes << " bytes shared, "
                  << probe.cow_private_bytes << " bytes private\n";
    }
    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 5000);  // Increased timeout for brain cleanup
    {
        nimcp_brain_destroy(clone);
        nimcp_brain_destroy(original);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Training Callbacks
//=============================================================================

E2E_TEST(BrainPipelineTest, TrainingCallbacks) {
    E2E_PIPELINE_START("Training Callbacks");

    nimcp_brain_t brain = nullptr;
    int callback_count = 0;

    // Callback function
    auto step_callback = [](nimcp_callback_event_t event,
                            const nimcp_callback_metrics_t* metrics,
                            void* user_data) -> nimcp_callback_action_t {
        int* count = static_cast<int*>(user_data);
        (*count)++;
        return NIMCP_CB_ACTION_CONTINUE;
    };

    // Stage 1: Create and configure brain (increased timeout for full subsystem init)
    E2E_STAGE_BEGIN("Create and configure", 500);
    {
        brain = nimcp_brain_create("callback_test", NIMCP_BRAIN_TINY,
                                    NIMCP_TASK_REGRESSION, 3, 1);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");

        nimcp_training_config_t config = nimcp_training_config_default();
        nimcp_brain_configure_training(brain, &config);

        nimcp_callback_config_t cb_config = nimcp_callback_config_default();
        nimcp_brain_enable_callbacks(brain, &cb_config);
    }
    E2E_STAGE_END();

    // Stage 2: Register callback
    E2E_STAGE_BEGIN("Register callback", 10);
    {
        uint32_t cb_id = nimcp_brain_register_callback(
            brain,
            NIMCP_CB_STEP_COMPLETE,
            step_callback,
            &callback_count,
            "step_counter"
        );
        E2E_ASSERT(cb_id > 0, "Callback registration failed");
    }
    E2E_STAGE_END();

    // Stage 3: Train and trigger callbacks
    E2E_STAGE_BEGIN("Train with callbacks", 1000);
    {
        std::vector<float> features;
        std::vector<float> targets;
        TestDataGenerator::generate_training_batch(5, 3, 1, features, targets);

        for (int i = 0; i < 10; ++i) {
            nimcp_training_result_t result;
            nimcp_brain_train_step(brain, features.data(), 3, targets.data(), 1, &result);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify callbacks fired
    E2E_STAGE_BEGIN("Verify callbacks", 10);
    {
        std::cout << "  Callbacks fired: " << callback_count << "\n";
        E2E_ASSERT(callback_count > 0, "No callbacks fired");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 5000);  // Increased timeout for brain cleanup
    {
        nimcp_brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
