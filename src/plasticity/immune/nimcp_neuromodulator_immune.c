/**
 * @file nimcp_neuromodulator_immune.c
 * @brief Brain Immune-Neuromodulator Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "plasticity/immune/nimcp_neuromodulator_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromodulator_immune)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Compute deviation from baseline
 *
 * WHAT: Calculate how far current level is from homeostatic baseline
 * WHY:  Quantify imbalance magnitude
 * HOW:  Normalized difference: (current - baseline) / baseline
 *
 * @param current Current level
 * @param baseline Homeostatic baseline
 * @return Deviation (-1 to +1, 0 = balanced)
 */
static float compute_deviation(float current, float baseline) {
    if (baseline < 1e-10f) return 0.0f;
    return (current - baseline) / baseline;
}

/**
 * @brief Check if deviation exceeds threshold
 *
 * @param deviation Deviation value
 * @param threshold Detection threshold
 * @return true if imbalanced
 */
static bool is_imbalanced(float deviation, float threshold) {
    return fabsf(deviation) > threshold;
}

/**
 * @brief Classify imbalance type from deviations
 *
 * @param da_dev Dopamine deviation
 * @param sert_dev Serotonin deviation
 * @param ne_dev Norepinephrine deviation
 * @param ach_dev Acetylcholine deviation
 * @param threshold Detection threshold
 * @return Imbalance type
 */
static neuromod_imbalance_type_t classify_imbalance(
    float da_dev, float sert_dev, float ne_dev, float ach_dev, float threshold)
{
    /* Check each neuromodulator in order of clinical significance */
    if (fabsf(da_dev) > threshold) {
        return da_dev > 0 ? NEUROMOD_IMBALANCE_DA_EXCESS : NEUROMOD_IMBALANCE_DA_DEFICIENCY;
    }
    if (fabsf(sert_dev) > threshold) {
        return sert_dev > 0 ? NEUROMOD_IMBALANCE_5HT_EXCESS : NEUROMOD_IMBALANCE_5HT_DEFICIENCY;
    }
    if (fabsf(ne_dev) > threshold) {
        return ne_dev > 0 ? NEUROMOD_IMBALANCE_NE_EXCESS : NEUROMOD_IMBALANCE_NE_DEFICIENCY;
    }
    if (fabsf(ach_dev) > threshold) {
        return ach_dev > 0 ? NEUROMOD_IMBALANCE_ACH_EXCESS : NEUROMOD_IMBALANCE_ACH_DEFICIENCY;
    }
    return NEUROMOD_IMBALANCE_NONE;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int neuromod_immune_default_config(neuromod_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Biological baselines (µM) */
    config->dopamine_baseline = 0.00005f;        /* 50 nM */
    config->serotonin_baseline = 0.00003f;       /* 30 nM */
    config->norepinephrine_baseline = 0.00002f;  /* 20 nM */
    config->acetylcholine_baseline = 0.0001f;    /* 100 nM */

    /* Imbalance detection */
    config->imbalance_threshold = NEUROMOD_IMMUNE_IMBALANCE_THRESHOLD;
    config->immune_alert_threshold = 0.6f;
    config->min_duration_for_alert_ms = 5000;

    /* Cytokine effect parameters (biological estimates) */
    config->cytokine_sensitivity = 0.5f;
    config->il1_effect_strength = 0.7f;
    config->il6_effect_strength = 0.6f;
    config->tnf_alpha_effect_strength = 0.8f;
    config->il10_effect_strength = -0.5f;  /* Negative = restorative */

    /* Homeostatic correction */
    config->enable_auto_correction = true;
    config->correction_rate = 0.1f;
    config->correction_delay_ms = 10000;

    /* Integration enables */
    config->enable_cytokine_effects = true;
    config->enable_imbalance_detection = true;
    config->enable_metabolic_modeling = true;

    return 0;
}

neuromod_immune_system_t* neuromod_immune_create(const neuromod_immune_config_t* config) {
    /* WHAT: Allocate and initialize neuromodulator-immune integration system
     * WHY:  Set up bidirectional neuroimmune communication
     * HOW:  Allocate state, initialize metabolic models, configure thresholds
     */

    neuromod_immune_system_t* system = (neuromod_immune_system_t*)nimcp_malloc(
        sizeof(neuromod_immune_system_t));
    if (!system) {
        LOG_MODULE_ERROR(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Failed to allocate neuromod-immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;
    }

    memset(system, 0, sizeof(neuromod_immune_system_t));

    /* Use provided config or defaults */
    neuromod_immune_config_t default_cfg;
    if (!config) {
        neuromod_immune_default_config(&default_cfg);
        config = &default_cfg;
    }

    /* Initialize baselines */
    system->dopamine_baseline = config->dopamine_baseline;
    system->serotonin_baseline = config->serotonin_baseline;
    system->norepinephrine_baseline = config->norepinephrine_baseline;
    system->acetylcholine_baseline = config->acetylcholine_baseline;

    /* Initialize thresholds */
    system->imbalance_detection_threshold = config->imbalance_threshold;
    system->immune_alert_threshold = config->immune_alert_threshold;
    system->cytokine_sensitivity = config->cytokine_sensitivity;

    /* Initialize metabolic states if enabled */
    if (config->enable_metabolic_modeling) {
        metabolic_config_t met_cfg;

        met_cfg = metabolic_config_dopamine_default();
        metabolic_state_init_with_config(&system->dopamine_metabolism, &met_cfg);

        met_cfg = metabolic_config_serotonin_default();
        metabolic_state_init_with_config(&system->serotonin_metabolism, &met_cfg);

        met_cfg = metabolic_config_norepinephrine_default();
        metabolic_state_init_with_config(&system->norepinephrine_metabolism, &met_cfg);

        met_cfg = metabolic_config_acetylcholine_default();
        metabolic_state_init_with_config(&system->acetylcholine_metabolism, &met_cfg);
    }

    /* Initialize phasic-tonic states */
    phasic_tonic_config_t pt_cfg;
    uint64_t current_time = 0;  /* Will be set on first update */

    pt_cfg = phasic_tonic_config_dopamine_default();
    phasic_tonic_init(&system->dopamine_phasic, &pt_cfg, current_time);

    pt_cfg = phasic_tonic_config_serotonin_default();
    phasic_tonic_init(&system->serotonin_phasic, &pt_cfg, current_time);

    pt_cfg = phasic_tonic_config_norepinephrine_default();
    phasic_tonic_init(&system->norepinephrine_phasic, &pt_cfg, current_time);

    /* Initialize cytokine effects to neutral */
    system->cytokine_effects.dopamine_synthesis_multiplier = 1.0f;
    system->cytokine_effects.serotonin_synthesis_multiplier = 1.0f;
    system->cytokine_effects.norepinephrine_synthesis_multiplier = 1.0f;
    system->cytokine_effects.acetylcholine_synthesis_multiplier = 1.0f;
    system->cytokine_effects.tyrosine_availability = 1.0f;
    system->cytokine_effects.tryptophan_availability = 1.0f;
    system->cytokine_effects.choline_availability = 1.0f;
    system->cytokine_effects.tyrosine_hydroxylase_activity = 1.0f;
    system->cytokine_effects.tryptophan_hydroxylase_activity = 1.0f;
    system->cytokine_effects.dopa_decarboxylase_activity = 1.0f;
    system->cytokine_effects.mao_activity = 1.0f;
    system->cytokine_effects.comt_activity = 1.0f;
    system->cytokine_effects.ache_activity = 1.0f;

    /* Allocate imbalance tracking */
    system->imbalance_capacity = 32;
    system->imbalances = (neuromod_imbalance_t*)nimcp_malloc(
        system->imbalance_capacity * sizeof(neuromod_imbalance_t));
    if (!system->imbalances) {
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_immune_create: system->imbalances is NULL");
        return NULL;
    }
    system->imbalance_count = 0;
    system->next_imbalance_id = 1;

    /* Create mutex for thread safety */
    system->mutex = nimcp_platform_mutex_create();
    if (!system->mutex) {
        nimcp_free(system->imbalances);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_immune_create: system->mutex is NULL");
        return NULL;
    }

    system->running = false;

    LOG_MODULE_INFO(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Created neuromod-immune integration system");

    return system;
}

void neuromod_immune_destroy(neuromod_immune_system_t* system) {
    /* WHAT: Clean up neuromodulator-immune integration system
     * WHY:  Proper resource deallocation
     * HOW:  Free all allocated memory, destroy mutex
     */

    if (!system) return;

    if (system->mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)system->mutex);
    }

    if (system->imbalances) {
        nimcp_free(system->imbalances);
    }

    nimcp_free(system);

    LOG_MODULE_INFO(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Destroyed neuromod-immune integration system");
}

int neuromod_immune_connect_immune(
    neuromod_immune_system_t* system,
    brain_immune_system_t* immune_system)
{
    /* WHAT: Connect to brain immune system
     * WHY:  Receive cytokine signals, send imbalance alerts
     * HOW:  Store reference for later access
     */

    if (!system || !immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_connect_immune: required parameter is NULL (system, immune_system)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    system->immune_system = immune_system;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    LOG_MODULE_INFO(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Connected to brain immune system");

    return 0;
}

int neuromod_immune_connect_neuromod(
    neuromod_immune_system_t* system,
    neuromodulator_system_t neuromod_system)
{
    /* WHAT: Connect to neuromodulator system
     * WHY:  Monitor levels, apply cytokine effects
     * HOW:  Store reference for level queries
     */

    if (!system || !neuromod_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_connect_neuromod: required parameter is NULL (system, neuromod_system)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    system->neuromod_system = neuromod_system;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    LOG_MODULE_INFO(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Connected to neuromodulator system");

    return 0;
}

/* ============================================================================
 * Cytokine → Neuromodulator Implementation
 * ============================================================================ */

int neuromod_immune_apply_cytokine_effect(
    neuromod_immune_system_t* system,
    cytokine_type_t cytokine_type,
    float concentration)
{
    /* WHAT: Apply specific cytokine effect on neuromodulator synthesis
     * WHY:  Model biological cytokine-neurotransmitter interactions
     * HOW:  Modulate enzyme activity and precursor availability
     */

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return -1;


    }
    if (concentration < 0.0f || concentration > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_apply_cytokine_effect: validation failed");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    float effect_strength = system->cytokine_sensitivity * concentration;

    switch (cytokine_type) {
    case CYTOKINE_IL1B:
        /* IL-1β: Moderate suppression of DA/5-HT, increase NE */
        system->cytokine_effects.tyrosine_hydroxylase_activity *= (1.0f - 0.3f * effect_strength);
        system->cytokine_effects.tryptophan_hydroxylase_activity *= (1.0f - 0.4f * effect_strength);
        system->cytokine_effects.dopamine_synthesis_multiplier *= (1.0f - 0.3f * effect_strength);
        system->cytokine_effects.serotonin_synthesis_multiplier *= (1.0f - 0.4f * effect_strength);
        system->cytokine_effects.norepinephrine_synthesis_multiplier *= (1.0f + 0.2f * effect_strength);
        system->cytokine_effects.total_suppressions++;
        break;

    case CYTOKINE_IL6:
        /* IL-6: Strong suppression of DA/5-HT */
        system->cytokine_effects.tyrosine_hydroxylase_activity *= (1.0f - 0.4f * effect_strength);
        system->cytokine_effects.tryptophan_hydroxylase_activity *= (1.0f - 0.5f * effect_strength);
        system->cytokine_effects.dopamine_synthesis_multiplier *= (1.0f - 0.4f * effect_strength);
        system->cytokine_effects.serotonin_synthesis_multiplier *= (1.0f - 0.5f * effect_strength);
        system->cytokine_effects.total_suppressions++;
        break;

    case CYTOKINE_TNFA:
        /* TNF-α: Very strong suppression, increase MAO activity */
        system->cytokine_effects.tyrosine_hydroxylase_activity *= (1.0f - 0.9f * effect_strength);
        system->cytokine_effects.tryptophan_hydroxylase_activity *= (1.0f - 1.1f * effect_strength);
        system->cytokine_effects.dopamine_synthesis_multiplier *= (1.0f - 0.9f * effect_strength);
        system->cytokine_effects.serotonin_synthesis_multiplier *= (1.0f - 1.1f * effect_strength);
        system->cytokine_effects.mao_activity *= (1.0f + 0.5f * effect_strength);
        system->cytokine_effects.total_suppressions++;
        break;

    case CYTOKINE_IL10:
        /* IL-10: Anti-inflammatory, restores synthesis */
        system->cytokine_effects.tyrosine_hydroxylase_activity += 0.3f * effect_strength;
        system->cytokine_effects.tryptophan_hydroxylase_activity += 0.4f * effect_strength;
        system->cytokine_effects.dopamine_synthesis_multiplier += 0.2f * effect_strength;
        system->cytokine_effects.serotonin_synthesis_multiplier += 0.3f * effect_strength;
        /* Clamp to reasonable range */
        if (system->cytokine_effects.tyrosine_hydroxylase_activity > 1.2f)
            system->cytokine_effects.tyrosine_hydroxylase_activity = 1.2f;
        if (system->cytokine_effects.tryptophan_hydroxylase_activity > 1.2f)
            system->cytokine_effects.tryptophan_hydroxylase_activity = 1.2f;
        system->cytokine_effects.total_enhancements++;
        break;

    case CYTOKINE_TGFB:
        /* TGF-β: Anti-inflammatory, similar to IL-10 */
        system->cytokine_effects.dopamine_synthesis_multiplier += 0.15f * effect_strength;
        system->cytokine_effects.serotonin_synthesis_multiplier += 0.2f * effect_strength;
        system->cytokine_effects.total_enhancements++;
        break;

    default:
        nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_apply_cytokine_effect: operation failed");
        return -1;
    }

    /* Clamp all multipliers to reasonable range [0.1, 1.5] */
    system->cytokine_effects.dopamine_synthesis_multiplier = fminf(1.5f, fmaxf(0.1f,
        system->cytokine_effects.dopamine_synthesis_multiplier));
    system->cytokine_effects.serotonin_synthesis_multiplier = fminf(1.5f, fmaxf(0.1f,
        system->cytokine_effects.serotonin_synthesis_multiplier));
    system->cytokine_effects.norepinephrine_synthesis_multiplier = fminf(1.5f, fmaxf(0.1f,
        system->cytokine_effects.norepinephrine_synthesis_multiplier));

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return 0;
}

int neuromod_immune_apply_proinflammatory_effect(
    neuromod_immune_system_t* system,
    float severity)
{
    /* WHAT: Apply generalized pro-inflammatory effect
     * WHY:  Simplify application of multiple pro-inflammatory cytokines
     * HOW:  Suppress synthesis, reduce precursor availability
     */

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return -1;


    }
    if (severity < 0.0f || severity > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_apply_proinflammatory_effect: validation failed");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    /* Suppress enzyme activity */
    float enzyme_suppression = 0.4f * severity;
    system->cytokine_effects.tyrosine_hydroxylase_activity *= (1.0f - enzyme_suppression);
    system->cytokine_effects.tryptophan_hydroxylase_activity *= (1.0f - enzyme_suppression * 1.2f);

    /* Reduce precursor availability */
    float precursor_reduction = 0.3f * severity;
    system->cytokine_effects.tyrosine_availability *= (1.0f - precursor_reduction);
    system->cytokine_effects.tryptophan_availability *= (1.0f - precursor_reduction * 1.5f);

    /* Increase degradation */
    float degradation_increase = 0.2f * severity;
    system->cytokine_effects.mao_activity *= (1.0f + degradation_increase);
    system->cytokine_effects.comt_activity *= (1.0f + degradation_increase * 0.8f);

    /* Update synthesis multipliers */
    system->cytokine_effects.dopamine_synthesis_multiplier *= (1.0f - 0.45f * severity);
    system->cytokine_effects.serotonin_synthesis_multiplier *= (1.0f - 0.55f * severity);
    system->cytokine_effects.norepinephrine_synthesis_multiplier *= (1.0f + 0.15f * severity);

    system->cytokine_effects.total_suppressions++;
    system->cytokine_effects.avg_suppression_magnitude =
        (system->cytokine_effects.avg_suppression_magnitude * (system->cytokine_effects.total_suppressions - 1) + severity) /
        system->cytokine_effects.total_suppressions;

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    LOG_MODULE_DEBUG(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Applied pro-inflammatory effect: severity=%.2f", severity);

    return 0;
}

int neuromod_immune_apply_antiinflammatory_effect(
    neuromod_immune_system_t* system,
    float il10_concentration)
{
    /* WHAT: Apply IL-10 restorative effect
     * WHY:  Counter pro-inflammatory suppression
     * HOW:  Restore enzyme activity and synthesis rates
     */

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return -1;


    }
    if (il10_concentration < 0.0f || il10_concentration > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_apply_antiinflammatory_effect: validation failed");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    float restoration = 0.3f * il10_concentration;

    /* Restore enzyme activity toward 1.0 */
    system->cytokine_effects.tyrosine_hydroxylase_activity += restoration *
        (1.0f - system->cytokine_effects.tyrosine_hydroxylase_activity);
    system->cytokine_effects.tryptophan_hydroxylase_activity += restoration *
        (1.0f - system->cytokine_effects.tryptophan_hydroxylase_activity);

    /* Restore precursor availability */
    system->cytokine_effects.tyrosine_availability += restoration *
        (1.0f - system->cytokine_effects.tyrosine_availability);
    system->cytokine_effects.tryptophan_availability += restoration *
        (1.0f - system->cytokine_effects.tryptophan_availability);

    /* Normalize degradation */
    system->cytokine_effects.mao_activity -= restoration *
        (system->cytokine_effects.mao_activity - 1.0f);
    system->cytokine_effects.comt_activity -= restoration *
        (system->cytokine_effects.comt_activity - 1.0f);

    /* Restore synthesis multipliers */
    system->cytokine_effects.dopamine_synthesis_multiplier += restoration *
        (1.0f - system->cytokine_effects.dopamine_synthesis_multiplier);
    system->cytokine_effects.serotonin_synthesis_multiplier += restoration *
        (1.0f - system->cytokine_effects.serotonin_synthesis_multiplier);

    system->cytokine_effects.total_enhancements++;

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    LOG_MODULE_DEBUG(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Applied anti-inflammatory effect: IL-10=%.2f", il10_concentration);

    return 0;
}

/* ============================================================================
 * Neuromodulator → Immune Implementation
 * ============================================================================ */

int neuromod_immune_detect_imbalance(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t** imbalance_out)
{
    /* WHAT: Detect if neuromodulator levels are imbalanced
     * WHY:  Early detection allows immune intervention
     * HOW:  Compare current to baseline, check duration
     */

    if (!system || !imbalance_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_detect_imbalance: required parameter is NULL (system, imbalance_out)");
        return -1;
    }
    if (!system->neuromod_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_detect_imbalance: system->neuromod_system is NULL");
        return -1;
    }

    *imbalance_out = NULL;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    /* Query current levels */
    float da_current = neuromodulator_get_level(system->neuromod_system, NEUROMOD_DOPAMINE);
    float sert_current = neuromodulator_get_level(system->neuromod_system, NEUROMOD_SEROTONIN);
    float ne_current = neuromodulator_get_level(system->neuromod_system, NEUROMOD_NOREPINEPHRINE);
    float ach_current = neuromodulator_get_level(system->neuromod_system, NEUROMOD_ACETYLCHOLINE);

    /* Compute deviations */
    float da_dev = compute_deviation(da_current, system->dopamine_baseline);
    float sert_dev = compute_deviation(sert_current, system->serotonin_baseline);
    float ne_dev = compute_deviation(ne_current, system->norepinephrine_baseline);
    float ach_dev = compute_deviation(ach_current, system->acetylcholine_baseline);

    /* Check if any imbalance exists */
    neuromod_imbalance_type_t type = classify_imbalance(
        da_dev, sert_dev, ne_dev, ach_dev, system->imbalance_detection_threshold);

    if (type == NEUROMOD_IMBALANCE_NONE) {
        nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_detect_imbalance: validation failed");
        return -1;  /* No imbalance */
    }

    /* Check if we already have this imbalance tracked */
    for (size_t i = 0; i < system->imbalance_count; i++) {
        if (system->imbalances[i].type == type && !system->imbalances[i].corrective_action_taken) {
            *imbalance_out = &system->imbalances[i];
            nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);
            return 0;  /* Existing imbalance */
        }
    }

    /* Create new imbalance entry */
    if (system->imbalance_count >= system->imbalance_capacity) {
        /* Grow array */
        size_t new_capacity = system->imbalance_capacity * 2;
        neuromod_imbalance_t* new_array = (neuromod_imbalance_t*)nimcp_malloc(
            new_capacity * sizeof(neuromod_imbalance_t));
        if (!new_array) {
            nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_immune_detect_imbalance: new_array is NULL");
            return -1;
        }
        memcpy(new_array, system->imbalances,
               system->imbalance_count * sizeof(neuromod_imbalance_t));
        nimcp_free(system->imbalances);
        system->imbalances = new_array;
        system->imbalance_capacity = new_capacity;
    }

    neuromod_imbalance_t* imbalance = &system->imbalances[system->imbalance_count++];
    memset(imbalance, 0, sizeof(neuromod_imbalance_t));

    imbalance->id = system->next_imbalance_id++;
    imbalance->type = type;
    imbalance->dopamine_deviation = da_dev;
    imbalance->serotonin_deviation = sert_dev;
    imbalance->norepinephrine_deviation = ne_dev;
    imbalance->acetylcholine_deviation = ach_dev;

    /* Compute severity (max absolute deviation) */
    imbalance->severity = fmaxf(fabsf(da_dev), fmaxf(fabsf(sert_dev),
        fmaxf(fabsf(ne_dev), fabsf(ach_dev))));

    imbalance->onset_time = 0;  /* Will be set properly in update loop */
    imbalance->duration_ms = 0;
    imbalance->immune_alerted = false;
    imbalance->corrective_action_taken = false;

    *imbalance_out = imbalance;
    system->total_imbalances_detected++;

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    LOG_MODULE_INFO(NEUROMOD_IMMUNE_MODULE_NAME,
              "Detected neuromodulator imbalance: type=%s severity=%.2f",
              neuromod_immune_imbalance_to_string(type), imbalance->severity);

    return 0;
}

int neuromod_immune_alert_imbalance(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t* imbalance,
    uint32_t* antigen_id_out)
{
    /* WHAT: Alert immune system of neuromodulator imbalance
     * WHY:  Trigger immune response to restore homeostasis
     * HOW:  Present imbalance as antigen to brain immune system
     */

    if (!system || !imbalance || !antigen_id_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_alert_imbalance: required parameter is NULL (system, imbalance, antigen_id_out)");
        return -1;
    }
    if (!system->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_alert_imbalance: system->immune_system is NULL");
        return -1;
    }
    if (imbalance->immune_alerted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_alert_imbalance: validation failed");
        return -1;
    }

    /* Create epitope from imbalance signature */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));

    /* Encode imbalance type and deviations into epitope */
    epitope[0] = (uint8_t)imbalance->type;
    memcpy(&epitope[1], &imbalance->dopamine_deviation, sizeof(float));
    memcpy(&epitope[5], &imbalance->serotonin_deviation, sizeof(float));
    memcpy(&epitope[9], &imbalance->norepinephrine_deviation, sizeof(float));
    memcpy(&epitope[13], &imbalance->acetylcholine_deviation, sizeof(float));

    uint32_t severity = (uint32_t)(imbalance->severity * 10.0f);
    if (severity > 10) severity = 10;

    int result = brain_immune_present_antigen(
        system->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(epitope),
        severity,
        0,  /* source_node */
        antigen_id_out
    );

    if (result == 0) {
        imbalance->immune_alerted = true;
        imbalance->antigen_id = *antigen_id_out;
        system->total_immune_alerts++;

        LOG_MODULE_INFO(NEUROMOD_IMMUNE_MODULE_NAME,
                  "Alerted immune system of imbalance: type=%s antigen_id=%u",
                  neuromod_immune_imbalance_to_string(imbalance->type), *antigen_id_out);
    }

    return result;
}

int neuromod_immune_correct_imbalance(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t* imbalance)
{
    /* WHAT: Apply homeostatic correction to imbalance
     * WHY:  Restore neuromodulator balance
     * HOW:  Generate corrective immune response
     *
     * NOTE: Read imbalance type under lock, then release before calling
     *       apply_*_effect() to avoid deadlock (those functions lock mutex)
     */

    if (!system || !imbalance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_correct_imbalance: required parameter is NULL (system, imbalance)");
        return -1;
    }
    if (imbalance->corrective_action_taken) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_correct_imbalance: validation failed");
        return -1;
    }

    /* Read imbalance type under lock */
    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    neuromod_imbalance_type_t type = imbalance->type;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    /* Apply correction without holding lock (sub-functions acquire it) */
    switch (type) {
    case NEUROMOD_IMBALANCE_DA_EXCESS:
        /* Suppress dopamine via anti-inflammatory response */
        neuromod_immune_apply_proinflammatory_effect(system, 0.3f);
        break;

    case NEUROMOD_IMBALANCE_DA_DEFICIENCY:
        /* Enhance dopamine via IL-10 */
        neuromod_immune_apply_antiinflammatory_effect(system, 0.5f);
        break;

    case NEUROMOD_IMBALANCE_5HT_DEFICIENCY:
        /* Most common: restore serotonin synthesis */
        neuromod_immune_apply_antiinflammatory_effect(system, 0.6f);
        break;

    case NEUROMOD_IMBALANCE_NE_EXCESS:
        /* Reduce stress response */
        neuromod_immune_apply_proinflammatory_effect(system, 0.2f);
        break;

    default:
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_immune_correct_imbalance: operation failed");
        return -1;
    }

    /* Mark correction under lock */
    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    imbalance->corrective_action_taken = true;
    system->total_corrections++;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    LOG_MODULE_INFO(NEUROMOD_IMMUNE_MODULE_NAME,
              "Applied homeostatic correction for imbalance: type=%s",
              neuromod_immune_imbalance_to_string(type));

    return 0;
}

/* ============================================================================
 * Update and Query Implementation
 * ============================================================================ */

int neuromod_immune_update(
    neuromod_immune_system_t* system,
    uint64_t delta_ms)
{
    /* WHAT: Update neuromodulator-immune integration
     * WHY:  Process cytokine effects and detect imbalances
     * HOW:  Query immune system, apply effects, detect imbalances
     */

    if (!system) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");


        return -1;


    }
    if (!system->running) system->running = true;

    float dt_sec = delta_ms / 1000.0f;

    /* 1. Query immune system for cytokine levels (no lock needed for read) */
    float inflammation_level = 0.0f;
    if (system->immune_system) {
        brain_immune_stats_t immune_stats;
        if (brain_immune_get_stats(system->immune_system, &immune_stats) == 0) {
            /* Map inflammation sites to cytokine concentration */
            inflammation_level = (float)immune_stats.inflammation_sites / 10.0f;
            if (inflammation_level > 1.0f) inflammation_level = 1.0f;
        }
    }

    /* Apply pro-inflammatory effects (acquires its own lock) */
    if (inflammation_level > 0.1f) {
        neuromod_immune_apply_proinflammatory_effect(system, inflammation_level);
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    /* 2. Update metabolic states with cytokine-modified synthesis rates */
    float da_synth_mod = system->cytokine_effects.dopamine_synthesis_multiplier *
                         system->cytokine_effects.tyrosine_hydroxylase_activity;
    float sert_synth_mod = system->cytokine_effects.serotonin_synthesis_multiplier *
                           system->cytokine_effects.tryptophan_hydroxylase_activity;

    metabolic_set_enzyme_activity(&system->dopamine_metabolism, da_synth_mod);
    metabolic_set_enzyme_activity(&system->serotonin_metabolism, sert_synth_mod);

    /* Update metabolic states */
    metabolic_update(&system->dopamine_metabolism, dt_sec, 0.0f);
    metabolic_update(&system->serotonin_metabolism, dt_sec, 0.0f);
    metabolic_update(&system->norepinephrine_metabolism, dt_sec, 0.0f);
    metabolic_update(&system->acetylcholine_metabolism, dt_sec, 0.0f);

    /* 3. Update phasic-tonic states */
    uint64_t current_time_us = delta_ms * 1000;
    phasic_tonic_update(&system->dopamine_phasic, dt_sec, current_time_us);
    phasic_tonic_update(&system->serotonin_phasic, dt_sec, current_time_us);
    phasic_tonic_update(&system->norepinephrine_phasic, dt_sec, current_time_us);

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    /* 4. Detect imbalances */
    neuromod_imbalance_t* imbalance = NULL;
    if (neuromod_immune_detect_imbalance(system, &imbalance) == 0 && imbalance) {
        /* Update duration */
        imbalance->duration_ms += delta_ms;

        /* Alert immune system if threshold exceeded */
        if (!imbalance->immune_alerted &&
            imbalance->severity >= system->immune_alert_threshold &&
            imbalance->duration_ms >= 5000) {
            uint32_t antigen_id;
            neuromod_immune_alert_imbalance(system, imbalance, &antigen_id);
        }
    }

    return 0;
}

int neuromod_immune_get_cytokine_effects(
    neuromod_immune_system_t* system,
    cytokine_neuromod_effects_t* effects_out)
{
    if (!system || !effects_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_get_cytokine_effects: required parameter is NULL (system, effects_out)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    memcpy(effects_out, &system->cytokine_effects, sizeof(cytokine_neuromod_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return 0;
}

int neuromod_immune_get_imbalances(
    neuromod_immune_system_t* system,
    neuromod_imbalance_t* imbalances_out,
    size_t count_in,
    size_t* count_out)
{
    if (!system || !imbalances_out || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_immune_get_imbalances: required parameter is NULL (system, imbalances_out, count_out)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    size_t count = system->imbalance_count < count_in ?
                   system->imbalance_count : count_in;
    memcpy(imbalances_out, system->imbalances, count * sizeof(neuromod_imbalance_t));
    *count_out = count;

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return 0;
}

bool neuromod_immune_is_balanced(
    neuromod_immune_system_t* system,
    neuromodulator_type_t type)
{
    if (!system || !system->neuromod_system) return true;

    float current = neuromodulator_get_level(system->neuromod_system, type);
    float baseline = 0.0f;

    switch (type) {
    case NEUROMOD_DOPAMINE:
        baseline = system->dopamine_baseline;
        break;
    case NEUROMOD_SEROTONIN:
        baseline = system->serotonin_baseline;
        break;
    case NEUROMOD_NOREPINEPHRINE:
        baseline = system->norepinephrine_baseline;
        break;
    case NEUROMOD_ACETYLCHOLINE:
        baseline = system->acetylcholine_baseline;
        break;
    default:
        return true;
    }

    float deviation = compute_deviation(current, baseline);
    return !is_imbalanced(deviation, system->imbalance_detection_threshold);
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* neuromod_immune_imbalance_to_string(neuromod_imbalance_type_t type) {
    switch (type) {
    case NEUROMOD_IMBALANCE_NONE: return "NONE";
    case NEUROMOD_IMBALANCE_DA_EXCESS: return "DA_EXCESS";
    case NEUROMOD_IMBALANCE_DA_DEFICIENCY: return "DA_DEFICIENCY";
    case NEUROMOD_IMBALANCE_5HT_EXCESS: return "5HT_EXCESS";
    case NEUROMOD_IMBALANCE_5HT_DEFICIENCY: return "5HT_DEFICIENCY";
    case NEUROMOD_IMBALANCE_NE_EXCESS: return "NE_EXCESS";
    case NEUROMOD_IMBALANCE_NE_DEFICIENCY: return "NE_DEFICIENCY";
    case NEUROMOD_IMBALANCE_ACH_EXCESS: return "ACH_EXCESS";
    case NEUROMOD_IMBALANCE_ACH_DEFICIENCY: return "ACH_DEFICIENCY";
    default: return "UNKNOWN";
    }
}

const char* neuromod_immune_cytokine_effect_to_string(cytokine_effect_type_t effect) {
    switch (effect) {
    case CYTOKINE_EFFECT_NONE: return "NONE";
    case CYTOKINE_EFFECT_SUPPRESS_SYNTHESIS: return "SUPPRESS_SYNTHESIS";
    case CYTOKINE_EFFECT_ENHANCE_SYNTHESIS: return "ENHANCE_SYNTHESIS";
    case CYTOKINE_EFFECT_INCREASE_CLEARANCE: return "INCREASE_CLEARANCE";
    case CYTOKINE_EFFECT_BLOCK_RECEPTORS: return "BLOCK_RECEPTORS";
    default: return "UNKNOWN";
    }
}
