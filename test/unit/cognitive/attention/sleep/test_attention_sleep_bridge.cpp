/**
 * @file test_attention_sleep_bridge.cpp
 * @brief Unit tests for Attention-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "cognitive/attention/nimcp_attention_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

class AttentionSleepHelperTest : public ::testing::Test {};

/* Capacity Factor Tests */
TEST_F(AttentionSleepHelperTest, CapacityFactorAwake) {
    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(SLEEP_STATE_AWAKE),
                    ATTN_SLEEP_CAPACITY_AWAKE);
}

TEST_F(AttentionSleepHelperTest, CapacityFactorDrowsy) {
    float cap = attention_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(cap, ATTN_SLEEP_CAPACITY_DROWSY);
    EXPECT_LT(cap, 1.0f) << "Drowsy reduces attention capacity";
}

TEST_F(AttentionSleepHelperTest, CapacityFactorLightNREM) {
    float cap = attention_sleep_capacity_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(cap, ATTN_SLEEP_CAPACITY_LIGHT_NREM);
    EXPECT_LT(cap, 0.2f) << "Minimal attention in light NREM";
}

TEST_F(AttentionSleepHelperTest, CapacityFactorDeepNREM) {
    float cap = attention_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(cap, ATTN_SLEEP_CAPACITY_DEEP_NREM);
    EXPECT_FLOAT_EQ(cap, 0.0f) << "No external attention in deep NREM";
}

TEST_F(AttentionSleepHelperTest, CapacityFactorREM) {
    float cap = attention_sleep_capacity_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(cap, ATTN_SLEEP_CAPACITY_REM);
    EXPECT_GT(cap, 0.0f) << "Some internal attention in REM (dreams)";
}

/* Vigilance Factor Tests */
TEST_F(AttentionSleepHelperTest, VigilanceFactorAwake) {
    EXPECT_FLOAT_EQ(attention_sleep_vigilance_for_state(SLEEP_STATE_AWAKE),
                    ATTN_SLEEP_VIGILANCE_AWAKE);
}

TEST_F(AttentionSleepHelperTest, VigilanceFactorDrowsy) {
    float vig = attention_sleep_vigilance_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(vig, ATTN_SLEEP_VIGILANCE_DROWSY);
    EXPECT_LT(vig, 1.0f) << "Vigilance drops when drowsy";
}

TEST_F(AttentionSleepHelperTest, VigilanceFactorNREM) {
    float vig = attention_sleep_vigilance_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(vig, ATTN_SLEEP_VIGILANCE_NREM);
    EXPECT_FLOAT_EQ(vig, 0.0f) << "No vigilance during NREM";
}

TEST_F(AttentionSleepHelperTest, VigilanceFactorREM) {
    float vig = attention_sleep_vigilance_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(vig, ATTN_SLEEP_VIGILANCE_REM);
    EXPECT_GT(vig, 0.0f) << "Some vigilance in REM (can be awakened)";
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(AttentionSleepHelperTest, DefaultConfigValid) {
    attention_sleep_config_t config;
    ASSERT_EQ(attention_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_capacity_modulation);
    EXPECT_TRUE(config.enable_vigilance_modulation);
    EXPECT_GT(config.modulation_strength, 0.0f);
    EXPECT_LE(config.modulation_strength, 1.0f);
}

TEST_F(AttentionSleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(attention_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(AttentionSleepHelperTest, AwakeIsBaseline) {
    /* Awake state should be baseline (1.0) */
    EXPECT_FLOAT_EQ(attention_sleep_capacity_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(attention_sleep_vigilance_for_state(SLEEP_STATE_AWAKE), 1.0f);
}

TEST_F(AttentionSleepHelperTest, CapacityDecreasesDuringSleep) {
    /* Attention capacity should decrease as sleep deepens */
    float cap_awake = attention_sleep_capacity_for_state(SLEEP_STATE_AWAKE);
    float cap_drowsy = attention_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    float cap_light = attention_sleep_capacity_for_state(SLEEP_STATE_LIGHT_NREM);
    float cap_deep = attention_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(cap_awake, cap_drowsy);
    EXPECT_GT(cap_drowsy, cap_light);
    EXPECT_GE(cap_light, cap_deep);
}

TEST_F(AttentionSleepHelperTest, REMInternalAttention) {
    /* REM should have some attention (for dream processing) but less than awake */
    float cap_awake = attention_sleep_capacity_for_state(SLEEP_STATE_AWAKE);
    float cap_rem = attention_sleep_capacity_for_state(SLEEP_STATE_REM);
    float cap_deep = attention_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_LT(cap_rem, cap_awake) << "REM attention less than awake";
    EXPECT_GT(cap_rem, cap_deep) << "REM has more attention than deep NREM";
}

TEST_F(AttentionSleepHelperTest, VigilanceForArousal) {
    /* Vigilance determines arousability from sleep */
    float vig_drowsy = attention_sleep_vigilance_for_state(SLEEP_STATE_DROWSY);
    float vig_rem = attention_sleep_vigilance_for_state(SLEEP_STATE_REM);
    float vig_nrem = attention_sleep_vigilance_for_state(SLEEP_STATE_LIGHT_NREM);

    EXPECT_GT(vig_drowsy, vig_nrem) << "Easier to arouse when drowsy vs NREM";
    EXPECT_GT(vig_rem, vig_nrem) << "Easier to arouse from REM vs NREM";
}

TEST_F(AttentionSleepHelperTest, DrowsyAttentionLapses) {
    /* Drowsy state models attention lapses (microsleeps) */
    float cap = attention_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    EXPECT_GT(cap, 0.5f) << "Drowsy still has some capacity";
    EXPECT_LT(cap, 0.8f) << "Drowsy has reduced capacity (lapses)";
}
