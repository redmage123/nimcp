/**
 * @file nimcp_graphql_utils.c
 * @brief Lightweight GraphQL-Inspired Query Interface Implementation
 *
 * WHAT: Implementation of GraphQL-style query interface for GPU graphs
 * WHY:  Provides declarative query language for graph operations
 * HOW:  Query parsing, filter evaluation, executor pattern
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/graphql/nimcp_graphql_utils.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(graphql_utils)

//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_TOKEN_LEN 64
#define MAX_FILTER_DEPTH 16

//=============================================================================
// Query Type Mapping
//=============================================================================

static const char* g_query_type_strings[] = {
    "vertices",
    "edges",
    "neighbors",
    "path",
    "subgraph",
    "centrality",
    "clustering"
};

static const char* g_filter_field_strings[] = {
    "degree",
    "weight",
    "centrality",
    "clustering",
    "distance",
    "id",
    "feature"
};

static const char* g_filter_op_strings[] = {
    "==",
    "!=",
    "<",
    "<=",
    ">",
    ">=",
    "AND",
    "OR",
    "NOT"
};

//=============================================================================
// Forward Declarations (Internal)
//=============================================================================

static void* executor_default_execute(nimcp_graphql_executor_t* self,
                                       nimcp_graph_query_t* query);
static nimcp_graphql_filter_node_t* executor_default_parse_filter(
    nimcp_graphql_executor_t* self, const char* filter);
static bool executor_default_evaluate_filter(nimcp_graphql_executor_t* self,
                                              const nimcp_graphql_filter_node_t* filter,
                                              int vertex_id);

static nimcp_graphql_filter_field_t parse_field_name(const char* name);
static nimcp_graphql_filter_op_t parse_operator(const char* op_str);
static const char* skip_whitespace(const char* str);
static bool parse_comparison(const char** str, nimcp_graphql_filter_node_t* node);

//=============================================================================
// Executor Lifecycle
//=============================================================================

nimcp_graphql_executor_t* nimcp_graphql_executor_create(
    nimcp_gpu_context_t* gpu_context)
{
    if (!gpu_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_context is NULL");

        return NULL;
    }

    nimcp_graphql_executor_t* exec = (nimcp_graphql_executor_t*)nimcp_calloc(
        1, sizeof(nimcp_graphql_executor_t));
    if (!exec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exec is NULL");

        return NULL;
    }

    // Set default function pointers
    exec->execute = executor_default_execute;
    exec->parse_filter = executor_default_parse_filter;
    exec->evaluate_filter = executor_default_evaluate_filter;

    exec->gpu_ctx = gpu_context;
    exec->graph = NULL;
    exec->current_filter = NULL;

    return exec;
}

void nimcp_graphql_executor_destroy(nimcp_graphql_executor_t* exec)
{
    if (!exec) {
        return;
    }

    if (exec->current_filter) {
        nimcp_graphql_filter_destroy(exec->current_filter);
    }

    nimcp_free(exec);
}

nimcp_error_t nimcp_graphql_executor_set_graph(
    nimcp_graphql_executor_t* exec,
    void* graph)
{
    NIMCP_CHECK_THROW(exec, NIMCP_ERROR_NULL_POINTER, "exec is NULL");

    exec->graph = graph;
    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Execution
//=============================================================================

nimcp_error_t nimcp_graphql_execute(
    nimcp_graphql_executor_t* exec,
    const char* query_string,
    void** result)
{
    NIMCP_CHECK_THROW(exec, NIMCP_ERROR_NULL_POINTER, "exec is NULL");
    NIMCP_CHECK_THROW(query_string, NIMCP_ERROR_NULL_POINTER, "query_string is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    *result = NULL;

    // Create query structure from string
    nimcp_graph_query_t* query = nimcp_graph_query_create();
    if (!query) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Parse query string - simplified parser
    // Format: "{ <type>(<params>) [where <filter>] }"
    const char* ptr = skip_whitespace(query_string);

    // Skip opening brace if present
    if (*ptr == '{') {
        ptr = skip_whitespace(ptr + 1);
    }

    // Parse query type
    char type_buf[NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN];
    size_t type_len = 0;
    while (*ptr && *ptr != '(' && !isspace((unsigned char)*ptr) &&
           type_len < NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN - 1) {
        type_buf[type_len++] = *ptr++;
    }
    type_buf[type_len] = '\0';

    nimcp_error_t err = nimcp_graph_query_set_type(query, type_buf);
    if (err != NIMCP_SUCCESS) {
        nimcp_graph_query_destroy(query);
        return err;
    }

    // Parse parameters (simplified - just look for 'where')
    const char* where_pos = strstr(ptr, "where");
    if (where_pos) {
        where_pos = skip_whitespace(where_pos + 5);  // Skip "where"

        // Extract filter expression until closing brace or end
        char filter_buf[NIMCP_GRAPHQL_MAX_FILTER_LEN];
        size_t filter_len = 0;
        while (*where_pos && *where_pos != '}' &&
               filter_len < NIMCP_GRAPHQL_MAX_FILTER_LEN - 1) {
            filter_buf[filter_len++] = *where_pos++;
        }
        filter_buf[filter_len] = '\0';

        // Trim trailing whitespace
        while (filter_len > 0 && isspace((unsigned char)filter_buf[filter_len - 1])) {
            filter_buf[--filter_len] = '\0';
        }

        if (filter_len > 0) {
            err = nimcp_graph_query_set_filter(query, filter_buf);
            if (err != NIMCP_SUCCESS) {
                nimcp_graph_query_destroy(query);
                return err;
            }
        }
    }

    // Execute the query
    err = nimcp_graphql_execute_query(exec, query);
    if (err == NIMCP_SUCCESS && query->result) {
        *result = query->result;
        query->result = NULL;  // Transfer ownership
    }

    nimcp_graph_query_destroy(query);
    return err;
}

nimcp_error_t nimcp_graphql_execute_query(
    nimcp_graphql_executor_t* exec,
    nimcp_graph_query_t* query)
{
    NIMCP_CHECK_THROW(exec, NIMCP_ERROR_NULL_POINTER, "exec is NULL");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NULL_POINTER, "query is NULL");

    if (!exec->graph) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Execute using the executor's strategy
    void* result = exec->execute(exec, query);
    if (!result) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    // Store result in query
    if (query->result) {
        nimcp_graphql_result_destroy(query->result);
    }
    query->result = (nimcp_graphql_result_t*)result;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Default Executor Implementation
//=============================================================================

static void* executor_default_execute(nimcp_graphql_executor_t* self,
                                       nimcp_graph_query_t* query)
{
    if (!self || !query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executor_default_execute: required parameter is NULL (self, query)");
        return NULL;
    }

    // Parse filter if present
    nimcp_graphql_filter_node_t* filter = NULL;
    if (query->filter_expression[0] != '\0') {
        filter = self->parse_filter(self, query->filter_expression);
        // Filter is optional, continue even if parsing fails
    }

    // Create result structure
    nimcp_graphql_result_t* result = (nimcp_graphql_result_t*)nimcp_calloc(
        1, sizeof(nimcp_graphql_result_t));
    if (!result) {
        if (filter) nimcp_graphql_filter_destroy(filter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executor_default_execute: validation failed");
        return NULL;
    }

    // Determine query type
    nimcp_graphql_query_type_t query_type = nimcp_graphql_query_type_from_string(
        query->query_type);
    result->query_type = query_type;

    // Execute based on query type
    // Note: Actual GPU kernel execution would be done here via graph DAO
    // For now, we create placeholder results
    switch (query_type) {
        case NIMCP_GRAPHQL_QUERY_VERTICES:
        case NIMCP_GRAPHQL_QUERY_NEIGHBORS: {
            // Return vertex IDs as placeholder
            if (query->num_vertices > 0) {
                result->data = nimcp_malloc(query->num_vertices * sizeof(int));
                if (result->data) {
                    memcpy(result->data, query->vertex_ids,
                           query->num_vertices * sizeof(int));
                    result->num_elements = query->num_vertices;
                    result->element_size = sizeof(int);
                }
            }
            break;
        }

        case NIMCP_GRAPHQL_QUERY_EDGES:
        case NIMCP_GRAPHQL_QUERY_CENTRALITY:
        case NIMCP_GRAPHQL_QUERY_CLUSTERING: {
            // Return float scores as placeholder
            result->data = nimcp_calloc(1, sizeof(float));
            if (result->data) {
                result->num_elements = 1;
                result->element_size = sizeof(float);
            }
            break;
        }

        case NIMCP_GRAPHQL_QUERY_PATH:
        case NIMCP_GRAPHQL_QUERY_SUBGRAPH:
        default:
            // Empty result for unsupported queries
            result->num_elements = 0;
            result->element_size = 0;
            break;
    }

    result->on_device = false;

    if (filter) {
        nimcp_graphql_filter_destroy(filter);
    }

    return result;
}

static nimcp_graphql_filter_node_t* executor_default_parse_filter(
    nimcp_graphql_executor_t* self, const char* filter)
{
    (void)self;  // Unused in default implementation
    return nimcp_graphql_parse_filter(filter);
}

static bool executor_default_evaluate_filter(nimcp_graphql_executor_t* self,
                                              const nimcp_graphql_filter_node_t* filter,
                                              int vertex_id)
{
    (void)self;
    (void)vertex_id;

    if (!filter) {
        return true;  // No filter means accept all
    }

    // Default evaluation - would use graph data in real implementation
    return nimcp_graphql_filter_evaluate(filter, 0, 0.0f, 0.0f, 0.0f, 0.0f, NULL, 0);
}

//=============================================================================
// Query Builder API
//=============================================================================

nimcp_graph_query_t* nimcp_graph_query_create(void)
{
    nimcp_graph_query_t* query = (nimcp_graph_query_t*)nimcp_calloc(
        1, sizeof(nimcp_graph_query_t));
    if (!query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "query is NULL");

        return NULL;
    }

    query->depth = 1;  // Default depth
    return query;
}

void nimcp_graph_query_destroy(nimcp_graph_query_t* query)
{
    if (!query) {
        return;
    }

    if (query->vertex_ids) {
        nimcp_free(query->vertex_ids);
    }

    if (query->result) {
        nimcp_graphql_result_destroy(query->result);
    }

    nimcp_free(query);
}

nimcp_error_t nimcp_graph_query_set_type(
    nimcp_graph_query_t* query,
    const char* type)
{
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NULL_POINTER, "query is NULL");
    NIMCP_CHECK_THROW(type, NIMCP_ERROR_NULL_POINTER, "type is NULL");

    size_t len = strlen(type);
    if (len >= NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    strncpy(query->query_type, type, NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN - 1);
    query->query_type[NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN - 1] = '\0';

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_graph_query_set_filter(
    nimcp_graph_query_t* query,
    const char* filter)
{
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NULL_POINTER, "query is NULL");
    NIMCP_CHECK_THROW(filter, NIMCP_ERROR_NULL_POINTER, "filter is NULL");

    size_t len = strlen(filter);
    if (len >= NIMCP_GRAPHQL_MAX_FILTER_LEN) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    strncpy(query->filter_expression, filter, NIMCP_GRAPHQL_MAX_FILTER_LEN - 1);
    query->filter_expression[NIMCP_GRAPHQL_MAX_FILTER_LEN - 1] = '\0';

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_graph_query_set_vertices(
    nimcp_graph_query_t* query,
    const int* vertex_ids,
    size_t num_vertices)
{
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NULL_POINTER, "query is NULL");

    if (num_vertices > NIMCP_GRAPHQL_MAX_VERTICES) {
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    // Free existing vertices
    if (query->vertex_ids) {
        nimcp_free(query->vertex_ids);
        query->vertex_ids = NULL;
    }

    if (num_vertices > 0 && vertex_ids) {
        query->vertex_ids = (int*)nimcp_malloc(num_vertices * sizeof(int));
        if (!query->vertex_ids) {
            return NIMCP_ERROR_NO_MEMORY;
        }
        memcpy(query->vertex_ids, vertex_ids, num_vertices * sizeof(int));
    }

    query->num_vertices = num_vertices;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_graph_query_set_depth(
    nimcp_graph_query_t* query,
    int depth)
{
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NULL_POINTER, "query is NULL");

    if (depth < 0 || depth > NIMCP_GRAPHQL_MAX_DEPTH) {
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    query->depth = depth;
    return NIMCP_SUCCESS;
}

//=============================================================================
// Filter Expression Parsing
//=============================================================================

static const char* skip_whitespace(const char* str)
{
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

static nimcp_graphql_filter_field_t parse_field_name(const char* name)
{
    for (int i = 0; i < NIMCP_GRAPHQL_FIELD_COUNT; i++) {
        if (strncmp(name, g_filter_field_strings[i],
                    strlen(g_filter_field_strings[i])) == 0) {
            return (nimcp_graphql_filter_field_t)i;
        }
    }
    return NIMCP_GRAPHQL_FIELD_ID;  // Default
}

static nimcp_graphql_filter_op_t parse_operator(const char* op_str)
{
    // Check two-character operators first
    if (strncmp(op_str, "==", 2) == 0) return NIMCP_GRAPHQL_OP_EQ;
    if (strncmp(op_str, "!=", 2) == 0) return NIMCP_GRAPHQL_OP_NE;
    if (strncmp(op_str, "<=", 2) == 0) return NIMCP_GRAPHQL_OP_LE;
    if (strncmp(op_str, ">=", 2) == 0) return NIMCP_GRAPHQL_OP_GE;

    // Single character operators
    if (*op_str == '<') return NIMCP_GRAPHQL_OP_LT;
    if (*op_str == '>') return NIMCP_GRAPHQL_OP_GT;
    if (*op_str == '=') return NIMCP_GRAPHQL_OP_EQ;

    return NIMCP_GRAPHQL_OP_EQ;  // Default
}

static bool parse_comparison(const char** str, nimcp_graphql_filter_node_t* node)
{
    const char* ptr = skip_whitespace(*str);

    // Parse field name
    char field_buf[MAX_TOKEN_LEN];
    size_t field_len = 0;
    while (*ptr && isalpha((unsigned char)*ptr) &&
           field_len < MAX_TOKEN_LEN - 1) {
        field_buf[field_len++] = *ptr++;
    }
    field_buf[field_len] = '\0';

    if (field_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parse_comparison: field_len is zero");
        return false;
    }

    node->field = parse_field_name(field_buf);

    // Parse operator
    ptr = skip_whitespace(ptr);
    node->op = parse_operator(ptr);

    // Skip operator characters
    if (*ptr == '!' || *ptr == '=' || *ptr == '<' || *ptr == '>') {
        ptr++;
        if (*ptr == '=') ptr++;  // Handle ==, !=, <=, >=
    }

    // Parse value
    ptr = skip_whitespace(ptr);
    node->value = (float)strtod(ptr, (char**)&ptr);

    *str = ptr;
    return true;
}

nimcp_graphql_filter_node_t* nimcp_graphql_parse_filter(const char* filter_str)
{
    if (!filter_str || !*filter_str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graphql_parse_filter: filter_str is NULL");
        return NULL;
    }

    nimcp_graphql_filter_node_t* node = (nimcp_graphql_filter_node_t*)nimcp_calloc(
        1, sizeof(nimcp_graphql_filter_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    const char* ptr = filter_str;
    if (!parse_comparison(&ptr, node)) {
        nimcp_free(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graphql_parse_filter: parse_comparison is NULL");
        return NULL;
    }

    // Check for logical operators (AND, OR)
    ptr = skip_whitespace(ptr);
    if (strncasecmp(ptr, "AND", 3) == 0 || strncasecmp(ptr, "OR", 2) == 0) {
        nimcp_graphql_filter_op_t logical_op =
            (strncasecmp(ptr, "AND", 3) == 0) ? NIMCP_GRAPHQL_OP_AND : NIMCP_GRAPHQL_OP_OR;

        // Skip operator
        ptr += (logical_op == NIMCP_GRAPHQL_OP_AND) ? 3 : 2;

        // Parse right side recursively
        nimcp_graphql_filter_node_t* right = nimcp_graphql_parse_filter(ptr);
        if (right) {
            // Create logical node
            nimcp_graphql_filter_node_t* logical = nimcp_graphql_filter_create_logical(
                logical_op, node, right);
            if (logical) {
                return logical;
            }
            nimcp_graphql_filter_destroy(right);
        }
    }

    return node;
}

void nimcp_graphql_filter_destroy(nimcp_graphql_filter_node_t* filter)
{
    if (!filter) {
        return;
    }

    if (filter->left) {
        nimcp_graphql_filter_destroy(filter->left);
    }
    if (filter->right) {
        nimcp_graphql_filter_destroy(filter->right);
    }

    nimcp_free(filter);
}

bool nimcp_graphql_filter_evaluate(
    const nimcp_graphql_filter_node_t* filter,
    int degree,
    float weight,
    float centrality,
    float clustering,
    float distance,
    const float* features,
    size_t num_features)
{
    if (!filter) {
        return true;
    }

    // Handle logical operators
    if (filter->op == NIMCP_GRAPHQL_OP_AND) {
        return nimcp_graphql_filter_evaluate(filter->left, degree, weight,
                                             centrality, clustering, distance,
                                             features, num_features) &&
               nimcp_graphql_filter_evaluate(filter->right, degree, weight,
                                             centrality, clustering, distance,
                                             features, num_features);
    }

    if (filter->op == NIMCP_GRAPHQL_OP_OR) {
        return nimcp_graphql_filter_evaluate(filter->left, degree, weight,
                                             centrality, clustering, distance,
                                             features, num_features) ||
               nimcp_graphql_filter_evaluate(filter->right, degree, weight,
                                             centrality, clustering, distance,
                                             features, num_features);
    }

    if (filter->op == NIMCP_GRAPHQL_OP_NOT) {
        return !nimcp_graphql_filter_evaluate(filter->left, degree, weight,
                                              centrality, clustering, distance,
                                              features, num_features);
    }

    // Get field value
    float field_value = 0.0f;
    switch (filter->field) {
        case NIMCP_GRAPHQL_FIELD_DEGREE:
            field_value = (float)degree;
            break;
        case NIMCP_GRAPHQL_FIELD_WEIGHT:
            field_value = weight;
            break;
        case NIMCP_GRAPHQL_FIELD_CENTRALITY:
            field_value = centrality;
            break;
        case NIMCP_GRAPHQL_FIELD_CLUSTERING:
            field_value = clustering;
            break;
        case NIMCP_GRAPHQL_FIELD_DISTANCE:
            field_value = distance;
            break;
        case NIMCP_GRAPHQL_FIELD_FEATURE:
            if (features && filter->feature_index >= 0 &&
                (size_t)filter->feature_index < num_features) {
                field_value = features[filter->feature_index];
            }
            break;
        default:
            field_value = 0.0f;
    }

    // Evaluate comparison
    switch (filter->op) {
        case NIMCP_GRAPHQL_OP_EQ:
            return field_value == filter->value;
        case NIMCP_GRAPHQL_OP_NE:
            return field_value != filter->value;
        case NIMCP_GRAPHQL_OP_LT:
            return field_value < filter->value;
        case NIMCP_GRAPHQL_OP_LE:
            return field_value <= filter->value;
        case NIMCP_GRAPHQL_OP_GT:
            return field_value > filter->value;
        case NIMCP_GRAPHQL_OP_GE:
            return field_value >= filter->value;
        default:
            return true;
    }
}

nimcp_graphql_filter_node_t* nimcp_graphql_filter_create_comparison(
    nimcp_graphql_filter_field_t field,
    nimcp_graphql_filter_op_t op,
    float value)
{
    nimcp_graphql_filter_node_t* node = (nimcp_graphql_filter_node_t*)nimcp_calloc(
        1, sizeof(nimcp_graphql_filter_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    node->field = field;
    node->op = op;
    node->value = value;
    node->feature_index = -1;
    node->left = NULL;
    node->right = NULL;

    return node;
}

nimcp_graphql_filter_node_t* nimcp_graphql_filter_create_logical(
    nimcp_graphql_filter_op_t op,
    nimcp_graphql_filter_node_t* left,
    nimcp_graphql_filter_node_t* right)
{
    if (op != NIMCP_GRAPHQL_OP_AND && op != NIMCP_GRAPHQL_OP_OR &&
        op != NIMCP_GRAPHQL_OP_NOT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_graphql_filter_create_logical: operation failed");
        return NULL;
    }

    nimcp_graphql_filter_node_t* node = (nimcp_graphql_filter_node_t*)nimcp_calloc(
        1, sizeof(nimcp_graphql_filter_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;
    }

    node->op = op;
    node->left = left;
    node->right = right;

    return node;
}

//=============================================================================
// Result Handling
//=============================================================================

void nimcp_graphql_result_destroy(nimcp_graphql_result_t* result)
{
    if (!result) {
        return;
    }

    if (result->data) {
        if (result->on_device) {
            // TODO: Use nimcp_gpu_free when integrated with GPU graph
            // For now, assume host memory
        }
        nimcp_free(result->data);
    }

    nimcp_free(result);
}

nimcp_error_t nimcp_graphql_result_to_host(
    const nimcp_graphql_result_t* result,
    void* host_buffer,
    size_t buffer_size)
{
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");
    NIMCP_CHECK_THROW(host_buffer, NIMCP_ERROR_NULL_POINTER, "host_buffer is NULL");

    size_t data_size = result->num_elements * result->element_size;
    if (buffer_size < data_size) {
        return NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    if (!result->data || data_size == 0) {
        return NIMCP_SUCCESS;
    }

    if (result->on_device) {
        // TODO: Use cudaMemcpy when integrated with GPU graph
        return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    memcpy(host_buffer, result->data, data_size);
    return NIMCP_SUCCESS;
}

size_t nimcp_graphql_result_get_ints(
    const nimcp_graphql_result_t* result,
    int* out_array,
    size_t max_elements)
{
    if (!result || !out_array || !result->data) {
        return 0;
    }

    if (result->element_size != sizeof(int)) {
        return 0;
    }

    size_t count = (result->num_elements < max_elements) ?
                   result->num_elements : max_elements;

    if (result->on_device) {
        // TODO: GPU memcpy
        return 0;
    }

    memcpy(out_array, result->data, count * sizeof(int));
    return count;
}

size_t nimcp_graphql_result_get_floats(
    const nimcp_graphql_result_t* result,
    float* out_array,
    size_t max_elements)
{
    if (!result || !out_array || !result->data) {
        return 0;
    }

    if (result->element_size != sizeof(float)) {
        return 0;
    }

    size_t count = (result->num_elements < max_elements) ?
                   result->num_elements : max_elements;

    if (result->on_device) {
        // TODO: GPU memcpy
        return 0;
    }

    memcpy(out_array, result->data, count * sizeof(float));
    return count;
}

//=============================================================================
// Utility Functions
//=============================================================================

nimcp_graphql_query_type_t nimcp_graphql_query_type_from_string(const char* type_str)
{
    if (!type_str) {
        return NIMCP_GRAPHQL_QUERY_VERTICES;
    }

    for (int i = 0; i < NIMCP_GRAPHQL_QUERY_COUNT; i++) {
        if (strcasecmp(type_str, g_query_type_strings[i]) == 0) {
            return (nimcp_graphql_query_type_t)i;
        }
    }

    return NIMCP_GRAPHQL_QUERY_VERTICES;  // Default
}

const char* nimcp_graphql_query_type_to_string(nimcp_graphql_query_type_t type)
{
    if (type >= 0 && type < NIMCP_GRAPHQL_QUERY_COUNT) {
        return g_query_type_strings[type];
    }
    return "unknown";
}

bool nimcp_graphql_validate_query(
    const char* query_string,
    char* error_msg,
    size_t error_msg_size)
{
    if (!query_string || !*query_string) {
        if (error_msg && error_msg_size > 0) {
            snprintf(error_msg, error_msg_size, "Empty query string");
        }
        return false;
    }

    // Check for balanced braces
    int brace_count = 0;
    for (const char* p = query_string; *p; p++) {
        if (*p == '{') brace_count++;
        if (*p == '}') brace_count--;
        if (brace_count < 0) {
            if (error_msg && error_msg_size > 0) {
                snprintf(error_msg, error_msg_size, "Unbalanced braces");
            }
            return false;
        }
    }

    if (brace_count != 0) {
        if (error_msg && error_msg_size > 0) {
            snprintf(error_msg, error_msg_size, "Unbalanced braces");
        }
        return false;
    }

    // Check for valid query type
    const char* ptr = skip_whitespace(query_string);
    if (*ptr == '{') {
        ptr = skip_whitespace(ptr + 1);
    }

    char type_buf[NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN];
    size_t type_len = 0;
    while (*ptr && *ptr != '(' && !isspace((unsigned char)*ptr) &&
           type_len < NIMCP_GRAPHQL_MAX_QUERY_TYPE_LEN - 1) {
        type_buf[type_len++] = *ptr++;
    }
    type_buf[type_len] = '\0';

    bool found = false;
    for (int i = 0; i < NIMCP_GRAPHQL_QUERY_COUNT; i++) {
        if (strcasecmp(type_buf, g_query_type_strings[i]) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        if (error_msg && error_msg_size > 0) {
            snprintf(error_msg, error_msg_size, "Unknown query type: %s", type_buf);
        }
        return false;
    }

    return true;
}
