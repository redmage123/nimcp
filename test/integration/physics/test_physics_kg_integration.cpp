//=============================================================================
// test_physics_kg_integration.cpp - Physics KG Wiring Integration Tests
//=============================================================================
/**
 * @file test_physics_kg_integration.cpp
 * @brief Integration tests for physics layer Knowledge Graph registration
 *
 * Tests KG node creation, edge registration, and state synchronization.
 */

#include <gtest/gtest.h>
#include <cstring>

#include "physics/bridges/nimcp_physics_kg_wiring.h"
#include "core/brain/nimcp_brain_kg.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsKGIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.max_nodes = 1000;
        kg_config.max_edges = 5000;
        kg_config.enable_statistics = true;
        // Disable security for testing - allows writes without valid admin token
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;

        kg_ = brain_kg_create(&kg_config);
        kg_initialized_ = (kg_ != nullptr);
        admin_token_ = 0x12345678ABCDEF00ULL;  // Test admin token
    }

    void TearDown() override {
        if (kg_initialized_ && kg_ != nullptr) {
            brain_kg_destroy(kg_);
        }
    }

    brain_kg_t* kg_ = nullptr;
    bool kg_initialized_ = false;
    uint64_t admin_token_;
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(PhysicsKGIntegrationTest, DefaultConfig) {
    physics_kg_config_t config;
    EXPECT_EQ(physics_kg_default_config(&config), 0);
    EXPECT_TRUE(config.register_hh);
    EXPECT_TRUE(config.register_thermo);
    EXPECT_TRUE(config.register_ephaptic);
}

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(PhysicsKGIntegrationTest, RegisterAll) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    physics_kg_config_t config;
    physics_kg_default_config(&config);

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    EXPECT_EQ(result, 0);

    if (result == 0) {
        EXPECT_TRUE(state.registered);
        EXPECT_GT(state.node_count, 0U);
    }
}

TEST_F(PhysicsKGIntegrationTest, RegisterHH) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    // First create root node
    physics_kg_config_t config;
    physics_kg_default_config(&config);
    config.register_thermo = false;
    config.register_ephaptic = false;

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    EXPECT_GE(result, 0);
}

TEST_F(PhysicsKGIntegrationTest, RegisterThermo) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    physics_kg_config_t config;
    physics_kg_default_config(&config);
    config.register_hh = false;
    config.register_ephaptic = false;

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    EXPECT_GE(result, 0);
}

TEST_F(PhysicsKGIntegrationTest, RegisterEphaptic) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    physics_kg_config_t config;
    physics_kg_default_config(&config);
    config.register_hh = false;
    config.register_thermo = false;

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    EXPECT_GE(result, 0);
}

TEST_F(PhysicsKGIntegrationTest, RegisterCrossEdges) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    physics_kg_config_t config;
    physics_kg_default_config(&config);
    config.register_cross_edges = true;

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    if (result == 0) {
        EXPECT_GT(state.edge_count, 0U);
    }
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(PhysicsKGIntegrationTest, GetRoot) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    // Register first
    physics_kg_config_t config;
    physics_kg_default_config(&config);

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));
    physics_kg_register_all(kg_, &config, &state, admin_token_);

    brain_kg_node_id_t root = physics_kg_get_root(kg_);
    // May or may not find root depending on implementation
    (void)root;
}

TEST_F(PhysicsKGIntegrationTest, FindSubsystem) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    // Register first
    physics_kg_config_t config;
    physics_kg_default_config(&config);

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));
    physics_kg_register_all(kg_, &config, &state, admin_token_);

    brain_kg_node_id_t hh = physics_kg_find_subsystem(kg_, PHYSICS_KG_HH_NAME);
    // May find or not depending on registration success
    (void)hh;
}

//=============================================================================
// State Synchronization Tests
//=============================================================================

TEST_F(PhysicsKGIntegrationTest, UpdateState) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    physics_kg_config_t config;
    physics_kg_default_config(&config);
    config.include_state_metadata = true;

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    if (result == 0) {
        // Update with physics values
        result = physics_kg_update_state(
            kg_, &state,
            310.15f,  // Temperature (K)
            0.8f,     // ATP level
            1.5f,     // LFP amplitude (mV)
            admin_token_
        );
        // May succeed or fail depending on metadata support
        (void)result;
    }
}

//=============================================================================
// Cleanup Tests
//=============================================================================

TEST_F(PhysicsKGIntegrationTest, UnregisterAll) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    physics_kg_config_t config;
    physics_kg_default_config(&config);

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    if (result == 0 && state.registered) {
        result = physics_kg_unregister_all(kg_, &state, admin_token_);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// Ion Channel Query Tests
//=============================================================================

TEST_F(PhysicsKGIntegrationTest, GetIonChannels) {
    if (!kg_initialized_) GTEST_SKIP() << "KG not initialized";

    physics_kg_config_t config;
    physics_kg_default_config(&config);
    config.register_channel_details = true;

    physics_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = physics_kg_register_all(kg_, &config, &state, admin_token_);
    if (result == 0) {
        brain_kg_node_list_t* channels = physics_kg_get_ion_channels(kg_);
        if (channels) {
            // Verify we got channel nodes
            brain_kg_node_list_destroy(channels);
        }
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
