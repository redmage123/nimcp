/**
 * @file nimcp_phonological_loop.h
 * @brief Phonological-loop working memory buffer (Slice 5 of recurrent
 *        language architecture).
 *
 * Baddeley's working-memory model: language production maintains a short-
 * term phonological store (~2 seconds of speech) plus an articulatory
 * rehearsal mechanism that refreshes activation. Real speakers don't
 * construct an utterance in one shot; they hold a partial draft and
 * incrementally refine it.
 *
 * This module exposes the internal accessors used by
 * communication_cascade_run_recurrent. The buffer itself lives on
 * brain_struct (see loop_* fields in nimcp_brain_internal.h); these
 * helpers manage allocation, decay, merge, and surface-form rendering.
 *
 * Default OFF — when brain->loop_enabled is false, every helper here
 * becomes a no-op, preserving Slice 1+2 behavior byte-for-byte.
 *
 * Lock-ordering: each helper takes brain->loop_mutex internally; callers
 * must not hold the bridge or broca mutexes when entering. See
 * docs/claude/modules/lock-ordering.md.
 */

#ifndef NIMCP_PHONOLOGICAL_LOOP_H
#define NIMCP_PHONOLOGICAL_LOOP_H

#include "core/brain/nimcp_brain_internal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Lifecycle — invoked once at brain create/destroy. Init allocates the
 * initial buffer (256 bytes) + trace array (16 slots) + the mutex, all
 * zero-state. Loop is disabled by default; flip via the public setter.
 * ------------------------------------------------------------------------- */
int  phonological_loop_init(brain_t brain);
void phonological_loop_destroy(brain_t brain);

/* ---------------------------------------------------------------------------
 * Recurrent-cascade lifecycle helpers.
 * ------------------------------------------------------------------------- */

/* Decay every trace by brain->loop_decay_rate and drop traces < 0.05.
 * Called at the start of each iteration of the recurrent cascade. */
void phonological_loop_decay(brain_t brain);

/* Merge a freshly produced utterance (one-shot lexical output) into the
 * loop. Tokenizes on whitespace + punctuation. For each token:
 *   - if already present in the buffer: refresh its trace to 1.0
 *   - else: append (trace = 1.0), capped at loop_max_words.
 * Re-renders loop_buffer from the surviving tokens. */
void phonological_loop_merge_words(brain_t brain, const char* utterance);

/* Render the buffer's "intended surface form" — concatenate words whose
 * trace >= threshold, in insertion order. Writes a NUL-terminated string
 * to out_text up to out_max bytes; truncates with NUL if needed.
 * Returns the number of words written. Pass threshold = 0.3 for the
 * canonical "active surface form" used by stage_syntactic + stage_self_comp. */
uint32_t phonological_loop_render_active(brain_t brain,
                                          float threshold,
                                          char* out_text,
                                          uint32_t out_max);

/* Reset all loop state — drops every word + trace, clears buffer,
 * resets last_refresh_ms. Does not free the underlying allocation. */
void phonological_loop_clear(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHONOLOGICAL_LOOP_H */
