/**
 * @file nimcp_brain_wired.h
 * @brief Lifecycle hooks for modules that were compiled-out or never wired
 *        (dead-code audit 2026-11-24).
 *
 * Each group owns one slot, brain->wired[group], holding its private state,
 * and one implementation file, src/core/brain/wired/nimcp_brain_wired_<group>.c.
 * The dispatcher (nimcp_brain_wired.c) calls every group from the brain's
 * existing lifecycle points:
 *
 *   create  — end of nimcp_brain_eager_init_cognitive (after every subsystem
 *             a bridge may reference exists)
 *   tick    — brain_learn_vector tick chain (once per training step)
 *   save    — brain checkpoint save (sidecar <path>.wired_<group>, optional)
 *   load    — brain checkpoint load (missing sidecar = fresh state, rc 1)
 *   destroy — top of brain_destroy, after background drivers are stopped and
 *             before any subsystem a bridge references is freed
 *
 * A group is disabled at runtime with NIMCP_WIRED_<GROUP>=0 (e.g.
 * NIMCP_WIRED_GPU=0); disabled groups are never created, so every other hook
 * is a no-op for them.
 *
 * Contract for group implementations:
 *   - create returns 0 on success (state stored in brain->wired[g]), -1 on
 *     failure (slot left NULL; the brain keeps running).
 *   - tick must be cheap on average: rate-limit heavy work internally.
 *   - tick must never write NaN/Inf into shared brain state.
 *   - every hook is NULL-safe on brain->wired[g].
 */
#ifndef NIMCP_BRAIN_WIRED_H
#define NIMCP_BRAIN_WIRED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

typedef enum {
    BRAIN_WIRED_SLEEP = 0,      /* sleep-state bridges (cortical, middleware, perception, plasticity, introspection) */
    BRAIN_WIRED_BIOASYNC,       /* per-domain bio-async bridges + async event bus */
    BRAIN_WIRED_LANGUAGE,       /* language <-> cognitive-module bridges */
    BRAIN_WIRED_NOISE_IMMUNE,   /* pink-noise plasticity bridges + information/optimization/GPU immune bridges */
    BRAIN_WIRED_GPU,            /* CUDA pool / transfer / prefetch / topology / int8 / TensorRT / health */
    BRAIN_WIRED_COGNITIVE,      /* checkpointing, mental-health interventions, wellbeing, motor init, reasoning, training adapters, weights */
    BRAIN_WIRED_COMPUTE,        /* SIMD, semantic compression, distributed cognition, ternary LNN/topology, Louvain, swarm stats */
    BRAIN_WIRED_COUNT
} brain_wired_group_t;

#define BRAIN_WIRED_SLOTS 8     /* >= BRAIN_WIRED_COUNT; sizes brain->wired[] */

/* Dispatcher — called from the brain lifecycle. */
int  brain_wired_create_all(struct brain_struct* brain);   /* returns number of groups created */
void brain_wired_tick_all(struct brain_struct* brain, const float* features, uint32_t n, float dt_ms);
int  brain_wired_save_all(struct brain_struct* brain, const char* checkpoint_path);
int  brain_wired_load_all(struct brain_struct* brain, const char* checkpoint_path);
void brain_wired_destroy_all(struct brain_struct* brain);
/* JSON status of every group into buf; returns bytes written (excl. NUL). */
int  brain_wired_status_json(struct brain_struct* brain, char* buf, uint32_t cap);

/* Per-group hooks — implemented in src/core/brain/wired/nimcp_brain_wired_<group>.c. */
#define BRAIN_WIRED_DECLARE(g)                                                              \
    int  brain_wired_##g##_create(struct brain_struct* brain);                              \
    void brain_wired_##g##_tick(struct brain_struct* brain, const float* f, uint32_t n,     \
                                float dt_ms);                                               \
    int  brain_wired_##g##_save(struct brain_struct* brain, const char* path);              \
    int  brain_wired_##g##_load(struct brain_struct* brain, const char* path);              \
    void brain_wired_##g##_destroy(struct brain_struct* brain);                             \
    int  brain_wired_##g##_status_json(struct brain_struct* brain, char* buf, uint32_t cap);

BRAIN_WIRED_DECLARE(sleep)
BRAIN_WIRED_DECLARE(bioasync)
BRAIN_WIRED_DECLARE(language)
BRAIN_WIRED_DECLARE(noise_immune)
BRAIN_WIRED_DECLARE(gpu)
BRAIN_WIRED_DECLARE(cognitive)
BRAIN_WIRED_DECLARE(compute)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_WIRED_H */
