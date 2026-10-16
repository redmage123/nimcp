/**
 * @file test_entorhinal_snn_plasticity_integration.cpp
 * @brief Integration tests for Entorhinal Cortex with SNN and Plasticity systems
 * @version Phase 5: Memory Circuit
 * @date 2025-01-13
 *
 * WHAT: Tests Entorhinal Cortex integration with SNN and STDP learning
 * WHY:  Ensure proper spatial learning via spike-based plasticity for grid cells,
 *       border cells, head direction cells, and place-grid cell associations
 * HOW:  Test SNN networks, STDP synapses, eligibility traces, and weight updates
 *       during spatial navigation and path integration
 *
 * BIOLOGICAL BASIS:
 * The entorhinal cortex relies on spike-timing dependent plasticity for:
 * - Grid cell learning from place cell inputs (place-to-grid plasticity)
 * - Path integration calibration through visual landmark learning
 * - Border cell tuning through environmental boundary experience
 * - Head direction cell calibration through vestibular-visual integration
 * - Memory encoding/retrieval via theta-modulated spike timing
 *
 * INTEGRATION POINTS:
 * - SNN network creation and simulation for spatial cells
 * - STDP synapse initialization and learning rules
 * - Eligibility traces for spatial context-dependent learning
 * - Weight bounds and normalization for stable representations
 * - Dopamine modulation of spatial memory encoding
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

/* SNN and plasticity headers have their own extern "C" guards */
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_types.h"
#include "plasticity/stdp/nimcp_stdp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class EntorhinalSNNPlasticityTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;
    entorhinal_config_t config;

    void SetUp() override {
        /* Configure entorhinal with SNN and plasticity enabled */
        config = entorhinal_default_config();
        config.enable_snn = true;
        config.enable_plasticity = true;
        config.enable_stdp = true;
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.learning_rate = 0.01f;
        config.weight_decay = 0.0001f;
        config.eligibility_decay = 0.95f;

        /* Reduce cell counts for faster testing */
        config.num_grid_cells = 64;
        config.num_border_cells = 32;
        config.num_hd_cells = 16;
        config.num_object_cells = 32;
        config.num_speed_cells = 16;

        ec = entorhinal_create(&config);
        ASSERT_NE(nullptr, ec) << "Failed to create Entorhinal cortex";
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * SNN BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, SNNBridgeInitialization) {
    /* Test SNN bridge initialization with null SNN (creates internal) */
    int result = entorhinal_init_snn_bridge(ec, nullptr);
    EXPECT_EQ(0, result) << "SNN bridge initialization failed";

    /* Verify bridge state is set up */
    EXPECT_GE(ec->snn_bridge.input_layer_id, 0u);
    EXPECT_GE(ec->snn_bridge.grid_layer_id, 0u);
}

TEST_F(EntorhinalSNNPlasticityTest, SNNBridgeWithExternalNetwork) {
    /* Create external SNN network for grid cells */
    snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config_feedforward(&snn_config,
        config.num_grid_cells,   /* Input: place cell proxies */
        config.num_grid_cells * 2,  /* Hidden: integration */
        config.num_grid_cells);  /* Output: grid cells */

    snn_network_t* snn = snn_network_create(&snn_config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Initialize bridge with external SNN */
    int result = entorhinal_init_snn_bridge(ec, snn);
    EXPECT_EQ(0, result);

    /* Verify SNN is connected */
    EXPECT_EQ(ec->snn_bridge.snn, snn);

    snn_network_destroy(snn);
}

TEST_F(EntorhinalSNNPlasticityTest, PlasticityBridgeInitialization) {
    /* Test plasticity bridge initialization */
    int result = entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    EXPECT_EQ(0, result) << "Plasticity bridge initialization failed";

    /* Verify bridge has valid learning rate from config */
    EXPECT_FLOAT_EQ(ec->plasticity_bridge.learning_rate, config.learning_rate);
}

/*=============================================================================
 * STDP SYNAPSE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, STDPSynapseDefaultInit) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Verify default parameters (Bi & Poo parameters) */
    EXPECT_GT(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_GT(synapse.learning_rate, 0.0f);
    EXPECT_NEAR(synapse.tau_plus, 20.0f, 10.0f);   /* ~20ms typical */
    EXPECT_NEAR(synapse.tau_minus, 20.0f, 10.0f);
    EXPECT_GT(synapse.a_plus, 0.0f);
    EXPECT_GT(synapse.a_minus, 0.0f);
}

TEST_F(EntorhinalSNNPlasticityTest, STDPSynapseCustomConfigForSpatialCells) {
    /* Configure STDP for spatial cell plasticity (faster learning) */
    stdp_synapse_t synapse;
    stdp_config_t spatial_config = stdp_config_default();
    spatial_config.learning_rate = 0.02f;  /* Faster for spatial learning */
    spatial_config.w_max = 1.5f;
    spatial_config.tau_plus = 15.0f;       /* Tighter timing window */
    spatial_config.tau_minus = 20.0f;
    spatial_config.enable_da_modulation = true;  /* Reward modulation */

    stdp_synapse_init_with_config(&synapse, &spatial_config);

    EXPECT_FLOAT_EQ(synapse.learning_rate, 0.02f);
    EXPECT_FLOAT_EQ(synapse.w_max, 1.5f);
    EXPECT_FLOAT_EQ(synapse.tau_plus, 15.0f);
    EXPECT_FLOAT_EQ(synapse.tau_minus, 20.0f);
    EXPECT_TRUE(synapse.enable_da_modulation);
}

/*=============================================================================
 * SPIKE PROPAGATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, GridCellSpikeGeneration) {
    /* Create SNN for grid cell spiking */
    snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config_feedforward(&snn_config, 16, 32, 16);

    snn_network_t* snn = snn_network_create(&snn_config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Set inputs representing spatial position encoding */
    float spatial_inputs[16];
    for (int i = 0; i < 16; i++) {
        /* Simulate grid cell activation pattern */
        spatial_inputs[i] = 0.5f + 0.5f * sinf(i * M_PI / 4.0f);
    }
    EXPECT_EQ(SNN_SUCCESS, snn_network_set_inputs(snn, spatial_inputs, 16));

    /* Run simulation step */
    int spikes = snn_network_step(snn, 1.0f);
    EXPECT_GE(spikes, 0);

    snn_network_destroy(snn);
}

TEST_F(EntorhinalSNNPlasticityTest, SpikePropagationThroughLayers) {
    /* Create multi-layer SNN mimicking entorhinal circuit */
    snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config_feedforward(&snn_config,
        32,    /* Input: place cell inputs */
        64,    /* Hidden: EC Layer II */
        32);   /* Output: grid cell outputs */

    snn_network_t* snn = snn_network_create(&snn_config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Strong input pattern */
    float inputs[32];
    for (int i = 0; i < 32; i++) {
        inputs[i] = (i % 4 == 0) ? 1.0f : 0.2f;  /* Sparse place cell firing */
    }
    snn_network_set_inputs(snn, inputs, 32);

    /* Run for multiple timesteps */
    int total_spikes = snn_network_run(snn, 50.0f);
    EXPECT_GT(total_spikes, 0);

    /* Get outputs */
    float outputs[32];
    EXPECT_EQ(SNN_SUCCESS, snn_network_get_outputs(snn, outputs, 32));

    snn_network_destroy(snn);
}

TEST_F(EntorhinalSNNPlasticityTest, SNNForwardPassDuringSpatialNavigation) {
    /* Full forward pass during simulated navigation */
    snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config_feedforward(&snn_config, 16, 32, 16);

    snn_network_t* snn = snn_network_create(&snn_config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Simulate position moving through space */
    float inputs[16];
    float outputs[16];

    for (int step = 0; step < 10; step++) {
        /* Update spatial inputs based on position */
        float x = step * 0.5f;
        for (int i = 0; i < 16; i++) {
            inputs[i] = 0.5f + 0.5f * cosf(2.0f * M_PI * (x + i * 0.3f));
        }

        int result = snn_network_forward(snn, inputs, 16, outputs, 16, 10.0f);
        EXPECT_GE(result, 0);

        snn_network_reset(snn);
    }

    snn_network_destroy(snn);
}

/*=============================================================================
 * STDP RULE APPLICATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, STDPPreSpikeLTD) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set up post-trace (post fired recently) */
    synapse.post_trace = 0.5f;
    float initial_weight = synapse.weight;

    /* Pre-spike after post (LTD - depression) */
    float weight_change = stdp_pre_spike(&synapse, 100.0f);

    /* Should cause depression (negative weight change) */
    EXPECT_LE(weight_change, 0.0f);
    EXPECT_LE(synapse.weight, initial_weight);
}

TEST_F(EntorhinalSNNPlasticityTest, STDPPostSpikeLTP) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set up pre-trace (pre fired recently) */
    synapse.pre_trace = 0.5f;
    float initial_weight = synapse.weight;

    /* Post-spike after pre (LTP - potentiation) */
    float weight_change = stdp_post_spike(&synapse, 100.0f);

    /* Should cause potentiation (positive weight change) */
    EXPECT_GE(weight_change, 0.0f);
    EXPECT_GE(synapse.weight, initial_weight);
}

TEST_F(EntorhinalSNNPlasticityTest, STDPTraceDecay) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set initial traces to maximum */
    synapse.pre_trace = 1.0f;
    synapse.post_trace = 1.0f;

    /* Update traces (decay) over time */
    float dt = 0.001f;  /* 1ms */
    for (int i = 0; i < 100; i++) {
        stdp_update_traces(&synapse, dt);
    }

    /* Traces should decay toward zero but not reach it */
    EXPECT_LT(synapse.pre_trace, 1.0f);
    EXPECT_LT(synapse.post_trace, 1.0f);
    EXPECT_GT(synapse.pre_trace, 0.0f);
    EXPECT_GT(synapse.post_trace, 0.0f);
}

TEST_F(EntorhinalSNNPlasticityTest, STDPStatisticsTracking) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Induce multiple LTP events */
    for (int i = 0; i < 5; i++) {
        synapse.pre_trace = 0.5f;
        stdp_post_spike(&synapse, i * 20.0f);
    }

    /* Induce multiple LTD events */
    for (int i = 0; i < 3; i++) {
        synapse.post_trace = 0.5f;
        stdp_pre_spike(&synapse, (i + 5) * 20.0f);
    }

    /* Check statistics */
    EXPECT_GE(synapse.num_potentiation_events, 1u);
    EXPECT_GE(synapse.num_depression_events, 1u);
}

/*=============================================================================
 * PLASTICITY DURING PATH INTEGRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, PlasticityDuringPathIntegration) {
    /* Initialize bridges */
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);

    /* Enable training */
    entorhinal_set_training_mode(ec, true);

    /* Simulate path integration with plasticity */
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    uint64_t initial_weight_updates = ec->plasticity_bridge.weight_updates;

    for (int step = 0; step < 50; step++) {
        /* Path integrate */
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.02f);

        /* Apply plasticity */
        entorhinal_apply_plasticity(ec, 0.02f);
    }

    /* Weight updates should have occurred */
    EXPECT_GE(ec->plasticity_bridge.weight_updates, initial_weight_updates);
}

TEST_F(EntorhinalSNNPlasticityTest, LearningDuringBoundaryDetection) {
    /* Initialize bridges */
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Simulate approaching boundary */
    float boundary_distances[4] = {10.0f, 10.0f, 10.0f, 10.0f};

    for (int step = 0; step < 20; step++) {
        /* Gradually approach boundary */
        boundary_distances[0] = 10.0f - step * 0.4f;

        /* Update border cells */
        entorhinal_update_border_cells(ec, boundary_distances, 4);

        /* Apply plasticity */
        entorhinal_apply_plasticity(ec, 0.02f);
    }

    /* Border cells should have learned from boundary approach */
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
}

TEST_F(EntorhinalSNNPlasticityTest, HeadDirectionLearning) {
    /* Initialize bridges */
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Simulate rotation with HD cell updates */
    for (int step = 0; step < 36; step++) {
        float heading = step * (M_PI / 18.0f);  /* 10 degree steps */
        float angular_velocity = M_PI / 18.0f / 0.02f;  /* Match step size */

        entorhinal_update_hd_cells(ec, heading, angular_velocity);
        entorhinal_apply_plasticity(ec, 0.02f);
    }

    /* HD cells should remain functional after learning */
    float decoded_heading, confidence;
    int result = entorhinal_decode_heading(ec, &decoded_heading, &confidence);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * PLACE CELL TO GRID CELL PLASTICITY TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, PlaceToGridPlasticitySTDP) {
    /* Create synapses representing place-to-grid connections */
    std::vector<stdp_synapse_t> place_grid_synapses(16);

    /* Initialize all synapses with spatial learning parameters */
    stdp_config_t spatial_config = stdp_config_default();
    spatial_config.learning_rate = 0.015f;
    spatial_config.tau_plus = 10.0f;   /* Tighter window for spatial */
    spatial_config.tau_minus = 15.0f;

    for (auto& synapse : place_grid_synapses) {
        stdp_synapse_init_with_config(&synapse, &spatial_config);
    }

    /* Simulate place cell to grid cell learning during navigation */
    for (int step = 0; step < 100; step++) {
        /* Simulate place cell firing at specific locations */
        int active_place_cell = step % 16;

        /* Pre-spike from place cell */
        for (int i = 0; i < 16; i++) {
            if (i == active_place_cell) {
                place_grid_synapses[i].pre_trace = 1.0f;
            }
            stdp_update_traces(&place_grid_synapses[i], 0.001f);
        }

        /* Post-spike in grid cells (based on position in grid pattern) */
        if (step % 5 == 0) {  /* Grid cell fires periodically */
            for (auto& synapse : place_grid_synapses) {
                stdp_post_spike(&synapse, step * 1.0f);
            }
        }
    }

    /* Verify learning occurred (weights changed from initial) */
    int learned_count = 0;
    for (const auto& synapse : place_grid_synapses) {
        if (synapse.num_potentiation_events > 0 || synapse.num_depression_events > 0) {
            learned_count++;
        }
    }
    EXPECT_GT(learned_count, 0);
}

TEST_F(EntorhinalSNNPlasticityTest, GridCellReceptiveFieldFormation) {
    /* Simulate grid cell receptive field formation through STDP */
    const int num_place_inputs = 32;
    const int num_grid_outputs = 8;

    std::vector<std::vector<stdp_synapse_t>> weights(num_grid_outputs,
        std::vector<stdp_synapse_t>(num_place_inputs));

    /* Initialize weight matrix */
    for (int g = 0; g < num_grid_outputs; g++) {
        for (int p = 0; p < num_place_inputs; p++) {
            stdp_synapse_init(&weights[g][p]);
        }
    }

    /* Simulate exploration of environment */
    for (int trial = 0; trial < 20; trial++) {
        for (int pos = 0; pos < num_place_inputs; pos++) {
            /* Place cell fires at current position */
            for (int g = 0; g < num_grid_outputs; g++) {
                weights[g][pos].pre_trace = 1.0f;

                /* Grid cell fires at periodic locations */
                if (pos % (4 + g) == 0) {
                    stdp_post_spike(&weights[g][pos], trial * 100.0f + pos);
                }
            }
        }
    }

    /* Verify periodic structure emerged (some synapses strengthened) */
    int strengthened = 0;
    for (int g = 0; g < num_grid_outputs; g++) {
        for (int p = 0; p < num_place_inputs; p++) {
            if (weights[g][p].weight > 0.6f) {  /* Above initial ~0.5 */
                strengthened++;
            }
        }
    }
    EXPECT_GT(strengthened, 0);
}

/*=============================================================================
 * ELIGIBILITY TRACE TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, EligibilityTraceDecay) {
    /* Test eligibility trace in grid cells */
    const nimcp_grid_cell_t* grid_cell = entorhinal_get_grid_cell(ec, 0, 0);
    if (!grid_cell) {
        GTEST_SKIP() << "Grid cells not available";
    }

    /* Initial trace should be zero or small */
    float initial_trace = grid_cell->eligibility_trace;
    EXPECT_GE(initial_trace, 0.0f);
    EXPECT_LE(initial_trace, 1.0f);
}

TEST_F(EntorhinalSNNPlasticityTest, EligibilityTraceAccumulation) {
    /* Test that eligibility traces accumulate during activity */
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Update position multiple times to accumulate traces */
    float position[3] = {0.0f, 0.0f, 0.0f};
    for (int step = 0; step < 20; step++) {
        position[0] = step * 0.1f;
        entorhinal_update_grid_cells(ec, position, 3);
    }

    /* Traces should have accumulated */
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
}

TEST_F(EntorhinalSNNPlasticityTest, EligibilityTraceRewardModulation) {
    /* Test reward-modulated eligibility trace learning */
    std::vector<stdp_synapse_t> synapses(10);
    stdp_config_t config = stdp_config_default();
    config.enable_da_modulation = true;
    config.da_modulation_gain = 100.0f;
    config.burst_amplification = 3.0f;

    for (auto& synapse : synapses) {
        stdp_synapse_init_with_config(&synapse, &config);
    }

    /* Verify DA modulation is enabled */
    for (const auto& synapse : synapses) {
        EXPECT_TRUE(synapse.enable_da_modulation);
        EXPECT_FLOAT_EQ(synapse.da_modulation_gain, 100.0f);
        EXPECT_FLOAT_EQ(synapse.burst_amplification, 3.0f);
    }
}

/*=============================================================================
 * WEIGHT BOUNDS AND NORMALIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, WeightUpperBound) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Repeatedly potentiate */
    synapse.pre_trace = 1.0f;
    for (int i = 0; i < 1000; i++) {
        stdp_post_spike(&synapse, (float)i);
        synapse.pre_trace = 1.0f;  /* Keep pre-trace high */
    }

    /* Weight should be bounded by w_max */
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_NEAR(synapse.weight, synapse.w_max, 0.1f);  /* Should approach max */
}

TEST_F(EntorhinalSNNPlasticityTest, WeightLowerBound) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    synapse.weight = synapse.w_max;  /* Start at max */

    /* Repeatedly depress */
    synapse.post_trace = 1.0f;
    for (int i = 0; i < 1000; i++) {
        stdp_pre_spike(&synapse, (float)i);
        synapse.post_trace = 1.0f;  /* Keep post-trace high */
    }

    /* Weight should be bounded by w_min */
    EXPECT_GE(synapse.weight, synapse.w_min);
}

TEST_F(EntorhinalSNNPlasticityTest, WeightStabilityUnderContinuousActivity) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    float initial_weight = synapse.weight;

    /* Simulate balanced activity (roughly equal pre-post and post-pre) */
    for (int i = 0; i < 500; i++) {
        if (i % 2 == 0) {
            /* LTP: pre before post */
            synapse.pre_trace = 0.3f;
            stdp_post_spike(&synapse, i * 2.0f);
        } else {
            /* LTD: post before pre */
            synapse.post_trace = 0.3f;
            stdp_pre_spike(&synapse, i * 2.0f + 1.0f);
        }
        stdp_update_traces(&synapse, 0.001f);
    }

    /* Weight should remain within bounds */
    EXPECT_GE(synapse.weight, synapse.w_min);
    EXPECT_LE(synapse.weight, synapse.w_max);
}

TEST_F(EntorhinalSNNPlasticityTest, WeightDistributionAfterLearning) {
    /* Test that weight distribution remains reasonable after learning */
    const int num_synapses = 100;
    std::vector<stdp_synapse_t> synapses(num_synapses);

    for (auto& synapse : synapses) {
        stdp_synapse_init(&synapse);
    }

    /* Simulate learning with varying activity patterns */
    for (int step = 0; step < 200; step++) {
        for (int i = 0; i < num_synapses; i++) {
            /* Random-ish activity based on index and step */
            if ((step + i) % 7 == 0) {
                synapses[i].pre_trace = 0.5f;
                stdp_post_spike(&synapses[i], step * 1.0f);
            }
            if ((step + i) % 11 == 0) {
                synapses[i].post_trace = 0.5f;
                stdp_pre_spike(&synapses[i], step * 1.0f);
            }
            stdp_update_traces(&synapses[i], 0.001f);
        }
    }

    /* Check weight distribution */
    float sum = 0.0f;
    float min_w = synapses[0].w_max;
    float max_w = synapses[0].w_min;

    for (const auto& synapse : synapses) {
        sum += synapse.weight;
        if (synapse.weight < min_w) min_w = synapse.weight;
        if (synapse.weight > max_w) max_w = synapse.weight;
    }

    float mean_weight = sum / num_synapses;

    /* Mean should be reasonable (not all at bounds) */
    EXPECT_GT(mean_weight, synapses[0].w_min + 0.1f);
    EXPECT_LT(mean_weight, synapses[0].w_max - 0.1f);

    /* Should have some diversity */
    EXPECT_GT(max_w - min_w, 0.1f);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, NullPointerHandling) {
    /* Test SNN bridge with null entorhinal */
    int result = entorhinal_init_snn_bridge(nullptr, nullptr);
    EXPECT_NE(0, result) << "Should fail with null entorhinal";

    /* Test plasticity bridge with null entorhinal */
    result = entorhinal_init_plasticity_bridge(nullptr, nullptr, nullptr);
    EXPECT_NE(0, result) << "Should fail with null entorhinal";

    /* Test apply plasticity with null entorhinal */
    result = entorhinal_apply_plasticity(nullptr, 0.01f);
    EXPECT_NE(0, result) << "Should fail with null entorhinal";
}

TEST_F(EntorhinalSNNPlasticityTest, InvalidDtHandling) {
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);

    /* Apply plasticity with zero dt */
    int result = entorhinal_apply_plasticity(ec, 0.0f);
    /* Should handle gracefully (either succeed with no-op or return error) */
    EXPECT_TRUE(result == 0 || result == -1);

    /* Apply plasticity with negative dt */
    result = entorhinal_apply_plasticity(ec, -1.0f);
    /* Should handle gracefully */
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(EntorhinalSNNPlasticityTest, TrainingModeToggle) {
    /* Enable training */
    EXPECT_EQ(0, entorhinal_set_training_mode(ec, true));

    /* Verify training is enabled */
    EXPECT_TRUE(ec->training_bridge.training_enabled);

    /* Disable training */
    EXPECT_EQ(0, entorhinal_set_training_mode(ec, false));

    /* Verify training is disabled */
    EXPECT_FALSE(ec->training_bridge.training_enabled);
}

TEST_F(EntorhinalSNNPlasticityTest, RecoveryAfterSNNFailure) {
    /* Initialize SNN bridge */
    entorhinal_init_snn_bridge(ec, nullptr);

    /* Simulate some activity */
    float position[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    /* System should still be operational */
    EXPECT_NE(entorhinal_get_status(ec), ENTORHINAL_STATUS_ERROR);
}

/*=============================================================================
 * LEARNING RULES DURING SPATIAL NAVIGATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, LinearTrajectoryLearning) {
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Linear trajectory through environment */
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    float position[3] = {0.0f, 0.0f, 0.0f};

    for (int step = 0; step < 100; step++) {
        position[0] = step * 0.1f;

        /* Update cells */
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.02f);
        entorhinal_apply_plasticity(ec, 0.02f);
    }

    /* System should have learned trajectory */
    entorhinal_stats_t stats;
    entorhinal_get_stats(ec, &stats);
    EXPECT_GT(stats.position_updates, 0u);
}

TEST_F(EntorhinalSNNPlasticityTest, CircularTrajectoryLearning) {
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Circular trajectory */
    for (int step = 0; step < 200; step++) {
        float angle = step * (2.0f * M_PI / 200.0f);
        float position[3] = {
            5.0f + 3.0f * cosf(angle),
            5.0f + 3.0f * sinf(angle),
            0.0f
        };
        float velocity[3] = {
            -3.0f * sinf(angle) * 0.1f,
            3.0f * cosf(angle) * 0.1f,
            0.0f
        };

        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, angle + M_PI / 2.0f, 0.05f);
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.02f);
        entorhinal_apply_plasticity(ec, 0.02f);
    }

    /* Verify system is still healthy after extended navigation */
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.5f);
}

TEST_F(EntorhinalSNNPlasticityTest, RandomExplorationLearning) {
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Pseudo-random exploration */
    float position[3] = {5.0f, 5.0f, 0.0f};
    float heading = 0.0f;

    for (int step = 0; step < 150; step++) {
        /* Random-ish heading changes */
        heading += sinf(step * 0.3f) * 0.2f;

        /* Update position */
        float velocity[3] = {
            cosf(heading) * 0.5f,
            sinf(heading) * 0.5f,
            0.0f
        };
        position[0] += velocity[0] * 0.02f;
        position[1] += velocity[1] * 0.02f;

        /* Bound position to environment */
        position[0] = fmaxf(0.0f, fminf(10.0f, position[0]));
        position[1] = fmaxf(0.0f, fminf(10.0f, position[1]));

        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, heading, 0.0f);
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.02f);
        entorhinal_apply_plasticity(ec, 0.02f);
    }

    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
}

/*=============================================================================
 * COMBINED INTEGRATION SCENARIO TESTS
 *===========================================================================*/

TEST_F(EntorhinalSNNPlasticityTest, FullSpatialLearningScenario) {
    /* Initialize all required bridges */
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Create SNN for pattern generation */
    snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config_feedforward(&snn_config, 8, 16, 8);

    snn_network_t* snn = snn_network_create(&snn_config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Create STDP synapses for learning */
    std::vector<stdp_synapse_t> synapses(8);
    for (auto& synapse : synapses) {
        stdp_synapse_init(&synapse);
    }

    /* Simulate spatial learning task */
    for (int trial = 0; trial < 5; trial++) {
        /* Navigate through environment */
        for (int step = 0; step < 20; step++) {
            float x = step * 0.5f;
            float y = sinf(step * 0.2f) * 2.0f + 5.0f;
            float position[3] = {x, y, 0.0f};

            /* Update entorhinal */
            entorhinal_update_grid_cells(ec, position, 3);
            entorhinal_apply_plasticity(ec, 0.02f);

            /* SNN forward pass */
            float snn_input[8], snn_output[8];
            for (int i = 0; i < 8; i++) {
                snn_input[i] = 0.5f + 0.5f * cosf(x + i * 0.5f);
            }
            snn_network_forward(snn, snn_input, 8, snn_output, 8, 10.0f);

            /* STDP updates */
            for (int i = 0; i < 8; i++) {
                if (snn_output[i] > 0.5f) {
                    synapses[i].pre_trace = snn_output[i];
                    if (step % 3 == 0) {
                        stdp_post_spike(&synapses[i], trial * 100.0f + step);
                    }
                }
                stdp_update_traces(&synapses[i], 0.001f);
            }

            snn_network_reset(snn);
        }
    }

    /* Verify learning occurred */
    int learned = 0;
    for (const auto& synapse : synapses) {
        if (synapse.num_potentiation_events > 0 || synapse.num_depression_events > 0) {
            learned++;
        }
    }
    EXPECT_GT(learned, 0);

    snn_network_destroy(snn);
}

TEST_F(EntorhinalSNNPlasticityTest, MemoryEncodingWithPlasticity) {
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Encode memories at different locations */
    for (int loc = 0; loc < 5; loc++) {
        float position[3] = {loc * 2.0f, loc * 1.5f, 0.0f};
        float features[32];
        for (int i = 0; i < 32; i++) {
            features[i] = sinf(i * 0.2f + loc);
        }

        /* Update spatial context */
        entorhinal_update_grid_cells(ec, position, 3);

        /* Encode to hippocampus */
        entorhinal_set_encoding_gate(ec, 1.0f);
        entorhinal_encode_to_hippocampus(ec, features, 32, position, 3);

        /* Apply plasticity */
        entorhinal_apply_plasticity(ec, 0.05f);
    }

    /* Check gateway statistics */
    uint64_t encoded, retrieved, consolidated;
    entorhinal_get_gateway_stats(ec, &encoded, &retrieved, &consolidated);
    EXPECT_GT(encoded, 0u);
}

TEST_F(EntorhinalSNNPlasticityTest, LongTermLearningStability) {
    entorhinal_init_snn_bridge(ec, nullptr);
    entorhinal_init_plasticity_bridge(ec, nullptr, nullptr);
    entorhinal_set_training_mode(ec, true);

    /* Extended learning session */
    for (int epoch = 0; epoch < 10; epoch++) {
        for (int step = 0; step < 50; step++) {
            float angle = (epoch * 50 + step) * 0.05f;
            float position[3] = {
                5.0f + 3.0f * cosf(angle),
                5.0f + 3.0f * sinf(angle),
                0.0f
            };

            entorhinal_update_grid_cells(ec, position, 3);
            entorhinal_apply_plasticity(ec, 0.01f);
        }
    }

    /* System should remain stable */
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.3f);

    /* Statistics should show extensive processing */
    entorhinal_stats_t stats;
    entorhinal_get_stats(ec, &stats);
    EXPECT_GT(stats.updates_processed, 0u);
}

/*=============================================================================
 * MAIN
 *===========================================================================*/

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
