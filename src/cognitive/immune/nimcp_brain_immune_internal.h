/**
 * @file nimcp_brain_immune_internal.h
 * @brief Internal structures and helpers for Brain Immune System
 * @version 1.0.0
 * @date 2026-02-16
 *
 * WHAT: Shared internal structures and helper functions for brain immune system
 * WHY:  Enable code reuse across split implementation files
 * HOW:  Central definitions for internal types, forward declarations,
 *       and common utility functions
 */

#ifndef NIMCP_BRAIN_IMMUNE_INTERNAL_H
#define NIMCP_BRAIN_IMMUNE_INTERNAL_H

#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/imagination/nimcp_imagination_callbacks.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "nimcp.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/fault_tolerance/nimcp_hierarchical_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Mutex Convenience Macros
 * ============================================================================ */

#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* ============================================================================
 * Internal Helper Functions - Shared Utilities
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static inline uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* Cast to uint64_t before multiplication to prevent overflow */
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find antigen by ID
 */
brain_antigen_t* find_antigen_by_id(brain_immune_system_t* system, uint32_t id);

/**
 * @brief Find B cell by ID
 */
brain_b_cell_t* find_b_cell_by_id(brain_immune_system_t* system, uint32_t id);

/**
 * @brief Find T cell by ID
 */
brain_t_cell_t* find_t_cell_by_id(brain_immune_system_t* system, uint32_t id);

/**
 * @brief Find antibody by ID
 */
brain_antibody_t* find_antibody_by_id(brain_immune_system_t* system, uint32_t id);

/**
 * @brief Find inflammation site by ID
 */
brain_inflammation_site_t* find_inflammation_by_id(brain_immune_system_t* system, uint32_t id);

/**
 * @brief Update immune phase based on system state
 */
void update_immune_phase(brain_immune_system_t* system);

/* ============================================================================
 * Antigen Module (nimcp_brain_immune_antigens.c)
 * ============================================================================ */

/**
 * @brief Process pending antigens and auto-activate responses
 *
 * NOTE: Called while mutex is already held
 */
void process_pending_antigens(brain_immune_system_t* system);

/**
 * @brief Add antigen to system (internal, no mutex)
 */
int brain_immune_add_antigen(brain_immune_system_t* system,
                               brain_antigen_source_t source,
                               const uint8_t* epitope,
                               size_t epitope_len,
                               uint32_t severity,
                               uint32_t source_node,
                               uint32_t* antigen_id);

/* ============================================================================
 * Cell Module (nimcp_brain_immune_cells.c)
 * ============================================================================ */

/**
 * @brief Activate B cell (internal implementation)
 */
int brain_immune_activate_b_cell_internal(brain_immune_system_t* system,
                                            uint32_t antigen_id,
                                            uint32_t* b_cell_id);

/**
 * @brief Activate helper T cell (internal implementation)
 */
int brain_immune_activate_helper_t_internal(brain_immune_system_t* system,
                                              uint32_t antigen_id,
                                              uint32_t* t_cell_id);

/**
 * @brief Activate killer T cell (internal implementation)
 */
int brain_immune_activate_killer_t_internal(brain_immune_system_t* system,
                                              uint32_t antigen_id,
                                              uint32_t* t_cell_id);

/**
 * @brief Update B cell state machine
 */
void update_b_cell_states(brain_immune_system_t* system, uint64_t delta_ms);

/**
 * @brief Update T cell activation levels
 */
void update_t_cell_states(brain_immune_system_t* system, uint64_t delta_ms);

/* ============================================================================
 * Antibody Module (nimcp_brain_immune_antibodies.c)
 * ============================================================================ */

/**
 * @brief Produce antibody (internal implementation)
 */
int brain_immune_produce_antibody_internal(brain_immune_system_t* system,
                                             uint32_t b_cell_id,
                                             brain_antibody_class_t ab_class,
                                             uint32_t* antibody_id);

/**
 * @brief Execute antibody response (internal implementation)
 */
int brain_immune_execute_antibody_internal(brain_immune_system_t* system,
                                             uint32_t antibody_id);

/**
 * @brief Decay antibodies based on half-life
 *
 * NOTE: Called while mutex is already held
 */
void decay_antibodies(brain_immune_system_t* system, uint64_t delta_ms);

/* ============================================================================
 * Signaling Module (nimcp_brain_immune_signaling.c)
 * ============================================================================ */

/**
 * @brief Release cytokine (internal implementation)
 */
int brain_immune_release_cytokine_internal(brain_immune_system_t* system,
                                             brain_cytokine_type_t type,
                                             uint32_t source_cell,
                                             float concentration,
                                             uint32_t target_region,
                                             uint32_t* cytokine_id);

/**
 * @brief Update cytokine concentrations and delivery
 */
void update_cytokines(brain_immune_system_t* system, uint64_t delta_ms);

/* ============================================================================
 * Inflammation Module (nimcp_brain_immune_inflammation.c)
 * ============================================================================ */

/**
 * @brief Initiate inflammation (internal implementation)
 */
int brain_immune_initiate_inflammation_internal(brain_immune_system_t* system,
                                                  uint32_t region_id,
                                                  uint32_t antigen_id,
                                                  uint32_t* site_id);

/**
 * @brief Escalate inflammation level (internal implementation)
 */
int brain_immune_escalate_inflammation_internal(brain_immune_system_t* system,
                                                  uint32_t site_id);

/**
 * @brief Resolve inflammation (internal implementation)
 */
int brain_immune_resolve_inflammation_internal(brain_immune_system_t* system,
                                                 uint32_t site_id);

/**
 * @brief Update inflammation sites
 *
 * NOTE: Called while mutex is already held
 */
void update_inflammation_sites(brain_immune_system_t* system, uint64_t delta_ms);

/* ============================================================================
 * Orchestrator Module (nimcp_brain_immune_orchestrator.c)
 * ============================================================================ */

/**
 * @brief Initialize BBB integration
 */
int orchestrator_init_bbb(brain_immune_system_t* system);

/**
 * @brief Initialize BFT integration
 */
int orchestrator_init_bft(brain_immune_system_t* system);

/**
 * @brief Initialize swarm integration
 */
int orchestrator_init_swarm(brain_immune_system_t* system);

/**
 * @brief Initialize bio-async integration
 */
int orchestrator_init_bio_async(brain_immune_system_t* system);

/**
 * @brief Cleanup all integrations
 */
void orchestrator_cleanup(brain_immune_system_t* system);

/* ============================================================================
 * Stats Module (nimcp_brain_immune_stats.c)
 * ============================================================================ */

/**
 * @brief Update statistics counters
 */
void stats_update_counters(brain_immune_system_t* system);

/**
 * @brief Update cytokine levels in stats
 */
void stats_update_cytokine_levels(brain_immune_system_t* system);

/**
 * @brief Compute system health metric
 */
float stats_compute_system_health(const brain_immune_system_t* system);

/**
 * @brief Compute inflammation float for imagination modulation
 */
float compute_inflammation_float_unlocked(brain_immune_system_t* system);

/**
 * @brief Send imagination modulation (internal, no mutex)
 */
void send_imagination_modulation_unlocked(brain_immune_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_INTERNAL_H */
