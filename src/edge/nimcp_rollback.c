/**
 * @file nimcp_rollback.c
 * @brief Rollback safety for edge on-device learning.
 *
 * Backs up current weights before applying updates. If validation loss
 * exceeds baseline x threshold, restores the previous weights.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>

/* ============================================================================
 * nimcp_rollback_init
 * ============================================================================ */

int nimcp_rollback_init(
    nimcp_rollback_state_t* state,
    const float* current_weights,
    uint32_t num_weights,
    float baseline_loss,
    uint32_t validation_steps,
    float threshold)
{
    if (!state || !current_weights || num_weights == 0) {
        return -1;
    }

    /* Overflow check: num_weights * sizeof(float) */
    if (num_weights > UINT32_MAX / sizeof(float)) {
        return -1;
    }

    /* Copy current weights to backup */
    state->previous_weights = (float*)nimcp_malloc(num_weights * sizeof(float));
    if (!state->previous_weights) {
        return -1;
    }
    memcpy(state->previous_weights, current_weights, num_weights * sizeof(float));

    state->num_weights = num_weights;
    state->previous_version = 0;
    state->baseline_loss = baseline_loss;
    state->validation_steps = validation_steps;
    state->rollback_threshold = threshold;
    state->steps_evaluated = 0;
    state->running_loss = 0.0f;
    state->rollback_triggered = false;
    state->active = true;

    return 0;
}

/* ============================================================================
 * nimcp_rollback_check_step
 * ============================================================================ */

int nimcp_rollback_check_step(nimcp_rollback_state_t* state, float step_loss) {
    if (!state || !state->active) {
        return -1;
    }

    state->steps_evaluated++;
    state->running_loss += step_loss;

    /* Check if we've completed all validation steps */
    if (state->steps_evaluated >= state->validation_steps) {
        float avg_loss = state->running_loss / (float)state->steps_evaluated;
        float threshold_loss = state->baseline_loss * state->rollback_threshold;

        if (avg_loss > threshold_loss) {
            state->rollback_triggered = true;
            return 1; /* Signal: rollback needed */
        }

        /* Validation passed — update baseline to new average */
        state->baseline_loss = avg_loss;
        state->steps_evaluated = 0;
        state->running_loss = 0.0f;
    }

    return 0; /* No rollback needed */
}

/* ============================================================================
 * nimcp_rollback_execute
 * ============================================================================ */

int nimcp_rollback_execute(nimcp_rollback_state_t* state, float* weights) {
    if (!state || !weights || !state->active || !state->previous_weights) {
        return -1;
    }

    /* Restore previous weights */
    memcpy(weights, state->previous_weights, state->num_weights * sizeof(float));

    /* Reset evaluation counters */
    state->steps_evaluated = 0;
    state->running_loss = 0.0f;
    state->rollback_triggered = false;

    return 0;
}

/* ============================================================================
 * nimcp_rollback_cleanup
 * ============================================================================ */

void nimcp_rollback_cleanup(nimcp_rollback_state_t* state) {
    if (!state) {
        return;
    }

    if (state->previous_weights) {
        nimcp_free(state->previous_weights);
        state->previous_weights = NULL;
    }

    state->active = false;
    state->num_weights = 0;
}
