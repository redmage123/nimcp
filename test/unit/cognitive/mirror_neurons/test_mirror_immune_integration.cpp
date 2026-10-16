/**
 * @file test_mirror_immune_integration.cpp
 * @brief Unit tests for Mirror Neuron - Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Tests bidirectional coupling between mirror neurons and brain immune system.
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/mirror_neurons/nimcp_mirror_immune_integration.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

class MirrorImmuneIntegrationTest : public ::testing::Test {
protected:
    mirror_neurons_t mirror_system;
    brain_immune_system_t* immune_system;
    mirror_immune_integration_t* integration;

    void SetUp() override {
        /* Create mirror neuron system */
        mirror_neuron_config_t mirror_config = mirror_neurons_get_default_config();
        mirror_config.max_actions = 50;
        mirror_system = mirror_neurons_create(&mirror_config);
        ASSERT_NE(mirror_system, nullptr);

        /* Enable resonance for suppression testing */
        ASSERT_TRUE(mirror_neurons_enable_resonance(mirror_system, 50));

        /* Create brain immune system */
        brain_immune_config_t immune_config;
        ASSERT_EQ(brain_immune_default_config(&immune_config), 0);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        ASSERT_EQ(brain_immune_start(immune_system), 0);

        /* Create integration (not enabled yet) */
        integration = nullptr;
    }

    void TearDown() override {
        if (integration) {
            mirror_immune_destroy(integration);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
        if (mirror_system) {
            mirror_neurons_destroy(mirror_system);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, DefaultConfiguration) {
    mirror_immune_config_t config;
    ASSERT_EQ(mirror_immune_get_default_config(&config), 0);

    /* Verify biological defaults */
    EXPECT_GT(config.cytokine_sensitivity, 0.0f);
    EXPECT_LE(config.cytokine_sensitivity, 1.0f);
    EXPECT_GT(config.max_resonance_suppression, 0.0f);
    EXPECT_TRUE(config.enable_sickness_behavior);
    EXPECT_TRUE(config.enable_isolation_detection);
    EXPECT_GT(config.isolation_threshold_s, 0.0f);
}

TEST_F(MirrorImmuneIntegrationTest, CreateDestroy) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);

    /* Verify initial state */
    EXPECT_EQ(mirror_immune_get_social_state(integration), SOCIAL_STATE_ENGAGED);
    EXPECT_EQ(mirror_immune_get_immune_effect(integration), IMMUNE_EFFECT_HEALTHY);
    EXPECT_FLOAT_EQ(mirror_immune_get_resonance_suppression(integration), 0.0f);
}

TEST_F(MirrorImmuneIntegrationTest, NullParameterHandling) {
    /* Null mirror system */
    integration = mirror_immune_create(nullptr, nullptr, immune_system);
    EXPECT_EQ(integration, nullptr);

    /* Null immune system */
    integration = mirror_immune_create(nullptr, mirror_system, nullptr);
    EXPECT_EQ(integration, nullptr);

    /* Both null */
    integration = mirror_immune_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(integration, nullptr);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, EnableDisable) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);

    /* Initially disabled */
    EXPECT_EQ(mirror_immune_enable(integration), 0);

    /* Can disable */
    EXPECT_EQ(mirror_immune_disable(integration), 0);

    /* Can re-enable */
    EXPECT_EQ(mirror_immune_enable(integration), 0);
}

/* ============================================================================
 * Immune → Mirror Modulation Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, CytokineSuppressionComputation) {
    mirror_immune_config_t config;
    mirror_immune_get_default_config(&config);
    integration = mirror_immune_create(&config, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Simulate high inflammation (will set cytokine levels) */
    /* Need multiple inflammation sites to overcome IL-10 anti-inflammatory effect */
    /* With inflammation_level = sites/10, need 6+ sites for net positive suppression */
    uint32_t antigen_id;
    uint8_t epitope[32] = {0x01, 0x02, 0x03};
    ASSERT_EQ(brain_immune_present_antigen(
        immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        8,  /* High severity */
        0,
        &antigen_id
    ), 0);

    /* Trigger multiple inflammation sites to reach meaningful inflammation level */
    for (int i = 0; i < 8; i++) {
        uint32_t site_id;
        ASSERT_EQ(brain_immune_initiate_inflammation(
            immune_system, i + 1, antigen_id, &site_id
        ), 0);
    }

    /* Update integration to sample immune state */
    uint64_t current_time = 1000000;  /* 1 second */
    ASSERT_EQ(mirror_immune_update(integration, current_time), 0);

    /* Should have some resonance suppression due to inflammation */
    float suppression = mirror_immune_get_resonance_suppression(integration);
    EXPECT_GT(suppression, 0.0f);
    EXPECT_LE(suppression, config.max_resonance_suppression);
}

TEST_F(MirrorImmuneIntegrationTest, SicknessBehavior) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Apply sickness behavior */
    float severity = 0.7f;
    EXPECT_EQ(mirror_immune_apply_sickness_behavior(integration, severity), 0);

    /* Verify immune effect updated */
    EXPECT_EQ(mirror_immune_get_immune_effect(integration), IMMUNE_EFFECT_SICKNESS);
}

TEST_F(MirrorImmuneIntegrationTest, SocialFunctionRestoration) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Apply sickness first */
    ASSERT_EQ(mirror_immune_apply_sickness_behavior(integration, 0.8f), 0);

    /* Restore with IL-10 */
    float il10_level = 0.9f;
    EXPECT_EQ(mirror_immune_restore_social_function(integration, il10_level), 0);

    /* Should be in recovery */
    EXPECT_EQ(mirror_immune_get_immune_effect(integration), IMMUNE_EFFECT_RECOVERY);
}

TEST_F(MirrorImmuneIntegrationTest, EmpathyThresholdModulation) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Get baseline threshold */
    float baseline = mirror_immune_compute_empathy_threshold(integration);
    EXPECT_GT(baseline, 0.0f);

    /* Simulate inflammation and recompute */
    ASSERT_EQ(mirror_immune_apply_sickness_behavior(integration, 0.5f), 0);
    ASSERT_EQ(mirror_immune_update(integration, 1000000), 0);

    float inflamed_threshold = mirror_immune_compute_empathy_threshold(integration);

    /* Threshold should increase with inflammation */
    EXPECT_GE(inflamed_threshold, baseline);
}

/* ============================================================================
 * Mirror → Immune Feedback Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, SocialIsolationDetection) {
    mirror_immune_config_t config;
    mirror_immune_get_default_config(&config);
    config.isolation_threshold_s = 5.0f;  /* 5 seconds for testing */
    integration = mirror_immune_create(&config, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Set a known baseline observation time (implementation uses real time internally) */
    uint64_t t0 = 1000000;  /* 1 second base */
    mirror_immune_notify_observation(integration, t0);

    /* Immediately after observation, not isolated */
    EXPECT_FALSE(mirror_immune_detect_isolation(integration, t0 + 1000));

    /* After threshold (5s), should detect isolation */
    uint64_t t_isolated = t0 + (uint64_t)(6.0f * 1000000);  /* 6 seconds after t0 */
    EXPECT_TRUE(mirror_immune_detect_isolation(integration, t_isolated));
}

TEST_F(MirrorImmuneIntegrationTest, IsolationTriggersIL6) {
    mirror_immune_config_t config;
    mirror_immune_get_default_config(&config);
    config.isolation_threshold_s = 1.0f;  /* 1 second */
    integration = mirror_immune_create(&config, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Get initial IL-6 count */
    mirror_immune_stats_t stats_before;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_before), 0);
    uint32_t il6_before = stats_before.il6_releases;

    /* Trigger isolation response */
    EXPECT_EQ(mirror_immune_trigger_isolation_response(integration), 0);

    /* Verify IL-6 was released */
    mirror_immune_stats_t stats_after;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_after), 0);
    EXPECT_GT(stats_after.il6_releases, il6_before);
    EXPECT_GT(stats_after.isolation_detections, 0u);
}

TEST_F(MirrorImmuneIntegrationTest, RejectionTriggersStressResponse) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Notify multiple failures */
    for (int i = 0; i < 5; i++) {
        mirror_immune_notify_imitation_failure(integration);
    }

    /* Get stats before */
    mirror_immune_stats_t stats_before;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_before), 0);

    /* Trigger rejection response */
    EXPECT_EQ(mirror_immune_trigger_rejection_response(integration), 0);

    /* Should have released IL-6 */
    mirror_immune_stats_t stats_after;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_after), 0);
    EXPECT_GT(stats_after.il6_releases, stats_before.il6_releases);
}

TEST_F(MirrorImmuneIntegrationTest, SocialSuccessReleasesIL10) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Get stats before */
    mirror_immune_stats_t stats_before;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_before), 0);

    /* Release IL-10 on success */
    EXPECT_EQ(mirror_immune_release_social_success_il10(integration), 0);

    /* Verify IL-10 release */
    mirror_immune_stats_t stats_after;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_after), 0);
    EXPECT_GT(stats_after.il10_releases, stats_before.il10_releases);
}

/* ============================================================================
 * Social State Tracking Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, SocialStateTransitions) {
    mirror_immune_config_t config;
    mirror_immune_get_default_config(&config);
    config.isolation_threshold_s = 2.0f;
    integration = mirror_immune_create(&config, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Start engaged */
    EXPECT_EQ(mirror_immune_get_social_state(integration), SOCIAL_STATE_ENGAGED);

    /* After isolation threshold, should be isolated */
    uint64_t t_isolated = 3000000;  /* 3 seconds */
    ASSERT_EQ(mirror_immune_update_social_state(integration, t_isolated), 0);
    EXPECT_EQ(mirror_immune_get_social_state(integration), SOCIAL_STATE_ISOLATED);

    /* Notify observation - should clear isolation */
    mirror_immune_notify_observation(integration, t_isolated);
    ASSERT_EQ(mirror_immune_update_social_state(integration, t_isolated + 100000), 0);
    EXPECT_NE(mirror_immune_get_social_state(integration), SOCIAL_STATE_ISOLATED);
}

TEST_F(MirrorImmuneIntegrationTest, RejectedStateOnFailures) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Set baseline observation time to prevent isolation detection overriding rejection */
    uint64_t current_time = 1000000;
    mirror_immune_notify_observation(integration, current_time);

    /* Notify many failures */
    for (int i = 0; i < 10; i++) {
        mirror_immune_notify_imitation_failure(integration);
    }

    /* Update state (shortly after observation so not isolated) */
    ASSERT_EQ(mirror_immune_update_social_state(integration, current_time + 1000), 0);

    /* Should be in rejected state */
    EXPECT_EQ(mirror_immune_get_social_state(integration), SOCIAL_STATE_REJECTED);
}

/* ============================================================================
 * Event Notification Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, ObservationNotification) {
    mirror_immune_config_t config;
    mirror_immune_get_default_config(&config);
    config.isolation_threshold_s = 1.0f;
    integration = mirror_immune_create(&config, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Wait long enough to be isolated */
    uint64_t t_before = 0;
    uint64_t t_isolated = 2000000;  /* 2 seconds */
    EXPECT_TRUE(mirror_immune_detect_isolation(integration, t_isolated));

    /* Notify observation */
    mirror_immune_notify_observation(integration, t_isolated);

    /* Should no longer be isolated shortly after */
    EXPECT_FALSE(mirror_immune_detect_isolation(integration, t_isolated + 10000));
}

TEST_F(MirrorImmuneIntegrationTest, ImitationSuccessNotification) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Get initial stats */
    mirror_immune_stats_t stats_before;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_before), 0);

    /* Notify success */
    uint64_t success_time = 1000000;
    mirror_immune_notify_imitation_success(integration, success_time);

    /* Should have triggered IL-10 release */
    mirror_immune_stats_t stats_after;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats_after), 0);
    EXPECT_GT(stats_after.il10_releases, stats_before.il10_releases);
}

TEST_F(MirrorImmuneIntegrationTest, FailureNotificationAccumulates) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Notify failures */
    for (int i = 0; i < 3; i++) {
        mirror_immune_notify_imitation_failure(integration);
    }

    /* Notify success - should reset counter */
    mirror_immune_notify_imitation_success(integration, 1000000);

    /* Notify more failures - starts from 0 again */
    for (int i = 0; i < 2; i++) {
        mirror_immune_notify_imitation_failure(integration);
    }

    /* Should need more failures to trigger rejection */
    EXPECT_EQ(mirror_immune_trigger_rejection_response(integration), 0);
}

/* ============================================================================
 * Integration Update Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, PeriodicUpdate) {
    mirror_immune_config_t config;
    mirror_immune_get_default_config(&config);
    config.update_interval_ms = 100;  /* 100ms updates */
    integration = mirror_immune_create(&config, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* First update */
    uint64_t t0 = 0;
    EXPECT_EQ(mirror_immune_update(integration, t0), 0);

    /* Too soon - should skip */
    uint64_t t1 = 50000;  /* 50ms */
    EXPECT_EQ(mirror_immune_update(integration, t1), 0);

    /* After interval - should process */
    uint64_t t2 = 200000;  /* 200ms */
    EXPECT_EQ(mirror_immune_update(integration, t2), 0);
}

TEST_F(MirrorImmuneIntegrationTest, DisabledUpdateDoesNothing) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);

    /* Don't enable */

    /* Update should return error */
    EXPECT_EQ(mirror_immune_update(integration, 1000000), -1);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, Statistics) {
    integration = mirror_immune_create(nullptr, mirror_system, immune_system);
    ASSERT_NE(integration, nullptr);
    ASSERT_EQ(mirror_immune_enable(integration), 0);

    /* Get initial stats */
    mirror_immune_stats_t stats;
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats), 0);

    /* All should be zero initially */
    EXPECT_EQ(stats.isolation_detections, 0u);
    EXPECT_EQ(stats.il6_releases, 0u);
    EXPECT_EQ(stats.il10_releases, 0u);
    EXPECT_EQ(stats.sickness_behavior_activations, 0u);

    /* Trigger some events */
    mirror_immune_trigger_isolation_response(integration);
    mirror_immune_release_social_success_il10(integration);
    mirror_immune_apply_sickness_behavior(integration, 0.5f);

    /* Get updated stats */
    ASSERT_EQ(mirror_immune_get_stats(integration, &stats), 0);
    EXPECT_GT(stats.isolation_detections, 0u);
    EXPECT_GT(stats.il6_releases, 0u);
    EXPECT_GT(stats.il10_releases, 0u);
    EXPECT_GT(stats.sickness_behavior_activations, 0u);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(MirrorImmuneIntegrationTest, SocialStateStrings) {
    EXPECT_STREQ(mirror_social_state_to_string(SOCIAL_STATE_ENGAGED), "ENGAGED");
    EXPECT_STREQ(mirror_social_state_to_string(SOCIAL_STATE_PASSIVE), "PASSIVE");
    EXPECT_STREQ(mirror_social_state_to_string(SOCIAL_STATE_ISOLATED), "ISOLATED");
    EXPECT_STREQ(mirror_social_state_to_string(SOCIAL_STATE_REJECTED), "REJECTED");
}

TEST_F(MirrorImmuneIntegrationTest, ImmuneEffectStrings) {
    EXPECT_STREQ(mirror_immune_effect_to_string(IMMUNE_EFFECT_NONE), "NONE");
    EXPECT_STREQ(mirror_immune_effect_to_string(IMMUNE_EFFECT_SICKNESS), "SICKNESS");
    EXPECT_STREQ(mirror_immune_effect_to_string(IMMUNE_EFFECT_STRESS), "STRESS");
    EXPECT_STREQ(mirror_immune_effect_to_string(IMMUNE_EFFECT_RECOVERY), "RECOVERY");
    EXPECT_STREQ(mirror_immune_effect_to_string(IMMUNE_EFFECT_HEALTHY), "HEALTHY");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
