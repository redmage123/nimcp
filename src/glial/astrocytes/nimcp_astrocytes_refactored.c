/**
 * @file nimcp_astrocytes.c
 * @brief Refactored astrocyte module with async, unified memory, logging, config, security
 *
 * REFACTORING GOALS:
 * 1. Replace all malloc/free with unified memory
 * 2. Add async event communication to decouple from other modules
 * 3. Add comprehensive logging at all key points
 * 4. Make hyperparameters configurable via config system
 * 5. Register with security system
 *
 * BIOLOGICAL CONTEXT:
 * Astrocytes are star-shaped glial cells that perform critical functions:
 * - Cover ~100,000 synapses in mammalian cortex
 * - Calcium waves propagate via IP3/gap junctions at 10-30 µm/s
 * - Release glutamate/D-serine to modulate synaptic transmission
 * - Enforce homeostatic plasticity (synaptic scaling)
 *
 * @version 3.0.0 - Refactored Edition
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "api/nimcp_api_exception.h"

#define LOG_MODULE "ASTROCYTES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocytes_refactored)

#include "glial/astrocytes/nimcp_astrocytes.h"
#include "async/nimcp_future.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "security/nimcp_security.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Module Constants and Configuration Keys
//=============================================================================

#define ASTROCYTE_MODULE_NAME "astrocytes"
#define ASTROCYTE_SECURITY_MODULE_ID 0x4153544F  // 'ASTO'

// Configuration keys
#define CFG_ASTRO_MAX_SYNAPSES "astrocytes.max_synapses"
#define CFG_ASTRO_MAX_COUPLED "astrocytes.max_coupled"
#define CFG_ASTRO_BASELINE_CALCIUM "astrocytes.baseline_calcium_um"
#define CFG_ASTRO_WAVE_THRESHOLD "astrocytes.wave_threshold_um"
#define CFG_ASTRO_COUPLING_RADIUS "astrocytes.coupling_radius_um"
#define CFG_ASTRO_CALCIUM_DIFFUSION "astrocytes.calcium_diffusion_coeff"
#define CFG_ASTRO_IP3_DIFFUSION "astrocytes.ip3_diffusion_coeff"
#define CFG_ASTRO_WAVE_SPEED "astrocytes.wave_speed_target"
#define CFG_ASTRO_IP3_PRODUCTION "astrocytes.ip3_production_rate"
#define CFG_ASTRO_IP3_DEGRADATION "astrocytes.ip3_degradation_rate"
#define CFG_ASTRO_CA_RELEASE_FLUX "astrocytes.calcium_release_flux"
#define CFG_ASTRO_CA_UPTAKE_RATE "astrocytes.calcium_uptake_rate"

// Event types for async communication
#define ASTRO_EVENT_CALCIUM_SPIKE "astrocyte.calcium_spike"
#define ASTRO_EVENT_WAVE_PROPAGATION "astrocyte.wave_propagation"
#define ASTRO_EVENT_GLUTAMATE_RELEASE "astrocyte.glutamate_release"
#define ASTRO_EVENT_HOMEOSTATIC_SCALING "astrocyte.homeostatic_scaling"

//=============================================================================
// Module State
//=============================================================================

static bool g_astrocyte_module_initialized = false;
static nimcp_mutex_t g_astrocyte_module_lock;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get configured value with fallback to default
 */
static inline float get_config_float(const char* key, float default_value) {
    return (float)config_get_float(key, (double)default_value);
}

static inline int get_config_int(const char* key, int default_value) {
    return (int)config_get_int(key, (int64_t)default_value);
}

/**
 * @brief Calculate euclidean distance between two astrocytes
 */
static inline float astro_distance(const astrocyte_t* a1, const astrocyte_t* a2) {
    if (!a1 || !a2) return INFINITY;

    float dx = a1->position[0] - a2->position[0];
    float dy = a1->position[1] - a2->position[1];
    float dz = a1->position[2] - a2->position[2];

    return sqrtf(dx*dx + dy*dy + dz*dz);
}

//=============================================================================
// Module Initialization and Shutdown
//=============================================================================

/**
 * @brief Initialize astrocyte module
 *
 * WHAT: One-time module initialization
 * WHY:  Register with security, setup logging, load config
 * HOW:  Call once at startup before creating astrocytes
 */
nimcp_result_t astrocyte_module_init(void) {
    nimcp_mutex_lock(&g_astrocyte_module_lock);

    if (g_astrocyte_module_initialized) {
        LOG_MODULE_WARN(ASTROCYTE_MODULE_NAME, "Module already initialized");
        nimcp_mutex_unlock(&g_astrocyte_module_lock);
        return NIMCP_SUCCESS;
    }

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME, "Initializing astrocyte module");

    // Register with security system
    nimcp_result_t result = security_register_module(
        ASTROCYTE_MODULE_NAME,
        ASTROCYTE_SECURITY_MODULE_ID,
        "Astrocyte glial cell simulation with calcium dynamics"
    );

    if (result != NIMCP_SUCCESS) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to register with security system: error=%d", result);
        nimcp_mutex_unlock(&g_astrocyte_module_lock);
        return result;
    }

    // Log configuration values
    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME, "Configuration loaded:");
    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME, "  max_synapses=%d",
        get_config_int(CFG_ASTRO_MAX_SYNAPSES, ASTROCYTE_MAX_SYNAPSES));
    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME, "  baseline_calcium=%.3f µM",
        get_config_float(CFG_ASTRO_BASELINE_CALCIUM, ASTROCYTE_BASELINE_CALCIUM_UM));
    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME, "  wave_threshold=%.3f µM",
        get_config_float(CFG_ASTRO_WAVE_THRESHOLD, ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM));

    g_astrocyte_module_initialized = true;

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME, "Astrocyte module initialized successfully");
    nimcp_mutex_unlock(&g_astrocyte_module_lock);

    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown astrocyte module
 */
void astrocyte_module_shutdown(void) {
    nimcp_mutex_lock(&g_astrocyte_module_lock);

    if (!g_astrocyte_module_initialized) {
        nimcp_mutex_unlock(&g_astrocyte_module_lock);
        return;
    }

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME, "Shutting down astrocyte module");

    g_astrocyte_module_initialized = false;

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME, "Astrocyte module shutdown complete");
    nimcp_mutex_unlock(&g_astrocyte_module_lock);
}

//=============================================================================
// Astrocyte Creation and Destruction
//=============================================================================

astrocyte_t* astrocyte_create(uint32_t id, astrocyte_type_t type,
                               float x, float y, float z, float coverage_radius) {
    if (!g_astrocyte_module_initialized) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Cannot create astrocyte: module not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "g_astrocyte_module_initialized is NULL");

        return NULL;
    }

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Creating astrocyte id=%u type=%d pos=(%.1f,%.1f,%.1f) radius=%.1f",
        id, type, x, y, z, coverage_radius);

    // Allocate astrocyte using unified memory
    astrocyte_t* astro = (astrocyte_t*)nimcp_calloc(1, sizeof(astrocyte_t));
    if (!astro) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to allocate astrocyte id=%u: out of memory", id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro is NULL");

        return NULL;
    }

    // Initialize identity
    astro->id = id;
    astro->type = type;

    // Initialize spatial position
    astro->position[0] = x;
    astro->position[1] = y;
    astro->position[2] = z;
    astro->coverage_radius = coverage_radius;

    // Initialize calcium dynamics from config
    astro->calcium_baseline = get_config_float(CFG_ASTRO_BASELINE_CALCIUM,
                                                ASTROCYTE_BASELINE_CALCIUM_UM);
    astro->calcium_concentration = astro->calcium_baseline;
    astro->ip3_concentration = 0.0f;
    astro->last_calcium_spike = 0;

    // Initialize neurotransmitter pools
    astro->glutamate_pool = 1.0f;  // Start full
    astro->d_serine_pool = 1.0f;   // Start full
    astro->atp_level = 1.0f;       // Start full

    // Allocate synapse coverage arrays
    int max_synapses = get_config_int(CFG_ASTRO_MAX_SYNAPSES, ASTROCYTE_MAX_SYNAPSES);
    astro->covered_synapse_ids = (uint32_t*)nimcp_malloc(max_synapses * sizeof(uint32_t));
    astro->synapse_calcium_levels = (float*)nimcp_malloc(max_synapses * sizeof(float));

    if (!astro->covered_synapse_ids || !astro->synapse_calcium_levels) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to allocate synapse arrays for astrocyte id=%u", id);
        if (astro->covered_synapse_ids) nimcp_free(astro->covered_synapse_ids);
        if (astro->synapse_calcium_levels) nimcp_free(astro->synapse_calcium_levels);
        nimcp_free(astro);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_module_shutdown: validation failed");
        return NULL;
    }

    astro->num_covered_synapses = 0;
    memset(astro->covered_synapse_ids, 0, max_synapses * sizeof(uint32_t));
    memset(astro->synapse_calcium_levels, 0, max_synapses * sizeof(float));

    // Allocate gap junction coupling arrays
    int max_coupled = get_config_int(CFG_ASTRO_MAX_COUPLED, ASTROCYTE_MAX_COUPLED);
    astro->coupled_astrocyte_ids = (uint32_t*)nimcp_malloc(max_coupled * sizeof(uint32_t));
    astro->coupling_strengths = (float*)nimcp_malloc(max_coupled * sizeof(float));

    if (!astro->coupled_astrocyte_ids || !astro->coupling_strengths) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to allocate coupling arrays for astrocyte id=%u", id);
        nimcp_free(astro->covered_synapse_ids);
        nimcp_free(astro->synapse_calcium_levels);
        if (astro->coupled_astrocyte_ids) nimcp_free(astro->coupled_astrocyte_ids);
        if (astro->coupling_strengths) nimcp_free(astro->coupling_strengths);
        nimcp_free(astro);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_module_shutdown: validation failed");
        return NULL;
    }

    astro->num_coupled_astrocytes = 0;
    memset(astro->coupled_astrocyte_ids, 0, max_coupled * sizeof(uint32_t));
    memset(astro->coupling_strengths, 0, max_coupled * sizeof(float));

    // Initialize homeostatic regulation
    astro->target_activity_level = 0.1f;  // 10 Hz target
    astro->scaling_factor = 1.0f;

    // Initialize thread safety
    nimcp_spinlock_init(&astro->lock);

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Created astrocyte id=%u successfully", id);

    return astro;
}

void astrocyte_destroy(astrocyte_t* astro) {
    if (!astro) return;

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Destroying astrocyte id=%u", astro->id);

    // Free all arrays using unified memory
    if (astro->covered_synapse_ids) {
        nimcp_free(astro->covered_synapse_ids);
    }
    if (astro->synapse_calcium_levels) {
        nimcp_free(astro->synapse_calcium_levels);
    }
    if (astro->coupled_astrocyte_ids) {
        nimcp_free(astro->coupled_astrocyte_ids);
    }
    if (astro->coupling_strengths) {
        nimcp_free(astro->coupling_strengths);
    }

    // Free astrocyte itself
    nimcp_free(astro);

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME, "Astrocyte destroyed");
}

//=============================================================================
// Calcium Dynamics
//=============================================================================

void astrocyte_update_calcium(astrocyte_t* astro, float dt, float external_stimulus) {
    if (!astro || dt <= 0.0f) {
        LOG_MODULE_WARN(ASTROCYTE_MODULE_NAME,
            "Invalid calcium update: astro=%p dt=%f", (void*)astro, dt);
        return;
    }

    nimcp_spinlock_lock(&astro->lock);

    // Get config parameters
    float ip3_prod = get_config_float(CFG_ASTRO_IP3_PRODUCTION, IP3_PRODUCTION_RATE);
    float ip3_deg = get_config_float(CFG_ASTRO_IP3_DEGRADATION, IP3_DEGRADATION_RATE);
    float ca_flux = get_config_float(CFG_ASTRO_CA_RELEASE_FLUX, CALCIUM_RELEASE_FLUX);
    float ca_uptake = get_config_float(CFG_ASTRO_CA_UPTAKE_RATE, CALCIUM_UPTAKE_RATE);

    // Update IP3 concentration
    float d_ip3 = ip3_prod * external_stimulus - ip3_deg * astro->ip3_concentration;
    astro->ip3_concentration += d_ip3 * dt;
    astro->ip3_concentration = fmaxf(0.0f, fminf(5.0f, astro->ip3_concentration));

    // IP3-dependent calcium release from ER
    float ip3_factor = astro->ip3_concentration / (astro->ip3_concentration + 1.0f);
    float J_release = ca_flux * ip3_factor;

    // Calcium uptake (ATP-dependent pumps)
    float J_uptake = ca_uptake * (astro->calcium_concentration - astro->calcium_baseline);

    // Update calcium concentration
    float d_ca = J_release - J_uptake;
    astro->calcium_concentration += d_ca * dt;
    astro->calcium_concentration = fmaxf(astro->calcium_baseline,
                                         fminf(10.0f, astro->calcium_concentration));

    // Check for calcium spike
    float wave_threshold = get_config_float(CFG_ASTRO_WAVE_THRESHOLD,
                                           ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM);

    if (astro->calcium_concentration >= wave_threshold) {
        uint64_t now = nimcp_time_monotonic_us();
        uint64_t since_last = now - astro->last_calcium_spike;

        // Refractory period: only log if > 100ms since last spike
        if (since_last > 100000) {
            LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
                "Calcium spike detected: astro=%u Ca=%.3f µM (threshold=%.3f µM)",
                astro->id, astro->calcium_concentration, wave_threshold);

            astro->last_calcium_spike = now;

            // Publish async event for calcium spike via bio-async
            // This notifies other modules (e.g., synapses, BCM) without tight coupling
            // Uses predictive coding: observers only notified on prediction errors
            if (bio_router_is_initialized()) {
                // Get the astrocyte bio-async context from the main module
                // This shares the context registered in nimcp_astrocytes.c
                extern bio_module_context_t g_astrocyte_bio_ctx;
                extern nimcp_atomic_bool_t g_astrocyte_bio_initialized;

                if (nimcp_atomic_load_bool(&g_astrocyte_bio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE) &&
                    g_astrocyte_bio_ctx) {
                    // Publish calcium spike as a predictive signal
                    // Signal name encodes astrocyte ID for specific routing
                    char signal_name[NIMCP_ID_BUFFER_SIZE];
                    snprintf(signal_name, sizeof(signal_name), "astrocyte.calcium_spike.%u", astro->id);
                    bio_router_publish_signal(g_astrocyte_bio_ctx, signal_name, astro->calcium_concentration);

                    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
                        "Published calcium spike event: astro=%u Ca=%.3f µM",
                        astro->id, astro->calcium_concentration);
                }
            }
        }
    }

    nimcp_spinlock_unlock(&astro->lock);
}

void astrocyte_propagate_calcium_wave(astrocyte_t* astro, astrocyte_network_t* network, float dt) {
    if (!astro || !network || dt <= 0.0f) {
        LOG_MODULE_WARN(ASTROCYTE_MODULE_NAME,
            "Invalid wave propagation: astro=%p network=%p dt=%f",
            (void*)astro, (void*)network, dt);
        return;
    }

    float coupling_decay = network->coupling_decay_rate * dt;

    // Propagate to each coupled neighbor
    for (uint32_t i = 0; i < astro->num_coupled_astrocytes; i++) {
        uint32_t neighbor_id = astro->coupled_astrocyte_ids[i];
        float coupling_strength = astro->coupling_strengths[i];

        // Find neighbor astrocyte
        astrocyte_t* neighbor = NULL;
        for (uint32_t j = 0; j < network->num_astrocytes; j++) {
            if (network->astrocytes[j] && network->astrocytes[j]->id == neighbor_id) {
                neighbor = network->astrocytes[j];
                break;
            }
        }

        if (!neighbor) continue;

        // Lock both astrocytes in address order to prevent AB-BA deadlock
        astrocyte_t* first = (astro < neighbor) ? astro : neighbor;
        astrocyte_t* second = (astro < neighbor) ? neighbor : astro;
        nimcp_spinlock_lock(&first->lock);
        if (first != second) {
            nimcp_spinlock_lock(&second->lock);
        }

        // Calculate diffusion flux
        float delta_ca = astro->calcium_concentration - neighbor->calcium_concentration;
        float flux = coupling_strength * coupling_decay * delta_ca;

        // Update neighbor's calcium
        neighbor->calcium_concentration += flux;
        neighbor->calcium_concentration = fmaxf(neighbor->calcium_baseline,
                                               fminf(10.0f, neighbor->calcium_concentration));
        if (first != second) {
            nimcp_spinlock_unlock(&second->lock);
        }
        nimcp_spinlock_unlock(&first->lock);

        if (fabsf(flux) > 0.01f) {
            LOG_MODULE_TRACE(ASTROCYTE_MODULE_NAME,
                "Ca wave: astro %u → %u, flux=%.4f, delta=%.4f",
                astro->id, neighbor_id, flux, delta_ca);
        }
    }
}

//=============================================================================
// Synaptic Coverage Management
//=============================================================================

nimcp_result_t astrocyte_add_synapse(astrocyte_t* astro, uint32_t synapse_id) {
    NIMCP_CHECK_THROW(astro, NIMCP_ERROR_NULL_POINTER, "astro is NULL");

    nimcp_spinlock_lock(&astro->lock);

    int max_synapses = get_config_int(CFG_ASTRO_MAX_SYNAPSES, ASTROCYTE_MAX_SYNAPSES);

    if (astro->num_covered_synapses >= (uint32_t)max_synapses) {
        LOG_MODULE_WARN(ASTROCYTE_MODULE_NAME,
            "Astrocyte %u at synapse capacity (%d), cannot add synapse %u",
            astro->id, max_synapses, synapse_id);
        nimcp_spinlock_unlock(&astro->lock);
        return NIMCP_BUFFER_FULL;
    }

    // Check for duplicates
    for (uint32_t i = 0; i < astro->num_covered_synapses; i++) {
        if (astro->covered_synapse_ids[i] == synapse_id) {
            LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
                "Synapse %u already covered by astrocyte %u",
                synapse_id, astro->id);
            nimcp_spinlock_unlock(&astro->lock);
            return NIMCP_SUCCESS;
        }
    }

    // Add synapse
    uint32_t idx = astro->num_covered_synapses;
    astro->covered_synapse_ids[idx] = synapse_id;
    astro->synapse_calcium_levels[idx] = astro->calcium_baseline;
    astro->num_covered_synapses++;

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Astrocyte %u now covers synapse %u (%u total)",
        astro->id, synapse_id, astro->num_covered_synapses);

    nimcp_spinlock_unlock(&astro->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t astrocyte_remove_synapse(astrocyte_t* astro, uint32_t synapse_id) {
    NIMCP_CHECK_THROW(astro, NIMCP_ERROR_NULL_POINTER, "astro is NULL");

    nimcp_spinlock_lock(&astro->lock);

    // Find synapse
    int32_t found_idx = -1;
    for (uint32_t i = 0; i < astro->num_covered_synapses; i++) {
        if (astro->covered_synapse_ids[i] == synapse_id) {
            found_idx = (int32_t)i;
            break;
        }
    }

    if (found_idx < 0) {
        LOG_MODULE_WARN(ASTROCYTE_MODULE_NAME,
            "Synapse %u not found in astrocyte %u coverage",
            synapse_id, astro->id);
        nimcp_spinlock_unlock(&astro->lock);
        return NIMCP_NOT_FOUND;
    }

    // Remove by shifting
    uint32_t idx = (uint32_t)found_idx;
    for (uint32_t i = idx; i < astro->num_covered_synapses - 1; i++) {
        astro->covered_synapse_ids[i] = astro->covered_synapse_ids[i + 1];
        astro->synapse_calcium_levels[i] = astro->synapse_calcium_levels[i + 1];
    }
    astro->num_covered_synapses--;

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Removed synapse %u from astrocyte %u (%u remaining)",
        synapse_id, astro->id, astro->num_covered_synapses);

    nimcp_spinlock_unlock(&astro->lock);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Gap Junction Coupling Management
//=============================================================================

nimcp_result_t astrocyte_couple(astrocyte_t* astro1, astrocyte_t* astro2,
                                 float coupling_strength) {
    NIMCP_CHECK_THROW(astro1 && astro2, NIMCP_ERROR_NULL_POINTER, "astro1 or astro2 is NULL");
    if (astro1 == astro2) return NIMCP_ERROR_INVALID_PARAM;

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Coupling astrocytes %u and %u with strength %.3f",
        astro1->id, astro2->id, coupling_strength);

    int max_coupled = get_config_int(CFG_ASTRO_MAX_COUPLED, ASTROCYTE_MAX_COUPLED);

    // Add astro2 to astro1's coupling list
    nimcp_spinlock_lock(&astro1->lock);

    if (astro1->num_coupled_astrocytes >= (uint32_t)max_coupled) {
        LOG_MODULE_WARN(ASTROCYTE_MODULE_NAME,
            "Astrocyte %u at coupling capacity (%d)",
            astro1->id, max_coupled);
        nimcp_spinlock_unlock(&astro1->lock);
        return NIMCP_BUFFER_FULL;
    }

    uint32_t idx1 = astro1->num_coupled_astrocytes;
    astro1->coupled_astrocyte_ids[idx1] = astro2->id;
    astro1->coupling_strengths[idx1] = coupling_strength;
    astro1->num_coupled_astrocytes++;

    nimcp_spinlock_unlock(&astro1->lock);

    // Add astro1 to astro2's coupling list (bidirectional)
    nimcp_spinlock_lock(&astro2->lock);

    if (astro2->num_coupled_astrocytes >= (uint32_t)max_coupled) {
        // Rollback astro1's coupling
        nimcp_spinlock_lock(&astro1->lock);
        astro1->num_coupled_astrocytes--;
        nimcp_spinlock_unlock(&astro1->lock);

        LOG_MODULE_WARN(ASTROCYTE_MODULE_NAME,
            "Astrocyte %u at coupling capacity (%d), coupling rolled back",
            astro2->id, max_coupled);
        nimcp_spinlock_unlock(&astro2->lock);
        return NIMCP_BUFFER_FULL;
    }

    uint32_t idx2 = astro2->num_coupled_astrocytes;
    astro2->coupled_astrocyte_ids[idx2] = astro1->id;
    astro2->coupling_strengths[idx2] = coupling_strength;
    astro2->num_coupled_astrocytes++;

    nimcp_spinlock_unlock(&astro2->lock);

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Successfully coupled astrocytes %u and %u",
        astro1->id, astro2->id);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Astrocyte Network Management
//=============================================================================

astrocyte_network_t* astrocyte_network_create(uint32_t capacity) {
    if (capacity == 0) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Cannot create network with zero capacity");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "astrocyte_network_create: capacity is zero");
        return NULL;
    }

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Creating astrocyte network with capacity=%u", capacity);

    astrocyte_network_t* network = (astrocyte_network_t*)nimcp_calloc(1, sizeof(astrocyte_network_t));
    if (!network) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to allocate astrocyte network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;
    }

    network->capacity = capacity;
    network->num_astrocytes = 0;

    // Allocate astrocyte pointer array
    network->astrocytes = (astrocyte_t**)nimcp_calloc(capacity, sizeof(astrocyte_t*));
    if (!network->astrocytes) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to allocate astrocyte array");
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "astrocyte_network_create: network->astrocytes is NULL");
        return NULL;
    }

    // Initialize global parameters from config
    network->calcium_threshold_um = get_config_float(CFG_ASTRO_WAVE_THRESHOLD,
                                                     ASTROCYTE_CALCIUM_WAVE_THRESHOLD_UM);
    network->coupling_decay_rate = 0.1f;  // 1/ms
    network->coupling_radius_um = get_config_float(CFG_ASTRO_COUPLING_RADIUS,
                                                   ASTROCYTE_COUPLING_RADIUS_UM);

    // Initialize spatial index (placeholder - would use KD-tree)
    network->spatial_index = NULL;

    // Calcium system initialized separately
    network->calcium_system = NULL;

    // Initialize thread safety
    nimcp_mutex_init(&network->lock, NULL);

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Astrocyte network created successfully");

    return network;
}

void astrocyte_network_destroy(astrocyte_network_t* network) {
    if (!network) return;

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Destroying astrocyte network with %u astrocytes",
        network->num_astrocytes);

    // Destroy all astrocytes
    if (network->astrocytes) {
        for (uint32_t i = 0; i < network->num_astrocytes; i++) {
            if (network->astrocytes[i]) {
                astrocyte_destroy(network->astrocytes[i]);
            }
        }
        nimcp_free(network->astrocytes);
    }

    // Destroy calcium system if exists
    if (network->calcium_system) {
        astrocyte_calcium_system_destroy(network->calcium_system);
    }

    // Destroy spatial index if exists
    if (network->spatial_index) {
        // kdtree_destroy(network->spatial_index);
    }

    nimcp_mutex_destroy(&network->lock);
    nimcp_free(network);

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Astrocyte network destroyed");
}

nimcp_result_t astrocyte_network_add(astrocyte_network_t* network, astrocyte_t* astro) {
    NIMCP_CHECK_THROW(network && astro, NIMCP_ERROR_NULL_POINTER, "network or astro is NULL");

    nimcp_mutex_lock(&network->lock);

    if (network->num_astrocytes >= network->capacity) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Network at capacity (%u), cannot add astrocyte %u",
            network->capacity, astro->id);
        nimcp_mutex_unlock(&network->lock);
        return NIMCP_BUFFER_FULL;
    }

    network->astrocytes[network->num_astrocytes] = astro;
    network->num_astrocytes++;

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Added astrocyte %u to network (%u total)",
        astro->id, network->num_astrocytes);

    nimcp_mutex_unlock(&network->lock);
    return NIMCP_SUCCESS;
}

void astrocyte_network_auto_couple(astrocyte_network_t* network) {
    if (!network) return;

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Auto-coupling %u astrocytes within radius %.1f µm",
        network->num_astrocytes, network->coupling_radius_um);

    uint32_t coupling_count = 0;

    // Simple O(n²) coupling - would use spatial index for large networks
    for (uint32_t i = 0; i < network->num_astrocytes; i++) {
        astrocyte_t* a1 = network->astrocytes[i];
        if (!a1) continue;

        for (uint32_t j = i + 1; j < network->num_astrocytes; j++) {
            astrocyte_t* a2 = network->astrocytes[j];
            if (!a2) continue;

            float dist = astro_distance(a1, a2);

            if (dist <= network->coupling_radius_um) {
                // Coupling strength inversely proportional to distance
                float strength = 1.0f - (dist / network->coupling_radius_um);

                if (astrocyte_couple(a1, a2, strength) == NIMCP_SUCCESS) {
                    coupling_count++;
                }
            }
        }
    }

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Auto-coupling complete: %u gap junctions formed",
        coupling_count);
}

//=============================================================================
// Calcium System (Reaction-Diffusion)
//=============================================================================

astrocyte_calcium_system_t* astrocyte_calcium_system_create(astrocyte_network_t* network) {
    if (!network) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Cannot create calcium system: network is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;
    }

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Creating calcium system for %u astrocytes",
        network->num_astrocytes);

    astrocyte_calcium_system_t* sys = (astrocyte_calcium_system_t*)nimcp_calloc(1,
        sizeof(astrocyte_calcium_system_t));
    if (!sys) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to allocate calcium system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sys is NULL");

        return NULL;
    }

    sys->num_astrocytes = network->num_astrocytes;
    sys->network = network;

    // Allocate state arrays
    sys->calcium = (float*)nimcp_calloc(sys->num_astrocytes, sizeof(float));
    sys->ip3 = (float*)nimcp_calloc(sys->num_astrocytes, sizeof(float));
    sys->calcium_er = (float*)nimcp_calloc(sys->num_astrocytes, sizeof(float));

    // Allocate workspace arrays
    sys->workspace_dCa = (float*)nimcp_calloc(sys->num_astrocytes, sizeof(float));
    sys->workspace_dIP3 = (float*)nimcp_calloc(sys->num_astrocytes, sizeof(float));
    sys->workspace_dCaER = (float*)nimcp_calloc(sys->num_astrocytes, sizeof(float));

    // Allocate wave tracking
    sys->last_wave_time = (uint64_t*)nimcp_calloc(sys->num_astrocytes, sizeof(uint64_t));

    if (!sys->calcium || !sys->ip3 || !sys->calcium_er ||
        !sys->workspace_dCa || !sys->workspace_dIP3 || !sys->workspace_dCaER ||
        !sys->last_wave_time) {
        LOG_MODULE_ERROR(ASTROCYTE_MODULE_NAME,
            "Failed to allocate calcium system arrays");
        if (sys->calcium) nimcp_free(sys->calcium);
        if (sys->ip3) nimcp_free(sys->ip3);
        if (sys->calcium_er) nimcp_free(sys->calcium_er);
        if (sys->workspace_dCa) nimcp_free(sys->workspace_dCa);
        if (sys->workspace_dIP3) nimcp_free(sys->workspace_dIP3);
        if (sys->workspace_dCaER) nimcp_free(sys->workspace_dCaER);
        if (sys->last_wave_time) nimcp_free(sys->last_wave_time);
        nimcp_free(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astrocyte_calcium_system_create: validation failed");
        return NULL;
    }

    // Load config parameters
    sys->D_ca = get_config_float(CFG_ASTRO_CALCIUM_DIFFUSION, CALCIUM_DIFFUSION_COEFF);
    sys->D_ip3 = get_config_float(CFG_ASTRO_IP3_DIFFUSION, IP3_DIFFUSION_COEFF);
    sys->ip3_production_rate = get_config_float(CFG_ASTRO_IP3_PRODUCTION, IP3_PRODUCTION_RATE);
    sys->ip3_degradation_rate = get_config_float(CFG_ASTRO_IP3_DEGRADATION, IP3_DEGRADATION_RATE);
    sys->ca_release_flux = get_config_float(CFG_ASTRO_CA_RELEASE_FLUX, CALCIUM_RELEASE_FLUX);
    sys->ca_uptake_rate = get_config_float(CFG_ASTRO_CA_UPTAKE_RATE, CALCIUM_UPTAKE_RATE);

    // Initialize state from astrocytes
    for (uint32_t i = 0; i < sys->num_astrocytes; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            sys->calcium[i] = astro->calcium_concentration;
            sys->ip3[i] = astro->ip3_concentration;
            sys->calcium_er[i] = 400.0f;  // ER calcium store in µM
        }
    }

    sys->wave_speed_measured = 0.0f;
    sys->total_update_time_us = 0;
    sys->num_updates = 0;

    nimcp_spinlock_init(&sys->lock);

    LOG_MODULE_INFO(ASTROCYTE_MODULE_NAME,
        "Calcium system created successfully");

    return sys;
}

void astrocyte_calcium_system_destroy(astrocyte_calcium_system_t* sys) {
    if (!sys) return;

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Destroying calcium system");

    if (sys->calcium) nimcp_free(sys->calcium);
    if (sys->ip3) nimcp_free(sys->ip3);
    if (sys->calcium_er) nimcp_free(sys->calcium_er);
    if (sys->workspace_dCa) nimcp_free(sys->workspace_dCa);
    if (sys->workspace_dIP3) nimcp_free(sys->workspace_dIP3);
    if (sys->workspace_dCaER) nimcp_free(sys->workspace_dCaER);
    if (sys->last_wave_time) nimcp_free(sys->last_wave_time);

    nimcp_free(sys);

    LOG_MODULE_DEBUG(ASTROCYTE_MODULE_NAME,
        "Calcium system destroyed");
}

//=============================================================================
// Global Module Initialization (Called Once at Startup)
//=============================================================================

static void __attribute__((constructor)) astrocyte_module_constructor(void) {
    nimcp_mutex_init(&g_astrocyte_module_lock, NULL);
}

static void __attribute__((destructor)) astrocyte_module_destructor(void) {
    astrocyte_module_shutdown();
    nimcp_mutex_destroy(&g_astrocyte_module_lock);
}
