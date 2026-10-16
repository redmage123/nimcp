/**
 * @file nimcp_quantum_mcts_fep_bridge.h
 * @brief Bridge between Quantum MCTS and FEP
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_QUANTUM_MCTS_FEP_BRIDGE_H
#define NIMCP_QUANTUM_MCTS_FEP_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_QMCTS_FEP_BRIDGE  0x039D

typedef struct qmcts_fep_bridge {
    bridge_base_t base;
    float quantum_exploration_boost;
    uint64_t plans_executed;
} qmcts_fep_bridge_t;

NIMCP_API qmcts_fep_bridge_t* qmcts_fep_bridge_create(void);
NIMCP_API void qmcts_fep_bridge_destroy(qmcts_fep_bridge_t* bridge);
NIMCP_API float qmcts_fep_bridge_expected_value(
    qmcts_fep_bridge_t* bridge, const float* state, uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_MCTS_FEP_BRIDGE_H */
