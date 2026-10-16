#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_immune_bridge.c - Language-Immune Bridge Implementation
//=============================================================================
/**
 * @file nimcp_language_immune_bridge.c
 * @brief Implementation of consolidated Language-Immune bridge
 *
 * WHAT: Unified bridge for all language-immune interactions
 * WHY:  Model neuroinflammation effects on language (aphasia, etc.)
 * HOW:  Cytokine signals modulate Wernicke, Broca, and NLP processing
 *
 * BIOLOGICAL BASIS:
 * - Neuroinflammation impairs language processing
 * - Wernicke's aphasia: comprehension deficits from temporal lobe inflammation
 * - Broca's aphasia: production deficits from frontal lobe inflammation
 * - Cytokines (IL-1β, IL-6, TNF-α) impair neural function
 * - IL-10 promotes recovery and anti-inflammatory effects
 *
 * @version 1.0.0 - Phase L3: Immune Consolidation
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/bridges/nimcp_language_immune_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "LANG_IMMUNE_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_immune_bridge)

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* inflammation_level_to_string(inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return "none";
        case INFLAMMATION_MILD:     return "mild";
        case INFLAMMATION_MODERATE: return "moderate";
        case INFLAMMATION_SEVERE:   return "severe";
        case INFLAMMATION_CRITICAL: return "critical";
        default:                    return "unknown";
    }
}

const char* aphasia_type_to_string(aphasia_type_t type) {
    switch (type) {
        case APHASIA_NONE:       return "none";
        case APHASIA_WERNICKE:   return "wernicke";
        case APHASIA_BROCA:      return "broca";
        case APHASIA_CONDUCTION: return "conduction";
        case APHASIA_GLOBAL:     return "global";
        case APHASIA_ANOMIC:     return "anomic";
        default:                 return "unknown";
    }
}

const char* language_region_to_string(language_region_t region) {
    switch (region) {
        case REGION_WERNICKE:          return "wernicke";
        case REGION_BROCA:             return "broca";
        case REGION_ANGULAR_GYRUS:     return "angular_gyrus";
        case REGION_ARCUATE_FASCICULUS: return "arcuate_fasciculus";
        case REGION_STG:               return "stg";
        case REGION_MTG:               return "mtg";
        case REGION_IFG:               return "ifg";
        default:                       return "unknown";
    }
}

const char* recovery_phase_to_string(recovery_phase_t phase) {
    switch (phase) {
        case RECOVERY_NONE:     return "none";
        case RECOVERY_ACUTE:    return "acute";
        case RECOVERY_SUBACUTE: return "subacute";
        case RECOVERY_CHRONIC:  return "chronic";
        case RECOVERY_COMPLETE: return "complete";
        default:                return "unknown";
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static inflammation_level_t classify_inflammation(float level) {
    if (level < LANGUAGE_IMMUNE_MILD_THRESHOLD) {
        return INFLAMMATION_NONE;
    } else if (level < LANGUAGE_IMMUNE_MODERATE_THRESHOLD) {
        return INFLAMMATION_MILD;
    } else if (level < LANGUAGE_IMMUNE_SEVERE_THRESHOLD) {
        return INFLAMMATION_MODERATE;
    } else if (level < 0.9f) {
        return INFLAMMATION_SEVERE;
    } else {
        return INFLAMMATION_CRITICAL;
    }
}

static void init_region_inflammation(region_inflammation_t* region,
                                      language_region_t region_type) {
    region->region = region_type;
    region->inflammation_level = 0.0f;
    region->severity = INFLAMMATION_NONE;
    region->impairment_factor = 0.0f;
    region->recovery_progress = 0.0f;
    region->recovery_phase = RECOVERY_NONE;
    region->inflammation_onset_ms = 0;
    region->peak_inflammation_ms = 0;
}

static void init_wernicke_effects(wernicke_immune_effects_t* effects) {
    init_region_inflammation(&effects->inflammation, REGION_WERNICKE);
    effects->phonological_impairment = 0.0f;
    effects->lexical_impairment = 0.0f;
    effects->semantic_impairment = 0.0f;
    effects->syntactic_impairment = 0.0f;
    effects->fluent_speech = true;
    effects->paraphasia_phonemic = false;
    effects->paraphasia_semantic = false;
    effects->neologisms = false;
    effects->jargon = false;
    effects->comprehension_capacity = 1.0f;
    effects->aphasia_type = APHASIA_NONE;
}

static void init_broca_effects(broca_immune_effects_t* effects) {
    init_region_inflammation(&effects->inflammation, REGION_BROCA);
    effects->articulatory_impairment = 0.0f;
    effects->syntactic_impairment = 0.0f;
    effects->fluency_impairment = 0.0f;
    effects->word_finding_impairment = 0.0f;
    effects->telegraphic_speech = false;
    effects->agrammatism = false;
    effects->apraxia_of_speech = false;
    effects->effortful_production = false;
    effects->production_capacity = 1.0f;
    effects->aphasia_type = APHASIA_NONE;
}

static void init_nlp_effects(nlp_immune_effects_t* effects) {
    effects->network_inflammation = 0.0f;
    effects->attention_impairment = 0.0f;
    effects->embedding_quality = 1.0f;
    effects->processing_speed = 1.0f;
    effects->processing_degraded = false;
}

static void compute_wernicke_effects(language_immune_bridge_t* bridge) {
    wernicke_immune_effects_t* effects = &bridge->wernicke_effects;
    float inflammation = effects->inflammation.inflammation_level;

    /* Compute impairments from inflammation level */
    effects->phonological_impairment = inflammation * LANGUAGE_IMMUNE_COMPREHENSION_SCALE;
    effects->lexical_impairment = inflammation * LANGUAGE_IMMUNE_COMPREHENSION_SCALE * 0.9f;
    effects->semantic_impairment = inflammation * LANGUAGE_IMMUNE_COMPREHENSION_SCALE * 1.1f;
    effects->syntactic_impairment = inflammation * LANGUAGE_IMMUNE_COMPREHENSION_SCALE * 0.8f;

    /* Compute remaining capacity */
    float avg_impairment = (effects->phonological_impairment +
                            effects->lexical_impairment +
                            effects->semantic_impairment +
                            effects->syntactic_impairment) / 4.0f;
    effects->comprehension_capacity = 1.0f - avg_impairment;
    if (effects->comprehension_capacity < 0.0f) effects->comprehension_capacity = 0.0f;

    /* Determine symptoms based on severity */
    inflammation_level_t severity = effects->inflammation.severity;

    effects->fluent_speech = (severity < INFLAMMATION_SEVERE);
    effects->paraphasia_phonemic = (severity >= INFLAMMATION_MILD);
    effects->paraphasia_semantic = (severity >= INFLAMMATION_MODERATE);
    effects->neologisms = (severity >= INFLAMMATION_SEVERE);
    effects->jargon = (severity >= INFLAMMATION_CRITICAL);

    /* Classify aphasia type */
    if (severity == INFLAMMATION_NONE) {
        effects->aphasia_type = APHASIA_NONE;
    } else if (severity <= INFLAMMATION_MODERATE) {
        effects->aphasia_type = APHASIA_ANOMIC;  /* Word-finding only */
    } else {
        effects->aphasia_type = APHASIA_WERNICKE;  /* Full comprehension aphasia */
    }
}

static void compute_broca_effects(language_immune_bridge_t* bridge) {
    broca_immune_effects_t* effects = &bridge->broca_effects;
    float inflammation = effects->inflammation.inflammation_level;

    /* Compute impairments from inflammation level */
    effects->articulatory_impairment = inflammation * LANGUAGE_IMMUNE_PRODUCTION_SCALE;
    effects->syntactic_impairment = inflammation * LANGUAGE_IMMUNE_PRODUCTION_SCALE * 1.1f;
    effects->fluency_impairment = inflammation * LANGUAGE_IMMUNE_FLUENCY_SCALE;
    effects->word_finding_impairment = inflammation * LANGUAGE_IMMUNE_PRODUCTION_SCALE * 0.9f;

    /* Compute remaining capacity */
    float avg_impairment = (effects->articulatory_impairment +
                            effects->syntactic_impairment +
                            effects->fluency_impairment +
                            effects->word_finding_impairment) / 4.0f;
    effects->production_capacity = 1.0f - avg_impairment;
    if (effects->production_capacity < 0.0f) effects->production_capacity = 0.0f;

    /* Determine symptoms based on severity */
    inflammation_level_t severity = effects->inflammation.severity;

    effects->telegraphic_speech = (severity >= INFLAMMATION_MODERATE);
    effects->agrammatism = (severity >= INFLAMMATION_SEVERE);
    effects->apraxia_of_speech = (severity >= INFLAMMATION_SEVERE);
    effects->effortful_production = (severity >= INFLAMMATION_MILD);

    /* Classify aphasia type */
    if (severity == INFLAMMATION_NONE) {
        effects->aphasia_type = APHASIA_NONE;
    } else if (severity <= INFLAMMATION_MILD) {
        effects->aphasia_type = APHASIA_ANOMIC;  /* Word-finding only */
    } else {
        effects->aphasia_type = APHASIA_BROCA;  /* Full production aphasia */
    }
}

static void compute_nlp_effects(language_immune_bridge_t* bridge) {
    nlp_immune_effects_t* effects = &bridge->nlp_effects;
    float inflammation = effects->network_inflammation;

    effects->attention_impairment = inflammation * 0.8f;
    effects->embedding_quality = 1.0f - inflammation * 0.5f;
    effects->processing_speed = 1.0f - inflammation * 0.6f;

    if (effects->embedding_quality < 0.3f) effects->embedding_quality = 0.3f;
    if (effects->processing_speed < 0.2f) effects->processing_speed = 0.2f;

    effects->processing_degraded = (inflammation > LANGUAGE_IMMUNE_MILD_THRESHOLD);
}

static void update_impairment_summary(language_immune_bridge_t* bridge) {
    language_impairment_summary_t* summary = &bridge->summary;

    summary->comprehension_score = bridge->wernicke_effects.comprehension_capacity;
    summary->production_score = bridge->broca_effects.production_capacity;

    /* Repetition requires both comprehension and production */
    summary->repetition_score = (summary->comprehension_score + summary->production_score) / 2.0f;

    /* Naming primarily requires word retrieval (Broca) */
    summary->naming_score = bridge->broca_effects.production_capacity *
                            (1.0f - bridge->broca_effects.word_finding_impairment);

    /* Overall language ability */
    summary->language_ability = (summary->comprehension_score +
                                  summary->production_score +
                                  summary->repetition_score +
                                  summary->naming_score) / 4.0f;

    /* Determine dominant aphasia */
    if (bridge->wernicke_effects.aphasia_type == APHASIA_WERNICKE &&
        bridge->broca_effects.aphasia_type == APHASIA_BROCA) {
        summary->dominant_aphasia = APHASIA_GLOBAL;
    } else if (bridge->wernicke_effects.aphasia_type == APHASIA_WERNICKE) {
        summary->dominant_aphasia = APHASIA_WERNICKE;
    } else if (bridge->broca_effects.aphasia_type == APHASIA_BROCA) {
        summary->dominant_aphasia = APHASIA_BROCA;
    } else if (bridge->wernicke_effects.aphasia_type == APHASIA_ANOMIC ||
               bridge->broca_effects.aphasia_type == APHASIA_ANOMIC) {
        summary->dominant_aphasia = APHASIA_ANOMIC;
    } else {
        summary->dominant_aphasia = APHASIA_NONE;
    }

    /* Check recovery trend */
    summary->improving = (bridge->wernicke_effects.inflammation.recovery_progress > 0.0f ||
                           bridge->broca_effects.inflammation.recovery_progress > 0.0f);
    summary->recovery_rate = bridge->config.recovery_rate;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

language_immune_bridge_t* language_immune_bridge_create(
    const language_immune_config_t* config)
{
    language_immune_bridge_t* bridge = (language_immune_bridge_t*)
        nimcp_calloc(1, sizeof(language_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_immune_bridge_create: allocation failed");
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge");
        return NULL;
    }

    /* Copy configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(language_immune_config_t));
    } else {
        language_immune_default_config(&bridge->config);
    }

    /* Initialize cytokine state */
    bridge->cytokines.il1_beta = 0.0f;
    bridge->cytokines.il6 = 0.0f;
    bridge->cytokines.tnf_alpha = 0.0f;
    bridge->cytokines.il10 = 0.5f;  /* Baseline anti-inflammatory */
    bridge->cytokines.composite_inflammatory = 0.0f;
    bridge->cytokines.last_update_ms = 0;

    /* Initialize region-specific effects */
    init_wernicke_effects(&bridge->wernicke_effects);
    init_broca_effects(&bridge->broca_effects);
    init_nlp_effects(&bridge->nlp_effects);

    /* Initialize summary */
    memset(&bridge->summary, 0, sizeof(language_impairment_summary_t));
    bridge->summary.comprehension_score = 1.0f;
    bridge->summary.production_score = 1.0f;
    bridge->summary.repetition_score = 1.0f;
    bridge->summary.naming_score = 1.0f;
    bridge->summary.language_ability = 1.0f;
    bridge->summary.dominant_aphasia = APHASIA_NONE;

    /* Initialize inflammation history */
    bridge->history_size = 100;
    bridge->history_idx = 0;
    bridge->immune_memory_enabled = true;
    bridge->inflammation_history = (float*)nimcp_calloc(bridge->history_size, sizeof(float));
    if (!bridge->inflammation_history) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "language_immune_bridge_create: bridge->inflammation_history is NULL");
        return NULL;
    }

    bridge->initialized = true;
    bridge->active = false;

    LOG_INFO(LOG_MODULE, "Immune bridge created");
    return bridge;
}

void language_immune_bridge_destroy(language_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_immune");

    if (bridge->bio_async_registered) {
        language_immune_bridge_bio_async_unregister(bridge);
    }

    nimcp_free(bridge->inflammation_history);
    nimcp_free(bridge);
    LOG_INFO(LOG_MODULE, "Immune bridge destroyed");
}

int language_immune_bridge_init(language_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_init: bridge is NULL");
        return -1;
    }

    memset(&bridge->stats, 0, sizeof(language_immune_stats_t));
    bridge->initialized = true;

    LOG_DEBUG(LOG_MODULE, "Immune bridge initialized");
    return 0;
}

int language_immune_bridge_start(language_immune_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_start: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    bridge->active = true;
    LOG_INFO(LOG_MODULE, "Immune bridge started");
    return 0;
}

int language_immune_bridge_stop(language_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_stop: bridge is NULL");
        return -1;
    }

    bridge->active = false;
    LOG_INFO(LOG_MODULE, "Immune bridge stopped");
    return 0;
}

//=============================================================================
// Connection API Implementation
//=============================================================================

int language_immune_bridge_connect_orchestrator(
    language_immune_bridge_t* bridge,
    language_orchestrator_t* orchestrator)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_connect_orchestrator: bridge is NULL");
        return -1;
    }
    bridge->orchestrator = orchestrator;
    return 0;
}

int language_immune_bridge_connect_brain_immune(
    language_immune_bridge_t* bridge,
    brain_immune_system_t* brain_immune)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_connect_brain_immune: bridge is NULL");
        return -1;
    }
    bridge->brain_immune = brain_immune;
    return 0;
}

int language_immune_bridge_connect_wernicke(
    language_immune_bridge_t* bridge,
    wernicke_adapter_t* wernicke)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_connect_wernicke: bridge is NULL");
        return -1;
    }
    bridge->wernicke = wernicke;
    return 0;
}

int language_immune_bridge_connect_broca(
    language_immune_bridge_t* bridge,
    broca_adapter_t* broca)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_connect_broca: bridge is NULL");
        return -1;
    }
    bridge->broca = broca;
    return 0;
}

int language_immune_bridge_connect_nlp(
    language_immune_bridge_t* bridge,
    nlp_network_t nlp_network)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_connect_nlp: bridge is NULL");
        return -1;
    }
    bridge->nlp_network = nlp_network;
    return 0;
}

//=============================================================================
// Cytokine API Implementation
//=============================================================================

int language_immune_bridge_update_cytokines(language_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_update_cytokines: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_update_cytokines: bridge->active is NULL");
        return -1;
    }

    /* In production, would fetch from brain_immune system */
    /* For now, compute composite from current levels */

    language_cytokine_state_t* cyt = &bridge->cytokines;

    /* Compute composite inflammatory score */
    float pro = (cyt->il1_beta * bridge->config.il1b_sensitivity +
                 cyt->il6 * bridge->config.il6_sensitivity +
                 cyt->tnf_alpha * bridge->config.tnfa_sensitivity) / 3.0f;
    float anti = cyt->il10 * bridge->config.il10_sensitivity;

    cyt->composite_inflammatory = pro - anti * 0.5f;
    if (cyt->composite_inflammatory < 0.0f) cyt->composite_inflammatory = 0.0f;
    if (cyt->composite_inflammatory > 1.0f) cyt->composite_inflammatory = 1.0f;

    /* Apply inflammation to regions */
    bridge->wernicke_effects.inflammation.inflammation_level = cyt->composite_inflammatory;
    bridge->wernicke_effects.inflammation.severity = classify_inflammation(cyt->composite_inflammatory);

    bridge->broca_effects.inflammation.inflammation_level = cyt->composite_inflammatory;
    bridge->broca_effects.inflammation.severity = classify_inflammation(cyt->composite_inflammatory);

    bridge->nlp_effects.network_inflammation = cyt->composite_inflammatory * 0.8f;

    /* Record history */
    if (bridge->immune_memory_enabled && bridge->inflammation_history) {
        bridge->inflammation_history[bridge->history_idx] = cyt->composite_inflammatory;
        bridge->history_idx = (bridge->history_idx + 1) % bridge->history_size;
    }

    bridge->stats.inflammation_events++;
    return 0;
}

int language_immune_bridge_get_cytokines(
    const language_immune_bridge_t* bridge,
    language_cytokine_state_t* cytokines)
{
    if (!bridge || !cytokines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_get_cytokines: required parameter is NULL (bridge, cytokines)");
        return -1;
    }
    memcpy(cytokines, &bridge->cytokines, sizeof(language_cytokine_state_t));
    return 0;
}

int language_immune_bridge_set_sensitivities(
    language_immune_bridge_t* bridge,
    float il1b_sens,
    float il6_sens,
    float tnfa_sens,
    float il10_sens)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_set_sensitivities: bridge is NULL");
        return -1;
    }

    bridge->config.il1b_sensitivity = il1b_sens;
    bridge->config.il6_sensitivity = il6_sens;
    bridge->config.tnfa_sensitivity = tnfa_sens;
    bridge->config.il10_sensitivity = il10_sens;

    return 0;
}

//=============================================================================
// Inflammation API Implementation
//=============================================================================

float language_immune_bridge_get_inflammation(
    const language_immune_bridge_t* bridge,
    language_region_t region)
{
    if (!bridge) return 0.0f;

    switch (region) {
        case REGION_WERNICKE:
        case REGION_STG:
        case REGION_MTG:
            return bridge->wernicke_effects.inflammation.inflammation_level;
        case REGION_BROCA:
        case REGION_IFG:
            return bridge->broca_effects.inflammation.inflammation_level;
        default:
            return bridge->cytokines.composite_inflammatory;
    }
}

inflammation_level_t language_immune_bridge_get_severity(
    const language_immune_bridge_t* bridge,
    language_region_t region)
{
    if (!bridge) return INFLAMMATION_NONE;

    switch (region) {
        case REGION_WERNICKE:
        case REGION_STG:
        case REGION_MTG:
            return bridge->wernicke_effects.inflammation.severity;
        case REGION_BROCA:
        case REGION_IFG:
            return bridge->broca_effects.inflammation.severity;
        default:
            return classify_inflammation(bridge->cytokines.composite_inflammatory);
    }
}

bool language_immune_bridge_is_inflamed(
    const language_immune_bridge_t* bridge,
    language_region_t region)
{
    if (!bridge) {
        return false;
    }
    return language_immune_bridge_get_inflammation(bridge, region) >= LANGUAGE_IMMUNE_MILD_THRESHOLD;
}

//=============================================================================
// Impairment API Implementation
//=============================================================================

int language_immune_bridge_get_wernicke_effects(
    const language_immune_bridge_t* bridge,
    wernicke_immune_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_get_wernicke_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    memcpy(effects, &bridge->wernicke_effects, sizeof(wernicke_immune_effects_t));
    return 0;
}

int language_immune_bridge_get_broca_effects(
    const language_immune_bridge_t* bridge,
    broca_immune_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_get_broca_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    memcpy(effects, &bridge->broca_effects, sizeof(broca_immune_effects_t));
    return 0;
}

int language_immune_bridge_get_impairment_summary(
    const language_immune_bridge_t* bridge,
    language_impairment_summary_t* summary)
{
    if (!bridge || !summary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_get_impairment_summary: required parameter is NULL (bridge, summary)");
        return -1;
    }
    memcpy(summary, &bridge->summary, sizeof(language_impairment_summary_t));
    return 0;
}

aphasia_type_t language_immune_bridge_get_aphasia_type(
    const language_immune_bridge_t* bridge)
{
    if (!bridge) return APHASIA_NONE;
    return bridge->summary.dominant_aphasia;
}

float language_immune_bridge_get_comprehension_capacity(
    const language_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->wernicke_effects.comprehension_capacity;
}

float language_immune_bridge_get_production_capacity(
    const language_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->broca_effects.production_capacity;
}

//=============================================================================
// Recovery API Implementation
//=============================================================================

recovery_phase_t language_immune_bridge_get_recovery_phase(
    const language_immune_bridge_t* bridge,
    language_region_t region)
{
    if (!bridge) return RECOVERY_NONE;

    switch (region) {
        case REGION_WERNICKE:
        case REGION_STG:
        case REGION_MTG:
            return bridge->wernicke_effects.inflammation.recovery_phase;
        case REGION_BROCA:
        case REGION_IFG:
            return bridge->broca_effects.inflammation.recovery_phase;
        default:
            return RECOVERY_NONE;
    }
}

float language_immune_bridge_get_recovery_progress(
    const language_immune_bridge_t* bridge,
    language_region_t region)
{
    if (!bridge) return 0.0f;

    switch (region) {
        case REGION_WERNICKE:
        case REGION_STG:
        case REGION_MTG:
            return bridge->wernicke_effects.inflammation.recovery_progress;
        case REGION_BROCA:
        case REGION_IFG:
            return bridge->broca_effects.inflammation.recovery_progress;
        default:
            return 0.0f;
    }
}

int language_immune_bridge_set_recovery_rate(
    language_immune_bridge_t* bridge,
    float rate)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_set_recovery_rate: bridge is NULL");
        return -1;
    }
    bridge->config.recovery_rate = rate;
    return 0;
}

//=============================================================================
// Aphasia Modeling API Implementation
//=============================================================================

int language_immune_bridge_set_aphasia_modeling(
    language_immune_bridge_t* bridge,
    bool enabled)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_set_aphasia_modeling: bridge is NULL");
        return -1;
    }
    bridge->config.enable_aphasia_modeling = enabled;
    return 0;
}

bool language_immune_bridge_has_symptom(
    const language_immune_bridge_t* bridge,
    aphasia_type_t symptom)
{
    if (!bridge) {
        return false;
    }

    switch (symptom) {
        case APHASIA_WERNICKE:
            return bridge->wernicke_effects.aphasia_type == APHASIA_WERNICKE;
        case APHASIA_BROCA:
            return bridge->broca_effects.aphasia_type == APHASIA_BROCA;
        case APHASIA_GLOBAL:
            return bridge->summary.dominant_aphasia == APHASIA_GLOBAL;
        case APHASIA_ANOMIC:
            return bridge->wernicke_effects.aphasia_type == APHASIA_ANOMIC ||
                   bridge->broca_effects.aphasia_type == APHASIA_ANOMIC;
        case APHASIA_CONDUCTION:
            /* Conduction: good comprehension, good production, poor repetition */
            return (bridge->summary.repetition_score < 0.5f &&
                    bridge->summary.comprehension_score > 0.7f &&
                    bridge->summary.production_score > 0.7f);
        default:
            return false;
    }
}

//=============================================================================
// Update and Query API Implementation
//=============================================================================

int language_immune_bridge_update(
    language_immune_bridge_t* bridge,
    uint64_t current_time_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_update: bridge is NULL");
        return -1;
    }
    if (!bridge->active) return 0;

    /* Save previous update time for delta calculation before overwriting */
    uint64_t prev_update_time_ms = bridge->stats.last_update_time_ms;
    bridge->stats.last_update_time_ms = current_time_ms;
    bridge->cytokines.last_update_ms = current_time_ms;

    /* Apply cytokine decay */
    float decay = 0.995f;
    bridge->cytokines.il1_beta *= decay;
    bridge->cytokines.il6 *= decay;
    bridge->cytokines.tnf_alpha *= decay;

    /* Update cytokines and inflammation */
    language_immune_bridge_update_cytokines(bridge);

    /* Compute regional effects */
    compute_wernicke_effects(bridge);
    compute_broca_effects(bridge);
    compute_nlp_effects(bridge);

    /* Update summary */
    update_impairment_summary(bridge);

    /* Apply recovery if inflammation decreasing */
    if (bridge->cytokines.composite_inflammatory < LANGUAGE_IMMUNE_MILD_THRESHOLD) {
        float recovery = bridge->config.recovery_rate;

        bridge->wernicke_effects.inflammation.recovery_progress += recovery;
        bridge->broca_effects.inflammation.recovery_progress += recovery;

        if (bridge->wernicke_effects.inflammation.recovery_progress >= 1.0f) {
            bridge->wernicke_effects.inflammation.recovery_phase = RECOVERY_COMPLETE;
            bridge->stats.recovery_events++;
        }
        if (bridge->broca_effects.inflammation.recovery_progress >= 1.0f) {
            bridge->broca_effects.inflammation.recovery_phase = RECOVERY_COMPLETE;
            bridge->stats.recovery_events++;
        }
    }

    /* Update statistics */
    if (bridge->cytokines.composite_inflammatory > LANGUAGE_IMMUNE_MILD_THRESHOLD) {
        bridge->stats.time_inflamed_ms += (current_time_ms - prev_update_time_ms);
    }

    if (bridge->wernicke_effects.inflammation.inflammation_level > bridge->stats.max_wernicke_inflammation) {
        bridge->stats.max_wernicke_inflammation = bridge->wernicke_effects.inflammation.inflammation_level;
    }
    if (bridge->broca_effects.inflammation.inflammation_level > bridge->stats.max_broca_inflammation) {
        bridge->stats.max_broca_inflammation = bridge->broca_effects.inflammation.inflammation_level;
    }

    /* Rolling average of impairments */
    bridge->stats.avg_comprehension_impairment =
        bridge->stats.avg_comprehension_impairment * 0.95f +
        (1.0f - bridge->wernicke_effects.comprehension_capacity) * 0.05f;
    bridge->stats.avg_production_impairment =
        bridge->stats.avg_production_impairment * 0.95f +
        (1.0f - bridge->broca_effects.production_capacity) * 0.05f;

    return 0;
}

int language_immune_bridge_apply_effects(language_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_apply_effects: bridge->active is NULL");
        return -1;
    }

    /* This would apply computed effects to actual Wernicke/Broca modules */
    /* For now, effects are computed and stored in the effects structs */

    return 0;
}

int language_immune_bridge_get_stats(
    const language_immune_bridge_t* bridge,
    language_immune_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(language_immune_stats_t));
    return 0;
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

int language_immune_bridge_bio_async_register(
    language_immune_bridge_t* bridge,
    bio_router_t* router)
{
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_bio_async_register: required parameter is NULL (bridge, router)");
        return -1;
    }

    bridge->bio_router = router;
    bridge->bio_async_registered = true;

    LOG_DEBUG(LOG_MODULE, "Registered with bio-async router");
    return 0;
}

int language_immune_bridge_bio_async_unregister(language_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_immune_bridge_bio_async_unregister: bridge is NULL");
        return -1;
    }

    bridge->bio_router = NULL;
    bridge->bio_async_registered = false;

    LOG_DEBUG(LOG_MODULE, "Unregistered from bio-async router");
    return 0;
}
