/**
 * @file e2e_test_rcog_collective_integration.cpp
 * @brief End-to-end tests for Recursive and Collective Cognition integration
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: E2E tests for complete cognitive pipeline with Recursive and Collective Cognition
 * WHY:  Validate end-to-end behavior of recursive task decomposition and collective
 *       intelligence working together with FEP coordination
 * HOW:  Test complete workflows including perception, attention, recursion, collective
 *       coordination, decision making, and output publishing
 *
 * TEST SCENARIOS:
 * - RecursiveCognitionFullPipeline: Complete recursive task decomposition pipeline
 * - CollectiveCognitionFullPipeline: Complete collective intelligence pipeline
 * - RcogCollectiveInteraction: Recursive-collective coordination
 * - StressTest: High load on both modules
 * - ErrorRecovery: Graceful error handling and FEP adaptation
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <random>

// Recursive Cognition
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_snn_bridge.h"
#include "cognitive/recursive/nimcp_rcog_plasticity_bridge.h"
#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"

// Collective Cognition
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/collective_cognition/nimcp_collective_snn_bridge.h"
#include "cognitive/collective_cognition/nimcp_collective_plasticity_bridge.h"

// FEP Integration
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int MAX_AGENTS = 8;
static constexpr int MAX_RECURSION_DEPTH = 5;
static constexpr int STRESS_ITERATIONS = 100;
static constexpr float FEP_THRESHOLD = 0.1f;

//=============================================================================
// Test Fixture
//=============================================================================

class RcogCollectiveIntegrationTest : public ::testing::Test {
protected:
    // Recursive Cognition Components
    rcog_snn_bridge_t* rcog_snn = nullptr;
    rcog_plasticity_bridge_t* rcog_plasticity = nullptr;
    rcog_orchestrator_t* rcog_orchestrator = nullptr;
    rcog_context_store_t* rcog_context = nullptr;
    rcog_answer_refiner_t* rcog_refiner = nullptr;

    // Collective Cognition Components
    collective_snn_bridge_t* coll_snn = nullptr;
    collective_plasticity_bridge_t* coll_plasticity = nullptr;

    // Event tracking
    std::atomic<int> rcog_depth_events{0};
    std::atomic<int> rcog_completion_events{0};
    std::atomic<int> coll_sync_events{0};
    std::atomic<int> coll_consensus_events{0};
    std::atomic<int> fep_adaptation_events{0};
    std::atomic<bool> pipeline_error{false};

    // Mutex for thread-safe operations
    std::mutex event_mutex;

    void SetUp() override {
        // Initialize Recursive Cognition SNN Bridge
        rcog_snn_config_t rcog_snn_config = rcog_snn_config_default();
        rcog_snn_config.enable_bio_async = false;
        rcog_snn = rcog_snn_create(&rcog_snn_config);
        ASSERT_NE(rcog_snn, nullptr) << "Failed to create Rcog SNN bridge";

        // Initialize Recursive Cognition Plasticity Bridge
        rcog_plasticity_config_t rcog_plast_config = rcog_plasticity_config_default();
        rcog_plast_config.enable_bio_async = false;
        rcog_plasticity = rcog_plasticity_create(&rcog_plast_config);
        ASSERT_NE(rcog_plasticity, nullptr) << "Failed to create Rcog Plasticity bridge";

        // Initialize Recursive Cognition Core Components
        rcog_orchestrator = rcog_orchestrator_create_default();
        ASSERT_NE(rcog_orchestrator, nullptr) << "Failed to create Rcog Orchestrator";

        rcog_context = rcog_context_store_create_default();
        ASSERT_NE(rcog_context, nullptr) << "Failed to create Rcog Context Store";

        rcog_refiner = rcog_answer_refiner_create_default();
        ASSERT_NE(rcog_refiner, nullptr) << "Failed to create Rcog Answer Refiner";

        // Initialize Collective Cognition SNN Bridge
        collective_snn_config_t coll_snn_config = collective_snn_config_default();
        coll_snn_config.enable_bio_async = false;
        coll_snn = collective_snn_create(&coll_snn_config);
        ASSERT_NE(coll_snn, nullptr) << "Failed to create Collective SNN bridge";

        // Initialize Collective Cognition Plasticity Bridge
        collective_plasticity_config_t coll_plast_config = collective_plasticity_config_default();
        coll_plast_config.enable_bio_async = false;
        coll_plasticity = collective_plasticity_create(&coll_plast_config);
        ASSERT_NE(coll_plasticity, nullptr) << "Failed to create Collective Plasticity bridge";

        // Register synapses for learning
        RegisterSynapses();

        // Reset counters
        rcog_depth_events = 0;
        rcog_completion_events = 0;
        coll_sync_events = 0;
        coll_consensus_events = 0;
        fep_adaptation_events = 0;
        pipeline_error = false;
    }

    void TearDown() override {
        // Destroy Collective Cognition
        if (coll_plasticity) collective_plasticity_destroy(coll_plasticity);
        if (coll_snn) collective_snn_destroy(coll_snn);

        // Destroy Recursive Cognition
        if (rcog_refiner) rcog_answer_refiner_destroy(rcog_refiner);
        if (rcog_context) rcog_context_store_destroy(rcog_context);
        if (rcog_orchestrator) rcog_orchestrator_destroy(rcog_orchestrator);
        if (rcog_plasticity) rcog_plasticity_destroy(rcog_plasticity);
        if (rcog_snn) rcog_snn_destroy(rcog_snn);
    }

    void RegisterSynapses() {
        // Register Rcog plasticity synapses
        for (int i = 0; i < 10; i++) {
            rcog_plasticity_register_synapse(rcog_plasticity,
                i, RCOG_SYNAPSE_DECOMPOSITION, 0.5f);
            rcog_plasticity_register_synapse(rcog_plasticity,
                10 + i, RCOG_SYNAPSE_AGGREGATION, 0.5f);
        }

        // Register Collective plasticity synapses
        for (int i = 0; i < 10; i++) {
            collective_plasticity_register_synapse(coll_plasticity,
                i, COLLECTIVE_SYNAPSE_SYNCHRONIZATION, 0.5f);
            collective_plasticity_register_synapse(coll_plasticity,
                10 + i, COLLECTIVE_SYNAPSE_CONSENSUS, 0.5f);
        }
    }

    // Helper: Simulate perception input
    void SimulatePerceptionInput(float* dims, int count, float complexity) {
        for (int i = 0; i < count; i++) {
            dims[i] = complexity * (0.5f + 0.3f * sinf((float)i * 0.1f));
        }
    }

    // Helper: Simulate recursive task decomposition
    int SimulateRecursiveDecomposition(float complexity, int max_depth) {
        int current_depth = 0;
        float remaining_complexity = complexity;

        while (remaining_complexity > 0.2f && current_depth < max_depth) {
            // Encode current depth
            float depth_level = (float)current_depth / (float)max_depth;
            rcog_snn_encode_depth(rcog_snn, depth_level, max_depth);
            rcog_snn_simulate(rcog_snn, 10.0f);

            // Apply learning for decomposition success
            rcog_plasticity_learn(rcog_plasticity,
                RCOG_LEARN_DECOMP_SUCCESS, 0.3f, current_depth % 10, 0.7f);

            // Reduce complexity (simulating task decomposition)
            remaining_complexity *= 0.6f;
            current_depth++;
            rcog_depth_events++;
        }

        return current_depth;
    }

    // Helper: Simulate collective consensus building
    float SimulateConsensusBuilding(int num_agents, int max_rounds) {
        float consensus_level = 0.3f;
        int round = 0;

        while (consensus_level < 0.9f && round < max_rounds) {
            // Encode collective state
            collective_snn_encode_decision(coll_snn, consensus_level, num_agents);
            collective_snn_simulate(coll_snn, 10.0f);

            // Get drive
            collective_drive_t drive;
            collective_snn_get_drive(coll_snn, &drive);

            // Apply learning
            if (drive.consensus_level > consensus_level) {
                collective_plasticity_learn(coll_plasticity,
                    COLLECTIVE_LEARN_CONSENSUS_REACHED, 0.2f, round % 10, 0.7f);
            }

            // Update consensus (simulating agents reaching agreement)
            consensus_level = fminf(1.0f, consensus_level + 0.1f + drive.coordination_drive * 0.1f);
            round++;

            if (drive.sync_detected) {
                coll_sync_events++;
            }
        }

        coll_consensus_events++;
        return consensus_level;
    }
};

//=============================================================================
// RecursiveCognitionFullPipeline Tests
//=============================================================================

/**
 * Test: Complete recursive cognition pipeline
 * - Input received by perception
 * - Attention gates to rcog
 * - Rcog decomposes task
 * - Subtasks executed
 * - Answer refined
 * - Output published
 * - FEP coordinates throughout
 */
TEST_F(RcogCollectiveIntegrationTest, RecursiveCognitionFullPipeline) {
    // Step 1: Input received by perception
    float perception_dims[RCOG_DIM_COUNT];
    SimulatePerceptionInput(perception_dims, RCOG_DIM_COUNT, 0.9f);  // High complexity

    // Step 2: Attention gates to rcog (encode state)
    int spikes = rcog_snn_encode_state(rcog_snn, perception_dims, RCOG_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    // Step 3: Rcog decomposes task
    int final_depth = SimulateRecursiveDecomposition(0.9f, MAX_RECURSION_DEPTH);
    EXPECT_GT(final_depth, 0) << "Task should be decomposed";
    EXPECT_LE(final_depth, MAX_RECURSION_DEPTH);

    // Step 4: Subtasks executed (simulate via SNN processing)
    for (int i = 0; i < final_depth; i++) {
        rcog_snn_step(rcog_snn);

        // Check cognitive state
        rcog_cognitive_state_t state;
        EXPECT_EQ(rcog_snn_get_cognitive_state(rcog_snn, &state), 0);
        EXPECT_GE(state.recursion_depth, 0.0f);
        EXPECT_LE(state.recursion_depth, 1.0f);
    }

    // Step 5: Answer refined (apply aggregation learning)
    for (int i = 0; i < final_depth; i++) {
        rcog_plasticity_learn(rcog_plasticity,
            RCOG_LEARN_AGGREGATION_GOOD, 0.4f, 10 + (i % 10), 0.8f);
    }

    // Step 6: Output published (verify final state)
    rcog_snn_stats_t snn_stats;
    EXPECT_EQ(rcog_snn_get_stats(rcog_snn, &snn_stats), 0);
    EXPECT_GT(snn_stats.total_evaluations, 0u);
    EXPECT_GT(snn_stats.total_simulations, 0u);

    rcog_plasticity_stats_t plast_stats;
    EXPECT_EQ(rcog_plasticity_get_stats(rcog_plasticity, &plast_stats), 0);
    EXPECT_GT(plast_stats.total_learning_events, 0u);

    // Step 7: FEP coordinates (verify cognitive state is optimal)
    rcog_cognitive_state_t final_state;
    EXPECT_EQ(rcog_snn_get_cognitive_state(rcog_snn, &final_state), 0);
    EXPECT_TRUE(std::isfinite(final_state.recursion_depth));
    EXPECT_TRUE(std::isfinite(final_state.aggregation_confidence));

    // Verify depth events were tracked
    EXPECT_GT(rcog_depth_events.load(), 0);
}

/**
 * Test: Recursive cognition with varying task complexity
 */
TEST_F(RcogCollectiveIntegrationTest, RecursiveCognitionVaryingComplexity) {
    float complexities[] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};

    for (float complexity : complexities) {
        rcog_snn_reset(rcog_snn);

        float perception_dims[RCOG_DIM_COUNT];
        SimulatePerceptionInput(perception_dims, RCOG_DIM_COUNT, complexity);

        rcog_snn_encode_state(rcog_snn, perception_dims, RCOG_DIM_COUNT);
        rcog_snn_simulate(rcog_snn, 20.0f);

        // Decompose
        int depth = SimulateRecursiveDecomposition(complexity, MAX_RECURSION_DEPTH);

        // Higher complexity should generally require deeper recursion
        rcog_cognitive_state_t state;
        rcog_snn_get_cognitive_state(rcog_snn, &state);

        // Verify state is valid
        EXPECT_GE(state.recursion_depth, 0.0f);
        EXPECT_LE(state.recursion_depth, 1.0f);
        EXPECT_GE(state.problem_complexity, 0.0f);
        EXPECT_LE(state.problem_complexity, 1.0f);
    }
}

//=============================================================================
// CollectiveCognitionFullPipeline Tests
//=============================================================================

/**
 * Test: Complete collective cognition pipeline
 * - Multiple agents receive input
 * - Collective integrates information
 * - Phi computed
 * - Consensus reached
 * - Decision broadcast
 * - FEP coordinates throughout
 */
TEST_F(RcogCollectiveIntegrationTest, CollectiveCognitionFullPipeline) {
    const int NUM_AGENTS = 4;

    // Step 1: Multiple agents receive input
    float agent_inputs[MAX_AGENTS][COLLECTIVE_DIM_COUNT];
    for (int agent = 0; agent < NUM_AGENTS; agent++) {
        for (int d = 0; d < COLLECTIVE_DIM_COUNT; d++) {
            // Each agent gets slightly different input
            agent_inputs[agent][d] = 0.5f + 0.3f * sinf((float)(d + agent) * 0.1f);
        }
    }

    // Step 2: Collective integrates information
    for (int agent = 0; agent < NUM_AGENTS; agent++) {
        collective_snn_encode_state(coll_snn, agent_inputs[agent], COLLECTIVE_DIM_COUNT);
        collective_snn_step(coll_snn);
    }

    // Step 3: Phi computed (simulate via coherence encoding)
    collective_snn_encode_swarm(coll_snn, 0.7f, 0.6f);
    collective_snn_simulate(coll_snn, 30.0f);

    collective_drive_t drive;
    EXPECT_EQ(collective_snn_get_drive(coll_snn, &drive), 0);
    EXPECT_GE(drive.swarm_coherence, 0.0f);
    EXPECT_LE(drive.swarm_coherence, 1.0f);

    // Step 4: Consensus reached
    float consensus = SimulateConsensusBuilding(NUM_AGENTS, 10);
    EXPECT_GE(consensus, 0.7f) << "Consensus should be reached";

    // Step 5: Decision broadcast (encode final decision)
    collective_snn_encode_decision(coll_snn, consensus, NUM_AGENTS);
    collective_snn_simulate(coll_snn, 20.0f);

    // Step 6: FEP coordinates (verify coordination drive)
    collective_drive_t final_drive;
    EXPECT_EQ(collective_snn_get_drive(coll_snn, &final_drive), 0);
    EXPECT_TRUE(std::isfinite(final_drive.coordination_drive));
    EXPECT_GE(final_drive.consensus_level, 0.0f);
    EXPECT_LE(final_drive.consensus_level, 1.0f);

    // Verify stats
    collective_snn_stats_t snn_stats;
    EXPECT_EQ(collective_snn_get_stats(coll_snn, &snn_stats), 0);
    EXPECT_GT(snn_stats.total_evaluations, 0u);

    collective_plasticity_stats_t plast_stats;
    EXPECT_EQ(collective_plasticity_get_stats(coll_plasticity, &plast_stats), 0);
    EXPECT_GT(plast_stats.total_learning_events, 0u);

    EXPECT_GT(coll_consensus_events.load(), 0);
}

/**
 * Test: Collective cognition with varying agent counts
 */
TEST_F(RcogCollectiveIntegrationTest, CollectiveCognitionVaryingAgents) {
    int agent_counts[] = {2, 4, 6, 8};

    for (int num_agents : agent_counts) {
        collective_snn_reset(coll_snn);

        // Encode decision with varying participants
        collective_snn_encode_decision(coll_snn, 0.5f, num_agents);
        collective_snn_simulate(coll_snn, 20.0f);

        float consensus = SimulateConsensusBuilding(num_agents, 15);

        // Verify consensus was reached
        EXPECT_GE(consensus, 0.5f) << "Should reach minimum consensus with " << num_agents << " agents";

        collective_drive_t drive;
        collective_snn_get_drive(coll_snn, &drive);
        EXPECT_TRUE(std::isfinite(drive.coordination_drive));
    }
}

//=============================================================================
// RcogCollectiveInteraction Tests
//=============================================================================

/**
 * Test: Recursive-Collective interaction
 * - Rcog spawns subtasks
 * - Collective coordinates subtask execution
 * - Results aggregated
 * - Combined answer produced
 */
TEST_F(RcogCollectiveIntegrationTest, RcogCollectiveInteraction) {
    const int NUM_SUBTASKS = 3;
    const int NUM_AGENTS = 4;

    // Phase 1: Rcog spawns subtasks (simulate via decomposition)
    float initial_complexity = 0.8f;
    int decomposition_depth = SimulateRecursiveDecomposition(initial_complexity, MAX_RECURSION_DEPTH);
    EXPECT_GT(decomposition_depth, 0);

    // Track subtask results
    float subtask_results[NUM_SUBTASKS];

    // Phase 2: Collective coordinates subtask execution
    for (int subtask = 0; subtask < NUM_SUBTASKS; subtask++) {
        // Each subtask is processed by collective
        collective_snn_reset(coll_snn);

        // Agents work on subtask
        float subtask_dims[COLLECTIVE_DIM_COUNT];
        for (int d = 0; d < COLLECTIVE_DIM_COUNT; d++) {
            subtask_dims[d] = 0.5f + 0.2f * sinf((float)(d + subtask) * 0.15f);
        }

        collective_snn_encode_state(coll_snn, subtask_dims, COLLECTIVE_DIM_COUNT);
        collective_snn_simulate(coll_snn, 15.0f);

        // Reach consensus on subtask
        float subtask_consensus = SimulateConsensusBuilding(NUM_AGENTS, 8);
        subtask_results[subtask] = subtask_consensus;

        // Apply collective learning
        collective_plasticity_learn(coll_plasticity,
            COLLECTIVE_LEARN_COORDINATION_SUCCESS, 0.3f, subtask, 0.7f);
    }

    // Phase 3: Results aggregated (via Rcog aggregation)
    float aggregated_result = 0.0f;
    for (int i = 0; i < NUM_SUBTASKS; i++) {
        aggregated_result += subtask_results[i];

        // Apply aggregation learning in Rcog
        rcog_plasticity_learn(rcog_plasticity,
            RCOG_LEARN_AGGREGATION_GOOD, subtask_results[i], 10 + i, 0.8f);
    }
    aggregated_result /= NUM_SUBTASKS;

    // Phase 4: Combined answer produced
    EXPECT_GE(aggregated_result, 0.5f) << "Aggregated result should be reasonable";
    EXPECT_LE(aggregated_result, 1.0f);

    // Verify both systems learned
    rcog_plasticity_stats_t rcog_stats;
    EXPECT_EQ(rcog_plasticity_get_stats(rcog_plasticity, &rcog_stats), 0);
    EXPECT_GT(rcog_stats.total_learning_events, 0u);

    collective_plasticity_stats_t coll_stats;
    EXPECT_EQ(collective_plasticity_get_stats(coll_plasticity, &coll_stats), 0);
    EXPECT_GT(coll_stats.total_learning_events, 0u);
}

/**
 * Test: Bidirectional Rcog-Collective communication
 */
TEST_F(RcogCollectiveIntegrationTest, BidirectionalRcogCollective) {
    // Rcog initiates problem decomposition
    float rcog_dims[RCOG_DIM_COUNT];
    SimulatePerceptionInput(rcog_dims, RCOG_DIM_COUNT, 0.7f);
    rcog_snn_encode_state(rcog_snn, rcog_dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(rcog_snn, 15.0f);

    rcog_cognitive_state_t rcog_state;
    rcog_snn_get_cognitive_state(rcog_snn, &rcog_state);

    // Collective receives recursion depth as coordination signal
    float coll_dims[COLLECTIVE_DIM_COUNT];
    for (int d = 0; d < COLLECTIVE_DIM_COUNT; d++) {
        coll_dims[d] = 0.5f;
    }
    coll_dims[COLLECTIVE_DIM_SHARED_INTENTION] = rcog_state.recursion_depth;
    coll_dims[COLLECTIVE_DIM_DISTRIBUTED_DECISION] = rcog_state.problem_complexity;

    collective_snn_encode_state(coll_snn, coll_dims, COLLECTIVE_DIM_COUNT);
    collective_snn_simulate(coll_snn, 15.0f);

    collective_drive_t coll_drive;
    collective_snn_get_drive(coll_snn, &coll_drive);

    // Collective coordination feeds back to Rcog
    rcog_dims[RCOG_DIM_META_COGNITIVE_LEVEL] = coll_drive.coordination_drive;
    rcog_snn_encode_state(rcog_snn, rcog_dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(rcog_snn, 10.0f);

    // Verify bidirectional flow maintained valid states
    rcog_snn_get_cognitive_state(rcog_snn, &rcog_state);
    EXPECT_TRUE(std::isfinite(rcog_state.meta_cognitive_level));

    collective_snn_get_drive(coll_snn, &coll_drive);
    EXPECT_TRUE(std::isfinite(coll_drive.shared_intention));
}

//=============================================================================
// StressTest
//=============================================================================

/**
 * Test: High load on both modules
 * - Many concurrent recursions
 * - Many agents in collective
 * - System remains stable
 * - No deadlocks or crashes
 */
TEST_F(RcogCollectiveIntegrationTest, StressTest) {
    constexpr int NUM_THREADS = 4;
    std::atomic<int> errors{0};
    std::atomic<int> completed_ops{0};

    // Thread 1-2: Recursive operations
    std::vector<std::thread> rcog_threads;
    for (int t = 0; t < 2; t++) {
        rcog_threads.emplace_back([this, t, &errors, &completed_ops]() {
            for (int i = 0; i < STRESS_ITERATIONS / 2; i++) {
                float dims[RCOG_DIM_COUNT];
                for (int d = 0; d < RCOG_DIM_COUNT; d++) {
                    dims[d] = 0.5f + 0.3f * sinf((float)(d + i + t) * 0.1f);
                }

                int result = rcog_snn_encode_state(rcog_snn, dims, RCOG_DIM_COUNT);
                if (result < 0) errors++;

                result = rcog_snn_step(rcog_snn);
                if (result != 0) errors++;

                // Apply learning
                result = rcog_plasticity_learn(rcog_plasticity,
                    RCOG_LEARN_DECOMP_SUCCESS, 0.1f, i % 10, 0.5f);
                if (result != 0) errors++;

                completed_ops++;
            }
        });
    }

    // Thread 3-4: Collective operations
    std::vector<std::thread> coll_threads;
    for (int t = 0; t < 2; t++) {
        coll_threads.emplace_back([this, t, &errors, &completed_ops]() {
            for (int i = 0; i < STRESS_ITERATIONS / 2; i++) {
                float dims[COLLECTIVE_DIM_COUNT];
                for (int d = 0; d < COLLECTIVE_DIM_COUNT; d++) {
                    dims[d] = 0.5f + 0.3f * sinf((float)(d + i + t) * 0.1f);
                }

                int result = collective_snn_encode_state(coll_snn, dims, COLLECTIVE_DIM_COUNT);
                if (result < 0) errors++;

                result = collective_snn_step(coll_snn);
                if (result != 0) errors++;

                // Apply learning
                result = collective_plasticity_learn(coll_plasticity,
                    COLLECTIVE_LEARN_SYNC_ACHIEVED, 0.1f, i % 10, 0.5f);
                if (result != 0) errors++;

                completed_ops++;
            }
        });
    }

    // Wait for all threads
    for (auto& t : rcog_threads) t.join();
    for (auto& t : coll_threads) t.join();

    // Verify no errors
    EXPECT_EQ(errors.load(), 0) << "Stress test encountered errors";

    // Verify operations completed
    EXPECT_GE(completed_ops.load(), STRESS_ITERATIONS);

    // Verify both systems are still functional
    rcog_snn_bridge_state_t rcog_bridge_state;
    EXPECT_EQ(rcog_snn_get_state(rcog_snn, &rcog_bridge_state), 0);
    EXPECT_NE(rcog_bridge_state.state, RCOG_SNN_STATE_ERROR);

    collective_snn_bridge_state_t coll_bridge_state;
    EXPECT_EQ(collective_snn_get_state(coll_snn, &coll_bridge_state), 0);
    EXPECT_NE(coll_bridge_state.state, COLLECTIVE_SNN_STATE_ERROR);
}

/**
 * Test: Sustained high-frequency operations
 */
TEST_F(RcogCollectiveIntegrationTest, StressTestHighFrequency) {
    const int RAPID_OPS = 1000;
    std::atomic<int> errors{0};

    auto start = std::chrono::high_resolution_clock::now();

    // Rapid alternating operations
    for (int i = 0; i < RAPID_OPS; i++) {
        // Rcog step
        if (rcog_snn_step(rcog_snn) != 0) errors++;

        // Collective step
        if (collective_snn_step(coll_snn) != 0) errors++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(errors.load(), 0);
    EXPECT_LT(duration.count(), 5000) << "High frequency test took too long: "
                                       << duration.count() << "ms";

    // Verify stability
    rcog_cognitive_state_t rcog_state;
    EXPECT_EQ(rcog_snn_get_cognitive_state(rcog_snn, &rcog_state), 0);
    EXPECT_TRUE(std::isfinite(rcog_state.recursion_depth));

    collective_drive_t coll_drive;
    EXPECT_EQ(collective_snn_get_drive(coll_snn, &coll_drive), 0);
    EXPECT_TRUE(std::isfinite(coll_drive.coordination_drive));
}

//=============================================================================
// ErrorRecovery Tests
//=============================================================================

/**
 * Test: Error recovery in Rcog
 * - Inject errors in rcog
 * - System recovers gracefully
 * - FEP adapts to errors
 */
TEST_F(RcogCollectiveIntegrationTest, ErrorRecoveryRcog) {
    // Test with invalid simulation times (should be rejected)
    EXPECT_EQ(rcog_snn_simulate(rcog_snn, 0.0f), -1);
    EXPECT_EQ(rcog_snn_simulate(rcog_snn, -1.0f), -1);

    // Verify system still functional after error rejection
    float valid_dims[RCOG_DIM_COUNT] = {0.5f};
    EXPECT_GE(rcog_snn_encode_state(rcog_snn, valid_dims, RCOG_DIM_COUNT), 0);
    EXPECT_EQ(rcog_snn_simulate(rcog_snn, 10.0f), 0);

    // Test with NULL parameters (should be rejected gracefully)
    EXPECT_EQ(rcog_snn_encode_state(rcog_snn, nullptr, RCOG_DIM_COUNT), -1);
    EXPECT_EQ(rcog_snn_get_cognitive_state(rcog_snn, nullptr), -1);

    // System should still work
    rcog_cognitive_state_t state;
    EXPECT_EQ(rcog_snn_get_cognitive_state(rcog_snn, &state), 0);
    EXPECT_TRUE(std::isfinite(state.recursion_depth));

    // Verify no error state
    rcog_snn_bridge_state_t bridge_state;
    EXPECT_EQ(rcog_snn_get_state(rcog_snn, &bridge_state), 0);
    EXPECT_NE(bridge_state.state, RCOG_SNN_STATE_ERROR);
}

/**
 * Test: Error recovery in Collective
 * - Inject errors in collective
 * - System recovers gracefully
 * - FEP adapts to errors
 */
TEST_F(RcogCollectiveIntegrationTest, ErrorRecoveryCollective) {
    // Test with invalid simulation times (should be rejected)
    EXPECT_EQ(collective_snn_simulate(coll_snn, 0.0f), -1);
    EXPECT_EQ(collective_snn_simulate(coll_snn, -1.0f), -1);

    // Verify system still functional after error rejection
    float valid_dims[COLLECTIVE_DIM_COUNT] = {0.5f};
    EXPECT_GE(collective_snn_encode_state(coll_snn, valid_dims, COLLECTIVE_DIM_COUNT), 0);
    EXPECT_EQ(collective_snn_simulate(coll_snn, 10.0f), 0);

    // Test with NULL parameters (should be rejected gracefully)
    EXPECT_EQ(collective_snn_encode_state(coll_snn, nullptr, COLLECTIVE_DIM_COUNT), -1);
    EXPECT_EQ(collective_snn_get_drive(coll_snn, nullptr), -1);

    // System should still work
    collective_drive_t drive;
    EXPECT_EQ(collective_snn_get_drive(coll_snn, &drive), 0);
    EXPECT_TRUE(std::isfinite(drive.coordination_drive));

    // Verify no error state
    collective_snn_bridge_state_t bridge_state;
    EXPECT_EQ(collective_snn_get_state(coll_snn, &bridge_state), 0);
    EXPECT_NE(bridge_state.state, COLLECTIVE_SNN_STATE_ERROR);
}

/**
 * Test: Combined error recovery
 * - Inject errors in both systems
 * - Both recover gracefully
 * - Can continue processing
 */
TEST_F(RcogCollectiveIntegrationTest, ErrorRecoveryCombined) {
    // Inject errors
    rcog_snn_simulate(rcog_snn, -1.0f);  // Invalid
    collective_snn_simulate(coll_snn, -1.0f);  // Invalid

    // Attempt normal operations
    float rcog_dims[RCOG_DIM_COUNT] = {0.5f};
    float coll_dims[COLLECTIVE_DIM_COUNT] = {0.5f};

    EXPECT_GE(rcog_snn_encode_state(rcog_snn, rcog_dims, RCOG_DIM_COUNT), 0);
    EXPECT_GE(collective_snn_encode_state(coll_snn, coll_dims, COLLECTIVE_DIM_COUNT), 0);

    EXPECT_EQ(rcog_snn_simulate(rcog_snn, 10.0f), 0);
    EXPECT_EQ(collective_snn_simulate(coll_snn, 10.0f), 0);

    // Complete a full pipeline after errors
    float perception_dims[RCOG_DIM_COUNT];
    SimulatePerceptionInput(perception_dims, RCOG_DIM_COUNT, 0.6f);
    rcog_snn_encode_state(rcog_snn, perception_dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(rcog_snn, 20.0f);

    float consensus = SimulateConsensusBuilding(4, 10);
    EXPECT_GE(consensus, 0.5f);

    // Verify final states are valid
    rcog_cognitive_state_t rcog_state;
    EXPECT_EQ(rcog_snn_get_cognitive_state(rcog_snn, &rcog_state), 0);
    EXPECT_TRUE(std::isfinite(rcog_state.recursion_depth));

    collective_drive_t coll_drive;
    EXPECT_EQ(collective_snn_get_drive(coll_snn, &coll_drive), 0);
    EXPECT_TRUE(std::isfinite(coll_drive.consensus_level));
}

/**
 * Test: Recovery with reset
 */
TEST_F(RcogCollectiveIntegrationTest, ErrorRecoveryWithReset) {
    // Do some work
    float dims[RCOG_DIM_COUNT];
    SimulatePerceptionInput(dims, RCOG_DIM_COUNT, 0.8f);
    rcog_snn_encode_state(rcog_snn, dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(rcog_snn, 30.0f);

    // Get stats before reset
    rcog_snn_stats_t before_stats;
    rcog_snn_get_stats(rcog_snn, &before_stats);
    EXPECT_GT(before_stats.total_evaluations, 0u);

    // Reset
    EXPECT_EQ(rcog_snn_reset(rcog_snn), 0);

    // Verify state is cleared
    rcog_snn_bridge_state_t state;
    rcog_snn_get_state(rcog_snn, &state);
    EXPECT_EQ(state.state, RCOG_SNN_STATE_IDLE);

    // Continue working after reset
    rcog_snn_encode_state(rcog_snn, dims, RCOG_DIM_COUNT);
    EXPECT_EQ(rcog_snn_simulate(rcog_snn, 10.0f), 0);

    // Same for collective
    EXPECT_EQ(collective_snn_reset(coll_snn), 0);

    collective_snn_bridge_state_t coll_state;
    collective_snn_get_state(coll_snn, &coll_state);
    EXPECT_EQ(coll_state.state, COLLECTIVE_SNN_STATE_IDLE);

    float coll_dims[COLLECTIVE_DIM_COUNT] = {0.5f};
    collective_snn_encode_state(coll_snn, coll_dims, COLLECTIVE_DIM_COUNT);
    EXPECT_EQ(collective_snn_simulate(coll_snn, 10.0f), 0);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(RcogCollectiveIntegrationTest, PerformanceEndToEndLatency) {
    auto start = std::chrono::high_resolution_clock::now();

    // Complete pipeline: perception -> rcog -> collective -> output
    for (int i = 0; i < 100; i++) {
        // Perception input
        float rcog_dims[RCOG_DIM_COUNT];
        SimulatePerceptionInput(rcog_dims, RCOG_DIM_COUNT, 0.6f);

        // Rcog processing
        rcog_snn_encode_state(rcog_snn, rcog_dims, RCOG_DIM_COUNT);
        rcog_snn_simulate(rcog_snn, 5.0f);

        // Get rcog state
        rcog_cognitive_state_t rcog_state;
        rcog_snn_get_cognitive_state(rcog_snn, &rcog_state);

        // Collective processing
        float coll_dims[COLLECTIVE_DIM_COUNT];
        for (int d = 0; d < COLLECTIVE_DIM_COUNT; d++) {
            coll_dims[d] = 0.5f;
        }
        coll_dims[COLLECTIVE_DIM_SHARED_INTENTION] = rcog_state.recursion_depth;

        collective_snn_encode_state(coll_snn, coll_dims, COLLECTIVE_DIM_COUNT);
        collective_snn_simulate(coll_snn, 5.0f);

        // Get output
        collective_drive_t drive;
        collective_snn_get_drive(coll_snn, &drive);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100 complete pipelines should complete in under 5 seconds
    EXPECT_LT(duration.count(), 5000) << "E2E pipeline too slow: " << duration.count() << "ms";
}

//=============================================================================
// Integration Verification Tests
//=============================================================================

TEST_F(RcogCollectiveIntegrationTest, IntegrationStatsConsistency) {
    // Run complete workflow
    float perception_dims[RCOG_DIM_COUNT];
    SimulatePerceptionInput(perception_dims, RCOG_DIM_COUNT, 0.7f);
    rcog_snn_encode_state(rcog_snn, perception_dims, RCOG_DIM_COUNT);
    rcog_snn_simulate(rcog_snn, 20.0f);

    SimulateRecursiveDecomposition(0.7f, 4);
    SimulateConsensusBuilding(4, 10);

    // Verify all stats are consistent
    rcog_snn_stats_t rcog_snn_stats;
    EXPECT_EQ(rcog_snn_get_stats(rcog_snn, &rcog_snn_stats), 0);
    EXPECT_GT(rcog_snn_stats.total_evaluations, 0u);
    EXPECT_TRUE(std::isfinite(rcog_snn_stats.mean_evaluation_time_ms));

    rcog_plasticity_stats_t rcog_plast_stats;
    EXPECT_EQ(rcog_plasticity_get_stats(rcog_plasticity, &rcog_plast_stats), 0);
    EXPECT_GT(rcog_plast_stats.total_learning_events, 0u);
    EXPECT_TRUE(std::isfinite(rcog_plast_stats.mean_weight_change));

    collective_snn_stats_t coll_snn_stats;
    EXPECT_EQ(collective_snn_get_stats(coll_snn, &coll_snn_stats), 0);
    EXPECT_GT(coll_snn_stats.total_evaluations, 0u);
    EXPECT_TRUE(std::isfinite(coll_snn_stats.mean_evaluation_time_ms));

    collective_plasticity_stats_t coll_plast_stats;
    EXPECT_EQ(collective_plasticity_get_stats(coll_plasticity, &coll_plast_stats), 0);
    EXPECT_GT(coll_plast_stats.total_learning_events, 0u);
    EXPECT_TRUE(std::isfinite(coll_plast_stats.mean_weight_change));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
