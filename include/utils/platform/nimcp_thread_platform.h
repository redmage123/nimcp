/**
 * @file nimcp_thread_platform.h
 * @brief Convenience header for all cross-platform threading primitives
 *
 * WHAT: Single include file for all platform abstraction modules
 * WHY:  Simplifies include statements, maintains backward compatibility
 * HOW:  Includes all modular platform headers
 *
 * DESIGN: This is a convenience header that aggregates the modular SRP-compliant
 *         platform abstraction components. Each component has a single responsibility:
 *
 *         - nimcp_platform.h          Platform detection and compiler macros
 *         - nimcp_platform_mutex.h    Mutex operations only
 *         - nimcp_platform_thread.h   Thread lifecycle only
 *         - nimcp_platform_cond.h     Condition variables only
 *         - nimcp_platform_rwlock.h   Read-write locks only
 *         - nimcp_platform_once.h     Once initialization only
 *         - nimcp_platform_time.h     Time measurement only
 *
 * USAGE: Include this header to get all platform abstractions:
 *        #include "utils/platform/nimcp_thread_platform.h"
 *
 * ALTERNATIVELY: Include specific modules for reduced compilation:
 *        #include "utils/platform/nimcp_platform_mutex.h"
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_THREAD_PLATFORM_H
#define NIMCP_THREAD_PLATFORM_H

/* Core platform detection */
#include "utils/platform/nimcp_platform.h"

/* Modular threading primitives (SRP-compliant) */
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_thread.h"
#include "utils/platform/nimcp_platform_cond.h"
#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/platform/nimcp_platform_once.h"

/* Time operations */
#include "utils/platform/nimcp_platform_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * BACKWARD COMPATIBILITY NOTES
 * ======================================================================== */

/*
 * This header now serves as a convenience aggregator for all platform
 * abstraction modules. Each module follows SRP (Single Responsibility
 * Principle) and can be included independently.
 *
 * MIGRATION: Existing code can continue to include this header without
 * changes. For new code, consider including only the specific modules
 * needed to reduce compilation dependencies.
 */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THREAD_PLATFORM_H */
