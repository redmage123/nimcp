/**
 * @file test_entorhinal_core.cpp
 * @brief Unit tests for Entorhinal Cortex core functionality
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

class EntorhinalCoreTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, CreateWithDefaultConfig) {
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
    EXPECT_EQ(entorhinal_get_last_error(ec), ENTORHINAL_ERROR_NONE);
}

TEST_F(EntorhinalCoreTest, CreateWithNullConfig) {
    nimcp_entorhinal_t* ec2 = entorhinal_create(nullptr);
    ASSERT_NE(ec2, nullptr);
    EXPECT_EQ(entorhinal_get_status(ec2), ENTORHINAL_STATUS_READY);
    entorhinal_destroy(ec2);
}

TEST_F(EntorhinalCoreTest, CreateWithCustomConfig) {
    entorhinal_config_t config = entorhinal_default_config();
    config.num_grid_cells = 256;
    config.num_border_cells = 64;
    config.num_hd_cells = 30;

    nimcp_entorhinal_t* ec2 = entorhinal_create(&config);
    ASSERT_NE(ec2, nullptr);

    entorhinal_config_t retrieved_config;
    EXPECT_EQ(entorhinal_get_config(ec2, &retrieved_config), 0);
    EXPECT_EQ(retrieved_config.num_grid_cells, 256u);
    EXPECT_EQ(retrieved_config.num_border_cells, 64u);
    EXPECT_EQ(retrieved_config.num_hd_cells, 30u);

    entorhinal_destroy(ec2);
}

TEST_F(EntorhinalCoreTest, DestroyNull) {
    // Should not crash
    entorhinal_destroy(nullptr);
}

TEST_F(EntorhinalCoreTest, Reset) {
    // Make some changes
    float position[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    // Reset
    EXPECT_TRUE(entorhinal_reset(ec));
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_IDLE);
}

TEST_F(EntorhinalCoreTest, ResetNull) {
    EXPECT_FALSE(entorhinal_reset(nullptr));
}

/*=============================================================================
 * DEFAULT CONFIG TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, DefaultConfigValues) {
    entorhinal_config_t config = entorhinal_default_config();

    EXPECT_EQ(config.num_grid_cells, ENTORHINAL_DEFAULT_GRID_CELLS);
    EXPECT_EQ(config.num_border_cells, ENTORHINAL_DEFAULT_BORDER_CELLS);
    EXPECT_EQ(config.num_hd_cells, ENTORHINAL_DEFAULT_HD_CELLS);
    EXPECT_EQ(config.num_grid_modules, GRID_MODULE_COUNT);
    EXPECT_FLOAT_EQ(config.min_grid_spacing, ENTORHINAL_MIN_GRID_SPACING);
    EXPECT_FLOAT_EQ(config.max_grid_spacing, ENTORHINAL_MAX_GRID_SPACING);
    EXPECT_TRUE(config.enable_path_integration);
    EXPECT_TRUE(config.enable_boundary_detection);
    EXPECT_TRUE(config.enable_security);
    EXPECT_TRUE(config.enable_immune);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_snn);
    EXPECT_TRUE(config.enable_plasticity);
    EXPECT_TRUE(config.enable_cognitive);
    EXPECT_TRUE(config.enable_hippocampus);
}

/*=============================================================================
 * STATUS AND ERROR TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, GetStatusNull) {
    EXPECT_EQ(entorhinal_get_status(nullptr), ENTORHINAL_STATUS_ERROR);
}

TEST_F(EntorhinalCoreTest, GetLastErrorNull) {
    EXPECT_EQ(entorhinal_get_last_error(nullptr), ENTORHINAL_ERROR_INTERNAL);
}

TEST_F(EntorhinalCoreTest, ErrorStringMapping) {
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_NONE), "No error");
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_GRID_DRIFT), "Grid drift");
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_PATH_INTEGRATION_FAILURE), "Path integration failure");
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_MEMORY_GATEWAY_BLOCKED), "Memory gateway blocked");
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_SECURITY_VIOLATION), "Security violation");
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_IMMUNE_REJECTION), "Immune rejection");
    EXPECT_STREQ(entorhinal_error_string(ENTORHINAL_ERROR_SUBSTRATE_DEPLETED), "Substrate depleted");
}

TEST_F(EntorhinalCoreTest, StatusStringMapping) {
    EXPECT_STREQ(entorhinal_status_string(ENTORHINAL_STATUS_IDLE), "Idle");
    EXPECT_STREQ(entorhinal_status_string(ENTORHINAL_STATUS_PATH_INTEGRATING), "Path integrating");
    EXPECT_STREQ(entorhinal_status_string(ENTORHINAL_STATUS_ENCODING), "Encoding");
    EXPECT_STREQ(entorhinal_status_string(ENTORHINAL_STATUS_RETRIEVING), "Retrieving");
    EXPECT_STREQ(entorhinal_status_string(ENTORHINAL_STATUS_READY), "Ready");
    EXPECT_STREQ(entorhinal_status_string(ENTORHINAL_STATUS_ERROR), "Error");
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, GetStatsInitial) {
    entorhinal_stats_t stats;
    EXPECT_EQ(entorhinal_get_stats(ec, &stats), 0);

    EXPECT_EQ(stats.updates_processed, 0u);
    EXPECT_EQ(stats.position_updates, 0u);
    EXPECT_EQ(stats.memory_encodings, 0u);
    EXPECT_EQ(stats.memory_retrievals, 0u);
}

TEST_F(EntorhinalCoreTest, GetStatsAfterUpdates) {
    float position[3] = {1.0f, 2.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    entorhinal_stats_t stats;
    EXPECT_EQ(entorhinal_get_stats(ec, &stats), 0);
    EXPECT_EQ(stats.position_updates, 1u);
}

TEST_F(EntorhinalCoreTest, GetStatsNull) {
    EXPECT_EQ(entorhinal_get_stats(nullptr, nullptr), -1);
    entorhinal_stats_t stats;
    EXPECT_EQ(entorhinal_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(entorhinal_get_stats(ec, nullptr), -1);
}

/*=============================================================================
 * HEALTH STATUS TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, GetHealthStatusInitial) {
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(EntorhinalCoreTest, GetHealthStatusNull) {
    EXPECT_FLOAT_EQ(entorhinal_get_health_status(nullptr), 0.0f);
}

/*=============================================================================
 * CONFIGURATION RETRIEVAL TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, GetConfigSuccess) {
    entorhinal_config_t config;
    EXPECT_EQ(entorhinal_get_config(ec, &config), 0);
    EXPECT_EQ(config.num_grid_cells, ENTORHINAL_DEFAULT_GRID_CELLS);
}

TEST_F(EntorhinalCoreTest, GetConfigNull) {
    EXPECT_EQ(entorhinal_get_config(nullptr, nullptr), -1);
    entorhinal_config_t config;
    EXPECT_EQ(entorhinal_get_config(nullptr, &config), -1);
    EXPECT_EQ(entorhinal_get_config(ec, nullptr), -1);
}

/*=============================================================================
 * SERIALIZATION SIZE TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, GetSerializationSize) {
    size_t size = entorhinal_get_serialization_size(ec);
    EXPECT_GT(size, 0u);
}

TEST_F(EntorhinalCoreTest, GetSerializationSizeNull) {
    EXPECT_EQ(entorhinal_get_serialization_size(nullptr), 0u);
}

/*=============================================================================
 * TRAINING MODE TESTS
 *===========================================================================*/

TEST_F(EntorhinalCoreTest, SetTrainingModeEnable) {
    EXPECT_EQ(entorhinal_set_training_mode(ec, true), 0);
}

TEST_F(EntorhinalCoreTest, SetTrainingModeDisable) {
    EXPECT_EQ(entorhinal_set_training_mode(ec, false), 0);
}

TEST_F(EntorhinalCoreTest, SetTrainingModeNull) {
    EXPECT_EQ(entorhinal_set_training_mode(nullptr, true), -1);
}

TEST_F(EntorhinalCoreTest, GetTrainingLoss) {
    float loss = entorhinal_get_training_loss(ec);
    EXPECT_GE(loss, 0.0f);
}

TEST_F(EntorhinalCoreTest, GetTrainingLossNull) {
    EXPECT_FLOAT_EQ(entorhinal_get_training_loss(nullptr), 0.0f);
}
