/**
 * @file nimcp_native_language.c
 * @brief Brain-native language production system
 *
 * WHAT: Generates text from brain neural embeddings without external LLM
 * WHY:  Language should belong to the brain, not be parasitic on Phi-3
 * HOW:  Learned projection (brain→token space) + autoregressive decoding
 *       with nucleus sampling, repetition penalty, and bigram fluency
 */

#include "cognitive/language/nimcp_native_language.h"
#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define LOG_MODULE "NATIVE_LANGUAGE"
#define BRAIN_DIM 4096
#define MAX_RECENT 32

struct nimcp_native_language {
    nimcp_language_config_t config;
    nimcp_vocabulary_t vocab;
    nimcp_phonological_loop_t* phono_loop;

    /* Projection: brain_dim → embed_dim */
    float* brain_to_token;      /* [BRAIN_DIM × embed_dim] */
    float* token_to_brain;      /* [embed_dim × BRAIN_DIM] */

    float* attention_scores;    /* [max_vocab] temp buffer */
    float* softmax_buf;         /* [max_vocab] temp buffer */

    /* Recently generated token IDs (for repetition penalty) */
    uint32_t recent[MAX_RECENT];
    uint32_t recent_count;

    uint32_t train_steps;
    float ema_loss;
    nimcp_mutex_t* lock;
    uint32_t rng_state;

    /* Positional encoding for sequence position awareness */
    nimcp_pos_encoder_t* pos_encoder;
    float* pos_buffer;             /* [embed_dim] temp buffer for positional encoding */
};

/* ======================================================================== */
/* Helpers                                                                    */
/* ======================================================================== */

static uint32_t _xorshift(uint32_t* s) {
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x; return x;
}

static float _randf(uint32_t* s) {
    return (_xorshift(s) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float _dot(const float* a, const float* b, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

static void _matvec(const float* mat, const float* vec, float* out,
                     uint32_t rows, uint32_t cols) {
    for (uint32_t r = 0; r < rows; r++) {
        float sum = 0.0f;
        for (uint32_t c = 0; c < cols; c++)
            sum += mat[r * cols + c] * vec[c];
        out[r] = sum;
    }
}

/* Xavier initialization */
static void _xavier_init(float* mat, uint32_t rows, uint32_t cols, uint32_t* rng) {
    float scale = sqrtf(6.0f / (float)(rows + cols));
    for (uint32_t i = 0; i < rows * cols; i++)
        mat[i] = (_randf(rng) * 2.0f - 1.0f) * scale;
}

static void _init_special_token(nimcp_vocabulary_t* v, uint32_t id,
                                 const char* text, uint32_t embed_dim, uint32_t* rng) {
    nimcp_token_t* t = &v->tokens[id];
    t->id = id;
    snprintf(t->text, sizeof(t->text), "%s", text);
    t->embed_dim = embed_dim;
    t->embedding = nimcp_calloc(embed_dim, sizeof(float));
    if (t->embedding) {
        for (uint32_t i = 0; i < embed_dim; i++)
            t->embedding[i] = (_randf(rng) * 2.0f - 1.0f) * 0.1f;
    }
    t->frequency = 0.0f;
    t->co_occurrences = 0;
}

/* ======================================================================== */
/* Lifecycle                                                                  */
/* ======================================================================== */

nimcp_language_config_t nimcp_language_config_default(void) {
    nimcp_language_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_vocab_size = 8192;
    cfg.embed_dim = 256;
    cfg.max_seq_length = 128;
    cfg.temperature = 0.7f;
    cfg.top_p = 0.9f;
    cfg.repetition_penalty = 1.2f;
    cfg.learn_vocabulary = true;
    cfg.min_token_frequency = 0.001f;
    return cfg;
}

nimcp_native_language_t* nimcp_native_language_create(const nimcp_language_config_t* config) {
    nimcp_native_language_t* lang = nimcp_calloc(1, sizeof(nimcp_native_language_t));
    if (!lang) return NULL;

    lang->config = config ? *config : nimcp_language_config_default();
    uint32_t ed = lang->config.embed_dim;
    uint32_t mv = lang->config.max_vocab_size;

    /* Allocate vocabulary */
    lang->vocab.tokens = nimcp_calloc(mv, sizeof(nimcp_token_t));
    lang->vocab.max_vocab = mv;
    lang->vocab.embed_dim = ed;
    if (!lang->vocab.tokens) { nimcp_free(lang); return NULL; }

    /* Projection matrices */
    lang->brain_to_token = nimcp_calloc(BRAIN_DIM * ed, sizeof(float));
    lang->token_to_brain = nimcp_calloc(ed * BRAIN_DIM, sizeof(float));
    lang->attention_scores = nimcp_calloc(mv, sizeof(float));
    lang->softmax_buf = nimcp_calloc(mv, sizeof(float));

    if (!lang->brain_to_token || !lang->token_to_brain ||
        !lang->attention_scores || !lang->softmax_buf) {
        nimcp_native_language_destroy(lang);
        return NULL;
    }

    lang->rng_state = 42;
    _xavier_init(lang->brain_to_token, ed, BRAIN_DIM, &lang->rng_state);
    _xavier_init(lang->token_to_brain, BRAIN_DIM, ed, &lang->rng_state);

    /* Initialize special tokens */
    _init_special_token(&lang->vocab, 0, "<BOS>", ed, &lang->rng_state);
    _init_special_token(&lang->vocab, 1, "<EOS>", ed, &lang->rng_state);
    _init_special_token(&lang->vocab, 2, "<UNK>", ed, &lang->rng_state);
    _init_special_token(&lang->vocab, 3, "<PAD>", ed, &lang->rng_state);
    lang->vocab.bos_id = 0; lang->vocab.eos_id = 1;
    lang->vocab.unk_id = 2; lang->vocab.pad_id = 3;
    lang->vocab.vocab_size = 4;

    /* Seed with common English words */
    const char* seed_words[] = {
        "the", "a", "is", "are", "was", "were", "be", "been", "being",
        "have", "has", "had", "do", "does", "did", "will", "would", "could",
        "should", "may", "might", "must", "shall", "can", "need",
        "I", "you", "he", "she", "it", "we", "they", "me", "him", "her",
        "us", "them", "my", "your", "his", "its", "our", "their",
        "this", "that", "these", "those", "what", "which", "who", "whom",
        "and", "or", "but", "not", "no", "yes", "if", "then", "so", "because",
        "in", "on", "at", "to", "for", "with", "from", "by", "of", "about",
        "up", "out", "into", "over", "after", "before", "between", "under",
        "like", "see", "feel", "hear", "think", "know", "want", "make",
        "go", "come", "take", "give", "look", "find", "tell", "say",
        "good", "bad", "big", "small", "new", "old", "long", "little",
        "right", "wrong", "light", "dark", "warm", "cold", "soft", "hard",
        NULL
    };
    for (int i = 0; seed_words[i] && lang->vocab.vocab_size < mv; i++) {
        _init_special_token(&lang->vocab, lang->vocab.vocab_size,
                            seed_words[i], ed, &lang->rng_state);
        lang->vocab.tokens[lang->vocab.vocab_size].frequency = 0.01f;
        lang->vocab.vocab_size++;
    }

    /* Phonological loop */
    lang->phono_loop = nimcp_phonological_loop_create(
        lang->config.max_seq_length, ed);

    lang->lock = nimcp_mutex_create(NULL);

    /* Positional encoding — sinusoidal, no training needed */
    nimcp_pos_config_t pos_cfg;
    memset(&pos_cfg, 0, sizeof(pos_cfg));
    pos_cfg.type = NIMCP_POS_SINUSOIDAL;
    pos_cfg.config.sinusoidal.base.embedding_dim = ed;
    pos_cfg.config.sinusoidal.base.max_seq_length = lang->config.max_seq_length;
    pos_cfg.config.sinusoidal.base.cache_enabled = true;
    pos_cfg.config.sinusoidal.frequency_base = 10000.0f;
    pos_cfg.config.sinusoidal.frequency_scale = 1.0f;
    lang->pos_encoder = nimcp_pos_encoder_create(&pos_cfg);
    lang->pos_buffer = nimcp_calloc(ed, sizeof(float));
    if (lang->pos_encoder) {
        nimcp_pos_cache_precompute(lang->pos_encoder,
                                    lang->config.max_seq_length);
    }

    LOG_INFO("[%s] Created (vocab=%u seed tokens, embed_dim=%u)",
             LOG_MODULE, lang->vocab.vocab_size, ed);
    return lang;
}

void nimcp_native_language_destroy(nimcp_native_language_t* lang) {
    if (!lang) return;
    for (uint32_t i = 0; i < lang->vocab.vocab_size; i++)
        nimcp_free(lang->vocab.tokens[i].embedding);
    nimcp_free(lang->vocab.tokens);
    nimcp_free(lang->brain_to_token);
    nimcp_free(lang->token_to_brain);
    nimcp_free(lang->attention_scores);
    nimcp_free(lang->softmax_buf);
    nimcp_phonological_loop_destroy(lang->phono_loop);
    if (lang->pos_encoder) nimcp_pos_encoder_destroy(lang->pos_encoder);
    nimcp_free(lang->pos_buffer);
    if (lang->lock) nimcp_mutex_free(lang->lock);
    nimcp_free(lang);
}

/* ======================================================================== */
/* Generation                                                                 */
/* ======================================================================== */

int nimcp_language_generate(nimcp_native_language_t* lang,
    const float* brain_embedding, uint32_t embed_dim,
    char* output_text, uint32_t max_text_length)
{
    if (!lang || !brain_embedding || !output_text || max_text_length == 0) return -1;

    /* Thread safety: lock for duration of generation */
    if (lang->lock) nimcp_mutex_lock(lang->lock);

    output_text[0] = '\0';

    uint32_t ed = lang->config.embed_dim;
    uint32_t vs = lang->vocab.vocab_size;
    if (vs < 4) { /* Need at least special tokens */
        if (lang->lock) nimcp_mutex_unlock(lang->lock);
        return -1;
    }

    /* 1. Project brain embedding to token space */
    float* query = nimcp_calloc(ed, sizeof(float));
    float* original_query = nimcp_calloc(ed, sizeof(float));
    if (!query || !original_query) {
        nimcp_free(query); nimcp_free(original_query);
        if (lang->lock) nimcp_mutex_unlock(lang->lock);
        return -1;
    }

    /* Reduce brain_dim to embed_dim via projection matrix */
    uint32_t bd = embed_dim < BRAIN_DIM ? embed_dim : BRAIN_DIM;
    _matvec(lang->brain_to_token, brain_embedding, query, ed, bd);
    memcpy(original_query, query, ed * sizeof(float));

    /* Reset recent tokens */
    lang->recent_count = 0;
    if (lang->phono_loop) nimcp_phonological_loop_reset(lang->phono_loop);

    uint32_t text_pos = 0;

    /* 2. Autoregressive loop */
    for (uint32_t step = 0; step < lang->config.max_seq_length; step++) {
        /* Reset query = original projection + positional encoding for this step.
         * Without this reset, positional encodings accumulate across steps. */
        memcpy(query, original_query, ed * sizeof(float));
        if (lang->pos_encoder && lang->pos_buffer) {
            if (nimcp_pos_encode_position(lang->pos_encoder, step,
                                           lang->pos_buffer) == NIMCP_POS_SUCCESS) {
                for (uint32_t i = 0; i < ed; i++)
                    query[i] += lang->pos_buffer[i];
            }
        }

        /* a. Score each token */
        float max_score = -1e9f;
        for (uint32_t t = 4; t < vs; t++) { /* Skip special tokens */
            if (!lang->vocab.tokens[t].embedding) {
                lang->attention_scores[t] = -1e9f;
                continue;
            }
            float score = _dot(query, lang->vocab.tokens[t].embedding, ed)
                          / sqrtf((float)ed);

            /* Repetition penalty */
            for (uint32_t r = 0; r < lang->recent_count; r++) {
                if (lang->recent[r] == t) {
                    score /= lang->config.repetition_penalty;
                    break;
                }
            }

            lang->attention_scores[t] = score;
            if (score > max_score) max_score = score;
        }

        /* b. Temperature + softmax */
        /* Zero out special token slots to avoid uninitialized reads */
        for (uint32_t t = 0; t < 4 && t < vs; t++)
            lang->softmax_buf[t] = 0.0f;

        float sum_exp = 0.0f;
        for (uint32_t t = 4; t < vs; t++) {
            float s = (lang->attention_scores[t] - max_score) / lang->config.temperature;
            if (s < -20.0f) s = -20.0f;
            lang->softmax_buf[t] = expf(s);
            sum_exp += lang->softmax_buf[t];
        }
        if (sum_exp < 1e-10f) sum_exp = 1e-10f;
        for (uint32_t t = 4; t < vs; t++)
            lang->softmax_buf[t] /= sum_exp;

        /* c. Nucleus (top-p) sampling:
         *    1. Random value r in [0, 1]
         *    2. Walk through softmax in order, accumulating probability
         *    3. Select the token where cumulative probability exceeds r
         *    This naturally samples from the full distribution weighted by probability */
        float r = _randf(&lang->rng_state);
        float cum = 0.0f;
        uint32_t sampled = lang->vocab.eos_id;

        for (uint32_t t = 4; t < vs; t++) {
            cum += lang->softmax_buf[t];
            if (cum >= r) {
                sampled = t;
                break;
            }
        }

        /* d. Check EOS */
        if (sampled == lang->vocab.eos_id || sampled == lang->vocab.unk_id)
            break;

        /* e. Append token text */
        const char* tok_text = lang->vocab.tokens[sampled].text;
        uint32_t tok_len = (uint32_t)strlen(tok_text);
        if (text_pos + tok_len + 2 >= max_text_length) break;

        if (text_pos > 0) { output_text[text_pos++] = ' '; }
        memcpy(output_text + text_pos, tok_text, tok_len);
        text_pos += tok_len;
        output_text[text_pos] = '\0';

        /* f. Update original_query with context drift.
         * query is reset from original_query at the start of each step,
         * so we drift original_query to incorporate generated context. */
        if (lang->vocab.tokens[sampled].embedding) {
            for (uint32_t i = 0; i < ed; i++)
                original_query[i] = 0.7f * original_query[i] +
                                    0.3f * lang->vocab.tokens[sampled].embedding[i];
        }

        /* g. Track recent */
        if (lang->recent_count < MAX_RECENT)
            lang->recent[lang->recent_count++] = sampled;

        /* h. Phonological loop */
        if (lang->phono_loop && lang->vocab.tokens[sampled].embedding)
            nimcp_phonological_loop_push(lang->phono_loop,
                                          lang->vocab.tokens[sampled].embedding);
    }

    nimcp_free(query);
    nimcp_free(original_query);

    /* W12 KG emit: one event per generation call (utterance-level, not
     * per-token). recent_count is the number of sampled tokens this call. */
    {
        uint32_t tokens_out = lang->recent_count;
        /* Proxy for mean-score: ratio of non-special output len to cap. */
        float mean_score = (text_pos > 0 && max_text_length > 0)
            ? (float)text_pos / (float)max_text_length
            : 0.0f;
        w12_emit_native_generate_auto(tokens_out, mean_score);
    }

    if (lang->lock) nimcp_mutex_unlock(lang->lock);
    return (int)text_pos;
}

/* ======================================================================== */
/* Comprehension (encode text → brain embedding)                              */
/* ======================================================================== */

int nimcp_language_encode(nimcp_native_language_t* lang,
    const char* text, float* embedding, uint32_t max_dim)
{
    if (!lang || !text || !embedding) return -1;

    uint32_t ed = lang->config.embed_dim;
    float* avg = nimcp_calloc(ed, sizeof(float));
    if (!avg) return -1;

    /* Simple: whitespace tokenize, average embeddings */
    uint32_t count = 0;
    char buf[128];
    const char* p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        uint32_t len = 0;
        while (*p && !isspace((unsigned char)*p) && len < 127)
            buf[len++] = (char)tolower((unsigned char)*p++);
        buf[len] = '\0';

        /* Look up token — O(n) linear scan.
         * TODO: Replace with hash table for O(1) lookup when vocab
         * grows large enough to impact encode() performance. */
        uint32_t token_id = UINT32_MAX;
        for (uint32_t t = 0; t < lang->vocab.vocab_size; t++) {
            if (strcmp(lang->vocab.tokens[t].text, buf) == 0) {
                token_id = t;
                break;
            }
        }
        if (token_id != UINT32_MAX && lang->vocab.tokens[token_id].embedding) {
            for (uint32_t i = 0; i < ed; i++)
                avg[i] += lang->vocab.tokens[token_id].embedding[i];
            count++;
        }
    }

    if (count > 0) {
        for (uint32_t i = 0; i < ed; i++) avg[i] /= (float)count;
    }

    /* Project to brain space */
    uint32_t out_dim = max_dim < BRAIN_DIM ? max_dim : BRAIN_DIM;
    _matvec(lang->token_to_brain, avg, embedding, out_dim, ed);

    nimcp_free(avg);
    return (int)out_dim;
}

/* ======================================================================== */
/* Training                                                                   */
/* ======================================================================== */

int nimcp_language_train_step(nimcp_native_language_t* lang,
    const float* brain_embedding, uint32_t embed_dim,
    const char* target_text, float learning_rate)
{
    if (!lang || !brain_embedding || !target_text) return -1;

    uint32_t ed = lang->config.embed_dim;
    uint32_t bd = embed_dim < BRAIN_DIM ? embed_dim : BRAIN_DIM;

    /* Project brain embedding to token space */
    float* query = nimcp_calloc(ed, sizeof(float));
    if (!query) return -1;
    _matvec(lang->brain_to_token, brain_embedding, query, ed, bd);

    /* Tokenize target */
    float total_loss = 0.0f;
    uint32_t token_count = 0;
    char buf[128];
    const char* p = target_text;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        uint32_t len = 0;
        while (*p && !isspace((unsigned char)*p) && len < 127)
            buf[len++] = (char)tolower((unsigned char)*p++);
        buf[len] = '\0';

        /* Find target token */
        uint32_t target_id = lang->vocab.unk_id;
        for (uint32_t t = 0; t < lang->vocab.vocab_size; t++) {
            if (strcmp(lang->vocab.tokens[t].text, buf) == 0) {
                target_id = t;
                break;
            }
        }

        if (target_id == lang->vocab.unk_id) {
            /* Learn new token if vocab has room */
            if (lang->config.learn_vocabulary)
                nimcp_language_learn_token(lang, buf, brain_embedding, embed_dim);
            continue;
        }

        /* Compute cross-entropy loss for this token */
        if (lang->vocab.tokens[target_id].embedding) {
            float score = _dot(query, lang->vocab.tokens[target_id].embedding, ed);
            float loss = -logf(1.0f / (1.0f + expf(-score)) + 1e-8f);
            total_loss += loss;
            token_count++;

            /* Gradient update: push query toward target embedding */
            float grad_scale = learning_rate * (1.0f - 1.0f / (1.0f + expf(-score)));
            for (uint32_t i = 0; i < ed; i++) {
                lang->vocab.tokens[target_id].embedding[i] +=
                    grad_scale * query[i] * 0.1f;
            }

            /* Update projection matrix (outer product gradient) */
            for (uint32_t i = 0; i < ed && i < 64; i++) { /* Limit for speed */
                for (uint32_t j = 0; j < bd && j < 64; j++) {
                    lang->brain_to_token[i * bd + j] +=
                        learning_rate * 0.01f * grad_scale *
                        lang->vocab.tokens[target_id].embedding[i] *
                        brain_embedding[j];
                }
            }
        }
    }

    nimcp_free(query);

    lang->train_steps++;
    float avg_loss = token_count > 0 ? total_loss / (float)token_count : 0.0f;
    lang->ema_loss = 0.95f * lang->ema_loss + 0.05f * avg_loss;

    return 0;
}

/* ======================================================================== */
/* Vocabulary                                                                 */
/* ======================================================================== */

int nimcp_language_learn_token(nimcp_native_language_t* lang, const char* text,
    const float* context_embedding, uint32_t embed_dim)
{
    if (!lang || !text || !text[0]) return -1;
    if (lang->vocab.vocab_size >= lang->vocab.max_vocab) return -1;

    /* Check if already exists */
    for (uint32_t t = 0; t < lang->vocab.vocab_size; t++) {
        if (strcmp(lang->vocab.tokens[t].text, text) == 0) {
            lang->vocab.tokens[t].co_occurrences++;
            lang->vocab.tokens[t].frequency += 0.001f;
            /* Update embedding via EMA if context provided */
            if (context_embedding && lang->vocab.tokens[t].embedding) {
                uint32_t ed = lang->vocab.embed_dim;
                float* proj = nimcp_calloc(ed, sizeof(float));
                if (proj) {
                    uint32_t bd = embed_dim < BRAIN_DIM ? embed_dim : BRAIN_DIM;
                    _matvec(lang->brain_to_token, context_embedding, proj, ed, bd);
                    for (uint32_t i = 0; i < ed; i++)
                        lang->vocab.tokens[t].embedding[i] =
                            0.9f * lang->vocab.tokens[t].embedding[i] + 0.1f * proj[i];
                    nimcp_free(proj);
                }
            }
            return 0;
        }
    }

    /* Add new token */
    uint32_t id = lang->vocab.vocab_size;
    _init_special_token(&lang->vocab, id, text, lang->vocab.embed_dim, &lang->rng_state);
    lang->vocab.tokens[id].frequency = 0.001f;
    lang->vocab.tokens[id].co_occurrences = 1;
    lang->vocab.vocab_size++;

    /* VOCABULARY SYNC NOTE: The native language and brain tokenizer vocabularies
     * are trained from the same text labels in brain_learn_vector(). They converge
     * during training. No explicit sync mechanism is needed. */

    return 0;
}

uint32_t nimcp_language_get_vocab_size(const nimcp_native_language_t* lang) {
    return lang ? lang->vocab.vocab_size : 0;
}

int nimcp_language_save_vocabulary(const nimcp_native_language_t* lang, const char* filepath) {
    if (!lang || !filepath) return -1;
    FILE* f = fopen(filepath, "w");
    if (!f) return -1;
    fprintf(f, "NVOC %u %u\n", lang->vocab.vocab_size, lang->vocab.embed_dim);
    for (uint32_t i = 0; i < lang->vocab.vocab_size; i++) {
        const nimcp_token_t* t = &lang->vocab.tokens[i];
        fprintf(f, "%u %s %.6f %u\n", t->id, t->text, t->frequency, t->co_occurrences);
    }
    fclose(f);
    return 0;
}

int nimcp_language_load_vocabulary(nimcp_native_language_t* lang, const char* filepath) {
    if (!lang || !filepath) return -1;
    FILE* f = fopen(filepath, "r");
    if (!f) return -1;
    uint32_t vs, ed;
    if (fscanf(f, "NVOC %u %u\n", &vs, &ed) != 2) { fclose(f); return -1; }
    for (uint32_t i = 0; i < vs && i < lang->vocab.max_vocab; i++) {
        uint32_t id, co;
        char text[64];
        float freq;
        if (fscanf(f, "%u %63s %f %u\n", &id, text, &freq, &co) == 4) {
            if (id < lang->vocab.max_vocab) {
                _init_special_token(&lang->vocab, id, text, lang->vocab.embed_dim,
                                     &lang->rng_state);
                lang->vocab.tokens[id].frequency = freq;
                lang->vocab.tokens[id].co_occurrences = co;
                if (id >= lang->vocab.vocab_size)
                    lang->vocab.vocab_size = id + 1;
            }
        }
    }
    fclose(f);
    return 0;
}

/* ======================================================================== */
/* Phonological Loop                                                          */
/* ======================================================================== */

nimcp_phonological_loop_t* nimcp_phonological_loop_create(uint32_t capacity, uint32_t embed_dim) {
    nimcp_phonological_loop_t* loop = nimcp_calloc(1, sizeof(nimcp_phonological_loop_t));
    if (!loop) return NULL;
    loop->capacity = capacity;
    loop->embed_dim = embed_dim;
    loop->buffer = nimcp_calloc(capacity, sizeof(float*));
    if (!loop->buffer) { nimcp_free(loop); return NULL; }
    for (uint32_t i = 0; i < capacity; i++) {
        loop->buffer[i] = nimcp_calloc(embed_dim, sizeof(float));
        if (!loop->buffer[i]) {
            nimcp_phonological_loop_destroy(loop);
            return NULL;
        }
    }
    return loop;
}

void nimcp_phonological_loop_destroy(nimcp_phonological_loop_t* loop) {
    if (!loop) return;
    if (loop->buffer) {
        for (uint32_t i = 0; i < loop->capacity; i++)
            nimcp_free(loop->buffer[i]);
        nimcp_free(loop->buffer);
    }
    nimcp_free(loop);
}

int nimcp_phonological_loop_push(nimcp_phonological_loop_t* loop, const float* embedding) {
    if (!loop || !embedding) return -1;
    memcpy(loop->buffer[loop->head], embedding, loop->embed_dim * sizeof(float));
    loop->head = (loop->head + 1) % loop->capacity;
    if (loop->length < loop->capacity) loop->length++;
    return 0;
}

void nimcp_phonological_loop_reset(nimcp_phonological_loop_t* loop) {
    if (!loop) return;
    loop->head = 0;
    loop->length = 0;
}
