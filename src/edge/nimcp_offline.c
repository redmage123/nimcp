/**
 * @file nimcp_offline.c
 * @brief Offline degradation policy for edge devices operating without
 *        connectivity to the master brain.
 *
 * As time since last sync increases, the device transitions through
 * progressively more cautious operating modes with decaying confidence.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include <string.h>

/* ============================================================================
 * nimcp_offline_policy_init
 * ============================================================================ */

int nimcp_offline_policy_init(nimcp_offline_policy_t* policy) {
    if (!policy) {
        return -1;
    }

    memset(policy, 0, sizeof(*policy));

    policy->last_sync_timestamp = 0;
    policy->steps_since_sync = 0;
    policy->confidence_decay_rate = 0.9999f;
    policy->min_confidence_multiplier = 0.5f;
    policy->current_confidence = 1.0f;

    policy->cautious_after_steps = 1000;
    policy->conservative_after_steps = 5000;
    policy->frozen_after_steps = 20000;
    policy->current_mode = NIMCP_OFFLINE_NORMAL;

    return 0;
}

/* ============================================================================
 * nimcp_offline_policy_step
 * ============================================================================ */

int nimcp_offline_policy_step(nimcp_offline_policy_t* policy) {
    if (!policy) {
        return -1;
    }

    policy->steps_since_sync++;

    /* Update mode based on steps since last sync */
    if (policy->steps_since_sync >= policy->frozen_after_steps) {
        policy->current_mode = NIMCP_OFFLINE_FROZEN;
    } else if (policy->steps_since_sync >= policy->conservative_after_steps) {
        policy->current_mode = NIMCP_OFFLINE_CONSERVATIVE;
    } else if (policy->steps_since_sync >= policy->cautious_after_steps) {
        policy->current_mode = NIMCP_OFFLINE_CAUTIOUS;
    } else {
        policy->current_mode = NIMCP_OFFLINE_NORMAL;
    }

    /* Decay confidence, clamped to minimum */
    policy->current_confidence *= policy->confidence_decay_rate;
    if (policy->current_confidence < policy->min_confidence_multiplier) {
        policy->current_confidence = policy->min_confidence_multiplier;
    }

    return 0;
}

/* ============================================================================
 * nimcp_offline_policy_on_sync
 * ============================================================================ */

void nimcp_offline_policy_on_sync(nimcp_offline_policy_t* policy) {
    if (!policy) {
        return;
    }

    policy->steps_since_sync = 0;
    policy->current_confidence = 1.0f;
    policy->current_mode = NIMCP_OFFLINE_NORMAL;
}

/* ============================================================================
 * nimcp_offline_get_confidence
 * ============================================================================ */

float nimcp_offline_get_confidence(const nimcp_offline_policy_t* policy) {
    if (!policy) {
        return 0.0f;
    }
    return policy->current_confidence;
}

/* ============================================================================
 * nimcp_offline_get_lr_scale
 * ============================================================================ */

float nimcp_offline_get_lr_scale(const nimcp_offline_policy_t* policy) {
    if (!policy) {
        return 0.0f;
    }

    switch (policy->current_mode) {
        case NIMCP_OFFLINE_NORMAL:
            return 1.0f;
        case NIMCP_OFFLINE_CAUTIOUS:
            return 0.5f;
        case NIMCP_OFFLINE_CONSERVATIVE:
            return 0.1f;
        case NIMCP_OFFLINE_FROZEN:
            return 0.0f;  /* No learning when frozen */
        default:
            return 1.0f;
    }
}
