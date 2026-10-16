/**
 * @file nimcp_receptor_subtypes.h
 * @brief Neurotransmitter receptor subtype modeling (Phase C2.2)
 *
 * WHAT: Models receptor subtypes (D1-D5, 5-HT1-7, etc.) with binding kinetics
 * WHY:  Different receptors have different effects (D1 excitatory, D2 inhibitory)
 * HOW:  Hill equation binding, per-neuron receptor expression profiles
 *
 * @version Phase C2.2 Enhancement #1
 * @date 2025-11-12
 */

#ifndef NIMCP_RECEPTOR_SUBTYPES_H
#define NIMCP_RECEPTOR_SUBTYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Receptor Subtype Enumerations
// ============================================================================

/**
 * @brief Dopamine receptor subtypes
 *
 * Biological facts:
 * - D1/D5: Gs-coupled, excitatory (increase cAMP)
 * - D2/D3/D4: Gi-coupled, inhibitory (decrease cAMP)
 */
typedef enum {
    DOPAMINE_D1 = 0,    /**< D1 receptor - excitatory, high PFC expression */
    DOPAMINE_D2 = 1,    /**< D2 receptor - inhibitory, striatum autoreceptor */
    DOPAMINE_D3 = 2,    /**< D3 receptor - inhibitory, limbic system */
    DOPAMINE_D4 = 3,    /**< D4 receptor - inhibitory, prefrontal cortex */
    DOPAMINE_D5 = 4,    /**< D5 receptor - excitatory, hippocampus */
    DOPAMINE_RECEPTOR_COUNT = 5
} dopamine_receptor_subtype_t;

/**
 * @brief Serotonin receptor subtypes (simplified - 7 major families)
 *
 * Biological facts:
 * - 5-HT1A: Gi-coupled, anxiolytic, autoreceptor
 * - 5-HT2A: Gq-coupled, hallucinogenic, excitatory
 * - 5-HT3: Ionotropic, fast excitatory
 */
typedef enum {
    SEROTONIN_5HT1A = 0,  /**< 5-HT1A - inhibitory, raphe autoreceptor */
    SEROTONIN_5HT1B = 1,  /**< 5-HT1B - inhibitory, basal ganglia */
    SEROTONIN_5HT2A = 2,  /**< 5-HT2A - excitatory, cortex (psychedelics) */
    SEROTONIN_5HT2C = 3,  /**< 5-HT2C - excitatory, appetite regulation */
    SEROTONIN_5HT3 = 4,   /**< 5-HT3 - ionotropic, fast excitation */
    SEROTONIN_5HT4 = 5,   /**< 5-HT4 - excitatory, GI tract */
    SEROTONIN_5HT7 = 6,   /**< 5-HT7 - excitatory, circadian rhythm */
    SEROTONIN_RECEPTOR_COUNT = 7
} serotonin_receptor_subtype_t;

/**
 * @brief Acetylcholine receptor subtypes (simplified)
 *
 * Biological facts:
 * - Nicotinic: Ionotropic, fast excitatory
 * - Muscarinic M1/M3/M5: Gq-coupled, excitatory
 * - Muscarinic M2/M4: Gi-coupled, inhibitory
 */
typedef enum {
    ACH_NICOTINIC = 0,    /**< Nicotinic - ionotropic, fast excitation */
    ACH_MUSCARINIC_M1 = 1, /**< M1 - excitatory, cortex/striatum */
    ACH_MUSCARINIC_M2 = 2, /**< M2 - inhibitory, heart/autoreceptor */
    ACH_MUSCARINIC_M3 = 3, /**< M3 - excitatory, smooth muscle */
    ACH_MUSCARINIC_M4 = 4, /**< M4 - inhibitory, striatum */
    ACH_MUSCARINIC_M5 = 5, /**< M5 - excitatory, VTA/substantia nigra */
    ACH_RECEPTOR_COUNT = 6
} acetylcholine_receptor_subtype_t;

/**
 * @brief Norepinephrine receptor subtypes
 *
 * Biological facts:
 * - α1: Gq-coupled, excitatory (arousal)
 * - α2: Gi-coupled, inhibitory (autoreceptor)
 * - β1/β2/β3: Gs-coupled, excitatory (learning, memory)
 */
typedef enum {
    NOREPINEPHRINE_ALPHA1 = 0, /**< α1 - excitatory, arousal */
    NOREPINEPHRINE_ALPHA2 = 1, /**< α2 - inhibitory, autoreceptor */
    NOREPINEPHRINE_BETA1 = 2,  /**< β1 - excitatory, cardiac */
    NOREPINEPHRINE_BETA2 = 3,  /**< β2 - excitatory, learning */
    NOREPINEPHRINE_BETA3 = 4,  /**< β3 - excitatory, metabolism */
    NOREPINEPHRINE_RECEPTOR_COUNT = 5
} norepinephrine_receptor_subtype_t;

// ============================================================================
// Receptor Configuration and State
// ============================================================================

/**
 * @brief Single receptor subtype configuration
 *
 * Contains binding affinity, expression level, and functional properties
 */
typedef struct {
    float kd;                  /**< Dissociation constant (µM) - lower = stronger binding */
    float hill_coefficient;    /**< Hill coefficient (cooperativity) - typically 1.0 */
    float expression_level;    /**< Receptor density [0-1] - 1.0 = maximal expression */
    float max_effect;          /**< Maximum functional effect [0-1] */
    bool is_excitatory;        /**< true = excitatory, false = inhibitory */
    float desensitization_rate; /**< Rate of receptor desensitization (1/s) */
} receptor_config_t;

/**
 * @brief Runtime receptor state for a single subtype
 *
 * Tracks current occupancy, desensitization, and functional output
 */
typedef struct {
    float occupancy;           /**< Current receptor occupancy [0-1] */
    float desensitization;     /**< Desensitization level [0-1] - 1.0 = fully desensitized */
    float functional_output;   /**< Actual functional effect accounting for desensitization */
} receptor_state_t;

/**
 * @brief Dopamine receptor system (all 5 subtypes)
 *
 * Per-neuron receptor profile for dopamine
 */
typedef struct {
    receptor_config_t config[DOPAMINE_RECEPTOR_COUNT];
    receptor_state_t state[DOPAMINE_RECEPTOR_COUNT];

    float free_concentration;  /**< Free (unbound) dopamine concentration (µM) */
    float total_excitation;    /**< Sum of D1/D5 excitatory effects */
    float total_inhibition;    /**< Sum of D2/D3/D4 inhibitory effects */
    float net_modulation;      /**< total_excitation - total_inhibition */
} dopamine_receptor_system_t;

/**
 * @brief Serotonin receptor system (7 major subtypes)
 */
typedef struct {
    receptor_config_t config[SEROTONIN_RECEPTOR_COUNT];
    receptor_state_t state[SEROTONIN_RECEPTOR_COUNT];

    float free_concentration;
    float total_excitation;
    float total_inhibition;
    float net_modulation;
} serotonin_receptor_system_t;

/**
 * @brief Acetylcholine receptor system (6 subtypes)
 */
typedef struct {
    receptor_config_t config[ACH_RECEPTOR_COUNT];
    receptor_state_t state[ACH_RECEPTOR_COUNT];

    float free_concentration;
    float total_excitation;
    float total_inhibition;
    float net_modulation;
} acetylcholine_receptor_system_t;

/**
 * @brief Norepinephrine receptor system (5 subtypes)
 */
typedef struct {
    receptor_config_t config[NOREPINEPHRINE_RECEPTOR_COUNT];
    receptor_state_t state[NOREPINEPHRINE_RECEPTOR_COUNT];

    float free_concentration;
    float total_excitation;
    float total_inhibition;
    float net_modulation;
} norepinephrine_receptor_system_t;

/**
 * @brief Complete receptor profile for a single neuron
 *
 * Contains all receptor systems for all neurotransmitters
 */
typedef struct {
    dopamine_receptor_system_t dopamine;
    serotonin_receptor_system_t serotonin;
    acetylcholine_receptor_system_t acetylcholine;
    norepinephrine_receptor_system_t norepinephrine;
} neuron_receptor_profile_t;

// ============================================================================
// Biological Parameter Constants
// ============================================================================

/**
 * @brief Dopamine receptor binding affinities (Kd in nM)
 *
 * Source: Beaulieu & Gainetdinov (2011) Pharmacological Reviews
 */
#define DOPAMINE_D1_KD  (5.0e-3f)   /**< D1 Kd = 5 nM = 0.005 µM */
#define DOPAMINE_D2_KD  (0.5e-3f)   /**< D2 Kd = 0.5 nM = 0.0005 µM (high affinity) */
#define DOPAMINE_D3_KD  (1.0e-3f)   /**< D3 Kd = 1 nM */
#define DOPAMINE_D4_KD  (2.0e-3f)   /**< D4 Kd = 2 nM */
#define DOPAMINE_D5_KD  (8.0e-3f)   /**< D5 Kd = 8 nM */

/**
 * @brief Serotonin receptor binding affinities (Kd in nM)
 */
#define SEROTONIN_5HT1A_KD  (1.0e-3f)   /**< 5-HT1A Kd = 1 nM */
#define SEROTONIN_5HT2A_KD  (2.0e-3f)   /**< 5-HT2A Kd = 2 nM */

// ============================================================================
// Function Declarations
// ============================================================================

/**
 * @brief Create default receptor configuration for dopamine subtype
 *
 * @param subtype Dopamine receptor subtype
 * @return Default configuration with biological Kd values
 */
receptor_config_t dopamine_receptor_default_config(dopamine_receptor_subtype_t subtype);

/**
 * @brief Create cortical receptor profile (high D1, moderate D2)
 *
 * Typical for prefrontal cortex neurons
 *
 * @return Receptor profile with cortical expression pattern
 */
neuron_receptor_profile_t receptor_profile_cortical(void);

/**
 * @brief Create striatal receptor profile (high D2, moderate D1)
 *
 * Typical for striatum neurons (medium spiny neurons)
 *
 * @return Receptor profile with striatal expression pattern
 */
neuron_receptor_profile_t receptor_profile_striatal(void);

/**
 * @brief Update receptor binding based on free neurotransmitter concentration
 *
 * Uses Hill equation: occupancy = [NT]^n / (Kd^n + [NT]^n)
 *
 * @param config Receptor configuration (Kd, Hill coefficient)
 * @param state Receptor state to update (occupancy)
 * @param free_concentration Free neurotransmitter concentration (µM)
 * @param dt Time step (seconds)
 */
void receptor_update_binding(
    const receptor_config_t* config,
    receptor_state_t* state,
    float free_concentration,
    float dt
);

/**
 * @brief Compute net dopamine modulation for a neuron
 *
 * Combines excitatory (D1/D5) and inhibitory (D2/D3/D4) receptor effects
 *
 * @param system Dopamine receptor system
 * @param free_concentration Current free dopamine concentration (µM)
 * @param dt Time step (seconds)
 * @return Net modulation [-1 to +1] (negative = inhibition, positive = excitation)
 */
float dopamine_receptor_compute_modulation(
    dopamine_receptor_system_t* system,
    float free_concentration,
    float dt
);

/**
 * @brief Compute net serotonin modulation for a neuron
 *
 * Combines excitatory and inhibitory serotonin receptor effects
 *
 * @param system Serotonin receptor system
 * @param free_concentration Current free serotonin concentration (µM)
 * @param dt Time step (seconds)
 * @return Net modulation [-1 to +1]
 */
float serotonin_receptor_compute_modulation(
    serotonin_receptor_system_t* system,
    float free_concentration,
    float dt
);

/**
 * @brief Simulate D2 receptor blockade (antipsychotic drugs)
 *
 * Models drugs like risperidone, haloperidol that block D2 receptors
 *
 * @param system Dopamine receptor system
 * @param blockade_fraction Fraction of D2 receptors blocked [0-1]
 */
void dopamine_receptor_apply_d2_blockade(
    dopamine_receptor_system_t* system,
    float blockade_fraction
);

/**
 * @brief Simulate selective serotonin reuptake inhibitor (SSRI) effect
 *
 * SSRIs increase synaptic serotonin by blocking reuptake
 * This function increases the free_concentration to simulate that effect
 *
 * @param system Serotonin receptor system
 * @param reuptake_inhibition Fraction of reuptake blocked [0-1]
 * @param baseline_concentration Baseline concentration before SSRI
 * @return New free concentration after SSRI effect
 */
float serotonin_receptor_apply_ssri(
    serotonin_receptor_system_t* system,
    float reuptake_inhibition,
    float baseline_concentration
);

/**
 * @brief Initialize receptor system with default configurations
 *
 * @param system Receptor system to initialize
 */
void dopamine_receptor_system_init(dopamine_receptor_system_t* system);
void serotonin_receptor_system_init(serotonin_receptor_system_t* system);
void acetylcholine_receptor_system_init(acetylcholine_receptor_system_t* system);
void norepinephrine_receptor_system_init(norepinephrine_receptor_system_t* system);

/**
 * @brief Initialize complete neuron receptor profile
 *
 * @param profile Receptor profile to initialize
 * @param region Brain region type (determines expression pattern)
 */
typedef enum {
    BRAIN_REGION_CORTEX,
    BRAIN_REGION_STRIATUM,
    BRAIN_REGION_HIPPOCAMPUS,
    BRAIN_REGION_THALAMUS,
    BRAIN_REGION_GENERIC
} brain_region_t;

void neuron_receptor_profile_init(neuron_receptor_profile_t* profile, brain_region_t region);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RECEPTOR_SUBTYPES_H */
