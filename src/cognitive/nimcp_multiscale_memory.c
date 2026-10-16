/**
 * @file nimcp_multiscale_memory.c
 * @brief Multi-Timescale Memory Buffer — immediate, recent, and consolidated memory.
 *
 * WHAT: Three-tier memory system with ring buffers at immediate and recent timescales.
 * WHY:  Bridges the gap between working memory (seconds) and long-term engrams.
 * HOW:  Immediate buffer overflows into compressed recent buffer; similar recent
 *       memories consolidate by averaging.
 *
 * DEPENDENCIES: None (standalone cognitive module)
 * TRAINING_IMPACT: None (inference-only memory system)
 *
 * KG INTEGRATION (W13, 2026-04-24): This module is a pure helper with no
 * brain pointer. Multi-timescale push events are emitted to the KG from the
 * brain-side callers (nimcp_brain_part_core.c inference path and
 * nimcp_brain_learning.c training path) where the brain handle is in scope.
 * See include/cognitive/kg/nimcp_wave13_metacog_kg.h.
 *
 * @author Claude Code
 * @date 2026-03
 */

#define LOG_MODULE "multiscale_memory"

#include "cognitive/nimcp_multiscale_memory.h"
#include "utils/memory/nimcp_memory_guards.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

typedef struct {
    float*   embedding;              /**< Dynamically allocated embedding */
    uint32_t embed_dim;              /**< Embedding dimensionality */
    char     label[MULTISCALE_LABEL_LEN];
    float    importance;
    bool     occupied;               /**< Slot is in use */
} memory_slot_t;

struct nimcp_multiscale_memory {
    nimcp_multiscale_config_t config;

    /* Immediate buffer (full resolution, ring) */
    memory_slot_t* immediate;
    uint32_t       immediate_count;
    uint32_t       immediate_head;   /**< Next write position */
    uint32_t       immediate_dim;    /**< Full embedding dim */

    /* Recent buffer (compressed, ring) */
    memory_slot_t* recent;
    uint32_t       recent_count;
    uint32_t       recent_head;      /**< Next write position */
    uint32_t       recent_dim;       /**< Compressed dim = immediate_dim / compression_ratio */

    /* Compression quality monitoring */
    float          last_compression_mse; /**< MSE of last spill compression (for quality tracking) */
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

/**
 * @brief Cosine similarity between two vectors of equal dimension.
 */
static float cosine_sim(const float* a, const float* b, uint32_t dim)
{
    if (dim == 0) { return 0.0f; }

    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }

    float denom = sqrtf(na) * sqrtf(nb);
    if (denom < 1e-12f) { return 0.0f; }

    return dot / denom;
}

/**
 * @brief Cosine similarity between vectors of potentially different dimensions.
 *        Uses min(a_dim, b_dim).
 */
static float cosine_sim_flex(const float* a, uint32_t a_dim,
                              const float* b, uint32_t b_dim)
{
    uint32_t dim = (a_dim < b_dim) ? a_dim : b_dim;
    return cosine_sim(a, b, dim);
}

/**
 * @brief Compress an embedding by averaging chunks of compression_ratio floats.
 * @param src        Source embedding.
 * @param src_dim    Source dimensionality.
 * @param dst        Destination (caller-allocated, dst_dim floats).
 * @param dst_dim    Destination dimensionality.
 * @param ratio      Compression ratio.
 */
static void compress_embedding(const float* src, uint32_t src_dim,
                                float* dst, uint32_t dst_dim, uint32_t ratio)
{
    memset(dst, 0, dst_dim * sizeof(float));

    for (uint32_t i = 0; i < dst_dim; i++) {
        float sum = 0.0f;
        uint32_t count = 0;
        for (uint32_t j = 0; j < ratio && (i * ratio + j) < src_dim; j++) {
            sum += src[i * ratio + j];
            count++;
        }
        dst[i] = (count > 0) ? sum / (float)count : 0.0f;
    }
}

/**
 * @brief Free a memory slot's embedding.
 */
static void slot_free(memory_slot_t* slot)
{
    if (!slot) { return; }
    if (slot->embedding) {
        nimcp_free(slot->embedding);
        slot->embedding = NULL;
    }
    slot->occupied = false;
    slot->embed_dim = 0;
}

/**
 * @brief Move the oldest immediate entry into the recent buffer (compressed).
 */
static void spill_to_recent(nimcp_multiscale_memory_t* handle, uint32_t imm_idx)
{
    memory_slot_t* imm = &handle->immediate[imm_idx];
    if (!imm->occupied) { return; }

    /* Compute compressed dim */
    uint32_t cdim = handle->recent_dim;
    if (cdim == 0) { cdim = 1; }

    /* Allocate compressed embedding */
    float* compressed = nimcp_calloc(cdim, sizeof(float));
    if (!compressed) {
        LOG_ERROR("Failed to allocate compressed embedding");
        return;
    }

    compress_embedding(imm->embedding, imm->embed_dim, compressed, cdim,
                       handle->config.compression_ratio);

    /* Compute compression loss (MSE between original and reconstructed) */
    {
        float mse = 0.0f;
        uint32_t ratio = handle->config.compression_ratio;
        for (uint32_t i = 0; i < imm->embed_dim; i++) {
            float reconstructed = compressed[i / ratio]; /* Nearest-neighbor upscale */
            float diff = imm->embedding[i] - reconstructed;
            mse += diff * diff;
        }
        mse /= (float)(imm->embed_dim > 0 ? imm->embed_dim : 1);
        handle->last_compression_mse = mse;
        /* MSE > 0.1 means significant information loss — consider higher resolution */
    }

    /* Write into recent ring buffer */
    uint32_t slot_idx = handle->recent_head;
    memory_slot_t* r = &handle->recent[slot_idx];

    /* Free old contents if occupied */
    slot_free(r);

    r->embedding = compressed;
    r->embed_dim = cdim;
    strncpy(r->label, imm->label, MULTISCALE_LABEL_LEN - 1);
    r->label[MULTISCALE_LABEL_LEN - 1] = '\0';
    r->importance = imm->importance;
    r->occupied   = true;

    handle->recent_head = (handle->recent_head + 1) % handle->config.recent_capacity;
    if (handle->recent_count < handle->config.recent_capacity) {
        handle->recent_count++;
    }

    LOG_DEBUG("Spilled '%s' to recent buffer (compressed %u -> %u)",
              r->label, imm->embed_dim, cdim);
}

/* ============================================================================
 * Simple insertion sort for query results (descending by similarity).
 * ============================================================================ */

static void insert_result(nimcp_memory_query_result_t* results, uint32_t* count,
                           uint32_t max_results,
                           float* embedding, uint32_t embed_dim,
                           const char* label, float similarity, float importance)
{
    /* Find insertion point */
    uint32_t pos = *count;
    for (uint32_t i = 0; i < *count; i++) {
        if (similarity > results[i].similarity) {
            pos = i;
            break;
        }
    }

    if (pos >= max_results) { return; }

    /* Shift down */
    uint32_t end = (*count < max_results) ? *count : max_results - 1;
    for (uint32_t i = end; i > pos; i--) {
        results[i] = results[i - 1];
    }

    results[pos].embedding  = embedding;
    results[pos].embed_dim  = embed_dim;
    strncpy(results[pos].label, label, MULTISCALE_LABEL_LEN - 1);
    results[pos].label[MULTISCALE_LABEL_LEN - 1] = '\0';
    results[pos].similarity = similarity;
    results[pos].importance = importance;

    if (*count < max_results) { (*count)++; }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_multiscale_config_t nimcp_multiscale_config_default(void)
{
    nimcp_multiscale_config_t cfg;
    cfg.immediate_capacity     = 10;
    cfg.recent_capacity        = 1000;
    cfg.compression_ratio      = 4;
    cfg.consolidation_threshold = 0.7f;
    return cfg;
}

nimcp_multiscale_memory_t* nimcp_multiscale_create(const nimcp_multiscale_config_t* config)
{
    nimcp_multiscale_config_t cfg = config ? *config : nimcp_multiscale_config_default();

    if (cfg.immediate_capacity == 0) {
        LOG_ERROR("immediate_capacity must be > 0");
        return NULL;
    }
    if (cfg.recent_capacity == 0) {
        LOG_ERROR("recent_capacity must be > 0");
        return NULL;
    }
    if (cfg.compression_ratio == 0) {
        LOG_WARN("compression_ratio was 0, defaulting to 4");
        cfg.compression_ratio = 4;
    }

    nimcp_multiscale_memory_t* handle = nimcp_calloc(1, sizeof(*handle));
    if (!handle) {
        LOG_ERROR("Failed to allocate multiscale memory");
        return NULL;
    }

    handle->config = cfg;

    handle->immediate = nimcp_calloc(cfg.immediate_capacity, sizeof(memory_slot_t));
    if (!handle->immediate) {
        LOG_ERROR("Failed to allocate immediate buffer");
        nimcp_free(handle);
        return NULL;
    }

    handle->recent = nimcp_calloc(cfg.recent_capacity, sizeof(memory_slot_t));
    if (!handle->recent) {
        LOG_ERROR("Failed to allocate recent buffer");
        nimcp_free(handle->immediate);
        nimcp_free(handle);
        return NULL;
    }

    handle->immediate_count = 0;
    handle->immediate_head  = 0;
    handle->immediate_dim   = MULTISCALE_IMMEDIATE_DIM;

    handle->recent_count = 0;
    handle->recent_head  = 0;
    handle->recent_dim   = MULTISCALE_IMMEDIATE_DIM / cfg.compression_ratio;

    LOG_INFO("Multiscale memory created: immediate=%u, recent=%u, "
             "compress=%u:1, recent_dim=%u",
             cfg.immediate_capacity, cfg.recent_capacity,
             cfg.compression_ratio, handle->recent_dim);

    return handle;
}

void nimcp_multiscale_destroy(nimcp_multiscale_memory_t* handle)
{
    if (!handle) { return; }

    /* Free all immediate slot embeddings */
    if (handle->immediate) {
        for (uint32_t i = 0; i < handle->config.immediate_capacity; i++) {
            slot_free(&handle->immediate[i]);
        }
        nimcp_free(handle->immediate);
    }

    /* Free all recent slot embeddings */
    if (handle->recent) {
        for (uint32_t i = 0; i < handle->config.recent_capacity; i++) {
            slot_free(&handle->recent[i]);
        }
        nimcp_free(handle->recent);
    }

    nimcp_free(handle);

    LOG_DEBUG("Multiscale memory destroyed");
}

/* ============================================================================
 * Memory Push
 * ============================================================================ */

int nimcp_multiscale_push(nimcp_multiscale_memory_t* handle,
                          const float* embedding, uint32_t embed_dim,
                          const char* label, float importance)
{
    if (!handle)    { return -1; }
    if (!embedding) { return -1; }
    if (embed_dim == 0 || embed_dim > MULTISCALE_IMMEDIATE_DIM) { return -1; }

    /* If immediate buffer is full, spill oldest to recent */
    if (handle->immediate_count >= handle->config.immediate_capacity) {
        spill_to_recent(handle, handle->immediate_head);
    }

    /* Write into immediate ring buffer */
    uint32_t slot_idx = handle->immediate_head;
    memory_slot_t* s = &handle->immediate[slot_idx];

    /* Free old contents if occupied */
    slot_free(s);

    s->embedding = nimcp_calloc(embed_dim, sizeof(float));
    if (!s->embedding) {
        LOG_ERROR("Failed to allocate immediate embedding (%u floats)", embed_dim);
        return -1;
    }

    memcpy(s->embedding, embedding, embed_dim * sizeof(float));
    s->embed_dim = embed_dim;

    if (label) {
        strncpy(s->label, label, MULTISCALE_LABEL_LEN - 1);
        s->label[MULTISCALE_LABEL_LEN - 1] = '\0';
    } else {
        s->label[0] = '\0';
    }

    s->importance = importance;
    s->occupied   = true;

    /* Advance head — but skip over high-importance memories (emotional protection).
     * Memories with importance > 0.7 resist eviction, like flashbulb memories.
     * If all slots are high-importance, evict the least important one. */
    uint32_t next = (handle->immediate_head + 1) % handle->config.immediate_capacity;
    if (handle->immediate_count >= handle->config.immediate_capacity) {
        /* Buffer full — find a slot to evict */
        uint32_t evict_slot = next;
        float min_importance = 2.0f;
        uint32_t attempts = 0;
        while (handle->immediate[evict_slot].occupied &&
               handle->immediate[evict_slot].importance > 0.7f &&
               attempts < handle->config.immediate_capacity) {
            /* This slot is emotionally protected — try next */
            evict_slot = (evict_slot + 1) % handle->config.immediate_capacity;
            attempts++;
        }
        if (attempts >= handle->config.immediate_capacity) {
            /* All slots are high-importance — evict lowest */
            for (uint32_t j = 0; j < handle->config.immediate_capacity; j++) {
                if (handle->immediate[j].importance < min_importance) {
                    min_importance = handle->immediate[j].importance;
                    evict_slot = j;
                }
            }
        }
        next = evict_slot;
    }
    handle->immediate_head = next;
    if (handle->immediate_count < handle->config.immediate_capacity) {
        handle->immediate_count++;
    }

    LOG_DEBUG("Pushed '%s' to immediate buffer (dim=%u, importance=%.3f)",
              s->label, embed_dim, importance);

    return 0;
}

/* ============================================================================
 * Query — Immediate
 * ============================================================================ */

int nimcp_multiscale_query_immediate(nimcp_multiscale_memory_t* handle,
                                     const float* query, uint32_t query_dim,
                                     nimcp_memory_query_result_t* results_out,
                                     uint32_t max_results)
{
    if (!handle)      { return 0; }
    if (!query)       { return 0; }
    if (!results_out) { return 0; }
    if (max_results == 0) { return 0; }

    uint32_t found = 0;

    for (uint32_t i = 0; i < handle->config.immediate_capacity; i++) {
        memory_slot_t* s = &handle->immediate[i];
        if (!s->occupied) { continue; }

        float sim = cosine_sim_flex(query, query_dim, s->embedding, s->embed_dim);

        insert_result(results_out, &found, max_results,
                      s->embedding, s->embed_dim,
                      s->label, sim, s->importance);
    }

    return (int)found;
}

/* ============================================================================
 * Query — Recent
 * ============================================================================ */

int nimcp_multiscale_query_recent(nimcp_multiscale_memory_t* handle,
                                  const float* query, uint32_t query_dim,
                                  nimcp_memory_query_result_t* results_out,
                                  uint32_t max_results)
{
    if (!handle)      { return 0; }
    if (!query)       { return 0; }
    if (!results_out) { return 0; }
    if (max_results == 0) { return 0; }

    /* Compress query to match recent buffer dimensionality */
    uint32_t cdim = handle->recent_dim;
    float* cquery = nimcp_calloc(cdim, sizeof(float));
    if (!cquery) { return 0; }

    compress_embedding(query, query_dim, cquery, cdim, handle->config.compression_ratio);

    uint32_t found = 0;

    for (uint32_t i = 0; i < handle->config.recent_capacity; i++) {
        memory_slot_t* s = &handle->recent[i];
        if (!s->occupied) { continue; }

        float sim = cosine_sim(cquery, s->embedding, cdim);

        insert_result(results_out, &found, max_results,
                      s->embedding, s->embed_dim,
                      s->label, sim, s->importance);
    }

    nimcp_free(cquery);

    return (int)found;
}

/* ============================================================================
 * Query — All (merge immediate + recent by relevance)
 * ============================================================================ */

int nimcp_multiscale_query_all(nimcp_multiscale_memory_t* handle,
                               const float* query, uint32_t query_dim,
                               nimcp_memory_query_result_t* results_out,
                               uint32_t max_results)
{
    if (!handle)      { return 0; }
    if (!query)       { return 0; }
    if (!results_out) { return 0; }
    if (max_results == 0) { return 0; }

    uint32_t found = 0;

    /* Search immediate buffer (full resolution) */
    for (uint32_t i = 0; i < handle->config.immediate_capacity; i++) {
        memory_slot_t* s = &handle->immediate[i];
        if (!s->occupied) { continue; }

        float sim = cosine_sim_flex(query, query_dim, s->embedding, s->embed_dim);

        insert_result(results_out, &found, max_results,
                      s->embedding, s->embed_dim,
                      s->label, sim, s->importance);
    }

    /* Search recent buffer (compressed query) */
    uint32_t cdim = handle->recent_dim;
    float* cquery = nimcp_calloc(cdim, sizeof(float));
    if (!cquery) { return (int)found; }

    compress_embedding(query, query_dim, cquery, cdim, handle->config.compression_ratio);

    for (uint32_t i = 0; i < handle->config.recent_capacity; i++) {
        memory_slot_t* s = &handle->recent[i];
        if (!s->occupied) { continue; }

        float sim = cosine_sim(cquery, s->embedding, cdim);

        insert_result(results_out, &found, max_results,
                      s->embedding, s->embed_dim,
                      s->label, sim, s->importance);
    }

    nimcp_free(cquery);

    return (int)found;
}

/* ============================================================================
 * Count Queries
 * ============================================================================ */

uint32_t nimcp_multiscale_get_immediate_count(const nimcp_multiscale_memory_t* handle)
{
    if (!handle) { return 0; }
    return handle->immediate_count;
}

uint32_t nimcp_multiscale_get_recent_count(const nimcp_multiscale_memory_t* handle)
{
    if (!handle) { return 0; }
    return handle->recent_count;
}

/* ============================================================================
 * Consolidation
 * ============================================================================ */

int nimcp_multiscale_consolidate(nimcp_multiscale_memory_t* handle)
{
    if (!handle) { return -1; }

    uint32_t merged = 0;
    float threshold = handle->config.consolidation_threshold;
    uint32_t cap = handle->config.recent_capacity;

    /* O(n^2) pairwise scan — acceptable for 1000-entry buffer */
    for (uint32_t i = 0; i < cap; i++) {
        memory_slot_t* a = &handle->recent[i];
        if (!a->occupied) { continue; }

        for (uint32_t j = i + 1; j < cap; j++) {
            memory_slot_t* b = &handle->recent[j];
            if (!b->occupied) { continue; }
            if (a->embed_dim != b->embed_dim) { continue; }

            float sim = cosine_sim(a->embedding, b->embedding, a->embed_dim);

            if (sim >= threshold) {
                /* Merge b into a: average embeddings and importance */
                for (uint32_t k = 0; k < a->embed_dim; k++) {
                    a->embedding[k] = 0.5f * a->embedding[k] + 0.5f * b->embedding[k];
                }
                a->importance = 0.5f * a->importance + 0.5f * b->importance;

                /* Free b */
                slot_free(b);
                handle->recent_count--;
                merged++;

                LOG_DEBUG("Merged recent memory '%s' into '%s' (sim=%.3f)",
                          b->label, a->label, sim);
            }
        }
    }

    if (merged > 0) {
        LOG_INFO("Consolidation merged %u recent memories", merged);
    }

    return (int)merged;
}
