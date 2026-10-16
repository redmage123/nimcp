/**
 * @file nimcp_pr_visual_bridge.c
 * @brief Implementation of Prime Resonant Visual Bridge
 *
 * @version 1.0.0
 * @date 2025-01-09
 */

#include "cognitive/memory/core/nimcp_pr_visual_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_visual_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_visual_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_visual_bridge_mesh_registry = NULL;

nimcp_error_t pr_visual_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_visual_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_visual_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_visual_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_visual_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_visual_bridge_mesh_registry = registry;
    return err;
}

void pr_visual_bridge_mesh_unregister(void) {
    if (g_pr_visual_bridge_mesh_registry && g_pr_visual_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_visual_bridge_mesh_registry, g_pr_visual_bridge_mesh_id);
        g_pr_visual_bridge_mesh_id = 0;
        g_pr_visual_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_visual_bridge module (instance-level) */
static inline void pr_visual_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_visual_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_visual_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_visual_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_VISUAL_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS(pr_visual_bridge)

/* ============================================================================
 * Static Constants
 * ============================================================================ */

/* Note: PRIMES_64 is defined in nimcp_prime_signature.h */

/** Error strings */
static const char* ERROR_STRINGS[] = {
    "Success",
    "Null parameter provided",
    "Invalid configuration",
    "Bridge not initialized",
    "Visual cortex not connected",
    "FEP bridge not connected",
    "Retina not connected",
    "Visual memory capacity exceeded",
    "Memory encoding failed",
    "Memory retrieval failed",
    "Theta-gamma phase window mismatch",
    "Prime signature computation failed",
    "Quaternion computation failed",
    "Entanglement operation failed",
    "Memory allocation failed",
    "Mutex operation failed"
};

/* Thread-local error storage */
static _Thread_local pr_visual_bridge_error_t g_last_error = PR_VISUAL_OK;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Set last error and return it
 */
static pr_visual_bridge_error_t set_error(pr_visual_bridge_t* bridge,
                                          pr_visual_bridge_error_t error) {
    g_last_error = error;
    if (bridge) {
        bridge->last_error = error;
    }
    return error;
}

/**
 * @brief Hash function for feature bin to prime index mapping
 */
static uint32_t hash_feature_bin(uint32_t bin_index, uint32_t feature_type) {
    uint32_t hash = bin_index * 31 + feature_type * 17;
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);
    return hash % PR_VISUAL_PRIME_COUNT;
}

/**
 * @brief Quantize float value to bin index
 */
static uint32_t quantize_to_bin(float value, uint32_t num_bins) {
    if (value <= 0.0f) return 0;
    if (value >= 1.0f) return num_bins - 1;
    return (uint32_t)(value * (float)num_bins);
}

/**
 * @brief Compute color warmth from visual cortex features
 */
static float compute_color_warmth(const pr_visual_feature_vector_t* features) {
    if (!features) return 0.5f;

    /* Color warmth based on histogram distribution
     * Higher values in red/warm bins = higher warmth */
    float warm_sum = 0.0f;
    float cool_sum = 0.0f;

    /* First half of color histogram assumed to be warm colors */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS / 2; i++) {
        warm_sum += features->color_histogram[i];
    }
    /* Second half assumed to be cool colors */
    for (uint32_t i = PR_VISUAL_FEATURE_BINS / 2; i < PR_VISUAL_FEATURE_BINS; i++) {
        cool_sum += features->color_histogram[i];
    }

    float total = warm_sum + cool_sum;
    if (total < 0.001f) return 0.5f;

    return warm_sum / total;
}

/**
 * @brief Compute attention intensity from saliency bins
 */
static float compute_attention_intensity(const pr_visual_feature_vector_t* features) {
    if (!features) return 0.0f;

    float sum = 0.0f;
    float max_val = 0.0f;

    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        sum += features->saliency_bins[i];
        if (features->saliency_bins[i] > max_val) {
            max_val = features->saliency_bins[i];
        }
    }

    /* Combine mean and max for attention intensity */
    float mean = sum / PR_VISUAL_FEATURE_BINS;
    return 0.5f * mean + 0.5f * max_val;
}

/**
 * @brief Lock bridge mutex
 */
static pr_visual_bridge_error_t lock_bridge(pr_visual_bridge_t* bridge) {
    if (!bridge || !bridge->base.mutex) {
        return PR_VISUAL_ERROR_NULL_PARAM;
    }
    if (nimcp_mutex_lock(bridge->base.mutex) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return PR_VISUAL_ERROR_MUTEX;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return PR_VISUAL_OK;
}

/**
 * @brief Unlock bridge mutex
 */
static void unlock_bridge(pr_visual_bridge_t* bridge) {
    if (bridge && bridge->base.mutex) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }
}

/**
 * @brief Allocate memory node from pool
 */
static pr_memory_node_t* allocate_memory_node(pr_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_memory_node: bridge is NULL");
        return NULL;
    }

    if (bridge->memory_pool_count >= bridge->memory_pool_size) {
        /* Pool is full */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_memory_node: capacity exceeded");
        return NULL;
    }

    pr_memory_node_t* node = nimcp_calloc(1, sizeof(pr_memory_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_memory_node: node is NULL");
        return NULL;
    }

    bridge->memory_pool[bridge->memory_pool_count++] = node;
    return node;
}

/**
 * @brief Compare function for sorting retrieval results by resonance score
 */
static int compare_retrieval_results(const void* a, const void* b) {
    const pr_visual_retrieval_result_t* ra = (const pr_visual_retrieval_result_t*)a;
    const pr_visual_retrieval_result_t* rb = (const pr_visual_retrieval_result_t*)b;

    if (ra->resonance_score > rb->resonance_score) return -1;
    if (ra->resonance_score < rb->resonance_score) return 1;
    return 0;
}

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_config_t pr_visual_bridge_default_config(void) {
    pr_visual_bridge_config_t config = {0};

    config.max_memories = PR_VISUAL_MAX_MEMORIES;
    config.theta_gamma_coupling = PR_VISUAL_DEFAULT_TG_COUPLING;
    config.emotion_weight = PR_VISUAL_DEFAULT_EMOTION_WT;
    config.salience_weight = PR_VISUAL_DEFAULT_SALIENCE_WT;
    config.accessibility_weight = PR_VISUAL_DEFAULT_ACCESS_WT;
    config.consolidation_decay = PR_VISUAL_DEFAULT_CONSOL_DECAY;
    config.pe_threshold = PR_VISUAL_DEFAULT_PE_THRESHOLD;
    config.resonance_threshold = PR_VISUAL_DEFAULT_RES_THRESHOLD;
    config.auto_entangle = true;
    config.enable_active_inference = true;
    config.enable_phase_gating = true;

    /* Default prime signature config */
    config.sig_config.hash_rounds = 16;
    config.sig_config.seed = 0x12345678;
    config.sig_config.normalize_exponents = true;
    config.sig_config.sparsity_target = 0.3f;

    /* Default resonance config */
    config.resonance_config.weight_jaccard = 0.3f;
    config.resonance_config.weight_phase = 0.2f;
    config.resonance_config.weight_quaternion = 0.3f;
    config.resonance_config.weight_kuramoto = 0.2f;
    config.resonance_config.threshold = 0.5f;
    config.resonance_config.normalize_weights = true;

    return config;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_t* pr_visual_bridge_create(
    const pr_visual_bridge_config_t* config) {

    pr_visual_bridge_t* bridge = nimcp_calloc(1, sizeof(pr_visual_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_visual_bridge_create: failed to allocate bridge");
        set_error(NULL, PR_VISUAL_ERROR_ALLOCATION);
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = pr_visual_bridge_default_config();
    }

    /* Validate configuration */
    if (bridge->config.max_memories == 0 ||
        bridge->config.max_memories > PR_VISUAL_MAX_MEMORIES * 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_create: invalid max_memories config");
        nimcp_free(bridge);
        bridge = NULL;
        set_error(NULL, PR_VISUAL_ERROR_INVALID_CONFIG);
        return NULL;
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_visual") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "pr_visual_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        set_error(NULL, PR_VISUAL_ERROR_MUTEX);
        return NULL;
    }

    /* Allocate memory pool */
    bridge->memory_pool = nimcp_calloc(bridge->config.max_memories,
                                        sizeof(pr_memory_node_t*));
    if (!bridge->memory_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_visual_bridge_create: failed to allocate memory pool");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        set_error(NULL, PR_VISUAL_ERROR_ALLOCATION);
        return NULL;
    }
    bridge->memory_pool_size = bridge->config.max_memories;
    bridge->memory_pool_count = 0;

    /* Create entanglement graph */
    entangle_config_t entangle_config = entangle_config_default();
    entangle_config.initial_node_capacity = bridge->config.max_memories;
    entangle_config.initial_edge_capacity = bridge->config.max_memories * 8;

    bridge->visual_entanglement = entangle_graph_create(&entangle_config);
    if (!bridge->visual_entanglement) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_visual_bridge_create: failed to create entanglement graph");
        nimcp_free(bridge->memory_pool);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        set_error(NULL, PR_VISUAL_ERROR_ALLOCATION);
        return NULL;
    }

    /* Note: resonance_engine is optional, using NULL for now */
    bridge->resonance_engine = NULL;

    /* Initialize signature config */
    bridge->sig_config = bridge->config.sig_config;

    /* Initialize quaternion to identity-like state */
    bridge->current_visual_quat.w = 0.5f;  /* Medium consolidation */
    bridge->current_visual_quat.x = 0.5f;  /* Neutral emotion */
    bridge->current_visual_quat.y = 0.5f;  /* Medium salience */
    bridge->current_visual_quat.z = 0.5f;  /* Medium accessibility */

    /* Initialize statistics */
    bridge->stats.min_processing_time_ns = UINT64_MAX;

    bridge->initialized = true;

    NIMCP_LOG_INFO("PR Visual Bridge created with max_memories=%u",
                   bridge->config.max_memories);

    return bridge;
}

NIMCP_EXPORT void pr_visual_bridge_destroy(pr_visual_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_visual");

    lock_bridge(bridge);

    /* Free memory pool nodes */
    if (bridge->memory_pool) {
        for (uint32_t i = 0; i < bridge->memory_pool_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->memory_pool_count > 256) {
                pr_visual_bridge_heartbeat("pr_visual_br_loop",
                                 (float)(i + 1) / (float)bridge->memory_pool_count);
            }

            if (bridge->memory_pool[i]) {
                pr_memory_node_destroy(bridge->memory_pool[i]);
            }
        }
        nimcp_free(bridge->memory_pool);
    }

    /* Destroy entanglement graph */
    if (bridge->visual_entanglement) {
        entangle_graph_destroy(bridge->visual_entanglement);
    }

    /* Note: resonance_engine cleanup not needed (was set to NULL) */

    unlock_bridge(bridge);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOG_INFO("PR Visual Bridge destroyed");
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_reset(
    pr_visual_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_reset: bridge is NULL");
        return set_error(NULL, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    /* Reset memory pool */
    for (uint32_t i = 0; i < bridge->memory_pool_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->memory_pool_count > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)bridge->memory_pool_count);
        }

        if (bridge->memory_pool[i]) {
            pr_memory_node_destroy(bridge->memory_pool[i]);
            bridge->memory_pool[i] = NULL;
        }
    }
    bridge->memory_pool_count = 0;
    bridge->current_visual_memory = NULL;

    /* Reset entanglement graph */
    if (bridge->visual_entanglement) {
        entangle_graph_clear(bridge->visual_entanglement);
    }

    /* Reset current state */
    memset(&bridge->current_features, 0, sizeof(bridge->current_features));
    memset(&bridge->current_signature, 0, sizeof(bridge->current_signature));
    bridge->current_visual_quat.w = 0.5f;
    bridge->current_visual_quat.x = 0.5f;
    bridge->current_visual_quat.y = 0.5f;
    bridge->current_visual_quat.z = 0.5f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.min_processing_time_ns = UINT64_MAX;

    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_visual_cortex(
    pr_visual_bridge_t* bridge,
    visual_cortex_t* cortex) {

    if (!bridge || !cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_connect_visual_cortex: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    bridge->visual_cortex = cortex;

    unlock_bridge(bridge);

    NIMCP_LOG_DEBUG("Visual cortex connected to PR bridge");

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_fep_bridge(
    pr_visual_bridge_t* bridge,
    visual_cortex_fep_bridge_t* fep_bridge) {

    if (!bridge || !fep_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_connect_fep_bridge: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    bridge->fep_bridge = fep_bridge;

    unlock_bridge(bridge);

    NIMCP_LOG_DEBUG("FEP bridge connected to PR bridge");

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_retina(
    pr_visual_bridge_t* bridge,
    retina_t* retina) {

    if (!bridge || !retina) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_connect_retina: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    bridge->retina = retina;

    unlock_bridge(bridge);

    NIMCP_LOG_DEBUG("Retina connected to PR bridge");

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_connect_theta_gamma(
    pr_visual_bridge_t* bridge,
    theta_gamma_manager_t* tg_manager) {

    if (!bridge || !tg_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_connect_theta_gamma: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    bridge->theta_gamma = tg_manager;

    unlock_bridge(bridge);

    NIMCP_LOG_DEBUG("Theta-gamma manager connected to PR bridge");

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Feature Extraction Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_extract_features(
    pr_visual_bridge_t* bridge,
    pr_visual_feature_vector_t* features) {

    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_extract_features: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    if (!bridge->visual_cortex) {
        return set_error(bridge, PR_VISUAL_ERROR_NO_VISUAL_CORTEX);
    }

    memset(features, 0, sizeof(pr_visual_feature_vector_t));
    features->timestamp_ns = nimcp_time_now_us() * 1000;  /* Convert us to ns */

    /*
     * NOTE: Feature extraction from visual cortex requires integration
     * with actual visual_cortex_process() API. For now, generate
     * placeholder features based on cortex state.
     *
     * Future integration should call:
     * - visual_cortex_process() to get feature vectors
     * - visual_cortex_compute_attention() for saliency
     * - visual_cortex_compute_novelty() for novelty score
     */

    /* Generate placeholder features - uniform distribution */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        features->color_histogram[i] = 1.0f / PR_VISUAL_FEATURE_BINS;
        features->edge_orientations[i] = 1.0f / PR_VISUAL_FEATURE_BINS;
        features->spatial_frequencies[i] = 1.0f / PR_VISUAL_FEATURE_BINS;
        features->texture_energy[i] = 1.0f / PR_VISUAL_FEATURE_BINS;
        features->multispectral[i] = 0.0f;  /* No multispectral data by default */
        features->saliency_bins[i] = 1.0f / PR_VISUAL_FEATURE_BINS;
    }

    features->feature_count = 6 * PR_VISUAL_FEATURE_BINS;

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Prime Signature Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_compute_visual_prime_sig(
    pr_visual_bridge_t* bridge,
    const pr_visual_feature_vector_t* features,
    prime_signature_t* signature) {

    if (!bridge || !features || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_compute_visual_prime_sig: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    /* Max exponent constant (8 levels: 0-7) */
    const uint8_t MAX_EXPONENT = 8;

    memset(signature, 0, sizeof(prime_signature_t));

    /* Copy primes */
    for (uint32_t i = 0; i < PRIME_SIG_DIM && i < PR_VISUAL_PRIME_COUNT; i++) {
        signature->primes[i] = PRIMES_64[i];
    }

    /* Map features to prime exponents */

    /* Process color histogram */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        if (features->color_histogram[i] > 0.01f) {
            uint32_t prime_idx = hash_feature_bin(i, 0);
            uint8_t exponent = (uint8_t)(features->color_histogram[i] * 7.0f) + 1;
            if (exponent > MAX_EXPONENT) exponent = MAX_EXPONENT;
            if (signature->exponents[prime_idx] < exponent) {
                signature->exponents[prime_idx] = exponent;
            }
        }
    }

    /* Process edge orientations */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        if (features->edge_orientations[i] > 0.01f) {
            uint32_t prime_idx = hash_feature_bin(i, 1);
            uint8_t exponent = (uint8_t)(features->edge_orientations[i] * 7.0f) + 1;
            if (exponent > MAX_EXPONENT) exponent = MAX_EXPONENT;
            if (signature->exponents[prime_idx] < exponent) {
                signature->exponents[prime_idx] = exponent;
            }
        }
    }

    /* Process spatial frequencies */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        if (features->spatial_frequencies[i] > 0.01f) {
            uint32_t prime_idx = hash_feature_bin(i, 2);
            uint8_t exponent = (uint8_t)(features->spatial_frequencies[i] * 7.0f) + 1;
            if (exponent > MAX_EXPONENT) exponent = MAX_EXPONENT;
            if (signature->exponents[prime_idx] < exponent) {
                signature->exponents[prime_idx] = exponent;
            }
        }
    }

    /* Process texture energy */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        if (features->texture_energy[i] > 0.01f) {
            uint32_t prime_idx = hash_feature_bin(i, 3);
            uint8_t exponent = (uint8_t)(features->texture_energy[i] * 7.0f) + 1;
            if (exponent > MAX_EXPONENT) exponent = MAX_EXPONENT;
            if (signature->exponents[prime_idx] < exponent) {
                signature->exponents[prime_idx] = exponent;
            }
        }
    }

    /* Process multispectral features */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        if (features->multispectral[i] > 0.01f) {
            uint32_t prime_idx = hash_feature_bin(i, 4);
            uint8_t exponent = (uint8_t)(features->multispectral[i] * 7.0f) + 1;
            if (exponent > MAX_EXPONENT) exponent = MAX_EXPONENT;
            if (signature->exponents[prime_idx] < exponent) {
                signature->exponents[prime_idx] = exponent;
            }
        }
    }

    /* Process saliency bins */
    for (uint32_t i = 0; i < PR_VISUAL_FEATURE_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_VISUAL_FEATURE_BINS > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)PR_VISUAL_FEATURE_BINS);
        }

        if (features->saliency_bins[i] > 0.01f) {
            uint32_t prime_idx = hash_feature_bin(i, 5);
            uint8_t exponent = (uint8_t)(features->saliency_bins[i] * 7.0f) + 1;
            if (exponent > MAX_EXPONENT) exponent = MAX_EXPONENT;
            if (signature->exponents[prime_idx] < exponent) {
                signature->exponents[prime_idx] = exponent;
            }
        }
    }

    /* Update hash and factor count */
    signature->hash = prime_sig_hash(signature);
    signature->num_factors = prime_sig_recount_factors(signature);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_signature_similarity(
    pr_visual_bridge_t* bridge,
    const prime_signature_t* sig1,
    const prime_signature_t* sig2,
    float* similarity) {

    if (!bridge || !sig1 || !sig2 || !similarity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_signature_similarity: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    *similarity = prime_sig_jaccard(sig1, sig2);

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Quaternion Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_compute_visual_quaternion(
    pr_visual_bridge_t* bridge,
    nimcp_quaternion_t* quat) {

    if (!bridge || !quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_compute_visual_quaternion: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    /* Start with current quaternion as base */
    *quat = bridge->current_visual_quat;

    /* Compute emotion (x) from color warmth */
    float warmth = compute_color_warmth(&bridge->current_features);
    quat->x = warmth * bridge->config.emotion_weight +
              quat->x * (1.0f - bridge->config.emotion_weight);

    /* Compute salience (y) from attention intensity */
    float attention = compute_attention_intensity(&bridge->current_features);
    quat->y = attention * bridge->config.salience_weight +
              quat->y * (1.0f - bridge->config.salience_weight);

    /* Compute accessibility (z) from novelty if visual cortex connected */
    if (bridge->visual_cortex) {
        /* visual_cortex_compute_novelty requires features - use current features */
        float novelty = visual_cortex_compute_novelty(bridge->visual_cortex,
            bridge->current_features.color_histogram);
        quat->z = novelty * bridge->config.accessibility_weight +
                  quat->z * (1.0f - bridge->config.accessibility_weight);
    }

    /* Apply consolidation decay to w */
    quat->w = quat->w * (1.0f - bridge->config.consolidation_decay);
    if (quat->w < 0.0f) quat->w = 0.0f;

    /* Normalize to unit quaternion */
    *quat = quat_normalize(*quat);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_apply_attention_to_salience(
    pr_visual_bridge_t* bridge,
    const attention_map_t* attention_map,
    nimcp_quaternion_t* quat) {

    if (!bridge || !attention_map || !quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_apply_attention_to_salience: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    /* Compute mean attention from attention map
     * Note: attention_map_t is opaque, so we sample the map at grid points */
    float sum = 0.0f;
    uint32_t count = 0;

    /* Sample at 8x8 grid points to get average attention */
    for (uint32_t y = 0; y < 8; y++) {
        /* Phase 8: Loop progress heartbeat */
        if ((y & 0xFF) == 0 && 8 > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(y + 1) / (float)8);
        }

        for (uint32_t x = 0; x < 8; x++) {
            /* Phase 8: Loop progress heartbeat */
            if ((x & 0xFF) == 0 && 8 > 256) {
                pr_visual_bridge_heartbeat("pr_visual_br_loop",
                                 (float)(x + 1) / (float)8);
            }

            float val = attention_map_get(attention_map, x * 8, y * 8);
            if (val >= 0.0f) {  /* Valid value */
                sum += val;
                count++;
            }
        }
    }

    if (count > 0) {
        float mean_attention = sum / (float)count;

        /* Blend into salience */
        quat->y = mean_attention * bridge->config.salience_weight +
                  quat->y * (1.0f - bridge->config.salience_weight);
    }

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_apply_novelty_to_accessibility(
    pr_visual_bridge_t* bridge,
    float novelty,
    nimcp_quaternion_t* quat) {

    if (!bridge || !quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_apply_novelty_to_accessibility: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    /* Clamp novelty */
    if (novelty < 0.0f) novelty = 0.0f;
    if (novelty > 1.0f) novelty = 1.0f;

    /* Blend into accessibility */
    quat->z = novelty * bridge->config.accessibility_weight +
              quat->z * (1.0f - bridge->config.accessibility_weight);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_apply_warmth_to_emotion(
    pr_visual_bridge_t* bridge,
    float warmth,
    nimcp_quaternion_t* quat) {

    if (!bridge || !quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_apply_warmth_to_emotion: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    /* Clamp warmth */
    if (warmth < 0.0f) warmth = 0.0f;
    if (warmth > 1.0f) warmth = 1.0f;

    /* Blend into emotion */
    quat->x = warmth * bridge->config.emotion_weight +
              quat->x * (1.0f - bridge->config.emotion_weight);

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Memory Encoding Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_encode_to_memory(
    pr_visual_bridge_t* bridge,
    pr_memory_node_t** memory_node) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_encode_to_memory: bridge is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    /* Check phase gating if enabled */
    if (bridge->config.enable_phase_gating && bridge->theta_gamma && *bridge->theta_gamma) {
        if (!pr_visual_bridge_in_encode_phase(bridge)) {
            unlock_bridge(bridge);
            bridge->stats.phase_gated_encodes++;
            return set_error(bridge, PR_VISUAL_ERROR_PHASE_MISMATCH);
        }
    }

    /* Allocate memory node */
    pr_memory_node_t* node = allocate_memory_node(bridge);
    if (!node) {
        unlock_bridge(bridge);
        return set_error(bridge, PR_VISUAL_ERROR_MEMORY_FULL);
    }

    /* Memory node already zeroed by nimcp_calloc in allocate_memory_node */

    /* Copy signature */
    memcpy(&node->signature, &bridge->current_signature, sizeof(prime_signature_t));

    /* Copy quaternion state */
    memcpy(&node->state, &bridge->current_visual_quat, sizeof(nimcp_quaternion_t));

    /* Set initial tier (Z0 - working memory) */
    node->tier = PR_MEMORY_TIER_Z0;
    node->created_time_ms = nimcp_time_now_us() / 1000;  /* Convert us to ms */
    node->last_accessed_ms = node->created_time_ms;
    node->current_strength = 1.0f;
    node->decay_rate = PR_NODE_DECAY_Z0;

    /* Set current visual memory pointer */
    bridge->current_visual_memory = node;

    /* Auto-entangle if enabled */
    if (bridge->config.auto_entangle) {
        uint32_t edges_created = 0;
        pr_visual_bridge_auto_entangle(bridge, 0.3f, &edges_created);
    }

    /* Update statistics */
    bridge->stats.memories_encoded++;
    bridge->stats.current_memory_count = bridge->memory_pool_count;

    /* Return node if requested */
    if (memory_node) {
        *memory_node = node;
    }

    unlock_bridge(bridge);

    NIMCP_LOG_DEBUG("Visual memory encoded: hash=0x%016llx",
                    (unsigned long long)node->signature.hash);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_encode_explicit(
    pr_visual_bridge_t* bridge,
    const prime_signature_t* signature,
    const nimcp_quaternion_t* quat,
    pr_memory_node_t** memory_node) {

    if (!bridge || !signature || !quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_encode_explicit: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    /* Copy to current state */
    memcpy(&bridge->current_signature, signature, sizeof(prime_signature_t));
    memcpy(&bridge->current_visual_quat, quat, sizeof(nimcp_quaternion_t));

    unlock_bridge(bridge);

    /* Use standard encode */
    return pr_visual_bridge_encode_to_memory(bridge, memory_node);
}

/* ============================================================================
 * Memory Retrieval Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_retrieve_similar_visual(
    pr_visual_bridge_t* bridge,
    const prime_signature_t* query_signature,
    const nimcp_quaternion_t* query_quat,
    pr_visual_retrieval_result_t* results,
    uint32_t max_results,
    uint32_t* result_count) {

    if (!bridge || !query_signature || !results || !result_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_retrieve_similar_visual: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    *result_count = 0;

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    /* Check phase gating if enabled */
    if (bridge->config.enable_phase_gating && bridge->theta_gamma && *bridge->theta_gamma) {
        if (!pr_visual_bridge_in_retrieve_phase(bridge)) {
            unlock_bridge(bridge);
            bridge->stats.phase_gated_retrieves++;
            return set_error(bridge, PR_VISUAL_ERROR_PHASE_MISMATCH);
        }
    }

    bridge->stats.retrieval_queries++;

    /* Search through memory pool */
    pr_visual_retrieval_result_t* temp_results = nimcp_calloc(
        bridge->memory_pool_count, sizeof(pr_visual_retrieval_result_t));
    if (!temp_results) {
        unlock_bridge(bridge);
        return set_error(bridge, PR_VISUAL_ERROR_ALLOCATION);
    }

    uint32_t match_count = 0;
    float total_resonance = 0.0f;

    for (uint32_t i = 0; i < bridge->memory_pool_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->memory_pool_count > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)bridge->memory_pool_count);
        }

        pr_memory_node_t* node = bridge->memory_pool[i];
        if (!node) continue;

        /* Compute resonance score components */
        float jaccard = prime_sig_jaccard(query_signature, &node->signature);

        float phase_alignment = 1.0f;
        if (*bridge->theta_gamma) {
            float query_phase = theta_gamma_get_theta_phase(*bridge->theta_gamma);
            /* Use creation time as proxy for encoding phase */
            float node_phase = (float)(node->created_time_ms % 360);
            phase_alignment = 1.0f - fabsf(query_phase - node_phase) / 360.0f;
        }

        float quat_similarity = 1.0f;
        if (query_quat) {
            quat_similarity = quat_dot(*query_quat, node->state);
            if (quat_similarity < 0.0f) quat_similarity = -quat_similarity;
        }

        /* Default kuramoto value - resonance_engine not used in this implementation */
        float kuramoto = 0.5f;
        (void)kuramoto;  /* Suppress unused warning */

        /* Compute weighted resonance using config weights */
        float resonance =
            bridge->config.resonance_config.weight_jaccard * jaccard +
            bridge->config.resonance_config.weight_phase * phase_alignment +
            bridge->config.resonance_config.weight_quaternion * quat_similarity +
            bridge->config.resonance_config.weight_kuramoto * kuramoto;

        if (resonance >= bridge->config.resonance_threshold) {
            temp_results[match_count].memory_node = node;
            temp_results[match_count].resonance_score = resonance;
            temp_results[match_count].jaccard_component = jaccard;
            temp_results[match_count].phase_component = phase_alignment;
            temp_results[match_count].quaternion_component = quat_similarity;
            temp_results[match_count].kuramoto_component = kuramoto;

            /* Compute prediction error */
            temp_results[match_count].prediction_error =
                1.0f - (jaccard * 0.5f + quat_similarity * 0.5f);

            total_resonance += resonance;
            match_count++;
        }
    }

    /* Sort by resonance score (descending) */
    if (match_count > 1) {
        qsort(temp_results, match_count, sizeof(pr_visual_retrieval_result_t),
              compare_retrieval_results);
    }

    /* Copy top results */
    uint32_t copy_count = match_count < max_results ? match_count : max_results;
    memcpy(results, temp_results, copy_count * sizeof(pr_visual_retrieval_result_t));
    *result_count = copy_count;

    /* Update statistics */
    if (copy_count > 0) {
        bridge->stats.successful_retrievals++;
        bridge->stats.avg_resonance_score =
            (bridge->stats.avg_resonance_score * (bridge->stats.successful_retrievals - 1) +
             total_resonance / (match_count > 0 ? match_count : 1)) / bridge->stats.successful_retrievals;
    }

    nimcp_free(temp_results);
    temp_results = NULL;
    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_retrieve_current(
    pr_visual_bridge_t* bridge,
    pr_visual_retrieval_result_t* results,
    uint32_t max_results,
    uint32_t* result_count) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_retrieve_current: bridge is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    return pr_visual_bridge_retrieve_similar_visual(
        bridge,
        &bridge->current_signature,
        &bridge->current_visual_quat,
        results,
        max_results,
        result_count);
}

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_process_frame(
    pr_visual_bridge_t* bridge,
    uint64_t frame_id) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_process_frame: bridge is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    if (!bridge->initialized) {
        return set_error(bridge, PR_VISUAL_ERROR_NOT_INITIALIZED);
    }

    if (!bridge->visual_cortex) {
        return set_error(bridge, PR_VISUAL_ERROR_NO_VISUAL_CORTEX);
    }

    uint64_t start_time = (nimcp_time_monotonic_us() * 1000);

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    /* Step 1: Extract features */
    err = pr_visual_bridge_extract_features(bridge, &bridge->current_features);
    if (err != PR_VISUAL_OK) {
        unlock_bridge(bridge);
        return err;
    }

    /* Step 2: Compute prime signature */
    err = pr_visual_bridge_compute_visual_prime_sig(
        bridge, &bridge->current_features, &bridge->current_signature);
    if (err != PR_VISUAL_OK) {
        unlock_bridge(bridge);
        return err;
    }

    /* Step 3: Compute quaternion */
    err = pr_visual_bridge_compute_visual_quaternion(bridge, &bridge->current_visual_quat);
    if (err != PR_VISUAL_OK) {
        unlock_bridge(bridge);
        return err;
    }

    /* Step 4: Update from FEP if connected and enabled */
    if (bridge->fep_bridge && bridge->config.enable_active_inference) {
        pr_visual_bridge_update_from_fep(bridge);
    }

    /* Update frame counter */
    bridge->stats.frames_processed++;

    /* Update timing statistics */
    uint64_t elapsed = (nimcp_time_monotonic_us() * 1000) - start_time;
    bridge->stats.total_processing_time_ns += elapsed;
    if (elapsed > bridge->stats.max_processing_time_ns) {
        bridge->stats.max_processing_time_ns = elapsed;
    }
    if (elapsed < bridge->stats.min_processing_time_ns) {
        bridge->stats.min_processing_time_ns = elapsed;
    }

    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * FEP Integration Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_update_from_fep(
    pr_visual_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_update_from_fep: bridge is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    if (!bridge->fep_bridge) {
        return set_error(bridge, PR_VISUAL_ERROR_NO_FEP_BRIDGE);
    }

    /* Get prediction error from FEP bridge */
    visual_cortex_fep_state_t fep_state;
    int result = visual_cortex_fep_bridge_get_state(bridge->fep_bridge, &fep_state);
    if (result != 0) {
        return set_error(bridge, PR_VISUAL_ERROR_NO_FEP_BRIDGE);
    }

    float prediction_error = fep_state.current_visual_pe;

    /* Update accessibility based on prediction error */
    /* High PE = novel = high accessibility */
    /* Low PE = familiar = low accessibility (consolidated) */
    bridge->current_visual_quat.z = prediction_error;

    /* If PE is low, increase consolidation */
    if (prediction_error < bridge->config.pe_threshold) {
        bridge->current_visual_quat.w += 0.1f * (1.0f - prediction_error);
        if (bridge->current_visual_quat.w > 1.0f) {
            bridge->current_visual_quat.w = 1.0f;
        }
    }

    /* Update average PE statistic */
    bridge->stats.fep_updates++;
    bridge->stats.avg_prediction_error =
        (bridge->stats.avg_prediction_error * (bridge->stats.fep_updates - 1) +
         prediction_error) / bridge->stats.fep_updates;

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_memory_pe(
    pr_visual_bridge_t* bridge,
    const pr_memory_node_t* memory_node,
    float* prediction_error) {

    if (!bridge || !memory_node || !prediction_error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_get_memory_pe: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    /* Compute PE as difference between current and memory signatures */
    float similarity = prime_sig_jaccard(
        &bridge->current_signature, &memory_node->signature);

    *prediction_error = 1.0f - similarity;

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Entanglement Functions
 * ============================================================================ */

NIMCP_EXPORT entangle_graph_t pr_visual_bridge_get_visual_entanglement(
    pr_visual_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_get_visual_entanglement: bridge is NULL");
        return NULL;
    }
    return bridge->visual_entanglement;
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_entangle_memories(
    pr_visual_bridge_t* bridge,
    pr_memory_node_t* node1,
    pr_memory_node_t* node2,
    entangle_edge_type_t edge_type,
    float strength) {

    if (!bridge || !node1 || !node2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_entangle_memories: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    if (!bridge->visual_entanglement) {
        return set_error(bridge, PR_VISUAL_ERROR_ENTANGLE_FAILED);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    /* Create entanglement edge */
    entangle_edge_t edge = {
        .from_id = node1->node_id,
        .to_id = node2->node_id,
        .type = edge_type,
        .weight = strength,
        .resonance_score = strength,
        .bidirectional = true
    };
    bool success = entangle_add_edge(bridge->visual_entanglement, &edge);

    if (!success) {
        unlock_bridge(bridge);
        return set_error(bridge, PR_VISUAL_ERROR_ENTANGLE_FAILED);
    }

    bridge->stats.entangle_edges_created++;
    bridge->stats.current_edge_count++;

    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_auto_entangle(
    pr_visual_bridge_t* bridge,
    float similarity_threshold,
    uint32_t* edges_created) {

    if (!bridge || !edges_created) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_auto_entangle: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    *edges_created = 0;

    if (!bridge->current_visual_memory) {
        return set_error(bridge, PR_VISUAL_OK);  /* Nothing to entangle */
    }

    pr_memory_node_t* current = bridge->current_visual_memory;

    /* Find similar memories and create edges */
    for (uint32_t i = 0; i < bridge->memory_pool_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->memory_pool_count > 256) {
            pr_visual_bridge_heartbeat("pr_visual_br_loop",
                             (float)(i + 1) / (float)bridge->memory_pool_count);
        }

        pr_memory_node_t* other = bridge->memory_pool[i];
        if (!other || other == current) continue;

        float similarity = prime_sig_jaccard(
            &current->signature, &other->signature);

        if (similarity >= similarity_threshold) {
            /* Determine edge type based on temporal proximity */
            entangle_edge_type_t edge_type = ENTANGLE_EDGE_ASSOCIATIVE;

            uint64_t time_diff = current->created_time_ms > other->created_time_ms ?
                current->created_time_ms - other->created_time_ms :
                other->created_time_ms - current->created_time_ms;

            /* If within 1 second, mark as temporal */
            if (time_diff < 1000ULL) {  /* created_time_ms is in ms */
                edge_type = ENTANGLE_EDGE_TEMPORAL;
            }

            pr_visual_bridge_error_t err = pr_visual_bridge_entangle_memories(
                bridge, current, other, edge_type, similarity);

            if (err == PR_VISUAL_OK) {
                (*edges_created)++;
            }
        }
    }

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Theta-Gamma Functions
 * ============================================================================ */

NIMCP_EXPORT bool pr_visual_bridge_in_encode_phase(pr_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_in_encode_phase: bridge is NULL");
        return true;  /* Default to allow */
    }
    if (!bridge->theta_gamma || !*bridge->theta_gamma) return true;  /* Default to allow */

    float phase = theta_gamma_get_theta_phase(*bridge->theta_gamma);
    /* Encode phase: 0-90 degrees */
    return (phase >= 0.0f && phase < 90.0f);
}

NIMCP_EXPORT bool pr_visual_bridge_in_retrieve_phase(pr_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_in_retrieve_phase: bridge is NULL");
        return true;  /* Default to allow */
    }
    if (!bridge->theta_gamma || !*bridge->theta_gamma) return true;  /* Default to allow */

    float phase = theta_gamma_get_theta_phase(*bridge->theta_gamma);
    /* Retrieve phase: 180-270 degrees */
    return (phase >= 180.0f && phase < 270.0f);
}

NIMCP_EXPORT float pr_visual_bridge_get_theta_phase(pr_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_get_theta_phase: bridge is NULL");
        return 0.0f;
    }
    if (!bridge->theta_gamma || !*bridge->theta_gamma) return 0.0f;

    return theta_gamma_get_theta_phase(*bridge->theta_gamma);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_set_theta_phase(
    pr_visual_bridge_t* bridge,
    float phase_degrees) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_set_theta_phase: bridge is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    if (!bridge->theta_gamma || !*bridge->theta_gamma) {
        return set_error(bridge, PR_VISUAL_OK);  /* No-op if no theta-gamma */
    }

    /* Normalize phase to 0-360 using theta_gamma_wrap_phase */
    float wrapped_phase = theta_gamma_wrap_phase(phase_degrees);

    /* Note: Phase setting is controlled by theta_gamma_update() time progression.
     * Direct phase manipulation is not supported - this is read-only.
     * Return success as the request is acknowledged but not acted upon. */
    (void)wrapped_phase;

    return set_error(bridge, PR_VISUAL_OK);
}

/* ============================================================================
 * Statistics and Diagnostics Functions
 * ============================================================================ */

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_stats(
    pr_visual_bridge_t* bridge,
    pr_visual_bridge_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_get_stats: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    memcpy(stats, &bridge->stats, sizeof(pr_visual_bridge_stats_t));

    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_reset_stats(
    pr_visual_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_reset_stats: bridge is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    memset(&bridge->stats, 0, sizeof(pr_visual_bridge_stats_t));
    bridge->stats.min_processing_time_ns = UINT64_MAX;
    bridge->stats.current_memory_count = bridge->memory_pool_count;

    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_last_error(
    pr_visual_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_get_last_error: bridge is NULL");
        return g_last_error;
    }
    return bridge->last_error;
}

NIMCP_EXPORT const char* pr_visual_bridge_error_string(
    pr_visual_bridge_error_t error) {

    if (error == PR_VISUAL_ERROR_UNKNOWN) {
        return "Unknown error";
    }

    int index = -error;
    if (index < 0 || index >= (int)(sizeof(ERROR_STRINGS) / sizeof(ERROR_STRINGS[0]))) {
        return "Invalid error code";
    }

    return ERROR_STRINGS[index];
}

NIMCP_EXPORT bool pr_visual_bridge_is_connected(pr_visual_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return (bridge->visual_cortex != NULL &&
            bridge->fep_bridge != NULL &&
            bridge->retina != NULL);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_current_quaternion(
    pr_visual_bridge_t* bridge,
    nimcp_quaternion_t* quat) {

    if (!bridge || !quat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_get_current_quaternion: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    memcpy(quat, &bridge->current_visual_quat, sizeof(nimcp_quaternion_t));

    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

NIMCP_EXPORT pr_visual_bridge_error_t pr_visual_bridge_get_current_signature(
    pr_visual_bridge_t* bridge,
    prime_signature_t* signature) {

    if (!bridge || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_visual_bridge_get_current_signature: required parameter is NULL");
        return set_error(bridge, PR_VISUAL_ERROR_NULL_PARAM);
    }

    pr_visual_bridge_error_t err = lock_bridge(bridge);
    if (err != PR_VISUAL_OK) {
        return set_error(bridge, err);
    }

    memcpy(signature, &bridge->current_signature, sizeof(prime_signature_t));

    unlock_bridge(bridge);

    return set_error(bridge, PR_VISUAL_OK);
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_visual_bridge_set_instance_health_agent(
    pr_visual_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_visual_bridge_training_begin(pr_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_visual_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_visual_bridge_heartbeat_instance(bridge->health_agent, "pr_visual_bridge_training_begin", 0.0f);
    return 0;
}

int pr_visual_bridge_training_end(pr_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_visual_bridge_training_end: NULL argument");
        return -1;
    }
    pr_visual_bridge_heartbeat_instance(bridge->health_agent, "pr_visual_bridge_training_end", 1.0f);
    return 0;
}

int pr_visual_bridge_training_step(pr_visual_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_visual_bridge_training_step: NULL argument");
        return -1;
    }
    pr_visual_bridge_heartbeat_instance(bridge->health_agent, "pr_visual_bridge_training_step", progress);
    return 0;
}
