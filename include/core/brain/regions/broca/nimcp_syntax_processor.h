/**
 * @file nimcp_syntax_processor.h
 * @brief Syntax processing module for Broca's region
 *
 * WHAT: Sentence structure generation, grammar rule application, and syntactic tree construction
 * WHY:  Enable grammatical language production in NIMCP brain
 * HOW:  Phrase structure rules + morphological processing + syntactic tree building
 *
 * BIOLOGICAL CONTEXT:
 * Models Broca's area (BA 44/45) syntax processing:
 * - BA 44 (pars opercularis): Syntactic hierarchy and phrase structure
 * - BA 45 (pars triangularis): Morphological processing and word formation
 * - Left inferior frontal gyrus: Grammar rule application
 * - Sequential processing: Word order and constituent structure
 *
 * DESIGN PRINCIPLES:
 * - Hierarchical: Words → Phrases → Clauses → Sentences
 * - Compositional: Bottom-up tree construction
 * - Rule-based: Context-free grammar with transformations
 * - Incremental: Online parsing and generation
 *
 * REFERENCES:
 * - Broca (1861) "Remarks on the seat of the faculty of articulated language"
 * - Chomsky (1957) "Syntactic Structures"
 * - Friederici (2011) "The brain basis of language processing"
 * - Hagoort (2005) "On Broca, brain, and binding"
 * - Grodzinsky & Santi (2008) "The battle for Broca's region"
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 * @version 2.7 (Phase 8.8)
 */

#ifndef NIMCP_SYNTAX_PROCESSOR_H
#define NIMCP_SYNTAX_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct brain_struct* brain_t;

//=============================================================================
// Configuration Constants
//=============================================================================

/** Maximum syntactic units in a sentence */
#define SYNTAX_MAX_UNITS 256

/** Maximum tree depth (typical: 8-12 for complex sentences) */
#define SYNTAX_MAX_TREE_DEPTH 16

/** Maximum phrase structure rules */
#define SYNTAX_MAX_RULES 128

/** Maximum morphemes per word */
#define SYNTAX_MAX_MORPHEMES 8

/** Maximum sentence length (words) */
#define SYNTAX_MAX_SENTENCE_LENGTH 64

//=============================================================================
// Syntactic Categories and Structures
//=============================================================================

/**
 * @brief Part of speech (POS) categories
 *
 * WHAT: Grammatical word classes
 * WHY:  Fundamental building blocks for phrase structure
 * HOW:  Standard linguistic categories with subcategories
 */
typedef enum {
    // === MAJOR CATEGORIES ===
    POS_NOUN,           /**< Noun (person, place, thing) */
    POS_VERB,           /**< Verb (action, state) */
    POS_ADJECTIVE,      /**< Adjective (modifier) */
    POS_ADVERB,         /**< Adverb (manner, time, place) */
    POS_PRONOUN,        /**< Pronoun (he, she, it, they) */

    // === FUNCTIONAL CATEGORIES ===
    POS_DETERMINER,     /**< Determiner (the, a, this, that) */
    POS_PREPOSITION,    /**< Preposition (in, on, at, to) */
    POS_CONJUNCTION,    /**< Conjunction (and, but, or) */
    POS_COMPLEMENTIZER, /**< Complementizer (that, if, whether) */
    POS_AUXILIARY,      /**< Auxiliary verb (is, have, will) */

    // === SPECIAL ===
    POS_PARTICLE,       /**< Particle (up, down, out) */
    POS_INTERJECTION,   /**< Interjection (oh, wow, ouch) */
    POS_PUNCTUATION,    /**< Punctuation mark */
    POS_UNKNOWN,        /**< Unknown category */

    POS_COUNT
} part_of_speech_t;

/**
 * @brief Phrase types (X-bar theory)
 *
 * WHAT: Phrasal categories in hierarchical syntax
 * WHY:  Organize words into larger constituents
 * HOW:  X-bar theory: XP → Specifier + X' → X' + Complement
 */
typedef enum {
    PHRASE_NONE,        /**< Not a phrase */
    PHRASE_NP,          /**< Noun Phrase (the cat) */
    PHRASE_VP,          /**< Verb Phrase (ate fish) */
    PHRASE_AP,          /**< Adjective Phrase (very tall) */
    PHRASE_PP,          /**< Prepositional Phrase (in the box) */
    PHRASE_ADVP,        /**< Adverb Phrase (very quickly) */
    PHRASE_CP,          /**< Complementizer Phrase (that she left) */
    PHRASE_IP,          /**< Inflectional Phrase (sentence) */
    PHRASE_DP,          /**< Determiner Phrase (extended NP) */
    /* APPENDED — keep at end for ABI safety */
    PHRASE_CONJP,       /**< Conjunction Phrase (and / or / but) */

    PHRASE_COUNT
} phrase_type_t;

/**
 * @brief Grammatical features
 *
 * WHAT: Morphosyntactic features for agreement and inflection
 * WHY:  Track number, person, tense, case, etc.
 * HOW:  Feature bundle representation
 */
typedef struct {
    // === NUMBER ===
    uint8_t number;     /**< 0=unmarked, 1=singular, 2=plural */

    // === PERSON ===
    uint8_t person;     /**< 0=unmarked, 1=first, 2=second, 3=third */

    // === TENSE ===
    uint8_t tense;      /**< 0=unmarked, 1=present, 2=past, 3=future */

    // === ASPECT ===
    uint8_t aspect;     /**< 0=unmarked, 1=progressive, 2=perfect */

    // === MOOD ===
    uint8_t mood;       /**< 0=unmarked, 1=indicative, 2=subjunctive, 3=imperative */

    // === CASE ===
    uint8_t case_type;  /**< 0=unmarked, 1=nominative, 2=accusative, 3=genitive */

    // === AGREEMENT ===
    bool has_agreement; /**< Whether this element participates in agreement */

} grammatical_features_t;

/**
 * @brief Syntactic unit (word with grammatical info)
 *
 * WHAT: Basic unit of syntactic processing
 * WHY:  Represent words with POS and features
 * HOW:  Combines lexical ID with grammatical information
 */
typedef struct {
    part_of_speech_t pos;           /**< Part of speech */
    uint32_t word_id;                /**< Lexical item ID */
    uint32_t phrase_level;           /**< Phrase level in tree (0=word, 1=phrase, etc.) */
    phrase_type_t phrase_type;       /**< Type of phrase (if phrase_level > 0) */
    grammatical_features_t features; /**< Morphosyntactic features */
} syntactic_unit_t;

/**
 * @brief Syntactic tree node
 *
 * WHAT: Node in phrase structure tree
 * WHY:  Represent hierarchical sentence structure
 * HOW:  Binary branching with left/right children
 */
typedef struct syntax_tree_node {
    syntactic_unit_t unit;              /**< Syntactic unit at this node */
    struct syntax_tree_node* left;      /**< Left child (specifier/head) */
    struct syntax_tree_node* right;     /**< Right child (complement) */
    struct syntax_tree_node* parent;    /**< Parent node (for traversal) */
    uint32_t depth;                     /**< Depth in tree */
    bool is_head;                       /**< Whether this is a head of phrase */
} syntax_tree_node_t;

/**
 * @brief Phrase structure rule (CFG production)
 *
 * WHAT: Context-free grammar rule
 * WHY:  Define legal phrase structures
 * HOW:  Rewrite rule: LHS → RHS1 RHS2 ...
 */
typedef struct {
    phrase_type_t lhs;                  /**< Left-hand side (parent) */
    phrase_type_t rhs[4];               /**< Right-hand side (children, max 4) */
    uint32_t num_rhs;                   /**< Number of RHS elements */
    float probability;                  /**< Rule probability (for PCFG) */
    bool is_active;                     /**< Whether rule is enabled */
} phrase_structure_rule_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Syntax processor configuration
 */
typedef struct {
    uint32_t max_units;              /**< Maximum syntactic units */
    uint32_t max_tree_depth;         /**< Maximum tree depth */
    uint32_t max_rules;              /**< Maximum phrase structure rules */
    uint32_t max_morphemes;          /**< Maximum morphemes per word */
    bool enable_morphology;          /**< Enable morphological processing */
    bool enable_agreement;           /**< Enable subject-verb agreement */
    bool enable_movement;            /**< Enable transformational movement */
    bool enable_tree_caching;        /**< Cache recently built trees */

    // NIMCP integration
    bool enable_neuromodulation;     /**< Enable DA/ACh modulation */
    float learning_rate;             /**< Rule learning rate */

} syntax_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Syntax processor instance (opaque)
 *
 * WHAT: Internal syntax processor state
 * WHY:  Encapsulation and data hiding
 * HOW:  Opaque pointer pattern
 */
typedef struct syntax_processor syntax_processor_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default syntax processor configuration
 *
 * WHAT: Return sensible defaults
 * WHY:  Simplify initialization
 * HOW:  Static configuration with standard values
 *
 * @return Default configuration
 */
syntax_config_t syntax_default_config(void);

/**
 * @brief Create syntax processor instance
 *
 * WHAT: Allocate and initialize syntax processor
 * WHY:  Enable syntactic processing in Broca's region
 * HOW:  Allocate memory, initialize rules, build grammar
 *
 * COMPLEXITY: O(R) where R = max_rules
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Syntax processor instance or NULL on failure
 */
syntax_processor_t* syntax_create(const syntax_config_t* config);

/**
 * @brief Destroy syntax processor instance
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Free trees, rules, and internal state
 *
 * @param processor Syntax processor to destroy (NULL-safe)
 */
void syntax_destroy(syntax_processor_t* processor);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Add syntactic unit to processor
 *
 * WHAT: Add word/phrase to current sentence buffer
 * WHY:  Build up sentence for parsing
 * HOW:  Append to internal unit buffer
 *
 * @param processor Syntax processor instance
 * @param unit Syntactic unit to add
 * @return true on success, false if buffer full or NULL input
 */
bool syntax_add_unit(syntax_processor_t* processor, const syntactic_unit_t* unit);

/**
 * @brief Build syntactic tree from units
 *
 * WHAT: Parse units into hierarchical phrase structure
 * WHY:  Convert linear word sequence into tree
 * HOW:  Bottom-up chart parsing with phrase structure rules
 *
 * ALGORITHM:
 * 1. Initialize chart with lexical items (words)
 * 2. Apply phrase structure rules bottom-up
 * 3. Build tree from successful parses
 * 4. Select best parse (if multiple)
 *
 * COMPLEXITY: O(N³) for CYK parsing where N = sentence length
 *
 * @param processor Syntax processor instance
 * @return true on success, false on parse failure or NULL input
 */
bool syntax_build_tree(syntax_processor_t* processor);

/**
 * @brief Validate grammar of current sentence
 *
 * WHAT: Check if sentence is grammatically well-formed
 * WHY:  Detect syntax errors before generation
 * HOW:  Check phrase structure rules and agreement
 *
 * CHECKS:
 * - Subject-verb agreement
 * - Case assignment
 * - Complete phrase structure
 * - Movement constraints
 *
 * @param processor Syntax processor instance
 * @param is_valid Output: true if grammatical, false otherwise
 * @return true on success (check completed), false on NULL input
 */
bool syntax_validate_grammar(syntax_processor_t* processor, bool* is_valid);

/**
 * @brief Reset syntax processor state
 *
 * WHAT: Clear current sentence and tree
 * WHY:  Prepare for new sentence
 * HOW:  Clear unit buffer and free tree
 *
 * @param processor Syntax processor instance
 * @return true on success, false on NULL input
 */
bool syntax_reset(syntax_processor_t* processor);

//=============================================================================
// Tree Operations
//=============================================================================

/**
 * @brief Get root of current syntactic tree
 *
 * WHAT: Return root node of phrase structure tree
 * WHY:  Access full sentence structure
 * HOW:  Return cached tree root
 *
 * @param processor Syntax processor instance
 * @return Root node or NULL if no tree built
 */
const syntax_tree_node_t* syntax_get_tree_root(const syntax_processor_t* processor);

/**
 * @brief Get tree depth
 *
 * WHAT: Return maximum depth of current tree
 * WHY:  Measure sentence complexity
 * HOW:  Return cached depth value
 *
 * @param processor Syntax processor instance
 * @return Tree depth or 0 if no tree
 */
uint32_t syntax_get_tree_depth(const syntax_processor_t* processor);

/**
 * @brief Print syntactic tree (debug)
 *
 * WHAT: Print tree structure to stdout
 * WHY:  Debugging and visualization
 * HOW:  Recursive tree traversal with indentation
 *
 * @param processor Syntax processor instance
 * @return true on success, false on NULL input
 */
bool syntax_print_tree(const syntax_processor_t* processor);

//=============================================================================
// Morphological Processing
//=============================================================================

/**
 * @brief Apply morphological inflection
 *
 * WHAT: Add inflectional morphemes (tense, number, etc.)
 * WHY:  Generate correct word forms (walk → walked)
 * HOW:  Apply morphological rules based on features
 *
 * EXAMPLES:
 * - Plural: cat + -s → cats
 * - Past tense: walk + -ed → walked
 * - Third person: walk + -s → walks
 *
 * @param processor Syntax processor instance
 * @param unit Syntactic unit with features
 * @param inflected_form Output buffer for inflected form
 * @param buffer_size Size of output buffer
 * @return true on success, false on error
 */
bool syntax_apply_inflection(
    syntax_processor_t* processor,
    const syntactic_unit_t* unit,
    char* inflected_form,
    uint32_t buffer_size
);

/**
 * @brief Decompose word into morphemes
 *
 * WHAT: Break word into constituent morphemes
 * WHY:  Understand internal word structure
 * HOW:  Morphological analysis (prefix + root + suffix)
 *
 * EXAMPLE: unhappiness → un- + happy + -ness
 *
 * @param processor Syntax processor instance
 * @param word Word to decompose
 * @param morphemes Output buffer for morphemes
 * @param max_morphemes Size of output buffer
 * @param num_morphemes Number of morphemes found
 * @return true on success, false on error
 */
bool syntax_decompose_morphemes(
    syntax_processor_t* processor,
    const char* word,
    char morphemes[][32],
    uint32_t max_morphemes,
    uint32_t* num_morphemes
);

//=============================================================================
// Grammar Rule Management
//=============================================================================

/**
 * @brief Add phrase structure rule
 *
 * WHAT: Add new grammar rule
 * WHY:  Extend grammar dynamically
 * HOW:  Append to rule table
 *
 * EXAMPLE: VP → V NP (verb phrase = verb + noun phrase)
 *
 * @param processor Syntax processor instance
 * @param rule Phrase structure rule to add
 * @return true on success, false if rule table full
 */
bool syntax_add_rule(syntax_processor_t* processor, const phrase_structure_rule_t* rule);

/**
 * @brief Get number of active rules
 *
 * @param processor Syntax processor instance
 * @return Number of active rules
 */
uint32_t syntax_get_rule_count(const syntax_processor_t* processor);

/**
 * @brief Load default phrase structure rules
 *
 * WHAT: Initialize grammar with standard English rules
 * WHY:  Provide baseline grammar
 * HOW:  Load rule set for basic English syntax
 *
 * RULES INCLUDE:
 * - S → NP VP (sentence)
 * - VP → V NP (verb phrase)
 * - NP → Det N (noun phrase)
 * - PP → P NP (prepositional phrase)
 * - etc.
 *
 * @param processor Syntax processor instance
 * @return true on success, false on error
 */
bool syntax_load_default_rules(syntax_processor_t* processor);

//=============================================================================
// Statistics and Introspection
//=============================================================================

/**
 * @brief Syntax processor statistics
 */
typedef struct {
    uint64_t sentences_processed;    /**< Total sentences parsed */
    uint64_t successful_parses;      /**< Successful parses */
    uint64_t failed_parses;          /**< Failed parses */
    uint64_t agreement_violations;   /**< Agreement errors detected */
    uint64_t morphological_ops;      /**< Morphological operations */
    float avg_tree_depth;            /**< Average tree depth */
    float avg_sentence_length;       /**< Average sentence length */
} syntax_stats_t;

/**
 * @brief Get syntax processor statistics
 *
 * @param processor Syntax processor instance
 * @param stats Output statistics structure
 * @return true on success, false on NULL input
 */
bool syntax_get_stats(const syntax_processor_t* processor, syntax_stats_t* stats);

/**
 * @brief Get current sentence unit count
 *
 * @param processor Syntax processor instance
 * @return Number of units in current sentence
 */
uint32_t syntax_get_unit_count(const syntax_processor_t* processor);

/**
 * @brief Get syntactic unit by index
 *
 * WHAT: Retrieve a specific unit from the sentence buffer
 * WHY:  Enable iteration over sentence structure
 * HOW:  Copy unit at specified index to output
 *
 * @param processor Syntax processor instance
 * @param index Unit index (0 to count-1)
 * @param unit Output unit (filled on success)
 * @return true on success, false if index out of bounds
 */
bool syntax_get_unit(const syntax_processor_t* processor,
                     uint32_t index,
                     syntactic_unit_t* unit);

//=============================================================================
// Brain Integration
//=============================================================================

/**
 * @brief Associate brain with syntax processor
 *
 * WHAT: Set brain reference for neuromodulation
 * WHY:  Enable DA/ACh modulation of syntax processing
 * HOW:  Store brain pointer for neurotransmitter reading
 *
 * BIOLOGY:
 * - Dopamine modulates rule learning and probabilistic parsing
 * - Acetylcholine modulates working memory for sentence processing
 * - Broca's area receives strong dopaminergic innervation
 *
 * @param processor Syntax processor instance
 * @param brain Brain instance (or NULL to clear)
 */
void syntax_set_brain(syntax_processor_t* processor, brain_t brain);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get POS name string
 *
 * @param pos Part of speech
 * @return Human-readable POS name
 */
const char* syntax_pos_name(part_of_speech_t pos);

/**
 * @brief Get phrase type name string
 *
 * @param phrase Phrase type
 * @return Human-readable phrase name
 */
const char* syntax_phrase_name(phrase_type_t phrase);

/**
 * @brief Check if POS is content word
 *
 * WHAT: Determine if POS is content vs function word
 * WHY:  Content words carry meaning; function words structure
 * HOW:  Check if POS is noun/verb/adj/adv
 *
 * @param pos Part of speech
 * @return true if content word (noun, verb, adj, adv)
 */
bool syntax_is_content_word(part_of_speech_t pos);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYNTAX_PROCESSOR_H
