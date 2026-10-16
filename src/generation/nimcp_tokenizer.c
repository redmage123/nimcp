/**
 * @file nimcp_tokenizer.c
 * @brief Word-level tokenizer with BPE subword fallback — full implementation
 *
 * WHAT: Tokenizer mapping text <-> integer token IDs with BPE subword handling
 * WHY:  All language generation requires tokenization before embedding lookup
 * HOW:  Hash table for O(1) token-to-id, BPE merge rules for OOV segmentation
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include "generation/nimcp_tokenizer.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*=============================================================================
 * Internal Structure Definition
 *===========================================================================*/

/* Frequency cache: small direct-mapped cache for hot token lookups */
#define TOKEN_FREQ_CACHE_SIZE 256
#define TOKEN_FREQ_CACHE_TOKEN_LEN 32

typedef struct {
    char token[TOKEN_FREQ_CACHE_TOKEN_LEN];
    uint32_t id;
    bool valid;
} token_freq_cache_entry_t;

struct tokenizer {
    char**   id_to_token;       /**< Array: id -> heap-allocated token string */
    hash_table_t* token_to_id;  /**< String -> uint32_t ID lookup */
    uint32_t vocab_size;        /**< Current number of tokens */
    uint32_t vocab_capacity;    /**< Allocated capacity of id_to_token array */
    uint32_t max_token_length;  /**< Maximum allowed token string length */

    /* Special token IDs (always 0-3) */
    uint32_t unk_id;
    uint32_t bos_id;
    uint32_t eos_id;
    uint32_t pad_id;

    /* BPE merge rules — applied in order during encoding */
    char**   merge_rules;       /**< Array of "tokenA tokenB" strings */
    uint32_t num_merges;
    uint32_t merge_capacity;

    /* Frequency cache for fast token-to-id lookups */
    token_freq_cache_entry_t freq_cache[TOKEN_FREQ_CACHE_SIZE];
};

/**
 * WHAT: Compute a simple hash index into the frequency cache
 * WHY:  Fast O(1) cache lookup using first few characters of the token
 * HOW:  FNV-1a-like hash of first few chars, masked to cache size
 */
static inline uint32_t freq_cache_index(const char* token)
{
    uint32_t h = 2166136261u;
    for (int i = 0; token[i] && i < 8; i++) {
        h ^= (uint32_t)(unsigned char)token[i];
        h *= 16777619u;
    }
    return h & (TOKEN_FREQ_CACHE_SIZE - 1);
}

/**
 * WHAT: Look up a token in the frequency cache
 * WHY:  Avoid hash table lookup for frequently accessed tokens
 * HOW:  Direct-mapped cache indexed by simple hash of token string
 *
 * @return Token ID if cache hit, TOKENIZER_INVALID_ID on miss
 */
static inline uint32_t freq_cache_lookup(const token_freq_cache_entry_t* cache, const char* token)
{
    uint32_t idx = freq_cache_index(token);
    const token_freq_cache_entry_t* entry = &cache[idx];
    if (entry->valid && strncmp(entry->token, token, TOKEN_FREQ_CACHE_TOKEN_LEN - 1) == 0) {
        return entry->id;
    }
    return TOKENIZER_INVALID_ID;
}

/**
 * WHAT: Store a token-to-id mapping in the frequency cache
 */
static inline void freq_cache_store(token_freq_cache_entry_t* cache, const char* token, uint32_t id)
{
    if (strlen(token) >= TOKEN_FREQ_CACHE_TOKEN_LEN) return; /* Too long to cache */
    uint32_t idx = freq_cache_index(token);
    token_freq_cache_entry_t* entry = &cache[idx];
    strncpy(entry->token, token, TOKEN_FREQ_CACHE_TOKEN_LEN - 1);
    entry->token[TOKEN_FREQ_CACHE_TOKEN_LEN - 1] = '\0';
    entry->id = id;
    entry->valid = true;
}

/**
 * WHAT: Invalidate the entire frequency cache
 * WHY:  Must be called when vocabulary changes (add/remove tokens, reset)
 */
static inline void freq_cache_invalidate(token_freq_cache_entry_t* cache)
{
    memset(cache, 0, TOKEN_FREQ_CACHE_SIZE * sizeof(token_freq_cache_entry_t));
}

/*=============================================================================
 * Internal Helpers — String Utilities
 *===========================================================================*/

/**
 * WHAT: Lowercase a string in-place
 * WHY:  Tokenizer normalizes input to lowercase
 */
static void str_to_lower(char* s)
{
    if (!s) return;
    for (; *s; s++) {
        *s = (char)tolower((unsigned char)*s);
    }
}

/**
 * WHAT: Check if a character is punctuation that should be its own token
 * WHY:  Punctuation is split out as separate tokens during encoding
 */
static bool is_punct_separator(char c)
{
    return (c == '.' || c == ',' || c == '!' || c == '?' || c == ';' ||
            c == ':' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '"' || c == '\'' || c == '-' ||
            c == '/' || c == '\\' || c == '@' || c == '#' || c == '$' ||
            c == '%' || c == '&' || c == '*' || c == '+' || c == '=' ||
            c == '<' || c == '>' || c == '~' || c == '`' || c == '^');
}

/*=============================================================================
 * Internal Helpers — Vocabulary Management
 *===========================================================================*/

/**
 * WHAT: Grow the id_to_token array if at capacity
 * WHY:  Dynamic vocabulary growth
 * HOW:  Double capacity via nimcp_realloc
 *
 * @return 0 on success, -1 on alloc failure
 */
static int ensure_vocab_capacity(tokenizer_t* tok)
{
    if (tok->vocab_size < tok->vocab_capacity) return 0;

    uint32_t new_cap = tok->vocab_capacity * 2;
    if (new_cap < 64) new_cap = 64;

    char** new_arr = (char**)nimcp_realloc(tok->id_to_token, new_cap * sizeof(char*));
    if (!new_arr) {
        LOG_ERROR("tokenizer: failed to grow vocab array to %u", new_cap);
        return -1;
    }
    /* Zero new slots */
    memset(new_arr + tok->vocab_capacity, 0, (new_cap - tok->vocab_capacity) * sizeof(char*));
    tok->id_to_token = new_arr;
    tok->vocab_capacity = new_cap;
    return 0;
}

/**
 * WHAT: Grow the merge_rules array if at capacity
 */
static int ensure_merge_capacity(tokenizer_t* tok)
{
    if (tok->num_merges < tok->merge_capacity) return 0;

    uint32_t new_cap = tok->merge_capacity * 2;
    if (new_cap < 64) new_cap = 64;

    char** new_arr = (char**)nimcp_realloc(tok->merge_rules, new_cap * sizeof(char*));
    if (!new_arr) {
        LOG_ERROR("tokenizer: failed to grow merge_rules to %u", new_cap);
        return -1;
    }
    memset(new_arr + tok->merge_capacity, 0, (new_cap - tok->merge_capacity) * sizeof(char*));
    tok->merge_rules = new_arr;
    tok->merge_capacity = new_cap;
    return 0;
}

/**
 * WHAT: Internal add-token that returns the ID (no duplicate check for internal use)
 * WHY:  BPE build needs to add tokens and get their IDs
 */
static int internal_add_token(tokenizer_t* tok, const char* token)
{
    if (!tok || !token) return -1;

    /* Check if token already exists */
    void* existing = hash_table_lookup_string(tok->token_to_id, token);
    if (existing) {
        return (int)(*(uint32_t*)existing);
    }

    if (ensure_vocab_capacity(tok) != 0) return -1;

    uint32_t id = tok->vocab_size;
    tok->id_to_token[id] = nimcp_strdup(token);
    if (!tok->id_to_token[id]) {
        LOG_ERROR("tokenizer: strdup failed for token '%s'", token);
        return -1;
    }

    /* Insert id into hash table (store uint32_t value) */
    if (!hash_table_insert_string(tok->token_to_id, token, &id, sizeof(uint32_t))) {
        nimcp_free(tok->id_to_token[id]);
        tok->id_to_token[id] = NULL;
        LOG_ERROR("tokenizer: hash table insert failed for '%s'", token);
        return -1;
    }

    tok->vocab_size++;

    /* Invalidate frequency cache since vocabulary changed */
    freq_cache_invalidate(tok->freq_cache);

    return (int)id;
}

/*=============================================================================
 * Internal Helpers — BPE Word Representation
 *===========================================================================*/

/**
 * WHAT: A BPE word is an array of subtoken strings
 * WHY:  BPE operates by merging adjacent subtokens in a word
 */
typedef struct {
    char** pieces;       /**< Array of subtoken strings (heap-allocated) */
    uint32_t num_pieces;
    uint32_t capacity;
} bpe_word_t;

static bpe_word_t* bpe_word_create(uint32_t initial_cap)
{
    bpe_word_t* w = (bpe_word_t*)nimcp_calloc(1, sizeof(bpe_word_t));
    if (!w) return NULL;
    w->capacity = initial_cap > 0 ? initial_cap : 16;
    w->pieces = (char**)nimcp_calloc(w->capacity, sizeof(char*));
    if (!w->pieces) {
        nimcp_free(w);
        return NULL;
    }
    return w;
}

static void bpe_word_destroy(bpe_word_t* w)
{
    if (!w) return;
    for (uint32_t i = 0; i < w->num_pieces; i++) {
        nimcp_free(w->pieces[i]);
    }
    nimcp_free(w->pieces);
    nimcp_free(w);
}

static int bpe_word_append(bpe_word_t* w, const char* piece)
{
    if (w->num_pieces >= w->capacity) {
        uint32_t new_cap = w->capacity * 2;
        char** new_arr = (char**)nimcp_realloc(w->pieces, new_cap * sizeof(char*));
        if (!new_arr) return -1;
        memset(new_arr + w->capacity, 0, (new_cap - w->capacity) * sizeof(char*));
        w->pieces = new_arr;
        w->capacity = new_cap;
    }
    w->pieces[w->num_pieces] = nimcp_strdup(piece);
    if (!w->pieces[w->num_pieces]) return -1;
    w->num_pieces++;
    return 0;
}

/**
 * WHAT: Apply a single merge to a BPE word
 * WHY:  Core BPE operation — merge adjacent (a, b) -> "ab" everywhere in word
 * HOW:  Scan for adjacent pair, replace with concatenation, shift remaining
 *
 * @return Number of merges applied (0 if pair not found)
 */
static int bpe_word_apply_merge(bpe_word_t* w, const char* a, const char* b)
{
    if (!w || w->num_pieces < 2) return 0;

    int merges_applied = 0;
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char* merged = (char*)nimcp_malloc(len_a + len_b + 1);
    if (!merged) return 0;
    memcpy(merged, a, len_a);
    memcpy(merged + len_a, b, len_b);
    merged[len_a + len_b] = '\0';

    uint32_t i = 0;
    while (i + 1 < w->num_pieces) {
        if (strcmp(w->pieces[i], a) == 0 && strcmp(w->pieces[i + 1], b) == 0) {
            /* Replace pieces[i] with merged, remove pieces[i+1] */
            nimcp_free(w->pieces[i]);
            w->pieces[i] = nimcp_strdup(merged);
            nimcp_free(w->pieces[i + 1]);

            /* Shift remaining pieces left */
            for (uint32_t j = i + 1; j + 1 < w->num_pieces; j++) {
                w->pieces[j] = w->pieces[j + 1];
            }
            w->num_pieces--;
            w->pieces[w->num_pieces] = NULL;
            merges_applied++;
            /* Don't increment i — check for consecutive merges at same position */
        } else {
            i++;
        }
    }

    nimcp_free(merged);
    return merges_applied;
}

/*=============================================================================
 * Internal Helpers — BPE Pair Counting
 *===========================================================================*/

/**
 * WHAT: A pair frequency entry for BPE training
 */
typedef struct pair_entry {
    char* a;
    char* b;
    uint32_t count;
    struct pair_entry* next;
} pair_entry_t;

typedef struct {
    pair_entry_t** buckets;
    uint32_t num_buckets;
    uint32_t num_entries;
} pair_table_t;

static uint32_t pair_hash(const char* a, const char* b, uint32_t num_buckets)
{
    /* FNV-1a on concatenation of a and b */
    uint32_t hash = 2166136261u;
    for (const char* p = a; *p; p++) {
        hash ^= (uint32_t)(unsigned char)*p;
        hash *= 16777619u;
    }
    hash ^= 0xFF; /* separator */
    hash *= 16777619u;
    for (const char* p = b; *p; p++) {
        hash ^= (uint32_t)(unsigned char)*p;
        hash *= 16777619u;
    }
    return hash % num_buckets;
}

static pair_table_t* pair_table_create(uint32_t num_buckets)
{
    pair_table_t* t = (pair_table_t*)nimcp_calloc(1, sizeof(pair_table_t));
    if (!t) return NULL;
    t->num_buckets = num_buckets;
    t->buckets = (pair_entry_t**)nimcp_calloc(num_buckets, sizeof(pair_entry_t*));
    if (!t->buckets) {
        nimcp_free(t);
        return NULL;
    }
    return t;
}

static void pair_table_destroy(pair_table_t* t)
{
    if (!t) return;
    for (uint32_t i = 0; i < t->num_buckets; i++) {
        pair_entry_t* e = t->buckets[i];
        while (e) {
            pair_entry_t* next = e->next;
            nimcp_free(e->a);
            nimcp_free(e->b);
            nimcp_free(e);
            e = next;
        }
    }
    nimcp_free(t->buckets);
    nimcp_free(t);
}

static void pair_table_increment(pair_table_t* t, const char* a, const char* b)
{
    uint32_t idx = pair_hash(a, b, t->num_buckets);
    pair_entry_t* e = t->buckets[idx];
    while (e) {
        if (strcmp(e->a, a) == 0 && strcmp(e->b, b) == 0) {
            e->count++;
            return;
        }
        e = e->next;
    }
    /* New entry */
    pair_entry_t* new_e = (pair_entry_t*)nimcp_calloc(1, sizeof(pair_entry_t));
    if (!new_e) return;
    new_e->a = nimcp_strdup(a);
    new_e->b = nimcp_strdup(b);
    if (!new_e->a || !new_e->b) {
        nimcp_free(new_e->a);
        nimcp_free(new_e->b);
        nimcp_free(new_e);
        return;
    }
    new_e->count = 1;
    new_e->next = t->buckets[idx];
    t->buckets[idx] = new_e;
    t->num_entries++;
}

/**
 * WHAT: Find the most frequent pair in the table
 * WHY:  BPE training merges the most frequent adjacent pair each iteration
 *
 * @param best_a  [OUT] First token of best pair
 * @param best_b  [OUT] Second token of best pair
 * @return Frequency of best pair, or 0 if table is empty
 */
static uint32_t pair_table_find_best(const pair_table_t* t, char** best_a, char** best_b)
{
    uint32_t best_count = 0;
    *best_a = NULL;
    *best_b = NULL;

    for (uint32_t i = 0; i < t->num_buckets; i++) {
        const pair_entry_t* e = t->buckets[i];
        while (e) {
            if (e->count > best_count) {
                best_count = e->count;
                *best_a = e->a;
                *best_b = e->b;
            }
            e = e->next;
        }
    }
    return best_count;
}

/*=============================================================================
 * Internal Helpers — Text Splitting
 *===========================================================================*/

/**
 * WHAT: Split text into word tokens (lowercased), separating punctuation
 * WHY:  First step of tokenizer_encode
 * HOW:  Walk chars; accumulate alpha/digit runs as words, emit punctuation
 *        individually, skip whitespace
 *
 * @param text        Input text
 * @param words       [OUT] Array of heap-allocated word strings
 * @param max_words   Capacity of words array
 * @param num_words   [OUT] Number of words written
 * @return 0 on success, -1 on error
 */
static int split_text_to_words(const char* text, char** words, uint32_t max_words, uint32_t* num_words)
{
    *num_words = 0;

    /* Make a lowercase working copy */
    size_t text_len = strlen(text);
    char* lower = (char*)nimcp_malloc(text_len + 1);
    if (!lower) return -1;
    memcpy(lower, text, text_len + 1);
    str_to_lower(lower);

    const char* p = lower;
    char buf[512];
    uint32_t buf_len = 0;

    while (*p) {
        if (isspace((unsigned char)*p)) {
            /* Flush current word */
            if (buf_len > 0 && *num_words < max_words) {
                buf[buf_len] = '\0';
                words[*num_words] = nimcp_strdup(buf);
                if (!words[*num_words]) { nimcp_free(lower); return -1; }
                (*num_words)++;
                buf_len = 0;
            }
            p++;
        } else if (is_punct_separator(*p)) {
            /* Flush current word first */
            if (buf_len > 0 && *num_words < max_words) {
                buf[buf_len] = '\0';
                words[*num_words] = nimcp_strdup(buf);
                if (!words[*num_words]) { nimcp_free(lower); return -1; }
                (*num_words)++;
                buf_len = 0;
            }
            /* Emit punctuation as its own token */
            if (*num_words < max_words) {
                char punct_buf[2] = { *p, '\0' };
                words[*num_words] = nimcp_strdup(punct_buf);
                if (!words[*num_words]) { nimcp_free(lower); return -1; }
                (*num_words)++;
            }
            p++;
        } else {
            /* Accumulate into word buffer */
            if (buf_len < sizeof(buf) - 1) {
                buf[buf_len++] = *p;
            }
            p++;
        }
    }

    /* Flush trailing word */
    if (buf_len > 0 && *num_words < max_words) {
        buf[buf_len] = '\0';
        words[*num_words] = nimcp_strdup(buf);
        if (!words[*num_words]) { nimcp_free(lower); return -1; }
        (*num_words)++;
    }

    nimcp_free(lower);
    return 0;
}

/**
 * WHAT: Split a word string into individual UTF-8 characters as bpe_word pieces
 * WHY:  BPE starts from character-level representation
 */
static bpe_word_t* word_to_chars(const char* word)
{
    if (!word) return NULL;
    bpe_word_t* w = bpe_word_create((uint32_t)strlen(word) + 1);
    if (!w) return NULL;

    const char* p = word;
    while (*p) {
        /* Handle ASCII characters one at a time */
        /* For simplicity, treat each byte as a character (works for ASCII + Latin-1) */
        char ch[2] = { *p, '\0' };
        if (bpe_word_append(w, ch) != 0) {
            bpe_word_destroy(w);
            return NULL;
        }
        p++;
    }
    return w;
}

/*=============================================================================
 * Public API — Configuration
 *===========================================================================*/

tokenizer_config_t tokenizer_default_config(void)
{
    tokenizer_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.initial_vocab_capacity = TOKENIZER_DEFAULT_VOCAB_CAPACITY;
    cfg.max_token_length = TOKENIZER_DEFAULT_MAX_TOKEN_LEN;
    cfg.unk_token = "<UNK>";
    cfg.bos_token = "<BOS>";
    cfg.eos_token = "<EOS>";
    cfg.pad_token = "<PAD>";
    return cfg;
}

/*=============================================================================
 * Public API — Lifecycle
 *===========================================================================*/

tokenizer_t* tokenizer_create(const tokenizer_config_t* config)
{
    tokenizer_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = tokenizer_default_config();
    }

    /* Validate */
    if (cfg.initial_vocab_capacity == 0) cfg.initial_vocab_capacity = TOKENIZER_DEFAULT_VOCAB_CAPACITY;
    if (cfg.max_token_length == 0) cfg.max_token_length = TOKENIZER_DEFAULT_MAX_TOKEN_LEN;
    if (!cfg.unk_token) cfg.unk_token = "<UNK>";
    if (!cfg.bos_token) cfg.bos_token = "<BOS>";
    if (!cfg.eos_token) cfg.eos_token = "<EOS>";
    if (!cfg.pad_token) cfg.pad_token = "<PAD>";

    tokenizer_t* tok = (tokenizer_t*)nimcp_calloc(1, sizeof(tokenizer_t));
    if (!tok) {
        LOG_ERROR("tokenizer_create: allocation failed");
        return NULL;
    }

    tok->vocab_capacity = cfg.initial_vocab_capacity;
    tok->max_token_length = cfg.max_token_length;

    tok->id_to_token = (char**)nimcp_calloc(tok->vocab_capacity, sizeof(char*));
    if (!tok->id_to_token) {
        LOG_ERROR("tokenizer_create: vocab array allocation failed");
        nimcp_free(tok);
        return NULL;
    }

    /* Create hash table for token -> id lookup */
    hash_table_config_t ht_cfg;
    memset(&ht_cfg, 0, sizeof(ht_cfg));
    ht_cfg.initial_buckets = cfg.initial_vocab_capacity;
    ht_cfg.key_type = HASH_KEY_STRING;
    ht_cfg.hash_algorithm = HASH_ALG_FNV1A;
    ht_cfg.case_insensitive = false;

    tok->token_to_id = hash_table_create(&ht_cfg);
    if (!tok->token_to_id) {
        LOG_ERROR("tokenizer_create: hash table creation failed");
        nimcp_free(tok->id_to_token);
        nimcp_free(tok);
        return NULL;
    }

    /* Initialize merge rules array */
    tok->merge_capacity = 256;
    tok->merge_rules = (char**)nimcp_calloc(tok->merge_capacity, sizeof(char*));
    if (!tok->merge_rules) {
        LOG_ERROR("tokenizer_create: merge rules allocation failed");
        hash_table_destroy(tok->token_to_id);
        nimcp_free(tok->id_to_token);
        nimcp_free(tok);
        return NULL;
    }

    /* Register special tokens at IDs 0-3 */
    int pad_id = internal_add_token(tok, cfg.pad_token);
    int unk_id = internal_add_token(tok, cfg.unk_token);
    int bos_id = internal_add_token(tok, cfg.bos_token);
    int eos_id = internal_add_token(tok, cfg.eos_token);

    if (pad_id < 0 || unk_id < 0 || bos_id < 0 || eos_id < 0) {
        LOG_ERROR("tokenizer_create: failed to register special tokens");
        tokenizer_destroy(tok);
        return NULL;
    }

    tok->pad_id = (uint32_t)pad_id;
    tok->unk_id = (uint32_t)unk_id;
    tok->bos_id = (uint32_t)bos_id;
    tok->eos_id = (uint32_t)eos_id;

    /* Initialize frequency cache (already zeroed by nimcp_calloc, explicit for clarity) */
    freq_cache_invalidate(tok->freq_cache);

    LOG_INFO("tokenizer_create: created with capacity=%u, special tokens at IDs %u/%u/%u/%u",
             tok->vocab_capacity, tok->pad_id, tok->unk_id, tok->bos_id, tok->eos_id);

    return tok;
}

void tokenizer_destroy(tokenizer_t* tok)
{
    if (!tok) return;

    /* Free all token strings */
    if (tok->id_to_token) {
        for (uint32_t i = 0; i < tok->vocab_size; i++) {
            nimcp_free(tok->id_to_token[i]);
        }
        nimcp_free(tok->id_to_token);
    }

    /* Free merge rules */
    if (tok->merge_rules) {
        for (uint32_t i = 0; i < tok->num_merges; i++) {
            nimcp_free(tok->merge_rules[i]);
        }
        nimcp_free(tok->merge_rules);
    }

    /* Destroy hash table */
    hash_table_destroy(tok->token_to_id);

    nimcp_free(tok);
}

/*=============================================================================
 * Public API — Vocabulary Management
 *===========================================================================*/

int tokenizer_add_token(tokenizer_t* tok, const char* token)
{
    if (!tok || !token) return -1;
    if (strlen(token) == 0) return -1;

    /* Check for duplicate */
    void* existing = hash_table_lookup_string(tok->token_to_id, token);
    if (existing) {
        /* Already exists — return existing ID */
        return (int)(*(uint32_t*)existing);
    }

    return internal_add_token(tok, token);
}

int tokenizer_add_merge_rule(tokenizer_t* tok, const char* token_a, const char* token_b)
{
    if (!tok || !token_a || !token_b) return -1;

    if (ensure_merge_capacity(tok) != 0) return -1;

    /* Store as "tokenA tokenB" */
    size_t len = strlen(token_a) + 1 + strlen(token_b) + 1;
    char* rule = (char*)nimcp_malloc(len);
    if (!rule) return -1;

    snprintf(rule, len, "%s %s", token_a, token_b);
    tok->merge_rules[tok->num_merges] = rule;
    tok->num_merges++;

    return 0;
}

int tokenizer_build_from_text(tokenizer_t* tok, const char* text, uint32_t target_vocab_size)
{
    if (!tok || !text || target_vocab_size == 0) return -1;

    LOG_INFO("tokenizer_build_from_text: building vocab up to %u tokens from %zu bytes of text",
             target_vocab_size, strlen(text));

    /*
     * Step 1: Split text into words (lowercased, punctuation separated)
     */
    uint32_t max_words = 100000;
    char** words = (char**)nimcp_calloc(max_words, sizeof(char*));
    if (!words) return -1;
    uint32_t num_words = 0;

    if (split_text_to_words(text, words, max_words, &num_words) != 0) {
        nimcp_free(words);
        return -1;
    }

    LOG_INFO("tokenizer_build_from_text: split into %u words", num_words);

    /*
     * Step 2: Initialize character-level vocabulary from all unique chars in text
     */
    {
        char* lower_text = nimcp_strdup(text);
        if (!lower_text) {
            for (uint32_t i = 0; i < num_words; i++) nimcp_free(words[i]);
            nimcp_free(words);
            return -1;
        }
        str_to_lower(lower_text);

        /* Collect unique characters */
        bool seen[256];
        memset(seen, 0, sizeof(seen));
        for (const char* p = lower_text; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (!isspace(c) && !seen[c]) {
                seen[c] = true;
                char ch_str[2] = { (char)c, '\0' };
                internal_add_token(tok, ch_str);
            }
        }
        nimcp_free(lower_text);
    }

    LOG_INFO("tokenizer_build_from_text: character vocab = %u tokens (incl. specials)",
             tok->vocab_size);

    /* Also add each unique word that appears in the text to know about them */
    /* (We do NOT add all words now — BPE will build subwords) */

    /*
     * Step 3: Build BPE word representations (character-level split per word)
     * Track word frequencies too for weighted pair counting.
     */
    /* Compute unique words and their frequencies */
    hash_table_config_t freq_cfg;
    memset(&freq_cfg, 0, sizeof(freq_cfg));
    freq_cfg.initial_buckets = 8192;
    freq_cfg.key_type = HASH_KEY_STRING;
    freq_cfg.hash_algorithm = HASH_ALG_FNV1A;
    freq_cfg.case_insensitive = false;

    hash_table_t* word_freq = hash_table_create(&freq_cfg);
    if (!word_freq) {
        for (uint32_t i = 0; i < num_words; i++) nimcp_free(words[i]);
        nimcp_free(words);
        return -1;
    }

    for (uint32_t i = 0; i < num_words; i++) {
        void* val = hash_table_lookup_string(word_freq, words[i]);
        if (val) {
            (*(uint32_t*)val)++;
        } else {
            uint32_t one = 1;
            hash_table_insert_string(word_freq, words[i], &one, sizeof(uint32_t));
        }
    }

    /* Build bpe_word arrays for each unique word */
    /* We'll store them in parallel arrays */
    uint32_t max_unique = (uint32_t)hash_table_size(word_freq);
    if (max_unique == 0) {
        hash_table_destroy(word_freq);
        for (uint32_t i = 0; i < num_words; i++) nimcp_free(words[i]);
        nimcp_free(words);
        return 0;
    }

    bpe_word_t** bpe_words = (bpe_word_t**)nimcp_calloc(max_unique, sizeof(bpe_word_t*));
    uint32_t* bpe_freqs = (uint32_t*)nimcp_calloc(max_unique, sizeof(uint32_t));
    char** unique_words = (char**)nimcp_calloc(max_unique, sizeof(char*));
    if (!bpe_words || !bpe_freqs || !unique_words) {
        nimcp_free(bpe_words);
        nimcp_free(bpe_freqs);
        nimcp_free(unique_words);
        hash_table_destroy(word_freq);
        for (uint32_t i = 0; i < num_words; i++) nimcp_free(words[i]);
        nimcp_free(words);
        return -1;
    }

    /* Deduplicate: build unique word list with frequencies */
    uint32_t num_unique = 0;
    {
        /* Use a second pass to collect unique words in order */
        hash_table_config_t seen_cfg;
        memset(&seen_cfg, 0, sizeof(seen_cfg));
        seen_cfg.initial_buckets = 8192;
        seen_cfg.key_type = HASH_KEY_STRING;
        seen_cfg.hash_algorithm = HASH_ALG_FNV1A;

        hash_table_t* seen_ht = hash_table_create(&seen_cfg);
        if (!seen_ht) {
            nimcp_free(bpe_words);
            nimcp_free(bpe_freqs);
            nimcp_free(unique_words);
            hash_table_destroy(word_freq);
            for (uint32_t i = 0; i < num_words; i++) nimcp_free(words[i]);
            nimcp_free(words);
            return -1;
        }

        for (uint32_t i = 0; i < num_words && num_unique < max_unique; i++) {
            if (hash_table_lookup_string(seen_ht, words[i]) != NULL) continue;

            uint32_t marker = 1;
            hash_table_insert_string(seen_ht, words[i], &marker, sizeof(uint32_t));

            void* freq_val = hash_table_lookup_string(word_freq, words[i]);
            uint32_t freq = freq_val ? *(uint32_t*)freq_val : 1;

            unique_words[num_unique] = nimcp_strdup(words[i]);
            bpe_freqs[num_unique] = freq;
            bpe_words[num_unique] = word_to_chars(words[i]);
            if (!unique_words[num_unique] || !bpe_words[num_unique]) {
                hash_table_destroy(seen_ht);
                goto cleanup_bpe;
            }
            num_unique++;
        }
        hash_table_destroy(seen_ht);
    }

    LOG_INFO("tokenizer_build_from_text: %u unique words, starting BPE iterations", num_unique);

    /*
     * Step 4: Iterative BPE merging
     * Repeat until vocab_size >= target_vocab_size:
     *   a. Count all adjacent pairs (weighted by word frequency)
     *   b. Find most frequent pair
     *   c. Merge that pair in all words
     *   d. Add merged token to vocabulary
     *   e. Record merge rule
     */
    {
        uint32_t iteration = 0;
        uint32_t max_iterations = target_vocab_size * 2; /* safety limit */

        while (tok->vocab_size < target_vocab_size && iteration < max_iterations) {
            iteration++;

            /* Count adjacent pairs */
            pair_table_t* pt = pair_table_create(4096);
            if (!pt) break;

            for (uint32_t w = 0; w < num_unique; w++) {
                bpe_word_t* bw = bpe_words[w];
                if (!bw || bw->num_pieces < 2) continue;
                for (uint32_t p = 0; p + 1 < bw->num_pieces; p++) {
                    /* Increment by word frequency for weighted counting */
                    for (uint32_t f = 0; f < bpe_freqs[w]; f++) {
                        pair_table_increment(pt, bw->pieces[p], bw->pieces[p + 1]);
                    }
                }
            }

            /* Find best pair */
            char* best_a = NULL;
            char* best_b = NULL;
            uint32_t best_count = pair_table_find_best(pt, &best_a, &best_b);

            if (best_count == 0 || !best_a || !best_b) {
                pair_table_destroy(pt);
                break; /* No more pairs to merge */
            }

            /* Create merged token string */
            size_t merged_len = strlen(best_a) + strlen(best_b) + 1;
            char* merged_token = (char*)nimcp_malloc(merged_len);
            if (!merged_token) {
                pair_table_destroy(pt);
                break;
            }
            snprintf(merged_token, merged_len, "%s%s", best_a, best_b);

            /* Make copies of best_a and best_b before destroying pair table */
            char* saved_a = nimcp_strdup(best_a);
            char* saved_b = nimcp_strdup(best_b);

            pair_table_destroy(pt);

            if (!saved_a || !saved_b) {
                nimcp_free(saved_a);
                nimcp_free(saved_b);
                nimcp_free(merged_token);
                break;
            }

            /* Add merged token to vocabulary */
            internal_add_token(tok, merged_token);

            /* Record merge rule */
            tokenizer_add_merge_rule(tok, saved_a, saved_b);

            /* Apply merge to all BPE words */
            for (uint32_t w = 0; w < num_unique; w++) {
                if (bpe_words[w]) {
                    bpe_word_apply_merge(bpe_words[w], saved_a, saved_b);
                }
            }

            nimcp_free(saved_a);
            nimcp_free(saved_b);
            nimcp_free(merged_token);

            if (iteration % 100 == 0) {
                LOG_INFO("tokenizer_build_from_text: iteration %u, vocab_size=%u",
                         iteration, tok->vocab_size);
            }
        }

        LOG_INFO("tokenizer_build_from_text: finished after %u iterations, vocab_size=%u",
                 iteration, tok->vocab_size);
    }

    /* Also add complete words that appear frequently enough */
    for (uint32_t w = 0; w < num_unique; w++) {
        if (bpe_freqs[w] >= 2 && tok->vocab_size < target_vocab_size) {
            internal_add_token(tok, unique_words[w]); /* Duplicates are safely ignored */
        }
    }

cleanup_bpe:
    /* Clean up BPE workspace */
    for (uint32_t i = 0; i < num_unique; i++) {
        bpe_word_destroy(bpe_words[i]);
        nimcp_free(unique_words[i]);
    }
    nimcp_free(bpe_words);
    nimcp_free(bpe_freqs);
    nimcp_free(unique_words);
    hash_table_destroy(word_freq);

    for (uint32_t i = 0; i < num_words; i++) {
        nimcp_free(words[i]);
    }
    nimcp_free(words);

    LOG_INFO("tokenizer_build_from_text: final vocab_size=%u, num_merges=%u",
             tok->vocab_size, tok->num_merges);

    return 0;
}

/*=============================================================================
 * Public API — Encoding
 *===========================================================================*/

int tokenizer_encode(const tokenizer_t* tok, const char* text,
                     uint32_t* token_ids, uint32_t max_tokens, uint32_t* num_tokens)
{
    if (!tok || !text || !token_ids || !num_tokens) return -1;

    *num_tokens = 0;

    /* Split text into word tokens */
    uint32_t max_words = max_tokens * 2; /* words can expand to multiple subtokens */
    if (max_words < 256) max_words = 256;
    if (max_words > 100000) max_words = 100000;

    char** words = (char**)nimcp_calloc(max_words, sizeof(char*));
    if (!words) return -1;

    uint32_t num_words = 0;
    if (split_text_to_words(text, words, max_words, &num_words) != 0) {
        nimcp_free(words);
        return -1;
    }

    for (uint32_t w = 0; w < num_words && *num_tokens < max_tokens; w++) {
        const char* word = words[w];

        /* Try frequency cache first (avoids hash table lookup for hot tokens) */
        uint32_t cached_id = freq_cache_lookup(tok->freq_cache, word);
        if (cached_id != TOKENIZER_INVALID_ID) {
            token_ids[*num_tokens] = cached_id;
            (*num_tokens)++;
            continue;
        }

        /* Try direct vocabulary lookup */
        void* val = hash_table_lookup_string(tok->token_to_id, word);
        if (val) {
            uint32_t word_id = *(uint32_t*)val;
            /* Store in frequency cache for next time */
            freq_cache_store(((tokenizer_t*)tok)->freq_cache, word, word_id);
            token_ids[*num_tokens] = word_id;
            (*num_tokens)++;
            continue;
        }

        /*
         * Word not in vocabulary — apply BPE merge rules to subword-segment it.
         * 1. Split word into characters
         * 2. Apply each merge rule in order
         * 3. Look up resulting subtokens
         */
        bpe_word_t* bw = word_to_chars(word);
        if (!bw) continue;

        /* Apply merge rules in order */
        for (uint32_t m = 0; m < tok->num_merges; m++) {
            const char* rule = tok->merge_rules[m];
            /* Parse "tokenA tokenB" */
            const char* space = strchr(rule, ' ');
            if (!space) continue;

            size_t a_len = (size_t)(space - rule);
            char a_buf[256];
            if (a_len >= sizeof(a_buf)) continue;
            memcpy(a_buf, rule, a_len);
            a_buf[a_len] = '\0';

            const char* b_str = space + 1;

            bpe_word_apply_merge(bw, a_buf, b_str);
        }

        /* Emit subtokens — look up each piece in vocab */
        for (uint32_t p = 0; p < bw->num_pieces && *num_tokens < max_tokens; p++) {
            void* sub_val = hash_table_lookup_string(tok->token_to_id, bw->pieces[p]);
            if (sub_val) {
                token_ids[*num_tokens] = *(uint32_t*)sub_val;
            } else {
                /* Unknown subtoken -> UNK */
                token_ids[*num_tokens] = tok->unk_id;
            }
            (*num_tokens)++;
        }

        bpe_word_destroy(bw);
    }

    /* Free words */
    for (uint32_t i = 0; i < num_words; i++) {
        nimcp_free(words[i]);
    }
    nimcp_free(words);

    return 0;
}

/*=============================================================================
 * Public API — Decoding
 *===========================================================================*/

int tokenizer_decode(const tokenizer_t* tok, const uint32_t* token_ids,
                     uint32_t num_tokens, char* text, uint32_t max_length)
{
    if (!tok || !token_ids || !text || max_length == 0) return -1;

    text[0] = '\0';
    uint32_t pos = 0;

    for (uint32_t i = 0; i < num_tokens; i++) {
        uint32_t id = token_ids[i];

        /* Skip PAD tokens */
        if (id == tok->pad_id) continue;

        /* Skip BOS/EOS in output */
        if (id == tok->bos_id || id == tok->eos_id) continue;

        const char* token_str = NULL;
        if (id < tok->vocab_size) {
            token_str = tok->id_to_token[id];
        }
        if (!token_str) {
            token_str = tok->id_to_token[tok->unk_id];
        }
        if (!token_str) continue;

        size_t tok_len = strlen(token_str);

        /* Add space separator between tokens (not before first token) */
        if (pos > 0 && pos < max_length - 1) {
            /*
             * Heuristic: don't add space before punctuation or after opening brackets
             * to produce more natural-looking text
             */
            bool skip_space = false;
            if (tok_len == 1 && is_punct_separator(token_str[0])) {
                skip_space = true;
            }
            /* Check if previous char was an opening bracket */
            if (pos > 0 && (text[pos - 1] == '(' || text[pos - 1] == '[' || text[pos - 1] == '{')) {
                skip_space = true;
            }

            if (!skip_space) {
                text[pos++] = ' ';
            }
        }

        /* Copy token string */
        for (size_t c = 0; c < tok_len && pos < max_length - 1; c++) {
            text[pos++] = token_str[c];
        }
    }

    text[pos] = '\0';
    return 0;
}

/*=============================================================================
 * Public API — Lookups
 *===========================================================================*/

uint32_t tokenizer_get_vocab_size(const tokenizer_t* tok)
{
    if (!tok) return 0;
    return tok->vocab_size;
}

uint32_t tokenizer_token_to_id(const tokenizer_t* tok, const char* token)
{
    if (!tok || !token) return TOKENIZER_INVALID_ID;

    /* Check frequency cache first */
    uint32_t cached = freq_cache_lookup(tok->freq_cache, token);
    if (cached != TOKENIZER_INVALID_ID) return cached;

    void* val = hash_table_lookup_string(tok->token_to_id, token);
    if (!val) return TOKENIZER_INVALID_ID;

    uint32_t id = *(uint32_t*)val;

    /* Store in frequency cache for next time (cast away const for cache update) */
    freq_cache_store(((tokenizer_t*)tok)->freq_cache, token, id);

    return id;
}

const char* tokenizer_id_to_token(const tokenizer_t* tok, uint32_t id)
{
    if (!tok) return NULL;
    if (id >= tok->vocab_size) return NULL;
    return tok->id_to_token[id];
}

/*=============================================================================
 * Public API — Serialization
 *===========================================================================*/

int tokenizer_save(const tokenizer_t* tok, const char* path)
{
    if (!tok || !path) return -1;

    FILE* f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("tokenizer_save: cannot open '%s' for writing", path);
        return -1;
    }

    /* Header: magic, version, vocab_size, num_merges, max_token_length, special IDs */
    uint32_t magic = TOKENIZER_MAGIC;
    uint32_t version = TOKENIZER_VERSION;

    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);
    fwrite(&tok->vocab_size, sizeof(uint32_t), 1, f);
    fwrite(&tok->num_merges, sizeof(uint32_t), 1, f);
    fwrite(&tok->max_token_length, sizeof(uint32_t), 1, f);
    fwrite(&tok->unk_id, sizeof(uint32_t), 1, f);
    fwrite(&tok->bos_id, sizeof(uint32_t), 1, f);
    fwrite(&tok->eos_id, sizeof(uint32_t), 1, f);
    fwrite(&tok->pad_id, sizeof(uint32_t), 1, f);

    /* Vocabulary: for each token, write length + string (no null terminator) */
    for (uint32_t i = 0; i < tok->vocab_size; i++) {
        const char* token = tok->id_to_token[i];
        uint32_t len = token ? (uint32_t)strlen(token) : 0;
        fwrite(&len, sizeof(uint32_t), 1, f);
        if (len > 0) {
            fwrite(token, 1, len, f);
        }
    }

    /* Merge rules: for each rule, write length + string */
    for (uint32_t i = 0; i < tok->num_merges; i++) {
        const char* rule = tok->merge_rules[i];
        uint32_t len = rule ? (uint32_t)strlen(rule) : 0;
        fwrite(&len, sizeof(uint32_t), 1, f);
        if (len > 0) {
            fwrite(rule, 1, len, f);
        }
    }

    fclose(f);
    LOG_INFO("tokenizer_save: saved %u tokens, %u merges to '%s'",
             tok->vocab_size, tok->num_merges, path);
    return 0;
}

tokenizer_t* tokenizer_load(const char* path)
{
    if (!path) return NULL;

    FILE* f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR("tokenizer_load: cannot open '%s' for reading", path);
        return NULL;
    }

    /* Read header */
    uint32_t magic, version, vocab_size, num_merges, max_token_length;
    uint32_t unk_id, bos_id, eos_id, pad_id;

    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != TOKENIZER_MAGIC) {
        LOG_ERROR("tokenizer_load: invalid magic number in '%s'", path);
        fclose(f);
        return NULL;
    }
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version != TOKENIZER_VERSION) {
        LOG_ERROR("tokenizer_load: unsupported version %u in '%s'", version, path);
        fclose(f);
        return NULL;
    }

    if (fread(&vocab_size, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&num_merges, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&max_token_length, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&unk_id, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&bos_id, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&eos_id, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&pad_id, sizeof(uint32_t), 1, f) != 1) goto read_error;

    /* Create tokenizer with enough capacity (bypass special token auto-add) */
    tokenizer_t* tok = (tokenizer_t*)nimcp_calloc(1, sizeof(tokenizer_t));
    if (!tok) goto read_error;

    tok->max_token_length = max_token_length;
    tok->vocab_capacity = vocab_size + 64; /* some headroom */
    tok->id_to_token = (char**)nimcp_calloc(tok->vocab_capacity, sizeof(char*));
    if (!tok->id_to_token) {
        nimcp_free(tok);
        goto read_error;
    }

    hash_table_config_t ht_cfg;
    memset(&ht_cfg, 0, sizeof(ht_cfg));
    ht_cfg.initial_buckets = tok->vocab_capacity;
    ht_cfg.key_type = HASH_KEY_STRING;
    ht_cfg.hash_algorithm = HASH_ALG_FNV1A;

    tok->token_to_id = hash_table_create(&ht_cfg);
    if (!tok->token_to_id) {
        nimcp_free(tok->id_to_token);
        nimcp_free(tok);
        goto read_error;
    }

    /* Read vocabulary */
    for (uint32_t i = 0; i < vocab_size; i++) {
        uint32_t len;
        if (fread(&len, sizeof(uint32_t), 1, f) != 1) {
            tokenizer_destroy(tok);
            goto read_error;
        }

        char* token = (char*)nimcp_malloc(len + 1);
        if (!token) {
            tokenizer_destroy(tok);
            goto read_error;
        }
        if (len > 0 && fread(token, 1, len, f) != len) {
            nimcp_free(token);
            tokenizer_destroy(tok);
            goto read_error;
        }
        token[len] = '\0';

        tok->id_to_token[i] = token;
        uint32_t id = i;
        hash_table_insert_string(tok->token_to_id, token, &id, sizeof(uint32_t));
    }
    tok->vocab_size = vocab_size;
    tok->unk_id = unk_id;
    tok->bos_id = bos_id;
    tok->eos_id = eos_id;
    tok->pad_id = pad_id;

    /* Read merge rules */
    tok->merge_capacity = num_merges + 64;
    tok->merge_rules = (char**)nimcp_calloc(tok->merge_capacity, sizeof(char*));
    if (!tok->merge_rules) {
        tokenizer_destroy(tok);
        goto read_error;
    }

    for (uint32_t i = 0; i < num_merges; i++) {
        uint32_t len;
        if (fread(&len, sizeof(uint32_t), 1, f) != 1) {
            tokenizer_destroy(tok);
            goto read_error;
        }

        char* rule = (char*)nimcp_malloc(len + 1);
        if (!rule) {
            tokenizer_destroy(tok);
            goto read_error;
        }
        if (len > 0 && fread(rule, 1, len, f) != len) {
            nimcp_free(rule);
            tokenizer_destroy(tok);
            goto read_error;
        }
        rule[len] = '\0';
        tok->merge_rules[i] = rule;
    }
    tok->num_merges = num_merges;

    fclose(f);
    LOG_INFO("tokenizer_load: loaded %u tokens, %u merges from '%s'",
             tok->vocab_size, tok->num_merges, path);
    return tok;

read_error:
    LOG_ERROR("tokenizer_load: read error in '%s'", path);
    fclose(f);
    return NULL;
}
