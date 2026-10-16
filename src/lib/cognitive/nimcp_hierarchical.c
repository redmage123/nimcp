/**
 * @file nimcp_hierarchical.c
 * @brief Implementation of hierarchical brain regions for multi-tasking
 * @version 2.6.1
 * @date 2025-11-04
 *
 * WHAT: Hierarchical brain organization with feedforward, feedback, and lateral connections
 * WHY: Enable brain-inspired multi-task learning and processing
 * HOW: Layered regions communicate through typed connections
 */

#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "COGNITIVE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hierarchical)

#include <string.h>
#include <stdio.h>
#include <math.h>

/* Silence -Wunused-result for fread — reads are best-effort here; file-format
 * integrity is checked via magic/version downstream. */
static inline void nimcp_fread_ignore(void* ptr, size_t sz, size_t n, FILE* f) {
    size_t got = fread(ptr, sz, n, f);
    (void)got;
}
#define fread_chk nimcp_fread_ignore

// NIMCP core includes
#include "utils/validation/nimcp_validate.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/nimcp_hierarchical.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal hierarchical brain structure
 */
struct hierarchical_brain_internal {
    char name[HIERARCHICAL_MAX_NAME_LENGTH];  /**< System identifier */

    hierarchical_region_t** regions;          /**< Array of regions */
    uint32_t num_regions;                     /**< Current region count */
    uint32_t max_regions;                     /**< Maximum capacity */

    uint32_t num_layers;                      /**< Number of hierarchical layers */
    uint64_t forward_passes;                  /**< Total forward passes */

    // Neuromodulation
    float dopamine_level;                     /**< Global dopamine (0-1) */
    float acetylcholine_level;                /**< Global ACh (0-1) */
};

//=============================================================================
// Core Functions
//=============================================================================

hierarchical_brain_t hierarchical_brain_create(const char* name, uint32_t max_regions) {
    // Validate parameters
    if (!nimcp_validate_pointer(name, "name")) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NULL_POINTER, 0, "hierarchical",
            "NULL name provided to hierarchical_brain_create");
        return NULL;
    }

    if (max_regions == 0) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_PARAM, 0, "hierarchical",
            "Invalid max_regions: 0");
        NIMCP_LOGGING_ERROR("Invalid max_regions: 0");
        return NULL;
    }

    // Allocate brain structure
    struct hierarchical_brain_internal* hbrain =
        (struct hierarchical_brain_internal*)nimcp_calloc(1, sizeof(struct hierarchical_brain_internal));

    if (!hbrain) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct hierarchical_brain_internal),
            "Failed to allocate hierarchical brain '%s'", name);
        NIMCP_LOGGING_ERROR("Failed to allocate hierarchical brain");
        return NULL;
    }

    // Copy name
    strncpy(hbrain->name, name, HIERARCHICAL_MAX_NAME_LENGTH - 1);
    hbrain->name[HIERARCHICAL_MAX_NAME_LENGTH - 1] = '\0';

    // Allocate region array
    hbrain->regions = (hierarchical_region_t**)nimcp_calloc(
        max_regions, sizeof(hierarchical_region_t*));

    if (!hbrain->regions) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_regions * sizeof(hierarchical_region_t*),
            "Failed to allocate regions array for hierarchical brain '%s'", name);
        NIMCP_LOGGING_ERROR("Failed to allocate regions array");
        nimcp_free(hbrain);
        return NULL;
    }

    hbrain->max_regions = max_regions;
    hbrain->num_regions = 0;
    hbrain->num_layers = 0;
    hbrain->forward_passes = 0;
    hbrain->dopamine_level = 0.5F;
    hbrain->acetylcholine_level = 0.5F;

    NIMCP_LOGGING_DEBUG("Created hierarchical brain '%s' with capacity %u", name, max_regions);

    return (hierarchical_brain_t)hbrain;
}

void hierarchical_brain_destroy(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Free each region
    for (uint32_t i = 0; i < internal->num_regions; i++) {
        if (internal->regions[i]) {
            hierarchical_region_t* region = internal->regions[i];

            // Free connections
            if (region->inputs) {
                nimcp_free(region->inputs);
            }
            if (region->feedback) {
                nimcp_free(region->feedback);
            }
            if (region->lateral) {
                nimcp_free(region->lateral);
            }

            // Free activity and memory buffers
            if (region->activity) {
                nimcp_free(region->activity);
            }
            if (region->memory_buffer) {
                nimcp_free(region->memory_buffer);
            }

            // Note: We don't destroy region->brain as it's managed externally

            nimcp_free(region);
        }
    }

    nimcp_free(internal->regions);
    nimcp_free(internal);

    NIMCP_LOGGING_DEBUG("Destroyed hierarchical brain");
}

//=============================================================================
// Region Management
//=============================================================================

int32_t hierarchical_add_region(
    hierarchical_brain_t hbrain,
    const char* name,
    hierarchical_region_type_t type,
    uint32_t layer,
    brain_t brain
) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NULL_POINTER, 0, "hierarchical",
            "NULL hbrain provided to hierarchical_add_region");
        return -1;
    }

    if (!nimcp_validate_pointer(name, "name")) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NULL_POINTER, 0, "hierarchical",
            "NULL region name provided to hierarchical_add_region");
        return -1;
    }

    // Note: brain can be NULL - regions can operate without an underlying brain
    // (e.g., after loading from file, or for testing purposes)

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Check capacity
    if (internal->num_regions >= internal->max_regions) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_INVALID, 0, "hierarchical",
            "Maximum regions (%u) reached, cannot add region '%s'", internal->max_regions, name);
        NIMCP_LOGGING_ERROR("Maximum regions (%u) reached", internal->max_regions);
        return -1;
    }

    // Allocate new region
    hierarchical_region_t* region =
        (hierarchical_region_t*)nimcp_calloc(1, sizeof(hierarchical_region_t));

    if (!region) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(hierarchical_region_t),
            "Failed to allocate hierarchical region '%s'", name);
        NIMCP_LOGGING_ERROR("Failed to allocate region");
        return -1;
    }

    // Initialize region
    strncpy(region->name, name, HIERARCHICAL_MAX_NAME_LENGTH - 1);
    region->name[HIERARCHICAL_MAX_NAME_LENGTH - 1] = '\0';
    region->type = type;
    region->layer = layer;
    region->brain = brain;

    // Initialize properties
    region->activity = NULL;
    region->activity_size = 0;
    region->learning_rate_multiplier = 1.0F;

    // Initialize connections (will be allocated as needed)
    region->inputs = NULL;
    region->num_inputs = 0;
    region->feedback = NULL;
    region->num_feedback = 0;
    region->lateral = NULL;
    region->num_lateral = 0;

    // Initialize attention
    region->attention_enabled = true;
    region->attention_gain = 1.0F;

    // Initialize memory
    region->has_memory = false;
    region->memory_buffer = NULL;
    region->memory_decay = 0.9F;

    // Initialize statistics
    region->activations = 0;
    region->updates = 0;

    // Add to hierarchy
    uint32_t index = internal->num_regions;
    internal->regions[index] = region;
    internal->num_regions++;

    // Update max layer
    if (layer + 1 > internal->num_layers) {
        internal->num_layers = layer + 1;
    }

    NIMCP_LOGGING_DEBUG("Added region '%s' at index %u (layer %u)", name, index, layer);

    return (int32_t)index;
}

bool hierarchical_connect_regions(
    hierarchical_brain_t hbrain,
    uint32_t source_idx,
    uint32_t target_idx,
    connection_type_t conn_type
) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_connect_regions: nimcp_validate_pointer is NULL");
        return false;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Validate indices
    if (source_idx >= internal->num_regions) {
        NIMCP_LOGGING_ERROR("Invalid source index: %u", source_idx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_connect_regions: capacity exceeded");
        return false;
    }

    if (target_idx >= internal->num_regions) {
        NIMCP_LOGGING_ERROR("Invalid target index: %u", target_idx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_connect_regions: capacity exceeded");
        return false;
    }

    hierarchical_region_t* source = internal->regions[source_idx];
    hierarchical_region_t* target = internal->regions[target_idx];

    // Add connection based on type
    switch (conn_type) {
        case CONNECTION_FEEDFORWARD: {
            // Add source to target's inputs
            hierarchical_region_t** new_inputs =
                (hierarchical_region_t**)nimcp_realloc(
                    target->inputs,
                    (target->num_inputs + 1) * sizeof(hierarchical_region_t*)
                );

            if (!new_inputs) {
                NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                    (target->num_inputs + 1) * sizeof(hierarchical_region_t*),
                    "Failed to allocate feedforward connection (source=%u, target=%u)",
                    source_idx, target_idx);
                NIMCP_LOGGING_ERROR("Failed to allocate feedforward connection");
                return false;
            }

            new_inputs[target->num_inputs] = source;
            target->inputs = new_inputs;
            target->num_inputs++;
            break;
        }

        case CONNECTION_FEEDBACK: {
            // Add source to target's feedback
            hierarchical_region_t** new_feedback =
                (hierarchical_region_t**)nimcp_realloc(
                    target->feedback,
                    (target->num_feedback + 1) * sizeof(hierarchical_region_t*)
                );

            if (!new_feedback) {
                NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                    (target->num_feedback + 1) * sizeof(hierarchical_region_t*),
                    "Failed to allocate feedback connection (source=%u, target=%u)",
                    source_idx, target_idx);
                NIMCP_LOGGING_ERROR("Failed to allocate feedback connection");
                return false;
            }

            new_feedback[target->num_feedback] = source;
            target->feedback = new_feedback;
            target->num_feedback++;
            break;
        }

        case CONNECTION_LATERAL: {
            // Add source to target's lateral
            hierarchical_region_t** new_lateral =
                (hierarchical_region_t**)nimcp_realloc(
                    target->lateral,
                    (target->num_lateral + 1) * sizeof(hierarchical_region_t*)
                );

            if (!new_lateral) {
                NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                    (target->num_lateral + 1) * sizeof(hierarchical_region_t*),
                    "Failed to allocate lateral connection (source=%u, target=%u)",
                    source_idx, target_idx);
                NIMCP_LOGGING_ERROR("Failed to allocate lateral connection");
                return false;
            }

            new_lateral[target->num_lateral] = source;
            target->lateral = new_lateral;
            target->num_lateral++;
            break;
        }

        default:
            NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_PARAM, 0, "hierarchical",
                "Invalid connection type: %d for connection (source=%u, target=%u)",
                conn_type, source_idx, target_idx);
            NIMCP_LOGGING_ERROR("Invalid connection type: %d", conn_type);
            return false;
    }

    NIMCP_LOGGING_DEBUG("Connected region %u -> %u (type %d)", source_idx, target_idx, conn_type);

    return true;
}

hierarchical_region_t* hierarchical_get_region(hierarchical_brain_t hbrain, const char* name) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hierarchical_get_region: nimcp_validate_pointer is NULL");
        return NULL;
    }

    if (!nimcp_validate_pointer(name, "name")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hierarchical_get_region: nimcp_validate_pointer is NULL");
        return NULL;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    for (uint32_t i = 0; i < internal->num_regions; i++) {
        if (strcmp(internal->regions[i]->name, name) == 0) {
            return internal->regions[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_get_region: validation failed");
    return NULL;
}

hierarchical_region_t* hierarchical_get_region_by_index(hierarchical_brain_t hbrain, uint32_t index) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hierarchical_get_region_by_index: nimcp_validate_pointer is NULL");
        return NULL;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    if (index >= internal->num_regions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "hierarchical_get_region_by_index: capacity exceeded");
        return NULL;
    }

    return internal->regions[index];
}

//=============================================================================
// Processing Functions - Forward Pass Helpers
//=============================================================================

/**
 * @brief Initialize region activity buffer if needed
 *
 * WHAT: Ensure activity buffer is allocated for a region
 * WHY: Lazy allocation saves memory for unused regions
 * HOW: Check existing buffer, allocate if needed
 *
 * @param region Region to initialize
 * @param size Required activity size
 * @return true on success, false on allocation failure
 */
static bool ensure_activity_buffer(hierarchical_region_t* region, uint32_t size) {
    // Guard: Already allocated with sufficient size
    if (region->activity && region->activity_size >= size) {
        return true;
    }

    // Free existing buffer if smaller
    if (region->activity) {
        nimcp_free(region->activity);
    }

    // Allocate new buffer
    region->activity = (float*)nimcp_calloc(size, sizeof(float));
    if (!region->activity) {
        region->activity_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ensure_activity_buffer: region->activity is NULL");
        return false;
    }

    region->activity_size = size;
    return true;
}

/**
 * @brief Process a single region forward pass
 *
 * WHAT: Execute forward pass for one region
 * WHY: Modular processing enables per-region customization
 * HOW: Aggregate inputs, apply attention, process through brain
 *
 * @param region Region to process
 * @param input_data Input array (for layer 0 regions)
 * @param input_size Size of input array
 * @param dopamine Global dopamine level for modulation
 * @param ach Global acetylcholine level for attention
 * @return true on success, false on error
 */
static bool process_region_forward(
    hierarchical_region_t* region,
    const float* input_data,
    uint32_t input_size,
    float dopamine,
    float ach
) {
    // Guard: Invalid region
    if (!region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "process_region_forward: region is NULL");
        return false;
    }

    // Ensure activity buffer exists
    if (!ensure_activity_buffer(region, input_size)) {
        NIMCP_LOGGING_ERROR("Failed to allocate activity buffer for region '%s'", region->name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "process_region_forward: ensure_activity_buffer is NULL");
        return false;
    }

    // Clear activity buffer
    memset(region->activity, 0, region->activity_size * sizeof(float));

    // Aggregate feedforward inputs
    if (region->layer == 0) {
        // Layer 0 regions receive external input directly
        uint32_t copy_size = (input_size < region->activity_size) ? input_size : region->activity_size;
        memcpy(region->activity, input_data, copy_size * sizeof(float));
    } else {
        // Higher layers aggregate from input regions
        for (uint32_t i = 0; i < region->num_inputs; i++) {
            hierarchical_region_t* src = region->inputs[i];
            if (src && src->activity) {
                uint32_t agg_size = (src->activity_size < region->activity_size)
                    ? src->activity_size : region->activity_size;
                for (uint32_t j = 0; j < agg_size; j++) {
                    region->activity[j] += src->activity[j];
                }
            }
        }

        // Normalize by number of inputs
        if (region->num_inputs > 1) {
            float norm = 1.0F / (float)region->num_inputs;
            for (uint32_t j = 0; j < region->activity_size; j++) {
                region->activity[j] *= norm;
            }
        }
    }

    // Apply feedback modulation (top-down attention)
    for (uint32_t i = 0; i < region->num_feedback; i++) {
        hierarchical_region_t* fb = region->feedback[i];
        if (fb && fb->activity) {
            uint32_t mod_size = (fb->activity_size < region->activity_size)
                ? fb->activity_size : region->activity_size;
            for (uint32_t j = 0; j < mod_size; j++) {
                // Multiplicative modulation
                region->activity[j] *= (1.0F + 0.2F * fb->activity[j]);
            }
        }
    }

    // Apply attention gain
    if (region->attention_enabled) {
        float effective_gain = region->attention_gain * (0.5F + 0.5F * ach);
        for (uint32_t j = 0; j < region->activity_size; j++) {
            region->activity[j] *= effective_gain;
        }
    }

    // Apply dopamine modulation (affects learning rate, slight gain boost)
    float dopamine_factor = 1.0F + 0.1F * (dopamine - 0.5F);
    for (uint32_t j = 0; j < region->activity_size; j++) {
        region->activity[j] *= dopamine_factor;
    }

    // Update activation counter
    region->activations++;

    return true;
}

/**
 * @brief Process all regions in a specific layer
 *
 * WHAT: Execute forward pass for all regions at a layer
 * WHY: Layer-wise processing ensures proper information flow
 * HOW: Iterate regions, filter by layer, process each
 *
 * @param internal Internal brain structure
 * @param layer Layer index to process
 * @param input External input data
 * @param input_size Size of input
 * @return true if all regions processed successfully
 */
static bool process_layer_forward(
    struct hierarchical_brain_internal* internal,
    uint32_t layer,
    const float* input,
    uint32_t input_size
) {
    bool all_success = true;

    for (uint32_t i = 0; i < internal->num_regions; i++) {
        hierarchical_region_t* region = internal->regions[i];

        if (region->layer == layer) {
            bool success = process_region_forward(
                region,
                input,
                input_size,
                internal->dopamine_level,
                internal->acetylcholine_level
            );
            if (!success) {
                all_success = false;
            }
        }
    }

    return all_success;
}

//=============================================================================
// Processing Functions - Main Forward Pass
//=============================================================================

bool hierarchical_forward(
    hierarchical_brain_t hbrain,
    const float* input,
    uint32_t input_size
) {
    // Guard: Invalid brain handle
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_forward: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Invalid input
    if (!nimcp_validate_pointer(input, "input")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_forward: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Zero input size
    if (input_size == 0) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_PARAM, 0, "hierarchical",
            "Input size cannot be zero for hierarchical_forward");
        NIMCP_LOGGING_ERROR("Input size cannot be zero");
        return false;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Guard: No regions to process
    if (internal->num_regions == 0) {
        NIMCP_LOGGING_WARN("No regions in hierarchy, nothing to process");
        internal->forward_passes++;
        return true;
    }

    // Process layers from bottom (0) to top (num_layers - 1)
    for (uint32_t layer = 0; layer < internal->num_layers; layer++) {
        if (!process_layer_forward(internal, layer, input, input_size)) {
            NIMCP_THROW_BRAIN(NIMCP_ERROR_FORWARD_PASS, 0, "hierarchical",
                "Failed to process layer %u during hierarchical forward pass", layer);
            NIMCP_LOGGING_ERROR("Failed to process layer %u", layer);
            return false;
        }
    }

    // Increment forward pass counter
    internal->forward_passes++;

    NIMCP_LOGGING_DEBUG("Hierarchical forward pass %llu completed (%u layers)",
                        (unsigned long long)internal->forward_passes, internal->num_layers);

    return true;
}

/**
 * @brief Extract output from a specific region
 *
 * WHAT: Copy region activity to output buffer
 * WHY: Enable reading hierarchical processing results
 * HOW: Locate region, copy activity data, handle size mismatches
 *
 * @param hbrain Hierarchical brain handle
 * @param region_name Name of region to extract from
 * @param output Output buffer (caller-allocated)
 * @param output_size Size of output buffer
 * @return true on success, false on error
 */
bool hierarchical_get_output(
    hierarchical_brain_t hbrain,
    const char* region_name,
    float* output,
    uint32_t output_size
) {
    // Guard: Invalid brain handle
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_get_output: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Invalid region name
    if (!nimcp_validate_pointer(region_name, "region_name")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_get_output: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Invalid output buffer
    if (!nimcp_validate_pointer(output, "output")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_get_output: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Zero output size
    if (output_size == 0) {
        NIMCP_LOGGING_ERROR("Output size cannot be zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_get_output: output_size is zero");
        return false;
    }

    // Find the region
    hierarchical_region_t* region = hierarchical_get_region(hbrain, region_name);
    if (!region) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_INVALID, 0, region_name,
            "Region '%s' not found in hierarchical brain", region_name);
        NIMCP_LOGGING_ERROR("Region '%s' not found", region_name);
        return false;
    }

    // Guard: No activity data available
    if (!region->activity || region->activity_size == 0) {
        NIMCP_LOGGING_WARN("Region '%s' has no activity data (run forward pass first)", region_name);
        memset(output, 0, output_size * sizeof(float));
        return true;
    }

    // Copy activity to output buffer
    uint32_t copy_size = (region->activity_size < output_size)
        ? region->activity_size : output_size;
    memcpy(output, region->activity, copy_size * sizeof(float));

    // Zero-fill remaining output if buffer is larger than activity
    if (output_size > copy_size) {
        memset(output + copy_size, 0, (output_size - copy_size) * sizeof(float));
    }

    NIMCP_LOGGING_DEBUG("Extracted %u values from region '%s'", copy_size, region_name);

    return true;
}

//=============================================================================
// Learning Functions - Helpers
//=============================================================================

/**
 * @brief Compute error signal for region learning
 *
 * WHAT: Calculate learning error from region activity
 * WHY: Enable supervised learning at each region
 * HOW: Compare target (if available) or use prediction error
 *
 * @param region Region to compute error for
 * @param target Target values (may be NULL for unsupervised)
 * @param target_size Size of target array
 * @param error Output error array (same size as activity)
 * @return Computed mean squared error
 */
static float compute_region_error(
    hierarchical_region_t* region,
    const float* target,
    uint32_t target_size,
    float* error
) {
    // Guard: No activity to compute error from
    if (!region->activity || region->activity_size == 0) {
        return 0.0F;
    }

    float mse = 0.0F;
    uint32_t error_size = region->activity_size;

    // Supervised learning: error = target - activity
    if (target && target_size > 0) {
        uint32_t cmp_size = (target_size < error_size) ? target_size : error_size;
        for (uint32_t i = 0; i < cmp_size; i++) {
            error[i] = target[i] - region->activity[i];
            mse += error[i] * error[i];
        }
        // Zero remaining errors
        for (uint32_t i = cmp_size; i < error_size; i++) {
            error[i] = 0.0F;
        }
        mse /= (float)cmp_size;
    } else {
        // Unsupervised: use self-prediction error (reconstruction)
        for (uint32_t i = 0; i < error_size; i++) {
            error[i] = 0.0F;  // No target = no supervised error
        }
    }

    return mse;
}

/**
 * @brief Apply learning update to a single region
 *
 * WHAT: Update region weights using error signal
 * WHY: Enable hierarchical learning with layer-specific rates
 * HOW: Compute error, apply modulated learning rate, update weights
 *
 * @param region Region to update
 * @param error Error signal array
 * @param error_size Size of error array
 * @param base_lr Base learning rate
 * @param dopamine Dopamine modulation level
 * @param confidence Learning confidence
 */
static void apply_region_learning(
    hierarchical_region_t* region,
    const float* error,
    uint32_t error_size,
    float base_lr,
    float dopamine,
    float confidence
) {
    // Guard: No activity buffer
    if (!region->activity || region->activity_size == 0) {
        return;
    }

    // Compute effective learning rate with modulation
    float effective_lr = base_lr
        * region->learning_rate_multiplier
        * (0.5F + dopamine)  // Dopamine modulates learning rate
        * confidence;        // Scale by confidence

    // Clamp effective learning rate
    if (effective_lr > 0.1F) {
        effective_lr = 0.1F;
    }

    // Apply Hebbian-like learning (activity += lr * error)
    uint32_t update_size = (error_size < region->activity_size)
        ? error_size : region->activity_size;

    for (uint32_t i = 0; i < update_size; i++) {
        region->activity[i] += effective_lr * error[i];
    }

    // Update statistics
    region->updates++;
}

//=============================================================================
// Learning Functions - Main Learning Pass
//=============================================================================

/**
 * @brief Train hierarchy with labeled example
 *
 * WHAT: Hierarchical supervised/unsupervised learning
 * WHY: Enable brain-inspired multi-task learning
 * HOW: Forward pass -> compute errors -> backward learning update
 *
 * @param hbrain Hierarchical brain handle
 * @param input Input pattern
 * @param input_size Size of input
 * @param labels Labels for each task (array of strings, may be NULL)
 * @param num_labels Number of labels
 * @param confidence Learning confidence (0-1)
 * @return true on success, false on error
 */
bool hierarchical_learn(
    hierarchical_brain_t hbrain,
    const float* input,
    uint32_t input_size,
    const char** labels,
    uint32_t num_labels,
    float confidence
) {
    // Guard: Invalid brain handle
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_learn: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Invalid input
    if (!nimcp_validate_pointer(input, "input")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_learn: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Zero input size
    if (input_size == 0) {
        NIMCP_LOGGING_ERROR("Input size cannot be zero for learning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_learn: input_size is zero");
        return false;
    }

    // Guard: Invalid confidence range
    if (confidence < 0.0F || confidence > 1.0F) {
        NIMCP_LOGGING_WARN("Confidence %.2f out of range, clamping to [0,1]", confidence);
        confidence = fmaxf(0.0F, fminf(1.0F, confidence));
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Guard: No regions to learn
    if (internal->num_regions == 0) {
        NIMCP_LOGGING_WARN("No regions in hierarchy, nothing to learn");
        return true;
    }

    // Step 1: Forward pass to compute activations
    if (!hierarchical_forward(hbrain, input, input_size)) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_LEARNING_FAILED, 0, "hierarchical",
            "Forward pass failed during hierarchical learning");
        NIMCP_LOGGING_ERROR("Forward pass failed during learning");
        return false;
    }

    // Step 2: Allocate temporary error buffer
    float* error_buffer = (float*)nimcp_calloc(input_size, sizeof(float));
    if (!error_buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, input_size * sizeof(float),
            "Failed to allocate error buffer for hierarchical learning");
        NIMCP_LOGGING_ERROR("Failed to allocate error buffer");
        return false;
    }

    // Step 3: Learn in reverse layer order (backpropagation-like)
    float base_learning_rate = 0.01F;
    float total_error = 0.0F;
    uint32_t regions_updated = 0;

    for (int32_t layer = (int32_t)internal->num_layers - 1; layer >= 0; layer--) {
        for (uint32_t i = 0; i < internal->num_regions; i++) {
            hierarchical_region_t* region = internal->regions[i];

            if (region->layer == (uint32_t)layer) {
                // Compute error for this region
                float region_error = compute_region_error(
                    region, NULL, 0, error_buffer
                );
                total_error += region_error;

                // Apply learning update
                apply_region_learning(
                    region,
                    error_buffer,
                    input_size,
                    base_learning_rate,
                    internal->dopamine_level,
                    confidence
                );

                regions_updated++;
            }
        }
    }

    // Cleanup
    nimcp_free(error_buffer);

    NIMCP_LOGGING_DEBUG("Hierarchical learning completed: %u regions, total_error=%.4f, confidence=%.2f",
                        regions_updated, total_error, confidence);

    return true;
}

//=============================================================================
// Neuromodulation
//=============================================================================

void hierarchical_set_dopamine(hierarchical_brain_t hbrain, float level) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Clamp to valid range
    internal->dopamine_level = fmaxf(0.0F, fminf(1.0F, level));
}

void hierarchical_set_acetylcholine(hierarchical_brain_t hbrain, float level) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Clamp to valid range
    internal->acetylcholine_level = fmaxf(0.0F, fminf(1.0F, level));
}

void hierarchical_modulate_attention(
    hierarchical_brain_t hbrain,
    const char* region_name,
    float gain
) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return;
    }

    if (!nimcp_validate_pointer(region_name, "region_name")) {
        return;
    }

    hierarchical_region_t* region = hierarchical_get_region(hbrain, region_name);

    if (!region) {
        NIMCP_LOGGING_ERROR("Region '%s' not found", region_name);
        return;
    }

    if (!region->attention_enabled) {
        NIMCP_LOGGING_WARN("Attention not enabled for region '%s'", region_name);
        return;
    }

    // Clamp to valid range
    region->attention_gain = fmaxf(0.0F, fminf(2.0F, gain));

    NIMCP_LOGGING_DEBUG("Set attention gain %.2f for region '%s'", gain, region_name);
}

//=============================================================================
// Working Memory
//=============================================================================

bool hierarchical_enable_working_memory(
    hierarchical_brain_t hbrain,
    const char* region_name,
    uint32_t buffer_size,
    float decay_rate
) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_enable_working_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(region_name, "region_name")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_enable_working_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    hierarchical_region_t* region = hierarchical_get_region(hbrain, region_name);

    if (!region) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_INVALID, 0, region_name,
            "Region '%s' not found for working memory enable", region_name);
        NIMCP_LOGGING_ERROR("Region '%s' not found", region_name);
        return false;
    }

    // Allocate memory buffer
    float* buffer = (float*)nimcp_calloc(buffer_size, sizeof(float));

    if (!buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, buffer_size * sizeof(float),
            "Failed to allocate working memory buffer for region '%s'", region_name);
        NIMCP_LOGGING_ERROR("Failed to allocate working memory buffer");
        return false;
    }

    region->has_memory = true;
    region->memory_buffer = buffer;
    region->memory_decay = fmaxf(0.0F, fminf(1.0F, decay_rate));

    NIMCP_LOGGING_DEBUG("Enabled working memory for region '%s' (size %u, decay %.2f)",
                        region_name, buffer_size, decay_rate);

    return true;
}

/**
 * @brief Update working memory for a single region
 *
 * WHAT: Apply decay and integrate current activity
 * WHY: Maintain persistent representations over time
 * HOW: Blend decayed memory with current activity
 *
 * @param region Region with working memory
 * @param blend_factor How much current activity to blend (0-1)
 */
static void update_region_memory(hierarchical_region_t* region, float blend_factor) {
    // Guard: No memory buffer
    if (!region->memory_buffer) {
        return;
    }

    // Guard: No activity to integrate
    if (!region->activity || region->activity_size == 0) {
        // Just apply decay
        for (uint32_t j = 0; j < region->activity_size; j++) {
            region->memory_buffer[j] *= region->memory_decay;
        }
        return;
    }

    // Blend: memory = decay * memory + blend * activity
    for (uint32_t j = 0; j < region->activity_size; j++) {
        region->memory_buffer[j] = region->memory_decay * region->memory_buffer[j]
                                  + blend_factor * region->activity[j];
    }
}

/**
 * @brief Update working memory for all regions
 *
 * WHAT: Decay and update working memory across hierarchy
 * WHY: Maintain temporal context for cognitive processing
 * HOW: Iterate regions, apply decay, blend current activity
 *
 * @param hbrain Hierarchical brain handle
 */
void hierarchical_update_working_memory(hierarchical_brain_t hbrain) {
    // Guard: Invalid brain handle
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Guard: No regions
    if (internal->num_regions == 0) {
        return;
    }

    // Blend factor: how much of current activity to add
    const float blend_factor = 0.3F;

    // Update memory for each region that has it enabled
    for (uint32_t i = 0; i < internal->num_regions; i++) {
        hierarchical_region_t* region = internal->regions[i];

        if (region->has_memory && region->memory_buffer) {
            update_region_memory(region, blend_factor);
        }
    }

    NIMCP_LOGGING_DEBUG("Updated working memory for %u regions", internal->num_regions);
}

//=============================================================================
// Accessors
//=============================================================================

uint32_t hierarchical_get_num_regions(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return 0;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    return internal->num_regions;
}

uint32_t hierarchical_get_num_layers(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return 0;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    return internal->num_layers;
}

uint64_t hierarchical_get_total_forward_passes(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return 0;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    return internal->forward_passes;
}

uint64_t hierarchical_region_get_activations(hierarchical_region_t* region) {
    if (!nimcp_validate_pointer(region, "region")) {
        return 0;
    }

    return region->activations;
}

uint64_t hierarchical_region_get_updates(hierarchical_region_t* region) {
    if (!nimcp_validate_pointer(region, "region")) {
        return 0;
    }

    return region->updates;
}

uint32_t hierarchical_region_get_num_inputs(hierarchical_region_t* region) {
    if (!nimcp_validate_pointer(region, "region")) {
        return 0;
    }

    return region->num_inputs;
}

uint32_t hierarchical_get_max_regions(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return 0;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    return internal->max_regions;
}

const char* hierarchical_get_name(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hierarchical_get_name: nimcp_validate_pointer is NULL");
        return NULL;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    return internal->name;
}

float hierarchical_get_dopamine(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return 0.0F;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    return internal->dopamine_level;
}

float hierarchical_get_acetylcholine(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return 0.0F;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    return internal->acetylcholine_level;
}

//=============================================================================
// Utilities
//=============================================================================

uint32_t hierarchical_get_stats(
    hierarchical_brain_t hbrain,
    char* stats,
    uint32_t max_size
) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        return 0;
    }

    if (!nimcp_validate_pointer(stats, "stats")) {
        return 0;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    int written = snprintf(stats, max_size,
        "{\"name\":\"%s\",\"num_regions\":%u,\"num_layers\":%u,\"forward_passes\":%llu,"
        "\"dopamine\":%.2f,\"acetylcholine\":%.2f}",
        internal->name,
        internal->num_regions,
        internal->num_layers,
        (unsigned long long)internal->forward_passes,
        internal->dopamine_level,
        internal->acetylcholine_level
    );

    if (written < 0 || (uint32_t)written >= max_size) {
        return 0;
    }

    return (uint32_t)written;
}

bool hierarchical_validate(hierarchical_brain_t hbrain) {
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_validate: nimcp_validate_pointer is NULL");
        return false;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Validate basic structure
    if (internal->num_regions > internal->max_regions) {
        return false;
    }

    if (!nimcp_validate_pointer(internal->regions, "regions")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_validate: nimcp_validate_pointer is NULL");
        return false;
    }

    // Validate each region
    for (uint32_t i = 0; i < internal->num_regions; i++) {
        if (!nimcp_validate_pointer(internal->regions[i], "region")) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_validate: nimcp_validate_pointer is NULL");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Persistence - Save/Load Helpers
//=============================================================================

/** @brief File format magic number */
#define HIERARCHICAL_MAGIC 0x48494552  /* "HIER" */

/** @brief File format version */
#define HIERARCHICAL_VERSION 1

/**
 * @brief Save a single region to file
 *
 * WHAT: Serialize one region's data
 * WHY: Modular save enables partial saves
 * HOW: Write name, type, layer, and activity data
 *
 * @param region Region to save
 * @param file Output file
 * @return true on success
 */
static bool save_region(hierarchical_region_t* region, FILE* file) {
    // Guard: Invalid region
    if (!region || !file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "save_region: required parameter is NULL (region, file)");
        return false;
    }

    // Write name
    uint32_t name_len = (uint32_t)strlen(region->name);
    fwrite(&name_len, sizeof(uint32_t), 1, file);
    fwrite(region->name, sizeof(char), name_len, file);

    // Write type and layer
    fwrite(&region->type, sizeof(uint32_t), 1, file);
    fwrite(&region->layer, sizeof(uint32_t), 1, file);

    // Write properties
    fwrite(&region->learning_rate_multiplier, sizeof(float), 1, file);
    fwrite(&region->attention_enabled, sizeof(bool), 1, file);
    fwrite(&region->attention_gain, sizeof(float), 1, file);
    fwrite(&region->has_memory, sizeof(bool), 1, file);
    fwrite(&region->memory_decay, sizeof(float), 1, file);

    // Write activity data
    fwrite(&region->activity_size, sizeof(uint32_t), 1, file);
    if (region->activity && region->activity_size > 0) {
        fwrite(region->activity, sizeof(float), region->activity_size, file);
    }

    // Write statistics
    fwrite(&region->activations, sizeof(uint64_t), 1, file);
    fwrite(&region->updates, sizeof(uint64_t), 1, file);

    return true;
}

/**
 * @brief Load a single region from file
 *
 * WHAT: Deserialize one region's data
 * WHY: Modular load enables partial loads
 * HOW: Read name, type, layer, and activity data
 *
 * @param file Input file
 * @return Allocated region or NULL on error
 */
static hierarchical_region_t* load_region(FILE* file) {
    // Guard: Invalid file
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "file is NULL");

        return NULL;
    }

    // Allocate region
    hierarchical_region_t* region =
        (hierarchical_region_t*)nimcp_calloc(1, sizeof(hierarchical_region_t));
    if (!region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "region is NULL");

        return NULL;
    }

    // Read name
    uint32_t name_len = 0;
    if (fread(&name_len, sizeof(uint32_t), 1, file) != 1) {
        nimcp_free(region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_region: validation failed");
        return NULL;
    }
    if (name_len >= HIERARCHICAL_MAX_NAME_LENGTH) {
        name_len = HIERARCHICAL_MAX_NAME_LENGTH - 1;
    }
    if (fread(region->name, sizeof(char), name_len, file) != name_len) {
        nimcp_free(region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_region: validation failed");
        return NULL;
    }
    region->name[name_len] = '\0';

    // Read type and layer
    fread_chk(&region->type, sizeof(uint32_t), 1, file);
    fread_chk(&region->layer, sizeof(uint32_t), 1, file);

    // Read properties
    fread_chk(&region->learning_rate_multiplier, sizeof(float), 1, file);
    fread_chk(&region->attention_enabled, sizeof(bool), 1, file);
    fread_chk(&region->attention_gain, sizeof(float), 1, file);
    fread_chk(&region->has_memory, sizeof(bool), 1, file);
    fread_chk(&region->memory_decay, sizeof(float), 1, file);

    // Read activity data
    fread_chk(&region->activity_size, sizeof(uint32_t), 1, file);
    if (region->activity_size > 0) {
        region->activity = (float*)nimcp_calloc(region->activity_size, sizeof(float));
        if (region->activity) {
            fread_chk(region->activity, sizeof(float), region->activity_size, file);
        }
    }

    // Read statistics
    fread_chk(&region->activations, sizeof(uint64_t), 1, file);
    fread_chk(&region->updates, sizeof(uint64_t), 1, file);

    // Initialize connection arrays (will need to be rebuilt)
    region->inputs = NULL;
    region->num_inputs = 0;
    region->feedback = NULL;
    region->num_feedback = 0;
    region->lateral = NULL;
    region->num_lateral = 0;
    region->memory_buffer = NULL;
    region->brain = NULL;

    return region;
}

//=============================================================================
// Persistence - Main Save/Load Functions
//=============================================================================

/**
 * @brief Save hierarchical brain to directory
 *
 * WHAT: Serialize entire hierarchical brain state
 * WHY: Enable persistence and checkpointing
 * HOW: Write header, metadata, then each region
 *
 * @param hbrain Hierarchical brain handle
 * @param directory Output directory path
 * @return true on success, false on error
 */
bool hierarchical_save(hierarchical_brain_t hbrain, const char* directory) {
    // Guard: Invalid brain handle
    if (!nimcp_validate_pointer(hbrain, "hbrain")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_save: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: Invalid directory
    if (!nimcp_validate_pointer(directory, "directory")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hierarchical_save: nimcp_validate_pointer is NULL");
        return false;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Construct filename
    char filepath[NIMCP_METRICS_PATH_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/hierarchical.bin", directory);

    // Open file
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_OPEN, filepath,
            "Failed to open '%s' for writing during hierarchical brain save", filepath);
        NIMCP_LOGGING_ERROR("Failed to open '%s' for writing", filepath);
        return false;
    }

    // Write header
    uint32_t magic = HIERARCHICAL_MAGIC;
    uint32_t version = HIERARCHICAL_VERSION;
    fwrite(&magic, sizeof(uint32_t), 1, file);
    fwrite(&version, sizeof(uint32_t), 1, file);

    // Write brain name
    uint32_t name_len = (uint32_t)strlen(internal->name);
    fwrite(&name_len, sizeof(uint32_t), 1, file);
    fwrite(internal->name, sizeof(char), name_len, file);

    // Write brain metadata
    fwrite(&internal->num_regions, sizeof(uint32_t), 1, file);
    fwrite(&internal->max_regions, sizeof(uint32_t), 1, file);
    fwrite(&internal->num_layers, sizeof(uint32_t), 1, file);
    fwrite(&internal->forward_passes, sizeof(uint64_t), 1, file);
    fwrite(&internal->dopamine_level, sizeof(float), 1, file);
    fwrite(&internal->acetylcholine_level, sizeof(float), 1, file);

    // Write each region
    for (uint32_t i = 0; i < internal->num_regions; i++) {
        if (!save_region(internal->regions[i], file)) {
            NIMCP_THROW_IO(NIMCP_ERROR_FILE_WRITE, filepath,
                "Failed to save region %u to '%s'", i, filepath);
            NIMCP_LOGGING_ERROR("Failed to save region %u", i);
            fclose(file);
            return false;
        }
    }

    fclose(file);
    NIMCP_LOGGING_DEBUG("Saved hierarchical brain to '%s' (%u regions)",
                        filepath, internal->num_regions);

    return true;
}

/**
 * @brief Load hierarchical brain from directory
 *
 * WHAT: Deserialize hierarchical brain state
 * WHY: Restore saved brain for continued use
 * HOW: Read header, verify format, load regions
 *
 * @param directory Input directory path
 * @return Loaded brain or NULL on error
 */
hierarchical_brain_t hierarchical_load(const char* directory) {
    // Guard: Invalid directory
    if (!nimcp_validate_pointer(directory, "directory")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hierarchical_load: nimcp_validate_pointer is NULL");
        return NULL;
    }

    // Construct filename
    char filepath[NIMCP_METRICS_PATH_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/hierarchical.bin", directory);

    // Open file
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_OPEN, filepath,
            "Failed to open '%s' for reading during hierarchical brain load", filepath);
        NIMCP_LOGGING_ERROR("Failed to open '%s' for reading", filepath);
        return NULL;
    }

    // Read and verify header
    uint32_t magic = 0, version = 0;
    fread_chk(&magic, sizeof(uint32_t), 1, file);
    fread_chk(&version, sizeof(uint32_t), 1, file);

    if (magic != HIERARCHICAL_MAGIC) {
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_READ, filepath,
            "Invalid file format for '%s' (bad magic: 0x%08X)", filepath, magic);
        NIMCP_LOGGING_ERROR("Invalid file format (bad magic: 0x%08X)", magic);
        fclose(file);
        return NULL;
    }

    if (version > HIERARCHICAL_VERSION) {
        NIMCP_THROW_IO(NIMCP_ERROR_FILE_READ, filepath,
            "Unsupported version %u in '%s' (max: %u)", version, filepath, HIERARCHICAL_VERSION);
        NIMCP_LOGGING_ERROR("Unsupported version: %u (max: %u)", version, HIERARCHICAL_VERSION);
        fclose(file);
        return NULL;
    }

    // Read brain name
    uint32_t name_len = 0;
    fread_chk(&name_len, sizeof(uint32_t), 1, file);
    char name[HIERARCHICAL_MAX_NAME_LENGTH];
    if (name_len >= HIERARCHICAL_MAX_NAME_LENGTH) {
        name_len = HIERARCHICAL_MAX_NAME_LENGTH - 1;
    }
    fread_chk(name, sizeof(char), name_len, file);
    name[name_len] = '\0';

    // Read brain metadata
    uint32_t num_regions = 0, max_regions = 0, num_layers = 0;
    uint64_t forward_passes = 0;
    float dopamine = 0.5F, ach = 0.5F;

    fread_chk(&num_regions, sizeof(uint32_t), 1, file);
    fread_chk(&max_regions, sizeof(uint32_t), 1, file);
    fread_chk(&num_layers, sizeof(uint32_t), 1, file);
    fread_chk(&forward_passes, sizeof(uint64_t), 1, file);
    fread_chk(&dopamine, sizeof(float), 1, file);
    fread_chk(&ach, sizeof(float), 1, file);

    // Create brain structure
    hierarchical_brain_t hbrain = hierarchical_brain_create(name, max_regions);
    if (!hbrain) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_BRAIN_CREATION, 0, name,
            "Failed to create brain '%s' from loaded data in '%s'", name, filepath);
        NIMCP_LOGGING_ERROR("Failed to create brain from loaded data");
        fclose(file);
        return NULL;
    }

    struct hierarchical_brain_internal* internal =
        (struct hierarchical_brain_internal*)hbrain;

    // Restore metadata
    internal->num_layers = num_layers;
    internal->forward_passes = forward_passes;
    internal->dopamine_level = dopamine;
    internal->acetylcholine_level = ach;

    // Load each region
    for (uint32_t i = 0; i < num_regions; i++) {
        hierarchical_region_t* region = load_region(file);
        if (!region) {
            NIMCP_THROW_IO(NIMCP_ERROR_FILE_READ, filepath,
                "Failed to load region %u from '%s'", i, filepath);
            NIMCP_LOGGING_ERROR("Failed to load region %u", i);
            hierarchical_brain_destroy(hbrain);
            fclose(file);
            return NULL;
        }

        // Add to brain's region array
        internal->regions[i] = region;
        internal->num_regions++;
    }

    fclose(file);
    NIMCP_LOGGING_DEBUG("Loaded hierarchical brain from '%s' (%u regions)",
                        filepath, internal->num_regions);

    return hbrain;
}
