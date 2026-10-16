/**
 * @file test_perirhinal_backward_compat.cpp
 * @brief Backward compatibility regression tests for Perirhinal Cortex
 *
 * WHAT: Tests Perirhinal Cortex API stability and backward compatibility
 * WHY:  Ensure existing perirhinal code continues to work after updates
 * HOW:  Test core API functions, data structures, and return values
 *
 * REGRESSION FOCUS:
 * - API function signatures unchanged
 * - Return value semantics preserved
 * - Default behaviors maintained
 * - Error codes consistent
 * - Configuration defaults stable
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/perirhinal/nimcp_perirhinal.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class PerirhinalBackwardCompatTest : public ::testing::Test {
protected:
    nimcp_perirhinal_t* perirhinal;
    perirhinal_config_t config;

    static constexpr uint32_t FEATURE_DIM = 128;

    void SetUp() override {
        config = perirhinal_default_config();
        config.enable_bio_async = false;
        perirhinal = perirhinal_create(&config);
        ASSERT_NE(nullptr, perirhinal);
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
 * API FUNCTION SIGNATURE TESTS
 *===========================================================================*/

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_default_config_exists) {
    perirhinal_config_t cfg = perirhinal_default_config();
    EXPECT_TRUE(true);  /* Compilation success = function exists */
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_create_exists) {
    perirhinal_config_t cfg = perirhinal_default_config();
    cfg.enable_bio_async = false;
    nimcp_perirhinal_t* test = perirhinal_create(&cfg);
    ASSERT_NE(nullptr, test);
    perirhinal_destroy(test);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_destroy_exists) {
    perirhinal_config_t cfg = perirhinal_default_config();
    cfg.enable_bio_async = false;
    nimcp_perirhinal_t* test = perirhinal_create(&cfg);
    perirhinal_destroy(test);
    perirhinal_destroy(nullptr);  /* Should handle NULL safely */
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_encode_object_exists) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(perirhinal, features, FEATURE_DIM,
        "test_object", &object_id);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_recognize_object_exists) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "test", &object_id);

    perirhinal_recognition_result_t result;
    memset(&result, 0, sizeof(result));
    int ret = perirhinal_recognize_object(perirhinal, features, FEATURE_DIM, &result);
    EXPECT_GE(ret, -1);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_get_object_exists) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "test", &object_id);

    const nimcp_stored_object_t* stored = perirhinal_get_object(perirhinal, object_id);
    EXPECT_NE(nullptr, stored);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_reset_exists) {
    int result = perirhinal_reset(perirhinal);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_get_status_exists) {
    perirhinal_status_t status = perirhinal_get_status(perirhinal);
    EXPECT_GE((int)status, 0);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_get_config_exists) {
    perirhinal_config_t retrieved;
    int result = perirhinal_get_config(perirhinal, &retrieved);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_get_stats_exists) {
    perirhinal_stats_t stats;
    int result = perirhinal_get_stats(perirhinal, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_update_exists) {
    int result = perirhinal_update(perirhinal, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(PerirhinalBackwardCompatTest, API_perirhinal_process_visual_input_exists) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    int result = perirhinal_process_visual_input(perirhinal, features, FEATURE_DIM);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * RETURN VALUE SEMANTICS TESTS
 *===========================================================================*/

TEST_F(PerirhinalBackwardCompatTest, ReturnSemantics_EncodeReturnsZeroOnSuccess) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    EXPECT_EQ(0, perirhinal_encode_object(perirhinal, features, FEATURE_DIM,
        "test", &object_id));
}

TEST_F(PerirhinalBackwardCompatTest, ReturnSemantics_NullHandledGracefully) {
    /* NULL perirhinal should be handled */
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t object_id = 0;
    int result = perirhinal_encode_object(nullptr, features, FEATURE_DIM,
        "test", &object_id);
    EXPECT_NE(0, result);
}

/*=============================================================================
 * DEFAULT BEHAVIOR TESTS
 *===========================================================================*/

TEST_F(PerirhinalBackwardCompatTest, DefaultConfig_HasReasonableValues) {
    perirhinal_config_t cfg = perirhinal_default_config();

    EXPECT_GT(cfg.max_stored_objects, 0u);
    EXPECT_GT(cfg.feature_dim, 0u);
}

TEST_F(PerirhinalBackwardCompatTest, DefaultStats_AreZero) {
    perirhinal_stats_t stats;
    perirhinal_get_stats(perirhinal, &stats);

    EXPECT_EQ(0u, stats.total_stored_objects);
    EXPECT_EQ(0u, stats.correct_recognitions);
}

/*=============================================================================
 * STRUCTURE SIZE TESTS
 *===========================================================================*/

TEST_F(PerirhinalBackwardCompatTest, StructSize_perirhinal_recognition_result_t) {
    /* Structure should have minimum expected fields */
    EXPECT_GT(sizeof(perirhinal_recognition_result_t), sizeof(uint32_t) * 2);
}

TEST_F(PerirhinalBackwardCompatTest, StructSize_perirhinal_config_t) {
    EXPECT_GT(sizeof(perirhinal_config_t), sizeof(uint32_t) * 2);
}

TEST_F(PerirhinalBackwardCompatTest, StructSize_perirhinal_stats_t) {
    EXPECT_GT(sizeof(perirhinal_stats_t), sizeof(uint32_t) * 2);
}

/*=============================================================================
 * LIFECYCLE COMPATIBILITY TESTS
 *===========================================================================*/

TEST_F(PerirhinalBackwardCompatTest, Lifecycle_CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        perirhinal_config_t cfg = perirhinal_default_config();
        cfg.enable_bio_async = false;
        nimcp_perirhinal_t* test = perirhinal_create(&cfg);
        EXPECT_NE(nullptr, test);
        perirhinal_destroy(test);
    }
}

TEST_F(PerirhinalBackwardCompatTest, Lifecycle_ResetMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        float features[FEATURE_DIM];
        CreateTestFeatures(features, FEATURE_DIM, (float)i * 0.1f);

        uint32_t object_id = 0;
        perirhinal_encode_object(perirhinal, features, FEATURE_DIM, nullptr, &object_id);
        perirhinal_reset(perirhinal);
    }
}

TEST_F(PerirhinalBackwardCompatTest, Lifecycle_OperationsAfterReset) {
    float features[FEATURE_DIM];
    CreateTestFeatures(features, FEATURE_DIM, 0.5f);

    uint32_t id1 = 0;
    perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "pre", &id1);
    perirhinal_reset(perirhinal);

    uint32_t id2 = 0;
    int result = perirhinal_encode_object(perirhinal, features, FEATURE_DIM, "post", &id2);
    EXPECT_EQ(0, result);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
