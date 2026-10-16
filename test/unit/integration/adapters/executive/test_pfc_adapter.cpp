/**
 * @file test_pfc_adapter.cpp
 * @brief Unit tests for Prefrontal Cortex Adapter
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/executive/nimcp_pfc_adapter.h"

class PFCAdapterTest : public ::testing::Test {
protected:
    nimcp_pfc_adapter_t adapter;
    nimcp_pfc_config_t config;

    void SetUp() override {
        config = nimcp_pfc_adapter_default_config();
        adapter = nimcp_pfc_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_pfc_adapter_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_pfc_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(PFCAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_pfc_config_t default_config = nimcp_pfc_adapter_default_config();

    EXPECT_GT(default_config.wm_slots, 0u);
    EXPECT_LE(default_config.wm_slots, 16u);  // Working memory limited
    EXPECT_GT(default_config.gate_threshold, 0.0f);
    EXPECT_LE(default_config.gate_threshold, 1.0f);
    EXPECT_GT(default_config.decay_rate, 0.0f);
}

TEST_F(PFCAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_pfc_adapter_t adapter_null = nimcp_pfc_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_pfc_adapter_destroy(adapter_null);
}

TEST_F(PFCAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_pfc_adapter_destroy(NULL);
}

TEST_F(PFCAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_pfc_adapter_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

// ============================================================================
// UPDATE TESTS
// ============================================================================

TEST_F(PFCAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_pfc_adapter_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

// ============================================================================
// WORKING MEMORY STORE TESTS
// ============================================================================

TEST_F(PFCAdapterTest, WMStoreSucceeds) {
    float content[32];
    for (int i = 0; i < 32; i++) {
        content[i] = (float)i / 32.0f;
    }

    uint32_t slot;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.8f, &slot));
}

TEST_F(PFCAdapterTest, WMStoreNullFails) {
    float content[32];
    uint32_t slot;
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_store(NULL, content, 32, 0.8f, &slot));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_store(adapter, NULL, 32, 0.8f, &slot));
}

TEST_F(PFCAdapterTest, WMStoreLowPriorityGated) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;

    uint32_t slot;
    // Very low priority should be gated (below threshold)
    nimcp_layer_error_t result = nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.1f, &slot);
    EXPECT_NE(NIMCP_LAYER_OK, result);  // Should fail - gate closed
}

TEST_F(PFCAdapterTest, WMStoreMultipleItems) {
    float content[32];

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 32; j++) content[j] = (float)(i * 32 + j) / 160.0f;
        uint32_t slot;
        EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.8f, &slot));
    }

    nimcp_pfc_state_t state;
    nimcp_pfc_adapter_get_state(adapter, &state);
    EXPECT_EQ(5u, state.occupied_slots);
}

// ============================================================================
// WORKING MEMORY RETRIEVE TESTS
// ============================================================================

TEST_F(PFCAdapterTest, WMRetrieveSucceeds) {
    // Store first
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = (float)i / 32.0f;
    uint32_t slot;
    nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.8f, &slot);

    // Retrieve
    float retrieved[32];
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_retrieve(adapter, slot, retrieved, 32));

    // Verify content
    for (int i = 0; i < 32; i++) {
        EXPECT_NEAR(content[i], retrieved[i], 0.001f);
    }
}

TEST_F(PFCAdapterTest, WMRetrieveNullFails) {
    float retrieved[32];
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_retrieve(NULL, 0, retrieved, 32));
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_retrieve(adapter, 0, NULL, 32));
}

TEST_F(PFCAdapterTest, WMRetrieveEmptySlotFails) {
    float retrieved[32];
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_retrieve(adapter, 0, retrieved, 32));
}

// ============================================================================
// WORKING MEMORY REFRESH TESTS
// ============================================================================

TEST_F(PFCAdapterTest, WMRefreshSucceeds) {
    // Store first
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;
    uint32_t slot;
    nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.8f, &slot);

    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_refresh(adapter, slot));
}

TEST_F(PFCAdapterTest, WMRefreshPreventsDecay) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;
    uint32_t slot;
    nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.8f, &slot);

    nimcp_module_interface_t* iface = nimcp_pfc_adapter_adapter_get_interface(adapter);

    // Let some time pass with decay
    for (int i = 0; i < 50; i++) {
        iface->update(adapter, 0.1f);
    }

    // Refresh
    nimcp_pfc_adapter_wm_refresh(adapter, slot);

    // Should still be accessible
    float retrieved[32];
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_retrieve(adapter, slot, retrieved, 32));
}

// ============================================================================
// WORKING MEMORY CLEAR TESTS
// ============================================================================

TEST_F(PFCAdapterTest, WMClearSucceeds) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;
    uint32_t slot;
    nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.8f, &slot);

    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_clear(adapter, slot));

    // Verify cleared
    float retrieved[32];
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_retrieve(adapter, slot, retrieved, 32));
}

// ============================================================================
// GATING MODULATION TESTS
// ============================================================================

TEST_F(PFCAdapterTest, ModulateGatingSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_modulate_gating(adapter, 0.8f));
}

TEST_F(PFCAdapterTest, ModulateGatingNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_modulate_gating(NULL, 0.8f));
}

TEST_F(PFCAdapterTest, HighDopamineLowersGateThreshold) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;

    // First, try storing with low priority (should fail with normal gating)
    uint32_t slot;
    nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.3f, &slot);  // May fail

    // Increase dopamine
    nimcp_pfc_adapter_modulate_gating(adapter, 0.9f);

    // Now low priority should pass (lower threshold)
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.3f, &slot));
}

// ============================================================================
// CONFLICT MONITORING TESTS
// ============================================================================

TEST_F(PFCAdapterTest, GetConflictSucceeds) {
    float conflict;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_get_conflict(adapter, &conflict));
    EXPECT_GE(conflict, 0.0f);
    EXPECT_LE(conflict, 1.0f);
}

TEST_F(PFCAdapterTest, ConflictIncreasesWithLoad) {
    float conflict_before, conflict_after;
    nimcp_pfc_adapter_get_conflict(adapter, &conflict_before);

    // Fill working memory
    float content[32];
    for (int j = 0; j < 32; j++) content[j] = 0.5f;

    for (uint32_t i = 0; i < config.wm_slots; i++) {
        uint32_t slot;
        nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.9f, &slot);
    }

    nimcp_module_interface_t* iface = nimcp_pfc_adapter_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    nimcp_pfc_adapter_get_conflict(adapter, &conflict_after);
    EXPECT_GE(conflict_after, conflict_before);
}

// ============================================================================
// INHIBITION TESTS
// ============================================================================

TEST_F(PFCAdapterTest, ApplyInhibitionSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_apply_inhibition(adapter, 0.8f));
}

TEST_F(PFCAdapterTest, ApplyInhibitionNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_pfc_adapter_apply_inhibition(NULL, 0.8f));
}

TEST_F(PFCAdapterTest, InhibitionAffectsState) {
    nimcp_pfc_state_t state_before, state_after;
    nimcp_pfc_adapter_get_state(adapter, &state_before);

    nimcp_pfc_adapter_apply_inhibition(adapter, 0.9f);

    nimcp_module_interface_t* iface = nimcp_pfc_adapter_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    nimcp_pfc_adapter_get_state(adapter, &state_after);
    EXPECT_GT(state_after.inhibitory_control, state_before.inhibitory_control);
}

// ============================================================================
// STATE AND STATS TESTS
// ============================================================================

TEST_F(PFCAdapterTest, GetStateSucceeds) {
    nimcp_pfc_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(PFCAdapterTest, GetStatsSucceeds) {
    nimcp_pfc_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_get_stats(adapter, &stats));
}

TEST_F(PFCAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_pfc_adapter_reset_stats(adapter));
}

TEST_F(PFCAdapterTest, StatsTrackStoresAndRetrievals) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;

    // Store 3 items
    for (int i = 0; i < 3; i++) {
        uint32_t slot;
        nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.8f, &slot);
    }

    // Retrieve from slot 0
    float retrieved[32];
    nimcp_pfc_adapter_wm_retrieve(adapter, 0, retrieved, 32);

    nimcp_pfc_stats_t stats;
    nimcp_pfc_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(3u, stats.wm_stores);
    EXPECT_EQ(1u, stats.wm_retrievals);
}

// ============================================================================
// DECAY TESTS
// ============================================================================

TEST_F(PFCAdapterTest, WorkingMemoryDecays) {
    float content[32];
    for (int i = 0; i < 32; i++) content[i] = 0.5f;

    uint32_t slot;
    nimcp_pfc_adapter_wm_store(adapter, content, 32, 0.6f, &slot);

    nimcp_module_interface_t* iface = nimcp_pfc_adapter_adapter_get_interface(adapter);

    // Let time pass (with decay)
    for (int i = 0; i < 200; i++) {
        iface->update(adapter, 0.1f);
    }

    // After decay, slot should be empty
    float retrieved[32];
    nimcp_layer_error_t result = nimcp_pfc_adapter_wm_retrieve(adapter, slot, retrieved, 32);
    EXPECT_NE(NIMCP_LAYER_OK, result);  // Decayed
}
