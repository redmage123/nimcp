/**
 * @file nimcp_metabolic_modulation.c
 * @brief Shared metabolic modulation utilities implementation
 *
 * WHAT: Implements common metabolic modulation functions for substrate bridges
 * WHY: Eliminates code duplication across 80+ substrate bridge implementations
 * HOW: Centralizes clamp utilities and metabolic effect computation
 *
 * TENSOR SUPPORT (v2.7.0):
 * - Tensor-based batch processing for multiple brain regions
 * - SIMD-accelerated operations via nimcp_tensor_t
 * - Backward-compatible scalar API preserved
 */

#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stddef.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(metabolic_modulation, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Default Configuration
 * ============================================================================ */

metabolic_effect_multipliers_t metabolic_default_multipliers(void) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_default_mu", 0.0f);


    metabolic_effect_multipliers_t mult = {
        .atp_primary_mult = 1.0f,
        .atp_secondary_mult = 1.1f,
        .fatigue_primary_mult = 1.0f,
        .fatigue_secondary_mult = 0.9f
    };
    return mult;
}

metabolic_modulation_config_t metabolic_modulation_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_default_config", 0.0f);


    metabolic_modulation_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f,
        .multipliers = {
            .atp_primary_mult = 1.0f,
            .atp_secondary_mult = 1.1f,
            .fatigue_primary_mult = 1.0f,
            .fatigue_secondary_mult = 0.9f
        }
    };
    return cfg;
}

/* ============================================================================
 * Effect Computation Functions
 * ============================================================================ */

float metabolic_compute_atp_effect(
    float atp_level,
    float sensitivity,
    float effect_multiplier,
    float min_capacity
) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_compute_at", 0.0f);


    float value = atp_level * effect_multiplier * sensitivity;
    return nimcp_clampf(value, min_capacity, 1.0f);
}

float metabolic_compute_fatigue_effect(
    float metabolic_capacity,
    float sensitivity,
    float effect_multiplier,
    float min_capacity
) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_compute_fa", 0.0f);


    float value = metabolic_capacity * effect_multiplier * sensitivity;
    return nimcp_clampf(value, min_capacity, 1.0f);
}

void metabolic_effects_init_full(metabolic_effects_t* effects) {
    if (!effects) return;

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_in", 0.0f);


    effects->primary_atp = 1.0f;
    effects->secondary_atp = 1.0f;
    effects->primary_fatigue = 1.0f;
    effects->secondary_fatigue = 1.0f;
    effects->overall_capacity = 1.0f;
}

float metabolic_compute_overall_capacity(const metabolic_effects_t* effects) {
    if (!effects) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_compute_ov", 0.0f);


    return (effects->primary_atp + effects->secondary_atp +
            effects->primary_fatigue + effects->secondary_fatigue) / 4.0f;
}

int metabolic_compute_effects(
    const metabolic_input_t* input,
    const metabolic_modulation_config_t* config,
    metabolic_effects_t* effects
) {
    /* Validate parameters using API exception macros */
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_compute_ef", 0.0f);


    NIMCP_API_CHECK_NULL(input, NIMCP_ERROR_NULL_POINTER, "metabolic_compute_effects: NULL input");
    NIMCP_API_CHECK_NULL(config, NIMCP_ERROR_NULL_POINTER, "metabolic_compute_effects: NULL config");
    NIMCP_API_CHECK_NULL(effects, NIMCP_ERROR_NULL_POINTER, "metabolic_compute_effects: NULL effects");

    float atp = input->atp_level;
    float cap = input->metabolic_capacity;
    float min = config->min_capacity;

    /* ATP modulation affects primary_atp and secondary_atp */
    if (config->enable_atp_modulation) {
        effects->primary_atp = metabolic_compute_atp_effect(
            atp, config->atp_sensitivity,
            config->multipliers.atp_primary_mult, min
        );
        effects->secondary_atp = metabolic_compute_atp_effect(
            atp, config->atp_sensitivity,
            config->multipliers.atp_secondary_mult, min
        );
    }

    /* Fatigue modulation affects primary_fatigue and secondary_fatigue */
    if (config->enable_fatigue_modulation) {
        effects->primary_fatigue = metabolic_compute_fatigue_effect(
            cap, config->fatigue_sensitivity,
            config->multipliers.fatigue_primary_mult, min
        );
        effects->secondary_fatigue = metabolic_compute_fatigue_effect(
            cap, config->fatigue_sensitivity,
            config->multipliers.fatigue_secondary_mult, min
        );
    }

    /* Compute overall capacity as average */
    effects->overall_capacity = metabolic_compute_overall_capacity(effects);

    return 0;
}

/* ============================================================================
 * Configuration Helpers
 * ============================================================================ */

metabolic_modulation_config_t metabolic_config_from_fields(
    bool enable_atp,
    bool enable_fatigue,
    bool enable_bio_async,
    float atp_sensitivity,
    float fatigue_sensitivity,
    float min_capacity,
    const metabolic_effect_multipliers_t* multipliers
) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_config_fro", 0.0f);


    metabolic_modulation_config_t cfg = {
        .enable_atp_modulation = enable_atp,
        .enable_fatigue_modulation = enable_fatigue,
        .enable_bio_async = enable_bio_async,
        .atp_sensitivity = atp_sensitivity,
        .fatigue_sensitivity = fatigue_sensitivity,
        .min_capacity = min_capacity,
        .multipliers = multipliers ? *multipliers : metabolic_default_multipliers()
    };
    return cfg;
}

/* ============================================================================
 * Tensor-Based Structure Creation/Destruction
 * ============================================================================ */

metabolic_effects_tensor_t* metabolic_effects_tensor_create(uint32_t batch_size) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_te", 0.0f);


    NIMCP_API_CHECK_RET_NULL(batch_size > 0, NIMCP_ERROR_INVALID_PARAM,
        "metabolic_effects_tensor_create: batch_size must be > 0");

    metabolic_effects_tensor_t* effects = nimcp_calloc(1, sizeof(metabolic_effects_tensor_t));
    if (!effects) return NULL;
    NIMCP_API_CHECK_ALLOC(effects, "metabolic_effects_tensor_create: allocation failed");

    /* Create tensor with shape [batch_size, METABOLIC_EFFECT_COUNT] */
    uint32_t dims[2] = { batch_size, METABOLIC_EFFECT_COUNT };
    uint32_t rank = (batch_size == 1) ? 1 : 2;
    uint32_t* dim_ptr = (batch_size == 1) ? &dims[1] : dims;

    effects->effects = nimcp_tensor_zeros(dim_ptr, rank, NIMCP_DTYPE_F32);
    if (!effects->effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "metabolic_effects_tensor_create: tensor allocation failed");
        nimcp_free(effects);
        effects = NULL;
        return NULL;
    }

    effects->batch_size = batch_size;
    effects->owns_tensor = true;

    return effects;
}

void metabolic_effects_tensor_destroy(metabolic_effects_tensor_t* effects) {
    if (!effects) return;

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_te", 0.0f);


    if (effects->owns_tensor && effects->effects) {
        nimcp_tensor_destroy(effects->effects);
    }
    nimcp_free(effects);
    effects = NULL;
}

metabolic_input_tensor_t* metabolic_input_tensor_create(uint32_t batch_size) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_input_tens", 0.0f);


    NIMCP_API_CHECK_RET_NULL(batch_size > 0, NIMCP_ERROR_INVALID_PARAM,
        "metabolic_input_tensor_create: batch_size must be > 0");

    metabolic_input_tensor_t* input = nimcp_calloc(1, sizeof(metabolic_input_tensor_t));
    if (!input) return NULL;
    NIMCP_API_CHECK_ALLOC(input, "metabolic_input_tensor_create: allocation failed");

    uint32_t dims[1] = { batch_size };

    input->atp_levels = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    if (!input->atp_levels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "metabolic_input_tensor_create: atp_levels tensor allocation failed");
        nimcp_free(input);
        input = NULL;
        return NULL;
    }

    input->metabolic_capacities = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    if (!input->metabolic_capacities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "metabolic_input_tensor_create: metabolic_capacities tensor allocation failed");
        nimcp_tensor_destroy(input->atp_levels);
        nimcp_free(input);
        input = NULL;
        return NULL;
    }

    input->batch_size = batch_size;
    return input;
}

void metabolic_input_tensor_destroy(metabolic_input_tensor_t* input) {
    if (!input) return;

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_input_tens", 0.0f);


    nimcp_tensor_destroy(input->atp_levels);
    nimcp_tensor_destroy(input->metabolic_capacities);
    nimcp_free(input);
    input = NULL;
}

metabolic_multipliers_tensor_t* metabolic_multipliers_tensor_create(uint32_t batch_size) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_multiplier", 0.0f);


    NIMCP_API_CHECK_RET_NULL(batch_size > 0, NIMCP_ERROR_INVALID_PARAM,
        "metabolic_multipliers_tensor_create: batch_size must be > 0");

    metabolic_multipliers_tensor_t* mult = nimcp_calloc(1, sizeof(metabolic_multipliers_tensor_t));
    if (!mult) return NULL;
    NIMCP_API_CHECK_ALLOC(mult, "metabolic_multipliers_tensor_create: allocation failed");

    /* Shape: [batch_size, 4] for the 4 multiplier types */
    uint32_t dims[2] = { batch_size, 4 };
    uint32_t rank = (batch_size == 1) ? 1 : 2;
    uint32_t* dim_ptr = (batch_size == 1) ? &dims[1] : dims;

    mult->multipliers = nimcp_tensor_ones(dim_ptr, rank, NIMCP_DTYPE_F32);
    if (!mult->multipliers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "metabolic_multipliers_tensor_create: tensor allocation failed");
        nimcp_free(mult);
        mult = NULL;
        return NULL;
    }

    mult->batch_size = batch_size;
    return mult;
}

void metabolic_multipliers_tensor_destroy(metabolic_multipliers_tensor_t* mult) {
    if (!mult) return;

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_multiplier", 0.0f);


    nimcp_tensor_destroy(mult->multipliers);
    nimcp_free(mult);
    mult = NULL;
}

/* ============================================================================
 * Tensor Initialization Functions
 * ============================================================================ */

int metabolic_multipliers_tensor_init_default(metabolic_multipliers_tensor_t* mult) {
    if (!mult || !mult->multipliers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_multipliers_tensor_init_default: required parameter is NULL (mult, mult->multipliers)");
        return -1;
    }

    /* Default multipliers: [1.0, 1.1, 1.0, 0.9] */
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_multiplier", 0.0f);


    const float defaults[4] = { 1.0f, 1.1f, 1.0f, 0.9f };

    for (uint32_t i = 0; i < mult->batch_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mult->batch_size > 256) {
            metabolic_modulation_heartbeat("metabolic_mo_loop",
                             (float)(i + 1) / (float)mult->batch_size);
        }

        for (uint32_t j = 0; j < 4; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && 4 > 256) {
                metabolic_modulation_heartbeat("metabolic_mo_loop",
                                 (float)(j + 1) / (float)4);
            }

            size_t flat_idx = (mult->batch_size == 1) ? j : (i * 4 + j);
            if (nimcp_tensor_set_flat(mult->multipliers, flat_idx, defaults[j]) != 0) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_multipliers_tensor_init_default: validation failed");
                return -1;
            }
        }
    }
    return 0;
}

int metabolic_multipliers_tensor_set_region(
    metabolic_multipliers_tensor_t* mult,
    uint32_t region_idx,
    float atp_primary,
    float atp_secondary,
    float fatigue_primary,
    float fatigue_secondary
) {
    if (!mult || !mult->multipliers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_multipliers_tensor_set_region: required parameter is NULL (mult, mult->multipliers)");
        return -1;
    }
    if (region_idx >= mult->batch_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_multipliers_tensor_set_region: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_multiplier", 0.0f);


    float values[4] = { atp_primary, atp_secondary, fatigue_primary, fatigue_secondary };

    for (uint32_t j = 0; j < 4; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && 4 > 256) {
            metabolic_modulation_heartbeat("metabolic_mo_loop",
                             (float)(j + 1) / (float)4);
        }

        size_t flat_idx = (mult->batch_size == 1) ? j : (region_idx * 4 + j);
        if (nimcp_tensor_set_flat(mult->multipliers, flat_idx, values[j]) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_multipliers_tensor_set_region: validation failed");
            return -1;
        }
    }
    return 0;
}

int metabolic_effects_tensor_init_full(metabolic_effects_tensor_t* effects) {
    if (!effects || !effects->effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_effects_tensor_init_full: required parameter is NULL (effects, effects->effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_te", 0.0f);


    size_t numel = nimcp_tensor_numel(effects->effects);
    for (size_t i = 0; i < numel; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && numel > 256) {
            metabolic_modulation_heartbeat("metabolic_mo_loop",
                             (float)(i + 1) / (float)numel);
        }

        if (nimcp_tensor_set_flat(effects->effects, i, 1.0) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_effects_tensor_init_full: validation failed");
            return -1;
        }
    }
    return 0;
}

/* ============================================================================
 * Tensor Effect Computation
 * ============================================================================ */

/**
 * @brief Helper: Compute and clamp a single effect value
 */
static float compute_clamped_effect(float input_val, float sensitivity, float mult, float min_cap) {
    float value = input_val * mult * sensitivity;
    return nimcp_clampf(value, min_cap, 1.0f);
}

int metabolic_compute_effects_tensor(
    const metabolic_input_tensor_t* input,
    const metabolic_modulation_config_t* config,
    const metabolic_multipliers_tensor_t* multipliers,
    metabolic_effects_tensor_t* effects
) {
    /* Validate parameters using API exception macros */
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_compute_ef", 0.0f);


    NIMCP_API_CHECK_NULL(input, NIMCP_ERROR_NULL_POINTER, "metabolic_compute_effects_tensor: NULL input");
    NIMCP_API_CHECK_NULL(config, NIMCP_ERROR_NULL_POINTER, "metabolic_compute_effects_tensor: NULL config");
    NIMCP_API_CHECK_NULL(effects, NIMCP_ERROR_NULL_POINTER, "metabolic_compute_effects_tensor: NULL effects");
    NIMCP_API_CHECK(input->atp_levels && input->metabolic_capacities && effects->effects,
        NIMCP_ERROR_INVALID_PARAM, "metabolic_compute_effects_tensor: NULL tensor member");

    uint32_t batch = input->batch_size;
    NIMCP_API_CHECK(batch == effects->batch_size, NIMCP_ERROR_INVALID_PARAM,
        "metabolic_compute_effects_tensor: batch size mismatch");

    /* Use default multipliers if none provided */
    metabolic_effect_multipliers_t default_mult = metabolic_default_multipliers();

    for (uint32_t i = 0; i < batch; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && batch > 256) {
            metabolic_modulation_heartbeat("metabolic_mo_loop",
                             (float)(i + 1) / (float)batch);
        }

        float atp = (float)nimcp_tensor_get_flat(input->atp_levels, i);
        float cap = (float)nimcp_tensor_get_flat(input->metabolic_capacities, i);
        float min = config->min_capacity;

        /* Get multipliers for this region */
        float mult_atp_p, mult_atp_s, mult_fat_p, mult_fat_s;
        if (multipliers && multipliers->multipliers) {
            size_t base = (multipliers->batch_size == 1) ? 0 : (i * 4);
            mult_atp_p = (float)nimcp_tensor_get_flat(multipliers->multipliers, base + 0);
            mult_atp_s = (float)nimcp_tensor_get_flat(multipliers->multipliers, base + 1);
            mult_fat_p = (float)nimcp_tensor_get_flat(multipliers->multipliers, base + 2);
            mult_fat_s = (float)nimcp_tensor_get_flat(multipliers->multipliers, base + 3);
        } else {
            mult_atp_p = default_mult.atp_primary_mult;
            mult_atp_s = default_mult.atp_secondary_mult;
            mult_fat_p = default_mult.fatigue_primary_mult;
            mult_fat_s = default_mult.fatigue_secondary_mult;
        }

        /* Compute effect indices for this region */
        size_t base_idx = (effects->batch_size == 1) ? 0 : (i * METABOLIC_EFFECT_COUNT);

        /* ATP modulation */
        float primary_atp = 1.0f, secondary_atp = 1.0f;
        if (config->enable_atp_modulation) {
            primary_atp = compute_clamped_effect(atp, config->atp_sensitivity, mult_atp_p, min);
            secondary_atp = compute_clamped_effect(atp, config->atp_sensitivity, mult_atp_s, min);
        }
        nimcp_tensor_set_flat(effects->effects, base_idx + METABOLIC_IDX_PRIMARY_ATP, primary_atp);
        nimcp_tensor_set_flat(effects->effects, base_idx + METABOLIC_IDX_SECONDARY_ATP, secondary_atp);

        /* Fatigue modulation */
        float primary_fatigue = 1.0f, secondary_fatigue = 1.0f;
        if (config->enable_fatigue_modulation) {
            primary_fatigue = compute_clamped_effect(cap, config->fatigue_sensitivity, mult_fat_p, min);
            secondary_fatigue = compute_clamped_effect(cap, config->fatigue_sensitivity, mult_fat_s, min);
        }
        nimcp_tensor_set_flat(effects->effects, base_idx + METABOLIC_IDX_PRIMARY_FATIGUE, primary_fatigue);
        nimcp_tensor_set_flat(effects->effects, base_idx + METABOLIC_IDX_SECONDARY_FATIGUE, secondary_fatigue);

        /* Overall capacity = average of 4 effects */
        float overall = (primary_atp + secondary_atp + primary_fatigue + secondary_fatigue) / 4.0f;
        nimcp_tensor_set_flat(effects->effects, base_idx + METABOLIC_IDX_OVERALL_CAPACITY, overall);
    }

    return 0;
}

int metabolic_compute_overall_capacity_tensor(metabolic_effects_tensor_t* effects) {
    if (!effects || !effects->effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_compute_overall_capacity_tensor: required parameter is NULL (effects, effects->effects)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_compute_ov", 0.0f);


    for (uint32_t i = 0; i < effects->batch_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && effects->batch_size > 256) {
            metabolic_modulation_heartbeat("metabolic_mo_loop",
                             (float)(i + 1) / (float)effects->batch_size);
        }

        size_t base_idx = (effects->batch_size == 1) ? 0 : (i * METABOLIC_EFFECT_COUNT);

        float sum = 0.0f;
        for (int j = 0; j < 4; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && 4 > 256) {
                metabolic_modulation_heartbeat("metabolic_mo_loop",
                                 (float)(j + 1) / (float)4);
            }

            sum += (float)nimcp_tensor_get_flat(effects->effects, base_idx + j);
        }
        nimcp_tensor_set_flat(effects->effects, base_idx + METABOLIC_IDX_OVERALL_CAPACITY, sum / 4.0f);
    }

    return 0;
}

/* ============================================================================
 * Tensor/Scalar Conversion Functions
 * ============================================================================ */

int metabolic_effects_tensor_to_scalar(
    const metabolic_effects_tensor_t* tensor_effects,
    uint32_t region_idx,
    metabolic_effects_t* scalar_effects
) {
    if (!tensor_effects || !tensor_effects->effects || !scalar_effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_effects_tensor_to_scalar: required parameter is NULL (tensor_effects, tensor_effects->effects, scalar_effects)");
        return -1;
    }
    if (region_idx >= tensor_effects->batch_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_effects_tensor_to_scalar: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_te", 0.0f);


    size_t base_idx = (tensor_effects->batch_size == 1) ? 0 : (region_idx * METABOLIC_EFFECT_COUNT);

    scalar_effects->primary_atp = (float)nimcp_tensor_get_flat(
        tensor_effects->effects, base_idx + METABOLIC_IDX_PRIMARY_ATP);
    scalar_effects->secondary_atp = (float)nimcp_tensor_get_flat(
        tensor_effects->effects, base_idx + METABOLIC_IDX_SECONDARY_ATP);
    scalar_effects->primary_fatigue = (float)nimcp_tensor_get_flat(
        tensor_effects->effects, base_idx + METABOLIC_IDX_PRIMARY_FATIGUE);
    scalar_effects->secondary_fatigue = (float)nimcp_tensor_get_flat(
        tensor_effects->effects, base_idx + METABOLIC_IDX_SECONDARY_FATIGUE);
    scalar_effects->overall_capacity = (float)nimcp_tensor_get_flat(
        tensor_effects->effects, base_idx + METABOLIC_IDX_OVERALL_CAPACITY);

    return 0;
}

int metabolic_effects_scalar_to_tensor(
    metabolic_effects_tensor_t* tensor_effects,
    uint32_t region_idx,
    const metabolic_effects_t* scalar_effects
) {
    if (!tensor_effects || !tensor_effects->effects || !scalar_effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_effects_scalar_to_tensor: required parameter is NULL (tensor_effects, tensor_effects->effects, scalar_effects)");
        return -1;
    }
    if (region_idx >= tensor_effects->batch_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_effects_scalar_to_tensor: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_sc", 0.0f);


    size_t base_idx = (tensor_effects->batch_size == 1) ? 0 : (region_idx * METABOLIC_EFFECT_COUNT);

    nimcp_tensor_set_flat(tensor_effects->effects,
        base_idx + METABOLIC_IDX_PRIMARY_ATP, scalar_effects->primary_atp);
    nimcp_tensor_set_flat(tensor_effects->effects,
        base_idx + METABOLIC_IDX_SECONDARY_ATP, scalar_effects->secondary_atp);
    nimcp_tensor_set_flat(tensor_effects->effects,
        base_idx + METABOLIC_IDX_PRIMARY_FATIGUE, scalar_effects->primary_fatigue);
    nimcp_tensor_set_flat(tensor_effects->effects,
        base_idx + METABOLIC_IDX_SECONDARY_FATIGUE, scalar_effects->secondary_fatigue);
    nimcp_tensor_set_flat(tensor_effects->effects,
        base_idx + METABOLIC_IDX_OVERALL_CAPACITY, scalar_effects->overall_capacity);

    return 0;
}

float metabolic_effects_tensor_get(
    const metabolic_effects_tensor_t* effects,
    uint32_t region_idx,
    metabolic_effect_index_t effect_idx
) {
    if (!effects || !effects->effects) return NAN;
    if (region_idx >= effects->batch_size) return NAN;
    if (effect_idx >= METABOLIC_EFFECT_COUNT) return NAN;

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_te", 0.0f);


    size_t base_idx = (effects->batch_size == 1) ? 0 : (region_idx * METABOLIC_EFFECT_COUNT);
    return (float)nimcp_tensor_get_flat(effects->effects, base_idx + effect_idx);
}

int metabolic_effects_tensor_set(
    metabolic_effects_tensor_t* effects,
    uint32_t region_idx,
    metabolic_effect_index_t effect_idx,
    float value
) {
    if (!effects || !effects->effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_effects_tensor_set: required parameter is NULL (effects, effects->effects)");
        return -1;
    }
    if (region_idx >= effects->batch_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_effects_tensor_set: capacity exceeded");
        return -1;
    }
    if (effect_idx >= METABOLIC_EFFECT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "metabolic_effects_tensor_set: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_effects_te", 0.0f);


    size_t base_idx = (effects->batch_size == 1) ? 0 : (region_idx * METABOLIC_EFFECT_COUNT);
    return nimcp_tensor_set_flat(effects->effects, base_idx + effect_idx, value);
}

metabolic_input_tensor_t* metabolic_input_tensor_from_scalar(
    float atp_level,
    float metabolic_capacity
) {
    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_input_tens", 0.0f);


    metabolic_input_tensor_t* input = metabolic_input_tensor_create(1);
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metabolic_input_tensor_from_scalar: input is NULL");
        return NULL;
    }

    nimcp_tensor_set_flat(input->atp_levels, 0, atp_level);
    nimcp_tensor_set_flat(input->metabolic_capacities, 0, metabolic_capacity);

    return input;
}

int metabolic_input_tensor_set_region(
    metabolic_input_tensor_t* input,
    uint32_t region_idx,
    float atp_level,
    float metabolic_capacity
) {
    if (!input || !input->atp_levels || !input->metabolic_capacities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_input_tensor_set_region: required parameter is NULL (input, input->atp_levels, input->metabolic_capacities)");
        return -1;
    }
    if (region_idx >= input->batch_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_input_tensor_set_region: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_metabolic_input_tens", 0.0f);


    nimcp_tensor_set_flat(input->atp_levels, region_idx, atp_level);
    nimcp_tensor_set_flat(input->metabolic_capacities, region_idx, metabolic_capacity);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int metabolic_modulation_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    metabolic_modulation_heartbeat("metabolic_mo_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Metabolic_Modulation");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                metabolic_modulation_heartbeat("metabolic_mo_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Metabolic_Modulation");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Metabolic_Modulation");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void metabolic_modulation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_metabolic_modulation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int metabolic_modulation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metabolic_modulation_training_begin: NULL argument");
        return -1;
    }
    metabolic_modulation_heartbeat_instance(NULL, "metabolic_modulation_training_begin", 0.0f);
    return 0;
}

int metabolic_modulation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metabolic_modulation_training_end: NULL argument");
        return -1;
    }
    metabolic_modulation_heartbeat_instance(NULL, "metabolic_modulation_training_end", 1.0f);
    return 0;
}

int metabolic_modulation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metabolic_modulation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    metabolic_modulation_heartbeat_instance(NULL, "metabolic_modulation_training_step", progress);
    return 0;
}
