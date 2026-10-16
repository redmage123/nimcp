/**
 * @file nimcp_dimension_constants.h
 * @brief Centralized dimension and array size constants for NIMCP
 * @version 1.0.0
 * @date 2025-02-03
 *
 * WHAT: Defines all dimension-related constants used throughout the codebase
 * WHY:  Ensures consistency in array sizes, grid dimensions, neural architecture
 * HOW:  Single header with organization by subsystem and data type
 *
 * Usage: #include "constants/nimcp_dimension_constants.h"
 */

#ifndef NIMCP_DIMENSION_CONSTANTS_H
#define NIMCP_DIMENSION_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Cortical Column and Hypercolumn Dimensions
 *===========================================================================*/

/** @brief Default number of hypercolumns */
#define NIMCP_DEFAULT_HYPERCOLUMNS          64

/** @brief Small hypercolumn count for constrained systems */
#define NIMCP_SMALL_HYPERCOLUMNS            32

/** @brief Large hypercolumn count for full systems */
#define NIMCP_LARGE_HYPERCOLUMNS            128

/** @brief Standard neurons per minicolumn */
#define NIMCP_NEURONS_PER_MINICOLUMN        80

/** @brief Standard minicolumns per hypercolumn */
#define NIMCP_MINICOLUMNS_PER_HYPERCOLUMN   100

/*=============================================================================
 * Attention Map Dimensions
 *===========================================================================*/

/** @brief Default attention map width */
#define NIMCP_ATTENTION_MAP_WIDTH           64

/** @brief Default attention map height */
#define NIMCP_ATTENTION_MAP_HEIGHT          64

/** @brief Maximum attention map width */
#define NIMCP_ATTENTION_MAP_MAX_WIDTH       256

/** @brief Maximum attention map height */
#define NIMCP_ATTENTION_MAP_MAX_HEIGHT      256

/** @brief Spatial attention grid default size */
#define NIMCP_SPATIAL_ATTENTION_GRID_SIZE   64

/*=============================================================================
 * Grid and Spatial Dimensions
 *===========================================================================*/

/** @brief Default grid dimension */
#define NIMCP_DEFAULT_GRID_DIM              64

/** @brief Small grid dimension */
#define NIMCP_SMALL_GRID_DIM                32

/** @brief Large grid dimension */
#define NIMCP_LARGE_GRID_DIM                128

/** @brief Morphogenesis default grid size */
#define NIMCP_MORPH_DEFAULT_GRID_SIZE       64

/** @brief Minimum voxel size for spatial processing */
#define NIMCP_MIN_VOXEL_SIZE                0.1f

/** @brief Maximum voxel size for spatial processing */
#define NIMCP_MAX_VOXEL_SIZE                100.0f

/*=============================================================================
 * Vector and Embedding Dimensions
 *===========================================================================*/

/** @brief Default embedding dimension (CPU-staged, not resident in VRAM) */
#define NIMCP_DEFAULT_EMBEDDING_DIM         2048

/** @brief Small embedding dimension */
#define NIMCP_SMALL_EMBEDDING_DIM           128

/** @brief Medium embedding dimension */
#define NIMCP_MEDIUM_EMBEDDING_DIM          512

/** @brief Large embedding dimension */
#define NIMCP_LARGE_EMBEDDING_DIM           2048

/** @brief GPU batch size for embedding relevance recomputation */
#define NIMCP_EMBEDDING_BATCH_SIZE          4096

/** @brief Sentinel value for "no embedding allocated" */
#define NIMCP_EMBEDDING_POOL_NONE           UINT32_MAX

/** @brief Ambient dimension for information geometry */
#define NIMCP_AMBIENT_DIM                   256

/** @brief Feature vector dimension */
#define NIMCP_FEATURE_DIM                   128

/** @brief State vector dimension */
#define NIMCP_STATE_DIM                     64

/*=============================================================================
 * Memory and Knowledge Graph Dimensions
 *===========================================================================*/

/** @brief Default working memory slots */
#define NIMCP_WM_SLOTS                      7

/** @brief Maximum working memory slots */
#define NIMCP_WM_MAX_SLOTS                  12

/** @brief Episodic memory pattern dimension */
#define NIMCP_EPISODIC_PATTERN_DIM          256

/** @brief Semantic memory concept dimension */
#define NIMCP_SEMANTIC_CONCEPT_DIM          128

/** @brief KG maximum path length */
#define NIMCP_KG_MAX_PATH_LEN               16

/** @brief Maximum path samples for topology */
#define NIMCP_MAX_PATH_SAMPLES              100

/*=============================================================================
 * Neural Network Layer Dimensions
 *===========================================================================*/

/** @brief Default hidden layer size */
#define NIMCP_DEFAULT_HIDDEN_SIZE           256

/** @brief Small hidden layer size */
#define NIMCP_SMALL_HIDDEN_SIZE             64

/** @brief Medium hidden layer size */
#define NIMCP_MEDIUM_HIDDEN_SIZE            128

/** @brief Large hidden layer size */
#define NIMCP_LARGE_HIDDEN_SIZE             512

/** @brief Default attention heads */
#define NIMCP_DEFAULT_ATTENTION_HEADS       8

/** @brief Default transformer layers */
#define NIMCP_DEFAULT_TRANSFORMER_LAYERS    6

/*=============================================================================
 * Swarm and Collective Dimensions
 *===========================================================================*/

/** @brief Default swarm agent count */
#define NIMCP_DEFAULT_SWARM_AGENTS          32

/** @brief Maximum swarm agents */
#define NIMCP_MAX_SWARM_AGENTS              1000

/** @brief Default collective nodes */
#define NIMCP_DEFAULT_COLLECTIVE_NODES      16

/** @brief Maximum collective nodes */
#define NIMCP_MAX_COLLECTIVE_NODES          128

/*=============================================================================
 * Sensory Processing Dimensions
 *===========================================================================*/

/** @brief Visual field width (degrees) */
#define NIMCP_VISUAL_FIELD_WIDTH            180.0f

/** @brief Visual field height (degrees) */
#define NIMCP_VISUAL_FIELD_HEIGHT           120.0f

/** @brief Retina ganglion cell count approximation */
#define NIMCP_RETINA_GANGLION_CELLS         1000000

/** @brief Cochlea hair cell count */
#define NIMCP_COCHLEA_HAIR_CELLS            15000

/** @brief Tonotopic map frequency bins */
#define NIMCP_TONOTOPIC_BINS                128

/*=============================================================================
 * Motion and Control Dimensions
 *===========================================================================*/

/** @brief Motion vector dimension (x, y, z, rx, ry, rz) */
#define NIMCP_MOTION_VECTOR_DIM             6

/** @brief Control signal dimension */
#define NIMCP_CONTROL_SIGNAL_DIM            4

/** @brief Joint angle dimension (typical humanoid) */
#define NIMCP_JOINT_ANGLE_DIM               32

/** @brief Dragonfly motion content size */
#define NIMCP_DRAGONFLY_CONTENT_SIZE        6

/*=============================================================================
 * Batch and Processing Dimensions
 *===========================================================================*/

/** @brief Default batch size */
#define NIMCP_DEFAULT_BATCH_SIZE            32

/** @brief Small batch size */
#define NIMCP_SMALL_BATCH_SIZE              8

/** @brief Medium batch size */
#define NIMCP_MEDIUM_BATCH_SIZE             64

/** @brief Large batch size */
#define NIMCP_LARGE_BATCH_SIZE              128

/** @brief Maximum batch size */
#define NIMCP_MAX_BATCH_SIZE                256

/*=============================================================================
 * Reasoning and Logic Dimensions
 *===========================================================================*/

/** @brief Maximum reasoning path length */
#define NIMCP_MAX_REASONING_PATH            5

/** @brief Default proof tree depth */
#define NIMCP_DEFAULT_PROOF_DEPTH           10

/** @brief Maximum unification depth */
#define NIMCP_MAX_UNIFICATION_DEPTH         20

/*=============================================================================
 * Thread and Concurrency Limits
 *===========================================================================*/

/** @brief Default thread pool size */
#define NIMCP_DEFAULT_THREAD_POOL_SIZE      4

/** @brief Maximum thread pool size */
#define NIMCP_MAX_THREAD_POOL_SIZE          32

/** @brief Default worker threads */
#define NIMCP_DEFAULT_WORKERS               4

/*=============================================================================
 * Platform Tier Specific Dimensions
 * Note: These use TIER prefix to avoid conflicts with generic dimensions above
 *===========================================================================*/

/* Full platform dimensions */
#define NIMCP_TIER_FULL_HYPERCOLUMNS             128
#define NIMCP_TIER_FULL_EMBEDDING_DIM            512
#define NIMCP_TIER_FULL_HIDDEN_SIZE              512

/* Medium platform dimensions */
#define NIMCP_TIER_MEDIUM_HYPERCOLUMNS           64
#define NIMCP_TIER_MEDIUM_EMBEDDING_DIM          256
#define NIMCP_TIER_MEDIUM_HIDDEN_SIZE            256

/* Constrained platform dimensions */
#define NIMCP_TIER_CONSTRAINED_HYPERCOLUMNS      32
#define NIMCP_TIER_CONSTRAINED_EMBEDDING_DIM     64
#define NIMCP_TIER_CONSTRAINED_HIDDEN_SIZE       64

/* Minimal platform dimensions */
#define NIMCP_TIER_MINIMAL_HYPERCOLUMNS          16
#define NIMCP_TIER_MINIMAL_EMBEDDING_DIM         32
#define NIMCP_TIER_MINIMAL_HIDDEN_SIZE           32

/*=============================================================================
 * Dimension Calculation Macros
 *===========================================================================*/

/** @brief Calculate 2D array total size */
#define NIMCP_2D_SIZE(width, height)        ((width) * (height))

/** @brief Calculate 3D array total size */
#define NIMCP_3D_SIZE(w, h, d)              ((w) * (h) * (d))

/** @brief Calculate aligned dimension (power of 2) */
#define NIMCP_ALIGN_DIM(dim, align)         (((dim) + (align) - 1) & ~((align) - 1))

/** @brief Check if dimension is power of 2 */
#define NIMCP_IS_POW2(x)                    (((x) != 0) && (((x) & ((x) - 1)) == 0))

/** @brief Next power of 2 */
#define NIMCP_NEXT_POW2(x)                  \
    (1U << (32 - __builtin_clz((x) - 1)))

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DIMENSION_CONSTANTS_H */
