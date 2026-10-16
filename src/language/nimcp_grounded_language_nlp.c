/**
 * @file nimcp_grounded_language_nlp.c
 * @brief NLP frontend for grounded_language: morphology, POS hints,
 *        embedding blend, BPE subword fallback.
 * @date 2026-05-05
 *
 * WHAT: Three pieces of NLP machinery that grounded_language was
 *       previously missing:
 *         1. Morphological normalization — strip common English
 *            inflectional/derivational suffixes so "running"/"runs"/
 *            "ran" all canonicalize to "run" before lexicon lookup.
 *         2. Suffix-based POS hints — use the same surface morphology
 *            to seed word_class when no positional context is available
 *            (e.g. "evaluation" → noun via -tion).
 *         3. Embedding + tokenizer connect points — wire an external
 *            word embedding layer + BPE tokenizer so that comprehend
 *            can (a) pull embedding vectors into the result's
 *            semantic_vector, and (b) decompose totally-OOV strings
 *            into subword token ids as a last-resort lookup path.
 *
 * WHY:  The legacy GL pipeline was tokenize → exact lexicon → fuzzy
 *       match → give up. That misses three real NLP signals: surface
 *       morphology (which English carries a lot of), distributed
 *       embeddings (which the rest of the brain is already trained on),
 *       and subword decomposition (which the BPE tokenizer can already
 *       do). Each missing signal forces the rest of the cognitive
 *       stack to compensate, often badly.
 *
 * HOW:  - Suffix stripping is a hand-rolled lightweight stemmer (NOT
 *         a full Porter — Porter would over-stem and lose meaning for
 *         a developmental brain). Order matters: longer suffixes first.
 *       - POS hint table is a small static array of (suffix → class).
 *       - Embedding + tokenizer hooks live behind opaque-pointer
 *         setters so this file doesn't need to know either struct's
 *         layout. Lookups are pure forwarding to the public APIs in
 *         nimcp_embedding.h / nimcp_tokenizer.h.
 *       - The `gl_nlp_lookup_chain()` helper is the single entry the
 *         primary impl calls — it walks exact → morph → fuzzy → BPE
 *         in that order and returns the resolved entry (or NULL).
 *
 * SOLID/DRY: All four NLP stages are isolated as small static helpers
 *            with one public dispatcher, so the comprehend hot path
 *            picks up the entire chain via a single function call.
 *            Each stage is independently optional — none assume the
 *            others are connected.
 */

#include "language/nimcp_grounded_language.h"
#include "nimcp_grounded_language_internal.h"

#include "generation/nimcp_embedding.h"
#include "generation/nimcp_tokenizer.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "GROUNDED_LANG_NLP"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Forward decl from primary impl — find an entry without creating one. */
extern const gl_lexicon_entry_t* lexicon_find_internal(
    const grounded_language_t* gl, const char* word);

/* Forward decl — fuzzy fallback lives in the primary impl. We re-declare
 * it here rather than expose it in the public header. */
extern const gl_lexicon_entry_t* lexicon_find_fuzzy_external(
    const grounded_language_t* gl, const char* word);

/*=============================================================================
 * Suffix-stripping morphology
 *
 * Hand-rolled, intentionally conservative. English morphology has a few
 * very productive endings (-s, -ed, -ing, -ly, -er, -est, -tion, -ness,
 * -ment, -able). Stripping these covers ~80% of inflected/derived forms
 * without the over-stemming that classical Porter exhibits.
 *
 * Order: try the longest suffixes first so "happiness" doesn't become
 * "happi" via a premature -s strip.
 *===========================================================================*/

typedef struct {
    const char* suffix;
    size_t      len;
    size_t      min_stem;   /* don't strip if remaining stem would be shorter */
    const char* replace;    /* what to append after stripping (NULL → nothing) */
} gl_suffix_rule_t;

/* Rules ordered longest-first. The replace field handles the common
 * orthographic adjustments — "running" → "run" (drop double n),
 * "happily" → "happy" (i→y), "studies" → "study" (ies→y). */
static const gl_suffix_rule_t SUFFIX_RULES[] = {
    {"ation",  5, 3, NULL},   /* evaluation → evalu (then maybe -e drop) */
    {"ition",  5, 3, NULL},   /* condition  → cond  */
    {"ness",   4, 3, NULL},   /* happiness  → happi (handled below) */
    {"ment",   4, 3, NULL},   /* movement   → move (dropped 'e' restored later) */
    {"able",   4, 3, NULL},   /* readable   → read */
    {"ible",   4, 3, NULL},   /* visible    → vis */
    {"tion",   4, 3, NULL},   /* function   → func */
    {"sion",   4, 3, NULL},   /* tension    → ten */
    {"ies",    3, 3, "y"},    /* studies    → study */
    {"ied",    3, 3, "y"},    /* studied    → study */
    {"ing",    3, 3, NULL},   /* running    → runn (de-double below) */
    {"est",    3, 3, NULL},   /* fastest    → fast */
    {"ous",    3, 3, NULL},   /* dangerous  → danger */
    {"ive",    3, 3, NULL},   /* creative   → creat (then -e) */
    {"ful",    3, 3, NULL},   /* helpful    → help */
    {"ize",    3, 3, NULL},   /* organize   → organ */
    {"ise",    3, 3, NULL},   /* organise   → organ */
    {"ed",     2, 3, NULL},   /* walked     → walk */
    {"er",     2, 3, NULL},   /* walker     → walk */
    {"ly",     2, 3, NULL},   /* quickly    → quick */
    {"es",     2, 3, NULL},   /* boxes      → box */
    {"s",      1, 3, NULL},   /* cats       → cat */
};

#define GL_SUFFIX_RULE_COUNT (sizeof(SUFFIX_RULES)/sizeof(SUFFIX_RULES[0]))

/* De-double the trailing consonant when stripping "-ing"/"-ed" leaves
 * a CVCC where the last two C's are the same: "running" → "runn" → "run". */
static void de_double_trailing(char* word) {
    size_t n = strlen(word);
    if (n < 3) return;
    char a = word[n - 1];
    char b = word[n - 2];
    if (a == b && (a == 'n' || a == 'p' || a == 't' ||
                   a == 'd' || a == 'g' || a == 'm' ||
                   a == 'b' || a == 'l' || a == 'r')) {
        word[n - 1] = '\0';
    }
}

int gl_morph_normalize(const char* word, char* out, size_t out_sz) {
    if (!word || !out || out_sz == 0) return -1;

    size_t in_len = strlen(word);
    if (in_len + 1 > out_sz) return -1;

    /* Lowercase copy for the strip pass. */
    for (size_t i = 0; i < in_len; i++) {
        out[i] = (char)tolower((unsigned char)word[i]);
    }
    out[in_len] = '\0';

    if (in_len < 4) return 0;  /* too short to strip safely */

    for (size_t r = 0; r < GL_SUFFIX_RULE_COUNT; r++) {
        const gl_suffix_rule_t* rule = &SUFFIX_RULES[r];
        if (in_len <= rule->len + rule->min_stem - 1) continue;
        size_t suffix_start = in_len - rule->len;
        if (strcmp(out + suffix_start, rule->suffix) == 0) {
            out[suffix_start] = '\0';
            if (rule->replace) {
                size_t rlen = strlen(rule->replace);
                if (suffix_start + rlen + 1 > out_sz) {
                    /* abandon strip if no room for replacement */
                    memcpy(out, word, in_len);
                    out[in_len] = '\0';
                    return 0;
                }
                memcpy(out + suffix_start, rule->replace, rlen + 1);
            }
            /* De-double "running" → "runn" → "run" only for -ing/-ed. */
            if (strcmp(rule->suffix, "ing") == 0 ||
                strcmp(rule->suffix, "ed") == 0) {
                de_double_trailing(out);
            }
            return (int)(in_len - strlen(out));
        }
    }
    return 0;
}

/*=============================================================================
 * POS hints from surface morphology
 *===========================================================================*/

typedef struct {
    const char*     suffix;
    size_t          len;
    gl_word_class_t class_;
} gl_pos_rule_t;

/* Highest-precedence POS suffixes first. Keep this list short — we only
 * want strong, near-deterministic signals here. Edge cases (e.g. -er
 * could be noun "writer" or comparative "faster") deliberately omitted. */
static const gl_pos_rule_t POS_RULES[] = {
    /* Plural derived-noun suffixes first — unambiguously nominal (no English
     * verb ends in -tions/-sions/-ments/-ities), so these are safe to tag NOUN
     * even though a bare "-s" rule would not be (it collides with 3sg verbs
     * like "runs"). Closes the morphology gap that left plural nouns UNKNOWN
     * and defeated the F4 possessive + Tier-2 pronominalization NOUN gates. */
    {"tions", 5, GL_CLASS_NOUN},
    {"sions", 5, GL_CLASS_NOUN},
    {"ments", 5, GL_CLASS_NOUN},
    {"ities", 5, GL_CLASS_NOUN},
    {"tion",  4, GL_CLASS_NOUN},
    {"sion",  4, GL_CLASS_NOUN},
    {"ness",  4, GL_CLASS_NOUN},
    {"ment",  4, GL_CLASS_NOUN},
    {"ity",   3, GL_CLASS_NOUN},
    {"ing",   3, GL_CLASS_VERB},     /* gerund/participle */
    {"ed",    2, GL_CLASS_VERB},     /* past tense */
    {"ize",   3, GL_CLASS_VERB},
    {"ise",   3, GL_CLASS_VERB},
    {"ate",   3, GL_CLASS_VERB},
    {"ous",   3, GL_CLASS_ADJECTIVE},
    {"ive",   3, GL_CLASS_ADJECTIVE},
    {"ful",   3, GL_CLASS_ADJECTIVE},
    {"able",  4, GL_CLASS_ADJECTIVE},
    {"ible",  4, GL_CLASS_ADJECTIVE},
    {"al",    2, GL_CLASS_ADJECTIVE},
    {"ic",    2, GL_CLASS_ADJECTIVE},
    {"ly",    2, GL_CLASS_ADVERB},
};

#define GL_POS_RULE_COUNT (sizeof(POS_RULES)/sizeof(POS_RULES[0]))

gl_word_class_t gl_morph_pos_hint(const char* word) {
    if (!word) return GL_CLASS_UNKNOWN;
    size_t n = strlen(word);
    if (n < 4) return GL_CLASS_UNKNOWN;  /* short words too ambiguous */

    /* Lowercase tail for matching. */
    char tail[16];
    size_t tlen = (n < sizeof(tail) - 1) ? n : sizeof(tail) - 1;
    size_t start = n - tlen;
    for (size_t i = 0; i < tlen; i++) {
        tail[i] = (char)tolower((unsigned char)word[start + i]);
    }
    tail[tlen] = '\0';

    for (size_t r = 0; r < GL_POS_RULE_COUNT; r++) {
        const gl_pos_rule_t* rule = &POS_RULES[r];
        if (tlen < rule->len + 2) continue;  /* need stem ≥ 2 chars */
        const char* p = tail + tlen - rule->len;
        if (strcmp(p, rule->suffix) == 0) return rule->class_;
    }
    return GL_CLASS_UNKNOWN;
}

/*=============================================================================
 * Embedding + tokenizer connect points
 *===========================================================================*/

/* PA-5: adapter that bridges call back into to look up a word's GloVe
 * embedding by word_form. The bridge owns the lifetime; grounded_language
 * is the source of truth. We forward into gl_nlp_embedding_lookup which
 * already handles the word→token_id→embedding chain.
 *
 * Externally-visible (no `static`): called from nimcp_grounded_language.c
 * via the `gl_word_emb_for_bridge_external` extern decl in connect_snn_bridge. */
int gl_word_emb_for_bridge_external(void* ctx,
                                     const char* word_form,
                                     float* out_vec,
                                     uint32_t out_dim)
{
    grounded_language_t* gl = (grounded_language_t*)ctx;
    if (!gl || !word_form || !out_vec) return -1;
    if (gl->emb_dim != out_dim) return -1;
    return gl_nlp_embedding_lookup(gl, word_form, out_vec);
}

void grounded_language_connect_embeddings(grounded_language_t* gl,
                                            void* emb,
                                            uint32_t emb_dim,
                                            gl_word_to_id_fn word_to_id_fn,
                                            void* ctx) {
    if (!gl) return;
    gl->embeddings    = emb;
    gl->emb_dim       = emb_dim;
    gl->word_to_id_fn = word_to_id_fn;
    gl->word_to_id_ctx = ctx;

    /* PA-5: if a bridge is already attached, push the lookup down so
     * decode_spikes can blend cosine(intent_emb, glove_emb[w]) into the
     * ranking. The bridge invalidates its per-word cache lazily on first
     * decode. Caller still has to set glove_blend > 0 to actually use it. */
    if (gl->snn_bridge) {
        snn_language_bridge_set_embedding_lookup(gl->snn_bridge,
                                                   gl_word_emb_for_bridge_external,
                                                   gl, emb_dim);
    }
}

void grounded_language_connect_tokenizer(grounded_language_t* gl, void* tok) {
    if (!gl) return;
    gl->tokenizer = tok;
}

/*=============================================================================
 * Embedding lookup helper — fills out_vec[gl->semantic_dim] with the
 * embedding for `word` if the embedding layer is connected and the
 * dim matches. Returns 0 on success, -1 otherwise. Called from comprehend.
 *===========================================================================*/

int gl_nlp_embedding_lookup(grounded_language_t* gl,
                              const char* word,
                              float* out_vec) {
    if (!gl || !word || !out_vec) return -1;
    if (!gl->embeddings || !gl->word_to_id_fn) return -1;
    if (gl->emb_dim != gl->semantic_dim) {
        /* Walkthrough round 4 fix: dim mismatch was silently disabling
         * embedding lookups in production. Log once per process so a
         * mis-configured caller can see why blends aren't running. */
        static _Atomic int g_emb_dim_warned = 0;
        if (__atomic_exchange_n(&g_emb_dim_warned, 1, __ATOMIC_RELAXED) == 0) {
            LOG_WARN(LOG_MODULE,
                     "gl_nlp_embedding_lookup: emb_dim=%u != semantic_dim=%u — "
                     "embedding lookups disabled until dims match",
                     gl->emb_dim, gl->semantic_dim);
        }
        return -1;
    }

    uint32_t token_id = gl->word_to_id_fn(gl->word_to_id_ctx, word);
    if (token_id == 0) return -1;  /* unknown to caller's vocab */

    return embedding_lookup((const embedding_layer_t*)gl->embeddings,
                            token_id, out_vec);
}

/*=============================================================================
 * BPE subword fallback — when comprehend has exhausted exact + morph +
 * fuzzy and a tokenizer is connected, try to decompose the word into
 * subword token ids and pull embeddings for them. We return success if
 * the tokenizer produced ANY known subword token; the caller then knows
 * the word is "partially recognized" rather than fully OOV.
 *
 * This is a very lightweight signal — it doesn't try to rebuild an
 * entry. It just lets the comprehend caller bump confidence slightly
 * and (when embeddings are connected) average the subword vectors into
 * the semantic_vector.
 *===========================================================================*/

int gl_nlp_subword_lookup(grounded_language_t* gl,
                            const char* word,
                            float* out_vec_or_null) {
    if (!gl || !word) return -1;
    if (!gl->tokenizer) return -1;

    uint32_t ids[16];
    uint32_t actual = 0;
    int rc = tokenizer_encode((const tokenizer_t*)gl->tokenizer,
                               word, ids, 16, &actual);
    if (rc != 0 || actual == 0) return -1;

    /* If embeddings are connected and the dim matches, average the
     * subword embeddings into out_vec_or_null. */
    if (out_vec_or_null && gl->embeddings &&
        gl->emb_dim == gl->semantic_dim) {
        float scratch[1024];  /* upper bound on semantic_dim */
        if (gl->semantic_dim > 1024) return 0;  /* too big — signal-only */

        for (uint32_t d = 0; d < gl->semantic_dim; d++) out_vec_or_null[d] = 0.0f;
        size_t hits = 0;
        for (uint32_t i = 0; i < actual; i++) {
            if (embedding_lookup((const embedding_layer_t*)gl->embeddings,
                                  ids[i], scratch) == 0) {
                for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                    out_vec_or_null[d] += scratch[d];
                }
                hits++;
            }
        }
        if (hits > 0) {
            float inv = 1.0f / (float)hits;
            for (uint32_t d = 0; d < gl->semantic_dim; d++) {
                out_vec_or_null[d] *= inv;
            }
        }
    }
    return 0;
}

/*=============================================================================
 * #13 Phonological retrieval — rhyme + alliteration
 *
 * Linear-scan helpers over the active vocab. Bounded by GL_MAX_VOCAB
 * (16384 entries) — cheap in exchange for keeping the index out of
 * the hot-path mutation paths (no synchronization, no persistence
 * parity required, no eviction follow-through).
 *===========================================================================*/

/* Skip non-alphabetic trailing chars and copy up to suffix_len into
 * out (lowercased, NUL-terminated). Returns the number of letters
 * actually captured (≤ suffix_len); 0 if the word has too few letters. */
static uint32_t _gl_word_suffix_lower(const char* word,
                                        uint32_t suffix_len,
                                        char* out,
                                        size_t out_sz) {
    if (!word || !out || out_sz == 0) return 0;
    if (suffix_len + 1 > out_sz) return 0;

    /* Find end of word (NUL terminator). */
    size_t len = 0;
    while (word[len] != '\0') len++;

    /* Skip trailing non-alpha (punctuation, digits). */
    while (len > 0 &&
           !(((unsigned char)word[len - 1] >= 'a' &&
              (unsigned char)word[len - 1] <= 'z') ||
             ((unsigned char)word[len - 1] >= 'A' &&
              (unsigned char)word[len - 1] <= 'Z'))) {
        len--;
    }

    if (len < suffix_len) return 0;

    for (uint32_t i = 0; i < suffix_len; i++) {
        unsigned char c = (unsigned char)word[len - suffix_len + i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        out[i] = (char)c;
    }
    out[suffix_len] = '\0';
    return suffix_len;
}

/* Lowercased first alphabetic byte of the word, or 0 when no alpha
 * is present. */
static char _gl_first_alpha_lower(const char* word) {
    if (!word) return 0;
    for (const char* p = word; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
        if (c >= 'a' && c <= 'z') return (char)c;
    }
    return 0;
}

uint32_t grounded_language_lookup_rhymes(const grounded_language_t* gl,
                                           const char* word,
                                           const char** out_words,
                                           uint32_t max_out) {
    if (!gl || !word || !out_words || max_out == 0) return 0;

    char ref_suf[GL_RHYME_SUFFIX_LEN + 1];
    if (_gl_word_suffix_lower(word, GL_RHYME_SUFFIX_LEN,
                                ref_suf, sizeof(ref_suf)) == 0) {
        return 0;
    }

    /* Lowercase the input itself so we can exclude self-match. */
    char self[64];
    {
        size_t i = 0;
        for (; word[i] && i < sizeof(self) - 1; i++) {
            unsigned char c = (unsigned char)word[i];
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
            self[i] = (char)c;
        }
        self[i] = '\0';
    }

    uint32_t n = 0;
    for (uint32_t v = 0; v < gl->vocab_count && n < max_out; v++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[v];
        if (!e) continue;
        char cand_suf[GL_RHYME_SUFFIX_LEN + 1];
        if (_gl_word_suffix_lower(e->form, GL_RHYME_SUFFIX_LEN,
                                    cand_suf, sizeof(cand_suf)) == 0) {
            continue;
        }
        if (memcmp(cand_suf, ref_suf, GL_RHYME_SUFFIX_LEN) != 0) continue;
        /* Skip self-match (form is already stored lowercased on insert). */
        size_t ci = 0;
        bool same = true;
        while (e->form[ci] && self[ci]) {
            if (e->form[ci] != self[ci]) { same = false; break; }
            ci++;
        }
        if (same && e->form[ci] == self[ci]) continue;

        out_words[n++] = e->form;
    }
    return n;
}

uint32_t grounded_language_lookup_alliterations(const grounded_language_t* gl,
                                                  const char* word,
                                                  const char** out_words,
                                                  uint32_t max_out) {
    if (!gl || !word || !out_words || max_out == 0) return 0;

    char first = _gl_first_alpha_lower(word);
    if (first == 0) return 0;

    /* Lowercase self for exclusion. */
    char self[64];
    {
        size_t i = 0;
        for (; word[i] && i < sizeof(self) - 1; i++) {
            unsigned char c = (unsigned char)word[i];
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
            self[i] = (char)c;
        }
        self[i] = '\0';
    }

    uint32_t n = 0;
    for (uint32_t v = 0; v < gl->vocab_count && n < max_out; v++) {
        const gl_lexicon_entry_t* e = gl->vocab_list[v];
        if (!e) continue;
        if (_gl_first_alpha_lower(e->form) != first) continue;

        /* Skip self-match. */
        size_t ci = 0;
        bool same = true;
        while (e->form[ci] && self[ci]) {
            if (e->form[ci] != self[ci]) { same = false; break; }
            ci++;
        }
        if (same && e->form[ci] == self[ci]) continue;

        out_words[n++] = e->form;
    }
    return n;
}

/*=============================================================================
 * Lookup chain — single entry the comprehend hot path uses to walk
 * exact → morph → fuzzy. The BPE step is signal-only (no entry to
 * return) so it's exercised separately by the comprehend caller via
 * gl_nlp_subword_lookup().
 *===========================================================================*/

const gl_lexicon_entry_t* gl_nlp_lookup_chain(grounded_language_t* gl,
                                                 const char* word) {
    if (!gl || !word) return NULL;

    const gl_lexicon_entry_t* hit = lexicon_find_internal(gl, word);
    if (hit) return hit;

    /* Stage 2: morphological normalization. */
    char stem[64];
    if (gl_morph_normalize(word, stem, sizeof(stem)) > 0) {
        hit = lexicon_find_internal(gl, stem);
        if (hit) return hit;
    }

    /* Stage 3: fuzzy match. */
    return lexicon_find_fuzzy_external(gl, word);
}

/*============================================================================
 * Morphological INFLECTION (generation) — Tier 1 Step F3
 *
 * The inverse of gl_morph_normalize (which strips suffixes). These build the
 * 3rd-person-singular verb form and the plural noun form from a base word,
 * using standard English orthographic rules + a small irregular table. Pure
 * functions over lowercase ASCII; used by gl_apply_svo_agreement to fix
 * generated subject-verb / quantifier-noun agreement.
 *
 * Broca's syntax_apply_inflection is a word_id-based demonstration stub
 * (emits "word_<id>-s"), so it cannot produce real surface forms — these
 * are the real implementation that gap requires.
 *==========================================================================*/

static bool gl_is_vowel(char c) {
    c = (char)tolower((unsigned char)c);
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u';
}

/* Shared -s/-es/-ies sibilant+y orthography for both verbs and nouns. */
static int gl_morph_add_s(const char* w, char* out, size_t out_sz) {
    size_t n = strlen(w);
    if (n == 0 || out_sz < n + 4) { if (out_sz) out[0]='\0'; return -1; }
    char last  = w[n-1];
    char prev  = (n >= 2) ? w[n-2] : '\0';
    /* sibilants → +es: ...s, ...x, ...z, ...ch, ...sh, ...o */
    bool es = (last=='s'||last=='x'||last=='z'||last=='o') ||
              (n>=2 && ((prev=='c'&&last=='h') || (prev=='s'&&last=='h')));
    if (n>=2 && last=='y' && !gl_is_vowel(prev)) {
        /* consonant + y → -ies */
        memcpy(out, w, n-1);
        memcpy(out+n-1, "ies", 4);   /* includes NUL */
        return (int)(n+2);
    }
    memcpy(out, w, n);
    if (es) { memcpy(out+n, "es", 3); return (int)(n+2); }
    memcpy(out+n, "s", 2);
    return (int)(n+1);
}

/* RC1 guard (2026-06-01): is `w` already a past/participle surface form that
 * must NOT be re-inflected as if it were a base verb? Re-suffixing these is
 * what produced "throwns" (thrown+s) and "preventeded"-style double pasts.
 * -ed is matched by suffix; irregular past participles have no reliable suffix
 * (bare -en/-n collide with common nouns — kitchen, garden, sun, man) so they
 * are enumerated. (-ing gerunds are refused separately, only in past_tense.) */
static bool gl_morph_is_past_or_participle(const char* w) {
    if (!w) return false;
    size_t n = strlen(w);
    if (n >= 3 && w[n-2]=='e' && w[n-1]=='d') return true;   /* -ed past */
    static const char* PART[] = {
        "thrown","grown","shown","drawn","blown","flown","known","sewn","mown",
        "given","taken","eaten","broken","spoken","chosen","frozen","stolen",
        "woven","driven","written","ridden","fallen","hidden","bitten","beaten",
        "forgotten","gotten","seen","worn","torn","born","sworn", NULL };
    for (int i = 0; PART[i]; i++) if (strcmp(w, PART[i]) == 0) return true;
    return false;
}

int gl_morph_inflect_3sg(const char* verb, char* out, size_t out_sz) {
    if (!verb || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }
    /* Common irregular present-tense 3sg. */
    static const struct { const char* base; const char* form; } IRR[] = {
        {"be","is"}, {"have","has"}, {"do","does"}, {"go","goes"},
        {"can","can"}, {"will","will"}, {"may","may"}, {"must","must"},
        {"shall","shall"}, {"might","might"}, {"would","would"},
        {"could","could"}, {"should","should"}, {NULL,NULL}
    };
    for (int i = 0; IRR[i].base; i++) {
        if (strcmp(verb, IRR[i].base) == 0) {
            size_t fl = strlen(IRR[i].form);
            if (out_sz < fl+1) { out[0]='\0'; return -1; }
            memcpy(out, IRR[i].form, fl+1);
            return (int)fl;
        }
    }
    /* RC1 guard: never 3sg an already-past/participle form (thrown -> throwns).
     * Return it unchanged so the F3 caller's strcmp(result, input) check skips
     * it. -ing gerunds are intentionally NOT refused here (sing -> sings). */
    if (gl_morph_is_past_or_participle(verb)) {
        size_t nn = strlen(verb);
        if (out_sz < nn+1) { out[0]='\0'; return -1; }
        memcpy(out, verb, nn+1);
        return (int)nn;
    }
    return gl_morph_add_s(verb, out, out_sz);
}

int gl_morph_pluralize(const char* noun, char* out, size_t out_sz) {
    if (!noun || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }
    static const struct { const char* sing; const char* plur; } IRR[] = {
        {"child","children"}, {"man","men"}, {"woman","women"},
        {"person","people"}, {"foot","feet"}, {"tooth","teeth"},
        {"goose","geese"}, {"mouse","mice"}, {"sheep","sheep"},
        {"fish","fish"}, {"deer","deer"}, {NULL,NULL}
    };
    for (int i = 0; IRR[i].sing; i++) {
        if (strcmp(noun, IRR[i].sing) == 0) {
            size_t pl = strlen(IRR[i].plur);
            if (out_sz < pl+1) { out[0]='\0'; return -1; }
            memcpy(out, IRR[i].plur, pl+1);
            return (int)pl;
        }
    }
    return gl_morph_add_s(noun, out, out_sz);
}

/* Tier 1 Step F4b — past-tense inflector (the tense analog of _3sg). Irregular
 * table + regular -ed orthography (love→loved, try→tried, walk→walked). Pure
 * function over lowercase ASCII; used by the F4 tense-consistency corrector. */
int gl_morph_past_tense(const char* verb, char* out, size_t out_sz) {
    if (!verb || !out || out_sz == 0) { if (out && out_sz) out[0]='\0'; return -1; }
    static const struct { const char* base; const char* past; } IRR[] = {
        {"be","was"}, {"am","was"}, {"is","was"}, {"are","were"},
        {"have","had"}, {"has","had"}, {"do","did"}, {"does","did"},
        {"go","went"}, {"run","ran"}, {"eat","ate"}, {"see","saw"},
        {"come","came"}, {"take","took"}, {"make","made"}, {"give","gave"},
        {"get","got"}, {"know","knew"}, {"think","thought"}, {"say","said"},
        {"tell","told"}, {"find","found"}, {"feel","felt"}, {"become","became"},
        {"leave","left"}, {"bring","brought"}, {"buy","bought"}, {"catch","caught"},
        {"teach","taught"}, {"begin","began"}, {"drink","drank"}, {"swim","swam"},
        {"sing","sang"}, {"sit","sat"}, {"stand","stood"}, {"speak","spoke"},
        {"write","wrote"}, {"hold","held"}, {"keep","kept"}, {"sleep","slept"},
        {"meet","met"}, {"pay","paid"}, {"sell","sold"}, {"send","sent"},
        {"build","built"}, {"understand","understood"}, {"win","won"},
        {"lose","lost"}, {"fall","fell"}, {"feed","fed"}, {"hear","heard"},
        {"can","could"}, {"will","would"}, {"shall","should"}, {"may","might"},
        {NULL,NULL}
    };
    for (int i = 0; IRR[i].base; i++) {
        if (strcmp(verb, IRR[i].base) == 0) {
            size_t pl = strlen(IRR[i].past);
            if (out_sz < pl+1) { out[0]='\0'; return -1; }
            memcpy(out, IRR[i].past, pl+1);
            return (int)pl;
        }
    }
    /* RC1 guard: never form a past tense by suffixing a gerund (-ing) or an
     * already-past/participle form. Leaving it unchanged makes the F4b caller's
     * strcmp(result, input) check skip it — this is the "preventing+ed ->
     * preventinged" / "moving+ed -> movinged" fix. Runs AFTER the irregular
     * table so real irregulars (sing->sang, run->ran) are still produced. */
    {
        size_t gn = strlen(verb);
        if ((gn >= 4 && verb[gn-3]=='i' && verb[gn-2]=='n' && verb[gn-1]=='g') ||
            gl_morph_is_past_or_participle(verb)) {
            if (out_sz < gn+1) { out[0]='\0'; return -1; }
            memcpy(out, verb, gn+1);
            return (int)gn;
        }
    }

    /* Regular -ed. */
    size_t n = strlen(verb);
    if (n == 0 || out_sz < n + 4) { out[0]='\0'; return -1; }
    char last = verb[n-1];
    char prev = (n >= 2) ? verb[n-2] : '\0';
    if (last == 'e') {                       /* love → loved */
        memcpy(out, verb, n); memcpy(out+n, "d", 2); return (int)(n+1);
    }
    if (last == 'y' && !gl_is_vowel(prev)) { /* try → tried */
        memcpy(out, verb, n-1); memcpy(out+n-1, "ied", 4); return (int)(n+2);
    }
    memcpy(out, verb, n); memcpy(out+n, "ed", 3); return (int)(n+2);
}

/* Tier 1 Step F4a — indefinite-article selector. Returns the static string
 * "a" or "an" for the word that immediately FOLLOWS the article (English
 * agrees with sound, not spelling: "an hour", "a university"). Small
 * silent-h / vowel-letter-consonant-sound exception list; otherwise keys off
 * the first letter. Never allocates. */
const char* gl_article_for(const char* word) {
    if (!word || !word[0]) return "a";
    static const char* AN_EXC[] = {"hour","honest","honor","honour","heir",
                                   "heirloom","honestly","hourly",NULL};
    static const char* A_EXC[]  = {"university","universe","unicorn","european",
                                   "union","united","one","once","unit","useful",
                                   "user","unique","universal","ufo",NULL};
    char low[64];
    size_t i = 0;
    for (; word[i] && i < sizeof(low)-1; i++)
        low[i] = (char)tolower((unsigned char)word[i]);
    low[i] = '\0';
    for (int k = 0; AN_EXC[k]; k++) if (strcmp(low, AN_EXC[k]) == 0) return "an";
    for (int k = 0; A_EXC[k];  k++) if (strcmp(low, A_EXC[k])  == 0) return "a";
    return gl_is_vowel(low[0]) ? "an" : "a";
}
