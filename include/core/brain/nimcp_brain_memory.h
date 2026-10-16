//=============================================================================
// nimcp_brain_memory.h - Working Memory State Persistence
//=============================================================================

#ifndef NIMCP_BRAIN_MEMORY_H
#define NIMCP_BRAIN_MEMORY_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Working memory save/load functions
bool save_working_memory_state(working_memory_t* wm, FILE* file);
bool load_working_memory_item(working_memory_t* wm, FILE* file);
bool load_working_memory_state(brain_t brain, FILE* file);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_MEMORY_H
