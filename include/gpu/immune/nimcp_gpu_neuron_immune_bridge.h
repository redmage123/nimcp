/**
 * @file nimcp_gpu_neuron_immune_bridge.h
 * @brief GPU Neuron-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and GPU neural computation
 * WHY:  GPU errors (CUDA errors, memory exhaustion) represent threats to neural integrity;
 *       inflammation should modulate GPU resource allocation and batch sizes
 * HOW:  GPU errors trigger immune responses; cytokines reduce batch sizes and throttle
 *       GPU execution to conserve resources during inflammatory states
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → GPU PATHWAYS:
 * ---------------------
 * 1. Fever-Induced Thermal Regulation:
 *    - High inflammation → reduce GPU clock speeds (biological thermal regulation)
 *    - Cytokine storm → throttle GPU to prevent overheating
 *    - IL-1β, IL-6 → reduce batch size to conserve energy
 *    - Reference: Biological fever reduces metabolic rate to conserve energy
 *
 * 2. Energy Conservation During Illness:
 *    - Inflammation → reduce parallel computation efficiency
 *    - Systemic inflammation → smaller batch sizes, fewer concurrent kernels
 *    - Resource reallocation to immune response
 *    - Reference: "Sickness behavior" - reduced activity during infection
 *
 * 3. Cytokine Effects on Neural Computation:
 *    - IL-1β → -30% batch size (impairs parallel processing)
 *    - IL-6 → -20% batch size (moderate impairment)
 *    - TNF-α → -40% batch size (severe impairment)
 *    - IL-10 → +20% recovery boost (anti-inflammatory)
 *
 * GPU → IMMUNE PATHWAYS:
 * ---------------------
 * 1. GPU Error Detection:
 *    - CUDA errors → antigen presentation (severity 8-10)
 *    - Memory exhaustion → immune activation (severity 7)
 *    - Kernel launch failure → threat detection (severity 6)
 *    - Synchronization errors → moderate threat (severity 4)
 *
 * 2. Performance Degradation:
 *    - Sustained high GPU utilization → stress response
 *    - Thermal throttling → inflammatory signaling
 *    - Memory pressure → cytokine release
 *
 * 3. Resource Exhaustion:
 *    - GPU OOM → trigger antibody production
 *    - Device reset required → cytokine storm
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    GPU NEURON-IMMUNE BRIDGE                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → GPU PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.2 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.4 │         ├──→ Batch Size Reduction               │  ║
 * ║   │   │              │         │    Kernel Throttling                  │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     GPU NEURON SYSTEM           │                             │  ║
 * ║   │   │  - Reduced batch sizes          │                             │  ║
 * ║   │   │  - Throttled execution          │                             │  ║
 * ║   │   │  - Lower clock speeds           │                             │  ║
 * ║   │   │  - Energy conservation          │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -10% batch    │                                     │  ║
 * ║   │   │ REGIONAL → -30% batch    │                                     │  ║
 * ║   │   │ SYSTEMIC → -50% batch    │                                     │  ║
 * ║   │   │ STORM    → -80% batch    │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  GPU → IMMUNE PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ CUDA ERRORS  │ ──→ Antigen Presentation (severity 10)          │  ║
 * ║   │   │ GPU OOM      │ ──→ Immune Activation (severity 8)              │  ║
 * ║   │   │ THERMAL      │ ──→ Cytokine Release (severity 6)               │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PERFORMANCE  │ ──→ Stress Response                             │  ║
 * ║   │   │ DEGRADATION  │ ──→ Inflammatory Signaling                      │  ║
 * ║   │   └──────────────┘                                                 │  ║
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

#ifndef NIMCP_GPU_NEURON_IMMUNE_BRIDGE_H
#define NIMCP_GPU_NEURON_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "gpu/nimcp_gpu_neuron.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine GPU batch size impact factors */
#define CYTOKINE_IL1_GPU_BATCH_IMPACT      -0.3f   /**< IL-1β → batch size reduction */
#define CYTOKINE_IL6_GPU_BATCH_IMPACT      -0.2f   /**< IL-6 → batch size reduction */
#define CYTOKINE_TNF_GPU_BATCH_IMPACT      -0.4f   /**< TNF-α → strong reduction */
#define CYTOKINE_IFN_GAMMA_GPU_BATCH_IMPACT -0.15f /**< IFN-γ → mild reduction */
#define CYTOKINE_IL10_GPU_BATCH_IMPACT      0.2f   /**< IL-10 → recovery boost */

/* Inflammation GPU resource modulation */
#define INFLAMMATION_NONE_GPU_FACTOR     1.0f   /**< No reduction */
#define INFLAMMATION_LOCAL_GPU_FACTOR    0.9f   /**< -10% batch size */
#define INFLAMMATION_REGIONAL_GPU_FACTOR 0.7f   /**< -30% batch size */
#define INFLAMMATION_SYSTEMIC_GPU_FACTOR 0.5f   /**< -50% batch size */
#define INFLAMMATION_STORM_GPU_FACTOR    0.2f   /**< -80% batch size (emergency) */

/* GPU error severity mapping */
#define GPU_ERROR_SEVERITY_CUDA_ERROR        10  /**< Critical CUDA error */
#define GPU_ERROR_SEVERITY_OOM               8   /**< Out of memory */
#define GPU_ERROR_SEVERITY_KERNEL_FAILURE    7   /**< Kernel launch failure */
#define GPU_ERROR_SEVERITY_THERMAL           6   /**< Thermal throttling */
#define GPU_ERROR_SEVERITY_SYNC_TIMEOUT      4   /**< Synchronization timeout */
#define GPU_ERROR_SEVERITY_PERFORMANCE_DROP  3   /**< Performance degradation */

/* GPU utilization thresholds */
#define GPU_UTILIZATION_HIGH_THRESHOLD   0.90f  /**< >90% = high stress */
#define GPU_UTILIZATION_CRITICAL_THRESHOLD 0.95f /**< >95% = critical stress */
#define GPU_THERMAL_WARNING_THRESHOLD    80.0f  /**< 80°C warning */
#define GPU_THERMAL_CRITICAL_THRESHOLD   90.0f  /**< 90°C critical */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine GPU effects
 *
 * Represents how cytokine levels modulate GPU resource allocation
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_batch_reduction;        /**< IL-1β induced reduction */
    float il6_batch_reduction;        /**< IL-6 induced reduction */
    float tnf_batch_reduction;        /**< TNF-α induced reduction */
    float ifn_gamma_batch_reduction;  /**< IFN-γ induced reduction */

    /* Anti-inflammatory effects */
    float il10_batch_recovery;        /**< IL-10 recovery boost */

    /* Aggregate effects */
    float total_batch_factor;         /**< Combined batch size factor [0-1] */
    float kernel_throttle_factor;     /**< Kernel launch throttling [0-1] */
    float memory_allocation_factor;   /**< Memory allocation factor [0-1] */
    float clock_speed_factor;         /**< GPU clock speed factor [0-1] */
} gpu_neuron_cytokine_effects_t;

/**
 * @brief GPU error state for immune monitoring
 *
 * Tracks GPU errors and performance for immune system
 */
typedef struct {
    /* Error counters */
    uint32_t cuda_errors;             /**< CUDA error count */
    uint32_t oom_events;              /**< Out of memory events */
    uint32_t kernel_failures;         /**< Kernel launch failures */
    uint32_t sync_timeouts;           /**< Sync timeout count */

    /* Performance metrics */
    float current_utilization;        /**< GPU utilization [0-1] */
    float temperature_celsius;        /**< GPU temperature (°C) */
    uint64_t memory_used_bytes;       /**< Current memory usage */
    uint64_t memory_total_bytes;      /**< Total GPU memory */

    /* Stress indicators */
    bool thermal_throttling;          /**< GPU thermally throttled */
    bool memory_pressure;             /**< Low memory available */
    bool performance_degraded;        /**< Performance below baseline */

    /* Last error info */
    int last_cuda_error_code;         /**< Last CUDA error code */
    uint64_t last_error_timestamp;    /**< When last error occurred */
} gpu_neuron_error_state_t;

/**
 * @brief GPU-driven immune modulation
 *
 * How GPU state affects immune function
 */
typedef struct {
    /* GPU stress state */
    float utilization_stress_level;   /**< Stress from high utilization [0-1] */
    float thermal_stress_level;       /**< Stress from temperature [0-1] */
    float memory_stress_level;        /**< Stress from memory pressure [0-1] */

    /* Immune triggers */
    bool should_trigger_immune;       /**< Should activate immune response */
    uint8_t antigen_severity;         /**< Severity for antigen presentation */
    bool cytokine_release_triggered;  /**< Cytokine release from GPU error */

    /* Error-specific responses */
    uint32_t errors_since_last_update; /**< Errors since last immune update */
    bool critical_error_detected;     /**< Critical error requiring response */
} gpu_neuron_immune_modulation_t;

/**
 * @brief Complete GPU neuron-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    gpu_neural_network_t gpu_network;

    /* Current state */
    gpu_neuron_cytokine_effects_t cytokine_effects;
    gpu_neuron_error_state_t error_state;
    gpu_neuron_immune_modulation_t immune_modulation;

    /* Integration flags */
    bool enable_cytokine_gpu_modulation;
    bool enable_gpu_error_immune_response;
    bool enable_thermal_regulation;
    bool enable_batch_size_modulation;
    bool enable_memory_conservation;

    /* Configuration */
    uint32_t base_batch_size;         /**< Baseline batch size */
    uint32_t min_batch_size;          /**< Minimum allowed batch size */
    uint32_t max_batch_size;          /**< Maximum allowed batch size */

    /* Timing */
    uint64_t last_update_time;
    uint64_t error_window_start;      /**< Start of error counting window */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t immune_triggers;
    uint32_t batch_reductions;
    uint32_t thermal_throttle_events;

    nimcp_platform_mutex_t* mutex;
} gpu_neuron_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_gpu_modulation;
    bool enable_gpu_error_immune_response;
    bool enable_thermal_regulation;
    bool enable_batch_size_modulation;
    bool enable_memory_conservation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float error_sensitivity;            /**< Error response sensitivity [0.5-2.0] */
    float thermal_sensitivity;          /**< Thermal response sensitivity [0.5-2.0] */

    /* Batch size limits */
    uint32_t base_batch_size;           /**< Baseline batch size */
    uint32_t min_batch_size;            /**< Minimum allowed batch size */
    uint32_t max_batch_size;            /**< Maximum allowed batch size */

    /* Thresholds */
    float utilization_threshold;        /**< Utilization for stress [0.7-0.95] */
    float thermal_threshold_celsius;    /**< Temperature for stress [70-90] */
    float memory_pressure_threshold;    /**< Memory usage for stress [0.8-0.95] */
} gpu_neuron_immune_config_t;

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
int gpu_neuron_immune_default_config(gpu_neuron_immune_config_t* config);

/**
 * @brief Create GPU neuron-immune bridge
 *
 * WHAT: Initialize bidirectional GPU-immune integration
 * WHY:  Enable realistic GPU-immune coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param gpu_network GPU neural network
 * @return New bridge or NULL on failure
 */
gpu_neuron_immune_bridge_t* gpu_neuron_immune_create(
    const gpu_neuron_immune_config_t* config,
    brain_immune_system_t* immune_system,
    gpu_neural_network_t gpu_network
);

/**
 * @brief Destroy GPU neuron-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void gpu_neuron_immune_destroy(gpu_neuron_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → GPU API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to GPU resources
 *
 * WHAT: Modulate GPU batch size and execution based on cytokine levels
 * WHY:  Inflammation should conserve resources (biological fever response)
 * HOW:  Query immune system cytokines, reduce batch size and throttle execution
 *
 * @param bridge GPU neuron-immune bridge
 * @return 0 on success
 */
int gpu_neuron_immune_apply_cytokine_effects(gpu_neuron_immune_bridge_t* bridge);

/**
 * @brief Compute GPU batch size from immune state
 *
 * WHAT: Calculate optimal batch size given inflammation level
 * WHY:  Conserve resources during immune activation
 * HOW:  Map inflammation level to batch size factor
 *
 * @param bridge GPU neuron-immune bridge
 * @return Batch size factor [0-1] (1.0 = full, 0.2 = 20% of baseline)
 */
float gpu_neuron_immune_compute_batch_factor(const gpu_neuron_immune_bridge_t* bridge);

/**
 * @brief Get modulated batch size
 *
 * WHAT: Get current batch size accounting for immune modulation
 * WHY:  Need actual batch size for GPU kernel launch
 * HOW:  Apply immune factors to base batch size
 *
 * @param bridge GPU neuron-immune bridge
 * @return Modulated batch size (neurons per kernel)
 */
uint32_t gpu_neuron_immune_get_batch_size(const gpu_neuron_immune_bridge_t* bridge);

/* ============================================================================
 * GPU → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from GPU error
 *
 * WHAT: Activate immune system when GPU errors occur
 * WHY:  GPU errors threaten neural computation integrity
 * HOW:  Present antigen based on error severity
 *
 * @param bridge GPU neuron-immune bridge
 * @param error_code CUDA error code
 * @param error_message Error description
 * @return 0 on success
 */
int gpu_neuron_immune_trigger_error_response(
    gpu_neuron_immune_bridge_t* bridge,
    int error_code,
    const char* error_message
);

/**
 * @brief Monitor GPU stress and trigger immune if needed
 *
 * WHAT: Check GPU utilization, temperature, memory; trigger immune if stressed
 * WHY:  Chronic GPU stress should activate inflammatory response
 * HOW:  Query GPU metrics, compare to thresholds, trigger cytokine release
 *
 * @param bridge GPU neuron-immune bridge
 * @return 0 on success
 */
int gpu_neuron_immune_monitor_stress(gpu_neuron_immune_bridge_t* bridge);

/**
 * @brief Update GPU error state
 *
 * WHAT: Refresh GPU error counters and performance metrics
 * WHY:  Need current GPU state for immune decision making
 * HOW:  Query GPU network for stats and errors
 *
 * @param bridge GPU neuron-immune bridge
 * @return 0 on success
 */
int gpu_neuron_immune_update_error_state(gpu_neuron_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update GPU neuron-immune bridge (both directions)
 *
 * WHAT: Process all GPU-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, check GPU errors, adjust batch sizes
 *
 * @param bridge GPU neuron-immune bridge
 * @return 0 on success
 */
int gpu_neuron_immune_update(gpu_neuron_immune_bridge_t* bridge);

/**
 * @brief Apply modulation to GPU network
 *
 * WHAT: Update GPU network configuration with immune-modulated parameters
 * WHY:  Actually apply computed batch sizes and throttling
 * HOW:  Call GPU network APIs to update batch size, memory limits
 *
 * @param bridge GPU neuron-immune bridge
 * @return 0 on success
 */
int gpu_neuron_immune_apply_modulation(gpu_neuron_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine GPU effects
 *
 * @param bridge GPU neuron-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int gpu_neuron_immune_get_cytokine_effects(
    const gpu_neuron_immune_bridge_t* bridge,
    gpu_neuron_cytokine_effects_t* effects
);

/**
 * @brief Get current GPU error state
 *
 * @param bridge GPU neuron-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int gpu_neuron_immune_get_error_state(
    const gpu_neuron_immune_bridge_t* bridge,
    gpu_neuron_error_state_t* state
);

/**
 * @brief Check if GPU is under immune-induced throttling
 *
 * @param bridge GPU neuron-immune bridge
 * @return true if throttled (batch size < baseline)
 */
bool gpu_neuron_immune_is_throttled(const gpu_neuron_immune_bridge_t* bridge);

/**
 * @brief Get current batch size factor
 *
 * @param bridge GPU neuron-immune bridge
 * @return Batch size factor [0-1]
 */
float gpu_neuron_immune_get_batch_factor(const gpu_neuron_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_GPU_NEURON
 *
 * @param bridge GPU neuron-immune bridge
 * @return 0 on success, -1 on error
 */
int gpu_neuron_immune_connect_bio_async(gpu_neuron_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge GPU neuron-immune bridge
 * @return 0 on success
 */
int gpu_neuron_immune_disconnect_bio_async(gpu_neuron_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge GPU neuron-immune bridge
 * @return true if connected
 */
bool gpu_neuron_immune_is_bio_async_connected(const gpu_neuron_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_NEURON_IMMUNE_BRIDGE_H */
