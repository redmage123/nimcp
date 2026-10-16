/**
 * @file nimcp_brain_tick_entorhinal.c
 * @brief Wave 8B-c entorhinal tick — ACTIVE (minimal).
 *
 * Advances brain->entorhinal one step. The underlying adapter is a
 * MINIMAL implementation (see nimcp_entorhinal_adapter.c): it increments
 * counters + timestamps but does NOT simulate grid cells or path
 * integration. The driver still fires every tick so the coordinator
 * slot is live and future waves can swap in richer logic.
 */

#include "core/brain/nimcp_brain_tick_entorhinal.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_adapter.h"

void brain_tick_entorhinal(brain_t brain, float dt_ms)
{
    if (!brain) return;
    if (!brain->entorhinal) return;  /* Adapter absent (TINY init). */
    if (!brain->entorhinal_enabled) return;

    entorhinal_adapter_update(brain->entorhinal, dt_ms);
}
