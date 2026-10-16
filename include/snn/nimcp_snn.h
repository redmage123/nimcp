//=============================================================================
// nimcp_snn.h - Spiking Neural Network Master Header
//=============================================================================
/**
 * @file nimcp_snn.h
 * @brief Master include file for NIMCP Spiking Neural Network module
 *
 * WHAT: Single include for all SNN functionality
 * WHY:  Convenience header for external users
 * HOW:  Includes all SNN submodules in correct order
 *
 * USAGE:
 * ```c
 * #include "snn/nimcp_snn.h"
 *
 * // Create SNN
 * snn_config_t config;
 * snn_config_feedforward(&config, 784, 256, 10);
 * snn_network_t* snn = snn_network_create(&config);
 *
 * // Forward pass
 * float outputs[10];
 * snn_network_forward(snn, inputs, 784, outputs, 10, 100.0f);
 *
 * // Cleanup
 * snn_network_destroy(snn);
 * ```
 *
 * INTEGRATION WITH EXISTING INFRASTRUCTURE:
 * - Uses neuron_t from core/neuralnet (LIF, Izhikevich models)
 * - Uses synapse_t from core/neuralnet (AMPA, NMDA, GABA types)
 * - Uses axon_t from core/axon (conduction delays)
 * - Uses dendrite_t from core/dendrite (spatial integration)
 * - Integrates with bio-async and brain immune system
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_H
#define NIMCP_SNN_H

/* Core types and enums */
#include "snn/nimcp_snn_types.h"

/* Configuration management */
#include "snn/nimcp_snn_config.h"

/* Network API */
#include "snn/nimcp_snn_network.h"

/* Spike encoding/decoding */
#include "snn/nimcp_snn_encoding.h"

/* Training algorithms */
#include "snn/nimcp_snn_training.h"

/* Bio-async integration */
#include "snn/nimcp_snn_bio_async.h"

/* Immune system integration */
#include "snn/nimcp_snn_immune.h"

/*
 * Future includes (as modules are implemented):
 * #include "snn/nimcp_snn_population.h"
 * #include "snn/nimcp_snn_simulation.h"
 */

#endif /* NIMCP_SNN_H */
