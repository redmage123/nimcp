/**
 * @file nimcp_eligibility_fep_bridge.h
 * @brief Free Energy Principle - Eligibility Traces Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and eligibility traces
 * WHY:  Eligibility traces implement temporal credit assignment for FEP learning;
 *       FEP provides structured learning signals for trace-based updates.
 * HOW:  FEP prediction errors modulate trace decay; traces enable delayed
 *       free energy minimization across time.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → ELIGIBILITY PATHWAYS:
 * ---------------------------
 * 1. Prediction Error as Eligibility Signal:
 *    - High PE → strong eligibility marking
 *    - Traces tag synapses for future FEP updates
 *    - Temporal credit assignment for free energy minimization
 *
 * 2. Precision Modulates Trace Decay:
 *    - High precision → slow decay (confident predictions)
 *    - Low precision → fast decay (uncertain predictions)
 *    - λ_effective = λ_base × precision
 *
 * 3. Free Energy Gates Trace Consolidation:
 *    - High FE → consolidate traces (learning needed)
 *    - Low FE → minimal consolidation (converged)
 *
 * ELIGIBILITY → FEP PATHWAYS:
 * ---------------------------
 * 1. Traces Enable Delayed FEP Learning:
 *    - Weight updates occur when both trace and PE present
 *    - Solves temporal credit assignment for FEP
 *    - Bridges delay between action and outcome
 *
 * 2. Trace Magnitude Indicates Learning Readiness:
 *    - Strong traces → synapses ready for FEP update
 *    - Weak traces → skip updates (efficiency)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ELIGIBILITY_FEP_BRIDGE_H
#define NIMCP_ELIGIBILITY_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ELIGIBILITY_FEP_PE_TRACE_SCALING    1.0f
#define ELIGIBILITY_FEP_PRECISION_DECAY_MIN 0.5f
#define ELIGIBILITY_FEP_PRECISION_DECAY_MAX 0.99f

typedef struct eligibility_fep_bridge eligibility_fep_bridge_t;

typedef struct {
    float pe_trace_gain;
    float precision_decay_sensitivity;
    float fe_consolidation_threshold;
    bool enable_pe_eligibility;
    bool enable_precision_decay_modulation;
    bool enable_fe_gated_consolidation;
} eligibility_fep_config_t;

typedef struct {
    float pe_magnitude;
    float pe_trace_scaling;
    float precision_value;
    float precision_decay_modulation;
    float free_energy_value;
    float fe_consolidation_factor;
    float total_decay_modulation;
} eligibility_fep_effects_t;

typedef struct {
    float current_pe;
    float current_precision;
    float current_free_energy;
    float decay_modulation;
    bool consolidation_active;
    uint64_t last_update_time;
} eligibility_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t trace_consolidations;
    float avg_pe;
    float avg_precision;
    float avg_decay_modulation;
} eligibility_fep_stats_t;

struct eligibility_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    eligibility_fep_config_t config;
    fep_system_t* fep_system;
    eligibility_trace_t* eligibility_system;
    uint32_t num_traces;
    eligibility_fep_effects_t effects;
    eligibility_fep_state_t state;
    eligibility_fep_stats_t stats;
};

int eligibility_fep_bridge_default_config(eligibility_fep_config_t* config);
eligibility_fep_bridge_t* eligibility_fep_bridge_create(const eligibility_fep_config_t* config);
void eligibility_fep_bridge_destroy(eligibility_fep_bridge_t* bridge);

int eligibility_fep_bridge_connect_fep(eligibility_fep_bridge_t* bridge, fep_system_t* fep);
int eligibility_fep_bridge_connect_eligibility(eligibility_fep_bridge_t* bridge, eligibility_trace_t* traces, uint32_t num);
int eligibility_fep_bridge_disconnect(eligibility_fep_bridge_t* bridge);

float eligibility_fep_apply_pe_eligibility(eligibility_fep_bridge_t* bridge, float pe);
float eligibility_fep_apply_precision_decay_modulation(eligibility_fep_bridge_t* bridge, float precision);
bool eligibility_fep_should_consolidate(const eligibility_fep_bridge_t* bridge);
float eligibility_fep_get_effective_decay(const eligibility_fep_bridge_t* bridge, float base_decay);

int eligibility_fep_report_consolidation(eligibility_fep_bridge_t* bridge);
int eligibility_fep_bridge_update(eligibility_fep_bridge_t* bridge, uint64_t delta_ms);

int eligibility_fep_bridge_get_state(const eligibility_fep_bridge_t* bridge, eligibility_fep_state_t* state);
int eligibility_fep_bridge_get_stats(const eligibility_fep_bridge_t* bridge, eligibility_fep_stats_t* stats);

int eligibility_fep_bridge_connect_bio_async(eligibility_fep_bridge_t* bridge);
int eligibility_fep_bridge_disconnect_bio_async(eligibility_fep_bridge_t* bridge);
bool eligibility_fep_bridge_is_bio_async_connected(const eligibility_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELIGIBILITY_FEP_BRIDGE_H */
