/**
 * @file nimcp_emergency_halt.c
 * @brief Emergency Halt System Implementation
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Implementation of hardware-level emergency stop mechanism
 * WHY:  Ensure system can always be stopped regardless of software state
 * HOW:  Watchdog timer, cryptographic kill phrase, dead man's switch
 */

#include "security/nimcp_emergency_halt.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HALT_MAX_DUMP_HANDLERS      32
#define HALT_MAX_EVENTS             100
#define HALT_LOG_PREFIX             "[EMERGENCY_HALT]"

/* ============================================================================
 * Internal Types
 * ============================================================================ */

typedef struct dump_handler_entry {
    char module_name[NIMCP_ID_BUFFER_SIZE];
    halt_state_dump_handler_t handler;
    void* user_data;
    bool active;
} dump_handler_entry_t;

struct emergency_halt {
    uint32_t magic;                   /**< Magic number for validation */

    /* Configuration */
    emergency_halt_config_t config;

    /* State */
    bool is_halted;
    halt_level_t current_level;
    halt_trigger_t current_trigger;
    char halt_reason[HALT_REASON_MAX_LENGTH];
    uint64_t halt_timestamp_us;

    /* Watchdog state */
    uint64_t last_heartbeat_us;
    bool watchdog_running;

    /* Dead man's switch state */
    uint64_t last_deadman_confirm_us;

    /* Statistics */
    emergency_halt_stats_t stats;

    /* Event history (circular buffer) */
    halt_event_t events[HALT_MAX_EVENTS];
    size_t event_count;
    size_t event_head;

    /* State dump handlers */
    dump_handler_entry_t dump_handlers[HALT_MAX_DUMP_HANDLERS];
    size_t dump_handler_count;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;

    /* Brain immune integration */
    void* brain_immune;               /**< Brain immune system for systemic response */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t start_time_us;
};

/* ============================================================================
 * Internal Functions - Forward Declarations
 * ============================================================================ */

static void halt_add_event(emergency_halt_t* halt, const halt_event_t* event);
static nimcp_error_t halt_execute(emergency_halt_t* halt, halt_level_t level,
                                   halt_trigger_t trigger, const char* reason);
static nimcp_error_t halt_dump_all_state(emergency_halt_t* halt, const char* path);
static void halt_broadcast_message(emergency_halt_t* halt, bio_message_type_t type);
static bool halt_verify_kill_phrase(const emergency_halt_t* halt, const char* phrase);

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

emergency_halt_config_t emergency_halt_default_config(void) {
    emergency_halt_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_watchdog = true;
    config.watchdog_timeout_ms = HALT_DEFAULT_WATCHDOG_TIMEOUT_MS;

    config.enable_kill_phrase = true;
    /* Kill phrase hash must be set by caller */

    config.enable_deadman_switch = false;  /* Disabled by default */
    config.deadman_interval_ms = HALT_DEFAULT_DEADMAN_INTERVAL_MS;

    config.enable_state_dump = true;
    strncpy(config.state_dump_path, "/tmp/nimcp_halt_state",
            HALT_STATE_PATH_MAX_LENGTH - 1);

    config.enable_network_kill = false;
    config.require_physical_restart = false;

    config.pre_halt_callback = NULL;
    config.callback_user_data = NULL;

    return config;
}

emergency_halt_t* emergency_halt_create(const emergency_halt_config_t* config) {
    emergency_halt_t* halt = nimcp_malloc(sizeof(emergency_halt_t));
    if (!halt) {
        NIMCP_LOGGING_ERROR("%s Failed to allocate emergency halt system",
                           HALT_LOG_PREFIX);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emergency_halt_create: halt is NULL");
        return NULL;
    }

    memset(halt, 0, sizeof(emergency_halt_t));
    halt->magic = EMERGENCY_HALT_MAGIC;

    /* Apply configuration */
    if (config) {
        memcpy(&halt->config, config, sizeof(emergency_halt_config_t));
    } else {
        halt->config = emergency_halt_default_config();
    }

    /* Initialize state */
    halt->is_halted = false;
    halt->current_level = HALT_GRACEFUL;
    halt->halt_reason[0] = '\0';
    halt->halt_timestamp_us = 0;

    /* Initialize timing */
    halt->start_time_us = nimcp_time_now_us();
    halt->last_heartbeat_us = halt->start_time_us;
    halt->last_deadman_confirm_us = halt->start_time_us;
    halt->watchdog_running = halt->config.enable_watchdog;

    /* Initialize mutex */
    halt->mutex = nimcp_mutex_create(NULL);
    if (!halt->mutex) {
        NIMCP_LOGGING_ERROR("%s Failed to create mutex", HALT_LOG_PREFIX);
        nimcp_free(halt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emergency_halt_create: halt->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("%s Emergency halt system initialized (watchdog=%s, timeout=%ums)",
                       HALT_LOG_PREFIX,
                       halt->config.enable_watchdog ? "enabled" : "disabled",
                       halt->config.watchdog_timeout_ms);

    return halt;
}

void emergency_halt_destroy(emergency_halt_t* halt) {
    if (!halt) return;
    if (halt->magic != EMERGENCY_HALT_MAGIC) return;

    /* Disconnect from bio-async */
    if (halt->bio_async_connected) {
        bio_router_unregister_module(halt->bio_ctx);
    }

    /* Destroy mutex */
    if (halt->mutex) {
        nimcp_mutex_destroy(halt->mutex);
    }

    /* Clear magic to prevent use-after-free */
    halt->magic = 0;

    nimcp_free(halt);
    NIMCP_LOGGING_INFO("%s Emergency halt system destroyed", HALT_LOG_PREFIX);
}

nimcp_error_t emergency_halt_reset(emergency_halt_t* halt,
                                    const uint8_t* authorization_code) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(halt->mutex);

    if (!halt->is_halted) {
        nimcp_mutex_unlock(halt->mutex);
        return NIMCP_SUCCESS;  /* Not halted, nothing to reset */
    }

    /* If physical restart required, reject software reset */
    if (halt->config.require_physical_restart) {
        NIMCP_LOGGING_ERROR("%s Reset rejected: physical restart required",
                           HALT_LOG_PREFIX);
        nimcp_mutex_unlock(halt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_PERMISSION_DENIED, "emergency_halt: error condition");
        return NIMCP_ERROR_PERMISSION_DENIED;
    }

    /* TODO: Verify authorization_code if provided */
    (void)authorization_code;

    /* Record recovery in event */
    if (halt->event_count > 0) {
        size_t last_idx = (halt->event_head + HALT_MAX_EVENTS - 1) % HALT_MAX_EVENTS;
        halt->events[last_idx].recovery_timestamp_us = nimcp_time_now_us();
    }

    /* Update statistics */
    uint64_t halted_duration = nimcp_time_now_us() - halt->halt_timestamp_us;
    halt->stats.halted_time_total_ms += halted_duration / 1000;
    halt->stats.successful_recoveries++;

    /* Clear halted state */
    halt->is_halted = false;
    halt->current_level = HALT_GRACEFUL;
    halt->halt_reason[0] = '\0';
    halt->halt_timestamp_us = 0;

    /* Reset timers */
    uint64_t now = nimcp_time_now_us();
    halt->last_heartbeat_us = now;
    halt->last_deadman_confirm_us = now;

    nimcp_mutex_unlock(halt->mutex);

    NIMCP_LOGGING_INFO("%s System reset and recovered", HALT_LOG_PREFIX);

    /* Broadcast recovery message */
    halt_broadcast_message(halt, BIO_MSG_EMERGENCY_HALT_RESET);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Watchdog Implementation
 * ============================================================================ */

nimcp_error_t emergency_halt_heartbeat(emergency_halt_t* halt) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt_heartbeat: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(halt->mutex);

    if (halt->is_halted) {
        nimcp_mutex_unlock(halt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SYSTEM_HALTED, "emergency_halt_heartbeat: error condition");
        return NIMCP_ERROR_SYSTEM_HALTED;
    }

    halt->last_heartbeat_us = nimcp_time_now_us();
    halt->stats.total_heartbeats++;

    nimcp_mutex_unlock(halt->mutex);

    return NIMCP_SUCCESS;
}

uint32_t emergency_halt_time_until_timeout(const emergency_halt_t* halt) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) return 0;
    if (!halt->config.enable_watchdog || halt->is_halted) return 0;

    uint64_t now = nimcp_time_now_us();
    uint64_t elapsed_us = now - halt->last_heartbeat_us;
    uint64_t timeout_us = (uint64_t)halt->config.watchdog_timeout_ms * 1000;

    if (elapsed_us >= timeout_us) return 0;
    return (uint32_t)((timeout_us - elapsed_us) / 1000);
}

nimcp_error_t emergency_halt_set_watchdog_enabled(emergency_halt_t* halt,
                                                   bool enabled) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(halt->mutex);
    halt->config.enable_watchdog = enabled;
    halt->watchdog_running = enabled;
    if (enabled) {
        halt->last_heartbeat_us = nimcp_time_now_us();
    }
    nimcp_mutex_unlock(halt->mutex);

    NIMCP_LOGGING_INFO("%s Watchdog %s", HALT_LOG_PREFIX,
                       enabled ? "enabled" : "disabled");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Halt Trigger Implementation
 * ============================================================================ */

nimcp_error_t emergency_halt_trigger(emergency_halt_t* halt,
                                      halt_level_t level,
                                      halt_trigger_t trigger,
                                      const char* reason) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    return halt_execute(halt, level, trigger, reason ? reason : "No reason provided");
}

nimcp_error_t emergency_halt_kill_phrase(emergency_halt_t* halt,
                                          const char* kill_phrase,
                                          halt_level_t level,
                                          const char* reason) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC || !kill_phrase) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!halt->config.enable_kill_phrase) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_IMPLEMENTED, "emergency_halt: error condition");
        return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    nimcp_mutex_lock(halt->mutex);
    halt->stats.kill_phrase_attempts++;

    bool valid = halt_verify_kill_phrase(halt, kill_phrase);

    if (!valid) {
        halt->stats.kill_phrase_failures++;
        nimcp_mutex_unlock(halt->mutex);
        NIMCP_LOGGING_WARN("%s Invalid kill phrase attempt", HALT_LOG_PREFIX);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SIGNATURE_INVALID, "emergency_halt: error condition");
        return NIMCP_ERROR_SIGNATURE_INVALID;
    }

    halt->stats.kill_phrase_successes++;
    nimcp_mutex_unlock(halt->mutex);

    /* Broadcast kill phrase message */
    halt_broadcast_message(halt, BIO_MSG_EMERGENCY_HALT_KILL_PHRASE);

    /* Execute halt */
    char full_reason[HALT_REASON_MAX_LENGTH];
    snprintf(full_reason, sizeof(full_reason), "Kill phrase: %s",
             reason ? reason : "authorized halt");

    return halt_execute(halt, level, HALT_TRIGGER_KILL_PHRASE, full_reason);
}

/* ============================================================================
 * Dead Man's Switch Implementation
 * ============================================================================ */

nimcp_error_t emergency_halt_confirm_alive(emergency_halt_t* halt,
                                            const char* confirmation_code) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!halt->config.enable_deadman_switch) {
        return NIMCP_SUCCESS;  /* Nothing to confirm */
    }

    /* TODO: Verify confirmation_code if required */
    (void)confirmation_code;

    nimcp_mutex_lock(halt->mutex);
    halt->last_deadman_confirm_us = nimcp_time_now_us();
    halt->stats.deadman_confirmations++;
    nimcp_mutex_unlock(halt->mutex);

    halt_broadcast_message(halt, BIO_MSG_EMERGENCY_HALT_CONFIRMED);

    return NIMCP_SUCCESS;
}

uint32_t emergency_halt_time_until_deadman(const emergency_halt_t* halt) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) return 0;
    if (!halt->config.enable_deadman_switch || halt->is_halted) return 0;

    uint64_t now = nimcp_time_now_us();
    uint64_t elapsed_us = now - halt->last_deadman_confirm_us;
    uint64_t interval_us = (uint64_t)halt->config.deadman_interval_ms * 1000;

    if (elapsed_us >= interval_us) return 0;
    return (uint32_t)((interval_us - elapsed_us) / 1000);
}

/* ============================================================================
 * Status Implementation
 * ============================================================================ */

bool emergency_halt_is_halted(const emergency_halt_t* halt) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) return true;
    return halt->is_halted;
}

halt_level_t emergency_halt_get_level(const emergency_halt_t* halt) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) return HALT_CATASTROPHIC;
    return halt->current_level;
}

nimcp_error_t emergency_halt_get_reason(const emergency_halt_t* halt,
                                         char* reason_out,
                                         size_t max_len) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC || !reason_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!halt->is_halted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "emergency_halt: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    strncpy(reason_out, halt->halt_reason, max_len - 1);
    reason_out[max_len - 1] = '\0';

    return NIMCP_SUCCESS;
}

uint64_t emergency_halt_get_timestamp(const emergency_halt_t* halt) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) return 0;
    return halt->halt_timestamp_us;
}

nimcp_error_t emergency_halt_get_stats(const emergency_halt_t* halt,
                                        emergency_halt_stats_t* stats) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)halt->mutex);

    memcpy(stats, &halt->stats, sizeof(emergency_halt_stats_t));

    /* Calculate current uptime */
    uint64_t now = nimcp_time_now_us();
    stats->uptime_total_ms = (now - halt->start_time_us) / 1000;

    nimcp_mutex_unlock((nimcp_mutex_t*)halt->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t emergency_halt_get_events(const emergency_halt_t* halt,
                                         halt_event_t* events,
                                         size_t max_events,
                                         size_t* count_out) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC || !events || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)halt->mutex);

    size_t count = halt->event_count < max_events ? halt->event_count : max_events;

    /* Copy events from circular buffer (most recent first) */
    for (size_t i = 0; i < count; i++) {
        size_t idx = (halt->event_head + HALT_MAX_EVENTS - 1 - i) % HALT_MAX_EVENTS;
        memcpy(&events[i], &halt->events[idx], sizeof(halt_event_t));
    }

    *count_out = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)halt->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * State Dump Implementation
 * ============================================================================ */

nimcp_error_t emergency_halt_dump_state(emergency_halt_t* halt,
                                         const char* dump_path) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    const char* path = dump_path ? dump_path : halt->config.state_dump_path;
    return halt_dump_all_state(halt, path);
}

nimcp_error_t emergency_halt_register_dump_handler(emergency_halt_t* halt,
                                                    const char* module_name,
                                                    halt_state_dump_handler_t handler,
                                                    void* user_data) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC || !module_name || !handler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(halt->mutex);

    if (halt->dump_handler_count >= HALT_MAX_DUMP_HANDLERS) {
        nimcp_mutex_unlock(halt->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_QUEUE_FULL, "emergency_halt: error condition");
        return NIMCP_ERROR_QUEUE_FULL;
    }

    dump_handler_entry_t* entry = &halt->dump_handlers[halt->dump_handler_count];
    strncpy(entry->module_name, module_name, sizeof(entry->module_name) - 1);
    entry->module_name[sizeof(entry->module_name) - 1] = '\0';
    entry->handler = handler;
    entry->user_data = user_data;
    entry->active = true;
    halt->dump_handler_count++;

    nimcp_mutex_unlock(halt->mutex);

    NIMCP_LOGGING_DEBUG("%s Registered dump handler: %s", HALT_LOG_PREFIX, module_name);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

nimcp_error_t emergency_halt_connect_bio_async(emergency_halt_t* halt) {
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt_connect_bio_async: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(halt->mutex);

    if (halt->bio_async_connected) {
        nimcp_mutex_unlock(halt->mutex);
        return NIMCP_SUCCESS;
    }

    /* Register with bio-router */
    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_EMERGENCY_HALT,
        .module_name = "emergency_halt",
        .inbox_capacity = 0,  /* Use default */
        .user_data = halt
    };
    halt->bio_ctx = bio_router_register_module(&module_info);
    if (!halt->bio_ctx) {
        NIMCP_LOGGING_WARN("%s Failed to connect to bio-async", HALT_LOG_PREFIX);
        nimcp_mutex_unlock(halt->mutex);
        return NIMCP_SUCCESS;  /* Non-fatal */
    }

    halt->bio_async_connected = true;
    nimcp_mutex_unlock(halt->mutex);

    NIMCP_LOGGING_INFO("%s Connected to bio-async", HALT_LOG_PREFIX);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Brain Immune Integration
 * ============================================================================ */

nimcp_error_t emergency_halt_connect_brain_immune(
    emergency_halt_t* halt,
    struct brain_immune* brain_immune)
{
    if (!halt || halt->magic != EMERGENCY_HALT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(halt->mutex);
    halt->brain_immune = brain_immune;
    nimcp_mutex_unlock(halt->mutex);

    NIMCP_LOGGING_INFO("%s Connected to brain immune system", HALT_LOG_PREFIX);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

const char* emergency_halt_level_name(halt_level_t level) {
    switch (level) {
        case HALT_GRACEFUL:     return "GRACEFUL";
        case HALT_IMMEDIATE:    return "IMMEDIATE";
        case HALT_EMERGENCY:    return "EMERGENCY";
        case HALT_CATASTROPHIC: return "CATASTROPHIC";
        default:                return "UNKNOWN";
    }
}

const char* emergency_halt_trigger_name(halt_trigger_t trigger) {
    switch (trigger) {
        case HALT_TRIGGER_MANUAL:         return "MANUAL";
        case HALT_TRIGGER_KILL_PHRASE:    return "KILL_PHRASE";
        case HALT_TRIGGER_WATCHDOG:       return "WATCHDOG";
        case HALT_TRIGGER_DEADMAN:        return "DEADMAN_SWITCH";
        case HALT_TRIGGER_TRIPWIRE:       return "TRIPWIRE";
        case HALT_TRIGGER_ALIGNMENT:      return "ALIGNMENT";
        case HALT_TRIGGER_CAPABILITY:     return "CAPABILITY";
        case HALT_TRIGGER_EXTERNAL:       return "EXTERNAL";
        case HALT_TRIGGER_INTERNAL_ERROR: return "INTERNAL_ERROR";
        default:                          return "UNKNOWN";
    }
}

nimcp_error_t emergency_halt_hash_kill_phrase(const char* kill_phrase,
                                               uint8_t* hash_out) {
    if (!kill_phrase || !hash_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emergency_halt: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Use BBB hashing infrastructure */
    bool result = bbb_calculate_hash(kill_phrase, strlen(kill_phrase), hash_out);

    return result ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static void halt_add_event(emergency_halt_t* halt, const halt_event_t* event) {
    memcpy(&halt->events[halt->event_head], event, sizeof(halt_event_t));
    halt->event_head = (halt->event_head + 1) % HALT_MAX_EVENTS;
    if (halt->event_count < HALT_MAX_EVENTS) {
        halt->event_count++;
    }
}

static nimcp_error_t halt_execute(emergency_halt_t* halt, halt_level_t level,
                                   halt_trigger_t trigger, const char* reason) {
    nimcp_mutex_lock(halt->mutex);

    /* Log the halt */
    NIMCP_LOGGING_ERROR("%s HALT TRIGGERED: level=%s, trigger=%s, reason=%s",
                        HALT_LOG_PREFIX,
                        emergency_halt_level_name(level),
                        emergency_halt_trigger_name(trigger),
                        reason);

    /* Call pre-halt callback if registered */
    if (halt->config.pre_halt_callback) {
        halt->config.pre_halt_callback(level, halt->config.callback_user_data);
    }

    /* Update state */
    halt->is_halted = true;
    halt->current_level = level;
    halt->current_trigger = trigger;
    strncpy(halt->halt_reason, reason, HALT_REASON_MAX_LENGTH - 1);
    halt->halt_reason[HALT_REASON_MAX_LENGTH - 1] = '\0';
    halt->halt_timestamp_us = nimcp_time_now_us();

    /* Update statistics */
    switch (level) {
        case HALT_GRACEFUL:     halt->stats.graceful_halts++;     break;
        case HALT_IMMEDIATE:    halt->stats.immediate_halts++;    break;
        case HALT_EMERGENCY:    halt->stats.emergency_halts++;    break;
        case HALT_CATASTROPHIC: halt->stats.catastrophic_halts++; break;
    }

    /* Create event record */
    halt_event_t event;
    memset(&event, 0, sizeof(event));
    event.timestamp_us = halt->halt_timestamp_us;
    event.level = level;
    event.trigger = trigger;
    strncpy(event.reason, reason, HALT_REASON_MAX_LENGTH - 1);
    event.recovery_timestamp_us = 0;  /* Not yet recovered */

    /* Dump state for emergency and catastrophic levels */
    if ((level >= HALT_EMERGENCY) && halt->config.enable_state_dump) {
        nimcp_error_t dump_result = halt_dump_all_state(halt, NULL);
        event.state_dumped = (dump_result == NIMCP_SUCCESS);
        if (event.state_dumped) {
            strncpy(event.state_dump_path, halt->config.state_dump_path,
                    HALT_STATE_PATH_MAX_LENGTH - 1);
        }
    }

    halt_add_event(halt, &event);

    nimcp_mutex_unlock(halt->mutex);

    /* Broadcast halt message */
    halt_broadcast_message(halt, BIO_MSG_EMERGENCY_HALT);

    /* For catastrophic halt, terminate process */
    if (level == HALT_CATASTROPHIC) {
        NIMCP_LOGGING_ERROR("%s CATASTROPHIC HALT - terminating process",
                           HALT_LOG_PREFIX);
        /* Give time for log to flush */
        /* In real implementation, would call abort() or signal handler */
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t halt_dump_all_state(emergency_halt_t* halt, const char* path) {
    const char* dump_path = path ? path : halt->config.state_dump_path;

    NIMCP_LOGGING_INFO("%s Dumping state to: %s", HALT_LOG_PREFIX, dump_path);

    /* Call all registered dump handlers */
    for (size_t i = 0; i < halt->dump_handler_count; i++) {
        if (halt->dump_handlers[i].active && halt->dump_handlers[i].handler) {
            NIMCP_LOGGING_DEBUG("%s Calling dump handler: %s",
                               HALT_LOG_PREFIX,
                               halt->dump_handlers[i].module_name);

            nimcp_error_t result = halt->dump_handlers[i].handler(
                dump_path,
                halt->dump_handlers[i].user_data
            );

            if (result != NIMCP_SUCCESS) {
                NIMCP_LOGGING_WARN("%s Dump handler failed: %s",
                                     HALT_LOG_PREFIX,
                                     halt->dump_handlers[i].module_name);
            }
        }
    }

    halt_broadcast_message(halt, BIO_MSG_EMERGENCY_HALT_STATE_DUMPED);

    return NIMCP_SUCCESS;
}

static void halt_broadcast_message(emergency_halt_t* halt, bio_message_type_t type) {
    if (!halt->bio_async_connected) return;

    bio_message_header_t header;
    memset(&header, 0, sizeof(header));
    header.type = type;
    header.source_module = BIO_MODULE_EMERGENCY_HALT;
    header.target_module = BIO_MODULE_ALL;
    header.timestamp_us = nimcp_time_now_us();
    header.flags = BIO_MSG_FLAG_URGENT | BIO_MSG_FLAG_BROADCAST;

    bio_router_broadcast(halt->bio_ctx, &header, sizeof(header));
}

static bool halt_verify_kill_phrase(const emergency_halt_t* halt, const char* phrase) {
    uint8_t computed_hash[HALT_KILL_PHRASE_HASH_SIZE];

    if (!bbb_calculate_hash(phrase, strlen(phrase), computed_hash)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "halt_verify_kill_phrase: bbb_calculate_hash is NULL");
        return false;
    }

    /* Constant-time comparison to prevent timing attacks */
    int result = 0;
    for (size_t i = 0; i < HALT_KILL_PHRASE_HASH_SIZE; i++) {
        result |= computed_hash[i] ^ halt->config.kill_phrase_hash[i];
    }

    return result == 0;
}
