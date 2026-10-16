/**
 * @file nimcp_security_fractal.h
 * @brief Fractal-Based Security Enhancement
 *
 * WHAT: Applies fractal patterns to security verification, creating
 *       self-similar hierarchical defense structures.
 *
 * WHY:  Fractal security provides:
 *       - Hierarchical integrity verification (like Merkle trees but fractal)
 *       - Anomaly detection through fractal dimension analysis
 *       - Self-similar defense layers at multiple scales
 *       - Biological parallels to immune system's fractal organization
 *
 * HOW:  Integrates with existing nimcp_fractal_topology to create:
 *       - Fractal hash trees for integrity verification
 *       - Scale-invariant anomaly detection
 *       - Hierarchical trust propagation
 *
 * BIOLOGICAL MOTIVATION:
 * - Immune system exhibits fractal organization (Burroughs, 2015)
 * - Fractal vigilance patterns optimize coverage (Viswanathan, 1999)
 * - Self-similar structures provide redundant protection
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_SECURITY_FRACTAL_H
#define NIMCP_SECURITY_FRACTAL_H

#include "security/nimcp_security.h"
#include "security/nimcp_security_coverage.h"
#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum fractal hierarchy depth */
#define NIMCP_FSC_MAX_DEPTH 8

/** Maximum nodes per level */
#define NIMCP_FSC_MAX_NODES_PER_LEVEL 1024

/** Hash size in bytes */
#define NIMCP_FSC_HASH_SIZE 32

/** Default fractal dimension for security structures */
#define NIMCP_FSC_DEFAULT_DIMENSION 2.0f

/** Anomaly threshold (deviation from expected dimension) */
#define NIMCP_FSC_ANOMALY_THRESHOLD 0.15f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Fractal security node types
 */
typedef enum {
    NIMCP_FSC_NODE_ROOT = 0,     /**< Root of fractal tree */
    NIMCP_FSC_NODE_BRANCH,       /**< Internal branch node */
    NIMCP_FSC_NODE_LEAF,         /**< Leaf node (actual data) */
    NIMCP_FSC_NODE_GUARDIAN      /**< Special security sentinel */
} nimcp_fsc_node_type_t;

/**
 * @brief Fractal security verification result
 */
typedef enum {
    NIMCP_FSC_INTACT = 0,        /**< Structure intact */
    NIMCP_FSC_HASH_MISMATCH,     /**< Hash verification failed */
    NIMCP_FSC_DIMENSION_ANOMALY, /**< Fractal dimension anomaly */
    NIMCP_FSC_STRUCTURE_CORRUPT, /**< Structure corruption detected */
    NIMCP_FSC_GUARDIAN_ALERT,    /**< Guardian node triggered */
    NIMCP_FSC_PROPAGATION_FAIL   /**< Trust propagation failed */
} nimcp_fsc_result_t;

/**
 * @brief Anomaly detection mode
 */
typedef enum {
    NIMCP_FSC_DETECT_STRUCTURAL, /**< Structural anomalies only */
    NIMCP_FSC_DETECT_TEMPORAL,   /**< Temporal pattern anomalies */
    NIMCP_FSC_DETECT_HYBRID      /**< Both structural and temporal */
} nimcp_fsc_detect_mode_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Fractal security node
 */
typedef struct nimcp_fsc_node {
    nimcp_fsc_node_type_t type;          /**< Node type */
    uint32_t level;                       /**< Hierarchy level (0 = root) */
    uint32_t index;                       /**< Index within level */
    uint8_t hash[NIMCP_FSC_HASH_SIZE];   /**< Node hash */
    void* protected_data;                 /**< Protected resource pointer */
    size_t data_size;                     /**< Size of protected data */
    struct nimcp_fsc_node* parent;        /**< Parent node */
    struct nimcp_fsc_node** children;     /**< Child nodes array */
    uint32_t num_children;                /**< Number of children */
    uint32_t max_children;                /**< Branching factor */
    float local_dimension;                /**< Local fractal dimension */
    uint64_t last_verified;               /**< Last verification timestamp */
    uint64_t access_count;                /**< Access counter */
    bool verified;                        /**< Current verification status */
} nimcp_fsc_node_t;

/**
 * @brief Fractal security configuration
 */
typedef struct {
    float fractal_dimension;              /**< Target fractal dimension */
    uint32_t hierarchy_depth;             /**< Number of levels */
    uint32_t branching_factor;            /**< Children per node */
    float anomaly_threshold;              /**< Deviation threshold */
    nimcp_fsc_detect_mode_t detect_mode;  /**< Detection mode */
    bool enable_guardians;                /**< Enable sentinel nodes */
    uint32_t guardian_interval;           /**< Guardian placement interval */
    uint64_t verification_interval_ms;    /**< Auto-verify interval */
} nimcp_fsc_config_t;

/**
 * @brief Fractal security statistics
 */
typedef struct {
    uint32_t total_nodes;                 /**< Total nodes in tree */
    uint32_t leaf_nodes;                  /**< Leaf node count */
    uint32_t guardian_nodes;              /**< Guardian node count */
    uint32_t current_depth;               /**< Current tree depth */
    float measured_dimension;             /**< Measured fractal dimension */
    uint64_t verifications;               /**< Total verifications */
    uint64_t hash_mismatches;             /**< Hash mismatches detected */
    uint64_t dimension_anomalies;         /**< Dimension anomalies detected */
    uint64_t guardian_alerts;             /**< Guardian alerts triggered */
    uint64_t integrity_repairs;           /**< Auto-repairs performed */
    float coverage_ratio;                 /**< Security coverage ratio */
} nimcp_fsc_stats_t;

/**
 * @brief Fractal anomaly event
 */
typedef struct {
    nimcp_fsc_result_t type;              /**< Anomaly type */
    uint32_t level;                       /**< Level where detected */
    uint32_t node_index;                  /**< Node index */
    float expected_dimension;             /**< Expected dimension */
    float actual_dimension;               /**< Actual dimension */
    uint64_t timestamp;                   /**< Detection time */
    void* affected_data;                  /**< Affected data pointer */
} nimcp_fsc_anomaly_t;

/**
 * @brief Anomaly callback type
 */
typedef void (*nimcp_fsc_anomaly_callback_t)(
    const nimcp_fsc_anomaly_t* anomaly,
    void* user_data
);

/**
 * @brief Fractal security context (opaque handle)
 */
typedef struct nimcp_fractal_security nimcp_fractal_security_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create fractal security system
 *
 * @return Fractal security context or NULL on failure
 */
nimcp_fractal_security_t* nimcp_fractal_security_create(void);

/**
 * @brief Initialize with configuration
 *
 * @param fsc Fractal security context
 * @param config Configuration parameters
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_init(
    nimcp_fractal_security_t* fsc,
    const nimcp_fsc_config_t* config
);

/**
 * @brief Destroy fractal security system
 *
 * @param fsc Fractal security context
 */
void nimcp_fractal_security_destroy(nimcp_fractal_security_t* fsc);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
nimcp_fsc_config_t nimcp_fractal_security_default_config(void);

//=============================================================================
// Resource Protection
//=============================================================================

/**
 * @brief Register data for fractal protection
 *
 * Data is added to the fractal tree with appropriate hashing.
 *
 * @param fsc Fractal security context
 * @param data Pointer to data to protect
 * @param size Size of data
 * @param node_out Output: assigned node (can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_protect(
    nimcp_fractal_security_t* fsc,
    void* data,
    size_t size,
    nimcp_fsc_node_t** node_out
);

/**
 * @brief Unprotect and remove data from tree
 *
 * @param fsc Fractal security context
 * @param data Pointer to data to unprotect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_unprotect(
    nimcp_fractal_security_t* fsc,
    void* data
);

/**
 * @brief Update hash for modified data
 *
 * Call after legitimately modifying protected data.
 *
 * @param fsc Fractal security context
 * @param data Pointer to modified data
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_update_hash(
    nimcp_fractal_security_t* fsc,
    void* data
);

//=============================================================================
// Verification Functions
//=============================================================================

/**
 * @brief Verify specific data integrity
 *
 * @param fsc Fractal security context
 * @param data Data to verify
 * @return Verification result
 */
nimcp_fsc_result_t nimcp_fractal_security_verify_data(
    nimcp_fractal_security_t* fsc,
    void* data
);

/**
 * @brief Verify node and propagate up tree
 *
 * @param fsc Fractal security context
 * @param node Node to verify
 * @return Verification result
 */
nimcp_fsc_result_t nimcp_fractal_security_verify_node(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

/**
 * @brief Verify entire subtree from node
 *
 * @param fsc Fractal security context
 * @param node Root of subtree (NULL = entire tree)
 * @return Verification result
 */
nimcp_fsc_result_t nimcp_fractal_security_verify_subtree(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

/**
 * @brief Full tree verification
 *
 * @param fsc Fractal security context
 * @return Verification result
 */
nimcp_fsc_result_t nimcp_fractal_security_verify_all(
    nimcp_fractal_security_t* fsc
);

//=============================================================================
// Fractal Dimension Analysis
//=============================================================================

/**
 * @brief Compute fractal dimension of security tree
 *
 * Uses box-counting method to estimate dimension.
 *
 * @param fsc Fractal security context
 * @param dimension Output: computed dimension
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_compute_dimension(
    nimcp_fractal_security_t* fsc,
    float* dimension
);

/**
 * @brief Check for dimension anomaly
 *
 * Compares current dimension to expected, detects tampering.
 *
 * @param fsc Fractal security context
 * @param deviation Output: deviation from expected (can be NULL)
 * @return true if anomaly detected
 */
bool nimcp_fractal_security_check_dimension_anomaly(
    nimcp_fractal_security_t* fsc,
    float* deviation
);

/**
 * @brief Get local dimension at node
 *
 * @param fsc Fractal security context
 * @param node Node to analyze
 * @return Local fractal dimension
 */
float nimcp_fractal_security_local_dimension(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

//=============================================================================
// Guardian Sentinels
//=============================================================================

/**
 * @brief Place guardian sentinel at node
 *
 * Guardian nodes provide additional verification points.
 *
 * @param fsc Fractal security context
 * @param node Node to guard
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_place_guardian(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

/**
 * @brief Check all guardian sentinels
 *
 * @param fsc Fractal security context
 * @return Number of guardians that detected issues
 */
uint32_t nimcp_fractal_security_check_guardians(
    nimcp_fractal_security_t* fsc
);

/**
 * @brief Auto-place guardians based on fractal pattern
 *
 * Places guardians at scale-invariant intervals.
 *
 * @param fsc Fractal security context
 * @return Number of guardians placed
 */
uint32_t nimcp_fractal_security_auto_place_guardians(
    nimcp_fractal_security_t* fsc
);

//=============================================================================
// Trust Propagation
//=============================================================================

/**
 * @brief Propagate trust verification up tree
 *
 * After verifying a leaf, propagate verification state
 * to parent nodes.
 *
 * @param fsc Fractal security context
 * @param node Starting node
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_propagate_trust(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

/**
 * @brief Get trust level at node
 *
 * Returns 0.0-1.0 based on verification state of
 * node and children.
 *
 * @param fsc Fractal security context
 * @param node Node to query
 * @return Trust level (0.0 = untrusted, 1.0 = fully verified)
 */
float nimcp_fractal_security_trust_level(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

/**
 * @brief Invalidate trust below node
 *
 * Marks node and all descendants as requiring re-verification.
 *
 * @param fsc Fractal security context
 * @param node Node to invalidate
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_invalidate_trust(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

//=============================================================================
// Anomaly Detection
//=============================================================================

/**
 * @brief Set anomaly callback
 *
 * @param fsc Fractal security context
 * @param callback Callback function
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_set_anomaly_callback(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_anomaly_callback_t callback,
    void* user_data
);

/**
 * @brief Run anomaly detection scan
 *
 * Performs full scan for structural and temporal anomalies.
 *
 * @param fsc Fractal security context
 * @param anomalies Output: array of detected anomalies (caller frees)
 * @param count Output: number of anomalies
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_detect_anomalies(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_anomaly_t** anomalies,
    uint32_t* count
);

//=============================================================================
// Self-Healing
//=============================================================================

/**
 * @brief Enable automatic repair
 *
 * When corruption is detected, attempt to repair using
 * redundant fractal structures.
 *
 * @param fsc Fractal security context
 * @param enable Enable/disable auto-repair
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_enable_auto_repair(
    nimcp_fractal_security_t* fsc,
    bool enable
);

/**
 * @brief Attempt to repair corrupted node
 *
 * @param fsc Fractal security context
 * @param node Node to repair
 * @return true if repair successful
 */
bool nimcp_fractal_security_repair_node(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

/**
 * @brief Rebuild subtree from valid children
 *
 * @param fsc Fractal security context
 * @param node Root of subtree to rebuild
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_rebuild_subtree(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_node_t* node
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get fractal security statistics
 *
 * @param fsc Fractal security context
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_get_stats(
    nimcp_fractal_security_t* fsc,
    nimcp_fsc_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param fsc Fractal security context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_reset_stats(
    nimcp_fractal_security_t* fsc
);

//=============================================================================
// Integration with Coverage System
//=============================================================================

/**
 * @brief Register fractal security with coverage system
 *
 * @param fsc Fractal security context
 * @param coverage Coverage system context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_fractal_security_register_coverage(
    nimcp_fractal_security_t* fsc,
    nimcp_security_coverage_t* coverage
);

/**
 * @brief Get coverage contribution
 *
 * Returns how much fractal security contributes to overall coverage.
 *
 * @param fsc Fractal security context
 * @return Coverage ratio (0.0-1.0)
 */
float nimcp_fractal_security_coverage_contribution(
    nimcp_fractal_security_t* fsc
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get result name as string
 *
 * @param result Result code
 * @return Result name
 */
const char* nimcp_fsc_result_name(nimcp_fsc_result_t result);

/**
 * @brief Get node type name
 *
 * @param type Node type
 * @return Type name
 */
const char* nimcp_fsc_node_type_name(nimcp_fsc_node_type_t type);

/**
 * @brief Dump tree structure for debugging
 *
 * @param fsc Fractal security context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
int nimcp_fractal_security_dump_tree(
    nimcp_fractal_security_t* fsc,
    char* buffer,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SECURITY_FRACTAL_H
