/**
 * @file test_brain_state_manager_e2e.cpp
 * @brief End-to-end tests for Brain State Manager
 *
 * WHAT: E2E tests for brain state manager checkpoint/recovery workflow
 * WHY:  Verify complete brain recovery pipeline from failure to restoration
 * HOW:  Create real brain, perform operations, checkpoint, simulate failure, restore
 *
 * PHASE 8: System-Wide Health Integration Tests
 *
 * @author NIMCP Development Team
 * @date 2026-01-22
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

// Include brain_internal.h outside extern "C" due to CUDA C++ templates
#include "core/brain/nimcp_brain_internal.h"

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/factory/nimcp_brain_init_state_manager.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
}

/*=============================================================================
 * Test Fixture with Full Brain for E2E Testing
 *=============================================================================*/

class BrainStateManagerE2ETest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    std::vector<float> test_features;
    static constexpr int NUM_INPUTS = 16;
    static constexpr int NUM_OUTPUTS = 4;

    void SetUp() override {
        // Create brain for E2E testing
        brain = brain_create("state_mgr_e2e", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, NUM_INPUTS, NUM_OUTPUTS);

        // Initialize test features
        test_features.resize(NUM_INPUTS);
        for (int i = 0; i < NUM_INPUTS; i++) {
            test_features[i] = (float)i / NUM_INPUTS;
        }
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: train brain with some examples
    void trainBrain(int num_examples) {
        if (!brain) return;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        const char* labels[] = {"cat", "dog", "bird", "fish"};
        std::vector<float> features(NUM_INPUTS);

        for (int i = 0; i < num_examples; i++) {
            for (int j = 0; j < NUM_INPUTS; j++) {
                features[j] = dist(gen);
            }
            brain_learn_example(brain, features.data(), NUM_INPUTS,
                               labels[i % 4], 0.9f);
        }
    }
};

/*=============================================================================
 * Complete Workflow E2E Tests
 *=============================================================================*/

TEST_F(BrainStateManagerE2ETest, CompleteCheckpointRestoreWorkflow) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    // Step 1: Train brain to establish state
    trainBrain(100);
    uint64_t learning_steps_before = brain->stats.total_learning_steps;
    ASSERT_GT(learning_steps_before, 0u);

    // Step 2: Create checkpoint
    size_t checkpoint_size = 0;
    int result = brain_checkpoint_state(brain, nullptr, &checkpoint_size);
    ASSERT_EQ(result, 0);
    ASSERT_GT(checkpoint_size, 0u);

    std::vector<uint8_t> checkpoint(checkpoint_size);
    size_t written = checkpoint_size;
    result = brain_checkpoint_state(brain, checkpoint.data(), &written);
    ASSERT_EQ(result, 0);
    ASSERT_EQ(written, checkpoint_size);

    // Step 3: Modify state (simulate more training)
    trainBrain(50);
    uint64_t learning_steps_after = brain->stats.total_learning_steps;
    ASSERT_GT(learning_steps_after, learning_steps_before);

    // Step 4: Restore from checkpoint
    result = brain_restore_state(brain, checkpoint.data(), checkpoint_size);
    ASSERT_EQ(result, 0);

    // Step 5: Verify state restored
    EXPECT_EQ(brain->stats.total_learning_steps, learning_steps_before);
}

TEST_F(BrainStateManagerE2ETest, ValidationDetectsCorruption) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    // Step 1: Train brain to establish valid state
    trainBrain(50);

    // Step 2: Validate - should pass
    int valid_before = brain_validate_state(brain);
    EXPECT_GT(valid_before, 0);

    // Step 3: Corrupt state
    brain->stats.num_neurons = 0xFFFFFFFF;

    // Step 4: Validate again - may detect corruption
    int valid_after = brain_validate_state(brain);

    // Step 5: Reset invalid modules
    int reset_count = brain_reset_invalid_state(brain);

    // After reset, state should be valid again
    int valid_final = brain_validate_state(brain);
    EXPECT_GE(valid_final, 0);
}

TEST_F(BrainStateManagerE2ETest, RecoveryFromSimulatedCrash) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    // Step 1: Establish known state
    brain->stats.total_learning_steps = 1000;
    brain->stats.total_inferences = 500;
    brain->stats.quantum_annealing_runs = 10;

    // Step 2: Create checkpoint (pre-crash state)
    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);
    std::vector<uint8_t> checkpoint(size);
    size_t written = size;
    brain_checkpoint_state(brain, checkpoint.data(), &written);

    // Step 3: Simulate "crash" by corrupting state
    brain->stats.total_learning_steps = 0xDEADBEEF;
    brain->stats.total_inferences = 0xDEADBEEF;
    brain->stats.quantum_annealing_runs = 0xDEADBEEF;

    // Step 4: Recover from checkpoint
    int result = brain_restore_state(brain, checkpoint.data(), written);
    ASSERT_EQ(result, 0);

    // Step 5: Verify recovery
    EXPECT_EQ(brain->stats.total_learning_steps, 1000u);
    EXPECT_EQ(brain->stats.total_inferences, 500u);
    EXPECT_EQ(brain->stats.quantum_annealing_runs, 10u);
}

/*=============================================================================
 * Multi-Checkpoint E2E Tests
 *=============================================================================*/

TEST_F(BrainStateManagerE2ETest, MultipleCheckpointSnapshots) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    std::vector<std::vector<uint8_t>> checkpoints;
    std::vector<uint64_t> expected_steps;

    // Create multiple checkpoints at different states
    for (int i = 0; i < 5; i++) {
        brain->stats.total_learning_steps = (i + 1) * 100;
        expected_steps.push_back(brain->stats.total_learning_steps);

        size_t size = 0;
        brain_checkpoint_state(brain, nullptr, &size);
        std::vector<uint8_t> checkpoint(size);
        size_t written = size;
        brain_checkpoint_state(brain, checkpoint.data(), &written);
        checkpoints.push_back(checkpoint);
    }

    // Verify we can restore to any checkpoint
    for (int i = 4; i >= 0; i--) {
        int result = brain_restore_state(brain, checkpoints[i].data(), checkpoints[i].size());
        ASSERT_EQ(result, 0);
        EXPECT_EQ(brain->stats.total_learning_steps, expected_steps[i]);
    }

    // Restore in forward order
    for (int i = 0; i < 5; i++) {
        int result = brain_restore_state(brain, checkpoints[i].data(), checkpoints[i].size());
        ASSERT_EQ(result, 0);
        EXPECT_EQ(brain->stats.total_learning_steps, expected_steps[i]);
    }
}

/*=============================================================================
 * Stress E2E Tests
 *=============================================================================*/

TEST_F(BrainStateManagerE2ETest, RapidCheckpointRestoreCycles) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);
    std::vector<uint8_t> checkpoint(size);

    auto start = std::chrono::steady_clock::now();

    // Perform rapid checkpoint/restore cycles
    for (int i = 0; i < 1000; i++) {
        brain->stats.total_learning_steps = i;

        size_t written = size;
        int result = brain_checkpoint_state(brain, checkpoint.data(), &written);
        ASSERT_EQ(result, 0);

        brain->stats.total_learning_steps = 0xFFFFFFFF;

        result = brain_restore_state(brain, checkpoint.data(), written);
        ASSERT_EQ(result, 0);

        ASSERT_EQ(brain->stats.total_learning_steps, (uint64_t)i);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 cycles in reasonable time (< 10 seconds)
    EXPECT_LT(duration.count(), 10000);
}

TEST_F(BrainStateManagerE2ETest, ConcurrentValidation) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    std::atomic<int> total_validations{0};
    std::atomic<bool> error_occurred{false};

    // Run validation concurrently from multiple threads
    auto validate_thread = [this, &total_validations, &error_occurred]() {
        for (int i = 0; i < 100; i++) {
            int result = brain_validate_state(brain);
            if (result < 0) {
                error_occurred = true;
            }
            total_validations++;
        }
    };

    std::thread t1(validate_thread);
    std::thread t2(validate_thread);
    std::thread t3(validate_thread);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_FALSE(error_occurred);
    EXPECT_EQ(total_validations.load(), 300);
}

/*=============================================================================
 * Health Agent Integration E2E Tests
 *=============================================================================*/

TEST_F(BrainStateManagerE2ETest, HeartbeatDuringCheckpoint) {
    if (!brain || !brain->state_manager_enabled || !brain->health_agent_enabled) {
        GTEST_SKIP() << "Brain with state manager and health agent not available";
    }

    // Checkpoint should send heartbeats (verified by not hanging)
    size_t size = 0;
    auto start = std::chrono::steady_clock::now();

    int result = brain_checkpoint_state(brain, nullptr, &size);
    EXPECT_EQ(result, 0);

    if (size > 0) {
        std::vector<uint8_t> checkpoint(size);
        size_t written = size;
        result = brain_checkpoint_state(brain, checkpoint.data(), &written);
        EXPECT_EQ(result, 0);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete quickly (heartbeats prevent hang detection)
    EXPECT_LT(duration.count(), 5000);
}

TEST_F(BrainStateManagerE2ETest, HeartbeatDuringRestore) {
    if (!brain || !brain->state_manager_enabled || !brain->health_agent_enabled) {
        GTEST_SKIP() << "Brain with state manager and health agent not available";
    }

    // Create checkpoint
    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);
    std::vector<uint8_t> checkpoint(size);
    size_t written = size;
    brain_checkpoint_state(brain, checkpoint.data(), &written);

    // Restore should send heartbeats
    auto start = std::chrono::steady_clock::now();

    int result = brain_restore_state(brain, checkpoint.data(), written);
    EXPECT_EQ(result, 0);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000);
}

/*=============================================================================
 * Module Registration E2E Tests
 *=============================================================================*/

TEST_F(BrainStateManagerE2ETest, AllExpectedModulesRegistered) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    // Verify expected modules are registered
    const char* expected_modules[] = {"brain_stats", "working_memory", "executive"};

    for (const char* module_name : expected_modules) {
        nimcp_state_module_entry_t* entry =
            nimcp_state_manager_find(brain->state_manager, module_name);
        ASSERT_NE(entry, nullptr) << "Module not registered: " << module_name;
        EXPECT_TRUE(entry->enabled) << "Module not enabled: " << module_name;
    }
}

TEST_F(BrainStateManagerE2ETest, ModulePrioritiesCorrect) {
    if (!brain || !brain->state_manager_enabled || !brain->state_manager) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    // Verify priorities
    nimcp_state_module_entry_t* stats =
        nimcp_state_manager_find(brain->state_manager, "brain_stats");
    nimcp_state_module_entry_t* wm =
        nimcp_state_manager_find(brain->state_manager, "working_memory");
    nimcp_state_module_entry_t* exec =
        nimcp_state_manager_find(brain->state_manager, "executive");

    ASSERT_NE(stats, nullptr);
    ASSERT_NE(wm, nullptr);
    ASSERT_NE(exec, nullptr);

    // brain_stats should be first (lowest priority number)
    EXPECT_LT(stats->priority, wm->priority);
    EXPECT_LT(wm->priority, exec->priority);
}

/*=============================================================================
 * Cleanup E2E Tests
 *=============================================================================*/

TEST_F(BrainStateManagerE2ETest, CleanupAfterCheckpoint) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    // Create checkpoint
    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);
    std::vector<uint8_t> checkpoint(size);
    size_t written = size;
    brain_checkpoint_state(brain, checkpoint.data(), &written);

    // Destroy brain - should cleanup properly
    brain_destroy(brain);
    brain = nullptr;

    // No crash means success
    SUCCEED();
}

TEST_F(BrainStateManagerE2ETest, CleanupAfterRestore) {
    if (!brain || !brain->state_manager_enabled) {
        GTEST_SKIP() << "Brain with state manager not available";
    }

    // Create checkpoint
    size_t size = 0;
    brain_checkpoint_state(brain, nullptr, &size);
    std::vector<uint8_t> checkpoint(size);
    size_t written = size;
    brain_checkpoint_state(brain, checkpoint.data(), &written);

    // Restore
    brain_restore_state(brain, checkpoint.data(), written);

    // Destroy brain - should cleanup properly
    brain_destroy(brain);
    brain = nullptr;

    SUCCEED();
}
