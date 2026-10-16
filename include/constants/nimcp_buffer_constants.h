/**
 * @file nimcp_buffer_constants.h
 * @brief Centralized buffer size constants for NIMCP
 * @version 1.0.0
 * @date 2025-02-03
 *
 * WHAT: Defines all buffer size constants used throughout the codebase
 * WHY:  Eliminates magic numbers, prevents buffer overflows, ensures consistency
 * HOW:  Single header with hierarchical organization by buffer purpose
 *
 * Usage: #include "constants/nimcp_buffer_constants.h"
 */

#ifndef NIMCP_BUFFER_CONSTANTS_H
#define NIMCP_BUFFER_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Path Buffer Sizes
 *===========================================================================*/

/** @brief Maximum path length for file operations */
#ifndef NIMCP_PATH_BUFFER_SIZE
#define NIMCP_PATH_BUFFER_SIZE              4096
#endif

/** @brief Maximum path length for short paths */
#define NIMCP_SHORT_PATH_SIZE               256

/** @brief Maximum path length for metrics/config paths */
#define NIMCP_METRICS_PATH_SIZE             512

/** @brief Source cache maximum path length */
#define NIMCP_SOURCE_CACHE_PATH_SIZE        4096

/** @brief DWARF symbols maximum path length */
#define NIMCP_DWARF_PATH_SIZE               1024

/** @brief VCS integration maximum path length */
#define NIMCP_VCS_PATH_SIZE                 1024

/** @brief Recompiler maximum path length */
#define NIMCP_RECOMPILER_PATH_SIZE          4096

/** @brief LGSS maximum path length */
#define NIMCP_LGSS_PATH_SIZE                512

/** @brief Wiring diagram maximum path length */
#define NIMCP_WIRING_PATH_SIZE              512

/** @brief KG persistence maximum path length */
#define NIMCP_KG_PERSIST_PATH_SIZE          512

/** @brief KG disaster recovery maximum path length */
#define NIMCP_KG_DR_PATH_SIZE               512

/** @brief KG observability maximum path length */
#define NIMCP_KG_OBS_PATH_SIZE              256

/** @brief Immune persistence maximum path length */
#define NIMCP_IMMUNE_PERSIST_PATH_SIZE      512

/** @brief Code immune persistence maximum path length */
#define NIMCP_CODE_IMMUNE_PATH_SIZE         512

/*=============================================================================
 * Error and Message Buffer Sizes
 *===========================================================================*/

/** @brief Standard error buffer size */
#define NIMCP_ERROR_BUFFER_SIZE             256

/** @brief Large error buffer for detailed messages */
#define NIMCP_ERROR_BUFFER_LARGE            512

/** @brief Small error buffer for constrained platforms */
#define NIMCP_ERROR_BUFFER_SMALL            64

/** @brief Medium error buffer for most cases */
#define NIMCP_ERROR_BUFFER_MEDIUM           128

/** @brief Log message buffer size */
#define NIMCP_LOG_BUFFER_SIZE               1024

/** @brief Debug message buffer size */
#define NIMCP_DEBUG_BUFFER_SIZE             2048

/*=============================================================================
 * Command and Line Buffer Sizes
 *===========================================================================*/

/** @brief Command buffer size for system commands */
#define NIMCP_CMD_BUFFER_SIZE               1024

/** @brief Line buffer for file I/O */
#define NIMCP_LINE_BUFFER_SIZE              4096

/** @brief Shell command buffer size */
#define NIMCP_SHELL_CMD_SIZE                512

/** @brief JSON string buffer size */
#define NIMCP_JSON_BUFFER_SIZE              2048

/*=============================================================================
 * Name and Identifier Buffer Sizes
 *===========================================================================*/

/** @brief Maximum name length for entities (may be overridden by tier_optimization.h) */
#ifndef NIMCP_NAME_BUFFER_SIZE
#define NIMCP_NAME_BUFFER_SIZE              256
#endif

/** @brief Maximum identifier length */
#define NIMCP_ID_BUFFER_SIZE                64

/** @brief Maximum label length */
#define NIMCP_LABEL_BUFFER_SIZE             128

/** @brief Context key size for logging */
#define NIMCP_CONTEXT_KEY_SIZE              32

/*=============================================================================
 * Data Structure Capacities
 *===========================================================================*/

/** @brief Default hash table size */
#define NIMCP_HASH_TABLE_SIZE               256

/** @brief Large hash table size */
#define NIMCP_HASH_TABLE_LARGE              1024

/** @brief Small hash table size */
#define NIMCP_HASH_TABLE_SMALL              64

/** @brief Default array capacity */
#define NIMCP_ARRAY_CAPACITY                256

/** @brief Large array capacity */
#define NIMCP_ARRAY_CAPACITY_LARGE          1024

/** @brief Small array capacity */
#define NIMCP_ARRAY_CAPACITY_SMALL          64

/** @brief Maximum pending operations in queues */
#define NIMCP_MAX_PENDING                   256

/** @brief Default queue capacity */
#define NIMCP_QUEUE_CAPACITY                1024

/*=============================================================================
 * Event and Signal Buffer Sizes
 *===========================================================================*/

/** @brief Spike buffer size for SNN */
#define NIMCP_SPIKE_BUFFER_SIZE             1024

/** @brief Eligibility trace table size */
#define NIMCP_ELIGIBILITY_TABLE_SIZE        512

/** @brief Event queue high priority size */
#define NIMCP_EVENT_QUEUE_HIGH_SIZE         1000

/** @brief Event queue normal priority size */
#define NIMCP_EVENT_QUEUE_NORMAL_SIZE       5000

/** @brief Event queue low priority size */
#define NIMCP_EVENT_QUEUE_LOW_SIZE          500

/** @brief Simulation queue size */
#define NIMCP_SIM_QUEUE_SIZE                256

/*=============================================================================
 * History and Record Buffer Sizes
 *===========================================================================*/

/** @brief Error history buffer size */
#define NIMCP_ERROR_HISTORY_SIZE            256

/** @brief Metrics history buffer size */
#define NIMCP_METRICS_HISTORY_SIZE          128

/** @brief Checkpoint history size */
#define NIMCP_CHECKPOINT_HISTORY_SIZE       100

/** @brief Morphogenesis history size */
#define NIMCP_MORPH_HISTORY_SIZE            100

/** @brief Health records buffer size */
#define NIMCP_HEALTH_RECORDS_SIZE           512

/*=============================================================================
 * Network and Communication Buffer Sizes
 *===========================================================================*/

/** @brief LoRa header size */
#define NIMCP_LORA_HEADER_SIZE              4

/** @brief LoRa CRC size */
#define NIMCP_LORA_CRC_SIZE                 2

/** @brief Feature map initial capacity */
#define NIMCP_FEATURE_MAP_CAPACITY          256

/** @brief Maximum modules in registry */
#define NIMCP_MAX_MODULES                   512

/** @brief Max paths per routing source */
#define NIMCP_MAX_PATHS_PER_SOURCE          64

/*=============================================================================
 * Swarm and Flocking Buffer Sizes
 *===========================================================================*/

/** @brief Default boid capacity for flocking */
#define NIMCP_BOID_CAPACITY                 1000

/** @brief Default obstacle capacity */
#define NIMCP_OBSTACLE_CAPACITY             100

/** @brief Default neighbor capacity */
#define NIMCP_NEIGHBOR_CAPACITY             20

/** @brief Default morphogenesis grid size */
#define NIMCP_MORPH_GRID_SIZE               64

/*=============================================================================
 * Neuron and Neural Buffer Sizes
 *===========================================================================*/

/** @brief Initial neuron capacity for ephaptic processing */
#define NIMCP_INITIAL_NEURON_CAPACITY       256

/** @brief Default hypercolumn count */
#define NIMCP_DEFAULT_HYPERCOLUMNS          64

/** @brief Ambient dimension for information geometry */
#define NIMCP_AMBIENT_DIMENSION             256

/*=============================================================================
 * Platform-Tier Specific Sizes
 *===========================================================================*/

/** @brief Error buffer for constrained platforms */
#define NIMCP_CONSTRAINED_ERROR_BUFFER      64

/** @brief Error buffer for medium platforms */
#define NIMCP_MEDIUM_ERROR_BUFFER           128

/** @brief Error buffer for full platforms */
#define NIMCP_FULL_ERROR_BUFFER             256

/*=============================================================================
 * Size Validation Macros
 *===========================================================================*/

/** @brief Check if buffer size is sufficient */
#define NIMCP_BUFFER_CHECK(actual, required) ((actual) >= (required))

/** @brief Safe copy with null termination */
#ifndef NIMCP_SAFE_STRCPY
#define NIMCP_SAFE_STRCPY(dst, src, size) \
    do { \
        strncpy((dst), (src), (size) - 1); \
        (dst)[(size) - 1] = '\0'; \
    } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BUFFER_CONSTANTS_H */
