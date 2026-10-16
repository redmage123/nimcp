/**
 * @file nimcp_second_messengers.h
 * @brief Intracellular second messenger cascade signaling system
 *
 * WHAT: Models intracellular signaling pathways from receptor activation:
 *       - cAMP pathway (Gs-coupled receptors: D1, beta-adrenergic)
 *       - IP3/DAG pathway (Gq-coupled receptors: 5-HT2A, mGluR1/5)
 *       - Calcium signaling (Ca2+/calmodulin, CaMKII)
 *       - Gene expression (CREB phosphorylation, immediate early genes)
 *
 * WHY: Receptors don't directly affect neurons - they trigger cascades:
 *       - Amplification: 1 receptor activation -> 100s of effector molecules
 *       - Timescale: seconds to minutes (slower than synaptic transmission)
 *       - Long-term effects: transcription factor activation, gene expression
 *       - Specificity: Different receptor subtypes trigger different cascades
 *
 * HOW: Two-tier architecture:
 *       1. Fast kinase cascades (PKA, PKC, CaMKII) - seconds timescale
 *       2. Slow gene expression (CREB -> IEGs) - minutes to hours timescale
 *
 * BIOLOGICAL MAPPING:
 * - cAMP Pathway: Gs-coupled receptor -> adenylyl cyclase -> cAMP -> PKA -> CREB
 * - IP3/DAG Pathway: Gq-coupled receptor -> PLC -> IP3 + DAG -> Ca2+ release + PKC
 * - Calcium Signaling: IP3 -> ER Ca2+ release -> calmodulin -> CaMKII -> CREB
 * - Gene Expression: CREB phosphorylation -> CRE-mediated transcription -> IEGs
 *
 * INTEGRATION POINTS:
 * - Receptor subtypes (D1/D2, 5-HT1A/2A, alpha/beta adrenergic)
 * - Neuromodulator system (existing)
 * - Plasticity mechanisms (STDP, LTP, LTD)
 * - Gene expression (new)
 * - Astrocyte calcium (existing)
 *
 * DESIGN PATTERNS:
 * - Observer: Downstream targets observe cascade state changes
 * - State: Different activation states affect cellular behavior
 * - Strategy: Different receptor types trigger different cascade strategies
 *
 * PERFORMANCE: O(1) for state queries, O(n) for cascade propagation
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#ifndef NIMCP_SECOND_MESSENGERS_H
#define NIMCP_SECOND_MESSENGERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/validation/nimcp_common.h"
#include "common/nimcp_export.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS AND CONFIGURATION
 *============================================================================*/

/** Maximum number of cascades per neuron */
#define SM_MAX_CASCADES_PER_NEURON 8

/** Maximum downstream targets per cascade */
#define SM_MAX_DOWNSTREAM_TARGETS 16

/** Default timestep for cascade dynamics (ms) */
#define SM_DEFAULT_DT_MS 1.0f

/** Concentration units */
#define SM_CAMP_BASELINE_UM 0.1f      /**< Baseline cAMP (micromolar) */
#define SM_CAMP_MAX_UM 10.0f          /**< Maximum cAMP (micromolar) */
#define SM_IP3_BASELINE_UM 0.05f      /**< Baseline IP3 (micromolar) */
#define SM_IP3_MAX_UM 5.0f            /**< Maximum IP3 (micromolar) */
#define SM_DAG_BASELINE 0.0f          /**< Baseline DAG (normalized) */
#define SM_DAG_MAX 1.0f               /**< Maximum DAG (normalized) */
#define SM_CA_BASELINE_NM 50.0f       /**< Baseline Ca2+ (nanomolar) */
#define SM_CA_MAX_NM 1000.0f          /**< Maximum Ca2+ (nanomolar) */

/** Enzyme kinetics parameters */
#define SM_KM_ADENYLYL_CYCLASE 0.5f   /**< Michaelis constant for AC */
#define SM_KM_PDE 1.0f                /**< Michaelis constant for PDE */
#define SM_KM_PLC 0.3f                /**< Michaelis constant for PLC */
#define SM_HILL_PKA 2.0f              /**< Hill coefficient for PKA */
#define SM_HILL_PKC 2.0f              /**< Hill coefficient for PKC */
#define SM_HILL_CAMKII 4.0f           /**< Hill coefficient for CaMKII */

/** Time constants (ms) */
#define SM_TAU_CAMP_SYNTHESIS 100.0f  /**< cAMP synthesis time constant */
#define SM_TAU_CAMP_DEGRADATION 500.0f /**< cAMP degradation (PDE) time constant */
#define SM_TAU_PKA_ACTIVATION 200.0f  /**< PKA activation time constant */
#define SM_TAU_IP3_SYNTHESIS 50.0f    /**< IP3 synthesis time constant */
#define SM_TAU_IP3_DEGRADATION 1000.0f /**< IP3 degradation time constant */
#define SM_TAU_DAG_SYNTHESIS 50.0f    /**< DAG synthesis time constant */
#define SM_TAU_DAG_DEGRADATION 3000.0f /**< DAG degradation time constant */
#define SM_TAU_CA_RELEASE 10.0f       /**< Ca2+ release time constant */
#define SM_TAU_CA_REUPTAKE 100.0f     /**< Ca2+ reuptake (SERCA) time constant */
#define SM_TAU_CALMODULIN 50.0f       /**< Calmodulin activation time constant */
#define SM_TAU_CAMKII 500.0f          /**< CaMKII activation time constant */
#define SM_TAU_PKC 300.0f             /**< PKC activation time constant */
#define SM_TAU_CREB_PHOS 1000.0f      /**< CREB phosphorylation time constant */
#define SM_TAU_IEG_EXPRESSION 60000.0f /**< IEG expression time constant (minutes) */

/*=============================================================================
 * TYPE DEFINITIONS
 *============================================================================*/

/**
 * @brief G-protein coupled receptor type
 *
 * WHAT: Classification of receptors by G-protein coupling
 * WHY:  Different G-proteins activate different cascades
 * HOW:  Gs -> cAMP, Gq -> IP3/DAG, Gi -> inhibit cAMP
 */
typedef enum {
    GPCR_GS_COUPLED,        /**< Gs-coupled: activates adenylyl cyclase (D1, beta-AR) */
    GPCR_GI_COUPLED,        /**< Gi-coupled: inhibits adenylyl cyclase (D2, alpha2-AR) */
    GPCR_GQ_COUPLED,        /**< Gq-coupled: activates PLC (5-HT2A, mGluR1/5) */
    GPCR_G12_COUPLED,       /**< G12/13-coupled: activates RhoA (rare) */
    GPCR_TYPE_COUNT
} gpcr_coupling_t;

/**
 * @brief Kinase type enumeration
 *
 * WHAT: Major protein kinases involved in signaling
 * WHY:  Kinases are the effector enzymes that modify targets
 * HOW:  Each kinase phosphorylates specific substrates
 */
typedef enum {
    KINASE_PKA,             /**< Protein Kinase A (cAMP-dependent) */
    KINASE_PKC,             /**< Protein Kinase C (DAG/Ca2+-dependent) */
    KINASE_CAMKII,          /**< Ca2+/Calmodulin-dependent Kinase II */
    KINASE_MAPK,            /**< Mitogen-Activated Protein Kinase (ERK) */
    KINASE_COUNT
} kinase_type_t;

/**
 * @brief Immediate early gene type
 *
 * WHAT: Rapidly induced transcription factors
 * WHY:  IEGs are markers of neuronal activity and plasticity
 * HOW:  CREB phosphorylation -> CRE binding -> IEG transcription
 */
typedef enum {
    IEG_CFOS,               /**< c-Fos: neural activity marker */
    IEG_ARC,                /**< Arc/Arg3.1: synaptic plasticity, AMPAR trafficking */
    IEG_BDNF,               /**< BDNF: neurotrophin, survival, plasticity */
    IEG_EGR1,               /**< Egr-1/Zif268: learning, memory consolidation */
    IEG_HOMER1A,            /**< Homer1a: mGluR modulation */
    IEG_COUNT
} ieg_type_t;

/**
 * @brief cAMP pathway state
 *
 * WHAT: State of the cAMP/PKA signaling cascade
 * WHY:  Tracks concentrations and enzyme activities
 * HOW:  Updated by Gs/Gi receptor activation
 */
typedef struct {
    float adenylyl_cyclase_activity;  /**< AC activity [0, 1] */
    float camp_concentration;         /**< cAMP concentration (micromolar) */
    float pde_activity;               /**< Phosphodiesterase activity [0, 1] */
    float pka_activity;               /**< PKA activity [0, 1] */
    float pka_catalytic_free;         /**< Free catalytic subunits [0, 1] */
    uint64_t last_update_ms;          /**< Last update timestamp */
} camp_pathway_t;

/**
 * @brief IP3/DAG pathway state
 *
 * WHAT: State of the IP3/DAG signaling cascade
 * WHY:  Tracks lipid second messengers and calcium release
 * HOW:  Updated by Gq receptor activation
 */
typedef struct {
    float phospholipase_c_activity;   /**< PLC activity [0, 1] */
    float ip3_concentration;          /**< IP3 concentration (micromolar) */
    float dag_concentration;          /**< DAG concentration [0, 1] normalized */
    float ip3_receptor_open_prob;     /**< IP3R channel open probability [0, 1] */
    float pkc_activity;               /**< PKC activity [0, 1] */
    uint64_t last_update_ms;          /**< Last update timestamp */
} ip3_dag_pathway_t;

/**
 * @brief Calcium signaling state
 *
 * WHAT: Intracellular calcium dynamics
 * WHY:  Ca2+ is a universal second messenger
 * HOW:  IP3-mediated ER release, voltage-gated entry, SERCA reuptake
 */
typedef struct {
    float ca_cytoplasmic;             /**< Cytoplasmic Ca2+ (nanomolar) */
    float ca_er_store;                /**< ER Ca2+ store [0, 1] normalized */
    float calmodulin_activation;      /**< Ca2+/Calmodulin complex [0, 1] */
    float camkii_activity;            /**< CaMKII activity [0, 1] */
    float camkii_autophosphorylation; /**< CaMKII autonomous activity [0, 1] */
    float serca_activity;             /**< SERCA pump activity [0, 1] */
    uint64_t last_update_ms;          /**< Last update timestamp */
} calcium_signaling_t;

/**
 * @brief Gene expression state
 *
 * WHAT: Transcription factor and IEG expression levels
 * WHY:  Long-term plasticity requires gene expression
 * HOW:  CREB phosphorylation -> CRE-mediated transcription
 */
typedef struct {
    float creb_phosphorylation;       /**< pCREB level [0, 1] */
    float creb_activity;              /**< CREB transcriptional activity [0, 1] */
    float ieg_levels[IEG_COUNT];      /**< IEG expression levels [0, 1] */
    float protein_synthesis_rate;     /**< Overall protein synthesis [0, 1] */
    uint64_t last_expression_ms;      /**< Last gene expression update */
} gene_expression_t;

/**
 * @brief Second messenger cascade state (per neuron/synapse)
 *
 * WHAT: Complete intracellular signaling state
 * WHY:  Integrates all cascade pathways
 * HOW:  Composed of pathway substates
 */
typedef struct {
    camp_pathway_t camp;              /**< cAMP/PKA pathway */
    ip3_dag_pathway_t ip3_dag;        /**< IP3/DAG/PKC pathway */
    calcium_signaling_t calcium;      /**< Ca2+/CaM/CaMKII pathway */
    gene_expression_t gene_expr;      /**< Gene expression state */

    /* Integration state */
    float total_kinase_activity;      /**< Combined kinase output [0, 1] */
    float plasticity_modulation;      /**< Effect on plasticity threshold [0, 2] */
    float excitability_modulation;    /**< Effect on neuronal excitability [0, 2] */

    /* Metadata */
    uint32_t neuron_id;               /**< Associated neuron ID */
    uint64_t created_ms;              /**< Creation timestamp */
    uint64_t last_update_ms;          /**< Last update timestamp */
} second_messenger_state_t;

/**
 * @brief Second messenger system configuration
 */
typedef struct {
    /* Kinetics */
    float dt_ms;                      /**< Integration timestep (ms) */
    float camp_synthesis_rate;        /**< cAMP synthesis rate constant */
    float camp_degradation_rate;      /**< PDE degradation rate constant */
    float ip3_synthesis_rate;         /**< IP3 synthesis rate constant */
    float ip3_degradation_rate;       /**< IP3 degradation rate constant */
    float ca_release_rate;            /**< Ca2+ release rate constant */
    float ca_reuptake_rate;           /**< SERCA reuptake rate constant */

    /* Thresholds */
    float pka_activation_threshold;   /**< cAMP threshold for PKA [0, 1] */
    float pkc_activation_threshold;   /**< DAG/Ca2+ threshold for PKC [0, 1] */
    float camkii_activation_threshold;/**< Ca2+/CaM threshold for CaMKII [0, 1] */
    float creb_phosphorylation_threshold; /**< Kinase threshold for CREB [0, 1] */

    /* Gene expression */
    float ieg_induction_threshold;    /**< pCREB threshold for IEG induction */
    float protein_synthesis_delay_ms; /**< Delay for protein translation */

    /* Bio-async integration */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    float broadcast_threshold;        /**< Threshold for state broadcast */

    /* Security */
    bool enable_security;             /**< Register with security module */
} second_messenger_config_t;

/**
 * @brief Opaque handle to second messenger system
 */
typedef struct second_messenger_system second_messenger_system_t;

/**
 * @brief Receptor activation event
 *
 * WHAT: Input to the cascade system
 * WHY:  Receptors are the entry point for signaling
 * HOW:  Specifies receptor type and activation level
 */
typedef struct {
    receptor_type_t receptor;         /**< Which receptor was activated */
    gpcr_coupling_t coupling;         /**< G-protein coupling type */
    float occupancy;                  /**< Receptor occupancy [0, 1] */
    float duration_ms;                /**< Activation duration */
    uint32_t neuron_id;               /**< Target neuron ID */
    uint64_t timestamp_ms;            /**< Event timestamp */
} receptor_activation_event_t;

/**
 * @brief Cascade output effects
 *
 * WHAT: Effects of cascade activity on cellular function
 * WHY:  Cascades modulate plasticity, excitability, and gene expression
 * HOW:  Returned by cascade query functions
 */
typedef struct {
    /* Plasticity modulation */
    float ltp_threshold_modulation;   /**< Effect on LTP threshold [0.5, 2.0] */
    float ltd_threshold_modulation;   /**< Effect on LTD threshold [0.5, 2.0] */
    float stdp_window_modulation;     /**< Effect on STDP window width [0.5, 2.0] */
    float eligibility_trace_decay;    /**< Effect on eligibility trace decay */

    /* Excitability modulation */
    float spike_threshold_modulation; /**< Effect on spike threshold [0.8, 1.2] */
    float input_resistance_modulation;/**< Effect on Rin [0.8, 1.2] */
    float afterhyperpolarization;     /**< AHP modulation [0.5, 2.0] */

    /* Synaptic modulation */
    float release_probability_mod;    /**< Presynaptic release probability mod */
    float postsynaptic_gain_mod;      /**< Postsynaptic current gain mod */
    float nmda_modulation;            /**< NMDA receptor modulation */

    /* Gene expression markers */
    bool plasticity_tag_set;          /**< Synaptic tag for late-LTP */
    float bdnf_availability;          /**< Available BDNF for capture */
} cascade_effects_t;

/**
 * @brief Second messenger statistics
 */
typedef struct {
    uint64_t receptor_activations;    /**< Total receptor activation events */
    uint64_t cascade_updates;         /**< Total cascade update steps */
    uint64_t gene_expression_events;  /**< Gene expression events triggered */
    uint64_t plasticity_tags_set;     /**< Plasticity tags set */
    float avg_pka_activity;           /**< Average PKA activity */
    float avg_camkii_activity;        /**< Average CaMKII activity */
    float avg_creb_phosphorylation;   /**< Average pCREB level */
    uint64_t bio_async_messages_sent; /**< Bio-async messages sent */
    uint64_t bio_async_messages_recv; /**< Bio-async messages received */
} second_messenger_stats_t;

/*=============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Create second messenger system
 *
 * WHAT: Initialize second messenger cascade system
 * WHY:  Allocates resources and sets up bio-async integration
 * HOW:  Uses configuration or defaults, registers with BBB
 *
 * @param max_neurons Maximum number of neurons to track
 * @param config Configuration (NULL = defaults)
 * @return System handle or NULL on error
 */
NIMCP_EXPORT second_messenger_system_t* second_messenger_create(
    uint32_t max_neurons,
    const second_messenger_config_t* config
);

/**
 * @brief Get default configuration
 *
 * WHAT: Returns biologically realistic defaults
 * WHY:  Reasonable starting point for most use cases
 * HOW:  Based on published kinetics literature
 *
 * @return Default configuration
 */
NIMCP_EXPORT second_messenger_config_t second_messenger_default_config(void);

/**
 * @brief Destroy second messenger system
 *
 * WHAT: Clean up resources
 * WHY:  Prevent memory leaks
 * HOW:  Unregisters bio-async, frees memory
 *
 * @param system System handle
 */
NIMCP_EXPORT void second_messenger_destroy(second_messenger_system_t* system);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

/**
 * @brief Register with bio-async router
 *
 * WHAT: Enable asynchronous cascade signaling
 * WHY:  Cascades communicate with other brain modules
 * HOW:  Register handlers for relevant message types
 *
 * @param system System handle
 * @param router Bio-async router
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_register_bioasync(
    second_messenger_system_t* system,
    bio_router_t router
);

/**
 * @brief Process bio-async inbox
 *
 * WHAT: Handle pending messages
 * WHY:  React to neuromodulator releases, plasticity events
 * HOW:  Dequeue and process messages from inbox
 *
 * @param system System handle
 * @return Number of messages processed
 */
NIMCP_EXPORT uint32_t second_messenger_process_inbox(second_messenger_system_t* system);

/**
 * @brief Broadcast cascade state
 *
 * WHAT: Send current state to subscribers
 * WHY:  Notify plasticity and other modules of cascade state
 * HOW:  BIO_MSG_SECOND_MESSENGER_UPDATE message
 *
 * @param system System handle
 * @param neuron_id Neuron whose state to broadcast (0 = all)
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_broadcast_state(
    second_messenger_system_t* system,
    uint32_t neuron_id
);

/*=============================================================================
 * RECEPTOR ACTIVATION API
 *============================================================================*/

/**
 * @brief Activate receptor and trigger cascade
 *
 * WHAT: Start a signaling cascade from receptor activation
 * WHY:  Entry point for neuromodulator effects
 * HOW:  Routes to appropriate pathway based on receptor coupling
 *
 * @param system System handle
 * @param event Receptor activation event
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_activate_receptor(
    second_messenger_system_t* system,
    const receptor_activation_event_t* event
);

/**
 * @brief Activate Gs-coupled receptor (cAMP pathway)
 *
 * WHAT: Stimulate adenylyl cyclase
 * WHY:  D1, beta-adrenergic receptors use this pathway
 * HOW:  Increases cAMP -> PKA activation
 *
 * @param system System handle
 * @param neuron_id Target neuron
 * @param occupancy Receptor occupancy [0, 1]
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_activate_gs(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float occupancy,
    uint64_t timestamp_ms
);

/**
 * @brief Activate Gi-coupled receptor (inhibit cAMP)
 *
 * WHAT: Inhibit adenylyl cyclase
 * WHY:  D2, alpha2-adrenergic receptors use this pathway
 * HOW:  Decreases cAMP -> PKA inhibition
 *
 * @param system System handle
 * @param neuron_id Target neuron
 * @param occupancy Receptor occupancy [0, 1]
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_activate_gi(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float occupancy,
    uint64_t timestamp_ms
);

/**
 * @brief Activate Gq-coupled receptor (IP3/DAG pathway)
 *
 * WHAT: Stimulate phospholipase C
 * WHY:  5-HT2A, mGluR1/5 receptors use this pathway
 * HOW:  Increases IP3/DAG -> Ca2+ release + PKC activation
 *
 * @param system System handle
 * @param neuron_id Target neuron
 * @param occupancy Receptor occupancy [0, 1]
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_activate_gq(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float occupancy,
    uint64_t timestamp_ms
);

/*=============================================================================
 * CASCADE DYNAMICS API
 *============================================================================*/

/**
 * @brief Update cascade dynamics for one timestep
 *
 * WHAT: Integrate cascade equations for all tracked neurons
 * WHY:  Cascades evolve over time
 * HOW:  Euler integration with time constants
 *
 * @param system System handle
 * @param dt_ms Time step (ms)
 * @param timestamp_ms Current simulation time
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_update(
    second_messenger_system_t* system,
    float dt_ms,
    uint64_t timestamp_ms
);

/**
 * @brief Update cascade for single neuron
 *
 * WHAT: Update one neuron's cascade state
 * WHY:  Allows fine-grained control
 * HOW:  Euler integration of pathway equations
 *
 * @param system System handle
 * @param neuron_id Neuron to update
 * @param dt_ms Time step (ms)
 * @param timestamp_ms Current simulation time
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_update_neuron(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float dt_ms,
    uint64_t timestamp_ms
);

/**
 * @brief Inject external calcium
 *
 * WHAT: Add calcium from external source
 * WHY:  Voltage-gated channels, NMDA receptors add calcium
 * HOW:  Direct addition to cytoplasmic pool
 *
 * @param system System handle
 * @param neuron_id Target neuron
 * @param ca_nm Calcium amount (nanomolar)
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_inject_calcium(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float ca_nm,
    uint64_t timestamp_ms
);

/*=============================================================================
 * STATE QUERY API
 *============================================================================*/

/**
 * @brief Get cascade state for neuron
 *
 * WHAT: Query current cascade state
 * WHY:  Other modules need cascade information
 * HOW:  Returns copy of internal state
 *
 * @param system System handle
 * @param neuron_id Neuron to query
 * @param state Output state buffer
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_get_state(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    second_messenger_state_t* state
);

/**
 * @brief Get cascade effects on plasticity
 *
 * WHAT: Compute how cascades affect plasticity
 * WHY:  Cascades modulate learning rules
 * HOW:  Integrate kinase activities into plasticity modulation
 *
 * @param system System handle
 * @param neuron_id Neuron to query
 * @param effects Output effects buffer
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_get_effects(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    cascade_effects_t* effects
);

/**
 * @brief Get kinase activity
 *
 * WHAT: Query specific kinase activity
 * WHY:  Direct access to kinase state
 * HOW:  Lookup in cascade state
 *
 * @param system System handle
 * @param neuron_id Neuron to query
 * @param kinase Which kinase
 * @return Kinase activity [0, 1] or -1.0 on error
 */
NIMCP_EXPORT float second_messenger_get_kinase_activity(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    kinase_type_t kinase
);

/**
 * @brief Get IEG expression level
 *
 * WHAT: Query immediate early gene expression
 * WHY:  IEGs are plasticity markers
 * HOW:  Lookup in gene expression state
 *
 * @param system System handle
 * @param neuron_id Neuron to query
 * @param ieg Which IEG
 * @return Expression level [0, 1] or -1.0 on error
 */
NIMCP_EXPORT float second_messenger_get_ieg_level(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    ieg_type_t ieg
);

/**
 * @brief Check if plasticity tag is set
 *
 * WHAT: Query synaptic tagging state
 * WHY:  Synaptic tagging is required for late-LTP
 * HOW:  Check gene expression threshold
 *
 * @param system System handle
 * @param neuron_id Neuron to query
 * @return true if plasticity tag is set
 */
NIMCP_EXPORT bool second_messenger_is_tagged(
    const second_messenger_system_t* system,
    uint32_t neuron_id
);

/*=============================================================================
 * INTEGRATION API
 *============================================================================*/

/**
 * @brief Integrate with neuromodulator system
 *
 * WHAT: Connect cascades to neuromodulator release
 * WHY:  Neuromodulators activate receptors -> cascades
 * HOW:  Subscribe to neuromodulator release messages
 *
 * @param system Second messenger system
 * @param neuromod Neuromodulator system
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_integrate_neuromodulator(
    second_messenger_system_t* system,
    neuromodulator_system_t neuromod
);

/**
 * @brief Get plasticity modulation factor
 *
 * WHAT: Compute how cascades affect learning rate
 * WHY:  PKA, PKC, CaMKII modulate plasticity
 * HOW:  Weighted sum of kinase activities
 *
 * @param system System handle
 * @param neuron_id Neuron to query
 * @return Modulation factor [0.5, 2.0] where 1.0 = baseline
 */
NIMCP_EXPORT float second_messenger_get_plasticity_modulation(
    const second_messenger_system_t* system,
    uint32_t neuron_id
);

/**
 * @brief Get excitability modulation factor
 *
 * WHAT: Compute how cascades affect spike threshold
 * WHY:  Cascades can increase or decrease excitability
 * HOW:  Based on PKA (increases) and PKC (varies) activity
 *
 * @param system System handle
 * @param neuron_id Neuron to query
 * @return Modulation factor [0.8, 1.2] where 1.0 = baseline
 */
NIMCP_EXPORT float second_messenger_get_excitability_modulation(
    const second_messenger_system_t* system,
    uint32_t neuron_id
);

/*=============================================================================
 * RESET AND STATISTICS
 *============================================================================*/

/**
 * @brief Reset cascade state for neuron
 *
 * WHAT: Return cascade to baseline
 * WHY:  Initialization or error recovery
 * HOW:  Set all concentrations to baseline
 *
 * @param system System handle
 * @param neuron_id Neuron to reset (0 = all)
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_reset(
    second_messenger_system_t* system,
    uint32_t neuron_id
);

/**
 * @brief Get system statistics
 *
 * WHAT: Query performance and activity metrics
 * WHY:  Monitoring and debugging
 * HOW:  Return accumulated statistics
 *
 * @param system System handle
 * @param stats Output statistics buffer
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_get_stats(
    const second_messenger_system_t* system,
    second_messenger_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero all counters
 *
 * @param system System handle
 * @return NIMCP_SUCCESS or error
 */
NIMCP_EXPORT nimcp_result_t second_messenger_reset_stats(second_messenger_system_t* system);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get receptor coupling type
 *
 * WHAT: Determine G-protein coupling for receptor
 * WHY:  Maps receptor to cascade pathway
 * HOW:  Lookup table
 *
 * @param receptor Receptor type
 * @return G-protein coupling type
 */
NIMCP_EXPORT gpcr_coupling_t second_messenger_receptor_coupling(receptor_type_t receptor);

/**
 * @brief Get kinase name
 *
 * WHAT: Human-readable kinase name
 * WHY:  Logging and debugging
 * HOW:  Static string lookup
 *
 * @param kinase Kinase type
 * @return Kinase name string
 */
NIMCP_EXPORT const char* second_messenger_kinase_name(kinase_type_t kinase);

/**
 * @brief Get IEG name
 *
 * WHAT: Human-readable IEG name
 * WHY:  Logging and debugging
 * HOW:  Static string lookup
 *
 * @param ieg IEG type
 * @return IEG name string
 */
NIMCP_EXPORT const char* second_messenger_ieg_name(ieg_type_t ieg);

/**
 * @brief Validate configuration
 *
 * WHAT: Check configuration validity
 * WHY:  Catch errors early
 * HOW:  Range and consistency checks
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_result_t second_messenger_validate_config(
    const second_messenger_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECOND_MESSENGERS_H */
