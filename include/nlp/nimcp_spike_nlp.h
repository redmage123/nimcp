//=============================================================================
// nimcp_spike_nlp.h - Spike-Based Natural Language Processing
//=============================================================================
/**
 * @file nimcp_spike_nlp.h
 * @brief Convert word embeddings to spike trains and process through neural dynamics
 *
 * WHAT: Spike-based NLP processing using temporal neural dynamics
 * WHY:
 *   - More biologically realistic than rate-based processing
 *   - Temporal dynamics capture word order naturally
 *   - Integrates with STDP for online learning
 *   - Leverages fractal network topology for hierarchical processing
 *
 * HOW:
 *   1. Convert word embeddings to input spike patterns
 *   2. Propagate spikes through fractal network
 *   3. Hub neurons integrate semantic information
 *   4. Output spikes encode sentence meaning
 *
 * ARCHITECTURE:
 * ```
 * Input (Word Embeddings 50D)
 *        ↓
 *   [Embedding → Spikes]
 *   Rate coding: positive → high rate
 *        ↓
 *  [Fractal Network 500N]
 *  Attention synapses modulate
 *  Hubs integrate semantics
 *        ↓
 *   [Output Spike Patterns]
 *   Temporal patterns → meaning
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 3
 */

#ifndef NIMCP_SPIKE_NLP_H
#define NIMCP_SPIKE_NLP_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuralnet/nimcp_neuralnet.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

#define SPIKE_NLP_MAX_EMBEDDING_DIM 512   /**< Maximum embedding dimension */
#define SPIKE_NLP_TIME_PER_WORD 100       /**< Timesteps to process each word (ms) */
#define SPIKE_NLP_SETTLE_TIME 50          /**< Settling time between words (ms) */

//=============================================================================
// Embedding-to-Spike Conversion
//=============================================================================

/**
 * @brief Convert word embedding to spike pattern
 *
 * WHAT: Inject word embedding as input spikes to network
 * WHY: Transform continuous embeddings to temporal spike code
 * HOW: Map positive embedding values to neuron firing rates
 *
 * ALGORITHM:
 * - For each embedding dimension i:
 *   - Rate = max(0, embedding[i])
 *   - Add rate to input_neurons[i].state
 *   - If state > threshold, generate spike
 *
 * @param embedding Word embedding vector
 * @param dim Embedding dimension
 * @param network Neural network to inject spikes into
 * @param input_start First input neuron index
 * @param num_input Number of input neurons (should be >= dim)
 * @param timestamp Current simulation time (ms)
 * @return Number of spikes generated
 */
uint32_t spike_nlp_embed_to_spikes(
    const float* embedding,
    uint32_t dim,
    neural_network_t network,
    uint32_t input_start,
    uint32_t num_input,
    uint64_t timestamp
);

/**
 * @brief Process single word through network
 *
 * WHAT: Inject word embedding and run network for TIME_PER_WORD timesteps
 * WHY: Allow network dynamics to process word through hub neurons
 * HOW: Continuous injection + network stepping
 *
 * @param network Neural network
 * @param embedding Word embedding
 * @param dim Embedding dimension
 * @param input_start First input neuron
 * @param num_input Number of input neurons
 * @return Total number of output spikes
 */
uint32_t spike_nlp_process_word(
    neural_network_t network,
    const float* embedding,
    uint32_t dim,
    uint32_t input_start,
    uint32_t num_input
);

//=============================================================================
// Sentence Processing
//=============================================================================

/**
 * @brief Word embedding structure
 */
typedef struct {
    char word[32];                        /**< Word text */
    float embedding[SPIKE_NLP_MAX_EMBEDDING_DIM];  /**< Embedding vector */
    uint32_t embedding_dim;               /**< Actual dimension used */
} spike_nlp_word_t;

/**
 * @brief Sentence processing result
 */
typedef struct {
    uint32_t total_spikes;                /**< Total spikes generated */
    uint32_t output_spikes;               /**< Output neuron spikes */
    float avg_hub_activity;               /**< Average hub neuron activity */
    float semantic_coherence;             /**< Measure of semantic clustering */
} spike_nlp_result_t;

/**
 * @brief Process sentence through spike-based network
 *
 * WHAT: Process sequence of words through network with temporal dynamics
 * WHY: Capture word order and context through spike timing
 * HOW: Word-by-word injection with settling time between words
 *
 * ALGORITHM:
 * ```
 * for each word in sentence:
 *     inject_spikes(word.embedding)
 *     run_network(TIME_PER_WORD)
 *     wait(SETTLE_TIME)
 * ```
 *
 * @param network Neural network
 * @param sentence Array of words
 * @param sentence_len Number of words
 * @param input_start First input neuron
 * @param num_input Number of input neurons
 * @param output_start First output neuron
 * @param num_output Number of output neurons
 * @param result Output result structure
 * @return true on success, false on error
 */
bool spike_nlp_process_sentence(
    neural_network_t network,
    const spike_nlp_word_t* sentence,
    uint32_t sentence_len,
    uint32_t input_start,
    uint32_t num_input,
    uint32_t output_start,
    uint32_t num_output,
    spike_nlp_result_t* result
);

//=============================================================================
// Analysis and Metrics
//=============================================================================

/**
 * @brief Compute semantic coherence from hub activity
 *
 * WHAT: Measure how well hub neurons cluster related concepts
 * WHY: Validate that fractal topology supports semantic processing
 * HOW: Analyze hub activation patterns for clustering
 *
 * @param network Neural network
 * @param hub_indices Array of hub neuron indices
 * @param num_hubs Number of hubs
 * @return Coherence score [0,1] where 1 = perfect clustering
 */
float spike_nlp_compute_semantic_coherence(
    neural_network_t network,
    const uint32_t* hub_indices,
    uint32_t num_hubs
);

/**
 * @brief Extract output spike pattern
 *
 * WHAT: Get spike times from output neurons
 * WHY: Analyze temporal patterns for decoding
 * HOW: Extract spike history from output neurons
 *
 * @param network Neural network
 * @param output_start First output neuron
 * @param num_output Number of output neurons
 * @param spike_times Output array for spike times (caller allocates)
 * @param max_spikes Maximum spikes to extract
 * @return Number of spikes extracted
 */
uint32_t spike_nlp_extract_output_pattern(
    neural_network_t network,
    uint32_t output_start,
    uint32_t num_output,
    uint64_t* spike_times,
    uint32_t max_spikes
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPIKE_NLP_H
