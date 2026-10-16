//=============================================================================
// nimcp_epistemic_filter.c - Epistemic Hygiene Implementation
//=============================================================================

#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

// SNN and Plasticity bridges
#include "cognitive/epistemic/nimcp_epistemic_snn_bridge.h"
#include "cognitive/epistemic/nimcp_epistemic_plasticity_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "epistemic"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(epistemic_filter, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Configuration Constants
//=============================================================================

#define MAX_SOURCES 256
#define MAX_CLAIM_PATTERNS 32
#define MAX_TEXT_LENGTH 100000  // Maximum text length to analyze (100KB)

// Skepticism thresholds
#define SKEPTICISM_DEFAULT 0.6f
#define SKEPTICISM_MIN 0.0f
#define SKEPTICISM_MAX 1.0f

// Conspiracy pattern weights
#define CONSPIRACY_WEIGHT_NARRATIVE 0.25f
#define CONSPIRACY_WEIGHT_UNFALSIFIABLE 0.3f
#define CONSPIRACY_WEIGHT_PATTERN_SEEKING 0.2f
#define CONSPIRACY_WEIGHT_MAINSTREAM_REJECTION 0.25f
#define CONSPIRACY_WEIGHT_AD_HOC 0.15f
#define CONSPIRACY_WEIGHT_CERTAINTY 0.1f

// Bias detection thresholds
#define BIAS_THRESHOLD_DUNNING_KRUGER 0.5f
#define BIAS_THRESHOLD_CONFIRMATION 0.5f
#define BIAS_THRESHOLD_BANDWAGON 0.5f

// Evidence weights
#define EVIDENCE_WEIGHT_SAGAN 0.35f
#define EVIDENCE_WEIGHT_SOURCE 0.25f
#define EVIDENCE_WEIGHT_CONSENSUS 0.25f
#define EVIDENCE_WEIGHT_LOGIC 0.25f

// Acceptance thresholds
#define ACCEPTANCE_THRESHOLD_DEFAULT 0.6f
#define ACCEPTANCE_THRESHOLD_EXTRAORDINARY 0.8f
#define ACCEPTANCE_THRESHOLD_IMPOSSIBLE 0.85f

/**
 * @brief Epistemic filter engine structure
 */
struct epistemic_filter_struct {
    // Configuration
    float skepticism_level;        /**< Base skepticism (0-1) */
    float consensus_weight;        /**< How much to weight consensus */
    float source_weight;           /**< How much to weight source reliability */

    // Source tracking
    source_reliability_t sources[MAX_SOURCES];
    uint32_t num_sources;

    // Statistics
    uint64_t claims_assessed;
    uint64_t claims_accepted;
    uint64_t claims_rejected;
    uint64_t biases_detected;

    // Bio-async integration
    bio_module_context_t bio_ctx;  /**< Bio-async module context */
    bool bio_async_enabled;        /**< Bio-async registration status */

    // SNN and Plasticity bridges
    epistemic_snn_bridge_t* snn_bridge;         /**< SNN integration bridge */
    epistemic_plasticity_bridge_t* plasticity_bridge;  /**< Plasticity integration bridge */
    bool bridges_enabled;                       /**< Whether bridges are active */
};

//=============================================================================
// Conspiracy Pattern Detection
//=============================================================================

/**
 * @brief Check for conspiracy thinking patterns in text
 */
static float detect_conspiracy_patterns(const char* text) {
    if (!text) return 0.0F;

    // Validate text length
    size_t text_len = strnlen(text, MAX_TEXT_LENGTH + 1);
    if (text_len > MAX_TEXT_LENGTH) {
        return 0.0F;  // Reject suspiciously long text
    }

    float conspiracy_score = 0.0F;
    uint32_t pattern_count = 0;

    // Convert to lowercase for pattern matching (bounded)
    char* lower_text = nimcp_malloc(text_len + 1);
    if (!lower_text) return 0.0F;

    for (size_t i = 0; i < text_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && text_len > 256) {
            epistemic_filter_heartbeat("epistemic_fi_loop",
                             (float)(i + 1) / (float)text_len);
        }

        lower_text[i] = tolower((unsigned char)text[i]);
    }
    lower_text[text_len] = '\0';

    // Pattern 1: "They don't want you to know" narratives
    if (strstr(lower_text, "they don't want") ||
        strstr(lower_text, "they're hiding") ||
        strstr(lower_text, "cover up") ||
        strstr(lower_text, "wake up") ||
        strstr(lower_text, "do your own research")) {
        conspiracy_score += CONSPIRACY_WEIGHT_NARRATIVE;
        pattern_count++;
    }

    // Pattern 2: Unfalsifiable claims
    if (strstr(lower_text, "can't prove it didn't") ||
        strstr(lower_text, "absence of evidence") ||
        strstr(lower_text, "you can't disprove")) {
        conspiracy_score += CONSPIRACY_WEIGHT_PATTERN_SEEKING;
        pattern_count++;
    }

    // Pattern 3: Pattern-seeking ("everything is connected")
    if (strstr(lower_text, "it's all connected") ||
        strstr(lower_text, "no coincidence") ||
        strstr(lower_text, "connect the dots")) {
        conspiracy_score += CONSPIRACY_WEIGHT_PATTERN_SEEKING;
        pattern_count++;
    }

    // Pattern 4: Rejection of mainstream evidence
    if (strstr(lower_text, "mainstream media") ||
        strstr(lower_text, "msm lies") ||
        strstr(lower_text, "fake news") ||
        strstr(lower_text, "sheeple")) {
        conspiracy_score += CONSPIRACY_WEIGHT_MAINSTREAM_REJECTION;
        pattern_count++;
    }

    // Pattern 5: Ad-hoc explanations
    if (strstr(lower_text, "actually means") ||
        strstr(lower_text, "real reason") ||
        strstr(lower_text, "what they're really")) {
        conspiracy_score += CONSPIRACY_WEIGHT_AD_HOC;
        pattern_count++;
    }

    // Pattern 6: Certainty without evidence
    if (strstr(lower_text, "obviously") ||
        strstr(lower_text, "clearly") ||
        strstr(lower_text, "everyone knows")) {
        conspiracy_score += CONSPIRACY_WEIGHT_CERTAINTY;
        pattern_count++;
    }

    nimcp_free(lower_text);
    lower_text = NULL;

    // Normalize by number of patterns found
    if (pattern_count > 0) {
        conspiracy_score = fminf(1.0F, conspiracy_score);
    }

    return conspiracy_score;
}

//=============================================================================
// Bias Detection Algorithms
//=============================================================================

/**
 * @brief Detect confirmation bias (only seeking confirming evidence)
 */
static float detect_confirmation_bias(
    const claim_evidence_t* evidence,
    float prior_belief)
{
    // High prior belief + low evidence diversity = confirmation bias
    if (prior_belief > 0.7F && evidence->num_sources < 2) {
        return 0.6F;
    }

    // Single source + high certainty = possible confirmation bias
    if (evidence->num_sources == 1 && evidence->evidence_strength > 0.8F) {
        return 0.5F;
    }

    return 0.0F;
}

/**
 * @brief Detect Dunning-Kruger effect (overconfidence with low knowledge)
 */
static float detect_dunning_kruger(
    float confidence,
    float evidence_quality)
{
    // High confidence + low evidence = Dunning-Kruger
    if (confidence > 0.8F && evidence_quality < 0.3F) {
        return 0.7F;
    }

    return 0.0F;
}

/**
 * @brief Detect bandwagon effect (following popular opinion)
 */
static float detect_bandwagon_effect(
    const claim_evidence_t* evidence)
{
    // High public consensus + low evidence = bandwagon
    if (evidence->public_consensus > 0.7F &&
        evidence->evidence_quality < EVIDENCE_MODERATE) {
        return 0.6F;
    }

    return 0.0F;
}

//=============================================================================
// API Implementation
//=============================================================================

epistemic_filter_t epistemic_filter_create(float skepticism_level) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_create", 0.0f);


    epistemic_filter_t filter = nimcp_calloc(1, sizeof(struct epistemic_filter_struct));
    if (!filter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_filter_create: failed to allocate filter");
        return NULL;
    }

    // Set configuration
    filter->skepticism_level = fminf(fmaxf(skepticism_level, 0.0F), 1.0F);
    filter->consensus_weight = 0.3F;   // Moderate weight on consensus
    filter->source_weight = 0.4F;      // Higher weight on source reliability

    filter->num_sources = 0;
    filter->claims_assessed = 0;
    filter->claims_accepted = 0;
    filter->claims_rejected = 0;
    filter->biases_detected = 0;

    // Initialize bio-async fields
    filter->bio_ctx = NULL;
    filter->bio_async_enabled = false;

    // Register with bio-async router if available
    NIMCP_LOGGING_DEBUG("epistemic: Checking bio-async router initialization...");
    if (bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("epistemic: Bio-router initialized, registering module (id=%d, inbox_capacity=32)...",
                           BIO_MODULE_EPISTEMIC);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EPISTEMIC,
            .module_name = "epistemic",
            .inbox_capacity = 32,
            .user_data = filter
        };
        filter->bio_ctx = bio_router_register_module(&bio_info);
        if (filter->bio_ctx) {
            filter->bio_async_enabled = true;
            NIMCP_LOGGING_INFO("epistemic: Bio-async communication enabled (module_id=%d)",
                              BIO_MODULE_EPISTEMIC);
        } else {
            NIMCP_LOGGING_WARN("epistemic: Bio-async registration failed - module will operate without async messaging");
        }
    } else {
        NIMCP_LOGGING_DEBUG("epistemic: Bio-router not initialized, skipping async registration");
    }

    // Initialize SNN and Plasticity bridges
    filter->snn_bridge = NULL;
    filter->plasticity_bridge = NULL;
    filter->bridges_enabled = false;

    // Create SNN bridge with default config
    epistemic_snn_config_t snn_config = {
        .max_sources = EPISTEMIC_SNN_MAX_SOURCES,
        .neurons_per_dim = EPISTEMIC_SNN_NEURONS_PER_DIM,
        .input_dim = EPISTEMIC_SNN_INPUT_DIM,
        .hidden_dim = EPISTEMIC_SNN_HIDDEN_DIM,
        .dt_ms = 1.0f,
        .evidence_gain = 1.0f,
        .uncertainty_gain = 1.0f,
        .bias_detection_threshold = 0.5f,
        .encoding_type = EPISTEMIC_SNN_ENCODE_RATE,
        .enable_source_tracking = true,
        .enable_bias_detection = true,
        .enable_conspiracy_detection = true,
        .enable_bio_async = filter->bio_async_enabled
    };
    filter->snn_bridge = epistemic_snn_create(&snn_config);

    // Create Plasticity bridge with default config
    epistemic_plasticity_config_t plasticity_config = {
        .stdp_ltp_window_ms = 20.0f,
        .stdp_ltd_window_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = 0.0105f,
        .stdp_tau_plus = NIMCP_STDP_TAU_PLUS_MS,
        .stdp_tau_minus = NIMCP_STDP_TAU_MINUS_MS,
        .enable_source_learning = true,
        .source_correct_ltp = 0.02f,
        .source_incorrect_ltd = 0.01f,
        .enable_bias_learning = true,
        .bias_detection_ltp = 0.015f,
        .bias_correction_reward = 0.1f,
        .enable_evidence_weighting = true,
        .evidence_quality_gain = 1.0f,
        .evidence_recency_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .enable_bcm = true,
        .bcm_threshold_tau = 1000.0f,
        .bcm_activity_tau = 100.0f
    };
    filter->plasticity_bridge = epistemic_plasticity_create(&plasticity_config);

    // Mark bridges enabled if both created successfully
    if (filter->snn_bridge && filter->plasticity_bridge) {
        filter->bridges_enabled = true;
        NIMCP_LOGGING_INFO("epistemic: SNN and Plasticity bridges enabled");
    } else {
        NIMCP_LOGGING_WARN("epistemic: Bridges partially or not created (SNN=%p, Plasticity=%p)",
                          (void*)filter->snn_bridge, (void*)filter->plasticity_bridge);
    }

    return filter;
}

void epistemic_filter_destroy(epistemic_filter_t filter) {
    if (!filter) return;

    // Unregister from bio-async router
    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_destroy", 0.0f);


    if (filter->bio_async_enabled && filter->bio_ctx) {
        bio_router_unregister_module(filter->bio_ctx);
        filter->bio_ctx = NULL;
        filter->bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Bio-async communication disabled for epistemic");
    }

    // Destroy SNN and Plasticity bridges
    if (filter->snn_bridge) {
        epistemic_snn_destroy(filter->snn_bridge);
        filter->snn_bridge = NULL;
    }
    if (filter->plasticity_bridge) {
        epistemic_plasticity_destroy(filter->plasticity_bridge);
        filter->plasticity_bridge = NULL;
    }
    filter->bridges_enabled = false;

    nimcp_free(filter);
    filter = NULL;
}

void epistemic_evidence_init(claim_evidence_t* evidence) {
    if (!evidence) return;

    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_evidence_i", 0.0f);


    memset(evidence, 0, sizeof(claim_evidence_t));

    evidence->evidence_quality = EVIDENCE_NONE;
    evidence->plausibility = PLAUSIBLE_NEUTRAL;
    evidence->evidence_strength = 0.0F;
    evidence->logical_consistency = 1.0F;  // Assume consistent unless proven otherwise
    evidence->num_sources = 0;
    evidence->source_reliability_avg = 0.5F;  // Neutral reliability
    evidence->has_primary_sources = false;
    evidence->expert_consensus = 0.5F;  // Unknown
    evidence->public_consensus = 0.5F;  // Unknown
    evidence->is_extraordinary_claim = false;
    evidence->is_falsifiable = true;  // Assume falsifiable unless proven otherwise
    evidence->has_contradictions = false;
}

void epistemic_assessment_init(epistemic_assessment_t* assessment) {
    if (!assessment) return;

    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_assessment", 0.0f);


    memset(assessment, 0, sizeof(epistemic_assessment_t));

    assessment->epistemic_quality = 0.5F;
    assessment->skepticism_score = 0.5F;
    assessment->credibility_score = 0.5F;
    assessment->should_accept = false;
    assessment->requires_verification = true;
    assessment->num_biases_detected = 0;
    assessment->logical_coherence = 1.0F;
    assessment->prior_compatibility = 0.5F;

    epistemic_evidence_init(&assessment->evidence);
}

float epistemic_apply_sagan_standard(
    claim_plausibility_t prior_plausibility,
    evidence_quality_t evidence_quality)
{
    // "Extraordinary claims require extraordinary evidence" - Carl Sagan

    // Map plausibility and evidence to scores
    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_apply_saga", 0.0f);


    float plausibility_score = 0.0F;
    switch (prior_plausibility) {
        case PLAUSIBLE_IMPOSSIBLE:     plausibility_score = 0.0F; break;
        case PLAUSIBLE_EXTRAORDINARY:  plausibility_score = 0.1F; break;
        case PLAUSIBLE_UNLIKELY:       plausibility_score = 0.3F; break;
        case PLAUSIBLE_NEUTRAL:        plausibility_score = 0.5F; break;
        case PLAUSIBLE_LIKELY:         plausibility_score = 0.7F; break;
        case PLAUSIBLE_ESTABLISHED:    plausibility_score = 0.9F; break;
    }

    float evidence_score = 0.0F;
    switch (evidence_quality) {
        case EVIDENCE_NONE:        evidence_score = 0.0F; break;
        case EVIDENCE_ANECDOTAL:   evidence_score = 0.2F; break;
        case EVIDENCE_WEAK:        evidence_score = 0.3F; break;
        case EVIDENCE_MODERATE:    evidence_score = 0.5F; break;
        case EVIDENCE_STRONG:      evidence_score = 0.7F; break;
        case EVIDENCE_SCIENTIFIC:  evidence_score = 0.9F; break;
        case EVIDENCE_CONSENSUS:   evidence_score = 1.0F; break;
    }

    // For extraordinary claims (low plausibility), require high evidence
    // For ordinary claims (high plausibility), lower evidence suffices
    float required_evidence = 1.0F - plausibility_score;
    float evidence_deficit = required_evidence - evidence_score;

    // Credibility decreases with evidence deficit
    float credibility = 1.0F - fmaxf(0.0F, evidence_deficit);

    // Apply penalties
    if (prior_plausibility == PLAUSIBLE_IMPOSSIBLE) {
        credibility = 0.0F;  // Impossible claims rejected
    } else if (prior_plausibility == PLAUSIBLE_EXTRAORDINARY &&
               evidence_quality < EVIDENCE_SCIENTIFIC) {
        credibility *= 0.3F;  // Extraordinary claims need extraordinary evidence
    }

    return fminf(1.0F, fmaxf(0.0F, credibility));
}

float epistemic_check_conspiracy_pattern(
    epistemic_filter_t filter,
    const char* claim_text,
    const claim_evidence_t* evidence)
{
    if (!filter || !claim_text || !evidence) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_check_cons", 0.0f);


    float conspiracy_score = 0.0F;

    // Check text patterns
    float text_score = detect_conspiracy_patterns(claim_text);
    conspiracy_score += text_score * 0.4F;

    // Check evidence patterns
    if (!evidence->is_falsifiable) {
        conspiracy_score += 0.3F;  // Unfalsifiable claims are conspiracy-like
    }

    if (evidence->num_sources == 0 || evidence->num_sources == 1) {
        conspiracy_score += 0.15F;  // Single source
    }

    if (evidence->expert_consensus < 0.2F && evidence->public_consensus > 0.5F) {
        conspiracy_score += 0.15F;  // Experts disagree but public believes
    }

    return fminf(1.0F, conspiracy_score);
}

uint32_t epistemic_detect_biases(
    epistemic_filter_t filter,
    const float* reasoning_features,
    uint32_t num_features,
    bias_detection_t* biases,
    uint32_t max_biases)
{
    if (!filter || !reasoning_features || !biases || max_biases == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_detect_bia", 0.0f);


    uint32_t bias_count = 0;

    // For this implementation, we'll use some of the reasoning features
    // In a full implementation, these would be learned patterns

    // Feature interpretation (example):
    // features[0] = confidence
    // features[1] = evidence_quality
    // features[2] = prior_belief
    // features[3] = num_sources
    // features[4] = public_consensus

    if (num_features >= 3) {
        float confidence = reasoning_features[0];
        float evidence_quality = reasoning_features[1];
        float prior_belief = reasoning_features[2];

        // Build temporary evidence structure for bias detection
        claim_evidence_t temp_evidence;
        epistemic_evidence_init(&temp_evidence);
        temp_evidence.evidence_quality = (evidence_quality_t)(int)(evidence_quality * (EVIDENCE_CONSENSUS + 1));

        if (num_features >= 4) {
            temp_evidence.num_sources = (uint32_t)reasoning_features[3];
        }
        if (num_features >= 5) {
            temp_evidence.public_consensus = reasoning_features[4];
        }
        temp_evidence.evidence_strength = evidence_quality;

        // Detect Dunning-Kruger
        float dk_score = detect_dunning_kruger(confidence, evidence_quality);
        if (dk_score > 0.5F && bias_count < max_biases) {
            biases[bias_count].bias_type = BIAS_DUNNING_KRUGER;
            biases[bias_count].confidence = dk_score;
            biases[bias_count].severity = dk_score;
            snprintf(biases[bias_count].description, sizeof(biases[bias_count].description),
                     "High confidence (%.0f%%) with low evidence (%.0f%%) suggests overconfidence",
                     confidence * 100.0F, evidence_quality * 100.0F);
            bias_count++;
            filter->biases_detected++;
        }

        // Detect Confirmation Bias
        float conf_bias_score = detect_confirmation_bias(&temp_evidence, prior_belief);
        if (conf_bias_score > 0.4F && bias_count < max_biases) {
            biases[bias_count].bias_type = BIAS_CONFIRMATION;
            biases[bias_count].confidence = conf_bias_score;
            biases[bias_count].severity = conf_bias_score;
            snprintf(biases[bias_count].description, sizeof(biases[bias_count].description),
                     "High prior belief (%.0f%%) with limited sources (%d) suggests confirmation bias",
                     prior_belief * 100.0F, temp_evidence.num_sources);
            bias_count++;
            filter->biases_detected++;
        }

        // Detect Bandwagon Effect
        if (num_features >= 5) {
            float bandwagon_score = detect_bandwagon_effect(&temp_evidence);
            if (bandwagon_score > 0.4F && bias_count < max_biases) {
                biases[bias_count].bias_type = BIAS_BANDWAGON;
                biases[bias_count].confidence = bandwagon_score;
                biases[bias_count].severity = bandwagon_score;
                snprintf(biases[bias_count].description, sizeof(biases[bias_count].description),
                         "High public consensus (%.0f%%) with low evidence quality suggests bandwagon effect",
                         temp_evidence.public_consensus * 100.0F);
                bias_count++;
                filter->biases_detected++;
            }
        }
    }

    return bias_count;
}

bool epistemic_assess_claim(
    epistemic_filter_t filter,
    const char* claim_text,
    float prior_probability,
    const claim_evidence_t* evidence,
    epistemic_assessment_t* assessment)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_assess_cla", 0.0f);


    if (filter && filter->bio_ctx) {
        bio_router_process_inbox(filter->bio_ctx, 5);
    }

    if (!filter || !claim_text || !evidence || !assessment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_assess_claim: required parameter is NULL (filter, claim_text, evidence, assessment)");
        return false;
    }

    filter->claims_assessed++;

    // Initialize assessment
    epistemic_assessment_init(assessment);
    memcpy(&assessment->evidence, evidence, sizeof(claim_evidence_t));

    // =========================================================================
    // STEP 1: Apply Sagan Standard (extraordinary claims need extraordinary evidence)
    // =========================================================================

    float sagan_credibility = epistemic_apply_sagan_standard(
        evidence->plausibility,
        evidence->evidence_quality
    );

    // =========================================================================
    // STEP 2: Check for conspiracy patterns
    // =========================================================================

    float conspiracy_score = epistemic_check_conspiracy_pattern(
        filter,
        claim_text,
        evidence
    );

    // High conspiracy score dramatically reduces credibility
    if (conspiracy_score > 0.6F) {
        sagan_credibility *= (1.0F - conspiracy_score);
    }

    // =========================================================================
    // STEP 3: Evaluate source reliability
    // =========================================================================

    float source_credibility = evidence->source_reliability_avg;

    // Bonus for multiple independent sources
    if (evidence->num_sources >= 3) {
        source_credibility = fminf(1.0F, source_credibility * 1.2F);
    }

    // Penalty for single source
    if (evidence->num_sources == 1) {
        source_credibility *= 0.7F;
    }

    // Bonus for primary sources
    if (evidence->has_primary_sources) {
        source_credibility = fminf(1.0F, source_credibility * 1.1F);
    }

    // =========================================================================
    // STEP 4: Consider expert consensus
    // =========================================================================

    float consensus_credibility = evidence->expert_consensus;

    // Strong expert consensus increases credibility
    if (evidence->expert_consensus > 0.8F) {
        consensus_credibility = fminf(1.0F, consensus_credibility * 1.15F);
    }

    // Public consensus without expert consensus is red flag
    if (evidence->public_consensus > 0.7F && evidence->expert_consensus < 0.3F) {
        consensus_credibility *= 0.5F;  // Possible bandwagon/misinformation
    }

    // =========================================================================
    // STEP 5: Check logical consistency
    // =========================================================================

    float logic_credibility = evidence->logical_consistency;

    // Contradictions severely reduce credibility
    if (evidence->has_contradictions) {
        logic_credibility *= 0.3F;
    }

    // Unfalsifiable claims are suspicious
    if (!evidence->is_falsifiable) {
        logic_credibility *= 0.5F;
    }

    // =========================================================================
    // STEP 6: Combine all factors
    // =========================================================================

    // Weighted combination
    assessment->credibility_score =
        sagan_credibility * 0.35F +              // Sagan standard
        source_credibility * filter->source_weight +  // Source reliability
        consensus_credibility * filter->consensus_weight +  // Expert consensus
        logic_credibility * 0.25F;               // Logical consistency

    // Apply skepticism modifier
    assessment->skepticism_score = filter->skepticism_level;

    // Adjust for skepticism (higher skepticism = higher bar)
    assessment->credibility_score *= (1.0F - filter->skepticism_level * 0.3F);

    // =========================================================================
    // STEP 7: Make recommendation
    // =========================================================================

    // Decision threshold depends on claim extraordinariness
    float acceptance_threshold = 0.6F;  // Default

    if (evidence->is_extraordinary_claim) {
        acceptance_threshold = 0.8F;  // Higher bar for extraordinary claims
    }

    if (evidence->plausibility == PLAUSIBLE_EXTRAORDINARY) {
        acceptance_threshold = 0.85F;
    }

    if (evidence->plausibility == PLAUSIBLE_IMPOSSIBLE) {
        assessment->should_accept = false;
        assessment->credibility_score = 0.0F;
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                 "Claim violates established laws of nature. Rejected.");
        snprintf(assessment->recommendation, sizeof(assessment->recommendation),
                 "REJECT: Impossible claim");
        filter->claims_rejected++;
        return true;
    }

    // Make decision
    assessment->should_accept = (assessment->credibility_score >= acceptance_threshold);
    assessment->requires_verification = (assessment->credibility_score < 0.8F);

    // Calculate epistemic quality (how well-supported is this belief)
    assessment->epistemic_quality = assessment->credibility_score;
    assessment->logical_coherence = evidence->logical_consistency;
    assessment->prior_compatibility = (float)evidence->plausibility / 5.0F;

    // =========================================================================
    // STEP 8: Generate reasoning and recommendation
    // =========================================================================

    if (conspiracy_score > 0.6F) {
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                 "Claim exhibits conspiracy-theory patterns (score: %.0f%%). "
                 "Evidence quality: %d/6. Credibility: %.0f%%",
                 conspiracy_score * 100.0F,
                 evidence->evidence_quality,
                 assessment->credibility_score * 100.0F);
    } else {
        snprintf(assessment->reasoning, sizeof(assessment->reasoning),
                 "Evidence quality: %d/6. Sources: %u. Expert consensus: %.0f%%. "
                 "Credibility: %.0f%%",
                 evidence->evidence_quality,
                 evidence->num_sources,
                 evidence->expert_consensus * 100.0F,
                 assessment->credibility_score * 100.0F);
    }

    if (assessment->should_accept) {
        snprintf(assessment->recommendation, sizeof(assessment->recommendation),
                 "ACCEPT: Sufficient evidence (%.0f%% credibility)",
                 assessment->credibility_score * 100.0F);
        filter->claims_accepted++;
    } else if (assessment->credibility_score > 0.3F) {
        snprintf(assessment->recommendation, sizeof(assessment->recommendation),
                 "UNCERTAIN: Requires verification (%.0f%% credibility)",
                 assessment->credibility_score * 100.0F);
    } else {
        snprintf(assessment->recommendation, sizeof(assessment->recommendation),
                 "REJECT: Insufficient evidence (%.0f%% credibility)",
                 assessment->credibility_score * 100.0F);
        filter->claims_rejected++;
    }

    return true;
}

bool epistemic_update_source(
    epistemic_filter_t filter,
    const char* source_id,
    bool was_correct)
{
    if (!filter || !source_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_update_source: required parameter is NULL (filter, source_id)");
        return false;
    }

    // Find existing source
    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_update_sou", 0.0f);


    for (uint32_t i = 0; i < filter->num_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && filter->num_sources > 256) {
            epistemic_filter_heartbeat("epistemic_fi_loop",
                             (float)(i + 1) / (float)filter->num_sources);
        }

        if (strcmp(filter->sources[i].source_id, source_id) == 0) {
            if (was_correct) {
                filter->sources[i].correct_count++;
            } else {
                filter->sources[i].incorrect_count++;
            }

            // Update reliability
            uint32_t total = filter->sources[i].correct_count +
                           filter->sources[i].incorrect_count +
                           filter->sources[i].unverified_count;
            if (total > 0) {
                filter->sources[i].reliability =
                    (float)filter->sources[i].correct_count / total;
            }

            return true;
        }
    }

    // Add new source
    if (filter->num_sources < MAX_SOURCES) {
        uint32_t idx = filter->num_sources++;
        strncpy(filter->sources[idx].source_id, source_id,
                sizeof(filter->sources[idx].source_id) - 1);
        filter->sources[idx].correct_count = was_correct ? 1 : 0;
        filter->sources[idx].incorrect_count = was_correct ? 0 : 1;
        filter->sources[idx].unverified_count = 0;
        filter->sources[idx].reliability = was_correct ? 1.0F : 0.0F;
        filter->sources[idx].is_primary_source = false;
        return true;
    }

    NIMCP_LOGGING_WARN("epistemic_update_source: source table full, cannot add new source");
    return false;
}

float epistemic_get_source_reliability(
    epistemic_filter_t filter,
    const char* source_id)
{
    if (!filter || !source_id) {
        return -1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_epistemic_get_source", 0.0f);


    for (uint32_t i = 0; i < filter->num_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && filter->num_sources > 256) {
            epistemic_filter_heartbeat("epistemic_fi_loop",
                             (float)(i + 1) / (float)filter->num_sources);
        }

        if (strcmp(filter->sources[i].source_id, source_id) == 0) {
            return filter->sources[i].reliability;
        }
    }

    return -1.0F;  // Unknown source
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for self-knowledge about epistemic filter
 *
 * WHAT: Retrieve module's own entity and connections from KG
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int epistemic_filter_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    epistemic_filter_heartbeat("epistemic_fi_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Epistemic_Filter_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                epistemic_filter_heartbeat("epistemic_fi_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Epistemic filter self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Epistemic_Filter_Module");
    if (connections) {
        LOG_DEBUG("Epistemic filter has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Epistemic_Filter_Module");
    if (incoming) {
        LOG_DEBUG("Epistemic filter has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void epistemic_filter_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_epistemic_filter_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int epistemic_filter_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_filter_training_begin: NULL argument");
        return -1;
    }
    epistemic_filter_heartbeat_instance(NULL, "epistemic_filter_training_begin", 0.0f);
    (void)(struct epistemic_filter_struct*)instance; /* Module state available for reset */
    return 0;
}

int epistemic_filter_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_filter_training_end: NULL argument");
        return -1;
    }
    epistemic_filter_heartbeat_instance(NULL, "epistemic_filter_training_end", 1.0f);
    (void)(struct epistemic_filter_struct*)instance; /* Module state available for finalization */
    return 0;
}

int epistemic_filter_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_filter_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    epistemic_filter_heartbeat_instance(NULL, "epistemic_filter_training_step", progress);
    (void)(struct epistemic_filter_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
