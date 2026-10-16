/**
 * @file nimcp_brain_tick_basal_ganglia.c
 * @brief Implements brain_tick_basal_ganglia() — Wave 8B-c hot-path tick.
 *
 * Drives bg_enhanced_step(dt_ms) which advances all enhanced BG
 * subsystems in one pass: striatum dynamics, beta oscillations, vigor
 * decay, model-based evaluation, sequence chunking, hierarchical RL,
 * neuromodulator clearance, and outcome devaluation timing.
 *
 * @author NIMCP Development Team
 * @date 2026-04-24
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_tick_basal_ganglia.h"
#include "core/brain/nimcp_brain_internal.h"

#include "core/brain/subcortical/nimcp_basal_ganglia_enhanced.h"

void brain_tick_basal_ganglia(brain_t brain, float dt_ms)
{
    if (!brain) {
        return;
    }

    if (!brain->basal_ganglia || !brain->basal_ganglia_enabled) {
        return;
    }

    (void)bg_enhanced_step(brain->basal_ganglia, dt_ms);
}
