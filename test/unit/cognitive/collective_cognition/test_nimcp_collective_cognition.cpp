/**
 * @file test_nimcp_collective_cognition.cpp
 * @brief Unit tests for collective cognition system
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CollectiveCognitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = collective_cognition_default_config();
        cc_ = collective_cognition_create(&config_);
        ASSERT_NE(cc_, nullptr);
    }

    void TearDown() override {
        if (cc_) {
            collective_cognition_destroy(cc_);
            cc_ = nullptr;
        }
    }

    collective_cognition_config_t config_;
    collective_cognition_t* cc_ = nullptr;
};

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, CreateWithNullConfig) {
    collective_cognition_t* cc = collective_cognition_create(nullptr);
    ASSERT_NE(cc, nullptr);
    collective_cognition_destroy(cc);
}

TEST_F(CollectiveCognitionTest, DestroyNull) {
    collective_cognition_destroy(nullptr);  // Should not crash
}

TEST_F(CollectiveCognitionTest, Reset) {
    // Register some instances first
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);
    EXPECT_EQ(collective_cognition_instance_count(cc_), 2u);

    // Reset
    ASSERT_EQ(collective_cognition_reset(cc_), 0);
    EXPECT_EQ(collective_cognition_instance_count(cc_), 0u);
}

TEST_F(CollectiveCognitionTest, ResetNull) {
    EXPECT_EQ(collective_cognition_reset(nullptr), -1);
}

/*=============================================================================
 * Instance Management Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, RegisterInstance) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    EXPECT_EQ(collective_cognition_instance_count(cc_), 1u);
    EXPECT_TRUE(collective_cognition_has_instance(cc_, 1));
}

TEST_F(CollectiveCognitionTest, RegisterMultipleInstances) {
    for (uint32_t i = 1; i <= 5; i++) {
        ASSERT_EQ(collective_cognition_register_instance(cc_, i, nullptr), 0);
    }
    EXPECT_EQ(collective_cognition_instance_count(cc_), 5u);
}

TEST_F(CollectiveCognitionTest, RegisterDuplicateInstance) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    EXPECT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), -1);
    EXPECT_EQ(collective_cognition_instance_count(cc_), 1u);
}

TEST_F(CollectiveCognitionTest, UnregisterInstance) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);
    EXPECT_EQ(collective_cognition_instance_count(cc_), 2u);

    ASSERT_EQ(collective_cognition_unregister_instance(cc_, 1), 0);
    EXPECT_EQ(collective_cognition_instance_count(cc_), 1u);
    EXPECT_FALSE(collective_cognition_has_instance(cc_, 1));
    EXPECT_TRUE(collective_cognition_has_instance(cc_, 2));
}

TEST_F(CollectiveCognitionTest, UnregisterNonexistentInstance) {
    EXPECT_EQ(collective_cognition_unregister_instance(cc_, 999), -1);
}

TEST_F(CollectiveCognitionTest, HasInstanceNull) {
    EXPECT_FALSE(collective_cognition_has_instance(nullptr, 1));
}

/*=============================================================================
 * Update Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, UpdateEmpty) {
    EXPECT_EQ(collective_cognition_update(cc_), 0);
}

TEST_F(CollectiveCognitionTest, UpdateSingleInstance) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    EXPECT_EQ(collective_cognition_update(cc_), 0);
}

TEST_F(CollectiveCognitionTest, UpdateMultipleInstances) {
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(collective_cognition_register_instance(cc_, i, nullptr), 0);
    }

    // Run multiple updates
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(collective_cognition_update(cc_), 0);
    }
}

TEST_F(CollectiveCognitionTest, UpdateNull) {
    EXPECT_EQ(collective_cognition_update(nullptr), -1);
}

/*=============================================================================
 * State Query Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, GetState) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state), 0);
    EXPECT_EQ(state.active_instances, 2u);
}

TEST_F(CollectiveCognitionTest, GetStateNull) {
    collective_cognition_state_t state;
    EXPECT_EQ(collective_cognition_get_state(nullptr, &state), -1);
    EXPECT_EQ(collective_cognition_get_state(cc_, nullptr), -1);
}

TEST_F(CollectiveCognitionTest, GetConsciousnessLevel) {
    collective_consciousness_level_t level = collective_cognition_get_consciousness_level(cc_);
    EXPECT_EQ(level, COLLECTIVE_CONSCIOUSNESS_NONE);
}

TEST_F(CollectiveCognitionTest, GetHyperscanState) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    hyperscan_state_t state;
    ASSERT_EQ(collective_cognition_get_hyperscan_state(cc_, &state), 0);
}

TEST_F(CollectiveCognitionTest, GetExtendedMindState) {
    extended_mind_state_t state;
    ASSERT_EQ(collective_cognition_get_extended_mind_state(cc_, &state), 0);
}

TEST_F(CollectiveCognitionTest, GetPhi) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_phi_t phi;
    ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);
    EXPECT_GE(phi.phi_total, 0.0f);
}

TEST_F(CollectiveCognitionTest, GetWeMode) {
    we_mode_state_t state;
    ASSERT_EQ(collective_cognition_get_we_mode(cc_, &state), 0);
}

/*=============================================================================
 * Bio-Async Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, BioAsyncNotConnected) {
    EXPECT_FALSE(collective_cognition_is_bio_async_connected(cc_));
}

TEST_F(CollectiveCognitionTest, DisconnectBioAsyncWhenNotConnected) {
    EXPECT_EQ(collective_cognition_disconnect_bio_async(cc_), 0);
}

/*=============================================================================
 * Load Balancing Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, BalanceLoadEmpty) {
    EXPECT_EQ(collective_cognition_balance_load(cc_), 0);
}

TEST_F(CollectiveCognitionTest, BalanceLoadSingleInstance) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    EXPECT_EQ(collective_cognition_balance_load(cc_), 0);
}

TEST_F(CollectiveCognitionTest, OffloadTask) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);

    uint8_t task_data[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(collective_cognition_offload_task(cc_, 1, 2, task_data, sizeof(task_data)), 0);
}

TEST_F(CollectiveCognitionTest, OffloadTaskInvalidInstance) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);

    uint8_t task_data[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(collective_cognition_offload_task(cc_, 1, 999, task_data, sizeof(task_data)), -1);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, GetStats) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_stats_t stats;
    ASSERT_EQ(collective_cognition_get_stats(cc_, &stats), 0);
    EXPECT_EQ(stats.total_updates, 1u);
    EXPECT_EQ(stats.instances_joined, 1u);
}

TEST_F(CollectiveCognitionTest, ResetStats) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_reset_stats(cc_);

    collective_cognition_stats_t stats;
    ASSERT_EQ(collective_cognition_get_stats(cc_, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0u);
}

/*=============================================================================
 * Utility Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, ConsciousnessLevelName) {
    EXPECT_STREQ(collective_consciousness_level_name(COLLECTIVE_CONSCIOUSNESS_NONE), "NONE");
    EXPECT_STREQ(collective_consciousness_level_name(COLLECTIVE_CONSCIOUSNESS_MINIMAL), "MINIMAL");
    EXPECT_STREQ(collective_consciousness_level_name(COLLECTIVE_CONSCIOUSNESS_EMERGING), "EMERGING");
    EXPECT_STREQ(collective_consciousness_level_name(COLLECTIVE_CONSCIOUSNESS_PARTIAL), "PARTIAL");
    EXPECT_STREQ(collective_consciousness_level_name(COLLECTIVE_CONSCIOUSNESS_UNIFIED), "UNIFIED");
    EXPECT_STREQ(collective_consciousness_level_name(COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT), "TRANSCENDENT");
}

TEST_F(CollectiveCognitionTest, SyncBandName) {
    EXPECT_STREQ(sync_band_name(SYNC_BAND_DELTA), "DELTA");
    EXPECT_STREQ(sync_band_name(SYNC_BAND_THETA), "THETA");
    EXPECT_STREQ(sync_band_name(SYNC_BAND_ALPHA), "ALPHA");
    EXPECT_STREQ(sync_band_name(SYNC_BAND_BETA), "BETA");
    EXPECT_STREQ(sync_band_name(SYNC_BAND_GAMMA), "GAMMA");
}

TEST_F(CollectiveCognitionTest, ExtensionTypeName) {
    EXPECT_STREQ(extension_type_name(EXT_TYPE_MEMORY), "MEMORY");
    EXPECT_STREQ(extension_type_name(EXT_TYPE_PERCEPTION), "PERCEPTION");
    EXPECT_STREQ(extension_type_name(EXT_TYPE_REASONING), "REASONING");
    EXPECT_STREQ(extension_type_name(EXT_TYPE_ACTION), "ACTION");
    EXPECT_STREQ(extension_type_name(EXT_TYPE_COMMUNICATION), "COMMUNICATION");
}

TEST_F(CollectiveCognitionTest, DumpDoesNotCrash) {
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_dump(cc_);  // Should not crash
    collective_cognition_dump(nullptr);  // Should not crash
}

/*=============================================================================
 * Integration Tests
 *===========================================================================*/

TEST_F(CollectiveCognitionTest, MultiInstancePhiEvolution) {
    // Register multiple instances
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(collective_cognition_register_instance(cc_, i, nullptr), 0);
    }

    // Run updates and track phi evolution
    float prev_phi = 0.0f;
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(collective_cognition_update(cc_), 0);

        collective_phi_t phi;
        ASSERT_EQ(collective_cognition_get_phi(cc_, &phi), 0);

        // Phi should be non-negative
        EXPECT_GE(phi.phi_total, 0.0f);
        prev_phi = phi.phi_total;
    }
}

TEST_F(CollectiveCognitionTest, InstanceJoinLeave) {
    // Start with 2 instances
    ASSERT_EQ(collective_cognition_register_instance(cc_, 1, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 2, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state1;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state1), 0);
    EXPECT_EQ(state1.active_instances, 2u);

    // Add more instances
    ASSERT_EQ(collective_cognition_register_instance(cc_, 3, nullptr), 0);
    ASSERT_EQ(collective_cognition_register_instance(cc_, 4, nullptr), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state2;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state2), 0);
    EXPECT_EQ(state2.active_instances, 4u);

    // Remove some instances
    ASSERT_EQ(collective_cognition_unregister_instance(cc_, 1), 0);
    ASSERT_EQ(collective_cognition_unregister_instance(cc_, 3), 0);
    ASSERT_EQ(collective_cognition_update(cc_), 0);

    collective_cognition_state_t state3;
    ASSERT_EQ(collective_cognition_get_state(cc_, &state3), 0);
    EXPECT_EQ(state3.active_instances, 2u);
}
