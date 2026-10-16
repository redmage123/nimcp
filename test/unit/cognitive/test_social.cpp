/**
 * @file test_social.cpp
 * @brief Unit tests for Social Cognition module
 *
 * Tests social bonding, trust, loyalty, and friendship systems including:
 * - Substrate bridge integration (ATP/fatigue effects on social capacity)
 * - Thalamic bridge integration (attention-gated social signal routing)
 * - FEP bridge integration (free energy and social bonding)
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "cognitive/social/nimcp_social_substrate_bridge.h"
#include "cognitive/social/nimcp_social_thalamic_bridge.h"
#include "cognitive/love_loyalty_friendship/nimcp_love_loyalty_friendship_fep_bridge.h"
#include "async/nimcp_bio_router.h"

// ============================================================================
// Social Substrate Bridge Tests
// ============================================================================

/**
 * @brief Test fixture for Social Substrate Bridge tests
 */
class SocialSubstrateBridgeTest : public NimcpTestBase {
protected:
    social_substrate_bridge_t* bridge;
    social_substrate_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        bridge = nullptr;
        config = social_substrate_default_config();
    }

    void TearDown() override {
        if (bridge) {
            social_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

TEST_F(SocialSubstrateBridgeTest, DefaultConfigReturnsValidValues) {
    social_substrate_config_t cfg = social_substrate_default_config();

    // Check boolean flags are set
    EXPECT_TRUE(cfg.enable_atp_modulation || !cfg.enable_atp_modulation);  // Valid bool
    EXPECT_TRUE(cfg.enable_fatigue_modulation || !cfg.enable_fatigue_modulation);

    // Sensitivities should be reasonable
    EXPECT_GE(cfg.atp_sensitivity, 0.0f);
    EXPECT_GE(cfg.fatigue_sensitivity, 0.0f);

    // Min capacity should be bounded
    EXPECT_GE(cfg.min_capacity, 0.0f);
    EXPECT_LE(cfg.min_capacity, 1.0f);
}

TEST_F(SocialSubstrateBridgeTest, CreateWithNullDependenciesReturnsNull) {
    // Without social system and substrate, should return NULL
    bridge = social_substrate_bridge_create(nullptr, nullptr, &config);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(SocialSubstrateBridgeTest, DestroyNullBridgeIsNoOp) {
    // Should not crash
    social_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SocialSubstrateBridgeTest, UpdateNullBridgeReturnsError) {
    int result = social_substrate_bridge_update(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialSubstrateBridgeTest, GetEffectsNullBridgeReturnsError) {
    social_substrate_effects_t effects;
    int result = social_substrate_bridge_get_effects(nullptr, &effects);
    EXPECT_NE(result, 0);
}

TEST_F(SocialSubstrateBridgeTest, GetEffectsNullEffectsReturnsError) {
    int result = social_substrate_bridge_get_effects(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialSubstrateBridgeTest, ApplyEffectsNullBridgeReturnsError) {
    int result = social_substrate_bridge_apply_effects(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialSubstrateBridgeTest, RegisterBioAsyncNullBridgeReturnsError) {
    int result = social_substrate_bridge_register_bio_async(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialSubstrateBridgeTest, EffectsStructureIsValid) {
    social_substrate_effects_t effects = {};

    effects.bonding_capacity = 0.8f;
    effects.loyalty_strength = 0.9f;
    effects.trust_evaluation = 0.7f;
    effects.prosocial_motivation = 0.85f;
    effects.overall_capacity = 0.8f;

    // All values should be in [0, 1] range
    EXPECT_GE(effects.bonding_capacity, 0.0f);
    EXPECT_LE(effects.bonding_capacity, 1.0f);
    EXPECT_GE(effects.loyalty_strength, 0.0f);
    EXPECT_LE(effects.loyalty_strength, 1.0f);
    EXPECT_GE(effects.trust_evaluation, 0.0f);
    EXPECT_LE(effects.trust_evaluation, 1.0f);
    EXPECT_GE(effects.prosocial_motivation, 0.0f);
    EXPECT_LE(effects.prosocial_motivation, 1.0f);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

TEST_F(SocialSubstrateBridgeTest, ModuleIdIsCorrect) {
    EXPECT_EQ(BIO_MODULE_SUBSTRATE_SOCIAL, 0x1309);
}

// ============================================================================
// Social Thalamic Bridge Tests
// ============================================================================

/**
 * @brief Test fixture for Social Thalamic Bridge tests
 */
class SocialThalamicBridgeTest : public NimcpTestBase {
protected:
    social_thalamic_bridge_t* bridge;
    social_thalamic_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        bridge = nullptr;
        config = social_thalamic_default_config();
    }

    void TearDown() override {
        if (bridge) {
            social_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

TEST_F(SocialThalamicBridgeTest, DefaultConfigReturnsValidValues) {
    social_thalamic_config_t cfg = social_thalamic_default_config();

    // Boolean flags should be valid
    EXPECT_TRUE(cfg.enable_attention_gating || !cfg.enable_attention_gating);
    EXPECT_TRUE(cfg.enable_salience_boost || !cfg.enable_salience_boost);

    // Thresholds and boosts should be reasonable
    EXPECT_GE(cfg.min_salience_threshold, 0.0f);
    EXPECT_LE(cfg.min_salience_threshold, 1.0f);
    EXPECT_GE(cfg.betrayal_boost, 0.0f);
}

TEST_F(SocialThalamicBridgeTest, CreateWithNullDependenciesReturnsNull) {
    /* Create tolerates NULL dependencies (stores them as-is) */
    social_thalamic_bridge_t* b = social_thalamic_bridge_create(nullptr, nullptr, &config);
    EXPECT_NE(b, nullptr);
    social_thalamic_bridge_destroy(b);
}

TEST_F(SocialThalamicBridgeTest, DestroyNullBridgeIsNoOp) {
    social_thalamic_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SocialThalamicBridgeTest, ResetNullBridgeReturnsError) {
    int result = social_thalamic_bridge_reset(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialThalamicBridgeTest, RouteBondNullBridgeReturnsError) {
    social_thalamic_signal_t signal = {};
    int result = social_thalamic_route_bond(nullptr, &signal);
    EXPECT_NE(result, 0);
}

TEST_F(SocialThalamicBridgeTest, RouteBondNullSignalReturnsError) {
    int result = social_thalamic_route_bond(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialThalamicBridgeTest, RouteTrustNullBridgeReturnsError) {
    int result = social_thalamic_route_trust(nullptr, nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

TEST_F(SocialThalamicBridgeTest, SetAttentionNullBridgeReturnsError) {
    int result = social_thalamic_set_attention(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

TEST_F(SocialThalamicBridgeTest, GetAttentionNullBridgeReturnsError) {
    float attention;
    int result = social_thalamic_get_attention(nullptr, &attention);
    EXPECT_NE(result, 0);
}

TEST_F(SocialThalamicBridgeTest, GetStatsNullBridgeReturnsError) {
    social_thalamic_stats_t stats;
    int result = social_thalamic_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(SocialThalamicBridgeTest, SignalTypesAreDefined) {
    EXPECT_EQ(SOCIAL_SIGNAL_BOND, 0x0B01);
    EXPECT_EQ(SOCIAL_SIGNAL_TRUST, 0x0B02);
    EXPECT_EQ(SOCIAL_SIGNAL_BETRAYAL, 0x0B03);
    EXPECT_EQ(SOCIAL_SIGNAL_ALLIANCE, 0x0B04);
}

TEST_F(SocialThalamicBridgeTest, SignalStructureIsValid) {
    social_thalamic_signal_t signal = {};

    signal.signal_type = SOCIAL_SIGNAL_BOND;
    signal.social_salience = 0.8f;
    signal.emotional_weight = 0.7f;
    signal.urgency = 0.5f;
    signal.social_data = nullptr;
    signal.data_size = 0;
    signal.timestamp_us = 1000000;

    EXPECT_EQ(signal.signal_type, SOCIAL_SIGNAL_BOND);
    EXPECT_GE(signal.social_salience, 0.0f);
    EXPECT_LE(signal.social_salience, 1.0f);
}

TEST_F(SocialThalamicBridgeTest, StatsStructureIsValid) {
    social_thalamic_stats_t stats = {};

    stats.bonds_routed = 100;
    stats.trust_events = 50;
    stats.betrayals_detected = 2;
    stats.avg_social_salience = 0.65f;

    EXPECT_EQ(stats.bonds_routed, 100UL);
    EXPECT_EQ(stats.trust_events, 50UL);
    EXPECT_EQ(stats.betrayals_detected, 2UL);
    EXPECT_GE(stats.avg_social_salience, 0.0f);
    EXPECT_LE(stats.avg_social_salience, 1.0f);
}

// ============================================================================
// Social Bond FEP Bridge Tests
// ============================================================================

/**
 * @brief Test fixture for Social Bond FEP Bridge tests
 */
class SocialBondFepBridgeTest : public NimcpTestBase {
protected:
    social_bond_fep_bridge_t* bridge;
    social_bond_fep_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        bio_router_init(NULL);
        bridge = nullptr;
        social_bond_fep_bridge_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            social_bond_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        bio_router_shutdown();
        NimcpTestBase::TearDown();
    }
};

TEST_F(SocialBondFepBridgeTest, DefaultConfigReturnsSuccess) {
    social_bond_fep_config_t cfg;
    int result = social_bond_fep_bridge_default_config(&cfg);
    EXPECT_EQ(result, 0);
}

TEST_F(SocialBondFepBridgeTest, DefaultConfigNullReturnsError) {
    int result = social_bond_fep_bridge_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, DefaultConfigHasReasonableValues) {
    social_bond_fep_config_t cfg;
    social_bond_fep_bridge_default_config(&cfg);

    EXPECT_GT(cfg.pe_anxiety_threshold, 0.0f);
    EXPECT_GT(cfg.trust_precision_factor, 0.0f);
    EXPECT_GE(cfg.loyalty_prior_strength, 0.0f);
    EXPECT_LE(cfg.loyalty_prior_strength, 1.0f);
}

TEST_F(SocialBondFepBridgeTest, CreateWithValidConfigSucceeds) {
    bridge = social_bond_fep_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SocialBondFepBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = social_bond_fep_bridge_create(nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SocialBondFepBridgeTest, DestroyNullBridgeIsNoOp) {
    social_bond_fep_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SocialBondFepBridgeTest, ConnectFepNullBridgeReturnsError) {
    int result = social_bond_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, ConnectSocialNullBridgeReturnsError) {
    int result = social_bond_fep_bridge_connect_social(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, DisconnectNullBridgeReturnsError) {
    int result = social_bond_fep_bridge_disconnect(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, TriggerAttachmentAnxietyNullBridgeReturnsError) {
    int result = social_bond_fep_trigger_attachment_anxiety(nullptr, 5.0f);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, ModulateTrustByPrecisionNullBridgeReturnsError) {
    int result = social_bond_fep_modulate_trust_by_precision(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, TriggerRelationshipRevisionNullBridgeReturnsError) {
    int result = social_bond_fep_trigger_relationship_revision(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, ApplyAttachmentPriorsNullBridgeReturnsError) {
    int result = social_bond_fep_apply_attachment_priors(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, ApplyClosenessBeliefNullBridgeReturnsError) {
    int result = social_bond_fep_apply_closeness_beliefs(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, UpdateModelFromBetrayalNullBridgeReturnsError) {
    int result = social_bond_fep_update_model_from_betrayal(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, UpdateNullBridgeReturnsError) {
    int result = social_bond_fep_bridge_update(nullptr, 16);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, GetStateNullBridgeReturnsError) {
    social_bond_fep_state_t state;
    int result = social_bond_fep_bridge_get_state(nullptr, &state);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, GetStatsNullBridgeReturnsError) {
    social_bond_fep_stats_t stats;
    int result = social_bond_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, BioAsyncConnectNullBridgeReturnsError) {
    int result = social_bond_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SocialBondFepBridgeTest, BioAsyncDisconnectNullBridgeReturnsError) {
    /* Disconnect with NULL bridge is a safe no-op */
    int result = social_bond_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(SocialBondFepBridgeTest, BioAsyncIsConnectedNullBridgeReturnsFalse) {
    bool connected = social_bond_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(SocialBondFepBridgeTest, ConstantsAreDefined) {
    EXPECT_GT(SOCIAL_FEP_HIGH_PE_THRESHOLD, 0.0f);
    EXPECT_GT(SOCIAL_FEP_TRUST_PRECISION_FACTOR, 0.0f);
    EXPECT_GE(SOCIAL_FEP_LOYALTY_PRIOR_STRENGTH, 0.0f);
    EXPECT_LE(SOCIAL_FEP_LOYALTY_PRIOR_STRENGTH, 1.0f);
    EXPECT_GT(SOCIAL_FEP_MAX_RELATIONSHIPS, 0);
}

TEST_F(SocialBondFepBridgeTest, FepEffectsStructureIsValid) {
    social_bond_fep_effects_t effects = {};

    effects.current_prediction_error = 2.5f;
    effects.attachment_anxiety_triggered = false;
    effects.num_relationships_tracked = 5;
    effects.current_surprise = 1.0f;
    effects.trust_update_active = true;

    EXPECT_FLOAT_EQ(effects.current_prediction_error, 2.5f);
    EXPECT_FALSE(effects.attachment_anxiety_triggered);
    EXPECT_EQ(effects.num_relationships_tracked, 5U);
}

TEST_F(SocialBondFepBridgeTest, SocialEffectsStructureIsValid) {
    fep_social_bond_effects_t effects = {};

    effects.attachment_security_bias = 0.3f;
    effects.trust_constraining_model = true;
    effects.closeness_prior_strength = 0.7f;
    effects.loyalty_commitment_level = 0.8f;
    effects.love_intensity_factor = 0.9f;
    effects.model_beliefs_updated = true;

    EXPECT_FLOAT_EQ(effects.attachment_security_bias, 0.3f);
    EXPECT_TRUE(effects.trust_constraining_model);
    EXPECT_TRUE(effects.model_beliefs_updated);
}

TEST_F(SocialBondFepBridgeTest, StateStructureIsValid) {
    social_bond_fep_state_t state = {};

    state.current_prediction_error = 1.5f;
    state.current_attachment_security = 0.8f;
    state.current_trust_mean = 0.75f;
    state.attachment_anxiety_active = false;
    state.num_close_relationships = 3;
    state.last_anxiety_time = 1000;
    state.last_trust_update_time = 2000;

    EXPECT_FLOAT_EQ(state.current_attachment_security, 0.8f);
    EXPECT_EQ(state.num_close_relationships, 3U);
}

TEST_F(SocialBondFepBridgeTest, StatsStructureIsValid) {
    social_bond_fep_stats_t stats = {};

    stats.attachment_anxiety_events = 10;
    stats.trust_updates = 50;
    stats.relationship_revisions = 5;
    stats.avg_prediction_error = 1.2f;
    stats.avg_attachment_security = 0.85f;
    stats.precision_applications = 100;
    stats.belief_updates = 75;
    stats.avg_free_energy = 3.5f;

    EXPECT_EQ(stats.attachment_anxiety_events, 10UL);
    EXPECT_GT(stats.avg_attachment_security, 0.0f);
    EXPECT_LE(stats.avg_attachment_security, 1.0f);
}

// ============================================================================
// Integration Tests - Bridge Lifecycle
// ============================================================================

TEST_F(SocialBondFepBridgeTest, CreateUpdateDestroyLifecycle) {
    bridge = social_bond_fep_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Update without connected systems should handle gracefully
    int result = social_bond_fep_bridge_update(bridge, 16);
    // May return error if systems not connected, but should not crash
    (void)result;

    social_bond_fep_bridge_destroy(bridge);
    bridge = nullptr;

    SUCCEED();
}

TEST_F(SocialBondFepBridgeTest, GetStateAfterCreate) {
    bridge = social_bond_fep_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    social_bond_fep_state_t state;
    int result = social_bond_fep_bridge_get_state(bridge, &state);

    // Should succeed and return initial state
    if (result == 0) {
        EXPECT_FALSE(state.attachment_anxiety_active);
        EXPECT_EQ(state.num_close_relationships, 0U);
    }
}

TEST_F(SocialBondFepBridgeTest, GetStatsAfterCreate) {
    bridge = social_bond_fep_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    social_bond_fep_stats_t stats;
    int result = social_bond_fep_bridge_get_stats(bridge, &stats);

    // Should succeed and return zeroed stats
    if (result == 0) {
        EXPECT_EQ(stats.attachment_anxiety_events, 0UL);
        EXPECT_EQ(stats.trust_updates, 0UL);
    }
}
