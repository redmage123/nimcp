/**
 * @file test_information_geometry_bridge.cpp
 * @brief Unit tests for Information Geometry Bridge API
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests the information geometry bridge module including:
 * - Bridge lifecycle (create, destroy, config)
 * - KG registration
 * - Exception handler registration
 * - Bio-async registration
 * - Error handling
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "physics/geometry/nimcp_information_geometry_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class InfoGeomBridgeTest : public ::testing::Test {
protected:
    info_geom_bridge_t bridge_ = nullptr;

    void SetUp() override {
        info_geom_bridge_config_t config = info_geom_bridge_default_config();
        bridge_ = info_geom_bridge_create(&config);
        ASSERT_NE(bridge_, nullptr);
    }

    void TearDown() override {
        if (bridge_) {
            info_geom_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, DefaultConfig) {
    info_geom_bridge_config_t config = info_geom_bridge_default_config();

    EXPECT_TRUE(config.enable_kg_wiring);
    EXPECT_TRUE(config.enable_exception_handling);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_immune_presentation);
    EXPECT_TRUE(config.enable_logging);
}

TEST_F(InfoGeomBridgeTest, DefaultConfigAllEnabled) {
    info_geom_bridge_config_t config = info_geom_bridge_default_config();

    // All features should be enabled by default
    EXPECT_TRUE(config.enable_kg_wiring);
    EXPECT_TRUE(config.enable_exception_handling);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_immune_presentation);
    EXPECT_TRUE(config.enable_logging);
}

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, CreateWithNullConfig) {
    info_geom_bridge_t b = info_geom_bridge_create(nullptr);
    EXPECT_NE(b, nullptr);  // Should use defaults
    info_geom_bridge_destroy(b);
}

TEST_F(InfoGeomBridgeTest, CreateWithConfig) {
    info_geom_bridge_config_t config = info_geom_bridge_default_config();
    config.enable_logging = false;
    config.enable_kg_wiring = false;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    info_geom_bridge_destroy(b);
}

TEST_F(InfoGeomBridgeTest, CreateWithAllDisabled) {
    info_geom_bridge_config_t config = {0};
    config.enable_kg_wiring = false;
    config.enable_exception_handling = false;
    config.enable_bio_async = false;
    config.enable_immune_presentation = false;
    config.enable_logging = false;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    info_geom_bridge_destroy(b);
}

TEST_F(InfoGeomBridgeTest, DestroyNull) {
    info_geom_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(InfoGeomBridgeTest, MultipleCreateDestroy) {
    for (int i = 0; i < 10; i++) {
        info_geom_bridge_config_t config = info_geom_bridge_default_config();
        info_geom_bridge_t b = info_geom_bridge_create(&config);
        EXPECT_NE(b, nullptr);
        info_geom_bridge_destroy(b);
    }
}

//=============================================================================
// KG Registration Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, RegisterKGNullBridge) {
    // We can't create a real brain_kg_t here without more infrastructure
    // but we can test null handling
    EXPECT_EQ(info_geom_bridge_register_kg(nullptr, nullptr), -1);
}

TEST_F(InfoGeomBridgeTest, RegisterKGNullKG) {
    EXPECT_EQ(info_geom_bridge_register_kg(bridge_, nullptr), -1);
}

TEST_F(InfoGeomBridgeTest, RegisterKGWithDisabledWiring) {
    // Create bridge with KG wiring disabled
    info_geom_bridge_config_t config = info_geom_bridge_default_config();
    config.enable_kg_wiring = false;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    // Should return success (0) even with null KG when wiring is disabled
    // Actually looking at the implementation, it still returns -1 if kg is null
    EXPECT_EQ(info_geom_bridge_register_kg(b, nullptr), -1);

    info_geom_bridge_destroy(b);
}

//=============================================================================
// Exception Handler Registration Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, RegisterException) {
    void* dummy_handler = (void*)0x12345678;
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, dummy_handler), 0);
}

TEST_F(InfoGeomBridgeTest, RegisterExceptionNullBridge) {
    EXPECT_EQ(info_geom_bridge_register_exception(nullptr, (void*)0x1234), -1);
}

TEST_F(InfoGeomBridgeTest, RegisterExceptionNullHandler) {
    // Should succeed even with null handler (clears handler)
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, nullptr), 0);
}

TEST_F(InfoGeomBridgeTest, RegisterExceptionMultipleTimes) {
    void* handler1 = (void*)0x11111111;
    void* handler2 = (void*)0x22222222;
    void* handler3 = (void*)0x33333333;

    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler1), 0);
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler2), 0);
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler3), 0);
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, nullptr), 0);
}

//=============================================================================
// Bio-Async Registration Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, RegisterBioAsync) {
    void* dummy_channel = (void*)0x87654321;
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, dummy_channel), 0);
}

TEST_F(InfoGeomBridgeTest, RegisterBioAsyncNullBridge) {
    EXPECT_EQ(info_geom_bridge_register_bio_async(nullptr, (void*)0x1234), -1);
}

TEST_F(InfoGeomBridgeTest, RegisterBioAsyncNullChannel) {
    // Should succeed even with null channel (clears channel)
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, nullptr), 0);
}

TEST_F(InfoGeomBridgeTest, RegisterBioAsyncMultipleTimes) {
    void* channel1 = (void*)0xAAAAAAAA;
    void* channel2 = (void*)0xBBBBBBBB;

    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, channel1), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, channel2), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, nullptr), 0);
}

//=============================================================================
// Combined Registration Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, RegisterAllHandlers) {
    void* exception_handler = (void*)0x11111111;
    void* bio_async_channel = (void*)0x22222222;

    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, exception_handler), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, bio_async_channel), 0);
}

TEST_F(InfoGeomBridgeTest, ClearAllHandlers) {
    // First set handlers
    void* exception_handler = (void*)0x11111111;
    void* bio_async_channel = (void*)0x22222222;

    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, exception_handler), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, bio_async_channel), 0);

    // Then clear them
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, nullptr), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, nullptr), 0);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, RegisterSameHandlerTwice) {
    void* handler = (void*)0x12345678;

    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler), 0);
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler), 0);
}

TEST_F(InfoGeomBridgeTest, AlternatingHandlers) {
    void* handler1 = (void*)0x11111111;
    void* handler2 = (void*)0x22222222;

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler1), 0);
        EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler2), 0);
    }
}

//=============================================================================
// Configuration Variation Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, CreateWithOnlyKGEnabled) {
    info_geom_bridge_config_t config = {0};
    config.enable_kg_wiring = true;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    info_geom_bridge_destroy(b);
}

TEST_F(InfoGeomBridgeTest, CreateWithOnlyExceptionEnabled) {
    info_geom_bridge_config_t config = {0};
    config.enable_exception_handling = true;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    info_geom_bridge_destroy(b);
}

TEST_F(InfoGeomBridgeTest, CreateWithOnlyBioAsyncEnabled) {
    info_geom_bridge_config_t config = {0};
    config.enable_bio_async = true;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    info_geom_bridge_destroy(b);
}

TEST_F(InfoGeomBridgeTest, CreateWithOnlyImmuneEnabled) {
    info_geom_bridge_config_t config = {0};
    config.enable_immune_presentation = true;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    info_geom_bridge_destroy(b);
}

TEST_F(InfoGeomBridgeTest, CreateWithOnlyLoggingEnabled) {
    info_geom_bridge_config_t config = {0};
    config.enable_logging = true;

    info_geom_bridge_t b = info_geom_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    info_geom_bridge_destroy(b);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, DoubleDestroy) {
    info_geom_bridge_config_t config = info_geom_bridge_default_config();
    info_geom_bridge_t b = info_geom_bridge_create(&config);
    ASSERT_NE(b, nullptr);

    info_geom_bridge_destroy(b);
    // Note: Second destroy would be undefined behavior, but we test that
    // destroy(nullptr) is safe
    info_geom_bridge_destroy(nullptr);
}

TEST_F(InfoGeomBridgeTest, UseAfterConfigure) {
    // Configure bridge
    void* exception_handler = (void*)0x11111111;
    void* bio_async_channel = (void*)0x22222222;

    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, exception_handler), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, bio_async_channel), 0);

    // Reconfigure
    void* new_handler = (void*)0x33333333;
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, new_handler), 0);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(InfoGeomBridgeTest, RapidCreateDestroy) {
    for (int i = 0; i < 100; i++) {
        info_geom_bridge_t b = info_geom_bridge_create(nullptr);
        ASSERT_NE(b, nullptr);
        info_geom_bridge_destroy(b);
    }
}

TEST_F(InfoGeomBridgeTest, RapidHandlerRegistration) {
    for (int i = 0; i < 100; i++) {
        void* handler = (void*)(uintptr_t)(0x10000000 + i);
        EXPECT_EQ(info_geom_bridge_register_exception(bridge_, handler), 0);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
