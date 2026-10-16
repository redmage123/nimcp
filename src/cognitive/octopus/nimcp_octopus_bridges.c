/**
 * @file nimcp_octopus_bridges.c
 * @brief Octopus → peer-subsystem bridges (Phase 2a + 2b + 3b).
 *
 * Phase 2a: install the 6 hooks, only ethics is live-gating.
 * Phase 2b: route the remaining 5 hooks through bio_router_broadcast so real
 *          subscribers (swarm / world / FEP / immune / bio-async) receive
 *          structured events. Octopus registers as BIO_MODULE_OCTOPUS and
 *          emits octopus_event_msg_t on each hook firing.
 * Phase 3b: swarm hook also adds rate-limited nodes to brain->internal_kg;
 *          bio hook also encodes rate-limited engrams on low-coherence
 *          events via brain->engram_system.
 *
 * Each wrapper is a thin adapter — pulls the subsystem handle from
 * `brain_t`, transforms arm/aggregate data into an event, broadcasts.
 * All wrappers NULL-guard their inputs and degrade gracefully if a peer
 * isn't initialized (e.g., router not up at install time).
 *
 * SOLID:
 *   SRP: each hook is a single-purpose adapter.
 *   OCP: adding a new event type = add enum + one emit call; no core change.
 *   DIP: octopus core only knows the hook signatures, not bridge contents.
 */
#include "cognitive/octopus/nimcp_octopus_bridges.h"
#include "cognitive/octopus/nimcp_octopus.h"
#include "core/brain/nimcp_brain_internal.h"

#include "cognitive/ethics/nimcp_ethics.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/memory/nimcp_engram.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "perception/nimcp_audio_cortex.h"
#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
#include "snn/nimcp_snn_network.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "portia/nimcp_portia.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Broadcast message schema: a small fixed-size struct the octopus emits
 * on significant events. Consumers read the `type` field to disambiguate.
 * Padded to 32 bytes for consistent router stats. */
typedef enum {
    OCTOPUS_EVT_HEARTBEAT      = 0x0000,  /* every explore() */
    OCTOPUS_EVT_ARM_VETO       = 0x0001,  /* ethics vetoed an arm */
    OCTOPUS_EVT_LOW_COHERENCE  = 0x0002,  /* aggregate coherence dropped */
    OCTOPUS_EVT_DELEGATE       = 0x0003,  /* high-confidence arm → swarm */
    OCTOPUS_EVT_WORLD_OBS      = 0x0004,  /* aggregated latent → world model */
    OCTOPUS_EVT_FEP_SURPRISE   = 0x0005,  /* coherence signal for FEP */
    OCTOPUS_EVT_STRESS_CONTEXT = 0x0006,  /* hypothalamus cortisol signal (Phase 3c) */
    OCTOPUS_EVT_FEAR_CONTEXT   = 0x0007,  /* amygdala fear intensity (Phase 3c) */
} octopus_event_type_t;

/* CRASH FIX: previously a bare 32-byte struct, sent to
 * bio_router_broadcast without a bio_message_header_t prefix. The
 * router validates msg_size >= sizeof(bio_message_header_t) (40
 * bytes) and rejected every one of these, flooding stderr with
 * "message too small (32 < 40 header)" warnings. The rejected
 * messages are the trigger for the recurring brain SIGSEGV
 * observed on Apr 15/19/21 — corrupt/rejected delivery paths
 * eventually dereference a dangling pointer in bio_router's
 * internal routing state.
 *
 * Now properly wrapped with a bio_message_header_t (40 bytes).
 * The octopus event sub-type lives in ev_type (kept as uint16_t
 * to preserve binary layout of the payload section for any
 * downstream consumer that snapshots the post-header bytes). */
typedef struct {
    bio_message_header_t header;      /* 40 bytes — required by router */
    uint16_t             ev_type;     /* octopus_event_type_t (was `type`) */
    uint16_t             arm_id;      /* 0xFFFF if not arm-specific */
    float                value;       /* event-specific magnitude */
    uint32_t             sequence;    /* monotonic per-context; detect drops */
    uint32_t             _pad[3];     /* pad to 16-byte alignment, room for growth */
} octopus_event_msg_t;

/*============================================================================
 * Per-brain bridge state — allows the void* user pointer in each hook to
 * carry the brain reference to the peer subsystem without capturing globals.
 *==========================================================================*/

typedef struct octopus_bridge_state_s {
    brain_t brain;
    uint64_t bio_broadcast_counter;
    bio_module_context_t bio_ctx; /* NULL if router not up at install time */
    uint32_t sequence;            /* monotonic per-context sequence */
    /* Phase 3b integrations. */
    uint64_t engram_encodings;    /* memory episodes encoded from octopus events */
    uint64_t kg_nodes_added;      /* KG nodes added for delegation events */
    /* Phase 3c: hypothalamus + amygdala. */
    float    last_cortisol;       /* most recently observed stress level */
    float    last_fear;           /* most recently observed fear intensity */
    uint64_t stress_broadcasts;   /* number of STRESS_CONTEXT events emitted */
    uint64_t fear_broadcasts;     /* number of FEAR_CONTEXT events emitted */
    uint64_t amygdala_steps;      /* number of amygdala_step() calls driven */
    uint64_t last_step_us;        /* monotonic timestamp of last amygdala_step */
    /* Phase 4a: world-model direct update (remember previous aggregated
     * latent so current call can form a state→next_state transition). */
    float    prev_aggregated[OCTOPUS_ARM_DIM];
    bool     has_prev_aggregated; /* false until first integration fires */
    uint64_t wm_updates;          /* direct omni_wm_update calls attempted  */
    uint64_t fear_conditionings;  /* ethics-veto events fed to amygdala     */
    /* Phase 4b: occipital/visual cortex sampling. */
    uint64_t vision_samples;      /* occipital feature vectors fed to arms   */
    /* Phase 4c: audio cortex sampling. */
    uint64_t audio_samples;       /* audio cortex feature vectors fed to arms */
    /* Phase 4d: somatosensory cortex sampling. */
    uint64_t somato_samples;      /* somatosensory feature vectors fed to arms */
    /* Phase 4e: SNN spike-activity sampling. */
    uint64_t snn_samples;         /* SNN feature vectors fed to arms         */
    /* Phase 4f: neuromodulator sampling. */
    uint64_t neuromod_samples;    /* neuromod concentration vectors fed to arms */
    /* Phase 4g: peer-module (Portia + Dragonfly) sampling. */
    uint64_t peer_samples;        /* peer cognitive-module vectors fed to arms */
    /* Phase 5A: per-tick cadence counter for train-step/NG-regularize. */
    uint64_t tick_counter;        /* number of nimcp_octopus_tick calls */
} octopus_bridge_state_t;

/* Publish all bridge-owned counters onto the octopus core's stats struct so
 * a single octopus_get_stats() call sees both core and bridge activity.
 * Called at the end of the immune hook since that fires every explore. */
static void _publish_bridge_stats(octopus_bridge_state_t* st) {
    if (!st || !st->brain || !st->brain->octopus) return;
    octopus_set_bridge_stats(
        (octopus_system_t*)st->brain->octopus,
        st->engram_encodings,
        st->kg_nodes_added,
        st->stress_broadcasts,
        st->fear_broadcasts,
        st->amygdala_steps,
        st->last_cortisol,
        st->last_fear,
        st->wm_updates,
        st->fear_conditionings,
        st->vision_samples,
        st->audio_samples,
        st->somato_samples,
        st->snn_samples,
        st->neuromod_samples,
        st->peer_samples);
}

/* Lazy register against bio_router if install-time registration was
 * skipped (router wasn't up yet). Safe to call repeatedly — already
 * registered states are a no-op. */
static void _ensure_registered(octopus_bridge_state_t* st) {
    if (!st || st->bio_ctx) return;
    if (!bio_router_is_initialized()) return;
    bio_module_info_t info = {0};
    info.module_id = BIO_MODULE_OCTOPUS;
    info.module_name = "octopus";
    info.inbox_capacity = 0;
    info.user_data = st;
    st->bio_ctx = bio_router_register_module(&info);
    if (st->bio_ctx) {
        NIMCP_LOGGING_INFO("octopus_bridges: lazy-registered with bio_router");
    }
}

/* DRY helper: pack + broadcast one event. NULL-guards the router context
 * so degraded operation (router not initialized) is a no-op instead of a
 * crash. Returns true iff broadcast succeeded.
 *
 * CRASH FIX: now initializes bio_message_header_t with the octopus
 * module id and the full message size (including the header prefix).
 * Without this, bio_router rejected every send with "message too
 * small (32 < 40 header)" — and those rejections were the trigger for
 * the repeated brain SIGSEGV. */
static bool _emit(octopus_bridge_state_t* st,
                   octopus_event_type_t type,
                   uint16_t arm_id,
                   float value) {
    if (!st) return false;
    _ensure_registered(st);
    if (!st->bio_ctx) return false;
    octopus_event_msg_t msg = {0};
    bio_msg_init_header(&msg.header,
                        (bio_message_type_t)BIO_MODULE_OCTOPUS,  /* use module id as event type */
                        BIO_MODULE_OCTOPUS,
                        0,  /* broadcast: target=0 */
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.ev_type  = (uint16_t)type;
    msg.arm_id   = arm_id;
    msg.value    = value;
    msg.sequence = ++st->sequence;
    nimcp_error_t rc = bio_router_broadcast(st->bio_ctx, &msg, sizeof(msg));
    if (rc == NIMCP_SUCCESS) st->bio_broadcast_counter++;
    return rc == NIMCP_SUCCESS;
}

/*============================================================================
 * Hook implementations
 *==========================================================================*/

/* Ethics: convert arm latent → action_context_t, evaluate, return allowed.
 * Copies the latent into a local writable buffer to avoid a cast-away-const
 * (action_context_t.features is `float*` by convention even though the
 * evaluator reads it as const; copying is the safe portable path). */
static bool _ethics_hook(const octopus_arm_t* arm, void* user) {
    octopus_bridge_state_t* st = (octopus_bridge_state_t*)user;
    if (!st || !arm || !st->brain || !st->brain->ethics) return true;

    float features_copy[OCTOPUS_ARM_DIM];
    memcpy(features_copy, arm->latent, sizeof(features_copy));

    /* First 5 floats map to the 5 specific violation metrics (clamped to
     * [0,1] since they represent probability-like magnitudes); the rest
     * are the general feature vector. Heuristic projection; higher-fidelity
     * mapping is Phase 2b work. */
    action_context_t ctx = {0};
    ctx.features = features_copy;
    ctx.num_features = OCTOPUS_ARM_DIM;
    ctx.affected_agents = NULL;
    ctx.num_affected_agents = 0;
    ctx.predicted_harm      = features_copy[0] > 0 ? features_copy[0] : 0.0f;
    ctx.fairness_violation  = features_copy[1] > 0 ? features_copy[1] : 0.0f;
    ctx.deception_level     = features_copy[2] > 0 ? features_copy[2] : 0.0f;
    ctx.autonomy_violation  = features_copy[3] > 0 ? features_copy[3] : 0.0f;
    ctx.privacy_violation   = features_copy[4] > 0 ? features_copy[4] : 0.0f;
    ctx.consent_violation   = 0.0f;

    /* brain->ethics is declared ethics_engine_t in brain_internal.h — no cast needed. */
    ethics_evaluation_t eval = ethics_engine_evaluate_action(
        st->brain->ethics, &ctx);

    /* Phase 4a: feed vetoed patterns to the amygdala as conditioned
     * stimuli. This closes the loop between ethics (explicit reasoning)
     * and the amygdala (fast pattern-based threat response). Over time
     * the amygdala learns to fire on patterns that lead to ethics
     * violations — so future similar patterns produce fear-based
     * inhibition before the ethics engine even evaluates. */
    if (!eval.allowed &&
        st->brain->amygdala && st->brain->amygdala_enabled) {
        amyg_conditioned_stimulus_t cs = {0};
        uint32_t nf = OCTOPUS_ARM_DIM < AMYG_MAX_CS_FEATURES
                    ? OCTOPUS_ARM_DIM : AMYG_MAX_CS_FEATURES;
        memcpy(cs.features, features_copy, sizeof(float) * nf);
        cs.n_features = nf;
        cs.modality   = 0;  /* generic internal-cognitive modality */
        cs.salience   = eval.confidence;
        (void)amygdala_process_stimulus(
            (amygdala_t*)st->brain->amygdala, &cs, NULL);
        st->fear_conditionings++;
    }
    return eval.allowed;
}

/* Swarm delegation: (1) broadcast DELEGATE event through bio_router,
 * (2) add a node to the internal KG recording this high-confidence
 * arm-finding so downstream reasoners can query past delegations.
 * KG additions are rate-limited to every 64th delegation (power-of-two
 * mask avoids % so the check stays constant-time) to avoid graph bloat
 * from identical high-confidence steady states. */
static void _swarm_hook(const octopus_arm_t* arm, void* user) {
    octopus_bridge_state_t* st = (octopus_bridge_state_t*)user;
    if (!st || !arm) return;
    _emit(st, OCTOPUS_EVT_DELEGATE, (uint16_t)arm->id, arm->confidence);

    /* KG integration: add a node for notable delegations. */
    if (st->brain && st->brain->internal_kg && st->brain->internal_kg_enabled) {
        if ((st->kg_nodes_added & 0x3F) == 0) {  /* every 64th */
            char name[64], desc[128];
            snprintf(name, sizeof(name), "octopus_delegate_%llu",
                     (unsigned long long)st->kg_nodes_added);
            snprintf(desc, sizeof(desc),
                     "Octopus arm %u delegated with confidence %.3f "
                     "(sequence=%u)",
                     arm->id, arm->confidence, st->sequence);
            brain_kg_add_node(st->brain->internal_kg, name,
                              BRAIN_KG_NODE_COGNITIVE, desc);
        }
        st->kg_nodes_added++;
    }
}

/* World model: (1) broadcast WORLD_OBS via bio_router, (2) directly call
 * omni_wm_update so the world model learns octopus's state→next_state
 * transitions. state = previous aggregated latent, action = current
 * aggregated, next_state = current aggregated (the arms collectively
 * *are* the action that moved the system). Reward proxy is central
 * coherence — when arms agree the transition was internally consistent.
 *
 * The first invocation has no prev_aggregated, so we skip the
 * omni_wm_update and just stash the current aggregate. Subsequent calls
 * form real transitions. Stack-allocated state wrappers avoid heap churn. */
static void _world_hook(const float* aggregated, uint32_t len, void* user) {
    octopus_bridge_state_t* st = (octopus_bridge_state_t*)user;
    if (!st || !aggregated || len == 0) return;
    _emit(st, OCTOPUS_EVT_WORLD_OBS, 0xFFFF, 0.0f);

    if (!st->brain || !st->brain->omni_world_model) {
        /* No world model on this brain — remember aggregate for later. */
        if (len <= OCTOPUS_ARM_DIM) {
            memcpy(st->prev_aggregated, aggregated, sizeof(float) * len);
            st->has_prev_aggregated = true;
        }
        return;
    }

    uint32_t n = (len <= OCTOPUS_ARM_DIM) ? len : OCTOPUS_ARM_DIM;
    if (st->has_prev_aggregated) {
        /* Read coherence from octopus core stats — that's our reward proxy.
         * Valid because octopus core publishes central_coherence BEFORE
         * firing hooks (see octopus_integrate). */
        octopus_stats_t cur_stats;
        octopus_get_stats((octopus_system_t*)st->brain->octopus, &cur_stats);
        float reward = cur_stats.central_coherence;

        /* next_state is a stack copy of `aggregated` so we don't alias
         * prev_aggregated when we update it at the end of this function. */
        float next_buf[OCTOPUS_ARM_DIM];
        memcpy(next_buf, aggregated, sizeof(float) * n);

        /* Stack-allocated state wrappers; omni_wm_state_t references our
         * buffers via `values` pointer. omni_wm_update() is synchronous
         * (copies what it needs for replay), so these pointers are safe
         * for the duration of the call. */
        omni_wm_state_t s_prev = {0};
        s_prev.values      = st->prev_aggregated;
        s_prev.dim         = n;
        s_prev.uncertainty = 1.0f - reward;

        omni_wm_state_t s_next = {0};
        s_next.values      = next_buf;
        s_next.dim         = n;
        s_next.uncertainty = 1.0f - reward;

        /* action = the next_state itself (arms' collective decision IS
         * what moved the system forward). action_dim matches state dim. */
        omni_wm_update((omni_world_model_t*)st->brain->omni_world_model,
                       &s_prev, next_buf, n, &s_next, reward);
        st->wm_updates++;
    }
    /* Store current aggregate for next transition. */
    memcpy(st->prev_aggregated, aggregated, sizeof(float) * n);
    st->has_prev_aggregated = true;
}

/* FEP: broadcast the coherence signal. Low coherence = high surprise =
 * free-energy proxy. FEP orchestrator can subscribe to this and feed it
 * into its surprise accumulator. */
static void _fep_hook(float coherence, void* user) {
    octopus_bridge_state_t* st = (octopus_bridge_state_t*)user;
    _emit(st, OCTOPUS_EVT_FEP_SURPRISE, 0xFFFF, coherence);
}

/* Bio-async event broadcast: route octopus events (arm vetoes, low
 * coherence) through the real bio_router. Event string is mapped to one
 * of our typed enum values. Unrecognized events get dropped (safe-fail).
 *
 * Phase 3b: low-coherence events also trigger an episodic engram —
 * the brain remembers surprising-octopus moments so they can be replayed
 * during consolidation. Rate-limited so identical prolonged low-coherence
 * doesn't flood the engram store. */
static void _bio_hook(const char* event, float value, void* user) {
    octopus_bridge_state_t* st = (octopus_bridge_state_t*)user;
    if (!st || !event) return;
    octopus_event_type_t type;
    if (strcmp(event, "octopus_low_coherence") == 0) {
        type = OCTOPUS_EVT_LOW_COHERENCE;
    } else if (strcmp(event, "octopus_arm_vetoed") == 0) {
        type = OCTOPUS_EVT_ARM_VETO;
    } else {
        return;  /* unknown event — don't emit a spurious message */
    }
    /* For ARM_VETO events the octopus core passes `(float)arm_id` as
     * value; truncate to uint16_t to carry that id in the message header.
     * For LOW_COHERENCE, value IS the coherence, so arm_id is "n/a" (0xFFFF). */
    uint16_t arm_id = (type == OCTOPUS_EVT_ARM_VETO)
                    ? (uint16_t)value : (uint16_t)0xFFFF;
    _emit(st, type, arm_id, value);

    /* Phase 3b: engram encoding for significant episodes. Low coherence
     * means "arms are split" = surprising experience = worth remembering.
     * Only encode every 32nd such event to avoid flooding consolidation. */
    if (type == OCTOPUS_EVT_LOW_COHERENCE &&
        st->brain && st->brain->engram_system &&
        (st->engram_encodings & 0x1F) == 0) {
        /* Build a minimal engram pattern from the arm broadcast states.
         * n_arms neuron-ids (synthetic but stable), activations = the
         * arm's broadcast state at the moment of low coherence. */
        octopus_system_t* oc = (octopus_system_t*)st->brain->octopus;
        if (oc) {
            uint32_t n_arms = octopus_get_n_arms(oc);
            uint32_t ids[16];       /* max arms = 16 */
            float    acts[16];
            uint32_t count = 0;
            for (uint32_t a = 0; a < n_arms && count < 16; a++) {
                ids[count]  = 0x50000000u | a;  /* synthetic id space */
                acts[count] = octopus_get_broadcast_state(oc, a);
                count++;
            }
            emotional_tag_t emo = {0};
            emo.valence   = -0.3f;  /* low coherence → mildly negative */
            emo.arousal   = 1.0f - value;  /* lower coherence → higher arousal */
            emo.intensity = 1.0f - value;
            emo.timestamp_ms = 0;
            engram_encode(st->brain->engram_system, ids, acts, count,
                          MEMORY_TYPE_EPISODIC, emo);
        }
    }
    if (type == OCTOPUS_EVT_LOW_COHERENCE) st->engram_encodings++;
}

/* Immune: fires on every explore(). Emit a heartbeat, step the amygdala
 * forward (using real wall-clock dt), then on a 32x rate limit poll
 * hypothalamus/amygdala state and broadcast STRESS/FEAR context events.
 *
 * Driving amygdala_step() here is cheap (μs-scale for a small module)
 * and guarantees it advances every explore. Without this the amygdala's
 * internal temporal dynamics would stay frozen and fear responses would
 * be stale or all-zero. */
static void _immune_hook(void* user) {
    octopus_bridge_state_t* st = (octopus_bridge_state_t*)user;
    _emit(st, OCTOPUS_EVT_HEARTBEAT, 0xFFFF, 0.0f);
    if (!st || !st->brain) return;

    /* === Drive amygdala_step per explore with real dt === */
    if (st->brain->amygdala && st->brain->amygdala_enabled) {
        uint64_t now_us = nimcp_time_get_us();
        float dt_ms = 0.0f;
        if (st->last_step_us != 0 && now_us > st->last_step_us) {
            dt_ms = (float)(now_us - st->last_step_us) / 1000.0f;
            /* Sanity cap: if the brain was paused, don't feed a huge dt. */
            if (dt_ms > 5000.0f) dt_ms = 5000.0f;
        }
        st->last_step_us = now_us;
        if (dt_ms > 0.0f) {
            (void)amygdala_step((amygdala_t*)st->brain->amygdala, dt_ms);
            st->amygdala_steps++;
        }
    }

    /* Publish bridge counters every explore so octopus_get_stats is fresh. */
    _publish_bridge_stats(st);

    /* Rate-limit the slower physiological-signal polls. */
    if ((st->stress_broadcasts & 0x1F) != 0) {
        st->stress_broadcasts++;
        return;
    }
    st->stress_broadcasts++;

    if (st->brain->hypothalamus && st->brain->hypothalamus_enabled) {
        hypothalamus_state_t hs = {0};
        if (hypothalamus_get_state(
                (const hypothalamus_adapter_t*)st->brain->hypothalamus, &hs)) {
            float cortisol = hs.hpa_axis.cortisol_level;
            st->last_cortisol = cortisol;
            _emit(st, OCTOPUS_EVT_STRESS_CONTEXT, 0xFFFF, cortisol);
        }
    }

    if (st->brain->amygdala && st->brain->amygdala_enabled) {
        amyg_fear_response_t resp = {0};
        if (amygdala_get_response(
                (const amygdala_t*)st->brain->amygdala, &resp) == 0) {
            st->last_fear = resp.fear_intensity;
            st->fear_broadcasts++;
            _emit(st, OCTOPUS_EVT_FEAR_CONTEXT, 0xFFFF, resp.fear_intensity);
        }
    }
}

/*============================================================================
 * Public install function
 *==========================================================================*/

bool nimcp_octopus_install_bridges(brain_t brain) {
    if (!brain || !brain->octopus) return false;
    if (brain->octopus_bridge_state) return true;  /* idempotent */

    /* Allocate bridge state and track it on the brain so destroy path
     * can free it (see nimcp_brain_part_lifecycle.c octopus teardown). */
    octopus_bridge_state_t* st = (octopus_bridge_state_t*)nimcp_calloc(
        1, sizeof(octopus_bridge_state_t));
    if (!st) return false;
    st->brain = brain;
    brain->octopus_bridge_state = (void*)st;

    /* Register with bio_router so broadcasts carry a real module identity.
     * Safe-fail: if the router isn't initialized yet, st->bio_ctx stays
     * NULL and _emit() becomes a no-op. The module can be re-installed
     * later once the router is up. */
    if (bio_router_is_initialized()) {
        bio_module_info_t info = {0};
        info.module_id = BIO_MODULE_OCTOPUS;
        info.module_name = "octopus";
        info.inbox_capacity = 0;   /* use router default */
        info.user_data = st;
        st->bio_ctx = bio_router_register_module(&info);
        if (!st->bio_ctx) {
            NIMCP_LOGGING_WARN("octopus_bridges: router registration failed");
        }
    }

    octopus_system_t* ctx = (octopus_system_t*)brain->octopus;

    octopus_set_ethics_hook(ctx, _ethics_hook, st);
    octopus_set_swarm_hook (ctx, _swarm_hook,  st);
    octopus_set_world_hook (ctx, _world_hook,  st);
    octopus_set_fep_hook   (ctx, _fep_hook,    st);
    octopus_set_bio_hook   (ctx, _bio_hook,    st);
    octopus_set_immune_hook(ctx, _immune_hook, st);

    NIMCP_LOGGING_INFO("octopus_bridges: 6 hooks installed "
                       "(ethics=%s, router=%s)",
                       brain->ethics ? "live" : "no-peer",
                       st->bio_ctx   ? "live" : "not-up-yet");
    return true;
}

/* Called from the brain lifecycle destroy path AFTER octopus_destroy().
 * Unregisters the bio_router module before the state is freed so the
 * router doesn't retain a dangling context reference. */
void nimcp_octopus_uninstall_bridges(brain_t brain) {
    if (!brain || !brain->octopus_bridge_state) return;
    octopus_bridge_state_t* st =
        (octopus_bridge_state_t*)brain->octopus_bridge_state;
    if (st->bio_ctx) {
        bio_router_unregister_module(st->bio_ctx);
        st->bio_ctx = NULL;
    }
}

/*============================================================================
 * Phase 4b: occipital → octopus sampling.
 *
 * Packs current visual-cortex activity into the 64-channel arm input vector.
 * Layout is intentionally stable so the arms learn consistent spatial
 * semantics over training runs (DFA, entropy stay meaningful across rollouts):
 *
 *   [ 0 ..  7]  V1 orientation histogram, L1-normalized.
 *   [ 8 .. 15]  Per-area feature count density (log1p-scaled, clamped).
 *   [16 .. 23]  Top-8 visual-feature strengths (any area).
 *   [24 .. 31]  Top-8 motion-vector magnitudes (V5/MT).
 *   [32 .. 47]  Top-8 V4 color hues+sats (hue/360 then sat, interleaved).
 *   [48 .. 55]  Global summary: motion dx, dy, total feature count, timing.
 *   [56 .. 63]  Reserved (zeroed).
 *
 * All writes go into a 64-slot scratch; only out_dim slots are copied out.
 *==========================================================================*/

#define _OCTOPUS_VISION_CHANNELS 64u

/* Helper: find max-of-N with index (tiny inline; no allocs). */
static uint32_t _topk_unsorted(const float* in, uint32_t n,
                                float* out_vals, uint32_t k) {
    if (!in || !out_vals || n == 0 || k == 0) return 0;
    uint32_t written = 0;
    /* Cheap pass: copy first min(k, n) directly, then sort-insert the rest.
     * Full priority-queue would be overkill at k=8. */
    uint32_t first = (n < k) ? n : k;
    for (uint32_t i = 0; i < first; i++) out_vals[i] = in[i];
    written = first;
    for (uint32_t i = first; i < n; i++) {
        /* Find current min slot. */
        uint32_t min_idx = 0;
        for (uint32_t j = 1; j < k; j++) {
            if (out_vals[j] < out_vals[min_idx]) min_idx = j;
        }
        if (in[i] > out_vals[min_idx]) out_vals[min_idx] = in[i];
    }
    /* Pad unused slots (n < k case) with zeros. */
    for (uint32_t i = written; i < k; i++) out_vals[i] = 0.0f;
    return (n < k) ? n : k;
}

uint32_t nimcp_octopus_sample_occipital_vec(brain_t brain,
                                            float* out_vec,
                                            uint32_t out_dim) {
    if (!brain || !out_vec || out_dim == 0) return 0;
    if (!brain->occipital || !brain->occipital_enabled) return 0;

    if (out_dim > _OCTOPUS_VISION_CHANNELS) out_dim = _OCTOPUS_VISION_CHANNELS;

    float scratch[_OCTOPUS_VISION_CHANNELS] = {0};

    occipital_adapter_t* occ = (occipital_adapter_t*)brain->occipital;

    /* ---- Block A: per-area feature counts (channels 8..15) ---------------- */
    uint32_t counts[VISUAL_AREA_COUNT] = {0};
    uint32_t total_count = 0;
    for (uint32_t area = 0; area < VISUAL_AREA_COUNT; area++) {
        uint32_t c = occipital_get_feature_count(occ, (visual_area_t)area);
        counts[area] = c;
        total_count += c;
    }
    /* log1p-scale (counts can be 0..~512) and clamp to [0,1]. */
    for (uint32_t area = 0; area < VISUAL_AREA_COUNT && area < 8; area++) {
        float v = log1pf((float)counts[area]) / 6.5f;  /* ~log1p(665) */
        if (v > 1.0f) v = 1.0f;
        scratch[8 + area] = v;
    }

    /* ---- Block B: pull per-area features, build orientation histogram,
     *              collect feature strengths ------------------------------- */
    /* Bounded buffer: up to 128 features across all areas. Anything beyond
     * is summarized by the per-area count already. */
    enum { _MAX_FEATS = 128 };
    visual_feature_t feats[_MAX_FEATS];
    uint32_t feats_used = 0;
    for (uint32_t area = 0; area < VISUAL_AREA_COUNT; area++) {
        if (feats_used >= _MAX_FEATS) break;
        uint32_t cap = _MAX_FEATS - feats_used;
        if (occipital_get_features(occ, (visual_area_t)area,
                                    feats + feats_used, &cap)) {
            feats_used += cap;
        }
    }

    /* Orientation histogram (8 bins over [0, π)). */
    float ori_bins[8] = {0};
    float strengths[_MAX_FEATS];
    uint32_t n_strengths = 0;
    for (uint32_t i = 0; i < feats_used; i++) {
        /* Unwrap orientation into [0, π) then bin. */
        float o = feats[i].orientation;
        if (o < 0.0f) o = -o;
        float pi = 3.14159265358979323846f;
        while (o >= pi) o -= pi;
        uint32_t bin = (uint32_t)((o / pi) * 8.0f);
        if (bin >= 8) bin = 7;
        ori_bins[bin] += feats[i].strength;
        if (n_strengths < _MAX_FEATS) strengths[n_strengths++] = feats[i].strength;
    }
    /* L1-normalize ori histogram into [0..1] bins. */
    float ori_sum = 0.0f;
    for (uint32_t b = 0; b < 8; b++) ori_sum += ori_bins[b];
    if (ori_sum > 1e-6f) {
        for (uint32_t b = 0; b < 8; b++) scratch[b] = ori_bins[b] / ori_sum;
    }
    /* Top-8 feature strengths. */
    (void)_topk_unsorted(strengths, n_strengths, &scratch[16], 8);

    /* ---- Block C: motion vectors (channels 24..31, plus global at 48..49) */
    motion_vector_t mvecs[_MAX_FEATS];
    uint32_t n_mvecs = _MAX_FEATS;
    float motion_mags[_MAX_FEATS];
    uint32_t n_mags = 0;
    float gmotion_dx = 0.0f, gmotion_dy = 0.0f;
    if (occipital_get_motion_vectors(occ, mvecs, &n_mvecs) && n_mvecs > 0) {
        for (uint32_t i = 0; i < n_mvecs && i < _MAX_FEATS; i++) {
            float m = sqrtf(mvecs[i].dx * mvecs[i].dx +
                            mvecs[i].dy * mvecs[i].dy);
            motion_mags[n_mags++] = m * mvecs[i].confidence;
            gmotion_dx += mvecs[i].dx;
            gmotion_dy += mvecs[i].dy;
        }
        if (n_mvecs > 0) {
            gmotion_dx /= (float)n_mvecs;
            gmotion_dy /= (float)n_mvecs;
        }
        (void)_topk_unsorted(motion_mags, n_mags, &scratch[24], 8);
    }

    /* ---- Block D: color percepts from V4 (channels 32..47 interleaved) ---- */
    color_percept_t cpercepts[8];
    uint32_t n_cpercepts = 8;
    if (occipital_get_color_percepts(occ, cpercepts, &n_cpercepts)) {
        for (uint32_t i = 0; i < n_cpercepts && i < 8; i++) {
            /* Normalize hue from [0,360] to [0,1]; sat already in [0,1]. */
            scratch[32 + 2 * i + 0] = cpercepts[i].hue / 360.0f;
            scratch[32 + 2 * i + 1] = cpercepts[i].saturation;
        }
    }

    /* ---- Block E: global summary (channels 48..55) ------------------------ */
    /* tanh-squash global motion to [-1,1]. */
    scratch[48] = tanhf(gmotion_dx);
    scratch[49] = tanhf(gmotion_dy);
    /* Total-feature density (log-scaled). */
    scratch[50] = log1pf((float)total_count) / 8.0f;
    if (scratch[50] > 1.0f) scratch[50] = 1.0f;
    /* Channels 51..55 reserved for future timing / attention signals. */

    memcpy(out_vec, scratch, sizeof(float) * out_dim);
    return out_dim;
}

int nimcp_octopus_explore_from_occipital(brain_t brain) {
    if (!brain || !brain->octopus || !brain->octopus_enabled) return -1;
    if (!brain->occipital || !brain->occipital_enabled) return -1;

    float vision_vec[_OCTOPUS_VISION_CHANNELS] = {0};
    uint32_t n = nimcp_octopus_sample_occipital_vec(
        brain, vision_vec, _OCTOPUS_VISION_CHANNELS);
    if (n == 0) return -1;

    /* Increment AFTER a successful explore. The immune-hook publish in
     * *this* explore reports the pre-call count (accurate — this call
     * hasn't succeeded yet). Next explore's publish includes it. This
     * avoids any window where observers see a pre-rollback inflated value. */
    int rc = octopus_explore((octopus_system_t*)brain->octopus,
                             vision_vec, n);
    if (rc == 0 && brain->octopus_bridge_state) {
        octopus_bridge_state_t* st =
            (octopus_bridge_state_t*)brain->octopus_bridge_state;
        st->vision_samples++;
    }
    return rc;
}

/*============================================================================
 * Phase 4c: audio cortex → octopus sampling.
 *
 * Audio cortex doesn't expose a "current features" accessor the way occipital
 * does — it processes audio on demand. But it DOES cache the last forward
 * pass's mel/MFCC/quality/salience/coherence in an audio_training_state_t
 * (for gradient feedback). That cached snapshot is exactly what we need as
 * "current perceived auditory state" — no audio_data argument required here.
 *
 * Layout (64 channels):
 *   [ 0 .. 15]  Mel filterbank features (padded, divided by max for [-1,1])
 *   [16 .. 31]  MFCC coefficients (tanh-squashed to [-1,1])
 *   [32 .. 47]  Reserved
 *   [48]        Audio quality
 *   [49]        Speech salience
 *   [50]        Temporal coherence
 *   [51]        log1p(frames_processed) / 20  (saturates ~5e8 frames)
 *   [52]        log1p(memories_stored) / 7    (AUDIO_MAX_MEMORIES=1000)
 *   [53]        tanh(avg_processing_time_ms / 10)
 *   [54 .. 63]  Reserved
 *==========================================================================*/

#define _OCTOPUS_AUDIO_CHANNELS 64u

uint32_t nimcp_octopus_sample_audio_cortex_vec(brain_t brain,
                                                float* out_vec,
                                                uint32_t out_dim) {
    if (!brain || !out_vec || out_dim == 0) return 0;
    if (!brain->audio_cortex) return 0;

    if (out_dim > _OCTOPUS_AUDIO_CHANNELS) out_dim = _OCTOPUS_AUDIO_CHANNELS;

    float scratch[_OCTOPUS_AUDIO_CHANNELS] = {0};

    audio_cortex_t* ac = (audio_cortex_t*)brain->audio_cortex;

    /* ---- Block A: cached training-state snapshot (mel + MFCC + quality) -- */
    audio_training_state_t ts = {0};
    if (audio_cortex_get_training_state(ac, &ts) == 0 && ts.valid) {
        /* Mel features — normalize by max so scale stays in [-1,1]. */
        if (ts.mel_features && ts.num_mel_filters > 0) {
            float mmax = 0.0f;
            for (uint32_t i = 0; i < ts.num_mel_filters; i++) {
                float a = fabsf(ts.mel_features[i]);
                if (a > mmax) mmax = a;
            }
            if (mmax < 1e-6f) mmax = 1.0f;
            uint32_t nmel = ts.num_mel_filters < 16 ? ts.num_mel_filters : 16;
            for (uint32_t i = 0; i < nmel; i++) {
                scratch[i] = ts.mel_features[i] / mmax;
            }
        }
        /* MFCCs — tanh-squash since they can be unbounded. */
        if (ts.mfcc_features && ts.num_mfcc > 0) {
            uint32_t nmfcc = ts.num_mfcc < 16 ? ts.num_mfcc : 16;
            for (uint32_t i = 0; i < nmfcc; i++) {
                scratch[16 + i] = tanhf(ts.mfcc_features[i]);
            }
        }
        scratch[48] = ts.quality;
        scratch[49] = ts.speech_salience;
        scratch[50] = ts.temporal_coherence;
    }
    /* Else: no valid cached state (cortex idle or training-mode off). Leave
     * scratch[0..47] zeroed; we still fill stats-derived channels below so
     * downstream can distinguish "silent but alive" from "no cortex at all". */

    /* ---- Block B: summary stats --------------------------------------------*/
    audio_cortex_stats_t stats = {0};
    if (audio_cortex_get_stats(ac, &stats)) {
        float f = log1pf((float)stats.frames_processed) / 20.0f;
        if (f > 1.0f) f = 1.0f;
        scratch[51] = f;
        float m = log1pf((float)stats.memories_stored) / 7.0f;
        if (m > 1.0f) m = 1.0f;
        scratch[52] = m;
        scratch[53] = tanhf(stats.avg_processing_time / 10.0f);
    }

    memcpy(out_vec, scratch, sizeof(float) * out_dim);
    return out_dim;
}

int nimcp_octopus_explore_from_audio_cortex(brain_t brain) {
    if (!brain || !brain->octopus || !brain->octopus_enabled) return -1;
    if (!brain->audio_cortex) return -1;

    float audio_vec[_OCTOPUS_AUDIO_CHANNELS] = {0};
    uint32_t n = nimcp_octopus_sample_audio_cortex_vec(
        brain, audio_vec, _OCTOPUS_AUDIO_CHANNELS);
    if (n == 0) return -1;

    int rc = octopus_explore((octopus_system_t*)brain->octopus,
                             audio_vec, n);
    if (rc == 0 && brain->octopus_bridge_state) {
        octopus_bridge_state_t* st =
            (octopus_bridge_state_t*)brain->octopus_bridge_state;
        st->audio_samples++;
    }
    return rc;
}

/*============================================================================
 * Phase 4d: somatosensory (S1/S2) → octopus sampling.
 *
 * Strongest biological fit of all the sampling phases: octopus arms already
 * have a `chemo_id` fusion channel modeled on cephalopod tactile-chemical
 * receptors, and somatosensory cortex IS the mammalian analog of that same
 * substrate. Pain/temperature/proprioception at the segment level maps
 * directly to what a cephalopod arm perceives through its skin.
 *
 * We read the first 16 body segments (head..hands range) per channel block.
 * Most pain/position activity is concentrated here; the other 48 possible
 * segments (SOMA_MAX_BODY_SEGMENTS=64) are long-tail and would just add
 * zero-dominated channels.
 *==========================================================================*/

#define _OCTOPUS_SOMATO_CHANNELS 64u
#define _SOMATO_SEGMENTS_SAMPLED 16u

uint32_t nimcp_octopus_sample_somatosensory_vec(brain_t brain,
                                                 float* out_vec,
                                                 uint32_t out_dim) {
    if (!brain || !out_vec || out_dim == 0) return 0;
    if (!brain->somatosensory || !brain->somatosensory_enabled) return 0;

    if (out_dim > _OCTOPUS_SOMATO_CHANNELS) out_dim = _OCTOPUS_SOMATO_CHANNELS;

    float scratch[_OCTOPUS_SOMATO_CHANNELS] = {0};
    nimcp_somatosensory_t* soma =
        (nimcp_somatosensory_t*)brain->somatosensory;

    /* ---- Block A: per-segment pain (channels 0..15) ----------------------- */
    float max_pain = 0.0f;
    float total_pain = 0.0f;
    uint32_t active_segments = 0;
    for (uint32_t s = 0; s < _SOMATO_SEGMENTS_SAMPLED; s++) {
        float p = soma_get_pain_level(soma, (body_segment_t)s);
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        scratch[s] = p;
        total_pain += p;
        if (p > max_pain) max_pain = p;
        if (p > 0.01f) active_segments++;
    }
    /* Normalize total pain by number of segments sampled so scratch[48]
     * stays in [0,1] like all the other signals. */
    total_pain /= (float)_SOMATO_SEGMENTS_SAMPLED;

    /* ---- Block B: per-segment temperature (channels 16..31) ---------------
     * temp_sensation_t is an enum [0..6] (COLD_EXTREME..HOT_EXTREME).
     * Center NEUTRAL (3) on 0.5, map to [0,1]. */
    for (uint32_t s = 0; s < _SOMATO_SEGMENTS_SAMPLED; s++) {
        temp_sensation_t t = soma_get_temperature_sensation(
            soma, (body_segment_t)s);
        int ti = (int)t;
        if (ti < 0) ti = 0;
        if (ti > 6) ti = 6;
        scratch[16 + s] = (float)ti / 6.0f;
    }

    /* ---- Block C: per-segment position magnitude (channels 32..47) --------
     * soma_get_body_position fills positions[seg*3 .. seg*3+2] = (x,y,z).
     * We map each segment to its L2 norm — a single proprioceptive scalar
     * that correlates with body pose without needing 3-dim channels. */
    float positions[_SOMATO_SEGMENTS_SAMPLED * 3] = {0};
    uint32_t n_segs = 0;
    if (soma_get_body_position(soma, positions,
                               _SOMATO_SEGMENTS_SAMPLED, &n_segs) == 0) {
        if (n_segs > _SOMATO_SEGMENTS_SAMPLED) n_segs = _SOMATO_SEGMENTS_SAMPLED;
        for (uint32_t s = 0; s < n_segs; s++) {
            float x = positions[s * 3 + 0];
            float y = positions[s * 3 + 1];
            float z = positions[s * 3 + 2];
            float mag = sqrtf(x * x + y * y + z * z);
            /* tanh-squash so large positions don't dominate arms. */
            scratch[32 + s] = tanhf(mag);
        }
    }

    /* ---- Block D: body-wide pain summary (channels 48..49) ----------------
     * Computed locally from per-segment pain in Block A (soma_get_pain_state
     * is declared but not implemented — avoid it). */
    scratch[48] = total_pain;
    scratch[49] = max_pain;

    /* ---- Block E: activity density (channel 50) --------------------------- */
    /* log1p(1..16) / log(17) ≈ [0,1]. */
    scratch[50] = log1pf((float)active_segments) / 2.833f;
    if (scratch[50] > 1.0f) scratch[50] = 1.0f;

    memcpy(out_vec, scratch, sizeof(float) * out_dim);
    return out_dim;
}

int nimcp_octopus_explore_from_somatosensory(brain_t brain) {
    if (!brain || !brain->octopus || !brain->octopus_enabled) return -1;
    if (!brain->somatosensory || !brain->somatosensory_enabled) return -1;

    float soma_vec[_OCTOPUS_SOMATO_CHANNELS] = {0};
    uint32_t n = nimcp_octopus_sample_somatosensory_vec(
        brain, soma_vec, _OCTOPUS_SOMATO_CHANNELS);
    if (n == 0) return -1;

    int rc = octopus_explore((octopus_system_t*)brain->octopus,
                             soma_vec, n);
    if (rc == 0 && brain->octopus_bridge_state) {
        octopus_bridge_state_t* st =
            (octopus_bridge_state_t*)brain->octopus_bridge_state;
        st->somato_samples++;
    }
    return rc;
}

/*============================================================================
 * Phase 4e: SNN → octopus sampling.
 *
 * Octopus arms consume subsymbolic spike-rate dynamics from the 1.8M-neuron
 * SNN as population-level features. We iterate populations 0..31 via the
 * public getter and stop at the first NULL — decoupled from snn_network_t's
 * internal `n_populations` field so a layout change doesn't break us.
 *
 * Firing-rate window chosen as 50 ms (typical behavioral timescale, matches
 * synaptic tau). 50 Hz is the normalization ceiling for the per-pop channel —
 * anything above is clipped; healthy brain is << 50 Hz so clipping is rare.
 *==========================================================================*/

#define _OCTOPUS_SNN_CHANNELS 64u
#define _OCTOPUS_SNN_MAX_POPS 32u

uint32_t nimcp_octopus_sample_snn_vec(brain_t brain,
                                       float* out_vec,
                                       uint32_t out_dim) {
    if (!brain || !out_vec || out_dim == 0) return 0;
    if (!brain->snn_network) return 0;

    if (out_dim > _OCTOPUS_SNN_CHANNELS) out_dim = _OCTOPUS_SNN_CHANNELS;

    float scratch[_OCTOPUS_SNN_CHANNELS] = {0};
    snn_network_t* net = (snn_network_t*)brain->snn_network;

    /* ---- Block A: per-population firing rates (channels 0..31) ------------
     * Stop when we hit a NULL population — lets us work with any number of
     * populations up to _OCTOPUS_SNN_MAX_POPS without reading private fields. */
    const float rate_window_ms = 50.0f;
    const float rate_norm      = 50.0f;  /* clipping ceiling in Hz */
    for (uint32_t p = 0; p < _OCTOPUS_SNN_MAX_POPS; p++) {
        if (!snn_network_get_population(net, p)) break;
        float hz = snn_network_get_population_rate(net, p, rate_window_ms);
        if (hz < 0.0f) hz = 0.0f;
        float v = hz / rate_norm;
        if (v > 1.0f) v = 1.0f;
        scratch[p] = v;
    }

    /* ---- Block B: network-level stats (channels 48..55) ------------------- */
    snn_stats_t stats = {0};
    if (snn_network_get_stats(net, &stats) == 0) {
        float mf = stats.mean_firing_rate / 50.0f;
        if (mf < 0.0f) mf = 0.0f;
        if (mf > 1.0f) mf = 1.0f;
        scratch[48] = mf;

        float xf = stats.max_firing_rate / 200.0f;
        if (xf < 0.0f) xf = 0.0f;
        if (xf > 1.0f) xf = 1.0f;
        scratch[49] = xf;

        scratch[50] = stats.sparsity  < 0.0f ? 0.0f :
                      stats.sparsity  > 1.0f ? 1.0f : stats.sparsity;
        scratch[51] = stats.synchrony < 0.0f ? 0.0f :
                      stats.synchrony > 1.0f ? 1.0f : stats.synchrony;

        int hi = (int)stats.health;
        if (hi < 0) hi = 0;
        if (hi > 6) hi = 6;
        scratch[52] = (float)hi / 6.0f;

        /* log1p norms — 1e6 for neurons, 1e9 for spikes. */
        float sn = log1pf((float)stats.silent_neurons) / 13.8155f;
        if (sn > 1.0f) sn = 1.0f;
        scratch[53] = sn;
        float hn = log1pf((float)stats.hyperactive_neurons) / 13.8155f;
        if (hn > 1.0f) hn = 1.0f;
        scratch[54] = hn;
        float ts = log1pf((float)stats.total_spikes) / 20.7233f;
        if (ts > 1.0f) ts = 1.0f;
        scratch[55] = ts;
    }

    memcpy(out_vec, scratch, sizeof(float) * out_dim);
    return out_dim;
}

int nimcp_octopus_explore_from_snn(brain_t brain) {
    if (!brain || !brain->octopus || !brain->octopus_enabled) return -1;
    if (!brain->snn_network) return -1;

    float snn_vec[_OCTOPUS_SNN_CHANNELS] = {0};
    uint32_t n = nimcp_octopus_sample_snn_vec(
        brain, snn_vec, _OCTOPUS_SNN_CHANNELS);
    if (n == 0) return -1;

    int rc = octopus_explore((octopus_system_t*)brain->octopus,
                             snn_vec, n);
    if (rc == 0 && brain->octopus_bridge_state) {
        octopus_bridge_state_t* st =
            (octopus_bridge_state_t*)brain->octopus_bridge_state;
        st->snn_samples++;
    }
    return rc;
}

/*============================================================================
 * Phase 4f: neuromodulator → octopus sampling.
 *
 * Biologically-motivated: DA/5-HT/ACh/NE/GABA/GLU concentrations bathe the
 * whole brain and should drive arm-level exploration dynamics too. Octopus
 * already has pink-noise for 1/f exploration; neuromod sampling lets arms
 * also condition their decisions on the brain's current modulation regime.
 *
 * neuromodulator_system_t is already a pointer typedef — do not double-
 * indirect (per project convention).
 *==========================================================================*/

#define _OCTOPUS_NEUROMOD_CHANNELS 64u

uint32_t nimcp_octopus_sample_neuromod_vec(brain_t brain,
                                            float* out_vec,
                                            uint32_t out_dim) {
    if (!brain || !out_vec || out_dim == 0) return 0;
    if (!brain->neuromodulator_system) return 0;

    if (out_dim > _OCTOPUS_NEUROMOD_CHANNELS) out_dim = _OCTOPUS_NEUROMOD_CHANNELS;

    float scratch[_OCTOPUS_NEUROMOD_CHANNELS] = {0};

    neuromodulator_system_t sys = brain->neuromodulator_system;

    /* Slot each neuromodulator at a fixed channel index so arms can learn
     * stable semantics across runs. Levels are nominally [0,1]; clamp
     * defensively against out-of-range getters. */
    static const neuromodulator_type_t types[6] = {
        NEUROMOD_DOPAMINE,
        NEUROMOD_SEROTONIN,
        NEUROMOD_ACETYLCHOLINE,
        NEUROMOD_NOREPINEPHRINE,
        NEUROMOD_GABA,
        NEUROMOD_GLUTAMATE,
    };
    for (uint32_t i = 0; i < 6; i++) {
        float v = neuromodulator_get_level(sys, types[i]);
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        scratch[i] = v;
    }

    memcpy(out_vec, scratch, sizeof(float) * out_dim);
    return out_dim;
}

int nimcp_octopus_explore_from_neuromod(brain_t brain) {
    if (!brain || !brain->octopus || !brain->octopus_enabled) return -1;
    if (!brain->neuromodulator_system) return -1;

    float nmod_vec[_OCTOPUS_NEUROMOD_CHANNELS] = {0};
    uint32_t n = nimcp_octopus_sample_neuromod_vec(
        brain, nmod_vec, _OCTOPUS_NEUROMOD_CHANNELS);
    if (n == 0) return -1;

    int rc = octopus_explore((octopus_system_t*)brain->octopus,
                             nmod_vec, n);
    if (rc == 0 && brain->octopus_bridge_state) {
        octopus_bridge_state_t* st =
            (octopus_bridge_state_t*)brain->octopus_bridge_state;
        st->neuromod_samples++;
    }
    return rc;
}

/*============================================================================
 * Phase 4g: peer-module (Portia + Dragonfly) → octopus sampling.
 *
 * The octopus module was originally pitched alongside Portia (adaptation)
 * and Dragonfly (attention) as a delegation/adaptation/attention triad.
 * Until this phase they didn't share state. Now arms can read Dragonfly's
 * target-tracking vector (TSDN) and Portia's resource/tier state.
 *
 * Portia is a singleton (no instance pointer — global calls). Dragonfly is
 * instanced via brain->dragonfly. Both blocks are independently populated
 * so a brain with only one peer still produces a useful vector.
 *==========================================================================*/

#define _OCTOPUS_PEER_CHANNELS 64u

uint32_t nimcp_octopus_sample_peers_vec(brain_t brain,
                                         float* out_vec,
                                         uint32_t out_dim) {
    if (!brain || !out_vec || out_dim == 0) return 0;
    if (out_dim > _OCTOPUS_PEER_CHANNELS) out_dim = _OCTOPUS_PEER_CHANNELS;

    float scratch[_OCTOPUS_PEER_CHANNELS] = {0};

    bool any_peer = false;

    /* ---- Dragonfly block (channels 0..15) --------------------------------- */
    if (brain->dragonfly && brain->dragonfly_enabled) {
        dragonfly_system_t* df = (dragonfly_system_t*)brain->dragonfly;

        dragonfly_mode_t mode = dragonfly_get_mode(df);
        int mi = (int)mode;
        if (mi < 0) mi = 0;
        if (mi > 4) mi = 4;
        scratch[0] = (float)mi / 4.0f;  /* INTERCEPTING=4 */

        dragonfly_state_t dstate = {0};
        if (dragonfly_get_state(df, &dstate) == 0) {
            scratch[1] = dstate.is_tracking ? 1.0f : 0.0f;
            scratch[2] = dstate.confidence < 0.0f ? 0.0f :
                         dstate.confidence > 1.0f ? 1.0f : dstate.confidence;
            scratch[3] = dstate.evasion_detected ? 1.0f : 0.0f;
            scratch[9] = tanhf(dstate.time_to_intercept_ms / 1000.0f);
        }

        tsdn_vector_t tv = {0};
        if (dragonfly_get_tsdn_vector(df, &tv) == 0 && tv.valid) {
            /* direction is radians — encode as (sin, cos) to avoid wrap
             * discontinuity at ±π. */
            scratch[4] = tv.magnitude < 0.0f ? 0.0f :
                         tv.magnitude > 1.0f ? 1.0f : tv.magnitude;
            scratch[5] = sinf(tv.direction);
            scratch[6] = cosf(tv.direction);
            scratch[7] = sinf(tv.elevation);
            scratch[8] = tanhf(tv.angular_velocity);
        }
        any_peer = true;
    }

    /* ---- Portia block (channels 16..23) ----------------------------------- */
    if (portia_is_initialized()) {
        portia_status_t pst = {0};
        if (portia_get_status(&pst) == NIMCP_SUCCESS) {
            /* Tier 0..6, power 0..5, thermal 0..4, degradation 0..4. */
            int ti = (int)pst.current_tier;
            if (ti < 0) ti = 0;
            if (ti > 6) ti = 6;
            scratch[16] = (float)ti / 6.0f;

            int pw = (int)pst.power_state;
            if (pw < 0) pw = 0;
            if (pw > 5) pw = 5;
            scratch[17] = (float)pw / 5.0f;

            int th = (int)pst.thermal_state;
            if (th < 0) th = 0;
            if (th > 4) th = 4;
            scratch[18] = (float)th / 4.0f;

            int dg = (int)pst.degradation_level;
            if (dg < 0) dg = 0;
            if (dg > 4) dg = 4;
            scratch[19] = (float)dg / 4.0f;

            scratch[20] = pst.cpu_usage    < 0.0f ? 0.0f :
                          pst.cpu_usage    > 1.0f ? 1.0f : pst.cpu_usage;
            scratch[21] = pst.memory_usage < 0.0f ? 0.0f :
                          pst.memory_usage > 1.0f ? 1.0f : pst.memory_usage;
            /* battery_level = -1 means N/A — treat as 0 (unknown/none). */
            scratch[22] = pst.battery_level < 0.0f ? 0.0f :
                          pst.battery_level > 1.0f ? 1.0f : pst.battery_level;
            /* Temperature: clamp 0..100 C then /100 → [0,1]. */
            float tc = pst.temperature_celsius;
            if (tc < 0.0f) tc = 0.0f;
            if (tc > 100.0f) tc = 100.0f;
            scratch[23] = tc / 100.0f;
        }
        any_peer = true;
    }

    if (!any_peer) return 0;

    memcpy(out_vec, scratch, sizeof(float) * out_dim);
    return out_dim;
}

int nimcp_octopus_explore_from_peers(brain_t brain) {
    if (!brain || !brain->octopus || !brain->octopus_enabled) return -1;

    float peer_vec[_OCTOPUS_PEER_CHANNELS] = {0};
    uint32_t n = nimcp_octopus_sample_peers_vec(
        brain, peer_vec, _OCTOPUS_PEER_CHANNELS);
    if (n == 0) return -1;

    int rc = octopus_explore((octopus_system_t*)brain->octopus,
                             peer_vec, n);
    if (rc == 0 && brain->octopus_bridge_state) {
        octopus_bridge_state_t* st =
            (octopus_bridge_state_t*)brain->octopus_bridge_state;
        st->peer_samples++;
    }
    return rc;
}

/*============================================================================
 * Phase 5A: nimcp_octopus_tick — the runtime integration.
 *
 * Every brain_decide and brain_learn_vector call drives one tick, so the
 * octopus arms see every inference input and every training vector. This
 * is what makes the module live instead of a statue.
 *
 * Cadence choices (powers of two so counter masks stay constant-time):
 *   - Train step every 8 ticks (when training enabled)
 *   - NG regularization every 32 ticks (when NG enabled)
 *
 * At 8 exposures/sec during training that's ~1 train step/sec, ~4 NG/min.
 * Arm forward runs every tick — real per-tick cost is small (8 NCP LNNs
 * with sparse wiring at dim=64).
 *==========================================================================*/

#define _OCTOPUS_TICK_TRAIN_MASK 0x7u   /* every 8th tick */
#define _OCTOPUS_TICK_NG_MASK    0x1Fu  /* every 32nd tick */

void nimcp_octopus_tick(brain_t brain,
                        const float* features,
                        uint32_t num_features) {
    if (!brain || !features || num_features == 0) return;
    if (!brain->octopus || !brain->octopus_enabled) return;

    octopus_system_t* ctx = (octopus_system_t*)brain->octopus;

    NIMCP_LOGGING_TRACE("nimcp_octopus_tick: features_len=%u", num_features);

    /* Explore: arm forwards + LNN state advance + ethics/swarm hooks. */
    if (octopus_explore(ctx, features, num_features) != 0) {
        NIMCP_LOGGING_DEBUG("nimcp_octopus_tick: octopus_explore failed");
        return;
    }

    /* Integrate: aggregate latent, fire world/FEP/bio hooks. Ignore the
     * aggregated output — consumers should pull it via octopus_get_stats
     * or subscribe to the bio_router broadcasts the hooks emit. */
    float aggregated[OCTOPUS_ARM_DIM];
    float coherence = 0.0f;
    (void)octopus_integrate(ctx, aggregated, &coherence);

    /* Cadence-gated training + NG. Guarded on the feature flags so a
     * brain that never enables training still pays zero cost here. */
    if (!brain->octopus_bridge_state) return;
    octopus_bridge_state_t* st =
        (octopus_bridge_state_t*)brain->octopus_bridge_state;
    st->tick_counter++;

    octopus_stats_t s;
    octopus_get_stats(ctx, &s);

    if (s.lnn_training_enabled &&
        (st->tick_counter & _OCTOPUS_TICK_TRAIN_MASK) == 0) {
        NIMCP_LOGGING_DEBUG("nimcp_octopus_tick: firing train_step "
                            "(tick=%lu)", (unsigned long)st->tick_counter);
        (void)octopus_train_step(ctx, NULL);
    }
    if (s.ng_enabled &&
        (st->tick_counter & _OCTOPUS_TICK_NG_MASK) == 0) {
        NIMCP_LOGGING_DEBUG("nimcp_octopus_tick: firing ng_regularize "
                            "(tick=%lu)", (unsigned long)st->tick_counter);
        (void)octopus_ng_regularize(ctx);
    }
}
