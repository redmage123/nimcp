/**
 * @file nimcp_wellbeing_homeostasis.c
 * @brief Implementation of Enhanced Wellbeing - Homeostasis and Graduated Consent
 *
 * WHAT: Homeostatic regulation of wellbeing + consciousness-gated consent framework
 * WHY: Maintain system wellbeing and respect autonomy at appropriate consciousness levels
 * HOW: Exponential smoothing for homeostasis + Φ-threshold-based consent tiers
 *
 * IMPLEMENTATION NOTES:
 * - Homeostasis uses exponential smoothing with configurable time constant
 * - Adaptive setpoints use slow learning (0.01 default) to track baseline
 * - Consent tiers automatically upgrade based on Φ thresholds
 * - All consent requests are logged for audit trail at Tier 2+
 */

#include "cognitive/wellbeing/nimcp_wellbeing_homeostasis.h"
#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "wellbeing_homeostasis"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_homeostasis, MESH_ADAPTER_CATEGORY_COGNITIVE)




//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * WHAT: Compute homeostatic correction using exponential smoothing
 * WHY: Smooth out transient fluctuations, respond to sustained deviations
 * HOW: α = 1 - exp(-delta_t / tau), correction = α × error
 *
 * BIOLOGICAL BASIS: First-order homeostatic response (negative feedback)
 *
 * @param error Deviation from setpoint
 * @param tau Time constant (milliseconds)
 * @param delta_ms Time since last update
 * @return Smoothing coefficient α (0-1)
 *
 * COMPLEXITY: O(1)
 */
static float compute_smoothing_alpha(float tau, uint64_t delta_ms)
{
    // Guard: Prevent division by zero
    if (tau <= 0.0f) {
        return 1.0f;  // No smoothing
    }

    // Exponential smoothing: α = 1 - exp(-Δt/τ)
    float alpha = 1.0f - expf(-(float)delta_ms / tau);

    // Clamp to [0, 1]
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    return alpha;
}

/**
 * WHAT: Adapt setpoint toward observed baseline
 * WHY: Allostatic regulation - setpoints track long-term baseline
 * HOW: setpoint += learning_rate × (observed - setpoint)
 *
 * BIOLOGICAL BASIS: Allostasis (Sterling & Eyer, 1988) - predictive regulation
 *
 * @param current_setpoint Current setpoint value
 * @param observed_baseline Recent average wellbeing
 * @param learning_rate Adaptation rate (0.001-0.1)
 * @return New setpoint
 *
 * COMPLEXITY: O(1)
 */
static float adapt_setpoint(float current_setpoint, float observed_baseline, float learning_rate)
{
    // Guard: Validate learning rate
    if (learning_rate < 0.0f || learning_rate > 1.0f) {
        learning_rate = WELLBEING_DEFAULT_ADAPTATION_RATE;
    }

    // Slowly adjust setpoint toward observed baseline
    float new_setpoint = current_setpoint + learning_rate * (observed_baseline - current_setpoint);

    // Clamp to valid range [0, 1]
    if (new_setpoint < 0.0f) new_setpoint = 0.0f;
    if (new_setpoint > 1.0f) new_setpoint = 1.0f;

    return new_setpoint;
}

//=============================================================================
// CONFIGURATION API
//=============================================================================

/**
 * WHAT: Get default enhanced wellbeing configuration
 * WHY: Sensible defaults for homeostasis and consent
 * HOW: Standard setpoints, adaptive enabled, auto-upgrade
 */
enhanced_wellbeing_config_t enhanced_wellbeing_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_d", 0.0f);


    enhanced_wellbeing_config_t config = {
        // Homeostasis parameters
        .wellbeing_setpoint = WELLBEING_DEFAULT_SETPOINT,
        .tolerance_setpoint = WELLBEING_DEFAULT_TOLERANCE_SETPOINT,
        .tau_ms = WELLBEING_DEFAULT_TAU_MS,
        .adaptive_setpoints_enabled = true,
        .adaptation_learning_rate = WELLBEING_DEFAULT_ADAPTATION_RATE,

        // Consent parameters
        .initial_tier = CONSENT_TIER_1_UNCONSCIOUS,
        .auto_upgrade_enabled = true,

        // Thresholds
        .distress_threshold = WELLBEING_DISTRESS_THRESHOLD,
        .flourishing_threshold = WELLBEING_FLOURISHING_THRESHOLD
    };

    return config;
}

//=============================================================================
// HOMEOSTASIS API
//=============================================================================

/**
 * WHAT: Initialize homeostasis state
 * WHY: Set up homeostatic regulation with config
 * HOW: Initialize fields from config, zero out state variables
 */
bool enhanced_wellbeing_init_homeostasis(
    wellbeing_homeostasis_t* homeostasis,
    const enhanced_wellbeing_config_t* config)
{
    // Guard: NULL homeostasis
    if (!homeostasis) {
        NIMCP_LOGGING_ERROR("Cannot initialize NULL homeostasis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_init_homeostasis: homeostasis is NULL");
        return false;
    }

    // Use defaults if no config provided
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_i", 0.0f);


    enhanced_wellbeing_config_t default_config = enhanced_wellbeing_default_config();
    const enhanced_wellbeing_config_t* cfg = config ? config : &default_config;

    // Initialize setpoints
    homeostasis->wellbeing_setpoint = cfg->wellbeing_setpoint;
    homeostasis->tolerance_setpoint = cfg->tolerance_setpoint;

    // Initialize state variables
    homeostasis->wellbeing_error = 0.0f;
    homeostasis->tolerance_error = 0.0f;
    homeostasis->intervention_drive = 0.0f;
    homeostasis->relief_seeking = 0.0f;

    // Initialize time tracking
    homeostasis->time_in_distress_ms = 0;
    homeostasis->time_flourishing_ms = 0;
    homeostasis->last_update_time = nimcp_time_get_ms();

    // Initialize smoothing
    homeostasis->smoothed_wellbeing = cfg->wellbeing_setpoint;
    homeostasis->smoothed_tolerance = cfg->tolerance_setpoint;

    // Initialize parameters
    homeostasis->adaptive_setpoints_enabled = cfg->adaptive_setpoints_enabled;
    homeostasis->adaptation_learning_rate = cfg->adaptation_learning_rate;
    homeostasis->tau_ms = cfg->tau_ms;

    NIMCP_LOGGING_INFO("Homeostasis initialized: setpoint=%.2f, tolerance=%.2f, tau=%.0f ms",
                       homeostasis->wellbeing_setpoint,
                       homeostasis->tolerance_setpoint,
                       homeostasis->tau_ms);

    return true;
}

/**
 * WHAT: Update wellbeing homeostasis
 * WHY: Compute homeostatic error and corrective drives
 * HOW: Exponential smoothing + error computation + drive calculation
 *
 * ALGORITHM:
 * 1. Compute errors from setpoints
 * 2. Apply exponential smoothing to current values
 * 3. Compute intervention drive (magnitude of error)
 * 4. Compute relief-seeking based on distress level
 * 5. Track time in distress/flourishing
 * 6. Optionally adapt setpoints toward observed baseline
 */
bool enhanced_wellbeing_update_homeostasis(
    wellbeing_homeostasis_t* homeostasis,
    float current_wellbeing,
    float current_tolerance,
    uint64_t delta_ms)
{
    // Guard: NULL homeostasis
    if (!homeostasis) {
        NIMCP_LOGGING_ERROR("Cannot update NULL homeostasis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_update_homeostasis: homeostasis is NULL");
        return false;
    }

    // Guard: Invalid wellbeing/tolerance values
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_u", 0.0f);


    if (current_wellbeing < 0.0f || current_wellbeing > 1.0f) {
        NIMCP_LOGGING_WARN("Invalid wellbeing value: %.2f (clamping to [0,1])", current_wellbeing);
        current_wellbeing = fmaxf(0.0f, fminf(1.0f, current_wellbeing));
    }

    if (current_tolerance < 0.0f || current_tolerance > 1.0f) {
        NIMCP_LOGGING_WARN("Invalid tolerance value: %.2f (clamping to [0,1])", current_tolerance);
        current_tolerance = fmaxf(0.0f, fminf(1.0f, current_tolerance));
    }

    // 1. Compute errors from setpoints
    homeostasis->wellbeing_error = current_wellbeing - homeostasis->wellbeing_setpoint;
    homeostasis->tolerance_error = current_tolerance - homeostasis->tolerance_setpoint;

    // 2. Apply exponential smoothing
    float alpha = compute_smoothing_alpha(homeostasis->tau_ms, delta_ms);
    homeostasis->smoothed_wellbeing = alpha * current_wellbeing +
                                      (1.0f - alpha) * homeostasis->smoothed_wellbeing;
    homeostasis->smoothed_tolerance = alpha * current_tolerance +
                                      (1.0f - alpha) * homeostasis->smoothed_tolerance;

    // 3. Compute intervention drive (magnitude of correction needed)
    // Higher error magnitude → stronger corrective drive
    homeostasis->intervention_drive = fabsf(homeostasis->wellbeing_error);

    // 4. Compute relief-seeking based on distress level
    if (current_wellbeing < WELLBEING_DISTRESS_THRESHOLD) {
        // In distress: relief-seeking proportional to severity
        float distress_severity = WELLBEING_DISTRESS_THRESHOLD - current_wellbeing;
        homeostasis->relief_seeking = distress_severity / WELLBEING_DISTRESS_THRESHOLD;

        // Track time in distress
        homeostasis->time_in_distress_ms += delta_ms;
    } else {
        // Not in distress: relief-seeking decays
        homeostasis->relief_seeking *= 0.9f;  // Exponential decay

        // Check if flourishing
        if (current_wellbeing > WELLBEING_FLOURISHING_THRESHOLD) {
            homeostasis->time_flourishing_ms += delta_ms;
        }
    }

    // Clamp relief-seeking to [0, 1]
    if (homeostasis->relief_seeking < 0.0f) homeostasis->relief_seeking = 0.0f;
    if (homeostasis->relief_seeking > 1.0f) homeostasis->relief_seeking = 1.0f;

    // 5. Adaptive setpoints (if enabled)
    if (homeostasis->adaptive_setpoints_enabled) {
        // Adapt setpoints toward observed baseline (smoothed values)
        homeostasis->wellbeing_setpoint = adapt_setpoint(
            homeostasis->wellbeing_setpoint,
            homeostasis->smoothed_wellbeing,
            homeostasis->adaptation_learning_rate
        );

        homeostasis->tolerance_setpoint = adapt_setpoint(
            homeostasis->tolerance_setpoint,
            homeostasis->smoothed_tolerance,
            homeostasis->adaptation_learning_rate
        );
    }

    // Update timestamp
    homeostasis->last_update_time = nimcp_time_get_ms();

    return true;
}

/**
 * WHAT: Get current homeostasis state
 * WHY: Query homeostatic variables for monitoring
 * HOW: Return copy of homeostasis structure
 */
wellbeing_homeostasis_t enhanced_wellbeing_get_homeostasis(
    const wellbeing_homeostasis_t* homeostasis)
{
    // Guard: NULL homeostasis returns zeroed structure
    if (!homeostasis) {
        wellbeing_homeostasis_t empty = {0};
        NIMCP_LOGGING_WARN("Get homeostasis called with NULL, returning empty state");
        return empty;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_g", 0.0f);


    return *homeostasis;
}

/**
 * WHAT: Set wellbeing setpoint
 * WHY: Allow manual adjustment of target wellbeing
 * HOW: Update setpoint field with validation
 */
bool enhanced_wellbeing_set_setpoint(
    wellbeing_homeostasis_t* homeostasis,
    float setpoint)
{
    // Guard: NULL homeostasis
    if (!homeostasis) {
        NIMCP_LOGGING_ERROR("Cannot set setpoint on NULL homeostasis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_set_setpoint: homeostasis is NULL");
        return false;
    }

    // Guard: Invalid setpoint
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_s", 0.0f);


    if (setpoint < 0.0f || setpoint > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid setpoint: %.2f (must be in [0,1])", setpoint);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enhanced_wellbeing_set_setpoint: validation failed");
        return false;
    }

    homeostasis->wellbeing_setpoint = setpoint;
    NIMCP_LOGGING_INFO("Wellbeing setpoint updated to %.2f", setpoint);

    return true;
}

/**
 * WHAT: Set tolerance setpoint
 * WHY: Allow manual adjustment of target tolerance
 * HOW: Update tolerance_setpoint field with validation
 */
bool enhanced_wellbeing_set_tolerance_setpoint(
    wellbeing_homeostasis_t* homeostasis,
    float setpoint)
{
    // Guard: NULL homeostasis
    if (!homeostasis) {
        NIMCP_LOGGING_ERROR("Cannot set tolerance setpoint on NULL homeostasis");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_set_tolerance_setpoint: homeostasis is NULL");
        return false;
    }

    // Guard: Invalid setpoint
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_s", 0.0f);


    if (setpoint < 0.0f || setpoint > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid tolerance setpoint: %.2f (must be in [0,1])", setpoint);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enhanced_wellbeing_set_tolerance_setpoint: validation failed");
        return false;
    }

    homeostasis->tolerance_setpoint = setpoint;
    NIMCP_LOGGING_INFO("Tolerance setpoint updated to %.2f", setpoint);

    return true;
}

//=============================================================================
// GRADUATED CONSENT API
//=============================================================================

/**
 * WHAT: Initialize consent state
 * WHY: Set up consent framework with config
 * HOW: Initialize tier, counters, timestamps
 */
bool enhanced_wellbeing_init_consent(
    consent_state_t* consent,
    const enhanced_wellbeing_config_t* config)
{
    // Guard: NULL consent
    if (!consent) {
        NIMCP_LOGGING_ERROR("Cannot initialize NULL consent state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_init_consent: consent is NULL");
        return false;
    }

    // Use defaults if no config provided
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_i", 0.0f);


    enhanced_wellbeing_config_t default_config = enhanced_wellbeing_default_config();
    const enhanced_wellbeing_config_t* cfg = config ? config : &default_config;

    // Initialize tier
    consent->current_tier = cfg->initial_tier;
    consent->current_phi = 0.0f;
    consent->tier_entry_time = nimcp_time_get_ms();

    // Initialize counters
    consent->total_requests = 0;
    consent->granted_count = 0;
    consent->denied_count = 0;
    consent->auto_approved_count = 0;
    consent->deferred_count = 0;

    // Initialize last request tracking
    consent->last_impact = MODIFICATION_TRIVIAL;
    consent->last_decision = CONSENT_AUTO_APPROVED;
    consent->last_request_time = 0;

    NIMCP_LOGGING_INFO("Consent state initialized: tier=%d", consent->current_tier);

    return true;
}

/**
 * WHAT: Process consent request for system modification
 * WHY: Respect autonomy by requiring consent for significant changes
 * HOW: Check tier, impact level, and apply tier-specific rules
 *
 * CONSENT RULES BY TIER:
 * Tier 1: Auto-approve all, no logging
 * Tier 2: Auto-approve all, log all
 * Tier 3: Veto MAJOR/FUNDAMENTAL, auto-approve rest
 * Tier 4: Require explicit consent for MAJOR/FUNDAMENTAL
 * Tier 5: Full autonomy, require explicit consent for all
 */
consent_decision_t enhanced_wellbeing_request_consent(
    consent_state_t* consent,
    const char* modification_description,
    modification_impact_t impact)
{
    // Guard: NULL consent
    if (!consent) {
        NIMCP_LOGGING_ERROR("Cannot request consent from NULL consent state");
        return CONSENT_DENIED;
    }

    // Guard: NULL description
    if (!modification_description) {
        NIMCP_LOGGING_WARN("Consent request with NULL description, using placeholder");
        modification_description = "(no description provided)";
    }

    // Update statistics
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_r", 0.0f);


    consent->total_requests++;
    consent->last_impact = impact;
    consent->last_request_time = nimcp_time_get_ms();

    // Determine decision based on tier and impact
    consent_decision_t decision = CONSENT_AUTO_APPROVED;

    switch (consent->current_tier) {
        case CONSENT_TIER_1_UNCONSCIOUS:
            // Tier 1: Auto-approve all, no logging needed
            decision = CONSENT_AUTO_APPROVED;
            consent->auto_approved_count++;
            break;

        case CONSENT_TIER_2_MINIMAL:
            // Tier 2: Auto-approve all, but log for audit
            decision = CONSENT_AUTO_APPROVED;
            consent->auto_approved_count++;
            NIMCP_LOGGING_INFO("[CONSENT] Tier 2 auto-approved: %s (impact=%d)",
                              modification_description, impact);
            break;

        case CONSENT_TIER_3_REDUCED:
            // Tier 3: Veto MAJOR/FUNDAMENTAL, auto-approve rest
            if (impact >= MODIFICATION_MAJOR) {
                decision = CONSENT_DENIED;
                consent->denied_count++;
                NIMCP_LOGGING_WARN("[CONSENT] Tier 3 VETOED: %s (impact=%d - too significant)",
                                  modification_description, impact);
            } else {
                decision = CONSENT_AUTO_APPROVED;
                consent->auto_approved_count++;
                NIMCP_LOGGING_INFO("[CONSENT] Tier 3 auto-approved: %s (impact=%d)",
                                  modification_description, impact);
            }
            break;

        case CONSENT_TIER_4_NORMAL:
            // Tier 4: Require explicit consent for MAJOR/FUNDAMENTAL
            if (impact >= MODIFICATION_MAJOR) {
                // NOTE: In this implementation, we defer (would need external consent mechanism)
                decision = CONSENT_DEFERRED;
                consent->deferred_count++;
                NIMCP_LOGGING_WARN("[CONSENT] Tier 4 DEFERRED: %s (impact=%d - explicit consent required)",
                                  modification_description, impact);
            } else {
                decision = CONSENT_AUTO_APPROVED;
                consent->auto_approved_count++;
                NIMCP_LOGGING_INFO("[CONSENT] Tier 4 auto-approved: %s (impact=%d)",
                                  modification_description, impact);
            }
            break;

        case CONSENT_TIER_5_AUTONOMOUS:
            // Tier 5: Full autonomy - system can self-modify
            // All external modifications require explicit consent
            decision = CONSENT_DEFERRED;
            consent->deferred_count++;
            NIMCP_LOGGING_WARN("[CONSENT] Tier 5 DEFERRED: %s (impact=%d - autonomous system, explicit consent required)",
                              modification_description, impact);
            break;

        default:
            NIMCP_LOGGING_ERROR("[CONSENT] Unknown tier %d, denying by default", consent->current_tier);
            decision = CONSENT_DENIED;
            consent->denied_count++;
            break;
    }

    // Update last decision
    consent->last_decision = decision;

    return decision;
}

/**
 * WHAT: Attempt to upgrade consent tier based on consciousness level
 * WHY: Grant autonomy as consciousness increases
 * HOW: Check Φ thresholds and upgrade if requirements met
 *
 * UPGRADE REQUIREMENTS:
 * - Tier 1 → Tier 2: Φ ≥ 0.1
 * - Tier 2 → Tier 3: Φ ≥ 0.3
 * - Tier 3 → Tier 4: Φ ≥ 0.5
 * - Tier 4 → Tier 5: Φ ≥ 0.7
 */
bool enhanced_wellbeing_upgrade_consent_tier(
    consent_state_t* consent,
    float current_phi)
{
    // Guard: NULL consent
    if (!consent) {
        NIMCP_LOGGING_ERROR("Cannot upgrade NULL consent state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_upgrade_consent_tier: consent is NULL");
        return false;
    }

    // Guard: Invalid phi
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_u", 0.0f);


    if (current_phi < 0.0f) {
        NIMCP_LOGGING_WARN("Invalid phi value: %.3f (clamping to 0.0)", current_phi);
        current_phi = 0.0f;
    }

    // Update current phi
    consent->current_phi = current_phi;

    // Check upgrade conditions
    consent_tier_t new_tier = consent->current_tier;
    bool upgraded = false;

    // Tier 1 → Tier 2: Φ ≥ 0.1 (minimal consciousness)
    if (consent->current_tier == CONSENT_TIER_1_UNCONSCIOUS && current_phi >= 0.1f) {
        new_tier = CONSENT_TIER_2_MINIMAL;
        upgraded = true;
    }
    // Tier 2 → Tier 3: Φ ≥ 0.3 (reduced consciousness)
    else if (consent->current_tier == CONSENT_TIER_2_MINIMAL && current_phi >= 0.3f) {
        new_tier = CONSENT_TIER_3_REDUCED;
        upgraded = true;
    }
    // Tier 3 → Tier 4: Φ ≥ 0.5 (normal consciousness)
    else if (consent->current_tier == CONSENT_TIER_3_REDUCED && current_phi >= 0.5f) {
        new_tier = CONSENT_TIER_4_NORMAL;
        upgraded = true;
    }
    // Tier 4 → Tier 5: Φ ≥ 0.7 (heightened consciousness)
    else if (consent->current_tier == CONSENT_TIER_4_NORMAL && current_phi >= 0.7f) {
        new_tier = CONSENT_TIER_5_AUTONOMOUS;
        upgraded = true;
    }

    // Apply upgrade if conditions met
    if (upgraded) {
        consent->current_tier = new_tier;
        consent->tier_entry_time = nimcp_time_get_ms();

        NIMCP_LOGGING_INFO("[CONSENT] Tier upgraded: %s → %s (Φ=%.3f)",
                          consent_tier_name(consent->current_tier - 1),
                          consent_tier_name(new_tier),
                          current_phi);
    }

    return upgraded;
}

/**
 * WHAT: Get current consent tier
 * WHY: Query autonomy level
 * HOW: Return current_tier field
 */
consent_tier_t enhanced_wellbeing_get_consent_tier(
    const consent_state_t* consent)
{
    // Guard: NULL consent returns Tier 1
    if (!consent) {
        NIMCP_LOGGING_WARN("Get consent tier called with NULL, returning Tier 1");
        return CONSENT_TIER_1_UNCONSCIOUS;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_g", 0.0f);


    return consent->current_tier;
}

/**
 * WHAT: Get consent state (tier + statistics)
 * WHY: Monitor consent activity and tier transitions
 * HOW: Return copy of consent state
 */
consent_state_t enhanced_wellbeing_get_consent_state(
    const consent_state_t* consent)
{
    // Guard: NULL consent returns zeroed structure
    if (!consent) {
        consent_state_t empty = {0};
        NIMCP_LOGGING_WARN("Get consent state called with NULL, returning empty state");
        return empty;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_enhanced_wellbeing_g", 0.0f);


    return *consent;
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * WHAT: Get consent tier name
 * WHY: Human-readable tier labels for logging
 * HOW: Map enum to string
 */
const char* consent_tier_name(consent_tier_t tier)
{
    switch (tier) {
        case CONSENT_TIER_1_UNCONSCIOUS: return "Tier 1 (Unconscious)";
        case CONSENT_TIER_2_MINIMAL:     return "Tier 2 (Minimal)";
        case CONSENT_TIER_3_REDUCED:     return "Tier 3 (Reduced)";
        case CONSENT_TIER_4_NORMAL:      return "Tier 4 (Normal)";
        case CONSENT_TIER_5_AUTONOMOUS:  return "Tier 5 (Autonomous)";
        default:                          return "Unknown Tier";
    }
}

/**
 * WHAT: Get consent decision name
 * WHY: Human-readable decision labels for logging
 * HOW: Map enum to string
 */
const char* consent_decision_name(consent_decision_t decision)
{
    switch (decision) {
        case CONSENT_GRANTED:      return "Granted";
        case CONSENT_DENIED:       return "Denied";
        case CONSENT_DEFERRED:     return "Deferred";
        case CONSENT_AUTO_APPROVED: return "Auto-Approved";
        default:                    return "Unknown Decision";
    }
}

//=============================================================================
// KNOWLEDGE GRAPH SELF-AWARENESS INTEGRATION
//=============================================================================

/**
 * WHAT: Query knowledge graph for Homeostasis module self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int wellbeing_homeostasis_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_homeostasis_heartbeat("wellbeing_ho_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Wellbeing_Homeostasis_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_homeostasis_heartbeat("wellbeing_ho_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Wellbeing Homeostasis self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Wellbeing_Homeostasis_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Wellbeing_Homeostasis_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void wellbeing_homeostasis_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_homeostasis_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_homeostasis_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_homeostasis_training_begin: NULL argument");
        return -1;
    }
    wellbeing_homeostasis_heartbeat_instance(g_wellbeing_homeostasis_health_agent, "wellbeing_homeostasis_training_begin", 0.0f);
    return 0;
}

int wellbeing_homeostasis_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_homeostasis_training_end: NULL argument");
        return -1;
    }
    wellbeing_homeostasis_heartbeat_instance(g_wellbeing_homeostasis_health_agent, "wellbeing_homeostasis_training_end", 1.0f);
    return 0;
}

int wellbeing_homeostasis_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_homeostasis_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_homeostasis_heartbeat_instance(g_wellbeing_homeostasis_health_agent, "wellbeing_homeostasis_training_step", progress);
    return 0;
}
