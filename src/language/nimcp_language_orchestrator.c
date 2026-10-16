#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_orchestrator.c - Language Layer Central Orchestrator
//=============================================================================
/**
 * @file nimcp_language_orchestrator.c
 * @brief Implementation of the Language Layer central orchestrator
 *
 * Implements state machine, lifecycle management, subsystem coordination,
 * and bridge integration for unified language processing.
 *
 * @version 1.0.0 - Phase L5: Orchestrator Implementation
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#include "language/nimcp_language_orchestrator.h"
#include "language/nimcp_language_bio_async.h"
#include "language/bridges/nimcp_language_perception_bridge.h"
#include "language/bridges/nimcp_language_cognitive_bridge.h"
#include "language/bridges/nimcp_language_training_bridge.h"
#include "language/bridges/nimcp_language_omni_bridge.h"
#include "language/bridges/nimcp_language_immune_bridge.h"
#include "language/bridges/nimcp_language_gpu_bridge.h"

#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_orchestrator)

//=============================================================================
// Internal Constants
//=============================================================================

#define ORCHESTRATOR_MAX_CALLBACKS          8
#define ORCHESTRATOR_MAX_PENDING_PHONEMES   256
#define ORCHESTRATOR_MAX_PENDING_WORDS      64
#define ORCHESTRATOR_STATE_TIMEOUT_MS       NIMCP_DEFAULT_TIMEOUT_MS

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Callback registration entry
 */
typedef struct {
    language_event_callback_t callback;
    void* user_data;
    bool active;
} callback_entry_t;

/**
 * @brief Pending input buffer
 */
typedef struct {
    language_phoneme_t* phonemes;
    uint32_t phoneme_count;
    uint32_t phoneme_capacity;

    language_word_t* words;
    uint32_t word_count;
    uint32_t word_capacity;

    char* text_buffer;
    uint32_t text_length;
    uint32_t text_capacity;
} input_buffer_t;

/**
 * @brief Internal orchestrator structure
 */
struct language_orchestrator {
    /* Configuration */
    language_orchestrator_config_t config;
    bool initialized;
    bool running;

    /* State machine */
    language_state_t state;
    language_state_t prev_state;
    language_mode_t mode;
    uint64_t state_entry_time_ms;
    uint64_t last_update_ms;

    /* Subsystem connections */
    wernicke_adapter_t* wernicke;
    broca_adapter_t* broca;
    nlp_network_t nlp_network;
    speech_cortex_t* speech_cortex;
    multimodal_nlp_bridge_t* multimodal;

    /* Bridge connections */
    language_perception_bridge_t* perception_bridge;
    language_cognitive_bridge_t* cognitive_bridge;
    language_training_bridge_t* training_bridge;
    language_omni_bridge_t* omni_bridge;
    language_immune_bridge_t* immune_bridge;
    language_gpu_bridge_t* gpu_bridge;

    /* Input buffers */
    input_buffer_t input;

    /* Current results */
    language_comprehension_result_t current_comprehension;
    bool comprehension_valid;

    language_production_plan_t current_production;
    bool production_valid;

    /* Event callbacks */
    callback_entry_t callbacks[ORCHESTRATOR_MAX_CALLBACKS];
    uint32_t num_callbacks;

    /* Statistics */
    language_orchestrator_stats_t stats;

    /* Bio-async */
    bool bio_async_registered;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static int orchestrator_init_buffers(language_orchestrator_t* orch);
static void orchestrator_free_buffers(language_orchestrator_t* orch);
static int orchestrator_transition_state(language_orchestrator_t* orch,
                                         language_state_t new_state,
                                         uint64_t current_time_ms);
static int orchestrator_process_state(language_orchestrator_t* orch,
                                      uint64_t current_time_ms);
static void orchestrator_emit_event(language_orchestrator_t* orch,
                                    const language_event_t* event);
static int orchestrator_update_bridges(language_orchestrator_t* orch,
                                       uint64_t current_time_ms);

// Forward declarations for language production (used by part_core.c)
struct lexical_access;
typedef struct lexical_access lexical_access_t;
extern lexical_access_t* wernicke_get_lexical_access(wernicke_adapter_t* adapter);
extern uint32_t lexical_get_size(const lexical_access_t* lex);

// Lexical entry for word lookup (simplified local copy to avoid header conflicts)
typedef struct {
    uint32_t word_id;
    char orthography[32];
    uint8_t phonemes[24];
    uint32_t phoneme_count;
    int pos;
    float frequency;
    uint32_t syllable_count;
    uint32_t concept_id;
    uint32_t* sense_ids;
    uint32_t num_senses;
    uint32_t* neighbors;
    uint32_t num_neighbors;
    float* embedding;
} lexical_entry_t;

extern bool lexical_get_entry(const lexical_access_t* lex, uint32_t word_id, lexical_entry_t* entry);
extern bool broca_produce_from_strings(broca_adapter_t* adapter, const char* const* words, uint32_t num_words, void* result);

// lpb_token_t (local copy to avoid header conflicts)
typedef struct {
    uint32_t token_id;
    char token_str[32];
    uint8_t pos;
    float activation;
    float frequency;
} lpb_token_t;


//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_language_orchestrator_part_accessors.c"  // 7 functions: accessors
#include "nimcp_language_orchestrator_part_lifecycle.c"  // 8 functions: lifecycle
#include "nimcp_language_orchestrator_part_core.c"  // 16 functions: core
#include "nimcp_language_orchestrator_part_processing.c"  // 8 functions: processing
#include "nimcp_language_orchestrator_part_helpers.c"  // 2 functions: helpers
#include "nimcp_language_orchestrator_part_io.c"  // 10 functions: io
