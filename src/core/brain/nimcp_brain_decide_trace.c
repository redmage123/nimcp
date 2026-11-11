/**
 * @file nimcp_brain_decide_trace.c
 * @brief Opt-in stage-by-stage output-vector trace (see header).
 */
#include "core/brain/nimcp_brain_decide_trace.h"

#include "plasticity/adaptive/nimcp_adaptive.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DECIDE_TRACE_MAGIC 0x54524344u

static pthread_once_t s_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE* s_fp = NULL;
static uint64_t s_seq = 0;

static void trace_open(void)
{
    const char* path = getenv("NIMCP_DECIDE_TRACE");
    if (path && path[0]) {
        s_fp = fopen(path, "ab");
    }
}

bool nimcp_decide_trace_enabled(void)
{
    pthread_once(&s_once, trace_open);
    return s_fp != NULL;
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
