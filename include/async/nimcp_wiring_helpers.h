/**
 * @file nimcp_wiring_helpers.h
 * @brief Helper macros for KG-driven module wiring pattern
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Helper macros to simplify module migration to KG-driven wiring
 * WHY:  Enable gradual migration of 380+ modules to dynamic wiring
 * HOW:  Provide macros that encapsulate the callback pattern
 *
 * USAGE PATTERN:
 * ==============
 *
 * BEFORE (hardcoded):
 * -------------------
 * int module_init(brain_t brain) {
 *     ctx->bio_ctx = bio_router_register_module(&info);
 *     bio_router_register_handler(ctx->bio_ctx, BIO_MSG_QUERY, handle_query);
 *     bio_router_register_handler(ctx->bio_ctx, BIO_MSG_EVAL, handle_eval);
 * }
 *
 * AFTER (KG-driven):
 * ------------------
 * // Define handler mapping table
 * DEFINE_HANDLER_MAP_BEGIN(my_module)
 *     HANDLER_MAP_ENTRY(BIO_MSG_QUERY, handle_query)
 *     HANDLER_MAP_ENTRY(BIO_MSG_EVAL, handle_eval)
 * DEFINE_HANDLER_MAP_END()
 *
 * // Define callback function
 * DEFINE_HANDLER_CALLBACK(my_module, my_context_t, ctx)
 *
 * int module_init(brain_t brain) {
 *     ctx->bio_ctx = bio_router_register_module(&info);
 *     REGISTER_WIRING_CALLBACK(orchestrator, BIO_MODULE_MY_MODULE, my_module, ctx);
 * }
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WIRING_HELPERS_H
#define NIMCP_WIRING_HELPERS_H

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "async/nimcp_wiring_diagram.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Handler Map Macros
 * ============================================================================ */

/**
 * @brief Handler map entry structure
 */
typedef struct {
    bio_message_type_t message_type;
    bio_message_handler_t handler;
} wiring_handler_map_entry_t;

/**
 * @brief Begin handler map definition
 *
 * @param name Unique name for this module's handler map
 */
#define DEFINE_HANDLER_MAP_BEGIN(name) \
    static const wiring_handler_map_entry_t g_##name##_handler_map[] = {

/**
 * @brief Add entry to handler map
 *
 * @param msg_type Message type enum (BIO_MSG_*)
 * @param handler_fn Handler function pointer
 * NOTE: Parameter named handler_fn (not handler) to avoid macro expansion conflict
 *       with struct field name .handler
 */
#define HANDLER_MAP_ENTRY(msg_type, handler_fn) \
    { .message_type = (msg_type), .handler = (handler_fn) },

/**
 * @brief End handler map definition
 */
#define DEFINE_HANDLER_MAP_END() \
    };

/**
 * @brief Get handler map size
 */
#define HANDLER_MAP_SIZE(name) \
    (sizeof(g_##name##_handler_map) / sizeof(wiring_handler_map_entry_t))

/* ============================================================================
 * Callback Definition Macros
 * ============================================================================ */

/**
 * @brief Define a standard handler callback function
 *
 * This creates a callback function that:
 * 1. Receives discovered message types from the KG
 * 2. Looks up handlers in the module's handler map
 * 3. Registers found handlers with bio_router
 *
 * @param name Module name (must match DEFINE_HANDLER_MAP_BEGIN)
 * @param ctx_type Type of the context struct
 * @param ctx_name Name for the context variable
 */
#define DEFINE_HANDLER_CALLBACK(name, ctx_type, ctx_name) \
    static int name##_handler_callback( \
        bio_module_context_t bio_ctx, \
        const bio_message_type_t* message_types, \
        uint32_t message_count, \
        void* user_data \
    ) { \
        ctx_type* ctx_name = (ctx_type*)user_data; \
        (void)ctx_name; /* May be used in handler lookup */ \
        \
        int registered = 0; \
        for (uint32_t i = 0; i < message_count; i++) { \
            bio_message_type_t msg_type = message_types[i]; \
            \
            /* Look up in handler map */ \
            for (size_t j = 0; j < HANDLER_MAP_SIZE(name); j++) { \
                if (g_##name##_handler_map[j].message_type == msg_type) { \
                    bio_router_register_handler( \
                        bio_ctx, \
                        msg_type, \
                        g_##name##_handler_map[j].handler \
                    ); \
                    registered++; \
                    break; \
                } \
            } \
        } \
        return (registered > 0) ? 0 : -1; \
    }

/**
 * @brief Register handler callback with orchestrator
 *
 * @param orch Orchestrator pointer
 * @param module_id Module ID enum
 * @param name Module name (must match DEFINE_HANDLER_CALLBACK)
 * @param user_data User data (typically module context)
 */
#define REGISTER_WIRING_CALLBACK(orch, module_id, name, user_data) \
    bio_orchestrator_register_handler_callback( \
        (orch), \
        (module_id), \
        name##_handler_callback, \
        (void*)(user_data) \
    )

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * @brief Quick handler registration (still hardcoded, but cleaner syntax)
 *
 * Use this during migration - can be found/replaced later
 */
#define REGISTER_HANDLER(ctx, msg_type, handler) \
    bio_router_register_handler((ctx), (msg_type), (handler))

/**
 * @brief Macro to mark legacy hardcoded registrations
 *
 * Use this to tag code for future migration:
 *   LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(ctx, BIO_MSG_X, handler_x));
 */
#define LEGACY_HANDLER_REGISTRATION(code) code

/**
 * @brief Check if wiring has been discovered for a module
 */
#define IS_WIRING_DISCOVERED(entry) ((entry)->wiring_discovered)

/**
 * @brief Get discovered handler count
 */
#define GET_DISCOVERED_HANDLER_COUNT(entry) ((entry)->wiring.handles_message_count)

/* ============================================================================
 * Migration Helper Functions
 * ============================================================================ */

/**
 * @brief Register handlers from a static map (hybrid approach)
 *
 * WHAT: Register handlers using a map, with optional KG-filtering
 * WHY:  Enable gradual migration - modules can use map without full KG integration
 * HOW:  If discovered_types is NULL, register all; otherwise filter by KG
 *
 * @param bio_ctx Bio-async module context
 * @param map Handler map array
 * @param map_size Number of entries in map
 * @param discovered_types Array of discovered message types (NULL for all)
 * @param discovered_count Number of discovered types (0 for all)
 * @return Number of handlers registered
 */
static inline int wiring_register_handlers_from_map(
    bio_module_context_t bio_ctx,
    const wiring_handler_map_entry_t* map,
    size_t map_size,
    const bio_message_type_t* discovered_types,
    uint32_t discovered_count
) {
    if (!bio_ctx || !map) return 0;

    int registered = 0;

    for (size_t i = 0; i < map_size; i++) {
        bool should_register = false;

        if (discovered_types == NULL || discovered_count == 0) {
            /* No KG filtering - register all */
            should_register = true;
        } else {
            /* Filter by discovered types */
            for (uint32_t j = 0; j < discovered_count; j++) {
                if (discovered_types[j] == map[i].message_type) {
                    should_register = true;
                    break;
                }
            }
        }

        if (should_register && map[i].handler) {
            bio_router_register_handler(bio_ctx, map[i].message_type, map[i].handler);
            registered++;
        }
    }

    return registered;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WIRING_HELPERS_H */
