/**
 * @file nimcp_physics_chemistry_bridge.h
 * @brief Physics-Chemistry Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges physical constraints to chemical processes
 * WHY:  Chemical reactions must obey thermodynamic laws
 * HOW:  Translates energy states to reaction kinetics
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Physics → Chemistry):
 * - Thermodynamic state → reaction rates
 * - Energy availability → synthesis capacity
 * - Temperature → enzyme kinetics
 *
 * Top-Down (Chemistry → Physics):
 * - Reaction heat → local temperature
 * - Concentration gradients → diffusion physics
 * - Bond energy → system enthalpy
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PHYSICS_CHEMISTRY_BRIDGE_H
#define NIMCP_PHYSICS_CHEMISTRY_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/physics/nimcp_physics_intra_coordinator.h"
#include "integration/intra/chemistry/nimcp_chemistry_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define PHYS_CHEM_MSG_ENERGY_STATE      (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0001)
#define PHYS_CHEM_MSG_TEMP_UPDATE       (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0002)
#define PHYS_CHEM_MSG_REACTION_HEAT     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0003)
#define PHYS_CHEM_MSG_DIFFUSION_REQ     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0004)
#define PHYS_CHEM_MSG_ENTHALPY_UPDATE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0005)

typedef struct nimcp_physics_chemistry_bridge_struct* nimcp_physics_chemistry_bridge_t;

typedef struct {
    float energy_coupling_strength;     /**< Physics energy → chemistry rate */
    float thermal_coupling_strength;    /**< Temperature effects on kinetics */
    float diffusion_coupling_strength;  /**< Concentration gradient physics */
    uint32_t update_interval_ms;
    bool enable_thermodynamic_constraints;
    bool enable_logging;
    bool enable_metrics;
} nimcp_physics_chemistry_config_t;

typedef struct {
    float current_temperature;
    float reaction_rate_modifier;
    float energy_available;
    float diffusion_rate;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_physics_chemistry_state_t;

typedef struct {
    uint64_t energy_state_transfers;
    uint64_t temperature_updates;
    uint64_t reaction_heat_events;
    uint64_t diffusion_requests;
    float avg_energy_transfer;
    float avg_thermal_coupling;
} nimcp_physics_chemistry_stats_t;

NIMCP_EXPORT nimcp_physics_chemistry_config_t nimcp_physics_chemistry_default_config(void);
NIMCP_EXPORT nimcp_physics_chemistry_bridge_t nimcp_physics_chemistry_create(const nimcp_physics_chemistry_config_t* config);
NIMCP_EXPORT void nimcp_physics_chemistry_destroy(nimcp_physics_chemistry_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_init(
    nimcp_physics_chemistry_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_physics_intra_t physics,
    nimcp_chemistry_intra_t chemistry
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_shutdown(nimcp_physics_chemistry_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_update(nimcp_physics_chemistry_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_transfer_bottom_up(nimcp_physics_chemistry_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_transfer_top_down(nimcp_physics_chemistry_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_get_state(nimcp_physics_chemistry_bridge_t bridge, nimcp_physics_chemistry_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_get_stats(nimcp_physics_chemistry_bridge_t bridge, nimcp_physics_chemistry_stats_t* stats_out);
NIMCP_EXPORT float nimcp_physics_chemistry_get_coherence(nimcp_physics_chemistry_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_chemistry_reset_stats(nimcp_physics_chemistry_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_CHEMISTRY_BRIDGE_H */
