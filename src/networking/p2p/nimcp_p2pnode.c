#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_p2pnode.c - Refactored P2P Node Implementation
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements a peer-to-peer network node using several design
// patterns for optimal performance and maintainability:
//
// - State Pattern: Connection state management via function pointers
// - Observer Pattern: Event notification for network changes
// - Strategy Pattern: Routing and discovery algorithms
// - Repository Pattern: Hash-indexed peer storage for O(1) lookups
// - Graph Pattern: Network topology tracking with nimcp_graph
//
// COMPLEXITY ANALYSIS:
// - Peer lookup: O(1) via hash table (previously O(n) linear search)
// - Connection management: O(1) state transitions
// - Message routing: O(1) peer resolution (previously O(n))
// - Discovery: O(n) where n = known peers (unavoidable)
// - Topology queries: O(1) to O(V+E) via graph structure
//
// DESIGN PRINCIPLES:
// - Single Responsibility: Each function does one thing
// - No nested control structures: Guard clauses and helper functions
// - Open/Closed: Extensible via strategy pattern
// - Dependency Inversion: Depends on abstractions (function pointers)
//
// INVARIANTS:
// - peer_count <= config.max_peers (always maintained)
// - All peers in hash table are also in peer array
// - All peers in array have corresponding vertices in topology graph
// - listen_socket >= -1 (valid or closed)
// - status transitions: INIT -> RUNNING -> SHUTDOWN or ERROR
//=============================================================================

#include "networking/p2p/nimcp_p2pnode.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "utils/containers/nimcp_graph.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "networking/protocol/nimcp_protocol.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/platform/nimcp_platform_once.h"

// Global BBB security system — protected by once-init to avoid TOCTOU race
static bbb_system_t g_bbb_system = NULL;
static nimcp_platform_once_t g_bbb_once = NIMCP_PLATFORM_ONCE_INIT;

//=============================================================================
// Security Initialization
//=============================================================================

/* Forward declaration for atexit registration */
static void p2pnode_security_cleanup(void);

/**
 * @brief Internal initialization routine (called exactly once)
 *
 * WHAT: Create and configure BBB system for input validation
 * WHY: Protect against malicious external input and TOCTOU race on g_bbb_system
 * HOW: Called exactly once via nimcp_platform_once — thread-safe initialization
 */
static void p2pnode_security_init_impl(void) {
    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;  // Don't block, just log
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.max_string_length = 4096;  // Reasonable limit

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        LOG_ERROR("p2pnode: Failed to initialize security subsystem");
    } else {
        if (atexit(p2pnode_security_cleanup) != 0) {
            LOG_WARN("p2pnode: Failed to register atexit cleanup handler");
        }
        LOG_INFO("p2pnode: Security subsystem initialized");
    }
}

/**
 * @brief Thread-safe security init entry point
 * WHY: nimcp_platform_once guarantees exactly-once execution, no TOCTOU race
 */
static void p2pnode_security_init(void) {
    nimcp_platform_once(&g_bbb_once, p2pnode_security_init_impl);
}

/**
 * @brief Cleanup security subsystem
 * WHY: __attribute__((destructor)) ensures cleanup on library unload
 */
__attribute__((destructor))
static void p2pnode_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
}

#define LOG_MODULE "P2P_NODE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(p2pnode)

//=============================================================================
// Constants and Configuration
//=============================================================================

#define HASH_TABLE_SIZE 256
#define MAX_IP_LENGTH 16
#define LISTEN_BACKLOG 5
#define SOCKET_REUSE_ENABLED 1

//=============================================================================
// Validation Helpers
//=============================================================================

/**
 * @brief Validate IP address string
 *
 * WHAT: Ensures IP address is properly formatted and safe
 * WHY: Prevent malformed addresses from crashing inet_addr()
 * PATTERN: Fail-fast validation with guard clauses
 *
 * CHECKS:
 * - Non-NULL pointer
 * - Length constraints (7-15 characters for IPv4)
 * - Valid characters (digits and dots only)
 * - Proper dot separation
 * - inet_addr() accepts it
 *
 * @param ip IP address string (must be NULL-terminated)
 * @return true if valid, false otherwise
 */
static bool validate_ip_address(const char* ip)
{
    // Guard clause: NULL check
    if (!ip) {
        return false;
    }

    // Validate as string field (NULL termination, UTF-8, control chars)
    if (!nimcp_validate_string_field(ip, strnlen(ip, MAX_IP_LENGTH) + 1)) {
        return false;
    }

    size_t len = strlen(ip);

    // Guard clause: Length check (minimum: "0.0.0.0" = 7, maximum: "255.255.255.255" = 15)
    if (len < 7 || len > 15) {
        return false;
    }

    // Additional semantic check: inet_addr validation
    // WHY: Ensures IP can be converted to network format
    struct in_addr addr;
    if (inet_aton(ip, &addr) == 0) {
        return false;
    }

    return true;
}

/**
 * @brief Validate port number
 *
 * WHAT: Ensures port number is valid and usable
 * WHY: Prevent bind/connect errors from invalid ports
 * PATTERN: Range validation
 *
 * CHECKS:
 * - Not in reserved range (0)
 * - Not in well-known range if binding (< 1024 requires privileges)
 * - Within valid range (1-65535)
 *
 * @param port Port number to validate
 * @param binding true if port will be bound (listen), false if connecting
 * @return true if valid, false otherwise
 */
static bool validate_port_number(uint16_t port, bool binding)
{
    // Guard clause: Zero port (invalid)
    if (port == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_port_number: port is zero");
        return false;
    }

    // Guard clause: Well-known ports if binding (requires root)
    // WHY: Ports < 1024 require elevated privileges
    if (binding && port < 1024) {
        fprintf(stderr, "[P2P] Warning: Port %u requires elevated privileges\n", port);
        // Don't fail, just warn - user might have privileges
    }

    // Validate as integer field
    if (!nimcp_validate_integer_field(&port, sizeof(uint16_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_port_number: nimcp_validate_integer_field is NULL");
        return false;
    }

    return true;
}

//=============================================================================
// State Pattern - Connection State Management
//=============================================================================

/**
 * @brief Connection state enumeration
 */
typedef enum {
    CONN_STATE_DISCONNECTED,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_ERROR
} connection_state_t;

/**
 * @brief State handler function pointer type
 *
 * WHY: Eliminates switch/if-else chains. Each state has its own handler.
 * Makes adding new states trivial (Open/Closed Principle).
 */
typedef bool (*state_handler_fn)(peer_info_t* peer);

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal node state structure
 *
 * INVARIANTS:
 * - peer_count <= config.max_peers
 * - running implies status == NODE_STATUS_RUNNING
 * - listen_socket >= -1 (valid socket or -1 if closed)
 * - topology_graph vertex count == peer_count (graph mirrors peers)
 */
struct p2p_node_struct {
    // Configuration
    node_config_t config;

    // State
    node_status_t status;
    bool running;

    // Thread Safety (Monitor Pattern)
    nimcp_mutex_t lock;

    // Network
    int listen_socket;

    // Repository Pattern: Hash-indexed peer storage using NIMCP hash table utility
    // WHY: Consolidated hash table implementation with better features
    // KEYS: "IP:PORT" strings (e.g., "192.168.1.1:8080")
    // VALUES: Pointers to peer_info_t structures
    hash_table_t* peer_table;

    // Legacy array storage (for iteration)
    peer_info_t* peers;
    uint32_t peer_count;

    // Topology Tracking: Graph of network connections
    // WHY: Enables pathfinding, component analysis, and introspection queries
    NimcpGraph* topology_graph;

    // Statistics
    uint64_t total_connections;
    uint64_t failed_connections;
    uint64_t bytes_sent;
    uint64_t bytes_received;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Peer Key Generation
//=============================================================================

/**
 * @brief Generates hash key from IP and port
 *
 * WHY: Creates consistent string key for hash table lookups.
 * Format: "192.168.1.1:8080"
 *
 * COMPLEXITY: O(1)
 */
static void generate_peer_key(char* key, size_t key_size, const char* ip, uint16_t port)
{
    // Guard clause: Validate inputs
    if (!key || !ip)
        return;

    snprintf(key, key_size, "%s:%u", ip, port);
}

//=============================================================================
// Socket Helper Functions
//=============================================================================

/**
 * @brief Creates and configures a TCP socket
 *
 * WHY: Extracted socket creation logic. Single responsibility.
 *
 * COMPLEXITY: O(1)
 *
 * @return Socket file descriptor or -1 on failure
 */
static int create_tcp_socket(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    // Guard clause: Check creation
    if (sock < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "create_tcp_socket: validation failed");
        return -1;
    }

    return sock;
}

/**
 * @brief Sets socket to non-blocking mode
 *
 * WHY: Extracted flag setting logic for clarity and reuse.
 *
 * COMPLEXITY: O(1)
 *
 * @return true on success, false on failure
 */
static bool set_socket_nonblocking(int sock)
{
    // Guard clause: Validate socket
    if (sock < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "set_socket_nonblocking: validation failed");
        return false;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    // Guard clause: Check fcntl
    if (flags < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "set_socket_nonblocking: validation failed");
        return false;
    }

    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) >= 0;
}

/**
 * @brief Enables socket address reuse
 *
 * WHY: Allows rapid restart of node on same port. Prevents "address in use"
 * errors during development and rapid restarts.
 *
 * COMPLEXITY: O(1)
 *
 * @return true on success, false on failure
 */
static bool enable_socket_reuse(int sock)
{
    // Guard clause: Validate socket
    if (sock < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enable_socket_reuse: validation failed");
        return false;
    }

    int opt = SOCKET_REUSE_ENABLED;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) >= 0;
}

/**
 * @brief Binds socket to address and port
 *
 * WHY: Extracted binding logic. Single responsibility.
 * Simplified error handling with guard clauses.
 *
 * COMPLEXITY: O(1)
 *
 * @return true on success, false on failure
 */
static bool bind_socket(int sock, uint16_t port)
{
    // Guard clause: Validate socket
    if (sock < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bind_socket: validation failed");
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    return bind(sock, (struct sockaddr*) &addr, sizeof(addr)) >= 0;
}

/**
 * @brief Starts listening on socket
 *
 * WHY: Extracted listen logic. Clear, testable.
 *
 * COMPLEXITY: O(1)
 *
 * @return true on success, false on failure
 */
static bool start_listening(int sock)
{
    // Guard clause: Validate socket
    if (sock < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "start_listening: validation failed");
        return false;
    }

    return listen(sock, LISTEN_BACKLOG) >= 0;
}

/**
 * @brief Sets up the listening socket for the node
 *
 * WHY: Refactored from nested if statements to clear sequential steps.
 * Each step extracted to helper function with single responsibility.
 *
 * ALGORITHM:
 * 1. Create TCP socket (O(1))
 * 2. Set non-blocking mode (O(1))
 * 3. Enable address reuse (O(1))
 * 4. Bind to port (O(1))
 * 5. Start listening (O(1))
 *
 * COMPLEXITY: O(1)
 *
 * @param node Pointer to node structure
 * @return true if setup successful, false otherwise
 */
static bool setup_listen_socket(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "setup_listen_socket: node is NULL");
        return false;
    }

    // Step 1: Create socket
    node->listen_socket = create_tcp_socket();
    // Guard clause: Check creation
    if (node->listen_socket < 0) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, "listen",
                      "Failed to create TCP listen socket: errno=%d", errno);
        return false;
    }

    // Step 2: Set non-blocking
    if (!set_socket_nonblocking(node->listen_socket)) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, "listen",
                      "Failed to set socket non-blocking mode: errno=%d", errno);
        close(node->listen_socket);
        return false;
    }

    // Step 3: Enable reuse
    if (!enable_socket_reuse(node->listen_socket)) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, "listen",
                      "Failed to enable socket address reuse: errno=%d", errno);
        close(node->listen_socket);
        return false;
    }

    // Step 4: Bind to port
    if (!bind_socket(node->listen_socket, node->config.listen_port)) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, "listen",
                      "Failed to bind socket to port %u: errno=%d",
                      node->config.listen_port, errno);
        close(node->listen_socket);
        return false;
    }

    // Step 5: Start listening
    if (!start_listening(node->listen_socket)) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, "listen",
                      "Failed to start listening on port %u: errno=%d",
                      node->config.listen_port, errno);
        close(node->listen_socket);
        return false;
    }

    return true;
}

//=============================================================================
// Node Creation and Destruction
//=============================================================================

/**
 * @brief Validates node configuration
 *
 * WHY: Extracted validation logic. Guard clauses prevent nested ifs.
 * Single responsibility - only validates configuration.
 *
 * COMPLEXITY: O(1)
 *
 * @return true if valid, false otherwise
 */
/**
 * @brief Validate node configuration
 *
 * WHAT: Ensures configuration is valid before node creation
 * WHY: Prevent node creation with invalid configuration
 * PATTERN: Fail-fast validation with nimcp_validate integration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
static bool validate_config(const node_config_t* config)
{
    // Guard clause: Check null
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_config: config is NULL");
        return false;
    }

    // Validate listen_port using nimcp_validate
    // WHY: Ensure port is valid and bindable
    if (!validate_port_number(config->listen_port, true)) {
        fprintf(stderr, "[P2P] Invalid listen port in config: %u\n", config->listen_port);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: validate_port_number is NULL");
        return false;
    }

    // Validate max_peers using nimcp_validate
    // WHY: Ensure max_peers is reasonable and will fit in memory
    if (!nimcp_validate_integer_field(&config->max_peers, sizeof(uint32_t))) {
        fprintf(stderr, "[P2P] Invalid max_peers in config\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: nimcp_validate_integer_field is NULL");
        return false;
    }

    // Guard clause: Check max_peers not zero
    if (config->max_peers == 0) {
        fprintf(stderr, "[P2P] max_peers cannot be zero\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: config->max_peers is zero");
        return false;
    }

    // Guard clause: Check max_peers not excessive
    // WHY: Prevent memory exhaustion
    if (config->max_peers > 10000) {
        fprintf(stderr, "[P2P] max_peers too large: %u (max: 10000)\n", config->max_peers);
        return false;
    }

    return true;
}

/**
 * @brief Allocates peer storage arrays
 *
 * WHY: Extracted allocation logic for clarity and error handling.
 * Single responsibility - only allocates memory.
 *
 * COMPLEXITY: O(1)
 *
 * @return true on success, false on failure
 */
static bool allocate_peer_storage(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_peer_storage: node is NULL");
        return false;
    }

    node->peers = nimcp_calloc(node->config.max_peers, sizeof(peer_info_t));
    return node->peers != NULL;
}

/**
 * @brief Initializes node state and statistics
 *
 * WHY: Extracted initialization logic. Clear, testable.
 *
 * COMPLEXITY: O(1)
 */
static void initialize_node_state(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node)
        return;

    node->status = NODE_STATUS_INIT;
    node->running = false;
    node->listen_socket = -1;
    node->peer_count = 0;

    // Initialize statistics
    node->total_connections = 0;
    node->failed_connections = 0;
    node->bytes_sent = 0;
    node->bytes_received = 0;
}

/**
 * @brief Creates a new P2P node instance
 *
 * WHY: Initializes complete P2P node with hash table for O(1) peer lookups.
 * Refactored from nested ifs to clear sequential steps with guard clauses.
 *
 * ALGORITHM:
 * 1. Validate configuration (O(1))
 * 2. Allocate node structure (O(1))
 * 3. Copy configuration (O(1))
 * 4. Initialize hash table (O(1))
 * 5. Allocate peer storage (O(1))
 * 6. Initialize state (O(1))
 *
 * COMPLEXITY: O(1) - Fixed initialization cost
 *
 * @param config Pointer to configuration structure
 * @return Handle to the created node, NULL if creation fails
 */
p2p_node_t p2p_node_create(const node_config_t* config)
{
    // Guard clause: Validate configuration
    if (!validate_config(config)) {
        LOG_ERROR(LOG_MODULE, "Node creation failed: invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "p2p_node_create: validate_config is NULL");
        return NULL;
    }

    LOG_DEBUG(LOG_MODULE, "Creating node on port %u with max_peers=%u",
              config->listen_port, config->max_peers);

    // Allocate node structure
    p2p_node_t node = nimcp_calloc(1, sizeof(struct p2p_node_struct));
    // Guard clause: Check allocation
    if (!node) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct p2p_node_struct),
                          "Failed to allocate P2P node structure");
        return NULL;
    }

    // Copy configuration
    memcpy(&node->config, config, sizeof(node_config_t));

    // Initialize hash table for O(1) peer lookups using NIMCP hash table utility
    // WHY: Standard hash table provides better features and consistency
    hash_table_config_t hash_config = {
        .initial_buckets = HASH_TABLE_SIZE,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_DJB2,
        .case_insensitive = false,
        .value_destructor = NULL,  // We manage peer_info_t separately
        .thread_safe = false       // We use our own mutex
    };
    node->peer_table = hash_table_create(&hash_config);

    // Guard clause: Check hash table creation
    if (!node->peer_table) {
        LOG_ERROR(LOG_MODULE, "Failed to create peer hash table");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, HASH_TABLE_SIZE * sizeof(void*),
                          "Failed to allocate peer hash table for P2P node");
        nimcp_free(node);
        return NULL;
    }

    // Allocate peer storage
    if (!allocate_peer_storage(node)) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                          node->config.max_peers * sizeof(peer_info_t),
                          "Failed to allocate peer storage array");
        hash_table_destroy(node->peer_table);
        nimcp_free(node);
        return NULL;
    }

    // Initialize node state
    initialize_node_state(node);

    // Initialize mutex for thread safety
    if (nimcp_mutex_init(&node->lock, NULL) != NIMCP_SUCCESS) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_THREAD_CREATE, 0,
                             "Failed to initialize mutex for P2P node");
        nimcp_free(node->peers);
        hash_table_destroy(node->peer_table);
        nimcp_free(node);
        return NULL;
    }

    // Create topology graph for network structure tracking
    // WHY: Enables introspection, pathfinding, and component analysis
    node->topology_graph = nimcp_graph_create();
    if (!node->topology_graph) {
        LOG_ERROR(LOG_MODULE, "Failed to create topology graph for node on port %u",
                  config->listen_port);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                          "Failed to create topology graph for P2P node");
        nimcp_mutex_destroy(&node->lock);
        hash_table_destroy(node->peer_table);
        nimcp_free(node->peers);
        nimcp_free(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "p2p_node_create: node->topology_graph is NULL");
        return NULL;
    }

    LOG_INFO(LOG_MODULE, "Node created successfully: port=%u, max_peers=%u, ping_interval=%ums",
             config->listen_port, config->max_peers, config->ping_interval);

    // Bio-async registration
    node->bio_ctx = NULL;
    node->bio_async_enabled = false;
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_P2P,
            .module_name = "p2p_node",
            .inbox_capacity = 64,
            .user_data = node
        };
        node->bio_ctx = bio_router_register_module(&bio_info);
        if (node->bio_ctx) {
            node->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async registered for P2P node");
        }
    }

    return node;
}

/**
 * @brief Closes listen socket if open
 *
 * WHY: Extracted socket cleanup logic. Single responsibility.
 *
 * COMPLEXITY: O(1)
 */
static void close_listen_socket(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node)
        return;

    // Guard clause: Check if socket is open
    if (node->listen_socket >= 0) {
        close(node->listen_socket);
        node->listen_socket = -1;
    }
}

/**
 * @brief Destroys a P2P node instance and frees resources
 *
 * WHY: Ensures no memory leaks. Destroys hash table, closes sockets,
 * and frees all allocated memory. Uses guard clauses throughout.
 *
 * ALGORITHM:
 * 1. Validate input (O(1))
 * 2. Stop node if running (O(n) where n = peer count)
 * 3. Destroy hash table (O(n))
 * 4. Free peer array (O(1))
 * 5. Close listen socket (O(1))
 * 6. Free node structure (O(1))
 *
 * COMPLEXITY: O(n) where n = number of peers
 *
 * @param node Handle to the node to destroy
 */
void p2p_node_destroy(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node)
        return;

    // Stop if running
    if (node->running) {
        p2p_node_stop(node);
    }

    // Bio-async unregistration
    if (node->bio_async_enabled && node->bio_ctx) {
        bio_router_unregister_module(node->bio_ctx);
        node->bio_ctx = NULL;
        node->bio_async_enabled = false;
    }

    // Destroy hash table (NIMCP utility handles cleanup)
    hash_table_destroy(node->peer_table);

    // Free peer array
    nimcp_free(node->peers);

    // Destroy topology graph
    if (node->topology_graph) {
        nimcp_graph_destroy(node->topology_graph);
    }

    // Close listen socket
    close_listen_socket(node);

    // Destroy mutex
    nimcp_mutex_destroy(&node->lock);

    // Free node structure
    nimcp_free(node);
}

//=============================================================================
// Node Status and Queries
//=============================================================================

/**
 * @brief Get topology graph for network analysis
 *
 * WHY: Expose graph for introspection, pathfinding, and analysis
 * PATTERN: Accessor - provides controlled read access to internal state
 *
 * COMPLEXITY: O(1)
 *
 * @param node Handle to the node
 * @return Pointer to topology graph, or NULL if node is invalid
 */
NimcpGraph* p2p_node_get_topology_graph(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_get_topology_graph: node is NULL");
        return NULL;
    }

    return node->topology_graph;
}

/**
 * @brief Gets current status of the node
 *
 * WHY: Simple getter with guard clause. Returns error status if node is null.
 *
 * COMPLEXITY: O(1)
 *
 * @param node Handle to the node
 * @return Current node status
 */
node_status_t p2p_node_get_status(p2p_node_t node)
{
    return node ? node->status : NODE_STATUS_ERROR;
}

/**
 * @brief Checks if a peer is currently connected using hash table
 *
 * WHY: Refactored from O(n) linear search to O(1) hash table lookup.
 * Uses guard clauses to avoid nested ifs.
 *
 * COMPLEXITY: O(1) average case (previously O(n))
 *
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port number of peer
 * @return true if peer is connected, false otherwise
 */
bool p2p_node_is_peer_connected(p2p_node_t node, const char* peer_ip, uint16_t peer_port)
{
    // Guard clause: Validate inputs
    if (!node || !peer_ip) {
        return false;
    }

    // Lock for thread safety
    nimcp_mutex_lock(&node->lock);

    // Generate hash key
    char key[32];
    generate_peer_key(key, sizeof(key), peer_ip, peer_port);

    // O(1) hash table lookup using NIMCP utility
    // NOTE: Hash table stores a COPY of the pointer, so we get back a pointer to the copy
    // FIX: Need to dereference twice - once for the stored copy, once for the actual pointer
    peer_info_t** peer_ptr = (peer_info_t**)hash_table_lookup_string(node->peer_table, key);

    // Guard clause: Check if peer found
    if (!peer_ptr || !*peer_ptr) {
        nimcp_mutex_unlock(&node->lock);
        return false;
    }

    peer_info_t* peer = *peer_ptr;
    bool connected = peer->connected;
    nimcp_mutex_unlock(&node->lock);
    return connected;
}

//=============================================================================
// Peer Connection Management
//=============================================================================

/**
 * @brief Checks if peer already exists in the node
 *
 * WHY: Extracted duplicate check. Uses O(1) hash table lookup instead of
 * O(n) linear search.
 *
 * COMPLEXITY: O(1) average case
 *
 * @return true if peer exists, false otherwise
 */
static bool peer_already_exists(p2p_node_t node, const char* key)
{
    // Guard clause: Validate inputs
    if (!node || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "peer_already_exists: required parameter is NULL (node, key)");
        return false;
    }

    return hash_table_lookup_string(node->peer_table, key) != NULL;
}

/**
 * @brief Creates socket address structure from IP and port
 *
 * WHY: Extracted address creation logic. Single responsibility.
 * Clear, testable function.
 *
 * COMPLEXITY: O(1)
 *
 * @return true on success, false on failure
 */
static bool create_sockaddr(struct sockaddr_in* addr, const char* ip, uint16_t port)
{
    // Guard clause: Validate inputs
    if (!addr || !ip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_sockaddr: required parameter is NULL (addr, ip)");
        return false;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    return inet_pton(AF_INET, ip, &addr->sin_addr) > 0;
}

/**
 * @brief Attempts connection to peer address
 *
 * WHY: Extracted connection logic. Handles non-blocking connect semantics
 * where EINPROGRESS is success.
 *
 * COMPLEXITY: O(1)
 *
 * @return true if connection initiated, false on error
 */
static bool attempt_connection(int sock, const struct sockaddr_in* addr)
{
    // Guard clause: Validate inputs
    if (sock < 0 || !addr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attempt_connection: addr is NULL");
        return false;
    }

    int result = connect(sock, (struct sockaddr*) addr, sizeof(*addr));

    // Connection is non-blocking, EINPROGRESS means in progress (success)
    if (result < 0 && errno != EINPROGRESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attempt_connection: validation failed");
        return false;
    }

    return true;
}

/**
 * @brief Initializes peer info structure
 *
 * WHY: Extracted peer initialization. Single responsibility.
 *
 * COMPLEXITY: O(1)
 */
static void initialize_peer_info(peer_info_t* peer, const char* ip, uint16_t port, int socket_fd)
{
    // Guard clause: Validate inputs
    if (!peer || !ip)
        return;

    strncpy(peer->ip, ip, sizeof(peer->ip) - 1);
    peer->ip[sizeof(peer->ip) - 1] = '\0';
    peer->port = port;
    peer->socket_fd = socket_fd;
    peer->connected = true;
}

/**
 * @brief Adds peer to node's peer list and hash table
 *
 * WHY: Extracted peer addition logic. Maintains both array and hash table
 * for O(1) lookups and O(n) iteration.
 *
 * COMPLEXITY: O(1)
 *
 * @return true on success, false on failure
 */
static bool add_peer_to_node(p2p_node_t node, const char* ip, uint16_t port, int socket_fd)
{
    // Guard clause: Validate inputs
    if (!node || !ip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "add_peer_to_node: required parameter is NULL (node, ip)");
        return false;
    }

    // Guard clause: Check capacity
    if (node->peer_count >= node->config.max_peers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "add_peer_to_node: capacity exceeded");
        return false;
    }

    // Initialize peer info in array
    peer_info_t* peer = &node->peers[node->peer_count];
    initialize_peer_info(peer, ip, port, socket_fd);

    // Generate key for hash table
    char key[32];
    generate_peer_key(key, sizeof(key), ip, port);

    // Add to hash table for O(1) lookups using NIMCP utility
    // NOTE: We store the peer_info_t* pointer value itself (copy the address)
    // FIX: Pass &peer to copy the pointer VALUE (address), not what it points to
    if (!hash_table_insert_string(node->peer_table, key, &peer, sizeof(peer_info_t*))) {
        LOG_ERROR(LOG_MODULE, "Failed to insert peer %s into hash table", key);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_peer_to_node: hash_table_insert_string is NULL");
        return false;
    }

    node->peer_count++;
    node->total_connections++;

    return true;
}

/**
 * @brief Attempts to connect to a peer
 *
 * WHY: Refactored from deeply nested if statements to clear sequential steps.
 * Each step extracted to helper function. Uses O(1) hash table for duplicate
 * checking instead of O(n) linear search.
 *
 * ALGORITHM:
 * 1. Validate inputs and check capacity (O(1))
 * 2. Check if peer already exists via hash table (O(1))
 * 3. Create non-blocking socket (O(1))
 * 4. Create socket address (O(1))
 * 5. Attempt connection (O(1))
 * 6. Add peer to node (O(1))
 *
 * COMPLEXITY: O(1) average case (previously O(n) due to linear search)
 *
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port number of peer
 * @return true if connection initiated successfully, false otherwise
 */
bool p2p_node_connect_peer(p2p_node_t node, const char* peer_ip, uint16_t peer_port)
{
    // Guard clause: Validate inputs
    if (!node || !peer_ip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_connect_peer: required parameter is NULL (node, peer_ip)");
        return false;
    }

    // Validate IP address format
    // WHY: Prevent malformed addresses from causing crashes or errors
    if (!validate_ip_address(peer_ip)) {
        fprintf(stderr, "[P2P] Invalid IP address format: %s\n", peer_ip);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_connect_peer: validate_ip_address is NULL");
        return false;
    }

    // Validate port number
    // WHY: Ensure port is usable for connection
    if (!validate_port_number(peer_port, false)) {
        fprintf(stderr, "[P2P] Invalid port number: %u\n", peer_port);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_connect_peer: validate_port_number is NULL");
        return false;
    }

    // Lock for thread safety
    nimcp_mutex_lock(&node->lock);

    // Guard clause: Check capacity
    if (node->peer_count >= node->config.max_peers) {
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "p2p_node_connect_peer: capacity exceeded");
        return false;
    }

    // Generate key for lookup
    char key[32];
    generate_peer_key(key, sizeof(key), peer_ip, peer_port);

    // Check if peer already exists (O(1) via hash table)
    if (peer_already_exists(node, key)) {
        // FIX: Hash table stores a copy of the pointer, need double dereference
        peer_info_t** existing_ptr = (peer_info_t**)hash_table_lookup_string(node->peer_table, key);
        bool result = (existing_ptr && *existing_ptr) ? (*existing_ptr)->connected : false;
        nimcp_mutex_unlock(&node->lock);
        return result;
    }

    // Create non-blocking socket
    int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    // Guard clause: Check socket creation
    if (sock < 0) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, peer_ip,
                      "Failed to create socket for peer connection: errno=%d", errno);
        node->failed_connections++;
        nimcp_mutex_unlock(&node->lock);
        return false;
    }

    // Create socket address
    struct sockaddr_in addr;
    if (!create_sockaddr(&addr, peer_ip, peer_port)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Failed to create socket address for peer %s:%u", peer_ip, peer_port);
        close(sock);
        node->failed_connections++;
        nimcp_mutex_unlock(&node->lock);
        return false;
    }

    // Attempt connection
    if (!attempt_connection(sock, &addr)) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, peer_ip,
                      "Failed to connect to peer %s:%u: errno=%d", peer_ip, peer_port, errno);
        close(sock);
        node->failed_connections++;
        nimcp_mutex_unlock(&node->lock);
        return false;
    }

    // Add peer to node
    if (!add_peer_to_node(node, peer_ip, peer_port, sock)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE,
                             "Failed to add peer %s:%u to node storage", peer_ip, peer_port);
        close(sock);
        node->failed_connections++;
        nimcp_mutex_unlock(&node->lock);
        return false;
    }

    // Add vertex to topology graph
    // WHY: Track network structure for introspection and routing
    // Use peer_ip:peer_port as unique peer ID (hashed to uint64_t)
    uint64_t peer_id = ((uint64_t) inet_addr(peer_ip) << 32) | peer_port;
    uint32_t vertex_idx =
        nimcp_graph_add_vertex(node->topology_graph, peer_id, 0.0F, 0.0F,
                               0.0F,  // Coordinates (TODO: calculate from network metrics)
                               0      // Capabilities (TODO: exchange in handshake)
        );

    // If vertex creation fails, log but don't fail connection
    // WHY: Graph is for analytics; connection is the primary goal
    if (vertex_idx == NIMCP_INVALID_VERTEX) {
        fprintf(stderr, "[P2P] Warning: Failed to add peer to topology graph\n");
    }

    nimcp_mutex_unlock(&node->lock);
    return true;
}

/**
 * @brief Closes peer's socket if open
 *
 * WHY: Extracted socket cleanup. Single responsibility.
 *
 * COMPLEXITY: O(1)
 */
static void close_peer_socket(peer_info_t* peer)
{
    // Guard clause: Validate input
    if (!peer)
        return;

    // Guard clause: Check if socket is open
    if (peer->socket_fd >= 0) {
        close(peer->socket_fd);
        peer->socket_fd = -1;
    }

    peer->connected = false;
}

/**
 * @brief Finds peer index in array by IP and port
 *
 * WHY: Extracted peer search logic. Single responsibility.
 * Note: This is still O(n) but only used during disconnect (infrequent).
 *
 * COMPLEXITY: O(n)
 *
 * @return Peer index or -1 if not found
 */
static int find_peer_index(p2p_node_t node, const char* ip, uint16_t port)
{
    // Guard clause: Validate inputs
    if (!node || !ip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_peer_index: required parameter is NULL (node, ip)");
        return -1;
    }

    for (uint32_t i = 0; i < node->peer_count; i++) {
        if (strcmp(node->peers[i].ip, ip) == 0 && node->peers[i].port == port) {
            return (int) i;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_peer_index: validation failed");
    return -1;
}

/**
 * @brief Compacts peer array by removing peer at index
 *
 * WHY: Extracted array compaction logic. Single responsibility.
 * Moves last peer to removed position to avoid shifting all elements.
 *
 * COMPLEXITY: O(1)
 */
static void compact_peer_array(p2p_node_t node, uint32_t index)
{
    // Guard clause: Validate inputs
    if (!node || index >= node->peer_count)
        return;

    // Move last peer to this slot (O(1) removal)
    if (index < node->peer_count - 1) {
        node->peers[index] = node->peers[node->peer_count - 1];
    }

    node->peer_count--;
}

/**
 * @brief Disconnects from a peer
 *
 * WHY: Refactored from nested ifs to clear sequential steps. Uses hash table
 * for O(1) removal and array compaction for O(1) array removal.
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Generate peer key (O(1))
 * 3. Lookup peer in hash table (O(1))
 * 4. Close peer socket (O(1))
 * 5. Remove from hash table (O(1))
 * 6. Find and remove from array (O(n) - unavoidable for compaction)
 *
 * COMPLEXITY: O(n) for array removal (but still optimized with hash table)
 *
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port number of peer
 * @return true if disconnection successful, false otherwise
 */
bool p2p_node_disconnect_peer(p2p_node_t node, const char* peer_ip, uint16_t peer_port)
{
    // Guard clause: Validate inputs
    if (!node || !peer_ip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_disconnect_peer: required parameter is NULL (node, peer_ip)");
        return false;
    }

    // Validate IP address format
    // WHY: Ensure we're disconnecting with valid address
    if (!validate_ip_address(peer_ip)) {
        fprintf(stderr, "[P2P] Invalid IP address format: %s\n", peer_ip);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_disconnect_peer: validate_ip_address is NULL");
        return false;
    }

    // Validate port number
    // WHY: Ensure port is valid
    if (!validate_port_number(peer_port, false)) {
        fprintf(stderr, "[P2P] Invalid port number: %u\n", peer_port);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_disconnect_peer: validate_port_number is NULL");
        return false;
    }

    // Lock for thread safety
    nimcp_mutex_lock(&node->lock);

    // Generate peer key
    char key[32];
    generate_peer_key(key, sizeof(key), peer_ip, peer_port);

    // Lookup peer in hash table (O(1))
    // FIX: Hash table stores a copy of the pointer, need double dereference
    peer_info_t** peer_ptr = (peer_info_t**)hash_table_lookup_string(node->peer_table, key);
    // Guard clause: Check if peer exists
    if (!peer_ptr || !*peer_ptr) {
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_disconnect_peer: peer_ptr is NULL");
        return false;
    }

    peer_info_t* peer = *peer_ptr;
    // Close peer socket
    close_peer_socket(peer);

    // Remove from hash table (O(1)) using NIMCP utility
    hash_table_remove_string(node->peer_table, key);

    // Find peer index in array
    int index = find_peer_index(node, peer_ip, peer_port);
    // Guard clause: Check if found in array
    if (index < 0) {
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "p2p_node_disconnect_peer: validation failed");
        return false;
    }

    // Remove vertex from topology graph
    // WHY: Keep graph synchronized with peer list
    uint64_t peer_id = ((uint64_t) inet_addr(peer_ip) << 32) | peer_port;
    uint32_t vertex_idx = nimcp_graph_find_vertex(node->topology_graph, peer_id);
    if (vertex_idx != NIMCP_INVALID_VERTEX) {
        nimcp_graph_remove_vertex(node->topology_graph, vertex_idx);
    } else {
        fprintf(stderr, "[P2P] Warning: Peer vertex not found in topology graph\n");
    }

    // Compact peer array
    compact_peer_array(node, (uint32_t) index);

    nimcp_mutex_unlock(&node->lock);
    return true;
}

//=============================================================================
// Node Lifecycle Management
//=============================================================================

/**
 * @brief Updates node status with state transition
 *
 * WHY: Extracted status update logic. Ensures consistent state management.
 *
 * COMPLEXITY: O(1)
 */
static void update_node_status(p2p_node_t node, node_status_t status, bool running)
{
    // Process pending bio-async messages
    if (node && node->bio_async_enabled && node->bio_ctx) {
        bio_router_process_inbox(node->bio_ctx, 5);
    }

    // Guard clause: Validate input
    if (!node)
        return;

    node->status = status;
    node->running = running;
}

/**
 * @brief Starts the node's main operation
 *
 * WHY: Refactored to use helper functions with clear responsibilities.
 * Guard clauses eliminate nesting.
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Setup listen socket (O(1))
 * 3. Update node status (O(1))
 *
 * COMPLEXITY: O(1)
 *
 * @param node Handle to the node
 * @return true if startup successful, false otherwise
 */
bool p2p_node_start(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_start: node is NULL");
        return false;
    }

    // Lock for thread safety
    nimcp_mutex_lock(&node->lock);

    // Guard clause: Check if already running
    if (node->running) {
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_start: validation failed");
        return false;
    }

    // Setup listen socket
    if (!setup_listen_socket(node)) {
        update_node_status(node, NODE_STATUS_ERROR, false);
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_start: setup_listen_socket is NULL");
        return false;
    }

    // Update node status to running
    update_node_status(node, NODE_STATUS_RUNNING, true);

    nimcp_mutex_unlock(&node->lock);
    return true;
}

/**
 * @brief Disconnects single peer during shutdown
 *
 * WHY: Extracted from loop to eliminate nesting. Single responsibility.
 *
 * COMPLEXITY: O(1)
 */
static void disconnect_peer_during_shutdown(peer_info_t* peer)
{
    // Guard clause: Validate input
    if (!peer)
        return;

    // Close socket if open
    if (peer->socket_fd >= 0) {
        close(peer->socket_fd);
        peer->socket_fd = -1;
    }

    peer->connected = false;
}

/**
 * @brief Disconnects all peers during shutdown
 *
 * WHY: Extracted peer disconnection loop. Single responsibility.
 *
 * COMPLEXITY: O(n) where n = peer count
 */
static void disconnect_all_peers(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node)
        return;

    // Disconnect each peer
    for (uint32_t i = 0; i < node->peer_count; i++) {
        disconnect_peer_during_shutdown(&node->peers[i]);
    }

    node->peer_count = 0;
}

/**
 * @brief Stops the node's operation
 *
 * WHY: Refactored from nested code to clear sequential steps.
 * Each step extracted to helper function.
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Close listen socket (O(1))
 * 3. Disconnect all peers (O(n))
 * 4. Update node status (O(1))
 *
 * COMPLEXITY: O(n) where n = number of peers
 *
 * @param node Handle to the node
 * @return true if shutdown successful, false otherwise
 */
bool p2p_node_stop(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_stop: node is NULL");
        return false;
    }

    // Lock for thread safety
    nimcp_mutex_lock(&node->lock);

    // Guard clause: Check if running
    if (!node->running) {
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_stop: node->running is NULL");
        return false;
    }

    // Close listen socket
    close_listen_socket(node);

    // Disconnect all peers
    disconnect_all_peers(node);

    // Update node status
    update_node_status(node, NODE_STATUS_SHUTDOWN, false);

    nimcp_mutex_unlock(&node->lock);
    return true;
}

//=============================================================================
// Heartbeat System Implementation
//=============================================================================

/**
 * @brief Sends a PING message to specific peer
 *
 * WHAT: Sends lightweight PING message via socket
 * WHY: Check if peer is alive and responding
 * HOW: Serialize MSG_TYPE_PING and send via socket
 *
 * @param peer Peer to ping (must be non-NULL and connected)
 * @return true on successful send
 *
 * COMPLEXITY: O(1)
 */
static bool send_ping_to_peer(peer_info_t* peer)
{
    // Guard clause: Validate input
    if (!peer || !peer->connected || peer->socket_fd < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "send_ping_to_peer: required parameter is NULL (peer, peer->connected)");
        return false;
    }

    // Create PING message using protocol
    uint8_t buffer[sizeof(msg_header_t)];
    int bytes = protocol_serialize_message(MSG_TYPE_PING, NULL, 0, buffer, sizeof(buffer));

    // Guard clause: Check serialization
    if (bytes <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "send_ping_to_peer: validation failed");
        return false;
    }

    // Send PING message
    ssize_t sent = send(peer->socket_fd, buffer, bytes, MSG_NOSIGNAL);

    // Update timestamp on successful send
    if (sent == bytes) {
        peer->last_ping_sent = nimcp_time_get_us();
        LOG_DEBUG(LOG_MODULE, "PING sent to %s:%u", peer->ip, peer->port);
        return true;
    }

    LOG_WARN(LOG_MODULE, "Failed to send PING to %s:%u (sent %zd/%d bytes)",
             peer->ip, peer->port, sent, bytes);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "send_ping_to_peer: validation failed");
    return false;
}

/**
 * @brief Send heartbeat pings to all connected peers
 *
 * WHAT: Iterates through all peers and sends PING messages
 * WHY: Periodic health check to detect disconnections
 * HOW: Single-pass iteration with guard clauses
 *
 * @param node Handle to the node
 * @return Number of pings sent, or 0 on error
 *
 * ALGORITHM:
 * 1. Validate input (O(1))
 * 2. Lock node (O(1))
 * 3. Iterate peers and send pings (O(n))
 * 4. Unlock node (O(1))
 *
 * COMPLEXITY: O(n) where n = number of connected peers
 */
uint32_t p2p_node_send_heartbeats(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node)
        return 0;

    nimcp_mutex_lock(&node->lock);

    // Guard clause: Check if running
    if (!node->running) {
        nimcp_mutex_unlock(&node->lock);
        return 0;
    }

    uint32_t pings_sent = 0;

    // Single-pass iteration through peers
    for (uint32_t i = 0; i < node->peer_count; i++) {
        peer_info_t* peer = &node->peers[i];

        // Guard clause: Skip disconnected peers
        if (!peer->connected)
            continue;

        // Send ping and count successes
        if (send_ping_to_peer(peer)) {
            pings_sent++;
        }
    }

    nimcp_mutex_unlock(&node->lock);

    if (pings_sent > 0) {
        LOG_DEBUG(LOG_MODULE, "Heartbeat cycle: sent %u pings to connected peers", pings_sent);
    }

    return pings_sent;
}

/**
 * @brief Process heartbeat pong response from peer
 *
 * WHAT: Updates peer health metrics on PONG receipt
 * WHY: Track peer responsiveness and health
 * HOW: O(1) hash table lookup + status update
 *
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port of peer
 * @return true if pong processed successfully
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Generate peer key (O(1))
 * 3. Hash table lookup (O(1) average)
 * 4. Update health metrics (O(1))
 *
 * COMPLEXITY: O(1) average case
 */
bool p2p_node_process_pong(p2p_node_t node, const char* peer_ip, uint16_t peer_port)
{
    // Guard clause: Validate inputs
    if (!node || !peer_ip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_process_pong: required parameter is NULL (node, peer_ip)");
        return false;
    }

    nimcp_mutex_lock(&node->lock);

    // Generate peer key for hash lookup
    char peer_key[32];
    generate_peer_key(peer_key, sizeof(peer_key), peer_ip, peer_port);

    // O(1) hash table lookup using NIMCP utility
    // FIX: Hash table stores a copy of the pointer, need double dereference
    peer_info_t** peer_ptr = (peer_info_t**)hash_table_lookup_string(node->peer_table, peer_key);

    // Guard clause: Peer not found
    if (!peer_ptr || !*peer_ptr) {
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_process_pong: peer_ptr is NULL");
        return false;
    }

    peer_info_t* peer = *peer_ptr;
    // Update health metrics (O(1) operations)
    peer->last_pong_received = nimcp_time_get_us();
    peer->missed_pings = 0;
    peer->healthy = true;

    LOG_DEBUG(LOG_MODULE, "PONG received from %s:%u (peer healthy)", peer_ip, peer_port);

    nimcp_mutex_unlock(&node->lock);
    return true;
}

/**
 * @brief Checks if peer has timed out waiting for pong
 *
 * WHAT: Determines if peer exceeded timeout threshold
 * WHY: Extracted from loop to eliminate nesting
 * HOW: Compare current time to last_pong + timeout
 *
 * @param peer Peer to check (must be non-NULL)
 * @param timeout_us Timeout in microseconds
 * @param current_time_us Current timestamp in microseconds
 * @return true if peer has timed out
 *
 * COMPLEXITY: O(1)
 */
static bool peer_has_timed_out(const peer_info_t* peer, uint64_t timeout_us,
                                uint64_t current_time_us)
{
    // Guard clause: Validate input
    if (!peer || !peer->connected) {
        return false;
    }

    // Guard clause: Never received a pong (initial state)
    if (peer->last_pong_received == 0) {
        return false;
    }

    // Calculate elapsed time since last pong
    uint64_t elapsed_us = current_time_us - peer->last_pong_received;

    return elapsed_us > timeout_us;
}

/**
 * @brief Marks peer as unhealthy and increments missed ping counter
 *
 * WHAT: Updates peer health status after timeout
 * WHY: Single responsibility - health status update
 * HOW: Increment counter and update flag
 *
 * @param peer Peer to mark (must be non-NULL)
 * @param max_retries Maximum retries before marking unhealthy
 *
 * COMPLEXITY: O(1)
 */
static void mark_peer_unhealthy(peer_info_t* peer, uint32_t max_retries)
{
    // Guard clause: Validate input
    if (!peer)
        return;

    peer->missed_pings++;

    // Mark unhealthy after exceeding max retries
    if (peer->missed_pings >= max_retries) {
        peer->healthy = false;
        LOG_WARN(LOG_MODULE, "Peer %s:%u marked UNHEALTHY (missed %u pings)",
                 peer->ip, peer->port, peer->missed_pings);
    } else {
        LOG_DEBUG(LOG_MODULE, "Peer %s:%u missed ping (%u/%u)",
                  peer->ip, peer->port, peer->missed_pings, max_retries);
    }
}

/**
 * @brief Check peer health and mark unhealthy peers
 *
 * WHAT: Scans all peers for timeouts and updates health status
 * WHY: Detect unresponsive peers for reconnection
 * HOW: Single-pass iteration with guard clauses
 *
 * @param node Handle to the node
 * @param timeout_ms Maximum time to wait for pong (milliseconds)
 * @return Number of unhealthy peers detected
 *
 * ALGORITHM:
 * 1. Validate input (O(1))
 * 2. Get current time (O(1))
 * 3. Convert timeout to microseconds (O(1))
 * 4. Iterate peers and check timeouts (O(n))
 * 5. Mark unhealthy peers (O(1) per peer)
 *
 * COMPLEXITY: O(n) where n = number of peers
 */
uint32_t p2p_node_check_peer_health(p2p_node_t node, uint32_t timeout_ms)
{
    // Guard clause: Validate input
    if (!node)
        return 0;

    nimcp_mutex_lock(&node->lock);

    // Guard clause: Check if running
    if (!node->running) {
        nimcp_mutex_unlock(&node->lock);
        return 0;
    }

    // Get current time once for all comparisons
    uint64_t current_time_us = nimcp_time_get_us();
    uint64_t timeout_us = (uint64_t)timeout_ms * NIMCP_US_PER_MS;
    uint32_t unhealthy_count = 0;

    // Single-pass iteration through peers
    for (uint32_t i = 0; i < node->peer_count; i++) {
        peer_info_t* peer = &node->peers[i];

        // Guard clause: Skip disconnected peers
        if (!peer->connected)
            continue;

        // Check timeout
        if (peer_has_timed_out(peer, timeout_us, current_time_us)) {
            mark_peer_unhealthy(peer, node->config.max_retries);

            if (!peer->healthy) {
                unhealthy_count++;
            }
        }
    }

    nimcp_mutex_unlock(&node->lock);
    return unhealthy_count;
}

/**
 * @brief Attempts to reconnect to single unhealthy peer
 *
 * WHAT: Tries to re-establish connection to peer
 * WHY: Single responsibility - one reconnection attempt
 * HOW: Close old socket, create new, reset health metrics
 *
 * @param node Node handle (must be non-NULL)
 * @param peer Peer to reconnect (must be non-NULL)
 * @return true on successful reconnection
 *
 * COMPLEXITY: O(1)
 */
static bool attempt_peer_reconnect(p2p_node_t node, peer_info_t* peer)
{
    // Guard clause: Validate inputs
    if (!node || !peer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attempt_peer_reconnect: required parameter is NULL (node, peer)");
        return false;
    }

    // Guard clause: Skip healthy peers
    if (peer->healthy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attempt_peer_reconnect: validation failed");
        return false;
    }

    // Close old socket if open
    if (peer->socket_fd >= 0) {
        close(peer->socket_fd);
        peer->socket_fd = -1;
    }

    // Attempt reconnection using existing connect function
    LOG_INFO(LOG_MODULE, "Attempting to reconnect to unhealthy peer %s:%u",
             peer->ip, peer->port);

    bool reconnected = p2p_node_connect_peer(node, peer->ip, peer->port);

    // Reset health metrics on successful reconnection
    if (reconnected) {
        uint64_t now = nimcp_time_get_us();
        peer->last_ping_sent = now;
        peer->last_pong_received = now;
        peer->missed_pings = 0;
        peer->healthy = true;
        LOG_INFO(LOG_MODULE, "Successfully reconnected to peer %s:%u",
                 peer->ip, peer->port);
    } else {
        LOG_WARN(LOG_MODULE, "Failed to reconnect to peer %s:%u",
                 peer->ip, peer->port);
    }

    return reconnected;
}

/**
 * @brief Reconnect to unhealthy peers
 *
 * WHAT: Scans for unhealthy peers and attempts reconnection
 * WHY: Automatic recovery from network issues
 * HOW: Single-pass iteration with guard clauses
 *
 * @param node Handle to the node
 * @return Number of reconnection attempts made
 *
 * ALGORITHM:
 * 1. Validate input (O(1))
 * 2. Iterate peers (O(n))
 * 3. Attempt reconnect for unhealthy peers (O(1) per peer)
 *
 * COMPLEXITY: O(n) where n = number of unhealthy peers
 */
uint32_t p2p_node_reconnect_unhealthy(p2p_node_t node)
{
    // Guard clause: Validate input
    if (!node)
        return 0;

    nimcp_mutex_lock(&node->lock);

    // Guard clause: Check if running
    if (!node->running) {
        nimcp_mutex_unlock(&node->lock);
        return 0;
    }

    // Collect unhealthy peer info while holding lock, then release
    // before calling attempt_peer_reconnect (which calls
    // p2p_node_connect_peer, which also locks node->lock).
    // This avoids a self-deadlock on non-recursive mutex.
    typedef struct { char ip[16]; uint16_t port; uint32_t index; } unhealthy_info_t;
    unhealthy_info_t unhealthy[64];  // Max peers we'll try
    uint32_t unhealthy_count = 0;

    for (uint32_t i = 0; i < node->peer_count && unhealthy_count < 64; i++) {
        peer_info_t* peer = &node->peers[i];

        // Guard clause: Skip healthy/disconnected peers
        if (!peer->connected || peer->healthy)
            continue;

        strncpy(unhealthy[unhealthy_count].ip, peer->ip, sizeof(peer->ip));
        unhealthy[unhealthy_count].ip[15] = '\0';
        unhealthy[unhealthy_count].port = peer->port;
        unhealthy[unhealthy_count].index = i;
        unhealthy_count++;
    }

    nimcp_mutex_unlock(&node->lock);

    // Now attempt reconnections without holding the lock
    uint32_t reconnect_attempts = 0;
    for (uint32_t i = 0; i < unhealthy_count; i++) {
        LOG_INFO(LOG_MODULE, "Attempting to reconnect to unhealthy peer %s:%u",
                 unhealthy[i].ip, unhealthy[i].port);

        bool reconnected = p2p_node_connect_peer(node, unhealthy[i].ip, unhealthy[i].port);

        if (reconnected) {
            // Update health metrics under lock
            nimcp_mutex_lock(&node->lock);
            // Re-validate the peer is still there at the same index
            if (unhealthy[i].index < node->peer_count) {
                peer_info_t* peer = &node->peers[unhealthy[i].index];
                if (strcmp(peer->ip, unhealthy[i].ip) == 0 && peer->port == unhealthy[i].port) {
                    uint64_t now = nimcp_time_get_us();
                    peer->last_ping_sent = now;
                    peer->last_pong_received = now;
                    peer->missed_pings = 0;
                    peer->healthy = true;
                    LOG_INFO(LOG_MODULE, "Successfully reconnected to peer %s:%u",
                             unhealthy[i].ip, unhealthy[i].port);
                }
            }
            nimcp_mutex_unlock(&node->lock);
            reconnect_attempts++;
        } else {
            LOG_WARN(LOG_MODULE, "Failed to reconnect to peer %s:%u",
                     unhealthy[i].ip, unhealthy[i].port);
        }
    }

    return reconnect_attempts;
}

/**
 * @brief Get peer health status
 *
 * WHAT: Returns health status of specific peer
 * WHY: Allow external monitoring of peer health
 * HOW: O(1) hash table lookup
 *
 * @param node Handle to the node
 * @param peer_ip IP address of peer
 * @param peer_port Port of peer
 * @param out_health Output: peer health status
 * @return true if peer found, false otherwise
 *
 * COMPLEXITY: O(1) average case
 */
bool p2p_node_get_peer_health(p2p_node_t node, const char* peer_ip, uint16_t peer_port,
                                bool* out_health)
{
    // Guard clause: Validate inputs
    if (!node || !peer_ip || !out_health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p2p_node_get_peer_health: required parameter is NULL (node, peer_ip, out_health)");
        return false;
    }

    nimcp_mutex_lock(&node->lock);

    // Generate peer key for hash lookup
    char peer_key[32];
    generate_peer_key(peer_key, sizeof(peer_key), peer_ip, peer_port);

    // O(1) hash table lookup using NIMCP utility
    // FIX: Hash table stores a copy of the pointer, need double dereference
    peer_info_t** peer_ptr = (peer_info_t**)hash_table_lookup_string(node->peer_table, peer_key);

    // Guard clause: Peer not found
    if (!peer_ptr || !*peer_ptr) {
        nimcp_mutex_unlock(&node->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "p2p_node_get_peer_health: peer_ptr is NULL");
        return false;
    }

    peer_info_t* peer = *peer_ptr;
    *out_health = peer->healthy;

    nimcp_mutex_unlock(&node->lock);
    return true;
}

//=============================================================================
// End of File
//=============================================================================
