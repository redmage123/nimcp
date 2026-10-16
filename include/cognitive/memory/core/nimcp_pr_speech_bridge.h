//=============================================================================
// nimcp_pr_speech_bridge.h - Prime Resonant Speech Bridge
//=============================================================================
/**
 * @file nimcp_pr_speech_bridge.h
 * @brief Integration bridge between Speech Cortex and Prime Resonant Memory
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional bridge linking speech processing to PR memory encoding
 * WHY:  Enable phonemic patterns to be stored as prime-resonant memories with
 *       prosody-informed quaternion states for emotionally-aware speech recall
 * HOW:  Maps phoneme sequences to prime signatures, prosody to quaternions,
 *       integrates with theta-gamma for speech timing, uses FEP prediction errors
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 *
 *   Speech-Memory Integration Model:
 *   +-----------------------------------------------------------------------+
 *   |  Speech processing naturally integrates with episodic and semantic   |
 *   |  memory systems through multiple neural pathways:                    |
 *   |                                                                       |
 *   |  Phonological Loop (Baddeley 2003):                                   |
 *   |  - Temporary storage of speech-based information                     |
 *   |  - Articulatory rehearsal maintains phonemic representations         |
 *   |  - Capacity ~2 seconds of speech (7+/-2 items)                       |
 *   |                                                                       |
 *   |  Lexical-Semantic Interface:                                          |
 *   |  - Wernicke's area links phoneme sequences to word meanings          |
 *   |  - Bidirectional: comprehension (phoneme->meaning) and naming        |
 *   |  - Tip-of-tongue phenomenon shows partial access states              |
 *   |                                                                       |
 *   |  Emotional Prosody (Ross 1981):                                       |
 *   |  - Right hemisphere processes emotional tone                         |
 *   |  - Pitch contour, stress patterns encode affective content           |
 *   |  - Emotional memories enhanced by prosodic congruence                |
 *   +-----------------------------------------------------------------------+
 *
 *   Phoneme -> Prime Signature Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  Each phoneme maps to a unique prime number (first 44 primes for     |
 *   |  English phonemes). Word signatures are products of phoneme primes   |
 *   |  with position encoding:                                              |
 *   |                                                                       |
 *   |  word_signature = product(phoneme_prime[i] * position_factor[i])     |
 *   |                                                                       |
 *   |  Properties:                                                          |
 *   |  - Unique factorization ensures distinct word signatures             |
 *   |  - Similar words share prime factors (phonological neighbors)        |
 *   |  - Position encoding distinguishes anagrams (e.g., "stop" vs "pots") |
 *   |  - Rhymes share trailing phoneme primes                              |
 *   |                                                                       |
 *   |  Example: "cat" = prime(K) * prime(AE) * prime(T)                    |
 *   |           "hat" = prime(H) * prime(AE) * prime(T)                    |
 *   |           Jaccard similarity high (shared AE, T primes)              |
 *   +-----------------------------------------------------------------------+
 *
 *   Prosody -> Quaternion State Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  Quaternion components encode speech-specific semantic dimensions:    |
 *   |                                                                       |
 *   |  w (consolidation):                                                   |
 *   |    = f(word_frequency, repetition_count)                             |
 *   |    High-frequency words have stronger consolidation                  |
 *   |    Repeated words accumulate strength                                |
 *   |                                                                       |
 *   |  x (emotion):                                                         |
 *   |    = f(prosody_pitch_contour, stress_pattern, speaking_rate)         |
 *   |    Rising pitch = positive/questioning                               |
 *   |    Falling pitch = negative/declarative                              |
 *   |    Stress emphasis = intensity marker                                |
 *   |                                                                       |
 *   |  y (salience):                                                        |
 *   |    = f(word_importance, novelty, emphasis)                           |
 *   |    Content words (nouns, verbs) > function words                     |
 *   |    Novel words > familiar words                                      |
 *   |    Stressed syllables increase salience                              |
 *   |                                                                       |
 *   |  z (accessibility):                                                   |
 *   |    = f(familiarity, phonological_neighborhood_density)               |
 *   |    Common words = high accessibility                                 |
 *   |    Many phonological neighbors = faster retrieval                    |
 *   +-----------------------------------------------------------------------+
 *
 *   Theta-Gamma Coupling for Speech:
 *   +-----------------------------------------------------------------------+
 *   |  Speech naturally segments into theta-aligned units:                  |
 *   |                                                                       |
 *   |  Syllables ~200ms = ~5Hz = theta band                                |
 *   |  Phonemes ~40ms = ~25Hz = gamma band                                 |
 *   |                                                                       |
 *   |  Theta phase gates word boundary detection:                           |
 *   |  - Word onset triggers theta reset                                   |
 *   |  - Phonemes nest within theta as gamma bursts                        |
 *   |  - Memory encoding at theta trough (word complete)                   |
 *   |  - Memory retrieval at theta peak (lexical access)                   |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * =============================================================================
 * - Phoneme processing: ~50us per phoneme
 * - Word signature computation: ~100us (depends on word length)
 * - Quaternion computation from prosody: ~20us
 * - Memory encoding: ~200us per word
 * - Similar word retrieval (top-10): ~500us
 *
 * MEMORY:
 * =============================================================================
 * - pr_speech_bridge_t: ~2KB (fixed overhead + buffers)
 * - Per-word signature: 136 bytes
 * - Phoneme buffer: ~4KB (64 phonemes * 64 bytes each)
 *
 * INTEGRATION:
 * =============================================================================
 * - Speech Cortex: Phoneme detection, formants, prosody, lexical access
 * - Speech FEP Bridge: Prediction errors, word boundaries
 * - Prime Signature: Content-addressable word storage
 * - Quaternion: Semantic state for speech memories
 * - Theta-Gamma: Phase-gated encoding/retrieval
 * - Entanglement: Word association network
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_SPEECH_BRIDGE_H
#define NIMCP_PR_SPEECH_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Core dependencies
#include "perception/nimcp_speech_cortex.h"
#include "perception/nimcp_speech_cortex_fep_bridge.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_theta_gamma.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

/* Forward declaration for health agent (B25 Upgrade) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

//=============================================================================
// Constants
//=============================================================================

/** Number of phonemes in English (IPA subset) */
#define PR_SPEECH_NUM_PHONEMES          44

/** Maximum phonemes in buffer */
#define PR_SPEECH_MAX_PHONEME_BUFFER    64

/** Maximum word length in phonemes */
#define PR_SPEECH_MAX_WORD_PHONEMES     32

/** Maximum cached word signatures */
#define PR_SPEECH_MAX_WORD_SIGNATURES   256

/** Default word boundary PE threshold */
#define PR_SPEECH_WORD_BOUNDARY_PE      0.7f

/** Prosody valence scaling factor */
#define PR_SPEECH_PROSODY_VALENCE_SCALE 0.5f

/** Default salience for content words */
#define PR_SPEECH_CONTENT_WORD_SALIENCE 0.8f

/** Default salience for function words */
#define PR_SPEECH_FUNCTION_WORD_SALIENCE 0.3f

/** Theta frequency for syllable timing (Hz) */
#define PR_SPEECH_THETA_FREQ_HZ         5.0f

/** Gamma frequency for phoneme timing (Hz) */
#define PR_SPEECH_GAMMA_FREQ_HZ         25.0f

/** Epsilon for floating-point comparisons */
#define PR_SPEECH_EPSILON               1e-6f

/** Pi constant */

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Error codes for speech bridge operations
 */
typedef enum {
    PR_SPEECH_SUCCESS = 0,                /**< Operation succeeded */
    PR_SPEECH_ERROR_NULL_POINTER = -1,    /**< NULL pointer argument */
    PR_SPEECH_ERROR_INVALID_CONFIG = -2,  /**< Invalid configuration */
    PR_SPEECH_ERROR_NO_MEMORY = -3,       /**< Memory allocation failed */
    PR_SPEECH_ERROR_NOT_CONNECTED = -4,   /**< Required system not connected */
    PR_SPEECH_ERROR_BUFFER_FULL = -5,     /**< Phoneme buffer full */
    PR_SPEECH_ERROR_INVALID_PHONEME = -6, /**< Invalid phoneme value */
    PR_SPEECH_ERROR_ENCODING_FAILED = -7, /**< Memory encoding failed */
    PR_SPEECH_ERROR_RETRIEVAL_FAILED = -8,/**< Memory retrieval failed */
    PR_SPEECH_ERROR_FEP_FAILED = -9,      /**< FEP update failed */
    PR_SPEECH_ERROR_THETA_GAMMA = -10     /**< Theta-gamma error */
} pr_speech_error_t;

/**
 * @brief Word type classification for salience computation
 */
typedef enum {
    PR_WORD_TYPE_UNKNOWN = 0,    /**< Unknown/unclassified */
    PR_WORD_TYPE_CONTENT,        /**< Content word (noun, verb, adj, adv) */
    PR_WORD_TYPE_FUNCTION,       /**< Function word (article, prep, conj) */
    PR_WORD_TYPE_PROPER,         /**< Proper noun */
    PR_WORD_TYPE_TECHNICAL       /**< Technical/domain-specific term */
} pr_word_type_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Phoneme-to-prime mapping entry
 *
 * Maps each phoneme enum to a unique prime number for signature generation.
 */
typedef struct {
    phoneme_t phoneme;      /**< Phoneme enumeration value */
    uint64_t prime;         /**< Associated prime number */
    float frequency;        /**< Phoneme frequency in language (0-1) */
    bool is_vowel;          /**< True if vowel */
} phoneme_prime_entry_t;

/**
 * @brief Word signature with metadata
 *
 * Represents a complete word with its prime signature and properties.
 */
typedef struct {
    prime_signature_t signature;    /**< Prime signature for word */
    uint64_t word_id;               /**< Unique word identifier */
    char word_text[64];             /**< Orthographic representation */
    phoneme_t phonemes[PR_SPEECH_MAX_WORD_PHONEMES]; /**< Phoneme sequence */
    uint32_t phoneme_count;         /**< Number of phonemes */
    pr_word_type_t word_type;       /**< Content/function/proper/technical */
    float frequency;                /**< Word frequency (0-1) */
    float neighborhood_density;     /**< Phonological neighborhood density */
    uint32_t syllable_count;        /**< Number of syllables */
    uint64_t created_time_ms;       /**< Creation timestamp */
} pr_word_signature_t;

/**
 * @brief Prosody features for quaternion computation
 *
 * Extracted prosodic features that map to quaternion emotion component.
 */
typedef struct {
    float pitch_mean_hz;            /**< Mean pitch (Hz) */
    float pitch_range_hz;           /**< Pitch range (Hz) */
    float pitch_slope;              /**< Pitch slope (positive=rising) */
    float intensity_mean_db;        /**< Mean intensity (dB) */
    float intensity_range_db;       /**< Intensity range (dB) */
    float speaking_rate_sps;        /**< Speaking rate (syllables/sec) */
    float stress_level;             /**< Overall stress level (0-1) */
    float pause_ratio;              /**< Ratio of pauses to speech */
    float pitch_contour_type;       /**< Contour: -1=fall, 0=flat, +1=rise */
} pr_prosody_features_t;

/**
 * @brief Speech bridge configuration
 */
typedef struct {
    /* Signature generation */
    uint32_t position_encoding_bits;    /**< Bits for position in word */
    float position_decay_factor;        /**< Position influence decay */
    bool enable_syllable_weighting;     /**< Weight by syllable position */

    /* Quaternion mapping */
    float consolidation_base;           /**< Base consolidation for new words */
    float emotion_sensitivity;          /**< Prosody->emotion scaling */
    float salience_content_weight;      /**< Content word salience boost */
    float accessibility_base;           /**< Base accessibility */

    /* Memory encoding */
    pr_memory_tier_t default_tier;      /**< Default memory tier for words */
    bool auto_create_entanglements;     /**< Auto-link related words */
    float entanglement_threshold;       /**< Min resonance for auto-link */

    /* Theta-gamma integration */
    bool enable_theta_gamma;            /**< Enable phase-gated encoding */
    float theta_frequency_hz;           /**< Theta frequency for syllables */
    float gamma_frequency_hz;           /**< Gamma frequency for phonemes */

    /* FEP integration */
    bool enable_fep_updates;            /**< Enable FEP prediction error */
    float word_boundary_pe_threshold;   /**< PE threshold for word boundary */

    /* Retrieval */
    size_t max_retrieval_results;       /**< Max words in retrieval */
    float retrieval_threshold;          /**< Min resonance for retrieval */
} pr_speech_bridge_config_t;

/**
 * @brief Speech bridge state
 */
typedef struct {
    /* Current processing state */
    phoneme_event_t phoneme_buffer[PR_SPEECH_MAX_PHONEME_BUFFER];
    size_t buffer_pos;                  /**< Current buffer position */

    /* Current word being built */
    phoneme_t current_word_phonemes[PR_SPEECH_MAX_WORD_PHONEMES];
    uint32_t current_word_length;       /**< Phonemes in current word */

    /* Prosody accumulator */
    pr_prosody_features_t current_prosody;
    uint32_t prosody_sample_count;      /**< Samples for averaging */

    /* Quaternion state */
    nimcp_quaternion_t current_speech_quat;

    /* Theta-gamma phase tracking */
    float theta_phase;                  /**< Current theta phase (rad) */
    float gamma_phase;                  /**< Current gamma phase (rad) */

    /* FEP state */
    float current_pe;                   /**< Current prediction error */
    float avg_pe;                       /**< Running average PE */

    /* Timing */
    uint64_t last_phoneme_time_ms;      /**< Last phoneme timestamp */
    uint64_t word_start_time_ms;        /**< Current word start time */

    /* Processing flags */
    bool word_in_progress;              /**< Currently building a word */
    bool waiting_for_boundary;          /**< Waiting for word boundary */
} pr_speech_bridge_state_t;

/**
 * @brief Speech bridge statistics
 */
typedef struct {
    uint64_t phonemes_processed;        /**< Total phonemes processed */
    uint64_t words_encoded;             /**< Words stored in memory */
    uint64_t words_retrieved;           /**< Words retrieved */
    uint64_t word_boundaries_detected;  /**< Word boundaries from FEP */
    uint64_t signatures_computed;       /**< Signatures generated */
    uint64_t entanglements_created;     /**< Auto-created entanglements */
    float avg_word_length;              /**< Average phonemes per word */
    float avg_encoding_time_us;         /**< Average encoding time */
    float avg_retrieval_time_us;        /**< Average retrieval time */
    float avg_prosody_emotion;          /**< Average emotion from prosody */
    uint64_t theta_cycles;              /**< Theta cycles tracked */
    uint64_t gamma_bursts;              /**< Gamma bursts detected */
} pr_speech_bridge_stats_t;

/**
 * @brief Retrieval result entry
 */
typedef struct {
    uint64_t memory_id;                 /**< Memory node ID */
    pr_word_signature_t word_sig;       /**< Word signature info */
    float resonance_score;              /**< Resonance with query */
    float jaccard_similarity;           /**< Prime signature Jaccard */
    float quaternion_similarity;        /**< Quaternion state similarity */
} pr_speech_retrieval_result_t;

/**
 * @brief Main speech bridge structure
 */
typedef struct {
    /* Configuration */
    pr_speech_bridge_config_t config;

    /* Connected systems */
    speech_cortex_t* speech_cortex;
    speech_cortex_fep_bridge_t* fep_bridge;
    pr_node_manager_t node_manager;
    entangle_graph_t entanglement_graph;
    theta_gamma_manager_t theta_gamma;

    /* Current speech memory */
    pr_memory_node_t* current_speech_memory;

    /* Phoneme to prime mapping */
    phoneme_prime_entry_t phoneme_to_prime[PR_SPEECH_NUM_PHONEMES];

    /* Signature configuration */
    prime_sig_config_t sig_config;

    /* State */
    pr_speech_bridge_state_t state;

    /* Word signature cache */
    pr_word_signature_t* word_signatures;
    size_t word_count;
    size_t word_capacity;

    /* Statistics */
    pr_speech_bridge_stats_t stats;

    /* Initialization flag */
    bool initialized;

    /** Instance-level health agent (B25 Upgrade) */
    nimcp_health_agent_t* health_agent;
} pr_speech_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default speech bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for typical speech-memory integration
 * HOW:  Sets biologically-plausible parameters for phoneme-prime mapping
 *
 * @return Default configuration structure
 *
 * Default values:
 * - position_encoding_bits: 4
 * - emotion_sensitivity: 0.5
 * - default_tier: Z1 (short-term)
 * - enable_theta_gamma: true
 * - enable_fep_updates: true
 */
NIMCP_EXPORT pr_speech_bridge_config_t pr_speech_bridge_config_default(void);

/**
 * @brief Validate speech bridge configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_speech_bridge_config_validate(
    const pr_speech_bridge_config_t* config
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create speech bridge instance
 *
 * WHAT: Allocates and initializes speech-PR memory bridge
 * WHY:  Enable phoneme-to-prime mapping and speech memory encoding
 * HOW:  Allocates bridge, initializes phoneme-prime table, sets up buffers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 *
 * COMPLEXITY: O(1) allocation + O(N) phoneme table init
 * MEMORY: ~2KB base + configurable word signature cache
 */
NIMCP_EXPORT pr_speech_bridge_t* pr_speech_bridge_create(
    const pr_speech_bridge_config_t* config
);

/**
 * @brief Destroy speech bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_speech_bridge_destroy(pr_speech_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * Clears buffers and resets processing state without destroying bridge.
 *
 * @param bridge Speech bridge
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_reset(
    pr_speech_bridge_t* bridge
);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect speech cortex
 *
 * WHAT: Links bridge to speech cortex for phoneme/prosody input
 * WHY:  Enable processing of detected speech
 * HOW:  Store pointer, validate connection
 *
 * @param bridge Speech bridge
 * @param cortex Speech cortex instance
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_connect_speech_cortex(
    pr_speech_bridge_t* bridge,
    speech_cortex_t* cortex
);

/**
 * @brief Connect FEP bridge
 *
 * WHAT: Links to speech FEP bridge for prediction error updates
 * WHY:  Enable word boundary detection via PE spikes
 * HOW:  Store pointer, configure PE threshold
 *
 * @param bridge Speech bridge
 * @param fep_bridge Speech FEP bridge instance
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_connect_fep_bridge(
    pr_speech_bridge_t* bridge,
    speech_cortex_fep_bridge_t* fep_bridge
);

/**
 * @brief Connect PR node manager
 *
 * WHAT: Links to PR memory system for word storage
 * WHY:  Enable creation of memory nodes for words
 * HOW:  Store manager handle
 *
 * @param bridge Speech bridge
 * @param manager PR node manager
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_connect_node_manager(
    pr_speech_bridge_t* bridge,
    pr_node_manager_t manager
);

/**
 * @brief Connect entanglement graph
 *
 * WHAT: Links to entanglement graph for word associations
 * WHY:  Enable automatic linking of related words
 * HOW:  Store graph handle
 *
 * @param bridge Speech bridge
 * @param graph Entanglement graph
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_connect_entanglement(
    pr_speech_bridge_t* bridge,
    entangle_graph_t graph
);

/**
 * @brief Connect theta-gamma manager
 *
 * WHAT: Links theta-gamma for phase-gated speech encoding
 * WHY:  Align memory operations with speech rhythm
 * HOW:  Store manager handle, configure frequencies
 *
 * @param bridge Speech bridge
 * @param theta_gamma Theta-gamma manager
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_connect_theta_gamma(
    pr_speech_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma
);

//=============================================================================
// Phoneme Processing Functions
//=============================================================================

/**
 * @brief Process single phoneme event
 *
 * WHAT: Handles incoming phoneme from speech cortex
 * WHY:  Accumulate phonemes for word signature computation
 * HOW:  Add to buffer, update prosody, check for word boundary
 *
 * @param bridge Speech bridge
 * @param event Phoneme event from speech cortex
 * @return PR_SPEECH_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~50us
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_process_phoneme(
    pr_speech_bridge_t* bridge,
    const phoneme_event_t* event
);

/**
 * @brief Process multiple phonemes
 *
 * @param bridge Speech bridge
 * @param events Array of phoneme events
 * @param count Number of events
 * @return Number of phonemes successfully processed
 */
NIMCP_EXPORT size_t pr_speech_bridge_process_phonemes(
    pr_speech_bridge_t* bridge,
    const phoneme_event_t* events,
    size_t count
);

/**
 * @brief Process complete word
 *
 * WHAT: Handles complete word -> prime signature -> memory encoding
 * WHY:  Main entry point for word-level speech memory
 * HOW:  Compute signature, quaternion, create memory node, link
 *
 * @param bridge Speech bridge
 * @param word_text Orthographic text (optional, can be NULL)
 * @param phonemes Phoneme sequence
 * @param phoneme_count Number of phonemes
 * @param word_type Word type for salience
 * @return Created memory node or NULL on failure
 *
 * COMPLEXITY: O(n) where n = phoneme_count
 * PERFORMANCE: ~200us
 */
NIMCP_EXPORT pr_memory_node_t* pr_speech_bridge_process_word(
    pr_speech_bridge_t* bridge,
    const char* word_text,
    const phoneme_t* phonemes,
    uint32_t phoneme_count,
    pr_word_type_t word_type
);

//=============================================================================
// Signature Computation Functions
//=============================================================================

/**
 * @brief Compute prime signature from phoneme sequence
 *
 * WHAT: Converts phoneme sequence to prime signature
 * WHY:  Enable content-addressable word storage and retrieval
 * HOW:  Map each phoneme to prime, multiply with position encoding
 *
 * @param bridge Speech bridge
 * @param phonemes Phoneme sequence
 * @param count Number of phonemes
 * @param signature Output signature (caller-allocated)
 * @return PR_SPEECH_SUCCESS or error code
 *
 * Algorithm:
 *   For each phoneme p at position i:
 *     exponent[prime_index(p)] += position_weight(i)
 *   Compute hash from exponents
 *
 * COMPLEXITY: O(n) where n = phoneme count
 * PERFORMANCE: ~100us
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_compute_phoneme_prime_sig(
    pr_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    size_t count,
    prime_signature_t* signature
);

/**
 * @brief Compute word-level prime signature
 *
 * WHAT: Creates signature for complete word with metadata
 * WHY:  Include word-level features beyond phonemes
 * HOW:  Combine phoneme signature with word frequency, length
 *
 * @param bridge Speech bridge
 * @param word_text Word text (for frequency lookup)
 * @param phonemes Phoneme sequence
 * @param count Phoneme count
 * @param word_type Word type
 * @param word_sig Output word signature (caller-allocated)
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_compute_word_prime_sig(
    pr_speech_bridge_t* bridge,
    const char* word_text,
    const phoneme_t* phonemes,
    size_t count,
    pr_word_type_t word_type,
    pr_word_signature_t* word_sig
);

/**
 * @brief Get prime number for phoneme
 *
 * @param bridge Speech bridge
 * @param phoneme Phoneme enum value
 * @return Prime number for phoneme, or 0 if invalid
 */
NIMCP_EXPORT uint64_t pr_speech_bridge_get_phoneme_prime(
    const pr_speech_bridge_t* bridge,
    phoneme_t phoneme
);

//=============================================================================
// Quaternion Computation Functions
//=============================================================================

/**
 * @brief Compute speech quaternion from prosody
 *
 * WHAT: Maps prosodic features to quaternion state
 * WHY:  Encode emotional/attentional aspects of speech in memory
 * HOW:  Pitch -> emotion, emphasis -> salience, etc.
 *
 * Mapping:
 *   w = f(word_frequency, repetition)
 *   x = f(pitch_contour, stress_pattern, speaking_rate)
 *   y = f(word_importance, novelty, emphasis)
 *   z = f(familiarity, neighborhood_density)
 *
 * @param bridge Speech bridge
 * @param prosody Prosody features
 * @param word_type Word type
 * @param word_frequency Word frequency (0-1)
 * @param neighborhood_density Phonological neighborhood density (0-1)
 * @return Computed quaternion
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~20us
 */
NIMCP_EXPORT nimcp_quaternion_t pr_speech_bridge_compute_speech_quaternion(
    pr_speech_bridge_t* bridge,
    const pr_prosody_features_t* prosody,
    pr_word_type_t word_type,
    float word_frequency,
    float neighborhood_density
);

/**
 * @brief Update prosody accumulator
 *
 * WHAT: Accumulates prosody features during word processing
 * WHY:  Build word-level prosody from phoneme-level features
 * HOW:  Running average of prosodic measures
 *
 * @param bridge Speech bridge
 * @param pitch_hz Current pitch
 * @param intensity_db Current intensity
 * @param stress Current stress level
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_update_prosody(
    pr_speech_bridge_t* bridge,
    float pitch_hz,
    float intensity_db,
    float stress
);

/**
 * @brief Get current prosody features
 *
 * @param bridge Speech bridge
 * @param prosody Output prosody structure
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_get_current_prosody(
    const pr_speech_bridge_t* bridge,
    pr_prosody_features_t* prosody
);

//=============================================================================
// Memory Encoding Functions
//=============================================================================

/**
 * @brief Encode word to PR memory
 *
 * WHAT: Creates PR memory node for word with signature and quaternion
 * WHY:  Store speech in content-addressable memory system
 * HOW:  Create node, set signature/state, optionally entangle
 *
 * @param bridge Speech bridge
 * @param word_sig Word signature to encode
 * @param quaternion Quaternion state for word
 * @return Created memory node or NULL on failure
 *
 * COMPLEXITY: O(n) for signature + O(1) for node creation
 * PERFORMANCE: ~200us
 */
NIMCP_EXPORT pr_memory_node_t* pr_speech_bridge_encode_word_memory(
    pr_speech_bridge_t* bridge,
    const pr_word_signature_t* word_sig,
    nimcp_quaternion_t quaternion
);

/**
 * @brief Force encoding of current word buffer
 *
 * @param bridge Speech bridge
 * @return Created memory node or NULL if buffer empty
 */
NIMCP_EXPORT pr_memory_node_t* pr_speech_bridge_flush_word_buffer(
    pr_speech_bridge_t* bridge
);

//=============================================================================
// Memory Retrieval Functions
//=============================================================================

/**
 * @brief Retrieve similar words by phoneme pattern
 *
 * WHAT: Finds words with similar phoneme signatures
 * WHY:  Enable phonological neighbor retrieval, rhyme finding
 * HOW:  Compute query signature, resonance-based search
 *
 * @param bridge Speech bridge
 * @param phonemes Query phoneme sequence
 * @param count Phoneme count
 * @param results Output results array (caller-allocated)
 * @param max_results Maximum results
 * @param result_count Output: actual results found
 * @return PR_SPEECH_SUCCESS or error code
 *
 * COMPLEXITY: O(N) where N = stored words
 * PERFORMANCE: ~500us for top-10
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_retrieve_similar_words(
    pr_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    size_t count,
    pr_speech_retrieval_result_t* results,
    size_t max_results,
    size_t* result_count
);

/**
 * @brief Retrieve words by text pattern
 *
 * @param bridge Speech bridge
 * @param text_pattern Text to search for
 * @param results Output results
 * @param max_results Maximum results
 * @param result_count Output: actual count
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_retrieve_by_text(
    pr_speech_bridge_t* bridge,
    const char* text_pattern,
    pr_speech_retrieval_result_t* results,
    size_t max_results,
    size_t* result_count
);

/**
 * @brief Find rhyming words
 *
 * @param bridge Speech bridge
 * @param phonemes Query phoneme sequence (ending)
 * @param count Phonemes to match at end
 * @param results Output results
 * @param max_results Maximum results
 * @param result_count Output: actual count
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_find_rhymes(
    pr_speech_bridge_t* bridge,
    const phoneme_t* phonemes,
    size_t count,
    pr_speech_retrieval_result_t* results,
    size_t max_results,
    size_t* result_count
);

//=============================================================================
// FEP Integration Functions
//=============================================================================

/**
 * @brief Update from FEP prediction error
 *
 * WHAT: Processes prediction error from speech FEP bridge
 * WHY:  Use PE for word boundary detection and learning
 * HOW:  Check PE against threshold, trigger word encoding if high
 *
 * @param bridge Speech bridge
 * @param prediction_error Current PE value
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_update_from_fep(
    pr_speech_bridge_t* bridge,
    float prediction_error
);

/**
 * @brief Detect word boundary from PE
 *
 * WHAT: Checks if PE indicates word boundary
 * WHY:  PE spikes at unexpected phonemes (word boundaries)
 * HOW:  Compare PE to threshold, consider context
 *
 * @param bridge Speech bridge
 * @return true if word boundary detected
 */
NIMCP_EXPORT bool pr_speech_bridge_detect_word_boundary(
    pr_speech_bridge_t* bridge
);

//=============================================================================
// Theta-Gamma Integration Functions
//=============================================================================

/**
 * @brief Update theta-gamma phase
 *
 * WHAT: Advances oscillator phases for speech timing
 * WHY:  Align encoding/retrieval with speech rhythm
 * HOW:  Update phases based on elapsed time
 *
 * @param bridge Speech bridge
 * @param dt_ms Time delta in milliseconds
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_update_theta_gamma(
    pr_speech_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Check if encoding is allowed
 *
 * @param bridge Speech bridge
 * @return true if theta phase allows encoding
 */
NIMCP_EXPORT bool pr_speech_bridge_can_encode(
    const pr_speech_bridge_t* bridge
);

/**
 * @brief Check if retrieval is allowed
 *
 * @param bridge Speech bridge
 * @return true if theta phase allows retrieval
 */
NIMCP_EXPORT bool pr_speech_bridge_can_retrieve(
    const pr_speech_bridge_t* bridge
);

/**
 * @brief Get encoding gate strength
 *
 * @param bridge Speech bridge
 * @return Encoding gate strength (0-1)
 */
NIMCP_EXPORT float pr_speech_bridge_get_encode_strength(
    const pr_speech_bridge_t* bridge
);

//=============================================================================
// State and Statistics Functions
//=============================================================================

/**
 * @brief Get bridge state
 *
 * @param bridge Speech bridge
 * @param state Output state structure
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_get_state(
    const pr_speech_bridge_t* bridge,
    pr_speech_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Speech bridge
 * @param stats Output statistics structure
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_get_stats(
    const pr_speech_bridge_t* bridge,
    pr_speech_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Speech bridge
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_reset_stats(
    pr_speech_bridge_t* bridge
);

/**
 * @brief Get current speech quaternion
 *
 * @param bridge Speech bridge
 * @return Current quaternion state
 */
NIMCP_EXPORT nimcp_quaternion_t pr_speech_bridge_get_current_quaternion(
    const pr_speech_bridge_t* bridge
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_speech_error_string(pr_speech_error_t error);

/**
 * @brief Get word type name
 *
 * @param word_type Word type enum
 * @return Static string name
 */
NIMCP_EXPORT const char* pr_speech_word_type_name(pr_word_type_t word_type);

/**
 * @brief Initialize phoneme-to-prime mapping table
 *
 * Internal function exposed for testing.
 *
 * @param bridge Speech bridge
 * @return PR_SPEECH_SUCCESS or error code
 */
NIMCP_EXPORT pr_speech_error_t pr_speech_bridge_init_phoneme_primes(
    pr_speech_bridge_t* bridge
);

/**
 * @brief Compute position weight for signature
 *
 * @param position Position in word (0-indexed)
 * @param word_length Total word length
 * @param decay_factor Position decay factor
 * @return Position weight
 */
NIMCP_EXPORT float pr_speech_bridge_position_weight(
    uint32_t position,
    uint32_t word_length,
    float decay_factor
);

/**
 * @brief Print bridge state (debug)
 *
 * @param bridge Speech bridge
 */
NIMCP_EXPORT void pr_speech_bridge_print_state(
    const pr_speech_bridge_t* bridge
);

/**
 * @brief Print word signature (debug)
 *
 * @param word_sig Word signature to print
 */
NIMCP_EXPORT void pr_speech_word_signature_print(
    const pr_word_signature_t* word_sig
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_SPEECH_BRIDGE_H */
