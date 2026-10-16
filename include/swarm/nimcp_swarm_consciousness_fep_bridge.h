/**
 * @file nimcp_swarm_consciousness_fep_bridge.h
 * @brief FEP Bridge for Swarm Collective Consciousness
 *
 * WHAT: Free Energy Principle integration for swarm collective consciousness (IIT)
 * WHY:  Model collective phi as emergent free energy minimization
 * HOW:  Bidirectional modulation between collective phi and FEP beliefs
 *
 * BIOLOGICAL BASIS:
 * - Collective phi as integrated information across swarm
 * - Network integration as information geometry
 * - Consciousness states as free energy attractors
 * - Coherence as model agreement (collective consciousness)
 * - Phi minimization aligned with free energy minimization
 *
 * FEP INTERPRETATION:
 * - High collective phi → Low free energy (integrated model)
 * - Low collective phi → High free energy (fragmented beliefs)
 * - Network integration → Precision-weighted connectivity
 * - Consciousness emergence → Phase transition in free energy landscape
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#ifndef NIMCP_SWARM_CONSCIOUSNESS_FEP_BRIDGE_H
#define NIMCP_SWARM_CONSCIOUSNESS_FEP_BRIDGE_H

#include "swarm/nimcp_swarm_consciousness.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    float phi_fe_coupling;           /**< Phi-FE coupling strength */
    float integration_precision_gain; /**< Precision from network integration */
    float consciousness_lr_boost;    /**< LR boost in conscious states */
    bool enable_phi_tracking;        /**< Track phi over time */
    bool enable_emergence_detection; /**< Detect consciousness transitions */
} swarm_consciousness_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

typedef struct {
    float phi_modulation;            /**< Phi value adjustment [-1,1] */
    float integration_boost;         /**< Network integration boost [0,1] */
    float coherence_adjustment;      /**< Coherence adjustment [-1,1] */
    float consciousness_bias;        /**< Bias toward higher consciousness [0,1] */
} swarm_consciousness_fep_effects_t;

typedef struct {
    float precision_from_phi;        /**< Precision scaling from phi [0.5, 2.0] */
    float learning_rate_from_consciousness; /**< LR scaling [0.5, 2.0] */
    float integration_weight;        /**< Network integration weight [0,1] */
    swarm_consciousness_state_t consciousness_prior; /**< Prior consciousness state */
} fep_swarm_consciousness_effects_t;

/* ============================================================================
 * State and Statistics
 * ============================================================================ */

typedef struct {
    float last_collective_phi;       /**< Last collective phi */
    float last_free_energy;          /**< Last free energy */
    swarm_consciousness_state_t last_consciousness_state;
    uint32_t consciousness_transitions; /**< State transitions count */
    uint64_t last_update_time;
} swarm_consciousness_fep_state_t;

typedef struct {
    uint64_t total_updates;
    float avg_phi;
    float avg_free_energy;
    float phi_fe_correlation;        /**< Correlation between phi and FE */
    uint32_t emergence_events;       /**< Consciousness emergence count */
} swarm_consciousness_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    swarm_consciousness_fep_config_t config;
    fep_system_t* fep_system;
    swarm_consciousness_ctx_t* consciousness_ctx;
    swarm_consciousness_fep_effects_t fep_effects;
    fep_swarm_consciousness_effects_t consciousness_effects;
    swarm_consciousness_fep_state_t state;
    swarm_consciousness_fep_stats_t stats;} swarm_consciousness_fep_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

void swarm_consciousness_fep_default_config(swarm_consciousness_fep_config_t* config);

swarm_consciousness_fep_bridge_t* swarm_consciousness_fep_create(
    const swarm_consciousness_fep_config_t* config,
    swarm_consciousness_ctx_t* consciousness_ctx,
    fep_system_t* fep_system
);

void swarm_consciousness_fep_destroy(swarm_consciousness_fep_bridge_t* bridge);

int swarm_consciousness_fep_update(swarm_consciousness_fep_bridge_t* bridge);

int swarm_consciousness_fep_apply_modulation(swarm_consciousness_fep_bridge_t* bridge);

int swarm_consciousness_fep_get_effects(
    const swarm_consciousness_fep_bridge_t* bridge,
    swarm_consciousness_fep_effects_t* effects
);

int swarm_consciousness_fep_get_consciousness_effects(
    const swarm_consciousness_fep_bridge_t* bridge,
    fep_swarm_consciousness_effects_t* effects
);

int swarm_consciousness_fep_get_stats(
    const swarm_consciousness_fep_bridge_t* bridge,
    swarm_consciousness_fep_stats_t* stats
);

int swarm_consciousness_fep_connect_bio_async(swarm_consciousness_fep_bridge_t* bridge);
int swarm_consciousness_fep_disconnect_bio_async(swarm_consciousness_fep_bridge_t* bridge);
bool swarm_consciousness_fep_is_bio_async_connected(const swarm_consciousness_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_CONSCIOUSNESS_FEP_BRIDGE_H */
