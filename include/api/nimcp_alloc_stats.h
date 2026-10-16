#ifndef NIMCP_ALLOC_STATS_H
#define NIMCP_ALLOC_STATS_H

#include <stddef.h>
#include <stdint.h>

#include "nimcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocator / memory accounting snapshot.
 *
 * Aggregates signals from glibc mallinfo2(), the safety audit ring buffer,
 * the internal knowledge graph, and /proc/self/status. Used to attribute
 * the brain daemon's RSS growth to specific subsystems.
 *
 * Any field that cannot be filled (subsystem not initialized, /proc not
 * readable, etc.) is left at zero.
 */
typedef struct nimcp_alloc_stats {
    /* glibc allocator (mallinfo2) — bytes */
    size_t glibc_arena_bytes;       /* total non-mmap memory from sbrk */
    size_t glibc_uordblks_bytes;    /* in-use bytes (small allocs) */
    size_t glibc_fordblks_bytes;    /* free chunks in arena */
    size_t glibc_hblkhd_bytes;      /* glibc-tracked mmap'd memory */
    uint64_t glibc_hblks;           /* number of glibc-tracked mmap regions */
    uint64_t glibc_uordblks_count;  /* hint: number of allocated chunks (approx) */

    /* /proc/self/status — bytes */
    size_t proc_vm_rss_bytes;
    size_t proc_vm_data_bytes;
    size_t proc_vm_peak_bytes;
    size_t proc_vm_lib_bytes;
    size_t proc_rss_anon_bytes;
    size_t proc_rss_file_bytes;
    size_t proc_rss_shmem_bytes;

    /* Always-on safety audit ring buffer */
    uint32_t audit_entry_count;
    size_t audit_estimated_bytes;

    /* Internal knowledge graph (per-brain) */
    uint32_t kg_node_count;
    uint32_t kg_edge_count;
    uint64_t kg_query_count;
    uint64_t kg_modification_count;
    size_t kg_estimated_bytes;
} nimcp_alloc_stats_t;

/**
 * Fill `out` with a current snapshot.
 * @param brain optional — may be NULL; if NULL, brain-specific counters are zero.
 * @param out non-NULL output buffer.
 * @return NIMCP_OK on success; NIMCP_ERROR_NULL_ARG if `out` is NULL.
 */
NIMCP_EXPORT nimcp_status_t nimcp_get_alloc_stats(
    nimcp_brain_t brain, nimcp_alloc_stats_t* out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ALLOC_STATS_H */
