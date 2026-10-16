/**
 * @file nimcp_thalamic_channel.h
 * @brief Uniform per-network adapter for the thalamic router.
 *
 * Each network type that wants attention-gated routing owns one
 * thalamic_channel_t per source population. Narrow interface:
 *   - get_gate(dest): read current attention weight
 *   - submit(dest, payload, priority): route a signal
 *   - tick(): update Hebbian route weights
 *
 * Replaces the bespoke per-network bridge pattern with a single shared
 * adapter so LNN/CNN/FNO/HNN don't each need a separate bridge file.
 * The existing snn_thalamic_bridge continues to be used by SNN; this
 * channel is additive and may supplement/replace it in future work.
 */
#ifndef NIMCP_THALAMIC_CHANNEL_H
#define NIMCP_THALAMIC_CHANNEL_H

#include <stdint.h>
#include <stddef.h>

#include "utils/error/nimcp_error_codes.h"
#include "middleware/routing/nimcp_thalamic_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mirror THALAMIC_MAX_DESTINATIONS so a single channel owns at most one
 * fan-out group. Independent constant to allow divergence later. */
#define THALAMIC_CHANNEL_MAX_DESTS 16

typedef enum {
    THALAMIC_CHAN_BURST    = 0,
    THALAMIC_CHAN_TONIC    = 1,
    THALAMIC_CHAN_ADAPTIVE = 2
} thalamic_channel_mode_t;

typedef struct thalamic_channel_s {
    uint32_t                source_id;
    uint32_t                n_destinations;
    uint32_t                destinations[THALAMIC_CHANNEL_MAX_DESTS];
    float                   attention_weights[THALAMIC_CHANNEL_MAX_DESTS];
    thalamic_channel_mode_t mode;
    thalamic_router_t*      router;          /* borrowed — not owned */
    uint64_t                submits_total;
    uint64_t                gates_fetched;
} thalamic_channel_t;

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

/**
 * Allocate and initialize a new channel. `router` is borrowed — the caller
 * retains ownership of the router's lifecycle. Returns NULL if router is
 * NULL or allocation fails.
 */
thalamic_channel_t* thalamic_channel_create(thalamic_router_t* router,
                                            uint32_t source_id);

/**
 * Free a channel. Safe to call with NULL. Does not free the router.
 */
void thalamic_channel_destroy(thalamic_channel_t* ch);

/* ---------------------------------------------------------------------------
 * Destinations
 * ------------------------------------------------------------------------- */

/**
 * Register a destination on this channel. Initial attention weight is 1.0.
 * Returns NIMCP_SUCCESS on success, NIMCP_ERROR_NULL_POINTER if ch is NULL,
 * or NIMCP_ERROR_CAPACITY_EXCEEDED when THALAMIC_CHANNEL_MAX_DESTS has been
 * reached.
 */
nimcp_error_t thalamic_channel_add_destination(thalamic_channel_t* ch,
                                               uint32_t dest_id);

/* ---------------------------------------------------------------------------
 * Read & submit
 * ------------------------------------------------------------------------- */

/**
 * Return the current attention gate for `dest_id`. If the destination is
 * not registered (or ch is NULL), returns 1.0 — the conservative default
 * that does not suppress routing. Increments `gates_fetched`.
 */
float thalamic_channel_get_gate(const thalamic_channel_t* ch,
                                uint32_t dest_id);

/**
 * Submit a signal payload to `dest_id` via the underlying router.
 * `priority` is one of THALAMIC_PRIORITY_LOW/NORMAL/HIGH from the router
 * header. Safe no-op (returns NIMCP_ERROR_NULL_POINTER) if ch or its
 * router is NULL. Increments `submits_total` on a successful dispatch.
 */
nimcp_error_t thalamic_channel_submit(thalamic_channel_t* ch,
                                      uint32_t dest_id,
                                      const float* payload,
                                      size_t n,
                                      int priority);

/* ---------------------------------------------------------------------------
 * Mode + tick
 * ------------------------------------------------------------------------- */

/**
 * Set the relay mode. No-op on NULL channel.
 */
void thalamic_channel_set_mode(thalamic_channel_t* ch,
                               thalamic_channel_mode_t mode);

/**
 * End-of-step tick. Refreshes cached attention weights from the router
 * (so get_gate() observes the router's latest state) and lets the router's
 * own Hebbian learning — which happens automatically inside submit() — be
 * reflected in this channel's attention_weights snapshot. Safe no-op on
 * NULL channel.
 */
void thalamic_channel_tick(thalamic_channel_t* ch);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THALAMIC_CHANNEL_H */
