/**
 * @file nimcp_parietal_linguistics_statistics_bridge.h
 * @brief Statistics & ML Integration Bridge for Parietal Linguistics
 * @version 1.0.0
 * @date 2026-01-31
 *
 * WHAT: Integrates NIMCP statistics/ML systems with parietal linguistics modules
 *       for HMM-based number word sequences, Bayesian reference frame selection,
 *       and information-theoretic phonological similarity
 *
 * WHY:  Linguistic processing requires probabilistic models:
 *       - Number word sequences follow Markovian structure (P("one"|"twenty-"))
 *       - Reference frame selection under ambiguity needs Bayesian inference
 *       - Phonological similarity measured via mutual information
 *
 * BIOLOGICAL BASIS:
 * - IPS neurons encode numerical magnitudes with Weber-Fechner uncertainty
 * - Angular gyrus integrates multiple reference frames probabilistically
 * - Supramarginal gyrus uses predictive coding for phonological processing
 *
 * MESH INTEGRATION:
 * - Implements linguistics_mesh_handler_t for mesh participation
 * - Contributes probabilistic beliefs with precision weighting
 * - Participates in FEP convergence for linguistic understanding
 *
 * HMM NUMBER WORD MODEL:
 * States: {units, teens, tens, hundreds, thousands, millions, ...}
 * Observations: Number words (one, two, ..., twenty, hundred, ...)
 * Transitions: P(state_t | state_{t-1}) learned from corpus
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_LINGUISTICS_STATISTICS_BRIDGE_H
#define NIMCP_PARIETAL_LINGUISTICS_STATISTICS_BRIDGE_H

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Bio-async module ID for statistics bridge */
#define BIO_MODULE_LING_STATS_BRIDGE     0x8410

/** Maximum HMM states for number word model */
#define LING_STATS_HMM_MAX_STATES        32

/** Maximum observations (number words) */
#define LING_STATS_HMM_MAX_OBS           128

/** Maximum reference frames */
#define LING_STATS_MAX_REF_FRAMES        8

/** Default precision for statistics bridge */
#define LING_STATS_DEFAULT_PRECISION     0.8f

/* ============================================================================
 * ERROR CODES
 * ============================================================================ */

#define LING_STATS_ERR_OK                0
#define LING_STATS_ERR_NULL              -1
#define LING_STATS_ERR_INVALID_STATE     -2
#define LING_STATS_ERR_HMM_FAIL          -3
#define LING_STATS_ERR_BAYES_FAIL        -4
#define LING_STATS_ERR_INFO_FAIL         -5
#define LING_STATS_ERR_NOT_INIT          -6
#define LING_STATS_ERR_MESH_REGISTER     -7

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for statistics bridge */
typedef struct ling_stats_bridge ling_stats_bridge_t;

/**
 * @brief HMM state for number word sequences
 */
typedef enum {
    NUM_HMM_STATE_START,                /**< Initial state */
    NUM_HMM_STATE_UNITS,                /**< Units (one-nine) */
    NUM_HMM_STATE_TEENS,                /**< Teens (ten-nineteen) */
    NUM_HMM_STATE_TENS,                 /**< Tens (twenty, thirty, ...) */
    NUM_HMM_STATE_TENS_UNITS,           /**< Compound (twenty-one) */
    NUM_HMM_STATE_HUNDRED,              /**< Hundred */
    NUM_HMM_STATE_HUNDRED_UNITS,        /**< After hundred (e.g., one hundred ONE) */
    NUM_HMM_STATE_THOUSAND,             /**< Thousand */
    NUM_HMM_STATE_MILLION,              /**< Million */
    NUM_HMM_STATE_BILLION,              /**< Billion */
    NUM_HMM_STATE_ORDINAL,              /**< Ordinal suffix (-th, -st, -nd, -rd) */
    NUM_HMM_STATE_END,                  /**< End of number */
    NUM_HMM_STATE_COUNT
} num_hmm_state_t;

/**
 * @brief Number word observation type
 */
typedef enum {
    NUM_OBS_ZERO,
    NUM_OBS_ONE, NUM_OBS_TWO, NUM_OBS_THREE, NUM_OBS_FOUR, NUM_OBS_FIVE,
    NUM_OBS_SIX, NUM_OBS_SEVEN, NUM_OBS_EIGHT, NUM_OBS_NINE, NUM_OBS_TEN,
    NUM_OBS_ELEVEN, NUM_OBS_TWELVE, NUM_OBS_THIRTEEN, NUM_OBS_FOURTEEN,
    NUM_OBS_FIFTEEN, NUM_OBS_SIXTEEN, NUM_OBS_SEVENTEEN, NUM_OBS_EIGHTEEN,
    NUM_OBS_NINETEEN, NUM_OBS_TWENTY, NUM_OBS_THIRTY, NUM_OBS_FORTY,
    NUM_OBS_FIFTY, NUM_OBS_SIXTY, NUM_OBS_SEVENTY, NUM_OBS_EIGHTY,
    NUM_OBS_NINETY, NUM_OBS_HUNDRED, NUM_OBS_THOUSAND, NUM_OBS_MILLION,
    NUM_OBS_BILLION, NUM_OBS_AND, NUM_OBS_HYPHEN, NUM_OBS_ORDINAL_SUFFIX,
    NUM_OBS_COUNT
} num_observation_t;

/**
 * @brief HMM decode result
 */
typedef struct {
    num_hmm_state_t* state_sequence;    /**< Decoded state sequence */
    uint32_t sequence_length;           /**< Length of sequence */
    float log_probability;              /**< Log probability of best path */
    float confidence;                   /**< Decoding confidence [0,1] */
} num_hmm_decode_result_t;

/**
 * @brief Reference frame hypothesis
 */
typedef struct {
    reference_frame_t frame;            /**< Frame type */
    float prior;                        /**< Prior probability */
    float likelihood;                   /**< Likelihood P(evidence|frame) */
    float posterior;                    /**< Posterior probability */
} ref_frame_hypothesis_t;

/**
 * @brief Bayesian reference frame selection result
 */
typedef struct {
    ref_frame_hypothesis_t hypotheses[LING_STATS_MAX_REF_FRAMES];
    uint32_t num_hypotheses;
    reference_frame_t selected_frame;   /**< MAP estimate */
    float confidence;                   /**< Selection confidence */
    float precision;                    /**< Precision for mesh */
} ref_frame_selection_result_t;

/**
 * @brief Phonological similarity result
 */
typedef struct {
    float mutual_information;           /**< MI(phoneme_a; phoneme_b) */
    float entropy_a;                    /**< H(phoneme_a) */
    float entropy_b;                    /**< H(phoneme_b) */
    float joint_entropy;                /**< H(phoneme_a, phoneme_b) */
    float normalized_mi;                /**< NMI in [0,1] */
    float similarity;                   /**< 1 - normalized MI */
    float confusability;                /**< Probability of confusion */
} phonological_similarity_t;

/**
 * @brief Statistics bridge configuration
 */
typedef struct {
    /* HMM configuration */
    float hmm_smoothing;                /**< Laplace smoothing (default: 1e-6) */
    uint32_t hmm_max_iterations;        /**< Max EM iterations (default: 100) */
    float hmm_convergence_threshold;    /**< EM convergence (default: 1e-4) */

    /* Bayesian configuration */
    float prior_egocentric;             /**< Prior P(egocentric) (default: 0.4) */
    float prior_allocentric;            /**< Prior P(allocentric) (default: 0.3) */
    float prior_intrinsic;              /**< Prior P(intrinsic) (default: 0.2) */
    float prior_relative;               /**< Prior P(relative) (default: 0.1) */

    /* Information theory configuration */
    uint32_t entropy_bins;              /**< Bins for entropy estimation (default: 32) */
    bool use_kl_divergence;             /**< Use KL for similarity (default: true) */

    /* Mesh integration */
    bool enable_mesh;                   /**< Register with mesh (default: true) */
    float mesh_learning_rate;           /**< FEP update rate (default: 0.1) */

    /* Infrastructure */
    bool enable_bbb;                    /**< Enable BBB validation (default: true) */
    bool enable_health;                 /**< Enable health monitoring (default: true) */
    bool enable_logging;                /**< Enable structured logging (default: true) */
} ling_stats_bridge_config_t;

/**
 * @brief Statistics bridge statistics
 */
typedef struct {
    uint64_t hmm_decodes;               /**< HMM Viterbi decodes */
    uint64_t hmm_forward_passes;        /**< HMM forward algorithm calls */
    uint64_t bayesian_inferences;       /**< Bayesian inference calls */
    uint64_t similarity_computations;   /**< Information theory computations */
    uint64_t mesh_contributions;        /**< Mesh belief contributions */
    uint64_t mesh_updates;              /**< FEP belief updates */

    float avg_hmm_confidence;           /**< Average HMM confidence */
    float avg_bayes_confidence;         /**< Average Bayesian confidence */
    float avg_precision;                /**< Average precision */
    float avg_latency_us;               /**< Average latency */

    uint64_t exceptions;                /**< Exceptions thrown */
} ling_stats_bridge_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default statistics bridge configuration
 *
 * @return Configuration with sensible defaults
 */
ling_stats_bridge_config_t ling_stats_bridge_default_config(void);

/**
 * @brief Create statistics bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
ling_stats_bridge_t* ling_stats_bridge_create(
    const ling_stats_bridge_config_t* config
);

/**
 * @brief Destroy statistics bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void ling_stats_bridge_destroy(ling_stats_bridge_t* bridge);

/**
 * @brief Register with linguistics mesh coordinator
 *
 * @param bridge Statistics bridge
 * @param mesh Mesh coordinator
 * @return 0 on success
 */
int ling_stats_bridge_register_mesh(
    ling_stats_bridge_t* bridge,
    linguistics_mesh_t* mesh
);

/* ============================================================================
 * HMM NUMBER WORD API
 * ============================================================================ */

/**
 * @brief Initialize HMM with default transition/emission probabilities
 *
 * @param bridge Statistics bridge
 * @return 0 on success
 */
int ling_stats_hmm_init_default(ling_stats_bridge_t* bridge);

/**
 * @brief Decode number word sequence using Viterbi algorithm
 *
 * @param bridge Statistics bridge
 * @param observations Array of observed number words
 * @param num_obs Number of observations
 * @param result Output decode result
 * @return 0 on success
 */
int ling_stats_hmm_viterbi_decode(
    ling_stats_bridge_t* bridge,
    const num_observation_t* observations,
    uint32_t num_obs,
    num_hmm_decode_result_t* result
);

/**
 * @brief Compute forward probability P(observations)
 *
 * @param bridge Statistics bridge
 * @param observations Array of observed number words
 * @param num_obs Number of observations
 * @param log_probability Output log probability
 * @return 0 on success
 */
int ling_stats_hmm_forward(
    ling_stats_bridge_t* bridge,
    const num_observation_t* observations,
    uint32_t num_obs,
    float* log_probability
);

/**
 * @brief Predict next number word given current state
 *
 * @param bridge Statistics bridge
 * @param current_state Current HMM state
 * @param predictions Output probability distribution over observations
 * @param num_predictions Size of predictions array
 * @return 0 on success
 */
int ling_stats_hmm_predict_next(
    ling_stats_bridge_t* bridge,
    num_hmm_state_t current_state,
    float* predictions,
    uint32_t num_predictions
);

/**
 * @brief Parse number word string to observation
 *
 * @param word Number word string
 * @param obs Output observation type
 * @return 0 on success, -1 if unknown
 */
int ling_stats_parse_number_word(const char* word, num_observation_t* obs);

/**
 * @brief Get observation name
 *
 * @param obs Observation type
 * @return Static string name
 */
const char* ling_stats_observation_name(num_observation_t obs);

/* ============================================================================
 * BAYESIAN REFERENCE FRAME API
 * ============================================================================ */

/**
 * @brief Select reference frame using Bayesian inference
 *
 * P(frame | evidence) ∝ P(evidence | frame) × P(frame)
 *
 * @param bridge Statistics bridge
 * @param spatial_word Spatial word (affects likelihood)
 * @param context_cues Array of context cue values [0,1]
 * @param num_cues Number of context cues
 * @param result Output selection result
 * @return 0 on success
 */
int ling_stats_bayes_select_frame(
    ling_stats_bridge_t* bridge,
    spatial_preposition_t spatial_word,
    const float* context_cues,
    uint32_t num_cues,
    ref_frame_selection_result_t* result
);

/**
 * @brief Update reference frame prior based on evidence
 *
 * @param bridge Statistics bridge
 * @param frame Frame to update
 * @param evidence_strength Strength of evidence [0,1]
 * @return 0 on success
 */
int ling_stats_bayes_update_prior(
    ling_stats_bridge_t* bridge,
    reference_frame_t frame,
    float evidence_strength
);

/**
 * @brief Get current priors for all reference frames
 *
 * @param bridge Statistics bridge
 * @param priors Output array of priors
 * @param num_frames Number of frames
 * @return 0 on success
 */
int ling_stats_bayes_get_priors(
    const ling_stats_bridge_t* bridge,
    float* priors,
    uint32_t num_frames
);

/* ============================================================================
 * INFORMATION THEORY API
 * ============================================================================ */

/**
 * @brief Compute phonological similarity using mutual information
 *
 * @param bridge Statistics bridge
 * @param phoneme_a First phoneme ID
 * @param phoneme_b Second phoneme ID
 * @param result Output similarity result
 * @return 0 on success
 */
int ling_stats_phonological_similarity(
    ling_stats_bridge_t* bridge,
    uint32_t phoneme_a,
    uint32_t phoneme_b,
    phonological_similarity_t* result
);

/**
 * @brief Compute entropy of phoneme distribution
 *
 * @param bridge Statistics bridge
 * @param distribution Probability distribution
 * @param size Distribution size
 * @param entropy Output entropy in bits
 * @return 0 on success
 */
int ling_stats_entropy(
    ling_stats_bridge_t* bridge,
    const float* distribution,
    uint32_t size,
    float* entropy
);

/**
 * @brief Compute KL divergence between distributions
 *
 * @param bridge Statistics bridge
 * @param p Distribution P
 * @param q Distribution Q
 * @param size Distribution size
 * @param kl_div Output KL divergence D_KL(P || Q)
 * @return 0 on success
 */
int ling_stats_kl_divergence(
    ling_stats_bridge_t* bridge,
    const float* p,
    const float* q,
    uint32_t size,
    float* kl_div
);

/* ============================================================================
 * MESH HANDLER INTERFACE
 * ============================================================================ */

/**
 * @brief Mesh process callback - produce statistical belief for request
 *
 * @param ctx Bridge context
 * @param request Linguistics request
 * @param belief Output belief with precision
 * @return 0 on success
 */
int ling_stats_mesh_process(
    void* ctx,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
);

/**
 * @brief Mesh update callback - FEP belief update from neighbors
 *
 * @param ctx Bridge context
 * @param neighbors Neighbor beliefs
 * @param count Number of neighbors
 * @param updated Output updated belief
 * @return 0 on success
 */
int ling_stats_mesh_update(
    void* ctx,
    const linguistics_belief_t* neighbors,
    uint32_t count,
    linguistics_belief_t* updated
);

/**
 * @brief Get current precision for mesh weighting
 *
 * @param ctx Bridge context
 * @return Precision
 */
float ling_stats_mesh_get_precision(void* ctx);

/**
 * @brief Get mesh handler interface
 *
 * @param bridge Statistics bridge
 * @param handler Output handler struct
 * @return 0 on success
 */
int ling_stats_get_mesh_handler(
    ling_stats_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Statistics bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int ling_stats_bridge_get_stats(
    const ling_stats_bridge_t* bridge,
    ling_stats_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Statistics bridge
 */
void ling_stats_bridge_reset_stats(ling_stats_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* ling_stats_bridge_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_LINGUISTICS_STATISTICS_BRIDGE_H */
