/**
 * @file nimcp_concept_registry.h
 * @brief Slice B — Cross-modal population binding registry
 *
 * WHAT: Maps referents (text labels, visual feature hashes, audio feature
 *       hashes) onto a SINGLE canonical `concept_pop_id`. First-write wins;
 *       subsequent registrations of the same referent return the existing
 *       id. Cross-modal binding (e.g. image of leaves + spoken "leaves")
 *       coalesces the two ids under union-find so future lookups resolve
 *       to the same root.
 *
 * WHY:  Quian Quiroga concept cells fire for the image, written word, and
 *       spoken word of the same referent — biology uses a single
 *       population to encode a referent across modalities. Pre-Slice-B,
 *       visual-encode and text-comprehend allocated independent concept
 *       pops with no enforcement that they coalesce; supervised
 *       (image, label) training had no path from one to the other.
 *
 * HOW:  Three hash maps (text fingerprint → id, visual feature digest →
 *       id, audio feature digest → id) feeding a union-find structure
 *       keyed on `concept_pop_id_t`. Lookups are O(α(n)) amortized;
 *       hashes are O(n) over the input string / feature vector length.
 *
 * THREAD-SAFETY: All public functions are thread-safe via an internal
 *       `nimcp_mutex_t`. This is NOT a hot path — typical call rate is
 *       <100 Hz during training.
 *
 * SERIALIZATION: `concept_registry_save` / `concept_registry_load` round-
 *       trip the union-find roots, fingerprints, and total bindings count.
 *       Wired into brain_save / brain_load via a `.concept_registry`
 *       sidecar (see src/core/brain/persistence/nimcp_brain_persistence.c).
 *
 * ID ALLOCATION:
 *       - 0          reserved ("no concept")
 *       - 1..N-1     allocated, monotonic increment
 *       - 0xFFFFFFFF reserved ("invalid", aka CONCEPT_POP_ID_INVALID)
 *
 * AUTHOR: Slice B — Option 1 architectural rebuild (2026-05-19)
 * SEE:    docs/claude/option1-rebuild.md (Slice B section)
 */

#ifndef NIMCP_CONCEPT_REGISTRY_H
#define NIMCP_CONCEPT_REGISTRY_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle. The struct definition lives entirely in the .c file. */
typedef struct concept_registry concept_registry_t;

/* Concept population identifier. 0 == "no concept", 0xFFFFFFFF == invalid. */
typedef uint32_t concept_pop_id_t;

#define CONCEPT_POP_ID_NONE     0u
#define CONCEPT_POP_ID_INVALID  0xFFFFFFFFu

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * @brief Create a new concept registry.
 *
 * @param initial_capacity  Hint for hash-table sizing. Pass 0 for default.
 * @return New registry, or NULL on allocation failure.
 *
 * COMPLEXITY: O(initial_capacity)
 */
concept_registry_t* concept_registry_create(size_t initial_capacity);

/**
 * @brief Destroy a registry and release all memory. NULL-safe.
 */
void concept_registry_destroy(concept_registry_t* reg);

/*=============================================================================
 * Interning — first write binds, subsequent calls return the same id
 *===========================================================================*/

/**
 * @brief Intern a text referent.
 *
 * The text is trimmed of leading/trailing whitespace and lowercased before
 * hashing, so "Leaves", "leaves ", and " leaves" all map to the same id.
 *
 * @param reg   Registry (non-NULL)
 * @param text  NUL-terminated UTF-8 string (non-NULL, non-empty)
 * @return  The canonical concept_pop_id_t for this text, or
 *          CONCEPT_POP_ID_INVALID on error.
 *
 * COMPLEXITY: O(len(text)) for hashing + O(α(n)) union-find resolve.
 */
concept_pop_id_t concept_registry_intern_text(concept_registry_t* reg,
                                              const char* text);

/**
 * @brief Intern a visual feature referent.
 *
 * Features are quantized to the nearest 0.1 before hashing. This is a
 * crude LSH that works for the bring-up — adjacent features (e.g. two
 * frames of the same scene) collide; small noise is tolerated. Tune
 * later in walkthroughs.
 *
 * @param features  Float array (non-NULL, n > 0)
 * @param n         Number of floats
 * @return  Canonical id, or CONCEPT_POP_ID_INVALID on error.
 *
 * COMPLEXITY: O(n) for hashing.
 */
concept_pop_id_t concept_registry_intern_visual(concept_registry_t* reg,
                                                const float* features,
                                                size_t n);

/**
 * @brief Intern an audio feature referent. Same quantization as visual.
 */
concept_pop_id_t concept_registry_intern_audio(concept_registry_t* reg,
                                               const float* features,
                                               size_t n);

/*=============================================================================
 * Cross-modal binding
 *===========================================================================*/

/**
 * @brief Bind two ids as referring to the same referent.
 *
 * Implemented via union-find: the smaller root becomes the canonical id
 * (stable across binding order). After this call, both ids resolve to
 * the same canonical root via concept_registry_canonical().
 *
 * @return 0 on success, -1 on invalid id or NULL registry.
 *
 * COMPLEXITY: O(α(n)) amortized.
 */
int concept_registry_bind_modalities(concept_registry_t* reg,
                                     concept_pop_id_t a,
                                     concept_pop_id_t b);

/**
 * @brief Resolve any id (root or alias) to its current canonical root.
 *
 * Applies path compression as a side effect, so repeated lookups on the
 * same id are O(1).
 *
 * @return Canonical id, or CONCEPT_POP_ID_INVALID if `id` is unknown.
 *
 * COMPLEXITY: O(α(n)) amortized.
 */
concept_pop_id_t concept_registry_canonical(concept_registry_t* reg,
                                            concept_pop_id_t id);

/*=============================================================================
 * Stats
 *===========================================================================*/

/**
 * @brief Total number of distinct referents allocated (root count + alias
 *        count, i.e. every id ever returned by an intern_* call).
 *
 * Note: this is the raw allocation count. To count distinct canonical
 * roots, walk every id from 1..total and dedupe via canonical(). For the
 * tests we only need total.
 */
size_t concept_registry_total_referents(const concept_registry_t* reg);

/**
 * @brief Count of cross-modal binding edges issued (sum of
 *        bind_modalities calls that produced a merge — same-root binds
 *        are not counted).
 */
size_t concept_registry_total_modality_bindings(const concept_registry_t* reg);

/*=============================================================================
 * Serialization
 *===========================================================================*/

/**
 * @brief Serialize the registry to a stdio file. The file must be open
 *        for writing in binary mode.
 *
 * Format (v1):
 *   "NIMC_CR1" magic (8 bytes)
 *   uint32_t   total_referents
 *   uint32_t   total_modality_bindings
 *   uint32_t   union_find size (== total_referents + 1)
 *   uint32_t[] parent_array (size_t length above)
 *   uint32_t   n_text_entries
 *   for each text entry: uint32_t id, uint32_t text_len, char[] text
 *   uint32_t   n_visual_entries
 *   for each visual entry: uint32_t id, uint64_t fingerprint
 *   uint32_t   n_audio_entries
 *   for each audio entry: uint32_t id, uint64_t fingerprint
 *
 * @return 0 on success, -1 on I/O error.
 */
int concept_registry_save(const concept_registry_t* reg, FILE* fp);

/**
 * @brief Load registry state from a stdio file. Overwrites in-memory
 *        state. The file must be open for reading in binary mode.
 *
 * @return 0 on success, -1 on I/O error or version mismatch.
 */
int concept_registry_load(concept_registry_t* reg, FILE* fp);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONCEPT_REGISTRY_H */
