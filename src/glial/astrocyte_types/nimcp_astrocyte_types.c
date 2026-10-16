/**
 * @file nimcp_astrocyte_types.c
 * @brief Implementation of region-specific astrocyte modulation
 *
 * IMPLEMENTATION NOTES:
 * - All modulation functions are stateless (read-only access to astrocyte/synapse)
 * - Biological parameters derived from empirical neuroscience literature
 * - Modulation factors designed to be multiplicative (1.0 = no change)
 * - Thread-safe due to read-only access pattern
 *
 * @version 2.7.0 (Phase 8.7)
 */

#include "glial/astrocytes/nimcp_astrocytes.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
// Logging module identifier
#define LOG_MODULE "ASTROCYTE_TYPES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocyte_types)

//=============================================================================
// Constants for Type-Specific Modulation
//=============================================================================

// V1 Sensory Constants
#define V1_ORIENTATION_TUNING_SIGMA_DEG 20.0f    // Tuning width (degrees)
#define V1_CONTRAST_ADAPTATION_RATE 0.3f         // Adaptation strength
#define V1_MAX_ENHANCEMENT 2.0f                   // Max modulation factor
#define V1_MIN_SUPPRESSION 0.5f                   // Min modulation factor

// A1 Sensory Constants
#define A1_FREQUENCY_TUNING_SIGMA_OCTAVES 1.5f   // Tuning width (octaves)
#define A1_ADAPTATION_TAU_MS 200.0f               // Adaptation time constant
#define A1_MAX_ENHANCEMENT 1.8f
#define A1_MIN_SUPPRESSION 0.6f

// Multimodal Integration Constants
#define MULTIMODAL_TEMPORAL_WINDOW_MS 100.0f      // Binding window (gamma period)
#define MULTIMODAL_BASE_ENHANCEMENT 1.2f          // Enhancement for 2 modalities
#define MULTIMODAL_SUPERLINEAR_FACTOR 1.5f        // Superlinear for 3+ modalities
#define MULTIMODAL_MAX_ENHANCEMENT 2.5f

// Metacognitive Constants
#define METACOG_UNCERTAINTY_THRESHOLD 0.5f        // Threshold for enhanced plasticity
#define METACOG_CONFIDENCE_SCALING 0.8f           // Scaling factor for confidence
#define METACOG_ERROR_AMPLIFICATION 1.3f          // Amplification for error trials
#define METACOG_MAX_ENHANCEMENT 1.5f
#define METACOG_MIN_SUPPRESSION 0.8f

// Executive Control Constants
#define EXEC_GOAL_RELEVANCE_THRESHOLD 0.6f        // Threshold for prioritization
#define EXEC_WM_MAINTENANCE_BOOST 1.4f            // Boost for WM-relevant synapses
#define EXEC_DISTRACTOR_SUPPRESSION 0.4f          // Suppression for distractors
#define EXEC_MAX_ENHANCEMENT 2.0f
#define EXEC_MIN_SUPPRESSION 0.3f

// Generic Constants
#define GENERIC_MAX_MODULATION 1.2f               // Mild modulation for generic
#define GENERIC_MIN_MODULATION 0.8f

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute Gaussian tuning curve
 *
 * WHAT: Computes exp(-distance² / (2*sigma²))
 * WHY:  Standard model for orientation/frequency tuning
 * HOW:  Simple Gaussian function
 *
 * @param distance Distance from preferred value
 * @param sigma Tuning width
 * @return Tuning strength (0-1)
 */
static inline float gaussian_tuning(float distance, float sigma)
{
    if (sigma <= 0.0F) {
        return 1.0F;
    }
    float normalized = distance / sigma;
    return expf(-0.5F * normalized * normalized);
}

//=============================================================================
// Type Name Lookup
//=============================================================================

const char* astrocyte_type_get_name(astrocyte_type_t type)
{
    LOG_MODULE_DEBUG(LOG_MODULE, "Getting name for astrocyte type %d", type);

    switch (type) {
        case ASTROCYTE_TYPE_GENERIC:
            return "GENERIC";
        case ASTROCYTE_TYPE_V1_SENSORY:
            return "V1_SENSORY";
        case ASTROCYTE_TYPE_A1_SENSORY:
            return "A1_SENSORY";
        case ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION:
            return "MULTIMODAL_INTEGRATION";
        case ASTROCYTE_TYPE_METACOGNITIVE:
            return "METACOGNITIVE";
        case ASTROCYTE_TYPE_EXECUTIVE_CONTROL:
            return "EXECUTIVE_CONTROL";
        default:
            LOG_MODULE_WARN(LOG_MODULE, "Unknown astrocyte type: %d", type);
            return "UNKNOWN";
    }
}

//=============================================================================
// Context Initialization
//=============================================================================

nimcp_result_t astrocyte_type_init_context(astrocyte_type_context_t* context,
                                            astrocyte_type_t type)
{
    if (!context) {
        LOG_MODULE_ERROR(LOG_MODULE, "NULL context pointer in astrocyte_type_init_context");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Initializing astrocyte type context for type %s", astrocyte_type_get_name(type));

    memset(context, 0, sizeof(astrocyte_type_context_t));
    context->type = type;

    switch (type) {
        case ASTROCYTE_TYPE_V1_SENSORY:
            // Initialize V1 sensory context with default orientation tuning
            context->v1.preferred_orientation = 0.0F;  // Will be set by brain region
            context->v1.orientation_selectivity = V1_ORIENTATION_TUNING_SIGMA_DEG;
            context->v1.contrast_adaptation_state = 0.0F;
            context->v1.local_contrast = 0.5F;
            break;

        case ASTROCYTE_TYPE_A1_SENSORY:
            // Initialize A1 sensory context with default frequency tuning
            context->a1.preferred_frequency_hz = 1000.0F;  // Will be set by tonotopic map
            context->a1.frequency_bandwidth_octaves = A1_FREQUENCY_TUNING_SIGMA_OCTAVES;
            context->a1.adaptation_timescale_ms = A1_ADAPTATION_TAU_MS;
            context->a1.current_frequency_input = 0.0F;
            break;

        case ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION:
            // Initialize multimodal integration context
            context->multimodal.num_modalities_active = 0;
            context->multimodal.temporal_binding_window_ms = MULTIMODAL_TEMPORAL_WINDOW_MS;
            context->multimodal.cross_modal_enhancement = MULTIMODAL_BASE_ENHANCEMENT;
            context->multimodal.last_binding_event_us = 0;
            break;

        case ASTROCYTE_TYPE_METACOGNITIVE:
            // Initialize metacognitive context
            context->metacognitive.uncertainty_level = 0.0F;
            context->metacognitive.confidence_threshold = METACOG_UNCERTAINTY_THRESHOLD;
            context->metacognitive.error_signal = 0.0F;
            context->metacognitive.conflict_detection_strength = 0.0F;
            break;

        case ASTROCYTE_TYPE_EXECUTIVE_CONTROL:
            // Initialize executive control context
            context->executive.goal_relevance = 0.0F;
            context->executive.working_memory_load = 0.0F;
            context->executive.distractor_suppression = 0.0F;
            context->executive.task_priority = 0.5F;
            break;

        case ASTROCYTE_TYPE_GENERIC:
        default:
            // Generic astrocyte - no special initialization needed
            break;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// V1 Sensory Modulation
//=============================================================================

float astrocyte_type_v1_modulate(struct astrocyte_t* astro,
                                  struct synapse_t* synapse,
                                  astrocyte_type_context_t* context)
{
    /**
     * WHAT: Implements orientation-selective and contrast-adaptive modulation
     * WHY:  V1 astrocytes enhance responses to preferred orientations and
     *       implement contrast gain control to prevent saturation
     * HOW:
     *   1. Compute orientation tuning (Gaussian centered on preferred)
     *   2. Apply contrast adaptation (reduces gain for high contrast)
     *   3. Combine factors multiplicatively
     *   4. Scale by astrocyte calcium level (activity-dependent)
     */

    if (!astro || !synapse || !context) {
        return 1.0F;  // No modulation if invalid input
    }

    // Base modulation from astrocyte calcium concentration
    // Higher calcium = stronger modulation
    float calcium_factor = 1.0F;
    if (astro->calcium_concentration > astro->calcium_baseline) {
        float calcium_excess = astro->calcium_concentration - astro->calcium_baseline;
        calcium_factor = 1.0F + 0.5F * tanhf(calcium_excess / 2.0F);
    }

    // Orientation tuning modulation
    // Note: In real implementation, synapse would have orientation tag
    // For now, we use a placeholder based on synapse strength
    float synapse_orientation = fmodf(synapse->strength * 180.0F, 180.0F);
    float orientation_distance = fabsf(synapse_orientation - context->v1.preferred_orientation);

    // Handle circular orientation space (180° = 0°)
    if (orientation_distance > 90.0F) {
        orientation_distance = 180.0F - orientation_distance;
    }

    float orientation_tuning = gaussian_tuning(orientation_distance,
                                                 context->v1.orientation_selectivity);

    // Contrast adaptation: reduce gain for high contrast
    float contrast_factor = 1.0F - context->v1.contrast_adaptation_state * V1_CONTRAST_ADAPTATION_RATE;
    contrast_factor = nimcp_clampf(contrast_factor, 0.5F, 1.0F);

    // Combine all factors
    float modulation = calcium_factor * orientation_tuning * contrast_factor;

    // Clamp to biological range
    return nimcp_clampf(modulation, V1_MIN_SUPPRESSION, V1_MAX_ENHANCEMENT);
}

//=============================================================================
// A1 Sensory Modulation
//=============================================================================

float astrocyte_type_a1_modulate(struct astrocyte_t* astro,
                                  struct synapse_t* synapse,
                                  astrocyte_type_context_t* context)
{
    /**
     * WHAT: Implements frequency-selective tonotopic modulation
     * WHY:  A1 astrocytes normalize responses across frequency bands
     * HOW:
     *   1. Compute log-frequency distance (octaves)
     *   2. Apply log-Gaussian tuning curve
     *   3. Implement adaptation to sustained tones
     *   4. Modulate based on astrocyte state
     */

    if (!astro || !synapse || !context) {
        return 1.0F;
    }

    // Base modulation from astrocyte calcium
    float calcium_factor = 1.0F;
    if (astro->calcium_concentration > astro->calcium_baseline) {
        float calcium_excess = astro->calcium_concentration - astro->calcium_baseline;
        calcium_factor = 1.0F + 0.4F * tanhf(calcium_excess / 2.0F);
    }

    // Frequency tuning in log space (octaves)
    // Note: In real implementation, synapse would have frequency tag
    float synapse_frequency = 200.0F + synapse->strength * 5000.0F;  // Placeholder

    if (synapse_frequency <= 0.0F || context->a1.preferred_frequency_hz <= 0.0F) {
        return 1.0F;
    }

    float frequency_ratio = synapse_frequency / context->a1.preferred_frequency_hz;
    float octave_distance = fabsf(log2f(frequency_ratio));

    float frequency_tuning = gaussian_tuning(octave_distance,
                                              context->a1.frequency_bandwidth_octaves);

    // Adaptation to sustained tones
    // Higher current_frequency_input = more adaptation
    float adaptation_factor = 1.0F - 0.4F * tanhf(context->a1.current_frequency_input / 5.0F);
    adaptation_factor = nimcp_clampf(adaptation_factor, 0.6F, 1.0F);

    // Combine factors
    float modulation = calcium_factor * frequency_tuning * adaptation_factor;

    return nimcp_clampf(modulation, A1_MIN_SUPPRESSION, A1_MAX_ENHANCEMENT);
}

//=============================================================================
// Multimodal Integration Modulation
//=============================================================================

float astrocyte_type_multimodal_modulate(struct astrocyte_t* astro,
                                          struct synapse_t* synapse,
                                          astrocyte_type_context_t* context)
{
    /**
     * WHAT: Enhances synapses when multiple sensory modalities are active
     * WHY:  Superior colliculus astrocytes coordinate multisensory binding
     * HOW:
     *   1. Detect number of active modalities
     *   2. Apply superlinear enhancement for 3+ modalities
     *   3. Enforce temporal binding window
     *   4. Trigger calcium wave for strong multimodal events
     */

    if (!astro || !synapse || !context) {
        return 1.0F;
    }

    // Base modulation from astrocyte calcium
    float calcium_factor = 1.0F;
    if (astro->calcium_concentration > astro->calcium_baseline) {
        float calcium_excess = astro->calcium_concentration - astro->calcium_baseline;
        calcium_factor = 1.0F + 0.6F * tanhf(calcium_excess / 2.0F);
    }

    // Multimodal enhancement based on number of active modalities
    float multimodal_enhancement = 1.0F;

    if (context->multimodal.num_modalities_active >= 2) {
        // Linear enhancement for 2 modalities
        multimodal_enhancement = MULTIMODAL_BASE_ENHANCEMENT;

        // Superlinear enhancement for 3+ modalities
        if (context->multimodal.num_modalities_active >= 3) {
            multimodal_enhancement *= MULTIMODAL_SUPERLINEAR_FACTOR;
        }
    }

    // Temporal binding window enforcement
    // Enhancement only applies if within temporal window of last binding event
    // (In full implementation, this would check timestamps)
    float temporal_factor = 1.0F;
    if (context->multimodal.last_binding_event_us > 0) {
        // Placeholder: assume within window if last_binding_event is set
        temporal_factor = context->multimodal.cross_modal_enhancement;
    }

    // Combine factors
    float modulation = calcium_factor * multimodal_enhancement * temporal_factor;

    return nimcp_clampf(modulation, 1.0F, MULTIMODAL_MAX_ENHANCEMENT);
}

//=============================================================================
// Metacognitive Modulation
//=============================================================================

float astrocyte_type_metacognitive_modulate(struct astrocyte_t* astro,
                                             struct synapse_t* synapse,
                                             astrocyte_type_context_t* context)
{
    /**
     * WHAT: Modulates plasticity based on uncertainty and confidence
     * WHY:  PFC astrocytes support error monitoring and metacognition
     * HOW:
     *   1. Increase D-serine release for uncertain/novel inputs
     *   2. Decrease modulation for confident/familiar inputs
     *   3. Amplify error signals for conflict detection
     *   4. Support metacognitive monitoring
     */

    if (!astro || !synapse || !context) {
        return 1.0F;
    }

    // Base modulation from astrocyte calcium and D-serine pool
    float calcium_factor = 1.0F;
    if (astro->calcium_concentration > astro->calcium_baseline) {
        float calcium_excess = astro->calcium_concentration - astro->calcium_baseline;
        calcium_factor = 1.0F + 0.3F * tanhf(calcium_excess / 2.0F);
    }

    // D-serine availability (required for NMDA-dependent plasticity)
    float d_serine_factor = 0.5F + 0.5F * astro->d_serine_pool;

    // Uncertainty-driven modulation
    // Higher uncertainty = stronger modulation (enhance plasticity for novel inputs)
    float uncertainty_factor = 1.0F;
    if (context->metacognitive.uncertainty_level > context->metacognitive.confidence_threshold) {
        float uncertainty_excess = context->metacognitive.uncertainty_level -
                                    context->metacognitive.confidence_threshold;
        uncertainty_factor = 1.0F + 0.5F * uncertainty_excess;
    }

    // Error signal amplification
    float error_factor = 1.0F;
    if (context->metacognitive.error_signal > 0.5F) {
        error_factor = METACOG_ERROR_AMPLIFICATION;
    }

    // Conflict detection enhancement
    float conflict_factor = 1.0F + 0.3F * context->metacognitive.conflict_detection_strength;

    // Combine factors
    float modulation = calcium_factor * d_serine_factor * uncertainty_factor *
                       error_factor * conflict_factor;

    return nimcp_clampf(modulation, METACOG_MIN_SUPPRESSION, METACOG_MAX_ENHANCEMENT);
}

//=============================================================================
// Executive Control Modulation
//=============================================================================

float astrocyte_type_executive_modulate(struct astrocyte_t* astro,
                                         struct synapse_t* synapse,
                                         astrocyte_type_context_t* context)
{
    /**
     * WHAT: Implements goal-directed metabolic support
     * WHY:  DLPFC astrocytes prioritize task-relevant neurons
     * HOW:
     *   1. Assess goal relevance of synapse
     *   2. Allocate ATP preferentially to relevant synapses
     *   3. Suppress distractors via reduced glutamate
     *   4. Maintain working memory via persistent calcium
     */

    if (!astro || !synapse || !context) {
        return 1.0F;
    }

    // Base modulation from astrocyte calcium and ATP level
    float calcium_factor = 1.0F;
    if (astro->calcium_concentration > astro->calcium_baseline) {
        float calcium_excess = astro->calcium_concentration - astro->calcium_baseline;
        calcium_factor = 1.0F + 0.5F * tanhf(calcium_excess / 2.0F);
    }

    // ATP-dependent metabolic support
    // Low ATP reduces modulation capacity
    float atp_factor = 0.5F + 0.5F * astro->atp_level;

    // Goal relevance modulation
    float goal_factor = 1.0F;
    if (context->executive.goal_relevance > EXEC_GOAL_RELEVANCE_THRESHOLD) {
        // Enhance goal-relevant synapses
        goal_factor = 1.0F + (context->executive.goal_relevance - EXEC_GOAL_RELEVANCE_THRESHOLD) * 2.0F;
    } else if (context->executive.goal_relevance < 0.3F) {
        // Suppress irrelevant synapses (distractors)
        goal_factor = EXEC_DISTRACTOR_SUPPRESSION;
    }

    // Working memory maintenance boost
    float wm_factor = 1.0F;
    if (context->executive.working_memory_load > 0.5F) {
        wm_factor = EXEC_WM_MAINTENANCE_BOOST;
    }

    // Task priority scaling
    float priority_factor = 0.5F + context->executive.task_priority;

    // Distractor suppression
    float distractor_factor = 1.0F - context->executive.distractor_suppression;
    distractor_factor = nimcp_clampf(distractor_factor, 0.3F, 1.0F);

    // Combine factors
    float modulation = calcium_factor * atp_factor * goal_factor *
                       wm_factor * priority_factor * distractor_factor;

    return nimcp_clampf(modulation, EXEC_MIN_SUPPRESSION, EXEC_MAX_ENHANCEMENT);
}

//=============================================================================
// Generic Modulation
//=============================================================================

float astrocyte_type_generic_modulate(struct astrocyte_t* astro,
                                       struct synapse_t* synapse,
                                       astrocyte_type_context_t* context)
{
    /**
     * WHAT: Implements basic calcium-dependent modulation
     * WHY:  Fallback for non-specialized regions
     * HOW:  Simple glutamate release scaling with calcium
     */

    if (!astro || !synapse) {
        return 1.0F;
    }

    // Simple calcium-dependent modulation
    float modulation = 1.0F;

    if (astro->calcium_concentration > astro->calcium_baseline) {
        float calcium_excess = astro->calcium_concentration - astro->calcium_baseline;
        modulation = 1.0F + 0.2F * tanhf(calcium_excess / 3.0F);
    }

    // Include glutamate pool depletion
    modulation *= (0.5F + 0.5F * astro->glutamate_pool);

    return nimcp_clampf(modulation, GENERIC_MIN_MODULATION, GENERIC_MAX_MODULATION);
}

//=============================================================================
// Type Dispatch Function
//=============================================================================

float astrocyte_type_dispatch_modulation(struct astrocyte_t* astro,
                                          struct synapse_t* synapse,
                                          astrocyte_type_context_t* context)
{
    /**
     * WHAT: Routes to type-specific modulation function
     * WHY:  Single entry point for all astrocyte modulation
     * HOW:  Switch on context->type
     *
     * PERFORMANCE: O(1) switch statement (could use function pointers)
     */

    if (!astro || !synapse || !context) {
        return 1.0F;
    }

    switch (context->type) {
        case ASTROCYTE_TYPE_V1_SENSORY:
            return astrocyte_type_v1_modulate(astro, synapse, context);

        case ASTROCYTE_TYPE_A1_SENSORY:
            return astrocyte_type_a1_modulate(astro, synapse, context);

        case ASTROCYTE_TYPE_MULTIMODAL_INTEGRATION:
            return astrocyte_type_multimodal_modulate(astro, synapse, context);

        case ASTROCYTE_TYPE_METACOGNITIVE:
            return astrocyte_type_metacognitive_modulate(astro, synapse, context);

        case ASTROCYTE_TYPE_EXECUTIVE_CONTROL:
            return astrocyte_type_executive_modulate(astro, synapse, context);

        case ASTROCYTE_TYPE_GENERIC:
        default:
            return astrocyte_type_generic_modulate(astro, synapse, context);
    }
}
