#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_quantum_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(stdp_quantum_bridge)

#define LOG_MODULE "STDP_QUANTUM_BRIDGE"

//=============================================================================
// STDP Quantum Bridge Implementation
//=============================================================================

/*
 * Define NIMCP_STDP_QUANTUM_BRIDGE_IMPLEMENTATION to pull in the struct
 * definition and all function bodies from the header.
 */
#define NIMCP_STDP_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/stdp/nimcp_stdp_quantum_bridge.h"
#include "utils/logging/nimcp_logging.h"
