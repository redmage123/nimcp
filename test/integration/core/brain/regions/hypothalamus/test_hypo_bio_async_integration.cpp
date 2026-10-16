/**
 * @file test_hypo_bio_async_integration.cpp
 * @brief Integration tests for Hypothalamus Bio-Async integration
 *
 * WHAT: Integration tests verifying the hypothalamus bio-async bridge
 *       correctly integrates with the bio-router and orchestrator
 * WHY:  Ensure message routing, subscription management, and broadcast
 *       functionality works correctly between hypothalamus and modules
 * HOW:  Test message flow, subscription filtering, broadcast delivery,
 *       and drive state communication via bio-async infrastructure
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_bio_async_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_MODULE_ATTENTION       0x2001
#define TEST_MODULE_MEMORY          0x2002
#define TEST_MODULE_EXECUTIVE       0x2003
#define TEST_MODULE_EMOTION         0x2004
#define TEST_MODULE_WELLBEING       0x2005

#define TEST_DRIVE_LEVEL_LOW        0.2f
#define TEST_DRIVE_LEVEL_HIGH       0.8f
#define TEST_DRIVE_LEVEL_URGENT     0.95f

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypoBioAsyncIntegrationTest : public ::testing::Test {
protected:
    hypo_orchestrator_t orchestrator;
    hypo_bio_async_bridge_t* bio_async_bridge;

    void SetUp() override {
        /* Initialize global bio-router */
        bio_router_config_t router_config = bio_router_default_config();
        if (!bio_router_is_initialized()) {
            nimcp_error_t err = bio_router_init(&router_config);
            ASSERT_EQ(NIMCP_SUCCESS, err);
        }

        /* Create orchestrator */
        hypo_orch_config_t orch_config;
        hypo_orch_default_config(&orch_config);
        orchestrator = hypo_orch_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr);

        /* Create bio-async bridge */
        hypo_bio_async_config_t bio_config;
        hypo_bio_async_default_config(&bio_config);
        bio_async_bridge = hypo_bio_async_bridge_create(&bio_config);
        ASSERT_NE(bio_async_bridge, nullptr);

        /* Connect bridge to orchestrator and global router */
        int ret = hypo_bio_async_connect(bio_async_bridge, orchestrator, nullptr);
        ASSERT_EQ(0, ret);
    }

    void TearDown() override {
        if (bio_async_bridge) {
            hypo_bio_async_disconnect(bio_async_bridge);
            hypo_bio_async_bridge_destroy(bio_async_bridge);
            bio_async_bridge = nullptr;
        }
        if (orchestrator) {
            hypo_orch_destroy(orchestrator);
            orchestrator = nullptr;
        }
        /* Note: We don't shutdown global router here as other tests might use it */
    }

    /* Helper to update the bridge */
    void update_system(uint32_t delta_ms) {
        hypo_bio_async_update(bio_async_bridge, delta_ms);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, CreateWithDefaultConfig) {
    /* Destroy existing bridge */
    hypo_bio_async_disconnect(bio_async_bridge);
    hypo_bio_async_bridge_destroy(bio_async_bridge);

    /* Create with NULL config should use defaults */
    bio_async_bridge = hypo_bio_async_bridge_create(nullptr);
    ASSERT_NE(bio_async_bridge, nullptr);

    /* Connect should succeed */
    int ret = hypo_bio_async_connect(bio_async_bridge, orchestrator, nullptr);
    EXPECT_EQ(0, ret);
}

TEST_F(HypoBioAsyncIntegrationTest, DestroyNullIsSafe) {
    /* Should not crash */
    hypo_bio_async_bridge_destroy(nullptr);
}

TEST_F(HypoBioAsyncIntegrationTest, ConnectionStatus) {
    /* Should be connected after SetUp */
    EXPECT_TRUE(hypo_bio_async_is_connected(bio_async_bridge));

    /* Disconnect */
    hypo_bio_async_disconnect(bio_async_bridge);
    EXPECT_FALSE(hypo_bio_async_is_connected(bio_async_bridge));

    /* Reconnect */
    int ret = hypo_bio_async_connect(bio_async_bridge, orchestrator, nullptr);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(hypo_bio_async_is_connected(bio_async_bridge));
}

/* ============================================================================
 * Subscription Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, SubscribeModule) {
    /* Subscribe module to drive state messages */
    int ret = hypo_bio_async_subscribe_module(bio_async_bridge,
                                               TEST_MODULE_ATTENTION,
                                               HYPO_BIO_SUB_DRIVE_STATE);
    EXPECT_EQ(0, ret);

    /* Get subscriber count */
    uint32_t count = hypo_bio_async_get_subscriber_count(bio_async_bridge,
                                                          HYPO_BIO_MSG_DRIVE_STATE);
    EXPECT_EQ(1u, count);
}

TEST_F(HypoBioAsyncIntegrationTest, SubscribeMultipleTypes) {
    /* Subscribe to multiple message types */
    uint32_t msg_types = HYPO_BIO_SUB_DRIVE_STATE |
                         HYPO_BIO_SUB_CIRCADIAN_PHASE |
                         HYPO_BIO_SUB_STRESS_LEVEL;

    int ret = hypo_bio_async_subscribe_module(bio_async_bridge,
                                               TEST_MODULE_MEMORY,
                                               msg_types);
    EXPECT_EQ(0, ret);

    /* Should be subscribed to all three types */
    EXPECT_GE(hypo_bio_async_get_subscriber_count(bio_async_bridge,
                                                   HYPO_BIO_MSG_DRIVE_STATE), 1u);
    EXPECT_GE(hypo_bio_async_get_subscriber_count(bio_async_bridge,
                                                   HYPO_BIO_MSG_CIRCADIAN_PHASE), 1u);
    EXPECT_GE(hypo_bio_async_get_subscriber_count(bio_async_bridge,
                                                   HYPO_BIO_MSG_STRESS_LEVEL), 1u);
}

TEST_F(HypoBioAsyncIntegrationTest, UnsubscribeModule) {
    /* Subscribe */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_ALL);

    uint32_t initial_count = hypo_bio_async_get_subscriber_count(bio_async_bridge,
                                                                   HYPO_BIO_MSG_DRIVE_STATE);

    /* Unsubscribe */
    int ret = hypo_bio_async_unsubscribe_module(bio_async_bridge, TEST_MODULE_ATTENTION);
    EXPECT_EQ(0, ret);

    /* Count should decrease */
    uint32_t final_count = hypo_bio_async_get_subscriber_count(bio_async_bridge,
                                                                 HYPO_BIO_MSG_DRIVE_STATE);
    EXPECT_LT(final_count, initial_count);
}

TEST_F(HypoBioAsyncIntegrationTest, UpdateSubscription) {
    /* Subscribe to drive state only */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_EXECUTIVE,
                                     HYPO_BIO_SUB_DRIVE_STATE);

    /* Update to include stress */
    uint32_t new_types = HYPO_BIO_SUB_DRIVE_STATE | HYPO_BIO_SUB_STRESS_LEVEL;
    int ret = hypo_bio_async_update_subscription(bio_async_bridge,
                                                  TEST_MODULE_EXECUTIVE,
                                                  new_types);
    EXPECT_EQ(0, ret);

    /* Should now be subscribed to stress */
    EXPECT_GE(hypo_bio_async_get_subscriber_count(bio_async_bridge,
                                                   HYPO_BIO_MSG_STRESS_LEVEL), 1u);
}

/* ============================================================================
 * Broadcast Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, BroadcastDriveState) {
    /* Subscribe modules to drive state */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_DRIVE_STATE);
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_MEMORY,
                                     HYPO_BIO_SUB_DRIVE_STATE);

    /* Broadcast drive state */
    int ret = hypo_bio_async_broadcast_drive_state(bio_async_bridge);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_GT(stats.drive_state_broadcasts, 0u);
}

TEST_F(HypoBioAsyncIntegrationTest, BroadcastCircadianPhase) {
    /* Subscribe module to circadian messages */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_EXECUTIVE,
                                     HYPO_BIO_SUB_CIRCADIAN_PHASE);

    /* Broadcast circadian phase */
    float phase = 3.14159f;  /* Midday */
    int ret = hypo_bio_async_broadcast_circadian(bio_async_bridge, phase);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_GT(stats.circadian_broadcasts, 0u);
}

TEST_F(HypoBioAsyncIntegrationTest, BroadcastStressLevel) {
    /* Subscribe modules to stress messages */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_STRESS_LEVEL);
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_EMOTION,
                                     HYPO_BIO_SUB_STRESS_LEVEL);

    /* Broadcast stress level */
    float cortisol = 0.7f;
    int ret = hypo_bio_async_broadcast_stress(bio_async_bridge, cortisol);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_GT(stats.stress_broadcasts, 0u);
}

TEST_F(HypoBioAsyncIntegrationTest, BroadcastArousalState) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_AROUSAL_STATE);

    float arousal = 0.8f;
    int ret = hypo_bio_async_broadcast_arousal(bio_async_bridge, arousal);
    EXPECT_EQ(0, ret);
}

TEST_F(HypoBioAsyncIntegrationTest, BroadcastAutonomicState) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_WELLBEING,
                                     HYPO_BIO_SUB_AUTONOMIC_STATE);

    float sympathetic = 0.6f;
    float parasympathetic = 0.4f;
    int ret = hypo_bio_async_broadcast_autonomic(bio_async_bridge,
                                                   sympathetic, parasympathetic);
    EXPECT_EQ(0, ret);
}

TEST_F(HypoBioAsyncIntegrationTest, BroadcastTemperature) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_TEMPERATURE);

    float core_temp = 37.0f;
    float setpoint_temp = 37.0f;
    int ret = hypo_bio_async_broadcast_temperature(bio_async_bridge,
                                                     core_temp, setpoint_temp);
    EXPECT_EQ(0, ret);
}

TEST_F(HypoBioAsyncIntegrationTest, BroadcastFatigue) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_EXECUTIVE,
                                     HYPO_BIO_SUB_FATIGUE_LEVEL);

    float fatigue = 0.5f;
    float sleep_pressure = 0.6f;
    int ret = hypo_bio_async_broadcast_fatigue(bio_async_bridge,
                                                 fatigue, sleep_pressure);
    EXPECT_EQ(0, ret);
}

/* ============================================================================
 * Urgent Drive Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, SendUrgentDrive) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_DRIVE_URGENT);
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_EXECUTIVE,
                                     HYPO_BIO_SUB_DRIVE_URGENT);

    /* Send urgent drive notification */
    int ret = hypo_bio_async_send_urgent_drive(bio_async_bridge,
                                                 HYPO_DRIVE_SAFETY,
                                                 TEST_DRIVE_LEVEL_URGENT,
                                                 0.95f);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_GT(stats.urgent_notifications, 0u);
}

/* ============================================================================
 * Reward Signal Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, SendRewardSignal) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_MEMORY,
                                     HYPO_BIO_SUB_REWARD_SIGNAL);

    /* Send reward signal */
    float reward = 0.8f;
    int ret = hypo_bio_async_send_reward(bio_async_bridge, reward, TEST_MODULE_MEMORY);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_GT(stats.reward_signals_sent, 0u);
}

TEST_F(HypoBioAsyncIntegrationTest, BroadcastRewardSignal) {
    /* Subscribe multiple modules */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_MEMORY,
                                     HYPO_BIO_SUB_REWARD_SIGNAL);
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_REWARD_SIGNAL);

    /* Send broadcast reward (target 0 means broadcast) */
    float reward = 0.5f;
    int ret = hypo_bio_async_send_reward(bio_async_bridge, reward, 0);
    EXPECT_EQ(0, ret);
}

/* ============================================================================
 * Homeostatic Alert Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, BroadcastHomeostaticAlert) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_HOMEOSTATIC_ALERT);
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_EXECUTIVE,
                                     HYPO_BIO_SUB_HOMEOSTATIC_ALERT);

    /* Broadcast homeostatic alert */
    int ret = hypo_bio_async_broadcast_homeostatic_alert(bio_async_bridge,
                                                           1,      /* Variable ID */
                                                           0.9f,   /* Current value */
                                                           0.5f,   /* Setpoint */
                                                           true);  /* Critical */
    EXPECT_EQ(0, ret);
}

/* ============================================================================
 * Update and Processing Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, UpdateProcessesMessages) {
    /* Subscribe to various message types */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_ALL);

    /* Broadcast several message types */
    hypo_bio_async_broadcast_drive_state(bio_async_bridge);
    hypo_bio_async_broadcast_circadian(bio_async_bridge, 3.14f);
    hypo_bio_async_broadcast_stress(bio_async_bridge, 0.5f);

    /* Update bridge to process messages */
    int ret = hypo_bio_async_update(bio_async_bridge, 16);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_GT(stats.messages_sent, 0u);
}

TEST_F(HypoBioAsyncIntegrationTest, ProcessInboxMessages) {
    /* Process inbox (even if empty) should not fail */
    int ret = hypo_bio_async_process_inbox(bio_async_bridge, 0);
    EXPECT_GE(ret, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, StatisticsAccuracy) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_DRIVE_STATE);
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_MEMORY,
                                     HYPO_BIO_SUB_DRIVE_STATE);

    /* Broadcast multiple times */
    for (int i = 0; i < 10; i++) {
        hypo_bio_async_broadcast_drive_state(bio_async_bridge);
    }

    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);

    EXPECT_EQ(10u, stats.drive_state_broadcasts);
    EXPECT_GE(stats.messages_sent, 10u);
    EXPECT_GE(stats.active_subscriptions, 2u);
}

TEST_F(HypoBioAsyncIntegrationTest, ResetStatistics) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_DRIVE_STATE);

    /* Generate activity */
    for (int i = 0; i < 5; i++) {
        hypo_bio_async_broadcast_drive_state(bio_async_bridge);
    }

    /* Reset stats */
    int ret = hypo_bio_async_reset_stats(bio_async_bridge);
    EXPECT_EQ(0, ret);

    /* Stats should be reset */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_EQ(0u, stats.drive_state_broadcasts);
    EXPECT_EQ(0u, stats.messages_sent);
    /* Subscriptions should remain */
    EXPECT_GE(stats.active_subscriptions, 1u);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, NullHandling) {
    /* Operations on null bridge should fail gracefully */
    int ret = hypo_bio_async_broadcast_drive_state(nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_bio_async_subscribe_module(nullptr, TEST_MODULE_ATTENTION,
                                           HYPO_BIO_SUB_DRIVE_STATE);
    EXPECT_EQ(-1, ret);

    hypo_bio_async_stats_t stats;
    ret = hypo_bio_async_get_stats(nullptr, &stats);
    EXPECT_EQ(-1, ret);
}

TEST_F(HypoBioAsyncIntegrationTest, DisconnectedBridgeHandling) {
    /* Disconnect bridge */
    hypo_bio_async_disconnect(bio_async_bridge);
    EXPECT_FALSE(hypo_bio_async_is_connected(bio_async_bridge));

    /* Operations should fail gracefully when disconnected */
    int ret = hypo_bio_async_broadcast_drive_state(bio_async_bridge);
    EXPECT_EQ(-1, ret);

    /* Reconnect for teardown */
    hypo_bio_async_connect(bio_async_bridge, orchestrator, nullptr);
}

/* ============================================================================
 * Message Type Names Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, MessageTypeNames) {
    const char* name;

    name = hypo_bio_msg_type_name(HYPO_BIO_MSG_DRIVE_STATE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_bio_msg_type_name(HYPO_BIO_MSG_STRESS_LEVEL);
    EXPECT_NE(name, nullptr);

    name = hypo_bio_msg_type_name(HYPO_BIO_MSG_CIRCADIAN_PHASE);
    EXPECT_NE(name, nullptr);

    name = hypo_bio_msg_type_name(HYPO_BIO_MSG_REWARD_SIGNAL);
    EXPECT_NE(name, nullptr);
}

/* ============================================================================
 * Integration with Orchestrator Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, OrchestratorBioAsyncCoordination) {
    /* Register bridge with orchestrator */
    uint32_t bridge_id;
    int ret = hypo_orch_register_bridge(orchestrator, HYPO_BRIDGE_BIO_ASYNC,
                                         "bio_async_bridge",
                                         bio_async_bridge,
                                         nullptr, &bridge_id);
    EXPECT_EQ(0, ret);

    /* Subscribe module to drive state via bio-async */
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_DRIVE_STATE);

    /* Report drive to orchestrator - should propagate to bio-async */
    hypo_orch_report_drive(orchestrator, bridge_id, HYPO_DRIVE_HUNGER,
                            TEST_DRIVE_LEVEL_HIGH, HYPO_URGENCY_ELEVATED,
                            "Hunger drive");

    /* Broadcast should include the updated drive state */
    ret = hypo_bio_async_broadcast_drive_state(bio_async_bridge);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_GT(stats.drive_state_broadcasts, 0u);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, ConcurrentBroadcasts) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_ALL);

    std::atomic<int> broadcast_count{0};

    auto broadcast_thread = [this, &broadcast_count]() {
        for (int i = 0; i < 50; i++) {
            hypo_bio_async_broadcast_drive_state(bio_async_bridge);
            hypo_bio_async_broadcast_circadian(bio_async_bridge, 3.14f);
            hypo_bio_async_broadcast_stress(bio_async_bridge, 0.5f);
            broadcast_count++;
        }
    };

    std::thread t1(broadcast_thread);
    std::thread t2(broadcast_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(100, broadcast_count.load());

    /* Bridge should still be functional */
    hypo_bio_async_stats_t stats;
    int ret = hypo_bio_async_get_stats(bio_async_bridge, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GT(stats.messages_sent, 0u);
}

TEST_F(HypoBioAsyncIntegrationTest, ConcurrentSubscriptions) {
    std::atomic<int> subscription_count{0};

    auto subscribe_thread = [this, &subscription_count]() {
        for (int i = 0; i < 10; i++) {
            uint32_t module_id = TEST_MODULE_ATTENTION + i;
            int ret = hypo_bio_async_subscribe_module(bio_async_bridge,
                                                        module_id,
                                                        HYPO_BIO_SUB_DRIVE_STATE);
            if (ret == 0) subscription_count++;

            /* Small delay */
            std::this_thread::sleep_for(std::chrono::microseconds(100));

            hypo_bio_async_unsubscribe_module(bio_async_bridge, module_id);
        }
    };

    std::thread t1(subscribe_thread);
    std::thread t2(subscribe_thread);

    t1.join();
    t2.join();

    /* Some subscriptions should have succeeded */
    EXPECT_GT(subscription_count.load(), 0);

    /* Bridge should still be functional */
    int ret = hypo_bio_async_broadcast_drive_state(bio_async_bridge);
    EXPECT_EQ(0, ret);
}

/* ============================================================================
 * Extended Update Cycle Tests
 * ============================================================================ */

TEST_F(HypoBioAsyncIntegrationTest, ExtendedUpdateCycles) {
    hypo_bio_async_subscribe_module(bio_async_bridge,
                                     TEST_MODULE_ATTENTION,
                                     HYPO_BIO_SUB_ALL);

    /* Run many update cycles */
    for (int cycle = 0; cycle < 100; cycle++) {
        /* Vary what we broadcast */
        if (cycle % 3 == 0) {
            hypo_bio_async_broadcast_drive_state(bio_async_bridge);
        }
        if (cycle % 5 == 0) {
            hypo_bio_async_broadcast_stress(bio_async_bridge, 0.3f + (cycle % 10) * 0.05f);
        }
        if (cycle % 7 == 0) {
            hypo_bio_async_broadcast_circadian(bio_async_bridge, (cycle % 24) * 0.26f);
        }

        /* Update */
        hypo_bio_async_update(bio_async_bridge, 16);
    }

    /* Check final stats */
    hypo_bio_async_stats_t stats;
    hypo_bio_async_get_stats(bio_async_bridge, &stats);

    EXPECT_GT(stats.drive_state_broadcasts, 30u);  /* Approximately 100/3 */
    EXPECT_GT(stats.stress_broadcasts, 15u);       /* Approximately 100/5 */
    EXPECT_GT(stats.circadian_broadcasts, 10u);    /* Approximately 100/7 */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    /* Clean up global router */
    if (bio_router_is_initialized()) {
        bio_router_shutdown();
    }

    return result;
}
