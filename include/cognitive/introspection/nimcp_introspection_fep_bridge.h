/**
 * @file nimcp_introspection_fep_bridge.h
 * @brief Free Energy Principle - Introspection Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and Introspection
 * WHY:  Metacognition as precision estimation. Introspection monitors FEP state
 *       (prediction errors, uncertainty, free energy), enabling metacognitive
 *       awareness of inference quality.
 * HOW:  FEP precision → introspective confidence; introspection → FEP meta-learning
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex (ACC) monitors prediction errors (metacognition)
 * - Prefrontal Cortex tracks uncertainty (metacognitive confidence)
 * - Introspection = precision estimation of own beliefs
 * - Reference: Fleming & Dolan (2012) "The neural basis of metacognitive ability"
 */

#ifndef NIMCP_INTROSPECTION_FEP_BRIDGE_H
#define NIMCP_INTROSPECTION_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

#define INTROSPECTION_FEP_HIGH_PE_THRESHOLD      5.0f
#define INTROSPECTION_FEP_HIGH_UNCERTAINTY       0.7f
#define INTROSPECTION_FEP_META_UPDATE_RATE       0.1f

typedef struct introspection_fep_bridge introspection_fep_bridge_t;

typedef struct {
    float pe_threshold;
    float uncertainty_threshold;
    float meta_learning_rate;
    bool enable_precision_monitoring;
    bool enable_meta_learning;
    float pe_sensitivity;
} introspection_fep_config_t;

typedef struct {
    float precision_estimate;
    float uncertainty_estimate;
    float meta_confidence;
} introspection_fep_effects_t;

typedef struct {
    float current_precision;
    float current_uncertainty;
    uint32_t pe_events_monitored;
} introspection_fep_state_t;

typedef struct {
    uint64_t precision_estimates_total;
    float avg_precision;
    float avg_uncertainty;
} introspection_fep_stats_t;

struct introspection_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    introspection_fep_config_t config;
    fep_system_t* fep_system;
    introspection_context_t introspection_system;
    introspection_fep_effects_t effects;
    introspection_fep_state_t state;
    introspection_fep_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

int introspection_fep_bridge_default_config(introspection_fep_config_t* config);
introspection_fep_bridge_t* introspection_fep_bridge_create(const introspection_fep_config_t* config);
void introspection_fep_bridge_destroy(introspection_fep_bridge_t* bridge);

int introspection_fep_bridge_connect_fep(introspection_fep_bridge_t* bridge, fep_system_t* fep);
int introspection_fep_bridge_connect_introspection(introspection_fep_bridge_t* bridge, introspection_context_t intro);

int introspection_fep_estimate_precision(introspection_fep_bridge_t* bridge, float* precision);
int introspection_fep_monitor_uncertainty(introspection_fep_bridge_t* bridge);
int introspection_fep_meta_learn(introspection_fep_bridge_t* bridge, float prediction_error);

int introspection_fep_bridge_update(introspection_fep_bridge_t* bridge, uint64_t delta_ms);
int introspection_fep_bridge_get_state(introspection_fep_bridge_t* bridge, introspection_fep_state_t* state);
int introspection_fep_bridge_get_stats(introspection_fep_bridge_t* bridge, introspection_fep_stats_t* stats);

int introspection_fep_bridge_connect_bio_async(introspection_fep_bridge_t* bridge);
int introspection_fep_bridge_disconnect_bio_async(introspection_fep_bridge_t* bridge);
bool introspection_fep_bridge_is_bio_async_connected(const introspection_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_FEP_BRIDGE_H */
