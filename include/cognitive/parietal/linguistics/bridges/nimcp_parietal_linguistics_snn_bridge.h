/**
 * @file nimcp_parietal_linguistics_snn_bridge.h
 * @brief SNN Integration Bridge for Parietal Linguistics
 * @version 1.0.0
 * @date 2026-01-31
 *
 * WHAT: Integrates NIMCP spiking neural network system with parietal linguistics
 *       for spike-based encoding of phonemes, spatial words, and number words
 *
 * WHY:  Biological language processing uses spike-timing for precise phoneme
 *       representation, temporal coding for word sequences, and population
 *       coding for semantic features
 *
 * BIOLOGICAL BASIS:
 * - Auditory cortex encodes phonemes with precise spike timing
 * - Angular gyrus uses population codes for spatial word meaning
 * - Number sense (IPS) uses rate coding correlated with magnitude
 * - Phonological loop uses recurrent dynamics with theta-gamma coupling
 *
 * MESH INTEGRATION:
 * - Implements linguistics_mesh_handler_t for mesh participation
 * - Contributes spike-based beliefs with precision weighting
 * - Population firing rate stability determines precision
 *
 * POPULATIONS:
 * - Phoneme Population: 44 IPA phonemes with temporal coding
 * - Spatial Word Population: ~30 prepositions with population coding
 * - Number Word Population: Cardinals/ordinals with rate coding
 * - Reference Frame Population: Ego/allo/intrinsic with population coding
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_LINGUISTICS_SNN_BRIDGE_H
#define NIMCP_PARIETAL_LINGUISTICS_SNN_BRIDGE_H

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_encoding.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Bio-async module ID for SNN bridge */
#define BIO_MODULE_LING_SNN_BRIDGE      0x8290

/** Number of IPA phonemes in English */
#define LING_SNN_NUM_PHONEMES           44

/** Number of spatial prepositions */
#define LING_SNN_NUM_SPATIAL_WORDS      ((uint32_t)SPATIAL_PREPOSITION_COUNT)

/** Number of reference frames */
#define LING_SNN_NUM_REFERENCE_FRAMES   4

/** Number of number word types */
#define LING_SNN_NUM_NUMBER_TYPES       5

/** Neurons per phoneme (temporal population coding) */
#define LING_SNN_NEURONS_PER_PHONEME    8

/** Neurons per spatial word (population coding) */
#define LING_SNN_NEURONS_PER_SPATIAL    16

/** Neurons per number word (rate coding) */
#define LING_SNN_NEURONS_PER_NUMBER     8

/** Default precision for SNN bridge */
#define LING_SNN_DEFAULT_PRECISION      0.85f

/** Precision floor for unstable firing */
#define LING_SNN_PRECISION_FLOOR        0.1f

/** Precision ceiling for stable firing */
#define LING_SNN_PRECISION_CEILING      0.99f

/* ============================================================================
 * ERROR CODES
 * ============================================================================ */

#define LING_SNN_ERR_OK                 0
#define LING_SNN_ERR_NULL               -1
#define LING_SNN_ERR_INVALID_PHONEME    -2
#define LING_SNN_ERR_INVALID_WORD       -3
#define LING_SNN_ERR_ENCODING_FAILED    -4
#define LING_SNN_ERR_DECODING_FAILED    -5
#define LING_SNN_ERR_NOT_INIT           -6
#define LING_SNN_ERR_MESH_REGISTER      -7
#define LING_SNN_ERR_POPULATION_FAILED  -8
#define LING_SNN_ERR_SIM_FAILED         -9

/* ============================================================================
 * PHONEME ENUMERATION (44 IPA English Phonemes)
 * ============================================================================ */

typedef enum {
    /* Vowels (12) */
    LING_PHONEME_IY = 0,     /**< "ee" as in "beat" */
    LING_PHONEME_IH,         /**< "i" as in "bit" */
    LING_PHONEME_EY,         /**< "ay" as in "bait" */
    LING_PHONEME_EH,         /**< "e" as in "bet" */
    LING_PHONEME_AE,         /**< "a" as in "bat" */
    LING_PHONEME_AA,         /**< "ah" as in "bot" */
    LING_PHONEME_AO,         /**< "aw" as in "bought" */
    LING_PHONEME_OW,         /**< "oh" as in "boat" */
    LING_PHONEME_UH,         /**< "oo" as in "book" */
    LING_PHONEME_UW,         /**< "oo" as in "boot" */
    LING_PHONEME_AH,         /**< "uh" as in "but" */
    LING_PHONEME_ER,         /**< "er" as in "bird" */

    /* Stops (6) */
    LING_PHONEME_P,          /**< "p" as in "pat" */
    LING_PHONEME_B,          /**< "b" as in "bat" */
    LING_PHONEME_T,          /**< "t" as in "tap" */
    LING_PHONEME_D,          /**< "d" as in "dap" */
    LING_PHONEME_K,          /**< "k" as in "cap" */
    LING_PHONEME_G,          /**< "g" as in "gap" */

    /* Fricatives (9) */
    LING_PHONEME_F,          /**< "f" as in "fat" */
    LING_PHONEME_V,          /**< "v" as in "vat" */
    LING_PHONEME_TH,         /**< "th" as in "thin" */
    LING_PHONEME_DH,         /**< "th" as in "this" */
    LING_PHONEME_S,          /**< "s" as in "sat" */
    LING_PHONEME_Z,          /**< "z" as in "zap" */
    LING_PHONEME_SH,         /**< "sh" as in "ship" */
    LING_PHONEME_ZH,         /**< "zh" as in "measure" */
    LING_PHONEME_H,          /**< "h" as in "hat" */

    /* Nasals (3) */
    LING_PHONEME_M,          /**< "m" as in "mat" */
    LING_PHONEME_N,          /**< "n" as in "nat" */
    LING_PHONEME_NG,         /**< "ng" as in "sing" */

    /* Approximants (4) */
    LING_PHONEME_L,          /**< "l" as in "lap" */
    LING_PHONEME_R,          /**< "r" as in "rap" */
    LING_PHONEME_W,          /**< "w" as in "wap" */
    LING_PHONEME_Y,          /**< "y" as in "yap" */

    /* Affricates (2) */
    LING_PHONEME_CH,         /**< "ch" as in "chat" */
    LING_PHONEME_JH,         /**< "j" as in "jam" */

    /* Diphthongs (6) */
    LING_PHONEME_AY,         /**< "ai" as in "buy" */
    LING_PHONEME_AW,         /**< "ow" as in "cow" */
    LING_PHONEME_OY,         /**< "oy" as in "boy" */
    LING_PHONEME_EYR,        /**< "air" as in "fair" */
    LING_PHONEME_IYR,        /**< "ear" as in "fear" */
    LING_PHONEME_UWR,        /**< "oor" as in "poor" */

    /* Special */
    LING_PHONEME_SILENCE,    /**< Silence */
    LING_PHONEME_UNKNOWN,    /**< Unknown phoneme */

    LING_PHONEME_COUNT
} ling_phoneme_t;

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for SNN bridge */
typedef struct ling_snn_bridge ling_snn_bridge_t;

/**
 * @brief Spike train for linguistic unit
 */
typedef struct {
    uint64_t spike_times[64];       /**< Spike times in microseconds */
    uint32_t spike_count;           /**< Number of spikes */
    float inst_rate;                /**< Instantaneous firing rate (Hz) */
    float avg_rate;                 /**< Average firing rate (Hz) */
} ling_spike_train_t;

/**
 * @brief Phoneme spike encoding result
 */
typedef struct {
    ling_phoneme_t phoneme;         /**< Encoded phoneme */
    ling_spike_train_t spike_train; /**< Generated spike train */
    float encoding_confidence;      /**< Encoding confidence [0,1] */
    float duration_ms;              /**< Phoneme duration */
    float formants[4];              /**< F1-F4 formant frequencies */
} ling_phoneme_encoding_t;

/**
 * @brief Spatial word spike encoding result
 */
typedef struct {
    spatial_preposition_t preposition;  /**< Encoded preposition */
    float population_activity[LING_SNN_NUM_SPATIAL_WORDS]; /**< Population firing rates */
    float winner_rate;              /**< Winning population rate */
    uint32_t winner_idx;            /**< Index of winning population */
    float encoding_precision;       /**< Encoding precision */
} ling_spatial_encoding_t;

/**
 * @brief Number word spike encoding result
 */
typedef struct {
    number_word_type_t type;        /**< Number word type */
    float magnitude;                /**< Encoded magnitude */
    float firing_rate;              /**< Rate coding (Weber-Fechner scaled) */
    float uncertainty;              /**< Weber-Fechner uncertainty */
    bool is_approximate;            /**< Approximate quantity */
} ling_number_encoding_t;

/**
 * @brief SNN bridge configuration
 */
typedef struct {
    /* Population settings */
    uint32_t neurons_per_phoneme;   /**< Neurons per phoneme (default: 8) */
    uint32_t neurons_per_spatial;   /**< Neurons per spatial word (default: 16) */
    uint32_t neurons_per_number;    /**< Neurons per number type (default: 8) */

    /* Encoding settings */
    snn_encoding_t phoneme_encoding;    /**< Phoneme encoding method (default: TEMPORAL) */
    snn_encoding_t spatial_encoding;    /**< Spatial encoding method (default: POPULATION) */
    snn_encoding_t number_encoding;     /**< Number encoding method (default: RATE) */

    /* Topology settings */
    snn_topology_t phoneme_topology;    /**< Phoneme population topology (default: RECURRENT) */
    snn_topology_t spatial_topology;    /**< Spatial population topology (default: FEEDFORWARD) */

    /* Firing rate settings */
    float max_firing_rate;          /**< Maximum firing rate Hz (default: 100) */
    float min_firing_rate;          /**< Minimum firing rate Hz (default: 1) */

    /* Precision settings */
    float base_precision;           /**< Base precision (default: 0.85) */
    float rate_stability_weight;    /**< Weight for rate stability (default: 0.5) */

    /* Simulation settings */
    float dt_ms;                    /**< Simulation timestep (default: 0.1) */
    float encoding_window_ms;       /**< Encoding window (default: 50) */

    /* Mesh integration */
    bool enable_mesh;               /**< Register with mesh (default: true) */

    /* Infrastructure */
    bool enable_stdp;               /**< Enable STDP learning (default: true) */
    bool enable_health;             /**< Enable health monitoring (default: true) */
    bool enable_logging;            /**< Enable logging (default: true) */
} ling_snn_bridge_config_t;

/**
 * @brief SNN bridge statistics
 */
typedef struct {
    uint64_t total_encodings;       /**< Total encode operations */
    uint64_t total_decodings;       /**< Total decode operations */
    uint64_t total_spikes;          /**< Total spikes generated */
    uint64_t mesh_contributions;    /**< Mesh belief contributions */

    float avg_phoneme_rate;         /**< Average phoneme firing rate */
    float avg_spatial_rate;         /**< Average spatial firing rate */
    float avg_number_rate;          /**< Average number firing rate */
    float avg_precision;            /**< Average precision */
    float avg_latency_us;           /**< Average encoding latency */

    uint32_t silent_populations;    /**< Number of silent populations */
    uint32_t hyperactive_populations; /**< Number of hyperactive populations */
    snn_state_health_t health;      /**< Network health status */
} ling_snn_bridge_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default SNN bridge configuration
 *
 * @return Configuration with sensible defaults
 */
ling_snn_bridge_config_t ling_snn_bridge_default_config(void);

/**
 * @brief Create SNN bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
ling_snn_bridge_t* ling_snn_bridge_create(
    const ling_snn_bridge_config_t* config
);

/**
 * @brief Destroy SNN bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void ling_snn_bridge_destroy(ling_snn_bridge_t* bridge);

/**
 * @brief Register with linguistics mesh coordinator
 *
 * @param bridge SNN bridge
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int ling_snn_bridge_register_mesh(
    ling_snn_bridge_t* bridge,
    linguistics_mesh_t* mesh
);

/* ============================================================================
 * PHONEME ENCODING API
 * ============================================================================ */

/**
 * @brief Encode phoneme to spike train
 *
 * Uses temporal coding with precise spike timing to represent phoneme identity.
 *
 * @param bridge SNN bridge
 * @param phoneme Phoneme to encode
 * @param duration_ms Phoneme duration
 * @param result Output encoding result
 * @return 0 on success
 */
int ling_snn_encode_phoneme(
    ling_snn_bridge_t* bridge,
    ling_phoneme_t phoneme,
    float duration_ms,
    ling_phoneme_encoding_t* result
);

/**
 * @brief Encode phoneme sequence (word)
 *
 * @param bridge SNN bridge
 * @param phonemes Array of phonemes
 * @param durations Array of durations
 * @param count Number of phonemes
 * @param results Output array of encoding results
 * @return 0 on success
 */
int ling_snn_encode_phoneme_sequence(
    ling_snn_bridge_t* bridge,
    const ling_phoneme_t* phonemes,
    const float* durations,
    uint32_t count,
    ling_phoneme_encoding_t* results
);

/**
 * @brief Decode spike train to phoneme
 *
 * @param bridge SNN bridge
 * @param spike_train Input spike train
 * @param phoneme Output decoded phoneme
 * @param confidence Output confidence
 * @return 0 on success
 */
int ling_snn_decode_phoneme(
    ling_snn_bridge_t* bridge,
    const ling_spike_train_t* spike_train,
    ling_phoneme_t* phoneme,
    float* confidence
);

/* ============================================================================
 * SPATIAL WORD ENCODING API
 * ============================================================================ */

/**
 * @brief Encode spatial preposition to population activity
 *
 * Uses population coding with Gaussian tuning curves.
 *
 * @param bridge SNN bridge
 * @param preposition Preposition to encode
 * @param activation Activation level [0,1]
 * @param result Output encoding result
 * @return 0 on success
 */
int ling_snn_encode_spatial_word(
    ling_snn_bridge_t* bridge,
    spatial_preposition_t preposition,
    float activation,
    ling_spatial_encoding_t* result
);

/**
 * @brief Decode population activity to spatial preposition
 *
 * @param bridge SNN bridge
 * @param population_activity Input population activity
 * @param preposition Output decoded preposition
 * @param confidence Output confidence
 * @return 0 on success
 */
int ling_snn_decode_spatial_word(
    ling_snn_bridge_t* bridge,
    const float* population_activity,
    spatial_preposition_t* preposition,
    float* confidence
);

/**
 * @brief Get spatial word population activity
 *
 * @param bridge SNN bridge
 * @param preposition Preposition to query
 * @param activity Output activity array
 * @param size Size of activity array
 * @return 0 on success
 */
int ling_snn_get_spatial_population(
    const ling_snn_bridge_t* bridge,
    spatial_preposition_t preposition,
    float* activity,
    uint32_t size
);

/* ============================================================================
 * NUMBER WORD ENCODING API
 * ============================================================================ */

/**
 * @brief Encode number to firing rate (Weber-Fechner scaling)
 *
 * Uses rate coding with logarithmic magnitude mapping per Weber-Fechner law.
 *
 * @param bridge SNN bridge
 * @param magnitude Number magnitude
 * @param type Number word type
 * @param result Output encoding result
 * @return 0 on success
 */
int ling_snn_encode_number(
    ling_snn_bridge_t* bridge,
    float magnitude,
    number_word_type_t type,
    ling_number_encoding_t* result
);

/**
 * @brief Decode firing rate to number magnitude
 *
 * @param bridge SNN bridge
 * @param firing_rate Input firing rate
 * @param type Number word type
 * @param magnitude Output magnitude
 * @param uncertainty Output Weber-Fechner uncertainty
 * @return 0 on success
 */
int ling_snn_decode_number(
    ling_snn_bridge_t* bridge,
    float firing_rate,
    number_word_type_t type,
    float* magnitude,
    float* uncertainty
);

/* ============================================================================
 * POPULATION MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Step phoneme population simulation
 *
 * @param bridge SNN bridge
 * @param dt_ms Timestep in milliseconds
 * @return 0 on success
 */
int ling_snn_step_phoneme_population(
    ling_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Step spatial population simulation
 *
 * @param bridge SNN bridge
 * @param dt_ms Timestep in milliseconds
 * @return 0 on success
 */
int ling_snn_step_spatial_population(
    ling_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Step number population simulation
 *
 * @param bridge SNN bridge
 * @param dt_ms Timestep in milliseconds
 * @return 0 on success
 */
int ling_snn_step_number_population(
    ling_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Get population firing rate
 *
 * @param bridge SNN bridge
 * @param population_id Population identifier (0=phoneme, 1=spatial, 2=number)
 * @return Mean firing rate in Hz
 */
float ling_snn_get_population_rate(
    const ling_snn_bridge_t* bridge,
    uint32_t population_id
);

/**
 * @brief Get population synchrony
 *
 * @param bridge SNN bridge
 * @param population_id Population identifier
 * @return Synchrony measure [0,1]
 */
float ling_snn_get_population_synchrony(
    const ling_snn_bridge_t* bridge,
    uint32_t population_id
);

/* ============================================================================
 * MESH HANDLER INTERFACE
 * ============================================================================ */

/**
 * @brief Mesh process callback - produce spike-based belief for request
 *
 * Implements linguistics_mesh_handler_t::process
 *
 * @param ctx Bridge context
 * @param request Linguistics request
 * @param belief Output belief with precision
 * @return 0 on success
 */
int ling_snn_mesh_process(
    void* ctx,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
);

/**
 * @brief Mesh update callback - FEP belief update from neighbors
 *
 * Implements linguistics_mesh_handler_t::update
 *
 * @param ctx Bridge context
 * @param neighbors Neighbor beliefs
 * @param count Number of neighbors
 * @param updated Output updated belief
 * @return 0 on success
 */
int ling_snn_mesh_update(
    void* ctx,
    const linguistics_belief_t* neighbors,
    uint32_t count,
    linguistics_belief_t* updated
);

/**
 * @brief Get current precision for mesh weighting
 *
 * Precision based on population firing rate stability.
 *
 * @param ctx Bridge context
 * @return Precision in [PRECISION_FLOOR, PRECISION_CEILING]
 */
float ling_snn_mesh_get_precision(void* ctx);

/**
 * @brief Get mesh handler interface
 *
 * @param bridge SNN bridge
 * @param handler Output handler struct
 * @return 0 on success
 */
int ling_snn_get_mesh_handler(
    ling_snn_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge SNN bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int ling_snn_bridge_get_stats(
    const ling_snn_bridge_t* bridge,
    ling_snn_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge SNN bridge
 */
void ling_snn_bridge_reset_stats(ling_snn_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* ling_snn_bridge_get_last_error(void);

/* ============================================================================
 * UTILITY API
 * ============================================================================ */

/**
 * @brief Get phoneme name string
 *
 * @param phoneme Phoneme type
 * @return Static string name (IPA symbol)
 */
const char* ling_snn_phoneme_name(ling_phoneme_t phoneme);

/**
 * @brief Parse phoneme from IPA string
 *
 * @param str IPA string
 * @param phoneme Output phoneme
 * @return 0 on success, -1 if unknown
 */
int ling_snn_parse_phoneme(const char* str, ling_phoneme_t* phoneme);

/**
 * @brief Get phoneme class (vowel, stop, fricative, etc.)
 *
 * @param phoneme Phoneme
 * @return Class string
 */
const char* ling_snn_phoneme_class(ling_phoneme_t phoneme);

/**
 * @brief Check if phoneme is voiced
 *
 * @param phoneme Phoneme
 * @return true if voiced
 */
bool ling_snn_phoneme_is_voiced(ling_phoneme_t phoneme);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_LINGUISTICS_SNN_BRIDGE_H */
