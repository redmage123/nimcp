//=============================================================================
// nimcp_brain_tick_sensorimotor.c — Sensorimotor + emotional region tick driver
//=============================================================================
/**
 * @file nimcp_brain_tick_sensorimotor.c
 * @brief Drives dt-based step functions on six sensorimotor + emotional regions.
 *
 * WHAT: Implements brain_tick_sensorimotor() — a single call that advances
 *       time-dependent state for motor, olfactory, gustatory, somatosensory,
 *       amygdala, and cingulate cortex.
 *
 * WHY:  Prior to Wave 8B-b each region's primary step was a statue. Without
 *       this driver, fear/anxiety never decayed, olfactory/gustatory
 *       adaptation never advanced, somatosensory receptive-field state froze,
 *       motor trajectories never progressed, and cingulate's bio-router inbox
 *       grew unbounded.
 *
 * HOW:  For each region, NULL-guard + `*_enabled` gate, then call the
 *       region-appropriate dt-based step function. Cingulate lacks a public
 *       dt step; its tick drains the per-module bio-router inbox instead
 *       (same state-advancement role fulfilled by the bio_async driver for
 *       other adapters). Motor's update_execution enforces a status precondition
 *       internally — we guard it with motor_get_status() to avoid flooding
 *       the immune system when the motor adapter is idle.
 *
 * AUDIT NOTE: The Wave 8B-b task prompt referenced `motor_adapter_update`,
 * `olfactory_update`, `gustatory_step`, `somatosensory_step`, and
 * `cingulate_step`. Actual API names are `motor_update_execution`,
 * `olfact_update`, `gust_update`, `soma_update`, and (no public dt step —
 * substituted with `cingulate_process_bio_messages`).
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_sensorimotor.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "core/brain/regions/olfactory/nimcp_olfactory.h"
#include "core/brain/regions/gustatory/nimcp_gustatory.h"
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"
#include "core/brain/subcortical/nimcp_amygdala.h"

#include "utils/logging/nimcp_logging.h"

/* Per-tick bio-router inbox drain budget for cingulate. Matches the bound
 * used by brain_tick_bio_async for the other region adapters. */
#define _SM_BIO_BATCH 16u

void brain_tick_sensorimotor(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }
    if (!(dt_ms > 0.0f)) {
        /* amygdala_step rejects non-positive dt via NIMCP_THROW_TO_IMMUNE.
         * Skip the entire pass rather than individually gating each call. */
        return;
    }

    NIMCP_LOGGING_TRACE("brain_tick_sensorimotor: dt_ms=%.3f", (double)dt_ms);

    /* Motor cortex — advances active trajectory time. motor_update_execution
     * asserts status in {EXECUTING, CORRECTING} and will THROW_TO_IMMUNE
     * otherwise, so we guard with motor_get_status() to keep idle motor
     * adapters quiet. */
    if (brain->motor && brain->motor_enabled) {
        motor_adapter_t* adapter = (motor_adapter_t*)brain->motor;
        motor_status_t status = motor_get_status(adapter);
        if (status == MOTOR_STATUS_EXECUTING || status == MOTOR_STATUS_CORRECTING) {
            (void)motor_update_execution(adapter, dt_ms);
        }
    }

    /* Olfactory / piriform cortex — advances sniff cycle + adaptation. */
    if (brain->olfactory && brain->olfactory_enabled) {
        (void)olfact_update((nimcp_olfactory_t*)brain->olfactory, dt_ms);
    }

    /* Gustatory / insular cortex — decays insula + OFC activation, advances
     * taste adaptation. */
    if (brain->gustatory && brain->gustatory_enabled) {
        (void)gust_update((nimcp_gustatory_t*)brain->gustatory, dt_ms);
    }

    /* Somatosensory cortex (S1/S2) — advances receptive-field state,
     * proprioception decay, gate-control on pain. */
    if (brain->somatosensory && brain->somatosensory_enabled) {
        (void)soma_update((nimcp_somatosensory_t*)brain->somatosensory, dt_ms);
    }

    /* Amygdala — decays nuclei activations + anxiety + fear toward baseline;
     * updates spontaneous-recovery on fear memories. No extra signal args —
     * amygdala_step is a pure time-advance step; threat/safety signals enter
     * via amygdala_process_stimulus which is called from other hot paths. */
    if (brain->amygdala && brain->amygdala_enabled) {
        (void)amygdala_step((amygdala_t*)brain->amygdala, dt_ms);
    }

    /* Cingulate cortex (ACC + PCC) — no public dt step function exists.
     * Drain the per-module bio-router inbox to advance state from incoming
     * conflict / error / self-reference messages. Budget matches bio_async
     * driver to bound per-tick wall time. Uses 0.0f-neutral defaults
     * implicitly — cingulate_process_bio_messages does not take signal args. */
    if (brain->cingulate && brain->cingulate_enabled) {
        (void)cingulate_process_bio_messages(
            (cingulate_adapter_t*)brain->cingulate, _SM_BIO_BATCH);
    }
}
