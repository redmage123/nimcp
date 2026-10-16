/**
 * @file nimcp_parietal_phonological_wm.h
 * @brief Phonological Working Memory Module (Supramarginal Gyrus, BA40)
 * @version 1.0.0
 * @date 2025-01-31
 *
 * WHAT: Phonological loop implementation with encoding, rehearsal, decay,
 *       and pattern-completion retrieval for linguistic working memory
 *
 * WHY:  The supramarginal gyrus is critical for phonological working memory,
 *       implementing Baddeley's phonological loop model
 *
 * BIOLOGICAL BASIS:
 * - Phonological store: Holds acoustic/phonological traces (~2s decay)
 * - Articulatory rehearsal: Subvocal loop maintains traces
 * - Miller's law: 7±2 item capacity
 * - Phonological similarity effect: Similar phonemes interfere
 * - Word length effect: Longer words harder to maintain
 *
 * THETA-GAMMA INTEGRATION:
 * - Theta phase (0-90°): Encoding window
 * - Theta phase (180-270°): Retrieval window
 * - Gamma bursts: Phoneme feature binding
 *
 * USAGE:
 * ```c
 * phonological_wm_t* pwm = phonological_wm_create();
 *
 * // Encode a word
 * phonological_trace_t trace;
 * phonological_wm_encode(pwm, "hello", &trace);
 *
 * // Trigger rehearsal to maintain
 * phonological_wm_rehearse(pwm);
 *
 * // Retrieve by partial cue
 * phonological_trace_t retrieved;
 * phonological_wm_retrieve(pwm, "hel", &retrieved);
 *
 * phonological_wm_destroy(pwm);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_PHONOLOGICAL_WM_H
#define NIMCP_PARIETAL_PHONOLOGICAL_WM_H

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Default trace decay time constant (ms) */
#define PHONOLOGICAL_WM_DEFAULT_DECAY_MS        2000

/** Default rehearsal rate (items per second) */
#define PHONOLOGICAL_WM_DEFAULT_REHEARSAL_RATE  4.0f

/** Default buffer capacity (Miller's 7±2) */
#define PHONOLOGICAL_WM_DEFAULT_CAPACITY        7

/** Phonological similarity threshold for interference */
#define PHONOLOGICAL_WM_SIMILARITY_THRESHOLD    0.7f

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for phonological working memory */
typedef struct phonological_wm phonological_wm_t;

/**
 * @brief Phonological working memory configuration
 */
typedef struct {
    /* Capacity & timing */
    uint32_t buffer_capacity;       /**< Maximum items (default: 7) */
    uint32_t decay_time_ms;         /**< Trace decay time (default: 2000ms) */
    float rehearsal_rate;           /**< Items per second for rehearsal (default: 4.0) */

    /* Effects */
    bool enable_similarity_effect;  /**< Phonological similarity interference (default: true) */
    bool enable_word_length_effect; /**< Longer words harder to maintain (default: true) */
    bool enable_recency_effect;     /**< Recent items recalled better (default: true) */

    /* Feature flags */
    bool enable_automatic_rehearsal; /**< Auto-trigger rehearsal when activated (default: false) */
    bool enable_bio_async;          /**< Enable bio-async messaging (default: false) */
    bool enable_mesh_participation; /**< Participate in linguistics mesh (default: true) */

    /* Theta-gamma integration */
    bool enable_theta_gating;       /**< Theta phase gates encoding/retrieval (default: false) */

    /* Modulation */
    float inflammation_sensitivity; /**< Immune modulation factor (0-1) */
    float fatigue_sensitivity;      /**< Fatigue modulation factor (0-1) */
    float arousal_sensitivity;      /**< Arousal affects capacity (0-1) */
} phonological_wm_config_t;

/**
 * @brief Phoneme feature structure
 *
 * Distinctive features for phoneme representation
 */
typedef struct {
    bool is_voiced;                 /**< Voiced vs voiceless */
    bool is_nasal;                  /**< Nasal sound */
    bool is_stop;                   /**< Stop consonant */
    bool is_fricative;              /**< Fricative consonant */
    bool is_vowel;                  /**< Vowel */
    uint8_t place;                  /**< Place of articulation (0-7) */
    uint8_t manner;                 /**< Manner of articulation (0-7) */
    uint8_t height;                 /**< Vowel height (0-3) */
    uint8_t frontness;              /**< Vowel frontness (0-2) */
} phoneme_features_t;

/**
 * @brief Phonological similarity result
 */
typedef struct {
    float similarity;               /**< Similarity score [0,1] */
    uint32_t feature_overlap;       /**< Number of shared features */
    uint32_t total_features;        /**< Total features compared */
    bool will_interfere;            /**< Above interference threshold */
} similarity_result_t;

/**
 * @brief Buffer state snapshot
 */
typedef struct {
    uint32_t item_count;            /**< Current items in buffer */
    uint32_t capacity;              /**< Maximum capacity */
    float total_activation;         /**< Sum of all activations */
    float avg_activation;           /**< Average activation */
    bool is_rehearsing;             /**< Currently in rehearsal */
    uint32_t rehearsal_position;    /**< Current rehearsal index */
    float oldest_activation;        /**< Lowest activation (next to forget) */
} buffer_state_t;

/**
 * @brief Phonological working memory statistics
 */
typedef struct {
    uint64_t words_encoded;         /**< Total words encoded */
    uint64_t words_retrieved;       /**< Successful retrievals */
    uint64_t words_forgotten;       /**< Items lost to decay */
    uint64_t rehearsals_triggered;  /**< Rehearsal cycles */
    uint64_t overflow_events;       /**< Buffer overflow events */

    float avg_encoding_time_us;     /**< Average encoding time */
    float avg_retrieval_time_us;    /**< Average retrieval time */
    float avg_trace_lifetime_ms;    /**< Average trace duration before forget */

    uint32_t peak_occupancy;        /**< Highest buffer utilization */
    float similarity_interference_rate; /**< Rate of similarity-based errors */
} phonological_wm_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create phonological working memory with default configuration
 *
 * @return Handle or NULL on error
 */
phonological_wm_t* phonological_wm_create(void);

/**
 * @brief Create phonological working memory with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
phonological_wm_t* phonological_wm_create_custom(const phonological_wm_config_t* config);

/**
 * @brief Destroy phonological working memory
 *
 * @param pwm Handle (NULL safe)
 */
void phonological_wm_destroy(phonological_wm_t* pwm);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
phonological_wm_config_t phonological_wm_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool phonological_wm_validate_config(const phonological_wm_config_t* config);

/* ============================================================================
 * ENCODING API
 * ============================================================================ */

/**
 * @brief Encode a word into phonological trace
 *
 * Converts a word to its phonological representation and stores
 * in the phonological buffer. If buffer is full, oldest item
 * is displaced.
 *
 * @param pwm Phonological WM handle
 * @param word Word to encode
 * @param out Output trace (can be NULL if not needed)
 * @return 0 on success, error code on failure
 */
int phonological_wm_encode(
    phonological_wm_t* pwm,
    const char* word,
    phonological_trace_t* out
);

/**
 * @brief Encode word with explicit phoneme sequence
 *
 * For when phoneme sequence is already known (e.g., from speech
 * recognition).
 *
 * @param pwm Phonological WM handle
 * @param word Original word string
 * @param phonemes Phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param out Output trace
 * @return 0 on success
 */
int phonological_wm_encode_phonemes(
    phonological_wm_t* pwm,
    const char* word,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    phonological_trace_t* out
);

/**
 * @brief Convert word to phoneme sequence
 *
 * Letter-to-phoneme conversion for English words.
 *
 * @param pwm Phonological WM handle
 * @param word Word to convert
 * @param phonemes Output phoneme array
 * @param max_phonemes Maximum array size
 * @param num_phonemes Output number of phonemes
 * @return 0 on success
 */
int phonological_wm_word_to_phonemes(
    const phonological_wm_t* pwm,
    const char* word,
    phoneme_t* phonemes,
    uint32_t max_phonemes,
    uint32_t* num_phonemes
);

/* ============================================================================
 * REHEARSAL API
 * ============================================================================ */

/**
 * @brief Trigger subvocal rehearsal
 *
 * Refreshes all traces in the buffer by cycling through them
 * at the rehearsal rate. Maintains activation levels.
 *
 * @param pwm Phonological WM handle
 * @return 0 on success
 */
int phonological_wm_rehearse(phonological_wm_t* pwm);

/**
 * @brief Rehearse specific item
 *
 * Refreshes a single trace by index.
 *
 * @param pwm Phonological WM handle
 * @param index Buffer index to rehearse
 * @return 0 on success
 */
int phonological_wm_rehearse_item(
    phonological_wm_t* pwm,
    uint32_t index
);

/**
 * @brief Check if rehearsal is needed
 *
 * Returns true if any trace is about to decay below threshold.
 *
 * @param pwm Phonological WM handle
 * @return true if rehearsal recommended
 */
bool phonological_wm_needs_rehearsal(const phonological_wm_t* pwm);

/**
 * @brief Start automatic rehearsal mode
 *
 * @param pwm Phonological WM handle
 * @return 0 on success
 */
int phonological_wm_start_rehearsal(phonological_wm_t* pwm);

/**
 * @brief Stop automatic rehearsal mode
 *
 * @param pwm Phonological WM handle
 * @return 0 on success
 */
int phonological_wm_stop_rehearsal(phonological_wm_t* pwm);

/* ============================================================================
 * DECAY & UPDATE API
 * ============================================================================ */

/**
 * @brief Update all traces for time passage
 *
 * Applies decay to all traces based on elapsed time.
 * Should be called periodically.
 *
 * @param pwm Phonological WM handle
 * @param elapsed_ms Elapsed time in milliseconds
 * @return Number of traces that decayed to zero (forgotten)
 */
int phonological_wm_update(
    phonological_wm_t* pwm,
    uint32_t elapsed_ms
);

/**
 * @brief Get trace activation level
 *
 * @param pwm Phonological WM handle
 * @param index Buffer index
 * @param activation Output activation [0,1]
 * @return 0 on success
 */
int phonological_wm_get_activation(
    const phonological_wm_t* pwm,
    uint32_t index,
    float* activation
);

/**
 * @brief Clear all traces
 *
 * @param pwm Phonological WM handle
 */
void phonological_wm_clear(phonological_wm_t* pwm);

/* ============================================================================
 * RETRIEVAL API
 * ============================================================================ */

/**
 * @brief Retrieve trace by partial cue (pattern completion)
 *
 * Finds the best matching trace for a partial phonological cue.
 *
 * @param pwm Phonological WM handle
 * @param cue Partial word or phoneme sequence
 * @param out Output retrieved trace
 * @return 0 on success, error if no match
 */
int phonological_wm_retrieve(
    phonological_wm_t* pwm,
    const char* cue,
    phonological_trace_t* out
);

/**
 * @brief Retrieve trace by index
 *
 * @param pwm Phonological WM handle
 * @param index Buffer index
 * @param out Output trace
 * @return 0 on success
 */
int phonological_wm_get_trace(
    const phonological_wm_t* pwm,
    uint32_t index,
    phonological_trace_t* out
);

/**
 * @brief Find trace by word
 *
 * @param pwm Phonological WM handle
 * @param word Word to find
 * @param index Output buffer index
 * @return 0 if found, error if not
 */
int phonological_wm_find(
    const phonological_wm_t* pwm,
    const char* word,
    uint32_t* index
);

/**
 * @brief Check if word is in buffer
 *
 * @param pwm Phonological WM handle
 * @param word Word to check
 * @return true if present
 */
bool phonological_wm_contains(
    const phonological_wm_t* pwm,
    const char* word
);

/* ============================================================================
 * SIMILARITY API
 * ============================================================================ */

/**
 * @brief Compute phonological similarity between words
 *
 * Uses distinctive feature comparison for phoneme similarity.
 *
 * @param pwm Phonological WM handle
 * @param word_a First word
 * @param word_b Second word
 * @param result Output similarity result
 * @return 0 on success
 */
int phonological_wm_similarity(
    const phonological_wm_t* pwm,
    const char* word_a,
    const char* word_b,
    similarity_result_t* result
);

/**
 * @brief Compute phoneme similarity
 *
 * @param pwm Phonological WM handle
 * @param phoneme_a First phoneme
 * @param phoneme_b Second phoneme
 * @return Similarity [0,1]
 */
float phonological_wm_phoneme_similarity(
    const phonological_wm_t* pwm,
    phoneme_t phoneme_a,
    phoneme_t phoneme_b
);

/**
 * @brief Get phoneme features
 *
 * @param pwm Phonological WM handle
 * @param phoneme Phoneme to analyze
 * @param features Output features
 * @return 0 on success
 */
int phonological_wm_get_phoneme_features(
    const phonological_wm_t* pwm,
    phoneme_t phoneme,
    phoneme_features_t* features
);

/* ============================================================================
 * BUFFER STATE API
 * ============================================================================ */

/**
 * @brief Get current buffer state
 *
 * @param pwm Phonological WM handle
 * @param state Output state
 * @return 0 on success
 */
int phonological_wm_get_state(
    const phonological_wm_t* pwm,
    buffer_state_t* state
);

/**
 * @brief Get current item count
 *
 * @param pwm Phonological WM handle
 * @return Number of items in buffer
 */
uint32_t phonological_wm_count(const phonological_wm_t* pwm);

/**
 * @brief Get buffer capacity
 *
 * @param pwm Phonological WM handle
 * @return Maximum capacity
 */
uint32_t phonological_wm_capacity(const phonological_wm_t* pwm);

/**
 * @brief Check if buffer is full
 *
 * @param pwm Phonological WM handle
 * @return true if at capacity
 */
bool phonological_wm_is_full(const phonological_wm_t* pwm);

/* ============================================================================
 * MESH INTEGRATION API
 * ============================================================================ */

/**
 * @brief Process mesh request and produce belief
 *
 * @param pwm Phonological WM handle
 * @param request Mesh request
 * @param belief Output belief with precision
 * @return 0 on success
 */
int phonological_wm_mesh_process(
    phonological_wm_t* pwm,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
);

/**
 * @brief Update belief based on neighbor beliefs
 *
 * @param pwm Phonological WM handle
 * @param neighbor_beliefs Beliefs from mesh neighbors
 * @param neighbor_count Number of neighbor beliefs
 * @param updated_belief Output updated belief
 * @return 0 on success
 */
int phonological_wm_mesh_update(
    phonological_wm_t* pwm,
    const linguistics_belief_t* neighbor_beliefs,
    uint32_t neighbor_count,
    linguistics_belief_t* updated_belief
);

/**
 * @brief Get current precision
 *
 * @param pwm Phonological WM handle
 * @return Precision Π
 */
float phonological_wm_get_precision(const phonological_wm_t* pwm);

/**
 * @brief Get mesh handler interface
 *
 * @param pwm Phonological WM handle
 * @param handler Output handler struct
 * @return 0 on success
 */
int phonological_wm_get_mesh_handler(
    phonological_wm_t* pwm,
    linguistics_mesh_handler_t* handler
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * High inflammation reduces buffer capacity and increases decay.
 *
 * @param pwm Phonological WM handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int phonological_wm_set_inflammation(
    phonological_wm_t* pwm,
    float level
);

/**
 * @brief Set fatigue level
 *
 * Fatigue increases decay rate and reduces rehearsal effectiveness.
 *
 * @param pwm Phonological WM handle
 * @param level Fatigue level [0,1]
 * @return 0 on success
 */
int phonological_wm_set_fatigue(
    phonological_wm_t* pwm,
    float level
);

/**
 * @brief Set arousal level
 *
 * Optimal arousal improves capacity; extremes reduce it.
 *
 * @param pwm Phonological WM handle
 * @param level Arousal level [0,1]
 * @return 0 on success
 */
int phonological_wm_set_arousal(
    phonological_wm_t* pwm,
    float level
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param pwm Phonological WM handle
 * @param stats Output statistics
 * @return 0 on success
 */
int phonological_wm_get_stats(
    const phonological_wm_t* pwm,
    phonological_wm_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param pwm Phonological WM handle
 */
void phonological_wm_reset_stats(phonological_wm_t* pwm);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* phonological_wm_get_last_error(void);

/* ============================================================================
 * UTILITY API
 * ============================================================================ */

/**
 * @brief Get human-readable phoneme name
 *
 * @param phoneme Phoneme type
 * @return Static string name (IPA symbol)
 */
const char* phonological_wm_phoneme_name(phoneme_t phoneme);

/**
 * @brief Get IPA symbol for phoneme
 *
 * @param phoneme Phoneme type
 * @return IPA symbol string
 */
const char* phonological_wm_phoneme_ipa(phoneme_t phoneme);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_PHONOLOGICAL_WM_H */
