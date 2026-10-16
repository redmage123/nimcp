/**
 * @file nimcp_neuromodulators.h
 * @brief Neurotransmitter and neuromodulator system for biological realism
 *
 * WHAT: Models 4 major neuromodulatory systems:
 *       - Dopamine (reward, motivation, learning)
 *       - Serotonin (mood, inhibition, patience)
 *       - Acetylcholine (attention, encoding, salience)
 *       - Norepinephrine (arousal, alertness, focus)
 *
 * WHY:  Neuromodulators are critical for:
 *       - Gating plasticity (when to learn)
 *       - Attention allocation (what to process)
 *       - Memory consolidation (what to remember)
 *       - Emotional context (how to value information)
 *
 * HOW:  Two-level architecture:
 *       1. Global: Broadcast neuromodulator levels (volume transmission)
 *       2. Local: Synapse-specific receptor densities (targeted modulation)
 *
 * BIOLOGICAL MAPPING:
 * - Dopamine: VTA/SNc → striatum, cortex (reward prediction error)
 * - Serotonin: Raphe nuclei → widespread (mood, inhibition)
 * - Acetylcholine: Basal forebrain → cortex, hippocampus (attention)
 * - Norepinephrine: Locus coeruleus → cortex (arousal)
 *
 * RAG ETHICS INTEGRATION:
 * - High ethics → dopamine (reward good information)
 * - Low ethics → serotonin (inhibit bad information)
 * - High salience → acetylcholine (attend to important)
 * - Threat detected → norepinephrine (vigilance)
 *
 * DESIGN PATTERNS:
 * - Observer: Neurons observe global neuromodulator changes
 * - Strategy: Different modulation strategies per neurotransmitter
 * - Singleton: Global neuromodulator pool
 * - Mediator: Centralized coordination of modulation
 *
 * PERFORMANCE: O(1) to query levels, O(n) to broadcast updates
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_NEUROMODULATORS_H
#define NIMCP_NEUROMODULATORS_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/tensor/nimcp_tensor.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sleep state type defined in cognitive/nimcp_sleep_wake.h (already included via neuralnet.h → bcm.h) */

//=============================================================================
// Neurotransmitter Types
//=============================================================================

/**
 * @brief Major neuromodulator systems
 */
typedef enum {
    NEUROMOD_DOPAMINE,      /**< DA: Reward, motivation, learning */
    NEUROMOD_SEROTONIN,     /**< 5-HT: Mood, inhibition, patience */
    NEUROMOD_ACETYLCHOLINE, /**< ACh: Attention, encoding, salience */
    NEUROMOD_NOREPINEPHRINE,/**< NE: Arousal, alertness, stress */
    NEUROMOD_GABA,          /**< GABA: Inhibition (fast) */
    NEUROMOD_GLUTAMATE,     /**< GLU: Excitation (fast) */
    NEUROMOD_COUNT          /**< Total count */
} neuromodulator_type_t;

/**
 * @brief Receptor types for each neuromodulator
 */
typedef enum {
    // Dopamine receptors
    RECEPTOR_D1,  /**< D1: Excitatory, increases plasticity */
    RECEPTOR_D2,  /**< D2: Inhibitory, decreases plasticity */

    // Serotonin receptors
    RECEPTOR_5HT1A, /**< 5-HT1A: Inhibitory, reduces firing */
    RECEPTOR_5HT2A, /**< 5-HT2A: Excitatory, modulates perception */

    // Acetylcholine receptors
    RECEPTOR_NICOTINIC,  /**< Fast excitation */
    RECEPTOR_MUSCARINIC, /**< Slow modulation */

    // Norepinephrine receptors
    RECEPTOR_ALPHA1, /**< α1: Excitatory */
    RECEPTOR_ALPHA2, /**< α2: Inhibitory (auto-receptor) */
    RECEPTOR_BETA,   /**< β: Modulates plasticity */

    RECEPTOR_COUNT
} receptor_type_t;

//=============================================================================
// Neuromodulator Dynamics
//=============================================================================

/**
 * @brief Global neuromodulator concentrations (tensor-based)
 *
 * WHAT: System-wide "chemical bath" that affects all neurons
 * WHY:  Models volume transmission - slow, diffuse signaling
 * HOW:  Updated by events (rewards, threats, salience)
 *
 * BIOLOGICAL: Like cerebrospinal fluid concentrations
 * RANGE: All values 0.0 to 1.0 (normalized)
 *
 * TENSOR FORMAT:
 * - concentrations: [NEUROMOD_COUNT] - All neuromodulator levels in single tensor
 * - decay_rates: [NEUROMOD_COUNT] - Decay time constants per modulator
 *
 * BACKWARD COMPATIBILITY:
 * - Scalar accessors (dopamine, serotonin, etc.) provided via helper functions
 * - Legacy code can use neuromodulator_pool_get_dopamine(), etc.
 */
typedef struct {
    /**
     * @brief Tensor holding all neuromodulator concentrations
     * Shape: [NEUROMOD_COUNT], dtype: NIMCP_DTYPE_F32
     * Index mapping:
     *   [0] = dopamine
     *   [1] = serotonin
     *   [2] = acetylcholine
     *   [3] = norepinephrine
     *   [4] = gaba
     *   [5] = glutamate
     */
    nimcp_tensor_t* concentrations;

    /**
     * @brief Tensor holding decay time constants
     * Shape: [NEUROMOD_COUNT], dtype: NIMCP_DTYPE_F32
     */
    nimcp_tensor_t* decay_rates;

    /**
     * @brief Last update timestamp (microseconds)
     */
    uint64_t last_update;

    /**
     * @brief Flag indicating if tensors are owned (should be freed on destroy)
     */
    bool owns_tensors;
} neuromodulator_pool_t;

/**
 * @brief Create neuromodulator pool with tensor storage
 *
 * WHAT: Allocates tensors for concentrations and decay rates
 * WHY:  Initialize pool with proper tensor memory
 * HOW:  Creates [NEUROMOD_COUNT] tensors for each field
 *
 * @return Initialized pool (call neuromodulator_pool_destroy to free)
 */
neuromodulator_pool_t neuromodulator_pool_create(void);

/**
 * @brief Destroy neuromodulator pool and free tensor memory
 *
 * WHAT: Frees tensor resources in pool
 * WHY:  Proper cleanup of tensor memory
 *
 * @param pool Pool to destroy
 */
void neuromodulator_pool_destroy(neuromodulator_pool_t* pool);

/**
 * @brief Get dopamine concentration from pool
 *
 * WHAT: Accessor for dopamine level (backward compatibility)
 * WHY:  Allows legacy code to work with tensor-based pool
 *
 * @param pool Neuromodulator pool
 * @return Dopamine concentration (0-1)
 */
float neuromodulator_pool_get_dopamine(const neuromodulator_pool_t* pool);

/**
 * @brief Get serotonin concentration from pool
 */
float neuromodulator_pool_get_serotonin(const neuromodulator_pool_t* pool);

/**
 * @brief Get acetylcholine concentration from pool
 */
float neuromodulator_pool_get_acetylcholine(const neuromodulator_pool_t* pool);

/**
 * @brief Get norepinephrine concentration from pool
 */
float neuromodulator_pool_get_norepinephrine(const neuromodulator_pool_t* pool);

/**
 * @brief Get GABA concentration from pool
 */
float neuromodulator_pool_get_gaba(const neuromodulator_pool_t* pool);

/**
 * @brief Get glutamate concentration from pool
 */
float neuromodulator_pool_get_glutamate(const neuromodulator_pool_t* pool);

/**
 * @brief Set dopamine concentration in pool
 *
 * @param pool Neuromodulator pool
 * @param value New dopamine concentration (0-1)
 */
void neuromodulator_pool_set_dopamine(neuromodulator_pool_t* pool, float value);

/**
 * @brief Set serotonin concentration in pool
 */
void neuromodulator_pool_set_serotonin(neuromodulator_pool_t* pool, float value);

/**
 * @brief Set acetylcholine concentration in pool
 */
void neuromodulator_pool_set_acetylcholine(neuromodulator_pool_t* pool, float value);

/**
 * @brief Set norepinephrine concentration in pool
 */
void neuromodulator_pool_set_norepinephrine(neuromodulator_pool_t* pool, float value);

/**
 * @brief Set GABA concentration in pool
 */
void neuromodulator_pool_set_gaba(neuromodulator_pool_t* pool, float value);

/**
 * @brief Set glutamate concentration in pool
 */
void neuromodulator_pool_set_glutamate(neuromodulator_pool_t* pool, float value);

/**
 * @brief Get concentration by neuromodulator type
 *
 * @param pool Neuromodulator pool
 * @param type Neuromodulator type enum
 * @return Concentration value (0-1)
 */
float neuromodulator_pool_get_by_type(const neuromodulator_pool_t* pool, neuromodulator_type_t type);

/**
 * @brief Set concentration by neuromodulator type
 *
 * @param pool Neuromodulator pool
 * @param type Neuromodulator type enum
 * @param value New concentration (0-1)
 */
void neuromodulator_pool_set_by_type(neuromodulator_pool_t* pool, neuromodulator_type_t type, float value);

/**
 * @brief Receptor densities per neuron (tensor-based)
 *
 * WHAT: How sensitive each neuron is to each neuromodulator
 * WHY:  Different brain regions have different receptor distributions
 * HOW:  Multiplies global concentration by local receptor density
 *
 * BIOLOGICAL: Like receptor expression levels
 * RANGE: 0.0 = no receptors, 1.0 = maximum density
 *
 * TENSOR FORMAT:
 * - densities: [RECEPTOR_COUNT] - All receptor densities in single tensor
 *   Index mapping:
 *     [0] = D1 receptor density
 *     [1] = D2 receptor density
 *     [2] = 5-HT1A receptor density
 *     [3] = 5-HT2A receptor density
 *     [4] = Nicotinic ACh receptor density
 *     [5] = Muscarinic ACh receptor density
 *     [6] = α1-adrenergic receptor density
 *     [7] = α2-adrenergic receptor density
 *     [8] = β-adrenergic receptor density
 *
 * BACKWARD COMPATIBILITY:
 * - Legacy accessors provided for common receptor types
 */
typedef struct {
    /**
     * @brief Tensor holding all receptor densities
     * Shape: [RECEPTOR_COUNT], dtype: NIMCP_DTYPE_F32
     */
    nimcp_tensor_t* densities;

    /**
     * @brief Flag indicating if tensor is owned (should be freed on destroy)
     */
    bool owns_tensor;
} receptor_profile_t;

/**
 * @brief Create receptor profile with tensor storage
 *
 * WHAT: Allocates tensor for receptor densities
 * WHY:  Initialize profile with proper tensor memory
 *
 * @return Initialized profile (call receptor_profile_destroy to free)
 */
receptor_profile_t receptor_profile_create(void);

/**
 * @brief Destroy receptor profile and free tensor memory
 *
 * @param profile Profile to destroy
 */
void receptor_profile_destroy(receptor_profile_t* profile);

/**
 * @brief Get receptor density by type
 *
 * @param profile Receptor profile
 * @param type Receptor type enum
 * @return Density value (0-1)
 */
float receptor_profile_get_density(const receptor_profile_t* profile, receptor_type_t type);

/**
 * @brief Set receptor density by type
 *
 * @param profile Receptor profile
 * @param type Receptor type enum
 * @param value New density (0-1)
 */
void receptor_profile_set_density(receptor_profile_t* profile, receptor_type_t type, float value);

/* Backward compatibility accessors for common receptor types */
float receptor_profile_get_d1_density(const receptor_profile_t* profile);
float receptor_profile_get_d2_density(const receptor_profile_t* profile);
float receptor_profile_get_serotonin_density(const receptor_profile_t* profile);
float receptor_profile_get_nicotinic_density(const receptor_profile_t* profile);
float receptor_profile_get_alpha_density(const receptor_profile_t* profile);
float receptor_profile_get_beta_density(const receptor_profile_t* profile);

void receptor_profile_set_d1_density(receptor_profile_t* profile, float value);
void receptor_profile_set_d2_density(receptor_profile_t* profile, float value);
void receptor_profile_set_serotonin_density(receptor_profile_t* profile, float value);
void receptor_profile_set_nicotinic_density(receptor_profile_t* profile, float value);
void receptor_profile_set_alpha_density(receptor_profile_t* profile, float value);
void receptor_profile_set_beta_density(receptor_profile_t* profile, float value);

/**
 * @brief Neuromodulation effects on a synapse (tensor-based)
 *
 * WHAT: How neuromodulators change synaptic behavior
 * WHY:  Gates plasticity and transmission strength
 * HOW:  Computed from global levels × local receptors
 *
 * TENSOR FORMAT:
 * - effects: [4] tensor containing:
 *   [0] = learning_rate_multiplier (0-2)
 *   [1] = transmission_gain (0-2)
 *   [2] = excitability_shift (-0.5 to +0.5)
 *   [3] = attention_weight (0-1)
 */
typedef struct {
    /**
     * @brief Tensor holding all modulation effects
     * Shape: [4], dtype: NIMCP_DTYPE_F32
     */
    nimcp_tensor_t* effects;

    /**
     * @brief Flag indicating if tensor is owned
     */
    bool owns_tensor;
} modulation_effects_t;

/* Effect tensor indices */
#define MODULATION_EFFECT_LEARNING_RATE    0
#define MODULATION_EFFECT_TRANSMISSION     1
#define MODULATION_EFFECT_EXCITABILITY     2
#define MODULATION_EFFECT_ATTENTION        3
#define MODULATION_EFFECT_COUNT            4

/**
 * @brief Create modulation effects with tensor storage
 *
 * @return Initialized effects (call modulation_effects_destroy to free)
 */
modulation_effects_t modulation_effects_create(void);

/**
 * @brief Destroy modulation effects and free tensor memory
 *
 * @param effects Effects to destroy
 */
void modulation_effects_destroy(modulation_effects_t* effects);

/* Backward compatibility accessors */
float modulation_effects_get_learning_rate_multiplier(const modulation_effects_t* effects);
float modulation_effects_get_transmission_gain(const modulation_effects_t* effects);
float modulation_effects_get_excitability_shift(const modulation_effects_t* effects);
float modulation_effects_get_attention_weight(const modulation_effects_t* effects);

void modulation_effects_set_learning_rate_multiplier(modulation_effects_t* effects, float value);
void modulation_effects_set_transmission_gain(modulation_effects_t* effects, float value);
void modulation_effects_set_excitability_shift(modulation_effects_t* effects, float value);
void modulation_effects_set_attention_weight(modulation_effects_t* effects, float value);

//=============================================================================
// Neuromodulator System
//=============================================================================

/**
 * @brief Opaque handle to neuromodulator system
 */
typedef struct neuromodulator_system_struct* neuromodulator_system_t;

/**
 * @brief Configuration for neuromodulator system
 */
typedef struct {
    // Baseline concentrations
    float baseline_dopamine;
    float baseline_serotonin;
    float baseline_acetylcholine;
    float baseline_norepinephrine;

    // Decay time constants (seconds)
    float dopamine_decay;      /**< Default: 2.0s */
    float serotonin_decay;     /**< Default: 10.0s (slow) */
    float acetylcholine_decay; /**< Default: 0.5s (fast) */
    float norepinephrine_decay;/**< Default: 3.0s */

    // Response gains (how much to release per unit stimulus)
    float reward_dopamine_gain;     /**< Default: 0.5 */
    float threat_norepinephrine_gain;/**< Default: 0.7 */
    float salience_acetylcholine_gain;/**< Default: 0.6 */
    float punishment_serotonin_gain; /**< Default: 0.4 */

    // Volume transmission
    bool enable_volume_transmission; /**< Broadcast to all neurons */
    float diffusion_rate;            /**< Spatial spread rate */

} neuromodulator_config_t;

/**
 * @brief Create neuromodulator system
 *
 * @param config System configuration
 * @return Neuromodulator system handle or NULL on error
 */
neuromodulator_system_t neuromodulator_system_create(const neuromodulator_config_t* config);

/**
 * @brief Destroy neuromodulator system
 *
 * @param system System to destroy
 */
void neuromodulator_system_destroy(neuromodulator_system_t system);

/**
 * @brief Get current neuromodulator concentrations
 *
 * @param system Neuromodulator system
 * @param pool Output: current concentrations
 * @return true on success
 */
bool neuromodulator_get_levels(neuromodulator_system_t system, neuromodulator_pool_t* pool);

/**
 * @brief Get current level of a single neuromodulator
 *
 * WHAT: Convenience function to query one neuromodulator concentration
 * WHY:  Simpler API when only one level is needed
 *
 * @param system Neuromodulator system
 * @param type Neuromodulator type to query
 * @return Current concentration (0-1), or 0.0f on error
 */
float neuromodulator_get_level(neuromodulator_system_t system, neuromodulator_type_t type);

/**
 * @brief Get pointer to dopamine phasic-tonic state from neuromodulator system
 *
 * WHAT: Returns a pointer to the internal dopamine phasic-tonic state
 * WHY:  Needed by synapse compute for three-factor learning with burst detection
 * HOW:  Casts opaque handle to internal struct and returns field pointer
 *
 * @param system Neuromodulator system handle
 * @return Pointer to dopamine phasic-tonic state, or NULL if system is NULL
 */
phasic_tonic_state_t* neuromodulator_get_dopamine_phasic_tonic(neuromodulator_system_t system);

/**
 * @brief Set neuromodulator concentration directly
 *
 * @param system Neuromodulator system
 * @param type Neuromodulator type
 * @param level New concentration (0-1)
 * @return true on success
 */
bool neuromodulator_set_level(neuromodulator_system_t system, neuromodulator_type_t type,
                              float level);

/**
 * @brief Update neuromodulator dynamics (decay)
 *
 * WHAT: Applies exponential decay to all concentrations
 * WHY:  Neurotransmitters are cleared by reuptake/metabolism
 * HOW:  c(t+Δt) = c(t) × exp(-Δt/τ)
 *
 * COMPLEXITY: O(1) - fixed number of neuromodulators
 *
 * @param system Neuromodulator system
 * @param dt Time step (seconds)
 * @return true on success
 */
bool neuromodulator_update(neuromodulator_system_t system, float dt);

/**
 * @brief Process pending bio-async messages for neuromodulator system
 *
 * WHAT: Polls inbox and invokes handlers for pending messages
 * WHY:  Messages are queued; must be explicitly processed
 * HOW:  Delegates to bio_router_process_inbox
 *
 * NOTE: This function should be called periodically when using bio-async
 *       messaging with the neuromodulator system.
 *
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t neuromodulator_bio_async_process(uint32_t max_messages);

//=============================================================================
// Event-Driven Neuromodulator Release
//=============================================================================

/**
 * @brief Release dopamine in response to reward
 *
 * WHAT: Reward prediction error → dopamine burst
 * WHY:  Reinforcement learning signal
 * HOW:  δ = reward - prediction, DA ∝ δ
 *
 * BIOLOGICAL: Phasic dopamine from VTA
 *
 * @param system Neuromodulator system
 * @param reward_magnitude Reward size (0-1)
 * @param predicted_reward Expected reward (0-1)
 * @return Actual dopamine released (RPE)
 */
float neuromodulator_release_dopamine(neuromodulator_system_t system, float reward_magnitude,
                                     float predicted_reward);

/**
 * @brief Release serotonin for mood/inhibition
 *
 * WHAT: Punishment/aversion → serotonin increase
 * WHY:  Inhibits impulsive behavior, promotes patience
 * HOW:  5-HT ∝ punishment + uncertainty
 *
 * BIOLOGICAL: Raphe nuclei response to negative outcomes
 *
 * @param system Neuromodulator system
 * @param punishment_magnitude Punishment size (0-1)
 * @return Serotonin released
 */
float neuromodulator_release_serotonin(neuromodulator_system_t system, float punishment_magnitude);

/**
 * @brief Release acetylcholine for attention
 *
 * WHAT: Salience/surprise → acetylcholine burst
 * WHY:  Tags important information for encoding
 * HOW:  ACh ∝ |actual - expected|
 *
 * BIOLOGICAL: Basal forebrain response to salience
 *
 * @param system Neuromodulator system
 * @param salience Importance/surprise (0-1)
 * @return Acetylcholine released
 */
float neuromodulator_release_acetylcholine(neuromodulator_system_t system, float salience);

/**
 * @brief Release norepinephrine for arousal
 *
 * WHAT: Threat/stress → norepinephrine burst
 * WHY:  Increases vigilance and alertness
 * HOW:  NE ∝ threat_level + uncertainty
 *
 * BIOLOGICAL: Locus coeruleus response to threat
 *
 * @param system Neuromodulator system
 * @param threat_level Threat magnitude (0-1)
 * @param uncertainty Uncertainty level (0-1)
 * @return Norepinephrine released
 */
float neuromodulator_release_norepinephrine(neuromodulator_system_t system, float threat_level,
                                           float uncertainty);

//=============================================================================
// Local Receptor-Mediated Effects
//=============================================================================

/**
 * @brief Compute modulation effects for a neuron
 *
 * WHAT: Combines global levels with local receptor densities
 * WHY:  Different neurons respond differently to same modulator
 * HOW:  effect = global_concentration × receptor_density
 *
 * COMPLEXITY: O(1)
 *
 * @param system Neuromodulator system
 * @param receptors Neuron's receptor profile
 * @param effects Output: modulation effects
 * @return true on success
 */
bool neuromodulator_compute_effects(neuromodulator_system_t system,
                                   const receptor_profile_t* receptors,
                                   modulation_effects_t* effects);

/**
 * @brief Apply neuromodulation to learning rate
 *
 * WHAT: Scales plasticity based on neuromodulator levels
 * WHY:  Learning should be gated by context (reward, attention, etc.)
 * HOW:  effective_lr = base_lr × modulation_multiplier
 *
 * ALGORITHM:
 * learning_multiplier =
 *   1.0 +
 *   dopamine × d1_density × 0.5 -        # Enhances learning
 *   dopamine × d2_density × 0.3 +        # Suppresses learning
 *   acetylcholine × nicotinic × 0.4 +   # Enhances encoding
 *   norepinephrine × beta × 0.3         # Enhances consolidation
 *
 * @param base_learning_rate Unmodulated learning rate
 * @param effects Computed modulation effects
 * @return Modulated learning rate
 */
float neuromodulator_modulate_learning_rate(float base_learning_rate,
                                           const modulation_effects_t* effects);

/**
 * @brief Apply neuromodulation to synaptic transmission
 *
 * WHAT: Scales synaptic strength based on neuromodulators
 * WHY:  Attention should amplify relevant signals
 * HOW:  effective_weight = base_weight × transmission_gain
 *
 * @param base_weight Unmodulated synaptic weight
 * @param effects Computed modulation effects
 * @return Modulated synaptic weight
 */
float neuromodulator_modulate_transmission(float base_weight, const modulation_effects_t* effects);

/**
 * @brief Apply neuromodulation to firing threshold
 *
 * WHAT: Shifts neuron excitability based on arousal
 * WHY:  Norepinephrine increases alertness = lower threshold
 * HOW:  effective_threshold = base_threshold + excitability_shift
 *
 * @param base_threshold Unmodulated firing threshold
 * @param effects Computed modulation effects
 * @return Modulated firing threshold
 */
float neuromodulator_modulate_threshold(float base_threshold, const modulation_effects_t* effects);

//=============================================================================
// Receptor Profile Presets
//=============================================================================

/**
 * @brief Create receptor profile for cortical excitatory neuron
 *
 * High D1, moderate ACh, some NE sensitivity
 */
receptor_profile_t neuromodulator_profile_cortical_excitatory(void);

/**
 * @brief Create receptor profile for cortical inhibitory neuron
 *
 * High D2, high GABA, moderate 5-HT
 */
receptor_profile_t neuromodulator_profile_cortical_inhibitory(void);

/**
 * @brief Create receptor profile for hippocampal neuron
 *
 * High ACh (encoding), high D1 (reward), high 5-HT
 */
receptor_profile_t neuromodulator_profile_hippocampal(void);

/**
 * @brief Create receptor profile for striatal neuron
 *
 * Very high dopamine sensitivity (both D1 and D2)
 */
receptor_profile_t neuromodulator_profile_striatal(void);

/**
 * @brief Create receptor profile for amygdala neuron
 *
 * High NE (threat), high dopamine (valence), high 5-HT
 */
receptor_profile_t neuromodulator_profile_amygdala(void);

//=============================================================================
// Integration with Ethics/RAG System
//=============================================================================

/**
 * @brief Compute neuromodulator release from ethics evaluation
 *
 * WHAT: Maps ethics scores to neurotransmitter release
 * WHY:  Ethics = value system, neurotransmitters = value signals
 * HOW:  Direct mapping based on biological principles
 *
 * MAPPING:
 * - High golden_rule_score → dopamine (reward)
 * - Low golden_rule_score → serotonin (inhibit)
 * - High trustworthiness → acetylcholine (attend)
 * - High harm → norepinephrine (threat response)
 *
 * @param system Neuromodulator system
 * @param golden_rule_score Golden rule evaluation (-1 to +1)
 * @param trustworthiness Information trustworthiness (0-1)
 * @param harm_score Harm potential (0-1)
 * @param salience Information salience (0-1)
 * @return true on success
 */
bool neuromodulator_release_from_ethics(neuromodulator_system_t system, float golden_rule_score,
                                       float trustworthiness, float harm_score, float salience);

/**
 * @brief Get effective learning weight from neuromodulators
 *
 * WHAT: Converts neuromodulator state to training weight
 * WHY:  High DA + ACh = strong learning, high 5-HT = weak learning
 * HOW:  Weighted combination of modulator levels
 *
 * ALGORITHM:
 * weight =
 *   0.3 × dopamine +           # Reward
 *   0.3 × acetylcholine +      # Attention
 *   0.2 × (1 - serotonin) +    # Not inhibited
 *   0.2 × norepinephrine       # Aroused
 *
 * @param system Neuromodulator system
 * @param receptors Neuron receptor profile
 * @return Effective learning weight (0-1)
 */
float neuromodulator_get_learning_weight(neuromodulator_system_t system,
                                        const receptor_profile_t* receptors);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Neuromodulator system statistics
 */
typedef struct {
    // Current levels
    float current_dopamine;
    float current_serotonin;
    float current_acetylcholine;
    float current_norepinephrine;

    // Historical averages
    float avg_dopamine;
    float avg_serotonin;
    float avg_acetylcholine;
    float avg_norepinephrine;

    // Release events
    uint64_t dopamine_releases;
    uint64_t serotonin_releases;
    uint64_t acetylcholine_releases;
    uint64_t norepinephrine_releases;

    // Dynamics
    float dopamine_variance;
    float reward_prediction_accuracy;
} neuromodulator_stats_t;

/**
 * @brief Get neuromodulator system statistics
 *
 * @param system Neuromodulator system
 * @param stats Output statistics
 * @return true on success
 */
bool neuromodulator_get_stats(neuromodulator_system_t system, neuromodulator_stats_t* stats);

/**
 * @brief Reset neuromodulator levels to baseline
 *
 * @param system Neuromodulator system
 * @return true on success
 */
bool neuromodulator_reset(neuromodulator_system_t system);

//=============================================================================
// Sleep Integration
//=============================================================================

/**
 * @brief Set current sleep state for neuromodulator modulation
 *
 * WHAT: Update the sleep state that modulates neuromodulator profiles
 * WHY:  Sleep states fundamentally alter neuromodulator release and decay
 * HOW:  Store state, used in next update to apply sleep-based modulation
 *
 * BIOLOGICAL: ACh, NE, 5-HT, DA all vary dramatically across sleep stages
 *
 * @param system Neuromodulator system
 * @param sleep_state Current sleep state
 * @return true on success
 */
bool neuromodulator_set_sleep_state(neuromodulator_system_t system,
                                    sleep_state_t sleep_state);

/**
 * @brief Get current sleep state
 *
 * WHAT: Query the current sleep state affecting neuromodulators
 * WHY:  Check what modulation is being applied
 *
 * @param system Neuromodulator system
 * @return Current sleep state
 */
sleep_state_t neuromodulator_get_sleep_state(neuromodulator_system_t system);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_NEUROMODULATORS_H
