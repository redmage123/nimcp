//=============================================================================
// nimcp_brain_init_validation.c - BBB Global System Management
//=============================================================================
/**
 * @file nimcp_brain_init_validation.c
 * @brief Blood-Brain Barrier global system management
 *
 * WHAT: Global BBB system access and lifecycle management
 * WHY:  Provides shared BBB instance across all brain instances
 * HOW:  Thread-safe singleton with reference counting
 *
 * EXTRACTED FROM: nimcp_brain_init.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_validation.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdint.h>
#include <stdbool.h>

#define LOG_MODULE "BRAIN_INIT_VALIDATION"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_validation, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Global BBB System (Singleton with Reference Counting)
//=============================================================================

static bbb_system_t g_bbb_system = NULL;
static uint32_t g_bbb_refcount = 0;
static nimcp_platform_mutex_t g_bbb_mutex;
static bool g_bbb_mutex_initialized = false;

/**
 * @brief Get or create the global BBB system (thread-safe)
 * @return BBB system handle, or NULL on failure
 */
bbb_system_t get_global_bbb_system(void)
{
    // Lazy mutex initialization
    if (!g_bbb_mutex_initialized) {
        if (nimcp_platform_mutex_init(&g_bbb_mutex, false) != 0) {
            LOG_ERROR("Failed to initialize BBB mutex");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "get_global_bbb_system: validation failed");
            return NULL;
        }
        g_bbb_mutex_initialized = true;
    }

    nimcp_platform_mutex_lock(&g_bbb_mutex);

    if (!g_bbb_system) {
        // Create with default conservative configuration
        bbb_config_t config = bbb_default_config();
        g_bbb_system = bbb_system_create(&config);
        if (g_bbb_system) {
            LOG_INFO("Global BBB system created (Phase IS-1)");
        } else {
            LOG_WARNING("Failed to create global BBB system");
        }
    }

    if (g_bbb_system) {
        g_bbb_refcount++;
        LOG_DEBUG("BBB system refcount incremented to %u", g_bbb_refcount);
    }

    nimcp_platform_mutex_unlock(&g_bbb_mutex);
    return g_bbb_system;
}

void nimcp_bbb_release_global_system(void)
{
    if (!g_bbb_mutex_initialized) {
        return;
    }

    nimcp_platform_mutex_lock(&g_bbb_mutex);

    if (g_bbb_refcount > 0) {
        g_bbb_refcount--;
        LOG_DEBUG("BBB system refcount decremented to %u", g_bbb_refcount);

        if (g_bbb_refcount == 0 && g_bbb_system) {
            bbb_system_destroy(g_bbb_system);
            g_bbb_system = NULL;
            LOG_INFO("Global BBB system destroyed (no more references)");
        }
    }

    nimcp_platform_mutex_unlock(&g_bbb_mutex);
}

bbb_system_t nimcp_bbb_get_global_system(void)
{
    // Return NULL silently if BBB not initialized - callers check for NULL
    if (!g_bbb_mutex_initialized) {
        return NULL;
    }

    nimcp_platform_mutex_lock(&g_bbb_mutex);
    bbb_system_t result = g_bbb_system;
    nimcp_platform_mutex_unlock(&g_bbb_mutex);

    return result;
}
