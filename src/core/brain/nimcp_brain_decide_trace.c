/**
 * @file nimcp_brain_decide_trace.c
 * @brief Opt-in stage-by-stage output-vector trace (see header).
 */
#include "core/brain/nimcp_brain_decide_trace.h"

#include "plasticity/adaptive/nimcp_adaptive.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define DECIDE_TRACE_MAGIC 0x54524344u

static pthread_once_t s_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE* s_fp = NULL;
static uint64_t s_seq = 0;
/* NaN-only mode (NIMCP_DECIDE_TRACE_NAN=1): nothing is written to a file;
 * each record is scanned and only non-finite values are reported, on stderr.
 * Cheap enough to leave on in production to localise where a NaN first
 * appears in the decide/learn pipeline. */
static bool s_nan_mode = false;
static atomic_uint_fast64_t s_nan_reports = 0;
#define NAN_TRIP_MAX_REPORTS 200u

static void trace_open(void)
{
    const char* path = getenv("NIMCP_DECIDE_TRACE");
    if (path && path[0]) {
        s_fp = fopen(path, "ab");
    }
    const char* nan_mode = getenv("NIMCP_DECIDE_TRACE_NAN");
    s_nan_mode = (s_fp == NULL && nan_mode && nan_mode[0] == '1');
}

bool nimcp_decide_trace_enabled(void)
{
    pthread_once(&s_once, trace_open);
    return s_fp != NULL || s_nan_mode;
}

static void nan_trip_check(const char* stage, const float* v, uint32_t n)
{
    uint32_t bad = 0, first = 0;
    float first_val = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (!isfinite(v[i])) {
            if (bad == 0) { first = i; first_val = v[i]; }
            bad++;
        }
    }
    if (bad == 0) {
        return;
    }
    uint64_t k = atomic_fetch_add(&s_nan_reports, 1);
    /* First NAN_TRIP_MAX_REPORTS reports, then every 1000th. */
    if (k >= NAN_TRIP_MAX_REPORTS && (k % 1000) != 0) {
        return;
    }
    pthread_mutex_lock(&s_lock);
    uint64_t seq = s_seq;
    pthread_mutex_unlock(&s_lock);
    char ts[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(stderr, "[NAN-TRIP] %s report=%llu seq=%llu stage=%s nonfinite=%u/%u first_idx=%u first_val=%f\n",
            ts, (unsigned long long)k, (unsigned long long)seq, stage, bad, n, first,
            (double)first_val);
}

void nimcp_decide_trace_begin(void)
{
    if (!nimcp_decide_trace_enabled()) {
        return;
    }
    pthread_mutex_lock(&s_lock);
    s_seq++;
    pthread_mutex_unlock(&s_lock);
}

void nimcp_decide_trace(const char* stage, const float* v, uint32_t n)
{
    if (!nimcp_decide_trace_enabled() || !v || !stage) {
        return;
    }
    if (!s_fp) {
        nan_trip_check(stage, v, n);
        return;
    }
    uint32_t magic = DECIDE_TRACE_MAGIC;
    char name[32] = {0};
    strncpy(name, stage, sizeof(name) - 1);
    pthread_mutex_lock(&s_lock);
    fwrite(&magic, sizeof(magic), 1, s_fp);
    fwrite(&s_seq, sizeof(s_seq), 1, s_fp);
    fwrite(name, sizeof(name), 1, s_fp);
    fwrite(&n, sizeof(n), 1, s_fp);
    fwrite(v, sizeof(float), n, s_fp);
    fflush(s_fp);
    pthread_mutex_unlock(&s_lock);
}

void nimcp_decide_trace_ann(const char* stage, void* adaptive_net, const float* x,
                            uint32_t n, uint32_t out_n)
{
    if (!nimcp_decide_trace_enabled() || !adaptive_net || !x || out_n == 0) {
        return;
    }
    float* out = calloc(out_n, sizeof(float));
    if (!out) {
        return;
    }
    adaptive_network_forward_readonly((adaptive_network_t)adaptive_net, x, n, out, out_n, 0);
    nimcp_decide_trace(stage, out, out_n);
    free(out);
}
