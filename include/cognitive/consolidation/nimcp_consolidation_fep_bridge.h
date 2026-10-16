/**
 * @file nimcp_consolidation_fep_bridge.h
 * @brief Free Energy Principle - Consolidation Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and memory consolidation
 * WHY:  Consolidation optimizes generative models offline, reducing free energy through
 *       pattern strengthening and synaptic homeostasis.
 * HOW:  FEP model complexity guides consolidation strategy; consolidation improves
 *       model accuracy and reduces long-term free energy.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CONSOLIDATION AS MODEL OPTIMIZATION:
 * -------------------------------------
 * - Friston et al. (2017): Memory consolidation = offline generative model optimization
 * - Replay strengthens important predictions, improves model accuracy
 * - Synaptic scaling maintains precision bounds, prevents runaway complexity
 *
 * FEP → CONSOLIDATION PATHWAYS:
 * ------------------------------
 * 1. Model Complexity Guides Consolidation Strategy:
 *    - High complexity → More replay cycles needed
 *    - Many weak patterns → Aggressive pruning
 *    - Stable model → Minimal consolidation
 *
 * 2. Prediction Error Patterns Select Replay Content:
 *    - High PE patterns → Prioritize for replay
 *    - Frequently wrong predictions → Need strengthening
 *    - Novel patterns → Integrate into model
 *
 * 3. Free Energy Determines Consolidation Urgency:
 *    - High cumulative FE → Urgent consolidation needed
 *    - Low FE → Maintenance consolidation sufficient
 *
 * CONSOLIDATION → FEP PATHWAYS:
 * ------------------------------
 * 1. Pattern Replay Improves Model Accuracy:
 *    - Replay strengthens predictions → Lower PE
 *    - Better model → Lower free energy
 *    - Reduced surprise on future observations
 *
 * 2. Synaptic Pruning Reduces Model Complexity:
 *    - Remove weak connections → Simpler model
 *    - Lower complexity term in free energy
 *    - Improved precision (less noise)
 *
 * 3. Scaling Maintains Precision Homeostasis:
 *    - Normalized weights → Stable precision
 *    - Prevents precision drift
 *    - Maintains free energy bounds
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CONSOLIDATION_FEP_BRIDGE_H
#define NIMCP_CONSOLIDATION_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CONSOLIDATION_FEP_HIGH_COMPLEXITY_THRESHOLD   100.0f
#define CONSOLIDATION_FEP_HIGH_FE_THRESHOLD           20.0f
#define CONSOLIDATION_FEP_REPLAY_FE_REDUCTION         0.1f
#define CONSOLIDATION_FEP_PRUNING_COMPLEXITY_REDUCTION 0.05f
#define CONSOLIDATION_FEP_SCALING_PRECISION_BOOST     0.1f

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct consolidation_fep_bridge consolidation_fep_bridge_t;

typedef struct {
    /* FEP → Consolidation */
    float complexity_replay_scaling;
    float fe_urgency_threshold;
    bool enable_complexity_guided_consolidation;
    bool enable_pe_replay_selection;
    bool enable_fe_urgency;

    /* Consolidation → FEP */
    float replay_fe_reduction;
    float pruning_complexity_reduction;
    float scaling_precision_boost;
    bool enable_replay_fe_reduction;
    bool enable_pruning_complexity;
    bool enable_scaling_precision;

    /* Sensitivity */
    float fe_sensitivity;
    float consolidation_sensitivity;
} consolidation_fep_config_t;

typedef struct {
    float current_complexity;
    float current_free_energy;
    float replay_cycles_needed;
    uint32_t patterns_to_replay;
    float consolidation_urgency;
} consolidation_fep_effects_t;

typedef struct {
    uint32_t patterns_replayed;
    uint32_t connections_pruned;
    float fe_reduction;
    float complexity_reduction;
    float precision_improvement;
} fep_consolidation_effects_t;

typedef struct {
    float current_free_energy;
    float current_complexity;
    float fe_reduction;
    float complexity_reduction;
    bool consolidation_active;
    uint64_t last_consolidation_time;
} consolidation_fep_state_t;

typedef struct {
    uint64_t consolidation_cycles;
    uint64_t replay_events;
    uint64_t pruning_events;
    float total_fe_reduced;
    float total_complexity_reduced;
    float avg_free_energy;
    float avg_complexity;
} consolidation_fep_stats_t;

struct consolidation_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    consolidation_fep_config_t config;
    fep_system_t* fep_system;
    consolidation_fep_effects_t fep_effects;
    fep_consolidation_effects_t consolidation_effects;
    consolidation_fep_state_t state;
    consolidation_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int consolidation_fep_bridge_default_config(consolidation_fep_config_t* config);
consolidation_fep_bridge_t* consolidation_fep_bridge_create(const consolidation_fep_config_t* config);
void consolidation_fep_bridge_destroy(consolidation_fep_bridge_t* bridge);
int consolidation_fep_bridge_connect_fep(consolidation_fep_bridge_t* bridge, fep_system_t* fep);
int consolidation_fep_bridge_disconnect(consolidation_fep_bridge_t* bridge);
int consolidation_fep_bridge_update(consolidation_fep_bridge_t* bridge);
int consolidation_fep_bridge_get_state(consolidation_fep_bridge_t* bridge,
                                        consolidation_fep_state_t* state);
int consolidation_fep_bridge_get_stats(consolidation_fep_bridge_t* bridge,
                                        consolidation_fep_stats_t* stats);
int consolidation_fep_bridge_connect_bio_async(consolidation_fep_bridge_t* bridge);
int consolidation_fep_bridge_disconnect_bio_async(consolidation_fep_bridge_t* bridge);
bool consolidation_fep_bridge_is_bio_async_connected(const consolidation_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONSOLIDATION_FEP_BRIDGE_H */
