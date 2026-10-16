/**
 * @file nimcp_neural_logic_quantum_bridge.c
 * @brief Implementation of quantum-accelerated neural logic bridge
 *
 * WHAT: Quantum SAT solving for neural logic circuits
 * WHY:  √N speedup for satisfiability search
 * HOW:  Grover's algorithm on circuit CNF encoding
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

#define NIMCP_NEURAL_LOGIC_QUANTUM_BRIDGE_IMPLEMENTATION
#include "core/logic/nimcp_neural_logic_quantum_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include <string.h>  /* for memcpy */
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Opaque wrapper for neural_logic.c (avoids header dependency)
//=============================================================================

/**
 * @brief Opaque wrapper for neural_logic_quantum_default_config()
 * @param config_out Pointer to neural_logic_quantum_config_t buffer
 *
 * Called by nimcp_neural_logic.c which forward-declares this as
 * extern void neural_logic_quantum_default_config_opaque(void* config_out);
 */
void neural_logic_quantum_default_config_opaque(void* config_out) {
    if (!config_out) return;
    neural_logic_quantum_config_t cfg = neural_logic_quantum_default_config();
    *(neural_logic_quantum_config_t*)config_out = cfg;
}
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_logic_quantum_bridge)

#undef LOG_MODULE
#define LOG_MODULE "NEURAL_LOGIC_QUANTUM_BRIDGE"

//=============================================================================
