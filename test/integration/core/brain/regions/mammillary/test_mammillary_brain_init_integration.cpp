/**
 * @file test_mammillary_brain_init_integration.cpp
 * @brief Integration tests for Mammillary Bodies with Brain Factory initialization
 *
 * WHAT: Tests Mammillary Bodies integration with brain factory lifecycle
 * WHY:  Ensure proper creation, initialization, and destruction via brain factory
 * HOW:  Test config, lifecycle, status queries, and reset functionality
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "nimcp.h"
#include "utils/logging/nimcp_logging.h"
#include "core/brain/regions/mammillary/nimcp_mammillary.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MammillaryBrainInitTest : public ::testing::Test {
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
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(MammillaryBrainInitTest, CreateWithDefaultConfig) {
    mammillary_config_t default_cfg = mammillary_default_config();
    nimcp_mammillary_t* mb = mammillary_create(&default_cfg);
    ASSERT_NE(nullptr, mb);
    mammillary_destroy(mb);
}

TEST_F(MammillaryBrainInitTest, CreateWithNullConfig) {
    nimcp_mammillary_t* mb = mammillary_create(nullptr);
    /* Should either create with defaults or fail gracefully */
    if (mb) {
        mammillary_destroy(mb);
    }
}

TEST_F(MammillaryBrainInitTest, DestroyNull) {
    /* Should not crash */
    mammillary_destroy(nullptr);
}

TEST_F(MammillaryBrainInitTest, CreateMultipleInstances) {
    mammillary_config_t cfg = mammillary_default_config();

    nimcp_mammillary_t* mb1 = mammillary_create(&cfg);
    nimcp_mammillary_t* mb2 = mammillary_create(&cfg);
    nimcp_mammillary_t* mb3 = mammillary_create(&cfg);

    ASSERT_NE(nullptr, mb1);
    ASSERT_NE(nullptr, mb2);
    ASSERT_NE(nullptr, mb3);

    mammillary_destroy(mb3);
    mammillary_destroy(mb2);
    mammillary_destroy(mb1);
}

/*=============================================================================
 * CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(MammillaryBrainInitTest, DefaultConfigValid) {
    mammillary_config_t cfg = mammillary_default_config();

    EXPECT_GT(cfg.num_hd_cells, 0u);
    EXPECT_GT(cfg.num_relay_cells, 0u);
    EXPECT_GT(cfg.consolidation_threshold, 0.0f);
}

TEST_F(MammillaryBrainInitTest, GetConfigAfterCreate) {
    mammillary_config_t retrieved;
    int result = mammillary_get_config(mammillary, &retrieved);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(retrieved.enable_papez_circuit);
    EXPECT_TRUE(retrieved.enable_head_direction);
}

TEST_F(MammillaryBrainInitTest, ConfigWithCustomHDCells) {
    mammillary_config_t custom = mammillary_default_config();
    custom.num_hd_cells = 128;

    nimcp_mammillary_t* mb = mammillary_create(&custom);
    ASSERT_NE(nullptr, mb);

    mammillary_config_t retrieved;
    mammillary_get_config(mb, &retrieved);
    EXPECT_EQ(128u, retrieved.num_hd_cells);

    mammillary_destroy(mb);
}

/*=============================================================================
 * STATUS TESTS
 *===========================================================================*/

TEST_F(MammillaryBrainInitTest, InitialStatusReady) {
    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_GE((int)status, 0);
    EXPECT_NE(status, MAMMILLARY_STATUS_ERROR);
}

TEST_F(MammillaryBrainInitTest, NoErrorAfterCreate) {
    mammillary_error_t error = mammillary_get_last_error(mammillary);
    EXPECT_EQ(MAMMILLARY_ERROR_NONE, error);
}

TEST_F(MammillaryBrainInitTest, ErrorStringNotNull) {
    const char* str = mammillary_error_string(MAMMILLARY_ERROR_NONE);
    EXPECT_NE(nullptr, str);

    str = mammillary_error_string(MAMMILLARY_ERROR_RELAY_FAILED);
    EXPECT_NE(nullptr, str);
}

TEST_F(MammillaryBrainInitTest, StatusStringNotNull) {
    const char* str = mammillary_status_string(MAMMILLARY_STATUS_IDLE);
    EXPECT_NE(nullptr, str);

    str = mammillary_status_string(MAMMILLARY_STATUS_RELAYING);
    EXPECT_NE(nullptr, str);
}

/*=============================================================================
 * RESET TESTS
 *===========================================================================*/

TEST_F(MammillaryBrainInitTest, ResetSucceeds) {
    int result = mammillary_reset(mammillary);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBrainInitTest, StatusAfterReset) {
    mammillary_reset(mammillary);

    mammillary_status_t status = mammillary_get_status(mammillary);
    EXPECT_GE((int)status, 0);
    EXPECT_NE(status, MAMMILLARY_STATUS_ERROR);
}

TEST_F(MammillaryBrainInitTest, OperationsAfterReset) {
    mammillary_reset(mammillary);

    /* Should be able to perform operations after reset */
    float position[3] = {1.0f, 2.0f, 0.0f};
    float context[128];
    for (int i = 0; i < 128; i++) context[i] = 0.1f;

    uint32_t trace_id = 0;
    int result = mammillary_encode_spatial_memory(mammillary, position, 0.0f,
        context, 128, &trace_id);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * UPDATE TESTS
 *===========================================================================*/

TEST_F(MammillaryBrainInitTest, BasicUpdate) {
    int result = mammillary_update(mammillary, 10.0f);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBrainInitTest, MultipleUpdates) {
    for (int i = 0; i < 100; i++) {
        int result = mammillary_update(mammillary, 1.0f);
        EXPECT_EQ(0, result);
    }
}

TEST_F(MammillaryBrainInitTest, BidirectionalUpdate) {
    int result = mammillary_bidirectional_update(mammillary, 10.0f);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(MammillaryBrainInitTest, GetStatsAfterCreate) {
    mammillary_stats_t stats;
    int result = mammillary_get_stats(mammillary, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(MammillaryBrainInitTest, StatsTrackUpdates) {
    for (int i = 0; i < 10; i++) {
        mammillary_update(mammillary, 10.0f);
    }

    mammillary_stats_t stats;
    mammillary_get_stats(mammillary, &stats);
    EXPECT_GE(stats.updates_processed, 10u);
}

TEST_F(MammillaryBrainInitTest, DiagnosticsRun) {
    int result = mammillary_log_diagnostics(mammillary);
    EXPECT_EQ(0, result);
}

/*=============================================================================
 * HEALTH STATUS TESTS
 *===========================================================================*/

TEST_F(MammillaryBrainInitTest, HealthStatusAfterCreate) {
    float health = mammillary_get_health_status(mammillary);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(MammillaryBrainInitTest, CircuitIntegrityAfterCreate) {
    float integrity = mammillary_get_circuit_integrity(mammillary);
    EXPECT_GE(integrity, 0.0f);
    EXPECT_LE(integrity, 1.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
