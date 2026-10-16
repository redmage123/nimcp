/**
 * @file nimcp_multimodal_nlp_immune_bridge.h
 * @brief Multimodal NLP-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and multimodal NLP
 * WHY:  Inflammation affects cross-modal integration and speech processing; multimodal
 *       processing errors trigger stress responses. Essential for realistic multimodal
 *       language modeling.
 * HOW:  Cytokines impair cross-modal binding and speech production; processing failures
 *       trigger immune responses; successful multimodal comprehension reduces inflammation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → MULTIMODAL NLP PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines:
 *    - Impair cross-modal integration (vision-language binding)
 *    - Reduce speech production fluency
 *    - Impair phonological processing
 *    - Degrade multimodal attention
 *    - Reference: Krabbe et al. (2005) "Brain-derived neurotrophic factor"
 *
 * 2. Chronic Inflammation:
 *    - Sustained cross-modal binding deficits
 *    - Speech production errors (word-finding, paraphasias)
 *    - Reduced multimodal working memory
 *    - Reference: Marsland et al. (2006) "Brain morphology links inflammation"
 *
 * 3. Speech-Specific Effects:
 *    - Motor planning impairment
 *    - Articulatory coordination deficits
 *    - Prosody degradation
 *    - Reference: Duffy (2013) "Motor Speech Disorders"
 *
 * MULTIMODAL NLP → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Cross-modal Binding Failures:
 *    - Failed integration → frustration → stress response
 *    - Multimodal confusion → inflammatory cytokines
 *    - Reference: Elenkov et al. (2000) "The sympathetic nerve"
 *
 * 2. Speech Production Errors:
 *    - Dysfluency → anxiety → cortisol elevation
 *    - Communication failure → chronic inflammation
 *    - Reference: Brosschot et al. (2006) "Perseverative cognition"
 *
 * 3. Successful Multimodal Processing:
 *    - Coherent integration → IL-10 release
 *    - Fluent speech → reduced stress
 *    - Reference: Davidson et al. (2003) "Mindfulness meditation"
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MULTIMODAL_NLP_IMMUNE_BRIDGE_H
#define NIMCP_MULTIMODAL_NLP_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "nlp/nimcp_multimodal_nlp_bridge.h"
#include "perception/nimcp_speech_cortex.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine multimodal impact factors */
#define CYTOKINE_IL1_MULTIMODAL_IMPACT    -0.25f  /**< IL-1β → integration deficit */
#define CYTOKINE_IL6_MULTIMODAL_IMPACT    -0.2f   /**< IL-6 → integration deficit */
#define CYTOKINE_TNF_MULTIMODAL_IMPACT    -0.35f  /**< TNF-α → strong deficit */
#define CYTOKINE_IL10_MULTIMODAL_IMPACT    0.2f   /**< IL-10 → recovery */

/* Inflammation multimodal capacity reduction */
#define INFLAMMATION_MULTIMODAL_BASE      0.9f    /**< Base capacity */
#define INFLAMMATION_MULTIMODAL_PER_LEVEL 0.15f   /**< Per level reduction */

/* Speech production thresholds */
#define SPEECH_ERROR_IMMUNE_THRESHOLD     0.25f   /**< Error rate threshold */
#define SPEECH_DYSFLUENCY_THRESHOLD       0.3f    /**< Dysfluency threshold */
#define MULTIMODAL_BINDING_THRESHOLD      0.6f    /**< Binding success threshold */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine multimodal effects
 */
typedef struct {
    float il1_multimodal_deficit;
    float il6_multimodal_deficit;
    float tnf_multimodal_deficit;
    float il10_multimodal_recovery;

    float total_integration_impairment;  /**< Cross-modal binding [0-1] */
    float speech_production_deficit;     /**< Speech fluency [0-1] */
    float phonological_impairment;       /**< Phoneme processing [0-1] */
    float working_memory_deficit;        /**< Multimodal WM [0-1] */
} multimodal_cytokine_effects_t;

/**
 * @brief Inflammation multimodal state
 */
typedef struct {
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;

    float integration_capacity;          /**< Cross-modal capacity [0-1] */
    float speech_fluency_factor;         /**< Speech fluency [0-1] */
    float binding_error_rate;            /**< Binding errors [0-1] */
    float phonological_error_rate;       /**< Phoneme errors [0-1] */

    uint32_t binding_errors;
    uint32_t speech_errors;
    uint32_t phonological_errors;
} multimodal_inflammation_state_t;

/**
 * @brief Multimodal-driven immune modulation
 */
typedef struct {
    float binding_success_rate;
    float speech_fluency_rate;
    float multimodal_complexity;

    bool binding_failure_inflammation;
    bool speech_error_inflammation;
    float il10_from_fluent_speech;

    uint32_t total_multimodal_inputs;
    uint32_t successful_integrations;
    uint32_t failed_integrations;
} multimodal_immune_modulation_t;

/**
 * @brief Multimodal NLP-immune bridge configuration
 */
typedef struct {
    bool enable_cytokine_multimodal_impairment;
    bool enable_inflammation_binding_errors;
    bool enable_binding_failure_inflammation;
    bool enable_speech_error_inflammation;
    bool enable_fluent_speech_il10;

    float cytokine_sensitivity;
    float inflammation_sensitivity;
    float error_immune_sensitivity;

    float binding_failure_threshold;
    float speech_error_threshold;
    float fluency_success_threshold;
} multimodal_nlp_immune_config_t;

/**
 * @brief Complete multimodal NLP-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    brain_immune_system_t* immune_system;
    speech_cortex_t* speech_cortex;
    visual_cortex_t* visual_cortex;
    audio_cortex_t* audio_cortex;

    multimodal_cytokine_effects_t cytokine_effects;
    multimodal_inflammation_state_t inflammation_state;
    multimodal_immune_modulation_t multimodal_modulation;

    multimodal_nlp_immune_config_t config;

    uint64_t last_update_time;
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t binding_error_triggers;
    uint32_t speech_error_triggers;
    uint32_t fluency_boosts;} multimodal_nlp_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int multimodal_nlp_immune_default_config(multimodal_nlp_immune_config_t* config);

multimodal_nlp_immune_bridge_t* multimodal_nlp_immune_bridge_create(
    const multimodal_nlp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    speech_cortex_t* speech_cortex,
    visual_cortex_t* visual_cortex,
    audio_cortex_t* audio_cortex
);

void multimodal_nlp_immune_bridge_destroy(multimodal_nlp_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Multimodal NLP API
 * ============================================================================ */

int multimodal_nlp_immune_apply_cytokine_effects(multimodal_nlp_immune_bridge_t* bridge);

int multimodal_nlp_immune_apply_inflammation_effects(multimodal_nlp_immune_bridge_t* bridge);

float multimodal_nlp_immune_compute_integration_capacity(
    const multimodal_nlp_immune_bridge_t* bridge
);

float multimodal_nlp_immune_compute_speech_fluency(
    const multimodal_nlp_immune_bridge_t* bridge
);

/* ============================================================================
 * Multimodal NLP → Immune API
 * ============================================================================ */

int multimodal_nlp_immune_trigger_binding_failure_inflammation(
    multimodal_nlp_immune_bridge_t* bridge,
    float binding_error_rate
);

int multimodal_nlp_immune_trigger_speech_error_inflammation(
    multimodal_nlp_immune_bridge_t* bridge,
    float speech_error_rate
);

int multimodal_nlp_immune_release_il10_from_fluent_speech(
    multimodal_nlp_immune_bridge_t* bridge,
    float fluency_rate
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int multimodal_nlp_immune_bridge_update(
    multimodal_nlp_immune_bridge_t* bridge,
    uint64_t delta_ms
);

int multimodal_nlp_immune_apply_modulation(
    multimodal_nlp_immune_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

int multimodal_nlp_immune_get_cytokine_effects(
    const multimodal_nlp_immune_bridge_t* bridge,
    multimodal_cytokine_effects_t* effects
);

int multimodal_nlp_immune_get_inflammation_state(
    const multimodal_nlp_immune_bridge_t* bridge,
    multimodal_inflammation_state_t* state
);

bool multimodal_nlp_immune_has_integration_deficit(
    const multimodal_nlp_immune_bridge_t* bridge
);

float multimodal_nlp_immune_get_integration_capacity(
    const multimodal_nlp_immune_bridge_t* bridge
);

float multimodal_nlp_immune_get_binding_error_rate(
    const multimodal_nlp_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int multimodal_nlp_immune_connect_bio_async(
    multimodal_nlp_immune_bridge_t* bridge
);

int multimodal_nlp_immune_disconnect_bio_async(
    multimodal_nlp_immune_bridge_t* bridge
);

bool multimodal_nlp_immune_is_bio_async_connected(
    const multimodal_nlp_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MULTIMODAL_NLP_IMMUNE_BRIDGE_H */
