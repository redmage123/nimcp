#!/bin/bash

#=============================================================================
# refactor_middleware_module.sh - Automated Middleware Refactoring Script
#=============================================================================
#
# USAGE: ./refactor_middleware_module.sh <source_file> <module_name>
#
# WHAT: Applies systematic refactoring pattern to middleware modules
# WHY:  Automate repetitive refactoring tasks across 46 modules
# HOW:  Uses sed/awk to add includes, replace allocations, add logging
#
# EXAMPLE:
#   ./refactor_middleware_module.sh \
#       src/middleware/encoding/nimcp_population_coding.c \
#       population_coding
#
#=============================================================================

set -e  # Exit on error

# Check arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <source_file> <module_name>"
    echo "Example: $0 src/middleware/encoding/nimcp_population_coding.c population_coding"
    exit 1
fi

SOURCE_FILE="$1"
MODULE_NAME="$2"

# Verify file exists
if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: Source file not found: $SOURCE_FILE"
    exit 1
fi

# Create backup
BACKUP_FILE="${SOURCE_FILE}.bak"
cp "$SOURCE_FILE" "$BACKUP_FILE"
echo "Created backup: $BACKUP_FILE"

#=============================================================================
# Step 1: Add Required Includes
#=============================================================================

echo "Step 1: Adding required includes..."

# Check if includes already exist
if ! grep -q "utils/logging/nimcp_logging.h" "$SOURCE_FILE"; then
    # Find the last #include line
    LAST_INCLUDE_LINE=$(grep -n "^#include" "$SOURCE_FILE" | tail -1 | cut -d: -f1)

    if [ -n "$LAST_INCLUDE_LINE" ]; then
        # Add new includes after last include
        sed -i "${LAST_INCLUDE_LINE}a\\
#include \"utils/logging/nimcp_logging.h\"\\
#include \"utils/config/nimcp_dynamic_config.h\"\\
#include \"security/nimcp_security.h\"\\
#include \"async/nimcp_future.h\"" "$SOURCE_FILE"
        echo "  - Added logging, config, security, async includes"
    else
        echo "  - Warning: Could not find include section"
    fi
else
    echo "  - Includes already present, skipping"
fi

#=============================================================================
# Step 2: Add Module-Level Globals
#=============================================================================

echo "Step 2: Adding module-level globals..."

# Check if MODULE_NAME define exists
if ! grep -q "#define MODULE_NAME" "$SOURCE_FILE"; then
    # Find position after includes and before first struct/function
    # Add after includes section
    INCLUDE_END=$(grep -n "^#include" "$SOURCE_FILE" | tail -1 | cut -d: -f1)

    if [ -n "$INCLUDE_END" ]; then
        INCLUDE_END=$((INCLUDE_END + 1))
        sed -i "${INCLUDE_END}a\\
\\
// Module name for logging and security registration\\
#define MODULE_NAME \"${MODULE_NAME}\"\\
\\
// Security module ID (set during registration)\\
static uint32_t s_security_module_id = 0;\\
static bool s_module_initialized = false;" "$SOURCE_FILE"
        echo "  - Added MODULE_NAME and module globals"
    fi
else
    echo "  - Module globals already present, skipping"
fi

#=============================================================================
# Step 3: Replace Memory Allocations
#=============================================================================

echo "Step 3: Replacing memory allocations..."

# Count replacements
MALLOC_COUNT=$(grep -c '\bmalloc(' "$SOURCE_FILE" || true)
CALLOC_COUNT=$(grep -c '\bcalloc(' "$SOURCE_FILE" || true)
REALLOC_COUNT=$(grep -c '\brealloc(' "$SOURCE_FILE" || true)
FREE_COUNT=$(grep -c '\bfree(' "$SOURCE_FILE" || true)
STRDUP_COUNT=$(grep -c '\bstrdup(' "$SOURCE_FILE" || true)

# Replace memory functions
sed -i 's/\bmalloc(/nimcp_malloc(/g' "$SOURCE_FILE"
sed -i 's/\bcalloc(/nimcp_calloc(/g' "$SOURCE_FILE"
sed -i 's/\brealloc(/nimcp_realloc(/g' "$SOURCE_FILE"
sed -i 's/\bstrdup(/nimcp_strdup(/g' "$SOURCE_FILE"

# Be careful with free() - only replace standalone free calls, not nimcp_free or other_free
sed -i 's/\([^_]\)free(/\1nimcp_free(/g' "$SOURCE_FILE"
sed -i 's/^free(/nimcp_free(/g' "$SOURCE_FILE"

echo "  - Replaced malloc: $MALLOC_COUNT"
echo "  - Replaced calloc: $CALLOC_COUNT"
echo "  - Replaced realloc: $REALLOC_COUNT"
echo "  - Replaced free: $FREE_COUNT"
echo "  - Replaced strdup: $STRDUP_COUNT"

#=============================================================================
# Step 4: Create Init/Shutdown Template
#=============================================================================

echo "Step 4: Creating init/shutdown function template..."

TEMPLATE_FILE="${SOURCE_FILE}.init_template"

cat > "$TEMPLATE_FILE" << 'EOF'

//=============================================================================
// MODULE LIFECYCLE (ADD THIS TO YOUR MODULE)
//=============================================================================

/**
 * @brief Initialize module
 *
 * WHAT: Register module with security system and load configuration
 * WHY:  Enable security monitoring and configurable parameters
 * HOW:  Call security_register_module() and load config defaults
 *
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t MODULE_NAME_init(void) {
    if (s_module_initialized) {
        LOG_MODULE_WARN(MODULE_NAME, "Module already initialized");
        return NIMCP_SUCCESS;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Initializing %s module", MODULE_NAME);

    // Register with security system
    s_security_module_id = security_register_module(MODULE_NAME, SECURITY_LEVEL_MEDIUM);
    if (s_security_module_id == 0) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to register with security system");
        return NIMCP_ERROR_SECURITY_REGISTRATION_FAILED;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Registered with security system (ID: %u)",
                    s_security_module_id);

    // TODO: Load configuration defaults
    // Example:
    // s_default_capacity = config_get_int("MODULE.default_capacity", 1024);
    // s_timeout_ms = config_get_int("MODULE.timeout_ms", 1000);

    s_module_initialized = true;
    LOG_MODULE_INFO(MODULE_NAME, "Module initialization complete");

    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown module
 */
static void MODULE_NAME_shutdown(void) {
    if (!s_module_initialized) {
        return;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Shutting down %s module", MODULE_NAME);

    // TODO: Module cleanup if needed

    s_security_module_id = 0;
    s_module_initialized = false;

    LOG_MODULE_INFO(MODULE_NAME, "Module shutdown complete");
}

EOF

# Replace MODULE_NAME placeholder in template
sed -i "s/MODULE_NAME/${MODULE_NAME}/g" "$TEMPLATE_FILE"

echo "  - Created init/shutdown template: $TEMPLATE_FILE"
echo "  - MANUAL ACTION REQUIRED: Copy functions from template to source file"

#=============================================================================
# Step 5: Generate Logging Examples
#=============================================================================

echo "Step 5: Generating logging examples..."

LOGGING_GUIDE="${SOURCE_FILE}.logging_guide"

cat > "$LOGGING_GUIDE" << EOF
#=============================================================================
# Logging Guide for ${MODULE_NAME}
#=============================================================================

Add logging at these points:

1. FUNCTION ENTRY (Debug level):
   LOG_MODULE_DEBUG(MODULE_NAME, "function_name called with param=%d", param);

2. SUCCESSFUL OPERATIONS (Info level):
   LOG_MODULE_INFO(MODULE_NAME, "Operation completed successfully");

3. WARNINGS (Warn level):
   LOG_MODULE_WARN(MODULE_NAME, "Warning condition: value=%f", value);

4. ERRORS (Error level):
   LOG_MODULE_ERROR(MODULE_NAME, "Operation failed: reason=%s", reason);

5. CREATE FUNCTIONS:
   Add to create functions:
   - Validate inputs: LOG_MODULE_ERROR on invalid params
   - Log creation: LOG_MODULE_INFO on success
   - Log allocation failures: LOG_MODULE_ERROR

6. DESTROY FUNCTIONS:
   Add to destroy functions:
   - LOG_MODULE_DEBUG on entry
   - LOG_MODULE_DEBUG on completion

7. CONFIG LOOKUPS:
   size_t max_val = config_get_int("${MODULE_NAME}.max_value", 1024);
   LOG_MODULE_DEBUG(MODULE_NAME, "Config: max_value=%zu", max_val);

8. AUTO-INIT PATTERN:
   Add to all create/init functions:

   if (!s_module_initialized) {
       if (${MODULE_NAME}_init() != NIMCP_SUCCESS) {
           LOG_MODULE_ERROR(MODULE_NAME, "Module initialization failed");
           return NULL;
       }
   }

EOF

echo "  - Created logging guide: $LOGGING_GUIDE"

#=============================================================================
# Step 6: Generate Config Keys
#=============================================================================

echo "Step 6: Generating config keys..."

CONFIG_KEYS="${SOURCE_FILE}.config_keys"

cat > "$CONFIG_KEYS" << EOF
#=============================================================================
# Configuration Keys for ${MODULE_NAME}
#=============================================================================

Suggested configuration keys (add to your config file):

[${MODULE_NAME}]
# Maximum capacity/size limits
max_capacity = 1048576         # Maximum buffer/structure capacity
max_size = 65536               # Maximum size in bytes

# Timeout settings
timeout_ms = 1000              # Operation timeout in milliseconds
retry_count = 3                # Number of retries on failure

# Thresholds
warn_threshold = 0.90          # Warning threshold (0.0-1.0)
error_threshold = 0.95         # Error threshold (0.0-1.0)

# Feature flags
enable_logging = true          # Enable detailed logging
enable_stats = true            # Enable statistics tracking
warn_on_overflow = true        # Warn on buffer overflow

# Performance tuning
batch_size = 32                # Batch processing size
update_interval_ms = 100       # Update interval in milliseconds

Replace these with actual configurable parameters from your module.

USAGE IN CODE:

size_t max_cap = config_get_int("${MODULE_NAME}.max_capacity", 1048576);
int timeout = config_get_int("${MODULE_NAME}.timeout_ms", 1000);
float threshold = config_get_float("${MODULE_NAME}.warn_threshold", 0.90f);
bool enable_log = config_get_bool("${MODULE_NAME}.enable_logging", true);
const char* mode = config_get_string("${MODULE_NAME}.mode", "default");

EOF

echo "  - Created config keys: $CONFIG_KEYS"

#=============================================================================
# Summary
#=============================================================================

echo ""
echo "======================================================================"
echo "Refactoring complete for: $SOURCE_FILE"
echo "======================================================================"
echo ""
echo "Automated changes:"
echo "  ✓ Added logging, config, security, async includes"
echo "  ✓ Added MODULE_NAME define and module globals"
echo "  ✓ Replaced malloc/calloc/realloc/free/strdup with nimcp_* equivalents"
echo ""
echo "Manual steps required:"
echo "  1. Review changes: diff $BACKUP_FILE $SOURCE_FILE"
echo "  2. Copy init/shutdown functions from: $TEMPLATE_FILE"
echo "  3. Add logging using guide: $LOGGING_GUIDE"
echo "  4. Add config lookups using: $CONFIG_KEYS"
echo "  5. Add async event handling where needed (see MIDDLEWARE_REFACTORING_GUIDE.md)"
echo "  6. Test the module"
echo "  7. Remove backup: rm $BACKUP_FILE"
echo ""
echo "Files created:"
echo "  - Backup: $BACKUP_FILE"
echo "  - Init template: $TEMPLATE_FILE"
echo "  - Logging guide: $LOGGING_GUIDE"
echo "  - Config keys: $CONFIG_KEYS"
echo ""
