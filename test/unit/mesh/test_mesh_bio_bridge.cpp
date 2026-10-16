/**
 * @file test_mesh_bio_bridge.cpp
 * @brief Unit tests for mesh bio bridge (Phase 14)
 *
 * Tests bio-async to mesh translation and pattern extraction.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MeshBioBridgeTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_bio_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create bootstrap with minimal config */
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems.enable_cognitive = false;
        config.subsystems.enable_sensory = false;
        config.subsystems.enable_motor = false;
        config.subsystems.enable_memory = false;
        config.subsystems.enable_security = false;
        config.subsystems.enable_gpu = false;
        config.subsystems.enable_plasticity = false;
        config.subsystems.enable_glial = false;
        config.subsystems.enable_swarm = false;
        config.subsystems.enable_async = false;
        config.subsystems.enable_lnn = false;
        config.subsystems.enable_snn = false;

        bootstrap = mesh_bootstrap_create(&config);
        if (bootstrap) {
            bridge = mesh_bootstrap_get_bio_bridge(bootstrap);
        }
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
            bridge = nullptr;  /* Destroyed with bootstrap */
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST(MeshBioBridgeConfigTest, DefaultConfig) {
    mesh_bio_bridge_config_t config;
    nimcp_error_t err = mesh_bio_bridge_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(config.enable_pattern_routing);
    EXPECT_TRUE(config.enable_channel_mapping);
    EXPECT_GT(config.default_timeout_ms, 0.0f);
}

TEST(MeshBioBridgeConfigTest, DefaultConfigNullPointer) {
    nimcp_error_t err = mesh_bio_bridge_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshBioBridgeTest, BridgeCreatedWithBootstrap) {
    /* Bio bridge should be created automatically when bootstrap creates it */
    EXPECT_NE(bridge, nullptr);
}

TEST_F(MeshBioBridgeTest, IsConnectedReturnsFalseInitially) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }
    /* Bridge is not connected to a router initially */
    EXPECT_FALSE(mesh_bio_bridge_is_connected(bridge));
}

/* ============================================================================
 * Channel Mapping Tests
 * ============================================================================ */

TEST_F(MeshBioBridgeTest, NeuralCategoryMapsToSubcortical) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }
    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_NEURAL);
    /* Neural by default goes to subcortical */
    EXPECT_EQ(channel, MESH_CHANNEL_SUBCORTICAL);
}

TEST_F(MeshBioBridgeTest, CognitiveCategoryMapsToLeftHemisphere) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }
    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_COGNITIVE);
    EXPECT_EQ(channel, MESH_CHANNEL_LEFT_HEMISPHERE);
}

TEST_F(MeshBioBridgeTest, PerceptionCategoryMapsToRightHemisphere) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }
    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_PERCEPTION);
    EXPECT_EQ(channel, MESH_CHANNEL_RIGHT_HEMISPHERE);
}

TEST_F(MeshBioBridgeTest, SecurityCategoryMapsToSystem) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }
    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_SECURITY);
    EXPECT_EQ(channel, MESH_CHANNEL_SYSTEM);
}

TEST_F(MeshBioBridgeTest, SetChannelMapping) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    /* Override default mapping */
    nimcp_error_t err = mesh_bio_bridge_set_channel_mapping(
        bridge, MESH_BIO_CAT_NEURAL, MESH_CHANNEL_GPU_COMPUTE);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify new mapping */
    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_NEURAL);
    EXPECT_EQ(channel, MESH_CHANNEL_GPU_COMPUTE);
}

/* ============================================================================
 * Pattern Range Tests
 * ============================================================================ */

TEST(MeshBioBridgePatternRangeTest, NeuralPatternRange) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(MESH_BIO_CAT_NEURAL, &range);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(range.start, 0u);
    EXPECT_EQ(range.end, 8u);
}

TEST(MeshBioBridgePatternRangeTest, PlasticityPatternRange) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(MESH_BIO_CAT_PLASTICITY, &range);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(range.start, 8u);
    EXPECT_EQ(range.end, 16u);
}

TEST(MeshBioBridgePatternRangeTest, CognitivePatternRange) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(MESH_BIO_CAT_COGNITIVE, &range);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(range.start, 32u);
    EXPECT_EQ(range.end, 40u);
}

TEST(MeshBioBridgePatternRangeTest, InvalidCategoryReturnsNotFound) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(0xFFFF, &range);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshBioBridgeTest, GetStats) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_bio_bridge_stats_t stats;
    nimcp_error_t err = mesh_bio_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.bio_messages_received, 0u);  /* No messages processed yet */
}

TEST_F(MeshBioBridgeTest, ResetStats) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    nimcp_error_t err = mesh_bio_bridge_reset_stats(bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_bio_bridge_stats_t stats;
    mesh_bio_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.bio_messages_received, 0u);
}

/* ============================================================================
 * Pattern Dimension Index Tests
 * ============================================================================ */

TEST(MeshPatternDimensionTest, NeuralDimensionsRange) {
    /* Neural dimensions should be 0-7 */
    EXPECT_EQ(MESH_DIM_NEURAL_ACTIVATION, 0u);
    EXPECT_LT(MESH_DIM_NEURAL_ACTIVATION, 8u);
}

TEST(MeshPatternDimensionTest, PlasticityDimensionsRange) {
    /* Plasticity dimensions should be 8-15 */
    EXPECT_EQ(MESH_DIM_PLASTICITY_STDP, 8u);
    EXPECT_LT(MESH_DIM_PLASTICITY_STDP, 16u);
}

TEST(MeshPatternDimensionTest, CognitiveDimensionsRange) {
    /* Cognitive dimensions should be 32-39 */
    EXPECT_EQ(MESH_DIM_COGNITIVE_REASONING, 32u);
    EXPECT_LT(MESH_DIM_COGNITIVE_REASONING, 40u);
}

TEST(MeshPatternDimensionTest, PerceptionDimensionsRange) {
    /* Perception dimensions should be 24-31 */
    EXPECT_EQ(MESH_DIM_PERCEPTION_VISUAL, 24u);
    EXPECT_LT(MESH_DIM_PERCEPTION_VISUAL, 32u);
}

TEST(MeshPatternDimensionTest, MotorDimensionsRange) {
    /* Motor dimensions should be 40-47 */
    EXPECT_EQ(MESH_DIM_MOTOR_COMMAND, 40u);
    EXPECT_LT(MESH_DIM_MOTOR_COMMAND, 48u);
}

TEST(MeshPatternDimensionTest, SecurityDimensionsRange) {
    /* Security dimensions should be 48-55 */
    EXPECT_EQ(MESH_DIM_SECURITY_THREAT, 48u);
    EXPECT_LT(MESH_DIM_SECURITY_THREAT, 56u);
}
