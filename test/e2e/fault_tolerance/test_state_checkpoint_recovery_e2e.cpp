/**
 * @file test_state_checkpoint_recovery_e2e.cpp
 * @brief End-to-end tests for state checkpoint and recovery system
 *
 * WHAT: E2E tests for complete state checkpoint/recovery workflows
 * WHY:  Verify the entire recovery pipeline works from failure to restoration
 * HOW:  Simulate realistic failure scenarios and verify full recovery
 *
 * PHASE 8: System-Wide Health Integration
 *
 * @author NIMCP Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <random>

extern "C" {
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/fault_tolerance/nimcp_module_recovery.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief E2E state checkpoint/recovery test fixture
 *
 * WHAT: Provides complete fault tolerance infrastructure for E2E tests
 * WHY:  Test realistic recovery scenarios with multiple modules
 * HOW:  Create both managers with real modules, simulate failures, verify recovery
 */
class StateCheckpointRecoveryE2ETest : public ::testing::Test {
protected:
    nimcp_state_manager_t* state_manager;
    nimcp_module_recovery_manager_t* recovery_manager;

    /* Real modules */
    stdp_synapse_t synapses[10];
    astrocyte_network_t* network;

    static constexpr int NUM_SYNAPSES = 10;
    static constexpr int NETWORK_SIZE = 20;

    void SetUp() override {
        state_manager = nimcp_state_manager_create();
        recovery_manager = nimcp_module_recovery_manager_create();
        network = astrocyte_network_create(NETWORK_SIZE);

        /* Initialize synapses with varied weights */
        for (int i = 0; i < NUM_SYNAPSES; i++) {
            stdp_synapse_init(&synapses[i]);
            synapses[i].weight = 0.1f * (i + 1);  /* 0.1 to 1.0 */
        }

        /* Register modules with both managers */
        const nimcp_module_state_ops_t* stdp_state_ops = stdp_get_state_ops();
        const nimcp_module_state_ops_t* astro_state_ops = astrocyte_network_get_state_ops();
        const nimcp_module_recovery_ops_t* stdp_recovery_ops = nimcp_stdp_get_recovery_ops();
        const nimcp_module_recovery_ops_t* astro_recovery_ops = nimcp_astrocyte_get_recovery_ops();

        for (int i = 0; i < NUM_SYNAPSES; i++) {
            char name[32];
            snprintf(name, sizeof(name), "stdp_%d", i);
            nimcp_state_manager_register(state_manager, name, stdp_state_ops, &synapses[i]);
            nimcp_module_recovery_register(recovery_manager, name, stdp_recovery_ops, &synapses[i]);
        }

        nimcp_state_manager_register(state_manager, "astrocyte_network", astro_state_ops, network);
        nimcp_module_recovery_register(recovery_manager, "astrocyte_network", astro_recovery_ops, network);
    }

    void TearDown() override {
        if (state_manager) {
            nimcp_state_manager_destroy(state_manager);
            state_manager = nullptr;
        }
        if (recovery_manager) {
            nimcp_module_recovery_manager_destroy(recovery_manager);
            recovery_manager = nullptr;
        }
        if (network) {
            astrocyte_network_destroy(network);
            network = nullptr;
        }
    }

    /**
     * @brief Simulate training by modifying synapse weights
     */
    void simulateTraining(int epochs) {
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> delta(-0.02f, 0.05f);

        for (int epoch = 0; epoch < epochs; epoch++) {
            for (int i = 0; i < NUM_SYNAPSES; i++) {
                float dw = delta(rng);
                synapses[i].weight += dw;
                /* Clamp to valid range */
                if (synapses[i].weight < 0.0f) synapses[i].weight = 0.0f;
                if (synapses[i].weight > 1.0f) synapses[i].weight = 1.0f;

                /* Update traces */
                synapses[i].pre_trace *= 0.9f;
                synapses[i].post_trace *= 0.9f;
                synapses[i].pre_trace += 0.1f;
            }
        }
    }

    /**
     * @brief Corrupt random modules to simulate failures
     */
    void corruptRandomModules(int count) {
        std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> synapse_dist(0, NUM_SYNAPSES - 1);

        for (int i = 0; i < count; i++) {
            int idx = synapse_dist(rng);
            synapses[idx].weight = NAN;
            synapses[idx].pre_trace = INFINITY;
        }
    }

    /**
     * @brief Verify all modules are valid
     */
    bool allModulesValid() {
        int valid = nimcp_state_manager_validate_all(state_manager);
        return valid == (NUM_SYNAPSES + 1);  /* +1 for astrocyte network */
    }
};

//=============================================================================
// E2E Scenario: Checkpoint During Training
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, CheckpointDuringTraining) {
    /* Simulate training for 50 epochs */
    simulateTraining(50);

    /* Record weights */
    std::vector<float> weights_before(NUM_SYNAPSES);
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        weights_before[i] = synapses[i].weight;
    }

    /* Checkpoint */
    size_t checkpoint_size = 0;
    int result = nimcp_state_manager_checkpoint_all(state_manager, nullptr, &checkpoint_size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(checkpoint_size, 0u);

    std::vector<uint8_t> checkpoint(checkpoint_size);
    size_t written = checkpoint_size;
    result = nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &written);
    EXPECT_EQ(result, 0);

    /* Continue training */
    simulateTraining(50);

    /* Weights should have changed */
    bool weights_changed = false;
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        if (std::abs(synapses[i].weight - weights_before[i]) > 0.001f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    /* Restore from checkpoint */
    result = nimcp_state_manager_restore_all(state_manager, checkpoint.data(), written);
    EXPECT_EQ(result, 0);

    /* Weights should match checkpoint */
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        EXPECT_NEAR(synapses[i].weight, weights_before[i], 0.001f)
            << "Synapse " << i << " weight mismatch";
    }
}

//=============================================================================
// E2E Scenario: Failure Detection and Recovery
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, FailureDetectionAndRecovery) {
    /* Train to establish good state */
    simulateTraining(100);

    /* Checkpoint */
    size_t checkpoint_size = 0;
    nimcp_state_manager_checkpoint_all(state_manager, nullptr, &checkpoint_size);
    std::vector<uint8_t> checkpoint(checkpoint_size);
    size_t written = checkpoint_size;
    nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &written);

    /* Verify all valid before corruption */
    EXPECT_TRUE(allModulesValid());

    /* Corrupt some modules */
    corruptRandomModules(3);

    /* Validate detects failures */
    int valid_count = nimcp_state_manager_validate_all(state_manager);
    EXPECT_LT(valid_count, NUM_SYNAPSES + 1);

    /* Health check should show degraded health */
    float avg_health = nimcp_module_recovery_check_all_health(recovery_manager);
    EXPECT_LT(avg_health, 0.9f);

    /* Recover unhealthy modules */
    recovery_manager->health_threshold = 0.7f;
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(recovery_manager);
    EXPECT_GE(recovered, 1);

    /* All modules should now be valid */
    EXPECT_TRUE(allModulesValid());
}

//=============================================================================
// E2E Scenario: Graduated Recovery Escalation
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, GraduatedRecoveryEscalation) {
    /* Severely corrupt first synapse */
    synapses[0].weight = NAN;
    synapses[0].pre_trace = INFINITY;
    synapses[0].post_trace = NAN;
    synapses[0].learning_rate = -1.0f;

    /* Enable auto-escalation */
    recovery_manager->auto_escalate = true;
    recovery_manager->max_escalation_level = NIMCP_MODULE_RECOVERY_FULL;

    /* Attempt recovery starting from LIGHT */
    nimcp_module_recovery_result_t result = nimcp_module_recovery_attempt(
        recovery_manager, "stdp_0", NIMCP_MODULE_RECOVERY_LIGHT);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

    /* Verify recovery */
    EXPECT_TRUE(std::isfinite(synapses[0].weight));
    EXPECT_TRUE(std::isfinite(synapses[0].pre_trace));
    EXPECT_TRUE(std::isfinite(synapses[0].post_trace));
    EXPECT_GT(synapses[0].learning_rate, 0.0f);

    /* Module should validate */
    int valid = nimcp_state_manager_validate_module(state_manager, "stdp_0");
    EXPECT_EQ(valid, 0);
}

//=============================================================================
// E2E Scenario: Checkpoint to File and Restore
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, CheckpointToFileAndRestore) {
    /* Train */
    simulateTraining(100);

    /* Record state */
    std::vector<float> original_weights(NUM_SYNAPSES);
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        original_weights[i] = synapses[i].weight;
    }

    /* Checkpoint to memory */
    size_t checkpoint_size = 0;
    nimcp_state_manager_checkpoint_all(state_manager, nullptr, &checkpoint_size);
    std::vector<uint8_t> checkpoint(checkpoint_size);
    size_t written = checkpoint_size;
    nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &written);

    /* Write to temporary file */
    std::string temp_file = "/tmp/nimcp_e2e_checkpoint_test.bin";
    {
        std::ofstream ofs(temp_file, std::ios::binary);
        ASSERT_TRUE(ofs.is_open());
        ofs.write(reinterpret_cast<const char*>(checkpoint.data()), written);
    }

    /* Corrupt all state */
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        synapses[i].weight = 0.0f;
        synapses[i].pre_trace = 0.0f;
    }

    /* Read from file */
    std::vector<uint8_t> loaded_checkpoint;
    {
        std::ifstream ifs(temp_file, std::ios::binary | std::ios::ate);
        ASSERT_TRUE(ifs.is_open());
        size_t file_size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        loaded_checkpoint.resize(file_size);
        ifs.read(reinterpret_cast<char*>(loaded_checkpoint.data()), file_size);
    }

    /* Restore from file */
    int result = nimcp_state_manager_restore_all(
        state_manager, loaded_checkpoint.data(), loaded_checkpoint.size());
    EXPECT_EQ(result, 0);

    /* Verify restoration */
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        EXPECT_NEAR(synapses[i].weight, original_weights[i], 0.001f);
    }

    /* Clean up */
    std::remove(temp_file.c_str());
}

//=============================================================================
// E2E Scenario: Periodic Checkpointing
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, PeriodicCheckpointing) {
    std::vector<std::vector<uint8_t>> checkpoints;
    std::vector<std::vector<float>> weight_history;

    /* Simulate training with periodic checkpoints */
    for (int checkpoint_idx = 0; checkpoint_idx < 5; checkpoint_idx++) {
        /* Train for some epochs */
        simulateTraining(20);

        /* Record weights */
        std::vector<float> current_weights(NUM_SYNAPSES);
        for (int i = 0; i < NUM_SYNAPSES; i++) {
            current_weights[i] = synapses[i].weight;
        }
        weight_history.push_back(current_weights);

        /* Checkpoint */
        size_t size = 0;
        nimcp_state_manager_checkpoint_all(state_manager, nullptr, &size);
        std::vector<uint8_t> checkpoint(size);
        size_t written = size;
        nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &written);
        checkpoints.push_back(checkpoint);
    }

    /* Verify we can restore to any checkpoint */
    for (int restore_idx = 0; restore_idx < 5; restore_idx++) {
        /* Restore */
        int result = nimcp_state_manager_restore_all(
            state_manager, checkpoints[restore_idx].data(), checkpoints[restore_idx].size());
        EXPECT_EQ(result, 0);

        /* Verify weights match */
        for (int i = 0; i < NUM_SYNAPSES; i++) {
            EXPECT_NEAR(synapses[i].weight, weight_history[restore_idx][i], 0.001f)
                << "Checkpoint " << restore_idx << ", Synapse " << i;
        }
    }
}

//=============================================================================
// E2E Scenario: Module Isolation and System Degradation
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, ModuleIsolationAndDegradation) {
    /* Simulate catastrophic failure in some modules */
    for (int i = 0; i < 3; i++) {
        synapses[i].weight = NAN;
        synapses[i].pre_trace = INFINITY;
    }

    /* Isolate failed modules */
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stdp_%d", i);
        nimcp_module_recovery_isolate(recovery_manager, name);
    }

    /* System should continue operating with remaining modules */
    float remaining_health = 0.0f;
    int healthy_count = 0;
    for (int i = 3; i < NUM_SYNAPSES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stdp_%d", i);
        float health;
        if (nimcp_module_recovery_check_health(recovery_manager, name, &health) == 0) {
            remaining_health += health;
            healthy_count++;
        }
    }
    remaining_health /= healthy_count;
    EXPECT_GT(remaining_health, 0.8f);

    /* Verify isolated modules are skipped in recovery */
    recovery_manager->health_threshold = 0.9f;
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(recovery_manager);
    /* Should only recover non-isolated unhealthy modules */

    /* Restore isolated modules */
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stdp_%d", i);
        nimcp_module_recovery_restore(recovery_manager, name);

        /* Now recover */
        nimcp_module_recovery_attempt(recovery_manager, name, NIMCP_MODULE_RECOVERY_FULL);
    }

    /* All modules should now be healthy */
    EXPECT_TRUE(allModulesValid());
}

//=============================================================================
// E2E Scenario: Complete Failure Recovery Workflow
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, CompleteFailureRecoveryWorkflow) {
    /* Phase 1: Normal operation with periodic checkpoints */
    std::vector<uint8_t> last_good_checkpoint;

    for (int phase = 0; phase < 3; phase++) {
        simulateTraining(30);

        /* Checkpoint */
        size_t size = 0;
        nimcp_state_manager_checkpoint_all(state_manager, nullptr, &size);
        last_good_checkpoint.resize(size);
        size_t written = size;
        nimcp_state_manager_checkpoint_all(state_manager, last_good_checkpoint.data(), &written);

        /* Verify health */
        float health = nimcp_module_recovery_check_all_health(recovery_manager);
        EXPECT_GT(health, 0.8f) << "Phase " << phase << " health degraded";
    }

    /* Phase 2: Simulate catastrophic failure */
    for (int i = 0; i < NUM_SYNAPSES; i++) {
        synapses[i].weight = NAN;
        synapses[i].pre_trace = INFINITY;
    }

    /* Phase 3: Detect failure */
    int valid = nimcp_state_manager_validate_all(state_manager);
    EXPECT_EQ(valid, 1);  /* Only astrocyte should validate */

    float health = nimcp_module_recovery_check_all_health(recovery_manager);
    EXPECT_LT(health, 0.5f);

    /* Phase 4: Attempt recovery */
    recovery_manager->health_threshold = 0.7f;
    recovery_manager->auto_escalate = true;
    int recovered = nimcp_module_recovery_attempt_all_unhealthy(recovery_manager);
    EXPECT_GE(recovered, NUM_SYNAPSES);

    /* Phase 5: Verify system restored to working state */
    EXPECT_TRUE(allModulesValid());
    health = nimcp_module_recovery_check_all_health(recovery_manager);
    EXPECT_GT(health, 0.7f);

    /* Phase 6: Resume normal operation */
    simulateTraining(10);
    EXPECT_TRUE(allModulesValid());
}

//=============================================================================
// E2E Scenario: Statistics and Monitoring
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, StatisticsAndMonitoring) {
    /* Perform various operations and verify statistics */

    /* Initial stats */
    nimcp_state_manager_stats_t state_stats;
    nimcp_module_recovery_stats_t recovery_stats;

    nimcp_state_manager_get_stats(state_manager, &state_stats);
    EXPECT_EQ(state_stats.module_count, (uint32_t)(NUM_SYNAPSES + 1));

    /* Perform checkpoints */
    for (int i = 0; i < 5; i++) {
        simulateTraining(10);

        size_t size = 0;
        nimcp_state_manager_checkpoint_all(state_manager, nullptr, &size);
        std::vector<uint8_t> checkpoint(size);
        size_t written = size;
        nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &written);
    }

    /* Perform validations */
    for (int i = 0; i < 3; i++) {
        nimcp_state_manager_validate_all(state_manager);
    }

    /* Perform recoveries */
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stdp_%d", i);
        nimcp_module_recovery_attempt(recovery_manager, name, NIMCP_MODULE_RECOVERY_LIGHT);
    }

    /* Verify stats updated */
    nimcp_state_manager_get_stats(state_manager, &state_stats);
    EXPECT_GE(state_stats.total_checkpoints, 5u);
    EXPECT_GE(state_stats.total_validations, 3u);

    nimcp_module_recovery_get_stats(recovery_manager, &recovery_stats);
    EXPECT_GE(recovery_stats.total_recoveries, 3u);
}

//=============================================================================
// E2E Scenario: Long Running Stability
//=============================================================================

TEST_F(StateCheckpointRecoveryE2ETest, LongRunningStability) {
    auto start_time = std::chrono::steady_clock::now();
    int iterations = 0;
    int recoveries = 0;

    /* Run for at least 1 second, simulating long-running operation */
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(1)) {
        /* Train */
        simulateTraining(5);

        /* Periodic validation */
        if (iterations % 10 == 0) {
            nimcp_state_manager_validate_all(state_manager);
        }

        /* Periodic checkpoint */
        if (iterations % 20 == 0) {
            size_t size = 0;
            nimcp_state_manager_checkpoint_all(state_manager, nullptr, &size);
            std::vector<uint8_t> checkpoint(size);
            size_t written = size;
            nimcp_state_manager_checkpoint_all(state_manager, checkpoint.data(), &written);
        }

        /* Occasional random corruption and recovery */
        if (iterations % 50 == 0) {
            int idx = iterations % NUM_SYNAPSES;
            synapses[idx].pre_trace = NAN;

            char name[32];
            snprintf(name, sizeof(name), "stdp_%d", idx);
            nimcp_module_recovery_attempt(recovery_manager, name, NIMCP_MODULE_RECOVERY_LIGHT);
            recoveries++;
        }

        iterations++;
    }

    /* Verify system is still healthy after extended operation */
    EXPECT_TRUE(allModulesValid());
    EXPECT_GT(iterations, 100) << "Should have run many iterations";
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
