//=============================================================================
// nimcp_brain_accessors.c - Brain Subsystem Accessor Functions Implementation
//=============================================================================
/**
 * @file nimcp_brain_accessors.c
 * @brief Implementation of brain subsystem accessor functions
 *
 * WHAT: Provides safe access to brain subsystem components
 * WHY:  Decouples brain internals from external modules
 * HOW:  NULL-checked accessors with error handling
 *
 * EXTRACTED FROM: nimcp_brain.c (lines 5097-5447)
 * DATE: 2025-11-19
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/accessors/nimcp_brain_accessors.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stddef.h>

#define LOG_MODULE "BRAIN_ACCESSORS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_accessors, MESH_ADAPTER_CATEGORY_COGNITIVE)


// External error handling functions from nimcp_brain.c
extern void set_error(const char* format, ...);
extern void brain_clear_error(void);

//=============================================================================
// Simple Subsystem Accessors
//=============================================================================

glial_integration_t* brain_get_glial(brain_t brain) {
    return brain ? brain->glial : NULL;
}

myelin_sheath_network_t* brain_get_myelin_sheath(brain_t brain) {
    return brain ? brain->myelin_sheath : NULL;
}

brain_oscillation_analyzer_t* brain_get_oscillations(brain_t brain) {
    return brain ? brain->oscillations : NULL;
}

introspection_context_t brain_get_introspection(brain_t brain) {
    return brain ? brain->introspection : NULL;
}

ethics_engine_t brain_get_ethics(brain_t brain) {
    return brain ? brain->ethics : NULL;
}

salience_evaluator_t brain_get_salience(brain_t brain) {
    return brain ? brain->salience : NULL;
}

consolidation_handle_t brain_get_consolidation(brain_t brain) {
    return brain ? brain->consolidation : NULL;
}

curiosity_engine_t brain_get_curiosity(brain_t brain) {
    return brain ? brain->curiosity : NULL;
}

knowledge_system_t brain_get_knowledge(brain_t brain) {
    return brain ? brain->knowledge : NULL;
}

neural_logic_network_t brain_get_logic(brain_t brain) {
    return brain ? brain->logic : NULL;
}

symbolic_logic_t* brain_get_symbolic_logic(brain_t brain) {
    return brain ? brain->symbolic_logic : NULL;
}

void* brain_get_pink_noise(brain_t brain) {
    return brain ? (void*)brain->pink_noise : NULL;
}

//=============================================================================
// Mirror Neuron & Empathy Functions
//=============================================================================

/**
 * @brief Get mirror neuron activations for Theory of Mind integration
 *
 * WHAT: Extract current mirror neuron activation pattern
 * WHY:  Enable ToM to infer agent intentions from observed actions
 * HOW:  Query mirror neuron system for all current activations
 *
 * BIOLOGICAL RATIONALE:
 * Mirror neurons in premotor cortex (F5) and inferior parietal lobule (IPL)
 * fire both during action execution and observation (Rizzolatti & Craighero, 2004).
 * This activation pattern enables Theory of Mind to infer "why" an agent is
 * performing an action, supporting empathy and social cognition.
 *
 * COMPLEXITY: O(n) where n = number of learned actions
 *
 * @param brain Brain handle
 * @param activations Output buffer (must be allocated by caller)
 * @param max_size Buffer size (number of floats)
 * @param out_size Actual number of activations returned
 * @return true on success, false on error
 */
bool brain_get_mirror_activations(brain_t brain, float* activations,
                                  uint32_t max_size, uint32_t* out_size)
{
    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_mirror_activations: brain is NULL");
        set_error("brain_get_mirror_activations: NULL brain");
        return false;
    }

    if (!activations || !out_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_mirror_activations: output parameters are NULL");
        set_error("brain_get_mirror_activations: NULL output parameters");
        return false;
    }

    if (max_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_mirror_activations: max_size must be > 0");
        set_error("brain_get_mirror_activations: max_size must be > 0");
        return false;
    }

    // Guard: Check if mirror neurons enabled
    if (!brain->config.enable_mirror_neurons || !brain->mirror_neurons) {
        set_error("brain_get_mirror_activations: mirror neurons not enabled");
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_mirror_activations: required parameter is NULL (brain->config, brain->mirror_neurons)");
        return false;
    }

    // Extract activations from mirror neuron system
    bool success = mirror_neurons_get_all_activations(
        brain->mirror_neurons,
        activations,
        max_size,
        out_size
    );

    if (!success) {
        set_error("brain_get_mirror_activations: failed to extract activations");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_mirror_activations: success is NULL");
        return false;
    }

    brain_clear_error();
    return true;
}

/**
 * @brief Compute empathy response from mirror neuron activations
 *
 * WHAT: Generate empathetic emotional response from observed behavior
 * WHY:  Mirror neurons enable emotional contagion and empathy
 * HOW:  Observe action → activate mirror neurons → infer emotion → empathy
 *
 * BIOLOGICAL RATIONALE:
 * Empathy emerges from shared representations in mirror neuron system
 * (Preston & de Waal, 2002). When observing others' emotional expressions:
 * 1. Mirror neurons activate (action observation)
 * 2. Emotional system infers observed emotion
 * 3. Empathetic response generated (emotional contagion)
 *
 * This models the perception-action model of empathy.
 *
 * COMPLEXITY: O(n) where n = num_features
 *
 * @param brain Brain handle
 * @param observed_features Observed behavior features
 * @param num_features Number of features
 * @param empathy_valence Output: valence (-1 to 1)
 * @param empathy_arousal Output: arousal (0 to 1)
 * @param empathy_confidence Output: confidence (0 to 1)
 * @return true on success
 */
bool brain_compute_empathy(brain_t brain, const float* observed_features,
                          uint32_t num_features, float* empathy_valence,
                          float* empathy_arousal, float* empathy_confidence)
{
    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_compute_empathy: brain is NULL");
        set_error("brain_compute_empathy: NULL brain");
        return false;
    }

    if (!observed_features || num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_compute_empathy: invalid features");
        set_error("brain_compute_empathy: invalid features");
        return false;
    }

    if (!empathy_valence || !empathy_arousal || !empathy_confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_compute_empathy: output parameters are NULL");
        set_error("brain_compute_empathy: NULL output parameters");
        return false;
    }

    // Guard: Check required subsystems
    if (!brain->config.enable_mirror_neurons || !brain->mirror_neurons) {
        set_error("brain_compute_empathy: mirror neurons not enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_compute_empathy: required parameter is NULL (brain->config, brain->mirror_neurons)");
        return false;
    }

    // Step 1: Get mirror neuron activations
    float mirror_activations[100];
    uint32_t num_activations = 0;

    bool success = mirror_neurons_get_all_activations(
        brain->mirror_neurons,
        mirror_activations,
        100,
        &num_activations
    );

    if (!success || num_activations == 0) {
        // No mirror neuron activity - neutral empathy
        *empathy_valence = 0.0F;
        *empathy_arousal = 0.0F;
        *empathy_confidence = 0.0F;
        brain_clear_error();
        return true;
    }

    // Step 2: Compute empathy from mirror activations
    // WHAT: Map mirror neuron pattern to emotional response
    // WHY:  Shared representations enable emotional understanding
    // HOW:  Aggregate activations → infer valence/arousal

    float total_activation = 0.0F;
    float weighted_valence = 0.0F;

    // Use observed features to influence empathy calculation
    float feature_sum = 0.0F;
    for (uint32_t i = 0; i < num_features && i < 8; i++) {
        feature_sum += observed_features[i];
    }
    float feature_mean = num_features > 0 ? feature_sum / num_features : 0.5F;

    for (uint32_t i = 0; i < num_activations; i++) {
        total_activation += mirror_activations[i];
        // Differentiate valence based on feature patterns and activations
        // Higher feature values (joy) → positive valence
        // Lower feature values (distress) → negative valence
        float valence_sign = (feature_mean > 0.5F) ? 1.0F : -1.0F;
        weighted_valence += mirror_activations[i] * valence_sign * (0.5F + 0.5F * fabsf(feature_mean - 0.5F));
    }

    // Normalize
    if (num_activations > 0) {
        *empathy_arousal = total_activation / num_activations;
        *empathy_valence = weighted_valence / total_activation;
        *empathy_confidence = total_activation > 0.1F ? 0.7F : 0.3F;

        // Clamp to valid ranges
        if (*empathy_valence < -1.0F) *empathy_valence = -1.0F;
        if (*empathy_valence > 1.0F) *empathy_valence = 1.0F;
        if (*empathy_arousal < 0.0F) *empathy_arousal = 0.0F;
        if (*empathy_arousal > 1.0F) *empathy_arousal = 1.0F;
        if (*empathy_confidence < 0.0F) *empathy_confidence = 0.0F;
        if (*empathy_confidence > 1.0F) *empathy_confidence = 1.0F;
    } else {
        *empathy_valence = 0.0F;
        *empathy_arousal = 0.0F;
        *empathy_confidence = 0.0F;
    }

    // Step 3: Integrate with emotional system if available
    if (brain->config.enable_emotional_tagging && brain->emotional_system) {
        // Future enhancement: Query emotional system for better inference
        // emotional_system_infer_from_observation(brain->emotional_system, ...)
    }

    brain_clear_error();
    return true;
}

//=============================================================================
// Astrocyte High-Level API
//=============================================================================

bool brain_enable_astrocytes(brain_t brain, uint32_t num_astrocytes, float coverage_radius_um) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_enable_astrocytes: brain is NULL");
        set_error("brain_enable_astrocytes: NULL brain");
        return false;
    }

    if (!brain->config.enable_glial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "brain_enable_astrocytes: enable_glial must be true in config");
        set_error("brain_enable_astrocytes: enable_glial must be true in config");
        return false;
    }

    if (!brain->glial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "brain_enable_astrocytes: glial integration system not initialized");
        set_error("brain_enable_astrocytes: glial integration system not initialized");
        return false;
    }

    // Auto-calculate num_astrocytes if not specified (biological ratio: 1 per 10-20 synapses)
    if (num_astrocytes == 0) {
        // Use network regions configuration or default heuristic
        if (brain->config.enable_brain_regions && brain->config.num_brain_regions > 0) {
            // Estimate based on regions: 1 astrocyte per ~15 neurons
            uint32_t total_neurons = brain->config.num_brain_regions * brain->config.neurons_per_region;
            num_astrocytes = (total_neurons / 15) + 1;
        } else {
            // Default heuristic for non-region based brains
            num_astrocytes = 100;  // Conservative default
        }

        // Clamp to reasonable range
        if (num_astrocytes < 10) num_astrocytes = 10;
        if (num_astrocytes > 10000) num_astrocytes = 10000;
    }

    // Set default coverage radius if not specified
    if (coverage_radius_um <= 0.0F) {
        coverage_radius_um = 75.0F;  // Typical: 50-100 µm
    }

    // Step 1: Create astrocyte network
    astrocyte_network_t* astro_net = astrocyte_network_create(num_astrocytes);
    if (!astro_net) {
        set_error("brain_enable_astrocytes: failed to create astrocyte network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_enable_astrocytes: astro_net is NULL");
        return false;
    }

    // Step 2: Create calcium system
    astrocyte_calcium_system_t* ca_sys = astrocyte_calcium_system_create(astro_net);
    if (!ca_sys) {
        astrocyte_network_destroy(astro_net);
        set_error("brain_enable_astrocytes: failed to create calcium system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_enable_astrocytes: ca_sys is NULL");
        return false;
    }

    // Step 3: Assign to glial integration
    nimcp_result_t result = glial_integration_set_astrocyte_network(brain->glial, astro_net);
    if (result != NIMCP_SUCCESS) {
        astrocyte_calcium_system_destroy(ca_sys);
        astrocyte_network_destroy(astro_net);
        set_error("brain_enable_astrocytes: failed to assign astrocyte network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_enable_astrocytes: validation failed");
        return false;
    }

    // Step 4: Auto-assign astrocytes to synapses (spatial proximity)
    uint32_t assignments = glial_integration_auto_assign_spatial(brain->glial);
    // Note: assignments may be 0 if network doesn't have spatial positions yet
    (void)assignments; // Suppress unused variable warning

    // Step 5: Enable astrocyte modulation
    glial_integration_set_astrocyte_modulation_enabled(brain->glial, true);

    brain_clear_error();
    return true;
}

bool brain_get_astrocyte_stats(brain_t brain, astrocyte_stats_t* stats) {
    if (!brain || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_astrocyte_stats: brain or stats is NULL");
        set_error("brain_get_astrocyte_stats: NULL parameter");
        return false;
    }

    if (!brain->glial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "brain_get_astrocyte_stats: glial integration not initialized");
        set_error("brain_get_astrocyte_stats: glial integration not initialized");
        return false;
    }

    // Get glial integration stats
    glial_integration_stats_t gi_stats;
    nimcp_result_t result = glial_integration_get_stats(brain->glial, &gi_stats);
    if (result != NIMCP_SUCCESS) {
        set_error("brain_get_astrocyte_stats: failed to get glial stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_astrocyte_stats: validation failed");
        return false;
    }

    // Map to astrocyte-specific stats
    stats->num_astrocytes = gi_stats.num_astrocytes;
    stats->avg_calcium_um = 0.0F;  // Will be computed if calcium system exists
    stats->num_tripartite_synapses = gi_stats.num_tripartite_synapses;
    stats->total_modulations = gi_stats.total_modulations;
    stats->avg_modulation_strength = gi_stats.avg_synaptic_modulation;

    // TODO: Get average calcium from astrocyte calcium system when API available
    // For now, use a placeholder value based on whether modulation is active
    if (gi_stats.num_astrocytes > 0 && gi_stats.total_modulations > 0) {
        // Estimate average calcium based on modulation activity
        // Active modulation suggests elevated calcium (0.2-0.5 µM)
        stats->avg_calcium_um = 0.3F;
    } else {
        // Baseline calcium (~0.1 µM)
        stats->avg_calcium_um = 0.1F;
    }

    brain_clear_error();
    return true;
}
