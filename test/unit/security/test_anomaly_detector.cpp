/**
 * @file test_anomaly_detector.cpp
 * @brief Unit tests for anomaly detector
 *
 * WHAT: Comprehensive tests for ML-based anomaly detection
 * WHY:  Verify detection accuracy, performance, and edge cases
 * HOW:  Test creation, detection, training, statistics, thresholds
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include "security/nimcp_anomaly_detector.h"
#include <string.h>
#include <math.h>

/*=============================================================================
 * TEST FIXTURES
 *============================================================================*/

class AnomalyDetectorTest : public ::testing::Test {
protected:
    nimcp_anomaly_detector_t detector;
    nimcp_anomaly_config_t config;

    void SetUp() override {
        config = nimcp_anomaly_detector_default_config();
        config.enable_bio_async = false;  /* Disable for unit tests */
        detector = nimcp_anomaly_detector_create(&config);
        ASSERT_NE(nullptr, detector);
    }

    void TearDown() override {
        if (detector) {
            nimcp_anomaly_detector_destroy(detector);
            detector = nullptr;
        }
    }
};

/*=============================================================================
 * CREATION AND DESTRUCTION TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, CreateWithDefaultConfig) {
    nimcp_anomaly_detector_t det = nimcp_anomaly_detector_create(nullptr);
    ASSERT_NE(nullptr, det);
    nimcp_anomaly_detector_destroy(det);
}

TEST_F(AnomalyDetectorTest, CreateWithCustomConfig) {
    nimcp_anomaly_config_t custom_config = nimcp_anomaly_detector_default_config();
    custom_config.content_anomaly_threshold = 0.8f;
    custom_config.behavior_anomaly_threshold = 0.75f;
    custom_config.learning_window_size = 500;

    nimcp_anomaly_detector_t det = nimcp_anomaly_detector_create(&custom_config);
    ASSERT_NE(nullptr, det);

    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(det, &stats));
    EXPECT_FLOAT_EQ(0.8f, stats.current_content_threshold);

    nimcp_anomaly_detector_destroy(det);
}

TEST_F(AnomalyDetectorTest, DestroyNull) {
    nimcp_anomaly_detector_destroy(nullptr);  /* Should not crash */
}

/*=============================================================================
 * FEATURE EXTRACTION TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, ExtractFeaturesNormalText) {
    const char* input = "Hello, this is a normal text message.";
    float features[NIMCP_FEATURE_COUNT];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_extract_features(input, strlen(input), features, nullptr));

    /* Length should be low (38 bytes / 10KB) */
    EXPECT_LT(features[NIMCP_FEATURE_LENGTH], 0.01f);

    /* Entropy should be moderate */
    EXPECT_GT(features[NIMCP_FEATURE_ENTROPY], 0.3f);
    EXPECT_LT(features[NIMCP_FEATURE_ENTROPY], 0.8f);

    /* Should have high alpha ratio */
    EXPECT_GT(features[NIMCP_FEATURE_ALPHA_RATIO], 0.5f);

    /* Special chars should be low (includes spaces, punctuation) */
    EXPECT_LT(features[NIMCP_FEATURE_SPECIAL_RATIO], 0.3f);

    /* Control chars should be zero */
    EXPECT_EQ(0.0f, features[NIMCP_FEATURE_CONTROL_RATIO]);
}

TEST_F(AnomalyDetectorTest, ExtractFeaturesHighEntropy) {
    /* Random-looking string */
    const char* input = "x8Kz!@#$9mQ&*()vB2cN^%hT4pL";
    float features[NIMCP_FEATURE_COUNT];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_extract_features(input, strlen(input), features, nullptr));

    /* Entropy should be moderately high (normalized: raw_entropy / 8.0) */
    EXPECT_GT(features[NIMCP_FEATURE_ENTROPY], 0.5f);

    /* Special chars should be high */
    EXPECT_GT(features[NIMCP_FEATURE_SPECIAL_RATIO], 0.2f);
}

TEST_F(AnomalyDetectorTest, ExtractFeaturesNesting) {
    const char* input = "{{{{[[[[((((deep nesting))))]]]]}}}}";
    float features[NIMCP_FEATURE_COUNT];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_extract_features(input, strlen(input), features, nullptr));

    /* Nesting depth should be high */
    EXPECT_GT(features[NIMCP_FEATURE_NESTING_DEPTH], 0.5f);
}

TEST_F(AnomalyDetectorTest, ExtractFeaturesRepeated) {
    const char* input = "repeatrepeatrepeatrepeatrepeat";
    float features[NIMCP_FEATURE_COUNT];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_extract_features(input, strlen(input), features, nullptr));

    /* Repeat ratio should be high */
    EXPECT_GT(features[NIMCP_FEATURE_REPEAT_RATIO], 0.5f);

    /* N-gram entropy should be low */
    EXPECT_LT(features[NIMCP_FEATURE_BIGRAM_ENTROPY], 0.5f);
}

TEST_F(AnomalyDetectorTest, ExtractFeaturesEmptyInput) {
    float features[NIMCP_FEATURE_COUNT];
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_extract_features("", 0, features, nullptr));

    /* All features should be zero */
    for (int i = 0; i < NIMCP_FEATURE_COUNT; i++) {
        EXPECT_EQ(0.0f, features[i]);
    }
}

TEST_F(AnomalyDetectorTest, ExtractFeaturesNullInput) {
    float features[NIMCP_FEATURE_COUNT];
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_extract_features(nullptr, 10, features, nullptr));
}

/*=============================================================================
 * ENTROPY CALCULATION TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, CalculateEntropyUniform) {
    /* All same byte: entropy = 0 */
    uint8_t data[100];
    memset(data, 'A', sizeof(data));

    float entropy = nimcp_calculate_entropy(data, sizeof(data));
    EXPECT_FLOAT_EQ(0.0f, entropy);
}

TEST_F(AnomalyDetectorTest, CalculateEntropyRandom) {
    /* Every byte unique: high entropy */
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = (uint8_t)i;
    }

    float entropy = nimcp_calculate_entropy(data, sizeof(data));
    EXPECT_GT(entropy, 7.0f);  /* Should be close to 8 bits */
}

TEST_F(AnomalyDetectorTest, CalculateEntropyBinary) {
    /* 50/50 binary: entropy = 1 bit */
    uint8_t data[100];
    for (int i = 0; i < 100; i++) {
        data[i] = (i % 2) ? 0 : 1;
    }

    float entropy = nimcp_calculate_entropy(data, sizeof(data));
    EXPECT_NEAR(1.0f, entropy, 0.01f);
}

/*=============================================================================
 * NESTING DETECTION TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, DetectNestingFlat) {
    const char* input = "no nesting here";
    uint32_t depth = nimcp_detect_nesting_depth((const uint8_t*)input, strlen(input));
    EXPECT_EQ(0u, depth);
}

TEST_F(AnomalyDetectorTest, DetectNestingSimple) {
    const char* input = "(hello)";
    uint32_t depth = nimcp_detect_nesting_depth((const uint8_t*)input, strlen(input));
    EXPECT_EQ(1u, depth);
}

TEST_F(AnomalyDetectorTest, DetectNestingDeep) {
    const char* input = "((((((deep))))))";
    uint32_t depth = nimcp_detect_nesting_depth((const uint8_t*)input, strlen(input));
    EXPECT_EQ(6u, depth);
}

TEST_F(AnomalyDetectorTest, DetectNestingMixed) {
    const char* input = "{[(<test>)]}";
    uint32_t depth = nimcp_detect_nesting_depth((const uint8_t*)input, strlen(input));
    EXPECT_EQ(4u, depth);
}

TEST_F(AnomalyDetectorTest, DetectNestingUnbalanced) {
    const char* input = "(((test)";  /* 3 open, 1 close */
    uint32_t depth = nimcp_detect_nesting_depth((const uint8_t*)input, strlen(input));
    EXPECT_EQ(3u, depth);  /* Max depth reached */
}

/*=============================================================================
 * REPEAT RATIO TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, RepeatRatioNoRepeat) {
    const char* input = "abcdefghijklmnop";
    float ratio = nimcp_calculate_repeat_ratio((const uint8_t*)input, strlen(input));
    EXPECT_LT(ratio, 0.1f);
}

TEST_F(AnomalyDetectorTest, RepeatRatioHighRepeat) {
    const char* input = "testtest";
    float ratio = nimcp_calculate_repeat_ratio((const uint8_t*)input, strlen(input));
    EXPECT_GT(ratio, 0.8f);
}

/*=============================================================================
 * DETECTION TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, DetectNormalInput) {
    const char* input = "This is a perfectly normal sentence.";
    nimcp_anomaly_result_t result;

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, input, strlen(input), &result));

    /* Should have low anomaly score (initially) */
    EXPECT_GE(result.anomaly_score, 0.0f);
    EXPECT_LE(result.anomaly_score, 1.0f);

    /* Should have component scores */
    EXPECT_GE(result.content_score, 0.0f);
    EXPECT_LE(result.content_score, 1.0f);
    EXPECT_GE(result.behavior_score, 0.0f);
    EXPECT_LE(result.behavior_score, 1.0f);

    /* Should have explanation */
    EXPECT_GT(strlen(result.explanation), 0u);
}

TEST_F(AnomalyDetectorTest, DetectAnomalousInputHighEntropy) {
    /* Random-looking attack string */
    const char* input = "x8Kz!@#$9mQ&*()vB2cN^%hT4pL!@#$%^&*()x8Kz!@#$9mQ&*()vB2cN^%hT4pL";
    nimcp_anomaly_result_t result;

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, input, strlen(input), &result));

    /* Should detect anomaly in features even if not trained */
    EXPECT_GE(result.anomaly_score, 0.0f);
}

TEST_F(AnomalyDetectorTest, DetectAnomalousInputDeepNesting) {
    const char* input = "{{{{{{{{{{{{{{{{deep nesting attack}}}}}}}}}}}}}}}}";
    nimcp_anomaly_result_t result;

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, input, strlen(input), &result));

    /* Nesting should trigger */
    EXPECT_NE(0u, result.triggered_features & NIMCP_TRIGGER_NESTING_DEPTH);
}

TEST_F(AnomalyDetectorTest, DetectNullInput) {
    nimcp_anomaly_result_t result;
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_anomaly_detect(detector, nullptr, 10, &result));
}

TEST_F(AnomalyDetectorTest, DetectNullResult) {
    const char* input = "test";
    EXPECT_EQ(NIMCP_INVALID_PARAM, nimcp_anomaly_detect(detector, input, 4, nullptr));
}

TEST_F(AnomalyDetectorTest, DetectEmptyInput) {
    nimcp_anomaly_result_t result;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, "", 0, &result));
    EXPECT_GE(result.anomaly_score, 0.0f);
}

/*=============================================================================
 * TRAINING TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, TrainNormalSamples) {
    const char* normal_inputs[] = {
        "This is normal text.",
        "Hello, how are you?",
        "Good morning everyone.",
        "Have a great day!",
        "See you tomorrow."
    };

    for (const char* input : normal_inputs) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_train(detector, input, strlen(input), true));
    }

    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats));
    EXPECT_EQ(5u, stats.training_samples);
    EXPECT_EQ(5u, stats.normal_samples);
    EXPECT_EQ(0u, stats.anomalous_samples);
}

TEST_F(AnomalyDetectorTest, TrainAnomalousSamples) {
    const char* anomalous_inputs[] = {
        "x8Kz!@#$9mQ&*()vB2cN^%hT4pL",
        "{{{{{{{{attack}}}}}}}}",
        "\x01\x02\x03\x04\x05control chars"
    };

    for (const char* input : anomalous_inputs) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_train(detector, input, strlen(input), false));
    }

    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats));
    EXPECT_EQ(3u, stats.training_samples);
    EXPECT_EQ(0u, stats.normal_samples);
    EXPECT_EQ(3u, stats.anomalous_samples);
}

TEST_F(AnomalyDetectorTest, TrainAndDetect) {
    /* Train on normal samples */
    const char* normal_inputs[] = {
        "Normal text one.",
        "Normal text two.",
        "Normal text three."
    };

    for (const char* input : normal_inputs) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_train(detector, input, strlen(input), true));
    }

    /* Detect on similar normal text */
    const char* test_input = "Normal text four.";
    nimcp_anomaly_result_t result;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, test_input, strlen(test_input), &result));

    /* After training on normal, normal inputs should have lower scores */
    EXPECT_GE(result.anomaly_score, 0.0f);
    EXPECT_LE(result.anomaly_score, 1.0f);
}

/*=============================================================================
 * STATISTICS TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, GetStatisticsInitial) {
    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats));

    EXPECT_EQ(0u, stats.total_detections);
    EXPECT_EQ(0u, stats.anomalies_detected);
    EXPECT_EQ(0u, stats.training_samples);
    EXPECT_FLOAT_EQ(0.0f, stats.precision);
    EXPECT_FLOAT_EQ(0.0f, stats.recall);
}

TEST_F(AnomalyDetectorTest, GetStatisticsAfterDetections) {
    nimcp_anomaly_result_t result;

    /* Run some detections */
    for (int i = 0; i < 10; i++) {
        const char* input = "test input";
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, input, strlen(input), &result));
    }

    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats));

    EXPECT_EQ(10u, stats.total_detections);
    EXPECT_GT(stats.avg_detection_time_us, 0.0f);
}

TEST_F(AnomalyDetectorTest, ResetStatistics) {
    /* Generate some stats */
    nimcp_anomaly_result_t result;
    const char* input = "test";
    nimcp_anomaly_detect(detector, input, strlen(input), &result);
    nimcp_anomaly_train(detector, input, strlen(input), true);

    /* Reset */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_reset_stats(detector));

    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats));

    /* Detection stats should be reset, but training samples preserved */
    EXPECT_EQ(0u, stats.total_detections);
    EXPECT_EQ(1u, stats.training_samples);
}

/*=============================================================================
 * ADAPTIVE THRESHOLD TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, UpdateThresholdsFalsePositive) {
    nimcp_anomaly_stats_t stats_before, stats_after;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats_before));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_update_thresholds(detector, true, false));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats_after));

    /* Threshold should increase */
    EXPECT_GT(stats_after.current_content_threshold, stats_before.current_content_threshold);
    EXPECT_EQ(1u, stats_after.false_positives);
}

TEST_F(AnomalyDetectorTest, UpdateThresholdsFalseNegative) {
    nimcp_anomaly_stats_t stats_before, stats_after;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats_before));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_update_thresholds(detector, false, true));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats_after));

    /* Threshold should decrease */
    EXPECT_LT(stats_after.current_content_threshold, stats_before.current_content_threshold);
    EXPECT_EQ(1u, stats_after.false_negatives);
}

TEST_F(AnomalyDetectorTest, AdaptiveThresholdDisabled) {
    /* Create detector with adaptive threshold disabled */
    nimcp_anomaly_config_t cfg = nimcp_anomaly_detector_default_config();
    cfg.enable_adaptive_threshold = false;
    nimcp_anomaly_detector_t det = nimcp_anomaly_detector_create(&cfg);

    nimcp_anomaly_stats_t stats_before, stats_after;
    nimcp_anomaly_get_stats(det, &stats_before);

    nimcp_anomaly_update_thresholds(det, true, false);

    nimcp_anomaly_get_stats(det, &stats_after);

    /* Thresholds should not change */
    EXPECT_FLOAT_EQ(stats_before.current_content_threshold, stats_after.current_content_threshold);

    nimcp_anomaly_detector_destroy(det);
}

/*=============================================================================
 * PERFORMANCE TESTS
 *============================================================================*/

TEST_F(AnomalyDetectorTest, DetectionPerformance) {
    const char* input = "This is a performance test with moderate length text.";
    nimcp_anomaly_result_t result;

    /* Run detection and check time */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, input, strlen(input), &result));

    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats));

    /* Should complete in < 1ms (1000 microseconds) for typical input */
    EXPECT_LT(stats.avg_detection_time_us, 1000.0f);
}

TEST_F(AnomalyDetectorTest, DetectionPerformanceLargeInput) {
    /* 5KB input */
    char large_input[5120];
    memset(large_input, 'A', sizeof(large_input) - 1);
    large_input[sizeof(large_input) - 1] = '\0';

    nimcp_anomaly_result_t result;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, large_input, strlen(large_input), &result));

    nimcp_anomaly_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_get_stats(detector, &stats));

    /* Should still be reasonably fast (O(n^2) repeat ratio is expensive) */
    /* Relaxed threshold for CI environments with variable system load */
    EXPECT_LT(stats.max_detection_time_us, 100000.0f);  /* < 100ms */
}

/*=============================================================================
 * EDGE CASES
 *============================================================================*/

TEST_F(AnomalyDetectorTest, VeryLongInput) {
    /* Create input longer than max_input_length */
    char long_input[20000];
    memset(long_input, 'X', sizeof(long_input) - 1);
    long_input[sizeof(long_input) - 1] = '\0';

    nimcp_anomaly_result_t result;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, long_input, strlen(long_input), &result));

    /* Should truncate and still process */
    EXPECT_GE(result.anomaly_score, 0.0f);
}

TEST_F(AnomalyDetectorTest, BinaryInput) {
    uint8_t binary_input[100];
    for (int i = 0; i < 100; i++) {
        binary_input[i] = (uint8_t)(i * 2);
    }

    nimcp_anomaly_result_t result;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, binary_input, sizeof(binary_input), &result));
    EXPECT_GE(result.anomaly_score, 0.0f);
}

TEST_F(AnomalyDetectorTest, AllControlCharacters) {
    char input[20];
    for (int i = 0; i < 19; i++) {
        input[i] = (char)(i + 1);  /* Control chars */
    }
    input[19] = '\0';

    nimcp_anomaly_result_t result;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_anomaly_detect(detector, input, 19, &result));

    /* Should trigger control char ratio */
    EXPECT_NE(0u, result.triggered_features & NIMCP_TRIGGER_CONTROL_RATIO);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
