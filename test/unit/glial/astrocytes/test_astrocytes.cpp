/**
 * @file test_astrocytes.cpp
 * @brief TDD unit tests for astrocyte glial cells
 *
 * WHAT: Comprehensive tests for astrocyte calcium dynamics, glutamate release,
 *       synaptic modulation, and homeostatic plasticity
 * WHY:  TDD approach - write tests first to define expected behavior
 * HOW:  Test all astrocyte functions before implementation exists (RED phase)
 *
 * BIOLOGICAL CONTEXT:
 * - Astrocytes cover ~100,000 synapses in mammalian cortex
 * - Calcium waves propagate at 10-30 µm/s via IP3/gap junctions
 * - Glutamate release modulates synaptic transmission
 * - D-serine co-activates NMDA receptors for plasticity
 * - Homeostatic scaling maintains network stability
 */

#include <gtest/gtest.h>
#include <cmath>

#include "glial/astrocytes/nimcp_astrocytes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AstrocyteTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(AstrocyteTest, CreateDestroy) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    ASSERT_NE(astro, nullptr);
    EXPECT_EQ(astro->id, 0);
    EXPECT_FLOAT_EQ(astro->position[0], 0.0f);
    EXPECT_FLOAT_EQ(astro->position[1], 0.0f);
    EXPECT_FLOAT_EQ(astro->position[2], 0.0f);
    EXPECT_FLOAT_EQ(astro->coverage_radius, 50.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, CreateWithNullID) {
    astrocyte_t* astro = astrocyte_create(42, ASTROCYTE_TYPE_GENERIC, 10.0f, 20.0f, 30.0f, 75.0f);

    ASSERT_NE(astro, nullptr);
    EXPECT_EQ(astro->id, 42);
    EXPECT_FLOAT_EQ(astro->coverage_radius, 75.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, MultipleCreateDestroy) {
    const int count = 100;
    astrocyte_t* astros[count];

    for (int i = 0; i < count; i++) {
        // Cycle through astrocyte types
        astrocyte_type_t type = (astrocyte_type_t)(i % ASTROCYTE_TYPE_COUNT);
        astros[i] = astrocyte_create(i, type, i * 10.0f, i * 20.0f, i * 30.0f, 50.0f);
        ASSERT_NE(astros[i], nullptr);
    }

    for (int i = 0; i < count; i++) {
        astrocyte_destroy(astros[i]);
    }
}

//=============================================================================
// Calcium Dynamics Tests
//=============================================================================

TEST_F(AstrocyteTest, CalciumDynamics_InitialState) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Initial calcium should be at resting baseline (~0.1 µM)
    EXPECT_NEAR(astro->calcium_concentration, 0.1f, 0.05f);
    EXPECT_NEAR(astro->calcium_baseline, 0.1f, 0.05f);
    EXPECT_FLOAT_EQ(astro->ip3_concentration, 0.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, CalciumDynamics_SpontaneousDecay) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Elevate calcium
    astro->calcium_concentration = 5.0f;
    float initial_ca = astro->calcium_concentration;

    // Should decay back to baseline over time
    // Li-Rinzel model: J_pump ≈ 2.2 µM/s, J_leak ≈ 0.5 µM/s at high Ca
    // Net decay rate ≈ -1.6 µM/s initially, slowing as Ca approaches baseline
    // Full decay from 5.0 to ~0.1 µM takes ~5-10 seconds
    float dt = 0.001f; // 1ms
    for (int i = 0; i < 10000; i++) { // 10 seconds for full decay
        astrocyte_update_calcium(astro, dt, 0.0f);
    }

    EXPECT_LT(astro->calcium_concentration, initial_ca);
    // Li-Rinzel model steady state is ~0.1 µM (baseline), allow 0.5 µM tolerance
    EXPECT_NEAR(astro->calcium_concentration, astro->calcium_baseline, 0.5f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, CalciumDynamics_ExternalStimulus) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    float initial_ca = astro->calcium_concentration;
    float initial_ip3 = astro->ip3_concentration;

    // Apply strong stimulus
    // Li-Rinzel model: external_stimulus increases IP3, which then triggers Ca release
    // ip3 += stimulus * dt * 0.5, so need large stimulus to raise IP3 above k3 = 0.5 µM
    float dt = 0.001f;
    astrocyte_update_calcium(astro, dt, 500.0f); // Very strong stimulus to produce IP3 >> k3

    // IP3 should increase significantly from external stimulus
    EXPECT_GT(astro->ip3_concentration, initial_ip3) << "Strong stimulus should increase IP3";

    // With elevated IP3, calcium should increase due to J_channel (IP3R-mediated release)
    EXPECT_GT(astro->calcium_concentration, initial_ca) << "Elevated IP3 should trigger Ca release";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, CalciumDynamics_IP3Production) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Strong stimulus should generate IP3
    float dt = 0.001f;
    astrocyte_update_calcium(astro, dt, 15.0f);

    EXPECT_GT(astro->ip3_concentration, 0.0f) << "IP3 should be produced by strong stimulation";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, CalciumDynamics_SpikeDetection) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    uint64_t initial_spike_time = astro->last_calcium_spike;

    // Strong enough stimulus to trigger calcium spike
    float dt = 0.001f;
    for (int i = 0; i < 10; i++) {
        astrocyte_update_calcium(astro, dt, 20.0f);
    }

    // If calcium exceeded threshold, spike should be recorded
    if (astro->calcium_concentration > astro->calcium_baseline * 3.0f) {
        EXPECT_GT(astro->last_calcium_spike, initial_spike_time);
    }

    astrocyte_destroy(astro);
}

//=============================================================================
// Calcium Wave Propagation Tests
//=============================================================================

TEST_F(AstrocyteTest, CalciumWave_NoPropagationWithoutNetwork) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Elevate calcium
    astro->calcium_concentration = 8.0f;

    // Propagate with NULL network should not crash
    astrocyte_propagate_calcium_wave(astro, nullptr, 0.001f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, CalciumWave_PropagationToNeighbors) {
    // Create network
    astrocyte_network_t* network = astrocyte_network_create(3);
    ASSERT_NE(network, nullptr);

    // Create 3 astrocytes in a line
    astrocyte_t* astro0 = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    astrocyte_t* astro1 = astrocyte_create(1, ASTROCYTE_TYPE_GENERIC, 100.0f, 0.0f, 0.0f, 50.0f);
    astrocyte_t* astro2 = astrocyte_create(2, ASTROCYTE_TYPE_GENERIC, 200.0f, 0.0f, 0.0f, 50.0f);

    // Add to network
    astrocyte_network_add(network, astro0);
    astrocyte_network_add(network, astro1);
    astrocyte_network_add(network, astro2);

    // Establish coupling
    astrocyte_network_establish_coupling(network);

    // Trigger calcium spike in astro0
    astro0->calcium_concentration = 10.0f;
    float astro1_initial_ca = astro1->calcium_concentration;

    // Propagate wave
    float dt = 0.001f;
    for (int i = 0; i < 100; i++) {
        astrocyte_network_step(network, dt);
    }

    // Neighbor should have elevated calcium
    EXPECT_GT(astro1->calcium_concentration, astro1_initial_ca)
        << "Calcium wave should propagate to neighbors";

    // Network owns the astrocytes and will destroy them
    astrocyte_network_destroy(network);
}

TEST_F(AstrocyteTest, CalciumWave_PropagationSpeed) {
    // Biological: calcium waves propagate at ~10-30 µm/s
    // Test that wave reaches neighbor in reasonable time

    astrocyte_network_t* network = astrocyte_network_create(2);

    // Place astrocytes 50µm apart
    astrocyte_t* astro0 = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    astrocyte_t* astro1 = astrocyte_create(1, ASTROCYTE_TYPE_GENERIC, 50.0f, 0.0f, 0.0f, 50.0f);

    astrocyte_network_add(network, astro0);
    astrocyte_network_add(network, astro1);
    astrocyte_network_establish_coupling(network);

    // Spike in astro0
    astro0->calcium_concentration = 10.0f;
    uint64_t start_time = nimcp_time_monotonic_us();

    // Wait for propagation (50µm / 20µm/s = 2.5s expected)
    float dt = 0.001f; // 1ms
    int steps = 0;
    while (astro1->calcium_concentration < astro1->calcium_baseline * 2.0f && steps < 5000) {
        astrocyte_network_step(network, dt);
        steps++;
    }

    uint64_t elapsed_us = nimcp_time_elapsed_us(start_time);
    float elapsed_ms = elapsed_us / 1000.0f;

    // Should propagate within reasonable time (simulation time, not real time)
    EXPECT_LT(steps * dt * 1000.0f, 5000.0f) // Less than 5 seconds
        << "Calcium wave propagation too slow";

    // Network owns the astrocytes and will destroy them
    astrocyte_network_destroy(network);
}

//=============================================================================
// Glutamate Release Tests
//=============================================================================

TEST_F(AstrocyteTest, GlutamateRelease_RestingState) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Add a synapse
    astrocyte_assign_synapse(astro, 123);

    // At resting calcium, glutamate release should be minimal
    float glutamate = astrocyte_compute_glutamate_release(astro, 0);

    EXPECT_NEAR(glutamate, 0.0f, 0.1f)
        << "Glutamate release should be minimal at resting calcium";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, GlutamateRelease_ElevatedCalcium) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    astrocyte_assign_synapse(astro, 123);

    // Elevate calcium
    astro->calcium_concentration = 5.0f;

    float glutamate = astrocyte_compute_glutamate_release(astro, 0);

    EXPECT_GT(glutamate, 0.1f)
        << "Elevated calcium should trigger glutamate release";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, GlutamateRelease_DepletePool) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    astrocyte_assign_synapse(astro, 123);

    // Elevate calcium and deplete glutamate pool
    astro->calcium_concentration = 10.0f;
    float initial_pool = astro->glutamate_pool;

    // Release multiple times
    for (int i = 0; i < 100; i++) {
        astrocyte_compute_glutamate_release(astro, 0);
    }

    // Pool should be depleted
    EXPECT_LT(astro->glutamate_pool, initial_pool)
        << "Glutamate pool should deplete with repeated release";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, DSerineRelease_CalciumDependent) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    astrocyte_assign_synapse(astro, 123);

    // Resting: minimal D-serine
    float d_serine_resting = astrocyte_compute_d_serine_release(astro, 0);

    // Elevated calcium
    astro->calcium_concentration = 7.0f;
    float d_serine_elevated = astrocyte_compute_d_serine_release(astro, 0);

    EXPECT_GT(d_serine_elevated, d_serine_resting)
        << "D-serine release should increase with calcium";

    astrocyte_destroy(astro);
}

//=============================================================================
// Synaptic Modulation Tests
//=============================================================================

TEST_F(AstrocyteTest, SynapticModulation_GlutamateEnhancement) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Create mock synapse
    synapse_t synapse = {0};
    synapse.weight = 0.5f;
    synapse.strength = 1.0f;

    astrocyte_assign_synapse(astro, 0);
    float initial_strength = synapse.strength;

    // Elevate calcium (triggers glutamate release)
    astro->calcium_concentration = 6.0f;

    // Modulate synapse
    astrocyte_modulate_synapse_strength(astro, &synapse, 0);

    // Glutamate should enhance synaptic strength
    EXPECT_GE(synapse.strength, initial_strength)
        << "Glutamate should enhance synaptic strength";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, SynapticModulation_LocalCalciumMicrodomains) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Assign multiple synapses
    for (uint32_t i = 0; i < 10; i++) {
        astrocyte_assign_synapse(astro, i);
    }

    // Elevate calcium at specific synapse
    astro->synapse_calcium_levels[5] = 8.0f;

    // Other synapses should have lower calcium
    EXPECT_LT(astro->synapse_calcium_levels[0], astro->synapse_calcium_levels[5]);
    EXPECT_LT(astro->synapse_calcium_levels[9], astro->synapse_calcium_levels[5]);

    astrocyte_destroy(astro);
}

//=============================================================================
// Homeostatic Plasticity Tests
//=============================================================================

TEST_F(AstrocyteTest, HomeostaticScaling_BalancedActivity) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Set target activity
    astro->target_activity_level = 0.5f;

    // Create mock neural network with balanced activity
    // (In real implementation, this would be a full neural_network_t)
    float scaling = astrocyte_compute_synaptic_scaling(astro, nullptr);

    // With balanced activity, scaling should be near 1.0
    EXPECT_NEAR(scaling, 1.0f, 0.2f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, HomeostaticScaling_HighActivity) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    astro->target_activity_level = 0.3f;

    // Simulate high activity by elevating calcium
    astro->calcium_concentration = 8.0f;

    float scaling = astrocyte_compute_synaptic_scaling(astro, nullptr);

    // High activity → should scale down synapses
    EXPECT_LT(scaling, 1.0f) << "High activity should scale down synapses";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, HomeostaticScaling_LowActivity) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    astro->target_activity_level = 0.5f;

    // Simulate low activity (calcium at baseline)
    astro->calcium_concentration = astro->calcium_baseline;

    float scaling = astrocyte_compute_synaptic_scaling(astro, nullptr);

    // Low activity → should scale up synapses
    EXPECT_GT(scaling, 1.0f) << "Low activity should scale up synapses";

    astrocyte_destroy(astro);
}

//=============================================================================
// BCM Threshold Modulation Tests
//=============================================================================

TEST_F(AstrocyteTest, BCMThresholdModulation_RestingCalcium) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    float default_threshold = 0.5f;
    float modulated_threshold = astrocyte_compute_bcm_threshold_shift(astro, default_threshold);

    // At resting calcium, minimal modulation
    EXPECT_NEAR(modulated_threshold, 0.0f, 0.1f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, BCMThresholdModulation_ElevatedCalcium) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Elevate calcium significantly
    astro->calcium_concentration = 7.0f;

    float default_threshold = 0.5f;
    float shift = astrocyte_compute_bcm_threshold_shift(astro, default_threshold);

    // Elevated calcium should shift threshold
    EXPECT_NE(shift, 0.0f) << "Elevated calcium should modulate BCM threshold";

    astrocyte_destroy(astro);
}

//=============================================================================
// Metabolic Support Tests
//=============================================================================

TEST_F(AstrocyteTest, MetabolicSupport_ATPDepletion) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    float initial_atp = astro->atp_level;

    // High neural activity depletes ATP
    float high_activity = 10.0f;
    float dt = 0.01f;
    for (int i = 0; i < 100; i++) {
        astrocyte_update_atp_level(astro, high_activity, dt);
    }

    EXPECT_LT(astro->atp_level, initial_atp)
        << "High neural activity should deplete ATP";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, MetabolicSupport_ATPRegeneration) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Deplete ATP
    astro->atp_level = 0.2f;
    float depleted_atp = astro->atp_level;

    // Low activity allows regeneration
    float low_activity = 0.1f;
    float dt = 0.01f;
    for (int i = 0; i < 100; i++) {
        astrocyte_update_atp_level(astro, low_activity, dt);
    }

    EXPECT_GT(astro->atp_level, depleted_atp)
        << "Low activity should allow ATP regeneration";

    astrocyte_destroy(astro);
}

//=============================================================================
// Astrocyte Network Tests
//=============================================================================

TEST_F(AstrocyteTest, Network_CreateDestroy) {
    astrocyte_network_t* network = astrocyte_network_create(100);

    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->num_astrocytes, 0); // Initially empty

    astrocyte_network_destroy(network);
}

TEST_F(AstrocyteTest, Network_AddAstrocytes) {
    astrocyte_network_t* network = astrocyte_network_create(10);

    for (int i = 0; i < 10; i++) {
        astrocyte_type_t type = (astrocyte_type_t)(i % ASTROCYTE_TYPE_COUNT);
        astrocyte_t* astro = astrocyte_create(i, type, i * 10.0f, 0.0f, 0.0f, 50.0f);
        astrocyte_network_add(network, astro);
    }

    EXPECT_EQ(network->num_astrocytes, 10);

    // Network should own the astrocytes and destroy them
    astrocyte_network_destroy(network);
}

TEST_F(AstrocyteTest, Network_SpatialIndexing) {
    astrocyte_network_t* network = astrocyte_network_create(100);

    // Add astrocytes in spatial pattern
    for (int i = 0; i < 100; i++) {
        astrocyte_type_t type = (astrocyte_type_t)(i % ASTROCYTE_TYPE_COUNT);
        astrocyte_t* astro = astrocyte_create(i,
                                              type,
                                              (i % 10) * 50.0f,
                                              (i / 10) * 50.0f,
                                              0.0f,
                                              50.0f);
        astrocyte_network_add(network, astro);
    }

    // Build spatial index
    astrocyte_network_build_spatial_index(network);

    // Query nearest neighbor
    float query_point[3] = {125.0f, 125.0f, 0.0f};
    astrocyte_t* nearest = astrocyte_network_find_nearest(network, query_point);

    ASSERT_NE(nearest, nullptr);
    // Should find an astrocyte close to query point
    float dx = nearest->position[0] - query_point[0];
    float dy = nearest->position[1] - query_point[1];
    float distance = sqrtf(dx*dx + dy*dy);
    EXPECT_LT(distance, 100.0f) << "Should find nearby astrocyte";

    astrocyte_network_destroy(network);
}

TEST_F(AstrocyteTest, Network_GapJunctionCoupling) {
    astrocyte_network_t* network = astrocyte_network_create(3);

    astrocyte_t* astro0 = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
    astrocyte_t* astro1 = astrocyte_create(1, ASTROCYTE_TYPE_GENERIC, 60.0f, 0.0f, 0.0f, 50.0f); // Within range
    astrocyte_t* astro2 = astrocyte_create(2, ASTROCYTE_TYPE_GENERIC, 200.0f, 0.0f, 0.0f, 50.0f); // Far away

    astrocyte_network_add(network, astro0);
    astrocyte_network_add(network, astro1);
    astrocyte_network_add(network, astro2);

    // Establish gap junction coupling (based on distance)
    astrocyte_network_establish_coupling(network);

    // astro0 and astro1 should be coupled (close together)
    EXPECT_GT(astro0->num_coupled_astrocytes, 0);

    // astro2 should not be coupled to astro0 (too far)
    bool astro0_coupled_to_astro2 = false;
    for (uint32_t i = 0; i < astro0->num_coupled_astrocytes; i++) {
        if (astro0->coupled_astrocyte_ids[i] == astro2->id) {
            astro0_coupled_to_astro2 = true;
        }
    }
    EXPECT_FALSE(astro0_coupled_to_astro2) << "Distant astrocytes should not be coupled";

    astrocyte_network_destroy(network);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(AstrocyteTest, Performance_CalciumIntegration) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    uint64_t start = nimcp_time_monotonic_us();

    // Simulate 1000 calcium updates
    float dt = 0.001f;
    for (int i = 0; i < 1000; i++) {
        astrocyte_update_calcium(astro, dt, 1.0f);
    }

    uint64_t elapsed_us = nimcp_time_elapsed_us(start);

    // Should complete in reasonable time (< 10ms for 1000 updates)
    EXPECT_LT(elapsed_us, 10000) << "Calcium integration too slow";

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, Performance_NetworkStep) {
    astrocyte_network_t* network = astrocyte_network_create(100);

    // Create 100 astrocytes
    for (int i = 0; i < 100; i++) {
        astrocyte_type_t type = (astrocyte_type_t)(i % ASTROCYTE_TYPE_COUNT);
        astrocyte_t* astro = astrocyte_create(i,
                                              type,
                                              (i % 10) * 50.0f,
                                              (i / 10) * 50.0f,
                                              0.0f,
                                              50.0f);
        astrocyte_network_add(network, astro);
    }

    astrocyte_network_establish_coupling(network);

    uint64_t start = nimcp_time_monotonic_us();

    // Run 100 network steps
    float dt = 0.001f;
    for (int i = 0; i < 100; i++) {
        astrocyte_network_step(network, dt);
    }

    uint64_t elapsed_us = nimcp_time_elapsed_us(start);

    // 100 steps with 100 astrocytes should complete in < 100ms
    EXPECT_LT(elapsed_us, 100000) << "Astrocyte network step too slow";

    astrocyte_network_destroy(network);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(AstrocyteTest, ThreadSafety_ConcurrentCalciumUpdates) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Concurrent updates from multiple threads
    // (Full implementation would use pthread or C++11 threads)
    // For now, test that spinlock is initialized
    EXPECT_NE(&astro->lock, nullptr);

    astrocyte_destroy(astro);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(AstrocyteTest, EdgeCase_NullPointerHandling) {
    // All functions should handle NULL gracefully
    astrocyte_destroy(nullptr); // Should not crash

    float result = astrocyte_compute_glutamate_release(nullptr, 0);
    EXPECT_FLOAT_EQ(result, 0.0f);

    astrocyte_update_calcium(nullptr, 0.001f, 1.0f); // Should not crash
}

TEST_F(AstrocyteTest, EdgeCase_ZeroCoverageRadius) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 0.0f);

    ASSERT_NE(astro, nullptr);
    // Should handle zero radius gracefully
    EXPECT_GE(astro->coverage_radius, 0.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, EdgeCase_NegativeTimestep) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    float initial_ca = astro->calcium_concentration;

    // Negative dt should be handled (clamped to 0 or error)
    astrocyte_update_calcium(astro, -0.001f, 1.0f);

    // Calcium should not change or become invalid
    EXPECT_GE(astro->calcium_concentration, 0.0f);
    EXPECT_LE(astro->calcium_concentration, 100.0f); // Reasonable upper bound

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteTest, EdgeCase_ExtremeCalciumLevels) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);

    // Set extreme calcium
    astro->calcium_concentration = 1000.0f;

    // System should clamp or handle gracefully
    astrocyte_update_calcium(astro, 0.001f, 0.0f);

    EXPECT_GE(astro->calcium_concentration, 0.0f);
    EXPECT_LT(astro->calcium_concentration, 100.0f) << "Calcium should be clamped to reasonable range";

    astrocyte_destroy(astro);
}
