/**
 * @file nimcp_vestibular.c
 * @brief Vestibular nuclei processor implementation
 *
 * WHAT: Processes vestibular signals for balance and gaze stabilization
 * WHY:  Enable VOR and vestibulospinal reflexes
 * HOW:  Models four vestibular nuclei and their output pathways
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2025-01-03
 */

#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(vestibular, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define VESTIBULAR_LOG_MODULE "VESTIBULAR"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal vestibular processor structure
 */
struct vestibular_processor {
    /* Configuration */
    vestibular_config_t config;

    /* Nuclei state */
    vestibular_nucleus_state_t nuclei[VESTIBULAR_NUM_NUCLEI];

    /* VOR state */
    vor_state_t vor;

    /* Vestibulospinal state */
    vestibulospinal_state_t vsr;

    /* Current input */
    vestibular_input_t current_input;

    /* Output signals */
    vestibular_mossy_signal_t mossy_signal;

    /* State */
    vestibular_status_t status;
    vestibular_error_t last_error;
    uint64_t current_time_us;

    /* Statistics */
    vestibular_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(vestibular_processor_t* p, vestibular_error_t error) {
    if (!p) return;
    p->last_error = error;
    if (error != VESTIBULAR_ERROR_NONE) {
        p->status = VESTIBULAR_STATUS_ERROR;
        LOG_ERROR("[%s] Error: %d", VESTIBULAR_LOG_MODULE, error);
    }
}

/**
 * @brief Initialize a single nucleus
 */
static void init_nucleus(vestibular_nucleus_state_t* nucleus,
                          vestibular_nucleus_type_t type,
                          uint32_t num_neurons) {
    memset(nucleus, 0, sizeof(*nucleus));
    nucleus->type = type;
    nucleus->num_neurons = num_neurons;
    nucleus->baseline_rate = 100.0f;  /* ~100 Hz baseline firing */
    nucleus->current_rate = 100.0f;
    nucleus->cerebellar_modulation = 1.0f;  /* No modulation */

    for (int i = 0; i < 3; i++) {
        nucleus->activity[i] = 0.0f;
        nucleus->velocity_storage[i] = 0.0f;
    }
}

/**
 * @brief Process velocity storage integration
 *
 * Velocity storage extends the time constant of vestibular response
 * from ~5s (peripheral) to ~15s (central)
 */
static void update_velocity_storage(vestibular_processor_t* p,
                                     float dt_s) {
    if (!p->config.enable_velocity_storage) return;

    float tau = p->config.velocity_storage_tau_s;
    if (tau <= 0.0f) tau = VESTIBULAR_VELOCITY_STORAGE_TAU;

    /* Decay factor */
    float decay = expf(-dt_s / tau);

    /* Update velocity storage in each nucleus */
    for (int n = 0; n < VESTIBULAR_NUM_NUCLEI; n++) {
        vestibular_nucleus_state_t* nucleus = &p->nuclei[n];

        for (int i = 0; i < 3; i++) {
            /* Velocity storage: integrates and decays */
            nucleus->velocity_storage[i] = nucleus->velocity_storage[i] * decay +
                                           nucleus->activity[i] * (1.0f - decay);
        }
    }
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

vestibular_config_t vestibular_default_config(void) {
    vestibular_config_t config;
    memset(&config, 0, sizeof(config));

    /* Nucleus neuron counts */
    config.mvn_neurons = VESTIBULAR_DEFAULT_MVN_NEURONS;
    config.lvn_neurons = VESTIBULAR_DEFAULT_LVN_NEURONS;
    config.svn_neurons = VESTIBULAR_DEFAULT_SVN_NEURONS;
    config.ivn_neurons = VESTIBULAR_DEFAULT_IVN_NEURONS;

    /* VOR parameters */
    config.initial_vor_gain = VESTIBULAR_DEFAULT_VOR_GAIN;
    config.vor_adaptation_rate = 0.01f;
    config.enable_vor_adaptation = true;
    config.vor_time_constant_ms = 10.0f;

    /* Velocity storage */
    config.velocity_storage_tau_s = VESTIBULAR_VELOCITY_STORAGE_TAU;
    config.enable_velocity_storage = true;

    /* Vestibulospinal */
    config.enable_vestibulospinal = true;
    config.vsr_gain = 1.0f;

    /* Cerebellar connection */
    config.enable_cerebellar_input = true;
    config.cerebellar_weight = 1.0f;

    return config;
}

vestibular_processor_t* vestibular_create(const vestibular_config_t* config) {
    LOG_INFO("[%s] Creating vestibular processor", VESTIBULAR_LOG_MODULE);

    vestibular_processor_t* p = nimcp_calloc(1, sizeof(vestibular_processor_t));
    if (!p) {
        LOG_ERROR("[%s] Failed to allocate processor", VESTIBULAR_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vestibular_create: p is NULL");
        return NULL;
    }

    /* Set configuration */
    p->config = config ? *config : vestibular_default_config();

    /* Initialize nuclei */
    init_nucleus(&p->nuclei[VESTIBULAR_NUCLEUS_MEDIAL],
                 VESTIBULAR_NUCLEUS_MEDIAL, p->config.mvn_neurons);
    init_nucleus(&p->nuclei[VESTIBULAR_NUCLEUS_LATERAL],
                 VESTIBULAR_NUCLEUS_LATERAL, p->config.lvn_neurons);
    init_nucleus(&p->nuclei[VESTIBULAR_NUCLEUS_SUPERIOR],
                 VESTIBULAR_NUCLEUS_SUPERIOR, p->config.svn_neurons);
    init_nucleus(&p->nuclei[VESTIBULAR_NUCLEUS_INFERIOR],
                 VESTIBULAR_NUCLEUS_INFERIOR, p->config.ivn_neurons);

    /* Initialize VOR */
    for (int i = 0; i < 3; i++) {
        p->vor.gain[i] = p->config.initial_vor_gain;
        p->vor.phase[i] = 0.0f;
        p->vor.eye_velocity[i] = 0.0f;
        p->vor.head_velocity[i] = 0.0f;
    }
    p->vor.retinal_slip = 0.0f;
    p->vor.adaptation_active = false;
    p->vor.adaptation_rate = p->config.vor_adaptation_rate;

    /* Initialize VSR */
    memset(&p->vsr, 0, sizeof(p->vsr));
    p->vsr.reflex_active = false;

    /* Initialize state */
    p->status = VESTIBULAR_STATUS_IDLE;
    p->last_error = VESTIBULAR_ERROR_NONE;
    p->current_time_us = 0;

    LOG_INFO("[%s] Vestibular processor created successfully", VESTIBULAR_LOG_MODULE);
    return p;
}

void vestibular_destroy(vestibular_processor_t* p) {
    if (!p) return;

    LOG_INFO("[%s] Destroying vestibular processor", VESTIBULAR_LOG_MODULE);
    nimcp_free(p);
}

bool vestibular_reset(vestibular_processor_t* p) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_reset: processor is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting processor", VESTIBULAR_LOG_MODULE);

    /* Reset nuclei */
    for (int n = 0; n < VESTIBULAR_NUM_NUCLEI; n++) {
        vestibular_nucleus_state_t* nucleus = &p->nuclei[n];
        nucleus->current_rate = nucleus->baseline_rate;
        nucleus->cerebellar_modulation = 1.0f;
        for (int i = 0; i < 3; i++) {
            nucleus->activity[i] = 0.0f;
            nucleus->velocity_storage[i] = 0.0f;
        }
    }

    /* Reset VOR */
    for (int i = 0; i < 3; i++) {
        p->vor.gain[i] = p->config.initial_vor_gain;
        p->vor.eye_velocity[i] = 0.0f;
        p->vor.head_velocity[i] = 0.0f;
    }
    p->vor.retinal_slip = 0.0f;

    /* Reset VSR */
    memset(&p->vsr, 0, sizeof(p->vsr));

    /* Reset state */
    p->status = VESTIBULAR_STATUS_IDLE;
    p->last_error = VESTIBULAR_ERROR_NONE;

    return true;
}

/*=============================================================================
 * INPUT PROCESSING
 *===========================================================================*/

bool vestibular_process_input(vestibular_processor_t* p,
                               const vestibular_input_t* input) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_process_input: processor is NULL");
        return false;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_process_input: input is NULL");
        set_error(p, VESTIBULAR_ERROR_INVALID_INPUT);
        return false;
    }

    p->status = VESTIBULAR_STATUS_PROCESSING;
    p->current_input = *input;

    /* Process canal input for angular velocity */
    if (input->canals_valid) {
        vestibular_process_canal_input(p, &input->canals);
    }

    /* Process otolith input for linear acceleration */
    if (input->otoliths_valid) {
        vestibular_process_otolith_input(p, &input->otoliths);
    }

    /* Update velocity storage */
    float dt_s = 0.001f;  /* Assume 1ms timestep */
    if (p->current_time_us > 0 && input->canals.timestamp_us > p->current_time_us) {
        dt_s = (float)(input->canals.timestamp_us - p->current_time_us) / 1000000.0f;
    }
    update_velocity_storage(p, dt_s);

    p->current_time_us = input->canals.timestamp_us;
    p->status = VESTIBULAR_STATUS_IDLE;

    return true;
}

bool vestibular_process_canal_input(vestibular_processor_t* p,
                                     const semicircular_canal_input_t* input) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_process_canal_input: processor is NULL");
        return false;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_process_canal_input: input is NULL");
        set_error(p, VESTIBULAR_ERROR_INVALID_INPUT);
        return false;
    }

    p->stats.canal_inputs++;

    /* Store head velocity */
    p->vor.head_velocity[0] = input->yaw_velocity;
    p->vor.head_velocity[1] = input->pitch_velocity;
    p->vor.head_velocity[2] = input->roll_velocity;

    /*
     * MVN (Medial Vestibular Nucleus): Horizontal VOR
     * Processes yaw (horizontal) head rotation
     */
    vestibular_nucleus_state_t* mvn = &p->nuclei[VESTIBULAR_NUCLEUS_MEDIAL];
    mvn->activity[0] = input->yaw_velocity;  /* Yaw sensitivity */
    mvn->activity[1] = input->pitch_velocity * 0.3f;  /* Some pitch sensitivity */
    mvn->activity[2] = 0.0f;

    /* Apply cerebellar modulation */
    for (int i = 0; i < 3; i++) {
        mvn->activity[i] *= mvn->cerebellar_modulation;
    }

    /* Update firing rate (baseline +/- modulation) */
    float yaw_mod = fabsf(input->yaw_velocity) * 50.0f;  /* ~50 Hz/rad/s gain */
    mvn->current_rate = mvn->baseline_rate + yaw_mod * mvn->cerebellar_modulation;

    /*
     * SVN (Superior Vestibular Nucleus): Vertical VOR
     * Processes pitch and roll (vertical) head rotation
     */
    vestibular_nucleus_state_t* svn = &p->nuclei[VESTIBULAR_NUCLEUS_SUPERIOR];
    svn->activity[0] = 0.0f;
    svn->activity[1] = input->pitch_velocity;  /* Pitch sensitivity */
    svn->activity[2] = input->roll_velocity;   /* Roll sensitivity */

    for (int i = 0; i < 3; i++) {
        svn->activity[i] *= svn->cerebellar_modulation;
    }

    float vertical_mod = fabsf(input->pitch_velocity) + fabsf(input->roll_velocity);
    svn->current_rate = svn->baseline_rate + vertical_mod * 40.0f * svn->cerebellar_modulation;

    /*
     * IVN (Inferior Vestibular Nucleus): Cerebellar integration
     * Receives input from nodulus, integrates with canals
     */
    vestibular_nucleus_state_t* ivn = &p->nuclei[VESTIBULAR_NUCLEUS_INFERIOR];
    ivn->activity[0] = input->yaw_velocity * 0.5f;
    ivn->activity[1] = input->pitch_velocity * 0.5f;
    ivn->activity[2] = input->roll_velocity * 0.5f;

    for (int i = 0; i < 3; i++) {
        ivn->activity[i] *= ivn->cerebellar_modulation;
    }

    /*
     * Compute VOR eye velocity command
     * Eye velocity = -gain * head velocity (opposite direction)
     */
    p->status = VESTIBULAR_STATUS_VOR_ACTIVE;

    for (int i = 0; i < 3; i++) {
        /* Get combined activity from MVN/SVN with velocity storage */
        float activity = 0.0f;
        if (i == 0) {  /* Yaw */
            activity = mvn->activity[0] + mvn->velocity_storage[0];
        } else {  /* Pitch, Roll */
            activity = svn->activity[i] + svn->velocity_storage[i];
        }

        /* VOR: eye velocity opposes head velocity */
        p->vor.eye_velocity[i] = -p->vor.gain[i] * activity;
    }

    p->stats.vor_commands++;

    /* Integrate head position from angular velocity */
    float dt_s = 0.001f;  /* Default timestep */
    if (p->current_time_us > 0 && input->timestamp_us > p->current_time_us) {
        dt_s = (float)(input->timestamp_us - p->current_time_us) / 1000000.0f;
    }
    p->vsr.head_position[0] += input->yaw_velocity * dt_s;
    p->vsr.head_position[1] += input->pitch_velocity * dt_s;
    p->vsr.head_position[2] += input->roll_velocity * dt_s;

    return true;
}

bool vestibular_process_otolith_input(vestibular_processor_t* p,
                                       const otolith_input_t* input) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_process_otolith_input: processor is NULL");
        return false;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_process_otolith_input: input is NULL");
        set_error(p, VESTIBULAR_ERROR_INVALID_INPUT);
        return false;
    }

    p->stats.otolith_inputs++;

    /*
     * LVN (Lateral Vestibular Nucleus): Vestibulospinal reflex
     * Processes gravity and linear acceleration for posture control
     */
    vestibular_nucleus_state_t* lvn = &p->nuclei[VESTIBULAR_NUCLEUS_LATERAL];

    /* Otolith signals (gravity + acceleration) */
    lvn->activity[0] = input->x_accel;  /* Forward/back */
    lvn->activity[1] = input->y_accel;  /* Left/right */
    lvn->activity[2] = input->z_accel - 9.81f;  /* Vertical (subtract gravity) */

    for (int i = 0; i < 3; i++) {
        lvn->activity[i] *= lvn->cerebellar_modulation;
    }

    /* Compute magnitude for firing rate */
    float accel_mag = sqrtf(lvn->activity[0] * lvn->activity[0] +
                            lvn->activity[1] * lvn->activity[1] +
                            lvn->activity[2] * lvn->activity[2]);
    lvn->current_rate = lvn->baseline_rate + accel_mag * 10.0f * lvn->cerebellar_modulation;

    /* Update vestibulospinal output if enabled */
    if (p->config.enable_vestibulospinal) {
        p->status = VESTIBULAR_STATUS_VSR_ACTIVE;

        /* Postural command proportional to tilt */
        p->vsr.body_tilt[0] = input->head_tilt_pitch;
        p->vsr.body_tilt[1] = input->head_tilt_roll;

        /* Generate postural correction command */
        p->vsr.postural_command[0] = -input->head_tilt_pitch * p->config.vsr_gain;
        p->vsr.postural_command[1] = -input->head_tilt_roll * p->config.vsr_gain;
        p->vsr.postural_command[2] = (input->z_accel - 9.81f) * 0.1f * p->config.vsr_gain;

        p->vsr.reflex_active = (fabsf(input->head_tilt_pitch) > 0.05f ||
                                fabsf(input->head_tilt_roll) > 0.05f);

        p->stats.vsr_commands++;
    }

    /* Update head position estimate */
    p->vsr.head_position[0] = 0.0f;  /* Yaw integration needs gyro */
    p->vsr.head_position[1] = input->head_tilt_pitch;
    p->vsr.head_position[2] = input->head_tilt_roll;

    return true;
}

/*=============================================================================
 * VOR FUNCTIONS
 *===========================================================================*/

bool vestibular_get_vor_command(const vestibular_processor_t* p,
                                 float eye_velocity[3]) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_get_vor_command: processor is NULL");
        return false;
    }
    if (!eye_velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_get_vor_command: eye_velocity is NULL");
        return false;
    }

    eye_velocity[0] = p->vor.eye_velocity[0];
    eye_velocity[1] = p->vor.eye_velocity[1];
    eye_velocity[2] = p->vor.eye_velocity[2];

    return true;
}

bool vestibular_set_vor_gain(vestibular_processor_t* p,
                              const float* gain,
                              bool per_axis) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_set_vor_gain: processor is NULL");
        return false;
    }
    if (!gain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_set_vor_gain: gain is NULL");
        return false;
    }

    if (per_axis) {
        for (int i = 0; i < 3; i++) {
            p->vor.gain[i] = nimcp_clampf(gain[i],
                                     VESTIBULAR_MIN_VOR_GAIN,
                                     VESTIBULAR_MAX_VOR_GAIN);
        }
    } else {
        float g = nimcp_clampf(gain[0], VESTIBULAR_MIN_VOR_GAIN, VESTIBULAR_MAX_VOR_GAIN);
        for (int i = 0; i < 3; i++) {
            p->vor.gain[i] = g;
        }
    }

    LOG_DEBUG("[%s] VOR gain set: [%.2f, %.2f, %.2f]",
              VESTIBULAR_LOG_MODULE, p->vor.gain[0], p->vor.gain[1], p->vor.gain[2]);

    return true;
}

bool vestibular_get_vor_gain(const vestibular_processor_t* p,
                              float gain[3]) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_get_vor_gain: processor is NULL");
        return false;
    }
    if (!gain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_get_vor_gain: gain is NULL");
        return false;
    }

    gain[0] = p->vor.gain[0];
    gain[1] = p->vor.gain[1];
    gain[2] = p->vor.gain[2];

    return true;
}

bool vestibular_report_retinal_slip(vestibular_processor_t* p,
                                     float retinal_slip,
                                     const float direction[3]) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_report_retinal_slip: processor is NULL");
        return false;
    }

    p->vor.retinal_slip = retinal_slip;

    if (!p->config.enable_vor_adaptation) {
        return true;
    }

    if (fabsf(retinal_slip) < 0.01f) {
        /* Small slip - no adaptation needed */
        return true;
    }

    p->status = VESTIBULAR_STATUS_CALIBRATING;
    p->vor.adaptation_active = true;

    /*
     * VOR gain adaptation:
     * - Retinal slip in same direction as head movement -> increase gain
     * - Retinal slip opposite to head movement -> decrease gain
     *
     * This is normally driven by cerebellum (flocculus LTD),
     * but we provide a simplified direct mechanism here.
     */
    float adapt_rate = p->vor.adaptation_rate * retinal_slip;

    for (int i = 0; i < 3; i++) {
        if (fabsf(direction[i]) > 0.1f && fabsf(p->vor.head_velocity[i]) > 0.01f) {
            /* Adapt gain in this axis */
            float sign = (direction[i] * p->vor.head_velocity[i] > 0) ? 1.0f : -1.0f;
            p->vor.gain[i] += adapt_rate * sign;
            p->vor.gain[i] = nimcp_clampf(p->vor.gain[i],
                                     VESTIBULAR_MIN_VOR_GAIN,
                                     VESTIBULAR_MAX_VOR_GAIN);
        }
    }

    p->stats.adaptation_events++;
    p->stats.avg_retinal_slip = p->stats.avg_retinal_slip * 0.95f + fabsf(retinal_slip) * 0.05f;

    /* Update stats */
    for (int i = 0; i < 3; i++) {
        p->stats.current_vor_gain[i] = p->vor.gain[i];
    }

    LOG_DEBUG("[%s] VOR adaptation: slip=%.3f, gain=[%.2f, %.2f, %.2f]",
              VESTIBULAR_LOG_MODULE, retinal_slip,
              p->vor.gain[0], p->vor.gain[1], p->vor.gain[2]);

    return true;
}

/*=============================================================================
 * VESTIBULOSPINAL FUNCTIONS
 *===========================================================================*/

bool vestibular_get_postural_command(const vestibular_processor_t* p,
                                      float postural_command[3]) {
    if (!p || !postural_command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (p, postural_command)");
        return false;
    }

    postural_command[0] = p->vsr.postural_command[0];
    postural_command[1] = p->vsr.postural_command[1];
    postural_command[2] = p->vsr.postural_command[2];

    return true;
}

bool vestibular_get_head_position(const vestibular_processor_t* p,
                                   float position[3]) {
    if (!p || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (p, position)");
        return false;
    }

    position[0] = p->vsr.head_position[0];
    position[1] = p->vsr.head_position[1];
    position[2] = p->vsr.head_position[2];

    return true;
}

/*=============================================================================
 * CEREBELLAR INTERFACE
 *===========================================================================*/

bool vestibular_get_mossy_signal(const vestibular_processor_t* p,
                                  vestibular_mossy_signal_t* signal) {
    if (!p || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (p, signal)");
        return false;
    }

    memset(signal, 0, sizeof(*signal));

    /* Copy head velocity from canals */
    signal->head_velocity[0] = p->vor.head_velocity[0];
    signal->head_velocity[1] = p->vor.head_velocity[1];
    signal->head_velocity[2] = p->vor.head_velocity[2];

    /* Copy linear acceleration from otoliths */
    signal->linear_accel[0] = p->nuclei[VESTIBULAR_NUCLEUS_LATERAL].activity[0];
    signal->linear_accel[1] = p->nuclei[VESTIBULAR_NUCLEUS_LATERAL].activity[1];
    signal->linear_accel[2] = p->nuclei[VESTIBULAR_NUCLEUS_LATERAL].activity[2];

    /* Copy efference copy of eye velocity */
    signal->eye_velocity[0] = p->vor.eye_velocity[0];
    signal->eye_velocity[1] = p->vor.eye_velocity[1];
    signal->eye_velocity[2] = p->vor.eye_velocity[2];

    /* Copy retinal slip (error signal) */
    signal->retinal_slip = p->vor.retinal_slip;

    signal->source = VESTIBULAR_NUCLEUS_MEDIAL;  /* Primary source */
    signal->timestamp_us = p->current_time_us;

    return true;
}

bool vestibular_apply_cerebellar_modulation(vestibular_processor_t* p,
                                             vestibular_nucleus_type_t nucleus,
                                             float modulation) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: p is NULL");
        return false;
    }

    if (nucleus >= VESTIBULAR_NUM_NUCLEI) {
        set_error(p, VESTIBULAR_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: capacity exceeded");
        return false;
    }

    if (!p->config.enable_cerebellar_input) {
        return true;  /* Ignored but not an error */
    }

    /* Apply modulation weight */
    modulation = 1.0f + (modulation - 1.0f) * p->config.cerebellar_weight;

    /* Clamp modulation to reasonable range */
    modulation = nimcp_clampf(modulation, 0.0f, 2.0f);

    p->nuclei[nucleus].cerebellar_modulation = modulation;

    LOG_DEBUG("[%s] Cerebellar modulation on nucleus %d: %.2f",
              VESTIBULAR_LOG_MODULE, nucleus, modulation);

    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

vestibular_status_t vestibular_get_status(const vestibular_processor_t* p) {
    if (!p) return VESTIBULAR_STATUS_ERROR;
    return p->status;
}

vestibular_error_t vestibular_get_last_error(const vestibular_processor_t* p) {
    if (!p) return VESTIBULAR_ERROR_INTERNAL;
    return p->last_error;
}

bool vestibular_get_stats(const vestibular_processor_t* p,
                           vestibular_stats_t* stats) {
    if (!p || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_get_last_error: required parameter is NULL (p, stats)");
        return false;
    }
    *stats = p->stats;
    return true;
}

bool vestibular_get_nucleus_state(const vestibular_processor_t* p,
                                   vestibular_nucleus_type_t nucleus,
                                   vestibular_nucleus_state_t* state) {
    if (!p || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vestibular_get_last_error: required parameter is NULL (p, state)");
        return false;
    }

    if (nucleus >= VESTIBULAR_NUM_NUCLEI) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vestibular_get_last_error: capacity exceeded");
        return false;
    }

    *state = p->nuclei[nucleus];
    return true;
}

const char* vestibular_error_string(vestibular_error_t error) {
    switch (error) {
        case VESTIBULAR_ERROR_NONE: return "No error";
        case VESTIBULAR_ERROR_INVALID_INPUT: return "Invalid input";
        case VESTIBULAR_ERROR_NOT_INITIALIZED: return "Not initialized";
        case VESTIBULAR_ERROR_CALIBRATION_FAIL: return "Calibration failed";
        case VESTIBULAR_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* vestibular_status_string(vestibular_status_t status) {
    switch (status) {
        case VESTIBULAR_STATUS_IDLE: return "Idle";
        case VESTIBULAR_STATUS_PROCESSING: return "Processing";
        case VESTIBULAR_STATUS_VOR_ACTIVE: return "VOR active";
        case VESTIBULAR_STATUS_VSR_ACTIVE: return "VSR active";
        case VESTIBULAR_STATUS_CALIBRATING: return "Calibrating";
        case VESTIBULAR_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}
