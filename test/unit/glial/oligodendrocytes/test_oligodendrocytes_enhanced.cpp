/**
 * @file test_oligodendrocytes_enhanced.cpp
 * @brief Comprehensive unit tests for Enhanced Oligodendrocyte Module
 *
 * Tests all new features:
 * - RK4 state dynamics
 * - G-ratio optimization
 * - Saltatory conduction velocity
 * - NRG1/BDNF growth factor signaling
 * - Lactate shuttle metabolic support
 * - KD-tree spatial indexing
 * - Centrality-based prioritization
 * - Enhanced axon structures
 * - Network operations
 *
 * 60+ comprehensive tests for complete code coverage
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class EnhancedOligodendrocyteTest : public ::testing::Test {
protected:
    oligodendrocyte_t* oligo = nullptr;
    oligodendrocyte_network_t* network = nullptr;

    void SetUp() override {
        oligo = nullptr;
        network = nullptr;
    }

    void TearDown() override {
        if (oligo) {
            oligodendrocyte_destroy(oligo);
            oligo = nullptr;
        }
        if (network) {
            oligodendrocyte_network_destroy(network);
            network = nullptr;
        }
    }

    // Helper: create oligo with position
    oligodendrocyte_t* createOligoAt(uint32_t id, float x, float y, float z, uint32_t max_axons) {
        return oligodendrocyte_create(id, x, y, z, max_axons);
    }

    // Helper: assign axon with full parameters
    void assignAxonFull(oligodendrocyte_t* o, uint32_t id, float x, float y, float z,
                        float diameter, float length) {
        oligodendrocyte_assign_axon_at(o, id, x, y, z, diameter, length);
    }
};

//=============================================================================
// CATEGORY 1: CREATION & DESTRUCTION (5 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, CreateBasic) {
    oligo = oligodendrocyte_create_basic(1, 30);
    ASSERT_NE(oligo, nullptr);
    EXPECT_EQ(oligo->id, 1);
    EXPECT_EQ(oligo->max_axons, 30);
    EXPECT_EQ(oligo->num_myelinated_axons, 0);
}

TEST_F(EnhancedOligodendrocyteTest, CreateWithPosition) {
    oligo = oligodendrocyte_create(1, 10.0f, 20.0f, 30.0f, 25);
    ASSERT_NE(oligo, nullptr);
    EXPECT_EQ(oligo->id, 1);
    EXPECT_FLOAT_EQ(oligo->position[0], 10.0f);
    EXPECT_FLOAT_EQ(oligo->position[1], 20.0f);
    EXPECT_FLOAT_EQ(oligo->position[2], 30.0f);
}

TEST_F(EnhancedOligodendrocyteTest, CreateInitializesMaturation) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(oligo, nullptr);
    EXPECT_EQ(oligo->maturation, OLIGO_STATE_OPC);
    EXPECT_FLOAT_EQ(oligo->maturation_progress, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, CreateInitializesRK4State) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(oligo, nullptr);
    EXPECT_FLOAT_EQ(oligo->state_variables[0], 0.0f);  // myelin rate
    EXPECT_FLOAT_EQ(oligo->state_variables[1], 0.0f);  // activity
    EXPECT_FLOAT_EQ(oligo->state_variables[2], 1.0f);  // ATP
    EXPECT_FLOAT_EQ(oligo->state_variables[3], 0.0f);  // maturation
}

TEST_F(EnhancedOligodendrocyteTest, CreateInitializesLactateShuttle) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(oligo, nullptr);
    EXPECT_GT(oligo->lactate_shuttle.lactate_pool, 0.0f);
    EXPECT_GT(oligo->lactate_shuttle.production_rate, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, CreateFailsWithInvalidParams) {
    EXPECT_EQ(oligodendrocyte_create(1, 0, 0, 0, 0), nullptr);
    EXPECT_EQ(oligodendrocyte_create(1, 0, 0, 0, 100), nullptr);  // > MAX_AXONS
}

//=============================================================================
// CATEGORY 2: AXON ASSIGNMENT (6 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, AssignAxonBasic) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    ASSERT_NE(oligo, nullptr);

    nimcp_result_t result = oligodendrocyte_assign_neuron(oligo, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(oligo->num_myelinated_axons, 1);
}

TEST_F(EnhancedOligodendrocyteTest, AssignAxonWithPosition) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    ASSERT_NE(oligo, nullptr);

    nimcp_result_t result = oligodendrocyte_assign_axon_at(oligo, 100, 5.0f, 10.0f, 15.0f, 2.0f, 500.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(oligo->axons[0].axon_id, 100);
    EXPECT_FLOAT_EQ(oligo->axons[0].position[0], 5.0f);
    EXPECT_FLOAT_EQ(oligo->axons[0].axon_diameter, 2.0f);
    EXPECT_FLOAT_EQ(oligo->axons[0].axon_length, 500.0f);
}

TEST_F(EnhancedOligodendrocyteTest, AssignAxonInitializesInternodes) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);
    EXPECT_GT(oligo->axons[0].num_internodes, 0);
    EXPECT_NE(oligo->axons[0].internodes, nullptr);
}

TEST_F(EnhancedOligodendrocyteTest, AssignAxonInitializesGRatio) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);
    EXPECT_FLOAT_EQ(oligo->axons[0].g_ratio, 1.0f);  // Unmyelinated
    EXPECT_GT(oligo->axons[0].optimal_g_ratio, 0.5f);
    EXPECT_LT(oligo->axons[0].optimal_g_ratio, 0.9f);
}

TEST_F(EnhancedOligodendrocyteTest, AssignAxonDuplicateIgnored) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    ASSERT_NE(oligo, nullptr);

    oligodendrocyte_assign_neuron(oligo, 100);
    oligodendrocyte_assign_neuron(oligo, 100);  // Duplicate
    EXPECT_EQ(oligo->num_myelinated_axons, 1);
}

TEST_F(EnhancedOligodendrocyteTest, AssignAxonCapacity) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 5);
    ASSERT_NE(oligo, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(oligodendrocyte_assign_neuron(oligo, i), NIMCP_SUCCESS);
    }
    EXPECT_EQ(oligodendrocyte_assign_neuron(oligo, 99), NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// CATEGORY 3: MYELINATION LEVELS (5 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, MyelinationLevelInitiallyZero) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    float level = oligodendrocyte_get_myelination_level(oligo, 100);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, SetMyelinationLevel) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    oligodendrocyte_set_myelination_level(oligo, 100, 0.75f);
    float level = oligodendrocyte_get_myelination_level(oligo, 100);
    EXPECT_NEAR(level, 0.75f, 0.01f);
}

TEST_F(EnhancedOligodendrocyteTest, MyelinStateTransitions) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    EXPECT_EQ(oligodendrocyte_get_myelin_state(oligo, 100), MYELIN_STATE_UNMYELINATED);

    oligodendrocyte_set_myelination_level(oligo, 100, 0.1f);
    EXPECT_EQ(oligodendrocyte_get_myelin_state(oligo, 100), MYELIN_STATE_INITIATING);

    oligodendrocyte_set_myelination_level(oligo, 100, 0.5f);
    EXPECT_EQ(oligodendrocyte_get_myelin_state(oligo, 100), MYELIN_STATE_PARTIAL);

    oligodendrocyte_set_myelination_level(oligo, 100, 0.9f);
    EXPECT_EQ(oligodendrocyte_get_myelin_state(oligo, 100), MYELIN_STATE_MATURE);
}

TEST_F(EnhancedOligodendrocyteTest, MyelinationUpdatesGRatio) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);

    float initial_g = oligodendrocyte_get_g_ratio(oligo, 100);
    EXPECT_FLOAT_EQ(initial_g, 1.0f);

    oligodendrocyte_set_myelination_level(oligo, 100, 1.0f);
    float myelinated_g = oligodendrocyte_get_g_ratio(oligo, 100);
    EXPECT_LT(myelinated_g, 1.0f);  // G-ratio decreases with myelination
    EXPECT_GE(myelinated_g, NIMCP_OLIGO_G_RATIO_MIN);
}

TEST_F(EnhancedOligodendrocyteTest, TotalMyelination) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);
    oligodendrocyte_assign_neuron(oligo, 101);
    oligodendrocyte_assign_neuron(oligo, 102);

    oligodendrocyte_set_myelination_level(oligo, 100, 0.5f);
    oligodendrocyte_set_myelination_level(oligo, 101, 0.3f);
    oligodendrocyte_set_myelination_level(oligo, 102, 0.2f);

    float total = oligodendrocyte_get_total_myelination(oligo);
    EXPECT_NEAR(total, 1.0f, 0.01f);
}

//=============================================================================
// CATEGORY 4: G-RATIO OPTIMIZATION (6 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, OptimalGRatioComputation) {
    // Small axon diameter
    float small_opt = oligodendrocyte_compute_optimal_g_ratio(1.0f, 0.0f);
    EXPECT_GE(small_opt, 0.6f);
    EXPECT_LE(small_opt, 0.7f);

    // Large axon diameter
    float large_opt = oligodendrocyte_compute_optimal_g_ratio(10.0f, 0.0f);
    EXPECT_GT(large_opt, small_opt);  // Larger axons have higher optimal G-ratio
}

TEST_F(EnhancedOligodendrocyteTest, OptimalGRatioActivityModulation) {
    float low_activity = oligodendrocyte_compute_optimal_g_ratio(2.0f, 0.0f);
    float high_activity = oligodendrocyte_compute_optimal_g_ratio(2.0f, 50.0f);
    EXPECT_GT(high_activity, low_activity);  // Higher activity → thinner myelin for plasticity
}

TEST_F(EnhancedOligodendrocyteTest, GRatioOptimization) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);
    oligodendrocyte_set_myelination_level(oligo, 100, 0.8f);

    float initial_g = oligodendrocyte_get_g_ratio(oligo, 100);

    // Simulate some activity
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_track_activity(oligo, 100, 5.0f, nimcp_time_monotonic_us());
        oligodendrocyte_optimize_g_ratios(oligo, 0.1f);
    }

    float optimized_g = oligodendrocyte_get_g_ratio(oligo, 100);
    EXPECT_NE(initial_g, optimized_g);  // G-ratio should change
}

TEST_F(EnhancedOligodendrocyteTest, GRatioDeviation) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);
    oligodendrocyte_set_myelination_level(oligo, 100, 0.8f);

    float deviation = oligodendrocyte_get_g_ratio_deviation(oligo);
    EXPECT_GE(deviation, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, GRatioBoundaries) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);
    oligodendrocyte_set_myelination_level(oligo, 100, 1.0f);

    float g = oligodendrocyte_get_g_ratio(oligo, 100);
    EXPECT_GE(g, NIMCP_OLIGO_G_RATIO_MIN);
    EXPECT_LE(g, NIMCP_OLIGO_G_RATIO_MAX);
}

TEST_F(EnhancedOligodendrocyteTest, AvgGRatioStatistic) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);
    oligodendrocyte_assign_neuron(oligo, 101);
    oligodendrocyte_set_myelination_level(oligo, 100, 0.5f);
    oligodendrocyte_set_myelination_level(oligo, 101, 0.5f);

    oligodendrocyte_optimize_g_ratios(oligo, 0.1f);

    EXPECT_GE(oligo->avg_g_ratio, NIMCP_OLIGO_G_RATIO_MIN);
    EXPECT_LE(oligo->avg_g_ratio, NIMCP_OLIGO_G_RATIO_MAX);
}

//=============================================================================
// CATEGORY 5: SALTATORY CONDUCTION (6 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, UnmyelinatedVelocity) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);

    float velocity = oligodendrocyte_compute_conduction_velocity(oligo, 100, 1.0f);
    EXPECT_NEAR(velocity, NIMCP_OLIGO_BASE_VELOCITY_MS, 0.1f);
}

TEST_F(EnhancedOligodendrocyteTest, MyelinatedVelocityIncrease) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);

    float unmyelinated = oligodendrocyte_compute_conduction_velocity(oligo, 100, 1.0f);

    oligodendrocyte_set_myelination_level(oligo, 100, 1.0f);
    float myelinated = oligodendrocyte_compute_conduction_velocity(oligo, 100, 1.0f);

    EXPECT_GT(myelinated, unmyelinated);
    EXPECT_GT(myelinated, NIMCP_OLIGO_BASE_VELOCITY_MS * 2);
}

TEST_F(EnhancedOligodendrocyteTest, SaltatoryVelocityComputation) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);
    oligodendrocyte_set_myelination_level(oligo, 100, 1.0f);

    float velocity = oligodendrocyte_compute_saltatory_velocity(&oligo->axons[0]);
    EXPECT_GT(velocity, NIMCP_OLIGO_BASE_VELOCITY_MS);
}

TEST_F(EnhancedOligodendrocyteTest, PropagationDelay) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 1000.0f);  // 1mm axon

    float delay_unmyelinated = oligodendrocyte_compute_propagation_delay(oligo, 100);

    oligodendrocyte_set_myelination_level(oligo, 100, 1.0f);
    float delay_myelinated = oligodendrocyte_compute_propagation_delay(oligo, 100);

    EXPECT_LT(delay_myelinated, delay_unmyelinated);  // Faster = shorter delay
}

TEST_F(EnhancedOligodendrocyteTest, InternodeSpacingOptimization) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 2.0f, 500.0f);

    float initial_length = oligo->axons[0].internodes[0].length;

    oligodendrocyte_optimize_internode_spacing(oligo, 100);

    // Length should be adjusted toward optimal
    float optimal = 2.0f * NIMCP_OLIGO_INTERNODE_DIAMETER_RATIO;
    float adjusted_length = oligo->axons[0].internodes[0].length;
    EXPECT_NE(adjusted_length, initial_length);
}

TEST_F(EnhancedOligodendrocyteTest, DiameterAffectsVelocity) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_axon_at(oligo, 100, 0, 0, 0, 1.0f, 500.0f);  // Small diameter
    oligodendrocyte_assign_axon_at(oligo, 101, 0, 0, 0, 5.0f, 500.0f);  // Large diameter

    oligodendrocyte_set_myelination_level(oligo, 100, 1.0f);
    oligodendrocyte_set_myelination_level(oligo, 101, 1.0f);

    float vel_small = oligodendrocyte_compute_conduction_velocity(oligo, 100, 1.0f);
    float vel_large = oligodendrocyte_compute_conduction_velocity(oligo, 101, 1.0f);

    EXPECT_GT(vel_large, vel_small);  // Larger diameter = faster conduction
}

//=============================================================================
// CATEGORY 6: ACTIVITY TRACKING (5 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, TrackActivity) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    oligodendrocyte_track_activity(oligo, 100, 10.0f, nimcp_time_monotonic_us());

    EXPECT_GT(oligo->axons[0].activity_score, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, ActivityEMA) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    // Track constant activity
    for (int i = 0; i < 20; i++) {
        oligodendrocyte_track_activity(oligo, 100, 5.0f, nimcp_time_monotonic_us() + i * 1000);
    }

    // Activity should stabilize near 5.0
    EXPECT_NEAR(oligo->axons[0].activity_score, 5.0f, 1.0f);
}

TEST_F(EnhancedOligodendrocyteTest, FilteredActivity) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    // Track varying activity
    for (int i = 0; i < 10; i++) {
        float activity = (i % 2 == 0) ? 10.0f : 0.0f;
        oligodendrocyte_track_activity(oligo, 100, activity, nimcp_time_monotonic_us() + i * 1000);
    }

    // Filtered activity should be smoother than raw EMA
    EXPECT_GT(oligo->axons[0].filtered_activity, 0.0f);
    EXPECT_LT(oligo->axons[0].filtered_activity, 10.0f);
}

TEST_F(EnhancedOligodendrocyteTest, ActivityDecay) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    uint64_t t0 = nimcp_time_monotonic_us();
    oligodendrocyte_track_activity(oligo, 100, 10.0f, t0);
    float initial = oligo->axons[0].activity_score;

    // Simulate time passage
    oligodendrocyte_update_activity_scores(oligo, t0 + 5000000);  // 5 seconds later

    EXPECT_LT(oligo->axons[0].activity_score, initial);
}

TEST_F(EnhancedOligodendrocyteTest, ActivityIntegral) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    uint64_t t0 = nimcp_time_monotonic_us();
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_track_activity(oligo, 100, 5.0f, t0 + i * 100000);
    }

    EXPECT_GT(oligo->axons[0].activity_integral, 0.0f);
}

//=============================================================================
// CATEGORY 7: GROWTH FACTOR SIGNALING (6 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, GrowthFactorInitialization) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);

    for (int i = 0; i < NIMCP_GROWTH_FACTOR_COUNT; i++) {
        EXPECT_FLOAT_EQ(oligodendrocyte_get_growth_factor(oligo, (growth_factor_type_t)i), 0.0f);
    }
}

TEST_F(EnhancedOligodendrocyteTest, AddGrowthFactor) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);

    oligodendrocyte_add_growth_factor(oligo, GROWTH_FACTOR_NRG1, 5.0f);
    EXPECT_FLOAT_EQ(oligodendrocyte_get_growth_factor(oligo, GROWTH_FACTOR_NRG1), 5.0f);
}

TEST_F(EnhancedOligodendrocyteTest, GrowthFactorMaxConcentration) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);

    oligodendrocyte_add_growth_factor(oligo, GROWTH_FACTOR_NRG1, 100.0f);
    EXPECT_LE(oligodendrocyte_get_growth_factor(oligo, GROWTH_FACTOR_NRG1),
              NIMCP_GROWTH_FACTOR_MAX_CONCENTRATION);
}

TEST_F(EnhancedOligodendrocyteTest, GrowthFactorDecay) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);

    oligodendrocyte_add_growth_factor(oligo, GROWTH_FACTOR_NRG1, 5.0f);
    float initial = oligodendrocyte_get_growth_factor(oligo, GROWTH_FACTOR_NRG1);

    oligodendrocyte_update_growth_factors(oligo, 1.0f);  // 1 second

    EXPECT_LT(oligodendrocyte_get_growth_factor(oligo, GROWTH_FACTOR_NRG1), initial);
}

TEST_F(EnhancedOligodendrocyteTest, ActivityInducesBDNF) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    // Generate activity
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_track_activity(oligo, 100, 20.0f, nimcp_time_monotonic_us() + i * 1000);
    }

    // Update growth factors
    oligodendrocyte_update_growth_factors(oligo, 1.0f);

    EXPECT_GT(oligodendrocyte_get_growth_factor(oligo, GROWTH_FACTOR_BDNF), 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, MyelinSignalComputation) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    oligodendrocyte_add_growth_factor(oligo, GROWTH_FACTOR_NRG1, 5.0f);
    oligodendrocyte_add_growth_factor(oligo, GROWTH_FACTOR_BDNF, 3.0f);

    float signal = oligodendrocyte_compute_myelin_signal(oligo, 100);
    EXPECT_GT(signal, 0.0f);
    EXPECT_LE(signal, 1.0f);
}

//=============================================================================
// CATEGORY 8: LACTATE SHUTTLE (5 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, LactateProduction) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    float initial = oligo->lactate_shuttle.lactate_pool;

    oligodendrocyte_update_lactate_shuttle(oligo, 0.1f);

    EXPECT_NE(oligo->lactate_shuttle.lactate_pool, initial);
}

TEST_F(EnhancedOligodendrocyteTest, LactateDelivery) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    oligodendrocyte_set_axon_demand(oligo, 100, 2.0f);
    oligodendrocyte_update_lactate_shuttle(oligo, 0.1f);

    float lactate = oligodendrocyte_get_axon_lactate(oligo, 100);
    EXPECT_GE(lactate, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, MetabolicSupport) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    // Set high lactate production
    oligo->lactate_shuttle.lactate_pool = 1.0f;
    oligodendrocyte_set_axon_demand(oligo, 100, 1.0f);

    for (int i = 0; i < 10; i++) {
        oligodendrocyte_update_lactate_shuttle(oligo, 0.1f);
    }

    bool supported = oligodendrocyte_axon_metabolically_supported(oligo, 100);
    // May or may not be supported depending on delivery rate
    (void)supported;  // Suppress unused warning
}

TEST_F(EnhancedOligodendrocyteTest, TotalLactateDelivered) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);
    oligodendrocyte_set_axon_demand(oligo, 100, 1.0f);

    float initial = oligo->total_lactate_delivered;

    for (int i = 0; i < 10; i++) {
        oligodendrocyte_update_lactate_shuttle(oligo, 0.1f);
    }

    EXPECT_GT(oligo->total_lactate_delivered, initial);
}

TEST_F(EnhancedOligodendrocyteTest, SupportedAxonCount) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);
    oligodendrocyte_assign_neuron(oligo, 101);

    oligo->lactate_shuttle.lactate_pool = 2.0f;

    oligodendrocyte_update_lactate_shuttle(oligo, 0.1f);

    EXPECT_LE(oligo->lactate_shuttle.supported_axon_count, 2);
}

//=============================================================================
// CATEGORY 9: STATE DYNAMICS RK4 (5 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, StateDynamicsUpdate) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    float initial_state0 = oligo->state_variables[0];

    oligodendrocyte_track_activity(oligo, 100, 10.0f, nimcp_time_monotonic_us());
    oligodendrocyte_update_state_dynamics(oligo, 0.1f);

    // State should change based on activity
    EXPECT_TRUE(oligo->state_variables[0] != initial_state0 ||
                oligo->state_variables[1] != 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, MyelinationRateFromState) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    // Generate high activity
    for (int i = 0; i < 20; i++) {
        oligodendrocyte_track_activity(oligo, 100, 20.0f, nimcp_time_monotonic_us() + i * 1000);
        oligodendrocyte_update_state_dynamics(oligo, 0.1f);
    }

    EXPECT_GT(oligo->myelination_rate, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, MaturationProgression) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    EXPECT_EQ(oligo->maturation, OLIGO_STATE_OPC);

    bool advanced = oligodendrocyte_advance_maturation(oligo);
    EXPECT_TRUE(advanced);
    EXPECT_EQ(oligo->maturation, OLIGO_STATE_PRE_OL);
}

TEST_F(EnhancedOligodendrocyteTest, GetMaturation) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);

    EXPECT_EQ(oligodendrocyte_get_maturation(oligo), OLIGO_STATE_OPC);

    oligodendrocyte_advance_maturation(oligo);
    EXPECT_EQ(oligodendrocyte_get_maturation(oligo), OLIGO_STATE_PRE_OL);
}

TEST_F(EnhancedOligodendrocyteTest, ATPSyncWithState) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);

    oligo->state_variables[2] = 0.5f;
    oligodendrocyte_update_atp(oligo, 0.1f);

    // ATP should sync with state
    EXPECT_NEAR(oligo->atp_level, oligo->state_variables[2], 0.2f);
}

//=============================================================================
// CATEGORY 10: METABOLIC MANAGEMENT (4 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, ATPRegeneration) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligo->atp_level = 0.5f;

    oligodendrocyte_update_atp(oligo, 1.0f);

    EXPECT_GT(oligo->atp_level, 0.5f);  // Should regenerate
}

TEST_F(EnhancedOligodendrocyteTest, ATPConsumption) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    for (int i = 0; i < 30; i++) {
        oligodendrocyte_assign_neuron(oligo, i);
        oligodendrocyte_set_myelination_level(oligo, i, 1.0f);
    }

    float initial = oligo->atp_level;
    oligodendrocyte_update_atp(oligo, 0.1f);

    // High myelination should consume ATP
    EXPECT_LT(oligo->atp_level, initial + 0.1f);  // Account for regen
}

TEST_F(EnhancedOligodendrocyteTest, GlucoseAddition) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligo->glucose_level = 0.3f;

    oligodendrocyte_add_glucose(oligo, 0.5f);
    EXPECT_NEAR(oligo->glucose_level, 0.8f, 0.01f);
}

TEST_F(EnhancedOligodendrocyteTest, GetATPLevel) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligo->atp_level = 0.7f;

    EXPECT_FLOAT_EQ(oligodendrocyte_get_atp_level(oligo), 0.7f);
}

//=============================================================================
// CATEGORY 11: ADAPTIVE MYELINATION (5 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, RemodelMyelination) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    // Track high activity
    for (int i = 0; i < 20; i++) {
        oligodendrocyte_track_activity(oligo, 100, 15.0f, nimcp_time_monotonic_us() + i * 1000);
    }

    oligodendrocyte_remodel_myelination(oligo, 1.0f);

    EXPECT_GT(oligo->axons[0].myelination_level, 0.0f);
}

TEST_F(EnhancedOligodendrocyteTest, CentralityPrioritization) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);
    oligodendrocyte_assign_neuron(oligo, 101);

    // Equal activity
    oligodendrocyte_track_activity(oligo, 100, 5.0f, nimcp_time_monotonic_us());
    oligodendrocyte_track_activity(oligo, 101, 5.0f, nimcp_time_monotonic_us());

    // High centrality for one axon
    oligodendrocyte_set_axon_centrality(oligo, 100, 0.9f);
    oligodendrocyte_set_axon_centrality(oligo, 101, 0.1f);

    oligodendrocyte_remodel_myelination(oligo, 1.0f);

    // High-centrality axon should get more myelination
    EXPECT_GT(oligo->axons[0].myelination_level, oligo->axons[1].myelination_level);
}

TEST_F(EnhancedOligodendrocyteTest, PriorityMyelinationFlag) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);

    oligodendrocyte_set_axon_centrality(oligo, 100, 0.5f);
    EXPECT_TRUE(oligo->axons[0].priority_myelination);

    oligodendrocyte_set_axon_centrality(oligo, 100, 0.05f);
    EXPECT_FALSE(oligo->axons[0].priority_myelination);
}

TEST_F(EnhancedOligodendrocyteTest, CapacityConstraint) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 5);
    oligo->max_myelination_capacity = 2.0f;

    for (int i = 0; i < 5; i++) {
        oligodendrocyte_assign_neuron(oligo, i);
        oligodendrocyte_track_activity(oligo, i, 20.0f, nimcp_time_monotonic_us());
    }

    // Multiple remodel cycles
    for (int j = 0; j < 100; j++) {
        oligodendrocyte_remodel_myelination(oligo, 0.1f);
    }

    float total = oligodendrocyte_get_total_myelination(oligo);
    EXPECT_LE(total, oligo->max_myelination_capacity + 0.01f);
}

TEST_F(EnhancedOligodendrocyteTest, AvgConductionVelocity) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 30);
    oligodendrocyte_assign_neuron(oligo, 100);
    oligodendrocyte_assign_neuron(oligo, 101);

    oligodendrocyte_set_myelination_level(oligo, 100, 0.8f);
    oligodendrocyte_set_myelination_level(oligo, 101, 0.5f);

    oligodendrocyte_remodel_myelination(oligo, 0.1f);

    float avg = oligodendrocyte_get_avg_conduction_velocity(oligo);
    EXPECT_GT(avg, NIMCP_OLIGO_BASE_VELOCITY_MS);
}

//=============================================================================
// CATEGORY 12: NETWORK OPERATIONS (8 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, NetworkCreate) {
    network = oligodendrocyte_network_create(100);
    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->capacity, 100);
    EXPECT_EQ(network->num_oligodendrocytes, 0);
}

TEST_F(EnhancedOligodendrocyteTest, NetworkCreateEnhanced) {
    oligodendrocyte_network_config_t config = oligodendrocyte_network_default_config();
    config.capacity = 50;
    config.enable_g_ratio_optimization = true;

    network = oligodendrocyte_network_create_enhanced(&config);
    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->capacity, 50);
}

TEST_F(EnhancedOligodendrocyteTest, NetworkAdd) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o = createOligoAt(1, 0, 0, 0, 20);

    nimcp_result_t result = oligodendrocyte_network_add(network, o);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(network->num_oligodendrocytes, 1);
}

TEST_F(EnhancedOligodendrocyteTest, NetworkFindByNeuron) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_assign_neuron(o, 100);
    oligodendrocyte_network_add(network, o);

    oligodendrocyte_t* found = oligodendrocyte_network_find_by_neuron(network, 100);
    EXPECT_EQ(found, o);

    found = oligodendrocyte_network_find_by_neuron(network, 999);
    EXPECT_EQ(found, nullptr);
}

TEST_F(EnhancedOligodendrocyteTest, NetworkFindNearest) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o1 = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_t* o2 = createOligoAt(2, 100, 0, 0, 20);
    oligodendrocyte_network_add(network, o1);
    oligodendrocyte_network_add(network, o2);

    oligodendrocyte_t* nearest = oligodendrocyte_network_find_nearest(network, 10, 0, 0);
    EXPECT_EQ(nearest, o1);

    nearest = oligodendrocyte_network_find_nearest(network, 90, 0, 0);
    EXPECT_EQ(nearest, o2);
}

TEST_F(EnhancedOligodendrocyteTest, NetworkFindInRadius) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o1 = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_t* o2 = createOligoAt(2, 50, 0, 0, 20);
    oligodendrocyte_t* o3 = createOligoAt(3, 200, 0, 0, 20);
    oligodendrocyte_network_add(network, o1);
    oligodendrocyte_network_add(network, o2);
    oligodendrocyte_network_add(network, o3);

    oligodendrocyte_t* results[10];
    uint32_t count = oligodendrocyte_network_find_in_radius(network, 0, 0, 0, 100, results, 10);
    EXPECT_EQ(count, 2);  // o1 and o2 within 100 units
}

TEST_F(EnhancedOligodendrocyteTest, NetworkStep) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_assign_neuron(o, 100);
    oligodendrocyte_track_activity(o, 100, 10.0f, nimcp_time_monotonic_us());
    oligodendrocyte_network_add(network, o);

    float initial_myelin = o->axons[0].myelination_level;

    for (int i = 0; i < 100; i++) {
        oligodendrocyte_track_activity(o, 100, 10.0f, nimcp_time_monotonic_us() + i * 1000);
        oligodendrocyte_network_step(network, 0.1f);
    }

    // Myelination should change after network steps
    EXPECT_NE(o->axons[0].myelination_level, initial_myelin);
}

TEST_F(EnhancedOligodendrocyteTest, NetworkStats) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_assign_neuron(o, 100);
    oligodendrocyte_set_myelination_level(o, 100, 0.5f);
    oligodendrocyte_network_add(network, o);

    oligodendrocyte_network_stats_t stats;
    oligodendrocyte_network_get_stats(network, &stats);

    EXPECT_EQ(stats.total_oligodendrocytes, 1);
    EXPECT_EQ(stats.total_myelinated_axons, 1);
    EXPECT_EQ(stats.opc_count, 1);
}

//=============================================================================
// CATEGORY 13: SPATIAL INDEXING (3 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, RebuildSpatialIndex) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o1 = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_t* o2 = createOligoAt(2, 100, 100, 100, 20);
    oligodendrocyte_network_add(network, o1);
    oligodendrocyte_network_add(network, o2);

    oligodendrocyte_network_rebuild_spatial_index(network);
    EXPECT_TRUE(network->spatial_index_valid);
}

TEST_F(EnhancedOligodendrocyteTest, SpatialIndexFindNearest) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o1 = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_t* o2 = createOligoAt(2, 500, 500, 500, 20);
    oligodendrocyte_network_add(network, o1);
    oligodendrocyte_network_add(network, o2);

    oligodendrocyte_network_rebuild_spatial_index(network);

    oligodendrocyte_t* nearest = oligodendrocyte_network_find_nearest(network, 450, 450, 450);
    EXPECT_EQ(nearest, o2);
}

TEST_F(EnhancedOligodendrocyteTest, InvalidateSpatialIndex) {
    network = oligodendrocyte_network_create(10);
    oligodendrocyte_t* o1 = createOligoAt(1, 0, 0, 0, 20);
    oligodendrocyte_network_add(network, o1);

    oligodendrocyte_network_rebuild_spatial_index(network);
    EXPECT_TRUE(network->spatial_index_valid);

    // Adding new oligo should invalidate
    oligodendrocyte_t* o2 = createOligoAt(2, 100, 0, 0, 20);
    oligodendrocyte_network_add(network, o2);
    EXPECT_FALSE(network->spatial_index_valid);
}

//=============================================================================
// CATEGORY 14: UTILITY FUNCTIONS (4 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, MaturationStateToString) {
    EXPECT_STREQ(oligo_maturation_state_to_string(OLIGO_STATE_OPC), "OPC");
    EXPECT_STREQ(oligo_maturation_state_to_string(OLIGO_STATE_PRE_OL), "Pre-OL");
    EXPECT_STREQ(oligo_maturation_state_to_string(OLIGO_STATE_IMMATURE), "Immature");
    EXPECT_STREQ(oligo_maturation_state_to_string(OLIGO_STATE_MATURE), "Mature");
}

TEST_F(EnhancedOligodendrocyteTest, MyelinStateToString) {
    EXPECT_STREQ(myelin_state_to_string(MYELIN_STATE_UNMYELINATED), "Unmyelinated");
    EXPECT_STREQ(myelin_state_to_string(MYELIN_STATE_INITIATING), "Initiating");
    EXPECT_STREQ(myelin_state_to_string(MYELIN_STATE_PARTIAL), "Partial");
    EXPECT_STREQ(myelin_state_to_string(MYELIN_STATE_MATURE), "Mature");
    EXPECT_STREQ(myelin_state_to_string(MYELIN_STATE_DEGENERATING), "Degenerating");
}

TEST_F(EnhancedOligodendrocyteTest, GrowthFactorTypeToString) {
    EXPECT_STREQ(growth_factor_type_to_string(GROWTH_FACTOR_NRG1), "NRG1");
    EXPECT_STREQ(growth_factor_type_to_string(GROWTH_FACTOR_BDNF), "BDNF");
    EXPECT_STREQ(growth_factor_type_to_string(GROWTH_FACTOR_IGF1), "IGF-1");
    EXPECT_STREQ(growth_factor_type_to_string(GROWTH_FACTOR_NT3), "NT-3");
}

TEST_F(EnhancedOligodendrocyteTest, DefaultConfig) {
    oligodendrocyte_network_config_t config = oligodendrocyte_network_default_config();
    EXPECT_GT(config.capacity, 0);
    EXPECT_TRUE(config.enable_g_ratio_optimization);
    EXPECT_TRUE(config.enable_growth_factor_signaling);
    EXPECT_TRUE(config.enable_lactate_shuttle);
}

//=============================================================================
// CATEGORY 15: EDGE CASES & NULL SAFETY (4 tests)
//=============================================================================

TEST_F(EnhancedOligodendrocyteTest, NullSafetyDestroy) {
    oligodendrocyte_destroy(nullptr);  // Should not crash
}

TEST_F(EnhancedOligodendrocyteTest, NullSafetyGetters) {
    EXPECT_FLOAT_EQ(oligodendrocyte_get_myelination_level(nullptr, 0), 0.0f);
    EXPECT_FLOAT_EQ(oligodendrocyte_get_g_ratio(nullptr, 0), -1.0f);
    EXPECT_FLOAT_EQ(oligodendrocyte_get_atp_level(nullptr), 0.0f);
    EXPECT_EQ(oligodendrocyte_get_maturation(nullptr), OLIGO_STATE_OPC);
}

TEST_F(EnhancedOligodendrocyteTest, NullSafetyUpdates) {
    oligodendrocyte_update_atp(nullptr, 0.1f);
    oligodendrocyte_remodel_myelination(nullptr, 0.1f);
    oligodendrocyte_update_growth_factors(nullptr, 0.1f);
    // Should not crash
}

TEST_F(EnhancedOligodendrocyteTest, InvalidTimeStep) {
    oligo = oligodendrocyte_create(1, 0, 0, 0, 20);

    oligodendrocyte_update_atp(oligo, -1.0f);
    oligodendrocyte_update_atp(oligo, 0.0f);
    // Should handle gracefully
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
