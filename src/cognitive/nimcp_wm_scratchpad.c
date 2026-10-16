/**
 * @file nimcp_wm_scratchpad.c
 * @brief Working Memory Scratchpad — persistent labeled slots with temporal
 *        decay for multi-step reasoning. Brain can write intermediate results
 *        and read them back.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "WM_SCRATCHPAD"

#include "cognitive/nimcp_wm_scratchpad.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

typedef struct {
    float  data[NIMCP_WMS_MAX_SLOT_DIM];
    char   label[NIMCP_WMS_LABEL_LEN];
    uint32_t data_dim;     /* Actual dimension written */
    bool   written;
    uint32_t age;          /* Number of decay calls since last write */
} nimcp_wms_slot_t;

struct nimcp_wm_scratchpad {
    nimcp_wms_config_t config;
    nimcp_wms_slot_t*  slots;
    uint32_t           num_slots;
};

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_wms_config_t nimcp_wms_config_default(void)
{
    nimcp_wms_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_slots  = 8;
    cfg.slot_dim   = 512;
    cfg.decay_rate = 0.95f;
    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_wm_scratchpad_t* nimcp_wms_create(const nimcp_wms_config_t* config)
{
    nimcp_wm_scratchpad_t* wms = (nimcp_wm_scratchpad_t*)nimcp_calloc(
        1, sizeof(nimcp_wm_scratchpad_t));
    if (!wms) {
        LOG_ERROR("Failed to allocate WM scratchpad");
        return NULL;
    }

    if (config) {
        wms->config = *config;
    } else {
        wms->config = nimcp_wms_config_default();
    }

    /* Clamp */
    if (wms->config.num_slots == 0) wms->config.num_slots = 8;
    if (wms->config.num_slots > NIMCP_WMS_MAX_SLOTS) wms->config.num_slots = NIMCP_WMS_MAX_SLOTS;
    if (wms->config.slot_dim == 0) wms->config.slot_dim = 512;
    if (wms->config.slot_dim > NIMCP_WMS_MAX_SLOT_DIM) wms->config.slot_dim = NIMCP_WMS_MAX_SLOT_DIM;
    if (wms->config.decay_rate <= 0.0f || wms->config.decay_rate > 1.0f) {
        wms->config.decay_rate = 0.95f;
    }

    wms->num_slots = wms->config.num_slots;
    wms->slots = (nimcp_wms_slot_t*)nimcp_calloc(wms->num_slots, sizeof(nimcp_wms_slot_t));
    if (!wms->slots) {
        LOG_ERROR("Failed to allocate %u scratchpad slots", wms->num_slots);
        nimcp_free(wms);
        return NULL;
    }

    LOG_INFO("Created WM scratchpad (slots=%u, dim=%u, decay=%.3f)",
             wms->num_slots, wms->config.slot_dim, wms->config.decay_rate);

    return wms;
}

void nimcp_wms_destroy(nimcp_wm_scratchpad_t* wms)
{
    if (!wms) {
        return;
    }
    if (wms->slots) {
        nimcp_free(wms->slots);
    }
    nimcp_free(wms);
}

/* ============================================================================
 * Slot Operations
 * ============================================================================ */

int nimcp_wms_write(nimcp_wm_scratchpad_t* wms, uint32_t slot_idx,
    const float* data, uint32_t data_dim, const char* label)
{
    if (!wms || !data) {
        return -1;
    }
    if (slot_idx >= wms->num_slots) {
        return -1;
    }
    if (data_dim == 0) {
        return -1;
    }

    nimcp_wms_slot_t* slot = &wms->slots[slot_idx];
    uint32_t dim = data_dim < wms->config.slot_dim ? data_dim : wms->config.slot_dim;

    memset(slot->data, 0, wms->config.slot_dim * sizeof(float));
    memcpy(slot->data, data, dim * sizeof(float));
    slot->data_dim = dim;
    slot->written = true;
    slot->age = 0;

    if (label) {
        strncpy(slot->label, label, NIMCP_WMS_LABEL_LEN - 1);
        slot->label[NIMCP_WMS_LABEL_LEN - 1] = '\0';
    } else {
        slot->label[0] = '\0';
    }

    return 0;
}

int nimcp_wms_read(nimcp_wm_scratchpad_t* wms, uint32_t slot_idx,
    float* data_out, uint32_t max_dim)
{
    if (!wms || !data_out) {
        return -1;
    }
    if (slot_idx >= wms->num_slots) {
        return -1;
    }

    const nimcp_wms_slot_t* slot = &wms->slots[slot_idx];
    if (!slot->written) {
        return 0;  /* Empty slot — 0 floats written */
    }

    uint32_t dim = slot->data_dim < max_dim ? slot->data_dim : max_dim;
    memcpy(data_out, slot->data, dim * sizeof(float));

    return (int)dim;
}

int nimcp_wms_read_all(nimcp_wm_scratchpad_t* wms,
    float* concatenated_out, uint32_t max_dim)
{
    if (!wms || !concatenated_out) {
        return -1;
    }

    uint32_t written = 0;
    for (uint32_t i = 0; i < wms->num_slots && written < max_dim; i++) {
        const nimcp_wms_slot_t* slot = &wms->slots[i];
        if (!slot->written) {
            continue;
        }

        uint32_t remaining = max_dim - written;
        uint32_t dim = slot->data_dim < remaining ? slot->data_dim : remaining;
        memcpy(concatenated_out + written, slot->data, dim * sizeof(float));
        written += dim;
    }

    return (int)written;
}

void nimcp_wms_clear(nimcp_wm_scratchpad_t* wms)
{
    if (!wms || !wms->slots) {
        return;
    }
    memset(wms->slots, 0, wms->num_slots * sizeof(nimcp_wms_slot_t));
}

int nimcp_wms_clear_slot(nimcp_wm_scratchpad_t* wms, uint32_t slot_idx)
{
    if (!wms) {
        return -1;
    }
    if (slot_idx >= wms->num_slots) {
        return -1;
    }
    memset(&wms->slots[slot_idx], 0, sizeof(nimcp_wms_slot_t));
    return 0;
}

void nimcp_wms_decay(nimcp_wm_scratchpad_t* wms)
{
    if (!wms || !wms->slots) {
        return;
    }

    float rate = wms->config.decay_rate;
    for (uint32_t i = 0; i < wms->num_slots; i++) {
        nimcp_wms_slot_t* slot = &wms->slots[i];
        if (!slot->written) {
            continue;
        }

        slot->age++;
        for (uint32_t j = 0; j < slot->data_dim; j++) {
            slot->data[j] *= rate;
        }
    }
}

int nimcp_wms_find_by_label(const nimcp_wm_scratchpad_t* wms, const char* label)
{
    if (!wms || !label || !wms->slots) {
        return -1;
    }

    for (uint32_t i = 0; i < wms->num_slots; i++) {
        if (wms->slots[i].written && strcmp(wms->slots[i].label, label) == 0) {
            return (int)i;
        }
    }

    return -1;
}

uint32_t nimcp_wms_get_active_count(const nimcp_wm_scratchpad_t* wms)
{
    if (!wms || !wms->slots) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < wms->num_slots; i++) {
        if (wms->slots[i].written) {
            count++;
        }
    }

    return count;
}
