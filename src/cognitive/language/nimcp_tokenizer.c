/**
 * @file nimcp_tokenizer.c
 * @brief Brain-native tokenizer with BPE-style subword merging
 */

#include "cognitive/language/nimcp_tokenizer.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define LOG_MODULE "TOKENIZER"
#define HASH_CAPACITY_MULT 3
#define MAX_MERGES 16384

struct nimcp_tokenizer {
    nimcp_tokenizer_config_t config;
    nimcp_token_entry_t* tokens;
    uint32_t vocab_size;

    /* Text→ID hash table (open addressing) */
    uint32_t* hash_ids;
    uint32_t hash_capacity;

    /* Merge rules */
    nimcp_merge_rule_t* merges;
    uint32_t num_merges;

    /* Pair statistics */
    uint32_t* pair_left;
    uint32_t* pair_right;
    uint32_t* pair_count;
    uint32_t num_pairs;
    uint32_t max_pairs;

    nimcp_mutex_t* lock;
    uint32_t rng_state;
};

/* FNV-1a hash */
static uint32_t _hash_str(const char* s, uint32_t cap) {
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h % cap;
}

static uint32_t _xorshift(uint32_t* s) {
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x;
}

static float _randf(uint32_t* s) {
    return (_xorshift(s) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

nimcp_tokenizer_config_t nimcp_tokenizer_config_default(void) {
    nimcp_tokenizer_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_vocab_size = 8192;
    cfg.min_frequency = 2;
    cfg.embed_dim = 256;
    cfg.merge_threshold = 10.0f;
    cfg.learn_from_brain = true;
    cfg.enable_subword = true;
    cfg.enable_phrase = true;
    cfg.max_token_length = 32;
    return cfg;
}

static void _add_token(nimcp_tokenizer_t* tok, const char* text, nimcp_token_type_t type) {
    if (tok->vocab_size >= tok->config.max_vocab_size) return;
    uint32_t id = tok->vocab_size;
    nimcp_token_entry_t* t = &tok->tokens[id];
    t->id = id;
    snprintf(t->text, sizeof(t->text), "%s", text);
    t->type = type;
    t->frequency = 0.0f;
    t->embed_dim = tok->config.embed_dim;
    t->embedding = NULL; /* Allocated on demand */
    t->merge_count = 0;

    /* Add to hash table */
    uint32_t slot = _hash_str(text, tok->hash_capacity);
    uint32_t attempts = 0;
    while (tok->hash_ids[slot] != UINT32_MAX && attempts < tok->hash_capacity) {
        slot = (slot + 1) % tok->hash_capacity;
        attempts++;
    }
    if (attempts < tok->hash_capacity) {
        tok->hash_ids[slot] = id;
        tok->vocab_size++;
    } else {
        /* Hash table full — token not added. This shouldn't happen with
         * capacity = 3 × max_vocab_size, but log if it does. */
        LOG_WARN("[%s] Hash table full, token '%s' not added", LOG_MODULE, text);
    }
}

nimcp_tokenizer_t* nimcp_tokenizer_create(const nimcp_tokenizer_config_t* config) {
    nimcp_tokenizer_t* tok = nimcp_calloc(1, sizeof(nimcp_tokenizer_t));
    if (!tok) return NULL;

    tok->config = config ? *config : nimcp_tokenizer_config_default();
    uint32_t mv = tok->config.max_vocab_size;

    tok->tokens = nimcp_calloc(mv, sizeof(nimcp_token_entry_t));
    tok->hash_capacity = mv * HASH_CAPACITY_MULT;
    tok->hash_ids = nimcp_calloc(tok->hash_capacity, sizeof(uint32_t));
    tok->merges = nimcp_calloc(MAX_MERGES, sizeof(nimcp_merge_rule_t));
    tok->max_pairs = mv * 4;
    tok->pair_left = nimcp_calloc(tok->max_pairs, sizeof(uint32_t));
    tok->pair_right = nimcp_calloc(tok->max_pairs, sizeof(uint32_t));
    tok->pair_count = nimcp_calloc(tok->max_pairs, sizeof(uint32_t));

    if (!tok->tokens || !tok->hash_ids || !tok->merges ||
        !tok->pair_left || !tok->pair_right || !tok->pair_count) {
        nimcp_tokenizer_destroy(tok);
        return NULL;
    }

    /* Initialize hash table to empty */
    memset(tok->hash_ids, 0xFF, tok->hash_capacity * sizeof(uint32_t));

    tok->rng_state = 42;
    tok->lock = nimcp_mutex_create(NULL);

    /* Special tokens */
    _add_token(tok, "<BOS>", NIMCP_TOKEN_SPECIAL);
    _add_token(tok, "<EOS>", NIMCP_TOKEN_SPECIAL);
    _add_token(tok, "<UNK>", NIMCP_TOKEN_SPECIAL);
    _add_token(tok, "<PAD>", NIMCP_TOKEN_SPECIAL);

    /* ASCII printable characters */
    char ch[2] = {0, 0};
    for (int c = 32; c < 127; c++) {
        ch[0] = (char)c;
        _add_token(tok, ch, NIMCP_TOKEN_CHARACTER);
    }

    LOG_INFO("[%s] Created (vocab=%u initial tokens, max=%u)",
             LOG_MODULE, tok->vocab_size, mv);
    return tok;
}

void nimcp_tokenizer_destroy(nimcp_tokenizer_t* tok) {
    if (!tok) return;
    if (tok->tokens) {
        for (uint32_t i = 0; i < tok->vocab_size; i++)
            nimcp_free(tok->tokens[i].embedding);
        nimcp_free(tok->tokens);
    }
    nimcp_free(tok->hash_ids);
    nimcp_free(tok->merges);
    nimcp_free(tok->pair_left);
    nimcp_free(tok->pair_right);
    nimcp_free(tok->pair_count);
    if (tok->lock) nimcp_mutex_free(tok->lock);
    nimcp_free(tok);
}

uint32_t nimcp_tokenizer_text_to_id(const nimcp_tokenizer_t* tok, const char* text) {
    if (!tok || !text) return 2; /* UNK */
    uint32_t slot = _hash_str(text, tok->hash_capacity);
    uint32_t attempts = 0;
    while (attempts < tok->hash_capacity) {
        uint32_t id = tok->hash_ids[slot];
        if (id == UINT32_MAX) return 2; /* UNK - empty slot */
        if (id < tok->vocab_size && strcmp(tok->tokens[id].text, text) == 0)
            return id;
        slot = (slot + 1) % tok->hash_capacity;
        attempts++;
    }
    return 2; /* UNK */
}

const char* nimcp_tokenizer_id_to_text(const nimcp_tokenizer_t* tok, uint32_t id) {
    if (!tok || id >= tok->vocab_size) return "<UNK>";
    return tok->tokens[id].text;
}

int nimcp_tokenizer_encode(const nimcp_tokenizer_t* tok,
    const char* text, uint32_t* token_ids, uint32_t max_tokens)
{
    if (!tok || !text || !token_ids || max_tokens == 0) return -1;

    /* Step 1: character-level tokenization */
    uint32_t ids[4096];
    uint32_t n = 0;
    for (const char* p = text; *p && n < 4095; p++) {
        char ch[2] = {*p, 0};
        ids[n++] = nimcp_tokenizer_text_to_id(tok, ch);
    }

    /* Step 2: apply merge rules greedily */
    for (uint32_t m = 0; m < tok->num_merges; m++) {
        const nimcp_merge_rule_t* rule = &tok->merges[m];
        for (uint32_t i = 0; i + 1 < n; i++) {
            if (ids[i] == rule->left_id && ids[i+1] == rule->right_id) {
                ids[i] = rule->merged_id;
                /* Shift remaining */
                for (uint32_t j = i + 1; j + 1 < n; j++)
                    ids[j] = ids[j + 1];
                n--;
            }
        }
    }

    /* Copy to output */
    uint32_t out = n < max_tokens ? n : max_tokens;
    memcpy(token_ids, ids, out * sizeof(uint32_t));

    /* W12 KG emit: "encode" events fire on every call to a tokenization
     * hot path; emit is internally rate-limited to ~1 in 512. */
    w12_emit_tokenizer_event_auto("encode", tok->vocab_size, out);

    return (int)out;
}

int nimcp_tokenizer_decode(const nimcp_tokenizer_t* tok,
    const uint32_t* token_ids, uint32_t num_tokens,
    char* text, uint32_t max_text_length)
{
    if (!tok || !token_ids || !text || max_text_length == 0) return -1;
    text[0] = '\0';
    uint32_t pos = 0;
    for (uint32_t i = 0; i < num_tokens; i++) {
        const char* t = nimcp_tokenizer_id_to_text(tok, token_ids[i]);
        uint32_t len = (uint32_t)strlen(t);
        if (pos + len >= max_text_length) break;
        memcpy(text + pos, t, len);
        pos += len;
    }
    text[pos] = '\0';
    return (int)pos;
}

int nimcp_tokenizer_train(nimcp_tokenizer_t* tok, const char** texts, uint32_t num_texts) {
    if (!tok || !texts || num_texts == 0) return -1;

    /* Encode all texts to character tokens first */
    for (uint32_t t = 0; t < num_texts; t++) {
        if (!texts[t]) continue;
        /* Count word frequencies */
        char buf[128];
        const char* p = texts[t];
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            uint32_t len = 0;
            while (*p && !isspace((unsigned char)*p) && len < 127)
                buf[len++] = (char)tolower((unsigned char)*p++);
            buf[len] = '\0';
            if (len > 0) {
                uint32_t id = nimcp_tokenizer_text_to_id(tok, buf);
                if (id == 2 && tok->vocab_size < tok->config.max_vocab_size) {
                    _add_token(tok, buf, NIMCP_TOKEN_WORD);
                    tok->tokens[tok->vocab_size - 1].frequency = 1.0f;
                } else if (id < tok->vocab_size) {
                    tok->tokens[id].frequency += 1.0f;
                }
            }
        }
    }

    LOG_INFO("[%s] Trained on %u texts, vocab=%u", LOG_MODULE, num_texts, tok->vocab_size);
    return 0;
}

int nimcp_tokenizer_learn(nimcp_tokenizer_t* tok, const char* text) {
    if (!tok || !text) return -1;
    const char* arr[1] = {text};
    return nimcp_tokenizer_train(tok, arr, 1);
}

int nimcp_tokenizer_learn_from_brain(nimcp_tokenizer_t* tok,
    const char* text, const float* brain_embedding, uint32_t embed_dim)
{
    if (!tok || !text) return -1;
    nimcp_tokenizer_learn(tok, text);

    /* Associate brain embedding with tokens in this text */
    if (brain_embedding && embed_dim > 0) {
        char buf[128];
        const char* p = text;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            uint32_t len = 0;
            while (*p && !isspace((unsigned char)*p) && len < 127)
                buf[len++] = (char)tolower((unsigned char)*p++);
            buf[len] = '\0';
            uint32_t id = nimcp_tokenizer_text_to_id(tok, buf);
            if (id < tok->vocab_size && id >= 4) {
                /* Initialize or update embedding */
                uint32_t ed = tok->config.embed_dim;
                if (!tok->tokens[id].embedding) {
                    tok->tokens[id].embedding = nimcp_calloc(ed, sizeof(float));
                    tok->tokens[id].embed_dim = ed;
                }
                if (tok->tokens[id].embedding) {
                    uint32_t chunk = embed_dim / ed;
                    if (chunk == 0) chunk = 1;
                    for (uint32_t i = 0; i < ed && i < embed_dim; i++) {
                        float val = brain_embedding[i % embed_dim];
                        tok->tokens[id].embedding[i] =
                            0.9f * tok->tokens[id].embedding[i] + 0.1f * val;
                    }
                }
            }
        }
    }
    return 0;
}

const float* nimcp_tokenizer_get_embedding(const nimcp_tokenizer_t* tok, uint32_t id) {
    if (!tok || id >= tok->vocab_size) return NULL;
    return tok->tokens[id].embedding;
}

int nimcp_tokenizer_set_embedding(nimcp_tokenizer_t* tok,
    uint32_t id, const float* embedding, uint32_t embed_dim)
{
    if (!tok || id >= tok->vocab_size || !embedding) return -1;
    uint32_t ed = tok->config.embed_dim;
    if (!tok->tokens[id].embedding) {
        tok->tokens[id].embedding = nimcp_calloc(ed, sizeof(float));
        tok->tokens[id].embed_dim = ed;
    }
    if (!tok->tokens[id].embedding) return -1;
    uint32_t n = ed < embed_dim ? ed : embed_dim;
    memcpy(tok->tokens[id].embedding, embedding, n * sizeof(float));
    return 0;
}

int nimcp_tokenizer_save(const nimcp_tokenizer_t* tok, const char* filepath) {
    if (!tok || !filepath) return -1;
    FILE* f = fopen(filepath, "w");
    if (!f) return -1;
    fprintf(f, "NTOK 1 %u %u\n", tok->vocab_size, tok->num_merges);
    for (uint32_t i = 0; i < tok->vocab_size; i++) {
        const nimcp_token_entry_t* t = &tok->tokens[i];
        fprintf(f, "%u %s %d %.6f\n", t->id, t->text, t->type, t->frequency);
    }
    fprintf(f, "MERGES\n");
    for (uint32_t i = 0; i < tok->num_merges; i++) {
        const nimcp_merge_rule_t* m = &tok->merges[i];
        fprintf(f, "%u %u %u %.2f\n", m->left_id, m->right_id, m->merged_id, m->score);
    }
    fclose(f);
    return 0;
}

int nimcp_tokenizer_load(nimcp_tokenizer_t* tok, const char* filepath) {
    if (!tok || !filepath) return -1;
    FILE* f = fopen(filepath, "r");
    if (!f) return -1;
    uint32_t vs, nm;
    if (fscanf(f, "NTOK 1 %u %u\n", &vs, &nm) != 2) { fclose(f); return -1; }

    for (uint32_t i = 0; i < vs && i < tok->config.max_vocab_size; i++) {
        uint32_t id;
        int type;
        float freq;
        char text[128];
        if (fscanf(f, "%u %127s %d %f\n", &id, text, &type, &freq) == 4) {
            if (id < tok->vocab_size) {
                /* Token already present at this slot — refresh its frequency.
                 * The on-disk id is only trustworthy when it indexes an
                 * already-loaded slot; writing tok->tokens[id] for an
                 * arbitrary file-supplied id is a heap OOB write. */
                tok->tokens[id].frequency = freq;
            } else {
                /* New token. _add_token ignores the file's id and assigns the
                 * next sequential slot (tok->vocab_size), so the frequency must
                 * be written to the slot it actually used, not to tok->tokens[id]. */
                uint32_t before = tok->vocab_size;
                _add_token(tok, text, (nimcp_token_type_t)type);
                if (tok->vocab_size > before) {
                    tok->tokens[tok->vocab_size - 1].frequency = freq;
                }
            }
        }
    }

    char line[256];
    if (fgets(line, sizeof(line), f) && strncmp(line, "MERGES", 6) == 0) {
        for (uint32_t i = 0; i < nm && tok->num_merges < MAX_MERGES; i++) {
            nimcp_merge_rule_t m;
            if (fscanf(f, "%u %u %u %f\n", &m.left_id, &m.right_id,
                       &m.merged_id, &m.score) == 4) {
                tok->merges[tok->num_merges++] = m;
            }
        }
    }

    fclose(f);
    return 0;
}

uint32_t nimcp_tokenizer_get_vocab_size(const nimcp_tokenizer_t* tok) {
    return tok ? tok->vocab_size : 0;
}

uint32_t nimcp_tokenizer_get_num_merges(const nimcp_tokenizer_t* tok) {
    return tok ? tok->num_merges : 0;
}
