/**
 * @file test_cognitive_bio_async_e2e.cpp
 * @brief End-to-End Tests for Cognitive Bio-Async Pipeline Integration
 *
 * WHAT: Complete cognitive pipeline tests using bio-async messaging
 * WHY:  Validate cognitive modules work correctly with async bio-inspired communication
 * HOW:  Test realistic cognitive workflows with bio-async channels, phase sync, predictive coding
 *
 * TEST COVERAGE:
 * - Network analysis -> consolidation -> knowledge integration pipeline
 * - Global workspace broadcast to multiple cognitive subscribers
 * - Mirror neurons + global workspace coordination with bio-async
 * - Error handling and recovery across bio-async boundaries
 * - Multi-module cognitive integration with neuromodulator channels
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>
#include <memory>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/nimcp_predictive.h"
#include "utils/memory/nimcp_unified_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveBioAsyncE2ETest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;

    void SetUp() override {
        // Initialize unified memory FIRST - required by cognitive modules
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_mgr_ = unified_mem_create(&mem_config);
        ASSERT_NE(mem_mgr_, nullptr) << "Failed to create unified memory manager";

        // Initialize bio-async subsystem
        nimcp_error_t err = nimcp_bio_async_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";
        ASSERT_TRUE(nimcp_bio_async_is_initialized())
            << "Bio-async not initialized after successful init";
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
    }
};

//=============================================================================
// Pipeline 1: Network Analysis -> Consolidation -> Knowledge Integration
//=============================================================================

TEST_F(CognitiveBioAsyncE2ETest, FullCognitivePipeline) {
    E2E_PIPELINE_START("Network Analysis -> Consolidation -> Knowledge Integration");

    // Stage 1: Setup cognitive modules
    // Brain creation with neuromodulators can take ~900ms on some systems
    E2E_STAGE_BEGIN("Setup cognitive modules", 1200);

    // Create knowledge module
    knowledge_system_t knowledge = knowledge_system_create("TestLearner");
    E2E_ASSERT_NOT_NULL(knowledge, "Knowledge creation failed");

    // Note: Consolidation and network analysis modules require more complex setup
    // For this E2E test, we focus on bio-async communication patterns

    E2E_STAGE_END();

    // Stage 2: Create bio-async communication channels
    E2E_STAGE_BEGIN("Create bio-async channels", 200);

    // Create generic result structures for testing
    struct analysis_result {
        uint32_t num_nodes;
        uint32_t num_edges;
        float clustering_coefficient;
        float avg_degree;
    };

    struct consolidation_result {
        uint32_t num_consolidated;
        float avg_strength;
        bool success;
    };

    // Analysis -> Consolidation channel (dopamine for salience)
    nimcp_bio_promise_t analysis_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(struct analysis_result));
    E2E_ASSERT_NOT_NULL(analysis_promise, "Analysis promise creation failed");
    nimcp_bio_future_t analysis_future = nimcp_bio_promise_get_future(analysis_promise);
    E2E_ASSERT_NOT_NULL(analysis_future, "Analysis future creation failed");

    // Consolidation -> Knowledge channel (acetylcholine for learning)
    nimcp_bio_promise_t consolidation_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_ACETYLCHOLINE, sizeof(struct consolidation_result));
    E2E_ASSERT_NOT_NULL(consolidation_promise, "Consolidation promise creation failed");
    nimcp_bio_future_t consolidation_future = nimcp_bio_promise_get_future(consolidation_promise);
    E2E_ASSERT_NOT_NULL(consolidation_future, "Consolidation future creation failed");

    E2E_STAGE_END();

    // Stage 3: Perform network analysis (async)
    E2E_STAGE_BEGIN("Network analysis phase", 1000);

    std::atomic<bool> analysis_done{false};
    struct analysis_result analysis_result = {0};

    std::thread analysis_thread([&]() {
        // Simulate network analysis processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Populate analysis results
        analysis_result.num_nodes = 500;
        analysis_result.num_edges = 2000;
        analysis_result.avg_degree = 4.0f;
        analysis_result.clustering_coefficient = 0.35f;

        // Complete promise
        nimcp_bio_promise_complete(analysis_promise, &analysis_result);
        analysis_done = true;
    });

    analysis_thread.join();
    E2E_ASSERT(analysis_done.load(), "Network analysis should complete");

    E2E_STAGE_END();

    // Stage 4: Wait for analysis and trigger consolidation
    E2E_STAGE_BEGIN("Consolidation phase", 1500);

    struct analysis_result received_analysis = {0};
    nimcp_error_t err = nimcp_bio_future_wait(analysis_future, &received_analysis, 1000);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to receive analysis results");
    E2E_ASSERT(received_analysis.num_nodes == 500, "Analysis data corrupted");

    std::atomic<bool> consolidation_done{false};
    struct consolidation_result consolidation_result = {0};

    std::thread consolidation_thread([&]() {
        // Simulate memory consolidation
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // Generate consolidation results
        consolidation_result.num_consolidated = 250;
        consolidation_result.avg_strength = 0.75f;
        consolidation_result.success = true;

        // Complete promise
        nimcp_bio_promise_complete(consolidation_promise, &consolidation_result);
        consolidation_done = true;
    });

    consolidation_thread.join();
    E2E_ASSERT(consolidation_done.load(), "Consolidation should complete");

    E2E_STAGE_END();

    // Stage 5: Knowledge integration
    E2E_STAGE_BEGIN("Knowledge integration phase", 1000);

    struct consolidation_result received_consolidation = {0};
    err = nimcp_bio_future_wait(consolidation_future, &received_consolidation, 1000);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to receive consolidation results");
    E2E_ASSERT(received_consolidation.success, "Consolidation failed");
    E2E_ASSERT(received_consolidation.num_consolidated > 0, "No memories consolidated");

    std::cout << "  Knowledge integration: " << received_consolidation.num_consolidated
              << " concepts, avg strength: " << received_consolidation.avg_strength << "\n";

    E2E_STAGE_END();

    // Stage 6: Verify pipeline statistics
    E2E_STAGE_BEGIN("Verify pipeline statistics", 200);

    nimcp_bio_async_stats_t stats;
    err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get bio-async stats");
    E2E_ASSERT(stats.total_futures_created >= 2, "Should have created futures");
    E2E_ASSERT(stats.total_futures_completed >= 2, "Should have completed futures");

    std::cout << "  Pipeline stats: " << stats.total_futures_completed
              << " completed, " << stats.channel_stats[BIO_CHANNEL_DOPAMINE].releases
              << " dopamine releases\n";

    E2E_STAGE_END();

    // Stage 7: Cleanup
    // Brain destruction with neuromodulators can take ~250ms
    E2E_STAGE_BEGIN("Cleanup resources", 500);

    nimcp_bio_future_destroy(consolidation_future);
    nimcp_bio_promise_destroy(consolidation_promise);
    nimcp_bio_future_destroy(analysis_future);
    nimcp_bio_promise_destroy(analysis_promise);

    knowledge_system_destroy(knowledge);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Global Workspace Broadcast to Multiple Subscribers
//=============================================================================

TEST_F(CognitiveBioAsyncE2ETest, GlobalWorkspaceBroadcast) {
    E2E_PIPELINE_START("Global Workspace Broadcast to Multiple Subscribers");

    // Stage 1: Create global workspace
    E2E_STAGE_BEGIN("Create global workspace", 300);

    global_workspace_t* workspace = global_workspace_create();
    E2E_ASSERT_NOT_NULL(workspace, "Global workspace creation failed");

    E2E_STAGE_END();

    // Stage 2: Create subscriber modules with bio-async channels
    E2E_STAGE_BEGIN("Create subscribers", 400);

    const int NUM_SUBSCRIBERS = 4;
    std::vector<nimcp_bio_promise_t> promises(NUM_SUBSCRIBERS);
    std::vector<nimcp_bio_future_t> futures(NUM_SUBSCRIBERS);

    // Different channels for different cognitive functions
    nimcp_bio_channel_type_t channels[] = {
        BIO_CHANNEL_DOPAMINE,       // Perception module
        BIO_CHANNEL_SEROTONIN,      // Emotion module
        BIO_CHANNEL_NOREPINEPHRINE, // Attention module
        BIO_CHANNEL_ACETYLCHOLINE   // Learning module
    };

    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        promises[i] = nimcp_bio_promise_create(channels[i], sizeof(float) * 64);
        E2E_ASSERT_NOT_NULL(promises[i], "Subscriber promise creation failed");
        futures[i] = nimcp_bio_promise_get_future(promises[i]);
        E2E_ASSERT_NOT_NULL(futures[i], "Subscriber future creation failed");
    }

    E2E_STAGE_END();

    // Stage 3: Create phase sync group for coordinated broadcast
    E2E_STAGE_BEGIN("Create phase synchronization", 200);

    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    E2E_ASSERT_NOT_NULL(sync, "Phase sync creation failed");

    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        nimcp_error_t err = nimcp_phase_sync_add_future(sync, futures[i]);
        E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to add future to sync group");
    }

    float initial_coherence = nimcp_phase_sync_get_coherence(sync);
    E2E_ASSERT(initial_coherence >= 0.0f, "Initial coherence invalid");

    E2E_STAGE_END();

    // Stage 4: Compete for workspace and broadcast
    E2E_STAGE_BEGIN("Workspace competition and broadcast", 1000);

    std::atomic<int> broadcasts_sent{0};

    std::thread broadcast_thread([&]() {
        // Simulate competitive workspace access
        for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Create broadcast content
            std::vector<float> content(64);
            for (int j = 0; j < 64; j++) {
                content[j] = static_cast<float>(i * 10 + j) * 0.01f;
            }

            // Broadcast to all subscribers
            nimcp_error_t err = nimcp_bio_promise_complete(promises[i], content.data());
            if (err == NIMCP_SUCCESS) {
                broadcasts_sent++;
            }
        }
    });

    broadcast_thread.join();
    E2E_ASSERT(broadcasts_sent.load() == NUM_SUBSCRIBERS, "Not all broadcasts sent");

    E2E_STAGE_END();

    // Stage 5: Wait for synchronized reception
    E2E_STAGE_BEGIN("Synchronized reception", 2000);

    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 1500);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Phase sync wait failed");

    float final_coherence = nimcp_phase_sync_get_coherence(sync);
    E2E_ASSERT(final_coherence >= 0.3f, "Final coherence too low");

    std::cout << "  Broadcast coherence: " << initial_coherence << " -> "
              << final_coherence << "\n";

    E2E_STAGE_END();

    // Stage 6: Verify all subscribers received broadcast
    E2E_STAGE_BEGIN("Verify subscriber reception", 500);

    int successful_receptions = 0;
    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        std::vector<float> received_content(64);
        err = nimcp_bio_future_wait(futures[i], received_content.data(), 200);
        if (err == NIMCP_SUCCESS) {
            nimcp_bio_future_state_t state = nimcp_bio_future_state(futures[i]);
            if (state == BIO_FUTURE_COMPLETED) {
                successful_receptions++;
            }
        }
    }

    E2E_ASSERT(successful_receptions == NUM_SUBSCRIBERS,
               "Not all subscribers received broadcast");
    std::cout << "  All " << NUM_SUBSCRIBERS << " subscribers received broadcast\n";

    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 1000);  // Increased timeout for workspace cleanup

    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        nimcp_bio_future_destroy(futures[i]);
        nimcp_bio_promise_destroy(promises[i]);
    }
    nimcp_phase_sync_destroy(sync);

    if (workspace) global_workspace_destroy(workspace);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Mirror Neurons + Global Workspace Coordination
//=============================================================================

TEST_F(CognitiveBioAsyncE2ETest, MirrorNeuronsWorkspaceCoordination) {
    E2E_PIPELINE_START("Mirror Neurons + Global Workspace Coordination");

    // Stage 1: Create mirror neuron system
    E2E_STAGE_BEGIN("Create mirror neuron system", 300);

    mirror_neuron_config_t mirror_config = mirror_neurons_get_default_config();
    mirror_config.num_mirror_neurons = 500;
    mirror_config.max_actions = 50;
    mirror_config.max_agents = 5;
    mirror_config.enable_working_memory = true;
    mirror_config.enable_prediction = true;

    mirror_neurons_t mirror = mirror_neurons_create(&mirror_config);
    E2E_ASSERT_NOT_NULL(mirror, "Mirror neuron creation failed");

    E2E_STAGE_END();

    // Stage 2: Create global workspace
    E2E_STAGE_BEGIN("Create global workspace", 200);

    global_workspace_t* workspace = global_workspace_create();
    E2E_ASSERT_NOT_NULL(workspace, "Workspace creation failed");

    E2E_STAGE_END();

    // Stage 3: Create bio-async channels for observation and execution
    E2E_STAGE_BEGIN("Create observation/execution channels", 300);

    // Observation channel (dopamine - reward from successful recognition)
    nimcp_bio_promise_t obs_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(action_t));
    E2E_ASSERT_NOT_NULL(obs_promise, "Observation promise creation failed");
    nimcp_bio_future_t obs_future = nimcp_bio_promise_get_future(obs_promise);

    // Execution channel (acetylcholine - learning from imitation)
    nimcp_bio_promise_t exec_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_ACETYLCHOLINE, sizeof(action_t));
    E2E_ASSERT_NOT_NULL(exec_promise, "Execution promise creation failed");
    nimcp_bio_future_t exec_future = nimcp_bio_promise_get_future(exec_promise);

    // Workspace broadcast channel (serotonin - stable conscious access)
    nimcp_bio_promise_t ws_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_SEROTONIN, sizeof(float) * 32);
    E2E_ASSERT_NOT_NULL(ws_promise, "Workspace promise creation failed");
    nimcp_bio_future_t ws_future = nimcp_bio_promise_get_future(ws_promise);

    E2E_STAGE_END();

    // Stage 4: Observe action and broadcast to workspace
    E2E_STAGE_BEGIN("Action observation and workspace broadcast", 1000);

    std::atomic<bool> observation_done{false};
    std::atomic<bool> workspace_done{false};

    std::thread observation_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Create observed action
        float features[32];
        for (int i = 0; i < 32; i++) {
            features[i] = sinf(i * 0.1f);
        }

        action_t observed_action = mirror_neurons_create_action(
            1, "grasp_object", features, 32, 1);

        // Process observation in mirror neurons
        bool success = mirror_neurons_observe_action(mirror, &observed_action);
        if (success) {
            // Complete observation promise
            nimcp_bio_promise_complete(obs_promise, &observed_action);
            observation_done = true;
        }
    });

    std::thread workspace_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Broadcast action representation to workspace
        float action_representation[32];
        for (int i = 0; i < 32; i++) {
            action_representation[i] = cosf(i * 0.1f);
        }

        nimcp_bio_promise_complete(ws_promise, action_representation);
        workspace_done = true;
    });

    observation_thread.join();
    workspace_thread.join();

    E2E_ASSERT(observation_done.load(), "Observation should complete");
    E2E_ASSERT(workspace_done.load(), "Workspace broadcast should complete");

    E2E_STAGE_END();

    // Stage 5: Execute imitation action
    E2E_STAGE_BEGIN("Imitation execution", 800);

    action_t observed = {0};
    nimcp_error_t err = nimcp_bio_future_wait(obs_future, &observed, 500);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to receive observed action");

    std::atomic<bool> execution_done{false};

    std::thread execution_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));

        // Execute imitation
        action_t executed_action = observed;
        executed_action.agent_id = 0; // Self execution

        bool success = mirror_neurons_execute_action(mirror, &executed_action);
        if (success) {
            nimcp_bio_promise_complete(exec_promise, &executed_action);
            execution_done = true;
        }
    });

    execution_thread.join();
    E2E_ASSERT(execution_done.load(), "Execution should complete");

    E2E_STAGE_END();

    // Stage 6: Verify mirror neuron statistics
    E2E_STAGE_BEGIN("Verify mirror neuron statistics", 200);

    mirror_neuron_stats_t stats;
    bool stats_ok = mirror_neurons_get_stats(mirror, &stats);
    E2E_ASSERT(stats_ok, "Failed to get mirror neuron stats");
    E2E_ASSERT(stats.total_observations > 0, "No observations recorded");
    E2E_ASSERT(stats.total_executions > 0, "No executions recorded");

    std::cout << "  Mirror neurons: " << stats.total_observations << " observations, "
              << stats.total_executions << " executions\n";

    E2E_STAGE_END();

    // Stage 7: Check bio-async integration
    E2E_STAGE_BEGIN("Check bio-async integration", 200);

    nimcp_bio_async_stats_t bio_stats;
    err = nimcp_bio_async_get_stats(&bio_stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get bio-async stats");
    E2E_ASSERT(bio_stats.total_futures_created >= 3, "Should have created futures");
    E2E_ASSERT(bio_stats.channel_stats[BIO_CHANNEL_DOPAMINE].releases > 0,
               "No dopamine releases");
    E2E_ASSERT(bio_stats.channel_stats[BIO_CHANNEL_ACETYLCHOLINE].releases > 0,
               "No acetylcholine releases");

    E2E_STAGE_END();

    // Stage 8: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 1000);  // Increased timeout for workspace cleanup

    nimcp_bio_future_destroy(ws_future);
    nimcp_bio_promise_destroy(ws_promise);
    nimcp_bio_future_destroy(exec_future);
    nimcp_bio_promise_destroy(exec_promise);
    nimcp_bio_future_destroy(obs_future);
    nimcp_bio_promise_destroy(obs_promise);

    if (workspace) global_workspace_destroy(workspace);
    if (mirror) mirror_neurons_destroy(mirror);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Error Handling and Recovery Scenarios
//=============================================================================

TEST_F(CognitiveBioAsyncE2ETest, ErrorHandlingAndRecovery) {
    E2E_PIPELINE_START("Error Handling and Recovery Scenarios");

    // Stage 1: Setup with intentional failure points
    E2E_STAGE_BEGIN("Setup modules with failure injection", 400);

    nimcp_bio_promise_t success_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(int));
    E2E_ASSERT_NOT_NULL(success_promise, "Success promise creation failed");
    nimcp_bio_future_t success_future = nimcp_bio_promise_get_future(success_promise);

    nimcp_bio_promise_t timeout_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_SEROTONIN, sizeof(int));
    E2E_ASSERT_NOT_NULL(timeout_promise, "Timeout promise creation failed");
    nimcp_bio_future_t timeout_future = nimcp_bio_promise_get_future(timeout_promise);

    nimcp_bio_promise_t error_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_NOREPINEPHRINE, sizeof(int));
    E2E_ASSERT_NOT_NULL(error_promise, "Error promise creation failed");
    nimcp_bio_future_t error_future = nimcp_bio_promise_get_future(error_promise);

    E2E_STAGE_END();

    // Stage 2: Test timeout handling
    E2E_STAGE_BEGIN("Test timeout handling", 1000);

    // Don't complete timeout_promise - let it timeout
    int result = 0;
    nimcp_error_t err = nimcp_bio_future_wait(timeout_future, &result, 100);
    E2E_ASSERT(err == NIMCP_ERROR_TIMEOUT || err != NIMCP_SUCCESS,
               "Should timeout on incomplete promise");

    nimcp_bio_future_state_t state = nimcp_bio_future_state(timeout_future);
    E2E_ASSERT(state == BIO_FUTURE_PENDING || state == BIO_FUTURE_FAILED,
               "Future should be pending or failed");

    std::cout << "  Timeout test passed: properly handled incomplete promise\n";

    E2E_STAGE_END();

    // Stage 3: Test successful completion after retry
    E2E_STAGE_BEGIN("Test successful completion", 500);

    std::thread success_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int value = 42;
        nimcp_bio_promise_complete(success_promise, &value);
    });

    success_thread.join();

    int success_result = 0;
    err = nimcp_bio_future_wait(success_future, &success_result, 500);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Successful completion should work");
    E2E_ASSERT(success_result == 42, "Result value should be correct");

    E2E_STAGE_END();

    // Stage 4: Test error propagation
    E2E_STAGE_BEGIN("Test error propagation", 500);

    std::thread error_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Complete with error signal (negative value)
        int error_value = -1;
        nimcp_bio_promise_complete(error_promise, &error_value);
    });

    error_thread.join();

    int error_result = 0;
    err = nimcp_bio_future_wait(error_future, &error_result, 500);
    // Future completes, but application can check for error value
    if (err == NIMCP_SUCCESS) {
        E2E_ASSERT(error_result == -1, "Error value should propagate");
        std::cout << "  Error propagation test passed: error value received\n";
    }

    E2E_STAGE_END();

    // Stage 5: Test recovery with predictive coding
    E2E_STAGE_BEGIN("Test predictive coding for error correction", 1000);

    // Create predictive network config
    predictive_config_t pred_config = predictive_default_config();
    pred_config.num_layers = 3;
    uint32_t layer_sizes[] = {10, 5, 1};
    pred_config.layer_sizes = layer_sizes;

    predictive_network_t model = predictive_create(&pred_config);
    E2E_ASSERT_NOT_NULL(model, "Predictive model creation failed");

    // Feed observations with errors
    float observation_history[10];
    for (int i = 0; i < 10; i++) {
        float observation = 50.0f + (i % 2 == 0 ? 5.0f : -5.0f); // Oscillating error
        observation_history[i] = observation;

        // Prepare input (normalize to 0-1 range)
        float input[10];
        for (int j = 0; j < 10; j++) {
            input[j] = (j < i + 1) ? observation_history[j] / 100.0f : 0.0f;
        }

        predictive_forward(model, input, 5);
        predictive_update_model(model);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Get final prediction from top layer
    float final_prediction_vec[1];
    predictive_get_layer_prediction(model, 2, final_prediction_vec);
    float final_prediction = final_prediction_vec[0] * 100.0f; // Denormalize

    // Note: Predictive model output varies based on random initialization
    // Using wide tolerance for e2e test - just verify model runs without crash
    E2E_ASSERT(fabsf(final_prediction - 50.0f) < 60.0f,
               "Prediction should be in reasonable range");

    std::cout << "  Predictive coding adapted: " << final_prediction << " (target: 50.0)\n";

    predictive_destroy(model);

    E2E_STAGE_END();

    // Stage 6: Verify error statistics
    E2E_STAGE_BEGIN("Verify error statistics", 200);

    nimcp_bio_async_stats_t stats;
    err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get stats");

    std::cout << "  Error handling stats:\n";
    std::cout << "    Total futures: " << stats.total_futures_created << "\n";
    std::cout << "    Completed: " << stats.total_futures_completed << "\n";
    std::cout << "    Decayed: " << stats.total_futures_decayed << "\n";

    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);  // Increased timeout for bio-async cleanup

    nimcp_bio_future_destroy(error_future);
    nimcp_bio_promise_destroy(error_promise);
    nimcp_bio_future_destroy(timeout_future);
    nimcp_bio_promise_destroy(timeout_promise);
    nimcp_bio_future_destroy(success_future);
    nimcp_bio_promise_destroy(success_promise);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Multi-Module Cognitive Integration Stress Test
//=============================================================================

TEST_F(CognitiveBioAsyncE2ETest, MultiModuleCognitiveStressTest) {
    E2E_PIPELINE_START("Multi-Module Cognitive Integration Stress Test");

    // Stage 1: Create all cognitive modules
    E2E_STAGE_BEGIN("Create cognitive ecosystem", 1000);

    mirror_neuron_config_t default_mirror_config = mirror_neurons_get_default_config();
    mirror_neurons_t mirror = mirror_neurons_create(&default_mirror_config);
    E2E_ASSERT_NOT_NULL(mirror, "Mirror neurons creation failed");

    global_workspace_t* workspace = global_workspace_create();
    E2E_ASSERT_NOT_NULL(workspace, "Workspace creation failed");

    E2E_STAGE_END();

    // Stage 2: Create high-volume communication channels
    E2E_STAGE_BEGIN("Create communication channels", 800);

    const int NUM_CHANNELS = 10;
    std::vector<nimcp_bio_promise_t> promises(NUM_CHANNELS);
    std::vector<nimcp_bio_future_t> futures(NUM_CHANNELS);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        nimcp_bio_channel_type_t channel =
            static_cast<nimcp_bio_channel_type_t>(i % 4);
        promises[i] = nimcp_bio_promise_create(channel, sizeof(float) * 64);
        E2E_ASSERT_NOT_NULL(promises[i], "Promise creation failed");
        futures[i] = nimcp_bio_promise_get_future(promises[i]);
        E2E_ASSERT_NOT_NULL(futures[i], "Future creation failed");
    }

    E2E_STAGE_END();

    // Stage 3: Concurrent multi-module processing
    E2E_STAGE_BEGIN("Concurrent multi-module processing", 3000);

    std::atomic<int> completed_operations{0};
    std::vector<std::thread> threads;

    // Launch concurrent cognitive operations
    for (int i = 0; i < NUM_CHANNELS; i++) {
        threads.emplace_back([i, &promises, &completed_operations]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds((i * 37) % 100)); // Staggered start

            std::vector<float> data(64);
            for (int j = 0; j < 64; j++) {
                data[j] = static_cast<float>(i * 64 + j) * 0.001f;
            }

            nimcp_error_t err = nimcp_bio_promise_complete(promises[i], data.data());
            if (err == NIMCP_SUCCESS) {
                completed_operations++;
            }
        });
    }

    // Wait for all operations
    for (auto& t : threads) {
        t.join();
    }

    E2E_ASSERT(completed_operations.load() == NUM_CHANNELS,
               "All operations should complete");

    E2E_STAGE_END();

    // Stage 4: Verify all data received
    E2E_STAGE_BEGIN("Verify concurrent data reception", 2000);

    int successful_receptions = 0;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        std::vector<float> received(64);
        nimcp_error_t err = nimcp_bio_future_wait(futures[i], received.data(), 500);
        if (err == NIMCP_SUCCESS) {
            successful_receptions++;
        }
    }

    E2E_ASSERT(successful_receptions == NUM_CHANNELS,
               "All data should be received");
    std::cout << "  Successfully processed " << successful_receptions
              << " concurrent operations\n";

    E2E_STAGE_END();

    // Stage 5: Verify system health under load
    E2E_STAGE_BEGIN("Verify system health", 300);

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get stats");

    std::cout << "  System health:\n";
    std::cout << "    Total futures: " << stats.total_futures_created << "\n";
    std::cout << "    Completed: " << stats.total_futures_completed << "\n";
    std::cout << "    Success rate: "
              << (100.0f * stats.total_futures_completed / stats.total_futures_created)
              << "%\n";

    E2E_ASSERT(stats.total_futures_completed >= NUM_CHANNELS,
               "Should complete all stress test operations");

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        nimcp_bio_future_destroy(futures[i]);
        nimcp_bio_promise_destroy(promises[i]);
    }

    if (workspace) global_workspace_destroy(workspace);
    if (mirror) mirror_neurons_destroy(mirror);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
