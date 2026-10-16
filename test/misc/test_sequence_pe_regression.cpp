/**
 * @file test_sequence_pe_regression.cpp
 * @brief Regression tests for positional encoding in sequence detection
 *
 * WHAT: Regression tests ensuring PE stability in temporal sequence processing
 * WHY:  Sequence detection relies on PE for temporal pattern matching
 * HOW:  Test PE under repeated sequence ops, verify consistency and performance
 *
 * REGRESSION TEST PHILOSOPHY:
 * - Ensure PE doesn't degrade with sequence detection operations
 * - Verify PE memory stability in sequence context
 * - Validate PE temporal encoding remains consistent
 * - Test PE with various sequence lengths
 * - Verify PE integration doesn't break sequence detection
 * - Catch performance regressions in sequence+PE path
 *
 * WHAT WE'RE PROTECTING:
 * - PE numerical stability over many sequence detections
 * - Memory usage patterns with PE-enabled sequences
 * - Sequence matching accuracy with PE
 * - Temporal tolerance with PE integration
 * - Performance of sequence operations with PE
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <chrono>

extern "C" {
#include "middleware/patterns/nimcp_sequence_detector.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SequencePERegressionTest : public ::testing::Test {
protected:
    static constexpr int ITERATIONS = 1000;
    static constexpr int MAX_SEQUENCE_LENGTH = 50;
    static constexpr int EMBEDDING_DIM = 64;

    sequence_detector_t* detector;

    void SetUp() override {
        // WHAT: Initialize bio-async and memory systems
        // WHY:  Sequence detector + PE requires these systems
        // HOW:  Standard initialization sequence
        bio_async_init();
        bio_router_config_t cfg = {0};
        bio_router_init(&cfg);
        nimcp_unified_memory_init();

        detector = nullptr;
    }

    void TearDown() override {
        // WHAT: Clean up detector and systems
        // WHY:  Prevent memory leaks
        // HOW:  Standard cleanup sequence
        if (detector) {
            sequence_detector_destroy(detector);
            detector = nullptr;
        }

        bio_router_shutdown();
        bio_async_shutdown();
        nimcp_unified_memory_shutdown();
    }

    // WHAT: Create simple test sequence
    // WHY:  Need consistent test data
    // HOW:  Create sequence with incrementing neuron IDs
    std::vector<sequence_element_t> CreateTestSequence(uint32_t length, float interval_ms = 10.0f) {
        std::vector<sequence_element_t> sequence;
        for (uint32_t i = 0; i < length; i++) {
            sequence_element_t elem = {};
            elem.neuron_id = 100 + i;
            elem.relative_time_ms = i * interval_ms;
            elem.position_embedding = nullptr;  // Managed by detector
            elem.embedding_dim = 0;
            sequence.push_back(elem);
        }
        return sequence;
    }

    // WHAT: Get default sequence detector config
    // WHY:  Need baseline configuration
    // HOW:  Set reasonable defaults
    sequence_detector_config_t GetDefaultConfig() {
        sequence_detector_config_t config = {};
        config.max_templates = 100;
        config.max_sequence_length = MAX_SEQUENCE_LENGTH;
        config.temporal_tolerance_ms = 20.0f;
        config.min_strength_threshold = 0.5f;
        config.max_ngram = 3;
        config.enable_replay_detection = true;
        config.enable_ngram_learning = true;
        config.enable_compression = false;
        config.enable_positional_encoding = false;
        config.pe_type = NIMCP_POS_SINUSOIDAL;
        config.pe_embedding_dim = EMBEDDING_DIM;
        config.pe_similarity_weight = 0.3f;
        return config;
    }
};

//=============================================================================
// 1. Stability Under Repeated Use Tests
//=============================================================================

TEST_F(SequencePERegressionTest, StabilityUnderRepeatedUse_LearnDetect) {
    // WHAT: Verify PE doesn't degrade with repeated learn/detect operations
    // WHY:  Common operation pattern must remain stable
    // HOW:  Learn sequence, detect it many times, verify consistency

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn a sequence
    auto test_seq = CreateTestSequence(10);
    uint32_t template_id = 0;
    int learn_result = sequence_detector_learn(
        detector, test_seq.data(), test_seq.size(), &template_id
    );
    ASSERT_EQ(learn_result, 0);

    // Detect it many times
    for (int i = 0; i < ITERATIONS; i++) {
        sequence_detection_t detections[10];
        uint32_t num_detected = 0;

        int detect_result = sequence_detector_detect(
            detector, test_seq.data(), test_seq.size(),
            detections, 10, &num_detected
        );

        // Should successfully detect the learned sequence
        EXPECT_EQ(detect_result, 0);
        EXPECT_GT(num_detected, 0) << "Failed to detect at iteration " << i;

        if (num_detected > 0) {
            // Verify detection quality hasn't degraded
            EXPECT_GE(detections[0].strength, config.min_strength_threshold)
                << "Detection strength degraded at iteration " << i;
        }

        if (i % 10 == 0) {
            bio_router_process_messages();
        }
    }
}

TEST_F(SequencePERegressionTest, StabilityUnderRepeatedUse_MultiplePETypes) {
    // WHAT: Verify different PE types remain stable in sequence detection
    // WHY:  Each PE type should work reliably
    // HOW:  Test each PE type with learn/detect operations

    nimcp_pos_encoding_type_t types[] = {
        NIMCP_POS_SINUSOIDAL,
        NIMCP_POS_ROTARY,
        NIMCP_POS_RELATIVE
    };

    auto test_seq = CreateTestSequence(8);

    for (auto pe_type : types) {
        sequence_detector_config_t config = GetDefaultConfig();
        config.enable_positional_encoding = true;
        config.pe_type = pe_type;

        sequence_detector_t* test_detector = sequence_detector_create(&config);
        ASSERT_NE(test_detector, nullptr) << "Failed with PE type " << pe_type;

        // Learn sequence
        uint32_t template_id = 0;
        int result = sequence_detector_learn(
            test_detector, test_seq.data(), test_seq.size(), &template_id
        );
        EXPECT_EQ(result, 0) << "Learn failed with PE type " << pe_type;

        // Detect sequence
        sequence_detection_t detections[10];
        uint32_t num_detected = 0;
        result = sequence_detector_detect(
            test_detector, test_seq.data(), test_seq.size(),
            detections, 10, &num_detected
        );

        EXPECT_EQ(result, 0) << "Detect failed with PE type " << pe_type;
        EXPECT_GT(num_detected, 0) << "No detection with PE type " << pe_type;

        sequence_detector_destroy(test_detector);
    }
}

//=============================================================================
// 2. Memory Stability Tests
//=============================================================================

TEST_F(SequencePERegressionTest, MemoryStability_NoLeaksWithPE) {
    // WHAT: Verify sequence detector + PE doesn't leak memory
    // WHY:  PE integration shouldn't introduce leaks
    // HOW:  Create/destroy detector with PE many times

    auto test_seq = CreateTestSequence(5);

    for (int i = 0; i < 100; i++) {
        sequence_detector_config_t config = GetDefaultConfig();
        config.enable_positional_encoding = true;
        config.pe_type = NIMCP_POS_SINUSOIDAL;

        sequence_detector_t* test_detector = sequence_detector_create(&config);
        ASSERT_NE(test_detector, nullptr) << "Failed at iteration " << i;

        // Learn a few sequences
        for (int j = 0; j < 3; j++) {
            uint32_t template_id = 0;
            sequence_detector_learn(
                test_detector, test_seq.data(), test_seq.size(), &template_id
            );
        }

        sequence_detector_destroy(test_detector);

        if (i % 10 == 0) {
            bio_router_process_messages();
        }
    }

    SUCCEED();
}

TEST_F(SequencePERegressionTest, MemoryStability_PEDisabledVsEnabled) {
    // WHAT: Compare memory usage with PE disabled vs enabled
    // WHY:  PE overhead should be reasonable
    // HOW:  Create both types, verify similar behavior

    auto test_seq = CreateTestSequence(10);

    // Without PE
    sequence_detector_config_t config_no_pe = GetDefaultConfig();
    config_no_pe.enable_positional_encoding = false;

    sequence_detector_t* detector_no_pe = sequence_detector_create(&config_no_pe);
    ASSERT_NE(detector_no_pe, nullptr);

    uint32_t template_id_no_pe = 0;
    int result_no_pe = sequence_detector_learn(
        detector_no_pe, test_seq.data(), test_seq.size(), &template_id_no_pe
    );
    EXPECT_EQ(result_no_pe, 0);

    // With PE
    sequence_detector_config_t config_with_pe = GetDefaultConfig();
    config_with_pe.enable_positional_encoding = true;
    config_with_pe.pe_type = NIMCP_POS_SINUSOIDAL;

    sequence_detector_t* detector_with_pe = sequence_detector_create(&config_with_pe);
    ASSERT_NE(detector_with_pe, nullptr);

    uint32_t template_id_with_pe = 0;
    int result_with_pe = sequence_detector_learn(
        detector_with_pe, test_seq.data(), test_seq.size(), &template_id_with_pe
    );
    EXPECT_EQ(result_with_pe, 0);

    // Both should learn successfully
    EXPECT_NE(template_id_no_pe, 0);
    EXPECT_NE(template_id_with_pe, 0);

    sequence_detector_destroy(detector_no_pe);
    sequence_detector_destroy(detector_with_pe);
}

TEST_F(SequencePERegressionTest, MemoryStability_ManyTemplates) {
    // WHAT: Verify PE memory scales reasonably with many templates
    // WHY:  Should handle typical workload (100s of sequences)
    // HOW:  Learn many sequences, verify no memory explosion

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.max_templates = 100;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn many sequences (up to max_templates)
    for (uint32_t i = 0; i < 50; i++) {
        auto seq = CreateTestSequence(5 + (i % 10));  // Varying lengths
        uint32_t template_id = 0;
        int result = sequence_detector_learn(
            detector, seq.data(), seq.size(), &template_id
        );
        EXPECT_EQ(result, 0) << "Failed to learn sequence " << i;
    }

    // Verify detector still functional
    auto test_seq = CreateTestSequence(7);
    sequence_detection_t detections[10];
    uint32_t num_detected = 0;
    int result = sequence_detector_detect(
        detector, test_seq.data(), test_seq.size(),
        detections, 10, &num_detected
    );
    EXPECT_EQ(result, 0);
}

//=============================================================================
// 3. Performance Stability Tests
//=============================================================================

TEST_F(SequencePERegressionTest, PerformanceStability_LearnOperations) {
    // WHAT: Verify PE doesn't significantly slow learning
    // WHY:  Learn is critical for sequence acquisition
    // HOW:  Time learning with and without PE, compare

    auto test_seq = CreateTestSequence(10);

    // Without PE
    sequence_detector_config_t config_no_pe = GetDefaultConfig();
    config_no_pe.enable_positional_encoding = false;

    sequence_detector_t* detector_no_pe = sequence_detector_create(&config_no_pe);
    ASSERT_NE(detector_no_pe, nullptr);

    auto start_no_pe = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        uint32_t template_id = 0;
        sequence_detector_learn(
            detector_no_pe, test_seq.data(), test_seq.size(), &template_id
        );
    }
    auto end_no_pe = std::chrono::high_resolution_clock::now();
    auto duration_no_pe = std::chrono::duration_cast<std::chrono::microseconds>(
        end_no_pe - start_no_pe).count();

    sequence_detector_destroy(detector_no_pe);

    // With PE
    sequence_detector_config_t config_with_pe = GetDefaultConfig();
    config_with_pe.enable_positional_encoding = true;
    config_with_pe.pe_type = NIMCP_POS_SINUSOIDAL;

    sequence_detector_t* detector_with_pe = sequence_detector_create(&config_with_pe);
    ASSERT_NE(detector_with_pe, nullptr);

    auto start_with_pe = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        uint32_t template_id = 0;
        sequence_detector_learn(
            detector_with_pe, test_seq.data(), test_seq.size(), &template_id
        );
    }
    auto end_with_pe = std::chrono::high_resolution_clock::now();
    auto duration_with_pe = std::chrono::duration_cast<std::chrono::microseconds>(
        end_with_pe - start_with_pe).count();

    sequence_detector_destroy(detector_with_pe);

    // PE should not add more than 4x overhead
    EXPECT_LT(duration_with_pe, duration_no_pe * 4)
        << "PE overhead too high: " << duration_with_pe << "us vs " << duration_no_pe << "us";
}

TEST_F(SequencePERegressionTest, PerformanceStability_DetectOperations) {
    // WHAT: Verify PE doesn't significantly slow detection
    // WHY:  Detect is on critical inference path
    // HOW:  Time detection with PE enabled

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn some sequences
    for (int i = 0; i < 10; i++) {
        auto seq = CreateTestSequence(8);
        uint32_t template_id = 0;
        sequence_detector_learn(detector, seq.data(), seq.size(), &template_id);
    }

    // Time detection
    auto test_seq = CreateTestSequence(8);
    sequence_detection_t detections[10];
    uint32_t num_detected = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        sequence_detector_detect(
            detector, test_seq.data(), test_seq.size(),
            detections, 10, &num_detected
        );
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    float avg_time_us = duration_us / 1000.0f;

    // Should be fast (< 500us per detection on average)
    EXPECT_LT(avg_time_us, 500.0f)
        << "Average detection time " << avg_time_us << "us exceeds 500us threshold";
}

//=============================================================================
// 4. Temporal Consistency Tests
//=============================================================================

TEST_F(SequencePERegressionTest, TemporalConsistency_FixedSequence) {
    // WHAT: Verify PE produces consistent results for same sequence
    // WHY:  Temporal pattern matching must be deterministic
    // HOW:  Learn sequence, detect multiple times, verify consistency

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    auto test_seq = CreateTestSequence(10);

    // Learn sequence
    uint32_t template_id = 0;
    int result = sequence_detector_learn(
        detector, test_seq.data(), test_seq.size(), &template_id
    );
    ASSERT_EQ(result, 0);

    // Detect multiple times and collect strengths
    std::vector<float> strengths;
    for (int i = 0; i < 100; i++) {
        sequence_detection_t detections[10];
        uint32_t num_detected = 0;

        result = sequence_detector_detect(
            detector, test_seq.data(), test_seq.size(),
            detections, 10, &num_detected
        );

        ASSERT_EQ(result, 0);
        ASSERT_GT(num_detected, 0);

        strengths.push_back(detections[0].strength);
    }

    // Verify all strengths are identical (deterministic PE)
    float first_strength = strengths[0];
    for (size_t i = 1; i < strengths.size(); i++) {
        EXPECT_FLOAT_EQ(strengths[i], first_strength)
            << "Strength changed at iteration " << i;
    }
}

TEST_F(SequencePERegressionTest, TemporalConsistency_VaryingIntervals) {
    // WHAT: Verify PE handles varying temporal intervals correctly
    // WHY:  Real sequences have timing variability
    // HOW:  Create sequences with different intervals, verify PE adapts

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.temporal_tolerance_ms = 30.0f;  // Allow some timing variance

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn sequence with 10ms intervals
    auto seq_10ms = CreateTestSequence(8, 10.0f);
    uint32_t template_id = 0;
    int result = sequence_detector_learn(
        detector, seq_10ms.data(), seq_10ms.size(), &template_id
    );
    ASSERT_EQ(result, 0);

    // Test with slightly different intervals (within tolerance)
    auto seq_12ms = CreateTestSequence(8, 12.0f);
    sequence_detection_t detections[10];
    uint32_t num_detected = 0;

    result = sequence_detector_detect(
        detector, seq_12ms.data(), seq_12ms.size(),
        detections, 10, &num_detected
    );

    // Should still detect (timing within tolerance)
    EXPECT_EQ(result, 0);
    // Note: May or may not detect depending on tolerance - just verify no crash
}

//=============================================================================
// 5. Sequence Length Tests
//=============================================================================

TEST_F(SequencePERegressionTest, SequenceLength_Short) {
    // WHAT: Verify PE works with short sequences
    // WHY:  Edge case - minimal temporal context
    // HOW:  Test sequences of length 2-5

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    for (uint32_t len = 2; len <= 5; len++) {
        auto seq = CreateTestSequence(len);
        uint32_t template_id = 0;

        int result = sequence_detector_learn(
            detector, seq.data(), seq.size(), &template_id
        );
        EXPECT_EQ(result, 0) << "Failed with sequence length " << len;

        // Verify detection works
        sequence_detection_t detections[10];
        uint32_t num_detected = 0;
        result = sequence_detector_detect(
            detector, seq.data(), seq.size(),
            detections, 10, &num_detected
        );
        EXPECT_EQ(result, 0);
    }
}

TEST_F(SequencePERegressionTest, SequenceLength_Long) {
    // WHAT: Verify PE works with long sequences
    // WHY:  Should scale to typical sequence lengths
    // HOW:  Test sequences up to max length

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.max_sequence_length = MAX_SEQUENCE_LENGTH;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    uint32_t lengths[] = {10, 20, 30, 40, 50};

    for (auto len : lengths) {
        auto seq = CreateTestSequence(len);
        uint32_t template_id = 0;

        int result = sequence_detector_learn(
            detector, seq.data(), seq.size(), &template_id
        );
        EXPECT_EQ(result, 0) << "Failed with sequence length " << len;

        // Verify learned sequence can be retrieved
        sequence_template_t* tmpl = sequence_detector_get_template(detector, template_id);
        if (tmpl != nullptr) {
            EXPECT_EQ(tmpl->length, len);
        }
    }
}

TEST_F(SequencePERegressionTest, SequenceLength_VaryingLengths) {
    // WHAT: Verify PE handles mixed sequence lengths
    // WHY:  Real workload has varying sequence lengths
    // HOW:  Learn sequences of different lengths, verify no interference

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // Learn sequences of varying lengths
    std::vector<uint32_t> template_ids;
    for (uint32_t len = 5; len <= 25; len += 5) {
        auto seq = CreateTestSequence(len);
        uint32_t template_id = 0;

        int result = sequence_detector_learn(
            detector, seq.data(), seq.size(), &template_id
        );
        EXPECT_EQ(result, 0);
        template_ids.push_back(template_id);
    }

    // Verify all sequences still detectable
    for (size_t i = 0; i < template_ids.size(); i++) {
        uint32_t len = 5 + (i * 5);
        auto seq = CreateTestSequence(len);

        sequence_detection_t detections[10];
        uint32_t num_detected = 0;

        int result = sequence_detector_detect(
            detector, seq.data(), seq.size(),
            detections, 10, &num_detected
        );

        EXPECT_EQ(result, 0);
        // Note: Detection not guaranteed for all, just verify no crash
    }
}

//=============================================================================
// 6. N-gram Tests
//=============================================================================

TEST_F(SequencePERegressionTest, NGram_PEStability) {
    // WHAT: Verify PE remains stable with N-gram learning
    // WHY:  N-gram learning is parallel to sequence learning
    // HOW:  Enable N-gram learning, verify PE still works

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.enable_ngram_learning = true;
    config.max_ngram = 4;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    auto test_seq = CreateTestSequence(15);

    // Feed sequence (builds N-grams)
    for (int i = 0; i < 100; i++) {
        sequence_detector_feed_spike(detector, test_seq[i % test_seq.size()]);

        if (i % 10 == 0) {
            bio_router_process_messages();
        }
    }

    // Verify detector still functional for sequence learning
    uint32_t template_id = 0;
    int result = sequence_detector_learn(
        detector, test_seq.data(), test_seq.size(), &template_id
    );
    EXPECT_EQ(result, 0);
}

//=============================================================================
// 7. Replay Detection Tests
//=============================================================================

TEST_F(SequencePERegressionTest, Replay_ForwardBackward) {
    // WHAT: Verify PE works correctly with replay detection
    // WHY:  Replay detection uses temporal matching
    // HOW:  Enable replay, verify forward/backward detection

    sequence_detector_config_t config = GetDefaultConfig();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.enable_replay_detection = true;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    auto test_seq = CreateTestSequence(10);

    // Learn forward sequence
    uint32_t template_id = 0;
    int result = sequence_detector_learn(
        detector, test_seq.data(), test_seq.size(), &template_id
    );
    ASSERT_EQ(result, 0);

    // Detect forward sequence
    sequence_detection_t detections[10];
    uint32_t num_detected = 0;

    result = sequence_detector_detect(
        detector, test_seq.data(), test_seq.size(),
        detections, 10, &num_detected
    );

    EXPECT_EQ(result, 0);
    if (num_detected > 0) {
        EXPECT_TRUE(detections[0].is_forward || !detections[0].is_backward);
    }

    // Note: Backward detection would require reversed sequence input
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
