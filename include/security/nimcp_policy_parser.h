/**
 * @file nimcp_policy_parser.h
 * @brief NIMCP Policy Parser
 */

#ifndef NIMCP_POLICY_PARSER_H
#define NIMCP_POLICY_PARSER_H

#include "security/nimcp_policy_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse policy text into AST
 *
 * @param input Policy source text
 * @param filename Source filename (for error messages)
 * @param error_msg Output error message (caller must free)
 * @return AST root node or NULL on error
 */
nimcp_ast_node_t* nimcp_policy_parse(
    const char* input,
    const char* filename,
    char** error_msg
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POLICY_PARSER_H */
