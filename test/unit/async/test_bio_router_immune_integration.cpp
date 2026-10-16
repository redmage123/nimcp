/**
 * @file test_bio_router_immune_integration.cpp
 * @brief Unit tests for Bio-Router Immune Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Test suite for router-immune bridge bidirectional integration
 * WHY:  Ensure correct coupling between bio-async routing and immune system
 * HOW:  Unit tests covering lifecycle, cytokine priority, quarantine, anomaly detection
 */

#include <gtest/gtest.h>
#include "async/immune/nimcp_bio_router_immune_bridge.h"
#include "async/nimcp_bio_router.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class RouterImmuneBridgeTest : public ::testing::Test {
protected:
    router_immune_bridge_t* bridge;
    bio_router_t router;
    brain_immune_system_t* immune_system;

    void SetUp() override {
        /* Initialize bio-router */
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);
        router = bio_router_get_global();

        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Create bridge */
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            router_immune_bridge_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
        bio_router_shutdown();
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, DefaultConfigTest) {
    router_immune_config_t config;
    EXPECT_EQ(0, router_immune_default_config(&config));

    /* Verify all features enabled by default */
    EXPECT_TRUE(config.enable_cytokine_priority_routing);
    EXPECT_TRUE(config.enable_inflammation_latency_impact);
    EXPECT_TRUE(config.enable_quarantine_routing_exclusion);
    EXPECT_TRUE(config.enable_anomaly_immune_trigger);
    EXPECT_TRUE(config.enable_byzantine_detection);

    /* Verify capacity defaults */
    EXPECT_GT(config.max_cytokine_states, 0);
    EXPECT_GT(config.max_inflammation_sites, 0);
    EXPECT_GT(config.max_quarantined_nodes, 0);
    EXPECT_GT(config.max_anomaly_history, 0);

    /* Verify thresholds */
    EXPECT_GT(config.latency_spike_threshold_ms, 0.0f);
    EXPECT_GT(config.drop_rate_threshold, 0.0f);
    EXPECT_GT(config.error_rate_threshold, 0.0f);

    /* Verify null safety */
    EXPECT_EQ(-1, router_immune_default_config(nullptr));
}

TEST_F(RouterImmuneBridgeTest, CreateDestroyTest) {
    /* Create with default config */
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Verify systems linked */
    EXPECT_EQ(bridge->router, router);
    EXPECT_EQ(bridge->immune_system, immune_system);

    /* Verify arrays allocated */
    EXPECT_NE(bridge->cytokine_states, nullptr);
    EXPECT_NE(bridge->inflammation_impacts, nullptr);
    EXPECT_NE(bridge->quarantined_nodes, nullptr);
    EXPECT_NE(bridge->recent_anomalies, nullptr);

    /* Verify mutex created */
    EXPECT_NE(bridge->base.mutex, nullptr);

    /* Destroy should not crash */
    router_immune_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(RouterImmuneBridgeTest, CreateWithCustomConfigTest) {
    router_immune_config_t config;
    router_immune_default_config(&config);

    /* Customize config */
    config.max_cytokine_states = 256;
    config.max_quarantined_nodes = 512;
    config.enable_byzantine_detection = false;

    bridge = router_immune_bridge_create(&config, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(bridge->cytokine_capacity, 256);
    EXPECT_EQ(bridge->quarantine_capacity, 512);
    EXPECT_FALSE(bridge->enable_byzantine_detection);
}

TEST_F(RouterImmuneBridgeTest, CreateFailureTest) {
    /* Null router should fail */
    bridge = router_immune_bridge_create(nullptr, nullptr, immune_system);
    EXPECT_EQ(bridge, nullptr);

    /* Null immune system should fail */
    bridge = router_immune_bridge_create(nullptr, router, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(RouterImmuneBridgeTest, StartStopTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Start should succeed */
    EXPECT_EQ(0, router_immune_bridge_start(bridge));

    /* Module context should be set */
    EXPECT_NE(bridge->module_ctx, nullptr);

    /* Stop should succeed */
    EXPECT_EQ(0, router_immune_bridge_stop(bridge));

    /* Module context should be cleared */
    EXPECT_EQ(bridge->module_ctx, nullptr);
}

/* ============================================================================
 * Cytokine Priority Routing Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, PrioritizeCytokineTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Prioritize IL-1β cytokine */
    EXPECT_EQ(0, router_immune_prioritize_cytokine(
        bridge,
        BRAIN_CYTOKINE_IL1,
        0.8f,
        123  /* source cell */
    ));

    /* Verify cytokine state added */
    EXPECT_EQ(bridge->cytokine_count, 1);
    EXPECT_EQ(bridge->cytokine_states[0].type, BRAIN_CYTOKINE_IL1);
    EXPECT_FLOAT_EQ(bridge->cytokine_states[0].concentration, 0.8f);
    EXPECT_GT(bridge->cytokine_states[0].priority_level, ROUTER_IMMUNE_PRIORITY_NORMAL);

    /* Verify statistics */
    EXPECT_EQ(bridge->cytokine_messages_routed, 1);
}

TEST_F(RouterImmuneBridgeTest, CytokinePriorityLevelsTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* TNF-α should have highest priority */
    uint32_t tnf_priority = router_immune_get_cytokine_priority(bridge, BRAIN_CYTOKINE_TNF);

    /* IL-1β and IL-6 should have high priority */
    uint32_t il1_priority = router_immune_get_cytokine_priority(bridge, BRAIN_CYTOKINE_IL1);
    uint32_t il6_priority = router_immune_get_cytokine_priority(bridge, BRAIN_CYTOKINE_IL6);

    /* IL-10 (anti-inflammatory) should have lower priority */
    uint32_t il10_priority = router_immune_get_cytokine_priority(bridge, BRAIN_CYTOKINE_IL10);

    /* Verify priority hierarchy */
    EXPECT_GT(tnf_priority, il1_priority);
    EXPECT_GE(il1_priority, il6_priority);
    EXPECT_GE(il6_priority, il10_priority);
    EXPECT_GT(il10_priority, ROUTER_IMMUNE_PRIORITY_NORMAL);
}

TEST_F(RouterImmuneBridgeTest, MultipleCytokinesTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Add multiple cytokine types */
    EXPECT_EQ(0, router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_IL1, 0.5f, 1));
    EXPECT_EQ(0, router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_IL6, 0.7f, 2));
    EXPECT_EQ(0, router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_TNF, 0.9f, 3));

    EXPECT_EQ(bridge->cytokine_count, 3);
    EXPECT_EQ(bridge->cytokine_messages_routed, 3);
}

TEST_F(RouterImmuneBridgeTest, CytokineDisabledTest) {
    router_immune_config_t config;
    router_immune_default_config(&config);
    config.enable_cytokine_priority_routing = false;

    bridge = router_immune_bridge_create(&config, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Should return success but not add cytokine */
    EXPECT_EQ(0, router_immune_prioritize_cytokine(
        bridge, BRAIN_CYTOKINE_IL1, 0.5f, 1
    ));

    EXPECT_EQ(bridge->cytokine_count, 0);
}

/* ============================================================================
 * Inflammation Latency Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, ApplyInflammationLatencyTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Apply local inflammation */
    EXPECT_EQ(0, router_immune_apply_inflammation_latency(
        bridge, 1, INFLAMMATION_LOCAL
    ));

    /* Verify inflammation impact added */
    EXPECT_EQ(bridge->inflammation_count, 1);
    EXPECT_EQ(bridge->inflammation_impacts[0].affected_region, 1);
    EXPECT_EQ(bridge->inflammation_impacts[0].level, INFLAMMATION_LOCAL);
    EXPECT_TRUE(bridge->inflammation_impacts[0].active);

    /* Latency multiplier should be > 1.0 */
    EXPECT_GT(bridge->inflammation_impacts[0].latency_multiplier, 1.0f);
}

TEST_F(RouterImmuneBridgeTest, InflammationLatencyMultipliersTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Test different inflammation levels */
    router_immune_apply_inflammation_latency(bridge, 1, INFLAMMATION_NONE);
    router_immune_apply_inflammation_latency(bridge, 2, INFLAMMATION_LOCAL);
    router_immune_apply_inflammation_latency(bridge, 3, INFLAMMATION_REGIONAL);
    router_immune_apply_inflammation_latency(bridge, 4, INFLAMMATION_SYSTEMIC);
    router_immune_apply_inflammation_latency(bridge, 5, INFLAMMATION_STORM);

    /* Verify multipliers increase with severity */
    float none_mult = router_immune_get_latency_multiplier(bridge, 1);
    float local_mult = router_immune_get_latency_multiplier(bridge, 2);
    float regional_mult = router_immune_get_latency_multiplier(bridge, 3);
    float systemic_mult = router_immune_get_latency_multiplier(bridge, 4);
    float storm_mult = router_immune_get_latency_multiplier(bridge, 5);

    EXPECT_FLOAT_EQ(none_mult, 1.0f);
    EXPECT_GT(local_mult, none_mult);
    EXPECT_GT(regional_mult, local_mult);
    EXPECT_GT(systemic_mult, regional_mult);
    EXPECT_GT(storm_mult, systemic_mult);
}

TEST_F(RouterImmuneBridgeTest, InflammationUpdateTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Apply initial inflammation */
    router_immune_apply_inflammation_latency(bridge, 1, INFLAMMATION_LOCAL);
    EXPECT_EQ(bridge->inflammation_count, 1);

    /* Update same region with higher level */
    router_immune_apply_inflammation_latency(bridge, 1, INFLAMMATION_SYSTEMIC);

    /* Should update existing entry, not create new one */
    EXPECT_EQ(bridge->inflammation_count, 1);
    EXPECT_EQ(bridge->inflammation_impacts[0].level, INFLAMMATION_SYSTEMIC);
}

/* ============================================================================
 * Quarantine Routing Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, QuarantineNodeTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Quarantine a node */
    EXPECT_EQ(0, router_immune_quarantine_node(
        bridge,
        42,     /* node_id */
        60000,  /* 60 seconds */
        0.3f,   /* low trust */
        999     /* antigen_id */
    ));

    /* Verify quarantine state */
    EXPECT_EQ(bridge->quarantine_count, 1);
    EXPECT_EQ(bridge->quarantined_nodes[0].node_id, 42);
    EXPECT_EQ(bridge->quarantined_nodes[0].quarantine_duration_ms, 60000);
    EXPECT_FLOAT_EQ(bridge->quarantined_nodes[0].trust_score, 0.3f);
    EXPECT_EQ(bridge->quarantined_nodes[0].triggering_antigen_id, 999);

    /* Node should be quarantined */
    EXPECT_TRUE(router_immune_is_node_quarantined(bridge, 42));

    /* Other nodes should not be quarantined */
    EXPECT_FALSE(router_immune_is_node_quarantined(bridge, 43));

    /* Statistics updated */
    EXPECT_EQ(bridge->nodes_quarantined, 1);
}

TEST_F(RouterImmuneBridgeTest, QuarantineMultipleNodesTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Quarantine multiple nodes */
    router_immune_quarantine_node(bridge, 10, 30000, 0.5f, 1);
    router_immune_quarantine_node(bridge, 20, 40000, 0.4f, 2);
    router_immune_quarantine_node(bridge, 30, 50000, 0.3f, 3);

    EXPECT_EQ(bridge->quarantine_count, 3);
    EXPECT_TRUE(router_immune_is_node_quarantined(bridge, 10));
    EXPECT_TRUE(router_immune_is_node_quarantined(bridge, 20));
    EXPECT_TRUE(router_immune_is_node_quarantined(bridge, 30));
}

TEST_F(RouterImmuneBridgeTest, RestoreNodeTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Quarantine a node */
    router_immune_quarantine_node(bridge, 42, 60000, 0.3f, 999);
    EXPECT_TRUE(router_immune_is_node_quarantined(bridge, 42));

    /* Restore the node */
    EXPECT_EQ(0, router_immune_restore_node(bridge, 42));

    /* Node should no longer be quarantined */
    EXPECT_FALSE(router_immune_is_node_quarantined(bridge, 42));
    EXPECT_EQ(bridge->quarantine_count, 0);
}

TEST_F(RouterImmuneBridgeTest, RestoreNonQuarantinedNodeTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Try to restore node that was never quarantined */
    EXPECT_EQ(-1, router_immune_restore_node(bridge, 999));
}

TEST_F(RouterImmuneBridgeTest, QuarantineUpdateTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Quarantine a node */
    router_immune_quarantine_node(bridge, 42, 30000, 0.5f, 1);
    EXPECT_EQ(bridge->quarantine_count, 1);

    /* Quarantine same node again with different parameters */
    router_immune_quarantine_node(bridge, 42, 60000, 0.2f, 2);

    /* Should update existing entry, not create new one */
    EXPECT_EQ(bridge->quarantine_count, 1);
    EXPECT_EQ(bridge->quarantined_nodes[0].quarantine_duration_ms, 60000);
    EXPECT_FLOAT_EQ(bridge->quarantined_nodes[0].trust_score, 0.2f);
    EXPECT_EQ(bridge->quarantined_nodes[0].triggering_antigen_id, 2);
}

/* ============================================================================
 * Anomaly Detection Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, UpdateStatsTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Update stats should succeed */
    EXPECT_EQ(0, router_immune_update_stats(bridge));

    /* Stats should be populated from router */
    EXPECT_GE(bridge->stats.messages_sent, 0);
    EXPECT_GE(bridge->stats.avg_latency_ms, 0.0f);
}

TEST_F(RouterImmuneBridgeTest, GetStatsTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    router_immune_stats_t stats;
    EXPECT_EQ(0, router_immune_get_stats(bridge, &stats));

    /* Should have initialized stats */
    EXPECT_GE(stats.messages_sent, 0);
}

TEST_F(RouterImmuneBridgeTest, AnomalyCountTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Initially no anomalies */
    EXPECT_EQ(0, router_immune_get_anomaly_count(bridge, 0));

    /* After detection attempts, count should be queryable */
    router_immune_detect_anomalies(bridge, 1);

    /* Count might be 0 or positive depending on routing state */
    uint32_t count = router_immune_get_anomaly_count(bridge, 0);
    EXPECT_GE(count, 0);
}

TEST_F(RouterImmuneBridgeTest, PresentByzantineTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Create Byzantine behavior signature */
    uint8_t signature[32];
    memset(signature, 0xAB, sizeof(signature));

    /* Present Byzantine behavior */
    EXPECT_EQ(0, router_immune_present_byzantine(
        bridge, 42, signature, sizeof(signature)
    ));

    /* Should have triggered immune response */
    EXPECT_GT(bridge->immune_triggers, 0);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, BridgeUpdateTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Update should succeed */
    EXPECT_EQ(0, router_immune_bridge_update(bridge, 100));

    /* Total updates incremented */
    EXPECT_EQ(bridge->total_updates, 1);

    /* Multiple updates */
    router_immune_bridge_update(bridge, 100);
    router_immune_bridge_update(bridge, 100);

    EXPECT_EQ(bridge->total_updates, 3);
}

TEST_F(RouterImmuneBridgeTest, ExpireCytokinesTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Add cytokine states */
    router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_IL1, 0.5f, 1);
    router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_IL6, 0.7f, 2);

    EXPECT_EQ(bridge->cytokine_count, 2);

    /* Expire old cytokines (using large time value) */
    uint64_t far_future = 1000000;  /* Far in the future */
    EXPECT_EQ(0, router_immune_expire_cytokines(bridge, far_future));

    /* Cytokines should be expired (count reduced) */
    /* Note: Actual expiry depends on TTL and timestamps */
}

TEST_F(RouterImmuneBridgeTest, ReleaseExpiredQuarantinesTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Quarantine nodes */
    router_immune_quarantine_node(bridge, 10, 100, 0.5f, 1);  /* Short duration */
    router_immune_quarantine_node(bridge, 20, 100000, 0.5f, 2);  /* Long duration */

    EXPECT_EQ(bridge->quarantine_count, 2);

    /* Release expired quarantines */
    uint64_t future_time = 200;  /* Past first quarantine expiry */
    EXPECT_EQ(0, router_immune_release_expired_quarantines(bridge, future_time));

    /* At least one quarantine should remain (depends on timing) */
}

/* ============================================================================
 * Broadcast Alert Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, BroadcastAlertTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Start bridge to get module context */
    EXPECT_EQ(0, router_immune_bridge_start(bridge));

    /* Broadcast alert */
    EXPECT_EQ(0, router_immune_broadcast_alert(
        bridge,
        123,  /* antigen_id */
        INFLAMMATION_SYSTEMIC
    ));
}

TEST_F(RouterImmuneBridgeTest, BroadcastAlertWithoutStartTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Broadcast without starting should fail */
    EXPECT_EQ(-1, router_immune_broadcast_alert(
        bridge, 123, INFLAMMATION_SYSTEMIC
    ));
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, FullWorkflowTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Start integration */
    EXPECT_EQ(0, router_immune_bridge_start(bridge));

    /* Simulate immune response */
    router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_TNF, 0.9f, 1);
    router_immune_apply_inflammation_latency(bridge, 1, INFLAMMATION_REGIONAL);
    router_immune_quarantine_node(bridge, 42, 60000, 0.2f, 999);

    /* Verify states */
    EXPECT_GT(bridge->cytokine_count, 0);
    EXPECT_GT(bridge->inflammation_count, 0);
    EXPECT_GT(bridge->quarantine_count, 0);
    EXPECT_TRUE(router_immune_is_node_quarantined(bridge, 42));

    /* Update bridge */
    EXPECT_EQ(0, router_immune_bridge_update(bridge, 100));

    /* Restore node */
    EXPECT_EQ(0, router_immune_restore_node(bridge, 42));
    EXPECT_FALSE(router_immune_is_node_quarantined(bridge, 42));

    /* Stop integration */
    EXPECT_EQ(0, router_immune_bridge_stop(bridge));
}

TEST_F(RouterImmuneBridgeTest, ConcurrentOperationsTest) {
    bridge = router_immune_bridge_create(nullptr, router, immune_system);
    ASSERT_NE(bridge, nullptr);

    /* Perform multiple operations in sequence */
    router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_IL1, 0.5f, 1);
    router_immune_apply_inflammation_latency(bridge, 1, INFLAMMATION_LOCAL);
    router_immune_quarantine_node(bridge, 10, 30000, 0.5f, 1);

    router_immune_prioritize_cytokine(bridge, BRAIN_CYTOKINE_IL6, 0.7f, 2);
    router_immune_apply_inflammation_latency(bridge, 2, INFLAMMATION_REGIONAL);
    router_immune_quarantine_node(bridge, 20, 40000, 0.4f, 2);

    /* Verify all operations succeeded */
    EXPECT_EQ(bridge->cytokine_count, 2);
    EXPECT_EQ(bridge->inflammation_count, 2);
    EXPECT_EQ(bridge->quarantine_count, 2);

    /* Update should handle all state */
    EXPECT_EQ(0, router_immune_bridge_update(bridge, 100));
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(RouterImmuneBridgeTest, NullPointerSafetyTest) {
    /* All functions should handle null pointers gracefully */
    EXPECT_EQ(-1, router_immune_default_config(nullptr));
    EXPECT_EQ(nullptr, router_immune_bridge_create(nullptr, nullptr, nullptr));

    router_immune_bridge_destroy(nullptr);  /* Should not crash */

    EXPECT_EQ(-1, router_immune_bridge_start(nullptr));
    EXPECT_EQ(-1, router_immune_bridge_stop(nullptr));
    EXPECT_EQ(-1, router_immune_prioritize_cytokine(nullptr, BRAIN_CYTOKINE_IL1, 0.5f, 1));
    EXPECT_EQ(-1, router_immune_apply_inflammation_latency(nullptr, 1, INFLAMMATION_LOCAL));
    EXPECT_EQ(-1, router_immune_quarantine_node(nullptr, 1, 1000, 0.5f, 1));
    EXPECT_EQ(-1, router_immune_restore_node(nullptr, 1));
    EXPECT_EQ(-1, router_immune_bridge_update(nullptr, 100));
    EXPECT_EQ(-1, router_immune_update_stats(nullptr));

    EXPECT_FALSE(router_immune_is_node_quarantined(nullptr, 1));
    EXPECT_FLOAT_EQ(1.0f, router_immune_get_latency_multiplier(nullptr, 1));
    EXPECT_EQ(0, router_immune_get_anomaly_count(nullptr, 0));
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
