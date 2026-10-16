/**
 * @file nimcp_concept_registry.c
 * @brief Slice B — Cross-modal population binding registry implementation
 *
 * See include/language/nimcp_concept_registry.h for the API contract.
 *
 * Implementation notes:
 *   - Hash maps use linear probing over chained-bucket arrays. The maps
 *     are growable but the union-find parent array is a flat dynamic
 *     buffer indexed by concept_pop_id_t.
 *   - Pop IDs start at 1; 0 is reserved for CONCEPT_POP_ID_NONE.
 *   - The text fingerprint is the FNV-1a 64-bit hash of the lowercased,
 *     whitespace-trimmed input. Collisions in 64-bit FNV are vanishingly
 *     unlikely for our scale (<< 10^9 referents).
 *   - The visual/audio fingerprint quantizes each float to nearest 0.1
 *     and hashes the resulting int16_t array with FNV-1a 64.
 */

#include "language/nimcp_concept_registry.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Log prefix used by every LOG_* call site below. The bare LOG_INFO macro
 * is variadic — we prepend the module string to the format manually. */
#define CR_LOG_PREFIX "CONCEPT_REGISTRY: "

/* Initial hash-table capacity if caller passes 0. Power of 2 for cheap modulo. */
#define CR_DEFAULT_CAPACITY  256u

/* Magic for serialization. */
static const char CR_MAGIC_V1[8] = {'N','I','M','C','_','C','R','1'};

/*=============================================================================
 * Internal types
 *===========================================================================*/

/* Text hash-table entry. Linear-probed open addressing. */
typedef struct {
    uint64_t         fingerprint;   /* 0 == empty slot */
    char*            text;          /* owned NUL-terminated string */
    concept_pop_id_t id;            /* CONCEPT_POP_ID_NONE == empty */
} cr_text_slot_t;

/* Visual / audio hash-table entry. */
typedef struct {
    uint64_t         fingerprint;   /* 0 == empty slot */
    concept_pop_id_t id;            /* CONCEPT_POP_ID_NONE == empty */
} cr_feat_slot_t;

struct concept_registry {
    /* Union-find parent array. parent[i] == i means i is a root. */
    concept_pop_id_t* parent;
    size_t            parent_len;     /* capacity */
    size_t            parent_count;   /* high-water-mark, == next id to allocate */

    /* Text fingerprint → id */
    cr_text_slot_t*   text_slots;
    size_t            text_cap;
    size_t            text_count;

    /* Visual fingerprint → id */
    cr_feat_slot_t*   visual_slots;
    size_t            visual_cap;
    size_t            visual_count;

    /* Audio fingerprint → id */
    cr_feat_slot_t*   audio_slots;
    size_t            audio_cap;
    size_t            audio_count;

    /* Counter of merges that actually changed a root. */
    size_t            modality_bind_merges;

    /* All public API is serialized through this mutex. */
    nimcp_mutex_t*    mutex;
};

/*=============================================================================
 * Hashing helpers
 *===========================================================================*/

/* FNV-1a 64-bit over arbitrary bytes. */
static uint64_t fnv1a_64(const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)p[i];
        h *= 0x100000001b3ULL;
    }
    /* Guarantee non-zero — slot 0 means "empty". On the 1-in-2^64 chance
     * of a true 0 hash, bump to 1. */
    return h ? h : 1ULL;
}

/* Trim leading/trailing ASCII whitespace and lowercase ASCII letters.
 * Writes to dst (capacity dst_cap including NUL). Returns length written
 * (excluding NUL), or 0 on failure / empty result. */
static size_t cr_normalize_text(const char* src, char* dst, size_t dst_cap)
{
    if (!src || dst_cap == 0) return 0;
    /* Skip leading whitespace */
    while (*src && isspace((unsigned char)*src)) src++;
    /* Find end, then trim trailing whitespace */
    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1])) len--;
    if (len == 0) return 0;
    if (len >= dst_cap) len = dst_cap - 1;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (char)tolower(c);
    }
    dst[len] = '\0';
    return len;
}

/* Quantize float vector to int16_t array at step 0.1 and hash.
 * Caps n at 4096 (defensive — features should be much smaller in practice). */
static uint64_t cr_hash_features(const float* features, size_t n)
{
    if (!features || n == 0) return 0;
    if (n > 4096) n = 4096;
    /* int16_t range covers [-3276.7, +3276.7] at step 0.1 — fine for
     * normalized network features in [-1, 1] or small dynamic ranges. */
    int16_t buf_stack[256];
    int16_t* buf = buf_stack;
    int16_t* buf_heap = NULL;
    if (n > 256) {
        buf_heap = (int16_t*)nimcp_malloc(n * sizeof(int16_t));
        if (!buf_heap) return 0;
        buf = buf_heap;
    }
    for (size_t i = 0; i < n; i++) {
        float v = features[i];
        if (isnan(v) || isinf(v)) v = 0.0f;
        /* round-half-to-even via lrintf */
        float scaled = v * 10.0f;
        long q = lrintf(scaled);
        if (q > 32767) q = 32767;
        if (q < -32768) q = -32768;
        buf[i] = (int16_t)q;
    }
    uint64_t h = fnv1a_64(buf, n * sizeof(int16_t));
    if (buf_heap) nimcp_free(buf_heap);
    return h;
}

/*=============================================================================
 * Union-find primitives. Caller holds reg->mutex.
 *===========================================================================*/

static int cr_ensure_parent_capacity(concept_registry_t* reg, size_t need)
{
    if (need <= reg->parent_len) return 0;
    size_t new_cap = reg->parent_len ? reg->parent_len : 16;
    while (new_cap < need) new_cap *= 2;
    concept_pop_id_t* np = (concept_pop_id_t*)nimcp_realloc(
        reg->parent, new_cap * sizeof(concept_pop_id_t));
    if (!np) return -1;
    /* Zero-init the new slots (won't be referenced until they get an id). */
    memset(np + reg->parent_len, 0,
           (new_cap - reg->parent_len) * sizeof(concept_pop_id_t));
    reg->parent = np;
    reg->parent_len = new_cap;
    return 0;
}

/* Allocate a fresh id (root pointing to itself). Caller holds mutex. */
static concept_pop_id_t cr_alloc_id(concept_registry_t* reg)
{
    /* parent_count is the next id to allocate. We start at 1. */
    size_t next = reg->parent_count == 0 ? 1u : reg->parent_count;
    /* Defensive cap — leave headroom under INVALID. */
    if (next >= CONCEPT_POP_ID_INVALID) {
        LOG_ERROR(CR_LOG_PREFIX "cr_alloc_id: id space exhausted (next=%zu)", next);
        return CONCEPT_POP_ID_INVALID;
    }
    if (cr_ensure_parent_capacity(reg, next + 1) != 0) {
        return CONCEPT_POP_ID_INVALID;
    }
    concept_pop_id_t id = (concept_pop_id_t)next;
    reg->parent[id] = id;             /* root points to itself */
    reg->parent_count = next + 1;
    return id;
}

/* Path-compressing find. Caller holds mutex. */
static concept_pop_id_t cr_find(concept_registry_t* reg, concept_pop_id_t id)
{
    if (id == CONCEPT_POP_ID_NONE || id == CONCEPT_POP_ID_INVALID ||
        (size_t)id >= reg->parent_count) {
        return CONCEPT_POP_ID_INVALID;
    }
    /* Walk to root */
    concept_pop_id_t root = id;
    while (reg->parent[root] != root) {
        root = reg->parent[root];
    }
    /* Path compression */
    concept_pop_id_t cur = id;
    while (reg->parent[cur] != root) {
        concept_pop_id_t nxt = reg->parent[cur];
        reg->parent[cur] = root;
        cur = nxt;
    }
    return root;
}

/*=============================================================================
 * Hash-table primitives. Caller holds mutex.
 *===========================================================================*/

static size_t cr_next_pow2(size_t n)
{
    size_t r = 1;
    while (r < n) r <<= 1;
    return r;
}

/* Returns slot index where fingerprint either matches or empty slot was found.
 * Linear probing. table_cap MUST be a power of two. */
static size_t cr_probe_text(const cr_text_slot_t* slots, size_t cap,
                            uint64_t fp)
{
    size_t mask = cap - 1;
    size_t i = (size_t)fp & mask;
    while (slots[i].fingerprint != 0 && slots[i].fingerprint != fp) {
        i = (i + 1) & mask;
    }
    return i;
}

static size_t cr_probe_feat(const cr_feat_slot_t* slots, size_t cap,
                            uint64_t fp)
{
    size_t mask = cap - 1;
    size_t i = (size_t)fp & mask;
    while (slots[i].fingerprint != 0 && slots[i].fingerprint != fp) {
        i = (i + 1) & mask;
    }
    return i;
}

/* Resize text table to new_cap (power of 2). Returns 0 on success. */
static int cr_text_resize(concept_registry_t* reg, size_t new_cap)
{
    cr_text_slot_t* ns = (cr_text_slot_t*)nimcp_calloc(
        new_cap, sizeof(cr_text_slot_t));
    if (!ns) return -1;
    for (size_t i = 0; i < reg->text_cap; i++) {
        if (reg->text_slots[i].fingerprint == 0) continue;
        size_t j = cr_probe_text(ns, new_cap, reg->text_slots[i].fingerprint);
        ns[j] = reg->text_slots[i];
    }
    nimcp_free(reg->text_slots);
    reg->text_slots = ns;
    reg->text_cap = new_cap;
    return 0;
}

static int cr_feat_resize(cr_feat_slot_t** slots_io, size_t* cap_io,
                          size_t new_cap)
{
    cr_feat_slot_t* ns = (cr_feat_slot_t*)nimcp_calloc(
        new_cap, sizeof(cr_feat_slot_t));
    if (!ns) return -1;
    for (size_t i = 0; i < *cap_io; i++) {
        if ((*slots_io)[i].fingerprint == 0) continue;
        size_t j = cr_probe_feat(ns, new_cap, (*slots_io)[i].fingerprint);
        ns[j] = (*slots_io)[i];
    }
    nimcp_free(*slots_io);
    *slots_io = ns;
    *cap_io = new_cap;
    return 0;
}

/* Grow table if load factor > 0.7. */
static int cr_text_maybe_grow(concept_registry_t* reg)
{
    if (reg->text_count * 10 < reg->text_cap * 7) return 0;
    return cr_text_resize(reg, reg->text_cap * 2);
}

static int cr_visual_maybe_grow(concept_registry_t* reg)
{
    if (reg->visual_count * 10 < reg->visual_cap * 7) return 0;
    return cr_feat_resize(&reg->visual_slots, &reg->visual_cap,
                          reg->visual_cap * 2);
}

static int cr_audio_maybe_grow(concept_registry_t* reg)
{
    if (reg->audio_count * 10 < reg->audio_cap * 7) return 0;
    return cr_feat_resize(&reg->audio_slots, &reg->audio_cap,
                          reg->audio_cap * 2);
}

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

concept_registry_t* concept_registry_create(size_t initial_capacity)
{
    concept_registry_t* reg = (concept_registry_t*)nimcp_calloc(
        1, sizeof(concept_registry_t));
    if (!reg) {
        LOG_ERROR(CR_LOG_PREFIX "concept_registry_create: alloc failed");
        return NULL;
    }

    size_t cap = initial_capacity > 0 ? initial_capacity : CR_DEFAULT_CAPACITY;
    cap = cr_next_pow2(cap);
    if (cap < 16) cap = 16;

    reg->text_slots = (cr_text_slot_t*)nimcp_calloc(cap, sizeof(cr_text_slot_t));
    reg->visual_slots = (cr_feat_slot_t*)nimcp_calloc(cap, sizeof(cr_feat_slot_t));
    reg->audio_slots = (cr_feat_slot_t*)nimcp_calloc(cap, sizeof(cr_feat_slot_t));
    /* Allocate parent buffer with slot 0 reserved (== CONCEPT_POP_ID_NONE). */
    reg->parent = (concept_pop_id_t*)nimcp_calloc(16, sizeof(concept_pop_id_t));

    if (!reg->text_slots || !reg->visual_slots || !reg->audio_slots ||
        !reg->parent) {
        concept_registry_destroy(reg);
        return NULL;
    }

    reg->text_cap = cap;
    reg->visual_cap = cap;
    reg->audio_cap = cap;
    reg->parent_len = 16;
    reg->parent_count = 1;     /* IDs start at 1 */
    reg->parent[0] = 0;        /* slot 0 is sentinel */

    reg->mutex = nimcp_mutex_create(NULL);
    if (!reg->mutex) {
        concept_registry_destroy(reg);
        return NULL;
    }

    LOG_INFO(CR_LOG_PREFIX "concept_registry_create: capacity=%zu", cap);
    return reg;
}

void concept_registry_destroy(concept_registry_t* reg)
{
    if (!reg) return;

    if (reg->text_slots) {
        for (size_t i = 0; i < reg->text_cap; i++) {
            if (reg->text_slots[i].text) {
                nimcp_free(reg->text_slots[i].text);
            }
        }
        nimcp_free(reg->text_slots);
    }
    if (reg->visual_slots) nimcp_free(reg->visual_slots);
    if (reg->audio_slots) nimcp_free(reg->audio_slots);
    if (reg->parent) nimcp_free(reg->parent);
    if (reg->mutex) nimcp_mutex_free(reg->mutex);

    nimcp_free(reg);
}

/*=============================================================================
 * Interning
 *===========================================================================*/

concept_pop_id_t concept_registry_intern_text(concept_registry_t* reg,
                                              const char* text)
{
    if (!reg || !text) return CONCEPT_POP_ID_INVALID;

    /* Normalize: trim + lowercase. Cap at 256 (defensive — labels are
     * usually a single word). Empty after normalize → invalid. */
    char norm[256];
    size_t nlen = cr_normalize_text(text, norm, sizeof(norm));
    if (nlen == 0) return CONCEPT_POP_ID_INVALID;

    uint64_t fp = fnv1a_64(norm, nlen);

    nimcp_mutex_lock(reg->mutex);
    size_t slot = cr_probe_text(reg->text_slots, reg->text_cap, fp);
    if (reg->text_slots[slot].fingerprint == fp) {
        /* Hit. Resolve through union-find for current canonical root. */
        concept_pop_id_t root = cr_find(reg, reg->text_slots[slot].id);
        nimcp_mutex_unlock(reg->mutex);
        return root;
    }
    /* Miss → allocate fresh id. */
    concept_pop_id_t id = cr_alloc_id(reg);
    if (id == CONCEPT_POP_ID_INVALID) {
        nimcp_mutex_unlock(reg->mutex);
        return CONCEPT_POP_ID_INVALID;
    }
    char* dup = (char*)nimcp_malloc(nlen + 1);
    if (!dup) {
        nimcp_mutex_unlock(reg->mutex);
        return CONCEPT_POP_ID_INVALID;
    }
    memcpy(dup, norm, nlen + 1);
    reg->text_slots[slot].fingerprint = fp;
    reg->text_slots[slot].text = dup;
    reg->text_slots[slot].id = id;
    reg->text_count++;
    if (cr_text_maybe_grow(reg) != 0) {
        /* Allocation failure on grow — log and continue with the over-full
         * table. Future inserts will retry the grow. */
        LOG_WARN(CR_LOG_PREFIX "text table grow failed (count=%zu cap=%zu)",
                 reg->text_count, reg->text_cap);
    }
    nimcp_mutex_unlock(reg->mutex);
    return id;
}

static concept_pop_id_t cr_intern_feature(concept_registry_t* reg,
                                          cr_feat_slot_t** slots_io,
                                          size_t* cap_io, size_t* count_io,
                                          const float* features, size_t n,
                                          int (*grow_fn)(concept_registry_t*))
{
    if (!reg || !features || n == 0) return CONCEPT_POP_ID_INVALID;
    uint64_t fp = cr_hash_features(features, n);
    if (fp == 0) return CONCEPT_POP_ID_INVALID;

    nimcp_mutex_lock(reg->mutex);
    size_t slot = cr_probe_feat(*slots_io, *cap_io, fp);
    if ((*slots_io)[slot].fingerprint == fp) {
        concept_pop_id_t root = cr_find(reg, (*slots_io)[slot].id);
        nimcp_mutex_unlock(reg->mutex);
        return root;
    }
    concept_pop_id_t id = cr_alloc_id(reg);
    if (id == CONCEPT_POP_ID_INVALID) {
        nimcp_mutex_unlock(reg->mutex);
        return CONCEPT_POP_ID_INVALID;
    }
    (*slots_io)[slot].fingerprint = fp;
    (*slots_io)[slot].id = id;
    (*count_io)++;
    if (grow_fn(reg) != 0) {
        LOG_WARN(CR_LOG_PREFIX "feature table grow failed (count=%zu cap=%zu)",
                 *count_io, *cap_io);
    }
    nimcp_mutex_unlock(reg->mutex);
    return id;
}

concept_pop_id_t concept_registry_intern_visual(concept_registry_t* reg,
                                                const float* features,
                                                size_t n)
{
    if (!reg) return CONCEPT_POP_ID_INVALID;
    return cr_intern_feature(reg, &reg->visual_slots, &reg->visual_cap,
                             &reg->visual_count, features, n,
                             cr_visual_maybe_grow);
}

concept_pop_id_t concept_registry_intern_audio(concept_registry_t* reg,
                                               const float* features,
                                               size_t n)
{
    if (!reg) return CONCEPT_POP_ID_INVALID;
    return cr_intern_feature(reg, &reg->audio_slots, &reg->audio_cap,
                             &reg->audio_count, features, n,
                             cr_audio_maybe_grow);
}

/*=============================================================================
 * Cross-modal binding
 *===========================================================================*/

int concept_registry_bind_modalities(concept_registry_t* reg,
                                     concept_pop_id_t a,
                                     concept_pop_id_t b)
{
    if (!reg) return -1;
    if (a == CONCEPT_POP_ID_NONE || a == CONCEPT_POP_ID_INVALID) return -1;
    if (b == CONCEPT_POP_ID_NONE || b == CONCEPT_POP_ID_INVALID) return -1;

    nimcp_mutex_lock(reg->mutex);
    concept_pop_id_t ra = cr_find(reg, a);
    concept_pop_id_t rb = cr_find(reg, b);
    if (ra == CONCEPT_POP_ID_INVALID || rb == CONCEPT_POP_ID_INVALID) {
        nimcp_mutex_unlock(reg->mutex);
        return -1;
    }
    if (ra == rb) {
        /* already coalesced — no merge */
        nimcp_mutex_unlock(reg->mutex);
        return 0;
    }
    /* Smaller-root-wins for stability across binding order. */
    concept_pop_id_t lo = ra < rb ? ra : rb;
    concept_pop_id_t hi = ra < rb ? rb : ra;
    reg->parent[hi] = lo;
    reg->modality_bind_merges++;
    nimcp_mutex_unlock(reg->mutex);
    return 0;
}

concept_pop_id_t concept_registry_canonical(concept_registry_t* reg,
                                            concept_pop_id_t id)
{
    if (!reg) return CONCEPT_POP_ID_INVALID;
    nimcp_mutex_lock(reg->mutex);
    concept_pop_id_t r = cr_find(reg, id);
    nimcp_mutex_unlock(reg->mutex);
    return r;
}

/*=============================================================================
 * Stats
 *===========================================================================*/

size_t concept_registry_total_referents(const concept_registry_t* reg)
{
    if (!reg) return 0;
    /* parent_count includes the sentinel slot 0. Active id range is
     * [1, parent_count) so the count is parent_count - 1. */
    return reg->parent_count > 0 ? reg->parent_count - 1 : 0;
}

size_t concept_registry_total_modality_bindings(const concept_registry_t* reg)
{
    if (!reg) return 0;
    return reg->modality_bind_merges;
}

/*=============================================================================
 * Serialization
 *===========================================================================*/

int concept_registry_save(const concept_registry_t* reg, FILE* fp)
{
    if (!reg || !fp) return -1;

    /* The cast-away-const is local — we only need it to take the lock.
     * The mutation is just the lock state, not the logical contents. */
    concept_registry_t* mreg = (concept_registry_t*)reg;
    nimcp_mutex_lock(mreg->mutex);

    int rc = -1;

    /* Magic */
    if (fwrite(CR_MAGIC_V1, 1, 8, fp) != 8) goto out;

    uint32_t total_ref  = (uint32_t)concept_registry_total_referents(reg);
    uint32_t total_bind = (uint32_t)reg->modality_bind_merges;
    uint32_t parent_n   = (uint32_t)reg->parent_count;  /* includes slot 0 */

    if (fwrite(&total_ref,  sizeof(uint32_t), 1, fp) != 1) goto out;
    if (fwrite(&total_bind, sizeof(uint32_t), 1, fp) != 1) goto out;
    if (fwrite(&parent_n,   sizeof(uint32_t), 1, fp) != 1) goto out;

    if (parent_n > 0) {
        if (fwrite(reg->parent, sizeof(concept_pop_id_t), parent_n, fp) !=
            parent_n) {
            goto out;
        }
    }

    /* Text table — write entries with non-zero fingerprint. */
    uint32_t n_text = (uint32_t)reg->text_count;
    if (fwrite(&n_text, sizeof(uint32_t), 1, fp) != 1) goto out;
    for (size_t i = 0; i < reg->text_cap; i++) {
        const cr_text_slot_t* s = &reg->text_slots[i];
        if (s->fingerprint == 0) continue;
        uint32_t id = s->id;
        uint32_t tlen = s->text ? (uint32_t)strlen(s->text) : 0;
        if (fwrite(&id,   sizeof(uint32_t), 1, fp) != 1) goto out;
        if (fwrite(&tlen, sizeof(uint32_t), 1, fp) != 1) goto out;
        if (tlen > 0) {
            if (fwrite(s->text, 1, tlen, fp) != tlen) goto out;
        }
    }

    /* Visual table */
    uint32_t n_vis = (uint32_t)reg->visual_count;
    if (fwrite(&n_vis, sizeof(uint32_t), 1, fp) != 1) goto out;
    for (size_t i = 0; i < reg->visual_cap; i++) {
        const cr_feat_slot_t* s = &reg->visual_slots[i];
        if (s->fingerprint == 0) continue;
        uint32_t id = s->id;
        if (fwrite(&id,             sizeof(uint32_t), 1, fp) != 1) goto out;
        if (fwrite(&s->fingerprint, sizeof(uint64_t), 1, fp) != 1) goto out;
    }

    /* Audio table */
    uint32_t n_aud = (uint32_t)reg->audio_count;
    if (fwrite(&n_aud, sizeof(uint32_t), 1, fp) != 1) goto out;
    for (size_t i = 0; i < reg->audio_cap; i++) {
        const cr_feat_slot_t* s = &reg->audio_slots[i];
        if (s->fingerprint == 0) continue;
        uint32_t id = s->id;
        if (fwrite(&id,             sizeof(uint32_t), 1, fp) != 1) goto out;
        if (fwrite(&s->fingerprint, sizeof(uint64_t), 1, fp) != 1) goto out;
    }

    rc = 0;
out:
    nimcp_mutex_unlock(mreg->mutex);
    return rc;
}

int concept_registry_load(concept_registry_t* reg, FILE* fp)
{
    if (!reg || !fp) return -1;

    nimcp_mutex_lock(reg->mutex);
    int rc = -1;

    char magic[8];
    if (fread(magic, 1, 8, fp) != 8) goto out;
    if (memcmp(magic, CR_MAGIC_V1, 8) != 0) {
        LOG_WARN(CR_LOG_PREFIX "concept_registry_load: bad magic");
        goto out;
    }

    uint32_t total_ref  = 0;
    uint32_t total_bind = 0;
    uint32_t parent_n   = 0;
    if (fread(&total_ref,  sizeof(uint32_t), 1, fp) != 1) goto out;
    if (fread(&total_bind, sizeof(uint32_t), 1, fp) != 1) goto out;
    if (fread(&parent_n,   sizeof(uint32_t), 1, fp) != 1) goto out;

    if (parent_n > 0) {
        if (cr_ensure_parent_capacity(reg, parent_n) != 0) goto out;
        if (fread(reg->parent, sizeof(concept_pop_id_t), parent_n, fp) !=
            parent_n) {
            goto out;
        }
        reg->parent_count = parent_n;
    } else {
        reg->parent_count = 1;
        if (reg->parent && reg->parent_len > 0) reg->parent[0] = 0;
    }
    reg->modality_bind_merges = total_bind;

    /* Wipe + reload text table. Free existing text pointers. */
    for (size_t i = 0; i < reg->text_cap; i++) {
        if (reg->text_slots[i].text) {
            nimcp_free(reg->text_slots[i].text);
        }
    }
    memset(reg->text_slots, 0, reg->text_cap * sizeof(cr_text_slot_t));
    reg->text_count = 0;

    uint32_t n_text = 0;
    if (fread(&n_text, sizeof(uint32_t), 1, fp) != 1) goto out;
    /* Make sure capacity is enough — re-grow heuristically. */
    while (reg->text_cap < (size_t)n_text * 2 + 16) {
        if (cr_text_resize(reg, reg->text_cap * 2) != 0) goto out;
    }
    for (uint32_t i = 0; i < n_text; i++) {
        uint32_t id = 0;
        uint32_t tlen = 0;
        if (fread(&id, sizeof(uint32_t), 1, fp) != 1) goto out;
        if (fread(&tlen, sizeof(uint32_t), 1, fp) != 1) goto out;
        if (tlen > 4096) goto out;  /* sanity */
        char* dup = (char*)nimcp_malloc((size_t)tlen + 1);
        if (!dup) goto out;
        if (tlen > 0) {
            if (fread(dup, 1, tlen, fp) != tlen) { nimcp_free(dup); goto out; }
        }
        dup[tlen] = '\0';
        uint64_t fphash = fnv1a_64(dup, tlen);
        size_t slot = cr_probe_text(reg->text_slots, reg->text_cap, fphash);
        if (reg->text_slots[slot].fingerprint != 0) {
            nimcp_free(dup);
            goto out;  /* duplicate — corrupted file */
        }
        reg->text_slots[slot].fingerprint = fphash;
        reg->text_slots[slot].text = dup;
        reg->text_slots[slot].id = id;
        reg->text_count++;
    }

    /* Visual */
    memset(reg->visual_slots, 0, reg->visual_cap * sizeof(cr_feat_slot_t));
    reg->visual_count = 0;
    uint32_t n_vis = 0;
    if (fread(&n_vis, sizeof(uint32_t), 1, fp) != 1) goto out;
    while (reg->visual_cap < (size_t)n_vis * 2 + 16) {
        if (cr_feat_resize(&reg->visual_slots, &reg->visual_cap,
                           reg->visual_cap * 2) != 0) goto out;
    }
    for (uint32_t i = 0; i < n_vis; i++) {
        uint32_t id = 0;
        uint64_t fphash = 0;
        if (fread(&id,     sizeof(uint32_t), 1, fp) != 1) goto out;
        if (fread(&fphash, sizeof(uint64_t), 1, fp) != 1) goto out;
        if (fphash == 0) goto out;
        size_t slot = cr_probe_feat(reg->visual_slots, reg->visual_cap, fphash);
        if (reg->visual_slots[slot].fingerprint != 0) goto out;
        reg->visual_slots[slot].fingerprint = fphash;
        reg->visual_slots[slot].id = id;
        reg->visual_count++;
    }

    /* Audio */
    memset(reg->audio_slots, 0, reg->audio_cap * sizeof(cr_feat_slot_t));
    reg->audio_count = 0;
    uint32_t n_aud = 0;
    if (fread(&n_aud, sizeof(uint32_t), 1, fp) != 1) goto out;
    while (reg->audio_cap < (size_t)n_aud * 2 + 16) {
        if (cr_feat_resize(&reg->audio_slots, &reg->audio_cap,
                           reg->audio_cap * 2) != 0) goto out;
    }
    for (uint32_t i = 0; i < n_aud; i++) {
        uint32_t id = 0;
        uint64_t fphash = 0;
        if (fread(&id,     sizeof(uint32_t), 1, fp) != 1) goto out;
        if (fread(&fphash, sizeof(uint64_t), 1, fp) != 1) goto out;
        if (fphash == 0) goto out;
        size_t slot = cr_probe_feat(reg->audio_slots, reg->audio_cap, fphash);
        if (reg->audio_slots[slot].fingerprint != 0) goto out;
        reg->audio_slots[slot].fingerprint = fphash;
        reg->audio_slots[slot].id = id;
        reg->audio_count++;
    }

    /* Sanity: total_ref should match (parent_count - 1). */
    (void)total_ref;  /* informational only — parent_count is authoritative */

    rc = 0;
out:
    nimcp_mutex_unlock(reg->mutex);
    return rc;
}
