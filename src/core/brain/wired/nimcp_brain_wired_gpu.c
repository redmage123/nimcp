/**
 * @file nimcp_brain_wired_gpu.c
 * @brief Wired-module group BRAIN_WIRED_GPU. See include/core/brain/nimcp_brain_wired.h.
 *
 * WHAT: Drives the previously-uncompiled CUDA tooling from the live brain:
 *   - gpu/common/nimcp_gpu_health.cu      GPU health monitor (background probe of
 *                                         the brain's own device only) -> health agent
 *   - gpu/memory/nimcp_gpu_pool_factory.cu creates a small scratch pool on brain->gpu_ctx
 *   - gpu/memory/nimcp_gpu_pool.cu        that scratch pool serves the probe's device buffers
 *   - gpu/transfer/nimcp_async_transfer.cu transfer manager riding ctx->transfer_stream
 *                                         (no new streams); pinned staging + D2H readback
 *   - gpu/transfer/nimcp_prefetch.cu      streams the recent-input sample ring to the GPU
 *                                         through double buffers on the ctx streams
 *   - gpu/inference/nimcp_int8_inference.cu calibrates INT8 quantization of the brain's
 *                                         recent inputs; reports scale / SQNR / clip rate
 *   - gpu/topology/nimcp_topology_kernels.cu Louvain + clustering of the input-channel
 *                                         co-activation graph built from the same ring
 *   - gpu/inference/nimcp_tensorrt_export.cu on-demand: availability/version + one lazy
 *                                         performance estimate for an input-sized INT8
 *                                         layer (status only; no engine is ever built)
 *
 * WHY:  Gives the brain an "INT8/GPU readiness" view of its live input distribution and
 *       a GPU health signal in the health agent, without touching learning state.
 *
 * COST: tick() is a counter check on most steps. Every WG_SAMPLE_EVERY ticks it copies
 *       <= WG_MAX_DIM floats into a host ring (<= 16 KB memcpy). Every probe_every ticks
 *       (default 4096) it runs the GPU probe (~32 small H2D copies + a few tiny kernels,
 *       a few ms) — amortised well under 0.01 ms/tick. GPU health probing (cudaMemGetInfo
 *       + 1 KB test alloc) runs in its own thread every 30 s; tick only reads the cached
 *       result every WG_HEALTH_EVERY ticks.
 *
 * VRAM: scratch pool 256 KB (NIMCP_WIRED_GPU_POOL_KB), prefetch double buffers
 *       2 x dim x 4 B, calibrator 8 B, a 64-node graph. No arena is reserved beyond that.
 *
 * SAFETY: writes nothing into shared brain state; only reads input features and the
 *       GPU context. Never calls nimcp_gpu_health_recover (cudaDeviceReset).
 *       Without CUDA (NIMCP_ENABLE_CUDA undefined) every hook is a no-op and create()
 *       returns -1.
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef NIMCP_ENABLE_CUDA

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/common/nimcp_gpu_health.h"
#include "gpu/memory/nimcp_gpu_pool.h"
#include "gpu/memory/nimcp_gpu_pool_factory.h"
#include "gpu/transfer/nimcp_async_transfer.h"
#include "gpu/transfer/nimcp_prefetch.h"
#include "gpu/topology/nimcp_topology_gpu.h"
#include "gpu/inference/nimcp_int8_inference.h"
#include "gpu/inference/nimcp_tensorrt_export.h"
#include "utils/memory/nimcp_memory.h"

/*=============================================================================
 * Tunables
 *===========================================================================*/
#define WG_RING                 32u     /* recent-input samples per probe */
#define WG_MAX_DIM              4096u   /* features sampled per vector (prefix) */
#define WG_SAMPLE_EVERY         128u    /* ticks between ring samples */
#define WG_PROBE_EVERY_DEFAULT  4096u   /* ticks between GPU probes */
#define WG_HEALTH_EVERY         256u    /* ticks between health-cache reads */
#define WG_HEALTH_INTERVAL_MS   30000u  /* background health probe period */
#define WG_POOL_KB_DEFAULT      256u    /* scratch pool size */
#define WG_TOPO_NODES           64u     /* channel groups in co-activation graph */
#define WG_TOPO_EDGE_R          0.6f    /* |pearson r| threshold for an edge */
#define WG_MAX_FAILURES         3u      /* consecutive probe failures -> stop probing */
#define WG_SIDECAR_MAGIC        0x5047574Eu  /* "NWGP" */
#define WG_SIDECAR_VERSION      1u

typedef struct {
    /* borrowed */
    nimcp_gpu_context_t* ctx;
    int device_id;

    /* gpu_health */
    gpu_health_monitor_t* health;
    gpu_health_status_t last_reported_status;
    gpu_health_status_t last_status;
    uint64_t health_reports;
    size_t last_free_mb, last_total_mb;

    /* pool (factory-built) */
    nimcp_gpu_pool_t* scratch;
    size_t pool_bytes;

    /* lazily created on the first probe */
    nimcp_transfer_manager_t* xfer;
    nimcp_prefetch_manager_t* prefetch;
    uint32_t prefetch_dim;
    nimcp_int8_calibrator_t* calib;
    int8_t* h_codes;                 /* pinned, from xfer */
    uint32_t h_codes_cap;

    /* recent-input ring (host) */
    float* ring;                     /* WG_RING x WG_MAX_DIM */
    uint32_t dim;                    /* sampled width (0 until first sample) */
    uint32_t ring_count, ring_head;

    /* schedule */
    uint64_t ticks;
    uint32_t probe_every;
    uint32_t consecutive_failures;
    bool probing_disabled;

    /* int8 metrics (persisted) */
    uint64_t probes, probe_failures;
    float q_scale, q_min, q_max;
    int32_t q_zero_point;
    float sqnr_db, sqnr_ema_db, clip_rate;
    bool tensor_cores;

    /* topology metrics (persisted) */
    float topo_modularity, topo_clustering;
    uint32_t topo_communities, topo_edges, topo_nodes;

    /* prefetch/transfer stats snapshot (status reads only these copies) */
    uint64_t pf_batches, pf_stalls;
    uint64_t xfer_bytes;

    /* TensorRT (on-demand) */
    bool trt_available;
    bool trt_estimated;
    float trt_latency_ms, trt_throughput;
    size_t trt_memory_mb;
} wired_gpu_state_t;

typedef struct {
    uint32_t magic, version;
    uint64_t probes, probe_failures;
    float q_scale, q_min, q_max;
    int32_t q_zero_point;
    float sqnr_db, sqnr_ema_db, clip_rate;
    float topo_modularity, topo_clustering;
    uint32_t topo_communities, topo_edges, topo_nodes;
    uint32_t reserved;
} wired_gpu_sidecar_t;

static wired_gpu_state_t* wg_state(struct brain_struct* brain) {
    return brain ? (wired_gpu_state_t*)brain->wired[BRAIN_WIRED_GPU] : NULL;
}

static uint32_t env_u32(const char* key, uint32_t def, uint32_t lo, uint32_t hi) {
    const char* v = getenv(key);
    if (!v || !*v) return def;
    char* end = NULL;
    unsigned long x = strtoul(v, &end, 10);
    if (end == v) return def;
    if (x < lo) x = lo;
    if (x > hi) x = hi;
    return (uint32_t)x;
}

/*=============================================================================
 * GPU health -> health agent
 *===========================================================================*/
static void wg_health_poll(struct brain_struct* brain, wired_gpu_state_t* st) {
    if (!st->health) return;
    /* nimcp_health_agent.h cannot share a TU with nimcp_brain_internal.h (its
     * guarded forward typedefs clash), so the agent message is built inside the
     * gpu_health module. */
    gpu_health_metrics_t m;
    memset(&m, 0, sizeof(m));
    int rc = nimcp_gpu_health_report_to_agent(
        st->health, st->device_id,
        (brain->health_agent_enabled ? brain->health_agent : NULL),
        &st->last_reported_status, &m);
    if (rc < 0) return;
    st->last_status = m.status;
    st->last_free_mb = m.free_memory / (1024u * 1024u);
    st->last_total_mb = m.total_memory / (1024u * 1024u);
    if (rc > 0) st->health_reports++;
}

/*=============================================================================
 * Probe: prefetch ring -> INT8 calibration/quantization -> D2H clip rate
 *===========================================================================*/
static void* wg_ring_loader(int batch_idx, int tensor_idx, size_t size, void* user) {
    wired_gpu_state_t* st = (wired_gpu_state_t*)user;
    (void)tensor_idx;
    if (!st || batch_idx < 0 || (uint32_t)batch_idx >= st->ring_count) return NULL;
    if (size > (size_t)st->dim * sizeof(float)) return NULL;
    /* oldest -> newest; the ring is not written while a probe runs (same thread) */
    uint32_t slot = (st->ring_head + WG_RING - st->ring_count + (uint32_t)batch_idx) % WG_RING;
    return st->ring + (size_t)slot * WG_MAX_DIM;
}

static bool wg_ensure_probe_resources(wired_gpu_state_t* st) {
    if (!st->xfer) {
        /* pinned: codes readback + one staging buffer for the prefetcher */
        size_t pinned = (size_t)WG_MAX_DIM * sizeof(float) + WG_MAX_DIM + 4096;
        st->xfer = nimcp_transfer_manager_create_shared(st->ctx, pinned);
        if (!st->xfer) return false;
    }
    if (st->prefetch && st->prefetch_dim != st->dim) {
        nimcp_prefetch_destroy(st->prefetch);
        st->prefetch = NULL;
    }
    if (!st->prefetch) {
        size_t bytes = (size_t)st->dim * sizeof(float);
        st->prefetch = nimcp_prefetch_create_ex(st->ctx, &bytes, 1, 2, st->xfer);
        if (!st->prefetch) return false;
        st->prefetch_dim = st->dim;
    }
    if (!st->calib) {
        st->calib = nimcp_int8_calibrator_create(st->ctx, INT8_CALIB_MINMAX, false, 0, 0);
        if (!st->calib) return false;
    }
    if (!st->h_codes || st->h_codes_cap < st->dim) {
        if (st->h_codes) nimcp_transfer_free_pinned(st->xfer, st->h_codes);
        st->h_codes = (int8_t*)nimcp_transfer_alloc_pinned(st->xfer, WG_MAX_DIM);
        st->h_codes_cap = st->h_codes ? WG_MAX_DIM : 0;
        if (!st->h_codes) return false;
    }
    return true;
}

static bool wg_probe_int8(wired_gpu_state_t* st) {
    if (!wg_ensure_probe_resources(st)) return false;

    nimcp_int8_calibrator_reset(st->calib);
    nimcp_prefetch_reset(st->prefetch);
    int nbatches = (int)st->ring_count;
    if (nimcp_prefetch_start_with_count(st->prefetch, wg_ring_loader, st, nbatches) != 0) {
        return false;
    }

    bool ok = true;
    int seen = 0;
    void** last = NULL;
    for (int b = 0; b < nbatches; b++) {
        void** batch = nimcp_prefetch_get_batch_timeout(st->prefetch, 2000);
        if (!batch || !batch[0]) { ok = false; break; }
        if (nimcp_int8_calibrator_observe(st->calib, (const float*)batch[0], st->dim) != 0) {
            ok = false;
            nimcp_prefetch_release_batch(st->prefetch);
            break;
        }
        seen++;
        if (b == nbatches - 1) { last = batch; break; }   /* keep newest for quantization */
        /* observe() launched on ctx->compute_stream: finish before the loader may
         * overwrite this double-buffer slot */
        nimcp_gpu_stream_synchronize(st->ctx);
        nimcp_prefetch_release_batch(st->prefetch);
    }

    if (ok && last && seen > 0) {
        nimcp_int8_quant_params_t qp;
        memset(&qp, 0, sizeof(qp));
        nimcp_int8_params_init(&qp);
        int8_t* d_codes = NULL;
        if (nimcp_int8_calibrator_compute_params(st->calib, &qp) == 0 &&
            isfinite(qp.scale) && qp.scale > 0.0f &&
            (d_codes = (int8_t*)nimcp_gpu_pool_alloc(st->scratch, st->dim, 256, NULL)) != NULL &&
            nimcp_int8_quantize(st->ctx, (const float*)last[0], d_codes, st->dim, &qp) == 0) {

            float sqnr = nimcp_int8_compute_sqnr(st->ctx, (const float*)last[0], d_codes,
                                                 st->dim, &qp);   /* syncs compute stream */
            uint64_t id = nimcp_transfer_async(st->xfer, st->h_codes, d_codes, st->dim,
                                               NIMCP_TRANSFER_D2H, NULL, NULL);
            float clip = -1.0f;
            if (id != NIMCP_TRANSFER_INVALID_ID) {
                nimcp_transfer_wait(st->xfer, id);
                uint32_t sat = 0;
                for (uint32_t i = 0; i < st->dim; i++) {
                    if (st->h_codes[i] == 127 || st->h_codes[i] == -128) sat++;
                }
                clip = (float)sat / (float)st->dim;
            }

            st->q_scale = qp.scale;
            st->q_zero_point = qp.zero_point;
            if (isfinite(qp.min_val)) st->q_min = qp.min_val;
            if (isfinite(qp.max_val)) st->q_max = qp.max_val;
            if (isfinite(sqnr)) {
                st->sqnr_db = sqnr;
                st->sqnr_ema_db = (st->probes == 0) ? sqnr : 0.9f * st->sqnr_ema_db + 0.1f * sqnr;
            }
            if (clip >= 0.0f) st->clip_rate = clip;
        } else {
            ok = false;
        }
        nimcp_gpu_pool_reset(st->scratch);
        nimcp_prefetch_release_batch(st->prefetch);
    }

    nimcp_prefetch_stop(st->prefetch);

    nimcp_prefetch_stats_t ps;
    memset(&ps, 0, sizeof(ps));
    nimcp_prefetch_get_stats(st->prefetch, &ps);
    st->pf_batches = ps.batches_loaded;
    st->pf_stalls = ps.stalls;
    st->xfer_bytes = st->xfer->total_bytes_transferred;
    return ok;
}

/*=============================================================================
 * Probe: input-channel co-activation graph -> GPU Louvain + clustering
 *===========================================================================*/
static bool wg_probe_topology(wired_gpu_state_t* st) {
    uint32_t K = st->ring_count;
    uint32_t G = st->dim < WG_TOPO_NODES ? st->dim : WG_TOPO_NODES;
    if (K < 8 || G < 4) return true;   /* not enough data yet; not a failure */

    float* x = (float*)nimcp_calloc((size_t)K * G, sizeof(float));   /* group means */
    int* row_ptrs = (int*)nimcp_calloc(G + 1, sizeof(int));
    int* cols = (int*)nimcp_malloc((size_t)G * G * sizeof(int));
    float* w = (float*)nimcp_malloc((size_t)G * G * sizeof(float));
    float* cc = (float*)nimcp_calloc(G, sizeof(float));
    bool ok = false;
    nimcp_graph_gpu_t* graph = NULL;
    nimcp_community_result_gpu_t* comm = NULL;
    nimcp_gpu_tensor_t* clus = NULL;
    if (!x || !row_ptrs || !cols || !w || !cc) goto out;

    uint32_t span = st->dim / G;
    for (uint32_t k = 0; k < K; k++) {
        uint32_t slot = (st->ring_head + WG_RING - K + k) % WG_RING;
        const float* v = st->ring + (size_t)slot * WG_MAX_DIM;
        for (uint32_t g = 0; g < G; g++) {
            double s = 0.0;
            for (uint32_t j = 0; j < span; j++) s += v[g * span + j];
            x[k * G + g] = (float)(s / span);
        }
    }
    /* z-score each channel over the K samples */
    for (uint32_t g = 0; g < G; g++) {
        double mu = 0.0, var = 0.0;
        for (uint32_t k = 0; k < K; k++) mu += x[k * G + g];
        mu /= K;
        for (uint32_t k = 0; k < K; k++) { double d = x[k * G + g] - mu; var += d * d; }
        double sd = sqrt(var / K);
        for (uint32_t k = 0; k < K; k++)
            x[k * G + g] = (sd > 1e-8) ? (float)((x[k * G + g] - mu) / sd) : 0.0f;
    }
    /* symmetric CSR, columns ascending (triangle counting needs sorted rows) */
    int nnz = 0;
    for (uint32_t a = 0; a < G; a++) {
        row_ptrs[a] = nnz;
        for (uint32_t b = 0; b < G; b++) {
            if (a == b) continue;
            double r = 0.0;
            for (uint32_t k = 0; k < K; k++) r += (double)x[k * G + a] * x[k * G + b];
            r /= K;
            if (isfinite(r) && fabs(r) >= WG_TOPO_EDGE_R) {
                cols[nnz] = (int)b;
                w[nnz] = (float)fabs(r);
                nnz++;
            }
        }
    }
    row_ptrs[G] = nnz;
    st->topo_nodes = G;
    st->topo_edges = (uint32_t)(nnz / 2);
    if (nnz == 0) {                       /* fully decorrelated inputs: valid result */
        st->topo_modularity = 0.0f;
        st->topo_clustering = 0.0f;
        st->topo_communities = G;
        ok = true;
        goto out;
    }

    graph = nimcp_graph_gpu_from_csr(st->ctx, row_ptrs, cols, w, (int)G, nnz);
    if (!graph) goto out;
    comm = nimcp_community_detect_louvain(st->ctx, graph, 1.0f, 20, 1e-5f);
    if (!comm) goto out;
    size_t dims[1] = { G };
    clus = nimcp_gpu_tensor_create(st->ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!clus || !nimcp_topology_compute_clustering(st->ctx, graph, clus) ||
        !nimcp_gpu_tensor_to_host(clus, cc)) goto out;

    double mean_cc = 0.0;
    for (uint32_t g = 0; g < G; g++) if (isfinite(cc[g])) mean_cc += cc[g];
    mean_cc /= G;
    if (isfinite(comm->modularity)) st->topo_modularity = comm->modularity;
    if (isfinite(mean_cc)) st->topo_clustering = (float)mean_cc;
    st->topo_communities = (uint32_t)(comm->num_communities > 0 ? comm->num_communities : 0);
    ok = true;

out:
    if (clus) nimcp_gpu_tensor_destroy(clus);
    if (comm) nimcp_community_result_gpu_destroy(comm);
    if (graph) nimcp_graph_gpu_destroy(graph);
    nimcp_free(x); nimcp_free(row_ptrs); nimcp_free(cols); nimcp_free(w); nimcp_free(cc);
    return ok;
}

/*=============================================================================
 * TensorRT (on demand, once)
 *===========================================================================*/
static void wg_trt_estimate_once(wired_gpu_state_t* st) {
    if (st->trt_estimated || st->dim == 0) return;
    st->trt_estimated = true;
    if (!st->trt_available) return;            /* stub build: nothing to estimate */

    nimcp_trt_config_t cfg;
    if (nimcp_trt_default_config(&cfg) != 0) return;
    cfg.max_workspace_size = 16ULL << 20;      /* estimate only; never built */
    /* One dense INT8 layer: input width -> WG_TOPO_NODES, using the calibrated
     * input quantization. Weights are placeholders (estimate is shape-based). */
    nimcp_trt_network_def_t* net = nimcp_trt_network_create("wired_gpu_input_probe", 1);
    if (!net) return;
    float* wts = (float*)nimcp_calloc((size_t)st->dim * WG_TOPO_NODES, sizeof(float));
    nimcp_int8_quant_params_t qp;
    memset(&qp, 0, sizeof(qp));
    nimcp_int8_params_init(&qp);
    if (st->q_scale > 0.0f) {
        qp.scale = st->q_scale; qp.zero_point = st->q_zero_point;
        qp.min_val = st->q_min; qp.max_val = st->q_max;
    }
    int in_dims[1] = { (int)st->dim };
    float lat = 0.0f, thr = 0.0f;
    size_t mem = 0;
    if (wts &&
        nimcp_trt_network_add_input(net, "input", in_dims, 1) == 0 &&
        nimcp_trt_network_add_dense(net, 0, "probe_dense", (int)st->dim, (int)WG_TOPO_NODES,
                                    wts, NULL, TRT_ACTIVATION_RELU, &qp) == 0 &&
        nimcp_trt_estimate_performance(net, &cfg, &lat, &thr, &mem) == 0 &&
        isfinite(lat) && isfinite(thr)) {
        st->trt_latency_ms = lat;
        st->trt_throughput = thr;
        st->trt_memory_mb = mem;
    }
    nimcp_free(wts);
    nimcp_trt_network_destroy(net);
}

static void wg_run_probe(wired_gpu_state_t* st) {
    if (st->probing_disabled || st->ring_count < 2 || st->dim == 0 || !st->scratch) return;

    bool ok_q = wg_probe_int8(st);
    bool ok_t = wg_probe_topology(st);
    wg_trt_estimate_once(st);

    if (ok_q && ok_t) {
        st->probes++;
        st->consecutive_failures = 0;
    } else {
        st->probe_failures++;
        if (++st->consecutive_failures >= WG_MAX_FAILURES) {
            st->probing_disabled = true;
            fprintf(stderr, "[WIRED-GPU] probe failed %u times in a row (int8=%d topo=%d); "
                    "probing disabled for this run\n", st->consecutive_failures, ok_q, ok_t);
        }
    }
}

/*=============================================================================
 * Lifecycle
 *===========================================================================*/
int brain_wired_gpu_create(struct brain_struct* brain) {
    if (!brain) return -1;
    if (brain->wired[BRAIN_WIRED_GPU]) return 0;
    nimcp_gpu_context_t* ctx = brain->gpu_ctx;
    if (!brain->gpu_enabled || !ctx || !nimcp_gpu_context_is_valid(ctx)) {
        fprintf(stderr, "[WIRED-GPU] no GPU context on this brain; group inactive\n");
        return -1;
    }

    wired_gpu_state_t* st = (wired_gpu_state_t*)nimcp_calloc(1, sizeof(*st));
    if (!st) return -1;
    st->ctx = ctx;
    st->device_id = ctx->device_id;
    st->probe_every = env_u32("NIMCP_WIRED_GPU_PROBE_EVERY", WG_PROBE_EVERY_DEFAULT, 256, 1u << 24);
    st->pool_bytes = (size_t)env_u32("NIMCP_WIRED_GPU_POOL_KB", WG_POOL_KB_DEFAULT, 64, 16384) * 1024u;
    st->last_reported_status = GPU_HEALTH_EXCELLENT;
    st->last_status = GPU_HEALTH_EXCELLENT;

    st->ring = (float*)nimcp_calloc((size_t)WG_RING * WG_MAX_DIM, sizeof(float));
    if (!st->ring) { nimcp_free(st); return -1; }

    /* Small scratch pool on the existing context (no large arena). */
    nimcp_pool_factory_hints_t hints = nimcp_pool_factory_hints_default();
    hints.usage = NIMCP_POOL_USAGE_SCRATCH;
    hints.max_memory_bytes = st->pool_bytes;
    hints.enable_stats = true;
    st->scratch = nimcp_pool_factory_create(ctx, &hints);
    if (!st->scratch) {
        fprintf(stderr, "[WIRED-GPU] scratch pool (%zu KB) unavailable; probes disabled\n",
                st->pool_bytes / 1024u);
    }

    /* GPU health: probe only this brain's device, never reset it. */
    st->health = nimcp_gpu_health_init(WG_HEALTH_INTERVAL_MS);
    if (st->health) {
        nimcp_gpu_health_set_device_filter(st->health, st->device_id);
        if (!nimcp_gpu_health_start_monitoring(st->health)) {
            fprintf(stderr, "[WIRED-GPU] health monitor thread not started\n");
        }
    }

    st->tensor_cores = nimcp_int8_tensor_cores_available(ctx);
    st->trt_available = nimcp_trt_available();

    brain->wired[BRAIN_WIRED_GPU] = st;
    fprintf(stderr, "[WIRED-GPU] device %d: pool=%zuKB probe_every=%u health=%s trt=%s\n",
            st->device_id, st->scratch ? st->pool_bytes / 1024u : 0u, st->probe_every,
            st->health ? "on" : "off", st->trt_available ? "yes" : "no");
    return 0;
}

void brain_wired_gpu_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms) {
    (void)dt_ms;
    wired_gpu_state_t* st = wg_state(brain);
    if (!st) return;
    st->ticks++;

    if (f && n > 0 && (st->ticks % WG_SAMPLE_EVERY) == 0) {
        uint32_t d = n < WG_MAX_DIM ? n : WG_MAX_DIM;
        if (st->dim != d) {                 /* input width changed: restart the ring */
            st->dim = d;
            st->ring_count = 0;
            st->ring_head = 0;
        }
        float* dst = st->ring + (size_t)st->ring_head * WG_MAX_DIM;
        for (uint32_t i = 0; i < d; i++) dst[i] = isfinite(f[i]) ? f[i] : 0.0f;
        st->ring_head = (st->ring_head + 1) % WG_RING;
        if (st->ring_count < WG_RING) st->ring_count++;
    }

    if ((st->ticks % WG_HEALTH_EVERY) == 0) {
        wg_health_poll(brain, st);
    }

    if ((st->ticks % st->probe_every) == 0) {
        wg_run_probe(st);
    }
}

int brain_wired_gpu_save(struct brain_struct* brain, const char* path) {
    wired_gpu_state_t* st = wg_state(brain);
    if (!st || !path) return 0;
    char side[4096];
    if (snprintf(side, sizeof(side), "%s.wired_gpu", path) >= (int)sizeof(side)) return -1;

    wired_gpu_sidecar_t s;
    memset(&s, 0, sizeof(s));
    s.magic = WG_SIDECAR_MAGIC;
    s.version = WG_SIDECAR_VERSION;
    s.probes = st->probes;
    s.probe_failures = st->probe_failures;
    s.q_scale = st->q_scale; s.q_min = st->q_min; s.q_max = st->q_max;
    s.q_zero_point = st->q_zero_point;
    s.sqnr_db = st->sqnr_db; s.sqnr_ema_db = st->sqnr_ema_db; s.clip_rate = st->clip_rate;
    s.topo_modularity = st->topo_modularity; s.topo_clustering = st->topo_clustering;
    s.topo_communities = st->topo_communities; s.topo_edges = st->topo_edges;
    s.topo_nodes = st->topo_nodes;

    FILE* fp = fopen(side, "wb");
    if (!fp) return -1;
    size_t wr = fwrite(&s, sizeof(s), 1, fp);
    int rc = fclose(fp);
    return (wr == 1 && rc == 0) ? 0 : -1;
}

int brain_wired_gpu_load(struct brain_struct* brain, const char* path) {
    wired_gpu_state_t* st = wg_state(brain);
    if (!st || !path) return 1;
    char side[4096];
    if (snprintf(side, sizeof(side), "%s.wired_gpu", path) >= (int)sizeof(side)) return -1;
    FILE* fp = fopen(side, "rb");
    if (!fp) return 1;                       /* no sidecar: fresh state */
    wired_gpu_sidecar_t s;
    size_t rd = fread(&s, sizeof(s), 1, fp);
    fclose(fp);
    if (rd != 1 || s.magic != WG_SIDECAR_MAGIC || s.version != WG_SIDECAR_VERSION) return -1;

#define WG_F(x) (isfinite(x) ? (x) : 0.0f)
    st->probes = s.probes;
    st->probe_failures = s.probe_failures;
    st->q_scale = WG_F(s.q_scale); st->q_min = WG_F(s.q_min); st->q_max = WG_F(s.q_max);
    st->q_zero_point = s.q_zero_point;
    st->sqnr_db = WG_F(s.sqnr_db); st->sqnr_ema_db = WG_F(s.sqnr_ema_db);
    st->clip_rate = WG_F(s.clip_rate);
    st->topo_modularity = WG_F(s.topo_modularity); st->topo_clustering = WG_F(s.topo_clustering);
    st->topo_communities = s.topo_communities; st->topo_edges = s.topo_edges;
    st->topo_nodes = s.topo_nodes;
#undef WG_F
    return 0;
}

void brain_wired_gpu_destroy(struct brain_struct* brain) {
    wired_gpu_state_t* st = wg_state(brain);
    if (!brain) return;
    brain->wired[BRAIN_WIRED_GPU] = NULL;
    if (!st) return;

    if (st->health) {
        nimcp_gpu_health_stop_monitoring(st->health);
        nimcp_gpu_health_destroy(st->health);
    }
    if (st->prefetch) nimcp_prefetch_destroy(st->prefetch);   /* stops loader thread */
    if (st->calib) nimcp_int8_calibrator_destroy(st->calib);
    if (st->xfer) {
        if (st->h_codes) nimcp_transfer_free_pinned(st->xfer, st->h_codes);
        nimcp_transfer_manager_destroy(st->xfer);              /* borrowed stream kept */
    }
    if (st->scratch) nimcp_gpu_pool_destroy(st->scratch);
    nimcp_free(st->ring);
    nimcp_free(st);
}

int brain_wired_gpu_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    wired_gpu_state_t* st = wg_state(brain);
    if (!buf || cap == 0) return 0;
    if (!st) return snprintf(buf, cap, "null");

    nimcp_gpu_pool_stats_t ps;
    memset(&ps, 0, sizeof(ps));
    if (st->scratch) nimcp_gpu_pool_get_stats(st->scratch, &ps);

    return snprintf(buf, cap,
        "{\"device\":%d,\"ticks\":%llu,"
        "\"health\":{\"status\":%d,\"free_mb\":%zu,\"total_mb\":%zu,\"reports\":%llu},"
        "\"pool\":{\"size\":%zu,\"peak_used\":%zu,\"allocs\":%llu,\"failed\":%llu},"
        "\"int8\":{\"probes\":%llu,\"failures\":%llu,\"disabled\":%s,\"dim\":%u,"
        "\"scale\":%.6g,\"zero_point\":%d,\"min\":%.6g,\"max\":%.6g,"
        "\"sqnr_db\":%.3f,\"sqnr_ema_db\":%.3f,\"clip_rate\":%.4f,\"tensor_cores\":%s},"
        "\"prefetch\":{\"batches\":%llu,\"stalls\":%llu,\"bytes\":%llu},"
        "\"topology\":{\"nodes\":%u,\"edges\":%u,\"communities\":%u,"
        "\"modularity\":%.4f,\"clustering\":%.4f},"
        "\"tensorrt\":{\"available\":%s,\"version\":\"%s\",\"est_latency_ms\":%.3f,"
        "\"est_throughput\":%.1f,\"est_memory_mb\":%zu}}",
        st->device_id, (unsigned long long)st->ticks,
        (int)st->last_status, st->last_free_mb, st->last_total_mb,
        (unsigned long long)st->health_reports,
        ps.total_size, ps.peak_used, (unsigned long long)ps.allocation_count,
        (unsigned long long)ps.failed_allocations,
        (unsigned long long)st->probes, (unsigned long long)st->probe_failures,
        st->probing_disabled ? "true" : "false", st->dim,
        st->q_scale, st->q_zero_point, st->q_min, st->q_max,
        st->sqnr_db, st->sqnr_ema_db, st->clip_rate, st->tensor_cores ? "true" : "false",
        (unsigned long long)st->pf_batches, (unsigned long long)st->pf_stalls,
        (unsigned long long)st->xfer_bytes,
        st->topo_nodes, st->topo_edges, st->topo_communities,
        st->topo_modularity, st->topo_clustering,
        st->trt_available ? "true" : "false", nimcp_trt_version(),
        st->trt_latency_ms, st->trt_throughput, st->trt_memory_mb);
}

#else /* !NIMCP_ENABLE_CUDA — CUDA sources are not built; the group is inert. */

int brain_wired_gpu_create(struct brain_struct* brain) { (void)brain; return -1; }
void brain_wired_gpu_tick(struct brain_struct* brain, const float* f, uint32_t n, float dt_ms) {
    (void)brain; (void)f; (void)n; (void)dt_ms;
}
int brain_wired_gpu_save(struct brain_struct* brain, const char* path) { (void)brain; (void)path; return 0; }
int brain_wired_gpu_load(struct brain_struct* brain, const char* path) { (void)brain; (void)path; return 1; }
void brain_wired_gpu_destroy(struct brain_struct* brain) { if (brain) brain->wired[BRAIN_WIRED_GPU] = NULL; }
int brain_wired_gpu_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    (void)brain;
    if (!buf || cap == 0) return 0;
    return snprintf(buf, cap, "{\"cuda\":false}");
}

#endif /* NIMCP_ENABLE_CUDA */
