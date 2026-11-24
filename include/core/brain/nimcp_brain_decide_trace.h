/**
 * @file nimcp_brain_decide_trace.h
 * @brief Opt-in stage-by-stage dump of the decision output vector (2026-11-11)
 *
 * WHY: the overfit probe (scripts/overfit_probe.py) showed learn_vector does
 * not move decide_full outputs toward trained targets. brain_decide rewrites
 * output_vector in ~a dozen places after the ANN forward; this trace records
 * the vector after each so the stage that discards the learned signal can be
 * found. learn_vector records its target and the ANN prediction too.
 *
 * Enabled by NIMCP_DECIDE_TRACE=<file>. Off (one cached getenv) otherwise.
 * NIMCP_DECIDE_TRACE_NAN=1 (without NIMCP_DECIDE_TRACE): write nothing, but
 * report on stderr every stage whose vector holds NaN/Inf ("[NAN-TRIP] ..."),
 * first 200 reports then every 1000th. Used to find where a NaN originates.
 * Record layout, appended to <file>, little-endian:
 *   uint32 magic 0x54524344 ("DCRT"), uint64 seq, char stage[32],
 *   uint32 n, float v[n]
 * seq increments on every nimcp_decide_trace_begin() (one per brain_decide
 * or brain_learn_vector call); records written outside a call carry seq 0.
 */
#ifndef NIMCP_BRAIN_DECIDE_TRACE_H
#define NIMCP_BRAIN_DECIDE_TRACE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** True when NIMCP_DECIDE_TRACE or NIMCP_DECIDE_TRACE_NAN=1 is set (cached). */
bool nimcp_decide_trace_enabled(void);

/** Start a new call: bumps the sequence number that tags later records. */
void nimcp_decide_trace_begin(void);

/** Append one record (no-op when disabled or v is NULL). */
void nimcp_decide_trace(const char* stage, const float* v, uint32_t n);

/** Append a read-only forward of the ANN (adaptive_network_t passed as void*)
 *  on (x, n) with out_n outputs. Probes the network's state at a point in the
 *  pipeline without touching it. No-op when disabled. */
void nimcp_decide_trace_ann(const char* stage, void* adaptive_net, const float* x,
                            uint32_t n, uint32_t out_n);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_DECIDE_TRACE_H */
