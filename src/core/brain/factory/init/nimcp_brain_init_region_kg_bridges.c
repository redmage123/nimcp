//=============================================================================
// nimcp_brain_init_region_kg_bridges.c - W3 region KG wiring init wave
//=============================================================================
/**
 * @file nimcp_brain_init_region_kg_bridges.c
 * @brief Invokes all W3 region KG-wiring init functions.
 *
 * Part of the KG-integration retrofit Wave W3 (2026-04-24). Registers
 * structural nodes for 10 brain regions that previously had zero KG refs:
 * occipital, somatosensory, motor, broca, gustatory, olfactory, brainstem,
 * raphe, parahippocampal, perirhinal. Each region's wiring_init is
 * null-tolerant (returns 0 when internal_kg is disabled or missing).
 *
 * Runs after Wave 22 (internal_kg creation) — safe to run as its own wave
 * since structural node creation is idempotent (ensure_node pattern).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

#include "core/brain/regions/occipital/bridges/nimcp_occipital_kg_wiring.h"
#include "core/brain/regions/somatosensory/bridges/nimcp_somatosensory_kg_wiring.h"
#include "core/brain/regions/motor/bridges/nimcp_motor_kg_wiring.h"
#include "core/brain/regions/broca/bridges/nimcp_broca_kg_wiring.h"
#include "core/brain/regions/gustatory/bridges/nimcp_gustatory_kg_wiring.h"
#include "core/brain/regions/olfactory/bridges/nimcp_olfactory_kg_wiring.h"
#include "core/brain/regions/brainstem/bridges/nimcp_brainstem_kg_wiring.h"
#include "core/brain/regions/raphe/bridges/nimcp_raphe_kg_wiring.h"
#include "core/brain/regions/parahippocampal/bridges/nimcp_parahippocampal_kg_wiring.h"
#include "core/brain/regions/perirhinal/bridges/nimcp_perirhinal_kg_wiring.h"

/* W4 KG retrofit (2026-04-24): 5 remaining region/peptide subsystems. */
#include "core/brain/regions/endocannabinoid/bridges/nimcp_endocannabinoid_kg_wiring.h"
#include "core/brain/regions/glymphatic/bridges/nimcp_glymphatic_kg_wiring.h"
#include "core/brain/regions/mammillary/bridges/nimcp_mammillary_kg_wiring.h"
#include "core/brain/regions/neuropeptide/bridges/nimcp_neuropeptide_kg_wiring.h"
#include "core/brain/regions/sensory_integration/bridges/nimcp_sensory_integration_kg_wiring.h"

/* W10 KG retrofit (2026-04-24): affective / social / ToM / mirror / personality
 * / empathy / emotion-recognition / grief / shadow / collective cognition
 * structural roots + runtime emit helpers. */
#include "cognitive/kg/nimcp_wave10_affective_kg.h"

/* W13 KG retrofit (2026-04-24): curiosity / meta-learning / consolidation /
 * sleep-wake / self-curriculum / analogical / multiscale / contrastive-self
 * structural roots + runtime emit helpers. */
#include "cognitive/kg/nimcp_wave13_metacog_kg.h"

/* W14 KG retrofit (2026-04-24): math disciplines + game-theory + parietal
 * genius + analogical/hypothesis/insight/financial structural roots +
 * runtime emit/query helpers. */
#include "cognitive/kg/nimcp_wave14_math_genius_kg.h"

/* W12 KG retrofit (2026-04-24): language + communication structural roots
 * + runtime emit helpers. Also registers brain handle for _auto wrappers
 * used by pure-language modules that lack a brain_t pointer. */
#include "cognitive/language/nimcp_w12_language_kg_events.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_region_kg_bridges, MESH_ADAPTER_CATEGORY_SYSTEM)

/**
 * @brief Run every W3 region KG wiring init.
 *
 * @return true on success. Never fails fatally: missing/disabled internal_kg
 *         simply makes each init a no-op (null-tolerant by design).
 */
bool nimcp_brain_factory_init_region_kg_bridges_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_region_kg_bridges_subsystem: brain is NULL");
        return false;
    }

    if (!brain->internal_kg_enabled || !brain->internal_kg) {
        fprintf(stderr, "[W3_REGION_KG] internal_kg disabled/missing - skipping region wirings\n");
        return true;
    }

    fprintf(stderr, "[W3_REGION_KG] Wiring W3 region structural nodes...\n");

    (void)nimcp_occipital_kg_wiring_init(brain);
    (void)nimcp_somatosensory_kg_wiring_init(brain);
    (void)nimcp_motor_kg_wiring_init(brain);
    (void)nimcp_broca_kg_wiring_init(brain);
    (void)nimcp_gustatory_kg_wiring_init(brain);
    (void)nimcp_olfactory_kg_wiring_init(brain);
    (void)nimcp_brainstem_kg_wiring_init(brain);
    (void)nimcp_raphe_kg_wiring_init(brain);
    (void)nimcp_parahippocampal_kg_wiring_init(brain);
    (void)nimcp_perirhinal_kg_wiring_init(brain);

    /* W4: endocannabinoid + glymphatic + mammillary + neuropeptide + sensory_integration */
    (void)nimcp_endocannabinoid_kg_wiring_init(brain);
    (void)nimcp_glymphatic_kg_wiring_init(brain);
    (void)nimcp_mammillary_kg_wiring_init(brain);
    (void)nimcp_neuropeptide_kg_wiring_init(brain);
    (void)nimcp_sensory_integration_kg_wiring_init(brain);

    /* W10: affective/social 10-module structural roots + cross-edges. */
    (void)nimcp_wave10_affective_kg_init(brain);

    /* W13: curiosity/meta/consolidation/sleep/curriculum/analogy/multiscale/
     *      contrastive-self 9-module structural roots + cross-edges. */
    (void)nimcp_wave13_metacog_kg_init(brain);

    /* W14: math disciplines + game-theory + parietal genius + analogical/
     *      hypothesis/insight/financial structural roots + cross-edges. */
    (void)nimcp_wave14_math_genius_kg_init(brain);

    /* W12: language + communication structural roots. Also registers the
     * brain handle so pure-language modules (emergent_language, inner_speech,
     * native_language, tokenizer) can emit via _auto wrappers without
     * threading a brain_t pointer through their APIs. */
    w12_language_ensure_roots(brain);
    w12_language_kg_register_brain(brain);

    fprintf(stderr, "[W3_REGION_KG] W3+W4+W10+W12+W13+W14 structural wiring complete\n");
    return true;
}
