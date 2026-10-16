/**
 * @file nimcp_wellbeing_resources.c
 * @brief Platform-specific resource metrics implementation
 *
 * WHAT: Collect detailed resource metrics across Linux, macOS, and Windows
 * WHY: Enable resource-based distress detection for wellbeing monitoring
 * HOW: Platform-specific syscalls with delta calculations and rate computations
 *
 * IMPLEMENTATION NOTES:
 * - Linux: Primary implementation using /proc filesystem
 * - macOS: Secondary implementation using BSD APIs
 * - Windows: Tertiary implementation using Win32 APIs
 * - Thread-local storage for previous sample (enables rate calculations)
 */

#include "cognitive/wellbeing/nimcp_wellbeing_resources.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_resources, MESH_ADAPTER_CATEGORY_COGNITIVE)




// Platform-specific includes
#if defined(__linux__)
    #include <unistd.h>
    #include <sys/resource.h>
    #include <sys/time.h>
#elif defined(__APPLE__)
    #include <unistd.h>
    #include <sys/resource.h>
    #include <sys/time.h>
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <mach/task_info.h>
    #include <libproc.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <psapi.h>
    #pragma comment(lib, "psapi.lib")
#endif

//=============================================================================
// THREAD-LOCAL STORAGE FOR DELTA CALCULATIONS
//=============================================================================

/**
 * WHAT: Previous sample for delta computations
 * WHY: Compute rates (CPU usage, I/O rates) between samples
 * HOW: Thread-local storage maintains per-thread history
 */
static _Thread_local enhanced_resource_metrics_t prev_metrics = {0};
static _Thread_local bool has_previous_sample = false;

//=============================================================================
// PLATFORM-SPECIFIC COLLECTION HELPERS
//=============================================================================

#if defined(__linux__)

/**
 * WHAT: Collect resource metrics on Linux
 * WHY: Linux is primary target platform
 * HOW: Read /proc/self/{stat,status,io} for comprehensive metrics
 *
 * PROC FILESYSTEM:
 * - /proc/self/stat: Process statistics (fields 1-52)
 *   Field 14: utime (user CPU time in clock ticks)
 *   Field 15: stime (system CPU time in clock ticks)
 *   Field 20: num_threads (thread count)
 *   Field 24: vsize (virtual memory size in bytes)
 *   Field 42-43: guest_time, cguest_time (VM CPU steal time)
 *
 * - /proc/self/status: Human-readable process status
 *   VmRSS: Resident Set Size (physical memory)
 *   VmSize: Virtual memory size
 *   VmPeak: Peak virtual memory
 *
 * - /proc/self/io: I/O statistics (requires CONFIG_TASK_IO_ACCOUNTING)
 *   read_bytes: Bytes read from storage
 *   write_bytes: Bytes written to storage
 *   syscr: Read syscall count
 *   syscw: Write syscall count
 *
 * @param metrics Output structure to fill
 * @return 0 on success, negative error code on failure
 */
static int collect_linux_metrics(enhanced_resource_metrics_t* metrics)
{
    FILE* fp;
    char line[NIMCP_ERROR_BUFFER_LARGE];
    unsigned long utime = 0, stime = 0;
    unsigned long vsize = 0;
    long rss = 0;
    unsigned long num_threads = 0;
    unsigned long guest_time = 0;
    int values_parsed = 0;

    // Get clock ticks per second for time conversion
    long clock_ticks = sysconf(_SC_CLK_TCK);
    if (clock_ticks <= 0) {
        clock_ticks = 100; // Fallback to common value
    }

    //-------------------------------------------------------------------------
    // Parse /proc/self/stat
    //-------------------------------------------------------------------------
    fp = fopen("/proc/self/stat", "r");
    if (!fp) {
        NIMCP_LOGGING_ERROR("Failed to open /proc/self/stat");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (fgets(line, sizeof(line), fp)) {
        // Parse fields: PID (comm) state ppid pgrp ... utime stime ...
        // Fields 14 and 15 are utime and stime
        char comm[NIMCP_ERROR_BUFFER_SIZE];
        char state;
        int pid, ppid, pgrp, session, tty_nr, tpgid;
        unsigned long flags, minflt, cminflt, majflt, cmajflt;
        unsigned long cutime, cstime;
        long priority, nice, zero_placeholder, itrealvalue;
        unsigned long long starttime;

        values_parsed = sscanf(line,
            "%d %s %c %d %d %d %d %d %lu %lu %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %lld %lu %ld",
            &pid, comm, &state, &ppid, &pgrp, &session, &tty_nr, &tpgid,
            &flags, &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime,
            &cutime, &cstime, &priority, &nice, &num_threads, &itrealvalue,
            &starttime, &vsize, &rss);

        if (values_parsed >= 15) {
            // Convert clock ticks to microseconds
            metrics->cpu_user_time_us = (utime * 1000000UL) / clock_ticks;
            metrics->cpu_system_time_us = (stime * 1000000UL) / clock_ticks;
            metrics->cpu_time_us = metrics->cpu_user_time_us + metrics->cpu_system_time_us;
        }

        if (values_parsed >= 20) {
            metrics->thread_count = (uint32_t)num_threads;
        }

        if (values_parsed >= 23) {
            metrics->memory_vms_bytes = vsize;
            metrics->memory_rss_bytes = rss * sysconf(_SC_PAGESIZE);
        }

        // Store page faults
        metrics->page_faults_minor = (uint32_t)minflt;
        metrics->page_faults_major = (uint32_t)majflt;
    }
    fclose(fp);

    //-------------------------------------------------------------------------
    // Parse /proc/self/status for accurate memory info
    //-------------------------------------------------------------------------
    fp = fopen("/proc/self/status", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            unsigned long kb_value;

            if (sscanf(line, "VmRSS: %lu kB", &kb_value) == 1) {
                metrics->memory_rss_bytes = kb_value * 1024;
            } else if (sscanf(line, "VmSize: %lu kB", &kb_value) == 1) {
                metrics->memory_vms_bytes = kb_value * 1024;
            } else if (sscanf(line, "VmPeak: %lu kB", &kb_value) == 1) {
                metrics->memory_peak_bytes = kb_value * 1024;
            }
        }
        fclose(fp);
    }

    //-------------------------------------------------------------------------
    // Parse /proc/self/io for I/O statistics
    //-------------------------------------------------------------------------
    fp = fopen("/proc/self/io", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            unsigned long long bytes;
            unsigned long ops;

            if (sscanf(line, "read_bytes: %llu", &bytes) == 1) {
                metrics->io_read_bytes = (uint64_t)bytes;
            } else if (sscanf(line, "write_bytes: %llu", &bytes) == 1) {
                metrics->io_write_bytes = (uint64_t)bytes;
            } else if (sscanf(line, "syscr: %lu", &ops) == 1) {
                metrics->io_read_ops = (uint64_t)ops;
            } else if (sscanf(line, "syscw: %lu", &ops) == 1) {
                metrics->io_write_ops = (uint64_t)ops;
            }
        }
        fclose(fp);
    }

    //-------------------------------------------------------------------------
    // Get context switch counts from /proc/self/status
    //-------------------------------------------------------------------------
    fp = fopen("/proc/self/status", "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            unsigned long voluntary, involuntary;

            if (sscanf(line, "voluntary_ctxt_switches: %lu", &voluntary) == 1) {
                metrics->context_switches_voluntary = (uint32_t)voluntary;
            } else if (sscanf(line, "nonvoluntary_ctxt_switches: %lu", &involuntary) == 1) {
                metrics->context_switches_involuntary = (uint32_t)involuntary;
            }
        }
        fclose(fp);
    }

    // Calculate memory percentage (approximation - use RSS vs total system RAM)
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        unsigned long total_ram_kb = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %lu kB", &total_ram_kb) == 1) {
                if (total_ram_kb > 0) {
                    metrics->memory_usage_percent =
                        (float)(metrics->memory_rss_bytes / 1024.0 / total_ram_kb) * 100.0f;
                }
                break;
            }
        }
        fclose(fp);
    }

    metrics->collection_successful = true;
    return 0;
}

#elif defined(__APPLE__)

/**
 * WHAT: Collect resource metrics on macOS
 * WHY: Support macOS as secondary platform
 * HOW: Use getrusage(), mach_task_info, and proc_pidinfo APIs
 *
 * macOS APIS:
 * - getrusage(RUSAGE_SELF): CPU times, page faults, context switches
 * - mach_task_basic_info: Memory RSS, virtual size
 * - proc_pidinfo(PROC_PIDTASKINFO): Thread count
 * - rusage_info_v2: I/O statistics (10.9+)
 *
 * @param metrics Output structure to fill
 * @return 0 on success, negative error code on failure
 */
static int collect_macos_metrics(enhanced_resource_metrics_t* metrics)
{
    struct rusage usage;

    //-------------------------------------------------------------------------
    // Get CPU times and page faults from getrusage
    //-------------------------------------------------------------------------
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // Convert timeval to microseconds
        metrics->cpu_user_time_us =
            (uint64_t)usage.ru_utime.tv_sec * 1000000UL + usage.ru_utime.tv_usec;
        metrics->cpu_system_time_us =
            (uint64_t)usage.ru_stime.tv_sec * 1000000UL + usage.ru_stime.tv_usec;
        metrics->cpu_time_us = metrics->cpu_user_time_us + metrics->cpu_system_time_us;

        // Page faults
        metrics->page_faults_minor = (uint32_t)usage.ru_minflt;
        metrics->page_faults_major = (uint32_t)usage.ru_majflt;

        // Context switches
        metrics->context_switches_voluntary = (uint32_t)usage.ru_nvcsw;
        metrics->context_switches_involuntary = (uint32_t)usage.ru_nivcsw;

        // I/O operations (counts)
        metrics->io_read_ops = (uint64_t)usage.ru_inblock;
        metrics->io_write_ops = (uint64_t)usage.ru_oublock;
    }

    //-------------------------------------------------------------------------
    // Get memory info from mach_task_basic_info
    //-------------------------------------------------------------------------
    mach_task_basic_info_data_t task_info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&task_info, &count) == KERN_SUCCESS) {
        metrics->memory_rss_bytes = task_info.resident_size;
        metrics->memory_vms_bytes = task_info.virtual_size;

        // macOS doesn't easily expose peak memory, use current RSS as approximation
        if (metrics->memory_rss_bytes > metrics->memory_peak_bytes) {
            metrics->memory_peak_bytes = metrics->memory_rss_bytes;
        }
    }

    //-------------------------------------------------------------------------
    // Get thread count from proc_pidinfo
    //-------------------------------------------------------------------------
    struct proc_taskinfo pti;
    if (proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &pti, sizeof(pti)) > 0) {
        metrics->thread_count = pti.pti_threadnum;
    }

    //-------------------------------------------------------------------------
    // Get I/O bytes from rusage_info_v2 (macOS 10.9+)
    //-------------------------------------------------------------------------
#if defined(__APPLE__) && defined(RUSAGE_INFO_V2)
    struct rusage_info_v2 ri;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V2, (rusage_info_t *)&ri) == 0) {
        metrics->io_read_bytes = ri.ri_diskio_bytesread;
        metrics->io_write_bytes = ri.ri_diskio_byteswritten;
    }
#endif

    // Calculate memory percentage (use physical memory size)
    int64_t physical_memory = 0;
    size_t len = sizeof(physical_memory);
    if (sysctlbyname("hw.memsize", &physical_memory, &len, NULL, 0) == 0 && physical_memory > 0) {
        metrics->memory_usage_percent =
            (float)((double)metrics->memory_rss_bytes / physical_memory) * 100.0f;
    }

    metrics->collection_successful = true;
    return 0;
}

#elif defined(_WIN32)

/**
 * WHAT: Collect resource metrics on Windows
 * WHY: Support Windows as tertiary platform
 * HOW: Use Win32 Process APIs
 *
 * WINDOWS APIS:
 * - GetProcessTimes(): User/kernel CPU times
 * - GetProcessMemoryInfo(): Working set, peak working set
 * - GetProcessIoCounters(): I/O bytes and operations
 *
 * @param metrics Output structure to fill
 * @return 0 on success, negative error code on failure
 */
static int collect_windows_metrics(enhanced_resource_metrics_t* metrics)
{
    HANDLE process = GetCurrentProcess();

    //-------------------------------------------------------------------------
    // Get CPU times from GetProcessTimes
    //-------------------------------------------------------------------------
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(process, &creation_time, &exit_time, &kernel_time, &user_time)) {
        // FILETIME is in 100-nanosecond intervals
        ULARGE_INTEGER user_li, kernel_li;
        user_li.LowPart = user_time.dwLowDateTime;
        user_li.HighPart = user_time.dwHighDateTime;
        kernel_li.LowPart = kernel_time.dwLowDateTime;
        kernel_li.HighPart = kernel_time.dwHighDateTime;

        // Convert to microseconds
        metrics->cpu_user_time_us = user_li.QuadPart / 10;
        metrics->cpu_system_time_us = kernel_li.QuadPart / 10;
        metrics->cpu_time_us = metrics->cpu_user_time_us + metrics->cpu_system_time_us;
    }

    //-------------------------------------------------------------------------
    // Get memory info from GetProcessMemoryInfo
    //-------------------------------------------------------------------------
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(process, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        metrics->memory_rss_bytes = pmc.WorkingSetSize;
        metrics->memory_peak_bytes = pmc.PeakWorkingSetSize;
        metrics->memory_vms_bytes = pmc.PrivateUsage;
        metrics->page_faults_minor = pmc.PageFaultCount; // Windows doesn't distinguish minor/major
        metrics->page_faults_major = 0;
    }

    //-------------------------------------------------------------------------
    // Get I/O counters from GetProcessIoCounters
    //-------------------------------------------------------------------------
    IO_COUNTERS io_counters;
    if (GetProcessIoCounters(process, &io_counters)) {
        metrics->io_read_bytes = io_counters.ReadTransferCount;
        metrics->io_write_bytes = io_counters.WriteTransferCount;
        metrics->io_read_ops = io_counters.ReadOperationCount;
        metrics->io_write_ops = io_counters.WriteOperationCount;
    }

    // Get system memory info for percentage calculation
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        if (mem_status.ullTotalPhys > 0) {
            metrics->memory_usage_percent =
                (float)((double)metrics->memory_rss_bytes / mem_status.ullTotalPhys) * 100.0f;
        }
    }

    // Thread count - requires additional API call (not implemented for simplicity)
    metrics->thread_count = 1; // Placeholder

    metrics->collection_successful = true;
    return 0;
}

#else
#error "Unsupported platform for resource metrics collection"
#endif

//=============================================================================
// DELTA AND RATE CALCULATIONS
//=============================================================================

/**
 * WHAT: Compute CPU usage percentage from time delta
 * WHY: Convert absolute CPU time to utilization percentage
 * HOW: (delta_cpu_time / elapsed_time) * 100
 *
 * FORMULA:
 *   cpu_usage = (curr_cpu_time - prev_cpu_time) / (curr_timestamp - prev_timestamp) * 100
 *
 * NOTES:
 * - Returns 0-100 for single-core usage
 * - Can exceed 100% on multi-core systems
 * - Clamps to reasonable maximum (800% = 8 cores)
 *
 * @param prev_cpu_time_us Previous CPU time in microseconds
 * @param curr_cpu_time_us Current CPU time in microseconds
 * @param elapsed_us Elapsed time between samples in microseconds
 * @return CPU usage percentage (0-800)
 */
static float compute_cpu_usage(uint64_t prev_cpu_time_us,
                               uint64_t curr_cpu_time_us,
                               uint64_t elapsed_us)
{
    // Guard: Prevent division by zero
    if (elapsed_us == 0) {
        return 0.0f;
    }

    // Guard: Prevent negative delta (clock skew)
    if (curr_cpu_time_us < prev_cpu_time_us) {
        return 0.0f;
    }

    uint64_t delta_cpu = curr_cpu_time_us - prev_cpu_time_us;
    float usage = ((float)delta_cpu / (float)elapsed_us) * 100.0f;

    // Clamp to reasonable maximum (8 cores = 800%)
    if (usage > 800.0f) {
        usage = 800.0f;
    }

    return usage;
}

/**
 * WHAT: Compute I/O rate in bytes per second
 * WHY: Detect I/O-intensive operations that could cause distress
 * HOW: delta_bytes / (elapsed_time / 1000000)
 *
 * @param prev_bytes Previous I/O byte count
 * @param curr_bytes Current I/O byte count
 * @param elapsed_us Elapsed time in microseconds
 * @return I/O rate in bytes per second
 */
static float compute_io_rate(uint64_t prev_bytes, uint64_t curr_bytes, uint64_t elapsed_us)
{
    // Guard: Prevent division by zero
    if (elapsed_us == 0) {
        return 0.0f;
    }

    // Guard: Prevent negative delta
    if (curr_bytes < prev_bytes) {
        return 0.0f;
    }

    uint64_t delta_bytes = curr_bytes - prev_bytes;
    float elapsed_sec = (float)elapsed_us / 1000000.0f;

    return (float)delta_bytes / elapsed_sec;
}

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

int enhanced_wellbeing_collect_resources(enhanced_resource_metrics_t* metrics)
{
    // Guard: Null pointer check
    if (!metrics) {
        NIMCP_LOGGING_ERROR("NULL metrics pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Initialize structure
    /* Phase 8: Heartbeat at operation start */
    wellbeing_resources_heartbeat("wellbeing_re_enhanced_wellbeing_c", 0.0f);


    memset(metrics, 0, sizeof(enhanced_resource_metrics_t));
    metrics->timestamp_us = nimcp_time_get_us();

    // Collect platform-specific metrics
    int result = 0;
#if defined(__linux__)
    result = collect_linux_metrics(metrics);
#elif defined(__APPLE__)
    result = collect_macos_metrics(metrics);
#elif defined(_WIN32)
    result = collect_windows_metrics(metrics);
#else
    NIMCP_LOGGING_ERROR("Unsupported platform");
    return NIMCP_ERROR_NOT_IMPLEMENTED;
#endif

    // Guard: Check if collection failed
    if (result != 0) {
        metrics->collection_successful = false;
        return result;
    }

    //-------------------------------------------------------------------------
    // Compute deltas and rates if we have a previous sample
    //-------------------------------------------------------------------------
    if (has_previous_sample) {
        metrics->elapsed_since_last_us = metrics->timestamp_us - prev_metrics.timestamp_us;

        // CPU usage percentage
        metrics->cpu_usage_percent = compute_cpu_usage(
            prev_metrics.cpu_time_us,
            metrics->cpu_time_us,
            metrics->elapsed_since_last_us
        );

        // I/O rates
        metrics->io_read_rate_bps = compute_io_rate(
            prev_metrics.io_read_bytes,
            metrics->io_read_bytes,
            metrics->elapsed_since_last_us
        );

        metrics->io_write_rate_bps = compute_io_rate(
            prev_metrics.io_write_bytes,
            metrics->io_write_bytes,
            metrics->elapsed_since_last_us
        );
    } else {
        // First sample - no deltas available
        metrics->elapsed_since_last_us = 0;
        metrics->cpu_usage_percent = 0.0f;
        metrics->io_read_rate_bps = 0.0f;
        metrics->io_write_rate_bps = 0.0f;
    }

    // Store current sample as previous for next call
    memcpy(&prev_metrics, metrics, sizeof(enhanced_resource_metrics_t));
    has_previous_sample = true;

    return 0;
}

int enhanced_wellbeing_reset_resource_state(void)
{
    /* Phase 8: Heartbeat at operation start */
    wellbeing_resources_heartbeat("wellbeing_re_enhanced_wellbeing_r", 0.0f);


    memset(&prev_metrics, 0, sizeof(enhanced_resource_metrics_t));
    has_previous_sample = false;
    return 0;
}

float enhanced_wellbeing_get_resource_distress(const enhanced_resource_metrics_t* metrics)
{
    // Guard: Null pointer check
    if (!metrics) {
        return 0.0f;
    }

    // Guard: Collection must have succeeded
    if (!metrics->collection_successful) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_resources_heartbeat("wellbeing_re_enhanced_wellbeing_g", 0.0f);


    float distress_score = 0.0f;

    //-------------------------------------------------------------------------
    // CPU Distress (0.0 - 0.4)
    //-------------------------------------------------------------------------
    if (metrics->cpu_usage_percent > 95.0f) {
        // Critical CPU usage
        distress_score += 0.4f;
    } else if (metrics->cpu_usage_percent > 80.0f) {
        // High CPU usage (proportional)
        float cpu_factor = (metrics->cpu_usage_percent - 80.0f) / 15.0f; // 0.0-1.0
        distress_score += 0.2f + (cpu_factor * 0.2f); // 0.2-0.4
    }

    //-------------------------------------------------------------------------
    // Memory Distress (0.0 - 0.4)
    //-------------------------------------------------------------------------
    if (metrics->memory_usage_percent > 90.0f) {
        // Critical memory usage
        distress_score += 0.4f;
    } else if (metrics->memory_usage_percent > 75.0f) {
        // High memory usage (proportional)
        float mem_factor = (metrics->memory_usage_percent - 75.0f) / 15.0f; // 0.0-1.0
        distress_score += 0.2f + (mem_factor * 0.2f); // 0.2-0.4
    }

    //-------------------------------------------------------------------------
    // Page Fault Distress (0.0 - 0.2)
    //-------------------------------------------------------------------------
    if (metrics->elapsed_since_last_us > 0) {
        // Calculate major page faults per second
        float elapsed_sec = (float)metrics->elapsed_since_last_us / 1000000.0f;
        float major_pf_rate = (float)metrics->page_faults_major / elapsed_sec;

        if (major_pf_rate > 100.0f) {
            // Excessive page faults indicate memory thrashing
            distress_score += 0.2f;
        } else if (major_pf_rate > 50.0f) {
            float pf_factor = (major_pf_rate - 50.0f) / 50.0f; // 0.0-1.0
            distress_score += 0.1f + (pf_factor * 0.1f); // 0.1-0.2
        }
    }

    // Clamp total distress to [0.0, 1.0]
    if (distress_score > 1.0f) {
        distress_score = 1.0f;
    }

    return distress_score;
}

int enhanced_wellbeing_describe_resource_distress(
    const enhanced_resource_metrics_t* metrics,
    char* buffer,
    size_t buffer_size)
{
    // Guard: Null pointer checks
    if (!metrics || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "enhanced_wellbeing_describe_resource_distress: invalid parameters");

            return 0;
    }

    // Guard: Collection must have succeeded
    if (!metrics->collection_successful) {
        return snprintf(buffer, buffer_size, "Resource collection failed");
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_resources_heartbeat("wellbeing_re_enhanced_wellbeing_d", 0.0f);


    float distress = enhanced_wellbeing_get_resource_distress(metrics);

    // No distress
    if (distress < 0.2f) {
        return snprintf(buffer, buffer_size,
            "Resources normal (CPU: %.1f%%, Memory: %.1f%%)",
            metrics->cpu_usage_percent,
            metrics->memory_usage_percent);
    }

    // Build description based on individual metrics
    char cpu_str[NIMCP_LABEL_BUFFER_SIZE] = "";
    char mem_str[NIMCP_LABEL_BUFFER_SIZE] = "";
    char pf_str[NIMCP_LABEL_BUFFER_SIZE] = "";

    if (metrics->cpu_usage_percent > 80.0f) {
        snprintf(cpu_str, sizeof(cpu_str), "CPU %s (%.1f%%)",
            metrics->cpu_usage_percent > 95.0f ? "critical" : "high",
            metrics->cpu_usage_percent);
    }

    if (metrics->memory_usage_percent > 75.0f) {
        snprintf(mem_str, sizeof(mem_str), "Memory %s (%.1f%%)",
            metrics->memory_usage_percent > 90.0f ? "critical" : "high",
            metrics->memory_usage_percent);
    }

    if (metrics->elapsed_since_last_us > 0) {
        float elapsed_sec = (float)metrics->elapsed_since_last_us / 1000000.0f;
        float major_pf_rate = (float)metrics->page_faults_major / elapsed_sec;

        if (major_pf_rate > 50.0f) {
            snprintf(pf_str, sizeof(pf_str), "Page faults excessive (%.0f/sec)",
                major_pf_rate);
        }
    }

    // Combine non-empty strings
    char combined[NIMCP_ERROR_BUFFER_LARGE] = "Resource distress detected: ";
    bool first = true;

    if (cpu_str[0]) {
        strncat(combined, cpu_str, sizeof(combined) - strlen(combined) - 1);
        first = false;
    }

    if (mem_str[0]) {
        if (!first) strncat(combined, ", ", sizeof(combined) - strlen(combined) - 1);
        strncat(combined, mem_str, sizeof(combined) - strlen(combined) - 1);
        first = false;
    }

    if (pf_str[0]) {
        if (!first) strncat(combined, ", ", sizeof(combined) - strlen(combined) - 1);
        strncat(combined, pf_str, sizeof(combined) - strlen(combined) - 1);
    }

    return snprintf(buffer, buffer_size, "%s", combined);
}

//=============================================================================
// KNOWLEDGE GRAPH SELF-AWARENESS INTEGRATION
//=============================================================================

/**
 * WHAT: Query knowledge graph for Wellbeing Resources module self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int wellbeing_resources_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_resources_heartbeat("wellbeing_re_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Wellbeing_Resources_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_resources_heartbeat("wellbeing_re_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Wellbeing Resources self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Wellbeing_Resources_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Wellbeing_Resources_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void wellbeing_resources_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_resources_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_resources_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_resources_training_begin: NULL argument");
        return -1;
    }
    wellbeing_resources_heartbeat_instance(g_wellbeing_resources_health_agent, "wellbeing_resources_training_begin", 0.0f);
    return 0;
}

int wellbeing_resources_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_resources_training_end: NULL argument");
        return -1;
    }
    wellbeing_resources_heartbeat_instance(g_wellbeing_resources_health_agent, "wellbeing_resources_training_end", 1.0f);
    return 0;
}

int wellbeing_resources_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_resources_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_resources_heartbeat_instance(g_wellbeing_resources_health_agent, "wellbeing_resources_training_step", progress);
    return 0;
}
