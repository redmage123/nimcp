/**
 * @file nimcp_reasoning_factory.h
 * @brief MODULE 6: Reasoning Factory - Create pre-configured symbolic logic engines
 *
 * SINGLE RESPONSIBILITY: Create symbolic logic engines with standard configurations
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#ifndef NIMCP_REASONING_FACTORY_H
#define NIMCP_REASONING_FACTORY_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Factory Configuration Types
//=============================================================================

typedef enum {
    REASONING_SIZE_SMALL,   /**< Small: 100 facts, 50 rules */
    REASONING_SIZE_MEDIUM,  /**< Medium: 500 facts, 250 rules */
    REASONING_SIZE_LARGE    /**< Large: 1000 facts, 500 rules */
} reasoning_size_t;

//=============================================================================
// Factory API - SOLE RESPONSIBILITY
//=============================================================================

/**
 * @brief Create symbolic logic engine with default configuration
 */
symbolic_logic_t* create_default_symbolic_logic(reasoning_size_t size);

/**
 * @brief Create symbolic logic engine with custom configuration
 */
symbolic_logic_t* create_symbolic_logic_with_config(const logic_config_t* config);

/**
 * @brief Create symbolic logic engine optimized for forward chaining
 */
symbolic_logic_t* create_forward_chaining_engine(reasoning_size_t size);

/**
 * @brief Create symbolic logic engine optimized for backward chaining
 */
symbolic_logic_t* create_backward_chaining_engine(reasoning_size_t size);

/**
 * @brief Get last error message
 */
const char* reasoning_factory_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_FACTORY_H
