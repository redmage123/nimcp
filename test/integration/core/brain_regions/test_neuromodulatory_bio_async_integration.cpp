/**
 * @file test_neuromodulatory_bio_async_integration.cpp
 * @brief Integration tests for neuromodulatory bio-async and cognitive hub bridges
 *
 * Tests complete integration between neuromodulatory centers and:
 * - Bio-async messaging system (LC, VTA, Raphe, Habenula bridges)
 * - Cognitive integration hub
 * - Cross-neuromodulatory coordination
 *
 * @version 1.0.0
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Bio-async bridges
#include "core/brain/regions/locus_coeruleus/nimcp_lc_bio_async_bridge.h"
#include "core/brain/regions/vta/nimcp_vta_bio_async_bridge.h"
#include "core/brain/regions/raphe/nimcp_raphe_bio_async_bridge.h"
#include "core/brain/regions/habenula/nimcp_habenula_bio_async_bridge.h"

// Cognitive hub bridge
#include "core/brain/regions/nimcp_neuromodulatory_cognitive_bridge.h"

//=============================================================================
// LC Bio-Async Bridge Tests
//=============================================================================

class LCBioAsyncIntegration : public ::testing::Test {
protected:
    lc_bio_async_bridge_t* bridge = nullptr;

    void SetUp() override {
        lc_bio_async_config_t config;
        lc_bio_async_default_config(&config);
        bridge = lc_bio_async_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            lc_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(LCBioAsyncIntegration, BridgeCreation) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_FALSE(lc_bio_async_is_connected(bridge));
}

TEST_F(LCBioAsyncIntegration, DefaultConfig) {
    lc_bio_async_config_t config;
    int ret = lc_bio_async_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_auto_broadcast);
    EXPECT_TRUE(config.enable_gain_modulation);
    EXPECT_TRUE(config.enable_stress_routing);
    EXPECT_TRUE(config.enable_plasticity_gating);
    EXPECT_EQ(config.default_channel, BIO_CHANNEL_NOREPINEPHRINE);
}

TEST_F(LCBioAsyncIntegration, SubscriptionManagement) {
    // Subscribe a module
    int ret = lc_bio_async_subscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING, LC_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);

    // Verify subscriber count
    uint32_t count = lc_bio_async_get_subscriber_count(bridge, LC_BIO_MSG_NE_STATE);
    EXPECT_EQ(count, 1);

    // Update subscription
    ret = lc_bio_async_update_subscription(bridge, BIO_MODULE_COGNITIVE_TRAINING, LC_BIO_SUB_NE_STATE);
    EXPECT_EQ(ret, 0);

    // Unsubscribe
    ret = lc_bio_async_unsubscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING);
    EXPECT_EQ(ret, 0);
}

TEST_F(LCBioAsyncIntegration, StatisticsTracking) {
    lc_bio_async_stats_t stats;
    int ret = lc_bio_async_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.broadcasts_sent, 0);

    // Reset stats
    ret = lc_bio_async_reset_stats(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(LCBioAsyncIntegration, MessageTypeNames) {
    const char* name = lc_bio_msg_type_name(LC_BIO_MSG_NE_STATE);
    EXPECT_STREQ(name, "NE_STATE");

    name = lc_bio_msg_type_name(LC_BIO_MSG_AROUSAL_CHANGE);
    EXPECT_STREQ(name, "AROUSAL_CHANGE");

    name = lc_bio_msg_type_name(LC_BIO_MSG_PHASIC_BURST);
    EXPECT_STREQ(name, "PHASIC_BURST");
}

//=============================================================================
// VTA Bio-Async Bridge Tests
//=============================================================================

class VTABioAsyncIntegration : public ::testing::Test {
protected:
    vta_bio_async_bridge_t* bridge = nullptr;

    void SetUp() override {
        vta_bio_async_config_t config;
        vta_bio_async_default_config(&config);
        bridge = vta_bio_async_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            vta_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(VTABioAsyncIntegration, BridgeCreation) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_FALSE(vta_bio_async_is_connected(bridge));
}

TEST_F(VTABioAsyncIntegration, DefaultConfig) {
    vta_bio_async_config_t config;
    int ret = vta_bio_async_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_auto_broadcast);
    EXPECT_TRUE(config.enable_rpe_routing);
    EXPECT_TRUE(config.enable_motivation_routing);
    EXPECT_TRUE(config.enable_plasticity_gating);
    EXPECT_EQ(config.default_channel, BIO_CHANNEL_DOPAMINE);
}

TEST_F(VTABioAsyncIntegration, SubscriptionManagement) {
    int ret = vta_bio_async_subscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING, VTA_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);

    uint32_t count = vta_bio_async_get_subscriber_count(bridge, VTA_BIO_MSG_DA_STATE);
    EXPECT_EQ(count, 1);

    ret = vta_bio_async_unsubscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING);
    EXPECT_EQ(ret, 0);
}

TEST_F(VTABioAsyncIntegration, MessageTypeNames) {
    const char* name = vta_bio_msg_type_name(VTA_BIO_MSG_DA_STATE);
    EXPECT_STREQ(name, "DA_STATE");

    name = vta_bio_msg_type_name(VTA_BIO_MSG_RPE);
    EXPECT_STREQ(name, "RPE");

    name = vta_bio_msg_type_name(VTA_BIO_MSG_DA_BURST);
    EXPECT_STREQ(name, "DA_BURST");
}

//=============================================================================
// Raphe Bio-Async Bridge Tests
//=============================================================================

class RapheBioAsyncIntegration : public ::testing::Test {
protected:
    raphe_bio_async_bridge_t* bridge = nullptr;

    void SetUp() override {
        raphe_bio_async_config_t config;
        raphe_bio_async_default_config(&config);
        bridge = raphe_bio_async_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            raphe_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(RapheBioAsyncIntegration, BridgeCreation) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_FALSE(raphe_bio_async_is_connected(bridge));
}

TEST_F(RapheBioAsyncIntegration, DefaultConfig) {
    raphe_bio_async_config_t config;
    int ret = raphe_bio_async_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_auto_broadcast);
    EXPECT_TRUE(config.enable_mood_routing);
    EXPECT_TRUE(config.enable_social_routing);
    EXPECT_TRUE(config.enable_plasticity_gating);
    EXPECT_EQ(config.default_channel, BIO_CHANNEL_SEROTONIN);
}

TEST_F(RapheBioAsyncIntegration, SubscriptionManagement) {
    int ret = raphe_bio_async_subscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING, RAPHE_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);

    uint32_t count = raphe_bio_async_get_subscriber_count(bridge, RAPHE_BIO_MSG_5HT_STATE);
    EXPECT_EQ(count, 1);

    ret = raphe_bio_async_unsubscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING);
    EXPECT_EQ(ret, 0);
}

TEST_F(RapheBioAsyncIntegration, MessageTypeNames) {
    const char* name = raphe_bio_msg_type_name(RAPHE_BIO_MSG_5HT_STATE);
    EXPECT_STREQ(name, "5HT_STATE");

    name = raphe_bio_msg_type_name(RAPHE_BIO_MSG_MOOD_CHANGE);
    EXPECT_STREQ(name, "MOOD_CHANGE");
}

//=============================================================================
// Habenula Bio-Async Bridge Tests
//=============================================================================

class HabenulaBioAsyncIntegration : public ::testing::Test {
protected:
    hab_bio_async_bridge_t* bridge = nullptr;

    void SetUp() override {
        hab_bio_async_config_t config;
        hab_bio_async_default_config(&config);
        bridge = hab_bio_async_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hab_bio_async_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(HabenulaBioAsyncIntegration, BridgeCreation) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_FALSE(hab_bio_async_is_connected(bridge));
}

TEST_F(HabenulaBioAsyncIntegration, DefaultConfig) {
    hab_bio_async_config_t config;
    int ret = hab_bio_async_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_auto_broadcast);
    EXPECT_TRUE(config.enable_vta_inhibition);
    EXPECT_TRUE(config.enable_raphe_inhibition);
    EXPECT_TRUE(config.enable_avoidance_routing);
    EXPECT_TRUE(config.enable_plasticity_gating);
}

TEST_F(HabenulaBioAsyncIntegration, SubscriptionManagement) {
    int ret = hab_bio_async_subscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING, HAB_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);

    uint32_t count = hab_bio_async_get_subscriber_count(bridge, HAB_BIO_MSG_STATE);
    EXPECT_EQ(count, 1);

    ret = hab_bio_async_unsubscribe_module(bridge, BIO_MODULE_COGNITIVE_TRAINING);
    EXPECT_EQ(ret, 0);
}

TEST_F(HabenulaBioAsyncIntegration, MessageTypeNames) {
    const char* name = hab_bio_msg_type_name(HAB_BIO_MSG_STATE);
    EXPECT_STREQ(name, "STATE");

    name = hab_bio_msg_type_name(HAB_BIO_MSG_NEGATIVE_RPE);
    EXPECT_STREQ(name, "NEGATIVE_RPE");
}

//=============================================================================
// Cognitive Hub Bridge Tests
//=============================================================================

class CognitiveHubIntegration : public ::testing::Test {
protected:
    neuromod_cognitive_hub_bridge_t* hub = nullptr;

    void SetUp() override {
        neuromod_cognitive_hub_config_t config;
        neuromod_cognitive_hub_default_config(&config);
        hub = neuromod_cognitive_hub_create(&config);
        ASSERT_NE(hub, nullptr);
    }

    void TearDown() override {
        if (hub) {
            neuromod_cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

TEST_F(CognitiveHubIntegration, HubCreation) {
    EXPECT_NE(hub, nullptr);
    EXPECT_FALSE(neuromod_cognitive_hub_is_connected(hub));
}

TEST_F(CognitiveHubIntegration, DefaultConfig) {
    neuromod_cognitive_hub_config_t config;
    int ret = neuromod_cognitive_hub_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_lc_integration);
    EXPECT_TRUE(config.enable_vta_integration);
    EXPECT_TRUE(config.enable_raphe_integration);
    EXPECT_TRUE(config.enable_habenula_integration);
    EXPECT_TRUE(config.enable_cognitive_feedback);
    EXPECT_TRUE(config.broadcast_on_change);
}

TEST_F(CognitiveHubIntegration, ArousalPublishing) {
    // Connect first (even with NULL cog_hub for basic testing)
    int ret = neuromod_cognitive_hub_connect(hub, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(neuromod_cognitive_hub_is_connected(hub));

    // Publish arousal
    neuromod_arousal_payload_t arousal = {0};
    arousal.arousal_level = 0.7f;
    arousal.alertness = 0.8f;
    arousal.gain_factor = 1.2f;
    arousal.phasic_burst = false;

    ret = neuromod_cognitive_hub_publish_arousal(hub, &arousal);
    EXPECT_EQ(ret, 0);

    // Verify state updated
    neuromod_cog_state_t state;
    ret = neuromod_cognitive_hub_get_state(hub, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(state.arousal, 0.7f);
    EXPECT_FLOAT_EQ(state.alertness, 0.8f);
}

TEST_F(CognitiveHubIntegration, RewardPublishing) {
    int ret = neuromod_cognitive_hub_connect(hub, nullptr);
    EXPECT_EQ(ret, 0);

    neuromod_reward_payload_t reward = {0};
    reward.rpe = 0.5f;
    reward.motivation = 0.8f;
    reward.value = 1.0f;
    reward.positive_rpe = true;

    ret = neuromod_cognitive_hub_publish_reward(hub, &reward);
    EXPECT_EQ(ret, 0);

    neuromod_cog_state_t state;
    ret = neuromod_cognitive_hub_get_state(hub, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(state.last_rpe, 0.5f);
    EXPECT_FLOAT_EQ(state.motivation, 0.8f);
    EXPECT_TRUE(state.reward_predicted);
}

TEST_F(CognitiveHubIntegration, MoodPublishing) {
    int ret = neuromod_cognitive_hub_connect(hub, nullptr);
    EXPECT_EQ(ret, 0);

    neuromod_mood_payload_t mood = {0};
    mood.mood_level = 0.6f;
    mood.impulse_inhibition = 0.7f;
    mood.patience = 0.8f;
    mood.social_confidence = 0.75f;

    ret = neuromod_cognitive_hub_publish_mood(hub, &mood);
    EXPECT_EQ(ret, 0);

    neuromod_cog_state_t state;
    ret = neuromod_cognitive_hub_get_state(hub, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(state.mood, 0.6f);
    EXPECT_FLOAT_EQ(state.impulse_control, 0.7f);
    EXPECT_FLOAT_EQ(state.patience, 0.8f);
}

TEST_F(CognitiveHubIntegration, AversivePublishing) {
    int ret = neuromod_cognitive_hub_connect(hub, nullptr);
    EXPECT_EQ(ret, 0);

    neuromod_aversive_payload_t aversive = {0};
    aversive.negative_rpe = 0.4f;
    aversive.avoidance_strength = 0.6f;
    aversive.disappointment = 0.5f;
    aversive.urgent = true;

    ret = neuromod_cognitive_hub_publish_aversive(hub, &aversive);
    EXPECT_EQ(ret, 0);

    neuromod_cog_state_t state;
    ret = neuromod_cognitive_hub_get_state(hub, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(state.negative_rpe, 0.4f);
    EXPECT_FLOAT_EQ(state.avoidance_drive, 0.6f);
    EXPECT_TRUE(state.punishment_detected);
}

TEST_F(CognitiveHubIntegration, StatisticsTracking) {
    int ret = neuromod_cognitive_hub_connect(hub, nullptr);
    EXPECT_EQ(ret, 0);

    neuromod_arousal_payload_t arousal = {0};
    arousal.arousal_level = 0.5f;
    neuromod_cognitive_hub_publish_arousal(hub, &arousal);

    neuromod_cognitive_hub_stats_t stats;
    ret = neuromod_cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.lc_events_published, 0);
    EXPECT_GT(stats.total_events_published, 0);

    // Reset stats
    ret = neuromod_cognitive_hub_reset_stats(hub);
    EXPECT_EQ(ret, 0);
}

TEST_F(CognitiveHubIntegration, CenterNames) {
    const char* name = neuromod_center_name(NEUROMOD_CENTER_LC);
    EXPECT_STREQ(name, "Locus Coeruleus (NE)");

    name = neuromod_center_name(NEUROMOD_CENTER_VTA);
    EXPECT_STREQ(name, "Ventral Tegmental Area (DA)");

    name = neuromod_center_name(NEUROMOD_CENTER_RAPHE);
    EXPECT_STREQ(name, "Raphe Nuclei (5-HT)");

    name = neuromod_center_name(NEUROMOD_CENTER_HABENULA);
    EXPECT_STREQ(name, "Habenula");
}

TEST_F(CognitiveHubIntegration, EventNames) {
    const char* name = neuromod_cog_event_name(NEUROMOD_COG_EVENT_AROUSAL_CHANGE);
    EXPECT_STREQ(name, "AROUSAL_CHANGE");

    name = neuromod_cog_event_name(NEUROMOD_COG_EVENT_RPE_SIGNAL);
    EXPECT_STREQ(name, "RPE_SIGNAL");

    name = neuromod_cog_event_name(NEUROMOD_COG_EVENT_MOOD_CHANGE);
    EXPECT_STREQ(name, "MOOD_CHANGE");

    name = neuromod_cog_event_name(NEUROMOD_COG_EVENT_NEGATIVE_RPE);
    EXPECT_STREQ(name, "NEGATIVE_RPE");
}

//=============================================================================
// Cross-Neuromodulatory Coordination Tests
//=============================================================================

class CrossNeuromodulatoryIntegration : public ::testing::Test {
protected:
    lc_bio_async_bridge_t* lc = nullptr;
    vta_bio_async_bridge_t* vta = nullptr;
    raphe_bio_async_bridge_t* raphe = nullptr;
    hab_bio_async_bridge_t* hab = nullptr;
    neuromod_cognitive_hub_bridge_t* hub = nullptr;

    void SetUp() override {
        // Create all bridges
        lc_bio_async_config_t lc_config;
        lc_bio_async_default_config(&lc_config);
        lc = lc_bio_async_bridge_create(&lc_config);
        ASSERT_NE(lc, nullptr);

        vta_bio_async_config_t vta_config;
        vta_bio_async_default_config(&vta_config);
        vta = vta_bio_async_bridge_create(&vta_config);
        ASSERT_NE(vta, nullptr);

        raphe_bio_async_config_t raphe_config;
        raphe_bio_async_default_config(&raphe_config);
        raphe = raphe_bio_async_bridge_create(&raphe_config);
        ASSERT_NE(raphe, nullptr);

        hab_bio_async_config_t hab_config;
        hab_bio_async_default_config(&hab_config);
        hab = hab_bio_async_bridge_create(&hab_config);
        ASSERT_NE(hab, nullptr);

        neuromod_cognitive_hub_config_t hub_config;
        neuromod_cognitive_hub_default_config(&hub_config);
        hub = neuromod_cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr);
    }

    void TearDown() override {
        if (lc) { lc_bio_async_bridge_destroy(lc); lc = nullptr; }
        if (vta) { vta_bio_async_bridge_destroy(vta); vta = nullptr; }
        if (raphe) { raphe_bio_async_bridge_destroy(raphe); raphe = nullptr; }
        if (hab) { hab_bio_async_bridge_destroy(hab); hab = nullptr; }
        if (hub) { neuromod_cognitive_hub_destroy(hub); hub = nullptr; }
    }
};

TEST_F(CrossNeuromodulatoryIntegration, AllBridgesCreated) {
    EXPECT_NE(lc, nullptr);
    EXPECT_NE(vta, nullptr);
    EXPECT_NE(raphe, nullptr);
    EXPECT_NE(hab, nullptr);
    EXPECT_NE(hub, nullptr);
}

TEST_F(CrossNeuromodulatoryIntegration, CrossSubscription) {
    // Each bridge can subscribe to cognitive hub events
    int ret;

    ret = lc_bio_async_subscribe_module(lc, BIO_MODULE_VTA, LC_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);

    ret = vta_bio_async_subscribe_module(vta, BIO_MODULE_LOCUS_COERULEUS, VTA_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);

    ret = raphe_bio_async_subscribe_module(raphe, BIO_MODULE_HABENULA, RAPHE_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);

    ret = hab_bio_async_subscribe_module(hab, BIO_MODULE_VTA, HAB_BIO_SUB_ALL);
    EXPECT_EQ(ret, 0);
}

TEST_F(CrossNeuromodulatoryIntegration, CognitiveHubIntegratesAllSources) {
    neuromod_cognitive_hub_connect(hub, nullptr);

    // Publish from each center
    neuromod_arousal_payload_t arousal = {0};
    arousal.arousal_level = 0.6f;
    neuromod_cognitive_hub_publish_arousal(hub, &arousal);

    neuromod_reward_payload_t reward = {0};
    reward.motivation = 0.7f;
    neuromod_cognitive_hub_publish_reward(hub, &reward);

    neuromod_mood_payload_t mood = {0};
    mood.mood_level = 0.8f;
    neuromod_cognitive_hub_publish_mood(hub, &mood);

    neuromod_aversive_payload_t aversive = {0};
    aversive.avoidance_strength = 0.3f;
    neuromod_cognitive_hub_publish_aversive(hub, &aversive);

    // Verify all state is integrated
    neuromod_cog_state_t state;
    neuromod_cognitive_hub_get_state(hub, &state);

    EXPECT_FLOAT_EQ(state.arousal, 0.6f);
    EXPECT_FLOAT_EQ(state.motivation, 0.7f);
    EXPECT_FLOAT_EQ(state.mood, 0.8f);
    EXPECT_FLOAT_EQ(state.avoidance_drive, 0.3f);

    // Verify statistics
    neuromod_cognitive_hub_stats_t stats;
    neuromod_cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.total_events_published, 4);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
