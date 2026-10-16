/**
 * @file nimcp_neuromodulatory_immune_bridge.c
 * @brief Implementation of Unified Neuromodulatory-Immune Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/nimcp_neuromodulatory_immune_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(neuromodulatory_immune_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "NEUROMODULATORY_IMMUNE_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_immune_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    uint32_t magic;
    neuromod_immune_bridge_config_t config;

    /* Adapter connections */
    nimcp_lc_adapter_t lc_adapter;
    nimcp_vta_adapter_t vta_adapter;
    nimcp_raphe_adapter_t raphe_adapter;
    nimcp_habenula_adapter_t habenula_adapter;

    /* Immune system connection */
    nimcp_immune_system_t immune;
    bool connected;

    /* State caches */
    neuromod_immune_modulation_t modulation;
    neuromod_immune_feedback_t feedback;

    /* Stress tracking */
    float continuous_stress_duration_ms;

    /* Timing */
    uint64_t last_update_us;
    float time_since_update_ms;

    /* Statistics */
    neuromod_immune_bridge_stats_t stats;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int neuromod_immune_bridge_default_config(neuromod_immune_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_bridge_default_config: config is NULL");
        return -1;
    }

    config->enable_lc_immune_modulation = true;
    config->enable_vta_immune_modulation = true;
    config->enable_raphe_immune_modulation = true;
    config->enable_habenula_immune_modulation = true;

    config->enable_cytokine_feedback = true;
    config->enable_inflammation_feedback = true;

    config->ne_acute_weight = NE_ACUTE_IL6_BOOST;
    config->ne_chronic_weight = NE_CHRONIC_IL10_SUPPRESS;
    config->da_reward_weight = DA_IL10_BOOST;
    config->ht_mood_weight = HT_IL10_BOOST;
    config->hab_suppression_weight = HAB_IMMUNOSUPPRESS_FACTOR;

    config->il1_fatigue_sensitivity = IL1_NE_FATIGUE_FACTOR;
    config->il6_anhedonia_sensitivity = IL6_DA_ANHEDONIA_FACTOR;
    config->tnf_depression_sensitivity = TNF_5HT_SUPPRESS_FACTOR;
    config->ifn_depletion_sensitivity = IFN_TRYPTOPHAN_DEPLETION;

    config->chronic_stress_threshold_ms = 30000.0f;  /* 30 seconds of continuous stress */

    config->update_interval_ms = NEUROMOD_IMM_DEFAULT_UPDATE_MS;
    config->broadcast_on_change = true;

    config->event_buffer_size = NEUROMOD_IMM_MAX_EVENT_BUFFER;

    return 0;
}

neuromod_immune_bridge_t* neuromod_immune_bridge_create(const neuromod_immune_bridge_config_t* config) {
    neuromod_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(neuromod_immune_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->magic = NEUROMOD_IMMUNE_BRIDGE_MAGIC;

    if (config) {
        bridge->config = *config;
    } else {
        neuromod_immune_bridge_default_config(&bridge->config);
    }

    /* Initialize default modulation state (neutral) */
    bridge->modulation.il1_modulation = 1.0f;
    bridge->modulation.il6_modulation = 1.0f;
    bridge->modulation.il10_modulation = 1.0f;
    bridge->modulation.tnf_modulation = 1.0f;
    bridge->modulation.ifn_modulation = 1.0f;

    bridge->last_update_us = get_timestamp_us();
    NIMCP_LOGGING_INFO("Created %s bridge", "neuromodulatory_immune");
    return bridge;
}

void neuromod_immune_bridge_destroy(neuromod_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromodulatory_immune");
    if (bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) return;

    if (bridge->connected) {
        neuromod_immune_bridge_disconnect(bridge);
    }

    bridge->magic = 0;
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection
 * ============================================================================ */

int neuromod_immune_bridge_connect_immune(neuromod_immune_bridge_t* bridge, nimcp_immune_system_t immune) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_connect_immune: bridge is NULL");
        return -1;
    }

    bridge->immune = immune;
    bridge->connected = true;

    return 0;
}

int neuromod_immune_bridge_disconnect(neuromod_immune_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->immune = NULL;
    bridge->connected = false;

    return 0;
}

bool neuromod_immune_bridge_is_connected(const neuromod_immune_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Adapter Registration
 * ============================================================================ */

int neuromod_immune_bridge_register_lc(neuromod_immune_bridge_t* bridge, nimcp_lc_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_register_lc: bridge is NULL");
        return -1;
    }
    bridge->lc_adapter = adapter;
    return 0;
}

int neuromod_immune_bridge_register_vta(neuromod_immune_bridge_t* bridge, nimcp_vta_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_register_vta: bridge is NULL");
        return -1;
    }
    bridge->vta_adapter = adapter;
    return 0;
}

int neuromod_immune_bridge_register_raphe(neuromod_immune_bridge_t* bridge, nimcp_raphe_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_register_raphe: bridge is NULL");
        return -1;
    }
    bridge->raphe_adapter = adapter;
    return 0;
}

int neuromod_immune_bridge_register_habenula(neuromod_immune_bridge_t* bridge, nimcp_habenula_adapter_t adapter) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_register_habenula: bridge is NULL");
        return -1;
    }
    bridge->habenula_adapter = adapter;
    return 0;
}

/* ============================================================================
 * Update and Processing
 * ============================================================================ */

int neuromod_immune_bridge_update(neuromod_immune_bridge_t* bridge, float delta_ms) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_update: bridge is NULL");
        return -1;
    }

    bridge->time_since_update_ms += delta_ms;
    bridge->modulation.timestamp_us = get_timestamp_us();

    /* Update stress duration tracking */
    if (bridge->modulation.ne_level > 0.7f) {
        bridge->continuous_stress_duration_ms += delta_ms;
        if (bridge->continuous_stress_duration_ms > bridge->config.chronic_stress_threshold_ms) {
            bridge->modulation.chronic_stress_mode = true;
            bridge->modulation.acute_stress_mode = false;
        }
    } else {
        bridge->continuous_stress_duration_ms = 0.0f;
        bridge->modulation.chronic_stress_mode = false;
    }

    return 0;
}

int neuromod_immune_bridge_process_events(neuromod_immune_bridge_t* bridge, uint32_t max_events) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_process_events: bridge is NULL");
        return -1;
    }

    /* Process pending immune events - stub for now */
    (void)max_events;
    return 0;
}

/* ============================================================================
 * Neuromodulatory -> Immune Modulation
 * ============================================================================ */

int neuromod_immune_apply_ne_stress(neuromod_immune_bridge_t* bridge, const neuromod_imm_ne_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_apply_ne_stress: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_lc_immune_modulation) return 0;

    bridge->modulation.ne_level = nimcp_clampf(payload->ne_level, 0.0f, 1.0f);

    /* Determine acute vs chronic based on duration */
    if (payload->stress_duration_ms < bridge->config.chronic_stress_threshold_ms) {
        /* Acute stress: immune mobilization */
        bridge->modulation.acute_stress_mode = true;
        bridge->modulation.chronic_stress_mode = false;
        bridge->modulation.acute_stress_mobilization =
            payload->mobilization_strength * bridge->config.ne_acute_weight;
        bridge->modulation.chronic_stress_suppression = 0.0f;
    } else {
        /* Chronic stress: immunosuppression */
        bridge->modulation.acute_stress_mode = false;
        bridge->modulation.chronic_stress_mode = true;
        bridge->modulation.acute_stress_mobilization = 0.0f;
        bridge->modulation.chronic_stress_suppression =
            payload->mobilization_strength * bridge->config.ne_chronic_weight;
    }

    bridge->stats.lc_immune_modulations++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    /* Recompute net cytokine modulation */
    neuromod_immune_compute_modulation(bridge);

    return 0;
}

int neuromod_immune_apply_da_reward(neuromod_immune_bridge_t* bridge, const neuromod_imm_da_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_apply_da_reward: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_vta_immune_modulation) return 0;

    bridge->modulation.da_level = nimcp_clampf(payload->da_level, 0.0f, 1.0f);
    bridge->modulation.positive_affect = payload->positive_outcome;

    if (payload->positive_outcome) {
        /* Positive state: anti-inflammatory */
        bridge->modulation.reward_anti_inflammatory =
            payload->da_level * bridge->config.da_reward_weight;
        bridge->modulation.anhedonia_pro_inflammatory = 0.0f;
    } else {
        /* Anhedonia/low DA: pro-inflammatory */
        bridge->modulation.reward_anti_inflammatory = 0.0f;
        bridge->modulation.anhedonia_pro_inflammatory =
            (1.0f - payload->da_level) * DA_TNF_SUPPRESS;
    }

    bridge->stats.vta_immune_modulations++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    neuromod_immune_compute_modulation(bridge);

    return 0;
}

int neuromod_immune_apply_ht_mood(neuromod_immune_bridge_t* bridge, const neuromod_imm_ht_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_apply_ht_mood: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_raphe_immune_modulation) return 0;

    bridge->modulation.ht_level = nimcp_clampf(payload->ht_level, 0.0f, 1.0f);
    bridge->modulation.good_mood = (payload->mood_valence > 0.0f);

    if (payload->mood_valence > 0.0f) {
        /* Good mood: anti-inflammatory */
        bridge->modulation.mood_anti_inflammatory =
            payload->ht_level * bridge->config.ht_mood_weight;
        bridge->modulation.depression_pro_inflammatory = 0.0f;
    } else {
        /* Depression/low 5-HT: pro-inflammatory */
        bridge->modulation.mood_anti_inflammatory = 0.0f;
        bridge->modulation.depression_pro_inflammatory =
            fabsf(payload->mood_valence) * HT_LOW_INFLAMMATION_BOOST;
    }

    bridge->stats.raphe_immune_modulations++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    neuromod_immune_compute_modulation(bridge);

    return 0;
}

int neuromod_immune_apply_hab_aversion(neuromod_immune_bridge_t* bridge, const neuromod_imm_hab_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_apply_hab_aversion: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_habenula_immune_modulation) return 0;

    bridge->modulation.habenula_activation = nimcp_clampf(payload->habenula_activation, 0.0f, 1.0f);
    bridge->modulation.chronic_aversion_suppression =
        payload->suppression_strength * bridge->config.hab_suppression_weight;

    bridge->stats.habenula_immune_modulations++;
    bridge->stats.total_events_sent++;
    bridge->stats.last_activity_us = get_timestamp_us();

    neuromod_immune_compute_modulation(bridge);

    return 0;
}

/* ============================================================================
 * Immune -> Neuromodulatory Feedback (Sickness Behavior)
 * ============================================================================ */

int neuromod_immune_report_cytokines(neuromod_immune_bridge_t* bridge, const neuromod_imm_cytokine_payload_t* payload) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !payload) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_report_cytokines: required parameter is NULL (bridge, payload)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_feedback) return 0;

    /* Update cytokine levels */
    bridge->feedback.il1_level = nimcp_clampf(payload->il1_level, 0.0f, 1.0f);
    bridge->feedback.il6_level = nimcp_clampf(payload->il6_level, 0.0f, 1.0f);
    bridge->feedback.il10_level = nimcp_clampf(payload->il10_level, 0.0f, 1.0f);
    bridge->feedback.tnf_level = nimcp_clampf(payload->tnf_level, 0.0f, 1.0f);
    bridge->feedback.ifn_level = nimcp_clampf(payload->ifn_level, 0.0f, 1.0f);
    bridge->feedback.inflammation_level = nimcp_clampf(payload->inflammation_level, 0.0f, 1.0f);
    bridge->feedback.systemic_inflammation = (payload->inflammation_level > 0.7f);
    bridge->feedback.cytokine_storm = payload->urgent;
    bridge->feedback.last_update_us = get_timestamp_us();

    /* Compute sickness behavior effects */
    bridge->feedback.fatigue_induction =
        payload->il1_level * bridge->config.il1_fatigue_sensitivity;
    bridge->feedback.anhedonia_induction =
        payload->il6_level * bridge->config.il6_anhedonia_sensitivity;
    bridge->feedback.depression_induction =
        payload->tnf_level * bridge->config.tnf_depression_sensitivity;
    bridge->feedback.tryptophan_depletion =
        payload->ifn_level * bridge->config.ifn_depletion_sensitivity;

    bridge->stats.cytokine_events_received++;
    bridge->stats.total_events_received++;
    bridge->stats.last_activity_us = get_timestamp_us();

    /* Track correlation with stress */
    if (bridge->modulation.ne_level > 0.7f && payload->inflammation_level > 0.5f) {
        bridge->stats.high_stress_inflammation_correlation++;
    }
    if (bridge->modulation.positive_affect && payload->inflammation_level < 0.3f) {
        bridge->stats.positive_affect_recovery_correlation++;
    }

    /* Send sickness behavior signals */
    if (bridge->feedback.fatigue_induction > 0.3f) {
        bridge->stats.fatigue_signals_sent++;
    }
    if (bridge->feedback.anhedonia_induction > 0.3f) {
        bridge->stats.anhedonia_signals_sent++;
    }
    if (bridge->feedback.depression_induction > 0.3f) {
        bridge->stats.depression_signals_sent++;
    }

    return 0;
}

/* ============================================================================
 * Compute Net Cytokine Modulation
 * ============================================================================ */

int neuromod_immune_compute_modulation(neuromod_immune_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_compute_modulation: bridge is NULL");
        return -1;
    }

    /* Start from neutral (1.0 = no change) */
    float il1_mod = 1.0f;
    float il6_mod = 1.0f;
    float il10_mod = 1.0f;
    float tnf_mod = 1.0f;
    float ifn_mod = 1.0f;

    /* NE effects */
    if (bridge->modulation.acute_stress_mode) {
        /* Acute stress: boost IL-6 for immune mobilization */
        il6_mod += bridge->modulation.acute_stress_mobilization;
    }
    if (bridge->modulation.chronic_stress_mode) {
        /* Chronic stress: suppress IL-10 (immunosuppression) */
        il10_mod -= bridge->modulation.chronic_stress_suppression;
    }

    /* DA effects */
    if (bridge->modulation.positive_affect) {
        /* Positive state: boost IL-10, reduce TNF */
        il10_mod += bridge->modulation.reward_anti_inflammatory;
        tnf_mod -= bridge->modulation.reward_anti_inflammatory * 0.8f;
    } else {
        /* Anhedonia: increase TNF */
        tnf_mod += bridge->modulation.anhedonia_pro_inflammatory;
    }

    /* 5-HT effects */
    if (bridge->modulation.good_mood) {
        /* Good mood: boost IL-10 */
        il10_mod += bridge->modulation.mood_anti_inflammatory;
    } else {
        /* Depression: increase IL-1, IL-6 */
        il1_mod += bridge->modulation.depression_pro_inflammatory;
        il6_mod += bridge->modulation.depression_pro_inflammatory * 0.8f;
    }

    /* Habenula effects (general suppression) */
    if (bridge->modulation.habenula_activation > 0.5f) {
        /* Chronic aversion: general immunosuppression (reduced response) */
        il1_mod -= bridge->modulation.chronic_aversion_suppression * 0.3f;
        il6_mod -= bridge->modulation.chronic_aversion_suppression * 0.3f;
        il10_mod -= bridge->modulation.chronic_aversion_suppression * 0.5f;
    }

    /* Clamp and store */
    bridge->modulation.il1_modulation = nimcp_clampf(il1_mod, 0.1f, 2.0f);
    bridge->modulation.il6_modulation = nimcp_clampf(il6_mod, 0.1f, 2.0f);
    bridge->modulation.il10_modulation = nimcp_clampf(il10_mod, 0.1f, 2.0f);
    bridge->modulation.tnf_modulation = nimcp_clampf(tnf_mod, 0.1f, 2.0f);
    bridge->modulation.ifn_modulation = nimcp_clampf(ifn_mod, 0.1f, 2.0f);

    bridge->modulation.timestamp_us = get_timestamp_us();

    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int neuromod_immune_bridge_get_modulation(const neuromod_immune_bridge_t* bridge, neuromod_immune_modulation_t* modulation) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_bridge_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    *modulation = bridge->modulation;
    return 0;
}

int neuromod_immune_bridge_get_feedback(const neuromod_immune_bridge_t* bridge, neuromod_immune_feedback_t* feedback) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_bridge_get_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }
    *feedback = bridge->feedback;
    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int neuromod_immune_bridge_get_stats(const neuromod_immune_bridge_t* bridge, neuromod_immune_bridge_stats_t* stats) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int neuromod_immune_bridge_reset_stats(neuromod_immune_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

static const char* event_names[] = {
    "NE_ACUTE_STRESS", "NE_CHRONIC_STRESS",
    "DA_REWARD_STATE", "DA_ANHEDONIA",
    "5HT_MOOD_POSITIVE", "5HT_MOOD_NEGATIVE",
    "HAB_CHRONIC_AVERSION",
    "IL1_FATIGUE", "IL6_MOTIVATIONAL", "TNF_DEPRESSIVE",
    "IFN_TRYPTOPHAN", "INFLAMMATION_GENERAL"
};

const char* neuromod_imm_event_name(neuromod_imm_event_t event) {
    if (event >= NEUROMOD_IMM_EVENT_COUNT) return "UNKNOWN";
    return event_names[event];
}

void neuromod_immune_bridge_print_summary(const neuromod_immune_bridge_t* bridge) {
    if (!bridge || bridge->magic != NEUROMOD_IMMUNE_BRIDGE_MAGIC) {
        printf("Neuromodulatory Immune Bridge: NULL or invalid\n");
        return;
    }

    printf("Neuromodulatory Immune Bridge Summary:\n");
    printf("  Connected: %s\n", bridge->connected ? "yes" : "no");
    printf("  Adapters registered:\n");
    printf("    LC: %s, VTA: %s, Raphe: %s, Habenula: %s\n",
           bridge->lc_adapter ? "yes" : "no",
           bridge->vta_adapter ? "yes" : "no",
           bridge->raphe_adapter ? "yes" : "no",
           bridge->habenula_adapter ? "yes" : "no");
    printf("  Modulations sent:\n");
    printf("    LC: %u, VTA: %u, Raphe: %u, Habenula: %u\n",
           bridge->stats.lc_immune_modulations,
           bridge->stats.vta_immune_modulations,
           bridge->stats.raphe_immune_modulations,
           bridge->stats.habenula_immune_modulations);
    printf("  Cytokine feedback: %u events\n", bridge->stats.cytokine_events_received);
    printf("  Sickness behavior signals:\n");
    printf("    Fatigue: %u, Anhedonia: %u, Depression: %u\n",
           bridge->stats.fatigue_signals_sent,
           bridge->stats.anhedonia_signals_sent,
           bridge->stats.depression_signals_sent);
    printf("  Current neuromodulator levels:\n");
    printf("    NE: %.2f, DA: %.2f, 5-HT: %.2f, Hab: %.2f\n",
           bridge->modulation.ne_level, bridge->modulation.da_level,
           bridge->modulation.ht_level, bridge->modulation.habenula_activation);
    printf("  Cytokine modulation (1.0=neutral):\n");
    printf("    IL-1: %.2f, IL-6: %.2f, IL-10: %.2f, TNF: %.2f\n",
           bridge->modulation.il1_modulation, bridge->modulation.il6_modulation,
           bridge->modulation.il10_modulation, bridge->modulation.tnf_modulation);
}
