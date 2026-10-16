/**
 * @file nimcp_grounded_language_chunker.c
 * @brief Shallow chunker + NER for grounded_language.
 * @date 2026-05-05
 *
 * WHAT: Two layers of structure on top of the per-word lexicon:
 *         1. Named Entity Recognition — classify capitalized /
 *            numeric / acronym tokens as PERSON/PLACE/ORG/NUMBER/
 *            DATE/OTHER without touching the lexicon.
 *         2. Shallow chunker — tokenize + per-word POS classify
 *            (lexicon class → morph hint → NER fallback), then
 *            greedily group tokens into NP/VP/PP/ADJP/ADVP using
 *            regex-style POS patterns. Chinking breaks NPs at
 *            commas, conjunctions, and verbs.
 *
 * WHY:  Without phrase-level structure, the rest of the cognitive
 *       stack reasons over bare words and loses head→modifier
 *       relationships. Chunks let working memory store "the big
 *       red ball" as one unit instead of three; NER lets the KG
 *       distinguish "John" (PERSON) from "ran" (VERB).
 *
 * HOW:  Chunker is a single-pass state machine over the POS
 *       sequence. State enum mirrors the chunk types. Transitions
 *       are pure POS lookups + chink rules.
 *
 * SOLID/DRY: Each chunk type's pattern is a tiny inline matcher,
 *            single state machine drives them all.
 */

#include "language/nimcp_grounded_language.h"
#include "nimcp_grounded_language_internal.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* From primary impl. */
extern const gl_lexicon_entry_t* lexicon_find_internal(
    const grounded_language_t* gl, const char* word);

/*=============================================================================
 * NER
 *===========================================================================*/

static bool _is_all_alpha(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; p++) {
        if (!isalpha((unsigned char)*p)) return false;
    }
    return true;
}

static bool _is_all_digits(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    return true;
}

static bool _is_capitalized(const char* s) {
    if (!s || !*s) return false;
    return isupper((unsigned char)s[0]);
}

static bool _is_all_caps(const char* s) {
    if (!s || strlen(s) < 2) return false;
    int letters = 0;
    for (const char* p = s; *p; p++) {
        if (isalpha((unsigned char)*p)) {
            letters++;
            if (!isupper((unsigned char)*p)) return false;
        }
    }
    return letters >= 2;
}

static bool _has_suffix(const char* s, const char* suffix) {
    size_t n = strlen(s), k = strlen(suffix);
    if (n < k) return false;
    for (size_t i = 0; i < k; i++) {
        if (tolower((unsigned char)s[n - k + i]) !=
            tolower((unsigned char)suffix[i])) return false;
    }
    return true;
}

gl_entity_type_t gl_ner_classify(const char* word,
                                   const char* prev_word_or_null,
                                   bool is_sentence_start) {
    if (!word || !*word) return GL_ENTITY_NONE;

    /* Numeric-form gates first (caps don't apply). */
    if (_is_all_digits(word)) {
        size_t n = strlen(word);
        /* 4-digit token in plausible year range → DATE. */
        if (n == 4) {
            int yr = atoi(word);
            if (yr >= 1000 && yr <= 9999) return GL_ENTITY_DATE;
        }
        return GL_ENTITY_NUMBER;
    }
    /* Date forms with slashes: 12/31/2025. */
    if (strchr(word, '/') || strchr(word, '-')) {
        bool digity = true;
        for (const char* p = word; *p; p++) {
            if (!isdigit((unsigned char)*p) && *p != '/' && *p != '-') {
                digity = false; break;
            }
        }
        if (digity) return GL_ENTITY_DATE;
    }

    /* All-caps acronym (≥2 letters) → ORG. */
    if (_is_all_caps(word)) return GL_ENTITY_ORG;

    /* Not capitalized → not an entity. */
    if (!_is_capitalized(word) || !_is_all_alpha(word)) return GL_ENTITY_NONE;

    /* Sentence-initial caps don't imply entity. The previous word being
     * sentence-final punctuation also counts as start. */
    if (is_sentence_start) return GL_ENTITY_NONE;
    if (prev_word_or_null) {
        size_t pn = strlen(prev_word_or_null);
        if (pn > 0) {
            char last = prev_word_or_null[pn - 1];
            if (last == '.' || last == '!' || last == '?') {
                return GL_ENTITY_NONE;
            }
        }
    }

    /* Place-name suffix heuristics. */
    if (_has_suffix(word, "ville") || _has_suffix(word, "burg") ||
        _has_suffix(word, "ton")   || _has_suffix(word, "ford") ||
        _has_suffix(word, "land")  || _has_suffix(word, "shire")) {
        return GL_ENTITY_PLACE;
    }

    /* Org suffix (Inc, Corp, LLC, Ltd) — only catch if the suffix is
     * separated by space (caller already split). */
    if (_has_suffix(word, "Corp")  || _has_suffix(word, "Inc") ||
        _has_suffix(word, "LLC")   || _has_suffix(word, "Ltd")) {
        return GL_ENTITY_ORG;
    }

    /* Default for capitalized mid-sentence alpha = PERSON. The chunker
     * will re-tag if the surrounding context is more place-like. */
    return GL_ENTITY_PERSON;
}

/*=============================================================================
 * Tokenization for the chunker
 *
 * The primary impl has its own tokenize_text but it lowercases as it
 * splits, which kills NER (caps are the signal). We need a caps-preserving
 * tokenizer here. Splits on whitespace + strips trailing punctuation.
 *===========================================================================*/

#define _GL_CHUNK_MAX_WORDS 64

static bool _is_terminal_punct(char c) {
    return c == ',' || c == ';' || c == ':' ||
           c == '!' || c == '?' || c == '.';
}

static uint32_t _tokenize_preserve_case(const char* text,
                                          char words[_GL_CHUNK_MAX_WORDS][32],
                                          bool sentence_starts[_GL_CHUNK_MAX_WORDS]) {
    if (!text) return 0;
    uint32_t count = 0;
    const char* p = text;
    bool next_is_start = true;

    while (*p && count < _GL_CHUNK_MAX_WORDS) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char* start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = (size_t)(p - start);
        if (len == 0) continue;

        /* Find trailing punctuation suffix. */
        size_t word_end = len;
        while (word_end > 0 && _is_terminal_punct(start[word_end - 1])) {
            word_end--;
        }

        /* Emit the alphanumeric prefix (if any). */
        if (word_end > 0) {
            size_t take = word_end > 31 ? 31 : word_end;
            memcpy(words[count], start, take);
            words[count][take] = '\0';
            sentence_starts[count] = next_is_start;
            next_is_start = false;
            count++;
            if (count >= _GL_CHUNK_MAX_WORDS) break;
        }
        /* Emit each trailing punctuation character as its own token. */
        for (size_t k = word_end; k < len; k++) {
            if (count >= _GL_CHUNK_MAX_WORDS) break;
            words[count][0] = start[k];
            words[count][1] = '\0';
            sentence_starts[count] = false;
            char c = start[k];
            count++;
            next_is_start = (c == '.' || c == '!' || c == '?');
        }
    }
    return count;
}

/*=============================================================================
 * Per-word POS classification used by the chunker.
 * Lookup priority: lexicon class → morph hint → NER → UNKNOWN.
 * NER tags collapse to "noun-ish" for chunk pattern matching.
 *===========================================================================*/

static gl_word_class_t _classify(const grounded_language_t* gl,
                                   const char* word,
                                   gl_entity_type_t ent) {
    if (ent != GL_ENTITY_NONE) return GL_CLASS_NOUN;  /* entities act as nouns */

    /* Lowercase view for lexicon lookup. */
    char lower[32];
    size_t i = 0;
    for (; word[i] && i < sizeof(lower) - 1; i++) {
        lower[i] = (char)tolower((unsigned char)word[i]);
    }
    lower[i] = '\0';

    const gl_lexicon_entry_t* e = lexicon_find_internal(gl, lower);
    if (e && e->learned_class != GL_CLASS_UNKNOWN) return e->learned_class;

    gl_word_class_t hint = gl_morph_pos_hint(lower);
    if (hint != GL_CLASS_UNKNOWN) return hint;

    /* Leave unclassified words as UNKNOWN. The chunker's NP pattern
     * uses _is_noun_like() which treats alphabetic UNKNOWN tokens as
     * noun-equivalents *only* inside NP runs — keeps verbs out of NPs. */
    return GL_CLASS_UNKNOWN;
}

/* True for tokens that can fill a noun slot: real NOUN, any entity,
 * or an alphabetic UNKNOWN word (default-noun-only-inside-NP). */
static bool _is_noun_like(const char* word,
                            gl_word_class_t pos,
                            gl_entity_type_t ent) {
    if (pos == GL_CLASS_NOUN) return true;
    if (ent != GL_ENTITY_NONE) return true;
    if (pos != GL_CLASS_UNKNOWN) return false;
    /* UNKNOWN: accept iff alphabetic length ≥ 2 and lowercase
     * (capitalized would have been classified as ENTITY upstream). */
    if (!word || strlen(word) < 2) return false;
    for (const char* p = word; *p; p++) {
        if (!isalpha((unsigned char)*p)) return false;
    }
    return true;
}

static bool _is_chink_punct(const char* word) {
    if (!word || !word[0]) return false;
    if (word[1] != '\0') return false;
    char c = word[0];
    return c == ',' || c == ';' || c == ':';
}

/* Conjunctions break NP runs (chinking rule). */
static bool _is_conjunction(const char* word) {
    if (!word) return false;
    return strcasecmp(word, "and") == 0 ||
           strcasecmp(word, "or")  == 0 ||
           strcasecmp(word, "but") == 0 ||
           strcasecmp(word, "nor") == 0;
}

static bool _is_preposition(const char* word) {
    if (!word) return false;
    static const char* PREPS[] = {
        "in", "on", "at", "to", "for", "with", "by", "from",
        "of", "into", "onto", "about", "after", "before", "over",
        "under", "through", NULL
    };
    for (int i = 0; PREPS[i]; i++) {
        if (strcasecmp(word, PREPS[i]) == 0) return true;
    }
    return false;
}

static bool _is_determiner(const char* word) {
    if (!word) return false;
    static const char* DETS[] = {
        "the", "a", "an", "this", "that", "these", "those",
        "my", "your", "his", "her", "its", "our", "their", NULL
    };
    for (int i = 0; DETS[i]; i++) {
        if (strcasecmp(word, DETS[i]) == 0) return true;
    }
    return false;
}

static bool _is_aux(const char* word) {
    if (!word) return false;
    static const char* AUX[] = {
        "is", "are", "was", "were", "be", "been", "being",
        "have", "has", "had", "do", "does", "did",
        "will", "would", "could", "should", "can", "may", "might", NULL
    };
    for (int i = 0; AUX[i]; i++) {
        if (strcasecmp(word, AUX[i]) == 0) return true;
    }
    return false;
}

/*=============================================================================
 * Chunker — single state machine over the POS sequence.
 *===========================================================================*/

int grounded_language_chunk(grounded_language_t* gl,
                              const char* text,
                              gl_chunk_t* chunks_out,
                              uint32_t max_chunks,
                              uint32_t* chunk_count_out) {
    if (!text || !chunks_out || !chunk_count_out) return -1;
    *chunk_count_out = 0;

    char tokens[_GL_CHUNK_MAX_WORDS][32];
    bool starts[_GL_CHUNK_MAX_WORDS];
    uint32_t n = _tokenize_preserve_case(text, tokens, starts);
    if (n == 0) return 0;

    /* Pre-classify per-word. */
    gl_word_class_t pos[_GL_CHUNK_MAX_WORDS];
    gl_entity_type_t ent[_GL_CHUNK_MAX_WORDS];
    for (uint32_t i = 0; i < n; i++) {
        const char* prev = (i > 0) ? tokens[i - 1] : NULL;
        ent[i] = gl_ner_classify(tokens[i], prev, starts[i]);
        pos[i] = _classify(gl, tokens[i], ent[i]);
    }

    uint32_t out = 0;
    uint32_t i = 0;
    while (i < n && out < max_chunks) {
        const char* w = tokens[i];

        /* Skip standalone punctuation / chink markers — they don't
         * start a chunk on their own. Also skip conjunctions that
         * surface here (they live between chunks). */
        if (_is_chink_punct(w) || _is_conjunction(w)) { i++; continue; }

        gl_chunk_t* c = &chunks_out[out];
        memset(c, 0, sizeof(*c));
        c->start_word = i;

        /* PP: prep + NP run. */
        if (_is_preposition(w)) {
            c->type = GL_CHUNK_PP;
            uint32_t j = i + 1;
            /* allow optional determiner + adjectives + nouns/entities */
            if (j < n && _is_determiner(tokens[j])) j++;
            while (j < n && pos[j] == GL_CLASS_ADJECTIVE) j++;
            while (j < n && _is_noun_like(tokens[j], pos[j], ent[j])) j++;
            c->end_word = j;
            if (j > i + 1) {
                strncpy(c->head_word, tokens[j - 1], sizeof(c->head_word) - 1);
                c->head_entity = ent[j - 1];
            } else {
                strncpy(c->head_word, w, sizeof(c->head_word) - 1);
            }
            i = j > i ? j : i + 1;
            out++;
            continue;
        }

        /* VP: optional aux + verb + adverbs. */
        if (_is_aux(w) || pos[i] == GL_CLASS_VERB) {
            c->type = GL_CHUNK_VP;
            uint32_t j = i;
            if (_is_aux(tokens[j]) && j + 1 < n &&
                pos[j + 1] == GL_CLASS_VERB) j++;
            if (pos[j] == GL_CLASS_VERB || _is_aux(tokens[j])) j++;
            while (j < n && pos[j] == GL_CLASS_ADVERB) j++;
            c->end_word = j;
            strncpy(c->head_word, tokens[i], sizeof(c->head_word) - 1);
            i = j > i ? j : i + 1;
            out++;
            continue;
        }

        /* ADJP / ADVP: standalone runs. */
        if (pos[i] == GL_CLASS_ADVERB) {
            c->type = GL_CHUNK_ADVP;
            uint32_t j = i + 1;
            while (j < n && pos[j] == GL_CLASS_ADVERB) j++;
            c->end_word = j;
            strncpy(c->head_word, tokens[i], sizeof(c->head_word) - 1);
            i = j;
            out++;
            continue;
        }
        if (pos[i] == GL_CLASS_ADJECTIVE) {
            /* Could be the start of a bare ADJP or of an NP. Look ahead. */
            uint32_t j = i;
            while (j < n && pos[j] == GL_CLASS_ADJECTIVE) j++;
            if (j < n && _is_noun_like(tokens[j], pos[j], ent[j])) {
                /* Roll into NP. Fall through to NP handler below. */
            } else {
                c->type = GL_CHUNK_ADJP;
                c->end_word = j;
                strncpy(c->head_word, tokens[i], sizeof(c->head_word) - 1);
                i = j;
                out++;
                continue;
            }
        }

        /* NP: (DT|PRON)? ADJ* (NOUN|ENTITY|UNKNOWN-alpha)+ — with
         * chinking on commas and conjunctions.
         *
         * Entry rule: an NP may *only* start with DT, PRON, ADJ, NOUN,
         * or ENTITY. A bare UNKNOWN token doesn't kick one off (else
         * the irregular-verb "met" / "ran" silently get swallowed as
         * if they were nouns). */
        bool can_start_np =
            _is_determiner(tokens[i]) ||
            pos[i] == GL_CLASS_PRONOUN ||
            pos[i] == GL_CLASS_ADJECTIVE ||
            pos[i] == GL_CLASS_NOUN ||
            ent[i] != GL_ENTITY_NONE;
        if (can_start_np) {
            uint32_t j = i;
            bool consumed_anything = false;
            /* Pronoun: complete NP by itself — don't run on into the
             * following noun loop. "I met John" → NP(I) + ... rather
             * than NP(I met John). */
            if (pos[j] == GL_CLASS_PRONOUN) {
                c->type = GL_CHUNK_NP;
                c->end_word = j + 1;
                strncpy(c->head_word, tokens[j], sizeof(c->head_word) - 1);
                c->head_entity = ent[j];
                i = j + 1;
                out++;
                continue;
            }
            if (_is_determiner(tokens[j])) { j++; consumed_anything = true; }
            while (j < n && pos[j] == GL_CLASS_ADJECTIVE) {
                j++; consumed_anything = true;
            }
            uint32_t noun_start = j;
            bool seen_entity = false;
            while (j < n && _is_noun_like(tokens[j], pos[j], ent[j])) {
                if (ent[j] != GL_ENTITY_NONE) seen_entity = true;
                j++; consumed_anything = true;
                /* Chink: stop if the next token is a comma/conjunction. */
                if (j < n && (_is_chink_punct(tokens[j]) ||
                              _is_conjunction(tokens[j]))) {
                    c->chinked = true;
                    break;
                }
                /* Don't run on past an entity into an UNKNOWN: an entity
                 * is usually the head of its NP. */
                if (seen_entity && j < n &&
                    pos[j] == GL_CLASS_UNKNOWN &&
                    ent[j] == GL_ENTITY_NONE) {
                    break;
                }
            }
            if (consumed_anything && j > i && j > noun_start) {
                c->type = GL_CHUNK_NP;
                c->end_word = j;
                strncpy(c->head_word, tokens[j - 1], sizeof(c->head_word) - 1);
                c->head_entity = ent[j - 1];
                i = j;
                out++;
                continue;
            }
        }

        /* No pattern matched — advance one token. */
        i++;
    }

    *chunk_count_out = out;
    return 0;
}
