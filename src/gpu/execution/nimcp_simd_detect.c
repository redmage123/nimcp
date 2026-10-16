/**
 * @file nimcp_simd_detect.c
 * @brief SIMD Capability Detection Implementation
 *
 * WHAT: Detects CPU SIMD capabilities at runtime
 * WHY:  Enables optimal vectorization for neural network operations
 * HOW:  Uses CPUID on x86, feature registers on ARM, fallback for others
 *
 * PLATFORM SUPPORT:
 * - x86/x64: Uses CPUID instruction via compiler intrinsics
 * - ARM64: Uses getauxval() on Linux, sysctl() on macOS
 * - Other: Returns no SIMD support (safe fallback)
 *
 * THREAD SAFETY:
 * - Detection is thread-safe (read-only hardware query)
 * - Results are cached in static variable after first call
 * - Cache uses pthread_once for thread-safe one-time initialization
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#define LOG_MODULE "SIMD_DETECT"
#define LOG_MODULE_ID 0x0901
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(simd_detect)

#include "gpu/execution/nimcp_simd_detect.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

// Platform-specific includes
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define SIMD_X86 1
    #if defined(_MSC_VER)
        #include <intrin.h>
    #elif defined(__GNUC__) || defined(__clang__)
        #include <cpuid.h>
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define SIMD_ARM64 1
    #if defined(__linux__)
        #include <sys/auxv.h>
        #include <asm/hwcap.h>
    #elif defined(__APPLE__)
        #include <sys/sysctl.h>
    #endif
#endif

// Thread-safe initialization
#include <pthread.h>

//=============================================================================
// Static Cache
//=============================================================================

static simd_capabilities_t s_cached_caps = {0};
static pthread_once_t s_cache_init_once = PTHREAD_ONCE_INIT;

// Forward declaration for pthread_once callback
static void simd_detect_init_impl(void);

//=============================================================================
// x86 CPUID Detection
//=============================================================================

#ifdef SIMD_X86

/**
 * @brief Execute CPUID instruction
 *
 * @param leaf CPUID leaf (EAX value)
 * @param subleaf CPUID subleaf (ECX value)
 * @param eax Output EAX
 * @param ebx Output EBX
 * @param ecx Output ECX
 * @param edx Output EDX
 */
static void cpuid(uint32_t leaf, uint32_t subleaf,
                  uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx)
{
#if defined(_MSC_VER)
    int regs[4];
    __cpuidex(regs, (int)leaf, (int)subleaf);
    *eax = (uint32_t)regs[0];
    *ebx = (uint32_t)regs[1];
    *ecx = (uint32_t)regs[2];
    *edx = (uint32_t)regs[3];
#elif defined(__GNUC__) || defined(__clang__)
    __cpuid_count(leaf, subleaf, *eax, *ebx, *ecx, *edx);
#else
    *eax = *ebx = *ecx = *edx = 0;
#endif
}

/**
 * @brief Get extended control register (XCR0)
 *
 * WHY: AVX/AVX-512 require OS support (XSAVE/XRSTOR)
 *      We must check XCR0 to ensure OS enables these features
 */
static uint64_t xgetbv(uint32_t xcr)
{
#if defined(_MSC_VER)
    return _xgetbv(xcr);
#elif defined(__GNUC__) || defined(__clang__)
    uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
    return ((uint64_t)edx << 32) | eax;
#else
    (void)xcr;
    return 0;
#endif
}

/**
 * @brief Detect x86 SIMD capabilities using CPUID
 */
static void detect_x86_simd(simd_capabilities_t* caps)
{
    uint32_t eax, ebx, ecx, edx;
    uint32_t max_leaf, max_ext_leaf;

    // Get max CPUID leaf
    cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);

    // Get vendor string
    memcpy(caps->vendor + 0, &ebx, 4);
    memcpy(caps->vendor + 4, &edx, 4);
    memcpy(caps->vendor + 8, &ecx, 4);
    caps->vendor[12] = '\0';

    // Get max extended leaf
    cpuid(0x80000000, 0, &max_ext_leaf, &ebx, &ecx, &edx);

    // Get brand string (extended leaves 0x80000002-0x80000004)
    if (max_ext_leaf >= 0x80000004) {
        cpuid(0x80000002, 0, &eax, &ebx, &ecx, &edx);
        memcpy(caps->brand + 0, &eax, 4);
        memcpy(caps->brand + 4, &ebx, 4);
        memcpy(caps->brand + 8, &ecx, 4);
        memcpy(caps->brand + 12, &edx, 4);

        cpuid(0x80000003, 0, &eax, &ebx, &ecx, &edx);
        memcpy(caps->brand + 16, &eax, 4);
        memcpy(caps->brand + 20, &ebx, 4);
        memcpy(caps->brand + 24, &ecx, 4);
        memcpy(caps->brand + 28, &edx, 4);

        cpuid(0x80000004, 0, &eax, &ebx, &ecx, &edx);
        memcpy(caps->brand + 32, &eax, 4);
        memcpy(caps->brand + 36, &ebx, 4);
        memcpy(caps->brand + 40, &ecx, 4);
        memcpy(caps->brand + 44, &edx, 4);
        caps->brand[48] = '\0';
    }

    // Get cache info (leaf 0x80000006 on AMD, 0x04 on Intel)
    if (max_ext_leaf >= 0x80000006) {
        cpuid(0x80000006, 0, &eax, &ebx, &ecx, &edx);
        caps->l2_cache_size = (ecx >> 16) & 0xFFFF;  // L2 in KB
        caps->cache_line_size = ecx & 0xFF;
    }

    // Get feature flags (leaf 1)
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);

        // EDX features
        caps->has_sse = (edx >> 25) & 1;
        caps->has_sse2 = (edx >> 26) & 1;

        // ECX features
        caps->has_sse3 = (ecx >> 0) & 1;
        caps->has_ssse3 = (ecx >> 9) & 1;
        caps->has_sse41 = (ecx >> 19) & 1;
        caps->has_sse42 = (ecx >> 20) & 1;
        caps->has_fma = (ecx >> 12) & 1;

        // AVX requires OS support (OSXSAVE bit)
        bool osxsave = (ecx >> 27) & 1;
        bool avx_bit = (ecx >> 28) & 1;

        if (osxsave && avx_bit) {
            // Check XCR0 for AVX state support
            uint64_t xcr0 = xgetbv(0);
            bool avx_os_support = (xcr0 & 0x6) == 0x6;  // XMM + YMM

            if (avx_os_support) {
                caps->has_avx = true;
            }
        }
    }

    // Get extended features (leaf 7)
    if (max_leaf >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);

        // AVX2 (EBX bit 5)
        if (caps->has_avx && ((ebx >> 5) & 1)) {
            caps->has_avx2 = true;
        }

        // AVX-512 features (require OS support for ZMM state)
        bool avx512f_bit = (ebx >> 16) & 1;
        bool avx512vl_bit = (ebx >> 31) & 1;
        bool avx512bw_bit = (ebx >> 30) & 1;
        bool avx512dq_bit = (ebx >> 17) & 1;
        bool avx512cd_bit = (ebx >> 28) & 1;
        bool avx512vnni_bit = (ecx >> 11) & 1;

        if (avx512f_bit && caps->has_avx) {
            // Check XCR0 for AVX-512 state support
            uint64_t xcr0 = xgetbv(0);
            bool avx512_os_support = (xcr0 & 0xE6) == 0xE6;  // XMM + YMM + ZMM + opmask

            if (avx512_os_support) {
                caps->has_avx512f = true;
                caps->has_avx512vl = avx512vl_bit;
                caps->has_avx512bw = avx512bw_bit;
                caps->has_avx512dq = avx512dq_bit;
                caps->has_avx512vnni = avx512vnni_bit;
                (void)avx512cd_bit;  // Not exposed in struct yet
            }
        }
    }

    // Update feature bitmask
    if (caps->has_sse) caps->features |= SIMD_SSE;
    if (caps->has_sse2) caps->features |= SIMD_SSE2;
    if (caps->has_sse3) caps->features |= SIMD_SSE3;
    if (caps->has_ssse3) caps->features |= SIMD_SSSE3;
    if (caps->has_sse41) caps->features |= SIMD_SSE41;
    if (caps->has_sse42) caps->features |= SIMD_SSE42;
    if (caps->has_avx) caps->features |= SIMD_AVX;
    if (caps->has_avx2) caps->features |= SIMD_AVX2;
    if (caps->has_fma) caps->features |= SIMD_FMA;
    if (caps->has_avx512f) caps->features |= SIMD_AVX512F;
    if (caps->has_avx512vl) caps->features |= SIMD_AVX512VL;
    if (caps->has_avx512bw) caps->features |= SIMD_AVX512BW;
    if (caps->has_avx512dq) caps->features |= SIMD_AVX512DQ;
    if (caps->has_avx512vnni) caps->features |= SIMD_AVX512VNNI;

    // Determine max vector width
    if (caps->has_avx512f) {
        caps->max_vector_width = 512;
    } else if (caps->has_avx || caps->has_avx2) {
        caps->max_vector_width = 256;
    } else if (caps->has_sse || caps->has_sse2) {
        caps->max_vector_width = 128;
    } else {
        caps->max_vector_width = 0;
    }
}

#endif // SIMD_X86

//=============================================================================
// ARM64 SIMD Detection
//=============================================================================

#ifdef SIMD_ARM64

/**
 * @brief Detect ARM64 SIMD capabilities
 */
static void detect_arm64_simd(simd_capabilities_t* caps)
{
    // ARM64 always has NEON
    caps->has_neon = true;
    caps->features |= SIMD_NEON;
    caps->max_vector_width = 128;

    strncpy(caps->vendor, "ARM", sizeof(caps->vendor) - 1);
    strncpy(caps->brand, "ARM64 Processor", sizeof(caps->brand) - 1);

#if defined(__linux__)
    // Check for SVE support via getauxval
    #ifdef HWCAP_SVE
    unsigned long hwcap = getauxval(AT_HWCAP);
    if (hwcap & HWCAP_SVE) {
        caps->has_sve = true;
        caps->features |= SIMD_SVE;
        // SVE width varies by implementation, use 256 as common case
        caps->max_vector_width = 256;
    }
    #endif

    #ifdef HWCAP2_SVE2
    unsigned long hwcap2 = getauxval(AT_HWCAP2);
    if (hwcap2 & HWCAP2_SVE2) {
        caps->has_sve2 = true;
        caps->features |= SIMD_SVE2;
    }
    #endif

#elif defined(__APPLE__)
    // macOS ARM (Apple Silicon) always has NEON
    // SVE is not available on Apple Silicon as of 2025
    caps->max_vector_width = 128;
    strncpy(caps->brand, "Apple Silicon", sizeof(caps->brand) - 1);
#endif
}

#endif // SIMD_ARM64

//=============================================================================
// Internal Initialization (called via pthread_once)
//=============================================================================

/**
 * @brief Thread-safe one-time initialization of SIMD detection cache
 *
 * WHY:  pthread_once guarantees this runs exactly once, even with concurrent calls
 * HOW:  Called by pthread_once in simd_detect_capabilities
 */
static void simd_detect_init_impl(void)
{
    // Initialize structure
    memset(&s_cached_caps, 0, sizeof(simd_capabilities_t));

    // Platform-specific detection
#ifdef SIMD_X86
    detect_x86_simd(&s_cached_caps);
    LOG_INFO("Detected x86 SIMD: vendor=%s, AVX2=%d, AVX512=%d, width=%u",
             s_cached_caps.vendor, s_cached_caps.has_avx2, s_cached_caps.has_avx512f,
             s_cached_caps.max_vector_width);
#elif defined(SIMD_ARM64)
    detect_arm64_simd(&s_cached_caps);
    LOG_INFO("Detected ARM64 SIMD: NEON=%d, SVE=%d, width=%u",
             s_cached_caps.has_neon, s_cached_caps.has_sve, s_cached_caps.max_vector_width);
#else
    LOG_INFO("No SIMD detection available for this platform");
    strncpy(s_cached_caps.vendor, "Unknown", sizeof(s_cached_caps.vendor) - 1);
    strncpy(s_cached_caps.brand, "Unknown Processor", sizeof(s_cached_caps.brand) - 1);
#endif
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool simd_detect_capabilities(simd_capabilities_t* caps)
{
    if (!caps) {
        LOG_ERROR("NULL capabilities pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "simd_detect_capabilities: caps is NULL");
        return false;
    }

    // Thread-safe one-time initialization using pthread_once
    pthread_once(&s_cache_init_once, simd_detect_init_impl);

    // Copy cached result to caller's buffer
    memcpy(caps, &s_cached_caps, sizeof(simd_capabilities_t));
    return true;
}

bool simd_has_feature(simd_feature_t feature)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, simd_detect_init_impl);

    return (s_cached_caps.features & feature) == feature;
}

uint32_t simd_get_vector_width(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, simd_detect_init_impl);

    return s_cached_caps.max_vector_width;
}

const char* simd_feature_name(simd_feature_t feature)
{
    switch (feature) {
        case SIMD_NONE:       return "None";
        case SIMD_SSE:        return "SSE";
        case SIMD_SSE2:       return "SSE2";
        case SIMD_SSE3:       return "SSE3";
        case SIMD_SSSE3:      return "SSSE3";
        case SIMD_SSE41:      return "SSE4.1";
        case SIMD_SSE42:      return "SSE4.2";
        case SIMD_AVX:        return "AVX";
        case SIMD_AVX2:       return "AVX2";
        case SIMD_FMA:        return "FMA";
        case SIMD_AVX512F:    return "AVX-512F";
        case SIMD_AVX512VL:   return "AVX-512VL";
        case SIMD_AVX512BW:   return "AVX-512BW";
        case SIMD_AVX512DQ:   return "AVX-512DQ";
        case SIMD_AVX512CD:   return "AVX-512CD";
        case SIMD_AVX512VNNI: return "AVX-512VNNI";
        case SIMD_NEON:       return "NEON";
        case SIMD_SVE:        return "SVE";
        case SIMD_SVE2:       return "SVE2";
        default:              return "Unknown";
    }
}

size_t simd_features_string(char* buffer, size_t size)
{
    if (!buffer || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "simd_features_string: invalid parameters");

            return 0;
    }

    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, simd_detect_init_impl);

    buffer[0] = '\0';
    size_t offset = 0;

    // List of features to check
    static const simd_feature_t features[] = {
        SIMD_SSE, SIMD_SSE2, SIMD_SSE3, SIMD_SSSE3,
        SIMD_SSE41, SIMD_SSE42, SIMD_AVX, SIMD_AVX2, SIMD_FMA,
        SIMD_AVX512F, SIMD_AVX512VL, SIMD_AVX512BW, SIMD_AVX512DQ,
        SIMD_AVX512VNNI, SIMD_NEON, SIMD_SVE, SIMD_SVE2
    };

    for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
        if (s_cached_caps.features & features[i]) {
            const char* name = simd_feature_name(features[i]);
            size_t name_len = strlen(name);

            if (offset + name_len + 2 < size) {
                if (offset > 0) {
                    buffer[offset++] = ' ';
                }
                memcpy(buffer + offset, name, name_len);
                offset += name_len;
                buffer[offset] = '\0';
            }
        }
    }

    return offset;
}
