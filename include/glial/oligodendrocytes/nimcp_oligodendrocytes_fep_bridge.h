/**
 * @file nimcp_oligodendrocytes_fep_bridge.h
 * @brief Free Energy Principle bridge for oligodendrocyte glial cells
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between oligodendrocytes and Free Energy Principle
 * WHY:  Oligodendrocytes optimize conduction velocity (precision of timing)
 * HOW:  FEP temporal precision signals guide myelination; myelin affects FEP timing
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 * - Oligodendrocytes form myelin sheaths to accelerate action potential propagation
 * - Temporal precision: Myelination ensures precise spike timing (critical for FEP)
 * - Activity-dependent myelination: High-activity axons receive more myelin
 * - G-ratio optimization: Optimal myelin thickness maximizes information transfer
 * - Metabolic efficiency: Myelination reduces energy cost of signaling
 *
 * FEP INTEGRATION:
 * =================================================================================
 * - FEP → Oligodendrocyte: Temporal precision requirements drive myelination
 * - Oligodendrocyte → FEP: Conduction delays affect prediction timing
 * - Prediction error minimization: Optimize timing precision
 * - Active inference: Predict optimal myelination patterns
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OLIGODENDROCYTES_FEP_BRIDGE_H
#define NIMCP_OLIGODENDROCYTES_FEP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#define OLIGODENDROCYTES_FEP_DEFAULT_TEMPORAL_SENSITIVITY 0.7f
#define OLIGODENDROCYTES_FEP_DEFAULT_MYELINATION_GAIN 0.5f
#define OLIGODENDROCYTES_FEP_MAX_VELOCITY_MODULATION 50.0f

typedef struct {
    float temporal_sensitivity;
    float myelination_gain;
    float g_ratio_precision_factor;
    bool enable_temporal_precision;
    bool enable_activity_myelination;
    bool enable_metabolic_efficiency;
} oligodendrocytes_fep_config_t;

typedef struct {
    float myelination_rate_modulation;
    float g_ratio_target_shift;
    float temporal_precision_requirement;
    float metabolic_cost_prediction;
} fep_oligodendrocyte_effects_t;

typedef struct {
    float conduction_delay_factor;
    float temporal_precision_contribution;
    float myelination_level;
    float energy_efficiency_ratio;
} oligodendrocyte_fep_effects_t;

typedef struct {
    uint64_t last_update_time;
    float predicted_myelination_level;
    float myelination_prediction_error;
    uint32_t num_myelinated_axons;
} oligodendrocytes_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t temporal_precision_adjustments;
    uint64_t g_ratio_optimizations;
    float avg_conduction_delay;
    float avg_myelination_level;
} oligodendrocytes_fep_stats_t;

typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    oligodendrocytes_fep_config_t config;
    fep_system_t* fep_system;
    oligodendrocyte_network_t* oligodendrocyte_network;
    fep_oligodendrocyte_effects_t fep_effects;
    oligodendrocyte_fep_effects_t oligo_effects;
    oligodendrocytes_fep_state_t state;
    oligodendrocytes_fep_stats_t stats;} oligodendrocytes_fep_bridge_t;

int oligodendrocytes_fep_default_config(oligodendrocytes_fep_config_t* config);
oligodendrocytes_fep_bridge_t* oligodendrocytes_fep_create(
    const oligodendrocytes_fep_config_t* config,
    oligodendrocyte_network_t* oligodendrocyte_network,
    fep_system_t* fep_system
);
void oligodendrocytes_fep_destroy(oligodendrocytes_fep_bridge_t* bridge);
int oligodendrocytes_fep_update_fep_to_oligodendrocyte(oligodendrocytes_fep_bridge_t* bridge);
int oligodendrocytes_fep_update_oligodendrocyte_to_fep(oligodendrocytes_fep_bridge_t* bridge);
int oligodendrocytes_fep_update(oligodendrocytes_fep_bridge_t* bridge);
int oligodendrocytes_fep_apply_modulation(oligodendrocytes_fep_bridge_t* bridge);
float oligodendrocytes_fep_get_myelination_modulation(const oligodendrocytes_fep_bridge_t* bridge);
float oligodendrocytes_fep_get_temporal_precision(const oligodendrocytes_fep_bridge_t* bridge);
int oligodendrocytes_fep_get_stats(const oligodendrocytes_fep_bridge_t* bridge,
                                   oligodendrocytes_fep_stats_t* stats);
int oligodendrocytes_fep_connect_bio_async(oligodendrocytes_fep_bridge_t* bridge);
int oligodendrocytes_fep_disconnect_bio_async(oligodendrocytes_fep_bridge_t* bridge);
bool oligodendrocytes_fep_is_bio_async_connected(const oligodendrocytes_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLIGODENDROCYTES_FEP_BRIDGE_H */
