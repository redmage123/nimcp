/**
 * @file nimcp_wm_scratchpad.h
 * @brief Working Memory Scratchpad — persistent scratchpad for multi-step
 *        reasoning. The brain can write intermediate results and read them back.
 *
 * NOTE: This is distinct from nimcp_working_memory.h (Miller's 7+/-2 salience
 * buffer). The scratchpad provides simple labeled slots with temporal decay.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_WM_SCRATCHPAD_H
#define NIMCP_WM_SCRATCHPAD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_WMS_MAX_SLOTS     32
#define NIMCP_WMS_MAX_SLOT_DIM  2048
#define NIMCP_WMS_LABEL_LEN     64

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t num_slots;    /**< Number of scratchpad slots (default 8) */
    uint32_t slot_dim;     /**< Dimension per slot (default 512) */
    float    decay_rate;   /**< Per-decay-call multiplier (default 0.95) */
} nimcp_wms_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_wm_scratchpad nimcp_wm_scratchpad_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Create a working memory scratchpad.
 * @param config Configuration. NULL uses defaults.
 * @return Handle, or NULL on failure.
 */
nimcp_wm_scratchpad_t* nimcp_wms_create(const nimcp_wms_config_t* config);

/**
 * @brief Destroy a working memory scratchpad. NULL-safe.
 */
void nimcp_wms_destroy(nimcp_wm_scratchpad_t* wms);

/**
 * @brief Return default configuration.
 */
nimcp_wms_config_t nimcp_wms_config_default(void);

/* ============================================================================
 * Slot Operations
 * ============================================================================ */

/**
 * @brief Write data to a slot.
 * @param wms       Handle.
 * @param slot_idx  Slot index [0, num_slots).
 * @param data      Data vector to write.
 * @param data_dim  Dimension of data (clamped to slot_dim).
 * @param label     Optional label (up to 63 chars). NULL for no label.
 * @return 0 on success, -1 on failure.
 */
int nimcp_wms_write(nimcp_wm_scratchpad_t* wms, uint32_t slot_idx,
    const float* data, uint32_t data_dim, const char* label);

/**
 * @brief Read from a slot (applies decay to the read data).
 * @param wms       Handle.
 * @param slot_idx  Slot index.
 * @param data_out  Output buffer.
 * @param max_dim   Size of output buffer.
 * @return Number of floats written, or -1 on failure.
 */
int nimcp_wms_read(nimcp_wm_scratchpad_t* wms, uint32_t slot_idx,
    float* data_out, uint32_t max_dim);

/**
 * @brief Read and concatenate all active slots into one vector.
 * @param wms              Handle.
 * @param concatenated_out Output buffer.
 * @param max_dim          Size of output buffer.
 * @return Total number of floats written, or -1 on failure.
 */
int nimcp_wms_read_all(nimcp_wm_scratchpad_t* wms,
    float* concatenated_out, uint32_t max_dim);

/**
 * @brief Clear all slots.
 */
void nimcp_wms_clear(nimcp_wm_scratchpad_t* wms);

/**
 * @brief Clear a single slot.
 * @return 0 on success, -1 on invalid index.
 */
int nimcp_wms_clear_slot(nimcp_wm_scratchpad_t* wms, uint32_t slot_idx);

/**
 * @brief Apply decay to all slots (multiply contents by decay_rate).
 */
void nimcp_wms_decay(nimcp_wm_scratchpad_t* wms);

/**
 * @brief Find a slot by its label.
 * @return Slot index, or -1 if not found.
 */
int nimcp_wms_find_by_label(const nimcp_wm_scratchpad_t* wms, const char* label);

/**
 * @brief Count how many slots have been written to.
 */
uint32_t nimcp_wms_get_active_count(const nimcp_wm_scratchpad_t* wms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WM_SCRATCHPAD_H */
