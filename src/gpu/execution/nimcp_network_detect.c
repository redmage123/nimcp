/**
 * @file nimcp_network_detect.c
 * @brief Network/MPI Capability Detection Implementation
 *
 * WHAT: Detects network and distributed computing capabilities
 * WHY:  Enables distributed neural network execution across nodes
 * HOW:  Probes for MPI, network interfaces, and connectivity
 *
 * PLATFORM SUPPORT:
 * - Linux: Full support (netlink, MPI, InfiniBand)
 * - macOS: Basic support (BSD sockets, limited MPI)
 * - Windows: Basic support (WinSock)
 *
 * MPI DETECTION:
 * - Checks environment variables (OMPI_*, PMI_*, SLURM_*)
 * - Dynamically loads MPI library if available
 * - Reports MPI version and implementation
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#define LOG_MODULE "NETWORK_DETECT"
#define LOG_MODULE_ID 0x0902
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(network_detect)

#include "gpu/execution/nimcp_network_detect.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Platform-specific includes
#ifdef __linux__
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <net/if.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <linux/ethtool.h>
    #include <linux/sockios.h>
#elif defined(__APPLE__)
    #include <unistd.h>
    #include <sys/socket.h>
    #include <net/if.h>
    #include <ifaddrs.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#elif defined(_WIN32)
    #include <winsock2.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "ws2_32.lib")
#endif

// MPI conditional support
#ifdef NIMCP_ENABLE_MPI
    #include <mpi.h>
#endif

// Thread-safe initialization
#include <pthread.h>
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Static State
//=============================================================================

static network_capabilities_t s_cached_caps = {0};
static pthread_once_t s_cache_init_once = PTHREAD_ONCE_INIT;
static volatile int s_mpi_initialized_by_us = 0;

// Forward declaration for pthread_once callback
static void network_detect_init_impl(void);

//=============================================================================
// Environment Variable Detection
//=============================================================================

/**
 * @brief Check if running under MPI based on environment variables
 */
static bool detect_mpi_environment(network_capabilities_t* caps)
{
    // P1-2 fix: Helper to safely parse environment variables using strtol
    // Open MPI environment variables
    if (getenv("OMPI_COMM_WORLD_SIZE") != NULL) {
        caps->mpi_impl = MPI_IMPL_OPENMPI;
        const char* size_str = getenv("OMPI_COMM_WORLD_SIZE");
        const char* rank_str = getenv("OMPI_COMM_WORLD_RANK");
        const char* local_rank_str = getenv("OMPI_COMM_WORLD_LOCAL_RANK");

        if (size_str) {
            char* endptr;
            long val = strtol(size_str, &endptr, 10);
            if (endptr != size_str && val > 0 && val <= UINT32_MAX) {
                caps->num_procs = (uint32_t)val;
            }
        }
        if (rank_str) {
            char* endptr;
            long val = strtol(rank_str, &endptr, 10);
            if (endptr != rank_str && val >= 0 && val <= INT32_MAX) {
                caps->world_rank = (int)val;
            }
        }
        if (local_rank_str) {
            char* endptr;
            long val = strtol(local_rank_str, &endptr, 10);
            if (endptr != local_rank_str && val >= 0 && val <= INT32_MAX) {
                caps->local_rank = (int)val;
            }
        }

        LOG_DEBUG("Detected Open MPI environment: procs=%u, rank=%d",
                  caps->num_procs, caps->world_rank);
        return true;
    }

    // MPICH environment variables
    if (getenv("PMI_RANK") != NULL || getenv("PMI_SIZE") != NULL) {
        caps->mpi_impl = MPI_IMPL_MPICH;
        const char* size_str = getenv("PMI_SIZE");
        const char* rank_str = getenv("PMI_RANK");

        if (size_str) {
            char* endptr;
            long val = strtol(size_str, &endptr, 10);
            if (endptr != size_str && val > 0 && val <= UINT32_MAX) {
                caps->num_procs = (uint32_t)val;
            }
        }
        if (rank_str) {
            char* endptr;
            long val = strtol(rank_str, &endptr, 10);
            if (endptr != rank_str && val >= 0 && val <= INT32_MAX) {
                caps->world_rank = (int)val;
            }
        }

        LOG_DEBUG("Detected MPICH environment: procs=%u, rank=%d",
                  caps->num_procs, caps->world_rank);
        return true;
    }

    // Intel MPI
    if (getenv("I_MPI_INFO_NUMA_NODE_NUM") != NULL) {
        caps->mpi_impl = MPI_IMPL_INTEL_MPI;
        LOG_DEBUG("Detected Intel MPI environment");
        return true;
    }

    // SLURM (may indicate MPI job)
    if (getenv("SLURM_PROCID") != NULL) {
        const char* ntasks_str = getenv("SLURM_NTASKS");
        const char* procid_str = getenv("SLURM_PROCID");
        const char* nnodes_str = getenv("SLURM_NNODES");

        if (ntasks_str) {
            char* endptr;
            long val = strtol(ntasks_str, &endptr, 10);
            if (endptr != ntasks_str && val > 0 && val <= UINT32_MAX) {
                caps->num_procs = (uint32_t)val;
            }
        }
        if (procid_str) {
            char* endptr;
            long val = strtol(procid_str, &endptr, 10);
            if (endptr != procid_str && val >= 0 && val <= INT32_MAX) {
                caps->world_rank = (int)val;
            }
        }
        if (nnodes_str) {
            char* endptr;
            long val = strtol(nnodes_str, &endptr, 10);
            if (endptr != nnodes_str && val > 0 && val <= UINT32_MAX) {
                caps->num_nodes = (uint32_t)val;
            }
        }

        LOG_DEBUG("Detected SLURM environment: procs=%u, nodes=%u, rank=%d",
                  caps->num_procs, caps->num_nodes, caps->world_rank);
        return true;
    }

    // PBS/Torque
    if (getenv("PBS_NODEFILE") != NULL) {
        LOG_DEBUG("Detected PBS/Torque environment");
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_mpi_environment: validation failed");
    return false;
}

//=============================================================================
// Network Interface Detection (Linux)
//=============================================================================

#ifdef __linux__

/**
 * @brief Get link speed for an interface
 */
static uint32_t get_interface_speed(const char* ifname)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 0;

    struct ifreq ifr;
    struct ethtool_cmd edata;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    edata.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (char*)&edata;

    uint32_t speed = 0;
    if (ioctl(sock, SIOCETHTOOL, &ifr) >= 0) {
        speed = ethtool_cmd_speed(&edata);
        /* Unknown speed returns -1 as uint16_t or uint32_t */
        if (speed == (uint16_t)(-1) || speed == (uint32_t)(-1) || speed == 0) {
            speed = 0;
        }
    }

    close(sock);
    return speed;
}

/**
 * @brief Detect network interfaces on Linux
 */
static void detect_interfaces_linux(network_capabilities_t* caps)
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        LOG_ERROR("Failed to get interface addresses");
        return;
    }

    uint32_t idx = 0;
    for (ifa = ifaddr; ifa != NULL && idx < 16; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        // Only process IPv4 for simplicity
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        network_interface_t* iface = &caps->interfaces[idx];

        strncpy(iface->name, ifa->ifa_name, sizeof(iface->name) - 1);

        struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, iface->ip_address, sizeof(iface->ip_address));

        iface->is_up = (ifa->ifa_flags & IFF_UP) != 0;
        iface->is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;

        // Get link speed
        iface->speed_mbps = get_interface_speed(ifa->ifa_name);

        // Determine transport type
        if (iface->is_loopback) {
            iface->type = NETWORK_TRANSPORT_SHARED_MEM;
        } else if (strncmp(ifa->ifa_name, "ib", 2) == 0 ||
                   strncmp(ifa->ifa_name, "mlx", 3) == 0) {
            iface->type = NETWORK_TRANSPORT_RDMA;
            caps->rdma_available = true;
            caps->transports |= NETWORK_TRANSPORT_RDMA;
        } else {
            iface->type = NETWORK_TRANSPORT_TCP;
            caps->transports |= NETWORK_TRANSPORT_TCP;
        }

        // Track max bandwidth
        if (iface->speed_mbps > caps->max_bandwidth_mbps && !iface->is_loopback) {
            caps->max_bandwidth_mbps = iface->speed_mbps;
        }

        idx++;
    }

    caps->num_interfaces = idx;
    freeifaddrs(ifaddr);
}

#elif defined(__APPLE__)

/**
 * @brief Detect network interfaces on macOS
 */
static void detect_interfaces_macos(network_capabilities_t* caps)
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        LOG_ERROR("Failed to get interface addresses");
        return;
    }

    uint32_t idx = 0;
    for (ifa = ifaddr; ifa != NULL && idx < 16; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        network_interface_t* iface = &caps->interfaces[idx];

        strncpy(iface->name, ifa->ifa_name, sizeof(iface->name) - 1);

        struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, iface->ip_address, sizeof(iface->ip_address));

        iface->is_up = (ifa->ifa_flags & IFF_UP) != 0;
        iface->is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;

        // macOS doesn't have easy speed detection, assume 1Gbps for now
        iface->speed_mbps = iface->is_loopback ? 0 : 1000;

        iface->type = iface->is_loopback ? NETWORK_TRANSPORT_SHARED_MEM : NETWORK_TRANSPORT_TCP;
        if (!iface->is_loopback) {
            caps->transports |= NETWORK_TRANSPORT_TCP;
            if (iface->speed_mbps > caps->max_bandwidth_mbps) {
                caps->max_bandwidth_mbps = iface->speed_mbps;
            }
        }

        idx++;
    }

    caps->num_interfaces = idx;
    freeifaddrs(ifaddr);
}

#elif defined(_WIN32)

/**
 * @brief Detect network interfaces on Windows
 */
static void detect_interfaces_windows(network_capabilities_t* caps)
{
    ULONG outBufLen = sizeof(IP_ADAPTER_INFO) * 16;
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO*)nimcp_malloc(outBufLen);

    if (pAdapterInfo == NULL) {
        LOG_ERROR("Memory allocation failed for adapter info");
        return;
    }

    if (GetAdaptersInfo(pAdapterInfo, &outBufLen) != NO_ERROR) {
        nimcp_free(pAdapterInfo);
        return;
    }

    PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
    uint32_t idx = 0;

    while (pAdapter && idx < 16) {
        network_interface_t* iface = &caps->interfaces[idx];

        strncpy(iface->name, pAdapter->AdapterName, sizeof(iface->name) - 1);
        strncpy(iface->ip_address, pAdapter->IpAddressList.IpAddress.String,
                sizeof(iface->ip_address) - 1);

        iface->is_up = true;  // Simplification
        iface->is_loopback = (strcmp(iface->ip_address, "127.0.0.1") == 0);
        iface->speed_mbps = 1000;  // Assume 1Gbps
        iface->type = iface->is_loopback ? NETWORK_TRANSPORT_SHARED_MEM : NETWORK_TRANSPORT_TCP;

        if (!iface->is_loopback) {
            caps->transports |= NETWORK_TRANSPORT_TCP;
            if (iface->speed_mbps > caps->max_bandwidth_mbps) {
                caps->max_bandwidth_mbps = iface->speed_mbps;
            }
        }

        idx++;
        pAdapter = pAdapter->Next;
    }

    caps->num_interfaces = idx;
    nimcp_free(pAdapterInfo);
}

#endif // Platform-specific interface detection

//=============================================================================
// MPI Detection
//=============================================================================

#ifdef NIMCP_ENABLE_MPI

/**
 * @brief Detect MPI capabilities when compiled with MPI support
 */
static void detect_mpi_runtime(network_capabilities_t* caps)
{
    int initialized = 0;
    MPI_Initialized(&initialized);

    caps->is_mpi_initialized = (initialized != 0);

    if (initialized) {
        // Get MPI version
        int version, subversion;
        MPI_Get_version(&version, &subversion);
        caps->mpi_version_major = version;
        caps->mpi_version_minor = subversion;

        snprintf(caps->mpi_version_string, sizeof(caps->mpi_version_string),
                 "MPI %d.%d", version, subversion);

        // Get comm world info
        int world_size, world_rank;
        MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

        caps->num_procs = (uint32_t)world_size;
        caps->world_rank = world_rank;

        // Estimate number of nodes (using hostnames)
        char hostname[MPI_MAX_PROCESSOR_NAME];
        int namelen;
        MPI_Get_processor_name(hostname, &namelen);

        // For now, assume 1 node if only 1 process
        caps->num_nodes = (world_size > 1) ? 2 : 1;  // Simplified estimate

        caps->mpi_available = true;
        caps->network_available = (world_size > 1);

        LOG_INFO("MPI initialized: version=%d.%d, procs=%d, rank=%d",
                 version, subversion, world_size, world_rank);
    } else {
        // MPI not initialized, but may be available
        caps->mpi_available = true;  // Available but not running

        // Still get version info
        int version, subversion;
        MPI_Get_version(&version, &subversion);
        caps->mpi_version_major = version;
        caps->mpi_version_minor = subversion;

        snprintf(caps->mpi_version_string, sizeof(caps->mpi_version_string),
                 "MPI %d.%d (not initialized)", version, subversion);

        caps->num_procs = 1;
        caps->num_nodes = 1;
    }

    // Check for CUDA-aware MPI
    #ifdef MPIX_CUDA_AWARE_SUPPORT
    caps->cuda_aware_mpi = (MPIX_Query_cuda_support() != 0);
    #endif
}

#else

/**
 * @brief Stub for when MPI is not available
 */
static void detect_mpi_runtime(network_capabilities_t* caps)
{
    caps->mpi_available = false;
    caps->mpi_impl = MPI_IMPL_NONE;
    caps->num_procs = 1;
    caps->num_nodes = 1;
    strncpy(caps->mpi_version_string, "Not available", sizeof(caps->mpi_version_string) - 1);
}

#endif // NIMCP_ENABLE_MPI

//=============================================================================
// InfiniBand Detection
//=============================================================================

/**
 * @brief Detect InfiniBand/RDMA availability
 */
static void detect_infiniband(network_capabilities_t* caps)
{
#ifdef __linux__
    // Check for InfiniBand devices in /sys
    FILE* fp = popen("ls /sys/class/infiniband/ 2>/dev/null | wc -l", "r");
    if (fp) {
        int count = 0;
        if (fscanf(fp, "%d", &count) == 1 && count > 0) {
            caps->rdma_available = true;
            caps->transports |= NETWORK_TRANSPORT_RDMA;
            caps->transports |= NETWORK_TRANSPORT_INFINITY;

            // Estimate bandwidth (56Gbps for FDR, 100Gbps for EDR, 200Gbps for HDR)
            if (caps->max_bandwidth_mbps < 56000) {
                caps->max_bandwidth_mbps = 56000;  // FDR InfiniBand
            }

            LOG_INFO("InfiniBand detected: %d devices", count);
        }
        pclose(fp);
    }
#else
    (void)caps;
#endif
}

//=============================================================================
// Internal Initialization (called via pthread_once)
//=============================================================================

/**
 * @brief Thread-safe one-time initialization of network detection cache
 *
 * WHY:  pthread_once guarantees this runs exactly once, even with concurrent calls
 * HOW:  Called by pthread_once in network_detect_capabilities
 */
static void network_detect_init_impl(void)
{
    // Initialize structure
    memset(&s_cached_caps, 0, sizeof(network_capabilities_t));
    s_cached_caps.num_nodes = 1;
    s_cached_caps.num_procs = 1;
    s_cached_caps.world_rank = 0;
    s_cached_caps.local_rank = 0;

    // Check environment for MPI indicators
    bool mpi_env = detect_mpi_environment(&s_cached_caps);
    (void)mpi_env;

    // Detect network interfaces
#ifdef __linux__
    detect_interfaces_linux(&s_cached_caps);
#elif defined(__APPLE__)
    detect_interfaces_macos(&s_cached_caps);
#elif defined(_WIN32)
    detect_interfaces_windows(&s_cached_caps);
#endif

    // Detect InfiniBand
    detect_infiniband(&s_cached_caps);

    // Detect MPI runtime
    detect_mpi_runtime(&s_cached_caps);

    // Set overall network availability
    if (s_cached_caps.num_interfaces > 0 && s_cached_caps.max_bandwidth_mbps > 0) {
        s_cached_caps.network_available = true;
    }

    // Estimate latency based on transport type
    if (s_cached_caps.rdma_available) {
        s_cached_caps.estimated_latency_us = 1;  // RDMA: ~1 microsecond
    } else if (s_cached_caps.max_bandwidth_mbps >= 10000) {
        s_cached_caps.estimated_latency_us = 50;  // 10GbE: ~50 microseconds
    } else if (s_cached_caps.max_bandwidth_mbps >= 1000) {
        s_cached_caps.estimated_latency_us = 100;  // 1GbE: ~100 microseconds
    } else {
        s_cached_caps.estimated_latency_us = 500;  // Slower networks
    }

    LOG_INFO("Network detection complete: interfaces=%u, mpi=%d, rdma=%d, bandwidth=%u Mbps",
             s_cached_caps.num_interfaces, s_cached_caps.mpi_available, s_cached_caps.rdma_available,
             s_cached_caps.max_bandwidth_mbps);
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool network_detect_capabilities(network_capabilities_t* caps)
{
    if (!caps) {
        LOG_ERROR("NULL capabilities pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_detect_capabilities: caps is NULL");
        return false;
    }

    // Thread-safe one-time initialization using pthread_once
    pthread_once(&s_cache_init_once, network_detect_init_impl);

    // Copy cached result to caller's buffer
    memcpy(caps, &s_cached_caps, sizeof(network_capabilities_t));
    return true;
}

bool network_has_mpi(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, network_detect_init_impl);
    return s_cached_caps.mpi_available;
}

bool network_is_mpi_process(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, network_detect_init_impl);
    return s_cached_caps.is_mpi_initialized || (s_cached_caps.num_procs > 1);
}

uint32_t network_get_num_nodes(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, network_detect_init_impl);
    return s_cached_caps.num_nodes;
}

const char* network_mpi_impl_name(mpi_impl_t impl)
{
    switch (impl) {
        case MPI_IMPL_NONE:         return "None";
        case MPI_IMPL_OPENMPI:      return "Open MPI";
        case MPI_IMPL_MPICH:        return "MPICH";
        case MPI_IMPL_INTEL_MPI:    return "Intel MPI";
        case MPI_IMPL_MVAPICH2:     return "MVAPICH2";
        case MPI_IMPL_CRAY_MPICH:   return "Cray MPICH";
        case MPI_IMPL_MICROSOFT_MPI: return "Microsoft MPI";
        case MPI_IMPL_OTHER:        return "Other";
        default:                    return "Unknown";
    }
}

const char* network_transport_name(network_transport_t transport)
{
    switch (transport) {
        case NETWORK_TRANSPORT_NONE:       return "None";
        case NETWORK_TRANSPORT_TCP:        return "TCP";
        case NETWORK_TRANSPORT_UDP:        return "UDP";
        case NETWORK_TRANSPORT_RDMA:       return "RDMA";
        case NETWORK_TRANSPORT_SHARED_MEM: return "Shared Memory";
        case NETWORK_TRANSPORT_NVLINK:     return "NVLink";
        case NETWORK_TRANSPORT_INFINITY:   return "InfiniBand";
        default:                           return "Unknown";
    }
}

uint32_t network_get_bandwidth(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, network_detect_init_impl);
    return s_cached_caps.max_bandwidth_mbps;
}

bool network_mpi_init(void)
{
#ifdef NIMCP_ENABLE_MPI
    int initialized = 0;
    MPI_Initialized(&initialized);

    if (!initialized) {
        int argc = 0;
        char** argv = NULL;
        int result = MPI_Init(&argc, &argv);
        if (result == MPI_SUCCESS) {
            s_mpi_initialized_by_us = 1;
            // Re-detect MPI runtime state after initialization
            // (pthread_once has already run, so update cache directly)
            detect_mpi_runtime(&s_cached_caps);
            LOG_INFO("MPI initialized by network_mpi_init()");
            return true;
        }
        LOG_ERROR("MPI_Init failed with error %d", result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "MPI initialization failed with error %d", result);
        return false;
    }
    return true;  // Already initialized
#else
    LOG_WARN("MPI support not compiled in");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "network_mpi_init: operation failed");
    return false;
#endif
}

void network_mpi_finalize(void)
{
#ifdef NIMCP_ENABLE_MPI
    if (s_mpi_initialized_by_us) {
        int finalized = 0;
        MPI_Finalized(&finalized);

        if (!finalized) {
            MPI_Finalize();
            s_mpi_initialized_by_us = 0;
            LOG_INFO("MPI finalized");
        }
    }
#endif
}

bool network_is_gpu_aware(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, network_detect_init_impl);
    return s_cached_caps.cuda_aware_mpi || s_cached_caps.rocm_aware_mpi;
}
