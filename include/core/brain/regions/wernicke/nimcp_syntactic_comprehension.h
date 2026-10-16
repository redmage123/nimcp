/**
 * @file nimcp_syntactic_comprehension.h
 * @brief Syntactic Comprehension Layer for Wernicke's Area
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Sentence-level syntactic parsing and comprehension
 * WHY:  Extract grammatical structure for meaning integration
 * HOW:  Incremental parsing with predictive structure building
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * SYNTACTIC PROCESSING IN WERNICKE'S AREA:
 * ----------------------------------------
 * While Broca's area handles syntactic production, Wernicke's area
 * contributes to syntactic comprehension through:
 *
 *   1. INCREMENTAL PARSING:
 *      - Word-by-word structure building
 *      - Predictive completion of constituents
 *      - Garden-path detection and reanalysis
 *
 *   2. THEMATIC ROLE ASSIGNMENT:
 *      - Who did what to whom
 *      - Agent, patient, theme, goal, etc.
 *      - Verb argument structure matching
 *
 *   3. DEPENDENCY EXTRACTION:
 *      - Subject-verb agreement
 *      - Long-distance dependencies
 *      - Relative clause attachment
 *
 *   4. PHRASE STRUCTURE:
 *      - NP, VP, PP constituent recognition
 *      - Hierarchical grouping
 *      - Clause boundary detection
 *
 * PARSING STRATEGY:
 * -----------------
 * Uses a left-corner parsing approach with prediction:
 * - Bottom-up: Recognize completed constituents
 * - Top-down: Predict expected continuations
 * - Probabilistic: PCFG-style rule probabilities
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SYNTACTIC_COMPREHENSION_H
#define NIMCP_SYNTACTIC_COMPREHENSION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum sentence length in words */
#define SYNTACTIC_MAX_SENTENCE_LEN     64

/** @brief Maximum parse tree depth */
#define SYNTACTIC_MAX_TREE_DEPTH       16

/** @brief Maximum constituents in parse */
#define SYNTACTIC_MAX_CONSTITUENTS     128

/** @brief Maximum thematic roles per verb */
#define SYNTACTIC_MAX_ROLES            6

/** @brief Maximum dependencies per sentence */
#define SYNTACTIC_MAX_DEPENDENCIES     64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Syntactic category (part of speech)
 */
typedef enum {
    SYN_CAT_NOUN = 0,        /**< Noun (N) */
    SYN_CAT_VERB,            /**< Verb (V) */
    SYN_CAT_ADJ,             /**< Adjective (Adj) */
    SYN_CAT_ADV,             /**< Adverb (Adv) */
    SYN_CAT_DET,             /**< Determiner (Det) */
    SYN_CAT_PREP,            /**< Preposition (P) */
    SYN_CAT_CONJ,            /**< Conjunction (Conj) */
    SYN_CAT_PRON,            /**< Pronoun (Pron) */
    SYN_CAT_AUX,             /**< Auxiliary verb (Aux) */
    SYN_CAT_COMP,            /**< Complementizer (Comp) */
    SYN_CAT_NEG,             /**< Negation (Neg) */
    SYN_CAT_PUNCT,           /**< Punctuation */
    SYN_CAT_UNKNOWN,         /**< Unknown category */
    SYN_CAT_COUNT            /**< Number of categories */
} syntactic_category_t;

/**
 * @brief Phrase type
 */
typedef enum {
    PHRASE_NP = 0,           /**< Noun phrase */
    PHRASE_VP,               /**< Verb phrase */
    PHRASE_PP,               /**< Prepositional phrase */
    PHRASE_AP,               /**< Adjective phrase */
    PHRASE_ADVP,             /**< Adverb phrase */
    PHRASE_S,                /**< Sentence/clause */
    PHRASE_SBAR,             /**< Subordinate clause */
    PHRASE_CP,               /**< Complementizer phrase */
    PHRASE_IP,               /**< Inflectional phrase */
    PHRASE_DP,               /**< Determiner phrase */
    PHRASE_UNKNOWN,          /**< Unknown phrase type */
    PHRASE_COUNT             /**< Number of phrase types */
} phrase_type_t;

/**
 * @brief Thematic role
 */
typedef enum {
    ROLE_AGENT = 0,          /**< Doer of action */
    ROLE_PATIENT,            /**< Affected entity */
    ROLE_THEME,              /**< Entity undergoing change */
    ROLE_EXPERIENCER,        /**< Entity experiencing state */
    ROLE_GOAL,               /**< Endpoint of motion/transfer */
    ROLE_SOURCE,             /**< Starting point */
    ROLE_LOCATION,           /**< Place */
    ROLE_INSTRUMENT,         /**< Tool used */
    ROLE_BENEFICIARY,        /**< Entity benefiting */
    ROLE_TIME,               /**< Temporal reference */
    ROLE_MANNER,             /**< How action is performed */
    ROLE_CAUSE,              /**< What caused event */
    ROLE_UNKNOWN,            /**< Unknown role */
    ROLE_COUNT               /**< Number of roles */
} thematic_role_t;

/**
 * @brief Dependency relation type
 */
typedef enum {
    DEP_NSUBJ = 0,           /**< Nominal subject */
    DEP_DOBJ,                /**< Direct object */
    DEP_IOBJ,                /**< Indirect object */
    DEP_NMOD,                /**< Nominal modifier */
    DEP_AMOD,                /**< Adjectival modifier */
    DEP_ADVMOD,              /**< Adverbial modifier */
    DEP_DET,                 /**< Determiner */
    DEP_CASE,                /**< Case marker (preposition) */
    DEP_AUX,                 /**< Auxiliary */
    DEP_COP,                 /**< Copula */
    DEP_MARK,                /**< Subordinating marker */
    DEP_CONJ,                /**< Conjunct */
    DEP_CC,                  /**< Coordinating conjunction */
    DEP_ROOT,                /**< Root of sentence */
    DEP_PUNCT,               /**< Punctuation */
    DEP_UNKNOWN,             /**< Unknown relation */
    DEP_COUNT                /**< Number of relations */
} dependency_type_t;

/**
 * @brief Parse state
 */
typedef enum {
    PARSE_STATE_INIT = 0,    /**< Initial state */
    PARSE_STATE_ACTIVE,      /**< Actively parsing */
    PARSE_STATE_COMPLETE,    /**< Parse complete */
    PARSE_STATE_AMBIGUOUS,   /**< Multiple valid parses */
    PARSE_STATE_GARDEN_PATH, /**< Reanalysis needed */
    PARSE_STATE_ERROR        /**< Parse failed */
} parse_state_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Word with syntactic information
 */
typedef struct {
    char word[32];                   /**< Word string */
    uint32_t position;               /**< Position in sentence (0-indexed) */
    syntactic_category_t category;   /**< Part of speech */
    float category_confidence;       /**< POS confidence [0-1] */
    uint32_t lemma_id;               /**< Lemma identifier */
    bool is_head;                    /**< Is head of its phrase */
} syntactic_word_t;

/**
 * @brief Parse tree node (constituent)
 */
typedef struct syntactic_node {
    phrase_type_t phrase_type;       /**< Type of phrase */
    uint32_t start_pos;              /**< Start word position */
    uint32_t end_pos;                /**< End word position (exclusive) */
    struct syntactic_node* parent;   /**< Parent node (NULL for root) */
    struct syntactic_node** children;/**< Child nodes */
    uint32_t num_children;           /**< Number of children */
    uint32_t head_child;             /**< Index of head child */
    float probability;               /**< Rule probability */
    bool is_complete;                /**< Constituent complete */
} syntactic_node_t;

/**
 * @brief Dependency relation
 */
typedef struct {
    uint32_t head_pos;               /**< Position of head word */
    uint32_t dependent_pos;          /**< Position of dependent word */
    dependency_type_t relation;      /**< Type of dependency */
    float confidence;                /**< Relation confidence [0-1] */
} syntactic_dependency_t;

/**
 * @brief Thematic role assignment
 */
typedef struct {
    uint32_t verb_pos;               /**< Position of verb */
    uint32_t argument_start;         /**< Start of argument phrase */
    uint32_t argument_end;           /**< End of argument phrase */
    thematic_role_t role;            /**< Assigned role */
    float confidence;                /**< Assignment confidence [0-1] */
} thematic_assignment_t;

/**
 * @brief Complete parse result
 */
typedef struct {
    /* Words */
    syntactic_word_t* words;         /**< Words in sentence */
    uint32_t num_words;              /**< Number of words */

    /* Phrase structure */
    syntactic_node_t* root;          /**< Root of parse tree */
    syntactic_node_t* nodes;         /**< All constituent nodes */
    uint32_t num_nodes;              /**< Number of nodes */

    /* Dependencies */
    syntactic_dependency_t* dependencies; /**< Dependency relations */
    uint32_t num_dependencies;       /**< Number of dependencies */

    /* Thematic roles */
    thematic_assignment_t* roles;    /**< Role assignments */
    uint32_t num_roles;              /**< Number of assignments */

    /* Parse quality */
    parse_state_t state;             /**< Current parse state */
    float parse_probability;         /**< Overall parse probability */
    float syntactic_complexity;      /**< Syntactic complexity score */
    bool is_grammatical;             /**< Grammaticality judgment */
    uint32_t reanalysis_count;       /**< Garden-path reanalyses */

    /* === APPENDED 2026-05-11 (parser-coverage Wave) === */
    /* Sentence-type flags. Appended at struct end for ABI safety —
     * existing field offsets are preserved. */
    bool is_question;                /**< Yes/no or wh-question */
    bool is_imperative;              /**< Imperative (null subject) */

    /* Bitmask of which reduction rules fired during this parse.
     * Bit indices are the WERNICKE_REDUCTION_* constants below.
     * Zero means no reductions fired (parse failed or trivial). */
    uint32_t reductions_applied;     /**< Bitmask of fired reductions */
} syntactic_parse_t;

/* ============================================================================
 * Reduction-rule bitmask
 *
 * Tracks which grammar rules fired during a parse. Set on
 * syntactic_parse_t.reductions_applied so cascade Stage 7 (syntactic_validity)
 * and downstream consumers can introspect the grammar coverage that was
 * actually exercised by an utterance.
 * ============================================================================ */
#define WERNICKE_REDUCTION_NP_DET_N         (1u << 0)  /* DP + NP    → NP   */
#define WERNICKE_REDUCTION_PP_P_NP          (1u << 1)  /* PP + NP    → PP   */
#define WERNICKE_REDUCTION_VP_V_NP          (1u << 2)  /* VP + NP    → VP   */
#define WERNICKE_REDUCTION_VP_V_PP          (1u << 3)  /* VP + PP    → VP   */
#define WERNICKE_REDUCTION_S_NP_VP          (1u << 4)  /* NP + VP    → S    */
#define WERNICKE_REDUCTION_CP_WH_S          (1u << 5)  /* WH + S/VP  → CP   */
#define WERNICKE_REDUCTION_CP_AUX_S         (1u << 6)  /* AUX-inv    → CP   */
#define WERNICKE_REDUCTION_S_IMPERATIVE     (1u << 7)  /* VP-only    → S    */
#define WERNICKE_REDUCTION_NP_CONJ_NP       (1u << 8)  /* NP+Conj+NP → NP   */
#define WERNICKE_REDUCTION_S_CONJ_S         (1u << 9)  /* S+Conj+S   → S    */
#define WERNICKE_REDUCTION_VP_INTRANSITIVE  (1u << 10) /* lone VP    → VP   */

/**
 * @brief Incremental parse state
 */
typedef struct {
    syntactic_word_t* buffer;        /**< Word buffer */
    uint32_t buffer_len;             /**< Words in buffer */
    syntactic_node_t** stack;        /**< Parse stack */
    uint32_t stack_depth;            /**< Stack depth */
    float* predictions;              /**< Predicted categories */
    uint32_t prediction_len;         /**< Number of predictions */
    parse_state_t state;             /**< Current state */
} incremental_state_t;

/**
 * @brief Syntactic comprehension configuration
 */
typedef struct {
    /* Parsing parameters */
    uint32_t max_sentence_length;    /**< Max words per sentence */
    uint32_t beam_width;             /**< Beam search width */
    float pruning_threshold;         /**< Probability pruning threshold */

    /* Prediction */
    bool enable_prediction;          /**< Enable predictive parsing */
    float prediction_weight;         /**< Weight for predictions */

    /* Thematic roles */
    bool enable_thematic_roles;      /**< Enable role assignment */
    bool strict_argument_structure;  /**< Require verb argument match */

    /* Garden paths */
    bool enable_reanalysis;          /**< Allow garden-path reanalysis */
    uint32_t max_reanalyses;         /**< Maximum reanalysis attempts */

    /* Integration */
    bool enable_semantic_guide;      /**< Use semantic plausibility */
    float semantic_weight;           /**< Semantic guidance weight */
} syntactic_config_t;

/**
 * @brief Syntactic comprehension statistics
 */
typedef struct {
    uint64_t sentences_parsed;       /**< Total sentences parsed */
    uint64_t words_processed;        /**< Total words processed */
    uint64_t constituents_built;     /**< Constituents constructed */
    uint64_t garden_paths;           /**< Garden-path sentences */
    uint64_t reanalyses;             /**< Total reanalyses */
    uint64_t parse_failures;         /**< Failed parses */
    float avg_sentence_length;       /**< Average sentence length */
    float avg_tree_depth;            /**< Average tree depth */
    float avg_parse_probability;     /**< Average parse probability */
    float avg_complexity;            /**< Average complexity */
} syntactic_stats_t;

/**
 * @brief Syntactic comprehension context
 */
typedef struct syntactic_comprehension syntactic_comprehension_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int syntactic_default_config(syntactic_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create syntactic comprehension context
 *
 * @param config Configuration (NULL for defaults)
 * @return Context handle or NULL on failure
 */
syntactic_comprehension_t* syntactic_comprehension_create(
    const syntactic_config_t* config);

/**
 * @brief Destroy context and free resources
 *
 * @param ctx Context to destroy
 */
void syntactic_comprehension_destroy(syntactic_comprehension_t* ctx);

/* ============================================================================
 * Parsing API
 * ============================================================================ */

/**
 * @brief Parse complete sentence
 *
 * @param ctx Comprehension context
 * @param words Array of words with POS tags
 * @param num_words Number of words
 * @param parse Output parse result
 * @return 0 on success, -1 on error
 */
int syntactic_parse_sentence(syntactic_comprehension_t* ctx,
                              const syntactic_word_t* words,
                              uint32_t num_words,
                              syntactic_parse_t* parse);

/**
 * @brief Begin incremental parsing
 *
 * @param ctx Comprehension context
 * @return 0 on success, -1 on error
 */
int syntactic_begin_incremental(syntactic_comprehension_t* ctx);

/**
 * @brief Add word to incremental parse
 *
 * @param ctx Comprehension context
 * @param word Word to add
 * @return Parse state after adding word
 */
parse_state_t syntactic_add_word(syntactic_comprehension_t* ctx,
                                  const syntactic_word_t* word);

/**
 * @brief Finish incremental parse
 *
 * @param ctx Comprehension context
 * @param parse Output parse result
 * @return 0 on success, -1 on error
 */
int syntactic_finish_incremental(syntactic_comprehension_t* ctx,
                                  syntactic_parse_t* parse);

/**
 * @brief Get current incremental state
 *
 * @param ctx Comprehension context
 * @param state Output incremental state
 * @return 0 on success, -1 on error
 */
int syntactic_get_incremental_state(const syntactic_comprehension_t* ctx,
                                     incremental_state_t* state);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Predict next syntactic category
 *
 * @param ctx Comprehension context
 * @param probs Output probability distribution [SYN_CAT_COUNT]
 * @return Most likely category
 */
syntactic_category_t syntactic_predict_category(
    const syntactic_comprehension_t* ctx,
    float* probs);

/**
 * @brief Predict expected phrase completion
 *
 * @param ctx Comprehension context
 * @param phrase_type Output expected phrase type
 * @param confidence Output prediction confidence
 * @return 0 on success, -1 on error
 */
int syntactic_predict_phrase(const syntactic_comprehension_t* ctx,
                              phrase_type_t* phrase_type,
                              float* confidence);

/* ============================================================================
 * Dependency API
 * ============================================================================ */

/**
 * @brief Extract dependencies from parse
 *
 * @param ctx Comprehension context
 * @param parse Parse result
 * @param dependencies Output dependencies
 * @param max_deps Maximum dependencies to extract
 * @param num_deps Output actual count
 * @return 0 on success, -1 on error
 */
int syntactic_extract_dependencies(syntactic_comprehension_t* ctx,
                                    const syntactic_parse_t* parse,
                                    syntactic_dependency_t* dependencies,
                                    uint32_t max_deps,
                                    uint32_t* num_deps);

/**
 * @brief Find head of phrase
 *
 * @param node Phrase node
 * @return Head word position, or -1 on error
 */
int32_t syntactic_find_head(const syntactic_node_t* node);

/* ============================================================================
 * Thematic Role API
 * ============================================================================ */

/**
 * @brief Assign thematic roles for sentence
 *
 * @param ctx Comprehension context
 * @param parse Parse result
 * @param roles Output role assignments
 * @param max_roles Maximum roles
 * @param num_roles Output actual count
 * @return 0 on success, -1 on error
 */
int syntactic_assign_roles(syntactic_comprehension_t* ctx,
                            const syntactic_parse_t* parse,
                            thematic_assignment_t* roles,
                            uint32_t max_roles,
                            uint32_t* num_roles);

/**
 * @brief Get verb argument structure
 *
 * @param ctx Comprehension context
 * @param verb_word Verb word
 * @param expected_roles Output expected roles
 * @param num_roles Output count
 * @return 0 on success, -1 on error
 */
int syntactic_get_argument_structure(syntactic_comprehension_t* ctx,
                                      const syntactic_word_t* verb_word,
                                      thematic_role_t* expected_roles,
                                      uint32_t* num_roles);

/* ============================================================================
 * Garden Path API
 * ============================================================================ */

/**
 * @brief Check if current parse is garden path
 *
 * @param ctx Comprehension context
 * @return true if garden path detected
 */
bool syntactic_is_garden_path(const syntactic_comprehension_t* ctx);

/**
 * @brief Attempt reanalysis of garden path
 *
 * @param ctx Comprehension context
 * @param parse Parse to reanalyze
 * @return 0 on success, -1 if reanalysis failed
 */
int syntactic_reanalyze(syntactic_comprehension_t* ctx,
                         syntactic_parse_t* parse);

/**
 * @brief Get reanalysis suggestions
 *
 * @param ctx Comprehension context
 * @param suggestions Output suggested attachment points
 * @param max_suggestions Maximum suggestions
 * @param num_suggestions Output actual count
 * @return 0 on success, -1 on error
 */
int syntactic_get_reanalysis_suggestions(
    const syntactic_comprehension_t* ctx,
    uint32_t* suggestions,
    uint32_t max_suggestions,
    uint32_t* num_suggestions);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get parse statistics
 *
 * @param ctx Comprehension context
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int syntactic_get_stats(const syntactic_comprehension_t* ctx,
                         syntactic_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param ctx Comprehension context
 */
void syntactic_reset_stats(syntactic_comprehension_t* ctx);

/**
 * @brief Get syntactic complexity of parse
 *
 * @param parse Parse result
 * @return Complexity score [0-1]
 */
float syntactic_compute_complexity(const syntactic_parse_t* parse);

/**
 * @brief Check grammaticality of parse
 *
 * @param ctx Comprehension context
 * @param parse Parse to check
 * @return true if grammatical
 */
bool syntactic_is_grammatical(const syntactic_comprehension_t* ctx,
                               const syntactic_parse_t* parse);

/* ============================================================================
 * Memory Management API
 * ============================================================================ */

/**
 * @brief Free parse result memory
 *
 * @param parse Parse to free
 */
void syntactic_parse_free(syntactic_parse_t* parse);

/**
 * @brief Clone parse result
 *
 * @param src Source parse
 * @param dst Destination parse
 * @return 0 on success, -1 on error
 */
int syntactic_parse_clone(const syntactic_parse_t* src,
                           syntactic_parse_t* dst);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* syntactic_category_to_string(syntactic_category_t cat);
const char* syntactic_phrase_to_string(phrase_type_t phrase);
const char* syntactic_role_to_string(thematic_role_t role);
const char* syntactic_dependency_to_string(dependency_type_t dep);
const char* syntactic_state_to_string(parse_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYNTACTIC_COMPREHENSION_H */
