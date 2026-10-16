/* nimcp_part_alloc_stats.c — allocator accounting endpoint
 * Part of nimcp.c (SRP #include-based split)
 * DO NOT compile separately — #included from nimcp.c
 */

#include <malloc.h>     /* mallinfo2 */
#include <stdio.h>
#include <string.h>

#include "api/nimcp_alloc_stats.h"
#include "security/nimcp_audit_log.h"
#include "core/brain/nimcp_brain_kg.h"

/* Approximate per-entry size for KG nodes/edges. Used for an estimate; the
 * real layout is inside `struct brain_kg_node` / `brain_kg_edge` in
 * src/core/brain/nimcp_brain_kg.c. We assume ~256 bytes per node and ~64
 * bytes per edge based on the struct definitions there. */
#define NIMCP_KG_NODE_BYTES_HINT  256
#define NIMCP_KG_EDGE_BYTES_HINT   64

/* Audit ring capacity comes from src/security/nimcp_audit_log.c. We
 * deliberately don't expose it as a public symbol — instead derive
 * estimated bytes from the visible count. */
#define NIMCP_AUDIT_ENTRY_BYTES_HINT  192

static void _read_proc_status(nimcp_alloc_stats_t* out) {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        size_t kb = 0;
        if (sscanf(line, "VmRSS: %zu", &kb) == 1) out->proc_vm_rss_bytes = kb * 1024;
        else if (sscanf(line, "VmData: %zu", &kb) == 1) out->proc_vm_data_bytes = kb * 1024;
        else if (sscanf(line, "VmPeak: %zu", &kb) == 1) out->proc_vm_peak_bytes = kb * 1024;
        else if (sscanf(line, "VmLib: %zu", &kb) == 1) out->proc_vm_lib_bytes = kb * 1024;
        else if (sscanf(line, "RssAnon: %zu", &kb) == 1) out->proc_rss_anon_bytes = kb * 1024;
        else if (sscanf(line, "RssFile: %zu", &kb) == 1) out->proc_rss_file_bytes = kb * 1024;
        else if (sscanf(line, "RssShmem: %zu", &kb) == 1) out->proc_rss_shmem_bytes = kb * 1024;
    }
    fclose(f);
}

nimcp_status_t nimcp_get_alloc_stats(nimcp_brain_t brain, nimcp_alloc_stats_t* out) {
    if (!out) return NIMCP_ERROR_NULL_ARG;
    memset(out, 0, sizeof(*out));

    /* glibc mallinfo2 — accurate large counters (mallinfo() is 32-bit) */
    struct mallinfo2 mi = mallinfo2();
    out->glibc_arena_bytes     = (size_t)mi.arena;
    out->glibc_uordblks_bytes  = (size_t)mi.uordblks;
    out->glibc_fordblks_bytes  = (size_t)mi.fordblks;
    out->glibc_hblkhd_bytes    = (size_t)mi.hblkhd;
    out->glibc_hblks           = (uint64_t)mi.hblks;
    out->glibc_uordblks_count  = (uint64_t)mi.ordblks;  /* free chunks; in-use count not in mallinfo */

    /* /proc/self/status */
    _read_proc_status(out);

    /* Safety audit ring */
    out->audit_entry_count = nimcp_safety_audit_get_count();
    out->audit_estimated_bytes = (size_t)out->audit_entry_count * NIMCP_AUDIT_ENTRY_BYTES_HINT;

    /* Internal knowledge graph (only if brain is initialized) */
    if (brain && brain->internal_brain && brain->internal_brain->internal_kg) {
        brain_kg_stats_t ks;
        memset(&ks, 0, sizeof(ks));
        if (brain_kg_get_stats(brain->internal_brain->internal_kg, &ks) == 0) {
            out->kg_node_count = ks.total_nodes;
            out->kg_edge_count = ks.total_edges;
            out->kg_query_count = ks.queries_count;
            out->kg_modification_count = ks.modifications_count;
            out->kg_estimated_bytes =
                (size_t)ks.total_nodes * NIMCP_KG_NODE_BYTES_HINT +
                (size_t)ks.total_edges * NIMCP_KG_EDGE_BYTES_HINT;
        }
    }

    return NIMCP_OK;
}
