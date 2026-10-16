//=============================================================================
// nimcp_nlp.h - NLP Integration Module
//=============================================================================
/**
 * @file nimcp_nlp.h
 * @brief Natural Language Processing integration for NIMCP
 *
 * WHAT: Unified NLP API combining programmable synapses, attention, neuromodulation
 * WHY: Enable language processing using biologically-inspired mechanisms
 * HOW: Integrates 3 major NIMCP subsystems into coherent NLP architecture
 *
 * ARCHITECTURE:
 * - Facade Pattern: Simple API hiding complex subsystem integration
 * - Bridge Pattern: Decouples NLP interface from neural implementation
 * - Composite Pattern: Language model = network + attention + neuromodulation
 *
 * DESIGN PHILOSOPHY:
 * This module unifies NIMCP's cognitive capabilities for language:
 * 1. Programmable Synapses (NIMCP 2.7): Context-dependent connections
 * 2. Multihead Attention: Query-key-value semantic routing
 * 3. Neuromodulation: Reward-based learning and gating
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Forward pass: O(n × m × d) where n=neurons, m=sequence_length, d=embedding_dim
 * - Attention computation: O(m² × d) per head
 * - Neuromodulation: O(1) per synapse (multiplicative scaling)
 * - Total: O(n × m × d + h × m² × d) where h=num_attention_heads
 *
 * BIOLOGICAL INSPIRATION:
 * - Attention: Cortico-thalamic loops, selective processing
 * - Neuromodulation: Dopaminergic reward, cholinergic salience
 * - Gating: Basal ganglia action selection
 *
 * @author Claude Code + NIMCP Development Team
 * @date 2025-11-07
 * @version 2.7.0
 */

#ifndef NIMCP_NLP_H
#define NIMCP_NLP_H

#include <stdint.h>
#include <stdbool.h>

// Core NIMCP dependencies
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/attention/nimcp_attention.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Configuration for NLP-enhanced neural network
 *
 * WHAT: Unified configuration combining network, attention, and neuromodulation
 * WHY: Single entry point for complex multi-system setup
 * HOW: Aggregates configs from all subsystems
 */
typedef struct {
    // Base network configuration
    network_config_t network_config;

    // Attention configuration
    multihead_attention_config_t attention_config;

    // Neuromodulator configuration
    neuromodulator_config_t neuromod_config;

    // NLP-specific parameters
    uint32_t vocab_size;           /**< Size of vocabulary */
    uint32_t embedding_dim;        /**< Dimension of word embeddings */
    uint32_t max_sequence_length;  /**< Maximum sequence length */

    // Synapse computation modes
    bool use_attention_synapses;   /**< Enable attention-modulated synapses */
    bool use_semantic_synapses;    /**< Enable semantic similarity synapses */
    bool use_gating_synapses;      /**< Enable gating synapses */
    bool use_neuromodulated_synapses; /**< Enable neuromodulator-sensitive synapses */

    // Bio-async integration
    bool enable_bio_async;         /**< Enable bio-async communication */
} nlp_network_config_t;

/**
 * @brief Opaque handle to NLP-enhanced network
 *
 * DESIGN: Opaque pointer pattern - hides implementation details
 */
typedef struct nlp_network_struct* nlp_network_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create NLP-enhanced neural network
 *
 * WHAT: Factory function for integrated NLP network
 * WHY: Encapsulates complex multi-system initialization
 * HOW: Creates network, attention system, neuromodulator system, wires them
 * WHEN: Called once during application initialization
 *
 * ALGORITHM:
 * 1. Create base neural network
 * 2. Create multihead attention system
 * 3. Create neuromodulator system
 * 4. Configure synapse compute functions based on flags
 * 5. Wire systems together via compute contexts
 *
 * COMPLEXITY: O(n + h × d²) where n=neurons, h=heads, d=embedding_dim
 *
 * @param config Configuration structure
 * @return Opaque handle to network (NULL on failure)
 */
nlp_network_t nlp_network_create(const nlp_network_config_t* config);

/**
 * @brief Destroy NLP network and free resources
 *
 * WHAT: Cleanup function for integrated network
 * WHY: Prevent memory leaks from multi-system allocation
 * HOW: Destroys subsystems in reverse creation order
 *
 * COMPLEXITY: O(n + h × d²)
 *
 * @param network Network to destroy
 */
void nlp_network_destroy(nlp_network_t network);

//=============================================================================
// Forward Pass Functions
//=============================================================================

/**
 * @brief Process text sequence through NLP network
 *
 * WHAT: Main inference function - text → embeddings → network → output
 * WHY: End-to-end language processing in single API call
 * HOW: Attention → synapse computation → neuromodulation → output
 * WHEN: Called for every input sequence (sentence, paragraph, etc)
 *
 * ALGORITHM:
 * 1. Convert token IDs to embeddings (vocab lookup)
 * 2. Run multihead attention over sequence
 * 3. Store attention context in global state
 * 4. Inject into network via synapse compute contexts
 * 5. Run network forward pass
 * 6. Apply neuromodulation effects
 * 7. Extract output representation
 *
 * COMPLEXITY: O(n × m × d + h × m² × d)
 *
 * @param network NLP network
 * @param token_ids Input token sequence [sequence_length]
 * @param sequence_length Length of input sequence
 * @param output Output representation [output_dim]
 * @param output_dim Size of output vector
 * @return true on success, false on error
 */
bool nlp_network_forward(
    nlp_network_t network,
    const uint32_t* token_ids,
    uint32_t sequence_length,
    float* output,
    uint32_t output_dim
);

/**
 * @brief Get embedding for a token
 *
 * WHAT: Lookup/compute embedding vector for token ID
 * WHY: Needed for converting discrete tokens to continuous representations
 * HOW: Direct array lookup or learned embedding matrix
 *
 * COMPLEXITY: O(1) lookup, O(d) copy
 *
 * @param network NLP network
 * @param token_id Token to lookup
 * @param embedding Output embedding vector [embedding_dim]
 * @return true on success, false if token_id invalid
 */
bool nlp_network_get_embedding(
    nlp_network_t network,
    uint32_t token_id,
    float* embedding
);

/**
 * @brief Set embedding for a token
 *
 * WHAT: Update embedding vector for token ID
 * WHY: Support learned embeddings or external pre-trained vectors
 * HOW: Direct array write or embedding matrix update
 *
 * COMPLEXITY: O(d)
 *
 * @param network NLP network
 * @param token_id Token to update
 * @param embedding New embedding vector [embedding_dim]
 * @return true on success, false if token_id invalid
 */
bool nlp_network_set_embedding(
    nlp_network_t network,
    uint32_t token_id,
    const float* embedding
);

//=============================================================================
// Attention Control Functions
//=============================================================================

/**
 * @brief Set attention gate signal
 *
 * WHAT: Control thalamic gating of attention heads
 * WHY: Task-dependent attention modulation (focus vs diffuse)
 * HOW: Scales attention weights multiplicatively
 *
 * BIOLOGICAL: Models thalamic reticular nucleus gating
 *
 * COMPLEXITY: O(1)
 *
 * @param network NLP network
 * @param gate_signal Gate strength [0,1] (0=closed, 1=open)
 * @return true on success
 */
bool nlp_network_set_attention_gate(nlp_network_t network, float gate_signal);

/**
 * @brief Get attention weights for last forward pass
 *
 * WHAT: Retrieve computed attention matrix
 * WHY: Interpretability - which tokens attended to which
 * HOW: Copies internal attention weights to output buffer
 *
 * USE CASE: Visualizing what the model "focused on"
 *
 * COMPLEXITY: O(h × m²)
 *
 * @param network NLP network
 * @param head_idx Attention head to query (0..num_heads-1)
 * @param weights Output attention matrix [sequence_length × sequence_length]
 * @return true on success, false if head_idx invalid
 */
bool nlp_network_get_attention_weights(
    nlp_network_t network,
    uint32_t head_idx,
    float* weights
);

//=============================================================================
// Neuromodulation Functions
//=============================================================================

/**
 * @brief Release dopamine (reward signal)
 *
 * WHAT: Trigger dopamine release for reward learning
 * WHY: Reinforce successful language predictions
 * HOW: Updates neuromodulator pool, modulates synapse learning
 *
 * BIOLOGICAL: Ventral tegmental area reward signaling
 *
 * COMPLEXITY: O(1)
 *
 * @param network NLP network
 * @param reward_magnitude Reward strength [-1,1] (negative=punishment)
 * @param predicted_reward Expected reward (for prediction error)
 * @return Prediction error (reward - predicted)
 */
float nlp_network_release_dopamine(
    nlp_network_t network,
    float reward_magnitude,
    float predicted_reward
);

/**
 * @brief Release acetylcholine (salience signal)
 *
 * WHAT: Trigger acetylcholine for salience/surprise
 * WHY: Modulate learning rate based on unexpectedness
 * HOW: Amplifies plasticity for novel stimuli
 *
 * BIOLOGICAL: Basal forebrain cholinergic system
 *
 * COMPLEXITY: O(1)
 *
 * @param network NLP network
 * @param salience Salience level [0,1]
 * @return Current acetylcholine level
 */
float nlp_network_release_acetylcholine(
    nlp_network_t network,
    float salience
);

/**
 * @brief Get current neuromodulator levels
 *
 * WHAT: Query current concentration of all neuromodulators
 * WHY: Monitoring network state for analysis/debugging
 * HOW: Reads from neuromodulator pool
 *
 * COMPLEXITY: O(1)
 *
 * @param network NLP network
 * @param dopamine Output dopamine level (can be NULL)
 * @param serotonin Output serotonin level (can be NULL)
 * @param acetylcholine Output acetylcholine level (can be NULL)
 * @param norepinephrine Output norepinephrine level (can be NULL)
 * @return true on success
 */
bool nlp_network_get_neuromodulator_levels(
    nlp_network_t network,
    float* dopamine,
    float* serotonin,
    float* acetylcholine,
    float* norepinephrine
);

//=============================================================================
// Training Functions
//=============================================================================

/**
 * @brief Train network on sequence with reward
 *
 * WHAT: Supervised learning with reward-modulated plasticity
 * WHY: Learn language patterns from labeled data
 * HOW: Forward pass + reward signal + synaptic updates
 *
 * ALGORITHM:
 * 1. Forward pass through network
 * 2. Compute prediction error
 * 3. Release dopamine based on error
 * 4. Apply STDP with neuromodulation
 * 5. Update attention weights
 *
 * COMPLEXITY: O(n × m × d + h × m² × d)
 *
 * @param network NLP network
 * @param token_ids Input sequence [sequence_length]
 * @param sequence_length Length of input
 * @param target Target output [output_dim]
 * @param output_dim Size of output
 * @param learning_rate Learning rate multiplier
 * @return Loss value (prediction error)
 */
float nlp_network_train(
    nlp_network_t network,
    const uint32_t* token_ids,
    uint32_t sequence_length,
    const float* target,
    uint32_t output_dim,
    float learning_rate
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get network statistics
 *
 * WHAT: Comprehensive network state metrics
 * WHY: Monitoring training progress and network health
 * HOW: Aggregates stats from all subsystems
 *
 * COMPLEXITY: O(n + h)
 *
 * @param network NLP network
 * @param stats Output statistics structure
 * @return true on success
 */
bool nlp_network_get_stats(nlp_network_t network, network_stats_t* stats);

/**
 * @brief Save network to file
 *
 * WHAT: Serialize network state to disk
 * WHY: Checkpoint trained models
 * HOW: Writes embeddings, weights, attention params, neuromod state
 *
 * COMPLEXITY: O(n × s + v × d) where v=vocab_size
 *
 * @param network NLP network
 * @param filepath Path to save file
 * @return true on success
 */
bool nlp_network_save(nlp_network_t network, const char* filepath);

/**
 * @brief Load network from file
 *
 * WHAT: Deserialize network state from disk
 * WHY: Restore trained models
 * HOW: Reads and reconstructs all subsystems
 *
 * COMPLEXITY: O(n × s + v × d)
 *
 * @param filepath Path to load file
 * @return Loaded network (NULL on failure)
 */
nlp_network_t nlp_network_load(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NLP_H
