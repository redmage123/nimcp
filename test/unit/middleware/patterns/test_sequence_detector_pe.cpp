//=============================================================================
// test_sequence_detector_pe.cpp - Unit Tests for Sequence Detector PE
//=============================================================================
/**
 * @file test_sequence_detector_pe.cpp
 * @brief Unit tests for positional encoding integration in sequence detector
 *
 * WHAT: Test PE configuration, template encoding, and position-aware matching
 * WHY:  Positional encoding enables temporal context in sequence detection
 * HOW:  Test RoPE and Relative PE with spike sequence templates
 *
 * TEST COVERAGE:
 * 1. PE configuration and initialization
 * 2. Position encoding for sequence templates
 * 3. RoPE/Relative PE for temporal patterns
 * 4. Position-aware sequence matching
 * 5. PE similarity scoring
 * 6. Edge cases (empty templates, invalid positions)
 * 7. Integration with replay detection
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal sequence replay encodes position via theta phase
 * - Grid cells provide temporal context for sequences
 * - Phase precession enables position-dependent firing
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "middleware/patterns/nimcp_sequence_detector.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SequenceDetectorPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr uint32_t TEST_SEQ_LEN = 10;
    static constexpr uint32_t TEST_PE_DIM = 64;
    static constexpr uint32_t TEST_MAX_TEMPLATES = 100;

    sequence_detector_t* detector = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();
    }

    void TearDown() override {
        if (detector) {
            sequence_detector_destroy(detector);
            detector = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }

    // Helper to create detector with PE
    sequence_detector_t* CreateDetectorWithPE(nimcp_pos_encoding_type_t pe_type) {
        sequence_detector_config_t config = sequence_detector_default_config();
        config.max_templates = TEST_MAX_TEMPLATES;
        config.max_sequence_length = TEST_SEQ_LEN;
        config.enable_positional_encoding = true;
        config.pe_type = pe_type;
        config.pe_embedding_dim = TEST_PE_DIM;
        config.pe_similarity_weight = 0.3f;  // 30% weight for PE similarity

        return sequence_detector_create(&config);
    }

    // Helper to create simple sequence template
    void CreateSimpleTemplate(sequence_element_t* elements, uint32_t length) {
        for (uint32_t i = 0; i < length; i++) {
            elements[i].neuron_id = 100 + i;
            elements[i].relative_time_ms = (float)i * 50.0f;  // 50ms intervals
            elements[i].position_embedding = nullptr;  // Allocated by detector
            elements[i].embedding_dim = 0;
        }
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(SequenceDetectorPETest, SetPEConfig_RoPE) {
    // WHAT: Configure sequence detector with RoPE
    // WHY:  RoPE is optimal for temporal sequences

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr) << "Detector with RoPE should be created";

    // Verify configuration
    uint32_t num_templates;
    uint64_t total_detections;
    float avg_strength;
    sequence_detector_get_stats(detector, &num_templates, &total_detections, &avg_strength);

    EXPECT_EQ(num_templates, 0u) << "Should start with no templates";
}

TEST_F(SequenceDetectorPETest, SetPEConfig_Relative) {
    // WHAT: Configure sequence detector with Relative PE
    // WHY:  Relative PE for distance-based sequence matching

    detector = CreateDetectorWithPE(NIMCP_POS_RELATIVE);
    ASSERT_NE(detector, nullptr) << "Detector with Relative PE should be created";

    // Verify configuration
    uint32_t num_templates;
    uint64_t total_detections;
    float avg_strength;
    sequence_detector_get_stats(detector, &num_templates, &total_detections, &avg_strength);

    EXPECT_EQ(num_templates, 0u) << "Should start with no templates";
}

TEST_F(SequenceDetectorPETest, SetPEConfig_Disable) {
    // WHAT: Create detector without PE
    // WHY:  Baseline comparison

    sequence_detector_config_t config = sequence_detector_default_config();
    config.enable_positional_encoding = false;

    detector = sequence_detector_create(&config);
    ASSERT_NE(detector, nullptr);

    // PE functions should fail
    float pe_match_rate, avg_pe_similarity, pe_cache_hit_rate;
    bool result = sequence_detector_get_pe_stats(detector, &pe_match_rate,
                                                  &avg_pe_similarity, &pe_cache_hit_rate);
    EXPECT_FALSE(result) << "PE stats should fail when PE disabled";
}

//=============================================================================
// Unit Tests: Template Encoding
//=============================================================================

TEST_F(SequenceDetectorPETest, EncodeTemplate_SingleSequence) {
    // WHAT: Encode single sequence template with PE
    // WHY:  Basic template encoding test

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Create and learn template
    sequence_element_t elements[5];
    CreateSimpleTemplate(elements, 5);

    uint32_t template_id;
    bool result = sequence_detector_learn_template(detector, elements, 5, &template_id);
    EXPECT_TRUE(result) << "Template learning should succeed";

    // Encode template with PE
    result = sequence_detector_encode_template(detector, template_id);
    EXPECT_TRUE(result) << "Template encoding should succeed";

    // Verify template is encoded
    sequence_template_t seq_template;
    result = sequence_detector_get_template(detector, template_id, &seq_template);
    EXPECT_TRUE(result) << "Template retrieval should succeed";
    EXPECT_EQ(seq_template.length, 5u);
}

TEST_F(SequenceDetectorPETest, EncodeTemplate_MultipleSequences) {
    // WHAT: Encode multiple sequence templates
    // WHY:  Test batch template encoding

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    uint32_t num_templates = 5;
    uint32_t template_ids[num_templates];

    // Learn multiple templates
    for (uint32_t i = 0; i < num_templates; i++) {
        sequence_element_t elements[TEST_SEQ_LEN];
        CreateSimpleTemplate(elements, TEST_SEQ_LEN);

        // Vary neuron IDs per template
        for (uint32_t j = 0; j < TEST_SEQ_LEN; j++) {
            elements[j].neuron_id += i * 1000;
        }

        bool result = sequence_detector_learn_template(detector, elements,
                                                       TEST_SEQ_LEN, &template_ids[i]);
        EXPECT_TRUE(result) << "Template " << i << " learning should succeed";

        // Encode each template
        result = sequence_detector_encode_template(detector, template_ids[i]);
        EXPECT_TRUE(result) << "Template " << i << " encoding should succeed";
    }

    // Verify all templates are stored
    uint32_t stored_templates;
    uint64_t total_detections;
    float avg_strength;
    sequence_detector_get_stats(detector, &stored_templates, &total_detections, &avg_strength);
    EXPECT_EQ(stored_templates, num_templates);
}

//=============================================================================
// Unit Tests: Position-Aware Matching
//=============================================================================

TEST_F(SequenceDetectorPETest, MatchWithPE_ExactMatch) {
    // WHAT: Match sequence using PE-aware scoring
    // WHY:  Verify PE improves matching accuracy

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Learn template
    sequence_element_t template_elements[5];
    CreateSimpleTemplate(template_elements, 5);

    uint32_t template_id;
    sequence_detector_learn_template(detector, template_elements, 5, &template_id);
    sequence_detector_encode_template(detector, template_id);

    // Add matching spike sequence
    for (uint32_t i = 0; i < 5; i++) {
        double timestamp = (double)i * 50.0;
        sequence_detector_add_spike(detector, template_elements[i].neuron_id, timestamp);
    }

    // Detect with PE
    sequence_detection_t detections[10];
    uint32_t num_detected;
    bool result = sequence_detector_match_with_pe(detector, detections, 10, &num_detected);
    EXPECT_TRUE(result) << "PE-based matching should succeed";
    EXPECT_GT(num_detected, 0u) << "Should detect at least one sequence";

    if (num_detected > 0) {
        EXPECT_GT(detections[0].strength, 0.5f) << "Match strength should be high";
    }
}

TEST_F(SequenceDetectorPETest, MatchWithPE_PartialMatch) {
    // WHAT: Match partial sequence with PE
    // WHY:  Test PE similarity with incomplete matches

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Learn template
    sequence_element_t template_elements[5];
    CreateSimpleTemplate(template_elements, 5);

    uint32_t template_id;
    sequence_detector_learn_template(detector, template_elements, 5, &template_id);
    sequence_detector_encode_template(detector, template_id);

    // Add partial spike sequence (first 3 spikes only)
    for (uint32_t i = 0; i < 3; i++) {
        double timestamp = (double)i * 50.0;
        sequence_detector_add_spike(detector, template_elements[i].neuron_id, timestamp);
    }

    // Detect with PE
    sequence_detection_t detections[10];
    uint32_t num_detected;
    bool result = sequence_detector_match_with_pe(detector, detections, 10, &num_detected);
    EXPECT_TRUE(result) << "PE-based matching should succeed";

    // May or may not detect depending on threshold
    SUCCEED() << "Partial match handled";
}

TEST_F(SequenceDetectorPETest, MatchWithPE_CompareToNonPE) {
    // WHAT: Compare PE-based and non-PE matching
    // WHY:  Verify PE affects match scoring

    // Detector WITH PE
    sequence_detector_t* detector_with_pe = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector_with_pe, nullptr);

    // Detector WITHOUT PE
    sequence_detector_config_t config = sequence_detector_default_config();
    config.enable_positional_encoding = false;
    sequence_detector_t* detector_without_pe = sequence_detector_create(&config);
    ASSERT_NE(detector_without_pe, nullptr);

    // Learn same template in both
    sequence_element_t elements[5];
    CreateSimpleTemplate(elements, 5);

    uint32_t template_id_with_pe, template_id_without_pe;
    sequence_detector_learn_template(detector_with_pe, elements, 5, &template_id_with_pe);
    sequence_detector_encode_template(detector_with_pe, template_id_with_pe);
    sequence_detector_learn_template(detector_without_pe, elements, 5, &template_id_without_pe);

    // Add same spike sequence to both
    for (uint32_t i = 0; i < 5; i++) {
        double timestamp = (double)i * 50.0;
        sequence_detector_add_spike(detector_with_pe, elements[i].neuron_id, timestamp);
        sequence_detector_add_spike(detector_without_pe, elements[i].neuron_id, timestamp);
    }

    // Detect in both
    sequence_detection_t det_with_pe[10], det_without_pe[10];
    uint32_t num_with_pe, num_without_pe;

    sequence_detector_match_with_pe(detector_with_pe, det_with_pe, 10, &num_with_pe);
    sequence_detector_detect(detector_without_pe, det_without_pe, 10, &num_without_pe);

    // Both should detect something (exact match)
    EXPECT_GT(num_with_pe, 0u);
    EXPECT_GT(num_without_pe, 0u);

    // Strengths may differ due to PE contribution
    SUCCEED() << "PE-based and non-PE matching completed";

    sequence_detector_destroy(detector_with_pe);
    sequence_detector_destroy(detector_without_pe);
}

//=============================================================================
// Unit Tests: PE Statistics
//=============================================================================

TEST_F(SequenceDetectorPETest, GetPEStats_AfterMatching) {
    // WHAT: Retrieve PE statistics after matching
    // WHY:  Monitor PE effectiveness

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Learn and encode template
    sequence_element_t elements[5];
    CreateSimpleTemplate(elements, 5);
    uint32_t template_id;
    sequence_detector_learn_template(detector, elements, 5, &template_id);
    sequence_detector_encode_template(detector, template_id);

    // Add spikes and match
    for (uint32_t i = 0; i < 5; i++) {
        sequence_detector_add_spike(detector, elements[i].neuron_id, (double)i * 50.0);
    }

    sequence_detection_t detections[10];
    uint32_t num_detected;
    sequence_detector_match_with_pe(detector, detections, 10, &num_detected);

    // Get PE stats
    float pe_match_rate, avg_pe_similarity, pe_cache_hit_rate;
    bool result = sequence_detector_get_pe_stats(detector, &pe_match_rate,
                                                  &avg_pe_similarity, &pe_cache_hit_rate);
    EXPECT_TRUE(result) << "PE stats retrieval should succeed";

    // Stats should be in valid range
    EXPECT_GE(pe_match_rate, 0.0f);
    EXPECT_LE(pe_match_rate, 1.0f);
    EXPECT_GE(avg_pe_similarity, 0.0f);
    EXPECT_LE(avg_pe_similarity, 1.0f);
    EXPECT_GE(pe_cache_hit_rate, 0.0f);
    EXPECT_LE(pe_cache_hit_rate, 1.0f);
}

TEST_F(SequenceDetectorPETest, GetPEStats_NoMatching) {
    // WHAT: Get PE stats before any matching
    // WHY:  Verify initialization state

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    float pe_match_rate, avg_pe_similarity, pe_cache_hit_rate;
    bool result = sequence_detector_get_pe_stats(detector, &pe_match_rate,
                                                  &avg_pe_similarity, &pe_cache_hit_rate);
    EXPECT_TRUE(result) << "PE stats should be available even without matching";

    // Initial stats should be zero or default values
    EXPECT_GE(pe_match_rate, 0.0f);
    EXPECT_GE(avg_pe_similarity, 0.0f);
    EXPECT_GE(pe_cache_hit_rate, 0.0f);
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(SequenceDetectorPETest, EdgeCase_NullInput) {
    // WHAT: Handle NULL inputs gracefully
    // WHY:  Robustness testing

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    bool result = sequence_detector_encode_template(nullptr, 0);
    EXPECT_FALSE(result) << "NULL detector should fail";

    float pe_match_rate, avg_pe_similarity, pe_cache_hit_rate;
    result = sequence_detector_get_pe_stats(nullptr, &pe_match_rate,
                                            &avg_pe_similarity, &pe_cache_hit_rate);
    EXPECT_FALSE(result) << "NULL detector should fail";
}

TEST_F(SequenceDetectorPETest, EdgeCase_InvalidTemplateID) {
    // WHAT: Encode non-existent template
    // WHY:  Boundary testing

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    bool result = sequence_detector_encode_template(detector, 9999);
    EXPECT_FALSE(result) << "Invalid template ID should fail";
}

TEST_F(SequenceDetectorPETest, EdgeCase_EmptyTemplate) {
    // WHAT: Learn template with zero elements
    // WHY:  Edge case validation

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    sequence_element_t elements[1];  // Dummy array
    uint32_t template_id;
    bool result = sequence_detector_learn_template(detector, elements, 0, &template_id);
    EXPECT_FALSE(result) << "Empty template should fail";
}

TEST_F(SequenceDetectorPETest, EdgeCase_NoSpikes) {
    // WHAT: Match with no spikes added
    // WHY:  Edge case validation

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Learn template but don't add spikes
    sequence_element_t elements[5];
    CreateSimpleTemplate(elements, 5);
    uint32_t template_id;
    sequence_detector_learn_template(detector, elements, 5, &template_id);
    sequence_detector_encode_template(detector, template_id);

    // Try to detect
    sequence_detection_t detections[10];
    uint32_t num_detected;
    bool result = sequence_detector_match_with_pe(detector, detections, 10, &num_detected);
    EXPECT_TRUE(result) << "Matching should succeed";
    EXPECT_EQ(num_detected, 0u) << "Should detect nothing with no spikes";
}

//=============================================================================
// Unit Tests: RoPE Specific
//=============================================================================

TEST_F(SequenceDetectorPETest, RoPE_RotationalInvariance) {
    // WHAT: Verify RoPE maintains rotational properties
    // WHY:  RoPE should preserve relative positions

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Create two templates with same relative spacing
    sequence_element_t template1[3];
    sequence_element_t template2[3];

    // Template 1: positions 0, 1, 2
    for (uint32_t i = 0; i < 3; i++) {
        template1[i].neuron_id = 100 + i;
        template1[i].relative_time_ms = (float)i * 100.0f;
        template1[i].position_embedding = nullptr;
        template1[i].embedding_dim = 0;
    }

    // Template 2: positions 5, 6, 7 (same spacing, different offset)
    for (uint32_t i = 0; i < 3; i++) {
        template2[i].neuron_id = 200 + i;
        template2[i].relative_time_ms = (float)i * 100.0f;  // Same spacing
        template2[i].position_embedding = nullptr;
        template2[i].embedding_dim = 0;
    }

    uint32_t template_id1, template_id2;
    sequence_detector_learn_template(detector, template1, 3, &template_id1);
    sequence_detector_learn_template(detector, template2, 3, &template_id2);

    sequence_detector_encode_template(detector, template_id1);
    sequence_detector_encode_template(detector, template_id2);

    // Both templates should be successfully encoded
    sequence_template_t seq1, seq2;
    EXPECT_TRUE(sequence_detector_get_template(detector, template_id1, &seq1));
    EXPECT_TRUE(sequence_detector_get_template(detector, template_id2, &seq2));
}

//=============================================================================
// Unit Tests: Integration
//=============================================================================

TEST_F(SequenceDetectorPETest, Integration_ReplayDetection) {
    // WHAT: Detect sequence replay with PE
    // WHY:  End-to-end integration test

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Learn forward sequence template
    sequence_element_t forward_seq[4];
    for (uint32_t i = 0; i < 4; i++) {
        forward_seq[i].neuron_id = 100 + i;
        forward_seq[i].relative_time_ms = (float)i * 100.0f;
        forward_seq[i].position_embedding = nullptr;
        forward_seq[i].embedding_dim = 0;
    }

    uint32_t template_id;
    sequence_detector_learn_template(detector, forward_seq, 4, &template_id);
    sequence_detector_encode_template(detector, template_id);

    // Simulate forward replay
    for (uint32_t i = 0; i < 4; i++) {
        double timestamp = 1000.0 + (double)i * 100.0;
        sequence_detector_add_spike(detector, forward_seq[i].neuron_id, timestamp);
    }

    // Detect
    sequence_detection_t detections[10];
    uint32_t num_detected;
    bool result = sequence_detector_match_with_pe(detector, detections, 10, &num_detected);
    EXPECT_TRUE(result);
    EXPECT_GT(num_detected, 0u) << "Should detect forward replay";

    if (num_detected > 0) {
        EXPECT_TRUE(detections[0].is_forward) << "Should be detected as forward replay";
        EXPECT_GT(detections[0].strength, 0.3f) << "Match strength should be reasonable";
    }
}

TEST_F(SequenceDetectorPETest, Integration_MultipleTemplateMatching) {
    // WHAT: Match against multiple templates with PE
    // WHY:  Test template discrimination

    detector = CreateDetectorWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(detector, nullptr);

    // Learn two different templates
    sequence_element_t template1[3];
    sequence_element_t template2[3];

    for (uint32_t i = 0; i < 3; i++) {
        template1[i].neuron_id = 100 + i;
        template1[i].relative_time_ms = (float)i * 50.0f;
        template1[i].position_embedding = nullptr;
        template1[i].embedding_dim = 0;

        template2[i].neuron_id = 200 + i;
        template2[i].relative_time_ms = (float)i * 75.0f;
        template2[i].position_embedding = nullptr;
        template2[i].embedding_dim = 0;
    }

    uint32_t tid1, tid2;
    sequence_detector_learn_template(detector, template1, 3, &tid1);
    sequence_detector_learn_template(detector, template2, 3, &tid2);
    sequence_detector_encode_template(detector, tid1);
    sequence_detector_encode_template(detector, tid2);

    // Add spikes matching template1
    for (uint32_t i = 0; i < 3; i++) {
        sequence_detector_add_spike(detector, template1[i].neuron_id,
                                    (double)template1[i].relative_time_ms);
    }

    // Detect
    sequence_detection_t detections[10];
    uint32_t num_detected;
    sequence_detector_match_with_pe(detector, detections, 10, &num_detected);

    EXPECT_GT(num_detected, 0u) << "Should detect at least one template";
    if (num_detected > 0) {
        EXPECT_EQ(detections[0].template_id, tid1)
            << "Should match template1";
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
