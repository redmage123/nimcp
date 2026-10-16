/**
 * @file nimcp_brain_kg_sidecar.c
 * @brief Minimal file sidecar for brain->internal_kg
 * @date 2026-05-04
 *
 * Format (little-endian, packed):
 *
 *   magic[8]   = "NIMCP_KG"
 *   version    = uint32_t (currently 1)
 *   reserved   = uint32_t (zero)
 *   node_count = uint32_t
 *   edge_count = uint32_t
 *
 *   For each node:
 *     name_len    : uint16_t
 *     name        : char[name_len]   (no NUL)
 *     desc_len    : uint16_t
 *     desc        : char[desc_len]
 *     type        : uint32_t  (brain_kg_node_type_t cast)
 *     state       : uint32_t  (brain_kg_node_state_t cast)
 *     enabled     : uint8_t
 *     saved_id    : uint32_t  (caller's old ID — used for edge remap)
 *
 *   For each edge:
 *     saved_from  : uint32_t  (matches saved_id)
 *     saved_to    : uint32_t
 *     type        : uint32_t  (brain_kg_edge_type_t)
 *     desc_len    : uint16_t
 *     desc        : char[desc_len]
 *     weight      : float
 *     bidir       : uint8_t
 *
 * The file is small (a few hundred KB at full population). Single fwrite
 * per array isn't possible because we serialize per-node — but each
 * node/edge fits in one I/O at the buffered-IO layer, which is good
 * enough for sidecar volumes.
 */

#include "core/brain/persistence/nimcp_brain_kg_sidecar.h"
#include "core/brain/nimcp_brain_kg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char KG_SIDECAR_MAGIC[8] = {
    'N', 'I', 'M', 'C', 'P', '_', 'K', 'G'
};
#define KG_SIDECAR_VERSION 1u

/* ---------------------------------------------------------------- helpers */

static int write_u8(FILE* f, uint8_t v) {
    return fwrite(&v, 1, 1, f) == 1 ? 0 : -1;
}
static int write_u16(FILE* f, uint16_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int write_u32(FILE* f, uint32_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int write_f32(FILE* f, float v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}
static int write_bytes(FILE* f, const void* buf, size_t n) {
    if (n == 0) return 0;
    return fwrite(buf, 1, n, f) == n ? 0 : -1;
}
static int write_str(FILE* f, const char* s, size_t maxlen) {
    size_t n = s ? strnlen(s, maxlen) : 0;
    if (n > UINT16_MAX) n = UINT16_MAX;
    if (write_u16(f, (uint16_t)n) != 0) return -1;
    return write_bytes(f, s, n);
}

static int read_u8(FILE* f, uint8_t* v) {
    return fread(v, 1, 1, f) == 1 ? 0 : -1;
}
static int read_u16(FILE* f, uint16_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int read_u32(FILE* f, uint32_t* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int read_f32(FILE* f, float* v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}
static int read_str(FILE* f, char* out, size_t out_size) {
    uint16_t n = 0;
    if (read_u16(f, &n) != 0) return -1;
    if ((size_t)n >= out_size) {
        /* Truncate gracefully — read all bytes then null-terminate. */
        size_t to_keep = out_size - 1;
        if (fread(out, 1, to_keep, f) != to_keep) return -1;
        if (fseek(f, (long)(n - to_keep), SEEK_CUR) != 0) return -1;
        out[to_keep] = '\0';
        return 0;
    }
    if (n > 0 && fread(out, 1, n, f) != n) return -1;
    out[n] = '\0';
    return 0;
}

/* ----------------------------------------------------------------- save  */

int brain_kg_save(struct brain_kg* kg, const char* filepath) {
    if (!kg || !filepath) return -1;

    /* Snapshot via the public API — it handles locking + skips empty slots. */
    brain_kg_node_list_t* nodes = brain_kg_get_all_nodes(kg);
    if (!nodes) return -1;

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        brain_kg_node_list_destroy(nodes);
        return -1;
    }

    /* Header. */
    if (write_bytes(f, KG_SIDECAR_MAGIC, sizeof(KG_SIDECAR_MAGIC)) != 0) goto fail;
    if (write_u32(f, KG_SIDECAR_VERSION) != 0) goto fail;
    if (write_u32(f, 0u) != 0) goto fail;  /* reserved */

    uint32_t node_count = nodes->count;
    if (write_u32(f, node_count) != 0) goto fail;

    /* Reserve slot for edge_count; we'll backfill once we know it. */
    long edge_count_pos = ftell(f);
    if (edge_count_pos < 0) goto fail;
    if (write_u32(f, 0u) != 0) goto fail;

    /* Nodes. */
    for (uint32_t i = 0; i < node_count; i++) {
        const brain_kg_node_t* n = nodes->nodes[i];
        if (!n) continue;
        if (write_str(f, n->name, sizeof(n->name)) != 0) goto fail;
        if (write_str(f, n->description, sizeof(n->description)) != 0) goto fail;
        if (write_u32(f, (uint32_t)n->type) != 0) goto fail;
        if (write_u32(f, (uint32_t)n->state) != 0) goto fail;
        if (write_u8(f, n->enabled ? 1u : 0u) != 0) goto fail;
        if (write_u32(f, (uint32_t)n->id) != 0) goto fail;
    }

    /* Edges — gather per source-node by iterating outgoing lists.
     * Avoids needing a public "get all edges" API. */
    uint32_t edge_count = 0;
    for (uint32_t i = 0; i < node_count; i++) {
        const brain_kg_node_t* src = nodes->nodes[i];
        if (!src) continue;
        brain_kg_edge_list_t* outs = brain_kg_get_outgoing(kg, src->id);
        if (!outs) continue;
        for (uint32_t j = 0; j < outs->count; j++) {
            const brain_kg_edge_t* e = outs->edges[j];
            if (!e) continue;
            if (write_u32(f, (uint32_t)e->from) != 0) {
                brain_kg_edge_list_destroy(outs);
                goto fail;
            }
            if (write_u32(f, (uint32_t)e->to) != 0) goto fail_with_outs;
            if (write_u32(f, (uint32_t)e->type) != 0) goto fail_with_outs;
            if (write_str(f, e->description, sizeof(e->description)) != 0) goto fail_with_outs;
            if (write_f32(f, e->weight) != 0) goto fail_with_outs;
            if (write_u8(f, e->bidirectional ? 1u : 0u) != 0) goto fail_with_outs;
            edge_count++;
            continue;
        fail_with_outs:
            brain_kg_edge_list_destroy(outs);
            goto fail;
        }
        brain_kg_edge_list_destroy(outs);
    }

    /* Backfill edge_count. */
    long after_pos = ftell(f);
    if (after_pos < 0) goto fail;
    if (fseek(f, edge_count_pos, SEEK_SET) != 0) goto fail;
    if (write_u32(f, edge_count) != 0) goto fail;
    if (fseek(f, after_pos, SEEK_SET) != 0) goto fail;

    fclose(f);
    brain_kg_node_list_destroy(nodes);
    return 0;

fail:
    fclose(f);
    brain_kg_node_list_destroy(nodes);
    /* Best effort: leave the half-written file in place — caller's `[WARN]`
     * print will surface the failure. */
    return -1;
}

/* ----------------------------------------------------------------- load  */

int brain_kg_load(struct brain_kg* kg, const char* filepath) {
    if (!kg || !filepath) return -1;

    FILE* f = fopen(filepath, "rb");
    if (!f) return -1;

    char magic[8];
    if (fread(magic, 1, sizeof(magic), f) != sizeof(magic)
        || memcmp(magic, KG_SIDECAR_MAGIC, sizeof(magic)) != 0) {
        fclose(f);
        return -1;
    }
    uint32_t version = 0, reserved = 0;
    if (read_u32(f, &version) != 0 || read_u32(f, &reserved) != 0) {
        fclose(f);
        return -1;
    }
    if (version != KG_SIDECAR_VERSION) {
        fclose(f);
        return -1;
    }
    uint32_t node_count = 0, edge_count = 0;
    if (read_u32(f, &node_count) != 0 || read_u32(f, &edge_count) != 0) {
        fclose(f);
        return -1;
    }

    /* ID remap table: saved_id -> new_id assigned by brain_kg_add_node().
     * Sized for the saved set, not the global cap, so it stays small. */
    uint32_t* saved_ids = (uint32_t*)calloc(node_count ? node_count : 1,
                                             sizeof(uint32_t));
    uint32_t* new_ids = (uint32_t*)calloc(node_count ? node_count : 1,
                                           sizeof(uint32_t));
    if (!saved_ids || !new_ids) {
        free(saved_ids); free(new_ids);
        fclose(f);
        return -1;
    }

    char name_buf[BRAIN_KG_MAX_NAME_LEN];
    char desc_buf[BRAIN_KG_MAX_DESC_LEN];

    for (uint32_t i = 0; i < node_count; i++) {
        if (read_str(f, name_buf, sizeof(name_buf)) != 0) goto fail;
        if (read_str(f, desc_buf, sizeof(desc_buf)) != 0) goto fail;
        uint32_t type_u = 0, state_u = 0;
        uint8_t enabled = 0;
        uint32_t saved_id = 0;
        if (read_u32(f, &type_u) != 0
            || read_u32(f, &state_u) != 0
            || read_u8(f, &enabled) != 0
            || read_u32(f, &saved_id) != 0) {
            goto fail;
        }

        /* If a node with the same name already exists, reuse its ID rather
         * than creating a duplicate. The KG add_node implementation may or
         * may not reject duplicates; checking here is cheap and explicit. */
        brain_kg_node_id_t existing = brain_kg_find_node(kg, name_buf);
        brain_kg_node_id_t new_id = BRAIN_KG_INVALID_NODE;
        if (existing != BRAIN_KG_INVALID_NODE) {
            new_id = existing;
            /* Update description/state best-effort — ignore failure. */
            (void)brain_kg_update_node(kg, existing, desc_buf,
                                       (brain_kg_node_state_t)state_u);
        } else {
            new_id = brain_kg_add_node(kg, name_buf,
                                       (brain_kg_node_type_t)type_u, desc_buf);
            if (new_id != BRAIN_KG_INVALID_NODE) {
                (void)brain_kg_update_node(kg, new_id, NULL,
                                           (brain_kg_node_state_t)state_u);
            }
        }
        saved_ids[i] = saved_id;
        new_ids[i] = new_id;
    }

    /* Edges. */
    for (uint32_t i = 0; i < edge_count; i++) {
        uint32_t saved_from = 0, saved_to = 0, type_u = 0;
        float weight = 0.0f;
        uint8_t bidir = 0;
        if (read_u32(f, &saved_from) != 0
            || read_u32(f, &saved_to) != 0
            || read_u32(f, &type_u) != 0) {
            goto fail;
        }
        if (read_str(f, desc_buf, sizeof(desc_buf)) != 0) goto fail;
        if (read_f32(f, &weight) != 0) goto fail;
        if (read_u8(f, &bidir) != 0) goto fail;

        /* Linear scan — node count is bounded (<= 16384) and load is
         * one-shot. A hash map isn't worth the dependency. */
        brain_kg_node_id_t new_from = BRAIN_KG_INVALID_NODE;
        brain_kg_node_id_t new_to = BRAIN_KG_INVALID_NODE;
        for (uint32_t k = 0; k < node_count; k++) {
            if (saved_ids[k] == saved_from) new_from = new_ids[k];
            if (saved_ids[k] == saved_to)   new_to   = new_ids[k];
        }
        if (new_from == BRAIN_KG_INVALID_NODE
            || new_to == BRAIN_KG_INVALID_NODE) {
            /* Either endpoint is missing — skip the edge. Possible if a
             * save-time node failed to insert on load (cap reached). */
            continue;
        }

        /* Skip if an edge with the same endpoints already exists; otherwise
         * we'd silently double-edge the graph. */
        if (brain_kg_find_edge(kg, new_from, new_to) != BRAIN_KG_INVALID_NODE) {
            continue;
        }

        (void)brain_kg_add_edge(kg, new_from, new_to,
                                (brain_kg_edge_type_t)type_u, desc_buf, weight);
        (void)bidir;  /* CRUD API doesn't expose bidirectional setter directly */
    }

    free(saved_ids);
    free(new_ids);
    fclose(f);
    return 0;

fail:
    free(saved_ids);
    free(new_ids);
    fclose(f);
    return -1;
}
