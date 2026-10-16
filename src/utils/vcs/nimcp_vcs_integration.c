/**
 * @file nimcp_vcs_integration.c
 * @brief VCS Integration Implementation
 *
 * WHAT: Implementation of git integration for source code modification
 * WHY:  Persist self-repair fixes to codebase with proper version control
 * HOW:  File backup/write, git CLI commands, commit tracking
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "utils/vcs/nimcp_vcs_integration.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(vcs_integration)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief VCS integration internal state
 */
struct vcs_integration {
    uint32_t magic;                     /**< Validation magic */
    vcs_config_t config;                /**< Configuration */

    /* Repository info */
    vcs_type_t detected_type;           /**< Detected VCS type */
    char repo_root[VCS_MAX_PATH];       /**< Repository root directory */
    char current_branch[VCS_MAX_BRANCH_NAME]; /**< Current branch */

    /* Commit tracking */
    vcs_commit_record_t* commits;
    uint32_t commit_count;
    uint32_t commit_capacity;
    uint64_t next_commit_id;

    /* Statistics */
    vcs_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Bio-async communication */
    bio_module_context_t bio_ctx;

    /* State */
    bool ready;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static int execute_git_command(vcs_integration_t* vcs, const char* cmd, char* output, size_t output_size);
static int detect_repo_root(vcs_integration_t* vcs);
static uint64_t get_timestamp_ms(void);
static int copy_file(const char* src, const char* dst);
static int read_file_lines(const char* path, char*** lines, uint32_t* line_count);
static int write_file_lines(const char* path, char** lines, uint32_t line_count);
static void free_file_lines(char** lines, uint32_t line_count);
static nimcp_error_t vcs_handle_bio_message(const void* msg, size_t msg_size,
                                            nimcp_bio_promise_t response_promise,
                                            void* user_data);
static int register_vcs_bio_handlers(vcs_integration_t* vcs);

//=============================================================================
// Lifecycle Functions
//=============================================================================

vcs_config_t vcs_default_config(void) {
    vcs_config_t config = {0};

    config.vcs_type = VCS_TYPE_AUTO;
    strncpy(config.auto_fix_branch, "auto-fix", sizeof(config.auto_fix_branch) - 1);
    config.create_branch_per_fix = false;
    config.checkout_branch_on_fix = false;

    strncpy(config.commit_prefix, "fix(auto): ", sizeof(config.commit_prefix) - 1);
    strncpy(config.author_name, "NIMCP Self-Repair", sizeof(config.author_name) - 1);
    strncpy(config.author_email, "nimcp-autofix@localhost", sizeof(config.author_email) - 1);
    config.sign_commits = false;

    config.create_backup = true;
    config.require_validation = true;
    config.dry_run = false;

    config.auto_push = false;
    strncpy(config.remote_name, "origin", sizeof(config.remote_name) - 1);

    config.verbose = false;

    return config;
}

vcs_integration_t* vcs_create(const vcs_config_t* config) {
    vcs_integration_t* vcs = nimcp_calloc(1, sizeof(vcs_integration_t));
    if (!vcs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vcs is NULL");

        return NULL;
    }

    vcs->magic = VCS_MAGIC;

    /* Apply configuration */
    if (config) {
        vcs->config = *config;
    } else {
        vcs->config = vcs_default_config();
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    vcs->mutex = nimcp_mutex_create(&attr);
    if (!vcs->mutex) {
        nimcp_free(vcs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vcs_create: vcs->mutex is NULL");
        return NULL;
    }

    /* Allocate commit tracking */
    vcs->commit_capacity = 256;
    vcs->commits = nimcp_calloc(vcs->commit_capacity, sizeof(vcs_commit_record_t));
    if (!vcs->commits) {
        nimcp_mutex_free(vcs->mutex);
        nimcp_free(vcs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vcs_create: vcs->commits is NULL");
        return NULL;
    }

    /* Detect VCS type */
    if (vcs->config.vcs_type == VCS_TYPE_AUTO) {
        const char* path = vcs->config.repo_path[0] ? vcs->config.repo_path : ".";
        vcs->detected_type = vcs_detect_type(path);
    } else {
        vcs->detected_type = vcs->config.vcs_type;
    }

    /* Find repository root */
    if (vcs->detected_type == VCS_TYPE_GIT) {
        if (detect_repo_root(vcs) != 0) {
            /* Not a fatal error - might be working with files outside repo */
            vcs->detected_type = VCS_TYPE_NONE;
        }
    }

    /* Get current branch */
    if (vcs->detected_type == VCS_TYPE_GIT) {
        vcs_get_current_branch(vcs, vcs->current_branch, sizeof(vcs->current_branch));
    }

    vcs->next_commit_id = 1;

    /* Register with bio-async router */
    if (bio_router_is_initialized()) {
        bio_module_info_t info = {0};
        info.module_id = BIO_MODULE_VCS_INTEGRATION;
        info.module_name = "vcs_integration";
        info.inbox_capacity = 16;
        info.user_data = vcs;
        vcs->bio_ctx = bio_router_register_module(&info);
        if (vcs->bio_ctx) {
            register_vcs_bio_handlers(vcs);
        }
    }

    vcs->ready = true;

    return vcs;
}

void vcs_destroy(vcs_integration_t* vcs) {
    if (!vcs) {
        return;
    }

    if (vcs->magic != VCS_MAGIC) {
        return;
    }

    vcs->ready = false;
    vcs->magic = 0;

    /* Unregister from bio-async router */
    if (vcs->bio_ctx) {
        bio_router_unregister_module(vcs->bio_ctx);
        vcs->bio_ctx = NULL;
    }

    if (vcs->commits) {
        nimcp_free(vcs->commits);
    }
    if (vcs->mutex) {
        nimcp_mutex_free(vcs->mutex);
    }

    nimcp_free(vcs);
}

bool vcs_is_ready(const vcs_integration_t* vcs) {
    if (!vcs || vcs->magic != VCS_MAGIC) {
        return false;
    }
    return vcs->ready;
}

vcs_type_t vcs_detect_type(const char* path) {
    if (!path) {
        return VCS_TYPE_NONE;
    }

    /* Check for .git directory */
    char git_path[VCS_MAX_PATH];
    snprintf(git_path, sizeof(git_path), "%s/.git", path);

    struct stat st;
    if (stat(git_path, &st) == 0) {
        return VCS_TYPE_GIT;
    }

    /* Check parent directories */
    char parent[VCS_MAX_PATH];
    strncpy(parent, path, sizeof(parent) - 1);

    char* last_slash = strrchr(parent, '/');
    while (last_slash && last_slash != parent) {
        *last_slash = '\0';
        /* Cap source to VCS_MAX_PATH - 6 (/.git + null) so the
         * destination can't overflow. */
        snprintf(git_path, sizeof(git_path), "%.506s/.git", parent);
        if (stat(git_path, &st) == 0) {
            return VCS_TYPE_GIT;
        }
        last_slash = strrchr(parent, '/');
    }

    return VCS_TYPE_NONE;
}

//=============================================================================
// File Write Functions
//=============================================================================

int vcs_write_fix(
    vcs_integration_t* vcs,
    const vcs_write_request_t* request,
    vcs_write_result_t* result
) {
    if (!vcs || !request || !result) {
        return VCS_ERR_NULL;
    }

    memset(result, 0, sizeof(*result));

    if (vcs->config.dry_run) {
        result->success = true;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Dry run - no changes made");
        return VCS_OK;
    }

    nimcp_mutex_lock(vcs->mutex);

    /* Check file exists. error_message is 256 bytes; source_file can be
     * up to VCS_MAX_PATH (512). Cap the path so "File not found: <path>"
     * (16 + path + null) fits in 256 → path limit 239. */
    struct stat st;
    if (stat(request->source_file, &st) != 0) {
        result->error = VCS_ERR_FILE_NOT_FOUND;
        snprintf(result->error_message, sizeof(result->error_message),
                 "File not found: %.239s", request->source_file);
        nimcp_mutex_unlock(vcs->mutex);
        return VCS_ERR_FILE_NOT_FOUND;
    }

    /* Create backup if requested */
    if (request->create_backup || vcs->config.create_backup) {
        int ret = vcs_create_backup(vcs, request->source_file,
                                    result->backup_path, sizeof(result->backup_path));
        if (ret != VCS_OK) {
            result->error = VCS_ERR_BACKUP_FAILED;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Failed to create backup");
            nimcp_mutex_unlock(vcs->mutex);
            return VCS_ERR_BACKUP_FAILED;
        }
        vcs->stats.backups_created++;
    }

    /* Read existing file. Same 239-char path cap as above. */
    char** lines = NULL;
    uint32_t line_count = 0;
    if (read_file_lines(request->source_file, &lines, &line_count) != 0) {
        result->error = VCS_ERR_WRITE_FAILED;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to read file: %.234s", request->source_file);
        nimcp_mutex_unlock(vcs->mutex);
        return VCS_ERR_WRITE_FAILED;
    }

    /* Calculate new content */
    uint32_t start_line = request->replace_start_line > 0 ? request->replace_start_line - 1 : 0;
    uint32_t end_line = request->replace_end_line > 0 ? request->replace_end_line - 1 : start_line;

    if (start_line >= line_count) {
        start_line = line_count > 0 ? line_count - 1 : 0;
    }
    if (end_line >= line_count) {
        end_line = line_count > 0 ? line_count - 1 : 0;
    }

    /* Count lines in new content */
    uint32_t new_content_lines = 0;
    if (request->new_content) {
        new_content_lines = 1;
        for (const char* p = request->new_content; *p; p++) {
            if (*p == '\n') new_content_lines++;
        }
    }

    /* Build new file content */
    uint32_t lines_to_remove = end_line - start_line + 1;
    uint32_t new_line_count = line_count - lines_to_remove + new_content_lines;

    char** new_lines = nimcp_calloc(new_line_count + 1, sizeof(char*));
    if (!new_lines) {
        free_file_lines(lines, line_count);
        result->error = VCS_ERR_WRITE_FAILED;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Memory allocation failed");
        nimcp_mutex_unlock(vcs->mutex);
        return VCS_ERR_WRITE_FAILED;
    }

    /* Copy lines before replacement */
    uint32_t out_idx = 0;
    for (uint32_t i = 0; i < start_line && i < line_count; i++) {
        new_lines[out_idx++] = strdup(lines[i]);
    }

    /* Insert new content */
    if (request->new_content) {
        const char* content = request->new_content;
        const char* line_start = content;
        for (const char* p = content; ; p++) {
            if (*p == '\n' || *p == '\0') {
                size_t len = p - line_start;
                /* Use malloc (not nimcp_malloc) to match strdup allocations */
                /* so free_file_lines can use nimcp_free() uniformly */
                new_lines[out_idx] = nimcp_malloc(len + 2);
                if (new_lines[out_idx]) {
                    strncpy(new_lines[out_idx], line_start, len);
                    new_lines[out_idx][len] = '\n';
                    new_lines[out_idx][len + 1] = '\0';
                }
                out_idx++;
                if (*p == '\0') break;
                line_start = p + 1;
            }
        }
    }

    /* Copy lines after replacement */
    for (uint32_t i = end_line + 1; i < line_count; i++) {
        new_lines[out_idx++] = strdup(lines[i]);
    }

    /* Write new file */
    if (write_file_lines(request->source_file, new_lines, out_idx) != 0) {
        free_file_lines(new_lines, out_idx);
        free_file_lines(lines, line_count);
        result->error = VCS_ERR_WRITE_FAILED;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to write file: %.233s", request->source_file);
        nimcp_mutex_unlock(vcs->mutex);
        return VCS_ERR_WRITE_FAILED;
    }

    /* Cleanup */
    free_file_lines(new_lines, out_idx);
    free_file_lines(lines, line_count);

    result->success = true;
    result->lines_replaced = lines_to_remove;
    result->new_line_count = out_idx;
    vcs->stats.files_modified++;

    nimcp_mutex_unlock(vcs->mutex);
    return VCS_OK;
}

int vcs_create_backup(
    vcs_integration_t* vcs,
    const char* file_path,
    char* backup_path,
    size_t backup_path_size
) {
    if (!vcs || !file_path || !backup_path) {
        return VCS_ERR_NULL;
    }

    /* Generate backup path */
    snprintf(backup_path, backup_path_size, "%s%s", file_path, VCS_BACKUP_SUFFIX);

    /* Copy file */
    if (copy_file(file_path, backup_path) != 0) {
        return VCS_ERR_BACKUP_FAILED;
    }

    return VCS_OK;
}

int vcs_restore_from_backup(
    vcs_integration_t* vcs,
    const char* backup_path,
    const char* original_path
) {
    if (!vcs || !backup_path || !original_path) {
        return VCS_ERR_NULL;
    }

    if (copy_file(backup_path, original_path) != 0) {
        return VCS_ERR_ROLLBACK;
    }

    return VCS_OK;
}

//=============================================================================
// Git Operations
//=============================================================================

int vcs_git_add(vcs_integration_t* vcs, const char* file_path) {
    if (!vcs || !file_path) {
        return VCS_ERR_NULL;
    }

    if (vcs->detected_type != VCS_TYPE_GIT) {
        return VCS_ERR_NOT_REPO;
    }

    char cmd[VCS_MAX_PATH + 64];
    snprintf(cmd, sizeof(cmd), "git add \"%s\"", file_path);

    if (execute_git_command(vcs, cmd, NULL, 0) != 0) {
        return VCS_ERR_GIT_ADD;
    }

    return VCS_OK;
}

int vcs_git_commit(
    vcs_integration_t* vcs,
    const vcs_commit_request_t* request,
    vcs_commit_result_t* result
) {
    if (!vcs || !request || !result) {
        return VCS_ERR_NULL;
    }

    memset(result, 0, sizeof(*result));

    if (vcs->detected_type != VCS_TYPE_GIT) {
        result->error = VCS_ERR_NOT_REPO;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Not a git repository");
        return VCS_ERR_NOT_REPO;
    }

    nimcp_mutex_lock(vcs->mutex);

    /* Build commit command. cmd must be large enough to hold
     *   git commit --author="<256> <<256>>" -m "<1024>"
     * plus fixed chars. Worst case ≈ 1600 bytes. Size cmd generously. */
    char cmd[VCS_MAX_COMMIT_MSG + (VCS_MAX_AUTHOR * 2) + 128];
    char message[VCS_MAX_COMMIT_MSG];

    if (request->message[0]) {
        strncpy(message, request->message, sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
    } else {
        /* Auto-generate message. commit_prefix[64] + literal(19) +
         * file_path up to 512 = ~595 bytes, fits in 1024. */
        snprintf(message, sizeof(message),
                 "%.60sAutonomous fix for %.500s",
                 vcs->config.commit_prefix,
                 request->file_path[0] ? request->file_path : "code issue");
    }

    /* Build command with author if set. Field-level caps keep the
     * formatted total under cmd's capacity. */
    if (vcs->config.author_name[0] && vcs->config.author_email[0]) {
        snprintf(cmd, sizeof(cmd),
                 "git commit --author=\"%.255s <%.255s>\" -m \"%.1020s\"",
                 vcs->config.author_name,
                 vcs->config.author_email,
                 message);
    } else {
        snprintf(cmd, sizeof(cmd), "git commit -m \"%.1020s\"", message);
    }

    char output[NIMCP_LOG_BUFFER_SIZE];
    if (execute_git_command(vcs, cmd, output, sizeof(output)) != 0) {
        result->error = VCS_ERR_GIT_COMMIT;
        snprintf(result->error_message, sizeof(result->error_message),
                 "git commit failed");
        nimcp_mutex_unlock(vcs->mutex);
        return VCS_ERR_GIT_COMMIT;
    }

    /* Get commit hash */
    if (execute_git_command(vcs, "git rev-parse HEAD", result->commit_hash, sizeof(result->commit_hash)) == 0) {
        /* Trim newline */
        char* newline = strchr(result->commit_hash, '\n');
        if (newline) *newline = '\0';
    }

    /* Get branch name */
    vcs_get_current_branch(vcs, result->branch, sizeof(result->branch));

    /* Push if requested */
    if (request->push_after_commit || vcs->config.auto_push) {
        if (vcs_git_push(vcs, NULL, NULL) == VCS_OK) {
            result->pushed = true;
            vcs->stats.commits_pushed++;
        }
    }

    /* Record commit */
    if (vcs->commit_count < vcs->commit_capacity) {
        vcs_commit_record_t* record = &vcs->commits[vcs->commit_count++];
        record->commit_id = vcs->next_commit_id++;
        record->fix_id = request->fix_id;
        strncpy(record->commit_hash, result->commit_hash, sizeof(record->commit_hash) - 1);
        strncpy(record->branch_name, result->branch, sizeof(record->branch_name) - 1);
        strncpy(record->commit_message, message, sizeof(record->commit_message) - 1);
        strncpy(record->source_file, request->file_path, sizeof(record->source_file) - 1);
        record->status = result->pushed ? COMMIT_STATUS_PUSHED : COMMIT_STATUS_COMMITTED;
        record->timestamp = get_timestamp_ms();
        record->can_rollback = true;
    }

    result->success = true;
    vcs->stats.commits_made++;

    nimcp_mutex_unlock(vcs->mutex);
    return VCS_OK;
}

int vcs_git_push(
    vcs_integration_t* vcs,
    const char* remote,
    const char* branch
) {
    if (!vcs) {
        return VCS_ERR_NULL;
    }

    if (vcs->detected_type != VCS_TYPE_GIT) {
        return VCS_ERR_NOT_REPO;
    }

    const char* remote_name = remote ? remote : vcs->config.remote_name;
    char cmd[VCS_MAX_PATH];

    if (branch) {
        snprintf(cmd, sizeof(cmd), "git push %s %s", remote_name, branch);
    } else {
        snprintf(cmd, sizeof(cmd), "git push %s", remote_name);
    }

    if (execute_git_command(vcs, cmd, NULL, 0) != 0) {
        return VCS_ERR_GIT_PUSH;
    }

    return VCS_OK;
}

int vcs_git_revert(
    vcs_integration_t* vcs,
    const char* commit_hash,
    vcs_commit_result_t* result
) {
    if (!vcs || !commit_hash || !result) {
        return VCS_ERR_NULL;
    }

    memset(result, 0, sizeof(*result));

    if (vcs->detected_type != VCS_TYPE_GIT) {
        result->error = VCS_ERR_NOT_REPO;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Not a git repository");
        return VCS_ERR_NOT_REPO;
    }

    char cmd[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "git revert --no-edit %s", commit_hash);

    if (execute_git_command(vcs, cmd, NULL, 0) != 0) {
        result->error = VCS_ERR_ROLLBACK;
        snprintf(result->error_message, sizeof(result->error_message),
                 "git revert failed");
        return VCS_ERR_ROLLBACK;
    }

    /* Get new commit hash */
    if (execute_git_command(vcs, "git rev-parse HEAD", result->commit_hash, sizeof(result->commit_hash)) == 0) {
        char* newline = strchr(result->commit_hash, '\n');
        if (newline) *newline = '\0';
    }

    result->success = true;
    vcs->stats.rollbacks_performed++;

    return VCS_OK;
}

//=============================================================================
// Branch Operations
//=============================================================================

int vcs_create_branch(
    vcs_integration_t* vcs,
    const char* branch_name,
    bool checkout
) {
    if (!vcs || !branch_name) {
        return VCS_ERR_NULL;
    }

    if (vcs->detected_type != VCS_TYPE_GIT) {
        return VCS_ERR_NOT_REPO;
    }

    char cmd[VCS_MAX_PATH];
    if (checkout) {
        snprintf(cmd, sizeof(cmd), "git checkout -b %s", branch_name);
    } else {
        snprintf(cmd, sizeof(cmd), "git branch %s", branch_name);
    }

    if (execute_git_command(vcs, cmd, NULL, 0) != 0) {
        return VCS_ERR_GIT_BRANCH;
    }

    if (checkout) {
        strncpy(vcs->current_branch, branch_name, sizeof(vcs->current_branch) - 1);
    }

    return VCS_OK;
}

int vcs_checkout_branch(vcs_integration_t* vcs, const char* branch_name) {
    if (!vcs || !branch_name) {
        return VCS_ERR_NULL;
    }

    if (vcs->detected_type != VCS_TYPE_GIT) {
        return VCS_ERR_NOT_REPO;
    }

    char cmd[VCS_MAX_PATH];
    snprintf(cmd, sizeof(cmd), "git checkout %s", branch_name);

    if (execute_git_command(vcs, cmd, NULL, 0) != 0) {
        return VCS_ERR_GIT_BRANCH;
    }

    strncpy(vcs->current_branch, branch_name, sizeof(vcs->current_branch) - 1);
    return VCS_OK;
}

int vcs_get_current_branch(
    vcs_integration_t* vcs,
    char* branch_out,
    size_t branch_out_size
) {
    if (!vcs || !branch_out) {
        return VCS_ERR_NULL;
    }

    if (vcs->detected_type != VCS_TYPE_GIT) {
        strncpy(branch_out, "", branch_out_size);
        return VCS_ERR_NOT_REPO;
    }

    if (execute_git_command(vcs, "git rev-parse --abbrev-ref HEAD", branch_out, branch_out_size) != 0) {
        return VCS_ERR_COMMAND;
    }

    /* Trim newline */
    char* newline = strchr(branch_out, '\n');
    if (newline) *newline = '\0';

    return VCS_OK;
}

//=============================================================================
// High-Level Operations
//=============================================================================

int vcs_apply_and_commit(
    vcs_integration_t* vcs,
    const generated_fix_t* fix,
    vcs_commit_record_t* record
) {
    if (!vcs || !fix || !record) {
        return VCS_ERR_NULL;
    }

    memset(record, 0, sizeof(*record));

    /* Prepare write request */
    vcs_write_request_t write_req = {0};
    write_req.fix = fix;
    strncpy(write_req.source_file, fix->source_file, sizeof(write_req.source_file) - 1);
    write_req.replace_start_line = fix->start_line;
    write_req.replace_end_line = fix->end_line;
    write_req.new_content = fix->fixed_code;
    write_req.create_backup = true;

    /* Write fix to file */
    vcs_write_result_t write_result;
    int ret = vcs_write_fix(vcs, &write_req, &write_result);
    if (ret != VCS_OK) {
        vcs->stats.write_failures++;
        return ret;
    }

    strncpy(record->backup_file, write_result.backup_path, sizeof(record->backup_file) - 1);

    /* Stage file */
    ret = vcs_git_add(vcs, fix->source_file);
    if (ret != VCS_OK) {
        /* Restore from backup */
        vcs_restore_from_backup(vcs, write_result.backup_path, fix->source_file);
        return ret;
    }

    /* Commit */
    vcs_commit_request_t commit_req = {0};
    commit_req.fix_id = fix->fix_id;
    strncpy(commit_req.file_path, fix->source_file, sizeof(commit_req.file_path) - 1);

    /* Generate commit message */
    vcs_generate_commit_message(vcs, fix, commit_req.message, sizeof(commit_req.message));

    vcs_commit_result_t commit_result;
    ret = vcs_git_commit(vcs, &commit_req, &commit_result);
    if (ret != VCS_OK) {
        /* Restore from backup */
        vcs_restore_from_backup(vcs, write_result.backup_path, fix->source_file);
        vcs->stats.commit_failures++;
        return ret;
    }

    /* Fill record */
    record->commit_id = vcs->next_commit_id - 1;
    record->fix_id = fix->fix_id;
    strncpy(record->commit_hash, commit_result.commit_hash, sizeof(record->commit_hash) - 1);
    strncpy(record->branch_name, commit_result.branch, sizeof(record->branch_name) - 1);
    strncpy(record->commit_message, commit_req.message, sizeof(record->commit_message) - 1);
    strncpy(record->source_file, fix->source_file, sizeof(record->source_file) - 1);
    record->start_line = fix->start_line;
    record->end_line = fix->end_line;
    record->status = commit_result.pushed ? COMMIT_STATUS_PUSHED : COMMIT_STATUS_COMMITTED;
    record->timestamp = get_timestamp_ms();
    record->can_rollback = true;

    return VCS_OK;
}

int vcs_rollback(
    vcs_integration_t* vcs,
    vcs_commit_record_t* record
) {
    if (!vcs || !record) {
        return VCS_ERR_NULL;
    }

    if (!record->can_rollback) {
        return VCS_ERR_ROLLBACK;
    }

    int ret = VCS_OK;

    /* Try backup first */
    if (record->backup_file[0]) {
        ret = vcs_restore_from_backup(vcs, record->backup_file, record->source_file);
        if (ret == VCS_OK) {
            /* Stage the restored file */
            vcs_git_add(vcs, record->source_file);

            /* Commit the restoration */
            vcs_commit_request_t commit_req = {0};
            commit_req.fix_id = record->fix_id;
            strncpy(commit_req.file_path, record->source_file, sizeof(commit_req.file_path) - 1);
            snprintf(commit_req.message, sizeof(commit_req.message),
                     "%sRollback fix %lu", vcs->config.commit_prefix, (unsigned long)record->fix_id);

            vcs_commit_result_t commit_result;
            vcs_git_commit(vcs, &commit_req, &commit_result);
        }
    }

    /* If backup failed or not available, try git revert */
    if (ret != VCS_OK && record->commit_hash[0]) {
        vcs_commit_result_t revert_result;
        ret = vcs_git_revert(vcs, record->commit_hash, &revert_result);
    }

    if (ret == VCS_OK) {
        record->status = COMMIT_STATUS_REVERTED;
        record->rolled_back = true;
        record->can_rollback = false;
    }

    return ret;
}

int vcs_generate_commit_message(
    vcs_integration_t* vcs,
    const generated_fix_t* fix,
    char* message,
    size_t message_size
) {
    if (!vcs || !fix || !message) {
        return VCS_ERR_NULL;
    }

    const char* strategy = code_gen_strategy_name(fix->strategy);
    const char* prefix = vcs->config.commit_prefix;

    snprintf(message, message_size,
             "%s%s in %s:%u\n\n"
             "Root cause: %s\n"
             "Fix confidence: %.0f%%\n"
             "Risk score: %.0f%%\n\n"
             "%s\n\n"
             "Auto-generated by NIMCP Self-Repair",
             prefix,
             strategy,
             fix->function_name[0] ? fix->function_name : "unknown",
             fix->start_line,
             fix->root_cause,
             fix->confidence * 100,
             fix->risk_score * 100,
             fix->explanation);

    return VCS_OK;
}

//=============================================================================
// Query Functions
//=============================================================================

const vcs_commit_record_t* vcs_get_commit_record(
    vcs_integration_t* vcs,
    uint64_t commit_id
) {
    if (!vcs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vcs is NULL");

        return NULL;
    }

    nimcp_mutex_lock(vcs->mutex);

    for (uint32_t i = 0; i < vcs->commit_count; i++) {
        if (vcs->commits[i].commit_id == commit_id) {
            nimcp_mutex_unlock(vcs->mutex);
            return &vcs->commits[i];
        }
    }

    nimcp_mutex_unlock(vcs->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vcs_get_commit_record: validation failed");
    return NULL;
}

int vcs_get_stats(const vcs_integration_t* vcs, vcs_stats_t* stats) {
    if (!vcs || !stats) {
        return VCS_ERR_NULL;
    }

    nimcp_mutex_lock(((vcs_integration_t*)vcs)->mutex);
    *stats = vcs->stats;
    nimcp_mutex_unlock(((vcs_integration_t*)vcs)->mutex);

    return VCS_OK;
}

bool vcs_has_uncommitted_changes(vcs_integration_t* vcs, const char* file_path) {
    if (!vcs || !file_path) {
        return false;
    }

    if (vcs->detected_type != VCS_TYPE_GIT) {
        return false;
    }

    char cmd[VCS_MAX_PATH + 64];
    char output[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "git status --porcelain \"%s\"", file_path);

    if (execute_git_command(vcs, cmd, output, sizeof(output)) != 0) {
        return false;
    }

    /* Non-empty output means there are changes */
    return output[0] != '\0';
}

bool vcs_is_repo_clean(vcs_integration_t* vcs) {
    if (!vcs) {
        return true;
    }

    if (vcs->detected_type != VCS_TYPE_GIT) {
        return true;
    }

    char output[NIMCP_ERROR_BUFFER_SIZE];
    if (execute_git_command(vcs, "git status --porcelain", output, sizeof(output)) != 0) {
        return false;
    }

    return output[0] == '\0';
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* vcs_strerror(vcs_error_t error) {
    switch (error) {
        case VCS_OK:                return "Success";
        case VCS_ERR_NULL:          return "NULL parameter";
        case VCS_ERR_NOT_REPO:      return "Not a repository";
        case VCS_ERR_FILE_NOT_FOUND: return "File not found";
        case VCS_ERR_BACKUP_FAILED: return "Backup failed";
        case VCS_ERR_WRITE_FAILED:  return "Write failed";
        case VCS_ERR_GIT_ADD:       return "git add failed";
        case VCS_ERR_GIT_COMMIT:    return "git commit failed";
        case VCS_ERR_GIT_PUSH:      return "git push failed";
        case VCS_ERR_GIT_BRANCH:    return "Branch operation failed";
        case VCS_ERR_ROLLBACK:      return "Rollback failed";
        case VCS_ERR_NO_CHANGES:    return "No changes to commit";
        case VCS_ERR_VALIDATION:    return "Validation failed";
        case VCS_ERR_COMMAND:       return "Command failed";
        case VCS_ERR_INVALID_STATE: return "Invalid state";
        default:                    return "Unknown error";
    }
}

const char* vcs_type_name(vcs_type_t type) {
    switch (type) {
        case VCS_TYPE_NONE: return "none";
        case VCS_TYPE_GIT:  return "git";
        case VCS_TYPE_AUTO: return "auto";
        default:            return "unknown";
    }
}

const char* vcs_commit_status_name(commit_status_t status) {
    switch (status) {
        case COMMIT_STATUS_PENDING:   return "pending";
        case COMMIT_STATUS_STAGED:    return "staged";
        case COMMIT_STATUS_COMMITTED: return "committed";
        case COMMIT_STATUS_PUSHED:    return "pushed";
        case COMMIT_STATUS_REVERTED:  return "reverted";
        case COMMIT_STATUS_FAILED:    return "failed";
        default:                      return "unknown";
    }
}

const char* vcs_version(void) {
    return VCS_VERSION;
}

//=============================================================================
// Internal Functions
//=============================================================================

static int execute_git_command(vcs_integration_t* vcs, const char* cmd, char* output, size_t output_size) {
    char full_cmd[VCS_MAX_PATH * 2];

    /* Execute in repo root if available */
    if (vcs->repo_root[0]) {
        snprintf(full_cmd, sizeof(full_cmd), "cd \"%s\" && %s 2>&1", vcs->repo_root, cmd);
    } else {
        snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", cmd);
    }

    FILE* fp = popen(full_cmd, "r");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fp is NULL");

        return -1;
    }

    if (output && output_size > 0) {
        output[0] = '\0';
        size_t total = 0;
        char buf[NIMCP_ERROR_BUFFER_SIZE];
        while (fgets(buf, sizeof(buf), fp) && total < output_size - 1) {
            size_t len = strlen(buf);
            if (total + len >= output_size) {
                len = output_size - total - 1;
            }
            strncat(output, buf, len);
            total += len;
        }
    } else {
        /* Drain output */
        char buf[NIMCP_ERROR_BUFFER_SIZE];
        while (fgets(buf, sizeof(buf), fp)) {}
    }

    int status = pclose(fp);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int detect_repo_root(vcs_integration_t* vcs) {
    char output[VCS_MAX_PATH];
    if (execute_git_command(vcs, "git rev-parse --show-toplevel", output, sizeof(output)) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_repo_root: validation failed");
        return -1;
    }

    /* Trim newline */
    char* newline = strchr(output, '\n');
    if (newline) *newline = '\0';

    strncpy(vcs->repo_root, output, sizeof(vcs->repo_root) - 1);
    return 0;
}

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "in is NULL");

        return -1;
    }

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "copy_file: out is NULL");
        return -1;
    }

    char buf[NIMCP_PATH_BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "copy_file: validation failed");
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

static int read_file_lines(const char* path, char*** lines, uint32_t* line_count) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fp is NULL");

        return -1;
    }

    /* Count lines first */
    uint32_t count = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n') count++;
    }
    if (c != '\n' && count > 0) count++;  /* Handle missing final newline */

    rewind(fp);

    /* Allocate array */
    *lines = nimcp_calloc(count + 1, sizeof(char*));
    if (!*lines) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_file_lines: validation failed");
        return -1;
    }

    /* Read lines */
    char buf[NIMCP_PATH_BUFFER_SIZE];
    uint32_t idx = 0;
    while (fgets(buf, sizeof(buf), fp) && idx < count) {
        (*lines)[idx] = strdup(buf);
        idx++;
    }

    *line_count = idx;
    fclose(fp);
    return 0;
}

static int write_file_lines(const char* path, char** lines, uint32_t line_count) {
    FILE* fp = fopen(path, "w");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fp is NULL");

        return -1;
    }

    for (uint32_t i = 0; i < line_count; i++) {
        if (lines[i]) {
            fputs(lines[i], fp);
        }
    }

    fclose(fp);
    return 0;
}

static void free_file_lines(char** lines, uint32_t line_count) {
    if (!lines) {
        return;
    }

    for (uint32_t i = 0; i < line_count; i++) {
        if (lines[i]) {
            nimcp_free(lines[i]);
        }
    }
    nimcp_free(lines);
}

//=============================================================================
// Bio-Async Communication
//=============================================================================

/**
 * @brief Handle incoming bio-async messages
 */
static nimcp_error_t vcs_handle_bio_message(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    vcs_integration_t* vcs = (vcs_integration_t*)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    (void)response_promise;  /* May be NULL for fire-and-forget messages */
    (void)vcs;  /* Used for future message processing */

    switch (header->type) {
        case BIO_MSG_VCS_WRITE_FIX:
            /* Handle write fix request via bio-async */
            break;

        case BIO_MSG_VCS_COMMIT:
            /* Handle commit request */
            break;

        case BIO_MSG_VCS_ROLLBACK:
            /* Handle rollback request */
            break;

        case BIO_MSG_VCS_STATUS:
            /* Handle status query */
            break;

        default:
            /* Unknown message type for this module */
            return NIMCP_ERROR_UNKNOWN;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Register message handlers for VCS integration module
 */
static int register_vcs_bio_handlers(vcs_integration_t* vcs) {
    if (!vcs || !vcs->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "register_vcs_bio_handlers: required parameter is NULL (vcs, vcs->bio_ctx)");
        return -1;
    }

    /* Register handlers for VCS messages */
    bio_router_register_handler(vcs->bio_ctx, BIO_MSG_VCS_WRITE_FIX,
                                vcs_handle_bio_message);
    bio_router_register_handler(vcs->bio_ctx, BIO_MSG_VCS_COMMIT,
                                vcs_handle_bio_message);
    bio_router_register_handler(vcs->bio_ctx, BIO_MSG_VCS_ROLLBACK,
                                vcs_handle_bio_message);
    bio_router_register_handler(vcs->bio_ctx, BIO_MSG_VCS_STATUS,
                                vcs_handle_bio_message);

    return 0;
}

/**
 * @brief Broadcast commit completion via bio-async
 */
int vcs_broadcast_commit(
    vcs_integration_t* vcs,
    uint64_t fix_id,
    const char* commit_hash,
    bool success
) {
    if (!vcs || !vcs->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vcs_broadcast_commit: required parameter is NULL (vcs, vcs->bio_ctx)");
        return -1;
    }

    /* Build and send commit notification message */
    struct {
        bio_message_header_t header;
        uint64_t fix_id;
        uint8_t success;
        char commit_hash[NIMCP_ID_BUFFER_SIZE];
    } msg = {0};

    msg.header.type = BIO_MSG_VCS_COMMIT;
    msg.header.source_module = BIO_MODULE_VCS_INTEGRATION;
    msg.header.target_module = BIO_MODULE_SELF_REPAIR;  /* Target self-repair coordinator */
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.fix_id = fix_id;
    msg.success = success ? 1 : 0;
    if (commit_hash) {
        strncpy(msg.commit_hash, commit_hash, sizeof(msg.commit_hash) - 1);
    }

    bio_router_send(vcs->bio_ctx, &msg, sizeof(msg), 0);
    return 0;
}

/**
 * @brief Process pending bio-async messages
 */
uint32_t vcs_process_messages(vcs_integration_t* vcs, uint32_t max_messages) {
    if (!vcs || !vcs->bio_ctx) {
        return 0;
    }
    return bio_router_process_inbox(vcs->bio_ctx, max_messages);
}
