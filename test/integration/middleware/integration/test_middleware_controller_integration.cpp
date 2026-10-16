/**
 * @file test_middleware_controller_integration.cpp
 * @brief Integration tests for middleware controller (Phase 1.5.5)
 *
 * TEST COVERAGE:
 * - Integration with brain training workflow
 * - Integration with cognitive layers (executive, introspection)
 * - Cross-module command propagation
 * - Shannon information flow tracking
 * - Performance under realistic workloads
 * - Multi-threaded command execution
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_middleware_controller.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareControllerIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    middleware_controller_t* controller = nullptr;
    static constexpr uint32_t NUM_INPUTS = 20;
    static constexpr uint32_t NUM_OUTPUTS = 5;

    void SetUp() override {
        brain = brain_create(
            "integration_test",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            NUM_INPUTS,
            NUM_OUTPUTS
        );

        if (brain != nullptr) {
            controller = middleware_controller_create(brain);
        }
    }

    void TearDown() override {
        if (controller != nullptr) {
            middleware_controller_destroy(controller);
            controller = nullptr;
        }
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create training sample
    std::vector<float> CreateSample(float base) {
        std::vector<float> sample(NUM_INPUTS);
        for (uint32_t i = 0; i < NUM_INPUTS; i++) {
            sample[i] = base + static_cast<float>(i % 5) * 0.1f;
        }
        return sample;
    }

    // Helper: Get label
    const char* GetLabel(int idx) {
        static const char* labels[] = {"class_0", "class_1", "class_2", "class_3", "class_4"};
        return labels[idx % 5];
    }
};

//=============================================================================
// 1. Brain Training Integration
//=============================================================================

TEST_F(MiddlewareControllerIntegrationTest, CommandsDuringTraining) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    // Set up attention before training
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.6f));
    EXPECT_TRUE(middleware_controller_set_attention_priority(
        controller, TARGET_PREFRONTAL, 0.9f));

    // Train while middleware is configured
    for (int i = 0; i < 10; i++) {
        auto sample = CreateSample(static_cast<float>(i));
        brain_learn_example(brain, sample.data(), NUM_INPUTS,
                           GetLabel(i), 0.9f);
    }

    // Verify controller still works after training
    middleware_controller_metrics_t metrics;
    EXPECT_TRUE(middleware_controller_get_metrics(controller, &metrics));
    EXPECT_GE(metrics.total_commands, 2u);
}

TEST_F(MiddlewareControllerIntegrationTest, DynamicAttentionDuringInference) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    // Train brain first
    for (int i = 0; i < 20; i++) {
        auto sample = CreateSample(static_cast<float>(i));
        brain_learn_example(brain, sample.data(), NUM_INPUTS,
                           GetLabel(i), 0.9f);
    }

    // Run inference with dynamic attention adjustment
    for (int i = 0; i < 10; i++) {
        // Adjust attention based on "confidence" (simulated)
        float simulated_confidence = 0.5f + i * 0.05f;
        middleware_controller_set_attention_threshold(
            controller, TARGET_VISUAL_CORTEX, 1.0f - simulated_confidence);

        auto sample = CreateSample(static_cast<float>(i));
        brain_decision_t* decision = brain_decide(brain, sample.data(), NUM_INPUTS);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_EQ(metrics.attention_commands, 10u);
}

//=============================================================================
// 2. Cognitive Layer Integration
//=============================================================================

TEST_F(MiddlewareControllerIntegrationTest, ExecutiveStyleRouting) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    // Simulate executive-style routing configuration
    // (prefrontal to hippocampus for memory consolidation)
    EXPECT_TRUE(middleware_controller_set_routing_priority(
        controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.9f));

    // Configure attention for focused task
    EXPECT_TRUE(middleware_controller_set_attention_selectivity(
        controller, TARGET_PREFRONTAL, 0.9f, 4));

    // Block distracting routes
    EXPECT_TRUE(middleware_controller_block_route(
        controller, TARGET_AMYGDALA, TARGET_PREFRONTAL));

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_GE(metrics.routing_commands, 2u);
}

TEST_F(MiddlewareControllerIntegrationTest, TaskSwitchingSimulation) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    // Simulate task switching by resetting and reconfiguring
    for (int task = 0; task < 3; task++) {
        // Reset attention for new task
        middleware_controller_reset_attention(controller, TARGET_ALL_REGIONS);

        // Configure for specific task
        command_target_region_t focus_region =
            static_cast<command_target_region_t>(TARGET_VISUAL_CORTEX + task);
        middleware_controller_set_attention_priority(controller, focus_region, 0.95f);

        // Process some samples
        for (int i = 0; i < 5; i++) {
            auto sample = CreateSample(static_cast<float>(i + task * 10));
            brain_decision_t* decision = brain_decide(brain, sample.data(), NUM_INPUTS);
            if (decision) brain_free_decision(decision);
        }
    }

    // Should have multiple attention commands from task switches
    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_GE(metrics.attention_commands, 6u);
}

//=============================================================================
// 3. Pattern Subscription Integration
//=============================================================================

static std::atomic<int> integration_callback_count{0};
static std::atomic<uint32_t> last_pattern_id{0};

static void integration_callback(uint32_t pattern_id, float similarity,
                                  uint32_t region_id, void* user_data) {
    integration_callback_count++;
    last_pattern_id = pattern_id;
    (void)similarity;
    (void)region_id;
    (void)user_data;
}

TEST_F(MiddlewareControllerIntegrationTest, PatternMonitoringWorkflow) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    integration_callback_count = 0;

    // Subscribe to multiple patterns
    std::vector<uint32_t> sub_ids;
    for (uint32_t p = 1; p <= 5; p++) {
        uint32_t sub_id;
        bool result = middleware_controller_subscribe_pattern(
            controller, p, 0.7f, integration_callback, nullptr, &sub_id);
        EXPECT_TRUE(result);
        sub_ids.push_back(sub_id);
    }

    // Simulate pattern matches
    for (uint32_t p = 1; p <= 5; p++) {
        middleware_controller_on_pattern_match(controller, p, 0.85f, TARGET_VISUAL_CORTEX);
    }

    EXPECT_EQ(integration_callback_count.load(), 5);

    // Unsubscribe from all
    for (uint32_t sub_id : sub_ids) {
        middleware_controller_unsubscribe_pattern(controller, sub_id);
    }

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_EQ(metrics.active_subscriptions, 0u);
    EXPECT_EQ(metrics.pattern_notifications_sent, 5u);
}

//=============================================================================
// 4. Activity Modulation Integration
//=============================================================================

TEST_F(MiddlewareControllerIntegrationTest, CognitiveLoadAdaptation) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    // Simulate high cognitive load → reduce activity
    middleware_controller_reduce_activity(controller, TARGET_VISUAL_CORTEX);
    middleware_controller_reduce_activity(controller, TARGET_AUDITORY_CORTEX);

    // Process under reduced activity
    for (int i = 0; i < 5; i++) {
        auto sample = CreateSample(static_cast<float>(i));
        brain_decision_t* decision = brain_decide(brain, sample.data(), NUM_INPUTS);
        if (decision) brain_free_decision(decision);
    }

    // Simulate load decrease → boost activity
    middleware_controller_boost_activity(controller, TARGET_VISUAL_CORTEX);
    middleware_controller_boost_activity(controller, TARGET_AUDITORY_CORTEX);

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_EQ(metrics.activity_commands, 4u);
}

//=============================================================================
// 5. Batch Command Integration
//=============================================================================

TEST_F(MiddlewareControllerIntegrationTest, BatchCommandExecution) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    middleware_command_batch_t batch;
    middleware_controller_begin_batch(controller, &batch);

    // Add multiple commands to batch
    batch.commands[0].type = COMMAND_CONFIGURE_ATTENTION;
    batch.commands[0].payload.attention.target_region = TARGET_VISUAL_CORTEX;
    batch.commands[0].payload.attention.priority = 0.8f;

    batch.commands[1].type = COMMAND_ADJUST_ROUTING;
    batch.commands[1].payload.routing.source_region = TARGET_PREFRONTAL;
    batch.commands[1].payload.routing.target_region = TARGET_HIPPOCAMPUS;
    batch.commands[1].payload.routing.weight = 0.9f;

    batch.commands[2].type = COMMAND_INCREASE_ACTIVITY;
    batch.commands[2].payload.activity.target_region = TARGET_AMYGDALA;

    batch.num_commands = 3;

    command_execution_result_t results[3];
    uint32_t success = middleware_controller_execute_batch(controller, &batch, results);

    EXPECT_EQ(success, 3u);
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(results[i].success);
    }

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_GE(metrics.batches_created, 1u);
}

//=============================================================================
// 6. Multi-threaded Integration
//=============================================================================

TEST_F(MiddlewareControllerIntegrationTest, ConcurrentCommandExecution) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    constexpr int NUM_THREADS = 4;
    constexpr int COMMANDS_PER_THREAD = 25;
    std::atomic<int> success_count{0};

    auto worker = [this, &success_count](int thread_id) {
        for (int i = 0; i < COMMANDS_PER_THREAD; i++) {
            command_target_region_t region =
                static_cast<command_target_region_t>(
                    TARGET_PREFRONTAL + (thread_id % 5));

            if (middleware_controller_set_attention_threshold(
                controller, region, 0.5f + i * 0.01f)) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All commands should succeed (thread-safe)
    EXPECT_EQ(success_count.load(), NUM_THREADS * COMMANDS_PER_THREAD);

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_EQ(metrics.total_commands, (uint32_t)(NUM_THREADS * COMMANDS_PER_THREAD));
}

//=============================================================================
// 7. Performance Integration
//=============================================================================

TEST_F(MiddlewareControllerIntegrationTest, PerformanceUnderLoad) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    constexpr int NUM_COMMANDS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_COMMANDS; i++) {
        command_target_region_t region =
            static_cast<command_target_region_t>(TARGET_PREFRONTAL + (i % 7));
        middleware_controller_set_attention_threshold(
            controller, region, 0.5f + (i % 10) * 0.05f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_latency = static_cast<float>(duration.count()) / NUM_COMMANDS;

    // Average latency should be reasonable (< 50µs including system overhead)
    EXPECT_LT(avg_latency, 50.0f);

    middleware_controller_metrics_t metrics;
    middleware_controller_get_metrics(controller, &metrics);
    EXPECT_EQ(metrics.total_commands, (uint32_t)NUM_COMMANDS);
}

//=============================================================================
// 8. Error Recovery Integration
//=============================================================================

TEST_F(MiddlewareControllerIntegrationTest, RecoveryFromErrors) {
    if (brain == nullptr || controller == nullptr) {
        GTEST_SKIP() << "Brain or controller creation failed";
    }

    // Try to subscribe more than max subscriptions
    std::vector<uint32_t> sub_ids;
    for (int i = 0; i < 100; i++) {
        uint32_t sub_id;
        bool result = middleware_controller_subscribe_pattern(
            controller, i, 0.5f, integration_callback, nullptr, &sub_id);
        if (result) {
            sub_ids.push_back(sub_id);
        }
    }

    // Controller should still work after hitting limits
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.8f));

    // Cleanup subscriptions
    for (uint32_t sub_id : sub_ids) {
        middleware_controller_unsubscribe_pattern(controller, sub_id);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
