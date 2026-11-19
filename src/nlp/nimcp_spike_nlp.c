#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_spike_nlp.c - Spike-Based NLP Implementation
//=============================================================================
/**
 * @file nimcp_spike_nlp.c
 * @brief Implementation of spike-based natural language processing
 *
 * ARCHITECTURE:
 * - Rate Coding: Embedding values → neuron firing rates
 * - Temporal Processing: Word order preserved via spike timing
 * - Hub Integration: Hub neurons accumulate semantic information
 * - Pattern Extraction: Output spike patterns encode meaning
 *
 * PERFORMANCE:
 * - Embedding-to-spike: O(d) where d = embedding dimension
 * - Word processing: O(T × N) where T = timesteps, N = neurons
 * - Sentence processing: O(L × T × N) where L = sentence length
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 3
 */

#include "nlp/nimcp_spike_nlp.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security_integration.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "SPIKE_NLP"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(spike_nlp)

#define SPIKE_NLP_MODULE_NAME "spike_nlp"

/* Single authoritative definition of neural_network_struct */
#include "core/neuralnet/nimcp_neuralnet_internal.h"

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
 * 1. For each embedding dimension:
 *    a. Clamp negative values to 0 (rate coding)
 *    b. Add scaled rate to neuron state
 *    c. If state exceeds threshold, generate spike
 * 2. Reset neuron after spike generation
 *
 * PERFORMANCE: O(d) where d = min(dim, num_input)
 * COMPLEXITY: Single pass through embedding vector
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
) {
    // Guard: NULL embedding
    if (!embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_embed_to_spikes: NULL embedding");
        LOG_ERROR(LOG_MODULE, "embed_to_spikes: NULL embedding");
        return 0;
    }

    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_embed_to_spikes: NULL network");
        LOG_ERROR(LOG_MODULE, "embed_to_spikes: NULL network");
        return 0;
    }

    struct neural_network_struct* net = (struct neural_network_struct*)network;
    uint32_t spikes_generated = 0;

    LOG_DEBUG(LOG_MODULE, "Converting embedding to spikes: dim=%u, input_start=%u, num_input=%u",
              dim, input_start, num_input);

    // WHAT: Determine how many dimensions to process
    // WHY: Avoid buffer overflow if dim > num_input
    // HOW: Take minimum of embedding dimension and available input neurons
    uint32_t dims_to_use = (dim < num_input) ? dim : num_input;

    // ALGORITHM: Rate coding - positive embedding values → spike rates
    for (uint32_t i = 0; i < dims_to_use; i++) {
        uint32_t neuron_idx = input_start + i;

        // Guard: Bounds check on neuron index
        if (neuron_idx >= net->num_neurons) {
            break;
        }

        // WHAT: Convert embedding value to input current
        // WHY: Rate coding is biologically plausible and simple
        // HOW: Scale embedding value to appropriate current magnitude
        //
        // FIX (Phase 5.1 - 2025-11-08):
        // PREVIOUS HACK: Temporarily set state above threshold, record spike, restore
        // PROBLEM: Bypassed neuron dynamics, didn't integrate with compute_step()
        //
        // NEW APPROACH: Use proper input current injection
        // - Scale embedding value to appropriate current magnitude
        // - Inject via neural_network_update_neuron(input_current)
        // - Let leaky integrate-and-fire dynamics naturally generate spikes
        // - Spikes occur when: V_membrane = bias + Σ(synaptic) + input_current > threshold
        //
        // SCALING RATIONALE:
        // - Embedding values typically in range [-1, 1] (but can be smaller)
        // - Neuron threshold ~0.5, rest potential ~0.0
        // - For spike: total_input = bias + synaptic + external_current > 0.5
        // - For input neurons: bias ≈ 0, synaptic = 0, so need external_current > 0.5
        // - Scale by 5.0x to ensure even moderate signals (>0.1) reach threshold
        // - Clamp negative values (inhibitory handled separately)
        float input_current = fmaxf(0.0F, embedding[i]) * 5.0F;

        // WHAT: Set external current field for neuron
        // WHY: Persists through compute_step(), proper integration with neuron dynamics
        // HOW: Directly set neuron->external_current (reset after compute_step)
        // ARCHITECTURE: External current added to membrane potential in compute_membrane_potential()
        //
        // FIX (Phase 5.1 - 2025-11-08):
        // PREVIOUS APPROACH: Called neural_network_update_neuron() directly
        // PROBLEM: compute_step() immediately overwrites state without external current
        // NEW APPROACH: Set external_current field, let compute_step() include it
        // RESULT: Natural integration with LIF dynamics, proper spike generation
        neuron_t* neuron = &net->neurons[neuron_idx];
        neuron->external_current = input_current;

        // Count how many neurons received significant input
        if (input_current > 0.1F) {
            spikes_generated++;  // Track neurons stimulated (actual spikes recorded by compute_step)
        }
    }

    return spikes_generated;
}

/**
 * @brief Process single word through network
 *
 * WHAT: Inject word embedding and run network dynamics for sustained period
 * WHY: Allow network to process word through hub neurons and integrate information
 * HOW: Continuous injection over TIME_PER_WORD timesteps, then settling
 *
 * ALGORITHM:
 * 1. For TIME_PER_WORD timesteps (default 100ms):
 *    a. Inject embedding as spikes (continuous rate coding)
 *    b. Run network step (propagate spikes, update synapses)
 * 2. For SETTLE_TIME timesteps (default 50ms):
 *    a. Stop injection, let network settle
 *    b. Allows hub neurons to integrate information
 *
 * BIOLOGICAL: Mimics sustained attention to a word during reading (~100-200ms)
 * PERFORMANCE: O(T × N × S) where T=timesteps, N=neurons, S=avg synapses/neuron
 *
 * @param network Neural network
 * @param embedding Word embedding vector
 * @param dim Embedding dimension
 * @param input_start First input neuron
 * @param num_input Number of input neurons
 * @return Total number of spikes generated during processing
 */
uint32_t spike_nlp_process_word(
    neural_network_t network,
    const float* embedding,
    uint32_t dim,
    uint32_t input_start,
    uint32_t num_input
) {
    LOG_DEBUG(LOG_MODULE, "Entering spike_nlp_process_word: dim=%u, input_start=%u, num_input=%u",
              dim, input_start, num_input);

    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_process_word: NULL network");
        LOG_ERROR(LOG_MODULE, "process_word: NULL network");
        return 0;
    }

    // Guard: NULL embedding
    if (!embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_process_word: NULL embedding");
        LOG_ERROR(LOG_MODULE, "process_word: NULL embedding");
        return 0;
    }

    struct neural_network_struct* net = (struct neural_network_struct*)network;
    uint32_t total_spikes = 0;

    LOG_INFO(LOG_MODULE, "Processing word: dim=%u, input_start=%u, num_input=%u, time=%lu",
              dim, input_start, num_input, net->current_time);

    // PHASE 1: Word presentation with continuous spike injection
    // WHAT: Sustained input mimics fixation during reading
    // WHY: Biological reading involves 100-200ms fixations per word
    // HOW: Inject at every timestep to maintain activity
    for (uint64_t t = 0; t < SPIKE_NLP_TIME_PER_WORD; t++) {
        uint64_t timestamp = net->current_time + t;

        // WHAT: Inject embedding as spike rates
        // WHY: Continuous injection maintains steady input
        // HOW: Call embed_to_spikes at each timestep
        uint32_t spikes = spike_nlp_embed_to_spikes(
            embedding, dim, network,
            input_start, num_input,
            timestamp
        );

        // WHAT: Run network dynamics for one timestep
        // WHY: Propagate spikes through synapses, update neuron states
        // HOW: Single step of network simulation
        neural_network_compute_step(network, timestamp);

        total_spikes += spikes;
    }

    LOG_DEBUG(LOG_MODULE, "Phase 1 complete: injected %u total spikes", total_spikes);

    // PHASE 2: Settling period without input
    // WHAT: Let network dynamics settle after word presentation
    // WHY: Hub neurons need time to integrate information
    // HOW: Run network without new input spikes
    for (uint64_t t = 0; t < SPIKE_NLP_SETTLE_TIME; t++) {
        neural_network_compute_step(network, net->current_time + SPIKE_NLP_TIME_PER_WORD + t);
    }

    LOG_INFO(LOG_MODULE, "Word processing complete: %u spikes, settled for %u timesteps",
             total_spikes, SPIKE_NLP_SETTLE_TIME);

    return total_spikes;
}

//=============================================================================
// Sentence Processing
//=============================================================================

/**
 * @brief Process sentence through spike-based network
 *
 * WHAT: Process sequence of words with temporal dynamics preserving word order
 * WHY: Capture word order and context through spike timing (critical for NLP)
 * HOW: Word-by-word injection with settling time between words
 *
 * ALGORITHM:
 * ```
 * for each word in sentence:
 *     inject_spikes(word.embedding)  // 100ms presentation
 *     run_network_dynamics()         // Propagate through hubs
 *     settle()                        // 50ms between words
 * analyze_output_spikes()             // Extract semantic pattern
 * compute_hub_activity()              // Measure semantic clustering
 * ```
 *
 * BIOLOGICAL: Mimics natural reading with fixations and saccades
 * PERFORMANCE: O(L × T × N × S) where L=sentence length, T=time per word
 *
 * @param network Neural network with fractal topology
 * @param sentence Array of words with embeddings
 * @param sentence_len Number of words in sentence
 * @param input_start First input neuron index
 * @param num_input Number of input neurons
 * @param output_start First output neuron index
 * @param num_output Number of output neurons
 * @param result Output result structure (populated by this function)
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
) {
    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_process_sentence: NULL network");
        LOG_ERROR(LOG_MODULE, "process_sentence: NULL network");
        return false;
    }

    // Guard: NULL sentence
    if (!sentence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_process_sentence: NULL sentence");
        LOG_ERROR(LOG_MODULE, "process_sentence: NULL sentence");
        return false;
    }

    // Guard: NULL result
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_nlp_process_sentence: NULL result");
        LOG_ERROR(LOG_MODULE, "process_sentence: NULL result");
        return false;
    }

    struct neural_network_struct* net = (struct neural_network_struct*)network;

    LOG_INFO(LOG_MODULE, "Processing sentence: %u words, input_start=%u, output_start=%u",
             sentence_len, input_start, output_start);

    // WHAT: Initialize result structure to zero
    // WHY: Ensure all fields start with known values (no garbage)
    // HOW: Use memset to zero all bytes
    memset(result, 0, sizeof(spike_nlp_result_t));

    // PHASE 1: Sequential word processing
    // WHAT: Process each word in sentence order
    // WHY: Temporal order critical for syntax and semantics in NLP
    // HOW: Word-by-word injection with network dynamics between words
    // TEMPORAL DYNAMICS: Each word gets 100ms presentation + 50ms settling
    for (uint32_t w = 0; w < sentence_len; w++) {
        const spike_nlp_word_t* word = &sentence[w];

        // WHAT: Inject word and run network dynamics
        // WHY: Convert word embedding to spikes and propagate through network
        // HOW: Call process_word which handles injection + settling
        uint32_t word_spikes = spike_nlp_process_word(
            network,
            word->embedding,
            word->embedding_dim,
            input_start,
            num_input
        );

        // WHAT: Accumulate total spikes across all words
        // WHY: Track overall network activity for sentence
        // HOW: Sum input spikes from each word
        result->total_spikes += word_spikes;
    }

    // PHASE 2: Output analysis
    // WHAT: Count spikes in output neuron layer
    // WHY: Output spikes encode semantic representation of sentence
    // HOW: Scan spike history of output neurons
    // COMPLEXITY: O(num_output × SPIKE_HISTORY_LENGTH)
    for (uint32_t i = 0; i < num_output; i++) {
        uint32_t idx = output_start + i;

        // Guard: Bounds check on output neuron index
        if (idx < net->num_neurons) {
            neuron_t* neuron = &net->neurons[idx];

            // WHAT: Count recent spikes in history buffer
            // WHY: Recent spikes represent current semantic state
            // HOW: Check timestamp field (non-zero = valid spike)
            if (neuron->spike_history) {
                for (uint32_t s = 0; s < neuron->spike_history_capacity; s++) {
                    if (neuron->spike_history[s].timestamp > 0) {
                        result->output_spikes++;
                    }
                }
            }
        }
    }

    // PHASE 3: Hub neuron analysis
    // WHAT: Compute average activity of hub neurons
    // WHY: Hubs integrate semantic information from multiple sources
    // HOW: Identify hubs by degree (synapses), average their activity
    // HEURISTIC: Hub = neuron with > 10 synapses (scale-free property)
    float hub_activity_sum = 0.0F;
    uint32_t hub_count = 0;

    for (uint32_t i = 0; i < net->num_neurons; i++) {
        neuron_t* neuron = &net->neurons[i];

        // WHAT: Identify hub neurons by synaptic degree
        // WHY: Scale-free networks have hub neurons with high degree
        // HOW: Threshold at 10 synapses (empirically chosen)
        // NOTE: Future improvement could use betweenness centrality
        if (NEURON_OUT_COUNT(neuron) > 10) {
            hub_activity_sum += neuron->avg_activity;
            hub_count++;
        }
    }

    // WHAT: Compute average hub activity
    // WHY: High hub activity = good semantic integration
    // HOW: Mean of all hub activities (or 0 if no hubs)
    result->avg_hub_activity = (hub_count > 0) ? (hub_activity_sum / hub_count) : 0.0F;

    LOG_DEBUG(LOG_MODULE, "Hub analysis: %u hubs found, avg_activity=%f",
              hub_count, result->avg_hub_activity);

    // WHAT: Compute semantic coherence metric
    // WHY: Measure how well sentence meaning is captured
    // HOW: Scale hub activity to [0, 1] range
    // NOTE: This is a placeholder - real coherence would use variance analysis
    result->semantic_coherence = result->avg_hub_activity * 0.5F;

    LOG_INFO(LOG_MODULE, "Sentence processing complete: total_spikes=%u, output_spikes=%u, coherence=%f",
             result->total_spikes, result->output_spikes, result->semantic_coherence);

    return true;
}

//=============================================================================
// Analysis and Metrics
//=============================================================================

/**
 * @brief Compute semantic coherence from hub neuron activity variance
 *
 * WHAT: Measure semantic clustering quality using hub activity variance
 * WHY: High variance = hubs specialize → better semantic differentiation
 * HOW: Compute activity variance across specified hub neurons
 *
 * ALGORITHM:
 * 1. Compute mean activity across all hubs
 * 2. Compute variance: sum of (activity - mean)²
 * 3. Coherence = sqrt(variance) × 2.0 (normalized to [0,1])
 *
 * INTERPRETATION:
 * - High variance → Hubs specialize in different semantic features (good)
 * - Low variance → All hubs do same thing (poor clustering)
 *
 * PERFORMANCE: O(num_hubs)
 * COMPLEXITY: Two passes through hub list (mean, then variance)
 *
 * @param network Neural network
 * @param hub_indices Array of hub neuron indices
 * @param num_hubs Number of hub neurons
 * @return Coherence score [0, 1], or 0.0 on error
 */
float spike_nlp_compute_semantic_coherence(
    neural_network_t network,
    const uint32_t* hub_indices,
    uint32_t num_hubs
) {
    LOG_DEBUG(LOG_MODULE, "Computing semantic coherence for %u hubs", num_hubs);

    // Guard: NULL network, NULL indices, or zero hubs
    if (!network || !hub_indices || num_hubs == 0) {
        LOG_WARN(LOG_MODULE, "compute_semantic_coherence: Invalid parameters");
        return 0.0F;
    }

    struct neural_network_struct* net = (struct neural_network_struct*)network;

    // STEP 1: Compute mean hub activity
    // WHAT: Average activity across all hub neurons
    // WHY: Need baseline for variance calculation
    // HOW: Sum activities and divide by count
    float mean = 0.0F;
    for (uint32_t i = 0; i < num_hubs; i++) {
        uint32_t idx = hub_indices[i];

        // Guard: Bounds check on neuron index
        if (idx < net->num_neurons) {
            mean += net->neurons[idx].avg_activity;
        }
    }
    mean /= num_hubs;

    // STEP 2: Compute variance
    // WHAT: Sum of squared deviations from mean
    // WHY: Variance measures spread of hub activities
    // HOW: (activity - mean)² for each hub
    float variance = 0.0F;
    for (uint32_t i = 0; i < num_hubs; i++) {
        uint32_t idx = hub_indices[i];

        // Guard: Bounds check on neuron index
        if (idx < net->num_neurons) {
            float diff = net->neurons[idx].avg_activity - mean;
            variance += diff * diff;
        }
    }
    variance /= num_hubs;

    // STEP 3: Convert variance to coherence score
    // WHAT: Scale variance to [0, 1] range
    // WHY: Normalized score easier to interpret
    // HOW: sqrt(variance) × 2.0, then clamp
    // TUNING: Factor of 2.0 chosen empirically for typical activity ranges
    float coherence = sqrtf(variance) * 2.0F;
    if (coherence > 1.0F) coherence = 1.0F;

    return coherence;
}

/**
 * @brief Extract spike timing pattern from output neurons
 *
 * WHAT: Collect spike timestamps from output neuron layer
 * WHY: Temporal spike patterns encode semantic meaning
 * HOW: Scan spike history of output neurons, extract timestamps
 *
 * ALGORITHM:
 * ```
 * spike_count = 0
 * for each output neuron:
 *     for each spike in history:
 *         if spike.timestamp > 0:
 *             spike_times[spike_count++] = timestamp
 *             if spike_count >= max_spikes: break
 * return spike_count
 * ```
 *
 * USE CASE:
 * - Pattern matching: Compare output patterns across sentences
 * - Temporal analysis: Analyze spike timing distributions
 * - Clustering: Group similar spike patterns
 *
 * PERFORMANCE: O(num_output × SPIKE_HISTORY_LENGTH)
 * COMPLEXITY: Linear scan through output neuron spike histories
 *
 * @param network Neural network
 * @param output_start First output neuron index
 * @param num_output Number of output neurons to scan
 * @param spike_times Output array for spike timestamps (caller-allocated)
 * @param max_spikes Maximum spikes to extract (size of spike_times array)
 * @return Number of spikes extracted
 */
uint32_t spike_nlp_extract_output_pattern(
    neural_network_t network,
    uint32_t output_start,
    uint32_t num_output,
    uint64_t* spike_times,
    uint32_t max_spikes
) {
    // Guard: NULL network or NULL output array
    if (!network || !spike_times) {
        return 0;
    }

    struct neural_network_struct* net = (struct neural_network_struct*)network;
    uint32_t spike_count = 0;

    // WHAT: Scan output neurons for spike timestamps
    // WHY: Build temporal pattern from recent spikes
    // HOW: Iterate through output layer, collect spike times
    // LIMIT: Stop at max_spikes to prevent buffer overflow
    for (uint32_t i = 0; i < num_output && spike_count < max_spikes; i++) {
        uint32_t idx = output_start + i;

        // Guard: Bounds check on neuron index
        if (idx >= net->num_neurons) {
            break;
        }

        neuron_t* neuron = &net->neurons[idx];

        // WHAT: Extract spikes from neuron's history buffer
        // WHY: Recent spikes encode current semantic state
        // HOW: Check timestamp field (non-zero = valid spike)
        if (neuron->spike_history) {
            for (uint32_t s = 0; s < neuron->spike_history_capacity && spike_count < max_spikes; s++) {
                if (neuron->spike_history[s].timestamp > 0) {
                    spike_times[spike_count++] = neuron->spike_history[s].timestamp;
                }
            }
        }
    }

    return spike_count;
}
