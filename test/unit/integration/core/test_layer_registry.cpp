/**
 * @file test_layer_registry.cpp
 * @brief Unit tests for Layer Registry
 *
 * WHAT: Test suite for nimcp_layer_registry
 * WHY:  Verify correct registration and lookup of layers and modules
 * HOW:  Unit tests for create, register, lookup, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/core/nimcp_layer_registry.h"
#include "integration/core/nimcp_layer_types.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class LayerRegistryTest : public ::testing::Test {
protected:
    nimcp_layer_registry_t registry = nullptr;

    void SetUp() override {
        nimcp_layer_registry_config_t config = nimcp_layer_registry_default_config();
        registry = nimcp_layer_registry_create(&config);
        ASSERT_NE(registry, nullptr);
    }

    void TearDown() override {
        if (registry) {
            nimcp_layer_registry_destroy(registry);
            registry = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST(LayerRegistryCreateTest, CreateWithDefaultConfig) {
    nimcp_layer_registry_t reg = nimcp_layer_registry_create(nullptr);
    ASSERT_NE(reg, nullptr);
    nimcp_layer_registry_destroy(reg);
}

TEST(LayerRegistryCreateTest, CreateWithCustomConfig) {
    nimcp_layer_registry_config_t config = {
        .max_layers = 16,
        .max_modules_per_layer = 32,
        .enable_logging = true,
        .thread_safe = true
    };
    nimcp_layer_registry_t reg = nimcp_layer_registry_create(&config);
    ASSERT_NE(reg, nullptr);
    nimcp_layer_registry_destroy(reg);
}

TEST(LayerRegistryCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_layer_registry_destroy(nullptr);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(LayerRegistryTest, ResetSuccess) {
    nimcp_layer_error_t err = nimcp_layer_registry_reset(registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerRegistryTest, ResetNull) {
    nimcp_layer_error_t err = nimcp_layer_registry_reset(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Layer Registration Tests
//=============================================================================

TEST_F(LayerRegistryTest, RegisterLayer) {
    nimcp_layer_config_t config = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_error_t err = nimcp_layer_registry_register_layer(registry, &config);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerRegistryTest, RegisterLayerNull) {
    nimcp_layer_error_t err = nimcp_layer_registry_register_layer(nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(LayerRegistryTest, IsLayerRegistered) {
    nimcp_layer_config_t config = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_registry_register_layer(registry, &config);

    EXPECT_TRUE(nimcp_layer_registry_is_layer_registered(registry, NIMCP_LAYER_PHYSICS));
    EXPECT_FALSE(nimcp_layer_registry_is_layer_registered(registry, NIMCP_LAYER_CHEMISTRY));
}

//=============================================================================
// Lookup Tests
//=============================================================================

TEST_F(LayerRegistryTest, GetLayerConfig) {
    nimcp_layer_config_t config = nimcp_layer_default_config(NIMCP_LAYER_CHEMISTRY);
    nimcp_layer_registry_register_layer(registry, &config);

    nimcp_layer_config_t retrieved;
    nimcp_layer_error_t err = nimcp_layer_registry_get_layer_config(registry, NIMCP_LAYER_CHEMISTRY, &retrieved);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(retrieved.layer_id, NIMCP_LAYER_CHEMISTRY);
}

TEST_F(LayerRegistryTest, GetLayerCount) {
    int count = nimcp_layer_registry_get_layer_count(registry);
    EXPECT_EQ(count, 0);

    nimcp_layer_config_t config = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_registry_register_layer(registry, &config);

    count = nimcp_layer_registry_get_layer_count(registry);
    EXPECT_EQ(count, 1);
}

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(LayerRegistryTest, RegisterModule) {
    /* First register the layer */
    nimcp_layer_config_t layer_config = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_registry_register_layer(registry, &layer_config);

    /* Create a mock module interface */
    nimcp_module_interface_t iface = {0};

    /* Register a module */
    uint32_t module_id = 0;
    nimcp_layer_error_t err = nimcp_layer_registry_register_module(
        registry, NIMCP_LAYER_PHYSICS, (void*)0x1234, &iface, "test_module", &module_id
    );
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerRegistryTest, GetModuleCount) {
    nimcp_layer_config_t config = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_registry_register_layer(registry, &config);

    int count = nimcp_layer_registry_get_module_count(registry, NIMCP_LAYER_PHYSICS);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(LayerRegistryTest, RegisterConnection) {
    /* Register two layers */
    nimcp_layer_config_t config1 = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_config_t config2 = nimcp_layer_default_config(NIMCP_LAYER_CHEMISTRY);
    nimcp_layer_registry_register_layer(registry, &config1);
    nimcp_layer_registry_register_layer(registry, &config2);

    /* Create and register connection */
    nimcp_layer_connection_t conn = {
        .layer_a = NIMCP_LAYER_PHYSICS,
        .layer_b = NIMCP_LAYER_CHEMISTRY,
        .bidirectional = true,
        .bottom_up_enabled = true,
        .top_down_enabled = true,
        .coupling_strength = 1.0f,
        .queue_depth = 64
    };
    nimcp_layer_error_t err = nimcp_layer_registry_register_connection(registry, &conn);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerRegistryTest, AreConnected) {
    /* Register two layers */
    nimcp_layer_config_t config1 = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_config_t config2 = nimcp_layer_default_config(NIMCP_LAYER_CHEMISTRY);
    nimcp_layer_registry_register_layer(registry, &config1);
    nimcp_layer_registry_register_layer(registry, &config2);

    /* Create and register connection */
    nimcp_layer_connection_t conn = {
        .layer_a = NIMCP_LAYER_PHYSICS,
        .layer_b = NIMCP_LAYER_CHEMISTRY,
        .bidirectional = true,
        .bottom_up_enabled = true,
        .top_down_enabled = true,
        .coupling_strength = 1.0f,
        .queue_depth = 64
    };
    nimcp_layer_registry_register_connection(registry, &conn);

    EXPECT_TRUE(nimcp_layer_registry_are_connected(registry, NIMCP_LAYER_PHYSICS, NIMCP_LAYER_CHEMISTRY));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
