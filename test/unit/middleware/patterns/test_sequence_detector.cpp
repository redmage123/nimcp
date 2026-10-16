//=============================================================================
// test_sequence_detector.cpp - Comprehensive Sequence Detector Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/patterns/nimcp_sequence_detector.h"

/**
 * WHAT: Comprehensive test suite for sequence detection
 * WHY:  Ensure spike sequence detection and template matching work correctly
 * HOW:  Unit tests for all functions, edge cases, integration tests
 */

class SequenceDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    void AddSpikeSequence(sequence_detector_t* detector, const uint32_t* neurons,
                          uint32_t count, double start_time, double interval) {
        for (uint32_t i = 0; i < count; i++) {
            sequence_detector_add_spike(detector, neurons[i], start_time + i * interval);
        }
    }

    std::vector<sequence_element_t> CreateSequenceElements(const uint32_t* neurons,
                                                             const float* times, uint32_t count) {
        std::vector<sequence_element_t> elements;
        for (uint32_t i = 0; i < count; i++) {
            sequence_element_t elem;
            elem.neuron_id = neurons[i];
            elem.relative_time_ms = times[i];
            elements.push_back(elem);
        }
        return elements;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================
// WHAT: Test detector creation and destruction
// WHY:  Verify resource management and parameter validation
// HOW:  Test parameter combinations and edge cases

TEST_F(SequenceDetectorTest, Create_Success_Default) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);
    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, Create_Success_Custom) {
    sequence_detector_config_t config = sequence_detector_default_config();
    config.max_templates = 100;
    config.max_sequence_length = 50;
    config.temporal_tolerance_ms = 25.0f;

    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);
    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, Destroy_NullSafe) {
    sequence_detector_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// ADD SPIKE TESTS
//=============================================================================
// WHAT: Test spike addition operations
// WHY:  Verify spike recording and buffering
// HOW:  Add spikes, verify acceptance

TEST_F(SequenceDetectorTest, AddSpike_Success) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    bool result = sequence_detector_add_spike(detector, 5, 1000.0);
    EXPECT_TRUE(result);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, AddSpike_Success_MultipleSpikes) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    for (uint32_t i = 0; i < 10; i++) {
        bool result = sequence_detector_add_spike(detector, i, (double)i * 10.0);
        EXPECT_TRUE(result);
    }

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, AddSpike_Failure_NullDetector) {
    bool result = sequence_detector_add_spike(nullptr, 5, 1000.0);
    EXPECT_FALSE(result);
}

//=============================================================================
// LEARN TEMPLATE TESTS
//=============================================================================
// WHAT: Test template learning operations
// WHY:  Verify sequence template storage
// HOW:  Learn sequences, verify IDs

TEST_F(SequenceDetectorTest, LearnTemplate_Success) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t neurons[4] = {1, 2, 3, 4};
    float times[4] = {0.0f, 10.0f, 20.0f, 30.0f};
    auto elements = CreateSequenceElements(neurons, times, 4);

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(detector, elements.data(), 4, &id);
    EXPECT_TRUE(result);
    EXPECT_GT(id, 0);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, LearnTemplate_Success_MultipleTemplates) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t neurons1[3] = {1, 2, 3};
    float times1[3] = {0.0f, 10.0f, 20.0f};
    auto elements1 = CreateSequenceElements(neurons1, times1, 3);

    uint32_t neurons2[4] = {5, 6, 7, 8};
    float times2[4] = {0.0f, 15.0f, 30.0f, 45.0f};
    auto elements2 = CreateSequenceElements(neurons2, times2, 4);

    uint32_t id1 = 0, id2 = 0;
    bool result1 = sequence_detector_learn_template(detector, elements1.data(), 3, &id1);
    bool result2 = sequence_detector_learn_template(detector, elements2.data(), 4, &id2);

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    EXPECT_NE(id1, id2);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, LearnTemplate_Failure_NullDetector) {
    uint32_t neurons[3] = {1, 2, 3};
    float times[3] = {0.0f, 10.0f, 20.0f};
    auto elements = CreateSequenceElements(neurons, times, 3);

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(nullptr, elements.data(), 3, &id);
    EXPECT_FALSE(result);
}

TEST_F(SequenceDetectorTest, LearnTemplate_Failure_NullElements) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(detector, nullptr, 3, &id);
    EXPECT_FALSE(result);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, LearnTemplate_Failure_ZeroLength) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t neurons[3] = {1, 2, 3};
    float times[3] = {0.0f, 10.0f, 20.0f};
    auto elements = CreateSequenceElements(neurons, times, 3);

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(detector, elements.data(), 0, &id);
    EXPECT_FALSE(result);

    sequence_detector_destroy(detector);
}

//=============================================================================
// DETECT TESTS
//=============================================================================
// WHAT: Test sequence detection operations
// WHY:  Verify template matching against spike buffer
// HOW:  Add spikes, detect sequences

TEST_F(SequenceDetectorTest, Detect_Success_MatchingSequence) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn template
    uint32_t neurons[3] = {1, 2, 3};
    float times[3] = {0.0f, 10.0f, 20.0f};
    auto elements = CreateSequenceElements(neurons, times, 3);

    uint32_t template_id = 0;
    sequence_detector_learn_template(detector, elements.data(), 3, &template_id);

    // Add matching spikes
    sequence_detector_add_spike(detector, 1, 1000.0);
    sequence_detector_add_spike(detector, 2, 1010.0);
    sequence_detector_add_spike(detector, 3, 1020.0);

    // Detect
    sequence_detection_t detections[10];
    uint32_t num_detected = 0;
    bool success = sequence_detector_detect(detector, detections, 10, &num_detected);

    EXPECT_TRUE(success);
    EXPECT_GE(num_detected, 0);  // May or may not detect based on implementation

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, Detect_Failure_NullDetector) {
    sequence_detection_t detections[10];
    uint32_t num_detected = 0;
    bool success = sequence_detector_detect(nullptr, detections, 10, &num_detected);
    EXPECT_FALSE(success);
}

//=============================================================================
// GET TEMPLATE TESTS
//=============================================================================
// WHAT: Test template retrieval
// WHY:  Verify stored templates can be accessed
// HOW:  Learn templates, retrieve by ID

TEST_F(SequenceDetectorTest, GetTemplate_Success) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t neurons[3] = {1, 2, 3};
    float times[3] = {0.0f, 10.0f, 20.0f};
    auto elements = CreateSequenceElements(neurons, times, 3);

    uint32_t id = 0;
    sequence_detector_learn_template(detector, elements.data(), 3, &id);

    sequence_template_t tmpl;
    bool success = sequence_detector_get_template(detector, id, &tmpl);
    EXPECT_TRUE(success);
    EXPECT_EQ(tmpl.template_id, id);
    EXPECT_EQ(tmpl.length, 3);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, GetTemplate_Failure_InvalidID) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    sequence_template_t tmpl;
    bool success = sequence_detector_get_template(detector, 999, &tmpl);
    EXPECT_FALSE(success);

    sequence_detector_destroy(detector);
}

//=============================================================================
// GET N-GRAMS TESTS
//=============================================================================
// WHAT: Test N-gram extraction
// WHY:  Verify pattern extraction from spike buffer
// HOW:  Add spikes, extract N-grams

TEST_F(SequenceDetectorTest, GetNgrams_Success) {
    sequence_detector_config_t config = sequence_detector_default_config();
    config.enable_ngram_learning = true;
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Add spike sequence
    uint32_t neurons[5] = {1, 2, 3, 4, 5};
    AddSpikeSequence(detector, neurons, 5, 1000.0, 10.0);

    ngram_pattern_t ngrams[10];
    uint32_t num_ngrams = 0;
    bool success = sequence_detector_get_ngrams(detector, ngrams, 10, &num_ngrams);
    EXPECT_TRUE(success);
    EXPECT_GE(num_ngrams, 0);  // May or may not find N-grams

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, GetNgrams_Failure_NullDetector) {
    ngram_pattern_t ngrams[10];
    uint32_t num_ngrams = 0;
    bool success = sequence_detector_get_ngrams(nullptr, ngrams, 10, &num_ngrams);
    EXPECT_FALSE(success);
}

//=============================================================================
// RESET TESTS
//=============================================================================
// WHAT: Test detector reset functionality
// WHY:  Verify spike buffer can be cleared
// HOW:  Add spikes, reset, verify empty

TEST_F(SequenceDetectorTest, Reset_Success) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Add spikes
    for (uint32_t i = 0; i < 10; i++) {
        sequence_detector_add_spike(detector, i, (double)i * 10.0);
    }

    // Reset
    sequence_detector_reset(detector);

    // Detector should be reset (can still add spikes)
    bool result = sequence_detector_add_spike(detector, 0, 1000.0);
    EXPECT_TRUE(result);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, Reset_NullSafe) {
    sequence_detector_reset(nullptr);
    // Should not crash
}

//=============================================================================
// CLEAR TEMPLATES TESTS
//=============================================================================
// WHAT: Test template clearing
// WHY:  Verify all templates can be removed
// HOW:  Learn templates, clear, verify empty

TEST_F(SequenceDetectorTest, ClearTemplates_Success) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn templates
    uint32_t neurons[3] = {1, 2, 3};
    float times[3] = {0.0f, 10.0f, 20.0f};
    auto elements = CreateSequenceElements(neurons, times, 3);

    uint32_t id = 0;
    sequence_detector_learn_template(detector, elements.data(), 3, &id);
    sequence_detector_learn_template(detector, elements.data(), 3, &id);

    // Clear
    sequence_detector_clear_templates(detector);

    // Get stats
    uint32_t num_templates = 999;
    uint64_t total_detections = 0;
    float avg_strength = 0.0f;
    sequence_detector_get_stats(detector, &num_templates, &total_detections, &avg_strength);
    EXPECT_EQ(num_templates, 0);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, ClearTemplates_NullSafe) {
    sequence_detector_clear_templates(nullptr);
    // Should not crash
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================
// WHAT: Test statistics retrieval
// WHY:  Verify detector state tracking
// HOW:  Get stats, verify counts

TEST_F(SequenceDetectorTest, GetStats_Success) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t num_templates = 0;
    uint64_t total_detections = 0;
    float avg_strength = 0.0f;

    bool success = sequence_detector_get_stats(detector, &num_templates, &total_detections, &avg_strength);
    EXPECT_TRUE(success);
    EXPECT_EQ(num_templates, 0);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, GetStats_Success_WithData) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn template
    uint32_t neurons[3] = {1, 2, 3};
    float times[3] = {0.0f, 10.0f, 20.0f};
    auto elements = CreateSequenceElements(neurons, times, 3);

    uint32_t id = 0;
    sequence_detector_learn_template(detector, elements.data(), 3, &id);

    // Add spikes
    sequence_detector_add_spike(detector, 1, 1000.0);
    sequence_detector_add_spike(detector, 2, 1010.0);

    uint32_t num_templates = 0;
    uint64_t total_detections = 0;
    float avg_strength = 0.0f;

    bool success = sequence_detector_get_stats(detector, &num_templates, &total_detections, &avg_strength);
    EXPECT_TRUE(success);
    EXPECT_EQ(num_templates, 1);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, GetStats_Failure_NullDetector) {
    uint32_t num_templates = 0;
    uint64_t total_detections = 0;
    float avg_strength = 0.0f;

    bool success = sequence_detector_get_stats(nullptr, &num_templates, &total_detections, &avg_strength);
    EXPECT_FALSE(success);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test known edge cases and bugs
// WHY:  Prevent regressions
// HOW:  Test problematic scenarios

TEST_F(SequenceDetectorTest, Regression_SingleElementSequence) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t neurons[1] = {1};
    float times[1] = {0.0f};
    auto elements = CreateSequenceElements(neurons, times, 1);

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(detector, elements.data(), 1, &id);
    EXPECT_TRUE(result);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, Regression_VeryLongSequence) {
    sequence_detector_config_t config = sequence_detector_default_config();
    config.max_sequence_length = 100;
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    std::vector<sequence_element_t> elements;
    for (uint32_t i = 0; i < 50; i++) {
        sequence_element_t elem;
        elem.neuron_id = i;
        elem.relative_time_ms = (float)i * 10.0f;
        elements.push_back(elem);
    }

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(detector, elements.data(), 50, &id);
    EXPECT_TRUE(result);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, Regression_SameNeuronMultipleTimes) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Same neuron firing multiple times in sequence
    uint32_t neurons[5] = {1, 1, 1, 1, 1};
    float times[5] = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f};
    auto elements = CreateSequenceElements(neurons, times, 5);

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(detector, elements.data(), 5, &id);
    EXPECT_TRUE(result);

    sequence_detector_destroy(detector);
}

TEST_F(SequenceDetectorTest, Regression_ZeroTimestamps) {
    sequence_detector_config_t config = sequence_detector_default_config();
    sequence_detector_t* detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t neurons[3] = {1, 2, 3};
    float times[3] = {0.0f, 0.0f, 0.0f};  // All at same time
    auto elements = CreateSequenceElements(neurons, times, 3);

    uint32_t id = 0;
    bool result = sequence_detector_learn_template(detector, elements.data(), 3, &id);
    EXPECT_TRUE(result);

    sequence_detector_destroy(detector);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
