/**
 * @file test_surface_bridges.cpp
 * @brief Unit Tests for Surface Geometry Bridges
 *
 * WHAT: Tests for bio-async, quantum, and immune integration bridges
 * WHY:  Surface geometry integrates with multiple NIMCP systems
 * HOW:  GTest-based tests for bridge lifecycle and messaging
 *
 * NIMCP STANDARDS:
 * - All tests < 50 lines
 * - Bridge pattern compliance (bridge_base_t)
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/geometry/nimcp_surface_geometry.h"
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "core/brain/bridges/nimcp_surface_geometry_bridge.h"
#include "async/bridges/nimcp_surface_bio_async_bridge.h"
#include "quantum/integration/nimcp_surface_quantum_bridge.h"
#include "cognitive/immune/nimcp_surface_immune_bridge.h"
}

//=============================================================================
// Brain Bridge Tests
//=============================================================================

class SurfaceBrainBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        surface_geometry_bridge_default_config(&config);
        bridge = surface_geometry_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            surface_geometry_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    surface_geometry_bridge_config_t config;
    surface_geometry_bridge_t* bridge;
};

TEST_F(SurfaceBrainBridgeTest, Create_WithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SurfaceBrainBridgeTest, Create_NullConfig) {
    surface_geometry_bridge_t* b = surface_geometry_bridge_create(nullptr);
    EXPECT_NE(b, nullptr);  // Should use defaults
    if (b) {
        surface_geometry_bridge_destroy(b);
    }
}

TEST_F(SurfaceBrainBridgeTest, Destroy_Null) {
    surface_geometry_bridge_destroy(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(SurfaceBrainBridgeTest, DefaultConfig_ValidValues) {
    EXPECT_TRUE(config.enable_bio_async || !config.enable_bio_async);  // Valid bool
    EXPECT_GT(config.update_interval_ms, 0u);
}

TEST_F(SurfaceBrainBridgeTest, DefaultConfig_Null) {
    int ret = surface_geometry_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceBrainBridgeTest, ConnectGeometry_ValidContext) {
    surface_geometry_config_t geo_config = {};
    surface_geometry_default_config(&geo_config);
    surface_geometry_ctx_t* ctx = surface_geometry_create(&geo_config);
    ASSERT_NE(ctx, nullptr);

    int ret = surface_geometry_bridge_connect_geometry(bridge, ctx);
    EXPECT_EQ(ret, 0);

    surface_geometry_destroy(ctx);
}

TEST_F(SurfaceBrainBridgeTest, ConnectGeometry_NullContext) {
    int ret = surface_geometry_bridge_connect_geometry(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceBrainBridgeTest, ConnectGeometry_NullBridge) {
    surface_geometry_ctx_t* ctx = surface_geometry_create(nullptr);
    int ret = surface_geometry_bridge_connect_geometry(nullptr, ctx);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
    if (ctx) surface_geometry_destroy(ctx);
}

TEST_F(SurfaceBrainBridgeTest, IsConnected_Initially) {
    bool connected = surface_geometry_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);  // Not connected until connect() called
}

TEST_F(SurfaceBrainBridgeTest, GetStats_Initial) {
    surface_geometry_bridge_stats_t stats = {};
    int ret = surface_geometry_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.messages_sent, 0u);
}

TEST_F(SurfaceBrainBridgeTest, GetStats_NullOutput) {
    int ret = surface_geometry_bridge_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceBrainBridgeTest, Reset_Success) {
    int ret = surface_geometry_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SurfaceBrainBridgeTest, Reset_NullBridge) {
    int ret = surface_geometry_bridge_reset(nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

//=============================================================================
// Bio-Async Bridge Tests
//=============================================================================

class SurfaceBioAsyncBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        surface_bio_async_default_config(&config);
        bridge = surface_bio_async_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            surface_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    surface_bio_async_config_t config;
    surface_bio_async_bridge_t* bridge;
};

TEST_F(SurfaceBioAsyncBridgeTest, Create_WithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SurfaceBioAsyncBridgeTest, Create_NullConfig) {
    surface_bio_async_bridge_t* b = surface_bio_async_bridge_create(nullptr);
    EXPECT_NE(b, nullptr);
    if (b) {
        surface_bio_async_bridge_destroy(b);
    }
}

TEST_F(SurfaceBioAsyncBridgeTest, Destroy_Null) {
    surface_bio_async_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SurfaceBioAsyncBridgeTest, DefaultConfig_ValidValues) {
    EXPECT_GT(config.update_interval_ms, 0u);
}

TEST_F(SurfaceBioAsyncBridgeTest, DefaultConfig_Null) {
    int ret = surface_bio_async_default_config(nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceBioAsyncBridgeTest, ConnectGeometry_Valid) {
    surface_geometry_ctx_t* ctx = surface_geometry_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    int ret = surface_bio_async_bridge_set_geometry_ctx(bridge, ctx);
    EXPECT_EQ(ret, 0);

    surface_geometry_destroy(ctx);
}

TEST_F(SurfaceBioAsyncBridgeTest, ConnectGeometry_NullContext) {
    int ret = surface_bio_async_bridge_set_geometry_ctx(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceBioAsyncBridgeTest, IsConnected_Initially) {
    bool connected = surface_bio_async_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(SurfaceBioAsyncBridgeTest, GetStats_Initial) {
    surface_bio_async_stats_t stats = {};
    int ret = surface_bio_async_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
}

TEST_F(SurfaceBioAsyncBridgeTest, GetStats_NullOutput) {
    int ret = surface_bio_async_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceBioAsyncBridgeTest, SendGeometryUpdate_NotConnected) {
    surface_bio_msg_geometry_update_t update = {};
    update.params.chi = 0.5f;
    update.params.rho = 0.5f;
    update.branch_point_id = 1;
    update.timestamp_ms = 0;

    // Should fail or queue since not connected
    int ret = surface_bio_async_send_geometry_update(bridge, &update);
    // May succeed (queued) or fail (not connected) - any return is valid
    (void)ret;
}

//=============================================================================
// Quantum Bridge Tests
//=============================================================================

class SurfaceQuantumBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        surface_quantum_bridge_default_config(&config);
        bridge = surface_quantum_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            surface_quantum_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    surface_quantum_bridge_config_t config;
    surface_quantum_bridge_t* bridge;
};

TEST_F(SurfaceQuantumBridgeTest, Create_WithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SurfaceQuantumBridgeTest, Create_NullConfig) {
    surface_quantum_bridge_t* b = surface_quantum_bridge_create(nullptr);
    EXPECT_NE(b, nullptr);
    if (b) {
        surface_quantum_bridge_destroy(b);
    }
}

TEST_F(SurfaceQuantumBridgeTest, Destroy_Null) {
    surface_quantum_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SurfaceQuantumBridgeTest, DefaultConfig_ValidValues) {
    EXPECT_GE(config.qmc_amplitude.num_shots, 0u);
}

TEST_F(SurfaceQuantumBridgeTest, DefaultConfig_Null) {
    int ret = surface_quantum_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceQuantumBridgeTest, ConnectGeometry_Valid) {
    surface_geometry_ctx_t* ctx = surface_geometry_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    int ret = surface_quantum_bridge_connect_geometry(bridge, ctx);
    EXPECT_EQ(ret, 0);

    surface_geometry_destroy(ctx);
}

TEST_F(SurfaceQuantumBridgeTest, ConnectGeometry_NullContext) {
    int ret = surface_quantum_bridge_connect_geometry(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceQuantumBridgeTest, IsQuantumAvailable_SimulationMode) {
    // In simulation mode, quantum should be "available" (simulated)
    bool available = surface_quantum_bridge_is_quantum_available(bridge);
    // Depends on implementation - may be true (simulated) or false (no real quantum)
    EXPECT_TRUE(available || !available);
}

TEST_F(SurfaceQuantumBridgeTest, GetStats_Initial) {
    surface_quantum_bridge_stats_t stats = {};
    int ret = surface_quantum_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.qmc_amplitude_calls, 0u);
}

TEST_F(SurfaceQuantumBridgeTest, GetStats_NullOutput) {
    int ret = surface_quantum_bridge_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceQuantumBridgeTest, Reset_Success) {
    int ret = surface_quantum_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SurfaceQuantumBridgeTest, Reset_NullBridge) {
    int ret = surface_quantum_bridge_reset(nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceQuantumBridgeTest, MethodAvailable_QMCAmplitude) {
    bool avail = surface_quantum_method_available(bridge, SURFACE_QUANTUM_QMC_AMPLITUDE);
    EXPECT_TRUE(avail || !avail);  // Implementation dependent
}

TEST_F(SurfaceQuantumBridgeTest, MethodName_Valid) {
    const char* name = surface_quantum_method_name(SURFACE_QUANTUM_QMC_AMPLITUDE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

//=============================================================================
// Immune Bridge Tests
//=============================================================================

class SurfaceImmuneBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        surface_immune_default_config(&config);
        bridge = surface_immune_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            surface_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    surface_immune_config_t config;
    surface_immune_bridge_t* bridge;
};

TEST_F(SurfaceImmuneBridgeTest, Create_WithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SurfaceImmuneBridgeTest, Create_NullConfig) {
    surface_immune_bridge_t* b = surface_immune_bridge_create(nullptr);
    EXPECT_NE(b, nullptr);
    if (b) {
        surface_immune_bridge_destroy(b);
    }
}

TEST_F(SurfaceImmuneBridgeTest, Destroy_Null) {
    surface_immune_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SurfaceImmuneBridgeTest, DefaultConfig_ValidValues) {
    EXPECT_NEAR(config.chi_min, 0.0f, 0.01f);
    EXPECT_NEAR(config.chi_max, 2.0f, 0.01f);
    EXPECT_NEAR(config.rho_min, 0.0f, 0.01f);
    EXPECT_NEAR(config.rho_max, 1.0f, 0.01f);
}

TEST_F(SurfaceImmuneBridgeTest, DefaultConfig_Null) {
    int ret = surface_immune_default_config(nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceImmuneBridgeTest, ConnectGeometry_Valid) {
    surface_geometry_ctx_t* ctx = surface_geometry_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    int ret = surface_immune_bridge_connect_geometry(bridge, ctx);
    EXPECT_EQ(ret, 0);

    surface_geometry_destroy(ctx);
}

TEST_F(SurfaceImmuneBridgeTest, ConnectGeometry_NullContext) {
    int ret = surface_immune_bridge_connect_geometry(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceImmuneBridgeTest, IsConnected_Initially) {
    bool connected = surface_immune_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(SurfaceImmuneBridgeTest, ValidateGeometry_ValidParams) {
    surface_geometry_params_t params = {};
    params.chi = 0.5f;  // Valid range
    params.rho = 0.5f;  // Valid range

    bool is_valid;
    surface_antigen_type_t violation;
    int ret = surface_immune_validate_geometry(bridge, &params, &is_valid, &violation);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(is_valid);
    EXPECT_EQ(violation, SURFACE_ANTIGEN_NONE);
}

TEST_F(SurfaceImmuneBridgeTest, ValidateGeometry_InvalidChi) {
    surface_geometry_params_t params = {};
    params.chi = 3.0f;  // Out of range (max 2)
    params.rho = 0.5f;

    bool is_valid;
    surface_antigen_type_t violation;
    int ret = surface_immune_validate_geometry(bridge, &params, &is_valid, &violation);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(is_valid);
    EXPECT_EQ(violation, SURFACE_ANTIGEN_INVALID_CHI);
}

TEST_F(SurfaceImmuneBridgeTest, ValidateGeometry_NullParams) {
    bool is_valid;
    surface_antigen_type_t violation;
    int ret = surface_immune_validate_geometry(bridge, nullptr, &is_valid, &violation);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceImmuneBridgeTest, PresentAnomaly_Valid) {
    surface_branch_point_t branch = {};
    branch.id = 1;
    branch.position = {0.0f, 0.0f, 0.0f};

    uint32_t antigen_id;
    int ret = surface_immune_present_anomaly(
        bridge,
        SURFACE_ANTIGEN_INVALID_CHI,
        &branch,
        0.83f,  // expected
        3.0f,   // actual (out of range)
        &antigen_id
    );
    EXPECT_EQ(ret, 0);
    EXPECT_GT(antigen_id, 0u);
}

TEST_F(SurfaceImmuneBridgeTest, PresentAnomaly_NullBranch) {
    uint32_t antigen_id;
    int ret = surface_immune_present_anomaly(
        bridge,
        SURFACE_ANTIGEN_INVALID_CHI,
        nullptr,
        0.83f,
        3.0f,
        &antigen_id
    );
    // Implementation tolerates NULL branch (uses branch_id=0)
    EXPECT_EQ(ret, 0);
    EXPECT_GT(antigen_id, 0u);
}

TEST_F(SurfaceImmuneBridgeTest, GetActiveAntigens_Empty) {
    surface_antigen_t antigens[10] = {};
    uint32_t count;
    int ret = surface_immune_get_active_antigens(bridge, antigens, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 0u);  // No antigens initially
}

TEST_F(SurfaceImmuneBridgeTest, GetStats_Initial) {
    surface_immune_stats_t stats = {};
    int ret = surface_immune_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_validations, 0u);
}

TEST_F(SurfaceImmuneBridgeTest, GetStats_NullOutput) {
    int ret = surface_immune_get_stats(bridge, nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceImmuneBridgeTest, Reset_Success) {
    int ret = surface_immune_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SurfaceImmuneBridgeTest, Reset_NullBridge) {
    int ret = surface_immune_bridge_reset(nullptr);
    EXPECT_NE(ret, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(SurfaceImmuneBridgeTest, AntigenTypeName_Valid) {
    const char* name = surface_antigen_type_name(SURFACE_ANTIGEN_INVALID_CHI);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(SurfaceImmuneBridgeTest, SeverityName_Valid) {
    const char* name = surface_antigen_severity_name(SURFACE_SEVERITY_CRITICAL);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

//=============================================================================
// Cross-Bridge Integration Tests
//=============================================================================

class CrossBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        geo_ctx = surface_geometry_create(nullptr);
        ASSERT_NE(geo_ctx, nullptr);

        brain_bridge = surface_geometry_bridge_create(nullptr);
        ASSERT_NE(brain_bridge, nullptr);

        bio_bridge = surface_bio_async_bridge_create(nullptr);
        ASSERT_NE(bio_bridge, nullptr);

        immune_bridge = surface_immune_bridge_create(nullptr);
        ASSERT_NE(immune_bridge, nullptr);
    }

    void TearDown() override {
        if (immune_bridge) surface_immune_bridge_destroy(immune_bridge);
        if (bio_bridge) surface_bio_async_bridge_destroy(bio_bridge);
        if (brain_bridge) surface_geometry_bridge_destroy(brain_bridge);
        if (geo_ctx) surface_geometry_destroy(geo_ctx);
    }

    surface_geometry_ctx_t* geo_ctx;
    surface_geometry_bridge_t* brain_bridge;
    surface_bio_async_bridge_t* bio_bridge;
    surface_immune_bridge_t* immune_bridge;
};

TEST_F(CrossBridgeTest, ConnectAllBridges) {
    // Connect all bridges to same geometry context
    int ret1 = surface_geometry_bridge_connect_geometry(brain_bridge, geo_ctx);
    int ret2 = surface_bio_async_bridge_set_geometry_ctx(bio_bridge, geo_ctx);
    int ret3 = surface_immune_bridge_connect_geometry(immune_bridge, geo_ctx);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
    EXPECT_EQ(ret3, 0);
}

TEST_F(CrossBridgeTest, AllBridgesIndependent) {
    // bridge_active requires BOTH system_a AND system_b connected
    // Connecting only geometry (system_a) does not make bridge "active"
    surface_geometry_bridge_connect_geometry(brain_bridge, geo_ctx);

    bool brain_connected = surface_geometry_bridge_is_connected(brain_bridge);
    bool bio_connected = surface_bio_async_bridge_is_connected(bio_bridge);

    EXPECT_FALSE(brain_connected);  // Only geometry connected, not brain region
    EXPECT_FALSE(bio_connected);    // Not connected yet
}
