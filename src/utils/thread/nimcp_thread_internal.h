//=============================================================================
// nimcp_thread_internal.h - Internal APIs for thread subsystem
//=============================================================================

#ifndef NIMCP_THREAD_INTERNAL_H
#define NIMCP_THREAD_INTERNAL_H

#include "utils/thread/nimcp_thread.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Error Handling (shared across all thread modules)
//=============================================================================

/**
 * @brief Set thread-local error message
 * @param error_code Error status code
 * @param format Printf-style format string
 * @param ... Variable arguments for format
 */
void set_thread_error(int error_code, const char* format, ...);

/**
 * @brief Get last error message for current thread
 * @return Pointer to error message string (thread-local)
 */
const char* nimcp_thread_get_error(void);

/**
 * @brief Clear error state for current thread
 */
void nimcp_thread_clear_error(void);

//=============================================================================
// Initialization (shared state)
//=============================================================================

/**
 * @brief Get resource lock table singleton
 * @return Pointer to global resource lock table
 */
resource_lock_table_t* nimcp_thread_get_resource_table(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_THREAD_INTERNAL_H
