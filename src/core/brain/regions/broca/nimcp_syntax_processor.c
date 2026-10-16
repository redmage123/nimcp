/**
 * @file nimcp_syntax_processor.c
 * @brief Implementation of syntax processing module for Broca's region
 *
 * WHAT: Sentence structure generation, grammar rule application, syntactic tree construction
 * WHY:  Enable grammatical language production in NIMCP brain
 * HOW:  Chart parsing + phrase structure rules + morphological processing
 *
 * IMPLEMENTATION NOTES:
 * - Uses CYK (Cocke-Younger-Kasami) chart parsing algorithm
 * - Binary branching tree structure (X-bar theory)
 * - Simple inflectional morphology (English)
 * - Feature checking for agreement
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 * @version 2.7 (Phase 8.8)
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/regions/broca/nimcp_syntax_processor.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "BROCA_SYNTAX"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(syntax_processor, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Chart cell for CYK parsing
 */
typedef struct {
    phrase_type_t phrase_type;
    float probability;
    bool is_filled;
    uint32_t split_point;  // For backtracking
} chart_cell_t;

/**
 * @brief Syntax processor internal state
 */
struct syntax_processor {
    // Configuration
    syntax_config_t config;

    // Current sentence buffer
    syntactic_unit_t* units;
    uint32_t unit_count;

    // Phrase structure rules
    phrase_structure_rule_t* rules;
    uint32_t rule_count;

    // Syntactic tree
    syntax_tree_node_t* tree_root;
    uint32_t tree_depth;

    // Chart for parsing
    chart_cell_t** chart;  // 2D array [N][N]
    uint32_t chart_size;

    // Memory pool for hot-path allocations (Phase 1.5)
    // Pool for syntax tree nodes in build_tree_recursive()
    memory_pool_t tree_node_pool;

    // Statistics
    syntax_stats_t stats;

    // Brain reference (for neuromodulation)
    brain_t brain;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void free_tree_recursive(syntax_processor_t* processor, syntax_tree_node_t* node);
static syntax_tree_node_t* build_tree_from_chart(syntax_processor_t* processor);
static bool check_agreement(const syntactic_unit_t* subject, const syntactic_unit_t* verb);
static void apply_default_features(syntactic_unit_t* unit);

//=============================================================================
// Lifecycle Functions
//=============================================================================

syntax_config_t syntax_default_config(void) {
    syntax_config_t config;

    config.max_units = SYNTAX_MAX_UNITS;
    config.max_tree_depth = SYNTAX_MAX_TREE_DEPTH;
    config.max_rules = SYNTAX_MAX_RULES;
    config.max_morphemes = SYNTAX_MAX_MORPHEMES;
    config.enable_morphology = true;
    config.enable_agreement = true;
    config.enable_movement = false;  // Advanced feature
    config.enable_tree_caching = true;
    config.enable_neuromodulation = false;
    config.learning_rate = 0.01F;

    return config;
}

syntax_processor_t* syntax_create(const syntax_config_t* config) {
    // Use default config if NULL
    syntax_config_t default_cfg = syntax_default_config();
    if (config == NULL) {
        config = &default_cfg;
    }

    // Allocate processor
    syntax_processor_t* processor = (syntax_processor_t*)nimcp_calloc(1, sizeof(syntax_processor_t));
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate syntax processor");
        return NULL;
    }

    // Copy configuration
    processor->config = *config;

    // Allocate unit buffer
    processor->units = (syntactic_unit_t*)nimcp_calloc(config->max_units, sizeof(syntactic_unit_t));
    if (processor->units == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate unit buffer");
        nimcp_free(processor);
        return NULL;
    }
    processor->unit_count = 0;

    // Allocate rule table
    processor->rules = (phrase_structure_rule_t*)nimcp_calloc(config->max_rules, sizeof(phrase_structure_rule_t));
    if (processor->rules == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate rule table");
        nimcp_free(processor->units);
        nimcp_free(processor);
        return NULL;
    }
    processor->rule_count = 0;

    // Allocate chart for parsing (NxN where N = max_units)
    processor->chart_size = config->max_units;
    processor->chart = (chart_cell_t**)nimcp_calloc(config->max_units, sizeof(chart_cell_t*));
    if (processor->chart == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate chart");
        nimcp_free(processor->rules);
        nimcp_free(processor->units);
        nimcp_free(processor);
        return NULL;
    }

    for (uint32_t i = 0; i < config->max_units; i++) {
        processor->chart[i] = (chart_cell_t*)nimcp_calloc(config->max_units, sizeof(chart_cell_t));
        if (processor->chart[i] == NULL) {
            // Cleanup on failure
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(processor->chart[j]);
            }
            nimcp_free(processor->chart);
            nimcp_free(processor->rules);
            nimcp_free(processor->units);
            nimcp_free(processor);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "syntax_create: failed to allocate chart row");
            return NULL;
        }
    }

    // Initialize tree
    processor->tree_root = NULL;
    processor->tree_depth = 0;

    // Initialize memory pool for hot-path allocations (Phase 1.5)
    // Pool for syntax tree nodes - estimate 2*max_units nodes per tree, 4 trees concurrently
    memory_pool_config_t node_pool_config = {
        .block_size = sizeof(syntax_tree_node_t),
        .num_blocks = config->max_units * 8,  // 2 nodes per unit * 4 trees
        .alignment = 16,  // SIMD alignment
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    processor->tree_node_pool = memory_pool_create(&node_pool_config);
    if (!processor->tree_node_pool) {
        syntax_destroy(processor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "syntax_create: processor->tree_node_pool is NULL");
        return NULL;
    }

    // Initialize statistics
    memset(&processor->stats, 0, sizeof(syntax_stats_t));

    // No brain initially
    processor->brain = NULL;

    // Load default phrase structure rules
    syntax_load_default_rules(processor);

    return processor;
}

void syntax_destroy(syntax_processor_t* processor) {
    if (processor == NULL) {
        return;
    }

    // Free tree
    if (processor->tree_root != NULL) {
        free_tree_recursive(processor, processor->tree_root);
    }

    // Free chart
    if (processor->chart != NULL) {
        for (uint32_t i = 0; i < processor->chart_size; i++) {
            if (processor->chart[i] != NULL) {
                nimcp_free(processor->chart[i]);
            }
        }
        nimcp_free(processor->chart);
    }

    // Free rules
    if (processor->rules != NULL) {
        nimcp_free(processor->rules);
    }

    // Free unit buffer
    if (processor->units != NULL) {
        nimcp_free(processor->units);
    }

    // Destroy memory pool (Phase 1.5)
    memory_pool_destroy(processor->tree_node_pool);

    // Free processor
    nimcp_free(processor);
}

//=============================================================================
// Core Operations
//=============================================================================

bool syntax_add_unit(syntax_processor_t* processor, const syntactic_unit_t* unit) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (unit == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unit is NULL");
        return false;
    }

    if (processor->unit_count >= processor->config.max_units) {
        LOG_DEBUG("syntax_processor: unit buffer full (count=%u, max=%u)",
                  processor->unit_count, processor->config.max_units);
        return false;
    }

    // Copy unit
    processor->units[processor->unit_count] = *unit;

    // Apply default features if needed
    apply_default_features(&processor->units[processor->unit_count]);

    processor->unit_count++;
    return true;
}

bool syntax_get_unit(const syntax_processor_t* processor,
                     uint32_t index,
                     syntactic_unit_t* unit) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (unit == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unit output is NULL");
        return false;
    }

    if (index >= processor->unit_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Index out of bounds");
        return false;
    }

    *unit = processor->units[index];
    return true;
}

bool syntax_build_tree(syntax_processor_t* processor) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (processor->unit_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "No units to build tree");
        return false;
    }

    // Clear old tree
    if (processor->tree_root != NULL) {
        free_tree_recursive(processor, processor->tree_root);
        processor->tree_root = NULL;
    }

    // Clear chart
    for (uint32_t i = 0; i < processor->chart_size; i++) {
        for (uint32_t j = 0; j < processor->chart_size; j++) {
            processor->chart[i][j].is_filled = false;
            processor->chart[i][j].probability = 0.0F;
        }
    }

    const uint32_t n = processor->unit_count;

    // CYK Chart Parsing (simplified bottom-up)
    // Fill diagonal with lexical items
    //
    // POS → phrase-type mapping. Previously this switch handled only 6 of
    // the 13 POS classes; ADVERB / CONJUNCTION / COMPLEMENTIZER / AUXILIARY
    // / INTERJECTION / PUNCTUATION / NUMBER / UNKNOWN all fell through to
    // PHRASE_NONE, which silently dropped every sentence containing "and",
    // "is", "very", "that", etc. into stats.failed_parses with no
    // diagnostic. This expanded mapping covers all POS classes; the
    // unfilled-cell branch logs a LOG_DEBUG so training-time failures are
    // attributable to a specific word + position. (Logging is LOG_DEBUG
    // because these are common during training, not warnings.)
    for (uint32_t i = 0; i < n; i++) {
        syntactic_unit_t* unit = &processor->units[i];

        // Map POS to phrase type
        phrase_type_t phrase = PHRASE_NONE;
        bool is_transparent = false;  // skip cell (e.g. punctuation)
        switch (unit->pos) {
            case POS_NOUN:
            case POS_PRONOUN:
            case POS_DETERMINER:  // Treat determiners as NP heads in simplified grammar
                phrase = PHRASE_NP;
                break;
            case POS_VERB:
                phrase = PHRASE_VP;
                break;
            case POS_AUXILIARY:
                // Auxiliaries head verb phrases (is/are/have/will).
                phrase = PHRASE_VP;
                break;
            case POS_ADJECTIVE:
                phrase = PHRASE_AP;
                break;
            case POS_PREPOSITION:
                phrase = PHRASE_PP;
                break;
            case POS_ADVERB:
                phrase = PHRASE_ADVP;
                break;
            case POS_CONJUNCTION:
                phrase = PHRASE_CONJP;
                break;
            case POS_COMPLEMENTIZER:
                phrase = PHRASE_CP;
                break;
            case POS_PARTICLE:
                // Particles ("up", "down", "out") combine with verbs;
                // treat as ADVP for simplified grammar.
                phrase = PHRASE_ADVP;
                break;
            case POS_PUNCTUATION:
            case POS_INTERJECTION:
                // Transparent: don't fill this cell, just advance past it.
                is_transparent = true;
                break;
            case POS_UNKNOWN:
                // Diagnostic: log so training-time parse failures can be
                // attributed to a specific OOV/UNKNOWN word.
                LOG_DEBUG("BROCA_SYNTAX: POS_UNKNOWN at chart[%u][%u] "
                          "(word_id=%u) — chart cell left unfilled",
                          i, i, unit->word_id);
                phrase = PHRASE_NONE;
                break;
            default:
                // Cover POS_COUNT and any future enum additions safely.
                phrase = PHRASE_NONE;
                break;
        }

        processor->chart[i][i].phrase_type = phrase;
        processor->chart[i][i].probability = 1.0F;
        processor->chart[i][i].is_filled = (phrase != PHRASE_NONE);

        if (!processor->chart[i][i].is_filled && !is_transparent) {
            LOG_DEBUG("BROCA_SYNTAX: chart[%u][%u] unfilled "
                      "(pos=%s word_id=%u)",
                      i, i, syntax_pos_name(unit->pos), unit->word_id);
        }
    }

    // Fill chart bottom-up
    for (uint32_t span = 2; span <= n; span++) {
        for (uint32_t i = 0; i <= n - span; i++) {
            uint32_t j = i + span - 1;

            // Try all split points
            for (uint32_t k = i; k < j; k++) {
                // Check if cells are filled
                if (!processor->chart[i][k].is_filled || !processor->chart[k+1][j].is_filled) {
                    continue;
                }

                phrase_type_t left = processor->chart[i][k].phrase_type;
                phrase_type_t right = processor->chart[k+1][j].phrase_type;

                // Try to apply rules
                for (uint32_t r = 0; r < processor->rule_count; r++) {
                    phrase_structure_rule_t* rule = &processor->rules[r];
                    if (!rule->is_active || rule->num_rhs < 2) {
                        continue;
                    }

                    if (rule->rhs[0] == left && rule->rhs[1] == right) {
                        float prob = processor->chart[i][k].probability *
                                   processor->chart[k+1][j].probability *
                                   rule->probability;

                        if (prob > processor->chart[i][j].probability) {
                            processor->chart[i][j].phrase_type = rule->lhs;
                            processor->chart[i][j].probability = prob;
                            processor->chart[i][j].is_filled = true;
                            processor->chart[i][j].split_point = k;
                        }
                    }
                }
            }
        }
    }

    // Check if we have a complete parse (top-level IP/sentence)
    // Must be filled AND must be a sentence (IP) phrase type
    if (!processor->chart[0][n-1].is_filled ||
        processor->chart[0][n-1].phrase_type != PHRASE_IP) {
        processor->stats.failed_parses++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "syntax_build_tree: operation failed");
        return false;
    }

    // Build tree from chart
    processor->tree_root = build_tree_from_chart(processor);
    if (processor->tree_root == NULL) {
        processor->stats.failed_parses++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "syntax_build_tree: validation failed");
        return false;
    }

    processor->stats.successful_parses++;
    processor->stats.sentences_processed++;

    /* W12 KG emit: one node per successful parse tree. Fires at
     * utterance-level granularity. syntax has its own brain ptr via
     * `syntax_set_brain`, but we use the registered-brain path for
     * consistency with the rest of the language stack. */
    w12_emit_syntax_parse_auto(processor->unit_count,
                               processor->tree_depth, true);

    return true;
}

bool syntax_validate_grammar(syntax_processor_t* processor, bool* is_valid) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (is_valid == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_valid output is NULL");
        return false;
    }

    *is_valid = true;

    // Must have at least one unit
    if (processor->unit_count == 0) {
        *is_valid = false;
        return true;
    }

    // Check subject-verb agreement if enabled
    if (processor->config.enable_agreement) {
        // Simple check: find first noun and first verb
        syntactic_unit_t* subject = NULL;
        syntactic_unit_t* verb = NULL;

        for (uint32_t i = 0; i < processor->unit_count; i++) {
            if (processor->units[i].pos == POS_NOUN || processor->units[i].pos == POS_PRONOUN) {
                if (subject == NULL) {
                    subject = &processor->units[i];
                }
            }
            if (processor->units[i].pos == POS_VERB) {
                if (verb == NULL) {
                    verb = &processor->units[i];
                }
            }
        }

        if (subject != NULL && verb != NULL) {
            if (!check_agreement(subject, verb)) {
                *is_valid = false;
                processor->stats.agreement_violations++;
            }
        }
    }

    return true;
}

bool syntax_reset(syntax_processor_t* processor) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "syntax_reset: validation failed");
        return false;
    }

    // Clear unit buffer
    processor->unit_count = 0;

    // Free tree
    if (processor->tree_root != NULL) {
        free_tree_recursive(processor, processor->tree_root);
        processor->tree_root = NULL;
        processor->tree_depth = 0;
    }

    return true;
}

//=============================================================================
// Tree Operations
//=============================================================================

const syntax_tree_node_t* syntax_get_tree_root(const syntax_processor_t* processor) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "syntax_get_tree_root: validation failed");
        return NULL;
    }
    return processor->tree_root;
}

uint32_t syntax_get_tree_depth(const syntax_processor_t* processor) {
    if (processor == NULL) {
        return 0;
    }
    return processor->tree_depth;
}

static void print_tree_recursive(const syntax_tree_node_t* node, uint32_t depth) {
    if (node == NULL) {
        return;
    }

    // Indent
    for (uint32_t i = 0; i < depth; i++) {
        printf("  ");
    }

    // Print node info
    printf("[%s", syntax_phrase_name(node->unit.phrase_type));
    if (node->is_head) {
        printf(" (head)");
    }
    printf("]\n");

    // Recurse
    print_tree_recursive(node->left, depth + 1);
    print_tree_recursive(node->right, depth + 1);
}

bool syntax_print_tree(const syntax_processor_t* processor) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "syntax_print_tree: validation failed");
        return false;
    }

    if (processor->tree_root == NULL) {
        printf("(No tree built)\n");
        return true;
    }

    printf("Syntactic Tree:\n");
    print_tree_recursive(processor->tree_root, 0);
    return true;
}

//=============================================================================
// Morphological Processing
//=============================================================================

bool syntax_apply_inflection(
    syntax_processor_t* processor,
    const syntactic_unit_t* unit,
    char* inflected_form,
    uint32_t buffer_size
) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (unit == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unit is NULL");
        return false;
    }
    if (inflected_form == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "inflected_form buffer is NULL");
        return false;
    }

    if (!processor->config.enable_morphology) {
        snprintf(inflected_form, buffer_size, "word_%u", unit->word_id);
        return true;
    }

    // Simple English inflection (demonstration)
    char base[NIMCP_ID_BUFFER_SIZE];
    snprintf(base, sizeof(base), "word_%u", unit->word_id);

    // Apply inflection based on POS and features
    if (unit->pos == POS_NOUN) {
        if (unit->features.number == 2) {  // Plural
            snprintf(inflected_form, buffer_size, "%s-s", base);
        } else {
            snprintf(inflected_form, buffer_size, "%s", base);
        }
    } else if (unit->pos == POS_VERB) {
        if (unit->features.tense == 2) {  // Past
            snprintf(inflected_form, buffer_size, "%s-ed", base);
        } else if (unit->features.person == 3 && unit->features.number == 1) {  // 3rd singular
            snprintf(inflected_form, buffer_size, "%s-s", base);
        } else {
            snprintf(inflected_form, buffer_size, "%s", base);
        }
    } else {
        snprintf(inflected_form, buffer_size, "%s", base);
    }

    processor->stats.morphological_ops++;
    return true;
}

bool syntax_decompose_morphemes(
    syntax_processor_t* processor,
    const char* word,
    char morphemes[][32],
    uint32_t max_morphemes,
    uint32_t* num_morphemes
) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (word == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "word is NULL");
        return false;
    }
    if (morphemes == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "morphemes buffer is NULL");
        return false;
    }
    if (num_morphemes == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "num_morphemes output is NULL");
        return false;
    }

    *num_morphemes = 0;

    if (!processor->config.enable_morphology || max_morphemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "syntax_decompose_morphemes: processor->config is NULL");
        return false;
    }

    // Simple morpheme decomposition (demonstration)
    // In production, use proper morphological analyzer

    size_t len = strlen(word);
    if (len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "syntax_decompose_morphemes: len is zero");
        return false;
    }

    // Check for common suffixes
    // Format: {suffix_without_hyphen, suffix_with_hyphen_for_output}
    const char* suffix_patterns[] = {"ed", "ing", "es", "s", "er", "est", "ly", "ness"};
    const char* suffix_outputs[] = {"-ed", "-ing", "-es", "-s", "-er", "-est", "-ly", "-ness"};
    const size_t suffix_count = sizeof(suffix_patterns) / sizeof(suffix_patterns[0]);

    bool found_suffix = false;
    for (size_t i = 0; i < suffix_count; i++) {
        size_t suffix_len = strlen(suffix_patterns[i]);
        if (len > suffix_len && strcmp(word + len - suffix_len, suffix_patterns[i]) == 0) {
            // Root
            size_t root_len = len - suffix_len;
            if (root_len < 32) {
                strncpy(morphemes[0], word, root_len);
                morphemes[0][root_len] = '\0';
                // Suffix (with hyphen for output)
                strncpy(morphemes[1], suffix_outputs[i], 32);
                morphemes[1][31] = '\0';
                *num_morphemes = 2;
                found_suffix = true;
                break;
            }
        }
    }

    if (!found_suffix) {
        // No decomposition, return whole word
        strncpy(morphemes[0], word, 32);
        *num_morphemes = 1;
    }

    processor->stats.morphological_ops++;
    return true;
}

//=============================================================================
// Grammar Rule Management
//=============================================================================

bool syntax_add_rule(syntax_processor_t* processor, const phrase_structure_rule_t* rule) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (rule == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rule is NULL");
        return false;
    }

    if (processor->rule_count >= processor->config.max_rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Rule table full");
        return false;
    }

    processor->rules[processor->rule_count] = *rule;
    processor->rule_count++;
    return true;
}

uint32_t syntax_get_rule_count(const syntax_processor_t* processor) {
    if (processor == NULL) {
        return 0;
    }
    return processor->rule_count;
}

bool syntax_load_default_rules(syntax_processor_t* processor) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "syntax_load_default_rules: validation failed");
        return false;
    }

    processor->rule_count = 0;

    /*=========================================================================
     * Phrase Structure Rules for English
     *
     * Based on X-bar theory with simplified binary branching.
     * Rules are ordered by probability for efficient parsing.
     *
     * Notation: LHS → RHS1 RHS2
     * - IP = Inflectional Phrase (sentence)
     * - NP = Noun Phrase
     * - VP = Verb Phrase
     * - AP = Adjective Phrase
     * - PP = Prepositional Phrase
     * - DP = Determiner Phrase
     *=======================================================================*/

    phrase_structure_rule_t rule;
    memset(&rule, 0, sizeof(rule));

    /* === SENTENCE-LEVEL RULES === */

    /* S → NP VP (basic declarative sentence) */
    rule.lhs = PHRASE_IP;
    rule.rhs[0] = PHRASE_NP;
    rule.rhs[1] = PHRASE_VP;
    rule.num_rhs = 2;
    rule.probability = 0.9F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* S → DP VP (sentence with determiner phrase subject) */
    rule.lhs = PHRASE_IP;
    rule.rhs[0] = PHRASE_DP;
    rule.rhs[1] = PHRASE_VP;
    rule.num_rhs = 2;
    rule.probability = 0.85F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === VERB PHRASE RULES === */

    /* VP → VP NP (transitive verb: "ate [the apple]") */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_VP;
    rule.rhs[1] = PHRASE_NP;
    rule.num_rhs = 2;
    rule.probability = 0.7F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* VP → VP DP (transitive verb with DP object) */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_VP;
    rule.rhs[1] = PHRASE_DP;
    rule.num_rhs = 2;
    rule.probability = 0.65F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* VP → VP PP (verb with prepositional phrase: "ran [to the store]") */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_VP;
    rule.rhs[1] = PHRASE_PP;
    rule.num_rhs = 2;
    rule.probability = 0.5F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* VP → VP AP (verb with adjective complement: "became [happy]") */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_VP;
    rule.rhs[1] = PHRASE_AP;
    rule.num_rhs = 2;
    rule.probability = 0.3F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* VP → VP ADVP (verb with adverb: "ran [quickly]") */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_VP;
    rule.rhs[1] = PHRASE_ADVP;
    rule.num_rhs = 2;
    rule.probability = 0.4F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === NOUN PHRASE RULES === */

    /* NP → DP NP (determiner phrase + noun: "[the] cat") */
    rule.lhs = PHRASE_NP;
    rule.rhs[0] = PHRASE_DP;
    rule.rhs[1] = PHRASE_NP;
    rule.num_rhs = 2;
    rule.probability = 0.7F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* NP → AP NP (adjective + noun phrase: "[big] cat") */
    rule.lhs = PHRASE_NP;
    rule.rhs[0] = PHRASE_AP;
    rule.rhs[1] = PHRASE_NP;
    rule.num_rhs = 2;
    rule.probability = 0.5F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* NP → NP PP (noun with prepositional modifier: "cat [on the mat]") */
    rule.lhs = PHRASE_NP;
    rule.rhs[0] = PHRASE_NP;
    rule.rhs[1] = PHRASE_PP;
    rule.num_rhs = 2;
    rule.probability = 0.4F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* NP → NP NP (compound noun: "[cat] food") */
    rule.lhs = PHRASE_NP;
    rule.rhs[0] = PHRASE_NP;
    rule.rhs[1] = PHRASE_NP;
    rule.num_rhs = 2;
    rule.probability = 0.3F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === DETERMINER PHRASE RULES === */

    /* DP → DP NP (full determiner phrase: "[the big] cat") */
    rule.lhs = PHRASE_DP;
    rule.rhs[0] = PHRASE_DP;
    rule.rhs[1] = PHRASE_NP;
    rule.num_rhs = 2;
    rule.probability = 0.6F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === PREPOSITIONAL PHRASE RULES === */

    /* PP → PP NP (preposition + noun phrase: "[in] the box") */
    rule.lhs = PHRASE_PP;
    rule.rhs[0] = PHRASE_PP;
    rule.rhs[1] = PHRASE_NP;
    rule.num_rhs = 2;
    rule.probability = 0.8F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* PP → PP DP (preposition + determiner phrase) */
    rule.lhs = PHRASE_PP;
    rule.rhs[0] = PHRASE_PP;
    rule.rhs[1] = PHRASE_DP;
    rule.num_rhs = 2;
    rule.probability = 0.7F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === ADJECTIVE PHRASE RULES === */

    /* AP → ADVP AP (adverb + adjective: "[very] tall") */
    rule.lhs = PHRASE_AP;
    rule.rhs[0] = PHRASE_ADVP;
    rule.rhs[1] = PHRASE_AP;
    rule.num_rhs = 2;
    rule.probability = 0.5F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* AP → AP PP (adjective with PP complement: "proud [of you]") */
    rule.lhs = PHRASE_AP;
    rule.rhs[0] = PHRASE_AP;
    rule.rhs[1] = PHRASE_PP;
    rule.num_rhs = 2;
    rule.probability = 0.3F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === ADVERB PHRASE RULES === */

    /* ADVP → ADVP ADVP (adverb + adverb: "[very] quickly") */
    rule.lhs = PHRASE_ADVP;
    rule.rhs[0] = PHRASE_ADVP;
    rule.rhs[1] = PHRASE_ADVP;
    rule.num_rhs = 2;
    rule.probability = 0.4F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* VP → ADVP VP (sentence-initial adverb: "quickly [ran home]") */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_ADVP;
    rule.rhs[1] = PHRASE_VP;
    rule.num_rhs = 2;
    rule.probability = 0.3F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === CONJUNCTION RULES ===
     *
     * "the cat and the dog ran" — handled in two stages:
     *   1.  CONJP + NP → NP  (collapse "and the dog" into an NP)
     *   2.  NP + NP   → NP   (compound NP rule already loaded above)
     * Same pattern lifts S + Conj + S into S via the IP rule below.
     *
     * Scope note: full coordinate-structure grammar is out of scope;
     * we only cover the common Conj+X attachment case so simple
     * conjoined NPs / sentences parse. Three-way coordination
     * ("X, Y, and Z") still fails — needs comma handling + ternary
     * rules. */

    /* NP → CONJP NP ("and the dog") */
    rule.lhs = PHRASE_NP;
    rule.rhs[0] = PHRASE_CONJP;
    rule.rhs[1] = PHRASE_NP;
    rule.num_rhs = 2;
    rule.probability = 0.5F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* VP → CONJP VP ("and ran") */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_CONJP;
    rule.rhs[1] = PHRASE_VP;
    rule.num_rhs = 2;
    rule.probability = 0.4F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* IP → IP CONJP (clause-final conjunction "S, and") — rare but legal */
    rule.lhs = PHRASE_IP;
    rule.rhs[0] = PHRASE_IP;
    rule.rhs[1] = PHRASE_CONJP;
    rule.num_rhs = 2;
    rule.probability = 0.2F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* IP → IP IP (sentence coordination via Conj+IP collapse: "X and Y") */
    rule.lhs = PHRASE_IP;
    rule.rhs[0] = PHRASE_IP;
    rule.rhs[1] = PHRASE_IP;
    rule.num_rhs = 2;
    rule.probability = 0.25F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === COMPLEMENTIZER PHRASE RULES === */

    /* CP → CP IP ("that [she left]" — embedded clause) */
    rule.lhs = PHRASE_CP;
    rule.rhs[0] = PHRASE_CP;
    rule.rhs[1] = PHRASE_IP;
    rule.num_rhs = 2;
    rule.probability = 0.7F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* VP → VP CP (verb selecting clausal complement: "said [that she left]") */
    rule.lhs = PHRASE_VP;
    rule.rhs[0] = PHRASE_VP;
    rule.rhs[1] = PHRASE_CP;
    rule.num_rhs = 2;
    rule.probability = 0.3F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    /* === YES/NO QUESTIONS (auxiliary inversion) ===
     *
     * "is the cat hungry" → AUX + NP + AP, which we collapse as:
     *   VP + NP → VP  (AUX-as-VP + subject NP)  -- existing transitive rule
     *   VP + AP → VP  -- existing complement rule
     *   IP → NP VP   -- but subject is INSIDE the VP, so we need IP → VP
     * Add IP → VP to permit the question to top out at IP. */
    rule.lhs = PHRASE_IP;
    rule.rhs[0] = PHRASE_VP;
    rule.rhs[1] = PHRASE_VP;
    rule.num_rhs = 2;
    rule.probability = 0.2F;
    rule.is_active = true;
    syntax_add_rule(processor, &rule);

    return true;
}

//=============================================================================
// Statistics and Introspection
//=============================================================================

bool syntax_get_stats(const syntax_processor_t* processor, syntax_stats_t* stats) {
    if (processor == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats output is NULL");
        return false;
    }

    *stats = processor->stats;

    // Calculate averages
    if (processor->stats.sentences_processed > 0) {
        stats->avg_tree_depth = (float)processor->tree_depth;
        stats->avg_sentence_length = (float)processor->unit_count;
    }

    return true;
}

uint32_t syntax_get_unit_count(const syntax_processor_t* processor) {
    if (processor == NULL) {
        return 0;
    }
    return processor->unit_count;
}

//=============================================================================
// Brain Integration
//=============================================================================

void syntax_set_brain(syntax_processor_t* processor, brain_t brain) {
    if (processor == NULL) {
        return;
    }
    processor->brain = brain;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* syntax_pos_name(part_of_speech_t pos) {
    switch (pos) {
        case POS_NOUN: return "NOUN";
        case POS_VERB: return "VERB";
        case POS_ADJECTIVE: return "ADJ";
        case POS_ADVERB: return "ADV";
        case POS_PRONOUN: return "PRON";
        case POS_DETERMINER: return "DET";
        case POS_PREPOSITION: return "PREP";
        case POS_CONJUNCTION: return "CONJ";
        case POS_COMPLEMENTIZER: return "COMP";
        case POS_AUXILIARY: return "AUX";
        case POS_PARTICLE: return "PART";
        case POS_INTERJECTION: return "INTERJ";
        case POS_PUNCTUATION: return "PUNCT";
        case POS_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

const char* syntax_phrase_name(phrase_type_t phrase) {
    switch (phrase) {
        case PHRASE_NONE: return "NONE";
        case PHRASE_NP: return "NP";
        case PHRASE_VP: return "VP";
        case PHRASE_AP: return "AP";
        case PHRASE_PP: return "PP";
        case PHRASE_ADVP: return "ADVP";
        case PHRASE_CP: return "CP";
        case PHRASE_IP: return "IP";
        case PHRASE_DP: return "DP";
        case PHRASE_CONJP: return "CONJP";
        default: return "INVALID";
    }
}

bool syntax_is_content_word(part_of_speech_t pos) {
    return (pos == POS_NOUN || pos == POS_VERB ||
            pos == POS_ADJECTIVE || pos == POS_ADVERB);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static void free_tree_recursive(syntax_processor_t* processor, syntax_tree_node_t* node) {
    if (node == NULL) {
        return;
    }

    free_tree_recursive(processor, node->left);
    free_tree_recursive(processor, node->right);

    /* Release back to pool (Phase 1.5) */
    memory_pool_release(processor->tree_node_pool, node);
}

/**
 * @brief Recursively build tree node from chart cell
 *
 * WHAT: Create tree structure from CYK chart using split points
 * WHY:  Reconstruct full parse tree from bottom-up chart parsing
 * HOW:  Recursively split at stored split points, create left/right children
 *
 * @param processor Syntax processor with filled chart
 * @param i Start index in sentence
 * @param j End index in sentence
 * @param depth Current tree depth
 * @param parent Parent node (NULL for root)
 * @return Newly created tree node or NULL on failure
 */
static syntax_tree_node_t* build_tree_recursive(
    syntax_processor_t* processor,
    uint32_t i,
    uint32_t j,
    uint32_t depth,
    syntax_tree_node_t* parent
) {
    if (!processor || i > j || j >= processor->unit_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "build_tree_recursive: processor is NULL");
        return NULL;
    }

    /* Check if this cell was filled during parsing */
    if (!processor->chart[i][j].is_filled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_tree_recursive: processor->chart is NULL");
        return NULL;
    }

    /* Allocate new node - Phase 1.5 O(1) pool allocation */
    syntax_tree_node_t* node = (syntax_tree_node_t*)memory_pool_acquire(processor->tree_node_pool);
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_tree_recursive: failed to acquire tree node from pool");

        return NULL;
    }
    memset(node, 0, sizeof(syntax_tree_node_t));

    /* Set node properties */
    node->unit.phrase_type = processor->chart[i][j].phrase_type;
    node->depth = depth;
    node->parent = parent;
    node->left = NULL;
    node->right = NULL;

    /* Track maximum depth */
    if (depth + 1 > processor->tree_depth) {
        processor->tree_depth = depth + 1;
    }

    /* Base case: single word (lexical item) */
    if (i == j) {
        /* Copy the syntactic unit from the sentence buffer */
        node->unit = processor->units[i];
        node->unit.phrase_type = processor->chart[i][j].phrase_type;
        node->is_head = true;  /* Lexical items are heads */
        return node;
    }

    /* Recursive case: use split point to build children */
    uint32_t k = processor->chart[i][j].split_point;

    /* Build left child (i to k) */
    node->left = build_tree_recursive(processor, i, k, depth + 1, node);
    if (!node->left) {
        /* Failed to build left child - cleanup and return NULL */
        memory_pool_release(processor->tree_node_pool, node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_tree_recursive: node->left is NULL");
        return NULL;
    }

    /* Build right child (k+1 to j) */
    node->right = build_tree_recursive(processor, k + 1, j, depth + 1, node);
    if (!node->right) {
        /* Failed to build right child - cleanup left child and node */
        free_tree_recursive(processor, node->left);
        memory_pool_release(processor->tree_node_pool, node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_tree_recursive: node->right is NULL");
        return NULL;
    }

    /* Determine head: left child is typically the head in X-bar theory */
    node->is_head = false;  /* Non-terminal nodes are not heads themselves */
    if (node->left) {
        node->left->is_head = true;
    }

    return node;
}

static syntax_tree_node_t* build_tree_from_chart(syntax_processor_t* processor) {
    if (processor == NULL || processor->unit_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_tree_from_chart: processor->unit_count is zero");
        return NULL;
    }

    uint32_t n = processor->unit_count;

    /* Verify we have a valid parse */
    if (!processor->chart[0][n-1].is_filled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_tree_from_chart: processor->chart is NULL");
        return NULL;
    }

    /* Initialize tree depth tracking */
    processor->tree_depth = 0;

    /* Build tree recursively from root cell chart[0][n-1] */
    syntax_tree_node_t* root = build_tree_recursive(processor, 0, n - 1, 0, NULL);

    return root;
}

static bool check_agreement(const syntactic_unit_t* subject, const syntactic_unit_t* verb) {
    if (subject == NULL || verb == NULL) {
        return false;
    }

    // Check number agreement
    if (subject->features.number != verb->features.number &&
        subject->features.number != 0 && verb->features.number != 0) {
        return false;
    }

    // Check person agreement
    if (subject->features.person != verb->features.person &&
        subject->features.person != 0 && verb->features.person != 0) {
        return false;
    }

    return true;
}

static void apply_default_features(syntactic_unit_t* unit) {
    if (unit == NULL) {
        return;
    }

    // Set default values if not specified
    if (unit->features.number == 0) {
        unit->features.number = 1;  // Default singular
    }
    if (unit->features.person == 0) {
        unit->features.person = 3;  // Default 3rd person
    }
    if (unit->features.tense == 0) {
        unit->features.tense = 1;   // Default present
    }

    // Set has_agreement based on POS
    if (unit->pos == POS_NOUN || unit->pos == POS_PRONOUN || unit->pos == POS_VERB) {
        unit->features.has_agreement = true;
    } else {
        unit->features.has_agreement = false;
    }
}
