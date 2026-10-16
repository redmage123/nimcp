/**
 * @file nimcp_immune_vaccine.c
 * @brief Immune Vaccine System Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implements vaccine-like pre-training for brain immune system
 * WHY:  Enable proactive immunity without full inflammatory response
 * HOW:  Direct memory B cell injection, bypassing activation cycle
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_immune_vaccine.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(immune_vaccine, MESH_ADAPTER_CATEGORY_SECURITY)



/* Mutex convenience macros */
#define nimcp_mutex_create() nimcp_platform_mutex_create()
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_destroy(m) do { \
    nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)(m)); \
    nimcp_free(m); \
    (m) = NULL; \
} while(0)

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static uint64_t get_timestamp_ms(void);
static vaccine_entry_t* find_vaccine_by_id(vaccine_system_t* system, uint32_t id);
static int add_to_schedule(vaccine_system_t* system, uint32_t vaccine_id,
                          uint64_t scheduled_time, bool is_booster, uint32_t original_id);
static void process_scheduled_vaccines(vaccine_system_t* system, uint64_t current_time);
static void update_all_efficacies(vaccine_system_t* system, uint64_t delta_ms);
static void update_statistics(vaccine_system_t* system);
static uint32_t calculate_checksum(const void* data, size_t len);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * @brief Convert vaccine type to string
 */
const char* vaccine_type_to_string(vaccine_type_t type) {
    switch (type) {
        case VACCINE_TYPE_ATTENUATED:  return "ATTENUATED";
        case VACCINE_TYPE_INACTIVATED: return "INACTIVATED";
        case VACCINE_TYPE_SUBUNIT:     return "SUBUNIT";
        case VACCINE_TYPE_MRNA:        return "MRNA";
        case VACCINE_TYPE_PASSIVE:     return "PASSIVE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert vaccine status to string
 */
const char* vaccine_status_to_string(vaccine_status_t status) {
    switch (status) {
        case VACCINE_STATUS_PENDING:       return "PENDING";
        case VACCINE_STATUS_SCHEDULED:     return "SCHEDULED";
        case VACCINE_STATUS_ADMINISTERED:  return "ADMINISTERED";
        case VACCINE_STATUS_BOOSTED:       return "BOOSTED";
        case VACCINE_STATUS_EXPIRED:       return "EXPIRED";
        case VACCINE_STATUS_FAILED:        return "FAILED";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal Helpers - Implementation
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Retrieve current time as millisecond timestamp
 * WHY:  Track vaccine timing and efficacy decay
 * HOW:  Use platform time functions
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Find vaccine entry by ID
 *
 * WHAT: Locate vaccine in array by ID
 * WHY:  Access vaccine for operations
 * HOW:  Linear search through vaccine array
 */
static vaccine_entry_t* find_vaccine_by_id(vaccine_system_t* system, uint32_t id) {
    if (!system || !system->vaccines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_vaccine_by_id: required parameter is NULL (system, system->vaccines)");
        return NULL;
    }

    for (size_t i = 0; i < system->vaccine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->vaccine_count > 256) {
            immune_vaccine_heartbeat("immune_vacci_loop",
                             (float)(i + 1) / (float)system->vaccine_count);
        }

        if (system->vaccines[i].id == id) {
            return &system->vaccines[i];
        }
    }
    return NULL;
}

/**
 * @brief Add vaccine to schedule
 *
 * WHAT: Queue vaccine for future administration
 * WHY:  Controlled, timed vaccine rollout
 * HOW:  Append to schedule array
 */
static int add_to_schedule(vaccine_system_t* system, uint32_t vaccine_id,
                          uint64_t scheduled_time, bool is_booster,
                          uint32_t original_id) {
    if (!system || !system->schedule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_vaccine_by_id: required parameter is NULL (system, system->schedule)");
        return -1;
    }
    if (system->schedule_count >= system->schedule_capacity) {
        NIMCP_LOGGING_WARN("Schedule capacity exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_vaccine_by_id: capacity exceeded");
        return -1;
    }

    vaccine_schedule_entry_t* entry = &system->schedule[system->schedule_count++];
    entry->vaccine_id = vaccine_id;
    entry->scheduled_time = scheduled_time;
    entry->is_booster = is_booster;
    entry->original_vaccine_id = original_id;

    return 0;
}

/**
 * @brief Process scheduled vaccines that are due
 *
 * WHAT: Administer vaccines that have reached scheduled time
 * WHY:  Execute scheduled vaccination plan
 * HOW:  Check each schedule entry against current time
 */
static void process_scheduled_vaccines(vaccine_system_t* system,
                                       uint64_t current_time) {
    if (!system || !system->schedule) return;

    /* Process due vaccines (iterate backwards to allow removal) */
    for (int i = (int)system->schedule_count - 1; i >= 0; i--) {
        vaccine_schedule_entry_t* entry = &system->schedule[i];

        if (entry->scheduled_time <= current_time) {
            /* Administer vaccine */
            if (entry->is_booster) {
                vaccine_booster(system, entry->original_vaccine_id);
            } else {
                vaccine_administer(system, entry->vaccine_id);
            }

            /* Remove from schedule (swap with last) */
            if (i < (int)system->schedule_count - 1) {
                system->schedule[i] = system->schedule[system->schedule_count - 1];
            }
            system->schedule_count--;
        }
    }
}

/**
 * @brief Update efficacy for all vaccines based on time decay
 *
 * WHAT: Apply time-based efficacy decay to all active vaccines
 * WHY:  Model waning immunity over time
 * HOW:  Reduce efficacy by decay_rate per day elapsed
 */
static void update_all_efficacies(vaccine_system_t* system, uint64_t delta_ms) {
    if (!system || !system->vaccines) return;

    float days_elapsed = (float)delta_ms / (1000.0f * 60.0f * 60.0f * 24.0f);

    for (size_t i = 0; i < system->vaccine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->vaccine_count > 256) {
            immune_vaccine_heartbeat("immune_vacci_loop",
                             (float)(i + 1) / (float)system->vaccine_count);
        }

        vaccine_entry_t* vaccine = &system->vaccines[i];

        if (vaccine->status == VACCINE_STATUS_ADMINISTERED ||
            vaccine->status == VACCINE_STATUS_BOOSTED) {

            /* Apply decay */
            float decay = vaccine->decay_rate * days_elapsed;
            vaccine->efficacy = fmaxf(0.0f, vaccine->efficacy - decay);

            /* Check expiration threshold */
            if (vaccine->efficacy < system->config.efficacy_threshold_expire) {
                vaccine->status = VACCINE_STATUS_EXPIRED;
                system->stats.expired_vaccines++;

                NIMCP_LOGGING_WARN("Vaccine %u expired (efficacy: %.2f)",
                                  vaccine->id, vaccine->efficacy);
            }
        }
    }
}

/**
 * @brief Update system statistics
 *
 * WHAT: Recalculate aggregate vaccine statistics
 * WHY:  Provide current system health snapshot
 * HOW:  Iterate vaccines, compute averages
 */
static void update_statistics(vaccine_system_t* system) {
    if (!system || !system->vaccines) return;

    uint32_t active = 0;
    float sum_efficacy = 0.0f;
    float min_eff = 1.0f;
    float max_eff = 0.0f;

    for (size_t i = 0; i < system->vaccine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->vaccine_count > 256) {
            immune_vaccine_heartbeat("immune_vacci_loop",
                             (float)(i + 1) / (float)system->vaccine_count);
        }

        vaccine_entry_t* v = &system->vaccines[i];

        if (v->status == VACCINE_STATUS_ADMINISTERED ||
            v->status == VACCINE_STATUS_BOOSTED) {
            active++;
            sum_efficacy += v->efficacy;
            if (v->efficacy < min_eff) min_eff = v->efficacy;
            if (v->efficacy > max_eff) max_eff = v->efficacy;
        }
    }

    system->stats.active_vaccines = active;
    system->stats.avg_efficacy = active > 0 ? sum_efficacy / active : 0.0f;
    system->stats.min_efficacy = active > 0 ? min_eff : 0.0f;
    system->stats.max_efficacy = active > 0 ? max_eff : 0.0f;
}

/**
 * @brief Calculate CRC32 checksum
 *
 * WHAT: Compute CRC32 checksum for data integrity
 * WHY:  Verify database file integrity
 * HOW:  Standard CRC32 algorithm
 */
static uint32_t calculate_checksum(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            immune_vaccine_heartbeat("immune_vacci_loop",
                             (float)(i + 1) / (float)len);
        }

        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && 8 > 256) {
                immune_vaccine_heartbeat("immune_vacci_loop",
                                 (float)(j + 1) / (float)8);
            }

            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}

/* ============================================================================
 * Lifecycle API - Implementation
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int vaccine_default_config(vaccine_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_default_conf", 0.0f);


    memset(config, 0, sizeof(vaccine_config_t));

    /* Capacity limits */
    config->max_vaccines = VACCINE_MAX_ENTRIES;
    config->max_scheduled = VACCINE_MAX_SCHEDULE;

    /* Default vaccine properties */
    config->default_attenuation = VACCINE_ATTENUATION_MODERATE;
    config->default_affinity = 0.8f;
    config->default_decay_rate = 0.001f;  /* 0.1% per day */

    /* Efficacy thresholds */
    config->efficacy_threshold_warn = VACCINE_EFFICACY_MODERATE;
    config->efficacy_threshold_expire = VACCINE_EFFICACY_MINIMAL;

    /* Booster settings */
    config->default_booster_interval_ms = VACCINE_BOOSTER_STANDARD;
    config->auto_schedule_boosters = true;

    /* Integration */
    config->enable_passive_import = true;
    config->enable_auto_vaccination = false;
    config->enable_logging = true;

    return 0;
}

/**
 * @brief Create vaccine system
 */
vaccine_system_t* vaccine_create(const vaccine_config_t* config,
                                 brain_immune_system_t* immune_system) {
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("Immune system is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_default_config: immune_system is NULL");
        return NULL;
    }

    /* Allocate system */
    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_create", 0.0f);


    vaccine_system_t* system = (vaccine_system_t*)nimcp_malloc(sizeof(vaccine_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("Failed to allocate vaccine system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_default_config: system is NULL");
        return NULL;
    }
    memset(system, 0, sizeof(vaccine_system_t));

    /* Set configuration */
    if (config) {
        system->config = *config;
    } else {
        vaccine_default_config(&system->config);
    }

    /* Allocate vaccine storage */
    system->vaccine_capacity = system->config.max_vaccines;
    system->vaccines = (vaccine_entry_t*)nimcp_malloc(
        system->vaccine_capacity * sizeof(vaccine_entry_t));
    if (!system->vaccines) {
        NIMCP_LOGGING_ERROR("Failed to allocate vaccine array");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vaccine_default_config: system->vaccines is NULL");
        return NULL;
    }
    memset(system->vaccines, 0, system->vaccine_capacity * sizeof(vaccine_entry_t));

    /* Allocate schedule */
    system->schedule_capacity = system->config.max_scheduled;
    system->schedule = (vaccine_schedule_entry_t*)nimcp_malloc(
        system->schedule_capacity * sizeof(vaccine_schedule_entry_t));
    if (!system->schedule) {
        NIMCP_LOGGING_ERROR("Failed to allocate schedule");
        nimcp_free(system->vaccines);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vaccine_default_config: system->schedule is NULL");
        return NULL;
    }
    memset(system->schedule, 0, system->schedule_capacity * sizeof(vaccine_schedule_entry_t));

    /* Integration */
    system->immune_system = immune_system;

    /* Thread safety */
    system->mutex = nimcp_mutex_create();
    if (!system->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(system->schedule);
        nimcp_free(system->vaccines);
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vaccine_default_config: system->mutex is NULL");
        return NULL;
    }

    /* State */
    system->running = false;
    system->start_time = get_timestamp_ms();
    system->next_vaccine_id = 1;

    NIMCP_LOGGING_INFO("Vaccine system created (capacity: %zu vaccines, %zu scheduled)",
                      system->vaccine_capacity, system->schedule_capacity);

    return system;
}

/**
 * @brief Destroy vaccine system
 */
void vaccine_destroy(vaccine_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_destroy", 0.0f);


    NIMCP_LOGGING_INFO("Destroying vaccine system");

    /* Stop if running */
    if (system->running) {
        vaccine_stop(system);
    }

    /* Free resources */
    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    if (system->schedule) {
        nimcp_free(system->schedule);
    }

    if (system->vaccines) {
        nimcp_free(system->vaccines);
    }

    nimcp_free(system);
    system = NULL;
}

/**
 * @brief Start vaccine system
 */
int vaccine_start(vaccine_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_start: system is NULL");
        return -1;
    }
    if (system->running) return 0;

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_start", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->running = true;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Vaccine system started");
    return 0;
}

/**
 * @brief Stop vaccine system
 */
int vaccine_stop(vaccine_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_stop: system is NULL");
        return -1;
    }
    if (!system->running) return 0;

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_stop", 0.0f);


    nimcp_mutex_lock(system->mutex);
    system->running = false;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Vaccine system stopped");
    return 0;
}

/* ============================================================================
 * Vaccine Entry Creation API - Implementation
 * ============================================================================ */

/**
 * @brief Create vaccine entry
 */
int vaccine_create_entry(vaccine_system_t* system, vaccine_type_t type,
                        const uint8_t* epitope, size_t epitope_len,
                        const char* name, uint32_t* vaccine_id) {
    if (!system || !epitope || epitope_len == 0 || !vaccine_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_stop: required parameter is NULL (system, epitope, vaccine_id)");
        return -1;
    }
    if (epitope_len > VACCINE_EPITOPE_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_stop: validation failed");
        return -1;
    }
    if (system->vaccine_count >= system->vaccine_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "vaccine_stop: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_create_entry", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Allocate vaccine entry */
    vaccine_entry_t* vaccine = &system->vaccines[system->vaccine_count++];
    memset(vaccine, 0, sizeof(vaccine_entry_t));

    /* Set properties */
    vaccine->id = system->next_vaccine_id++;
    vaccine->type = type;
    vaccine->status = VACCINE_STATUS_PENDING;

    /* Copy epitope */
    memcpy(vaccine->epitope, epitope, epitope_len);
    vaccine->epitope_len = epitope_len;

    /* Set name */
    if (name) {
        strncpy(vaccine->name, name, VACCINE_NAME_MAX_LEN - 1);
        vaccine->name[VACCINE_NAME_MAX_LEN - 1] = '\0';
    } else {
        snprintf(vaccine->name, VACCINE_NAME_MAX_LEN, "vaccine_%u", vaccine->id);
    }

    /* Default properties */
    vaccine->attenuation_factor = system->config.default_attenuation;
    vaccine->initial_affinity = system->config.default_affinity;
    vaccine->decay_rate = system->config.default_decay_rate;
    vaccine->antibody_class = ANTIBODY_IGG;
    vaccine->efficacy = 1.0f;
    vaccine->booster_interval_ms = system->config.default_booster_interval_ms;

    *vaccine_id = vaccine->id;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Created vaccine %u: %s (type: %s)",
                      vaccine->id, vaccine->name, vaccine_type_to_string(type));

    return 0;
}

/**
 * @brief Set vaccine properties
 */
int vaccine_set_properties(vaccine_system_t* system, uint32_t vaccine_id,
                          float attenuation, float affinity, float decay_rate) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_stop: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_set_properti", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_stop: vaccine is NULL");
        return -1;
    }

    vaccine->attenuation_factor = fmaxf(0.0f, fminf(1.0f, attenuation));
    vaccine->initial_affinity = fmaxf(0.0f, fminf(1.0f, affinity));
    vaccine->decay_rate = fmaxf(0.0f, decay_rate);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Set vaccine description
 */
int vaccine_set_description(vaccine_system_t* system, uint32_t vaccine_id,
                           const char* description) {
    if (!system || !description) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_stop: required parameter is NULL (system, description)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_set_descript", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_stop: vaccine is NULL");
        return -1;
    }

    strncpy(vaccine->description, description, VACCINE_DESCRIPTION_MAX_LEN - 1);
    vaccine->description[VACCINE_DESCRIPTION_MAX_LEN - 1] = '\0';

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Vaccine Administration API - Implementation
 * ============================================================================ */

/**
 * @brief Administer vaccine (direct memory injection)
 */
int vaccine_administer(vaccine_system_t* system, uint32_t vaccine_id) {
    if (!system || !system->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_administer: required parameter is NULL (system, system->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_administer", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_LOGGING_ERROR("Vaccine %u not found", vaccine_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_administer: vaccine is NULL");
        return -1;
    }

    /* First present the vaccine epitope as an antigen (low severity for vaccine) */
    uint32_t antigen_id = 0;
    int result = brain_immune_present_antigen(system->immune_system,
                                              ANTIGEN_SOURCE_MANUAL,
                                              vaccine->epitope,
                                              vaccine->epitope_len,
                                              1,  /* Low severity for vaccine */
                                              0,  /* No specific node */
                                              &antigen_id);
    if (result != 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_LOGGING_ERROR("Failed to present antigen for vaccine %u", vaccine_id);
        vaccine->status = VACCINE_STATUS_FAILED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_administer: validation failed");
        return -1;
    }

    /* Create memory B cell directly (bypassing activation) */
    uint32_t b_cell_id = 0;
    result = brain_immune_activate_b_cell(system->immune_system,
                                          antigen_id,
                                          &b_cell_id);

    if (result != 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_LOGGING_ERROR("Failed to create B cell for vaccine %u", vaccine_id);
        vaccine->status = VACCINE_STATUS_FAILED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_administer: validation failed");
        return -1;
    }

    /* Convert to memory immediately */
    result = brain_immune_b_cell_to_memory(system->immune_system, b_cell_id);
    if (result != 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_LOGGING_ERROR("Failed to convert B cell to memory for vaccine %u", vaccine_id);
        vaccine->status = VACCINE_STATUS_FAILED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_administer: validation failed");
        return -1;
    }

    /* Update vaccine state */
    vaccine->status = VACCINE_STATUS_ADMINISTERED;
    vaccine->administration_time = get_timestamp_ms();
    vaccine->memory_b_cell_id = b_cell_id;
    vaccine->efficacy = vaccine->initial_affinity;

    /* Update statistics */
    system->stats.total_vaccines++;

    /* Schedule booster if auto-scheduling enabled */
    if (system->config.auto_schedule_boosters && vaccine->booster_interval_ms > 0) {
        uint64_t booster_time = vaccine->administration_time + vaccine->booster_interval_ms;
        add_to_schedule(system, vaccine_id, booster_time, true, vaccine_id);
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Administered vaccine %u: %s (memory B cell: %u, efficacy: %.2f)",
                      vaccine->id, vaccine->name, b_cell_id, vaccine->efficacy);

    return 0;
}

/**
 * @brief Administer attenuated vaccine (reduced severity)
 */
int vaccine_administer_attenuated(vaccine_system_t* system, uint32_t vaccine_id,
                                  float severity_reduction) {
    if (!system || !system->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_administer: required parameter is NULL (system, system->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_administer_a", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_administer: vaccine is NULL");
        return -1;
    }

    /* Calculate attenuated severity */
    uint32_t original_severity = vaccine->severity > 0 ? vaccine->severity : 5;
    uint32_t attenuated_severity = (uint32_t)(original_severity * (1.0f - severity_reduction));
    if (attenuated_severity < 1) attenuated_severity = 1;

    /* Present attenuated antigen to immune system */
    uint32_t antigen_id = 0;
    int result = brain_immune_present_antigen(
        system->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        vaccine->epitope,
        vaccine->epitope_len,
        attenuated_severity,
        0, /* source node */
        &antigen_id
    );

    if (result != 0) {
        nimcp_mutex_unlock(system->mutex);
        vaccine->status = VACCINE_STATUS_FAILED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_administer: validation failed");
        return -1;
    }

    /* Update vaccine state */
    vaccine->status = VACCINE_STATUS_ADMINISTERED;
    vaccine->administration_time = get_timestamp_ms();
    vaccine->efficacy = vaccine->initial_affinity;

    system->stats.total_vaccines++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Administered attenuated vaccine %u (severity: %u -> %u)",
                      vaccine_id, original_severity, attenuated_severity);

    return 0;
}

/**
 * @brief Administer booster dose
 */
int vaccine_booster(vaccine_system_t* system, uint32_t vaccine_id) {
    if (!system || !system->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: required parameter is NULL (system, system->immune_system)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_booster", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: vaccine is NULL");
        return -1;
    }

    /* Boost efficacy */
    vaccine->efficacy = fminf(1.0f, vaccine->efficacy + 0.2f);
    vaccine->last_booster_time = get_timestamp_ms();
    vaccine->booster_count++;
    vaccine->status = VACCINE_STATUS_BOOSTED;

    /* Update statistics */
    system->stats.boosters_given++;

    /* Schedule next booster */
    if (system->config.auto_schedule_boosters && vaccine->booster_interval_ms > 0) {
        uint64_t next_booster = vaccine->last_booster_time + vaccine->booster_interval_ms;
        add_to_schedule(system, vaccine_id, next_booster, true, vaccine_id);
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOGGING_INFO("Administered booster for vaccine %u (booster #%u, efficacy: %.2f)",
                      vaccine_id, vaccine->booster_count, vaccine->efficacy);

    return 0;
}

/* ============================================================================
 * Passive Immunity API - Implementation
 * ============================================================================ */

/**
 * @brief Import passive immunity from external source
 */
int vaccine_import_passive_immunity(vaccine_system_t* system,
                                   const uint8_t* epitope, size_t epitope_len,
                                   float affinity, const char* source_description,
                                   uint32_t* vaccine_id) {
    if (!system || !epitope || !vaccine_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: required parameter is NULL (system, epitope, vaccine_id)");
        return -1;
    }
    if (!system->config.enable_passive_import) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: system->config is NULL");
        return -1;
    }

    /* Create passive vaccine entry */
    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_import_passi", 0.0f);


    uint32_t vid = 0;
    int result = vaccine_create_entry(system, VACCINE_TYPE_PASSIVE,
                                     epitope, epitope_len,
                                     "passive_immunity", &vid);
    if (result != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_booster: validation failed");
        return -1;
    }

    /* Set description */
    if (source_description) {
        vaccine_set_description(system, vid, source_description);
    }

    /* Set affinity */
    vaccine_set_properties(system, vid, 0.0f, affinity,
                          system->config.default_decay_rate);

    /* Administer immediately */
    result = vaccine_administer(system, vid);
    if (result != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_booster: validation failed");
        return -1;
    }

    *vaccine_id = vid;
    system->stats.passive_imports++;

    NIMCP_LOGGING_INFO("Imported passive immunity (vaccine %u, affinity: %.2f)",
                      vid, affinity);

    return 0;
}

/**
 * @brief Import vaccine database from file
 */
int vaccine_import_database(vaccine_system_t* system, const char* filepath,
                           uint32_t* imported_count) {
    if (!system || !filepath || !imported_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: required parameter is NULL (system, filepath, imported_count)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_import_datab", 0.0f);


    FILE* file = fopen(filepath, "rb");
    if (!file) {
        NIMCP_LOGGING_ERROR("Failed to open vaccine database: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: file is NULL");
        return -1;
    }

    /* Read header */
    vaccine_database_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        NIMCP_LOGGING_ERROR("Failed to read database header");
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_booster: validation failed");
        return -1;
    }

    /* Verify header */
    if (header.magic != VACCINE_DATABASE_MAGIC) {
        NIMCP_LOGGING_ERROR("Invalid database magic number");
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_booster: validation failed");
        return -1;
    }

    if (header.version != VACCINE_DATABASE_VERSION) {
        NIMCP_LOGGING_ERROR("Unsupported database version: %u", header.version);
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_booster: validation failed");
        return -1;
    }

    /* Import entries */
    uint32_t count = 0;
    for (uint32_t i = 0; i < header.entry_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && header.entry_count > 256) {
            immune_vaccine_heartbeat("immune_vacci_loop",
                             (float)(i + 1) / (float)header.entry_count);
        }

        vaccine_entry_t entry;
        if (fread(&entry, sizeof(entry), 1, file) != 1) {
            NIMCP_LOGGING_WARN("Failed to read vaccine entry %u", i);
            continue;
        }

        /* Create new vaccine from imported entry */
        uint32_t vaccine_id = 0;
        if (vaccine_create_entry(system, entry.type, entry.epitope,
                                entry.epitope_len, entry.name, &vaccine_id) == 0) {
            /* Copy properties */
            vaccine_set_properties(system, vaccine_id, entry.attenuation_factor,
                                 entry.initial_affinity, entry.decay_rate);
            vaccine_set_description(system, vaccine_id, entry.description);
            count++;
        }
    }

    fclose(file);

    *imported_count = count;
    system->stats.database_imports++;

    NIMCP_LOGGING_INFO("Imported %u vaccines from database: %s", count, filepath);

    return 0;
}

/**
 * @brief Export vaccine database to file
 */
int vaccine_export_database(vaccine_system_t* system, const char* filepath,
                           const char* description) {
    if (!system || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: required parameter is NULL (system, filepath)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_export_datab", 0.0f);


    FILE* file = fopen(filepath, "wb");
    if (!file) {
        NIMCP_LOGGING_ERROR("Failed to create database file: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_booster: file is NULL");
        return -1;
    }

    nimcp_mutex_lock(system->mutex);

    /* Write header */
    vaccine_database_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = VACCINE_DATABASE_MAGIC;
    header.version = VACCINE_DATABASE_VERSION;
    header.entry_count = (uint32_t)system->vaccine_count;
    header.creation_time = get_timestamp_ms();

    if (description) {
        strncpy(header.description, description, sizeof(header.description) - 1);
    }

    /* Calculate checksum (over vaccine data) */
    header.checksum = calculate_checksum(system->vaccines,
                                        system->vaccine_count * sizeof(vaccine_entry_t));

    fwrite(&header, sizeof(header), 1, file);

    /* Write vaccine entries */
    fwrite(system->vaccines, sizeof(vaccine_entry_t), system->vaccine_count, file);

    nimcp_mutex_unlock(system->mutex);

    fclose(file);

    system->stats.database_exports++;

    NIMCP_LOGGING_INFO("Exported %zu vaccines to database: %s",
                      system->vaccine_count, filepath);

    return 0;
}

/* ============================================================================
 * Scheduling API - Implementation
 * ============================================================================ */

/**
 * @brief Schedule vaccine for future administration
 */
int vaccine_schedule_add(vaccine_system_t* system, uint32_t vaccine_id,
                        uint64_t scheduled_time) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_schedule_add", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: vaccine is NULL");
        return -1;
    }

    int result = add_to_schedule(system, vaccine_id, scheduled_time, false, 0);
    if (result == 0) {
        vaccine->status = VACCINE_STATUS_SCHEDULED;
        vaccine->scheduled_time = scheduled_time;
    }

    nimcp_mutex_unlock(system->mutex);

    if (result == 0) {
        NIMCP_LOGGING_INFO("Scheduled vaccine %u for time %lu",
                          vaccine_id, (unsigned long)scheduled_time);
    }

    return result;
}

/**
 * @brief Schedule booster dose
 */
int vaccine_schedule_booster(vaccine_system_t* system, uint32_t vaccine_id,
                            uint64_t interval_ms) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_schedule_boo", 0.0f);


    uint64_t booster_time = get_timestamp_ms() + interval_ms;

    nimcp_mutex_lock(system->mutex);
    int result = add_to_schedule(system, vaccine_id, booster_time, true, vaccine_id);
    nimcp_mutex_unlock(system->mutex);

    if (result == 0) {
        NIMCP_LOGGING_INFO("Scheduled booster for vaccine %u in %lu ms",
                          vaccine_id, (unsigned long)interval_ms);
    }

    return result;
}

/**
 * @brief Cancel scheduled vaccine
 */
int vaccine_schedule_cancel(vaccine_system_t* system, uint32_t vaccine_id) {
    if (!system || !system->schedule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_schedule_cancel: required parameter is NULL (system, system->schedule)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_schedule_can", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Find and remove from schedule */
    for (size_t i = 0; i < system->schedule_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->schedule_count > 256) {
            immune_vaccine_heartbeat("immune_vacci_loop",
                             (float)(i + 1) / (float)system->schedule_count);
        }

        if (system->schedule[i].vaccine_id == vaccine_id) {
            /* Swap with last and decrement count */
            if (i < system->schedule_count - 1) {
                system->schedule[i] = system->schedule[system->schedule_count - 1];
            }
            system->schedule_count--;
            nimcp_mutex_unlock(system->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_schedule_cancel: operation failed");
    return -1;
}

/* ============================================================================
 * Efficacy Tracking API - Implementation
 * ============================================================================ */

/**
 * @brief Get vaccine efficacy
 */
int vaccine_get_efficacy(vaccine_system_t* system, uint32_t vaccine_id,
                        float* efficacy) {
    if (!system || !efficacy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_schedule_cancel: required parameter is NULL (system, efficacy)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_get_efficacy", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_schedule_cancel: vaccine is NULL");
        return -1;
    }

    *efficacy = vaccine->efficacy;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Record vaccine success
 */
int vaccine_record_success(vaccine_system_t* system, uint32_t vaccine_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_success: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_record_succe", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_success: vaccine is NULL");
        return -1;
    }

    vaccine->exposures_prevented++;
    system->stats.threats_prevented++;

    /* Boost efficacy slightly from real-world success */
    vaccine->efficacy = fminf(1.0f, vaccine->efficacy + 0.01f);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Record vaccine failure
 */
int vaccine_record_failure(vaccine_system_t* system, uint32_t vaccine_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_failure: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_record_failu", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_failure: vaccine is NULL");
        return -1;
    }

    vaccine->exposures_failed++;

    /* Reduce efficacy from failure */
    vaccine->efficacy = fmaxf(0.0f, vaccine->efficacy - 0.05f);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Update vaccine efficacy (decay over time)
 */
int vaccine_update_efficacy(vaccine_system_t* system, uint32_t vaccine_id,
                           uint64_t elapsed_ms) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_failure: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_update_effic", 0.0f);


    nimcp_mutex_lock(system->mutex);

    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    if (!vaccine) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_failure: vaccine is NULL");
        return -1;
    }

    float days = (float)elapsed_ms / (1000.0f * 60.0f * 60.0f * 24.0f);
    float decay = vaccine->decay_rate * days;
    vaccine->efficacy = fmaxf(0.0f, vaccine->efficacy - decay);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/* ============================================================================
 * Query API - Implementation
 * ============================================================================ */

/**
 * @brief Get vaccine entry by ID
 */
const vaccine_entry_t* vaccine_get_entry(vaccine_system_t* system,
                                        uint32_t vaccine_id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_get_entry", 0.0f);


    nimcp_mutex_lock(system->mutex);
    vaccine_entry_t* vaccine = find_vaccine_by_id(system, vaccine_id);
    nimcp_mutex_unlock(system->mutex);

    return vaccine;
}

/**
 * @brief Find vaccine by epitope
 */
int vaccine_find_by_epitope(vaccine_system_t* system, const uint8_t* epitope,
                           size_t epitope_len, uint32_t* vaccine_id) {
    if (!system || !epitope || !vaccine_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_failure: required parameter is NULL (system, epitope, vaccine_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_find_by_epit", 0.0f);


    nimcp_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->vaccine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->vaccine_count > 256) {
            immune_vaccine_heartbeat("immune_vacci_loop",
                             (float)(i + 1) / (float)system->vaccine_count);
        }

        vaccine_entry_t* v = &system->vaccines[i];
        if (v->epitope_len == epitope_len &&
            memcmp(v->epitope, epitope, epitope_len) == 0) {
            *vaccine_id = v->id;
            nimcp_mutex_unlock(system->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vaccine_record_failure: operation failed");
    return -1;
}

/**
 * @brief Get all active vaccines
 */
int vaccine_get_active_vaccines(vaccine_system_t* system, uint32_t* vaccine_ids,
                               size_t max_count, size_t* count) {
    if (!system || !vaccine_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_record_failure: required parameter is NULL (system, vaccine_ids, count)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_get_active_v", 0.0f);


    nimcp_mutex_lock(system->mutex);

    size_t active_count = 0;
    for (size_t i = 0; i < system->vaccine_count && active_count < max_count; i++) {
        vaccine_entry_t* v = &system->vaccines[i];
        if (v->status == VACCINE_STATUS_ADMINISTERED ||
            v->status == VACCINE_STATUS_BOOSTED) {
            vaccine_ids[active_count++] = v->id;
        }
    }

    *count = active_count;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

/**
 * @brief Get vaccine statistics
 */
int vaccine_get_stats(vaccine_system_t* system, vaccine_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_get_stats", 0.0f);


    nimcp_mutex_lock(system->mutex);
    update_statistics(system);
    *stats = system->stats;
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Update API - Implementation
 * ============================================================================ */

/**
 * @brief Update vaccine system
 */
int vaccine_update(vaccine_system_t* system, uint64_t delta_ms) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vaccine_update: system is NULL");
        return -1;
    }
    if (!system->running) return 0;

    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_update", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Process scheduled vaccines */
    uint64_t current_time = get_timestamp_ms();
    process_scheduled_vaccines(system, current_time);

    /* Update efficacy decay */
    update_all_efficacies(system, delta_ms);

    /* Update statistics */
    update_statistics(system);

    nimcp_mutex_unlock(system->mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about immune vaccine
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int vaccine_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    immune_vaccine_heartbeat("immune_vacci_vaccine_query_self_k", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Immune_Vaccine");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                immune_vaccine_heartbeat("immune_vacci_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Immune vaccine self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Immune_Vaccine");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Immune_Vaccine");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void immune_vaccine_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_immune_vaccine_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int immune_vaccine_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_vaccine_training_begin: NULL argument");
        return -1;
    }
    immune_vaccine_heartbeat_instance(NULL, "immune_vaccine_training_begin", 0.0f);
    return 0;
}

int immune_vaccine_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_vaccine_training_end: NULL argument");
        return -1;
    }
    immune_vaccine_heartbeat_instance(NULL, "immune_vaccine_training_end", 1.0f);
    return 0;
}

int immune_vaccine_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_vaccine_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    immune_vaccine_heartbeat_instance(NULL, "immune_vaccine_training_step", progress);
    return 0;
}
