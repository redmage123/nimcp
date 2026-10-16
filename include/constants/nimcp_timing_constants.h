/**
 * @file nimcp_timing_constants.h
 * @brief Centralized timing constants for NIMCP
 * @version 1.0.0
 * @date 2025-02-03
 *
 * WHAT: Defines all timing-related constants used throughout the codebase
 * WHY:  Eliminates magic numbers, ensures consistency, enables easy tuning
 * HOW:  Single header with hierarchical organization by subsystem
 *
 * Usage: #include "constants/nimcp_timing_constants.h"
 */

#ifndef NIMCP_TIMING_CONSTANTS_H
#define NIMCP_TIMING_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * General Default Timeouts
 *===========================================================================*/

/** @brief Default timeout for general operations (5 seconds) */
#define NIMCP_DEFAULT_TIMEOUT_MS            5000

/** @brief Fast timeout for real-time operations (100ms) */
#define NIMCP_FAST_TIMEOUT_MS               100

/** @brief Short timeout for quick operations (500ms) */
#define NIMCP_SHORT_TIMEOUT_MS              500

/** @brief Medium timeout for moderate operations (1 second) */
#define NIMCP_MEDIUM_TIMEOUT_MS             1000

/** @brief Long timeout for extended operations (30 seconds) */
#define NIMCP_LONG_TIMEOUT_MS               30000

/** @brief Extended timeout for training/batch operations (5 minutes) */
#define NIMCP_EXTENDED_TIMEOUT_MS           300000

/*=============================================================================
 * Heartbeat Intervals
 *===========================================================================*/

/** @brief Default heartbeat interval for health monitoring (100ms) */
#define NIMCP_HEARTBEAT_INTERVAL_MS         100

/** @brief Fast heartbeat for critical systems (50ms) */
#define NIMCP_FAST_HEARTBEAT_MS             50

/** @brief Standard heartbeat for regular monitoring (1 second) */
#define NIMCP_STANDARD_HEARTBEAT_MS         1000

/** @brief Slow heartbeat for low-priority monitoring (5 seconds) */
#define NIMCP_SLOW_HEARTBEAT_MS             5000

/*=============================================================================
 * Consensus and Replication Timeouts
 *===========================================================================*/

/** @brief Consensus timeout for distributed operations (5 seconds) */
#define NIMCP_CONSENSUS_TIMEOUT_MS          5000

/** @brief Replication timeout for data sync (5 seconds) */
#define NIMCP_REPLICATION_TIMEOUT_MS        5000

/** @brief Vote timeout for distributed consensus (100ms) */
#define NIMCP_VOTE_TIMEOUT_MS               100

/** @brief Election timeout for leader election (500ms) */
#define NIMCP_ELECTION_TIMEOUT_MS           500

/*=============================================================================
 * Queue and Batch Timeouts
 *===========================================================================*/

/** @brief Default dequeue timeout (100ms) */
#define NIMCP_DEQUEUE_TIMEOUT_MS            100

/** @brief Batch processing timeout (50ms) */
#define NIMCP_BATCH_TIMEOUT_MS              50

/** @brief Event batch timeout for aggregation (100ms) */
#define NIMCP_EVENT_BATCH_TIMEOUT_MS        100

/*=============================================================================
 * Recovery and Fault Tolerance Timeouts
 *===========================================================================*/

/** @brief Default recovery timeout (1 second) */
#define NIMCP_RECOVERY_TIMEOUT_MS           1000

/** @brief Circuit breaker timeout (1 second) */
#define NIMCP_CIRCUIT_BREAKER_TIMEOUT_MS    1000

/** @brief Failure detection timeout (500ms) */
#define NIMCP_FAILURE_DETECTION_MS          500

/** @brief Watchdog timeout (10 seconds) */
#define NIMCP_WATCHDOG_TIMEOUT_MS           10000

/** @brief Hierarchical recovery timeouts per level */
#define NIMCP_RECOVERY_L0_TIMEOUT_MS        100     /**< Local recovery */
#define NIMCP_RECOVERY_L1_TIMEOUT_MS        500     /**< Module recovery */
#define NIMCP_RECOVERY_L2_TIMEOUT_MS        2000    /**< Subsystem recovery */
#define NIMCP_RECOVERY_L3_TIMEOUT_MS        10000   /**< System recovery */

/*=============================================================================
 * Neural Processing Timeouts
 *===========================================================================*/

/** @brief Attention timeout (100ms) */
#define NIMCP_ATTENTION_TIMEOUT_MS          100

/** @brief Competition timeout for global workspace (100ms) */
#define NIMCP_COMPETITION_TIMEOUT_MS        100

/** @brief Broadcast timeout for global workspace (500ms) */
#define NIMCP_BROADCAST_TIMEOUT_MS          500

/** @brief Encoding timeout for memory formation (5 seconds) */
#define NIMCP_ENCODING_TIMEOUT_MS           5000

/** @brief Retrieval timeout for memory recall (1 second) */
#define NIMCP_RETRIEVAL_TIMEOUT_MS          1000

/*=============================================================================
 * Security and Safety Timeouts
 *===========================================================================*/

/** @brief LGSS evaluation timeout (5 seconds) */
#define NIMCP_LGSS_TIMEOUT_MS               5000

/** @brief Override controller timeout (30 seconds) */
#define NIMCP_OVERRIDE_TIMEOUT_MS           30000

/** @brief Action interceptor timeout (5 seconds) */
#define NIMCP_ACTION_INTERCEPT_TIMEOUT_MS   5000

/** @brief TOCTOU token timeout (5 seconds) */
#define NIMCP_TOCTOU_TOKEN_TIMEOUT_MS       5000

/** @brief Safety verification timeout per check (5 seconds) */
#define NIMCP_SAFETY_CHECK_TIMEOUT_MS       5000

/*=============================================================================
 * Learning and Training Timeouts
 *===========================================================================*/

/** @brief Simulation timeout for imagination/planning (1 second) */
#define NIMCP_SIMULATION_TIMEOUT_MS         1000

/** @brief Proof search timeout (5 seconds) */
#define NIMCP_PROOF_TIMEOUT_MS              5000

/** @brief Component initialization timeout (5 seconds) */
#define NIMCP_COMPONENT_INIT_TIMEOUT_MS     5000

/** @brief Action selection timeout (100ms) */
#define NIMCP_ACTION_SELECTION_TIMEOUT_MS   100

/** @brief Learning update timeout (1 second) */
#define NIMCP_LEARNING_TIMEOUT_MS           1000

/*=============================================================================
 * I/O and Communication Timeouts
 *===========================================================================*/

/** @brief Connection establishment timeout (5 seconds) */
#define NIMCP_CONNECTION_TIMEOUT_MS         5000

/** @brief Message timeout for communication (1 second) */
#define NIMCP_MESSAGE_TIMEOUT_MS            1000

/** @brief Handshake timeout (5 seconds) */
#define NIMCP_HANDSHAKE_TIMEOUT_MS          5000

/** @brief Routing timeout (100ms) */
#define NIMCP_ROUTING_TIMEOUT_MS            100

/** @brief Synchronization timeout (100ms) */
#define NIMCP_SYNC_TIMEOUT_MS               100

/*=============================================================================
 * Database and Persistence Timeouts
 *===========================================================================*/

/** @brief Database connection timeout (5 seconds) */
#define NIMCP_DB_CONNECTION_TIMEOUT_MS      5000

/** @brief Database pool timeout (1 second) */
#define NIMCP_DB_POOL_TIMEOUT_MS            1000

/** @brief Write operation timeout (5 seconds) */
#define NIMCP_WRITE_TIMEOUT_MS              5000

/** @brief Read operation timeout (5 seconds) */
#define NIMCP_READ_TIMEOUT_MS               5000

/** @brief Failover timeout for disaster recovery (5 seconds) */
#define NIMCP_FAILOVER_TIMEOUT_MS           5000

/*=============================================================================
 * Interval Utilities
 * Note: Guard against redefinitions from nimcp_common.h
 *===========================================================================*/

/** @brief Convert milliseconds to microseconds */
#ifndef NIMCP_MS_TO_US
#define NIMCP_MS_TO_US(ms)                  ((ms) * 1000)
#endif

/** @brief Convert milliseconds to nanoseconds */
#ifndef NIMCP_MS_TO_NS
#define NIMCP_MS_TO_NS(ms)                  ((ms) * 1000000)
#endif

/** @brief Convert seconds to milliseconds */
#ifndef NIMCP_SEC_TO_MS
#define NIMCP_SEC_TO_MS(sec)                ((sec) * 1000)
#endif

/** @brief Microseconds per millisecond */
#ifndef NIMCP_US_PER_MS
#define NIMCP_US_PER_MS                     1000
#endif

/** @brief Nanoseconds per millisecond */
#ifndef NIMCP_NS_PER_MS
#define NIMCP_NS_PER_MS                     1000000
#endif

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TIMING_CONSTANTS_H */
