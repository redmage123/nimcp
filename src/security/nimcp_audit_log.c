//=============================================================================
// nimcp_audit_log.c - Tamper-Resistant Safety Audit Logging Implementation
//=============================================================================
/**
 * @file nimcp_audit_log.c
 * @brief Tamper-resistant audit logging for safety-critical events
 *
 * WHAT: Always-on, append-only audit log with integrity verification
 * WHY:  Non-repudiable records of all safety-critical decisions
 * HOW:  In-memory ring buffer + append-only disk log with CRC32 + sequence numbers
 *
 * IMPORTANT: This audit log is NOT disableable via any config flag.
 *            It is always active. The only way to remove it is to modify
 *            source code — which is the point.
 *
 * @version 1.0.0
 * @date 2026-03-21
 */

#include "security/nimcp_audit_log.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define LOG_MODULE "SAFETY_AUDIT"

/* Ring buffer capacity */
#define SAFETY_AUDIT_RING_CAPACITY 100000

/* CRC32 lookup table (IEEE polynomial) */
static uint32_t s_crc32_table[256];
static bool s_crc32_initialized = false;

static void _init_crc32_table(void) {
    if (s_crc32_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
        s_crc32_table[i] = crc;
    }
    s_crc32_initialized = true;
}

static uint32_t _compute_crc32(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ s_crc32_table[(crc ^ bytes[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFFu;
}

/* Global safety audit log structure */
struct nimcp_safety_audit_log {
    nimcp_safety_audit_entry_t* ring;  /* Ring buffer of entries */
    uint32_t ring_capacity;            /* Max entries in ring */
    uint32_t ring_head;                /* Next write position */
    uint32_t ring_count;               /* Current entries in ring */
    uint32_t next_sequence;            /* Monotonic sequence counter */
    nimcp_mutex_t* mutex;              /* Thread safety */
    int log_fd;                        /* File descriptor for append-only log */
    char log_path[512];                /* Path to log file */
    bool initialized;                  /* Initialization flag */
};

/* Global singleton */
static nimcp_safety_audit_log_t* s_global_safety_audit = NULL;
static volatile bool s_safety_audit_init_done = false;

/* Get microsecond timestamp */
static uint64_t _get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/* Compute CRC32 for an entry (excluding the checksum field itself) */
static uint32_t _entry_checksum(const nimcp_safety_audit_entry_t* entry) {
    /* Checksum covers everything except the checksum field */
    size_t check_len = offsetof(nimcp_safety_audit_entry_t, checksum);
    return _compute_crc32(entry, check_len);
}

/* Write entry to disk (best-effort) */
static void _write_entry_to_disk(nimcp_safety_audit_log_t* log,
                                  const nimcp_safety_audit_entry_t* entry) {
    if (log->log_fd < 0) return;

    /* Format: [seq] timestamp severity event description\n */
    char line[512];
    int len = snprintf(line, sizeof(line),
        "[%08u] %llu sev=%u evt=%d %s\n",
        entry->sequence_number,
        (unsigned long long)entry->timestamp_us,
        entry->severity,
        (int)entry->event,
        entry->description);

    if (len > 0 && len < (int)sizeof(line)) {
        /* Best-effort write — if it fails, entry stays in memory ring */
        ssize_t written = write(log->log_fd, line, (size_t)len);
        (void)written;  /* Intentionally ignoring — best-effort */
    }
}

nimcp_safety_audit_log_t* nimcp_safety_audit_get_global(void) {
    return s_global_safety_audit;
}

int nimcp_safety_audit_init(const char* log_dir) {
    if (s_safety_audit_init_done) return 0;  /* Already initialized */

    _init_crc32_table();

    /* Allocate the global safety audit log */
    nimcp_safety_audit_log_t* log = nimcp_malloc(sizeof(nimcp_safety_audit_log_t));
    if (!log) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate safety audit log structure");
        return -1;
    }
    memset(log, 0, sizeof(*log));

    /* Allocate ring buffer */
    log->ring = nimcp_malloc(SAFETY_AUDIT_RING_CAPACITY * sizeof(nimcp_safety_audit_entry_t));
    if (!log->ring) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate safety audit ring buffer (%u entries)",
            SAFETY_AUDIT_RING_CAPACITY);
        nimcp_free(log);
        return -1;
    }
    memset(log->ring, 0, SAFETY_AUDIT_RING_CAPACITY * sizeof(nimcp_safety_audit_entry_t));
    log->ring_capacity = SAFETY_AUDIT_RING_CAPACITY;
    log->ring_head = 0;
    log->ring_count = 0;
    log->next_sequence = 1;  /* Start at 1 — 0 means uninitialized */

    /* Create mutex */
    log->mutex = nimcp_mutex_create(NULL);
    if (!log->mutex) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to create safety audit log mutex");
        nimcp_free(log->ring);
        nimcp_free(log);
        return -1;
    }

    /* Try to open log file (best-effort — works without disk) */
    const char* dir = log_dir ? log_dir : "/var/log/nimcp";
    log->log_fd = -1;

    /* Try to create directory (may fail if no permissions — that's OK) */
    mkdir(dir, 0750);

    snprintf(log->log_path, sizeof(log->log_path), "%s/nimcp_safety_audit.log", dir);

    /* Open with O_APPEND for atomicity, O_CREAT to create if missing */
    log->log_fd = open(log->log_path, O_WRONLY | O_APPEND | O_CREAT, 0640);
    if (log->log_fd < 0) {
        /* Not fatal — continue with in-memory only */
        LOG_MODULE_WARN(LOG_MODULE, "Cannot open safety audit log file %s: %s — "
            "audit entries will be in-memory only",
            log->log_path, strerror(errno));
    } else {
        LOG_MODULE_INFO(LOG_MODULE, "Safety audit log opened: %s", log->log_path);
    }

    log->initialized = true;
    s_global_safety_audit = log;
    s_safety_audit_init_done = true;

    /* Log our own initialization */
    nimcp_safety_audit_log_event(NIMCP_SAFETY_AUDIT_CONFIG_CHANGE, 0,
        "Safety audit log initialized (ring=%u, file=%s)",
        SAFETY_AUDIT_RING_CAPACITY, log->log_fd >= 0 ? log->log_path : "MEMORY-ONLY");

    return 0;
}

void nimcp_safety_audit_log_event(nimcp_safety_audit_event_t event, uint32_t severity,
    const char* fmt, ...)
{
    nimcp_safety_audit_log_t* log = s_global_safety_audit;
    if (!log || !log->initialized) return;

    nimcp_mutex_lock(log->mutex);

    /* Build entry */
    nimcp_safety_audit_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.timestamp_us = _get_timestamp_us();
    entry.event = event;
    entry.severity = severity;
    entry.sequence_number = log->next_sequence++;

    /* Format description */
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(entry.description, sizeof(entry.description), fmt, args);
        va_end(args);
    }

    /* Compute checksum */
    entry.checksum = _entry_checksum(&entry);

    /* Write to ring buffer */
    log->ring[log->ring_head] = entry;
    log->ring_head = (log->ring_head + 1) % log->ring_capacity;
    if (log->ring_count < log->ring_capacity) {
        log->ring_count++;
    }

    /* Write to disk (best-effort) */
    _write_entry_to_disk(log, &entry);

    nimcp_mutex_unlock(log->mutex);
}

int nimcp_safety_audit_flush(void) {
    nimcp_safety_audit_log_t* log = s_global_safety_audit;
    if (!log || !log->initialized) return -1;
    if (log->log_fd < 0) return -1;

    nimcp_mutex_lock(log->mutex);
    int result = fsync(log->log_fd);
    nimcp_mutex_unlock(log->mutex);

    return result == 0 ? 0 : -1;
}

uint32_t nimcp_safety_audit_get_count(void) {
    nimcp_safety_audit_log_t* log = s_global_safety_audit;
    if (!log || !log->initialized) return 0;
    return log->next_sequence - 1;  /* Sequences start at 1 */
}

int nimcp_safety_audit_verify_integrity(uint32_t* gaps_found, uint32_t* corrupted) {
    nimcp_safety_audit_log_t* log = s_global_safety_audit;
    if (!log || !log->initialized) return -1;

    uint32_t gaps = 0;
    uint32_t corrupt = 0;

    nimcp_mutex_lock(log->mutex);

    if (log->ring_count > 0) {
        /* Walk ring buffer and verify */
        uint32_t start;
        uint32_t count = log->ring_count;

        if (count >= log->ring_capacity) {
            /* Ring has wrapped — start from head (oldest entry) */
            start = log->ring_head;
        } else {
            start = 0;
        }

        uint32_t prev_seq = 0;
        for (uint32_t i = 0; i < count; i++) {
            uint32_t idx = (start + i) % log->ring_capacity;
            nimcp_safety_audit_entry_t* e = &log->ring[idx];

            /* Check CRC32 */
            uint32_t expected_crc = _entry_checksum(e);
            if (e->checksum != expected_crc) {
                corrupt++;
            }

            /* Check sequence continuity */
            if (prev_seq > 0 && e->sequence_number != prev_seq + 1) {
                gaps++;
            }
            prev_seq = e->sequence_number;
        }
    }

    nimcp_mutex_unlock(log->mutex);

    if (gaps_found) *gaps_found = gaps;
    if (corrupted) *corrupted = corrupt;

    return (gaps == 0 && corrupt == 0) ? 0 : -1;
}
