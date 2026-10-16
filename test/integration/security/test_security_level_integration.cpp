/**
 * @file test_security_level_integration.cpp
 * @brief Integration tests for NIMCP Security Level Management
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
// Headers have their own extern "C" guards
#include "security/nimcp_security_level.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_router.h"

class SecurityLevelIntegrationTest : public ::testing::Test {
protected:
    nimcp_security_state_t state;
    bio_router_t* router;
    inbox_t* inbox;

    void SetUp() override {
        router = bio_router_create();
        inbox = inbox_create(router, 32);
        inbox_subscribe(inbox, "security.level.changed");
        inbox_subscribe(inbox, "security.level.response");

        state = nullptr;
    }

    void TearDown() override {
        if (state) {
            nimcp_security_state_destroy(state);
        }
        if (inbox) {
            inbox_destroy(inbox);
        }
        if (router) {
            bio_router_destroy(router);
        }
    }
};

/* Bio-async integration tests */

TEST_F(SecurityLevelIntegrationTest, LevelChangeNotification) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_STANDARD;
    config.router = router;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Upgrade level */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);

    /* Give time for notification to propagate */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Should receive notification */
    bio_message_t* msg = inbox_poll(inbox);
    ASSERT_NE(nullptr, msg);

    const char* topic = bio_message_get_topic(msg);
    EXPECT_STREQ("security.level.changed", topic);

    const char* payload = (const char*)bio_message_get_payload(msg);
    EXPECT_TRUE(strstr(payload, "STANDARD") != nullptr);
    EXPECT_TRUE(strstr(payload, "ELEVATED") != nullptr);

    bio_message_destroy(msg);
}

TEST_F(SecurityLevelIntegrationTest, ComponentLevelChangeNotification) {
    nimcp_security_state_config_t config = {};
    config.router = router;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Set component level */
    nimcp_security_set_component_level(state, "neural_network",
                                       NIMCP_SECURITY_LEVEL_MAXIMUM);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Should receive notification with component name */
    bio_message_t* msg = inbox_poll(inbox);
    ASSERT_NE(nullptr, msg);

    const char* payload = (const char*)bio_message_get_payload(msg);
    EXPECT_TRUE(strstr(payload, "neural_network") != nullptr);
    EXPECT_TRUE(strstr(payload, "MAXIMUM") != nullptr);

    bio_message_destroy(msg);
}

TEST_F(SecurityLevelIntegrationTest, MultipleLevelChanges) {
    nimcp_security_state_config_t config = {};
    config.router = router;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Multiple changes */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* Should receive both notifications */
    int notifications = 0;
    bio_message_t* msg;
    while ((msg = inbox_poll(inbox)) != nullptr) {
        notifications++;
        bio_message_destroy(msg);
    }

    EXPECT_EQ(2, notifications);
}

/* Multi-component integration tests */

TEST_F(SecurityLevelIntegrationTest, MultipleComponents) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Set levels for multiple components */
    const char* components[] = {
        "neural_network",
        "memory_system",
        "plasticity_engine",
        "cognitive_module",
        "emotion_system"
    };

    for (int i = 0; i < 5; i++) {
        nimcp_security_set_component_level(state, components[i],
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    }

    /* Verify all components have correct level */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED,
                 nimcp_security_get_component_level(state, components[i]));
    }

    /* Check statistics */
    nimcp_security_state_stats_t stats;
    nimcp_security_get_stats(state, &stats);
    EXPECT_EQ(5, stats.component_count);
    EXPECT_EQ(5, stats.component_levels_set);
}

TEST_F(SecurityLevelIntegrationTest, ComponentHierarchy) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Set global to ELEVATED */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);

    /* Set specific component to MAXIMUM */
    nimcp_security_set_component_level(state, "critical_component",
                                       NIMCP_SECURITY_LEVEL_MAXIMUM);

    /* Critical component should be MAXIMUM */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM,
             nimcp_security_get_component_level(state, "critical_component"));

    /* Other components inherit ELEVATED */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED,
             nimcp_security_get_component_level(state, "other_component"));
}

/* Feature enablement integration tests */

TEST_F(SecurityLevelIntegrationTest, FeatureEnablementProgression) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* STANDARD - some features enabled */
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));
    EXPECT_FALSE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_ENCRYPTION));

    /* Upgrade to ELEVATED */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));

    /* Upgrade to MAXIMUM */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_ENCRYPTION));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_MEMORY_ZEROING));
}

/* Audit trail integration tests */

TEST_F(SecurityLevelIntegrationTest, CompleteAuditTrail) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Perform series of operations */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    nimcp_security_set_component_level(state, "comp1", NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_lock_level(state);

    /* Get full audit trail */
    nimcp_security_audit_entry_t entries[20];
    size_t count;

    nimcp_security_get_audit_trail(state, entries, 20, &count);

    /* Should have entries for creation, upgrade, component set, and lock */
    EXPECT_GE(count, 3);

    /* Verify entries are in chronological order */
    for (size_t i = 1; i < count; i++) {
        EXPECT_GE(entries[i].timestamp, entries[i-1].timestamp);
    }
}

TEST_F(SecurityLevelIntegrationTest, AuditTrailOverride) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Upgrade and lock */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_lock_level(state);

    /* Emergency override */
    nimcp_security_emergency_override(state,
                                     NIMCP_SECURITY_LEVEL_ELEVATED,
                                     "emergency_token_123456",
                                     "Critical bug requires downgrade");

    /* Verify override in audit trail */
    nimcp_security_audit_entry_t entries[20];
    size_t count;
    nimcp_security_get_audit_trail(state, entries, 20, &count);

    bool found_override = false;
    for (size_t i = 0; i < count; i++) {
        if (entries[i].is_override) {
            found_override = true;
            EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM, entries[i].old_level);
            EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, entries[i].new_level);
            EXPECT_STREQ("Critical bug requires downgrade", entries[i].reason);
            EXPECT_STREQ("emergency_token_123456", entries[i].authorization);
        }
    }

    EXPECT_TRUE(found_override);
}

/* Stress and longevity tests */

TEST_F(SecurityLevelIntegrationTest, ManyComponents) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Create many components */
    char comp_name[32];
    for (int i = 0; i < 100; i++) {
        snprintf(comp_name, sizeof(comp_name), "component_%03d", i);
        nimcp_security_set_component_level(state, comp_name,
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    }

    /* Verify all were created */
    nimcp_security_state_stats_t stats;
    nimcp_security_get_stats(state, &stats);
    EXPECT_EQ(100, stats.component_count);

    /* Verify random components */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED,
             nimcp_security_get_component_level(state, "component_042"));
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED,
             nimcp_security_get_component_level(state, "component_099"));
}

TEST_F(SecurityLevelIntegrationTest, LargeAuditTrail) {
    nimcp_security_state_config_t config = {};
    config.max_audit_entries = 1000;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Generate many audit entries */
    char comp_name[32];
    for (int i = 0; i < 500; i++) {
        snprintf(comp_name, sizeof(comp_name), "comp_%d", i);
        nimcp_security_set_component_level(state, comp_name,
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    }

    /* Get audit trail */
    nimcp_security_audit_entry_t entries[1000];
    size_t count;
    nimcp_security_get_audit_trail(state, entries, 1000, &count);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, 1000);
}

/* Concurrent access tests */

TEST_F(SecurityLevelIntegrationTest, ConcurrentReadsAndWrites) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    std::atomic<bool> stop{false};
    std::atomic<int> errors{0};

    /* Reader thread */
    std::thread reader([&]() {
        while (!stop) {
            nimcp_security_level_t level = nimcp_security_get_level(state);
            if (level < NIMCP_SECURITY_LEVEL_MINIMAL ||
                level > NIMCP_SECURITY_LEVEL_PARANOID) {
                errors++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    /* Writer thread */
    std::thread writer([&]() {
        for (int i = 0; i < 10; i++) {
            nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    writer.join();
    stop = true;
    reader.join();

    EXPECT_EQ(0, errors);
}

/* Real-world scenario tests */

TEST_F(SecurityLevelIntegrationTest, ProductionScenario) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_STANDARD;
    config.router = router;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Normal operation */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_STANDARD, nimcp_security_get_level(state));

    /* Detected security event - elevate */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);

    /* Lock to prevent accidental downgrade */
    nimcp_security_lock_level(state);

    /* Verify locked at elevated */
    EXPECT_TRUE(nimcp_security_is_locked(state));
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, nimcp_security_get_level(state));

    /* Attempt to downgrade fails */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD));

    /* After resolving issue, authorized downgrade */
    EXPECT_EQ(NIMCP_SUCCESS,
              nimcp_security_emergency_override(state,
                                               NIMCP_SECURITY_LEVEL_STANDARD,
                                               "authorized_token_1234567890",
                                               "Issue resolved, downgrading"));

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_STANDARD, nimcp_security_get_level(state));
}

TEST_F(SecurityLevelIntegrationTest, DevelopmentToProductionTransition) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_MINIMAL;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Development: minimal checks */
    EXPECT_FALSE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));

    /* Testing: standard checks */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD);
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));

    /* Production: elevated security */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));

    /* Lock for production */
    nimcp_security_lock_level(state);
    EXPECT_TRUE(nimcp_security_is_locked(state));

    /* Verify audit trail shows progression */
    nimcp_security_audit_entry_t entries[20];
    size_t count;
    nimcp_security_get_audit_trail(state, entries, 20, &count);

    EXPECT_GE(count, 3);  /* Creation + 2 upgrades + lock */
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
