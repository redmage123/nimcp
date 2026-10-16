//=============================================================================
// nimcp_financial_working_memory_bridge.h - Financial Working Memory Bridge
//=============================================================================
/**
 * @file nimcp_financial_working_memory_bridge.h
 * @brief Working memory bridge specialized for financial data management
 *
 * WHAT: Bridges financial trading data to working memory subsystem with
 *       salience-based eviction and capacity limits (Miller's 7+/-2).
 *
 * WHY:  Traders/systems need to hold limited high-priority financial
 *       information in active memory. This bridge manages:
 *       - Capacity-limited storage (7 slots default)
 *       - Salience-based retention and eviction
 *       - Temporal decay for item relevance
 *       - Type-based retrieval for financial decisions
 *
 * HOW:  Circular buffer with salience-ordered eviction. Items decay over
 *       time; refreshing boosts salience. Integrates with immune/BBB for
 *       security validation, KG wiring for messaging, health agent for
 *       heartbeat monitoring.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_FINANCIAL_WORKING_MEMORY_BRIDGE_H
#define NIMCP_FINANCIAL_WORKING_MEMORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define FIN_WM_CAPACITY 7  /* Miller's 7+/-2: default working memory slots */
#define FIN_WM_MAX_CAPACITY 12  /* Maximum configurable capacity */

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_WM_ERR_OK           0
#define FIN_WM_ERR_NULL        -1
#define FIN_WM_ERR_MEMORY      -2
#define FIN_WM_ERR_VALIDATION  -3
#define FIN_WM_ERR_FULL        -4
#define FIN_WM_ERR_NOT_FOUND   -5
#define FIN_WM_ERR_SUBSYSTEM   -6
#define FIN_WM_ERROR_BASE      -3100

//=============================================================================
// Forward Declarations
//=============================================================================

/* Main bridge structure */
struct financial_wm_bridge;
typedef struct financial_wm_bridge financial_wm_bridge_t;

/* Brain immune system */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

/* Blood-brain barrier */
struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;

/* Knowledge graph wiring */
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* Bio-async context */
struct bio_async_context;
typedef struct bio_async_context bio_async_context_t;

/* Health agent */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* Working memory system */
struct working_memory;
typedef struct working_memory working_memory_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Types of items that can be stored in financial working memory
 */
typedef enum {
    FIN_WM_ITEM_PRICE,      /**< Price update/level */
    FIN_WM_ITEM_SIGNAL,     /**< Trading signal */
    FIN_WM_ITEM_NEWS,       /**< News event */
    FIN_WM_ITEM_POSITION,   /**< Position information */
    FIN_WM_ITEM_RISK_ALERT, /**< Risk warning */
    FIN_WM_ITEM_PATTERN,    /**< Detected pattern */
    FIN_WM_ITEM_DECISION,   /**< Pending decision */
    FIN_WM_ITEM_TYPE_COUNT
} fin_wm_item_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Financial working memory item
 *
 * WHAT: Single item stored in working memory
 * WHY:  Hold financial information with salience for prioritization
 * HOW:  Type-tagged data with salience score and timestamp
 */
typedef struct {
    fin_wm_item_type_t type;    /**< Item type for categorization */
    float salience;             /**< Salience/importance score [0,1] */
    uint64_t timestamp_ms;      /**< Creation/update timestamp */
    float data[16];             /**< Generic data storage (prices, signals, etc.) */
    char label[64];             /**< Human-readable label */
} fin_wm_item_t;

/**
 * @brief Statistics for working memory bridge operations
 */
typedef struct {
    uint64_t items_added;       /**< Total items added */
    uint64_t items_evicted;     /**< Items evicted due to capacity */
    uint64_t queries;           /**< Total retrieval queries */
    uint64_t refreshes;         /**< Item refresh operations */
    uint64_t immune_checks;     /**< Immune validation calls */
    uint64_t bbb_validations;   /**< BBB validation calls */
    uint64_t kg_messages_sent;  /**< KG messages published */
    uint64_t health_heartbeats; /**< Health heartbeats sent */
} fin_wm_bridge_stats_t;

/**
 * @brief Configuration for financial working memory bridge
 */
typedef struct {
    uint32_t capacity;                  /**< Working memory capacity (default: 7) */
    float decay_rate;                   /**< Salience decay rate per second */
    bool enable_immune_validation;      /**< Enable immune system checks */
    bool enable_bbb_validation;         /**< Enable BBB validation */
} fin_wm_bridge_config_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a financial working memory bridge
 * @param bridge Output pointer for created bridge
 * @param config Configuration (NULL for defaults)
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_create(financial_wm_bridge_t** bridge,
                               const fin_wm_bridge_config_t* config);

/**
 * @brief Destroy a financial working memory bridge
 * @param bridge Bridge to destroy (NULL-safe)
 */
void financial_wm_bridge_destroy(financial_wm_bridge_t* bridge);

/**
 * @brief Fill config with default values
 * @param config Configuration structure to fill
 */
void financial_wm_bridge_default_config(fin_wm_bridge_config_t* config);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Set the underlying working memory system
 * @param bridge Financial WM bridge
 * @param wm Working memory system to integrate with
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_set_working_memory(financial_wm_bridge_t* bridge,
                                            working_memory_t* wm);

/**
 * @brief Add an item to working memory
 * @param bridge Financial WM bridge
 * @param item Item to add (copied)
 * @return FIN_WM_ERR_OK on success, FIN_WM_ERR_FULL if eviction fails
 *
 * If capacity is reached, the lowest-salience item is evicted.
 */
int financial_wm_bridge_add(financial_wm_bridge_t* bridge,
                            const fin_wm_item_t* item);

/**
 * @brief Get all active items in working memory
 * @param bridge Financial WM bridge
 * @param items Output array (must hold capacity items)
 * @param count Output: number of active items
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_get_active(financial_wm_bridge_t* bridge,
                                    fin_wm_item_t* items,
                                    uint32_t* count);

/**
 * @brief Apply temporal decay to all items
 * @param bridge Financial WM bridge
 * @param dt_sec Time elapsed in seconds
 * @return FIN_WM_ERR_OK on success
 *
 * Items with salience decayed to 0 are removed.
 */
int financial_wm_bridge_decay_step(financial_wm_bridge_t* bridge, float dt_sec);

/**
 * @brief Refresh an item to boost its salience
 * @param bridge Financial WM bridge
 * @param item_index Index of item to refresh
 * @return FIN_WM_ERR_OK on success, FIN_WM_ERR_NOT_FOUND if invalid index
 */
int financial_wm_bridge_refresh(financial_wm_bridge_t* bridge, uint32_t item_index);

/**
 * @brief Get items of a specific type
 * @param bridge Financial WM bridge
 * @param type Type of items to retrieve
 * @param items Output array
 * @param count In: max items to retrieve, Out: actual count
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_get_by_type(financial_wm_bridge_t* bridge,
                                     fin_wm_item_type_t type,
                                     fin_wm_item_t* items,
                                     uint32_t* count);

/**
 * @brief Clear all items from working memory
 * @param bridge Financial WM bridge
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_clear(financial_wm_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

/**
 * @brief Set brain immune system for validation
 * @param bridge Financial WM bridge
 * @param immune Immune system instance
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_set_immune(financial_wm_bridge_t* bridge,
                                    brain_immune_system_t* immune);

/**
 * @brief Set BBB system for data validation
 * @param bridge Financial WM bridge
 * @param bbb BBB system instance
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_set_bbb(financial_wm_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set KG wiring for messaging
 * @param bridge Financial WM bridge
 * @param kg KG wiring instance
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_set_kg_wiring(financial_wm_bridge_t* bridge, kg_wiring_t* kg);

/**
 * @brief Set health agent for monitoring
 * @param bridge Financial WM bridge
 * @param agent Health agent instance
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_set_health_agent(financial_wm_bridge_t* bridge,
                                          nimcp_health_agent_t* agent);

/**
 * @brief Set logger for bridge operations
 * @param bridge Financial WM bridge
 * @param logger Logger instance (void* for flexibility)
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_set_logger(financial_wm_bridge_t* bridge, void* logger);

/**
 * @brief Set bio-async context for async operations
 * @param bridge Financial WM bridge
 * @param ctx Bio-async context
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_set_bio_async(financial_wm_bridge_t* bridge,
                                       bio_async_context_t* ctx);

//=============================================================================
// Statistics & Diagnostics
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Financial WM bridge
 * @param stats Output statistics structure
 * @return FIN_WM_ERR_OK on success
 */
int financial_wm_bridge_get_stats(const financial_wm_bridge_t* bridge,
                                   fin_wm_bridge_stats_t* stats);

/**
 * @brief Reset statistics counters
 * @param bridge Financial WM bridge
 */
void financial_wm_bridge_reset_stats(financial_wm_bridge_t* bridge);

/**
 * @brief Get last error message (thread-local)
 * @return Error message string
 */
const char* financial_wm_bridge_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_WORKING_MEMORY_BRIDGE_H */
