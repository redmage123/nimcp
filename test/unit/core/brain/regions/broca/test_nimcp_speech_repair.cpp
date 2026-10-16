/**
 * @file test_nimcp_speech_repair.cpp
 * @brief Unit tests for nimcp_speech_repair.c
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/regions/broca/nimcp_speech_repair.h"

class SpeechRepairTest : public ::testing::Test {
protected:
    speech_repair_t* processor;
    speech_repair_config_t config;

    void SetUp() override {
        config = speech_repair_default_config();
        processor = speech_repair_create(&config);
        ASSERT_NE(nullptr, processor);
    }

    void TearDown() override {
        speech_repair_destroy(processor);
    }
};

// Lifecycle Tests
TEST_F(SpeechRepairTest, DefaultConfigReasonable) {
    auto cfg = speech_repair_default_config();
    EXPECT_GT(cfg.max_repairs, 0u);
    EXPECT_GT(cfg.history_size, 0u);
    EXPECT_TRUE(cfg.enable_auto_correction);
}

TEST_F(SpeechRepairTest, CreateWithNullConfig) {
    auto* p = speech_repair_create(NULL);
    ASSERT_NE(nullptr, p);
    speech_repair_destroy(p);
}

TEST_F(SpeechRepairTest, DestroyNull) {
    speech_repair_destroy(NULL);
}

TEST_F(SpeechRepairTest, Reset) {
    EXPECT_TRUE(speech_repair_reset(processor));
    EXPECT_EQ(speech_repair_get_status(processor), REPAIR_STATUS_IDLE);
}

// Disfluency Detection Tests
TEST_F(SpeechRepairTest, DetectFilledPause_Um) {
    disfluency_t disfluencies[8];
    uint32_t count = speech_repair_detect_disfluencies(processor,
        "I um want to go", disfluencies, 8);

    EXPECT_GT(count, 0u);
    bool found_um = false;
    for (uint32_t i = 0; i < count; i++) {
        if (disfluencies[i].type == DISFLUENCY_FILLED_PAUSE &&
            strcmp(disfluencies[i].content, "um") == 0) {
            found_um = true;
            break;
        }
    }
    EXPECT_TRUE(found_um);
}

TEST_F(SpeechRepairTest, DetectFilledPause_Uh) {
    disfluency_t disfluencies[8];
    uint32_t count = speech_repair_detect_disfluencies(processor,
        "I uh need help", disfluencies, 8);

    EXPECT_GT(count, 0u);
}

TEST_F(SpeechRepairTest, DetectFilledPause_Like) {
    disfluency_t disfluencies[8];
    uint32_t count = speech_repair_detect_disfluencies(processor,
        "It was like really good", disfluencies, 8);

    EXPECT_GT(count, 0u);
}

TEST_F(SpeechRepairTest, NoFalsePositives) {
    disfluency_t disfluencies[8];
    uint32_t count = speech_repair_detect_disfluencies(processor,
        "The sky is blue today", disfluencies, 8);

    EXPECT_EQ(count, 0u);
}

TEST_F(SpeechRepairTest, IsFillerTrue) {
    EXPECT_TRUE(speech_repair_is_filler("um"));
    EXPECT_TRUE(speech_repair_is_filler("uh"));
    EXPECT_TRUE(speech_repair_is_filler("er"));
    EXPECT_TRUE(speech_repair_is_filler("like"));
}

TEST_F(SpeechRepairTest, IsFillerFalse) {
    EXPECT_FALSE(speech_repair_is_filler("hello"));
    EXPECT_FALSE(speech_repair_is_filler("the"));
    EXPECT_FALSE(speech_repair_is_filler("is"));
}

TEST_F(SpeechRepairTest, DisfluencyNames) {
    EXPECT_STREQ(speech_repair_disfluency_name(DISFLUENCY_NONE), "NONE");
    EXPECT_STREQ(speech_repair_disfluency_name(DISFLUENCY_FILLED_PAUSE), "FILLED_PAUSE");
    EXPECT_STREQ(speech_repair_disfluency_name(DISFLUENCY_WORD_FRAGMENT), "WORD_FRAGMENT");
    EXPECT_STREQ(speech_repair_disfluency_name(static_cast<disfluency_type_t>(999)), "INVALID");
}

// Repair Detection Tests
TEST_F(SpeechRepairTest, DetectCorrection_IMean) {
    repair_instance_t repairs[4];
    uint32_t count = speech_repair_detect_repairs(processor,
        "Turn left, I mean, right", repairs, 4);

    EXPECT_GT(count, 0u);
}

TEST_F(SpeechRepairTest, DetectCorrection_Sorry) {
    repair_instance_t repairs[4];
    uint32_t count = speech_repair_detect_repairs(processor,
        "It costs ten, sorry, twenty dollars", repairs, 4);

    EXPECT_GT(count, 0u);
}

TEST_F(SpeechRepairTest, DetectRepetition) {
    repair_instance_t repairs[4];
    uint32_t count = speech_repair_detect_repairs(processor,
        "I want the the book", repairs, 4);

    EXPECT_GT(count, 0u);
    bool found_repetition = false;
    for (uint32_t i = 0; i < count; i++) {
        if (repairs[i].type == REPAIR_TYPE_REPETITION) {
            found_repetition = true;
            break;
        }
    }
    EXPECT_TRUE(found_repetition);
}

TEST_F(SpeechRepairTest, RepairTypeNames) {
    EXPECT_STREQ(speech_repair_type_name(REPAIR_TYPE_NONE), "NONE");
    EXPECT_STREQ(speech_repair_type_name(REPAIR_TYPE_RESTART), "RESTART");
    EXPECT_STREQ(speech_repair_type_name(REPAIR_TYPE_CORRECTION), "CORRECTION");
    EXPECT_STREQ(speech_repair_type_name(REPAIR_TYPE_REPETITION), "REPETITION");
    EXPECT_STREQ(speech_repair_type_name(static_cast<repair_type_t>(999)), "INVALID");
}

// Full Analysis Tests
TEST_F(SpeechRepairTest, FullAnalysis) {
    repair_analysis_t analysis;
    EXPECT_TRUE(speech_repair_analyze(processor,
        "I um want to, I mean, need help", &analysis));

    EXPECT_GT(analysis.disfluency_count, 0u);
    EXPECT_TRUE(analysis.has_cleaned_output);
    EXPECT_GE(analysis.fluency_score, 0.0f);
    EXPECT_LE(analysis.fluency_score, 1.0f);
}

TEST_F(SpeechRepairTest, FullAnalysisFluent) {
    repair_analysis_t analysis;
    EXPECT_TRUE(speech_repair_analyze(processor,
        "The weather is very nice today", &analysis));

    EXPECT_EQ(analysis.disfluency_count, 0u);
    EXPECT_EQ(analysis.repair_count, 0u);
    EXPECT_FLOAT_EQ(analysis.fluency_score, 1.0f);
}

// Cleaning Tests
TEST_F(SpeechRepairTest, CleanUtterance) {
    char cleaned[256];
    EXPECT_TRUE(speech_repair_clean(processor,
        "I um want to go", cleaned, sizeof(cleaned)));

    EXPECT_STRNE(cleaned, "I um want to go");
    EXPECT_TRUE(strstr(cleaned, "um") == NULL || strlen(cleaned) < strlen("I um want to go"));
}

TEST_F(SpeechRepairTest, CleanMultipleFillers) {
    char cleaned[256];
    EXPECT_TRUE(speech_repair_clean(processor,
        "So um like you know the thing", cleaned, sizeof(cleaned)));

    // Should have removed at least some fillers
    EXPECT_LT(strlen(cleaned), strlen("So um like you know the thing"));
}

// Repair Generation Tests
TEST_F(SpeechRepairTest, GenerateCorrection) {
    char output[256];
    EXPECT_TRUE(speech_repair_generate_correction(processor,
        "left", "right", REPAIR_TYPE_CORRECTION, output, sizeof(output)));

    EXPECT_TRUE(strstr(output, "left") != NULL);
    EXPECT_TRUE(strstr(output, "right") != NULL);
    EXPECT_TRUE(strstr(output, "mean") != NULL);
}

TEST_F(SpeechRepairTest, GenerateReplacement) {
    char output[256];
    EXPECT_TRUE(speech_repair_generate_correction(processor,
        "Monday", "Tuesday", REPAIR_TYPE_REPLACEMENT, output, sizeof(output)));

    EXPECT_TRUE(strstr(output, "Monday") != NULL);
    EXPECT_TRUE(strstr(output, "Tuesday") != NULL);
}

TEST_F(SpeechRepairTest, InsertHesitation) {
    char output[256];
    EXPECT_TRUE(speech_repair_insert_hesitation(processor,
        "I want to go", 2, DISFLUENCY_FILLED_PAUSE, output, sizeof(output)));

    EXPECT_TRUE(strstr(output, "um") != NULL);
}

// Statistics Tests
TEST_F(SpeechRepairTest, StatsTracking) {
    repair_analysis_t analysis;
    speech_repair_analyze(processor, "I um want help", &analysis);

    repair_stats_t stats;
    EXPECT_TRUE(speech_repair_get_stats(processor, &stats));
    EXPECT_GT(stats.utterances_processed, 0u);
    EXPECT_GT(stats.disfluencies_detected, 0u);
}

TEST_F(SpeechRepairTest, StatsReset) {
    repair_analysis_t analysis;
    speech_repair_analyze(processor, "I um want help", &analysis);

    speech_repair_reset_stats(processor);

    repair_stats_t stats;
    speech_repair_get_stats(processor, &stats);
    EXPECT_EQ(stats.utterances_processed, 0u);
}

// Configuration
TEST_F(SpeechRepairTest, GetConfig) {
    speech_repair_config_t retrieved;
    EXPECT_TRUE(speech_repair_get_config(processor, &retrieved));
    EXPECT_EQ(retrieved.max_repairs, config.max_repairs);
}

// Null Checks
TEST_F(SpeechRepairTest, NullChecks) {
    disfluency_t disfluencies[4];
    repair_instance_t repairs[4];
    repair_analysis_t analysis;
    char cleaned[64];

    EXPECT_EQ(speech_repair_detect_disfluencies(NULL, "test", disfluencies, 4), 0u);
    EXPECT_EQ(speech_repair_detect_disfluencies(processor, NULL, disfluencies, 4), 0u);
    EXPECT_EQ(speech_repair_detect_repairs(NULL, "test", repairs, 4), 0u);
    EXPECT_FALSE(speech_repair_analyze(NULL, "test", &analysis));
    EXPECT_FALSE(speech_repair_analyze(processor, NULL, &analysis));
    EXPECT_FALSE(speech_repair_clean(NULL, "test", cleaned, 64));
    EXPECT_FALSE(speech_repair_is_filler(NULL));

    EXPECT_EQ(speech_repair_get_status(NULL), REPAIR_STATUS_ERROR);
    EXPECT_EQ(speech_repair_get_last_error(NULL), REPAIR_ERROR_INTERNAL);
}

// Preserve Disfluencies Option
TEST_F(SpeechRepairTest, PreserveDisfluencies) {
    speech_repair_config_t preserve_config = speech_repair_default_config();
    preserve_config.preserve_disfluencies = true;

    speech_repair_t* preserve_proc = speech_repair_create(&preserve_config);
    ASSERT_NE(nullptr, preserve_proc);

    // With preservation enabled, analysis should still work
    repair_analysis_t analysis;
    EXPECT_TRUE(speech_repair_analyze(preserve_proc, "I um want help", &analysis));

    speech_repair_destroy(preserve_proc);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
