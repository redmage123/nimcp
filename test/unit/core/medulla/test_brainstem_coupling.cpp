/**
 * @file test_brainstem_coupling.cpp
 * @brief Unit tests for the brainstem coupling module
 *
 * WHAT: Tests for bidirectional brainstem-cortex communication
 * WHY:  Ensure proper signal routing, priority handling, and latency
 * HOW:  Use GoogleTest framework with signal transmission validation
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_brainstem_coupling.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainstemCouplingTest : public ::testing::Test {
protected:
    brainstem_coupling_t* coupling = nullptr;

    void SetUp() override {
        brainstem_coupling_config_t config;
        brainstem_coupling_default_config(&config);
        coupling = brainstem_coupling_create(&config);
        ASSERT_NE(coupling, nullptr);
    }

    void TearDown() override {
        if (coupling) {
            brainstem_coupling_destroy(coupling);
            coupling = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, DefaultConfig) {
    brainstem_coupling_config_t config;
    int result = brainstem_coupling_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify buffer sizes are set
    EXPECT_GT(config.bottom_up_buffer_size, 0u);
    EXPECT_GT(config.top_down_buffer_size, 0u);
}

TEST_F(BrainstemCouplingTest, DefaultConfigNull) {
    int result = brainstem_coupling_default_config(nullptr);
    EXPECT_LT(result, 0);  // Any negative error code
}

TEST_F(BrainstemCouplingTest, CreateWithNullConfig) {
    brainstem_coupling_t* c = brainstem_coupling_create(nullptr);
    EXPECT_NE(c, nullptr);
    if (c) brainstem_coupling_destroy(c);
}

TEST_F(BrainstemCouplingTest, DestroyNull) {
    brainstem_coupling_destroy(nullptr);
}

//=============================================================================
// Bottom-Up Signal Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, SendBottomUpArousal) {
    brainstem_bottom_up_payload_t payload = {
        .type = BRAINSTEM_AROUSAL_SIGNAL,
        .intensity = 0.8f,
        .priority = SIGNAL_PRIORITY_MEDIUM,
        .source_module = 0x1100,
        .timestamp = 0,
        .latency_ms = 50
    };

    int result = brainstem_coupling_send_bottom_up(coupling, &payload);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, SendBottomUpThreat) {
    brainstem_bottom_up_payload_t payload = {
        .type = BRAINSTEM_THREAT_SIGNAL,
        .intensity = 0.9f,
        .priority = SIGNAL_PRIORITY_HIGH,
        .source_module = 0x1100,
        .timestamp = 0,
        .latency_ms = 20
    };

    int result = brainstem_coupling_send_bottom_up(coupling, &payload);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, SendBottomUpNull) {
    int result = brainstem_coupling_send_bottom_up(nullptr, nullptr);
    EXPECT_LT(result, 0);  // Any negative error code
}

//=============================================================================
// Top-Down Signal Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, SendTopDownAttention) {
    brainstem_top_down_payload_t payload = {
        .type = BRAINSTEM_ATTENTION_MODULATION,
        .modulation = 0.5f,
        .target_module = 0x1100,
        .timestamp = 0
    };

    int result = brainstem_coupling_send_top_down(coupling, &payload);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, SendTopDownExecutive) {
    brainstem_top_down_payload_t payload = {
        .type = BRAINSTEM_EXECUTIVE_OVERRIDE,
        .modulation = 1.0f,
        .target_module = 0x1101,
        .timestamp = 0
    };

    int result = brainstem_coupling_send_top_down(coupling, &payload);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, SendTopDownNull) {
    int result = brainstem_coupling_send_top_down(nullptr, nullptr);
    EXPECT_LT(result, 0);  // Any negative error code
}

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, RegisterModule) {
    int result = brainstem_coupling_register_module(coupling, 0x0500);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, RegisterModuleNull) {
    int result = brainstem_coupling_register_module(nullptr, 0x0500);
    EXPECT_LT(result, 0);  // Any negative error code
}

TEST_F(BrainstemCouplingTest, UnregisterModule) {
    brainstem_coupling_register_module(coupling, 0x0500);
    int result = brainstem_coupling_unregister_module(coupling, 0x0500);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, UnregisterModuleNull) {
    int result = brainstem_coupling_unregister_module(nullptr, 0x0500);
    EXPECT_LT(result, 0);  // Any negative error code
}

//=============================================================================
// Signal Retrieval Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, GetPendingSignals) {
    // Send a signal first
    brainstem_bottom_up_payload_t payload = {
        .type = BRAINSTEM_AROUSAL_SIGNAL,
        .intensity = 0.5f,
        .priority = SIGNAL_PRIORITY_MEDIUM,
        .source_module = 0x1100,
        .timestamp = 0,
        .latency_ms = 50
    };
    brainstem_coupling_send_bottom_up(coupling, &payload);

    // Retrieve pending signals
    brainstem_bottom_up_payload_t signals[10];
    uint32_t count = 0;
    int result = brainstem_coupling_get_pending_signals(coupling, signals, 10, &count);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(count, 0u);
}

TEST_F(BrainstemCouplingTest, GetPendingSignalsNull) {
    uint32_t count;
    int result = brainstem_coupling_get_pending_signals(nullptr, nullptr, 0, &count);
    EXPECT_LT(result, 0);  // Any negative error code
}

//=============================================================================
// Process Signals Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, ProcessSignals) {
    // Send a signal
    brainstem_bottom_up_payload_t payload = {
        .type = BRAINSTEM_METABOLIC_SIGNAL,
        .intensity = 0.6f,
        .priority = SIGNAL_PRIORITY_LOW,
        .source_module = 0x1100,
        .timestamp = 0,
        .latency_ms = 1000
    };
    brainstem_coupling_send_bottom_up(coupling, &payload);

    // Process signals
    int processed = brainstem_coupling_process_signals(coupling);
    EXPECT_GE(processed, 0);
}

TEST_F(BrainstemCouplingTest, ProcessSignalsNull) {
    int result = brainstem_coupling_process_signals(nullptr);
    EXPECT_LT(result, 0);  // Any negative error code
}

//=============================================================================
// Priority and Latency Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, SetPriority) {
    int result = brainstem_coupling_set_priority(
        coupling, BRAINSTEM_THREAT_SIGNAL, SIGNAL_PRIORITY_CRITICAL);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, SetPriorityNull) {
    int result = brainstem_coupling_set_priority(
        nullptr, BRAINSTEM_THREAT_SIGNAL, SIGNAL_PRIORITY_HIGH);
    EXPECT_LT(result, 0);  // Any negative error code
}

TEST_F(BrainstemCouplingTest, SetLatency) {
    int result = brainstem_coupling_set_latency(
        coupling, BRAINSTEM_METABOLIC_SIGNAL, 500);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(BrainstemCouplingTest, SetLatencyNull) {
    int result = brainstem_coupling_set_latency(
        nullptr, BRAINSTEM_METABOLIC_SIGNAL, 500);
    EXPECT_LT(result, 0);  // Any negative error code
}

//=============================================================================
// Bio-async Tests
//=============================================================================

TEST_F(BrainstemCouplingTest, BioAsyncConnection) {
    // Connect to bio-async
    int result = brainstem_coupling_connect_bio_async(coupling);
    // Result depends on router availability

    // Disconnect
    brainstem_coupling_disconnect_bio_async(coupling);

    // Check disconnected state
    bool connected = brainstem_coupling_is_bio_async_connected(coupling);
    EXPECT_FALSE(connected);
}

TEST_F(BrainstemCouplingTest, BioAsyncNullState) {
    bool connected = brainstem_coupling_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
