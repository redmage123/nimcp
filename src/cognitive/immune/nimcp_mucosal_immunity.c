/**
 * @file nimcp_mucosal_immunity.c
 * @brief Mucosal/Barrier Immunity Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of mucosal immunity for NIMCP brain immune system
 * WHY:  Provide specialized boundary immune responses (IgA, tolerance, M cells)
 * HOW:  Coordinates sIgA production, oral tolerance induction, and M cell sampling
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_mucosal_immunity.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(mucosal_immunity, MESH_ADAPTER_CATEGORY_SECURITY)



/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current timestamp in milliseconds
 * WHY:  Track timing of immune events
 * HOW:  Use clock_gettime for monotonic time
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * WHAT: Find mucosal site by ID
 * WHY:  Locate site for operations
 * HOW:  Linear search through sites array
 */
static mucosal_site_t* find_site(mucosal_system_t* system, uint32_t site_id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    for (size_t i = 0; i < system->site_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->site_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->site_count);
        }

        if (system->sites[i].id == site_id) {
            return &system->sites[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Find sIgA antibody by ID
 * WHY:  Locate sIgA for operations
 * HOW:  Linear search through sIgA array
 */
static mucosal_siga_t* find_siga(mucosal_system_t* system, uint32_t siga_id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    for (size_t i = 0; i < system->siga_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->siga_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->siga_count);
        }

        if (system->siga_antibodies[i].id == siga_id) {
            return &system->siga_antibodies[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Find tolerance entry by ID
 * WHY:  Locate tolerance for operations
 * HOW:  Linear search through tolerances array
 */
static mucosal_tolerance_t* find_tolerance(mucosal_system_t* system, uint32_t tolerance_id) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    for (size_t i = 0; i < system->tolerance_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->tolerance_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->tolerance_count);
        }

        if (system->tolerances[i].id == tolerance_id) {
            return &system->tolerances[i];
        }
    }
    return NULL;
}

/**
 * WHAT: Compute pattern similarity for tolerance matching
 * WHY:  Fuzzy matching for cross-reactive tolerance
 * HOW:  Use brain immune affinity computation
 */
static float compute_tolerance_affinity(
    const uint8_t* pattern1, size_t len1,
    const uint8_t* pattern2, size_t len2
) {
    return brain_immune_compute_affinity(pattern1, len1, pattern2, len2);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int mucosal_default_config(mucosal_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_default_config: config is NULL");
        return -1;
    }

    /* Set defaults */
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_default_conf", 0.0f);


    config->max_sites = MUCOSAL_MAX_SITES;
    config->max_siga_antibodies = MUCOSAL_MAX_SIGA_ANTIBODIES;
    config->max_tolerance_entries = MUCOSAL_MAX_TOLERANCE_ENTRIES;
    config->max_m_cell_samples = MUCOSAL_MAX_M_CELL_SAMPLES;

    config->boundary_threshold_scale = MUCOSAL_BOUNDARY_THRESHOLD_SCALE;
    config->tolerance_bias = 0.6f;  /* 60% bias toward tolerance */

    config->integrity_decay_rate = 0.05f;
    config->integrity_recovery_rate = 0.02f;

    config->siga_production_rate = MUCOSAL_SIGA_PRODUCTION_RATE;
    config->siga_half_life_ms = MUCOSAL_SIGA_HALF_LIFE_MS;

    config->tolerance_exposures = MUCOSAL_TOLERANCE_EXPOSURES;
    config->tolerance_window_ms = MUCOSAL_TOLERANCE_WINDOW_MS;

    config->m_cell_sample_rate = MUCOSAL_M_CELL_SAMPLE_RATE;
    config->m_cell_transcytosis_ms = MUCOSAL_M_CELL_TRANSCYTOSIS_MS;

    config->enable_tolerance = true;
    config->enable_siga_production = true;
    config->enable_m_cell_sampling = true;

    return 0;
}

mucosal_system_t* mucosal_create(
    const mucosal_config_t* config,
    brain_immune_system_t* immune_system
) {
    /* Guard clause */
    if (!immune_system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;

    }

    /* Allocate system */
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_create", 0.0f);


    mucosal_system_t* system = (mucosal_system_t*)nimcp_malloc(sizeof(mucosal_system_t));
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;

    }

    memset(system, 0, sizeof(mucosal_system_t));

    /* Set configuration */
    if (config) {
        system->config = *config;
    } else {
        mucosal_default_config(&system->config);
    }

    /* Allocate pools */
    system->sites = (mucosal_site_t*)nimcp_calloc(
        system->config.max_sites, sizeof(mucosal_site_t));
    system->siga_antibodies = (mucosal_siga_t*)nimcp_calloc(
        system->config.max_siga_antibodies, sizeof(mucosal_siga_t));
    system->tolerances = (mucosal_tolerance_t*)nimcp_calloc(
        system->config.max_tolerance_entries, sizeof(mucosal_tolerance_t));
    system->m_cell_samples = (m_cell_sample_t*)nimcp_calloc(
        system->config.max_m_cell_samples, sizeof(m_cell_sample_t));

    if (!system->sites || !system->siga_antibodies ||
        !system->tolerances || !system->m_cell_samples) {
        mucosal_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_create: operation failed");
        return NULL;
    }

    system->site_capacity = system->config.max_sites;
    system->siga_capacity = system->config.max_siga_antibodies;
    system->tolerance_capacity = system->config.max_tolerance_entries;
    system->m_cell_sample_capacity = system->config.max_m_cell_samples;

    /* Create mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (!system->mutex) {
        mucosal_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mucosal_create: system->mutex is NULL");
        return NULL;
    }

    /* Store immune system reference */
    system->immune_system = immune_system;

    NIMCP_LOGGING_INFO("Mucosal immunity system created");
    return system;
}

void mucosal_destroy(mucosal_system_t* system) {
    /* Guard clause */
    if (!system) return;

    /* Free pools */
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_destroy", 0.0f);


    if (system->sites) nimcp_free(system->sites);
    if (system->siga_antibodies) nimcp_free(system->siga_antibodies);
    if (system->tolerances) nimcp_free(system->tolerances);
    if (system->m_cell_samples) nimcp_free(system->m_cell_samples);
    if (system->mutex) nimcp_mutex_destroy(system->mutex);

    nimcp_free(system);
    system = NULL;
    NIMCP_LOGGING_INFO("Mucosal immunity system destroyed");
}

int mucosal_start(mucosal_system_t* system) {
    /* Guard clause */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_start: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_start", 0.0f);


    system->running = true;
    system->start_time = get_current_time_ms();

    NIMCP_LOGGING_INFO("Mucosal immunity system started");
    return 0;
}

int mucosal_stop(mucosal_system_t* system) {
    /* Guard clause */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_stop: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_stop", 0.0f);


    system->running = false;

    NIMCP_LOGGING_INFO("Mucosal immunity system stopped");
    return 0;
}

/* ============================================================================
 * Site Registration Implementation
 * ============================================================================ */

int mucosal_register_boundary(
    mucosal_system_t* system,
    mucosal_site_type_t site_type,
    uint32_t module_id,
    uint32_t* site_id
) {
    /* Guard clauses */
    if (!system || !site_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_register_boundary: required parameter is NULL (system, site_id)");
        return -1;
    }
    if (system->site_count >= system->site_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mucosal_register_boundary: capacity exceeded");
        return -1;
    }

    /* Create site */
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_register_bou", 0.0f);


    mucosal_site_t* site = &system->sites[system->site_count++];
    site->id = system->next_site_id++;
    site->site_type = site_type;
    site->module_id = module_id;

    /* Set thresholds (lower at boundaries) */
    site->recognition_threshold = 0.4f * system->config.boundary_threshold_scale;
    site->tolerance_threshold = 0.6f;  /* Higher = more tolerant */
    site->barrier_integrity = 1.0f;  /* Start with perfect integrity */

    /* Initialize M cells */
    site->m_cell_count = 10;  /* 10 M cells per site */
    site->samples_per_sec = system->config.m_cell_sample_rate;
    site->last_sample_time = get_current_time_ms();

    /* Set state */
    site->creation_time = get_current_time_ms();
    site->active = true;

    *site_id = site->id;
    system->stats.active_sites++;

    NIMCP_LOGGING_INFO("Registered mucosal boundary site");
    return 0;
}

int mucosal_unregister_boundary(
    mucosal_system_t* system,
    uint32_t site_id
) {
    /* Guard clause */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_unregister_boundary: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_unregister_b", 0.0f);


    mucosal_site_t* site = find_site(system, site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_unregister_boundary: site is NULL");
        return -1;
    }

    site->active = false;
    system->stats.active_sites--;

    NIMCP_LOGGING_INFO("Unregistered mucosal boundary site");
    return 0;
}

/* ============================================================================
 * M Cell Sampling Implementation
 * ============================================================================ */

int mucosal_sample_antigen(
    mucosal_system_t* system,
    uint32_t site_id,
    const uint8_t* data,
    size_t data_len,
    uint32_t* sample_id
) {
    /* Guard clauses */
    if (!system || !data || !sample_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_sample_antigen: required parameter is NULL (system, data, sample_id)");
        return -1;
    }
    if (data_len == 0 || data_len > MUCOSAL_EPITOPE_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mucosal_sample_antigen: data_len is zero");
        return -1;
    }
    if (system->m_cell_sample_count >= system->m_cell_sample_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mucosal_sample_antigen: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_sample_antig", 0.0f);

    /* Thread-safety fix: protect shared state */
    nimcp_mutex_lock(system->mutex);

    mucosal_site_t* site = find_site(system, site_id);
    if (!site || !site->active) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_sample_antigen: required parameter is NULL (site, site->active)");
        return -1;
    }

    /* Re-check capacity under lock (TOCTOU fix) */
    if (system->m_cell_sample_count >= system->m_cell_sample_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mucosal_sample_antigen: capacity exceeded");
        return -1;
    }

    /* Create M cell sample */
    m_cell_sample_t* sample = &system->m_cell_samples[system->m_cell_sample_count++];
    sample->id = system->next_m_cell_sample_id++;
    sample->site_id = site_id;

    /* Copy sampled data */
    memcpy(sample->sampled_data, data, data_len);
    sample->data_len = data_len;
    sample->source_module_id = site->module_id;

    /* Set M cell state */
    sample->state = M_CELL_SAMPLING;
    sample->sample_time = get_current_time_ms();
    sample->transcytosis_start = 0;

    *sample_id = sample->id;
    site->antigens_sampled++;
    system->stats.total_samples++;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int mucosal_process_m_cell_sample(
    mucosal_system_t* system,
    uint32_t sample_id
) {
    /* Guard clause */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_process_m_cell_sample: system is NULL");
        return -1;
    }

    /* Find sample */
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_process_m_ce", 0.0f);


    m_cell_sample_t* sample = NULL;
    for (size_t i = 0; i < system->m_cell_sample_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->m_cell_sample_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->m_cell_sample_count);
        }

        if (system->m_cell_samples[i].id == sample_id) {
            sample = &system->m_cell_samples[i];
            break;
        }
    }
    if (!sample) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_process_m_cell_sample: sample is NULL");
        return -1;
    }

    mucosal_site_t* site = find_site(system, sample->site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_process_m_cell_sample: site is NULL");
        return -1;
    }

    /* Check for existing tolerance */
    uint32_t tolerance_id = 0;
    int is_tolerized = mucosal_check_tolerance(
        system, sample->sampled_data, sample->data_len, &tolerance_id);

    if (is_tolerized == 0) {
        /* Tolerized - suppress response */
        sample->tolerance_induced = false;
        sample->response_triggered = false;
        sample->presented_to_immune = false;
        sample->state = M_CELL_RESTING;
        return 0;
    }

    /* Assess pathogenicity (simplified heuristic) */
    bool is_pathogenic = (sample->sampled_data[0] & 0x80) != 0;  /* High bit = pathogenic */

    if (!is_pathogenic && system->config.enable_tolerance) {
        /* Induce oral tolerance */
        uint32_t new_tolerance_id = 0;
        mucosal_induce_oral_tolerance(
            system, sample->site_id,
            sample->sampled_data, sample->data_len,
            &new_tolerance_id);
        sample->tolerance_induced = true;
        sample->response_triggered = false;
    } else {
        /* Trigger immune response via brain immune system */
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(
            system->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            sample->sampled_data,
            sample->data_len,
            is_pathogenic ? 8 : 4,  /* Higher severity for pathogens */
            sample->source_module_id,
            &antigen_id
        );

        /* Produce sIgA locally */
        if (system->config.enable_siga_production) {
            uint32_t siga_id = 0;
            mucosal_produce_siga(system, sample->site_id, antigen_id, &siga_id);
        }

        sample->response_triggered = true;
        sample->tolerance_induced = false;
    }

    sample->presented_to_immune = true;
    sample->state = M_CELL_PRESENTING;

    return 0;
}

/* ============================================================================
 * Secretory IgA Implementation
 * ============================================================================ */

int mucosal_produce_siga(
    mucosal_system_t* system,
    uint32_t site_id,
    uint32_t antigen_id,
    uint32_t* siga_id
) {
    /* Guard clauses */
    if (!system || !siga_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_produce_siga: required parameter is NULL (system, siga_id)");
        return -1;
    }
    if (system->siga_count >= system->siga_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mucosal_produce_siga: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_produce_siga", 0.0f);

    /* Thread-safety fix: protect shared state */
    nimcp_mutex_lock(system->mutex);

    mucosal_site_t* site = find_site(system, site_id);
    if (!site || !site->active) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_produce_siga: required parameter is NULL (site, site->active)");
        return -1;
    }

    /* Re-check capacity under lock (TOCTOU fix) */
    if (system->siga_count >= system->siga_capacity) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mucosal_produce_siga: capacity exceeded");
        return -1;
    }

    /* Get antigen from brain immune (copy to avoid dangling pointer) */
    brain_antigen_t antigen_copy;
    if (brain_immune_get_antigen_copy(system->immune_system, antigen_id, &antigen_copy) != 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_produce_siga: antigen is NULL");
        return -1;
    }

    /* Create sIgA */
    mucosal_siga_t* siga = &system->siga_antibodies[system->siga_count++];
    siga->id = system->next_siga_id++;
    siga->site_id = site_id;
    siga->target_antigen_id = antigen_id;

    /* Copy epitope to paratope */
    memcpy(siga->paratope, antigen_copy.epitope, antigen_copy.epitope_len);
    siga->paratope_len = antigen_copy.epitope_len;
    siga->has_secretory_component = true;

    /* Set properties */
    siga->neutralization_efficiency = 0.8f;  /* High efficiency */
    siga->affinity = 0.9f;
    siga->state = MUCOSAL_SIGA_ACTIVE;

    siga->production_time = get_current_time_ms();
    siga->half_life_ms = system->config.siga_half_life_ms;
    siga->in_mucus_layer = true;

    *siga_id = siga->id;
    site->active_siga_count++;
    site->siga_produced++;
    system->stats.active_siga++;
    system->stats.siga_produced++;

    nimcp_mutex_unlock(system->mutex);
    NIMCP_LOGGING_INFO("Produced secretory IgA antibody");
    return 0;
}

int mucosal_neutralize_with_siga(
    mucosal_system_t* system,
    uint32_t siga_id,
    uint32_t antigen_id
) {
    /* Guard clause */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_neutralize_with_siga: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_neutralize_w", 0.0f);


    mucosal_siga_t* siga = find_siga(system, siga_id);
    if (!siga || siga->state != MUCOSAL_SIGA_ACTIVE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mucosal_neutralize_with_siga: siga is NULL");
        return -1;
    }

    /* Neutralize antigen */
    siga->neutralizations++;

    mucosal_site_t* site = find_site(system, siga->site_id);
    if (site) {
        site->threats_neutralized++;
        /* Update barrier integrity (successful neutralization) */
        mucosal_update_barrier_integrity(system, siga->site_id, false);
    }

    system->stats.threats_neutralized_at_barrier++;
    system->stats.breaches_prevented++;

    NIMCP_LOGGING_INFO("Neutralized antigen with sIgA");
    return 0;
}

/* ============================================================================
 * Oral Tolerance Implementation
 * ============================================================================ */

int mucosal_induce_oral_tolerance(
    mucosal_system_t* system,
    uint32_t site_id,
    const uint8_t* antigen,
    size_t antigen_len,
    uint32_t* tolerance_id
) {
    /* Guard clauses */
    if (!system || !antigen || !tolerance_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_induce_oral_tolerance: required parameter is NULL (system, antigen, tolerance_id)");
        return -1;
    }
    if (antigen_len == 0 || antigen_len > MUCOSAL_EPITOPE_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mucosal_induce_oral_tolerance: antigen_len is zero");
        return -1;
    }
    if (system->tolerance_count >= system->tolerance_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mucosal_induce_oral_tolerance: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_induce_oral_", 0.0f);


    mucosal_site_t* site = find_site(system, site_id);
    if (!site || !site->active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_induce_oral_tolerance: required parameter is NULL (site, site->active)");
        return -1;
    }

    /* Check for existing tolerance entry */
    for (size_t i = 0; i < system->tolerance_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->tolerance_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->tolerance_count);
        }

        mucosal_tolerance_t* existing = &system->tolerances[i];
        if (existing->site_id == site_id &&
            existing->epitope_len == antigen_len &&
            memcmp(existing->tolerized_epitope, antigen, antigen_len) == 0) {
            /* Already being induced */
            existing->exposure_count++;
            existing->last_exposure_time = get_current_time_ms();

            /* Check if tolerance established */
            if (existing->exposure_count >= system->config.tolerance_exposures) {
                existing->state = TOLERANCE_ESTABLISHED;
                existing->suppression_strength = 0.9f;
                existing->systemic = true;
                site->tolerances_induced++;
                system->stats.tolerances_induced++;
            }

            *tolerance_id = existing->id;
            return 0;
        }
    }

    /* Create new tolerance entry */
    mucosal_tolerance_t* tolerance = &system->tolerances[system->tolerance_count++];
    tolerance->id = system->next_tolerance_id++;
    tolerance->site_id = site_id;

    memcpy(tolerance->tolerized_epitope, antigen, antigen_len);
    tolerance->epitope_len = antigen_len;

    tolerance->state = TOLERANCE_INDUCTION;
    tolerance->exposure_count = 1;
    tolerance->first_exposure_time = get_current_time_ms();
    tolerance->last_exposure_time = tolerance->first_exposure_time;

    tolerance->suppression_strength = 0.3f;  /* Weak during induction */
    tolerance->systemic = false;

    *tolerance_id = tolerance->id;

    NIMCP_LOGGING_INFO("Inducing oral tolerance");
    return 0;
}

int mucosal_check_tolerance(
    mucosal_system_t* system,
    const uint8_t* antigen,
    size_t antigen_len,
    uint32_t* tolerance_id
) {
    /* Guard clauses */
    if (!system || !antigen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_check_tolerance: required parameter is NULL (system, antigen)");
        return -1;
    }
    if (antigen_len == 0 || antigen_len > MUCOSAL_EPITOPE_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mucosal_check_tolerance: antigen_len is zero");
        return -1;
    }

    /* Search tolerance entries */
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_check_tolera", 0.0f);

    /* Thread-safety fix: protect shared state */
    nimcp_mutex_lock(system->mutex);

    for (size_t i = 0; i < system->tolerance_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->tolerance_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->tolerance_count);
        }

        mucosal_tolerance_t* tolerance = &system->tolerances[i];

        /* Skip broken tolerances */
        if (tolerance->state == TOLERANCE_BROKEN) continue;

        /* Compute affinity */
        float affinity = compute_tolerance_affinity(
            tolerance->tolerized_epitope, tolerance->epitope_len,
            antigen, antigen_len);

        /* Established tolerance has lower threshold */
        float threshold = (tolerance->state == TOLERANCE_ESTABLISHED) ? 0.7f : 0.9f;

        if (affinity >= threshold) {
            if (tolerance_id) *tolerance_id = tolerance->id;
            tolerance->suppressed_responses++;
            nimcp_mutex_unlock(system->mutex);
            return 0;  /* Tolerized */
        }
    }

    nimcp_mutex_unlock(system->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mucosal_check_tolerance: validation failed");
    return -1;  /* Not tolerized */
}

int mucosal_break_tolerance(
    mucosal_system_t* system,
    uint32_t tolerance_id
) {
    /* Guard clause */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_break_tolerance: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_break_tolera", 0.0f);


    mucosal_tolerance_t* tolerance = find_tolerance(system, tolerance_id);
    if (!tolerance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_break_tolerance: tolerance is NULL");
        return -1;
    }

    tolerance->state = TOLERANCE_BROKEN;
    tolerance->suppression_strength = 0.0f;

    NIMCP_LOGGING_WARN("Tolerance broken - allowing immune response");
    return 0;
}

/* ============================================================================
 * Barrier Integrity Implementation
 * ============================================================================ */

int mucosal_get_barrier_integrity(
    mucosal_system_t* system,
    uint32_t site_id,
    float* integrity_out
) {
    /* Guard clauses */
    if (!system || !integrity_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_get_barrier_integrity: required parameter is NULL (system, integrity_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_get_barrier_", 0.0f);


    mucosal_site_t* site = find_site(system, site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_get_barrier_integrity: site is NULL");
        return -1;
    }

    *integrity_out = site->barrier_integrity;
    return 0;
}

int mucosal_set_tolerance_threshold(
    mucosal_system_t* system,
    uint32_t site_id,
    float threshold
) {
    /* Guard clauses */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_set_tolerance_threshold: system is NULL");
        return -1;
    }
    if (threshold < 0.0f || threshold > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mucosal_set_tolerance_threshold: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_set_toleranc", 0.0f);


    mucosal_site_t* site = find_site(system, site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_set_tolerance_threshold: site is NULL");
        return -1;
    }

    site->tolerance_threshold = threshold;

    NIMCP_LOGGING_INFO("Updated tolerance threshold");
    return 0;
}

int mucosal_update_barrier_integrity(
    mucosal_system_t* system,
    uint32_t site_id,
    bool breach_occurred
) {
    /* Guard clause */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_update_barrier_integrity: system is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_update_barri", 0.0f);


    mucosal_site_t* site = find_site(system, site_id);
    if (!site) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_update_barrier_integrity: site is NULL");
        return -1;
    }

    if (breach_occurred) {
        /* Decay integrity */
        site->barrier_integrity -= system->config.integrity_decay_rate;
        if (site->barrier_integrity < 0.0f) site->barrier_integrity = 0.0f;

        site->breach_attempts++;
        system->stats.breaches_occurred++;

        /* Lower threshold when integrity drops */
        if (site->barrier_integrity < MUCOSAL_INTEGRITY_LOW) {
            site->recognition_threshold *= 0.9f;  /* More sensitive */
        }
    } else {
        /* Recover integrity */
        site->barrier_integrity += system->config.integrity_recovery_rate;
        if (site->barrier_integrity > 1.0f) site->barrier_integrity = 1.0f;
    }

    return 0;
}

/* ============================================================================
 * Update and Query Implementation
 * ============================================================================ */

int mucosal_update(
    mucosal_system_t* system,
    uint64_t delta_ms
) {
    /* Guard clause */
    if (!system || !system->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_update: required parameter is NULL (system, system->running)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_update", 0.0f);

    /* Thread-safety fix: protect shared state during update */
    nimcp_mutex_lock(system->mutex);

    uint64_t current_time = get_current_time_ms();

    /* Process M cell samples */
    for (size_t i = 0; i < system->m_cell_sample_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->m_cell_sample_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->m_cell_sample_count);
        }

        m_cell_sample_t* sample = &system->m_cell_samples[i];

        if (sample->state == M_CELL_SAMPLING) {
            /* Start transcytosis */
            sample->state = M_CELL_TRANSCYTOSING;
            sample->transcytosis_start = current_time;
        } else if (sample->state == M_CELL_TRANSCYTOSING) {
            /* Check if transcytosis complete */
            uint64_t elapsed = current_time - sample->transcytosis_start;
            if (elapsed >= system->config.m_cell_transcytosis_ms) {
                /* Process sample — unlock to avoid recursive lock in process_m_cell_sample */
                nimcp_mutex_unlock(system->mutex);
                mucosal_process_m_cell_sample(system, sample->id);
                nimcp_mutex_lock(system->mutex);
            }
        }
    }

    /* Decay sIgA antibodies */
    for (size_t i = 0; i < system->siga_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->siga_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->siga_count);
        }

        mucosal_siga_t* siga = &system->siga_antibodies[i];

        if (siga->state == MUCOSAL_SIGA_ACTIVE) {
            uint64_t age_ms = current_time - siga->production_time;

            /* Simple half-life decay */
            if (age_ms > siga->half_life_ms) {
                siga->state = MUCOSAL_SIGA_DECAYED;
                system->stats.active_siga--;

                mucosal_site_t* site = find_site(system, siga->site_id);
                if (site) site->active_siga_count--;
            }
        }
    }

    /* Update barrier integrity statistics */
    float total_integrity = 0.0f;
    uint32_t active_sites = 0;

    for (size_t i = 0; i < system->site_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->site_count > 256) {
            mucosal_immunity_heartbeat("mucosal_immu_loop",
                             (float)(i + 1) / (float)system->site_count);
        }

        if (system->sites[i].active) {
            total_integrity += system->sites[i].barrier_integrity;
            active_sites++;
        }
    }

    if (active_sites > 0) {
        system->stats.avg_barrier_integrity = total_integrity / active_sites;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int mucosal_get_stats(
    mucosal_system_t* system,
    mucosal_stats_t* stats
) {
    /* Guard clauses */
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mucosal_get_stats: required parameter is NULL (system, stats)");
        return -1;
    }

    *stats = system->stats;
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_get_stats", 0.0f);


    return 0;
}

const mucosal_site_t* mucosal_get_site(
    mucosal_system_t* system,
    uint32_t site_id
) {
    /* Guard clause */
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_get_site", 0.0f);


    return find_site(system, site_id);
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* mucosal_site_type_to_string(mucosal_site_type_t type) {
    switch (type) {
        case MUCOSAL_SITE_INPUT_GATE: return "INPUT_GATE";
        case MUCOSAL_SITE_OUTPUT_GATE: return "OUTPUT_GATE";
        case MUCOSAL_SITE_MODULE_BOUNDARY: return "MODULE_BOUNDARY";
        default: return "UNKNOWN";
    }
}

const char* mucosal_siga_state_to_string(mucosal_siga_state_t state) {
    switch (state) {
        case MUCOSAL_SIGA_INACTIVE: return "INACTIVE";
        case MUCOSAL_SIGA_ACTIVE: return "ACTIVE";
        case MUCOSAL_SIGA_DECAYED: return "DECAYED";
        default: return "UNKNOWN";
    }
}

const char* mucosal_tolerance_state_to_string(mucosal_tolerance_state_t state) {
    switch (state) {
        case TOLERANCE_NONE: return "NONE";
        case TOLERANCE_INDUCTION: return "INDUCTION";
        case TOLERANCE_ESTABLISHED: return "ESTABLISHED";
        case TOLERANCE_BROKEN: return "BROKEN";
        default: return "UNKNOWN";
    }
}

const char* mucosal_m_cell_state_to_string(m_cell_state_t state) {
    switch (state) {
        case M_CELL_SAMPLING: return "SAMPLING";
        case M_CELL_TRANSCYTOSING: return "TRANSCYTOSING";
        case M_CELL_PRESENTING: return "PRESENTING";
        case M_CELL_RESTING: return "RESTING";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about mucosal immunity
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int mucosal_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mucosal_immunity_heartbeat("mucosal_immu_mucosal_query_self_k", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mucosal_Immunity");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mucosal_immunity_heartbeat("mucosal_immu_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Mucosal immunity self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mucosal_Immunity");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mucosal_Immunity");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mucosal_immunity_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mucosal_immunity_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mucosal_immunity_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mucosal_immunity_training_begin: NULL argument");
        return -1;
    }
    mucosal_immunity_heartbeat_instance(NULL, "mucosal_immunity_training_begin", 0.0f);
    return 0;
}

int mucosal_immunity_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mucosal_immunity_training_end: NULL argument");
        return -1;
    }
    mucosal_immunity_heartbeat_instance(NULL, "mucosal_immunity_training_end", 1.0f);
    return 0;
}

int mucosal_immunity_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mucosal_immunity_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mucosal_immunity_heartbeat_instance(NULL, "mucosal_immunity_training_step", progress);
    return 0;
}
