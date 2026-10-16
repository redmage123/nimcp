/**
 * @file nimcp_brain_init_cochlea.c
 * @brief Cochlea + 15 consumer bridges init (Wave 8A)
 *
 * Creates brain->cochlea and each of the 15 cochlea_*_bridge_t consumers,
 * then registers a single BRAIN_CYCLE_COCHLEA_BRIDGES driven cycle at 10ms
 * that invokes every live bridge's *_update() in order each tick.
 *
 * Each bridge init is wrapped in an isolated helper — a failure to create
 * one bridge (e.g. its dep on brain_struct is still NULL at this wave) does
 * not prevent the other bridges from being wired. Same null-tolerant
 * pattern as nimcp_brain_init_biology.c.
 *
 * TYPEDEF COLLISIONS (resolved with per-bridge trampoline #defines):
 *   - cochlea_medulla_bridge.h: typedef struct medulla medulla_t;
 *     conflicts with nimcp_medulla.h: typedef struct medulla_struct* medulla_t;
 *     We never include nimcp_medulla.h here — we only pass brain->medulla
 *     (already a handle; cast to the bridge's opaque medulla_t at the call
 *     site).
 *   - cochlea_thalamic_bridge.h declares a `thalamus_t` tag which has no
 *     matching field on brain_struct; brain has `thalamic_router_t*`
 *     instead. SKIPPED — dep missing.
 *   - cochlea_kg_bridge.h declares a `kg_engine_t` tag; brain has
 *     `struct brain_kg* internal_kg`. SKIPPED — dep missing.
 *   - cochlea_sleep_bridge.h declares a `sleep_controller_t` tag; brain has
 *     `sleep_system_t sleep_system` (different tag, different semantics).
 *     SKIPPED — dep missing.
 *
 * The remaining 12 bridges wire cleanly.
 */

#include "core/brain/factory/init/nimcp_brain_init_cochlea.h"

/* Include brain_internal.h FIRST so all the top-level typedefs win
 * (medulla_t, thalamic_router_t, immune_response_t, neuron_population_t,
 * etc). The cochlea + bridge headers below re-typedef `medulla_t` and other
 * tags locally — those are shadowed by the brain-internal ones. To avoid C
 * redefinition errors, we use the same `#define` trampoline idiom biology
 * uses: rename the colliding names to file-local aliases before each
 * include. The bridges only store the passed-in pointers opaquely, so the
 * ABI is unaffected by the rename. */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"

/* Now include cochlea + the 11 wireable bridge headers. Rename every
 * colliding typedef to a file-local alias via #define. We only ever touch
 * these types at the bridge-API boundary, where the bridge treats them as
 * opaque pointers. */

/* nimcp_cochlea.h has `typedef struct medulla medulla_t;` conflicting with
 * nimcp_medulla.h's `typedef struct medulla_struct* medulla_t;`. Rename. */
#define medulla_t cochlea_internal_medulla_t
#define thalamus_t cochlea_internal_thalamus_t
#include "perception/nimcp_cochlea.h"
#undef medulla_t
#undef thalamus_t

/* Wireable bridges (clean — no enum or tag conflicts with brain-internal).
 * Each header forward-declares its deps as local struct tags that don't
 * collide with any brain-internal typedef.
 *
 * audio_cortex_bridge.h transitively includes thalamic_bridge.h which
 * conflicts with parietal.h's thalamic_router_t tag. Forward-declare that
 * bridge's API below instead. */
#include "perception/bridges/nimcp_cochlea_bio_async_bridge.h"
#include "perception/bridges/nimcp_cochlea_broca_bridge.h"
#include "perception/bridges/nimcp_cochlea_collective_bridge.h"
#include "perception/bridges/nimcp_cochlea_cortical_deep_bridge.h"
#include "perception/bridges/nimcp_cochlea_fep_bridge.h"
#include "perception/bridges/nimcp_cochlea_occipital_bridge.h"
#include "perception/bridges/nimcp_cochlea_rcog_bridge.h"
#include "perception/bridges/nimcp_cochlea_verification_bridge.h"

/* The next three bridge headers redefine types / enums that conflict with
 * brain-internal definitions. Each collision type:
 *
 *   - immune_bridge.h: redefines INFLAMMATION_NONE etc. (enum) and
 *     immune_response_t (tag differs).
 *   - substrate_bridge.h: redefines neuron_population_t (tag differs
 *     from brain-internal's `struct neuron_population_s`).
 *   - medulla_bridge.h: redefines arousal_level_t, protection_level_t,
 *     circadian_phase_t (enums) and medulla_t (tag differs).
 *
 * For each we forward-declare only the three API symbols we actually
 * call, treating the dep pointer as void* at the boundary. Each bridge
 * stores its dep opaquely, so this is ABI-compatible. */

struct cochlea_immune_bridge;
typedef struct cochlea_immune_bridge cochlea_immune_bridge_t;
extern cochlea_immune_bridge_t* cochlea_immune_bridge_create(
    cochlea_t* cochlea, void* immune, const void* config);
extern void cochlea_immune_bridge_destroy(cochlea_immune_bridge_t* bridge);
extern nimcp_error_t cochlea_immune_bridge_update(
    cochlea_immune_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms);

struct cochlea_substrate_bridge;
typedef struct cochlea_substrate_bridge cochlea_substrate_bridge_t;
extern cochlea_substrate_bridge_t* cochlea_substrate_bridge_create(
    cochlea_t* cochlea, void* substrate, const void* config);
extern void cochlea_substrate_bridge_destroy(cochlea_substrate_bridge_t* bridge);
extern nimcp_error_t cochlea_substrate_bridge_update(
    cochlea_substrate_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms);

struct cochlea_medulla_bridge;
typedef struct cochlea_medulla_bridge cochlea_medulla_bridge_t;
extern cochlea_medulla_bridge_t* cochlea_medulla_bridge_create(
    cochlea_t* cochlea, void* medulla, const void* config);
extern void cochlea_medulla_bridge_destroy(cochlea_medulla_bridge_t* bridge);
extern nimcp_error_t cochlea_medulla_bridge_update(
    cochlea_medulla_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms);

/* The audio_cortex bridge takes a `cochlea_thalamic_bridge_t*` in its
 * create signature and would transitively pull in thalamic_bridge.h (which
 * collides with parietal's thalamic_router_t). Forward-declare the API we
 * need and treat the audio_cortex / thalamic bridge deps as void*. */
struct cochlea_thalamic_bridge;
typedef struct cochlea_thalamic_bridge cochlea_thalamic_bridge_t;
struct cochlea_audio_cortex_bridge;
typedef struct cochlea_audio_cortex_bridge cochlea_audio_cortex_bridge_t;
struct mgn_output; /* audio_cortex update takes optional mgn_output_t*, we pass NULL */
typedef struct mgn_output mgn_output_t;
extern cochlea_audio_cortex_bridge_t* cochlea_audio_cortex_bridge_create(
    cochlea_t* cochlea,
    cochlea_thalamic_bridge_t* thalamic_bridge,
    void* audio_cortex,
    const void* config);
extern void cochlea_audio_cortex_bridge_destroy(
    cochlea_audio_cortex_bridge_t* bridge);
extern nimcp_error_t cochlea_audio_cortex_bridge_update(
    cochlea_audio_cortex_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    const mgn_output_t* mgn_output,
    float dt_ms);

/* Wave 8A completion — the 3 bridges the first pass skipped because their
 * declared dep types (thalamus_t, kg_engine_t, sleep_controller_t) are
 * ghost typedefs: forward-declared in the bridge headers but never backed
 * by a real struct. Same opaque-stash pattern as immune/substrate/medulla
 * above — bridges store the pointer without dereferencing, so passing the
 * brain's real pointer (thalamic_router / internal_kg / sleep_system) as
 * void* is ABI-compatible. Confirmed by grepping bridge .c files for any
 * `thalamus->field`, `kg_engine->field`, `sleep_controller->field`
 * dereferences — none exist (2026-04-24). */
struct cochlea_kg_bridge;
typedef struct cochlea_kg_bridge cochlea_kg_bridge_t;
extern cochlea_kg_bridge_t* cochlea_kg_bridge_create(
    cochlea_t* cochlea, void* kg, const void* config);
extern void cochlea_kg_bridge_destroy(cochlea_kg_bridge_t* bridge);
extern nimcp_error_t cochlea_kg_bridge_update(
    cochlea_kg_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms);

struct cochlea_sleep_bridge;
typedef struct cochlea_sleep_bridge cochlea_sleep_bridge_t;
extern cochlea_sleep_bridge_t* cochlea_sleep_bridge_create(
    cochlea_t* cochlea, void* sleep, const void* config);
extern void cochlea_sleep_bridge_destroy(cochlea_sleep_bridge_t* bridge);
extern nimcp_error_t cochlea_sleep_bridge_update(
    cochlea_sleep_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms);

/* cochlea_thalamic_bridge already forward-declared above (line 122) because
 * cochlea_audio_cortex_bridge_create takes it as a parameter type. Add the
 * create/destroy/update APIs here with void* for the thalamus dep. */
extern cochlea_thalamic_bridge_t* cochlea_thalamic_bridge_create(
    cochlea_t* cochlea, void* thalamus, const void* config);
extern void cochlea_thalamic_bridge_destroy(cochlea_thalamic_bridge_t* bridge);
extern nimcp_error_t cochlea_thalamic_bridge_update(
    cochlea_thalamic_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms);

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_cochlea, MESH_ADAPTER_CATEGORY_SYSTEM)

#define COCHLEA_INIT_LOG_MODULE "BRAIN_INIT_COCHLEA"

/* Global bio-router singleton — created in parallel_init Wave 13. We cache
 * a stable pointer at init time so the bridge's stored `bio_router_t*` stays
 * valid for the brain's lifetime.
 *
 * NOTE: bio_router_t is itself a pointer typedef (struct bio_router_struct*),
 * so the bridge's `bio_router_t*` field is effectively a pointer-to-pointer
 * into this file-local storage slot — which is how the existing code pattern
 * uses it elsewhere in the tree. */
static bio_router_t g_cochlea_router_cache = NULL;

/*=============================================================================
 * TICK WRAPPER
 *
 * One cycle ticks every live bridge in order. Each bridge *_update() call
 * is null-guarded. We pass dt_ms=10 matching the 10ms cycle interval, and
 * NULL for cochlea_output — bridges are expected to sample from brain->
 * cochlea internally when output is NULL. The audio_cortex bridge also
 * takes an mgn_output_t*; we pass NULL (the bridge synthesizes or reads
 * from its thalamic_bridge dep internally).
 *===========================================================================*/

static void cochlea_bridges_tick_wrapper(void* ctx) {
    struct brain_struct* brain = (struct brain_struct*)ctx;
    if (!brain || !brain->cochlea_bridges_enabled) return;
    const float dt_ms = 10.0f;
    /* Wave 8A+ (2026-04-24): 11 of the 15 cochlea bridges hard-reject a NULL
     * cochlea_output and early-return. brain->cochlea_output_buffer is a
     * shared, zero-initialized cochlea_output_t allocated at init time.
     * Passing it instead of NULL lets each bridge actually advance state
     * (update its predictions/precision/free-energy/etc.) on every tick. */
    const cochlea_output_t* cb = (const cochlea_output_t*)brain->cochlea_output_buffer;

    if (brain->cochlea_audio_cortex_bridge) {
        (void)cochlea_audio_cortex_bridge_update(
            brain->cochlea_audio_cortex_bridge, cb, NULL, dt_ms);
    }
    if (brain->cochlea_bio_async_bridge) {
        (void)cochlea_bio_async_bridge_update(
            brain->cochlea_bio_async_bridge, cb, dt_ms);
    }
    if (brain->cochlea_broca_bridge) {
        (void)cochlea_broca_bridge_update(
            brain->cochlea_broca_bridge, cb, dt_ms);
    }
    if (brain->cochlea_collective_bridge) {
        (void)cochlea_collective_bridge_update(
            brain->cochlea_collective_bridge, cb, dt_ms);
    }
    if (brain->cochlea_cortical_deep_bridge) {
        (void)cochlea_cortical_deep_bridge_update(
            brain->cochlea_cortical_deep_bridge, cb, dt_ms);
    }
    if (brain->cochlea_fep_bridge) {
        (void)cochlea_fep_bridge_update(
            brain->cochlea_fep_bridge, cb, dt_ms);
    }
    if (brain->cochlea_immune_bridge) {
        (void)cochlea_immune_bridge_update(
            brain->cochlea_immune_bridge, cb, dt_ms);
    }
    if (brain->cochlea_medulla_bridge) {
        (void)cochlea_medulla_bridge_update(
            brain->cochlea_medulla_bridge, cb, dt_ms);
    }
    if (brain->cochlea_occipital_bridge) {
        (void)cochlea_occipital_bridge_update(
            brain->cochlea_occipital_bridge, cb, dt_ms);
    }
    if (brain->cochlea_rcog_bridge) {
        (void)cochlea_rcog_bridge_update(
            brain->cochlea_rcog_bridge, cb, dt_ms);
    }
    if (brain->cochlea_substrate_bridge) {
        (void)cochlea_substrate_bridge_update(
            brain->cochlea_substrate_bridge, cb, dt_ms);
    }
    if (brain->cochlea_verification_bridge) {
        (void)cochlea_verification_bridge_update(
            brain->cochlea_verification_bridge, dt_ms);
    }
    if (brain->cochlea_thalamic_bridge) {
        (void)cochlea_thalamic_bridge_update(
            (cochlea_thalamic_bridge_t*)brain->cochlea_thalamic_bridge,
            cb, dt_ms);
    }
    if (brain->cochlea_kg_bridge) {
        (void)cochlea_kg_bridge_update(
            (cochlea_kg_bridge_t*)brain->cochlea_kg_bridge, cb, dt_ms);
    }
    if (brain->cochlea_sleep_bridge) {
        (void)cochlea_sleep_bridge_update(
            (cochlea_sleep_bridge_t*)brain->cochlea_sleep_bridge, cb, dt_ms);
    }
}

/*=============================================================================
 * PER-BRIDGE INIT HELPERS — one per wireable bridge
 *
 * Contract: returns true if the bridge is live after the call (either it was
 * successfully created this call, or its dep was NULL so we skipped
 * cleanly — both are "not a hard failure" from the wave dispatcher's POV).
 * Only returns false if something genuinely broke (logged).
 *===========================================================================*/

/* Helper: create thalamic bridge (extracted from the main factory so
 * init_audio_cortex_bridge can call it if thalamic hasn't been wired yet).
 * Idempotent — no-op if already created. Returns true on success or
 * intentional skip (dep missing), false only on create failure. */
static bool init_thalamic_bridge(struct brain_struct* brain) {
    if (brain->cochlea_thalamic_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->thalamic_router) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "thalamic_bridge skipped — brain->thalamic_router is NULL");
        return true;
    }
    cochlea_thalamic_bridge_t* b = cochlea_thalamic_bridge_create(
        brain->cochlea, (void*)brain->thalamic_router, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "cochlea_thalamic_bridge_create failed");
        return false;
    }
    brain->cochlea_thalamic_bridge = (struct cochlea_thalamic_bridge*)b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
        "thalamic_bridge wired (via brain->thalamic_router)");
    return true;
}

static bool init_audio_cortex_bridge(struct brain_struct* brain) {
    if (brain->cochlea_audio_cortex_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->audio_cortex) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "audio_cortex_bridge skipped — brain->audio_cortex is NULL");
        return true;
    }
    /* Ensure thalamic_bridge is wired BEFORE audio_cortex_bridge so we can
     * pass the real bridge pointer. audio_cortex_bridge stores the
     * thalamic_bridge pointer opaquely — if it's NULL at create time the
     * reference is NULL forever and any future feature depending on
     * MGN→A1 relay would get silently broken. Calling init_thalamic_bridge
     * here is idempotent; the dedicated caller later in the main factory
     * becomes a no-op. */
    (void)init_thalamic_bridge(brain);

    /* audio_cortex passed as void* (bridge stores it opaquely). */
    cochlea_audio_cortex_bridge_t* b = cochlea_audio_cortex_bridge_create(
        brain->cochlea,
        (cochlea_thalamic_bridge_t*)brain->cochlea_thalamic_bridge,  /* may still be NULL if thalamic_router absent */
        (void*)brain->audio_cortex, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "audio_cortex_bridge_create failed");
        return false;
    }
    brain->cochlea_audio_cortex_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
        "audio_cortex_bridge wired (thalamic_bridge=%s)",
        brain->cochlea_thalamic_bridge ? "live" : "NULL");
    return true;
}

static bool init_bio_async_bridge(struct brain_struct* brain) {
    if (brain->cochlea_bio_async_bridge) return true;
    if (!brain->cochlea) return true;
    if (!bio_router_is_initialized()) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "bio_async_bridge skipped — global bio_router not initialized");
        return true;
    }
    g_cochlea_router_cache = bio_router_get_global();
    if (!g_cochlea_router_cache) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "bio_async_bridge skipped — bio_router_get_global returned NULL");
        return true;
    }
    cochlea_bio_async_bridge_t* b = cochlea_bio_async_bridge_create(
        brain->cochlea, &g_cochlea_router_cache, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "bio_async_bridge_create failed");
        return false;
    }
    brain->cochlea_bio_async_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "bio_async_bridge wired");
    return true;
}

static bool init_broca_bridge(struct brain_struct* brain) {
    if (brain->cochlea_broca_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->broca) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "broca_bridge skipped — brain->broca is NULL");
        return true;
    }
    cochlea_broca_bridge_t* b = cochlea_broca_bridge_create(
        brain->cochlea, brain->broca, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE, "broca_bridge_create failed");
        return false;
    }
    brain->cochlea_broca_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "broca_bridge wired");
    return true;
}

static bool init_collective_bridge(struct brain_struct* brain) {
    if (brain->cochlea_collective_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->collective_cognition) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "collective_bridge skipped — brain->collective_cognition is NULL");
        return true;
    }
    cochlea_collective_bridge_t* b = cochlea_collective_bridge_create(
        brain->cochlea, brain->collective_cognition, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "collective_bridge_create failed");
        return false;
    }
    brain->cochlea_collective_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "collective_bridge wired");
    return true;
}

static bool init_cortical_deep_bridge(struct brain_struct* brain) {
    if (brain->cochlea_cortical_deep_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->cortical_column_pool) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "cortical_deep_bridge skipped — brain->cortical_column_pool is NULL");
        return true;
    }
    cochlea_cortical_deep_bridge_t* b = cochlea_cortical_deep_bridge_create(
        brain->cochlea, brain->cortical_column_pool, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "cortical_deep_bridge_create failed");
        return false;
    }
    brain->cochlea_cortical_deep_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "cortical_deep_bridge wired");
    return true;
}

static bool init_fep_bridge(struct brain_struct* brain) {
    if (brain->cochlea_fep_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->fep_orchestrator) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "fep_bridge skipped — brain->fep_orchestrator is NULL");
        return true;
    }
    cochlea_fep_bridge_t* b = cochlea_fep_bridge_create(
        brain->cochlea, brain->fep_orchestrator, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE, "fep_bridge_create failed");
        return false;
    }
    brain->cochlea_fep_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "fep_bridge wired");
    return true;
}

static bool init_immune_bridge(struct brain_struct* brain) {
    if (brain->cochlea_immune_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->immune_system) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "immune_bridge skipped — brain->immune_system is NULL");
        return true;
    }
    /* Bridge takes immune as void* (forward-declared above to avoid enum
     * collision with brain-internal immune types). */
    cochlea_immune_bridge_t* b = cochlea_immune_bridge_create(
        brain->cochlea, (void*)brain->immune_system, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "immune_bridge_create failed");
        return false;
    }
    brain->cochlea_immune_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "immune_bridge wired");
    return true;
}

static bool init_medulla_bridge(struct brain_struct* brain) {
    if (brain->cochlea_medulla_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->medulla) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "medulla_bridge skipped — brain->medulla is NULL");
        return true;
    }
    /* brain->medulla is medulla_t (already an opaque pointer — from
     * nimcp_medulla.h: typedef struct medulla_struct* medulla_t). The
     * bridge's real parameter type is a different `struct medulla*`
     * tag, but the bridge only stores it opaquely — a void* is
     * ABI-compatible, which is why we forward-declared the bridge API
     * with `void* medulla` above. */
    cochlea_medulla_bridge_t* b = cochlea_medulla_bridge_create(
        brain->cochlea, (void*)brain->medulla, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "medulla_bridge_create failed");
        return false;
    }
    brain->cochlea_medulla_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "medulla_bridge wired");
    return true;
}

static bool init_occipital_bridge(struct brain_struct* brain) {
    if (brain->cochlea_occipital_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->occipital) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "occipital_bridge skipped — brain->occipital is NULL");
        return true;
    }
    cochlea_occipital_bridge_t* b = cochlea_occipital_bridge_create(
        brain->cochlea, brain->occipital, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "occipital_bridge_create failed");
        return false;
    }
    brain->cochlea_occipital_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "occipital_bridge wired");
    return true;
}

static bool init_rcog_bridge(struct brain_struct* brain) {
    if (brain->cochlea_rcog_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->rcog_engine) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "rcog_bridge skipped — brain->rcog_engine is NULL");
        return true;
    }
    cochlea_rcog_bridge_t* b = cochlea_rcog_bridge_create(
        brain->cochlea, brain->rcog_engine, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE, "rcog_bridge_create failed");
        return false;
    }
    brain->cochlea_rcog_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "rcog_bridge wired");
    return true;
}

static bool init_substrate_bridge(struct brain_struct* brain) {
    if (brain->cochlea_substrate_bridge) return true;
    if (!brain->cochlea) return true;
    if (!brain->substrate) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "substrate_bridge skipped — brain->substrate is NULL");
        return true;
    }
    /* Bridge takes substrate as void* (forward-declared above to avoid
     * neuron_population_t tag collision with brain-internal). */
    cochlea_substrate_bridge_t* b = cochlea_substrate_bridge_create(
        brain->cochlea, (void*)brain->substrate, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "substrate_bridge_create failed");
        return false;
    }
    brain->cochlea_substrate_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "substrate_bridge wired");
    return true;
}

static bool init_verification_bridge(struct brain_struct* brain) {
    if (brain->cochlea_verification_bridge) return true;
    if (!brain->cochlea) return true;
    cochlea_verification_bridge_t* b = cochlea_verification_bridge_create(
        brain->cochlea, NULL);
    if (!b) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "verification_bridge_create failed");
        return false;
    }
    brain->cochlea_verification_bridge = b;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE, "verification_bridge wired");
    return true;
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

bool nimcp_brain_factory_init_cochlea_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_cochlea_subsystem: brain is NULL");
        return false;
    }

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;
    if (!coord || !brain->cycle_coordinator_enabled) {
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "cycle coordinator absent — skipping cochlea wiring");
        return false;
    }

    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
        "Initializing cochlea + 12 bridges (Wave 8A)");

    /* Cochlea itself first — bridges all need it. */
    if (!brain->cochlea) {
        cochlea_config_t cfg = cochlea_config_default(BM_MODE_HUMAN, 16000u);
        brain->cochlea = cochlea_create(&cfg);
        if (!brain->cochlea) {
            LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
                "cochlea_create failed — skipping all bridges");
            return false;
        }
        brain->cochlea_enabled = true;
        LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
            "cochlea created (human mode, 16 kHz)");
    }

    /* Wave 8A+ (2026-04-24) — allocate the shared zero-fill cochlea_output
     * buffer that every tick will feed into the 11 NULL-rejecting consumer
     * bridges. Without this, the bridges early-return on !cochlea_output
     * and their state never advances, turning the whole wiring wave into a
     * semi-statue. 1024 samples is enough for 64ms @ 16kHz per tick, matching
     * the 10ms driven cycle with comfortable slack. */
    if (brain->cochlea && !brain->cochlea_output_buffer) {
        cochlea_output_t* out = cochlea_output_create(brain->cochlea, 1024u);
        if (out) {
            cochlea_output_clear(out);  /* Explicit zero-fill. */
            brain->cochlea_output_buffer = (void*)out;
            LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
                "cochlea_output_buffer allocated (1024 samples, zeroed)");
        } else {
            LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
                "cochlea_output_create failed — bridges will early-return on NULL");
        }
    }

    /* Bridge inits — each is independently failure-tolerant. */
    (void)init_audio_cortex_bridge(brain);
    (void)init_bio_async_bridge(brain);
    (void)init_broca_bridge(brain);
    (void)init_collective_bridge(brain);
    (void)init_cortical_deep_bridge(brain);
    (void)init_fep_bridge(brain);
    (void)init_immune_bridge(brain);
    (void)init_medulla_bridge(brain);
    (void)init_occipital_bridge(brain);
    (void)init_rcog_bridge(brain);
    (void)init_substrate_bridge(brain);
    (void)init_verification_bridge(brain);

    /* Wave 8A completion — the 3 bridges with ghost-typedef deps. Each
     * skips cleanly if its brain-side pointer is NULL. */
    if (brain->cochlea && brain->thalamic_router &&
        !brain->cochlea_thalamic_bridge) {
        cochlea_thalamic_bridge_t* b = cochlea_thalamic_bridge_create(
            brain->cochlea, (void*)brain->thalamic_router, NULL);
        if (b) {
            brain->cochlea_thalamic_bridge = (struct cochlea_thalamic_bridge*)b;
            LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
                "thalamic_bridge wired (via brain->thalamic_router)");
        } else {
            LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
                "cochlea_thalamic_bridge_create failed");
        }
    }

    if (brain->cochlea && brain->internal_kg && !brain->cochlea_kg_bridge) {
        cochlea_kg_bridge_t* b = cochlea_kg_bridge_create(
            brain->cochlea, (void*)brain->internal_kg, NULL);
        if (b) {
            brain->cochlea_kg_bridge = (struct cochlea_kg_bridge*)b;
            LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
                "kg_bridge wired (via brain->internal_kg)");
        } else {
            LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
                "cochlea_kg_bridge_create failed");
        }
    }

    if (brain->cochlea && brain->sleep_system && !brain->cochlea_sleep_bridge) {
        cochlea_sleep_bridge_t* b = cochlea_sleep_bridge_create(
            brain->cochlea, (void*)brain->sleep_system, NULL);
        if (b) {
            brain->cochlea_sleep_bridge = (struct cochlea_sleep_bridge*)b;
            LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
                "sleep_bridge wired (via brain->sleep_system)");
        } else {
            LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
                "cochlea_sleep_bridge_create failed");
        }
    }

    /* Register the driven cycle LAST — now every live bridge will tick. */
    int reg_rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, 10000ull,  /* 10ms */
        cochlea_bridges_tick_wrapper, brain, NULL);
    if (reg_rc != 0) {
        LOG_MODULE_WARN(COCHLEA_INIT_LOG_MODULE,
            "register_driven(COCHLEA_BRIDGES) failed — bridges created but "
            "will not tick periodically (hot-path calls still work)");
        /* Don't tear anything down — hot-path wiring in brain_decide /
         * brain_learn_vector will still exercise the bridges. */
        return true;
    }

    brain->cochlea_bridges_enabled = true;
    LOG_MODULE_INFO(COCHLEA_INIT_LOG_MODULE,
        "cochlea + bridges wired (10ms cycle)");
    return true;
}

void nimcp_brain_factory_destroy_cochlea_subsystem(brain_t brain) {
    if (!brain) return;

    brain_cycle_coordinator_t* coord =
        (brain_cycle_coordinator_t*)brain->cycle_coordinator;

    /* Unregister cycle FIRST so no tick is in flight when we start freeing. */
    if (brain->cochlea_bridges_enabled && coord) {
        (void)brain_cycle_coordinator_unregister(coord,
                                                  BRAIN_CYCLE_COCHLEA_BRIDGES);
        brain->cochlea_bridges_enabled = false;
    }

    /* Destroy bridges in reverse creation order. Each destroy is NULL-safe. */
    /* Wave 8A completion: destroy the 3 ghost-typedef bridges first since
     * they were created last. */
    if (brain->cochlea_sleep_bridge) {
        cochlea_sleep_bridge_destroy(
            (cochlea_sleep_bridge_t*)brain->cochlea_sleep_bridge);
        brain->cochlea_sleep_bridge = NULL;
    }
    if (brain->cochlea_kg_bridge) {
        cochlea_kg_bridge_destroy(
            (cochlea_kg_bridge_t*)brain->cochlea_kg_bridge);
        brain->cochlea_kg_bridge = NULL;
    }
    if (brain->cochlea_thalamic_bridge) {
        cochlea_thalamic_bridge_destroy(
            (cochlea_thalamic_bridge_t*)brain->cochlea_thalamic_bridge);
        brain->cochlea_thalamic_bridge = NULL;
    }

    if (brain->cochlea_verification_bridge) {
        cochlea_verification_bridge_destroy(brain->cochlea_verification_bridge);
        brain->cochlea_verification_bridge = NULL;
    }
    if (brain->cochlea_substrate_bridge) {
        cochlea_substrate_bridge_destroy(brain->cochlea_substrate_bridge);
        brain->cochlea_substrate_bridge = NULL;
    }
    if (brain->cochlea_rcog_bridge) {
        cochlea_rcog_bridge_destroy(brain->cochlea_rcog_bridge);
        brain->cochlea_rcog_bridge = NULL;
    }
    if (brain->cochlea_occipital_bridge) {
        cochlea_occipital_bridge_destroy(brain->cochlea_occipital_bridge);
        brain->cochlea_occipital_bridge = NULL;
    }
    if (brain->cochlea_medulla_bridge) {
        cochlea_medulla_bridge_destroy(brain->cochlea_medulla_bridge);
        brain->cochlea_medulla_bridge = NULL;
    }
    if (brain->cochlea_immune_bridge) {
        cochlea_immune_bridge_destroy(brain->cochlea_immune_bridge);
        brain->cochlea_immune_bridge = NULL;
    }
    if (brain->cochlea_fep_bridge) {
        cochlea_fep_bridge_destroy(brain->cochlea_fep_bridge);
        brain->cochlea_fep_bridge = NULL;
    }
    if (brain->cochlea_cortical_deep_bridge) {
        cochlea_cortical_deep_bridge_destroy(brain->cochlea_cortical_deep_bridge);
        brain->cochlea_cortical_deep_bridge = NULL;
    }
    if (brain->cochlea_collective_bridge) {
        cochlea_collective_bridge_destroy(brain->cochlea_collective_bridge);
        brain->cochlea_collective_bridge = NULL;
    }
    if (brain->cochlea_broca_bridge) {
        cochlea_broca_bridge_destroy(brain->cochlea_broca_bridge);
        brain->cochlea_broca_bridge = NULL;
    }
    if (brain->cochlea_bio_async_bridge) {
        cochlea_bio_async_bridge_destroy(brain->cochlea_bio_async_bridge);
        brain->cochlea_bio_async_bridge = NULL;
    }
    if (brain->cochlea_audio_cortex_bridge) {
        cochlea_audio_cortex_bridge_destroy(brain->cochlea_audio_cortex_bridge);
        brain->cochlea_audio_cortex_bridge = NULL;
    }

    /* Shared output buffer (Wave 8A+ 2026-04-24) — destroy BEFORE the
     * cochlea itself, because cochlea_output_destroy may read cochlea state
     * to free its per-channel arrays safely. */
    if (brain->cochlea_output_buffer) {
        cochlea_output_destroy((cochlea_output_t*)brain->cochlea_output_buffer);
        brain->cochlea_output_buffer = NULL;
    }

    /* Cochlea last. */
    if (brain->cochlea) {
        cochlea_destroy(brain->cochlea);
        brain->cochlea = NULL;
        brain->cochlea_enabled = false;
    }
}
