/**
 * @file test_offline.cpp
 * @brief GoogleTest unit tests for NIMCP edge offline degradation policy
 *
 * Tests mode transitions (NORMAL -> CAUTIOUS -> CONSERVATIVE -> FROZEN),
 * confidence decay, LR scaling, and sync recovery.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class OfflineTest : public ::testing::Test {
protected:
    nimcp_offline_policy_t policy;

    void SetUp() override {
        memset(&policy, 0, sizeof(policy));
    }
};

TEST_F(OfflineTest, InitStartsNormalFullConfidence) {
    int ret = nimcp_offline_policy_init(&policy);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_NORMAL);

    float conf = nimcp_offline_get_confidence(&policy);
    EXPECT_FLOAT_EQ(conf, 1.0f);
}

TEST_F(OfflineTest, StepThroughToCautious) {
    nimcp_offline_policy_init(&policy);

    // Step until cautious mode
    uint32_t cautious_steps = policy.cautious_after_steps;
    ASSERT_GT(cautious_steps, 0u);

    for (uint32_t i = 0; i < cautious_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }

    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_CAUTIOUS);
}

TEST_F(OfflineTest, StepThroughToConservative) {
    nimcp_offline_policy_init(&policy);

    uint32_t conservative_steps = policy.conservative_after_steps;
    ASSERT_GT(conservative_steps, 0u);

    for (uint32_t i = 0; i < conservative_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }

    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_CONSERVATIVE);
}

TEST_F(OfflineTest, StepThroughToFrozen) {
    nimcp_offline_policy_init(&policy);

    uint32_t frozen_steps = policy.frozen_after_steps;
    ASSERT_GT(frozen_steps, 0u);

    for (uint32_t i = 0; i < frozen_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }

    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_FROZEN);
}

TEST_F(OfflineTest, OnSyncResetsToNormal) {
    nimcp_offline_policy_init(&policy);

    // Go to frozen
    for (uint32_t i = 0; i < policy.frozen_after_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }
    ASSERT_EQ(policy.current_mode, NIMCP_OFFLINE_FROZEN);

    nimcp_offline_policy_on_sync(&policy);
    EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_NORMAL);

    float conf = nimcp_offline_get_confidence(&policy);
    EXPECT_FLOAT_EQ(conf, 1.0f);
}

TEST_F(OfflineTest, ConfidenceDecaysMonotonically) {
    nimcp_offline_policy_init(&policy);

    float prev_conf = nimcp_offline_get_confidence(&policy);
    bool monotonic = true;

    for (int i = 0; i < 100; i++) {
        nimcp_offline_policy_step(&policy);
        float conf = nimcp_offline_get_confidence(&policy);
        if (conf > prev_conf) {
            monotonic = false;
            break;
        }
        prev_conf = conf;
    }

    EXPECT_TRUE(monotonic) << "Confidence should decay monotonically";
}

TEST_F(OfflineTest, LRScaleReducesWithMode) {
    nimcp_offline_policy_init(&policy);

    float lr_normal = nimcp_offline_get_lr_scale(&policy);

    // Advance to cautious
    for (uint32_t i = 0; i < policy.cautious_after_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }
    float lr_cautious = nimcp_offline_get_lr_scale(&policy);

    // Advance to conservative
    for (uint32_t i = policy.cautious_after_steps; i < policy.conservative_after_steps; i++) {
        nimcp_offline_policy_step(&policy);
    }
    float lr_conservative = nimcp_offline_get_lr_scale(&policy);

    EXPECT_GE(lr_normal, lr_cautious);
    EXPECT_GE(lr_cautious, lr_conservative);
}

TEST_F(OfflineTest, MinConfidenceFloorRespected) {
    nimcp_offline_policy_init(&policy);

    // Step far beyond frozen
    for (int i = 0; i < 100000; i++) {
        nimcp_offline_policy_step(&policy);
    }

    float conf = nimcp_offline_get_confidence(&policy);
    EXPECT_GE(conf, policy.min_confidence_multiplier);
    EXPECT_GE(conf, 0.0f);
}

TEST_F(OfflineTest, RepeatedSyncsKeepConfidenceHigh) {
    nimcp_offline_policy_init(&policy);

    for (int sync = 0; sync < 5; sync++) {
        // Step a bit
        for (int i = 0; i < 100; i++) {
            nimcp_offline_policy_step(&policy);
        }
        nimcp_offline_policy_on_sync(&policy);

        float conf = nimcp_offline_get_confidence(&policy);
        EXPECT_FLOAT_EQ(conf, 1.0f);
        EXPECT_EQ(policy.current_mode, NIMCP_OFFLINE_NORMAL);
    }
}

TEST_F(OfflineTest, StepsSinceSyncIncrements) {
    nimcp_offline_policy_init(&policy);
    EXPECT_EQ(policy.steps_since_sync, 0u);

    nimcp_offline_policy_step(&policy);
    EXPECT_EQ(policy.steps_since_sync, 1u);

    nimcp_offline_policy_step(&policy);
    EXPECT_EQ(policy.steps_since_sync, 2u);

    nimcp_offline_policy_on_sync(&policy);
    EXPECT_EQ(policy.steps_since_sync, 0u);
}
