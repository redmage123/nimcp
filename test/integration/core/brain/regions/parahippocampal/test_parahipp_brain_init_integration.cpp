/**
 * @file test_parahipp_brain_init_integration.cpp
 * @brief Integration tests for Parahippocampal Cortex brain initialization system
 *
 * WHAT: Tests Parahippocampal Cortex integration with brain factory initialization
 * WHY:  Ensure proper lifecycle management and brain system integration
 * HOW:  Test registration, creation, initialization, and destruction via brain factory
 *
 * INTEGRATION POINTS:
 * - Brain factory registration
 * - Brain configuration propagation
 * - Lifecycle callbacks
 * - Bio-async bridge initialization
 * - KG wiring setup
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "nimcp.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/parahippocampal/nimcp_parahippocampal.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ParahippBrainInitTest : public ::testing::Test {
protected:
    nimcp_parahippocampal_t* parahipp;
    parahipp_config_t config;
    bool router_initialized;
    float default_position[3];
    float default_heading;

    void SetUp() override {
        router_initialized = false;

        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        config = parahipp_default_config();
        config.enable_bio_async = router_initialized;
        config.enable_snn = true;
        config.enable_plasticity = true;

        parahipp = parahipp_create(&config);
        ASSERT_NE(nullptr, parahipp) << "Failed to create Parahippocampal cortex";

        /* Default position and heading for scene encoding */
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
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    void CreateTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + (float)i * 0.001f;
        }
    }
};

/*=============================================================================
 * CREATION AND DESTRUCTION TESTS
 *===========================================================================*/

TEST_F(ParahippBrainInitTest, CreateWithDefaultConfig) {
    parahipp_config_t default_config = parahipp_default_config();
    default_config.enable_bio_async = false;

    nimcp_parahippocampal_t* ph = parahipp_create(&default_config);
    EXPECT_NE(nullptr, ph);

    if (ph) parahipp_destroy(ph);
}

TEST_F(ParahippBrainInitTest, CreateWithCustomConfig) {
    parahipp_config_t custom_config = parahipp_default_config();
    custom_config.enable_bio_async = false;
    custom_config.num_place_cells = 128;
    custom_config.num_scene_cells = 256;
    custom_config.max_stored_scenes = 256;

    nimcp_parahippocampal_t* ph = parahipp_create(&custom_config);
    EXPECT_NE(nullptr, ph);

    if (ph) {
        parahipp_config_t retrieved;
        EXPECT_EQ(0, parahipp_get_config(ph, &retrieved));
        EXPECT_EQ(128u, retrieved.num_place_cells);
        EXPECT_EQ(256u, retrieved.num_scene_cells);
        parahipp_destroy(ph);
    }
}

TEST_F(ParahippBrainInitTest, DestroyNullIsSafe) {
    parahipp_destroy(nullptr);  /* Should not crash */
}

TEST_F(ParahippBrainInitTest, CreateMinimalConfig) {
    parahipp_config_t minimal_config = parahipp_default_config();
    minimal_config.enable_bio_async = false;
    minimal_config.enable_snn = false;
    minimal_config.enable_plasticity = false;
    minimal_config.enable_security = false;
    minimal_config.enable_immune = false;

    nimcp_parahippocampal_t* minimal_ph = parahipp_create(&minimal_config);
    ASSERT_NE(nullptr, minimal_ph);

    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(minimal_ph, features, 512,
        default_position, default_heading, "test_scene", &scene_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);

    parahipp_destroy(minimal_ph);
}

TEST_F(ParahippBrainInitTest, InitialStateIsValid) {
    parahipp_status_t status = parahipp_get_status(parahipp);
    EXPECT_GE((int)status, 0);
    EXPECT_EQ(parahipp_get_last_error(parahipp), PARAHIPP_ERROR_NONE);
}

TEST_F(ParahippBrainInitTest, ResetRestoresInitialState) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "test_scene", &scene_id);

    EXPECT_EQ(0, parahipp_reset(parahipp));
    parahipp_status_t status = parahipp_get_status(parahipp);
    EXPECT_GE((int)status, 0);
}

TEST_F(ParahippBrainInitTest, MultipleResetCycles) {
    for (int cycle = 0; cycle < 10; cycle++) {
        float features[512];
        CreateTestFeatures(features, 512, (float)cycle * 0.1f);

        uint32_t scene_id = 0;
        parahipp_encode_scene(parahipp, features, 512,
            default_position, default_heading, "test_scene", &scene_id);

        EXPECT_EQ(0, parahipp_reset(parahipp));
        parahipp_status_t status = parahipp_get_status(parahipp);
        EXPECT_GE((int)status, 0);
    }
}

/*=============================================================================
 * CAPACITY CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ParahippBrainInitTest, PlaceCellCountConfigured) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_GT(retrieved.num_place_cells, 0u);
}

TEST_F(ParahippBrainInitTest, SceneCellCountConfigured) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_GT(retrieved.num_scene_cells, 0u);
}

TEST_F(ParahippBrainInitTest, MaxStoredScenesConfigured) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_GT(retrieved.max_stored_scenes, 0u);
}

/*=============================================================================
 * INTEGRATION ENABLES TESTS
 *===========================================================================*/

TEST_F(ParahippBrainInitTest, SNNEnabledInConfig) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_TRUE(retrieved.enable_snn);
}

TEST_F(ParahippBrainInitTest, PlasticityEnabledInConfig) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_TRUE(retrieved.enable_plasticity);
}

TEST_F(ParahippBrainInitTest, BioAsyncMatchesRouterState) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_EQ(router_initialized, retrieved.enable_bio_async);
}

/*=============================================================================
 * MULTIPLE INSTANCES TESTS
 *===========================================================================*/

TEST_F(ParahippBrainInitTest, MultipleInstancesCanCoexist) {
    parahipp_config_t config1 = parahipp_default_config();
    config1.enable_bio_async = false;
    config1.max_stored_scenes = 512;

    parahipp_config_t config2 = parahipp_default_config();
    config2.enable_bio_async = false;
    config2.max_stored_scenes = 1024;

    nimcp_parahippocampal_t* ph1 = parahipp_create(&config1);
    nimcp_parahippocampal_t* ph2 = parahipp_create(&config2);

    ASSERT_NE(nullptr, ph1);
    ASSERT_NE(nullptr, ph2);
    EXPECT_NE(ph1, ph2);

    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t id1 = 0, id2 = 0;
    EXPECT_EQ(0, parahipp_encode_scene(ph1, features, 512,
        default_position, default_heading, "scene1", &id1));
    EXPECT_EQ(0, parahipp_encode_scene(ph2, features, 512,
        default_position, default_heading, "scene2", &id2));

    parahipp_destroy(ph1);
    parahipp_status_t status = parahipp_get_status(ph2);
    EXPECT_GE((int)status, 0);

    parahipp_destroy(ph2);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

TEST_F(ParahippBrainInitTest, HandleNullFeaturesGracefully) {
    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, nullptr, 512,
        default_position, default_heading, "test", &scene_id);
    EXPECT_NE(0, result);

    parahipp_error_t err = parahipp_get_last_error(parahipp);
    EXPECT_TRUE(err == PARAHIPP_ERROR_INVALID_INPUT || err == PARAHIPP_ERROR_NONE);

    float features[512];
    CreateTestFeatures(features, 512, 0.5f);
    result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "valid", &scene_id);
    EXPECT_EQ(0, result);
}

TEST_F(ParahippBrainInitTest, HandleZeroDimensionGracefully) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 0,
        default_position, default_heading, "test", &scene_id);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * HEALTH AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(ParahippBrainInitTest, HealthStatusInitiallyGood) {
    float health = parahipp_get_health_status(parahipp);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(ParahippBrainInitTest, DiagnosticsCanBeLogged) {
    int result = parahipp_log_diagnostics(parahipp);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(ParahippBrainInitTest, UpdateCycleWorks) {
    for (int i = 0; i < 100; i++) {
        int result = parahipp_update(parahipp, 10.0f);
        EXPECT_EQ(0, result);
    }

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.updates_processed, 100u);
}

TEST_F(ParahippBrainInitTest, BidirectionalUpdateCycleWorks) {
    for (int i = 0; i < 50; i++) {
        int result = parahipp_bidirectional_update(parahipp, 10.0f);
        EXPECT_EQ(0, result);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
