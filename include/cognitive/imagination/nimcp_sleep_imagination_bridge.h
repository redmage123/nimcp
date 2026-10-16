/**
 * @file nimcp_sleep_imagination_bridge.h
 * @brief Sleep-Imagination Bidirectional Bridge
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting sleep system with imagination engine
 * WHY:  Sleep stages profoundly modulate imaginative processes; dreams are imagination
 * HOW:  Full bridge pattern with effects in both directions
 *
 * BIOLOGICAL BASIS:
 * Sleep and imagination are deeply intertwined:
 * - REM sleep triggers creative imagination and dream generation
 * - NREM stages facilitate memory consolidation and replay
 * - Sleep stage transitions modulate vividness and content
 * - Dreams represent spontaneous imagination during altered consciousness
 * - Creative problem-solving often occurs during sleep-wake transitions
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * |    SLEEP SYSTEM      |                    |  IMAGINATION ENGINE  |
 * |                      |                    |                      |
 * | * Sleep stages       |<---- REM mode ---->| * Scenario manager   |
 * | * Circadian rhythm   |     activation     | * Latent space       |
 * | * Consolidation      |                    | * World model        |
 * | * Replay triggers    |<-- dream content --| * Visual generation  |
 * | * Wake signals       |    generation      | * Prospective sim    |
 * |                      |                    |                      |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +--------------- BRIDGE --------------------+
 *                    (bidirectional effects)
 * ```
 *
 * SLEEP STAGES AND IMAGINATION:
 * - WAKE: Full conscious imagination, voluntary control
 * - NREM1: Hypnagogic imagery, reduced control, spontaneous images
 * - NREM2: Minimal imagery, memory consolidation active
 * - NREM3: Deep sleep, no conscious imagery, system maintenance
 * - REM: Vivid dreaming, creative recombination, emotional processing
 *
 * USAGE:
 * ```c
 * sleep_imagination_bridge_t* bridge = sleep_imagination_bridge_create(NULL);
 * sleep_imagination_connect_sleep(bridge, sleep_system);
 * sleep_imagination_connect_imagination(bridge, imagination_engine);
 *
 * // In update loop:
 * sleep_imagination_update(bridge, delta_time);
 *
 * // Notify of sleep stage change
 * sleep_imagination_set_sleep_stage(bridge, SLEEP_STAGE_REM);
 * ```
 */

#ifndef NIMCP_SLEEP_IMAGINATION_BRIDGE_H
#define NIMCP_SLEEP_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/tensor/nimcp_tensor.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct sleep_system;
struct imagination_engine;
struct imagination_scenario;
struct imagination_goal;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum dream scenarios to track per sleep session */
#define SLEEP_IMAG_MAX_DREAM_SCENARIOS      8

/** Default REM vividness multiplier */
#define SLEEP_IMAG_DEFAULT_REM_VIVIDNESS    0.9f

/** Default NREM1 vividness multiplier (hypnagogic) */
#define SLEEP_IMAG_DEFAULT_NREM1_VIVIDNESS  0.4f

/** Default creative mode boost during REM */
#define SLEEP_IMAG_DEFAULT_REM_CREATIVITY   1.8f

/*=============================================================================
 * SLEEP STAGE ENUMERATION
 *===========================================================================*/

/**
 * @brief Sleep stages affecting imagination
 *
 * WHAT: Classification of sleep-wake states
 * WHY:  Each stage has distinct effects on imaginative capacity
 */
typedef enum {
    SLEEP_STAGE_WAKE = 0,       /**< Awake: full conscious control */
    SLEEP_STAGE_NREM1,          /**< Light sleep: hypnagogic imagery */
    SLEEP_STAGE_NREM2,          /**< Sleep spindles: reduced imagery */
    SLEEP_STAGE_NREM3,          /**< Deep sleep: minimal imagery */
    SLEEP_STAGE_REM,            /**< REM: vivid dreaming */
    SLEEP_STAGE_COUNT
} sleep_stage_t;

/**
 * @brief Sleep stage descriptors for each stage
 */
typedef struct {
    const char* name;           /**< Stage name */
    float vividness_factor;     /**< Imagery vividness [0.0-1.0] */
    float control_factor;       /**< Voluntary control [0.0-1.0] */
    float creativity_factor;    /**< Creative recombination [0.0-2.0] */
    bool imagery_active;        /**< Whether imagery is active */
    bool consolidation_active;  /**< Memory consolidation active */
} sleep_stage_descriptor_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from sleep system to imagination
 *
 * WHAT: Sleep-derived modulation of imagination parameters
 * WHY:  Sleep state profoundly affects imaginative capacity
 */
typedef struct {
    /* Current sleep state */
    sleep_stage_t current_stage;        /**< Current sleep stage */
    float stage_depth;                  /**< How deep into stage [0.0-1.0] */
    float stage_stability;              /**< Stability of current stage */

    /* Imagination modulation */
    float vividness_modulation;         /**< How vivid imagery can be [0.0-1.0] */
    float control_modulation;           /**< Voluntary control level [0.0-1.0] */
    float creativity_boost;             /**< Creative recombination factor [0.0-2.0] */
    bool enable_creative_mode;          /**< Enable REM-like creative mode */

    /* Content influences */
    float emotional_salience_weight;    /**< Weight for emotional content */
    float recency_weight;               /**< Weight for recent memories */
    float novelty_preference;           /**< Preference for novel combinations */

    /* Consolidation signals */
    bool trigger_consolidation;         /**< Signal to consolidate imagined content */
    float consolidation_strength;       /**< Strength of consolidation signal */

    /* Replay influence */
    bool replay_active;                 /**< Whether hippocampal replay is occurring */
    float replay_content_influence;     /**< How much replay feeds into imagination */
} sleep_to_imagination_effects_t;

/**
 * @brief Effects flowing from imagination to sleep system
 *
 * WHAT: Imagination-derived signals for sleep modulation
 * WHY:  Dream content and activity feed back to sleep regulation
 */
typedef struct {
    /* Dream activity metrics */
    float dream_activity_level;         /**< Overall dream activity [0.0-1.0] */
    float emotional_intensity;          /**< Emotional intensity of dreams */
    bool nightmare_detected;            /**< High-intensity negative content */

    /* Content generation state */
    uint32_t active_scenario_id;        /**< Currently active dream scenario */
    nimcp_tensor_t* dream_embedding;    /**< Latent representation of dream content */
    float narrative_coherence;          /**< How coherent the dream narrative is */

    /* Replay requests */
    bool request_replay;                /**< Request hippocampal replay */
    nimcp_tensor_t* replay_cue;         /**< Cue for replay content */

    /* Stage transition signals */
    bool suggest_stage_change;          /**< Suggest transitioning stages */
    sleep_stage_t suggested_stage;      /**< Suggested target stage */

    /* Memory encoding */
    bool encode_dream_memory;           /**< Should encode dream as memory */
    float dream_salience;               /**< Importance of dream content */
} imagination_to_sleep_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Stage-specific parameters */
    float rem_vividness;                /**< Vividness during REM [0.0-1.0] */
    float rem_creativity_boost;         /**< Creativity boost during REM [1.0-3.0] */
    float nrem1_vividness;              /**< Vividness during NREM1 [0.0-1.0] */

    /* Content generation */
    bool enable_spontaneous_dreams;     /**< Allow spontaneous dream generation */
    bool enable_lucid_mode;             /**< Allow lucid dreaming (increased control) */
    float lucid_control_threshold;      /**< Threshold for lucid awareness */

    /* Consolidation */
    bool enable_consolidation_signals;  /**< Send consolidation signals */
    float consolidation_threshold;      /**< Threshold for triggering consolidation */

    /* Nightmare management */
    bool enable_nightmare_detection;    /**< Detect and manage nightmares */
    float nightmare_intensity_threshold; /**< Threshold for nightmare detection */
    bool enable_nightmare_interruption; /**< Can interrupt nightmares */

    /* Update frequency */
    float update_interval_ms;           /**< Minimum time between updates */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} sleep_imagination_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Sleep stage tracking */
    uint64_t stage_transitions;         /**< Total stage transitions */
    uint64_t rem_periods;               /**< Total REM periods entered */
    float total_rem_time_ms;            /**< Total time in REM */

    /* Dream generation stats */
    uint64_t dreams_generated;          /**< Total dreams generated */
    uint64_t nightmares_detected;       /**< Nightmares detected */
    uint64_t nightmares_interrupted;    /**< Nightmares interrupted */

    /* Consolidation stats */
    uint64_t consolidation_triggers;    /**< Times consolidation triggered */
    uint64_t dreams_encoded;            /**< Dreams encoded as memories */

    /* Creativity stats */
    float avg_creativity_during_rem;    /**< Average creativity during REM */
    uint64_t creative_recombinations;   /**< Creative recombinations made */

    /* Timing */
    uint64_t total_updates;             /**< Total update calls */
    float avg_update_time_ms;           /**< Average update time */
} sleep_imagination_stats_t;

/*=============================================================================
 * MAIN BRIDGE STRUCTURE
 *===========================================================================*/

/**
 * @brief Sleep-Imagination bridge
 *
 * Coordinates bidirectional communication between sleep system and imagination.
 */
typedef struct sleep_imagination_bridge {
    bridge_base_t base;                 /**< MUST be first - base bridge infrastructure */

    /* Connected systems (typed for convenience, also in base) */
    struct sleep_system* sleep;
    struct imagination_engine* imagination;

    /* Bidirectional effects */
    sleep_to_imagination_effects_t sleep_to_imag;
    imagination_to_sleep_effects_t imag_to_sleep;

    /* Configuration */
    sleep_imagination_config_t config;

    /* State tracking */
    sleep_stage_t current_stage;
    sleep_stage_t previous_stage;
    uint64_t stage_entry_time_ms;
    uint32_t dream_scenarios[SLEEP_IMAG_MAX_DREAM_SCENARIOS];
    uint32_t num_dream_scenarios;

    /* REM tracking */
    bool in_rem_period;
    uint64_t rem_start_time_ms;
    float accumulated_rem_time_ms;

    /* Statistics */
    sleep_imagination_stats_t stats;

    /* Timing */
    uint64_t last_update_time_ms;
} sleep_imagination_bridge_t;

/*=============================================================================
 * STAGE DESCRIPTOR ACCESS
 *===========================================================================*/

/**
 * @brief Get descriptor for a sleep stage
 *
 * @param stage Sleep stage
 * @return Pointer to stage descriptor (never NULL)
 */
const sleep_stage_descriptor_t* sleep_imagination_get_stage_descriptor(sleep_stage_t stage);

/**
 * @brief Get stage name string
 *
 * @param stage Sleep stage
 * @return Stage name string
 */
const char* sleep_imagination_stage_name(sleep_stage_t stage);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sleep_imagination_default_config(sleep_imagination_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int sleep_imagination_validate_config(const sleep_imagination_config_t* config);

/**
 * @brief Create bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on error
 */
sleep_imagination_bridge_t* sleep_imagination_bridge_create(
    const sleep_imagination_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void sleep_imagination_bridge_destroy(sleep_imagination_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * Clears effects and dream tracking, keeps connections and config.
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int sleep_imagination_reset(sleep_imagination_bridge_t* bridge);

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

/**
 * @brief Connect sleep system
 *
 * @param bridge Bridge
 * @param sleep Sleep system to connect
 * @return 0 on success, -1 on error
 */
int sleep_imagination_connect_sleep(
    sleep_imagination_bridge_t* bridge,
    struct sleep_system* sleep);

/**
 * @brief Connect imagination engine
 *
 * @param bridge Bridge
 * @param imagination Imagination engine to connect
 * @return 0 on success, -1 on error
 */
int sleep_imagination_connect_imagination(
    sleep_imagination_bridge_t* bridge,
    struct imagination_engine* imagination);

/**
 * @brief Disconnect sleep system
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_disconnect_sleep(sleep_imagination_bridge_t* bridge);

/**
 * @brief Disconnect imagination
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_disconnect_imagination(sleep_imagination_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge
 * @return true if both systems connected
 */
bool sleep_imagination_is_connected(const sleep_imagination_bridge_t* bridge);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Main update function
 *
 * Computes and applies effects in both directions.
 *
 * @param bridge Bridge
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int sleep_imagination_update(
    sleep_imagination_bridge_t* bridge,
    float delta_time_ms);

/**
 * @brief Compute sleep -> imagination effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_compute_sleep_effects(sleep_imagination_bridge_t* bridge);

/**
 * @brief Compute imagination -> sleep effects only
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_compute_imag_effects(sleep_imagination_bridge_t* bridge);

/**
 * @brief Apply all computed effects to connected systems
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_apply_effects(sleep_imagination_bridge_t* bridge);

/*=============================================================================
 * SLEEP STAGE API
 *===========================================================================*/

/**
 * @brief Set current sleep stage
 *
 * Triggers stage transition and updates imagination parameters.
 *
 * @param bridge Bridge
 * @param stage New sleep stage
 * @return 0 on success, -1 on error
 */
int sleep_imagination_set_sleep_stage(
    sleep_imagination_bridge_t* bridge,
    sleep_stage_t stage);

/**
 * @brief Get current sleep stage
 *
 * @param bridge Bridge
 * @return Current sleep stage
 */
sleep_stage_t sleep_imagination_get_sleep_stage(
    const sleep_imagination_bridge_t* bridge);

/**
 * @brief Get time in current stage
 *
 * @param bridge Bridge
 * @return Time in current stage (milliseconds)
 */
uint64_t sleep_imagination_get_stage_duration(
    const sleep_imagination_bridge_t* bridge);

/*=============================================================================
 * DREAM GENERATION API
 *===========================================================================*/

/**
 * @brief Initiate dream scenario during REM
 *
 * Starts a new dream scenario with given emotional seed.
 *
 * @param bridge Bridge
 * @param emotional_seed Emotional content seed (can be NULL)
 * @param goal Dream goal/theme (can be NULL for spontaneous)
 * @return Scenario ID on success, 0 on failure
 */
uint32_t sleep_imagination_start_dream(
    sleep_imagination_bridge_t* bridge,
    const nimcp_tensor_t* emotional_seed,
    struct imagination_goal* goal);

/**
 * @brief End current dream scenario
 *
 * @param bridge Bridge
 * @param encode_as_memory Whether to encode dream content as memory
 * @return 0 on success, -1 on error
 */
int sleep_imagination_end_dream(
    sleep_imagination_bridge_t* bridge,
    bool encode_as_memory);

/**
 * @brief Trigger memory replay integration
 *
 * Requests hippocampal replay to feed into dream content.
 *
 * @param bridge Bridge
 * @param memory_cue Cue for memory selection (can be NULL for random)
 * @return 0 on success, -1 on error
 */
int sleep_imagination_trigger_replay(
    sleep_imagination_bridge_t* bridge,
    const nimcp_tensor_t* memory_cue);

/**
 * @brief Interrupt nightmare
 *
 * Interrupts a nightmare scenario and transitions to calmer content.
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error (or no nightmare active)
 */
int sleep_imagination_interrupt_nightmare(sleep_imagination_bridge_t* bridge);

/*=============================================================================
 * EFFECTS ACCESS API
 *===========================================================================*/

/**
 * @brief Get current sleep effects on imagination
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int sleep_imagination_get_sleep_effects(
    const sleep_imagination_bridge_t* bridge,
    sleep_to_imagination_effects_t* effects);

/**
 * @brief Get current imagination effects on sleep
 *
 * @param bridge Bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int sleep_imagination_get_imag_effects(
    const sleep_imagination_bridge_t* bridge,
    imagination_to_sleep_effects_t* effects);

/*=============================================================================
 * QUERY API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sleep_imagination_get_stats(
    const sleep_imagination_bridge_t* bridge,
    sleep_imagination_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_reset_stats(sleep_imagination_bridge_t* bridge);

/**
 * @brief Get number of active dream scenarios
 *
 * @param bridge Bridge
 * @return Number of active scenarios
 */
uint32_t sleep_imagination_get_dream_count(const sleep_imagination_bridge_t* bridge);

/**
 * @brief Check if currently in REM period
 *
 * @param bridge Bridge
 * @return true if in REM
 */
bool sleep_imagination_is_rem(const sleep_imagination_bridge_t* bridge);

/**
 * @brief Get accumulated REM time
 *
 * @param bridge Bridge
 * @return Total REM time in milliseconds
 */
float sleep_imagination_get_rem_time(const sleep_imagination_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_connect_bio_async(sleep_imagination_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int sleep_imagination_disconnect_bio_async(sleep_imagination_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool sleep_imagination_is_bio_async_connected(const sleep_imagination_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge
 * @return Number of messages processed
 */
int sleep_imagination_process_messages(sleep_imagination_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_IMAGINATION_BRIDGE_H */
