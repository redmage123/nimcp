/**
 * @file test_health_module_integration.cpp
 * @brief Integration tests for Health Agent and Module integration
 *
 * WHAT: Tests for health agent integration with training/cognitive modules
 * WHY:  Phase 8 requires modules to send heartbeats during long operations
 * HOW:  Test end-to-end heartbeat flow from modules to health agent
 *
 * @author NIMCP Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

// Forward declarations for module health agent setters
void dist_set_health_agent(struct nimcp_health_agent* agent);
void adv_set_health_agent(struct nimcp_health_agent* agent);
void hpo_set_health_agent(struct nimcp_health_agent* agent);
void meta_set_health_agent(struct nimcp_health_agent* agent);
void executive_set_health_agent(struct nimcp_health_agent* agent);
void qa_set_health_agent(struct nimcp_health_agent* agent);
}

/**
 * @brief Test fixture for Health Module Integration tests
 */
class HealthModuleIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.heartbeat_interval_ms = 50;
        config.watchdog_timeout_ms = 500;
        config.enable_auto_recovery = false;  // Disable for testing
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        // Disconnect modules from health agent
        dist_set_health_agent(nullptr);
        adv_set_health_agent(nullptr);
        hpo_set_health_agent(nullptr);
        meta_set_health_agent(nullptr);
        executive_set_health_agent(nullptr);
        qa_set_health_agent(nullptr);

        if (agent) {
            nimcp_health_agent_stop(agent);
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/**
 * @test Verify distributed training module heartbeat integration
 */
TEST_F(HealthModuleIntegrationTest, DistributedTrainingHeartbeat) {
    // Connect module to health agent
    dist_set_health_agent(agent);

    // Simulate distributed training heartbeat (normally called from ring_all_reduce)
    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "ring_all_reduce",
                                        static_cast<float>(i + 1) / 10.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 10u);
}

/**
 * @test Verify adversarial training module heartbeat integration
 */
TEST_F(HealthModuleIntegrationTest, AdversarialTrainingHeartbeat) {
    // Connect module to health agent
    adv_set_health_agent(agent);

    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    // Simulate PGD attack steps
    for (int step = 0; step < 20; step++) {
        nimcp_health_agent_heartbeat_ex(agent, "pgd_attack",
                                        static_cast<float>(step + 1) / 20.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 20u);
}

/**
 * @test Verify hyperparameter optimization module heartbeat integration
 */
TEST_F(HealthModuleIntegrationTest, HPOHeartbeat) {
    hpo_set_health_agent(agent);

    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    // Simulate HPO trials
    for (int trial = 0; trial < 50; trial++) {
        nimcp_health_agent_heartbeat_ex(agent, "hpo_trial",
                                        static_cast<float>(trial + 1) / 50.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 50u);
}

/**
 * @test Verify meta-learning module heartbeat integration
 */
TEST_F(HealthModuleIntegrationTest, MetaLearningHeartbeat) {
    meta_set_health_agent(agent);

    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    // Simulate MAML inner loop
    for (int step = 0; step < 5; step++) {
        nimcp_health_agent_heartbeat_ex(agent, "maml_inner",
                                        static_cast<float>(step + 1) / 5.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 5u);
}

/**
 * @test Verify executive module heartbeat integration
 */
TEST_F(HealthModuleIntegrationTest, ExecutiveHeartbeat) {
    executive_set_health_agent(agent);

    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    // Simulate planning steps
    nimcp_health_agent_heartbeat_ex(agent, "mcts_planning", 0.25f);
    nimcp_health_agent_heartbeat_ex(agent, "mcts_planning", 0.50f);
    nimcp_health_agent_heartbeat_ex(agent, "mcts_planning", 0.75f);
    nimcp_health_agent_heartbeat_ex(agent, "mcts_planning", 1.0f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 4u);
}

/**
 * @test Verify quantum annealing module heartbeat integration
 */
TEST_F(HealthModuleIntegrationTest, QuantumAnnealingHeartbeat) {
    qa_set_health_agent(agent);

    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    // Simulate annealing iterations
    for (int iter = 0; iter < 100; iter++) {
        nimcp_health_agent_heartbeat_ex(agent, "annealing",
                                        static_cast<float>(iter + 1) / 100.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 100u);
}

/**
 * @test Verify multiple modules can share single health agent
 */
TEST_F(HealthModuleIntegrationTest, MultipleModulesSharedAgent) {
    // Connect all modules to same agent
    dist_set_health_agent(agent);
    adv_set_health_agent(agent);
    hpo_set_health_agent(agent);
    meta_set_health_agent(agent);
    executive_set_health_agent(agent);
    qa_set_health_agent(agent);

    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    // Send heartbeats from different "modules"
    nimcp_health_agent_heartbeat_ex(agent, "dist_training", 0.5f);
    nimcp_health_agent_heartbeat_ex(agent, "adv_training", 0.5f);
    nimcp_health_agent_heartbeat_ex(agent, "hpo", 0.5f);
    nimcp_health_agent_heartbeat_ex(agent, "meta_learning", 0.5f);
    nimcp_health_agent_heartbeat_ex(agent, "executive", 0.5f);
    nimcp_health_agent_heartbeat_ex(agent, "quantum_annealing", 0.5f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 6u);
}

/**
 * @test Verify health agent with running thread
 */
TEST_F(HealthModuleIntegrationTest, AgentThreadMonitoring) {
    // Start the health agent thread
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    // Connect a module
    dist_set_health_agent(agent);

    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    // Simulate continuous operation
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "continuous_op",
                                        static_cast<float>(i + 1) / 20.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_GE(stats.heartbeats_received, 20u);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

/**
 * @test Verify concurrent heartbeats from multiple modules
 */
TEST_F(HealthModuleIntegrationTest, ConcurrentModuleHeartbeats) {
    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    std::atomic<int> total_sent{0};

    // Simulate multiple modules sending heartbeats concurrently
    auto send_heartbeats = [this, &total_sent](const char* module_name, int count) {
        for (int i = 0; i < count; i++) {
            nimcp_health_agent_heartbeat_ex(agent, module_name,
                                            static_cast<float>(i + 1) / count);
            total_sent++;
        }
    };

    std::thread t1(send_heartbeats, "dist_training", 50);
    std::thread t2(send_heartbeats, "adv_training", 50);
    std::thread t3(send_heartbeats, "hpo", 50);
    std::thread t4(send_heartbeats, "meta_learning", 50);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);

    EXPECT_EQ(stats.heartbeats_received, 200u);
}

/**
 * @test Verify state manager integration with health agent
 */
TEST_F(HealthModuleIntegrationTest, StateManagerIntegration) {
    nimcp_state_manager_t* state_mgr = nimcp_state_manager_create();
    ASSERT_NE(state_mgr, nullptr);

    // Create a simple module state
    struct {
        int counter;
        bool healthy;
    } module_state = {0, true};

    nimcp_module_state_ops_t ops = {
        .serialize = [](void* state, uint8_t* buf, size_t* sz) -> int {
            if (!buf) { *sz = 8; return 0; }
            if (*sz < 8) return -2;
            memcpy(buf, state, 8);
            *sz = 8;
            return 0;
        },
        .deserialize = [](void* state, const uint8_t* buf, size_t sz) -> int {
            if (sz < 8) return -1;
            memcpy(state, buf, 8);
            return 0;
        },
        .validate = [](void* state) -> int {
            auto* s = static_cast<decltype(module_state)*>(state);
            return s->healthy ? 0 : -1;
        },
        .reset = [](void* state) -> int {
            auto* s = static_cast<decltype(module_state)*>(state);
            s->counter = 0;
            s->healthy = true;
            return 0;
        },
        .get_size = [](void*) -> size_t { return 8; }
    };

    // Register module
    int result = nimcp_state_manager_register(state_mgr, "test_module", &ops, &module_state);
    EXPECT_EQ(result, 0);

    // Simulate work with heartbeats
    extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                 const char* operation,
                                                 float progress);

    for (int i = 0; i < 10; i++) {
        module_state.counter++;
        nimcp_health_agent_heartbeat_ex(agent, "work", static_cast<float>(i + 1) / 10.0f);
    }

    // Checkpoint state
    size_t checkpoint_size = 0;
    nimcp_state_manager_checkpoint_all(state_mgr, nullptr, &checkpoint_size);
    std::vector<uint8_t> checkpoint(checkpoint_size);
    nimcp_state_manager_checkpoint_all(state_mgr, checkpoint.data(), &checkpoint_size);

    // Verify
    EXPECT_EQ(module_state.counter, 10);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);

    nimcp_state_manager_destroy(state_mgr);
}

/**
 * @brief Main entry point
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
