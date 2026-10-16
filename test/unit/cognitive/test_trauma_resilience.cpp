/**
 * @file test_trauma_resilience.cpp
 * @brief Unit tests for the trauma resilience module
 *
 * Tests lifecycle, recall modulation, arousal homeostasis, wellbeing, and integration.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/nimcp_trauma_resilience.h"
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST(TraumaResilienceLifecycle, CreateDestroyDefault)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);
    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceLifecycle, DestroyNullSafe)
{
    /* Must not crash */
    nimcp_trauma_resilience_destroy(NULL);
}

TEST(TraumaResilienceLifecycle, ConfigDefaultsReasonable)
{
    nimcp_trauma_resilience_config_t cfg = nimcp_trauma_resilience_config_default();
    EXPECT_EQ(cfg.max_recall_frequency, 5u);
    EXPECT_EQ(cfg.recall_window_steps, 500u);
    EXPECT_FLOAT_EQ(cfg.dampening_factor, 0.7f);
    EXPECT_FLOAT_EQ(cfg.min_recall_blend, 0.02f);
    EXPECT_FLOAT_EQ(cfg.arousal_ceiling, 0.85f);
    EXPECT_FLOAT_EQ(cfg.arousal_floor, 0.15f);
    EXPECT_EQ(cfg.arousal_patience, 200u);
    EXPECT_FLOAT_EQ(cfg.homeostatic_pull, 0.01f);
    EXPECT_FLOAT_EQ(cfg.habituation_rate, 0.9f);
    EXPECT_EQ(cfg.habituation_memory, 100u);
    EXPECT_FLOAT_EQ(cfg.stress_threshold, 0.6f);
    EXPECT_FLOAT_EQ(cfg.distress_threshold, 0.3f);
    EXPECT_FLOAT_EQ(cfg.crisis_threshold, 0.1f);
}

/* ============================================================================
 * Recall Modulation Tests
 * ============================================================================ */

TEST(TraumaResilienceRecall, FirstRecallReturnsNormalBlend)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    /* First time seeing engram 42 -- not yet tracked, should return normal blend */
    float blend = nimcp_trauma_resilience_modulate_recall(tr, 42, 0.9f, 0.5f);
    EXPECT_FLOAT_EQ(blend, 0.15f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceRecall, RepeatedRecallsDecreaseBlend)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    uint64_t engram_id = 100;

    /* Record several recalls to build up count and habituation decay */
    nimcp_trauma_resilience_record_recall(tr, engram_id, 0.5f);
    nimcp_trauma_resilience_record_recall(tr, engram_id, 0.5f);
    nimcp_trauma_resilience_record_recall(tr, engram_id, 0.5f);

    /* Now modulate -- should be less than normal blend */
    float blend = nimcp_trauma_resilience_modulate_recall(tr, engram_id, 0.9f, 0.5f);
    EXPECT_LT(blend, 0.15f);
    EXPECT_GE(blend, 0.02f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceRecall, MaxFrequencyDropsToMinBlend)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    uint64_t engram_id = 200;

    /* Record max_recall_frequency (5) recalls */
    for (int i = 0; i < 5; i++) {
        nimcp_trauma_resilience_record_recall(tr, engram_id, 0.5f);
    }

    /* At count >= max_recall_frequency, blend should be min_recall_blend */
    float blend = nimcp_trauma_resilience_modulate_recall(tr, engram_id, 0.9f, 0.5f);
    EXPECT_FLOAT_EQ(blend, 0.02f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceRecall, DifferentEngramsDontInterfere)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    /* Hammer engram 1 to suppression */
    for (int i = 0; i < 5; i++) {
        nimcp_trauma_resilience_record_recall(tr, 1, 0.5f);
    }

    /* Engram 2 should still be at normal blend (not yet tracked by modulate) */
    float blend2 = nimcp_trauma_resilience_modulate_recall(tr, 2, 0.9f, 0.5f);
    EXPECT_FLOAT_EQ(blend2, 0.15f);

    /* Engram 1 should be suppressed */
    float blend1 = nimcp_trauma_resilience_modulate_recall(tr, 1, 0.9f, 0.5f);
    EXPECT_FLOAT_EQ(blend1, 0.02f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceRecall, ZeroSimilarityStillModulates)
{
    /* similarity parameter is not used in modulation logic currently,
     * so zero similarity should behave the same as high similarity */
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    float blend = nimcp_trauma_resilience_modulate_recall(tr, 99, 0.0f, 0.5f);
    EXPECT_FLOAT_EQ(blend, 0.15f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceRecall, NullSafetyModulate)
{
    float blend = nimcp_trauma_resilience_modulate_recall(NULL, 1, 0.9f, 0.5f);
    EXPECT_FLOAT_EQ(blend, 0.15f);
}

TEST(TraumaResilienceRecall, NullSafetyRecord)
{
    /* Must not crash */
    nimcp_trauma_resilience_record_recall(NULL, 1, 0.5f);
}

TEST(TraumaResilienceRecall, HabituationReducesBlendGradually)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    uint64_t engram_id = 300;

    /* Record one recall */
    nimcp_trauma_resilience_record_recall(tr, engram_id, 0.5f);
    float blend1 = nimcp_trauma_resilience_modulate_recall(tr, engram_id, 0.9f, 0.5f);

    /* Record a second recall */
    nimcp_trauma_resilience_record_recall(tr, engram_id, 0.5f);
    float blend2 = nimcp_trauma_resilience_modulate_recall(tr, engram_id, 0.9f, 0.5f);

    /* Each additional recall should further reduce blend */
    EXPECT_LT(blend2, blend1);
    EXPECT_GT(blend1, 0.02f);

    nimcp_trauma_resilience_destroy(tr);
}

/* ============================================================================
 * Arousal Homeostasis Tests
 * ============================================================================ */

TEST(TraumaResilienceArousal, NormalArousalPassesThrough)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    /* Normal arousal (0.5) in first call should pass through mostly unchanged
     * since high/low counters haven't exceeded patience */
    float adjusted = nimcp_trauma_resilience_regulate_arousal(tr, 0.5f);
    EXPECT_FLOAT_EQ(adjusted, 0.5f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceArousal, HighArousalPulledDown)
{
    nimcp_trauma_resilience_config_t cfg = nimcp_trauma_resilience_config_default();
    cfg.arousal_patience = 5;   /* Low patience for faster test */
    cfg.arousal_ceiling = 0.70f; /* Lower ceiling so EMA crosses sooner */
    cfg.homeostatic_pull = 0.05f;
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(&cfg);
    ASSERT_NE(tr, nullptr);

    /* Feed high arousal for many steps so EMA exceeds ceiling and patience is exceeded */
    float adjusted = 0.95f;
    for (int i = 0; i < 100; i++) {
        adjusted = nimcp_trauma_resilience_regulate_arousal(tr, 0.95f);
    }

    /* After exceeding patience, arousal should be pulled down */
    EXPECT_LT(adjusted, 0.95f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceArousal, LowArousalPulledUp)
{
    nimcp_trauma_resilience_config_t cfg = nimcp_trauma_resilience_config_default();
    cfg.arousal_patience = 5;
    cfg.arousal_floor = 0.30f;  /* Higher floor so EMA crosses sooner */
    cfg.homeostatic_pull = 0.05f;
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(&cfg);
    ASSERT_NE(tr, nullptr);

    float adjusted = 0.05f;
    for (int i = 0; i < 100; i++) {
        adjusted = nimcp_trauma_resilience_regulate_arousal(tr, 0.05f);
    }

    /* After exceeding patience, arousal should be pulled up */
    EXPECT_GT(adjusted, 0.05f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceArousal, ArousalClampedToUnitInterval)
{
    nimcp_trauma_resilience_config_t cfg = nimcp_trauma_resilience_config_default();
    cfg.arousal_patience = 1;
    cfg.homeostatic_pull = 2.0f;  /* Very strong pull */
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(&cfg);
    ASSERT_NE(tr, nullptr);

    /* Even with extreme pull, result should be in [0, 1] */
    for (int i = 0; i < 10; i++) {
        float adjusted = nimcp_trauma_resilience_regulate_arousal(tr, 0.99f);
        EXPECT_GE(adjusted, 0.0f);
        EXPECT_LE(adjusted, 1.0f);
    }

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceArousal, NullSafety)
{
    float adjusted = nimcp_trauma_resilience_regulate_arousal(NULL, 0.7f);
    EXPECT_FLOAT_EQ(adjusted, 0.7f);
}

/* ============================================================================
 * Wellbeing Tests
 * ============================================================================ */

TEST(TraumaResilienceWellbeing, InitialStateIsHealthy)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    nimcp_mental_state_t state = nimcp_trauma_resilience_get_state(tr);
    EXPECT_EQ(state, NIMCP_MENTAL_HEALTHY);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceWellbeing, GetWellbeingReturnsValidMetrics)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    nimcp_wellbeing_t wb;
    int ret = nimcp_trauma_resilience_get_wellbeing(tr, &wb);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wb.state, NIMCP_MENTAL_HEALTHY);
    EXPECT_EQ(wb.crisis_count, 0u);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceWellbeing, WellbeingScoreInUnitInterval)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    nimcp_wellbeing_t wb;
    nimcp_trauma_resilience_get_wellbeing(tr, &wb);
    EXPECT_GE(wb.wellbeing_score, 0.0f);
    EXPECT_LE(wb.wellbeing_score, 1.0f);

    /* Stress the system and check again */
    for (int i = 0; i < 20; i++) {
        nimcp_trauma_resilience_record_recall(tr, 1, 0.9f);
        nimcp_trauma_resilience_modulate_recall(tr, 1, 0.9f, 0.9f);
    }

    nimcp_trauma_resilience_get_wellbeing(tr, &wb);
    EXPECT_GE(wb.wellbeing_score, 0.0f);
    EXPECT_LE(wb.wellbeing_score, 1.0f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceWellbeing, StateNameReturnsNonNull)
{
    EXPECT_NE(nimcp_mental_state_name(NIMCP_MENTAL_HEALTHY), nullptr);
    EXPECT_NE(nimcp_mental_state_name(NIMCP_MENTAL_STRESSED), nullptr);
    EXPECT_NE(nimcp_mental_state_name(NIMCP_MENTAL_DISTRESSED), nullptr);
    EXPECT_NE(nimcp_mental_state_name(NIMCP_MENTAL_CRISIS), nullptr);
    EXPECT_NE(nimcp_mental_state_name((nimcp_mental_state_t)99), nullptr);
}

TEST(TraumaResilienceWellbeing, ResetReturnsToHealthy)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    /* Build up recall history to degrade state */
    for (int i = 0; i < 20; i++) {
        nimcp_trauma_resilience_record_recall(tr, 1, 0.9f);
        /* Trigger wellbeing update via modulate (updates every 10 steps) */
        nimcp_trauma_resilience_modulate_recall(tr, 1, 0.9f, 0.9f);
    }

    /* Reset */
    int ret = nimcp_trauma_resilience_reset(tr);
    EXPECT_EQ(ret, 0);

    nimcp_mental_state_t state = nimcp_trauma_resilience_get_state(tr);
    EXPECT_EQ(state, NIMCP_MENTAL_HEALTHY);

    nimcp_trauma_resilience_destroy(tr);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST(TraumaResilienceIntegration, RecordRecallUpdatesTracking)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    /* Record recall, then modulate -- should see dampening */
    nimcp_trauma_resilience_record_recall(tr, 50, 0.5f);
    float blend = nimcp_trauma_resilience_modulate_recall(tr, 50, 0.9f, 0.5f);

    /* After 1 record, habituation should reduce blend below normal */
    EXPECT_LT(blend, 0.15f);
    EXPECT_GE(blend, 0.02f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceIntegration, MultipleRecallsSameIdHabituation)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    uint64_t engram_id = 500;
    float prev_blend = 0.15f;

    /* Each additional recall should further reduce blend via habituation + dampening */
    for (int i = 0; i < 4; i++) {
        nimcp_trauma_resilience_record_recall(tr, engram_id, 0.5f);
        float blend = nimcp_trauma_resilience_modulate_recall(tr, engram_id, 0.9f, 0.5f);
        EXPECT_LE(blend, prev_blend);
        prev_blend = blend;
    }

    /* Final blend should be much less than normal */
    EXPECT_LT(prev_blend, 0.1f);

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceIntegration, CrisisStateSeverelyDampensRecall)
{
    nimcp_trauma_resilience_config_t cfg = nimcp_trauma_resilience_config_default();
    cfg.crisis_threshold = 0.5f;   /* Easier to trigger crisis */
    cfg.stress_threshold = 0.9f;
    cfg.distress_threshold = 0.7f;
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(&cfg);
    ASSERT_NE(tr, nullptr);

    /* Hammer the same engram many times to push wellbeing down */
    for (int i = 0; i < 30; i++) {
        nimcp_trauma_resilience_record_recall(tr, 1, 0.9f);
        /* modulate_recall triggers wellbeing update every 10 steps */
        nimcp_trauma_resilience_modulate_recall(tr, 1, 0.95f, 0.9f);
    }

    /* Check that state has deteriorated */
    nimcp_mental_state_t state = nimcp_trauma_resilience_get_state(tr);
    EXPECT_NE(state, NIMCP_MENTAL_HEALTHY);

    /* A new engram should still get reduced blend if in crisis/distressed */
    if (state == NIMCP_MENTAL_CRISIS) {
        /* Record engram 2 once so it's tracked */
        nimcp_trauma_resilience_record_recall(tr, 2, 0.3f);
        float blend = nimcp_trauma_resilience_modulate_recall(tr, 2, 0.9f, 0.5f);
        /* Crisis applies 0.1x multiplier, so blend should be very low */
        EXPECT_LT(blend, 0.05f);
    }

    nimcp_trauma_resilience_destroy(tr);
}

TEST(TraumaResilienceIntegration, MentalStateNameStringsCorrect)
{
    EXPECT_STREQ(nimcp_mental_state_name(NIMCP_MENTAL_HEALTHY), "HEALTHY");
    EXPECT_STREQ(nimcp_mental_state_name(NIMCP_MENTAL_STRESSED), "STRESSED");
    EXPECT_STREQ(nimcp_mental_state_name(NIMCP_MENTAL_DISTRESSED), "DISTRESSED");
    EXPECT_STREQ(nimcp_mental_state_name(NIMCP_MENTAL_CRISIS), "CRISIS");
    EXPECT_STREQ(nimcp_mental_state_name((nimcp_mental_state_t)99), "UNKNOWN");
}

/* ============================================================================
 * Null Safety for Wellbeing
 * ============================================================================ */

TEST(TraumaResilienceWellbeing, GetWellbeingNullSafety)
{
    nimcp_trauma_resilience_t* tr = nimcp_trauma_resilience_create(NULL);
    ASSERT_NE(tr, nullptr);

    /* NULL tr */
    nimcp_wellbeing_t wb;
    EXPECT_EQ(nimcp_trauma_resilience_get_wellbeing(NULL, &wb), -1);

    /* NULL wellbeing */
    EXPECT_EQ(nimcp_trauma_resilience_get_wellbeing(tr, NULL), -1);

    /* Both NULL */
    EXPECT_EQ(nimcp_trauma_resilience_get_wellbeing(NULL, NULL), -1);

    /* NULL tr for get_state */
    EXPECT_EQ(nimcp_trauma_resilience_get_state(NULL), NIMCP_MENTAL_HEALTHY);

    /* NULL tr for reset */
    EXPECT_EQ(nimcp_trauma_resilience_reset(NULL), -1);

    nimcp_trauma_resilience_destroy(tr);
}
