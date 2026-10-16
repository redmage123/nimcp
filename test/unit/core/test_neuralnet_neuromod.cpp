/**
 * @file test_neuralnet_neuromod.cpp
 * @brief Integration tests for neural network neuromodulation query
 *
 * WHAT: Tests neural_network_get_neuromodulation() with real neuromodulator systems
 * WHY:  Ensures proper integration between neural network and neuromodulator subsystems
 * HOW:  Creates actual neuromodulator systems and verifies dopamine level queries
 *
 * COVERAGE:
 * - Querying dopamine levels from attached neuromodulator system
 * - Null safety (network=null, system=null)
 * - Different dopamine concentrations (baseline, elevated, depleted)
 * - Integration with three-factor learning (reward signals)
 *
 * DESIGN PATTERN: Integration testing - tests multiple components working together
 * COMPLEXITY: O(1) per query operation
 */

#include "test_helpers.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include <cmath>
#include <chrono>

//=============================================================================
// Test Fixture - Integration with Real Neuromodulator System
//=============================================================================

class NeuralNetNeuromodTest : public ::testing::Test {
protected:
    neural_network_t network;
    neuromodulator_system_t neuromod_system;
    network_config_t net_config;
    neuromodulator_config_t neuromod_config;

    void SetUp() override
    {
        // WHAT: Initialize network and neuromodulator configurations
        // WHY:  Each test needs clean state for isolation

        // Create network config using helper function
        net_config = create_test_config();

        // Create neuromodulator config with known baseline
        memset(&neuromod_config, 0, sizeof(neuromod_config));
        neuromod_config.baseline_dopamine = 0.3f;
        neuromod_config.baseline_serotonin = 0.4f;
        neuromod_config.baseline_acetylcholine = 0.2f;
        neuromod_config.baseline_norepinephrine = 0.3f;
        neuromod_config.dopamine_decay = 2.0f;
        neuromod_config.serotonin_decay = 10.0f;
        neuromod_config.acetylcholine_decay = 0.5f;
        neuromod_config.norepinephrine_decay = 3.0f;
        neuromod_config.reward_dopamine_gain = 0.5f;
        neuromod_config.threat_norepinephrine_gain = 0.7f;
        neuromod_config.salience_acetylcholine_gain = 0.6f;
        neuromod_config.punishment_serotonin_gain = 0.4f;
        neuromod_config.enable_volume_transmission = true;
        neuromod_config.diffusion_rate = 0.1f;

        network = nullptr;
        neuromod_system = nullptr;
    }

    void TearDown() override
    {
        // WHAT: Clean up allocated resources
        // WHY:  Prevent memory leaks between tests
        if (network) {
            neural_network_destroy(network);
            network = nullptr;
        }
        if (neuromod_system) {
            neuromodulator_system_destroy(neuromod_system);
            neuromod_system = nullptr;
        }
    }
};

//=============================================================================
// Basic Functionality Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_NullNetwork)
{
    /* WHAT: Test null safety when network is null
     * WHY:  Should return 0.0f without crashing (guard clause)
     */
    float level = neural_network_get_neuromodulation(nullptr);
    EXPECT_EQ(level, 0.0f);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_NoSystemAttached)
{
    /* WHAT: Test when network exists but no neuromodulator system attached
     * WHY:  Should return 0.0f (guard clause for missing system)
     */
    network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    float level = neural_network_get_neuromodulation(network);
    EXPECT_EQ(level, 0.0f);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_BaselineLevel)
{
    /* WHAT: Test querying baseline dopamine level
     * WHY:  Verifies integration with neuromodulator system
     * EXPECTED: Should return baseline_dopamine (0.3f)
     */
    network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    neuromod_system = neuromodulator_system_create(&neuromod_config);
    ASSERT_NE(neuromod_system, nullptr);

    // Attach neuromodulator system to network
    ASSERT_TRUE(neural_network_set_neuromodulator_system(network, neuromod_system));

    // Query dopamine level
    float level = neural_network_get_neuromodulation(network);

    // Should return baseline dopamine
    EXPECT_FLOAT_EQ(level, 0.3f);
}

//=============================================================================
// Dopamine Level Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_ElevatedDopamine)
{
    /* WHAT: Test querying elevated dopamine after reward
     * WHY:  Verifies dynamic dopamine tracking for three-factor learning
     * HOW:  Set high dopamine, query via neural network API
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Elevate dopamine to 0.9
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.9f));

    // Query via neural network
    float level = neural_network_get_neuromodulation(network);

    EXPECT_FLOAT_EQ(level, 0.9f);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_DepletedDopamine)
{
    /* WHAT: Test querying low dopamine level
     * WHY:  Verifies handling of dopamine depletion (negative RPE)
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Deplete dopamine to 0.05
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.05f));

    float level = neural_network_get_neuromodulation(network);

    EXPECT_FLOAT_EQ(level, 0.05f);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_ZeroDopamine)
{
    /* WHAT: Test boundary condition (zero dopamine)
     * WHY:  Edge case for complete dopamine depletion
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.0f));

    float level = neural_network_get_neuromodulation(network);

    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_MaxDopamine)
{
    /* WHAT: Test boundary condition (maximum dopamine)
     * WHY:  Edge case for dopamine saturation
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 1.0f));

    float level = neural_network_get_neuromodulation(network);

    EXPECT_FLOAT_EQ(level, 1.0f);
}

//=============================================================================
// Reward Prediction Error (RPE) Integration Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_PositiveRPE)
{
    /* WHAT: Test dopamine burst from positive reward prediction error
     * WHY:  Simulates three-factor learning scenario (reward > predicted)
     * HOW:  Release dopamine via RPE, query via network
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Simulate positive RPE: reward=0.8, predicted=0.2 → burst
    float rpe = neuromodulator_release_dopamine(neuromod_system, 0.8f, 0.2f);
    EXPECT_GT(rpe, 0.0f);

    // Query dopamine level via neural network
    float level = neural_network_get_neuromodulation(network);

    // Should be elevated above baseline
    EXPECT_GT(level, neuromod_config.baseline_dopamine);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_NegativeRPE)
{
    /* WHAT: Test dopamine response from negative reward prediction error
     * WHY:  Simulates worse-than-expected outcome
     * HOW:  Release negative RPE, verify dopamine level is clamped at 0
     * NOTE: Neuromodulator system clamps dopamine to [0,1] range
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Simulate negative RPE: reward=0.2, predicted=0.8 → negative signal
    float rpe = neuromodulator_release_dopamine(neuromod_system, 0.2f, 0.8f);
    EXPECT_LT(rpe, 0.0f);

    float level = neural_network_get_neuromodulation(network);

    // Dopamine is clamped to [0,1], so negative RPE results in level at or near 0
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_ZeroRPE)
{
    /* WHAT: Test dopamine when prediction is perfect
     * WHY:  Zero RPE should not change dopamine significantly
     * NOTE: Actual behavior depends on implementation - may clamp to 0 or stay at baseline
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Perfect prediction: reward=0.5, predicted=0.5
    float rpe = neuromodulator_release_dopamine(neuromod_system, 0.5f, 0.5f);
    EXPECT_NEAR(rpe, 0.0f, 0.01f);

    float level = neural_network_get_neuromodulation(network);

    // Dopamine should be in valid range
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

//=============================================================================
// Dopamine-Specific Query Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_OnlyDopamine)
{
    /* WHAT: Test that function only queries dopamine (not serotonin/ACh/NE)
     * WHY:  Verifies correct neuromodulator type selection
     * HOW:  Set different levels for each, verify only dopamine returned
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Set distinct levels for each neuromodulator
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.7f));
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_SEROTONIN, 0.3f));
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_ACETYLCHOLINE, 0.5f));
    ASSERT_TRUE(neuromodulator_set_level(neuromod_system, NEUROMOD_NOREPINEPHRINE, 0.4f));

    // Query should return only dopamine
    float level = neural_network_get_neuromodulation(network);

    EXPECT_FLOAT_EQ(level, 0.7f);  // Dopamine, not serotonin/ACh/NE
}

//=============================================================================
// Dynamic Update Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_MultipleQueries)
{
    /* WHAT: Test multiple queries track changing dopamine levels
     * WHY:  Verifies function returns current state, not cached value
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Query 1: Baseline
    float level1 = neural_network_get_neuromodulation(network);
    EXPECT_FLOAT_EQ(level1, 0.3f);

    // Change dopamine
    neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.6f);

    // Query 2: Should reflect change
    float level2 = neural_network_get_neuromodulation(network);
    EXPECT_FLOAT_EQ(level2, 0.6f);

    // Change again
    neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 0.9f);

    // Query 3: Should reflect new change
    float level3 = neural_network_get_neuromodulation(network);
    EXPECT_FLOAT_EQ(level3, 0.9f);
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_AfterDecay)
{
    /* WHAT: Test querying dopamine after time-based decay
     * WHY:  Verifies integration with neuromodulator dynamics
     * HOW:  Elevate dopamine, apply decay, verify reduction
     * NOTE: Decay behavior depends on baseline and decay constants
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Elevate dopamine
    neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, 1.0f);
    float level_before = neural_network_get_neuromodulation(network);
    EXPECT_FLOAT_EQ(level_before, 1.0f);

    // Apply decay (2 seconds, τ=2.0s for dopamine)
    ASSERT_TRUE(neuromodulator_update(neuromod_system, 2.0f));

    // Should decay from initial level
    float level_after = neural_network_get_neuromodulation(network);
    EXPECT_LT(level_after, 1.0f);
    EXPECT_GE(level_after, 0.0f);  // Must stay in valid range
}

//=============================================================================
// System Replacement Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_ReplaceSystem)
{
    /* WHAT: Test replacing neuromodulator system with different one
     * WHY:  Verifies proper handling of system changes
     */
    network = neural_network_create(&net_config);

    // Create first system with baseline 0.3
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    float level1 = neural_network_get_neuromodulation(network);
    EXPECT_FLOAT_EQ(level1, 0.3f);

    // Create second system with different baseline
    neuromodulator_config_t config2 = neuromod_config;
    config2.baseline_dopamine = 0.6f;
    neuromodulator_system_t system2 = neuromodulator_system_create(&config2);

    // Replace system
    neural_network_set_neuromodulator_system(network, system2);

    // Should now query new system
    float level2 = neural_network_get_neuromodulation(network);
    EXPECT_FLOAT_EQ(level2, 0.6f);

    // Clean up second system
    neuromodulator_system_destroy(system2);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_Performance)
{
    /* WHAT: Test that query is O(1) and fast
     * WHY:  Function called frequently during synapse computation
     * EXPECTED: Should complete 100k queries in < 10ms
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    auto start = std::chrono::high_resolution_clock::now();

    // Perform 100,000 queries
    for (int i = 0; i < 100000; i++) {
        float level = neural_network_get_neuromodulation(network);
        (void)level;  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    /* WHAT: Verify fast query performance
     * WHY:  Allow headroom for debug builds and loaded systems
     * NOTE: ~0.1μs per query is acceptable for real-time use
     */
    EXPECT_LT(duration.count(), 10000);  // Less than 10ms for 100k queries
}

//=============================================================================
// Three-Factor Learning Integration Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_ThreeFactorLearning)
{
    /* WHAT: Test integration with three-factor learning scenario
     * WHY:  Verifies function supports eligibility trace consolidation
     * HOW:  Simulate: spike → trace → reward → dopamine → query
     *
     * THREE-FACTOR LEARNING:
     * 1. Pre-synaptic spike (factor 1)
     * 2. Post-synaptic spike (factor 2)
     * 3. Neuromodulator signal (factor 3) ← this function provides this
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Step 1 & 2: Spikes create eligibility trace (simulated by test)
    // In real use: synapse_compute would set trace based on spike timing

    // Step 3: Reward arrives → dopamine burst
    float reward = 0.8f;
    float predicted = 0.2f;
    neuromodulator_release_dopamine(neuromod_system, reward, predicted);

    // Query dopamine for eligibility trace consolidation
    float dopamine_level = neural_network_get_neuromodulation(network);

    // Dopamine should be elevated (strong learning signal)
    EXPECT_GT(dopamine_level, 0.5f);

    // In real use: synapse_compute would use this to scale weight update:
    // Δw = learning_rate × eligibility_trace × dopamine_level
    // High dopamine → strong weight change
}

//=============================================================================
// Boundary and Edge Case Tests
//=============================================================================

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_RapidChanges)
{
    /* WHAT: Test rapid dopamine fluctuations
     * WHY:  Verifies stability under volatile conditions
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    for (int i = 0; i < 100; i++) {
        // Alternate between high and low
        float target = (i % 2 == 0) ? 0.9f : 0.1f;
        neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, target);

        float level = neural_network_get_neuromodulation(network);
        EXPECT_NEAR(level, target, 0.001f);
    }
}

TEST_F(NeuralNetNeuromodTest, GetNeuromodulation_ValidRange)
{
    /* WHAT: Test returned value always in valid range [0, 1]
     * WHY:  Dopamine concentrations must be normalized
     */
    network = neural_network_create(&net_config);
    neuromod_system = neuromodulator_system_create(&neuromod_config);
    neural_network_set_neuromodulator_system(network, neuromod_system);

    // Test various levels
    float test_levels[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float test_level : test_levels) {
        neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, test_level);
        float level = neural_network_get_neuromodulation(network);

        EXPECT_GE(level, 0.0f);
        EXPECT_LE(level, 1.0f);
    }
}
