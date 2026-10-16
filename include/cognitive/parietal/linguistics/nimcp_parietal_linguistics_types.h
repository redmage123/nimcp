/**
 * @file nimcp_parietal_linguistics_types.h
 * @brief Shared types for parietal linguistics modules
 * @version 1.0.0
 * @date 2025-01-31
 *
 * WHAT: Shared enums, constants, and structures for the three parietal
 *       linguistics sub-modules (spatial language, numerical language,
 *       phonological working memory)
 *
 * WHY:  Provide common type definitions to ensure consistency across
 *       modules while avoiding circular dependencies
 *
 * BIOLOGICAL BASIS:
 * The parietal lobe contains specialized language processing regions:
 * - Angular Gyrus (BA39): Spatial-semantic mapping, metaphor processing
 * - Supramarginal Gyrus (BA40): Phonological working memory, subvocal rehearsal
 * - Intraparietal Sulcus (IPS): Number word ↔ magnitude mapping
 *
 * MESH ARCHITECTURE:
 * All linguistics modules participate in a mesh network using:
 * - Gossip belief propagation for peer-to-peer communication
 * - FEP convergence for distributed consensus
 * - Precision weighting (Π = 1/σ²) for trust
 * - CRDT workspace for conflict resolution
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_LINGUISTICS_TYPES_H
#define NIMCP_PARIETAL_LINGUISTICS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * BIO-ASYNC MODULE IDs
 * ============================================================================ */

/** Spatial Language module (Angular Gyrus) */
#define BIO_MODULE_SPATIAL_LANGUAGE             0x0826

/** Numerical Language module (Intraparietal Sulcus) */
#define BIO_MODULE_NUMERICAL_LANGUAGE           0x0827

/** Phonological Working Memory module (Supramarginal Gyrus) */
#define BIO_MODULE_PHONOLOGICAL_WM              0x0828

/** Linguistics SNN Bridge */
#define BIO_MODULE_LINGUISTICS_SNN_BRIDGE       0x0829

/** Linguistics Plasticity Bridge */
#define BIO_MODULE_LINGUISTICS_PLASTICITY_BRIDGE 0x082A

/** Linguistics Training Bridge */
#define BIO_MODULE_LINGUISTICS_TRAINING_BRIDGE  0x082B

/** Linguistics Hypothalamus Bridge */
#define BIO_MODULE_LINGUISTICS_HYPOTHALAMUS_BRIDGE 0x082C

/** Linguistics Omni/World Model Bridge */
#define BIO_MODULE_LINGUISTICS_OMNI_BRIDGE      0x082D

/** Linguistics Engram Bridges */
#define BIO_MODULE_LINGUISTICS_ENGRAM_BRIDGE    0x082E

/** Linguistics Math Bridge */
#define BIO_MODULE_LINGUISTICS_MATH_BRIDGE      0x082F

/** Linguistics Quantum Bridge */
#define BIO_MODULE_LINGUISTICS_QUANTUM_BRIDGE   0x0830

/** Linguistics Health Bridge */
#define BIO_MODULE_LINGUISTICS_HEALTH_BRIDGE    0x0831

/** Linguistics KG Bridge */
#define BIO_MODULE_LINGUISTICS_KG_BRIDGE        0x0832

/** Linguistics Logging Bridge */
#define BIO_MODULE_LINGUISTICS_LOGGING_BRIDGE   0x0833

/** Linguistics Symbolic Bridge */
#define BIO_MODULE_LINGUISTICS_SYMBOLIC_BRIDGE  0x0834

/** Linguistics Basal Ganglia Bridge */
#define BIO_MODULE_LINGUISTICS_BG_BRIDGE        0x0835

/** Linguistics Cerebellum Bridge */
#define BIO_MODULE_LINGUISTICS_CEREBELLUM_BRIDGE 0x0836

/** Linguistics Medulla Bridge */
#define BIO_MODULE_LINGUISTICS_MEDULLA_BRIDGE   0x0837

/** Linguistics Perception Bridge */
#define BIO_MODULE_LINGUISTICS_PERCEPTION_BRIDGE 0x0838

/** Linguistics ToM+WM Bridge */
#define BIO_MODULE_LINGUISTICS_TOM_WM_BRIDGE    0x0839

/** Linguistics Mesh Coordinator */
#define BIO_MODULE_LINGUISTICS_MESH_COORDINATOR 0x083A

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum word length for linguistics processing */
#define LINGUISTICS_MAX_WORD_LENGTH             128

/** Maximum phoneme sequence length */
#define LINGUISTICS_MAX_PHONEME_SEQUENCE        64

/** Maximum phonological buffer capacity (Miller's 7±2) */
#define LINGUISTICS_PHONOLOGICAL_BUFFER_SIZE    9

/** Default phonological trace decay time (ms) */
#define LINGUISTICS_TRACE_DECAY_MS              2000

/** Maximum mesh participants */
#define LINGUISTICS_MAX_MESH_PARTICIPANTS       32

/** Default mesh convergence threshold (agreement score) */
#define LINGUISTICS_MESH_AGREEMENT_THRESHOLD    0.75f

/** Default mesh max iterations */
#define LINGUISTICS_MESH_MAX_ITERATIONS         100

/** Default FEP belief learning rate */
#define LINGUISTICS_FEP_LEARNING_RATE           0.1f

/** Default gossip probability */
#define LINGUISTICS_GOSSIP_PROBABILITY          0.3f

/** Precision floor (minimum allowed precision) */
#define LINGUISTICS_PRECISION_FLOOR             0.01f

/** Precision ceiling (maximum allowed precision) */
#define LINGUISTICS_PRECISION_CEILING           100.0f

/** Belief vector dimension */
#define LINGUISTICS_BELIEF_VEC_SIZE             16

/** Maximum spatial prepositions */
#define LINGUISTICS_MAX_SPATIAL_PREPOSITIONS    64

/** Maximum number words */
#define LINGUISTICS_MAX_NUMBER_WORDS            256

/** Maximum quantifiers */
#define LINGUISTICS_MAX_QUANTIFIERS             32

/** Number of IPA phonemes (English) */
#define LINGUISTICS_IPA_PHONEME_COUNT           44

/* ============================================================================
 * ERROR CODES
 * ============================================================================ */

/** Linguistics error code base */
#define LINGUISTICS_ERROR_BASE                  30000

#define LING_ERR_OK                             0
#define LING_ERR_NULL_POINTER                   (LINGUISTICS_ERROR_BASE + 1)
#define LING_ERR_INVALID_PARAM                  (LINGUISTICS_ERROR_BASE + 2)
#define LING_ERR_UNKNOWN_WORD                   (LINGUISTICS_ERROR_BASE + 3)
#define LING_ERR_UNKNOWN_PREPOSITION            (LINGUISTICS_ERROR_BASE + 4)
#define LING_ERR_UNKNOWN_NUMBER_WORD            (LINGUISTICS_ERROR_BASE + 5)
#define LING_ERR_BUFFER_OVERFLOW                (LINGUISTICS_ERROR_BASE + 6)
#define LING_ERR_BUFFER_EMPTY                   (LINGUISTICS_ERROR_BASE + 7)
#define LING_ERR_ALLOC_FAILED                   (LINGUISTICS_ERROR_BASE + 8)
#define LING_ERR_INVALID_PHONEME                (LINGUISTICS_ERROR_BASE + 9)
#define LING_ERR_INVALID_FRAME                  (LINGUISTICS_ERROR_BASE + 10)
#define LING_ERR_INVALID_QUANTIFIER             (LINGUISTICS_ERROR_BASE + 11)
#define LING_ERR_MESH_NOT_READY                 (LINGUISTICS_ERROR_BASE + 12)
#define LING_ERR_MESH_TIMEOUT                   (LINGUISTICS_ERROR_BASE + 13)
#define LING_ERR_MESH_DEADLOCK                  (LINGUISTICS_ERROR_BASE + 14)
#define LING_ERR_CONVERGENCE_FAILED             (LINGUISTICS_ERROR_BASE + 15)
#define LING_ERR_BBB_BLOCKED                    (LINGUISTICS_ERROR_BASE + 16)
#define LING_ERR_SAFETY_BLOCKED                 (LINGUISTICS_ERROR_BASE + 17)
#define LING_ERR_NOT_INITIALIZED                (LINGUISTICS_ERROR_BASE + 18)
#define LING_ERR_REHEARSAL_FAILED               (LINGUISTICS_ERROR_BASE + 19)
#define LING_ERR_ENCODING_FAILED                (LINGUISTICS_ERROR_BASE + 20)

/* ============================================================================
 * SPATIAL LANGUAGE TYPES
 * ============================================================================ */

/**
 * @brief Spatial preposition categories
 *
 * 30+ prepositions organized by semantic category.
 * Each maps to fuzzy membership functions for distance/angle.
 */
typedef enum {
    /* Proximity */
    SPATIAL_PREP_NEAR,              /**< Near/close to */
    SPATIAL_PREP_FAR,               /**< Far from */
    SPATIAL_PREP_ADJACENT,          /**< Adjacent to */
    SPATIAL_PREP_BESIDE,            /**< Beside */
    SPATIAL_PREP_BY,                /**< By/next to */

    /* Horizontal relations */
    SPATIAL_PREP_LEFT,              /**< Left of */
    SPATIAL_PREP_RIGHT,             /**< Right of */
    SPATIAL_PREP_FRONT,             /**< In front of */
    SPATIAL_PREP_BEHIND,            /**< Behind */

    /* Vertical relations */
    SPATIAL_PREP_ABOVE,             /**< Above */
    SPATIAL_PREP_BELOW,             /**< Below */
    SPATIAL_PREP_OVER,              /**< Over (with contact) */
    SPATIAL_PREP_UNDER,             /**< Under (with contact) */
    SPATIAL_PREP_ON,                /**< On (surface contact) */
    SPATIAL_PREP_BENEATH,           /**< Beneath */

    /* Containment */
    SPATIAL_PREP_IN,                /**< Inside */
    SPATIAL_PREP_INSIDE,            /**< Inside (explicit) */
    SPATIAL_PREP_OUTSIDE,           /**< Outside */
    SPATIAL_PREP_WITHIN,            /**< Within (bounded) */

    /* Path/trajectory */
    SPATIAL_PREP_THROUGH,           /**< Through */
    SPATIAL_PREP_ACROSS,            /**< Across */
    SPATIAL_PREP_ALONG,             /**< Along */
    SPATIAL_PREP_TOWARD,            /**< Toward */
    SPATIAL_PREP_AWAY,              /**< Away from */
    SPATIAL_PREP_INTO,              /**< Into */
    SPATIAL_PREP_OUT_OF,            /**< Out of */

    /* Complex relations */
    SPATIAL_PREP_BETWEEN,           /**< Between (two refs) */
    SPATIAL_PREP_AMONG,             /**< Among (multiple refs) */
    SPATIAL_PREP_AROUND,            /**< Around */
    SPATIAL_PREP_OPPOSITE,          /**< Opposite to */

    /* Boundary relations */
    SPATIAL_PREP_AT,                /**< At (point location) */
    SPATIAL_PREP_AGAINST,           /**< Against (touching) */

    SPATIAL_PREPOSITION_COUNT
} spatial_preposition_t;

/**
 * @brief Spatial reference frame types
 *
 * Different coordinate systems for interpreting spatial language.
 * Frame selection depends on context, speaker/listener perspective.
 */
typedef enum {
    REF_FRAME_EGOCENTRIC,           /**< Body-centered (viewer's perspective) */
    REF_FRAME_ALLOCENTRIC,          /**< World-centered (absolute directions) */
    REF_FRAME_INTRINSIC,            /**< Object-centered (inherent front/back) */
    REF_FRAME_RELATIVE,             /**< Speaker-relative (deictic) */
    REF_FRAME_GEOGRAPHIC,           /**< Cardinal directions (N/S/E/W) */
    REF_FRAME_COUNT
} reference_frame_t;

/**
 * @brief Spatial semantics result
 *
 * Complete semantic representation of a spatial expression
 * with fuzzy membership functions and reference frame.
 */
typedef struct {
    spatial_preposition_t preposition;  /**< Parsed preposition */
    reference_frame_t frame;            /**< Selected reference frame */
    float distance_membership;          /**< Fuzzy distance degree [0,1] */
    float angle_membership;             /**< Fuzzy angle degree [0,1] */
    float frame_confidence;             /**< Bayesian posterior for frame */
    float overall_confidence;           /**< Overall interpretation confidence */

    /* Spatial vector representation */
    float direction[3];                 /**< Unit direction vector (x,y,z) */
    float distance_center;              /**< MF center distance (meters) */
    float distance_spread;              /**< MF spread (sigma) */

    /* Hedge modification */
    bool hedge_applied;                 /**< Whether a hedge was applied */
    uint8_t hedge_type;                 /**< Applied hedge (FUZZY_HEDGE_*) */
} spatial_semantics_t;

/* ============================================================================
 * NUMERICAL LANGUAGE TYPES
 * ============================================================================ */

/**
 * @brief Number word types
 */
typedef enum {
    NUM_WORD_CARDINAL,              /**< Cardinal: one, two, three... */
    NUM_WORD_ORDINAL,               /**< Ordinal: first, second, third... */
    NUM_WORD_MULTIPLIER,            /**< Multiplier: double, triple... */
    NUM_WORD_FRACTION,              /**< Fraction: half, third, quarter... */
    NUM_WORD_APPROXIMATE,           /**< Approximate: few, several, many... */
    NUM_WORD_TYPE_COUNT
} number_word_type_t;

/**
 * @brief Linguistic quantifier types
 */
typedef enum {
    QUANTIFIER_UNIVERSAL,           /**< Universal: all, every, each */
    QUANTIFIER_EXISTENTIAL,         /**< Existential: some, a, an */
    QUANTIFIER_NEGATIVE,            /**< Negative: no, none, neither */
    QUANTIFIER_PROPORTIONAL,        /**< Proportional: most, few, many, half */
    QUANTIFIER_COUNT
} linguistic_quantifier_t;

/**
 * @brief Numerical semantics result
 */
typedef struct {
    number_word_type_t type;        /**< Type of number word */
    float magnitude;                /**< Exact or approximate magnitude */
    float uncertainty;              /**< Weber-Fechner derived uncertainty */
    float confidence;               /**< Parse confidence [0,1] */
    bool is_approximate;            /**< True if fuzzy quantity */

    /* For ordinals */
    uint32_t ordinal_position;      /**< Position (1-indexed) */

    /* For fractions */
    uint32_t numerator;             /**< Fraction numerator */
    uint32_t denominator;           /**< Fraction denominator */

    /* For quantifiers */
    linguistic_quantifier_t quantifier; /**< Quantifier type if applicable */
} numerical_semantics_t;

/* ============================================================================
 * PHONOLOGICAL WORKING MEMORY TYPES
 * ============================================================================ */

/**
 * @brief IPA phoneme categories (English, 44 phonemes)
 */
typedef enum {
    /* Vowels (12) */
    PHONEME_IY,     /**< /i:/ as in "beet" */
    PHONEME_IH,     /**< /I/ as in "bit" */
    PHONEME_EY,     /**< /eI/ as in "bait" */
    PHONEME_EH,     /**< /E/ as in "bet" */
    PHONEME_AE,     /**< /ae/ as in "bat" */
    PHONEME_AA,     /**< /A:/ as in "bot" (father) */
    PHONEME_AO,     /**< /O:/ as in "bought" */
    PHONEME_OW,     /**< /oU/ as in "boat" */
    PHONEME_UH,     /**< /U/ as in "book" */
    PHONEME_UW,     /**< /u:/ as in "boot" */
    PHONEME_AH,     /**< /V/ as in "but" */
    PHONEME_ER,     /**< /3:/ as in "bird" */

    /* Stops (6) */
    PHONEME_P,      /**< /p/ as in "pat" */
    PHONEME_B,      /**< /b/ as in "bat" */
    PHONEME_T,      /**< /t/ as in "tap" */
    PHONEME_D,      /**< /d/ as in "dab" */
    PHONEME_K,      /**< /k/ as in "cat" */
    PHONEME_G,      /**< /g/ as in "gap" */

    /* Fricatives (9) */
    PHONEME_F,      /**< /f/ as in "fat" */
    PHONEME_V,      /**< /v/ as in "vat" */
    PHONEME_TH,     /**< /T/ as in "thin" */
    PHONEME_DH,     /**< /D/ as in "this" */
    PHONEME_S,      /**< /s/ as in "sat" */
    PHONEME_Z,      /**< /z/ as in "zap" */
    PHONEME_SH,     /**< /S/ as in "ship" */
    PHONEME_ZH,     /**< /Z/ as in "measure" */
    PHONEME_HH,     /**< /h/ as in "hat" */

    /* Nasals (3) */
    PHONEME_M,      /**< /m/ as in "mat" */
    PHONEME_N,      /**< /n/ as in "nat" */
    PHONEME_NG,     /**< /N/ as in "sing" */

    /* Approximants (4) */
    PHONEME_L,      /**< /l/ as in "lap" */
    PHONEME_R,      /**< /r/ as in "rap" */
    PHONEME_W,      /**< /w/ as in "wag" */
    PHONEME_Y,      /**< /j/ as in "yap" */

    /* Affricates (2) */
    PHONEME_CH,     /**< /tS/ as in "chat" */
    PHONEME_JH,     /**< /dZ/ as in "judge" */

    /* Diphthongs (6) - additional */
    PHONEME_AY,     /**< /aI/ as in "buy" */
    PHONEME_AW,     /**< /aU/ as in "how" */
    PHONEME_OY,     /**< /OI/ as in "boy" */
    PHONEME_EY2,    /**< /eI/ as in "say" (variant) */
    PHONEME_OW2,    /**< /oU/ as in "go" (variant) */
    PHONEME_UW2,    /**< /u:/ as in "too" (variant) */

    /* Special */
    PHONEME_SILENCE,/**< Silence/pause */
    PHONEME_UNKNOWN,/**< Unknown phoneme */

    PHONEME_COUNT
} phoneme_t;

/**
 * @brief Phonological trace structure
 *
 * Represents a word's phonological encoding in working memory.
 * Subject to decay (~2 seconds) and similarity-based interference.
 */
typedef struct {
    phoneme_t phonemes[LINGUISTICS_MAX_PHONEME_SEQUENCE]; /**< Phoneme sequence */
    float durations[LINGUISTICS_MAX_PHONEME_SEQUENCE];     /**< Duration per phoneme (ms) */
    uint32_t length;            /**< Number of phonemes */

    float activation;           /**< Current activation level [0,1] */
    float decay_rate;           /**< Decay rate (default: based on TRACE_DECAY_MS) */
    uint64_t encode_time_ms;    /**< When trace was encoded */
    uint64_t last_rehearse_ms;  /**< When last rehearsed */

    char word[LINGUISTICS_MAX_WORD_LENGTH]; /**< Original word string */
    uint32_t word_length;       /**< Character length */

    float similarity_susceptibility; /**< How susceptible to similar-item interference */
} phonological_trace_t;

/**
 * @brief Phonological loop state
 *
 * Implements Baddeley's phonological loop with:
 * - 7±2 item capacity (Miller's law)
 * - ~2 second trace decay
 * - Subvocal rehearsal for maintenance
 */
typedef struct {
    phonological_trace_t buffer[LINGUISTICS_PHONOLOGICAL_BUFFER_SIZE]; /**< Buffer slots */
    uint32_t count;             /**< Current item count */

    float rehearsal_rate;       /**< Subvocal loop speed (items/sec) */
    bool is_rehearsing;         /**< Currently in rehearsal mode */
    uint32_t rehearsal_position;/**< Current rehearsal index */

    float total_activation;     /**< Sum of all trace activations */
    float word_length_effect;   /**< Longer words = harder to maintain */
} phonological_loop_t;

/* ============================================================================
 * MESH ARCHITECTURE TYPES
 * ============================================================================ */

/**
 * @brief Linguistics belief structure
 *
 * Represents a module's belief about a linguistic interpretation.
 * Used in gossip propagation and FEP convergence.
 */
typedef struct {
    uint32_t belief_id;         /**< Unique belief identifier */
    uint32_t source_module_id;  /**< Module that produced this belief */
    char topic[128];            /**< Belief topic (e.g., "spatial_preposition_left") */

    float certainty;            /**< Local confidence [0,1] */
    float precision;            /**< Π = 1/σ² (inverse variance) */

    float belief_vector[16];    /**< Neural encoding of interpretation */
    uint32_t vector_dim;        /**< Dimension of belief vector */

    uint64_t timestamp_ms;      /**< When belief was generated */
} linguistics_belief_t;

/**
 * @brief Linguistics mesh request types
 */
typedef enum {
    LING_REQUEST_PARSE_SPATIAL,         /**< Parse spatial preposition */
    LING_REQUEST_PARSE_NUMBER,          /**< Parse number word */
    LING_REQUEST_ENCODE_PHONOLOGICAL,   /**< Encode word phonologically */
    LING_REQUEST_SELECT_FRAME,          /**< Select reference frame */
    LING_REQUEST_GENERATE_NUMBER_WORD,  /**< Generate number word from magnitude */
    LING_REQUEST_REHEARSE,              /**< Trigger rehearsal */
    LING_REQUEST_RETRIEVE,              /**< Pattern completion retrieval */
    LING_REQUEST_SIMILARITY,            /**< Compute phonological similarity */
    LING_REQUEST_TYPE_COUNT
} linguistics_request_type_t;

/**
 * @brief Spatial request data
 */
typedef struct {
    spatial_preposition_t preposition;  /**< Preposition to parse/encode */
    reference_frame_t preferred_frame;  /**< Preferred reference frame (or 0 for auto) */
} linguistics_spatial_request_t;

/**
 * @brief Number request data
 */
typedef struct {
    float value;                        /**< Number value/magnitude */
    number_word_type_t type;            /**< Number word type */
    linguistic_quantifier_t quantifier; /**< Quantifier type (for approx) */
} linguistics_number_request_t;

/**
 * @brief Phonological request data
 */
typedef struct {
    char word[LINGUISTICS_MAX_WORD_LENGTH]; /**< Word to encode */
} linguistics_phonological_request_t;

/**
 * @brief Linguistics mesh request
 */
typedef struct {
    linguistics_request_type_t type;    /**< Request type */
    uint64_t request_id;                /**< Unique request ID */
    uint64_t timestamp_ms;              /**< Request timestamp */

    /* Input data (type-dependent union) */
    union {
        linguistics_spatial_request_t spatial;      /**< Spatial request data */
        linguistics_number_request_t number;        /**< Number request data */
        linguistics_phonological_request_t phonological; /**< Phonological request data */
    };

    /* Common fields */
    char input_word[LINGUISTICS_MAX_WORD_LENGTH]; /**< Input word/phrase (legacy) */
    float input_magnitude;              /**< For number generation (legacy) */
    uint32_t input_flags;               /**< Type-specific flags */

    void* context;                      /**< Discourse/situation context */
} linguistics_request_t;

/**
 * @brief Linguistics mesh response
 */
typedef struct {
    linguistics_request_type_t type;    /**< Original request type */
    uint64_t request_id;                /**< Matching request ID */

    /* Result (type-dependent) */
    spatial_semantics_t spatial;        /**< For spatial parsing */
    numerical_semantics_t numerical;    /**< For numerical parsing */
    phonological_trace_t phonological;  /**< For phonological encoding */

    float confidence;                   /**< Overall confidence [0,1] */
    float precision;                    /**< Final precision (collective) */

    /* Mesh convergence info */
    uint32_t iterations;                /**< Convergence iterations */
    float agreement_score;              /**< Final agreement level */
    bool converged;                     /**< Whether mesh converged */
    bool used_voting_fallback;          /**< Whether voting fallback was used */

    uint32_t error_code;                /**< Error code if failed */
    char error_message[256];            /**< Error description */
} linguistics_response_t;

/**
 * @brief Mesh participant handler interface
 *
 * Each integrated module implements this interface to participate
 * in the linguistics mesh network.
 */
typedef struct {
    /**
     * @brief Process request and produce local belief
     *
     * @param ctx Module context
     * @param request Input request
     * @param belief Output belief (module's interpretation)
     * @return 0 on success, error code on failure
     */
    int (*process)(void* ctx,
                   const linguistics_request_t* request,
                   linguistics_belief_t* belief);

    /**
     * @brief Update belief based on neighbor beliefs (FEP update)
     *
     * Implements: μ' = μ - lr * Π * ε
     *
     * @param ctx Module context
     * @param neighbor_beliefs Beliefs from neighbors
     * @param neighbor_count Number of neighbor beliefs
     * @param updated_belief Output updated belief
     * @return 0 on success
     */
    int (*update)(void* ctx,
                  const linguistics_belief_t* neighbor_beliefs,
                  uint32_t neighbor_count,
                  linguistics_belief_t* updated_belief);

    /**
     * @brief Get current precision (inverse prediction error variance)
     *
     * @param ctx Module context
     * @return Precision value Π ∈ [PRECISION_FLOOR, PRECISION_CEILING]
     */
    float (*get_precision)(void* ctx);

    /** Module context */
    void* ctx;
} linguistics_mesh_handler_t;

/**
 * @brief Mesh convergence state
 */
typedef enum {
    MESH_STATE_IDLE,            /**< Not processing */
    MESH_STATE_BROADCASTING,    /**< Broadcasting request */
    MESH_STATE_COLLECTING,      /**< Collecting initial beliefs */
    MESH_STATE_PROPAGATING,     /**< Gossip propagation phase */
    MESH_STATE_CONVERGING,      /**< FEP convergence phase */
    MESH_STATE_VOTING,          /**< Voting fallback (deadlock) */
    MESH_STATE_CONVERGED,       /**< Successfully converged */
    MESH_STATE_FAILED,          /**< Failed to converge */
    MESH_STATE_TIMEOUT          /**< Timeout exceeded */
} mesh_convergence_state_t;

/**
 * @brief Mesh statistics
 */
typedef struct {
    uint64_t total_requests;        /**< Total requests processed */
    uint64_t successful_convergences; /**< Successfully converged */
    uint64_t voting_fallbacks;      /**< Required voting fallback */
    uint64_t timeouts;              /**< Timeout occurrences */
    uint64_t deadlocks;             /**< Deadlock detections */

    float avg_iterations;           /**< Average convergence iterations */
    float avg_agreement_score;      /**< Average final agreement */
    float avg_latency_ms;           /**< Average processing latency */

    uint32_t active_participants;   /**< Currently registered participants */
} linguistics_mesh_stats_t;

/* ============================================================================
 * LOGGING MODULE IDs
 * ============================================================================ */

/** Logging module identifier for spatial language */
#define LOG_MODULE_SPATIAL_LANG     "SPATIAL_LANG"

/** Logging module identifier for numerical language */
#define LOG_MODULE_NUMERICAL_LANG   "NUMERICAL_LANG"

/** Logging module identifier for phonological WM */
#define LOG_MODULE_PHONOLOGICAL_WM  "PHONOLOGICAL_WM"

/** Logging module identifier for linguistics SNN */
#define LOG_MODULE_LING_SNN         "LING_SNN"

/** Logging module identifier for linguistics plasticity */
#define LOG_MODULE_LING_PLASTICITY  "LING_PLASTICITY"

/** Logging module identifier for linguistics training */
#define LOG_MODULE_LING_TRAINING    "LING_TRAINING"

/** Logging module identifier for linguistics mesh */
#define LOG_MODULE_LING_MESH        "LING_MESH"

/* ============================================================================
 * UTILITY MACROS
 * ============================================================================ */

/** Check if phoneme is a vowel */
#define PHONEME_IS_VOWEL(p) \
    ((p) >= PHONEME_IY && (p) <= PHONEME_ER)

/** Check if phoneme is a consonant */
#define PHONEME_IS_CONSONANT(p) \
    ((p) >= PHONEME_P && (p) <= PHONEME_JH)

/** Check if phoneme is a stop */
#define PHONEME_IS_STOP(p) \
    ((p) >= PHONEME_P && (p) <= PHONEME_G)

/** Check if phoneme is a fricative */
#define PHONEME_IS_FRICATIVE(p) \
    ((p) >= PHONEME_F && (p) <= PHONEME_HH)

/** Check if phoneme is a nasal */
#define PHONEME_IS_NASAL(p) \
    ((p) >= PHONEME_M && (p) <= PHONEME_NG)

/** Check if preposition indicates proximity */
#define PREP_IS_PROXIMITY(p) \
    ((p) >= SPATIAL_PREP_NEAR && (p) <= SPATIAL_PREP_BY)

/** Check if preposition indicates horizontal relation */
#define PREP_IS_HORIZONTAL(p) \
    ((p) >= SPATIAL_PREP_LEFT && (p) <= SPATIAL_PREP_BEHIND)

/** Check if preposition indicates vertical relation */
#define PREP_IS_VERTICAL(p) \
    ((p) >= SPATIAL_PREP_ABOVE && (p) <= SPATIAL_PREP_BENEATH)

/** Check if preposition indicates containment */
#define PREP_IS_CONTAINMENT(p) \
    ((p) >= SPATIAL_PREP_IN && (p) <= SPATIAL_PREP_WITHIN)

/** Check if preposition indicates path/trajectory */
#define PREP_IS_PATH(p) \
    ((p) >= SPATIAL_PREP_THROUGH && (p) <= SPATIAL_PREP_OUT_OF)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_LINGUISTICS_TYPES_H */
