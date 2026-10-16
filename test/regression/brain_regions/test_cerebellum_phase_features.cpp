/**
 * @file test_cerebellum_phase_features.cpp
 * @brief Regression tests for cerebellum phase features
 *
 * Tests backward compatibility for:
 * - Phase 1: Synaptic dynamics (vesicle pools, STP, calcium)
 * - Phase 2: Inhibitory interneurons (basket, stellate, Golgi)
 * - Phase 3: Vestibulocerebellum integration
 *
 * @version Phase 1-3 Regression
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>

#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"

//=============================================================================
// Phase 1: Synaptic Dynamics Regression Tests
//=============================================================================

class SynapticDynamicsRegressionTest : public ::testing::Test {
protected:
    static constexpr int BENCHMARK_ITERATIONS = 100;

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
};

TEST_F(SynapticDynamicsRegressionTest, APIStability_DefaultConfigUnchanged) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();

    // Core parameters should remain stable
    EXPECT_EQ(config.rrp_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY);
    EXPECT_EQ(config.recycling_capacity, CEREBELLUM_SYNAPSE_DEFAULT_RECYCLING_CAP);
    EXPECT_FLOAT_EQ(config.release_probability, CEREBELLUM_SYNAPSE_DEFAULT_RELEASE_PROB);
    EXPECT_FLOAT_EQ(config.refill_rate, CEREBELLUM_SYNAPSE_DEFAULT_REFILL_RATE);

    // STP parameters
    EXPECT_FLOAT_EQ(config.stp_U, CEREBELLUM_SYNAPSE_DEFAULT_STP_U);
    EXPECT_FLOAT_EQ(config.stp_tau_D, CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_D);
    EXPECT_FLOAT_EQ(config.stp_tau_F, CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_F);

    // Calcium parameters
    EXPECT_FLOAT_EQ(config.ca_baseline, CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE);
    EXPECT_FLOAT_EQ(config.ca_decay_tau, CEREBELLUM_SYNAPSE_DEFAULT_CA_DECAY_TAU);
    EXPECT_FLOAT_EQ(config.ca_influx_per_spike, CEREBELLUM_SYNAPSE_DEFAULT_CA_INFLUX);
}

TEST_F(SynapticDynamicsRegressionTest, APIStability_StateInitUnchanged) {
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    config.enable_vesicle_dynamics = true;
    config.enable_stp = true;
    config.enable_calcium_dynamics = true;

    cerebellar_synapse_init(&state, &config);

    // State should be initialized correctly
    EXPECT_EQ(state.rrp_available, config.rrp_capacity);
    EXPECT_EQ(state.rrp_capacity, config.rrp_capacity);
    EXPECT_FLOAT_EQ(state.vesicle_depletion, 0.0f);
    EXPECT_FLOAT_EQ(state.stp_x, 1.0f);
    EXPECT_FLOAT_EQ(state.stp_u, config.stp_U);
    EXPECT_FLOAT_EQ(state.calcium_concentration, config.ca_baseline);
}

TEST_F(SynapticDynamicsRegressionTest, Behavior_VesicleDepletionPattern) {
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    config.enable_vesicle_dynamics = true;
    cerebellar_synapse_init(&state, &config);

    uint32_t initial = state.rrp_available;

    // High activity should deplete vesicles
    for (int i = 0; i < 50; i++) {
        cerebellar_synapse_update(&state, 1.0f, 1.0f);
    }

    EXPECT_LT(state.rrp_available, initial);

    // Rest should recover
    uint32_t depleted = state.rrp_available;
    for (int i = 0; i < 500; i++) {
        cerebellar_synapse_update(&state, 1.0f, 0.0f);
    }

    EXPECT_GT(state.rrp_available, depleted);
}

TEST_F(SynapticDynamicsRegressionTest, Behavior_STDPattern) {
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    config.enable_stp = true;
    cerebellar_synapse_init(&state, &config);

    float initial_x = state.stp_x;

    // Activity causes depression
    for (int i = 0; i < 20; i++) {
        cerebellar_synapse_update(&state, 1.0f, 1.0f);
    }

    EXPECT_LT(state.stp_x, initial_x);
}

TEST_F(SynapticDynamicsRegressionTest, Behavior_CalciumDynamics) {
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    config.enable_calcium_dynamics = true;
    cerebellar_synapse_init(&state, &config);

    float baseline = state.calcium_concentration;

    // Activity increases calcium
    cerebellar_synapse_update(&state, 1.0f, 1.0f);
    EXPECT_GT(state.calcium_concentration, baseline);

    float peak = state.calcium_concentration;

    // Rest decays calcium
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 1.0f, 0.0f);
    }

    EXPECT_LT(state.calcium_concentration, peak);
}

TEST_F(SynapticDynamicsRegressionTest, Performance_UpdateUnder1us) {
    cerebellar_synapse_state_t state;
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    config.enable_vesicle_dynamics = true;
    config.enable_stp = true;
    config.enable_calcium_dynamics = true;
    cerebellar_synapse_init(&state, &config);

    // Warmup
    for (int i = 0; i < 100; i++) {
        cerebellar_synapse_update(&state, 1.0f, 0.5f);
    }

    std::vector<long long> times;
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            cerebellar_synapse_update(&state, 1.0f, 0.5f);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Synapse Update: avg=" << (avg_ns / 1000.0) << " us\n";
    EXPECT_LT(avg_ns, 1000.0) << "Synapse update should be < 1 us";
}

//=============================================================================
// Phase 2: Interneuron Regression Tests
//=============================================================================

class InterneuronRegressionTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_basket_cells = true;
        config.enable_stellate_cells = true;
        config.enable_golgi_cells = true;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(InterneuronRegressionTest, APIStability_ConfigFieldsExist) {
    cerebellum_config_t config = cerebellum_default_config();

    // Basket cell config fields
    EXPECT_FALSE(config.enable_basket_cells);  // Default false for backward compat
    EXPECT_GT(config.num_basket_cells, 0);
    EXPECT_GT(config.purkinje_per_basket, 0);

    // Stellate cell config fields
    EXPECT_FALSE(config.enable_stellate_cells);
    EXPECT_GT(config.num_stellate_cells, 0);
    EXPECT_GT(config.purkinje_per_stellate, 0);

    // Golgi cell config fields
    EXPECT_FALSE(config.enable_golgi_cells);
    EXPECT_GT(config.num_golgi_cells, 0);
    EXPECT_GT(config.granules_per_golgi, 0);
}

TEST_F(InterneuronRegressionTest, Behavior_BasketCellInhibition) {
    // Process input
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.basket_cell_spikes, 0);
    EXPECT_GT(stats.basket_inhibition_total, 0.0f);
}

TEST_F(InterneuronRegressionTest, Behavior_StellateCellInhibition) {
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.stellate_cell_spikes, 0);
    EXPECT_GT(stats.stellate_inhibition_total, 0.0f);
}

TEST_F(InterneuronRegressionTest, Behavior_GolgiCellFeedback) {
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.golgi_cell_spikes, 0);
    EXPECT_GT(stats.golgi_feedback_total, 0.0f);
}

TEST_F(InterneuronRegressionTest, Behavior_FiringRatesInBiologicalRange) {
    for (int i = 0; i < 100; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.7f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // Fast-spiking interneurons: up to ~200-300 Hz
    EXPECT_LT(stats.avg_basket_firing_rate, 300.0f);
    EXPECT_LT(stats.avg_stellate_firing_rate, 300.0f);
    EXPECT_LT(stats.avg_golgi_firing_rate, 300.0f);
}

//=============================================================================
// Phase 3: Vestibulocerebellum Regression Tests
//=============================================================================

class VestibulocerebellumRegressionTest : public ::testing::Test {
protected:
    vestibular_processor_t* vestibular = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_cerebellum_bridge_t* bridge = nullptr;

    void SetUp() override {
        vestibular_config_t vest_config = vestibular_default_config();
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        cerebellum_config_t cereb_config = cerebellum_default_config();
        cereb_config.enable_vestibulocerebellum = true;
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        bridge = vestibular_cerebellum_bridge_create(vestibular, cerebellum, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            vestibular_cerebellum_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (vestibular) {
            vestibular_destroy(vestibular);
            vestibular = nullptr;
        }
    }
};

TEST_F(VestibulocerebellumRegressionTest, APIStability_BridgeConfigUnchanged) {
    vestibular_cerebellum_config_t config = vestibular_cerebellum_default_config();

    EXPECT_GT(config.num_mossy_fibers, 0);
    EXPECT_GT(config.mossy_fiber_gain, 0.0f);
    EXPECT_TRUE(config.enable_vor_adaptation);
    EXPECT_FLOAT_EQ(config.vor_ltd_rate, VOR_DEFAULT_LTD_RATE);
    EXPECT_FLOAT_EQ(config.retinal_slip_threshold, VOR_RETINAL_SLIP_THRESHOLD);
    EXPECT_TRUE(config.route_to_flocculus);
    EXPECT_TRUE(config.route_to_nodulus);
    EXPECT_TRUE(config.enable_feedback_loop);
}

TEST_F(VestibulocerebellumRegressionTest, APIStability_ConstantsUnchanged) {
    EXPECT_FLOAT_EQ(VOR_DEFAULT_LTD_RATE, 0.01f);
    EXPECT_FLOAT_EQ(VOR_MAX_RETINAL_SLIP, 2.0f);
    EXPECT_FLOAT_EQ(VOR_RETINAL_SLIP_THRESHOLD, 0.02f);
}

TEST_F(VestibulocerebellumRegressionTest, Behavior_VorAdaptationWorks) {
    float initial_gain[3];
    bool adaptation_active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &adaptation_active);

    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 50; i++) {
        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.3f, slip_direction);
    }

    float final_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, final_gain, &adaptation_active);

    EXPECT_NE(final_gain[0], initial_gain[0]);
}

TEST_F(VestibulocerebellumRegressionTest, Behavior_MossySignalTransmission) {
    vestibular_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = VESTIBULAR_INPUT_CANAL;
    input.canal.angular_velocity[0] = 1.0f;
    input.timestamp_ms = 0.0f;

    vestibular_process(vestibular, &input);
    EXPECT_EQ(0, vestibular_cerebellum_send_mossy_signal(bridge));

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.mossy_signals_sent, 1);
}

TEST_F(VestibulocerebellumRegressionTest, Behavior_FeedbackLoopWorks) {
    vestibular_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = VESTIBULAR_INPUT_CANAL;
    input.canal.angular_velocity[0] = 1.0f;
    input.timestamp_ms = 0.0f;

    vestibular_process(vestibular, &input);
    vestibular_cerebellum_send_mossy_signal(bridge);

    motor_coordination_result_t motor_result;
    cerebellum_process(cerebellum, &motor_result);

    EXPECT_EQ(0, vestibular_cerebellum_apply_feedback(bridge));

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.feedback_events, 1);
}

TEST_F(VestibulocerebellumRegressionTest, ErrorHandling_NullParameters) {
    EXPECT_EQ(-1, vestibular_cerebellum_send_mossy_signal(nullptr));
    EXPECT_EQ(-1, vestibular_cerebellum_send_custom_signal(bridge, nullptr));
    EXPECT_EQ(-1, vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.5f, nullptr));
    EXPECT_EQ(-1, vestibular_cerebellum_apply_feedback(nullptr));
    EXPECT_EQ(-1, vestibular_cerebellum_get_stats(nullptr, nullptr));
}

//=============================================================================
// Cross-Phase Integration Regression Tests
//=============================================================================

class CrossPhaseRegressionTest : public ::testing::Test {
protected:
    vestibular_processor_t* vestibular = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_cerebellum_bridge_t* bridge = nullptr;

    void SetUp() override {
        vestibular_config_t vest_config = vestibular_default_config();
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        // Enable all phase features
        cerebellum_config_t cereb_config = cerebellum_default_config();
        cereb_config.enable_synaptic_dynamics = true;
        cereb_config.synapse_config = cerebellar_synapse_default_config();
        cereb_config.synapse_config.enable_vesicle_dynamics = true;
        cereb_config.synapse_config.enable_stp = true;
        cereb_config.synapse_config.enable_calcium_dynamics = true;
        cereb_config.enable_basket_cells = true;
        cereb_config.enable_stellate_cells = true;
        cereb_config.enable_golgi_cells = true;
        cereb_config.enable_vestibulocerebellum = true;
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        bridge = vestibular_cerebellum_bridge_create(vestibular, cerebellum, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            vestibular_cerebellum_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (vestibular) {
            vestibular_destroy(vestibular);
            vestibular = nullptr;
        }
    }
};

TEST_F(CrossPhaseRegressionTest, AllPhasesWorkTogether) {
    // Vestibular input
    vestibular_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = VESTIBULAR_INPUT_CANAL;
    input.canal.angular_velocity[0] = 1.5f;
    input.timestamp_ms = 0.0f;

    for (int i = 0; i < 100; i++) {
        input.timestamp_ms = (float)i;
        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);

        if (i % 10 == 0) {
            float slip_direction[3] = {1.0f, 0.0f, 0.0f};
            vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.2f, slip_direction);
        }

        vestibular_cerebellum_apply_feedback(bridge);
    }

    // Verify all phases contributed
    cerebellum_stats_t cereb_stats;
    cerebellum_get_stats(cerebellum, &cereb_stats);

    // Phase 1: Synaptic dynamics
    EXPECT_GT(cereb_stats.vesicle_releases, 0);

    // Phase 2: Interneurons
    EXPECT_GT(cereb_stats.basket_cell_spikes, 0);
    EXPECT_GT(cereb_stats.stellate_cell_spikes, 0);
    EXPECT_GT(cereb_stats.golgi_cell_spikes, 0);

    // Phase 3: Vestibular integration
    vestibular_cerebellum_stats_t bridge_stats;
    vestibular_cerebellum_get_stats(bridge, &bridge_stats);
    EXPECT_EQ(bridge_stats.mossy_signals_sent, 100);
}

TEST_F(CrossPhaseRegressionTest, BackwardCompatibility_DisabledFeaturesWork) {
    // Create cerebellum with all new features disabled
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_synaptic_dynamics = false;
    config.enable_basket_cells = false;
    config.enable_stellate_cells = false;
    config.enable_golgi_cells = false;
    config.enable_vestibulocerebellum = false;

    cerebellum_adapter_t* legacy_adapter = cerebellum_create(&config);
    ASSERT_NE(legacy_adapter, nullptr);

    // Should still work for basic processing
    mossy_fiber_input_t mossy_input;
    mossy_input.fiber_id = 0;
    mossy_input.activity = 0.8f;
    mossy_input.timestamp_ms = 0.0f;
    mossy_input.modality = 0;

    EXPECT_TRUE(cerebellum_process_mossy_input(legacy_adapter, &mossy_input));

    motor_coordination_result_t result;
    EXPECT_TRUE(cerebellum_process(legacy_adapter, &result));

    cerebellum_destroy(legacy_adapter);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
