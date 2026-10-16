/**
 * @file nimcp_immune_vaccine.h
 * @brief Immune Vaccine System - Pre-training and Passive Immunity
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Vaccine-like pre-training system for brain immune memory formation
 *       without triggering full inflammatory immune responses.
 * WHY:  Enable proactive immunity, faster threat responses, and reduced
 *       system stress during actual attacks via pre-loaded threat knowledge.
 * HOW:  Directly inject memory B cells into brain immune system, bypassing
 *       the full activation → plasma → memory lifecycle for known threats.
 *
 * BIOLOGICAL BASIS:
 * ```
 * VACCINE TYPE                 BIOLOGICAL MECHANISM              NIMCP IMPLEMENTATION
 * ────────────────────────────────────────────────────────────────────────────────────
 * Live attenuated           → Weakened pathogen exposure      → Reduced severity antigen
 * Inactivated (killed)      → Dead pathogen fragments         → Memory-only, no activation
 * Subunit                   → Specific antigen proteins       → Epitope pattern only
 * mRNA vaccine              → Antigen synthesis instructions  → Pattern generation rules
 * Passive immunity          → Antibody transfer               → Memory cell import
 * Booster dose              → Re-exposure for memory refresh  → Memory affinity boost
 * ```
 *
 * VACCINE WORKFLOW:
 * ```
 * ┌──────────────────────────────────────────────────────────────────────────┐
 * │                          VACCINE ADMINISTRATION                           │
 * ├──────────────────────────────────────────────────────────────────────────┤
 * │                                                                           │
 * │   Traditional Immune Response (WITHOUT Vaccine):                         │
 * │   ────────────────────────────────────────────────                       │
 * │   Antigen → Recognition → B Cell Activation → T Helper →                 │
 * │   Plasma → Antibody Production → Neutralization → Memory                 │
 * │   [SLOW: 100-1000ms, HIGH inflammation, resource intensive]              │
 * │                                                                           │
 * │   Vaccine Pre-Training (DIRECT Memory Injection):                        │
 * │   ────────────────────────────────────────────────                       │
 * │   Vaccine Entry → Memory B Cell Creation → [READY]                       │
 * │   [FAST: <1ms, NO inflammation, minimal resources]                       │
 * │                                                                           │
 * │   Vaccinated Response (Secondary Response):                              │
 * │   ────────────────────────────────────────────────                       │
 * │   Antigen → Memory Recognition → Rapid Antibody Production               │
 * │   [FASTER: 10-50ms, REDUCED inflammation, efficient]                     │
 * │                                                                           │
 * └──────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * USE CASES:
 * 1. **Known Threat Database**: Import signatures from external threat intelligence
 * 2. **Cross-Instance Learning**: Share learned threats between NIMCP instances
 * 3. **Rapid Deployment**: Pre-vaccinate new systems with common threat patterns
 * 4. **Testing & Development**: Inject test threats without triggering responses
 * 5. **Booster Updates**: Refresh existing immunity with updated signatures
 *
 * DESIGN PATTERNS:
 * - Facade: Simplified vaccine API over complex immune internals
 * - Template Method: Standardized vaccine administration pipeline
 * - Strategy: Multiple vaccine types (attenuated, inactivated, etc.)
 * - Repository: Vaccine database import/export
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_IMMUNE_VACCINE_H
#define NIMCP_IMMUNE_VACCINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VACCINE_MAX_ENTRIES           1024   /**< Max vaccine entries */
#define VACCINE_MAX_SCHEDULE          256    /**< Max scheduled vaccines */
#define VACCINE_EPITOPE_SIZE          64     /**< Vaccine signature size */
#define VACCINE_NAME_MAX_LEN          64     /**< Max vaccine name length */
#define VACCINE_DESCRIPTION_MAX_LEN   256    /**< Max description length */
#define VACCINE_DATABASE_MAGIC        0x4E494D56  /**< "NIMV" magic number */
#define VACCINE_DATABASE_VERSION      1      /**< Database format version */

/* Attenuation factors */
#define VACCINE_ATTENUATION_MILD      0.1f   /**< 90% severity reduction */
#define VACCINE_ATTENUATION_MODERATE  0.3f   /**< 70% severity reduction */
#define VACCINE_ATTENUATION_STRONG    0.5f   /**< 50% severity reduction */

/* Efficacy thresholds */
#define VACCINE_EFFICACY_EXCELLENT    0.95f  /**< 95%+ protection */
#define VACCINE_EFFICACY_GOOD         0.80f  /**< 80-95% protection */
#define VACCINE_EFFICACY_MODERATE     0.60f  /**< 60-80% protection */
#define VACCINE_EFFICACY_POOR         0.40f  /**< 40-60% protection */
#define VACCINE_EFFICACY_MINIMAL      0.20f  /**< <40% protection */

/* Booster timing (milliseconds) */
#define VACCINE_BOOSTER_EARLY         (30L * 24 * 3600 * 1000)   /**< 30 days */
#define VACCINE_BOOSTER_STANDARD      (180L * 24 * 3600 * 1000)  /**< 6 months */
#define VACCINE_BOOSTER_LATE          (365L * 24 * 3600 * 1000)  /**< 1 year */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct vaccine_system vaccine_system_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Vaccine type classification
 *
 * BIOLOGICAL BASIS:
 * Different vaccine types use different mechanisms to induce immunity.
 * Live attenuated = weakened but active pathogen
 * Inactivated = killed pathogen
 * Subunit = specific protein fragments
 * mRNA = genetic instructions for antigen production
 * Passive = pre-formed antibodies/memory
 */
typedef enum {
    VACCINE_TYPE_ATTENUATED = 0,   /**< Live attenuated (reduced severity) */
    VACCINE_TYPE_INACTIVATED,      /**< Inactivated (memory only, no activation) */
    VACCINE_TYPE_SUBUNIT,          /**< Subunit (epitope pattern only) */
    VACCINE_TYPE_MRNA,             /**< mRNA-style (pattern generation rules) */
    VACCINE_TYPE_PASSIVE,          /**< Passive immunity (memory cell import) */
    VACCINE_TYPE_COUNT
} vaccine_type_t;

/**
 * @brief Vaccine administration status
 */
typedef enum {
    VACCINE_STATUS_PENDING = 0,    /**< Not yet administered */
    VACCINE_STATUS_SCHEDULED,      /**< Scheduled for future administration */
    VACCINE_STATUS_ADMINISTERED,   /**< Successfully administered */
    VACCINE_STATUS_BOOSTED,        /**< Booster dose given */
    VACCINE_STATUS_EXPIRED,        /**< Immunity expired */
    VACCINE_STATUS_FAILED          /**< Administration failed */
} vaccine_status_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Vaccine entry - single threat signature for immunization
 *
 * Represents one threat pattern to be pre-loaded into immune memory.
 * Each entry becomes a memory B cell after administration.
 */
typedef struct {
    uint32_t id;                           /**< Unique vaccine ID */
    vaccine_type_t type;                   /**< Vaccine type */
    vaccine_status_t status;               /**< Administration status */

    /* Threat signature */
    uint8_t epitope[VACCINE_EPITOPE_SIZE]; /**< Threat pattern */
    size_t epitope_len;                    /**< Pattern length */
    uint32_t severity;                     /**< Original threat severity (1-10) */

    /* Metadata */
    char name[VACCINE_NAME_MAX_LEN];                   /**< Vaccine name */
    char description[VACCINE_DESCRIPTION_MAX_LEN];     /**< Description */

    /* Vaccine properties */
    float attenuation_factor;              /**< Severity reduction (0-1) */
    float initial_affinity;                /**< Initial memory affinity (0-1) */
    brain_antibody_class_t antibody_class; /**< Antibody class to produce */

    /* Administration tracking */
    uint64_t administration_time;          /**< When administered */
    uint64_t last_booster_time;            /**< Last booster dose */
    uint32_t booster_count;                /**< Number of boosters */
    uint32_t memory_b_cell_id;             /**< Created memory B cell ID */

    /* Efficacy tracking */
    float efficacy;                        /**< Current efficacy (0-1) */
    float decay_rate;                      /**< Efficacy decay per day */
    uint32_t exposures_prevented;          /**< Successful preventions */
    uint32_t exposures_failed;             /**< Failed preventions */

    /* Scheduling */
    uint64_t scheduled_time;               /**< When to administer (0=immediate) */
    uint64_t booster_interval_ms;          /**< Booster frequency */
} vaccine_entry_t;

/**
 * @brief Vaccine schedule entry
 *
 * Tracks vaccines scheduled for future administration.
 */
typedef struct {
    uint32_t vaccine_id;                   /**< Vaccine to administer */
    uint64_t scheduled_time;               /**< When to administer */
    bool is_booster;                       /**< Is this a booster dose? */
    uint32_t original_vaccine_id;          /**< Original vaccine (if booster) */
} vaccine_schedule_entry_t;

/**
 * @brief Vaccine database file header
 *
 * Used for import/export of vaccine databases.
 */
typedef struct {
    uint32_t magic;                        /**< Magic number (VACCINE_DATABASE_MAGIC) */
    uint32_t version;                      /**< Database format version */
    uint32_t entry_count;                  /**< Number of vaccine entries */
    uint64_t creation_time;                /**< Database creation time */
    char description[256];                 /**< Database description */
    uint32_t checksum;                     /**< CRC32 checksum */
    uint8_t reserved[236];                 /**< Reserved for future use */
} vaccine_database_header_t;

/**
 * @brief Vaccine system configuration
 */
typedef struct {
    /* Capacity limits */
    size_t max_vaccines;                   /**< Max vaccine entries */
    size_t max_scheduled;                  /**< Max scheduled vaccines */

    /* Default vaccine properties */
    float default_attenuation;             /**< Default attenuation factor */
    float default_affinity;                /**< Default initial affinity */
    float default_decay_rate;              /**< Default efficacy decay */

    /* Efficacy thresholds */
    float efficacy_threshold_warn;         /**< Warn below this */
    float efficacy_threshold_expire;       /**< Expire below this */

    /* Booster settings */
    uint64_t default_booster_interval_ms;  /**< Default booster frequency */
    bool auto_schedule_boosters;           /**< Auto-schedule boosters */

    /* Integration */
    bool enable_passive_import;            /**< Allow passive immunity */
    bool enable_auto_vaccination;          /**< Auto-vaccinate on threat */
    bool enable_logging;                   /**< Enable vaccine logging */
} vaccine_config_t;

/**
 * @brief Vaccine system statistics
 */
typedef struct {
    uint32_t total_vaccines;               /**< Total vaccines administered */
    uint32_t active_vaccines;              /**< Currently active vaccines */
    uint32_t expired_vaccines;             /**< Expired vaccines */
    uint32_t boosters_given;               /**< Total boosters administered */

    uint32_t threats_prevented;            /**< Threats prevented by vaccines */
    uint32_t passive_imports;              /**< Passive immunity imports */
    uint32_t database_imports;             /**< Database imports */
    uint32_t database_exports;             /**< Database exports */

    float avg_efficacy;                    /**< Average vaccine efficacy */
    float min_efficacy;                    /**< Minimum efficacy */
    float max_efficacy;                    /**< Maximum efficacy */
} vaccine_stats_t;

/**
 * @brief Vaccine system state
 */
struct vaccine_system {
    vaccine_config_t config;               /**< Configuration */

    /* Vaccine storage */
    vaccine_entry_t* vaccines;
    size_t vaccine_count;
    size_t vaccine_capacity;
    uint32_t next_vaccine_id;

    /* Schedule */
    vaccine_schedule_entry_t* schedule;
    size_t schedule_count;
    size_t schedule_capacity;

    /* Integration */
    brain_immune_system_t* immune_system;  /**< Brain immune system */

    /* Statistics */
    vaccine_stats_t stats;

    /* Thread safety */
    void* mutex;                           /**< Platform mutex */

    /* State */
    bool running;                          /**< System active */
    uint64_t start_time;                   /**< System start time */
};

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for vaccine administration
 */
typedef void (*vaccine_administer_cb_t)(
    vaccine_system_t* system,
    const vaccine_entry_t* vaccine,
    bool success,
    void* user_data
);

/**
 * @brief Callback for booster administration
 */
typedef void (*vaccine_booster_cb_t)(
    vaccine_system_t* system,
    const vaccine_entry_t* vaccine,
    uint32_t booster_number,
    void* user_data
);

/**
 * @brief Callback for efficacy warning
 */
typedef void (*vaccine_efficacy_warn_cb_t)(
    vaccine_system_t* system,
    const vaccine_entry_t* vaccine,
    float current_efficacy,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default vaccine configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int vaccine_default_config(vaccine_config_t* config);

/**
 * @brief Create vaccine system
 *
 * WHAT: Initialize vaccine pre-training system
 * WHY:  Enable proactive immune memory formation
 * HOW:  Allocate pools, connect to brain immune
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system to integrate with
 * @return New vaccine system or NULL on failure
 */
vaccine_system_t* vaccine_create(
    const vaccine_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy vaccine system
 *
 * WHAT: Clean up vaccine system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free pools, clear schedules
 *
 * @param system System to destroy
 */
void vaccine_destroy(vaccine_system_t* system);

/**
 * @brief Start vaccine system
 *
 * WHAT: Activate vaccine administration and scheduling
 * WHY:  Begin scheduled vaccinations
 * HOW:  Enable processing of schedule queue
 *
 * @param system Vaccine system
 * @return 0 on success
 */
int vaccine_start(vaccine_system_t* system);

/**
 * @brief Stop vaccine system
 *
 * WHAT: Deactivate vaccine system
 * WHY:  Graceful shutdown
 * HOW:  Complete pending administrations
 *
 * @param system Vaccine system
 * @return 0 on success
 */
int vaccine_stop(vaccine_system_t* system);

/* ============================================================================
 * Vaccine Entry Creation API
 * ============================================================================ */

/**
 * @brief Create vaccine entry
 *
 * WHAT: Define new vaccine for threat pattern
 * WHY:  Prepare vaccine for administration
 * HOW:  Allocate entry, set properties
 *
 * @param system Vaccine system
 * @param type Vaccine type
 * @param epitope Threat signature
 * @param epitope_len Signature length
 * @param name Vaccine name
 * @param vaccine_id Output: assigned vaccine ID
 * @return 0 on success
 */
int vaccine_create_entry(
    vaccine_system_t* system,
    vaccine_type_t type,
    const uint8_t* epitope,
    size_t epitope_len,
    const char* name,
    uint32_t* vaccine_id
);

/**
 * @brief Set vaccine properties
 *
 * WHAT: Configure vaccine entry properties
 * WHY:  Customize vaccine behavior
 * HOW:  Update entry fields
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to configure
 * @param attenuation Attenuation factor (0-1)
 * @param affinity Initial memory affinity (0-1)
 * @param decay_rate Efficacy decay per day (0-1)
 * @return 0 on success
 */
int vaccine_set_properties(
    vaccine_system_t* system,
    uint32_t vaccine_id,
    float attenuation,
    float affinity,
    float decay_rate
);

/**
 * @brief Set vaccine description
 *
 * WHAT: Add human-readable description
 * WHY:  Document vaccine purpose
 * HOW:  Copy description string
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to describe
 * @param description Description text
 * @return 0 on success
 */
int vaccine_set_description(
    vaccine_system_t* system,
    uint32_t vaccine_id,
    const char* description
);

/* ============================================================================
 * Vaccine Administration API
 * ============================================================================ */

/**
 * @brief Administer vaccine (direct memory injection)
 *
 * WHAT: Create memory B cell directly without immune activation
 * WHY:  Pre-load threat knowledge without inflammatory response
 * HOW:  Bypass activation cycle, create memory B cell immediately
 *
 * BIOLOGICAL BASIS:
 * Inactivated vaccines provide immunity without causing disease.
 * Memory B cells are created directly without full immune response.
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to administer
 * @return 0 on success
 */
int vaccine_administer(
    vaccine_system_t* system,
    uint32_t vaccine_id
);

/**
 * @brief Administer attenuated vaccine (reduced severity)
 *
 * WHAT: Trigger immune response with reduced severity
 * WHY:  Live attenuated vaccines provide strong immunity with minimal risk
 * HOW:  Present antigen with reduced severity, allow normal response
 *
 * BIOLOGICAL BASIS:
 * Live attenuated vaccines use weakened pathogens to trigger immunity
 * without causing full disease symptoms.
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to administer
 * @param severity_reduction Severity reduction factor (0-1, higher=more reduction)
 * @return 0 on success
 */
int vaccine_administer_attenuated(
    vaccine_system_t* system,
    uint32_t vaccine_id,
    float severity_reduction
);

/**
 * @brief Administer booster dose
 *
 * WHAT: Refresh existing vaccine immunity
 * WHY:  Maintain high efficacy over time
 * HOW:  Boost memory B cell affinity, reset efficacy
 *
 * BIOLOGICAL BASIS:
 * Booster doses re-expose immune system to antigen, strengthening
 * memory response and extending protection duration.
 *
 * @param system Vaccine system
 * @param vaccine_id Original vaccine to boost
 * @return 0 on success
 */
int vaccine_booster(
    vaccine_system_t* system,
    uint32_t vaccine_id
);

/* ============================================================================
 * Passive Immunity API
 * ============================================================================ */

/**
 * @brief Import passive immunity from external source
 *
 * WHAT: Import memory B cell from another system
 * WHY:  Share learned immunity across instances
 * HOW:  Create memory B cell from external signature
 *
 * BIOLOGICAL BASIS:
 * Passive immunity transfers pre-formed antibodies/memory cells
 * (e.g., maternal antibodies to newborn).
 *
 * @param system Vaccine system
 * @param epitope External threat signature
 * @param epitope_len Signature length
 * @param affinity Memory affinity (0-1)
 * @param source_description Source of immunity
 * @param vaccine_id Output: created vaccine ID
 * @return 0 on success
 */
int vaccine_import_passive_immunity(
    vaccine_system_t* system,
    const uint8_t* epitope,
    size_t epitope_len,
    float affinity,
    const char* source_description,
    uint32_t* vaccine_id
);

/**
 * @brief Import vaccine database from file
 *
 * WHAT: Load vaccine entries from external database file
 * WHY:  Share threat knowledge across systems and deployments
 * HOW:  Parse file format, create vaccine entries
 *
 * @param system Vaccine system
 * @param filepath Path to database file
 * @param imported_count Output: number of vaccines imported
 * @return 0 on success
 */
int vaccine_import_database(
    vaccine_system_t* system,
    const char* filepath,
    uint32_t* imported_count
);

/**
 * @brief Export vaccine database to file
 *
 * WHAT: Save vaccine entries to database file
 * WHY:  Share learned immunity with other systems
 * HOW:  Serialize vaccines to file format
 *
 * @param system Vaccine system
 * @param filepath Path to output file
 * @param description Database description
 * @return 0 on success
 */
int vaccine_export_database(
    vaccine_system_t* system,
    const char* filepath,
    const char* description
);

/* ============================================================================
 * Scheduling API
 * ============================================================================ */

/**
 * @brief Schedule vaccine for future administration
 *
 * WHAT: Queue vaccine for administration at specific time
 * WHY:  Controlled, gradual immunity rollout
 * HOW:  Add to schedule queue
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to schedule
 * @param scheduled_time When to administer (ms timestamp)
 * @return 0 on success
 */
int vaccine_schedule_add(
    vaccine_system_t* system,
    uint32_t vaccine_id,
    uint64_t scheduled_time
);

/**
 * @brief Schedule booster dose
 *
 * WHAT: Queue booster for future administration
 * WHY:  Maintain immunity over time
 * HOW:  Schedule booster at appropriate interval
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to boost
 * @param interval_ms Time until booster (relative to now)
 * @return 0 on success
 */
int vaccine_schedule_booster(
    vaccine_system_t* system,
    uint32_t vaccine_id,
    uint64_t interval_ms
);

/**
 * @brief Cancel scheduled vaccine
 *
 * WHAT: Remove vaccine from schedule queue
 * WHY:  Cancel unnecessary vaccination
 * HOW:  Remove from schedule array
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to cancel
 * @return 0 on success
 */
int vaccine_schedule_cancel(
    vaccine_system_t* system,
    uint32_t vaccine_id
);

/* ============================================================================
 * Efficacy Tracking API
 * ============================================================================ */

/**
 * @brief Get vaccine efficacy
 *
 * WHAT: Query current vaccine protection level
 * WHY:  Monitor immunity strength over time
 * HOW:  Return efficacy value (0-1)
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to query
 * @param efficacy Output: current efficacy (0-1)
 * @return 0 on success
 */
int vaccine_get_efficacy(
    vaccine_system_t* system,
    uint32_t vaccine_id,
    float* efficacy
);

/**
 * @brief Record vaccine success
 *
 * WHAT: Record successful threat prevention
 * WHY:  Track real-world vaccine effectiveness
 * HOW:  Increment success counter, update efficacy
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine that prevented threat
 * @return 0 on success
 */
int vaccine_record_success(
    vaccine_system_t* system,
    uint32_t vaccine_id
);

/**
 * @brief Record vaccine failure
 *
 * WHAT: Record failed threat prevention
 * WHY:  Track vaccine limitations
 * HOW:  Increment failure counter, update efficacy
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine that failed
 * @return 0 on success
 */
int vaccine_record_failure(
    vaccine_system_t* system,
    uint32_t vaccine_id
);

/**
 * @brief Update vaccine efficacy (decay over time)
 *
 * WHAT: Apply time-based efficacy decay
 * WHY:  Model waning immunity
 * HOW:  Reduce efficacy based on decay rate
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine to update
 * @param elapsed_ms Time elapsed since last update
 * @return 0 on success
 */
int vaccine_update_efficacy(
    vaccine_system_t* system,
    uint32_t vaccine_id,
    uint64_t elapsed_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get vaccine entry by ID
 *
 * @param system Vaccine system
 * @param vaccine_id Vaccine ID
 * @return Vaccine entry or NULL if not found
 */
const vaccine_entry_t* vaccine_get_entry(
    vaccine_system_t* system,
    uint32_t vaccine_id
);

/**
 * @brief Find vaccine by epitope
 *
 * WHAT: Search for vaccine matching threat signature
 * WHY:  Check if threat already vaccinated
 * HOW:  Compare epitope patterns
 *
 * @param system Vaccine system
 * @param epitope Threat signature to match
 * @param epitope_len Signature length
 * @param vaccine_id Output: matching vaccine ID (if found)
 * @return 0 if found, -1 if not found
 */
int vaccine_find_by_epitope(
    vaccine_system_t* system,
    const uint8_t* epitope,
    size_t epitope_len,
    uint32_t* vaccine_id
);

/**
 * @brief Get all active vaccines
 *
 * WHAT: List all currently active vaccines
 * WHY:  Monitor vaccination status
 * HOW:  Filter by status, return IDs
 *
 * @param system Vaccine system
 * @param vaccine_ids Output: array of vaccine IDs
 * @param max_count Maximum IDs to return
 * @param count Output: actual count returned
 * @return 0 on success
 */
int vaccine_get_active_vaccines(
    vaccine_system_t* system,
    uint32_t* vaccine_ids,
    size_t max_count,
    size_t* count
);

/**
 * @brief Get vaccine statistics
 *
 * @param system Vaccine system
 * @param stats Output statistics
 * @return 0 on success
 */
int vaccine_get_stats(
    vaccine_system_t* system,
    vaccine_stats_t* stats
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update vaccine system
 *
 * WHAT: Process scheduled vaccines and efficacy decay
 * WHY:  Advance vaccine state machine
 * HOW:  Process schedule, update efficacy, trigger boosters
 *
 * @param system Vaccine system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int vaccine_update(
    vaccine_system_t* system,
    uint64_t delta_ms
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* vaccine_type_to_string(vaccine_type_t type);
const char* vaccine_status_to_string(vaccine_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMMUNE_VACCINE_H */
