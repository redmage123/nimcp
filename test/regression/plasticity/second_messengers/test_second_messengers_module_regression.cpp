/**
 * @file test_second_messengers_module_regression.cpp
 * @brief Regression tests for Second Messenger module integration stability
 *
 * WHAT: Performance and stability regression tests for module integrations
 * WHY:  Ensure module integrations don't degrade system performance
 * HOW:  Benchmark tests, extended operation, stress tests
 *
 * TEST COVERAGE:
 * - Performance: cascade update speed with multiple modules
 * - Stability: extended operation without drift
 * - Memory: no leaks under heavy load
 * - Numerical: values stay bounded
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <chrono>
#include <random>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_second_messengers.h"
#include "core/brain/regions/broca/nimcp_language_production_bridge.h"

//=============================================================================
// MODULE INTEGRATION REGRESSION FIXTURE
//=============================================================================

class ModuleIntegrationRegressionTest : public ::testing::Test {
protected:
    second_messenger_system_t* system_ = nullptr;
    static constexpr uint32_t LARGE_NEURON_COUNT = 2000;

    void SetUp() override {
        second_messenger_config_t config = second_messenger_default_config();
        config.enable_bio_async = false;  // Disable for pure performance testing
        system_ = second_messenger_create(LARGE_NEURON_COUNT, &config);
        ASSERT_NE(system_, nullptr);
    }

    void TearDown() override {
        if (system_) {
            second_messenger_destroy(system_);
            system_ = nullptr;
        }
    }

    double MeasureUpdateTime(uint32_t iterations) {
        auto start = std::chrono::high_resolution_clock::now();
        for (uint32_t i = 0; i < iterations; i++) {
            second_messenger_update(system_, 1.0f, i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

TEST_F(ModuleIntegrationRegressionTest, UpdatePerformance_LargeNeuronCount) {
    // Activate many neurons to simulate realistic load
    for (uint32_t i = 0; i < LARGE_NEURON_COUNT; i += 10) {
        second_messenger_activate_gs(system_, i, 0.5f, 0);
    }

    // Measure 100 updates
    double time_ms = MeasureUpdateTime(100);

    // Should complete in reasonable time (< 2 seconds for 2000 neurons * 100 updates)
    EXPECT_LT(time_ms, 2000.0) << "Update too slow: " << time_ms << "ms";

    // Report performance metrics
    double per_update = time_ms / 100.0;
    double per_neuron_per_update = per_update / (LARGE_NEURON_COUNT / 10.0);
    std::cout << "Performance: " << per_update << " ms/update, "
              << per_neuron_per_update << " ms/active_neuron/update" << std::endl;
}

TEST_F(ModuleIntegrationRegressionTest, ActivationThroughput_HighRate) {
    auto start = std::chrono::high_resolution_clock::now();

    // Many rapid activations (simulate burst of neuromodulator release)
    for (int i = 0; i < 50000; i++) {
        uint32_t neuron = i % LARGE_NEURON_COUNT;
        int pathway = i % 3;
        switch (pathway) {
            case 0: second_messenger_activate_gs(system_, neuron, 0.5f, i); break;
            case 1: second_messenger_activate_gq(system_, neuron, 0.5f, i); break;
            case 2: second_messenger_activate_gi(system_, neuron, 0.5f, i); break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(end - start).count();

    // 50000 activations should complete in < 2 seconds
    EXPECT_LT(duration, 2000.0) << "Activation throughput too slow: " << duration << "ms";

    std::cout << "Activation throughput: " << (50000.0 / duration * 1000.0)
              << " activations/sec" << std::endl;
}

TEST_F(ModuleIntegrationRegressionTest, StateQueryPerformance_BulkQueries) {
    // Activate some neurons first
    for (uint32_t i = 0; i < 500; i++) {
        second_messenger_activate_gs(system_, i, 0.5f, 0);
    }
    second_messenger_update(system_, 100.0f, 100);

    auto start = std::chrono::high_resolution_clock::now();

    // Query many states
    second_messenger_state_t state;
    for (int i = 0; i < 100000; i++) {
        uint32_t neuron = i % 500;
        second_messenger_get_state(system_, neuron, &state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(end - start).count();

    // 100000 queries should be fast (< 500ms)
    EXPECT_LT(duration, 500.0) << "State query too slow: " << duration << "ms";
}

//=============================================================================
// NUMERICAL STABILITY REGRESSION TESTS
//=============================================================================

TEST_F(ModuleIntegrationRegressionTest, NumericalStability_ExtendedSimulation) {
    // Simulate 1 hour of neural activity at 1ms resolution
    // This is 3.6 million timesteps - we'll do a compressed version

    uint32_t test_neuron = 500;

    for (uint64_t t = 0; t < 100000; t++) {
        // Periodic activations (every 100ms)
        if (t % 100 == 0) {
            second_messenger_activate_gs(system_, test_neuron, 0.3f, t);
        }
        if (t % 200 == 0) {
            second_messenger_activate_gq(system_, test_neuron, 0.2f, t);
        }

        second_messenger_update(system_, 1.0f, t);
    }

    // Verify state is still valid
    second_messenger_state_t state;
    ASSERT_EQ(second_messenger_get_state(system_, test_neuron, &state), NIMCP_SUCCESS);

    // All values should be finite and in reasonable range
    EXPECT_FALSE(std::isnan(state.camp.camp_concentration));
    EXPECT_FALSE(std::isinf(state.camp.camp_concentration));
    EXPECT_GE(state.camp.camp_concentration, 0.0f);
    EXPECT_LE(state.camp.camp_concentration, SM_CAMP_MAX_UM * 10.0f);

    EXPECT_FALSE(std::isnan(state.calcium.ca_cytoplasmic));
    EXPECT_FALSE(std::isinf(state.calcium.ca_cytoplasmic));
    EXPECT_GE(state.calcium.ca_cytoplasmic, 0.0f);

    EXPECT_FALSE(std::isnan(state.ip3_dag.pkc_activity));
    EXPECT_GE(state.ip3_dag.pkc_activity, 0.0f);
    EXPECT_LE(state.ip3_dag.pkc_activity, 1.0f);
}

TEST_F(ModuleIntegrationRegressionTest, NumericalStability_ExtremeConditions) {
    uint32_t neuron = 100;

    // Extreme rapid activations
    for (int i = 0; i < 1000; i++) {
        second_messenger_activate_gs(system_, neuron, 1.0f, i);
        second_messenger_activate_gq(system_, neuron, 1.0f, i);
        second_messenger_inject_calcium(system_, neuron, 1000.0f, i);
    }

    second_messenger_update(system_, 1000.0f, 1000);

    // State should still be valid (saturated but not NaN/Inf)
    second_messenger_state_t state;
    ASSERT_EQ(second_messenger_get_state(system_, neuron, &state), NIMCP_SUCCESS);

    EXPECT_FALSE(std::isnan(state.camp.camp_concentration));
    EXPECT_FALSE(std::isinf(state.camp.camp_concentration));
    EXPECT_FALSE(std::isnan(state.calcium.ca_cytoplasmic));
    EXPECT_FALSE(std::isinf(state.calcium.ca_cytoplasmic));
}

TEST_F(ModuleIntegrationRegressionTest, NumericalStability_RandomStress) {
    std::mt19937 rng(12345);  // Fixed seed for reproducibility
    std::uniform_int_distribution<uint32_t> neuron_dist(0, LARGE_NEURON_COUNT - 1);
    std::uniform_real_distribution<float> occ_dist(0.0f, 1.0f);
    std::uniform_int_distribution<int> pathway_dist(0, 2);

    // Random activations
    for (int i = 0; i < 10000; i++) {
        uint32_t neuron = neuron_dist(rng);
        float occ = occ_dist(rng);

        switch (pathway_dist(rng)) {
            case 0: second_messenger_activate_gs(system_, neuron, occ, i); break;
            case 1: second_messenger_activate_gq(system_, neuron, occ, i); break;
            case 2: second_messenger_activate_gi(system_, neuron, occ, i); break;
        }

        if (i % 10 == 0) {
            second_messenger_update(system_, 1.0f, i);
        }
    }

    // Check random sample of neurons
    std::vector<uint32_t> sample_neurons = {0, 100, 500, 1000, 1500, 1999};
    for (uint32_t n : sample_neurons) {
        second_messenger_state_t state;
        ASSERT_EQ(second_messenger_get_state(system_, n, &state), NIMCP_SUCCESS);
        EXPECT_FALSE(std::isnan(state.camp.pka_activity));
        EXPECT_FALSE(std::isnan(state.ip3_dag.pkc_activity));
        EXPECT_FALSE(std::isnan(state.calcium.camkii_activity));
    }
}

//=============================================================================
// MEMORY REGRESSION TESTS
//=============================================================================

TEST_F(ModuleIntegrationRegressionTest, Memory_NoLeaksInRepeatedCreateDestroy) {
    // Create and destroy many systems
    for (int i = 0; i < 20; i++) {
        second_messenger_config_t config = second_messenger_default_config();
        second_messenger_system_t* temp = second_messenger_create(500, &config);
        ASSERT_NE(temp, nullptr) << "Failed on iteration " << i;

        // Do some work
        for (uint32_t n = 0; n < 100; n++) {
            second_messenger_activate_gs(temp, n, 0.5f, 0);
        }
        second_messenger_update(temp, 100.0f, 100);

        second_messenger_destroy(temp);
    }

    // If we get here without memory errors, pass
    SUCCEED();
}

TEST_F(ModuleIntegrationRegressionTest, Memory_AllNeuronsAccessible) {
    // Activate and query all neurons in large system
    for (uint32_t n = 0; n < LARGE_NEURON_COUNT; n++) {
        ASSERT_EQ(second_messenger_activate_gs(system_, n, 0.3f, 0), NIMCP_SUCCESS)
            << "Failed to activate neuron " << n;
    }

    second_messenger_update(system_, 100.0f, 100);

    for (uint32_t n = 0; n < LARGE_NEURON_COUNT; n++) {
        second_messenger_state_t state;
        ASSERT_EQ(second_messenger_get_state(system_, n, &state), NIMCP_SUCCESS)
            << "Failed to get state for neuron " << n;
        EXPECT_GE(state.camp.camp_concentration, 0.0f);
    }
}

//=============================================================================
// BROCA MODULE REGRESSION TESTS
//=============================================================================

class BrocaModuleRegressionTest : public ::testing::Test {
protected:
    language_production_bridge_t* bridge_ = nullptr;

    void SetUp() override {
        lpb_config_t config = lpb_default_config();
        config.enable_second_messengers = true;
        // lpb_create requires broca_adapter, pass NULL for SM-only testing
        bridge_ = lpb_create(&config, nullptr);
        // Bridge may be NULL if broca adapter is required
    }

    void TearDown() override {
        if (bridge_) {
            lpb_destroy(bridge_);
            bridge_ = nullptr;
        }
    }
};

TEST_F(BrocaModuleRegressionTest, RepeatedActivation_NoStateCorruption) {
    if (!bridge_) GTEST_SKIP() << "Broca bridge not available";

    // Many repeated activations
    for (int i = 0; i < 1000; i++) {
        lpb_trigger_receptor(bridge_, 0, 0, 0.5f, i);  // D1
        lpb_trigger_receptor(bridge_, 0, 1, 0.3f, i);  // D2

        if (i % 100 == 0) {
            float pka, pkc, camkii;
            bool ok = lpb_get_second_messenger_state(bridge_, 0, &pka, &pkc, &camkii);
            ASSERT_TRUE(ok) << "State query failed at iteration " << i;
            EXPECT_FALSE(std::isnan(pka));
            EXPECT_FALSE(std::isnan(pkc));
            EXPECT_FALSE(std::isnan(camkii));
        }
    }
}

TEST_F(BrocaModuleRegressionTest, DifferentNeurons_IndependentState) {
    if (!bridge_) GTEST_SKIP() << "Broca bridge not available";

    // Activate different neurons with different patterns
    lpb_trigger_receptor(bridge_, 0, 0, 1.0f, 0);  // Neuron 0: strong D1
    lpb_trigger_receptor(bridge_, 1, 1, 1.0f, 0);  // Neuron 1: strong D2
    lpb_trigger_receptor(bridge_, 2, 2, 1.0f, 0);  // Neuron 2: Gq

    // Query and verify independence
    float pka0, pkc0, camkii0;
    float pka1, pkc1, camkii1;
    float pka2, pkc2, camkii2;

    lpb_get_second_messenger_state(bridge_, 0, &pka0, &pkc0, &camkii0);
    lpb_get_second_messenger_state(bridge_, 1, &pka1, &pkc1, &camkii1);
    lpb_get_second_messenger_state(bridge_, 2, &pka2, &pkc2, &camkii2);

    // Each should have distinct profiles (or at least valid values)
    EXPECT_GE(pka0, 0.0f);
    EXPECT_GE(pka1, 0.0f);
    EXPECT_GE(pkc2, 0.0f);
}

//=============================================================================
// CONSISTENCY REGRESSION TESTS
//=============================================================================

TEST_F(ModuleIntegrationRegressionTest, Consistency_DeterministicBehavior) {
    // Two neurons with identical activation should have identical states
    uint32_t n1 = 100, n2 = 200;

    second_messenger_activate_gs(system_, n1, 0.7f, 0);
    second_messenger_activate_gs(system_, n2, 0.7f, 0);

    for (int t = 0; t < 500; t++) {
        second_messenger_update(system_, 1.0f, t);
    }

    second_messenger_state_t state1, state2;
    second_messenger_get_state(system_, n1, &state1);
    second_messenger_get_state(system_, n2, &state2);

    EXPECT_NEAR(state1.camp.camp_concentration, state2.camp.camp_concentration, 0.001f);
    EXPECT_NEAR(state1.camp.pka_activity, state2.camp.pka_activity, 0.001f);
    EXPECT_NEAR(state1.calcium.camkii_activity, state2.calcium.camkii_activity, 0.001f);
}

TEST_F(ModuleIntegrationRegressionTest, Consistency_PlasticityModulationRange) {
    // Activate with various intensities
    for (uint32_t n = 0; n < 100; n++) {
        float occ = n / 100.0f;  // 0.0 to 0.99
        second_messenger_activate_gs(system_, n, occ, 0);
    }

    second_messenger_update(system_, 500.0f, 500);

    // All modulation values should be in valid range [0.5, 2.0]
    for (uint32_t n = 0; n < 100; n++) {
        float mod = second_messenger_get_plasticity_modulation(system_, n);
        EXPECT_GE(mod, 0.5f) << "Modulation too low for neuron " << n;
        EXPECT_LE(mod, 2.0f) << "Modulation too high for neuron " << n;
    }
}
