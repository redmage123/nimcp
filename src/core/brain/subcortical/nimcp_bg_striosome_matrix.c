//=============================================================================
// nimcp_bg_striosome_matrix.c - Striosome-Matrix Implementation
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_striosome_matrix.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bgsm, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


//=============================================================================
// Internal Structure
//=============================================================================

struct bgsm_system {
    /* Striosomes */
    bgsm_striosome_t* striosomes;
    uint32_t num_striosomes;

    /* Matrix zones */
    bgsm_matrix_zone_t* matrix_zones;
    uint32_t num_matrix_zones;

    /* State */
    float snc_modulation;           /**< Current SNc modulation signal */
    float dopamine_level;           /**< Current dopamine level */
    float aggregate_motivation;     /**< Aggregate motivation signal */

    /* Configuration */
    bgsm_config_t config;

    /* Statistics */
    bgsm_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

void bgsm_default_config(bgsm_config_t* config) {
    if (!config) return;

    config->num_striosomes = 16;
    config->num_matrix_zones = 32;
    config->striosome_ratio = BGSM_STRIOSOME_RATIO;
    config->snc_modulation_gain = 1.0f;
    config->matrix_da_sensitivity = 1.0f;
    config->enable_lateral_inhibition = true;
    config->boundary_interaction = 0.1f;
}

bgsm_system_t* bgsm_create(const bgsm_config_t* config) {
    bgsm_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        bgsm_default_config(&cfg);
    }

    bgsm_system_t* system = nimcp_calloc(1, sizeof(bgsm_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bgsm_system");
        return NULL;
    }

    system->config = cfg;
    system->num_striosomes = cfg.num_striosomes;
    system->num_matrix_zones = cfg.num_matrix_zones;
    system->dopamine_level = 0.5f;

    /* Allocate striosomes */
    system->striosomes = nimcp_calloc(cfg.num_striosomes, sizeof(bgsm_striosome_t));
    if (!system->striosomes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate striosomes");
        nimcp_free(system);
        return NULL;
    }

    /* Initialize striosomes */
    for (uint32_t i = 0; i < cfg.num_striosomes; i++) {
        system->striosomes[i].id = i;
        system->striosomes[i].baseline = 0.2f;
        system->striosomes[i].activation = 0.2f;
        system->striosomes[i].state = BGSM_STATE_BASELINE;
        system->striosomes[i].limbic_weight = 0.6f;  /* Primary motivation input */
        system->striosomes[i].mpfc_weight = 0.4f;
        system->striosomes[i].amygdala_weight = 0.3f;
        system->striosomes[i].hippocampus_weight = 0.3f;
        system->striosomes[i].snc_weight = BGSM_DEFAULT_SNc_WEIGHT;
        system->striosomes[i].value_estimate = 1.0f;  /* Default full value */
    }

    /* Allocate matrix zones */
    system->matrix_zones = nimcp_calloc(cfg.num_matrix_zones, sizeof(bgsm_matrix_zone_t));
    if (!system->matrix_zones) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate matrix zones");
        nimcp_free(system->striosomes);
        nimcp_free(system);
        return NULL;
    }

    /* Initialize matrix zones */
    for (uint32_t i = 0; i < cfg.num_matrix_zones; i++) {
        system->matrix_zones[i].id = i;
        system->matrix_zones[i].action_id = i;
        system->matrix_zones[i].baseline = 0.1f;
        system->matrix_zones[i].motor_weight = 0.4f;
        system->matrix_zones[i].premotor_weight = 0.3f;
        system->matrix_zones[i].sma_weight = 0.3f;
    }

    /* Create mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (!system->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex");
        nimcp_free(system->matrix_zones);
        nimcp_free(system->striosomes);
        nimcp_free(system);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created striosome-matrix system: %u striosomes, %u matrix zones",
                       cfg.num_striosomes, cfg.num_matrix_zones);

    return system;
}

void bgsm_destroy(bgsm_system_t* system) {
    if (!system) return;

    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }
    nimcp_free(system->striosomes);
    nimcp_free(system->matrix_zones);
    nimcp_free(system);

    NIMCP_LOGGING_DEBUG("Destroyed striosome-matrix system");
}

int bgsm_reset(bgsm_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);

    /* Reset striosomes */
    for (uint32_t i = 0; i < system->num_striosomes; i++) {
        system->striosomes[i].activation = system->striosomes[i].baseline;
        system->striosomes[i].state = BGSM_STATE_BASELINE;
        system->striosomes[i].snc_output = 0.0f;
        system->striosomes[i].value_estimate = 0.0f;
        system->striosomes[i].motivation_signal = 0.0f;
    }

    /* Reset matrix zones */
    for (uint32_t i = 0; i < system->num_matrix_zones; i++) {
        system->matrix_zones[i].d1_activation = system->matrix_zones[i].baseline;
        system->matrix_zones[i].d2_activation = system->matrix_zones[i].baseline;
        system->matrix_zones[i].gpi_output = 0.0f;
        system->matrix_zones[i].gpe_output = 0.0f;
        system->matrix_zones[i].snr_output = 0.0f;
    }

    system->snc_modulation = 0.0f;
    system->dopamine_level = 0.5f;
    system->aggregate_motivation = 0.5f;
    memset(&system->stats, 0, sizeof(bgsm_stats_t));

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

//=============================================================================
// Striosome Functions
//=============================================================================

int bgsm_set_striosome_input(bgsm_system_t* system,
                              bgsm_striosome_input_t source,
                              const float* values) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!values, NIMCP_ERROR_NULL_POINTER, "values is NULL");
    if (!system || !values) return -1;

    nimcp_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->num_striosomes; i++) {
        float weight = 0.0f;
        switch (source) {
            case BGSM_INPUT_LIMBIC:
                weight = system->striosomes[i].limbic_weight;
                break;
            case BGSM_INPUT_MPFC:
                weight = system->striosomes[i].mpfc_weight;
                break;
            case BGSM_INPUT_AMYGDALA:
                weight = system->striosomes[i].amygdala_weight;
                break;
            case BGSM_INPUT_HIPPOCAMPUS:
                weight = system->striosomes[i].hippocampus_weight;
                break;
        }
        /* Accumulate weighted input */
        system->striosomes[i].activation += values[i] * weight;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsm_process_striosomes(bgsm_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);
    bgsm_heartbeat("process_striosomes", 0.0f);

    float total_snc = 0.0f;
    float total_motivation = 0.0f;
    uint32_t burst_count = 0;

    for (uint32_t i = 0; i < system->num_striosomes; i++) {
        bgsm_striosome_t* s = &system->striosomes[i];

        /* Clamp activation */
        s->activation = fmaxf(0.0f, fminf(1.0f, s->activation));

        /* Determine state based on activation */
        if (s->activation > 0.8f) {
            s->state = BGSM_STATE_BURST;
            burst_count++;
        } else if (s->activation > s->baseline + 0.2f) {
            s->state = BGSM_STATE_ACTIVATED;
        } else if (s->activation < s->baseline - 0.1f) {
            s->state = BGSM_STATE_SUPPRESSED;
        } else {
            s->state = BGSM_STATE_BASELINE;
        }

        /* Compute SNc output (striosomes INHIBIT dopamine neurons by default,
           so high striosome = lower dopamine, but we model the deviation) */
        s->snc_output = (s->activation - s->baseline) * s->snc_weight;
        total_snc += s->snc_output;

        /* Motivation signal is related to value estimate */
        s->motivation_signal = s->value_estimate * s->activation;
        total_motivation += s->motivation_signal;

        /* Decay activation toward baseline */
        s->activation = s->activation * 0.95f + s->baseline * 0.05f;
    }

    /* Aggregate SNc modulation */
    system->snc_modulation = total_snc / (float)system->num_striosomes;
    system->snc_modulation *= system->config.snc_modulation_gain;
    system->snc_modulation = fmaxf(-1.0f, fminf(1.0f, system->snc_modulation));

    /* Aggregate motivation */
    system->aggregate_motivation = total_motivation / (float)system->num_striosomes;
    system->aggregate_motivation = fmaxf(0.0f, fminf(1.0f, system->aggregate_motivation));

    /* Update stats */
    system->stats.striosome_bursts += burst_count;
    system->stats.snc_modulation_strength = fabsf(system->snc_modulation);

    bgsm_heartbeat("process_striosomes", 1.0f);
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

float bgsm_get_snc_modulation(const bgsm_system_t* system) {
    if (!system) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float mod = system->snc_modulation;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return mod;
}

float bgsm_get_striosome_activation(const bgsm_system_t* system, uint32_t striosome_id) {
    if (!system || striosome_id >= system->num_striosomes) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float act = system->striosomes[striosome_id].activation;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return act;
}

float bgsm_get_motivation(const bgsm_system_t* system) {
    if (!system) return 0.5f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float mot = system->aggregate_motivation;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return mot;
}

//=============================================================================
// Matrix Functions
//=============================================================================

int bgsm_set_matrix_input(bgsm_system_t* system,
                           bgsm_matrix_input_t source,
                           const float* values) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!values, NIMCP_ERROR_NULL_POINTER, "values is NULL");
    if (!system || !values) return -1;

    nimcp_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->num_matrix_zones; i++) {
        float weight = 0.0f;
        switch (source) {
            case BGSM_INPUT_MOTOR:
                weight = system->matrix_zones[i].motor_weight;
                break;
            case BGSM_INPUT_PREMOTOR:
                weight = system->matrix_zones[i].premotor_weight;
                break;
            case BGSM_INPUT_SMA:
                weight = system->matrix_zones[i].sma_weight;
                break;
            case BGSM_INPUT_SOMATOSENSORY:
                weight = 0.2f;  /* Default for somatosensory */
                break;
        }
        /* Input drives both D1 and D2 initially */
        float input = values[i] * weight;
        system->matrix_zones[i].d1_activation += input;
        system->matrix_zones[i].d2_activation += input;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsm_set_matrix_dopamine(bgsm_system_t* system, float dopamine) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);
    system->dopamine_level = fmaxf(0.0f, fminf(1.0f, dopamine));
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

int bgsm_process_matrix(bgsm_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);
    bgsm_heartbeat("process_matrix", 0.0f);

    float da = system->dopamine_level;
    float da_sens = system->config.matrix_da_sensitivity;
    float total_d1 = 0.0f, total_d2 = 0.0f;

    for (uint32_t i = 0; i < system->num_matrix_zones; i++) {
        bgsm_matrix_zone_t* m = &system->matrix_zones[i];

        /* Dopamine modulation:
           - D1 (direct): Enhanced by dopamine
           - D2 (indirect): Suppressed by dopamine */
        m->d1_activation *= (1.0f + (da - 0.5f) * da_sens);
        m->d2_activation *= (1.0f - (da - 0.5f) * da_sens);

        /* Clamp */
        m->d1_activation = fmaxf(0.0f, fminf(1.0f, m->d1_activation));
        m->d2_activation = fmaxf(0.0f, fminf(1.0f, m->d2_activation));

        /* Compute outputs to GP/SNr
           D1 → GPi (inhibitory, so high D1 = low GPi output = disinhibition)
           D2 → GPe (inhibitory) */
        m->gpi_output = 1.0f - m->d1_activation;  /* Disinhibition */
        m->gpe_output = 1.0f - m->d2_activation;
        m->snr_output = m->gpi_output;  /* SNr similar to GPi */

        total_d1 += m->d1_activation;
        total_d2 += m->d2_activation;

        /* Decay toward baseline */
        m->d1_activation = m->d1_activation * 0.9f + m->baseline * 0.1f;
        m->d2_activation = m->d2_activation * 0.9f + m->baseline * 0.1f;
    }

    /* Update stats */
    system->stats.avg_matrix_activation = (total_d1 + total_d2) /
                                           (2.0f * system->num_matrix_zones);

    bgsm_heartbeat("process_matrix", 1.0f);
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

float bgsm_get_d1_output(const bgsm_system_t* system, uint32_t action_id) {
    if (!system || action_id >= system->num_matrix_zones) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float d1 = system->matrix_zones[action_id].d1_activation;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return d1;
}

float bgsm_get_d2_output(const bgsm_system_t* system, uint32_t action_id) {
    if (!system || action_id >= system->num_matrix_zones) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float d2 = system->matrix_zones[action_id].d2_activation;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return d2;
}

int bgsm_get_all_d1_output(const bgsm_system_t* system, float* output) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!output, NIMCP_ERROR_NULL_POINTER, "output is NULL");
    if (!system || !output) return -1;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    for (uint32_t i = 0; i < system->num_matrix_zones; i++) {
        output[i] = system->matrix_zones[i].d1_activation;
    }
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return 0;
}

int bgsm_get_all_d2_output(const bgsm_system_t* system, float* output) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!output, NIMCP_ERROR_NULL_POINTER, "output is NULL");
    if (!system || !output) return -1;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    for (uint32_t i = 0; i < system->num_matrix_zones; i++) {
        output[i] = system->matrix_zones[i].d2_activation;
    }
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return 0;
}

//=============================================================================
// Interaction Functions
//=============================================================================

int bgsm_apply_striosome_modulation(bgsm_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);

    /* Striosome motivation gates matrix output */
    float motivation = system->aggregate_motivation;

    for (uint32_t i = 0; i < system->num_matrix_zones; i++) {
        /* Higher motivation amplifies action */
        system->matrix_zones[i].d1_activation *= (0.5f + motivation);
        /* Lower motivation suppresses competing actions */
        system->matrix_zones[i].d2_activation *= (1.5f - motivation);

        /* Clamp */
        system->matrix_zones[i].d1_activation =
            fmaxf(0.0f, fminf(1.0f, system->matrix_zones[i].d1_activation));
        system->matrix_zones[i].d2_activation =
            fmaxf(0.0f, fminf(1.0f, system->matrix_zones[i].d2_activation));
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsm_process_boundary(bgsm_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    if (!system->config.enable_lateral_inhibition) return 0;

    nimcp_mutex_lock(system->mutex);

    float interaction = system->config.boundary_interaction;
    float spillover = 0.0f;

    /* Simplified boundary: adjacent striosomes/matrix interact */
    for (uint32_t i = 0; i < system->num_striosomes && i < system->num_matrix_zones; i++) {
        float striosome_act = system->striosomes[i].activation;
        float matrix_act = (system->matrix_zones[i].d1_activation +
                           system->matrix_zones[i].d2_activation) / 2.0f;

        /* Cross-compartment influence */
        float cross = (striosome_act - matrix_act) * interaction;
        spillover += fabsf(cross);

        /* Apply weak interaction */
        system->matrix_zones[i].d1_activation += cross * 0.1f;
        system->striosomes[i].activation -= cross * 0.05f;

        /* Clamp */
        system->matrix_zones[i].d1_activation =
            fmaxf(0.0f, fminf(1.0f, system->matrix_zones[i].d1_activation));
        system->striosomes[i].activation =
            fmaxf(0.0f, fminf(1.0f, system->striosomes[i].activation));
    }

    system->stats.boundary_spillover = spillover / (float)system->num_striosomes;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int bgsm_step(bgsm_system_t* system, float dt_ms) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    (void)dt_ms;  /* Time-based decay already in process functions */

    bgsm_process_striosomes(system);
    bgsm_process_matrix(system);
    bgsm_apply_striosome_modulation(system);
    bgsm_process_boundary(system);

    return 0;
}

int bgsm_process(bgsm_system_t* system) {
    return bgsm_step(system, 1.0f);
}

//=============================================================================
// Statistics Functions
//=============================================================================

int bgsm_get_stats(const bgsm_system_t* system, bgsm_stats_t* stats) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    if (!system || !stats) return -1;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    /* Calculate average striosome activation */
    float total = 0.0f;
    for (uint32_t i = 0; i < system->num_striosomes; i++) {
        total += system->striosomes[i].activation;
    }
    stats->avg_striosome_activation = total / (float)system->num_striosomes;

    stats->avg_matrix_activation = system->stats.avg_matrix_activation;
    stats->snc_modulation_strength = system->stats.snc_modulation_strength;
    stats->striosome_bursts = system->stats.striosome_bursts;
    stats->boundary_spillover = system->stats.boundary_spillover;

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);
    return 0;
}

const char* bgsm_compartment_name(bgsm_compartment_type_t type) {
    switch (type) {
        case BGSM_COMPARTMENT_STRIOSOME: return "Striosome";
        case BGSM_COMPARTMENT_MATRIX: return "Matrix";
        default: return "Unknown";
    }
}

const char* bgsm_state_name(bgsm_striosome_state_t state) {
    switch (state) {
        case BGSM_STATE_BASELINE: return "Baseline";
        case BGSM_STATE_ACTIVATED: return "Activated";
        case BGSM_STATE_SUPPRESSED: return "Suppressed";
        case BGSM_STATE_BURST: return "Burst";
        default: return "Unknown";
    }
}
