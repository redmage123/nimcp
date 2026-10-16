/**
 * @file nimcp_oligodendrocytes_sleep_bridge.h
 * @brief Sleep-Oligodendrocyte Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and oligodendrocytes
 * WHY:  Oligodendrocytes maintain myelin during sleep, repair damage, and modulate
 *       conduction velocity based on activity patterns
 * HOW:  Sleep state modulates OPC differentiation, myelin synthesis, and repair
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active myelin maintenance, minimal new synthesis
 * - DROWSY: Reduced activity, preparing for repair phase
 * - LIGHT_NREM: Beginning myelin synthesis, OPC activation
 * - DEEP_NREM: Peak myelin repair and synthesis, OPC differentiation
 * - REM: Moderate activity, selective maintenance of active circuits
 *
 * Key oligodendrocyte-sleep functions:
 * - Myelin synthesis: Primarily during deep NREM sleep
 * - OPC differentiation: Sleep-dependent maturation of progenitors
 * - Myelin repair: Damage repair accelerated during sleep
 * - Activity-dependent myelination: Sleep consolidates active pathways
 * - Metabolic support: Sleep enables energy-intensive myelin production
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OLIGODENDROCYTES_SLEEP_BRIDGE_H
#define NIMCP_OLIGODENDROCYTES_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "utils/thread/nimcp_thread.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Myelin synthesis rate modulation (relative to baseline) */
#define OLIGO_SLEEP_SYNTHESIS_AWAKE         0.2f   /* Low synthesis during activity */
#define OLIGO_SLEEP_SYNTHESIS_DROWSY        0.4f   /* Ramping up */
#define OLIGO_SLEEP_SYNTHESIS_LIGHT_NREM    0.7f   /* Moderate synthesis */
#define OLIGO_SLEEP_SYNTHESIS_DEEP_NREM     1.0f   /* Peak synthesis */
#define OLIGO_SLEEP_SYNTHESIS_REM           0.5f   /* Moderate during REM */

/* OPC differentiation rate modulation */
#define OLIGO_SLEEP_OPC_AWAKE               0.1f   /* Minimal differentiation while awake */
#define OLIGO_SLEEP_OPC_DROWSY              0.3f   /* Beginning to increase */
#define OLIGO_SLEEP_OPC_LIGHT_NREM          0.6f   /* Active differentiation */
#define OLIGO_SLEEP_OPC_DEEP_NREM           1.0f   /* Peak differentiation */
#define OLIGO_SLEEP_OPC_REM                 0.4f   /* Reduced during REM */

/* Myelin repair rate modulation */
#define OLIGO_SLEEP_REPAIR_AWAKE            0.3f   /* Basic maintenance only */
#define OLIGO_SLEEP_REPAIR_DROWSY           0.5f   /* Increasing */
#define OLIGO_SLEEP_REPAIR_LIGHT_NREM       0.7f   /* Active repair */
#define OLIGO_SLEEP_REPAIR_DEEP_NREM        1.0f   /* Peak repair activity */
#define OLIGO_SLEEP_REPAIR_REM              0.6f   /* Continued repair */

/* Metabolic support modulation */
#define OLIGO_SLEEP_METABOLIC_AWAKE         1.0f   /* Full metabolic support for active neurons */
#define OLIGO_SLEEP_METABOLIC_DROWSY        0.9f   /* Slightly reduced */
#define OLIGO_SLEEP_METABOLIC_LIGHT_NREM    0.7f   /* Redirected to synthesis */
#define OLIGO_SLEEP_METABOLIC_DEEP_NREM     0.5f   /* Minimal, focused on myelin */
#define OLIGO_SLEEP_METABOLIC_REM           0.8f   /* Supporting REM activity */

/**
 * WHAT: Configuration for oligodendrocyte-sleep integration
 * WHY:  Control sensitivity to sleep states
 * HOW:  Enable/disable modulation features
 */
typedef struct {
    bool enable_synthesis_modulation;       /**< Modulate myelin synthesis by sleep state */
    bool enable_opc_modulation;             /**< Modulate OPC differentiation */
    bool enable_repair_modulation;          /**< Modulate myelin repair rate */
    bool enable_metabolic_modulation;       /**< Modulate metabolic support */
    bool enable_activity_myelination;       /**< Activity-dependent myelination during sleep */
    float modulation_strength;              /**< Overall strength [0-1] */
    float repair_boost_multiplier;          /**< Repair boost during deep sleep [default: 3.0] */
    float synthesis_boost_multiplier;       /**< Synthesis boost during deep sleep [default: 5.0] */
} oligo_sleep_config_t;

/**
 * WHAT: Sleep-modulated oligodendrocyte effects
 * WHY:  Track how sleep state affects oligodendrocyte behavior
 * HOW:  Computed effects applied to oligodendrocyte parameters
 */
typedef struct {
    float myelin_synthesis_factor;         /**< Multiplier for synthesis rate */
    float opc_differentiation_factor;      /**< Multiplier for OPC maturation */
    float myelin_repair_factor;            /**< Multiplier for repair rate */
    float metabolic_support_factor;        /**< Metabolic support level */
    float activity_myelination_factor;     /**< Activity-dependent myelination */
    sleep_state_t current_state;           /**< Current sleep state */
    float sleep_pressure;                  /**< Current sleep pressure [0-1] */
    bool synthesis_active;                 /**< Peak synthesis mode active */
    bool repair_active;                    /**< Active repair in progress */
} oligo_sleep_effects_t;

/**
 * WHAT: Opaque handle to oligodendrocyte-sleep bridge
 * WHY:  Encapsulation of bridge implementation
 * HOW:  Pointer to internal structure
 */
typedef struct oligo_sleep_bridge_struct* oligo_sleep_bridge_t;

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get default oligodendrocyte-sleep configuration
 * WHY:  Sensible defaults for typical use
 * HOW:  Return pre-configured struct
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * DEFAULTS:
 * - All modulations enabled
 * - Modulation strength: 1.0 (full effect)
 * - Repair boost: 3.0x (typical during deep sleep)
 * - Synthesis boost: 5.0x (typical during deep sleep)
 */
int oligo_sleep_default_config(oligo_sleep_config_t* config);

/**
 * WHAT: Create oligodendrocyte-sleep bridge
 * WHY:  Initialize integration between sleep system and oligodendrocytes
 * HOW:  Allocate structures, register callbacks, set initial state
 *
 * @param config Configuration parameters (NULL for defaults)
 * @param sleep Sleep system handle
 * @return Bridge handle or NULL on failure
 */
oligo_sleep_bridge_t oligo_sleep_create(
    const oligo_sleep_config_t* config,
    sleep_system_t sleep);

/**
 * WHAT: Destroy oligodendrocyte-sleep bridge
 * WHY:  Free resources
 * HOW:  Unregister callbacks, free memory
 *
 * @param bridge Bridge handle (NULL-safe)
 */
void oligo_sleep_destroy(oligo_sleep_bridge_t bridge);

/* ========================================================================
 * MYELIN SYNTHESIS FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Enable myelin synthesis mode
 * WHY:  Activate synthesis during sleep
 * HOW:  Set synthesis factor based on sleep depth
 *
 * @param bridge Bridge handle
 * @return Current synthesis rate multiplier
 */
float oligo_sleep_enable_synthesis(oligo_sleep_bridge_t bridge);

/**
 * WHAT: Get current myelin synthesis rate
 * WHY:  Query synthesis efficiency
 * HOW:  Return synthesis multiplier based on sleep state
 *
 * @param bridge Bridge handle
 * @return Synthesis rate multiplier
 */
float oligo_sleep_get_synthesis_rate(const oligo_sleep_bridge_t bridge);

/**
 * WHAT: Check if synthesis is active
 * WHY:  Determine if in peak synthesis mode
 * HOW:  Check current sleep state and synthesis factor
 *
 * @param bridge Bridge handle
 * @return true if peak synthesis is active
 */
bool oligo_sleep_is_synthesis_active(const oligo_sleep_bridge_t bridge);

/* ========================================================================
 * OPC DIFFERENTIATION FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get OPC differentiation factor
 * WHY:  Query progenitor maturation rate
 * HOW:  Return differentiation factor based on sleep state
 *
 * @param bridge Bridge handle
 * @return OPC differentiation multiplier
 */
float oligo_sleep_get_opc_factor(const oligo_sleep_bridge_t bridge);

/**
 * WHAT: Initiate OPC differentiation
 * WHY:  Activate progenitor maturation during sleep
 * HOW:  Set differentiation factor based on sleep depth
 *
 * @param bridge Bridge handle
 * @return Current differentiation factor
 */
float oligo_sleep_initiate_opc_differentiation(oligo_sleep_bridge_t bridge);

/* ========================================================================
 * MYELIN REPAIR FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Enable myelin repair mode
 * WHY:  Activate damage repair during sleep
 * HOW:  Set repair factor based on sleep depth
 *
 * @param bridge Bridge handle
 * @return Current repair rate multiplier
 */
float oligo_sleep_enable_repair(oligo_sleep_bridge_t bridge);

/**
 * WHAT: Get myelin repair rate
 * WHY:  Query repair efficiency
 * HOW:  Return repair multiplier based on sleep state
 *
 * @param bridge Bridge handle
 * @return Repair rate multiplier
 */
float oligo_sleep_get_repair_rate(const oligo_sleep_bridge_t bridge);

/**
 * WHAT: Check if repair is active
 * WHY:  Determine if in active repair mode
 * HOW:  Check current sleep state and repair factor
 *
 * @param bridge Bridge handle
 * @return true if repair is active
 */
bool oligo_sleep_is_repair_active(const oligo_sleep_bridge_t bridge);

/* ========================================================================
 * STATE QUERY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get current sleep-modulated effects
 * WHY:  Query all oligodendrocyte modulations at once
 * HOW:  Copy current effects to output struct
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int oligo_sleep_get_effects(
    const oligo_sleep_bridge_t bridge,
    oligo_sleep_effects_t* effects);

/**
 * WHAT: Update bridge state for new timestep
 * WHY:  Evolve synthesis, repair, and modulations
 * HOW:  Called each simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int oligo_sleep_update(oligo_sleep_bridge_t bridge, float dt_ms);

/**
 * WHAT: Get metabolic support factor
 * WHY:  Query oligodendrocyte metabolic support level
 * HOW:  Return factor based on sleep state
 *
 * @param bridge Bridge handle
 * @return Metabolic support factor
 */
float oligo_sleep_get_metabolic_factor(const oligo_sleep_bridge_t bridge);

/* ========================================================================
 * BIO-ASYNC INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging
 * HOW:  Register with bio-async system
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int oligo_sleep_connect_bio_async(oligo_sleep_bridge_t bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown
 * HOW:  Unregister from bio-async system
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int oligo_sleep_disconnect_bio_async(oligo_sleep_bridge_t bridge);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection status
 * HOW:  Return enabled flag
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool oligo_sleep_is_bio_async_connected(const oligo_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLIGODENDROCYTES_SLEEP_BRIDGE_H */
