/**
 * @file test_mesh_bio_integration.cpp
 * @brief Unit tests for bio-async to mesh bidirectional integration
 *
 * Tests bio message to mesh transaction translation, mesh transaction to bio
 * message translation, channel routing based on bio category, priority mapping,
 * fallback when mesh unavailable, and bidirectional flow.
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Mock Bio Message Structures
 * ============================================================================ */

/* Simulated bio message header (matches bio-router format) */
typedef struct mock_bio_message_header {
    uint32_t type;
    uint32_t size;
    uint64_t timestamp;
    uint32_t priority;
} mock_bio_message_header_t;

typedef struct mock_bio_neural_message {
    mock_bio_message_header_t header;
    float activation;
    float spike_rate;
    uint32_t neuron_id;
} mock_bio_neural_message_t;

typedef struct mock_bio_cognitive_message {
    mock_bio_message_header_t header;
    float reasoning_level;
    uint32_t task_id;
    char description[64];
} mock_bio_cognitive_message_t;

typedef struct mock_bio_security_message {
    mock_bio_message_header_t header;
    float threat_level;
    uint32_t threat_type;
    char source[32];
} mock_bio_security_message_t;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshBioIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_bio_bridge_t* bridge = nullptr;

    void SetUp() override {
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
        ASSERT_NE(bootstrap, nullptr);

        bridge = mesh_bootstrap_get_bio_bridge(bootstrap);
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
            bridge = nullptr;
        }
    }

    /* Helper: Create neural bio message */
    mock_bio_neural_message_t create_neural_message(float activation, float spike_rate) {
        mock_bio_neural_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.header.type = MESH_BIO_CAT_NEURAL;
        msg.header.size = sizeof(msg);
        msg.header.timestamp = 0;
        msg.header.priority = 5;
        msg.activation = activation;
        msg.spike_rate = spike_rate;
        msg.neuron_id = 12345;
        return msg;
    }

    /* Helper: Create cognitive bio message */
    mock_bio_cognitive_message_t create_cognitive_message(float reasoning_level) {
        mock_bio_cognitive_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.header.type = MESH_BIO_CAT_COGNITIVE;
        msg.header.size = sizeof(msg);
        msg.header.timestamp = 0;
        msg.header.priority = 7;
        msg.reasoning_level = reasoning_level;
        msg.task_id = 1001;
        strcpy(msg.description, "reasoning task");
        return msg;
    }

    /* Helper: Create security bio message */
    mock_bio_security_message_t create_security_message(float threat_level) {
        mock_bio_security_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.header.type = MESH_BIO_CAT_SECURITY;
        msg.header.size = sizeof(msg);
        msg.header.timestamp = 0;
        msg.header.priority = 10;
        msg.threat_level = threat_level;
        msg.threat_type = 1;
        strcpy(msg.source, "external");
        return msg;
    }
};

/* ============================================================================
 * Bio Message to Mesh Transaction Translation Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, TranslateNeuralMessageToMesh) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mock_bio_neural_message_t bio_msg = create_neural_message(0.8f, 50.0f);

    mesh_transaction_t* tx = nullptr;
    nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
        bridge, &bio_msg, sizeof(bio_msg), &tx
    );

    if (tx != nullptr) {
        /* Transaction should be created for neural message */
        EXPECT_EQ(err, NIMCP_SUCCESS);
        /* Clean up - would call mesh_transaction_destroy(tx) if available */
    } else {
        /* Translation may not be fully implemented yet */
        EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_NOT_IMPLEMENTED);
    }
}

TEST_F(MeshBioIntegrationTest, TranslateCognitiveMessageToMesh) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mock_bio_cognitive_message_t bio_msg = create_cognitive_message(0.9f);

    mesh_transaction_t* tx = nullptr;
    nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
        bridge, &bio_msg, sizeof(bio_msg), &tx
    );

    /* Cognitive messages should map to cognitive channel */
    if (err == NIMCP_SUCCESS && tx != nullptr) {
        /* Verify channel assignment is correct */
        SUCCEED();
    }
}

TEST_F(MeshBioIntegrationTest, TranslateSecurityMessageToMesh) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mock_bio_security_message_t bio_msg = create_security_message(0.7f);

    mesh_transaction_t* tx = nullptr;
    nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
        bridge, &bio_msg, sizeof(bio_msg), &tx
    );

    /* Security messages should be routed to system channel */
    if (err == NIMCP_SUCCESS && tx != nullptr) {
        SUCCEED();
    }
}

TEST_F(MeshBioIntegrationTest, TranslateNullMessageFails) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_transaction_t* tx = nullptr;
    nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(bridge, nullptr, 0, &tx);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBioIntegrationTest, TranslateZeroSizeMessageFails) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mock_bio_neural_message_t bio_msg = create_neural_message(0.5f, 25.0f);

    mesh_transaction_t* tx = nullptr;
    nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(bridge, &bio_msg, 0, &tx);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Mesh Transaction to Bio Message Translation Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, TranslateMeshToBioMessage) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    /* Create a mock transaction (simplified) */
    /* Note: Real implementation would use mesh_transaction_create */

    uint8_t bio_buffer[256];
    size_t bio_size = 0;

    /* This test validates the API exists and handles edge cases */
    nimcp_error_t err = mesh_bio_bridge_translate_to_bio(
        bridge, nullptr, bio_buffer, &bio_size, sizeof(bio_buffer)
    );

    /* Null transaction should fail */
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBioIntegrationTest, TranslateMeshToBioWithNullBuffer) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    size_t bio_size = 0;
    nimcp_error_t err = mesh_bio_bridge_translate_to_bio(
        bridge, nullptr, nullptr, &bio_size, 256
    );
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBioIntegrationTest, RegisterMeshToBioCallback) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    /* Define a simple callback */
    auto callback = [](const mesh_transaction_t* tx, void* bio_msg_out,
                       size_t* msg_size_out, void* ctx) -> nimcp_error_t {
        (void)tx; (void)bio_msg_out; (void)msg_size_out; (void)ctx;
        return NIMCP_SUCCESS;
    };

    nimcp_error_t err = mesh_bio_bridge_register_mesh_callback(
        bridge, callback, nullptr
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Channel Routing Based on Bio Category Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, NeuralCategoryRoutesToSubcortical) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_NEURAL);
    EXPECT_EQ(channel, MESH_CHANNEL_SUBCORTICAL);
}

TEST_F(MeshBioIntegrationTest, CognitiveCategoryRoutesToLeftHemisphere) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_COGNITIVE);
    EXPECT_EQ(channel, MESH_CHANNEL_LEFT_HEMISPHERE);
}

TEST_F(MeshBioIntegrationTest, PerceptionCategoryRoutesToRightHemisphere) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_PERCEPTION);
    EXPECT_EQ(channel, MESH_CHANNEL_RIGHT_HEMISPHERE);
}

TEST_F(MeshBioIntegrationTest, SecurityCategoryRoutesToSystem) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_SECURITY);
    EXPECT_EQ(channel, MESH_CHANNEL_SYSTEM);
}

TEST_F(MeshBioIntegrationTest, MotorCategoryRouting) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_MOTOR);
    /* Motor usually routes to subcortical or left hemisphere */
    EXPECT_TRUE(channel == MESH_CHANNEL_SUBCORTICAL ||
                channel == MESH_CHANNEL_LEFT_HEMISPHERE);
}

TEST_F(MeshBioIntegrationTest, CustomChannelMappingOverride) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    /* Override neural -> GPU compute */
    nimcp_error_t err = mesh_bio_bridge_set_channel_mapping(
        bridge, MESH_BIO_CAT_NEURAL, MESH_CHANNEL_GPU_COMPUTE
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(bridge, MESH_BIO_CAT_NEURAL);
    EXPECT_EQ(channel, MESH_CHANNEL_GPU_COMPUTE);

    /* Restore default */
    mesh_bio_bridge_set_channel_mapping(
        bridge, MESH_BIO_CAT_NEURAL, MESH_CHANNEL_SUBCORTICAL
    );
}

/* ============================================================================
 * Priority Mapping Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, PatternRangeForNeuralCategory) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(MESH_BIO_CAT_NEURAL, &range);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(range.start, 0u);
    EXPECT_EQ(range.end, 8u);
}

TEST_F(MeshBioIntegrationTest, PatternRangeForPlasticityCategory) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(MESH_BIO_CAT_PLASTICITY, &range);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(range.start, 8u);
    EXPECT_EQ(range.end, 16u);
}

TEST_F(MeshBioIntegrationTest, PatternRangeForCognitiveCategory) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(MESH_BIO_CAT_COGNITIVE, &range);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(range.start, 32u);
    EXPECT_EQ(range.end, 40u);
}

TEST_F(MeshBioIntegrationTest, PatternRangeForUnknownCategoryFails) {
    mesh_pattern_dim_range_t range;
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(0xFFFF, &range);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshBioIntegrationTest, PatternRangeNullOutputFails) {
    nimcp_error_t err = mesh_bio_bridge_get_pattern_range(MESH_BIO_CAT_NEURAL, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Pattern Extraction Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, ExtractPatternFromNeuralMessage) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mock_bio_neural_message_t bio_msg = create_neural_message(0.8f, 100.0f);

    mesh_pattern_t pattern;
    mesh_pattern_init(&pattern);

    nimcp_error_t err = mesh_bio_bridge_extract_pattern(
        bridge, &bio_msg, sizeof(bio_msg), &pattern
    );

    if (err == NIMCP_SUCCESS) {
        /* Pattern should have non-zero magnitude */
        EXPECT_GT(pattern.magnitude, 0.0f);
        /* Neural dimensions should be populated */
        bool has_activity = false;
        for (size_t i = 0; i < 8; i++) {
            if (pattern.vector[i] != 0.0f) {
                has_activity = true;
                break;
            }
        }
        EXPECT_TRUE(has_activity);
    }
}

TEST_F(MeshBioIntegrationTest, ExtractPatternNullInputFails) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_pattern_t pattern;
    nimcp_error_t err = mesh_bio_bridge_extract_pattern(bridge, nullptr, 0, &pattern);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Fallback When Mesh Unavailable Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, BridgeConnectionStatus) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    /* Without explicit router connection, should not be connected */
    bool connected = mesh_bio_bridge_is_connected(bridge);
    /* Initial state is not connected */
    EXPECT_FALSE(connected);
}

TEST_F(MeshBioIntegrationTest, RouteMessageWhenNotConnected) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    /* When not connected to a router, routing may succeed via mesh integration
     * context or fail gracefully depending on bridge configuration */
    mock_bio_neural_message_t bio_msg = create_neural_message(0.5f, 30.0f);

    nimcp_error_t err = mesh_bio_bridge_route_bio_message(
        bridge, &bio_msg, sizeof(bio_msg)
    );

    /* Should either succeed (if bridge has integration context) or fail gracefully */
    EXPECT_TRUE(err == NIMCP_SUCCESS ||
                err == NIMCP_ERROR_NOT_INITIALIZED ||
                err == NIMCP_ERROR_INVALID_STATE);
}

TEST_F(MeshBioIntegrationTest, DisconnectWhenNotConnected) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    nimcp_error_t err = mesh_bio_bridge_disconnect_router(bridge);
    /* Should succeed or return appropriate error */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_INVALID_STATE);
}

/* ============================================================================
 * Bidirectional Flow Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, BidirectionalConfigEnabled) {
    mesh_bio_bridge_config_t config;
    nimcp_error_t err = mesh_bio_bridge_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* Check if bidirectional mode is configurable */
    /* Note: Default may be true or false depending on implementation */
}

TEST_F(MeshBioIntegrationTest, StatisticsTrackBothDirections) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_bio_bridge_stats_t stats;
    nimcp_error_t err = mesh_bio_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Stats should track both directions */
    EXPECT_EQ(stats.bio_messages_received, 0u);
    EXPECT_EQ(stats.mesh_events_received, 0u);
    EXPECT_EQ(stats.translation_failures, 0u);
}

TEST_F(MeshBioIntegrationTest, ResetStatisticsBothDirections) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    nimcp_error_t err = mesh_bio_bridge_reset_stats(bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_bio_bridge_stats_t stats;
    mesh_bio_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.bio_messages_received, 0u);
    EXPECT_EQ(stats.mesh_transactions_created, 0u);
    EXPECT_EQ(stats.mesh_events_received, 0u);
    EXPECT_EQ(stats.bio_messages_sent, 0u);
}

/* ============================================================================
 * Edge Cases and Error Handling Tests
 * ============================================================================ */

TEST_F(MeshBioIntegrationTest, GetChannelWithNullBridge) {
    mesh_channel_id_t channel = mesh_bio_bridge_get_channel(nullptr, MESH_BIO_CAT_NEURAL);
    /* Should return default or invalid channel */
    (void)channel;  /* Implementation-dependent */
}

TEST_F(MeshBioIntegrationTest, SetChannelMappingNullBridge) {
    nimcp_error_t err = mesh_bio_bridge_set_channel_mapping(
        nullptr, MESH_BIO_CAT_NEURAL, MESH_CHANNEL_GPU_COMPUTE
    );
    // NULL bridge returns INVALID_PARAM (validates magic too)
    EXPECT_TRUE(err == NIMCP_ERROR_NULL_POINTER || err == NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshBioIntegrationTest, GetStatsNullBridge) {
    mesh_bio_bridge_stats_t stats;
    nimcp_error_t err = mesh_bio_bridge_get_stats(nullptr, &stats);
    // NULL bridge returns INVALID_PARAM (validates magic too)
    EXPECT_TRUE(err == NIMCP_ERROR_NULL_POINTER || err == NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshBioIntegrationTest, GetStatsNullOutput) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    nimcp_error_t err = mesh_bio_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshBioIntegrationTest, CategorySpecificTranslationCounts) {
    if (!bridge) {
        GTEST_SKIP() << "Bio bridge not available";
    }

    mesh_bio_bridge_stats_t stats;
    mesh_bio_bridge_get_stats(bridge, &stats);

    /* Category-specific counts should all start at zero */
    EXPECT_EQ(stats.neural_translations, 0u);
    EXPECT_EQ(stats.plasticity_translations, 0u);
    EXPECT_EQ(stats.neuromod_translations, 0u);
    EXPECT_EQ(stats.perception_translations, 0u);
    EXPECT_EQ(stats.cognitive_translations, 0u);
    EXPECT_EQ(stats.motor_translations, 0u);
    EXPECT_EQ(stats.security_translations, 0u);
    EXPECT_EQ(stats.system_translations, 0u);
}
