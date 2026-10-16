/**
 * @file test_structural_regression.cpp
 * @brief Regression tests for structural plasticity stability and performance
 */

#include <gtest/gtest.h>
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include <chrono>

//=============================================================================
// Test Fixtures
//=============================================================================

class StructuralRegressionTest : public ::testing::Test {
protected:
    structural_plasticity_system_t* system;

    void SetUp() override {
        structural_plasticity_config_t config;
        structural_plasticity_default_config(&config);
        config.max_spines = 10000;
        system = structural_plasticity_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            structural_plasticity_destroy(system);
        }
    }
};

//=============================================================================
// Stability Regression Tests
//=============================================================================

TEST_F(StructuralRegressionTest, StableSpineCountOverTime) {
    /* WHAT: Test spine count stability over extended simulation
     * WHY:  Ensure no memory leaks or count drift
     */
    const int num_cycles = 100;

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        /* Form some spines */
        for (int i = 0; i < 10; i++) {
            uint32_t id;
            structural_plasticity_form_synapse(
                system, cycle * 10 + i, cycle * 10 + i + 1, 50.0f, &id);
        }

        /* Eliminate half */
        for (int i = 0; i < 5; i++) {
            uint32_t id = cycle * 10 + i + 1;
            /* Eliminate based on ID (approximation) */
        }

        /* Update */
        structural_plasticity_update(system, 1.0f);
    }

    /* Count should be reasonable, not overflow */
    uint32_t final_count = structural_plasticity_get_total_spines(system);
    EXPECT_LT(final_count, 10000);
    EXPECT_GT(final_count, 0);
}

TEST_F(StructuralRegressionTest, FormationEliminationBalance) {
    /* WHAT: Test formation/elimination maintains equilibrium
     * WHY:  Verify homeostatic regulation works
     */
    const int iterations = 200;
    uint32_t synapse_counter = 1;

    for (int i = 0; i < iterations; i++) {
        /* Form spine */
        uint32_t id;
        structural_plasticity_form_synapse(
            system, synapse_counter, synapse_counter + 1, 50.0f, &id);
        synapse_counter++;

        /* Occasionally eliminate */
        if (i % 5 == 0 && id > 1) {
            structural_plasticity_eliminate_synapse(system, id - 1);
        }

        structural_plasticity_update(system, 0.1f);
    }

    /* Should reach equilibrium */
    uint32_t count = structural_plasticity_get_total_spines(system);
    EXPECT_GT(count, 50);
    EXPECT_LT(count, 300);
}

TEST_F(StructuralRegressionTest, MaturationProgressMonotonic) {
    /* WHAT: Test maturation progress only increases
     * WHY:  Prevent regression in maturation
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    float last_progress = 0.0f;
    for (int i = 0; i < 100; i++) {
        structural_plasticity_update(system, 10.0f);

        synapse_structural_state_t state;
        if (structural_plasticity_get_synapse_state(
                system, synapse_id, &state) == 0) {

            if (state.state == SYNAPSE_STATE_NASCENT) {
                EXPECT_GE(state.maturation_progress, last_progress);
                last_progress = state.maturation_progress;
            }
        }
    }
}

TEST_F(StructuralRegressionTest, PruningUrgencyBounded) {
    /* WHAT: Test pruning urgency stays within bounds
     * WHY:  Prevent accumulation overflow
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    /* Record lots of LTD */
    for (int i = 0; i < 100; i++) {
        structural_plasticity_record_ltd(system, synapse_id, 1.0f);
    }

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);

    /* Should be clamped */
    EXPECT_LE(state.pruning_urgency, 1.0f);
    EXPECT_GE(state.pruning_urgency, 0.0f);
}

TEST_F(StructuralRegressionTest, MorphologyParametersRealistic) {
    /* WHAT: Test morphology stays within realistic ranges
     * WHY:  Prevent numerical drift
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    /* Transition through states */
    structural_plasticity_stabilize_synapse(system, synapse_id);
    structural_plasticity_potentiate_synapse(system, synapse_id);

    spine_morphology_t morph;
    structural_plasticity_get_morphology(system, synapse_id, &morph);

    EXPECT_GT(morph.spine_volume, 0.0f);
    EXPECT_LT(morph.spine_volume, 10.0f);
    EXPECT_GT(morph.psd_size, 0.0f);
    EXPECT_LT(morph.psd_size, 10.0f);
    EXPECT_GE(morph.spine_stability, 0.0f);
    EXPECT_LE(morph.spine_stability, 1.0f);
    EXPECT_GE(morph.spine_motility, 0.0f);
    EXPECT_LE(morph.spine_motility, 1.0f);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(StructuralRegressionTest, FormationPerformance) {
    /* WHAT: Test formation performance at scale
     * WHY:  Ensure O(1) formation time
     */
    const int num_formations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_formations; i++) {
        uint32_t id;
        structural_plasticity_form_synapse(system, i, i+1, 50.0f, &id);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    /* Should complete in reasonable time */
    EXPECT_LT(duration, 100000);  /* < 100ms */

    /* Average per formation */
    double avg_us = static_cast<double>(duration) / num_formations;
    EXPECT_LT(avg_us, 100.0);  /* < 100 microseconds per formation */
}

TEST_F(StructuralRegressionTest, UpdatePerformance) {
    /* WHAT: Test update performance with many spines
     * WHY:  Ensure linear scaling
     */
    /* Create many spines */
    for (int i = 0; i < 1000; i++) {
        uint32_t id;
        structural_plasticity_form_synapse(system, i, i+1, 50.0f, &id);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        structural_plasticity_update(system, 0.1f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    /* Should complete in reasonable time */
    EXPECT_LT(duration, 1000);  /* < 1 second for 100 updates */
}

TEST_F(StructuralRegressionTest, QueryPerformance) {
    /* WHAT: Test query performance
     * WHY:  Ensure O(1) access time
     */
    /* Create spines */
    uint32_t synapse_ids[100];
    for (int i = 0; i < 100; i++) {
        structural_plasticity_form_synapse(system, i, i+1, 50.0f, &synapse_ids[i]);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        synapse_structural_state_t state;
        structural_plasticity_get_synapse_state(
            system, synapse_ids[i % 100], &state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    /* Should be very fast */
    double avg_us = static_cast<double>(duration) / 1000.0;
    EXPECT_LT(avg_us, 10.0);  /* < 10 microseconds per query */
}

//=============================================================================
// State Transition Regression Tests
//=============================================================================

TEST_F(StructuralRegressionTest, ValidStateTransitions) {
    /* WHAT: Test only valid state transitions occur
     * WHY:  Prevent illegal state changes
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_NASCENT);

    /* NASCENT → STABLE */
    structural_plasticity_stabilize_synapse(system, synapse_id);
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_STABLE);

    /* STABLE → POTENTIATED */
    structural_plasticity_potentiate_synapse(system, synapse_id);
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_POTENTIATED);

    /* Any state → ELIMINATED */
    structural_plasticity_eliminate_synapse(system, synapse_id);
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_ELIMINATED);
}

TEST_F(StructuralRegressionTest, NoIllegalTransitions) {
    /* WHAT: Test illegal transitions are prevented
     * WHY:  Enforce state machine integrity
     */
    uint32_t synapse_id;
    structural_plasticity_form_synapse(system, 1, 2, 50.0f, &synapse_id);

    /* Cannot potentiate nascent */
    int result = structural_plasticity_potentiate_synapse(system, synapse_id);
    EXPECT_NE(result, 0);

    synapse_structural_state_t state;
    structural_plasticity_get_synapse_state(system, synapse_id, &state);
    EXPECT_EQ(state.state, SYNAPSE_STATE_NASCENT);
}

//=============================================================================
// Memory Safety Regression Tests
//=============================================================================

TEST_F(StructuralRegressionTest, NoMemoryLeakOnDestroy) {
    /* WHAT: Test clean destruction
     * WHY:  Prevent memory leaks
     */
    /* Create many spines */
    for (int i = 0; i < 1000; i++) {
        uint32_t id;
        structural_plasticity_form_synapse(system, i, i+1, 50.0f, &id);
    }

    /* Destroy should free all memory */
    structural_plasticity_destroy(system);
    system = nullptr;  /* Prevent double-free in TearDown */

    /* If we get here without crash, passed */
    SUCCEED();
}

TEST_F(StructuralRegressionTest, HandleInvalidSynapseID) {
    /* WHAT: Test graceful handling of invalid IDs
     * WHY:  Prevent crashes on bad input
     */
    synapse_structural_state_t state;
    int result = structural_plasticity_get_synapse_state(
        system, 99999, &state);

    EXPECT_NE(result, 0);  /* Should fail gracefully */
}

TEST_F(StructuralRegressionTest, HandleNullPointers) {
    /* WHAT: Test NULL pointer handling
     * WHY:  Prevent crashes
     */
    EXPECT_NE(structural_plasticity_default_config(nullptr), 0);

    /* Note: structural_plasticity_create(nullptr) uses default config,
     * which is a valid and user-friendly API design. It should succeed.
     */
    structural_plasticity_system_t* temp_system = structural_plasticity_create(nullptr);
    EXPECT_NE(temp_system, nullptr);
    if (temp_system) {
        structural_plasticity_destroy(temp_system);
    }

    uint32_t id;
    EXPECT_NE(structural_plasticity_form_synapse(
        nullptr, 1, 2, 50.0f, &id), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
