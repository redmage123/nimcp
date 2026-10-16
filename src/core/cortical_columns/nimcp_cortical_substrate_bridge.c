/**
 * @file nimcp_cortical_substrate_bridge.c
 * @brief Cortical Column-Neural Substrate Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between metabolic/thermal substrate and cortical columns
 * WHY:  Cortical columns are metabolically demanding structures. Layer-specific processing,
 *       lateral competition, hierarchical depth, and sparse coding all require sustained
 *       ATP and are temperature-sensitive. Substrate state modulates cortical capabilities.
 * HOW:  Monitors substrate (ATP, temperature, metabolic stress), computes cortical
 *       effects (column fidelity, layer gains, competition efficiency, sparsity, depth),
 *       and applies modulation to cortical systems.
 *
 * BIOLOGICAL BASIS:
 * - Cortical columns maintain high metabolic activity for sustained processing
 * - Layer-specific circuits have different Q10 temperature sensitivities
 * - ATP depletion reduces lateral competition and column selectivity
 * - Metabolic stress affects sparse coding efficiency
 * - Hyperthermia (fever) impairs hierarchical processing depth
 */

#include "core/cortical_columns/nimcp_cortical_substrate_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_substrate_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute Q10-based temperature scaling factor
 * WHAT: Calculate multiplicative factor for rate constants based on temperature
 * WHY:  Biological processes have exponential temperature dependence
 * HOW:  factor = Q10^((T - T_ref) / 10)
 *
 * BIOLOGICAL: Q10 rule from Arrhenius equation
 * - Q10 = 2.5: rate increases 2.5x per 10°C
 * - Hyperthermia accelerates processes but impairs precision
 * - Hypothermia slows processes
 */
static float compute_q10_factor(float current_temp, float reference_temp, float q10)
{
    if (q10 <= 0.0f) {
        return 1.0f;
    }

    float temp_diff = current_temp - reference_temp;
    float exponent = temp_diff / 10.0f;
    return powf(q10, exponent);
}

/**
 * @brief Update running average with exponential moving average
 * WHAT: Compute EMA of a value
 * WHY:  Smooth statistics over time
 * HOW:  new_avg = (1-alpha)*current + alpha*new_value
 */
static float update_running_avg(float current, float new_value, float alpha)
{
    return (1.0f - alpha) * current + alpha * new_value;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

void cortical_substrate_default_config(cortical_substrate_config_t* config)
{
    if (!config) {
        return;
    }

    /* Enable all modulations by default */
    config->enable_column_fidelity_modulation = true;
    config->enable_layer_gain_modulation = true;
    config->enable_competition_modulation = true;
    config->enable_sparsity_modulation = true;
    config->enable_hierarchical_modulation = true;
    config->enable_bio_async = false;

    /* Moderate sensitivity (1.0 = biological measurements) */
    config->atp_sensitivity = 1.0f;
    config->temperature_sensitivity = 1.0f;
}

cortical_substrate_bridge_t* cortical_substrate_bridge_create(
    const cortical_substrate_config_t* config,
    neural_substrate_t* substrate)
{
    /* Guard: Validate inputs */
    if (!substrate) {
        NIMCP_LOGGING_ERROR("Cannot create cortical substrate bridge: NULL substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;
    }

    /* Allocate bridge structure */
    cortical_substrate_bridge_t* bridge =
        (cortical_substrate_bridge_t*)nimcp_malloc(sizeof(cortical_substrate_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate cortical substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cortical substrate bridge");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(cortical_substrate_bridge_t));

    /* Store references */
    bridge->substrate = substrate;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        cortical_substrate_default_config(&bridge->config);
    }

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, 0, "cortical_substrate") != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base for cortical substrate bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_substrate_bridge_create: bridge_base_init failed");
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for cortical substrate bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_substrate_bridge_create: mutex is NULL");
        return NULL;
    }

    /* Initialize effects to optimal (no impairment) */
    bridge->effects.column_fidelity = 1.0f;
    for (int i = 0; i < CORTICAL_SUBSTRATE_NUM_LAYERS; i++) {
        bridge->effects.layer_gain[i] = 1.0f;
    }
    bridge->effects.competition_efficiency = 1.0f;
    bridge->effects.sparsity_modulation = 1.0f;
    bridge->effects.hierarchical_depth = 1.0f;
    bridge->effects.is_impaired = false;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(cortical_substrate_stats_t));
    bridge->stats.min_fidelity_observed = 1.0f;
    bridge->stats.max_fidelity_observed = 1.0f;

    NIMCP_LOGGING_INFO("Created cortical substrate bridge");

    return bridge;
}

void cortical_substrate_bridge_destroy(cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        cortical_substrate_disconnect_bio_async(bridge);
    }

    /* Destroy bridge base (cleans up mutex) */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed cortical substrate bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int cortical_substrate_connect_columns(
    cortical_substrate_bridge_t* bridge,
    void* columns)
{
    if (!bridge || !columns) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->columns = columns;
    return 0;
}

int cortical_substrate_connect_laminar(
    cortical_substrate_bridge_t* bridge,
    void* laminar)
{
    if (!bridge || !laminar) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->laminar = laminar;
    return 0;
}

int cortical_substrate_connect_hierarchy(
    cortical_substrate_bridge_t* bridge,
    void* hierarchy)
{
    if (!bridge || !hierarchy) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->hierarchy = hierarchy;
    return 0;
}

int cortical_substrate_connect_sparse(
    cortical_substrate_bridge_t* bridge,
    void* sparse)
{
    if (!bridge || !sparse) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    bridge->sparse = sparse;
    return 0;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int cortical_substrate_connect_bio_async(cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_CORTICAL,
        .module_name = "cortical_substrate_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected cortical substrate bridge to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    return 0;  /* Not an error - bio-async is optional */
}

int cortical_substrate_disconnect_bio_async(cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected cortical substrate bridge from bio-async router");

    return 0;
}

bool cortical_substrate_is_bio_async_connected(const cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Update API - Core Substrate-Cortical Integration
 * ============================================================================ */

int cortical_substrate_update(cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get substrate state using proper API */
    substrate_metabolic_state_t metabolic;
    substrate_physical_state_t physical;

    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_LOGGING_ERROR("Failed to get metabolic state from substrate");
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        NIMCP_LOGGING_ERROR("Failed to get physical state from substrate");
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    float atp = metabolic.atp_level;
    float temperature = physical.temperature;
    float metabolic_capacity = metabolic.metabolic_capacity;
    float physical_capacity = physical.physical_capacity;

    /* ========================================================================
     * BIOLOGICAL EFFECT 1: Column Fidelity
     *
     * WHAT: Ability of columns to maintain precise feature selectivity
     * WHY:  Column fidelity requires sustained ATP for maintaining tuning
     * HOW:  column_fidelity = clamp(atp * metabolic_capacity * sensitivity, 0.2, 1.0)
     *
     * BIOLOGICAL BASIS:
     * - Cortical columns require high ATP for precise feature detection
     * - ATP depletion → reduced selectivity → broader tuning
     * - Floor of 0.2 represents minimal column function
     * ======================================================================== */

    if (bridge->config.enable_column_fidelity_modulation) {
        float atp_factor = atp * metabolic_capacity * bridge->config.atp_sensitivity;
        bridge->effects.column_fidelity = nimcp_clampf(atp_factor, 0.2f, 1.0f);
    } else {
        bridge->effects.column_fidelity = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 2: Layer Gain
     *
     * WHAT: Layer-specific processing gain modulated by temperature
     * WHY:  Different cortical layers have different Q10 sensitivities
     * HOW:  layer_gain[i] = compute_q10_factor(temp, normal, Q10_layer[i])
     *
     * BIOLOGICAL BASIS:
     * - Layer I (molecular): Q10 = 2.0 (feedback modulation)
     * - Layer II/III (lateral): Q10 = 2.8 (integration, highest)
     * - Layer IV (thalamic input): Q10 = 2.3 (feedforward)
     * - Layer V (output): Q10 = 2.5 (action commands)
     * - Layer VI (feedback): Q10 = 2.4 (prediction)
     * ======================================================================== */

    if (bridge->config.enable_layer_gain_modulation) {
        /* Q10 coefficients for each layer */
        float layer_q10[CORTICAL_SUBSTRATE_NUM_LAYERS] = {
            CORTICAL_SUBSTRATE_Q10_LAYER_I,
            CORTICAL_SUBSTRATE_Q10_LAYER_II_III,
            CORTICAL_SUBSTRATE_Q10_LAYER_IV,
            CORTICAL_SUBSTRATE_Q10_LAYER_V,
            CORTICAL_SUBSTRATE_Q10_LAYER_VI
        };

        float reference_temp = 37.0f;  /* Normal brain temperature */
        float hyperthermia_threshold = 39.0f;

        for (int i = 0; i < CORTICAL_SUBSTRATE_NUM_LAYERS; i++) {
            float temp_factor = compute_q10_factor(
                temperature,
                reference_temp,
                layer_q10[i]
            );

            /* Scale by temperature sensitivity */
            temp_factor = 1.0f + (temp_factor - 1.0f) * bridge->config.temperature_sensitivity;

            /* Hyperthermia impairs layer processing */
            if (temperature > hyperthermia_threshold) {
                float hyperthermia_penalty =
                    (temperature - hyperthermia_threshold) / 5.0f;
                temp_factor *= (1.0f - nimcp_clampf(hyperthermia_penalty, 0.0f, 0.5f));
            }

            bridge->effects.layer_gain[i] = nimcp_clampf(temp_factor, 0.3f, 1.5f);
        }
    } else {
        for (int i = 0; i < CORTICAL_SUBSTRATE_NUM_LAYERS; i++) {
            bridge->effects.layer_gain[i] = 1.0f;
        }
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 3: Competition Efficiency
     *
     * WHAT: Effectiveness of lateral competition between columns
     * WHY:  Competition requires ATP for sustained inhibition
     * HOW:  competition_efficiency = atp > 0.5 ? 1.0 : atp * 2.0
     *
     * BIOLOGICAL BASIS:
     * - Lateral inhibition requires sustained GABAergic activity
     * - ATP depletion → weakened competition → reduced selectivity
     * - Binary-like transition at 50% ATP models sharp competition threshold
     * ======================================================================== */

    if (bridge->config.enable_competition_modulation) {
        if (atp > 0.5f) {
            bridge->effects.competition_efficiency = 1.0f;
        } else {
            bridge->effects.competition_efficiency = atp * 2.0f;
        }
        bridge->effects.competition_efficiency =
            nimcp_clampf(bridge->effects.competition_efficiency, 0.0f, 1.0f);
    } else {
        bridge->effects.competition_efficiency = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 4: Sparsity Modulation
     *
     * WHAT: Modulation of sparse coding efficiency
     * WHY:  Metabolic stress increases coding density (energy conservation)
     * HOW:  sparsity_modulation = 1.0 + (1.0 - metabolic_capacity) * 0.5
     *
     * BIOLOGICAL BASIS:
     * - Sparse codes are metabolically efficient
     * - Under stress, system recruits more neurons (denser coding)
     * - Factor of 1.5x max models recruitment of reserve neurons
     * ======================================================================== */

    if (bridge->config.enable_sparsity_modulation) {
        float stress_factor = 1.0f - metabolic_capacity;
        bridge->effects.sparsity_modulation = 1.0f + stress_factor * 0.5f;
        bridge->effects.sparsity_modulation =
            nimcp_clampf(bridge->effects.sparsity_modulation, 0.5f, 2.0f);
    } else {
        bridge->effects.sparsity_modulation = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL EFFECT 5: Hierarchical Depth
     *
     * WHAT: Capacity for hierarchical processing depth
     * WHY:  Deep hierarchical processing requires sustained substrate health
     * HOW:  hierarchical_depth = physical_capacity * (1.0 - hyperthermia_penalty)
     *
     * BIOLOGICAL BASIS:
     * - Hierarchical processing involves multiple layers and regions
     * - Physical substrate degradation limits processing depth
     * - Hyperthermia particularly impairs higher-order processing
     * ======================================================================== */

    if (bridge->config.enable_hierarchical_modulation) {
        float hyperthermia_penalty = 0.0f;
        float hyperthermia_threshold = 39.0f;

        if (temperature > hyperthermia_threshold) {
            hyperthermia_penalty =
                (temperature - hyperthermia_threshold) / 10.0f;
            hyperthermia_penalty = nimcp_clampf(hyperthermia_penalty, 0.0f, 0.8f);
        }

        bridge->effects.hierarchical_depth =
            physical_capacity * (1.0f - hyperthermia_penalty);
        bridge->effects.hierarchical_depth =
            nimcp_clampf(bridge->effects.hierarchical_depth, 0.2f, 1.0f);
    } else {
        bridge->effects.hierarchical_depth = 1.0f;
    }

    /* ========================================================================
     * IMPAIRMENT DETECTION
     *
     * WHAT: Overall cortical impairment flag
     * WHY:  Quick check for critical cortical degradation
     * HOW:  is_impaired = (column_fidelity < 0.5 || competition_efficiency < 0.3)
     *
     * BIOLOGICAL BASIS:
     * - Critical column dysfunction occurs at ~50% fidelity
     * - Competition breakdown significant below 30%
     * ======================================================================== */

    bool was_impaired = bridge->effects.is_impaired;
    bridge->effects.is_impaired = (bridge->effects.column_fidelity < 0.5f ||
                                    bridge->effects.competition_efficiency < 0.3f);

    /* Track impairment events (transitions from normal to impaired) */
    if (bridge->effects.is_impaired && !was_impaired) {
        bridge->stats.impairment_events++;
        NIMCP_LOGGING_WARN("Cortical processing became impaired (fidelity: %.2f, competition: %.2f)",
                          bridge->effects.column_fidelity,
                          bridge->effects.competition_efficiency);
    }

    /* Track competition downgrades */
    if (bridge->effects.competition_efficiency < 0.5f) {
        bridge->stats.competition_downgrades++;
    }

    /* ========================================================================
     * UPDATE STATISTICS
     * ======================================================================== */

    bridge->stats.update_count++;

    /* Running averages (exponential moving average with alpha = 0.01) */
    float alpha = 0.01f;
    float new_fidelity = update_running_avg(bridge->stats.avg_column_fidelity,
                          bridge->effects.column_fidelity, alpha);
    if (isfinite(new_fidelity)) bridge->stats.avg_column_fidelity = new_fidelity;

    float new_comp = update_running_avg(bridge->stats.avg_competition_efficiency,
                          bridge->effects.competition_efficiency, alpha);
    if (isfinite(new_comp)) bridge->stats.avg_competition_efficiency = new_comp;

    float new_sparsity = update_running_avg(bridge->stats.avg_sparsity_modulation,
                          bridge->effects.sparsity_modulation, alpha);
    if (isfinite(new_sparsity)) bridge->stats.avg_sparsity_modulation = new_sparsity;

    float new_depth = update_running_avg(bridge->stats.avg_hierarchical_depth,
                          bridge->effects.hierarchical_depth, alpha);
    if (isfinite(new_depth)) bridge->stats.avg_hierarchical_depth = new_depth;

    /* Track min/max fidelity */
    if (bridge->effects.column_fidelity < bridge->stats.min_fidelity_observed) {
        bridge->stats.min_fidelity_observed = bridge->effects.column_fidelity;
    }
    if (bridge->effects.column_fidelity > bridge->stats.max_fidelity_observed) {
        bridge->stats.max_fidelity_observed = bridge->effects.column_fidelity;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float cortical_substrate_get_column_fidelity(const cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    return bridge->effects.column_fidelity;
}

float cortical_substrate_get_layer_gain(
    const cortical_substrate_bridge_t* bridge,
    int layer_index)
{
    if (!bridge || layer_index < 0 || layer_index >= CORTICAL_SUBSTRATE_NUM_LAYERS) {
        return -1.0f;
    }
    return bridge->effects.layer_gain[layer_index];
}

float cortical_substrate_get_competition_efficiency(const cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    return bridge->effects.competition_efficiency;
}

float cortical_substrate_get_sparsity_modulation(const cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    return bridge->effects.sparsity_modulation;
}

float cortical_substrate_get_hierarchical_depth(const cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return -1.0f;
    }
    return bridge->effects.hierarchical_depth;
}

int cortical_substrate_get_effects(
    const cortical_substrate_bridge_t* bridge,
    cortical_substrate_effects_t* effects)
{
    if (!bridge || !effects) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    *effects = bridge->effects;
    return 0;
}

bool cortical_substrate_is_impaired(const cortical_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->effects.is_impaired;
}

int cortical_substrate_get_stats(
    const cortical_substrate_bridge_t* bridge,
    cortical_substrate_stats_t* stats)
{
    if (!bridge || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    return 0;
}
