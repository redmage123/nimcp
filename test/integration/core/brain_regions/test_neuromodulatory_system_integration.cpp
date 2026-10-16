/**
 * @file test_neuromodulatory_system_integration.cpp
 * @brief Integration tests for Neuromodulatory-Security, Neuromodulatory-Immune,
 *        and Neuromodulatory-Logging bridges
 * @date 2026-01-11
 *
 * Tests bidirectional communication between Phase 4 neuromodulatory modules
 * (LC, VTA, Raphe, Habenula) and core system components (Security, Immune, Logging).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/regions/nimcp_neuromodulatory_security_bridge.h"
#include "core/brain/regions/nimcp_neuromodulatory_immune_bridge.h"
#include "core/brain/regions/nimcp_neuromodulatory_logging_bridge.h"
}

/* ============================================================================
 * Security Bridge Tests
 * ============================================================================ */

class NeuromodSecurityBridgeTest : public ::testing::Test {
protected:
    neuromod_security_bridge_t* bridge;
    neuromod_security_bridge_config_t config;

    void SetUp() override {
        neuromod_security_bridge_default_config(&config);
        bridge = neuromod_security_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_security_bridge_destroy(bridge);
        }
    }
};

TEST_F(NeuromodSecurityBridgeTest, CreateAndDestroy) {
    ASSERT_NE(bridge, nullptr);
    neuromod_security_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(NeuromodSecurityBridgeTest, DefaultConfig) {
    neuromod_security_bridge_config_t cfg;
    int result = neuromod_security_bridge_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_lc_security_modulation);
    EXPECT_TRUE(cfg.enable_vta_security_modulation);
    EXPECT_TRUE(cfg.enable_raphe_security_modulation);
    EXPECT_TRUE(cfg.enable_habenula_security_modulation);
    EXPECT_TRUE(cfg.enable_threat_feedback);
    EXPECT_GT(cfg.ne_sensitivity_weight, 0.0f);
    EXPECT_GT(cfg.update_interval_ms, 0.0f);
}

TEST_F(NeuromodSecurityBridgeTest, Connection) {
    EXPECT_FALSE(neuromod_security_bridge_is_connected(bridge));

    /* Connect with NULL security context (allowed for testing) */
    int result = neuromod_security_bridge_connect_security(bridge, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(neuromod_security_bridge_is_connected(bridge));

    result = neuromod_security_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(neuromod_security_bridge_is_connected(bridge));
}

TEST_F(NeuromodSecurityBridgeTest, AdapterRegistration) {
    EXPECT_EQ(neuromod_security_bridge_register_lc(bridge, nullptr), 0);
    EXPECT_EQ(neuromod_security_bridge_register_vta(bridge, nullptr), 0);
    EXPECT_EQ(neuromod_security_bridge_register_raphe(bridge, nullptr), 0);
    EXPECT_EQ(neuromod_security_bridge_register_habenula(bridge, nullptr), 0);
}

TEST_F(NeuromodSecurityBridgeTest, ApplyArousal) {
    neuromod_security_bridge_connect_security(bridge, nullptr);

    neuromod_sec_arousal_payload_t payload = {
        .arousal_level = 0.8f,
        .vigilance = 0.9f,
        .sensitivity_boost = 0.5f,
        .phasic_burst = true,
        .timestamp = 12345
    };

    int result = neuromod_security_apply_arousal(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_security_modulation_t modulation;
    result = neuromod_security_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_GT(modulation.threat_sensitivity_boost, 1.0f);  /* Should be boosted */
    EXPECT_FLOAT_EQ(modulation.vigilance_level, 0.9f);
    EXPECT_TRUE(modulation.phasic_alert_active);
}

TEST_F(NeuromodSecurityBridgeTest, ApplyLearning) {
    neuromod_security_bridge_connect_security(bridge, nullptr);

    neuromod_sec_learning_payload_t payload = {
        .rpe = 0.5f,
        .learning_rate = 1.2f,
        .adaptation_rate = 1.1f,
        .positive_outcome = true,
        .pattern_id = 42,
        .timestamp = 12345
    };

    int result = neuromod_security_apply_learning(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_security_modulation_t modulation;
    result = neuromod_security_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_GT(modulation.pattern_learning_rate, 1.0f);
    EXPECT_TRUE(modulation.safety_confirmed);
}

TEST_F(NeuromodSecurityBridgeTest, ApplyPatience) {
    neuromod_security_bridge_connect_security(bridge, nullptr);

    neuromod_sec_patience_payload_t payload = {
        .patience = 0.8f,
        .impulse_inhibition = 0.7f,
        .tolerance_boost = 0.3f,
        .timestamp = 12345
    };

    int result = neuromod_security_apply_patience(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_security_modulation_t modulation;
    result = neuromod_security_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_GT(modulation.tolerance_window, 1.0f);
    EXPECT_FLOAT_EQ(modulation.patience_level, 0.8f);
}

TEST_F(NeuromodSecurityBridgeTest, ApplyAversive) {
    neuromod_security_bridge_connect_security(bridge, nullptr);

    neuromod_sec_aversive_payload_t payload = {
        .punishment_strength = 0.9f,
        .avoidance_drive = 0.8f,
        .quarantine_request = true,
        .threat_id = 123,
        .timestamp = 12345
    };

    int result = neuromod_security_apply_aversive(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_security_modulation_t modulation;
    result = neuromod_security_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_GT(modulation.defensive_boost, 1.0f);
    EXPECT_TRUE(modulation.quarantine_mode);
}

TEST_F(NeuromodSecurityBridgeTest, ReportThreat) {
    neuromod_security_bridge_connect_security(bridge, nullptr);

    neuromod_sec_threat_payload_t payload = {
        .threat_level = 0.75f,
        .threat_count = 3,
        .threat_type = 1,
        .urgent = true,
        .timestamp = 12345
    };

    int result = neuromod_security_report_threat(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_security_feedback_t feedback;
    result = neuromod_security_bridge_get_feedback(bridge, &feedback);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(feedback.current_threat_level, 0.75f);
    EXPECT_EQ(feedback.threats_detected, 3u);
    EXPECT_TRUE(feedback.under_attack);
}

TEST_F(NeuromodSecurityBridgeTest, Statistics) {
    neuromod_security_bridge_connect_security(bridge, nullptr);

    neuromod_sec_arousal_payload_t arousal = {.arousal_level = 0.5f};
    neuromod_security_apply_arousal(bridge, &arousal);

    neuromod_sec_learning_payload_t learning = {.rpe = 0.3f};
    neuromod_security_apply_learning(bridge, &learning);

    neuromod_security_bridge_stats_t stats;
    int result = neuromod_security_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.lc_modulations_sent, 1u);
    EXPECT_EQ(stats.vta_modulations_sent, 1u);
    EXPECT_EQ(stats.total_events_sent, 2u);
}

TEST_F(NeuromodSecurityBridgeTest, EventNames) {
    EXPECT_STREQ(neuromod_sec_event_name(NEUROMOD_SEC_EVENT_AROUSAL_BOOST), "AROUSAL_BOOST");
    EXPECT_STREQ(neuromod_sec_event_name(NEUROMOD_SEC_EVENT_PATTERN_LEARN), "PATTERN_LEARN");
    EXPECT_STREQ(neuromod_sec_event_name(NEUROMOD_SEC_EVENT_PATIENCE_INCREASE), "PATIENCE_INCREASE");
    EXPECT_STREQ(neuromod_sec_event_name(NEUROMOD_SEC_EVENT_AVERSIVE_TRIGGER), "AVERSIVE_TRIGGER");
}

/* ============================================================================
 * Immune Bridge Tests
 * ============================================================================ */

class NeuromodImmuneBridgeTest : public ::testing::Test {
protected:
    neuromod_immune_bridge_t* bridge;
    neuromod_immune_bridge_config_t config;

    void SetUp() override {
        neuromod_immune_bridge_default_config(&config);
        bridge = neuromod_immune_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_immune_bridge_destroy(bridge);
        }
    }
};

TEST_F(NeuromodImmuneBridgeTest, CreateAndDestroy) {
    ASSERT_NE(bridge, nullptr);
    neuromod_immune_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(NeuromodImmuneBridgeTest, DefaultConfig) {
    neuromod_immune_bridge_config_t cfg;
    int result = neuromod_immune_bridge_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_lc_immune_modulation);
    EXPECT_TRUE(cfg.enable_vta_immune_modulation);
    EXPECT_TRUE(cfg.enable_raphe_immune_modulation);
    EXPECT_TRUE(cfg.enable_habenula_immune_modulation);
    EXPECT_TRUE(cfg.enable_cytokine_feedback);
    EXPECT_GT(cfg.chronic_stress_threshold_ms, 0.0f);
}

TEST_F(NeuromodImmuneBridgeTest, Connection) {
    EXPECT_FALSE(neuromod_immune_bridge_is_connected(bridge));

    int result = neuromod_immune_bridge_connect_immune(bridge, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(neuromod_immune_bridge_is_connected(bridge));

    result = neuromod_immune_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(neuromod_immune_bridge_is_connected(bridge));
}

TEST_F(NeuromodImmuneBridgeTest, ApplyAcuteStress) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    neuromod_imm_ne_payload_t payload = {
        .ne_level = 0.9f,
        .stress_duration_ms = 5000.0f,  /* Short = acute */
        .mobilization_strength = 0.8f,
        .phasic_response = true,
        .timestamp = 12345
    };

    int result = neuromod_immune_apply_ne_stress(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_immune_modulation_t modulation;
    result = neuromod_immune_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(modulation.ne_level, 0.9f);
    EXPECT_TRUE(modulation.acute_stress_mode);
    EXPECT_FALSE(modulation.chronic_stress_mode);
    EXPECT_GT(modulation.acute_stress_mobilization, 0.0f);
}

TEST_F(NeuromodImmuneBridgeTest, ApplyChronicStress) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    neuromod_imm_ne_payload_t payload = {
        .ne_level = 0.8f,
        .stress_duration_ms = 60000.0f,  /* Long = chronic */
        .mobilization_strength = 0.7f,
        .phasic_response = false,
        .timestamp = 12345
    };

    int result = neuromod_immune_apply_ne_stress(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_immune_modulation_t modulation;
    result = neuromod_immune_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(modulation.acute_stress_mode);
    EXPECT_TRUE(modulation.chronic_stress_mode);
    EXPECT_GT(modulation.chronic_stress_suppression, 0.0f);
}

TEST_F(NeuromodImmuneBridgeTest, ApplyRewardState) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    neuromod_imm_da_payload_t payload = {
        .da_level = 0.85f,
        .rpe = 0.5f,
        .motivation = 0.9f,
        .positive_outcome = true,
        .timestamp = 12345
    };

    int result = neuromod_immune_apply_da_reward(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_immune_modulation_t modulation;
    result = neuromod_immune_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(modulation.da_level, 0.85f);
    EXPECT_TRUE(modulation.positive_affect);
    EXPECT_GT(modulation.reward_anti_inflammatory, 0.0f);
}

TEST_F(NeuromodImmuneBridgeTest, ApplyMood) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    neuromod_imm_ht_payload_t payload = {
        .ht_level = 0.7f,
        .mood_valence = 0.6f,  /* Positive mood */
        .social_context = 0.5f,
        .timestamp = 12345
    };

    int result = neuromod_immune_apply_ht_mood(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_immune_modulation_t modulation;
    result = neuromod_immune_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(modulation.ht_level, 0.7f);
    EXPECT_TRUE(modulation.good_mood);
    EXPECT_GT(modulation.mood_anti_inflammatory, 0.0f);
}

TEST_F(NeuromodImmuneBridgeTest, ApplyAversion) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    neuromod_imm_hab_payload_t payload = {
        .habenula_activation = 0.8f,
        .aversion_duration_ms = 10000.0f,
        .suppression_strength = 0.6f,
        .timestamp = 12345
    };

    int result = neuromod_immune_apply_hab_aversion(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_immune_modulation_t modulation;
    result = neuromod_immune_bridge_get_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(modulation.habenula_activation, 0.8f);
    EXPECT_GT(modulation.chronic_aversion_suppression, 0.0f);
}

TEST_F(NeuromodImmuneBridgeTest, ReportCytokines) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    neuromod_imm_cytokine_payload_t payload = {
        .il1_level = 0.6f,
        .il6_level = 0.5f,
        .il10_level = 0.3f,
        .tnf_level = 0.4f,
        .ifn_level = 0.2f,
        .inflammation_level = 0.5f,
        .urgent = false,
        .timestamp = 12345
    };

    int result = neuromod_immune_report_cytokines(bridge, &payload);
    EXPECT_EQ(result, 0);

    neuromod_immune_feedback_t feedback;
    result = neuromod_immune_bridge_get_feedback(bridge, &feedback);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(feedback.il1_level, 0.6f);
    EXPECT_FLOAT_EQ(feedback.inflammation_level, 0.5f);
    EXPECT_GT(feedback.fatigue_induction, 0.0f);
}

TEST_F(NeuromodImmuneBridgeTest, CytokineModulation) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    /* Apply positive state (DA) - should boost IL-10, reduce TNF */
    neuromod_imm_da_payload_t da_payload = {
        .da_level = 0.9f,
        .rpe = 0.5f,
        .motivation = 0.9f,
        .positive_outcome = true,
        .timestamp = 12345
    };
    neuromod_immune_apply_da_reward(bridge, &da_payload);

    neuromod_immune_modulation_t modulation;
    neuromod_immune_bridge_get_modulation(bridge, &modulation);

    EXPECT_GT(modulation.il10_modulation, 1.0f);  /* Anti-inflammatory boosted */
    EXPECT_LT(modulation.tnf_modulation, 1.0f);   /* TNF suppressed */
}

TEST_F(NeuromodImmuneBridgeTest, Statistics) {
    neuromod_immune_bridge_connect_immune(bridge, nullptr);

    neuromod_imm_ne_payload_t ne = {.ne_level = 0.5f, .stress_duration_ms = 1000.0f};
    neuromod_immune_apply_ne_stress(bridge, &ne);

    neuromod_imm_da_payload_t da = {.da_level = 0.5f};
    neuromod_immune_apply_da_reward(bridge, &da);

    neuromod_immune_bridge_stats_t stats;
    int result = neuromod_immune_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.lc_immune_modulations, 1u);
    EXPECT_EQ(stats.vta_immune_modulations, 1u);
    EXPECT_EQ(stats.total_events_sent, 2u);
}

TEST_F(NeuromodImmuneBridgeTest, EventNames) {
    EXPECT_STREQ(neuromod_imm_event_name(NEUROMOD_IMM_EVENT_NE_ACUTE_STRESS), "NE_ACUTE_STRESS");
    EXPECT_STREQ(neuromod_imm_event_name(NEUROMOD_IMM_EVENT_DA_REWARD_STATE), "DA_REWARD_STATE");
    EXPECT_STREQ(neuromod_imm_event_name(NEUROMOD_IMM_EVENT_5HT_MOOD_POSITIVE), "5HT_MOOD_POSITIVE");
    EXPECT_STREQ(neuromod_imm_event_name(NEUROMOD_IMM_EVENT_IL1_FATIGUE), "IL1_FATIGUE");
}

/* ============================================================================
 * Logging Bridge Tests
 * ============================================================================ */

class NeuromodLoggingBridgeTest : public ::testing::Test {
protected:
    neuromod_logging_bridge_t* bridge;
    neuromod_logging_bridge_config_t config;

    void SetUp() override {
        neuromod_logging_bridge_default_config(&config);
        bridge = neuromod_logging_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_logging_bridge_destroy(bridge);
        }
    }
};

TEST_F(NeuromodLoggingBridgeTest, CreateAndDestroy) {
    ASSERT_NE(bridge, nullptr);
    neuromod_logging_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(NeuromodLoggingBridgeTest, DefaultConfig) {
    neuromod_logging_bridge_config_t cfg;
    int result = neuromod_logging_bridge_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_lc_logging);
    EXPECT_TRUE(cfg.enable_vta_logging);
    EXPECT_TRUE(cfg.enable_raphe_logging);
    EXPECT_TRUE(cfg.enable_habenula_logging);
    EXPECT_TRUE(cfg.enable_pattern_analysis);
    EXPECT_GT(cfg.log_buffer_size, 0u);
}

TEST_F(NeuromodLoggingBridgeTest, Connection) {
    EXPECT_FALSE(neuromod_logging_bridge_is_connected(bridge));

    int result = neuromod_logging_bridge_connect_logging(bridge, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(neuromod_logging_bridge_is_connected(bridge));

    result = neuromod_logging_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(neuromod_logging_bridge_is_connected(bridge));
}

TEST_F(NeuromodLoggingBridgeTest, AdapterRegistration) {
    EXPECT_EQ(neuromod_logging_bridge_register_lc(bridge, nullptr), 0);
    EXPECT_EQ(neuromod_logging_bridge_register_vta(bridge, nullptr), 0);
    EXPECT_EQ(neuromod_logging_bridge_register_raphe(bridge, nullptr), 0);
    EXPECT_EQ(neuromod_logging_bridge_register_habenula(bridge, nullptr), 0);
}

TEST_F(NeuromodLoggingBridgeTest, LogLcEvent) {
    neuromod_logging_bridge_connect_logging(bridge, nullptr);

    int result = neuromod_logging_log_lc_event(bridge,
        NEUROMOD_LOG_EVENT_AROUSAL_CHANGE,
        NEUROMOD_LOG_LEVEL_INFO,
        0.8f, 0.5f, "Arousal increased");
    EXPECT_EQ(result, 0);

    neuromod_logging_bridge_stats_t stats;
    result = neuromod_logging_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.lc_events_logged, 1u);
    EXPECT_EQ(stats.info_events, 1u);
}

TEST_F(NeuromodLoggingBridgeTest, LogVtaEvent) {
    neuromod_logging_bridge_connect_logging(bridge, nullptr);

    int result = neuromod_logging_log_vta_event(bridge,
        NEUROMOD_LOG_EVENT_RPE_POSITIVE,
        NEUROMOD_LOG_LEVEL_INFO,
        0.7f, 0.4f, "Positive RPE detected");
    EXPECT_EQ(result, 0);

    neuromod_logging_bridge_stats_t stats;
    result = neuromod_logging_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.vta_events_logged, 1u);
}

TEST_F(NeuromodLoggingBridgeTest, LogRapheEvent) {
    neuromod_logging_bridge_connect_logging(bridge, nullptr);

    int result = neuromod_logging_log_raphe_event(bridge,
        NEUROMOD_LOG_EVENT_MOOD_POSITIVE,
        NEUROMOD_LOG_LEVEL_INFO,  /* Use INFO to pass default filter */
        0.6f, 0.5f, "Mood elevated");
    EXPECT_EQ(result, 0);

    neuromod_logging_bridge_stats_t stats;
    result = neuromod_logging_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.raphe_events_logged, 1u);
}

TEST_F(NeuromodLoggingBridgeTest, LogHabenulaEvent) {
    neuromod_logging_bridge_connect_logging(bridge, nullptr);

    int result = neuromod_logging_log_habenula_event(bridge,
        NEUROMOD_LOG_EVENT_PUNISHMENT_DETECTED,
        NEUROMOD_LOG_LEVEL_WARN,
        0.85f, 0.9f, "Punishment signal");
    EXPECT_EQ(result, 0);

    neuromod_logging_bridge_stats_t stats;
    result = neuromod_logging_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.habenula_events_logged, 1u);
    EXPECT_EQ(stats.warn_events, 1u);
}

TEST_F(NeuromodLoggingBridgeTest, LogLevelFilter) {
    /* Set minimum level to INFO - TRACE and DEBUG should be filtered */
    config.min_log_level = NEUROMOD_LOG_LEVEL_INFO;
    neuromod_logging_bridge_destroy(bridge);
    bridge = neuromod_logging_bridge_create(&config);
    neuromod_logging_bridge_connect_logging(bridge, nullptr);

    /* TRACE should be filtered */
    neuromod_logging_log_lc_event(bridge,
        NEUROMOD_LOG_EVENT_AROUSAL_CHANGE,
        NEUROMOD_LOG_LEVEL_TRACE,
        0.5f, 0.3f, "Trace event");

    /* INFO should pass */
    neuromod_logging_log_lc_event(bridge,
        NEUROMOD_LOG_EVENT_AROUSAL_CHANGE,
        NEUROMOD_LOG_LEVEL_INFO,
        0.6f, 0.4f, "Info event");

    neuromod_logging_bridge_stats_t stats;
    neuromod_logging_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.trace_events, 0u);  /* Filtered */
    EXPECT_EQ(stats.info_events, 1u);   /* Passed */
    EXPECT_EQ(stats.total_events_logged, 1u);
}

TEST_F(NeuromodLoggingBridgeTest, FlushBuffer) {
    neuromod_logging_bridge_connect_logging(bridge, nullptr);

    /* Log some events */
    for (int i = 0; i < 5; i++) {
        neuromod_logging_log_lc_event(bridge,
            NEUROMOD_LOG_EVENT_AROUSAL_CHANGE,
            NEUROMOD_LOG_LEVEL_INFO,
            0.5f + i * 0.1f, 0.3f, "Test event");
    }

    int result = neuromod_logging_flush(bridge);
    EXPECT_EQ(result, 0);

    neuromod_logging_bridge_stats_t stats;
    neuromod_logging_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.flushes_performed, 1u);
}

TEST_F(NeuromodLoggingBridgeTest, PatternAnalysis) {
    neuromod_logging_bridge_connect_logging(bridge, nullptr);

    /* Log many similar events to create a pattern */
    for (int i = 0; i < 50; i++) {
        neuromod_logging_log_lc_event(bridge,
            NEUROMOD_LOG_EVENT_AROUSAL_CHANGE,
            NEUROMOD_LOG_LEVEL_INFO,
            0.5f, 0.3f, "Repeated arousal");
    }

    int result = neuromod_logging_analyze_patterns(bridge);
    EXPECT_EQ(result, 0);

    uint32_t pattern_count = neuromod_logging_get_pattern_count(bridge);
    EXPECT_GT(pattern_count, 0u);  /* Should detect the pattern */

    if (pattern_count > 0) {
        neuromod_log_pattern_t pattern;
        result = neuromod_logging_get_pattern(bridge, 0, &pattern);
        EXPECT_EQ(result, 0);
        EXPECT_GT(pattern.confidence, 0.0f);
    }
}

TEST_F(NeuromodLoggingBridgeTest, CategoryNames) {
    EXPECT_STREQ(neuromod_log_category_name(NEUROMOD_LOG_CAT_LC), "LC");
    EXPECT_STREQ(neuromod_log_category_name(NEUROMOD_LOG_CAT_VTA), "VTA");
    EXPECT_STREQ(neuromod_log_category_name(NEUROMOD_LOG_CAT_RAPHE), "RAPHE");
    EXPECT_STREQ(neuromod_log_category_name(NEUROMOD_LOG_CAT_HABENULA), "HABENULA");
}

TEST_F(NeuromodLoggingBridgeTest, LevelNames) {
    EXPECT_STREQ(neuromod_log_level_name(NEUROMOD_LOG_LEVEL_TRACE), "TRACE");
    EXPECT_STREQ(neuromod_log_level_name(NEUROMOD_LOG_LEVEL_DEBUG), "DEBUG");
    EXPECT_STREQ(neuromod_log_level_name(NEUROMOD_LOG_LEVEL_INFO), "INFO");
    EXPECT_STREQ(neuromod_log_level_name(NEUROMOD_LOG_LEVEL_WARN), "WARN");
    EXPECT_STREQ(neuromod_log_level_name(NEUROMOD_LOG_LEVEL_ERROR), "ERROR");
    EXPECT_STREQ(neuromod_log_level_name(NEUROMOD_LOG_LEVEL_CRITICAL), "CRITICAL");
}

TEST_F(NeuromodLoggingBridgeTest, EventNames) {
    EXPECT_STREQ(neuromod_log_event_name(NEUROMOD_LOG_EVENT_AROUSAL_CHANGE), "AROUSAL_CHANGE");
    EXPECT_STREQ(neuromod_log_event_name(NEUROMOD_LOG_EVENT_PHASIC_BURST), "PHASIC_BURST");
    EXPECT_STREQ(neuromod_log_event_name(NEUROMOD_LOG_EVENT_RPE_POSITIVE), "RPE_POSITIVE");
    EXPECT_STREQ(neuromod_log_event_name(NEUROMOD_LOG_EVENT_MOOD_POSITIVE), "MOOD_POSITIVE");
    EXPECT_STREQ(neuromod_log_event_name(NEUROMOD_LOG_EVENT_PUNISHMENT_DETECTED), "PUNISHMENT_DETECTED");
}

/* ============================================================================
 * Cross-Bridge Integration Tests
 * ============================================================================ */

class NeuromodCrossBridgeTest : public ::testing::Test {
protected:
    neuromod_security_bridge_t* sec_bridge;
    neuromod_immune_bridge_t* imm_bridge;
    neuromod_logging_bridge_t* log_bridge;

    void SetUp() override {
        neuromod_security_bridge_config_t sec_config;
        neuromod_security_bridge_default_config(&sec_config);
        sec_bridge = neuromod_security_bridge_create(&sec_config);

        neuromod_immune_bridge_config_t imm_config;
        neuromod_immune_bridge_default_config(&imm_config);
        imm_bridge = neuromod_immune_bridge_create(&imm_config);

        neuromod_logging_bridge_config_t log_config;
        neuromod_logging_bridge_default_config(&log_config);
        log_bridge = neuromod_logging_bridge_create(&log_config);
    }

    void TearDown() override {
        if (sec_bridge) neuromod_security_bridge_destroy(sec_bridge);
        if (imm_bridge) neuromod_immune_bridge_destroy(imm_bridge);
        if (log_bridge) neuromod_logging_bridge_destroy(log_bridge);
    }
};

TEST_F(NeuromodCrossBridgeTest, AllBridgesCreateSuccessfully) {
    ASSERT_NE(sec_bridge, nullptr);
    ASSERT_NE(imm_bridge, nullptr);
    ASSERT_NE(log_bridge, nullptr);
}

TEST_F(NeuromodCrossBridgeTest, HighStressScenario) {
    /* Simulate high stress scenario affecting all systems */
    neuromod_security_bridge_connect_security(sec_bridge, nullptr);
    neuromod_immune_bridge_connect_immune(imm_bridge, nullptr);
    neuromod_logging_bridge_connect_logging(log_bridge, nullptr);

    /* LC arousal (high NE) */
    neuromod_sec_arousal_payload_t arousal = {
        .arousal_level = 0.95f,
        .vigilance = 0.9f,
        .sensitivity_boost = 0.5f,
        .phasic_burst = true
    };
    neuromod_security_apply_arousal(sec_bridge, &arousal);

    neuromod_imm_ne_payload_t ne_stress = {
        .ne_level = 0.95f,
        .stress_duration_ms = 5000.0f,
        .mobilization_strength = 0.8f
    };
    neuromod_immune_apply_ne_stress(imm_bridge, &ne_stress);

    neuromod_logging_log_lc_event(log_bridge,
        NEUROMOD_LOG_EVENT_STRESS_ONSET,
        NEUROMOD_LOG_LEVEL_WARN,
        0.95f, 0.9f, "High stress detected");

    /* Verify modulation states */
    neuromod_security_modulation_t sec_mod;
    neuromod_security_bridge_get_modulation(sec_bridge, &sec_mod);
    EXPECT_GT(sec_mod.threat_sensitivity_boost, 1.0f);

    neuromod_immune_modulation_t imm_mod;
    neuromod_immune_bridge_get_modulation(imm_bridge, &imm_mod);
    EXPECT_TRUE(imm_mod.acute_stress_mode);
    EXPECT_GT(imm_mod.il6_modulation, 1.0f);  /* Stress mobilization */

    neuromod_logging_bridge_stats_t log_stats;
    neuromod_logging_bridge_get_stats(log_bridge, &log_stats);
    EXPECT_EQ(log_stats.lc_events_logged, 1u);
}

TEST_F(NeuromodCrossBridgeTest, PositiveAffectScenario) {
    /* Simulate positive reward state affecting all systems */
    neuromod_security_bridge_connect_security(sec_bridge, nullptr);
    neuromod_immune_bridge_connect_immune(imm_bridge, nullptr);
    neuromod_logging_bridge_connect_logging(log_bridge, nullptr);

    /* VTA reward (high DA, positive RPE) */
    neuromod_sec_learning_payload_t learning = {
        .rpe = 0.7f,
        .learning_rate = 1.5f,
        .adaptation_rate = 1.2f,
        .positive_outcome = true
    };
    neuromod_security_apply_learning(sec_bridge, &learning);

    neuromod_imm_da_payload_t da_reward = {
        .da_level = 0.9f,
        .rpe = 0.7f,
        .motivation = 0.95f,
        .positive_outcome = true
    };
    neuromod_immune_apply_da_reward(imm_bridge, &da_reward);

    neuromod_logging_log_vta_event(log_bridge,
        NEUROMOD_LOG_EVENT_REWARD_RECEIVED,
        NEUROMOD_LOG_LEVEL_INFO,
        0.9f, 0.7f, "Positive reward signal");

    /* Verify modulation states */
    neuromod_security_modulation_t sec_mod;
    neuromod_security_bridge_get_modulation(sec_bridge, &sec_mod);
    EXPECT_TRUE(sec_mod.safety_confirmed);

    neuromod_immune_modulation_t imm_mod;
    neuromod_immune_bridge_get_modulation(imm_bridge, &imm_mod);
    EXPECT_TRUE(imm_mod.positive_affect);
    EXPECT_GT(imm_mod.il10_modulation, 1.0f);  /* Anti-inflammatory */

    neuromod_logging_bridge_stats_t log_stats;
    neuromod_logging_bridge_get_stats(log_bridge, &log_stats);
    EXPECT_EQ(log_stats.vta_events_logged, 1u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
