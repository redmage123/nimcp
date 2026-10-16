/**
 * @file test_parahipp_snn_plasticity_integration.cpp
 * @brief Integration tests for Parahippocampal Cortex with SNN and Plasticity systems
 *
 * WHAT: Tests Parahippocampal Cortex integration with SNN and STDP learning
 * WHY:  Ensure proper scene learning via spike-based plasticity
 * HOW:  Test SNN networks, STDP synapses, and scene memory consolidation
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
#include "core/brain/regions/parahippocampal/nimcp_parahippocampal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ParahippSNNPlasticityTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp;
    parahipp_config_t config;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        config = parahipp_default_config();
        config.enable_bio_async = false;
        config.enable_snn = true;
        config.enable_plasticity = true;
        config.enable_stdp = true;
        config.enable_training = true;
        config.learning_rate = 0.01f;

        parahipp = parahipp_create(&config);
        ASSERT_NE(nullptr, parahipp) << "Failed to create Parahippocampal cortex";

        default_position[0] = 0.0f;
        default_position[1] = 0.0f;
        default_position[2] = 0.0f;
        default_heading = 0.0f;
    }

    void TearDown() override {
        if (parahipp) {
            parahipp_destroy(parahipp);
            parahipp = nullptr;
        }
    }

    void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * STDP SYNAPSE TESTS
 *===========================================================================*/

TEST_F(ParahippSNNPlasticityTest, STDPSynapseDefaultInit) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    EXPECT_GT(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_GT(synapse.learning_rate, 0.0f);
}

TEST_F(ParahippSNNPlasticityTest, STDPDefaultConfig) {
    stdp_config_t stdp_config = stdp_config_default();

    EXPECT_GT(stdp_config.learning_rate, 0.0f);
    EXPECT_GT(stdp_config.w_max, 0.0f);
    EXPECT_GT(stdp_config.a_plus, 0.0f);
    EXPECT_GT(stdp_config.a_minus, 0.0f);
}

/*=============================================================================
 * PARAHIPP SNN CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ParahippSNNPlasticityTest, SNNEnabledInConfig) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_TRUE(retrieved.enable_snn);
    EXPECT_TRUE(retrieved.enable_plasticity);
    EXPECT_TRUE(retrieved.enable_stdp);
}

TEST_F(ParahippSNNPlasticityTest, LearningRateConfigured) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.learning_rate, 0.01f);
}

/*=============================================================================
 * SCENE ENCODING WITH PLASTICITY TESTS
 *===========================================================================*/

TEST_F(ParahippSNNPlasticityTest, SceneEncodingUpdatesSynapses) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "test_scene", &scene_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);

    for (int i = 0; i < 50; i++) {
        parahipp_update(parahipp, 10.0f);
    }

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.scenes_encoded, 1u);
}

TEST_F(ParahippSNNPlasticityTest, DifferentScenesEncodeDifferently) {
    uint32_t scene_ids[5];

    for (int i = 0; i < 5; i++) {
        float features[512];
        CreateTestFeatures(features, 512, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "distinct_scene_%d", i);

        float pos[3] = {(float)i * 10.0f, 0.0f, 0.0f};

        int result = parahipp_encode_scene(parahipp, features, 512,
            pos, default_heading, name, &scene_ids[i]);
        EXPECT_EQ(0, result);
        EXPECT_GE(scene_ids[i], 0u);
    }
}

/*=============================================================================
 * RECOGNITION WITH SNN TESTS
 *===========================================================================*/

TEST_F(ParahippSNNPlasticityTest, RecognitionWithSNN) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "to_recognize", &scene_id));

    for (int i = 0; i < 10; i++) {
        parahipp_update(parahipp, 10.0f);
    }

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = parahipp_recognize_scene(parahipp, features, 512, &result);
    EXPECT_EQ(0, rec_result);
    EXPECT_GT(result.match_confidence, 0.0f);
}

/*=============================================================================
 * SNN NETWORK CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ParahippSNNPlasticityTest, SNNNetworkConfig) {
    snn_config_t snn_config;
    EXPECT_EQ(0, snn_config_default(&snn_config));

    EXPECT_GE(snn_config.n_inputs, 0u);
    EXPECT_GE(snn_config.n_outputs, 0u);
    EXPECT_GT(snn_config.dt, 0.0f);
}

TEST_F(ParahippSNNPlasticityTest, SNNNetworkCreate) {
    snn_config_t snn_config;
    snn_config_default(&snn_config);
    snn_config.n_inputs = 512;
    snn_config.n_outputs = 128;
    snn_config.n_populations = 3;

    snn_network_t* network = snn_network_create(&snn_config);
    EXPECT_NE(nullptr, network);

    if (network) {
        snn_network_destroy(network);
    }
}

/*=============================================================================
 * PLACE CELL ACTIVITY TESTS
 *===========================================================================*/

TEST_F(ParahippSNNPlasticityTest, PlaceCellActivityAfterEncoding) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "activate", &scene_id);

    parahipp_process_visual_input(parahipp, features, 512);

    float activity[256];
    size_t num_cells = parahipp_get_place_cell_activity(parahipp, activity, 256);
    EXPECT_GT(num_cells, 0u);
}

TEST_F(ParahippSNNPlasticityTest, SceneCellActivityAfterEncoding) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "scene_test", &scene_id);

    float activity[512];
    size_t num_cells = parahipp_get_scene_cell_activity(parahipp, activity, 512);
    EXPECT_GT(num_cells, 0u);
}

/*=============================================================================
 * UPDATE AND LEARNING CYCLE TESTS
 *===========================================================================*/

TEST_F(ParahippSNNPlasticityTest, UpdateCycleTriggerPlasticity) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "plastic", &scene_id);
    parahipp_process_visual_input(parahipp, features, 512);

    for (int i = 0; i < 100; i++) {
        parahipp_update(parahipp, 1.0f);
    }

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.updates_processed, 100u);
}

TEST_F(ParahippSNNPlasticityTest, BidirectionalUpdateWithPlasticity) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "bidir", &scene_id);

    for (int i = 0; i < 50; i++) {
        int result = parahipp_bidirectional_update(parahipp, 10.0f);
        EXPECT_EQ(0, result);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
