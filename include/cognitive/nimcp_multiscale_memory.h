/**
 * @file nimcp_multiscale_memory.h
 * @brief Multi-Timescale Memory Buffer — immediate, recent, and consolidated memory.
 *
 * WHAT: Three timescales of memory: immediate (last 10, full resolution),
 *       recent (last 1000, compressed), and consolidated (permanent, in weights).
 * WHY:  Human memory operates at multiple timescales; the medium-term "recent"
 *       buffer bridges the gap between working memory and long-term storage.
 * HOW:  Ring buffers with cosine-similarity search; immediate overflows into
 *       recent via chunk-averaging compression; similar recent memories merge.
 *
 * Thread-safe: No (single-threaded cognitive module).
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_MULTISCALE_MEMORY_H
#define NIMCP_MULTISCALE_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MULTISCALE_IMMEDIATE_DIM    4096   /**< Full-resolution embedding dim */
#define MULTISCALE_LABEL_LEN          64   /**< Max label length */

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t immediate_capacity;     /**< Immediate buffer slots (default 10) */
    uint32_t recent_capacity;        /**< Recent buffer slots (default 1000) */
    uint32_t compression_ratio;      /**< Recent stored at 1/N resolution (default 4) */
    float    consolidation_threshold; /**< Cosine similarity for merging (default 0.7) */
} nimcp_multiscale_config_t;

/* ============================================================================
 * Query Result
 * ============================================================================ */

typedef struct {
    float*   embedding;              /**< Pointer to embedding data (valid until next push/consolidate) */
    uint32_t embed_dim;              /**< Embedding dimensionality */
    char     label[MULTISCALE_LABEL_LEN];  /**< Memory label */
    float    similarity;             /**< Cosine similarity to query */
    float    importance;             /**< Stored importance score */
} nimcp_memory_query_result_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_multiscale_memory nimcp_multiscale_memory_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Get default configuration.
 * @return Config with immediate=10, recent=1000, compression=4, consolidation=0.7.
 */
nimcp_multiscale_config_t nimcp_multiscale_config_default(void);

/**
 * @brief Create a multi-timescale memory buffer.
 * @param config Configuration (NULL for defaults).
 * @return Handle, or NULL on failure.
 */
nimcp_multiscale_memory_t* nimcp_multiscale_create(const nimcp_multiscale_config_t* config);

/**
 * @brief Destroy buffer and free all resources. NULL-safe.
 */
void nimcp_multiscale_destroy(nimcp_multiscale_memory_t* handle);

/* ============================================================================
 * Memory Push
 * ============================================================================ */

/**
 * @brief Push a new memory into the immediate buffer.
 *
 * When the immediate buffer is full, the oldest entry is compressed
 * (by averaging chunks of compression_ratio floats) and moved to
 * the recent buffer before being overwritten.
 *
 * @param handle      Memory handle.
 * @param embedding   Full-resolution embedding vector.
 * @param embed_dim   Embedding dimensionality (max MULTISCALE_IMMEDIATE_DIM).
 * @param label       Human-readable label (max 63 chars).
 * @param importance  Importance score [0-1].
 * @return 0 on success, -1 on error.
 */
int nimcp_multiscale_push(nimcp_multiscale_memory_t* handle,
                          const float* embedding, uint32_t embed_dim,
                          const char* label, float importance);

/* ============================================================================
 * Query
 * ============================================================================ */

/**
 * @brief Search the immediate buffer by cosine similarity.
 * @param handle       Memory handle.
 * @param query        Query embedding.
 * @param query_dim    Query dimensionality.
 * @param results_out  Output array (caller-allocated).
 * @param max_results  Maximum results to return.
 * @return Number of results found.
 */
int nimcp_multiscale_query_immediate(nimcp_multiscale_memory_t* handle,
                                     const float* query, uint32_t query_dim,
                                     nimcp_memory_query_result_t* results_out,
                                     uint32_t max_results);

/**
 * @brief Search the recent buffer by cosine similarity.
 * @param handle       Memory handle.
 * @param query        Query embedding (will be compressed to match recent dim).
 * @param query_dim    Query dimensionality.
 * @param results_out  Output array (caller-allocated).
 * @param max_results  Maximum results to return.
 * @return Number of results found.
 */
int nimcp_multiscale_query_recent(nimcp_multiscale_memory_t* handle,
                                  const float* query, uint32_t query_dim,
                                  nimcp_memory_query_result_t* results_out,
                                  uint32_t max_results);

/**
 * @brief Search both immediate and recent buffers, merge by relevance.
 * @param handle       Memory handle.
 * @param query        Query embedding.
 * @param query_dim    Query dimensionality.
 * @param results_out  Output array (caller-allocated).
 * @param max_results  Maximum results to return.
 * @return Number of results found.
 */
int nimcp_multiscale_query_all(nimcp_multiscale_memory_t* handle,
                               const float* query, uint32_t query_dim,
                               nimcp_memory_query_result_t* results_out,
                               uint32_t max_results);

/* ============================================================================
 * Count Queries
 * ============================================================================ */

/**
 * @brief Get number of entries in immediate buffer.
 */
uint32_t nimcp_multiscale_get_immediate_count(const nimcp_multiscale_memory_t* handle);

/**
 * @brief Get number of entries in recent buffer.
 */
uint32_t nimcp_multiscale_get_recent_count(const nimcp_multiscale_memory_t* handle);

/* ============================================================================
 * Consolidation
 * ============================================================================ */

/**
 * @brief Merge similar recent memories to free slots.
 *
 * Scans the recent buffer for pairs with cosine similarity above
 * consolidation_threshold, averaging their embeddings and importance.
 *
 * @param handle Memory handle.
 * @return Number of memories merged (freed slots), or -1 on error.
 */
int nimcp_multiscale_consolidate(nimcp_multiscale_memory_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MULTISCALE_MEMORY_H */
