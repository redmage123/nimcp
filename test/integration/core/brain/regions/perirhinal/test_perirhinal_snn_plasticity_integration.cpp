/**
 * @file test_perirhinal_snn_plasticity_integration.cpp
 * @brief Integration tests for Perirhinal Cortex with SNN and Plasticity systems
 *
 * WHAT: Tests Perirhinal Cortex integration with SNN and STDP learning
 * WHY:  Ensure proper object recognition learning via spike-based plasticity
 * HOW:  Test SNN networks, STDP synapses, and recognition memory consolidation
 *
 * BIOLOGICAL BASIS:
 * Perirhinal cortex relies on spike-timing dependent plasticity for:
 * - Object representation learning
 * - Familiarity memory encoding
 * - Recognition signal strengthening
 * - Synaptic weight adjustment for object cells
 *
 * INTEGRATION POINTS:
 * - SNN network creation and simulation
 * - STDP synapse initialization and learning
 * - Object recognition via SNN output
 * - Familiarity signal generation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Include NIMCP headers first, then region-specific for typedef compatibility
#include "nimcp.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_types.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PerirhinalSNNPlasticityTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;

    void SetUp() override {
        /* Configure perirhinal cortex with SNN and plasticity enabled */
        config = perirhinal_default_config();
        config.enable_bio_async = false;
        config.enable_snn = true;
        config.enable_plasticity = true;
        config.enable_stdp = true;
        config.enable_training = true;
        config.learning_rate = 0.01f;

        perirhinal = perirhinal_create(&config);
        ASSERT_NE(nullptr, perirhinal) << "Failed to create Perirhinal cortex";
    }

    void TearDown() override {
        if (perirhinal) {
            perirhinal_destroy(perirhinal);
            perirhinal = nullptr;
        }
    }

    void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * STDP SYNAPSE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, STDPSynapseDefaultInit) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Verify default parameters */
    EXPECT_GT(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_GT(synapse.learning_rate, 0.0f);
    EXPECT_GT(synapse.tau_plus, 0.0f);
    EXPECT_GT(synapse.tau_minus, 0.0f);
}

TEST_F(PerirhinalSNNPlasticityTest, STDPSynapseCustomInit) {
    stdp_synapse_t synapse;
    stdp_config_t custom_config = stdp_config_default();
    custom_config.learning_rate = 0.05f;
    custom_config.w_max = 2.0f;
    /* Note: tau values are in seconds */
    custom_config.tau_plus = 0.015f;
    custom_config.tau_minus = 0.025f;

    stdp_synapse_init_with_config(&synapse, &custom_config);

    EXPECT_FLOAT_EQ(synapse.learning_rate, 0.05f);
    EXPECT_FLOAT_EQ(synapse.w_max, 2.0f);
    EXPECT_FLOAT_EQ(synapse.tau_plus, 0.015f);
    EXPECT_FLOAT_EQ(synapse.tau_minus, 0.025f);
}

TEST_F(PerirhinalSNNPlasticityTest, STDPDefaultConfig) {
    stdp_config_t config = stdp_config_default();

    /* Verify sensible defaults */
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GT(config.w_max, 0.0f);
    EXPECT_GT(config.a_plus, 0.0f);
    EXPECT_GT(config.a_minus, 0.0f);
    /* tau values are in seconds (around 20ms typical) */
    EXPECT_NEAR(config.tau_plus, 0.020f, 0.010f);
    EXPECT_NEAR(config.tau_minus, 0.020f, 0.010f);
}

/*=============================================================================
 * STDP TRACE UPDATE TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, STDPTraceDecay) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set initial traces */
    synapse.pre_trace = 1.0f;
    synapse.post_trace = 1.0f;

    /* Update traces (decay) */
    float dt = 0.001f;  /* 1ms */
    for (int i = 0; i < 100; i++) {
        stdp_update_traces(&synapse, dt);
    }

    /* Traces should decay toward zero */
    EXPECT_LT(synapse.pre_trace, 1.0f);
    EXPECT_LT(synapse.post_trace, 1.0f);
    EXPECT_GT(synapse.pre_trace, 0.0f);
    EXPECT_GT(synapse.post_trace, 0.0f);
}

/*=============================================================================
 * STDP SPIKE PROCESSING TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, STDPPreSpikeLTD) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set up post-trace (post fired recently) */
    synapse.post_trace = 0.5f;
    float initial_weight = synapse.weight;

    /* Pre-spike after post (LTD) */
    float weight_change = stdp_pre_spike(&synapse, 100.0f);

    /* Should cause depression (negative weight change) */
    EXPECT_LE(weight_change, 0.0f);
    EXPECT_LE(synapse.weight, initial_weight);
}

TEST_F(PerirhinalSNNPlasticityTest, STDPPostSpikeLTP) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set up pre-trace (pre fired recently) */
    synapse.pre_trace = 0.5f;
    float initial_weight = synapse.weight;

    /* Post-spike after pre (LTP) */
    float weight_change = stdp_post_spike(&synapse, 100.0f);

    /* Should cause potentiation (positive weight change) */
    EXPECT_GE(weight_change, 0.0f);
    EXPECT_GE(synapse.weight, initial_weight);
}

TEST_F(PerirhinalSNNPlasticityTest, STDPWeightBounds) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Repeatedly potentiate */
    synapse.pre_trace = 1.0f;
    for (int i = 0; i < 1000; i++) {
        stdp_post_spike(&synapse, (float)i);
        synapse.pre_trace = 1.0f;
    }

    /* Weight should be bounded by w_max */
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_GE(synapse.weight, synapse.w_min);
}

/*=============================================================================
 * PERIRHINAL SNN CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, SNNEnabledInConfig) {
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(perirhinal, &retrieved));
    EXPECT_TRUE(retrieved.enable_snn);
    EXPECT_TRUE(retrieved.enable_plasticity);
    EXPECT_TRUE(retrieved.enable_stdp);
}

TEST_F(PerirhinalSNNPlasticityTest, LearningRateConfigured) {
    perirhinal_config_t retrieved;
    EXPECT_EQ(0, perirhinal_get_config(perirhinal, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.learning_rate, 0.01f);
}

/*=============================================================================
 * OBJECT ENCODING WITH PLASTICITY TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, ObjectEncodingUpdatesSynapses) {
    /* Encode an object */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, 256, "test_obj", &object_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(object_id, 0u);

    /* Run update cycles to process plasticity */
    for (int i = 0; i < 50; i++) {
        perirhinal_update(perirhinal, 10.0f);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.objects_encoded, 1u);
}

TEST_F(PerirhinalSNNPlasticityTest, RepeatedExposureStrengthens) {
    float features[256];
    CreateTestFeatures(features, 256, 0.3f);

    /* Encode object */
    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "repeated", &object_id));

    /* Multiple exposures (simulate repeated viewing) */
    for (int trial = 0; trial < 10; trial++) {
        perirhinal_update_familiarity(perirhinal, object_id);
        perirhinal_update(perirhinal, 10.0f);
    }

    /* Familiarity should increase with repeated exposure */
    float familiarity = perirhinal_get_object_familiarity(perirhinal, object_id);
    EXPECT_GT(familiarity, 0.0f);
}

TEST_F(PerirhinalSNNPlasticityTest, DifferentObjectsEncodeDifferently) {
    uint32_t object_ids[5];

    /* Encode multiple distinct objects */
    for (int i = 0; i < 5; i++) {
        float features[256];
        CreateTestFeatures(features, 256, (float)i * 0.2f);

        char name[32];
        snprintf(name, sizeof(name), "distinct_%d", i);

        int result = perirhinal_encode_object(perirhinal, features, 256, name, &object_ids[i]);
        EXPECT_EQ(0, result);
        EXPECT_GE(object_ids[i], 0u);
    }

    /* Each should have unique ID */
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            EXPECT_NE(object_ids[i], object_ids[j]);
        }
    }
}

/*=============================================================================
 * RECOGNITION WITH SNN TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, RecognitionWithSNN) {
    /* Encode object */
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "to_recognize", &object_id));

    /* Run updates to consolidate */
    for (int i = 0; i < 10; i++) {
        perirhinal_update(perirhinal, 10.0f);
    }

    /* Recognize */
    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = perirhinal_recognize_object(perirhinal, features, 256, &result);
    EXPECT_EQ(0, rec_result);
    EXPECT_GT(result.match_confidence, 0.0f);
}

TEST_F(PerirhinalSNNPlasticityTest, RecognitionConfidenceIncreasesWithExposure) {
    float features[256];
    CreateTestFeatures(features, 256, 0.4f);

    /* Encode object */
    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "exposed", &object_id));

    /* Get initial recognition */
    perirhinal_recognition_result_t result1;
    memset(&result1, 0, sizeof(result1));
    perirhinal_recognize_object(perirhinal, features, 256, &result1);
    float initial_confidence = result1.match_confidence;

    /* Multiple exposures */
    for (int i = 0; i < 20; i++) {
        perirhinal_update_familiarity(perirhinal, object_id);
        perirhinal_update(perirhinal, 10.0f);
    }

    /* Get later recognition */
    perirhinal_recognition_result_t result2;
    memset(&result2, 0, sizeof(result2));
    perirhinal_recognize_object(perirhinal, features, 256, &result2);

    /* Confidence should be at least as good after exposure */
    EXPECT_GE(result2.match_confidence, initial_confidence * 0.9f);
}

/*=============================================================================
 * FAMILIARITY SIGNAL GENERATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, FamiliaritySignalForKnownObject) {
    float features[256];
    CreateTestFeatures(features, 256, 0.6f);

    /* Encode object */
    uint32_t object_id = 0;
    ASSERT_EQ(0, perirhinal_encode_object(perirhinal, features, 256, "familiar", &object_id));

    /* Compute familiarity */
    float familiarity = perirhinal_compute_familiarity(perirhinal, features, 256);
    EXPECT_GE(familiarity, 0.0f);
    EXPECT_LE(familiarity, 1.0f);
}

TEST_F(PerirhinalSNNPlasticityTest, FamiliaritySignalForNovelObject) {
    float novel_features[256];
    CreateTestFeatures(novel_features, 256, 0.99f);

    /* Novel object should have low familiarity */
    float familiarity = perirhinal_compute_familiarity(perirhinal, novel_features, 256);
    EXPECT_GE(familiarity, 0.0f);
    EXPECT_LE(familiarity, 1.0f);
}

TEST_F(PerirhinalSNNPlasticityTest, FamiliarityClassification) {
    float low_fam = 0.1f;
    float high_fam = 0.9f;

    familiarity_type_t low_type = perirhinal_classify_familiarity(perirhinal, low_fam);
    familiarity_type_t high_type = perirhinal_classify_familiarity(perirhinal, high_fam);

    /* Low familiarity should be novel-ish */
    EXPECT_LE(low_type, FAMILIARITY_TYPE_FAMILIAR);

    /* High familiarity should be familiar-ish */
    EXPECT_GE(high_type, FAMILIARITY_TYPE_FAMILIAR);
}

/*=============================================================================
 * NOVELTY DETECTION WITH SNN TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, NoveltyForUnseenObject) {
    float novel_features[256];
    CreateTestFeatures(novel_features, 256, 0.95f);

    float novelty = perirhinal_compute_novelty(perirhinal, novel_features, 256);
    EXPECT_GE(novelty, 0.0f);

    bool is_novel = perirhinal_is_novel(perirhinal, novel_features, 256);
    EXPECT_TRUE(is_novel);
}

TEST_F(PerirhinalSNNPlasticityTest, NoveltyDecreasesWithExposure) {
    float features[256];
    CreateTestFeatures(features, 256, 0.7f);

    /* Initial novelty (should be high) */
    float initial_novelty = perirhinal_compute_novelty(perirhinal, features, 256);

    /* Encode the object */
    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "habituate", &object_id);

    /* Multiple exposures */
    for (int i = 0; i < 10; i++) {
        perirhinal_habituate(perirhinal, features, 256);
        perirhinal_update(perirhinal, 10.0f);
    }

    /* Novelty should decrease */
    float later_novelty = perirhinal_compute_novelty(perirhinal, features, 256);
    EXPECT_LE(later_novelty, initial_novelty);
}

/*=============================================================================
 * SNN NETWORK CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, SNNNetworkConfig) {
    snn_config_t snn_config;
    EXPECT_EQ(0, snn_config_default(&snn_config));

    /* Verify sensible defaults */
    EXPECT_GE(snn_config.n_inputs, 0u);
    EXPECT_GE(snn_config.n_outputs, 0u);
    EXPECT_GT(snn_config.dt, 0.0f);
}

TEST_F(PerirhinalSNNPlasticityTest, SNNNetworkCreate) {
    snn_config_t snn_config;
    snn_config_default(&snn_config);
    snn_config.n_inputs = 256;
    snn_config.n_outputs = 64;
    snn_config.n_populations = 3;

    snn_network_t* network = snn_network_create(&snn_config);
    EXPECT_NE(nullptr, network);

    if (network) {
        snn_network_destroy(network);
    }
}

/*=============================================================================
 * OBJECT CELL ACTIVITY TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, ObjectCellActivityAfterEncoding) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "activate", &object_id);

    /* Process visual input to activate cells */
    perirhinal_process_visual_input(perirhinal, features, 256);

    /* Get object cell activity */
    float activity[512];
    size_t num_cells = perirhinal_get_object_cell_activity(perirhinal, activity, 512);
    EXPECT_GT(num_cells, 0u);
}

TEST_F(PerirhinalSNNPlasticityTest, FamiliarityCellActivityAfterEncoding) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "fam_test", &object_id);

    /* Get familiarity cell activity */
    float activity[256];
    size_t num_cells = perirhinal_get_familiarity_cell_activity(perirhinal, activity, 256);
    EXPECT_GT(num_cells, 0u);
}

/*=============================================================================
 * UPDATE AND LEARNING CYCLE TESTS
 *===========================================================================*/

TEST_F(PerirhinalSNNPlasticityTest, UpdateCycleTriggerPlasticity) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    /* Encode and activate */
    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "plastic", &object_id);
    perirhinal_process_visual_input(perirhinal, features, 256);

    /* Multiple update cycles */
    for (int i = 0; i < 100; i++) {
        perirhinal_update(perirhinal, 1.0f);
    }

    perirhinal_stats_t stats;
    EXPECT_EQ(0, perirhinal_get_stats(perirhinal, &stats));
    EXPECT_GE(stats.updates_processed, 100u);
}

TEST_F(PerirhinalSNNPlasticityTest, BidirectionalUpdateWithPlasticity) {
    float features[256];
    CreateTestFeatures(features, 256, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, 256, "bidir", &object_id);

    /* Bidirectional update cycles */
    for (int i = 0; i < 50; i++) {
        int result = perirhinal_bidirectional_update(perirhinal, 10.0f);
        EXPECT_EQ(0, result);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
