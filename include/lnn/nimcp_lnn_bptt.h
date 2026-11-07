/**
 * @file nimcp_lnn_bptt.h
 * @brief Exact truncated BPTT for LNN networks trained step by step (2026-11-07).
 *
 * WHAT: Records what each TRAINING forward step consumed (per layer: input u,
 *       state before x0, state after x1, substrate tau multiplier) in a ring
 *       buffer, and back-propagates a loss gradient injected at the newest
 *       step's output through the recorded steps, newest to oldest.
 *
 * WHY:  The legacy adjoint (lnn_gradient_compute_adjoint) used the CURRENT
 *       layer state at every history step, gave W_in no gradient and pushed
 *       the 64-dim output adjoint into every layer, so nothing linked a loss to
 *       earlier inputs: the LNN could not learn temporal structure (video
 *       next-frame prediction showed none).
 *
 * HOW:  Each recorded step is the CPU LTC map of lnn_layer_forward, per layer:
 *         s   = W_tau·[u; x0] + b_tau,  sg = sigmoid(s)
 *         tau = floor(tau_base·sg) · mod  (floor 0.01, re-floored when mod != 1)
 *         h   = W_in·u + b_in + W_rec·x0,  a = act(h)
 *         y   = x0·E + tau·(1 - E)·a,  E = exp(-dt/tau)   (cpu_integrator = 1, default)
 *             = x0 + dt·(-x0/tau + a)                    (cpu_integrator = 0)
 *         x1  = LN(clamp(y, ±10))  (LN only when use_layer_norm and n > 1)
 *       The backward recomputes these local quantities from the recorded u
 *       and x0 with the CURRENT weights (standard truncated BPTT on a
 *       stream; exact when the weights did not change inside the window) and
 *       accumulates into the layers' grad_* tensors. dL/dx0 of a step flows
 *       to the previous recorded step only when that step's x1 equals this
 *       step's x0 bit for bit; anything that moved the state in between (a
 *       decide forward, a reset) cuts the chain there.
 *
 *       Only forwards made while lnn_network_set_bptt_record(net, true) is in
 *       effect are recorded (the UTM adapter does this around its training
 *       forward). Decide/inference forwards never enter the trace.
 *       lnn_network_backward uses this path whenever the trace is non-empty
 *       and falls back to the legacy adjoint otherwise.
 */
#ifndef NIMCP_LNN_BPTT_H
#define NIMCP_LNN_BPTT_H

#include <stdbool.h>
#include <stdint.h>

#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Per-backward statistics (also mirrored into the log line). */
typedef struct {
    uint32_t steps_recorded;   /**< Entries in the trace */
    uint32_t steps_used;       /**< Steps the gradient actually reached */
    uint32_t chain_breaks;     /**< Discontinuities that stopped the chain */
    float    raw_norm;         /**< L2 norm of this call's parameter gradient, before clamps */
    float    final_norm;       /**< L2 norm of the layer grad tensors after clamps */
    double   time_ms;          /**< Wall time of the backward */
} lnn_bptt_stats_t;

/**
 * @brief Turn recording of subsequent forward steps on or off.
 *
 * The trace (capacity = config->bptt_truncation, default 100) is allocated on
 * the first recorded forward. Turning recording off keeps the trace.
 */
void lnn_network_set_bptt_record(lnn_network_t* network, bool on);

/** @brief Number of recorded steps currently in the trace (0 when none). */
uint32_t lnn_bptt_trace_len(const lnn_network_t* network);

/** @brief Drop every recorded step (keeps the allocation). */
void lnn_bptt_clear(lnn_network_t* network);

/** @brief Free the trace (called by lnn_network_destroy). */
void lnn_bptt_free(lnn_network_t* network);

/**
 * @brief Truncated BPTT through the recorded steps.
 *
 * @param network   Network whose layer grad_* tensors receive the gradient
 *                  (accumulated, not overwritten)
 * @param loss_grad dL/d(output) at the newest step, [n_outputs]
 * @param stats     Optional out statistics
 * @return LNN_SUCCESS, or an LNN_ERROR_* code (LNN_ERROR_INVALID_STATE when
 *         the trace is empty)
 */
int lnn_bptt_backward(lnn_network_t* network, const nimcp_tensor_t* loss_grad,
                      lnn_bptt_stats_t* stats);

/**
 * @brief dL/d(network input) at the newest step from the last BPTT backward.
 * @param n Out: number of floats (network->n_inputs), 0 when unavailable
 * @return Borrowed pointer valid until the next backward, or NULL
 */
const float* lnn_bptt_last_input_grad(const lnn_network_t* network, uint32_t* n);

/* ---- Recording hooks used by lnn_network_forward_step (network mutex held) ---- */

/** Start a record for one forward step; returns NULL when not recording. */
void* lnn_bptt_record_begin(lnn_network_t* network, float dt, float input_gate);
/** Before layer i's forward: copy its input and starting state. */
void  lnn_bptt_record_layer_pre(lnn_network_t* network, void* rec, uint32_t i,
                                const nimcp_tensor_t* layer_input);
/** After layer i's forward: copy its resulting state. */
void  lnn_bptt_record_layer_post(lnn_network_t* network, void* rec, uint32_t i);
/** Finish the record; ok=false discards it (forward failed). */
void  lnn_bptt_record_commit(lnn_network_t* network, void* rec, bool ok);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_BPTT_H */
