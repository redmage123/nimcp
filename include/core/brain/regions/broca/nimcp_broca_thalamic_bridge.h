/**
 * @file nimcp_broca_thalamic_bridge.h
 * @brief Bridge between Broca's region and thalamic router
 *
 * WHAT: Routes language production signals through thalamic relay
 * WHY: Speech output requires thalamic coordination with motor cortex
 * HOW: Packages speech commands, routes via ventral anterior nucleus pathway
 *
 * BIOLOGICAL BASIS:
 * - Ventral anterior (VA) and ventral lateral (VL) nuclei relay motor speech
 * - Pulvinar coordinates language attention with parietal areas
 * - Thalamic reticular nucleus gates speech initiation
 * - Mediodorsal nucleus links to prefrontal language planning
 *
 * SIGNAL ROUTING:
 * - Speech motor commands → VA/VL → Motor cortex (M1)
 * - Syntactic structures → Pulvinar → Parietal integration
 * - Lexical access requests → MD → Prefrontal cortex
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BROCA_THALAMIC_BRIDGE_H
#define NIMCP_BROCA_THALAMIC_BRIDGE_H

#include "middleware/routing/nimcp_thalamic_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Signal Types
//=============================================================================

#define BROCA_SIGNAL_MOTOR_COMMAND    0x4401  /**< Motor speech command */
#define BROCA_SIGNAL_PHONEME_SEQUENCE 0x4402  /**< Phoneme sequence ready */
#define BROCA_SIGNAL_SYNTAX_COMPLETE  0x4403  /**< Syntactic structure built */
#define BROCA_SIGNAL_LEXICAL_REQUEST  0x4404  /**< Request lexical access */
#define BROCA_SIGNAL_UTTERANCE_START  0x4405  /**< Utterance initiation */
#define BROCA_SIGNAL_UTTERANCE_END    0x4406  /**< Utterance completion */

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Broca thalamic signal
 */
typedef struct {
    uint32_t signal_type;           /**< Signal type from defines above */
    float speech_urgency;           /**< Urgency/priority [0-1] */
    float attention_weight;         /**< Current attention [0-1] */
    uint32_t sequence_id;           /**< Utterance/sequence ID */
    uint32_t word_count;            /**< Words in utterance */
    uint32_t phoneme_count;         /**< Phonemes ready */
    void* content;                  /**< Signal content */
    uint32_t content_size;          /**< Content size */
    uint64_t timestamp_us;          /**< Timestamp */
} broca_thalamic_signal_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_attention_gating;   /**< Gate signals by attention */
    bool enable_motor_priority;     /**< Prioritize motor commands */
    bool enable_syntax_routing;     /**< Route syntax to parietal */
    float min_urgency_threshold;    /**< Minimum urgency for routing */
    float motor_boost;              /**< Motor command priority boost */
    float attention_decay_rate;     /**< Attention decay per second */
} broca_thalamic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t motor_commands_routed; /**< Motor commands sent */
    uint64_t phoneme_sequences;     /**< Phoneme sequences routed */
    uint64_t syntax_completions;    /**< Syntax structures sent */
    uint64_t lexical_requests;      /**< Lexical access requests */
    uint64_t utterances_started;    /**< Utterances initiated */
    uint64_t utterances_completed;  /**< Utterances finished */
    uint64_t signals_gated;         /**< Signals blocked by attention */
    float avg_speech_urgency;       /**< Average urgency level */
} broca_thalamic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct broca_thalamic_bridge broca_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default config with sensible values
 */
broca_thalamic_config_t broca_thalamic_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Broca thalamic bridge
 * @param broca Broca adapter handle
 * @param router Thalamic router handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
broca_thalamic_bridge_t* broca_thalamic_bridge_create(
    void* broca,
    thalamic_router_t* router,
    const broca_thalamic_config_t* config
);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void broca_thalamic_bridge_destroy(broca_thalamic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int broca_thalamic_bridge_reset(broca_thalamic_bridge_t* bridge);

//=============================================================================
// Signal Routing API
//=============================================================================

/**
 * @brief Route speech signal through thalamus
 * @param bridge Bridge handle
 * @param signal Signal to route
 * @return 0 on success, -1 on error
 */
int broca_thalamic_route_signal(
    broca_thalamic_bridge_t* bridge,
    const broca_thalamic_signal_t* signal
);

/**
 * @brief Route motor command with priority
 * @param bridge Bridge handle
 * @param command Motor command data
 * @param command_size Command size
 * @param urgency Urgency level [0-1]
 * @return 0 on success, -1 on error
 */
int broca_thalamic_route_motor_command(
    broca_thalamic_bridge_t* bridge,
    const void* command,
    uint32_t command_size,
    float urgency
);

/**
 * @brief Route phoneme sequence
 * @param bridge Bridge handle
 * @param phonemes Phoneme data
 * @param count Phoneme count
 * @return 0 on success, -1 on error
 */
int broca_thalamic_route_phonemes(
    broca_thalamic_bridge_t* bridge,
    const uint8_t* phonemes,
    uint32_t count
);

/**
 * @brief Signal utterance start
 * @param bridge Bridge handle
 * @param sequence_id Utterance ID
 * @param word_count Expected words
 * @return 0 on success, -1 on error
 */
int broca_thalamic_signal_utterance_start(
    broca_thalamic_bridge_t* bridge,
    uint32_t sequence_id,
    uint32_t word_count
);

/**
 * @brief Signal utterance end
 * @param bridge Bridge handle
 * @param sequence_id Utterance ID
 * @return 0 on success, -1 on error
 */
int broca_thalamic_signal_utterance_end(
    broca_thalamic_bridge_t* bridge,
    uint32_t sequence_id
);

//=============================================================================
// Attention API
//=============================================================================

/**
 * @brief Set attention level
 * @param bridge Bridge handle
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int broca_thalamic_set_attention(broca_thalamic_bridge_t* bridge, float attention);

/**
 * @brief Get current attention level
 * @param bridge Bridge handle
 * @param attention Output: attention level
 * @return 0 on success, -1 on error
 */
int broca_thalamic_get_attention(const broca_thalamic_bridge_t* bridge, float* attention);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int broca_thalamic_bridge_get_stats(
    const broca_thalamic_bridge_t* bridge,
    broca_thalamic_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 */
void broca_thalamic_bridge_reset_stats(broca_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BROCA_THALAMIC_BRIDGE_H */
