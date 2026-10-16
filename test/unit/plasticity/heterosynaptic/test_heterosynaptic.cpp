/**
 * @file test_heterosynaptic.cpp
 * @brief Unit Tests for Heterosynaptic Plasticity
 * @version 1.0.0
 * @date 2025-12-19
 *
 * Test-driven development for heterosynaptic plasticity implementation
 *
 * BIOLOGICAL REFERENCES:
 * - Lynch et al. (1977): Intracellular injections block LTP
 * - Royer & Paré (2003): Heterosynaptic facilitation
 * - Chistiakova & Volgushev (2009): Heterosynaptic plasticity
 * - Bhatt et al. (2016): Dendritic spine dynamics
 */

#include <gtest/gtest.h>
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic_sleep_bridge.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic_immune_bridge.h"
#include <cmath>

//=============================================================================
// Test Fixtures
//=============================================================================

class HeterosynapticTest : public ::testing::Test {
protected:
    hetero_system_t* system;
    hetero_config_t config;

    void SetUp() override {
        /* Initialize with defaults */
        hetero_default_config(&config);
        system = hetero_create(&config, 100);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        hetero_destroy(system);
    }

    /* Helper to add synapse at position */
    void add_synapse_at(float x, float y, float z, float weight, uint32_t id) {
        hetero_spatial_coords_t pos = {x, y, z};
        int result = hetero_add_synapse(system, &pos, weight, id, 0);
        ASSERT_EQ(result, 0);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HeterosynapticTest, CreateAndDestroy) {
    /* WHAT: Test system creation and destruction
     * WHY:  Verify memory management
     */
    EXPECT_NE(system, nullptr);
    /* TearDown handles destruction */
}

TEST_F(HeterosynapticTest, DefaultConfig) {
    /* WHAT: Test default configuration
     * WHY:  Verify biologically plausible defaults
     */
    hetero_config_t cfg;
    int result = hetero_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(cfg.neighbor_radius, HETERO_DEFAULT_NEIGHBOR_RADIUS);
    EXPECT_FLOAT_EQ(cfg.depression_factor, HETERO_DEFAULT_DEPRESSION_FACTOR);
    EXPECT_FLOAT_EQ(cfg.decay_lambda, HETERO_DEFAULT_DECAY_LAMBDA);
    EXPECT_TRUE(cfg.enable_competition);
}

TEST_F(HeterosynapticTest, CreateWithNullConfig) {
    /* WHAT: Test creation with NULL config uses defaults
     * WHY:  Convenience for simple use cases
     */
    hetero_system_t* sys = hetero_create(nullptr, 50);
    ASSERT_NE(sys, nullptr);
    hetero_destroy(sys);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(HeterosynapticTest, AddSynapse) {
    /* WHAT: Test adding synapse to system
     * WHY:  Core functionality for building network
     */
    hetero_spatial_coords_t pos = {10.0f, 5.0f, 0.0f};
    int result = hetero_add_synapse(system, &pos, 0.5f, 1, 0);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(system->num_synapses, 1);
}

TEST_F(HeterosynapticTest, AddMultipleSynapses) {
    /* WHAT: Test adding multiple synapses
     * WHY:  Verify array growth
     */
    for (uint32_t i = 0; i < 10; i++) {
        hetero_spatial_coords_t pos = {(float)i, 0.0f, 0.0f};
        int result = hetero_add_synapse(system, &pos, 0.5f, i, 0);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(system->num_synapses, 10);
}

TEST_F(HeterosynapticTest, GetSynapse) {
    /* WHAT: Test retrieving synapse by ID
     * WHY:  Verify lookup functionality
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.6f, 42);

    hetero_synapse_t* syn = hetero_get_synapse(system, 42);
    ASSERT_NE(syn, nullptr);
    EXPECT_EQ(syn->synapse_id, 42);
    EXPECT_FLOAT_EQ(syn->weight, 0.6f);
}

TEST_F(HeterosynapticTest, GetNonexistentSynapse) {
    /* WHAT: Test retrieving synapse that doesn't exist
     * WHY:  Verify error handling
     */
    hetero_synapse_t* syn = hetero_get_synapse(system, 999);
    EXPECT_EQ(syn, nullptr);
}

TEST_F(HeterosynapticTest, RemoveSynapse) {
    /* WHAT: Test removing synapse
     * WHY:  Support synaptic pruning
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);
    add_synapse_at(10.0f, 0.0f, 0.0f, 0.5f, 2);

    int result = hetero_remove_synapse(system, 1);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(system->num_synapses, 1);

    hetero_synapse_t* syn = hetero_get_synapse(system, 1);
    EXPECT_EQ(syn, nullptr);
}

//=============================================================================
// Spatial Query Tests
//=============================================================================

TEST_F(HeterosynapticTest, ComputeDistance) {
    /* WHAT: Test distance computation
     * WHY:  Fundamental for spatial competition
     */
    hetero_spatial_coords_t pos1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos2 = {3.0f, 4.0f, 0.0f};

    float dist = hetero_compute_distance(&pos1, &pos2);
    EXPECT_FLOAT_EQ(dist, 5.0f);  /* 3-4-5 triangle */
}

TEST_F(HeterosynapticTest, ComputeDistance3D) {
    /* WHAT: Test 3D distance computation
     * WHY:  Synapses exist in 3D space
     */
    hetero_spatial_coords_t pos1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos2 = {1.0f, 1.0f, 1.0f};

    float dist = hetero_compute_distance(&pos1, &pos2);
    EXPECT_FLOAT_EQ(dist, sqrtf(3.0f));
}

TEST_F(HeterosynapticTest, ComputeDepressionFactor) {
    /* WHAT: Test exponential spatial decay
     * WHY:  Distance-dependent depression (Lynch et al.)
     */
    float distance = 10.0f;
    float depression = 0.4f;
    float lambda = 10.0f;

    float factor = hetero_compute_depression_factor(distance, depression, lambda);

    /* exp(-10/10) = exp(-1) = 0.368, factor = 0.4 * 0.368 = 0.147 */
    EXPECT_NEAR(factor, 0.147f, 0.001f);
}

TEST_F(HeterosynapticTest, DepressionFactorDecaysWithDistance) {
    /* WHAT: Test depression decreases with distance
     * WHY:  Biological spatial decay (Bhatt et al.)
     */
    float d1 = hetero_compute_depression_factor(5.0f, 0.4f, 10.0f);
    float d2 = hetero_compute_depression_factor(10.0f, 0.4f, 10.0f);
    float d3 = hetero_compute_depression_factor(20.0f, 0.4f, 10.0f);

    EXPECT_GT(d1, d2);
    EXPECT_GT(d2, d3);
}

TEST_F(HeterosynapticTest, FindNeighbors) {
    /* WHAT: Test finding synapses within radius
     * WHY:  Identify competition set
     */
    /* Add synapses at different distances from origin */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);  /* 0 μm */
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.5f, 2);  /* 5 μm */
    add_synapse_at(10.0f, 0.0f, 0.0f, 0.5f, 3); /* 10 μm */
    add_synapse_at(20.0f, 0.0f, 0.0f, 0.5f, 4); /* 20 μm */

    hetero_spatial_coords_t center = {0.0f, 0.0f, 0.0f};
    hetero_synapse_t* neighbors[10];
    size_t num_found = 0;

    int result = hetero_find_neighbors(system, &center, 12.0f, neighbors, 10, &num_found);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_found, 2);  /* Synapses 2 and 3 (exclude self) */
}

TEST_F(HeterosynapticTest, FindNeighborsExcludesSelf) {
    /* WHAT: Test neighbor finding excludes center synapse
     * WHY:  Synapse shouldn't depress itself
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.5f, 2);

    hetero_spatial_coords_t center = {0.0f, 0.0f, 0.0f};
    hetero_synapse_t* neighbors[10];
    size_t num_found = 0;

    hetero_find_neighbors(system, &center, 10.0f, neighbors, 10, &num_found);

    EXPECT_EQ(num_found, 1);  /* Only synapse 2 */
    EXPECT_EQ(neighbors[0]->synapse_id, 2);
}

//=============================================================================
// Heterosynaptic Depression Tests
//=============================================================================

TEST_F(HeterosynapticTest, ApplyDepressionToNeighbors) {
    /* WHAT: Test heterosynaptic depression application
     * WHY:  Core mechanism (Lynch et al., Royer & Paré)
     */
    /* Add center synapse and neighbor */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);
    add_synapse_at(10.0f, 0.0f, 0.0f, 0.8f, 2);

    /* Potentiate center synapse */
    float ltp_amount = 0.2f;
    int result = hetero_apply_depression(system, 1, ltp_amount, 0);

    EXPECT_EQ(result, 0);

    /* Check neighbor was depressed */
    hetero_synapse_t* neighbor = hetero_get_synapse(system, 2);
    EXPECT_LT(neighbor->weight, 0.8f);
    EXPECT_GT(neighbor->num_neighbor_depressions, 0);
}

TEST_F(HeterosynapticTest, DepressionProportionalToLTP) {
    /* WHAT: Test depression scales with LTP amount
     * WHY:  Δw_j ∝ Δw_i
     */
    /* Small LTP - use separate system */
    hetero_system_t* sys1 = hetero_create(&config, 10);
    ASSERT_NE(sys1, nullptr);
    hetero_spatial_coords_t pos1_1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos1_2 = {10.0f, 0.0f, 0.0f};
    hetero_add_synapse(sys1, &pos1_1, 0.5f, 1, 0);
    hetero_add_synapse(sys1, &pos1_2, 0.8f, 2, 0);
    hetero_apply_depression(sys1, 1, 0.1f, 0);
    hetero_synapse_t* syn1 = hetero_get_synapse(sys1, 2);
    ASSERT_NE(syn1, nullptr);
    float weight1 = syn1->weight;

    /* Large LTP - use separate system */
    hetero_system_t* sys2 = hetero_create(&config, 10);
    ASSERT_NE(sys2, nullptr);
    hetero_spatial_coords_t pos2_1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos2_2 = {10.0f, 0.0f, 0.0f};
    hetero_add_synapse(sys2, &pos2_1, 0.5f, 1, 0);
    hetero_add_synapse(sys2, &pos2_2, 0.8f, 2, 0);
    hetero_apply_depression(sys2, 1, 0.3f, 0);
    hetero_synapse_t* syn2 = hetero_get_synapse(sys2, 2);
    ASSERT_NE(syn2, nullptr);
    float weight2 = syn2->weight;

    /* Larger LTP → more depression */
    EXPECT_LT(weight2, weight1);

    hetero_destroy(sys1);
    hetero_destroy(sys2);
}

TEST_F(HeterosynapticTest, DepressionStrongerForCloserSynapses) {
    /* WHAT: Test distance-dependent depression
     * WHY:  Exponential spatial decay (Bhatt et al.)
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.8f, 2);  /* Close */
    add_synapse_at(15.0f, 0.0f, 0.0f, 0.8f, 3); /* Far */

    hetero_apply_depression(system, 1, 0.2f, 0);

    float weight_close = hetero_get_synapse(system, 2)->weight;
    float weight_far = hetero_get_synapse(system, 3)->weight;

    /* Closer synapse depressed more */
    EXPECT_LT(weight_close, weight_far);
}

TEST_F(HeterosynapticTest, DepressionRespectsBounds) {
    /* WHAT: Test depression doesn't violate w_min
     * WHY:  Biological weight constraints
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.05f, 2);  /* Near minimum */

    hetero_apply_depression(system, 1, 0.5f, 0);

    hetero_synapse_t* neighbor = hetero_get_synapse(system, 2);
    EXPECT_GE(neighbor->weight, neighbor->w_min);
}

//=============================================================================
// Winner-Take-All Tests
//=============================================================================

TEST_F(HeterosynapticTest, WinnerTakeAllSelectsStrongest) {
    /* WHAT: Test WTA selects synapse with highest weight
     * WHY:  Input selectivity (Chistiakova & Volgushev)
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.9f, 1);  /* Strongest */
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.7f, 2);
    add_synapse_at(10.0f, 0.0f, 0.0f, 0.6f, 3);

    hetero_spatial_coords_t center = {5.0f, 0.0f, 0.0f};
    hetero_competition_result_t result;

    int status = hetero_winner_take_all(system, &center, 15.0f, &result);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(result.winner_id, 1);
    EXPECT_GT(result.num_competitors, 0);

    hetero_free_competition_result(&result);
}

TEST_F(HeterosynapticTest, WinnerTakeAllDepressesLosers) {
    /* WHAT: Test WTA suppresses non-winners
     * WHY:  Winner-take-all dynamics
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.9f, 1);  /* Winner */
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.7f, 2);  /* Loser */
    add_synapse_at(10.0f, 0.0f, 0.0f, 0.6f, 3); /* Loser */

    float weight2_before = hetero_get_synapse(system, 2)->weight;
    float weight3_before = hetero_get_synapse(system, 3)->weight;

    /* Use center offset from all synapse positions to include all in competition
     * (hetero_find_neighbors excludes synapses at distance 0) */
    hetero_spatial_coords_t center = {4.0f, 0.0f, 0.0f};
    hetero_competition_result_t result;
    hetero_winner_take_all(system, &center, 15.0f, &result);

    float weight2_after = hetero_get_synapse(system, 2)->weight;
    float weight3_after = hetero_get_synapse(system, 3)->weight;

    EXPECT_LT(weight2_after, weight2_before);
    EXPECT_LT(weight3_after, weight3_before);

    hetero_free_competition_result(&result);
}

TEST_F(HeterosynapticTest, WinnerTakeAllUpdatesStatistics) {
    /* WHAT: Test WTA updates win/competition counts
     * WHY:  Monitor competition dynamics
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.9f, 1);
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.7f, 2);

    hetero_spatial_coords_t center = {2.5f, 0.0f, 0.0f};
    hetero_competition_result_t result;
    hetero_winner_take_all(system, &center, 15.0f, &result);

    hetero_synapse_t* winner = hetero_get_synapse(system, result.winner_id);
    EXPECT_GT(winner->num_wins, 0);
    EXPECT_GT(winner->num_competitions, 0);

    hetero_free_competition_result(&result);
}

//=============================================================================
// Sleep Modulation Tests
//=============================================================================

TEST_F(HeterosynapticTest, SetSleepState) {
    /* WHAT: Test setting sleep state
     * WHY:  Sleep modulates competition
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);

    int result = hetero_set_sleep_state(system, SLEEP_STATE_DEEP_NREM);
    EXPECT_EQ(result, 0);

    hetero_synapse_t* syn = hetero_get_synapse(system, 1);
    EXPECT_EQ(syn->current_sleep_state, SLEEP_STATE_DEEP_NREM);
}

TEST_F(HeterosynapticTest, SleepModulatesDepressionFactor) {
    /* WHAT: Test sleep state reduces competition
     * WHY:  Consolidation requires reduced pruning
     */
    /* Need at least one synapse - sleep modulation uses first synapse's state */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);

    float base_factor = 0.4f;

    /* Awake: full competition */
    hetero_set_sleep_state(system, SLEEP_STATE_AWAKE);
    float awake_factor = hetero_get_sleep_modulated_factor(system, base_factor);

    /* Deep NREM: reduced competition */
    hetero_set_sleep_state(system, SLEEP_STATE_DEEP_NREM);
    float nrem_factor = hetero_get_sleep_modulated_factor(system, base_factor);

    EXPECT_LT(nrem_factor, awake_factor);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HeterosynapticTest, GetStatistics) {
    /* WHAT: Test retrieving system statistics
     * WHY:  Monitor competition dynamics
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.7f, 2);

    hetero_apply_depression(system, 1, 0.2f, 0);

    uint64_t competitions, depressions;
    float avg_neighbors;
    hetero_get_statistics(system, &competitions, &depressions, &avg_neighbors);

    EXPECT_GT(depressions, 0);
}

TEST_F(HeterosynapticTest, ResetStatistics) {
    /* WHAT: Test resetting statistics
     * WHY:  Start fresh tracking
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);
    add_synapse_at(5.0f, 0.0f, 0.0f, 0.7f, 2);

    hetero_apply_depression(system, 1, 0.2f, 0);

    hetero_reset_statistics(system);

    uint64_t competitions, depressions;
    float avg_neighbors;
    hetero_get_statistics(system, &competitions, &depressions, &avg_neighbors);

    EXPECT_EQ(competitions, 0);
    EXPECT_EQ(depressions, 0);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(HeterosynapticTest, ConnectBioAsync) {
    /* WHAT: Test bio-async connection
     * WHY:  Enable inter-module messaging
     */
    int result = hetero_connect_bio_async(system);
    /* May fail if bio-router not available, which is OK */
    EXPECT_TRUE(result == 0 || result == NIMCP_ERROR_OPERATION_FAILED);

    if (result == 0) {
        EXPECT_TRUE(hetero_is_bio_async_connected(system));
        hetero_disconnect_bio_async(system);
    }
}

TEST_F(HeterosynapticTest, DisconnectBioAsync) {
    /* WHAT: Test bio-async disconnection
     * WHY:  Clean shutdown
     */
    hetero_connect_bio_async(system);
    int result = hetero_disconnect_bio_async(system);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(hetero_is_bio_async_connected(system));
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(HeterosynapticTest, NullPointerChecks) {
    /* WHAT: Test NULL pointer handling
     * WHY:  Robust error handling
     */
    EXPECT_EQ(hetero_default_config(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(hetero_add_synapse(nullptr, nullptr, 0, 0, 0), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(hetero_get_synapse(nullptr, 0), nullptr);
}

TEST_F(HeterosynapticTest, InvalidParameters) {
    /* WHAT: Test invalid parameter handling
     * WHY:  Prevent undefined behavior
     */
    add_synapse_at(0.0f, 0.0f, 0.0f, 0.5f, 1);

    /* Negative LTP amount */
    int result = hetero_apply_depression(system, 1, -0.2f, 0);
    EXPECT_NE(result, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
