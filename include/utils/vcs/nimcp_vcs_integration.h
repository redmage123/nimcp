/**
 * @file nimcp_vcs_integration.h
 * @brief Version Control System Integration for Source Code Modification
 *
 * WHAT: Git integration for committing autonomous code fixes to source
 * WHY:  Persist self-repair fixes permanently to the codebase
 * HOW:  File backup/write, git add/commit, optional branch management
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                       VCS INTEGRATION MODULE                                 │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌────────────────────┐     ┌────────────────────┐     ┌─────────────────┐ │
 * │  │ GENERATED FIX      │     │ FILE OPERATIONS    │     │ GIT OPERATIONS  │ │
 * │  │ - source_file      │────>│ - Create backup    │────>│ - git add       │ │
 * │  │ - fixed_code       │     │ - Write fix        │     │ - git commit    │ │
 * │  │ - line range       │     │ - Verify write     │     │ - git push      │ │
 * │  └────────────────────┘     └────────────────────┘     └─────────────────┘ │
 * │                                                                              │
 * │  ┌────────────────────────────────────────────────────────────────────────┐ │
 * │  │                        ROLLBACK CAPABILITY                              │ │
 * │  │  - Restore from backup file                                            │ │
 * │  │  - git revert if committed                                              │ │
 * │  │  - Track rollback history                                               │ │
 * │  └────────────────────────────────────────────────────────────────────────┘ │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * SAFETY:
 * - Always creates backup before modification
 * - Validation required before commit
 * - Atomic file operations where possible
 * - Full rollback capability
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef NIMCP_VCS_INTEGRATION_H
#define NIMCP_VCS_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/parietal/nimcp_code_generation.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define VCS_VERSION                 "1.0.0"
#define VCS_MAX_PATH                512     /**< Max path length */
#define VCS_MAX_BRANCH_NAME         128     /**< Max branch name length */
#define VCS_MAX_COMMIT_MSG          1024    /**< Max commit message length */
#define VCS_MAX_AUTHOR              256     /**< Max author name length */
#define VCS_BACKUP_SUFFIX           ".nimcp.bak" /**< Backup file suffix */
#define VCS_MAGIC                   0x56435349  /**< 'VCSI' */

//=============================================================================
// VCS Types
//=============================================================================

/**
 * @brief Supported VCS types
 */
typedef enum {
    VCS_TYPE_NONE = 0,              /**< No VCS detected */
    VCS_TYPE_GIT,                   /**< Git repository */
    VCS_TYPE_AUTO                   /**< Auto-detect */
} vcs_type_t;

/**
 * @brief VCS operation result codes
 */
typedef enum {
    VCS_OK = 0,                     /**< Success */
    VCS_ERR_NULL = -1,              /**< NULL parameter */
    VCS_ERR_NOT_REPO = -2,          /**< Not a repository */
    VCS_ERR_FILE_NOT_FOUND = -3,    /**< File not found */
    VCS_ERR_BACKUP_FAILED = -4,     /**< Backup creation failed */
    VCS_ERR_WRITE_FAILED = -5,      /**< File write failed */
    VCS_ERR_GIT_ADD = -6,           /**< git add failed */
    VCS_ERR_GIT_COMMIT = -7,        /**< git commit failed */
    VCS_ERR_GIT_PUSH = -8,          /**< git push failed */
    VCS_ERR_GIT_BRANCH = -9,        /**< Branch operation failed */
    VCS_ERR_ROLLBACK = -10,         /**< Rollback failed */
    VCS_ERR_NO_CHANGES = -11,       /**< No changes to commit */
    VCS_ERR_VALIDATION = -12,       /**< Fix validation failed */
    VCS_ERR_COMMAND = -13,          /**< Shell command failed */
    VCS_ERR_INVALID_STATE = -14     /**< Invalid VCS state */
} vcs_error_t;

/**
 * @brief Commit status
 */
typedef enum {
    COMMIT_STATUS_PENDING = 0,      /**< Not yet committed */
    COMMIT_STATUS_STAGED,           /**< Changes staged */
    COMMIT_STATUS_COMMITTED,        /**< Successfully committed */
    COMMIT_STATUS_PUSHED,           /**< Pushed to remote */
    COMMIT_STATUS_REVERTED,         /**< Commit was reverted */
    COMMIT_STATUS_FAILED            /**< Commit failed */
} commit_status_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief VCS configuration
 */
typedef struct {
    /* Repository settings */
    char repo_path[VCS_MAX_PATH];           /**< Repository root path */
    vcs_type_t vcs_type;                    /**< VCS type (auto-detect if AUTO) */

    /* Branch settings */
    char auto_fix_branch[VCS_MAX_BRANCH_NAME]; /**< Branch for auto-fixes */
    bool create_branch_per_fix;             /**< Create new branch per fix */
    bool checkout_branch_on_fix;            /**< Checkout fix branch automatically */

    /* Commit settings */
    char commit_prefix[64];                 /**< Prefix for commit messages */
    char author_name[VCS_MAX_AUTHOR];       /**< Author name for commits */
    char author_email[VCS_MAX_AUTHOR];      /**< Author email for commits */
    bool sign_commits;                      /**< Sign commits with GPG */

    /* Safety settings */
    bool create_backup;                     /**< Create backup before modification */
    bool require_validation;                /**< Require fix validation before commit */
    bool dry_run;                           /**< Don't actually modify files */

    /* Push settings */
    bool auto_push;                         /**< Automatically push after commit */
    char remote_name[64];                   /**< Remote name (default: origin) */

    /* Logging */
    bool verbose;                           /**< Enable verbose output */
} vcs_config_t;

//=============================================================================
// Commit Record
//=============================================================================

/**
 * @brief Record of a committed fix
 */
typedef struct {
    uint64_t commit_id;                     /**< Internal commit ID */
    uint64_t fix_id;                        /**< Associated fix ID */

    /* Git info */
    char commit_hash[64];                   /**< Git commit SHA */
    char branch_name[VCS_MAX_BRANCH_NAME];  /**< Branch committed to */
    char commit_message[VCS_MAX_COMMIT_MSG]; /**< Commit message */

    /* File info */
    char source_file[VCS_MAX_PATH];         /**< Modified file path */
    char backup_file[VCS_MAX_PATH];         /**< Backup file path */
    uint32_t start_line;                    /**< Start line of modification */
    uint32_t end_line;                      /**< End line of modification */

    /* Status */
    commit_status_t status;                 /**< Current status */
    uint64_t timestamp;                     /**< Commit timestamp */

    /* Rollback */
    bool can_rollback;                      /**< Rollback possible */
    bool rolled_back;                       /**< Has been rolled back */
} vcs_commit_record_t;

//=============================================================================
// Write Operation Request
//=============================================================================

/**
 * @brief Request to write fix to source file
 */
typedef struct {
    const generated_fix_t* fix;             /**< Fix to apply */

    /* Target file */
    char source_file[VCS_MAX_PATH];         /**< File to modify */

    /* Line replacement */
    uint32_t replace_start_line;            /**< First line to replace (1-indexed) */
    uint32_t replace_end_line;              /**< Last line to replace (inclusive) */
    const char* new_content;                /**< New content to insert */

    /* Options */
    bool create_backup;                     /**< Create backup (default: true) */
    bool validate_syntax;                   /**< Validate syntax after write */
} vcs_write_request_t;

/**
 * @brief Result of write operation
 */
typedef struct {
    bool success;                           /**< Operation succeeded */
    vcs_error_t error;                      /**< Error code if failed */
    char error_message[256];                /**< Error description */

    char backup_path[VCS_MAX_PATH];         /**< Path to backup file */
    uint32_t lines_replaced;                /**< Number of lines replaced */
    uint32_t new_line_count;                /**< Total lines after modification */
} vcs_write_result_t;

//=============================================================================
// Commit Request
//=============================================================================

/**
 * @brief Request to commit changes
 */
typedef struct {
    uint64_t fix_id;                        /**< Fix being committed */
    char file_path[VCS_MAX_PATH];           /**< File to commit */

    /* Commit info */
    char message[VCS_MAX_COMMIT_MSG];       /**< Commit message (auto-generated if empty) */
    char branch[VCS_MAX_BRANCH_NAME];       /**< Target branch (current if empty) */

    /* Options */
    bool create_branch;                     /**< Create branch if doesn't exist */
    bool push_after_commit;                 /**< Push to remote after commit */
} vcs_commit_request_t;

/**
 * @brief Result of commit operation
 */
typedef struct {
    bool success;                           /**< Operation succeeded */
    vcs_error_t error;                      /**< Error code if failed */
    char error_message[256];                /**< Error description */

    char commit_hash[64];                   /**< Git commit SHA */
    char branch[VCS_MAX_BRANCH_NAME];       /**< Branch committed to */
    bool pushed;                            /**< Was pushed to remote */
} vcs_commit_result_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief VCS operation statistics
 */
typedef struct {
    uint64_t files_modified;                /**< Files modified */
    uint64_t backups_created;               /**< Backup files created */
    uint64_t commits_made;                  /**< Successful commits */
    uint64_t commits_pushed;                /**< Commits pushed */
    uint64_t rollbacks_performed;           /**< Rollbacks performed */
    uint64_t write_failures;                /**< Write operation failures */
    uint64_t commit_failures;               /**< Commit failures */
} vcs_stats_t;

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct vcs_integration vcs_integration_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default VCS configuration
 *
 * @return Default configuration
 */
vcs_config_t vcs_default_config(void);

/**
 * @brief Create VCS integration instance
 *
 * WHAT: Initialize VCS integration with repository
 * WHY:  Entry point for source code modification
 * HOW:  Detect VCS type, validate repository
 *
 * @param config Configuration (NULL for defaults)
 * @return VCS integration handle or NULL on failure
 */
vcs_integration_t* vcs_create(const vcs_config_t* config);

/**
 * @brief Destroy VCS integration instance
 *
 * @param vcs VCS integration handle (NULL safe)
 */
void vcs_destroy(vcs_integration_t* vcs);

/**
 * @brief Check if VCS integration is ready
 *
 * @param vcs VCS integration handle
 * @return true if ready for operations
 */
bool vcs_is_ready(const vcs_integration_t* vcs);

/**
 * @brief Detect VCS type for directory
 *
 * @param path Directory path
 * @return Detected VCS type
 */
vcs_type_t vcs_detect_type(const char* path);

//=============================================================================
// File Write Functions
//=============================================================================

/**
 * @brief Write fix to source file
 *
 * WHAT: Apply generated fix to source file
 * WHY:  Persist fix to source code
 * HOW:  Create backup, replace lines, verify
 *
 * @param vcs VCS integration handle
 * @param request Write request
 * @param result Output result
 * @return VCS_OK on success
 */
int vcs_write_fix(
    vcs_integration_t* vcs,
    const vcs_write_request_t* request,
    vcs_write_result_t* result
);

/**
 * @brief Create backup of source file
 *
 * WHAT: Create backup copy of file before modification
 * WHY:  Enable rollback if fix causes problems
 * HOW:  Copy file to backup location
 *
 * @param vcs VCS integration handle
 * @param file_path File to backup
 * @param backup_path Output: path to backup file
 * @param backup_path_size Size of backup_path buffer
 * @return VCS_OK on success
 */
int vcs_create_backup(
    vcs_integration_t* vcs,
    const char* file_path,
    char* backup_path,
    size_t backup_path_size
);

/**
 * @brief Restore file from backup
 *
 * WHAT: Restore original file from backup
 * WHY:  Undo fix that caused problems
 * HOW:  Copy backup over modified file
 *
 * @param vcs VCS integration handle
 * @param backup_path Path to backup file
 * @param original_path Original file path
 * @return VCS_OK on success
 */
int vcs_restore_from_backup(
    vcs_integration_t* vcs,
    const char* backup_path,
    const char* original_path
);

//=============================================================================
// Git Operations
//=============================================================================

/**
 * @brief Stage file for commit
 *
 * WHAT: Run git add on file
 * WHY:  Prepare file for commit
 * HOW:  Execute git add command
 *
 * @param vcs VCS integration handle
 * @param file_path File to stage
 * @return VCS_OK on success
 */
int vcs_git_add(vcs_integration_t* vcs, const char* file_path);

/**
 * @brief Commit staged changes
 *
 * WHAT: Create git commit with staged changes
 * WHY:  Persist fix to repository history
 * HOW:  Execute git commit command
 *
 * @param vcs VCS integration handle
 * @param request Commit request
 * @param result Output result
 * @return VCS_OK on success
 */
int vcs_git_commit(
    vcs_integration_t* vcs,
    const vcs_commit_request_t* request,
    vcs_commit_result_t* result
);

/**
 * @brief Push commits to remote
 *
 * WHAT: Push commits to remote repository
 * WHY:  Share fix with team/deployment
 * HOW:  Execute git push command
 *
 * @param vcs VCS integration handle
 * @param remote Remote name (NULL for default)
 * @param branch Branch name (NULL for current)
 * @return VCS_OK on success
 */
int vcs_git_push(
    vcs_integration_t* vcs,
    const char* remote,
    const char* branch
);

/**
 * @brief Revert a commit
 *
 * WHAT: Create revert commit for previous commit
 * WHY:  Undo committed fix
 * HOW:  Execute git revert command
 *
 * @param vcs VCS integration handle
 * @param commit_hash Commit hash to revert
 * @param result Output result
 * @return VCS_OK on success
 */
int vcs_git_revert(
    vcs_integration_t* vcs,
    const char* commit_hash,
    vcs_commit_result_t* result
);

//=============================================================================
// Branch Operations
//=============================================================================

/**
 * @brief Create new branch
 *
 * @param vcs VCS integration handle
 * @param branch_name Branch name to create
 * @param checkout Whether to checkout new branch
 * @return VCS_OK on success
 */
int vcs_create_branch(
    vcs_integration_t* vcs,
    const char* branch_name,
    bool checkout
);

/**
 * @brief Checkout existing branch
 *
 * @param vcs VCS integration handle
 * @param branch_name Branch to checkout
 * @return VCS_OK on success
 */
int vcs_checkout_branch(vcs_integration_t* vcs, const char* branch_name);

/**
 * @brief Get current branch name
 *
 * @param vcs VCS integration handle
 * @param branch_out Output buffer for branch name
 * @param branch_out_size Size of output buffer
 * @return VCS_OK on success
 */
int vcs_get_current_branch(
    vcs_integration_t* vcs,
    char* branch_out,
    size_t branch_out_size
);

//=============================================================================
// High-Level Operations
//=============================================================================

/**
 * @brief Apply fix and commit in one operation
 *
 * WHAT: Write fix to file and commit to repository
 * WHY:  Streamlined fix deployment
 * HOW:  Write → validate → add → commit
 *
 * @param vcs VCS integration handle
 * @param fix Generated fix to apply
 * @param record Output commit record
 * @return VCS_OK on success
 */
int vcs_apply_and_commit(
    vcs_integration_t* vcs,
    const generated_fix_t* fix,
    vcs_commit_record_t* record
);

/**
 * @brief Rollback a committed fix
 *
 * WHAT: Undo a previously committed fix
 * WHY:  Fix caused regression or other issues
 * HOW:  Restore from backup or git revert
 *
 * @param vcs VCS integration handle
 * @param record Commit record to rollback
 * @return VCS_OK on success
 */
int vcs_rollback(
    vcs_integration_t* vcs,
    vcs_commit_record_t* record
);

/**
 * @brief Generate commit message for fix
 *
 * WHAT: Create standardized commit message
 * WHY:  Consistent, informative commit history
 * HOW:  Template with fix details
 *
 * @param vcs VCS integration handle
 * @param fix Fix being committed
 * @param message Output buffer
 * @param message_size Size of output buffer
 * @return VCS_OK on success
 */
int vcs_generate_commit_message(
    vcs_integration_t* vcs,
    const generated_fix_t* fix,
    char* message,
    size_t message_size
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get commit record by ID
 *
 * @param vcs VCS integration handle
 * @param commit_id Commit ID
 * @return Commit record or NULL if not found
 */
const vcs_commit_record_t* vcs_get_commit_record(
    vcs_integration_t* vcs,
    uint64_t commit_id
);

/**
 * @brief Get VCS statistics
 *
 * @param vcs VCS integration handle
 * @param stats Output statistics
 * @return VCS_OK on success
 */
int vcs_get_stats(const vcs_integration_t* vcs, vcs_stats_t* stats);

/**
 * @brief Check if file has uncommitted changes
 *
 * @param vcs VCS integration handle
 * @param file_path File to check
 * @return true if file has uncommitted changes
 */
bool vcs_has_uncommitted_changes(vcs_integration_t* vcs, const char* file_path);

/**
 * @brief Check if repository is clean
 *
 * @param vcs VCS integration handle
 * @return true if no uncommitted changes
 */
bool vcs_is_repo_clean(vcs_integration_t* vcs);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get VCS error message
 *
 * @param error Error code
 * @return Error message string (static)
 */
const char* vcs_strerror(vcs_error_t error);

/**
 * @brief Get VCS type name
 *
 * @param type VCS type
 * @return Type name string (static)
 */
const char* vcs_type_name(vcs_type_t type);

/**
 * @brief Get commit status name
 *
 * @param status Commit status
 * @return Status name string (static)
 */
const char* vcs_commit_status_name(commit_status_t status);

/**
 * @brief Get VCS integration version
 *
 * @return Version string
 */
const char* vcs_version(void);

//=============================================================================
// Bio-Async Communication Functions
//=============================================================================

/**
 * @brief Broadcast commit completion via bio-async
 *
 * WHAT: Notify other modules about VCS commit completion
 * WHY:  Enable cross-module coordination during repair
 * HOW:  Send message via bio-router to self-repair coordinator
 *
 * @param vcs VCS integration handle
 * @param fix_id Fix ID
 * @param commit_hash Git commit hash (can be NULL if failed)
 * @param success True if commit succeeded
 * @return 0 on success, -1 on error
 */
int vcs_broadcast_commit(
    vcs_integration_t* vcs,
    uint64_t fix_id,
    const char* commit_hash,
    bool success
);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process incoming bio-async messages
 * WHY:  Handle requests from other modules
 * HOW:  Call bio_router_process_inbox
 *
 * @param vcs VCS integration handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t vcs_process_messages(
    vcs_integration_t* vcs,
    uint32_t max_messages
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VCS_INTEGRATION_H */
