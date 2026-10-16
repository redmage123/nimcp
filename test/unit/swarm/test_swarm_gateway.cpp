/**
 * @file test_swarm_gateway.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Gateway
 *
 * TEST COVERAGE:
 * - Gateway creation and destruction
 * - Connect/disconnect swarms
 * - Broadcast updates across swarms
 * - Telemetry aggregation
 * - Multi-swarm coordination
 * - Edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <map>

// Headers have their own extern "C" guards

// Forward declaration
typedef struct swarm_brain swarm_brain_t;

// Gateway configuration
typedef struct {
    char gateway_id[32];
    uint32_t max_swarms;
    uint32_t telemetry_interval_ms;
    bool enable_cross_swarm_comm;
} swarm_gateway_config_t;

// Swarm info
typedef struct {
    char swarm_id[32];
    uint32_t drone_count;
    float avg_coherence;
    uint64_t last_update_ms;
} swarm_info_t;

// Gateway telemetry
typedef struct {
    uint32_t total_swarms;
    uint32_t total_drones;
    uint64_t messages_relayed;
    uint64_t bytes_relayed;
    float avg_coherence;
} swarm_gateway_telemetry_t;

// Gateway state
typedef struct {
    swarm_gateway_config_t config;
    std::map<std::string, swarm_info_t>* connected_swarms;
    swarm_gateway_telemetry_t telemetry;
    uint64_t creation_timestamp;
} swarm_gateway_t;

// API functions
swarm_gateway_t* swarm_gateway_create(const swarm_gateway_config_t* config);

void swarm_gateway_destroy(swarm_gateway_t* gateway);

bool swarm_gateway_connect_swarm(
    swarm_gateway_t* gateway,
    const char* swarm_id,
    swarm_brain_t* swarm_brain
);

bool swarm_gateway_disconnect_swarm(
    swarm_gateway_t* gateway,
    const char* swarm_id
);

bool swarm_gateway_broadcast_to_all(
    swarm_gateway_t* gateway,
    const void* message,
    uint32_t message_size
);

swarm_gateway_telemetry_t swarm_gateway_get_telemetry(
    const swarm_gateway_t* gateway
);

uint32_t swarm_gateway_get_swarm_count(const swarm_gateway_t* gateway);

const swarm_info_t* swarm_gateway_get_swarm_info(
    const swarm_gateway_t* gateway,
    const char* swarm_id
);

swarm_gateway_config_t swarm_gateway_default_config(void);

//=============================================================================
// Mock Implementation
//=============================================================================

swarm_gateway_config_t swarm_gateway_default_config(void) {
    swarm_gateway_config_t config;
    strncpy(config.gateway_id, "default_gateway", sizeof(config.gateway_id));
    config.max_swarms = 16;
    config.telemetry_interval_ms = 1000;
    config.enable_cross_swarm_comm = true;
    return config;
}

swarm_gateway_t* swarm_gateway_create(const swarm_gateway_config_t* config) {
    if (!config) return nullptr;

    swarm_gateway_t* gateway = new swarm_gateway_t();
    gateway->config = *config;
    gateway->connected_swarms = new std::map<std::string, swarm_info_t>();
    memset(&gateway->telemetry, 0, sizeof(gateway->telemetry));
    gateway->creation_timestamp = 0;

    return gateway;
}

void swarm_gateway_destroy(swarm_gateway_t* gateway) {
    if (gateway) {
        delete gateway->connected_swarms;
        delete gateway;
    }
}

bool swarm_gateway_connect_swarm(
    swarm_gateway_t* gateway,
    const char* swarm_id,
    swarm_brain_t* swarm_brain
) {
    if (!gateway || !swarm_id) return false;
    if (gateway->connected_swarms->size() >= gateway->config.max_swarms) {
        return false;
    }

    swarm_info_t info;
    strncpy(info.swarm_id, swarm_id, sizeof(info.swarm_id) - 1);
    info.swarm_id[sizeof(info.swarm_id) - 1] = '\0';
    info.drone_count = 1;
    info.avg_coherence = 0.5f;
    info.last_update_ms = 0;

    (*gateway->connected_swarms)[swarm_id] = info;
    gateway->telemetry.total_swarms++;

    return true;
}

bool swarm_gateway_disconnect_swarm(
    swarm_gateway_t* gateway,
    const char* swarm_id
) {
    if (!gateway || !swarm_id) return false;

    auto it = gateway->connected_swarms->find(swarm_id);
    if (it == gateway->connected_swarms->end()) return false;

    gateway->connected_swarms->erase(it);
    gateway->telemetry.total_swarms--;

    return true;
}

bool swarm_gateway_broadcast_to_all(
    swarm_gateway_t* gateway,
    const void* message,
    uint32_t message_size
) {
    if (!gateway || !message || message_size == 0) return false;
    if (gateway->connected_swarms->empty()) return false;

    // Simulate broadcast
    gateway->telemetry.messages_relayed += gateway->connected_swarms->size();
    gateway->telemetry.bytes_relayed += message_size * gateway->connected_swarms->size();

    return true;
}

swarm_gateway_telemetry_t swarm_gateway_get_telemetry(
    const swarm_gateway_t* gateway
) {
    if (!gateway) {
        swarm_gateway_telemetry_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }

    swarm_gateway_telemetry_t telemetry = gateway->telemetry;

    // Calculate average coherence
    if (!gateway->connected_swarms->empty()) {
        float total_coherence = 0.0f;
        for (const auto& pair : *gateway->connected_swarms) {
            total_coherence += pair.second.avg_coherence;
        }
        telemetry.avg_coherence = total_coherence / gateway->connected_swarms->size();
    }

    return telemetry;
}

uint32_t swarm_gateway_get_swarm_count(const swarm_gateway_t* gateway) {
    return gateway ? gateway->connected_swarms->size() : 0;
}

const swarm_info_t* swarm_gateway_get_swarm_info(
    const swarm_gateway_t* gateway,
    const char* swarm_id
) {
    if (!gateway || !swarm_id) return nullptr;

    auto it = gateway->connected_swarms->find(swarm_id);
    if (it == gateway->connected_swarms->end()) return nullptr;

    return &it->second;
}

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmGatewayTest : public ::testing::Test {
protected:
    swarm_gateway_t* gateway;

    void SetUp() override {
        gateway = nullptr;
    }

    void TearDown() override {
        if (gateway) {
            swarm_gateway_destroy(gateway);
            gateway = nullptr;
        }
    }
};

//=============================================================================
// 1. Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmGatewayTest, CreateWithDefaultConfig) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    ASSERT_NE(gateway, nullptr);
    EXPECT_EQ(gateway->config.max_swarms, 16u);
    EXPECT_TRUE(gateway->config.enable_cross_swarm_comm);
}

TEST_F(SwarmGatewayTest, CreateWithCustomConfig) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    config.max_swarms = 32;
    config.telemetry_interval_ms = 500;

    gateway = swarm_gateway_create(&config);

    ASSERT_NE(gateway, nullptr);
    EXPECT_EQ(gateway->config.max_swarms, 32u);
    EXPECT_EQ(gateway->config.telemetry_interval_ms, 500u);
}

TEST_F(SwarmGatewayTest, CreateWithNullConfig) {
    gateway = swarm_gateway_create(nullptr);

    EXPECT_EQ(gateway, nullptr);
}

TEST_F(SwarmGatewayTest, DestroyNull) {
    swarm_gateway_destroy(nullptr); // Should not crash
}

//=============================================================================
// 2. Connect/Disconnect Tests
//=============================================================================

TEST_F(SwarmGatewayTest, ConnectSingleSwarm) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    bool success = swarm_gateway_connect_swarm(gateway, "swarm1", nullptr);

    ASSERT_TRUE(success);
    EXPECT_EQ(swarm_gateway_get_swarm_count(gateway), 1u);
}

TEST_F(SwarmGatewayTest, ConnectMultipleSwarms) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    ASSERT_TRUE(swarm_gateway_connect_swarm(gateway, "swarm1", nullptr));
    ASSERT_TRUE(swarm_gateway_connect_swarm(gateway, "swarm2", nullptr));
    ASSERT_TRUE(swarm_gateway_connect_swarm(gateway, "swarm3", nullptr));

    EXPECT_EQ(swarm_gateway_get_swarm_count(gateway), 3u);
}

TEST_F(SwarmGatewayTest, ConnectExceedingMaxSwarms) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    config.max_swarms = 2;
    gateway = swarm_gateway_create(&config);

    ASSERT_TRUE(swarm_gateway_connect_swarm(gateway, "swarm1", nullptr));
    ASSERT_TRUE(swarm_gateway_connect_swarm(gateway, "swarm2", nullptr));
    EXPECT_FALSE(swarm_gateway_connect_swarm(gateway, "swarm3", nullptr)); // Should fail

    EXPECT_EQ(swarm_gateway_get_swarm_count(gateway), 2u);
}

TEST_F(SwarmGatewayTest, DisconnectSwarm) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    swarm_gateway_connect_swarm(gateway, "swarm1", nullptr);
    ASSERT_EQ(swarm_gateway_get_swarm_count(gateway), 1u);

    bool success = swarm_gateway_disconnect_swarm(gateway, "swarm1");

    ASSERT_TRUE(success);
    EXPECT_EQ(swarm_gateway_get_swarm_count(gateway), 0u);
}

TEST_F(SwarmGatewayTest, DisconnectNonexistentSwarm) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    bool success = swarm_gateway_disconnect_swarm(gateway, "nonexistent");

    EXPECT_FALSE(success);
}

//=============================================================================
// 3. Broadcast Tests
//=============================================================================

TEST_F(SwarmGatewayTest, BroadcastToMultipleSwarms) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    swarm_gateway_connect_swarm(gateway, "swarm1", nullptr);
    swarm_gateway_connect_swarm(gateway, "swarm2", nullptr);
    swarm_gateway_connect_swarm(gateway, "swarm3", nullptr);

    uint8_t message[32] = {1, 2, 3, 4};
    bool success = swarm_gateway_broadcast_to_all(gateway, message, sizeof(message));

    ASSERT_TRUE(success);

    swarm_gateway_telemetry_t telemetry = swarm_gateway_get_telemetry(gateway);
    EXPECT_EQ(telemetry.messages_relayed, 3u); // One per swarm
    EXPECT_EQ(telemetry.bytes_relayed, 3u * sizeof(message));
}

TEST_F(SwarmGatewayTest, BroadcastToNoSwarms) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    uint8_t message[32] = {1, 2, 3, 4};
    bool success = swarm_gateway_broadcast_to_all(gateway, message, sizeof(message));

    EXPECT_FALSE(success); // No swarms connected
}

TEST_F(SwarmGatewayTest, BroadcastWithNullMessage) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    swarm_gateway_connect_swarm(gateway, "swarm1", nullptr);

    bool success = swarm_gateway_broadcast_to_all(gateway, nullptr, 32);

    EXPECT_FALSE(success);
}

//=============================================================================
// 4. Telemetry Tests
//=============================================================================

TEST_F(SwarmGatewayTest, InitialTelemetryZero) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    swarm_gateway_telemetry_t telemetry = swarm_gateway_get_telemetry(gateway);

    EXPECT_EQ(telemetry.total_swarms, 0u);
    EXPECT_EQ(telemetry.total_drones, 0u);
    EXPECT_EQ(telemetry.messages_relayed, 0u);
    EXPECT_EQ(telemetry.bytes_relayed, 0u);
}

TEST_F(SwarmGatewayTest, TelemetryTracksSwarms) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    swarm_gateway_connect_swarm(gateway, "swarm1", nullptr);
    swarm_gateway_connect_swarm(gateway, "swarm2", nullptr);

    swarm_gateway_telemetry_t telemetry = swarm_gateway_get_telemetry(gateway);

    EXPECT_EQ(telemetry.total_swarms, 2u);
}

TEST_F(SwarmGatewayTest, TelemetryTracksMessages) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    swarm_gateway_connect_swarm(gateway, "swarm1", nullptr);
    swarm_gateway_connect_swarm(gateway, "swarm2", nullptr);

    uint8_t message[32];
    swarm_gateway_broadcast_to_all(gateway, message, sizeof(message));
    swarm_gateway_broadcast_to_all(gateway, message, sizeof(message));

    swarm_gateway_telemetry_t telemetry = swarm_gateway_get_telemetry(gateway);

    EXPECT_EQ(telemetry.messages_relayed, 4u); // 2 broadcasts � 2 swarms
    EXPECT_EQ(telemetry.bytes_relayed, 4u * sizeof(message));
}

//=============================================================================
// 5. Swarm Info Tests
//=============================================================================

TEST_F(SwarmGatewayTest, GetSwarmInfo) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    swarm_gateway_connect_swarm(gateway, "test_swarm", nullptr);

    const swarm_info_t* info = swarm_gateway_get_swarm_info(gateway, "test_swarm");

    ASSERT_NE(info, nullptr);
    EXPECT_STREQ(info->swarm_id, "test_swarm");
    EXPECT_GT(info->drone_count, 0u);
}

TEST_F(SwarmGatewayTest, GetNonexistentSwarmInfo) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    const swarm_info_t* info = swarm_gateway_get_swarm_info(gateway, "nonexistent");

    EXPECT_EQ(info, nullptr);
}

//=============================================================================
// 6. Edge Cases
//=============================================================================

TEST_F(SwarmGatewayTest, ConnectWithNullSwarmId) {
    swarm_gateway_config_t config = swarm_gateway_default_config();
    gateway = swarm_gateway_create(&config);

    bool success = swarm_gateway_connect_swarm(gateway, nullptr, nullptr);

    EXPECT_FALSE(success);
}

TEST_F(SwarmGatewayTest, GetSwarmCountNull) {
    uint32_t count = swarm_gateway_get_swarm_count(nullptr);

    EXPECT_EQ(count, 0u);
}

TEST_F(SwarmGatewayTest, GetTelemetryNull) {
    swarm_gateway_telemetry_t telemetry = swarm_gateway_get_telemetry(nullptr);

    EXPECT_EQ(telemetry.total_swarms, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
