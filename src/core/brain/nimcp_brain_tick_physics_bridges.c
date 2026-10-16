/**
 * @file nimcp_brain_tick_physics_bridges.c
 * @brief Tick driver for 4 HALF-STATUE physics bridges (Wave 8C)
 *
 * Called from brain_learn_vector. For each live bridge:
 *   - ephaptic upstream: first advance the field + phase synchronization
 *     by dt_ms; then broadcast state via bio_async, then feed LFP into FFT.
 *   - hh population: advance by dt_ms with zero external current (the HH
 *     pop is a self-contained biophysical statue; the bridge just reports
 *     its membrane state), then process its inbox + auto-broadcast.
 *   - thermo state: advance by dt_ms with zero power/bit-erasure, then
 *     process inbox + auto-broadcast.
 *
 * Each sub-step is null-guarded. The whole function early-returns when
 * brain->physics_bridges_enabled is false so a fully-statued subsystem
 * has zero per-tick cost.
 */

#include "core/brain/nimcp_brain_tick_physics_bridges.h"
#include "core/brain/nimcp_brain_internal.h"

#include "physics/ephaptic/nimcp_ephaptic.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"

#include "physics/bridges/nimcp_ephaptic_bio_async_bridge.h"
#include "physics/bridges/nimcp_ephaptic_fft_bridge.h"
#include "physics/bridges/nimcp_hh_bio_async_bridge.h"
#include "physics/bridges/nimcp_thermo_bio_async_bridge.h"

#include <stdint.h>

void brain_tick_physics_bridges(brain_t brain, float dt_ms) {
    if (!brain || !brain->physics_bridges_enabled) return;

    /* ------------------------------------------------------------------
     * Ephaptic — advance the upstream field, then tick both consumer
     * bridges (bio_async + fft).
     * ---------------------------------------------------------------- */
    if (brain->ephaptic_system) {
        nimcp_ephaptic_system_t* sys =
            (nimcp_ephaptic_system_t*)brain->ephaptic_system;

        /* Advance extracellular field + phase sync. Both are soft-fail. */
        (void)nimcp_ephaptic_update_field(sys, dt_ms);
        (void)nimcp_ephaptic_synchronize(sys, dt_ms);

        /* Compute LFP at origin — the FFT bridge samples this. */
        nimcp_lfp_result_t lfp = (nimcp_lfp_result_t){0};
        const float origin[3] = {0.0f, 0.0f, 0.0f};
        int have_lfp = (nimcp_ephaptic_compute_lfp(sys, origin, &lfp) ==
                        NIMCP_SUCCESS);

        if (brain->ephaptic_bio_async_bridge) {
            ephaptic_bio_async_bridge_t* b =
                (ephaptic_bio_async_bridge_t*)brain->ephaptic_bio_async_bridge;
            (void)ephaptic_bio_async_process_inbox(b);
            (void)ephaptic_bio_async_broadcast_all(
                b, sys, have_lfp ? &lfp : NULL);
        }

        if (brain->ephaptic_fft_bridge && have_lfp) {
            ephaptic_fft_bridge_t* b =
                (ephaptic_fft_bridge_t*)brain->ephaptic_fft_bridge;
            /* Feed the latest LFP sample. The bridge keeps a circular
             * buffer and only runs FFT once it's full — early calls are
             * cheap. */
            (void)ephaptic_fft_add_lfp_result(b, &lfp, dt_ms);
            if (ephaptic_fft_buffer_ready(b)) {
                ephaptic_fft_result_t fft_res = (ephaptic_fft_result_t){0};
                (void)ephaptic_fft_compute_band_power(b, &fft_res);
            }
        }
    }

    /* ------------------------------------------------------------------
     * Hodgkin-Huxley — advance the population at zero external current
     * (the pop is a free-running statue; its bridge just reports state).
     * Then the bio_async bridge drains inbox + auto-broadcasts.
     * ---------------------------------------------------------------- */
    if (brain->hh_population) {
        nimcp_hh_population_t* pop =
            (nimcp_hh_population_t*)brain->hh_population;
        /* I_ext=NULL -> hh_population_update treats as zero-current drive. */
        (void)nimcp_hh_population_update(pop, NULL, dt_ms);
    }
    if (brain->hh_bio_async_bridge) {
        hh_bio_async_bridge_t* b =
            (hh_bio_async_bridge_t*)brain->hh_bio_async_bridge;
        (void)hh_bio_async_process_inbox(b, /*max_messages*/ 32u);
        (void)hh_bio_async_update(b, (uint32_t)dt_ms);
    }

    /* ------------------------------------------------------------------
     * Thermodynamics — advance the accounting state and tick its bridge.
     * Power/bits are zero-drive here; the bridge publishes the steady-
     * state (ATP level, temperature, entropy). Other subsystems can later
     * feed real energy consumption via the bridge's bio-async messages.
     * ---------------------------------------------------------------- */
    if (brain->thermo_state) {
        /* thermo_update takes double dt in seconds — convert ms to s. */
        double dt_s = (double)dt_ms * 1e-3;
        (void)nimcp_thermo_update(
            (nimcp_thermodynamic_state_t*)brain->thermo_state,
            dt_s, /*power*/ 0.0, /*bits*/ 0);
    }
    if (brain->thermo_bio_async_bridge) {
        thermo_bio_async_bridge_t* b =
            (thermo_bio_async_bridge_t*)brain->thermo_bio_async_bridge;
        (void)thermo_bio_async_process_inbox(b, /*max_messages*/ 16u);
        (void)thermo_bio_async_update(b, (uint32_t)dt_ms);
    }
}
