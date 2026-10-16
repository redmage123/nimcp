/**
 * @file nimcp_embedding.c
 * @brief Learned embedding layer — full implementation
 *
 * WHAT: Embedding matrix mapping token IDs to dense float vectors
 * WHY:  Bridges discrete tokenizer output to continuous neural representation
 * HOW:  Row-major weight matrix with Xavier init, gradient accumulation,
 *        cosine similarity, and brute-force nearest-neighbor search
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include "generation/nimcp_embedding.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/geometry/nimcp_lie_group.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*=============================================================================
 * Internal Structure Definition
 *===========================================================================*/

struct embedding_layer {
    float*   weights;       /**< [vocab_size * embed_dim] row-major weight matrix */
    float*   grad_accum;    /**< [vocab_size * embed_dim] gradient accumulator */
    uint32_t vocab_size;    /**< Number of vocabulary tokens */
    uint32_t embed_dim;     /**< Dimensionality of embedding vectors */
    uint32_t update_count;  /**< Number of gradient update steps applied */
    bool     frozen;        /**< If true, backward/update are no-ops */
    uint32_t rng_state;     /**< Per-instance xorshift32 PRNG state */
};

/*=============================================================================
 * Internal Helpers
 *===========================================================================*/

/**
 * WHAT: Per-instance xorshift32 PRNG step
 * WHY:  Thread-safe random number generation without global state
 * HOW:  xorshift32 with state stored per embedding instance
 */
static uint32_t embedding_xorshift32(uint32_t* state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/**
 * WHAT: Return a float uniformly distributed in [-1.0, 1.0]
 * WHY:  Xavier initialization needs uniform random values
 * HOW:  Per-instance PRNG avoids global state race conditions
 */
static float rand_uniform_instance(uint32_t* rng_state)
{
    return ((float)embedding_xorshift32(rng_state) / (float)UINT32_MAX) * 2.0f - 1.0f;
}

/**
 * WHAT: Seed a per-instance RNG state
 * WHY:  Each embedding gets its own PRNG to avoid thread contention
 * HOW:  Mix time, pointer address, and a constant for entropy
 */
static uint32_t embedding_seed_rng(const void* instance_ptr)
{
    uint32_t seed = (uint32_t)time(NULL) ^ 0xDEADBEEF;
    seed ^= (uint32_t)(uintptr_t)instance_ptr;
    if (seed == 0) seed = 1;
    return seed;
}

/**
 * WHAT: Compute dot product of two float vectors
 * WHY:  Used for cosine similarity computation
 */
static float vec_dot(const float* a, const float* b, uint32_t dim)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * WHAT: Compute L2 norm of a float vector
 * WHY:  Used for cosine similarity normalization
 */
static float vec_norm(const float* v, uint32_t dim)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

/*=============================================================================
 * Public API — Configuration
 *===========================================================================*/

embedding_config_t embedding_default_config(uint32_t vocab_size, uint32_t embed_dim)
{
    embedding_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.vocab_size = vocab_size;
    cfg.embed_dim = embed_dim;
    /* Xavier/Glorot uniform initialization scale */
    if (vocab_size > 0 && embed_dim > 0) {
        cfg.init_scale = sqrtf(2.0f / (float)(vocab_size + embed_dim));
    } else {
        cfg.init_scale = 0.01f;
    }
    cfg.freeze = false;
    return cfg;
}

/*=============================================================================
 * Public API — Lifecycle
 *===========================================================================*/

embedding_layer_t* embedding_create(const embedding_config_t* config)
{
    if (!config) {
        LOG_ERROR("embedding_create: NULL config");
        return NULL;
    }
    if (config->vocab_size == 0 || config->embed_dim == 0) {
        LOG_ERROR("embedding_create: vocab_size=%u embed_dim=%u must be > 0",
                  config->vocab_size, config->embed_dim);
        return NULL;
    }

    /* Check for overflow in allocation size */
    size_t total_floats = (size_t)config->vocab_size * (size_t)config->embed_dim;
    if (total_floats / config->vocab_size != config->embed_dim) {
        LOG_ERROR("embedding_create: overflow computing %u * %u",
                  config->vocab_size, config->embed_dim);
        return NULL;
    }

    embedding_layer_t* emb = (embedding_layer_t*)nimcp_calloc(1, sizeof(embedding_layer_t));
    if (!emb) {
        LOG_ERROR("embedding_create: struct allocation failed");
        return NULL;
    }

    emb->vocab_size = config->vocab_size;
    emb->embed_dim = config->embed_dim;
    emb->frozen = config->freeze;
    emb->update_count = 0;

    /* Allocate weight matrix */
    emb->weights = (float*)nimcp_calloc(total_floats, sizeof(float));
    if (!emb->weights) {
        LOG_ERROR("embedding_create: weights allocation failed (%zu floats)", total_floats);
        nimcp_free(emb);
        return NULL;
    }

    /* Allocate gradient accumulator */
    emb->grad_accum = (float*)nimcp_calloc(total_floats, sizeof(float));
    if (!emb->grad_accum) {
        LOG_ERROR("embedding_create: grad_accum allocation failed");
        nimcp_free(emb->weights);
        nimcp_free(emb);
        return NULL;
    }

    /* Xavier/Glorot uniform initialization */
    float scale = config->init_scale;
    if (scale <= 0.0f) {
        scale = sqrtf(2.0f / (float)(config->vocab_size + config->embed_dim));
    }

    emb->rng_state = embedding_seed_rng(emb);
    for (size_t i = 0; i < total_floats; i++) {
        emb->weights[i] = rand_uniform_instance(&emb->rng_state) * scale;
    }

    LOG_INFO("embedding_create: %u x %u, scale=%.6f, frozen=%s",
             emb->vocab_size, emb->embed_dim, scale, emb->frozen ? "true" : "false");

    return emb;
}

void embedding_destroy(embedding_layer_t* emb)
{
    if (!emb) return;

    nimcp_free(emb->weights);
    nimcp_free(emb->grad_accum);
    nimcp_free(emb);
}

/*=============================================================================
 * Public API — Forward Pass
 *===========================================================================*/

int embedding_lookup(const embedding_layer_t* emb, uint32_t token_id, float* output)
{
    if (!emb || !output) return -1;
    if (token_id >= emb->vocab_size) {
        /* UINT32_MAX is the tokenizer's "not found" sentinel — fires on every
         * OOV token in the hot path. Logging each one floods stderr (~117
         * lines/sec observed in 2026-05-18 stage-2 trace, 13MB/h log spam)
         * AND serializes the brain thread on stderr flush. Silent fast-path
         * for the sentinel; rate-limit every other out-of-range case to
         * once per ~16K calls so genuine bugs still surface. */
        if (token_id != UINT32_MAX) {
            static _Atomic uint64_t _emb_warn_ctr = 0;
            uint64_t n = atomic_fetch_add(&_emb_warn_ctr, 1u);
            if ((n & 0x3FFFu) == 0u) {
                LOG_WARN("embedding_lookup: token_id %u >= vocab_size %u (sample 1/16384, total=%llu)",
                         token_id, emb->vocab_size, (unsigned long long)(n + 1u));
            }
        }
        return -1;
    }

    const float* row = emb->weights + (size_t)token_id * emb->embed_dim;
    memcpy(output, row, emb->embed_dim * sizeof(float));
    return 0;
}

int embedding_lookup_batch(const embedding_layer_t* emb, const uint32_t* token_ids,
                           uint32_t count, float* output)
{
    if (!emb || !token_ids || !output) return -1;
    if (count == 0) return 0;

    for (uint32_t i = 0; i < count; i++) {
        if (token_ids[i] >= emb->vocab_size) {
            /* Same silencing policy as embedding_lookup — sentinel is silent,
             * other OOR cases sampled 1/16384. */
            if (token_ids[i] != UINT32_MAX) {
                static _Atomic uint64_t _emb_batch_warn_ctr = 0;
                uint64_t n = atomic_fetch_add(&_emb_batch_warn_ctr, 1u);
                if ((n & 0x3FFFu) == 0u) {
                    LOG_WARN("embedding_lookup_batch: token_id[%u]=%u >= vocab_size %u (sample 1/16384, total=%llu)",
                             i, token_ids[i], emb->vocab_size,
                             (unsigned long long)(n + 1u));
                }
            }
            return -1;
        }
        const float* row = emb->weights + (size_t)token_ids[i] * emb->embed_dim;
        memcpy(output + (size_t)i * emb->embed_dim, row, emb->embed_dim * sizeof(float));
    }

    return 0;
}

nimcp_tensor_t* embedding_forward(const embedding_layer_t* emb,
                                  const uint32_t* token_ids, uint32_t count)
{
    if (!emb || !token_ids || count == 0) return NULL;

    /* Create 2D tensor [count, embed_dim] */
    uint32_t dims[2] = { count, emb->embed_dim };
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    if (!tensor) {
        LOG_ERROR("embedding_forward: tensor creation failed");
        return NULL;
    }

    float* data = (float*)nimcp_tensor_data(tensor);
    if (!data) {
        nimcp_tensor_destroy(tensor);
        return NULL;
    }

    /* Fill tensor with embedding lookups */
    for (uint32_t i = 0; i < count; i++) {
        if (token_ids[i] >= emb->vocab_size) {
            LOG_WARN("embedding_forward: token_id[%u]=%u out of range, using zeros",
                     i, token_ids[i]);
            memset(data + (size_t)i * emb->embed_dim, 0, emb->embed_dim * sizeof(float));
        } else {
            const float* row = emb->weights + (size_t)token_ids[i] * emb->embed_dim;
            memcpy(data + (size_t)i * emb->embed_dim, row, emb->embed_dim * sizeof(float));
        }
    }

    return tensor;
}

/*=============================================================================
 * Public API — Backward Pass / Training
 *===========================================================================*/

int embedding_backward(embedding_layer_t* emb, uint32_t token_id, const float* grad_output)
{
    if (!emb || !grad_output) return -1;
    if (emb->frozen) return -1;
    if (token_id >= emb->vocab_size) {
        LOG_WARN("embedding_backward: token_id %u >= vocab_size %u", token_id, emb->vocab_size);
        return -1;
    }

    /* Accumulate gradient: grad_accum[token_id] += grad_output */
    float* grad_row = emb->grad_accum + (size_t)token_id * emb->embed_dim;
    for (uint32_t i = 0; i < emb->embed_dim; i++) {
        grad_row[i] += grad_output[i];
    }

    return 0;
}

int embedding_update(embedding_layer_t* emb, float learning_rate)
{
    if (!emb) return -1;
    if (emb->frozen) return -1;
    if (learning_rate <= 0.0f) {
        LOG_WARN("embedding_update: learning_rate %.6f must be positive", (double)learning_rate);
        return -1;
    }

    /*
     * SGD update: weights -= learning_rate * grad_accum
     * Then zero the gradient accumulator.
     */
    size_t total = (size_t)emb->vocab_size * emb->embed_dim;
    for (size_t i = 0; i < total; i++) {
        emb->weights[i] -= learning_rate * emb->grad_accum[i];
    }

    /* Zero gradients after update */
    memset(emb->grad_accum, 0, total * sizeof(float));

    emb->update_count++;
    return 0;
}

void embedding_zero_grad(embedding_layer_t* emb)
{
    if (!emb || !emb->grad_accum) return;
    size_t total = (size_t)emb->vocab_size * emb->embed_dim;
    memset(emb->grad_accum, 0, total * sizeof(float));
}

/*=============================================================================
 * Public API — Properties
 *===========================================================================*/

uint32_t embedding_get_dim(const embedding_layer_t* emb)
{
    if (!emb) return 0;
    return emb->embed_dim;
}

uint32_t embedding_get_vocab_size(const embedding_layer_t* emb)
{
    if (!emb) return 0;
    return emb->vocab_size;
}

void embedding_set_frozen(embedding_layer_t* emb, bool frozen)
{
    if (!emb) return;
    emb->frozen = frozen;
}

bool embedding_is_frozen(const embedding_layer_t* emb)
{
    if (!emb) return false;
    return emb->frozen;
}

/*=============================================================================
 * Public API — Similarity / Search
 *===========================================================================*/

float embedding_cosine_similarity(const embedding_layer_t* emb, uint32_t id_a, uint32_t id_b)
{
    if (!emb) return 0.0f;
    if (id_a >= emb->vocab_size || id_b >= emb->vocab_size) return 0.0f;

    const float* vec_a = emb->weights + (size_t)id_a * emb->embed_dim;
    const float* vec_b = emb->weights + (size_t)id_b * emb->embed_dim;

    float dot = vec_dot(vec_a, vec_b, emb->embed_dim);
    float norm_a = vec_norm(vec_a, emb->embed_dim);
    float norm_b = vec_norm(vec_b, emb->embed_dim);

    /* Guard against zero-norm vectors */
    if (norm_a < 1e-12f || norm_b < 1e-12f) return 0.0f;

    return dot / (norm_a * norm_b);
}

int embedding_nearest_neighbors(const embedding_layer_t* emb, const float* query_vector,
                                uint32_t k, uint32_t* result_ids, float* result_scores)
{
    if (!emb || !query_vector || !result_ids || !result_scores) return -1;
    if (k == 0) return 0;
    if (k > emb->vocab_size) k = emb->vocab_size;

    float query_norm = vec_norm(query_vector, emb->embed_dim);
    if (query_norm < 1e-12f) {
        /* Zero query — return first k tokens with score 0 */
        for (uint32_t i = 0; i < k; i++) {
            result_ids[i] = i;
            result_scores[i] = 0.0f;
        }
        return 0;
    }

    /*
     * Brute-force nearest neighbor search via cosine similarity.
     * Maintain a sorted top-k list (insertion sort on small k).
     * For very large vocabularies, a more efficient method (e.g., HNSW)
     * would be needed, but brute force is correct and sufficient for
     * typical vocabulary sizes (< 100K).
     */

    /* Initialize results with -infinity scores */
    for (uint32_t i = 0; i < k; i++) {
        result_ids[i] = 0;
        result_scores[i] = -2.0f; /* Below any possible cosine similarity */
    }

    for (uint32_t t = 0; t < emb->vocab_size; t++) {
        const float* vec_t = emb->weights + (size_t)t * emb->embed_dim;
        float norm_t = vec_norm(vec_t, emb->embed_dim);

        float sim;
        if (norm_t < 1e-12f) {
            sim = 0.0f;
        } else {
            sim = vec_dot(query_vector, vec_t, emb->embed_dim) / (query_norm * norm_t);
        }

        /* Check if this token beats the smallest score in our top-k */
        if (sim > result_scores[k - 1]) {
            /* Insert into sorted position (descending order) */
            result_ids[k - 1] = t;
            result_scores[k - 1] = sim;

            /* Bubble up to maintain sorted order */
            for (uint32_t j = k - 1; j > 0; j--) {
                if (result_scores[j] > result_scores[j - 1]) {
                    /* Swap */
                    float tmp_score = result_scores[j];
                    result_scores[j] = result_scores[j - 1];
                    result_scores[j - 1] = tmp_score;

                    uint32_t tmp_id = result_ids[j];
                    result_ids[j] = result_ids[j - 1];
                    result_ids[j - 1] = tmp_id;
                } else {
                    break;
                }
            }
        }
    }

    return 0;
}

/*=============================================================================
 * Public API — Serialization
 *===========================================================================*/

int embedding_save(const embedding_layer_t* emb, const char* path)
{
    if (!emb || !path) return -1;

    FILE* f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("embedding_save: cannot open '%s' for writing", path);
        return -1;
    }

    /* Header */
    uint32_t magic = EMBEDDING_MAGIC;
    uint32_t version = EMBEDDING_VERSION;

    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);
    fwrite(&emb->vocab_size, sizeof(uint32_t), 1, f);
    fwrite(&emb->embed_dim, sizeof(uint32_t), 1, f);
    fwrite(&emb->update_count, sizeof(uint32_t), 1, f);

    uint32_t frozen_flag = emb->frozen ? 1 : 0;
    fwrite(&frozen_flag, sizeof(uint32_t), 1, f);

    /* Weight matrix */
    size_t total = (size_t)emb->vocab_size * emb->embed_dim;
    size_t written = fwrite(emb->weights, sizeof(float), total, f);
    if (written != total) {
        LOG_ERROR("embedding_save: short write (%zu / %zu floats)", written, total);
        fclose(f);
        return -1;
    }

    fclose(f);
    LOG_INFO("embedding_save: saved %u x %u embeddings (%zu bytes) to '%s'",
             emb->vocab_size, emb->embed_dim, total * sizeof(float), path);
    return 0;
}

embedding_layer_t* embedding_load(const char* path)
{
    if (!path) return NULL;

    FILE* f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR("embedding_load: cannot open '%s' for reading", path);
        return NULL;
    }

    /* Read header */
    uint32_t magic, version, vocab_size, embed_dim, update_count, frozen_flag;

    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != EMBEDDING_MAGIC) {
        LOG_ERROR("embedding_load: invalid magic in '%s'", path);
        fclose(f);
        return NULL;
    }
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version != EMBEDDING_VERSION) {
        LOG_ERROR("embedding_load: unsupported version %u in '%s'", version, path);
        fclose(f);
        return NULL;
    }

    if (fread(&vocab_size, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&embed_dim, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&update_count, sizeof(uint32_t), 1, f) != 1) goto read_error;
    if (fread(&frozen_flag, sizeof(uint32_t), 1, f) != 1) goto read_error;

    if (vocab_size == 0 || embed_dim == 0) {
        LOG_ERROR("embedding_load: invalid dimensions %u x %u in '%s'",
                  vocab_size, embed_dim, path);
        fclose(f);
        return NULL;
    }

    /* Check for overflow */
    size_t total = (size_t)vocab_size * embed_dim;
    if (total / vocab_size != embed_dim) {
        LOG_ERROR("embedding_load: overflow %u * %u in '%s'", vocab_size, embed_dim, path);
        fclose(f);
        return NULL;
    }

    /* Allocate the embedding layer */
    embedding_layer_t* emb = (embedding_layer_t*)nimcp_calloc(1, sizeof(embedding_layer_t));
    if (!emb) goto read_error;

    emb->vocab_size = vocab_size;
    emb->embed_dim = embed_dim;
    emb->update_count = update_count;
    emb->frozen = (frozen_flag != 0);

    emb->weights = (float*)nimcp_calloc(total, sizeof(float));
    if (!emb->weights) {
        nimcp_free(emb);
        goto read_error;
    }

    emb->grad_accum = (float*)nimcp_calloc(total, sizeof(float));
    if (!emb->grad_accum) {
        nimcp_free(emb->weights);
        nimcp_free(emb);
        goto read_error;
    }

    /* Read weight matrix */
    size_t read_count = fread(emb->weights, sizeof(float), total, f);
    if (read_count != total) {
        LOG_ERROR("embedding_load: short read (%zu / %zu floats) from '%s'",
                  read_count, total, path);
        embedding_destroy(emb);
        fclose(f);
        return NULL;
    }

    fclose(f);
    LOG_INFO("embedding_load: loaded %u x %u embeddings from '%s'",
             emb->vocab_size, emb->embed_dim, path);
    return emb;

read_error:
    LOG_ERROR("embedding_load: read error in '%s'", path);
    fclose(f);
    return NULL;
}

/*=============================================================================
 * Public API — Geodesic Interpolation
 *===========================================================================*/

int embedding_geodesic_interpolate(const float* vec_a, const float* vec_b,
                                    float t, float* result, uint32_t dim)
{
    if (!vec_a || !vec_b || !result || dim == 0) return -1;

    /* Clamp t to [0,1] */
    if (t <= 0.0f) { memcpy(result, vec_a, dim * sizeof(float)); return 0; }
    if (t >= 1.0f) { memcpy(result, vec_b, dim * sizeof(float)); return 0; }

    /* SLERP on the unit hypersphere: geodesic interpolation respects the
     * spherical geometry of normalized embedding vectors.
     *
     * slerp(a, b, t) = sin((1-t)*omega)/sin(omega) * a + sin(t*omega)/sin(omega) * b
     * where omega = acos(a . b / (||a|| ||b||))
     */
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += vec_a[i] * vec_b[i];
        norm_a += vec_a[i] * vec_a[i];
        norm_b += vec_b[i] * vec_b[i];
    }
    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 1e-10f || norm_b < 1e-10f) {
        /* Degenerate: linear interpolation */
        for (uint32_t i = 0; i < dim; i++) {
            result[i] = (1.0f - t) * vec_a[i] + t * vec_b[i];
        }
        return 0;
    }

    float cos_omega = dot / (norm_a * norm_b);
    /* Clamp to avoid NaN from acos */
    if (cos_omega > 1.0f) cos_omega = 1.0f;
    if (cos_omega < -1.0f) cos_omega = -1.0f;

    float omega = acosf(cos_omega);

    if (omega < 1e-6f) {
        /* Vectors nearly parallel: linear interpolation */
        for (uint32_t i = 0; i < dim; i++) {
            result[i] = (1.0f - t) * vec_a[i] + t * vec_b[i];
        }
        return 0;
    }

    float sin_omega = sinf(omega);
    float coeff_a = sinf((1.0f - t) * omega) / sin_omega;
    float coeff_b = sinf(t * omega) / sin_omega;

    /* Interpolate preserving norms: scale to interpolated magnitude */
    float target_norm = (1.0f - t) * norm_a + t * norm_b;
    for (uint32_t i = 0; i < dim; i++) {
        /* SLERP on normalized vectors, then scale to interpolated norm */
        result[i] = coeff_a * (vec_a[i] / norm_a) + coeff_b * (vec_b[i] / norm_b);
    }

    /* Renormalize and scale to target norm */
    float result_norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        result_norm += result[i] * result[i];
    }
    result_norm = sqrtf(result_norm);
    if (result_norm > 1e-10f) {
        float scale = target_norm / result_norm;
        for (uint32_t i = 0; i < dim; i++) {
            result[i] *= scale;
        }
    }

    return 0;
}
