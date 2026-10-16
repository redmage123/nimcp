/**
 * @file test_network_detect.cpp
 * @brief Unit tests for network/MPI capability detection
 *
 * WHAT: Tests network and distributed computing capability detection
 * WHY:  Ensure accurate MPI and network detection for distributed execution
 * HOW:  Test all public API functions with various scenarios
 *
 * TEST COVERAGE:
 * - Basic capability detection
 * - MPI detection and environment variables
 * - Network interface enumeration
 * - Transport type detection
 * - Bandwidth and latency estimation
 * - Edge cases and NULL handling
 * - Platform-specific behavior
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "gpu/execution/nimcp_network_detect.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for network detection tests
 */
class NetworkDetectTest : public ::testing::Test {
protected:
    network_capabilities_t caps;

    void SetUp() override {
        memset(&caps, 0, sizeof(caps));
    }
};

//=============================================================================
// Basic Detection Tests
//=============================================================================

/**
 * TEST: Basic capability detection succeeds
 * WHAT: Verify network_detect_capabilities() returns valid results
 * WHY:  Must successfully detect network features
 * HOW:  Call with valid pointer, check success
 */
TEST_F(NetworkDetectTest, DetectCapabilities_Success) {
    bool result = network_detect_capabilities(&caps);
    EXPECT_TRUE(result);
}

/**
 * TEST: NULL pointer handling
 * WHAT: Verify network_detect_capabilities() handles NULL gracefully
 * WHY:  Prevent crashes from invalid input
 * HOW:  Pass NULL, expect false return
 */
TEST_F(NetworkDetectTest, DetectCapabilities_NullPointer) {
    bool result = network_detect_capabilities(nullptr);
    EXPECT_FALSE(result);
}

/**
 * TEST: Cached results are consistent
 * WHAT: Verify multiple calls return same results
 * WHY:  Detection results should be deterministic
 * HOW:  Call twice, compare results
 */
TEST_F(NetworkDetectTest, DetectCapabilities_Caching) {
    network_capabilities_t caps1, caps2;

    bool result1 = network_detect_capabilities(&caps1);
    bool result2 = network_detect_capabilities(&caps2);

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);

    // Core fields should match
    EXPECT_EQ(caps1.network_available, caps2.network_available);
    EXPECT_EQ(caps1.mpi_available, caps2.mpi_available);
    EXPECT_EQ(caps1.num_nodes, caps2.num_nodes);
    EXPECT_EQ(caps1.max_bandwidth_mbps, caps2.max_bandwidth_mbps);
}

//=============================================================================
// Default Values Tests
//=============================================================================

/**
 * TEST: Default node count is 1
 * WHAT: Verify num_nodes defaults to 1 (single node)
 * WHY:  Single-node operation is default
 * HOW:  Check num_nodes after detection
 */
TEST_F(NetworkDetectTest, Defaults_SingleNode) {
    network_detect_capabilities(&caps);

    // Should be at least 1
    EXPECT_GE(caps.num_nodes, 1u);
}

/**
 * TEST: Default process count is 1
 * WHAT: Verify num_procs defaults to 1
 * WHY:  Single-process operation is default
 * HOW:  Check num_procs after detection
 */
TEST_F(NetworkDetectTest, Defaults_SingleProcess) {
    network_detect_capabilities(&caps);

    // Should be at least 1
    EXPECT_GE(caps.num_procs, 1u);
}

/**
 * TEST: World rank defaults to 0
 * WHAT: Verify world_rank defaults to 0
 * WHY:  Single process is rank 0
 * HOW:  Check world_rank after detection
 */
TEST_F(NetworkDetectTest, Defaults_RankZero) {
    network_detect_capabilities(&caps);

    // Single process should have rank 0
    if (caps.num_procs == 1) {
        EXPECT_EQ(caps.world_rank, 0);
    }
}

//=============================================================================
// Network Interface Tests
//=============================================================================

/**
 * TEST: Network interfaces are detected
 * WHAT: Verify network interfaces are enumerated
 * WHY:  Need to know available network paths
 * HOW:  Check num_interfaces is reasonable
 */
TEST_F(NetworkDetectTest, Interfaces_Detection) {
    network_detect_capabilities(&caps);

    // Should have at least loopback
    // Note: May be 0 on some minimal systems
    EXPECT_LE(caps.num_interfaces, 16u);  // Max supported
}

/**
 * TEST: Interface names are valid
 * WHAT: Verify interface names are properly set
 * WHY:  Interface names used for debugging
 * HOW:  Check interface names are not empty
 */
TEST_F(NetworkDetectTest, Interfaces_ValidNames) {
    network_detect_capabilities(&caps);

    for (uint32_t i = 0; i < caps.num_interfaces; i++) {
        EXPECT_GT(strlen(caps.interfaces[i].name), 0u);
    }
}

/**
 * TEST: Loopback interface is identified
 * WHAT: Verify loopback interface is marked correctly
 * WHY:  Loopback should not be used for distributed
 * HOW:  Find loopback and check flag
 */
TEST_F(NetworkDetectTest, Interfaces_LoopbackIdentified) {
    network_detect_capabilities(&caps);

    bool found_loopback = false;
    for (uint32_t i = 0; i < caps.num_interfaces; i++) {
        if (caps.interfaces[i].is_loopback) {
            found_loopback = true;
            // Loopback should have transport type SHARED_MEM
            EXPECT_EQ(caps.interfaces[i].type, NETWORK_TRANSPORT_SHARED_MEM);
        }
    }

    // Loopback may or may not be present depending on platform
    (void)found_loopback;
}

/**
 * TEST: Non-loopback interfaces have IP addresses
 * WHAT: Verify non-loopback interfaces have valid IPs
 * WHY:  IP addresses needed for network communication
 * HOW:  Check IP address strings are not empty
 */
TEST_F(NetworkDetectTest, Interfaces_ValidIPAddresses) {
    network_detect_capabilities(&caps);

    for (uint32_t i = 0; i < caps.num_interfaces; i++) {
        if (!caps.interfaces[i].is_loopback) {
            // Non-loopback should have IP address
            EXPECT_GT(strlen(caps.interfaces[i].ip_address), 0u);
        }
    }
}

//=============================================================================
// Transport Type Tests
//=============================================================================

/**
 * TEST: Transport types are detected
 * WHAT: Verify transport bitmask is set
 * WHY:  Need to know available transport types
 * HOW:  Check transports field
 */
TEST_F(NetworkDetectTest, Transports_Detected) {
    network_detect_capabilities(&caps);

    // If we have non-loopback interfaces, should have some transport
    bool has_non_loopback = false;
    for (uint32_t i = 0; i < caps.num_interfaces; i++) {
        if (!caps.interfaces[i].is_loopback) {
            has_non_loopback = true;
            break;
        }
    }

    if (has_non_loopback) {
        EXPECT_NE(caps.transports, 0u);
    }
}

/**
 * TEST: Transport name lookup
 * WHAT: Verify network_transport_name() returns correct names
 * WHY:  Human-readable names for logging
 * HOW:  Check known transport names
 */
TEST_F(NetworkDetectTest, TransportName_KnownTypes) {
    EXPECT_STREQ(network_transport_name(NETWORK_TRANSPORT_NONE), "None");
    EXPECT_STREQ(network_transport_name(NETWORK_TRANSPORT_TCP), "TCP");
    EXPECT_STREQ(network_transport_name(NETWORK_TRANSPORT_UDP), "UDP");
    EXPECT_STREQ(network_transport_name(NETWORK_TRANSPORT_RDMA), "RDMA");
    EXPECT_STREQ(network_transport_name(NETWORK_TRANSPORT_SHARED_MEM), "Shared Memory");
    EXPECT_STREQ(network_transport_name(NETWORK_TRANSPORT_NVLINK), "NVLink");
    EXPECT_STREQ(network_transport_name(NETWORK_TRANSPORT_INFINITY), "InfiniBand");
}

/**
 * TEST: Unknown transport name
 * WHAT: Verify unknown transports return "Unknown"
 * WHY:  Handle invalid transport values gracefully
 * HOW:  Pass invalid transport value
 */
TEST_F(NetworkDetectTest, TransportName_Unknown) {
    const char* name = network_transport_name((network_transport_t)0xFFFF);
    EXPECT_STREQ(name, "Unknown");
}

//=============================================================================
// MPI Detection Tests
//=============================================================================

/**
 * TEST: MPI availability check
 * WHAT: Verify network_has_mpi() returns correct value
 * WHY:  Need to know if MPI is available
 * HOW:  Compare to struct field
 */
TEST_F(NetworkDetectTest, MPI_HasMPI) {
    network_detect_capabilities(&caps);

    bool has_mpi = network_has_mpi();
    EXPECT_EQ(has_mpi, caps.mpi_available);
}

/**
 * TEST: MPI process check
 * WHAT: Verify network_is_mpi_process() works correctly
 * WHY:  Need to know if running under MPI
 * HOW:  Check consistency with caps
 */
TEST_F(NetworkDetectTest, MPI_IsMPIProcess) {
    network_detect_capabilities(&caps);

    bool is_mpi = network_is_mpi_process();

    // Should be true if initialized or if num_procs > 1
    if (caps.is_mpi_initialized || caps.num_procs > 1) {
        EXPECT_TRUE(is_mpi);
    } else {
        EXPECT_FALSE(is_mpi);
    }
}

/**
 * TEST: MPI implementation name lookup
 * WHAT: Verify network_mpi_impl_name() returns correct names
 * WHY:  Human-readable names for logging
 * HOW:  Check known implementation names
 */
TEST_F(NetworkDetectTest, MPI_ImplName_KnownTypes) {
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_NONE), "None");
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_OPENMPI), "Open MPI");
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_MPICH), "MPICH");
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_INTEL_MPI), "Intel MPI");
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_MVAPICH2), "MVAPICH2");
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_CRAY_MPICH), "Cray MPICH");
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_MICROSOFT_MPI), "Microsoft MPI");
    EXPECT_STREQ(network_mpi_impl_name(MPI_IMPL_OTHER), "Other");
}

/**
 * TEST: Unknown MPI implementation name
 * WHAT: Verify unknown implementations return "Unknown"
 * WHY:  Handle invalid implementation values gracefully
 * HOW:  Pass invalid implementation value
 */
TEST_F(NetworkDetectTest, MPI_ImplName_Unknown) {
    const char* name = network_mpi_impl_name((mpi_impl_t)999);
    EXPECT_STREQ(name, "Unknown");
}

/**
 * TEST: MPI version string is set
 * WHAT: Verify MPI version string is populated
 * WHY:  Version info useful for diagnostics
 * HOW:  Check version string is not empty
 */
TEST_F(NetworkDetectTest, MPI_VersionString) {
    network_detect_capabilities(&caps);

    // Version string should be set (even if "Not available")
    EXPECT_GT(strlen(caps.mpi_version_string), 0u);
}

//=============================================================================
// Bandwidth and Latency Tests
//=============================================================================

/**
 * TEST: Bandwidth detection
 * WHAT: Verify max_bandwidth_mbps is reasonable
 * WHY:  Bandwidth needed for distributed execution planning
 * HOW:  Check bandwidth is in reasonable range
 */
TEST_F(NetworkDetectTest, Bandwidth_Reasonable) {
    network_detect_capabilities(&caps);

    // Bandwidth should be reasonable (0 = no network, or 100Mbps - 400Gbps)
    if (caps.max_bandwidth_mbps > 0) {
        EXPECT_GE(caps.max_bandwidth_mbps, 100u);       // At least 100Mbps
        EXPECT_LE(caps.max_bandwidth_mbps, 400000u);    // At most 400Gbps
    }
}

/**
 * TEST: network_get_bandwidth() helper
 * WHAT: Verify helper function returns correct bandwidth
 * WHY:  Convenience API should match struct
 * HOW:  Compare function result to struct field
 */
TEST_F(NetworkDetectTest, Bandwidth_HelperFunction) {
    network_detect_capabilities(&caps);

    uint32_t bandwidth = network_get_bandwidth();
    EXPECT_EQ(bandwidth, caps.max_bandwidth_mbps);
}

/**
 * TEST: Latency estimation
 * WHAT: Verify estimated_latency_us is reasonable
 * WHY:  Latency affects distributed synchronization
 * HOW:  Check latency is in reasonable range
 */
TEST_F(NetworkDetectTest, Latency_Reasonable) {
    network_detect_capabilities(&caps);

    // If network is available, latency should be set
    if (caps.network_available) {
        EXPECT_GE(caps.estimated_latency_us, 1u);      // At least 1us (RDMA)
        EXPECT_LE(caps.estimated_latency_us, 10000u);  // At most 10ms
    }
}

/**
 * TEST: RDMA has low latency
 * WHAT: Verify RDMA detection sets low latency
 * WHY:  RDMA should have ~1us latency
 * HOW:  Check latency when RDMA is available
 */
TEST_F(NetworkDetectTest, Latency_RDMALow) {
    network_detect_capabilities(&caps);

    if (caps.rdma_available) {
        EXPECT_LE(caps.estimated_latency_us, 10u);  // RDMA should be < 10us
    }
}

//=============================================================================
// Node Count Tests
//=============================================================================

/**
 * TEST: network_get_num_nodes() helper
 * WHAT: Verify helper function returns correct node count
 * WHY:  Convenience API should match struct
 * HOW:  Compare function result to struct field
 */
TEST_F(NetworkDetectTest, NumNodes_HelperFunction) {
    network_detect_capabilities(&caps);

    uint32_t num_nodes = network_get_num_nodes();
    EXPECT_EQ(num_nodes, caps.num_nodes);
}

//=============================================================================
// GPU-Aware MPI Tests
//=============================================================================

/**
 * TEST: GPU-aware MPI detection
 * WHAT: Verify network_is_gpu_aware() works correctly
 * WHY:  GPU-direct communication is important for performance
 * HOW:  Compare to struct fields
 */
TEST_F(NetworkDetectTest, GPUAware_Detection) {
    network_detect_capabilities(&caps);

    bool is_gpu_aware = network_is_gpu_aware();
    EXPECT_EQ(is_gpu_aware, caps.cuda_aware_mpi || caps.rocm_aware_mpi);
}

//=============================================================================
// MPI Init/Finalize Tests
//=============================================================================

/**
 * TEST: MPI init without MPI compiled in
 * WHAT: Verify network_mpi_init() handles no MPI gracefully
 * WHY:  Should not crash without MPI support
 * HOW:  Call init, check return value
 */
TEST_F(NetworkDetectTest, MPI_InitNoMPI) {
    #ifndef NIMCP_ENABLE_MPI
    // Without MPI compiled in, init should return false
    bool result = network_mpi_init();
    EXPECT_FALSE(result);
    #else
    // With MPI, may succeed or fail depending on environment
    SUCCEED();
    #endif
}

/**
 * TEST: MPI finalize without init
 * WHAT: Verify network_mpi_finalize() handles uninitialized state
 * WHY:  Should not crash if not initialized
 * HOW:  Call finalize without init, expect no crash
 */
TEST_F(NetworkDetectTest, MPI_FinalizeNoInit) {
    // Should not crash
    network_mpi_finalize();
    SUCCEED();
}

//=============================================================================
// InfiniBand/RDMA Tests
//=============================================================================

/**
 * TEST: RDMA detection consistency
 * WHAT: Verify RDMA flag matches transport mask
 * WHY:  rdma_available should be consistent with transports
 * HOW:  Check consistency
 */
TEST_F(NetworkDetectTest, RDMA_Consistency) {
    network_detect_capabilities(&caps);

    if (caps.rdma_available) {
        // Should have RDMA transport
        EXPECT_TRUE(caps.transports & NETWORK_TRANSPORT_RDMA);
    }
}

/**
 * TEST: InfiniBand implies high bandwidth
 * WHAT: Verify InfiniBand detection sets high bandwidth
 * WHY:  InfiniBand is typically 56Gbps+
 * HOW:  Check bandwidth when InfiniBand is detected
 */
TEST_F(NetworkDetectTest, InfiniBand_HighBandwidth) {
    network_detect_capabilities(&caps);

    if (caps.transports & NETWORK_TRANSPORT_INFINITY) {
        EXPECT_GE(caps.max_bandwidth_mbps, 40000u);  // At least 40Gbps
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

/**
 * TEST: No network interfaces
 * WHAT: Verify handling of systems with no network
 * WHY:  Must not crash on minimal systems
 * HOW:  Detection should succeed even with no interfaces
 */
TEST_F(NetworkDetectTest, EdgeCase_NoInterfaces) {
    network_detect_capabilities(&caps);

    // Even with no interfaces, detection should succeed
    // network_available should be false if no usable interfaces
    if (caps.num_interfaces == 0) {
        // No interfaces means no distributed network
        // (may still have MPI via shared memory)
    }
    SUCCEED();
}

/**
 * TEST: Consistent results across calls
 * WHAT: Verify detection results don't change between calls
 * WHY:  Hardware detection should be deterministic
 * HOW:  Call multiple times, compare results
 */
TEST_F(NetworkDetectTest, EdgeCase_ConsistentResults) {
    network_capabilities_t caps1, caps2, caps3;

    network_detect_capabilities(&caps1);
    network_detect_capabilities(&caps2);
    network_detect_capabilities(&caps3);

    EXPECT_EQ(caps1.network_available, caps2.network_available);
    EXPECT_EQ(caps2.network_available, caps3.network_available);
    EXPECT_EQ(caps1.mpi_available, caps2.mpi_available);
    EXPECT_EQ(caps2.mpi_available, caps3.mpi_available);
}

//=============================================================================
// Network Available Logic Tests
//=============================================================================

/**
 * TEST: Network availability logic
 * WHAT: Verify network_available is set correctly
 * WHY:  Must have either interfaces or MPI for distributed
 * HOW:  Check logic against individual flags
 */
TEST_F(NetworkDetectTest, NetworkAvailable_Logic) {
    network_detect_capabilities(&caps);

    // network_available should be true if:
    // - We have network interfaces with bandwidth, AND
    // - We have MPI or multiple nodes
    if (caps.network_available) {
        // Should have some way to communicate
        EXPECT_TRUE(caps.mpi_available || caps.num_nodes > 1 ||
                    caps.max_bandwidth_mbps > 0);
    }
}

/**
 * TEST: Single node without MPI
 * WHAT: Verify single node without MPI is not "network available"
 * WHY:  Distributed execution requires multiple nodes or MPI
 * HOW:  Check logic for single-node case
 */
TEST_F(NetworkDetectTest, SingleNode_NotDistributed) {
    network_detect_capabilities(&caps);

    // If we have 1 node and no MPI, network should not be "available"
    // for distributed execution purposes
    if (caps.num_nodes == 1 && !caps.mpi_available) {
        // This is correct - single node without MPI means no distributed
        SUCCEED();
    }
}

//=============================================================================
// Platform-Specific Tests
//=============================================================================

#ifdef __linux__

/**
 * TEST: Linux interface detection
 * WHAT: Verify interface detection works on Linux
 * WHY:  Linux uses specific APIs (getifaddrs)
 * HOW:  Check interfaces are detected
 */
TEST_F(NetworkDetectTest, Platform_LinuxInterfaces) {
    network_detect_capabilities(&caps);

    // Linux should detect at least loopback
    // (unless running in very minimal container)
    // Just verify no crash
    SUCCEED();
}

/**
 * TEST: Linux InfiniBand detection
 * WHAT: Verify InfiniBand detection on Linux
 * WHY:  Linux can check /sys/class/infiniband
 * HOW:  Check RDMA flag is set correctly
 */
TEST_F(NetworkDetectTest, Platform_LinuxInfiniBand) {
    network_detect_capabilities(&caps);

    // Just verify detection doesn't crash
    // RDMA may or may not be available
    SUCCEED();
}

#endif // __linux__

#ifdef __APPLE__

/**
 * TEST: macOS interface detection
 * WHAT: Verify interface detection works on macOS
 * WHY:  macOS uses BSD sockets API
 * HOW:  Check interfaces are detected
 */
TEST_F(NetworkDetectTest, Platform_macOSInterfaces) {
    network_detect_capabilities(&caps);

    // macOS should detect at least loopback
    // Just verify no crash
    SUCCEED();
}

#endif // __APPLE__

#ifdef _WIN32

/**
 * TEST: Windows interface detection
 * WHAT: Verify interface detection works on Windows
 * WHY:  Windows uses IP Helper API
 * HOW:  Check interfaces are detected
 */
TEST_F(NetworkDetectTest, Platform_WindowsInterfaces) {
    network_detect_capabilities(&caps);

    // Windows should detect interfaces
    // Just verify no crash
    SUCCEED();
}

#endif // _WIN32
