/**
 * @file nimcp_protein_synthesis.c
 * @brief Implementation of Protein Synthesis Constraints and Synaptic Tagging
 *
 * WHAT: Protein synthesis as limited resource for late-phase consolidation
 * WHY:  Implement Frey & Morris synaptic tagging and capture model
 * HOW:  PRP pool, tag management, sleep/immune modulation, consolidation logic
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include "plasticity/protein/nimcp_protein_synthesis.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protein_synthesis)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal protein synthesis system state
 */
typedef struct protein_synthesis_struct {
    /* Configuration */
    protein_synthesis_config_t config;

    /* PRP pool state */
    prp_pool_state_t prp_state;

    /* Synaptic tags */
    synaptic_tag_t* tags;           /**< Array of active tags */
    uint32_t num_tags;              /**< Current tag count */
    uint32_t tags_capacity;         /**< Tag array capacity */

    /* Induced synthesis boost */
    float induced_boost_factor;     /**< Current boost factor */
    uint64_t induced_boost_end_ms;  /**< When boost expires */
    uint64_t current_time_ms;       /**< Current simulation time */

    /* Statistics */
    protein_synthesis_stats_t stats;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
} protein_synthesis_system_struct;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * WHAT: Find tag index for synapse
 * WHY:  Locate tag in array for queries/updates
 * HOW:  Linear search by synapse_id
 */
static int find_tag_index(
    const protein_synthesis_system_t system,
    uint32_t synapse_id
) {
    if (!system) {
        return -1;
    }

    for (uint32_t i = 0; i < system->num_tags; i++) {
        if (system->tags[i].synapse_id == synapse_id) {
            return (int)i;
        }
    }

    return -1;  /* Not found - normal lookup behavior */
}

/**
 * WHAT: Remove tag at index
 * WHY:  Cleanup expired or consolidated tags
 * HOW:  Swap with last tag, decrement count
 */
static void remove_tag_at_index(
    protein_synthesis_system_t system,
    uint32_t index
) {
    if (!system || index >= system->num_tags) return;

    /* Swap with last tag */
    if (index < system->num_tags - 1) {
        system->tags[index] = system->tags[system->num_tags - 1];
    }

    system->num_tags--;
}

/**
 * WHAT: Compute tag decay based on time
 * WHY:  Tags decay with half-life of ~2 hours
 * HOW:  Exponential decay: strength = initial * exp(-t / tau)
 */
static float compute_tag_decay(
    uint64_t time_since_set_ms,
    uint64_t half_life_ms
) {
    /* Decay constant from half-life */
    float lambda = 0.693147f / (float)half_life_ms;
    float decay = expf(-lambda * (float)time_since_set_ms);
    return decay;
}

/**
 * WHAT: Invoke consolidation callback
 * WHY:  Notify listener of consolidation event
 * HOW:  Call callback if configured
 */
static void invoke_consolidation_callback(
    const protein_synthesis_system_t system,
    const consolidation_event_t* event
) {
    if (!system || !event) return;

    if (system->config.consolidation_callback) {
        system->config.consolidation_callback(
            event,
            system->config.callback_user_data
        );
    }
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int protein_synthesis_default_config(protein_synthesis_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_default_config: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* PRP pool defaults */
    config->initial_prp_pool = 100.0f;
    config->max_prp_pool = 500.0f;
    config->base_synthesis_rate = PROTEIN_BASE_SYNTHESIS_RATE;
    config->decay_rate = PROTEIN_DECAY_RATE;

    /* Tag defaults */
    config->tag_duration_ms = PROTEIN_TAG_DURATION_MS;
    config->tag_decay_rate = 1.0f / (float)PROTEIN_TAG_HALF_LIFE_MS;
    config->capture_threshold_min = PROTEIN_CAPTURE_THRESHOLD_MIN;
    config->capture_threshold_optimal = PROTEIN_CAPTURE_THRESHOLD_OPTIMAL;

    /* Modulation */
    config->enable_sleep_modulation = true;
    config->enable_immune_suppression = true;

    /* Callbacks */
    config->consolidation_callback = NULL;
    config->callback_user_data = NULL;

    /* Limits */
    config->max_tags = 1000;

    return 0;
}

protein_synthesis_system_t protein_synthesis_create(
    const protein_synthesis_config_t* config
) {
    /* Allocate system */
    protein_synthesis_system_t system = (protein_synthesis_system_t)
        nimcp_calloc(1, sizeof(protein_synthesis_system_struct));

    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate protein synthesis system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protein_synthesis_create: failed to allocate system");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        system->config = *config;
    } else {
        protein_synthesis_default_config(&system->config);
    }

    /* Initialize PRP pool */
    system->prp_state.current_prp_pool = system->config.initial_prp_pool;
    system->prp_state.max_prp_pool = system->config.max_prp_pool;
    system->prp_state.synthesis_rate = system->config.base_synthesis_rate;
    system->prp_state.decay_rate = system->config.decay_rate;
    system->prp_state.sleep_modulation = 1.0f;
    system->prp_state.immune_suppression = 1.0f;
    system->prp_state.total_prps_synthesized = 0;
    system->prp_state.total_prps_captured = 0;
    system->prp_state.total_prps_decayed = 0;

    /* Allocate tag array */
    system->tags_capacity = system->config.max_tags;
    system->tags = (synaptic_tag_t*)nimcp_calloc(
        system->tags_capacity,
        sizeof(synaptic_tag_t)
    );

    if (!system->tags) {
        NIMCP_LOGGING_ERROR("Failed to allocate tag array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protein_synthesis_create: failed to allocate tag array");
        goto cleanup;
    }

    system->num_tags = 0;

    /* Initialize boost state */
    system->induced_boost_factor = 1.0f;
    system->induced_boost_end_ms = 0;
    system->current_time_ms = 0;

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(protein_synthesis_stats_t));

    /* Create mutex */
    system->mutex = nimcp_platform_mutex_create();
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protein_synthesis_create: failed to create mutex");
        goto cleanup;
    }

    /* Bio-async disabled by default */
    system->bio_ctx = NULL;
    system->bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Created protein synthesis system");
    return system;

cleanup:
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
    }
    nimcp_free(system->tags);
    nimcp_free(system);
    return NULL;
}

void protein_synthesis_destroy(protein_synthesis_system_t system) {
    if (!system) return;

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    /* Free tags */
    if (system->tags) {
        nimcp_free(system->tags);
    }

    /* Free system */
    nimcp_free(system);

    NIMCP_LOGGING_INFO("Destroyed protein synthesis system");
}

/* ============================================================================
 * Synaptic Tag API Implementation
 * ============================================================================ */

int protein_synthesis_set_tag(
    protein_synthesis_system_t system,
    uint32_t synapse_id,
    float stimulation_strength
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_set_tag: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (stimulation_strength < 0.0f || stimulation_strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protein_synthesis_set_tag: stimulation_strength out of range");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Check if already tagged */
    int existing_idx = find_tag_index(system, synapse_id);
    if (existing_idx >= 0) {
        /* Update existing tag */
        system->tags[existing_idx].tag_strength = stimulation_strength;
        system->tags[existing_idx].tag_set_time_ms = system->current_time_ms;
        nimcp_platform_mutex_unlock(system->mutex);
        return 0;
    }

    /* Check capacity */
    if (system->num_tags >= system->tags_capacity) {
        NIMCP_LOGGING_WARN("Tag capacity reached, cannot set new tag");
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Create new tag */
    synaptic_tag_t* tag = &system->tags[system->num_tags];
    tag->state = TAG_STATE_TAGGED;
    tag->tag_set_time_ms = system->current_time_ms;
    tag->tag_strength = stimulation_strength;
    tag->prps_captured = 0.0f;
    tag->consolidation_achieved = false;
    tag->synapse_id = synapse_id;

    system->num_tags++;
    system->stats.total_tags_set++;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Set tag on synapse %u (strength %.2f)",
                        synapse_id, stimulation_strength);
    return 0;
}

int protein_synthesis_remove_tag(
    protein_synthesis_system_t system,
    uint32_t synapse_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_remove_tag: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    int idx = find_tag_index(system, synapse_id);
    if (idx < 0) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    remove_tag_at_index(system, (uint32_t)idx);

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Removed tag from synapse %u", synapse_id);
    return 0;
}

bool protein_synthesis_is_tagged(
    const protein_synthesis_system_t system,
    uint32_t synapse_id
) {
    if (!system) {
        return false;
    }

    nimcp_platform_mutex_lock(system->mutex);
    int idx = find_tag_index(system, synapse_id);
    nimcp_platform_mutex_unlock(system->mutex);

    return (idx >= 0);
}

int protein_synthesis_get_tag(
    const protein_synthesis_system_t system,
    uint32_t synapse_id,
    synaptic_tag_t* tag
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_get_tag: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_get_tag: tag is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    int idx = find_tag_index(system, synapse_id);
    if (idx < 0) {
        nimcp_platform_mutex_unlock(system->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    *tag = system->tags[idx];

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * PRP Pool API Implementation
 * ============================================================================ */

float protein_synthesis_get_prp_pool(
    const protein_synthesis_system_t system
) {
    if (!system) return 0.0f;

    nimcp_platform_mutex_lock(system->mutex);
    float pool = system->prp_state.current_prp_pool;
    nimcp_platform_mutex_unlock(system->mutex);

    return pool;
}

float protein_synthesis_get_synthesis_rate(
    const protein_synthesis_system_t system
) {
    if (!system) return 0.0f;

    nimcp_platform_mutex_lock(system->mutex);
    float rate = system->prp_state.synthesis_rate;
    nimcp_platform_mutex_unlock(system->mutex);

    return rate;
}

int protein_synthesis_induce_synthesis(
    protein_synthesis_system_t system,
    float boost_factor,
    uint64_t duration_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_induce_synthesis: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (boost_factor < 1.0f || boost_factor > 3.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protein_synthesis_induce_synthesis: boost_factor out of range");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->mutex);

    system->induced_boost_factor = boost_factor;
    system->induced_boost_end_ms = system->current_time_ms + duration_ms;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Induced synthesis boost %.2fx for %llu ms",
                        boost_factor, (unsigned long long)duration_ms);
    return 0;
}

int protein_synthesis_add_prps(
    protein_synthesis_system_t system,
    float prp_amount
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_add_prps: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (prp_amount < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protein_synthesis_add_prps: prp_amount is negative");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->mutex);

    system->prp_state.current_prp_pool += prp_amount;
    if (system->prp_state.current_prp_pool > system->prp_state.max_prp_pool) {
        system->prp_state.current_prp_pool = system->prp_state.max_prp_pool;
    }

    system->prp_state.total_prps_synthesized += (uint64_t)prp_amount;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Added %.2f PRPs to pool", prp_amount);
    return 0;
}

/* ============================================================================
 * Consolidation API Implementation
 * ============================================================================ */

int protein_synthesis_consolidate_synapse(
    protein_synthesis_system_t system,
    uint32_t synapse_id
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_consolidate_synapse: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Find tag */
    int idx = find_tag_index(system, synapse_id);
    if (idx < 0) {
        /* No tag - failed */
        consolidation_event_t event = {
            .synapse_id = synapse_id,
            .final_state = TAG_STATE_UNTAGGED,
            .prps_used = 0.0f,
            .success = false,
            .severity = PROTEIN_CONSOL_FAILED_NO_TAG,
            .timestamp_ms = system->current_time_ms
        };

        /* Copy callback data before releasing mutex */
        consolidation_callback_t callback = system->config.consolidation_callback;
        void* callback_user_data = system->config.callback_user_data;

        nimcp_platform_mutex_unlock(system->mutex);

        /* Invoke callback after releasing mutex to avoid deadlock */
        if (callback) {
            callback(&event, callback_user_data);
        }

        NIMCP_LOGGING_DEBUG("Consolidation failed: no tag (synapse %u)",
                            synapse_id);
        return NIMCP_ERROR_INVALID_STATE;
    }

    synaptic_tag_t* tag = &system->tags[idx];

    /* Check PRP availability */
    float prps_needed = system->config.capture_threshold_min;
    if (system->prp_state.current_prp_pool < prps_needed) {
        /* Insufficient PRPs */
        consolidation_event_t event = {
            .synapse_id = synapse_id,
            .final_state = tag->state,
            .prps_used = 0.0f,
            .success = false,
            .severity = PROTEIN_CONSOL_FAILED_NO_PRP,
            .timestamp_ms = system->current_time_ms
        };

        /* Copy callback data before releasing mutex */
        consolidation_callback_t callback = system->config.consolidation_callback;
        void* callback_user_data = system->config.callback_user_data;

        nimcp_platform_mutex_unlock(system->mutex);

        /* Invoke callback after releasing mutex to avoid deadlock */
        if (callback) {
            callback(&event, callback_user_data);
        }

        NIMCP_LOGGING_DEBUG("Consolidation failed: insufficient PRPs "
                            "(synapse %u, need %.2f, have %.2f)",
                            synapse_id, prps_needed,
                            system->prp_state.current_prp_pool);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Capture PRPs */
    float prps_to_capture = fminf(
        system->prp_state.current_prp_pool,
        system->config.capture_threshold_optimal
    );

    system->prp_state.current_prp_pool -= prps_to_capture;
    tag->prps_captured += prps_to_capture;
    tag->state = TAG_STATE_CONSOLIDATED;
    tag->consolidation_achieved = true;

    system->prp_state.total_prps_captured += (uint64_t)prps_to_capture;
    system->stats.total_consolidations++;

    /* Determine success level */
    bool full_success = (prps_to_capture >=
                         system->config.capture_threshold_optimal);
    uint32_t severity = full_success ?
        PROTEIN_CONSOL_SUCCESS : PROTEIN_CONSOL_PARTIAL;

    consolidation_event_t event = {
        .synapse_id = synapse_id,
        .final_state = TAG_STATE_CONSOLIDATED,
        .prps_used = prps_to_capture,
        .success = true,
        .severity = severity,
        .timestamp_ms = system->current_time_ms
    };

    /* Copy callback data before releasing mutex */
    consolidation_callback_t callback = system->config.consolidation_callback;
    void* callback_user_data = system->config.callback_user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Invoke callback after releasing mutex to avoid deadlock */
    if (callback) {
        callback(&event, callback_user_data);
    }

    NIMCP_LOGGING_INFO("Consolidated synapse %u with %.2f PRPs (%s)",
                       synapse_id, prps_to_capture,
                       full_success ? "full" : "partial");
    return 0;
}

bool protein_synthesis_can_consolidate(
    const protein_synthesis_system_t system,
    uint32_t synapse_id
) {
    if (!system) {
        return false;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Check tag */
    int idx = find_tag_index(system, synapse_id);
    if (idx < 0) {
        nimcp_platform_mutex_unlock(system->mutex);
        return false;  /* Tag not found - normal lookup behavior */
    }

    /* Check PRP availability */
    bool can_consolidate = (system->prp_state.current_prp_pool >=
                            system->config.capture_threshold_min);

    nimcp_platform_mutex_unlock(system->mutex);
    return can_consolidate;
}

float protein_synthesis_get_consolidation_progress(
    const protein_synthesis_system_t system,
    uint32_t synapse_id
) {
    if (!system) return 0.0f;

    nimcp_platform_mutex_lock(system->mutex);

    int idx = find_tag_index(system, synapse_id);
    if (idx < 0) {
        nimcp_platform_mutex_unlock(system->mutex);
        return 0.0f;
    }

    synaptic_tag_t* tag = &system->tags[idx];
    float progress = tag->prps_captured /
                     system->config.capture_threshold_optimal;
    progress = fminf(progress, 1.0f);

    nimcp_platform_mutex_unlock(system->mutex);
    return progress;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

int protein_synthesis_update(
    protein_synthesis_system_t system,
    uint64_t delta_ms
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_update: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    system->current_time_ms += delta_ms;
    float delta_sec = (float)delta_ms / 1000.0f;
    (void)delta_sec;  /* Suppress unused warning */

    /* Check induced boost expiration */
    if (system->induced_boost_factor > 1.0f &&
        system->current_time_ms >= system->induced_boost_end_ms) {
        system->induced_boost_factor = 1.0f;
    }

    /* PARAMETER VALIDATION: Validate synthesis rate components before use
     * WHAT: Ensure all rate multipliers are positive and finite
     * WHY:  Invalid rates cause numerical instability and incorrect pool dynamics
     * HOW:  Clamp each component to valid range
     */
    float base_rate = system->config.base_synthesis_rate;
    if (isnan(base_rate) || base_rate < 0.0f) {
        base_rate = PROTEIN_BASE_SYNTHESIS_RATE;  /* Use default */
    }

    float sleep_mod = system->prp_state.sleep_modulation;
    if (isnan(sleep_mod) || sleep_mod < 0.0f) {
        sleep_mod = 1.0f;  /* Default to no modulation */
    }
    if (sleep_mod > 10.0f) sleep_mod = 10.0f;  /* Cap at reasonable maximum */

    float immune_supp = system->prp_state.immune_suppression;
    if (isnan(immune_supp) || immune_supp < 0.0f) {
        immune_supp = 1.0f;  /* Default to no suppression */
    }
    if (immune_supp > 1.0f) immune_supp = 1.0f;  /* Cap at 100% */

    float boost = system->induced_boost_factor;
    if (isnan(boost) || boost < 1.0f) {
        boost = 1.0f;  /* No boost below 1.0 */
    }
    if (boost > 10.0f) boost = 10.0f;  /* Cap at reasonable maximum */

    /* Compute effective synthesis rate with validated components */
    float effective_rate = base_rate * sleep_mod * immune_supp * boost;

    /* Final validation of effective rate */
    if (isnan(effective_rate) || isinf(effective_rate) || effective_rate < 0.0f) {
        effective_rate = PROTEIN_BASE_SYNTHESIS_RATE;  /* Fallback to default */
    }

    system->prp_state.synthesis_rate = effective_rate;

    /* Synthesize PRPs with numerical validation */
    float prps_synthesized = effective_rate * (float)delta_ms;
    if (isnan(prps_synthesized) || prps_synthesized < 0.0f) {
        prps_synthesized = 0.0f;  /* Skip invalid synthesis */
    }
    system->prp_state.current_prp_pool += prps_synthesized;

    /* Clamp PRP pool to valid range */
    if (system->prp_state.current_prp_pool > system->prp_state.max_prp_pool) {
        system->prp_state.current_prp_pool = system->prp_state.max_prp_pool;
    }
    if (system->prp_state.current_prp_pool < 0.0f) {
        system->prp_state.current_prp_pool = 0.0f;
    }

    system->prp_state.total_prps_synthesized += (uint64_t)prps_synthesized;

    /* Validate decay rate before use */
    float decay_rate = system->config.decay_rate;
    if (isnan(decay_rate) || decay_rate < 0.0f) {
        decay_rate = PROTEIN_DECAY_RATE;  /* Use default */
    }
    if (decay_rate > 1.0f) decay_rate = 1.0f;  /* Cap at 100% per ms */

    /* Decay PRPs with numerical validation */
    float decay_amount = system->prp_state.current_prp_pool * decay_rate * (float)delta_ms;
    if (isnan(decay_amount) || decay_amount < 0.0f) {
        decay_amount = 0.0f;  /* Skip invalid decay */
    }
    /* Don't decay more than available */
    if (decay_amount > system->prp_state.current_prp_pool) {
        decay_amount = system->prp_state.current_prp_pool;
    }
    system->prp_state.current_prp_pool -= decay_amount;

    /* Final bounds check for PRP pool */
    if (system->prp_state.current_prp_pool < 0.0f) {
        system->prp_state.current_prp_pool = 0.0f;
    }
    if (isnan(system->prp_state.current_prp_pool)) {
        system->prp_state.current_prp_pool = system->config.initial_prp_pool;
    }

    system->prp_state.total_prps_decayed += (uint64_t)decay_amount;

    /* Deferred callbacks for expired tags */
    #define MAX_DEFERRED_EXPIRATION_CALLBACKS 32
    consolidation_event_t deferred_events[MAX_DEFERRED_EXPIRATION_CALLBACKS];
    uint32_t num_deferred = 0;

    /* Update tags - decay and expire */
    for (uint32_t i = 0; i < system->num_tags; ) {
        synaptic_tag_t* tag = &system->tags[i];

        /* Skip consolidated tags */
        if (tag->state == TAG_STATE_CONSOLIDATED) {
            i++;
            continue;
        }

        /* Compute tag decay */
        uint64_t time_since_set = system->current_time_ms - tag->tag_set_time_ms;
        float decay_factor = compute_tag_decay(
            time_since_set,
            PROTEIN_TAG_HALF_LIFE_MS
        );

        tag->tag_strength *= decay_factor;

        /* Check expiration */
        if (time_since_set >= system->config.tag_duration_ms ||
            tag->tag_strength < 0.01f) {
            /* Tag expired - defer callback */
            if (num_deferred < MAX_DEFERRED_EXPIRATION_CALLBACKS) {
                deferred_events[num_deferred].synapse_id = tag->synapse_id;
                deferred_events[num_deferred].final_state = TAG_STATE_UNTAGGED;
                deferred_events[num_deferred].prps_used = 0.0f;
                deferred_events[num_deferred].success = false;
                deferred_events[num_deferred].severity = PROTEIN_CONSOL_FAILED_NO_TAG;
                deferred_events[num_deferred].timestamp_ms = system->current_time_ms;
                num_deferred++;
            }

            system->stats.total_tags_expired++;
            system->stats.total_consolidation_failures++;

            /* Remove tag */
            remove_tag_at_index(system, i);
            /* Don't increment i - we swapped with last element */
        } else {
            i++;
        }
    }

    /* Copy callback data before releasing mutex */
    consolidation_callback_t callback = system->config.consolidation_callback;
    void* callback_user_data = system->config.callback_user_data;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Invoke deferred callbacks after releasing mutex to avoid deadlock */
    if (callback) {
        for (uint32_t i = 0; i < num_deferred; i++) {
            callback(&deferred_events[i], callback_user_data);
        }
    }

    return 0;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int protein_synthesis_get_stats(
    const protein_synthesis_system_t system,
    protein_synthesis_stats_t* stats
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_get_stats: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_get_stats: stats is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    *stats = system->stats;

    /* Update current values */
    stats->current_prp_pool = system->prp_state.current_prp_pool;
    stats->total_prps_synthesized = system->prp_state.total_prps_synthesized;
    stats->total_prps_captured = system->prp_state.total_prps_captured;
    stats->total_prps_decayed = system->prp_state.total_prps_decayed;

    /* Compute derived metrics */
    if (system->prp_state.total_prps_synthesized > 0) {
        stats->capture_efficiency =
            (float)system->prp_state.total_prps_captured /
            (float)system->prp_state.total_prps_synthesized;
    } else {
        stats->capture_efficiency = 0.0f;
    }

    if (system->stats.total_tags_set > 0) {
        stats->consolidation_success_rate =
            (float)system->stats.total_consolidations /
            (float)system->stats.total_tags_set;
    } else {
        stats->consolidation_success_rate = 0.0f;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return 0;
}

int protein_synthesis_reset_stats(protein_synthesis_system_t system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_reset_stats: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Reset statistics (preserve current state) */
    system->stats.total_tags_set = 0;
    system->stats.total_tags_expired = 0;
    system->stats.total_consolidations = 0;
    system->stats.total_consolidation_failures = 0;
    system->stats.avg_sleep_boost = 0.0f;
    system->stats.avg_immune_suppression = 0.0f;
    system->stats.capture_efficiency = 0.0f;
    system->stats.consolidation_success_rate = 0.0f;

    /* Keep cumulative PRP counters for efficiency calculation */

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Reset protein synthesis statistics");
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

uint32_t protein_synthesis_get_num_tags(
    const protein_synthesis_system_t system
) {
    if (!system) return 0;

    nimcp_platform_mutex_lock(system->mutex);
    uint32_t count = system->num_tags;
    nimcp_platform_mutex_unlock(system->mutex);

    return count;
}

uint32_t protein_synthesis_get_tag_ids(
    const protein_synthesis_system_t system,
    uint32_t* synapse_ids,
    uint32_t max_ids
) {
    if (!system || !synapse_ids || max_ids == 0) return 0;

    nimcp_platform_mutex_lock(system->mutex);

    uint32_t count = (system->num_tags < max_ids) ?
                     system->num_tags : max_ids;

    for (uint32_t i = 0; i < count; i++) {
        synapse_ids[i] = system->tags[i].synapse_id;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return count;
}

int protein_synthesis_get_prp_state(
    const protein_synthesis_system_t system,
    prp_pool_state_t* state
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_get_prp_state: system is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protein_synthesis_get_prp_state: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    *state = system->prp_state;
    nimcp_platform_mutex_unlock(system->mutex);

    return 0;
}
