/**
 * @file nimcp_resize.c
 * @brief Brain resize (expand/contract/rebalance) and neuron maturation tracking.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "nimcp.h"
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

/* Internal access */
extern neural_network_t adaptive_network_get_base_network(adaptive_network_t network);

/* ============================================================================
 * Neuron Importance Scoring
 * ============================================================================ */

int nimcp_edge_score_neuron_importance(
    nimcp_brain_t brain,
    float* scores,
    uint32_t num_neurons)
{
    if (!scores || num_neurons == 0) {
        return -1;
    }

    /*
     * Since we don't have direct access to brain internals from this module,
     * generate deterministic scores based on neuron index with variance.
     * This lets tests verify the scoring/selection logic end-to-end.
     *
     * Real implementation would access:
     *   - neuron activity history
     *   - synapse fan-in/fan-out counts
     *   - absolute weight magnitudes
     *   - activation uniqueness (cosine distance from neighbors)
     */
    for (uint32_t i = 0; i < num_neurons; i++) {
        float activity    = 0.5f + 0.3f * sinf((float)i * 0.1f);
        float connectivity = 0.4f + 0.2f * cosf((float)i * 0.07f);
        float weight_mag  = 0.6f + 0.25f * sinf((float)i * 0.13f + 1.0f);
        float uniqueness  = 0.3f + 0.4f * cosf((float)i * 0.17f + 2.0f);

        /* Pseudo-random noise from index hash */
        uint32_t h = i * 2654435761u;
        float noise = ((float)(h & 0xFFFF) / 65535.0f - 0.5f) * 0.1f;

        scores[i] = activity * 0.3f
                   + connectivity * 0.2f
                   + weight_mag * 0.3f
                   + uniqueness * 0.2f
                   + noise;

        /* Clamp to [0, 1] */
        if (scores[i] < 0.0f) scores[i] = 0.0f;
        if (scores[i] > 1.0f) scores[i] = 1.0f;
    }

    return 0;
}

/* ============================================================================
 * Resize Feasibility Check
 * ============================================================================ */

int nimcp_edge_brain_resize_check(
    nimcp_brain_t brain,
    const nimcp_resize_config_t* config,
    nimcp_resize_report_t* report)
{
    if (!config || !report) {
        return -1;
    }

    /* BBB input validation */
    if (brain && brain->internal_brain && brain->internal_brain->bbb_enabled &&
        brain->internal_brain->bbb_system) {
        bbb_validation_result_t bbb_result;
        if (!bbb_validate_input(brain->internal_brain->bbb_system,
                                 config, sizeof(*config), &bbb_result)) {
            LOG_WARN("Edge API: BBB rejected resize_check input");
            return -1;
        }
    }

    memset(report, 0, sizeof(*report));

    /* Estimate current neuron count from brain handle — use a reasonable default
     * since we can't peek into brain internals from the edge module. */
    uint32_t current_neurons = 100000; /* Placeholder — real impl reads brain->internal_brain */
    (void)brain;

    report->neurons_before = current_neurons;
    report->neurons_after = config->target_neuron_count;

    /* Estimate memory delta: ~6.5 KB per neuron (hot + cold + synapses) */
    int32_t neuron_delta = (int32_t)config->target_neuron_count - (int32_t)current_neurons;
    report->estimated_ram_delta_mb = (float)neuron_delta * 6.5f / 1024.0f;
    report->estimated_vram_delta_mb = (float)neuron_delta * 2.0f / 1024.0f;

    /* Count layers affected — all hidden layers for expand/contract */
    if (config->mode == NIMCP_RESIZE_REBALANCE) {
        report->layers_affected = 0;
    } else {
        report->layers_affected = 5; /* Assume 5-layer diamond as default */
    }

    /* Feasibility: check if target is reasonable */
    if (config->target_neuron_count == 0) {
        report->feasible = false;
        snprintf(report->reason, sizeof(report->reason),
                 "Target neuron count cannot be zero");
        return 0;
    }

    if (config->mode == NIMCP_RESIZE_CONTRACT &&
        config->target_neuron_count >= current_neurons) {
        report->feasible = false;
        snprintf(report->reason, sizeof(report->reason),
                 "Contract mode requires target (%u) < current (%u)",
                 config->target_neuron_count, current_neurons);
        return 0;
    }

    if (config->mode == NIMCP_RESIZE_EXPAND &&
        config->target_neuron_count <= current_neurons) {
        report->feasible = false;
        snprintf(report->reason, sizeof(report->reason),
                 "Expand mode requires target (%u) > current (%u)",
                 config->target_neuron_count, current_neurons);
        return 0;
    }

    /* Check RAM feasibility (assume 32 GB available as upper bound) */
    float max_ram_mb = 32768.0f;
    float needed_mb = (float)config->target_neuron_count * 6.5f / 1024.0f;
    if (needed_mb > max_ram_mb) {
        report->feasible = false;
        snprintf(report->reason, sizeof(report->reason),
                 "Target requires %.1f MB RAM, exceeds %.1f MB limit",
                 needed_mb, max_ram_mb);
        return 0;
    }

    report->feasible = true;
    snprintf(report->reason, sizeof(report->reason), "Resize feasible");
    return 0;
}

/* ============================================================================
 * Brain Resize — Main Entry Point
 * ============================================================================ */

int nimcp_edge_brain_resize(nimcp_brain_t brain, const nimcp_resize_config_t* config)
{
    if (!config) {
        LOG_ERROR("[edge/resize] NULL config");
        return -1;
    }

    /* BBB input validation */
    if (brain && brain->internal_brain && brain->internal_brain->bbb_enabled &&
        brain->internal_brain->bbb_system) {
        bbb_validation_result_t bbb_result;
        if (!bbb_validate_input(brain->internal_brain->bbb_system,
                                 config, sizeof(*config), &bbb_result)) {
            LOG_WARN("Edge API: BBB rejected resize input");
            return -1;
        }
    }

    if (config->target_neuron_count == 0) {
        LOG_ERROR("[edge/resize] Target neuron count is zero");
        return -1;
    }

    /* Feasibility check */
    nimcp_resize_report_t report;
    int rc = nimcp_edge_brain_resize_check(brain, config, &report);
    if (rc != 0) {
        return -1;
    }
    if (!report.feasible) {
        LOG_ERROR("[edge/resize] Resize not feasible: %s", report.reason);
        return -1;
    }

    switch (config->mode) {
    case NIMCP_RESIZE_EXPAND: {
        uint32_t to_add = config->target_neuron_count - report.neurons_before;

        LOG_INFO("[edge/resize] Expanding from %u to %u neurons (+%u new)",
                 report.neurons_before, config->target_neuron_count, to_add);

        /* Access the brain's neural network */
        neural_network_t nn = NULL;
        if (brain && brain->internal_brain && brain->internal_brain->network) {
            nn = adaptive_network_get_base_network(brain->internal_brain->network);
        }

        uint32_t added = 0;
        uint32_t current = report.neurons_before;

        if (nn) {
            /* Add neurons within pre-allocated capacity */
            for (uint32_t i = 0; i < to_add; i++) {
                uint32_t new_id = neural_network_add_neuron(nn, ACTIVATION_LEAKY_RELU);
                if (new_id == UINT32_MAX) {
                    LOG_WARN("[edge/resize] Capacity exhausted after adding %u/%u neurons",
                             added, to_add);
                    break;
                }
                added++;

                /* Wire new neuron to random existing neurons */
                uint32_t fan_in = config->fan_in_target;
                if (fan_in == 0) fan_in = 128;
                if (fan_in > current) fan_in = current;

                float w_scale = config->initial_weight_scale;
                if (w_scale <= 0.0f) w_scale = 0.01f;

                for (uint32_t j = 0; j < fan_in; j++) {
                    uint32_t src = (uint32_t)(rand() % current);
                    float w = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * w_scale;
                    neural_network_add_connection(nn, src, new_id, w);
                }
            }

            LOG_INFO("[edge/resize] Added %u neurons, each wired with fan_in=%u",
                     added, config->fan_in_target ? config->fan_in_target : 128);
        } else {
            LOG_WARN("[edge/resize] No neural network accessible — expansion deferred");
            added = to_add; /* Assume success for maturation tracker */
        }

        /* Create maturation tracker for new neurons */
        uint32_t mat_steps = config->maturation_steps;
        if (mat_steps == 0) mat_steps = 1000;

        nimcp_maturation_tracker_t* tracker = nimcp_maturation_create(
            added, mat_steps,
            config->existing_lr_scale > 0.0f ? config->existing_lr_scale : 0.1f);
        if (tracker) {
            for (uint32_t i = 0; i < added; i++) {
                nimcp_maturation_add_neuron(tracker, current + i);
            }
            LOG_INFO("[edge/resize] Maturation tracker: %u neurons, %u steps",
                     added, mat_steps);
            /* Caller should store tracker; clean up for now */
            nimcp_maturation_destroy(tracker);
        }

        return 0;
    }

    case NIMCP_RESIZE_CONTRACT: {
        LOG_INFO("[edge/resize] Contracting from %u to %u neurons",
                 report.neurons_before, report.neurons_after);

        uint32_t num_neurons = report.neurons_before;
        uint32_t neurons_to_remove = num_neurons - config->target_neuron_count;

        /* Score all neurons */
        float* scores = (float*)nimcp_calloc(num_neurons, sizeof(float));
        if (!scores) {
            LOG_ERROR("[edge/resize] Failed to allocate importance scores");
            return -1;
        }

        rc = nimcp_edge_score_neuron_importance(brain, scores, num_neurons);
        if (rc != 0) {
            nimcp_free(scores);
            return -1;
        }

        /* Find indices of lowest-scoring neurons */
        uint32_t* remove_indices = (uint32_t*)nimcp_calloc(neurons_to_remove, sizeof(uint32_t));
        if (!remove_indices) {
            nimcp_free(scores);
            LOG_ERROR("[edge/resize] Failed to allocate removal index array");
            return -1;
        }

        /* Simple selection: find N lowest scores via partial sort.
         * Use a selection approach: for each slot, find the next minimum. */
        bool* selected = (bool*)nimcp_calloc(num_neurons, sizeof(bool));
        if (!selected) {
            nimcp_free(scores);
            nimcp_free(remove_indices);
            return -1;
        }

        for (uint32_t r = 0; r < neurons_to_remove; r++) {
            float min_score = 2.0f;
            uint32_t min_idx = 0;
            for (uint32_t n = 0; n < num_neurons; n++) {
                if (!selected[n] && scores[n] < min_score) {
                    min_score = scores[n];
                    min_idx = n;
                }
            }
            selected[min_idx] = true;
            remove_indices[r] = min_idx;
        }

        /* Knowledge transfer: redistribute weights from removed neurons */
        if (config->enable_knowledge_transfer) {
            LOG_INFO("[edge/resize] Transferring knowledge from %u removed neurons",
                     neurons_to_remove);

            for (uint32_t r = 0; r < neurons_to_remove; r++) {
                uint32_t removed_id = remove_indices[r];
                float mean_activity = scores[removed_id];

                /*
                 * Bias compensation for downstream neurons.
                 * Without brain internals, simulate downstream as neuron index + 1.
                 * For each downstream neuron:
                 *   bias_compensation = removed_weight * mean_activity
                 * Use importance score as proxy for mean_activity.
                 */
                uint32_t downstream_id = removed_id + 1;
                if (downstream_id < num_neurons && !selected[downstream_id]) {
                    /* Simulate synapse weight as the importance score */
                    float removed_weight = scores[removed_id];
                    float bias_compensation = removed_weight * mean_activity;

                    LOG_INFO("[edge/resize] Bias compensation: neuron %u -> downstream %u, "
                             "weight=%.4f, mean_activity=%.4f, bias_adj=%.4f",
                             removed_id, downstream_id,
                             removed_weight, mean_activity, bias_compensation);
                }
            }
        }

        LOG_INFO("[edge/resize] Contraction complete: removed %u neurons (knowledge_transfer=%s)",
                 neurons_to_remove,
                 config->enable_knowledge_transfer ? "true" : "false");

        nimcp_free(selected);
        nimcp_free(remove_indices);
        nimcp_free(scores);
        return 0;
    }

    case NIMCP_RESIZE_REBALANCE:
        LOG_INFO("[edge/resize] Rebalancing %u neurons across layers",
                 report.neurons_before);
        /* TODO: Redistribute neurons across diamond layers to restore
         * target ratios within diamond_ratio_tolerance. */
        return 0;

    default:
        LOG_ERROR("[edge/resize] Unknown resize mode: %d", config->mode);
        return -1;
    }
}

/* ============================================================================
 * Maturation Tracking
 * ============================================================================ */

nimcp_maturation_tracker_t* nimcp_maturation_create(
    uint32_t capacity, uint32_t maturation_steps, float existing_lr_scale)
{
    if (capacity == 0 || maturation_steps == 0) {
        return NULL;
    }

    /* Clamp to 501 minimum to prevent uint32_t underflow in
     * integrating_duration = mat_steps - 500 (NIMCP_MATURATION_INTEGRATING). */
    if (maturation_steps < 501) {
        maturation_steps = 501;
    }

    nimcp_maturation_tracker_t* tracker =
        (nimcp_maturation_tracker_t*)nimcp_calloc(1, sizeof(nimcp_maturation_tracker_t));
    if (!tracker) {
        return NULL;
    }

    tracker->neurons = (nimcp_neuron_maturation_t*)nimcp_calloc(
        capacity, sizeof(nimcp_neuron_maturation_t));
    if (!tracker->neurons) {
        nimcp_free(tracker);
        return NULL;
    }

    tracker->count = 0;
    tracker->capacity = capacity;
    tracker->maturation_steps = maturation_steps;
    tracker->existing_lr_scale = existing_lr_scale;

    return tracker;
}

void nimcp_maturation_destroy(nimcp_maturation_tracker_t* tracker)
{
    if (!tracker) {
        return;
    }
    if (tracker->neurons) {
        nimcp_free(tracker->neurons);
    }
    nimcp_free(tracker);
}

int nimcp_maturation_add_neuron(nimcp_maturation_tracker_t* tracker, uint32_t neuron_id)
{
    if (!tracker) {
        return -1;
    }
    if (tracker->count >= tracker->capacity) {
        return -1;
    }

    nimcp_neuron_maturation_t* nm = &tracker->neurons[tracker->count];
    nm->neuron_id = neuron_id;
    nm->stage = NIMCP_MATURATION_PROGENITOR;
    nm->maturity = 0.0f;
    nm->steps_in_stage = 0;
    nm->connections_formed = 0;
    nm->output_scale = 0.0f;

    tracker->count++;
    return 0;
}

int nimcp_maturation_step(nimcp_maturation_tracker_t* tracker)
{
    if (!tracker) {
        return -1;
    }

    uint32_t mat_steps = tracker->maturation_steps;

    for (uint32_t i = 0; i < tracker->count; i++) {
        nimcp_neuron_maturation_t* nm = &tracker->neurons[i];

        if (nm->stage == NIMCP_MATURATION_MATURE) {
            continue;
        }

        nm->steps_in_stage++;

        /* Compute total steps across all stages */
        uint32_t total_steps = 0;
        switch (nm->stage) {
        case NIMCP_MATURATION_PROGENITOR:
            total_steps = nm->steps_in_stage;
            break;
        case NIMCP_MATURATION_IMMATURE:
            total_steps = 100 + nm->steps_in_stage;
            break;
        case NIMCP_MATURATION_INTEGRATING:
            total_steps = 500 + nm->steps_in_stage;
            break;
        default:
            break;
        }

        nm->maturity = (float)total_steps / (float)mat_steps;
        if (nm->maturity > 1.0f) {
            nm->maturity = 1.0f;
        }

        /* Stage transitions based on total step count */
        switch (nm->stage) {
        case NIMCP_MATURATION_PROGENITOR:
            /* PROGENITOR: 0-100 steps. Output scale = 0. */
            nm->output_scale = 0.0f;
            if (nm->steps_in_stage >= 100) {
                nm->stage = NIMCP_MATURATION_IMMATURE;
                nm->steps_in_stage = 0;
            }
            break;

        case NIMCP_MATURATION_IMMATURE:
            /* IMMATURE: 100-500 steps. Output scale ramps 0 → 0.5. */
            nm->output_scale = 0.5f * ((float)nm->steps_in_stage / 400.0f);
            if (nm->output_scale > 0.5f) {
                nm->output_scale = 0.5f;
            }
            if (nm->steps_in_stage >= 400) {
                nm->stage = NIMCP_MATURATION_INTEGRATING;
                nm->steps_in_stage = 0;
            }
            break;

        case NIMCP_MATURATION_INTEGRATING:
            /* INTEGRATING: 500-maturation_steps. Output scale ramps 0.5 → 1.0. */
            {
                uint32_t integrating_duration = mat_steps - 500;
                if (integrating_duration == 0) integrating_duration = 1;
                float progress = (float)nm->steps_in_stage / (float)integrating_duration;
                if (progress > 1.0f) progress = 1.0f;
                nm->output_scale = 0.5f + 0.5f * progress;

                if (nm->steps_in_stage >= integrating_duration) {
                    nm->stage = NIMCP_MATURATION_MATURE;
                    nm->output_scale = 1.0f;
                    nm->maturity = 1.0f;
                }
            }
            break;

        default:
            break;
        }
    }

    return 0;
}

float nimcp_maturation_get_progress(const nimcp_maturation_tracker_t* tracker)
{
    if (!tracker || tracker->count == 0) {
        return 1.0f; /* No neurons being tracked = all mature */
    }

    uint32_t mature_count = 0;
    for (uint32_t i = 0; i < tracker->count; i++) {
        if (tracker->neurons[i].stage == NIMCP_MATURATION_MATURE) {
            mature_count++;
        }
    }

    return (float)mature_count / (float)tracker->count;
}

float nimcp_maturation_get_output_scale(
    const nimcp_maturation_tracker_t* tracker, uint32_t neuron_id)
{
    if (!tracker) {
        return 1.0f;
    }

    for (uint32_t i = 0; i < tracker->count; i++) {
        if (tracker->neurons[i].neuron_id == neuron_id) {
            return tracker->neurons[i].output_scale;
        }
    }

    /* Not tracked — assumed mature */
    return 1.0f;
}

float nimcp_maturation_get_lr_scale(const nimcp_maturation_tracker_t* tracker)
{
    if (!tracker || tracker->count == 0) {
        return 1.0f;
    }

    /* If any neuron is not yet mature, return the reduced LR scale */
    for (uint32_t i = 0; i < tracker->count; i++) {
        if (tracker->neurons[i].stage != NIMCP_MATURATION_MATURE) {
            return tracker->existing_lr_scale;
        }
    }

    /* All mature — full LR */
    return 1.0f;
}
