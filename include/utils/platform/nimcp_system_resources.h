//=============================================================================
// nimcp_system_resources.h - System Resource Detection API
//=============================================================================
/**
 * @file nimcp_system_resources.h
 * @brief Hardware-aware resource detection for dynamic brain sizing
 *
 * WHAT: Detects available system resources (RAM, GPU, CPU, disk)
 * WHY:  Enable hardware-aware brain growth decisions
 * HOW:  Query OS and hardware capabilities
 *
 * FEATURES:
 * - RAM detection (total, available)
 * - GPU detection (CUDA, OpenCL, memory)
 * - Neuromorphic core detection (Intel Loihi, etc.)
 * - CPU capabilities (cores, cache)
 * - Disk space (for checkpoints)
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.8.0
 */

#ifndef NIMCP_SYSTEM_RESOURCES_H
#define NIMCP_SYSTEM_RESOURCES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Resource Structures
//=============================================================================

/**
 * @brief Basic GPU capabilities (simple struct for system resource queries)
 *
 * NOTE: For comprehensive GPU detection with device enumeration,
 * use gpu_capabilities_t from gpu/execution/nimcp_gpu_detect.h instead.
 */
typedef struct {
    bool cuda_available;           /**< CUDA GPU detected */
    bool opencl_available;         /**< OpenCL GPU detected */
    uint32_t num_gpus;             /**< Number of GPUs */
    uint64_t total_vram_mb;        /**< Total GPU memory (MB) */
    uint64_t available_vram_mb;    /**< Available GPU memory (MB) */
    uint32_t compute_units;        /**< GPU compute units */
} gpu_basic_capabilities_t;

/**
 * @brief Neuromorphic hardware capabilities
 */
typedef struct {
    bool loihi_available;          /**< Intel Loihi detected */
    bool spinnaker_available;      /**< SpiNNaker detected */
    bool brainscales_available;    /**< BrainScaleS detected */
    uint32_t num_cores;            /**< Number of neuromorphic cores */
    uint64_t neuron_capacity;      /**< Max neurons supported */
} neuromorphic_capabilities_t;

/**
 * @brief System resource summary
 */
typedef struct {
    // RAM
    uint64_t total_ram_mb;         /**< Total system RAM (MB) */
    uint64_t available_ram_mb;     /**< Available RAM (MB) */
    uint64_t free_ram_mb;          /**< Free RAM (MB) */

    // CPU
    uint32_t num_cpu_cores;        /**< Physical CPU cores */
    uint32_t num_threads;          /**< Hardware threads */
    uint64_t cpu_cache_kb;         /**< CPU cache size (KB) */

    // Storage
    uint64_t available_disk_mb;    /**< Available disk space (MB) */

    // Accelerators
    gpu_basic_capabilities_t gpu;       /**< Basic GPU info */
    neuromorphic_capabilities_t neuro;  /**< Neuromorphic hardware */
} system_resources_t;

//=============================================================================
// Resource Detection API
//=============================================================================

/**
 * @brief Query all system resources
 *
 * WHAT: Detect all available hardware resources
 * WHY:  Enable hardware-aware brain sizing decisions
 * HOW:  Query OS APIs and hardware capabilities
 *
 * PLATFORM SUPPORT:
 * - Linux: /proc/meminfo, sysconf, CUDA/OpenCL
 * - macOS: sysctl, Metal
 * - Windows: GlobalMemoryStatusEx, CUDA/OpenCL
 *
 * @param resources Output: system resource summary
 * @return true on success, false on error
 */
bool system_resources_query(system_resources_t* resources);

/**
 * @brief Estimate max neurons for available resources
 *
 * WHAT: Calculate maximum safe neuron count based on available resources
 * WHY:  Prevent OOM crashes from excessive brain growth
 * HOW:  Apply memory-per-neuron estimates with safety margin
 *
 * ESTIMATION:
 * - CPU mode: ~10KB per neuron (synapses, traces, state)
 * - GPU mode: ~5KB per neuron (parallel structures)
 * - Safety margin: 80% of available RAM
 *
 * @param resources System resources
 * @param use_gpu Whether GPU will be used
 * @return Maximum safe neuron count
 */
uint32_t system_resources_estimate_max_neurons(const system_resources_t* resources, bool use_gpu);

/**
 * @brief Check if sufficient resources for resize
 *
 * WHAT: Verify target size is achievable with current resources
 * WHY:  Prevent resize failures due to insufficient memory
 * HOW:  Compare estimated memory needs vs available
 *
 * @param resources System resources
 * @param target_neurons Target neuron count
 * @param use_gpu Whether GPU will be used
 * @return true if resize is safe, false if insufficient resources
 */
bool system_resources_can_resize(const system_resources_t* resources,
                                  uint32_t target_neurons, bool use_gpu);

/**
 * @brief Get recommended next size based on resources
 *
 * WHAT: Compute optimal next size considering hardware constraints
 * WHY:  Balance growth ambitions with resource availability
 * HOW:  Apply growth policy capped by resource limits
 *
 * GROWTH POLICY:
 * 1. Prefer 1.5× growth
 * 2. Cap at 80% of estimated max neurons
 * 3. Respect GPU VRAM limits
 * 4. Consider neuromorphic hardware capacity
 *
 * @param resources System resources
 * @param current_neurons Current neuron count
 * @param use_gpu Whether GPU will be used
 * @return Recommended next neuron count
 */
uint32_t system_resources_recommend_size(const system_resources_t* resources,
                                          uint32_t current_neurons, bool use_gpu);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYSTEM_RESOURCES_H
