/**
 * @file nimcp_lexical_access.h
 * @brief Lexical access layer for Wernicke's area - Word recognition from phoneme sequences
 *
 * WHAT: Word recognition, lexicon management, and frequency-weighted retrieval
 * WHY:  Map phoneme sequences to word forms for language comprehension
 * HOW:  Cohort model + frequency effects + neighborhood density
 *
 * BIOLOGICAL BASIS:
 * - Lexical access occurs in posterior STG and middle temporal gyrus (MTG)
 * - Word recognition is incremental (Cohort model, Marslen-Wilson 1987)
 * - High-frequency words recognized faster (frequency effect)
 * - Words with many similar neighbors harder to recognize (neighborhood density)
 * - Priming from semantic context speeds recognition
 *
 * PROCESSING MODEL:
 * 1. Cohort Generation: Initial phonemes activate candidate words
 * 2. Cohort Reduction: Subsequent phonemes eliminate mismatches
 * 3. Uniqueness Point: Recognition when one candidate remains
 * 4. Frequency Weighting: High-frequency words preferred
 * 5. Context Integration: Semantic priming biases selection
 *
 * KEY PHENOMENA MODELED:
 * - Word frequency effect (Oldfield & Wingfield 1965)
 * - Cohort activation (Marslen-Wilson & Welsh 1978)
 * - Neighborhood density effects (Luce & Pisoni 1998)
 * - Semantic priming (Meyer & Schvaneveldt 1971)
 * - Word superiority effect (Reicher 1969)
 *
 * @version Phase W2: Wernicke's Area Lexical Access
 * @date 2026-01-04
 */

#ifndef NIMCP_LEXICAL_ACCESS_H
#define NIMCP_LEXICAL_ACCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Phoneme types */
#include "perception/nimcp_speech_cortex.h"

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define LEX_DEFAULT_LEXICON_SIZE         10000
#define LEX_DEFAULT_MAX_COHORT_SIZE      100
#define LEX_DEFAULT_MAX_WORD_LENGTH      32
#define LEX_DEFAULT_MAX_PHONEME_LENGTH   24
#define LEX_DEFAULT_EMBEDDING_DIM        128
#define LEX_DEFAULT_HASH_BUCKETS         4096
#define LEX_DEFAULT_FREQUENCY_WEIGHT     0.3f
#define LEX_DEFAULT_PHONEME_MATCH_WEIGHT 0.7f

/**
 * @brief Part of speech tags
 */
typedef enum {
    POS_UNKNOWN = 0,
    POS_NOUN,
    POS_VERB,
    POS_ADJECTIVE,
    POS_ADVERB,
    POS_PRONOUN,
    POS_PREPOSITION,
    POS_CONJUNCTION,
    POS_DETERMINER,
    POS_INTERJECTION,
    POS_COUNT
} part_of_speech_t;

/**
 * @brief Lexical access configuration
 */
typedef struct {
    /* Lexicon capacity */
    uint32_t lexicon_size;               /**< Maximum vocabulary size */
    uint32_t hash_buckets;               /**< Hash table buckets */

    /* Word parameters */
    uint32_t max_word_length;            /**< Maximum word string length */
    uint32_t max_phoneme_length;         /**< Maximum phonemes per word */

    /* Cohort model parameters */
    uint32_t max_cohort_size;            /**< Maximum active cohort members */
    float cohort_activation_threshold;   /**< Minimum activation to stay in cohort */
    float uniqueness_threshold;          /**< Confidence for uniqueness point */

    /* Weighting parameters */
    float frequency_weight;              /**< Weight for word frequency [0,1] */
    float phoneme_match_weight;          /**< Weight for phoneme match [0,1] */
    float context_weight;                /**< Weight for semantic context [0,1] */

    /* Embedding */
    uint32_t embedding_dim;              /**< Word embedding dimension */
    bool enable_embeddings;              /**< Enable word embeddings */

    /* Priming */
    bool enable_priming;                 /**< Enable semantic priming */
    float priming_decay;                 /**< Priming decay rate per step */
    float priming_boost;                 /**< Activation boost from priming */
} lexical_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Lexical entry (word in mental lexicon)
 */
typedef struct {
    /* Identity */
    uint32_t word_id;                    /**< Unique word ID */
    char orthography[LEX_DEFAULT_MAX_WORD_LENGTH];  /**< Written form */

    /* Phonological form */
    uint8_t phonemes[LEX_DEFAULT_MAX_PHONEME_LENGTH]; /**< Phoneme sequence */
    uint32_t phoneme_count;              /**< Number of phonemes */

    /* Linguistic properties */
    part_of_speech_t pos;                /**< Part of speech */
    float frequency;                     /**< Log frequency (Zipf scale 1-7) */
    uint32_t syllable_count;             /**< Number of syllables */

    /* Semantic links */
    uint32_t concept_id;                 /**< Primary concept ID */
    uint32_t* sense_ids;                 /**< Multiple senses (polysemy) */
    uint32_t num_senses;                 /**< Number of senses */

    /* Neighborhood */
    uint32_t* neighbors;                 /**< Phonological neighbor IDs */
    uint32_t num_neighbors;              /**< Neighborhood density */

    /* Embedding (optional) */
    float* embedding;                    /**< Word embedding vector */
} lexical_entry_t;

/**
 * @brief Cohort member (active candidate during recognition)
 */
typedef struct {
    uint32_t word_id;                    /**< Word ID */
    float activation;                    /**< Current activation level [0,1] */
    float phoneme_match;                 /**< Phoneme match score [0,1] */
    float frequency_score;               /**< Frequency contribution */
    float context_score;                 /**< Semantic context contribution */
    uint32_t matched_phonemes;           /**< Phonemes matched so far */
    bool is_prefix_match;                /**< Still valid prefix match */
} cohort_member_t;

/**
 * @brief Cohort state (active word candidates)
 */
typedef struct {
    cohort_member_t* members;            /**< Active cohort members */
    uint32_t num_members;                /**< Current cohort size */
    uint32_t max_members;                /**< Maximum cohort size */

    /* Recognition state */
    uint32_t phonemes_processed;         /**< Phonemes seen so far */
    bool uniqueness_reached;             /**< Single candidate remaining */
    uint32_t winner_id;                  /**< Winner word ID (if unique) */
    float winner_confidence;             /**< Winner confidence [0,1] */
} cohort_state_t;

/**
 * @brief Lexical access result
 */
typedef struct {
    /* Recognition outcome */
    bool word_recognized;                /**< Word successfully recognized */
    uint32_t word_id;                    /**< Recognized word ID */
    char word[LEX_DEFAULT_MAX_WORD_LENGTH]; /**< Word string */
    float confidence;                    /**< Recognition confidence [0,1] */

    /* Timing */
    uint32_t recognition_point;          /**< Phoneme index at recognition */
    uint32_t uniqueness_point;           /**< Phoneme index at uniqueness */
    float latency_ms;                    /**< Recognition latency */

    /* Competition */
    uint32_t cohort_size_initial;        /**< Initial cohort size */
    uint32_t cohort_size_final;          /**< Final cohort size */
    float competition_index;             /**< Lexical competition [0,1] */

    /* Alternatives */
    uint32_t alt_word_ids[5];            /**< Alternative candidates */
    float alt_confidences[5];            /**< Alternative confidences */
    uint32_t num_alternatives;           /**< Number of alternatives */
} lexical_result_t;

/**
 * @brief Priming context
 */
typedef struct {
    uint32_t* primed_concepts;           /**< Primed concept IDs */
    float* priming_levels;               /**< Priming activation levels */
    uint32_t num_primed;                 /**< Number of primed concepts */
    uint32_t max_primed;                 /**< Maximum primed concepts */
} priming_context_t;

/**
 * @brief Lexical access statistics
 */
typedef struct {
    uint64_t lookups;                    /**< Total lookup attempts */
    uint64_t hits;                       /**< Successful recognitions */
    uint64_t misses;                     /**< Failed recognitions */
    uint64_t cohort_activations;         /**< Total cohort activations */

    float avg_cohort_size;               /**< Average initial cohort size */
    float avg_recognition_point;         /**< Average phoneme at recognition */
    float avg_confidence;                /**< Average recognition confidence */
    float avg_latency_ms;                /**< Average recognition latency */

    uint64_t priming_assists;            /**< Recognitions aided by priming */
} lexical_stats_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

typedef struct lexical_access lexical_access_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default lexical access configuration
 */
lexical_config_t lexical_default_config(void);

/**
 * @brief Create lexical access module
 *
 * @param config Configuration (NULL for defaults)
 * @return New module or NULL on failure
 */
lexical_access_t* lexical_create(const lexical_config_t* config);

/**
 * @brief Destroy lexical access module
 *
 * @param lex Module to destroy
 */
void lexical_destroy(lexical_access_t* lex);

/**
 * @brief Reset module state (clear cohort, priming)
 *
 * @param lex Module instance
 * @return true on success
 */
bool lexical_reset(lexical_access_t* lex);

/*=============================================================================
 * LEXICON MANAGEMENT
 *===========================================================================*/

/**
 * @brief Add word to lexicon
 *
 * WHAT: Store word entry in mental lexicon
 * WHY:  Build vocabulary for word recognition
 * HOW:  Hash by initial phonemes for cohort lookup
 *
 * @param lex Module instance
 * @param entry Lexical entry to add
 * @return true on success, false if lexicon full
 */
bool lexical_add_entry(lexical_access_t* lex, const lexical_entry_t* entry);

/**
 * @brief Add word with simple parameters
 *
 * WHAT: Convenience function to add word
 * WHY:  Simpler API for common case
 * HOW:  Build entry and call lexical_add_entry
 *
 * @param lex Module instance
 * @param word Word string
 * @param phonemes Phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param frequency Word frequency (Zipf 1-7)
 * @param concept_id Associated concept
 * @return Word ID or 0 on failure
 */
uint32_t lexical_add_word(
    lexical_access_t* lex,
    const char* word,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    float frequency,
    uint32_t concept_id
);

/**
 * @brief Look up word by ID
 *
 * @param lex Module instance
 * @param word_id Word ID to look up
 * @param entry Output entry (filled on success)
 * @return true if found
 */
bool lexical_get_entry(
    const lexical_access_t* lex,
    uint32_t word_id,
    lexical_entry_t* entry
);

/**
 * @brief Look up word by string
 *
 * @param lex Module instance
 * @param word Word string
 * @param entry Output entry (filled on success)
 * @return true if found
 */
bool lexical_lookup_word(
    const lexical_access_t* lex,
    const char* word,
    lexical_entry_t* entry
);

/**
 * @brief Get lexicon size
 *
 * @param lex Module instance
 * @return Number of words in lexicon
 */
uint32_t lexical_get_size(const lexical_access_t* lex);

/**
 * @brief Set word embedding
 *
 * @param lex Module instance
 * @param word_id Word ID
 * @param embedding Embedding vector (copied)
 * @param dim Embedding dimension
 * @return true on success
 */
bool lexical_set_embedding(
    lexical_access_t* lex,
    uint32_t word_id,
    const float* embedding,
    uint32_t dim
);

/*=============================================================================
 * WORD RECOGNITION (Cohort Model)
 *===========================================================================*/

/**
 * @brief Begin word recognition
 *
 * WHAT: Initialize cohort for new word
 * WHY:  Start fresh recognition process
 * HOW:  Clear previous cohort state
 *
 * @param lex Module instance
 * @return true on success
 */
bool lexical_begin_recognition(lexical_access_t* lex);

/**
 * @brief Process next phoneme
 *
 * WHAT: Update cohort with new phoneme
 * WHY:  Incremental word recognition
 * HOW:  Activate matching words, deactivate mismatches
 *
 * @param lex Module instance
 * @param phoneme New phoneme
 * @param confidence Phoneme detection confidence [0,1]
 * @return true on success
 */
bool lexical_process_phoneme(
    lexical_access_t* lex,
    phoneme_t phoneme,
    float confidence
);

/**
 * @brief Check if word is recognized
 *
 * WHAT: Query recognition status
 * WHY:  Check if uniqueness point reached
 * HOW:  Return true if single high-confidence candidate
 *
 * @param lex Module instance
 * @return true if word recognized
 */
bool lexical_is_recognized(const lexical_access_t* lex);

/**
 * @brief Get recognition result
 *
 * WHAT: Retrieve word recognition outcome
 * WHY:  Get recognized word and alternatives
 * HOW:  Return current best candidate and cohort state
 *
 * @param lex Module instance
 * @param result Output result structure
 * @return true on success
 */
bool lexical_get_result(
    const lexical_access_t* lex,
    lexical_result_t* result
);

/**
 * @brief Recognize word from complete phoneme sequence
 *
 * WHAT: One-shot word recognition
 * WHY:  Convenience for full phoneme sequences
 * HOW:  Begin, process all phonemes, return result
 *
 * @param lex Module instance
 * @param phonemes Phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param result Output result
 * @return true if word recognized
 */
bool lexical_recognize_word(
    lexical_access_t* lex,
    const phoneme_t* phonemes,
    uint32_t num_phonemes,
    lexical_result_t* result
);

/*=============================================================================
 * COHORT ACCESS
 *===========================================================================*/

/**
 * @brief Get current cohort state
 *
 * @param lex Module instance
 * @param state Output cohort state (do not free)
 * @return true on success
 */
bool lexical_get_cohort(
    const lexical_access_t* lex,
    const cohort_state_t** state
);

/**
 * @brief Get cohort member by index
 *
 * @param lex Module instance
 * @param index Member index
 * @param member Output member (filled on success)
 * @return true if valid index
 */
bool lexical_get_cohort_member(
    const lexical_access_t* lex,
    uint32_t index,
    cohort_member_t* member
);

/**
 * @brief Force activate specific word in cohort
 *
 * WHAT: Manually boost word activation
 * WHY:  Top-down influence from context
 * HOW:  Add activation boost to cohort member
 *
 * @param lex Module instance
 * @param word_id Word to activate
 * @param boost Activation boost [0,1]
 * @return true on success
 */
bool lexical_boost_word(
    lexical_access_t* lex,
    uint32_t word_id,
    float boost
);

/*=============================================================================
 * PRIMING
 *===========================================================================*/

/**
 * @brief Prime concept for faster recognition
 *
 * WHAT: Pre-activate words related to concept
 * WHY:  Model semantic priming effects
 * HOW:  Boost activation of words with matching concept
 *
 * @param lex Module instance
 * @param concept_id Concept to prime
 * @param strength Priming strength [0,1]
 * @return true on success
 */
bool lexical_prime_concept(
    lexical_access_t* lex,
    uint32_t concept_id,
    float strength
);

/**
 * @brief Prime word for faster recognition
 *
 * WHAT: Pre-activate specific word and neighbors
 * WHY:  Model word-level priming
 * HOW:  Boost word and phonological neighbors
 *
 * @param lex Module instance
 * @param word_id Word to prime
 * @param strength Priming strength [0,1]
 * @return true on success
 */
bool lexical_prime_word(
    lexical_access_t* lex,
    uint32_t word_id,
    float strength
);

/**
 * @brief Decay priming over time
 *
 * WHAT: Reduce priming activations
 * WHY:  Priming fades without reinforcement
 * HOW:  Multiply priming levels by decay factor
 *
 * @param lex Module instance
 * @return true on success
 */
bool lexical_decay_priming(lexical_access_t* lex);

/**
 * @brief Clear all priming
 *
 * @param lex Module instance
 */
void lexical_clear_priming(lexical_access_t* lex);

/**
 * @brief Get priming context
 *
 * @param lex Module instance
 * @param context Output priming context (do not free)
 * @return true on success
 */
bool lexical_get_priming(
    const lexical_access_t* lex,
    const priming_context_t** context
);

/*=============================================================================
 * NEIGHBORHOOD
 *===========================================================================*/

/**
 * @brief Compute phonological neighbors
 *
 * WHAT: Find words differing by one phoneme
 * WHY:  Neighborhood density affects recognition
 * HOW:  Edit distance = 1 in phoneme space
 *
 * @param lex Module instance
 * @param word_id Word ID
 * @param neighbors Output neighbor IDs
 * @param max_neighbors Maximum neighbors
 * @param num_neighbors Output: actual count
 * @return true on success
 */
bool lexical_get_neighbors(
    lexical_access_t* lex,
    uint32_t word_id,
    uint32_t* neighbors,
    uint32_t max_neighbors,
    uint32_t* num_neighbors
);

/**
 * @brief Get neighborhood density
 *
 * @param lex Module instance
 * @param word_id Word ID
 * @return Neighborhood density (number of neighbors)
 */
uint32_t lexical_get_neighborhood_density(
    const lexical_access_t* lex,
    uint32_t word_id
);

/*=============================================================================
 * FREQUENCY & STATISTICS
 *===========================================================================*/

/**
 * @brief Get word frequency
 *
 * @param lex Module instance
 * @param word_id Word ID
 * @return Frequency (Zipf scale 1-7) or 0 if not found
 */
float lexical_get_frequency(
    const lexical_access_t* lex,
    uint32_t word_id
);

/**
 * @brief Update word frequency (learning)
 *
 * WHAT: Adjust word frequency from exposure
 * WHY:  Model frequency effects from experience
 * HOW:  Increment frequency counter
 *
 * @param lex Module instance
 * @param word_id Word ID
 * @param delta Frequency adjustment
 * @return true on success
 */
bool lexical_update_frequency(
    lexical_access_t* lex,
    uint32_t word_id,
    float delta
);

/**
 * @brief Get lexical access statistics
 *
 * @param lex Module instance
 * @param stats Output statistics
 * @return true on success
 */
bool lexical_get_stats(
    const lexical_access_t* lex,
    lexical_stats_t* stats
);

/*=============================================================================
 * BATCH OPERATIONS
 *===========================================================================*/

/**
 * @brief Load lexicon from file
 *
 * WHAT: Import vocabulary from file
 * WHY:  Initialize with existing vocabulary
 * HOW:  Parse word-phoneme-frequency triples
 *
 * Format: word TAB phonemes TAB frequency TAB concept_id
 *
 * @param lex Module instance
 * @param filepath Path to lexicon file
 * @return Number of words loaded, or -1 on error
 */
int32_t lexical_load_lexicon(
    lexical_access_t* lex,
    const char* filepath
);

/**
 * @brief Save lexicon to file
 *
 * @param lex Module instance
 * @param filepath Output file path
 * @return true on success
 */
bool lexical_save_lexicon(
    const lexical_access_t* lex,
    const char* filepath
);

/**
 * @brief Build common English words
 *
 * WHAT: Initialize with frequent English words
 * WHY:  Quick setup for testing/demos
 * HOW:  Add ~1000 most common words
 *
 * @param lex Module instance
 * @return Number of words added
 */
uint32_t lexical_build_common_english(lexical_access_t* lex);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LEXICAL_ACCESS_H */
