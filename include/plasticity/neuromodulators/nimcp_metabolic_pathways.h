/**
 * @file nimcp_metabolic_pathways.h
 * @brief Neurotransmitter metabolism: synthesis, degradation, and reuptake (Phase C2.4 Enhancement #4)
 *
 * WHAT: Models complete metabolic lifecycle of neurotransmitters
 * WHY:  Enables realistic pharmacological interventions and homeostatic regulation
 * HOW:  Enzyme kinetics, transporter dynamics, precursor availability
 *
 * BIOLOGICAL BASIS:
 * - Cooper et al. (2003): "The Biochemical Basis of Neuropharmacology"
 * - Iversen (1971): "The Uptake and Storage of Noradrenaline in Sympathetic Nerves"
 * - Nestler et al. (2009): "Molecular Neuropharmacology"
 *
 * METABOLIC PATHWAYS:
 *
 * 1. DOPAMINE:
 *    Synthesis:   Tyrosine → L-DOPA (TH) → Dopamine (AADC)
 *    Degradation: Dopamine → DOPAC (MAO) → HVA (COMT)
 *    Reuptake:    DAT (Km ~0.5 µM, Vmax ~1 nmol/min/mg)
 *
 * 2. SEROTONIN:
 *    Synthesis:   Tryptophan → 5-HTP (TPH) → Serotonin (AADC)
 *    Degradation: Serotonin → 5-HIAA (MAO)
 *    Reuptake:    SERT (Km ~0.3 µM, Vmax ~0.8 nmol/min/mg)
 *
 * 3. NOREPINEPHRINE:
 *    Synthesis:   Dopamine → Norepinephrine (DBH)
 *    Degradation: Norepinephrine → MHPG (MAO + COMT)
 *    Reuptake:    NET (Km ~0.4 µM, Vmax ~0.6 nmol/min/mg)
 *
 * 4. ACETYLCHOLINE:
 *    Synthesis:   Choline + Acetyl-CoA → ACh (ChAT)
 *    Degradation: ACh → Choline + Acetate (AChE, very fast: τ < 1ms)
 *    Reuptake:    ChT (high-affinity choline transporter)
 *
 * PHARMACOLOGICAL TARGETS:
 * - MAO inhibitors: Selegiline, phenelzine (antidepressants)
 * - COMT inhibitors: Tolcapone (Parkinson's disease)
 * - Reuptake inhibitors: SSRIs, cocaine, amphetamine
 * - Synthesis modulators: L-DOPA, α-methyltyrosine
 *
 * @version Phase C2.4 Enhancement #4
 * @date 2025-11-13
 */

#ifndef NIMCP_METABOLIC_PATHWAYS_H
#define NIMCP_METABOLIC_PATHWAYS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONSTANTS - Biological Parameters
//=============================================================================

/* Synthesis rates (µM/second) - from Cooper et al. (2003) */
#define METABOLISM_SYNTHESIS_RATE_DOPAMINE      0.0001f  /**< DA synthesis via TH+AADC */
#define METABOLISM_SYNTHESIS_RATE_SEROTONIN     0.00008f /**< 5-HT synthesis via TPH+AADC */
#define METABOLISM_SYNTHESIS_RATE_NOREPINEPHRINE 0.00006f /**< NE synthesis via DBH */
#define METABOLISM_SYNTHESIS_RATE_ACETYLCHOLINE 0.0002f  /**< ACh synthesis via ChAT */

/* Degradation rates (1/second) - enzyme activity */
#define METABOLISM_DEGRADATION_RATE_MAO         0.1f     /**< MAO: moderate (100ms τ) */
#define METABOLISM_DEGRADATION_RATE_COMT        0.05f    /**< COMT: slow (200ms τ) */
#define METABOLISM_DEGRADATION_RATE_ACHE        10.0f    /**< AChE: very fast (100µs τ) */

/* Reuptake parameters (Michaelis-Menten kinetics) */
#define METABOLISM_REUPTAKE_KM_DAT              0.0005f  /**< DAT Km: 0.5 µM */
#define METABOLISM_REUPTAKE_VMAX_DAT            0.001f   /**< DAT Vmax: 1 µM/s */
#define METABOLISM_REUPTAKE_KM_SERT             0.0003f  /**< SERT Km: 0.3 µM */
#define METABOLISM_REUPTAKE_VMAX_SERT           0.0008f  /**< SERT Vmax: 0.8 µM/s */
#define METABOLISM_REUPTAKE_KM_NET              0.0004f  /**< NET Km: 0.4 µM */
#define METABOLISM_REUPTAKE_VMAX_NET            0.0006f  /**< NET Vmax: 0.6 µM/s */
#define METABOLISM_REUPTAKE_KM_CHT              0.002f   /**< ChT Km: 2 µM */
#define METABOLISM_REUPTAKE_VMAX_CHT            0.002f   /**< ChT Vmax: 2 µM/s */

/* Precursor availability (µM) - typical brain concentrations */
#define METABOLISM_PRECURSOR_TYROSINE           50.0f    /**< Tyrosine: 50 µM */
#define METABOLISM_PRECURSOR_TRYPTOPHAN         10.0f    /**< Tryptophan: 10 µM */
#define METABOLISM_PRECURSOR_CHOLINE            20.0f    /**< Choline: 20 µM */

/* Enzyme cofactor levels (normalized 0-1) */
#define METABOLISM_COFACTOR_BH4                 1.0f     /**< Tetrahydrobiopterin (TH cofactor) */
#define METABOLISM_COFACTOR_PYRIDOXAL           1.0f     /**< Pyridoxal phosphate (AADC cofactor) */
#define METABOLISM_COFACTOR_ASCORBATE           1.0f     /**< Ascorbic acid (DBH cofactor) */

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Synthesis pathway state
 *
 * WHAT: Tracks precursor-to-neurotransmitter conversion
 * WHY:  Models rate-limiting steps in neurotransmitter production
 * HOW:  Enzyme kinetics with precursor and cofactor availability
 */
typedef struct {
    // Precursor availability
    float precursor_concentration;     /**< Precursor level (µM) */
    float precursor_influx_rate;       /**< Blood-brain barrier transport */

    // Enzyme activity
    float enzyme_activity;             /**< Enzyme expression level (0-1) */
    float cofactor_availability;       /**< Cofactor level (0-1) */

    // Synthesis rate
    float base_synthesis_rate;         /**< Maximum synthesis rate (µM/s) */
    float current_synthesis_rate;      /**< Actual rate with limitations */

    // Statistics
    uint64_t total_synthesized;        /**< Lifetime synthesis count */
    float avg_synthesis_rate;          /**< Moving average */

} synthesis_pathway_state_t;

/**
 * @brief Degradation pathway state
 *
 * WHAT: Tracks enzymatic breakdown of neurotransmitters
 * WHY:  Models clearance kinetics and metabolite production
 * HOW:  First-order kinetics for MAO/COMT/AChE
 */
typedef struct {
    // Enzyme parameters
    float enzyme_activity;             /**< Enzyme expression (0-1) */
    float degradation_rate;            /**< Rate constant (1/s) */
    float inhibitor_blockade;          /**< Inhibitor effect (0-1) */

    // Metabolite tracking
    float metabolite_concentration;    /**< Breakdown product level (µM) */

    // Statistics
    uint64_t total_degraded;           /**< Lifetime degradation events */
    float avg_degradation_rate;        /**< Moving average */

} degradation_pathway_state_t;

/**
 * @brief Reuptake transporter state
 *
 * WHAT: Tracks transporter-mediated clearance from synaptic cleft
 * WHY:  Models major mechanism of neurotransmitter inactivation
 * HOW:  Michaelis-Menten kinetics with competitive inhibition
 */
typedef struct {
    // Transporter parameters (Michaelis-Menten)
    float km;                          /**< Half-maximal concentration (µM) */
    float vmax;                        /**< Maximum rate (µM/s) */
    float transporter_density;         /**< Expression level (0-1) */

    // Inhibition state
    float inhibitor_concentration;     /**< Competitive inhibitor (µM) */
    float inhibitor_ki;                /**< Inhibitor binding constant */

    // Reversal (e.g., amphetamine)
    bool is_reversed;                  /**< Transporter running backwards? */
    float reversal_magnitude;          /**< Efflux rate when reversed */

    // Statistics
    uint64_t total_reuptake_events;    /**< Lifetime reuptake count */
    float avg_reuptake_rate;           /**< Moving average */

} reuptake_transporter_state_t;

/**
 * @brief Complete metabolic state for one neurotransmitter
 *
 * WHAT: Integrates synthesis, degradation, and reuptake
 * WHY:  Provides complete metabolic lifecycle
 * HOW:  Coupled differential equations for concentration dynamics
 */
typedef struct {
    // Pathways
    synthesis_pathway_state_t synthesis;
    degradation_pathway_state_t degradation;
    reuptake_transporter_state_t reuptake;

    // Current neurotransmitter concentration
    float concentration;               /**< Synaptic cleft concentration (µM) */

    // Vesicular storage (separate from cytoplasmic)
    float vesicular_concentration;     /**< Inside vesicles (µM) */
    float vesicular_uptake_rate;       /**< VMAT/VAChT rate (µM/s) */

    // Timing
    uint64_t last_update_time_us;      /**< Last update timestamp */

} metabolic_state_t;

/**
 * @brief Configuration for metabolic pathways
 */
typedef struct {
    // Synthesis parameters
    float synthesis_rate;
    float precursor_level;
    float enzyme_activity;

    // Degradation parameters
    float degradation_rate;
    float enzyme_expression;

    // Reuptake parameters
    float reuptake_km;
    float reuptake_vmax;
    float transporter_density;

    // Feature flags
    bool enable_synthesis;             /**< Model synthesis dynamics */
    bool enable_degradation;           /**< Model enzymatic breakdown */
    bool enable_reuptake;              /**< Model transporter clearance */

} metabolic_config_t;

//=============================================================================
// CORE API - Lifecycle
//=============================================================================

/**
 * @brief Initialize metabolic state with default parameters
 *
 * WHAT: Set up synthesis, degradation, and reuptake with biological defaults
 * WHY:  Provides ready-to-use metabolic dynamics
 * HOW:  Literature-based parameter initialization
 *
 * @param state Metabolic state to initialize
 */
void metabolic_state_init(metabolic_state_t* state);

/**
 * @brief Initialize with custom configuration
 *
 * @param state Metabolic state to initialize
 * @param config Custom configuration
 */
void metabolic_state_init_with_config(metabolic_state_t* state,
                                      const metabolic_config_t* config);

/**
 * @brief Reset metabolic state to initial conditions
 *
 * @param state Metabolic state to reset
 */
void metabolic_state_reset(metabolic_state_t* state);

/**
 * @brief Get default configuration for dopamine
 *
 * @return Default dopamine metabolic config
 */
metabolic_config_t metabolic_config_dopamine_default(void);

/**
 * @brief Get default configuration for serotonin
 *
 * @return Default serotonin metabolic config
 */
metabolic_config_t metabolic_config_serotonin_default(void);

/**
 * @brief Get default configuration for norepinephrine
 *
 * @return Default norepinephrine metabolic config
 */
metabolic_config_t metabolic_config_norepinephrine_default(void);

/**
 * @brief Get default configuration for acetylcholine
 *
 * @return Default acetylcholine metabolic config
 */
metabolic_config_t metabolic_config_acetylcholine_default(void);

//=============================================================================
// SYNTHESIS PATHWAYS
//=============================================================================

/**
 * @brief Compute neurotransmitter synthesis
 *
 * WHAT: Models precursor → neurotransmitter conversion
 * WHY:  Synthesis is rate-limiting step in neurotransmitter production
 * HOW:  Enzyme kinetics with precursor and cofactor availability
 *
 * BIOLOGICAL: Tyrosine hydroxylase (TH) is rate-limiting for catecholamines
 *
 * @param state Metabolic state
 * @param dt Time step (seconds)
 * @return Amount synthesized (µM)
 */
float metabolic_synthesize(metabolic_state_t* state, float dt);

/**
 * @brief Set precursor availability
 *
 * WHAT: Update precursor concentration (e.g., dietary tyrosine)
 * WHY:  Precursor depletion limits synthesis
 *
 * @param state Metabolic state
 * @param precursor_level New precursor concentration (µM)
 */
void metabolic_set_precursor(metabolic_state_t* state, float precursor_level);

/**
 * @brief Modulate enzyme activity
 *
 * WHAT: Change enzyme expression/activity level
 * WHY:  Models transcriptional regulation, feedback inhibition
 *
 * @param state Metabolic state
 * @param activity New enzyme activity (0-1)
 */
void metabolic_set_enzyme_activity(metabolic_state_t* state, float activity);

//=============================================================================
// DEGRADATION PATHWAYS
//=============================================================================

/**
 * @brief Compute enzymatic degradation
 *
 * WHAT: Models neurotransmitter → metabolite breakdown
 * WHY:  Degradation clears neurotransmitter from cleft
 * HOW:  First-order kinetics: dC/dt = -k × C
 *
 * BIOLOGICAL: MAO in mitochondria, COMT in cytoplasm, AChE in cleft
 *
 * @param state Metabolic state
 * @param dt Time step (seconds)
 * @return Amount degraded (µM)
 */
float metabolic_degrade(metabolic_state_t* state, float dt);

/**
 * @brief Apply MAO inhibitor
 *
 * WHAT: Block monoamine oxidase enzyme
 * WHY:  Antidepressant mechanism (selegiline, phenelzine)
 * HOW:  Reduces degradation rate
 *
 * @param state Metabolic state
 * @param inhibition Inhibition strength (0-1, 1=complete block)
 */
void metabolic_apply_mao_inhibitor(metabolic_state_t* state, float inhibition);

/**
 * @brief Apply COMT inhibitor
 *
 * WHAT: Block catechol-O-methyltransferase enzyme
 * WHY:  Parkinson's treatment (tolcapone)
 * HOW:  Reduces catecholamine degradation
 *
 * @param state Metabolic state
 * @param inhibition Inhibition strength (0-1)
 */
void metabolic_apply_comt_inhibitor(metabolic_state_t* state, float inhibition);

//=============================================================================
// REUPTAKE MECHANISMS
//=============================================================================

/**
 * @brief Compute transporter-mediated reuptake
 *
 * WHAT: Models clearance via plasma membrane transporters
 * WHY:  Primary inactivation mechanism (>80% of clearance)
 * HOW:  Michaelis-Menten kinetics: v = Vmax × [S] / (Km + [S])
 *
 * BIOLOGICAL: DAT, SERT, NET are Na+/Cl- dependent symporters
 *
 * @param state Metabolic state
 * @param concentration Current cleft concentration (µM)
 * @param dt Time step (seconds)
 * @return Amount removed by reuptake (µM)
 */
float metabolic_reuptake(metabolic_state_t* state, float concentration, float dt);

/**
 * @brief Apply reuptake inhibitor
 *
 * WHAT: Block neurotransmitter transporter
 * WHY:  Antidepressant mechanism (SSRIs, cocaine)
 * HOW:  Competitive inhibition increases Km
 *
 * @param state Metabolic state
 * @param inhibitor_concentration Inhibitor level (µM)
 * @param inhibitor_ki Binding constant (µM)
 */
void metabolic_apply_reuptake_inhibitor(metabolic_state_t* state,
                                        float inhibitor_concentration,
                                        float inhibitor_ki);

/**
 * @brief Reverse transporter direction
 *
 * WHAT: Make transporter run backwards (efflux instead of influx)
 * WHY:  Amphetamine mechanism - releases neurotransmitter
 * HOW:  Reverse Na+ gradient drives outward flux
 *
 * @param state Metabolic state
 * @param magnitude Reversal strength (0-1)
 */
void metabolic_reverse_transporter(metabolic_state_t* state, float magnitude);

//=============================================================================
// INTEGRATED UPDATE
//=============================================================================

/**
 * @brief Update complete metabolic state
 *
 * WHAT: Integrate synthesis, degradation, and reuptake
 * WHY:  Provides unified concentration dynamics
 * HOW:  Coupled ODEs: dC/dt = synthesis - degradation - reuptake
 *
 * @param state Metabolic state
 * @param dt Time step (seconds)
 * @param release_amount Vesicular release this timestep (µM)
 * @return New neurotransmitter concentration (µM)
 */
float metabolic_update(metabolic_state_t* state, float dt, float release_amount);

//=============================================================================
// STATISTICS & MONITORING
//=============================================================================

/**
 * @brief Get synthesis statistics
 *
 * @param state Metabolic state
 * @param total_synthesized Output: total amount synthesized
 * @param avg_rate Output: average synthesis rate
 */
void metabolic_get_synthesis_stats(const metabolic_state_t* state,
                                   uint64_t* total_synthesized,
                                   float* avg_rate);

/**
 * @brief Get degradation statistics
 *
 * @param state Metabolic state
 * @param total_degraded Output: total amount degraded
 * @param avg_rate Output: average degradation rate
 */
void metabolic_get_degradation_stats(const metabolic_state_t* state,
                                     uint64_t* total_degraded,
                                     float* avg_rate);

/**
 * @brief Get reuptake statistics
 *
 * @param state Metabolic state
 * @param total_events Output: total reuptake events
 * @param avg_rate Output: average reuptake rate
 */
void metabolic_get_reuptake_stats(const metabolic_state_t* state,
                                  uint64_t* total_events,
                                  float* avg_rate);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METABOLIC_PATHWAYS_H */
