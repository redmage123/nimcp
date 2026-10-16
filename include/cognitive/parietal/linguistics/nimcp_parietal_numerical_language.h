/**
 * @file nimcp_parietal_numerical_language.h
 * @brief Numerical Language Processing Module (Intraparietal Sulcus)
 * @version 1.0.0
 * @date 2025-01-31
 *
 * WHAT: Number word parsing, ordinal processing, quantifier semantics,
 *       and bidirectional number↔word conversion
 *
 * WHY:  The intraparietal sulcus (IPS) is critical for numerical cognition,
 *       including mapping between linguistic and magnitude representations
 *
 * BIOLOGICAL BASIS:
 * - IPS neurons encode numerical magnitudes with Weber-Fechner scaling
 * - Bidirectional mapping: symbolic (words) ↔ analog (magnitudes)
 * - Approximate quantities inherit number sense uncertainty
 *
 * FUZZY INTEGRATION:
 * Approximate quantifiers map to fuzzy membership functions:
 * - "few" → Left-skewed MF (low proportions)
 * - "many" → Right-skewed MF (high proportions)
 * - "most" → S-shaped MF (majority proportion)
 *
 * HMM INTEGRATION:
 * Number word sequences modeled via HMM:
 * - P("one" | "twenty-") predicts next word
 * - Viterbi decoding for spoken number recognition
 *
 * USAGE:
 * ```c
 * numerical_language_t* nl = numerical_language_create();
 *
 * // Parse a number word
 * numerical_semantics_t sem;
 * numerical_language_parse_word(nl, "twenty-three", &sem);
 * // sem.magnitude = 23.0
 *
 * // Generate word from number
 * char word[64];
 * numerical_language_generate_word(nl, 42.0f, word, sizeof(word));
 * // word = "forty-two"
 *
 * // Parse ordinal
 * numerical_language_parse_ordinal(nl, "third", &sem);
 * // sem.ordinal_position = 3
 *
 * numerical_language_destroy(nl);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PARIETAL_NUMERICAL_LANGUAGE_H
#define NIMCP_PARIETAL_NUMERICAL_LANGUAGE_H

#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_types.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum parseable number */
#define NUMERICAL_LANG_MAX_VALUE            999999999.0f

/** Default Weber fraction for magnitude uncertainty */
#define NUMERICAL_LANG_DEFAULT_WEBER        0.15f

/** Maximum compound number word length */
#define NUMERICAL_LANG_MAX_COMPOUND_LEN     128

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for numerical language processor */
typedef struct numerical_language numerical_language_t;

/**
 * @brief Numerical language configuration
 */
typedef struct {
    /* Number sense integration */
    float weber_fraction;           /**< Weber fraction for uncertainty (default: 0.15) */
    bool enable_approximate_mode;   /**< Enable fuzzy quantifiers (default: true) */

    /* Word generation */
    bool use_hyphenation;           /**< Hyphenate compound numbers (default: true) */
    bool use_and_for_tens;          /**< "one hundred and one" (default: false) */

    /* Feature flags */
    bool enable_ordinals;           /**< Enable ordinal parsing (default: true) */
    bool enable_fractions;          /**< Enable fraction parsing (default: true) */
    bool enable_multipliers;        /**< Enable multipliers (double, triple) (default: true) */
    bool enable_bio_async;          /**< Enable bio-async messaging (default: false) */
    bool enable_mesh_participation; /**< Participate in linguistics mesh (default: true) */

    /* Modulation */
    float inflammation_sensitivity; /**< Immune modulation factor (0-1) */
    float fatigue_sensitivity;      /**< Fatigue modulation factor (0-1) */
} numerical_language_config_t;

/**
 * @brief Quantifier definition with fuzzy semantics
 */
typedef struct {
    linguistic_quantifier_t type;   /**< Quantifier category */
    char word[32];                  /**< Word string */
    fuzzy_mf_t proportion_mf;       /**< Membership function over [0,1] proportion */
} quantifier_definition_t;

/**
 * @brief Number word definition
 */
typedef struct {
    char word[32];                  /**< Word string */
    float value;                    /**< Numeric value */
    number_word_type_t type;        /**< Word type (cardinal, ordinal, etc.) */
    bool is_base;                   /**< Is a base word (one, ten, hundred...) */
    uint32_t magnitude_class;       /**< 0=units, 1=tens, 2=hundreds, etc. */
} number_word_entry_t;

/**
 * @brief Numerical language statistics
 */
typedef struct {
    uint64_t words_parsed;          /**< Total words parsed */
    uint64_t words_generated;       /**< Total words generated */
    uint64_t ordinals_parsed;       /**< Ordinals parsed */
    uint64_t quantifiers_parsed;    /**< Quantifiers parsed */
    uint64_t fractions_parsed;      /**< Fractions parsed */
    uint64_t unknown_words;         /**< Unknown word encounters */

    float avg_confidence;           /**< Average parse confidence */
    float avg_processing_time_us;   /**< Average processing time */
} numerical_language_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create numerical language processor with default configuration
 *
 * @return Handle or NULL on error
 */
numerical_language_t* numerical_language_create(void);

/**
 * @brief Create numerical language processor with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
numerical_language_t* numerical_language_create_custom(const numerical_language_config_t* config);

/**
 * @brief Destroy numerical language processor
 *
 * @param nl Handle (NULL safe)
 */
void numerical_language_destroy(numerical_language_t* nl);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
numerical_language_config_t numerical_language_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool numerical_language_validate_config(const numerical_language_config_t* config);

/* ============================================================================
 * PARSING API
 * ============================================================================ */

/**
 * @brief Parse a number word or phrase
 *
 * Converts written number words to numeric value.
 * Supports:
 * - Simple: "one", "twelve", "hundred"
 * - Compound: "twenty-three", "one hundred forty-five"
 * - Large: "one million two hundred thousand"
 *
 * @param nl Numerical language handle
 * @param word Number word or phrase
 * @param out Output semantics
 * @return 0 on success, error code on failure
 */
int numerical_language_parse_word(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
);

/**
 * @brief Parse an ordinal word
 *
 * Converts ordinal words to position number.
 * E.g., "first" → 1, "twenty-third" → 23
 *
 * @param nl Numerical language handle
 * @param word Ordinal word
 * @param out Output semantics
 * @return 0 on success, error code on failure
 */
int numerical_language_parse_ordinal(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
);

/**
 * @brief Parse a fraction word
 *
 * Converts fraction words to numeric ratio.
 * E.g., "half" → 0.5, "three quarters" → 0.75
 *
 * @param nl Numerical language handle
 * @param word Fraction word or phrase
 * @param out Output semantics
 * @return 0 on success, error code on failure
 */
int numerical_language_parse_fraction(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
);

/**
 * @brief Parse a quantifier word
 *
 * Converts quantifier to fuzzy membership function.
 * E.g., "few" → MF peaked at low proportions
 *
 * @param nl Numerical language handle
 * @param word Quantifier word
 * @param out Output semantics
 * @return 0 on success, error code on failure
 */
int numerical_language_parse_quantifier(
    numerical_language_t* nl,
    const char* word,
    numerical_semantics_t* out
);

/**
 * @brief Check if word is a known number word
 *
 * @param nl Numerical language handle
 * @param word Word to check
 * @return true if known number word
 */
bool numerical_language_is_number_word(
    const numerical_language_t* nl,
    const char* word
);

/* ============================================================================
 * GENERATION API
 * ============================================================================ */

/**
 * @brief Generate word from number
 *
 * Converts numeric value to written form.
 * E.g., 23 → "twenty-three"
 *
 * @param nl Numerical language handle
 * @param number Numeric value
 * @param word Output word buffer
 * @param max_len Maximum buffer length
 * @return 0 on success, error code on failure
 */
int numerical_language_generate_word(
    numerical_language_t* nl,
    float number,
    char* word,
    uint32_t max_len
);

/**
 * @brief Generate ordinal word from position
 *
 * Converts position number to ordinal word.
 * E.g., 23 → "twenty-third"
 *
 * @param nl Numerical language handle
 * @param position Position number (1-indexed)
 * @param word Output word buffer
 * @param max_len Maximum buffer length
 * @return 0 on success, error code on failure
 */
int numerical_language_generate_ordinal(
    numerical_language_t* nl,
    uint32_t position,
    char* word,
    uint32_t max_len
);

/* ============================================================================
 * FUZZY QUANTIFIER API
 * ============================================================================ */

/**
 * @brief Evaluate quantifier at proportion
 *
 * Returns membership degree for quantifier at given proportion.
 * E.g., quantifier_evaluate("most", 0.8) → high membership
 *
 * @param nl Numerical language handle
 * @param quantifier Quantifier type
 * @param proportion Proportion value [0,1]
 * @return Membership degree [0,1]
 */
float numerical_language_quantifier_evaluate(
    const numerical_language_t* nl,
    linguistic_quantifier_t quantifier,
    float proportion
);

/**
 * @brief Get quantifier membership function
 *
 * @param nl Numerical language handle
 * @param quantifier Quantifier type
 * @param out Output membership function
 * @return 0 on success
 */
int numerical_language_get_quantifier_mf(
    const numerical_language_t* nl,
    linguistic_quantifier_t quantifier,
    fuzzy_mf_t* out
);

/**
 * @brief Select best quantifier for proportion
 *
 * Returns the quantifier with highest membership at given proportion.
 *
 * @param nl Numerical language handle
 * @param proportion Proportion value [0,1]
 * @param out Output quantifier
 * @return 0 on success
 */
int numerical_language_select_quantifier(
    const numerical_language_t* nl,
    float proportion,
    linguistic_quantifier_t* out
);

/* ============================================================================
 * NUMBER SENSE INTEGRATION API
 * ============================================================================ */

/**
 * @brief Get Weber-Fechner uncertainty for magnitude
 *
 * Returns uncertainty based on Weber's law.
 *
 * @param nl Numerical language handle
 * @param magnitude Numeric magnitude
 * @return Uncertainty (σ)
 */
float numerical_language_get_uncertainty(
    const numerical_language_t* nl,
    float magnitude
);

/**
 * @brief Check if number is in subitizing range
 *
 * Numbers 1-4 can be instantly recognized (subitized).
 *
 * @param nl Numerical language handle
 * @param magnitude Numeric magnitude
 * @return true if subitizable
 */
bool numerical_language_is_subitizable(
    const numerical_language_t* nl,
    float magnitude
);

/* ============================================================================
 * MESH INTEGRATION API
 * ============================================================================ */

/**
 * @brief Process mesh request and produce belief
 *
 * Implements linguistics_mesh_handler_t::process for mesh participation.
 *
 * @param nl Numerical language handle
 * @param request Mesh request
 * @param belief Output belief with precision
 * @return 0 on success
 */
int numerical_language_mesh_process(
    numerical_language_t* nl,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
);

/**
 * @brief Update belief based on neighbor beliefs
 *
 * Implements FEP update: μ' = μ - lr * Π * ε
 *
 * @param nl Numerical language handle
 * @param neighbor_beliefs Beliefs from mesh neighbors
 * @param neighbor_count Number of neighbor beliefs
 * @param updated_belief Output updated belief
 * @return 0 on success
 */
int numerical_language_mesh_update(
    numerical_language_t* nl,
    const linguistics_belief_t* neighbor_beliefs,
    uint32_t neighbor_count,
    linguistics_belief_t* updated_belief
);

/**
 * @brief Get current precision (inverse prediction error variance)
 *
 * @param nl Numerical language handle
 * @return Precision Π ∈ [PRECISION_FLOOR, PRECISION_CEILING]
 */
float numerical_language_get_precision(const numerical_language_t* nl);

/**
 * @brief Get mesh handler interface
 *
 * @param nl Numerical language handle
 * @param handler Output handler struct
 * @return 0 on success
 */
int numerical_language_get_mesh_handler(
    numerical_language_t* nl,
    linguistics_mesh_handler_t* handler
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level for immune modulation
 *
 * @param nl Numerical language handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int numerical_language_set_inflammation(
    numerical_language_t* nl,
    float level
);

/**
 * @brief Set fatigue level
 *
 * @param nl Numerical language handle
 * @param level Fatigue level [0,1]
 * @return 0 on success
 */
int numerical_language_set_fatigue(
    numerical_language_t* nl,
    float level
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param nl Numerical language handle
 * @param stats Output statistics
 * @return 0 on success
 */
int numerical_language_get_stats(
    const numerical_language_t* nl,
    numerical_language_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param nl Numerical language handle
 */
void numerical_language_reset_stats(numerical_language_t* nl);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* numerical_language_get_last_error(void);

/* ============================================================================
 * UTILITY API
 * ============================================================================ */

/**
 * @brief Get human-readable name for number word type
 *
 * @param type Number word type
 * @return Static string name
 */
const char* numerical_language_type_name(number_word_type_t type);

/**
 * @brief Get human-readable name for quantifier
 *
 * @param quantifier Quantifier type
 * @return Static string name
 */
const char* numerical_language_quantifier_name(linguistic_quantifier_t quantifier);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_NUMERICAL_LANGUAGE_H */
