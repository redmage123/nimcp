/**
 * @file nimcp_evolutionary_proof_logic_bridge.h
 * @brief Bridge between Evolutionary Proof and Logic System
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_EVOLUTIONARY_PROOF_LOGIC_BRIDGE_H
#define NIMCP_EVOLUTIONARY_PROOF_LOGIC_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_EVOPROOF_LOGIC_BRIDGE  0x039A

typedef struct evoproof_logic_bridge {
    bridge_base_t base;
    bool enable_axiom_expansion;
    bool enable_lemma_caching;
    uint64_t proofs_submitted;
    uint64_t axioms_retrieved;
} evoproof_logic_bridge_t;

NIMCP_API evoproof_logic_bridge_t* evoproof_logic_bridge_create(void);
NIMCP_API void evoproof_logic_bridge_destroy(evoproof_logic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EVOLUTIONARY_PROOF_LOGIC_BRIDGE_H */
