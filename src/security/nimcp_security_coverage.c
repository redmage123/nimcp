/**
 * @file nimcp_security_coverage.c
 * @brief Implementation of Security Coverage Framework
 *
 * WHAT: Implements 100% security coverage verification system that ensures
 *       no blind spots exist in the NIMCP security architecture.
 *
 * WHY:  Security gaps are attack vectors. This module provides continuous
 *       verification that all 8 coverage dimensions maintain full protection.
 *
 * HOW:  Tracks registered protections across memory, code, I/O, IPC, temporal,
 *       thread, and external interface dimensions. Provides real-time status
 *       and gap detection for security operations teams.
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include "security/nimcp_security_coverage.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_coverage"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_coverage, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal security coverage context
 */
struct nimcp_security_coverage {
    // Memory regions
    nimcp_protected_region_t regions[NIMCP_MAX_PROTECTED_REGIONS];
    uint32_t num_regions;

    // Code paths
    nimcp_code_path_t code_paths[NIMCP_MAX_CODE_PATHS];
    uint32_t num_code_paths;

    // I/O channels
    nimcp_channel_t channels[NIMCP_MAX_CHANNELS];
    uint32_t num_channels;

    // IPC endpoints
    nimcp_ipc_endpoint_t ipc_endpoints[NIMCP_MAX_IPC_ENDPOINTS];
    uint32_t num_ipc_endpoints;

    // Temporal tracking
    uint64_t start_time;
    uint64_t last_heartbeat;
    uint64_t heartbeat_count;
    uint64_t max_gap_detected;

    // Coverage statistics
    nimcp_dimension_stats_t dimension_stats[NIMCP_COVERAGE_DIMENSION_COUNT];
    uint64_t total_violations;

    // State
    bool initialized;
    bool monitoring_active;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * NIMCP_MS_PER_SEC + ts.tv_nsec / NIMCP_NS_PER_MS);
}

/**
 * @brief Compute FNV-1a hash for memory region
 */
static void compute_region_hash(const void* data, size_t len, uint8_t* hash)
{
    if (!data || !hash || len == 0)
        return;

    const uint64_t FNV_OFFSET = 14695981039346656037ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;

    uint64_t h = FNV_OFFSET;
    const uint8_t* bytes = (const uint8_t*)data;

    for (size_t i = 0; i < len; i++) {
        h ^= bytes[i];
        h *= FNV_PRIME;
    }

    // Expand to 32 bytes
    for (int i = 0; i < 4; i++) {
        uint64_t mix = h * (i + 1);
        memcpy(hash + (i * 8), &mix, 8);
    }
}

/**
 * @brief Initialize dimension statistics
 */
static void init_dimension_stats(nimcp_dimension_stats_t* stats, nimcp_coverage_dimension_t dim)
{
    memset(stats, 0, sizeof(nimcp_dimension_stats_t));
    stats->dimension = dim;
    stats->status = NIMCP_COVERAGE_STATUS_UNKNOWN;
    stats->coverage_percent = 0.0F;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_security_coverage_t* nimcp_security_coverage_create(void)
{
    nimcp_security_coverage_t* coverage =
        (nimcp_security_coverage_t*)nimcp_calloc(1, sizeof(nimcp_security_coverage_t));

    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_security_coverage_create: failed to allocate coverage");
        return NULL;
    }

    // Initialize dimension statistics
    for (int i = 0; i < NIMCP_COVERAGE_DIMENSION_COUNT; i++) {
        init_dimension_stats(&coverage->dimension_stats[i], (nimcp_coverage_dimension_t)i);
    }

    coverage->start_time = get_timestamp_ms();
    coverage->last_heartbeat = coverage->start_time;
    coverage->initialized = false;
    coverage->monitoring_active = false;

    return coverage;
}

nimcp_result_t nimcp_security_coverage_init(nimcp_security_coverage_t* coverage)
{
    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_security_coverage_init: coverage is NULL");
        return NIMCP_INVALID_PARAM;
    }

    if (coverage->initialized)
        return NIMCP_INVALID_STATE;

    // Mark all dimensions as having no coverage initially
    for (int i = 0; i < NIMCP_COVERAGE_DIMENSION_COUNT; i++) {
        coverage->dimension_stats[i].status = NIMCP_COVERAGE_STATUS_NONE;
        coverage->dimension_stats[i].last_check = get_timestamp_ms();
    }

    coverage->initialized = true;
    coverage->monitoring_active = true;

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        "Security coverage monitoring initialized"
    );

    return NIMCP_SUCCESS;
}

void nimcp_security_coverage_destroy(nimcp_security_coverage_t* coverage)
{
    if (!coverage)
        return;

    // Log final coverage report
    nimcp_coverage_report_t report;
    if (nimcp_coverage_verify_all(coverage, &report) == NIMCP_SUCCESS) {
        char msg[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(msg, sizeof(msg),
                 "Coverage monitoring ended: %.1f%% overall, %lu violations",
                 report.overall_coverage, (unsigned long)report.total_violations);
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
            NIMCP_THREAT_NONE,
            msg
        );
    }

    memset(coverage, 0, sizeof(nimcp_security_coverage_t));
    nimcp_free(coverage);
}

//=============================================================================
// Memory Region Registration
//=============================================================================

int32_t nimcp_coverage_register_region(
    nimcp_security_coverage_t* coverage,
    void* base,
    size_t size,
    nimcp_protection_level_t protection,
    const char* name)
{
    if (!coverage || !base || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_register_region: required parameter is NULL (coverage, base)");
        return -1;
    }

    if (coverage->num_regions >= NIMCP_MAX_PROTECTED_REGIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_coverage_register_region: capacity exceeded");
        return -1;
    }

    int32_t region_id = (int32_t)coverage->num_regions;
    nimcp_protected_region_t* region = &coverage->regions[region_id];

    region->base_address = base;
    region->size = size;
    region->protection = protection;
    region->name = name;
    region->active = true;
    region->last_verified = get_timestamp_ms();

    // Compute initial hash for integrity verification
    compute_region_hash(base, size, region->hash);

    // Apply memory protection if requested
    if (protection == NIMCP_PROTECTION_READ_ONLY ||
        protection == NIMCP_PROTECTION_FULL) {
        // Round to page boundaries
        long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) page_size = 4096;

        uintptr_t aligned_base = (uintptr_t)base & ~(page_size - 1);
        size_t aligned_size = size + ((uintptr_t)base - aligned_base);
        aligned_size = ((aligned_size + page_size - 1) / page_size) * page_size;

        // Note: mprotect may fail if memory wasn't mmap'd
        // This is informational - hash verification is the backup
        mprotect((void*)aligned_base, aligned_size, PROT_READ);
    }

    coverage->num_regions++;

    // Update dimension statistics
    coverage->dimension_stats[NIMCP_COVERAGE_MEMORY_REGIONS].total_items++;
    coverage->dimension_stats[NIMCP_COVERAGE_MEMORY_REGIONS].protected_items++;

    return region_id;
}

nimcp_result_t nimcp_coverage_unregister_region(
    nimcp_security_coverage_t* coverage,
    int32_t region_id)
{
    if (!coverage || region_id < 0 || (uint32_t)region_id >= coverage->num_regions)
        return NIMCP_INVALID_PARAM;

    nimcp_protected_region_t* region = &coverage->regions[region_id];

    // Verify integrity before unregistering (audit trail)
    nimcp_coverage_verify_region(coverage, region_id);

    region->active = false;

    // Update statistics
    if (coverage->dimension_stats[NIMCP_COVERAGE_MEMORY_REGIONS].protected_items > 0) {
        coverage->dimension_stats[NIMCP_COVERAGE_MEMORY_REGIONS].protected_items--;
    }

    return NIMCP_SUCCESS;
}

bool nimcp_coverage_verify_region(
    nimcp_security_coverage_t* coverage,
    int32_t region_id)
{
    if (!coverage || region_id < 0 || (uint32_t)region_id >= coverage->num_regions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coverage_verify_region: coverage is NULL");
        return false;
    }

    nimcp_protected_region_t* region = &coverage->regions[region_id];

    if (!region->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_verify_region: region->active is NULL");
        return false;
    }

    // Compute current hash
    uint8_t current_hash[NIMCP_COVERAGE_HASH_SIZE];
    compute_region_hash(region->base_address, region->size, current_hash);

    // Compare with stored hash
    bool intact = (memcmp(current_hash, region->hash, NIMCP_COVERAGE_HASH_SIZE) == 0);

    region->last_verified = get_timestamp_ms();

    if (!intact) {
        coverage->total_violations++;
        coverage->dimension_stats[NIMCP_COVERAGE_MEMORY_REGIONS].violations_detected++;

        char msg[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "Memory region '%s' integrity violation detected",
                 region->name ? region->name : "unnamed");
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_DIRECTIVE_TAMPERED,
            NIMCP_THREAT_CRITICAL,
            msg
        );
    }

    return intact;
}

bool nimcp_coverage_verify_all_regions(nimcp_security_coverage_t* coverage)
{
    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_verify_all_regions: coverage is NULL");
        return false;
    }

    bool all_intact = true;

    for (uint32_t i = 0; i < coverage->num_regions; i++) {
        if (coverage->regions[i].active) {
            if (!nimcp_coverage_verify_region(coverage, (int32_t)i)) {
                all_intact = false;
            }
        }
    }

    return all_intact;
}

//=============================================================================
// Code Path Registration (CFI)
//=============================================================================

nimcp_result_t nimcp_coverage_register_code_path(
    nimcp_security_coverage_t* coverage,
    void* entry_point,
    uint32_t path_id)
{
    if (!coverage || !entry_point)
        return NIMCP_INVALID_PARAM;

    if (coverage->num_code_paths >= NIMCP_MAX_CODE_PATHS)
        return NIMCP_BUFFER_TOO_SMALL;

    nimcp_code_path_t* path = &coverage->code_paths[coverage->num_code_paths];

    path->entry_point = entry_point;
    path->expected_return = NULL;
    path->path_id = path_id;
    path->cfi_enabled = true;
    path->shadow_stack_enabled = true;
    path->call_count = 0;
    path->violation_count = 0;

    coverage->num_code_paths++;

    // Update statistics
    coverage->dimension_stats[NIMCP_COVERAGE_CODE_PATHS].total_items++;
    coverage->dimension_stats[NIMCP_COVERAGE_CODE_PATHS].protected_items++;

    return NIMCP_SUCCESS;
}

bool nimcp_coverage_record_code_path(
    nimcp_security_coverage_t* coverage,
    uint32_t path_id,
    void* return_address)
{
    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_record_code_path: coverage is NULL");
        return false;
    }

    // Find the path
    nimcp_code_path_t* path = NULL;
    for (uint32_t i = 0; i < coverage->num_code_paths; i++) {
        if (coverage->code_paths[i].path_id == path_id) {
            path = &coverage->code_paths[i];
            break;
        }
    }

    if (!path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_record_code_path: path is NULL");
        return false;
    }

    path->call_count++;

    // If we have an expected return address, verify it
    if (path->expected_return && return_address != path->expected_return) {
        path->violation_count++;
        coverage->total_violations++;
        coverage->dimension_stats[NIMCP_COVERAGE_CODE_PATHS].violations_detected++;

        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_CRITICAL,
            "CFI violation: unexpected return address"
        );

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coverage_record_code_path: operation failed");
        return false;
    }

    // Update expected return for next verification
    path->expected_return = return_address;

    return true;
}

//=============================================================================
// I/O Channel Registration
//=============================================================================

int32_t nimcp_coverage_register_input_channel(
    nimcp_security_coverage_t* coverage,
    const char* name,
    bool validation_enabled)
{
    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_register_input_channel: coverage is NULL");
        return -1;
    }

    if (coverage->num_channels >= NIMCP_MAX_CHANNELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_coverage_register_input_channel: capacity exceeded");
        return -1;
    }

    int32_t channel_id = (int32_t)coverage->num_channels;
    nimcp_channel_t* channel = &coverage->channels[channel_id];

    channel->channel_id = (uint32_t)channel_id;
    channel->name = name;
    channel->is_input = true;
    channel->validation_enabled = validation_enabled;
    channel->sanitization_enabled = false;
    channel->rate_limited = false;
    channel->bytes_processed = 0;
    channel->violations_blocked = 0;

    coverage->num_channels++;

    // Update statistics
    coverage->dimension_stats[NIMCP_COVERAGE_INPUT_CHANNELS].total_items++;
    if (validation_enabled) {
        coverage->dimension_stats[NIMCP_COVERAGE_INPUT_CHANNELS].protected_items++;
    }

    return channel_id;
}

int32_t nimcp_coverage_register_output_channel(
    nimcp_security_coverage_t* coverage,
    const char* name,
    bool sanitization_enabled,
    bool rate_limited)
{
    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_register_output_channel: coverage is NULL");
        return -1;
    }

    if (coverage->num_channels >= NIMCP_MAX_CHANNELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_coverage_register_output_channel: capacity exceeded");
        return -1;
    }

    int32_t channel_id = (int32_t)coverage->num_channels;
    nimcp_channel_t* channel = &coverage->channels[channel_id];

    channel->channel_id = (uint32_t)channel_id;
    channel->name = name;
    channel->is_input = false;
    channel->validation_enabled = false;
    channel->sanitization_enabled = sanitization_enabled;
    channel->rate_limited = rate_limited;
    channel->bytes_processed = 0;
    channel->violations_blocked = 0;

    coverage->num_channels++;

    // Update statistics
    coverage->dimension_stats[NIMCP_COVERAGE_OUTPUT_CHANNELS].total_items++;
    if (sanitization_enabled) {
        coverage->dimension_stats[NIMCP_COVERAGE_OUTPUT_CHANNELS].protected_items++;
    }

    return channel_id;
}

nimcp_result_t nimcp_coverage_record_channel_activity(
    nimcp_security_coverage_t* coverage,
    int32_t channel_id,
    size_t bytes,
    bool had_violation)
{
    if (!coverage || channel_id < 0 || (uint32_t)channel_id >= coverage->num_channels)
        return NIMCP_INVALID_PARAM;

    nimcp_channel_t* channel = &coverage->channels[channel_id];

    channel->bytes_processed += bytes;

    if (had_violation) {
        channel->violations_blocked++;
        coverage->total_violations++;

        nimcp_coverage_dimension_t dim = channel->is_input ?
            NIMCP_COVERAGE_INPUT_CHANNELS : NIMCP_COVERAGE_OUTPUT_CHANNELS;
        coverage->dimension_stats[dim].violations_detected++;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// IPC Endpoint Registration
//=============================================================================

int32_t nimcp_coverage_register_ipc_endpoint(
    nimcp_security_coverage_t* coverage,
    const char* name,
    bool authenticated,
    bool encrypted,
    uint32_t capability_mask)
{
    if (!coverage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_register_ipc_endpoint: coverage is NULL");
        return -1;
    }

    if (coverage->num_ipc_endpoints >= NIMCP_MAX_IPC_ENDPOINTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_coverage_register_ipc_endpoint: capacity exceeded");
        return -1;
    }

    int32_t endpoint_id = (int32_t)coverage->num_ipc_endpoints;
    nimcp_ipc_endpoint_t* endpoint = &coverage->ipc_endpoints[endpoint_id];

    endpoint->endpoint_id = (uint32_t)endpoint_id;
    endpoint->name = name;
    endpoint->authenticated = authenticated;
    endpoint->encrypted = encrypted;
    endpoint->capability_mask = capability_mask;
    endpoint->messages_sent = 0;
    endpoint->messages_received = 0;
    endpoint->auth_failures = 0;

    coverage->num_ipc_endpoints++;

    // Update statistics
    coverage->dimension_stats[NIMCP_COVERAGE_IPC_CHANNELS].total_items++;
    if (authenticated && encrypted) {
        coverage->dimension_stats[NIMCP_COVERAGE_IPC_CHANNELS].protected_items++;
    }

    return endpoint_id;
}

nimcp_result_t nimcp_coverage_record_ipc_message(
    nimcp_security_coverage_t* coverage,
    int32_t endpoint_id,
    bool is_send,
    bool auth_success)
{
    if (!coverage || endpoint_id < 0 ||
        (uint32_t)endpoint_id >= coverage->num_ipc_endpoints)
        return NIMCP_INVALID_PARAM;

    nimcp_ipc_endpoint_t* endpoint = &coverage->ipc_endpoints[endpoint_id];

    if (is_send) {
        endpoint->messages_sent++;
    } else {
        endpoint->messages_received++;
    }

    if (!auth_success) {
        endpoint->auth_failures++;
        coverage->total_violations++;
        coverage->dimension_stats[NIMCP_COVERAGE_IPC_CHANNELS].violations_detected++;

        char msg[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "IPC authentication failure on endpoint '%s'",
                 endpoint->name ? endpoint->name : "unnamed");
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_HIGH,
            msg
        );
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Coverage Verification
//=============================================================================

nimcp_result_t nimcp_coverage_verify_all(
    nimcp_security_coverage_t* coverage,
    nimcp_coverage_report_t* report)
{
    if (!coverage || !report)
        return NIMCP_INVALID_PARAM;

    memset(report, 0, sizeof(nimcp_coverage_report_t));
    report->report_timestamp = get_timestamp_ms();
    report->monitoring_uptime_ms = report->report_timestamp - coverage->start_time;

    float total_coverage = 0.0F;
    int active_dimensions = 0;

    for (int i = 0; i < NIMCP_COVERAGE_DIMENSION_COUNT; i++) {
        nimcp_dimension_stats_t* stats = &coverage->dimension_stats[i];

        // Calculate coverage percentage
        if (stats->total_items > 0) {
            stats->coverage_percent =
                (float)stats->protected_items / stats->total_items * 100.0F;
            active_dimensions++;
            total_coverage += stats->coverage_percent;
        } else {
            // No items = 100% coverage (nothing to protect)
            stats->coverage_percent = 100.0F;
        }

        // Determine status
        if (stats->coverage_percent >= 100.0F) {
            stats->status = NIMCP_COVERAGE_STATUS_FULL;
        } else if (stats->coverage_percent >= 50.0F) {
            stats->status = NIMCP_COVERAGE_STATUS_PARTIAL;
        } else if (stats->coverage_percent > 0.0F) {
            stats->status = NIMCP_COVERAGE_STATUS_PARTIAL;
        } else if (stats->total_items == 0) {
            stats->status = NIMCP_COVERAGE_STATUS_FULL;
        } else {
            stats->status = NIMCP_COVERAGE_STATUS_NONE;
        }

        stats->last_check = report->report_timestamp;
        memcpy(&report->dimensions[i], stats, sizeof(nimcp_dimension_stats_t));
        report->total_violations += stats->violations_detected;
    }

    // Calculate overall coverage
    if (active_dimensions > 0) {
        report->overall_coverage = total_coverage / active_dimensions;
    } else {
        report->overall_coverage = 100.0F;
    }

    // Check if all dimensions are at 100%
    report->all_dimensions_full = true;
    for (int i = 0; i < NIMCP_COVERAGE_DIMENSION_COUNT; i++) {
        if (report->dimensions[i].status != NIMCP_COVERAGE_STATUS_FULL) {
            report->all_dimensions_full = false;
            break;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_coverage_verify_dimension(
    nimcp_security_coverage_t* coverage,
    nimcp_coverage_dimension_t dimension,
    nimcp_dimension_stats_t* stats)
{
    if (!coverage || !stats || dimension >= NIMCP_COVERAGE_DIMENSION_COUNT)
        return NIMCP_INVALID_PARAM;

    memcpy(stats, &coverage->dimension_stats[dimension], sizeof(nimcp_dimension_stats_t));

    // Recalculate coverage
    if (stats->total_items > 0) {
        stats->coverage_percent =
            (float)stats->protected_items / stats->total_items * 100.0F;
    } else {
        stats->coverage_percent = 100.0F;
    }

    stats->last_check = get_timestamp_ms();

    return NIMCP_SUCCESS;
}

bool nimcp_coverage_is_complete(nimcp_security_coverage_t* coverage)
{
    if (!coverage) {
        return false;
    }

    nimcp_coverage_report_t report;
    if (nimcp_coverage_verify_all(coverage, &report) != NIMCP_SUCCESS) {
        return false;
    }

    return report.all_dimensions_full;
}

float nimcp_coverage_get_percentage(nimcp_security_coverage_t* coverage)
{
    if (!coverage)
        return 0.0F;

    nimcp_coverage_report_t report;
    if (nimcp_coverage_verify_all(coverage, &report) != NIMCP_SUCCESS)
        return 0.0F;

    return report.overall_coverage;
}

//=============================================================================
// Coverage Gap Detection
//=============================================================================

void** nimcp_coverage_find_memory_gaps(
    nimcp_security_coverage_t* coverage,
    uint32_t* gap_count)
{
    if (!coverage || !gap_count) {
        if (gap_count) *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_find_memory_gaps: validation failed");
        return NULL;
    }

    // Count inactive regions (gaps)
    uint32_t count = 0;
    for (uint32_t i = 0; i < coverage->num_regions; i++) {
        if (!coverage->regions[i].active) {
            count++;
        }
    }

    if (count == 0) {
        *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coverage_find_memory_gaps: count is zero");
        return NULL;
    }

    void** gaps = (void**)nimcp_calloc(count, sizeof(void*));
    if (!gaps) {
        *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_coverage_find_memory_gaps: gaps is NULL");
        return NULL;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < coverage->num_regions && idx < count; i++) {
        if (!coverage->regions[i].active) {
            gaps[idx++] = coverage->regions[i].base_address;
        }
    }

    *gap_count = count;
    return gaps;
}

uint32_t* nimcp_coverage_find_code_path_gaps(
    nimcp_security_coverage_t* coverage,
    uint32_t* gap_count)
{
    if (!coverage || !gap_count) {
        if (gap_count) *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_find_code_path_gaps: validation failed");
        return NULL;
    }

    // Count paths without CFI enabled
    uint32_t count = 0;
    for (uint32_t i = 0; i < coverage->num_code_paths; i++) {
        if (!coverage->code_paths[i].cfi_enabled) {
            count++;
        }
    }

    if (count == 0) {
        *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coverage_find_code_path_gaps: count is zero");
        return NULL;
    }

    uint32_t* gaps = (uint32_t*)nimcp_calloc(count, sizeof(uint32_t));
    if (!gaps) {
        *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_coverage_find_code_path_gaps: gaps is NULL");
        return NULL;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < coverage->num_code_paths && idx < count; i++) {
        if (!coverage->code_paths[i].cfi_enabled) {
            gaps[idx++] = coverage->code_paths[i].path_id;
        }
    }

    *gap_count = count;
    return gaps;
}

int32_t* nimcp_coverage_find_channel_gaps(
    nimcp_security_coverage_t* coverage,
    uint32_t* gap_count)
{
    if (!coverage || !gap_count) {
        if (gap_count) *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_find_channel_gaps: validation failed");
        return NULL;
    }

    // Count unprotected channels
    uint32_t count = 0;
    for (uint32_t i = 0; i < coverage->num_channels; i++) {
        nimcp_channel_t* ch = &coverage->channels[i];
        bool protected = ch->is_input ? ch->validation_enabled : ch->sanitization_enabled;
        if (!protected) {
            count++;
        }
    }

    if (count == 0) {
        *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coverage_find_channel_gaps: count is zero");
        return NULL;
    }

    int32_t* gaps = (int32_t*)nimcp_calloc(count, sizeof(int32_t));
    if (!gaps) {
        *gap_count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_coverage_find_channel_gaps: gaps is NULL");
        return NULL;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < coverage->num_channels && idx < count; i++) {
        nimcp_channel_t* ch = &coverage->channels[i];
        bool protected = ch->is_input ? ch->validation_enabled : ch->sanitization_enabled;
        if (!protected) {
            gaps[idx++] = (int32_t)ch->channel_id;
        }
    }

    *gap_count = count;
    return gaps;
}

//=============================================================================
// Temporal Coverage
//=============================================================================

nimcp_result_t nimcp_coverage_heartbeat(nimcp_security_coverage_t* coverage)
{
    if (!coverage)
        return NIMCP_INVALID_PARAM;

    uint64_t now = get_timestamp_ms();
    uint64_t gap = now - coverage->last_heartbeat;

    if (gap > coverage->max_gap_detected) {
        coverage->max_gap_detected = gap;
    }

    coverage->last_heartbeat = now;
    coverage->heartbeat_count++;

    // Update temporal coverage statistics
    coverage->dimension_stats[NIMCP_COVERAGE_TEMPORAL].total_items = 1;
    coverage->dimension_stats[NIMCP_COVERAGE_TEMPORAL].protected_items = 1;
    coverage->dimension_stats[NIMCP_COVERAGE_TEMPORAL].last_check = now;

    return NIMCP_SUCCESS;
}

bool nimcp_coverage_check_temporal(
    nimcp_security_coverage_t* coverage,
    uint64_t max_gap_ms)
{
    if (!coverage) {
        return false;
    }

    uint64_t now = get_timestamp_ms();
    uint64_t current_gap = now - coverage->last_heartbeat;

    if (current_gap > max_gap_ms || coverage->max_gap_detected > max_gap_ms) {
        coverage->dimension_stats[NIMCP_COVERAGE_TEMPORAL].status =
            NIMCP_COVERAGE_STATUS_DEGRADED;
        coverage->dimension_stats[NIMCP_COVERAGE_TEMPORAL].violations_detected++;
        coverage->total_violations++;

        char msg[NIMCP_LABEL_BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "Temporal coverage gap detected: %lu ms",
                 (unsigned long)(current_gap > max_gap_ms ? current_gap : coverage->max_gap_detected));
        nimcp_security_log_event(
            NIMCP_SECURITY_EVENT_THREAT_DETECTED,
            NIMCP_THREAT_MEDIUM,
            msg
        );

        return false;
    }

    return true;
}

uint64_t nimcp_coverage_get_uptime(nimcp_security_coverage_t* coverage)
{
    if (!coverage)
        return 0;

    return get_timestamp_ms() - coverage->start_time;
}

//=============================================================================
// Reporting
//=============================================================================

const char* nimcp_coverage_dimension_name(nimcp_coverage_dimension_t dimension)
{
    static const char* names[] = {
        "Memory Regions",
        "Code Paths",
        "Input Channels",
        "Output Channels",
        "IPC Channels",
        "Temporal",
        "Thread/Process",
        "External Interfaces"
    };

    if (dimension >= NIMCP_COVERAGE_DIMENSION_COUNT)
        return "Unknown";

    return names[dimension];
}

const char* nimcp_coverage_status_name(nimcp_coverage_status_t status)
{
    static const char* names[] = {
        "Unknown",
        "Full (100%)",
        "Partial",
        "None",
        "Degraded"
    };

    if (status > NIMCP_COVERAGE_STATUS_DEGRADED)
        return "Invalid";

    return names[status];
}

int32_t nimcp_coverage_generate_report(
    nimcp_security_coverage_t* coverage,
    char* buffer,
    size_t buffer_size)
{
    if (!coverage || !buffer || buffer_size < 256) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_coverage_generate_report: required parameter is NULL (coverage, buffer)");
        return -1;
    }

    nimcp_coverage_report_t report;
    if (nimcp_coverage_verify_all(coverage, &report) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_coverage_generate_report: validation failed");
        return -1;
    }

    int offset = 0;
    int remaining = (int)buffer_size;

    // Header
    int written = snprintf(buffer + offset, remaining,
        "=== NIMCP Security Coverage Report ===\n"
        "Overall Coverage: %.1f%%\n"
        "Status: %s\n"
        "Total Violations: %lu\n"
        "Uptime: %lu ms\n\n"
        "--- Dimension Breakdown ---\n",
        report.overall_coverage,
        report.all_dimensions_full ? "COMPLETE (100%%)" : "GAPS DETECTED",
        (unsigned long)report.total_violations,
        (unsigned long)report.monitoring_uptime_ms
    );

    if (written < 0 || written >= remaining)
        return NIMCP_ERROR_INVALID_PARAM;

    offset += written;
    remaining -= written;

    // Each dimension
    for (int i = 0; i < NIMCP_COVERAGE_DIMENSION_COUNT && remaining > 0; i++) {
        nimcp_dimension_stats_t* stats = &report.dimensions[i];

        written = snprintf(buffer + offset, remaining,
            "%-20s: %6.1f%% (%u/%u) - %s\n",
            nimcp_coverage_dimension_name((nimcp_coverage_dimension_t)i),
            stats->coverage_percent,
            stats->protected_items,
            stats->total_items,
            nimcp_coverage_status_name(stats->status)
        );

        if (written < 0 || written >= remaining)
            break;

        offset += written;
        remaining -= written;
    }

    return offset;
}
