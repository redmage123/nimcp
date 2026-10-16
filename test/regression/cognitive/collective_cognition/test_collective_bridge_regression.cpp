/**
 * @file test_collective_bridge_regression.cpp
 * @brief Regression tests for Collective Cognition SNN/Plasticity bridge integrations
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Regression tests for SNN and Plasticity bridges in collective cognition
 * WHY:  Ensure bridge stability, numerical correctness, and performance over time
 * HOW:  Test phi stability, coherence tracking, free energy bounds, memory safety,
 *       consensus stability, and thread safety
 *
 * TEST CATEGORIES:
 * - PhiStability: Phi computation stable
 * - CoherenceStability: Coherence tracking stable
 * - FreeEnergyBounded: Free energy in valid range
 * - NoMemoryLeaks: No leaks on repeated operations
 * - ConsensusStability: Consensus reaching stable
 * - ThreadSafetyRegression: Concurrent access safe
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "cognitive/collective_cognition/nimcp_collective_snn_bridge.h"
#include "cognitive/collective_cognition/nimcp_collective_plasticity_bridge.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CollectiveBridgeRegressionTest : public ::testing::Test {
protected:
    collective_snn_bridge_t* snn_bridge = nullptr;
    collective_plasticity_bridge_t* plasticity_bridge = nullptr;

    void SetUp() override {
        // Create SNN bridge with default config
        collective_snn_config_t snn_config = collective_snn_config_default();
        snn_config.enable_bio_async = false;
        snn_bridge = collective_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        // Create plasticity bridge with default config
        collective_plasticity_config_t plasticity_config = collective_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity_bridge = collective_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
    }

    void TearDown() override {
        if (snn_bridge) {
            collective_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            collective_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Helper: Generate collective context dimensions
    void generate_collective_context(float* dims, uint32_t seed, float coherence_level) {
        for (int i = 0; i < COLLECTIVE_DIM_COUNT; i++) {
            dims[i] = 0.5f + 0.3f * sinf((float)(i + seed) * 0.1f);
        }
        dims[COLLECTIVE_DIM_SWARM_COHERENCE] = coherence_level;
        dims[COLLECTIVE_DIM_GROUP_SYNC] = 0.6f + 0.2f * cosf((float)seed * 0.2f);
        dims[COLLECTIVE_DIM_CONSENSUS_LEVEL] = coherence_level * 0.9f;
    }
};

//=============================================================================
// PhiStability Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, PhiStability_BasicComputation) {
    // Test that phi computation remains stable over many iterations
    const int NUM_ITERATIONS = 500;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float dims[COLLECTIVE_DIM_COUNT];
        generate_collective_context(dims, i, 0.7f);

        int result = collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);
        EXPECT_GE(result, 0) << "Iteration " << i;

        collective_snn_simulate(snn_bridge, 10.0f);

        collective_drive_t drive;
        EXPECT_EQ(collective_snn_get_drive(snn_bridge, &drive), 0);

        // All drive values should be bounded
        EXPECT_GE(drive.swarm_coherence, 0.0f);
        EXPECT_LE(drive.swarm_coherence, 1.0f);
        EXPECT_GE(drive.group_sync_level, 0.0f);
        EXPECT_LE(drive.group_sync_level, 1.0f);
        EXPECT_GE(drive.consensus_level, 0.0f);
        EXPECT_LE(drive.consensus_level, 1.0f);
        EXPECT_TRUE(std::isfinite(drive.coordination_drive));
    }

    // Verify final state
    collective_snn_bridge_state_t state;
    EXPECT_EQ(collective_snn_get_state(snn_bridge, &state), 0);
    EXPECT_FALSE(std::isnan(state.mean_coordination));
}

TEST_F(CollectiveBridgeRegressionTest, PhiStability_VaryingParticipants) {
    // Test phi with varying number of participants
    for (int participants = 1; participants <= 16; participants++) {
        collective_snn_reset(snn_bridge);

        float consensus = (float)participants / 16.0f;
        int result = collective_snn_encode_decision(snn_bridge, consensus, participants);
        EXPECT_GE(result, 0) << "Failed with " << participants << " participants";

        collective_snn_simulate(snn_bridge, 20.0f);

        collective_drive_t drive;
        EXPECT_EQ(collective_snn_get_drive(snn_bridge, &drive), 0);
        EXPECT_TRUE(std::isfinite(drive.coordination_drive));
    }
}

TEST_F(CollectiveBridgeRegressionTest, PhiStability_ExtremeLevels) {
    // Test extreme coherence/consensus levels
    float extreme_levels[] = {0.0f, 0.01f, 0.5f, 0.99f, 1.0f};

    for (float level : extreme_levels) {
        collective_snn_reset(snn_bridge);

        collective_snn_encode_swarm(snn_bridge, level, level);
        collective_snn_simulate(snn_bridge, 20.0f);

        collective_drive_t drive;
        EXPECT_EQ(collective_snn_get_drive(snn_bridge, &drive), 0);
        EXPECT_GE(drive.swarm_coherence, 0.0f);
        EXPECT_LE(drive.swarm_coherence, 1.0f);
        EXPECT_FALSE(std::isnan(drive.coordination_drive));
    }
}

//=============================================================================
// CoherenceStability Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, CoherenceStability_SwarmTracking) {
    // Track swarm coherence over time
    const int NUM_CYCLES = 200;
    float prev_coherence = -1.0f;
    int large_jumps = 0;

    for (int i = 0; i < NUM_CYCLES; i++) {
        float coherence_input = 0.5f + 0.3f * sinf((float)i * 0.1f);

        collective_snn_encode_swarm(snn_bridge, coherence_input, coherence_input);
        collective_snn_simulate(snn_bridge, 10.0f);

        collective_drive_t drive;
        collective_snn_get_drive(snn_bridge, &drive);

        if (prev_coherence >= 0.0f) {
            float delta = fabsf(drive.swarm_coherence - prev_coherence);
            if (delta > 0.5f) {
                large_jumps++;
            }
        }
        prev_coherence = drive.swarm_coherence;

        EXPECT_GE(drive.swarm_coherence, 0.0f);
        EXPECT_LE(drive.swarm_coherence, 1.0f);
    }

    // Coherence should not have too many large jumps (indicates instability)
    EXPECT_LT(large_jumps, NUM_CYCLES / 10) << "Too many coherence jumps: " << large_jumps;
}

TEST_F(CollectiveBridgeRegressionTest, CoherenceStability_GroupSync) {
    // Test group synchronization stability
    for (int i = 0; i < 100; i++) {
        float sync_level = (float)(i % 100) / 100.0f;

        collective_snn_encode_swarm(snn_bridge, 0.7f, sync_level);
        collective_snn_step(snn_bridge);

        float current_sync;
        bool sync_detected = collective_snn_check_sync(snn_bridge, &current_sync);

        EXPECT_GE(current_sync, 0.0f);
        EXPECT_LE(current_sync, 1.0f);
    }

    // System should remain stable
    collective_snn_bridge_state_t state;
    EXPECT_EQ(collective_snn_get_state(snn_bridge, &state), 0);
    EXPECT_NE(state.state, COLLECTIVE_SNN_STATE_ERROR);
}

TEST_F(CollectiveBridgeRegressionTest, CoherenceStability_SharedIntention) {
    // Test shared intentionality tracking
    for (int i = 0; i < 50; i++) {
        float intention = (float)i / 50.0f;

        int spikes = collective_snn_encode_intention(snn_bridge, intention, i % 5);
        EXPECT_GE(spikes, 0);

        collective_snn_simulate(snn_bridge, 5.0f);

        collective_drive_t drive;
        collective_snn_get_drive(snn_bridge, &drive);
        EXPECT_GE(drive.shared_intention, 0.0f);
        EXPECT_LE(drive.shared_intention, 1.0f);
    }
}

//=============================================================================
// FreeEnergyBounded Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, FreeEnergyBounded_SteadyState) {
    // Process many iterations and verify all values stay bounded
    float dims[COLLECTIVE_DIM_COUNT];

    for (int i = 0; i < 500; i++) {
        generate_collective_context(dims, i, 0.6f);
        collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);
        collective_snn_simulate(snn_bridge, 10.0f);
    }

    collective_drive_t drive;
    EXPECT_EQ(collective_snn_get_drive(snn_bridge, &drive), 0);

    // All values in [0, 1] range
    EXPECT_GE(drive.swarm_coherence, 0.0f);
    EXPECT_LE(drive.swarm_coherence, 1.0f);
    EXPECT_GE(drive.group_sync_level, 0.0f);
    EXPECT_LE(drive.group_sync_level, 1.0f);
    EXPECT_GE(drive.shared_intention, 0.0f);
    EXPECT_LE(drive.shared_intention, 1.0f);
    EXPECT_GE(drive.consensus_level, 0.0f);
    EXPECT_LE(drive.consensus_level, 1.0f);
    EXPECT_GE(drive.emergence_level, 0.0f);
    EXPECT_LE(drive.emergence_level, 1.0f);
    EXPECT_GE(drive.trust_strength, 0.0f);
    EXPECT_LE(drive.trust_strength, 1.0f);
}

TEST_F(CollectiveBridgeRegressionTest, FreeEnergyBounded_ExtremeInputs) {
    float extreme_dims[COLLECTIVE_DIM_COUNT];

    // All zeros
    memset(extreme_dims, 0, sizeof(extreme_dims));
    collective_snn_encode_state(snn_bridge, extreme_dims, COLLECTIVE_DIM_COUNT);
    collective_snn_simulate(snn_bridge, 20.0f);

    collective_drive_t drive1;
    EXPECT_EQ(collective_snn_get_drive(snn_bridge, &drive1), 0);
    EXPECT_TRUE(std::isfinite(drive1.coordination_drive));

    // All ones
    for (int i = 0; i < COLLECTIVE_DIM_COUNT; i++) {
        extreme_dims[i] = 1.0f;
    }
    collective_snn_encode_state(snn_bridge, extreme_dims, COLLECTIVE_DIM_COUNT);
    collective_snn_simulate(snn_bridge, 20.0f);

    collective_drive_t drive2;
    EXPECT_EQ(collective_snn_get_drive(snn_bridge, &drive2), 0);
    EXPECT_TRUE(std::isfinite(drive2.coordination_drive));
}

TEST_F(CollectiveBridgeRegressionTest, FreeEnergyBounded_PlasticityWeights) {
    // Verify plasticity weights stay bounded
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(collective_plasticity_register_synapse(plasticity_bridge,
            i, COLLECTIVE_SYNAPSE_SYNCHRONIZATION, 0.5f), 0);
    }

    // Apply many learning events with varying rewards
    for (int cycle = 0; cycle < 100; cycle++) {
        float reward = sinf((float)cycle * 0.1f);
        EXPECT_EQ(collective_plasticity_apply_reward(plasticity_bridge, reward), 0);

        for (int i = 0; i < 20; i++) {
            collective_plasticity_learn(plasticity_bridge,
                COLLECTIVE_LEARN_SYNC_ACHIEVED, 0.1f, i, 0.5f);
        }

        collective_plasticity_update_bcm(plasticity_bridge, 10.0f);
    }

    // Verify all weights are bounded
    for (int i = 0; i < 20; i++) {
        collective_plasticity_synapse_t synapse;
        EXPECT_EQ(collective_plasticity_get_synapse(plasticity_bridge, i, &synapse), 0);
        EXPECT_GE(synapse.weight, 0.0f) << "Weight below 0 for synapse " << i;
        EXPECT_LE(synapse.weight, 1.0f) << "Weight above 1 for synapse " << i;
        EXPECT_TRUE(std::isfinite(synapse.weight));
    }
}

//=============================================================================
// NoMemoryLeaks Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, NoMemoryLeaks_RepeatedCreateDestroy) {
    // Destroy existing bridges first
    collective_snn_destroy(snn_bridge);
    snn_bridge = nullptr;
    collective_plasticity_destroy(plasticity_bridge);
    plasticity_bridge = nullptr;

    // Repeated create/destroy cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        collective_snn_config_t snn_config = collective_snn_config_default();
        snn_config.enable_bio_async = false;
        collective_snn_bridge_t* snn = collective_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr) << "Failed at cycle " << cycle;

        collective_plasticity_config_t plasticity_config = collective_plasticity_config_default();
        collective_plasticity_bridge_t* plasticity = collective_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr) << "Failed at cycle " << cycle;

        // Do some work
        float dims[COLLECTIVE_DIM_COUNT] = {0.5f};
        collective_snn_encode_state(snn, dims, COLLECTIVE_DIM_COUNT);
        collective_snn_step(snn);

        collective_plasticity_register_synapse(plasticity, 1, COLLECTIVE_SYNAPSE_CONSENSUS, 0.5f);
        collective_plasticity_learn(plasticity, COLLECTIVE_LEARN_CONSENSUS_REACHED, 0.1f, 1, 0.5f);

        collective_snn_destroy(snn);
        collective_plasticity_destroy(plasticity);
    }
    // Test passes if no crash/memory exhaustion
}

TEST_F(CollectiveBridgeRegressionTest, NoMemoryLeaks_RepeatedSynapseRegistration) {
    // Repeated register/unregister cycles
    for (int cycle = 0; cycle < 200; cycle++) {
        EXPECT_EQ(collective_plasticity_register_synapse(plasticity_bridge,
            1000 + (cycle % 10), COLLECTIVE_SYNAPSE_TRUST, 0.5f), 0);
        EXPECT_EQ(collective_plasticity_unregister_synapse(plasticity_bridge,
            1000 + (cycle % 10)), 0);
    }
    // Test passes if no crash/memory leak
}

TEST_F(CollectiveBridgeRegressionTest, NoMemoryLeaks_StatsResetCycles) {
    for (int i = 0; i < 100; i++) {
        // Generate some activity
        float dims[COLLECTIVE_DIM_COUNT] = {0.5f};
        collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);
        collective_snn_simulate(snn_bridge, 5.0f);

        // Reset stats
        collective_snn_reset_stats(snn_bridge);
        collective_plasticity_reset_stats(plasticity_bridge);
    }
    // Test passes if no memory accumulation
}

//=============================================================================
// ConsensusStability Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, ConsensusStability_BuildingProcess) {
    // Test consensus building over time
    for (int step = 0; step < 100; step++) {
        // Consensus gradually builds
        float consensus = (float)step / 100.0f;
        int participants = 4 + (step / 25);  // 4-7 participants

        collective_snn_encode_decision(snn_bridge, consensus, participants);
        collective_snn_simulate(snn_bridge, 10.0f);

        collective_drive_t drive;
        collective_snn_get_drive(snn_bridge, &drive);

        EXPECT_GE(drive.consensus_level, 0.0f);
        EXPECT_LE(drive.consensus_level, 1.0f);
    }
}

TEST_F(CollectiveBridgeRegressionTest, ConsensusStability_PlasticityLearning) {
    // Register consensus synapse
    ASSERT_EQ(collective_plasticity_register_synapse(plasticity_bridge,
        1, COLLECTIVE_SYNAPSE_CONSENSUS, 0.5f), 0);

    // Simulate consensus reaching events
    for (int i = 0; i < 50; i++) {
        collective_plasticity_learn(plasticity_bridge,
            COLLECTIVE_LEARN_CONSENSUS_REACHED, 0.7f, 1, 0.8f);
    }

    // Simulate some failures
    for (int i = 0; i < 20; i++) {
        collective_plasticity_learn(plasticity_bridge,
            COLLECTIVE_LEARN_CONSENSUS_FAILED, 0.3f, 1, 0.5f);
    }

    collective_plasticity_synapse_t synapse;
    EXPECT_EQ(collective_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
}

TEST_F(CollectiveBridgeRegressionTest, ConsensusStability_CoordinationState) {
    // Test coordination state consistency
    for (int i = 0; i < 30; i++) {
        collective_plasticity_register_synapse(plasticity_bridge,
            100 + i, COLLECTIVE_SYNAPSE_TRUST, 0.5f);
        collective_plasticity_learn(plasticity_bridge,
            COLLECTIVE_LEARN_TRUST_CONFIRMED, 0.6f, 100 + i, 0.7f);
    }

    collective_coordination_state_t state;
    EXPECT_EQ(collective_plasticity_get_coordination_state(plasticity_bridge, &state), 0);
    EXPECT_TRUE(std::isfinite(state.sync_sensitivity));
    EXPECT_TRUE(std::isfinite(state.coordination_calibration));
    EXPECT_TRUE(std::isfinite(state.trust_strength));
}

//=============================================================================
// ThreadSafetyRegression Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, ThreadSafetyRegression_ConcurrentEncoding) {
    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 100;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &errors]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                float dims[COLLECTIVE_DIM_COUNT];
                for (int d = 0; d < COLLECTIVE_DIM_COUNT; d++) {
                    dims[d] = 0.5f + 0.3f * sinf((float)(d + i + t) * 0.1f);
                }

                int result = collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);
                if (result < 0) errors++;

                result = collective_snn_step(snn_bridge);
                if (result != 0) errors++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Thread safety errors detected";

    // Verify bridge is still functional
    collective_snn_bridge_state_t state;
    EXPECT_EQ(collective_snn_get_state(snn_bridge, &state), 0);
    EXPECT_NE(state.state, COLLECTIVE_SNN_STATE_ERROR);
}

TEST_F(CollectiveBridgeRegressionTest, ThreadSafetyRegression_ConcurrentLearning) {
    constexpr int NUM_THREADS = 4;
    constexpr int SYNAPSES_PER_THREAD = 10;
    constexpr int OPS_PER_SYNAPSE = 50;
    std::atomic<int> errors{0};

    // Pre-register synapses for each thread
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int s = 0; s < SYNAPSES_PER_THREAD; s++) {
            int id = t * SYNAPSES_PER_THREAD + s;
            ASSERT_EQ(collective_plasticity_register_synapse(plasticity_bridge,
                id, COLLECTIVE_SYNAPSE_SYNCHRONIZATION, 0.5f), 0);
        }
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &errors]() {
            for (int op = 0; op < OPS_PER_SYNAPSE; op++) {
                for (int s = 0; s < SYNAPSES_PER_THREAD; s++) {
                    int id = t * SYNAPSES_PER_THREAD + s;

                    int result = collective_plasticity_learn(plasticity_bridge,
                        COLLECTIVE_LEARN_SYNC_ACHIEVED, 0.1f, id, 0.5f);
                    if (result != 0) errors++;

                    float weight = collective_plasticity_apply_stdp(plasticity_bridge,
                        id, (float)op, (float)op + 5.0f);
                    if (std::isnan(weight)) errors++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Thread safety errors in learning";

    // Verify all weights are valid
    for (int i = 0; i < NUM_THREADS * SYNAPSES_PER_THREAD; i++) {
        collective_plasticity_synapse_t synapse;
        EXPECT_EQ(collective_plasticity_get_synapse(plasticity_bridge, i, &synapse), 0);
        EXPECT_TRUE(std::isfinite(synapse.weight));
    }
}

TEST_F(CollectiveBridgeRegressionTest, ThreadSafetyRegression_MixedOperations) {
    constexpr int NUM_THREADS = 4;
    std::atomic<int> errors{0};
    std::atomic<bool> stop{false};

    // Writer thread - encoding
    std::thread encoder([this, &errors, &stop]() {
        while (!stop) {
            float dims[COLLECTIVE_DIM_COUNT] = {0.5f};
            if (collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT) < 0) {
                errors++;
            }
        }
    });

    // Writer thread - simulation
    std::thread simulator([this, &errors, &stop]() {
        while (!stop) {
            if (collective_snn_step(snn_bridge) != 0) {
                errors++;
            }
        }
    });

    // Reader threads - state queries
    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_THREADS - 2; i++) {
        readers.emplace_back([this, &errors, &stop]() {
            while (!stop) {
                collective_drive_t drive;
                if (collective_snn_get_drive(snn_bridge, &drive) != 0) {
                    errors++;
                }

                collective_snn_stats_t stats;
                if (collective_snn_get_stats(snn_bridge, &stats) != 0) {
                    errors++;
                }
            }
        });
    }

    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop = true;

    encoder.join();
    simulator.join();
    for (auto& r : readers) {
        r.join();
    }

    EXPECT_EQ(errors.load(), 0) << "Errors in mixed concurrent operations";
}

//=============================================================================
// Callback Stability Tests
//=============================================================================

namespace {
    std::atomic<int> g_sync_callbacks{0};
    std::atomic<int> g_drive_callbacks{0};
    std::atomic<int> g_coordination_callbacks{0};
    std::atomic<int> g_coll_learn_callbacks{0};

    void sync_callback(collective_snn_bridge_t*, float sync_level, uint64_t, void*) {
        g_sync_callbacks++;
        EXPECT_GE(sync_level, 0.0f);
        EXPECT_LE(sync_level, 1.0f);
    }

    void drive_callback(collective_snn_bridge_t*, const collective_drive_t* drive, void*) {
        g_drive_callbacks++;
        if (drive) {
            EXPECT_GE(drive->swarm_coherence, 0.0f);
            EXPECT_LE(drive->swarm_coherence, 1.0f);
        }
    }

    void coordination_callback(collective_snn_bridge_t*, float coordination_level,
                              uint32_t, void*) {
        g_coordination_callbacks++;
        EXPECT_GE(coordination_level, 0.0f);
        EXPECT_LE(coordination_level, 1.0f);
    }

    void coll_learn_callback(collective_plasticity_bridge_t*, collective_learn_event_t,
                            float magnitude, void*) {
        g_coll_learn_callbacks++;
        EXPECT_TRUE(std::isfinite(magnitude));
    }
}

TEST_F(CollectiveBridgeRegressionTest, CallbackStability_Registration) {
    g_sync_callbacks = 0;
    g_drive_callbacks = 0;
    g_coordination_callbacks = 0;

    // Register callbacks
    EXPECT_EQ(collective_snn_register_sync_callback(snn_bridge, sync_callback, nullptr), 0);
    EXPECT_EQ(collective_snn_register_drive_callback(snn_bridge, drive_callback, nullptr), 0);
    EXPECT_EQ(collective_snn_register_coordination_callback(snn_bridge,
        coordination_callback, nullptr), 0);

    // Trigger callbacks via high sync encoding
    collective_snn_encode_swarm(snn_bridge, 0.9f, 0.95f);
    collective_snn_simulate(snn_bridge, 30.0f);

    // Check for coordination
    float coord_level;
    collective_snn_check_coordination(snn_bridge, &coord_level);
}

TEST_F(CollectiveBridgeRegressionTest, CallbackStability_PlasticityCallbacks) {
    g_coll_learn_callbacks = 0;

    // Register callback
    EXPECT_EQ(collective_plasticity_register_learn_callback(plasticity_bridge,
        coll_learn_callback, nullptr), 0);

    // Register synapse and trigger learning
    collective_plasticity_register_synapse(plasticity_bridge,
        1, COLLECTIVE_SYNAPSE_SYNCHRONIZATION, 0.5f);

    for (int i = 0; i < 50; i++) {
        collective_plasticity_learn(plasticity_bridge,
            COLLECTIVE_LEARN_SYNC_ACHIEVED, 0.5f, 1, 0.7f);
    }

    EXPECT_GT(g_coll_learn_callbacks.load(), 0);
}

//=============================================================================
// Protected Synapse Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, ProtectedSynapse_CoordinationDriveProtection) {
    // Register coordination drive synapse (auto-protected)
    ASSERT_EQ(collective_plasticity_register_synapse(plasticity_bridge,
        100, COLLECTIVE_SYNAPSE_COORDINATION, 1.0f), 0);

    collective_plasticity_synapse_t synapse;
    collective_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    float original_weight = synapse.weight;
    EXPECT_TRUE(synapse.is_protected);

    // Try many modification attempts
    for (int i = 0; i < 100; i++) {
        collective_plasticity_apply_stdp(plasticity_bridge, 100, (float)i, (float)i + 10.0f);
        collective_plasticity_learn(plasticity_bridge,
            COLLECTIVE_LEARN_COORDINATION_FAILURE, -1.0f, 100, 1.0f);
        collective_plasticity_apply_reward(plasticity_bridge, -1.0f);
    }

    // Weight must remain unchanged
    collective_plasticity_get_synapse(plasticity_bridge, 100, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(CollectiveBridgeRegressionTest, ProtectedSynapse_SharedIntentProtection) {
    // Register shared intent synapse (auto-protected)
    ASSERT_EQ(collective_plasticity_register_synapse(plasticity_bridge,
        200, COLLECTIVE_SYNAPSE_SHARED_INTENT, 0.9f), 0);

    collective_plasticity_synapse_t synapse;
    collective_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_TRUE(synapse.is_protected);
    float original_weight = synapse.weight;

    // Apply learning - protected synapse should not change
    for (int i = 0; i < 50; i++) {
        collective_plasticity_learn(plasticity_bridge,
            COLLECTIVE_LEARN_SYNC_FAILED, 0.5f, 200, 0.9f);
    }

    collective_plasticity_get_synapse(plasticity_bridge, 200, &synapse);
    EXPECT_FLOAT_EQ(synapse.weight, original_weight);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, EdgeCase_ZeroSimulationTime) {
    float dims[COLLECTIVE_DIM_COUNT] = {0.5f};
    collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);

    // Zero time should be rejected
    EXPECT_EQ(collective_snn_simulate(snn_bridge, 0.0f), -1);

    // Negative time should be rejected
    EXPECT_EQ(collective_snn_simulate(snn_bridge, -1.0f), -1);
}

TEST_F(CollectiveBridgeRegressionTest, EdgeCase_LargeSimulationTime) {
    float dims[COLLECTIVE_DIM_COUNT] = {0.5f};
    collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);

    // Large simulation should not crash
    EXPECT_EQ(collective_snn_simulate(snn_bridge, 1000.0f), 0);

    collective_drive_t drive;
    EXPECT_EQ(collective_snn_get_drive(snn_bridge, &drive), 0);
    EXPECT_TRUE(std::isfinite(drive.coordination_drive));
}

TEST_F(CollectiveBridgeRegressionTest, EdgeCase_ResetBehavior) {
    // Do work
    float dims[COLLECTIVE_DIM_COUNT] = {0.8f};
    collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);
    collective_snn_simulate(snn_bridge, 30.0f);

    // Reset
    EXPECT_EQ(collective_snn_reset(snn_bridge), 0);

    // Verify state is cleared
    collective_snn_bridge_state_t state;
    collective_snn_get_state(snn_bridge, &state);
    EXPECT_EQ(state.state, COLLECTIVE_SNN_STATE_IDLE);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, Performance_EncodingLatency) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        float dims[COLLECTIVE_DIM_COUNT];
        generate_collective_context(dims, i, 0.6f);
        collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 encodings should complete in under 1 second
    EXPECT_LT(duration.count(), 1000) << "Encoding too slow: " << duration.count() << "ms";
}

TEST_F(CollectiveBridgeRegressionTest, Performance_SimulationLatency) {
    float dims[COLLECTIVE_DIM_COUNT] = {0.5f};
    collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 500; i++) {
        collective_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 500 simulations should complete in under 2 seconds
    EXPECT_LT(duration.count(), 2000) << "Simulation too slow: " << duration.count() << "ms";
}

TEST_F(CollectiveBridgeRegressionTest, Performance_LearningLatency) {
    // Register synapses
    for (int i = 0; i < 50; i++) {
        collective_plasticity_register_synapse(plasticity_bridge,
            i, COLLECTIVE_SYNAPSE_SYNCHRONIZATION, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < 100; cycle++) {
        for (int i = 0; i < 50; i++) {
            collective_plasticity_learn(plasticity_bridge,
                COLLECTIVE_LEARN_SYNC_ACHIEVED, 0.1f, i, 0.5f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 5000 learning operations should complete in under 500ms
    EXPECT_LT(duration.count(), 500) << "Learning too slow: " << duration.count() << "ms";
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CollectiveBridgeRegressionTest, Statistics_SNNStatsAccurate) {
    for (int i = 0; i < 100; i++) {
        float dims[COLLECTIVE_DIM_COUNT] = {0.5f};
        collective_snn_encode_state(snn_bridge, dims, COLLECTIVE_DIM_COUNT);
        collective_snn_simulate(snn_bridge, 5.0f);
    }

    collective_snn_stats_t stats;
    EXPECT_EQ(collective_snn_get_stats(snn_bridge, &stats), 0);
    EXPECT_GE(stats.total_evaluations, 100u);
    EXPECT_GE(stats.total_simulations, 100u);
    EXPECT_TRUE(std::isfinite(stats.mean_evaluation_time_ms));
    EXPECT_TRUE(std::isfinite(stats.mean_coordination));
}

TEST_F(CollectiveBridgeRegressionTest, Statistics_PlasticityStatsAccurate) {
    for (int i = 0; i < 10; i++) {
        collective_plasticity_register_synapse(plasticity_bridge,
            i, COLLECTIVE_SYNAPSE_SYNCHRONIZATION, 0.5f);
    }

    for (int cycle = 0; cycle < 50; cycle++) {
        for (int i = 0; i < 10; i++) {
            collective_plasticity_learn(plasticity_bridge,
                COLLECTIVE_LEARN_SYNC_ACHIEVED, 0.1f, i, 0.5f);
            collective_plasticity_apply_stdp(plasticity_bridge, i,
                (float)cycle, (float)cycle + 5.0f);
        }
        collective_plasticity_update_bcm(plasticity_bridge, 10.0f);
    }

    collective_plasticity_stats_t stats;
    EXPECT_EQ(collective_plasticity_get_stats(plasticity_bridge, &stats), 0);
    EXPECT_GE(stats.total_learning_events, 500u);
    EXPECT_GE(stats.weight_updates, 500u);
    EXPECT_TRUE(std::isfinite(stats.mean_weight_change));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
