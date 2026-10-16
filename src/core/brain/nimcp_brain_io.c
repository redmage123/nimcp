//=============================================================================
// nimcp_brain_io.c - Brain I/O and Persistence
//=============================================================================
/**
 * @file nimcp_brain_io.c
 * @brief Brain I/O, metadata persistence, and JSON serialization
 *
 * This module contains approximately 900 lines extracted from nimcp_brain.c:
 * - save_metadata() / load_metadata() - Brain metadata persistence
 * - brain_export_json() / brain_import_json() - JSON export/import
 * - brain_save_json() / brain_load_json() - JSON file operations
 * - ensure_snapshot_dir() - Snapshot directory management
 * - brain_get_memory_usage() - Memory usage calculation
 *
 * @version 1.0.0
 * @date 2025-12-08
 */

#include "core/brain/nimcp_brain_io.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <cjson/cJSON.h>
#include <dirent.h>
#include <sys/stat.h>
#include "io/serialization/nimcp_serialization.h"

#define LOG_MODULE "BRAIN_IO"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_io, MESH_ADAPTER_CATEGORY_COGNITIVE)


// NOTE: Implementation functions are currently in nimcp_brain.c
// External declarations for linking
extern bool save_metadata(brain_t brain, const char* filepath);
extern bool load_metadata(brain_t brain, const char* filepath);
extern bool ensure_snapshot_dir(const char* snapshot_dir);
extern size_t brain_get_memory_usage(brain_t brain);
extern brain_t brain_import_json(const char* json_str);
extern bool brain_save_json(brain_t brain, const char* filepath, uint32_t flags);
extern brain_t brain_load_json(const char* filepath);
extern bool brain_resize_update_subsystems_internal(brain_t brain, neural_network_t new_base_network, uint32_t new_neuron_count);
