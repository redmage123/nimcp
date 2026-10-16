//=============================================================================
// epistemic_filter_tests.cpp - Comprehensive Epistemic Filter Test Suite
//=============================================================================

#include <gtest/gtest.h>
#include "cognitive/epistemic/nimcp_epistemic_filter.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class EpistemicFilterTest : public ::testing::Test {
protected:
    epistemic_filter_t filter;

    void SetUp() override {
        filter = epistemic_filter_create(0.6f);  // Cautious skepticism
        ASSERT_NE(filter, nullptr);
    }

    void TearDown() override {
        if (filter) {
            epistemic_filter_destroy(filter);
        }
    }
};

//=============================================================================
// Unit Tests: Filter Creation
//=============================================================================

TEST(EpistemicFilterCreation, ValidSkepticismLevels) {
    epistemic_filter_t filter_low = epistemic_filter_create(0.0f);
    EXPECT_NE(filter_low, nullptr);
    epistemic_filter_destroy(filter_low);

    epistemic_filter_t filter_mid = epistemic_filter_create(0.5f);
    EXPECT_NE(filter_mid, nullptr);
    epistemic_filter_destroy(filter_mid);

    epistemic_filter_t filter_high = epistemic_filter_create(1.0f);
    EXPECT_NE(filter_high, nullptr);
    epistemic_filter_destroy(filter_high);
}

TEST(EpistemicFilterCreation, HandlesBoundarySkepticism) {
    // Should clamp values outside 0-1 range
    epistemic_filter_t filter_neg = epistemic_filter_create(-0.5f);
    EXPECT_NE(filter_neg, nullptr);
    epistemic_filter_destroy(filter_neg);

    epistemic_filter_t filter_high = epistemic_filter_create(1.5f);
    EXPECT_NE(filter_high, nullptr);
    epistemic_filter_destroy(filter_high);
}

TEST(EpistemicFilterCreation, DestroyNullIseSafe) {
    epistemic_filter_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Unit Tests: Sagan Standard
//=============================================================================

TEST(SaganStandard, OrdinaryClaimWeakEvidence) {
    float credibility = epistemic_apply_sagan_standard(
        PLAUSIBLE_LIKELY,      // Ordinary claim
        EVIDENCE_WEAK          // Weak evidence
    );
    EXPECT_GT(credibility, 0.3f);  // Should be somewhat credible
    // Note: ordinary claims are treated leniently even with weak evidence
}

TEST(SaganStandard, OrdinaryClaimStrongEvidence) {
    float credibility = epistemic_apply_sagan_standard(
        PLAUSIBLE_LIKELY,      // Ordinary claim
        EVIDENCE_STRONG        // Strong evidence
    );
    EXPECT_GT(credibility, 0.7f);  // Should be highly credible
}

TEST(SaganStandard, ExtraordinaryClaimWeakEvidence) {
    float credibility = epistemic_apply_sagan_standard(
        PLAUSIBLE_EXTRAORDINARY,  // Extraordinary claim
        EVIDENCE_WEAK             // Weak evidence
    );
    EXPECT_LT(credibility, 0.3f);  // Should be rejected
}

TEST(SaganStandard, ExtraordinaryClaimStrongEvidence) {
    float credibility = epistemic_apply_sagan_standard(
        PLAUSIBLE_EXTRAORDINARY,  // Extraordinary claim
        EVIDENCE_SCIENTIFIC       // Scientific evidence
    );
    EXPECT_GT(credibility, 0.3f);  // Should be more credible
}

TEST(SaganStandard, ImpossibleClaimAlwaysRejected) {
    float credibility = epistemic_apply_sagan_standard(
        PLAUSIBLE_IMPOSSIBLE,     // Impossible claim
        EVIDENCE_CONSENSUS        // Even with consensus
    );
    EXPECT_EQ(credibility, 0.0f);  // Should be completely rejected
}

//=============================================================================
// Unit Tests: Conspiracy Pattern Detection
//=============================================================================

TEST_F(EpistemicFilterTest, DetectsConspiracyNarratives) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.num_sources = 1;
    evidence.is_falsifiable = true;

    const char* conspiracy_text = "They don't want you to know the truth! Wake up sheeple!";
    float score = epistemic_check_conspiracy_pattern(filter, conspiracy_text, &evidence);

    EXPECT_GT(score, 0.3f);  // Should detect conspiracy pattern
}

TEST_F(EpistemicFilterTest, DetectsUnfalsifiableClaims) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.num_sources = 2;
    evidence.is_falsifiable = false;  // Unfalsifiable

    const char* text = "The simulation controls everything.";
    float score = epistemic_check_conspiracy_pattern(filter, text, &evidence);

    EXPECT_GT(score, 0.2f);  // Should penalize unfalsifiability
}

TEST_F(EpistemicFilterTest, NormalTextLowConspiracyScore) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.num_sources = 3;
    evidence.is_falsifiable = true;

    const char* normal_text = "The study found a correlation between X and Y.";
    float score = epistemic_check_conspiracy_pattern(filter, normal_text, &evidence);

    EXPECT_LT(score, 0.3f);  // Should have low conspiracy score
}

TEST_F(EpistemicFilterTest, MultipleConspiracyPatterns) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.num_sources = 1;
    evidence.is_falsifiable = false;

    const char* text = "They're hiding the truth! Do your own research! It's all connected!";
    float score = epistemic_check_conspiracy_pattern(filter, text, &evidence);

    EXPECT_GT(score, 0.6f);  // Should have high conspiracy score (multiple patterns)
}

//=============================================================================
// Unit Tests: Claim Assessment
//=============================================================================

TEST_F(EpistemicFilterTest, AssessWellEvidencedClaim) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_SCIENTIFIC;
    evidence.plausibility = PLAUSIBLE_LIKELY;
    evidence.num_sources = 5;
    evidence.source_reliability_avg = 0.9f;
    evidence.expert_consensus = 0.95f;
    evidence.is_falsifiable = true;
    evidence.has_contradictions = false;

    epistemic_assessment_t assessment;
    bool success = epistemic_assess_claim(
        filter,
        "Vaccines reduce disease transmission.",
        0.8f,  // High prior probability
        &evidence,
        &assessment
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(assessment.should_accept);
    EXPECT_GT(assessment.credibility_score, 0.7f);
    EXPECT_EQ(assessment.num_biases_detected, 0u);
}

TEST_F(EpistemicFilterTest, RejectsPoorlyEvidencedClaim) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_ANECDOTAL;
    evidence.plausibility = PLAUSIBLE_UNLIKELY;
    evidence.num_sources = 1;
    evidence.source_reliability_avg = 0.3f;
    evidence.expert_consensus = 0.2f;
    evidence.is_falsifiable = false;

    epistemic_assessment_t assessment;
    bool success = epistemic_assess_claim(
        filter,
        "Crystals cure cancer through quantum vibrations.",
        0.1f,  // Low prior probability
        &evidence,
        &assessment
    );

    EXPECT_TRUE(success);
    EXPECT_FALSE(assessment.should_accept);
    EXPECT_LT(assessment.credibility_score, 0.4f);
    EXPECT_TRUE(assessment.requires_verification);
}

TEST_F(EpistemicFilterTest, FlagsUncertainClaims) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_WEAK;  // Lower evidence quality
    evidence.plausibility = PLAUSIBLE_NEUTRAL;
    evidence.num_sources = 2;
    evidence.source_reliability_avg = 0.5f;  // Lower source reliability

    epistemic_assessment_t assessment;
    bool success = epistemic_assess_claim(
        filter,
        "New treatment shows promise in preliminary trials.",
        0.5f,
        &evidence,
        &assessment
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(assessment.requires_verification);
    EXPECT_GT(assessment.credibility_score, 0.3f);
}

TEST_F(EpistemicFilterTest, RejectsImpossibleClaims) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_STRONG;
    evidence.plausibility = PLAUSIBLE_IMPOSSIBLE;

    epistemic_assessment_t assessment;
    bool success = epistemic_assess_claim(
        filter,
        "Water has memory and responds to human emotions.",
        0.01f,
        &evidence,
        &assessment
    );

    EXPECT_TRUE(success);
    EXPECT_FALSE(assessment.should_accept);
    EXPECT_EQ(assessment.credibility_score, 0.0f);
}

//=============================================================================
// Unit Tests: Source Reliability
//=============================================================================

TEST_F(EpistemicFilterTest, TracksSourceReliability) {
    const char* source = "test_source_1";

    // Initially unknown
    float reliability = epistemic_get_source_reliability(filter, source);
    EXPECT_EQ(reliability, -1.0f);

    // Update with correct claim
    epistemic_update_source(filter, source, true);
    reliability = epistemic_get_source_reliability(filter, source);
    EXPECT_EQ(reliability, 1.0f);

    // Update with incorrect claim
    epistemic_update_source(filter, source, false);
    reliability = epistemic_get_source_reliability(filter, source);
    EXPECT_EQ(reliability, 0.5f);  // 1 correct, 1 incorrect
}

TEST_F(EpistemicFilterTest, MultipleSourceTracking) {
    epistemic_update_source(filter, "reliable_source", true);
    epistemic_update_source(filter, "reliable_source", true);
    epistemic_update_source(filter, "reliable_source", true);

    epistemic_update_source(filter, "unreliable_source", false);
    epistemic_update_source(filter, "unreliable_source", false);

    float reliable = epistemic_get_source_reliability(filter, "reliable_source");
    float unreliable = epistemic_get_source_reliability(filter, "unreliable_source");

    EXPECT_GT(reliable, 0.9f);
    EXPECT_LT(unreliable, 0.1f);
}

//=============================================================================
// Unit Tests: Bias Detection
//=============================================================================

TEST_F(EpistemicFilterTest, DetectsDunningKruger) {
    float reasoning_features[] = {
        0.95f,  // Very high confidence
        0.15f,  // Very low evidence quality
        0.5f,   // Neutral prior
        1.0f,   // Single source
        0.3f    // Low consensus
    };

    bias_detection_t biases[8];
    uint32_t count = epistemic_detect_biases(
        filter,
        reasoning_features,
        5,
        biases,
        8
    );

    EXPECT_GT(count, 0u);
    bool found_dk = false;
    for (uint32_t i = 0; i < count; i++) {
        if (biases[i].bias_type == BIAS_DUNNING_KRUGER) {
            found_dk = true;
            EXPECT_GT(biases[i].confidence, 0.5f);
        }
    }
    EXPECT_TRUE(found_dk);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EpistemicFilterTest, EndToEndWellReasonedClaim) {
    // Setup: Well-evidenced scientific claim
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_CONSENSUS;
    evidence.plausibility = PLAUSIBLE_ESTABLISHED;
    evidence.num_sources = 10;
    evidence.source_reliability_avg = 0.95f;
    evidence.expert_consensus = 0.98f;
    evidence.public_consensus = 0.85f;
    evidence.is_falsifiable = true;
    evidence.has_contradictions = false;
    evidence.has_primary_sources = true;

    epistemic_assessment_t assessment;
    bool success = epistemic_assess_claim(
        filter,
        "The Earth orbits the Sun in an elliptical path.",
        0.99f,
        &evidence,
        &assessment
    );

    // Expectations
    EXPECT_TRUE(success);
    EXPECT_TRUE(assessment.should_accept);
    EXPECT_GT(assessment.epistemic_quality, 0.8f);
    EXPECT_GT(assessment.credibility_score, 0.8f);
    EXPECT_EQ(assessment.num_biases_detected, 0u);
    EXPECT_FALSE(assessment.requires_verification);
    EXPECT_GT(assessment.logical_coherence, 0.9f);
}

TEST_F(EpistemicFilterTest, EndToEndConspiracyClaim) {
    // Setup: Conspiracy-style claim
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_ANECDOTAL;
    evidence.plausibility = PLAUSIBLE_EXTRAORDINARY;
    evidence.num_sources = 1;
    evidence.source_reliability_avg = 0.2f;
    evidence.expert_consensus = 0.05f;
    evidence.public_consensus = 0.3f;
    evidence.is_falsifiable = false;
    evidence.has_contradictions = true;

    const char* text = "The deep state is hiding the truth! "
                      "They don't want you to know! Wake up sheeple! "
                      "Do your own research!";

    epistemic_assessment_t assessment;
    bool success = epistemic_assess_claim(
        filter,
        text,
        0.05f,
        &evidence,
        &assessment
    );

    // Expectations
    EXPECT_TRUE(success);
    EXPECT_FALSE(assessment.should_accept);
    EXPECT_LT(assessment.epistemic_quality, 0.3f);
    EXPECT_LT(assessment.credibility_score, 0.3f);
    // Conspiracy patterns should be detected through the text analysis
    EXPECT_TRUE(assessment.requires_verification);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(EpistemicFilterTest, HandlesNullParameters) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    epistemic_assessment_t assessment;

    // Null filter
    EXPECT_FALSE(epistemic_assess_claim(nullptr, "claim", 0.5f, &evidence, &assessment));

    // Null claim text
    EXPECT_FALSE(epistemic_assess_claim(filter, nullptr, 0.5f, &evidence, &assessment));

    // Null evidence
    EXPECT_FALSE(epistemic_assess_claim(filter, "claim", 0.5f, nullptr, &assessment));

    // Null assessment
    EXPECT_FALSE(epistemic_assess_claim(filter, "claim", 0.5f, &evidence, nullptr));
}

TEST_F(EpistemicFilterTest, HandlesEmptyText) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    epistemic_assessment_t assessment;

    bool success = epistemic_assess_claim(filter, "", 0.5f, &evidence, &assessment);
    EXPECT_TRUE(success);  // Empty text should not crash
}

TEST_F(EpistemicFilterTest, HandlesVeryLongText) {
    std::string long_text(10000, 'a');  // 10KB of 'a'

    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    epistemic_assessment_t assessment;

    bool success = epistemic_assess_claim(filter, long_text.c_str(), 0.5f, &evidence, &assessment);
    EXPECT_TRUE(success);  // Should handle long text without crash
}

//=============================================================================
// Regression Tests
//=============================================================================

TEST_F(EpistemicFilterTest, ConsistentResults) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_MODERATE;
    evidence.plausibility = PLAUSIBLE_LIKELY;

    epistemic_assessment_t assessment1, assessment2;

    epistemic_assess_claim(filter, "Test claim", 0.5f, &evidence, &assessment1);
    epistemic_assess_claim(filter, "Test claim", 0.5f, &evidence, &assessment2);

    // Should produce identical results
    EXPECT_FLOAT_EQ(assessment1.credibility_score, assessment2.credibility_score);
    EXPECT_FLOAT_EQ(assessment1.epistemic_quality, assessment2.epistemic_quality);
    EXPECT_EQ(assessment1.should_accept, assessment2.should_accept);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(EpistemicFilterTest, PerformanceUnder1ms) {
    claim_evidence_t evidence;
    epistemic_evidence_init(&evidence);
    evidence.evidence_quality = EVIDENCE_MODERATE;
    epistemic_assessment_t assessment;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        epistemic_assess_claim(filter, "Test claim", 0.5f, &evidence, &assessment);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete 1000 assessments in reasonable time (< 20ms)
    EXPECT_LT(duration.count(), 20000);  // 20ms for 1000 iterations = 20μs each
}

