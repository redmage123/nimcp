/**
 * @file nimcp_brain_init_perception_bridges.c
 * @brief Round A/2: activate 10 previously-orphaned perception bridges.
 *
 * See header for rationale. All 10 modules existed as compiled code
 * with no create path before this commit. Each is now:
 *   1. Created at brain init (gated on target subsystem availability)
 *   2. Stored on brain->* field
 *   3. Destroyed at brain teardown
 *   4. Driven by the per-step perception tick (FEP + immune only;
 *      cortical bridges are sensor-input-driven, wait for real data)
 */
#include "core/brain/factory/init/nimcp_brain_init_perception_bridges.h"
#include "core/brain/nimcp_brain_internal.h"

#include "perception/nimcp_visual_cortex_fep_bridge.h"
#include "perception/nimcp_audio_cortex_fep_bridge.h"
#include "perception/nimcp_speech_cortex_fep_bridge.h"
#include "perception/immune/nimcp_visual_immune_bridge.h"
#include "perception/immune/nimcp_audio_immune_bridge.h"
#include "perception/immune/nimcp_speech_immune_bridge.h"
#include "perception/cortical/nimcp_visual_cortical_bridge.h"
#include "perception/cortical/nimcp_audio_cortical_bridge.h"
#include "perception/cortical/nimcp_speech_cortical_bridge.h"
#include "cognitive/memory/core/nimcp_pr_predictive_bridge.h"

#include "utils/logging/nimcp_logging.h"

/* ---------- FEP bridges ------------------------------------------------- */

static void _init_fep_bridges(brain_t brain) {
    /* Visual FEP bridge */
    if (!brain->visual_cortex_fep_bridge) {
        visual_cortex_fep_config_t cfg;
        (void)visual_cortex_fep_bridge_default_config(&cfg);
        visual_cortex_fep_bridge_t* vfb = visual_cortex_fep_bridge_create(&cfg);
        if (vfb && brain->visual_cortex) {
            (void)visual_cortex_fep_bridge_connect_visual_cortex(vfb, brain->visual_cortex);
        }
        brain->visual_cortex_fep_bridge = (void*)vfb;
        if (vfb) NIMCP_LOGGING_INFO("perception_bridges: visual FEP bridge live");
    }
    /* Audio FEP bridge */
    if (!brain->audio_cortex_fep_bridge) {
        audio_cortex_fep_config_t cfg;
        (void)audio_cortex_fep_bridge_default_config(&cfg);
        audio_cortex_fep_bridge_t* afb = audio_cortex_fep_bridge_create(&cfg);
        if (afb && brain->audio_cortex) {
            (void)audio_cortex_fep_bridge_connect_audio_cortex(afb, brain->audio_cortex);
        }
        brain->audio_cortex_fep_bridge = (void*)afb;
        if (afb) NIMCP_LOGGING_INFO("perception_bridges: audio FEP bridge live");
    }
    /* Speech FEP bridge */
    if (!brain->speech_cortex_fep_bridge) {
        speech_cortex_fep_config_t cfg;
        (void)speech_cortex_fep_bridge_default_config(&cfg);
        speech_cortex_fep_bridge_t* sfb = speech_cortex_fep_bridge_create(&cfg);
        if (sfb && brain->speech_cortex) {
            (void)speech_cortex_fep_bridge_connect_speech_cortex(sfb, brain->speech_cortex);
        }
        brain->speech_cortex_fep_bridge = (void*)sfb;
        if (sfb) NIMCP_LOGGING_INFO("perception_bridges: speech FEP bridge live");
    }
}

/* ---------- pr_predictive_bridge: consumes the 3 FEP bridges ----------- */

static void _init_pr_predictive_bridge(brain_t brain) {
    if (brain->pr_predictive_bridge) return;
    pr_predictive_bridge_config_t cfg = pr_predictive_bridge_config_default();
    pr_predictive_bridge_t* prb = pr_predictive_bridge_create(&cfg);
    if (!prb) return;

    if (brain->visual_cortex_fep_bridge) {
        (void)pr_predictive_bridge_connect_visual_fep(prb,
            (visual_cortex_fep_bridge_t*)brain->visual_cortex_fep_bridge);
    }
    if (brain->audio_cortex_fep_bridge) {
        (void)pr_predictive_bridge_connect_audio_fep(prb,
            (audio_cortex_fep_bridge_t*)brain->audio_cortex_fep_bridge);
    }
    if (brain->speech_cortex_fep_bridge) {
        (void)pr_predictive_bridge_connect_speech_fep(prb,
            (speech_cortex_fep_bridge_t*)brain->speech_cortex_fep_bridge);
    }
    brain->pr_predictive_bridge = (void*)prb;
    NIMCP_LOGGING_INFO("perception_bridges: pr_predictive_bridge live");
}

/* ---------- Immune bridges --------------------------------------------- */

static void _init_immune_bridges(brain_t brain) {
    if (!brain->immune_system) {
        NIMCP_LOGGING_DEBUG("perception_bridges: immune_system NULL — skipping immune bridges");
        return;
    }

    if (!brain->visual_immune_bridge && brain->visual_cortex) {
        visual_immune_config_t cfg;
        (void)visual_immune_default_config(&cfg);
        visual_immune_bridge_t* vib = visual_immune_bridge_create(
            &cfg, brain->immune_system, brain->visual_cortex);
        brain->visual_immune_bridge = (void*)vib;
        if (vib) NIMCP_LOGGING_INFO("perception_bridges: visual immune bridge live");
    }
    if (!brain->audio_immune_bridge && brain->audio_cortex) {
        audio_immune_config_t cfg;
        (void)audio_immune_default_config(&cfg);
        audio_immune_bridge_t* aib = audio_immune_bridge_create(
            &cfg, brain->immune_system, brain->audio_cortex);
        brain->audio_immune_bridge = (void*)aib;
        if (aib) NIMCP_LOGGING_INFO("perception_bridges: audio immune bridge live");
    }
    if (!brain->speech_immune_bridge && brain->speech_cortex) {
        speech_immune_config_t cfg;
        (void)speech_immune_default_config(&cfg);
        speech_immune_bridge_t* sib = speech_immune_bridge_create(
            &cfg, brain->immune_system, brain->speech_cortex);
        brain->speech_immune_bridge = (void*)sib;
        if (sib) NIMCP_LOGGING_INFO("perception_bridges: speech immune bridge live");
    }
}

/* ---------- Cortical (hypercolumn-routing) bridges --------------------- */

static void _init_cortical_bridges(brain_t brain) {
    if (!brain->visual_cortical_bridge && brain->visual_cortex) {
        visual_cortical_config_t cfg;
        visual_cortical_default_config(&cfg);
        visual_cortical_bridge_t* vcb = visual_cortical_bridge_create(
            &cfg, brain->visual_cortex);
        brain->visual_cortical_bridge = (void*)vcb;
        if (vcb) NIMCP_LOGGING_INFO("perception_bridges: visual cortical bridge live");
    }
    if (!brain->audio_cortical_bridge && brain->audio_cortex) {
        audio_cortical_config_t cfg;
        audio_cortical_default_config(&cfg);
        audio_cortical_bridge_t* acb = audio_cortical_bridge_create(
            &cfg, brain->audio_cortex);
        brain->audio_cortical_bridge = (void*)acb;
        if (acb) NIMCP_LOGGING_INFO("perception_bridges: audio cortical bridge live");
    }
    if (!brain->speech_cortical_bridge && brain->speech_cortex) {
        speech_cortical_config_t cfg;
        speech_cortical_default_config(&cfg);
        speech_cortical_bridge_t* scb = speech_cortical_bridge_create(
            &cfg, brain->speech_cortex);
        brain->speech_cortical_bridge = (void*)scb;
        if (scb) NIMCP_LOGGING_INFO("perception_bridges: speech cortical bridge live");
    }
}

/* ---------- Public API ------------------------------------------------- */

bool nimcp_brain_factory_init_perception_bridges_subsystem(brain_t brain) {
    if (!brain) return false;
    _init_fep_bridges(brain);
    _init_pr_predictive_bridge(brain);  /* after FEP bridges — depends on them */
    _init_immune_bridges(brain);
    _init_cortical_bridges(brain);
    return true;
}

void nimcp_brain_destroy_perception_bridges(brain_t brain) {
    if (!brain) return;

    /* Destroy consumer FIRST so its non-owning refs into FEP bridges
     * don't dangle during pr_predictive destruction. */
    if (brain->pr_predictive_bridge) {
        pr_predictive_bridge_destroy(
            (pr_predictive_bridge_t*)brain->pr_predictive_bridge);
        brain->pr_predictive_bridge = NULL;
    }
    /* Cortical bridges (sensor-input-driven). */
    if (brain->visual_cortical_bridge) {
        visual_cortical_bridge_destroy(
            (visual_cortical_bridge_t*)brain->visual_cortical_bridge);
        brain->visual_cortical_bridge = NULL;
    }
    if (brain->audio_cortical_bridge) {
        audio_cortical_bridge_destroy(
            (audio_cortical_bridge_t*)brain->audio_cortical_bridge);
        brain->audio_cortical_bridge = NULL;
    }
    if (brain->speech_cortical_bridge) {
        speech_cortical_bridge_destroy(
            (speech_cortical_bridge_t*)brain->speech_cortical_bridge);
        brain->speech_cortical_bridge = NULL;
    }
    /* Immune bridges. */
    if (brain->visual_immune_bridge) {
        visual_immune_bridge_destroy(
            (visual_immune_bridge_t*)brain->visual_immune_bridge);
        brain->visual_immune_bridge = NULL;
    }
    if (brain->audio_immune_bridge) {
        audio_immune_bridge_destroy(
            (audio_immune_bridge_t*)brain->audio_immune_bridge);
        brain->audio_immune_bridge = NULL;
    }
    if (brain->speech_immune_bridge) {
        speech_immune_bridge_destroy(
            (speech_immune_bridge_t*)brain->speech_immune_bridge);
        brain->speech_immune_bridge = NULL;
    }
    /* FEP bridges last (other bridges held non-owning refs into them). */
    if (brain->visual_cortex_fep_bridge) {
        visual_cortex_fep_bridge_destroy(
            (visual_cortex_fep_bridge_t*)brain->visual_cortex_fep_bridge);
        brain->visual_cortex_fep_bridge = NULL;
    }
    if (brain->audio_cortex_fep_bridge) {
        audio_cortex_fep_bridge_destroy(
            (audio_cortex_fep_bridge_t*)brain->audio_cortex_fep_bridge);
        brain->audio_cortex_fep_bridge = NULL;
    }
    if (brain->speech_cortex_fep_bridge) {
        speech_cortex_fep_bridge_destroy(
            (speech_cortex_fep_bridge_t*)brain->speech_cortex_fep_bridge);
        brain->speech_cortex_fep_bridge = NULL;
    }
}

void nimcp_brain_tick_perception_bridges(brain_t brain, float dt_ms) {
    if (!brain) return;
    uint64_t dt = (dt_ms > 0.0f) ? (uint64_t)dt_ms : 0;
    NIMCP_LOGGING_TRACE("perception_bridges_tick: dt_ms=%.3f", (double)dt_ms);

    /* FEP bridges — continuous predictive-coding state advance. */
    if (brain->visual_cortex_fep_bridge) {
        (void)visual_cortex_fep_bridge_update(
            (visual_cortex_fep_bridge_t*)brain->visual_cortex_fep_bridge, dt);
    }
    if (brain->audio_cortex_fep_bridge) {
        (void)audio_cortex_fep_bridge_update(
            (audio_cortex_fep_bridge_t*)brain->audio_cortex_fep_bridge, dt);
    }
    if (brain->speech_cortex_fep_bridge) {
        (void)speech_cortex_fep_bridge_update(
            (speech_cortex_fep_bridge_t*)brain->speech_cortex_fep_bridge, dt);
    }
    /* Immune bridges — cytokine→cortex modulation state. */
    if (brain->visual_immune_bridge) {
        (void)visual_immune_bridge_update(
            (visual_immune_bridge_t*)brain->visual_immune_bridge, dt);
    }
    if (brain->audio_immune_bridge) {
        (void)audio_immune_bridge_update(
            (audio_immune_bridge_t*)brain->audio_immune_bridge, dt);
    }
    if (brain->speech_immune_bridge) {
        (void)speech_immune_bridge_update(
            (speech_immune_bridge_t*)brain->speech_immune_bridge, dt);
    }
    /* Cortical bridges: sensor-input-driven, no per-tick advance. */
}
