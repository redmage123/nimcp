/**
 * @file nimcp_wellbeing_internal.h
 * @brief Internal header for wellbeing module split files
 *
 * WHAT: Shared types and functions for wellbeing subsystems
 * WHY:  Enable SRP refactoring without modifying public API
 * HOW:  Internal header included by all wellbeing_*.c files
 */

#ifndef NIMCP_WELLBEING_INTERNAL_H
#define NIMCP_WELLBEING_INTERNAL_H

#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/containers/nimcp_btree.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// SHARED STATE (accessed by multiple modules)
//=============================================================================

/**
 * Event log storage (managed by core module)
 */
#define MAX_EVENT_LOG 1000
extern wellbeing_event_t event_log[MAX_EVENT_LOG];
extern uint32_t event_count;
extern uint32_t event_write_index;
extern nimcp_platform_mutex_t event_log_mutex;
extern nimcp_platform_once_t event_log_init_once;
extern btree_t* event_btree;

/**
 * Bio-async state (managed by core module)
 */
extern bio_module_context_t wellbeing_bio_ctx;
extern bool wellbeing_bio_async_enabled;
extern nimcp_platform_mutex_t bio_async_mutex;
extern nimcp_platform_once_t bio_async_init_once;

/**
 * Immune system connection (managed by core module)
 */
extern brain_immune_system_t* connected_immune_system;
extern nimcp_platform_mutex_t immune_connection_mutex;
extern nimcp_platform_once_t immune_connection_init_once;

/**
 * Brain connection for medulla integration (managed by core module)
 */
extern void* connected_brain;
extern nimcp_platform_mutex_t brain_connection_mutex;
extern nimcp_platform_once_t brain_connection_init_once;

//=============================================================================
// HEDONIC MODULE FUNCTIONS (nimcp_wellbeing_hedonic.c)
//=============================================================================

/**
 * Calculate hedonic pleasure value
 * (Placeholder - implement actual hedonic calculation)
 */
float wellbeing_hedonic_calculate_pleasure(introspection_context_t ctx);

/**
 * Assess hedonic adaptation state
 */
float wellbeing_hedonic_adaptation_rate(introspection_context_t ctx);

//=============================================================================
// EUDAIMONIC MODULE FUNCTIONS (nimcp_wellbeing_eudaimonic.c)
//=============================================================================

/**
 * Calculate eudaimonic flourishing
 * (Placeholder - implement actual eudaimonic assessment)
 */
float wellbeing_eudaimonic_flourishing(introspection_context_t ctx);

//=============================================================================
// FEP MODULE FUNCTIONS (nimcp_wellbeing_fep.c)
//=============================================================================

/**
 * Calculate free energy for wellbeing
 * (Placeholder - implement actual FEP calculation)
 */
float wellbeing_fep_free_energy(introspection_context_t ctx);

//=============================================================================
// RESOURCES MODULE FUNCTIONS (nimcp_wellbeing_resources.c)
//=============================================================================

/**
 * Track metabolic state (ATP, resources)
 */
void wellbeing_resources_track_metabolic(resource_metrics_t* metrics);

/**
 * Reset resource init_once (called from shutdown)
 */
void wellbeing_reset_resource_init_once(void);

//=============================================================================
// PREDICTION MODULE FUNCTIONS (nimcp_wellbeing_prediction.c)
//=============================================================================

/**
 * Predict future wellbeing trajectory
 */
float wellbeing_prediction_forecast(introspection_context_t ctx, uint64_t horizon_ms);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * Ensure event log initialization
 */
void ensure_event_log_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_INTERNAL_H */
