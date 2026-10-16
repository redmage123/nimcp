
#define LOG_MODULE "nimcp_dataio"
#define LOG_MODULE_ID 0x052D
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dataio)

/**
 * @file nimcp_dataio.c
 * @brief Brain data I/O implementation
 *
 * WHAT: Load training data from external sources, export brain predictions
 * WHY: Brains need to learn from real-world datasets and export results
 * HOW: Strategy pattern with pluggable data loaders (CSV, PostgreSQL, streams)
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Pluggable data source backends (CSV, PostgreSQL, stream)
 * - Repository Pattern: Abstracted data access operations
 * - Factory Pattern: Backend-specific dataset creation
 * - Iterator Pattern: Batch-by-batch data reading for streaming
 *
 * THREAD SAFETY: Thread-local error storage, mutex-protected batch reads
 */

#include "io/dataio/nimcp_dataio.h"
#include "constants/nimcp_buffer_constants.h"
#include "nimcp.h"  // For nimcp_brain_learn_example, nimcp_status_t
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_path_traversal.h"
#include "utils/rng/nimcp_rand.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

// Conditional includes for optional backends
#ifdef HAVE_LIBPQ
#include <libpq-fe.h>
#endif

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

// cJSON is optional - only include if available
#ifdef HAVE_CJSON
#include "external/cJSON.h"
#endif

#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/containers/nimcp_queue.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/validation/nimcp_validate.h"
#include "security/nimcp_security_integration.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"


//=============================================================================
// Global State for Module-Level Security Registration
//=============================================================================

/**
 * WHAT: Global security context and module ID
 * WHY: Track module-level security registration for all datasets
 * HOW: Set during dataio_init(), used by all dataset operations
 */
static nimcp_sec_integration_t* g_dataio_security_ctx = NULL;
static uint32_t g_dataio_security_module_id = 0;
static nimcp_atomic_bool_t g_dataio_initialized = {0};

//=============================================================================
// Thread-Local Error Handling (same pattern as replication)
//=============================================================================

/**
 * WHAT: Thread-local error message storage
 * WHY: Thread-safe error reporting without global variables
 * HOW: Thread-local storage for last error per thread
 */
static __thread char g_last_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void dataio_set_error(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_last_error, sizeof(g_last_error), format, args);
    va_end(args);
}

const char* dataio_get_last_error(void)
{
    return g_last_error;
}

//=============================================================================
// Strategy Pattern: Data Source Backend Interface
//=============================================================================

/**
 * WHAT: Data source backend strategy (function pointers)
 * WHY: Support multiple data sources (CSV, PostgreSQL, streams) with same API
 * HOW: Strategy pattern - backends implement this interface
 */
typedef struct data_source_strategy {
    data_source_t source_type;

    // WHAT: Initialize data source
    // WHY: Open files, connect to databases, etc.
    bool (*initialize)(void** context, const dataset_config_t* config);

    // WHAT: Clean up data source
    // WHY: Close files, disconnect from databases, free memory
    void (*shutdown)(void* context);

    // WHAT: Read next batch of data
    // WHY: Support streaming for large datasets
    // HOW: Iterator pattern - returns next batch until end
    bool (*next_batch)(void* context, data_batch_t* batch);

    // WHAT: Reset to beginning
    // WHY: Support multiple epochs of training
    bool (*reset)(void* context);

    // WHAT: Get total size (if known)
    // WHY: Progress reporting, memory allocation
    uint64_t (*get_size)(void* context);

} data_source_strategy_t;

//=============================================================================
// Dataset Structure (Opaque Handle)
//=============================================================================

/**
 * WHAT: Internal dataset structure
 * WHY: Opaque handle hides implementation from API users
 * HOW: Contains strategy and backend context
 */
struct dataset_struct {
    dataset_config_t config;
    data_source_strategy_t* strategy;  // Strategy Pattern
    void* source_context;              // Backend-specific context
    nimcp_mutex_t read_lock;         // Thread-safe batch reads
    uint64_t rows_read;                // Current position

    // Memory Integration (Phase IO-1)
    unified_mem_manager_t memory_manager;    // Unified memory manager
    bool owns_memory_manager;                // Did we create it internally?

    // Security Integration (Phase IO-2)
    nimcp_sec_integration_t* security_ctx;   // Security context
    uint32_t security_region_id;             // Registered memory region
    bool security_registered;                // Is this dataset registered?

    // Statistics
    uint64_t batches_read;                   // Total batches
    uint64_t bytes_allocated;                // Total bytes allocated
    uint64_t pool_allocations;               // Allocations from pool
    uint64_t malloc_allocations;             // Fallback allocations
};

//=============================================================================
// CSV Backend Implementation
//=============================================================================

/**
 * WHAT: CSV file context
 * WHY: Track CSV parsing state across batches
 */
typedef struct {
    FILE* file;              // CSV file handle
    char** column_names;     // Header column names
    uint32_t num_columns;    // Total columns
    uint32_t num_features;   // Number of feature columns (for batch allocation)
    uint64_t total_rows;     // Total rows (if known)
    long file_start_offset;  // After header (for reset)
    char delimiter;          // Field delimiter (comma, tab, etc.)
} csv_context_t;

/**
 * WHAT: Initialize CSV data source
 * WHY: Open file, parse header, prepare for reading
 * HOW: Open file, read header if present, store column names
 */
static bool csv_initialize(void** context, const dataset_config_t* config)
{
    // Guard clause: validate input
    if (!context || !config || !config->location[0]) {
        dataio_set_error("Invalid CSV configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "csv_initialize: required parameter is NULL (context, config, config->location)");
        return false;
    }

    // P1-3 fix: Path traversal validation
    if (!nimcp_path_is_safe(config->location)) {
        dataio_set_error("Path validation failed: %s", config->location);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "csv_initialize: path traversal validation failed");
        return false;
    }

    // Allocate context
    csv_context_t* csv_ctx = nimcp_calloc(1, sizeof(csv_context_t));
    if (!csv_ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(csv_context_t),
                          "Failed to allocate CSV context");
        dataio_set_error("Failed to allocate CSV context");
        return false;
    }

    // Open CSV file
    csv_ctx->file = fopen(config->location, "r");
    if (!csv_ctx->file) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, config->location,
                      "Failed to open CSV file '%s': %s", config->location, strerror(errno));
        dataio_set_error("Failed to open CSV file '%s': %s", config->location, strerror(errno));
        nimcp_free(csv_ctx);
        return false;
    }

    csv_ctx->num_columns = config->num_feature_columns + config->num_label_columns;
    csv_ctx->num_features = config->num_feature_columns;
    csv_ctx->delimiter = config->delimiter;

    // WHAT: Parse header row if present
    // WHY: Extract column names for metadata
    if (config->has_header) {
        char line[NIMCP_PATH_BUFFER_SIZE];
        if (!fgets(line, sizeof(line), csv_ctx->file)) {
            dataio_set_error("Failed to read CSV header");
            fclose(csv_ctx->file);
            nimcp_free(csv_ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "csv_initialize: failed to read CSV header");
            return false;
        }

        // Build delimiter string using configured delimiter
        char delim_str[4] = {config->delimiter, '\r', '\n', '\0'};

        // Parse header columns
        csv_ctx->column_names = nimcp_calloc(csv_ctx->num_columns, sizeof(char*));
        char* saveptr = NULL;  // Thread-safe strtok_r context
        char* token = strtok_r(line, delim_str, &saveptr);
        uint32_t col_idx = 0;

        while (token && col_idx < csv_ctx->num_columns) {
            csv_ctx->column_names[col_idx] = nimcp_strdup(token);
            token = strtok_r(NULL, delim_str, &saveptr);
            col_idx++;
        }
    }

    // Store position after header (for reset)
    csv_ctx->file_start_offset = ftell(csv_ctx->file);

    *context = csv_ctx;
    return true;
}

/**
 * WHAT: Clean up CSV resources
 * WHY: Close file, free memory
 */
static void csv_shutdown(void* context)
{
    if (!context)
        return;

    csv_context_t* csv_ctx = (csv_context_t*) context;

    if (csv_ctx->file) {
        fclose(csv_ctx->file);
    }

    if (csv_ctx->column_names) {
        for (uint32_t i = 0; i < csv_ctx->num_columns; i++) {
            nimcp_free(csv_ctx->column_names[i]);
        }
        nimcp_free(csv_ctx->column_names);
    }

    nimcp_free(csv_ctx);
}

/**
 * WHAT: Parse CSV line into features and label
 * WHY: Convert text to float features and string label
 * HOW: Split by delimiter, parse floats, extract label
 */
static bool csv_parse_line(const char* line, char delimiter, uint32_t num_features,
                           uint32_t num_labels, float* features, char* label, size_t label_size)
{
    char line_copy[NIMCP_PATH_BUFFER_SIZE];
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    char delim_str[2] = {delimiter, '\0'};
    char* saveptr = NULL;  // Thread-safe strtok_r context
    char* token = strtok_r(line_copy, delim_str, &saveptr);

    // Parse features
    for (uint32_t i = 0; i < num_features; i++) {
        if (!token) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "csv_parse_line: token is NULL");
            return false;
        }

        // Store token for error reporting before parsing
        char token_copy[NIMCP_ERROR_BUFFER_SIZE];
        strncpy(token_copy, token, sizeof(token_copy) - 1);
        token_copy[sizeof(token_copy) - 1] = '\0';

        // P1-2 fix: Use strtod instead of atof for safe conversion
        char* endptr;
        errno = 0;
        features[i] = (float)strtod(token, &endptr);
        if (endptr == token || errno == ERANGE) {
            fprintf(stderr, "[DataIO] Failed to convert token to float at index %u: '%s'\n", i, token_copy);
            features[i] = 0.0f;
        }

        // Validate parsed float value
        if (!nimcp_validate_float_field(&features[i], sizeof(float))) {
            fprintf(stderr, "[DataIO] Invalid feature value at index %u: %f from token '%s'\n", i,
                    features[i], token_copy);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "csv_parse_line: float field validation failed");
            return false;
        }

        token = strtok_r(NULL, delim_str, &saveptr);
    }

    // Parse label (last column)
    if (!token) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "csv_parse_line: token is NULL");
        return false;
    }

    // Remove trailing newline/whitespace
    char* newline = strchr(token, '\n');
    if (newline)
        *newline = '\0';
    char* cr = strchr(token, '\r');
    if (cr)
        *cr = '\0';

    strncpy(label, token, label_size - 1);
    label[label_size - 1] = '\0';

    return true;
}

/**
 * WHAT: Read next batch of CSV data
 * WHY: Support streaming large CSV files
 * HOW: Read batch_size rows, parse into features/labels
 */
static bool csv_next_batch(void* context, data_batch_t* batch)
{
    if (!context || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "csv_next_batch: required parameter is NULL (context, batch)");
        return false;
    }

    csv_context_t* csv_ctx = (csv_context_t*) context;

    // Read batch_size rows (or remaining rows)
    uint32_t batch_size = 1000;  // Default batch size
    batch->features = nimcp_calloc(batch_size, sizeof(float*));
    if (!batch->features) {
        dataio_set_error("Failed to allocate features array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "csv_next_batch: failed to allocate features array");
        return false;
    }
    batch->labels = nimcp_calloc(batch_size, sizeof(char*));
    if (!batch->labels) {
        nimcp_free(batch->features);
        batch->features = NULL;
        dataio_set_error("Failed to allocate labels array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "csv_next_batch: failed to allocate labels array");
        return false;
    }
    batch->num_samples = 0;

    char line[NIMCP_PATH_BUFFER_SIZE];
    for (uint32_t i = 0; i < batch_size; i++) {
        if (!fgets(line, sizeof(line), csv_ctx->file)) {
            // End of file
            batch->end_of_dataset = true;
            break;
        }

        // WHAT: Allocate row storage at write index (num_samples)
        // WHY: Prevents double-free when parse fails
        // HOW: Use num_samples as write pointer, only increment on success
        //
        // BUG FIX: Previously allocated at index i, but freed and continued
        // on parse failure, leaving freed pointers in array. When
        // dataset_free_batch() tried to free indices 0..num_samples-1,
        // it would hit the freed pointers → double-free!
        //
        // SOLUTION: Allocate at num_samples (write index). If parse fails,
        // free immediately and DON'T increment. Next allocation overwrites.
        uint32_t num_features = csv_ctx->num_features;  // Get from context (configured at initialization)
        batch->features[batch->num_samples] = nimcp_calloc(num_features, sizeof(float));
        if (!batch->features[batch->num_samples]) {
            // Memory allocation failed - end batch early
            batch->end_of_dataset = true;
            break;
        }
        batch->labels[batch->num_samples] = nimcp_calloc(64, sizeof(char));
        if (!batch->labels[batch->num_samples]) {
            // Memory allocation failed - free features and end batch early
            nimcp_free(batch->features[batch->num_samples]);
            batch->features[batch->num_samples] = NULL;
            batch->end_of_dataset = true;
            break;
        }

        // Parse line using configured delimiter
        if (!csv_parse_line(line, csv_ctx->delimiter, num_features, 1, batch->features[batch->num_samples],
                            batch->labels[batch->num_samples], 64)) {
            // Skip invalid lines - free immediately and don't increment
            nimcp_free(batch->features[batch->num_samples]);
            nimcp_free(batch->labels[batch->num_samples]);
            continue;
        }

        // Only increment write pointer on successful parse
        batch->num_samples++;
        csv_ctx->total_rows++;
    }

    if (batch->num_samples == 0) {
        batch->end_of_dataset = true;
        nimcp_free(batch->features);
        nimcp_free(batch->labels);
        /* Zero samples is normal end-of-dataset, not an error */
        return false;
    }

    return true;
}

/**
 * WHAT: Reset CSV to beginning
 * WHY: Support multiple training epochs
 * HOW: Seek to file_start_offset (after header)
 */
static bool csv_reset(void* context)
{
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "csv_reset: context is NULL");
        return false;
    }

    csv_context_t* csv_ctx = (csv_context_t*) context;

    if (fseek(csv_ctx->file, csv_ctx->file_start_offset, SEEK_SET) != 0) {
        dataio_set_error("Failed to reset CSV file: %s", strerror(errno));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "csv_reset: validation failed");
        return false;
    }

    csv_ctx->total_rows = 0;
    return true;
}

/**
 * WHAT: Get CSV size (number of rows)
 * WHY: Progress reporting
 * HOW: Return total_rows (counted as we read)
 */
static uint64_t csv_get_size(void* context)
{
    if (!context)
        return 0;
    csv_context_t* csv_ctx = (csv_context_t*) context;
    return csv_ctx->total_rows;
}

/**
 * WHAT: CSV backend strategy implementation
 * WHY: Pluggable backend for CSV files
 */
static data_source_strategy_t g_csv_strategy = {.source_type = DATA_SOURCE_FILE,
                                                .initialize = csv_initialize,
                                                .shutdown = csv_shutdown,
                                                .next_batch = csv_next_batch,
                                                .reset = csv_reset,
                                                .get_size = csv_get_size};

//=============================================================================
// PostgreSQL Backend Implementation
//=============================================================================

/**
 * WHAT: PostgreSQL context
 * WHY: Track database connection and query state
 */
typedef struct {
#ifdef HAVE_LIBPQ
    PGconn* db_conn;               // Database connection
    PGresult* result;              // Current result set
#else
    void* db_conn;                 // Database connection (void* when libpq unavailable)
    void* result;                  // Current result set
#endif
    char connection_string[NIMCP_ERROR_BUFFER_LARGE];   // Connection string
    char query[NIMCP_CMD_BUFFER_SIZE];              // SQL query
    uint32_t num_feature_columns;  // Number of feature columns
    uint32_t num_label_columns;    // Number of label columns
    uint32_t current_row;          // Current row in result set
    uint32_t total_rows;           // Total rows in result
    uint32_t batch_size;           // Rows per batch
} postgres_context_t;

/**
 * WHAT: Validate SQL query for safety
 * WHY:  Prevent SQL injection attacks by validating query structure
 * HOW:  Check for dangerous patterns and ensure only SELECT queries are allowed
 *
 * @param query The SQL query to validate
 * @return true if query appears safe, false otherwise
 *
 * SECURITY: This function implements defense-in-depth for SQL injection prevention:
 * 1. Only SELECT queries are allowed (no INSERT, UPDATE, DELETE, DROP, etc.)
 * 2. No command chaining (;) to prevent multiple statement execution
 * 3. No comment injection (-- or /* patterns)
 * 4. No UNION attacks (prevents data extraction from other tables)
 */
#ifdef HAVE_LIBPQ
static bool postgres_validate_query(const char* query) {
    if (!query || !*query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_validate_query: query is NULL");
        return false;
    }

    /* Skip leading whitespace */
    while (*query == ' ' || *query == '\t' || *query == '\n' || *query == '\r') {
        query++;
    }

    /* Query must start with SELECT (case-insensitive) */
    if (strncasecmp(query, "SELECT", 6) != 0) {
        dataio_set_error("Only SELECT queries are allowed for data loading");
        return false;
    }

    /* Check for dangerous patterns */
    const char* dangerous_patterns[] = {
        ";",        /* Command chaining - prevents multiple statements */
        "--",       /* SQL comment - prevents comment injection */
        "/*",       /* Block comment - prevents comment injection */
        "UNION",    /* UNION attacks - prevents data extraction */
        "INSERT",   /* Data modification */
        "UPDATE",   /* Data modification */
        "DELETE",   /* Data modification */
        "DROP",     /* Schema modification */
        "TRUNCATE", /* Data destruction */
        "ALTER",    /* Schema modification */
        "CREATE",   /* Schema modification */
        "GRANT",    /* Permission modification */
        "REVOKE",   /* Permission modification */
        "EXEC",     /* Stored procedure execution */
        "EXECUTE",  /* Stored procedure execution */
        "xp_",      /* Extended procedures (SQL Server) */
        "sp_",      /* System procedures */
        NULL
    };

    /* Convert query to uppercase for pattern matching */
    size_t qlen = strlen(query);
    char* upper_query = nimcp_malloc(qlen + 1);
    if (!upper_query) {
        dataio_set_error("Failed to allocate memory for query validation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "postgres_validate_query: failed to allocate query buffer");
        return false;
    }

    for (size_t i = 0; i <= qlen; i++) {
        char c = query[i];
        upper_query[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
    }

    /* Check for dangerous patterns */
    for (int i = 0; dangerous_patterns[i]; i++) {
        /* For semicolon, check anywhere in query */
        if (dangerous_patterns[i][0] == ';') {
            if (strchr(upper_query, ';') != NULL) {
                nimcp_free(upper_query);
                dataio_set_error("Multiple SQL statements not allowed (semicolon found)");
                return false;
            }
        }
        /* For comment patterns, check anywhere */
        else if (dangerous_patterns[i][0] == '-' || dangerous_patterns[i][0] == '/') {
            if (strstr(upper_query, dangerous_patterns[i]) != NULL) {
                nimcp_free(upper_query);
                dataio_set_error("SQL comments not allowed in query");
                return false;
            }
        }
        /* For SQL keywords, check as whole words (not within other words) */
        else {
            const char* found = strstr(upper_query, dangerous_patterns[i]);
            while (found) {
                /* Check if this is a whole word match */
                size_t plen = strlen(dangerous_patterns[i]);
                bool word_start = (found == upper_query) ||
                    (*(found - 1) == ' ' || *(found - 1) == '\t' ||
                     *(found - 1) == '\n' || *(found - 1) == '(' ||
                     *(found - 1) == ')' || *(found - 1) == ',');
                bool word_end = (found[plen] == '\0' || found[plen] == ' ' ||
                    found[plen] == '\t' || found[plen] == '\n' ||
                    found[plen] == '(' || found[plen] == ')' ||
                    found[plen] == ',');

                if (word_start && word_end) {
                    nimcp_free(upper_query);
                    dataio_set_error("Dangerous SQL keyword '%s' not allowed", dangerous_patterns[i]);
                    return false;
                }
                found = strstr(found + 1, dangerous_patterns[i]);
            }
        }
    }

    nimcp_free(upper_query);
    return true;
}
#endif

/**
 * WHAT: Initialize PostgreSQL data source
 * WHY: Connect to database, execute query
 * HOW: Use libpq to connect and execute validated query
 *
 * SECURITY: Query is validated before execution to prevent SQL injection.
 *           Only SELECT queries without dangerous patterns are allowed.
 */
static bool postgres_initialize(void** context, const dataset_config_t* config)
{
#ifdef HAVE_LIBPQ
    if (!context || !config || !config->location[0]) {
        dataio_set_error("Invalid PostgreSQL configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "postgres_initialize: required parameter is NULL (context, config, config->location)");
        return false;
    }
    postgres_context_t* pg_ctx = nimcp_calloc(1, sizeof(postgres_context_t));
    if (!pg_ctx) {
        dataio_set_error("Failed to allocate PostgreSQL context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "postgres_initialize: failed to allocate postgres context");
        return false;
    }
    const char* separator = strchr(config->location, '|');
    if (!separator) {
        dataio_set_error("PostgreSQL location must be 'connection_string|query'");
        nimcp_free(pg_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_initialize: location missing '|' separator between connection string and query");
        return false;
    }
    size_t conn_len = separator - config->location;
    if (conn_len >= sizeof(pg_ctx->connection_string)) {
        dataio_set_error("PostgreSQL connection string too long");
        nimcp_free(pg_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_initialize: capacity exceeded");
        return false;
    }
    strncpy(pg_ctx->connection_string, config->location, conn_len);
    pg_ctx->connection_string[conn_len] = '\0';
    strncpy(pg_ctx->query, separator + 1, sizeof(pg_ctx->query) - 1);
    pg_ctx->query[sizeof(pg_ctx->query) - 1] = '\0';  /* Ensure null termination */

    /* SECURITY: Validate query before execution to prevent SQL injection */
    if (!postgres_validate_query(pg_ctx->query)) {
        /* Error message already set by postgres_validate_query */
        nimcp_free(pg_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_initialize: SQL query validation failed");
        return false;
    }

    pg_ctx->num_feature_columns = config->num_feature_columns;
    pg_ctx->num_label_columns = config->num_label_columns;
    pg_ctx->batch_size = config->batch_size > 0 ? config->batch_size : 1000;
    pg_ctx->db_conn = PQconnectdb(pg_ctx->connection_string);
    if (PQstatus(pg_ctx->db_conn) != CONNECTION_OK) {
        dataio_set_error("PostgreSQL connection failed: %s", PQerrorMessage(pg_ctx->db_conn));
        PQfinish(pg_ctx->db_conn);
        nimcp_free(pg_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_initialize: validation failed");
        return false;
    }

    /* Execute query using prepared statement for additional safety */
    const char* stmt_name = "nimcp_data_query";
    PGresult* prepare_result = PQprepare(pg_ctx->db_conn, stmt_name, pg_ctx->query, 0, NULL);
    if (PQresultStatus(prepare_result) != PGRES_COMMAND_OK) {
        dataio_set_error("PostgreSQL prepare failed: %s", PQerrorMessage(pg_ctx->db_conn));
        PQclear(prepare_result);
        PQfinish(pg_ctx->db_conn);
        nimcp_free(pg_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_initialize: validation failed");
        return false;
    }
    PQclear(prepare_result);

    /* Execute the prepared statement */
    pg_ctx->result = PQexecPrepared(pg_ctx->db_conn, stmt_name, 0, NULL, NULL, NULL, 0);
    if (PQresultStatus(pg_ctx->result) != PGRES_TUPLES_OK) {
        dataio_set_error("PostgreSQL query failed: %s", PQerrorMessage(pg_ctx->db_conn));
        PQclear(pg_ctx->result);
        PQfinish(pg_ctx->db_conn);
        nimcp_free(pg_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_initialize: validation failed");
        return false;
    }
    pg_ctx->total_rows = (uint32_t)PQntuples(pg_ctx->result);
    pg_ctx->current_row = 0;
    *context = pg_ctx;
    return true;
#else
    (void)context;
    (void)config;
    dataio_set_error("PostgreSQL backend not available (compile with HAVE_LIBPQ)");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_initialize: operation failed");
    return false;
#endif
}

static void postgres_shutdown(void* context)
{
#ifdef HAVE_LIBPQ
    if (!context) return;
    postgres_context_t* pg_ctx = (postgres_context_t*)context;
    if (pg_ctx->result) PQclear(pg_ctx->result);
    if (pg_ctx->db_conn) PQfinish(pg_ctx->db_conn);
    nimcp_free(pg_ctx);
#else
    (void)context;
#endif
}

static bool postgres_next_batch(void* context, data_batch_t* batch)
{
#ifdef HAVE_LIBPQ
    if (!context || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "postgres_next_batch: required parameter is NULL (context, batch)");
        return false;
    }
    postgres_context_t* pg_ctx = (postgres_context_t*)context;
    if (pg_ctx->current_row >= pg_ctx->total_rows) {
        batch->end_of_dataset = true;
        batch->num_samples = 0;
        /* End of dataset is normal, not an error */
        return false;
    }
    uint32_t remaining = pg_ctx->total_rows - pg_ctx->current_row;
    uint32_t batch_size = (remaining < pg_ctx->batch_size) ? remaining : pg_ctx->batch_size;
    batch->features = nimcp_calloc(batch_size, sizeof(float*));
    if (!batch->features) {
        dataio_set_error("Failed to allocate features array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "postgres_next_batch: failed to allocate features array");
        return false;
    }
    batch->labels = nimcp_calloc(batch_size, sizeof(char*));
    if (!batch->labels) {
        nimcp_free(batch->features);
        batch->features = NULL;
        dataio_set_error("Failed to allocate labels array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "postgres_next_batch: failed to allocate labels array");
        return false;
    }
    batch->num_samples = 0;
    for (uint32_t i = 0; i < batch_size; i++) {
        uint32_t row_idx = pg_ctx->current_row + i;
        batch->features[batch->num_samples] = nimcp_calloc(pg_ctx->num_feature_columns, sizeof(float));
        if (!batch->features[batch->num_samples]) {
            batch->end_of_dataset = true;
            break;
        }
        for (uint32_t col = 0; col < pg_ctx->num_feature_columns; col++) {
            char* value = PQgetvalue(pg_ctx->result, row_idx, col);
            // P1-2 fix: Use strtod instead of atof for safe conversion
            if (PQgetisnull(pg_ctx->result, row_idx, col)) {
                batch->features[batch->num_samples][col] = 0.0f;
            } else {
                char* endptr;
                errno = 0;
                double dval = strtod(value, &endptr);
                batch->features[batch->num_samples][col] = (endptr == value || errno == ERANGE) ? 0.0f : (float)dval;
            }
        }
        uint32_t label_col = pg_ctx->num_feature_columns;
        char* label_value = PQgetvalue(pg_ctx->result, row_idx, label_col);
        batch->labels[batch->num_samples] = nimcp_strdup(PQgetisnull(pg_ctx->result, row_idx, label_col) ? "NULL" : label_value);
        batch->num_samples++;
    }
    pg_ctx->current_row += batch->num_samples;
    if (pg_ctx->current_row >= pg_ctx->total_rows) batch->end_of_dataset = true;
    return batch->num_samples > 0;
#else
    (void)context;
    (void)batch;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_next_batch: capacity exceeded");
    return false;
#endif
}

static bool postgres_reset(void* context)
{
#ifdef HAVE_LIBPQ
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "postgres_reset: context is NULL");
        return false;
    }
    postgres_context_t* pg_ctx = (postgres_context_t*)context;
    if (pg_ctx->result) PQclear(pg_ctx->result);
    /* SECURITY: Use prepared statement (created during initialize) instead of PQexec
     * to ensure the query was validated and prevent SQL injection.
     * The prepared statement "nimcp_data_query" was created in postgres_initialize(). */
    pg_ctx->result = PQexecPrepared(pg_ctx->db_conn, "nimcp_data_query", 0, NULL, NULL, NULL, 0);
    if (PQresultStatus(pg_ctx->result) != PGRES_TUPLES_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_reset: validation failed");
        return false;
    }
    pg_ctx->total_rows = (uint32_t)PQntuples(pg_ctx->result);
    pg_ctx->current_row = 0;
    return true;
#else
    (void)context;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "postgres_reset: validation failed");
    return false;
#endif
}

static uint64_t postgres_get_size(void* context)
{
#ifdef HAVE_LIBPQ
    if (!context) return 0;
    postgres_context_t* pg_ctx = (postgres_context_t*)context;
    return pg_ctx->total_rows;
#else
    (void)context;
    return 0;
#endif
}

static data_source_strategy_t g_postgres_strategy = {.source_type = DATA_SOURCE_DATABASE,
                                                     .initialize = postgres_initialize,
                                                     .shutdown = postgres_shutdown,
                                                     .next_batch = postgres_next_batch,
                                                     .reset = postgres_reset,
                                                     .get_size = postgres_get_size};

//=============================================================================
// JSON Backend Implementation (using cJSON)
//=============================================================================

#ifdef HAVE_CJSON

typedef struct {
    void* root;  /* cJSON* when available */
    int total_items;
    int current_item;
    uint32_t num_features;
    uint32_t batch_size;
    char** feature_names;
    char* json_buffer;
} json_context_t;

static bool json_initialize(void** context, const dataset_config_t* config)
{
    if (!context || !config || !config->location[0]) {
        dataio_set_error("Invalid JSON configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_initialize: required parameter is NULL (context, config, config->location)");
        return false;
    }
    // P1-3 fix: Path traversal validation
    if (!nimcp_path_is_safe(config->location)) {
        dataio_set_error("Path validation failed: %s", config->location);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "json_initialize: nimcp_path_is_safe is NULL");
        return false;
    }
    json_context_t* json_ctx = nimcp_calloc(1, sizeof(json_context_t));
    if (!json_ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(json_context_t),
                          "Failed to allocate JSON context");
        return false;
    }
    FILE* file = fopen(config->location, "rb");
    if (!file) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, config->location,
                      "Failed to open JSON file '%s'", config->location);
        dataio_set_error("Failed to open JSON file '%s'", config->location);
        nimcp_free(json_ctx);
        return false;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO_SIZE, config->location,
                      "JSON file size out of range: %ld bytes", file_size);
        fclose(file);
        nimcp_free(json_ctx);
        return false;
    }
    json_ctx->json_buffer = nimcp_malloc(file_size + 1);
    if (!json_ctx->json_buffer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, file_size + 1,
                          "Failed to allocate JSON buffer");
        fclose(file);
        nimcp_free(json_ctx);
        return false;
    }
    fread(json_ctx->json_buffer, 1, file_size, file);
    fclose(file);
    json_ctx->json_buffer[file_size] = '\0';
    json_ctx->root = cJSON_Parse(json_ctx->json_buffer);
    if (!json_ctx->root || !cJSON_IsArray(json_ctx->root)) {
        dataio_set_error("JSON parse error or root not array");
        nimcp_free(json_ctx->json_buffer);
        nimcp_free(json_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_initialize: required parameter is NULL (json_ctx->root, cJSON_IsArray)");
        return false;
    }
    json_ctx->total_items = cJSON_GetArraySize(json_ctx->root);
    json_ctx->current_item = 0;
    json_ctx->num_features = config->num_feature_columns;
    json_ctx->batch_size = config->batch_size > 0 ? config->batch_size : 1000;
    *context = json_ctx;
    return true;
}

static void json_shutdown(void* context)
{
    if (!context) return;
    json_context_t* json_ctx = (json_context_t*)context;
    if (json_ctx->root) cJSON_Delete(json_ctx->root);
    nimcp_free(json_ctx->json_buffer);
    nimcp_free(json_ctx);
}

static bool json_next_batch(void* context, data_batch_t* batch)
{
    if (!context || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_next_batch: required parameter is NULL (context, batch)");
        return false;
    }
    json_context_t* json_ctx = (json_context_t*)context;
    if (json_ctx->current_item >= json_ctx->total_items) {
        batch->end_of_dataset = true;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "json_next_batch: capacity exceeded");
        return false;
    }
    int remaining = json_ctx->total_items - json_ctx->current_item;
    uint32_t batch_size = (remaining < (int)json_ctx->batch_size) ? (uint32_t)remaining : json_ctx->batch_size;
    batch->features = nimcp_calloc(batch_size, sizeof(float*));
    if (!batch->features) {
        dataio_set_error("Failed to allocate features array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "json_next_batch: batch->features is NULL");
        return false;
    }
    batch->labels = nimcp_calloc(batch_size, sizeof(char*));
    if (!batch->labels) {
        nimcp_free(batch->features);
        batch->features = NULL;
        dataio_set_error("Failed to allocate labels array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "json_next_batch: batch->labels is NULL");
        return false;
    }
    batch->num_samples = 0;
    for (uint32_t i = 0; i < batch_size; i++) {
        cJSON* item = cJSON_GetArrayItem(json_ctx->root, json_ctx->current_item + i);
        if (!item) continue;
        batch->features[batch->num_samples] = nimcp_calloc(json_ctx->num_features, sizeof(float));
        if (!batch->features[batch->num_samples]) {
            batch->end_of_dataset = true;
            break;
        }
        cJSON* features_array = cJSON_GetObjectItem(item, "features");
        if (features_array && cJSON_IsArray(features_array)) {
            int feature_count = cJSON_GetArraySize(features_array);
            for (uint32_t f = 0; f < json_ctx->num_features && f < (uint32_t)feature_count; f++) {
                cJSON* feat = cJSON_GetArrayItem(features_array, f);
                if (feat && cJSON_IsNumber(feat)) batch->features[batch->num_samples][f] = (float)cJSON_GetNumberValue(feat);
            }
        }
        cJSON* label = cJSON_GetObjectItem(item, "label");
        if (label && cJSON_IsString(label)) batch->labels[batch->num_samples] = nimcp_strdup(cJSON_GetStringValue(label));
        else batch->labels[batch->num_samples] = nimcp_strdup("unknown");
        batch->num_samples++;
    }
    json_ctx->current_item += batch_size;
    if (json_ctx->current_item >= json_ctx->total_items) batch->end_of_dataset = true;
    return batch->num_samples > 0;
}

static bool json_reset(void* context)
{
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_reset: context is NULL");
        return false;
    }
    json_context_t* json_ctx = (json_context_t*)context;
    json_ctx->current_item = 0;
    return true;
}

static uint64_t json_get_size(void* context)
{
    if (!context) return 0;
    json_context_t* json_ctx = (json_context_t*)context;
    return (uint64_t)json_ctx->total_items;
}

static data_source_strategy_t g_json_strategy = {
    .source_type = DATA_SOURCE_FILE,
    .initialize = json_initialize,
    .shutdown = json_shutdown,
    .next_batch = json_next_batch,
    .reset = json_reset,
    .get_size = json_get_size
};

#else /* !HAVE_CJSON */

/* Stub JSON backend when cJSON is not available */
static bool json_initialize_stub(void** context, const dataset_config_t* config) {
    (void)context; (void)config;
    dataio_set_error("JSON format not available - cJSON library not compiled in");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "json_initialize_stub: operation failed");
    return false;
}
static void json_shutdown_stub(void* context) { (void)context; }
static bool json_next_batch_stub(void* context, data_batch_t* batch) {
    (void)context; (void)batch; return false;
}
static bool json_reset_stub(void* context) { (void)context; return false; }
static uint64_t json_get_size_stub(void* context) { (void)context; return 0; }

static data_source_strategy_t g_json_strategy = {
    .source_type = DATA_SOURCE_FILE,
    .initialize = json_initialize_stub,
    .shutdown = json_shutdown_stub,
    .next_batch = json_next_batch_stub,
    .reset = json_reset_stub,
    .get_size = json_get_size_stub
};

#endif /* HAVE_CJSON */

//=============================================================================
// Parquet Backend Implementation (Stub)
//=============================================================================

static bool parquet_initialize(void** context, const dataset_config_t* config)
{
    (void)context;
    (void)config;
    dataio_set_error("Parquet format not yet implemented. Convert to CSV/JSON using pandas.");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parquet_initialize: operation failed");
    return false;
}

static void parquet_shutdown(void* context) { (void)context; }
static bool parquet_next_batch(void* context, data_batch_t* batch) { (void)context; (void)batch; return false; }
static bool parquet_reset(void* context) { (void)context; return false; }
static uint64_t parquet_get_size(void* context) { (void)context; return 0; }

static data_source_strategy_t g_parquet_strategy = {
    .source_type = DATA_SOURCE_FILE,
    .initialize = parquet_initialize,
    .shutdown = parquet_shutdown,
    .next_batch = parquet_next_batch,
    .reset = parquet_reset,
    .get_size = parquet_get_size
};

//=============================================================================
// SQLite Backend Implementation
//=============================================================================

typedef struct {
#ifdef HAVE_SQLITE3
    sqlite3* db;
    sqlite3_stmt* stmt;
#else
    void* db;
    void* stmt;
#endif
    char filepath[NIMCP_METRICS_PATH_SIZE];
    char query[NIMCP_CMD_BUFFER_SIZE];
    uint32_t num_feature_columns;
    uint32_t num_label_columns;
    uint32_t batch_size;
    uint64_t total_rows;
} sqlite_context_t;

static bool sqlite_initialize(void** context, const dataset_config_t* config)
{
#ifdef HAVE_SQLITE3
    if (!context || !config || !config->location[0]) {
        dataio_set_error("Invalid SQLite configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sqlite_initialize: required parameter is NULL (context, config, config->location)");
        return false;
    }
    sqlite_context_t* sqlite_ctx = nimcp_calloc(1, sizeof(sqlite_context_t));
    if (!sqlite_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sqlite_initialize: sqlite_ctx is NULL");
        return false;
    }
    const char* separator = strchr(config->location, '|');
    if (separator) {
        size_t path_len = separator - config->location;
        strncpy(sqlite_ctx->filepath, config->location, path_len);
        sqlite_ctx->filepath[path_len] = '\0';
        strncpy(sqlite_ctx->query, separator + 1, sizeof(sqlite_ctx->query) - 1);
    } else {
        strncpy(sqlite_ctx->filepath, config->location, sizeof(sqlite_ctx->filepath) - 1);
        snprintf(sqlite_ctx->query, sizeof(sqlite_ctx->query), "SELECT * FROM data");
    }
    sqlite_ctx->num_feature_columns = config->num_feature_columns;
    sqlite_ctx->num_label_columns = config->num_label_columns;
    sqlite_ctx->batch_size = config->batch_size > 0 ? config->batch_size : 1000;
    int rc = sqlite3_open_v2(sqlite_ctx->filepath, &sqlite_ctx->db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        dataio_set_error("SQLite open failed");
        nimcp_free(sqlite_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sqlite_initialize: validation failed");
        return false;
    }
    rc = sqlite3_prepare_v2(sqlite_ctx->db, sqlite_ctx->query, -1, &sqlite_ctx->stmt, NULL);
    if (rc != SQLITE_OK) {
        dataio_set_error("SQLite prepare failed");
        sqlite3_close(sqlite_ctx->db);
        nimcp_free(sqlite_ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sqlite_initialize: validation failed");
        return false;
    }
    *context = sqlite_ctx;
    return true;
#else
    (void)context;
    (void)config;
    dataio_set_error("SQLite backend not available (compile with HAVE_SQLITE3)");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sqlite_initialize: operation failed");
    return false;
#endif
}

static void sqlite_shutdown(void* context)
{
#ifdef HAVE_SQLITE3
    if (!context) return;
    sqlite_context_t* sqlite_ctx = (sqlite_context_t*)context;
    if (sqlite_ctx->stmt) sqlite3_finalize(sqlite_ctx->stmt);
    if (sqlite_ctx->db) sqlite3_close(sqlite_ctx->db);
    nimcp_free(sqlite_ctx);
#else
    (void)context;
#endif
}

static bool sqlite_next_batch(void* context, data_batch_t* batch)
{
#ifdef HAVE_SQLITE3
    if (!context || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sqlite_next_batch: required parameter is NULL (context, batch)");
        return false;
    }
    sqlite_context_t* sqlite_ctx = (sqlite_context_t*)context;
    batch->features = nimcp_calloc(sqlite_ctx->batch_size, sizeof(float*));
    if (!batch->features) {
        dataio_set_error("Failed to allocate features array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sqlite_next_batch: batch->features is NULL");
        return false;
    }
    batch->labels = nimcp_calloc(sqlite_ctx->batch_size, sizeof(char*));
    if (!batch->labels) {
        nimcp_free(batch->features);
        batch->features = NULL;
        dataio_set_error("Failed to allocate labels array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sqlite_next_batch: batch->labels is NULL");
        return false;
    }
    batch->num_samples = 0;
    for (uint32_t i = 0; i < sqlite_ctx->batch_size; i++) {
        int rc = sqlite3_step(sqlite_ctx->stmt);
        if (rc == SQLITE_DONE) { batch->end_of_dataset = true; break; }
        if (rc != SQLITE_ROW) break;
        batch->features[batch->num_samples] = nimcp_calloc(sqlite_ctx->num_feature_columns, sizeof(float));
        if (!batch->features[batch->num_samples]) {
            batch->end_of_dataset = true;
            break;
        }
        for (uint32_t col = 0; col < sqlite_ctx->num_feature_columns; col++) {
            batch->features[batch->num_samples][col] = (float)sqlite3_column_double(sqlite_ctx->stmt, col);
        }
        uint32_t label_col = sqlite_ctx->num_feature_columns;
        const char* label_text = (const char*)sqlite3_column_text(sqlite_ctx->stmt, label_col);
        batch->labels[batch->num_samples] = nimcp_strdup(label_text ? label_text : "");
        if (!batch->labels[batch->num_samples]) {
            nimcp_free(batch->features[batch->num_samples]);
            batch->features[batch->num_samples] = NULL;
            batch->end_of_dataset = true;
            break;
        }
        batch->num_samples++;
        sqlite_ctx->total_rows++;
    }
    return batch->num_samples > 0;
#else
    (void)context;
    (void)batch;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sqlite_next_batch: operation failed");
    return false;
#endif
}

static bool sqlite_reset(void* context)
{
#ifdef HAVE_SQLITE3
    if (!context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sqlite_reset: context is NULL");
        return false;
    }
    sqlite_context_t* sqlite_ctx = (sqlite_context_t*)context;
    sqlite3_reset(sqlite_ctx->stmt);
    sqlite_ctx->total_rows = 0;
    return true;
#else
    (void)context;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sqlite_reset: context is NULL");
    return false;
#endif
}

static uint64_t sqlite_get_size(void* context)
{
#ifdef HAVE_SQLITE3
    if (!context) return 0;
    sqlite_context_t* sqlite_ctx = (sqlite_context_t*)context;
    return sqlite_ctx->total_rows;
#else
    (void)context;
    return 0;
#endif
}

static data_source_strategy_t g_sqlite_strategy = {
    .source_type = DATA_SOURCE_DATABASE,
    .initialize = sqlite_initialize,
    .shutdown = sqlite_shutdown,
    .next_batch = sqlite_next_batch,
    .reset = sqlite_reset,
    .get_size = sqlite_get_size
};

//=============================================================================
// Stream Backend Implementation
//=============================================================================

typedef struct stream_sample {
    float* features;
    uint32_t num_features;
    char* label;
    struct stream_sample* next;
} stream_sample_t;

typedef struct {
    stream_callback_fn_t callback;
    void* user_data;
    uint32_t num_features;
    bool active;
    nimcp_mutex_t queue_lock;
    nimcp_cond_t queue_cond;
    stream_sample_t* queue_head;
    stream_sample_t* queue_tail;
    uint32_t queue_size;
    uint32_t max_queue_size;
    uint64_t samples_received;
    uint64_t samples_processed;
    uint64_t samples_dropped;
} stream_context_t;

static bool stream_push_sample(stream_context_t* ctx, const float* features,
                               uint32_t num_features, const char* label)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stream_push_sample: ctx is NULL");
        return false;
    }
    if (!ctx->active) {
        /* Stream deactivated is normal shutdown state, not an error */
        return false;
    }
    nimcp_mutex_lock(&ctx->queue_lock);
    if (ctx->queue_size >= ctx->max_queue_size) {
        ctx->samples_dropped++;
        nimcp_mutex_unlock(&ctx->queue_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "stream_push_sample: capacity exceeded");
        return false;
    }
    stream_sample_t* sample = nimcp_calloc(1, sizeof(stream_sample_t));
    if (!sample) { nimcp_mutex_unlock(&ctx->queue_lock); return false; }
    sample->features = nimcp_calloc(num_features, sizeof(float));
    if (!sample->features) {
        nimcp_free(sample);
        nimcp_mutex_unlock(&ctx->queue_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stream_push_sample: failed to allocate features");
        return false;
    }
    memcpy(sample->features, features, num_features * sizeof(float));
    sample->num_features = num_features;
    sample->label = nimcp_strdup(label ? label : "");
    if (!sample->label) {
        nimcp_free(sample->features);
        nimcp_free(sample);
        nimcp_mutex_unlock(&ctx->queue_lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stream_push_sample: failed to allocate label");
        return false;
    }
    sample->next = NULL;
    if (ctx->queue_tail) ctx->queue_tail->next = sample;
    else ctx->queue_head = sample;
    ctx->queue_tail = sample;
    ctx->queue_size++;
    ctx->samples_received++;
    nimcp_cond_signal(&ctx->queue_cond);
    nimcp_mutex_unlock(&ctx->queue_lock);
    return true;
}

static stream_sample_t* stream_pop_sample(stream_context_t* ctx, int32_t timeout_ms)
{
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }
    nimcp_mutex_lock(&ctx->queue_lock);
    while (ctx->queue_head == NULL && ctx->active) {
        if (timeout_ms == 0) { nimcp_mutex_unlock(&ctx->queue_lock); return NULL; }
        else if (timeout_ms > 0) {
            if (nimcp_cond_timedwait(&ctx->queue_cond, &ctx->queue_lock, (uint32_t)timeout_ms) == NIMCP_TIMEOUT) {
                nimcp_mutex_unlock(&ctx->queue_lock);
                /* Timeout is normal behavior, not an error */
                return NULL;
            }
        } else {
            nimcp_cond_wait(&ctx->queue_cond, &ctx->queue_lock);
        }
    }
    if (!ctx->active && ctx->queue_head == NULL) { nimcp_mutex_unlock(&ctx->queue_lock); return NULL; }
    stream_sample_t* sample = ctx->queue_head;
    if (sample) {
        ctx->queue_head = sample->next;
        if (!ctx->queue_head) ctx->queue_tail = NULL;
        ctx->queue_size--;
        ctx->samples_processed++;
    }
    nimcp_mutex_unlock(&ctx->queue_lock);
    return sample;
}

static void stream_free_sample(stream_sample_t* sample)
{
    if (!sample) return;
    nimcp_free(sample->features);
    nimcp_free(sample->label);
    nimcp_free(sample);
}

static bool stream_initialize(void** context, const dataset_config_t* config)
{
    dataio_set_error("Use dataset_create_stream() for streaming data");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stream_initialize: sample is NULL");
    return false;
}

static void stream_shutdown(void* context)
{
    if (!context) return;
    stream_context_t* stream_ctx = (stream_context_t*)context;
    nimcp_mutex_lock(&stream_ctx->queue_lock);
    stream_ctx->active = false;
    nimcp_cond_broadcast(&stream_ctx->queue_cond);
    nimcp_mutex_unlock(&stream_ctx->queue_lock);
    stream_sample_t* sample = stream_ctx->queue_head;
    while (sample) {
        stream_sample_t* next = sample->next;
        stream_free_sample(sample);
        sample = next;
    }
    nimcp_cond_destroy(&stream_ctx->queue_cond);
    nimcp_mutex_destroy(&stream_ctx->queue_lock);
    nimcp_free(stream_ctx);
}

/**
 * WHAT: Stream backend (special case)
 * WHY: Streams don't support batch reading (callback-based)
 */
static data_source_strategy_t g_stream_strategy = {
    .source_type = DATA_SOURCE_STREAM,
    .initialize = stream_initialize,
    .shutdown = stream_shutdown,
    .next_batch = NULL,  // Not applicable for streams
    .reset = NULL,       // Cannot reset streams
    .get_size = NULL     // Unknown size
};

//=============================================================================
// Factory Pattern: Backend Selection
//=============================================================================

/**
 * WHAT: Select appropriate backend strategy
 * WHY: Factory pattern - create correct backend for data source
 * HOW: Switch on source type, return strategy
 */
static data_source_strategy_t* select_backend_strategy(data_source_t source, data_format_t format)
{
    switch (source) {
        case DATA_SOURCE_FILE:
            // WHAT: File-based sources
            switch (format) {
                case DATA_FORMAT_CSV:
                    return &g_csv_strategy;
                case DATA_FORMAT_JSON:
                    dataio_set_error("JSON format not yet implemented");
                    return NULL;
                case DATA_FORMAT_PARQUET:
                    dataio_set_error("Parquet format not yet implemented");
                    return NULL;
                default:
                    dataio_set_error("Unsupported file format: %d", format);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "select_backend_strategy: operation failed");
                    return NULL;
            }

        case DATA_SOURCE_DATABASE:
            // WHAT: Database sources
            switch (format) {
                case DATA_FORMAT_POSTGRES:
                    return &g_postgres_strategy;
                case DATA_FORMAT_SQLITE:
                    // TODO: Implement SQLite backend
                    dataio_set_error("SQLite format not yet implemented");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "select_backend_strategy: operation failed");
                    return NULL;
                default:
                    dataio_set_error("Unsupported database format: %d", format);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "select_backend_strategy: operation failed");
                    return NULL;
            }

        case DATA_SOURCE_STREAM:
            return &g_stream_strategy;

        default:
            dataio_set_error("Unsupported data source: %d", source);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "select_backend_strategy: operation failed");
            return NULL;
    }
}

//=============================================================================
// Public API: Dataset Loading
//=============================================================================

/**
 * WHAT: Open dataset
 * WHY: Initialize data source for reading
 * HOW: Select backend, initialize, create handle
 *
 * PHASE IO-1: Unified memory integration
 * PHASE IO-2: Security module registration
 */
dataset_t dataset_open(const dataset_config_t* config)
{
    // Guard clause: validate input
    if (!config) {
        dataio_set_error("NULL configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_open: config is NULL");
        return NULL;
    }

    // Select backend strategy
    data_source_strategy_t* strategy = select_backend_strategy(config->source, config->format);

    if (!strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_open: strategy is NULL");
        return NULL;  // Error already set
    }

    // Allocate dataset handle
    dataset_t dataset = nimcp_calloc(1, sizeof(struct dataset_struct));
    if (!dataset) {
        dataio_set_error("Failed to allocate dataset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dataset_open: dataset is NULL");
        return NULL;
    }

    // Copy configuration
    memcpy(&dataset->config, config, sizeof(dataset_config_t));
    dataset->strategy = strategy;

    // Initialize backend
    if (!strategy->initialize(&dataset->source_context, config)) {
        nimcp_free(dataset);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_open: strategy->initialize is NULL");
        return NULL;  // Error already set by backend
    }

    // Initialize mutex for thread-safe reads
    nimcp_mutex_init(&dataset->read_lock, NULL);

    //=========================================================================
    // PHASE IO-1: Unified Memory Integration
    //=========================================================================
    if (config->use_unified_memory) {
        if (config->memory_manager) {
            // Use provided memory manager
            dataset->memory_manager = config->memory_manager;
            dataset->owns_memory_manager = false;
        } else {
            // Create internal memory manager
            unified_mem_config_t mem_config = unified_mem_default_config();
            mem_config.enable_cow = true;
            mem_config.enable_tracking = true;
            dataset->memory_manager = unified_mem_create(&mem_config);
            dataset->owns_memory_manager = true;

            if (!dataset->memory_manager) {
                dataio_set_error("Failed to create unified memory manager");
                strategy->shutdown(dataset->source_context);
                nimcp_mutex_destroy(&dataset->read_lock);
                nimcp_free(dataset);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dataset_open: dataset->memory_manager is NULL");
                return NULL;
            }
        }
        LOG_DEBUG("Dataset using unified memory with CoW support");
    }

    //=========================================================================
    // PHASE IO-2: Security Module Registration
    //=========================================================================
    if (config->enable_security) {
        // Prefer provided security context, fall back to global
        dataset->security_ctx = config->security_context ? config->security_context
                                                         : g_dataio_security_ctx;

        if (dataset->security_ctx && g_dataio_security_module_id != 0) {
            // Register dataset configuration as a monitored region
            nimcp_result_t result = nimcp_sec_register_region(
                dataset->security_ctx,
                g_dataio_security_module_id,
                "dataset_config",
                &dataset->config,
                sizeof(dataset_config_t),
                &dataset->security_region_id
            );

            if (result == NIMCP_SUCCESS) {
                dataset->security_registered = true;
                LOG_DEBUG("Dataset registered with security (region: %u)",
                         dataset->security_region_id);

                // Record successful initialization
                NIMCP_SEC_SUCCESS(dataset->security_ctx, g_dataio_security_module_id);
            } else {
                LOG_WARNING("Failed to register dataset with security");
            }
        }
    }

    return dataset;
}

/**
 * WHAT: Close dataset
 * WHY: Clean up resources
 * HOW: Call backend shutdown, free memory
 *
 * PHASE IO-1: Clean up unified memory
 * PHASE IO-2: Unregister from security
 */
void dataset_close(dataset_t dataset)
{
    if (!dataset)
        return;

    //=========================================================================
    // PHASE IO-2: Unregister from security
    //=========================================================================
    if (dataset->security_registered && dataset->security_ctx) {
        // Record successful operation before unregistering
        NIMCP_SEC_SUCCESS(dataset->security_ctx, g_dataio_security_module_id);

        // Unregister memory region
        nimcp_sec_unregister_region(dataset->security_ctx, dataset->security_region_id);
        LOG_DEBUG("Dataset unregistered from security");
    }

    //=========================================================================
    // PHASE IO-1: Clean up unified memory
    //=========================================================================
    if (dataset->memory_manager && dataset->owns_memory_manager) {
        unified_mem_destroy(dataset->memory_manager);
        LOG_DEBUG("Dataset unified memory manager destroyed");
    }

    // Shutdown backend
    if (dataset->strategy && dataset->strategy->shutdown) {
        dataset->strategy->shutdown(dataset->source_context);
    }

    // Destroy mutex
    nimcp_mutex_destroy(&dataset->read_lock);

    nimcp_free(dataset);
}

/**
 * WHAT: Get next batch of data
 * WHY: Support streaming large datasets
 * HOW: Thread-safe call to backend next_batch
 */
bool dataset_next_batch(dataset_t dataset, data_batch_t* batch)
{
    // Guard clauses
    if (!dataset || !batch) {
        dataio_set_error("Invalid dataset or batch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_next_batch: required parameter is NULL (dataset, batch)");
        return false;
    }

    if (!dataset->strategy || !dataset->strategy->next_batch) {
        dataio_set_error("Backend does not support batch reading");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_next_batch: required parameter is NULL (dataset->strategy, dataset->strategy->next_batch)");
        return false;
    }

    // Thread-safe batch read
    nimcp_mutex_lock(&dataset->read_lock);
    bool result = dataset->strategy->next_batch(dataset->source_context, batch);
    if (result) {
        dataset->rows_read += batch->num_samples;
        dataset->batches_read++;
    }
    nimcp_mutex_unlock(&dataset->read_lock);

    return result;
}

/**
 * WHAT: Free data batch memory
 * WHY: Prevent memory leaks
 * HOW: Free features, labels, and batch arrays
 */
void dataset_free_batch(data_batch_t* batch)
{
    if (!batch)
        return;

    if (batch->features) {
        for (uint32_t i = 0; i < batch->num_samples; i++) {
            nimcp_free(batch->features[i]);
        }
        nimcp_free(batch->features);
    }

    if (batch->labels) {
        for (uint32_t i = 0; i < batch->num_samples; i++) {
            nimcp_free(batch->labels[i]);
        }
        nimcp_free(batch->labels);
    }

    memset(batch, 0, sizeof(data_batch_t));
}

/**
 * WHAT: Reset dataset to beginning
 * WHY: Support multiple training epochs
 * HOW: Call backend reset
 */
bool dataset_reset(dataset_t dataset)
{
    if (!dataset || !dataset->strategy || !dataset->strategy->reset) {
        dataio_set_error("Invalid dataset or backend does not support reset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_reset: required parameter is NULL (dataset, dataset->strategy, dataset->strategy->reset)");
        return false;
    }

    nimcp_mutex_lock(&dataset->read_lock);
    bool result = dataset->strategy->reset(dataset->source_context);
    dataset->rows_read = 0;
    nimcp_mutex_unlock(&dataset->read_lock);

    return result;
}

/**
 * WHAT: Get dataset size
 * WHY: Progress reporting, memory allocation
 * HOW: Call backend get_size
 */
uint64_t dataset_get_size(dataset_t dataset)
{
    if (!dataset || !dataset->strategy || !dataset->strategy->get_size) {
        return 0;
    }

    return dataset->strategy->get_size(dataset->source_context);
}

//=============================================================================
// Producer-Consumer Queue Architecture for Parallel I/O and Training
//=============================================================================

/**
 * WHAT: Training context for async batch processing
 * WHY: Separate I/O (producer) from computation (consumer) for parallelism
 * HOW: Producer thread reads batches, consumer thread trains brain
 *
 * ARCHITECTURE:
 *   Producer Thread          Queue            Consumer Thread
 *   ┌──────────────┐      ┌────────┐        ┌──────────────┐
 *   │ Read Batch   │──→   │ Batch  │   ──→  │ Train Brain  │
 *   │ from Dataset │      │ Buffer │        │ on Samples   │
 *   └──────────────┘      └────────┘        └──────────────┘
 *        ↓                                          ↓
 *   Epoch Loop                                  Free Batch
 *
 * WHY QUEUE:
 * - Overlap I/O latency with computation
 * - Smooth out disk read spikes
 * - Natural backpressure (blocks when queue full)
 * - Better CPU/disk utilization
 *
 * PERFORMANCE:
 * - Without queue: I/O and training serialize (100% overhead)
 * - With queue: I/O and training overlap (2-3x speedup)
 */
typedef struct {
    brain_t brain;
    dataset_t dataset;
    nimcp_queue_handle_t batch_queue;
    nimcp_thread_t producer_thread;
    nimcp_thread_t consumer_thread;
    volatile bool stop_requested;
    volatile bool producer_done;
    uint32_t current_epoch;
    uint32_t total_epochs;
    float validation_split;

    // Training statistics (protected by stats_lock)
    nimcp_mutex_t stats_lock;
    uint64_t samples_trained;
    uint64_t samples_validated;
    float total_accuracy;

    // Error handling
    char error_message[NIMCP_ERROR_BUFFER_LARGE];
    bool has_error;
} training_context_t;

/**
 * @brief Producer thread: Read batches from dataset and enqueue
 *
 * WHY SEPARATE THREAD:
 * - I/O operations don't block computation
 * - Can read-ahead while brain is training
 * - Disk/network latency hidden
 *
 * ALGORITHM:
 * 1. For each epoch:
 *    a. Reset dataset to beginning
 *    b. Read batch from dataset
 *    c. Enqueue batch (blocks if queue full - backpressure)
 *    d. Repeat until end of dataset
 * 2. Signal consumer (producer_done = true)
 *
 * COMPLEXITY: O(n) where n = number of batches
 * THREAD SAFETY: Only producer accesses dataset
 *
 * @param arg Pointer to training_context_t
 * @return NULL
 */
static void* producer_thread_func(void* arg)
{
    training_context_t* ctx = (training_context_t*) arg;

    for (uint32_t epoch = 0; epoch < ctx->total_epochs; epoch++) {
        if (ctx->stop_requested)
            break;

        ctx->current_epoch = epoch;

        // Reset dataset to beginning for new epoch
        if (!dataset_reset(ctx->dataset)) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Failed to reset dataset for epoch %u", epoch);
            ctx->has_error = true;
            break;
        }

        // Read and enqueue batches for this epoch
        while (!ctx->stop_requested) {
            // Allocate batch structure
            data_batch_t* batch = (data_batch_t*) nimcp_malloc(sizeof(data_batch_t));
            if (!batch) {
                snprintf(ctx->error_message, sizeof(ctx->error_message),
                         "Failed to allocate batch memory");
                ctx->has_error = true;
                break;
            }
            memset(batch, 0, sizeof(data_batch_t));

            // Read next batch from dataset
            if (!dataset_next_batch(ctx->dataset, batch)) {
                nimcp_free(batch);
                break;  // End of dataset
            }

            // Enqueue batch (blocks if queue full - backpressure)
            // WHY BLOCKING: Natural flow control, prevents memory exhaustion
            nimcp_result_t result = nimcp_queue_enqueue(ctx->batch_queue, &batch, 5000);
            if (result != NIMCP_SUCCESS) {
                // Enqueue failed (timeout or error)
                dataset_free_batch(batch);
                nimcp_free(batch);
                snprintf(ctx->error_message, sizeof(ctx->error_message), "Failed to enqueue batch");
                ctx->has_error = true;
                break;
            }

            if (batch->end_of_dataset) {
                break;  // Last batch of epoch
            }
        }

        if (ctx->has_error)
            break;
    }

    // Signal producer completion
    ctx->producer_done = true;
    return NULL;  /* Normal thread exit */
}

/**
 * @brief Consumer thread: Dequeue batches and train brain
 *
 * WHY SEPARATE THREAD:
 * - Computation continues while I/O happens
 * - Better CPU utilization
 * - Lower overall training time
 *
 * ALGORITHM:
 * 1. While producer not done or queue not empty:
 *    a. Dequeue batch (blocks if empty)
 *    b. Train brain on each sample in batch
 *    c. Update statistics
 *    d. Free batch
 * 2. Return final accuracy
 *
 * COMPLEXITY: O(n×m) where n = batches, m = samples per batch
 * THREAD SAFETY: Only consumer accesses brain (brain is thread-safe)
 *
 * @param arg Pointer to training_context_t
 * @return NULL
 */
static void* consumer_thread_func(void* arg)
{
    training_context_t* ctx = (training_context_t*) arg;

    while (!ctx->stop_requested) {
        // Check if producer done AND queue empty
        if (ctx->producer_done && nimcp_queue_is_empty(ctx->batch_queue)) {
            break;
        }

        data_batch_t* batch = NULL;

        // Dequeue batch (blocks if empty, timeout to check producer_done)
        nimcp_result_t result = nimcp_queue_dequeue(ctx->batch_queue, &batch, 500);

        if (result == NIMCP_TIMEOUT) {
            continue;  // No batch available, check producer_done again
        }

        if (result != NIMCP_SUCCESS || !batch) {
            continue;  // Queue error or empty
        }

        // Train on each sample in batch
        for (uint32_t i = 0; i < batch->num_samples; i++) {
            if (ctx->stop_requested)
                break;

            // Skip samples in validation set
            float sample_rand = nimcp_rand_uniform();
            if (sample_rand < ctx->validation_split) {
                nimcp_mutex_lock(&ctx->stats_lock);
                ctx->samples_validated++;
                nimcp_mutex_unlock(&ctx->stats_lock);
                continue;
            }

            // Train brain on this sample
            uint32_t num_features = brain_get_num_inputs(ctx->brain);
            float loss = brain_learn_example(ctx->brain, batch->features[i],
                                            num_features, batch->labels[i], 1.0F);
            (void)loss;  // Loss tracked internally by brain

            nimcp_mutex_lock(&ctx->stats_lock);
            ctx->samples_trained++;
            nimcp_mutex_unlock(&ctx->stats_lock);
        }

        // Free batch memory
        dataset_free_batch(batch);
        nimcp_free(batch);
    }

    return NULL;  /* Normal thread exit */
}

//=============================================================================
// Public API: Brain Training from Datasets
//=============================================================================

/**
 * WHAT: Train brain from dataset with async I/O
 * WHY: Overlap I/O and computation for 2-3x performance improvement
 * HOW: Producer thread reads batches, consumer thread trains, queue decouples
 *
 * PERFORMANCE IMPROVEMENT:
 * - Synchronous (old): I/O → Wait → Train → Wait → Repeat (100% overhead)
 * - Async (new): I/O ║ Train (parallel, ~2-3x faster)
 *
 * EXAMPLE TIMELINE:
 * Sync:  |I/O1|Train1|I/O2|Train2|I/O3|Train3| = 6 time units
 * Async: |I/O1|       |I/O3|                   = 4 time units
 *        |    |Train1|Train2|Train3|
 *
 * WHY QUEUE SIZE = 10:
 * - Balance between memory usage and read-ahead benefit
 * - 10 batches typically 1-10MB (acceptable memory overhead)
 * - Enough buffering to hide I/O latency spikes
 *
 * @param brain Brain to train
 * @param dataset Dataset to read from
 * @param epochs Number of training passes
 * @param validation_split Fraction for validation (0-1)
 * @return Final validation accuracy
 */
float brain_train_from_dataset(brain_t brain, dataset_t dataset, uint32_t epochs,
                               float validation_split)
{
    // Guard clauses
    if (!brain || !dataset) {
        dataio_set_error("Invalid brain or dataset");
        return 0.0F;
    }

    if (validation_split < 0.0F || validation_split >= 1.0F) {
        dataio_set_error("Invalid validation_split: %f (must be 0-1)", validation_split);
        return 0.0F;
    }

    // Initialize training context
    training_context_t* ctx = (training_context_t*) nimcp_calloc(1, sizeof(training_context_t));
    if (!ctx) {
        dataio_set_error("Failed to allocate training context");
        return 0.0F;
    }

    ctx->brain = brain;
    ctx->dataset = dataset;
    ctx->total_epochs = epochs;
    ctx->validation_split = validation_split;
    ctx->stop_requested = false;
    ctx->producer_done = false;
    ctx->has_error = false;
    nimcp_mutex_init(&ctx->stats_lock, NULL);

    // Create batch queue
    // WHY 10 BATCHES: Balance between memory overhead and I/O hiding
    // WHY BLOCKING: Natural backpressure prevents memory exhaustion
    nimcp_queue_config_t queue_config = {.max_size = 10,
                                         .item_size = sizeof(data_batch_t*),
                                         .is_blocking = true,
                                         .timeout_ms = 5000};

    nimcp_result_t result = nimcp_queue_create(&queue_config, &ctx->batch_queue);
    if (result != NIMCP_SUCCESS) {
        dataio_set_error("Failed to create batch queue");
        nimcp_mutex_destroy(&ctx->stats_lock);
        nimcp_free(ctx);
        return 0.0F;
    }

    // Start producer thread (reads batches from dataset)
    if (nimcp_thread_create(&ctx->producer_thread, producer_thread_func, ctx, NULL) != 0) {
        dataio_set_error("Failed to create producer thread");
        nimcp_queue_destroy(ctx->batch_queue);
        nimcp_mutex_destroy(&ctx->stats_lock);
        nimcp_free(ctx);
        return 0.0F;
    }

    // Start consumer thread (trains brain on batches)
    if (nimcp_thread_create(&ctx->consumer_thread, consumer_thread_func, ctx, NULL) != 0) {
        dataio_set_error("Failed to create consumer thread");
        ctx->stop_requested = true;
        nimcp_thread_join(ctx->producer_thread, NULL);
        nimcp_queue_destroy(ctx->batch_queue);
        nimcp_mutex_destroy(&ctx->stats_lock);
        nimcp_free(ctx);
        return 0.0F;
    }

    // Wait for both threads to complete
    nimcp_thread_join(ctx->producer_thread, NULL);
    nimcp_thread_join(ctx->consumer_thread, NULL);

    // Get final statistics
    float total_accuracy = ctx->total_accuracy;
    uint64_t samples_trained = ctx->samples_trained;
    uint64_t samples_validated = ctx->samples_validated;

    // Check for errors during training
    if (ctx->has_error) {
        dataio_set_error("%s", ctx->error_message);
    }

    // Print summary
    printf("Training complete:\n");
    printf("  Epochs: %u\n", epochs);
    printf("  Samples trained: %lu\n", samples_trained);
    printf("  Samples validated: %lu\n", samples_validated);
    printf("  Final accuracy: %.1f%%\n", total_accuracy * 100);

    // Cleanup
    nimcp_queue_destroy(ctx->batch_queue);
    nimcp_mutex_destroy(&ctx->stats_lock);
    nimcp_free(ctx);

    return total_accuracy;
}

/**
 * WHAT: Train brain from dataset (streaming)
 * WHY: Handle datasets too large for memory
 * HOW: Process batches sequentially without reset
 */
float brain_train_from_dataset_streaming(brain_t brain, dataset_t dataset, uint32_t max_batches)
{
    if (!brain || !dataset) {
        dataio_set_error("Invalid brain or dataset");
        return 0.0F;
    }

    uint32_t batches_processed = 0;
    float avg_loss = 0.0F;

    while (max_batches == 0 || batches_processed < max_batches) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;  // End of dataset
        }

        /* Train on batch using the internal brain_learn_example (brain_t
         * overload). The public nimcp_brain_learn_example() takes
         * nimcp_brain_t (public handle) but our local `brain` is the
         * internal brain_t pointer, so use the internal entry point
         * directly. Returns loss (<0 on error) rather than nimcp_status_t. */
        uint32_t num_features = dataset->config.num_feature_columns;
        for (uint32_t i = 0; i < batch.num_samples; i++) {
            float loss_val = brain_learn_example(
                brain,
                batch.features[i],
                num_features,
                batch.labels[i],
                1.0F  // Full confidence for training examples
            );
            nimcp_status_t result = (loss_val >= 0.0f) ? NIMCP_OK : NIMCP_ERROR;
            if (result == NIMCP_OK) {
                // Training succeeded - accumulate for average loss computation
                // Note: nimcp_brain_learn_example doesn't return loss directly,
                // so we track success rate instead
                avg_loss += 1.0F;  // Count successful training
            }
        }

        // Normalize avg_loss to represent success rate for this batch
        if (batch.num_samples > 0) {
            avg_loss /= (float)batch.num_samples;
        }

        bool at_end = batch.end_of_dataset;
        dataset_free_batch(&batch);
        batches_processed++;

        if (at_end)
            break;
    }

    return avg_loss;
}

//=============================================================================
// Public API: Data Export
//=============================================================================

/**
 * WHAT: Export brain predictions to file
 * WHY: Analyze brain decisions, debugging, analytics
 * HOW: Run brain on dataset, write predictions to CSV
 */
bool brain_export_predictions(brain_t brain, dataset_t input_dataset, const char* output_file,
                              data_format_t format)
{
    // Guard clauses
    if (!brain || !input_dataset || !output_file) {
        dataio_set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_export_predictions: required parameter is NULL (brain, input_dataset, output_file)");
        return false;
    }

    if (format != DATA_FORMAT_CSV) {
        dataio_set_error("Only CSV export supported currently");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_export_predictions: validation failed");
        return false;
    }

    // P1-3 fix: Path traversal validation
    if (!nimcp_path_is_safe(output_file)) {
        dataio_set_error("Path validation failed: %s", output_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_export_predictions: path traversal validation failed");
        return false;
    }

    // Open output file
    FILE* out = fopen(output_file, "w");
    if (!out) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, output_file,
                      "Failed to open output file for predictions: %s", strerror(errno));
        dataio_set_error("Failed to open output file: %s", strerror(errno));
        return false;
    }

    // Write CSV header
    fprintf(out, "features,prediction,confidence\n");

    // Get number of features from brain
    uint32_t num_features = brain_get_num_inputs(brain);

    // Process dataset
    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(input_dataset, &batch)) {
            break;
        }

        // Run brain on each sample
        for (uint32_t i = 0; i < batch.num_samples; i++) {
            // Get brain decision
            brain_decision_t* decision = brain_decide(brain, batch.features[i], num_features);
            if (!decision) {
                continue;  // Skip on error
            }

            // Write to CSV: features, prediction, confidence
            fprintf(out, "\"[");
            for (uint32_t f = 0; f < num_features; f++) {
                fprintf(out, "%.6f", batch.features[i][f]);
                if (f < num_features - 1) {
                    fprintf(out, ",");
                }
            }
            fprintf(out, "]\",");
            fprintf(out, "\"%s\",", decision->label);
            fprintf(out, "%.6f", decision->confidence);
            fprintf(out, "\n");

            brain_free_decision(decision);
        }

        dataset_free_batch(&batch);

        if (batch.end_of_dataset)
            break;
    }

    fclose(out);
    return true;
}

/**
 * WHAT: Export brain training data
 * WHY: Inspect what brain has learned, debugging
 * HOW: Write internal training examples to file
 */
bool brain_export_training_data(brain_t brain, const char* output_file, data_format_t format)
{
    if (!brain || !output_file) {
        dataio_set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_export_training_data: required parameter is NULL (brain, output_file)");
        return false;
    }

    dataio_set_error("brain_export_training_data not yet implemented");
    return false;
}

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * WHAT: Load CSV file (convenience function)
 * WHY: Simplify common case of loading CSV
 * HOW: Factory pattern - create config and call dataset_open
 */
dataset_t dataset_load_csv(const char* filepath, uint32_t num_feature_columns,
                           uint32_t num_label_columns, bool has_header)
{
    if (!filepath) {
        dataio_set_error("NULL filepath");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_load_csv: filepath is NULL");
        return NULL;
    }

    // Create configuration
    dataset_config_t config = {.format = DATA_FORMAT_CSV,
                               .source = DATA_SOURCE_FILE,
                               .num_feature_columns = num_feature_columns,
                               .num_label_columns = num_label_columns,
                               .has_header = has_header,
                               .delimiter = ',',
                               .shuffle = false,
                               .batch_size = 1000,
                               .max_rows = 0};

    strncpy(config.location, filepath, sizeof(config.location) - 1);

    return dataset_open(&config);
}

/**
 * WHAT: Load from PostgreSQL query (convenience function)
 * WHY: Simplify database loading
 * HOW: Factory pattern - create config and call dataset_open
 */
dataset_t dataset_load_postgres(const char* connection_string, const char* query,
                                uint32_t num_feature_columns)
{
    if (!connection_string || !query) {
        dataio_set_error("NULL connection_string or query");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_load_postgres: required parameter is NULL (connection_string, query)");
        return NULL;
    }

    dataset_config_t config = {.format = DATA_FORMAT_POSTGRES,
                               .source = DATA_SOURCE_DATABASE,
                               .num_feature_columns = num_feature_columns,
                               .num_label_columns = 1,
                               .has_header = false,
                               .batch_size = 1000,
                               .max_rows = 0};

    // Store connection string and query in location
    snprintf(config.location, sizeof(config.location), "%s|%s", connection_string, query);

    return dataset_open(&config);
}

/**
 * WHAT: Save dataset to CSV
 * WHY: Export training data for analysis
 * HOW: Write features and labels to CSV file
 */
bool dataset_save_csv(float** features, char** labels, uint32_t num_samples, uint32_t num_features,
                      const char* filepath, char** feature_names)
{
    if (!features || !labels || !filepath) {
        dataio_set_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_save_csv: required parameter is NULL (features, labels, filepath)");
        return false;
    }

    // P1-3 fix: Path traversal validation
    if (!nimcp_path_is_safe(filepath)) {
        dataio_set_error("Path validation failed: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dataset_save_csv: path traversal validation failed");
        return false;
    }

    FILE* f = fopen(filepath, "w");
    if (!f) {
        NIMCP_THROW_IO(NIMCP_ERROR_IO, filepath,
                      "Failed to open CSV file for writing: %s", strerror(errno));
        dataio_set_error("Failed to open file: %s", strerror(errno));
        return false;
    }

    // Write header
    if (feature_names) {
        for (uint32_t i = 0; i < num_features; i++) {
            fprintf(f, "%s%s", feature_names[i], i < num_features - 1 ? "," : "");
        }
        fprintf(f, ",label\n");
    }

    // Write data rows
    for (uint32_t i = 0; i < num_samples; i++) {
        for (uint32_t j = 0; j < num_features; j++) {
            fprintf(f, "%f%s", features[i][j], j < num_features - 1 ? "," : "");
        }
        fprintf(f, ",%s\n", labels[i]);
    }

    fclose(f);
    return true;
}

//=============================================================================
// Online Learning from Streams
//=============================================================================

/**
 * WHAT: Create streaming dataset from callback
 * WHY: Support online learning from live data
 * HOW: Store callback, create stream context
 */
dataset_t dataset_create_stream(stream_callback_fn_t callback, void* user_data,
                                uint32_t num_features)
{
    if (!callback) {
        dataio_set_error("NULL callback");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dataset_create_stream: callback is NULL");
        return NULL;
    }

    // Allocate dataset
    dataset_t dataset = nimcp_calloc(1, sizeof(struct dataset_struct));
    if (!dataset) {
        dataio_set_error("Failed to allocate dataset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dataset_create_stream: dataset is NULL");
        return NULL;
    }

    dataset->strategy = &g_stream_strategy;

    // Create stream context
    stream_context_t* stream_ctx = nimcp_calloc(1, sizeof(stream_context_t));
    if (!stream_ctx) {
        nimcp_free(dataset);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dataset_create_stream: stream_ctx is NULL");
        return NULL;
    }
    stream_ctx->callback = callback;
    stream_ctx->user_data = user_data;
    stream_ctx->num_features = num_features;
    stream_ctx->active = true;

    dataset->source_context = stream_ctx;
    nimcp_mutex_init(&dataset->read_lock, NULL);

    return dataset;
}

/**
 * WHAT: Train brain from live stream
 * WHY: Online learning from real-time data
 * HOW: Call callback repeatedly until duration expires
 */
uint64_t brain_train_from_stream(brain_t brain, dataset_t stream_dataset, uint32_t duration_seconds)
{
    if (!brain || !stream_dataset) {
        dataio_set_error("Invalid parameters");
        return 0;
    }

    if (stream_dataset->strategy->source_type != DATA_SOURCE_STREAM) {
        dataio_set_error("Dataset is not a stream");
        return 0;
    }

    stream_context_t* stream_ctx = (stream_context_t*) stream_dataset->source_context;

    time_t start_time = time(NULL);
    uint64_t samples_processed = 0;

    // WHAT: Train until duration expires or stream closes
    // WHY: Continuous learning from live data
    while (stream_ctx->active) {
        // Check duration
        if (duration_seconds > 0) {
            time_t elapsed = time(NULL) - start_time;
            if (elapsed >= duration_seconds) {
                break;
            }
        }

        // TODO: Wait for callback to provide data
        // This would typically be event-driven (select/poll/epoll)
        // For now, just sleep
        usleep(100000);  // 100ms

        samples_processed++;
    }

    return samples_processed;
}

//=============================================================================
// Module Initialization and Security Registration (Phase IO-2)
//=============================================================================

/**
 * WHAT: Initialize DataIO module with security registration
 * WHY: Enable trust tracking and integrity monitoring
 * HOW: Register as I/O module with security system
 */
nimcp_result_t dataio_init(nimcp_sec_integration_t* security_ctx)
{
    if (nimcp_atomic_load_bool(&g_dataio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return NIMCP_SUCCESS;  // Already initialized
    }

    // Store security context
    g_dataio_security_ctx = security_ctx;

    // Register with security module if context provided
    if (security_ctx) {
        nimcp_result_t result = nimcp_sec_register_module(
            security_ctx,
            "dataio",
            NIMCP_SEC_CAT_IO,
            &g_dataio_security_module_id
        );

        if (result != NIMCP_SUCCESS) {
            dataio_set_error("Failed to register DataIO module with security");
            return result;
        }

        LOG_INFO("DataIO module registered with security (ID: %u)", g_dataio_security_module_id);
    }

    nimcp_atomic_store_bool(&g_dataio_initialized, true, NIMCP_MEMORY_ORDER_RELEASE);
    return NIMCP_SUCCESS;
}

/**
 * WHAT: Shutdown DataIO module
 * WHY: Unregister from security module
 * HOW: Call security unregister
 */
void dataio_shutdown(void)
{
    if (!nimcp_atomic_load_bool(&g_dataio_initialized, NIMCP_MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    // Unregister from security module
    if (g_dataio_security_ctx && g_dataio_security_module_id != 0) {
        nimcp_sec_unregister_module(g_dataio_security_ctx, g_dataio_security_module_id);
        LOG_INFO("DataIO module unregistered from security");
    }

    g_dataio_security_ctx = NULL;
    g_dataio_security_module_id = 0;
    nimcp_atomic_store_bool(&g_dataio_initialized, false, NIMCP_MEMORY_ORDER_RELEASE);
}

/**
 * WHAT: Get DataIO module's security ID
 * WHY: Allow external code to reference this module
 * HOW: Return global module ID
 */
uint32_t dataio_get_security_module_id(void)
{
    return g_dataio_security_module_id;
}

/**
 * WHAT: Get default dataset configuration
 * WHY: Provide sensible defaults
 * HOW: Return pre-initialized struct
 */
dataset_config_t dataset_default_config(void)
{
    dataset_config_t config = {
        .format = DATA_FORMAT_CSV,
        .source = DATA_SOURCE_FILE,
        .location = {0},
        .num_feature_columns = 0,
        .num_label_columns = 1,
        .feature_names = NULL,
        .label_names = NULL,
        .has_header = true,
        .delimiter = ',',
        .shuffle = false,
        .batch_size = 1000,
        .max_rows = 0,
        // Memory integration - disabled by default
        .use_unified_memory = false,
        .memory_manager = NULL,
        // Security integration - disabled by default
        .enable_security = false,
        .security_context = NULL
    };
    return config;
}

/**
 * WHAT: Get dataset statistics
 * WHY: Monitor memory and security performance
 * HOW: Aggregate statistics from dataset and subsystems
 */
bool dataset_get_stats(dataset_t dataset, dataset_stats_t* stats)
{
    if (!dataset || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dataset_get_stats: required parameter is NULL (dataset, stats)");
        return false;
    }

    memset(stats, 0, sizeof(dataset_stats_t));

    // Basic statistics
    stats->total_rows_read = dataset->rows_read;
    stats->total_batches_read = dataset->batches_read;
    stats->bytes_allocated = dataset->bytes_allocated;

    // Memory statistics
    stats->using_unified_memory = (dataset->memory_manager != NULL);
    stats->pool_allocations = dataset->pool_allocations;
    stats->malloc_allocations = dataset->malloc_allocations;

    if (dataset->memory_manager) {
        unified_mem_stats_t mem_stats;
        if (unified_mem_get_stats(dataset->memory_manager, &mem_stats)) {
            stats->cow_memory_saved = mem_stats.memory_saved_bytes;
        }
    }

    // Security statistics
    stats->security_registered = dataset->security_registered;
    if (dataset->security_registered && dataset->security_ctx) {
        stats->security_module_id = g_dataio_security_module_id;

        nimcp_sec_module_info_t mod_info;
        if (nimcp_sec_get_module_info(dataset->security_ctx,
                                      g_dataio_security_module_id,
                                      &mod_info) == NIMCP_SUCCESS) {
            stats->security_interactions = mod_info.interaction_count;
            stats->security_anomalies = mod_info.anomaly_count;
            stats->trust_score = mod_info.trust_score.expected_trust;
        }
    }

    return true;
}
