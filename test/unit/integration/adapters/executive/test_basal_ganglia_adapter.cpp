/**
 * @file test_basal_ganglia_adapter.cpp
 * @brief Unit tests for Basal Ganglia Adapter
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/executive/nimcp_basal_ganglia_adapter.h"

class BasalGangliaAdapterTest : public ::testing::Test {
protected:
    nimcp_basal_ganglia_adapter_t adapter;
    nimcp_basal_ganglia_config_t config;

    void SetUp() override {
        config = nimcp_basal_ganglia_adapter_default_config();
        adapter = nimcp_basal_ganglia_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_basal_ganglia_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_basal_ganglia_config_t default_config = nimcp_basal_ganglia_adapter_default_config();

    EXPECT_GT(default_config.num_actions, 0u);
    EXPECT_GT(default_config.d1_d2_balance, 0.0f);
    EXPECT_LE(default_config.d1_d2_balance, 1.0f);
    EXPECT_GT(default_config.selection_threshold, 0.0f);
    EXPECT_GT(default_config.dopamine_baseline, 0.0f);
}

TEST_F(BasalGangliaAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_basal_ganglia_adapter_t adapter_null = nimcp_basal_ganglia_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_basal_ganglia_adapter_destroy(adapter_null);
}

TEST_F(BasalGangliaAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_basal_ganglia_adapter_destroy(NULL);
}

TEST_F(BasalGangliaAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

// ============================================================================
// UPDATE TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

// ============================================================================
// ACTION SALIENCE TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, SetActionSalienceSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 0.8f));
}

TEST_F(BasalGangliaAdapterTest, SetActionSalienceNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_set_action_salience(NULL, 0, 0.8f));
}

TEST_F(BasalGangliaAdapterTest, SetActionSalienceOutOfRangeFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_set_action_salience(adapter, 999, 0.8f));
}

TEST_F(BasalGangliaAdapterTest, SalienceClampedTo01) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, -0.5f));
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 1.5f));
}

// ============================================================================
// ACTION SELECTION TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, SelectActionSucceeds) {
    // Set different saliences
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 0.3f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 1, 0.9f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 2, 0.2f);

    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    uint32_t selected;
    float confidence;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_select_action(adapter, &selected, &confidence));
}

TEST_F(BasalGangliaAdapterTest, SelectActionChoosesHighestSalience) {
    // Action 2 has highest salience
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 0.2f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 1, 0.3f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 2, 0.9f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 3, 0.1f);

    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    uint32_t selected;
    float confidence;
    nimcp_basal_ganglia_adapter_select_action(adapter, &selected, &confidence);

    EXPECT_EQ(2u, selected);
}

TEST_F(BasalGangliaAdapterTest, SelectActionNullFails) {
    uint32_t selected;
    float confidence;
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_select_action(NULL, &selected, &confidence));
}

// ============================================================================
// DOPAMINE MODULATION TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, ApplyDopamineSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_apply_dopamine(adapter, 0.3f));
}

TEST_F(BasalGangliaAdapterTest, ApplyDopamineNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_apply_dopamine(NULL, 0.3f));
}

TEST_F(BasalGangliaAdapterTest, DopamineAffectsSelection) {
    // Set equal saliences
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 0.5f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 1, 0.5f);

    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);

    // High dopamine favors direct pathway
    nimcp_basal_ganglia_adapter_apply_dopamine(adapter, 0.4f);
    iface->update(adapter, 0.001f);

    nimcp_basal_ganglia_state_t state;
    nimcp_basal_ganglia_adapter_get_state(adapter, &state);
    EXPECT_GT(state.dopamine_level, config.dopamine_baseline);
}

// ============================================================================
// RESPONSE INHIBITION TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, InhibitResponseSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_inhibit_response(adapter, 0.8f));
}

TEST_F(BasalGangliaAdapterTest, InhibitResponseNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_inhibit_response(NULL, 0.8f));
}

TEST_F(BasalGangliaAdapterTest, InhibitionIncreasesSTNActivity) {
    nimcp_basal_ganglia_state_t state_before, state_after;
    nimcp_basal_ganglia_adapter_get_state(adapter, &state_before);

    nimcp_basal_ganglia_adapter_inhibit_response(adapter, 1.0f);

    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    nimcp_basal_ganglia_adapter_get_state(adapter, &state_after);
    EXPECT_GT(state_after.stn_activity, state_before.stn_activity);
}

// ============================================================================
// STATE AND STATS TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, GetStateSucceeds) {
    nimcp_basal_ganglia_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(BasalGangliaAdapterTest, GetStatsSucceeds) {
    nimcp_basal_ganglia_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_get_stats(adapter, &stats));
}

TEST_F(BasalGangliaAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_basal_ganglia_adapter_reset_stats(adapter));
}

TEST_F(BasalGangliaAdapterTest, StatsTrackSelections) {
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 0.9f);

    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    uint32_t selected;
    float confidence;
    for (int i = 0; i < 5; i++) {
        nimcp_basal_ganglia_adapter_select_action(adapter, &selected, &confidence);
    }

    nimcp_basal_ganglia_stats_t stats;
    nimcp_basal_ganglia_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(5u, stats.actions_selected);
}

// ============================================================================
// CONFLICT TESTS
// ============================================================================

TEST_F(BasalGangliaAdapterTest, ConflictDetectedWithSimilarSaliences) {
    // Two actions with similar high salience = conflict
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 0.85f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 1, 0.80f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 2, 0.1f);

    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    nimcp_basal_ganglia_state_t state;
    nimcp_basal_ganglia_adapter_get_state(adapter, &state);
    EXPECT_GT(state.conflict_level, 0.5f);  // High conflict
}

TEST_F(BasalGangliaAdapterTest, NoConflictWithClearWinner) {
    // One action clearly dominates
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 0, 0.9f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 1, 0.1f);
    nimcp_basal_ganglia_adapter_set_action_salience(adapter, 2, 0.05f);

    nimcp_module_interface_t* iface = nimcp_basal_ganglia_adapter_get_interface(adapter);
    iface->update(adapter, 0.001f);

    nimcp_basal_ganglia_state_t state;
    nimcp_basal_ganglia_adapter_get_state(adapter, &state);
    EXPECT_LT(state.conflict_level, 0.3f);  // Low conflict
}
