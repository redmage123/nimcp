//=============================================================================
// nimcp_spinal_cord.c - Spinal Cord / Motor Output Core Implementation
//=============================================================================
/**
 * @file nimcp_spinal_cord.c
 * @brief Core implementation of spinal cord motor output system
 *
 * WHAT: Motor pools, CPGs, reflex arcs, descending tract integration
 * WHY:  Final common pathway for motor output with spinal-level processing
 * HOW:  Half-center CPG oscillators, reflex arc processing, motor pool
 *       recruitment following Henneman's size principle
 *
 * BIOLOGICAL BASIS:
 * - CPGs: Half-center model (Brown 1911, Grillner 2006)
 * - Reflexes: Sherrington's integrative action of the nervous system
 * - Motor pools: Size principle recruitment (Henneman 1957)
 * - Gate control: Melzack & Wall (1965)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/spinal/nimcp_spinal_cord.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "SPINAL_CORD"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(spinal_cord, MESH_ADAPTER_CATEGORY_MOTOR)

#include "utils/thread/nimcp_thread.h"

#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
// Internal Helpers
//=============================================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Get current time in microseconds (monotonic clock)
 */
static uint64_t spinal_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Clamp float to [lo, hi]
 */
static float spinal_clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

//=============================================================================
// Default Configuration
//=============================================================================

spinal_config_t spinal_default_config(void) {
    spinal_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_motor_pools      = 8;
    config.neurons_per_pool     = 32;
    config.num_cpgs             = 4;
    config.num_reflexes         = 3;
    config.default_cpg_frequency = 1.0f;   /* 1 Hz - walking cadence */
    config.default_reflex_gain  = 0.5f;

    return config;
}

//=============================================================================
// Create / Destroy
//=============================================================================

spinal_cord_t* spinal_create(const spinal_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_create: config is NULL");
        return NULL;
    }

    /* Validate bounds */
    if (config->num_motor_pools == 0 || config->num_motor_pools > SPINAL_MAX_MOTOR_POOLS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_create: num_motor_pools out of range");
        return NULL;
    }
    if (config->num_cpgs > SPINAL_MAX_CPGS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_create: num_cpgs exceeds maximum");
        return NULL;
    }
    if (config->num_reflexes > SPINAL_MAX_REFLEXES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_create: num_reflexes exceeds maximum");
        return NULL;
    }
    if (config->neurons_per_pool == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_create: neurons_per_pool is zero");
        return NULL;
    }

    /* Allocate main structure */
    spinal_cord_t* sc = (spinal_cord_t*)nimcp_calloc(1, sizeof(spinal_cord_t));
    if (!sc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_create: failed to allocate spinal_cord_t");
        return NULL;
    }

    sc->magic = SPINAL_CORD_MAGIC;
    sc->config = *config;
    sc->num_motor_pools = config->num_motor_pools;
    sc->num_cpgs = config->num_cpgs;
    sc->num_reflexes = config->num_reflexes;
    sc->gate_control_level = 1.0f;  /* Fully open gate by default */
    sc->last_update_us = spinal_get_time_us();

    /* Create mutex */
    sc->lock = nimcp_mutex_create(NULL);
    if (!sc->lock) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_create: failed to create mutex");
        nimcp_free(sc);
        return NULL;
    }

    /* Allocate motor pools */
    sc->motor_pools = (motor_pool_t*)nimcp_calloc(sc->num_motor_pools, sizeof(motor_pool_t));
    if (!sc->motor_pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_create: failed to allocate motor pools");
        goto fail;
    }

    for (uint32_t i = 0; i < sc->num_motor_pools; i++) {
        motor_pool_t* pool = &sc->motor_pools[i];
        pool->num_neurons = config->neurons_per_pool;

        pool->activations = (float*)nimcp_calloc(pool->num_neurons, sizeof(float));
        if (!pool->activations) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "spinal_create: failed to allocate motor pool activations");
            goto fail;
        }

        pool->target_forces = (float*)nimcp_calloc(pool->num_neurons, sizeof(float));
        if (!pool->target_forces) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "spinal_create: failed to allocate motor pool target_forces");
            goto fail;
        }
    }

    /* Allocate CPGs */
    if (sc->num_cpgs > 0) {
        sc->cpgs = (cpg_t*)nimcp_calloc(sc->num_cpgs, sizeof(cpg_t));
        if (!sc->cpgs) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "spinal_create: failed to allocate CPGs");
            goto fail;
        }

        float freq = config->default_cpg_frequency;
        if (!isfinite(freq) || freq <= 0.0f) {
            freq = 1.0f;
        }

        for (uint32_t i = 0; i < sc->num_cpgs; i++) {
            sc->cpgs[i].frequency = freq;
            sc->cpgs[i].phase = 0.0f;
            sc->cpgs[i].amplitude = 0.5f;
            sc->cpgs[i].flexor_output = 0.0f;
            sc->cpgs[i].extensor_output = 0.0f;
            sc->cpgs[i].active = false;
        }
    }

    /* Allocate reflex arcs */
    if (sc->num_reflexes > 0) {
        sc->reflexes = (reflex_arc_t*)nimcp_calloc(sc->num_reflexes, sizeof(reflex_arc_t));
        if (!sc->reflexes) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "spinal_create: failed to allocate reflex arcs");
            goto fail;
        }

        float gain = config->default_reflex_gain;
        if (!isfinite(gain)) {
            gain = 0.5f;
        }

        /* Initialize default reflex arcs */
        for (uint32_t i = 0; i < sc->num_reflexes; i++) {
            sc->reflexes[i].type = (reflex_type_t)(i % REFLEX_TYPE_COUNT);
            sc->reflexes[i].gain = gain;
            sc->reflexes[i].threshold = 0.3f;
            sc->reflexes[i].latency_ms = 5.0f + (float)i * 2.0f;  /* Increasing latency */
            sc->reflexes[i].input_pool = i % sc->num_motor_pools;
            sc->reflexes[i].output_pool = (i + 1) % sc->num_motor_pools;
        }
    }

    /* Allocate sensory afferent buffers */
    sc->muscle_spindle_ia = (float*)nimcp_calloc(sc->num_motor_pools, sizeof(float));
    sc->muscle_spindle_ii = (float*)nimcp_calloc(sc->num_motor_pools, sizeof(float));
    sc->golgi_tendon_ib   = (float*)nimcp_calloc(sc->num_motor_pools, sizeof(float));

    if (!sc->muscle_spindle_ia || !sc->muscle_spindle_ii || !sc->golgi_tendon_ib) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_create: failed to allocate sensory afferent buffers");
        goto fail;
    }

    /* Allocate descending tract input buffers */
    sc->corticospinal_input  = (float*)nimcp_calloc(sc->num_motor_pools, sizeof(float));
    sc->rubrospinal_input    = (float*)nimcp_calloc(sc->num_motor_pools, sizeof(float));
    sc->vestibulospinal_input = (float*)nimcp_calloc(sc->num_motor_pools, sizeof(float));

    if (!sc->corticospinal_input || !sc->rubrospinal_input || !sc->vestibulospinal_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "spinal_create: failed to allocate descending tract buffers");
        goto fail;
    }

    NIMCP_LOGGING_INFO("Spinal cord created: %u motor pools (%u neurons each), "
                       "%u CPGs, %u reflexes",
                       sc->num_motor_pools, config->neurons_per_pool,
                       sc->num_cpgs, sc->num_reflexes);

    return sc;

fail:
    spinal_destroy(sc);
    return NULL;
}

void spinal_destroy(spinal_cord_t* system) {
    if (!system) {
        return;
    }

    /* Free motor pool contents */
    if (system->motor_pools) {
        for (uint32_t i = 0; i < system->num_motor_pools; i++) {
            nimcp_free(system->motor_pools[i].activations);
            nimcp_free(system->motor_pools[i].target_forces);
        }
        nimcp_free(system->motor_pools);
    }

    /* Free CPGs */
    nimcp_free(system->cpgs);

    /* Free reflex arcs */
    nimcp_free(system->reflexes);

    /* Free sensory afferent buffers */
    nimcp_free(system->muscle_spindle_ia);
    nimcp_free(system->muscle_spindle_ii);
    nimcp_free(system->golgi_tendon_ib);

    /* Free descending tract buffers */
    nimcp_free(system->corticospinal_input);
    nimcp_free(system->rubrospinal_input);
    nimcp_free(system->vestibulospinal_input);

    /* Destroy mutex */
    if (system->lock) {
        nimcp_mutex_destroy(system->lock);
    }

    system->magic = 0;
    nimcp_free(system);
}

//=============================================================================
// CPG Update (Internal)
//=============================================================================

/**
 * @brief Step all active CPG oscillators
 *
 * BIOLOGICAL: Half-center model. Flexor and extensor outputs are
 * anti-phase sinusoids. The phase advances at the configured frequency.
 *
 * @param system Spinal cord
 * @param dt_s   Time step in seconds
 */
static void spinal_update_cpgs(spinal_cord_t* system, float dt_s) {
    for (uint32_t i = 0; i < system->num_cpgs; i++) {
        cpg_t* cpg = &system->cpgs[i];
        if (!cpg->active) {
            continue;
        }

        /* Advance phase: phase += 2*pi*f*dt */
        float dphase = 2.0f * (float)M_PI * cpg->frequency * dt_s;
        if (!isfinite(dphase)) {
            continue;
        }

        cpg->phase += dphase;

        /* Wrap phase to [0, 2*pi] */
        while (cpg->phase >= 2.0f * (float)M_PI) {
            cpg->phase -= 2.0f * (float)M_PI;
        }

        /* Half-center outputs: flexor = sin, extensor = -sin (anti-phase) */
        float sin_val = sinf(cpg->phase);
        cpg->flexor_output  = spinal_clampf(cpg->amplitude * fmaxf(sin_val, 0.0f), 0.0f, 1.0f);
        cpg->extensor_output = spinal_clampf(cpg->amplitude * fmaxf(-sin_val, 0.0f), 0.0f, 1.0f);
    }
}

//=============================================================================
// Reflex Processing (Internal)
//=============================================================================

/**
 * @brief Process reflex arc activation
 *
 * BIOLOGICAL: Reflex arcs bypass cortical processing for fast response.
 * - Stretch reflex (Ia): monosynaptic, ~5ms latency
 * - Withdrawal reflex: polysynaptic, ~10-15ms latency
 * - Crossed extension: contralateral activation during withdrawal
 *
 * @param system  Spinal cord
 * @param reflex  Reflex arc to process
 * @param stimulus Stimulus intensity [0.0-1.0]
 */
static void spinal_process_reflex(spinal_cord_t* system, const reflex_arc_t* reflex,
                                  float stimulus) {
    if (!isfinite(stimulus) || stimulus < reflex->threshold) {
        return;  /* Below threshold - no reflex response */
    }

    /* Compute reflex output = gain * (stimulus - threshold) * gate_control */
    float response = reflex->gain * (stimulus - reflex->threshold) * system->gate_control_level;
    response = spinal_clampf(response, 0.0f, 1.0f);

    if (reflex->output_pool >= system->num_motor_pools) {
        return;  /* Invalid pool index */
    }

    motor_pool_t* out_pool = &system->motor_pools[reflex->output_pool];

    /* Apply reflex response to motor pool (additive) */
    for (uint32_t n = 0; n < out_pool->num_neurons; n++) {
        out_pool->activations[n] = spinal_clampf(
            out_pool->activations[n] + response, 0.0f, 1.0f);
    }

    /* Crossed extension: activate contralateral extensors */
    if (reflex->type == REFLEX_CROSSED_EXTENSION) {
        uint32_t contra_pool = (reflex->output_pool + 1) % system->num_motor_pools;
        motor_pool_t* contra = &system->motor_pools[contra_pool];
        float contra_response = response * 0.7f;  /* Weaker contralateral response */
        for (uint32_t n = 0; n < contra->num_neurons; n++) {
            contra->activations[n] = spinal_clampf(
                contra->activations[n] + contra_response, 0.0f, 1.0f);
        }
    }
}

//=============================================================================
// Motor Pool Integration (Internal)
//=============================================================================

/**
 * @brief Integrate descending commands into motor pool activations
 *
 * BIOLOGICAL: Motor pool receives convergent input from:
 * 1. Corticospinal tract (voluntary fine motor)
 * 2. Rubrospinal tract (flexor facilitation)
 * 3. Vestibulospinal tract (postural balance)
 * 4. CPG output (rhythmic patterns)
 * 5. Reflex contributions (already added in process_reflex)
 *
 * Golgi tendon organ Ib afferents provide inhibitory feedback
 * (autogenic inhibition) to prevent muscle damage.
 *
 * @param system Spinal cord
 */
static void spinal_integrate_motor_pools(spinal_cord_t* system) {
    for (uint32_t p = 0; p < system->num_motor_pools; p++) {
        motor_pool_t* pool = &system->motor_pools[p];

        /* Sum descending tract inputs */
        float descending = 0.0f;
        if (isfinite(system->corticospinal_input[p])) {
            descending += system->corticospinal_input[p] * 0.6f;  /* Primary drive */
        }
        if (isfinite(system->rubrospinal_input[p])) {
            descending += system->rubrospinal_input[p] * 0.2f;    /* Flexor bias */
        }
        if (isfinite(system->vestibulospinal_input[p])) {
            descending += system->vestibulospinal_input[p] * 0.2f; /* Postural */
        }

        /* Golgi tendon Ib inhibition (autogenic inhibition) */
        float ib_inhibition = 0.0f;
        if (isfinite(system->golgi_tendon_ib[p])) {
            ib_inhibition = system->golgi_tendon_ib[p] * 0.3f;
        }

        /* CPG contribution: distribute first CPG to first pools, etc. */
        float cpg_drive = 0.0f;
        if (system->num_cpgs > 0) {
            uint32_t cpg_idx = p % system->num_cpgs;
            if (system->cpgs[cpg_idx].active) {
                /* Alternate flexor/extensor for even/odd pools */
                cpg_drive = (p % 2 == 0)
                    ? system->cpgs[cpg_idx].flexor_output
                    : system->cpgs[cpg_idx].extensor_output;
            }
        }

        /* Integrate: target_force = (descending + cpg + activation - ib_inhibition) * gate */
        for (uint32_t n = 0; n < pool->num_neurons; n++) {
            float total = pool->activations[n] + descending + cpg_drive - ib_inhibition;
            total *= system->gate_control_level;
            pool->target_forces[n] = spinal_clampf(total, 0.0f, 1.0f);
        }

        /* Reset per-cycle activation (reflex contributions are single-shot) */
        memset(pool->activations, 0, pool->num_neurons * sizeof(float));
    }
}

//=============================================================================
// Public API Implementation
//=============================================================================

int spinal_update(spinal_cord_t* system, float dt_s) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_update: system is NULL");
        return -1;
    }
    if (system->magic != SPINAL_CORD_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_update: invalid magic number");
        return -1;
    }
    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_update: invalid time step");
        return -1;
    }

    nimcp_mutex_lock(system->lock);

    /* 1. Step CPG oscillators */
    spinal_update_cpgs(system, dt_s);

    /* 2. Process stretch reflexes from Ia afferents */
    for (uint32_t r = 0; r < system->num_reflexes; r++) {
        reflex_arc_t* ref = &system->reflexes[r];
        if (ref->type == REFLEX_STRETCH && ref->input_pool < system->num_motor_pools) {
            float ia_signal = system->muscle_spindle_ia[ref->input_pool];
            spinal_process_reflex(system, ref, ia_signal);
        }
    }

    /* 3. Integrate all inputs into motor pool output */
    spinal_integrate_motor_pools(system);

    /* 4. Update timestamp */
    system->last_update_us = spinal_get_time_us();

    nimcp_mutex_unlock(system->lock);

    return 0;
}

int spinal_activate_cpg(spinal_cord_t* system, uint32_t cpg_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_activate_cpg: system is NULL");
        return -1;
    }
    if (cpg_id >= system->num_cpgs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_activate_cpg: cpg_id out of range");
        return -1;
    }

    nimcp_mutex_lock(system->lock);
    system->cpgs[cpg_id].active = true;
    system->cpgs[cpg_id].phase = 0.0f;  /* Reset phase on activation */
    nimcp_mutex_unlock(system->lock);

    NIMCP_LOGGING_DEBUG("CPG %u activated (freq=%.2f Hz)", cpg_id,
                        system->cpgs[cpg_id].frequency);
    return 0;
}

int spinal_deactivate_cpg(spinal_cord_t* system, uint32_t cpg_id) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_deactivate_cpg: system is NULL");
        return -1;
    }
    if (cpg_id >= system->num_cpgs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_deactivate_cpg: cpg_id out of range");
        return -1;
    }

    nimcp_mutex_lock(system->lock);
    system->cpgs[cpg_id].active = false;
    system->cpgs[cpg_id].flexor_output = 0.0f;
    system->cpgs[cpg_id].extensor_output = 0.0f;
    nimcp_mutex_unlock(system->lock);

    NIMCP_LOGGING_DEBUG("CPG %u deactivated", cpg_id);
    return 0;
}

int spinal_set_corticospinal_input(spinal_cord_t* system, const float* input, uint32_t size) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_set_corticospinal_input: system is NULL");
        return -1;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_set_corticospinal_input: input is NULL");
        return -1;
    }
    if (size != system->num_motor_pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_set_corticospinal_input: size mismatch");
        return -1;
    }

    nimcp_mutex_lock(system->lock);
    memcpy(system->corticospinal_input, input, size * sizeof(float));
    nimcp_mutex_unlock(system->lock);

    return 0;
}

int spinal_get_motor_output(const spinal_cord_t* system, uint32_t pool_id,
                            float* output, uint32_t size) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_get_motor_output: system is NULL");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_get_motor_output: output is NULL");
        return -1;
    }
    if (pool_id >= system->num_motor_pools) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_get_motor_output: pool_id out of range");
        return -1;
    }

    const motor_pool_t* pool = &system->motor_pools[pool_id];
    uint32_t copy_count = (size < pool->num_neurons) ? size : pool->num_neurons;

    /* Note: const-cast for lock since this is a read-only logical operation */
    nimcp_mutex_lock(((spinal_cord_t*)system)->lock);
    memcpy(output, pool->target_forces, copy_count * sizeof(float));
    nimcp_mutex_unlock(((spinal_cord_t*)system)->lock);

    return 0;
}

int spinal_trigger_reflex(spinal_cord_t* system, uint32_t reflex_id, float stimulus) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "spinal_trigger_reflex: system is NULL");
        return -1;
    }
    if (reflex_id >= system->num_reflexes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_trigger_reflex: reflex_id out of range");
        return -1;
    }
    if (!isfinite(stimulus)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "spinal_trigger_reflex: stimulus is not finite");
        return -1;
    }

    stimulus = spinal_clampf(stimulus, 0.0f, 1.0f);

    nimcp_mutex_lock(system->lock);
    spinal_process_reflex(system, &system->reflexes[reflex_id], stimulus);
    nimcp_mutex_unlock(system->lock);

    NIMCP_LOGGING_DEBUG("Reflex %u triggered (type=%d, stimulus=%.3f)",
                        reflex_id, system->reflexes[reflex_id].type, stimulus);
    return 0;
}
