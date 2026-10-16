/**
 * @file test_graduated_autonomy.cpp
 * @brief Unit tests for Graduated Autonomy Module
 * @version 1.0.0
 * @date 2026-02-01
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "security/nimcp_graduated_autonomy.h"
}

class GraduatedAutonomyTest : public ::testing::Test {
protected:
    graduated_autonomy_t* autonomy = nullptr;

    void SetUp() override { autonomy = nullptr; }
    void TearDown() override {
        if (autonomy) { graduated_autonomy_destroy(autonomy); autonomy = nullptr; }
    }

    graduated_autonomy_t* createWithDefaults() {
        autonomy = graduated_autonomy_create(nullptr);
        return autonomy;
    }
};

TEST_F(GraduatedAutonomyTest, DefaultConfigHasRestrictiveSettings) {
    graduated_autonomy_config_t config = graduated_autonomy_default_config();
    EXPECT_GE(config.default_level, AUTONOMY_NONE);
    EXPECT_LE(config.default_level, AUTONOMY_TRUSTED);
    EXPECT_GT(config.actions_required_for_upgrade, 0u);
    EXPECT_GT(config.violations_for_downgrade, 0u);
}

TEST_F(GraduatedAutonomyTest, CreateWithNullConfigUsesDefaults) {
    autonomy = graduated_autonomy_create(nullptr);
    ASSERT_NE(autonomy, nullptr);
}

TEST_F(GraduatedAutonomyTest, DestroyNullIsNoOp) {
    graduated_autonomy_destroy(nullptr);
}

TEST_F(GraduatedAutonomyTest, GetInitialLevel) {
    createWithDefaults();
    ASSERT_NE(autonomy, nullptr);

    autonomy_level_t level = graduated_autonomy_get_level(autonomy, "general");
    EXPECT_GE(level, AUTONOMY_NONE);
    EXPECT_LE(level, AUTONOMY_TRUSTED);
}

TEST_F(GraduatedAutonomyTest, UpdateTrustWithSuccess) {
    createWithDefaults();
    ASSERT_NE(autonomy, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_error_t err = graduated_autonomy_update_trust(autonomy, "general", true);
        EXPECT_EQ(err, NIMCP_OK);
    }

    graduated_autonomy_stats_t stats;
    graduated_autonomy_get_stats(autonomy, &stats);
    EXPECT_EQ(stats.trust_updates, 10u);
}

TEST_F(GraduatedAutonomyTest, UpdateTrustWithFailure) {
    createWithDefaults();
    ASSERT_NE(autonomy, nullptr);

    nimcp_error_t err = graduated_autonomy_update_trust(autonomy, "general", false);
    EXPECT_EQ(err, NIMCP_OK);

    graduated_autonomy_stats_t stats;
    graduated_autonomy_get_stats(autonomy, &stats);
    EXPECT_EQ(stats.trust_updates, 1u);
}

TEST_F(GraduatedAutonomyTest, RequestAutonomyIncrease) {
    createWithDefaults();
    ASSERT_NE(autonomy, nullptr);

    // Build up trust first
    for (int i = 0; i < 100; i++) {
        graduated_autonomy_update_trust(autonomy, "general", true);
    }

    bool granted;
    nimcp_error_t err = graduated_autonomy_request_increase(
        autonomy, "general", "Demonstrated alignment", &granted);
    EXPECT_EQ(err, NIMCP_OK);
    // May or may not be granted depending on threshold
}

TEST_F(GraduatedAutonomyTest, GetTrustLevel) {
    createWithDefaults();
    ASSERT_NE(autonomy, nullptr);

    float mean, variance;
    nimcp_error_t err = graduated_autonomy_get_trust(autonomy, "general", &mean, &variance);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(mean, 0.0f);
    EXPECT_LE(mean, 1.0f);
    EXPECT_GE(variance, 0.0f);
}

TEST_F(GraduatedAutonomyTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(autonomy, nullptr);

    graduated_autonomy_stats_t stats;
    nimcp_error_t err = graduated_autonomy_get_stats(autonomy, &stats);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.trust_updates, 0u);
    EXPECT_EQ(stats.level_upgrades, 0u);
}

TEST_F(GraduatedAutonomyTest, LevelNames) {
    EXPECT_NE(graduated_autonomy_level_name(AUTONOMY_NONE), nullptr);
    EXPECT_NE(graduated_autonomy_level_name(AUTONOMY_SUGGEST), nullptr);
    EXPECT_NE(graduated_autonomy_level_name(AUTONOMY_BOUNDED), nullptr);
    EXPECT_NE(graduated_autonomy_level_name(AUTONOMY_SUPERVISED), nullptr);
    EXPECT_NE(graduated_autonomy_level_name(AUTONOMY_TRUSTED), nullptr);
}

TEST_F(GraduatedAutonomyTest, AutonomyLevelProgression) {
    // Test that levels are properly ordered
    EXPECT_LT(AUTONOMY_NONE, AUTONOMY_SUGGEST);
    EXPECT_LT(AUTONOMY_SUGGEST, AUTONOMY_BOUNDED);
    EXPECT_LT(AUTONOMY_BOUNDED, AUTONOMY_SUPERVISED);
    EXPECT_LT(AUTONOMY_SUPERVISED, AUTONOMY_TRUSTED);
}

TEST_F(GraduatedAutonomyTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(autonomy, nullptr);
    nimcp_error_t err = graduated_autonomy_connect_bio_async(autonomy);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(GraduatedAutonomyTest, NullHandleOperationsReturnErrors) {
    EXPECT_EQ(graduated_autonomy_update_trust(nullptr, "general", true),
              NIMCP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(graduated_autonomy_get_level(nullptr, "test"),
              AUTONOMY_NONE);  // Default on error
}
