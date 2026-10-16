/**
 * @file test_working_memory_sleep_bridge.cpp
 * @brief Unit tests for Working Memory-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "cognitive/working_memory/nimcp_working_memory_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

class WorkingMemorySleepHelperTest : public ::testing::Test {};

/* Capacity Factor Tests */
TEST_F(WorkingMemorySleepHelperTest, CapacityFactorAwake) {
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(SLEEP_STATE_AWAKE),
                    WM_SLEEP_CAPACITY_AWAKE);
}

TEST_F(WorkingMemorySleepHelperTest, CapacityFactorDrowsy) {
    float cap = working_memory_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(cap, WM_SLEEP_CAPACITY_DROWSY);
    EXPECT_LT(cap, 1.0f) << "WM capacity reduced when drowsy (~5 items vs 7)";
}

TEST_F(WorkingMemorySleepHelperTest, CapacityFactorLightNREM) {
    float cap = working_memory_sleep_capacity_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(cap, WM_SLEEP_CAPACITY_LIGHT_NREM);
    EXPECT_LT(cap, 0.5f) << "Minimal WM in light NREM";
}

TEST_F(WorkingMemorySleepHelperTest, CapacityFactorDeepNREM) {
    float cap = working_memory_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(cap, WM_SLEEP_CAPACITY_DEEP_NREM);
    EXPECT_FLOAT_EQ(cap, 0.0f) << "WM offline in deep NREM";
}

TEST_F(WorkingMemorySleepHelperTest, CapacityFactorREM) {
    float cap = working_memory_sleep_capacity_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(cap, WM_SLEEP_CAPACITY_REM);
    EXPECT_GT(cap, 0.0f) << "Some WM in REM (dream narrative)";
}

/* Decay Rate Factor Tests */
TEST_F(WorkingMemorySleepHelperTest, DecayRateAwake) {
    EXPECT_FLOAT_EQ(working_memory_sleep_decay_for_state(SLEEP_STATE_AWAKE),
                    WM_SLEEP_DECAY_AWAKE);
}

TEST_F(WorkingMemorySleepHelperTest, DecayRateDrowsy) {
    float decay = working_memory_sleep_decay_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(decay, WM_SLEEP_DECAY_DROWSY);
    EXPECT_GT(decay, 1.0f) << "Faster decay when drowsy";
}

TEST_F(WorkingMemorySleepHelperTest, DecayRateLightNREM) {
    float decay = working_memory_sleep_decay_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(decay, WM_SLEEP_DECAY_LIGHT_NREM);
    EXPECT_GT(decay, 1.5f) << "Much faster decay in light NREM";
}

TEST_F(WorkingMemorySleepHelperTest, DecayRateDeepNREM) {
    /* Deep NREM: WM is offline, no decay (it's empty) */
    float decay = working_memory_sleep_decay_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(decay, WM_SLEEP_DECAY_DEEP_NREM);
    EXPECT_FLOAT_EQ(decay, 0.0f) << "No decay when WM offline";
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(WorkingMemorySleepHelperTest, DefaultConfigValid) {
    working_memory_sleep_config_t config;
    ASSERT_EQ(working_memory_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_capacity_modulation);
    EXPECT_TRUE(config.enable_decay_modulation);
    EXPECT_TRUE(config.enable_transfer_on_sleep);
    EXPECT_GT(config.modulation_strength, 0.0f);
}

TEST_F(WorkingMemorySleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(working_memory_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(WorkingMemorySleepHelperTest, AwakeIsBaseline) {
    /* Awake state: full 7+/-2 capacity, normal decay */
    EXPECT_FLOAT_EQ(working_memory_sleep_capacity_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(working_memory_sleep_decay_for_state(SLEEP_STATE_AWAKE), 1.0f);
}

TEST_F(WorkingMemorySleepHelperTest, CapacityDecreasesDuringSleep) {
    /* WM capacity should decrease as sleep deepens */
    float cap_awake = working_memory_sleep_capacity_for_state(SLEEP_STATE_AWAKE);
    float cap_drowsy = working_memory_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    float cap_light = working_memory_sleep_capacity_for_state(SLEEP_STATE_LIGHT_NREM);
    float cap_deep = working_memory_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(cap_awake, cap_drowsy);
    EXPECT_GT(cap_drowsy, cap_light);
    EXPECT_GT(cap_light, cap_deep);
}

TEST_F(WorkingMemorySleepHelperTest, DecayIncreasesWithDrowsiness) {
    /* Decay rate should increase as we get drowsy (harder to maintain) */
    float decay_awake = working_memory_sleep_decay_for_state(SLEEP_STATE_AWAKE);
    float decay_drowsy = working_memory_sleep_decay_for_state(SLEEP_STATE_DROWSY);
    float decay_light = working_memory_sleep_decay_for_state(SLEEP_STATE_LIGHT_NREM);

    EXPECT_LT(decay_awake, decay_drowsy);
    EXPECT_LT(decay_drowsy, decay_light);
}

TEST_F(WorkingMemorySleepHelperTest, DreamWM) {
    /* REM has some WM for dream narrative processing */
    float cap = working_memory_sleep_capacity_for_state(SLEEP_STATE_REM);
    float cap_deep = working_memory_sleep_capacity_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(cap, cap_deep) << "REM has more WM than deep NREM";
    EXPECT_GT(cap, 0.3f) << "REM needs some WM for dream narrative";
}

TEST_F(WorkingMemorySleepHelperTest, MillerNumber) {
    /* Capacity 1.0 represents the magic 7+/-2 items */
    float cap_awake = working_memory_sleep_capacity_for_state(SLEEP_STATE_AWAKE);
    float cap_drowsy = working_memory_sleep_capacity_for_state(SLEEP_STATE_DROWSY);

    /* Drowsy ~= 5 items, so about 70% of awake capacity */
    EXPECT_NEAR(cap_drowsy, 0.7f, 0.1f) << "Drowsy reduces to ~5 items";
}
