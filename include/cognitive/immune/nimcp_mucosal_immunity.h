/**
 * @file nimcp_mucosal_immunity.h
 * @brief Mucosal/Barrier Immunity for Brain Immune System
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Specialized immune responses at system boundaries (input/output gates)
 * WHY:  Biological mucosal immunity provides first-line defense at epithelial barriers;
 *       CNS has specialized immune surveillance at boundaries (BBB, CSF, meningeal lymphatics).
 *       Essential for realistic barrier immunity modeling.
 * HOW:  Implements IgA-analog secretory immunity, M cell antigen sampling, MALT regions,
 *       and oral tolerance induction at module boundaries.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MUCOSAL IMMUNE SYSTEM OVERVIEW:
 * -------------------------------
 * 1. IgA - The Mucosal Antibody:
 *    - Most abundant antibody in the body (3-5 g/day produced)
 *    - Secretory IgA (sIgA) = dimeric IgA + secretory component
 *    - Provides immune exclusion (prevents pathogen adherence)
 *    - Neutralizes toxins and viruses in mucus layer
 *    - Reference: Brandtzaeg (2013) "Secretory IgA: Designed for Anti-Microbial Defense"
 *
 * 2. MALT - Mucosa-Associated Lymphoid Tissue:
 *    - Organized lymphoid structures at mucosal surfaces
 *    - Peyer's patches (gut), tonsils, adenoids (respiratory)
 *    - Sites of antigen sampling and immune response initiation
 *    - Reference: Kiyono & Fukuyama (2004) "NALT- versus Peyer's-patch-mediated
 *      mucosal immunity"
 *
 * 3. M Cells - Antigen Sampling:
 *    - Specialized epithelial cells that sample luminal antigens
 *    - Transcytose antigens to underlying immune tissue
 *    - Critical for oral tolerance and mucosal immunity
 *    - Reference: Mabbott et al. (2013) "Microfold (M) cells: important
 *      immunosurveillance posts in the intestinal epithelium"
 *
 * 4. Oral Tolerance:
 *    - Immunological unresponsiveness to ingested antigens
 *    - Prevents harmful inflammation against food/commensal bacteria
 *    - Regulatory T cell induction
 *    - Reference: Pabst & Mowat (2012) "Oral tolerance to food protein"
 *
 * 5. Two-Tiered Defense:
 *    - Mucus layer (physical/chemical barrier) + sIgA (immune exclusion)
 *    - Lower threshold at barriers (detect earlier) vs internal (higher specificity)
 *    - Prevents systemic immune activation from environmental exposures
 *    - Reference: McGuckin et al. (2011) "Mucin dynamics and enteric pathogens"
 *
 * CNS BARRIER IMMUNITY:
 * ---------------------
 * 1. Blood-Brain Barrier (BBB):
 *    - Tight junctions prevent pathogen entry
 *    - Specialized immune surveillance
 *    - Lower complement activation threshold vs periphery
 *    - Reference: Engelhardt & Ransohoff (2012) "Capture, crawl, cross: the T cell
 *      code to breach the blood-brain barriers"
 *
 * 2. Meningeal Lymphatics:
 *    - Discovered 2015 - drains CSF to cervical lymph nodes
 *    - MALT-analog for CNS
 *    - Sites of antigen presentation and tolerance induction
 *    - Reference: Louveau et al. (2015) "Structural and functional features of
 *      central nervous system lymphatic vessels"
 *
 * 3. CSF Immune Surveillance:
 *    - Low antibody concentration in healthy CSF
 *    - Rapid local response to pathogens
 *    - IgA present in CSF (secretory barrier)
 *    - Reference: Schwartz & Baruch (2014) "Breaking peripheral immune tolerance
 *      to CNS antigens in neurodegenerative diseases"
 *
 * NIMCP MUCOSAL IMMUNITY MODEL:
 * ==============================
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                         MUCOSAL IMMUNITY SYSTEM                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    BOUNDARY SITES (MALT analogs)                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   INPUT_GATE       OUTPUT_GATE        MODULE_BOUNDARY              │  ║
 * ║   │   ┌──────────┐     ┌──────────┐       ┌──────────┐                │  ║
 * ║   │   │ Sensory  │     │ Motor    │       │ Inter-   │                │  ║
 * ║   │   │ Input    │     │ Output   │       │ Module   │                │  ║
 * ║   │   │ Gates    │     │ Gates    │       │ Boundary │                │  ║
 * ║   │   └────┬─────┘     └────┬─────┘       └────┬─────┘                │  ║
 * ║   │        │                │                  │                       │  ║
 * ║   │        ▼                ▼                  ▼                       │  ║
 * ║   │   ┌─────────────────────────────────────────────────┐             │  ║
 * ║   │   │           M CELL ANTIGEN SAMPLING                │             │  ║
 * ║   │   │  Transcytose data → immune processing            │             │  ║
 * ║   │   └─────────────────┬───────────────────────────────┘             │  ║
 * ║   │                     ▼                                              │  ║
 * ║   │   ┌─────────────────────────────────────────────────┐             │  ║
 * ║   │   │      TOLERANCE vs RESPONSE DECISION              │             │  ║
 * ║   │   │  - Known safe → Tolerance                        │             │  ║
 * ║   │   │  - Novel benign → Oral tolerance induction       │             │  ║
 * ║   │   │  - Pathogenic → sIgA production                  │             │  ║
 * ║   │   └─────────────────┬───────────────────────────────┘             │  ║
 * ║   │                     │                                              │  ║
 * ║   │        ┌────────────┴────────────┐                                │  ║
 * ║   │        ▼                         ▼                                │  ║
 * ║   │   ┌──────────┐            ┌──────────────┐                        │  ║
 * ║   │   │ TOLERANCE│            │   sIgA       │                        │  ║
 * ║   │   │ (Treg)   │            │ PRODUCTION   │                        │  ║
 * ║   │   │          │            │              │                        │  ║
 * ║   │   │ Suppress │            │ Neutralize   │                        │  ║
 * ║   │   │ Response │            │ at barrier   │                        │  ║
 * ║   │   └──────────┘            └──────┬───────┘                        │  ║
 * ║   │                                  │                                │  ║
 * ║   │                                  ▼                                │  ║
 * ║   │   ┌─────────────────────────────────────────────────┐             │  ║
 * ║   │   │          MUCUS LAYER (First Defense)            │             │  ║
 * ║   │   │  - Physical barrier                              │             │  ║
 * ║   │   │  - sIgA embedded in mucus                        │             │  ║
 * ║   │   │  - Prevents pathogen adherence                   │             │  ║
 * ║   │   └─────────────────────────────────────────────────┘             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               BARRIER INTEGRITY MONITORING                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   Integrity = f(sIgA_level, threat_neutralized, breach_attempts)   │  ║
 * ║   │                                                                     │  ║
 * ║   │   Low Integrity → ↑ Sampling Rate, ↓ Tolerance Threshold           │  ║
 * ║   │   High Integrity → ↓ Sampling Rate, ↑ Tolerance Threshold          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * KEY DIFFERENCES FROM SYSTEMIC IMMUNITY:
 * ---------------------------------------
 * - LOWER THRESHOLD at boundaries (early detection)
 * - TOLERANCE BIAS (prevent over-reaction to benign stimuli)
 * - IgA vs IgG (non-inflammatory neutralization)
 * - LOCAL CONTAINMENT (prevent systemic activation)
 * - CONTINUOUS SAMPLING (M cells actively sample environment)
 *
 * ARCHITECTURE:
 * - Mucosal sites registered at module boundaries
 * - M cells sample incoming data for antigenic content
 * - sIgA responses produced locally at barriers
 * - Oral tolerance prevents systemic immune activation
 * - Barrier integrity monitored and dynamically adjusted
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

#ifndef NIMCP_MUCOSAL_IMMUNITY_H
#define NIMCP_MUCOSAL_IMMUNITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MUCOSAL_MAX_SITES              64     /**< Max mucosal boundary sites */
#define MUCOSAL_MAX_SIGA_ANTIBODIES    256    /**< Max secretory IgA antibodies */
#define MUCOSAL_MAX_TOLERANCE_ENTRIES  128    /**< Max oral tolerance entries */
#define MUCOSAL_MAX_M_CELL_SAMPLES     512    /**< Max M cell samples */
#define MUCOSAL_EPITOPE_SIZE           64     /**< Antigen signature size */

/* Threshold scaling factors */
#define MUCOSAL_BOUNDARY_THRESHOLD_SCALE   0.5f   /**< Lower threshold at boundaries */
#define MUCOSAL_INTERNAL_THRESHOLD_SCALE   1.5f   /**< Higher threshold internally */

/* Barrier integrity thresholds */
#define MUCOSAL_INTEGRITY_CRITICAL     0.3f   /**< Critical barrier breach */
#define MUCOSAL_INTEGRITY_LOW          0.5f   /**< Low integrity */
#define MUCOSAL_INTEGRITY_NORMAL       0.7f   /**< Normal integrity */
#define MUCOSAL_INTEGRITY_EXCELLENT    0.9f   /**< Excellent integrity */

/* IgA production rates */
#define MUCOSAL_SIGA_PRODUCTION_RATE   1000   /**< sIgA per hour (arbitrary units) */
#define MUCOSAL_SIGA_HALF_LIFE_MS      3600000 /**< 1 hour half-life */

/* Tolerance induction */
#define MUCOSAL_TOLERANCE_EXPOSURES    3      /**< Exposures for tolerance */
#define MUCOSAL_TOLERANCE_WINDOW_MS    86400000 /**< 24 hour window */

/* M cell sampling */
#define MUCOSAL_M_CELL_SAMPLE_RATE     100    /**< Samples per second */
#define MUCOSAL_M_CELL_TRANSCYTOSIS_MS 50     /**< Transcytosis delay */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Mucosal site types
 *
 * BIOLOGICAL BASIS:
 * Different mucosal surfaces have specialized immune characteristics.
 * In NIMCP, module boundaries are analogous to mucosal barriers.
 */
typedef enum {
    MUCOSAL_SITE_INPUT_GATE = 0,    /**< Sensory input boundary (NALT-analog) */
    MUCOSAL_SITE_OUTPUT_GATE,       /**< Motor output boundary */
    MUCOSAL_SITE_MODULE_BOUNDARY,   /**< Inter-module boundary (Peyer's patch-analog) */
    MUCOSAL_SITE_COUNT
} mucosal_site_type_t;

/**
 * @brief Mucosal antibody state
 */
typedef enum {
    MUCOSAL_SIGA_INACTIVE = 0,      /**< Inactive sIgA */
    MUCOSAL_SIGA_ACTIVE,            /**< Active neutralization */
    MUCOSAL_SIGA_DECAYED            /**< Decayed below threshold */
} mucosal_siga_state_t;

/**
 * @brief Tolerance state
 */
typedef enum {
    TOLERANCE_NONE = 0,             /**< No tolerance */
    TOLERANCE_INDUCTION,            /**< Tolerance being induced */
    TOLERANCE_ESTABLISHED,          /**< Established tolerance (Treg) */
    TOLERANCE_BROKEN                /**< Tolerance broken (inflammation) */
} mucosal_tolerance_state_t;

/**
 * @brief M cell state
 */
typedef enum {
    M_CELL_SAMPLING = 0,            /**< Actively sampling */
    M_CELL_TRANSCYTOSING,           /**< Transcytosing antigen */
    M_CELL_PRESENTING,              /**< Presenting to immune cells */
    M_CELL_RESTING                  /**< Resting between samples */
} m_cell_state_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Mucosal boundary site
 *
 * Represents a MALT-analog region at a module boundary.
 * Site of antigen sampling, tolerance induction, and sIgA production.
 */
typedef struct {
    uint32_t id;                        /**< Site ID */
    mucosal_site_type_t site_type;      /**< Site type */
    uint32_t module_id;                 /**< Associated module */

    /* Barrier characteristics */
    float tolerance_threshold;          /**< Tolerance vs response threshold */
    float recognition_threshold;        /**< Antigen recognition threshold */
    float barrier_integrity;            /**< Barrier integrity (0-1) */

    /* Active immunity */
    uint32_t active_siga_count;         /**< Active sIgA antibodies */
    uint32_t tolerance_entry_count;     /**< Established tolerances */
    uint32_t breach_attempts;           /**< Failed breach attempts */

    /* M cell activity */
    uint32_t m_cell_count;              /**< M cells at site */
    uint32_t samples_per_sec;           /**< Sampling rate */
    uint64_t last_sample_time;          /**< Last M cell sample */

    /* Statistics */
    uint64_t antigens_sampled;          /**< Total antigens sampled */
    uint64_t tolerances_induced;        /**< Tolerances induced */
    uint64_t siga_produced;             /**< sIgA produced */
    uint64_t threats_neutralized;       /**< Threats neutralized at barrier */

    /* State */
    uint64_t creation_time;             /**< When site registered */
    bool active;                        /**< Site is active */
} mucosal_site_t;

/**
 * @brief Secretory IgA antibody
 *
 * Non-inflammatory antibody for mucosal neutralization.
 * Dimeric structure + secretory component.
 */
typedef struct {
    uint32_t id;                        /**< sIgA ID */
    uint32_t site_id;                   /**< Producing mucosal site */
    uint32_t target_antigen_id;         /**< Target antigen */

    /* Antibody structure (dimeric IgA) */
    uint8_t paratope[MUCOSAL_EPITOPE_SIZE]; /**< Antigen binding site */
    size_t paratope_len;                /**< Binding site length */
    bool has_secretory_component;       /**< Secretory component attached */

    /* Function */
    float neutralization_efficiency;    /**< Neutralization (0-1) */
    float affinity;                     /**< Antigen affinity (0-1) */
    mucosal_siga_state_t state;         /**< Current state */

    /* Lifecycle */
    uint64_t production_time;           /**< When produced */
    uint64_t half_life_ms;              /**< Half-life */
    uint32_t neutralizations;           /**< Successful neutralizations */

    /* Location */
    bool in_mucus_layer;                /**< Embedded in mucus */
} mucosal_siga_t;

/**
 * @brief Oral tolerance entry
 *
 * Represents established immunological tolerance to antigen.
 * Mediated by regulatory T cells (Treg).
 */
typedef struct {
    uint32_t id;                        /**< Tolerance ID */
    uint32_t site_id;                   /**< Inducing mucosal site */

    /* Antigen signature */
    uint8_t tolerized_epitope[MUCOSAL_EPITOPE_SIZE]; /**< Tolerized pattern */
    size_t epitope_len;                 /**< Pattern length */

    /* Tolerance state */
    mucosal_tolerance_state_t state;    /**< Current state */
    uint32_t exposure_count;            /**< Exposures during induction */
    uint64_t first_exposure_time;       /**< First exposure */
    uint64_t last_exposure_time;        /**< Last exposure */

    /* Regulatory activity */
    float suppression_strength;         /**< Treg suppression (0-1) */
    bool systemic;                      /**< Tolerance is systemic */

    /* Statistics */
    uint32_t suppressed_responses;      /**< Responses suppressed */
} mucosal_tolerance_t;

/**
 * @brief M cell antigen sample
 *
 * Represents M cell transcytosis of antigen from lumen to immune tissue.
 */
typedef struct {
    uint32_t id;                        /**< Sample ID */
    uint32_t site_id;                   /**< Sampling site */

    /* Sampled antigen */
    uint8_t sampled_data[MUCOSAL_EPITOPE_SIZE]; /**< Sampled data */
    size_t data_len;                    /**< Data length */
    uint32_t source_module_id;          /**< Source module */

    /* M cell processing */
    m_cell_state_t state;               /**< M cell state */
    uint64_t sample_time;               /**< When sampled */
    uint64_t transcytosis_start;        /**< Transcytosis start */

    /* Immune decision */
    bool presented_to_immune;           /**< Presented to immune system */
    bool tolerance_induced;             /**< Resulted in tolerance */
    bool response_triggered;            /**< Triggered immune response */
} m_cell_sample_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Mucosal immunity configuration
 */
typedef struct {
    /* Population limits */
    size_t max_sites;                   /**< Max mucosal sites */
    size_t max_siga_antibodies;         /**< Max sIgA antibodies */
    size_t max_tolerance_entries;       /**< Max tolerance entries */
    size_t max_m_cell_samples;          /**< Max M cell samples */

    /* Threshold tuning */
    float boundary_threshold_scale;     /**< Boundary vs internal threshold */
    float tolerance_bias;               /**< Bias toward tolerance (0-1) */

    /* Barrier integrity */
    float integrity_decay_rate;         /**< Integrity decay per breach */
    float integrity_recovery_rate;      /**< Recovery rate */

    /* IgA production */
    uint32_t siga_production_rate;      /**< sIgA per hour */
    uint64_t siga_half_life_ms;         /**< sIgA half-life */

    /* Tolerance induction */
    uint32_t tolerance_exposures;       /**< Exposures for tolerance */
    uint64_t tolerance_window_ms;       /**< Time window for exposures */

    /* M cell sampling */
    uint32_t m_cell_sample_rate;        /**< Samples per second */
    uint64_t m_cell_transcytosis_ms;    /**< Transcytosis delay */

    /* Integration */
    bool enable_tolerance;              /**< Enable oral tolerance */
    bool enable_siga_production;        /**< Enable sIgA production */
    bool enable_m_cell_sampling;        /**< Enable M cell sampling */
} mucosal_config_t;

/**
 * @brief Mucosal immunity statistics
 */
typedef struct {
    /* Site statistics */
    uint32_t active_sites;
    uint32_t active_siga;
    uint32_t established_tolerances;

    /* Activity */
    uint64_t total_samples;
    uint64_t tolerances_induced;
    uint64_t siga_produced;
    uint64_t threats_neutralized_at_barrier;

    /* Barrier health */
    float avg_barrier_integrity;
    uint32_t breaches_prevented;
    uint32_t breaches_occurred;
} mucosal_stats_t;

/* ============================================================================
 * Main System Structure
 * ============================================================================ */

/**
 * @brief Mucosal immunity system
 */
typedef struct {
    mucosal_config_t config;            /**< Configuration */

    /* Mucosal sites */
    mucosal_site_t* sites;
    size_t site_count;
    size_t site_capacity;
    uint32_t next_site_id;

    /* Secretory IgA antibodies */
    mucosal_siga_t* siga_antibodies;
    size_t siga_count;
    size_t siga_capacity;
    uint32_t next_siga_id;

    /* Oral tolerance entries */
    mucosal_tolerance_t* tolerances;
    size_t tolerance_count;
    size_t tolerance_capacity;
    uint32_t next_tolerance_id;

    /* M cell samples */
    m_cell_sample_t* m_cell_samples;
    size_t m_cell_sample_count;
    size_t m_cell_sample_capacity;
    uint32_t next_m_cell_sample_id;

    /* Integration */
    brain_immune_system_t* immune_system; /**< Brain immune system */

    /* Statistics */
    mucosal_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< Mutex for thread safety */

    /* State */
    bool running;                       /**< System is active */
    uint64_t start_time;                /**< System start time */
} mucosal_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default mucosal immunity configuration
 * WHY:  Provide sensible defaults for easy initialization
 * HOW:  Fill struct with biologically-inspired default values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int mucosal_default_config(mucosal_config_t* config);

/**
 * WHAT: Create mucosal immunity system
 * WHY:  Initialize boundary immune surveillance
 * HOW:  Allocate pools, register with brain immune system
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system to integrate with
 * @return New mucosal system or NULL on failure
 */
mucosal_system_t* mucosal_create(
    const mucosal_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * WHAT: Destroy mucosal immunity system
 * WHY:  Proper resource cleanup
 * HOW:  Free all pools and unregister from brain immune
 *
 * @param system System to destroy
 */
void mucosal_destroy(mucosal_system_t* system);

/**
 * WHAT: Start mucosal immunity monitoring
 * WHY:  Begin boundary surveillance and M cell sampling
 * HOW:  Activate all registered sites
 *
 * @param system Mucosal system
 * @return 0 on success
 */
int mucosal_start(mucosal_system_t* system);

/**
 * WHAT: Stop mucosal immunity system
 * WHY:  Graceful shutdown
 * HOW:  Deactivate sites, complete pending samples
 *
 * @param system Mucosal system
 * @return 0 on success
 */
int mucosal_stop(mucosal_system_t* system);

/* ============================================================================
 * Site Registration API
 * ============================================================================ */

/**
 * WHAT: Register mucosal boundary site
 * WHY:  Establish immune surveillance at module boundary
 * HOW:  Create site with M cells and lower detection threshold
 *
 * @param system Mucosal system
 * @param site_type Type of mucosal site
 * @param module_id Associated module ID
 * @param site_id Output: assigned site ID
 * @return 0 on success
 */
int mucosal_register_boundary(
    mucosal_system_t* system,
    mucosal_site_type_t site_type,
    uint32_t module_id,
    uint32_t* site_id
);

/**
 * WHAT: Unregister mucosal boundary site
 * WHY:  Remove boundary when module disconnects
 * HOW:  Deactivate site, clear associated sIgA/tolerances
 *
 * @param system Mucosal system
 * @param site_id Site to unregister
 * @return 0 on success
 */
int mucosal_unregister_boundary(
    mucosal_system_t* system,
    uint32_t site_id
);

/* ============================================================================
 * M Cell Sampling API
 * ============================================================================ */

/**
 * WHAT: M cell samples antigen from boundary
 * WHY:  Active immune surveillance at mucosal surfaces
 * HOW:  Transcytose data to underlying immune tissue for processing
 *
 * @param system Mucosal system
 * @param site_id Sampling site
 * @param data Data to sample
 * @param data_len Data length
 * @param sample_id Output: M cell sample ID
 * @return 0 on success
 */
int mucosal_sample_antigen(
    mucosal_system_t* system,
    uint32_t site_id,
    const uint8_t* data,
    size_t data_len,
    uint32_t* sample_id
);

/**
 * WHAT: Process M cell sample
 * WHY:  Decide tolerance vs immune response
 * HOW:  Check for known tolerance, assess pathogenicity, induce tolerance or trigger response
 *
 * @param system Mucosal system
 * @param sample_id Sample to process
 * @return 0 on success
 */
int mucosal_process_m_cell_sample(
    mucosal_system_t* system,
    uint32_t sample_id
);

/* ============================================================================
 * Secretory IgA API
 * ============================================================================ */

/**
 * WHAT: Produce secretory IgA antibody
 * WHY:  Non-inflammatory neutralization at mucosal barrier
 * HOW:  Create dimeric IgA + secretory component for antigen
 *
 * @param system Mucosal system
 * @param site_id Producing site
 * @param antigen_id Target antigen (from brain immune system)
 * @param siga_id Output: new sIgA ID
 * @return 0 on success
 */
int mucosal_produce_siga(
    mucosal_system_t* system,
    uint32_t site_id,
    uint32_t antigen_id,
    uint32_t* siga_id
);

/**
 * WHAT: Neutralize antigen with sIgA
 * WHY:  Prevent pathogen adherence/entry at barrier
 * HOW:  Bind antigen, immune exclusion, prevent systemic activation
 *
 * @param system Mucosal system
 * @param siga_id sIgA antibody
 * @param antigen_id Target antigen
 * @return 0 on success
 */
int mucosal_neutralize_with_siga(
    mucosal_system_t* system,
    uint32_t siga_id,
    uint32_t antigen_id
);

/* ============================================================================
 * Oral Tolerance API
 * ============================================================================ */

/**
 * WHAT: Induce oral tolerance to antigen
 * WHY:  Prevent harmful inflammation to benign stimuli
 * HOW:  Repeated exposure → Treg induction → suppression of systemic response
 *
 * @param system Mucosal system
 * @param site_id Inducing site
 * @param antigen Antigen signature
 * @param antigen_len Signature length
 * @param tolerance_id Output: new tolerance ID
 * @return 0 on success
 */
int mucosal_induce_oral_tolerance(
    mucosal_system_t* system,
    uint32_t site_id,
    const uint8_t* antigen,
    size_t antigen_len,
    uint32_t* tolerance_id
);

/**
 * WHAT: Check if antigen is tolerized
 * WHY:  Determine if immune response should be suppressed
 * HOW:  Search tolerance entries for matching epitope
 *
 * @param system Mucosal system
 * @param antigen Antigen signature
 * @param antigen_len Signature length
 * @param tolerance_id Output: matching tolerance (if found)
 * @return 0 if tolerized, -1 if not
 */
int mucosal_check_tolerance(
    mucosal_system_t* system,
    const uint8_t* antigen,
    size_t antigen_len,
    uint32_t* tolerance_id
);

/**
 * WHAT: Break established tolerance
 * WHY:  Strong pathogenic signal overrides tolerance
 * HOW:  Update tolerance state, allow immune response
 *
 * @param system Mucosal system
 * @param tolerance_id Tolerance to break
 * @return 0 on success
 */
int mucosal_break_tolerance(
    mucosal_system_t* system,
    uint32_t tolerance_id
);

/* ============================================================================
 * Barrier Integrity API
 * ============================================================================ */

/**
 * WHAT: Get barrier integrity at site
 * WHY:  Monitor barrier health
 * HOW:  Return integrity score (0-1)
 *
 * @param system Mucosal system
 * @param site_id Site to query
 * @param integrity_out Output: integrity (0-1)
 * @return 0 on success
 */
int mucosal_get_barrier_integrity(
    mucosal_system_t* system,
    uint32_t site_id,
    float* integrity_out
);

/**
 * WHAT: Set tolerance threshold at site
 * WHY:  Dynamic adjustment of tolerance vs response decision
 * HOW:  Update site threshold (lower = more tolerant)
 *
 * @param system Mucosal system
 * @param site_id Site to modify
 * @param threshold New tolerance threshold (0-1)
 * @return 0 on success
 */
int mucosal_set_tolerance_threshold(
    mucosal_system_t* system,
    uint32_t site_id,
    float threshold
);

/**
 * WHAT: Update barrier integrity
 * WHY:  Track barrier damage and recovery
 * HOW:  Decay on breaches, recover on successful neutralizations
 *
 * @param system Mucosal system
 * @param site_id Site to update
 * @param breach_occurred True if breach occurred
 * @return 0 on success
 */
int mucosal_update_barrier_integrity(
    mucosal_system_t* system,
    uint32_t site_id,
    bool breach_occurred
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * WHAT: Update mucosal immunity state
 * WHY:  Process M cell samples, decay sIgA, update integrity
 * HOW:  Advance state machines for all components
 *
 * @param system Mucosal system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int mucosal_update(
    mucosal_system_t* system,
    uint64_t delta_ms
);

/**
 * WHAT: Get mucosal immunity statistics
 * WHY:  Monitor barrier immune activity
 * HOW:  Return aggregate statistics
 *
 * @param system Mucosal system
 * @param stats Output statistics
 * @return 0 on success
 */
int mucosal_get_stats(
    mucosal_system_t* system,
    mucosal_stats_t* stats
);

/**
 * WHAT: Get mucosal site by ID
 * WHY:  Query site state
 * HOW:  Return site pointer
 *
 * @param system Mucosal system
 * @param site_id Site ID
 * @return Site or NULL if not found
 */
const mucosal_site_t* mucosal_get_site(
    mucosal_system_t* system,
    uint32_t site_id
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* mucosal_site_type_to_string(mucosal_site_type_t type);
const char* mucosal_siga_state_to_string(mucosal_siga_state_t state);
const char* mucosal_tolerance_state_to_string(mucosal_tolerance_state_t state);
const char* mucosal_m_cell_state_to_string(m_cell_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MUCOSAL_IMMUNITY_H */
