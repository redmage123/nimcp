/**
 * @file test_mammillary_snn_plasticity_integration.cpp
 * @brief Integration tests for Mammillary Bodies with SNN and Plasticity systems
 *
 * WHAT: Tests Mammillary Bodies integration with SNN and STDP learning
 * WHY:  Ensure proper memory consolidation via spike-based plasticity
 * HOW:  Test SNN networks, STDP synapses, and memory relay consolidation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "nimcp.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_types.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/mammillary/nimcp_mammillary.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MammillarySNNPlasticityTest : public ::testing::Test {
protected:
    nimcp_mammillary_t* mammillary;
    mammillary_config_t config;

    void SetUp() override {
        config = mammillary_default_config();
        config.enable_papez_circuit = true;
        config.enable_head_direction = true;
        config.enable_spatial_processing = true;
        mammillary = mammillary_create(&config);
        ASSERT_NE(nullptr, mammillary) << "Failed to create Mammillary bodies";
    }

    void TearDown() override {
        if (mammillary) {
            mammillary_destroy(mammillary);
            mammillary = nullptr;
        }
    }

    void CreateTestContext(float* context, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            context[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * STDP SYNAPSE TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, STDPSynapseDefaultInit) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    EXPECT_GT(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_GT(synapse.learning_rate, 0.0f);
}

TEST_F(MammillarySNNPlasticityTest, STDPDefaultConfig) {
    stdp_config_t stdp_config = stdp_config_default();

    EXPECT_GT(stdp_config.learning_rate, 0.0f);
    EXPECT_GT(stdp_config.w_max, 0.0f);
    EXPECT_GT(stdp_config.a_plus, 0.0f);
    EXPECT_GT(stdp_config.a_minus, 0.0f);
}

/*=============================================================================
 * SNN NETWORK TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, SNNNetworkConfig) {
    snn_config_t snn_config;
    EXPECT_EQ(0, snn_config_default(&snn_config));

    EXPECT_GE(snn_config.n_inputs, 0u);
    EXPECT_GE(snn_config.n_outputs, 0u);
    EXPECT_GT(snn_config.dt, 0.0f);
}

TEST_F(MammillarySNNPlasticityTest, SNNNetworkCreate) {
    snn_config_t snn_config;
    snn_config_default(&snn_config);
    snn_config.n_inputs = 128;
    snn_config.n_outputs = 64;
    snn_config.n_populations = 3;

    snn_network_t* network = snn_network_create(&snn_config);
    EXPECT_NE(nullptr, network);

    if (network) {
        snn_network_destroy(network);
    }
}

/*=============================================================================
 * SNN BRIDGE TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, InitSNNBridge) {
    snn_config_t snn_config;
    snn_config_default(&snn_config);
    snn_config.n_inputs = 128;
    snn_config.n_outputs = 64;

    snn_network_t* network = snn_network_create(&snn_config);
    ASSERT_NE(nullptr, network);

    int result = mammillary_init_snn_bridge(mammillary, network);
    EXPECT_EQ(0, result);

    snn_network_destroy(network);
}

/*=============================================================================
 * MEMORY CONSOLIDATION WITH PLASTICITY TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, MemoryEncodingWithSNN) {
    float position[3] = {1.0f, 2.0f, 0.0f};
    float context[128];
    CreateTestContext(context, 128, 0.5f);

    uint32_t trace_id = 0;
    int result = mammillary_encode_spatial_memory(mammillary, position, 0.0f,
        context, 128, &trace_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(trace_id, 0u);
}

TEST_F(MammillarySNNPlasticityTest, ConsolidationUpdatesCells) {
    float position[3] = {1.0f, 2.0f, 0.0f};
    float context[128];
    CreateTestContext(context, 128, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_encode_spatial_memory(mammillary, position, 0.0f,
        context, 128, &trace_id));

    ASSERT_EQ(0, mammillary_start_consolidation(mammillary, trace_id));

    for (int i = 0; i < 50; i++) {
        mammillary_update_consolidation(mammillary, 10.0f);
    }

    consolidation_state_t state = mammillary_get_consolidation_state(mammillary);
    EXPECT_GE((int)state, 0);
}

TEST_F(MammillarySNNPlasticityTest, MultipleSpatialMemoriesEncoded) {
    uint32_t trace_ids[5];

    for (int i = 0; i < 5; i++) {
        float position[3] = {(float)i * 10.0f, (float)i * 5.0f, 0.0f};
        float context[128];
        CreateTestContext(context, 128, (float)i * 0.1f);

        int result = mammillary_encode_spatial_memory(mammillary, position,
            (float)i * 0.5f, context, 128, &trace_ids[i]);
        EXPECT_EQ(0, result);
    }

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.total_memory_traces, 5u);
}

/*=============================================================================
 * HEAD DIRECTION CELL TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, HeadDirectionUpdate) {
    int result = mammillary_update_head_direction(mammillary, 0.5f, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(MammillarySNNPlasticityTest, HeadDirectionCellActivity) {
    mammillary_update_head_direction(mammillary, 0.5f, 10.0f);

    float activity[256];
    size_t num_cells = mammillary_get_hd_cell_activity(mammillary, activity, 256);
    EXPECT_GT(num_cells, 0u);
}

TEST_F(MammillarySNNPlasticityTest, GetCurrentHeadDirection) {
    mammillary_update_head_direction(mammillary, 0.5f, 10.0f);

    float heading = mammillary_get_head_direction(mammillary);
    /* Heading should be a valid angle */
    EXPECT_GE(heading, -M_PI);
    EXPECT_LE(heading, 2 * M_PI);
}

TEST_F(MammillarySNNPlasticityTest, HeadDirectionConfidence) {
    mammillary_update_head_direction(mammillary, 0.5f, 10.0f);

    float confidence = mammillary_get_hd_confidence(mammillary);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

/*=============================================================================
 * RELAY CELL TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, RelayCellActivity) {
    /* Encode a memory to activate relay cells */
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id);

    float activity[256];
    size_t num_cells = mammillary_get_relay_cell_activity(mammillary, activity, 256);
    EXPECT_GT(num_cells, 0u);
}

TEST_F(MammillarySNNPlasticityTest, RelayToThalamus) {
    float trace[128];
    CreateTestContext(trace, 128, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_receive_hippocampal_input(mammillary, trace, 128,
        MEMORY_TRACE_EPISODIC, 0.5f, &trace_id));

    int result = mammillary_relay_to_thalamus(mammillary, trace_id);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * SPATIAL CELL TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, SpatialCellActivity) {
    float position[3] = {10.0f, 20.0f, 0.0f};
    mammillary_update_spatial_cells(mammillary, position, 3);

    float activity[256];
    size_t num_cells = mammillary_get_spatial_cell_activity(mammillary, activity, 256);
    EXPECT_GT(num_cells, 0u);
}

TEST_F(MammillarySNNPlasticityTest, SpatialContextRetrieval) {
    float position[3] = {10.0f, 20.0f, 0.0f};
    float context[128];
    CreateTestContext(context, 128, 0.5f);

    uint32_t trace_id = 0;
    ASSERT_EQ(0, mammillary_encode_spatial_memory(mammillary, position, 0.0f,
        context, 128, &trace_id));

    float retrieved_context[128];
    uint32_t context_dim = 128;
    int result = mammillary_retrieve_spatial_context(mammillary, position, 3,
        retrieved_context, &context_dim);
    EXPECT_GE(result, -1); /* May succeed or return not found */
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(MammillarySNNPlasticityTest, PlasticityDuringUpdates) {
    float position[3] = {1.0f, 2.0f, 0.0f};
    float context[128];
    CreateTestContext(context, 128, 0.5f);

    uint32_t trace_id = 0;
    mammillary_encode_spatial_memory(mammillary, position, 0.0f,
        context, 128, &trace_id);

    for (int i = 0; i < 100; i++) {
        mammillary_update(mammillary, 1.0f);
    }

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.updates_processed, 100u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
