/**
 * @file nimcp_neural_substrate.h
 * @brief Neural Substrate Module - Computational Infrastructure Layer
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Low-level computational substrate modeling for neural networks
 * WHY:  Real brains have physical constraints (metabolic energy, temperature,
 *       ion concentrations, ATP availability). This module models these
 *       substrate-level properties that affect neural computation.
 * HOW:  Track substrate health metrics, metabolic state, and provide modulation
 *       factors for neural activity based on substrate conditions.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * METABOLIC SUBSTRATE:
 * --------------------
 * 1. ATP/Energy Budget:
 *    - Neurons consume ~20% of body's oxygen/glucose despite being ~2% of mass
 *    - Action potentials cost ~10^8 ATP molecules
 *    - Synaptic transmission requires continuous ATP
 *    - Energy deficit → reduced firing, impaired transmission
 *    - Reference: Attwell & Laughlin (2001) "An Energy Budget for Signaling"
 *
 * 2. Ion Concentrations:
 *    - Na+/K+ gradients maintained by Na+/K+-ATPase pumps
 *    - Ca2+ concentrations critical for neurotransmitter release
 *    - Ionic imbalance → altered excitability
 *    - Reference: Bhardwaj et al. (2016) "Ion homeostasis in brain function"
 *
 * 3. Oxygen/Glucose Supply:
 *    - Neurons cannot store glycogen (unlike astrocytes)
 *    - Continuous blood supply essential
 *    - Hypoxia/hypoglycemia → rapid dysfunction
 *    - Reference: Harris et al. (2012) "The neurovascular unit"
 *
 * PHYSICAL SUBSTRATE:
 * -------------------
 * 1. Temperature Effects:
 *    - Q10 = 2-3 for most neural processes
 *    - Hyperthermia → excitotoxicity, protein denaturation
 *    - Hypothermia → slowed conduction, reduced release
 *    - Reference: Hodgkin & Huxley (1952) temperature coefficients
 *
 * 2. Membrane Properties:
 *    - Lipid composition affects fluidity
 *    - Channel density determines conductance
 *    - Degraded membranes → leaky conductance
 *    - Reference: Bhargava et al. (2013) "Lipid-protein interactions"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    NEURAL SUBSTRATE MODULE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  METABOLIC SUBSTRATE                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐            │  ║
 * ║   │   │ ATP Level    │  │ O2 Saturation│  │ Glucose      │            │  ║
 * ║   │   │ [0.0 - 1.0]  │  │ [0.0 - 1.0]  │  │ [0.0 - 1.0]  │            │  ║
 * ║   │   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘            │  ║
 * ║   │          │                 │                 │                     │  ║
 * ║   │          └─────────────────┴─────────────────┘                     │  ║
 * ║   │                            ↓                                       │  ║
 * ║   │            ┌────────────────────────────┐                         │  ║
 * ║   │            │     ENERGY BUDGET          │                         │  ║
 * ║   │            │  (metabolic_capacity)      │                         │  ║
 * ║   │            └────────────────────────────┘                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  PHYSICAL SUBSTRATE                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐            │  ║
 * ║   │   │ Temperature  │  │ Membrane     │  │ Ion Balance  │            │  ║
 * ║   │   │ [°C]         │  │ Integrity    │  │ [0.0 - 1.0]  │            │  ║
 * ║   │   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘            │  ║
 * ║   │          │                 │                 │                     │  ║
 * ║   │          └─────────────────┴─────────────────┘                     │  ║
 * ║   │                            ↓                                       │  ║
 * ║   │            ┌────────────────────────────┐                         │  ║
 * ║   │            │     SUBSTRATE HEALTH       │                         │  ║
 * ║   │            │  (physical_capacity)       │                         │  ║
 * ║   │            └────────────────────────────┘                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              MODULATION OUTPUTS                                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   • firing_rate_modulation: How much firing rate is scaled         │  ║
 * ║   │   • transmission_efficiency: Synaptic transmission strength        │  ║
 * ║   │   • conduction_velocity: Axonal signal speed                       │  ║
 * ║   │   • plasticity_capacity: Learning rate modulation                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_SUBSTRATE_H
#define NIMCP_NEURAL_SUBSTRATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Normal operating values */
#define SUBSTRATE_NORMAL_TEMPERATURE       37.0f    /**< Normal brain temp (°C) */
#define SUBSTRATE_NORMAL_ATP               0.95f    /**< Normal ATP level [0-1] */
#define SUBSTRATE_NORMAL_O2_SAT            0.97f    /**< Normal O2 saturation [0-1] */
#define SUBSTRATE_NORMAL_GLUCOSE           0.90f    /**< Normal glucose [0-1] */
#define SUBSTRATE_NORMAL_ION_BALANCE       0.95f    /**< Normal ion balance [0-1] */
#define SUBSTRATE_NORMAL_MEMBRANE          0.98f    /**< Normal membrane integrity [0-1] */

/* Critical thresholds */
#define SUBSTRATE_CRITICAL_ATP             0.3f     /**< Critical ATP level */
#define SUBSTRATE_CRITICAL_O2              0.5f     /**< Critical O2 level */
#define SUBSTRATE_CRITICAL_GLUCOSE         0.4f     /**< Critical glucose level */
#define SUBSTRATE_HYPERTHERMIA_THRESHOLD   40.0f    /**< Hyperthermia (°C) */
#define SUBSTRATE_HYPOTHERMIA_THRESHOLD    32.0f    /**< Hypothermia (°C) */
#define SUBSTRATE_CRITICAL_ION_IMBALANCE   0.5f     /**< Critical ion imbalance */
#define SUBSTRATE_CRITICAL_MEMBRANE        0.6f     /**< Critical membrane damage */

/* Q10 temperature coefficients */
#define SUBSTRATE_Q10_FIRING               2.5f     /**< Q10 for firing rate */
#define SUBSTRATE_Q10_TRANSMISSION         2.0f     /**< Q10 for transmission */
#define SUBSTRATE_Q10_CONDUCTION           1.8f     /**< Q10 for conduction */
#define SUBSTRATE_Q10_PLASTICITY           2.2f     /**< Q10 for plasticity */

/* Energy costs (normalized units) */
#define SUBSTRATE_COST_PER_SPIKE           0.001f   /**< ATP cost per spike */
#define SUBSTRATE_COST_PER_TRANSMISSION    0.0005f  /**< ATP cost per synaptic event */
#define SUBSTRATE_COST_BASELINE            0.0001f  /**< Baseline metabolic cost */

/* Recovery rates */
#define SUBSTRATE_ATP_RECOVERY_RATE        0.01f    /**< ATP recovery per ms */
#define SUBSTRATE_ION_RECOVERY_RATE        0.005f   /**< Ion balance recovery per ms */
#define SUBSTRATE_MEMBRANE_REPAIR_RATE     0.001f   /**< Membrane repair per ms */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Substrate health levels
 */
typedef enum {
    SUBSTRATE_HEALTH_OPTIMAL = 0,    /**< All parameters normal */
    SUBSTRATE_HEALTH_STRESSED,       /**< Mild degradation */
    SUBSTRATE_HEALTH_COMPROMISED,    /**< Moderate impairment */
    SUBSTRATE_HEALTH_CRITICAL,       /**< Severe dysfunction */
    SUBSTRATE_HEALTH_FAILING         /**< Near-failure state */
} substrate_health_level_t;

/**
 * @brief Substrate alert types
 */
typedef enum {
    SUBSTRATE_ALERT_NONE = 0,
    SUBSTRATE_ALERT_LOW_ATP,         /**< ATP depletion */
    SUBSTRATE_ALERT_HYPOXIA,         /**< Low oxygen */
    SUBSTRATE_ALERT_HYPOGLYCEMIA,    /**< Low glucose */
    SUBSTRATE_ALERT_HYPERTHERMIA,    /**< High temperature */
    SUBSTRATE_ALERT_HYPOTHERMIA,     /**< Low temperature */
    SUBSTRATE_ALERT_ION_IMBALANCE,   /**< Ionic dysregulation */
    SUBSTRATE_ALERT_MEMBRANE_DAMAGE  /**< Membrane degradation */
} substrate_alert_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Metabolic state
 */
typedef struct {
    float atp_level;           /**< Current ATP [0-1] */
    float oxygen_saturation;   /**< O2 saturation [0-1] */
    float glucose_level;       /**< Glucose availability [0-1] */

    float metabolic_rate;      /**< Current consumption rate */
    float recovery_rate;       /**< Current recovery rate */

    float metabolic_capacity;  /**< Combined metabolic score [0-1] */
} substrate_metabolic_state_t;

/**
 * @brief Physical state
 */
typedef struct {
    float temperature;         /**< Current temperature (°C) */
    float membrane_integrity;  /**< Membrane health [0-1] */
    float ion_balance;         /**< Ion gradient health [0-1] */

    float na_k_pump_activity;  /**< Na+/K+-ATPase activity [0-1] */
    float ca_homeostasis;      /**< Ca2+ regulation [0-1] */

    float physical_capacity;   /**< Combined physical score [0-1] */
} substrate_physical_state_t;

/**
 * @brief Modulation factors computed from substrate state
 */
typedef struct {
    float firing_rate_mod;          /**< Firing rate multiplier [0-1.5] */
    float transmission_efficiency;  /**< Synaptic strength [0-1] */
    float conduction_velocity;      /**< Axonal speed multiplier [0.5-1.5] */
    float plasticity_capacity;      /**< Learning rate multiplier [0-1] */
    float overall_capacity;         /**< Combined modulation [0-1] */
} substrate_modulation_t;

/**
 * @brief Substrate statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t spikes_processed;
    uint64_t transmissions_processed;
    float total_atp_consumed;
    float peak_metabolic_rate;
    uint32_t alerts_generated;
    uint32_t critical_events;
    float avg_health_score;
} substrate_stats_t;

/**
 * @brief Substrate configuration
 */
typedef struct {
    /* Initial values */
    float initial_atp;
    float initial_o2;
    float initial_glucose;
    float initial_temperature;
    float initial_membrane;
    float initial_ion_balance;

    /* Recovery parameters */
    float atp_recovery_rate;
    float ion_recovery_rate;
    float membrane_repair_rate;

    /* Energy costs */
    float cost_per_spike;
    float cost_per_transmission;
    float baseline_cost;

    /* Feature enables */
    bool enable_metabolic_model;
    bool enable_temperature_effects;
    bool enable_ion_dynamics;
    bool enable_alerts;
} substrate_config_t;

/**
 * @brief Complete neural substrate state
 */
typedef struct neural_substrate {
    /* State components */
    substrate_metabolic_state_t metabolic;
    substrate_physical_state_t physical;
    substrate_modulation_t modulation;
    substrate_health_level_t health_level;

    /* Alert tracking */
    substrate_alert_type_t active_alerts[8];
    uint32_t alert_count;

    /* Configuration */
    substrate_config_t config;

    /* Statistics */
    substrate_stats_t stats;

    /* Last update timestamp */
    uint64_t last_update_ms;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} neural_substrate_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default substrate configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Set parameters based on physiological values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int substrate_default_config(substrate_config_t* config);

/**
 * @brief Create neural substrate
 *
 * WHAT: Initialize neural substrate module
 * WHY:  Track metabolic/physical constraints
 * HOW:  Allocate structure, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New substrate or NULL on failure
 */
neural_substrate_t* substrate_create(const substrate_config_t* config);

/**
 * @brief Destroy neural substrate
 *
 * WHAT: Clean up substrate resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure
 *
 * @param substrate Substrate to destroy (NULL safe)
 */
void substrate_destroy(neural_substrate_t* substrate);

/**
 * @brief Reset substrate to initial state
 *
 * @param substrate Neural substrate
 * @return 0 on success
 */
int substrate_reset(neural_substrate_t* substrate);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update substrate state
 *
 * WHAT: Advance substrate simulation
 * WHY:  Update metabolic/physical parameters over time
 * HOW:  Apply recovery, decay, compute modulation
 *
 * @param substrate Neural substrate
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int substrate_update(neural_substrate_t* substrate, uint64_t delta_ms);

/**
 * @brief Record spike event (consumes energy)
 *
 * @param substrate Neural substrate
 * @param neuron_count Number of neurons that spiked
 * @return 0 on success
 */
int substrate_record_spikes(neural_substrate_t* substrate, uint32_t neuron_count);

/**
 * @brief Record synaptic transmission (consumes energy)
 *
 * @param substrate Neural substrate
 * @param transmission_count Number of synaptic events
 * @return 0 on success
 */
int substrate_record_transmissions(neural_substrate_t* substrate, uint32_t transmission_count);

/* ============================================================================
 * Setter API - For external modulation (e.g., immune system)
 * ============================================================================ */

/**
 * @brief Set ATP level
 *
 * @param substrate Neural substrate
 * @param atp_level New ATP level [0-1]
 * @return 0 on success
 */
int substrate_set_atp(neural_substrate_t* substrate, float atp_level);

/**
 * @brief Set oxygen saturation
 *
 * @param substrate Neural substrate
 * @param o2_sat New O2 saturation [0-1]
 * @return 0 on success
 */
int substrate_set_oxygen(neural_substrate_t* substrate, float o2_sat);

/**
 * @brief Set glucose level
 *
 * @param substrate Neural substrate
 * @param glucose New glucose level [0-1]
 * @return 0 on success
 */
int substrate_set_glucose(neural_substrate_t* substrate, float glucose);

/**
 * @brief Set temperature
 *
 * @param substrate Neural substrate
 * @param temperature New temperature (°C)
 * @return 0 on success
 */
int substrate_set_temperature(neural_substrate_t* substrate, float temperature);

/**
 * @brief Set membrane integrity
 *
 * @param substrate Neural substrate
 * @param integrity New membrane integrity [0-1]
 * @return 0 on success
 */
int substrate_set_membrane_integrity(neural_substrate_t* substrate, float integrity);

/**
 * @brief Set ion balance
 *
 * @param substrate Neural substrate
 * @param balance New ion balance [0-1]
 * @return 0 on success
 */
int substrate_set_ion_balance(neural_substrate_t* substrate, float balance);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current metabolic state
 *
 * @param substrate Neural substrate
 * @param state Output metabolic state
 * @return 0 on success
 */
int substrate_get_metabolic_state(
    const neural_substrate_t* substrate,
    substrate_metabolic_state_t* state
);

/**
 * @brief Get current physical state
 *
 * @param substrate Neural substrate
 * @param state Output physical state
 * @return 0 on success
 */
int substrate_get_physical_state(
    const neural_substrate_t* substrate,
    substrate_physical_state_t* state
);

/**
 * @brief Get current modulation factors
 *
 * @param substrate Neural substrate
 * @param mod Output modulation factors
 * @return 0 on success
 */
int substrate_get_modulation(
    const neural_substrate_t* substrate,
    substrate_modulation_t* mod
);

/**
 * @brief Get substrate health level
 *
 * @param substrate Neural substrate
 * @return Current health level
 */
substrate_health_level_t substrate_get_health_level(const neural_substrate_t* substrate);

/**
 * @brief Get overall capacity factor
 *
 * @param substrate Neural substrate
 * @return Overall capacity [0-1]
 */
float substrate_get_capacity(const neural_substrate_t* substrate);

/**
 * @brief Get firing rate modulation
 *
 * @param substrate Neural substrate
 * @return Firing rate multiplier
 */
float substrate_get_firing_modulation(const neural_substrate_t* substrate);

/**
 * @brief Get transmission efficiency
 *
 * @param substrate Neural substrate
 * @return Transmission efficiency [0-1]
 */
float substrate_get_transmission_efficiency(const neural_substrate_t* substrate);

/**
 * @brief Get active alerts
 *
 * @param substrate Neural substrate
 * @param alerts Output array (pre-allocated, size 8)
 * @param count Output number of active alerts
 * @return 0 on success
 */
int substrate_get_alerts(
    const neural_substrate_t* substrate,
    substrate_alert_type_t* alerts,
    uint32_t* count
);

/**
 * @brief Get substrate statistics
 *
 * @param substrate Neural substrate
 * @param stats Output statistics
 * @return 0 on success
 */
int substrate_get_stats(
    const neural_substrate_t* substrate,
    substrate_stats_t* stats
);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert health level to string
 *
 * @param level Health level
 * @return Human-readable string
 */
const char* substrate_health_level_to_string(substrate_health_level_t level);

/**
 * @brief Convert alert type to string
 *
 * @param alert Alert type
 * @return Human-readable string
 */
const char* substrate_alert_type_to_string(substrate_alert_type_t alert);

/* ============================================================================
 * Imagination Engine Integration API
 *
 * BIOLOGICAL BASIS:
 * Imagination is metabolically expensive, requiring sustained activity in
 * prefrontal cortex and default mode network. ATP depletion and accumulated
 * fatigue reduce imaginative capacity - creative thinking and mental simulation
 * abilities decline with energy depletion.
 * ============================================================================ */

/**
 * @brief Send current imagination capacity to imagination engine
 *
 * WHAT: Notify imagination engine of current substrate capacity for imagination
 * WHY:  Allow imagination to adjust scenario complexity based on metabolic state
 * HOW:  Compute capacity modifier from ATP and fatigue, send via bio-async
 *
 * @param substrate Neural substrate
 * @return 0 on success, -1 on error
 *
 * CAPACITY FORMULA:
 * capacity = atp_level * (1 - fatigue * 0.5)
 * - Low ATP reduces available energy for DMN activity
 * - Fatigue impairs prefrontal function even with ATP available
 * - Minimum 10% capacity persists (consciousness maintained)
 */
int neural_substrate_send_imagination_capacity(neural_substrate_t* substrate);

/**
 * @brief Register bio-async handler for imagination integration
 *
 * WHAT: Connect neural substrate to imagination engine via bio-async
 * WHY:  Enable bidirectional communication for capacity modulation
 * HOW:  Register module, set up message handlers
 *
 * @param substrate Neural substrate to connect
 * @return 0 on success, -1 if bio-async not available
 */
int neural_substrate_register_imagination_handler(neural_substrate_t* substrate);

/**
 * @brief Unregister imagination bio-async handler
 *
 * WHAT: Disconnect neural substrate from imagination engine
 * WHY:  Clean shutdown of bio-async integration
 * HOW:  Unregister module from bio-async router
 *
 * @return 0 on success
 */
int neural_substrate_unregister_imagination_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEURAL_SUBSTRATE_H */
