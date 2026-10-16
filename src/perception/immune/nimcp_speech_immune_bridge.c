/**
 * @file nimcp_speech_immune_bridge.c
 * @brief Speech Cortex-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and speech processing systems
 * WHY:  Biological realism - cytokines affect speech fluency, distress affects immunity
 * HOW:  Monitor cytokine levels to modulate speech, monitor speech to trigger immune responses
 */

#include "perception/immune/nimcp_speech_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(speech_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get inflammation duration from immune system
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>3 days) has different speech effects
 * HOW:  Find oldest active inflammation site, compute duration
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;

    /* Query immune system for oldest inflammation site */
    float max_duration = 0.0f;
    /* P0 fix: Add bounds check to prevent array overflow */
    size_t count = (immune->inflammation_count < BRAIN_IMMUNE_MAX_INFLAMMATION) ?
                   immune->inflammation_count : BRAIN_IMMUNE_MAX_INFLAMMATION;
    for (size_t i = 0; i < count; i++) {
        brain_inflammation_site_t* site = &immune->inflammation_sites[i];
        uint64_t current_time = 0; /* Would get from system time */
        float duration = (current_time - site->start_time) / 1000.0f;
        if (duration > max_duration) {
            max_duration = duration;
        }
    }
    return max_duration;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines speech impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    /* P0 fix: Add bounds check to prevent array overflow */
    size_t count = (immune->inflammation_count < BRAIN_IMMUNE_MAX_INFLAMMATION) ?
                   immune->inflammation_count : BRAIN_IMMUNE_MAX_INFLAMMATION;
    for (size_t i = 0; i < count; i++) {
        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }
    return max_level;
}

/**
 * @brief Check if word is illness-related
 *
 * WHAT: Determine if word indicates illness expression
 * WHY:  Illness words trigger immune modulation
 * HOW:  Simple keyword matching (could be more sophisticated)
 */
static bool is_illness_word(const char* word) {
    if (!word) {
        return false;  /* NULL word is not an illness word */
    }

    /* Common illness-related words */
    const char* illness_words[] = {
        "sick", "ill", "pain", "hurt", "ache", "tired", "fatigue",
        "weak", "dizzy", "nauseous", "fever", "cold", "flu",
        NULL
    };

    for (int i = 0; illness_words[i] != NULL; i++) {
        if (strcmp(word, illness_words[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int speech_immune_default_config(speech_immune_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "speech_immune_default_config: NULL config");

    /* All features enabled by default */
    config->enable_cytokine_speech_modulation = true;
    config->enable_inflammation_impairment = true;
    config->enable_speech_immune_trigger = true;
    config->enable_distress_vocalization_trigger = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->speech_trigger_sensitivity = 1.0f;

    /* Evidence-based thresholds */
    config->effort_trigger_threshold = SPEECH_EFFORT_IMMUNE_TRIGGER;
    config->distress_threshold = DISTRESS_VOCALIZATION_THRESHOLD;

    return 0;
}

speech_immune_bridge_t* speech_immune_bridge_create(
    const speech_immune_config_t* config,
    brain_immune_system_t* immune_system,
    speech_cortex_t* speech_cortex
) {
    /* Guard: require immune and speech systems */
    NIMCP_API_CHECK_NULL_RET_NULL(immune_system, "speech_immune_bridge_create: NULL immune_system");
    NIMCP_API_CHECK_NULL_RET_NULL(speech_cortex, "speech_immune_bridge_create: NULL speech_cortex");

    /* Allocate bridge */
    speech_immune_bridge_t* bridge = (speech_immune_bridge_t*)
        nimcp_malloc(sizeof(speech_immune_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "speech_immune_bridge_create: Failed to allocate bridge");

    /* Initialize to zero */
    memset(bridge, 0, sizeof(speech_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->speech_cortex = speech_cortex;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_speech_modulation = config->enable_cytokine_speech_modulation;
        bridge->enable_inflammation_impairment = config->enable_inflammation_impairment;
        bridge->enable_speech_immune_trigger = config->enable_speech_immune_trigger;
        bridge->enable_distress_vocalization_trigger = config->enable_distress_vocalization_trigger;
    } else {
        /* Use defaults */
        speech_immune_config_t default_cfg;
        speech_immune_default_config(&default_cfg);
        bridge->enable_cytokine_speech_modulation = default_cfg.enable_cytokine_speech_modulation;
        bridge->enable_inflammation_impairment = default_cfg.enable_inflammation_impairment;
        bridge->enable_speech_immune_trigger = default_cfg.enable_speech_immune_trigger;
        bridge->enable_distress_vocalization_trigger = default_cfg.enable_distress_vocalization_trigger;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "speech_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    LOG_MODULE_INFO("speech_immune_bridge", "Bridge created successfully");
    return bridge;
}

void speech_immune_bridge_destroy(speech_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("speech_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Speech Implementation
 * ============================================================================ */

int speech_immune_apply_cytokine_effects(speech_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_apply_cytokine_effects: NULL bridge");
    if (!bridge->enable_cytokine_speech_modulation) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "speech_immune_apply_cytokine_effects: NULL immune_system");
    NIMCP_API_CHECK_NULL(bridge->speech_cortex, -1, "speech_immune_apply_cytokine_effects: NULL speech_cortex");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Query cytokine levels from immune system */
    /* In a full implementation, we'd query actual cytokine concentrations */
    float il1_level = 0.0f;
    float il6_level = 0.0f;
    float tnf_level = 0.0f;
    float ifn_gamma_level = 0.0f;

    /* Iterate through active cytokines (P1 fix: bounds check) */
    size_t cytokine_limit = (bridge->immune_system->cytokine_count < BRAIN_IMMUNE_MAX_CYTOKINES) ?
                            bridge->immune_system->cytokine_count : BRAIN_IMMUNE_MAX_CYTOKINES;
    for (size_t i = 0; i < cytokine_limit; i++) {
        brain_cytokine_t* cytokine = &bridge->immune_system->cytokines[i];
        /* Include all cytokines (delivered flag is for bio-async routing).
         * When no bio-async context, cytokines are effectively local/immediate. */

        switch (cytokine->type) {
            case BRAIN_CYTOKINE_IL1:
                il1_level += cytokine->concentration;
                break;
            case BRAIN_CYTOKINE_IL6:
                il6_level += cytokine->concentration;
                break;
            case BRAIN_CYTOKINE_TNF:
                tnf_level += cytokine->concentration;
                break;
            case BRAIN_CYTOKINE_IFN_GAMMA:
                ifn_gamma_level += cytokine->concentration;
                break;
            default:
                break;
        }
    }

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_fluency_reduction =
        nimcp_clampf(il1_level * CYTOKINE_IL1_FLUENCY_IMPACT, -1.0f, 0.0f);
    bridge->cytokine_effects.il6_word_retrieval_delay =
        nimcp_clampf(il6_level * CYTOKINE_IL6_FLUENCY_IMPACT, -1.0f, 0.0f);
    bridge->cytokine_effects.tnf_phoneme_discrimination =
        nimcp_clampf(tnf_level * CYTOKINE_TNF_FLUENCY_IMPACT, -1.0f, 0.0f);
    bridge->cytokine_effects.ifn_gamma_prosody_reduction =
        nimcp_clampf(ifn_gamma_level * CYTOKINE_IFN_GAMMA_FLUENCY_IMPACT, -1.0f, 0.0f);

    /* Compute aggregate effects */
    bridge->cytokine_effects.total_fluency_impairment = nimcp_clampf(
        -(bridge->cytokine_effects.il1_fluency_reduction +
          bridge->cytokine_effects.il6_word_retrieval_delay +
          bridge->cytokine_effects.tnf_phoneme_discrimination),
        0.0f, 1.0f
    );

    /* IL-6 specifically affects word retrieval latency (Marsland et al. 2006) */
    bridge->cytokine_effects.word_retrieval_latency_ms =
        il6_level * 200.0f; /* Up to 200ms added latency */

    /* TNF-α increases phoneme errors */
    bridge->cytokine_effects.phoneme_error_rate =
        nimcp_clampf(tnf_level * 0.3f, 0.0f, 0.3f);

    /* Speech rate reduction from overall cytokine burden */
    bridge->cytokine_effects.speech_rate_reduction =
        bridge->cytokine_effects.total_fluency_impairment;

    /* Prosody flattening */
    bridge->cytokine_effects.prosody_flattening =
        nimcp_clampf(-bridge->cytokine_effects.ifn_gamma_prosody_reduction, 0.0f, 0.8f);

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int speech_immune_apply_inflammation_effects(speech_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_apply_inflammation_effects: NULL bridge");
    if (!bridge->enable_inflammation_impairment) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "speech_immune_apply_inflammation_effects: NULL immune_system");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get inflammation state */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float duration = get_inflammation_duration_sec(bridge->immune_system);

    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.inflammation_duration_sec = duration;
    bridge->inflammation_state.is_chronic = (duration >= CHRONIC_SICKNESS_THRESHOLD);

    /* Map inflammation level to speech impairments */
    float inflammation_factor = 0.0f;
    switch (level) {
        case INFLAMMATION_NONE:
            inflammation_factor = 0.0f;
            break;
        case INFLAMMATION_LOCAL:
            inflammation_factor = 0.2f;
            break;
        case INFLAMMATION_REGIONAL:
            inflammation_factor = 0.4f;
            break;
        case INFLAMMATION_SYSTEMIC:
            inflammation_factor = 0.7f;
            break;
        case INFLAMMATION_STORM:
            inflammation_factor = 1.0f;
            break;
    }

    /* Chronic inflammation amplifies effects */
    if (bridge->inflammation_state.is_chronic) {
        inflammation_factor *= 1.3f;
        inflammation_factor = nimcp_clampf(inflammation_factor, 0.0f, 1.0f);
    }

    /* Compute speech impacts */
    bridge->inflammation_state.verbal_fluency_reduction =
        inflammation_factor * INFLAMMATION_MAX_SPEECH_IMPAIRMENT;

    bridge->inflammation_state.word_finding_difficulty =
        (inflammation_factor > INFLAMMATION_WORD_RETRIEVAL_THRESHOLD)
            ? (inflammation_factor - INFLAMMATION_WORD_RETRIEVAL_THRESHOLD) * 2.0f
            : 0.0f;

    bridge->inflammation_state.phonological_error_rate =
        (inflammation_factor > INFLAMMATION_PHONEME_ERROR_THRESHOLD)
            ? (inflammation_factor - INFLAMMATION_PHONEME_ERROR_THRESHOLD) * 2.5f
            : 0.0f;

    bridge->inflammation_state.articulation_slowing =
        inflammation_factor * 0.6f;

    bridge->inflammation_state.comprehension_impairment =
        inflammation_factor * 0.5f;

    /* Phonological working memory capacity reduction (7±2 items reduced) */
    bridge->inflammation_state.working_memory_capacity =
        1.0f - (inflammation_factor * 0.4f);  /* Max 40% reduction */

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float speech_immune_compute_impairment(const speech_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Combined impairment from cytokines and inflammation */
    float cytokine_impairment = bridge->cytokine_effects.total_fluency_impairment;
    float inflammation_impairment = bridge->inflammation_state.verbal_fluency_reduction;

    /* Take maximum (not additive to avoid over-impairment) */
    float total_impairment = fmaxf(cytokine_impairment, inflammation_impairment);
    return nimcp_clampf(total_impairment, 0.0f, 1.0f);
}

float speech_immune_get_retrieval_latency_increase(
    const speech_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;
    return bridge->cytokine_effects.word_retrieval_latency_ms;
}

float speech_immune_get_phoneme_error_rate(
    const speech_immune_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    /* Combine cytokine and inflammation error rates */
    float cytokine_errors = bridge->cytokine_effects.phoneme_error_rate;
    float inflammation_errors = bridge->inflammation_state.phonological_error_rate;

    return nimcp_clampf(cytokine_errors + inflammation_errors, 0.0f, 0.5f);
}

float speech_immune_get_speech_rate_factor(
    const speech_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;

    /* Speech rate multiplier (1.0 = normal, 0.5 = half speed) */
    float reduction = bridge->cytokine_effects.speech_rate_reduction;
    return nimcp_clampf(1.0f - reduction, 0.3f, 1.0f);  /* Min 30% of normal speed */
}

/* ============================================================================
 * Speech → Immune Implementation
 * ============================================================================ */

int speech_immune_trigger_from_effort(speech_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_trigger_from_effort: NULL bridge");
    if (!bridge->enable_speech_immune_trigger) return 0;
    NIMCP_API_CHECK_NULL(bridge->immune_system, -1, "speech_immune_trigger_from_effort: NULL immune_system");
    NIMCP_API_CHECK_NULL(bridge->speech_cortex, -1, "speech_immune_trigger_from_effort: NULL speech_cortex");

    /* Prepare data outside lock */
    bool do_effort_trigger = false;
    uint8_t effort_epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    float frustration = 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if speech effort exceeds threshold */
    if (bridge->speech_trigger.speech_effort_level < SPEECH_EFFORT_IMMUNE_TRIGGER) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* High effort triggers stress response */
    bridge->speech_trigger.cortisol_triggered = true;
    bridge->speech_trigger.immune_suppression =
        bridge->speech_trigger.speech_effort_level * 0.3f;

    /* Prepare antigen data */
    memset(effort_epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);
    snprintf((char*)effort_epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "speech_effort_stress");
    frustration = bridge->speech_trigger.frustration_level;
    do_effort_trigger = true;

    bridge->speech_triggered_responses++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present antigen to immune system OUTSIDE lock */
    if (do_effort_trigger) {
        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            effort_epitope,
            strlen((char*)effort_epitope),
            3,  /* Moderate severity */
            0,  /* No specific node */
            &antigen_id
        );

        /* Release IL-6 (stress cytokine) */
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_IL6,
            0,  /* No specific source cell */
            frustration,
            0,  /* Broadcast */
            &cytokine_id
        );
    }

    LOG_MODULE_INFO("speech_immune_bridge",
                  "Speech effort triggered immune response");
    return 0;
}

int speech_immune_detect_distress_vocalization(
    speech_immune_bridge_t* bridge,
    const void* prosody_features
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_detect_distress_vocalization: NULL bridge");
    if (!bridge->enable_distress_vocalization_trigger) return 0;
    NIMCP_API_CHECK_NULL(prosody_features, -1, "speech_immune_detect_distress_vocalization: NULL prosody_features");

    /* In full implementation, would analyze prosody features for:
     * - High pitch (pain/distress marker)
     * - Irregular pitch contour (emotional distress)
     * - High intensity variability (agitation)
     * - Tremor in voice (anxiety/pain)
     */

    /* Prepare data outside lock */
    bool do_distress_trigger = false;
    uint8_t distress_epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    float distress_intensity_val = 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Simplified distress detection */
    bool distress_detected = bridge->speech_trigger.distress_intensity > DISTRESS_VOCALIZATION_THRESHOLD;

    if (distress_detected) {
        bridge->speech_trigger.distress_detected = true;
        distress_intensity_val = bridge->speech_trigger.distress_intensity;

        /* Prepare antigen data */
        snprintf((char*)distress_epitope, BRAIN_IMMUNE_EPITOPE_SIZE, "distress_vocalization");
        do_distress_trigger = true;

        bridge->distress_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Present antigen and release cytokine OUTSIDE lock */
    if (do_distress_trigger) {
        /* Distress triggers HPA axis - cortisol - immune activation */
        uint32_t antigen_id;
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            distress_epitope,
            strlen((char*)distress_epitope),
            5,  /* Higher severity */
            0,
            &antigen_id
        );

        /* Release TNF-alpha (acute stress response) */
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            bridge->immune_system,
            BRAIN_CYTOKINE_TNF,
            0,
            distress_intensity_val,
            0,
            &cytokine_id
        );

        LOG_MODULE_INFO("speech_immune_bridge",
                  "Distress vocalization triggered immune response");
    }

    return 0;
}

int speech_immune_trigger_from_illness_expression(
    speech_immune_bridge_t* bridge,
    const char* word
) {
    /* Guard clauses */
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_trigger_from_illness_expression: NULL bridge");
    if (!bridge->enable_speech_immune_trigger) return 0;
    if (!word || !is_illness_word(word)) return 0;

    /* Release IL-10 OUTSIDE lock (Pennebaker 1997 - verbalizing illness modulates immune) */
    uint32_t cytokine_id;
    brain_immune_release_cytokine(
        bridge->immune_system,
        BRAIN_CYTOKINE_IL10,
        0,
        0.5f,  /* Moderate concentration */
        0,
        &cytokine_id
    );

    LOG_MODULE_DEBUG("speech_immune_bridge",
                  "Illness word '%s' modulated immune response", word);
    return 0;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int speech_immune_bridge_update(
    speech_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_bridge_update: NULL bridge");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->total_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply immune effects to speech */
    speech_immune_apply_cytokine_effects(bridge);
    speech_immune_apply_inflammation_effects(bridge);

    /* Check speech state for immune triggers */
    /* In full implementation, would query speech cortex for:
     * - Current error rate
     * - Retrieval latencies
     * - Speech effort metrics
     */

    /* Update speech effort level (would be computed from speech cortex metrics) */
    /* For now, use placeholder logic */
    float error_rate = speech_immune_get_phoneme_error_rate(bridge);
    bridge->speech_trigger.error_rate = error_rate;
    bridge->speech_trigger.speech_effort_level = error_rate * 1.5f;
    bridge->speech_trigger.frustration_level =
        nimcp_clampf(error_rate * 2.0f, 0.0f, 1.0f);

    /* Trigger immune from high effort if threshold exceeded */
    speech_immune_trigger_from_effort(bridge);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int speech_immune_get_cytokine_effects(
    const speech_immune_bridge_t* bridge,
    cytokine_speech_effects_t* effects
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_get_cytokine_effects: NULL bridge");
    NIMCP_API_CHECK_NULL(effects, -1, "speech_immune_get_cytokine_effects: NULL effects");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_speech_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int speech_immune_get_inflammation_state(
    const speech_immune_bridge_t* bridge,
    inflammation_speech_state_t* state
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_get_inflammation_state: NULL bridge");
    NIMCP_API_CHECK_NULL(state, -1, "speech_immune_get_inflammation_state: NULL state");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_speech_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool speech_immune_is_speech_impaired(const speech_immune_bridge_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "speech_immune_is_speech_impaired: bridge is NULL");

            return false;

        }

    float impairment = speech_immune_compute_impairment(bridge);
    return (impairment > 0.3f);  /* Threshold for noticeable impairment */
}

float speech_immune_get_fluency_reduction(const speech_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->cytokine_effects.total_fluency_impairment;
}

float speech_immune_get_working_memory_capacity(
    const speech_immune_bridge_t* bridge
) {
    if (!bridge) return 1.0f;
    return bridge->inflammation_state.working_memory_capacity;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define SPEECH_IMMUNE_MODULE_NAME "speech_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int speech_immune_connect_bio_async(speech_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_connect_bio_async: NULL bridge");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SPEECH,
        .module_name = SPEECH_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("speech_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int speech_immune_disconnect_bio_async(speech_immune_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "speech_immune_disconnect_bio_async: NULL bridge");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_DEBUG("speech_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool speech_immune_is_bio_async_connected(const speech_immune_bridge_t* bridge) {
    if (!bridge) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "speech_immune_is_bio_async_connected: bridge is NULL");

            return false;

        }
    return bridge->base.bio_async_enabled;
}
