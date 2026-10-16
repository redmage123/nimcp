//=============================================================================
// nimcp_language_thalamic_bridge.h - Language Layer Thalamic Integration
//=============================================================================
/**
 * @file nimcp_language_thalamic_bridge.h
 * @brief Bridge between Language Layer and Thalamic Router
 *
 * WHAT: Routes language signals through thalamic relay and gating
 * WHY:  Language processing requires thalamic coordination for attention,
 *       sensory relay, and motor speech control
 * HOW:  Packages language events, routes via appropriate thalamic nuclei
 *
 * BIOLOGICAL BASIS:
 * - Pulvinar nucleus: Language attention and multimodal integration
 * - Ventral anterior (VA): Motor speech relay to motor cortex
 * - Mediodorsal (MD): Prefrontal language planning connections
 * - Lateral geniculate (LGN): Visual word form processing
 * - Medial geniculate (MGN): Auditory speech relay
 * - Thalamic reticular nucleus (TRN): Attention gating for language
 *
 * SIGNAL ROUTING:
 * - Comprehension → Pulvinar → Parietal/temporal integration
 * - Production → VA/VL → Motor cortex (M1)
 * - Attention → TRN → Selective language focus
 * - Semantic → MD → Prefrontal conceptual processing
 *
 * @version 1.0.0 - Phase L8: Thalamic Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_THALAMIC_BRIDGE_H
#define NIMCP_LANGUAGE_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_orchestrator language_orchestrator_t;
typedef struct thalamic_router thalamic_router_t;

//=============================================================================
// Signal Types
//=============================================================================

#define LANG_THAL_SIGNAL_PHONEME_RELAY     0x0E70  /**< Phoneme to auditory areas */
#define LANG_THAL_SIGNAL_WORD_RELAY        0x0E71  /**< Word to semantic areas */
#define LANG_THAL_SIGNAL_ATTENTION_GATE    0x0E72  /**< Attention gating request */
#define LANG_THAL_SIGNAL_MOTOR_SPEECH      0x0E73  /**< Motor speech command */
#define LANG_THAL_SIGNAL_SEMANTIC_RELAY    0x0E74  /**< Semantic to prefrontal */
#define LANG_THAL_SIGNAL_COMPREHENSION     0x0E75  /**< Comprehension result */
#define LANG_THAL_SIGNAL_PRODUCTION        0x0E76  /**< Production plan */
#define LANG_THAL_SIGNAL_MULTIMODAL        0x0E77  /**< Multimodal integration */

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Thalamic nucleus target
 */
typedef enum {
    LANG_THAL_NUCLEUS_PULVINAR = 0,   /**< Pulvinar - attention/multimodal */
    LANG_THAL_NUCLEUS_VA,              /**< Ventral anterior - motor speech */
    LANG_THAL_NUCLEUS_VL,              /**< Ventral lateral - motor relay */
    LANG_THAL_NUCLEUS_MD,              /**< Mediodorsal - prefrontal */
    LANG_THAL_NUCLEUS_LGN,             /**< Lateral geniculate - visual */
    LANG_THAL_NUCLEUS_MGN,             /**< Medial geniculate - auditory */
    LANG_THAL_NUCLEUS_TRN,             /**< Reticular - attention gating */
    LANG_THAL_NUCLEUS_COUNT
} language_thalamic_nucleus_t;

/**
 * @brief Language thalamic signal
 */
typedef struct {
    uint32_t signal_type;              /**< Signal type */
    language_thalamic_nucleus_t target;/**< Target nucleus */
    float priority;                    /**< Signal priority [0-1] */
    float attention_weight;            /**< Attention level [0-1] */
    float gating_threshold;            /**< Gating threshold */
    uint32_t sequence_id;              /**< Utterance/sequence ID */
    void* payload;                     /**< Signal payload */
    uint32_t payload_size;             /**< Payload size */
    uint64_t timestamp_us;             /**< Timestamp */
} language_thalamic_signal_t;

/**
 * @brief Bridge configuration
 */
#ifndef LANGUAGE_THALAMIC_CONFIG_T_DEFINED
#define LANGUAGE_THALAMIC_CONFIG_T_DEFINED
typedef struct {
    /* Enable flags */
    bool enable_attention_gating;      /**< Gate by attention level */
    bool enable_motor_priority;        /**< Prioritize motor commands */
    bool enable_semantic_routing;      /**< Route semantic to MD */
    bool enable_multimodal_routing;    /**< Route via pulvinar */

    /* Thresholds */
    float attention_threshold;         /**< Minimum attention for routing */
    float motor_priority_boost;        /**< Priority boost for motor */
    float gating_decay_rate;           /**< Gating decay per second */

    /* Timing */
    uint32_t relay_latency_us;         /**< Simulated relay latency */
    uint32_t gating_window_ms;         /**< Attention gating window */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    uint32_t update_interval_ms;       /**< Update cycle interval */
} language_thalamic_config_t;
#endif /* LANGUAGE_THALAMIC_CONFIG_T_DEFINED */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t phonemes_relayed;         /**< Phoneme signals routed */
    uint64_t words_relayed;            /**< Word signals routed */
    uint64_t motor_commands_sent;      /**< Motor commands sent */
    uint64_t semantic_relays;          /**< Semantic relays */
    uint64_t attention_gates;          /**< Attention gating events */
    uint64_t signals_blocked;          /**< Signals blocked by gate */
    float avg_relay_latency_ms;        /**< Average relay latency */
    float avg_attention_level;         /**< Average attention */
} language_thalamic_stats_t;

/**
 * @brief Bridge state
 */
struct language_thalamic_bridge {
    language_thalamic_config_t config;
    bool initialized;
    bool active;

    language_orchestrator_t* orchestrator;
    thalamic_router_t* router;

    float current_attention;
    float gating_state[LANG_THAL_NUCLEUS_COUNT];

    language_thalamic_stats_t stats;
};

typedef struct language_thalamic_bridge language_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

void language_thalamic_default_config(language_thalamic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

language_thalamic_bridge_t* language_thalamic_bridge_create(
    language_orchestrator_t* orchestrator,
    const language_thalamic_config_t* config);

void language_thalamic_bridge_destroy(language_thalamic_bridge_t* bridge);

int language_thalamic_bridge_connect_router(
    language_thalamic_bridge_t* bridge,
    thalamic_router_t* router);

//=============================================================================
// Signal Routing API
//=============================================================================

int language_thalamic_bridge_route_phoneme(
    language_thalamic_bridge_t* bridge,
    const void* phoneme_data,
    uint32_t size);

int language_thalamic_bridge_route_word(
    language_thalamic_bridge_t* bridge,
    const void* word_data,
    uint32_t size);

int language_thalamic_bridge_route_motor_speech(
    language_thalamic_bridge_t* bridge,
    const void* motor_data,
    uint32_t size);

int language_thalamic_bridge_route_semantic(
    language_thalamic_bridge_t* bridge,
    const void* semantic_data,
    uint32_t size);

int language_thalamic_bridge_send_signal(
    language_thalamic_bridge_t* bridge,
    const language_thalamic_signal_t* signal);

//=============================================================================
// Attention Gating API
//=============================================================================

int language_thalamic_bridge_set_attention(
    language_thalamic_bridge_t* bridge,
    float attention_level);

int language_thalamic_bridge_gate_nucleus(
    language_thalamic_bridge_t* bridge,
    language_thalamic_nucleus_t nucleus,
    float gate_level);

float language_thalamic_bridge_get_gate_state(
    const language_thalamic_bridge_t* bridge,
    language_thalamic_nucleus_t nucleus);

//=============================================================================
// Statistics API
//=============================================================================

int language_thalamic_bridge_get_stats(
    const language_thalamic_bridge_t* bridge,
    language_thalamic_stats_t* stats);

void language_thalamic_bridge_reset_stats(language_thalamic_bridge_t* bridge);

//=============================================================================
// String Conversion
//=============================================================================

const char* language_thalamic_nucleus_to_string(language_thalamic_nucleus_t nucleus);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_THALAMIC_BRIDGE_H */
