/**
 * @file nimcp_self_curriculum.c
 * @brief Self-Generated Curriculum — curiosity-driven training item creation
 *
 * The brain monitors per-domain uncertainty (EMA of loss by label prefix)
 * and generates new training items for weak domains by:
 *   1. Selecting the top-3 most uncertain domains
 *   2. Building domain-specific probe vectors
 *   3. Running brain inference on the probe
 *   4. Blending the output with noise to create a training target
 *   5. Storing the (probe, target, label) triple for later consumption
 */

#include "cognitive/nimcp_self_curriculum.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/kg/nimcp_wave13_metacog_kg.h"  /* W13: curriculum events */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "self_curriculum"

/* EMA decay factor for domain uncertainty tracking */
#define EMA_ALPHA  0.1f

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_self_curriculum {
    nimcp_self_curriculum_config_t config;

    /* Domain uncertainty tracking */
    char     domain_names[NIMCP_SC_MAX_DOMAINS][NIMCP_SC_LABEL_LEN];
    float    domain_uncertainty[NIMCP_SC_MAX_DOMAINS];
    uint32_t domain_count;

    /* Generated item ring */
    nimcp_sc_item_t* generated_items;
    uint32_t         generated_count;
    uint32_t         generated_head;   /* next item to pop */
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

/**
 * Extract domain from a label by taking everything before the first '_'.
 * "ethics_trolley" → "ethics".  If no '_', the whole label is the domain.
 */
static void extract_domain(const char* label, char* domain, uint32_t domain_size)
{
    if (!label || !domain || domain_size == 0) {
        if (domain && domain_size > 0) domain[0] = '\0';
        return;
    }

    const char* underscore = strchr(label, '_');
    if (underscore) {
        uint32_t len = (uint32_t)(underscore - label);
        if (len >= domain_size) len = domain_size - 1;
        memcpy(domain, label, len);
        domain[len] = '\0';
    } else {
        uint32_t len = (uint32_t)strlen(label);
        if (len >= domain_size) len = domain_size - 1;
        memcpy(domain, label, len);
        domain[len] = '\0';
    }
}

/**
 * Find or create a domain slot.  Returns index, or -1 if full.
 */
static int find_or_create_domain(nimcp_self_curriculum_t* h, const char* domain)
{
    for (uint32_t i = 0; i < h->domain_count; i++) {
        if (strcmp(h->domain_names[i], domain) == 0) {
            return (int)i;
        }
    }
    if (h->domain_count >= NIMCP_SC_MAX_DOMAINS) {
        return -1;
    }
    uint32_t idx = h->domain_count++;
    strncpy(h->domain_names[idx], domain, NIMCP_SC_LABEL_LEN - 1);
    h->domain_names[idx][NIMCP_SC_LABEL_LEN - 1] = '\0';
    h->domain_uncertainty[idx] = 0.0f;
    return (int)idx;
}

/**
 * Simple deterministic hash-based pseudo-random float in [0, 1).
 * Not cryptographic — just enough for noise injection.
 */
static float pseudo_rand(uint32_t seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return (float)(seed & 0x7FFFFF) / (float)0x800000;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_self_curriculum_config_t nimcp_self_curriculum_config_default(void)
{
    nimcp_self_curriculum_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.generation_interval   = 50;
    cfg.max_generated         = 10;
    cfg.uncertainty_threshold = 0.5f;
    cfg.imagination_steps     = 3;
    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_self_curriculum_t* nimcp_self_curriculum_create(
    const nimcp_self_curriculum_config_t* config)
{
    nimcp_self_curriculum_t* h = (nimcp_self_curriculum_t*)nimcp_calloc(
        1, sizeof(nimcp_self_curriculum_t));
    if (!h) {
        return NULL;
    }

    if (config) {
        h->config = *config;
    } else {
        h->config = nimcp_self_curriculum_config_default();
    }

    /* Ensure sane max_generated */
    if (h->config.max_generated == 0) {
        h->config.max_generated = 10;
    }

    h->generated_items = (nimcp_sc_item_t*)nimcp_calloc(
        h->config.max_generated, sizeof(nimcp_sc_item_t));
    if (!h->generated_items) {
        nimcp_free(h);
        return NULL;
    }

    return h;
}

void nimcp_self_curriculum_destroy(nimcp_self_curriculum_t* handle)
{
    if (!handle) {
        return;
    }
    if (handle->generated_items) {
        nimcp_free(handle->generated_items);
    }
    nimcp_free(handle);
}

/* ============================================================================
 * Uncertainty Tracking
 * ============================================================================ */

int nimcp_self_curriculum_update_uncertainty(
    nimcp_self_curriculum_t* handle,
    const char* label,
    float loss)
{
    if (!handle || !label) {
        return -1;
    }

    char domain[NIMCP_SC_LABEL_LEN];
    extract_domain(label, domain, sizeof(domain));

    int idx = find_or_create_domain(handle, domain);
    if (idx < 0) {
        return -1;  /* domain table full */
    }

    /* Exponential moving average of loss */
    float prev = handle->domain_uncertainty[idx];
    handle->domain_uncertainty[idx] = prev * (1.0f - EMA_ALPHA) + loss * EMA_ALPHA;

    return 0;
}

/* ============================================================================
 * Generation Gate
 * ============================================================================ */

bool nimcp_self_curriculum_should_generate(
    const nimcp_self_curriculum_t* handle,
    uint64_t step)
{
    if (!handle || handle->config.generation_interval == 0) {
        return false;
    }
    if (step % handle->config.generation_interval != 0) {
        return false;
    }

    /* At least one domain must exceed the threshold */
    for (uint32_t i = 0; i < handle->domain_count; i++) {
        if (handle->domain_uncertainty[i] > handle->config.uncertainty_threshold) {
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Curriculum Generation
 * ============================================================================ */

int nimcp_self_curriculum_generate(
    nimcp_self_curriculum_t* handle,
    brain_t brain)
{
    if (!handle || !brain) {
        return -1;
    }
    if (handle->domain_count == 0) {
        return 0;
    }

    /* Reset generated queue */
    handle->generated_count = 0;
    handle->generated_head  = 0;

    /* --- Step 1: Find top-3 most uncertain domains --- */
    uint32_t top_indices[3] = {0, 0, 0};
    float    top_vals[3]    = {-1.0f, -1.0f, -1.0f};

    for (uint32_t i = 0; i < handle->domain_count; i++) {
        float u = handle->domain_uncertainty[i];
        if (u <= handle->config.uncertainty_threshold) {
            continue;  /* only consider uncertain domains */
        }
        for (int k = 0; k < 3; k++) {
            if (u > top_vals[k]) {
                /* Shift down */
                for (int j = 2; j > k; j--) {
                    top_indices[j] = top_indices[j - 1];
                    top_vals[j]    = top_vals[j - 1];
                }
                top_indices[k] = i;
                top_vals[k]    = u;
                break;
            }
        }
    }

    /* Count how many valid top domains we found */
    uint32_t n_top = 0;
    for (int k = 0; k < 3; k++) {
        if (top_vals[k] > 0.0f) n_top++;
    }
    if (n_top == 0) {
        return 0;
    }

    /* --- Step 2-4: For each top domain, generate items --- */
    uint32_t items_per_domain = handle->config.max_generated / n_top;
    if (items_per_domain == 0) items_per_domain = 1;

    int total_generated = 0;

    for (uint32_t t = 0; t < n_top; t++) {
        uint32_t d_idx = top_indices[t];
        const char* domain = handle->domain_names[d_idx];

        for (uint32_t item = 0; item < items_per_domain; item++) {
            if (handle->generated_count >= handle->config.max_generated) {
                break;
            }

            nimcp_sc_item_t* gi = &handle->generated_items[handle->generated_count];
            memset(gi, 0, sizeof(*gi));

            /* Build domain-specific probe: seed vector based on domain hash */
            uint32_t domain_hash = 5381;
            for (const char* p = domain; *p; p++) {
                domain_hash = ((domain_hash << 5) + domain_hash) + (uint32_t)*p;
            }
            uint32_t seed = domain_hash ^ (item * 0x9E3779B9u);

            for (uint32_t f = 0; f < NIMCP_SC_FEATURE_DIM; f++) {
                gi->features[f] = pseudo_rand(seed + f) * 2.0f - 1.0f;
            }

            /* Run brain inference on probe to get the brain's current "opinion" */
            float* output_buf = (float*)nimcp_calloc(NIMCP_SC_TARGET_DIM, sizeof(float));
            if (!output_buf) {
                break;
            }

            /* Use brain_decide to get output.  If it fails, fall back to
             * using the probe itself as a base for the target. */
            brain_decision_t* decision = brain_decide(
                brain, gi->features, NIMCP_SC_FEATURE_DIM);

            if (decision && decision->output_vector &&
                decision->output_size > 0) {
                uint32_t copy_dim = decision->output_size;
                if (copy_dim > NIMCP_SC_TARGET_DIM) copy_dim = NIMCP_SC_TARGET_DIM;
                memcpy(output_buf, decision->output_vector, copy_dim * sizeof(float));
                brain_free_decision(decision);
            } else {
                if (decision) brain_free_decision(decision);
                /* Fallback: copy features as base target */
                uint32_t copy_dim = NIMCP_SC_FEATURE_DIM < NIMCP_SC_TARGET_DIM
                    ? NIMCP_SC_FEATURE_DIM : NIMCP_SC_TARGET_DIM;
                memcpy(output_buf, gi->features, copy_dim * sizeof(float));
            }

            /* Imagination: blend output with noise over several steps */
            for (uint32_t step = 0; step < handle->config.imagination_steps; step++) {
                float noise_scale = 0.1f * expf(-(float)step);  /* exponential noise decay */
                uint32_t step_seed = seed ^ ((step + 1) * 0xDEADBEEFu);
                for (uint32_t f = 0; f < NIMCP_SC_TARGET_DIM; f++) {
                    float noise = (pseudo_rand(step_seed + f) * 2.0f - 1.0f) * noise_scale;
                    output_buf[f] += noise;
                }
            }

            /* Store target */
            memcpy(gi->target, output_buf, NIMCP_SC_TARGET_DIM * sizeof(float));
            nimcp_free(output_buf);

            /* Label */
            snprintf(gi->label, NIMCP_SC_LABEL_LEN, "self_curriculum_%s", domain);

            handle->generated_count++;
            total_generated++;
        }
    }

    /* W13: emit curriculum-advancement event to KG with top uncertainty. */
    if (total_generated > 0 && n_top > 0) {
        char top_domain[NIMCP_SC_LABEL_LEN] = {0};
        /* Reuse first top index to report a representative domain. */
        if (top_indices[0] < handle->domain_count) {
            strncpy(top_domain, handle->domain_names[top_indices[0]],
                    NIMCP_SC_LABEL_LEN - 1);
        }
        wave13_curriculum_emit_item_generated(brain, top_domain,
                                              (uint32_t)total_generated);
        wave13_curriculum_emit_uncertainty_update(brain, top_domain,
                                                  top_vals[0]);
    }

    return total_generated;
}

/* ============================================================================
 * Item Retrieval
 * ============================================================================ */

int nimcp_self_curriculum_get_next_item(
    nimcp_self_curriculum_t* handle,
    float* features_out, uint32_t feat_dim,
    float* target_out,   uint32_t tgt_dim,
    char*  label_out)
{
    if (!handle) {
        return -1;
    }
    if (handle->generated_head >= handle->generated_count) {
        return -1;  /* queue empty */
    }

    const nimcp_sc_item_t* item = &handle->generated_items[handle->generated_head];
    handle->generated_head++;

    if (features_out && feat_dim > 0) {
        uint32_t copy = feat_dim < NIMCP_SC_FEATURE_DIM ? feat_dim : NIMCP_SC_FEATURE_DIM;
        memcpy(features_out, item->features, copy * sizeof(float));
        /* Zero-fill remainder */
        if (feat_dim > NIMCP_SC_FEATURE_DIM) {
            memset(features_out + NIMCP_SC_FEATURE_DIM, 0,
                   (feat_dim - NIMCP_SC_FEATURE_DIM) * sizeof(float));
        }
    }
    if (target_out && tgt_dim > 0) {
        uint32_t copy = tgt_dim < NIMCP_SC_TARGET_DIM ? tgt_dim : NIMCP_SC_TARGET_DIM;
        memcpy(target_out, item->target, copy * sizeof(float));
        if (tgt_dim > NIMCP_SC_TARGET_DIM) {
            memset(target_out + NIMCP_SC_TARGET_DIM, 0,
                   (tgt_dim - NIMCP_SC_TARGET_DIM) * sizeof(float));
        }
    }
    if (label_out) {
        strncpy(label_out, item->label, NIMCP_SC_LABEL_LEN - 1);
        label_out[NIMCP_SC_LABEL_LEN - 1] = '\0';
    }

    return 0;
}

/* ============================================================================
 * Query
 * ============================================================================ */

int nimcp_self_curriculum_get_most_uncertain_domain(
    const nimcp_self_curriculum_t* handle,
    char* domain_name_out,
    float* uncertainty_out)
{
    if (!handle || handle->domain_count == 0) {
        return -1;
    }

    uint32_t best_idx = 0;
    float    best_val = handle->domain_uncertainty[0];

    for (uint32_t i = 1; i < handle->domain_count; i++) {
        if (handle->domain_uncertainty[i] > best_val) {
            best_val = handle->domain_uncertainty[i];
            best_idx = i;
        }
    }

    if (domain_name_out) {
        strncpy(domain_name_out, handle->domain_names[best_idx], NIMCP_SC_LABEL_LEN - 1);
        domain_name_out[NIMCP_SC_LABEL_LEN - 1] = '\0';
    }
    if (uncertainty_out) {
        *uncertainty_out = best_val;
    }

    return 0;
}
