/**
 * @file test_parahipp_bio_async_integration.cpp
 * @brief Integration tests for Parahippocampal Cortex with Bio-Async messaging system
 *
 * WHAT: Tests Parahippocampal Cortex integration with bio-async neuromodulator channels
 * WHY:  Ensure proper scene processing communication via biological messaging
 * HOW:  Test router initialization, module registration, and scene operations
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

class ParahippBioAsyncIntegrationTest : public ::testing::Test {
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
 * BIO-ROUTER LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ParahippBioAsyncIntegrationTest, RouterInitialization) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }
    EXPECT_TRUE(bio_router_is_initialized());
}

TEST_F(ParahippBioAsyncIntegrationTest, RouterGetGlobal) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_router_t global_router = bio_router_get_global();
    EXPECT_NE(nullptr, global_router);
}

TEST_F(ParahippBioAsyncIntegrationTest, DefaultConfig) {
    bio_router_config_t default_cfg = bio_router_default_config();
    EXPECT_GT(default_cfg.max_modules, 0u);
    EXPECT_GT(default_cfg.inbox_capacity, 0u);
}

/*=============================================================================
 * MODULE REGISTRATION TESTS
 *===========================================================================*/

TEST_F(ParahippBioAsyncIntegrationTest, RegisterParahippModule) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_PARIETAL_CORTEX;  /* Parahippocampal is part of parietal/temporal */
    info.module_name = "parahippocampal_cortex";
    info.inbox_capacity = 128;
    info.user_data = parahipp;

    bio_module_context_t ctx = bio_router_register_module(&info);
    EXPECT_NE(nullptr, ctx);

    if (ctx) {
        bio_router_unregister_module(ctx);
    }
}

/*=============================================================================
 * ROUTER STATISTICS TESTS
 *===========================================================================*/

TEST_F(ParahippBioAsyncIntegrationTest, RouterStatistics) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    bio_router_stats_t stats;
    nimcp_error_t result = bio_router_get_stats(&stats);
    EXPECT_EQ(NIMCP_OK, result);
    EXPECT_GE(stats.messages_routed, 0u);
}

/*=============================================================================
 * SCENE OPERATIONS WITH BIO-ASYNC TESTS
 *===========================================================================*/

TEST_F(ParahippBioAsyncIntegrationTest, ParahippBioAsyncEnabled) {
    parahipp_config_t retrieved;
    EXPECT_EQ(0, parahipp_get_config(parahipp, &retrieved));
    EXPECT_EQ(router_initialized, retrieved.enable_bio_async);
}

TEST_F(ParahippBioAsyncIntegrationTest, SceneEncodingWithBioAsync) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    int result = parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "test_scene", &scene_id);
    EXPECT_EQ(0, result);
    EXPECT_GE(scene_id, 0u);
}

TEST_F(ParahippBioAsyncIntegrationTest, SceneRecognitionWithBioAsync) {
    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    ASSERT_EQ(0, parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "known_scene", &scene_id));

    parahipp_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int rec_result = parahipp_recognize_scene(parahipp, features, 512, &result);
    EXPECT_EQ(0, rec_result);
    EXPECT_GT(result.match_confidence, 0.0f);
}

TEST_F(ParahippBioAsyncIntegrationTest, MultipleEncodingsWithBioAsync) {
    for (int i = 0; i < 10; i++) {
        float features[512];
        CreateTestFeatures(features, 512, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "scene_%d", i);

        float pos[3] = {(float)i, 0.0f, 0.0f};

        uint32_t scene_id = 0;
        int result = parahipp_encode_scene(parahipp, features, 512,
            pos, default_heading, name, &scene_id);
        EXPECT_EQ(0, result);
        EXPECT_GE(scene_id, 0u);
    }

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.scenes_encoded, 10u);
}

/*=============================================================================
 * UPDATE CYCLE WITH BIO-ASYNC TESTS
 *===========================================================================*/

TEST_F(ParahippBioAsyncIntegrationTest, UpdateCycleWithBioAsync) {
    for (int i = 0; i < 100; i++) {
        int result = parahipp_update(parahipp, 10.0f);
        EXPECT_EQ(0, result);
    }

    parahipp_stats_t stats;
    EXPECT_EQ(0, parahipp_get_stats(parahipp, &stats));
    EXPECT_GE(stats.updates_processed, 100u);
}

TEST_F(ParahippBioAsyncIntegrationTest, BidirectionalUpdateWithBioAsync) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available for bidirectional test";
    }

    for (int i = 0; i < 50; i++) {
        int result = parahipp_bidirectional_update(parahipp, 10.0f);
        EXPECT_EQ(0, result);
    }
}

/*=============================================================================
 * STATE PERSISTENCE TESTS
 *===========================================================================*/

TEST_F(ParahippBioAsyncIntegrationTest, ParahippResetDoesNotAffectRouter) {
    if (!router_initialized) {
        GTEST_SKIP() << "Bio-router not available";
    }

    float features[512];
    CreateTestFeatures(features, 512, 0.5f);

    uint32_t scene_id = 0;
    parahipp_encode_scene(parahipp, features, 512,
        default_position, default_heading, "test", &scene_id);

    parahipp_reset(parahipp);
    parahipp_status_t status = parahipp_get_status(parahipp);
    EXPECT_GE((int)status, 0);

    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_stats_t stats;
    EXPECT_EQ(NIMCP_OK, bio_router_get_stats(&stats));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
