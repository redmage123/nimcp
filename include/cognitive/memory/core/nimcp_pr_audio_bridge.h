//=============================================================================
// nimcp_pr_audio_bridge.h - Prime Resonant Audio Bridge
//=============================================================================
/**
 * @file nimcp_pr_audio_bridge.h
 * @brief Integration of audio processing with Prime Resonant memory
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge connecting cochlea, audio cortex, and audio FEP to PR memory system
 * WHY:  Enable semantic encoding of auditory experiences with prime signatures,
 *       quaternion states, and theta-gamma phase gating for audio memory
 * HOW:  Extract audio features (MFCC, mel, onset) -> compute prime signatures ->
 *       derive quaternion states -> encode to PR memory with entanglement
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 *
 * Auditory Memory Encoding Pathway:
 * +-------------------------------------------------------------------------+
 * |  Cochlea -> A1 (Primary Auditory Cortex) -> STG -> Hippocampus         |
 * |                                                                          |
 * |  Biological Process:                                                     |
 * |  1. Cochlea: Spectrotemporal decomposition (tonotopic map)              |
 * |  2. A1: Feature extraction (MFCC-like representations)                   |
 * |  3. Belt/Parabelt: Temporal pattern recognition                          |
 * |  4. Hippocampus: Binding to episodic memory via theta-gamma             |
 * |                                                                          |
 * |  Key Neural Signatures:                                                  |
 * |  - Onset response: Rapid adaptation to sound beginnings (MMN)            |
 * |  - Spectral tuning: Neurons selective for frequency bands                |
 * |  - Temporal patterns: Phase-locking to rhythmic structure                |
 * +-------------------------------------------------------------------------+
 *
 * Audio Features to Prime Signature Mapping:
 * +-------------------------------------------------------------------------+
 * |  MFCC Coefficients -> Prime Index Mapping:                               |
 * |  - 13 MFCCs quantized to bins (0-7 each)                                 |
 * |  - Each bin maps to a prime factor exponent                              |
 * |  - Captures timbral characteristics                                      |
 * |                                                                          |
 * |  Spectral Features -> Dedicated Primes:                                  |
 * |  - Spectral centroid -> primes[0-7]                                      |
 * |  - Spectral spread -> primes[8-15]                                       |
 * |  - Spectral flux -> primes[16-23]                                        |
 * |  - Spectral rolloff -> primes[24-31]                                     |
 * |                                                                          |
 * |  Temporal Patterns -> Modular Arithmetic:                                |
 * |  - Onset intervals -> primes[32-47]                                      |
 * |  - Rhythmic ratios -> primes[48-63]                                      |
 * +-------------------------------------------------------------------------+
 *
 * Quaternion State for Audio Memories:
 * +-------------------------------------------------------------------------+
 * |  Component | Audio Feature Mapping                    | Range            |
 * |------------|------------------------------------------|------------------|
 * |  w (cons.) | Repetition count + temporal coherence    | [0, 1]           |
 * |  x (emot.) | Mode + tempo + timbre brightness         | [-1, +1]         |
 * |  y (sal.)  | Onset strength + loudness + contrast     | [0, 1]           |
 * |  z (acc.)  | Familiarity + recent access frequency    | [0, 1]           |
 * +-------------------------------------------------------------------------+
 *
 * Theta-Gamma Coupling for Audio:
 * +-------------------------------------------------------------------------+
 * |  Theta Phase (4-8 Hz):                                                   |
 * |  - Encoding window: Store new audio patterns                             |
 * |  - Retrieval window: Recall similar sounds                               |
 * |                                                                          |
 * |  Gamma Bursts (30-100 Hz):                                               |
 * |  - High gamma (60-100 Hz): Feature binding during encoding               |
 * |  - Low gamma (30-60 Hz): Pattern completion during recall                |
 * |                                                                          |
 * |  Audio-Specific Timing:                                                  |
 * |  - MMN peaks at theta trough (encoding window)                           |
 * |  - Familiarity recognition at theta peak (retrieval window)              |
 * +-------------------------------------------------------------------------+
 *
 * FEP Integration (Prediction Error):
 * +-------------------------------------------------------------------------+
 * |  Audio FEP provides:                                                      |
 * |  - Prediction error magnitude -> salience modulation                     |
 * |  - Temporal prediction accuracy -> consolidation boost                   |
 * |  - Precision weighting -> quaternion accessibility                       |
 * |                                                                          |
 * |  Novel sounds (high PE) -> enhanced encoding                             |
 * |  Expected sounds (low PE) -> weaker encoding, quick retrieval            |
 * +-------------------------------------------------------------------------+
 *
 * ARCHITECTURE:
 * =============================================================================
 *
 *   +---------------------------------------------------------------------+
 *   |                  PR AUDIO BRIDGE                                     |
 *   |                                                                      |
 *   |  +-------------+    +----------------+    +------------------+       |
 *   |  |   Cochlea   |--->| Audio Features |--->| Prime Signature  |       |
 *   |  | (spectral)  |    | (MFCC/mel)     |    | (content hash)   |       |
 *   |  +-------------+    +----------------+    +------------------+       |
 *   |        |                   |                      |                  |
 *   |        v                   v                      v                  |
 *   |  +-------------+    +----------------+    +------------------+       |
 *   |  |Audio Cortex |--->|   Quaternion   |--->|  PR Memory Node  |       |
 *   |  | (attention) |    | (semantic)     |    | (entangled)      |       |
 *   |  +-------------+    +----------------+    +------------------+       |
 *   |        |                   |                      |                  |
 *   |        v                   v                      v                  |
 *   |  +-------------+    +----------------+    +------------------+       |
 *   |  | FEP Bridge  |--->| Theta-Gamma    |--->|  Entanglement    |       |
 *   |  | (pred err)  |    | (phase gate)   |    |  Graph           |       |
 *   |  +-------------+    +----------------+    +------------------+       |
 *   +---------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * =============================================================================
 * - Frame processing: ~500us (includes FFT, MFCC, signature generation)
 * - Signature computation: ~100us (MFCC + spectral + temporal)
 * - Quaternion computation: ~10us (feature aggregation)
 * - Memory encoding: ~200us (signature + quaternion + entanglement)
 * - Similar retrieval (N=1000): ~2ms (resonance scoring)
 *
 * MEMORY:
 * =============================================================================
 * - pr_audio_bridge_t: ~2KB (excluding connected components)
 * - MFCC history buffer: num_mfcc * history_len * 4 bytes
 * - Onset history: onset_history_len * 4 bytes
 *
 * THREAD SAFETY:
 * =============================================================================
 * - Bridge instance: NOT thread-safe (use external synchronization)
 * - Memory operations: Thread-safe via PR memory system
 * - Audio processing: Single-threaded (process in audio callback thread)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_AUDIO_BRIDGE_H
#define NIMCP_PR_AUDIO_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core dependencies */
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_theta_gamma.h"
#include "cognitive/memory/core/nimcp_resonance.h"

/* Audio components - forward declarations */
typedef struct cochlea cochlea_t;
typedef struct audio_cortex audio_cortex_t;
typedef struct audio_cortex_fep_bridge audio_cortex_fep_bridge_t;

/* Forward declaration for health agent (Phase 8) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro */
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum MFCC coefficients */
#define PR_AUDIO_MAX_MFCC               26

/** Maximum mel filter banks */
#define PR_AUDIO_MAX_MEL_FILTERS        128

/** Maximum history frames for temporal patterns */
#define PR_AUDIO_MAX_HISTORY_FRAMES     256

/** Maximum onset intervals to track */
#define PR_AUDIO_MAX_ONSET_INTERVALS    64

/** Default MFCC history length (frames) */
#define PR_AUDIO_DEFAULT_HISTORY_LEN    64

/** Default onset history length */
#define PR_AUDIO_DEFAULT_ONSET_HISTORY  32

/** Spectral centroid prime range start */
#define PR_AUDIO_PRIME_CENTROID_START   0

/** Spectral spread prime range start */
#define PR_AUDIO_PRIME_SPREAD_START     8

/** Spectral flux prime range start */
#define PR_AUDIO_PRIME_FLUX_START       16

/** Spectral rolloff prime range start */
#define PR_AUDIO_PRIME_ROLLOFF_START    24

/** Temporal onset prime range start */
#define PR_AUDIO_PRIME_ONSET_START      32

/** Rhythmic pattern prime range start */
#define PR_AUDIO_PRIME_RHYTHM_START     48

/** MFCC quantization bins */
#define PR_AUDIO_MFCC_BINS              8

/** Onset strength threshold for salience */
#define PR_AUDIO_ONSET_THRESHOLD        0.3f

/** Default tempo for neutral emotion (BPM) */
#define PR_AUDIO_NEUTRAL_TEMPO          90.0f

/** Default brightness for neutral emotion (spectral centroid Hz) */
#define PR_AUDIO_NEUTRAL_BRIGHTNESS     2000.0f

/** Maximum similar memories to retrieve */
#define PR_AUDIO_MAX_SIMILAR_RESULTS    100

/** Epsilon for float comparisons */
#define PR_AUDIO_EPSILON                1e-6f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Audio memory type classification
 */
typedef enum {
    PR_AUDIO_TYPE_UNKNOWN = 0,    /**< Unknown/unclassified audio */
    PR_AUDIO_TYPE_SPEECH,         /**< Speech or voice */
    PR_AUDIO_TYPE_MUSIC,          /**< Musical content */
    PR_AUDIO_TYPE_AMBIENT,        /**< Environmental sounds */
    PR_AUDIO_TYPE_ALARM,          /**< Alert or alarm sounds */
    PR_AUDIO_TYPE_RHYTHM,         /**< Rhythmic pattern only */
    PR_AUDIO_TYPE_MELODY,         /**< Melodic pattern */
    PR_AUDIO_TYPE_COUNT
} pr_audio_type_t;

/**
 * @brief Error codes for audio bridge operations
 */
typedef enum {
    PR_AUDIO_SUCCESS = 0,              /**< Operation succeeded */
    PR_AUDIO_ERROR_NULL_POINTER = -1,  /**< NULL pointer argument */
    PR_AUDIO_ERROR_INVALID_CONFIG = -2,/**< Invalid configuration */
    PR_AUDIO_ERROR_NOT_CONNECTED = -3, /**< Component not connected */
    PR_AUDIO_ERROR_NO_MEMORY = -4,     /**< Memory allocation failed */
    PR_AUDIO_ERROR_ENCODING = -5,      /**< Encoding failed */
    PR_AUDIO_ERROR_RETRIEVAL = -6,     /**< Retrieval failed */
    PR_AUDIO_ERROR_PHASE_BLOCKED = -7, /**< Operation blocked by theta phase */
    PR_AUDIO_ERROR_INVALID_AUDIO = -8  /**< Invalid audio data */
} pr_audio_error_t;

/**
 * @brief Temporal pattern types detected
 */
typedef enum {
    PR_AUDIO_PATTERN_NONE = 0,        /**< No clear pattern */
    PR_AUDIO_PATTERN_STEADY,          /**< Steady/continuous sound */
    PR_AUDIO_PATTERN_RHYTHMIC,        /**< Regular rhythm detected */
    PR_AUDIO_PATTERN_MELODIC,         /**< Pitch sequence detected */
    PR_AUDIO_PATTERN_SPEECH,          /**< Speech envelope pattern */
    PR_AUDIO_PATTERN_TRANSIENT        /**< Single transient event */
} pr_audio_pattern_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Configuration for prime signature generation from audio
 */
typedef struct {
    /* MFCC configuration */
    uint32_t num_mfcc;               /**< Number of MFCC coefficients to use */
    uint32_t mfcc_bins;              /**< Quantization bins per MFCC */
    bool use_delta_mfcc;             /**< Include delta MFCCs in signature */
    bool use_delta_delta_mfcc;       /**< Include delta-delta MFCCs */

    /* Spectral feature weights */
    float centroid_weight;           /**< Weight for spectral centroid */
    float spread_weight;             /**< Weight for spectral spread */
    float flux_weight;               /**< Weight for spectral flux */
    float rolloff_weight;            /**< Weight for spectral rolloff */

    /* Temporal pattern weights */
    float onset_weight;              /**< Weight for onset intervals */
    float rhythm_weight;             /**< Weight for rhythmic patterns */

    /* Normalization */
    bool normalize_features;         /**< Normalize features before hashing */
    float sparsity_target;           /**< Target signature sparsity [0, 1] */
} pr_audio_sig_config_t;

/**
 * @brief Configuration for audio quaternion computation
 */
typedef struct {
    /* Consolidation (w) parameters */
    float repetition_decay;          /**< Decay factor for repetition count */
    float coherence_weight;          /**< Weight of temporal coherence */

    /* Emotion (x) parameters - mapping audio features to valence */
    float tempo_influence;           /**< How much tempo affects emotion */
    float brightness_influence;      /**< How much spectral brightness affects */
    float mode_influence;            /**< How much major/minor affects */
    float neutral_tempo_bpm;         /**< Tempo for neutral emotion */
    float neutral_brightness_hz;     /**< Centroid for neutral emotion */

    /* Salience (y) parameters */
    float onset_salience_weight;     /**< Onset contribution to salience */
    float loudness_salience_weight;  /**< Loudness contribution */
    float contrast_salience_weight;  /**< Spectral contrast contribution */
    float novelty_salience_weight;   /**< Novelty from FEP contribution */

    /* Accessibility (z) parameters */
    float familiarity_weight;        /**< Weight of familiarity score */
    float recency_weight;            /**< Weight of recent access */
    float access_decay_rate;         /**< Decay rate for access history */
} pr_audio_quat_config_t;

/**
 * @brief Configuration for PR audio bridge
 */
typedef struct {
    /* Feature extraction */
    uint32_t sample_rate;            /**< Audio sample rate (Hz) */
    uint32_t frame_size;             /**< Processing frame size (samples) */
    uint32_t num_mel_filters;        /**< Number of mel filterbank channels */
    uint32_t num_mfcc;               /**< Number of MFCC coefficients */

    /* History buffers */
    uint32_t mfcc_history_len;       /**< Frames of MFCC history to keep */
    uint32_t onset_history_len;      /**< Number of onset intervals to track */

    /* Sub-configurations */
    pr_audio_sig_config_t sig_config;     /**< Prime signature config */
    pr_audio_quat_config_t quat_config;   /**< Quaternion config */

    /* Encoding parameters */
    float encoding_threshold;        /**< Min salience to encode new memory */
    float retrieval_threshold;       /**< Min resonance for retrieval match */
    bool auto_entangle;              /**< Automatically link similar memories */
    float entangle_threshold;        /**< Min resonance for auto-entanglement */

    /* Theta-gamma integration */
    bool use_theta_gating;           /**< Gate encoding/retrieval by theta phase */
    bool use_gamma_boost;            /**< Boost salience during gamma bursts */

    /* FEP integration */
    bool use_prediction_error;       /**< Use FEP PE for salience modulation */
    float pe_salience_factor;        /**< PE -> salience scaling factor */

    /* Memory tier */
    pr_memory_tier_t default_tier;   /**< Default tier for new audio memories (PR_MEMORY_TIER_Z0..Z3) */
} pr_audio_bridge_config_t;

/**
 * @brief Extracted audio features for signature computation
 */
typedef struct {
    /* MFCC features */
    float mfcc[PR_AUDIO_MAX_MFCC];        /**< MFCC coefficients */
    float delta_mfcc[PR_AUDIO_MAX_MFCC];  /**< Delta MFCCs */
    uint32_t num_mfcc;                     /**< Number of valid MFCCs */

    /* Spectral features */
    float spectral_centroid;         /**< Spectral centroid (Hz) */
    float spectral_spread;           /**< Spectral spread (Hz) */
    float spectral_flux;             /**< Spectral flux (energy change) */
    float spectral_rolloff;          /**< Spectral rolloff (Hz) */
    float spectral_contrast;         /**< Spectral contrast ratio */

    /* Temporal features */
    float onset_strength;            /**< Current onset strength [0, 1] */
    float tempo_estimate_bpm;        /**< Estimated tempo (BPM) */
    float beat_phase;                /**< Phase within beat [0, 1] */

    /* Energy features */
    float rms_energy;                /**< RMS energy level */
    float peak_energy;               /**< Peak energy in frame */
    float loudness_db;               /**< Loudness in dB */

    /* Derived features */
    float brightness;                /**< Spectral brightness (high freq ratio) */
    float roughness;                 /**< Sensory dissonance measure */
    float pitch_estimate_hz;         /**< Estimated pitch (Hz, 0 if unpitched) */

    /* Metadata */
    uint64_t timestamp_ms;           /**< Feature extraction timestamp */
    bool valid;                      /**< Features are valid */
} pr_audio_features_t;

/**
 * @brief Temporal pattern detection result
 */
typedef struct {
    pr_audio_pattern_t pattern_type; /**< Detected pattern type */
    float confidence;                /**< Pattern detection confidence [0, 1] */

    /* Rhythmic info (if pattern is rhythmic) */
    float tempo_bpm;                 /**< Detected tempo */
    float tempo_confidence;          /**< Tempo detection confidence */
    uint32_t beats_per_bar;          /**< Detected meter (e.g., 4 for 4/4) */

    /* Onset intervals */
    float onset_intervals[PR_AUDIO_MAX_ONSET_INTERVALS]; /**< Inter-onset intervals */
    uint32_t num_intervals;          /**< Number of valid intervals */
    float interval_regularity;       /**< How regular are intervals [0, 1] */

    /* Melodic info (if pattern is melodic) */
    float pitch_sequence[16];        /**< Recent pitch sequence (Hz) */
    uint32_t num_pitches;            /**< Number of pitches in sequence */
    bool ascending;                  /**< Overall ascending contour */

    /* Statistics */
    float mean_interval_ms;          /**< Mean inter-onset interval */
    float interval_variance;         /**< Interval variance */
} pr_audio_pattern_result_t;

/**
 * @brief Similar audio memory retrieval result
 */
typedef struct {
    uint64_t memory_id;              /**< PR memory node ID */
    pr_memory_node_t* memory_node;   /**< Pointer to memory node */
    float resonance_score;           /**< Total resonance [0, 1] */
    float signature_similarity;      /**< Prime signature similarity */
    float quaternion_similarity;     /**< Quaternion state similarity */
    pr_audio_type_t audio_type;      /**< Classified audio type */
    uint64_t original_timestamp;     /**< When original was encoded */
} pr_audio_retrieval_result_t;

/**
 * @brief Statistics for audio bridge operations
 */
typedef struct {
    /* Processing counts */
    uint64_t frames_processed;       /**< Total audio frames processed */
    uint64_t memories_encoded;       /**< Memories successfully encoded */
    uint64_t retrievals_performed;   /**< Retrieval queries executed */
    uint64_t patterns_detected;      /**< Temporal patterns detected */

    /* Timing statistics */
    float avg_frame_time_us;         /**< Average frame processing time */
    float avg_encode_time_us;        /**< Average encoding time */
    float avg_retrieval_time_us;     /**< Average retrieval time */

    /* Feature statistics */
    float mean_onset_strength;       /**< Mean onset strength seen */
    float mean_spectral_flux;        /**< Mean spectral flux */
    float mean_salience;             /**< Mean computed salience */

    /* Theta-gamma statistics */
    uint64_t encoding_windows;       /**< Frames in encoding window */
    uint64_t retrieval_windows;      /**< Frames in retrieval window */
    uint64_t blocked_by_phase;       /**< Operations blocked by phase */

    /* FEP statistics */
    float mean_prediction_error;     /**< Mean PE from audio FEP */
    uint64_t high_pe_events;         /**< High PE (novel sound) events */

    /* Memory statistics */
    uint64_t total_entanglements;    /**< Total entanglements created */
    float mean_entanglement_strength;/**< Mean entanglement weight */
} pr_audio_bridge_stats_t;

/**
 * @brief PR Audio Bridge instance
 *
 * Main structure integrating audio processing with Prime Resonant memory.
 * Connects cochlea, audio cortex, and FEP bridge to PR memory system.
 */
typedef struct pr_audio_bridge {
    /* Connected audio components */
    cochlea_t* cochlea;                      /**< Cochlea for low-level processing */
    audio_cortex_t* audio_cortex;            /**< Audio cortex for features */
    audio_cortex_fep_bridge_t* fep_bridge;   /**< FEP bridge for prediction error */

    /* PR memory integration */
    pr_node_manager_t node_manager;          /**< PR node manager for memory creation */
    pr_memory_node_t* current_audio_memory;  /**< Current/recent audio memory */
    entangle_graph_t audio_entanglement;     /**< Entanglement graph for audio */

    /* Feature -> Prime signature */
    prime_signature_t* current_signature;    /**< Current frame's signature */
    pr_audio_sig_config_t sig_config;        /**< Signature generation config */

    /* Quaternion for audio memories */
    nimcp_quaternion_t current_audio_quat;   /**< Current semantic state */
    pr_audio_quat_config_t quat_config;      /**< Quaternion computation config */

    /* Theta-gamma integration */
    theta_gamma_manager_t theta_gamma;       /**< Theta-gamma coupling manager */

    /* Feature history buffers */
    float* mfcc_history;                     /**< Rolling MFCC buffer */
    uint32_t mfcc_history_len;               /**< Frames in MFCC history */
    uint32_t mfcc_history_pos;               /**< Current position in ring buffer */
    uint32_t num_mfcc;                        /**< MFCCs per frame */

    /* Onset history */
    float* onset_history;                    /**< Onset interval history */
    uint32_t onset_history_len;              /**< Length of onset history */
    uint32_t onset_history_pos;              /**< Current position */
    uint64_t last_onset_time_ms;             /**< Time of last onset */

    /* State tracking */
    pr_audio_features_t current_features;    /**< Current extracted features */
    pr_audio_pattern_result_t current_pattern; /**< Current pattern detection */
    uint32_t repetition_count;               /**< Times similar pattern repeated */
    float familiarity_score;                 /**< Running familiarity estimate */

    /* FEP state */
    float current_prediction_error;          /**< Latest PE from FEP */
    float temporal_prediction_accuracy;      /**< Temporal prediction accuracy */

    /* Configuration */
    pr_audio_bridge_config_t config;         /**< Bridge configuration */

    /* Statistics */
    pr_audio_bridge_stats_t stats;           /**< Operational statistics */

    /* State flags */
    bool initialized;                        /**< Bridge is initialized */
    bool cochlea_connected;                  /**< Cochlea is connected */
    bool cortex_connected;                   /**< Audio cortex is connected */
    bool fep_connected;                      /**< FEP bridge is connected */

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
} pr_audio_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default audio bridge configuration
 *
 * WHAT: Returns sensible defaults for audio-PR integration
 * WHY:  Provides starting point for typical audio processing
 * HOW:  Sets standard sample rate, frame size, feature counts
 *
 * @return Default configuration structure
 *
 * Defaults:
 * - sample_rate: 16000 Hz
 * - frame_size: 512 samples (32ms)
 * - num_mel_filters: 40
 * - num_mfcc: 13
 * - mfcc_history_len: 64 frames
 * - encoding_threshold: 0.3
 * - use_theta_gating: true
 */
NIMCP_EXPORT pr_audio_bridge_config_t pr_audio_bridge_config_default(void);

/**
 * @brief Get default prime signature configuration for audio
 *
 * @return Default signature generation config
 */
NIMCP_EXPORT pr_audio_sig_config_t pr_audio_sig_config_default(void);

/**
 * @brief Get default quaternion configuration for audio
 *
 * @return Default quaternion computation config
 */
NIMCP_EXPORT pr_audio_quat_config_t pr_audio_quat_config_default(void);

/**
 * @brief Validate audio bridge configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_audio_bridge_config_validate(
    const pr_audio_bridge_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create PR audio bridge instance
 *
 * WHAT: Allocates and initializes audio-PR integration bridge
 * WHY:  Prepare for audio memory encoding and retrieval
 * HOW:  Allocates buffers, initializes state, creates sub-components
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param node_manager PR node manager for memory creation
 * @return New bridge instance or NULL on failure
 *
 * COMPLEXITY: O(buffer_sizes)
 * MEMORY: ~2KB + history buffers
 *
 * EXAMPLE:
 * ```c
 * pr_audio_bridge_config_t config = pr_audio_bridge_config_default();
 * pr_audio_bridge_t* bridge = pr_audio_bridge_create(&config, node_manager);
 * if (!bridge) {
 *     // Handle error
 * }
 * ```
 */
NIMCP_EXPORT pr_audio_bridge_t* pr_audio_bridge_create(
    const pr_audio_bridge_config_t* config,
    pr_node_manager_t node_manager);

/**
 * @brief Destroy PR audio bridge and free resources
 *
 * WHAT: Releases all bridge resources
 * WHY:  Proper cleanup
 * HOW:  Frees buffers, disconnects components, destroys sub-systems
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * NOTE: Does NOT destroy connected cochlea/cortex/FEP components
 */
NIMCP_EXPORT void pr_audio_bridge_destroy(pr_audio_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Audio bridge
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_reset(pr_audio_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect cochlea for low-level audio processing
 *
 * WHAT: Link cochlea to bridge for spectral features
 * WHY:  Cochlea provides channel energy, onset detection
 * HOW:  Store cochlea pointer, enable cochlea features
 *
 * @param bridge Audio bridge
 * @param cochlea Cochlea instance
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_cochlea(
    pr_audio_bridge_t* bridge,
    cochlea_t* cochlea);

/**
 * @brief Connect audio cortex for higher-level features
 *
 * WHAT: Link audio cortex for MFCC, attention, memory
 * WHY:  Cortex provides feature extraction and attention
 * HOW:  Store cortex pointer, enable cortex features
 *
 * @param bridge Audio bridge
 * @param cortex Audio cortex instance
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_audio_cortex(
    pr_audio_bridge_t* bridge,
    audio_cortex_t* cortex);

/**
 * @brief Connect audio FEP bridge for prediction error
 *
 * WHAT: Link FEP bridge for prediction error signals
 * WHY:  PE modulates salience and encoding strength
 * HOW:  Store FEP bridge pointer, enable PE features
 *
 * @param bridge Audio bridge
 * @param fep_bridge Audio FEP bridge instance
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_fep(
    pr_audio_bridge_t* bridge,
    audio_cortex_fep_bridge_t* fep_bridge);

/**
 * @brief Connect theta-gamma manager for phase gating
 *
 * @param bridge Audio bridge
 * @param theta_gamma Theta-gamma manager
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_theta_gamma(
    pr_audio_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma);

/**
 * @brief Connect entanglement graph for memory associations
 *
 * @param bridge Audio bridge
 * @param graph Entanglement graph
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_connect_entanglement(
    pr_audio_bridge_t* bridge,
    entangle_graph_t graph);

//=============================================================================
// Main Processing Functions
//=============================================================================

/**
 * @brief Process audio frame through complete pipeline
 *
 * WHAT: Main processing loop - audio features to PR memory
 * WHY:  Unified entry point for audio-memory integration
 * HOW:  Extract features -> compute signature -> compute quaternion ->
 *       check theta phase -> encode to memory if appropriate
 *
 * ALGORITHM:
 * 1. Extract audio features (MFCC, spectral, onset)
 * 2. Compute prime signature from features
 * 3. Compute quaternion semantic state
 * 4. Check theta phase for encoding/retrieval
 * 5. If encoding window and above threshold: create memory
 * 6. Auto-entangle with similar existing memories
 * 7. Update FEP predictions if connected
 *
 * @param bridge Audio bridge
 * @param audio_data Raw audio samples (float, mono)
 * @param num_samples Number of samples
 * @return PR_AUDIO_SUCCESS or error code
 *
 * PERFORMANCE: ~500us per frame
 *
 * EXAMPLE:
 * ```c
 * float audio_buffer[512];
 * // ... fill buffer from audio source ...
 * pr_audio_error_t err = pr_audio_bridge_process_frame(bridge, audio_buffer, 512);
 * if (err == PR_AUDIO_SUCCESS) {
 *     // Frame processed, memory may have been encoded
 * }
 * ```
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_process_frame(
    pr_audio_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples);

/**
 * @brief Extract audio features from raw samples
 *
 * WHAT: Feature extraction without memory operations
 * WHY:  Allow inspection of features before encoding
 * HOW:  Compute MFCC, spectral features, onset strength
 *
 * @param bridge Audio bridge
 * @param audio_data Raw audio samples
 * @param num_samples Number of samples
 * @param features Output feature structure
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_extract_features(
    pr_audio_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    pr_audio_features_t* features);

//=============================================================================
// Prime Signature Functions
//=============================================================================

/**
 * @brief Compute prime signature from audio features
 *
 * WHAT: Generate content-addressable signature from audio
 * WHY:  Enable similarity-based retrieval of audio memories
 * HOW:  Map MFCC + spectral + temporal features to prime exponents
 *
 * ALGORITHM:
 * 1. Quantize MFCCs to bins -> prime indices [0-31]
 * 2. Map spectral centroid/spread/flux/rolloff -> primes [32-55]
 * 3. Hash onset intervals -> primes [56-63]
 * 4. Compute final signature hash
 *
 * @param bridge Audio bridge
 * @param features Extracted audio features
 * @param signature Output signature (must be allocated)
 * @return PR_AUDIO_SUCCESS or error code
 *
 * PERFORMANCE: ~100us
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_compute_audio_prime_sig(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features,
    prime_signature_t* signature);

/**
 * @brief Compute signature similarity between two audio signatures
 *
 * @param bridge Audio bridge
 * @param sig1 First signature
 * @param sig2 Second signature
 * @return Similarity [0, 1] or -1 on error
 */
NIMCP_EXPORT float pr_audio_bridge_signature_similarity(
    pr_audio_bridge_t* bridge,
    const prime_signature_t* sig1,
    const prime_signature_t* sig2);

//=============================================================================
// Quaternion Functions
//=============================================================================

/**
 * @brief Compute quaternion semantic state from audio features
 *
 * WHAT: Map audio features to quaternion (w, x, y, z)
 * WHY:  Encode semantic/emotional state of audio memory
 * HOW:  Feature aggregation to quaternion components
 *
 * MAPPING:
 * - w (consolidation): f(repetition_count, temporal_coherence)
 * - x (emotion): f(major/minor_mode, tempo, brightness)
 * - y (salience): f(onset_strength, loudness, contrast, novelty)
 * - z (accessibility): f(familiarity, recent_access)
 *
 * @param bridge Audio bridge
 * @param features Extracted audio features
 * @param quaternion Output quaternion
 * @return PR_AUDIO_SUCCESS or error code
 *
 * PERFORMANCE: ~10us
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_compute_audio_quaternion(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features,
    nimcp_quaternion_t* quaternion);

/**
 * @brief Compute onset salience from onset features
 *
 * WHAT: Convert onset strength to salience contribution
 * WHY:  Onsets strongly affect attention and memory formation
 * HOW:  Threshold and scale onset strength
 *
 * @param bridge Audio bridge
 * @param onset_strength Raw onset strength [0, 1]
 * @param loudness_peak Peak loudness (dB)
 * @param spectral_contrast Spectral contrast ratio
 * @return Onset salience contribution [0, 1]
 */
NIMCP_EXPORT float pr_audio_bridge_compute_onset_salience(
    pr_audio_bridge_t* bridge,
    float onset_strength,
    float loudness_peak,
    float spectral_contrast);

/**
 * @brief Compute emotional valence from audio features
 *
 * WHAT: Map audio characteristics to emotional valence
 * WHY:  Audio conveys emotion through tempo, mode, timbre
 * HOW:  Combine tempo, brightness, mode indicators
 *
 * @param bridge Audio bridge
 * @param features Audio features
 * @return Emotional valence [-1, +1]
 */
NIMCP_EXPORT float pr_audio_bridge_compute_emotion_valence(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features);

//=============================================================================
// Memory Encoding Functions
//=============================================================================

/**
 * @brief Encode current audio state to PR memory
 *
 * WHAT: Create PR memory node from current audio state
 * WHY:  Persist audio experience for later retrieval
 * HOW:  Package signature + quaternion + data into PR node
 *
 * @param bridge Audio bridge
 * @param audio_type Classification of audio content
 * @param memory_out Output pointer to created memory (optional)
 * @return PR_AUDIO_SUCCESS or error code
 *
 * PERFORMANCE: ~200us
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_encode_to_memory(
    pr_audio_bridge_t* bridge,
    pr_audio_type_t audio_type,
    pr_memory_node_t** memory_out);

/**
 * @brief Encode with explicit features and signature
 *
 * WHAT: Create memory from provided features
 * WHY:  Allow external feature computation
 * HOW:  Use provided signature and quaternion
 *
 * @param bridge Audio bridge
 * @param features Audio features
 * @param signature Prime signature
 * @param quaternion Semantic state
 * @param audio_type Audio classification
 * @param memory_out Output memory pointer
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_encode_with_features(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features,
    const prime_signature_t* signature,
    nimcp_quaternion_t quaternion,
    pr_audio_type_t audio_type,
    pr_memory_node_t** memory_out);

//=============================================================================
// Retrieval Functions
//=============================================================================

/**
 * @brief Retrieve similar audio memories by resonance
 *
 * WHAT: Find memories that resonate with current audio
 * WHY:  Recognition, recall, association of audio patterns
 * HOW:  Compute resonance with all audio memories, return top-K
 *
 * @param bridge Audio bridge
 * @param max_results Maximum results to return
 * @param results Output array of results (pre-allocated)
 * @param num_results Output: actual number of results
 * @return PR_AUDIO_SUCCESS or error code
 *
 * PERFORMANCE: O(N) where N = number of audio memories
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_retrieve_similar_audio(
    pr_audio_bridge_t* bridge,
    uint32_t max_results,
    pr_audio_retrieval_result_t* results,
    uint32_t* num_results);

/**
 * @brief Retrieve by explicit query features
 *
 * WHAT: Find memories matching provided features
 * WHY:  Search by specific audio characteristics
 * HOW:  Compute resonance between query and stored memories
 *
 * @param bridge Audio bridge
 * @param query_signature Query signature
 * @param query_quaternion Query semantic state
 * @param max_results Maximum results
 * @param results Output array
 * @param num_results Output count
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_retrieve_by_query(
    pr_audio_bridge_t* bridge,
    const prime_signature_t* query_signature,
    nimcp_quaternion_t query_quaternion,
    uint32_t max_results,
    pr_audio_retrieval_result_t* results,
    uint32_t* num_results);

/**
 * @brief Retrieve by audio type classification
 *
 * @param bridge Audio bridge
 * @param audio_type Type to filter by
 * @param max_results Maximum results
 * @param results Output array
 * @param num_results Output count
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_retrieve_by_type(
    pr_audio_bridge_t* bridge,
    pr_audio_type_t audio_type,
    uint32_t max_results,
    pr_audio_retrieval_result_t* results,
    uint32_t* num_results);

//=============================================================================
// Temporal Pattern Functions
//=============================================================================

/**
 * @brief Detect temporal patterns in audio history
 *
 * WHAT: Recognize rhythmic, melodic, or speech patterns
 * WHY:  Temporal structure is key to audio memory
 * HOW:  Analyze onset intervals, pitch sequences
 *
 * @param bridge Audio bridge
 * @param result Output pattern detection result
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_detect_temporal_pattern(
    pr_audio_bridge_t* bridge,
    pr_audio_pattern_result_t* result);

/**
 * @brief Compute rhythm prime signature component
 *
 * WHAT: Generate signature from rhythmic structure
 * WHY:  Rhythm is distinct from spectral content
 * HOW:  Hash onset interval ratios to prime indices
 *
 * @param bridge Audio bridge
 * @param intervals Onset intervals (ms)
 * @param num_intervals Number of intervals
 * @param signature Output signature (modified)
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_compute_rhythm_signature(
    pr_audio_bridge_t* bridge,
    const float* intervals,
    uint32_t num_intervals,
    prime_signature_t* signature);

//=============================================================================
// FEP Integration Functions
//=============================================================================

/**
 * @brief Update from audio FEP prediction error
 *
 * WHAT: Incorporate FEP PE into audio memory encoding
 * WHY:  Novel sounds (high PE) should be better encoded
 * HOW:  PE modulates salience and encoding strength
 *
 * @param bridge Audio bridge
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_update_from_fep(
    pr_audio_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Audio bridge
 * @return Current PE value, or -1 if not connected
 */
NIMCP_EXPORT float pr_audio_bridge_get_prediction_error(
    const pr_audio_bridge_t* bridge);

/**
 * @brief Report audio features to FEP for prediction update
 *
 * @param bridge Audio bridge
 * @param features Current audio features
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_report_to_fep(
    pr_audio_bridge_t* bridge,
    const pr_audio_features_t* features);

//=============================================================================
// Theta-Gamma Integration Functions
//=============================================================================

/**
 * @brief Check if encoding is allowed by theta phase
 *
 * @param bridge Audio bridge
 * @return true if in encoding window
 */
NIMCP_EXPORT bool pr_audio_bridge_can_encode(const pr_audio_bridge_t* bridge);

/**
 * @brief Check if retrieval is allowed by theta phase
 *
 * @param bridge Audio bridge
 * @return true if in retrieval window
 */
NIMCP_EXPORT bool pr_audio_bridge_can_retrieve(const pr_audio_bridge_t* bridge);

/**
 * @brief Get current encoding gate strength
 *
 * @param bridge Audio bridge
 * @return Encoding strength [0, 1]
 */
NIMCP_EXPORT float pr_audio_bridge_get_encode_strength(
    const pr_audio_bridge_t* bridge);

/**
 * @brief Get current retrieval gate strength
 *
 * @param bridge Audio bridge
 * @return Retrieval strength [0, 1]
 */
NIMCP_EXPORT float pr_audio_bridge_get_retrieve_strength(
    const pr_audio_bridge_t* bridge);

/**
 * @brief Modulate quaternion by current theta phase
 *
 * @param bridge Audio bridge
 * @param quaternion Input quaternion
 * @return Phase-modulated quaternion
 */
NIMCP_EXPORT nimcp_quaternion_t pr_audio_bridge_modulate_by_phase(
    pr_audio_bridge_t* bridge,
    nimcp_quaternion_t quaternion);

//=============================================================================
// Entanglement Functions
//=============================================================================

/**
 * @brief Auto-entangle new memory with similar existing memories
 *
 * WHAT: Create entanglement edges based on resonance
 * WHY:  Build associative network of audio memories
 * HOW:  Find similar memories, create weighted edges
 *
 * @param bridge Audio bridge
 * @param memory New memory to entangle
 * @return Number of entanglements created
 */
NIMCP_EXPORT uint32_t pr_audio_bridge_auto_entangle(
    pr_audio_bridge_t* bridge,
    pr_memory_node_t* memory);

/**
 * @brief Manually entangle two audio memories
 *
 * @param bridge Audio bridge
 * @param memory1 First memory
 * @param memory2 Second memory
 * @param edge_type Type of entanglement
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_entangle_memories(
    pr_audio_bridge_t* bridge,
    pr_memory_node_t* memory1,
    pr_memory_node_t* memory2,
    entangle_edge_type_t edge_type);

//=============================================================================
// State and Statistics Functions
//=============================================================================

/**
 * @brief Get current audio features
 *
 * @param bridge Audio bridge
 * @return Pointer to current features (do not free)
 */
NIMCP_EXPORT const pr_audio_features_t* pr_audio_bridge_get_current_features(
    const pr_audio_bridge_t* bridge);

/**
 * @brief Get current prime signature
 *
 * @param bridge Audio bridge
 * @return Pointer to current signature (do not free)
 */
NIMCP_EXPORT const prime_signature_t* pr_audio_bridge_get_current_signature(
    const pr_audio_bridge_t* bridge);

/**
 * @brief Get current quaternion state
 *
 * @param bridge Audio bridge
 * @return Current quaternion
 */
NIMCP_EXPORT nimcp_quaternion_t pr_audio_bridge_get_current_quaternion(
    const pr_audio_bridge_t* bridge);

/**
 * @brief Get current pattern detection result
 *
 * @param bridge Audio bridge
 * @return Pointer to current pattern (do not free)
 */
NIMCP_EXPORT const pr_audio_pattern_result_t* pr_audio_bridge_get_current_pattern(
    const pr_audio_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Audio bridge
 * @param stats Output statistics structure
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_get_stats(
    const pr_audio_bridge_t* bridge,
    pr_audio_bridge_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Audio bridge
 * @return PR_AUDIO_SUCCESS or error code
 */
NIMCP_EXPORT pr_audio_error_t pr_audio_bridge_reset_stats(
    pr_audio_bridge_t* bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_audio_error_string(pr_audio_error_t error);

/**
 * @brief Get audio type name as string
 *
 * @param type Audio type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* pr_audio_type_name(pr_audio_type_t type);

/**
 * @brief Get pattern type name as string
 *
 * @param pattern Pattern type
 * @return Human-readable pattern name
 */
NIMCP_EXPORT const char* pr_audio_pattern_name(pr_audio_pattern_t pattern);

/**
 * @brief Print bridge state for debugging
 *
 * @param bridge Audio bridge
 */
NIMCP_EXPORT void pr_audio_bridge_print_state(const pr_audio_bridge_t* bridge);

/**
 * @brief Print current features for debugging
 *
 * @param features Features to print
 */
NIMCP_EXPORT void pr_audio_features_print(const pr_audio_features_t* features);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_AUDIO_BRIDGE_H */
