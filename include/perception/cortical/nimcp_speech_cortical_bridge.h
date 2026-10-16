/**
 * @file nimcp_speech_cortical_bridge.h
 * @brief Speech-Cortical Bridge - Connects speech cortex with cortical columns
 *
 * WHAT: Integration layer that connects speech_cortex perception module with
 *       cortical column processing (phoneme hypercolumns, tonotopic maps,
 *       cortical immune system).
 * WHY:  Provides biologically-realistic auditory cortex processing by replacing
 *       speech_cortex's internal phoneme detection with proper cortical column
 *       organization.
 * HOW:  Routes phoneme features through tonotopic mapping to phoneme hypercolumns,
 *       applies cortical immune modulation, and uses bio-async messaging.
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    SPEECH-CORTICAL BRIDGE                               │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  speech_cortex_t                phoneme_hypercolumn_t                   │
 * │  ┌─────────────┐               ┌───────────────────────┐               │
 * │  │ Audio Input │──────────────▶│ Phoneme Feature Bank  │               │
 * │  │ STG Filters │    Tono-      │ Phoneme Columns       │               │
 * │  │ Attention   │    topic      │ Feature Organization  │               │
 * │  └─────────────┘    Mapping    └───────────────────────┘               │
 * │        │                               │                                │
 * │        │                               ▼                                │
 * │        │           ┌───────────────────────────────┐                   │
 * │        └──────────▶│ topographic_map_t (Tonotopic) │                   │
 * │                    │ Frequency mapping             │                   │
 * │                    │ Cochlear magnification        │                   │
 * │                    └───────────────────────────────┘                   │
 * │                                │                                        │
 * │                                ▼                                        │
 * │                    ┌───────────────────────────────┐                   │
 * │                    │ cortical_immune_system_t       │                   │
 * │                    │ Microglial surveillance        │                   │
 * │                    │ Inflammation modulation        │                   │
 * │                    └───────────────────────────────┘                   │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL BASIS:
 * - Auditory cortex has columnar organization for phoneme selectivity
 * - Tonotopic mapping provides frequency correspondence with cochlea
 * - Cortical hypercolumns contain neurons tuned to phoneme features
 * - Microglial surveillance monitors cortical health and modulates processing
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SPEECH_CORTICAL_BRIDGE_H
#define NIMCP_SPEECH_CORTICAL_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Perception modules */
#include "perception/nimcp_speech_cortex.h"

/* Cortical column modules */
#include "core/cortical_columns/nimcp_feature_hypercolumns.h"
#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "core/cortical_columns/nimcp_cortical_immune.h"

/* Bio-async communication */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Memory management */
#include "utils/memory/nimcp_unified_memory.h"

/* Logging */
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of phoneme hypercolumns */
#define SPEECH_CORTICAL_MAX_HYPERCOLUMNS 256

/** Maximum number of tonotopic positions */
#define SPEECH_CORTICAL_MAX_POSITIONS 1024

/** Default number of phonemes per hypercolumn */
#define SPEECH_CORTICAL_DEFAULT_PHONEMES 44

/** Default formant frequency range (Hz) */
#define SPEECH_CORTICAL_DEFAULT_FORMANT_MIN 200.0f
#define SPEECH_CORTICAL_DEFAULT_FORMANT_MAX 3500.0f

/** Default tuning width for phoneme features */
#define SPEECH_CORTICAL_DEFAULT_TUNING_WIDTH 0.15f

/** Default immune inflammation modulation factor */
#define SPEECH_CORTICAL_DEFAULT_IMMUNE_FACTOR 1.0f

/** Bio-async module ID for speech-cortical bridge */
#define BIO_MODULE_SPEECH_CORTICAL 0x0612

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Processing mode for the speech-cortical bridge
 */
typedef enum {
    SPEECH_CORTICAL_MODE_DIRECT = 0,     /**< Direct phoneme detection (faster) */
    SPEECH_CORTICAL_MODE_HYPERCOLUMN,    /**< Full hypercolumn processing (more accurate) */
    SPEECH_CORTICAL_MODE_TONOTOPIC,      /**< Include tonotopic mapping */
    SPEECH_CORTICAL_MODE_FULL            /**< All features enabled */
} speech_cortical_mode_t;

/**
 * @brief State of the speech-cortical bridge
 */
typedef enum {
    SPEECH_CORTICAL_STATE_UNINITIALIZED = 0,
    SPEECH_CORTICAL_STATE_READY,
    SPEECH_CORTICAL_STATE_PROCESSING,
    SPEECH_CORTICAL_STATE_ERROR
} speech_cortical_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for speech-cortical bridge
 */
typedef struct {
    /** Number of phoneme hypercolumns to create */
    uint32_t num_hypercolumns;

    /** Number of phonemes per hypercolumn (typically 44 for English) */
    uint32_t phonemes_per_hypercolumn;

    /** Minimum formant frequency for mapping (Hz) */
    float min_formant_freq;

    /** Maximum formant frequency for mapping (Hz) */
    float max_formant_freq;

    /** Phoneme feature tuning width */
    float tuning_width;

    /** Processing mode */
    speech_cortical_mode_t mode;

    /** Enable tonotopic mapping */
    bool enable_tonotopic_mapping;

    /** Enable cortical immune integration */
    bool enable_cortical_immune;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Auditory field coverage in octaves */
    float auditory_field_octaves;

    /** Cochlear magnification factor */
    float cochlear_magnification;

    /** Immune modulation factor (0.0 = no effect, 1.0 = full effect) */
    float immune_modulation_factor;

    /** Use unified memory manager for allocations */
    bool use_umm;
} speech_cortical_config_t;

/**
 * @brief Statistics for speech-cortical bridge
 */
typedef struct {
    /** Total audio frames processed */
    uint64_t frames_processed;

    /** Total hypercolumn activations */
    uint64_t hypercolumn_activations;

    /** Total bio-async messages sent */
    uint64_t bio_messages_sent;

    /** Total bio-async messages received */
    uint64_t bio_messages_received;

    /** Average processing time in ms */
    float avg_processing_time_ms;

    /** Peak phoneme response */
    float peak_phoneme_response;

    /** Current dominant phoneme */
    phoneme_t current_dominant_phoneme;

    /** Current immune modulation level */
    float current_immune_modulation;

    /** Number of active hypercolumns */
    uint32_t active_hypercolumns;
} speech_cortical_stats_t;

/**
 * @brief Result of phoneme analysis
 */
typedef struct {
    /** Dominant phoneme */
    phoneme_t dominant_phoneme;

    /** Phoneme selectivity index (0-1) */
    float selectivity_index;

    /** Phoneme distribution [num_phonemes] */
    float* phoneme_responses;

    /** Number of phonemes */
    uint32_t num_phonemes;

    /** Confidence of phoneme detection (0-1) */
    float confidence;

    /** Tonotopic position x (if mapped) */
    float tono_x;

    /** Tonotopic position y (if mapped) */
    float tono_y;

    /** Formant frequencies (F1, F2, F3, F4) */
    float formants[SPEECH_NUM_FORMANTS];
} speech_cortical_phoneme_result_t;

/**
 * @brief Opaque handle to speech-cortical bridge instance
 */
typedef struct speech_cortical_bridge speech_cortical_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize default configuration
 *
 * WHAT: Sets up default configuration for speech-cortical bridge.
 * WHY:  Provide sensible defaults for typical auditory cortex processing.
 * HOW:  Fills config structure with biologically-plausible values.
 *
 * @param config Configuration structure to initialize
 */
void speech_cortical_default_config(speech_cortical_config_t* config);

/**
 * @brief Create speech-cortical bridge
 *
 * WHAT: Allocates and initializes the speech-cortical bridge.
 * WHY:  Connects speech perception with cortical column processing.
 * HOW:  Creates hypercolumns, tonotopic map, and immune connections.
 *
 * @param config Configuration (NULL for defaults)
 * @param speech_cortex Speech cortex instance to connect (may be NULL)
 * @return Bridge instance, or NULL on failure
 *
 * @note Caller must free with speech_cortical_bridge_destroy()
 */
speech_cortical_bridge_t* speech_cortical_bridge_create(
    const speech_cortical_config_t* config,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Destroy speech-cortical bridge
 *
 * WHAT: Frees all resources associated with the bridge.
 * WHY:  Clean up memory and connections.
 * HOW:  Destroys hypercolumns, map, and disconnects immune.
 *
 * @param bridge Bridge to destroy (may be NULL)
 */
void speech_cortical_bridge_destroy(speech_cortical_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect to speech cortex
 *
 * WHAT: Associates a speech cortex instance with this bridge.
 * WHY:  Enable bidirectional communication between perception and columns.
 * HOW:  Stores reference and sets up callbacks.
 *
 * @param bridge Speech-cortical bridge
 * @param speech_cortex Speech cortex to connect
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_connect_speech_cortex(
    speech_cortical_bridge_t* bridge,
    speech_cortex_t* speech_cortex
);

/**
 * @brief Connect to cortical immune system
 *
 * WHAT: Associates cortical immune system for modulation.
 * WHY:  Enable inflammation-based processing modulation.
 * HOW:  Stores reference and registers for cytokine updates.
 *
 * @param bridge Speech-cortical bridge
 * @param immune Cortical immune system to connect
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_connect_immune(
    speech_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers bridge with bio-async messaging system.
 * WHY:  Enable inter-module communication.
 * HOW:  Registers module and sets up message handlers.
 *
 * @param bridge Speech-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_connect_bio_async(speech_cortical_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters bridge from bio-async messaging system.
 * WHY:  Clean disconnection before shutdown.
 * HOW:  Unregisters module ID.
 *
 * @param bridge Speech-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_disconnect_bio_async(speech_cortical_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Speech-cortical bridge
 * @return true if connected, false otherwise
 */
bool speech_cortical_is_bio_async_connected(const speech_cortical_bridge_t* bridge);

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

/**
 * @brief Process audio frame through speech-cortical bridge
 *
 * WHAT: Routes audio through tonotopic mapping and phoneme hypercolumns.
 * WHY:  Biologically-realistic auditory cortex processing with proper columnar organization.
 * HOW:  Maps audio to cortical positions, processes through hypercolumns.
 *
 * @param bridge Speech-cortical bridge
 * @param audio_data Audio samples (mono, normalized)
 * @param num_samples Number of audio samples
 * @param result Output phoneme analysis result
 * @return 0 on success, non-zero on failure
 *
 * @note Result's phoneme_responses is allocated by this function.
 *       Caller must free with speech_cortical_free_result()
 */
int speech_cortical_process(
    speech_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    speech_cortical_phoneme_result_t* result
);

/**
 * @brief Process audio segment at specific tonotopic position
 *
 * WHAT: Processes a single audio segment through one hypercolumn.
 * WHY:  Efficient processing for attention-guided audition.
 * HOW:  Maps position to hypercolumn and processes.
 *
 * @param bridge Speech-cortical bridge
 * @param audio_segment Audio segment data
 * @param num_samples Number of samples in segment
 * @param tono_x Tonotopic x position (frequency in Hz)
 * @param tono_y Tonotopic y position
 * @param result Output phoneme analysis result
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_process_segment(
    speech_cortical_bridge_t* bridge,
    const float* audio_segment,
    uint32_t num_samples,
    float tono_x,
    float tono_y,
    speech_cortical_phoneme_result_t* result
);

/**
 * @brief Free phoneme result resources
 *
 * WHAT: Frees memory allocated in phoneme result.
 * WHY:  Clean up after processing.
 * HOW:  Frees phoneme_responses array.
 *
 * @param result Result to free
 */
void speech_cortical_free_result(speech_cortical_phoneme_result_t* result);

/**
 * @brief Get phoneme map for entire audio frame
 *
 * WHAT: Computes dominant phoneme for each position in audio.
 * WHY:  Generate phoneme-based feature map.
 * HOW:  Processes all positions through hypercolumns.
 *
 * @param bridge Speech-cortical bridge
 * @param audio_data Input audio
 * @param num_samples Number of samples
 * @param phoneme_map Output phoneme map [num_positions]
 * @param selectivity_map Optional selectivity map [num_positions]
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_get_phoneme_map(
    speech_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    phoneme_t* phoneme_map,
    float* selectivity_map
);

/* ============================================================================
 * Hypercolumn Functions
 * ============================================================================ */

/**
 * @brief Get hypercolumn at tonotopic position
 *
 * WHAT: Retrieves hypercolumn responsible for given frequency position.
 * WHY:  Access individual hypercolumn for analysis.
 * HOW:  Uses tonotopic mapping to find hypercolumn index.
 *
 * @param bridge Speech-cortical bridge
 * @param tono_x Tonotopic x position (frequency)
 * @param tono_y Tonotopic y position
 * @return Hypercolumn pointer, or NULL if not found
 */
const feature_hypercolumn_t* speech_cortical_get_hypercolumn(
    const speech_cortical_bridge_t* bridge,
    float tono_x,
    float tono_y
);

/**
 * @brief Get hypercolumn by index
 *
 * WHAT: Retrieves hypercolumn by array index.
 * WHY:  Direct access for iteration.
 * HOW:  Returns pointer from internal array.
 *
 * @param bridge Speech-cortical bridge
 * @param index Hypercolumn index
 * @return Hypercolumn pointer, or NULL if out of bounds
 */
const feature_hypercolumn_t* speech_cortical_get_hypercolumn_by_index(
    const speech_cortical_bridge_t* bridge,
    uint32_t index
);

/**
 * @brief Get number of hypercolumns
 *
 * @param bridge Speech-cortical bridge
 * @return Number of hypercolumns, or 0 if bridge is NULL
 */
uint32_t speech_cortical_get_num_hypercolumns(const speech_cortical_bridge_t* bridge);

/* ============================================================================
 * Immune Modulation Functions
 * ============================================================================ */

/**
 * @brief Update immune modulation
 *
 * WHAT: Updates processing gains based on immune state.
 * WHY:  Apply inflammation effects to phoneme processing.
 * HOW:  Queries immune system and adjusts gains.
 *
 * @param bridge Speech-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_update_immune_modulation(speech_cortical_bridge_t* bridge);

/**
 * @brief Set immune modulation factor
 *
 * WHAT: Manually sets immune modulation level.
 * WHY:  Allow direct control for testing or simulation.
 * HOW:  Sets internal modulation factor (0.0-1.0).
 *
 * @param bridge Speech-cortical bridge
 * @param factor Modulation factor (0.0 = no effect, 1.0 = full effect)
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_set_immune_factor(
    speech_cortical_bridge_t* bridge,
    float factor
);

/**
 * @brief Get current immune modulation factor
 *
 * @param bridge Speech-cortical bridge
 * @return Current modulation factor, or 0.0 if bridge is NULL
 */
float speech_cortical_get_immune_factor(const speech_cortical_bridge_t* bridge);

/* ============================================================================
 * Statistics and State Functions
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves processing and performance statistics.
 * WHY:  Monitoring and debugging.
 * HOW:  Copies internal stats to output structure.
 *
 * @param bridge Speech-cortical bridge
 * @param stats Output statistics structure
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_get_stats(
    const speech_cortical_bridge_t* bridge,
    speech_cortical_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clears all statistics counters.
 * WHY:  Fresh start for benchmarking.
 * HOW:  Zeros stats structure.
 *
 * @param bridge Speech-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_reset_stats(speech_cortical_bridge_t* bridge);

/**
 * @brief Get bridge state
 *
 * @param bridge Speech-cortical bridge
 * @return Current state, or UNINITIALIZED if bridge is NULL
 */
speech_cortical_state_t speech_cortical_get_state(
    const speech_cortical_bridge_t* bridge
);

/**
 * @brief Get tonotopic map
 *
 * WHAT: Returns the internal tonotopic map.
 * WHY:  Allow external access for analysis.
 * HOW:  Returns pointer to internal map.
 *
 * @param bridge Speech-cortical bridge
 * @return Tonotopic map, or NULL if not enabled
 */
const topographic_map_t* speech_cortical_get_tonotopic_map(
    const speech_cortical_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Message Handling
 * ============================================================================ */

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Handles incoming messages from other modules.
 * WHY:  Respond to attention shifts, immune updates, etc.
 * HOW:  Processes inbox and invokes handlers.
 *
 * @param bridge Speech-cortical bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t speech_cortical_process_bio_messages(
    speech_cortical_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Broadcast phoneme detection result
 *
 * WHAT: Sends phoneme detection to interested modules.
 * WHY:  Notify downstream modules of speech features.
 * HOW:  Sends BIO_MSG_CORTICAL_PHONEME_DETECTED.
 *
 * @param bridge Speech-cortical bridge
 * @param result Phoneme result to broadcast
 * @return 0 on success, non-zero on failure
 */
int speech_cortical_broadcast_phoneme(
    speech_cortical_bridge_t* bridge,
    const speech_cortical_phoneme_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPEECH_CORTICAL_BRIDGE_H */
