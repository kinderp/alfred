/* ============================================================================
 * alfred_tables.h - internal state tables for the core correlator
 *
 * These types are private to the core implementation. They store the state that
 * turns isolated raw events into semantic behavior: pending MOVED_FROM records
 * and per-path MODIFY debounce timestamps.
 * ========================================================================== */

#ifndef ALFRED_TABLES_H
#define ALFRED_TABLES_H

#include "alfred_correlator.h"

#include <stdint.h>

/*
 * alfred_move_entry_t - pending MOVED_FROM waiting for MOVED_TO
 * @cookie: backend move cookie used for correlation
 * @ts_ns: raw event timestamp used for timeout sweeping
 * @pid: process id if known
 * @is_dir: nonzero when the moved object is a directory
 * @src_path: copied source path owned by the entry
 * @next: next entry in the hash bucket chain
 */
typedef struct alfred_move_entry {
    uint32_t cookie;
    uint64_t ts_ns;
    int pid;
    int is_dir;
    char *src_path;
    struct alfred_move_entry *next;
} alfred_move_entry_t;

/*
 * alfred_debounce_entry_t - per-path MODIFY debounce state
 * @path: copied path key owned by the entry
 * @last_emit_ns: timestamp of the last emitted FILE_MODIFIED
 * @create_ns: reserved for future create-ready correlation
 * @next: next entry in the hash bucket chain
 */
typedef struct alfred_debounce_entry {
    char *path;
    uint64_t last_emit_ns;
    uint64_t create_ns;
    struct alfred_debounce_entry *next;
} alfred_debounce_entry_t;

/*
 * alfred_engine - private core engine state
 * @cfg: copied runtime configuration
 * @emit: callback used to publish semantic events
 * @userdata: opaque pointer passed to @emit
 * @seq: monotonically increasing semantic event sequence number
 * @moves: hash table of pending move entries keyed by cookie
 * @debounce: hash table of debounce entries keyed by path
 */
struct alfred_engine {
    alfred_config_t cfg;
    alfred_emit_fn emit;
    void *userdata;
    uint64_t seq;

    alfred_move_entry_t *moves[1024];
    alfred_debounce_entry_t *debounce[1024];
};

/*
 * alfred_move_insert - store a pending MOVED_FROM raw event
 * @e: core engine
 * @r: raw MOVED_FROM event
 */
void alfred_move_insert(
    alfred_engine_t *e,
    const alfred_raw_event_t *r);

/*
 * alfred_move_take - find and remove a pending move by cookie
 * @e: core engine
 * @cookie: backend move cookie
 *
 * Return: removed entry on success, NULL when no matching entry exists.
 */
alfred_move_entry_t *alfred_move_take(
    alfred_engine_t *e,
    uint32_t cookie);

/*
 * alfred_move_sweep - remove pending moves older than the configured timeout
 * @e: core engine
 * @now_ns: current monotonic timestamp
 */
void alfred_move_sweep(
    alfred_engine_t *e,
    uint64_t now_ns);

/*
 * alfred_debounce_get - find or create debounce state for a path
 * @e: core engine
 * @path: filesystem path used as debounce key
 *
 * Return: debounce entry on success, NULL on invalid input.
 */
alfred_debounce_entry_t *alfred_debounce_get(
    alfred_engine_t *e,
    const char *path);

/*
 * alfred_debounce_should_emit - decide whether MODIFY should be emitted
 * @e: core engine
 * @d: debounce entry for the path being modified
 * @now_ns: current monotonic timestamp
 *
 * Return: 1 to emit FILE_MODIFIED, 0 to suppress it.
 */
int alfred_debounce_should_emit(
    alfred_engine_t *e,
    alfred_debounce_entry_t *d,
    uint64_t now_ns);

/*
 * alfred_tables_destroy - release all internal move/debounce table state
 * @e: core engine to clean up
 */
void alfred_tables_destroy(alfred_engine_t *e);

#endif /* ALFRED_TABLES_H */
