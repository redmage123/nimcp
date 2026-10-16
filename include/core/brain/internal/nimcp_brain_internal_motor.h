//=============================================================================
// nimcp_brain_internal_motor.h - Motor Cortex Internal Fields
//=============================================================================
/**
 * @file nimcp_brain_internal_motor.h
 * @brief Internal brain_struct fields for Motor Cortex (M1, premotor, SMA)
 *
 * WHAT: Defines brain_struct fields for Motor Cortex and integration bridges
 * WHY:  Modularize brain_internal.h - separate motor fields for maintainability
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Brodmann area 4 (primary motor cortex, M1)
 * - Brodmann area 6 (premotor and supplementary motor areas)
 * - Critical for voluntary movement planning and execution
 * - Damage causes motor deficits (hemiparesis, apraxia)
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of motor output
 * - Thalamic Bridge: VA/VL routing of motor commands
 * - Quantum Bridge: Trajectory optimization and program selection
 *
 * REFACTORING HISTORY:
 * - Extracted as part of brain header modularization (Phase M1)
 * - Part of ongoing brain header modularization effort
 *
 * @version Phase M1: Motor Cortex Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_MOTOR_H
#define NIMCP_BRAIN_INTERNAL_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations for Motor Types
//=============================================================================
// Full definitions are in core/brain/regions/motor/ headers

struct motor_adapter;              // Unified Motor Cortex adapter
struct motor_substrate_bridge;     // Metabolic modulation bridge
struct motor_thalamic_bridge;      // Thalamic signal routing bridge
struct motor_quantum_bridge;       // Quantum-accelerated trajectory bridge

//=============================================================================
// Motor Cortex Fields for brain_struct
//=============================================================================
/**
 * MOTOR CORTEX INTEGRATION (Movement Planning & Execution)
 *
 * Motor Cortex (BA4/6) provides motor planning and execution capabilities:
 * - Primary Motor Cortex (M1): Direct motor command generation
 * - Premotor Cortex: Movement preparation and sequencing
 * - Supplementary Motor Area: Internal movement initiation
 * - Motor Programs: Stored movement sequences (skills)
 *
 * The Motor adapter unifies:
 * - M1 Processor: Primary motor command generation
 * - Premotor Processor: Movement preparation and sequencing
 * - SMA Processor: Internally-generated movement planning
 *
 * Integrates with:
 * - Neural Substrate: Metabolic modulation of motor output
 * - Thalamic Router: Motor signal routing through VA/VL nuclei
 * - Quantum Reasoner: Trajectory optimization and program selection
 * - Basal Ganglia: Action selection (which movement to execute)
 * - Cerebellum: Motor coordination and timing
 * - Brain Immune System: Inflammation affects motor function
 * - Training System: Motor skill learning
 *
 * FIELD NAMING CONVENTION:
 * - motor_*: Core motor cortex components
 * - motor_*_bridge: Integration bridges to other subsystems
 * - motor_enabled: Master enable flag
 * - last_motor_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining Motor fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - motor: Core adapter for M1/premotor/SMA processing
 * - motor_substrate_bridge: Metabolic state integration (ATP, fatigue)
 * - motor_thalamic_bridge: Thalamic routing (VA/VL -> motor execution)
 * - motor_quantum_bridge: Quantum-accelerated trajectory planning
 * - motor_enabled: Master enable flag for motor subsystem
 * - last_motor_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_MOTOR                                            \
    /* === MOTOR CORTEX INTEGRATION (Movement Planning & Execution) === */     \
    struct motor_adapter* motor;                   /* Motor cortex adapter */  \
    struct motor_substrate_bridge* motor_substrate_bridge;  /* Substrate metabolic integration */ \
    struct motor_thalamic_bridge* motor_thalamic_bridge;    /* Thalamic signal routing */ \
    struct motor_quantum_bridge* motor_quantum_bridge;      /* Quantum-accelerated trajectory */ \
    bool motor_enabled;                            /* Motor cortex enabled for this brain */ \
    uint64_t last_motor_update_us;                 /* Last motor update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_MOTOR_H */
