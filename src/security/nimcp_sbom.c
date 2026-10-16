/**
 * @file nimcp_sbom.c
 * @brief Software Bill of Materials (SBOM) Generation and Management
 *
 * WHAT: SPDX and CycloneDX SBOM format support
 * WHY: Document software composition for security audits and compliance
 * HOW: Parse/generate SBOM documents, track dependencies
 */

#include "security/nimcp_supply_chain.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/validation/nimcp_common.h"
#include "utils/thread/nimcp_thread.h"

/* Error code aliases for this file */
#ifndef NIMCP_OK
#define NIMCP_OK NIMCP_SUCCESS
#endif
#ifndef NIMCP_ERROR_INVALID_PARAM
#define NIMCP_ERROR_INVALID_PARAM NIMCP_ERROR_INVALID_PARAM
#endif
#ifndef NIMCP_ERROR_IO
#define NIMCP_ERROR_IO (-121)
#endif
#ifndef NIMCP_ERROR_CRYPTO
#define NIMCP_ERROR_CRYPTO (-130)
#endif
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(sbom, MESH_ADAPTER_CATEGORY_SECURITY)


/* Supply chain structure definition */
struct nimcp_supply_chain {
    uint32_t magic;
    nimcp_supply_chain_config_t config;
    nimcp_supply_chain_stats_t stats;
    nimcp_dependency_t* dependencies;
    size_t dependency_count;
    size_t dependency_capacity;
    nimcp_trusted_source_t* sources;
    size_t source_count;
    size_t source_capacity;
    nimcp_thread_t monitor_thread;
    bool monitoring_active;
    nimcp_mutex_t lock;
    bio_module_context_t bio_ctx;
    bool bio_registered;
};

/* ========================================================================
 * SBOM Loading
 * ======================================================================== */

nimcp_error_t nimcp_sbom_load(nimcp_supply_chain_t sc,
                               const char* filepath,
                               nimcp_sbom_format_t format) {
    if (!sc || sc->magic != NIMCP_SUPPLY_CHAIN_MAGIC || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "r");
    if (!file) {
        LOG_ERROR("nimcp_sbom_load: Cannot open file %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "sbom: error condition");
        return NIMCP_ERROR_IO;
    }

    LOG_INFO("Loading SBOM from %s (format=%d)", filepath, format);

    /* Read file content */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)nimcp_malloc(file_size + 1);
    if (!content) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sbom: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    size_t bytes_read = fread(content, 1, file_size, file);
    fclose(file);
    if (bytes_read != (size_t)file_size) {
        nimcp_free(content);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "sbom: incomplete file read");
        return NIMCP_ERROR_IO;
    }
    content[bytes_read] = '\0';

    /*
     * Parse SBOM (simplified - production would use full SPDX/CycloneDX parser)
     * For now, we'll look for dependency patterns
     */

    nimcp_mutex_lock(&sc->lock);

    /* Simple pattern matching for demonstration */
    char* line = strtok(content, "\n");
    while (line != NULL) {
        /* Look for dependency entries (very simplified) */
        if (strstr(line, "PackageName:") || strstr(line, "component")) {
            LOG_DEBUG("Found dependency entry: %s", line);
            /* Would parse full dependency information here */
        }
        line = strtok(NULL, "\n");
    }

    sc->stats.sbom_loads++;
    sc->stats.last_sbom_update = time(NULL);

    nimcp_mutex_unlock(&sc->lock);

    nimcp_free(content);

    LOG_INFO("SBOM loaded successfully");

    return NIMCP_OK;
}

/* ========================================================================
 * SBOM Generation
 * ======================================================================== */

static nimcp_error_t generate_spdx_sbom(nimcp_supply_chain_t sc, char** output) {
    /* Estimate size */
    size_t estimated_size = 4096 + (sc->dependency_count * 1024);
    char* sbom = (char*)nimcp_malloc(estimated_size);
    if (!sbom) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_spdx_sbom: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* SPDX 2.3 header */
    char* ptr = sbom;
    size_t remaining = estimated_size;
    int written;

    written = snprintf(ptr, remaining, "SPDXVersion: SPDX-2.3\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "DataLicense: CC0-1.0\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "SPDXID: SPDXRef-DOCUMENT\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "DocumentName: NIMCP-SBOM\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "DocumentNamespace: https://nimcp.ai/sbom/%ld\n", time(NULL));
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "Creator: Tool: NIMCP Supply Chain Security\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "Created: %s", ctime(&sc->stats.last_sbom_update));
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    /* Package information */
    written = snprintf(ptr, remaining, "PackageName: NIMCP\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "SPDXID: SPDXRef-Package-NIMCP\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "PackageVersion: 1.0.0\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "PackageSupplier: Organization: NIMCP Project\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    /* Dependencies */
    for (size_t i = 0; i < sc->dependency_count; i++) {
        nimcp_dependency_t* dep = &sc->dependencies[i];

        written = snprintf(ptr, remaining, "PackageName: %s\n", dep->name);
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        written = snprintf(ptr, remaining, "SPDXID: SPDXRef-Package-%zu\n", i);
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        written = snprintf(ptr, remaining, "PackageVersion: %s\n", dep->version);
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        written = snprintf(ptr, remaining, "PackageSupplier: %s\n", dep->supplier);
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        if (dep->license[0] != '\0') {
            written = snprintf(ptr, remaining, "PackageLicenseConcluded: %s\n", dep->license);
            if (written < 0 || (size_t)written >= remaining) goto overflow;
            ptr += written; remaining -= written;
        }
        if (dep->hash_sha256[0] != '\0') {
            written = snprintf(ptr, remaining, "PackageChecksum: SHA256: %s\n", dep->hash_sha256);
            if (written < 0 || (size_t)written >= remaining) goto overflow;
            ptr += written; remaining -= written;
        }

        written = snprintf(ptr, remaining, "\n");
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;
    }

    *output = sbom;
    return NIMCP_OK;

overflow:
    LOG_ERROR("SBOM buffer overflow - increase estimated_size");
    nimcp_free(sbom);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_spdx_sbom: memory allocation failed");
    return NIMCP_ERROR_NO_MEMORY;
}

static nimcp_error_t generate_cyclonedx_sbom(nimcp_supply_chain_t sc, char** output) {
    /* Estimate size */
    size_t estimated_size = 4096 + (sc->dependency_count * 1024);
    char* sbom = (char*)nimcp_malloc(estimated_size);
    if (!sbom) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_cyclonedx_sbom: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* CycloneDX 1.5 JSON format */
    char* ptr = sbom;
    size_t remaining = estimated_size;
    int written;

    written = snprintf(ptr, remaining, "{\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "  \"bomFormat\": \"CycloneDX\",\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "  \"specVersion\": \"1.5\",\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "  \"version\": 1,\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "  \"metadata\": {\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "    \"timestamp\": \"%ld\",\n", time(NULL));
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "    \"tools\": [\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "      {\"name\": \"NIMCP Supply Chain Security\"}\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "    ]\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "  },\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "  \"components\": [\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    /* Components */
    for (size_t i = 0; i < sc->dependency_count; i++) {
        nimcp_dependency_t* dep = &sc->dependencies[i];

        written = snprintf(ptr, remaining, "    {\n");
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        written = snprintf(ptr, remaining, "      \"type\": \"library\",\n");
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        written = snprintf(ptr, remaining, "      \"name\": \"%s\",\n", dep->name);
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        written = snprintf(ptr, remaining, "      \"version\": \"%s\",\n", dep->version);
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        if (dep->supplier[0] != '\0') {
            written = snprintf(ptr, remaining, "      \"supplier\": {\"name\": \"%s\"},\n", dep->supplier);
            if (written < 0 || (size_t)written >= remaining) goto overflow;
            ptr += written; remaining -= written;
        }
        if (dep->hash_sha256[0] != '\0') {
            written = snprintf(ptr, remaining, "      \"hashes\": [{\"alg\": \"SHA-256\", \"content\": \"%s\"}],\n",
                              dep->hash_sha256);
            if (written < 0 || (size_t)written >= remaining) goto overflow;
            ptr += written; remaining -= written;
        }

        written = snprintf(ptr, remaining, "      \"purl\": \"pkg:generic/%s@%s\"\n", dep->name, dep->version);
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;

        written = snprintf(ptr, remaining, "    }%s\n", (i < sc->dependency_count - 1) ? "," : "");
        if (written < 0 || (size_t)written >= remaining) goto overflow;
        ptr += written; remaining -= written;
    }

    written = snprintf(ptr, remaining, "  ]\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "}\n");
    if (written < 0 || (size_t)written >= remaining) goto overflow;
    ptr += written; remaining -= written;

    *output = sbom;
    return NIMCP_OK;

overflow:
    LOG_ERROR("SBOM buffer overflow - increase estimated_size");
    nimcp_free(sbom);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "generate_cyclonedx_sbom: memory allocation failed");
    return NIMCP_ERROR_NO_MEMORY;
}

nimcp_error_t nimcp_sbom_generate(nimcp_supply_chain_t sc,
                                   nimcp_sbom_format_t format,
                                   char** output) {
    if (!sc || sc->magic != NIMCP_SUPPLY_CHAIN_MAGIC || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&sc->lock);

    nimcp_error_t err;
    if (format == NIMCP_SBOM_FORMAT_SPDX) {
        err = generate_spdx_sbom(sc, output);
    } else if (format == NIMCP_SBOM_FORMAT_CYCLONEDX) {
        err = generate_cyclonedx_sbom(sc, output);
    } else {
        nimcp_mutex_unlock(&sc->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (err == NIMCP_OK) {
        sc->stats.sbom_generations++;
        sc->stats.last_sbom_update = time(NULL);
    }

    nimcp_mutex_unlock(&sc->lock);

    LOG_INFO("SBOM generated successfully (format=%d, size=%zu)",
                   format, *output ? strlen(*output) : 0);

    return err;
}

nimcp_error_t nimcp_sbom_save(nimcp_supply_chain_t sc,
                               const char* filepath,
                               nimcp_sbom_format_t format) {
    if (!sc || sc->magic != NIMCP_SUPPLY_CHAIN_MAGIC || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    char* sbom_content = NULL;
    nimcp_error_t err = nimcp_sbom_generate(sc, format, &sbom_content);
    if (err != NIMCP_OK || !sbom_content) {
        return err;
    }

    FILE* file = fopen(filepath, "w");
    if (!file) {
        nimcp_free(sbom_content);
        LOG_ERROR("nimcp_sbom_save: Cannot open file %s for writing", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "sbom: error condition");
        return NIMCP_ERROR_IO;
    }

    size_t bytes_written = fwrite(sbom_content, 1, strlen(sbom_content), file);
    fclose(file);
    nimcp_free(sbom_content);

    if (bytes_written == 0) {
        LOG_ERROR("nimcp_sbom_save: Write failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "sbom: error condition");
        return NIMCP_ERROR_IO;
    }

    LOG_INFO("SBOM saved to %s", filepath);

    return NIMCP_OK;
}

/* ========================================================================
 * Dependency Management
 * ======================================================================== */

nimcp_error_t nimcp_sbom_get_dependencies(nimcp_supply_chain_t sc,
                                           nimcp_dependency_t** deps,
                                           size_t* count) {
    if (!sc || sc->magic != NIMCP_SUPPLY_CHAIN_MAGIC || !deps || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&sc->lock);

    if (sc->dependency_count == 0) {
        *deps = NULL;
        *count = 0;
        nimcp_mutex_unlock(&sc->lock);
        return NIMCP_OK;
    }

    /* Allocate copy of dependencies */
    *deps = (nimcp_dependency_t*)nimcp_malloc(sc->dependency_count * sizeof(nimcp_dependency_t));
    if (!*deps) {
        nimcp_mutex_unlock(&sc->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sbom: memory allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    memcpy(*deps, sc->dependencies, sc->dependency_count * sizeof(nimcp_dependency_t));
    *count = sc->dependency_count;

    nimcp_mutex_unlock(&sc->lock);

    LOG_DEBUG("Retrieved %zu dependencies", *count);

    return NIMCP_OK;
}

nimcp_error_t nimcp_sbom_add_dependency(nimcp_supply_chain_t sc,
                                         const nimcp_dependency_t* dep) {
    if (!sc || sc->magic != NIMCP_SUPPLY_CHAIN_MAGIC || !dep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&sc->lock);

    /* Check capacity */
    if (sc->dependency_count >= sc->dependency_capacity) {
        size_t new_capacity = sc->dependency_capacity * 2;
        nimcp_dependency_t* new_deps = (nimcp_dependency_t*)nimcp_realloc(
            sc->dependencies,
            new_capacity * sizeof(nimcp_dependency_t)
        );
        if (!new_deps) {
            nimcp_mutex_unlock(&sc->lock);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sbom: memory allocation failed");
            return NIMCP_ERROR_NO_MEMORY;
        }
        sc->dependencies = new_deps;
        sc->dependency_capacity = new_capacity;
    }

    /* Add dependency */
    memcpy(&sc->dependencies[sc->dependency_count], dep, sizeof(nimcp_dependency_t));
    sc->dependencies[sc->dependency_count].magic = NIMCP_DEPENDENCY_MAGIC;
    sc->dependency_count++;
    sc->stats.total_dependencies++;

    nimcp_mutex_unlock(&sc->lock);

    LOG_INFO("Added dependency: %s version %s", dep->name, dep->version);

    return NIMCP_OK;
}

nimcp_error_t nimcp_sbom_remove_dependency(nimcp_supply_chain_t sc,
                                            const char* name) {
    if (!sc || sc->magic != NIMCP_SUPPLY_CHAIN_MAGIC || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&sc->lock);

    /* Find and remove dependency */
    for (size_t i = 0; i < sc->dependency_count; i++) {
        if (strcmp(sc->dependencies[i].name, name) == 0) {
            /* Shift remaining dependencies */
            memmove(&sc->dependencies[i], &sc->dependencies[i + 1],
                    (sc->dependency_count - i - 1) * sizeof(nimcp_dependency_t));
            sc->dependency_count--;
            nimcp_mutex_unlock(&sc->lock);
            LOG_INFO("Removed dependency: %s", name);
            return NIMCP_OK;
        }
    }

    nimcp_mutex_unlock(&sc->lock);

    LOG_WARN("Dependency not found: %s", name);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "sbom: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

nimcp_error_t nimcp_sbom_query_dependency(nimcp_supply_chain_t sc,
                                           const char* name,
                                           nimcp_dependency_t* dep) {
    if (!sc || sc->magic != NIMCP_SUPPLY_CHAIN_MAGIC || !name || !dep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sbom: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&sc->lock);

    /* Find dependency */
    for (size_t i = 0; i < sc->dependency_count; i++) {
        if (strcmp(sc->dependencies[i].name, name) == 0) {
            memcpy(dep, &sc->dependencies[i], sizeof(nimcp_dependency_t));
            nimcp_mutex_unlock(&sc->lock);
            return NIMCP_OK;
        }
    }

    nimcp_mutex_unlock(&sc->lock);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "sbom: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}
