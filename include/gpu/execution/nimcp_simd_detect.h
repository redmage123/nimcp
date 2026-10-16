/**
 * @file nimcp_simd_detect.h
 * @brief SIMD Capability Detection (AVX2, AVX512, NEON, etc.)
 *
 * WHAT: Detects CPU SIMD capabilities at runtime
 * WHY:  Enables optimal vectorization for neural network operations
 * HOW:  Uses CPUID on x86, feature registers on ARM, fallback for others
 *
 * ARCHITECTURE:
 *
 *   ┌────────────────────────────────────────────────────────────┐
 *   │                  SIMD CAPABILITY DETECTION                  │
 *   │                                                            │
 *   │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐ │
 *   │  │   x86/x64    │    │    ARM64     │    │   Fallback   │ │
 *   │  │   CPUID      │    │   Features   │    │   (None)     │ │
 *   │  └──────────────┘    └──────────────┘    └──────────────┘ │
 *   │         │                  │                    │         │
 *   │         └──────────────────┼────────────────────┘         │
 *   │                            ▼                              │
 *   │                 ┌──────────────────────┐                  │
 *   │                 │  simd_capabilities_t │                  │
 *   │                 │  (Unified Results)   │                  │
 *   │                 └──────────────────────┘                  │
 *   └────────────────────────────────────────────────────────────┘
 *
 * SUPPORTED INSTRUCTION SETS:
 * - x86/x64: SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, AVX-512
 * - ARM64: NEON, SVE, SVE2
 * - Fallback: No SIMD
 *
 * THREAD SAFETY: Thread-safe (detection is read-only after init)
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_SIMD_DETECT_H
#define NIMCP_SIMD_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// SIMD Feature Flags
//=============================================================================

/**
 * @brief SIMD feature flags (bitmask)
 *
 * WHY BITMASK:
 * - Efficient storage (single uint32_t)
 * - Fast feature testing (bitwise AND)
 * - Easy combination/comparison
 */
typedef enum {
    SIMD_NONE      = 0,          /**< No SIMD support */

    // x86/x64 SSE family
    SIMD_SSE       = (1 << 0),   /**< SSE (128-bit float) */
    SIMD_SSE2      = (1 << 1),   /**< SSE2 (128-bit int/double) */
    SIMD_SSE3      = (1 << 2),   /**< SSE3 (horizontal ops) */
    SIMD_SSSE3     = (1 << 3),   /**< SSSE3 (shuffle/permute) */
    SIMD_SSE41     = (1 << 4),   /**< SSE4.1 (blend/dot product) */
    SIMD_SSE42     = (1 << 5),   /**< SSE4.2 (string/CRC) */

    // x86/x64 AVX family
    SIMD_AVX       = (1 << 6),   /**< AVX (256-bit float) */
    SIMD_AVX2      = (1 << 7),   /**< AVX2 (256-bit int) */
    SIMD_FMA       = (1 << 8),   /**< FMA (fused multiply-add) */

    // x86/x64 AVX-512 family
    SIMD_AVX512F   = (1 << 9),   /**< AVX-512 Foundation */
    SIMD_AVX512VL  = (1 << 10),  /**< AVX-512 Vector Length */
    SIMD_AVX512BW  = (1 << 11),  /**< AVX-512 Byte/Word */
    SIMD_AVX512DQ  = (1 << 12),  /**< AVX-512 Doubleword/Quadword */
    SIMD_AVX512CD  = (1 << 13),  /**< AVX-512 Conflict Detection */
    SIMD_AVX512VNNI = (1 << 14), /**< AVX-512 VNNI (DL acceleration) */

    // ARM NEON family
    SIMD_NEON      = (1 << 16),  /**< ARM NEON (128-bit) */
    SIMD_SVE       = (1 << 17),  /**< ARM SVE (scalable vector) */
    SIMD_SVE2      = (1 << 18),  /**< ARM SVE2 (enhanced SVE) */

    // Combined convenience flags
    SIMD_X86_BASIC = SIMD_SSE | SIMD_SSE2,
    SIMD_X86_FULL  = SIMD_SSE | SIMD_SSE2 | SIMD_SSE3 | SIMD_SSSE3 |
                     SIMD_SSE41 | SIMD_SSE42 | SIMD_AVX | SIMD_AVX2 | SIMD_FMA,
    SIMD_AVX512_COMMON = SIMD_AVX512F | SIMD_AVX512VL | SIMD_AVX512BW | SIMD_AVX512DQ
} simd_feature_t;

//=============================================================================
// SIMD Capabilities Structure
//=============================================================================

/**
 * @brief SIMD capabilities structure
 *
 * WHAT: Contains detected SIMD features and vector widths
 * WHY:  Unified representation across platforms
 * HOW:  Populated by simd_detect_capabilities()
 */
typedef struct {
    // Feature flags
    uint32_t features;           /**< Bitmask of simd_feature_t */

    // Best available vector width (bits)
    uint32_t max_vector_width;   /**< 128, 256, 512, or platform SVE width */

    // Individual feature flags (convenient access)
    bool has_sse;
    bool has_sse2;
    bool has_sse3;
    bool has_ssse3;
    bool has_sse41;
    bool has_sse42;
    bool has_avx;
    bool has_avx2;
    bool has_fma;
    bool has_avx512f;
    bool has_avx512vl;
    bool has_avx512bw;
    bool has_avx512dq;
    bool has_avx512vnni;
    bool has_neon;
    bool has_sve;
    bool has_sve2;

    // Platform info
    char vendor[16];             /**< CPU vendor string (e.g., "GenuineIntel") */
    char brand[64];              /**< CPU brand string */

    // Cache info (useful for tiling)
    uint32_t l1_cache_size;      /**< L1 data cache size in KB */
    uint32_t l2_cache_size;      /**< L2 cache size in KB */
    uint32_t l3_cache_size;      /**< L3 cache size in KB */
    uint32_t cache_line_size;    /**< Cache line size in bytes */
} simd_capabilities_t;

//=============================================================================
// Detection API
//=============================================================================

/**
 * @brief Detect SIMD capabilities
 *
 * WHAT: Queries CPU for SIMD feature support
 * WHY:  Need to know available instructions for optimization
 * HOW:  Uses CPUID on x86, feature registers on ARM
 *
 * @param caps Output structure for capabilities (must not be NULL)
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe (read-only after detection)
 * CACHING: Results are cached internally after first call
 *
 * EXAMPLE:
 *   simd_capabilities_t caps;
 *   if (simd_detect_capabilities(&caps)) {
 *       if (caps.has_avx2) {
 *           use_avx2_kernels();
 *       } else if (caps.has_sse2) {
 *           use_sse2_kernels();
 *       }
 *   }
 */
NIMCP_EXPORT bool simd_detect_capabilities(simd_capabilities_t* caps);

/**
 * @brief Check if specific SIMD feature is available
 *
 * @param feature Feature flag to check (from simd_feature_t)
 * @return true if feature is available
 *
 * EXAMPLE:
 *   if (simd_has_feature(SIMD_AVX2)) {
 *       // Use AVX2 code path
 *   }
 */
NIMCP_EXPORT bool simd_has_feature(simd_feature_t feature);

/**
 * @brief Get best available vector width in bits
 *
 * @return Vector width (512, 256, 128, or 0 if no SIMD)
 *
 * EXAMPLE:
 *   uint32_t width = simd_get_vector_width();
 *   // width = 256 for AVX2, 512 for AVX-512
 */
NIMCP_EXPORT uint32_t simd_get_vector_width(void);

/**
 * @brief Get SIMD feature name as string
 *
 * @param feature Feature flag
 * @return Human-readable name (e.g., "AVX2")
 */
NIMCP_EXPORT const char* simd_feature_name(simd_feature_t feature);

/**
 * @brief Get all detected features as a string
 *
 * @param buffer Output buffer for feature string
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator)
 *
 * EXAMPLE:
 *   char buf[256];
 *   simd_features_string(buf, sizeof(buf));
 *   // buf = "SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 AVX AVX2 FMA"
 */
NIMCP_EXPORT size_t simd_features_string(char* buffer, size_t size);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Check if AVX2 is available (common check)
 */
#define SIMD_HAS_AVX2() simd_has_feature(SIMD_AVX2)

/**
 * @brief Check if AVX-512 is available (common check)
 */
#define SIMD_HAS_AVX512() simd_has_feature(SIMD_AVX512F)

/**
 * @brief Check if NEON is available (ARM)
 */
#define SIMD_HAS_NEON() simd_has_feature(SIMD_NEON)

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SIMD_DETECT_H
