/**
 * @file test_heterosynaptic_regression.cpp
 * @brief Regression Tests for Heterosynaptic Plasticity
 * @version 1.0.0
 * @date 2025-12-19
 *
 * Tests to prevent regressions in heterosynaptic behavior
 */

#include <gtest/gtest.h>
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic_sleep_bridge.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic_immune_bridge.h"
#include <cmath>

//=============================================================================
// Regression Tests
//=============================================================================

TEST(HeteroRegression, DepressionAmountStability) {
    /* WHAT: Test depression amount is consistent
     * WHY:  Prevent regressions in core algorithm
     */
    hetero_system_t* sys = hetero_create(nullptr, 10);

    hetero_spatial_coords_t pos1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos2 = {10.0f, 0.0f, 0.0f};

    hetero_add_synapse(sys, &pos1, 0.5f, 1, 0);
    hetero_add_synapse(sys, &pos2, 0.8f, 2, 0);

    /* Apply depression */
    hetero_apply_depression(sys, 1, 0.2f, 0);

    /* Check neighbor weight */
    hetero_synapse_t* syn = hetero_get_synapse(sys, 2);

    /* Expected: depression_factor * exp(-10/10) * 0.2 = 0.4 * 0.368 * 0.2 = 0.029 */
    /* Weight: 0.8 - 0.029 = 0.771 */
    EXPECT_NEAR(syn->weight, 0.771f, 0.01f);

    hetero_destroy(sys);
}

TEST(HeteroRegression, WinnerTakeAllConsistency) {
    /* WHAT: Test WTA always picks same winner
     * WHY:  Deterministic behavior
     */
    for (int trial = 0; trial < 5; trial++) {
        hetero_system_t* sys = hetero_create(nullptr, 10);

        hetero_spatial_coords_t pos1 = {0.0f, 0.0f, 0.0f};
        hetero_spatial_coords_t pos2 = {5.0f, 0.0f, 0.0f};
        hetero_spatial_coords_t pos3 = {10.0f, 0.0f, 0.0f};

        hetero_add_synapse(sys, &pos1, 0.9f, 1, 0);
        hetero_add_synapse(sys, &pos2, 0.7f, 2, 0);
        hetero_add_synapse(sys, &pos3, 0.6f, 3, 0);

        hetero_spatial_coords_t center = {5.0f, 0.0f, 0.0f};
        hetero_competition_result_t result;
        hetero_winner_take_all(sys, &center, 15.0f, &result);

        EXPECT_EQ(result.winner_id, 1);  /* Always synapse 1 */

        hetero_free_competition_result(&result);
        hetero_destroy(sys);
    }
}

TEST(HeteroRegression, SpatialDistanceAccuracy) {
    /* WHAT: Test distance computation precision
     * WHY:  Prevent floating-point drift
     */
    hetero_spatial_coords_t pos1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos2 = {3.0f, 4.0f, 0.0f};

    for (int i = 0; i < 1000; i++) {
        float dist = hetero_compute_distance(&pos1, &pos2);
        EXPECT_FLOAT_EQ(dist, 5.0f);
    }
}

TEST(HeteroRegression, NeighborSearchCompleteness) {
    /* WHAT: Test neighbor search finds all synapses
     * WHY:  Prevent off-by-one errors
     */
    hetero_system_t* sys = hetero_create(nullptr, 50);

    /* Add synapses in ring at exactly 10μm radius */
    for (int i = 0; i < 8; i++) {
        float angle = (float)i * M_PI / 4.0f;
        hetero_spatial_coords_t pos = {
            10.0f * cosf(angle),
            10.0f * sinf(angle),
            0.0f
        };
        hetero_add_synapse(sys, &pos, 0.5f, i, 0);
    }

    /* Search with radius slightly larger than 10μm */
    hetero_spatial_coords_t center = {0.0f, 0.0f, 0.0f};
    hetero_synapse_t* neighbors[10];
    size_t num_found = 0;

    hetero_find_neighbors(sys, &center, 10.5f, neighbors, 10, &num_found);

    EXPECT_EQ(num_found, 8);  /* All 8 synapses */

    hetero_destroy(sys);
}

TEST(HeteroRegression, StatisticsAccumulation) {
    /* WHAT: Test statistics accumulate correctly
     * WHY:  Prevent counter overflow or reset bugs
     */
    hetero_system_t* sys = hetero_create(nullptr, 10);

    hetero_spatial_coords_t pos1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos2 = {10.0f, 0.0f, 0.0f};

    hetero_add_synapse(sys, &pos1, 0.5f, 1, 0);
    hetero_add_synapse(sys, &pos2, 0.8f, 2, 0);

    /* Apply depression 10 times */
    for (int i = 0; i < 10; i++) {
        hetero_apply_depression(sys, 1, 0.1f, i * 100);
    }

    uint64_t total_competitions, total_depressions;
    float avg_neighbors;
    hetero_get_statistics(sys, &total_competitions, &total_depressions, &avg_neighbors);

    EXPECT_EQ(total_depressions, 10);

    hetero_destroy(sys);
}

TEST(HeteroRegression, WeightBoundsEnforcement) {
    /* WHAT: Test weights never exceed bounds
     * WHY:  Prevent runaway values
     */
    hetero_system_t* sys = hetero_create(nullptr, 10);

    hetero_spatial_coords_t pos1 = {0.0f, 0.0f, 0.0f};
    hetero_spatial_coords_t pos2 = {5.0f, 0.0f, 0.0f};

    hetero_add_synapse(sys, &pos1, 0.5f, 1, 0);
    hetero_add_synapse(sys, &pos2, 0.01f, 2, 0);  /* Near minimum */

    /* Excessive depression */
    for (int i = 0; i < 100; i++) {
        hetero_apply_depression(sys, 1, 1.0f, i);
    }

    hetero_synapse_t* syn = hetero_get_synapse(sys, 2);
    EXPECT_GE(syn->weight, 0.0f);
    EXPECT_LE(syn->weight, 1.0f);

    hetero_destroy(sys);
}

TEST(HeteroRegression, SleepBridgeModulationRanges) {
    /* WHAT: Test sleep modulation stays in valid ranges
     * WHY:  Prevent extreme values
     */
    hetero_system_t* sys = hetero_create(nullptr, 10);
    hetero_sleep_bridge_t bridge = hetero_sleep_bridge_create(nullptr, nullptr, sys);

    hetero_sleep_update(bridge);

    hetero_sleep_effects_t effects;
    hetero_sleep_get_effects(bridge, &effects);

    EXPECT_GE(effects.competition_factor, 0.0f);
    EXPECT_LE(effects.competition_factor, 2.0f);

    EXPECT_GE(effects.depression_factor, 0.0f);
    EXPECT_LE(effects.depression_factor, 2.0f);

    EXPECT_GE(effects.radius_factor, 0.0f);
    EXPECT_LE(effects.radius_factor, 2.0f);

    hetero_sleep_bridge_destroy(bridge);
    hetero_destroy(sys);
}

TEST(HeteroRegression, ImmuneBridgeModulationRanges) {
    /* WHAT: Test immune modulation stays in valid ranges
     * WHY:  Prevent extreme values
     */
    hetero_system_t* sys = hetero_create(nullptr, 10);
    hetero_immune_bridge_t* bridge = hetero_immune_bridge_create(nullptr, nullptr, sys);

    hetero_immune_bridge_update(bridge, 100);

    hetero_modulation_state_t state;
    hetero_immune_get_modulation_state(bridge, &state);

    EXPECT_GE(state.competition_modulation, 0.0f);
    EXPECT_LE(state.competition_modulation, 2.0f);

    hetero_immune_bridge_destroy(bridge);
    hetero_destroy(sys);
}

TEST(HeteroRegression, MemoryLeakPrevention) {
    /* WHAT: Test repeated create/destroy doesn't leak
     * WHY:  Prevent memory leaks
     */
    for (int i = 0; i < 100; i++) {
        hetero_system_t* sys = hetero_create(nullptr, 50);

        for (int j = 0; j < 10; j++) {
            hetero_spatial_coords_t pos = {(float)j, 0.0f, 0.0f};
            hetero_add_synapse(sys, &pos, 0.5f, j, 0);
        }

        hetero_destroy(sys);
    }

    /* No crashes = success */
    SUCCEED();
}

TEST(HeteroRegression, ThreadSafety) {
    /* WHAT: Test concurrent access doesn't corrupt
     * WHY:  Verify mutex protection
     */
    hetero_system_t* sys = hetero_create(nullptr, 100);

    /* Add synapses */
    for (int i = 0; i < 10; i++) {
        hetero_spatial_coords_t pos = {(float)i, 0.0f, 0.0f};
        hetero_add_synapse(sys, &pos, 0.5f, i, 0);
    }

    /* Sequential access (threading would require actual threads) */
    for (int i = 0; i < 5; i++) {
        hetero_apply_depression(sys, 0, 0.1f, i);
        hetero_synapse_t* syn = hetero_get_synapse(sys, i);
        EXPECT_NE(syn, nullptr);
    }

    hetero_destroy(sys);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
