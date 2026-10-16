//=============================================================================
// test_tier_optimization.cpp - Unit Tests for Tier Optimization (Opt 1, 8)
//=============================================================================
// WHAT: Tests tier-based constants, sizing macros, and scaling functions
// WHY:  Verify bio inbox sizing, JEPA scaling, and tier helper functions
// HOW:  Check compile-time constants and function return values

#include <gtest/gtest.h>

extern "C" {
#include "utils/platform/nimcp_tier_optimization.h"
}

//=============================================================================
// Opt 1: Bio-Async Tier Constants
//=============================================================================

class BioInboxTierTest : public ::testing::Test {};

TEST_F(BioInboxTierTest, InboxCapacity_IsPositive) {
    EXPECT_GT(NIMCP_BIO_INBOX_CAPACITY, 0u);
}

TEST_F(BioInboxTierTest, InboxCapacity_WithinReasonableRange) {
    // Should be between 4 (MINIMAL) and 32 (FULL)
    EXPECT_GE(NIMCP_BIO_INBOX_CAPACITY, 4u);
    EXPECT_LE(NIMCP_BIO_INBOX_CAPACITY, 32u);
}

TEST_F(BioInboxTierTest, MessagePoolSize_IsPositive) {
    EXPECT_GT(NIMCP_BIO_MESSAGE_POOL_SIZE, 0u);
}

TEST_F(BioInboxTierTest, MessagePoolSize_WithinReasonableRange) {
    // Should be between 64 (MINIMAL) and 1024 (FULL)
    EXPECT_GE(NIMCP_BIO_MESSAGE_POOL_SIZE, 64u);
    EXPECT_LE(NIMCP_BIO_MESSAGE_POOL_SIZE, 1024u);
}

//=============================================================================
// Opt 2: Feature Enable/Disable Macros
//=============================================================================

class FeatureEnableTierTest : public ::testing::Test {};

TEST_F(FeatureEnableTierTest, StatisticsEnabled_IsBoolLike) {
    // NIMCP_ENABLE_STATISTICS should be 0 or 1
    EXPECT_GE(NIMCP_ENABLE_STATISTICS, 0);
    EXPECT_LE(NIMCP_ENABLE_STATISTICS, 1);
}

TEST_F(FeatureEnableTierTest, DetailedHistoryEnabled_IsBoolLike) {
    EXPECT_GE(NIMCP_ENABLE_DETAILED_HISTORY, 0);
    EXPECT_LE(NIMCP_ENABLE_DETAILED_HISTORY, 1);
}

//=============================================================================
// Opt 8: JEPA Tier-Optimized Constants
//=============================================================================

class JepaTierScalingTest : public ::testing::Test {};

TEST_F(JepaTierScalingTest, LatentDim_WithinRange) {
    // Should be 64 (MINIMAL) to 512 (FULL)
    EXPECT_GE(NIMCP_JEPA_LATENT_DIM, 64u);
    EXPECT_LE(NIMCP_JEPA_LATENT_DIM, 512u);
}

TEST_F(JepaTierScalingTest, NumPatches_WithinRange) {
    // Should be 4 (MINIMAL) to 49 (FULL)
    EXPECT_GE(NIMCP_JEPA_NUM_PATCHES, 4u);
    EXPECT_LE(NIMCP_JEPA_NUM_PATCHES, 49u);
}

TEST_F(JepaTierScalingTest, PredictorHidden_WithinRange) {
    // Should be 32 (MINIMAL) to 256 (FULL)
    EXPECT_GE(NIMCP_JEPA_PREDICTOR_HIDDEN, 32u);
    EXPECT_LE(NIMCP_JEPA_PREDICTOR_HIDDEN, 256u);
}

TEST_F(JepaTierScalingTest, SpeechSequenceLen_WithinRange) {
    EXPECT_GE(NIMCP_JEPA_SPEECH_SEQUENCE_LEN, 16u);
    EXPECT_LE(NIMCP_JEPA_SPEECH_SEQUENCE_LEN, 64u);
}

TEST_F(JepaTierScalingTest, ContextDim_WithinRange) {
    EXPECT_GE(NIMCP_JEPA_CONTEXT_DIM, 32u);
    EXPECT_LE(NIMCP_JEPA_CONTEXT_DIM, 128u);
}

TEST_F(JepaTierScalingTest, JointDim_WithinRange) {
    EXPECT_GE(NIMCP_JEPA_JOINT_DIM, 64u);
    EXPECT_LE(NIMCP_JEPA_JOINT_DIM, 512u);
}

//=============================================================================
// Tier Helper Functions
//=============================================================================

class TierHelperTest : public ::testing::Test {};

TEST_F(TierHelperTest, MemoryBudget_Positive) {
    size_t budget = nimcp_tier_memory_budget_bytes();
    EXPECT_GT(budget, 0u);
    // Between 64MB and 8GB
    EXPECT_GE(budget, 64u * 1024 * 1024);
    EXPECT_LE(budget, 8UL * 1024 * 1024 * 1024);
}

TEST_F(TierHelperTest, ThreadCount_Positive) {
    uint32_t threads = nimcp_tier_thread_count();
    EXPECT_GT(threads, 0u);
    EXPECT_LE(threads, 8u);
}

TEST_F(TierHelperTest, ScaleSize_ReturnsPositive) {
    size_t scaled = nimcp_tier_scale_size(1024);
    EXPECT_GT(scaled, 0u);
    EXPECT_LE(scaled, 1024u);
}

TEST_F(TierHelperTest, ScaleSize_ZeroInput) {
    size_t scaled = nimcp_tier_scale_size(0);
    EXPECT_EQ(scaled, 0u);
}

TEST_F(TierHelperTest, ScaleCount_ReturnsPositive) {
    uint32_t scaled = nimcp_tier_scale_count(256);
    EXPECT_GT(scaled, 0u);
    EXPECT_LE(scaled, 256u);
}

TEST_F(TierHelperTest, ScaleCount_MinimumOne) {
    // Even with small input, should return at least 1
    uint32_t scaled = nimcp_tier_scale_count(8);
    EXPECT_GE(scaled, 1u);
}

//=============================================================================
// Sizing Macros - History and Buffer Sizes
//=============================================================================

class SizingMacroTest : public ::testing::Test {};

TEST_F(SizingMacroTest, HistorySizes_Ordered) {
    EXPECT_LE(NIMCP_HISTORY_SIZE_SMALL, NIMCP_HISTORY_SIZE_MEDIUM);
    EXPECT_LE(NIMCP_HISTORY_SIZE_MEDIUM, NIMCP_HISTORY_SIZE_LARGE);
}

TEST_F(SizingMacroTest, BufferSizes_Positive) {
    EXPECT_GT(NIMCP_NAME_BUFFER_SIZE, 0u);
    EXPECT_GT(NIMCP_DESC_BUFFER_SIZE, 0u);
    EXPECT_GT(NIMCP_ERROR_BUFFER_SIZE, 0u);
    EXPECT_GT(NIMCP_PATH_BUFFER_SIZE, 0u);
}

TEST_F(SizingMacroTest, MaxLayers_Positive) {
    EXPECT_GT(NIMCP_MAX_LAYERS, 0u);
    EXPECT_GT(NIMCP_MAX_NEURONS_PER_LAYER, 0u);
}
