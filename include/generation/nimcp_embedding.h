/**
 * @file nimcp_embedding.h
 * @brief Learned embedding layer for mapping token IDs to dense vectors
 *
 * WHAT: Embedding matrix that maps discrete token IDs to continuous vectors
 * WHY:  Neural networks operate on continuous representations; this layer
 *        bridges the discrete tokenizer output to dense neural input
 * HOW:  Lookup table (weights matrix) with gradient accumulation for training,
 *        Xavier initialization, cosine similarity, nearest-neighbor search
 *
 * ARCHITECTURE:
 *   token_id (uint32)
 *       |
 *       v
 *   weights[token_id * embed_dim ... (token_id+1) * embed_dim - 1]
 *       |
 *       v
 *   float[embed_dim] embedding vector
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#ifndef NIMCP_EMBEDDING_H
#define NIMCP_EMBEDDING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward declaration to avoid full tensor header dependency */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Magic number for serialization validation */
#define EMBEDDING_MAGIC    0x454D4244  /* "EMBD" */

/** Serialization format version */
#define EMBEDDING_VERSION  1

/*=============================================================================
 * Types
 *===========================================================================*/

/** Opaque embedding layer handle */
typedef struct embedding_layer embedding_layer_t;

/**
 * WHAT: Embedding layer configuration
 * WHY:  Specify dimensions, initialization scale, and freeze state
 * HOW:  Pass to embedding_create()
 */
typedef struct {
    uint32_t vocab_size;    /**< Number of tokens in vocabulary */
    uint32_t embed_dim;     /**< Dimensionality of each embedding vector */
    float    init_scale;    /**< Xavier scale override (0 = auto-compute) */
    bool     freeze;        /**< If true, skip gradient updates (default false) */
} embedding_config_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * WHAT: Return default embedding config for given vocab size and dimension
 * WHY:  Convenience — auto-computes Xavier init_scale
 * HOW:  scale = sqrt(2.0 / (vocab_size + embed_dim))
 *
 * @param vocab_size  Number of tokens
 * @param embed_dim   Embedding dimension
 * @return Configuration with defaults filled in
 */
embedding_config_t embedding_default_config(uint32_t vocab_size, uint32_t embed_dim);

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * WHAT: Create embedding layer with Xavier-initialized weights
 * WHY:  Allocate weight matrix and gradient accumulator
 * HOW:  Allocates [vocab_size * embed_dim] floats for weights and grads,
 *        initializes weights with uniform Xavier distribution
 *
 * @param config  Configuration (must not be NULL)
 * @return Embedding layer handle or NULL on failure
 */
embedding_layer_t* embedding_create(const embedding_config_t* config);

/**
 * WHAT: Destroy embedding layer and free all memory
 * WHY:  Clean up weight and gradient buffers
 *
 * @param emb  Embedding layer (NULL is safe)
 */
void embedding_destroy(embedding_layer_t* emb);

/*=============================================================================
 * Forward Pass
 *===========================================================================*/

/**
 * WHAT: Look up the embedding vector for a single token
 * WHY:  Single-token inference or generation step
 * HOW:  Copies weights[token_id * embed_dim .. +embed_dim] to output
 *
 * @param emb       Embedding layer
 * @param token_id  Token ID to look up
 * @param output    Output buffer (must hold embed_dim floats)
 * @return 0 on success, -1 on error (NULL args, out-of-range ID)
 */
int embedding_lookup(const embedding_layer_t* emb, uint32_t token_id, float* output);

/**
 * WHAT: Look up embedding vectors for a batch of tokens
 * WHY:  Batch processing for training efficiency
 * HOW:  Copies each token's embedding into output[i * embed_dim]
 *
 * @param emb        Embedding layer
 * @param token_ids  Array of token IDs
 * @param count      Number of tokens
 * @param output     Output buffer (must hold count * embed_dim floats)
 * @return 0 on success, -1 on error
 */
int embedding_lookup_batch(const embedding_layer_t* emb, const uint32_t* token_ids,
                           uint32_t count, float* output);

/**
 * WHAT: Forward pass returning a tensor [count, embed_dim]
 * WHY:  Integration with NIMCP tensor computation graph
 * HOW:  Creates 2D tensor, copies embeddings into tensor data
 *
 * @param emb        Embedding layer
 * @param token_ids  Array of token IDs
 * @param count      Number of tokens
 * @return 2D tensor [count, embed_dim] or NULL on error (caller must destroy)
 */
nimcp_tensor_t* embedding_forward(const embedding_layer_t* emb,
                                  const uint32_t* token_ids, uint32_t count);

/*=============================================================================
 * Backward Pass / Training
 *===========================================================================*/

/**
 * WHAT: Accumulate gradient for a single token's embedding
 * WHY:  Backpropagation — each token that was looked up receives gradient
 * HOW:  grad_accum[token_id * embed_dim + i] += grad_output[i]
 *
 * @param emb          Embedding layer
 * @param token_id     Token whose embedding received gradient
 * @param grad_output  Gradient vector (embed_dim floats)
 * @return 0 on success, -1 on error or frozen
 */
int embedding_backward(embedding_layer_t* emb, uint32_t token_id, const float* grad_output);

/**
 * WHAT: Apply accumulated gradients with SGD update
 * WHY:  Actually update the embedding weights after backward pass
 * HOW:  weights[i] -= learning_rate * grad_accum[i] for all i, then zero grads
 *
 * @param emb            Embedding layer
 * @param learning_rate  Learning rate (positive value)
 * @return 0 on success, -1 on error or frozen
 */
int embedding_update(embedding_layer_t* emb, float learning_rate);

/**
 * WHAT: Zero out gradient accumulator
 * WHY:  Reset before next training batch
 *
 * @param emb  Embedding layer (NULL is safe)
 */
void embedding_zero_grad(embedding_layer_t* emb);

/*=============================================================================
 * Properties
 *===========================================================================*/

/**
 * WHAT: Get embedding dimension
 * @param emb  Embedding layer
 * @return Embedding dimension, or 0 if NULL
 */
uint32_t embedding_get_dim(const embedding_layer_t* emb);

/**
 * WHAT: Get vocabulary size
 * @param emb  Embedding layer
 * @return Vocabulary size, or 0 if NULL
 */
uint32_t embedding_get_vocab_size(const embedding_layer_t* emb);

/**
 * WHAT: Set frozen state (frozen embeddings skip gradient updates)
 * @param emb     Embedding layer
 * @param frozen  true to freeze, false to unfreeze
 */
void embedding_set_frozen(embedding_layer_t* emb, bool frozen);

/**
 * WHAT: Check if embedding is frozen
 * @param emb  Embedding layer
 * @return true if frozen
 */
bool embedding_is_frozen(const embedding_layer_t* emb);

/*=============================================================================
 * Similarity / Search
 *===========================================================================*/

/**
 * WHAT: Compute cosine similarity between two token embeddings
 * WHY:  Measure semantic similarity between tokens
 * HOW:  cos(a,b) = (a . b) / (||a|| * ||b||)
 *
 * @param emb   Embedding layer
 * @param id_a  First token ID
 * @param id_b  Second token ID
 * @return Cosine similarity in [-1, 1], or 0.0 on error
 */
float embedding_cosine_similarity(const embedding_layer_t* emb, uint32_t id_a, uint32_t id_b);

/**
 * WHAT: Find k nearest neighbor tokens to a query vector
 * WHY:  Useful for generation, analogy, and debugging
 * HOW:  Brute-force cosine similarity scan over entire vocabulary
 *
 * @param emb            Embedding layer
 * @param query_vector   Query embedding vector (embed_dim floats)
 * @param k              Number of neighbors to find
 * @param result_ids     [OUT] Token IDs of nearest neighbors (size k)
 * @param result_scores  [OUT] Cosine similarity scores (size k)
 * @return 0 on success, -1 on error
 */
int embedding_nearest_neighbors(const embedding_layer_t* emb, const float* query_vector,
                                uint32_t k, uint32_t* result_ids, float* result_scores);

/*=============================================================================
 * Serialization
 *===========================================================================*/

/**
 * WHAT: Save embedding weights to binary file
 * WHY:  Persist trained embeddings for inference or continued training
 * HOW:  Writes magic, version, dimensions, weights in binary format
 *
 * @param emb   Embedding layer
 * @param path  File path to write
 * @return 0 on success, -1 on error
 */
int embedding_save(const embedding_layer_t* emb, const char* path);

/**
 * WHAT: Load embedding weights from binary file
 * WHY:  Restore previously trained embeddings
 * HOW:  Reads and validates magic/version, allocates and fills weights
 *
 * @param path  File path to read
 * @return Embedding layer handle or NULL on error
 */
embedding_layer_t* embedding_load(const char* path);

/**
 * WHAT: Geodesic interpolation between two embedding vectors
 * WHY:  Linear interpolation in high-dim space doesn't follow the data manifold.
 *       Geodesic interpolation respects the spherical geometry of normalized
 *       embeddings (SLERP on the unit hypersphere).
 * HOW:  Spherical linear interpolation (SLERP) between normalized vectors
 *
 * @param vec_a   First vector [embed_dim]
 * @param vec_b   Second vector [embed_dim]
 * @param t       Interpolation parameter [0,1]
 * @param result  Output interpolated vector [embed_dim]
 * @param dim     Embedding dimension
 * @return 0 on success, -1 on error
 */
int embedding_geodesic_interpolate(const float* vec_a, const float* vec_b,
                                    float t, float* result, uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMBEDDING_H */
