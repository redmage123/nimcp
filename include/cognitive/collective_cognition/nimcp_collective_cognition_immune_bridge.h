/**
 * @file nimcp_collective_cognition_immune_bridge.h
 * @brief Bridge between collective cognition and brain immune system
 *
 * WHAT: Coordinates immune responses across distributed brain instances
 * WHY:  Enables swarm-wide threat detection and collective immune memory
 * HOW:  Bridges collective cognition state to immune antigen presentation
 *
 * INTEGRATION POINTS:
 * - Collective Threats: Present threats detected by hyperscanning as antigens
 * - Inflammation Sharing: Propagate inflammation state via hyperscanning
 * - Memory Sync: Share immune memory cells across collective
 * - We-Mode Response: Coordinated immune response during we-mode
 *
 * THEORETICAL BASIS:
 * - Distributed immune memory across multi-agent systems
 * - Collective threat detection via neural synchronization
 * - Social immunity (Cremer et al., 2007)
 *
 * @author NIMCP Development Team
 * @date 2025-01-01
 */

#ifndef NIMCP_COLLECTIVE_COGNITION_IMMUNE_BRIDGE_H
#define NIMCP_COLLECTIVE_COGNITION_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct collective_cognition;
struct brain_immune_system;

typedef struct collective_cognition collective_cognition_t;
typedef struct brain_immune_system brain_immune_system_t;

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Collective threat types detected by hyperscanning
 */
typedef enum {
    COLLECTIVE_THREAT_DESYNC = 0,       /**< Synchronization loss */
    COLLECTIVE_THREAT_ROGUE_INSTANCE,   /**< Instance behaving abnormally */
    COLLECTIVE_THREAT_PHI_COLLAPSE,     /**< Sudden phi drop */
    COLLECTIVE_THREAT_WE_MODE_BREAK,    /**< We-mode disruption */
    COLLECTIVE_THREAT_EXTERNAL_ATTACK,  /**< Attack on cognitive extensions */
    COLLECTIVE_THREAT_BELIEF_CONFLICT,  /**< Irreconcilable belief conflict */
    COLLECTIVE_THREAT_COUNT
} collective_threat_type_t;

/**
 * @brief Collective threat descriptor
 */
typedef struct {
    collective_threat_type_t type;
    uint32_t source_instance_id;    /**< Instance that detected threat */
    uint32_t target_instance_id;    /**< Affected instance (0 = collective) */
    float severity;                 /**< Threat severity [0-1] */
    float confidence;               /**< Detection confidence [0-1] */
    uint64_t detection_time_us;     /**< When detected */
    char description[128];          /**< Human-readable description */
} collective_threat_t;

/**
 * @brief Collective immune bridge configuration
 */
typedef struct {
    bool enable_threat_sharing;         /**< Share threats across collective */
    bool enable_inflammation_sync;      /**< Sync inflammation via hyperscanning */
    bool enable_memory_propagation;     /**< Propagate immune memory */
    bool enable_we_mode_coordination;   /**< Coordinated response in we-mode */
    float threat_threshold;             /**< Min severity to trigger response */
    float sync_interval_ms;             /**< Inflammation sync interval */
} collective_immune_bridge_config_t;

/**
 * @brief Collective immune bridge statistics
 */
typedef struct {
    uint64_t threats_detected;
    uint64_t threats_shared;
    uint64_t antigens_presented;
    uint64_t inflammation_syncs;
    uint64_t memory_propagations;
    uint64_t we_mode_responses;
    float avg_threat_severity;
    float avg_response_time_ms;
} collective_immune_bridge_stats_t;

/**
 * @brief Collective immune bridge handle
 */
typedef struct collective_immune_bridge collective_immune_bridge_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
collective_immune_bridge_config_t collective_immune_bridge_default_config(void);

/**
 * @brief Create collective immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
collective_immune_bridge_t* collective_immune_bridge_create(
    const collective_immune_bridge_config_t* config
);

/**
 * @brief Destroy collective immune bridge
 *
 * @param bridge Bridge to destroy
 */
void collective_immune_bridge_destroy(collective_immune_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_reset(collective_immune_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to collective cognition system
 *
 * @param bridge Bridge handle
 * @param cc Collective cognition system
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_connect_collective_cognition(
    collective_immune_bridge_t* bridge,
    collective_cognition_t* cc
);

/**
 * @brief Connect to brain immune system
 *
 * @param bridge Bridge handle
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_connect_immune(
    collective_immune_bridge_t* bridge,
    brain_immune_system_t* immune
);

/*=============================================================================
 * Threat Detection API
 *===========================================================================*/

/**
 * @brief Report a collective threat
 *
 * @param bridge Bridge handle
 * @param threat Threat descriptor
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_report_threat(
    collective_immune_bridge_t* bridge,
    const collective_threat_t* threat
);

/**
 * @brief Check for collective threats
 *
 * Analyzes collective cognition state for threats.
 *
 * @param bridge Bridge handle
 * @return Number of threats detected
 */
uint32_t collective_immune_bridge_check_threats(
    collective_immune_bridge_t* bridge
);

/**
 * @brief Get pending threats
 *
 * @param bridge Bridge handle
 * @param threats Output array
 * @param max_threats Maximum threats to return
 * @return Number of threats returned
 */
uint32_t collective_immune_bridge_get_threats(
    const collective_immune_bridge_t* bridge,
    collective_threat_t* threats,
    uint32_t max_threats
);

/*=============================================================================
 * Immune Integration API
 *===========================================================================*/

/**
 * @brief Present collective threat as immune antigen
 *
 * @param bridge Bridge handle
 * @param threat Threat to present
 * @param antigen_id Output antigen ID
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_present_antigen(
    collective_immune_bridge_t* bridge,
    const collective_threat_t* threat,
    uint32_t* antigen_id
);

/**
 * @brief Sync inflammation state across collective
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_sync_inflammation(
    collective_immune_bridge_t* bridge
);

/**
 * @brief Propagate immune memory to collective
 *
 * @param bridge Bridge handle
 * @param b_cell_id B cell memory to propagate
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_propagate_memory(
    collective_immune_bridge_t* bridge,
    uint32_t b_cell_id
);

/*=============================================================================
 * We-Mode Response API
 *===========================================================================*/

/**
 * @brief Trigger coordinated immune response
 *
 * When in we-mode, all instances respond together.
 *
 * @param bridge Bridge handle
 * @param threat Triggering threat
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_we_mode_response(
    collective_immune_bridge_t* bridge,
    const collective_threat_t* threat
);

/*=============================================================================
 * Update and Query API
 *===========================================================================*/

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_update(collective_immune_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int collective_immune_bridge_get_stats(
    const collective_immune_bridge_t* bridge,
    collective_immune_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void collective_immune_bridge_reset_stats(collective_immune_bridge_t* bridge);

/*=============================================================================
 * Utility API
 *===========================================================================*/

/**
 * @brief Get threat type name
 *
 * @param type Threat type
 * @return Human-readable name
 */
const char* collective_threat_type_name(collective_threat_type_t type);

/**
 * @brief Dump bridge state for debugging
 *
 * @param bridge Bridge handle
 */
void collective_immune_bridge_dump(const collective_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_COGNITION_IMMUNE_BRIDGE_H */
