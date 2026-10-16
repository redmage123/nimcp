/**
 * @file nimcp_jepa_brain_bridges.c
 * @brief Brain-level wiring for the four JEPA bridges (Phase 4n-q).
 *
 * See header for purpose. Each create/destroy/tick entry no-ops
 * gracefully when the target subsystem is missing so the build stays
 * valid across brain configurations.
 */
#include "cognitive/jepa/nimcp_jepa_brain_bridges.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */

#include "cognitive/omni/nimcp_omni_jepa_bridge.h"
#include "plasticity/neuromodulators/nimcp_neuromod_jepa_bridge.h"
#include "perception/nimcp_audio_jepa_bridge.h"
#include "cognitive/memory/nimcp_engram_jepa_bridge.h"
#include "cognitive/memory/nimcp_consolidation_jepa_bridge.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_jepa_bridge.h"
#include "core/brain/regions/somatosensory/nimcp_soma_jepa_bridge.h"

#include "utils/logging/nimcp_logging.h"

/* Omni state dim — opaque to us; pick a reasonable small value. The
 * training pair is caller-provided vectors so bridge tolerates any dim
 * that matches between the pair, but since the unified tick doesn't
 * currently have natural per-call prev/cur vectors for omni, we just
 * set the size at init. If you later hook the bridge into a real
 * omni_wm_update wrapper, pass the actual state_dim in. */
#define _JEPA_OMNI_STATE_DIM 32u

/* Audio embedding dim (16 mel + 16 MFCC = 32). Matches the bridge's
 * packing logic inside audio_jepa_bridge_train_step. */
#define _JEPA_AUDIO_EMBED_DIM 32u

/* Engram embedding dim. Common engram_t embedding size. */
#define _JEPA_ENGRAM_EMBED_DIM 64u

/* Consolidation pre/post embedding dim — same family as engram. */
#define _JEPA_CONSOLIDATION_EMBED_DIM 64u

/* Cerebellum motor_command / outcome dim. */
#define _JEPA_CEREBELLUM_EMBED_DIM 16u

/* Somatosensory body-state embedding dim (16 pain + 16 temperature). */
#define _JEPA_SOMA_EMBED_DIM 32u

void nimcp_jepa_brain_bridges_init(brain_t brain) {
    if (!brain) return;

    /* Omni world model bridge — allocates on any brain since omni is
     * typically present. The bridge is driver-agnostic; the tick just
     * does not train it until caller-supplied pairs are provided. */
    if (!brain->omni_jepa_bridge) {
        brain->omni_jepa_bridge = (void*)omni_jepa_bridge_create(_JEPA_OMNI_STATE_DIM);
        if (brain->omni_jepa_bridge) {
            NIMCP_LOGGING_INFO("jepa_brain_bridges: omni bridge live (dim=%u)",
                               _JEPA_OMNI_STATE_DIM);
        }
    }

    /* Neuromodulator bridge — requires an active neuromodulator_system. */
    if (!brain->neuromod_jepa_bridge && brain->neuromodulator_system) {
        brain->neuromod_jepa_bridge =
            (void*)neuromod_jepa_bridge_create(brain->neuromodulator_system);
        if (brain->neuromod_jepa_bridge) {
            NIMCP_LOGGING_INFO("jepa_brain_bridges: neuromod bridge live");
        }
    }

    /* Audio cortex bridge — requires an active audio_cortex. */
    if (!brain->audio_jepa_bridge && brain->audio_cortex) {
        brain->audio_jepa_bridge =
            (void*)audio_jepa_bridge_create(brain->audio_cortex,
                                             _JEPA_AUDIO_EMBED_DIM);
        if (brain->audio_jepa_bridge) {
            NIMCP_LOGGING_INFO("jepa_brain_bridges: audio bridge live (dim=%u)",
                               _JEPA_AUDIO_EMBED_DIM);
        }
    }

    /* Engram bridge — standalone, driven by caller record() calls. */
    if (!brain->engram_jepa_bridge) {
        brain->engram_jepa_bridge =
            (void*)engram_jepa_bridge_create(_JEPA_ENGRAM_EMBED_DIM);
        if (brain->engram_jepa_bridge) {
            NIMCP_LOGGING_INFO("jepa_brain_bridges: engram bridge live (dim=%u)",
                               _JEPA_ENGRAM_EMBED_DIM);
        }
    }

    /* Consolidation bridge — caller-driven with pre/post pairs. */
    if (!brain->consolidation_jepa_bridge) {
        brain->consolidation_jepa_bridge =
            (void*)consolidation_jepa_bridge_create(_JEPA_CONSOLIDATION_EMBED_DIM);
        if (brain->consolidation_jepa_bridge) {
            NIMCP_LOGGING_INFO("jepa_brain_bridges: consolidation bridge live (dim=%u)",
                               _JEPA_CONSOLIDATION_EMBED_DIM);
        }
    }

    /* Cerebellum bridge — caller-driven with (motor_cmd, outcome) pairs. */
    if (!brain->cerebellum_jepa_bridge) {
        brain->cerebellum_jepa_bridge =
            (void*)cerebellum_jepa_bridge_create(_JEPA_CEREBELLUM_EMBED_DIM);
        if (brain->cerebellum_jepa_bridge) {
            NIMCP_LOGGING_INFO("jepa_brain_bridges: cerebellum bridge live (dim=%u)",
                               _JEPA_CEREBELLUM_EMBED_DIM);
        }
    }

    /* Somatosensory bridge — self-driving via brain->somatosensory. */
    if (!brain->soma_jepa_bridge && brain->somatosensory) {
        brain->soma_jepa_bridge =
            (void*)soma_jepa_bridge_create(
                (nimcp_somatosensory_t*)brain->somatosensory,
                _JEPA_SOMA_EMBED_DIM);
        if (brain->soma_jepa_bridge) {
            NIMCP_LOGGING_INFO("jepa_brain_bridges: soma bridge live (dim=%u)",
                               _JEPA_SOMA_EMBED_DIM);
        }
    }
}

void nimcp_jepa_brain_bridges_destroy(brain_t brain) {
    if (!brain) return;
    if (brain->omni_jepa_bridge) {
        omni_jepa_bridge_destroy((omni_jepa_bridge_t*)brain->omni_jepa_bridge);
        brain->omni_jepa_bridge = NULL;
    }
    if (brain->neuromod_jepa_bridge) {
        neuromod_jepa_bridge_destroy(
            (neuromod_jepa_bridge_t*)brain->neuromod_jepa_bridge);
        brain->neuromod_jepa_bridge = NULL;
    }
    if (brain->audio_jepa_bridge) {
        audio_jepa_bridge_destroy((audio_jepa_bridge_t*)brain->audio_jepa_bridge);
        brain->audio_jepa_bridge = NULL;
    }
    if (brain->engram_jepa_bridge) {
        engram_jepa_bridge_destroy(
            (engram_jepa_bridge_t*)brain->engram_jepa_bridge);
        brain->engram_jepa_bridge = NULL;
    }
    if (brain->consolidation_jepa_bridge) {
        consolidation_jepa_bridge_destroy(
            (consolidation_jepa_bridge_t*)brain->consolidation_jepa_bridge);
        brain->consolidation_jepa_bridge = NULL;
    }
    if (brain->cerebellum_jepa_bridge) {
        cerebellum_jepa_bridge_destroy(
            (cerebellum_jepa_bridge_t*)brain->cerebellum_jepa_bridge);
        brain->cerebellum_jepa_bridge = NULL;
    }
    if (brain->soma_jepa_bridge) {
        soma_jepa_bridge_destroy((soma_jepa_bridge_t*)brain->soma_jepa_bridge);
        brain->soma_jepa_bridge = NULL;
    }
}

void nimcp_jepa_brain_bridges_tick(brain_t brain,
                                   const float* features,
                                   uint32_t num_features) {
    if (!brain) return;

    /* W8: track bridge activity for aggregated KG emit. */
    uint32_t w8_bridges_active = 0;
    float w8_loss_sum = 0.0f;

    /* --- Self-driving bridges (read their own subsystem state) --- */
    if (brain->neuromod_jepa_bridge) {
        float loss = 0.0f;
        (void)neuromod_jepa_bridge_train_step(
            (neuromod_jepa_bridge_t*)brain->neuromod_jepa_bridge, &loss);
        w8_bridges_active++;
        w8_loss_sum += loss;
    }
    if (brain->audio_jepa_bridge) {
        float loss = 0.0f;
        (void)audio_jepa_bridge_train_step(
            (audio_jepa_bridge_t*)brain->audio_jepa_bridge, &loss);
        w8_bridges_active++;
        w8_loss_sum += loss;
    }
    if (brain->soma_jepa_bridge) {
        float loss = 0.0f;
        (void)soma_jepa_bridge_train_step(
            (soma_jepa_bridge_t*)brain->soma_jepa_bridge, &loss);
        w8_bridges_active++;
        w8_loss_sum += loss;
    }

    /* --- Pair-driven bridges (need caller-provided state vector) --- */
    if (!features || num_features == 0) return;

    if (brain->omni_jepa_bridge) {
        (void)omni_jepa_bridge_tick_from_vec(
            (omni_jepa_bridge_t*)brain->omni_jepa_bridge,
            features, num_features);
    }
    if (brain->engram_jepa_bridge) {
        (void)engram_jepa_bridge_tick_from_vec(
            (engram_jepa_bridge_t*)brain->engram_jepa_bridge,
            features, num_features);
    }
    if (brain->consolidation_jepa_bridge) {
        (void)consolidation_jepa_bridge_tick_from_vec(
            (consolidation_jepa_bridge_t*)brain->consolidation_jepa_bridge,
            features, num_features);
    }
    if (brain->cerebellum_jepa_bridge) {
        (void)cerebellum_jepa_bridge_tick_from_vec(
            (cerebellum_jepa_bridge_t*)brain->cerebellum_jepa_bridge,
            features, num_features);
    }

    /* W8: aggregate emit at tick end. Read-back: jepa_predictor partner
     * check so we know the canonical wired node is reachable. */
    {
        float avg_loss = (w8_bridges_active > 0)
            ? (w8_loss_sum / (float)w8_bridges_active) : 0.0f;
        world_model_kg_emit_jepa_tick(brain, w8_bridges_active, avg_loss);
        (void)world_model_kg_has_partner(brain, "jepa_predictor");
    }
}
