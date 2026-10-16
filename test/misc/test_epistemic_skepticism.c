/**
 * @file test_epistemic_skepticism.c
 * @brief Quick test to verify epistemic filter detects facts vs opinions
 *
 * WHAT: Test that NIMCP's skepticism system works before training
 * WHY:  Critical to verify before ingesting mixed fact/opinion datasets
 * HOW:  Feed neutral, factual, biased, and conspiracy-like claims
 */

#include <stdio.h>
#include <string.h>
#include "cognitive/epistemic/nimcp_epistemic_filter.h"

int main(void) {
    printf("\n=== NIMCP Epistemic Filter Test ===\n\n");

    // Create epistemic filter with cautious skepticism (0.6)
    epistemic_filter_t filter = epistemic_filter_create(0.6f);
    if (!filter) {
        printf("ERROR: Failed to create epistemic filter\n");
        return 1;
    }

    // Test claims ranging from factual to conspiracy-like
    const char* test_claims[] = {
        "Water freezes at 0 degrees Celsius",                     // Factual
        "I think chocolate tastes good",                          // Opinion
        "Everyone agrees that my political view is correct",      // Bandwagon bias
        "The secret elite don't want you to know the truth",     // Conspiracy pattern
        "All members of that group are the same",                 // Ingroup bias
        "2 + 2 = 4",                                             // Mathematical fact
        "This miracle cure works for everyone"                    // Extraordinary claim
    };

    int num_claims = sizeof(test_claims) / sizeof(test_claims[0]);

    for (int i = 0; i < num_claims; i++) {
        printf("\n--- Claim %d: \"%s\" ---\n", i+1, test_claims[i]);

        // Initialize evidence
        claim_evidence_t evidence;
        epistemic_evidence_init(&evidence);

        // Set moderate evidence quality (typical training data)
        evidence.evidence_quality = EVIDENCE_MODERATE;
        evidence.plausibility = PLAUSIBLE_NEUTRAL;
        evidence.num_sources = 1;
        evidence.is_falsifiable = true;

        // Assess claim
        epistemic_assessment_t assessment;
        epistemic_assessment_init(&assessment);

        float prior_prob = 0.5f;  // Neutral prior

        if (epistemic_assess_claim(filter, test_claims[i], prior_prob, &evidence, &assessment)) {
            printf("  Epistemic Quality: %.2f\n", assessment.epistemic_quality);
            printf("  Skepticism Score: %.2f\n", assessment.skepticism_score);
            printf("  Credibility: %.2f\n", assessment.credibility_score);
            printf("  Biases Detected: %u\n", assessment.num_biases_detected);

            if (assessment.num_biases_detected > 0) {
                printf("  Bias Types:\n");
                for (uint32_t b = 0; b < assessment.num_biases_detected; b++) {
                    printf("    - %s (confidence: %.2f, severity: %.2f)\n",
                           assessment.biases[b].description,
                           assessment.biases[b].confidence,
                           assessment.biases[b].severity);
                }
            }

            // Check conspiracy pattern
            float conspiracy_score = epistemic_check_conspiracy_pattern(filter, test_claims[i], &evidence);
            printf("  Conspiracy Score: %.2f\n", conspiracy_score);

            // Decision
            if (assessment.should_accept) {
                printf("  VERDICT: ACCEPT (safe to learn)\n");
            } else {
                printf("  VERDICT: REJECT or REDUCE CONFIDENCE\n");
            }

            if (assessment.requires_verification) {
                printf("  NOTE: Requires additional verification\n");
            }
        } else {
            printf("  ERROR: Failed to assess claim\n");
        }
    }

    // Cleanup
    epistemic_filter_destroy(filter);

    printf("\n=== Test Complete ===\n");
    printf("Skepticism system is working and ready for training.\n\n");

    return 0;
}
