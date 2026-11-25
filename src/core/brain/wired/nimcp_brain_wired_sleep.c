/**
 * @file nimcp_brain_wired_sleep.c
 * @brief Wired-module group BRAIN_WIRED_SLEEP. See include/core/brain/nimcp_brain_wired.h.
 *
 * WHAT: Drives the sleep-state bridges that have a real target on this brain
 *       from the brain's sleep/wake system (brain->sleep_system).
 *
 * HOW THE SLEEP SYSTEM DELIVERS STATE:
 *   sleep_run_cycle() (daemon sleep scheduler thread, under the service lock)
 *   walks DROWSY -> LIGHT_NREM -> DEEP_NREM -> REM -> AWAKE inside ONE call,
 *   calling sleep_enter_state()/sleep_wake_up() which notify registered
 *   listeners synchronously. Episodic replay inside DEEP_NREM/REM calls
 *   brain_learn_vector, so the training path runs while asleep. Polling from
 *   tick() alone would only ever see AWAKE between cycles, so this group
 *   registers ONE listener (sleep_register_state_callback) and fans the
 *   transition out to every bridge. The bridges themselves do not
 *   self-register: sleep_wake.c only invokes the newest 32 listeners.
 *   tick() is a cheap fallback that re-polls the state every 64 ticks and
 *   applies any transition the listener missed.
 *
 * BRIDGES DRIVEN HERE:
 *   - visual_sleep_bridge   -> brain->visual_cortex: local tonic
 *       norepinephrine (visual_cortex_set_tonic_level, type 2) scaled by the
 *       bridge's attention gate (LC-NE: 1.0 awake, 0.6 drowsy, 0.2 light NREM,
 *       0.0 deep NREM, 0.1 REM). The awake baseline is snapshotted on the way
 *       into sleep and restored exactly on wake. Consumed by
 *       visual_cortex_compute_neuromod_effects (V1 layer 4 and 2/3 gains).
 *   - cortical_layers_sleep_bridge -> brain->laminar_system: feedforward /
 *       feedback inter-layer strengths (laminar_connect_feedforward/feedback via
 *       cortical_layers_sleep_apply_modulation); AWAKE restores the canonical
 *       Douglas & Martin strengths. NOTE: nothing in the live brain reads
 *       laminar_system yet, so this has no behavioural effect today.
 *   - cortical_column_sleep_bridge (one per brain->hypercolumns[i]) ->
 *       competition mode, softmax temperature and lateral inhibition via the
 *       hypercolumn's locked setters; read by hypercolumn_compute in
 *       brain_decide's cortical column path. Awake values saved in the bridge
 *       on sleep entry, restored on wake / destroy.
 *   - retina_sleep_bridge -> brain->vision retina illuminance
 *       (brain_vision_set_illuminance) = awake value x light-sensitivity factor
 *       (eyelids: 0.8 drowsy, 0.4 light NREM, 0.1 deep NREM, 0.2 REM); read by
 *       every retina_circuit_see (staged visual sensory input).
 *   - adaptive_sleep_bridge -> brain->network threshold adaptation
 *       (adaptive_network_set_sleep_modulation): sparsity target scaled by the
 *       bridge's state/awake ratio, adaptation paused in deep NREM; read by the
 *       non-readonly adaptive forward (process_network_outputs). Runtime-only
 *       override, never persisted; (0,false) on wake restores exactly.
 *
 * No learned state: save/load are no-ops (load returns 1 = fresh).
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"

#include "cognitive/nimcp_sleep_wake.h"
#include "perception/sleep/nimcp_visual_cortex_sleep_bridge.h"
#include "core/cortical_columns/sleep/nimcp_cortical_layers_sleep_bridge.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/sleep/nimcp_retina_sleep_bridge.h"
#include "core/cortical_columns/sleep/nimcp_cortical_column_sleep_bridge.h"
#include "plasticity/adaptive/nimcp_adaptive_sleep_bridge.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/brain/nimcp_brain_vision.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WIRED_SLEEP_POLL_EVERY   64u   /* fallback poll cadence (ticks) */
#define WIRED_SLEEP_VC_NE        2u    /* visual cortex neuromod index: 0=DA 1=ACh 2=NE */
#define WIRED_SLEEP_NUM_STATES   5
#define WIRED_SLEEP_MAX_COLUMNS  1024u /* cap on per-hypercolumn bridges */

typedef struct {
    struct brain_struct* brain;
    sleep_system_t sleep;
    bool listener_registered;
    nimcp_mutex_t* mu;                 /* serialises apply (listener thread vs tick fallback) */

    /* bridges (NULL when the target does not exist on this brain) */
    visual_sleep_bridge_t visual;
    cortical_layers_sleep_bridge_t layers;
    cortical_column_sleep_bridge_t* columns;   /* [num_columns], one per hypercolumn */
    uint32_t num_columns;
    retina_sleep_bridge_t retina;
    adaptive_sleep_bridge_t adaptive;

    /* transition bookkeeping (under mu) */
    sleep_state_t applied_state;
    uint64_t transitions;
    uint64_t transitions_by_state[WIRED_SLEEP_NUM_STATES];
    uint64_t fallback_applies;         /* transitions caught by tick polling */
    uint32_t tick_count;               /* training thread only */

    /* visual cortex NE baseline (awake value, restored on wake) */
    bool vc_ne_have_base;
    float vc_ne_base;
    float vc_ne_last;
    uint64_t vc_applies;

    /* laminar */
    uint64_t layers_applies;
    float layers_ff;
    float layers_fb;

    /* hypercolumns */
    uint64_t column_applies;
    uint64_t column_failures;

    /* retina illuminance (awake value, restored on wake) */
    bool ret_have_base;
    float ret_base;
    float ret_last;
    uint64_t ret_applies;

    /* adaptive network threshold adaptation */
    float adapt_override;
    bool adapt_frozen;
    uint64_t adapt_applies;
} wired_sleep_state_t;

static const char* wired_sleep_state_name(sleep_state_t s)
{
    switch (s) {
        case SLEEP_STATE_AWAKE:      return "awake";
        case SLEEP_STATE_DROWSY:     return "drowsy";
        case SLEEP_STATE_LIGHT_NREM: return "light_nrem";
        case SLEEP_STATE_DEEP_NREM:  return "deep_nrem";
        case SLEEP_STATE_REM:        return "rem";
        default:                     return "unknown";
    }
}

/* Visual cortex: tonic NE <- awake baseline x attention gate. Caller holds st->mu. */
static void wired_sleep_apply_visual(wired_sleep_state_t* st, sleep_state_t state)
{
    if (!st->visual || !st->brain->visual_cortex) {
        return;
    }
    visual_cortex_t* vc = st->brain->visual_cortex;

    if (visual_sleep_update(st->visual) != 0) {
        return;
    }
    visual_sleep_effects_t eff;
    if (visual_sleep_get_effects(st->visual, &eff) != 0) {
        return;
    }

    if (state == SLEEP_STATE_AWAKE) {
        if (st->vc_ne_have_base) {
            visual_cortex_set_tonic_level(vc, WIRED_SLEEP_VC_NE, st->vc_ne_base);
            st->vc_ne_last = st->vc_ne_base;
            st->vc_ne_have_base = false;
            st->vc_applies++;
        }
        return;
    }

    if (!st->vc_ne_have_base) {
        const phasic_tonic_state_t* ne = visual_cortex_get_neuromod_state(vc, WIRED_SLEEP_VC_NE);
        if (!ne) {
            return;
        }
        float base = phasic_tonic_get_tonic_level(ne);
        if (!isfinite(base)) {
            return;
        }
        st->vc_ne_base = fminf(fmaxf(base, 0.0F), 1.0F);
        st->vc_ne_have_base = true;
    }

    float gate = eff.attention_gate;
    if (!isfinite(gate)) {
        return;
    }
    gate = fminf(fmaxf(gate, 0.0F), 1.0F);
    float level = st->vc_ne_base * gate;
    if (!isfinite(level)) {
        return;
    }
    visual_cortex_set_tonic_level(vc, WIRED_SLEEP_VC_NE, level);
    st->vc_ne_last = level;
    st->vc_applies++;
}

/* Laminar structure: FF/FB strengths for the new state. Caller holds st->mu. */
static void wired_sleep_apply_layers(wired_sleep_state_t* st)
{
    if (!st->layers || !st->brain->laminar_system) {
        return;
    }
    if (cortical_layers_sleep_update(st->layers) != 0) {
        return;
    }
    cortical_layers_sleep_effects_t eff;
    if (cortical_layers_sleep_get_effects(st->layers, &eff) != 0) {
        return;
    }
    if (!isfinite(eff.feedforward_balance) || !isfinite(eff.feedback_balance)) {
        return;
    }
    if (cortical_layers_sleep_apply_modulation(st->layers) == 0) {
        st->layers_ff = eff.feedforward_balance;
        st->layers_fb = eff.feedback_balance;
        st->layers_applies++;
    }
}

/* Hypercolumns: competition + inhibition. Caller holds st->mu. */
static void wired_sleep_apply_columns(wired_sleep_state_t* st)
{
    for (uint32_t i = 0; i < st->num_columns; i++) {
        if (!st->columns[i]) {
            continue;
        }
        if (cortical_column_sleep_update(st->columns[i]) != 0 ||
            cortical_column_sleep_apply_modulation(st->columns[i]) != 0) {
            st->column_failures++;
        } else {
            st->column_applies++;
        }
    }
}

/* Retina: illuminance <- awake value x light sensitivity. Caller holds st->mu. */
static void wired_sleep_apply_retina(wired_sleep_state_t* st, sleep_state_t state)
{
    if (!st->retina || !st->brain->vision) {
        return;
    }
    if (state == SLEEP_STATE_AWAKE) {
        if (st->ret_have_base) {
            if (brain_vision_set_illuminance(st->brain, st->ret_base) == 0) {
                st->ret_last = st->ret_base;
                st->ret_applies++;
            }
            st->ret_have_base = false;
        }
        return;
    }
    if (retina_sleep_update(st->retina) != 0) {
        return;
    }
    retina_sleep_effects_t eff;
    if (retina_sleep_get_effects(st->retina, &eff) != 0) {
        return;
    }
    if (!st->ret_have_base) {
        float base = 0.0F;
        if (brain_vision_get_illuminance(st->brain, &base) != 0 || !isfinite(base) || base < 0.0F) {
            return;
        }
        st->ret_base = base;
        st->ret_have_base = true;
    }
    float f = eff.light_sensitivity_factor;
    if (!isfinite(f)) {
        return;
    }
    f = fminf(fmaxf(f, 0.0F), 1.0F);
    float lux = st->ret_base * f;
    if (isfinite(lux) && brain_vision_set_illuminance(st->brain, lux) == 0) {
        st->ret_last = lux;
        st->ret_applies++;
    }
}

/* Adaptive network: sparsity target + adaptation freeze. Caller holds st->mu. */
static void wired_sleep_apply_adaptive(wired_sleep_state_t* st, sleep_state_t state)
{
    if (!st->adaptive || !st->brain->network) {
        return;
    }
    adaptive_network_t net = st->brain->network;
    if (state == SLEEP_STATE_AWAKE) {
        if (adaptive_network_set_sleep_modulation(net, 0.0F, false) == 0) {
            st->adapt_override = 0.0F;
            st->adapt_frozen = false;
            st->adapt_applies++;
        }
        return;
    }
    if (adaptive_sleep_update(st->adaptive) != 0) {
        return;
    }
    adaptive_sleep_effects_t eff;
    if (adaptive_sleep_get_effects(st->adaptive, &eff) != 0) {
        return;
    }
    float base = adaptive_network_get_sparsity_target(net);
    if (!isfinite(base) || base <= 0.0F || !isfinite(eff.sparsity_target)) {
        return;
    }
    /* The bridge's targets are absolute for a 0.75-sparsity network; apply
     * them as a ratio to this network's own awake target. */
    float target = base * (eff.sparsity_target / ADAPTIVE_SLEEP_SPARSITY_AWAKE);
    if (!isfinite(target)) {
        return;
    }
    target = fminf(fmaxf(target, ADAPTIVE_SLEEP_SPARSITY_MIN), ADAPTIVE_SLEEP_SPARSITY_MAX);
    if (adaptive_network_set_sleep_modulation(net, target, eff.freeze_thresholds) == 0) {
        st->adapt_override = target;
        st->adapt_frozen = eff.freeze_thresholds;
        st->adapt_applies++;
    }
}

/* Apply one transition to every bridge. Caller holds st->mu. */
static void wired_sleep_apply_locked(wired_sleep_state_t* st, sleep_state_t state)
{
    if (state == st->applied_state) {
        return;
    }
    st->applied_state = state;
    st->transitions++;
    if ((int)state >= 0 && (int)state < WIRED_SLEEP_NUM_STATES) {
        st->transitions_by_state[state]++;
    }
    wired_sleep_apply_visual(st, state);
    wired_sleep_apply_layers(st);
    wired_sleep_apply_columns(st);
    wired_sleep_apply_retina(st, state);
    wired_sleep_apply_adaptive(st, state);
}

/* Sleep-system listener: invoked synchronously from sleep_enter_state /
 * sleep_wake_up, without the sleep lock held. */
static void wired_sleep_on_state_change(sleep_state_t new_state, void* user_data)
{
    wired_sleep_state_t* st = (wired_sleep_state_t*)user_data;
    if (!st || !st->mu) {
        return;
    }
    nimcp_mutex_lock(st->mu);
    wired_sleep_apply_locked(st, new_state);
    nimcp_mutex_unlock(st->mu);
}

int brain_wired_sleep_create(struct brain_struct* brain)
{
    if (!brain) {
        return -1;
    }
    if (!brain->sleep_system) {
        NIMCP_LOGGING_INFO("wired/sleep: brain has no sleep system; group not created");
        return -1;
    }

    wired_sleep_state_t* st = (wired_sleep_state_t*)nimcp_calloc(1, sizeof(*st));
    if (!st) {
        return -1;
    }
    st->brain = brain;
    st->sleep = brain->sleep_system;
    st->mu = nimcp_mutex_create(NULL);
    if (!st->mu) {
        nimcp_free(st);
        return -1;
    }
    st->applied_state = sleep_get_current_state(st->sleep);
    st->layers_ff = 1.0F;
    st->layers_fb = 1.0F;

    if (brain->visual_cortex) {
        st->visual = visual_sleep_bridge_create(NULL, st->sleep);
    }
    if (brain->laminar_system) {
        st->layers = cortical_layers_sleep_bridge_create(NULL, brain->laminar_system, st->sleep);
    }
    if (brain->hypercolumns && brain->num_hypercolumns > 0) {
        uint32_t n = brain->num_hypercolumns;
        if (n > WIRED_SLEEP_MAX_COLUMNS) {
            NIMCP_LOGGING_WARN("wired/sleep: %u hypercolumns, modulating the first %u",
                               n, WIRED_SLEEP_MAX_COLUMNS);
            n = WIRED_SLEEP_MAX_COLUMNS;
        }
        st->columns = (cortical_column_sleep_bridge_t*)nimcp_calloc(n, sizeof(*st->columns));
        if (st->columns) {
            st->num_columns = n;
            for (uint32_t i = 0; i < n; i++) {
                if (brain->hypercolumns[i]) {
                    st->columns[i] = cortical_column_sleep_bridge_create(
                        NULL, brain->hypercolumns[i], st->sleep);
                }
            }
        }
    }
    if (brain->vision) {
        st->retina = retina_sleep_bridge_create(NULL, st->sleep);
    }
    if (brain->network) {
        st->adaptive = adaptive_sleep_bridge_create(NULL, st->sleep);
    }

    if (!st->visual && !st->layers && !st->num_columns && !st->retina && !st->adaptive) {
        NIMCP_LOGGING_INFO("wired/sleep: no bridge target on this brain");
        nimcp_free(st->columns);
        nimcp_mutex_free(st->mu);
        nimcp_free(st);
        return -1;
    }

    st->listener_registered =
        sleep_register_state_callback(st->sleep, wired_sleep_on_state_change, st);
    if (!st->listener_registered) {
        NIMCP_LOGGING_WARN("wired/sleep: listener registration failed; tick polling only");
    }

    brain->wired[BRAIN_WIRED_SLEEP] = st;
    NIMCP_LOGGING_INFO("wired/sleep: created (visual=%s laminar=%s columns=%u retina=%s "
                       "adaptive=%s listener=%s)",
                       st->visual ? "yes" : "no", st->layers ? "yes" : "no", st->num_columns,
                       st->retina ? "yes" : "no", st->adaptive ? "yes" : "no",
                       st->listener_registered ? "yes" : "no");
    return 0;
}

void brain_wired_sleep_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms)
{
    (void)f; (void)n; (void)dt_ms;
    if (!brain) {
        return;
    }
    wired_sleep_state_t* st = (wired_sleep_state_t*)brain->wired[BRAIN_WIRED_SLEEP];
    if (!st) {
        return;
    }
    if (++st->tick_count % WIRED_SLEEP_POLL_EVERY != 0) {
        return;
    }
    /* Fallback: catch a transition the listener missed (e.g. registration
     * failed, or the listener was pushed past sleep_wake.c's 32-entry cap). */
    sleep_state_t now = sleep_get_current_state(st->sleep);
    nimcp_mutex_lock(st->mu);
    if (now != st->applied_state) {
        st->fallback_applies++;
        wired_sleep_apply_locked(st, now);
    }
    nimcp_mutex_unlock(st->mu);
}

int brain_wired_sleep_save(struct brain_struct* brain, const char* path)
{
    (void)brain; (void)path;
    return 0;   /* no learned state */
}

int brain_wired_sleep_load(struct brain_struct* brain, const char* path)
{
    (void)brain; (void)path;
    return 1;   /* nothing to load: fresh state */
}

void brain_wired_sleep_destroy(struct brain_struct* brain)
{
    if (!brain) {
        return;
    }
    wired_sleep_state_t* st = (wired_sleep_state_t*)brain->wired[BRAIN_WIRED_SLEEP];
    brain->wired[BRAIN_WIRED_SLEEP] = NULL;
    if (!st) {
        return;
    }
    if (st->listener_registered && st->sleep) {
        sleep_unregister_state_callback(st->sleep, wired_sleep_on_state_change, st);
        st->listener_registered = false;
    }

    nimcp_mutex_lock(st->mu);
    /* Leave targets at their awake values. */
    if (st->visual && st->vc_ne_have_base && brain->visual_cortex) {
        visual_cortex_set_tonic_level(brain->visual_cortex, WIRED_SLEEP_VC_NE, st->vc_ne_base);
        st->vc_ne_have_base = false;
    }
    if (st->visual) {
        visual_sleep_bridge_destroy(st->visual);
        st->visual = NULL;
    }
    if (st->layers) {
        cortical_layers_sleep_bridge_destroy(st->layers);
        st->layers = NULL;
    }
    for (uint32_t i = 0; i < st->num_columns; i++) {
        if (st->columns[i]) {
            cortical_column_sleep_restore_awake(st->columns[i]);
            cortical_column_sleep_bridge_destroy(st->columns[i]);
        }
    }
    nimcp_free(st->columns);
    st->columns = NULL;
    st->num_columns = 0;
    if (st->retina) {
        if (st->ret_have_base && brain->vision) {
            brain_vision_set_illuminance(brain, st->ret_base);
            st->ret_have_base = false;
        }
        retina_sleep_bridge_destroy(st->retina);
        st->retina = NULL;
    }
    if (st->adaptive) {
        if (brain->network) {
            adaptive_network_set_sleep_modulation(brain->network, 0.0F, false);
        }
        adaptive_sleep_bridge_destroy(st->adaptive);
        st->adaptive = NULL;
    }
    nimcp_mutex_unlock(st->mu);

    nimcp_mutex_free(st->mu);
    nimcp_free(st);
}

int brain_wired_sleep_status_json(struct brain_struct* brain, char* buf, uint32_t cap)
{
    if (!buf || cap == 0) {
        return 0;
    }
    wired_sleep_state_t* st = brain ? (wired_sleep_state_t*)brain->wired[BRAIN_WIRED_SLEEP] : NULL;
    if (!st) {
        int w = snprintf(buf, cap, "{\"created\":false}");
        return (w < 0) ? 0 : ((uint32_t)w >= cap ? (int)cap - 1 : w);
    }

    nimcp_mutex_lock(st->mu);
    int w = snprintf(buf, cap,
        "{\"created\":true,\"listener\":%s,\"state\":\"%s\",\"transitions\":%llu,"
        "\"by_state\":{\"awake\":%llu,\"drowsy\":%llu,\"light_nrem\":%llu,\"deep_nrem\":%llu,\"rem\":%llu},"
        "\"fallback_applies\":%llu,"
        "\"visual\":{\"bridge\":%s,\"applies\":%llu,\"ne_base\":%.4f,\"ne_base_held\":%s,\"ne_last\":%.4f},"
        "\"laminar\":{\"bridge\":%s,\"applies\":%llu,\"ff_balance\":%.3f,\"fb_balance\":%.3f},"
        "\"columns\":{\"bridges\":%u,\"applies\":%llu,\"failures\":%llu},"
        "\"retina\":{\"bridge\":%s,\"applies\":%llu,\"illum_base\":%.4f,\"illum_base_held\":%s,\"illum_last\":%.4f},"
        "\"adaptive\":{\"bridge\":%s,\"applies\":%llu,\"sparsity_override\":%.3f,\"frozen\":%s}}",
        st->listener_registered ? "true" : "false",
        wired_sleep_state_name(st->applied_state),
        (unsigned long long)st->transitions,
        (unsigned long long)st->transitions_by_state[SLEEP_STATE_AWAKE],
        (unsigned long long)st->transitions_by_state[SLEEP_STATE_DROWSY],
        (unsigned long long)st->transitions_by_state[SLEEP_STATE_LIGHT_NREM],
        (unsigned long long)st->transitions_by_state[SLEEP_STATE_DEEP_NREM],
        (unsigned long long)st->transitions_by_state[SLEEP_STATE_REM],
        (unsigned long long)st->fallback_applies,
        st->visual ? "true" : "false",
        (unsigned long long)st->vc_applies,
        (double)st->vc_ne_base,
        st->vc_ne_have_base ? "true" : "false",
        (double)st->vc_ne_last,
        st->layers ? "true" : "false",
        (unsigned long long)st->layers_applies,
        (double)st->layers_ff,
        (double)st->layers_fb,
        st->num_columns,
        (unsigned long long)st->column_applies,
        (unsigned long long)st->column_failures,
        st->retina ? "true" : "false",
        (unsigned long long)st->ret_applies,
        (double)st->ret_base,
        st->ret_have_base ? "true" : "false",
        (double)st->ret_last,
        st->adaptive ? "true" : "false",
        (unsigned long long)st->adapt_applies,
        (double)st->adapt_override,
        st->adapt_frozen ? "true" : "false");
    nimcp_mutex_unlock(st->mu);

    if (w < 0) {
        return 0;
    }
    return ((uint32_t)w >= cap) ? (int)cap - 1 : w;
}
