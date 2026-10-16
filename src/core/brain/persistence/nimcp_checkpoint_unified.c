/**
 * @file nimcp_checkpoint_unified.c
 * @brief Unified checkpoint save/load — single file for brain + all sidecars
 *
 * ARCHITECTURE:
 *   Save: sync GPU → save each subsystem to temp file → copy into unified file
 *         → write section table → CRC32 → atomic rename
 *   Load: read header → read section table → extract each section to temp file
 *         → load each subsystem from temp file → cleanup temps
 *   Auto: read first 4 bytes → dispatch to unified or legacy loader
 */

#include "core/brain/persistence/nimcp_checkpoint_format.h"
#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_MODULE "CHECKPOINT"

/* Use the same forward declarations as nimcp_brain_persistence.c */
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"
#include "lnn/nimcp_lnn_network.h"
#include "training/nimcp_cnn_training.h"
#include "training/nimcp_cortex_cnn.h"
#include "gpu/training/nimcp_training_bridge.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/nimcp_working_memory.h"

/* Forward declarations matching exact signatures in the codebase */
extern brain_t brain_load(const char* filepath);
extern bool nimcp_brain_save_metadata(brain_t brain, const char* filepath);
extern bool nimcp_brain_load_metadata(brain_t brain, const char* filepath);

/*=============================================================================
 * CRC32 (standard polynomial 0xEDB88320)
 *=============================================================================*/

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t nimcp_crc32(const void* data, size_t length) {
    const uint8_t* buf = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t nimcp_crc32_file(FILE* fp, uint64_t offset, uint64_t length) {
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buf[65536];
    fseeko(fp, (off_t)offset, SEEK_SET);
    uint64_t remaining = length;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buf) ? (size_t)remaining : sizeof(buf);
        size_t read = fread(buf, 1, chunk, fp);
        if (read == 0) break;
        for (size_t i = 0; i < read; i++) {
            crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
        }
        remaining -= read;
    }
    return crc ^ 0xFFFFFFFF;
}

/*=============================================================================
 * Helper: save a subsystem to temp file, return size
 *=============================================================================*/

typedef bool (*subsystem_save_fn)(brain_t brain, const char* tmp_path);

/* Save adaptive network to temp path */
static bool save_brain_core(brain_t brain, const char* tmp_path) {
    if (!brain->network) return false;
    return adaptive_network_save(brain->network, tmp_path, 0 /* SERIALIZE_FORMAT_BINARY */);
}

/* Save metadata to temp path */
static bool save_meta(brain_t brain, const char* tmp_path) {
    /* nimcp_brain_save_metadata appends ".meta" to the path, but we want the
     * exact path. Save to a fake base then rename. */
    char base_path[4096];
    snprintf(base_path, sizeof(base_path), "%s", tmp_path);
    /* Strip .meta if present — the function adds it */
    size_t len = strlen(base_path);
    if (len > 5 && strcmp(base_path + len - 5, ".meta") == 0) {
        base_path[len - 5] = '\0';
    }
    return nimcp_brain_save_metadata(brain, base_path);
}

/* Save SNN */
static bool save_snn(brain_t brain, const char* tmp_path) {
    if (!brain->snn_network) return true; /* nothing to save */
    return snn_network_save(brain->snn_network, tmp_path) == 0;
}

/* Save LNN */
static bool save_lnn(brain_t brain, const char* tmp_path) {
    if (!brain->lnn_network) return true;
    return lnn_network_save(brain->lnn_network, tmp_path) == 0;
}

/* Save CNN */
static bool save_cnn(brain_t brain, const char* tmp_path) {
    if (!brain->cnn_trainer) return true;
    return cnn_trainer_save(brain->cnn_trainer, tmp_path) == 0;
}

/* Save cortex CNN by index */
static bool save_cortex(brain_t brain, const char* tmp_path, int cortex_idx) {
    if (cortex_idx < 0 || cortex_idx >= 4) return false;
    if (!brain->cortex_cnns[cortex_idx]) return true;
    return cortex_cnn_save(brain->cortex_cnns[cortex_idx], tmp_path) == 0;
}

/* Save executive to file */
static bool save_executive(brain_t brain, const char* tmp_path) {
    if (!brain->executive) return true;
    FILE* f = fopen(tmp_path, "wb");
    if (!f) return false;
    executive_save(brain->executive, f);
    fclose(f);
    return true;
}

/* Save mirror neurons to file */
static bool save_mirror(brain_t brain, const char* tmp_path) {
    if (!brain->mirror_neurons) return true;
    FILE* f = fopen(tmp_path, "wb");
    if (!f) return false;
    bool ok = mirror_neurons_save(brain->mirror_neurons, f);
    fclose(f);
    return ok;
}

/* Save tokenizer */
static bool save_tokenizer(brain_t brain, const char* tmp_path) {
    if (!brain->tokenizer) return true;
    return tokenizer_save(brain->tokenizer, tmp_path) == 0;
}

/*=============================================================================
 * Helper: copy file contents into unified file at current position
 *=============================================================================*/

static uint64_t copy_file_to_unified(FILE* unified, const char* src_path) {
    FILE* src = fopen(src_path, "rb");
    if (!src) return 0;

    /* Get source file size */
    fseeko(src, 0, SEEK_END);
    uint64_t size = (uint64_t)ftello(src);
    fseeko(src, 0, SEEK_SET);

    if (size == 0) { fclose(src); return 0; }

    /* Copy in chunks */
    uint8_t buf[65536];
    uint64_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buf) ? (size_t)remaining : sizeof(buf);
        size_t rd = fread(buf, 1, chunk, src);
        if (rd == 0) break;
        fwrite(buf, 1, rd, unified);
        remaining -= rd;
    }
    fclose(src);
    return size;
}

/*=============================================================================
 * SAVE: Unified checkpoint
 *=============================================================================*/

bool brain_save_unified(brain_t brain, const char* filepath)
{
    if (!brain || !filepath) return false;

    /* === GPU WEIGHT SYNC ===
     * Download GPU weights to CPU before saving. Without this,
     * GPU-side plasticity updates would be lost in the checkpoint. */
    if (brain->gpu_enabled && brain->network) {
        struct nimcp_gpu_weight_cache_s* cache =
            adaptive_network_get_gpu_weight_cache(brain->network);
        if (cache) {
            nimcp_gpu_weight_cache_download(cache,
                adaptive_network_get_base_network(brain->network));
            LOG_INFO("GPU weights synced to CPU before checkpoint save");
        }
    }

    /* Create temp file for atomic save */
    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.unified_tmp", filepath);

    FILE* uf = fopen(tmp_path, "w+b"); /* w+b: read+write for CRC computation */
    if (!uf) {
        LOG_ERROR("Cannot create unified checkpoint: %s (%s)", tmp_path, strerror(errno));
        return false;
    }

    /* Write placeholder header (will be updated at end) */
    nimcp_checkpoint_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = NIMCP_UNIFIED_MAGIC;
    header.format_version = NIMCP_UNIFIED_VERSION;
    /* Store layout version in reserved[0..3] for struct compatibility detection */
    uint32_t layout_ver = NIMCP_LAYOUT_VERSION;
    memcpy(header.reserved, &layout_ver, sizeof(layout_ver));
    fwrite(&header, sizeof(header), 1, uf);

    /* Section tracking */
    nimcp_section_entry_t sections[NIMCP_MAX_SECTIONS];
    memset(sections, 0, sizeof(sections));
    uint32_t num_sections = 0;

    /* Helper: save subsystem to temp, copy into unified, record section */
    #define SAVE_SECTION(section_name, save_expr) do { \
        if (num_sections >= NIMCP_MAX_SECTIONS) break; \
        char _stmp[4096]; \
        snprintf(_stmp, sizeof(_stmp), "%s._sec_%s", filepath, section_name); \
        if (save_expr) { \
            uint64_t _off = (uint64_t)ftello(uf); \
            uint64_t _sz = copy_file_to_unified(uf, _stmp); \
            if (_sz > 0) { \
                strncpy(sections[num_sections].name, section_name, \
                        NIMCP_SECTION_NAME_LEN - 1); \
                sections[num_sections].offset = _off; \
                sections[num_sections].size = _sz; \
                num_sections++; \
            } \
        } \
        unlink(_stmp); /* Always clean up temp file */ \
    } while(0)

    /* Save brain_core to temp file, then copy into unified.
     * This requires ~8.1GB of temp disk space. If insufficient,
     * the save fails and brain_save() falls back to legacy format
     * (which the daemon manages with disk pruning). */
    SAVE_SECTION(NIMCP_SEC_BRAIN_CORE, save_brain_core(brain, _stmp));

    if (num_sections == 0) {
        /* brain_core is mandatory — if it failed, abort unified save */
        LOG_WARN("brain_core save failed (disk full?) — falling back to legacy");
        fclose(uf);
        unlink(tmp_path);
        return false;
    }

    /* Metadata: nimcp_brain_save_metadata adds ".meta" suffix internally.
     * We pass the base path and then look for the .meta file. */
    {
        char meta_base[4096];
        snprintf(meta_base, sizeof(meta_base), "%s._sec_meta_base", filepath);
        if (nimcp_brain_save_metadata(brain, meta_base)) {
            char meta_actual[4096];
            snprintf(meta_actual, sizeof(meta_actual), "%s._sec_meta_base.meta", filepath);
            if (num_sections < NIMCP_MAX_SECTIONS) {
                uint64_t off = (uint64_t)ftello(uf);
                uint64_t sz = copy_file_to_unified(uf, meta_actual);
                if (sz > 0) {
                    strncpy(sections[num_sections].name, NIMCP_SEC_META,
                            NIMCP_SECTION_NAME_LEN - 1);
                    sections[num_sections].offset = off;
                    sections[num_sections].size = sz;
                    num_sections++;
                }
            }
            unlink(meta_actual);
        }
        /* Clean up any other files metadata might have created */
        char meta_cleanup[4096];
        const char* meta_exts[] = {".knowledge", ".executive", ".pink_noise",
                                    ".mirror_neurons", ".tokenizer", NULL};
        for (int i = 0; meta_exts[i]; i++) {
            snprintf(meta_cleanup, sizeof(meta_cleanup), "%s._sec_meta_base%s",
                     filepath, meta_exts[i]);
            unlink(meta_cleanup);
        }
    }

    SAVE_SECTION(NIMCP_SEC_SNN, save_snn(brain, _stmp));
    SAVE_SECTION(NIMCP_SEC_LNN, save_lnn(brain, _stmp));
    SAVE_SECTION(NIMCP_SEC_CNN, save_cnn(brain, _stmp));
    SAVE_SECTION(NIMCP_SEC_CORTEX_VIS, save_cortex(brain, _stmp, 0));
    SAVE_SECTION(NIMCP_SEC_CORTEX_AUD, save_cortex(brain, _stmp, 1));
    SAVE_SECTION(NIMCP_SEC_CORTEX_SPE, save_cortex(brain, _stmp, 2));
    SAVE_SECTION(NIMCP_SEC_CORTEX_SOM, save_cortex(brain, _stmp, 3));
    SAVE_SECTION(NIMCP_SEC_MIRROR, save_mirror(brain, _stmp));
    SAVE_SECTION(NIMCP_SEC_EXECUTIVE, save_executive(brain, _stmp));
    SAVE_SECTION(NIMCP_SEC_TOKENIZER, save_tokenizer(brain, _stmp));

    #undef SAVE_SECTION

    /* Write section table */
    uint64_t table_offset = (uint64_t)ftello(uf);
    fwrite(sections, sizeof(nimcp_section_entry_t), num_sections, uf);

    /* Compute CRC32 of all section data (from byte 64 to table_offset) */
    uint64_t data_start = NIMCP_CHECKPOINT_HEADER_SIZE;
    uint64_t data_len = table_offset - data_start;
    fflush(uf);
    uint32_t checksum = nimcp_crc32_file(uf, data_start, data_len);

    /* Update header */
    header.num_sections = num_sections;
    header.section_table_offset = table_offset;
    header.total_size = (uint64_t)ftello(uf);
    header.checksum = checksum;

    fseeko(uf, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, uf);

    fclose(uf);

    /* Atomic rename */
    if (rename(tmp_path, filepath) != 0) {
        LOG_ERROR("Atomic rename failed: %s → %s (%s)",
                  tmp_path, filepath, strerror(errno));
        unlink(tmp_path);
        return false;
    }

    LOG_INFO("Unified checkpoint saved: %s (%u sections, %.1f MB, CRC=%08X)",
             filepath, num_sections,
             (double)header.total_size / (1024.0 * 1024.0), checksum);

    return true;
}

/*=============================================================================
 * LOAD: Unified checkpoint
 *=============================================================================*/

brain_t brain_load_unified(const char* filepath)
{
    if (!filepath) return NULL;

    FILE* uf = fopen(filepath, "rb");
    if (!uf) {
        LOG_ERROR("Cannot open unified checkpoint: %s", filepath);
        return NULL;
    }

    /* Read header */
    nimcp_checkpoint_header_t header;
    if (fread(&header, sizeof(header), 1, uf) != 1) {
        LOG_ERROR("Cannot read checkpoint header");
        fclose(uf);
        return NULL;
    }

    if (header.magic != NIMCP_UNIFIED_MAGIC) {
        LOG_ERROR("Invalid checkpoint magic: 0x%08X (expected 0x%08X)",
                  header.magic, NIMCP_UNIFIED_MAGIC);
        fclose(uf);
        return NULL;
    }

    if (header.num_sections == 0 || header.num_sections > NIMCP_MAX_SECTIONS) {
        LOG_ERROR("Invalid section count: %u", header.num_sections);
        fclose(uf);
        return NULL;
    }

    /* Check layout version stored in reserved[0..3].
     * Old checkpoints (pre-SNN-primary) have 0 here. */
    uint32_t saved_layout = 0;
    memcpy(&saved_layout, header.reserved, sizeof(saved_layout));
    if (saved_layout < NIMCP_LAYOUT_VERSION) {
        LOG_ERROR("Checkpoint layout version %u is incompatible with current "
                  "version %u (SNN-primary architecture change). "
                  "Use --fresh to create a new brain.",
                  saved_layout, NIMCP_LAYOUT_VERSION);
        fclose(uf);
        return NULL;
    }

    /* Verify CRC32 */
    uint64_t data_start = NIMCP_CHECKPOINT_HEADER_SIZE;
    if (header.section_table_offset < data_start) {
        LOG_ERROR("Invalid section table offset: %lu < header size %lu",
                  (unsigned long)header.section_table_offset, (unsigned long)data_start);
        fclose(uf);
        return NULL;
    }
    uint64_t data_len = header.section_table_offset - data_start;
    uint32_t computed_crc = nimcp_crc32_file(uf, data_start, data_len);
    if (computed_crc != header.checksum) {
        LOG_WARN("CRC32 mismatch: computed=0x%08X stored=0x%08X — checkpoint may be corrupt",
                 computed_crc, header.checksum);
        /* Continue anyway — warn but don't fail (allows partial recovery) */
    }

    /* Read section table */
    nimcp_section_entry_t sections[NIMCP_MAX_SECTIONS];
    fseeko(uf, (off_t)header.section_table_offset, SEEK_SET);
    if (fread(sections, sizeof(nimcp_section_entry_t), header.num_sections, uf)
        != header.num_sections) {
        LOG_ERROR("Cannot read section table");
        fclose(uf);
        return NULL;
    }

    /* Extract each section to a temp file and load */
    brain_t brain = NULL;
    char tmp_path[4096];

    /* Helper: extract section to temp file */
    #define EXTRACT_SECTION(sec_name, out_tmp) do { \
        out_tmp[0] = '\0'; \
        for (uint32_t _si = 0; _si < header.num_sections; _si++) { \
            if (strcmp(sections[_si].name, sec_name) == 0) { \
                snprintf(out_tmp, sizeof(out_tmp), "%s._load_%s", filepath, sec_name); \
                FILE* _tf = fopen(out_tmp, "wb"); \
                if (_tf) { \
                    fseeko(uf, (off_t)sections[_si].offset, SEEK_SET); \
                    uint8_t _buf[65536]; \
                    uint64_t _rem = sections[_si].size; \
                    while (_rem > 0) { \
                        size_t _ch = _rem < sizeof(_buf) ? (size_t)_rem : sizeof(_buf); \
                        size_t _rd = fread(_buf, 1, _ch, uf); \
                        if (_rd == 0) break; \
                        fwrite(_buf, 1, _rd, _tf); \
                        _rem -= _rd; \
                    } \
                    fclose(_tf); \
                } \
                break; \
            } \
        } \
    } while(0)

    /* 1. Load brain core (adaptive network) */
    EXTRACT_SECTION(NIMCP_SEC_BRAIN_CORE, tmp_path);
    if (tmp_path[0]) {
        /* Use the legacy brain_load which handles adaptive_network_load +
         * metadata + brain struct allocation. We extract brain_core to a
         * temp file that looks like a legacy checkpoint. */
        fprintf(stderr, "[UNIFIED] Calling brain_load on extracted core: %s\n", tmp_path);
        brain = brain_load(tmp_path);
        fprintf(stderr, "[UNIFIED] brain_load returned: %p\n", (void*)brain);
        unlink(tmp_path);
    } else {
        fprintf(stderr, "[UNIFIED] EXTRACT_SECTION produced no tmp_path for brain_core\n");
    }

    if (!brain) {
        LOG_ERROR("Failed to load brain_core section");
        fclose(uf);
        return NULL;
    }

    /* 2. Load metadata (overwrites defaults from brain_load) */
    EXTRACT_SECTION(NIMCP_SEC_META, tmp_path);
    if (tmp_path[0]) {
        /* nimcp_brain_load_metadata expects base path, adds ".meta" */
        char meta_base[4096];
        snprintf(meta_base, sizeof(meta_base), "%s", tmp_path);
        size_t len = strlen(meta_base);
        /* The temp file IS the .meta file, so create the expected path */
        char meta_expected[4096];
        snprintf(meta_expected, sizeof(meta_expected), "%s._load_meta_base.meta", filepath);
        rename(tmp_path, meta_expected);
        char meta_base2[4096];
        snprintf(meta_base2, sizeof(meta_base2), "%s._load_meta_base", filepath);
        nimcp_brain_load_metadata(brain, meta_base2);
        unlink(meta_expected);
        (void)len;
    }

    /* 3. Load SNN */
    EXTRACT_SECTION(NIMCP_SEC_SNN, tmp_path);
    if (tmp_path[0]) {
        struct snn_network_s* snn = snn_network_load(tmp_path);
        if (snn) {
            brain->snn_network = snn;
            brain->owns_specialized_network = true;
            snn->config.input_current_scale = 70.0f;
            LOG_INFO("Loaded SNN from unified checkpoint");
        }
        unlink(tmp_path);
    }

    /* 4. Load LNN */
    EXTRACT_SECTION(NIMCP_SEC_LNN, tmp_path);
    if (tmp_path[0]) {
        struct lnn_network_s* lnn = lnn_network_load(tmp_path);
        if (lnn) {
            brain->lnn_network = lnn;
            brain->owns_specialized_network = true;
            LOG_INFO("Loaded LNN from unified checkpoint");
        }
        unlink(tmp_path);
    }

    /* 5. Load CNN */
    EXTRACT_SECTION(NIMCP_SEC_CNN, tmp_path);
    if (tmp_path[0]) {
        if (brain->cnn_trainer) {
            cnn_trainer_load_weights(brain->cnn_trainer, tmp_path);
            LOG_INFO("Loaded CNN from unified checkpoint");
        }
        unlink(tmp_path);
    }

    /* 6. Load cortex CNNs */
    const char* cortex_sections[4] = {
        NIMCP_SEC_CORTEX_VIS, NIMCP_SEC_CORTEX_AUD,
        NIMCP_SEC_CORTEX_SPE, NIMCP_SEC_CORTEX_SOM
    };
    for (int ci = 0; ci < 4; ci++) {
        EXTRACT_SECTION(cortex_sections[ci], tmp_path);
        if (tmp_path[0] && brain->cortex_cnns[ci]) {
            cortex_cnn_load(brain->cortex_cnns[ci], tmp_path);
            LOG_INFO("Loaded %s cortex from unified checkpoint", cortex_sections[ci]);
        }
        if (tmp_path[0]) unlink(tmp_path);
    }

    /* 7. Load mirror neurons */
    EXTRACT_SECTION(NIMCP_SEC_MIRROR, tmp_path);
    if (tmp_path[0]) {
        FILE* mf = fopen(tmp_path, "rb");
        if (mf) {
            void* mn = mirror_neurons_load(mf);
            if (mn) {
                brain->mirror_neurons = mn;
                LOG_INFO("Loaded mirror neurons from unified checkpoint");
            }
            fclose(mf);
        }
        unlink(tmp_path);
    }

    /* 8. Load executive */
    EXTRACT_SECTION(NIMCP_SEC_EXECUTIVE, tmp_path);
    if (tmp_path[0]) {
        FILE* ef = fopen(tmp_path, "rb");
        if (ef) {
            void* ex = executive_load(ef);
            if (ex) {
                brain->executive = ex;
                LOG_INFO("Loaded executive from unified checkpoint");
            }
            fclose(ef);
        }
        unlink(tmp_path);
    }

    #undef EXTRACT_SECTION

    fclose(uf);

    LOG_INFO("Unified checkpoint loaded: %s (%u sections)", filepath, header.num_sections);
    return brain;
}

/*=============================================================================
 * AUTO-DETECT: Read magic bytes and dispatch
 *=============================================================================*/

brain_t brain_load_auto(const char* filepath)
{
    if (!filepath) return NULL;

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        LOG_ERROR("Cannot open checkpoint: %s", filepath);
        return NULL;
    }

    uint32_t magic = 0;
    if (fread(&magic, sizeof(magic), 1, f) != 1) {
        LOG_ERROR("Cannot read checkpoint magic from %s", filepath);
        fclose(f);
        return NULL;
    }
    fclose(f);

    brain_t brain = NULL;
    if (magic == NIMCP_UNIFIED_MAGIC) {
        LOG_INFO("Detected unified checkpoint format (NIMV)");
        brain = brain_load_unified(filepath);
    } else if (magic == NIMCP_LEGACY_MAGIC) {
        LOG_INFO("Detected legacy checkpoint format (NIMC) — loading with sidecars");
        brain = brain_load(filepath);
    } else {
        LOG_WARN("Unknown checkpoint magic 0x%08X — trying legacy loader", magic);
        brain = brain_load(filepath);
    }

    /* Stash the source filepath so nimcp_brain_eager_init_cognitive() can
     * load the .immune/.kg/.gl_lang sidecars AFTER the corresponding
     * subsystems are created. Inside brain_load() itself, those subsystems
     * are still NULL, so the sidecar `if (brain->grounded_lang)` block
     * silently no-ops and the trained lexicon never reaches the live brain. */
    if (brain) {
        strncpy(brain->loaded_from_path, filepath,
                sizeof(brain->loaded_from_path) - 1);
        brain->loaded_from_path[sizeof(brain->loaded_from_path) - 1] = '\0';
    }

    /* Post-load layout version check: old checkpoints (pre-SNN-primary)
     * have snn_target_neurons == 0 because those fields didn't exist.
     * New brains always set snn_target_neurons during init. */
    if (brain && brain->config.snn_target_neurons == 0) {
        LOG_WARN("Checkpoint appears to be pre-SNN-primary architecture "
                 "(snn_target_neurons=0). Architecture has changed: "
                 "ANN 2M→150K, SNN 768→1.8M, CNN embed 64→256. "
                 "Use --fresh to create a new brain.");
        /* Don't block — allow loading for inspection/migration, but warn loudly */
    }

    return brain;
}
