/**
 * @file test_security_level.cpp
 * @brief Unit tests for NIMCP Security Level Management
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "security/nimcp_security_level.h"
#include "async/nimcp_bio_router.h"

class SecurityLevelTest : public ::testing::Test {
protected:
    nimcp_security_state_t state;
    bio_router_t router;

    void SetUp() override {
        bio_router_init(NULL);  // Use default config
        router = bio_router_get_global();
        state = nullptr;
    }

    void TearDown() override {
        if (state) {
            nimcp_security_state_destroy(state);
        }
        bio_router_shutdown();
    }
};

/* Basic lifecycle tests */

TEST_F(SecurityLevelTest, CreateDestroy) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Default level should be STANDARD */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_STANDARD, nimcp_security_get_level(state));
    EXPECT_FALSE(nimcp_security_is_locked(state));
}

TEST_F(SecurityLevelTest, CreateWithConfig) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_ELEVATED;
    config.lock_on_create = false;
    config.max_components = 128;
    config.max_audit_entries = 2048;
    config.router = &router;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, nimcp_security_get_level(state));
    EXPECT_FALSE(nimcp_security_is_locked(state));
}

TEST_F(SecurityLevelTest, CreateLocked) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_MAXIMUM;
    config.lock_on_create = true;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM, nimcp_security_get_level(state));
    EXPECT_TRUE(nimcp_security_is_locked(state));
}

/* Level upgrade tests */

TEST_F(SecurityLevelTest, UpgradeLevel) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Upgrade from STANDARD to ELEVATED */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED));
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, nimcp_security_get_level(state));

    /* Further upgrade to MAXIMUM */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM));
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM, nimcp_security_get_level(state));
}

TEST_F(SecurityLevelTest, MultipleUpgrades) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_PARANOID));

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_PARANOID, nimcp_security_get_level(state));

    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);
    EXPECT_EQ(3, stats.level_upgrades);
}

/* Downgrade prevention tests */

TEST_F(SecurityLevelTest, PreventDowngrade) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Upgrade to ELEVATED */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED));

    /* Attempt downgrade to STANDARD - should fail */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD));

    /* Level should remain ELEVATED */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, nimcp_security_get_level(state));

    /* Check statistics */
    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);
    EXPECT_EQ(1, stats.level_downgrades_blocked);
}

TEST_F(SecurityLevelTest, PreventMultipleDowngrades) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);

    /* Multiple downgrade attempts */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED));
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD));
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MINIMAL));

    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);
    EXPECT_EQ(3, stats.level_downgrades_blocked);
}

/* Level locking tests */

TEST_F(SecurityLevelTest, LockLevel) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    EXPECT_FALSE(nimcp_security_is_locked(state));

    /* Lock at current level */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_lock_level(state));
    EXPECT_TRUE(nimcp_security_is_locked(state));

    /* Cannot change once locked */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED));
}

TEST_F(SecurityLevelTest, LockAfterUpgrade) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Upgrade then lock */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_lock_level(state);

    /* Cannot upgrade further */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_PARANOID));

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM, nimcp_security_get_level(state));
}

TEST_F(SecurityLevelTest, DoubleLock) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Double lock should be idempotent */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_lock_level(state));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_lock_level(state));

    EXPECT_TRUE(nimcp_security_is_locked(state));
}

/* Component-specific level tests */

TEST_F(SecurityLevelTest, ComponentLevel) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Set component level */
    EXPECT_EQ(NIMCP_SUCCESS,
              nimcp_security_set_component_level(state, "neural_network",
                                                 NIMCP_SECURITY_LEVEL_ELEVATED));

    /* Get component level */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED,
              nimcp_security_get_component_level(state, "neural_network"));

    /* Unknown component inherits global level */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_STANDARD,
              nimcp_security_get_component_level(state, "unknown"));
}

TEST_F(SecurityLevelTest, ComponentInheritance) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Component inherits global level initially */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_STANDARD,
              nimcp_security_get_component_level(state, "component1"));

    /* Upgrade global level */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);

    /* New components inherit new global level */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED,
              nimcp_security_get_component_level(state, "component2"));
}

TEST_F(SecurityLevelTest, ComponentCannotBeLowerThanGlobal) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Set global to ELEVATED */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);

    /* Cannot set component to lower level */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_component_level(state, "component",
                                                 NIMCP_SECURITY_LEVEL_STANDARD));
}

TEST_F(SecurityLevelTest, ComponentUpgrade) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Set component level */
    nimcp_security_set_component_level(state, "component", NIMCP_SECURITY_LEVEL_ELEVATED);

    /* Upgrade component level */
    EXPECT_EQ(NIMCP_SUCCESS,
              nimcp_security_set_component_level(state, "component",
                                                 NIMCP_SECURITY_LEVEL_MAXIMUM));

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM,
              nimcp_security_get_component_level(state, "component"));
}

TEST_F(SecurityLevelTest, ComponentDowngradeBlocked) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_component_level(state, "component", NIMCP_SECURITY_LEVEL_MAXIMUM);

    /* Cannot downgrade */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_set_component_level(state, "component",
                                                 NIMCP_SECURITY_LEVEL_ELEVATED));

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM,
              nimcp_security_get_component_level(state, "component"));
}

/* Emergency override tests */

TEST_F(SecurityLevelTest, EmergencyOverride) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Upgrade and lock */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_lock_level(state);

    /* Emergency override allows downgrade */
    EXPECT_EQ(NIMCP_SUCCESS,
              nimcp_security_emergency_override(state,
                                               NIMCP_SECURITY_LEVEL_STANDARD,
                                               "emergency_token_12345678",
                                               "Critical system issue"));

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_STANDARD, nimcp_security_get_level(state));
    EXPECT_FALSE(nimcp_security_is_locked(state));  /* Override unlocks */

    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);
    EXPECT_EQ(1, stats.emergency_overrides);
}

TEST_F(SecurityLevelTest, EmergencyOverrideInvalidToken) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_lock_level(state);

    /* Invalid token (too short) */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_security_emergency_override(state,
                                               NIMCP_SECURITY_LEVEL_STANDARD,
                                               "short",
                                               "Emergency"));

    /* Level should remain unchanged */
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MAXIMUM, nimcp_security_get_level(state));
}

/* Feature enablement tests */

TEST_F(SecurityLevelTest, FeaturesMinimalLevel) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_MINIMAL;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* At MINIMAL, most features disabled */
    EXPECT_FALSE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));
    EXPECT_FALSE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_RANGE_CHECK));
    EXPECT_FALSE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));
}

TEST_F(SecurityLevelTest, FeaturesStandardLevel) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* At STANDARD, basic features enabled */
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_RANGE_CHECK));
    EXPECT_FALSE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));
}

TEST_F(SecurityLevelTest, FeaturesElevatedLevel) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_ELEVATED;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* At ELEVATED, more features enabled */
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUDIT_LOG));
}

TEST_F(SecurityLevelTest, FeaturesMaximumLevel) {
    nimcp_security_state_config_t config = {};
    config.initial_level = NIMCP_SECURITY_LEVEL_MAXIMUM;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* At MAXIMUM, all features enabled */
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_ENCRYPTION));
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_MEMORY_ZEROING));
}

TEST_F(SecurityLevelTest, FeaturesChangeWithLevel) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* STANDARD - no auth */
    EXPECT_FALSE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));

    /* Upgrade to ELEVATED - auth enabled */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_AUTHENTICATION));
}

/* Audit trail tests */

TEST_F(SecurityLevelTest, AuditTrailRecordsChanges) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Make some changes */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);

    /* Get audit trail */
    nimcp_security_audit_entry_t entries[10];
    size_t count;

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_security_get_audit_trail(state, entries, 10, &count));
    EXPECT_GE(count, 2);  /* At least creation + 2 upgrades */
}

TEST_F(SecurityLevelTest, AuditTrailComponentChanges) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_component_level(state, "test_component",
                                       NIMCP_SECURITY_LEVEL_ELEVATED);

    /* Get audit trail */
    nimcp_security_audit_entry_t entries[10];
    size_t count;

    nimcp_security_get_audit_trail(state, entries, 10, &count);

    /* Find component entry */
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].component, "test_component") == 0) {
            found = true;
            EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, entries[i].new_level);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SecurityLevelTest, AuditTrailOverride) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_lock_level(state);

    nimcp_security_emergency_override(state, NIMCP_SECURITY_LEVEL_STANDARD,
                                     "token_1234567890", "Test override");

    /* Get audit trail */
    nimcp_security_audit_entry_t entries[10];
    size_t count;

    nimcp_security_get_audit_trail(state, entries, 10, &count);

    /* Find override entry */
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (entries[i].is_override) {
            found = true;
            EXPECT_STREQ("Test override", entries[i].reason);
            EXPECT_STREQ("token_1234567890", entries[i].authorization);
        }
    }
    EXPECT_TRUE(found);
}

/* Statistics tests */

TEST_F(SecurityLevelTest, Statistics) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);

    EXPECT_EQ(NIMCP_SECURITY_LEVEL_STANDARD, stats.current_level);
    EXPECT_FALSE(stats.is_locked);
    EXPECT_EQ(0, stats.component_count);
    EXPECT_EQ(0, stats.level_upgrades);
}

TEST_F(SecurityLevelTest, StatisticsTracking) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Perform operations */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);  /* No change */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD);  /* Blocked */
    nimcp_security_set_component_level(state, "comp1", NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_set_component_level(state, "comp2", NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK);

    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);

    EXPECT_EQ(1, stats.level_upgrades);
    EXPECT_EQ(1, stats.level_downgrades_blocked);
    EXPECT_EQ(2, stats.component_levels_set);
    EXPECT_EQ(2, stats.component_count);
    EXPECT_EQ(1, stats.feature_queries);
}

/* Utility function tests */

TEST_F(SecurityLevelTest, LevelNames) {
    EXPECT_STREQ("MINIMAL", nimcp_security_level_name(NIMCP_SECURITY_LEVEL_MINIMAL));
    EXPECT_STREQ("STANDARD", nimcp_security_level_name(NIMCP_SECURITY_LEVEL_STANDARD));
    EXPECT_STREQ("ELEVATED", nimcp_security_level_name(NIMCP_SECURITY_LEVEL_ELEVATED));
    EXPECT_STREQ("MAXIMUM", nimcp_security_level_name(NIMCP_SECURITY_LEVEL_MAXIMUM));
    EXPECT_STREQ("PARANOID", nimcp_security_level_name(NIMCP_SECURITY_LEVEL_PARANOID));
    EXPECT_STREQ("UNKNOWN", nimcp_security_level_name((nimcp_security_level_t)99));
}

/* Error handling tests */

TEST_F(SecurityLevelTest, NullStateHandling) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_security_set_level(nullptr, NIMCP_SECURITY_LEVEL_ELEVATED));
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_MINIMAL, nimcp_security_get_level(nullptr));
    EXPECT_FALSE(nimcp_security_is_locked(nullptr));
}

TEST_F(SecurityLevelTest, InvalidLevels) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_security_set_level(state, (nimcp_security_level_t)-1));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_security_set_level(state, (nimcp_security_level_t)99));
}

/* Thread safety tests */

TEST_F(SecurityLevelTest, ConcurrentReads) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);

    /* Multiple concurrent reads should be safe */
    #pragma omp parallel for
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, nimcp_security_get_level(state));
        EXPECT_TRUE(nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
