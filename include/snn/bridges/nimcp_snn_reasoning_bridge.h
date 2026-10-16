/**
 * @file nimcp_snn_reasoning_bridge.h
 * @brief SNN-Reasoning integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and symbolic reasoning system
 * WHY:  Enable temporal integration for evidence accumulation via spike rates
 * HOW:  Convert spike patterns to evidence strength, spike rate competition for decisions
 *
 * BIOLOGICAL BASIS:
 * - Dorsolateral PFC accumulates evidence through ramping neural activity
 * - Spike rate competition implements winner-take-all decisions
 * - Temporal integration of spikes represents confidence accumulation
 * - Threshold crossing triggers decision commitment
 *
 * INTEGRATION:
 * - SNN → Reasoning: Spike rate represents evidence strength
 * - SNN → Reasoning: Population competition implements decision-making
 * - Reasoning → SNN: Inference goals bias population activity
 * - Reasoning → SNN: Successful proofs modulate learning rates
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_REASONING_BRIDGE_H
#define NIMCP_SNN_REASONING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"

/* Forward declaration - reasoning system is complex, use void* */
typedef void* reasoning_system_t;

//=============================================================================
// Configuration Types
//=============================================================================

typedef struct snn_reasoning_config_s {
    float evidence_rate_min;            /**< Min spike rate for evidence (Hz) */
    float evidence_rate_max;            /**< Max spike rate (Hz) */
    float decision_threshold;           /**< Threshold for decision commitment */
    float integration_time_window_ms;   /**< Temporal integration window */
    bool enable_competition;            /**< Enable population competition */
    float lateral_inhibition;           /**< Inhibition strength */
    uint32_t reasoning_population_id;   /**< Population for reasoning */
    float update_interval_ms;           /**< Update interval */
    bool enable_bio_async;              /**< Enable bio-async */
} snn_reasoning_config_t;

typedef struct snn_reasoning_state_s {
    float evidence_accumulation;        /**< Accumulated evidence [0, 1] */
    float decision_confidence;          /**< Decision confidence */
    bool decision_committed;            /**< Decision made */
    uint32_t winning_population;        /**< Winner in competition */
    uint32_t integration_steps;         /**< Steps of integration */
    float avg_evidence_rate;            /**< Average evidence rate */
} snn_reasoning_state_t;

typedef struct snn_reasoning_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;
    reasoning_system_t reasoning;
    snn_reasoning_config_t config;
    snn_reasoning_state_t state;
    snn_population_t* reasoning_pop;
    float last_update_time;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;
    void* mutex;
} snn_reasoning_bridge_t;

//=============================================================================
// API Functions
//=============================================================================

void snn_reasoning_config_default(snn_reasoning_config_t* config);

snn_reasoning_bridge_t* snn_reasoning_bridge_create(
    const snn_reasoning_config_t* config,
    snn_network_t* snn,
    reasoning_system_t reasoning
);

void snn_reasoning_bridge_destroy(snn_reasoning_bridge_t* bridge);

int snn_reasoning_bridge_connect_bio_async(snn_reasoning_bridge_t* bridge);
int snn_reasoning_bridge_disconnect_bio_async(snn_reasoning_bridge_t* bridge);
bool snn_reasoning_bridge_is_bio_async_connected(const snn_reasoning_bridge_t* bridge);

int snn_reasoning_bridge_process(
    snn_reasoning_bridge_t* bridge,
    const float* input,
    float* output
);

int snn_reasoning_bridge_update(snn_reasoning_bridge_t* bridge, float dt);

float snn_reasoning_accumulate_evidence(
    snn_reasoning_bridge_t* bridge,
    float spike_rate
);

int snn_reasoning_compete_populations(
    snn_reasoning_bridge_t* bridge,
    uint32_t* population_ids,
    uint32_t num_populations,
    uint32_t* winner
);

bool snn_reasoning_check_decision_threshold(const snn_reasoning_bridge_t* bridge);

int snn_reasoning_bridge_get_state(
    const snn_reasoning_bridge_t* bridge,
    snn_reasoning_state_t* state
);

float snn_reasoning_get_evidence(const snn_reasoning_bridge_t* bridge);
float snn_reasoning_get_confidence(const snn_reasoning_bridge_t* bridge);
bool snn_reasoning_is_decision_made(const snn_reasoning_bridge_t* bridge);

int snn_reasoning_get_stats(
    const snn_reasoning_bridge_t* bridge,
    uint32_t* integration_steps,
    uint32_t* decisions_made,
    float* avg_confidence
);

void snn_reasoning_reset_stats(snn_reasoning_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_REASONING_BRIDGE_H */
