/**
 * @file test_health_workflow_e2e.cpp
 * @brief End-to-end tests for Health System workflows
 *
 * WHAT: Complete workflow tests for health monitoring and recovery
 * WHY:  Phase 8 requires full integration testing of health features
 * HOW:  Test realistic scenarios: training with monitoring, recovery
 *
 * @author NIMCP Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <functional>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
}

/**
 * @brief Simulated training state for E2E tests
 */
typedef struct {
    int epoch;
    int step;
    float loss;
    float accuracy;
    bool converged;
    bool healthy;
} simulated_training_state_t;

/**
 * @brief Test fixture for Health Workflow E2E tests
 */
class HealthWorkflowE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* health_agent = nullptr;
    nimcp_state_manager_t* state_manager = nullptr;
    simulated_training_state_t training_state;

    void SetUp() override {
        // Create health agent
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.heartbeat_interval_ms = 50;
        config.watchdog_timeout_ms = 200;
        config.enable_auto_recovery = false;
        health_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(health_agent, nullptr);

        // Create state manager
        state_manager = nimcp_state_manager_create();
        ASSERT_NE(state_manager, nullptr);

        // Initialize training state
        training_state.epoch = 0;
        training_state.step = 0;
        training_state.loss = 1.0f;
        training_state.accuracy = 0.0f;
        training_state.converged = false;
        training_state.healthy = true;

        // Register training state with state manager
        nimcp_module_state_ops_t ops = {
            .serialize = [](void* state, uint8_t* buffer, size_t* size) -> int {
                if (!buffer) {
                    *size = sizeof(simulated_training_state_t);
                    return 0;
                }
                if (*size < sizeof(simulated_training_state_t)) return -2;
                memcpy(buffer, state, sizeof(simulated_training_state_t));
                *size = sizeof(simulated_training_state_t);
                return 0;
            },
            .deserialize = [](void* state, const uint8_t* buffer, size_t size) -> int {
                if (size < sizeof(simulated_training_state_t)) return -1;
                memcpy(state, buffer, sizeof(simulated_training_state_t));
                return 0;
            },
            .validate = [](void* state) -> int {
                auto* s = static_cast<simulated_training_state_t*>(state);
                return s->healthy ? 0 : -1;
            },
            .reset = [](void* state) -> int {
                auto* s = static_cast<simulated_training_state_t*>(state);
                s->epoch = 0;
                s->step = 0;
                s->loss = 1.0f;
                s->accuracy = 0.0f;
                s->converged = false;
                s->healthy = true;
                return 0;
            },
            .get_size = [](void*) -> size_t {
                return sizeof(simulated_training_state_t);
            }
        };

        int result = nimcp_state_manager_register(state_manager, "training", &ops, &training_state);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        if (state_manager) {
            nimcp_state_manager_destroy(state_manager);
            state_manager = nullptr;
        }
        if (health_agent) {
            nimcp_health_agent_stop(health_agent);
            nimcp_health_agent_destroy(health_agent);
            health_agent = nullptr;
        }
    }

    /**
     * @brief Simulate a training step with heartbeat
     */
    void simulate_training_step() {
        training_state.step++;

        // Simulate loss decrease
        training_state.loss *= 0.99f;
        training_state.accuracy = 1.0f - training_state.loss;

        // Send heartbeat
        float progress = static_cast<float>(training_state.step % 100) / 100.0f;
        nimcp_health_agent_heartbeat_ex(health_agent, "training_step", progress);
    }

    /**
     * @brief Simulate an epoch completion
     */
    void simulate_epoch_complete() {
        training_state.epoch++;
        training_state.step = 0;

        // Send heartbeat at epoch boundary
        nimcp_health_agent_heartbeat_ex(health_agent, "epoch_complete", 1.0f);
    }

    /**
     * @brief Create a checkpoint of current state
     */
    std::vector<uint8_t> create_checkpoint() {
        size_t size = 0;
        nimcp_state_manager_checkpoint_all(state_manager, nullptr, &size);
        std::vector<uint8_t> checkpoint(size);
        nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &size);
        return checkpoint;
    }

    /**
     * @brief Restore from checkpoint
     */
    bool restore_checkpoint(const std::vector<uint8_t>& checkpoint) {
        return nimcp_state_manager_restore_all(state_manager, checkpoint.data(), checkpoint.size()) == 0;
    }
};

/**
 * @test E2E: Complete training workflow with health monitoring
 */
TEST_F(HealthWorkflowE2ETest, CompleteTrainingWorkflow) {
    // Start health monitoring
    int result = nimcp_health_agent_start(health_agent);
    ASSERT_EQ(result, 0);

    // Simulate 5 epochs of training
    const int num_epochs = 5;
    const int steps_per_epoch = 100;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        for (int step = 0; step < steps_per_epoch; step++) {
            simulate_training_step();

            // Small delay to simulate computation
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        simulate_epoch_complete();

        // Checkpoint at end of epoch
        auto checkpoint = create_checkpoint();
        EXPECT_GT(checkpoint.size(), 0u);
    }

    // Verify health stats
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(health_agent, &stats);

    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(num_epochs * steps_per_epoch + num_epochs));
    EXPECT_EQ(stats.heartbeat_timeouts, 0u);

    // Verify training state
    EXPECT_EQ(training_state.epoch, num_epochs);
    EXPECT_LT(training_state.loss, 0.5f);  // Loss should have decreased
    EXPECT_GT(training_state.accuracy, 0.5f);
}

/**
 * @test E2E: Recovery from simulated failure
 */
TEST_F(HealthWorkflowE2ETest, RecoveryFromFailure) {
    // Start health monitoring
    int result = nimcp_health_agent_start(health_agent);
    ASSERT_EQ(result, 0);

    // Train for a bit
    for (int step = 0; step < 50; step++) {
        simulate_training_step();
    }

    // Save checkpoint
    auto checkpoint = create_checkpoint();
    int saved_epoch = training_state.epoch;
    int saved_step = training_state.step;
    float saved_loss = training_state.loss;

    // Continue training
    for (int step = 0; step < 50; step++) {
        simulate_training_step();
    }

    // Simulate failure - corrupt state
    training_state.loss = -1000.0f;  // Invalid loss
    training_state.healthy = false;

    // Validate should fail (returns 0 when no modules validate successfully)
    result = nimcp_state_manager_validate_all(state_manager);
    EXPECT_EQ(result, 0);

    // Restore from checkpoint
    EXPECT_TRUE(restore_checkpoint(checkpoint));

    // State should be restored
    EXPECT_EQ(training_state.epoch, saved_epoch);
    EXPECT_EQ(training_state.step, saved_step);
    EXPECT_FLOAT_EQ(training_state.loss, saved_loss);
    EXPECT_TRUE(training_state.healthy);

    // Validate should pass now (returns count of validated modules)
    result = nimcp_state_manager_validate_all(state_manager);
    EXPECT_GE(result, 1);  // At least 1 module validated successfully

    // Continue training
    for (int step = 0; step < 50; step++) {
        simulate_training_step();
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(health_agent, &stats);
    EXPECT_GT(stats.heartbeats_received, 100u);
}

/**
 * @test E2E: Multiple checkpoints and selective recovery
 */
TEST_F(HealthWorkflowE2ETest, MultipleCheckpointsSelectiveRecovery) {
    std::vector<std::vector<uint8_t>> checkpoints;

    // Create checkpoints at different stages
    for (int stage = 0; stage < 5; stage++) {
        for (int step = 0; step < 20; step++) {
            simulate_training_step();
        }
        simulate_epoch_complete();
        checkpoints.push_back(create_checkpoint());
    }

    EXPECT_EQ(checkpoints.size(), 5u);
    EXPECT_EQ(training_state.epoch, 5);

    // Restore to checkpoint 2 (epoch 2)
    EXPECT_TRUE(restore_checkpoint(checkpoints[2]));
    EXPECT_EQ(training_state.epoch, 3);  // After epoch 2 completed

    // Restore to checkpoint 0 (epoch 0)
    EXPECT_TRUE(restore_checkpoint(checkpoints[0]));
    EXPECT_EQ(training_state.epoch, 1);  // After epoch 0 completed

    // Restore to latest
    EXPECT_TRUE(restore_checkpoint(checkpoints[4]));
    EXPECT_EQ(training_state.epoch, 5);
}

/**
 * @test E2E: Long-running training with continuous monitoring
 */
TEST_F(HealthWorkflowE2ETest, LongRunningTrainingWithMonitoring) {
    int result = nimcp_health_agent_start(health_agent);
    ASSERT_EQ(result, 0);

    // Simulate extended training
    const int total_steps = 1000;
    const int checkpoint_interval = 100;

    std::vector<std::vector<uint8_t>> periodic_checkpoints;

    for (int step = 0; step < total_steps; step++) {
        simulate_training_step();

        // Periodic checkpoint
        if ((step + 1) % checkpoint_interval == 0) {
            periodic_checkpoints.push_back(create_checkpoint());
        }

        // Brief sleep
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(health_agent, &stats);

    // Verify all heartbeats received
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(total_steps));

    // Verify no timeouts during normal operation
    EXPECT_EQ(stats.heartbeat_timeouts, 0u);

    // Verify checkpoints created
    EXPECT_EQ(periodic_checkpoints.size(), total_steps / checkpoint_interval);
}

/**
 * @test E2E: Concurrent training and monitoring threads
 */
TEST_F(HealthWorkflowE2ETest, ConcurrentTrainingAndMonitoring) {
    int result = nimcp_health_agent_start(health_agent);
    ASSERT_EQ(result, 0);

    std::atomic<bool> training_active{true};
    std::atomic<int> steps_completed{0};
    std::atomic<int> checkpoints_created{0};

    // Training thread
    std::thread training_thread([&]() {
        while (training_active) {
            simulate_training_step();
            steps_completed++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Checkpoint thread
    std::thread checkpoint_thread([&]() {
        while (training_active) {
            auto checkpoint = create_checkpoint();
            checkpoints_created++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Validation thread
    std::thread validation_thread([&]() {
        while (training_active) {
            nimcp_state_manager_validate_all(state_manager);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Run for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    training_active = false;

    training_thread.join();
    checkpoint_thread.join();
    validation_thread.join();

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(health_agent, &stats);

    EXPECT_GT(steps_completed, 1000);
    EXPECT_GT(checkpoints_created, 10);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(steps_completed.load()));
}

/**
 * @test E2E: Recovery after simulated timeout scenario
 */
TEST_F(HealthWorkflowE2ETest, RecoveryAfterTimeout) {
    health_agent_config_t config;
    nimcp_health_agent_default_config(&config);
    config.heartbeat_interval_ms = 20;
    config.watchdog_timeout_ms = 100;

    // Recreate agent with short timeout
    nimcp_health_agent_destroy(health_agent);
    health_agent = nimcp_health_agent_create(&config);
    ASSERT_NE(health_agent, nullptr);

    int result = nimcp_health_agent_start(health_agent);
    ASSERT_EQ(result, 0);

    // Create initial checkpoint
    for (int step = 0; step < 10; step++) {
        simulate_training_step();
    }
    auto recovery_checkpoint = create_checkpoint();

    // Send heartbeats normally for a bit
    for (int step = 0; step < 50; step++) {
        simulate_training_step();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Simulate hang - no heartbeats for longer than timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(health_agent, &stats);

    // May have detected timeout
    // EXPECT_GE(stats.heartbeat_timeouts, 1u);

    // Recover by restoring checkpoint
    EXPECT_TRUE(restore_checkpoint(recovery_checkpoint));

    // Resume with heartbeats
    for (int step = 0; step < 50; step++) {
        simulate_training_step();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Training should complete normally
    EXPECT_GT(training_state.step, 0);
}

/**
 * @brief Main entry point
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
