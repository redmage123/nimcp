/**
 * @file test_amygdala_attention_bridge.cpp
 * @brief Unit tests for Amygdala-Attention Integration Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_amygdala_attention_bridge.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class AmygdalaAttentionBridgeTest : public ::testing::Test {
protected:
    amygdala_attention_bridge_t* bridge;
    amygdala_t* amygdala;
    amygdala_attention_config_t config;

    void SetUp() override {
        /* Create amygdala */
        amyg_config_t amyg_config;
        amygdala_default_config(&amyg_config);
        amygdala = amygdala_create(&amyg_config);
        ASSERT_NE(amygdala, nullptr);

        /* Create bridge */
        amygdala_attention_default_config(&config);
        bridge = amygdala_attention_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            amygdala_attention_destroy(bridge);
        }
        if (amygdala) {
            amygdala_destroy(amygdala);
        }
    }

    /* Helper to set amygdala state */
    void SetAmygdalaState(float fear, float anxiety, amyg_threat_level_t threat) {
        amygdala_set_anxiety(amygdala, anxiety);
        /* Note: Fear level is typically set by processing stimuli, but we can
         * manipulate it indirectly via conditioned stimuli and threat level */
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, DefaultConfigurationValid) {
    amygdala_attention_config_t cfg;
    int result = amygdala_attention_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_threat_salience_boost);
    EXPECT_TRUE(cfg.enable_hypervigilance_mode);
    EXPECT_TRUE(cfg.enable_attention_enhancement);
    EXPECT_TRUE(cfg.enable_distraction_suppression);
    EXPECT_FLOAT_EQ(cfg.threat_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.attention_sensitivity, 1.0f);
}

TEST_F(AmygdalaAttentionBridgeTest, ValidateGoodConfiguration) {
    amygdala_attention_config_t cfg;
    amygdala_attention_default_config(&cfg);
    int result = amygdala_attention_validate_config(&cfg);
    EXPECT_EQ(result, 0);
}

TEST_F(AmygdalaAttentionBridgeTest, ValidateInvalidSensitivity) {
    amygdala_attention_config_t cfg;
    amygdala_attention_default_config(&cfg);

    cfg.threat_sensitivity = 3.0f; /* Out of range */
    EXPECT_EQ(amygdala_attention_validate_config(&cfg), NIMCP_ERROR_INVALID_PARAMETER);

    cfg.threat_sensitivity = 1.0f;
    cfg.attention_sensitivity = 0.2f; /* Out of range */
    EXPECT_EQ(amygdala_attention_validate_config(&cfg), NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, CreateAndDestroyBridge) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_NE(bridge->base.mutex, nullptr);
    EXPECT_FALSE(bridge->base.system_a_connected);
    EXPECT_FALSE(bridge->base.system_b_connected);
    EXPECT_FALSE(bridge->base.bridge_active);
}

TEST_F(AmygdalaAttentionBridgeTest, CreateWithNullConfig) {
    amygdala_attention_bridge_t* test_bridge = amygdala_attention_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    EXPECT_TRUE(test_bridge->config.enable_threat_salience_boost);
    amygdala_attention_destroy(test_bridge);
}

TEST_F(AmygdalaAttentionBridgeTest, ResetBridge) {
    /* Set some state */
    amygdala_attention_connect_amygdala(bridge, amygdala);
    bridge->base.total_updates = 100;
    bridge->hypervigilance_activations = 10;

    /* Reset */
    int result = amygdala_attention_reset(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->base.total_updates, 0);
    EXPECT_EQ(bridge->hypervigilance_activations, 0);
    EXPECT_TRUE(bridge->base.system_a_connected); /* Connections preserved */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, ConnectAmygdala) {
    int result = amygdala_attention_connect_amygdala(bridge, amygdala);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->base.system_a_connected);
    EXPECT_EQ(bridge->base.system_a, amygdala);
}

TEST_F(AmygdalaAttentionBridgeTest, ConnectAttention) {
    void* dummy_attention = (void*)0x1234;
    int result = amygdala_attention_connect_attention(bridge, dummy_attention);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bridge->base.system_b_connected);
    EXPECT_EQ(bridge->base.system_b, dummy_attention);
}

TEST_F(AmygdalaAttentionBridgeTest, BridgeActivatesWhenBothConnected) {
    EXPECT_FALSE(bridge->base.bridge_active);

    amygdala_attention_connect_amygdala(bridge, amygdala);
    EXPECT_FALSE(bridge->base.bridge_active); /* Need both */

    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);
    EXPECT_TRUE(bridge->base.bridge_active); /* Both connected */
}

TEST_F(AmygdalaAttentionBridgeTest, DisconnectAmygdala) {
    amygdala_attention_connect_amygdala(bridge, amygdala);
    EXPECT_TRUE(bridge->base.system_a_connected);

    int result = amygdala_attention_disconnect_amygdala(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(bridge->base.system_a_connected);
    EXPECT_FALSE(bridge->base.bridge_active);
}

TEST_F(AmygdalaAttentionBridgeTest, IsConnected) {
    EXPECT_FALSE(amygdala_attention_is_connected(bridge));

    amygdala_attention_connect_amygdala(bridge, amygdala);
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    EXPECT_TRUE(amygdala_attention_is_connected(bridge));
}

/* ============================================================================
 * Amygdala → Attention Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, ThreatBoostMapping) {
    amygdala_attention_connect_amygdala(bridge, amygdala);

    /* Test different threat levels */
    struct {
        amyg_threat_level_t level;
        float expected_min_boost;
    } test_cases[] = {
        {AMYG_THREAT_NONE, 0.0f},
        {AMYG_THREAT_LOW, 0.08f},
        {AMYG_THREAT_MODERATE, 0.15f},
        {AMYG_THREAT_HIGH, 0.35f},
        {AMYG_THREAT_SEVERE, 0.70f}
    };

    for (auto& test : test_cases) {
        bridge->amygdala_effects.threat_level = test.level;
        bridge->amygdala_effects.fear_level = 0.0f;

        float boost = amygdala_attention_compute_threat_boost(bridge);
        EXPECT_GE(boost, test.expected_min_boost)
            << "Threat level " << test.level << " should produce boost >= " << test.expected_min_boost;
    }
}

TEST_F(AmygdalaAttentionBridgeTest, FearModulatesThreatBoost) {
    amygdala_attention_connect_amygdala(bridge, amygdala);

    bridge->amygdala_effects.threat_level = AMYG_THREAT_MODERATE;
    bridge->amygdala_effects.fear_level = 0.0f;
    float boost_no_fear = amygdala_attention_compute_threat_boost(bridge);

    bridge->amygdala_effects.fear_level = 0.8f;
    float boost_with_fear = amygdala_attention_compute_threat_boost(bridge);

    EXPECT_GT(boost_with_fear, boost_no_fear);
}

TEST_F(AmygdalaAttentionBridgeTest, HypervigilanceActivation) {
    amygdala_attention_connect_amygdala(bridge, amygdala);

    /* Low anxiety - no hypervigilance */
    amygdala_set_anxiety(amygdala, 0.5f);
    amygdala_attention_apply_amygdala_effects(bridge);
    EXPECT_FALSE(bridge->amygdala_effects.hypervigilance_active);

    /* High anxiety - hypervigilance */
    amygdala_set_anxiety(amygdala, 0.8f);
    amygdala_attention_apply_amygdala_effects(bridge);
    EXPECT_TRUE(bridge->amygdala_effects.hypervigilance_active);
}

TEST_F(AmygdalaAttentionBridgeTest, DisengagementDifficulty) {
    amygdala_attention_connect_amygdala(bridge, amygdala);

    /* Set moderate threat to indirectly affect fear */
    bridge->amygdala_effects.fear_level = 0.6f;
    bridge->amygdala_effects.hypervigilance_active = false;

    float difficulty = amygdala_attention_compute_disengagement_difficulty(bridge);
    EXPECT_GT(difficulty, 0.2f); /* Base + fear contribution */

    /* With hypervigilance, difficulty increases */
    bridge->amygdala_effects.hypervigilance_active = true;
    float difficulty_hv = amygdala_attention_compute_disengagement_difficulty(bridge);
    EXPECT_GT(difficulty_hv, difficulty);
}

TEST_F(AmygdalaAttentionBridgeTest, ApplyAmygdalaEffects) {
    amygdala_attention_connect_amygdala(bridge, amygdala);

    /* Set amygdala state */
    amygdala_set_anxiety(amygdala, 0.8f);

    int result = amygdala_attention_apply_amygdala_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Check that effects were computed */
    EXPECT_GE(bridge->amygdala_effects.vigilance_level, 0.0f);
    EXPECT_LE(bridge->amygdala_effects.vigilance_level, 1.0f);
}

/* ============================================================================
 * Attention → Amygdala Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, AttentionEnhancement) {
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    /* Set attention state */
    bridge->attention_effects.attention_strength = 0.8f;

    float enhancement = amygdala_attention_compute_attention_enhancement(bridge);
    EXPECT_GT(enhancement, AMYG_ATT_ATTENTION_ENHANCEMENT_BASE);
    EXPECT_LE(enhancement, 1.0f);
}

TEST_F(AmygdalaAttentionBridgeTest, DistractionSuppression) {
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    /* No neutral focus - no suppression */
    bridge->attention_effects.focus_on_neutral = 0.0f;
    float suppression_none = amygdala_attention_compute_distraction_suppression(bridge);
    EXPECT_FLOAT_EQ(suppression_none, 0.0f);

    /* High neutral focus - suppression */
    bridge->attention_effects.focus_on_neutral = 0.8f;
    float suppression_high = amygdala_attention_compute_distraction_suppression(bridge);
    EXPECT_GT(suppression_high, 0.0f);
}

TEST_F(AmygdalaAttentionBridgeTest, PrefrontalRegulation) {
    amygdala_attention_connect_amygdala(bridge, amygdala);
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    /* Set attention focused on neutral */
    bridge->attention_effects.focus_on_threat = 0.2f;
    bridge->attention_effects.focus_on_neutral = 0.8f;

    amygdala_attention_apply_attention_effects(bridge);

    /* Prefrontal regulation should be high */
    EXPECT_GT(bridge->attention_effects.prefrontal_regulation, 0.5f);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, UpdateRequiresBothConnections) {
    /* Only amygdala connected */
    amygdala_attention_connect_amygdala(bridge, amygdala);
    int result = amygdala_attention_update(bridge);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);

    /* Both connected */
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);
    result = amygdala_attention_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(AmygdalaAttentionBridgeTest, UpdateIncrementsStatistics) {
    amygdala_attention_connect_amygdala(bridge, amygdala);
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    uint64_t initial_updates = bridge->base.total_updates;

    amygdala_attention_update(bridge);

    EXPECT_EQ(bridge->base.total_updates, initial_updates + 1);
}

TEST_F(AmygdalaAttentionBridgeTest, ApplyModulation) {
    amygdala_attention_connect_amygdala(bridge, amygdala);
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    /* Set prefrontal regulation */
    bridge->attention_effects.prefrontal_regulation = 0.7f;

    int result = amygdala_attention_apply_modulation(bridge);
    EXPECT_EQ(result, 0);

    /* Check that amygdala received prefrontal inhibition */
    /* Note: This would require amygdala API to expose prefrontal_inhibition */
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, GetAmygdalaEffects) {
    amygdala_attention_connect_amygdala(bridge, amygdala);
    amygdala_set_anxiety(amygdala, 0.8f);
    amygdala_attention_apply_amygdala_effects(bridge);

    amygdala_attention_effects_t effects;
    int result = amygdala_attention_get_amygdala_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(effects.hypervigilance_active);
}

TEST_F(AmygdalaAttentionBridgeTest, GetAttentionEffects) {
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    bridge->attention_effects.attention_strength = 0.8f;
    amygdala_attention_apply_attention_effects(bridge);

    attention_amygdala_effects_t effects;
    int result = amygdala_attention_get_attention_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(effects.attention_strength, 0.8f);
}

TEST_F(AmygdalaAttentionBridgeTest, GetThreatSalienceBoost) {
    amygdala_attention_connect_amygdala(bridge, amygdala);

    /* Set fear level on amygdala directly - this updates threat level too */
    amygdala_set_fear_level(amygdala, 0.7f);

    /* Apply effects reads from amygdala and computes threat boost */
    amygdala_attention_apply_amygdala_effects(bridge);

    float boost = amygdala_attention_get_threat_salience_boost(bridge);
    EXPECT_GT(boost, 0.0f);
}

TEST_F(AmygdalaAttentionBridgeTest, GetStatistics) {
    amygdala_attention_connect_amygdala(bridge, amygdala);
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    /* Do some updates */
    amygdala_set_anxiety(amygdala, 0.8f);
    for (int i = 0; i < 5; i++) {
        amygdala_attention_update(bridge);
    }

    uint64_t total_updates;
    uint32_t hypervigilance_activations;
    uint32_t threat_boosts;
    uint32_t attention_enhancements;

    int result = amygdala_attention_get_statistics(
        bridge, &total_updates, &hypervigilance_activations,
        &threat_boosts, &attention_enhancements
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(total_updates, 5);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, ConnectBioAsync) {
    int result = amygdala_attention_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);
    /* Bio-async may not be available in test environment */
}

TEST_F(AmygdalaAttentionBridgeTest, DisconnectBioAsync) {
    amygdala_attention_connect_bio_async(bridge);
    int result = amygdala_attention_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(amygdala_attention_is_bio_async_connected(bridge));
}

TEST_F(AmygdalaAttentionBridgeTest, BioAsyncConnectionStatus) {
    EXPECT_FALSE(amygdala_attention_is_bio_async_connected(bridge));
    amygdala_attention_connect_bio_async(bridge);
    /* May or may not be connected depending on environment */
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(AmygdalaAttentionBridgeTest, FullPipeline) {
    /* Connect both systems */
    amygdala_attention_connect_amygdala(bridge, amygdala);
    void* dummy_attention = (void*)0x1234;
    amygdala_attention_connect_attention(bridge, dummy_attention);

    /* Set high anxiety (for hypervigilance) and fear (for threat boost) */
    amygdala_set_anxiety(amygdala, 0.8f);
    amygdala_set_fear_level(amygdala, 0.6f);

    /* Set attention state */
    bridge->attention_effects.attention_strength = 0.7f;
    bridge->attention_effects.focus_on_threat = 0.6f;
    bridge->attention_effects.focus_on_neutral = 0.4f;

    /* Update bridge */
    int result = amygdala_attention_update(bridge);
    EXPECT_EQ(result, 0);

    /* Check bidirectional effects */
    EXPECT_TRUE(bridge->amygdala_effects.hypervigilance_active);
    EXPECT_GT(bridge->amygdala_effects.threat_salience_boost, 0.0f);
    EXPECT_GT(bridge->attention_effects.attention_enhancement, 0.0f);

    /* Apply modulation */
    result = amygdala_attention_apply_modulation(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(AmygdalaAttentionBridgeTest, SensitivityModulation) {
    amygdala_attention_connect_amygdala(bridge, amygdala);

    bridge->amygdala_effects.threat_level = AMYG_THREAT_MODERATE;
    bridge->amygdala_effects.fear_level = 0.5f;

    /* Normal sensitivity */
    bridge->config.threat_sensitivity = 1.0f;
    float boost_normal = amygdala_attention_compute_threat_boost(bridge);

    /* High sensitivity */
    bridge->config.threat_sensitivity = 1.5f;
    float boost_high = amygdala_attention_compute_threat_boost(bridge);

    EXPECT_GT(boost_high, boost_normal);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
