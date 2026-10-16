/**
 * @file nimcp_thalamic_channel.c
 * @brief Shared thalamic-channel adapter for LNN/CNN/FNO/HNN (and future SNN).
 *
 * Thin wrapper over thalamic_router_t that presents a narrow, uniform
 * adapter API: add_destination / get_gate / submit / tick. Caches the
 * router's current attention weights per destination so get_gate() is an
 * O(1) read without touching the router's internal mutex.
 *
 * The router itself performs Hebbian route strengthening inside
 * thalamic_router_route_signal() when enable_learning is true, so this
 * channel's tick() merely refreshes the cached attention snapshot.
 */

#include "core/thalamic/nimcp_thalamic_channel.h"

#include <stddef.h>
#include <string.h>

#include "middleware/routing/nimcp_thalamic_router.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

thalamic_channel_t* thalamic_channel_create(thalamic_router_t* router,
                                            uint32_t source_id)
{
    if (!router) {
        return NULL;
    }

    thalamic_channel_t* ch =
        (thalamic_channel_t*)nimcp_calloc(1, sizeof(thalamic_channel_t));
    if (!ch) {
        return NULL;
    }

    ch->source_id      = source_id;
    ch->n_destinations = 0;
    ch->mode           = THALAMIC_CHAN_ADAPTIVE;
    ch->router         = router;
    ch->submits_total  = 0;
    ch->gates_fetched  = 0;

    /* Attention weights default to 1.0 (no suppression). destinations[] is
     * left zeroed; we track valid slots via n_destinations. */
    for (uint32_t i = 0; i < THALAMIC_CHANNEL_MAX_DESTS; i++) {
        ch->attention_weights[i] = 1.0f;
    }

    return ch;
}

void thalamic_channel_destroy(thalamic_channel_t* ch)
{
    if (!ch) return;
    /* Router is borrowed — do NOT destroy it here. */
    nimcp_free(ch);
}

/* ---------------------------------------------------------------------------
 * Destinations
 * ------------------------------------------------------------------------- */

nimcp_error_t thalamic_channel_add_destination(thalamic_channel_t* ch,
                                               uint32_t dest_id)
{
    if (!ch) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (ch->n_destinations >= THALAMIC_CHANNEL_MAX_DESTS) {
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    uint32_t slot = ch->n_destinations;
    ch->destinations[slot]       = dest_id;
    ch->attention_weights[slot]  = 1.0f;  /* conservative: no suppression */
    ch->n_destinations           = slot + 1;
    return NIMCP_SUCCESS;
}

/* Internal: linear search for a destination. Returns slot index or -1. */
static int32_t find_dest_slot(const thalamic_channel_t* ch, uint32_t dest_id)
{
    for (uint32_t i = 0; i < ch->n_destinations; i++) {
        if (ch->destinations[i] == dest_id) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * Read & submit
 * ------------------------------------------------------------------------- */

float thalamic_channel_get_gate(const thalamic_channel_t* ch,
                                uint32_t dest_id)
{
    if (!ch) {
        return 1.0f;  /* conservative default */
    }

    /* Mutate counter on a logically-const read: the count is advisory and we
     * want get_gate() to behave like a const accessor to the caller. Cast
     * away const for the counter only. */
    thalamic_channel_t* mut = (thalamic_channel_t*)ch;
    mut->gates_fetched += 1;

    int32_t slot = find_dest_slot(ch, dest_id);
    if (slot < 0) {
        return 1.0f;
    }
    return ch->attention_weights[slot];
}

nimcp_error_t thalamic_channel_submit(thalamic_channel_t* ch,
                                      uint32_t dest_id,
                                      const float* payload,
                                      size_t n,
                                      int priority)
{
    if (!ch || !ch->router) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!payload || n == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Map priority to the router's enum. Anything outside the known band
     * collapses to NORMAL. */
    signal_priority_t prio;
    if (priority == THALAMIC_PRIORITY_HIGH) {
        prio = SIGNAL_PRIORITY_HIGH;
    } else if (priority == THALAMIC_PRIORITY_LOW) {
        prio = SIGNAL_PRIORITY_LOW;
    } else {
        prio = SIGNAL_PRIORITY_NORMAL;
    }

    /* Use the cached attention weight if we have the dest registered. */
    float attention = 1.0f;
    int32_t slot = find_dest_slot(ch, dest_id);
    if (slot >= 0) {
        attention = ch->attention_weights[slot];
    }

    /* Build a stack-local routed_signal_t. The router's enqueue path copies
     * via signal_wrapper's CoW manager, and the high-priority path delivers
     * synchronously, so lifetime of these pointers through this call is
     * sufficient. */
    uint32_t dests[1] = { dest_id };
    routed_signal_t sig;
    memset(&sig, 0, sizeof(sig));
    sig.source_id        = ch->source_id;
    sig.dest_ids         = dests;
    sig.num_dests        = 1;
    sig.signal_data      = (float*)payload;
    sig.signal_size      = (uint32_t)n;
    sig.attention_weight = attention;
    sig.priority         = prio;
    sig.timestamp_ms     = 0;
    sig.bypass_queue     = (prio == SIGNAL_PRIORITY_HIGH);

    bool ok = thalamic_router_route_signal(ch->router, &sig);
    if (!ok) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    ch->submits_total += 1;
    return NIMCP_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * Mode + tick
 * ------------------------------------------------------------------------- */

void thalamic_channel_set_mode(thalamic_channel_t* ch,
                               thalamic_channel_mode_t mode)
{
    if (!ch) return;
    ch->mode = mode;
}

void thalamic_channel_tick(thalamic_channel_t* ch)
{
    if (!ch || !ch->router) return;

    /* Hebbian learning is already applied inside thalamic_router_route_signal
     * (router->config.enable_learning). Here we just refresh the cached
     * attention weights so subsequent get_gate() calls observe the router's
     * current state. */
    for (uint32_t i = 0; i < ch->n_destinations; i++) {
        float w = 1.0f;
        if (thalamic_router_get_attention(ch->router,
                                          ch->source_id,
                                          ch->destinations[i],
                                          &w)) {
            ch->attention_weights[i] = w;
        }
        /* On failure, leave the cached value untouched. */
    }
}
