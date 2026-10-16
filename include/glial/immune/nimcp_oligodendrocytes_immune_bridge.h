/**
 * @file nimcp_oligodendrocytes_immune_bridge.h
 * @brief Oligodendrocytes-Immune Bridge - Connects oligodendrocytes to brain immune system
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bridge connecting oligodendrocytes to brain immune system for demyelination modeling
 * WHY:  Inflammation damages oligodendrocytes and myelin (MS-like pathophysiology)
 * HOW:  Routes cytokine signals to modulate myelination and oligodendrocyte survival
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines (IL-1, TNF, IFN-gamma) damage oligodendrocytes
 * - Oligodendrocyte death leads to demyelination
 * - Anti-inflammatory cytokines (IL-10) protect oligodendrocytes
 * - Microglia-oligodendrocyte crosstalk regulates remyelination
 * - Oligodendrocyte progenitor cells (OPCs) are recruited for repair
 *
 * PATHOPHYSIOLOGY MODELED:
 * - Multiple sclerosis (MS)-like demyelination
 * - Inflammatory demyelinating lesions
 * - Remyelination capacity under inflammation
 * - OPC recruitment and differentiation
 *
 * INTEGRATION POINTS:
 * - nimcp_oligodendrocytes.h: Oligodendrocyte state and myelination
 * - nimcp_brain_immune.h: Brain immune system and cytokines
 * - nimcp_microglia.h: Microglia polarization and cytokine release
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OLIGODENDROCYTES_IMMUNE_BRIDGE_H
#define NIMCP_OLIGODENDROCYTES_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define OLIGO_IMMUNE_MODULE_NAME    "oligo_immune_bridge"
#define OLIGO_IMMUNE_MAX_DAMAGE     1.0f    /**< Max cumulative damage */
#define OLIGO_IMMUNE_DEATH_THRESHOLD 0.9f   /**< Damage level for death */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct oligo_immune_bridge oligo_immune_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Oligodendrocyte damage state
 */
typedef enum {
    OLIGO_DAMAGE_NONE = 0,       /**< Healthy */
    OLIGO_DAMAGE_MILD,           /**< Early inflammation */
    OLIGO_DAMAGE_MODERATE,       /**< Significant stress */
    OLIGO_DAMAGE_SEVERE,         /**< Near death */
    OLIGO_DAMAGE_DEAD            /**< Cell death (apoptosis) */
} oligo_damage_state_t;

/**
 * @brief Demyelination state
 */
typedef enum {
    DEMYELINATION_NONE = 0,      /**< Normal myelin */
    DEMYELINATION_EARLY,         /**< Beginning damage */
    DEMYELINATION_ACTIVE,        /**< Active destruction */
    DEMYELINATION_CHRONIC,       /**< Long-term damage */
    DEMYELINATION_REMYELINATING  /**< Repair in progress */
} demyelination_state_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Oligodendrocytes-immune bridge configuration
 */
typedef struct {
    /* Cytokine sensitivity */
    float il1_myelination_reduction;    /**< IL-1 myelin damage rate */
    float il6_progenitor_inhibition;    /**< IL-6 OPC inhibition */
    float tnf_oligodendrocyte_death;    /**< TNF-alpha death rate */
    float il10_protection_factor;       /**< IL-10 protective effect */
    float ifn_gamma_demyelination;      /**< IFN-gamma demyelination rate */

    /* Damage dynamics */
    float damage_accumulation_rate;     /**< How fast damage accumulates */
    float damage_repair_rate;           /**< Natural repair rate */
    float death_threshold;              /**< Damage level for apoptosis */

    /* Remyelination */
    float remyelination_rate;           /**< Base remyelination rate */
    float opc_recruitment_rate;         /**< OPC recruitment rate */
    bool enable_remyelination;          /**< Enable repair mechanisms */

    /* Bio-async */
    bool enable_bio_async;
    uint32_t inbox_capacity;
} oligo_immune_config_t;

/**
 * @brief Cytokine effects on oligodendrocytes
 */
typedef struct {
    float il1_effect;                   /**< IL-1 damage (0-1) */
    float il6_effect;                   /**< IL-6 inhibition (0-1) */
    float tnf_effect;                   /**< TNF death signal (0-1) */
    float il10_effect;                  /**< IL-10 protection (0-1) */
    float ifn_gamma_effect;             /**< IFN-gamma demyelination (0-1) */
    float net_damage_signal;            /**< Combined damage signal */
    float net_protection_signal;        /**< Combined protection signal */
} oligo_cytokine_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t cytokine_events;
    uint64_t damage_events;
    uint64_t death_events;
    uint64_t remyelination_events;
    float total_damage_accumulated;
    float total_myelin_lost;
    float total_myelin_restored;
    float current_damage_level;
    demyelination_state_t demyelination_state;
} oligo_immune_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Oligodendrocytes-immune bridge state
 */
struct oligo_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    oligo_immune_config_t config;

    /* Core connections */
    oligodendrocyte_t* oligo;                   /**< Connected oligodendrocyte */
    oligodendrocyte_network_t* oligo_network;   /**< Or network if applicable */
    brain_immune_system_t* immune_system;       /**< Brain immune system */

    /* Computed effects */
    oligo_cytokine_effects_t cytokine_effects;

    /* State */
    float damage_level;                         /**< Cumulative damage (0-1) */
    float myelination_rate_modifier;            /**< Modifier on myelination */
    float progenitor_recruitment;               /**< OPC recruitment level */
    oligo_damage_state_t damage_state;
    demyelination_state_t demyelination_state;

    /* Statistics */
    oligo_immune_stats_t stats;

    /* State */
    bool initialized;
    uint64_t last_update_time;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide biologically realistic defaults
 * WHY:  Easy initialization with MS-like parameters
 * HOW:  Return struct based on neuroimmunology literature
 *
 * @param config Output configuration
 * @return 0 on success
 */
int oligo_immune_default_config(oligo_immune_config_t* config);

/**
 * @brief Create oligodendrocytes-immune bridge
 *
 * WHAT: Initialize bridge connecting oligodendrocytes to immune system
 * WHY:  Model inflammation-induced demyelination
 * HOW:  Set up cytokine monitoring and damage tracking
 *
 * @param config Configuration (NULL for defaults)
 * @param oligo Oligodendrocyte to connect (or NULL)
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
oligo_immune_bridge_t* oligo_immune_create(
    const oligo_immune_config_t* config,
    oligodendrocyte_t* oligo,
    brain_immune_system_t* immune_system);

/**
 * @brief Destroy oligodendrocytes-immune bridge
 *
 * @param bridge Bridge to destroy
 */
void oligo_immune_destroy(oligo_immune_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to oligodendrocyte network
 *
 * @param bridge Oligo-immune bridge
 * @param network Oligodendrocyte network
 * @return 0 on success
 */
int oligo_immune_connect_network(
    oligo_immune_bridge_t* bridge,
    oligodendrocyte_network_t* network);

/**
 * @brief Connect to bio-async router
 */
int oligo_immune_connect_bio_async(oligo_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int oligo_immune_disconnect_bio_async(oligo_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 */
bool oligo_immune_is_bio_async_connected(const oligo_immune_bridge_t* bridge);

/* ============================================================================
 * Cytokine Effects API
 * ============================================================================ */

/**
 * @brief Update cytokine effects from immune system
 *
 * WHAT: Fetch current cytokine levels and compute effects
 * WHY:  Cytokines drive oligodendrocyte damage/protection
 * HOW:  Query immune system, apply sensitivity factors
 *
 * @param bridge Oligo-immune bridge
 * @return 0 on success
 */
int oligo_immune_update_cytokine_effects(oligo_immune_bridge_t* bridge);

/**
 * @brief Get current cytokine effects
 *
 * @param bridge Oligo-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int oligo_immune_get_cytokine_effects(
    const oligo_immune_bridge_t* bridge,
    oligo_cytokine_effects_t* effects);

/**
 * @brief Apply cytokine modulation to oligodendrocyte
 *
 * WHAT: Apply computed cytokine effects to oligodendrocyte state
 * WHY:  Translate immune signals to cellular effects
 * HOW:  Modify myelination rate, accumulate damage
 *
 * @param bridge Oligo-immune bridge
 * @return 0 on success
 */
int oligo_immune_apply_modulation(oligo_immune_bridge_t* bridge);

/* ============================================================================
 * Damage and Death API
 * ============================================================================ */

/**
 * @brief Accumulate damage from cytokines
 *
 * WHAT: Add damage from current cytokine levels
 * WHY:  Model cumulative inflammatory damage
 * HOW:  Apply damage rate based on net damage signal
 *
 * @param bridge Oligo-immune bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int oligo_immune_accumulate_damage(
    oligo_immune_bridge_t* bridge,
    float dt_ms);

/**
 * @brief Check and process cell death
 *
 * WHAT: Check if damage exceeds death threshold
 * WHY:  Model oligodendrocyte apoptosis
 * HOW:  Compare damage to threshold, trigger death cascade
 *
 * @param bridge Oligo-immune bridge
 * @return true if cell died
 */
bool oligo_immune_check_death(oligo_immune_bridge_t* bridge);

/**
 * @brief Get current damage state
 *
 * @param bridge Oligo-immune bridge
 * @return Damage state
 */
oligo_damage_state_t oligo_immune_get_damage_state(
    const oligo_immune_bridge_t* bridge);

/* ============================================================================
 * Demyelination API
 * ============================================================================ */

/**
 * @brief Process demyelination
 *
 * WHAT: Apply demyelination based on damage and inflammation
 * WHY:  Model myelin loss in inflammatory conditions
 * HOW:  Reduce myelination levels based on damage state
 *
 * @param bridge Oligo-immune bridge
 * @param dt_ms Time step
 * @return 0 on success
 */
int oligo_immune_process_demyelination(
    oligo_immune_bridge_t* bridge,
    float dt_ms);

/**
 * @brief Get demyelination state
 *
 * @param bridge Oligo-immune bridge
 * @return Demyelination state
 */
demyelination_state_t oligo_immune_get_demyelination_state(
    const oligo_immune_bridge_t* bridge);

/* ============================================================================
 * Remyelination API
 * ============================================================================ */

/**
 * @brief Process remyelination
 *
 * WHAT: Attempt myelin repair if inflammation resolved
 * WHY:  Model natural repair mechanisms
 * HOW:  Recruit OPCs, differentiate, restore myelin
 *
 * BIOLOGICAL BASIS:
 * Remyelination requires:
 * - Resolution of inflammation (low IL-1, TNF)
 * - OPC availability
 * - Trophic support (BDNF, IGF-1)
 *
 * @param bridge Oligo-immune bridge
 * @param dt_ms Time step
 * @return 0 on success
 */
int oligo_immune_process_remyelination(
    oligo_immune_bridge_t* bridge,
    float dt_ms);

/**
 * @brief Get remyelination capacity
 *
 * @param bridge Oligo-immune bridge
 * @return Remyelination capacity (0-1)
 */
float oligo_immune_get_remyelination_capacity(
    const oligo_immune_bridge_t* bridge);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Full update cycle
 *
 * WHAT: Complete update of oligo-immune state
 * WHY:  Periodic processing of immune effects
 * HOW:  Update cytokines, damage, demyelination, remyelination
 *
 * @param bridge Oligo-immune bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int oligo_immune_update(oligo_immune_bridge_t* bridge, float dt_ms);

/**
 * @brief Get bridge statistics
 */
int oligo_immune_get_stats(
    const oligo_immune_bridge_t* bridge,
    oligo_immune_stats_t* stats);

/**
 * @brief Reset statistics
 */
void oligo_immune_reset_stats(oligo_immune_bridge_t* bridge);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* oligo_damage_state_to_string(oligo_damage_state_t state);
const char* demyelination_state_to_string(demyelination_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLIGODENDROCYTES_IMMUNE_BRIDGE_H */
