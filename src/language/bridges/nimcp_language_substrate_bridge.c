#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_substrate_bridge.c - Language-Substrate Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_substrate_bridge.c
 * @brief Implementation of Language-Substrate bridge for metabolic modulation
 *
 * WHAT: Bridge connecting language layer with neural substrate
 * WHY:  Enable metabolic modulation of language processing
 * HOW:  ATP/fatigue/stress/neurotransmitter effects on speech
 *
 * @version 1.0.0 - Phase L8: Additional Integration Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_substrate_bridge.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_SUBSTRATE_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_substrate_bridge)

//=============================================================================
// Configuration API Implementation
//=============================================================================

static void substrate_default_config_internal(language_substrate_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(language_substrate_config_t));

    config->enable_atp_modulation = true;
    config->enable_fatigue_effects = true;
    config->enable_stress_effects = true;
    config->enable_neurotransmitter_effects = true;

    config->atp_sensitivity = 1.0f;
    config->fatigue_sensitivity = 1.0f;
    config->stress_sensitivity = 1.0f;

    config->critical_atp_threshold = 0.2f;
    config->high_fatigue_threshold = 0.7f;
    config->optimal_stress_level = 0.3f;

    config->update_interval_ms = 100;
    config->enable_bio_async = false;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static void compute_modulation(language_substrate_bridge_t* bridge) {
    language_metabolic_state_t* state = &bridge->current_state;
    language_modulation_t* mod = &bridge->current_modulation;
    language_substrate_config_t* cfg = &bridge->config;

    /* Default modulation (no effect) */
    mod->phoneme_speed_factor = 1.0f;
    mod->word_retrieval_factor = 1.0f;
    mod->semantic_activation_factor = 1.0f;
    mod->comprehension_accuracy = 1.0f;
    mod->production_fluency = 1.0f;
    mod->working_memory_capacity = 1.0f;
    mod->attention_capacity = 1.0f;
    mod->error_rate_factor = 1.0f;

    /* ATP modulation */
    if (cfg->enable_atp_modulation) {
        float atp_factor = state->atp_level;
        if (state->atp_level < cfg->critical_atp_threshold) {
            atp_factor = 0.3f;  /* Severe impairment */
            bridge->stats.low_atp_events++;
        }
        mod->phoneme_speed_factor *= (0.5f + 0.5f * atp_factor);
        mod->word_retrieval_factor *= (0.5f + 0.5f * atp_factor);
        mod->semantic_activation_factor *= (0.5f + 0.5f * atp_factor);

        /* Track min ATP observed */
        if (state->atp_level < bridge->stats.min_atp_observed ||
            bridge->stats.min_atp_observed == 0.0f) {
            bridge->stats.min_atp_observed = state->atp_level;
        }
    }

    /* Fatigue modulation */
    if (cfg->enable_fatigue_effects) {
        float fatigue_impact = state->fatigue_level * cfg->fatigue_sensitivity;
        float max_impairment = 0.5f;  /* Cap fatigue impairment at 50% */

        if (fatigue_impact > max_impairment) {
            fatigue_impact = max_impairment;
        }

        if (state->fatigue_level > cfg->high_fatigue_threshold) {
            bridge->stats.high_fatigue_events++;
        }

        mod->production_fluency *= (1.0f - fatigue_impact);
        mod->attention_capacity *= (1.0f - fatigue_impact * 0.8f);
        mod->error_rate_factor *= (1.0f + fatigue_impact * 2.0f);

        /* Track max fatigue observed */
        if (state->fatigue_level > bridge->stats.max_fatigue_observed) {
            bridge->stats.max_fatigue_observed = state->fatigue_level;
        }
    }

    /* Stress modulation (Yerkes-Dodson inverted U) */
    if (cfg->enable_stress_effects) {
        float deviation = fabsf(state->stress_level - cfg->optimal_stress_level);
        float stress_impact = deviation * cfg->stress_sensitivity;

        if (deviation > 0.3f) {
            bridge->stats.stress_impacts++;
        }

        mod->comprehension_accuracy *= (1.0f - stress_impact * 0.4f);
        mod->working_memory_capacity *= (1.0f - stress_impact * 0.3f);
    }

    /* Clamp all factors to valid ranges */
    if (mod->phoneme_speed_factor < 0.2f) mod->phoneme_speed_factor = 0.2f;
    if (mod->word_retrieval_factor < 0.2f) mod->word_retrieval_factor = 0.2f;
    if (mod->semantic_activation_factor < 0.2f) mod->semantic_activation_factor = 0.2f;
    if (mod->comprehension_accuracy < 0.3f) mod->comprehension_accuracy = 0.3f;
    if (mod->production_fluency < 0.3f) mod->production_fluency = 0.3f;
    if (mod->working_memory_capacity < 0.3f) mod->working_memory_capacity = 0.3f;
    if (mod->attention_capacity < 0.3f) mod->attention_capacity = 0.3f;

    /* Update averages */
    bridge->stats.modulation_updates++;
    float n = (float)bridge->stats.modulation_updates;
    bridge->stats.avg_speed_factor =
        ((n - 1) * bridge->stats.avg_speed_factor + mod->phoneme_speed_factor) / n;
    bridge->stats.avg_accuracy =
        ((n - 1) * bridge->stats.avg_accuracy + mod->comprehension_accuracy) / n;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_substrate_bridge_t* language_substrate_bridge_create(
    language_orchestrator_t* orchestrator,
    const language_substrate_config_t* config)
{
    language_substrate_bridge_t* bridge = (language_substrate_bridge_t*)
        nimcp_calloc(1, sizeof(language_substrate_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(language_substrate_config_t));
    } else {
        substrate_default_config_internal(&bridge->config);
    }

    bridge->orchestrator = orchestrator;
    bridge->substrate = NULL;

    /* Initialize metabolic state to healthy defaults */
    bridge->current_state.atp_level = 1.0f;
    bridge->current_state.glucose_level = 1.0f;
    bridge->current_state.oxygen_level = 1.0f;
    bridge->current_state.fatigue_level = 0.0f;
    bridge->current_state.stress_level = 0.2f;  /* Baseline arousal */
    bridge->current_state.dopamine_level = NIMCP_DOPAMINE_BASELINE;
    bridge->current_state.acetylcholine_level = NIMCP_ACETYLCHOLINE_BASELINE;
    bridge->current_state.norepinephrine_level = 0.3f;

    /* Compute initial modulation */
    compute_modulation(bridge);

    memset(&bridge->stats, 0, sizeof(language_substrate_stats_t));
    bridge->last_update_us = 0;
    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Substrate bridge created");
    return bridge;
}

void language_substrate_bridge_destroy(language_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_substrate");

    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Substrate bridge destroyed");
}

int language_substrate_bridge_connect_substrate(
    language_substrate_bridge_t* bridge,
    neural_substrate_t* substrate)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    bridge->substrate = substrate;
    return 0;
}

//=============================================================================
// Modulation API Implementation
//=============================================================================

int language_substrate_bridge_update(language_substrate_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->active) return 0;

    /* Natural recovery */
    bridge->current_state.atp_level += 0.001f;
    if (bridge->current_state.atp_level > 1.0f) {
        bridge->current_state.atp_level = 1.0f;
    }

    bridge->current_state.fatigue_level -= 0.0005f;
    if (bridge->current_state.fatigue_level < 0.0f) {
        bridge->current_state.fatigue_level = 0.0f;
    }

    bridge->current_state.stress_level *= 0.999f;

    /* Recompute modulation */
    compute_modulation(bridge);

    return 0;
}

int language_substrate_bridge_get_modulation(
    const language_substrate_bridge_t* bridge,
    language_modulation_t* modulation)
{
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_substrate_bridge_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    memcpy(modulation, &bridge->current_modulation, sizeof(language_modulation_t));
    return 0;
}

float language_substrate_bridge_get_speed_factor(
    const language_substrate_bridge_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->current_modulation.phoneme_speed_factor;
}

float language_substrate_bridge_get_accuracy_factor(
    const language_substrate_bridge_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->current_modulation.comprehension_accuracy;
}

float language_substrate_bridge_get_fluency_factor(
    const language_substrate_bridge_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->current_modulation.production_fluency;
}

//=============================================================================
// Metabolic State API Implementation
//=============================================================================

int language_substrate_bridge_get_metabolic_state(
    const language_substrate_bridge_t* bridge,
    language_metabolic_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_substrate_bridge_get_metabolic_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    memcpy(state, &bridge->current_state, sizeof(language_metabolic_state_t));
    return 0;
}

bool language_substrate_bridge_is_impaired(
    const language_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    return (bridge->current_state.atp_level < bridge->config.critical_atp_threshold ||
            bridge->current_state.fatigue_level > bridge->config.high_fatigue_threshold ||
            bridge->current_modulation.comprehension_accuracy < 0.7f);
}

const char* language_substrate_bridge_get_impairment_reason(
    const language_substrate_bridge_t* bridge)
{
    if (!bridge) return "invalid bridge";

    if (bridge->current_state.atp_level < bridge->config.critical_atp_threshold) {
        return "critical ATP depletion";
    }
    if (bridge->current_state.fatigue_level > bridge->config.high_fatigue_threshold) {
        return "severe fatigue";
    }
    if (bridge->current_state.stress_level > 0.8f) {
        return "excessive stress";
    }
    return "none";
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int language_substrate_bridge_get_stats(
    const language_substrate_bridge_t* bridge,
    language_substrate_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_substrate_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(language_substrate_stats_t));
    return 0;
}

void language_substrate_bridge_reset_stats(language_substrate_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(language_substrate_stats_t));
}
