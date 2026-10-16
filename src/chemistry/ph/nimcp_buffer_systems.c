/**
 * @file nimcp_buffer_systems.c
 * @brief Buffer Systems Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "chemistry/ph/nimcp_buffer_systems.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(buffer_systems)

//=============================================================================
// Internal Constants
//=============================================================================

/** Natural log of 10 */
#define LN_10 2.302585093f

/** Minimum buffer concentration */
#define MIN_BUFFER_CONCENTRATION 0.001f

/** Default regeneration time constant (ms) */
#define BUFFER_REGEN_TAU 10000.0f

/**
 * @brief Initialize bicarbonate buffer with defaults
 */
static void init_bicarbonate_defaults(nimcp_bicarbonate_buffer_t* buffer) {
    memset(buffer, 0, sizeof(nimcp_bicarbonate_buffer_t));

    buffer->hco3_concentration = BUFFER_HCO3_NORMAL;
    buffer->pco2 = BUFFER_PCO2_NORMAL;
    buffer->co2_concentration = BUFFER_PCO2_NORMAL * BUFFER_CO2_SOLUBILITY;
    buffer->h2co3_concentration = buffer->co2_concentration * 0.001f;  /* Small fraction */

    buffer->ventilation_factor = 1.0f;
    buffer->carbonic_anhydrase_activity = 1.0f;

    buffer->pka = BUFFER_BICARBONATE_PKA;
    buffer->buffering_power = buffer->hco3_concentration * LN_10;

    buffer->state = BUFFER_STATE_NORMAL;
    buffer->saturation = 0.5f;  /* At equilibrium */
}

/**
 * @brief Initialize phosphate buffer with defaults
 */
static void init_phosphate_defaults(nimcp_phosphate_buffer_t* buffer) {
    memset(buffer, 0, sizeof(nimcp_phosphate_buffer_t));

    buffer->total_phosphate = 1.0f;  /* mM, intracellular higher */
    buffer->pka = BUFFER_PHOSPHATE_PKA;

    /* At pH 7.4, ratio is about 4:1 HPO4/H2PO4 */
    buffer->hpo4_concentration = buffer->total_phosphate * 0.8f;
    buffer->h2po4_concentration = buffer->total_phosphate * 0.2f;

    buffer->buffering_power = buffer->total_phosphate * LN_10 * 0.8f;

    buffer->state = BUFFER_STATE_NORMAL;
    buffer->saturation = 0.5f;
}

/**
 * @brief Initialize protein buffer with defaults
 */
static void init_protein_defaults(nimcp_protein_buffer_t* buffer) {
    memset(buffer, 0, sizeof(nimcp_protein_buffer_t));

    buffer->total_protein = 70.0f;  /* g/L plasma protein */
    buffer->albumin_concentration = 42.0f;  /* g/L */
    buffer->globulin_concentration = 28.0f;  /* g/L */
    buffer->hemoglobin_concentration = 0.0f;  /* 0 unless blood */

    buffer->histidine_content = 0.1f;  /* mol His per g protein */
    buffer->effective_pka = BUFFER_HISTIDINE_PKA;
    buffer->protonated_fraction = 0.5f;

    /* Protein buffering: ~0.1 mM/pH per g/L */
    buffer->buffering_power = buffer->total_protein * 0.1f;

    buffer->state = BUFFER_STATE_NORMAL;
    buffer->saturation = 0.5f;
}

/**
 * @brief Initialize default configuration
 */
static void init_default_config(nimcp_buffer_config_t* config) {
    config->initial_hco3 = BUFFER_HCO3_NORMAL;
    config->initial_pco2 = BUFFER_PCO2_NORMAL;
    config->ca_activity = 1.0f;

    config->initial_phosphate = 1.0f;

    config->total_protein = 70.0f;
    config->albumin_fraction = 0.6f;

    config->regeneration_rate = 0.001f;
    config->target_ph = 7.4f;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

nimcp_buffer_error_t nimcp_buffer_init(
    nimcp_buffer_manager_t* manager,
    const nimcp_buffer_config_t* config
) {
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Buffer manager is NULL");
        return BUFFER_ERR_NULL_PTR;
    }

    memset(manager, 0, sizeof(nimcp_buffer_manager_t));

    /* Initialize individual buffer systems */
    init_bicarbonate_defaults(&manager->bicarbonate);
    init_phosphate_defaults(&manager->phosphate);
    init_protein_defaults(&manager->protein);

    /* Apply configuration */
    if (config) {
        memcpy(&manager->config, config, sizeof(nimcp_buffer_config_t));

        manager->bicarbonate.hco3_concentration = config->initial_hco3;
        manager->bicarbonate.pco2 = config->initial_pco2;
        manager->bicarbonate.co2_concentration = config->initial_pco2 * BUFFER_CO2_SOLUBILITY;
        manager->bicarbonate.carbonic_anhydrase_activity = config->ca_activity;

        manager->phosphate.total_phosphate = config->initial_phosphate;

        manager->protein.total_protein = config->total_protein;
        manager->protein.albumin_concentration = config->total_protein * config->albumin_fraction;
        manager->protein.globulin_concentration = config->total_protein * (1.0f - config->albumin_fraction);
    } else {
        init_default_config(&manager->config);
    }

    /* Calculate total buffering capacity */
    manager->total_buffering_capacity =
        manager->bicarbonate.buffering_power +
        manager->phosphate.buffering_power +
        manager->protein.buffering_power;

    manager->effective_buffer_power = manager->total_buffering_capacity;
    manager->ph_stability_index = 1.0f;
    manager->current_ph = manager->config.target_ph;
    manager->proton_load = 0.0f;

    manager->initialized = true;
    manager->update_count = 0;

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_buffer_shutdown(nimcp_buffer_manager_t* manager) {
    if (!manager) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Buffer manager is NULL in shutdown");
        return BUFFER_ERR_NULL_PTR;
    }

    memset(manager, 0, sizeof(nimcp_buffer_manager_t));

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_buffer_reset(nimcp_buffer_manager_t* manager) {
    if (!manager) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Buffer manager is NULL in reset");
        return BUFFER_ERR_NULL_PTR;
    }

    /* Reset to initial values */
    init_bicarbonate_defaults(&manager->bicarbonate);
    init_phosphate_defaults(&manager->phosphate);
    init_protein_defaults(&manager->protein);

    /* Apply saved config */
    manager->bicarbonate.hco3_concentration = manager->config.initial_hco3;
    manager->bicarbonate.pco2 = manager->config.initial_pco2;
    manager->phosphate.total_phosphate = manager->config.initial_phosphate;
    manager->protein.total_protein = manager->config.total_protein;

    /* Recalculate totals */
    manager->total_buffering_capacity =
        manager->bicarbonate.buffering_power +
        manager->phosphate.buffering_power +
        manager->protein.buffering_power;

    manager->current_ph = manager->config.target_ph;
    manager->proton_load = 0.0f;
    manager->ph_stability_index = 1.0f;

    /* Reset custom components */
    for (uint32_t i = 0; i < manager->num_components; i++) {
        manager->components[i].saturation = 0.5f;
        manager->components[i].state = BUFFER_STATE_NORMAL;
    }

    manager->update_count = 0;

    return BUFFER_OK;
}

//=============================================================================
// Bicarbonate Buffer API Implementation
//=============================================================================

nimcp_buffer_error_t nimcp_bicarbonate_set_concentration(
    nimcp_bicarbonate_buffer_t* buffer,
    float concentration
) {
    if (!buffer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Bicarbonate buffer is NULL in set_concentration");
        return BUFFER_ERR_NULL_PTR;
    }

    if (concentration < 0.0f) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Bicarbonate concentration cannot be negative: %f", concentration);
        return BUFFER_ERR_INVALID_PARAM;
    }

    buffer->hco3_concentration = concentration;
    buffer->buffering_power = concentration * LN_10;

    /* Update state based on concentration */
    if (concentration < 10.0f) {
        buffer->state = BUFFER_STATE_DEPLETED;
    } else if (concentration > 30.0f) {
        buffer->state = BUFFER_STATE_SATURATED;
    } else {
        buffer->state = BUFFER_STATE_NORMAL;
    }

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_bicarbonate_set_pco2(
    nimcp_bicarbonate_buffer_t* buffer,
    float pco2
) {
    if (!buffer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Bicarbonate buffer is NULL in set_pco2");
        return BUFFER_ERR_NULL_PTR;
    }

    if (pco2 < 0.0f) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "pCO2 cannot be negative: %f", pco2);
        return BUFFER_ERR_INVALID_PARAM;
    }

    buffer->pco2 = pco2;
    buffer->co2_concentration = pco2 * BUFFER_CO2_SOLUBILITY;

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_bicarbonate_calculate_ph(
    const nimcp_bicarbonate_buffer_t* buffer,
    float* ph
) {
    if (!buffer || !ph) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Bicarbonate calculate_ph: NULL argument");
        return BUFFER_ERR_NULL_PTR;
    }

    /* Henderson-Hasselbalch for bicarbonate:
     * pH = pKa + log([HCO3-] / [CO2])
     * Using pKa = 6.1 for CO2/HCO3- system
     */
    float co2_effective = buffer->pco2 * BUFFER_CO2_SOLUBILITY;

    if (co2_effective < MIN_BUFFER_CONCENTRATION) {
        co2_effective = MIN_BUFFER_CONCENTRATION;
    }

    float ratio = buffer->hco3_concentration / co2_effective;

    if (ratio <= 0.0f) {
        return BUFFER_ERR_INVALID_PH;
    }

    *ph = buffer->pka + log10f(ratio);
    *ph = nimcp_clampf(*ph, 0.0f, 14.0f);

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_bicarbonate_apply_acid(
    nimcp_bicarbonate_buffer_t* buffer,
    float h_load,
    float* delta_ph
) {
    if (!buffer || !delta_ph) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Bicarbonate apply_acid: NULL argument");
        return BUFFER_ERR_NULL_PTR;
    }

    /* Acid neutralization: H+ + HCO3- -> H2CO3 -> CO2 + H2O */
    float hco3_consumed = h_load;  /* 1:1 stoichiometry */

    /* Check if we have enough buffer */
    if (hco3_consumed > buffer->hco3_concentration * 0.9f) {
        hco3_consumed = buffer->hco3_concentration * 0.9f;
        buffer->state = BUFFER_STATE_DEPLETED;
    }

    /* Calculate pH before */
    float ph_before;
    nimcp_bicarbonate_calculate_ph(buffer, &ph_before);

    /* Update concentrations */
    buffer->hco3_concentration -= hco3_consumed;
    buffer->co2_concentration += hco3_consumed;  /* CO2 is produced */

    /* With ventilation, CO2 is removed */
    float co2_removed = (buffer->co2_concentration - BUFFER_PCO2_NORMAL * BUFFER_CO2_SOLUBILITY) *
                        buffer->ventilation_factor * 0.1f;
    buffer->co2_concentration -= co2_removed;
    if (buffer->co2_concentration < 0.5f) {
        buffer->co2_concentration = 0.5f;
    }

    /* Calculate pH after */
    float ph_after;
    nimcp_bicarbonate_calculate_ph(buffer, &ph_after);

    *delta_ph = ph_after - ph_before;

    /* Update saturation */
    buffer->saturation = 1.0f - (buffer->hco3_concentration / BUFFER_HCO3_NORMAL);
    buffer->saturation = nimcp_clampf(buffer->saturation, 0.0f, 1.0f);

    return BUFFER_OK;
}

//=============================================================================
// Phosphate Buffer API Implementation
//=============================================================================

nimcp_buffer_error_t nimcp_phosphate_set_concentration(
    nimcp_phosphate_buffer_t* buffer,
    float concentration
) {
    if (!buffer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Phosphate buffer is NULL in set_concentration");
        return BUFFER_ERR_NULL_PTR;
    }

    if (concentration < 0.0f) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Phosphate concentration cannot be negative: %f", concentration);
        return BUFFER_ERR_INVALID_PARAM;
    }

    buffer->total_phosphate = concentration;

    /* Redistribute based on current ratio */
    float total = buffer->h2po4_concentration + buffer->hpo4_concentration;
    if (total > MIN_BUFFER_CONCENTRATION) {
        float ratio = buffer->hpo4_concentration / total;
        buffer->hpo4_concentration = concentration * ratio;
        buffer->h2po4_concentration = concentration * (1.0f - ratio);
    } else {
        /* Default to pH 7.4 distribution */
        buffer->hpo4_concentration = concentration * 0.8f;
        buffer->h2po4_concentration = concentration * 0.2f;
    }

    buffer->buffering_power = concentration * LN_10 * 0.8f;

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_phosphate_apply_acid(
    nimcp_phosphate_buffer_t* buffer,
    float current_ph,
    float h_load,
    float* delta_ph
) {
    if (!buffer || !delta_ph) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Phosphate apply_acid: NULL argument");
        return BUFFER_ERR_NULL_PTR;
    }

    /* Acid neutralization: H+ + HPO4^2- -> H2PO4^- */
    float hpo4_consumed = h_load;

    /* Check availability */
    if (hpo4_consumed > buffer->hpo4_concentration * 0.9f) {
        hpo4_consumed = buffer->hpo4_concentration * 0.9f;
        buffer->state = BUFFER_STATE_DEPLETED;
    }

    /* Update species */
    buffer->hpo4_concentration -= hpo4_consumed;
    buffer->h2po4_concentration += hpo4_consumed;

    /* Calculate new pH using Henderson-Hasselbalch */
    float ratio = buffer->hpo4_concentration / buffer->h2po4_concentration;
    if (ratio < MIN_BUFFER_CONCENTRATION) {
        ratio = MIN_BUFFER_CONCENTRATION;
    }

    float new_ph = buffer->pka + log10f(ratio);
    new_ph = nimcp_clampf(new_ph, 0.0f, 14.0f);

    *delta_ph = new_ph - current_ph;

    /* Update saturation */
    buffer->saturation = buffer->h2po4_concentration / buffer->total_phosphate;
    buffer->saturation = nimcp_clampf(buffer->saturation, 0.0f, 1.0f);

    return BUFFER_OK;
}

//=============================================================================
// Protein Buffer API Implementation
//=============================================================================

nimcp_buffer_error_t nimcp_protein_set_concentration(
    nimcp_protein_buffer_t* buffer,
    float concentration
) {
    if (!buffer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Protein buffer is NULL in set_concentration");
        return BUFFER_ERR_NULL_PTR;
    }

    if (concentration < 0.0f) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Protein concentration cannot be negative: %f", concentration);
        return BUFFER_ERR_INVALID_PARAM;
    }

    buffer->total_protein = concentration;
    buffer->buffering_power = concentration * 0.1f;

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_protein_apply_acid(
    nimcp_protein_buffer_t* buffer,
    float current_ph,
    float h_load,
    float* delta_ph
) {
    if (!buffer || !delta_ph) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Protein apply_acid: NULL argument");
        return BUFFER_ERR_NULL_PTR;
    }

    /* Protein buffering via histidine residues */
    float available_sites = buffer->total_protein * buffer->histidine_content *
                           (1.0f - buffer->protonated_fraction);

    float h_buffered = h_load;
    if (h_buffered > available_sites * 0.9f) {
        h_buffered = available_sites * 0.9f;
        buffer->state = BUFFER_STATE_DEPLETED;
    }

    /* Calculate buffering effect */
    float beta = buffer->buffering_power;
    if (beta < MIN_BUFFER_CONCENTRATION) {
        beta = MIN_BUFFER_CONCENTRATION;
    }

    *delta_ph = -h_buffered / beta;

    /* Update protonation state */
    float total_sites = buffer->total_protein * buffer->histidine_content;
    if (total_sites > MIN_BUFFER_CONCENTRATION) {
        buffer->protonated_fraction += h_buffered / total_sites;
        buffer->protonated_fraction = nimcp_clampf(buffer->protonated_fraction, 0.0f, 1.0f);
    }

    /* Update saturation */
    buffer->saturation = buffer->protonated_fraction;

    (void)current_ph;  /* Used for consistency but not needed here */

    return BUFFER_OK;
}

//=============================================================================
// Buffer Manager API Implementation
//=============================================================================

nimcp_buffer_error_t nimcp_buffer_add_component(
    nimcp_buffer_manager_t* manager,
    nimcp_buffer_type_t type,
    const char* name,
    float pka,
    float concentration
) {
    if (!manager || !name) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Buffer add_component: NULL argument");
        return BUFFER_ERR_NULL_PTR;
    }

    if (manager->num_components >= BUFFER_MAX_SYSTEMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "Buffer component capacity exceeded: max %d", BUFFER_MAX_SYSTEMS);
        return BUFFER_ERR_CAPACITY_EXCEEDED;
    }

    nimcp_buffer_component_t* comp = &manager->components[manager->num_components];
    memset(comp, 0, sizeof(nimcp_buffer_component_t));

    comp->type = type;
    strncpy(comp->name, name, sizeof(comp->name) - 1);
    comp->pka = pka;
    comp->total_concentration = concentration;

    /* Set initial distribution based on target pH */
    float ratio = powf(10.0f, manager->config.target_ph - pka);
    comp->base_form = concentration * ratio / (1.0f + ratio);
    comp->acid_form = concentration - comp->base_form;

    /* Calculate buffering capacity */
    comp->buffering_capacity = concentration * LN_10 *
                              (comp->base_form / concentration) *
                              (comp->acid_form / concentration);
    comp->max_capacity = comp->buffering_capacity;

    comp->state = BUFFER_STATE_NORMAL;
    comp->saturation = 0.5f;
    comp->active = true;

    manager->num_components++;

    /* Update total capacity */
    manager->total_buffering_capacity += comp->buffering_capacity;

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_buffer_apply_acid_load(
    nimcp_buffer_manager_t* manager,
    float h_load,
    float current_ph,
    float* new_ph
) {
    if (!manager || !new_ph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Buffer manager or output pH is NULL");
        return BUFFER_ERR_NULL_PTR;
    }

    if (!manager->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "Buffer manager not initialized");
        return BUFFER_ERR_NOT_INITIALIZED;
    }

    float total_delta_ph = 0.0f;
    float remaining_h = h_load;

    /* Distribute load across buffers proportionally */
    float total_capacity = manager->total_buffering_capacity;
    if (total_capacity < MIN_BUFFER_CONCENTRATION) {
        /* No buffering - direct pH change */
        float h_conc = powf(10.0f, -current_ph);
        h_conc += h_load * 0.001f;
        *new_ph = -log10f(h_conc);
        *new_ph = nimcp_clampf(*new_ph, 0.0f, 14.0f);
        return BUFFER_OK;
    }

    /* Bicarbonate buffer (typically largest contribution) */
    {
        float fraction = manager->bicarbonate.buffering_power / total_capacity;
        float h_for_bicarb = remaining_h * fraction;
        float delta;
        nimcp_bicarbonate_apply_acid(&manager->bicarbonate, h_for_bicarb, &delta);
        total_delta_ph += delta * fraction;
    }

    /* Phosphate buffer */
    {
        float fraction = manager->phosphate.buffering_power / total_capacity;
        float h_for_phos = remaining_h * fraction;
        float delta;
        nimcp_phosphate_apply_acid(&manager->phosphate, current_ph, h_for_phos, &delta);
        total_delta_ph += delta * fraction;
    }

    /* Protein buffer */
    {
        float fraction = manager->protein.buffering_power / total_capacity;
        float h_for_prot = remaining_h * fraction;
        float delta;
        nimcp_protein_apply_acid(&manager->protein, current_ph, h_for_prot, &delta);
        total_delta_ph += delta * fraction;
    }

    /* Custom components */
    for (uint32_t i = 0; i < manager->num_components; i++) {
        nimcp_buffer_component_t* comp = &manager->components[i];
        if (!comp->active) continue;

        float fraction = comp->buffering_capacity / total_capacity;
        float h_for_comp = remaining_h * fraction;

        /* Simple buffering model for custom components */
        float delta = -h_for_comp / comp->buffering_capacity;
        total_delta_ph += delta * fraction;

        /* Update component state */
        comp->acid_form += h_for_comp;
        comp->base_form -= h_for_comp;
        if (comp->base_form < 0) {
            comp->base_form = 0;
            comp->state = BUFFER_STATE_DEPLETED;
        }
        comp->saturation = comp->acid_form / comp->total_concentration;
    }

    *new_ph = current_ph + total_delta_ph;
    *new_ph = nimcp_clampf(*new_ph, 0.0f, 14.0f);

    manager->current_ph = *new_ph;
    manager->proton_load += h_load;

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_buffer_apply_base_load(
    nimcp_buffer_manager_t* manager,
    float oh_load,
    float current_ph,
    float* new_ph
) {
    if (!manager || !new_ph) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Buffer apply_base_load: NULL argument");
        return BUFFER_ERR_NULL_PTR;
    }

    /* Base load is equivalent to negative acid load */
    return nimcp_buffer_apply_acid_load(manager, -oh_load, current_ph, new_ph);
}

nimcp_buffer_error_t nimcp_buffer_get_total_capacity(
    const nimcp_buffer_manager_t* manager,
    float* capacity
) {
    if (!manager || !capacity) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Buffer get_total_capacity: NULL argument");
        return BUFFER_ERR_NULL_PTR;
    }

    *capacity = manager->total_buffering_capacity;

    return BUFFER_OK;
}

nimcp_buffer_error_t nimcp_buffer_update(
    nimcp_buffer_manager_t* manager,
    float dt
) {
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Buffer manager is NULL in update");
        return BUFFER_ERR_NULL_PTR;
    }

    if (!manager->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "Buffer manager not initialized");
        return BUFFER_ERR_NOT_INITIALIZED;
    }

    float dt_sec = dt / 1000.0f;

    /* Regenerate buffer capacity over time */
    float regen_rate = manager->config.regeneration_rate;

    /* Bicarbonate regeneration (renal + respiratory) */
    if (manager->bicarbonate.hco3_concentration < BUFFER_HCO3_NORMAL) {
        float regen = (BUFFER_HCO3_NORMAL - manager->bicarbonate.hco3_concentration) *
                     regen_rate * dt_sec;
        manager->bicarbonate.hco3_concentration += regen;
    }

    /* CO2 equilibration */
    float co2_target = BUFFER_PCO2_NORMAL * BUFFER_CO2_SOLUBILITY;
    float co2_diff = co2_target - manager->bicarbonate.co2_concentration;
    manager->bicarbonate.co2_concentration += co2_diff * 0.1f * dt_sec;

    /* Phosphate redistribution toward equilibrium at current pH */
    float ph = manager->current_ph;
    float target_ratio = powf(10.0f, ph - manager->phosphate.pka);
    float current_ratio = manager->phosphate.hpo4_concentration /
                         (manager->phosphate.h2po4_concentration + MIN_BUFFER_CONCENTRATION);
    float ratio_diff = target_ratio - current_ratio;
    float redistribution = ratio_diff * 0.01f * dt_sec;

    if (redistribution > 0 && manager->phosphate.h2po4_concentration > MIN_BUFFER_CONCENTRATION) {
        float transfer = redistribution * manager->phosphate.h2po4_concentration;
        manager->phosphate.h2po4_concentration -= transfer;
        manager->phosphate.hpo4_concentration += transfer;
    } else if (redistribution < 0 && manager->phosphate.hpo4_concentration > MIN_BUFFER_CONCENTRATION) {
        float transfer = -redistribution * manager->phosphate.hpo4_concentration;
        manager->phosphate.hpo4_concentration -= transfer;
        manager->phosphate.h2po4_concentration += transfer;
    }

    /* Protein buffer regeneration */
    float target_protonation = 0.5f;  /* At pKa, 50% protonated */
    float prot_diff = target_protonation - manager->protein.protonated_fraction;
    manager->protein.protonated_fraction += prot_diff * 0.01f * dt_sec;

    /* Recalculate total capacity */
    manager->total_buffering_capacity =
        manager->bicarbonate.buffering_power +
        manager->phosphate.buffering_power +
        manager->protein.buffering_power;

    for (uint32_t i = 0; i < manager->num_components; i++) {
        if (manager->components[i].active) {
            manager->total_buffering_capacity += manager->components[i].buffering_capacity;
        }
    }

    /* Update stability index */
    float ph_dev = fabsf(manager->current_ph - manager->config.target_ph);
    manager->ph_stability_index = expf(-ph_dev * 5.0f);

    /* Update buffer states */
    if (manager->bicarbonate.hco3_concentration < 15.0f) {
        manager->bicarbonate.state = BUFFER_STATE_DEPLETED;
    } else if (manager->bicarbonate.saturation > 0.8f) {
        manager->bicarbonate.state = BUFFER_STATE_SATURATED;
    } else {
        manager->bicarbonate.state = BUFFER_STATE_NORMAL;
    }

    manager->update_count++;

    return BUFFER_OK;
}

//=============================================================================
// Henderson-Hasselbalch Calculations
//=============================================================================

float nimcp_buffer_henderson_hasselbalch(
    float pka,
    float base_form,
    float acid_form
) {
    if (acid_form <= MIN_BUFFER_CONCENTRATION) {
        acid_form = MIN_BUFFER_CONCENTRATION;
    }
    if (base_form <= MIN_BUFFER_CONCENTRATION) {
        base_form = MIN_BUFFER_CONCENTRATION;
    }

    return pka + log10f(base_form / acid_form);
}

float nimcp_buffer_ratio_from_ph(float ph, float pka) {
    return powf(10.0f, ph - pka);
}

float nimcp_buffer_capacity_at_ph(
    float total_buffer,
    float pka,
    float ph
) {
    /* Buffering capacity is maximal at pH = pKa */
    float ratio = powf(10.0f, ph - pka);
    float fraction_base = ratio / (1.0f + ratio);
    float fraction_acid = 1.0f / (1.0f + ratio);

    /* Beta = 2.3 * C * fa * fb */
    return LN_10 * total_buffer * fraction_acid * fraction_base;
}

//=============================================================================
// Utility API Implementation
//=============================================================================

float nimcp_buffer_ph_to_h(float ph) {
    return powf(10.0f, -ph);
}

float nimcp_buffer_h_to_ph(float h_concentration) {
    if (h_concentration <= 0.0f) {
        return 14.0f;  /* Very basic */
    }
    return -log10f(h_concentration);
}

const char* nimcp_buffer_state_string(nimcp_buffer_state_t state) {
    switch (state) {
        case BUFFER_STATE_NORMAL:
            return "Normal";
        case BUFFER_STATE_DEPLETED:
            return "Depleted";
        case BUFFER_STATE_SATURATED:
            return "Saturated";
        case BUFFER_STATE_REGENERATING:
            return "Regenerating";
        default:
            return "Unknown";
    }
}

const char* nimcp_buffer_error_string(nimcp_buffer_error_t error) {
    switch (error) {
        case BUFFER_OK:
            return "OK";
        case BUFFER_ERR_NULL_PTR:
            return "Null pointer";
        case BUFFER_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case BUFFER_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case BUFFER_ERR_BUFFER_EXHAUSTED:
            return "Buffer exhausted";
        case BUFFER_ERR_CAPACITY_EXCEEDED:
            return "Capacity exceeded";
        case BUFFER_ERR_INVALID_PH:
            return "Invalid pH";
        default:
            return "Unknown error";
    }
}
