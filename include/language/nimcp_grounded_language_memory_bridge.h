/**
 * @file nimcp_grounded_language_memory_bridge.h
 * @brief Internal-ish header for the memory-bridge dispatcher.
 *
 * The connect APIs are part of the public grounded_language.h surface;
 * the dispatcher entry point lives here because it's only called from
 * within src/language/ and shouldn't pollute the public header with the
 * gl_grounding_event_t parameter exposure.
 */

#ifndef NIMCP_GROUNDED_LANGUAGE_MEMORY_BRIDGE_H
#define NIMCP_GROUNDED_LANGUAGE_MEMORY_BRIDGE_H

#include "language/nimcp_grounded_language.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fan a successful grounding event out to working memory,
 *        episodic replay, and the hippocampus adapter (all optional).
 *
 * Called from grounded_language_ground() after a binding lands.
 * Skips the entire dispatch if attention < floor or features are NULL.
 */
void gl_dispatch_event_to_memory(grounded_language_t* gl,
                                  const gl_grounding_event_t* event,
                                  uint64_t concept_id);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GROUNDED_LANGUAGE_MEMORY_BRIDGE_H */
