/**
 * @file nimcp_neuromod_substrate_bridge.h
 * @brief Neural Substrate-Neuromodulator Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between neural substrate and neuromodulator system
 * WHY:  Neuromodulation requires energy (ATP), vesicle release requires calcium,
 *       synthesis requires metabolic capacity, and temperature affects all kinetics
 * HOW:  Substrate modulates synthesis/release/reuptake; neuromodulation can feed back
 *       to influence metabolic demand
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SUBSTRATE → NEUROMODULATION PATHWAYS:
 * -------------------------------------
 * 1. ATP-Dependent Synthesis:
 *    - Tyrosine hydroxylase (TH) requires ATP for dopamine synthesis
 *    - Tryptophan hydroxylase (TPH) requires ATP for serotonin synthesis
 *    - Vesicular transporters (VMAT2, VAChT) use ATP-driven H+ gradient
 *    - Low ATP → reduced synthesis, impaired vesicle loading
 *    - Reference: Cooper et al. (2003) "Biochemical Basis of Neuropharmacology"
 *
 * 2. Calcium-Dependent Release:
 *    - Vesicle fusion requires Ca2+ influx during action potential
 *    - Ca2+ homeostasis dysfunction → impaired release probability
 *    - Low substrate ca_homeostasis → reduced vesicle release
 *    - Reference: Südhof (2012) "Calcium control of neurotransmitter release"
 *
 * 3. Temperature Effects (Q10):
 *    - Enzyme kinetics scale with temperature (Q10 ≈ 2-3)
 *    - Synthesis: TH, TPH, ChAT activity increases with temperature
 *    - Degradation: MAO, COMT, AChE activity increases with temperature
 *    - Reuptake: DAT, SERT, NET kinetics accelerate with temperature
 *    - Binding: Receptor affinity changes with temperature
 *    - Reference: Hodgkin & Huxley (1952) "Temperature coefficients"
 *
 * 4. Na+/K+ Gradient for Reuptake:
 *    - DAT, SERT, NET are Na+/Cl- cotransporters
 *    - Require Na+ gradient maintained by Na+/K+-ATPase
 *    - Low substrate ion_balance → reduced reuptake efficiency
 *    - Reference: Torres et al. (2003) "Plasma membrane monoamine transporters"
 *
 * 5. Metabolic Burden of Neuromodulation:
 *    - Sustained release depletes ATP (vesicle recycling, pumps)
 *    - High synthesis demand consumes glucose/oxygen
 *    - Feedback: Excessive neuromodulation can stress substrate
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           NEUROMODULATOR-SUBSTRATE INTEGRATION BRIDGE                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            SUBSTRATE → NEUROMODULATION EFFECTS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │ ATP Level    │   │ Ca2+ Balance │   │ Temperature  │          │  ║
 * ║   │   │ ──────────── │   │ ──────────── │   │ ──────────── │          │  ║
 * ║   │   │ Synthesis ↓  │   │ Release ↓    │   │ Q10 → All    │          │  ║
 * ║   │   │ Vesicle ↓    │   │ Pr ↓         │   │ Kinetics ↑   │          │  ║
 * ║   │   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘          │  ║
 * ║   │          └──────────────────┴──────────────────┘                   │  ║
 * ║   │                             ↓                                      │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │         NEUROMODULATOR CAPACITY MODULATION                  │ │  ║
 * ║   │   │  • Synthesis rate multiplier [0-1.5]                        │ │  ║
 * ║   │   │  • Release probability multiplier [0-1.5]                   │ │  ║
 * ║   │   │  • Reuptake efficiency multiplier [0-1.5]                   │ │  ║
 * ║   │   │  • Receptor binding affinity shift                          │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                ION GRADIENT EFFECTS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐                             │  ║
 * ║   │   │ Na+/K+ Pump  │   │ Ion Balance  │                             │  ║
 * ║   │   │ Activity     │   │ [0-1]        │                             │  ║
 * ║   │   └──────┬───────┘   └──────┬───────┘                             │  ║
 * ║   │          └──────────────────┘                                      │  ║
 * ║   │                   ↓                                                │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │           REUPTAKE TRANSPORTER EFFICIENCY                   │ │  ║
 * ║   │   │  DAT/SERT/NET efficiency = ion_balance^2                    │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Strategy: Per-neuromodulator modulation strategies
 * - Observer: Substrate state changes notify bridge
 * - Adapter: Bridge adapts substrate metrics to neuromodulator parameters
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_SUBSTRATE_BRIDGE_H
#define NIMCP_NEUROMOD_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_receptor_subtypes.h"
#include "plasticity/neuromodulators/nimcp_vesicle_packaging.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"
#include "plasticity/neuromodulators/nimcp_metabolic_pathways.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Thread utilities */
#include "utils/thread/nimcp_thread.h"

/* Common utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* ATP effects on synthesis (tyrosine hydroxylase is ATP-dependent) */
#define ATP_SYNTHESIS_THRESHOLD         0.5f    /**< Below this, synthesis reduced */
#define ATP_SYNTHESIS_CRITICAL          0.3f    /**< Critical ATP for minimal synthesis */
#define ATP_SYNTHESIS_MAX_BOOST         1.5f    /**< Max synthesis boost at optimal ATP */

/* Calcium effects on vesicle release */
#define CA_RELEASE_THRESHOLD            0.6f    /**< Below this, release reduced */
#define CA_RELEASE_CRITICAL             0.4f    /**< Critical calcium for minimal release */
#define CA_RELEASE_MAX_BOOST            1.5f    /**< Max release boost at optimal calcium */

/* Temperature Q10 coefficients */
#define Q10_SYNTHESIS                   2.5f    /**< Q10 for enzyme synthesis */
#define Q10_DEGRADATION                 2.3f    /**< Q10 for MAO/COMT/AChE */
#define Q10_REUPTAKE                    2.0f    /**< Q10 for DAT/SERT/NET */
#define Q10_RECEPTOR_BINDING            1.8f    /**< Q10 for receptor affinity */
#define REFERENCE_TEMPERATURE           37.0f   /**< Reference temp (°C) */

/* Ion gradient effects on reuptake */
#define ION_REUPTAKE_THRESHOLD          0.6f    /**< Below this, reuptake impaired */
#define ION_REUPTAKE_CRITICAL           0.4f    /**< Critical ion balance */

/* Metabolic cost of neuromodulation */
#define COST_PER_SYNTHESIS              0.0001f /**< ATP cost per synthesis event */
#define COST_PER_RELEASE                0.0002f /**< ATP cost per vesicle release */
#define COST_PER_REUPTAKE               0.0001f /**< ATP cost per reuptake event */

/* Bio-async module ID */
#define BIO_MODULE_NEUROMOD_SUBSTRATE   0x0E00  /**< Bio-async module ID */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Neuromodulator types (matching neuromodulator system)
 */
typedef enum {
    NEUROMOD_BRIDGE_DOPAMINE = 0,
    NEUROMOD_BRIDGE_SEROTONIN,
    NEUROMOD_BRIDGE_ACETYLCHOLINE,
    NEUROMOD_BRIDGE_NOREPINEPHRINE,
    NEUROMOD_BRIDGE_COUNT
} neuromod_bridge_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Substrate effects on a single neuromodulator
 */
typedef struct {
    /* ATP-dependent synthesis modulation */
    float atp_synthesis_factor;         /**< Synthesis rate multiplier [0-1.5] */

    /* Calcium-dependent release modulation */
    float calcium_release_factor;       /**< Release probability multiplier [0-1.5] */

    /* Temperature-dependent kinetics (Q10) */
    float temp_synthesis_factor;        /**< Q10-scaled synthesis [0.5-2.0] */
    float temp_degradation_factor;      /**< Q10-scaled degradation [0.5-2.0] */
    float temp_reuptake_factor;         /**< Q10-scaled reuptake [0.5-2.0] */
    float temp_receptor_factor;         /**< Q10-scaled receptor binding [0.8-1.2] */

    /* Ion gradient-dependent reuptake */
    float ion_reuptake_factor;          /**< Reuptake transporter efficiency [0-1] */

    /* Composite modulation factors */
    float overall_synthesis_mod;        /**< Combined synthesis modulation */
    float overall_release_mod;          /**< Combined release modulation */
    float overall_reuptake_mod;         /**< Combined reuptake modulation */
    float overall_capacity;             /**< Overall neuromodulator capacity [0-1] */
} substrate_neuromod_effects_t;

/**
 * @brief Per-neuromodulator substrate effects
 */
typedef struct {
    substrate_neuromod_effects_t dopamine;
    substrate_neuromod_effects_t serotonin;
    substrate_neuromod_effects_t acetylcholine;
    substrate_neuromod_effects_t norepinephrine;
} neuromod_substrate_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;

    /* ATP depletion events */
    uint32_t atp_depletion_events;
    uint32_t synthesis_limited_cycles;

    /* Calcium issues */
    uint32_t calcium_depletion_events;
    uint32_t release_limited_cycles;

    /* Temperature extremes */
    uint32_t hyperthermia_cycles;
    uint32_t hypothermia_cycles;

    /* Ion imbalance */
    uint32_t ion_imbalance_cycles;
    uint32_t reuptake_limited_cycles;

    /* Metrics */
    float avg_synthesis_capacity;
    float avg_release_capacity;
    float avg_reuptake_capacity;
    float min_overall_capacity;
} neuromod_substrate_stats_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_atp_synthesis_modulation;
    bool enable_calcium_release_modulation;
    bool enable_temperature_modulation;
    bool enable_ion_reuptake_modulation;
    bool enable_metabolic_feedback;
    bool enable_bio_async;

    /* Sensitivity multipliers */
    float atp_sensitivity;
    float calcium_sensitivity;
    float temperature_sensitivity;
    float ion_sensitivity;

    /* Q10 coefficients (can be customized) */
    float q10_synthesis;
    float q10_degradation;
    float q10_reuptake;
    float q10_receptor;
} neuromod_substrate_config_t;

/**
 * @brief Complete neuromodulator-substrate bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    neural_substrate_t* substrate;
    neuromodulator_system_t neuromod_system;

    /* Current effects (per neuromodulator) */
    neuromod_substrate_effects_t effects;

    /* Configuration */
    neuromod_substrate_config_t config;

    /* Statistics */
    neuromod_substrate_stats_t stats;

    } neuromod_substrate_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Set parameters based on physiological values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int neuromod_substrate_default_config(neuromod_substrate_config_t* config);

/**
 * @brief Create neuromodulator-substrate bridge
 *
 * WHAT: Initialize bridge between substrate and neuromodulator system
 * WHY:  Enable substrate to modulate neuromodulation
 * HOW:  Allocate structure, connect systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @param substrate Neural substrate
 * @param neuromod_system Neuromodulator system
 * @return New bridge or NULL on failure
 */
neuromod_substrate_bridge_t* neuromod_substrate_bridge_create(
    const neuromod_substrate_config_t* config,
    neural_substrate_t* substrate,
    neuromodulator_system_t neuromod_system
);

/**
 * @brief Destroy neuromodulator-substrate bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure, disconnect bio-async
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void neuromod_substrate_bridge_destroy(neuromod_substrate_bridge_t* bridge);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Neuromodulator substrate bridge
 * @return 0 on success, -1 on error
 */
int neuromod_substrate_connect_bio_async(neuromod_substrate_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Neuromodulator substrate bridge
 * @return 0 on success, -1 on error
 */
int neuromod_substrate_disconnect_bio_async(neuromod_substrate_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Neuromodulator substrate bridge
 * @return true if connected
 */
bool neuromod_substrate_is_bio_async_connected(const neuromod_substrate_bridge_t* bridge);

/* ============================================================================
 * Substrate → Neuromodulation Effects API
 * ============================================================================ */

/**
 * @brief Compute ATP effects on synthesis
 *
 * WHAT: Calculate synthesis rate modulation based on ATP availability
 * WHY:  Tyrosine hydroxylase and other enzymes require ATP
 * HOW:  Sigmoid function: high ATP → boost, low ATP → impairment
 *
 * BIOLOGICAL: TH is rate-limiting and ATP-dependent for DA synthesis
 *
 * @param bridge Neuromodulator substrate bridge
 * @return 0 on success
 */
int neuromod_substrate_compute_atp_effects(neuromod_substrate_bridge_t* bridge);

/**
 * @brief Compute calcium effects on release
 *
 * WHAT: Calculate release probability modulation based on calcium homeostasis
 * WHY:  Vesicle fusion requires Ca2+ influx
 * HOW:  Linear scaling: high Ca2+ → normal release, low Ca2+ → reduced release
 *
 * BIOLOGICAL: Ca2+-triggered exocytosis (Südhof 2012)
 *
 * @param bridge Neuromodulator substrate bridge
 * @return 0 on success
 */
int neuromod_substrate_compute_calcium_effects(neuromod_substrate_bridge_t* bridge);

/**
 * @brief Compute temperature effects (Q10)
 *
 * WHAT: Calculate kinetic rate modulation based on temperature
 * WHY:  All biochemical processes have temperature dependence
 * HOW:  Q10 equation: rate(T) = rate(T_ref) × Q10^((T - T_ref)/10)
 *
 * BIOLOGICAL: Hodgkin & Huxley temperature coefficients
 *
 * @param bridge Neuromodulator substrate bridge
 * @return 0 on success
 */
int neuromod_substrate_compute_temperature_effects(neuromod_substrate_bridge_t* bridge);

/**
 * @brief Compute ion gradient effects on reuptake
 *
 * WHAT: Calculate reuptake efficiency based on Na+/K+ balance
 * WHY:  Transporters (DAT, SERT, NET) require Na+ gradient
 * HOW:  Quadratic scaling: efficiency ∝ ion_balance^2
 *
 * BIOLOGICAL: Na+/Cl- cotransport mechanism (Torres et al. 2003)
 *
 * @param bridge Neuromodulator substrate bridge
 * @return 0 on success
 */
int neuromod_substrate_compute_ion_effects(neuromod_substrate_bridge_t* bridge);

/**
 * @brief Update all substrate effects on neuromodulation
 *
 * WHAT: Compute all modulation factors (ATP, Ca2+, temp, ions)
 * WHY:  Complete substrate → neuromodulation integration
 * HOW:  Call all compute functions, update composite factors
 *
 * @param bridge Neuromodulator substrate bridge
 * @return 0 on success
 */
int neuromod_substrate_update_effects(neuromod_substrate_bridge_t* bridge);

/* ============================================================================
 * Neuromodulation → Substrate Feedback API
 * ============================================================================ */

/**
 * @brief Record synthesis event (consumes ATP)
 *
 * WHAT: Track metabolic cost of neurotransmitter synthesis
 * WHY:  Synthesis depletes ATP reserves
 * HOW:  Decrement substrate ATP by COST_PER_SYNTHESIS
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator being synthesized
 * @return 0 on success
 */
int neuromod_substrate_record_synthesis(
    neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
);

/**
 * @brief Record vesicle release event (consumes ATP)
 *
 * WHAT: Track metabolic cost of vesicle fusion and recycling
 * WHY:  Release requires ATP for Ca2+ pumps and vesicle recycling
 * HOW:  Decrement substrate ATP by COST_PER_RELEASE
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator being released
 * @param vesicle_count Number of vesicles released
 * @return 0 on success
 */
int neuromod_substrate_record_release(
    neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type,
    uint32_t vesicle_count
);

/**
 * @brief Record reuptake event (consumes ATP)
 *
 * WHAT: Track metabolic cost of transporter-mediated reuptake
 * WHY:  Transporters use Na+ gradient maintained by Na+/K+-ATPase
 * HOW:  Decrement substrate ATP by COST_PER_REUPTAKE
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator being reuptaken
 * @return 0 on success
 */
int neuromod_substrate_record_reuptake(
    neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get synthesis rate modulation for a neuromodulator
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator type
 * @return Synthesis rate multiplier [0-1.5]
 */
float neuromod_substrate_get_synthesis_mod(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
);

/**
 * @brief Get release probability modulation for a neuromodulator
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator type
 * @return Release probability multiplier [0-1.5]
 */
float neuromod_substrate_get_release_mod(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
);

/**
 * @brief Get reuptake efficiency modulation for a neuromodulator
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator type
 * @return Reuptake efficiency multiplier [0-1.5]
 */
float neuromod_substrate_get_reuptake_mod(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
);

/**
 * @brief Get overall neuromodulator capacity
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator type
 * @return Overall capacity [0-1]
 */
float neuromod_substrate_get_capacity(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
);

/**
 * @brief Get current substrate effects for a neuromodulator
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator type
 * @param effects Output effects structure
 * @return 0 on success
 */
int neuromod_substrate_get_effects(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type,
    substrate_neuromod_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Neuromodulator substrate bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int neuromod_substrate_get_stats(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_substrate_stats_t* stats
);

/**
 * @brief Check if neuromodulation is substrate-limited
 *
 * @param bridge Neuromodulator substrate bridge
 * @param neuromod_type Neuromodulator type
 * @return true if limited by substrate constraints
 */
bool neuromod_substrate_is_limited(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert neuromodulator type to string
 *
 * @param neuromod_type Neuromodulator type
 * @return Human-readable string
 */
const char* neuromod_bridge_type_to_string(neuromod_bridge_type_t neuromod_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_SUBSTRATE_BRIDGE_H */
