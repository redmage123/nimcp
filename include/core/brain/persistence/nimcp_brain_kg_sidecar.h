/**
 * @file nimcp_brain_kg_sidecar.h
 * @brief Minimal file-based sidecar for the internal runtime KG
 * @date 2026-05-04
 *
 * WHAT: brain_kg_save() / brain_kg_load() — flatten the internal KG to
 *       a small binary file alongside the brain checkpoint.
 * WHY:  Without this the runtime KG is rebuilt cold on every daemon
 *       restart. Edges + node metadata accumulated during training
 *       (HANDLES_MESSAGE bindings, runtime nodes added by consumers,
 *       weight updates) are otherwise lost.
 * HOW:  Iterate KG via the existing CRUD API; write a small magic-prefixed
 *       header followed by node and edge arrays. On load, re-add via the
 *       same CRUD API and remap saved IDs to fresh IDs assigned at insert.
 *
 * NOTE: This sidecar is intentionally narrow — it does NOT round-trip
 *       message-type indices, security state, immune-callback wiring or
 *       integrity baselines. Those are runtime concerns rebuilt at init.
 */

#ifndef NIMCP_BRAIN_KG_SIDECAR_H
#define NIMCP_BRAIN_KG_SIDECAR_H

#ifdef __cplusplus
extern "C" {
#endif

struct brain_kg;

/**
 * @brief Save the internal KG to a file (best-effort).
 *
 * Writes header magic, version, node array, edge array. Does NOT
 * fsync — caller decides durability. Truncates / overwrites destination.
 *
 * @param kg KG handle (NULL: returns -1)
 * @param filepath Destination path
 * @return 0 on success, -1 on error
 */
int brain_kg_save(struct brain_kg* kg, const char* filepath);

/**
 * @brief Load KG state from a sidecar file.
 *
 * Re-adds nodes via brain_kg_add_node() and edges via brain_kg_add_edge(),
 * remapping the saved IDs onto whatever IDs the KG assigns at insert time.
 * Existing nodes (name match) are kept; only missing nodes/edges are
 * appended. Returns -1 if the file is missing or has bad magic.
 *
 * @param kg KG handle (NULL: returns -1)
 * @param filepath Source path
 * @return 0 on success, -1 on error
 */
int brain_kg_load(struct brain_kg* kg, const char* filepath);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_KG_SIDECAR_H */
