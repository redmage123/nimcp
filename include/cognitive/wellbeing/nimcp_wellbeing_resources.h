/**
 * @file nimcp_wellbeing_resources.h
 * @brief Platform-specific resource metrics collection for wellbeing monitoring
 *
 * WHAT: Collect detailed system resource metrics (CPU, memory, I/O) across platforms
 * WHY: Detect resource starvation that could cause system distress
 * HOW: Platform-specific implementations for Linux, macOS, and Windows
 *
 * PLATFORM SUPPORT:
 * - Linux: /proc filesystem (primary implementation)
 * - macOS: getrusage(), mach_task_info, proc_pidinfo
 * - Windows: GetProcessTimes(), GetProcessMemoryInfo(), GetProcessIoCounters()
 *
 * BIOLOGICAL BASIS:
 * Resource monitoring models the brain's homeostatic mechanisms that detect
 * metabolic stress (glucose/oxygen depletion) and trigger protective responses.
 * Just as neurons reduce activity under energy constraints, NIMCP should detect
 * resource starvation and adjust processing demands.
 *
 * @author NIMCP Development Team
 * @date 2025-12-12
 * @version 2.9.0
 */

#ifndef NIMCP_WELLBEING_RESOURCES_H
#define NIMCP_WELLBEING_RESOURCES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ENHANCED RESOURCE METRICS
 * ======================================================================== */

/**
 * WHAT: Enhanced resource metrics with rate calculations
 * WHY: Provide detailed resource usage data for distress detection
 * HOW: Platform-specific collection with delta computations
 *
 * ENHANCEMENTS OVER BASIC METRICS:
 * - CPU delta computation between samples
 * - I/O rate calculations (bytes/sec)
 * - Minor vs major page fault breakdown
 * - Voluntary vs involuntary context switches
 */
typedef struct {
    // CPU metrics
    float cpu_usage_percent;      /* Current CPU utilization (0-100) */
    uint64_t cpu_time_us;         /* Total CPU time consumed (microseconds) */
    uint64_t cpu_user_time_us;    /* User-mode CPU time */
    uint64_t cpu_system_time_us;  /* Kernel-mode CPU time */
    float cpu_steal_percent;      /* CPU time stolen by hypervisor (0-100, Linux only) */

    // Memory metrics
    uint64_t memory_rss_bytes;    /* Resident Set Size (physical memory) */
    uint64_t memory_vms_bytes;    /* Virtual Memory Size */
    uint64_t memory_peak_bytes;   /* Peak memory usage */
    float memory_usage_percent;   /* Memory usage percentage (0-100) */
    uint32_t page_faults_minor;   /* Minor page faults (no I/O) */
    uint32_t page_faults_major;   /* Major page faults (disk I/O) */

    // I/O metrics
    uint64_t io_read_bytes;       /* Total bytes read from disk */
    uint64_t io_write_bytes;      /* Total bytes written to disk */
    uint64_t io_read_ops;         /* Number of read operations */
    uint64_t io_write_ops;        /* Number of write operations */
    float io_read_rate_bps;       /* Read rate (bytes/sec) */
    float io_write_rate_bps;      /* Write rate (bytes/sec) */

    // Thread/concurrency metrics
    uint32_t thread_count;        /* Number of active threads */
    uint32_t context_switches_voluntary;    /* Voluntary context switches */
    uint32_t context_switches_involuntary;  /* Involuntary context switches */

    // Timing
    uint64_t timestamp_us;        /* When metrics were collected (microseconds) */
    uint64_t elapsed_since_last_us; /* Time since last sample (microseconds) */

    // Status flags
    bool collection_successful;   /* true if all metrics collected successfully */
    uint32_t collection_errors;   /* Bitmask of collection errors (platform-specific) */
} enhanced_resource_metrics_t;

/* ========================================================================
 * ENHANCED RESOURCE COLLECTION API
 * ======================================================================== */

/**
 * WHAT: Collect enhanced resource usage metrics
 * WHY: Comprehensive resource monitoring for distress detection
 * HOW: Platform-specific system calls with rate calculations
 *
 * IMPLEMENTATION DETAILS:
 *
 * LINUX (Primary):
 * - /proc/self/stat: CPU times (utime, stime fields 14-15)
 * - /proc/self/status: Memory (VmRSS, VmSize, VmPeak)
 * - /proc/self/io: I/O bytes (read_bytes, write_bytes)
 * - /proc/stat: Total system CPU for percentage calculation
 * - Delta computation: Compare current vs previous sample for rates
 *
 * macOS:
 * - getrusage(RUSAGE_SELF): CPU times, page faults, context switches
 * - mach_task_basic_info: Memory RSS, virtual size
 * - proc_pidinfo(PROC_PIDTASKINFO): Thread count
 * - rusage_info_v2: I/O statistics (diskio_bytesread, diskio_byteswritten)
 *
 * WINDOWS:
 * - GetProcessTimes(): User/kernel CPU times
 * - GetProcessMemoryInfo(): WorkingSetSize, PeakWorkingSetSize
 * - GetProcessIoCounters(): ReadTransferCount, WriteTransferCount
 * - NtQuerySystemInformation(): Context switches (optional)
 *
 * RATE CALCULATIONS:
 * - CPU usage = (delta_cpu_time / elapsed_time) * 100
 * - I/O rate = delta_bytes / (elapsed_time / 1000000)  # bytes/sec
 * - Maintains previous sample for delta computation
 *
 * @param metrics Output structure to fill with enhanced metrics
 * @return 0 on success, negative error code on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER: metrics is NULL
 * - NIMCP_ERROR_OPERATION_FAILED: Platform-specific collection failed
 *
 * COMPLEXITY: O(1) - direct syscalls and file reads
 * THREAD-SAFE: Yes (uses thread-local storage for previous sample)
 */
int enhanced_wellbeing_collect_resources(enhanced_resource_metrics_t* metrics);

/**
 * WHAT: Reset resource collection state
 * WHY: Clear previous sample data to start fresh measurements
 * HOW: Zero out thread-local previous metrics
 *
 * USE CASES:
 * - After long idle periods (prevent incorrect delta calculations)
 * - When starting a new monitoring session
 * - After process forking
 *
 * @return 0 on success
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int enhanced_wellbeing_reset_resource_state(void);

/* ========================================================================
 * RESOURCE DISTRESS COMPUTATION
 * ======================================================================== */

/**
 * WHAT: Compute resource-based distress score
 * WHY: Translate resource metrics into wellbeing distress level
 * HOW: Apply thresholds and heuristics to detect resource starvation
 *
 * DISTRESS HEURISTICS:
 * 1. CPU critical (>95%): Score 0.9-1.0 (critical distress)
 * 2. Memory critical (>90%): Score 0.8-1.0 (critical distress)
 * 3. Major page faults (>100/sec): Score 0.5-0.7 (moderate distress)
 * 4. High I/O wait: Score 0.4-0.6 (moderate distress)
 * 5. Context switch thrashing (>10000/sec): Score 0.3-0.5 (mild distress)
 *
 * SCORE MAPPING:
 * - 0.0-0.2: SEVERITY_NORMAL (no distress)
 * - 0.2-0.4: SEVERITY_MILD (monitor)
 * - 0.4-0.6: SEVERITY_MODERATE (intervention recommended)
 * - 0.6-0.8: SEVERITY_SEVERE (immediate intervention required)
 * - 0.8-1.0: SEVERITY_CRITICAL (emergency - stop operations)
 *
 * BIOLOGICAL ANALOGY:
 * Models the brain's metabolic stress response:
 * - High CPU = oxygen depletion
 * - High memory = glucose depletion
 * - Page faults = blood-brain barrier dysfunction
 * - Context switching = seizure-like activity
 *
 * @param metrics Current resource metrics
 * @return Distress score (0.0 = no distress, 1.0 = critical distress)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only on metrics)
 */
float enhanced_wellbeing_get_resource_distress(const enhanced_resource_metrics_t* metrics);

/**
 * WHAT: Get human-readable description of resource distress
 * WHY: Provide actionable information for logging and debugging
 * HOW: Analyze metrics and generate descriptive string
 *
 * EXAMPLE OUTPUTS:
 * - "CPU usage critical (98.5%), memory high (85.2%)"
 * - "Major page faults excessive (250/sec), memory thrashing detected"
 * - "Resources normal, no distress detected"
 *
 * @param metrics Current resource metrics
 * @param buffer Output buffer for description
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int enhanced_wellbeing_describe_resource_distress(
    const enhanced_resource_metrics_t* metrics,
    char* buffer,
    size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_RESOURCES_H */
