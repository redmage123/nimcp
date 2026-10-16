//=============================================================================
// nimcp_language_types.h - Language Layer Shared Types
//=============================================================================
/**
 * @file nimcp_language_types.h
 * @brief Shared types, enums, and structures for the Language Layer
 *
 * WHAT: Common type definitions used across all language layer modules
 * WHY:  Ensure consistent data exchange between language subsystems
 * HOW:  Centralized type definitions included by all language modules
 *
 * BIOLOGICAL BASIS:
 * - Represents data structures flowing through language processing pathways
 * - Phonemes (STG), words (posterior MTG), concepts (ATL), syntax (BA45)
 * - Comprehension (Wernicke) ↔ Production (Broca) data exchange
 *
 * @version 1.0.0 - Phase L1: Language Layer Core Infrastructure
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_TYPES_H
#define NIMCP_LANGUAGE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Identification
//=============================================================================

/**
 * @brief Bio-async module IDs for language layer (0x0810-0x081F range)
 *
 * These are defined in nimcp_bio_messages.h as part of bio_module_id_t enum.
 * We define macros only if bio_messages.h hasn't been included yet, to provide
 * local usage without requiring the full bio-async system.
 */
#ifndef NIMCP_BIO_MESSAGES_H
#define BIO_MODULE_LANGUAGE_LAYER              0x0810
#define BIO_MODULE_LANGUAGE_PERCEPTION_BRIDGE  0x0811
#define BIO_MODULE_LANGUAGE_COGNITIVE_BRIDGE   0x0812
#define BIO_MODULE_LANGUAGE_TRAINING_BRIDGE    0x0813
#define BIO_MODULE_LANGUAGE_OMNI_BRIDGE        0x0814
#define BIO_MODULE_LANGUAGE_IMMUNE_BRIDGE      0x0815
#define BIO_MODULE_LANGUAGE_GPU_BRIDGE         0x0816
#endif

/** @brief Language layer version */
#define LANGUAGE_LAYER_VERSION_MAJOR    1
#define LANGUAGE_LAYER_VERSION_MINOR    0
#define LANGUAGE_LAYER_VERSION_PATCH    0

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum phoneme sequence length */
#define LANGUAGE_MAX_PHONEMES           512

/** @brief Maximum word count in utterance */
#define LANGUAGE_MAX_WORDS              128

/** @brief Maximum token sequence length */
#define LANGUAGE_MAX_TOKENS             256

/** @brief Maximum active concepts */
#define LANGUAGE_MAX_CONCEPTS           128

/** @brief Default semantic embedding dimension */
#define LANGUAGE_SEMANTIC_DIM           256

/** @brief Phonological working memory capacity (Miller's 7±2) */
#define LANGUAGE_PHONOLOGICAL_WM_SIZE   9

/** @brief Maximum formants tracked (F1-F4) */
#define LANGUAGE_MAX_FORMANTS           4

/** @brief Maximum sentence parse depth */
#define LANGUAGE_MAX_PARSE_DEPTH        32

//=============================================================================
// Orchestrator State Enumerations
//=============================================================================

/**
 * @brief Language layer processing state
 *
 * BIOLOGICAL BASIS:
 * - IDLE: Resting state, no language activation
 * - LISTENING: Primary auditory cortex active, speech detection
 * - COMPREHENDING: Wernicke's area (BA22) processing
 * - INTEGRATING: Cross-modal binding, semantic integration
 * - GENERATING: Broca's area (BA44/45) planning output
 * - PRODUCING: Motor cortex executing speech production
 */
typedef enum {
    LANGUAGE_STATE_IDLE = 0,          /**< No active processing */
    LANGUAGE_STATE_LISTENING,         /**< Receiving perceptual input */
    LANGUAGE_STATE_COMPREHENDING,     /**< Processing through Wernicke */
    LANGUAGE_STATE_INTEGRATING,       /**< Cross-module integration */
    LANGUAGE_STATE_GENERATING,        /**< Generating through Broca */
    LANGUAGE_STATE_PRODUCING,         /**< Motor output preparation */
    LANGUAGE_STATE_ERROR,             /**< Error state */
    LANGUAGE_STATE_COUNT
} language_state_t;

/**
 * @brief Language processing mode
 *
 * BIOLOGICAL BASIS:
 * - COMPREHENSION: Listening/reading pathway
 * - PRODUCTION: Speaking/writing pathway
 * - DIALOGUE: Full bidirectional (typical conversation)
 * - REPETITION: Direct Wernicke→Broca (via arcuate fasciculus)
 * - TRANSLATION: Cross-language (involves additional bilingual areas)
 */
typedef enum {
    LANGUAGE_MODE_COMPREHENSION = 0,  /**< Input processing only */
    LANGUAGE_MODE_PRODUCTION,         /**< Output generation only */
    LANGUAGE_MODE_DIALOGUE,           /**< Full bidirectional */
    LANGUAGE_MODE_REPETITION,         /**< Direct echo (bypass semantics) */
    LANGUAGE_MODE_TRANSLATION,        /**< Cross-language processing */
    LANGUAGE_MODE_COUNT
} language_mode_t;

/**
 * @brief Language input type
 */
typedef enum {
    LANGUAGE_INPUT_AUDIO = 0,         /**< Raw audio stream */
    LANGUAGE_INPUT_TEXT,              /**< Text string */
    LANGUAGE_INPUT_TOKENS,            /**< Pre-tokenized input */
    LANGUAGE_INPUT_PHONEMES,          /**< Phoneme sequence */
    LANGUAGE_INPUT_SEMANTIC,          /**< Semantic vector */
    LANGUAGE_INPUT_VISUAL,            /**< Visual input (reading) */
    LANGUAGE_INPUT_COUNT
} language_input_type_t;

/**
 * @brief Language output type
 */
typedef enum {
    LANGUAGE_OUTPUT_MOTOR_COMMANDS = 0,  /**< Speech motor commands */
    LANGUAGE_OUTPUT_PHONEMES,            /**< Phoneme sequence */
    LANGUAGE_OUTPUT_TOKENS,              /**< Token sequence */
    LANGUAGE_OUTPUT_TEXT,                /**< Text string */
    LANGUAGE_OUTPUT_SEMANTIC,            /**< Semantic vector */
    LANGUAGE_OUTPUT_COUNT
} language_output_type_t;

//=============================================================================
// Phonological Types
//=============================================================================

/**
 * @brief Phoneme categories (IPA-based)
 */
typedef enum {
    PHONEME_CAT_VOWEL = 0,            /**< Vowel sounds */
    PHONEME_CAT_STOP,                 /**< Stop consonants (p, b, t, d, k, g) */
    PHONEME_CAT_FRICATIVE,            /**< Fricatives (f, v, s, z, etc.) */
    PHONEME_CAT_AFFRICATE,            /**< Affricates (ch, j) */
    PHONEME_CAT_NASAL,                /**< Nasals (m, n, ng) */
    PHONEME_CAT_APPROXIMANT,          /**< Approximants (l, r, w, y) */
    PHONEME_CAT_SILENCE,              /**< Silence/pause */
    PHONEME_CAT_UNKNOWN,              /**< Unrecognized */
    PHONEME_CAT_COUNT
} phoneme_category_t;

/**
 * @brief Phoneme representation
 *
 * BIOLOGICAL BASIS:
 * - Represents neural encoding of phoneme in STG
 * - Formants encode vowel quality (F1-F2 vowel space)
 * - Duration and stress encode prosodic information
 */
typedef struct {
    uint32_t id;                      /**< Phoneme identifier (IPA code) */
    phoneme_category_t category;      /**< Phoneme category */
    float confidence;                 /**< Recognition confidence [0-1] */
    float duration_ms;                /**< Duration in milliseconds */
    float formants[LANGUAGE_MAX_FORMANTS];  /**< Formant frequencies (Hz) */
    float pitch_hz;                   /**< Fundamental frequency (F0) */
    float intensity;                  /**< Amplitude/loudness [0-1] */
    bool is_stressed;                 /**< Prosodic stress marker */
    bool is_word_boundary;            /**< Word boundary marker */
    bool is_phrase_boundary;          /**< Phrase boundary marker */
    uint64_t timestamp_ms;            /**< Time of occurrence */
} language_phoneme_t;

/**
 * @brief Prosodic contour
 *
 * BIOLOGICAL BASIS:
 * - Right hemisphere involvement in prosody
 * - Pitch contour encodes intonation (statement vs. question)
 * - Stress pattern encodes focus and emphasis
 */
typedef struct {
    float* pitch_contour;             /**< Pitch (F0) over time */
    float* intensity_contour;         /**< Intensity over time */
    uint32_t contour_length;          /**< Number of time points */
    float sample_rate_hz;             /**< Contour sample rate */
    bool is_question;                 /**< Question intonation detected */
    bool is_command;                  /**< Command intonation detected */
    float emotional_valence;          /**< Emotional tone [-1, +1] */
    float emotional_arousal;          /**< Emotional arousal [0, 1] */
} language_prosody_t;

//=============================================================================
// Lexical Types
//=============================================================================

/**
 * @brief Part of speech categories
 */
typedef enum {
    POS_NOUN = 0,
    POS_VERB,
    POS_ADJECTIVE,
    POS_ADVERB,
    POS_DETERMINER,
    POS_PREPOSITION,
    POS_CONJUNCTION,
    POS_PRONOUN,
    POS_AUXILIARY,
    POS_COMPLEMENTIZER,
    POS_NEGATION,
    POS_PUNCTUATION,
    POS_UNKNOWN,
    POS_COUNT
} part_of_speech_t;

/**
 * @brief Word representation
 *
 * BIOLOGICAL BASIS:
 * - Word form stored in posterior MTG
 * - Lexical frequency affects recognition speed
 * - Multiple senses for polysemous words
 */
typedef struct {
    uint32_t id;                      /**< Word identifier */
    char form[64];                    /**< Orthographic form */
    part_of_speech_t pos;             /**< Part of speech */
    float frequency;                  /**< Log frequency (Zipf scale) */
    float activation;                 /**< Current activation level [0-1] */
    float confidence;                 /**< Recognition confidence [0-1] */
    uint32_t sense_id;                /**< Active sense (for polysemy) */
    uint32_t num_senses;              /**< Number of available senses */
    uint32_t phoneme_count;           /**< Number of phonemes */
    uint32_t syllable_count;          /**< Number of syllables */
    uint64_t timestamp_ms;            /**< Recognition timestamp */
} language_word_t;

//=============================================================================
// Semantic Types
//=============================================================================

/**
 * @brief Thematic roles (Case Grammar)
 *
 * BIOLOGICAL BASIS:
 * - Computed in pMTG/angular gyrus
 * - Links syntax to semantics
 * - Critical for sentence comprehension
 */
typedef enum {
    THEMATIC_ROLE_AGENT = 0,          /**< Doer of action */
    THEMATIC_ROLE_PATIENT,            /**< Affected by action */
    THEMATIC_ROLE_THEME,              /**< Entity being moved/changed */
    THEMATIC_ROLE_EXPERIENCER,        /**< Perceiver/feeler */
    THEMATIC_ROLE_BENEFICIARY,        /**< Recipient of benefit */
    THEMATIC_ROLE_INSTRUMENT,         /**< Tool used */
    THEMATIC_ROLE_LOCATION,           /**< Place */
    THEMATIC_ROLE_SOURCE,             /**< Origin */
    THEMATIC_ROLE_GOAL,               /**< Destination */
    THEMATIC_ROLE_TIME,               /**< Temporal reference */
    THEMATIC_ROLE_MANNER,             /**< How action performed */
    THEMATIC_ROLE_CAUSE,              /**< Reason for action */
    THEMATIC_ROLE_NONE,               /**< No role assigned */
    THEMATIC_ROLE_COUNT
} thematic_role_t;

/**
 * @brief Semantic concept activation
 *
 * BIOLOGICAL BASIS:
 * - Concepts stored in anterior temporal lobe (ATL)
 * - Activation spreads through semantic network
 * - Relevance computed via context
 */
typedef struct {
    uint32_t id;                      /**< Concept identifier */
    char name[64];                    /**< Human-readable name */
    float activation;                 /**< Activation level [0-1] */
    float relevance;                  /**< Context relevance [0-1] */
    uint32_t source_word_id;          /**< Activating word (if any) */
    thematic_role_t role;             /**< Assigned thematic role */
    bool is_target;                   /**< Target of current utterance */
    uint64_t activation_time_ms;      /**< When activated */
} language_concept_t;

/**
 * @brief Semantic context
 *
 * BIOLOGICAL BASIS:
 * - Maintains discourse model
 * - Guides interpretation of ambiguous input
 * - Enables anaphora resolution
 */
typedef struct {
    language_concept_t* active_concepts;  /**< Currently active concepts */
    uint32_t num_concepts;                /**< Number of active concepts */
    float* context_vector;                /**< Aggregated context embedding */
    uint32_t context_dim;                 /**< Context vector dimension */
    float coherence_score;                /**< Discourse coherence [0-1] */
    uint32_t topic_id;                    /**< Current topic identifier */
    uint64_t last_update_ms;              /**< Last context update */
} language_context_t;

//=============================================================================
// Syntactic Types
//=============================================================================

/**
 * @brief Phrase types
 */
typedef enum {
    PHRASE_NP = 0,                    /**< Noun phrase */
    PHRASE_VP,                        /**< Verb phrase */
    PHRASE_PP,                        /**< Prepositional phrase */
    PHRASE_AP,                        /**< Adjective phrase */
    PHRASE_ADVP,                      /**< Adverb phrase */
    PHRASE_S,                         /**< Sentence */
    PHRASE_SBAR,                      /**< Subordinate clause */
    PHRASE_CP,                        /**< Complementizer phrase */
    PHRASE_IP,                        /**< Inflection phrase */
    PHRASE_DP,                        /**< Determiner phrase */
    PHRASE_COUNT
} phrase_type_t;

/**
 * @brief Parse state
 */
typedef enum {
    PARSE_STATE_INIT = 0,
    PARSE_STATE_ACTIVE,
    PARSE_STATE_COMPLETE,
    PARSE_STATE_AMBIGUOUS,
    PARSE_STATE_GARDEN_PATH,
    PARSE_STATE_ERROR,
    PARSE_STATE_COUNT
} parse_state_t;

/**
 * @brief Syntactic parse node
 */
typedef struct language_parse_node {
    phrase_type_t type;                   /**< Phrase type */
    uint32_t head_word_id;                /**< Head word of phrase */
    struct language_parse_node** children;/**< Child nodes */
    uint32_t num_children;                /**< Number of children */
    uint32_t start_position;              /**< Start word position */
    uint32_t end_position;                /**< End word position */
    float probability;                    /**< Parse probability */
} language_parse_node_t;

//=============================================================================
// Comprehension Result Types
//=============================================================================

/**
 * @brief Complete comprehension result
 *
 * BIOLOGICAL BASIS:
 * - Represents output of full Wernicke processing
 * - Integrates phonological, lexical, semantic, syntactic
 * - Ready for use by Broca or cognitive systems
 */
typedef struct {
    /* Lexical results */
    language_word_t* words;           /**< Recognized words */
    uint32_t word_count;              /**< Number of words */
    float lexical_confidence;         /**< Overall lexical confidence */

    /* Semantic results */
    language_concept_t* concepts;     /**< Activated concepts */
    uint32_t concept_count;           /**< Number of concepts */
    float semantic_coherence;         /**< Semantic coherence score */

    /* Syntactic results */
    language_parse_node_t* parse_tree;/**< Parse tree root */
    parse_state_t parse_state;        /**< Parse completion state */
    float syntactic_wellformedness;   /**< Grammaticality score */

    /* Prosodic results */
    language_prosody_t prosody;       /**< Prosodic information */

    /* Context */
    float* semantic_vector;           /**< Sentence semantic embedding */
    uint32_t semantic_dim;            /**< Embedding dimension */

    /* Performance */
    float processing_time_ms;         /**< Total processing time */
    float overall_confidence;         /**< Overall comprehension confidence */

    /* Anomaly detection (N400-like) */
    float semantic_anomaly;           /**< Semantic violation magnitude */
    float syntactic_anomaly;          /**< Syntactic violation magnitude */

    /* Metadata */
    uint64_t timestamp_ms;            /**< Completion timestamp */
    bool valid;                       /**< Result validity flag */
} language_comprehension_result_t;

//=============================================================================
// Production Plan Types
//=============================================================================

/**
 * @brief Speech motor command
 */
typedef struct {
    uint32_t articulator;             /**< Target articulator (lips/tongue/etc) */
    float position;                   /**< Target position [0-1] */
    float velocity;                   /**< Movement velocity */
    float duration_ms;                /**< Command duration */
    uint32_t phoneme_id;              /**< Associated phoneme */
    uint64_t timestamp_ms;            /**< Execution timestamp */
} language_motor_command_t;

/**
 * @brief Production plan
 *
 * BIOLOGICAL BASIS:
 * - Represents output of Broca's area processing
 * - Hierarchical: message → words → phonemes → motor
 * - Ready for motor cortex execution
 */
typedef struct {
    /* Input */
    float* semantic_input;            /**< Input semantic representation */
    uint32_t semantic_dim;            /**< Semantic dimension */

    /* Word plan */
    language_word_t* words;           /**< Planned word sequence */
    uint32_t word_count;              /**< Number of words */

    /* Phonological plan */
    language_phoneme_t* phonemes;     /**< Planned phoneme sequence */
    uint32_t phoneme_count;           /**< Number of phonemes */

    /* Motor plan */
    language_motor_command_t* motor_commands;  /**< Motor command sequence */
    uint32_t motor_command_count;     /**< Number of motor commands */

    /* Prosody plan */
    language_prosody_t prosody;       /**< Planned prosodic contour */

    /* State */
    uint32_t current_word_index;      /**< Current production position (word) */
    uint32_t current_phoneme_index;   /**< Current production position (phoneme) */
    float fluency_score;              /**< Expected fluency */

    /* Performance */
    float planning_time_ms;           /**< Planning time */

    /* Metadata */
    uint64_t timestamp_ms;            /**< Plan creation timestamp */
    bool valid;                       /**< Plan validity flag */
    bool complete;                    /**< Production complete flag */
} language_production_plan_t;

//=============================================================================
// Event Types for Bio-Async
//=============================================================================

/**
 * @brief Language layer event types
 */
typedef enum {
    LANGUAGE_EVENT_UTTERANCE_START = 0,    /**< Utterance processing started */
    LANGUAGE_EVENT_PHONEME_RECOGNIZED,     /**< Phoneme recognized */
    LANGUAGE_EVENT_WORD_RECOGNIZED,        /**< Word recognized */
    LANGUAGE_EVENT_CONCEPT_ACTIVATED,      /**< Concept activated */
    LANGUAGE_EVENT_COMPREHENSION_COMPLETE, /**< Comprehension finished */
    LANGUAGE_EVENT_PRODUCTION_START,       /**< Production started */
    LANGUAGE_EVENT_PRODUCTION_COMPLETE,    /**< Production finished */
    LANGUAGE_EVENT_AMBIGUITY_DETECTED,     /**< Ambiguous input */
    LANGUAGE_EVENT_ANOMALY_DETECTED,       /**< Semantic/syntactic anomaly */
    LANGUAGE_EVENT_ERROR,                  /**< Processing error */
    LANGUAGE_EVENT_STATE_CHANGE,           /**< State machine transition */
    LANGUAGE_EVENT_TRAINING_UPDATE,        /**< Training update applied */
    LANGUAGE_EVENT_COUNT
} language_event_type_t;

/**
 * @brief Language event payload
 */
typedef struct {
    language_event_type_t type;       /**< Event type */
    uint64_t timestamp_ms;            /**< Event timestamp */
    uint32_t source_module;           /**< Source module ID */

    union {
        struct {
            uint32_t phoneme_id;
            float confidence;
        } phoneme;

        struct {
            uint32_t word_id;
            float confidence;
        } word;

        struct {
            uint32_t concept_id;
            float activation;
        } concept_activation;

        struct {
            language_state_t old_state;
            language_state_t new_state;
        } state_change;

        struct {
            float anomaly_magnitude;
            bool is_semantic;
        } anomaly;

        struct {
            int error_code;
            char message[64];
        } error;
    } data;
} language_event_t;

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* language_state_to_string(language_state_t state);
const char* language_mode_to_string(language_mode_t mode);
const char* language_input_type_to_string(language_input_type_t type);
const char* language_output_type_to_string(language_output_type_t type);
const char* phoneme_category_to_string(phoneme_category_t cat);
const char* part_of_speech_to_string(part_of_speech_t pos);
const char* thematic_role_to_string(thematic_role_t role);
const char* phrase_type_to_string(phrase_type_t type);
const char* parse_state_to_string(parse_state_t state);
const char* language_event_type_to_string(language_event_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_TYPES_H */
