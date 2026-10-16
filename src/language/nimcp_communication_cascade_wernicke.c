/**
 * @file nimcp_communication_cascade_wernicke.c
 * @brief Stage 0 of the production cascade — input-side Wernicke
 *        comprehension. Pulled into its own TU because Wernicke's
 *        nimcp_syntactic_comprehension.h and Broca's
 *        nimcp_syntax_processor.h both define `phrase_type_t` with
 *        overlapping enum values; including both in the same TU
 *        produces redeclaration errors.
 *
 * Routes the prompt through Wernicke's syntactic_comprehension. Output:
 * parse tree, speech-act classification (question / imperative / decl),
 * subject/verb/object identification, garden-path flag, complexity.
 *
 * Phase 2D-A v1: heuristic POS tagger via the GL lexicon's
 * learned_class field (same data the GL→Broca mirror uses). Phase 2D-A
 * v2 will route through Wernicke's lexical_access cohort model for
 * proper disambiguation. v1 is enough to replace the punctuation-based
 * speech-act guess in stage_goal with a real syntactic signal.
 */

#include "language/nimcp_communication_cascade.h"
#include "language/nimcp_grounded_language.h"

#include "core/brain/nimcp_brain_internal.h"

#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_syntactic_comprehension.h"
#include "snn/bridges/nimcp_snn_language_bridge.h"

#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>

/* Internal-but-non-static accessor: find a lexicon entry by lowercased
 * form. Defined in nimcp_grounded_language.c. */
extern const gl_lexicon_entry_t* lexicon_find_internal(
    const grounded_language_t* gl, const char* word);

/* Map gl_word_class_t → SYN_CAT_*. Wernicke uses a separate POS enum
 * from Broca (different enum types, overlapping but not identical),
 * so the GL→Broca mirror's mapping doesn't transfer directly. */
static syntactic_category_t gl_class_to_syn_cat(gl_word_class_t cls) {
    switch (cls) {
        case GL_CLASS_NOUN:      return SYN_CAT_NOUN;
        case GL_CLASS_VERB:      return SYN_CAT_VERB;
        case GL_CLASS_ADJECTIVE: return SYN_CAT_ADJ;
        case GL_CLASS_ADVERB:    return SYN_CAT_ADV;
        case GL_CLASS_PRONOUN:   return SYN_CAT_PRON;
        case GL_CLASS_FUNCTION:  return SYN_CAT_DET;
        case GL_CLASS_UNKNOWN:
        default:                 return SYN_CAT_UNKNOWN;
    }
}

static bool token_is_wh_word(const char* w) {
    if (!w) return false;
    static const char* wh[] = {
        "what","where","when","who","whom","whose","why","how","which",
        NULL
    };
    for (int i = 0; wh[i]; i++) {
        if (strcasecmp(w, wh[i]) == 0) return true;
    }
    return false;
}

static bool token_is_aux(const char* w) {
    if (!w) return false;
    static const char* aux[] = {
        "is","are","was","were","am","be","been","being",
        "do","does","did","done",
        "have","has","had","having",
        "can","could","may","might","must","shall","should","will","would",
        NULL
    };
    for (int i = 0; aux[i]; i++) {
        if (strcasecmp(w, aux[i]) == 0) return true;
    }
    return false;
}

/* Exported (no static) — called from nimcp_communication_cascade.c which
 * has a forward decl. Records its own skip / fail / complete on the
 * cascade state via the helpers used by all stages. We don't include
 * those helpers' headers here; the cascade orchestrator reads our
 * outputs and bumps the right counters itself. Instead of touching
 * stages_* we just write the output fields and let the orchestrator
 * mark this stage completed via record_complete in the dispatch. */
int cascade_stage_wernicke(brain_t brain, const char* prompt,
                             production_cascade_state_t* state) {
    state->wernicke_parsed       = false;
    state->prompt_is_question    = false;
    state->prompt_is_imperative  = false;
    state->prompt_is_garden_path = false;
    state->prompt_word_count     = 0;
    state->prompt_complexity     = 0.0f;
    state->prompt_subject[0]     = '\0';
    state->prompt_verb[0]        = '\0';
    state->prompt_object[0]      = '\0';

    if (!prompt || !prompt[0]) return 0;
    if (!brain->grounded_lang) return 0;

    /* The Wernicke parser path is optional — if the adapter is missing
     * or its syntactic submodule wasn't initialized, we still extract
     * speech-act + SVO heuristically from token POS. The parse adds
     * complexity / garden-path / grammaticality on top when available. */
    syntactic_comprehension_t* syn = brain->wernicke
        ? wernicke_get_syntactic_comprehension(brain->wernicke)
        : NULL;

    /* Tokenize prompt into syntactic_word_t array. POS comes from GL
     * lexicon's learned_class field; words not in the lexicon get
     * SYN_CAT_UNKNOWN (the parser tolerates a few unknowns). */
    enum { MAX_WORDS = 32 };
    syntactic_word_t words[MAX_WORDS];
    char dup[1024];
    size_t plen = strlen(prompt);
    if (plen >= sizeof(dup)) plen = sizeof(dup) - 1;
    memcpy(dup, prompt, plen);
    dup[plen] = '\0';

    uint32_t num_words = 0;
    char* save = NULL;
    char* tok = strtok_r(dup, " \t\n\r.,!?;:\"'", &save);
    while (tok && num_words < MAX_WORDS) {
        if (!tok[0]) {
            tok = strtok_r(NULL, " \t\n\r.,!?;:\"'", &save);
            continue;
        }

        memset(&words[num_words], 0, sizeof(words[num_words]));
        size_t tlen = strlen(tok);
        if (tlen >= sizeof(words[num_words].word))
            tlen = sizeof(words[num_words].word) - 1;
        for (size_t k = 0; k < tlen; k++) {
            words[num_words].word[k] = (char)tolower((unsigned char)tok[k]);
        }
        words[num_words].word[tlen] = '\0';
        words[num_words].position = num_words;

        const gl_lexicon_entry_t* e = lexicon_find_internal(
            brain->grounded_lang, words[num_words].word);
        if (e) {
            words[num_words].category = gl_class_to_syn_cat(e->learned_class);
            words[num_words].category_confidence = e->class_confidence;
            words[num_words].lemma_id = e->form_hash;
        } else {
            words[num_words].category = SYN_CAT_UNKNOWN;
            words[num_words].category_confidence = 0.0f;
            /* Batch K followup — bump the lifetime miss counter so the
             * dashboard can spot prompts the lexicon is failing to cover.
             * Relaxed-order matches the rest of the cascade counter wiring. */
            atomic_fetch_add_explicit(&brain->cascade_wernicke_lexicon_miss,
                                      1u, memory_order_relaxed);
        }

        num_words++;
        tok = strtok_r(NULL, " \t\n\r.,!?;:\"'", &save);
    }

    state->prompt_word_count = num_words;
    if (num_words == 0) return 0;

    /* Speech-act detection (independent of parse success). */
    bool has_q_mark = (prompt[plen - 1] == '?');
    bool initial_wh  = token_is_wh_word(words[0].word);
    bool initial_aux = token_is_aux(words[0].word);
    state->prompt_is_question = has_q_mark || initial_wh || initial_aux;

    if (num_words >= 1 && words[0].category == SYN_CAT_VERB) {
        state->prompt_is_imperative = !state->prompt_is_question;
    }

    /* Heuristic SVO extraction — runs regardless of parse success.
     * First noun = subject, first verb = verb, second noun (after the
     * verb) = object. Phase 2D-A v2 will use Wernicke's
     * syntactic_assign_roles for proper thematic-role extraction once
     * the syntactic submodule is actually initialized on the brain. */
    for (uint32_t i = 0; i < num_words; i++) {
        if (words[i].category == SYN_CAT_NOUN &&
            state->prompt_subject[0] == '\0') {
            strncpy(state->prompt_subject, words[i].word,
                    sizeof(state->prompt_subject) - 1);
        } else if (words[i].category == SYN_CAT_VERB &&
                    state->prompt_verb[0] == '\0') {
            strncpy(state->prompt_verb, words[i].word,
                    sizeof(state->prompt_verb) - 1);
        } else if (words[i].category == SYN_CAT_NOUN &&
                    state->prompt_subject[0] != '\0' &&
                    state->prompt_object[0] == '\0' &&
                    state->prompt_verb[0] != '\0') {
            strncpy(state->prompt_object, words[i].word,
                    sizeof(state->prompt_object) - 1);
            break;
        }
    }

    /* Bonus #2: drive the SNN-language bridge's comprehend path on the
     * incoming prompt so concept-word bindings get unsupervised CSTDP
     * reinforcement when the comprehend_stdp flag is on (default OFF).
     * Output buffer is stack-allocated and intentionally discarded — we
     * only need the side effect on bridge state, not the activations.
     * Non-fatal if bridge is absent or comprehend fails. */
    if (brain->snn_lang_bridge) {
        enum { CASCADE_COMP_MAX_CONCEPTS = 256 };
        float concept_acts[CASCADE_COMP_MAX_CONCEPTS];
        uint32_t num_activated = 0;
        float comp_confidence = 0.0f;
        (void)snn_language_bridge_comprehend(
            brain->snn_lang_bridge, prompt,
            concept_acts, CASCADE_COMP_MAX_CONCEPTS,
            &num_activated, &comp_confidence);
    }

    /* Run Wernicke's parser if available. Failure here is non-fatal —
     * speech-act and SVO are already populated from the heuristics. */
    if (!syn) return 0;
    syntactic_parse_t parse;
    memset(&parse, 0, sizeof(parse));
    int rc = syntactic_parse_sentence(syn, words, num_words, &parse);
    if (rc == 0) {
        state->wernicke_parsed = true;
        state->prompt_is_garden_path = syntactic_is_garden_path(syn);
        state->prompt_complexity = syntactic_compute_complexity(&parse);
        if (state->prompt_complexity > 1.0f)
            state->prompt_complexity = 1.0f;
        syntactic_parse_free(&parse);
    }

    /* Stage_completed/skipped accounting is handled by the orchestrator
     * which checks state->wernicke_parsed after dispatch. */
    return 0;
}

/*============================================================================
 * Stage 8 (Phase 2D-B): Wernicke self-comprehension — sensorimotor loop
 *
 * After stages 6 (lexical) and 7 (syntactic) produce text via bridge +
 * Broca, we feed the brain's own output BACK through Wernicke +
 * grounded_language_comprehend and compare the re-derived semantic
 * vector to content_intent. The match score answers: "did the brain
 * actually say what it meant?"
 *
 * High match (>0.7): the production path successfully encoded the
 * intent. The bridge's bindings + Broca's syntax produced output that
 * means roughly the same thing as the intent that drove them.
 *
 * Low match (<0.3): the brain said something but it isn't what it
 * meant. The production path was lossy. Future work (Phase 2F+) will
 * route this match score back as a training signal — strengthen
 * bindings on high match, weaken on low match. That's the closed loop
 * the audit identified as the #1 architectural gap.
 *==========================================================================*/

/* Cosine similarity, defensive against zero vectors. Returns [0, 1]
 * (we clamp negative similarity to 0 since intent vectors should be
 * non-negative after ReLU). */
static float vec_cosine_sim(const float* a, const float* b, uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (uint32_t i = 0; i < dim; i++) {
        double ai = a[i], bi = b[i];
        dot += ai * bi;
        na  += ai * ai;
        nb  += bi * bi;
    }
    if (na < 1e-12 || nb < 1e-12) return 0.0f;
    double cs = dot / (sqrt(na) * sqrt(nb));
    if (cs < 0.0) cs = 0.0;
    if (cs > 1.0) cs = 1.0;
    return (float)cs;
}

int cascade_stage_self_comprehension(brain_t brain,
                                       production_cascade_state_t* state) {
    state->self_parsed         = false;
    state->self_complexity     = 0.0f;
    state->self_match          = 0.0f;
    state->self_grammaticality = 0.0f;

    if (!state->utterance || !state->utterance[0]) return 0;
    if (!brain->grounded_lang) return 0;

    /* Re-comprehend the brain's own output. Uses the lexicon-side
     * comprehend (same path that Stage 0 uses) — produces a semantic
     * vector. We compare that to state->content_intent: the cosine
     * similarity is the match score.
     *
     * NOTE: this comprehend fires GL_EVENT_COMPREHENDED on the cognitive
     * bus, which would normally feed observers (ToM, PFC, etc.). For
     * self-talk that's actually what we want — the brain hearing its
     * own voice updates working memory and ToM the same way external
     * input would. */
    gl_comprehension_result_t self_comp;
    memset(&self_comp, 0, sizeof(self_comp));
    int rc = grounded_language_comprehend(brain->grounded_lang,
                                            state->utterance, &self_comp);
    if (rc == 0 && self_comp.semantic_vector && state->content_intent) {
        uint32_t dim = state->content_dim;
        uint32_t comp_dim = grounded_language_get_semantic_dim(brain->grounded_lang);
        if (comp_dim < dim) dim = comp_dim;
        state->self_match = vec_cosine_sim(state->content_intent,
                                             self_comp.semantic_vector,
                                             dim);
        state->self_parsed = true;
    }
    gl_comprehension_result_cleanup(&self_comp);

    /* Run Wernicke's syntactic parser on the utterance to get a
     * grammaticality + complexity reading independent of the
     * lexicon-side semantics. */
    if (brain->wernicke) {
        syntactic_comprehension_t* syn =
            wernicke_get_syntactic_comprehension(brain->wernicke);
        if (syn) {
            enum { MAX_WORDS = 32 };
            syntactic_word_t words[MAX_WORDS];
            char dup[2048];
            size_t ulen = strlen(state->utterance);
            if (ulen >= sizeof(dup)) ulen = sizeof(dup) - 1;
            memcpy(dup, state->utterance, ulen);
            dup[ulen] = '\0';

            uint32_t num_words = 0;
            char* save = NULL;
            char* tok = strtok_r(dup, " \t\n\r.,!?;:\"'", &save);
            while (tok && num_words < MAX_WORDS) {
                if (!tok[0]) {
                    tok = strtok_r(NULL, " \t\n\r.,!?;:\"'", &save);
                    continue;
                }
                memset(&words[num_words], 0, sizeof(words[num_words]));
                size_t tlen = strlen(tok);
                if (tlen >= sizeof(words[num_words].word))
                    tlen = sizeof(words[num_words].word) - 1;
                for (size_t k = 0; k < tlen; k++) {
                    words[num_words].word[k] = (char)tolower((unsigned char)tok[k]);
                }
                words[num_words].word[tlen] = '\0';
                words[num_words].position = num_words;

                const gl_lexicon_entry_t* e = lexicon_find_internal(
                    brain->grounded_lang, words[num_words].word);
                words[num_words].category = e
                    ? gl_class_to_syn_cat(e->learned_class)
                    : SYN_CAT_UNKNOWN;
                words[num_words].category_confidence = e
                    ? e->class_confidence : 0.0f;

                num_words++;
                tok = strtok_r(NULL, " \t\n\r.,!?;:\"'", &save);
            }

            if (num_words > 0) {
                syntactic_parse_t parse;
                memset(&parse, 0, sizeof(parse));
                if (syntactic_parse_sentence(syn, words, num_words, &parse) == 0) {
                    state->self_complexity = syntactic_compute_complexity(&parse);
                    if (state->self_complexity > 1.0f)
                        state->self_complexity = 1.0f;
                    /* Grammaticality: parse-success + parse's own
                     * is_grammatical flag give us a 3-level signal. */
                    state->self_grammaticality = parse.is_grammatical ? 1.0f : 0.5f;
                    syntactic_parse_free(&parse);
                } else {
                    /* Parse failed — output is ungrammatical or unparseable. */
                    state->self_grammaticality = 0.0f;
                }
            }
        }
    }

    return 0;
}
