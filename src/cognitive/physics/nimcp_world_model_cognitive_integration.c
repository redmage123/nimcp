/**
 * @file nimcp_world_model_cognitive_integration.c
 * @brief Wires world model to cerebellum, BG, medulla, and all cognitive modules
 *
 * WHAT: Dispatches world model events to 15 brain systems
 * WHY:  World model predictions must inform action, error correction, arousal,
 *       curiosity, planning, morality, and social cognition
 * HOW:  Callback-based fan-out from bridge surprise/error events
 */

#include "cognitive/physics/nimcp_world_model_cognitive_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "WMCI"

/* ============================================================================
 * Extern declarations for brain system APIs (avoid header dependency chain)
 * ============================================================================ */

/* Cerebellum */
extern bool cerebellum_broadcast_error(void* adapter, float error_mag, uint8_t error_type);
extern bool cerebellum_update_forward_model(void* adapter, const float* cmd, const float* outcome, uint32_t dims);

/* Basal Ganglia */
extern int nimcp_brain_bg_update_reward(void* brain, float reward, float confidence) __attribute__((weak));

/* Medulla */
extern int medulla_boost_arousal(void* medulla, float delta) __attribute__((weak));
extern float medulla_get_arousal_level(const void* medulla) __attribute__((weak));

/* Curiosity */
extern int curiosity_report_novelty(void* curiosity, float novelty_score, const char* domain) __attribute__((weak));

/* Working Memory */
extern int working_memory_update_context(void* wm, const float* state, uint32_t dim, const char* label) __attribute__((weak));

/* Attention */
extern int attention_update_salience(void* attention, float physics_sal, float chem_sal, float bio_sal) __attribute__((weak));

/* Ethics */
extern int ethics_evaluate_consequence(void* ethics, float harm_potential) __attribute__((weak));

/* ============================================================================
 * Internal Callbacks (registered with the world model bridge)
 * ============================================================================ */

static void wmci_on_surprise(const wmb_surprise_event_t* event, void* user_data) {
    wmci_context_t* ctx = (wmci_context_t*)user_data;
    if (!ctx || !event) return;
    wmci_dispatch_surprise(ctx, event);
}

static void wmci_on_replay(const float* before, const float* after,
                             float surprise, void* user_data) {
    wmci_context_t* ctx = (wmci_context_t*)user_data;
    if (!ctx) return;
    wmci_dispatch_replay(ctx, before, after, surprise);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

wmci_context_t* wmci_create(world_model_bridge_t* bridge) {
    wmci_context_t* ctx = nimcp_calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->bridge = bridge;
    ctx->initialized = true;

    /* Register ourselves as the bridge's callback handler */
    if (bridge) {
        wmb_set_surprise_callback(bridge, wmci_on_surprise, ctx);
        wmb_set_replay_callback(bridge, wmci_on_replay, ctx);
    }

    LOG_INFO(LOG_TAG, "World model cognitive integration created");
    return ctx;
}

void wmci_destroy(wmci_context_t* ctx) {
    if (!ctx) return;
    /* Deregister callbacks */
    if (ctx->bridge) {
        wmb_set_surprise_callback(ctx->bridge, NULL, NULL);
        wmb_set_replay_callback(ctx->bridge, NULL, NULL);
    }
    nimcp_free(ctx);
}

void wmci_connect(wmci_context_t* ctx, wmci_target_flags_t target, void* system) {
    if (!ctx || !system) return;

    switch (target) {
    case WMCI_TARGET_CEREBELLUM:      ctx->cerebellum = system; break;
    case WMCI_TARGET_BASAL_GANGLIA:   ctx->basal_ganglia = system; break;
    case WMCI_TARGET_MEDULLA:         ctx->medulla = system; break;
    case WMCI_TARGET_FEP:             ctx->fep = system; break;
    case WMCI_TARGET_JEPA:            ctx->jepa = system; break;
    case WMCI_TARGET_CURIOSITY:       ctx->curiosity = system; break;
    case WMCI_TARGET_IMAGINATION:     ctx->imagination = system; break;
    case WMCI_TARGET_THEORY_OF_MIND:  ctx->theory_of_mind = system; break;
    case WMCI_TARGET_WORKING_MEMORY:  ctx->working_memory = system; break;
    case WMCI_TARGET_ATTENTION:       ctx->attention = system; break;
    case WMCI_TARGET_ETHICS:          ctx->ethics = system; break;
    case WMCI_TARGET_EXECUTIVE:       ctx->executive = system; break;
    case WMCI_TARGET_CAUSAL:          ctx->causal = system; break;
    case WMCI_TARGET_COUNTERFACTUAL:  ctx->counterfactual = system; break;
    default: return;
    }
    ctx->active_targets |= (uint32_t)target;
}

uint32_t wmci_active_count(const wmci_context_t* ctx) {
    if (!ctx) return 0;
    uint32_t count = 0;
    for (int i = 0; i < 15; i++)
        if (ctx->active_targets & (1u << i)) count++;
    return count;
}

/* ============================================================================
 * Event Dispatch
 * ============================================================================ */

void wmci_dispatch_surprise(wmci_context_t* ctx,
                              const wmb_surprise_event_t* event) {
    if (!ctx || !event) return;
    ctx->total_events_dispatched++;

    float error = event->prediction_error;
    float surprise = event->surprise_score;

    /* --- Cerebellum: fast error signal for motor correction --- */
    if (ctx->cerebellum && cerebellum_broadcast_error) {
        /* Error type: 0=physics, 1=chemistry, 2=biology */
        uint8_t err_type = (uint8_t)event->domain;
        float err_mag = error;
        if (err_mag > 1.0f) err_mag = 1.0f;
        if (err_mag < -1.0f) err_mag = -1.0f;
        cerebellum_broadcast_error(ctx->cerebellum, err_mag, err_type);

        /* Also train forward model with (predicted, actual) pair */
        float pred[3] = { event->predicted.physics.position[0],
                           event->predicted.physics.position[1],
                           event->predicted.physics.position[2] };
        float actual[3] = { event->actual.physics.position[0],
                              event->actual.physics.position[1],
                              event->actual.physics.position[2] };
        cerebellum_update_forward_model(ctx->cerebellum, pred, actual, 3);
    }

    /* --- Basal Ganglia: negative reward for prediction errors --- */
    /* Actions that lead to high prediction error are "bad" (unpredictable).
     * Actions that lead to low error are "good" (world behaves as expected).
     * This trains the BG to select actions that keep the world predictable. */
    if (ctx->basal_ganglia && nimcp_brain_bg_update_reward) {
        float reward = -error * 0.1f;  /* negative: errors are punishing */
        float confidence = 1.0f / (1.0f + surprise);  /* high surprise = low confidence */
        nimcp_brain_bg_update_reward(ctx->basal_ganglia, reward, confidence);
    }

    /* --- Medulla: arousal boost proportional to surprise --- */
    /* Surprising events should make the brain more alert.
     * This is the orienting response — "something unexpected happened!" */
    if (ctx->medulla && medulla_boost_arousal) {
        float arousal_delta = surprise * 0.05f;  /* scale surprise to arousal */
        if (arousal_delta > 0.3f) arousal_delta = 0.3f;  /* cap */
        medulla_boost_arousal(ctx->medulla, arousal_delta);
    }

    /* --- Curiosity: flag surprising domains for exploration --- */
    if (ctx->curiosity && curiosity_report_novelty) {
        const char* domain_names[] = {"physics", "chemistry", "biology", "cross-domain"};
        const char* domain = (event->domain < 4) ? domain_names[event->domain] : "unknown";
        curiosity_report_novelty(ctx->curiosity, surprise, domain);
    }

    /* --- Working Memory: update world state snapshot --- */
    if (ctx->working_memory && working_memory_update_context) {
        /* Pack the actual state into a context vector for WM */
        float wm_state[6] = {
            event->actual.physics.position[0],
            event->actual.physics.position[1],
            event->actual.physics.position[2],
            event->actual.chemistry.pH,
            event->actual.biology.total_biomass,
            error
        };
        working_memory_update_context(ctx->working_memory, wm_state, 6, "world_model_surprise");
    }

    /* --- Attention: precision-driven salience update --- */
    /* Domains with high precision (low expected error) that suddenly
     * have high error should get HIGH attention (most informative).
     * Domains with low precision (high expected error) that have
     * typical error should get LOW attention (nothing new). */
    if (ctx->attention && attention_update_salience && ctx->bridge) {
        float phys_sal = ctx->bridge->physics_precision * event->physics_error;
        float chem_sal = ctx->bridge->chemistry_precision * event->chemistry_error;
        float bio_sal = ctx->bridge->biology_precision * event->biology_error;
        attention_update_salience(ctx->attention, phys_sal, chem_sal, bio_sal);
    }

    /* --- Ethics: consequence evaluation for surprising outcomes --- */
    /* If a physical event could cause harm (high physics error + high energy),
     * flag it for ethical evaluation */
    if (ctx->ethics && ethics_evaluate_consequence) {
        float harm = event->physics_error * 0.1f;  /* rough proxy */
        if (harm > 0.1f)
            ethics_evaluate_consequence(ctx->ethics, harm);
    }
}

void wmci_dispatch_prediction_error(wmci_context_t* ctx,
                                      float physics_error,
                                      float chemistry_error,
                                      float biology_error) {
    if (!ctx) return;

    /* Cerebellum gets EVERY error, not just surprises — it learns fast */
    if (ctx->cerebellum && cerebellum_broadcast_error) {
        float max_err = physics_error;
        uint8_t type = 0;
        if (chemistry_error > max_err) { max_err = chemistry_error; type = 1; }
        if (biology_error > max_err) { max_err = biology_error; type = 2; }
        if (max_err > 1.0f) max_err = 1.0f;
        cerebellum_broadcast_error(ctx->cerebellum, max_err, type);
    }
}

void wmci_dispatch_replay(wmci_context_t* ctx,
                            const float* latent_before,
                            const float* latent_after,
                            float surprise_score) {
    if (!ctx) return;
    (void)latent_before;
    (void)latent_after;

    /* During replay, reduce medulla arousal (consolidation is calm) */
    if (ctx->medulla && medulla_boost_arousal) {
        medulla_boost_arousal(ctx->medulla, -0.01f);  /* slight calming */
    }

    /* Report replay to curiosity (learning progress signal) */
    if (ctx->curiosity && curiosity_report_novelty) {
        curiosity_report_novelty(ctx->curiosity, surprise_score * 0.3f, "replay");
    }
}

/* ============================================================================
 * Auto-wire from brain
 * ============================================================================ */

int wmci_auto_wire_from_brain(wmci_context_t* ctx, struct brain_struct* brain) {
    if (!ctx || !brain) return -1;

    /* Use accessor functions to avoid including brain_internal.h */
    /* These are defined in nimcp_brain_init_world_model.c */
    extern void* nimcp_brain_get_cerebellum(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_basal_ganglia(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_medulla(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_fep(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_jepa(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_curiosity(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_imagination(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_theory_of_mind(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_working_memory(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_attention(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_ethics(struct brain_struct* b) __attribute__((weak));
    extern void* nimcp_brain_get_executive(struct brain_struct* b) __attribute__((weak));

    /* Wire each system if the accessor exists and returns non-NULL */
    #define TRY_WIRE(target, getter) do { \
        if (getter) { \
            void* sys = getter(brain); \
            if (sys) wmci_connect(ctx, target, sys); \
        } \
    } while(0)

    TRY_WIRE(WMCI_TARGET_CEREBELLUM,      nimcp_brain_get_cerebellum);
    TRY_WIRE(WMCI_TARGET_BASAL_GANGLIA,   nimcp_brain_get_basal_ganglia);
    TRY_WIRE(WMCI_TARGET_MEDULLA,         nimcp_brain_get_medulla);
    TRY_WIRE(WMCI_TARGET_FEP,             nimcp_brain_get_fep);
    TRY_WIRE(WMCI_TARGET_JEPA,            nimcp_brain_get_jepa);
    TRY_WIRE(WMCI_TARGET_CURIOSITY,       nimcp_brain_get_curiosity);
    TRY_WIRE(WMCI_TARGET_IMAGINATION,     nimcp_brain_get_imagination);
    TRY_WIRE(WMCI_TARGET_THEORY_OF_MIND,  nimcp_brain_get_theory_of_mind);
    TRY_WIRE(WMCI_TARGET_WORKING_MEMORY,  nimcp_brain_get_working_memory);
    TRY_WIRE(WMCI_TARGET_ATTENTION,       nimcp_brain_get_attention);
    TRY_WIRE(WMCI_TARGET_ETHICS,          nimcp_brain_get_ethics);
    TRY_WIRE(WMCI_TARGET_EXECUTIVE,       nimcp_brain_get_executive);

    #undef TRY_WIRE

    uint32_t wired = wmci_active_count(ctx);
    LOG_INFO(LOG_TAG, "Auto-wired %u brain systems to world model", wired);
    return (int)wired;
}
