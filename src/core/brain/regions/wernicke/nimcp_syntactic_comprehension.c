/**
 * @file nimcp_syntactic_comprehension.c
 * @brief Syntactic Comprehension Layer Implementation
 *
 * Implements incremental sentence parsing with:
 * - Left-corner parsing strategy
 * - Predictive phrase structure building
 * - Thematic role assignment
 * - Garden-path detection and reanalysis
 *
 * @version Phase W5: Advanced Features
 * @date 2026-01-05
 */

#include "core/brain/regions/wernicke/nimcp_syntactic_comprehension.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(syntactic_comprehension, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL CONSTANTS
 *=============================================================================*/

#define DEFAULT_MAX_SENTENCE_LENGTH    32
#define DEFAULT_BEAM_WIDTH             4
#define DEFAULT_PRUNING_THRESHOLD      0.01f
#define DEFAULT_PREDICTION_WEIGHT      0.3f
#define DEFAULT_SEMANTIC_WEIGHT        0.2f
#define DEFAULT_MAX_REANALYSES         3

#define MAX_STACK_DEPTH                32
#define MAX_CHILDREN_PER_NODE          8

/*=============================================================================
 * INTERNAL STRUCTURES
 *=============================================================================*/

/**
 * @brief Grammar rule (simplified CFG)
 */
typedef struct {
    phrase_type_t lhs;               /**< Left-hand side */
    phrase_type_t rhs[4];            /**< Right-hand side */
    uint32_t rhs_len;                /**< RHS length */
    float probability;               /**< Rule probability */
} grammar_rule_t;

/**
 * @brief Verb argument frame
 */
typedef struct {
    char verb_lemma[32];             /**< Verb lemma */
    thematic_role_t roles[6];        /**< Expected roles */
    uint32_t num_roles;              /**< Number of roles */
    bool roles_optional[6];          /**< Which roles are optional */
} argument_frame_t;

/**
 * @brief Syntactic comprehension context
 */
struct syntactic_comprehension {
    syntactic_config_t config;

    /* Incremental parsing state */
    syntactic_word_t word_buffer[SYNTACTIC_MAX_SENTENCE_LEN];
    uint32_t buffer_len;

    syntactic_node_t* parse_stack[MAX_STACK_DEPTH];
    uint32_t stack_depth;

    /* Node pool for parse tree */
    syntactic_node_t node_pool[SYNTACTIC_MAX_CONSTITUENTS];
    uint32_t nodes_allocated;

    /* Current parse state */
    parse_state_t current_state;
    float current_probability;
    uint32_t reanalysis_count;

    /* Category predictions */
    float category_probs[SYN_CAT_COUNT];

    /* Grammar rules (simplified) */
    grammar_rule_t* rules;
    uint32_t num_rules;

    /* Verb argument frames */
    argument_frame_t* arg_frames;
    uint32_t num_frames;

    /* Reduction tracking (parser-coverage expansion 2026-05-11).
     * Each reduction fired during the current parse OR-s a
     * WERNICKE_REDUCTION_* bit into this mask; it is propagated onto
     * syntactic_parse_t.reductions_applied at finish_incremental(). */
    uint32_t reductions_applied;
    bool current_is_question;
    bool current_is_imperative;

    /* Statistics */
    syntactic_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *=============================================================================*/

/**
 * @brief Initialize simplified grammar rules
 */
static void init_grammar_rules(syntactic_comprehension_t* ctx) {
    /* Allocate basic rules */
    ctx->num_rules = 10;
    ctx->rules = nimcp_calloc(ctx->num_rules, sizeof(grammar_rule_t));
    if (!ctx->rules) {
        ctx->num_rules = 0;
        return;
    }

    /* S -> NP VP */
    ctx->rules[0] = (grammar_rule_t){
        .lhs = PHRASE_S,
        .rhs = {PHRASE_NP, PHRASE_VP},
        .rhs_len = 2,
        .probability = 0.8f
    };

    /* VP -> V NP */
    ctx->rules[1] = (grammar_rule_t){
        .lhs = PHRASE_VP,
        .rhs = {PHRASE_VP, PHRASE_NP},  /* V represented as VP leaf */
        .rhs_len = 2,
        .probability = 0.5f
    };

    /* VP -> V NP PP */
    ctx->rules[2] = (grammar_rule_t){
        .lhs = PHRASE_VP,
        .rhs = {PHRASE_VP, PHRASE_NP, PHRASE_PP},
        .rhs_len = 3,
        .probability = 0.3f
    };

    /* NP -> Det N */
    ctx->rules[3] = (grammar_rule_t){
        .lhs = PHRASE_NP,
        .rhs = {PHRASE_DP, PHRASE_NP},
        .rhs_len = 2,
        .probability = 0.6f
    };

    /* NP -> Det Adj N */
    ctx->rules[4] = (grammar_rule_t){
        .lhs = PHRASE_NP,
        .rhs = {PHRASE_DP, PHRASE_AP, PHRASE_NP},
        .rhs_len = 3,
        .probability = 0.3f
    };

    /* PP -> P NP */
    ctx->rules[5] = (grammar_rule_t){
        .lhs = PHRASE_PP,
        .rhs = {PHRASE_PP, PHRASE_NP},
        .rhs_len = 2,
        .probability = 0.9f
    };

    /* NP -> NP PP (attachment) */
    ctx->rules[6] = (grammar_rule_t){
        .lhs = PHRASE_NP,
        .rhs = {PHRASE_NP, PHRASE_PP},
        .rhs_len = 2,
        .probability = 0.4f
    };

    /* VP -> VP PP (attachment) */
    ctx->rules[7] = (grammar_rule_t){
        .lhs = PHRASE_VP,
        .rhs = {PHRASE_VP, PHRASE_PP},
        .rhs_len = 2,
        .probability = 0.4f
    };

    /* SBAR -> Comp S */
    ctx->rules[8] = (grammar_rule_t){
        .lhs = PHRASE_SBAR,
        .rhs = {PHRASE_CP, PHRASE_S},
        .rhs_len = 2,
        .probability = 0.7f
    };

    /* S -> NP VP SBAR */
    ctx->rules[9] = (grammar_rule_t){
        .lhs = PHRASE_S,
        .rhs = {PHRASE_NP, PHRASE_VP, PHRASE_SBAR},
        .rhs_len = 3,
        .probability = 0.2f
    };
}

/**
 * @brief Initialize verb argument frames
 */
static void init_argument_frames(syntactic_comprehension_t* ctx) {
    ctx->num_frames = 5;
    ctx->arg_frames = nimcp_calloc(ctx->num_frames, sizeof(argument_frame_t));
    if (!ctx->arg_frames) {
        ctx->num_frames = 0;
        return;
    }

    /* Transitive verb: Agent V Patient */
    strncpy(ctx->arg_frames[0].verb_lemma, "default_transitive", sizeof(ctx->arg_frames[0].verb_lemma) - 1);
    ctx->arg_frames[0].verb_lemma[sizeof(ctx->arg_frames[0].verb_lemma) - 1] = '\0';
    ctx->arg_frames[0].roles[0] = ROLE_AGENT;
    ctx->arg_frames[0].roles[1] = ROLE_PATIENT;
    ctx->arg_frames[0].num_roles = 2;
    ctx->arg_frames[0].roles_optional[0] = false;
    ctx->arg_frames[0].roles_optional[1] = false;

    /* Ditransitive: Agent V Theme Goal */
    strncpy(ctx->arg_frames[1].verb_lemma, "give", sizeof(ctx->arg_frames[1].verb_lemma) - 1);
    ctx->arg_frames[1].verb_lemma[sizeof(ctx->arg_frames[1].verb_lemma) - 1] = '\0';
    ctx->arg_frames[1].roles[0] = ROLE_AGENT;
    ctx->arg_frames[1].roles[1] = ROLE_THEME;
    ctx->arg_frames[1].roles[2] = ROLE_GOAL;
    ctx->arg_frames[1].num_roles = 3;

    /* Motion verb: Agent V Goal */
    strncpy(ctx->arg_frames[2].verb_lemma, "go", sizeof(ctx->arg_frames[2].verb_lemma) - 1);
    ctx->arg_frames[2].verb_lemma[sizeof(ctx->arg_frames[2].verb_lemma) - 1] = '\0';
    ctx->arg_frames[2].roles[0] = ROLE_AGENT;
    ctx->arg_frames[2].roles[1] = ROLE_GOAL;
    ctx->arg_frames[2].num_roles = 2;

    /* Experiencer verb: Experiencer V Theme */
    strncpy(ctx->arg_frames[3].verb_lemma, "see", sizeof(ctx->arg_frames[3].verb_lemma) - 1);
    ctx->arg_frames[3].verb_lemma[sizeof(ctx->arg_frames[3].verb_lemma) - 1] = '\0';
    ctx->arg_frames[3].roles[0] = ROLE_EXPERIENCER;
    ctx->arg_frames[3].roles[1] = ROLE_THEME;
    ctx->arg_frames[3].num_roles = 2;

    /* Intransitive: Agent V */
    strncpy(ctx->arg_frames[4].verb_lemma, "sleep", sizeof(ctx->arg_frames[4].verb_lemma) - 1);
    ctx->arg_frames[4].verb_lemma[sizeof(ctx->arg_frames[4].verb_lemma) - 1] = '\0';
    ctx->arg_frames[4].roles[0] = ROLE_AGENT;
    ctx->arg_frames[4].num_roles = 1;
}

/**
 * @brief Allocate node from pool
 */
static syntactic_node_t* allocate_node(syntactic_comprehension_t* ctx) {
    if (ctx->nodes_allocated >= SYNTACTIC_MAX_CONSTITUENTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_node: capacity exceeded");
        return NULL;
    }
    syntactic_node_t* node = &ctx->node_pool[ctx->nodes_allocated++];
    memset(node, 0, sizeof(syntactic_node_t));
    return node;
}

/**
 * @brief Reset node pool
 */
static void reset_node_pool(syntactic_comprehension_t* ctx) {
    ctx->nodes_allocated = 0;
    memset(ctx->node_pool, 0, sizeof(ctx->node_pool));
}

/**
 * @brief Map POS category to phrase type
 *
 * Coverage expansion 2026-05-11: SYN_CAT_CONJ and SYN_CAT_PUNCT used to
 * silently fall through to PHRASE_UNKNOWN, dropping any sentence with
 * "and"/"or"/"but"/"." into a parse failure. SYN_CAT_NEG is also handled
 * (treated as ADVP — "not" attaches as an adverbial modifier).
 */
static phrase_type_t category_to_phrase(syntactic_category_t cat) {
    switch (cat) {
        case SYN_CAT_NOUN:
        case SYN_CAT_PRON:
            return PHRASE_NP;
        case SYN_CAT_VERB:
        case SYN_CAT_AUX:
            return PHRASE_VP;
        case SYN_CAT_ADJ:
            return PHRASE_AP;
        case SYN_CAT_ADV:
        case SYN_CAT_NEG:
            /* Negation attaches as an adverbial in this simplified grammar */
            return PHRASE_ADVP;
        case SYN_CAT_PREP:
            return PHRASE_PP;
        case SYN_CAT_DET:
            return PHRASE_DP;
        case SYN_CAT_COMP:
            return PHRASE_CP;
        case SYN_CAT_CONJ:
            /* Conjunctions get a dedicated phrase type via CP-like slot.
             * Wernicke's PHRASE enum has no PHRASE_CONJP, so we re-use
             * PHRASE_CP — both are "head + clausal complement"
             * conjunctive elements for reduction purposes. */
            return PHRASE_CP;
        case SYN_CAT_PUNCT:
            /* Treat punctuation as transparent: caller sees PHRASE_UNKNOWN
             * and skips the stack push. */
            return PHRASE_UNKNOWN;
        default:
            return PHRASE_UNKNOWN;
    }
}

/**
 * @brief Detect wh-word from the word string (simple lookup).
 *
 * The Wernicke header has no SYN_CAT_WH; wh-words are typically tagged
 * as DET or PRON. This helper sniffs the string to flag wh-questions
 * during sentence-initial position checks.
 */
static bool is_wh_word(const char* w) {
    if (!w) return false;
    /* Match common English wh-words case-insensitively on the first
     * two letters; sufficient for who/what/when/where/why/which/how. */
    return ((w[0] == 'w' || w[0] == 'W') &&
            (w[1] == 'h' || w[1] == 'H')) ||
           ((w[0] == 'h' || w[0] == 'H') &&
            (w[1] == 'o' || w[1] == 'O') &&
            (w[2] == 'w' || w[2] == 'W'));
}

/**
 * @brief Compute phrase probability based on rules
 */
static float compute_phrase_probability(syntactic_comprehension_t* ctx,
                                         phrase_type_t type,
                                         const phrase_type_t* children,
                                         uint32_t num_children) {
    for (uint32_t i = 0; i < ctx->num_rules; i++) {
        if (ctx->rules[i].lhs == type &&
            ctx->rules[i].rhs_len == num_children) {
            bool match = true;
            for (uint32_t j = 0; j < num_children; j++) {
                if (ctx->rules[i].rhs[j] != children[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return ctx->rules[i].probability;
            }
        }
    }
    return 0.1f;  /* Default low probability */
}

/**
 * @brief Update category predictions based on context
 */
static void update_predictions(syntactic_comprehension_t* ctx) {
    /* Simple bigram-like prediction */
    memset(ctx->category_probs, 0, sizeof(ctx->category_probs));

    if (ctx->buffer_len == 0) {
        /* Start of sentence - expect Det or Noun */
        ctx->category_probs[SYN_CAT_DET] = 0.4f;
        ctx->category_probs[SYN_CAT_NOUN] = 0.3f;
        ctx->category_probs[SYN_CAT_PRON] = 0.2f;
        ctx->category_probs[SYN_CAT_ADJ] = 0.1f;
    } else {
        syntactic_category_t last = ctx->word_buffer[ctx->buffer_len - 1].category;

        switch (last) {
            case SYN_CAT_DET:
                ctx->category_probs[SYN_CAT_NOUN] = 0.5f;
                ctx->category_probs[SYN_CAT_ADJ] = 0.4f;
                ctx->category_probs[SYN_CAT_ADV] = 0.1f;
                break;
            case SYN_CAT_ADJ:
                ctx->category_probs[SYN_CAT_NOUN] = 0.6f;
                ctx->category_probs[SYN_CAT_ADJ] = 0.3f;
                ctx->category_probs[SYN_CAT_CONJ] = 0.1f;
                break;
            case SYN_CAT_NOUN:
                ctx->category_probs[SYN_CAT_VERB] = 0.5f;
                ctx->category_probs[SYN_CAT_PREP] = 0.2f;
                ctx->category_probs[SYN_CAT_CONJ] = 0.15f;
                ctx->category_probs[SYN_CAT_PUNCT] = 0.15f;
                break;
            case SYN_CAT_VERB:
                ctx->category_probs[SYN_CAT_DET] = 0.3f;
                ctx->category_probs[SYN_CAT_NOUN] = 0.25f;
                ctx->category_probs[SYN_CAT_ADV] = 0.2f;
                ctx->category_probs[SYN_CAT_PREP] = 0.15f;
                ctx->category_probs[SYN_CAT_PRON] = 0.1f;
                break;
            case SYN_CAT_PREP:
                ctx->category_probs[SYN_CAT_DET] = 0.5f;
                ctx->category_probs[SYN_CAT_NOUN] = 0.3f;
                ctx->category_probs[SYN_CAT_PRON] = 0.2f;
                break;
            default:
                /* Uniform */
                for (int i = 0; i < SYN_CAT_COUNT; i++) {
                    ctx->category_probs[i] = 1.0f / SYN_CAT_COUNT;
                }
                break;
        }
    }
}

/*=============================================================================
 * CONFIGURATION API
 *=============================================================================*/

int syntactic_default_config(syntactic_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return -1;
    }

    *config = (syntactic_config_t){
        .max_sentence_length = DEFAULT_MAX_SENTENCE_LENGTH,
        .beam_width = DEFAULT_BEAM_WIDTH,
        .pruning_threshold = DEFAULT_PRUNING_THRESHOLD,
        .enable_prediction = true,
        .prediction_weight = DEFAULT_PREDICTION_WEIGHT,
        .enable_thematic_roles = true,
        .strict_argument_structure = false,
        .enable_reanalysis = true,
        .max_reanalyses = DEFAULT_MAX_REANALYSES,
        .enable_semantic_guide = false,
        .semantic_weight = DEFAULT_SEMANTIC_WEIGHT
    };

    return 0;
}

/*=============================================================================
 * LIFECYCLE API
 *=============================================================================*/

syntactic_comprehension_t* syntactic_comprehension_create(
    const syntactic_config_t* config
) {
    syntactic_comprehension_t* ctx = nimcp_calloc(1, sizeof(syntactic_comprehension_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;

    }

    if (config) {
        ctx->config = *config;
    } else {
        syntactic_default_config(&ctx->config);
    }

    ctx->current_state = PARSE_STATE_INIT;
    ctx->current_probability = 1.0f;

    init_grammar_rules(ctx);
    init_argument_frames(ctx);

    return ctx;
}

void syntactic_comprehension_destroy(syntactic_comprehension_t* ctx) {
    if (!ctx) return;

    nimcp_free(ctx->rules);
    nimcp_free(ctx->arg_frames);
    nimcp_free(ctx);
}

/*=============================================================================
 * PARSING API
 *=============================================================================*/

int syntactic_parse_sentence(syntactic_comprehension_t* ctx,
                              const syntactic_word_t* words,
                              uint32_t num_words,
                              syntactic_parse_t* parse) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!words) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "words is NULL");
        return -1;
    }
    if (!parse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse is NULL");
        return -1;
    }
    if (num_words == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "num_words is zero");
        return -1;
    }
    if (num_words > SYNTACTIC_MAX_SENTENCE_LEN) return -1;

    /* Begin incremental parse */
    syntactic_begin_incremental(ctx);

    /* Add each word */
    for (uint32_t i = 0; i < num_words; i++) {
        parse_state_t state = syntactic_add_word(ctx, &words[i]);
        if (state == PARSE_STATE_ERROR) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "syntactic_comprehension_destroy: validation failed");
            return -1;
        }
    }

    /* Finish and get result */
    return syntactic_finish_incremental(ctx, parse);
}

int syntactic_begin_incremental(syntactic_comprehension_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }

    /* Reset state */
    ctx->buffer_len = 0;
    ctx->stack_depth = 0;
    ctx->reanalysis_count = 0;
    ctx->current_state = PARSE_STATE_ACTIVE;
    ctx->current_probability = 1.0f;
    ctx->reductions_applied = 0;
    ctx->current_is_question = false;
    ctx->current_is_imperative = false;

    reset_node_pool(ctx);
    update_predictions(ctx);

    return 0;
}

parse_state_t syntactic_add_word(syntactic_comprehension_t* ctx,
                                  const syntactic_word_t* word) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return PARSE_STATE_ERROR;
    }
    if (!word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "word is NULL");
        return PARSE_STATE_ERROR;
    }
    if (ctx->buffer_len >= SYNTACTIC_MAX_SENTENCE_LEN) return PARSE_STATE_ERROR;

    /* Add to buffer */
    ctx->word_buffer[ctx->buffer_len] = *word;
    ctx->word_buffer[ctx->buffer_len].position = ctx->buffer_len;
    ctx->buffer_len++;

    /* Update prediction error (for garden path detection) */
    float predicted_prob = ctx->category_probs[word->category];
    if (predicted_prob < 0.1f) {
        /* Low probability - possible garden path */
        if (ctx->config.enable_reanalysis &&
            ctx->reanalysis_count < ctx->config.max_reanalyses) {
            ctx->current_state = PARSE_STATE_GARDEN_PATH;
        }
    }

    /* Sentence-type detection at sentence start (position 0):
     *  - If the first word is an auxiliary verb (e.g. "is the cat hungry"),
     *    flag the parse as a yes/no question (auxiliary inversion).
     *  - If the first word is a wh-word (who/what/when/where/why/how/which),
     *    flag the parse as a wh-question.
     *  - The imperative flag (VP at sentence-start, null subject) is set
     *    at finish_incremental when the final tree has VP as root. */
    if (word->position == 0) {
        if (word->category == SYN_CAT_AUX) {
            ctx->current_is_question = true;
        } else if (is_wh_word(word->word)) {
            ctx->current_is_question = true;
        }
    }

    /* Create terminal node */
    syntactic_node_t* terminal = allocate_node(ctx);
    if (terminal) {
        terminal->phrase_type = category_to_phrase(word->category);
        terminal->start_pos = word->position;
        terminal->end_pos = word->position + 1;
        terminal->is_complete = true;
        terminal->probability = word->category_confidence;

        /* Punctuation is transparent: don't push onto the parse stack;
         * it would block reductions otherwise. */
        if (word->category == SYN_CAT_PUNCT) {
            /* Skip stack push for punctuation. */
        } else if (ctx->stack_depth < MAX_STACK_DEPTH) {
            ctx->parse_stack[ctx->stack_depth++] = terminal;
        }
    }

    /* Try to reduce (bottom-up).
     *
     * Coverage expansion 2026-05-11: previously only Det+N → NP and
     * P+NP → PP fired here. Added:
     *   V + NP → VP            (transitive)
     *   V + PP → VP            (PP-attachment)
     *   NP + VP → S            (basic declarative)
     *   NP + CP + NP → NP      (conjoined NPs: "the cat and the dog")
     *   S  + CP + S  → S       (conjoined clauses)
     *   CP + S → CP (question) (auxiliary inversion -- rough heuristic)
     *   (lone VP at root) → S-imperative (handled at finish_incremental)
     *
     * Out of scope for this commit: relative clauses, embedded
     * complement clauses ("that"-clauses), passive (be+V-en),
     * subject-aux inversion in matrix clauses requiring movement
     * traces, and full coordinate ellipsis. */

    /* Three-element conjunction reductions, checked BEFORE the binary
     * loop so the conjunction phrase (PHRASE_CP, re-used for "and"/"or")
     * doesn't get prematurely combined with just one side. */
    while (ctx->stack_depth >= 3) {
        syntactic_node_t* top    = ctx->parse_stack[ctx->stack_depth - 1];
        syntactic_node_t* middle = ctx->parse_stack[ctx->stack_depth - 2];
        syntactic_node_t* bottom = ctx->parse_stack[ctx->stack_depth - 3];

        /* NP + CONJ(CP) + NP → NP   ("the cat and the dog") */
        if (bottom->phrase_type == PHRASE_NP &&
            middle->phrase_type == PHRASE_CP &&
            top->phrase_type    == PHRASE_NP) {
            syntactic_node_t* np = allocate_node(ctx);
            if (np) {
                np->phrase_type = PHRASE_NP;
                np->start_pos = bottom->start_pos;
                np->end_pos   = top->end_pos;
                np->is_complete = true;
                np->probability = bottom->probability *
                                  middle->probability *
                                  top->probability * 0.5f;
                ctx->stack_depth -= 3;
                ctx->parse_stack[ctx->stack_depth++] = np;
                ctx->reductions_applied |= WERNICKE_REDUCTION_NP_CONJ_NP;
                continue;
            }
        }

        /* S + CONJ(CP) + S → S   ("X ran and Y jumped") */
        if (bottom->phrase_type == PHRASE_S &&
            middle->phrase_type == PHRASE_CP &&
            top->phrase_type    == PHRASE_S) {
            syntactic_node_t* s = allocate_node(ctx);
            if (s) {
                s->phrase_type = PHRASE_S;
                s->start_pos = bottom->start_pos;
                s->end_pos   = top->end_pos;
                s->is_complete = true;
                s->probability = bottom->probability *
                                 middle->probability *
                                 top->probability * 0.4f;
                ctx->stack_depth -= 3;
                ctx->parse_stack[ctx->stack_depth++] = s;
                ctx->reductions_applied |= WERNICKE_REDUCTION_S_CONJ_S;
                continue;
            }
        }

        /* VP + CONJ(CP) + VP → VP  ("ran and jumped") */
        if (bottom->phrase_type == PHRASE_VP &&
            middle->phrase_type == PHRASE_CP &&
            top->phrase_type    == PHRASE_VP) {
            syntactic_node_t* vp = allocate_node(ctx);
            if (vp) {
                vp->phrase_type = PHRASE_VP;
                vp->start_pos = bottom->start_pos;
                vp->end_pos   = top->end_pos;
                vp->is_complete = true;
                vp->probability = bottom->probability *
                                  middle->probability *
                                  top->probability * 0.4f;
                ctx->stack_depth -= 3;
                ctx->parse_stack[ctx->stack_depth++] = vp;
                ctx->reductions_applied |= WERNICKE_REDUCTION_S_CONJ_S;
                continue;
            }
        }

        break;  /* No ternary reduction possible */
    }

    /* Simplified: try to combine top stack elements (binary) */
    while (ctx->stack_depth >= 2) {
        syntactic_node_t* top = ctx->parse_stack[ctx->stack_depth - 1];
        syntactic_node_t* second = ctx->parse_stack[ctx->stack_depth - 2];

        /* Check for NP: Det + N */
        if (second->phrase_type == PHRASE_DP &&
            top->phrase_type == PHRASE_NP) {
            syntactic_node_t* np = allocate_node(ctx);
            if (np) {
                np->phrase_type = PHRASE_NP;
                np->start_pos = second->start_pos;
                np->end_pos = top->end_pos;
                np->is_complete = true;
                np->probability = second->probability * top->probability * 0.6f;

                /* Pop two, push combined */
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = np;
                ctx->reductions_applied |= WERNICKE_REDUCTION_NP_DET_N;
                continue;
            }
        }

        /* Check for PP: P + NP */
        if (second->phrase_type == PHRASE_PP &&
            top->phrase_type == PHRASE_NP &&
            !second->is_complete) {
            second->end_pos = top->end_pos;
            second->is_complete = true;
            second->probability *= top->probability * 0.9f;
            ctx->stack_depth--;
            ctx->reductions_applied |= WERNICKE_REDUCTION_PP_P_NP;
            continue;
        }

        /* VP + NP → VP  (transitive verb: "ate [the apple]") */
        if (second->phrase_type == PHRASE_VP &&
            top->phrase_type    == PHRASE_NP) {
            syntactic_node_t* vp = allocate_node(ctx);
            if (vp) {
                vp->phrase_type = PHRASE_VP;
                vp->start_pos = second->start_pos;
                vp->end_pos   = top->end_pos;
                vp->is_complete = true;
                vp->probability = second->probability *
                                  top->probability * 0.5f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = vp;
                ctx->reductions_applied |= WERNICKE_REDUCTION_VP_V_NP;
                continue;
            }
        }

        /* VP + PP → VP  ("ran [to the store]") */
        if (second->phrase_type == PHRASE_VP &&
            top->phrase_type    == PHRASE_PP) {
            syntactic_node_t* vp = allocate_node(ctx);
            if (vp) {
                vp->phrase_type = PHRASE_VP;
                vp->start_pos = second->start_pos;
                vp->end_pos   = top->end_pos;
                vp->is_complete = true;
                vp->probability = second->probability *
                                  top->probability * 0.4f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = vp;
                ctx->reductions_applied |= WERNICKE_REDUCTION_VP_V_PP;
                continue;
            }
        }

        /* VP + AP → VP  ("became [happy]") */
        if (second->phrase_type == PHRASE_VP &&
            top->phrase_type    == PHRASE_AP) {
            syntactic_node_t* vp = allocate_node(ctx);
            if (vp) {
                vp->phrase_type = PHRASE_VP;
                vp->start_pos = second->start_pos;
                vp->end_pos   = top->end_pos;
                vp->is_complete = true;
                vp->probability = second->probability *
                                  top->probability * 0.3f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = vp;
                ctx->reductions_applied |= WERNICKE_REDUCTION_VP_V_NP;
                continue;
            }
        }

        /* ADVP + VP → VP  ("quickly [ran]") */
        if (second->phrase_type == PHRASE_ADVP &&
            top->phrase_type    == PHRASE_VP) {
            syntactic_node_t* vp = allocate_node(ctx);
            if (vp) {
                vp->phrase_type = PHRASE_VP;
                vp->start_pos = second->start_pos;
                vp->end_pos   = top->end_pos;
                vp->is_complete = true;
                vp->probability = second->probability *
                                  top->probability * 0.4f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = vp;
                ctx->reductions_applied |= WERNICKE_REDUCTION_VP_V_NP;
                continue;
            }
        }

        /* VP + ADVP → VP  ("ran [quickly]") */
        if (second->phrase_type == PHRASE_VP &&
            top->phrase_type    == PHRASE_ADVP) {
            syntactic_node_t* vp = allocate_node(ctx);
            if (vp) {
                vp->phrase_type = PHRASE_VP;
                vp->start_pos = second->start_pos;
                vp->end_pos   = top->end_pos;
                vp->is_complete = true;
                vp->probability = second->probability *
                                  top->probability * 0.4f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = vp;
                ctx->reductions_applied |= WERNICKE_REDUCTION_VP_V_NP;
                continue;
            }
        }

        /* NP + VP → S  (basic declarative sentence) */
        if (second->phrase_type == PHRASE_NP &&
            top->phrase_type    == PHRASE_VP) {
            syntactic_node_t* s = allocate_node(ctx);
            if (s) {
                s->phrase_type = PHRASE_S;
                s->start_pos = second->start_pos;
                s->end_pos   = top->end_pos;
                s->is_complete = true;
                s->probability = second->probability *
                                 top->probability * 0.8f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = s;
                ctx->reductions_applied |= WERNICKE_REDUCTION_S_NP_VP;
                /* If sentence started with AUX/WH, the whole S becomes
                 * the body of a CP-question. We don't restructure the
                 * tree here — the question flag was already set when
                 * the first word was seen. */
                if (ctx->current_is_question) {
                    ctx->reductions_applied |= WERNICKE_REDUCTION_CP_AUX_S;
                }
                continue;
            }
        }

        /* NP + NP → NP  (compound noun: "[cat] food") */
        if (second->phrase_type == PHRASE_NP &&
            top->phrase_type    == PHRASE_NP) {
            syntactic_node_t* np = allocate_node(ctx);
            if (np) {
                np->phrase_type = PHRASE_NP;
                np->start_pos = second->start_pos;
                np->end_pos   = top->end_pos;
                np->is_complete = true;
                np->probability = second->probability *
                                  top->probability * 0.3f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = np;
                ctx->reductions_applied |= WERNICKE_REDUCTION_NP_DET_N;
                continue;
            }
        }

        /* NP + PP → NP  (PP-attachment: "cat [on the mat]") */
        if (second->phrase_type == PHRASE_NP &&
            top->phrase_type    == PHRASE_PP) {
            syntactic_node_t* np = allocate_node(ctx);
            if (np) {
                np->phrase_type = PHRASE_NP;
                np->start_pos = second->start_pos;
                np->end_pos   = top->end_pos;
                np->is_complete = true;
                np->probability = second->probability *
                                  top->probability * 0.4f;
                ctx->stack_depth -= 2;
                ctx->parse_stack[ctx->stack_depth++] = np;
                ctx->reductions_applied |= WERNICKE_REDUCTION_NP_DET_N;
                continue;
            }
        }

        break;  /* No reduction possible */
    }

    /* Update predictions for next word */
    update_predictions(ctx);

    /* Update probability */
    ctx->current_probability *= (predicted_prob > 0.01f ? predicted_prob : 0.01f);

    ctx->stats.words_processed++;

    return ctx->current_state;
}

int syntactic_finish_incremental(syntactic_comprehension_t* ctx,
                                  syntactic_parse_t* parse) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!parse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse is NULL");
        return -1;
    }

    memset(parse, 0, sizeof(syntactic_parse_t));

    /* Copy words */
    parse->words = nimcp_calloc(ctx->buffer_len, sizeof(syntactic_word_t));
    if (parse->words) {
        memcpy(parse->words, ctx->word_buffer,
               ctx->buffer_len * sizeof(syntactic_word_t));
        parse->num_words = ctx->buffer_len;
    }

    /* Build final tree from stack */
    if (ctx->stack_depth > 0) {
        /* Create root S node */
        syntactic_node_t* root = allocate_node(ctx);
        if (root) {
            root->phrase_type = PHRASE_S;
            root->start_pos = 0;
            root->end_pos = ctx->buffer_len;
            root->is_complete = true;
            root->probability = ctx->current_probability;

            /* Allocate children array */
            root->children = nimcp_calloc(ctx->stack_depth, sizeof(syntactic_node_t*));
            if (root->children) {
                for (uint32_t i = 0; i < ctx->stack_depth; i++) {
                    root->children[i] = ctx->parse_stack[i];
                    ctx->parse_stack[i]->parent = root;
                }
                root->num_children = ctx->stack_depth;
            }

            parse->root = root;
        }
    }

    /* Copy nodes */
    parse->nodes = nimcp_calloc(ctx->nodes_allocated, sizeof(syntactic_node_t));
    if (parse->nodes) {
        memcpy(parse->nodes, ctx->node_pool,
               ctx->nodes_allocated * sizeof(syntactic_node_t));
        parse->num_nodes = ctx->nodes_allocated;
    }

    /* Set parse state */
    parse->state = ctx->current_state;
    parse->parse_probability = ctx->current_probability;
    parse->reanalysis_count = ctx->reanalysis_count;
    parse->is_grammatical = (ctx->current_state != PARSE_STATE_ERROR);
    parse->syntactic_complexity = syntactic_compute_complexity(parse);

    /* === Sentence-type flag propagation (parser-coverage expansion) ===
     *
     * Imperative detection: a sentence-initial VP with no preceding NP
     * subject ("run!", "give me the ball"). Heuristic: first word is a
     * VERB (not AUX — that's question territory), and no NP+VP reduction
     * fired (no subject was seen). */
    bool first_is_verb =
        (ctx->buffer_len > 0 &&
         ctx->word_buffer[0].category == SYN_CAT_VERB);
    bool s_np_vp_fired =
        (ctx->reductions_applied & WERNICKE_REDUCTION_S_NP_VP) != 0;
    if (first_is_verb && !s_np_vp_fired && !ctx->current_is_question) {
        ctx->current_is_imperative = true;
        ctx->reductions_applied |= WERNICKE_REDUCTION_S_IMPERATIVE;
    }

    /* Lone-VP root → S(intransitive) bookkeeping bit. Fires when the
     * final stack has exactly one VP element and we didn't see NP+VP. */
    if (!s_np_vp_fired && ctx->stack_depth == 1 &&
        ctx->parse_stack[0]->phrase_type == PHRASE_VP) {
        ctx->reductions_applied |= WERNICKE_REDUCTION_VP_INTRANSITIVE;
    }

    parse->is_question     = ctx->current_is_question;
    parse->is_imperative   = ctx->current_is_imperative;
    parse->reductions_applied = ctx->reductions_applied;

    /* Extract dependencies */
    parse->dependencies = nimcp_calloc(SYNTACTIC_MAX_DEPENDENCIES,
                                  sizeof(syntactic_dependency_t));
    if (parse->dependencies) {
        syntactic_extract_dependencies(ctx, parse,
                                        parse->dependencies,
                                        SYNTACTIC_MAX_DEPENDENCIES,
                                        &parse->num_dependencies);
    }

    /* Assign thematic roles */
    if (ctx->config.enable_thematic_roles) {
        parse->roles = nimcp_calloc(SYNTACTIC_MAX_ROLES, sizeof(thematic_assignment_t));
        if (parse->roles) {
            syntactic_assign_roles(ctx, parse,
                                    parse->roles,
                                    SYNTACTIC_MAX_ROLES,
                                    &parse->num_roles);
        }
    }

    ctx->current_state = PARSE_STATE_COMPLETE;
    ctx->stats.sentences_parsed++;
    ctx->stats.constituents_built += ctx->nodes_allocated;

    return 0;
}

int syntactic_get_incremental_state(const syntactic_comprehension_t* ctx,
                                     incremental_state_t* state) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");
        return -1;
    }

    state->buffer = (syntactic_word_t*)ctx->word_buffer;
    state->buffer_len = ctx->buffer_len;
    state->stack = (syntactic_node_t**)ctx->parse_stack;
    state->stack_depth = ctx->stack_depth;
    state->predictions = (float*)ctx->category_probs;
    state->prediction_len = SYN_CAT_COUNT;
    state->state = ctx->current_state;

    return 0;
}

/*=============================================================================
 * PREDICTION API
 *=============================================================================*/

syntactic_category_t syntactic_predict_category(
    const syntactic_comprehension_t* ctx,
    float* probs
) {
    if (!ctx) return SYN_CAT_UNKNOWN;

    if (probs) {
        memcpy(probs, ctx->category_probs, sizeof(ctx->category_probs));
    }

    /* Find max */
    syntactic_category_t best = SYN_CAT_NOUN;
    float best_prob = ctx->category_probs[0];
    for (int i = 1; i < SYN_CAT_COUNT; i++) {
        if (ctx->category_probs[i] > best_prob) {
            best_prob = ctx->category_probs[i];
            best = (syntactic_category_t)i;
        }
    }

    return best;
}

int syntactic_predict_phrase(const syntactic_comprehension_t* ctx,
                              phrase_type_t* phrase_type,
                              float* confidence) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!phrase_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phrase_type is NULL");
        return -1;
    }
    if (!confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "confidence is NULL");
        return -1;
    }

    /* Predict based on stack state */
    if (ctx->stack_depth == 0) {
        *phrase_type = PHRASE_NP;  /* Expect subject NP */
        *confidence = 0.8f;
    } else {
        syntactic_node_t* top = ctx->parse_stack[ctx->stack_depth - 1];
        if (top->phrase_type == PHRASE_NP && !top->is_complete) {
            *phrase_type = PHRASE_NP;  /* Continue NP */
            *confidence = 0.7f;
        } else if (top->phrase_type == PHRASE_NP && top->is_complete) {
            *phrase_type = PHRASE_VP;  /* Expect predicate */
            *confidence = 0.8f;
        } else if (top->phrase_type == PHRASE_VP) {
            *phrase_type = PHRASE_NP;  /* Expect object */
            *confidence = 0.6f;
        } else {
            *phrase_type = PHRASE_UNKNOWN;
            *confidence = 0.3f;
        }
    }

    return 0;
}

/*=============================================================================
 * DEPENDENCY API
 *=============================================================================*/

int syntactic_extract_dependencies(syntactic_comprehension_t* ctx,
                                    const syntactic_parse_t* parse,
                                    syntactic_dependency_t* dependencies,
                                    uint32_t max_deps,
                                    uint32_t* num_deps) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!parse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse is NULL");
        return -1;
    }
    if (!dependencies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dependencies is NULL");
        return -1;
    }
    if (!num_deps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "num_deps is NULL");
        return -1;
    }

    *num_deps = 0;

    /* Simple heuristic extraction */
    int verb_pos = -1;
    int subj_pos = -1;
    int obj_pos = -1;

    /* Find main verb, subject, object */
    for (uint32_t i = 0; i < parse->num_words; i++) {
        if (parse->words[i].category == SYN_CAT_VERB && verb_pos < 0) {
            verb_pos = (int)i;
        } else if ((parse->words[i].category == SYN_CAT_NOUN ||
                    parse->words[i].category == SYN_CAT_PRON) &&
                   verb_pos < 0 && subj_pos < 0) {
            subj_pos = (int)i;
        } else if ((parse->words[i].category == SYN_CAT_NOUN ||
                    parse->words[i].category == SYN_CAT_PRON) &&
                   verb_pos >= 0 && obj_pos < 0) {
            obj_pos = (int)i;
        }
    }

    /* Create dependencies */
    if (verb_pos >= 0) {
        /* ROOT -> verb */
        if (*num_deps < max_deps) {
            dependencies[*num_deps] = (syntactic_dependency_t){
                .head_pos = (uint32_t)verb_pos,
                .dependent_pos = (uint32_t)verb_pos,
                .relation = DEP_ROOT,
                .confidence = 0.9f
            };
            (*num_deps)++;
        }

        /* Subject */
        if (subj_pos >= 0 && *num_deps < max_deps) {
            dependencies[*num_deps] = (syntactic_dependency_t){
                .head_pos = (uint32_t)verb_pos,
                .dependent_pos = (uint32_t)subj_pos,
                .relation = DEP_NSUBJ,
                .confidence = 0.85f
            };
            (*num_deps)++;
        }

        /* Object */
        if (obj_pos >= 0 && *num_deps < max_deps) {
            dependencies[*num_deps] = (syntactic_dependency_t){
                .head_pos = (uint32_t)verb_pos,
                .dependent_pos = (uint32_t)obj_pos,
                .relation = DEP_DOBJ,
                .confidence = 0.8f
            };
            (*num_deps)++;
        }
    }

    /* Det -> Noun dependencies */
    for (uint32_t i = 0; i + 1 < parse->num_words && *num_deps < max_deps; i++) {
        if (parse->words[i].category == SYN_CAT_DET &&
            parse->words[i + 1].category == SYN_CAT_NOUN) {
            dependencies[*num_deps] = (syntactic_dependency_t){
                .head_pos = i + 1,
                .dependent_pos = i,
                .relation = DEP_DET,
                .confidence = 0.95f
            };
            (*num_deps)++;
        }
    }

    return 0;
}

int32_t syntactic_find_head(const syntactic_node_t* node) {
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "syntactic_find_head: node is NULL");
        return -1;
    }

    if (node->num_children == 0) {
        return (int32_t)node->start_pos;
    }

    if (node->head_child < node->num_children) {
        return syntactic_find_head(node->children[node->head_child]);
    }

    /* Default: rightmost child for VP, leftmost for NP */
    if (node->phrase_type == PHRASE_VP) {
        return syntactic_find_head(node->children[node->num_children - 1]);
    } else {
        return syntactic_find_head(node->children[0]);
    }
}

/*=============================================================================
 * THEMATIC ROLE API
 *=============================================================================*/

int syntactic_assign_roles(syntactic_comprehension_t* ctx,
                            const syntactic_parse_t* parse,
                            thematic_assignment_t* roles,
                            uint32_t max_roles,
                            uint32_t* num_roles) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!parse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse is NULL");
        return -1;
    }
    if (!roles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "roles is NULL");
        return -1;
    }
    if (!num_roles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "num_roles is NULL");
        return -1;
    }

    *num_roles = 0;

    /* Find verbs and assign roles to arguments */
    for (uint32_t i = 0; i < parse->num_words; i++) {
        if (parse->words[i].category == SYN_CAT_VERB) {
            uint32_t verb_pos = i;

            /* Look for subject (before verb) */
            for (int j = (int)i - 1; j >= 0 && *num_roles < max_roles; j--) {
                if (parse->words[j].category == SYN_CAT_NOUN ||
                    parse->words[j].category == SYN_CAT_PRON) {
                    roles[*num_roles] = (thematic_assignment_t){
                        .verb_pos = verb_pos,
                        .argument_start = (uint32_t)j,
                        .argument_end = (uint32_t)j + 1,
                        .role = ROLE_AGENT,
                        .confidence = 0.8f
                    };
                    (*num_roles)++;
                    break;
                }
            }

            /* Look for object (after verb) */
            for (uint32_t j = i + 1; j < parse->num_words && *num_roles < max_roles; j++) {
                if (parse->words[j].category == SYN_CAT_NOUN ||
                    parse->words[j].category == SYN_CAT_PRON) {
                    roles[*num_roles] = (thematic_assignment_t){
                        .verb_pos = verb_pos,
                        .argument_start = j,
                        .argument_end = j + 1,
                        .role = ROLE_PATIENT,
                        .confidence = 0.75f
                    };
                    (*num_roles)++;
                    break;
                }
            }
        }
    }

    return 0;
}

int syntactic_get_argument_structure(syntactic_comprehension_t* ctx,
                                      const syntactic_word_t* verb_word,
                                      thematic_role_t* expected_roles,
                                      uint32_t* num_roles) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!verb_word) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "verb_word is NULL");
        return -1;
    }
    if (!expected_roles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expected_roles is NULL");
        return -1;
    }
    if (!num_roles) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "num_roles is NULL");
        return -1;
    }

    /* Look up verb in frames */
    for (uint32_t i = 0; i < ctx->num_frames; i++) {
        if (strcmp(ctx->arg_frames[i].verb_lemma, verb_word->word) == 0) {
            memcpy(expected_roles, ctx->arg_frames[i].roles,
                   ctx->arg_frames[i].num_roles * sizeof(thematic_role_t));
            *num_roles = ctx->arg_frames[i].num_roles;
            return 0;
        }
    }

    /* Default transitive frame */
    expected_roles[0] = ROLE_AGENT;
    expected_roles[1] = ROLE_PATIENT;
    *num_roles = 2;

    return 0;
}

/*=============================================================================
 * GARDEN PATH API
 *=============================================================================*/

bool syntactic_is_garden_path(const syntactic_comprehension_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->current_state == PARSE_STATE_GARDEN_PATH;
}

int syntactic_reanalyze(syntactic_comprehension_t* ctx,
                         syntactic_parse_t* parse) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!parse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse is NULL");
        return -1;
    }

    if (ctx->reanalysis_count >= ctx->config.max_reanalyses) {
        return -1;  /* Too many reanalyses */
    }

    ctx->reanalysis_count++;
    ctx->stats.reanalyses++;

    /* Simplified: just mark as reanalyzed */
    ctx->current_state = PARSE_STATE_AMBIGUOUS;
    parse->state = PARSE_STATE_AMBIGUOUS;
    parse->reanalysis_count = ctx->reanalysis_count;

    return 0;
}

int syntactic_get_reanalysis_suggestions(
    const syntactic_comprehension_t* ctx,
    uint32_t* suggestions,
    uint32_t max_suggestions,
    uint32_t* num_suggestions
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!suggestions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "suggestions is NULL");
        return -1;
    }
    if (!num_suggestions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "num_suggestions is NULL");
        return -1;
    }

    /* Return PP attachment points as suggestions */
    *num_suggestions = 0;

    for (uint32_t i = 0; i < ctx->buffer_len && *num_suggestions < max_suggestions; i++) {
        if (ctx->word_buffer[i].category == SYN_CAT_PREP) {
            suggestions[*num_suggestions] = i;
            (*num_suggestions)++;
        }
    }

    return 0;
}

/*=============================================================================
 * QUERY API
 *=============================================================================*/

int syntactic_get_stats(const syntactic_comprehension_t* ctx,
                         syntactic_stats_t* stats) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return -1;
    }
    *stats = ctx->stats;
    return 0;
}

void syntactic_reset_stats(syntactic_comprehension_t* ctx) {
    if (!ctx) return;
    memset(&ctx->stats, 0, sizeof(syntactic_stats_t));
}

float syntactic_compute_complexity(const syntactic_parse_t* parse) {
    if (!parse || parse->num_words == 0) return 0.0f;

    float complexity = 0.0f;

    /* Factors: sentence length, embedding depth, dependencies */
    complexity += (float)parse->num_words / SYNTACTIC_MAX_SENTENCE_LEN;
    complexity += (float)parse->num_dependencies / SYNTACTIC_MAX_DEPENDENCIES;
    complexity += (float)parse->reanalysis_count * 0.1f;

    /* Clamp to [0, 1] */
    if (complexity > 1.0f) complexity = 1.0f;

    return complexity;
}

bool syntactic_is_grammatical(const syntactic_comprehension_t* ctx,
                               const syntactic_parse_t* parse) {
    if (!ctx || !parse) {
        return false;
    }
    return parse->state != PARSE_STATE_ERROR &&
           parse->parse_probability > ctx->config.pruning_threshold;
}

/*=============================================================================
 * MEMORY MANAGEMENT API
 *=============================================================================*/

void syntactic_parse_free(syntactic_parse_t* parse) {
    if (!parse) return;

    nimcp_free(parse->words);
    nimcp_free(parse->dependencies);
    nimcp_free(parse->roles);

    /* Free node children arrays */
    if (parse->nodes) {
        for (uint32_t i = 0; i < parse->num_nodes; i++) {
            nimcp_free(parse->nodes[i].children);
        }
        nimcp_free(parse->nodes);
    }

    memset(parse, 0, sizeof(syntactic_parse_t));
}

int syntactic_parse_clone(const syntactic_parse_t* src,
                           syntactic_parse_t* dst) {
    if (!src) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");
        return -1;
    }
    if (!dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dst is NULL");
        return -1;
    }

    memset(dst, 0, sizeof(syntactic_parse_t));

    /* Clone words */
    if (src->words && src->num_words > 0) {
        dst->words = nimcp_calloc(src->num_words, sizeof(syntactic_word_t));
        if (dst->words) {
            memcpy(dst->words, src->words,
                   src->num_words * sizeof(syntactic_word_t));
            dst->num_words = src->num_words;
        }
    }

    /* Clone dependencies */
    if (src->dependencies && src->num_dependencies > 0) {
        dst->dependencies = nimcp_calloc(src->num_dependencies,
                                    sizeof(syntactic_dependency_t));
        if (dst->dependencies) {
            memcpy(dst->dependencies, src->dependencies,
                   src->num_dependencies * sizeof(syntactic_dependency_t));
            dst->num_dependencies = src->num_dependencies;
        }
    }

    /* Clone roles */
    if (src->roles && src->num_roles > 0) {
        dst->roles = nimcp_calloc(src->num_roles, sizeof(thematic_assignment_t));
        if (dst->roles) {
            memcpy(dst->roles, src->roles,
                   src->num_roles * sizeof(thematic_assignment_t));
            dst->num_roles = src->num_roles;
        }
    }

    /* Copy scalar fields */
    dst->state = src->state;
    dst->parse_probability = src->parse_probability;
    dst->syntactic_complexity = src->syntactic_complexity;
    dst->is_grammatical = src->is_grammatical;
    dst->reanalysis_count = src->reanalysis_count;

    return 0;
}

/*=============================================================================
 * STRING CONVERSION API
 *=============================================================================*/

const char* syntactic_category_to_string(syntactic_category_t cat) {
    switch (cat) {
        case SYN_CAT_NOUN:    return "N";
        case SYN_CAT_VERB:    return "V";
        case SYN_CAT_ADJ:     return "Adj";
        case SYN_CAT_ADV:     return "Adv";
        case SYN_CAT_DET:     return "Det";
        case SYN_CAT_PREP:    return "P";
        case SYN_CAT_CONJ:    return "Conj";
        case SYN_CAT_PRON:    return "Pron";
        case SYN_CAT_AUX:     return "Aux";
        case SYN_CAT_COMP:    return "Comp";
        case SYN_CAT_NEG:     return "Neg";
        case SYN_CAT_PUNCT:   return "Punct";
        default:              return "?";
    }
}

const char* syntactic_phrase_to_string(phrase_type_t phrase) {
    switch (phrase) {
        case PHRASE_NP:      return "NP";
        case PHRASE_VP:      return "VP";
        case PHRASE_PP:      return "PP";
        case PHRASE_AP:      return "AP";
        case PHRASE_ADVP:    return "AdvP";
        case PHRASE_S:       return "S";
        case PHRASE_SBAR:    return "SBAR";
        case PHRASE_CP:      return "CP";
        case PHRASE_IP:      return "IP";
        case PHRASE_DP:      return "DP";
        default:             return "?";
    }
}

const char* syntactic_role_to_string(thematic_role_t role) {
    switch (role) {
        case ROLE_AGENT:       return "Agent";
        case ROLE_PATIENT:     return "Patient";
        case ROLE_THEME:       return "Theme";
        case ROLE_EXPERIENCER: return "Experiencer";
        case ROLE_GOAL:        return "Goal";
        case ROLE_SOURCE:      return "Source";
        case ROLE_LOCATION:    return "Location";
        case ROLE_INSTRUMENT:  return "Instrument";
        case ROLE_BENEFICIARY: return "Beneficiary";
        case ROLE_TIME:        return "Time";
        case ROLE_MANNER:      return "Manner";
        case ROLE_CAUSE:       return "Cause";
        default:               return "?";
    }
}

const char* syntactic_dependency_to_string(dependency_type_t dep) {
    switch (dep) {
        case DEP_NSUBJ:  return "nsubj";
        case DEP_DOBJ:   return "dobj";
        case DEP_IOBJ:   return "iobj";
        case DEP_NMOD:   return "nmod";
        case DEP_AMOD:   return "amod";
        case DEP_ADVMOD: return "advmod";
        case DEP_DET:    return "det";
        case DEP_CASE:   return "case";
        case DEP_AUX:    return "aux";
        case DEP_COP:    return "cop";
        case DEP_MARK:   return "mark";
        case DEP_CONJ:   return "conj";
        case DEP_CC:     return "cc";
        case DEP_ROOT:   return "ROOT";
        case DEP_PUNCT:  return "punct";
        default:         return "?";
    }
}

const char* syntactic_state_to_string(parse_state_t state) {
    switch (state) {
        case PARSE_STATE_INIT:        return "init";
        case PARSE_STATE_ACTIVE:      return "active";
        case PARSE_STATE_COMPLETE:    return "complete";
        case PARSE_STATE_AMBIGUOUS:   return "ambiguous";
        case PARSE_STATE_GARDEN_PATH: return "garden_path";
        case PARSE_STATE_ERROR:       return "error";
        default:                      return "unknown";
    }
}
