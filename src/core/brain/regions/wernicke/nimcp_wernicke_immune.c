/**
 * @file nimcp_wernicke_immune.c
 * @brief Wernicke's Region - Brain Immune System Integration Implementation
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional integration between brain immune system and Wernicke's region
 * WHY:  Enable realistic comprehension impairment from inflammation
 * HOW:  Immune modulates comprehension, errors may trigger immune response
 *
 * @version Phase W6: Wernicke's Area Immune Integration
 * @author NIMCP Development Team
 */

#include "core/brain/regions/wernicke/nimcp_wernicke_immune.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "WERNICKE_IMMUNE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_immune, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define DEFAULT_ERROR_HISTORY_CAPACITY 256
#define DEFAULT_ERROR_ANALYSIS_WINDOW_MS 60000  /* 1 minute */

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Map inflammation level to comprehension impairment
 */
static float inflammation_to_impairment(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return COMPREHENSION_IMPAIRMENT_NONE;
        case INFLAMMATION_LOCAL:    return COMPREHENSION_IMPAIRMENT_MILD;
        case INFLAMMATION_REGIONAL: return COMPREHENSION_IMPAIRMENT_MODERATE;
        case INFLAMMATION_SYSTEMIC: return COMPREHENSION_IMPAIRMENT_SEVERE;
        case INFLAMMATION_STORM:    return COMPREHENSION_IMPAIRMENT_STORM;
        default:                    return COMPREHENSION_IMPAIRMENT_NONE;
    }
}

/**
 * @brief Map impairment level to dominant aphasia symptom
 */
static wernicke_aphasia_type_t impairment_to_aphasia(float impairment) {
    if (impairment < 0.1f) return WERNICKE_APHASIA_NONE;
    if (impairment < 0.25f) return WERNICKE_APHASIA_ANOMIC;
    if (impairment < 0.4f) return WERNICKE_APHASIA_WORD_DEAFNESS;
    if (impairment < 0.6f) return WERNICKE_APHASIA_PARAPHASIC;
    if (impairment < 0.8f) return WERNICKE_APHASIA_RECEPTIVE;
    return WERNICKE_APHASIA_GLOBAL;
}

/**
 * @brief Map state to immune state enum
 */
static wernicke_immune_state_t impairment_to_state(float impairment, bool recovering) {
    if (recovering) return WERNICKE_IMMUNE_RECOVERING;
    if (impairment < 0.1f) return WERNICKE_IMMUNE_NORMAL;
    if (impairment < 0.3f) return WERNICKE_IMMUNE_MILD_IMPAIRMENT;
    if (impairment < 0.6f) return WERNICKE_IMMUNE_MODERATE_APHASIA;
    if (impairment < 0.9f) return WERNICKE_IMMUNE_SEVERE_APHASIA;
    return WERNICKE_IMMUNE_STORM;
}

/**
 * @brief Allocate error history
 */
static bool allocate_error_history(wernicke_error_history_t* history, size_t capacity) {
    history->errors = nimcp_calloc(capacity, sizeof(wernicke_comp_error_t));
    if (!history->errors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_error_history: history->errors is NULL");
        return false;
    }
    history->error_capacity = capacity;
    history->error_count = 0;
    return true;
}

/**
 * @brief Free error history
 */
static void free_error_history(wernicke_error_history_t* history) {
    if (history->errors) {
        nimcp_free(history->errors);
        history->errors = NULL;
    }
    history->error_count = 0;
    history->error_capacity = 0;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int wernicke_immune_default_config(wernicke_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(wernicke_immune_config_t));

    /* Feature enables */
    config->enable_inflammation_impairment = true;
    config->enable_cytokine_modulation = true;
    config->enable_error_immune_trigger = true;
    config->enable_chronic_inflammation_tracking = true;
    config->enable_recovery_monitoring = true;

    /* Sensitivity tuning */
    config->inflammation_sensitivity = 1.0f;
    config->cytokine_sensitivity = 1.0f;
    config->error_detection_sensitivity = 1.0f;

    /* Thresholds */
    config->word_deafness_threshold = WORD_RECOGNITION_THRESHOLD_MODERATE;
    config->semantic_aphasia_threshold = 0.5f;
    config->error_trigger_threshold = COMP_ERROR_TRIGGER_THRESHOLD;

    /* Error tracking */
    config->max_error_history = DEFAULT_ERROR_HISTORY_CAPACITY;
    config->error_analysis_window_ms = DEFAULT_ERROR_ANALYSIS_WINDOW_MS;

    /* Logging */
    config->enable_logging = true;

    return 0;
}

wernicke_immune_bridge_t* wernicke_immune_bridge_create(
    const wernicke_immune_config_t* config,
    brain_immune_system_t* immune_system,
    wernicke_adapter_t* wernicke_adapter)
{
    if (!immune_system) {
        LOG_ERROR(LOG_MODULE, "Immune system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    wernicke_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(wernicke_immune_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        wernicke_immune_default_config(&bridge->config);
    }

    /* Store module handles */
    bridge->immune_system = immune_system;
    bridge->wernicke_adapter = wernicke_adapter;

    /* Initialize state */
    bridge->state = WERNICKE_IMMUNE_NORMAL;
    bridge->impairment.overall_impairment = 0.0f;
    bridge->impairment.dominant_symptom = WERNICKE_APHASIA_NONE;

    /* Allocate error history */
    if (!allocate_error_history(&bridge->error_history, bridge->config.max_error_history)) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate error history");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_immune_bridge_create: allocate_error_history is NULL");
        return NULL;
    }

    /* Initialize timestamps */
    bridge->last_update_time_ms = nimcp_time_get_ms();
    bridge->state_entry_time_ms = bridge->last_update_time_ms;

    LOG_INFO(LOG_MODULE, "Created Wernicke-immune bridge");
    return bridge;
}

void wernicke_immune_bridge_destroy(wernicke_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Stop if running */
    if (bridge->running) {
        wernicke_immune_bridge_stop(bridge);
    }

    /* Free error history */
    free_error_history(&bridge->error_history);

    nimcp_free(bridge);
    LOG_DEBUG(LOG_MODULE, "Destroyed Wernicke-immune bridge");
}

int wernicke_immune_bridge_start(wernicke_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->running) return 0;

    bridge->running = true;
    bridge->last_update_time_ms = nimcp_time_get_ms();

    LOG_INFO(LOG_MODULE, "Started Wernicke-immune integration");
    return 0;
}

int wernicke_immune_bridge_stop(wernicke_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->running) return 0;

    bridge->running = false;

    LOG_INFO(LOG_MODULE, "Stopped Wernicke-immune integration");
    return 0;
}

/* ============================================================================
 * Immune -> Wernicke API Implementation
 * ============================================================================ */

int wernicke_immune_apply_inflammation_effects(wernicke_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_impairment) return 0;

    /* Query current inflammation level */
    brain_inflammation_level_t level = brain_immune_get_inflammation_level(bridge->immune_system);

    /* Map to impairment */
    float base_impairment = inflammation_to_impairment(level);
    float adjusted = base_impairment * bridge->config.inflammation_sensitivity;

    /* Update state */
    bridge->inflammation_state.current_level = level;
    bridge->impairment.overall_impairment = fminf(adjusted, 1.0f);
    bridge->impairment.dominant_symptom = impairment_to_aphasia(bridge->impairment.overall_impairment);

    /* Distribute impairment across subsystems */
    bridge->impairment.phoneme_recognition_impairment = adjusted * 0.9f;
    bridge->impairment.lexical_access_impairment = adjusted * 0.95f;
    bridge->impairment.semantic_integration_impairment = adjusted * 1.0f;
    bridge->impairment.syntactic_parsing_impairment = adjusted * 0.85f;

    /* Update specific symptoms */
    bridge->impairment.word_deafness_severity = adjusted * 0.8f;
    bridge->impairment.semantic_paraphasia_rate = adjusted * 0.6f;
    bridge->impairment.phonemic_paraphasia_rate = adjusted * 0.5f;
    bridge->impairment.neologism_rate = adjusted * 0.4f;

    /* Performance metrics */
    bridge->impairment.processing_speed_multiplier = 1.0f - (adjusted * 0.5f);
    bridge->impairment.error_rate = adjusted * 0.8f;
    bridge->impairment.comprehension_delay_ms = adjusted * 500.0f;

    /* Update state enum */
    bridge->state = impairment_to_state(bridge->impairment.overall_impairment,
                                         bridge->inflammation_state.in_recovery);

    if (bridge->config.enable_logging && adjusted > 0.1f) {
        LOG_DEBUG(LOG_MODULE, "Inflammation effects: level=%d, impairment=%.2f",
                  level, bridge->impairment.overall_impairment);
    }

    return 0;
}

int wernicke_immune_apply_cytokine_effects(wernicke_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_modulation) return 0;

    /* Query cytokine levels from immune system */
    float il1 = 0.0f, il6 = 0.0f, tnf = 0.0f, ifn_gamma = 0.0f, il10 = 0.0f;

    /* Note: These would come from brain_immune_get_cytokine_level() */
    /* For now, estimate from inflammation level */
    float infl = inflammation_to_impairment(bridge->inflammation_state.current_level);
    il1 = infl * 0.8f;
    il6 = infl * 0.7f;
    tnf = infl * 0.9f;
    ifn_gamma = infl * 0.5f;
    il10 = (1.0f - infl) * 0.3f;  /* Anti-inflammatory rises during recovery */

    /* Apply sensitivity */
    float sens = bridge->config.cytokine_sensitivity;

    /* Pro-inflammatory effects */
    bridge->cytokine_effects.il1_phoneme_slowdown = il1 * CYTOKINE_IL1_PHONEME_SLOWDOWN * sens;
    bridge->cytokine_effects.il6_lexical_disruption = il6 * CYTOKINE_IL6_LEXICAL_IMPAIRMENT * sens;
    bridge->cytokine_effects.tnf_semantic_disruption = tnf * CYTOKINE_TNF_SEMANTIC_DISRUPTION * sens;
    bridge->cytokine_effects.ifn_gamma_parsing_impairment = ifn_gamma * 0.4f * sens;

    /* Anti-inflammatory effects */
    bridge->cytokine_effects.il10_recovery_boost = il10 * CYTOKINE_IL10_RECOVERY_BOOST;

    /* Compute aggregate modulation */
    bridge->cytokine_effects.total_phoneme_modulation =
        bridge->cytokine_effects.il1_phoneme_slowdown;
    bridge->cytokine_effects.total_lexical_modulation =
        bridge->cytokine_effects.il6_lexical_disruption;
    bridge->cytokine_effects.total_semantic_modulation =
        bridge->cytokine_effects.tnf_semantic_disruption;
    bridge->cytokine_effects.total_syntactic_modulation =
        bridge->cytokine_effects.ifn_gamma_parsing_impairment;

    return 0;
}

int wernicke_immune_compute_impairment(
    wernicke_immune_bridge_t* bridge,
    wernicke_comprehension_impairment_t* impairment)
{
    if (!bridge || !impairment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_immune_compute_impairment: required parameter is NULL (bridge, impairment)");
        return -1;
    }

    /* Apply inflammation effects first */
    wernicke_immune_apply_inflammation_effects(bridge);

    /* Apply cytokine effects */
    wernicke_immune_apply_cytokine_effects(bridge);

    /* Combine inflammation and cytokine effects */
    impairment->overall_impairment = bridge->impairment.overall_impairment;
    impairment->dominant_symptom = bridge->impairment.dominant_symptom;

    /* Subsystem impairments (add cytokine effects) */
    impairment->phoneme_recognition_impairment =
        fminf(bridge->impairment.phoneme_recognition_impairment +
              bridge->cytokine_effects.total_phoneme_modulation, 1.0f);

    impairment->lexical_access_impairment =
        fminf(bridge->impairment.lexical_access_impairment +
              bridge->cytokine_effects.total_lexical_modulation, 1.0f);

    impairment->semantic_integration_impairment =
        fminf(bridge->impairment.semantic_integration_impairment +
              bridge->cytokine_effects.total_semantic_modulation, 1.0f);

    impairment->syntactic_parsing_impairment =
        fminf(bridge->impairment.syntactic_parsing_impairment +
              bridge->cytokine_effects.total_syntactic_modulation, 1.0f);

    /* Copy specific symptoms */
    impairment->word_deafness_severity = bridge->impairment.word_deafness_severity;
    impairment->semantic_paraphasia_rate = bridge->impairment.semantic_paraphasia_rate;
    impairment->phonemic_paraphasia_rate = bridge->impairment.phonemic_paraphasia_rate;
    impairment->neologism_rate = bridge->impairment.neologism_rate;
    impairment->comprehension_delay_ms = bridge->impairment.comprehension_delay_ms;

    /* Performance metrics */
    impairment->processing_speed_multiplier = bridge->impairment.processing_speed_multiplier;
    impairment->error_rate = bridge->impairment.error_rate;
    impairment->semantic_priming_strength = 1.0f - impairment->semantic_integration_impairment;
    impairment->phoneme_discrimination_accuracy = 1.0f - impairment->phoneme_recognition_impairment;

    return 0;
}

/* ============================================================================
 * Wernicke -> Immune API Implementation
 * ============================================================================ */

int wernicke_immune_report_comp_error(
    wernicke_immune_bridge_t* bridge,
    wernicke_comp_error_type_t error_type,
    const char* expected,
    const char* actual,
    float severity)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    wernicke_error_history_t* history = &bridge->error_history;

    /* Add error to history (circular buffer) */
    size_t idx = history->error_count % history->error_capacity;
    wernicke_comp_error_t* error = &history->errors[idx];

    error->error_id = (uint32_t)(history->error_count + 1);
    error->type = error_type;
    error->timestamp_ms = nimcp_time_get_ms();
    error->severity = fminf(fmaxf(severity, 0.0f), 1.0f);
    error->catastrophic = (severity > 0.8f);

    if (expected) {
        strncpy(error->expected_meaning, expected, sizeof(error->expected_meaning) - 1);
    }
    if (actual) {
        strncpy(error->actual_interpretation, actual, sizeof(error->actual_interpretation) - 1);
    }

    history->error_count++;

    /* Update statistics */
    bridge->stats.total_comp_errors++;
    switch (error_type) {
        case COMP_ERROR_PHONOLOGICAL: bridge->stats.phonological_errors++; break;
        case COMP_ERROR_LEXICAL:      bridge->stats.lexical_errors++; break;
        case COMP_ERROR_SEMANTIC:     bridge->stats.semantic_errors++; break;
        case COMP_ERROR_SYNTACTIC:    bridge->stats.syntactic_errors++; break;
        default: break;
    }

    /* Check if should trigger immune */
    if (bridge->config.enable_error_immune_trigger) {
        wernicke_immune_trigger_from_errors(bridge);
    }

    return 0;
}

int wernicke_immune_analyze_error_patterns(
    wernicke_immune_bridge_t* bridge,
    float* phonological_damage,
    float* lexical_damage,
    float* semantic_damage,
    float* syntactic_damage)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    wernicke_error_history_t* history = &bridge->error_history;
    uint64_t now = nimcp_time_get_ms();
    uint64_t window = bridge->config.error_analysis_window_ms;

    /* Count errors in analysis window */
    uint32_t phon_count = 0, lex_count = 0, sem_count = 0, syn_count = 0;
    uint32_t total_in_window = 0;

    size_t count = history->error_count < history->error_capacity ?
                   history->error_count : history->error_capacity;

    for (size_t i = 0; i < count; i++) {
        wernicke_comp_error_t* err = &history->errors[i];
        if ((now - err->timestamp_ms) > window) continue;

        total_in_window++;
        switch (err->type) {
            case COMP_ERROR_PHONOLOGICAL: phon_count++; break;
            case COMP_ERROR_LEXICAL:      lex_count++; break;
            case COMP_ERROR_SEMANTIC:     sem_count++; break;
            case COMP_ERROR_SYNTACTIC:    syn_count++; break;
            default: break;
        }
    }

    /* Compute damage scores (errors per minute normalized) */
    float minutes = (float)window / 60000.0f;
    float rate_scale = 1.0f / (minutes * 10.0f);  /* 10 errors/min = 1.0 damage */

    history->phonological_damage_score = fminf(phon_count * rate_scale, 1.0f);
    history->lexical_damage_score = fminf(lex_count * rate_scale, 1.0f);
    history->semantic_damage_score = fminf(sem_count * rate_scale, 1.0f);
    history->syntactic_damage_score = fminf(syn_count * rate_scale, 1.0f);

    /* Update recent error rate */
    history->recent_error_rate = total_in_window * rate_scale;

    /* Output damage scores */
    if (phonological_damage) *phonological_damage = history->phonological_damage_score;
    if (lexical_damage) *lexical_damage = history->lexical_damage_score;
    if (semantic_damage) *semantic_damage = history->semantic_damage_score;
    if (syntactic_damage) *syntactic_damage = history->syntactic_damage_score;

    return 0;
}

int wernicke_immune_trigger_from_errors(wernicke_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_immune_trigger_from_errors: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Analyze current error patterns */
    float phon_dmg, lex_dmg, sem_dmg, syn_dmg;
    wernicke_immune_analyze_error_patterns(bridge, &phon_dmg, &lex_dmg, &sem_dmg, &syn_dmg);

    /* Check if threshold exceeded */
    float max_damage = fmaxf(fmaxf(phon_dmg, lex_dmg), fmaxf(sem_dmg, syn_dmg));

    if (max_damage < bridge->config.error_trigger_threshold) {
        return 0;  /* Below threshold — no trigger needed, this is normal */
    }

    /* Trigger immune response */
    /* Note: Would call brain_immune_present_antigen() here */
    bridge->stats.immune_triggers_from_errors++;

    if (bridge->config.enable_logging) {
        LOG_INFO(LOG_MODULE, "Triggering immune response from comprehension errors: "
                 "phon=%.2f, lex=%.2f, sem=%.2f, syn=%.2f",
                 phon_dmg, lex_dmg, sem_dmg, syn_dmg);
    }

    return 0;
}

/* ============================================================================
 * Update and State Management
 * ============================================================================ */

int wernicke_immune_bridge_update(
    wernicke_immune_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->running) return 0;

    /* Update inflammation state duration */
    if (bridge->inflammation_state.current_level != INFLAMMATION_NONE) {
        bridge->inflammation_state.inflammation_duration_ms +=
            (current_time_ms - bridge->last_update_time_ms);

        /* Check for chronic inflammation */
        if (bridge->inflammation_state.inflammation_duration_ms >=
            CHRONIC_COMP_INFLAMMATION_THRESHOLD_MS) {
            bridge->inflammation_state.is_chronic = true;
        }
    }

    /* Apply effects */
    wernicke_immune_apply_inflammation_effects(bridge);
    wernicke_immune_apply_cytokine_effects(bridge);

    /* Update recovery state */
    if (bridge->inflammation_state.in_recovery && bridge->config.enable_recovery_monitoring) {
        float recovery_rate = 0.001f * (current_time_ms - bridge->inflammation_state.recovery_start_ms);
        recovery_rate += bridge->cytokine_effects.il10_recovery_boost;
        bridge->inflammation_state.recovery_progress = fminf(recovery_rate, 1.0f);

        if (bridge->inflammation_state.recovery_progress >= 1.0f) {
            bridge->inflammation_state.in_recovery = false;
            bridge->stats.recovery_episodes++;
        }
    }

    /* Track state changes */
    wernicke_immune_state_t new_state = impairment_to_state(
        bridge->impairment.overall_impairment,
        bridge->inflammation_state.in_recovery);

    if (new_state != bridge->state) {
        bridge->state_entry_time_ms = current_time_ms;
        bridge->state = new_state;

        /* Update episode stats */
        switch (new_state) {
            case WERNICKE_IMMUNE_MILD_IMPAIRMENT:
                bridge->stats.mild_episodes++;
                bridge->stats.total_impairment_episodes++;
                break;
            case WERNICKE_IMMUNE_MODERATE_APHASIA:
                bridge->stats.moderate_episodes++;
                bridge->stats.total_impairment_episodes++;
                break;
            case WERNICKE_IMMUNE_SEVERE_APHASIA:
                bridge->stats.severe_episodes++;
                bridge->stats.total_impairment_episodes++;
                break;
            case WERNICKE_IMMUNE_STORM:
                bridge->stats.storm_episodes++;
                bridge->stats.total_impairment_episodes++;
                break;
            default:
                break;
        }
    }

    /* Track max impairment */
    if (bridge->impairment.overall_impairment > bridge->stats.max_impairment_observed) {
        bridge->stats.max_impairment_observed = bridge->impairment.overall_impairment;
    }

    bridge->last_update_time_ms = current_time_ms;
    return 0;
}

wernicke_immune_state_t wernicke_immune_get_state(
    const wernicke_immune_bridge_t* bridge)
{
    return bridge ? bridge->state : WERNICKE_IMMUNE_NORMAL;
}

int wernicke_immune_get_impairment(
    const wernicke_immune_bridge_t* bridge,
    wernicke_comprehension_impairment_t* impairment)
{
    if (!bridge || !impairment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_immune_get_impairment: required parameter is NULL (bridge, impairment)");
        return -1;
    }
    *impairment = bridge->impairment;
    return 0;
}

int wernicke_immune_get_stats(
    const wernicke_immune_bridge_t* bridge,
    wernicke_immune_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_immune_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int wernicke_immune_set_aphasia_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_aphasia_onset_cb_t callback,
    void* user_data)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Store callback - implementation would add callback storage to struct */
    (void)callback;
    (void)user_data;
    return 0;
}

int wernicke_immune_set_error_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_error_cb_t callback,
    void* user_data)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)callback;
    (void)user_data;
    return 0;
}

int wernicke_immune_set_impairment_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_impairment_cb_t callback,
    void* user_data)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)callback;
    (void)user_data;
    return 0;
}

int wernicke_immune_set_recovery_callback(
    wernicke_immune_bridge_t* bridge,
    wernicke_immune_recovery_cb_t callback,
    void* user_data)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)callback;
    (void)user_data;
    return 0;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* wernicke_aphasia_type_to_string(wernicke_aphasia_type_t type) {
    switch (type) {
        case WERNICKE_APHASIA_NONE:         return "None";
        case WERNICKE_APHASIA_WORD_DEAFNESS: return "Word Deafness";
        case WERNICKE_APHASIA_SEMANTIC:     return "Semantic Aphasia";
        case WERNICKE_APHASIA_ANOMIC:       return "Anomic Aphasia";
        case WERNICKE_APHASIA_PARAPHASIC:   return "Paraphasic";
        case WERNICKE_APHASIA_NEOLOGISTIC:  return "Neologistic Jargon";
        case WERNICKE_APHASIA_RECEPTIVE:    return "Receptive Aphasia";
        case WERNICKE_APHASIA_GLOBAL:       return "Global Aphasia";
        default:                            return "Unknown";
    }
}

const char* wernicke_immune_state_to_string(wernicke_immune_state_t state) {
    switch (state) {
        case WERNICKE_IMMUNE_NORMAL:           return "Normal";
        case WERNICKE_IMMUNE_MILD_IMPAIRMENT:  return "Mild Impairment";
        case WERNICKE_IMMUNE_MODERATE_APHASIA: return "Moderate Aphasia";
        case WERNICKE_IMMUNE_SEVERE_APHASIA:   return "Severe Aphasia";
        case WERNICKE_IMMUNE_STORM:            return "Cytokine Storm";
        case WERNICKE_IMMUNE_RECOVERING:       return "Recovering";
        default:                               return "Unknown";
    }
}

const char* wernicke_comp_error_type_to_string(wernicke_comp_error_type_t type) {
    switch (type) {
        case COMP_ERROR_NONE:         return "None";
        case COMP_ERROR_PHONOLOGICAL: return "Phonological";
        case COMP_ERROR_LEXICAL:      return "Lexical";
        case COMP_ERROR_SEMANTIC:     return "Semantic";
        case COMP_ERROR_SYNTACTIC:    return "Syntactic";
        case COMP_ERROR_REPETITION:   return "Repetition";
        case COMP_ERROR_CONTEXT:      return "Context";
        case COMP_ERROR_NEOLOGISM:    return "Neologism";
        default:                      return "Unknown";
    }
}
