//=============================================================================
// nimcp_brain_io.h - Brain I/O and Persistence
//=============================================================================

#ifndef NIMCP_BRAIN_IO_H
#define NIMCP_BRAIN_IO_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// Metadata persistence
bool save_metadata(brain_t brain, const char* filepath);
bool load_metadata(brain_t brain, const char* filepath);

// Utility functions
bool ensure_snapshot_dir(const char* snapshot_dir);
size_t brain_get_memory_usage(brain_t brain);

// JSON import/export
brain_t brain_import_json(const char* json_str);
bool brain_save_json(brain_t brain, const char* filepath, uint32_t flags);
brain_t brain_load_json(const char* filepath);

// Internal resize function
bool brain_resize_update_subsystems_internal(brain_t brain, neural_network_t new_base_network, uint32_t new_neuron_count);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_IO_H
