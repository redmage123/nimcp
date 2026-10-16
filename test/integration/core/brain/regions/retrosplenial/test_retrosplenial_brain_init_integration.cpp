/**
 * @file test_retrosplenial_brain_init_integration.cpp
 * @brief Integration tests for Retrosplenial Cortex brain initialization
 *
 * WHAT: Tests Retrosplenial Cortex integration with brain factory
 * WHY:  Ensure proper lifecycle and spatial-contextual integration
 * HOW:  Test creation, configuration, spatial transforms, context, and navigation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

/* Include nimcp.h after region headers to avoid type conflicts */
#include "core/brain/regions/retrosplenial/nimcp_retrosplenial.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class RetrosplenialBrainInitTest : public ::testing::Test {
protected:
    nimcp_retrosplenial_t* rsc;
    nimcp_rsc_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;
        rsc = NULL;

        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == 0) {
            router_initialized = true;
        }

        config = nimcp_rsc_default_config();
    }

    void TearDown() override {
        if (rsc) {
            nimcp_rsc_destroy(rsc);
            rsc = NULL;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, CreateWithDefaultConfig) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);
    EXPECT_TRUE(rsc->initialized);
}

TEST_F(RetrosplenialBrainInitTest, DestroyNull) {
    nimcp_rsc_destroy(NULL);
}

TEST_F(RetrosplenialBrainInitTest, ResetAfterCreate) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_error_t result = nimcp_rsc_reset(rsc);
    EXPECT_EQ(RSC_OK, result);
    EXPECT_TRUE(rsc->initialized);
}

TEST_F(RetrosplenialBrainInitTest, MultipleCreateDestroyCycles) {
    for (int i = 0; i < 5; i++) {
        rsc = nimcp_rsc_create(&config);
        ASSERT_NE(nullptr, rsc) << "Cycle " << i << " failed";
        nimcp_rsc_destroy(rsc);
        rsc = NULL;
    }
}

/*=============================================================================
 * STATUS TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, GetStatus) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_status_t status = nimcp_rsc_get_status(rsc);
    EXPECT_EQ(RSC_STATUS_IDLE, status);
}

TEST_F(RetrosplenialBrainInitTest, GetLastError) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_error_t error = nimcp_rsc_get_last_error(rsc);
    EXPECT_EQ(RSC_OK, error);
}

/*=============================================================================
 * REFERENCE FRAME TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, TransformPosition) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_position_t input;
    memset(&input, 0, sizeof(input));
    input.x = 1.0f;
    input.y = 2.0f;
    input.z = 3.0f;

    nimcp_rsc_position_t output;
    nimcp_rsc_error_t result = nimcp_rsc_transform_position(
        rsc, &input, RSC_FRAME_EGOCENTRIC, RSC_FRAME_ALLOCENTRIC, &output);
    EXPECT_EQ(RSC_OK, result);
}

/*=============================================================================
 * CONTEXT TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, EncodeContext) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    /* Use spatial and temporal features */
    float spatial_features[16] = {0.5f};
    float temporal_features[8] = {0.3f};

    nimcp_rsc_error_t result = nimcp_rsc_encode_context(rsc, spatial_features, 16, temporal_features, 8);
    EXPECT_EQ(RSC_OK, result);
}

TEST_F(RetrosplenialBrainInitTest, GetContext) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_context_t context;
    nimcp_rsc_error_t result = nimcp_rsc_get_context(rsc, &context);
    EXPECT_EQ(RSC_OK, result);
}

/*=============================================================================
 * SCENE RECOGNITION TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, ProcessScene) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    /* Process scene with feature vector */
    float scene_features[32] = {0.5f};
    nimcp_rsc_error_t result = nimcp_rsc_process_scene(rsc, scene_features, 32);
    EXPECT_EQ(RSC_OK, result);
}

TEST_F(RetrosplenialBrainInitTest, GetFamiliarity) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_familiarity_t familiarity;
    float score = 0.0f;
    nimcp_rsc_error_t result = nimcp_rsc_get_familiarity(rsc, &familiarity, &score);
    EXPECT_EQ(RSC_OK, result);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

/*=============================================================================
 * NAVIGATION TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, GetNavigationState) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_navigation_t state;
    nimcp_rsc_error_t result = nimcp_rsc_get_navigation_state(rsc, &state);
    EXPECT_EQ(RSC_OK, result);
}

TEST_F(RetrosplenialBrainInitTest, IntegrateHeadDirection) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_error_t result = nimcp_rsc_integrate_head_direction(rsc, 0.5f, 0.9f);
    EXPECT_EQ(RSC_OK, result);
}

/*=============================================================================
 * LANDMARK TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, AddLandmark) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    nimcp_rsc_position_t position;
    memset(&position, 0, sizeof(position));
    position.x = 10.0f;
    position.y = 20.0f;
    position.z = 0.0f;

    float visual_features[16] = {0.5f};
    uint32_t landmark_id = 0;

    nimcp_rsc_error_t result = nimcp_rsc_add_landmark(
        rsc, &position, "test_landmark", visual_features, 16, &landmark_id);
    EXPECT_EQ(RSC_OK, result);
}

/*=============================================================================
 * UPDATE CYCLE TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, UpdateCycle) {
    rsc = nimcp_rsc_create(&config);
    ASSERT_NE(nullptr, rsc);

    for (int i = 0; i < 100; i++) {
        nimcp_rsc_error_t result = nimcp_rsc_update(rsc, 10.0f);
        EXPECT_EQ(RSC_OK, result);
    }

    EXPECT_TRUE(rsc->initialized);
}

/*=============================================================================
 * STRING FUNCTION TESTS
 *===========================================================================*/

TEST_F(RetrosplenialBrainInitTest, ErrorString) {
    EXPECT_NE(nullptr, nimcp_rsc_error_string(RSC_OK));
    EXPECT_NE(nullptr, nimcp_rsc_error_string(RSC_ERR_NULL_PTR));
    EXPECT_NE(nullptr, nimcp_rsc_error_string(RSC_ERR_NOT_INITIALIZED));
}

TEST_F(RetrosplenialBrainInitTest, StatusString) {
    EXPECT_NE(nullptr, nimcp_rsc_status_string(RSC_STATUS_IDLE));
    EXPECT_NE(nullptr, nimcp_rsc_status_string(RSC_STATUS_NAVIGATING));
}

TEST_F(RetrosplenialBrainInitTest, FrameString) {
    EXPECT_NE(nullptr, nimcp_rsc_frame_string(RSC_FRAME_EGOCENTRIC));
    EXPECT_NE(nullptr, nimcp_rsc_frame_string(RSC_FRAME_ALLOCENTRIC));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
