#ifndef ALFRED_TABLES_H
#define ALFRED_TABLES_H

#include "alfred_correlator.h"

#include <stdint.h>

typedef struct alfred_move_entry {
    uint32_t cookie;
    uint64_t ts_ns;
    int pid;
    int is_dir;
    char *src_path;
    struct alfred_move_entry *next;
} alfred_move_entry_t;

typedef struct alfred_debounce_entry {
    char *path;
    uint64_t last_emit_ns;
    uint64_t create_ns;
    struct alfred_debounce_entry *next;
} alfred_debounce_entry_t;

struct alfred_engine {
    alfred_config_t cfg;
    alfred_emit_fn emit;
    void *userdata;
    uint64_t seq;

    alfred_move_entry_t *moves[1024];
    alfred_debounce_entry_t *debounce[1024];
};

void alfred_move_insert(
    alfred_engine_t *e,
    const alfred_raw_event_t *r);

alfred_move_entry_t *alfred_move_take(
    alfred_engine_t *e,
    uint32_t cookie);

void alfred_move_sweep(
    alfred_engine_t *e,
    uint64_t now_ns);

alfred_debounce_entry_t *alfred_debounce_get(
    alfred_engine_t *e,
    const char *path);

int alfred_debounce_should_emit(
    alfred_engine_t *e,
    alfred_debounce_entry_t *d,
    uint64_t now_ns);

void alfred_tables_destroy(alfred_engine_t *e);

#endif /* ALFRED_TABLES_H */
