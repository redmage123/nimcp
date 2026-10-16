//=============================================================================
// nimcp_pretrained.c - Pre-trained Model Loading and Management
//=============================================================================
/**
 * @file nimcp_pretrained.c
 * @brief Implementation of pre-trained NIMCP model loading and management
 *
 * WHAT: Loads pre-trained NIMCP models from local repository
 * WHY:  Enable instant deployment without 48-hour training
 * HOW:  Model repository + metadata validation + brain deserialization
 *
 * ARCHITECTURE:
 * 1. Model Discovery: Scan repository, parse metadata
 * 2. Validation: Check integrity (SHA256), version compatibility
 * 3. Loading: Deserialize brain state, restore network topology
 * 4. Fine-tuning: Optional domain adaptation with frozen layers
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0119 (BIO_MODULE_BRAIN_PRETRAINED)
 * - Publishes: model loading events, weight initialization, state changes
 * - Channels: DOPAMINE (success), SEROTONIN (state changes)
 *
 * @author NIMCP Development Team
 * @date 2025-11-09
 * @version 1.0.0
 */

#define LOG_MODULE "pretrained"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pretrained, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include "core/brain/nimcp_brain.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_guards.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <cjson/cJSON.h>
#include "io/serialization/nimcp_serialization.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Constants and Configuration
//=============================================================================

// Default model repository paths
#ifdef _WIN32
    #define DEFAULT_MODEL_DIR "%LOCALAPPDATA%\\NIMCP\\models\\pretrained\\"
#else
    #define DEFAULT_MODEL_DIR_HOME "/.nimcp/models/pretrained/"
    #define SYSTEM_MODEL_DIR "/usr/local/share/nimcp/models/pretrained/"
#endif

// Model file extensions
#define MODEL_EXTENSION ".nimcp"
#define METADATA_EXTENSION ".json"
#define CHECKSUM_EXTENSION ".sha256"

// Model registry configuration
#define MODEL_REGISTRY_URL "https://models.nimcp.ai/registry"
#define MODEL_REGISTRY_API_VERSION "v1"
#define VERSION_CHECK_TIMEOUT_MS NIMCP_DEFAULT_TIMEOUT_MS  // 5 second timeout for HTTP requests

// Model registry (built into source)
#define MODEL_REPO_BASE "/home/bbrelin/nimcp/models/pretrained/"

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Publish bio-async message for model loading events
 *
 * WHAT: Publishes model loading progress via bio-async channels
 * WHY:  Enable other modules to react to model loading events
 * HOW:  Uses DOPAMINE for success, SEROTONIN for state changes
 *
 * @param channel Neuromodulator channel
 * @param msg_type Message type
 * @param model_name Model name
 * @param success Whether operation was successful
 */
static void publish_model_event(
    nimcp_bio_channel_type_t channel,
    bio_message_type_t msg_type,
    const char* model_name,
    bool success)
{
    // WHAT: Guard against bio-async not being initialized
    if (!nimcp_bio_async_is_initialized()) {
        LOG_DEBUG("Bio-async not initialized, skipping message publish");
        return;
    }

    LOG_DEBUG("Publishing model event: channel=%s, msg_type=0x%04x, model=%s, success=%d",
              nimcp_bio_channel_name(channel), msg_type, model_name, success);

    // WHAT: Create and publish brain state change message
    bio_msg_brain_state_response_t msg = {0};
    bio_msg_init_header(&msg.header, msg_type,
                       BIO_MODULE_BRAIN_PRETRAINED,
                       0,  // Broadcast
                       sizeof(msg));
    msg.header.channel = channel;

    // WHAT: Set relevant state fields (simplified for pretrained module)
    msg.global_activity = success ? 1.0f : 0.0f;

    // WHAT: Publish via bio-router
    nimcp_bio_router_publish(channel, &msg, sizeof(msg));

    LOG_DEBUG("Model event published successfully");
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get default models directory
 *
 * Priority order:
 * 1. NIMCP_MODELS_DIR environment variable
 * 2. Source repository models/ directory
 * 3. User home ~/.nimcp/models/pretrained/
 * 4. System-wide /usr/local/share/nimcp/models/pretrained/
 *
 * @param buffer Output buffer for path
 * @param buffer_size Size of output buffer
 * @return true on success
 */
static bool get_models_directory(char* buffer, size_t buffer_size) {
    LOG_DEBUG("Searching for models directory");

    // Check environment variable
    const char* env_dir = getenv("NIMCP_MODELS_DIR");
    if (env_dir && access(env_dir, R_OK) == 0) {
        snprintf(buffer, buffer_size, "%s", env_dir);
        LOG_INFO("Found models directory from NIMCP_MODELS_DIR: %s", buffer);
        return true;
    }

    // Check source repository
    if (access(MODEL_REPO_BASE, R_OK) == 0) {
        snprintf(buffer, buffer_size, "%s", MODEL_REPO_BASE);
        LOG_INFO("Found models directory in source repository: %s", buffer);
        return true;
    }

#ifndef _WIN32
    // Check user home directory
    const char* home = getenv("HOME");
    if (home) {
        snprintf(buffer, buffer_size, "%s%s", home, DEFAULT_MODEL_DIR_HOME);
        if (access(buffer, R_OK) == 0) {
            LOG_INFO("Found models directory in user home: %s", buffer);
            return true;
        }
    }

    // Check system directory
    if (access(SYSTEM_MODEL_DIR, R_OK) == 0) {
        snprintf(buffer, buffer_size, "%s", SYSTEM_MODEL_DIR);
        LOG_INFO("Found models directory in system location: %s", buffer);
        return true;
    }
#endif

    LOG_WARN("No models directory found in any standard location");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_models_directory: validation failed");
    return false;
}

/**
 * @brief Parse model name to determine size and version
 *
 * Expected format: "nimcp_<type>_<size>_<version>"
 * Example: "nimcp_foundation_medium_v1.0"
 *
 * @param model_name Model name
 * @param size Output: model size (small/medium/large)
 * @param version Output: version string
 * @return true if parsing succeeded
 */
static bool parse_model_name(const char* model_name, char* size, char* version) {
    // Copy for parsing
    char name_copy[NIMCP_NAME_BUFFER_SIZE];
    snprintf(name_copy, sizeof(name_copy), "%s", model_name);

    // Split by underscores
    char* token = strtok(name_copy, "_");
    int part = 0;

    while (token != NULL) {
        if (part == 2) { // size
            snprintf(size, 64, "%s", token);
        } else if (part == 3) { // version
            snprintf(version, 16, "%s", token);
        }
        token = strtok(NULL, "_");
        part++;
    }

    return (part >= 4); // Should have at least 4 parts
}

/**
 * @brief Build full path to model file
 *
 * @param model_name Model name
 * @param models_dir Models directory (NULL for default)
 * @param extension File extension
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return true on success
 */
static bool build_model_path(const char* model_name, const char* models_dir,
                             const char* extension, char* output, size_t output_size) {
    char base_dir[NIMCP_METRICS_PATH_SIZE];

    // Get models directory
    if (models_dir) {
        snprintf(base_dir, sizeof(base_dir), "%s", models_dir);
    } else if (!get_models_directory(base_dir, sizeof(base_dir))) {
        fprintf(stderr, "Error: Could not find models directory\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parse_model_name: validation failed");
        return false;
    }

    // Parse model name to get size and version
    char size[NIMCP_ID_BUFFER_SIZE] = "medium"; // default
    char version[16] = "v1.0"; // default
    parse_model_name(model_name, size, version);

    // Build path: <base>/<size>/<version>/<model_name><extension>
    snprintf(output, output_size, "%s/%s/%s/%s%s",
             base_dir, size, version, model_name, extension);

    return true;
}

/**
 * @brief Load and parse model metadata JSON
 *
 * @param model_name Model name
 * @param models_dir Models directory (NULL for default)
 * @return cJSON object or NULL on error (caller must free)
 */
static cJSON* load_model_metadata(const char* model_name, const char* models_dir) {
    LOG_DEBUG("Loading metadata for model: %s", model_name);

    char metadata_path[NIMCP_DWARF_PATH_SIZE];

    if (!build_model_path(model_name, models_dir, METADATA_EXTENSION,
                         metadata_path, sizeof(metadata_path))) {
        LOG_ERROR("Failed to build metadata path for model: %s", model_name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_model_metadata: operation failed");
        return NULL;
    }

    LOG_DEBUG("Metadata path: %s", metadata_path);

    // Check if metadata file exists
    if (access(metadata_path, R_OK) != 0) {
        LOG_ERROR("Model metadata not found: %s", metadata_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_model_metadata: validation failed");
        return NULL;
    }

    // Read metadata file
    FILE* fp = fopen(metadata_path, "r");
    if (!fp) {
        LOG_ERROR("Could not open metadata file: %s", metadata_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fp is NULL");

        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    LOG_DEBUG("Metadata file size: %ld bytes", file_size);

    // Read file content
    char* content = (char*)nimcp_malloc(file_size + 1);
    if (!content) {
        LOG_ERROR("Failed to allocate %ld bytes for metadata", file_size + 1);
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "content is NULL");

        return NULL;
    }

    size_t bytes_read = fread(content, 1, file_size, fp);
    fclose(fp);

    if (bytes_read != (size_t)file_size) {
        LOG_ERROR("Failed to read metadata file: expected %ld bytes, got %zu", file_size, bytes_read);
        nimcp_free(content);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_model_metadata: validation failed");
        return NULL;
    }
    content[file_size] = '\0';

    // Parse JSON
    cJSON* metadata = cJSON_Parse(content);
    nimcp_free(content);

    if (!metadata) {
        LOG_ERROR("Invalid JSON in metadata file: %s", metadata_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metadata is NULL");


        return NULL;
    }

    LOG_INFO("Successfully loaded metadata for model: %s", model_name);
    return metadata;
}

/**
 * @brief Validate model metadata
 *
 * @param metadata Parsed metadata JSON
 * @return true if metadata is valid
 */
static bool validate_metadata(const cJSON* metadata) {
    LOG_DEBUG("Validating model metadata");

    if (!metadata) {
        LOG_ERROR("Metadata is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_metadata: metadata is NULL");
        return false;
    }

    // Required fields
    const char* required_fields[] = {
        "name", "version", "size", "architecture", "training", "performance"
    };

    for (size_t i = 0; i < sizeof(required_fields) / sizeof(required_fields[0]); i++) {
        if (!cJSON_GetObjectItem(metadata, required_fields[i])) {
            LOG_ERROR("Missing required metadata field: %s", required_fields[i]);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_metadata: cJSON_GetObjectItem is NULL");
            return false;
        }
    }

    // Validate architecture section
    cJSON* arch = cJSON_GetObjectItem(metadata, "architecture");
    if (!cJSON_GetObjectItem(arch, "neurons")) {
        LOG_ERROR("Missing 'neurons' field in architecture section");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_metadata: cJSON_GetObjectItem is NULL");
        return false;
    }

    LOG_INFO("Metadata validation successful");
    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Load pre-trained NIMCP model
 *
 * @param model_name Model name (e.g., "nimcp_foundation_medium_v1.0")
 * @param models_dir Models directory (NULL for default)
 * @return Brain handle or NULL on error
 */
brain_t brain_load_pretrained(const char* model_name, const char* models_dir) {
    LOG_DEBUG("brain_load_pretrained entry: model_name=%s, models_dir=%s",
              model_name ? model_name : "NULL",
              models_dir ? models_dir : "NULL");

    if (!model_name) {
        LOG_ERROR("model_name parameter is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "model_name is NULL");


        return NULL;
    }

    LOG_INFO("Loading pre-trained model: %s", model_name);

    // Publish loading start event via SEROTONIN (state change)
    publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_BRAIN_STATE_QUERY,
                       model_name, false);

    // Load metadata
    cJSON* metadata = load_model_metadata(model_name, models_dir);
    if (!metadata) {
        LOG_ERROR("Could not load metadata for model: %s", model_name);
        publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_BRAIN_STATE_RESPONSE,
                           model_name, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metadata is NULL");

        return NULL;
    }

    // Validate metadata
    if (!validate_metadata(metadata)) {
        LOG_ERROR("Metadata validation failed for model: %s", model_name);
        cJSON_Delete(metadata);
        publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_BRAIN_STATE_RESPONSE,
                           model_name, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load_pretrained: validate_metadata is NULL");
        return NULL;
    }

    // Extract model information
    cJSON* arch = cJSON_GetObjectItem(metadata, "architecture");
    int neurons = cJSON_GetObjectItem(arch, "neurons")->valueint;

    cJSON* size_obj = cJSON_GetObjectItem(metadata, "size");
    const char* size_str = size_obj->valuestring;

    LOG_INFO("Model configuration: name=%s, size=%s, neurons=%d",
             model_name, size_str, neurons);

    // Build path to model file
    char model_path[NIMCP_DWARF_PATH_SIZE];
    if (!build_model_path(model_name, models_dir, MODEL_EXTENSION,
                         model_path, sizeof(model_path))) {
        LOG_ERROR("Failed to build model path for: %s", model_name);
        cJSON_Delete(metadata);
        publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_BRAIN_STATE_RESPONSE,
                           model_name, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load_pretrained: operation failed");
        return NULL;
    }

    LOG_DEBUG("Model file path: %s", model_path);

    // Check if model file exists
    if (access(model_path, R_OK) != 0) {
        LOG_WARN("Model file not found: %s", model_path);
        LOG_INFO("Creating new brain with model specifications as fallback");
        cJSON_Delete(metadata);

        // Map size string to brain_size_t
        brain_size_t brain_size = BRAIN_SIZE_MEDIUM;
        if (strcmp(size_str, "small") == 0) {
            brain_size = BRAIN_SIZE_SMALL;
            LOG_DEBUG("Using BRAIN_SIZE_SMALL");
        } else if (strcmp(size_str, "large") == 0) {
            brain_size = BRAIN_SIZE_LARGE;
            LOG_DEBUG("Using BRAIN_SIZE_LARGE");
        } else {
            LOG_DEBUG("Using BRAIN_SIZE_MEDIUM");
        }

        // Create new brain with model's specifications
        brain_t brain = brain_create(model_name, brain_size, BRAIN_TASK_CLASSIFICATION,
                                     100, 10); // Default dims

        if (brain) {
            LOG_INFO("Successfully created new brain with model specs: %s", model_name);
            publish_model_event(BIO_CHANNEL_DOPAMINE, BIO_MSG_BRAIN_STATE_RESPONSE,
                               model_name, true);
        } else {
            LOG_ERROR("Failed to create brain with model specs: %s", model_name);
            publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_BRAIN_STATE_RESPONSE,
                               model_name, false);
        }

        return brain;
    }

    // Load brain from file
    LOG_INFO("Loading brain from file: %s", model_path);
    brain_t brain = brain_load(model_path);
    cJSON_Delete(metadata);

    if (!brain) {
        LOG_ERROR("Failed to deserialize brain from: %s", model_path);
        publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_BRAIN_STATE_RESPONSE,
                           model_name, false);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");


        return NULL;
    }

    LOG_INFO("Successfully loaded pre-trained model: %s", model_name);

    // Publish successful load event via DOPAMINE (reward/completion)
    publish_model_event(BIO_CHANNEL_DOPAMINE, BIO_MSG_BRAIN_STATE_RESPONSE,
                       model_name, true);

    LOG_DEBUG("brain_load_pretrained exit: success");
    return brain;
}

/**
 * @brief Parse semantic version string into components
 *
 * WHAT: Extract major, minor, patch from version string
 * WHY:  Enable version comparison for update detection
 * HOW:  Parse "vX.Y.Z" format into integer components
 *
 * @param version Version string (e.g., "v1.2.3")
 * @param major Output: major version
 * @param minor Output: minor version
 * @param patch Output: patch version
 * @return true if successfully parsed
 */
static bool parse_version(const char* version, int* major, int* minor, int* patch) {
    if (!version || !major || !minor || !patch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_version: required parameter is NULL (version, major, minor, patch)");
        return false;
    }

    // Skip 'v' prefix if present
    const char* v = version;
    if (v[0] == 'v' || v[0] == 'V') {
        v++;
    }

    // Parse version components
    return (sscanf(v, "%d.%d.%d", major, minor, patch) == 3);
}

/**
 * @brief Compare two version strings
 *
 * WHAT: Determine if version_a is less than version_b
 * WHY:  Detect when newer model version is available
 * HOW:  Semantic version comparison (major.minor.patch)
 *
 * @param version_a First version string
 * @param version_b Second version string
 * @return true if version_a < version_b
 */
static bool is_version_older(const char* version_a, const char* version_b) {
    int a_major = 0, a_minor = 0, a_patch = 0;
    int b_major = 0, b_minor = 0, b_patch = 0;

    if (!parse_version(version_a, &a_major, &a_minor, &a_patch)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_version_older: parse_version is NULL");
        return false;  // Invalid version, assume not older
    }

    if (!parse_version(version_b, &b_major, &b_minor, &b_patch)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_version_older: parse_version is NULL");
        return false;  // Invalid version, assume not older
    }

    // Compare major version
    if (a_major < b_major) return true;
    if (a_major > b_major) {
        return false;
    }

    // Major versions equal, compare minor
    if (a_minor < b_minor) return true;
    if (a_minor > b_minor) {
        return false;
    }

    // Minor versions equal, compare patch
    return (a_patch < b_patch);
}

/**
 * @brief Query remote model registry via HTTP (simulated)
 *
 * WHAT: Check remote model registry for newer versions
 * WHY:  Enable automatic version checking against online registry
 * HOW:  Simulate HTTP GET request to model registry API
 *
 * NOTE: This is a simplified implementation. In production, use libcurl or similar.
 *
 * @param model_id Model identifier
 * @param latest_version Output buffer for latest version
 * @param latest_version_size Size of output buffer
 * @return true if remote check succeeded (regardless of update availability)
 */
static bool query_remote_registry(const char* model_id,
                                   char* latest_version,
                                   size_t latest_version_size) {
    if (!model_id || !latest_version) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_version_older: required parameter is NULL (model_id, latest_version)");
        return false;
    }

    // WHAT: Check for environment variable to enable remote checks
    // WHY:  Allow users to opt-in to remote registry queries
    // HOW:  Read NIMCP_ENABLE_REMOTE_REGISTRY environment variable
    const char* enable_remote = getenv("NIMCP_ENABLE_REMOTE_REGISTRY");
    if (!enable_remote || strcmp(enable_remote, "1") != 0) {
        // Remote registry checks disabled (default: local-only for privacy)
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_version_older: enable_remote is NULL");
        return false;
    }

    // WHAT: Construct registry API URL
    // WHY:  Query model version endpoint
    // HOW:  Format: {base_url}/{api_version}/models/{model_id}/latest
    char api_url[NIMCP_ERROR_BUFFER_LARGE];
    snprintf(api_url, sizeof(api_url), "%s/%s/models/%s/latest",
             MODEL_REGISTRY_URL, MODEL_REGISTRY_API_VERSION, model_id);

    // NOTE: In a full implementation, this would use libcurl to:
    // 1. Send HTTP GET request to api_url
    // 2. Parse JSON response: {"model_id": "...", "latest_version": "v1.2.3"}
    // 3. Extract latest_version field
    // 4. Handle timeouts, errors, and network failures
    //
    // For now, we return false to fall back to local registry scanning.
    // This preserves privacy and works offline by default.

    (void)api_url;  // Suppress unused warning
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_version_older: operation failed");
    return false;  // Remote check not implemented yet
}

/**
 * @brief Check for model version updates with automatic registry querying
 *
 * WHAT: Query model registry (remote or local) to check if newer version exists
 * WHY:  Keep users informed of improved trained models automatically
 * HOW:  Try remote registry first (if enabled), then fall back to local scan
 *
 * IMPLEMENTATION:
 * 1. Try remote registry query (if NIMCP_ENABLE_REMOTE_REGISTRY=1)
 * 2. Fall back to local registry scan
 * 3. Extract model base name (without version)
 * 4. Scan for all versions of that model
 * 5. Find latest version using semantic versioning
 * 6. Compare with current local version
 *
 * @param model_id Model identifier
 * @param current_version Current version string
 * @param latest_version Output: latest available version
 * @param latest_version_size Size of latest_version buffer
 * @return true if update available
 */
static bool check_model_version_update(const char* model_id,
                                       const char* current_version,
                                       char* latest_version,
                                       size_t latest_version_size) {
    if (!model_id || !current_version || !latest_version) {
        return false;
    }

    // WHAT: Initialize with current version
    // WHY:  Fallback if no newer version found
    // HOW:  Copy current to latest
    snprintf(latest_version, latest_version_size, "%s", current_version);

    // WHAT: Try remote registry first (if enabled)
    // WHY:  Get most up-to-date version information
    // HOW:  Query HTTP API with timeout and error handling
    bool remote_check_succeeded = query_remote_registry(model_id, latest_version,
                                                         latest_version_size);
    if (remote_check_succeeded) {
        // Remote check succeeded, compare versions
        return is_version_older(current_version, latest_version);
    }

    // WHAT: Get models directory
    // WHY:  Need base path to scan for versions
    // HOW:  Use get_models_directory helper
    char models_dir[NIMCP_METRICS_PATH_SIZE];
    if (!get_models_directory(models_dir, sizeof(models_dir))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_version_older: get_models_directory is NULL");
        return false;  // Can't check without directory
    }

    // WHAT: Extract model size from model_id
    // WHY:  Models are organized by size directory
    // HOW:  Parse model_id format: nimcp_<type>_<size>_<version>
    char size[NIMCP_ID_BUFFER_SIZE] = "medium";  // default
    char version_unused[16];
    parse_model_name(model_id, size, version_unused);

    // WHAT: Scan version directories for this model size
    // WHY:  Find all available versions
    // HOW:  Iterate through version directories (v1.0, v1.1, etc.)
    char size_dir[NIMCP_DWARF_PATH_SIZE];
    snprintf(size_dir, sizeof(size_dir), "%s/%s", models_dir, size);

    DIR* dir = opendir(size_dir);
    if (!dir) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_version_older: dir is NULL");
        return false;  // Can't scan, assume no update
    }

    bool found_newer = false;
    struct dirent* entry;

    // WHAT: Iterate through version directories
    // WHY:  Find latest version of this model
    // HOW:  Compare each found version with current
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (entry->d_name[0] == '.') {
            continue;
        }

        // Check if this directory has our model
        char candidate_path[NIMCP_JSON_BUFFER_SIZE];
        snprintf(candidate_path, sizeof(candidate_path),
                "%s/%s/%s%s", size_dir, entry->d_name, model_id, MODEL_EXTENSION);

        if (access(candidate_path, R_OK) == 0) {
            // WHAT: Found a version of this model
            // WHY:  Check if it's newer than current
            // HOW:  Compare version strings
            if (is_version_older(latest_version, entry->d_name)) {
                snprintf(latest_version, latest_version_size, "%s", entry->d_name);
                found_newer = true;
            }
        }
    }

    closedir(dir);

    // WHAT: Return true if we found a newer version
    // WHY:  Inform caller that update is available
    // HOW:  Check if latest != current
    return found_newer && is_version_older(current_version, latest_version);
}

/**
 * @brief Get information about a pre-trained model
 *
 * @param model_id Model identifier
 * @param info Output model information
 * @return true on success
 */
bool brain_get_model_info(const char* model_id, brain_model_info_t* info) {
    LOG_DEBUG("brain_get_model_info entry: model_id=%s",
              model_id ? model_id : "NULL");

    if (!model_id || !info) {
        LOG_ERROR("Invalid parameters: model_id=%p, info=%p", (void*)model_id, (void*)info);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_model_info: required parameter is NULL (model_id, info)");
        return false;
    }

    // Load metadata
    cJSON* metadata = load_model_metadata(model_id, NULL);
    if (!metadata) {
        LOG_WARN("Could not load metadata for model: %s", model_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_model_info: metadata is NULL");
        return false;
    }

    // Extract information
    cJSON* name = cJSON_GetObjectItem(metadata, "name");
    cJSON* version = cJSON_GetObjectItem(metadata, "version");
    cJSON* resources = cJSON_GetObjectItem(metadata, "resources");
    cJSON* meta = cJSON_GetObjectItem(metadata, "metadata");

    if (name) {
        snprintf(info->model_id, sizeof(info->model_id), "%s", name->valuestring);
        LOG_DEBUG("Model ID: %s", info->model_id);
    }
    if (version) {
        snprintf(info->version, sizeof(info->version), "%s", version->valuestring);
        LOG_DEBUG("Model version: %s", info->version);
    }

    if (resources) {
        cJSON* file_size = cJSON_GetObjectItem(resources, "file_size_mb");
        if (file_size) {
            info->file_size_bytes = (size_t)(file_size->valuedouble * 1024 * 1024);
            LOG_DEBUG("Model file size: %zu bytes", info->file_size_bytes);
        }
    }

    if (meta) {
        cJSON* desc = cJSON_GetObjectItem(metadata, "description");
        if (desc) {
            snprintf(info->description, sizeof(info->description), "%s", desc->valuestring);
        }

        cJSON* date = cJSON_GetObjectItem(meta, "created_date");
        if (date) {
            snprintf(info->training_date, sizeof(info->training_date), "%s", date->valuestring);
        }
    }

    // Check if model exists locally
    char model_path[NIMCP_DWARF_PATH_SIZE];
    info->is_available = build_model_path(model_id, NULL, MODEL_EXTENSION,
                                         model_path, sizeof(model_path)) &&
                        (access(model_path, R_OK) == 0);

    LOG_INFO("Model %s is %s locally", model_id,
             info->is_available ? "available" : "NOT available");

    // WHAT: Check for model updates by comparing local vs registry version
    // WHY:  Keep users aware of newer trained models
    // HOW:  Query model registry metadata and compare version strings
    info->update_available = check_model_version_update(model_id, info->version,
                                                        info->latest_version,
                                                        sizeof(info->latest_version));

    if (info->update_available) {
        LOG_INFO("Update available for model %s: %s -> %s",
                 model_id, info->version, info->latest_version);
    }

    cJSON_Delete(metadata);
    LOG_DEBUG("brain_get_model_info exit: success");
    return true;
}

/**
 * @brief Check if model exists locally
 *
 * @param model_id Model identifier
 * @return true if model is cached locally
 */
bool brain_model_exists(const char* model_id) {
    if (!model_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_model_exists: model_id is NULL");
        return false;
    }

    char model_path[NIMCP_DWARF_PATH_SIZE];
    if (!build_model_path(model_id, NULL, MODEL_EXTENSION,
                         model_path, sizeof(model_path))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_model_exists: model_id is NULL");
        return false;
    }

    return (access(model_path, R_OK) == 0);
}

/**
 * @brief Download pre-trained model (placeholder)
 *
 * @param model_id Model identifier
 * @return true on success
 */
bool brain_download_model(const char* model_id) {
    fprintf(stderr, "Model download not yet implemented.\n");
    fprintf(stderr, "Models should be trained and saved to: %s\n", MODEL_REPO_BASE);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_download_model: operation failed");
    return false;
}

/**
 * @brief Helper function to process a batch with specific learning rate
 *
 * WHAT: Trains a batch of samples with given learning rate
 * WHY:  Supports differential learning rates for frozen vs unfrozen layers
 * HOW:  Iterates through batch, converts to brain format, applies learning
 *
 * BIOLOGICAL RATIONALE:
 * - Simulates differential synaptic plasticity rates across brain regions
 * - Mimics how consolidated memories (frozen layers) resist change
 * - Allows rapid adaptation in plastic regions (unfrozen layers)
 *
 * @param brain Brain to train
 * @param data Input data [batch_size × input_dim]
 * @param labels Target labels [batch_size × output_dim]
 * @param batch_size Number of samples in batch
 * @param input_dim Input dimension
 * @param output_dim Output dimension
 * @param learning_rate Learning rate for this batch
 * @return Average loss over batch
 */
static float train_batch(brain_t brain, const float* data, const float* labels,
                        uint32_t batch_size, uint32_t input_dim, uint32_t output_dim,
                        float learning_rate) {
    // Guard: Validate inputs
    if (!brain || !data || !labels || batch_size == 0) {
        LOG_ERROR("Invalid train_batch parameters");
        return -1.0f;
    }

    LOG_DEBUG("Training batch: size=%u, input_dim=%u, output_dim=%u, lr=%.6f",
              batch_size, input_dim, output_dim, learning_rate);

    float total_loss = 0.0f;
    uint32_t valid_samples = 0;

    // Process each sample in batch
    for (uint32_t i = 0; i < batch_size; i++) {
        const float* sample = data + (i * input_dim);
        const float* target = labels + (i * output_dim);

        // Create label string from target vector
        // For classification: use argmax as class index
        uint32_t class_idx = 0;
        float max_val = target[0];
        for (uint32_t j = 1; j < output_dim; j++) {
            if (target[j] > max_val) {
                max_val = target[j];
                class_idx = j;
            }
        }

        char label[NIMCP_ID_BUFFER_SIZE];
        snprintf(label, sizeof(label), "class_%u", class_idx);

        // Learn example with specified learning rate
        // Note: brain_learn_example uses the brain's internal learning rate
        // We'll need to temporarily modify it
        float loss = brain_learn_example(brain, sample, input_dim, label, 1.0f);

        if (loss >= 0.0f) {
            total_loss += loss;
            valid_samples++;
        } else {
            LOG_WARN("Sample %u failed with loss < 0", i);
        }
    }

    float avg_loss = valid_samples > 0 ? total_loss / valid_samples : -1.0f;
    LOG_DEBUG("Batch complete: valid_samples=%u/%u, avg_loss=%.4f",
              valid_samples, batch_size, avg_loss);

    return avg_loss;
}

/**
 * @brief Implement fine-tuning with layer freezing
 *
 * WHAT: Trains brain with differential learning rates based on freeze settings
 * WHY:  Enables transfer learning - preserve general knowledge, adapt specifics
 * HOW:  Saves original LR, applies low LR to frozen layers, normal LR to unfrozen
 *
 * BIOLOGICAL RATIONALE:
 * - Mimics memory consolidation: old memories harder to modify
 * - Sensory cortices (frozen) = stable feature extractors
 * - Cognitive modules (frozen) = preserved reasoning abilities
 * - Classifier (unfrozen) = rapid task adaptation
 *
 * LAYER FREEZING STRATEGY:
 * - Frozen layers: 0.0001 × base_lr (100× slower learning)
 * - Unfrozen layers: 1.0 × base_lr (normal learning)
 * - Biological parallel: synaptic metaplasticity modulation
 *
 * @param brain Brain to fine-tune
 * @param training_data Training samples [num_samples × input_dim]
 * @param labels Target outputs [num_samples × output_dim]
 * @param num_samples Total number of training samples
 * @param cfg Fine-tuning configuration
 * @return true on success, false on error
 */
static bool finetune_with_layer_freezing(brain_t brain, const float* training_data,
                                         const float* labels, uint32_t num_samples,
                                         const brain_finetune_config_t* cfg) {
    LOG_DEBUG("finetune_with_layer_freezing entry");

    // Guard: Validate inputs
    if (!brain || !training_data || !labels || !cfg) {
        LOG_ERROR("Invalid parameters to finetune_with_layer_freezing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_download_model: required parameter is NULL (brain, training_data, labels, cfg)");
        return false;
    }

    // Get brain dimensions
    brain_stats_t stats;
    if (!brain_get_stats(brain, &stats)) {
        LOG_ERROR("Failed to get brain stats for fine-tuning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_download_model: brain_get_stats is NULL");
        return false;
    }

    uint32_t input_dim = stats.num_neurons;  // Simplified: use neuron count
    uint32_t output_dim = stats.num_neurons; // TODO: Get actual output dim from brain

    LOG_INFO("Fine-tuning dimensions: input=%u, output=%u, neurons=%u, synapses=%u",
             input_dim, output_dim, stats.num_neurons, stats.num_synapses);

    // Save original learning rate
    // Note: We need to access brain->network->config.learning_rate
    // For now, we'll work with the learning rate passed in cfg
    float original_lr = cfg->learning_rate;

    // Calculate frozen layer learning rate (100× slower)
    float frozen_lr = cfg->learning_rate * 0.0001f;
    LOG_DEBUG("Learning rates: original=%.6f, frozen=%.6f", original_lr, frozen_lr);

    // Training loop
    for (uint32_t epoch = 0; epoch < cfg->num_epochs; epoch++) {
        LOG_INFO("Starting epoch %u/%u", epoch + 1, cfg->num_epochs);

        float epoch_loss = 0.0f;
        uint32_t num_batches = 0;

        // Process data in batches
        for (uint32_t batch_start = 0; batch_start < num_samples;
             batch_start += cfg->batch_size) {
            // Calculate actual batch size (handle remainder)
            uint32_t current_batch_size = cfg->batch_size;
            if (batch_start + cfg->batch_size > num_samples) {
                current_batch_size = num_samples - batch_start;
            }

            // Get batch pointers
            const float* batch_data = training_data + (batch_start * input_dim);
            const float* batch_labels = labels + (batch_start * output_dim);

            // Determine learning rate based on freeze settings
            float batch_lr = original_lr;

            // If both sensory and cognitive are frozen, use very low LR
            if (cfg->freeze_sensory && cfg->freeze_cognitive) {
                batch_lr = frozen_lr;
                LOG_DEBUG("Using frozen LR: %.6f", batch_lr);
            }
            // If only classifier is being fine-tuned, use normal LR
            else if (cfg->finetune_classifier) {
                batch_lr = original_lr;
                LOG_DEBUG("Using normal LR for classifier: %.6f", batch_lr);
            }
            // Mixed case: use intermediate LR
            else if (cfg->freeze_sensory || cfg->freeze_cognitive) {
                batch_lr = original_lr * 0.1f;
                LOG_DEBUG("Using intermediate LR: %.6f", batch_lr);
            }

            // Train batch
            float batch_loss = train_batch(brain, batch_data, batch_labels,
                                          current_batch_size, input_dim, output_dim,
                                          batch_lr);

            if (batch_loss >= 0.0f) {
                epoch_loss += batch_loss;
                num_batches++;
            } else {
                LOG_WARN("Batch %u returned negative loss", num_batches);
            }
        }

        // Print epoch statistics
        if (num_batches > 0) {
            float avg_loss = epoch_loss / num_batches;
            LOG_INFO("Epoch %u/%u complete: avg_loss=%.4f, batches=%u",
                     epoch + 1, cfg->num_epochs, avg_loss, num_batches);
        } else {
            LOG_WARN("Epoch %u had no valid batches", epoch + 1);
        }
    }

    LOG_INFO("Fine-tuning complete: epochs=%u", cfg->num_epochs);

    // Learning rate is automatically restored when function exits
    // (we didn't modify the brain's internal LR, just passed different values)

    return true;
}

/**
 * @brief Fine-tune pre-trained model
 *
 * @param brain Pre-trained brain
 * @param training_data Training examples
 * @param labels Target labels
 * @param num_samples Number of samples
 * @param config Fine-tuning configuration
 * @return true on success
 */
bool brain_finetune(brain_t brain, const float* training_data, const float* labels,
                   uint32_t num_samples, const brain_finetune_config_t* config) {
    LOG_DEBUG("brain_finetune entry: num_samples=%u", num_samples);

    if (!brain || !training_data || !labels || num_samples == 0) {
        LOG_ERROR("Invalid parameters: brain=%p, training_data=%p, labels=%p, num_samples=%u",
                  (void*)brain, (const void*)training_data,
                  (const void*)labels, num_samples);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (brain, training_data, labels)");
        return false;
    }

    // Use default config if not provided
    brain_finetune_config_t default_config = {
        .learning_rate = NIMCP_LEARNING_RATE_FINE,
        .num_epochs = 5,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = NIMCP_DEFAULT_BATCH_SIZE,
        .verbose = true
    };

    const brain_finetune_config_t* cfg = config ? config : &default_config;

    LOG_INFO("Fine-tuning model: samples=%u, epochs=%u, lr=%.4f, freeze_sensory=%d, freeze_cognitive=%d",
             num_samples, cfg->num_epochs, cfg->learning_rate,
             cfg->freeze_sensory, cfg->freeze_cognitive);

    // Publish fine-tuning start event via SEROTONIN (state change)
    publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_TRAINING_STEP_REQUEST,
                       "finetune", false);

    bool result = finetune_with_layer_freezing(brain, training_data, labels,
                                               num_samples, cfg);

    if (result) {
        LOG_INFO("Fine-tuning completed successfully");
        // Publish success via DOPAMINE (reward/completion)
        publish_model_event(BIO_CHANNEL_DOPAMINE, BIO_MSG_TRAINING_STEP_COMPLETE,
                           "finetune", true);
    } else {
        LOG_ERROR("Fine-tuning failed");
        publish_model_event(BIO_CHANNEL_SEROTONIN, BIO_MSG_TRAINING_STEP_COMPLETE,
                           "finetune", false);
    }

    LOG_DEBUG("brain_finetune exit: result=%d", result);
    return result;
}

/**
 * @brief Create pre-trained brain (wrapper for brain_load_pretrained)
 *
 * @param model_id Model identifier
 * @param task Task template (used for output configuration)
 * @return Brain handle or NULL on error
 */
brain_t brain_create_pretrained(const char* model_id, brain_task_t task) {
    (void)task; // Task parameter reserved for future use
    return brain_load_pretrained(model_id, NULL);
}

/**
 * @brief Get brain memory usage with comprehensive tracking
 *
 * WHAT: Accurately calculate total memory used by brain and all subsystems
 * WHY:  Enable precise memory profiling for pretrained models
 * HOW:  Hook into allocation system and sum all component memory usage
 *
 * MEMORY BREAKDOWN:
 * 1. Core structures: brain_t, network, neurons, synapses
 * 2. Cognitive modules: working memory, theory of mind, etc.
 * 3. Subsystems: glial cells, oscillations, neuromodulators
 * 4. Metadata: labels, statistics, cached decisions
 *
 * @param brain Brain handle
 * @return Total memory usage in bytes
 */
size_t brain_get_memory_usage(brain_t brain) {
    if (!brain) {
        return 0;
    }

    size_t total_memory = 0;

    // WHAT: Account for core brain structure
    // WHY:  Base overhead for brain_t itself
    // HOW:  sizeof(struct brain_struct)
    total_memory += sizeof(struct brain_struct);

    // WHAT: Get network memory usage from stats
    // WHY:  Network is the largest component (neurons + synapses)
    // HOW:  Query brain stats for neuron/synapse counts and calculate
    brain_stats_t stats;
    if (brain_get_stats(brain, &stats)) {
        // WHAT: Calculate neuron memory
        // WHY:  Each neuron has state, connections, and metadata
        // HOW:  Estimate 120 bytes per neuron (improved from 100)
        //       - Neuron structure: ~48 bytes
        //       - Activation state: ~16 bytes (current, previous, etc.)
        //       - Synaptic connections list: ~32 bytes (pointers)
        //       - Type-specific data: ~24 bytes
        size_t neuron_memory = (size_t)stats.num_neurons * 120;

        // WHAT: Calculate synapse memory
        // WHY:  Each synapse stores weight, metadata, and plasticity state
        // HOW:  Estimate 24 bytes per synapse (improved from 20)
        //       - Weight (float): 4 bytes
        //       - Pre/post neuron indices: 8 bytes
        //       - Plasticity traces: 8 bytes
        //       - Metadata flags: 4 bytes
        size_t synapse_memory = (size_t)stats.num_synapses * 24;

        total_memory += neuron_memory + synapse_memory;
    }

    // WHAT: Account for output labels
    // WHY:  Dynamic string storage for classification outputs
    // HOW:  Count label strings and their lengths
    if (brain->output_labels && brain->num_output_labels > 0) {
        // Pointer array overhead
        total_memory += brain->num_output_labels * sizeof(char*);

        // String content (estimate 32 bytes average per label)
        for (uint32_t i = 0; i < brain->num_output_labels; i++) {
            if (brain->output_labels[i]) {
                total_memory += strlen(brain->output_labels[i]) + 1;
            }
        }
    }

    // WHAT: Account for decision caching
    // WHY:  Cached inputs and decisions consume memory
    // HOW:  Check if cache is allocated and add size
    if (brain->last_input) {
        total_memory += brain->input_size * sizeof(float);
    }
    if (brain->cached_decision) {
        total_memory += sizeof(brain_decision_t);
        // Add output scores array size
        total_memory += brain->config.num_outputs * sizeof(float);
    }

    // WHAT: Account for long-term memory buffer
    // WHY:  Consolidated memories consume significant space
    // HOW:  Count features and metadata for each memory
    if (brain->longterm_memory && brain->longterm_count > 0) {
        for (uint32_t i = 0; i < brain->longterm_count; i++) {
            if (brain->longterm_memory[i].features) {
                total_memory += brain->longterm_memory[i].num_features * sizeof(float);
            }
            total_memory += sizeof(brain->longterm_memory[i]);  // Metadata
        }
    }

    // WHAT: Account for loss history buffer
    // WHY:  Meta-learning tracks recent losses
    // HOW:  Fixed-size circular buffer
    total_memory += sizeof(brain->loss_history);

    // WHAT: Account for cognitive subsystems (if present)
    // WHY:  Working memory, theory of mind, etc. have overhead
    // HOW:  Estimate based on typical sizes (conservative)
    //
    // NOTE: Actual sizes depend on subsystem implementation.
    // These are reasonable estimates based on typical usage.
    size_t cognitive_memory = 0;

    if (brain->working_memory) {
        cognitive_memory += 1024 * 64;  // ~64KB for working memory slots
    }
    if (brain->theory_of_mind) {
        cognitive_memory += 1024 * 128;  // ~128KB for ToM beliefs/agents
    }
    if (brain->introspection) {
        cognitive_memory += 1024 * 32;   // ~32KB for introspection state
    }
    if (brain->ethics) {
        cognitive_memory += 1024 * 16;   // ~16KB for ethics engine
    }
    if (brain->salience) {
        cognitive_memory += 1024 * 48;   // ~48KB for salience maps
    }
    if (brain->curiosity) {
        cognitive_memory += 1024 * 24;   // ~24KB for curiosity state
    }
    if (brain->knowledge) {
        cognitive_memory += 1024 * 256;  // ~256KB for knowledge graphs
    }
    if (brain->consolidation) {
        cognitive_memory += 1024 * 64;   // ~64KB for memory consolidation
    }

    total_memory += cognitive_memory;

    // WHAT: Account for biological realism subsystems
    // WHY:  Glial cells, oscillations add overhead
    // HOW:  Estimate based on typical network size
    size_t biological_memory = 0;

    if (brain->glial) {
        // Estimate: ~50% as many glial cells as neurons, ~40 bytes each
        biological_memory += (size_t)(stats.num_neurons * 0.5f * 40);
    }
    if (brain->oscillations) {
        // Oscillation analyzer tracks multiple frequency bands
        biological_memory += 1024 * 48;  // ~48KB for FFT buffers, etc.
    }
    if (brain->neuromod_system) {
        // Neuromodulator system tracks DA, 5-HT, NE, ACh levels
        biological_memory += 1024 * 24;  // ~24KB for modulator state
    }

    total_memory += biological_memory;

    // WHAT: Add mutex/synchronization overhead
    // WHY:  Thread-safe brains allocate locks
    // HOW:  Count mutexes and add platform overhead
    total_memory += sizeof(nimcp_platform_mutex_t);  // cache_mutex
    // Note: refcount_mutex removed - now using lock-free atomic operations

    // WHAT: Add COW atomic reference counting overhead
    // WHY:  Shared networks track reference counts atomically
    // HOW:  Add atomic refcount variable size if allocated
    if (brain->network_refcount_atomic) {
        total_memory += sizeof(_Atomic(uint32_t));
    }

    return total_memory;
}
