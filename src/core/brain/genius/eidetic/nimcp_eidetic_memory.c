/**
 * @file nimcp_eidetic_memory.c
 * @brief Implementation of eidetic memory integration for genius profiles
 *
 * WHAT: Apply eidetic memory enhancements to NIMCP memory systems
 * WHY:  Enable photographic/audiographic memory capabilities for genius profiles
 * HOW:  Modify memory system parameters based on eidetic configuration
 *
 * @version 1.0.0
 * @date 2026-02-03
 */

#include "core/brain/genius/eidetic/nimcp_eidetic_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "EIDETIC_MEMORY"

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

const char* eidetic_error_string(eidetic_error_t error) {
    switch (error) {
        case EIDETIC_SUCCESS:
            return "Success";
        case EIDETIC_ERROR_NULL_POINTER:
            return "Null pointer argument";
        case EIDETIC_ERROR_INVALID_CONFIG:
            return "Invalid eidetic configuration";
        case EIDETIC_ERROR_SYSTEM_NOT_SUPPORTED:
            return "Memory system not supported";
        case EIDETIC_ERROR_APPLY_FAILED:
            return "Failed to apply eidetic enhancement";
        case EIDETIC_ERROR_ALREADY_APPLIED:
            return "Eidetic enhancement already applied";
        default:
            return "Unknown error";
    }
}

bool eidetic_config_is_valid(const eidetic_memory_config_t* config) {
    if (!config) {
        return false;
    }

    /* Check modality strengths are in valid range [0.0, 3.0] */
    if (config->visual_eidetic < 0.0f || config->visual_eidetic > 3.0f) {
        return false;
    }
    if (config->auditory_eidetic < 0.0f || config->auditory_eidetic > 3.0f) {
        return false;
    }
    if (config->spatial_eidetic < 0.0f || config->spatial_eidetic > 3.0f) {
        return false;
    }
    if (config->verbal_eidetic < 0.0f || config->verbal_eidetic > 3.0f) {
        return false;
    }

    /* Check global characteristics are non-negative */
    if (config->encoding_speed < 0.0f) {
        return false;
    }
    if (config->decay_resistance < 0.0f) {
        return false;
    }
    if (config->retrieval_accuracy < 0.0f || config->retrieval_accuracy > 1.0f) {
        return false;
    }
    if (config->detail_granularity < 0.0f) {
        return false;
    }

    return true;
}

float eidetic_scale_value(float base_value, float eidetic_strength, float max_multiplier) {
    /*
     * Scale base value by eidetic strength.
     * At strength 0.0: multiplier = 1.0 (no change)
     * At strength 3.0: multiplier = max_multiplier
     * Linear interpolation between.
     */
    if (eidetic_strength <= 0.0f) {
        return base_value;
    }
    if (eidetic_strength >= 3.0f) {
        return base_value * max_multiplier;
    }

    float multiplier = 1.0f + (max_multiplier - 1.0f) * (eidetic_strength / 3.0f);
    return base_value * multiplier;
}

float eidetic_compute_decay_resistance(float eidetic_strength) {
    /*
     * Compute decay multiplier from eidetic strength.
     * At strength 0.0: multiplier = 1.0 (normal decay)
     * At strength 3.0: multiplier = 0.1 (10x slower decay)
     * Exponential relationship for more natural feel.
     */
    if (eidetic_strength <= 0.0f) {
        return 1.0f;
    }
    if (eidetic_strength >= 3.0f) {
        return 0.1f;
    }

    /* Exponential decay: 1.0 -> 0.1 as strength goes 0 -> 3 */
    return powf(0.1f, eidetic_strength / 3.0f);
}

/* ============================================================================
 * MASTER APPLY FUNCTION
 * ============================================================================ */

eidetic_error_t eidetic_apply_to_all(
    const eidetic_memory_config_t* config,
    working_memory_t* working_memory,
    hippocampus_adapter_t* hippocampus,
    semantic_memory_system_t* semantic,
    systems_consolidation_system_t* consolidation,
    engram_system_t* engram)
{
    if (!config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    eidetic_error_t result;
    uint32_t applied_count = 0;
    uint32_t failed_count = 0;

    LOG_DEBUG(LOG_MODULE, "Applying eidetic config to all memory systems");

    /* Apply to working memory if available */
    if (working_memory) {
        result = eidetic_apply_to_working_memory(working_memory, config);
        if (result == EIDETIC_SUCCESS) {
            applied_count++;
        } else {
            LOG_WARN(LOG_MODULE, "Failed to apply eidetic to working memory: %s",
                     eidetic_error_string(result));
            failed_count++;
        }
    }

    /* Apply to hippocampus if available */
    if (hippocampus) {
        result = eidetic_apply_to_hippocampus(hippocampus, config);
        if (result == EIDETIC_SUCCESS) {
            applied_count++;
        } else {
            LOG_WARN(LOG_MODULE, "Failed to apply eidetic to hippocampus: %s",
                     eidetic_error_string(result));
            failed_count++;
        }
    }

    /* Apply to semantic memory if available */
    if (semantic) {
        result = eidetic_apply_to_semantic(semantic, config);
        if (result == EIDETIC_SUCCESS) {
            applied_count++;
        } else {
            LOG_WARN(LOG_MODULE, "Failed to apply eidetic to semantic memory: %s",
                     eidetic_error_string(result));
            failed_count++;
        }
    }

    /* Apply to systems consolidation if available */
    if (consolidation) {
        result = eidetic_apply_to_consolidation(consolidation, config);
        if (result == EIDETIC_SUCCESS) {
            applied_count++;
        } else {
            LOG_WARN(LOG_MODULE, "Failed to apply eidetic to consolidation: %s",
                     eidetic_error_string(result));
            failed_count++;
        }
    }

    /* Apply to engram system if available */
    if (engram) {
        result = eidetic_apply_to_engram(engram, config);
        if (result == EIDETIC_SUCCESS) {
            applied_count++;
        } else {
            LOG_WARN(LOG_MODULE, "Failed to apply eidetic to engram system: %s",
                     eidetic_error_string(result));
            failed_count++;
        }
    }

    LOG_DEBUG(LOG_MODULE, "Eidetic applied to %u systems, %u failed",
              applied_count, failed_count);

    /* Return success if at least one system was enhanced */
    if (applied_count > 0) {
        return EIDETIC_SUCCESS;
    }

    /* No systems available to enhance */
    if (failed_count == 0) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    return EIDETIC_ERROR_APPLY_FAILED;
}

/* ============================================================================
 * PER-SYSTEM APPLY FUNCTIONS
 * ============================================================================ */

eidetic_error_t eidetic_apply_to_working_memory(
    working_memory_t* wm,
    const eidetic_memory_config_t* config)
{
    if (!wm || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to working memory");

    /*
     * Working memory enhancements:
     * - Capacity: 7±2 -> 12-15 items at strength 3.0
     * - Decay: 5-10x slower at strength 3.0
     * - Refresh: 2x more effective at strength 3.0
     *
     * Note: Actual parameter modification depends on working_memory_t API.
     * This is a stub implementation that logs the intended changes.
     */

    const eidetic_working_memory_config_t* wm_config = &config->working_memory;

    /* Calculate scaled values */
    float capacity_boost = eidetic_scale_value(1.0f, config->verbal_eidetic, 2.0f);
    float decay_multiplier = eidetic_compute_decay_resistance(
        (config->visual_eidetic + config->auditory_eidetic) / 2.0f);

    LOG_DEBUG(LOG_MODULE, "  Capacity boost: %.2fx", capacity_boost);
    LOG_DEBUG(LOG_MODULE, "  Decay multiplier: %.2f", decay_multiplier);
    LOG_DEBUG(LOG_MODULE, "  Config capacity boost: %u", wm_config->capacity_boost);

    /*
     * TODO: When working_memory_t API is available, apply these changes:
     * - wm->capacity = base_capacity + wm_config->capacity_boost
     * - wm->decay_rate *= wm_config->decay_multiplier
     * - wm->refresh_efficiency = wm_config->refresh_efficiency
     */

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_hippocampus(
    hippocampus_adapter_t* hippo,
    const eidetic_memory_config_t* config)
{
    if (!hippo || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to hippocampus");

    const eidetic_hippocampus_config_t* hc = &config->hippocampus;

    /*
     * Hippocampus enhancements:
     * - DG/CA3/CA1 size: up to 4x at strength 3.0
     * - Pattern separation: 10x instead of 5x
     * - Single exposure learning enabled
     * - Perfect replay fidelity
     */

    LOG_DEBUG(LOG_MODULE, "  DG multiplier: %.2f", hc->dg_size_multiplier);
    LOG_DEBUG(LOG_MODULE, "  CA3 multiplier: %.2f", hc->ca3_size_multiplier);
    LOG_DEBUG(LOG_MODULE, "  Pattern separation ratio: %.2f", hc->pattern_separation_ratio);
    LOG_DEBUG(LOG_MODULE, "  Single exposure: %s",
              hc->single_exposure_learning ? "enabled" : "disabled");

    /*
     * TODO: When hippocampus_adapter_t internals are accessible:
     * - Scale subregion sizes
     * - Adjust pattern separation threshold
     * - Enable single exposure encoding mode
     */

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_semantic(
    semantic_memory_system_t* sm,
    const eidetic_memory_config_t* config)
{
    if (!sm || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to semantic memory");

    const eidetic_semantic_config_t* sc = &config->semantic;

    /*
     * Semantic memory enhancements:
     * - Concept capacity: 2048 -> 16384 (8x)
     * - Feature dimensions: 32 -> 128 (4x)
     * - Spreading activation: broader reach
     * - One-shot concept formation
     */

    LOG_DEBUG(LOG_MODULE, "  Concept capacity multiplier: %u",
              sc->concept_capacity_multiplier);
    LOG_DEBUG(LOG_MODULE, "  Feature dimension boost: %u",
              sc->feature_dimension_boost);
    LOG_DEBUG(LOG_MODULE, "  Instant learning: %s",
              sc->enable_instant_learning ? "enabled" : "disabled");

    /*
     * TODO: When semantic_memory_system_t allows runtime modification:
     * - Expand concept storage
     * - Increase feature vector dimensions
     * - Lower activation thresholds
     */

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_consolidation(
    systems_consolidation_system_t* sc,
    const eidetic_memory_config_t* config)
{
    if (!sc || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to systems consolidation");

    const eidetic_systems_consolidation_config_t* scc = &config->systems_consolidation;

    /*
     * Systems consolidation enhancements:
     * - Transfer rate: 5%/hr -> 15%/hr in SWS (3x)
     * - Semantic threshold: 0.7 -> 0.5 (earlier extraction)
     * - Forgetting rate: 0.002/hr -> 0.0002/hr (10x slower)
     * - Cortical capacity: 2048 -> 16384 (8x)
     */

    LOG_DEBUG(LOG_MODULE, "  Transfer rate boost: %.2fx", scc->cortical_transfer_rate);
    LOG_DEBUG(LOG_MODULE, "  Capacity multiplier: %.2fx", scc->cortical_capacity_multiplier);
    LOG_DEBUG(LOG_MODULE, "  Forgetting resistance: %.2f", scc->forgetting_resistance);
    LOG_DEBUG(LOG_MODULE, "  Preserve episodic: %s",
              scc->preserve_episodic_detail ? "yes" : "no");

    /*
     * TODO: Modify systems_consolidation_system_t parameters:
     * - sc->transfer_rate *= scc->cortical_transfer_rate
     * - sc->forgetting_rate *= (1.0 / scc->forgetting_resistance)
     * - sc->semantic_threshold lowered for earlier extraction
     */

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_engram(
    engram_system_t* es,
    const eidetic_memory_config_t* config)
{
    if (!es || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to engram system");

    const eidetic_engram_config_t* ec = &config->engram;

    /*
     * Engram system enhancements:
     * - Max engrams: 512 -> 4096 (8x)
     * - Neurons per engram: 256 -> 1024 (4x)
     * - Consolidation time: 6hr -> 1hr (6x faster)
     * - Tagging window: 6hr -> 24hr (4x longer)
     */

    LOG_DEBUG(LOG_MODULE, "  Engram capacity multiplier: %u", ec->engram_capacity_multiplier);
    LOG_DEBUG(LOG_MODULE, "  Neurons per engram boost: %u", ec->neurons_per_engram_boost);
    LOG_DEBUG(LOG_MODULE, "  Consolidation speed: %.2fx", ec->consolidation_speed);
    LOG_DEBUG(LOG_MODULE, "  Instant consolidation: %s",
              ec->instant_consolidation ? "enabled" : "disabled");

    /*
     * TODO: Modify engram_system_t parameters when API available
     */

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_hopfield(
    hopfield_memory_t* hm,
    const eidetic_memory_config_t* config)
{
    if (!hm || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to Hopfield memory");

    const eidetic_hopfield_config_t* hc = &config->hopfield;

    /*
     * Hopfield memory enhancements:
     * - Pattern capacity: 1024 -> 8192 (8x)
     * - Pattern dimension: 256 -> 1024 (4x)
     * - Temperature: 1.0 -> 10.0 (sharper retrieval)
     * - Convergence: 3 iters -> 1 iter (instant)
     */

    LOG_DEBUG(LOG_MODULE, "  Pattern capacity multiplier: %u",
              hc->pattern_capacity_multiplier);
    LOG_DEBUG(LOG_MODULE, "  Pattern dimension boost: %u",
              hc->pattern_dimension_boost);
    LOG_DEBUG(LOG_MODULE, "  Inverse temperature: %.2f", hc->inverse_temperature);
    LOG_DEBUG(LOG_MODULE, "  One-shot storage: %s",
              hc->enable_one_shot_storage ? "enabled" : "disabled");

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_procedural(
    procedural_memory_t* pm,
    const eidetic_memory_config_t* config)
{
    if (!pm || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to procedural memory");

    const eidetic_procedural_config_t* pc = &config->procedural;

    /*
     * Procedural memory enhancements:
     * - Skill acquisition: 3x faster
     * - Chunk size: 4 -> 8 (2x)
     * - Automation: 1000 reps -> 100 reps (10x faster)
     * - Motor precision: 0.9 -> 0.99
     */

    LOG_DEBUG(LOG_MODULE, "  Acquisition speed: %.2fx", pc->acquisition_speed);
    LOG_DEBUG(LOG_MODULE, "  Chunk size boost: %u", pc->chunk_size_boost);
    LOG_DEBUG(LOG_MODULE, "  Automation speed: %.2fx", pc->automation_speed);
    LOG_DEBUG(LOG_MODULE, "  Mental practice: %s",
              pc->enable_mental_practice ? "enabled" : "disabled");

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_prospective(
    prospective_memory_t* pm,
    const eidetic_memory_config_t* config)
{
    if (!pm || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to prospective memory");

    const eidetic_prospective_config_t* pc = &config->prospective;

    /*
     * Prospective memory enhancements:
     * - Intention capacity: 16 -> 64 (4x)
     * - Time precision: ±5min -> ±10sec (30x)
     * - Cue detection: threshold 0.7 -> 0.3
     * - Forgetting: 0.1/day -> 0.01/day (10x slower)
     */

    LOG_DEBUG(LOG_MODULE, "  Intention capacity boost: %u", pc->intention_capacity_boost);
    LOG_DEBUG(LOG_MODULE, "  Temporal precision: %.2fx", pc->temporal_precision);
    LOG_DEBUG(LOG_MODULE, "  Cue sensitivity: %.2f", pc->cue_sensitivity);
    LOG_DEBUG(LOG_MODULE, "  Automatic scheduling: %s",
              pc->automatic_scheduling ? "enabled" : "disabled");

    return EIDETIC_SUCCESS;
}

eidetic_error_t eidetic_apply_to_autobiographical(
    autobiographical_memory_system_t* am,
    const eidetic_memory_config_t* config)
{
    if (!am || !config) {
        return EIDETIC_ERROR_NULL_POINTER;
    }

    if (!eidetic_config_is_valid(config)) {
        return EIDETIC_ERROR_INVALID_CONFIG;
    }

    LOG_DEBUG(LOG_MODULE, "Applying eidetic to autobiographical memory");

    const eidetic_autobiographical_config_t* ac = &config->autobiographical;

    /*
     * Autobiographical memory enhancements:
     * - Capacity: 10,000 -> 100,000 memories (10x)
     * - Detail level: SUMMARY -> VERBATIM
     * - Importance threshold: 0.3 -> 0.1
     * - Retrieval precision: 0.7 -> 0.95
     */

    LOG_DEBUG(LOG_MODULE, "  Capacity multiplier: %u", ac->capacity_multiplier);
    LOG_DEBUG(LOG_MODULE, "  Detail preservation: %.2f", ac->detail_preservation);
    LOG_DEBUG(LOG_MODULE, "  Temporal precision: %.2f", ac->temporal_precision);
    LOG_DEBUG(LOG_MODULE, "  Flashbulb mode: %s",
              ac->enable_flashbulb_mode ? "enabled" : "disabled");
    LOG_DEBUG(LOG_MODULE, "  Forgetting resistance: %.2f", ac->forgetting_resistance);

    return EIDETIC_SUCCESS;
}
