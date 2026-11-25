/**
 * @file nimcp_brain_wired.c
 * @brief Dispatcher for the wired-module groups. See nimcp_brain_wired.h.
 */
#include "core/brain/nimcp_brain_wired.h"
#include "core/brain/nimcp_brain_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;      /* env suffix: NIMCP_WIRED_<NAME>=0 disables */
    int  (*create)(struct brain_struct*);
    void (*tick)(struct brain_struct*, const float*, uint32_t, float);
    int  (*save)(struct brain_struct*, const char*);
    int  (*load)(struct brain_struct*, const char*);
    void (*destroy)(struct brain_struct*);
    int  (*status)(struct brain_struct*, char*, uint32_t);
} wired_group_ops_t;

#define OPS(g, NAME) { NAME, brain_wired_##g##_create, brain_wired_##g##_tick, brain_wired_##g##_save, \
                       brain_wired_##g##_load, brain_wired_##g##_destroy, brain_wired_##g##_status_json }

static const wired_group_ops_t k_groups[BRAIN_WIRED_COUNT] = {
    [BRAIN_WIRED_SLEEP]        = OPS(sleep, "SLEEP"),
    [BRAIN_WIRED_BIOASYNC]     = OPS(bioasync, "BIOASYNC"),
    [BRAIN_WIRED_LANGUAGE]     = OPS(language, "LANGUAGE"),
    [BRAIN_WIRED_NOISE_IMMUNE] = OPS(noise_immune, "NOISE_IMMUNE"),
    [BRAIN_WIRED_GPU]          = OPS(gpu, "GPU"),
    [BRAIN_WIRED_COGNITIVE]    = OPS(cognitive, "COGNITIVE"),
    [BRAIN_WIRED_COMPUTE]      = OPS(compute, "COMPUTE"),
};

static int group_enabled(int g) {
    char key[64];
    snprintf(key, sizeof(key), "NIMCP_WIRED_%s", k_groups[g].name);
    const char* v = getenv(key);
    if (v && v[0] == '0') return 0;
    v = getenv("NIMCP_WIRED");             /* master switch */
    if (v && v[0] == '0') return 0;
    return 1;
}

int brain_wired_create_all(struct brain_struct* brain) {
    if (!brain) return 0;
    int created = 0;
    for (int g = 0; g < BRAIN_WIRED_COUNT; g++) {
        if (brain->wired[g]) { created++; continue; }
        if (!group_enabled(g)) {
            fprintf(stderr, "[WIRED] %s disabled by env\n", k_groups[g].name);
            continue;
        }
        if (k_groups[g].create(brain) == 0 && brain->wired[g]) {
            created++;
            fprintf(stderr, "[WIRED] %s created\n", k_groups[g].name);
        } else {
            fprintf(stderr, "[WIRED] %s create failed (group inactive)\n", k_groups[g].name);
        }
    }
    return created;
}

void brain_wired_tick_all(struct brain_struct* brain, const float* features, uint32_t n, float dt_ms) {
    if (!brain) return;
    for (int g = 0; g < BRAIN_WIRED_COUNT; g++)
        if (brain->wired[g]) k_groups[g].tick(brain, features, n, dt_ms);
}

int brain_wired_save_all(struct brain_struct* brain, const char* path) {
    if (!brain || !path) return -1;
    int failures = 0;
    for (int g = 0; g < BRAIN_WIRED_COUNT; g++) {
        if (!brain->wired[g]) continue;
        if (k_groups[g].save(brain, path) < 0) {
            failures++;
            fprintf(stderr, "[WARN] wired %s save failed for %s\n", k_groups[g].name, path);
        }
    }
    return failures ? -1 : 0;
}

int brain_wired_load_all(struct brain_struct* brain, const char* path) {
    if (!brain || !path) return -1;
    int failures = 0;
    for (int g = 0; g < BRAIN_WIRED_COUNT; g++) {
        if (!brain->wired[g]) continue;
        if (k_groups[g].load(brain, path) < 0) {
            failures++;
            fprintf(stderr, "[WARN] wired %s load failed for %s (fresh state)\n", k_groups[g].name, path);
        }
    }
    return failures ? -1 : 0;
}

void brain_wired_destroy_all(struct brain_struct* brain) {
    if (!brain) return;
    for (int g = BRAIN_WIRED_COUNT - 1; g >= 0; g--)
        if (brain->wired[g]) k_groups[g].destroy(brain);
}

int brain_wired_status_json(struct brain_struct* brain, char* buf, uint32_t cap) {
    if (!buf || cap < 3) return 0;
    uint32_t off = 0;
    buf[off++] = '{';
    for (int g = 0; g < BRAIN_WIRED_COUNT && off + 2 < cap; g++) {
        int w = snprintf(buf + off, cap - off, "%s\"%s\":", g ? "," : "", k_groups[g].name);
        if (w < 0 || (uint32_t)w >= cap - off) break;
        off += (uint32_t)w;
        if (brain && brain->wired[g]) {
            w = k_groups[g].status(brain, buf + off, cap - off);
        } else {
            w = snprintf(buf + off, cap - off, "null");
        }
        if (w < 0 || (uint32_t)w >= cap - off) break;
        off += (uint32_t)w;
    }
    if (off + 2 > cap) off = cap - 2;
    buf[off++] = '}';
    buf[off] = '\0';
    return (int)off;
}
