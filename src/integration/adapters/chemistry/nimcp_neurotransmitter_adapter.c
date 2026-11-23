/**
 * @file nimcp_neurotransmitter_adapter.c
 * @brief Neurotransmitter Dynamics Adapter Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements neurotransmitter release/uptake for Chemistry layer
 * WHY:  Chemical signaling is the primary mode of neural communication
 * HOW:  Models vesicle release, diffusion, binding, and reuptake
 *
 * @author NIMCP Development Team
 */

#include "integration/adapters/chemistry/nimcp_neurotransmitter_adapter.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neurotransmitter_adapter)

//=============================================================================
// Constants
//=============================================================================

#define NT_RRP_MAX              10.0f   /**< Readily releasable vesicles per site */
#define NT_REFILL_PER_MS        0.1f    /**< Pool refill (vesicles/ms): ~10 ms per vesicle */
#define NT_SPILLOVER_THRESH_MM  0.1f    /**< Spillover only above this cleft [NT] */
#define NT_EXTRA_CLEAR_PER_MS   0.1f    /**< Extrasynaptic clearance rate (1/ms) */
#define NT_MAX_SUBSTEP_MS       1.0f    /**< Internal sub-step bound ... */
#define NT_SUBSTEP_PER_CLEAR    1.0f    /**< ... and at most this x the fastest clearance tau */
#define NT_MIN_SUBSTEP_MS       0.05f   /**< floor on the sub-step (cost bound) */
#define NT_FLUSH_CONC_MM        1e-6f   /**< Cleft [NT] below this -> 0 (1 nM) */
#define NT_FLUSH_OCC            1e-6f   /**< Occupancy below this -> 0 */
#define NT_FLUSH_EXTRA_MM       1e-9f   /**< Extrasynaptic [NT] below this -> 0 */

/**
 * Default receptor + clearance kinetics per transmitter (indexed by
 * neurotransmitter_type_t). Literature-plausible values:
 *  - glutamate/AMPA: EC50 ~0.5 mM, Hill ~1.5, deactivation ~1 ms; cleft
 *    clearance ~1.2 ms (Clements et al. 1992; Jonas & Sakmann 1992)
 *  - GABA/GABA-A: EC50 ~0.1-0.2 mM for brief pulses, Hill ~1.5-2, slow
 *    deactivation (IPSC decay ~5-20 ms); uptake by GAT over a few ms
 *  - ACh/muscle nicotinic: EC50 ~20-100 uM, Hill ~2, EPC decay ~1 ms;
 *    hydrolysed by AChE within ~0.1-0.2 ms
 *  - glycine/GlyR a1b: EC50 ~50-100 uM, Hill ~2-3, IPSC decay ~5-10 ms;
 *    GlyT1/2 uptake over a few ms
 */
static const nimcp_nt_kinetics_t NT_DEFAULT_KINETICS[NT_COUNT] = {
    [NT_GLUTAMATE]     = {.kd_mm = 0.50f, .hill_n = 1.5f, .tau_on_ms = 1.0f, .tau_off_ms = 1.0f,  .clearance_tau_ms = 1.2f},
    [NT_GABA]          = {.kd_mm = 0.15f, .hill_n = 1.7f, .tau_on_ms = 0.5f, .tau_off_ms = 10.0f, .clearance_tau_ms = 2.0f},
    [NT_ACETYLCHOLINE] = {.kd_mm = 0.05f, .hill_n = 1.9f, .tau_on_ms = 0.3f, .tau_off_ms = 1.0f,  .clearance_tau_ms = 0.2f},
    [NT_GLYCINE]       = {.kd_mm = 0.08f, .hill_n = 2.5f, .tau_on_ms = 0.5f, .tau_off_ms = 6.0f,  .clearance_tau_ms = 2.0f},
};

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_nt_adapter_struct {
    nimcp_nt_adapter_config_t config;
    nimcp_module_interface_t interface;

    /* Synaptic state */
    float* cleft_conc;                  /**< Cleft [NT] per synapse (mM) */
    float* receptor_occupancy;          /**< Receptor binding per synapse */
    float* vesicle_pool;                /**< Available vesicles per synapse */
    uint8_t* nt_type;                   /**< Transmitter per synapse */
    uint32_t types_present;             /**< Bit t set if some synapse may carry type t */
    float extrasynaptic;                /**< Extrasynaptic concentration */

    /* Kinetics used by this adapter (defaults, overridable) */
    nimcp_nt_kinetics_t kin[NT_COUNT];

    /* State tracking */
    nimcp_nt_adapter_state_t state;
    nimcp_nt_adapter_stats_t stats;
    bool is_initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Binding kinetics (Hill equation)
 */
static inline float binding_kinetics(float conc, float kd, float n) {
    if (conc <= 0.0f) return 0.0f;
    float r = powf(conc / kd, n);
    return r / (1.0f + r);
}

static bool kinetics_valid(const nimcp_nt_kinetics_t* k) {
    return k && k->kd_mm > 0.0f && k->hill_n > 0.0f && k->tau_on_ms > 0.0f &&
           k->tau_off_ms > 0.0f && k->clearance_tau_ms > 0.0f &&
           isfinite(k->kd_mm) && isfinite(k->hill_n) && isfinite(k->tau_on_ms) &&
           isfinite(k->tau_off_ms) && isfinite(k->clearance_tau_ms);
}

static void reset_synapses(nimcp_nt_adapter_t adapter) {
    uint32_t n = adapter->config.num_synapses;
    for (uint32_t i = 0; i < n; i++) {
        adapter->cleft_conc[i] = 0.0f;
        adapter->receptor_occupancy[i] = 0.0f;
        adapter->vesicle_pool[i] = NT_RRP_MAX;  /* ~10 readily releasable vesicles */
        adapter->nt_type[i] = (uint8_t)adapter->config.default_nt;
    }
    adapter->types_present = 1u << (unsigned)adapter->config.default_nt;
    adapter->extrasynaptic = 0.0f;
    memset(&adapter->state, 0, sizeof(adapter->state));
}

/** Per-transmitter factors for one sub-step of h ms */
typedef struct {
    float e_clear;      /**< exp(-k h), k = clearance (+ spillover if active) */
    float avg_clear;    /**< mean of exp(-k t) over [0, h] */
    float spill_frac;   /**< spillover share of the cleared amount */
    float e_on, e_off;  /**< receptor relaxation factors */
} nt_step_coef_t;

static void make_coef(nt_step_coef_t* c, float k_clear, float k_spill,
                      const nimcp_nt_kinetics_t* kin, float h) {
    float k = k_clear + k_spill;
    float kh = k * h;
    c->e_clear = expf(-kh);
    c->avg_clear = (kh > 1e-6f) ? (1.0f - c->e_clear) / kh : 1.0f;
    c->spill_frac = (k > 0.0f) ? k_spill / k : 0.0f;
    c->e_on = expf(-h / kin->tau_on_ms);
    c->e_off = expf(-h / kin->tau_off_ms);
}

/**
 * @brief Advance all synapses by dt_ms (exact exponentials, sub-stepped)
 *
 * Within a sub-step the cleft concentration decays exactly,
 * c(t) = c0 exp(-k t); the receptor occupancy relaxes exactly towards the
 * Hill occupancy of the sub-step's MEAN concentration, with tau_on when
 * rising and tau_off when falling. Both updates are convex combinations,
 * so occupancy stays in [0, 1] and concentration >= 0 for every dt.
 */
static void nt_advance(nimcp_nt_adapter_t adapter, float dt_ms) {
    const uint32_t n = adapter->config.num_synapses;

    /* clearance rate per transmitter (1/ms) */
    float k_clear[NT_COUNT];
    float h_max = NT_MAX_SUBSTEP_MS;
    for (int t = 0; t < NT_COUNT; t++) {
        k_clear[t] = (adapter->config.reuptake_rate > 0.0f)
                   ? adapter->config.reuptake_rate
                   : 1.0f / adapter->kin[t].clearance_tau_ms;
        /* the Hill target uses the sub-step's mean cleft [NT]; keep the
           sub-step no longer than the fastest clearance present (ACh/AChE
           0.2 ms) so that mean stays representative */
        if (adapter->types_present & (1u << t)) {
            float hc = NT_SUBSTEP_PER_CLEAR / k_clear[t];
            if (hc < h_max) h_max = hc;
        }
    }
    if (h_max < NT_MIN_SUBSTEP_MS) h_max = NT_MIN_SUBSTEP_MS;
    int nsub = (int)ceilf(dt_ms / h_max - 1e-4f);
    if (nsub < 1) nsub = 1;
    const float h = dt_ms / (float)nsub;
    const float k_spill = (adapter->config.enable_spillover && adapter->config.diffusion_rate > 0.0f)
                        ? adapter->config.diffusion_rate : 0.0f;

    nt_step_coef_t co[NT_COUNT];     /* without spillover */
    nt_step_coef_t cs[NT_COUNT];     /* with spillover (cleft above threshold) */
    for (int t = 0; t < NT_COUNT; t++) {
        make_coef(&co[t], k_clear[t], 0.0f, &adapter->kin[t], h);
        make_coef(&cs[t], k_clear[t], k_spill, &adapter->kin[t], h);
    }
    const float refill = NT_REFILL_PER_MS * h;
    const float e_extra = expf(-NT_EXTRA_CLEAR_PER_MS * h);

    float sum_conc = 0.0f;
    float sum_occupancy = 0.0f;

    for (int s = 0; s < nsub; s++) {
        const bool last = (s == nsub - 1);
        for (uint32_t i = 0; i < n; i++) {
            float conc = adapter->cleft_conc[i];
            float occ = adapter->receptor_occupancy[i];

            /* Replenish vesicle pool (slow process, ~10 ms per vesicle) */
            if (adapter->vesicle_pool[i] < NT_RRP_MAX) {
                float v = adapter->vesicle_pool[i] + refill;
                adapter->vesicle_pool[i] = (v > NT_RRP_MAX) ? NT_RRP_MAX : v;
            }

            /* Silent synapse: nothing to integrate (keeps it exactly zero) */
            if (conc == 0.0f && occ == 0.0f) continue;

            const uint32_t t = adapter->nt_type[i];
            const nimcp_nt_kinetics_t* kin = &adapter->kin[t];
            const nt_step_coef_t* c =
                (k_spill > 0.0f && conc > NT_SPILLOVER_THRESH_MM) ? &cs[t] : &co[t];

            /* Receptor binding towards the Hill occupancy of the mean cleft [NT] */
            float target = binding_kinetics(conc * c->avg_clear, kin->kd_mm, kin->hill_n);
            float e = (target > occ) ? c->e_on : c->e_off;
            occ = target + (occ - target) * e;
            if (occ < NT_FLUSH_OCC) occ = 0.0f;
            if (occ > 1.0f) occ = 1.0f;

            /* Reuptake/degradation (+ spillover to extrasynaptic space) */
            float cleared = conc * (1.0f - c->e_clear);
            conc -= cleared;
            if (cleared > 0.001f) adapter->stats.reuptake_events++;
            adapter->extrasynaptic += cleared * c->spill_frac;
            if (conc < NT_FLUSH_CONC_MM) conc = 0.0f;

            adapter->cleft_conc[i] = conc;
            adapter->receptor_occupancy[i] = occ;
        }

        /* Extrasynaptic clearance */
        adapter->extrasynaptic *= e_extra;
        if (adapter->extrasynaptic < NT_FLUSH_EXTRA_MM) adapter->extrasynaptic = 0.0f;

        if (last) {
            for (uint32_t i = 0; i < n; i++) {
                sum_conc += adapter->cleft_conc[i];
                sum_occupancy += adapter->receptor_occupancy[i];
            }
        }
    }

    /* Update state */
    adapter->state.mean_cleft_conc_mm = sum_conc / (float)n;
    adapter->state.mean_receptor_occupancy = sum_occupancy / (float)n;
    adapter->state.extrasynaptic_conc_mm = adapter->extrasynaptic;
}

static nimcp_layer_error_t validate_config(const nimcp_nt_adapter_config_t* c) {
    if (c->num_synapses == 0) return NIMCP_LAYER_ERR_CAPACITY;
    if ((unsigned)c->default_nt >= (unsigned)NT_COUNT) return NIMCP_LAYER_ERR_INVALID_MODULE;
    return NIMCP_LAYER_OK;
}

//=============================================================================
// Module Interface Callbacks
//=============================================================================

/**
 * Initialise (or re-initialise) the adapter. Idempotent: called by create(),
 * and callers may call it again through the interface; a repeated call
 * applies the (new) config, resizes the arrays if num_synapses changed, and
 * resets the synaptic state. On failure the previous state is kept.
 */
static nimcp_layer_error_t nt_init(void* module, void* config) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_nt_adapter_config_t cfg = config ? *(nimcp_nt_adapter_config_t*)config
                                           : adapter->config;
    nimcp_layer_error_t err = validate_config(&cfg);
    if (err != NIMCP_LAYER_OK) return err;

    const uint32_t n = cfg.num_synapses;
    const bool resize = !adapter->is_initialized || n != adapter->config.num_synapses;

    if (resize) {
        float* cleft = (float*)nimcp_calloc(n, sizeof(float));
        float* occ = (float*)nimcp_calloc(n, sizeof(float));
        float* pool = (float*)nimcp_calloc(n, sizeof(float));
        uint8_t* type = (uint8_t*)nimcp_calloc(n, sizeof(uint8_t));
        if (!cleft || !occ || !pool || !type) {
            nimcp_free(cleft);
            nimcp_free(occ);
            nimcp_free(pool);
            nimcp_free(type);
            return NIMCP_LAYER_ERR_NO_MEMORY;
        }
        nimcp_free(adapter->cleft_conc);
        nimcp_free(adapter->receptor_occupancy);
        nimcp_free(adapter->vesicle_pool);
        nimcp_free(adapter->nt_type);
        adapter->cleft_conc = cleft;
        adapter->receptor_occupancy = occ;
        adapter->vesicle_pool = pool;
        adapter->nt_type = type;
    }

    adapter->config = cfg;
    reset_synapses(adapter);
    adapter->is_initialized = true;
    adapter->state.is_active = true;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_shutdown(void* module) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;

    nimcp_free(adapter->cleft_conc);
    nimcp_free(adapter->receptor_occupancy);
    nimcp_free(adapter->vesicle_pool);
    nimcp_free(adapter->nt_type);

    adapter->cleft_conc = NULL;
    adapter->receptor_occupancy = NULL;
    adapter->vesicle_pool = NULL;
    adapter->nt_type = NULL;

    adapter->is_initialized = false;
    adapter->state.is_active = false;

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_step_ms(nimcp_nt_adapter_t adapter, float dt_ms) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (!isfinite(dt_ms)) return NIMCP_LAYER_ERR_INVALID_MSG;
    if (dt_ms <= 0.0f) return NIMCP_LAYER_OK;

    nt_advance(adapter, dt_ms);
    adapter->stats.updates_processed++;
    return NIMCP_LAYER_OK;
}

/** Layer-interface update: dt in SECONDS (see header). */
static nimcp_layer_error_t nt_update(void* module, float dt) {
    return nt_step_ms((nimcp_nt_adapter_t)module, dt * 1000.0f);
}

static nimcp_layer_error_t nt_handle_message(void* module, const nimcp_layer_msg_t* msg) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter || !msg) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    adapter->stats.messages_handled++;

    switch (msg->header.msg_type) {
        case NIMCP_LAYER_MSG_DATA_PUSH:
            /* Incoming spike data triggers release */
            if (msg->payload && msg->header.payload_size >= sizeof(float)) {
                float calcium = *(float*)msg->payload;
                /* Release probability depends on calcium */
                float release_prob = adapter->config.release_probability *
                                     (1.0f + calcium * 0.01f);
                if (release_prob > 1.0f) release_prob = 1.0f;

                /* Probabilistic release at each synapse */
                for (uint32_t i = 0; i < adapter->config.num_synapses; i++) {
                    if (adapter->vesicle_pool[i] > 0.0f) {
                        /* Simple threshold model */
                        if (calcium > 100.0f) {  /* Ca threshold ~100 nM */
                            float release = adapter->config.vesicle_content_mm;
                            adapter->cleft_conc[i] += release;
                            adapter->vesicle_pool[i] -= 1.0f;
                            adapter->stats.vesicles_released++;
                            adapter->state.release_events++;
                        }
                    }
                }
            }
            break;

        case NIMCP_LAYER_MSG_EXCITE:
            /* Direct excitation = guaranteed release */
            for (uint32_t i = 0; i < adapter->config.num_synapses; i++) {
                if (adapter->vesicle_pool[i] > 0.0f) {
                    adapter->cleft_conc[i] += adapter->config.vesicle_content_mm;
                    adapter->vesicle_pool[i] -= 1.0f;
                    adapter->stats.vesicles_released++;
                    adapter->state.release_events++;
                }
            }
            break;

        default:
            break;
    }

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_get_state(void* module, void* state_out, size_t* size) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter || !state_out || !size) return NIMCP_LAYER_ERR_NULL_PTR;

    if (*size < sizeof(nimcp_nt_adapter_state_t)) {
        *size = sizeof(nimcp_nt_adapter_state_t);
        return NIMCP_LAYER_ERR_CAPACITY;
    }

    *(nimcp_nt_adapter_state_t*)state_out = adapter->state;
    *size = sizeof(nimcp_nt_adapter_state_t);

    return NIMCP_LAYER_OK;
}

static nimcp_layer_error_t nt_set_state(void* module, const void* state, size_t size) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)module;
    if (!adapter || !state) return NIMCP_LAYER_ERR_NULL_PTR;

    if (size < sizeof(nimcp_nt_adapter_state_t)) {
        return NIMCP_LAYER_ERR_INVALID_MSG;
    }

    adapter->state = *(const nimcp_nt_adapter_state_t*)state;

    return NIMCP_LAYER_OK;
}

static const char* nt_get_name(void* module) {
    (void)module;
    return "Neurotransmitter_Adapter";
}

//=============================================================================
// Public API Implementation
//=============================================================================

nimcp_nt_adapter_config_t nimcp_nt_adapter_default_config(void) {
    nimcp_nt_adapter_config_t config = {
        .num_synapses = 100,
        .release_probability = 0.3f,    /* ~30% release probability */
        .cleft_volume_fl = 0.2f,        /* ~0.2 femtoliters */
        .reuptake_rate = 0.0f,          /* 0 = per-transmitter clearance_tau_ms */
        .diffusion_rate = 0.1f,         /* spillover 1/ms while cleft > 0.1 mM */
        .vesicle_content_mm = 1.0f,     /* ~1 mM equivalent per vesicle */
        .default_nt = NT_GLUTAMATE,
        .enable_spillover = true,
        .enable_logging = false,
        .graded_max_rate = 0.1f         /* vesicles/ms/site at full drive (= refill) */
    };
    return config;
}

nimcp_nt_adapter_t nimcp_nt_adapter_create(
    const nimcp_nt_adapter_config_t* config
) {
    nimcp_nt_adapter_t adapter = (nimcp_nt_adapter_t)nimcp_calloc(
        1, sizeof(struct nimcp_nt_adapter_struct));
    NIMCP_API_CHECK_ALLOC(adapter, "Failed to allocate neurotransmitter adapter");

    adapter->config = config ? *config : nimcp_nt_adapter_default_config();
    memcpy(adapter->kin, NT_DEFAULT_KINETICS, sizeof(adapter->kin));

    /* Set up module interface */
    adapter->interface.init = nt_init;
    adapter->interface.shutdown = nt_shutdown;
    adapter->interface.update = nt_update;
    adapter->interface.handle_message = nt_handle_message;
    adapter->interface.get_state = nt_get_state;
    adapter->interface.set_state = nt_set_state;
    adapter->interface.get_name = nt_get_name;

    /* Return a ready-to-use adapter (interface->init may still be called) */
    if (nt_init(adapter, NULL) != NIMCP_LAYER_OK) {
        LOG_ERROR("nimcp_nt_adapter_create: init failed (num_synapses=%u, default_nt=%d)",
                  adapter->config.num_synapses, (int)adapter->config.default_nt);
        nimcp_free(adapter);
        return NULL;
    }

    return adapter;
}

void nimcp_nt_adapter_destroy(nimcp_nt_adapter_t adapter) {
    if (!adapter) return;

    if (adapter->is_initialized) {
        nt_shutdown(adapter);
    }

    nimcp_free(adapter);
}

nimcp_module_interface_t* nimcp_nt_adapter_get_interface(
    nimcp_nt_adapter_t adapter
) {
    return adapter ? &adapter->interface : NULL;
}

nimcp_layer_error_t nimcp_nt_adapter_release(
    nimcp_nt_adapter_t adapter,
    int synapse_idx,
    uint32_t num_vesicles
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    if (synapse_idx < 0) {
        /* Release at all synapses */
        for (uint32_t i = 0; i < adapter->config.num_synapses; i++) {
            uint32_t release = num_vesicles;
            if (release > (uint32_t)adapter->vesicle_pool[i]) {
                release = (uint32_t)adapter->vesicle_pool[i];
            }
            adapter->cleft_conc[i] += release * adapter->config.vesicle_content_mm;
            adapter->vesicle_pool[i] -= release;
            adapter->stats.vesicles_released += release;
            adapter->state.release_events += release;
        }
    } else if ((uint32_t)synapse_idx < adapter->config.num_synapses) {
        uint32_t release = num_vesicles;
        if (release > (uint32_t)adapter->vesicle_pool[synapse_idx]) {
            release = (uint32_t)adapter->vesicle_pool[synapse_idx];
        }
        adapter->cleft_conc[synapse_idx] += release * adapter->config.vesicle_content_mm;
        adapter->vesicle_pool[synapse_idx] -= release;
        adapter->stats.vesicles_released += release;
        adapter->state.release_events += release;
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }

    return NIMCP_LAYER_OK;
}

static inline void graded_release_one(nimcp_nt_adapter_t adapter, uint32_t i, float drive, float dt_ms) {
    if (!(drive > 0.0f)) return;              /* also rejects NaN */
    if (drive > 1.0f) drive = 1.0f;
    float v = drive * adapter->config.graded_max_rate * dt_ms;
    if (v > adapter->vesicle_pool[i]) v = adapter->vesicle_pool[i];
    if (v <= 0.0f) return;
    adapter->vesicle_pool[i] -= v;
    adapter->cleft_conc[i] += v * adapter->config.vesicle_content_mm;
    adapter->stats.graded_vesicles_released += (double)v;
}

nimcp_layer_error_t nimcp_nt_adapter_release_graded(
    nimcp_nt_adapter_t adapter,
    int synapse_idx,
    float drive,
    float dt_ms
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (!(dt_ms >= 0.0f) || !isfinite(dt_ms)) return NIMCP_LAYER_ERR_INVALID_MSG;

    if (synapse_idx < 0) {
        for (uint32_t i = 0; i < adapter->config.num_synapses; i++) {
            graded_release_one(adapter, i, drive, dt_ms);
        }
    } else if ((uint32_t)synapse_idx < adapter->config.num_synapses) {
        graded_release_one(adapter, (uint32_t)synapse_idx, drive, dt_ms);
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_release_graded_array(
    nimcp_nt_adapter_t adapter,
    const float* drive,
    uint32_t count,
    float dt_ms
) {
    if (!adapter || !drive) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if (!(dt_ms >= 0.0f) || !isfinite(dt_ms)) return NIMCP_LAYER_ERR_INVALID_MSG;

    if (count > adapter->config.num_synapses) count = adapter->config.num_synapses;
    for (uint32_t i = 0; i < count; i++) {
        graded_release_one(adapter, i, drive[i], dt_ms);
    }
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_step(nimcp_nt_adapter_t adapter, float dt_ms) {
    return nt_step_ms(adapter, dt_ms);
}

nimcp_layer_error_t nimcp_nt_adapter_set_synapse_type(
    nimcp_nt_adapter_t adapter,
    int synapse_idx,
    neurotransmitter_type_t nt
) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    if ((unsigned)nt >= (unsigned)NT_COUNT) return NIMCP_LAYER_ERR_INVALID_MODULE;

    if (synapse_idx < 0) {
        memset(adapter->nt_type, (int)nt, adapter->config.num_synapses);
        adapter->types_present = 1u << (unsigned)nt;
    } else if ((uint32_t)synapse_idx < adapter->config.num_synapses) {
        adapter->nt_type[synapse_idx] = (uint8_t)nt;
        adapter->types_present |= 1u << (unsigned)nt;
    } else {
        return NIMCP_LAYER_ERR_INVALID_MODULE;
    }
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_set_kinetics(
    nimcp_nt_adapter_t adapter,
    neurotransmitter_type_t nt,
    const nimcp_nt_kinetics_t* kinetics
) {
    if (!adapter || !kinetics) return NIMCP_LAYER_ERR_NULL_PTR;
    if ((unsigned)nt >= (unsigned)NT_COUNT) return NIMCP_LAYER_ERR_INVALID_MODULE;
    if (!kinetics_valid(kinetics)) return NIMCP_LAYER_ERR_INVALID_MSG;
    adapter->kin[nt] = *kinetics;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_get_default_kinetics(
    neurotransmitter_type_t nt,
    nimcp_nt_kinetics_t* out
) {
    if (!out) return NIMCP_LAYER_ERR_NULL_PTR;
    if ((unsigned)nt >= (unsigned)NT_COUNT) return NIMCP_LAYER_ERR_INVALID_MODULE;
    *out = NT_DEFAULT_KINETICS[nt];
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_concentrations(
    nimcp_nt_adapter_t adapter,
    float* conc_out,
    uint32_t max_count,
    uint32_t* count_out
) {
    if (!adapter || !conc_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_synapses;
    if (count > max_count) count = max_count;

    memcpy(conc_out, adapter->cleft_conc, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_occupancy(
    nimcp_nt_adapter_t adapter,
    float* occupancy_out,
    uint32_t max_count,
    uint32_t* count_out
) {
    if (!adapter || !occupancy_out || !count_out) return NIMCP_LAYER_ERR_NULL_PTR;
    if (!adapter->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    uint32_t count = adapter->config.num_synapses;
    if (count > max_count) count = max_count;

    memcpy(occupancy_out, adapter->receptor_occupancy, count * sizeof(float));
    *count_out = count;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_state(
    nimcp_nt_adapter_t adapter,
    nimcp_nt_adapter_state_t* state_out
) {
    if (!adapter || !state_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *state_out = adapter->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_get_stats(
    nimcp_nt_adapter_t adapter,
    nimcp_nt_adapter_stats_t* stats_out
) {
    if (!adapter || !stats_out) return NIMCP_LAYER_ERR_NULL_PTR;
    *stats_out = adapter->stats;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_nt_adapter_reset_stats(nimcp_nt_adapter_t adapter) {
    if (!adapter) return NIMCP_LAYER_ERR_NULL_PTR;
    memset(&adapter->stats, 0, sizeof(adapter->stats));
    return NIMCP_LAYER_OK;
}
