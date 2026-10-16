/**
 * @file test_security_level_regression.cpp
 * @brief Regression tests for NIMCP Security Level Management
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
// Headers have their own extern "C" guards
#include "security/nimcp_security_level.h"
#include "async/nimcp_bio_router.h"

class SecurityLevelRegressionTest : public ::testing::Test {
protected:
    nimcp_security_state_t state;
    bio_router_t router;

    void SetUp() override {
        bio_router_init(NULL);
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

/* Memory leak regression tests */

TEST_F(SecurityLevelRegressionTest, NoMemoryLeakOnCreateDestroy) {
    /* Create and destroy many times */
    for (int i = 0; i < 1000; i++) {
        nimcp_security_state_t temp = nimcp_security_state_create(nullptr);
        ASSERT_NE(nullptr, temp);
        nimcp_security_state_destroy(temp);
    }
}

TEST_F(SecurityLevelRegressionTest, NoMemoryLeakWithComponents) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Add and query many components */
    char comp_name[64];
    for (int i = 0; i < 1000; i++) {
        snprintf(comp_name, sizeof(comp_name), "component_%d", i);
        nimcp_security_set_component_level(state, comp_name,
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    }

    /* Verify no corruption */
    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);
    EXPECT_EQ(1000, stats.component_count);
}

TEST_F(SecurityLevelRegressionTest, NoMemoryLeakWithAuditTrail) {
    nimcp_security_state_config_t config = {};
    config.max_audit_entries = 100;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Generate many more entries than capacity */
    for (int i = 0; i < 500; i++) {
        nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
        nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);

        /* Override to reset */
        char token[32];
        snprintf(token, sizeof(token), "token_%d_1234567890", i);
        nimcp_security_emergency_override(state, NIMCP_SECURITY_LEVEL_STANDARD,
                                         token, "Reset");
    }

    /* Audit should be bounded */
    nimcp_security_audit_entry_t entries[200];
    size_t count;
    nimcp_security_get_audit_trail(state, entries, 200, &count);
    EXPECT_LE(count, 100);
}

/* Downgrade prevention regression tests */

TEST_F(SecurityLevelRegressionTest, DowngradeAlwaysBlocked) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Systematically test all downgrade combinations */
    nimcp_security_level_t levels[] = {
        NIMCP_SECURITY_LEVEL_MINIMAL,
        NIMCP_SECURITY_LEVEL_STANDARD,
        NIMCP_SECURITY_LEVEL_ELEVATED,
        NIMCP_SECURITY_LEVEL_MAXIMUM,
        NIMCP_SECURITY_LEVEL_PARANOID
    };

    for (int i = 1; i < 5; i++) {
        /* Reset state */
        nimcp_security_emergency_override(state, levels[i],
                                         "reset_token_1234567890", "Reset");

        /* Try to downgrade to any lower level */
        for (int j = 0; j < i; j++) {
            EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
                     nimcp_security_set_level(state, levels[j]))
                << "Should block downgrade from " << i << " to " << j;
        }
    }
}

TEST_F(SecurityLevelRegressionTest, ComponentDowngradeAlwaysBlocked) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_component_level(state, "test",
                                       NIMCP_SECURITY_LEVEL_MAXIMUM);

    /* All downgrades should fail */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
             nimcp_security_set_component_level(state, "test",
                                                NIMCP_SECURITY_LEVEL_ELEVATED));
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
             nimcp_security_set_component_level(state, "test",
                                                NIMCP_SECURITY_LEVEL_STANDARD));
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
             nimcp_security_set_component_level(state, "test",
                                                NIMCP_SECURITY_LEVEL_MINIMAL));
}

/* Lock enforcement regression tests */

TEST_F(SecurityLevelRegressionTest, LockPreventsAllChanges) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    nimcp_security_lock_level(state);

    /* All changes should be blocked */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
             nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM));
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
             nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_PARANOID));
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
             nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD));
}

/* Feature enablement consistency regression tests */

TEST_F(SecurityLevelRegressionTest, FeatureEnablementConsistent) {
    nimcp_security_state_config_t config = {};

    /* Test each level */
    for (int level = 0; level <= 4; level++) {
        config.initial_level = (nimcp_security_level_t)level;
        nimcp_security_state_t temp = nimcp_security_state_create(&config);

        /* Query each feature multiple times - should be consistent */
        for (int feature = 0; feature < NIMCP_SECURITY_FEATURE_COUNT; feature++) {
            bool first = nimcp_security_feature_enabled(temp,
                                                       (nimcp_security_feature_t)feature);

            for (int i = 0; i < 100; i++) {
                bool current = nimcp_security_feature_enabled(temp,
                                                             (nimcp_security_feature_t)feature);
                EXPECT_EQ(first, current)
                    << "Feature enablement inconsistent for level " << level
                    << " feature " << feature;
            }
        }

        nimcp_security_state_destroy(temp);
    }
}

/* Audit trail integrity regression tests */

TEST_F(SecurityLevelRegressionTest, AuditTrailChronological) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Perform many operations */
    for (int i = 0; i < 50; i++) {
        char comp[32];
        snprintf(comp, sizeof(comp), "comp%d", i);
        nimcp_security_set_component_level(state, comp,
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    /* Get audit trail */
    nimcp_security_audit_entry_t entries[100];
    size_t count;
    nimcp_security_get_audit_trail(state, entries, 100, &count);

    /* Verify chronological order */
    for (size_t i = 1; i < count; i++) {
        EXPECT_GE(entries[i].timestamp, entries[i-1].timestamp)
            << "Audit trail not chronological at index " << i;
    }
}

TEST_F(SecurityLevelRegressionTest, AuditTrailNoDuplicates) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Make several changes */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
    nimcp_security_set_component_level(state, "comp1",
                                       NIMCP_SECURITY_LEVEL_MAXIMUM);
    nimcp_security_lock_level(state);

    /* Get audit trail multiple times */
    nimcp_security_audit_entry_t entries1[20], entries2[20];
    size_t count1, count2;

    nimcp_security_get_audit_trail(state, entries1, 20, &count1);
    nimcp_security_get_audit_trail(state, entries2, 20, &count2);

    /* Should be identical */
    EXPECT_EQ(count1, count2);
    for (size_t i = 0; i < count1; i++) {
        EXPECT_EQ(entries1[i].timestamp, entries2[i].timestamp);
        EXPECT_EQ(entries1[i].old_level, entries2[i].old_level);
        EXPECT_EQ(entries1[i].new_level, entries2[i].new_level);
    }
}

/* Statistics accuracy regression tests */

TEST_F(SecurityLevelRegressionTest, StatisticsAccurate) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Perform known operations */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);      /* 1 upgrade */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);       /* 2 upgrades */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD);      /* 1 blocked */
    nimcp_security_set_component_level(state, "c1", NIMCP_SECURITY_LEVEL_MAXIMUM);  /* 1 comp */
    nimcp_security_set_component_level(state, "c2", NIMCP_SECURITY_LEVEL_MAXIMUM);  /* 2 comp */
    nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_NAN_CHECK);        /* 1 query */
    nimcp_security_feature_enabled(state, NIMCP_SECURITY_FEATURE_ENCRYPTION);       /* 2 queries */

    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);

    EXPECT_EQ(2, stats.level_upgrades);
    EXPECT_EQ(1, stats.level_downgrades_blocked);
    EXPECT_EQ(2, stats.component_levels_set);
    EXPECT_EQ(2, stats.component_count);
    EXPECT_EQ(2, stats.feature_queries);
}

/* Thread safety regression tests */

TEST_F(SecurityLevelRegressionTest, ConcurrentAccessSafe) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    std::atomic<int> errors{0};
    std::atomic<bool> stop{false};

    /* Multiple reader threads */
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; i++) {
        readers.emplace_back([&]() {
            while (!stop) {
                nimcp_security_level_t level = nimcp_security_get_level(state);
                if (level < NIMCP_SECURITY_LEVEL_MINIMAL ||
                    level > NIMCP_SECURITY_LEVEL_PARANOID) {
                    errors++;
                }

                nimcp_security_state_stats_t stats;
                if (nimcp_security_level_get_stats(state, &stats) != NIMCP_SUCCESS) {
                    errors++;
                }
            }
        });
    }

    /* Writer thread */
    std::thread writer([&]() {
        for (int i = 0; i < 100; i++) {
            nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_ELEVATED);
            nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);

            char comp[32];
            snprintf(comp, sizeof(comp), "comp%d", i);
            nimcp_security_set_component_level(state, comp,
                                               NIMCP_SECURITY_LEVEL_ELEVATED);
        }
    });

    writer.join();
    stop = true;

    for (auto& reader : readers) {
        reader.join();
    }

    EXPECT_EQ(0, errors);
}

/* Component hash collision regression tests */

TEST_F(SecurityLevelRegressionTest, ComponentHashCollisionsHandled) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Create many components that might hash to same bucket */
    for (int i = 0; i < 200; i++) {
        char comp[64];
        snprintf(comp, sizeof(comp), "component_with_long_name_%d", i);
        nimcp_security_set_component_level(state, comp,
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    }

    /* Verify all components can be retrieved */
    for (int i = 0; i < 200; i++) {
        char comp[64];
        snprintf(comp, sizeof(comp), "component_with_long_name_%d", i);
        EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED,
                 nimcp_security_get_component_level(state, comp))
            << "Failed to retrieve component " << i;
    }
}

/* Boundary condition regression tests */

TEST_F(SecurityLevelRegressionTest, MaxComponentsHandled) {
    nimcp_security_state_config_t config = {};
    config.max_components = 128;

    state = nimcp_security_state_create(&config);
    ASSERT_NE(nullptr, state);

    /* Add many components */
    char comp[64];
    for (int i = 0; i < 256; i++) {
        snprintf(comp, sizeof(comp), "comp_%d", i);
        nimcp_security_set_component_level(state, comp,
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    }

    /* Should handle gracefully even beyond max */
    nimcp_security_state_stats_t stats;
    nimcp_security_level_get_stats(state, &stats);
    EXPECT_GT(stats.component_count, 0);
}

TEST_F(SecurityLevelRegressionTest, LongComponentNames) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Ensure global level allows component level to be set to ELEVATED */
    nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_STANDARD);

    /* Long but reasonable component name (63 chars, typical max identifier) */
    char long_name[64];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    /* Should handle gracefully */
    nimcp_error_t err = nimcp_security_set_component_level(state, long_name,
                                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    EXPECT_EQ(NIMCP_SUCCESS, err);

    /* Should be retrievable */
    nimcp_security_level_t level = nimcp_security_get_component_level(state, long_name);
    EXPECT_EQ(NIMCP_SECURITY_LEVEL_ELEVATED, level);
}

/* Emergency override regression tests */

TEST_F(SecurityLevelRegressionTest, OverrideAlwaysUnlocks) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Test override unlocking at each level */
    for (int i = 0; i < 5; i++) {
        nimcp_security_set_level(state, NIMCP_SECURITY_LEVEL_MAXIMUM);
        nimcp_security_lock_level(state);

        EXPECT_TRUE(nimcp_security_is_locked(state));

        nimcp_security_emergency_override(state,
                                         NIMCP_SECURITY_LEVEL_STANDARD,
                                         "override_token_12345678",
                                         "Test");

        EXPECT_FALSE(nimcp_security_is_locked(state))
            << "Override failed to unlock on iteration " << i;
    }
}

/* Performance regression tests */

TEST_F(SecurityLevelRegressionTest, LevelQueryPerformance) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    auto start = std::chrono::high_resolution_clock::now();

    /* Many level queries */
    for (int i = 0; i < 100000; i++) {
        nimcp_security_get_level(state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should complete quickly (< 100ms for 100k queries) */
    EXPECT_LT(duration.count(), 100)
        << "Level queries too slow: " << duration.count() << "ms";
}

TEST_F(SecurityLevelRegressionTest, ComponentQueryPerformance) {
    state = nimcp_security_state_create(nullptr);
    ASSERT_NE(nullptr, state);

    /* Create components */
    for (int i = 0; i < 100; i++) {
        char comp[32];
        snprintf(comp, sizeof(comp), "comp%d", i);
        nimcp_security_set_component_level(state, comp,
                                           NIMCP_SECURITY_LEVEL_ELEVATED);
    }

    auto start = std::chrono::high_resolution_clock::now();

    /* Query existing components */
    for (int i = 0; i < 10000; i++) {
        nimcp_security_get_component_level(state, "comp50");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should complete quickly */
    EXPECT_LT(duration.count(), 50)
        << "Component queries too slow: " << duration.count() << "ms";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
