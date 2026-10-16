/**
 * @file nimcp_multigpu_immune_bridge.h
 * @brief Multi-GPU Coordination-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and multi-GPU coordination
 * WHY:  Multi-GPU load balancing and work distribution should adapt to immune state;
 *       GPU coordination failures should trigger immune responses
 * HOW:  Inflammation modulates work partitioning and rebalancing strategies;
 *       coordination errors trigger antigen presentation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → MULTI-GPU PATHWAYS:
 * ---------------------------
 * 1. Resource Reallocation During Illness:
 *    - High inflammation → reduce active GPU count (energy conservation)
 *    - Cytokine storm → single GPU mode (emergency consolidation)
 *    - Systemic inflammation → disable work stealing (reduce overhead)
 *    - Reference: Biological systems consolidate resources during stress
 *
 * 2. Load Balancing Modulation:
 *    - IL-1β/IL-6/TNF-α → reduce rebalancing frequency
 *    - Inflammation → increase imbalance tolerance (avoid churn)
 *    - IL-10 → resume aggressive load balancing
 *
 * 3. Work Partitioning Adaptation:
 *    - NONE/LOCAL → Dynamic partitioning (adaptive)
 *    - REGIONAL → Static partitioning (predictable)
 *    - SYSTEMIC/STORM → Minimal GPUs (consolidate)
 *
 * MULTI-GPU → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Coordination Failures:
 *    - P2P access failure → antigen presentation (severity 7)
 *    - Device enumeration failure → immune activation (severity 6)
 *    - Memory sync failure → moderate threat (severity 5)
 *    - Load imbalance detection → stress indicator (severity 3)
 *
 * 2. Performance Monitoring:
 *    - Repeated rebalancing → chronic stress
 *    - Work stealing thrashing → inflammatory response
 *    - GPU utilization imbalance → adaptive immune learning
 *
 * 3. Resource Exhaustion:
 *    - Multi-GPU memory pressure → cytokine release
 *    - Broadcast/gather failures → immune system alert
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   MULTI-GPU IMMUNE BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → MULTI-GPU PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  INFLAMMATION    │                                             │  ║
 * ║   │   │ ──────────────── │                                             │  ║
 * ║   │   │ NONE     → 4 GPU │  ───────┐                                   │  ║
 * ║   │   │ LOCAL    → 4 GPU │         │                                   │  ║
 * ║   │   │ REGIONAL → 2 GPU │         ├──→ Active GPU Count               │  ║
 * ║   │   │ SYSTEMIC → 1 GPU │         │                                   │  ║
 * ║   │   │ STORM    → 1 GPU │         │                                   │  ║
 * ║   │   └──────────────────┘         │                                   │  ║
 * ║   │                                ▼                                   │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     MULTI-GPU SYSTEM            │                             │  ║
 * ║   │   │  - Active GPU count reduced     │                             │  ║
 * ║   │   │  - Partitioning simplified      │                             │  ║
 * ║   │   │  - Rebalancing less frequent    │                             │  ║
 * ║   │   │  - Work stealing disabled       │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  MULTI-GPU → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ P2P FAILURE      │ ──→ Antigen Presentation (severity 7)       │  ║
 * ║   │   │ SYNC FAILURE     │ ──→ Immune Activation (severity 5)          │  ║
 * ║   │   │ LOAD IMBALANCE   │ ──→ Stress Response (severity 3)            │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ REBALANCE CHURN  │ ──→ Chronic Stress Indicator                │  ║
 * ║   │   │ BROADCAST FAIL   │ ──→ Inflammatory Signaling                  │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MULTIGPU_IMMUNE_BRIDGE_H
#define NIMCP_MULTIGPU_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "gpu/nimcp_multigpu.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Inflammation active GPU count mapping */
#define INFLAMMATION_NONE_GPU_COUNT     0    /**< Use all GPUs (0 = all) */
#define INFLAMMATION_LOCAL_GPU_COUNT    0    /**< Still all GPUs */
#define INFLAMMATION_REGIONAL_GPU_COUNT 2    /**< Reduce to 2 GPUs */
#define INFLAMMATION_SYSTEMIC_GPU_COUNT 1    /**< Single GPU */
#define INFLAMMATION_STORM_GPU_COUNT    1    /**< Emergency single GPU */

/* Inflammation partition strategy mapping */
#define INFLAMMATION_PARTITION_NONE_LOCAL    MULTIGPU_PARTITION_DYNAMIC
#define INFLAMMATION_PARTITION_REGIONAL      MULTIGPU_PARTITION_HYBRID
#define INFLAMMATION_PARTITION_SYSTEMIC      MULTIGPU_PARTITION_LAYER   /* Use LAYER as the "simpler" strategy */
#define INFLAMMATION_PARTITION_STORM         MULTIGPU_PARTITION_LAYER

/* Inflammation rebalancing frequency modulation */
#define INFLAMMATION_REBALANCE_NONE_FACTOR     1.0f   /**< Normal frequency */
#define INFLAMMATION_REBALANCE_LOCAL_FACTOR    1.0f   /**< Normal */
#define INFLAMMATION_REBALANCE_REGIONAL_FACTOR 0.5f   /**< Half frequency */
#define INFLAMMATION_REBALANCE_SYSTEMIC_FACTOR 0.25f  /**< Quarter frequency */
#define INFLAMMATION_REBALANCE_STORM_FACTOR    0.0f   /**< Disabled */

/* Multi-GPU error severity mapping */
#define MULTIGPU_ERROR_SEVERITY_P2P_FAILURE       7   /**< P2P access failed */
#define MULTIGPU_ERROR_SEVERITY_DEVICE_ENUM_FAIL  6   /**< Device enumeration failed */
#define MULTIGPU_ERROR_SEVERITY_SYNC_FAILURE      5   /**< Memory sync failed */
#define MULTIGPU_ERROR_SEVERITY_BROADCAST_FAIL    5   /**< Broadcast failed */
#define MULTIGPU_ERROR_SEVERITY_GATHER_FAIL       5   /**< Gather failed */
#define MULTIGPU_ERROR_SEVERITY_LOAD_IMBALANCE    3   /**< Load imbalance detected */
#define MULTIGPU_ERROR_SEVERITY_REBALANCE_CHURN   3   /**< Frequent rebalancing */

/* Load imbalance thresholds */
#define MULTIGPU_IMBALANCE_NORMAL_THRESHOLD    0.15f  /**< 15% imbalance OK */
#define MULTIGPU_IMBALANCE_INFLAMED_THRESHOLD  0.30f  /**< 30% during inflammation */
#define MULTIGPU_IMBALANCE_STORM_THRESHOLD     0.50f  /**< 50% during storm */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine multi-GPU effects
 *
 * Represents how cytokine levels modulate multi-GPU coordination
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_coordination_impairment;  /**< IL-1β coordination penalty */
    float il6_coordination_impairment;  /**< IL-6 coordination penalty */
    float tnf_coordination_impairment;  /**< TNF-α coordination penalty */
    float ifn_gamma_gpu_reduction;      /**< IFN-γ GPU count reduction */

    /* Anti-inflammatory effects */
    float il10_coordination_recovery;   /**< IL-10 coordination recovery */

    /* Aggregate effects */
    uint32_t recommended_gpu_count;     /**< Immune-recommended GPU count */
    multigpu_partition_strategy_t recommended_partition; /**< Partition strategy */
    float rebalance_frequency_factor;   /**< Rebalancing frequency [0-1] */
    float imbalance_tolerance;          /**< Imbalance tolerance [0-1] */
    bool disable_work_stealing;         /**< Disable work stealing */
} multigpu_cytokine_effects_t;

/**
 * @brief Multi-GPU error state for immune monitoring
 *
 * Tracks multi-GPU coordination failures for immune system
 */
typedef struct {
    /* Error counters */
    uint32_t p2p_failures;            /**< P2P access failures */
    uint32_t device_enum_failures;    /**< Device enumeration failures */
    uint32_t sync_failures;           /**< Memory sync failures */
    uint32_t broadcast_failures;      /**< Broadcast failures */
    uint32_t gather_failures;         /**< Gather failures */
    uint32_t partition_failures;      /**< Partition failures */

    /* Coordination metrics */
    uint32_t rebalance_count;         /**< Rebalancing events */
    uint32_t work_stealing_events;    /**< Work stealing events */
    float current_load_imbalance;     /**< Current imbalance [0-1] */
    float avg_gpu_utilization;        /**< Average GPU utilization [0-1] */

    /* Per-GPU metrics */
    uint32_t active_gpu_count;        /**< Currently active GPUs */
    uint32_t total_gpu_count;         /**< Total available GPUs */
    float* per_gpu_utilization;       /**< Per-GPU utilization array */

    /* Stress indicators */
    bool load_imbalance_detected;     /**< Significant imbalance */
    bool rebalancing_thrashing;       /**< Excessive rebalancing */
    bool coordination_degraded;       /**< Coordination performance poor */

    /* Last error info */
    int last_error_code;              /**< Last error code */
    uint64_t last_error_timestamp;    /**< When last error occurred */
} multigpu_error_state_t;

/**
 * @brief Multi-GPU-driven immune modulation
 *
 * How multi-GPU state affects immune function
 */
typedef struct {
    /* Multi-GPU stress state */
    float coordination_stress_level;  /**< Stress from coordination [0-1] */
    float imbalance_stress_level;     /**< Stress from imbalance [0-1] */
    float rebalance_stress_level;     /**< Stress from rebalancing [0-1] */

    /* Immune triggers */
    bool should_trigger_immune;       /**< Should activate immune response */
    uint8_t antigen_severity;         /**< Severity for antigen presentation */
    bool cytokine_release_triggered;  /**< Cytokine release from coordination error */

    /* Error-specific responses */
    uint32_t errors_since_last_update; /**< Errors since last immune update */
    bool critical_error_detected;     /**< Critical error requiring response */
} multigpu_immune_modulation_t;

/**
 * @brief Complete multi-GPU immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    multigpu_context_t multigpu_context;

    /* Current state */
    multigpu_cytokine_effects_t cytokine_effects;
    multigpu_error_state_t error_state;
    multigpu_immune_modulation_t immune_modulation;

    /* Integration flags */
    bool enable_cytokine_coordination_modulation;
    bool enable_multigpu_error_immune_response;
    bool enable_gpu_count_modulation;
    bool enable_partition_modulation;
    bool enable_rebalance_modulation;

    /* Configuration */
    uint32_t baseline_gpu_count;      /**< Baseline GPU count */
    multigpu_partition_strategy_t baseline_partition; /**< Baseline partition */
    uint32_t baseline_rebalance_interval; /**< Baseline rebalance interval */

    /* Timing */
    uint64_t last_update_time;
    uint64_t error_window_start;      /**< Start of error counting window */
    uint64_t last_rebalance_time;     /**< Last rebalancing event */

    /* Statistics */
    uint64_t total_updates;
    uint32_t gpu_count_changes;
    uint32_t partition_changes;
    uint32_t immune_triggers;
    uint32_t rebalance_suppressions;

    nimcp_platform_mutex_t* mutex;
} multigpu_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_coordination_modulation;
    bool enable_multigpu_error_immune_response;
    bool enable_gpu_count_modulation;
    bool enable_partition_modulation;
    bool enable_rebalance_modulation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float error_sensitivity;            /**< Error response sensitivity [0.5-2.0] */
    float imbalance_sensitivity;        /**< Imbalance detection sensitivity [0.5-2.0] */

    /* Baseline configuration */
    uint32_t baseline_gpu_count;        /**< Baseline GPU count (0 = all) */
    multigpu_partition_strategy_t baseline_partition;
    uint32_t baseline_rebalance_interval; /**< Baseline rebalance interval */

    /* Thresholds */
    float imbalance_threshold;          /**< Imbalance threshold [0.1-0.3] */
    uint32_t rebalance_churn_threshold; /**< Rebalances for "thrashing" */
    uint32_t error_window_ms;           /**< Error counting window (ms) */
} multigpu_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int multigpu_immune_default_config(multigpu_immune_config_t* config);

/**
 * @brief Create multi-GPU immune bridge
 *
 * WHAT: Initialize bidirectional multi-GPU-immune integration
 * WHY:  Enable realistic multi-GPU-immune coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param multigpu_context Multi-GPU context
 * @return New bridge or NULL on failure
 */
multigpu_immune_bridge_t* multigpu_immune_create(
    const multigpu_immune_config_t* config,
    brain_immune_system_t* immune_system,
    multigpu_context_t multigpu_context
);

/**
 * @brief Destroy multi-GPU immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void multigpu_immune_destroy(multigpu_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Multi-GPU API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to multi-GPU coordination
 *
 * WHAT: Modulate GPU count and coordination based on cytokine levels
 * WHY:  Inflammation should conserve resources via GPU consolidation
 * HOW:  Query immune system cytokines, adjust GPU count and strategies
 *
 * @param bridge Multi-GPU immune bridge
 * @return 0 on success
 */
int multigpu_immune_apply_cytokine_effects(multigpu_immune_bridge_t* bridge);

/**
 * @brief Get recommended GPU count from immune state
 *
 * WHAT: Calculate optimal GPU count given inflammation level
 * WHY:  Balance parallelism vs energy conservation
 * HOW:  Map inflammation level to GPU count
 *
 * @param bridge Multi-GPU immune bridge
 * @return Recommended GPU count (0 = all available)
 */
uint32_t multigpu_immune_get_recommended_gpu_count(
    const multigpu_immune_bridge_t* bridge
);

/**
 * @brief Get recommended partition strategy from immune state
 *
 * WHAT: Determine partitioning strategy based on inflammation
 * WHY:  Simpler partitioning during stress reduces overhead
 * HOW:  Map inflammation to partition strategy
 *
 * @param bridge Multi-GPU immune bridge
 * @return Recommended partition strategy
 */
multigpu_partition_strategy_t multigpu_immune_get_recommended_partition(
    const multigpu_immune_bridge_t* bridge
);

/**
 * @brief Get rebalancing frequency factor
 *
 * WHAT: Get frequency multiplier for load rebalancing
 * WHY:  Reduce rebalancing during inflammation to avoid churn
 * HOW:  Return factor [0-1] based on inflammation
 *
 * @param bridge Multi-GPU immune bridge
 * @return Frequency factor [0-1] (0 = disabled, 1 = normal)
 */
float multigpu_immune_get_rebalance_frequency_factor(
    const multigpu_immune_bridge_t* bridge
);

/* ============================================================================
 * Multi-GPU → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from multi-GPU error
 *
 * WHAT: Activate immune system when coordination errors occur
 * WHY:  Multi-GPU failures threaten distributed computation integrity
 * HOW:  Present antigen based on error severity
 *
 * @param bridge Multi-GPU immune bridge
 * @param error_code Error code
 * @param error_message Error description
 * @return 0 on success
 */
int multigpu_immune_trigger_error_response(
    multigpu_immune_bridge_t* bridge,
    int error_code,
    const char* error_message
);

/**
 * @brief Monitor load imbalance and trigger immune if chronic
 *
 * WHAT: Check GPU utilization balance; trigger immune if imbalanced
 * WHY:  Chronic imbalance indicates coordination stress
 * HOW:  Query per-GPU utilization, compute imbalance, trigger if high
 *
 * @param bridge Multi-GPU immune bridge
 * @return 0 on success
 */
int multigpu_immune_monitor_load_balance(multigpu_immune_bridge_t* bridge);

/**
 * @brief Update multi-GPU error state
 *
 * WHAT: Refresh coordination error counters and metrics
 * WHY:  Need current multi-GPU state for immune decision making
 * HOW:  Query multi-GPU context for stats and errors
 *
 * @param bridge Multi-GPU immune bridge
 * @return 0 on success
 */
int multigpu_immune_update_error_state(multigpu_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update multi-GPU immune bridge (both directions)
 *
 * WHAT: Process all multi-GPU-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, check coordination errors, adjust GPUs
 *
 * @param bridge Multi-GPU immune bridge
 * @return 0 on success
 */
int multigpu_immune_update(multigpu_immune_bridge_t* bridge);

/**
 * @brief Apply modulation to multi-GPU context
 *
 * WHAT: Update multi-GPU configuration with immune-modulated parameters
 * WHY:  Actually apply GPU count, partition, and rebalancing changes
 * HOW:  Call multi-GPU APIs to update configuration
 *
 * @param bridge Multi-GPU immune bridge
 * @return 0 on success
 */
int multigpu_immune_apply_modulation(multigpu_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine multi-GPU effects
 *
 * @param bridge Multi-GPU immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int multigpu_immune_get_cytokine_effects(
    const multigpu_immune_bridge_t* bridge,
    multigpu_cytokine_effects_t* effects
);

/**
 * @brief Get current multi-GPU error state
 *
 * @param bridge Multi-GPU immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int multigpu_immune_get_error_state(
    const multigpu_immune_bridge_t* bridge,
    multigpu_error_state_t* state
);

/**
 * @brief Check if multi-GPU is under immune-induced reduction
 *
 * @param bridge Multi-GPU immune bridge
 * @return true if GPU count reduced due to immune
 */
bool multigpu_immune_is_gpu_count_reduced(const multigpu_immune_bridge_t* bridge);

/**
 * @brief Get current active GPU count
 *
 * @param bridge Multi-GPU immune bridge
 * @return Active GPU count
 */
uint32_t multigpu_immune_get_active_gpu_count(const multigpu_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_MULTIGPU
 *
 * @param bridge Multi-GPU immune bridge
 * @return 0 on success, -1 on error
 */
int multigpu_immune_connect_bio_async(multigpu_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Multi-GPU immune bridge
 * @return 0 on success
 */
int multigpu_immune_disconnect_bio_async(multigpu_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Multi-GPU immune bridge
 * @return true if connected
 */
bool multigpu_immune_is_bio_async_connected(const multigpu_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MULTIGPU_IMMUNE_BRIDGE_H */
