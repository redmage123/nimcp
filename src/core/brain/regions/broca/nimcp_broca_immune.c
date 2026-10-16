/**
 * @file nimcp_broca_immune.c
 * @brief Broca's Region - Brain Immune System Integration Implementation
 */

#include "core/brain/regions/broca/nimcp_broca_immune.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(broca_immune, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Map inflammation level to speech impairment
 *
 * WHAT: Convert inflammation severity to speech production impairment
 * WHY:  Quantify biological inflammation → aphasia mapping
 * HOW:  Use evidence-based thresholds for each level
 */
static float map_inflammation_to_impairment(brain_inflammation_level_t level)
{
    switch (level) {
        case INFLAMMATION_NONE:     return IMPAIRMENT_NONE;
        case INFLAMMATION_LOCAL:    return IMPAIRMENT_MILD;
        case INFLAMMATION_REGIONAL: return IMPAIRMENT_MODERATE;
        case INFLAMMATION_SYSTEMIC: return IMPAIRMENT_SEVERE;
        case INFLAMMATION_STORM:    return IMPAIRMENT_STORM;
        default:                    return IMPAIRMENT_NONE;
    }
}

/**
 * @brief Determine dominant aphasia symptom
 *
 * WHAT: Find which subsystem is most impaired
 * WHY:  Classify primary aphasia type
 * HOW:  Compare impairment scores
 */
static broca_aphasia_type_t determine_dominant_symptom(
    const broca_speech_impairment_t* impairment)
{
    if (!impairment) return APHASIA_NONE;

    /* Check for near-mutism first */
    if (impairment->overall_impairment >= IMPAIRMENT_STORM) {
        return APHASIA_MUTISM;
    }

    /* Non-fluent aphasia if severe across all systems */
    if (impairment->overall_impairment >= IMPAIRMENT_SEVERE) {
        return APHASIA_NONFLUENT;
    }

    /* Find most impaired subsystem */
    float max_impair = 0.0f;
    broca_aphasia_type_t dominant = APHASIA_NONE;

    if (impairment->lexical_access_impairment > max_impair) {
        max_impair = impairment->lexical_access_impairment;
        dominant = APHASIA_ANOMIA;
    }
    if (impairment->syntax_impairment > max_impair) {
        max_impair = impairment->syntax_impairment;
        dominant = APHASIA_AGRAMMATISM;
    }
    if (impairment->phonological_impairment > max_impair) {
        max_impair = impairment->phonological_impairment;
        dominant = APHASIA_PHONOLOGICAL;
    }
    if (impairment->motor_planning_impairment > max_impair) {
        max_impair = impairment->motor_planning_impairment;
        dominant = APHASIA_MOTOR_SPEECH;
    }

    return dominant;
}

/**
 * @brief Create error signature for antigen presentation
 *
 * WHAT: Generate epitope from error pattern
 * WHY:  Unique signature for immune recognition
 * HOW:  Hash error type, position, severity
 */
static void create_error_signature(
    const broca_speech_error_t* error,
    uint8_t* signature,
    size_t* signature_len)
{
    if (!error || !signature || !signature_len) return;

    size_t idx = 0;

    /* Error type */
    signature[idx++] = (uint8_t)error->type;

    /* Position */
    signature[idx++] = (uint8_t)(error->error_position & 0xFF);
    signature[idx++] = (uint8_t)((error->error_position >> 8) & 0xFF);

    /* Severity */
    signature[idx++] = (uint8_t)(error->severity * 255.0f);

    /* Timestamp hash (for pattern detection) */
    uint64_t time_hash = error->timestamp_ms % 256;
    signature[idx++] = (uint8_t)time_hash;

    /* Add string signatures */
    for (size_t i = 0; i < 32 && error->intended_utterance[i] != '\0'; i++) {
        signature[idx++] = (uint8_t)error->intended_utterance[i];
        if (idx >= 64) break;
    }

    *signature_len = idx;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int broca_immune_default_config(broca_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(broca_immune_config_t));

    config->enable_inflammation_impairment = true;
    config->enable_cytokine_modulation = true;
    config->enable_error_immune_trigger = true;
    config->enable_chronic_inflammation_tracking = true;
    config->enable_recovery_monitoring = true;

    config->inflammation_sensitivity = 1.0f;
    config->cytokine_sensitivity = 1.0f;
    config->error_detection_sensitivity = 1.0f;

    config->anomia_threshold = ANOMIA_THRESHOLD_MODERATE;
    config->agrammatism_threshold = IMPAIRMENT_MODERATE;
    config->error_trigger_threshold = ERROR_TRIGGER_THRESHOLD;

    config->max_error_history = 100;
    config->error_analysis_window_ms = 60000; /* 1 minute */

    config->enable_logging = true;

    return 0;
}

broca_immune_bridge_t* broca_immune_bridge_create(
    const broca_immune_config_t* config,
    brain_immune_system_t* immune_system,
    broca_adapter_t* broca_adapter)
{
    /* Guard clauses */
    if (!immune_system || !broca_adapter) {
        NIMCP_LOGGING_ERROR("broca_immune_bridge_create: NULL required parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_bridge_create: required parameter is NULL (immune_system, broca_adapter)");
        return NULL;
    }

    /* Allocate bridge */
    broca_immune_bridge_t* bridge = (broca_immune_bridge_t*)nimcp_malloc(
        sizeof(broca_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("broca_immune_bridge_create: Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(broca_immune_bridge_t));

    /* Set config */
    if (config) {
        bridge->config = *config;
    } else {
        broca_immune_default_config(&bridge->config);
    }

    /* Store handles */
    bridge->immune_system = immune_system;
    bridge->broca_adapter = broca_adapter;

    /* Get subsystem handles */
    bridge->syntax_processor = broca_get_syntax_processor(broca_adapter);
    bridge->phonological_processor = broca_get_phonological_processor(broca_adapter);
    bridge->speech_motor_planner = broca_get_speech_motor_planner(broca_adapter);
    bridge->language_bridge = NULL; /* Optional - set via separate connection API if available */

    /* Allocate error history */
    bridge->error_history.error_capacity = bridge->config.max_error_history;
    bridge->error_history.errors = (broca_speech_error_t*)nimcp_malloc(
        sizeof(broca_speech_error_t) * bridge->error_history.error_capacity);

    if (!bridge->error_history.errors) {
        NIMCP_LOGGING_ERROR("broca_immune_bridge_create: Failed to allocate error history");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_immune_bridge_create: bridge->error_history is NULL");
        return NULL;
    }

    bridge->state = BROCA_IMMUNE_NORMAL;
    bridge->running = false;

    NIMCP_LOGGING_INFO("Broca-immune bridge created successfully");

    return bridge;
}

void broca_immune_bridge_destroy(broca_immune_bridge_t* bridge)
{
    if (!bridge) return;

    /* Free error history */
    if (bridge->error_history.errors) {
        nimcp_free(bridge->error_history.errors);
    }

    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Broca-immune bridge destroyed");
}

int broca_immune_bridge_start(broca_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->running = true;
    bridge->last_update_time_ms = 0;
    bridge->state_entry_time_ms = 0;

    NIMCP_LOGGING_INFO("Broca-immune bridge started");

    return 0;
}

int broca_immune_bridge_stop(broca_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->running = false;

    NIMCP_LOGGING_INFO("Broca-immune bridge stopped");

    return 0;
}

/* ============================================================================
 * Immune → Broca API
 * ============================================================================ */

int broca_immune_compute_impairment(
    broca_immune_bridge_t* bridge,
    broca_speech_impairment_t* impairment)
{
    if (!bridge || !impairment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_compute_impairment: required parameter is NULL (bridge, impairment)");
        return -1;
    }

    memset(impairment, 0, sizeof(broca_speech_impairment_t));

    /* Get immune state */
    brain_immune_stats_t immune_stats;
    if (brain_immune_get_stats(bridge->immune_system, &immune_stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_immune_compute_impairment: validation failed");
        return -1;
    }

    /* Get inflammation level (use first site for simplicity) */
    brain_inflammation_level_t inflammation = INFLAMMATION_NONE;
    if (immune_stats.inflammation_sites > 0) {
        /* Query current inflammation level from immune system */
        brain_immune_phase_t phase = brain_immune_get_phase(bridge->immune_system);

        /* Map phase to inflammation estimate */
        if (phase == IMMUNE_PHASE_EFFECTOR) {
            inflammation = INFLAMMATION_REGIONAL;
        } else if (phase == IMMUNE_PHASE_ACTIVATION) {
            inflammation = INFLAMMATION_LOCAL;
        }
    }

    /* Compute base impairment */
    float base_impairment = map_inflammation_to_impairment(inflammation);
    base_impairment *= bridge->config.inflammation_sensitivity;
    base_impairment = fminf(base_impairment, 1.0f);

    impairment->overall_impairment = base_impairment;

    /* Compute subsystem impairments */
    impairment->lexical_access_impairment = base_impairment * 0.8f;
    impairment->syntax_impairment = base_impairment * 0.9f;
    impairment->phonological_impairment = base_impairment * 0.7f;
    impairment->motor_planning_impairment = base_impairment * 0.6f;

    /* Apply cytokine-specific modulation if enabled */
    if (bridge->config.enable_cytokine_modulation) {
        impairment->lexical_access_impairment +=
            bridge->cytokine_effects.il1_lexical_slowdown;
        impairment->syntax_impairment +=
            bridge->cytokine_effects.il6_syntax_disruption;
        impairment->phonological_impairment +=
            bridge->cytokine_effects.tnf_phonological_disruption;
        impairment->motor_planning_impairment +=
            bridge->cytokine_effects.ifn_gamma_motor_impairment;

        /* Apply IL-10 recovery boost */
        float recovery = bridge->cytokine_effects.il10_recovery_boost;
        impairment->lexical_access_impairment *= (1.0f - recovery);
        impairment->syntax_impairment *= (1.0f - recovery);
        impairment->phonological_impairment *= (1.0f - recovery);
        impairment->motor_planning_impairment *= (1.0f - recovery);
    }

    /* Clamp to [0, 1] */
    impairment->lexical_access_impairment = fminf(1.0f, fmaxf(0.0f,
        impairment->lexical_access_impairment));
    impairment->syntax_impairment = fminf(1.0f, fmaxf(0.0f,
        impairment->syntax_impairment));
    impairment->phonological_impairment = fminf(1.0f, fmaxf(0.0f,
        impairment->phonological_impairment));
    impairment->motor_planning_impairment = fminf(1.0f, fmaxf(0.0f,
        impairment->motor_planning_impairment));

    /* Compute specific symptoms */
    impairment->anomia_severity = impairment->lexical_access_impairment;
    impairment->agrammatism_severity = impairment->syntax_impairment;
    impairment->paraphasia_rate = impairment->phonological_impairment;
    impairment->apraxia_severity = impairment->motor_planning_impairment;
    impairment->dysfluency_rate = base_impairment * 0.5f;

    /* Compute performance metrics */
    impairment->speech_rate_multiplier = 1.0f - (base_impairment * 0.7f);
    impairment->error_rate = base_impairment * ERROR_RATE_SEVERE_INFLAMMATION;
    impairment->lexical_diversity = 1.0f - (base_impairment * 0.6f);
    impairment->mean_utterance_length = 7.0f * (1.0f - base_impairment * 0.8f);

    /* Determine dominant symptom */
    impairment->dominant_symptom = determine_dominant_symptom(impairment);

    return 0;
}

int broca_immune_apply_inflammation_effects(broca_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_apply_inflammation_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_inflammation_impairment) {
        return 0; /* Feature disabled, not an error */
    }

    /* Compute current impairment */
    broca_speech_impairment_t new_impairment;
    if (broca_immune_compute_impairment(bridge, &new_impairment) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_immune_apply_inflammation_effects: validation failed");
        return -1;
    }

    /* Store previous impairment for comparison */
    float old_impairment = bridge->impairment.overall_impairment;

    /* Update impairment state */
    bridge->impairment = new_impairment;

    /* Update state based on impairment level */
    broca_immune_state_t new_state = BROCA_IMMUNE_NORMAL;

    if (new_impairment.overall_impairment >= IMPAIRMENT_STORM) {
        new_state = BROCA_IMMUNE_STORM;
    } else if (new_impairment.overall_impairment >= IMPAIRMENT_SEVERE) {
        new_state = BROCA_IMMUNE_SEVERE_APHASIA;
    } else if (new_impairment.overall_impairment >= IMPAIRMENT_MODERATE) {
        new_state = BROCA_IMMUNE_MODERATE_APHASIA;
    } else if (new_impairment.overall_impairment >= IMPAIRMENT_MILD) {
        new_state = BROCA_IMMUNE_MILD_IMPAIRMENT;
    }

    /* Check for recovery */
    if (new_impairment.overall_impairment < old_impairment &&
        old_impairment >= IMPAIRMENT_MODERATE) {
        new_state = BROCA_IMMUNE_RECOVERING;
    }

    /* Update state and statistics */
    if (new_state != bridge->state) {
        bridge->state = new_state;
        bridge->stats.total_impairment_episodes++;

        switch (new_state) {
            case BROCA_IMMUNE_MILD_IMPAIRMENT:
                bridge->stats.mild_episodes++;
                break;
            case BROCA_IMMUNE_MODERATE_APHASIA:
                bridge->stats.moderate_episodes++;
                break;
            case BROCA_IMMUNE_SEVERE_APHASIA:
                bridge->stats.severe_episodes++;
                break;
            case BROCA_IMMUNE_STORM:
                bridge->stats.storm_episodes++;
                break;
            default:
                break;
        }
    }

    return 0;
}

int broca_immune_apply_cytokine_effects(broca_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_apply_cytokine_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_cytokine_modulation) {
        return 0; /* Feature disabled, not an error */
    }

    /* Reset cytokine effects */
    memset(&bridge->cytokine_effects, 0, sizeof(broca_cytokine_effects_t));

    /* Get immune stats to estimate cytokine levels */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_immune_apply_cytokine_effects: validation failed");
        return -1;
    }

    /* Estimate cytokine concentrations from immune activity */
    float immune_activity = (float)stats.cytokines_released /
        fmaxf(1.0f, (float)stats.antigens_processed);
    immune_activity = fminf(1.0f, immune_activity);

    /* Compute cytokine effects (simplified - in real system would query actual levels) */
    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.il1_lexical_slowdown =
        CYTOKINE_IL1_LEXICAL_SLOWDOWN * immune_activity * sensitivity;
    bridge->cytokine_effects.il6_syntax_disruption =
        CYTOKINE_IL6_SYNTAX_IMPAIRMENT * immune_activity * sensitivity;
    bridge->cytokine_effects.tnf_phonological_disruption =
        CYTOKINE_TNF_PHONOLOGICAL_DISRUPTION * immune_activity * sensitivity;
    bridge->cytokine_effects.ifn_gamma_motor_impairment =
        immune_activity * 0.25f * sensitivity;

    /* IL-10 provides recovery boost (inverse of inflammation) */
    if (stats.system_health > 0.7f) {
        bridge->cytokine_effects.il10_recovery_boost =
            CYTOKINE_IL10_RECOVERY_BOOST * stats.system_health * sensitivity;
    }

    /* Compute aggregate modulation */
    bridge->cytokine_effects.total_lexical_modulation =
        1.0f - bridge->cytokine_effects.il1_lexical_slowdown;
    bridge->cytokine_effects.total_syntax_modulation =
        1.0f - bridge->cytokine_effects.il6_syntax_disruption;
    bridge->cytokine_effects.total_phonological_modulation =
        1.0f - bridge->cytokine_effects.tnf_phonological_disruption;
    bridge->cytokine_effects.total_motor_modulation =
        1.0f - bridge->cytokine_effects.ifn_gamma_motor_impairment;

    return 0;
}

/* ============================================================================
 * Broca → Immune API
 * ============================================================================ */

int broca_immune_report_speech_error(
    broca_immune_bridge_t* bridge,
    broca_speech_error_type_t error_type,
    const char* intended,
    const char* actual,
    float severity)
{
    if (!bridge || !intended || !actual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_report_speech_error: required parameter is NULL (bridge, intended, actual)");
        return -1;
    }

    /* Check if we have capacity */
    if (bridge->error_history.error_count >= bridge->error_history.error_capacity) {
        /* Shift array (remove oldest) */
        memmove(&bridge->error_history.errors[0],
                &bridge->error_history.errors[1],
                sizeof(broca_speech_error_t) *
                (bridge->error_history.error_capacity - 1));
        bridge->error_history.error_count--;
    }

    /* Create error record */
    broca_speech_error_t* error =
        &bridge->error_history.errors[bridge->error_history.error_count++];

    memset(error, 0, sizeof(broca_speech_error_t));
    error->error_id = (uint32_t)bridge->stats.total_speech_errors;
    error->type = error_type;
    error->timestamp_ms = bridge->last_update_time_ms;
    error->severity = severity;
    error->catastrophic = (severity >= 0.9f);

    /* Copy strings (safely) */
    strncpy(error->intended_utterance, intended, 255);
    error->intended_utterance[255] = '\0';
    strncpy(error->actual_output, actual, 255);
    error->actual_output[255] = '\0';

    /* Create error signature */
    create_error_signature(error, error->error_signature, &error->signature_len);

    /* Update statistics */
    bridge->stats.total_speech_errors++;
    switch (error_type) {
        case SPEECH_ERROR_PHONOLOGICAL:
            bridge->stats.phonological_errors++;
            break;
        case SPEECH_ERROR_SYNTACTIC:
            bridge->stats.syntactic_errors++;
            break;
        case SPEECH_ERROR_MOTOR:
            bridge->stats.motor_errors++;
            break;
        default:
            break;
    }

    /* Check if should trigger immune response */
    if (bridge->config.enable_error_immune_trigger) {
        broca_immune_trigger_from_errors(bridge);
    }

    return 0;
}

int broca_immune_analyze_error_patterns(
    broca_immune_bridge_t* bridge,
    float* phonological_damage,
    float* syntactic_damage,
    float* motor_damage)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Initialize outputs */
    if (phonological_damage) *phonological_damage = 0.0f;
    if (syntactic_damage) *syntactic_damage = 0.0f;
    if (motor_damage) *motor_damage = 0.0f;

    if (bridge->error_history.error_count == 0) return 0;

    /* Count errors by type in recent window */
    uint64_t window_start = bridge->last_update_time_ms -
        bridge->config.error_analysis_window_ms;

    uint32_t phonological_count = 0;
    uint32_t syntactic_count = 0;
    uint32_t motor_count = 0;
    uint32_t total_recent = 0;

    for (size_t i = 0; i < bridge->error_history.error_count; i++) {
        if (bridge->error_history.errors[i].timestamp_ms >= window_start) {
            total_recent++;
            switch (bridge->error_history.errors[i].type) {
                case SPEECH_ERROR_PHONOLOGICAL:
                    phonological_count++;
                    break;
                case SPEECH_ERROR_SYNTACTIC:
                    syntactic_count++;
                    break;
                case SPEECH_ERROR_MOTOR:
                    motor_count++;
                    break;
                default:
                    break;
            }
        }
    }

    /* Compute damage scores */
    if (total_recent > 0) {
        if (phonological_damage) {
            *phonological_damage = (float)phonological_count / (float)total_recent;
        }
        if (syntactic_damage) {
            *syntactic_damage = (float)syntactic_count / (float)total_recent;
        }
        if (motor_damage) {
            *motor_damage = (float)motor_count / (float)total_recent;
        }
    }

    /* Store in error history */
    bridge->error_history.phonological_damage_score =
        phonological_damage ? *phonological_damage : 0.0f;
    bridge->error_history.syntactic_damage_score =
        syntactic_damage ? *syntactic_damage : 0.0f;
    bridge->error_history.motor_damage_score =
        motor_damage ? *motor_damage : 0.0f;

    return 0;
}

int broca_immune_trigger_from_errors(broca_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_trigger_from_errors: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_error_immune_trigger) {
        return 0; /* Feature disabled, not an error */
    }

    /* Analyze error patterns */
    float phono_damage, syntax_damage, motor_damage;
    broca_immune_analyze_error_patterns(bridge, &phono_damage,
        &syntax_damage, &motor_damage);

    /* Compute overall damage score */
    float max_damage = fmaxf(phono_damage, fmaxf(syntax_damage, motor_damage));

    /* Check if exceeds threshold */
    if (max_damage < bridge->config.error_trigger_threshold) {
        return 0; /* Below threshold, no trigger needed */
    }

    /* Create epitope from error pattern */
    uint8_t epitope[64];
    size_t epitope_len = 0;

    /* Use most recent error's signature */
    if (bridge->error_history.error_count > 0) {
        broca_speech_error_t* recent_error =
            &bridge->error_history.errors[bridge->error_history.error_count - 1];

        memcpy(epitope, recent_error->error_signature,
            recent_error->signature_len);
        epitope_len = recent_error->signature_len;
    }

    /* Present antigen to immune system */
    uint32_t severity = (uint32_t)(max_damage * 10.0f * ERROR_SEVERITY_MULTIPLIER);
    severity = (severity > 10) ? 10 : severity;

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        epitope_len,
        severity,
        0, /* No specific node ID for Broca errors */
        &antigen_id
    );

    if (result == 0) {
        bridge->stats.immune_triggers_from_errors++;
        NIMCP_LOGGING_INFO("Broca errors triggered immune response (antigen_id=%u)",
            antigen_id);
    }

    return result;
}

/* ============================================================================
 * Update and State Management
 * ============================================================================ */

int broca_immune_bridge_update(
    broca_immune_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->running) {
        return 0; /* Bridge not running, not an error */
    }

    bridge->last_update_time_ms = current_time_ms;

    /* Apply cytokine effects */
    if (bridge->config.enable_cytokine_modulation) {
        broca_immune_apply_cytokine_effects(bridge);
    }

    /* Apply inflammation effects */
    if (bridge->config.enable_inflammation_impairment) {
        broca_immune_apply_inflammation_effects(bridge);
    }

    /* Analyze error patterns periodically */
    if (bridge->config.enable_error_immune_trigger &&
        bridge->error_history.error_count > 0) {
        broca_immune_analyze_error_patterns(bridge, NULL, NULL, NULL);
    }

    return 0;
}

broca_immune_state_t broca_immune_get_state(const broca_immune_bridge_t* bridge)
{
    return bridge ? bridge->state : BROCA_IMMUNE_NORMAL;
}

int broca_immune_get_impairment(
    const broca_immune_bridge_t* bridge,
    broca_speech_impairment_t* impairment)
{
    if (!bridge || !impairment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_get_impairment: required parameter is NULL (bridge, impairment)");
        return -1;
    }

    *impairment = bridge->impairment;
    return 0;
}

int broca_immune_get_stats(
    const broca_immune_bridge_t* bridge,
    broca_immune_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_immune_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Callback Registration (Stubs - can be implemented if needed)
 * ============================================================================ */

int broca_immune_set_aphasia_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_aphasia_onset_cb_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->aphasia_cb = (void (*)(void))callback;
    bridge->aphasia_cb_data = user_data;
    return 0;
}

int broca_immune_set_error_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_error_cb_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->error_cb = (void (*)(void))callback;
    bridge->error_cb_data = user_data;
    return 0;
}

int broca_immune_set_impairment_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_impairment_cb_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->impairment_cb = (void (*)(void))callback;
    bridge->impairment_cb_data = user_data;
    return 0;
}

int broca_immune_set_recovery_callback(
    broca_immune_bridge_t* bridge,
    broca_immune_recovery_cb_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    bridge->recovery_cb = (void (*)(void))callback;
    bridge->recovery_cb_data = user_data;
    return 0;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* broca_aphasia_type_to_string(broca_aphasia_type_t type)
{
    switch (type) {
        case APHASIA_NONE:          return "None";
        case APHASIA_ANOMIA:        return "Anomia";
        case APHASIA_AGRAMMATISM:   return "Agrammatism";
        case APHASIA_PHONOLOGICAL:  return "Phonological";
        case APHASIA_MOTOR_SPEECH:  return "Motor Speech";
        case APHASIA_NONFLUENT:     return "Non-fluent";
        case APHASIA_MUTISM:        return "Mutism";
        default:                    return "Unknown";
    }
}

const char* broca_immune_state_to_string(broca_immune_state_t state)
{
    switch (state) {
        case BROCA_IMMUNE_NORMAL:           return "Normal";
        case BROCA_IMMUNE_MILD_IMPAIRMENT:  return "Mild Impairment";
        case BROCA_IMMUNE_MODERATE_APHASIA: return "Moderate Aphasia";
        case BROCA_IMMUNE_SEVERE_APHASIA:   return "Severe Aphasia";
        case BROCA_IMMUNE_STORM:            return "Cytokine Storm";
        case BROCA_IMMUNE_RECOVERING:       return "Recovering";
        default:                            return "Unknown";
    }
}

const char* broca_speech_error_type_to_string(broca_speech_error_type_t type)
{
    switch (type) {
        case SPEECH_ERROR_NONE:          return "None";
        case SPEECH_ERROR_PHONOLOGICAL:  return "Phonological";
        case SPEECH_ERROR_LEXICAL:       return "Lexical";
        case SPEECH_ERROR_SYNTACTIC:     return "Syntactic";
        case SPEECH_ERROR_MOTOR:         return "Motor";
        case SPEECH_ERROR_HESITATION:    return "Hesitation";
        case SPEECH_ERROR_PERSEVERATION: return "Perseveration";
        default:                         return "Unknown";
    }
}
