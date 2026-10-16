//=============================================================================
// nimcp_language_insula_bridge.h - Language-Insula Articulatory Bridge
//=============================================================================
/**
 * @file nimcp_language_insula_bridge.h
 * @brief Bridge for speech articulation planning and emotional prosody
 *
 * BIOLOGICAL BASIS:
 * - Anterior Insula: Speech motor planning, articulatory coordination
 * - Posterior Insula: Interoception, autonomic integration
 * - Insula-Broca connection: Articulatory programming for speech production
 * - Emotional prosody: Integrating emotion into speech patterns
 */

#ifndef NIMCP_LANGUAGE_INSULA_BRIDGE_H
#define NIMCP_LANGUAGE_INSULA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/insula/nimcp_insula_adapter.h"
#include "language/nimcp_language_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct language_insula_bridge language_insula_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;

#define LANGUAGE_INSULA_BIO_MODULE_ID 0x0826

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t update_interval_ms;
    bool enable_articulatory_planning;
    bool enable_emotional_prosody;
    bool enable_interoceptive_feedback;
    bool enable_bio_async;
} language_insula_config_t;

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    LI_STATE_IDLE = 0,
    LI_STATE_PLANNING,
    LI_STATE_ARTICULATING,
    LI_STATE_PROSODY_ACTIVE,
    LI_STATE_ERROR
} li_insula_state_t;

typedef enum {
    PROSODY_NEUTRAL = 0,
    PROSODY_HAPPY,
    PROSODY_SAD,
    PROSODY_ANGRY,
    PROSODY_FEARFUL,
    PROSODY_SURPRISED,
    PROSODY_DISGUSTED,
    PROSODY_COUNT
} emotional_prosody_t;

typedef enum {
    ARTICULATION_NORMAL = 0,
    ARTICULATION_SLOW,
    ARTICULATION_FAST,
    ARTICULATION_EMPHATIC,
    ARTICULATION_WHISPERED,
    ARTICULATION_COUNT
} articulation_style_t;

//=============================================================================
// Data Structures
//=============================================================================

typedef struct {
    char phoneme_sequence[128];       /**< Phonemes to articulate */
    uint32_t phoneme_count;           /**< Number of phonemes */
    articulation_style_t style;       /**< Articulation style */
    emotional_prosody_t prosody;      /**< Emotional coloring */
    float tempo;                      /**< Speech tempo [0.5-2.0] */
    float intensity;                  /**< Speech intensity [0-1] */
} articulation_plan_t;

typedef struct {
    emotional_prosody_t current;      /**< Current prosody */
    float intensity;                  /**< Prosody intensity [0-1] */
    float pitch_modulation;           /**< Pitch variation [-1, 1] */
    float tempo_modulation;           /**< Tempo variation [-1, 1] */
} prosody_state_t;

typedef struct {
    uint64_t plans_created;           /**< Articulation plans created */
    uint64_t prosody_changes;         /**< Prosody state changes */
    uint64_t articulation_errors;     /**< Articulation errors detected */
    li_insula_state_t state;          /**< Current bridge state */
    float avg_articulation_quality;   /**< Average articulation quality */
} language_insula_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_insula_config_t language_insula_default_config(void);

language_insula_bridge_t* language_insula_bridge_create(
    language_orchestrator_t* language,
    insula_adapter_t* insula,
    const language_insula_config_t* config
);

void language_insula_bridge_destroy(language_insula_bridge_t* bridge);

//=============================================================================
// Update Function
//=============================================================================

int language_insula_bridge_update(
    language_insula_bridge_t* bridge,
    uint64_t timestamp_ms
);

//=============================================================================
// Articulatory Planning
//=============================================================================

int language_insula_create_plan(
    language_insula_bridge_t* bridge,
    const char* phonemes,
    articulation_style_t style,
    articulation_plan_t* plan
);

int language_insula_execute_plan(
    language_insula_bridge_t* bridge,
    const articulation_plan_t* plan
);

//=============================================================================
// Emotional Prosody
//=============================================================================

int language_insula_set_prosody(
    language_insula_bridge_t* bridge,
    emotional_prosody_t prosody,
    float intensity
);

int language_insula_get_prosody(
    const language_insula_bridge_t* bridge,
    prosody_state_t* state
);

int language_insula_modulate_speech(
    language_insula_bridge_t* bridge,
    float* pitch,
    float* tempo
);

//=============================================================================
// Status Functions
//=============================================================================

int language_insula_get_stats(
    const language_insula_bridge_t* bridge,
    language_insula_stats_t* stats
);

li_insula_state_t language_insula_get_state(
    const language_insula_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_INSULA_BRIDGE_H */
