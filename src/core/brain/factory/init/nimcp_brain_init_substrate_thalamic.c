/**
 * @file nimcp_brain_init_substrate_thalamic.c
 * @brief Phase 1-4 Neural Substrate + Thalamic Router wire-up for brain init
 *
 * WHAT: Creates the CPU neural_substrate_t + thalamic_router_t instances and
 *       exposes a re-attach helper that wires them into every SNN/LNN/CNN
 *       network currently hanging off the brain.
 *
 * WHY:  Prior to this file, substrate_create() / thalamic_router_create() were
 *       never called from brain construction, which meant *_attach_substrate()
 *       and *_attach_thalamic_router() were never reached by real brain code.
 *       The Phase 1-4 biological adapters (ATP / temperature / ion dynamics
 *       in SNN/LNN/CNN, attention-gated routing via thalamic channels) existed
 *       but were dormant. 95/95 callers were test-only; 0/0 in src/.
 *
 * HOW:  Two entry points:
 *
 *       1. nimcp_brain_factory_init_substrate_thalamic_subsystem(brain)
 *          Runs once during brain creation — creates substrate and router and
 *          stores them on brain->substrate / brain->thalamic_router. Safe to
 *          call again; returns true without re-creating.
 *
 *       2. nimcp_brain_attach_substrate_thalamic(brain)
 *          Called AFTER any network creation (SNN / LNN / cortex_cnns[]) and
 *          AFTER checkpoint load to wire the substrate + router into every
 *          network that currently exists. attach_* functions are NULL-tolerant
 *          so it's safe to call even if a network was destroyed or never
 *          created. Idempotent: existing attachments are replaced.
 *
 *       Both functions are no-ops if the substrate or router is NULL, which
 *       keeps the Phase 1-4 adapters in safe fallback mode rather than
 *       crashing the pipeline.
 *
 * BIOLOGICAL DEFAULTS (substrate_create):
 *   atp=0.95, oxygen=0.97, glucose=0.90, temp=37.0 C, membrane=0.98,
 *   ion_balance=0.95 — healthy resting neural tissue.
 *
 * ROUTER DEFAULTS (thalamic_router_default_config):
 *   queue size ~1024, attention gating on, priority routing on, learning off
 *   by default (caller can enable via tunable).
 *
 * @author NIMCP Development Team
 * @date 2026-04-22
 * @version 1.0
 */

#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
/* NOTE: we do NOT include middleware/routing/nimcp_thalamic_router.h here
 * because parietal.h (pulled in transitively via brain_internal.h) declares
 * a conflicting typedef for thalamic_router_t (pointer vs. value). Forward-
 * declare the ops we need instead — same workaround pattern used by
 * src/core/brain/nimcp_brain.c around line 215. */
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>  /* memset for fallback config */
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * Forward declarations of thalamic router API (opaque here).
 * The full header's typedef clashes with parietal.h (pointer vs. value), but
 * the underlying functions take a concrete `struct thalamic_router*`. We
 * mirror their declarations against that concrete struct so the linker
 * resolves to the real symbols from nimcp_thalamic_router.c.
 *---------------------------------------------------------------------------*/
struct thalamic_router;

/* Local mirror of thalamic_router_config_t layout. Must match the
 * definition in include/middleware/routing/nimcp_thalamic_router.h — we
 * only pass a pointer to this struct across the create() boundary so
 * the actual implementation decodes it with the canonical type. */
typedef struct nimcp_thal_rt_cfg_local {
    uint32_t max_queue_size;
    uint32_t max_destinations;
    bool     enable_attention_gating;
    bool     enable_priority_routing;
    bool     enable_statistics;
    float    min_attention_threshold;
    bool     enable_learning;
    bool     enable_second_messengers;
    uint32_t num_neurons;
    bool     enable_quantum_routing;
} nimcp_thal_rt_cfg_local_t;

/* Real function symbols in nimcp_thalamic_router.c take pointers to the
 * concrete `struct thalamic_router`. The typedef clash is only about
 * `thalamic_router_t`, not the underlying struct name. */
extern struct thalamic_router* thalamic_router_create(
    const nimcp_thal_rt_cfg_local_t* config);
extern void thalamic_router_destroy(struct thalamic_router* router);
extern nimcp_thal_rt_cfg_local_t thalamic_router_default_config(void);

#define LOG_MODULE "BRAIN_INIT_SUBSTRATE_THALAMIC"

/*---------------------------------------------------------------------------
 * Forward declarations of attach functions (avoid pulling in full headers).
 * Signatures match the public API of each network module.
 *---------------------------------------------------------------------------*/

/* SNN attach — NULL-tolerant */
extern void snn_network_attach_substrate(struct snn_network_s* net,
                                         struct neural_substrate* sub);

/* LNN attach — NULL-tolerant for substrate; returns nimcp_error_t for router */
extern void         lnn_network_attach_substrate(struct lnn_network_s* net,
                                                 struct neural_substrate* sub);
extern nimcp_error_t lnn_network_attach_thalamic_router(struct lnn_network_s* net,
                                                        struct thalamic_router* router);

/* Cortex CNN attach — NULL-tolerant for substrate; nimcp_error_t for router */
extern void         cortex_cnn_attach_substrate(struct cortex_cnn_processor* proc,
                                                struct neural_substrate* substrate);
extern nimcp_error_t cortex_cnn_attach_thalamic_router(struct cortex_cnn_processor* proc,
                                                       struct thalamic_router* router);

/*---------------------------------------------------------------------------
 * Lifecycle: create substrate + router on brain init.
 *---------------------------------------------------------------------------*/

/**
 * @brief Initialize substrate + thalamic router subsystem.
 *
 * Creates a neural_substrate_t with biological defaults and a thalamic_router_t
 * with default routing config. Both are borrowed-pointer owners: the brain
 * owns them and destroys them in brain_destroy; networks receive borrowed
 * pointers via subsequent attach calls.
 *
 * Idempotent — if either subsystem already exists, it is left intact.
 * Non-fatal: on failure, the pointer stays NULL and Phase 1-4 adapters run
 * in their biological-default fallback mode.
 *
 * @param brain  Brain instance (must be non-NULL).
 * @return true on success (including partial / best-effort), false only on
 *         NULL brain.
 */
bool nimcp_brain_factory_init_substrate_thalamic_subsystem(brain_t brain)
{
    if (!brain) {
        LOG_ERROR("NULL brain provided to substrate+thalamic init");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_substrate_thalamic_subsystem: brain is NULL");
        return false;
    }

    /* 1) Neural substrate --------------------------------------------------*/
    if (!brain->substrate) {
        substrate_config_t sub_cfg;
        if (substrate_default_config(&sub_cfg) != 0) {
            LOG_WARN("substrate_default_config failed — using zeroed config");
            memset(&sub_cfg, 0, sizeof(sub_cfg));
            sub_cfg.initial_atp          = SUBSTRATE_NORMAL_ATP;
            sub_cfg.initial_o2           = SUBSTRATE_NORMAL_O2_SAT;
            sub_cfg.initial_glucose      = SUBSTRATE_NORMAL_GLUCOSE;
            sub_cfg.initial_temperature  = SUBSTRATE_NORMAL_TEMPERATURE;
            sub_cfg.initial_membrane     = SUBSTRATE_NORMAL_MEMBRANE;
            sub_cfg.initial_ion_balance  = SUBSTRATE_NORMAL_ION_BALANCE;
            sub_cfg.enable_metabolic_model    = true;
            sub_cfg.enable_temperature_effects = true;
            sub_cfg.enable_ion_dynamics       = true;
            sub_cfg.enable_alerts             = true;
        }

        brain->substrate = substrate_create(&sub_cfg);
        if (brain->substrate) {
            brain->substrate_enabled = true;
            LOG_INFO("Neural substrate created (atp=%.2f, temp=%.1fC, membrane=%.2f)",
                     sub_cfg.initial_atp, sub_cfg.initial_temperature,
                     sub_cfg.initial_membrane);
        } else {
            brain->substrate_enabled = false;
            LOG_WARN("substrate_create failed — Phase 1-4 adapters will run in "
                     "biological-default fallback mode");
        }
    }

    /* 2) Thalamic router ---------------------------------------------------*/
    if (!brain->thalamic_router) {
        nimcp_thal_rt_cfg_local_t rt_cfg = thalamic_router_default_config();

        brain->thalamic_router = thalamic_router_create(&rt_cfg);
        if (brain->thalamic_router) {
            brain->thalamic_router_enabled = true;
            LOG_INFO("Thalamic router created (queue=%u, max_dest=%u, gating=%d)",
                     rt_cfg.max_queue_size, rt_cfg.max_destinations,
                     (int)rt_cfg.enable_attention_gating);
        } else {
            brain->thalamic_router_enabled = false;
            LOG_WARN("thalamic_router_create failed — thalamic channels on "
                     "LNN/CNN will be inert");
        }
    }

    /* 3) Best-effort attach to any networks that may already exist.
     *    At this point in brain init the specialized networks usually don't
     *    exist yet (they're created on-demand in brain_enable_multi_network_
     *    training / cortex lazy-create). The attach helper is safe to call
     *    regardless. */
    nimcp_brain_attach_substrate_thalamic(brain);

    return true;
}

/**
 * @brief (Re-)attach substrate + thalamic router to every existing network.
 *
 * Idempotent. Safe to call multiple times and at any point after the
 * substrate/router have been created. Networks that are NULL are skipped.
 * Per-network attach functions are NULL-tolerant so passing a NULL substrate
 * or NULL router is also safe (it detaches).
 *
 * Call sites:
 *   - End of nimcp_brain_factory_init_substrate_thalamic_subsystem()
 *   - End of brain_enable_multi_network_training() (after SNN/LNN created)
 *   - After each lazy cortex_cnns[i] = cortex_cnn_create() in learning
 *   - End of brain_load() / brain_load_ex() after sidecars are loaded
 *
 * @param brain  Brain instance (NULL-tolerant).
 */
void nimcp_brain_attach_substrate_thalamic(brain_t brain)
{
    if (!brain) {
        return;
    }

    struct neural_substrate* sub    = brain->substrate;
    struct thalamic_router*  router = brain->thalamic_router;

    /* SNN — substrate only (SNN has its own routing bridge infrastructure).
     * Per design note: SNN thalamic integration is deferred; the SNN
     * routing bridge already handles cross-population spike gating. */
    if (brain->snn_network) {
        snn_network_attach_substrate(brain->snn_network, sub);
        LOG_DEBUG("Attached substrate to SNN network (sub=%p)", (void*)sub);
    }

    /* LNN — substrate + thalamic router */
    if (brain->lnn_network) {
        lnn_network_attach_substrate(brain->lnn_network, sub);
        nimcp_error_t rc = lnn_network_attach_thalamic_router(brain->lnn_network,
                                                              router);
        if (rc != NIMCP_SUCCESS && router != NULL) {
            LOG_WARN("lnn_network_attach_thalamic_router returned %d (non-fatal)",
                     (int)rc);
        }
        LOG_DEBUG("Attached substrate+router to LNN (sub=%p, router=%p)",
                  (void*)sub, (void*)router);
    }

    /* Per-cortex CNN processors — substrate + thalamic router.
     * Index 0=VISUAL, 1=AUDIO, 2=SPEECH, 3=SOMATO. */
    for (int ci = 0; ci < 4; ci++) {
        struct cortex_cnn_processor* proc = brain->cortex_cnns[ci];
        if (!proc) continue;

        cortex_cnn_attach_substrate(proc, sub);
        nimcp_error_t rc = cortex_cnn_attach_thalamic_router(proc, router);
        if (rc != NIMCP_SUCCESS && router != NULL) {
            LOG_WARN("cortex_cnn_attach_thalamic_router[%d] returned %d (non-fatal)",
                     ci, (int)rc);
        }
        LOG_DEBUG("Attached substrate+router to cortex CNN[%d] (sub=%p, router=%p)",
                  ci, (void*)sub, (void*)router);
    }
}

/**
 * @brief Destroy substrate + thalamic router subsystem.
 *
 * Before destroying, calls the attach helpers with NULL to detach from every
 * network that still holds a borrowed pointer. This order matters: networks
 * may be destroyed AFTER this function (e.g. during brain_destroy the SNN/LNN
 * cleanup happens later), so we explicitly null their handles first to avoid
 * use-after-free if they dereference during their own cleanup.
 *
 * @param brain  Brain instance (NULL-tolerant).
 */
void nimcp_brain_factory_destroy_substrate_thalamic_subsystem(brain_t brain)
{
    if (!brain) {
        return;
    }

    /* 1) Detach from all networks first (NULL-tolerant). */
    if (brain->snn_network) {
        snn_network_attach_substrate(brain->snn_network, NULL);
    }
    if (brain->lnn_network) {
        lnn_network_attach_substrate(brain->lnn_network, NULL);
        (void)lnn_network_attach_thalamic_router(brain->lnn_network, NULL);
    }
    for (int ci = 0; ci < 4; ci++) {
        if (brain->cortex_cnns[ci]) {
            cortex_cnn_attach_substrate(brain->cortex_cnns[ci], NULL);
            (void)cortex_cnn_attach_thalamic_router(brain->cortex_cnns[ci], NULL);
        }
    }

    /* 2) Destroy router first (it owns attention_gate, queues). */
    if (brain->thalamic_router) {
        thalamic_router_destroy(brain->thalamic_router);
        brain->thalamic_router = NULL;
        brain->thalamic_router_enabled = false;
        LOG_DEBUG("Thalamic router destroyed");
    }

    /* 3) Destroy substrate (owns its own mutex). */
    if (brain->substrate) {
        substrate_destroy(brain->substrate);
        brain->substrate = NULL;
        brain->substrate_enabled = false;
        LOG_DEBUG("Neural substrate destroyed");
    }
}

/*---------------------------------------------------------------------------
 * Accessor helpers (for regression tests and diagnostics).
 * Opaque pointer returns — callers should NOT dereference the structs here;
 * use them only to check identity / non-NULL or to pass back to library APIs.
 *---------------------------------------------------------------------------*/

struct neural_substrate* nimcp_brain_get_substrate(brain_t brain)
{
    return brain ? brain->substrate : NULL;
}

struct thalamic_router* nimcp_brain_get_thalamic_router(brain_t brain)
{
    return brain ? brain->thalamic_router : NULL;
}

bool nimcp_brain_substrate_is_enabled(brain_t brain)
{
    return brain && brain->substrate_enabled && brain->substrate != NULL;
}

bool nimcp_brain_thalamic_router_is_enabled(brain_t brain)
{
    return brain && brain->thalamic_router_enabled && brain->thalamic_router != NULL;
}

/* SNN: expose the substrate pointer the network holds (borrowed) so tests can
 * compare against brain->substrate. */
struct neural_substrate* nimcp_brain_snn_get_substrate_ref(brain_t brain)
{
    if (!brain || !brain->snn_network) return NULL;
    /* snn_network_t has a public `substrate` field — forward-declare the
     * struct here to avoid pulling the full SNN header (which may fight
     * with other typedefs). We use an opaque accessor in the SNN module. */
    extern struct neural_substrate* snn_network_get_substrate(struct snn_network_s* net);
    /* If the helper doesn't exist yet, fall back to direct field read. The
     * struct layout through a forward-decl is undefined, so we use the
     * SNN's own public getter if available, otherwise NULL. */
    return snn_network_get_substrate(brain->snn_network);
}

/* LNN: expose substrate pointer and thalamic channel pointer. */
struct neural_substrate* nimcp_brain_lnn_get_substrate_ref(brain_t brain)
{
    if (!brain || !brain->lnn_network) return NULL;
    extern struct neural_substrate* lnn_network_get_substrate(struct lnn_network_s* net);
    return lnn_network_get_substrate(brain->lnn_network);
}

void* nimcp_brain_lnn_get_thalamic_channel_ref(brain_t brain)
{
    if (!brain || !brain->lnn_network) return NULL;
    extern void* lnn_network_get_thalamic_channel(struct lnn_network_s* net);
    return lnn_network_get_thalamic_channel(brain->lnn_network);
}
