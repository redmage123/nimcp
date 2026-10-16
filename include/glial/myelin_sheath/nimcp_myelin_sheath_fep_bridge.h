/**
 * @file nimcp_myelin_sheath_fep_bridge.h
 * @brief Free Energy Principle bridge for myelin sheath structures
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between myelin sheaths and Free Energy Principle
 * WHY:  Myelin provides temporal precision for spike propagation
 * HOW:  FEP precision signals optimize myelin structure; myelin affects FEP timing
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 * - Myelin sheath structural integrity ensures reliable signal propagation
 * - G-ratio optimization: Precision-driven myelin thickness adjustment
 * - Paranodal junctions: Seal myelin and maintain temporal fidelity
 * - Conduction block: Pathological states reduce precision
 * - Metabolic efficiency: Energy cost vs. information transmission trade-off
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MYELIN_SHEATH_FEP_BRIDGE_H
#define NIMCP_MYELIN_SHEATH_FEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#define MYELIN_SHEATH_FEP_DEFAULT_INTEGRITY_SENSITIVITY 0.9f
#define MYELIN_SHEATH_FEP_DEFAULT_G_RATIO_GAIN 0.6f

typedef struct {
    float integrity_sensitivity;
    float g_ratio_gain;
    float velocity_precision_factor;
    bool enable_integrity_precision;
    bool enable_g_ratio_optimization;
    bool enable_conduction_block_modeling;
} myelin_sheath_fep_config_t;

typedef struct {
    float target_g_ratio_shift;
    float lamellae_count_modulation;
    float repair_rate_modulation;
    float compaction_target;
} fep_myelin_sheath_effects_t;

typedef struct {
    float velocity_precision_contribution;
    float integrity_uncertainty;
    float conduction_reliability;
    float metabolic_efficiency;
} myelin_sheath_fep_effects_t;

typedef struct {
    uint64_t last_update_time;
    float predicted_integrity;
    float integrity_prediction_error;
    uint32_t num_segments_monitored;
} myelin_sheath_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t g_ratio_optimizations;
    uint64_t integrity_adjustments;
    float avg_velocity_precision;
    float avg_integrity;
} myelin_sheath_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    myelin_sheath_fep_config_t config;
    fep_system_t* fep_system;
    myelin_sheath_network_t* myelin_network;
    fep_myelin_sheath_effects_t fep_effects;
    myelin_sheath_fep_effects_t myelin_effects;
    myelin_sheath_fep_state_t state;
    myelin_sheath_fep_stats_t stats;} myelin_sheath_fep_bridge_t;

int myelin_sheath_fep_default_config(myelin_sheath_fep_config_t* config);
myelin_sheath_fep_bridge_t* myelin_sheath_fep_create(
    const myelin_sheath_fep_config_t* config,
    myelin_sheath_network_t* myelin_network,
    fep_system_t* fep_system
);
void myelin_sheath_fep_destroy(myelin_sheath_fep_bridge_t* bridge);
int myelin_sheath_fep_update_fep_to_myelin(myelin_sheath_fep_bridge_t* bridge);
int myelin_sheath_fep_update_myelin_to_fep(myelin_sheath_fep_bridge_t* bridge);
int myelin_sheath_fep_update(myelin_sheath_fep_bridge_t* bridge);
int myelin_sheath_fep_apply_modulation(myelin_sheath_fep_bridge_t* bridge);
float myelin_sheath_fep_get_g_ratio_shift(const myelin_sheath_fep_bridge_t* bridge);
float myelin_sheath_fep_get_velocity_precision(const myelin_sheath_fep_bridge_t* bridge);
int myelin_sheath_fep_get_stats(const myelin_sheath_fep_bridge_t* bridge,
                                myelin_sheath_fep_stats_t* stats);
int myelin_sheath_fep_connect_bio_async(myelin_sheath_fep_bridge_t* bridge);
int myelin_sheath_fep_disconnect_bio_async(myelin_sheath_fep_bridge_t* bridge);
bool myelin_sheath_fep_is_bio_async_connected(const myelin_sheath_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MYELIN_SHEATH_FEP_BRIDGE_H */
