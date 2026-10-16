/**
 * @file nimcp_physics_intra_coordinator.h
 * @brief Physics Layer Intra-Layer Coordinator
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Coordinates communication between modules within the Physics layer
 * WHY:  Physics layer modules (Ephaptic, Info Geometry, HH, Thermo) must interact
 * HOW:  Manages intra-layer messaging, synchronization, and coherence
 *
 * PHYSICS LAYER MODULES:
 * ======================
 * - Ephaptic Coupling: Field effects between neurons
 * - Information Geometry: Geometric structure of neural spaces
 * - Hodgkin-Huxley Dynamics: Ion channel dynamics
 * - Thermodynamics: Energy and entropy considerations
 *
 * INTRA-LAYER CONNECTIONS:
 * ========================
 *   Ephaptic ←→ HH Dynamics (field affects ion channels)
 *   Ephaptic ←→ Info Geometry (field shapes information geometry)
 *   HH Dynamics ←→ Thermodynamics (energy from channel dynamics)
 *   Info Geometry ←→ Thermodynamics (entropy measures)
 *   All modules ←→ All modules (full mesh for physical coupling)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PHYSICS_INTRA_COORDINATOR_H
#define NIMCP_PHYSICS_INTRA_COORDINATOR_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Physics layer module IDs */
#define PHYSICS_MODULE_EPHAPTIC         0x0001
#define PHYSICS_MODULE_INFO_GEOMETRY    0x0002
#define PHYSICS_MODULE_HH_DYNAMICS      0x0003
#define PHYSICS_MODULE_THERMODYNAMICS   0x0004

/** Number of physics modules */
#define PHYSICS_MODULE_COUNT            4

/** Physics-specific message types (0x1000 + offset) */
#define PHYSICS_MSG_FIELD_UPDATE        (NIMCP_LAYER_MSG_MODULE_BASE + 0x0001)
#define PHYSICS_MSG_GEOMETRY_CHANGE     (NIMCP_LAYER_MSG_MODULE_BASE + 0x0002)
#define PHYSICS_MSG_CHANNEL_STATE       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0003)
#define PHYSICS_MSG_ENERGY_UPDATE       (NIMCP_LAYER_MSG_MODULE_BASE + 0x0004)
#define PHYSICS_MSG_ENTROPY_UPDATE      (NIMCP_LAYER_MSG_MODULE_BASE + 0x0005)
#define PHYSICS_MSG_CONSTRAINT_VIOLATION (NIMCP_LAYER_MSG_MODULE_BASE + 0x0006)

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque physics intra-layer coordinator handle
 */
typedef struct nimcp_physics_intra_struct* nimcp_physics_intra_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Physics intra-layer coordinator configuration
 */
typedef struct {
    /* Module enables */
    bool enable_ephaptic;           /**< Enable ephaptic coupling module */
    bool enable_info_geometry;      /**< Enable information geometry module */
    bool enable_hh_dynamics;        /**< Enable HH dynamics module */
    bool enable_thermodynamics;     /**< Enable thermodynamics module */

    /* Coupling strengths */
    float ephaptic_hh_coupling;     /**< Ephaptic ↔ HH coupling strength */
    float ephaptic_geometry_coupling; /**< Ephaptic ↔ Info Geometry coupling */
    float hh_thermo_coupling;       /**< HH ↔ Thermodynamics coupling */
    float geometry_thermo_coupling; /**< Info Geometry ↔ Thermodynamics coupling */

    /* Synchronization */
    uint32_t sync_interval_ms;      /**< Intra-layer sync interval */
    float coherence_threshold;      /**< Min coherence for stable operation */

    /* Physical constraints */
    bool enforce_energy_conservation; /**< Enforce energy conservation */
    bool enforce_entropy_increase;    /**< Enforce second law */
    float temperature_kelvin;         /**< Reference temperature */

    /* Logging/metrics */
    bool enable_logging;            /**< Enable coordinator logging */
    bool enable_metrics;            /**< Enable metrics collection */
} nimcp_physics_intra_config_t;

/**
 * @brief Physics layer state
 */
typedef struct {
    /* Module activity */
    bool ephaptic_active;
    bool info_geometry_active;
    bool hh_dynamics_active;
    bool thermodynamics_active;

    /* Physical state */
    float total_energy;             /**< Total energy in layer */
    float entropy;                  /**< Current entropy */
    float field_magnitude;          /**< Average field magnitude */
    float temperature;              /**< Effective temperature */

    /* Coherence */
    float layer_coherence;          /**< Overall layer coherence (0-1) */
    float module_coherences[PHYSICS_MODULE_COUNT]; /**< Per-module coherence */
} nimcp_physics_intra_state_t;

/**
 * @brief Physics layer statistics
 */
typedef struct {
    uint64_t messages_sent;         /**< Total intra-layer messages sent */
    uint64_t messages_received;     /**< Total messages received */
    uint64_t field_updates;         /**< Field update events */
    uint64_t channel_events;        /**< Ion channel events */
    uint64_t constraint_violations; /**< Physical constraint violations */
    uint64_t sync_events;           /**< Synchronization events */
    float avg_field_magnitude;      /**< Average field magnitude */
    float avg_energy;               /**< Average energy */
    float avg_entropy;              /**< Average entropy */
    float avg_coherence;            /**< Average coherence */
} nimcp_physics_intra_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT nimcp_physics_intra_config_t nimcp_physics_intra_default_config(void);

/**
 * @brief Create physics intra-layer coordinator
 *
 * @param config Configuration (NULL for defaults)
 * @return Coordinator handle or NULL on failure
 */
NIMCP_EXPORT nimcp_physics_intra_t nimcp_physics_intra_create(
    const nimcp_physics_intra_config_t* config
);

/**
 * @brief Destroy coordinator
 *
 * @param coord Coordinator to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_physics_intra_destroy(nimcp_physics_intra_t coord);

/**
 * @brief Initialize coordinator
 *
 * @param coord Coordinator handle
 * @param registry Layer registry for module registration
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_init(
    nimcp_physics_intra_t coord,
    nimcp_layer_registry_t registry
);

/**
 * @brief Shutdown coordinator
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_shutdown(
    nimcp_physics_intra_t coord
);

//=============================================================================
// Module Connection API
//=============================================================================

/**
 * @brief Connect ephaptic module
 *
 * @param coord Coordinator handle
 * @param module Ephaptic module instance
 * @param interface Module interface
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_connect_ephaptic(
    nimcp_physics_intra_t coord,
    void* module,
    nimcp_module_interface_t* interface
);

/**
 * @brief Connect information geometry module
 *
 * @param coord Coordinator handle
 * @param module Info geometry module instance
 * @param interface Module interface
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_connect_info_geometry(
    nimcp_physics_intra_t coord,
    void* module,
    nimcp_module_interface_t* interface
);

/**
 * @brief Connect Hodgkin-Huxley dynamics module
 *
 * @param coord Coordinator handle
 * @param module HH dynamics module instance
 * @param interface Module interface
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_connect_hh_dynamics(
    nimcp_physics_intra_t coord,
    void* module,
    nimcp_module_interface_t* interface
);

/**
 * @brief Connect thermodynamics module
 *
 * @param coord Coordinator handle
 * @param module Thermodynamics module instance
 * @param interface Module interface
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_connect_thermodynamics(
    nimcp_physics_intra_t coord,
    void* module,
    nimcp_module_interface_t* interface
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update all physics modules
 *
 * @param coord Coordinator handle
 * @param dt Delta time in seconds
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_update(
    nimcp_physics_intra_t coord,
    float dt
);

/**
 * @brief Synchronize physics modules
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_sync(
    nimcp_physics_intra_t coord
);

//=============================================================================
// Messaging API
//=============================================================================

/**
 * @brief Send message to specific module
 *
 * @param coord Coordinator handle
 * @param target_module Target module ID
 * @param msg Message to send
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_send(
    nimcp_physics_intra_t coord,
    uint32_t target_module,
    nimcp_layer_msg_t* msg
);

/**
 * @brief Broadcast message to all physics modules
 *
 * @param coord Coordinator handle
 * @param source_module Source module ID
 * @param msg Message to broadcast
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_broadcast(
    nimcp_physics_intra_t coord,
    uint32_t source_module,
    const nimcp_layer_msg_t* msg
);

//=============================================================================
// Physical Constraint API
//=============================================================================

/**
 * @brief Update total energy
 *
 * @param coord Coordinator handle
 * @param delta_energy Energy change
 * @return NIMCP_LAYER_OK or error (violation if conservation fails)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_update_energy(
    nimcp_physics_intra_t coord,
    float delta_energy
);

/**
 * @brief Update entropy
 *
 * @param coord Coordinator handle
 * @param delta_entropy Entropy change
 * @return NIMCP_LAYER_OK or error (violation if decrease)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_update_entropy(
    nimcp_physics_intra_t coord,
    float delta_entropy
);

/**
 * @brief Check physical constraints
 *
 * @param coord Coordinator handle
 * @param violations_out Output: number of violations
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_check_constraints(
    nimcp_physics_intra_t coord,
    uint32_t* violations_out
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get layer state
 *
 * @param coord Coordinator handle
 * @param state_out Output: layer state
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_get_state(
    nimcp_physics_intra_t coord,
    nimcp_physics_intra_state_t* state_out
);

/**
 * @brief Get layer statistics
 *
 * @param coord Coordinator handle
 * @param stats_out Output: statistics
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_get_stats(
    nimcp_physics_intra_t coord,
    nimcp_physics_intra_stats_t* stats_out
);

/**
 * @brief Get layer coherence
 *
 * @param coord Coordinator handle
 * @return Coherence (0-1) or -1 on error
 */
NIMCP_EXPORT float nimcp_physics_intra_get_coherence(
    nimcp_physics_intra_t coord
);

/**
 * @brief Reset statistics
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_physics_intra_reset_stats(
    nimcp_physics_intra_t coord
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_INTRA_COORDINATOR_H */
