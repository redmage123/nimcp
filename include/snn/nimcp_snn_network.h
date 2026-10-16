//=============================================================================
// nimcp_snn_network.h - SNN Network API
//=============================================================================
/**
 * @file nimcp_snn_network.h
 * @brief Main SNN network creation, simulation, and training API
 *
 * WHAT: Top-level SNN orchestration facade
 * WHY:  Unified API for spiking neural network operations
 * HOW:  Integrates with existing neural_network_t, adds spike-specific features
 *
 * DESIGN PATTERN: Facade
 * - SNN network is a facade over neural_network_t
 * - Adds spike encoding/decoding, population management, training
 * - Does NOT duplicate neuron/synapse infrastructure
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_NETWORK_H
#define NIMCP_SNN_NETWORK_H

#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Network Lifecycle
//=============================================================================

/**
 * @brief Create SNN network from configuration
 *
 * WHAT: Allocate and initialize a complete SNN
 * WHY:  Factory function for SNN creation
 * HOW:  Creates underlying neural_network, populations, encoder/decoder
 *
 * @param config Network configuration (validated before use)
 * @return New SNN network, or NULL on failure
 *
 * COMPLEXITY: O(n_inputs + n_outputs + sum(population_sizes))
 * MEMORY: ~500 bytes base + ~2KB per neuron (spike trains)
 */
snn_network_t* snn_network_create(const snn_config_t* config);

/**
 * @brief Destroy SNN network and free all resources
 *
 * WHAT: Clean up SNN and all owned resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroys populations, encoder/decoder, simulation, training context
 *
 * NOTE: Does NOT destroy the underlying neural_network if it was provided
 *       externally. Only destroys if created internally.
 *
 * @param network Network to destroy (can be NULL)
 *
 * COMPLEXITY: O(n_populations + n_neurons)
 */
void snn_network_destroy(snn_network_t* network);

/**
 * @brief Reset SNN network to initial state
 *
 * WHAT: Clear all spike history, reset membrane potentials
 * WHY:  Prepare for new input sequence without recreating network
 * HOW:  Resets all neurons to resting state, clears spike trains
 *
 * @param network Network to reset
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_neurons)
 */
int snn_network_reset(snn_network_t* network);

/**
 * @brief Attach a neural substrate to an SNN for biological modulation.
 *
 * WHAT: Sets the borrowed substrate pointer that the SNN's step function
 *       consults each tick for ATP/temperature/ion/membrane-driven
 *       modulation of tau, refractory, spike survival, and plasticity LR.
 * WHY:  The substrate carries slow-varying biological state that governs
 *       metabolic constraints; the SNN needs a handle to read it without
 *       owning its lifecycle.
 * HOW:  Null-tolerant pass-through; resets the update period counter so
 *       effects are recomputed on the next step. The network does NOT
 *       destroy the substrate in snn_network_destroy.
 *
 * @param net Network to attach to (NULL-tolerant: returns silently).
 * @param sub Substrate pointer or NULL to detach.
 */
void snn_network_attach_substrate(snn_network_t* net,
                                  struct neural_substrate* sub);

//=============================================================================
// Simulation
//=============================================================================

/**
 * @brief Run one simulation timestep
 *
 * WHAT: Advance simulation by dt milliseconds
 * WHY:  Discrete-time SNN simulation
 * HOW:  Update membrane potentials, generate spikes, propagate spikes
 *
 * ALGORITHM:
 * 1. For each population:
 *    a. Compute synaptic input currents
 *    b. Update membrane potentials (LIF/Izhikevich)
 *    c. Generate spikes where V >= V_thresh
 *    d. Apply refractory period
 *    e. Record spikes to spike trains
 * 2. If training: update eligibility traces
 * 3. If bio-async: broadcast spike events
 *
 * @param network SNN network
 * @param dt Timestep in milliseconds (uses config.dt if 0)
 * @return Number of spikes generated, or negative on error
 *
 * COMPLEXITY: O(n_neurons × avg_synapses)
 */
int snn_network_step(snn_network_t* network, float dt);

/**
 * @brief Run simulation for specified duration
 *
 * WHAT: Simulate network for T milliseconds
 * WHY:  Convenience function for running multiple steps
 * HOW:  Calls snn_network_step repeatedly
 *
 * @param network SNN network
 * @param duration_ms Simulation duration in milliseconds
 * @return Total spikes generated, or negative on error
 *
 * COMPLEXITY: O((duration/dt) × n_neurons × avg_synapses)
 */
int snn_network_run(snn_network_t* network, float duration_ms);

/**
 * @brief Update network->stats with derived firing-rate metrics.
 *
 * Shared helper so both the inference path (snn_network_run) and the BPTT
 * training path (snn_backprop_forward) can update the same visible
 * metrics — mean_firing_rate, max_firing_rate, sparsity, silent_neurons,
 * hyperactive_neurons, spikes_per_sample. Previously only the inference
 * path updated these, which made training-time SNN activity invisible
 * via get_snn_stats RPC.
 *
 * @param network       The SNN network (stats will be overwritten).
 * @param total_spikes  Total spikes accumulated during the run.
 * @param duration_ms   Simulated duration in milliseconds (for Hz conversion).
 */
void snn_network_update_stats(snn_network_t* network, int total_spikes, float duration_ms);

//=============================================================================
// Conductance-Based Migration — Weight Rescaling
//=============================================================================

/**
 * @brief One-shot rescale of all CSR synapse weights for CB-mode operation.
 *
 * Current-based weights are calibrated for direct mV-equivalent summation
 * into I_syn. Conductance-based weights act through a driving force
 * (E_rev - V), which at typical resting potential is ~50 mV. Multiplying
 * by 50× would over-drive the network; the canonical rescale divides each
 * weight by SNN_CB_DEFAULT_RESCALE (1/50) so the average-case post-synaptic
 * effect matches the current-based regime.
 *
 * Idempotent: checks the sticky `cb_weights_rescaled` knob and refuses to
 * apply twice. Caller must persist the knob (snn_tune.json) so the flag
 * survives daemon restart.
 *
 * @param network  SNN network to rescale.
 * @param factor   Multiplicative scale (e.g., 1.0/50.0 = 0.02). Must be > 0
 *                 and finite. Pass < 1 to compress (current → CB), > 1 to
 *                 expand (CB → current rollback).
 * @return 0 on success; SNN_ERROR_* on failure (already rescaled, NULL net,
 *         invalid factor).
 *
 * Touches every CSR storage in the network (entries[] AND GPU mirror if
 * resident). Wall time scales with total synapse count; for 1.8M neurons
 * × ~20 fan-in = 36M synapses, expect ~0.5 s on CPU, dominated by GPU
 * sync if GPU resident.
 */
#define SNN_CB_DEFAULT_RESCALE_FACTOR (1.0f / 50.0f)
int snn_rescale_weights_for_conductance(snn_network_t* network, float factor);

//=============================================================================
// Performance-Optimized Stepping
//=============================================================================

/**
 * @brief Per-step performance statistics
 */
typedef struct snn_step_stats_s {
    uint32_t total_neurons;         /**< Total neurons across populations */
    uint32_t neurons_updated;       /**< Neurons that were actually updated */
    uint32_t neurons_skipped;       /**< Neurons skipped (far from threshold) */
    uint32_t neurons_refractory;    /**< Neurons in refractory period */
    uint32_t spikes_generated;      /**< Spikes this step */
    float    compute_ratio;         /**< neurons_updated / total_neurons */
} snn_step_stats_t;

/**
 * @brief Spike-driven sparse step — only updates active neurons
 *
 * WHAT: Event-driven neuron update that skips quiescent neurons
 * WHY:  At 2-5% firing rate, 95-98% of neurons are silent; O(spikes) not O(N)
 * HOW:  Skip neurons whose membrane potential is far below threshold AND
 *       have no incoming synaptic current. Only neurons that received a spike
 *       or are within threshold_margin of firing are updated.
 *
 * @param network SNN network
 * @param dt Timestep in milliseconds (uses config.dt if 0)
 * @param threshold_margin Neurons within this margin of v_thresh are always updated (mV).
 *                         Use 0 for default (5.0 mV).
 * @param stats [out] Optional step statistics (may be NULL)
 * @return Number of spikes generated, or negative on error
 *
 * COMPLEXITY: O(active_neurons) where active = spiked + near-threshold + has-input
 */
int snn_network_step_sparse(snn_network_t* network, float dt,
                             float threshold_margin,
                             snn_step_stats_t* stats);

/**
 * @brief Population-parallel step — independent populations step concurrently
 *
 * WHAT: Step populations in parallel using thread pool
 * WHY:  Independent populations have no cross-dependencies within a step
 * HOW:  Partition populations into independent groups, step each in parallel
 *
 * @param network SNN network
 * @param dt Timestep in milliseconds (uses config.dt if 0)
 * @param n_threads Number of threads (0 = use config.n_threads or auto-detect)
 * @return Number of spikes generated, or negative on error
 *
 * COMPLEXITY: O(n_neurons / n_threads × avg_synapses)
 */
int snn_network_step_parallel(snn_network_t* network, float dt,
                               uint32_t n_threads);

/**
 * @brief Run simulation using sparse (event-driven) stepping
 *
 * WHAT: Run for duration using snn_network_step_sparse for each timestep
 * WHY:  Combines sparse stepping with full simulation convenience
 *
 * @param network SNN network
 * @param duration_ms Simulation duration
 * @param threshold_margin Skip margin (0 = default 5.0 mV)
 * @param stats [out] Accumulated statistics (may be NULL)
 * @return Total spikes generated, or negative on error
 */
int snn_network_run_sparse(snn_network_t* network, float duration_ms,
                            float threshold_margin,
                            snn_step_stats_t* stats);

//=============================================================================
// Input/Output (Encoding/Decoding)
//=============================================================================

/**
 * @brief Set input values (will be encoded to spikes)
 *
 * WHAT: Provide continuous input values for spike encoding
 * WHY:  Convert external signals to spike trains
 * HOW:  Store values, encode to spikes during simulation
 *
 * @param network SNN network
 * @param inputs Input values [n_inputs]
 * @param n_inputs Number of input values
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_inputs)
 */
int snn_network_set_inputs(snn_network_t* network,
                           const float* inputs,
                           uint32_t n_inputs);

/**
 * @brief Set input as tensor
 *
 * WHAT: Provide input as nimcp_tensor_t
 * WHY:  Integration with tensor pipeline
 * HOW:  Extract data from tensor, call snn_network_set_inputs
 *
 * @param network SNN network
 * @param input Input tensor [n_inputs] or [batch, n_inputs]
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_inputs)
 */
int snn_network_set_input_tensor(snn_network_t* network,
                                 const nimcp_tensor_t* input);

/**
 * @brief Get decoded output values
 *
 * WHAT: Decode output spike trains to continuous values
 * WHY:  Convert spikes to usable output
 * HOW:  Apply configured decoding method to output population
 *
 * @param network SNN network
 * @param outputs Output buffer [n_outputs]
 * @param n_outputs Size of output buffer
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_outputs × spike_count)
 */
int snn_network_get_outputs(snn_network_t* network,
                            float* outputs,
                            uint32_t n_outputs);

/**
 * @brief Get output as tensor
 *
 * WHAT: Return decoded outputs as tensor
 * WHY:  Integration with tensor pipeline
 * HOW:  Decode spikes, fill tensor
 *
 * @param network SNN network
 * @param output Output tensor (must be pre-allocated [n_outputs])
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_outputs × spike_count)
 */
int snn_network_get_output_tensor(snn_network_t* network,
                                  nimcp_tensor_t* output);

/**
 * @brief Forward pass (set input, run, get output)
 *
 * WHAT: Complete forward inference
 * WHY:  Convenience function for single inference
 * HOW:  Combines set_inputs, run, get_outputs
 *
 * @param network SNN network
 * @param inputs Input values [n_inputs]
 * @param n_inputs Number of inputs
 * @param outputs Output buffer [n_outputs]
 * @param n_outputs Number of outputs
 * @param duration_ms Simulation duration
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O((duration/dt) × n_neurons × avg_synapses)
 */
int snn_network_forward(snn_network_t* network,
                        const float* inputs,
                        uint32_t n_inputs,
                        float* outputs,
                        uint32_t n_outputs,
                        float duration_ms);

//=============================================================================
// Training
//=============================================================================

/**
 * @brief Set training mode
 *
 * WHAT: Enable or disable training mode
 * WHY:  Control whether learning updates are applied
 * HOW:  Creates/destroys training context
 *
 * @param network SNN network
 * @param training true to enable training
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1) if no context change, O(n_synapses) if allocating traces
 */
int snn_network_set_training(snn_network_t* network, bool training);

/**
 * @brief Apply STDP learning (uses existing synapse_t infrastructure)
 *
 * WHAT: Apply spike-timing dependent plasticity
 * WHY:  Local, biological learning rule
 * HOW:  Uses spike times from spike_trains, updates synapse weights
 *
 * NOTE: This leverages existing stdp_params in synapse_t, not duplicating
 *
 * @param network SNN network
 * @return Number of synapses modified
 *
 * COMPLEXITY: O(n_neurons × n_synapses × spike_count)
 */
int snn_network_apply_stdp(snn_network_t* network);

/**
 * @brief Apply reward-modulated STDP
 *
 * WHAT: Apply R-STDP with reward signal
 * WHY:  Reinforcement learning with biological plausibility
 * HOW:  STDP eligibility traces × reward signal
 *
 * @param network SNN network
 * @param reward Reward signal [-1, 1]
 * @return Number of synapses modified
 *
 * COMPLEXITY: O(n_synapses)
 */
int snn_network_apply_rstdp(snn_network_t* network, float reward);

/**
 * @brief Compute surrogate gradients (for backprop-through-time)
 *
 * WHAT: Compute gradients using surrogate gradient function
 * WHY:  Enable gradient-based training of SNNs
 * HOW:  Smooth approximation of spike derivative
 *
 * @param network SNN network
 * @param target Target output values [n_outputs]
 * @param n_targets Number of target values
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_neurons × n_synapses)
 */
int snn_network_compute_gradients(snn_network_t* network,
                                  const float* target,
                                  uint32_t n_targets);

/**
 * @brief Apply computed gradients to weights
 *
 * WHAT: Update weights using computed gradients
 * WHY:  Complete the training step
 * HOW:  w -= lr × grad_w
 *
 * @param network SNN network
 * @param learning_rate Learning rate (uses config if 0)
 * @return Number of weights updated
 *
 * COMPLEXITY: O(n_synapses)
 */
int snn_network_apply_gradients(snn_network_t* network, float learning_rate);

/**
 * @brief Complete training step (forward + backward + update)
 *
 * WHAT: Single training iteration
 * WHY:  Convenience function for training loop
 * HOW:  Forward pass, compute loss, compute gradients, update weights
 *
 * @param network SNN network
 * @param inputs Input values [n_inputs]
 * @param n_inputs Number of inputs
 * @param targets Target values [n_outputs]
 * @param n_targets Number of targets
 * @param duration_ms Simulation duration
 * @return Training loss, or negative on error
 *
 * COMPLEXITY: O((duration/dt) × n_neurons × avg_synapses)
 */
float snn_network_train_step(snn_network_t* network,
                             const float* inputs,
                             uint32_t n_inputs,
                             const float* targets,
                             uint32_t n_targets,
                             float duration_ms);

//=============================================================================
// Population Management
//=============================================================================

/**
 * @brief Add population to network
 *
 * WHAT: Create and add a new population of neurons
 * WHY:  Build network architecture incrementally
 * HOW:  Allocates population, adds neurons to underlying neural_network
 *
 * @param network SNN network
 * @param n_neurons Number of neurons in population
 * @param neuron_type Type of neurons (LIF, Izhikevich, etc.)
 * @param name Human-readable name
 * @return Population ID, or negative on error
 *
 * COMPLEXITY: O(n_neurons)
 */
int snn_network_add_population(snn_network_t* network,
                               uint32_t n_neurons,
                               neuron_type_t neuron_type,
                               const char* name);

/**
 * @brief Add population in lightweight/CSR mode (no dense allocation)
 *
 * @param network SNN network
 * @param n_neurons Number of neurons
 * @param neuron_type Type of neurons
 * @param name Human-readable name
 * @return Population ID, or negative on error
 */
int snn_network_add_population_lightweight(snn_network_t* network,
                                           uint32_t n_neurons,
                                           neuron_type_t neuron_type,
                                           const char* name);

/**
 * @brief Connect two populations
 *
 * WHAT: Create synaptic connections between populations
 * WHY:  Define network topology
 * HOW:  Creates synapses in underlying neural_network
 *
 * @param network SNN network
 * @param src_pop Source population ID
 * @param dst_pop Destination population ID
 * @param topology Connection topology
 * @param connectivity Connectivity ratio [0, 1] for sparse topologies
 * @param synapse_type Type of synapses (AMPA, NMDA, GABA, etc.)
 * @param weight_mean Mean initial weight
 * @param weight_std Weight std for random initialization
 * @return Number of synapses created, or negative on error
 *
 * COMPLEXITY: O(n_src × n_dst × connectivity)
 */
int snn_network_connect_populations(snn_network_t* network,
                                    uint32_t src_pop,
                                    uint32_t dst_pop,
                                    snn_topology_t topology,
                                    float connectivity,
                                    synapse_type_t synapse_type,
                                    float weight_mean,
                                    float weight_std);

/**
 * @brief Finalize CSR storage on all lightweight populations
 *
 * WHAT: Sort COO entries by destination neuron, build per-row index for
 *       O(1) per-neuron lookup during stepping.
 * WHY:  Calls to snn_network_connect_populations() append unsorted COO
 *       entries; the lightweight stepping path requires CSR. This must
 *       be called once after all wiring is complete and before the first
 *       network step or save.
 * HOW:  For each lightweight population, sort its inbound CSR scratch
 *       and build row_ptr/col_idx/weights into the live arrays.
 *
 * @param network SNN network
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(N_synapses × log N_synapses) per population
 */
int snn_network_finalize_connections(snn_network_t* network);

/**
 * @brief Get population by ID
 *
 * WHAT: Access population structure
 * WHY:  Inspect or modify population state
 * HOW:  Look up in populations array
 *
 * @param network SNN network
 * @param pop_id Population ID
 * @return Population pointer, or NULL if not found
 *
 * COMPLEXITY: O(1)
 */
snn_population_t* snn_network_get_population(snn_network_t* network,
                                             uint32_t pop_id);

/** Phase-2 (2026-06-02): mark a pop EXEMPT from the global homeostasis / R-STDP
 *  / intrinsic-reward auto-enrollment loops (set on the language projection pops
 *  so their warm-started weights aren't rescaled/reward-modulated and a
 *  burst-driven language pop can't poison global reward). Default false.
 *  Returns 0 on success, -1 on invalid pop. */
int snn_network_set_pop_exclude_from_plasticity(snn_network_t* network,
                                                uint32_t pop_id, bool exclude);
bool snn_network_get_pop_exclude_from_plasticity(const snn_network_t* network,
                                                 uint32_t pop_id);

/**
 * @brief Find population index by exact name match
 *
 * WHAT: Linear scan of network->populations[] for pop with matching name.
 * WHY:  Substrate consumers (TRN gain control, language adapters, etc.)
 *       attach by named pop rather than by raw ID, since IDs shift as
 *       populations are added across versions of the wiring schema.
 *
 * @param network SNN network
 * @param name    Exact pop name to match (max 63 chars; uses strncmp on full buffer)
 * @return Population index in [0, n_populations), or -1 if not found
 *
 * COMPLEXITY: O(n_populations)
 */
int snn_network_find_pop_by_name(const snn_network_t* network, const char* name);

/**
 * @brief Tag a population with its biological subclass.
 *
 * WHAT: Set pop->subclass; LIF parameter resolution (snn_pop_lif_params)
 *       picks subclass-specific τ_mem / t_ref overrides on subsequent calls.
 * WHY:  Hierarchical wiring (TRN, PV/SOM/VIP sub-pops) needs to mark which
 *       biological neuron-type each pop represents so the membrane
 *       dynamics match. Default after pop creation is SNN_NSC_PYRAMIDAL.
 *
 * @param network SNN network
 * @param pop_id  Population index
 * @param subclass Biological subclass tag
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 */
int snn_network_set_pop_subclass(snn_network_t* network,
                                 uint32_t pop_id,
                                 neuron_subclass_t subclass);

/**
 * @brief Set per-population gap-junction (electrical synapse) coupling weight.
 *
 * WHAT: Sets pop->gap_coupling. When > 0, the SNN hot loop adds
 *       dv += gap_coupling * (V_mean - V_n) to every neuron of the
 *       population each step (after membrane integration, before refractory),
 *       blending each neuron's voltage toward the population mean.
 * WHY:  Models Connexin-36 gap junctions between PV basket cells in cortex,
 *       which form a fast electrical syncytium and are the primary substrate
 *       for cortical gamma rhythm (30-80 Hz). Without gap coupling, PV pops
 *       fire asynchronously and gamma never emerges.
 * HOW:  Single ONE-thing setter — assigns the float; no side effects.
 *       Coupling is gated on conductance_mode in the hot loop (matches CB
 *       hot-loop convention), so it is only applied when the conductance
 *       runtime flag is on.
 *
 * @param network SNN network
 * @param pop_id  Population index in [0, n_populations)
 * @param weight  Gap-junction coupling weight (typical 0.05). Pass 0 to
 *                disable. Negative values are clamped to 0.
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 */
int snn_network_set_pop_gap_coupling(snn_network_t* network,
                                     uint32_t pop_id,
                                     float weight);

/**
 * @brief Set per-population axonal conduction delay (Wave E FFI fix).
 *
 * WHAT: Sets pop->conduction_delay_steps. Clamps to
 *       [0, SNN_MAX_CONDUCTION_DELAY_STEPS]. When > 0, downstream pops
 *       reading this pop's spike output via the per-pop spike-history
 *       ring buffer see spikes delayed by `steps` simulation steps.
 * WHY:  Restores the canonical feed-forward inhibition timing window
 *       (PV GABA arriving 1-3 ms after thalamic AMPA on shared pyr
 *       targets). Pre-Wave-E the deposit kernel read src->spike_output
 *       for the SAME tick the spike was emitted, collapsing effective
 *       conduction delay to 0 — making the FFI arm structurally
 *       feedback. See docs/claude/ffi-timing-audit-2026-04-27.md.
 * HOW:  Single ONE-thing setter — assigns the clamped value; no side
 *       effects. The hot loop's read-from-history path is gated on
 *       spike_history != NULL, so this setter does not allocate or
 *       free the ring buffer (it's allocated at population creation).
 *
 * @param network SNN network
 * @param pop_id  Population index in [0, n_populations)
 * @param steps   Conduction delay in simulation steps. Values above
 *                SNN_MAX_CONDUCTION_DELAY_STEPS are clamped down.
 * @return 0 on success, negative on error
 *
 * COMPLEXITY: O(1)
 */
int snn_network_set_pop_conduction_delay(snn_network_t* network,
                                         uint32_t pop_id,
                                         uint32_t steps);

/**
 * @brief Set per-population LIF heterogeneity (Wave G, 2026-04-27).
 *
 * WHAT: Sets pop->heterogeneity_sigma. When sigma > 0, allocates and
 *       populates pop->tau_mem_per_neuron and pop->v_thresh_per_neuron
 *       with values drawn from a Gaussian centered on the pop-wide
 *       resolved params:
 *           tau_mem_per_neuron[n]  = lif.tau_mem  × (1 + sigma × N(0,1))
 *           v_thresh_per_neuron[n] = lif.v_thresh × (1 + sigma × N(0,1))
 *       Independent draws per neuron and per field. Box-Muller using
 *       nimcp_tl_rand() for the underlying uniform.
 * WHY:  Real cortical pops have τ_mem and threshold distributions.
 *       Homogeneous pops are a degenerate failure mode that produces
 *       lock-step firing instead of asynchronous-irregular dynamics.
 * HOW:  Single ONE-thing setter — clamps σ to [0, 0.5], allocates arrays
 *       if needed, populates them. No side effects beyond pop->* fields.
 *       sigma == 0 leaves arrays NULL (the LIF lookup helper falls back
 *       to pop-wide values in that case).
 *
 * @param network SNN network
 * @param pop_id  Population index in [0, n_populations)
 * @param sigma   Relative σ. Clamped to [0, 0.5]. NaN/Inf rejected.
 *                A typical biological value for tier pyramidal pops is 0.10.
 * @return 0 on success, negative on error.
 *
 * COMPLEXITY: O(n_neurons) on first non-zero call, O(1) thereafter.
 */
int snn_network_set_pop_heterogeneity(snn_network_t* network,
                                      uint32_t pop_id,
                                      float sigma);

/**
 * @brief Enable two-compartment dendritic mode on a population (Wave H).
 *
 * WHAT: Allocates the 8 per-neuron arrays used by the two-compartment
 *       integration path:
 *         v_basal, v_apical              — compartment voltages
 *         g_ampa_basal, g_gaba_a_basal   — basal-side conductance buckets
 *         g_nmda_apical, g_gaba_b_apical — apical-side conductance buckets
 *         plateau_active, plateau_t0     — NMDA plateau onset state
 *       and sets pop->dendritic_enabled = true. v_basal and v_apical are
 *       both initialized to v_rest so a fresh dendritic pop starts at
 *       resting potential.
 * WHY:  Apical NMDA plateau spikes drive Larkum BAC firing. Real
 *       cortical pyramidal cells need two compartments; lightweight pops
 *       default to single-compartment to keep memory + perf overhead off
 *       for production runs. This setter opts a pop in.
 * HOW:  Single ONE-thing setter — allocates the 8 arrays. On any alloc
 *       failure, frees what was acquired and returns SNN_ERROR_OUT_OF_MEMORY.
 *       Idempotent: a second call when arrays are already allocated is a
 *       no-op.
 *
 * @param network SNN network
 * @param pop_id  Population index in [0, n_populations)
 * @return SNN_SUCCESS on success, SNN_ERROR_* on error.
 *
 * COMPLEXITY: O(n_neurons) on first non-failing call.
 *
 * See docs/claude/wave-h-dendritic-design-2026-04-27.md.
 */
int snn_network_enable_dendritic(snn_network_t* network, uint32_t pop_id);

/*============================================================================
 * Wave H — global runtime flag for dendritic compartment mode.
 *
 * Default 0.0 (OFF). Hierarchical wiring sets pop->dendritic_enabled on
 * tier-pyramidal pops only when this flag is non-zero at the time of
 * pop creation. Like the conductance flag, the gate must be set BEFORE
 * brain init for it to apply across the production hierarchy.
 *==========================================================================*/
extern void  snn_tune_set_dendritic_enabled(float v);
extern float snn_tune_get_dendritic_enabled(void);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get network statistics
 *
 * WHAT: Fill statistics structure with current metrics
 * WHY:  Monitor network health and performance
 * HOW:  Aggregate metrics from all populations
 *
 * @param network SNN network
 * @param stats Statistics structure to fill
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_populations)
 */
int snn_network_get_stats(snn_network_t* network, snn_stats_t* stats);

/**
 * @brief Check network health
 *
 * WHAT: Detect simulation issues (silence, explosion, NaN)
 * WHY:  Early warning for training problems
 * HOW:  Check firing rates, weight norms, voltage ranges
 *
 * @param network SNN network
 * @return Health status enum
 *
 * COMPLEXITY: O(n_populations)
 */
snn_state_health_t snn_network_check_health(snn_network_t* network);

/**
 * @brief Get firing rate for a neuron
 *
 * WHAT: Compute instantaneous firing rate
 * WHY:  Monitor individual neuron activity
 * HOW:  Count spikes in recent time window
 *
 * @param network SNN network
 * @param pop_id Population ID
 * @param neuron_idx Neuron index within population
 * @param window_ms Time window for rate calculation
 * @return Firing rate in Hz
 *
 * COMPLEXITY: O(spike_count in window)
 */
float snn_network_get_firing_rate(snn_network_t* network,
                                  uint32_t pop_id,
                                  uint32_t neuron_idx,
                                  float window_ms);

/**
 * @brief Get population mean firing rate
 *
 * WHAT: Average firing rate across population
 * WHY:  Monitor population activity level
 *
 * @param network SNN network
 * @param pop_id Population ID
 * @param window_ms Time window for rate calculation
 * @return Mean firing rate in Hz
 *
 * COMPLEXITY: O(n_neurons × spike_count)
 */
float snn_network_get_population_rate(snn_network_t* network,
                                      uint32_t pop_id,
                                      float window_ms);

/**
 * @brief Copy a population's temporal spike-count history into a caller buffer.
 *
 * Returns the last SNN_POP_HISTORY_LEN steps of population-aggregate spike
 * counts, time-ordered oldest→newest. Used by Python eval tooling for FFT,
 * cross-correlation, and autocorrelation analysis of population dynamics.
 *
 * @param network    SNN network
 * @param pop_id     Population index
 * @param out_counts Caller-allocated buffer of SNN_POP_HISTORY_LEN uint32_t
 * @param out_total_steps Optional — receives the monotonic step counter
 * @return Number of valid entries in out_counts (may be less than
 *         SNN_POP_HISTORY_LEN if the population hasn't run that many steps),
 *         or 0 on error.
 */
uint32_t snn_network_get_population_history(snn_network_t* network,
                                            uint32_t pop_id,
                                            uint32_t* out_counts,
                                            uint64_t* out_total_steps);

/**
 * @brief Emergency saturation rescue — run homeostatic apply N times in a row.
 *
 * When the SNN drifts far from biological range (e.g., 90%+ firing after
 * a bad training episode), normal homeostatic cadence (every 10 R-STDP
 * applies) is too slow to recover — can take hours. This force-applies the
 * homeostatic scaling N times in rapid succession, scaling weights by
 * ~0.9^N. 20 iterations drops firing rate by 8×.
 *
 * @param network SNN network
 * @param ctx     snn_training_ctx_t*
 * @param n_iter  How many back-to-back applies to run
 * @return Total populations scaled across all iterations
 */
uint32_t snn_network_force_homeostasis(snn_network_t* network,
                                       void* ctx,
                                       uint32_t n_iter);

/**
 * @brief Get population firing rate directly from population pointer
 *
 * WHAT: Average firing rate across population (convenience wrapper)
 * WHY:  Allow bridges with population pointers to query firing rate
 * HOW:  Computes mean rate from population's neurons
 *
 * @param population Population pointer
 * @return Mean firing rate in Hz (0.0 if NULL or no spikes)
 *
 * COMPLEXITY: O(n_neurons)
 */
float snn_population_get_firing_rate(const snn_population_t* population);

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register SNN with bio-async messaging system
 * WHY:  Enable inter-module spike event communication
 * HOW:  Registers with bio_router, sets up message handlers
 *
 * @param network SNN network
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_network_connect_bio_async(snn_network_t* network);

/**
 * @brief Disconnect from bio-async router
 *
 * @param network SNN network
 * @return SNN_SUCCESS on success
 */
int snn_network_disconnect_bio_async(snn_network_t* network);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Integrate SNN with immune system
 * WHY:  Cytokine modulation of excitability and plasticity
 * HOW:  Creates immune bridge, registers for cytokine updates
 *
 * @param network SNN network
 * @param immune Brain immune system handle
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(1)
 */
int snn_network_connect_immune(snn_network_t* network, void* immune);

/**
 * @brief Apply immune modulation effects
 *
 * WHAT: Update SNN parameters based on immune state
 * WHY:  Model inflammatory effects on neural activity
 * HOW:  Modulate threshold, learning rate, time constants
 *
 * @param network SNN network
 * @return SNN_SUCCESS on success
 *
 * COMPLEXITY: O(n_populations)
 */
int snn_network_apply_immune_modulation(snn_network_t* network);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get underlying neural_network handle
 *
 * WHAT: Access the underlying neural_network_t
 * WHY:  Direct access for advanced operations
 * HOW:  Return internal handle
 *
 * @param network SNN network
 * @return neural_network_t handle, or NULL
 */
neural_network_t snn_network_get_neural_net(snn_network_t* network);

/**
 * @brief Validate network structure
 *
 * WHAT: Check network integrity
 * WHY:  Debug and validation
 * HOW:  Verify magic number, population references, etc.
 *
 * @param network SNN network
 * @return SNN_SUCCESS if valid
 */
int snn_network_validate(const snn_network_t* network);

/**
 * @brief Validate Dale's principle across the network's wiring.
 *
 * WHAT: Walk every dst population's synapse_type_per_src table, build a
 *       reverse map src_pop -> {emits-excitatory, emits-inhibitory}, and
 *       flag any source population that emits BOTH classes. Excitatory =
 *       AMPA or NMDA; inhibitory = GABA_A or GABA_B. Modulatory /
 *       electrical / generic synapses are ignored (Dale governs classical
 *       fast neurotransmitters; co-released neuromodulators do not violate
 *       it).
 * WHY:  Dale's principle is a fundamental biological constraint. The
 *       per-receptor migration (g_ampa/g_nmda/g_gaba_a/g_gaba_b) makes the
 *       constraint observable for the first time — a pop emitting both
 *       AMPA and GABA_A would be biophysically impossible.
 * HOW:  Pure read. No wiring is mutated. Caller-provided err_buf collects
 *       a human-readable description of every violation (truncated if the
 *       buffer is too small).
 *
 * @param net          SNN network (may be NULL → returns 0)
 * @param err_buf      Caller buffer for violation messages (may be NULL)
 * @param err_buf_sz   Size of err_buf in bytes (ignored if err_buf NULL)
 * @return 0 if Dale holds for every src pop; non-zero violation count
 *         otherwise (one count per offending source population).
 *
 * COMPLEXITY: O(n_populations × SNN_MAX_POPULATIONS) bit ops, negligible.
 */
int snn_network_validate_dale(const snn_network_t* net,
                              char* err_buf,
                              size_t err_buf_sz);

/**
 * @brief Save SNN network (config + neuron weights) to file
 * @return 0 on success, -1 on error
 */
int snn_network_save(snn_network_t* network, const char* path);

/**
 * @brief Load SNN network from file
 * @return Network handle, or NULL on error
 */
snn_network_t* snn_network_load(const char* path);

/**
 * @brief Create a hierarchical SNN with cortical-inspired population topology
 *
 * WHAT: Creates a 1.8M-neuron SNN organized into 46 populations across 8 tiers
 * WHY:  The SNN is the primary brain — needs biological cortical hierarchy
 * HOW:  Builds populations tier-by-tier with feedforward, recurrent, and skip
 *       connections using efficient direct-sampling connectivity
 *
 * @param n_inputs Brain input dimension (for input population sizing)
 * @param n_outputs Brain output dimension (for output population sizing)
 * @param target_total_neurons Target total neuron count (actual may differ slightly)
 * @return Network handle, or NULL on error
 */
snn_network_t* snn_create_hierarchical_network(
    uint32_t n_inputs,
    uint32_t n_outputs,
    uint32_t target_total_neurons);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_NETWORK_H */
