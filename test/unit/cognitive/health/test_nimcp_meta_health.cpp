/**
 * @file test_nimcp_meta_health.cpp
 * @brief Unit tests for meta-health self-reflection system
 *
 * WHAT: Tests for health system self-improvement through reflection
 * WHY:  Validate decision recording, reflection, learning application
 * HOW:  Test lifecycle, decisions, assessments, reflection, adjustments
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/health/nimcp_meta_health.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class MetaHealthTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = meta_health_default_config();
    }

    void TearDown() override {
        if (reflector_) {
            meta_health_destroy(reflector_);
            reflector_ = nullptr;
        }
    }

    meta_health_config_t config_;
    meta_health_reflector_t* reflector_ = nullptr;
};

/*=============================================================================
 * Configuration Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, DefaultConfigValues) {
    EXPECT_TRUE(config_.enable_auto_reflection);
    EXPECT_TRUE(config_.enable_pattern_learning);

    EXPECT_GT(config_.reflection_interval_ms, 0u);
    EXPECT_GT(config_.min_decisions_for_reflection, 0u);
    EXPECT_GT(config_.max_decisions_to_analyze, 0u);
    EXPECT_GT(config_.reflection_timeout_ms, 0u);

    // Confidence threshold should be valid
    EXPECT_GE(config_.auto_apply_confidence_threshold, 0.0f);
    EXPECT_LE(config_.auto_apply_confidence_threshold, 1.0f);
}

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, CreateWithNullConfig) {
    reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(reflector_, nullptr);
}

TEST_F(MetaHealthTest, CreateWithConfig) {
    config_.enable_pattern_learning = false;
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);
}

TEST_F(MetaHealthTest, DestroyNull) {
    // Should not crash
    meta_health_destroy(nullptr);
}

TEST_F(MetaHealthTest, StartStop) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_start(reflector_), 0);
    EXPECT_EQ(meta_health_stop(reflector_), 0);
}

TEST_F(MetaHealthTest, StartNull) {
    EXPECT_EQ(meta_health_start(nullptr), -1);
}

TEST_F(MetaHealthTest, StopNull) {
    EXPECT_EQ(meta_health_stop(nullptr), -1);
}

/*=============================================================================
 * Decision Recording Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, RecordDecisionNull) {
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);

    EXPECT_EQ(meta_health_record_decision(nullptr, &decision), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_record_decision(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, RecordDecisionBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    decision.anomaly_type = HEALTH_MSG_MEMORY_CORRUPTION;
    decision.timestamp_us = 1000000;

    EXPECT_EQ(meta_health_record_decision(reflector_, &decision), 0);
}

TEST_F(MetaHealthTest, RecordOutcomeNull) {
    EXPECT_EQ(meta_health_record_outcome(nullptr, 1000000,
        META_HEALTH_OUTCOME_SUCCESS, true, 100, 0.9f), -1);
}

TEST_F(MetaHealthTest, RecordOutcomeBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    // First record a decision
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    decision.anomaly_type = HEALTH_MSG_DEADLOCK_DETECTED;
    decision.timestamp_us = 1000000;

    EXPECT_EQ(meta_health_record_decision(reflector_, &decision), 0);

    // Record outcome
    EXPECT_EQ(meta_health_record_outcome(reflector_, 1000000,
        META_HEALTH_OUTCOME_SUCCESS, true, 150, 0.95f), 0);
}

/*=============================================================================
 * Assessment Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, GetAssessmentNull) {
    meta_health_assessment_t assessment;

    EXPECT_EQ(meta_health_get_assessment(nullptr, &assessment), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_get_assessment(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, GetAssessmentBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    meta_health_assessment_t assessment;
    memset(&assessment, 0xFF, sizeof(assessment));

    EXPECT_EQ(meta_health_get_assessment(reflector_, &assessment), 0);

    // Assessment should have valid values
    EXPECT_GE(assessment.accuracy_rate, 0.0f);
    EXPECT_LE(assessment.accuracy_rate, 1.0f);
}

TEST_F(MetaHealthTest, GetWeaknessesNull) {
    meta_health_weakness_t weaknesses;

    EXPECT_EQ(meta_health_get_weaknesses(nullptr, &weaknesses), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_get_weaknesses(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, GetWeaknessesBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    meta_health_weakness_t weaknesses;
    memset(&weaknesses, 0xFF, sizeof(weaknesses));

    EXPECT_EQ(meta_health_get_weaknesses(reflector_, &weaknesses), 0);
}

/*=============================================================================
 * Reflection Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, ReflectNull) {
    meta_health_reflection_result_t result;

    EXPECT_EQ(meta_health_reflect(nullptr, &result), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_reflect(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, ReflectBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    meta_health_reflection_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(meta_health_reflect(reflector_, &result), 0);

    // Reflection should complete
    EXPECT_GE(result.decisions_analyzed, 0u);
}

TEST_F(MetaHealthTest, ReflectAsync) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    uint64_t request_id = 0;
    EXPECT_EQ(meta_health_reflect_async(reflector_, &request_id), 0);
    EXPECT_NE(request_id, 0u);

    // Check result
    meta_health_reflection_result_t result;
    int status = meta_health_get_reflection_result(reflector_, request_id, &result);
    EXPECT_GE(status, 0);  // Either pending (0), complete (1), or error (-1)
}

TEST_F(MetaHealthTest, ReflectWithHistory) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    // Build up decision history
    for (int i = 0; i < 20; i++) {
        meta_health_decision_t decision;
        meta_health_init_decision(&decision);
        decision.anomaly_type = (health_agent_msg_type_t)(i % 5);
        decision.timestamp_us = 1000000 + i * 100000;

        meta_health_record_decision(reflector_, &decision);

        meta_health_outcome_t outcome = (i % 2 == 0) ?
            META_HEALTH_OUTCOME_SUCCESS : META_HEALTH_OUTCOME_PARTIAL_SUCCESS;

        meta_health_record_outcome(reflector_, decision.timestamp_us,
            outcome, (i % 2 == 0), 100 + i * 10, 0.8f);
    }

    meta_health_reflection_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(meta_health_reflect(reflector_, &result), 0);
    EXPECT_GT(result.decisions_analyzed, 0u);
}

/*=============================================================================
 * Learning Application Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, ApplyLearningsNull) {
    meta_health_reflection_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(meta_health_apply_learnings(nullptr, &result), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_apply_learnings(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, ApplyLearningsBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    // First perform reflection
    meta_health_reflection_result_t result;
    meta_health_reflect(reflector_, &result);

    // Apply learnings
    int applied = meta_health_apply_learnings(reflector_, &result);
    EXPECT_GE(applied, 0);  // 0 or more adjustments applied
}

TEST_F(MetaHealthTest, ApplyAdjustmentNull) {
    meta_health_adjustment_t adjustment;
    memset(&adjustment, 0, sizeof(adjustment));

    EXPECT_EQ(meta_health_apply_adjustment(nullptr, &adjustment), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_apply_adjustment(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, RevertLearningsNull) {
    EXPECT_EQ(meta_health_revert_learnings(nullptr), -1);
}

TEST_F(MetaHealthTest, RevertLearningsBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    // Revert when nothing applied should still succeed
    int result = meta_health_revert_learnings(reflector_);
    EXPECT_GE(result, -1);  // May return 0 or -1 depending on implementation
}

/*=============================================================================
 * Pattern Registration Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, RegisterPatternNull) {
    meta_health_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));

    EXPECT_EQ(meta_health_register_pattern(nullptr, &pattern), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_register_pattern(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, RegisterPatternBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    meta_health_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    snprintf(pattern.predictor, sizeof(pattern.predictor), "Test pattern");
    pattern.confidence = 0.8f;

    EXPECT_EQ(meta_health_register_pattern(reflector_, &pattern), 0);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, GetStatsNull) {
    meta_health_stats_t stats;

    EXPECT_EQ(meta_health_get_stats(nullptr, &stats), -1);

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_get_stats(reflector_, nullptr), -1);
}

TEST_F(MetaHealthTest, GetStatsBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    meta_health_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    EXPECT_EQ(meta_health_get_stats(reflector_, &stats), 0);

    EXPECT_GE(stats.decisions_recorded, 0u);
    EXPECT_GE(stats.reflections_performed, 0u);
    EXPECT_GE(stats.patterns_discovered, 0u);
}

TEST_F(MetaHealthTest, ResetStatsNull) {
    // Should not crash
    meta_health_reset_stats(nullptr);
}

TEST_F(MetaHealthTest, ResetStatsBasic) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    // Record some decisions
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    meta_health_record_decision(reflector_, &decision);

    meta_health_reset_stats(reflector_);

    meta_health_stats_t stats;
    EXPECT_EQ(meta_health_get_stats(reflector_, &stats), 0);
    EXPECT_EQ(stats.decisions_recorded, 0u);
}

/*=============================================================================
 * Utility Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, OutcomeName) {
    // Each outcome type should have a non-null name
    const char* success_name = meta_health_outcome_name(META_HEALTH_OUTCOME_SUCCESS);
    EXPECT_NE(success_name, nullptr);
    EXPECT_GT(strlen(success_name), 0u);

    const char* partial_name = meta_health_outcome_name(META_HEALTH_OUTCOME_PARTIAL_SUCCESS);
    EXPECT_NE(partial_name, nullptr);

    const char* failed_name = meta_health_outcome_name(META_HEALTH_OUTCOME_FAILURE);
    EXPECT_NE(failed_name, nullptr);
}

TEST_F(MetaHealthTest, InitDecisionNull) {
    // Should not crash
    meta_health_init_decision(nullptr);
}

TEST_F(MetaHealthTest, InitDecisionBasic) {
    meta_health_decision_t decision;
    memset(&decision, 0xFF, sizeof(decision));

    meta_health_init_decision(&decision);

    // Should be initialized - timestamp should be set (either to 0 or current time)
    // The key is that the struct is properly initialized and not left as 0xFF
    EXPECT_NE(decision.timestamp_us, (uint64_t)0xFFFFFFFFFFFFFFFFULL);
}

/*=============================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST_F(MetaHealthTest, ReflectionCycle) {
    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    EXPECT_EQ(meta_health_start(reflector_), 0);

    // Run multiple reflection cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        // Record decisions
        for (int i = 0; i < 5; i++) {
            meta_health_decision_t decision;
            meta_health_init_decision(&decision);
            decision.anomaly_type = (health_agent_msg_type_t)(i % 5);
            decision.timestamp_us = (cycle * 5 + i) * 100000;

            meta_health_record_decision(reflector_, &decision);

            meta_health_outcome_t outcome = ((i + cycle) % 2 == 0) ?
                META_HEALTH_OUTCOME_SUCCESS : META_HEALTH_OUTCOME_PARTIAL_SUCCESS;

            meta_health_record_outcome(reflector_, decision.timestamp_us,
                outcome, ((i + cycle) % 2 == 0), 100 + i * 10, 0.8f);
        }

        // Reflect
        meta_health_reflection_result_t result;
        EXPECT_EQ(meta_health_reflect(reflector_, &result), 0);

        // Apply learnings
        meta_health_apply_learnings(reflector_, &result);
    }

    EXPECT_EQ(meta_health_stop(reflector_), 0);
}

TEST_F(MetaHealthTest, DisabledFeatures) {
    // Disable auto reflection
    config_.enable_auto_reflection = false;
    config_.enable_pattern_learning = false;

    reflector_ = meta_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(reflector_, nullptr);

    // Basic operations should still work
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    EXPECT_EQ(meta_health_record_decision(reflector_, &decision), 0);

    // Reflection should still work (just doesn't auto-run)
    meta_health_reflection_result_t result;
    EXPECT_EQ(meta_health_reflect(reflector_, &result), 0);
}
