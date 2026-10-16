/**
 * @file nimcp_column_reference_frame.h
 * @brief Grid Cell Reference Frames for Cortical Columns
 *
 * WHAT: Each cortical column gets a reference frame from entorhinal grid cells.
 *       Columns transform input through reference frames, building object models
 *       as "features at locations."
 * WHY:  Enables invariant object recognition — the same object recognized from
 *       different viewpoints by different columns with different phase offsets.
 * HOW:  Each hypercolumn binds to a grid module with unique phase offsets.
 *       Grid cell population vectors encode location. Feature-location pairs
 *       stored as associations. Movement updates via path integration.
 *
 * Based on Hawkins' Thousand Brains theory (Numenta, 2019).
 */

#ifndef NIMCP_COLUMN_REFERENCE_FRAME_H
#define NIMCP_COLUMN_REFERENCE_FRAME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants
 * ========================================================================= */

#define COL_REF_FRAME_LOCATION_DIM      3    /**< 3D location (x, y, z) */
#define COL_REF_FRAME_ENCODING_DIM      64   /**< Population vector encoding size */
#define COL_REF_FRAME_MAX_PAIRS         256  /**< Max feature-location pairs per frame */
#define COL_REF_FRAME_MAX_FRAMES        128  /**< Max frames (one per hypercolumn) */
#define COL_REF_FRAME_FEATURE_DIM       32   /**< Feature vector dimensionality */

/* =========================================================================
 * Types
 * ========================================================================= */

/**
 * @brief A feature-location association: what feature is at what location.
 */
typedef struct {
    float feature[COL_REF_FRAME_FEATURE_DIM];    /**< Feature SDR */
    float location[COL_REF_FRAME_LOCATION_DIM];  /**< Location in reference frame */
    float confidence;                              /**< Association strength [0,1] */
    uint32_t object_id;                            /**< Which object this belongs to */
} feature_location_pair_t;

/**
 * @brief Reference frame for a single cortical column.
 *
 * Each hypercolumn has one reference frame. The frame tracks the column's
 * current location in object-centric space via grid cell population encoding.
 */
typedef struct {
    uint32_t column_id;                                          /**< Bound hypercolumn ID */
    uint32_t grid_module_idx;                                    /**< Which entorhinal grid module */
    float location[COL_REF_FRAME_LOCATION_DIM];                 /**< Current location (x,y,z) */
    float orientation;                                            /**< Current orientation (radians) */
    float phase_offset[COL_REF_FRAME_LOCATION_DIM];             /**< Per-column phase offset */
    float location_encoding[COL_REF_FRAME_ENCODING_DIM];        /**< Population vector encoding */

    feature_location_pair_t pairs[COL_REF_FRAME_MAX_PAIRS];     /**< Stored associations */
    uint32_t num_pairs;                                           /**< Active pair count */

    uint64_t last_update_us;                                      /**< Last update timestamp */
} column_reference_frame_t;

/**
 * @brief Configuration for the reference frame manager.
 */
typedef struct {
    uint32_t max_frames;                /**< Max number of frames (default 128) */
    uint32_t encoding_dim;              /**< Encoding dimension (default 64) */
    uint32_t max_pairs_per_frame;       /**< Max feature-location pairs (default 256) */
    float movement_threshold;           /**< Min movement to trigger update (default 0.01) */
    float association_learning_rate;    /**< Learning rate for pair associations (default 0.1) */
    float recall_threshold;             /**< Min confidence for recall match (default 0.3) */
    float path_integration_gain;        /**< Scale factor for movement updates (default 1.0) */
} column_ref_frame_config_t;

/**
 * @brief Statistics for reference frame system.
 */
typedef struct {
    uint64_t total_location_updates;    /**< Total movement-driven updates */
    uint64_t total_feature_bindings;    /**< Total feature-location pairs stored */
    uint64_t total_recalls;             /**< Total recall queries */
    uint64_t successful_recalls;        /**< Recalls that found a match */
    float mean_recall_confidence;       /**< Average recall confidence */
} column_ref_frame_stats_t;

/**
 * @brief Manager for all column reference frames.
 */
typedef struct column_ref_frame_manager {
    column_reference_frame_t* frames;   /**< Array of frames [max_frames] */
    uint32_t num_frames;                /**< Active frame count */
    uint32_t max_frames;

    /* Configuration */
    float movement_threshold;
    float association_learning_rate;
    float recall_threshold;
    float path_integration_gain;
    uint32_t encoding_dim;

    /* Statistics */
    column_ref_frame_stats_t stats;

    /* Thread safety */
    void* mutex;                        /**< nimcp_mutex_t* */
} column_ref_frame_manager_t;

/* =========================================================================
 * API
 * ========================================================================= */

/** Set default configuration values */
void column_ref_frame_config_default(column_ref_frame_config_t* config);

/** Create reference frame manager */
column_ref_frame_manager_t* column_ref_frame_create(const column_ref_frame_config_t* config);

/** Destroy manager and free all resources */
void column_ref_frame_destroy(column_ref_frame_manager_t* mgr);

/**
 * @brief Bind a column to a grid module with phase offsets.
 * @return Frame index on success, -1 on error
 */
int column_ref_frame_bind_column(column_ref_frame_manager_t* mgr,
                                  uint32_t column_id, uint32_t grid_module_idx,
                                  const float* phase_offset);

/**
 * @brief Connect to entorhinal grid cell system (stub for integration).
 * @return 0 on success, -1 on error
 */
int column_ref_frame_connect_entorhinal(column_ref_frame_manager_t* mgr,
                                         void* entorhinal_ctx);

/**
 * @brief Update location via movement (path integration).
 * @param movement Delta movement vector [3]
 * @return 0 on success, -1 on error
 */
int column_ref_frame_update_location(column_ref_frame_manager_t* mgr,
                                      uint32_t frame_idx,
                                      const float* movement);

/**
 * @brief Encode a feature at the current location.
 * @return 0 on success, -1 on error
 */
int column_ref_frame_encode_feature_at_location(column_ref_frame_manager_t* mgr,
                                                  uint32_t frame_idx,
                                                  const float* feature, uint32_t feat_dim,
                                                  uint32_t object_id);

/**
 * @brief Get the current location encoding (population vector).
 * @param encoding Output buffer [encoding_dim]
 * @return 0 on success, -1 on error
 */
int column_ref_frame_get_location_encoding(const column_ref_frame_manager_t* mgr,
                                            uint32_t frame_idx,
                                            float* encoding, uint32_t max_dim);

/**
 * @brief Recall what feature is expected at the current location.
 * @param recalled_feature Output feature buffer
 * @param object_id Output: matched object ID
 * @param confidence Output: recall confidence
 * @return 0 if found, 1 if no match, -1 on error
 */
int column_ref_frame_recall_feature_at(column_ref_frame_manager_t* mgr,
                                        uint32_t frame_idx,
                                        float* recalled_feature, uint32_t feat_dim,
                                        uint32_t* object_id, float* confidence);

/**
 * @brief Predict next location given movement.
 * @param predicted_location Output location [3]
 * @return 0 on success, -1 on error
 */
int column_ref_frame_predict_next_location(const column_ref_frame_manager_t* mgr,
                                            uint32_t frame_idx,
                                            const float* movement,
                                            float* predicted_location);

/** Get full statistics */
int column_ref_frame_get_stats(const column_ref_frame_manager_t* mgr,
                                column_ref_frame_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLUMN_REFERENCE_FRAME_H */
