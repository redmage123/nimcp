/**
 * @file nimcp_second_messengers_fep_bridge.h
 * @brief Free Energy Principle - Second Messengers Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and second messenger cascades
 * WHY:  Second messengers implement intracellular FEP: cAMP/PKA=precision optimization, Ca²⁺=prediction errors, CREB=model updates.
 *       Essential for biochemical realization of active inference and synaptic consolidation.
 * HOW:  FEP prediction errors trigger Ca²⁺ release; FEP precision modulates cAMP/PKA; cascade states feedback to FEP plasticity modulation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SECOND MESSENGERS AS FEP IMPLEMENTATION (Friston, 2008):
 * ----------------------------------------------------------
 * 1. Calcium as Prediction Error Signal:
 *    - Ca²⁺ influx ∝ |prediction error| (NMDA receptors)
 *    - Spike timing coincidence = temporal prediction error
 *    - Ca²⁺ amplitude encodes PE magnitude
 *    - Reference: Friston (2008) "Hierarchical models in the brain"
 *
 * 2. cAMP/PKA as Precision Controller:
 *    - cAMP levels modulate synaptic gain (precision)
 *    - PKA phosphorylates GluR1 → AMPAR insertion (precision ↑)
 *    - D1 (Gs) increases precision; D2 (Gi) decreases precision
 *    - Reference: Seamans & Yang (2004) "The principal features of dopamine"
 *
 * 3. IP3/DAG/PKC as Complexity Regulator:
 *    - IP3-mediated Ca²⁺ release = internal model complexity
 *    - PKC modulates receptor trafficking (model refinement)
 *    - Gq pathway = hierarchical message passing
 *
 * 4. CREB/IEGs as Model Parameter Updates:
 *    - CREB phosphorylation = synaptic tag (capture FEP update)
 *    - Arc/c-Fos expression = long-term model consolidation
 *    - Gene expression = slow timescale FEP optimization
 *    - Reference: Lisman et al. (2011) "Memory formation depends on both synapse-specific modifications"
 *
 * FEP → SECOND MESSENGER PATHWAYS:
 * ---------------------------------
 * - Prediction error magnitude → Ca²⁺ release amplitude
 * - Precision estimate → cAMP/PKA activity
 * - Complexity cost → IP3/DAG modulation
 * - Expected free energy → CREB phosphorylation threshold
 *
 * SECOND MESSENGER → FEP PATHWAYS:
 * ---------------------------------
 * - Ca²⁺ levels → effective prediction error weight
 * - PKA activity → precision estimate
 * - CaMKII autophosphorylation → plasticity modulation
 * - CREB/IEG levels → model update confidence
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECOND_MESSENGERS_FEP_BRIDGE_H
#define NIMCP_SECOND_MESSENGERS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "plasticity/nimcp_second_messengers.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SM_FEP_CA_PE_GAIN           100.0f  /**< Ca²⁺ response to PE (nM per PE unit) */
#define SM_FEP_CAMP_PRECISION_GAIN  5.0f    /**< cAMP response to precision (μM per precision unit) */
#define SM_FEP_IP3_COMPLEXITY_GAIN  2.0f    /**< IP3 response to complexity (μM per complexity unit) */
#define SM_FEP_CREB_EFE_THRESHOLD   0.5f    /**< EFE threshold for CREB phosphorylation */

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct sm_fep_bridge sm_fep_bridge_t;

typedef struct {
    float ca_pe_sensitivity;
    float camp_precision_sensitivity;
    float ip3_complexity_sensitivity;
    float creb_efe_sensitivity;
    bool enable_ca_pe_coupling;
    bool enable_camp_precision_coupling;
    bool enable_ip3_complexity_coupling;
    bool enable_creb_plasticity_coupling;
} sm_fep_config_t;

typedef struct {
    float pe_magnitude;
    float ca_release;
    float precision_value;
    float camp_modulation;
    float complexity_value;
    float ip3_modulation;
    float efe_value;
    float creb_threshold;
} sm_fep_effects_t;

typedef struct {
    float ca_level;
    float camp_level;
    float pka_activity;
    float camkii_activity;
    float creb_phosphorylation;
    float plasticity_modulation;
    float precision_estimate;
} sm_fep_feedback_t;

typedef struct {
    uint64_t total_updates;
    uint64_t ca_releases;
    uint64_t camp_modulations;
    uint64_t creb_phosphorylations;
    float avg_ca_level;
    float avg_pka_activity;
    float avg_plasticity_modulation;
} sm_fep_stats_t;

struct sm_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    sm_fep_config_t config;
    fep_system_t* fep_system;
    second_messenger_system_t* sm_system;
    uint32_t neuron_id;
    sm_fep_effects_t fep_effects;
    sm_fep_feedback_t sm_effects;
    sm_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int sm_fep_bridge_default_config(sm_fep_config_t* config);
sm_fep_bridge_t* sm_fep_bridge_create(const sm_fep_config_t* config, uint32_t neuron_id);
void sm_fep_bridge_destroy(sm_fep_bridge_t* bridge);

int sm_fep_bridge_connect_fep(sm_fep_bridge_t* bridge, fep_system_t* fep);
int sm_fep_bridge_connect_sm(sm_fep_bridge_t* bridge, second_messenger_system_t* sm);
int sm_fep_bridge_disconnect(sm_fep_bridge_t* bridge);

float sm_fep_compute_ca_from_pe(sm_fep_bridge_t* bridge, float pe);
float sm_fep_compute_camp_from_precision(sm_fep_bridge_t* bridge, float precision);
float sm_fep_compute_ip3_from_complexity(sm_fep_bridge_t* bridge, float complexity);
int sm_fep_trigger_creb_from_efe(sm_fep_bridge_t* bridge, float efe);
float sm_fep_get_plasticity_modulation(const sm_fep_bridge_t* bridge);

int sm_fep_bridge_update(sm_fep_bridge_t* bridge, uint64_t delta_ms);
int sm_fep_bridge_get_stats(const sm_fep_bridge_t* bridge, sm_fep_stats_t* stats);

int sm_fep_bridge_connect_bio_async(sm_fep_bridge_t* bridge);
int sm_fep_bridge_disconnect_bio_async(sm_fep_bridge_t* bridge);
bool sm_fep_bridge_is_bio_async_connected(const sm_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECOND_MESSENGERS_FEP_BRIDGE_H */
