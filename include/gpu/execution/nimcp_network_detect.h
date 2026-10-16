/**
 * @file nimcp_network_detect.h
 * @brief Network/MPI Capability Detection for Distributed Execution
 *
 * WHAT: Detects network and distributed computing capabilities
 * WHY:  Enables distributed neural network execution across nodes
 * HOW:  Probes for MPI, network interfaces, and connectivity
 *
 * ARCHITECTURE:
 *
 *   ┌────────────────────────────────────────────────────────────┐
 *   │               NETWORK CAPABILITY DETECTION                 │
 *   │                                                            │
 *   │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐ │
 *   │  │     MPI      │    │   Network    │    │   InfiniBand │ │
 *   │  │   Runtime    │    │  Interfaces  │    │    RDMA      │ │
 *   │  └──────────────┘    └──────────────┘    └──────────────┘ │
 *   │         │                  │                    │         │
 *   │         └──────────────────┼────────────────────┘         │
 *   │                            ▼                              │
 *   │               ┌──────────────────────┐                    │
 *   │               │ network_capabilities │                    │
 *   │               │  (Unified Results)   │                    │
 *   │               └──────────────────────┘                    │
 *   └────────────────────────────────────────────────────────────┘
 *
 * DETECTED CAPABILITIES:
 * - MPI availability and version
 * - Number of available nodes/processes
 * - Network interfaces and speeds
 * - InfiniBand/RDMA availability
 * - Estimated inter-node bandwidth
 *
 * THREAD SAFETY: Thread-safe (detection is read-only)
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_NETWORK_DETECT_H
#define NIMCP_NETWORK_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Network Feature Flags
//=============================================================================

/**
 * @brief Network transport types (bitmask)
 */
typedef enum {
    NETWORK_TRANSPORT_NONE      = 0,
    NETWORK_TRANSPORT_TCP       = (1 << 0),  /**< TCP/IP networking */
    NETWORK_TRANSPORT_UDP       = (1 << 1),  /**< UDP networking */
    NETWORK_TRANSPORT_RDMA      = (1 << 2),  /**< RDMA (InfiniBand/RoCE) */
    NETWORK_TRANSPORT_SHARED_MEM = (1 << 3), /**< Shared memory (single node) */
    NETWORK_TRANSPORT_NVLINK    = (1 << 4),  /**< NVIDIA NVLink */
    NETWORK_TRANSPORT_INFINITY  = (1 << 5),  /**< InfiniBand */
} network_transport_t;

/**
 * @brief MPI implementation types
 */
typedef enum {
    MPI_IMPL_NONE,              /**< No MPI available */
    MPI_IMPL_OPENMPI,           /**< Open MPI */
    MPI_IMPL_MPICH,             /**< MPICH */
    MPI_IMPL_INTEL_MPI,         /**< Intel MPI */
    MPI_IMPL_MVAPICH2,          /**< MVAPICH2 (GPU-aware) */
    MPI_IMPL_CRAY_MPICH,        /**< Cray MPICH */
    MPI_IMPL_MICROSOFT_MPI,     /**< Microsoft MPI */
    MPI_IMPL_OTHER,             /**< Other MPI implementation */
} mpi_impl_t;

/**
 * @brief Network interface information
 */
typedef struct {
    char name[32];              /**< Interface name (e.g., "eth0", "ib0") */
    char ip_address[64];        /**< IP address string */
    uint32_t speed_mbps;        /**< Link speed in Mbps */
    bool is_up;                 /**< Interface is up and running */
    bool is_loopback;           /**< Is loopback interface */
    network_transport_t type;   /**< Transport type */
} network_interface_t;

//=============================================================================
// Network Capabilities Structure
//=============================================================================

/**
 * @brief Network capabilities structure
 *
 * WHAT: Contains detected network and distributed computing features
 * WHY:  Unified representation for mode selection
 * HOW:  Populated by network_detect_capabilities()
 */
typedef struct {
    // Overall availability
    bool network_available;     /**< Any usable network available */
    bool mpi_available;         /**< MPI runtime available */
    bool rdma_available;        /**< RDMA/InfiniBand available */

    // MPI information
    mpi_impl_t mpi_impl;        /**< MPI implementation type */
    int mpi_version_major;      /**< MPI version major */
    int mpi_version_minor;      /**< MPI version minor */
    char mpi_version_string[64];/**< Full MPI version string */

    // Cluster information
    uint32_t num_nodes;         /**< Number of nodes in cluster (1 = single node) */
    uint32_t num_procs;         /**< Number of MPI processes available */
    int local_rank;             /**< Local rank (if running under MPI) */
    int world_rank;             /**< World rank (if running under MPI) */
    bool is_mpi_initialized;    /**< MPI already initialized */

    // Network interfaces
    network_interface_t interfaces[16];
    uint32_t num_interfaces;

    // Performance estimates
    uint32_t max_bandwidth_mbps;    /**< Max inter-node bandwidth estimate */
    uint32_t estimated_latency_us;  /**< Estimated inter-node latency */

    // Transport capabilities
    uint32_t transports;        /**< Bitmask of network_transport_t */

    // GPU-aware MPI
    bool cuda_aware_mpi;        /**< MPI can directly access GPU memory */
    bool rocm_aware_mpi;        /**< MPI can directly access AMD GPU memory */
} network_capabilities_t;

//=============================================================================
// Detection API
//=============================================================================

/**
 * @brief Detect network capabilities
 *
 * WHAT: Probes system for network and distributed computing features
 * WHY:  Need to know if distributed execution is possible
 * HOW:  Checks MPI, network interfaces, RDMA availability
 *
 * @param caps Output structure for capabilities (must not be NULL)
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe (read-only operations)
 * CACHING: Results are cached after first call
 *
 * NOTES:
 * - If running under MPI, reports actual cluster configuration
 * - If not running under MPI, probes for MPI availability
 * - Network interfaces are enumerated regardless of MPI
 *
 * EXAMPLE:
 *   network_capabilities_t caps;
 *   if (network_detect_capabilities(&caps)) {
 *       if (caps.mpi_available && caps.num_nodes > 1) {
 *           use_distributed_mode();
 *       }
 *   }
 */
NIMCP_EXPORT bool network_detect_capabilities(network_capabilities_t* caps);

/**
 * @brief Check if MPI is available
 *
 * @return true if MPI runtime can be used
 */
NIMCP_EXPORT bool network_has_mpi(void);

/**
 * @brief Check if currently running under MPI
 *
 * @return true if process was launched via mpirun/mpiexec
 */
NIMCP_EXPORT bool network_is_mpi_process(void);

/**
 * @brief Get number of available nodes
 *
 * @return Number of nodes (1 = single node)
 */
NIMCP_EXPORT uint32_t network_get_num_nodes(void);

/**
 * @brief Get MPI implementation name
 *
 * @param impl Implementation type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* network_mpi_impl_name(mpi_impl_t impl);

/**
 * @brief Get network transport name
 *
 * @param transport Transport type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* network_transport_name(network_transport_t transport);

/**
 * @brief Get estimated bandwidth for distributed communication
 *
 * @return Bandwidth in Mbps (0 if no network available)
 */
NIMCP_EXPORT uint32_t network_get_bandwidth(void);

/**
 * @brief Initialize MPI if not already initialized
 *
 * WHAT: Initializes MPI runtime
 * WHY:  Required before MPI communication
 * HOW:  Calls MPI_Init if available and not already initialized
 *
 * @return true on success, false if MPI unavailable or init failed
 *
 * NOTE: Safe to call multiple times (idempotent)
 */
NIMCP_EXPORT bool network_mpi_init(void);

/**
 * @brief Finalize MPI
 *
 * WHAT: Shuts down MPI runtime
 * WHY:  Clean up at program exit
 * HOW:  Calls MPI_Finalize if initialized
 *
 * NOTE: Must be called at most once
 */
NIMCP_EXPORT void network_mpi_finalize(void);

/**
 * @brief Check if network supports GPU-direct communication
 *
 * @return true if MPI can directly access GPU memory
 */
NIMCP_EXPORT bool network_is_gpu_aware(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NETWORK_DETECT_H
