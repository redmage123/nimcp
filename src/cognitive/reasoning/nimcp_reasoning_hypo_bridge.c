/**
 * @file nimcp_reasoning_hypo_bridge.c
 * @brief Hypothalamus-Reasoning Bridge — motivational modulation of reasoning depth
 *
 * WHAT: Queries the brain's hypothalamus for cognitive/motivational state,
 *       computes reasoning modulation, and applies it to the engine config
 * WHY:  Reasoning depth should adapt to stress, circadian alertness, and urgency
 * HOW:  hypothalamus_get_state() → map fields → reasoning_hypo_modulation_t → apply
 *
 * MODULATION RULES:
 *   RELAXED (rest-and-digest) → full pipeline, no constraints
 *   NORMAL                    → full pipeline
 *   ALERT (sympathetic > 0.7) → cap steps to 30, no other changes
 *   FIGHT_OR_FLIGHT           → cap steps to 10, force sequential
 *
 *   cognitive_capacity = alertness * (1.0 - sleep_pressure * 0.7) * (1.0 - stress * 0.3)
 *   If capacity < 0.3 → force sequential (avoid thread contention when cognitively impaired)
 *   max_steps scaled by capacity: effective_steps = max_steps * max(capacity, 0.2)
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#include "cognitive/reasoning/nimcp_reasoning_hypo_bridge.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "reasoning_hypo"

/*=============================================================================
 * NEUTRAL MODULATION
 *===========================================================================*/

reasoning_hypo_modulation_t reasoning_hypo_neutral_modulation(void) {
    reasoning_hypo_modulation_t mod;
    memset(&mod, 0, sizeof(mod));

    mod.cognitive_capacity = 1.0f;
    mod.urgency_mode = REASONING_URGENCY_NORMAL;
    mod.stress_level = 0.0f;
    mod.alertness = 1.0f;
    mod.sleep_pressure = 0.0f;
    mod.recommended_max_steps = 0;  /* 0 = no override */
    mod.force_sequential = false;
    mod.force_wave_pipeline = false;
    mod.hypothalamus_available = false;

    return mod;
}

/*=============================================================================
 * COMPUTE MODULATION
 *===========================================================================*/

reasoning_hypo_modulation_t reasoning_hypo_compute_modulation(brain_t brain) {
    if (!brain) {
        NIMCP_LOGGING_DEBUG("No brain — returning neutral modulation");
        return reasoning_hypo_neutral_modulation();
    }

    /* Access brain internal struct to get hypothalamus pointer */
    struct brain_struct* b = (struct brain_struct*)brain;

    if (!b->hypothalamus_enabled || !b->hypothalamus) {
        NIMCP_LOGGING_DEBUG("Hypothalamus not enabled — returning neutral modulation");
        return reasoning_hypo_neutral_modulation();
    }

    /* Query full hypothalamus state */
    hypothalamus_state_t state;
    memset(&state, 0, sizeof(state));

    bool ok = hypothalamus_get_state(b->hypothalamus, &state);
    if (!ok) {
        NIMCP_LOGGING_WARN("hypothalamus_get_state() failed — returning neutral");
        return reasoning_hypo_neutral_modulation();
    }

    reasoning_hypo_modulation_t mod;
    memset(&mod, 0, sizeof(mod));
    mod.hypothalamus_available = true;

    /* Extract key signals (guard against NaN/Inf from hypothalamus) */
    mod.alertness = isfinite(state.circadian.alertness) ? state.circadian.alertness : 1.0f;
    mod.sleep_pressure = isfinite(state.circadian.sleep_pressure) ? state.circadian.sleep_pressure : 0.0f;
    mod.stress_level = isfinite(state.hpa_axis.cortisol_level) ? state.hpa_axis.cortisol_level : 0.0f;

    /*---------------------------------------------------------------
     * Determine urgency mode from autonomic state
     *---------------------------------------------------------------*/
    if (state.autonomic.fight_or_flight) {
        mod.urgency_mode = REASONING_URGENCY_FIGHT_OR_FLIGHT;
    } else if (state.autonomic.sympathetic_tone > 0.7f) {
        mod.urgency_mode = REASONING_URGENCY_ALERT;
    } else if (state.autonomic.rest_and_digest) {
        mod.urgency_mode = REASONING_URGENCY_RELAXED;
    } else {
        mod.urgency_mode = REASONING_URGENCY_NORMAL;
    }

    /*---------------------------------------------------------------
     * Compute cognitive capacity
     * capacity = alertness * (1 - sleep_pressure*0.7) * (1 - stress*0.3)
     *---------------------------------------------------------------*/
    float fatigue_factor = 1.0f - mod.sleep_pressure * 0.7f;
    if (fatigue_factor < 0.1f) fatigue_factor = 0.1f;

    float stress_factor = 1.0f - mod.stress_level * 0.3f;
    if (stress_factor < 0.1f) stress_factor = 0.1f;

    mod.cognitive_capacity = mod.alertness * fatigue_factor * stress_factor;
    if (!isfinite(mod.cognitive_capacity)) mod.cognitive_capacity = 0.5f;
    if (mod.cognitive_capacity < 0.0f) mod.cognitive_capacity = 0.0f;
    if (mod.cognitive_capacity > 1.0f) mod.cognitive_capacity = 1.0f;

    /*---------------------------------------------------------------
     * Recommended max_steps based on urgency
     *---------------------------------------------------------------*/
    switch (mod.urgency_mode) {
        case REASONING_URGENCY_FIGHT_OR_FLIGHT:
            mod.recommended_max_steps = 10;
            mod.force_sequential = true;
            mod.force_wave_pipeline = true;  /* No convergent under acute stress */
            break;
        case REASONING_URGENCY_ALERT:
            mod.recommended_max_steps = 30;
            break;
        case REASONING_URGENCY_RELAXED:
        case REASONING_URGENCY_NORMAL:
            mod.recommended_max_steps = 0;  /* no override */
            break;
    }

    /* Low cognitive capacity → force sequential */
    if (mod.cognitive_capacity < 0.3f) {
        mod.force_sequential = true;
    }

    NIMCP_LOGGING_DEBUG("Hypo modulation: urgency=%d capacity=%.2f stress=%.2f "
                        "alertness=%.2f sleep_pressure=%.2f max_steps=%u sequential=%s",
                        (int)mod.urgency_mode, (double)mod.cognitive_capacity,
                        (double)mod.stress_level, (double)mod.alertness,
                        (double)mod.sleep_pressure, mod.recommended_max_steps,
                        mod.force_sequential ? "yes" : "no");

    return mod;
}

/*=============================================================================
 * APPLY MODULATION
 *===========================================================================*/

int reasoning_hypo_apply_modulation(reasoning_engine_config_t* config,
                                     const reasoning_hypo_modulation_t* mod) {
    if (!config || !mod) return -1;

    /* If hypothalamus wasn't available, nothing to apply */
    if (!mod->hypothalamus_available) return 0;

    /* Force sequential if indicated */
    if (mod->force_sequential) {
        config->enable_concurrent_pipeline = false;
    }

    /* Force wave pipeline (disable convergent) */
    if (mod->force_wave_pipeline) {
        config->enable_convergent_reasoning = false;
        config->enable_concurrent_pipeline = false;
    }

    /* Apply urgency-based step cap */
    if (mod->recommended_max_steps > 0) {
        if (config->max_steps > mod->recommended_max_steps) {
            config->max_steps = mod->recommended_max_steps;
        }
    }

    /* Scale max_steps by cognitive capacity (floor at 20% of original) */
    if (mod->cognitive_capacity < 1.0f) {
        float capacity = mod->cognitive_capacity;
        if (capacity < 0.2f) capacity = 0.2f;

        uint32_t scaled = (uint32_t)((float)config->max_steps * capacity);
        if (scaled < 3) scaled = 3;  /* absolute minimum */
        if (scaled < config->max_steps) {
            config->max_steps = scaled;
        }
    }

    return 0;
}

/*=============================================================================
 * MODULATION SUMMARY
 *===========================================================================*/

int reasoning_hypo_modulation_summary(const reasoning_hypo_modulation_t* mod,
                                       char* buffer, uint32_t buffer_size) {
    if (!mod || !buffer || buffer_size == 0) return -1;

    const char* urgency_name;
    switch (mod->urgency_mode) {
        case REASONING_URGENCY_RELAXED:        urgency_name = "RELAXED";        break;
        case REASONING_URGENCY_NORMAL:         urgency_name = "NORMAL";         break;
        case REASONING_URGENCY_ALERT:          urgency_name = "ALERT";          break;
        case REASONING_URGENCY_FIGHT_OR_FLIGHT: urgency_name = "FIGHT_OR_FLIGHT"; break;
        default:                               urgency_name = "UNKNOWN";        break;
    }

    int written = snprintf(buffer, buffer_size,
        "Hypo modulation [%s]:"
        " capacity=%.2f"
        " alertness=%.2f"
        " stress=%.2f"
        " sleep_pressure=%.2f"
        " max_steps=%u"
        " sequential=%s"
        " available=%s",
        urgency_name,
        (double)mod->cognitive_capacity,
        (double)mod->alertness,
        (double)mod->stress_level,
        (double)mod->sleep_pressure,
        mod->recommended_max_steps,
        mod->force_sequential ? "yes" : "no",
        mod->hypothalamus_available ? "yes" : "no");

    return written;
}
