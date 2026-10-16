/**
 * @file test_astrocyte_comprehensive.cpp
 * @brief Comprehensive unit tests for ALL astrocyte functions
 *
 * TEST COVERAGE MATRIX:
 * =====================
 *
 * 1. CALCIUM SYSTEM TESTS (astrocyte_calcium_system_*)
 *    - Creation/destruction lifecycle
 *    - State initialization and validation
 *    - Update dynamics with diffusion verification
 *    - Wave propagation speed (100-2000 µm/s computational range)
 *    - Stimulation response
 *    - Thread safety under concurrent access
 *    - Performance overhead (< 100%)
 *    - Gliotransmitter release triggering
 *
 * 2. INDIVIDUAL ASTROCYTE TESTS (astrocyte_*)
 *    - Creation/destruction lifecycle
 *    - Calcium dynamics (Li-Rinzel simplified)
 *    - IP3 dynamics (production/degradation)
 *    - Neurotransmitter pools (glutamate, D-serine)
 *    - ATP dynamics (production/consumption)
 *    - Synapse assignment and coverage
 *
 * 3. NETWORK TESTS (astrocyte_network_*)
 *    - Network creation/destruction
 *    - Astrocyte addition with capacity management
 *    - Gap junction coupling establishment
 *    - Nearest neighbor spatial search
 *    - Spatial indexing
 *    - Network step integration
 *
 * 4. MODULATION TESTS
 *    - Synaptic scaling (homeostatic plasticity)
 *    - BCM threshold modulation (metaplasticity)
 *    - Synapse strength modulation
 *    - NMDA co-agonist (D-serine) effect
 *
 * 5. INTEGRATION TESTS
 *    - Calcium waves trigger gliotransmitter release
 *    - Released glutamate modulates synapses
 *    - Homeostatic plasticity maintains target activity
 *    - BCM threshold shifts with calcium
 *    - ATP depletion under high activity
 *
 * 6. REGRESSION TESTS
 *    - Backward compatibility with simple implementation
 *    - Parameter ranges stay within biological bounds
 *    - No memory leaks
 *    - Thread safety under concurrent access
 *
 * BIOLOGICAL PARAMETERS:
 * - Baseline calcium: 0.1 µM
 * - Wave threshold: 2.0 µM
 * - Wave speed: 10-20 µm/s (biological), 100-2000 µm/s (computational)
 * - Coupling radius: 100 µm
 * - IP3 production/degradation: 1.0 /s
 * - Calcium diffusion: 0.2 µm²/s
 * - IP3 diffusion: 150 µm²/s
 *
 * @version 1.0
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>

#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/astrocyte_types/nimcp_astrocyte_types.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AstrocyteComprehensiveTest : public ::testing::Test {
protected:
    astrocyte_network_t* network;
    astrocyte_calcium_system_t* calcium_system;
    neural_network_t neural_net;  // neural_network_t is already a pointer

    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Create astrocyte network with 1D chain arrangement
        network = astrocyte_network_create(100);
        ASSERT_NE(network, nullptr);

        // Create 10 astrocytes in a linear chain (50 µm spacing)
        for (uint32_t i = 0; i < 10; i++) {
            float x = i * 50.0f; // µm
            float y = 0.0f;
            float z = 0.0f;

            astrocyte_t* astro = astrocyte_create(
                i, ASTROCYTE_TYPE_GENERIC, x, y, z, 50.0f);
            ASSERT_NE(astro, nullptr);

            nimcp_result_t result = astrocyte_network_add(network, astro);
            ASSERT_EQ(result, NIMCP_SUCCESS);
        }

        // Establish gap junction coupling
        nimcp_result_t result = astrocyte_network_establish_coupling(network);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Create calcium system
        calcium_system = astrocyte_calcium_system_create(network);
        ASSERT_NE(calcium_system, nullptr);

        // Attach to network
        network->calcium_system = calcium_system;

        // Create minimal neural network for integration tests
        network_config_t net_config = {};
        net_config.num_neurons = 100;
        net_config.ei_ratio = 0.8f;
        net_config.learning_rate = 0.01f;
        net_config.stdp_window = 20.0f;
        net_config.refractory_period = 2.0f;
        net_config.min_weight = 0.0f;
        net_config.max_weight = 1.0f;
        net_config.input_size = 10;
        net_config.output_size = 10;

        neural_net = neural_network_create(&net_config);
        ASSERT_NE(neural_net, nullptr);
    }

    void TearDown() override {
        if (calcium_system) {
            astrocyte_calcium_system_destroy(calcium_system);
        }
        if (network) {
            astrocyte_network_destroy(network);
        }
        if (neural_net) {
            neural_network_destroy(neural_net);
        }

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected";
    }

    // Helper: Simulate for specified duration
    void simulate(float duration_seconds, float dt = 0.001f) {
        int num_steps = (int)(duration_seconds / dt);
        for (int step = 0; step < num_steps; step++) {
            astrocyte_calcium_system_update(calcium_system, dt, nullptr);
        }
    }

    // Helper: Check if calcium is in biological range
    bool is_calcium_valid(float ca) {
        return ca >= 0.0f && ca <= 10.0f;
    }

    // Helper: Check if IP3 is in biological range
    bool is_ip3_valid(float ip3) {
        return ip3 >= 0.0f && ip3 <= 5.0f;
    }
};

//=============================================================================
// 1. CALCIUM SYSTEM TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// 1.1 Creation/Destruction Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_Creation) {
    EXPECT_EQ(calcium_system->num_astrocytes, 10);
    EXPECT_NE(calcium_system->calcium, nullptr);
    EXPECT_NE(calcium_system->ip3, nullptr);
    EXPECT_NE(calcium_system->calcium_er, nullptr);
    EXPECT_NE(calcium_system->workspace_dCa, nullptr);
    EXPECT_NE(calcium_system->workspace_dIP3, nullptr);
    EXPECT_NE(calcium_system->workspace_dCaER, nullptr);
    EXPECT_EQ(calcium_system->network, network);
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_Destruction) {
    astrocyte_calcium_system_t* temp = astrocyte_calcium_system_create(network);
    ASSERT_NE(temp, nullptr);

    // Should not crash
    astrocyte_calcium_system_destroy(temp);
    astrocyte_calcium_system_destroy(nullptr); // NULL-safe
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_InvalidCreation) {
    // NULL network
    astrocyte_calcium_system_t* sys = astrocyte_calcium_system_create(nullptr);
    EXPECT_EQ(sys, nullptr);

    // Empty network
    astrocyte_network_t* empty_net = astrocyte_network_create(10);
    ASSERT_NE(empty_net, nullptr);

    astrocyte_calcium_system_t* sys2 = astrocyte_calcium_system_create(empty_net);
    EXPECT_EQ(sys2, nullptr);

    astrocyte_network_destroy(empty_net);
}

//-----------------------------------------------------------------------------
// 1.2 State Initialization Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_InitialState) {
    for (uint32_t i = 0; i < 10; i++) {
        float ca = astrocyte_calcium_system_get_calcium(calcium_system, i);
        EXPECT_NEAR(ca, ASTROCYTE_BASELINE_CALCIUM_UM, 0.01f)
            << "Astrocyte " << i << " initial calcium";

        float ip3 = astrocyte_calcium_system_get_ip3(calcium_system, i);
        EXPECT_NEAR(ip3, 0.0f, 0.01f)
            << "Astrocyte " << i << " initial IP3";
    }
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_ParameterInitialization) {
    EXPECT_FLOAT_EQ(calcium_system->D_ca, CALCIUM_DIFFUSION_COEFF);
    EXPECT_FLOAT_EQ(calcium_system->D_ip3, IP3_DIFFUSION_COEFF);
    EXPECT_FLOAT_EQ(calcium_system->ip3_production_rate, IP3_PRODUCTION_RATE);
    EXPECT_FLOAT_EQ(calcium_system->ip3_degradation_rate, IP3_DEGRADATION_RATE);
    EXPECT_FLOAT_EQ(calcium_system->ca_release_flux, CALCIUM_RELEASE_FLUX);
    EXPECT_FLOAT_EQ(calcium_system->ca_uptake_rate, CALCIUM_UPTAKE_RATE);
}

//-----------------------------------------------------------------------------
// 1.3 Update Dynamics Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_UpdateBasic) {
    float ca_before = astrocyte_calcium_system_get_calcium(calcium_system, 0);

    float dt = 0.001f;
    astrocyte_calcium_system_update(calcium_system, dt, nullptr);

    float ca_after = astrocyte_calcium_system_get_calcium(calcium_system, 0);

    // Should remain near baseline without stimulation
    EXPECT_NEAR(ca_after, ca_before, 0.1f);
    EXPECT_TRUE(is_calcium_valid(ca_after));
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_DiffusionOccurs) {
    // Stimulate astrocyte 0 to high calcium
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    float ca0_before = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    float ca1_before = astrocyte_calcium_system_get_calcium(calcium_system, 1);

    EXPECT_GT(ca0_before, ca1_before) << "Source should have higher calcium";

    // Simulate diffusion
    simulate(0.5f); // 500ms

    float ca0_after = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    float ca1_after = astrocyte_calcium_system_get_calcium(calcium_system, 1);

    // Neighbor should have increased, source should have decreased
    EXPECT_GT(ca1_after, ca1_before) << "Diffusion should increase neighbor calcium";
    EXPECT_LT(ca0_after, ca0_before) << "Diffusion should decrease source calcium";
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_ExternalStimulus) {
    float stimulus[10];
    for (int i = 0; i < 10; i++) {
        stimulus[i] = (i == 5) ? 10.0f : 0.0f; // Stimulate middle astrocyte
    }

    float dt = 0.001f;
    for (int step = 0; step < 100; step++) {
        astrocyte_calcium_system_update(calcium_system, dt, stimulus);
    }

    float ca5 = astrocyte_calcium_system_get_calcium(calcium_system, 5);
    EXPECT_GT(ca5, ASTROCYTE_BASELINE_CALCIUM_UM * 2.0f)
        << "Stimulated astrocyte should have elevated calcium";
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_BiologicalBounds) {
    // Strong stimulation everywhere
    for (uint32_t i = 0; i < 10; i++) {
        astrocyte_calcium_system_stimulate(calcium_system, i, 100.0f);
    }

    simulate(1.0f); // 1 second

    // All values should stay in biological range
    for (uint32_t i = 0; i < 10; i++) {
        float ca = astrocyte_calcium_system_get_calcium(calcium_system, i);
        float ip3 = astrocyte_calcium_system_get_ip3(calcium_system, i);

        EXPECT_TRUE(is_calcium_valid(ca))
            << "Astrocyte " << i << " calcium out of bounds: " << ca;
        EXPECT_TRUE(is_ip3_valid(ip3))
            << "Astrocyte " << i << " IP3 out of bounds: " << ip3;
    }
}

//-----------------------------------------------------------------------------
// 1.4 Wave Propagation Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_WavePropagation) {
    // Stimulate astrocyte 0
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    // Verify initial stimulus
    float ca0 = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    EXPECT_GT(ca0, ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM);

    // Simulate wave propagation
    simulate(1.0f); // 1 second

    // Wave should have propagated to neighbors
    float ca1 = astrocyte_calcium_system_get_calcium(calcium_system, 1);
    float ca2 = astrocyte_calcium_system_get_calcium(calcium_system, 2);

    EXPECT_GT(ca1, ASTROCYTE_BASELINE_CALCIUM_UM * 2.0f)
        << "Wave should reach first neighbor";
    EXPECT_GT(ca2, ASTROCYTE_BASELINE_CALCIUM_UM * 1.5f)
        << "Wave should reach second neighbor";
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_WaveSpeed) {
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    // Track when neighbor crosses threshold
    float dt = 0.0001f; // 0.1ms for accurate tracking
    int steps_to_neighbor = 0;
    bool neighbor_activated = false;

    for (int step = 0; step < 100000 && !neighbor_activated; step++) {
        astrocyte_calcium_system_update(calcium_system, dt, nullptr);

        float ca1 = astrocyte_calcium_system_get_calcium(calcium_system, 1);
        if (ca1 > ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM) {
            steps_to_neighbor = step;
            neighbor_activated = true;
        }
    }

    ASSERT_TRUE(neighbor_activated) << "Wave should reach neighbor";

    float time_elapsed = steps_to_neighbor * dt; // seconds
    float distance = 50.0f; // µm
    float wave_speed = distance / time_elapsed; // µm/s

    // Computational range (faster than biological for performance)
    EXPECT_GE(wave_speed, 100.0f) << "Wave speed too slow";
    EXPECT_LE(wave_speed, 2000.0f) << "Wave speed too fast";

    std::cout << "Measured wave speed: " << wave_speed << " µm/s" << std::endl;
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_WaveSpeedMeasurement) {
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    simulate(0.5f); // 500ms

    float measured_speed = astrocyte_calcium_system_get_wave_speed(calcium_system);

    if (measured_speed > 0.0f) {
        EXPECT_GE(measured_speed, 5.0f);
        EXPECT_LE(measured_speed, 50.0f);
        std::cout << "System-measured wave speed: " << measured_speed << " µm/s" << std::endl;
    }
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_WaveDecay) {
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    simulate(1.0f);

    // Wave should decay with distance
    float ca0 = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    float ca1 = astrocyte_calcium_system_get_calcium(calcium_system, 1);
    float ca5 = astrocyte_calcium_system_get_calcium(calcium_system, 5);
    float ca9 = astrocyte_calcium_system_get_calcium(calcium_system, 9);

    // Suppress unused variable warnings (ca0, ca5 may be useful for debugging)
    (void)ca0;
    (void)ca5;

    // Gradient should exist (though not necessarily monotonic due to dynamics)
    EXPECT_LT(ca9, ca1) << "Distant astrocyte should have less calcium than near";
}

//-----------------------------------------------------------------------------
// 1.5 Stimulation Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_StimulateBasic) {
    float ca_before = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    float ip3_before = astrocyte_calcium_system_get_ip3(calcium_system, 0);

    astrocyte_calcium_system_stimulate(calcium_system, 0, 5.0f);

    float ca_after = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    float ip3_after = astrocyte_calcium_system_get_ip3(calcium_system, 0);

    EXPECT_GT(ca_after, ca_before);
    EXPECT_GT(ip3_after, ip3_before);
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_StimulateIntensity) {
    astrocyte_calcium_system_stimulate(calcium_system, 0, 1.0f);
    float ca_weak = astrocyte_calcium_system_get_calcium(calcium_system, 0);

    astrocyte_calcium_system_stimulate(calcium_system, 1, 10.0f);
    float ca_strong = astrocyte_calcium_system_get_calcium(calcium_system, 1);

    EXPECT_GT(ca_strong, ca_weak) << "Stronger stimulus should produce more calcium";
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_StimulateInvalid) {
    // Invalid ID - should not crash
    astrocyte_calcium_system_stimulate(nullptr, 0, 5.0f);
    astrocyte_calcium_system_stimulate(calcium_system, 999, 5.0f);

    // Should still be at baseline
    float ca = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    EXPECT_NEAR(ca, ASTROCYTE_BASELINE_CALCIUM_UM, 0.1f);
}

//-----------------------------------------------------------------------------
// 1.6 Gliotransmitter Release Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_GliotransmitterThreshold) {
    // Below threshold
    bool should_release = astrocyte_calcium_system_should_release_gliotransmitter(
        calcium_system, 0);
    EXPECT_FALSE(should_release);

    // Above threshold
    astrocyte_calcium_system_stimulate(calcium_system, 0, 8.0f);
    should_release = astrocyte_calcium_system_should_release_gliotransmitter(
        calcium_system, 0);
    EXPECT_TRUE(should_release);
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_GliotransmitterMultiple) {
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    // Simulate briefly to allow wave propagation but not full decay
    simulate(0.1f);

    // Count astrocytes that should release
    int release_count = 0;
    for (uint32_t i = 0; i < 10; i++) {
        if (astrocyte_calcium_system_should_release_gliotransmitter(calcium_system, i)) {
            release_count++;
        }
    }

    EXPECT_GE(release_count, 1) << "At least source should release";
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_GliotransmitterInvalid) {
    // Invalid parameters - should return false
    bool result = astrocyte_calcium_system_should_release_gliotransmitter(nullptr, 0);
    EXPECT_FALSE(result);

    result = astrocyte_calcium_system_should_release_gliotransmitter(calcium_system, 999);
    EXPECT_FALSE(result);
}

//-----------------------------------------------------------------------------
// 1.7 Performance Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_PerformanceOverhead) {
    float dt = 0.001f;
    int num_steps = 10000; // 10 seconds

    uint64_t sim_start = nimcp_time_monotonic_us();

    for (int step = 0; step < num_steps; step++) {
        astrocyte_calcium_system_update(calcium_system, dt, nullptr);
    }

    uint64_t sim_end = nimcp_time_monotonic_us();
    uint64_t total_sim_time = sim_end - sim_start;

    float overhead_percent = astrocyte_calcium_system_get_overhead_percent(
        calcium_system, total_sim_time);

    std::cout << "Performance overhead: " << overhead_percent << "%" << std::endl;

    // Relaxed threshold for computationally intensive calcium dynamics
    EXPECT_LT(overhead_percent, 100.0f);
    EXPECT_GT(overhead_percent, 0.0f);
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_PerformanceScaling) {
    // Create larger network
    astrocyte_network_t* large_net = astrocyte_network_create(1000);
    ASSERT_NE(large_net, nullptr);

    for (uint32_t i = 0; i < 100; i++) {
        float x = (i % 10) * 50.0f;
        float y = (i / 10) * 50.0f;
        astrocyte_t* astro = astrocyte_create(i, ASTROCYTE_TYPE_GENERIC, x, y, 0.0f, 50.0f);
        astrocyte_network_add(large_net, astro);
    }

    astrocyte_network_establish_coupling(large_net);
    astrocyte_calcium_system_t* large_sys = astrocyte_calcium_system_create(large_net);
    ASSERT_NE(large_sys, nullptr);

    // Time single update
    uint64_t start = nimcp_time_monotonic_us();
    astrocyte_calcium_system_update(large_sys, 0.001f, nullptr);
    uint64_t end = nimcp_time_monotonic_us();

    uint64_t update_time = end - start;
    std::cout << "Update time for 100 astrocytes: " << update_time << " µs" << std::endl;

    EXPECT_LT(update_time, 10000) << "Should complete in < 10ms";

    astrocyte_calcium_system_destroy(large_sys);
    astrocyte_network_destroy(large_net);
}

//-----------------------------------------------------------------------------
// 1.8 Thread Safety Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_ConcurrentReads) {
    std::atomic<int> errors{0};

    auto reader = [&]() {
        for (int i = 0; i < 1000; i++) {
            float ca = astrocyte_calcium_system_get_calcium(calcium_system, 0);
            if (!is_calcium_valid(ca)) {
                errors++;
            }
        }
    };

    std::thread t1(reader);
    std::thread t2(reader);

    t1.join();
    t2.join();

    EXPECT_EQ(errors, 0) << "Concurrent reads should be safe";
}

TEST_F(AstrocyteComprehensiveTest, CalciumSystem_ConcurrentStimulation) {
    auto stimulator = [&](uint32_t id) {
        for (int i = 0; i < 100; i++) {
            astrocyte_calcium_system_stimulate(calcium_system, id, 5.0f);
        }
    };

    std::thread t1(stimulator, 0);
    std::thread t2(stimulator, 1);

    t1.join();
    t2.join();

    // Should not crash, values should be valid
    float ca0 = astrocyte_calcium_system_get_calcium(calcium_system, 0);
    float ca1 = astrocyte_calcium_system_get_calcium(calcium_system, 1);

    EXPECT_TRUE(is_calcium_valid(ca0));
    EXPECT_TRUE(is_calcium_valid(ca1));
}

//=============================================================================
// 2. INDIVIDUAL ASTROCYTE TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// 2.1 Creation/Destruction Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Astrocyte_Creation) {
    astrocyte_t* astro = astrocyte_create(42, ASTROCYTE_TYPE_V1_SENSORY,
                                          10.0f, 20.0f, 30.0f, 75.0f);

    ASSERT_NE(astro, nullptr);
    EXPECT_EQ(astro->id, 42);
    EXPECT_EQ(astro->type, ASTROCYTE_TYPE_V1_SENSORY);
    EXPECT_FLOAT_EQ(astro->position[0], 10.0f);
    EXPECT_FLOAT_EQ(astro->position[1], 20.0f);
    EXPECT_FLOAT_EQ(astro->position[2], 30.0f);
    EXPECT_FLOAT_EQ(astro->coverage_radius, 75.0f);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_InitialState) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0, 0, 0, 50.0f);

    EXPECT_NEAR(astro->calcium_concentration, ASTROCYTE_BASELINE_CALCIUM_UM, 0.01f);
    EXPECT_NEAR(astro->calcium_baseline, ASTROCYTE_BASELINE_CALCIUM_UM, 0.01f);
    EXPECT_FLOAT_EQ(astro->ip3_concentration, 0.0f);
    EXPECT_FLOAT_EQ(astro->glutamate_pool, 1.0f);
    EXPECT_FLOAT_EQ(astro->d_serine_pool, 1.0f);
    EXPECT_FLOAT_EQ(astro->atp_level, 1.0f);
    EXPECT_EQ(astro->num_covered_synapses, 0);
    EXPECT_EQ(astro->num_coupled_astrocytes, 0);

    astrocyte_destroy(astro);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_InvalidCreation) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0, 0, 0, -10.0f);
    EXPECT_EQ(astro, nullptr) << "Negative radius should fail";
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_Destruction) {
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0, 0, 0, 50.0f);
    ASSERT_NE(astro, nullptr);

    astrocyte_destroy(astro);
    astrocyte_destroy(nullptr); // NULL-safe
}

//-----------------------------------------------------------------------------
// 2.2 Calcium Dynamics Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Astrocyte_CalciumDecay) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->calcium_concentration = 5.0f;
    float initial_ca = astro->calcium_concentration;

    float dt = 0.001f;
    for (int i = 0; i < 1000; i++) {
        astrocyte_update_calcium(astro, dt, 0.0f);
    }

    EXPECT_LT(astro->calcium_concentration, initial_ca);
    EXPECT_NEAR(astro->calcium_concentration, astro->calcium_baseline, 0.5f);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_CalciumStimulus) {
    astrocyte_t* astro = network->astrocytes[0];

    float initial_ca = astro->calcium_concentration;

    // Apply stimulus with sufficient magnitude
    astrocyte_update_calcium(astro, 0.001f, 100.0f);

    EXPECT_GT(astro->calcium_concentration, initial_ca);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_CalciumBounds) {
    astrocyte_t* astro = network->astrocytes[0];

    // Extreme stimulus
    for (int i = 0; i < 100; i++) {
        astrocyte_update_calcium(astro, 0.001f, 1000.0f);
    }

    EXPECT_TRUE(is_calcium_valid(astro->calcium_concentration));
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_InvalidCalciumUpdate) {
    astrocyte_t* astro = network->astrocytes[0];

    astrocyte_update_calcium(nullptr, 0.001f, 0.0f); // Should not crash
    astrocyte_update_calcium(astro, -0.001f, 0.0f);  // Negative dt

    // Should remain valid
    EXPECT_TRUE(is_calcium_valid(astro->calcium_concentration));
}

//-----------------------------------------------------------------------------
// 2.3 IP3 Dynamics Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Astrocyte_IP3Production) {
    astrocyte_t* astro = network->astrocytes[0];

    EXPECT_NEAR(astro->ip3_concentration, 0.0f, 0.01f);

    astrocyte_update_calcium(astro, 0.001f, 15.0f); // Strong stimulus

    EXPECT_GT(astro->ip3_concentration, 0.0f);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_IP3Degradation) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->ip3_concentration = 3.0f;
    float initial_ip3 = astro->ip3_concentration;

    float dt = 0.001f;
    for (int i = 0; i < 5000; i++) {
        astrocyte_update_calcium(astro, dt, 0.0f);
    }

    EXPECT_LT(astro->ip3_concentration, initial_ip3);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_IP3Bounds) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->ip3_concentration = 10.0f; // Above max

    astrocyte_update_calcium(astro, 0.001f, 0.0f);

    EXPECT_TRUE(is_ip3_valid(astro->ip3_concentration));
}

//-----------------------------------------------------------------------------
// 2.4 Neurotransmitter Pool Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Astrocyte_GlutamateRelease) {
    astrocyte_t* astro = network->astrocytes[0];

    // Assign a dummy synapse
    astrocyte_assign_synapse(astro, 0);

    // Low calcium - no release
    astro->calcium_concentration = ASTROCYTE_BASELINE_CALCIUM_UM;
    float glu_low = astrocyte_compute_glutamate_release(astro, 0);
    EXPECT_NEAR(glu_low, 0.0f, 0.01f);

    // High calcium - release
    astro->calcium_concentration = 5.0f;
    float glu_high = astrocyte_compute_glutamate_release(astro, 0);
    EXPECT_GT(glu_high, 0.0f);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_GlutamateDepletion) {
    astrocyte_t* astro = network->astrocytes[0];
    astrocyte_assign_synapse(astro, 0);

    astro->calcium_concentration = 5.0f;
    astro->glutamate_pool = 1.0f;

    // Repeated release should deplete pool
    for (int i = 0; i < 100; i++) {
        astrocyte_compute_glutamate_release(astro, 0);
    }

    EXPECT_LT(astro->glutamate_pool, 1.0f);
    EXPECT_GE(astro->glutamate_pool, 0.0f);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_DSerineRelease) {
    astrocyte_t* astro = network->astrocytes[0];
    astrocyte_assign_synapse(astro, 0);

    // Low calcium
    astro->calcium_concentration = ASTROCYTE_BASELINE_CALCIUM_UM;
    float dser_low = astrocyte_compute_d_serine_release(astro, 0);
    EXPECT_NEAR(dser_low, 0.0f, 0.01f);

    // High calcium
    astro->calcium_concentration = 3.0f;
    float dser_high = astrocyte_compute_d_serine_release(astro, 0);
    EXPECT_GT(dser_high, 0.0f);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_NeurotransmitterInvalid) {
    astrocyte_t* astro = network->astrocytes[0];

    float glu = astrocyte_compute_glutamate_release(nullptr, 0);
    EXPECT_FLOAT_EQ(glu, 0.0f);

    glu = astrocyte_compute_glutamate_release(astro, 999);
    EXPECT_FLOAT_EQ(glu, 0.0f);
}

//-----------------------------------------------------------------------------
// 2.5 ATP Dynamics Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Astrocyte_ATPProduction) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->atp_level = 0.5f;
    float initial_atp = astro->atp_level;

    astrocyte_update_atp_level(astro, 0.0f, 1.0f); // Low activity

    EXPECT_GT(astro->atp_level, initial_atp);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_ATPConsumption) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->atp_level = 1.0f;
    float initial_atp = astro->atp_level;

    astrocyte_update_atp_level(astro, 10.0f, 1.0f); // High activity

    EXPECT_LT(astro->atp_level, initial_atp);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_ATPBounds) {
    astrocyte_t* astro = network->astrocytes[0];

    // Extreme consumption
    astro->atp_level = 0.1f;
    for (int i = 0; i < 100; i++) {
        astrocyte_update_atp_level(astro, 100.0f, 0.01f);
    }

    EXPECT_GE(astro->atp_level, 0.0f);
    EXPECT_LE(astro->atp_level, 1.0f);
}

//-----------------------------------------------------------------------------
// 2.6 Synapse Assignment Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Astrocyte_AssignSynapse) {
    astrocyte_t* astro = network->astrocytes[0];

    EXPECT_EQ(astro->num_covered_synapses, 0);

    nimcp_result_t result = astrocyte_assign_synapse(astro, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(astro->num_covered_synapses, 1);
    EXPECT_EQ(astro->covered_synapse_ids[0], 100);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_AssignMultipleSynapses) {
    astrocyte_t* astro = network->astrocytes[0];

    for (uint32_t i = 0; i < 10; i++) {
        nimcp_result_t result = astrocyte_assign_synapse(astro, i);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(astro->num_covered_synapses, 10);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_AssignSynapseInvalid) {
    nimcp_result_t result = astrocyte_assign_synapse(nullptr, 0);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

//-----------------------------------------------------------------------------
// 2.7 Calcium Wave Propagation Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Astrocyte_WavePropagation) {
    astrocyte_t* astro0 = network->astrocytes[0];
    astrocyte_t* astro1 = network->astrocytes[1];

    astro0->calcium_concentration = 5.0f;
    float ca1_before = astro1->calcium_concentration;

    astrocyte_propagate_calcium_wave(astro0, network, 0.001f);

    float ca1_after = astro1->calcium_concentration;

    // Neighbor should receive calcium
    EXPECT_GT(ca1_after, ca1_before);
}

TEST_F(AstrocyteComprehensiveTest, Astrocyte_NoWaveBelowThreshold) {
    astrocyte_t* astro0 = network->astrocytes[0];
    astrocyte_t* astro1 = network->astrocytes[1];

    astro0->calcium_concentration = 1.0f; // Below threshold
    float ca1_before = astro1->calcium_concentration;

    astrocyte_propagate_calcium_wave(astro0, network, 0.001f);

    float ca1_after = astro1->calcium_concentration;

    EXPECT_NEAR(ca1_after, ca1_before, 0.01f);
}

//=============================================================================
// 3. NETWORK TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// 3.1 Network Creation/Destruction Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Network_Creation) {
    astrocyte_network_t* net = astrocyte_network_create(50);

    ASSERT_NE(net, nullptr);
    EXPECT_EQ(net->capacity, 50);
    EXPECT_EQ(net->num_astrocytes, 0);
    EXPECT_NE(net->astrocytes, nullptr);

    astrocyte_network_destroy(net);
}

TEST_F(AstrocyteComprehensiveTest, Network_Destruction) {
    astrocyte_network_t* net = astrocyte_network_create(10);
    ASSERT_NE(net, nullptr);

    astrocyte_network_destroy(net);
    astrocyte_network_destroy(nullptr); // NULL-safe
}

//-----------------------------------------------------------------------------
// 3.2 Astrocyte Addition Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Network_AddAstrocyte) {
    astrocyte_network_t* net = astrocyte_network_create(10);
    ASSERT_NE(net, nullptr);

    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0, 0, 0, 50.0f);
    ASSERT_NE(astro, nullptr);

    nimcp_result_t result = astrocyte_network_add(net, astro);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(net->num_astrocytes, 1);

    astrocyte_network_destroy(net);
}

TEST_F(AstrocyteComprehensiveTest, Network_AddMultipleAstrocytes) {
    astrocyte_network_t* net = astrocyte_network_create(5);
    ASSERT_NE(net, nullptr);

    for (int i = 0; i < 10; i++) {
        astrocyte_t* astro = astrocyte_create(i, ASTROCYTE_TYPE_GENERIC,
                                              i * 10.0f, 0, 0, 50.0f);
        nimcp_result_t result = astrocyte_network_add(net, astro);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(net->num_astrocytes, 10);
    EXPECT_GE(net->capacity, 10); // Should have resized

    astrocyte_network_destroy(net);
}

TEST_F(AstrocyteComprehensiveTest, Network_AddInvalid) {
    astrocyte_network_t* net = astrocyte_network_create(10);

    nimcp_result_t result = astrocyte_network_add(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    result = astrocyte_network_add(net, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    astrocyte_network_destroy(net);
}

//-----------------------------------------------------------------------------
// 3.3 Gap Junction Coupling Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Network_EstablishCoupling) {
    EXPECT_EQ(network->num_astrocytes, 10);

    // Check coupling was established in SetUp
    astrocyte_t* astro0 = network->astrocytes[0];
    EXPECT_GT(astro0->num_coupled_astrocytes, 0);

    // First astrocyte should be coupled to second (within 100µm)
    bool found_neighbor = false;
    for (uint32_t i = 0; i < astro0->num_coupled_astrocytes; i++) {
        if (astro0->coupled_astrocyte_ids[i] == 1) {
            found_neighbor = true;
            EXPECT_GT(astro0->coupling_strengths[i], 0.0f);
        }
    }
    EXPECT_TRUE(found_neighbor);
}

TEST_F(AstrocyteComprehensiveTest, Network_CouplingRadius) {
    astrocyte_network_t* net = astrocyte_network_create(10);

    // Create two astrocytes far apart
    astrocyte_t* astro0 = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0, 0, 0, 50.0f);
    astrocyte_t* astro1 = astrocyte_create(1, ASTROCYTE_TYPE_GENERIC, 200.0f, 0, 0, 50.0f);

    astrocyte_network_add(net, astro0);
    astrocyte_network_add(net, astro1);

    astrocyte_network_establish_coupling(net);

    // Should not be coupled (distance > coupling_radius)
    EXPECT_EQ(astro0->num_coupled_astrocytes, 0);

    astrocyte_network_destroy(net);
}

TEST_F(AstrocyteComprehensiveTest, Network_CouplingStrength) {
    astrocyte_t* astro0 = network->astrocytes[0];

    if (astro0->num_coupled_astrocytes > 0) {
        for (uint32_t i = 0; i < astro0->num_coupled_astrocytes; i++) {
            EXPECT_GT(astro0->coupling_strengths[i], 0.0f);
            EXPECT_LT(astro0->coupling_strengths[i], 100000.0f); // Reasonable range
        }
    }
}

//-----------------------------------------------------------------------------
// 3.4 Spatial Search Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Network_FindNearest) {
    float point[3] = {25.0f, 0.0f, 0.0f}; // Between astrocyte 0 and 1

    astrocyte_t* nearest = astrocyte_network_find_nearest(network, point);

    ASSERT_NE(nearest, nullptr);
    // Should be astrocyte 0 or 1
    EXPECT_TRUE(nearest->id == 0 || nearest->id == 1);
}

TEST_F(AstrocyteComprehensiveTest, Network_FindNearestExact) {
    float point[3] = {100.0f, 0.0f, 0.0f}; // Exactly at astrocyte 2

    astrocyte_t* nearest = astrocyte_network_find_nearest(network, point);

    ASSERT_NE(nearest, nullptr);
    EXPECT_EQ(nearest->id, 2);
}

TEST_F(AstrocyteComprehensiveTest, Network_FindNearestInvalid) {
    astrocyte_t* nearest = astrocyte_network_find_nearest(nullptr, nullptr);
    EXPECT_EQ(nearest, nullptr);

    astrocyte_network_t* empty_net = astrocyte_network_create(10);
    float point[3] = {0, 0, 0};
    nearest = astrocyte_network_find_nearest(empty_net, point);
    EXPECT_EQ(nearest, nullptr);

    astrocyte_network_destroy(empty_net);
}

//-----------------------------------------------------------------------------
// 3.5 Network Step Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Network_Step) {
    float dt = 0.001f;

    astrocyte_network_step(network, dt);

    // Should not crash, values should be valid
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        EXPECT_TRUE(is_calcium_valid(astro->calcium_concentration));
    }
}

TEST_F(AstrocyteComprehensiveTest, Network_StepInvalid) {
    astrocyte_network_step(nullptr, 0.001f); // Should not crash
    astrocyte_network_step(network, -0.001f); // Invalid dt
}

//-----------------------------------------------------------------------------
// 3.6 Network Statistics Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Network_GetStats) {
    float avg_ca, max_ca, avg_glu;

    astrocyte_network_get_stats(network, &avg_ca, &max_ca, &avg_glu);

    EXPECT_TRUE(is_calcium_valid(avg_ca));
    EXPECT_TRUE(is_calcium_valid(max_ca));
    EXPECT_GE(avg_glu, 0.0f);
    EXPECT_LE(avg_glu, 1.0f);
}

TEST_F(AstrocyteComprehensiveTest, Network_GetStatsAfterStimulation) {
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);
    simulate(0.1f);

    float avg_ca, max_ca, avg_glu;
    astrocyte_network_get_stats(network, &avg_ca, &max_ca, &avg_glu);

    EXPECT_GT(max_ca, ASTROCYTE_BASELINE_CALCIUM_UM);
}

TEST_F(AstrocyteComprehensiveTest, Network_GetStatsInvalid) {
    float avg_ca, max_ca, avg_glu;

    astrocyte_network_get_stats(nullptr, &avg_ca, &max_ca, &avg_glu);

    EXPECT_FLOAT_EQ(avg_ca, 0.0f);
    EXPECT_FLOAT_EQ(max_ca, 0.0f);
    EXPECT_FLOAT_EQ(avg_glu, 0.0f);
}

//=============================================================================
// 4. MODULATION TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// 4.1 Synaptic Scaling Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Modulation_SynapticScaling) {
    astrocyte_t* astro = network->astrocytes[0];

    float scaling = astrocyte_compute_synaptic_scaling(astro, neural_net);

    EXPECT_GE(scaling, 0.5f);
    EXPECT_LE(scaling, 2.0f);
}

TEST_F(AstrocyteComprehensiveTest, Modulation_SynapticScalingHighActivity) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->calcium_concentration = 5.0f; // High activity
    float scaling = astrocyte_compute_synaptic_scaling(astro, neural_net);

    EXPECT_LT(scaling, 1.0f) << "High activity should scale down";
}

TEST_F(AstrocyteComprehensiveTest, Modulation_SynapticScalingLowActivity) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->calcium_concentration = 0.05f; // Low activity
    float scaling = astrocyte_compute_synaptic_scaling(astro, neural_net);

    EXPECT_GT(scaling, 1.0f) << "Low activity should scale up";
}

TEST_F(AstrocyteComprehensiveTest, Modulation_SynapticScalingInvalid) {
    float scaling = astrocyte_compute_synaptic_scaling(nullptr, neural_net);
    EXPECT_FLOAT_EQ(scaling, 1.0f);
}

//-----------------------------------------------------------------------------
// 4.2 BCM Threshold Modulation Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Modulation_BCMThresholdShift) {
    astrocyte_t* astro = network->astrocytes[0];

    float default_threshold = 1.0f;
    float shift = astrocyte_compute_bcm_threshold_shift(astro, default_threshold);

    EXPECT_GE(shift, -0.5f);
    EXPECT_LE(shift, 0.5f);
}

TEST_F(AstrocyteComprehensiveTest, Modulation_BCMThresholdHighCalcium) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->calcium_concentration = 3.0f;
    float shift = astrocyte_compute_bcm_threshold_shift(astro, 1.0f);

    EXPECT_GT(shift, 0.0f) << "High calcium should increase threshold";
}

TEST_F(AstrocyteComprehensiveTest, Modulation_BCMThresholdInvalid) {
    float shift = astrocyte_compute_bcm_threshold_shift(nullptr, 1.0f);
    EXPECT_FLOAT_EQ(shift, 0.0f);
}

//-----------------------------------------------------------------------------
// 4.3 Synapse Strength Modulation Tests
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Modulation_SynapseStrength) {
    astrocyte_t* astro = network->astrocytes[0];
    astrocyte_assign_synapse(astro, 0);

    // Create a synapse
    synapse_t synapse;
    synapse.strength = 1.0f;
    synapse.target_id = 1;  // Target neuron (pre-neuron is implicit)

    float initial_strength = synapse.strength;

    astro->calcium_concentration = 3.0f;
    astrocyte_modulate_synapse_strength(astro, &synapse, 0);

    // Modulation can increase or maintain strength
    EXPECT_GE(synapse.strength, initial_strength * 0.5f);
}

TEST_F(AstrocyteComprehensiveTest, Modulation_SynapseStrengthInvalid) {
    synapse_t synapse;
    synapse.strength = 1.0f;

    astrocyte_modulate_synapse_strength(nullptr, &synapse, 0);
    // Should not crash
}

//=============================================================================
// 5. INTEGRATION TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// 5.1 Calcium-Gliotransmitter Coupling
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Integration_CalciumTriggersRelease) {
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    // Simulate briefly to allow wave propagation but not full decay
    simulate(0.1f);

    int release_count = 0;
    for (uint32_t i = 0; i < 10; i++) {
        if (astrocyte_calcium_system_should_release_gliotransmitter(calcium_system, i)) {
            release_count++;
        }
    }

    EXPECT_GE(release_count, 1) << "Wave should trigger release";
}

//-----------------------------------------------------------------------------
// 5.2 Homeostatic Plasticity Integration
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Integration_HomeostaticMaintenance) {
    astrocyte_t* astro = network->astrocytes[0];

    // Simulate sustained high activity with strong stimulus
    for (int i = 0; i < 100; i++) {
        astrocyte_update_calcium(astro, 0.01f, 50.0f);
    }

    float scaling = astrocyte_compute_synaptic_scaling(astro, neural_net);

    EXPECT_LT(scaling, 1.0f) << "Should scale down high activity";
}

//-----------------------------------------------------------------------------
// 5.3 ATP-Activity Coupling
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Integration_ATPDepletion) {
    astrocyte_t* astro = network->astrocytes[0];

    astro->atp_level = 1.0f;

    // Sustained high activity
    for (int i = 0; i < 1000; i++) {
        astrocyte_update_atp_level(astro, 10.0f, 0.01f);
    }

    EXPECT_LT(astro->atp_level, 0.5f) << "High activity should deplete ATP";
}

//-----------------------------------------------------------------------------
// 5.4 Complete System Integration
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Integration_CompleteSystem) {
    // Stimulate source
    astrocyte_calcium_system_stimulate(calcium_system, 0, 10.0f);

    // Run complete system
    for (int i = 0; i < 1000; i++) {
        float dt = 0.001f;
        astrocyte_calcium_system_update(calcium_system, dt, nullptr);
        astrocyte_network_step(network, dt);
    }

    // Verify system is stable
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        EXPECT_TRUE(is_calcium_valid(astro->calcium_concentration));
        EXPECT_TRUE(is_ip3_valid(astro->ip3_concentration));
        EXPECT_GE(astro->atp_level, 0.0f);
        EXPECT_LE(astro->atp_level, 1.0f);
    }
}

//=============================================================================
// 6. REGRESSION TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// 6.1 Backward Compatibility
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Regression_SimpleCalciumStillWorks) {
    astrocyte_t* astro = network->astrocytes[0];

    // Old-style direct calcium update
    astrocyte_update_calcium(astro, 0.001f, 5.0f);

    EXPECT_TRUE(is_calcium_valid(astro->calcium_concentration));
}

//-----------------------------------------------------------------------------
// 6.2 Parameter Bounds
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Regression_BiologicalBounds) {
    // Test all astrocytes
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];

        EXPECT_GE(astro->calcium_baseline, 0.0f);
        EXPECT_LE(astro->calcium_baseline, 1.0f);
        EXPECT_GE(astro->target_activity_level, 0.0f);
        EXPECT_LE(astro->target_activity_level, 1.0f);
        EXPECT_GE(astro->coverage_radius, 0.0f);
        EXPECT_LE(astro->coverage_radius, 200.0f);
    }
}

//-----------------------------------------------------------------------------
// 6.3 Memory Management
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Regression_NoMemoryLeaks) {
    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // Create and destroy many astrocytes
    for (int i = 0; i < 100; i++) {
        astrocyte_t* astro = astrocyte_create(i, ASTROCYTE_TYPE_GENERIC, 0, 0, 0, 50.0f);
        astrocyte_destroy(astro);
    }

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    EXPECT_EQ(stats_before.current_allocated, stats_after.current_allocated);
}

//-----------------------------------------------------------------------------
// 6.4 State Query Functions
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Regression_StateQueriesValid) {
    // All get functions should return valid values
    for (uint32_t i = 0; i < 10; i++) {
        float ca = astrocyte_calcium_system_get_calcium(calcium_system, i);
        float ip3 = astrocyte_calcium_system_get_ip3(calcium_system, i);

        EXPECT_TRUE(is_calcium_valid(ca));
        EXPECT_TRUE(is_ip3_valid(ip3));
    }

    // Invalid IDs should return 0
    float ca = astrocyte_calcium_system_get_calcium(calcium_system, 999);
    EXPECT_FLOAT_EQ(ca, 0.0f);
}

//-----------------------------------------------------------------------------
// 6.5 Null Safety
//-----------------------------------------------------------------------------

TEST_F(AstrocyteComprehensiveTest, Regression_NullSafety) {
    // All functions should handle NULL gracefully
    astrocyte_destroy(nullptr);
    astrocyte_network_destroy(nullptr);
    astrocyte_calcium_system_destroy(nullptr);

    astrocyte_update_calcium(nullptr, 0.001f, 0.0f);
    astrocyte_propagate_calcium_wave(nullptr, network, 0.001f);
    astrocyte_propagate_calcium_wave(network->astrocytes[0], nullptr, 0.001f);

    astrocyte_network_step(nullptr, 0.001f);

    float ca = astrocyte_calcium_system_get_calcium(nullptr, 0);
    EXPECT_FLOAT_EQ(ca, 0.0f);

    // Should not have crashed
    SUCCEED();
}

//=============================================================================
// 7. EDGE CASES AND STRESS TESTS
//=============================================================================

TEST_F(AstrocyteComprehensiveTest, EdgeCase_EmptyNetwork) {
    astrocyte_network_t* empty_net = astrocyte_network_create(10);

    astrocyte_network_step(empty_net, 0.001f);

    float avg_ca, max_ca, avg_glu;
    astrocyte_network_get_stats(empty_net, &avg_ca, &max_ca, &avg_glu);

    // Should handle empty network gracefully
    astrocyte_network_destroy(empty_net);
}

TEST_F(AstrocyteComprehensiveTest, EdgeCase_SingleAstrocyte) {
    astrocyte_network_t* net = astrocyte_network_create(10);
    astrocyte_t* astro = astrocyte_create(0, ASTROCYTE_TYPE_GENERIC, 0, 0, 0, 50.0f);
    astrocyte_network_add(net, astro);
    astrocyte_network_establish_coupling(net);

    astrocyte_calcium_system_t* sys = astrocyte_calcium_system_create(net);
    ASSERT_NE(sys, nullptr);

    astrocyte_calcium_system_update(sys, 0.001f, nullptr);

    astrocyte_calcium_system_destroy(sys);
    astrocyte_network_destroy(net);
}

TEST_F(AstrocyteComprehensiveTest, StressTest_ManyUpdates) {
    // Run for extended time
    for (int i = 0; i < 10000; i++) {
        astrocyte_calcium_system_update(calcium_system, 0.001f, nullptr);
    }

    // System should remain stable
    for (uint32_t i = 0; i < 10; i++) {
        float ca = astrocyte_calcium_system_get_calcium(calcium_system, i);
        EXPECT_TRUE(is_calcium_valid(ca));
    }
}

TEST_F(AstrocyteComprehensiveTest, StressTest_ContinuousStimulation) {
    float stimulus[10];
    for (int i = 0; i < 10; i++) {
        stimulus[i] = 5.0f;
    }

    // Continuous stimulation for 10 seconds
    for (int i = 0; i < 10000; i++) {
        astrocyte_calcium_system_update(calcium_system, 0.001f, stimulus);
    }

    // System should saturate but remain stable
    for (uint32_t i = 0; i < 10; i++) {
        float ca = astrocyte_calcium_system_get_calcium(calcium_system, i);
        EXPECT_TRUE(is_calcium_valid(ca));
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
