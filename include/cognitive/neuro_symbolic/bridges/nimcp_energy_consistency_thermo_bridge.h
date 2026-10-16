/**
 * @file nimcp_energy_consistency_thermo_bridge.h
 * @brief Bridge between Energy Consistency and Thermodynamics
 *
 * Tracks ATP cost of mathematical reasoning and Landauer bounds.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_ENERGY_CONSISTENCY_THERMO_BRIDGE_H
#define NIMCP_ENERGY_CONSISTENCY_THERMO_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BIO_MODULE_ENERGY_THERMO_BRIDGE  0x0399

typedef struct energy_thermo_bridge_config {
    bool enable_modulation;
    float atp_per_operation;
    float landauer_constant;
    float temperature_kelvin;
} energy_thermo_bridge_config_t;

typedef struct energy_thermo_bridge {
    bridge_base_t base;
    energy_thermo_bridge_config_t config;
    float total_atp_consumed;
    float total_landauer_cost;
    uint64_t operations_tracked;
} energy_thermo_bridge_t;

NIMCP_API energy_thermo_bridge_t* energy_thermo_bridge_create(void);
NIMCP_API void energy_thermo_bridge_destroy(energy_thermo_bridge_t* bridge);
NIMCP_API nimcp_error_t energy_thermo_bridge_track_reasoning_cost(
    energy_thermo_bridge_t* bridge, uint32_t operations);
NIMCP_API double energy_thermo_bridge_landauer_proof_cost(
    energy_thermo_bridge_t* bridge, uint32_t proof_bits);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENERGY_CONSISTENCY_THERMO_BRIDGE_H */
