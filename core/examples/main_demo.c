/*======================================================================
 *
 * FILE: examples/main_demo.c
 *
 * PURPOSE:
 *   Demonstration program for Alfred correlator engine.
 *
 * WHAT IT DOES:
 *   Feeds synthetic raw filesystem events into the engine:
 *
 *     - file creation
 *     - modify storms
 *     - rename/move sequences
 *     - delete events
 *
 *   Then prints the resulting high-level semantic events.
 *
 * WHY THIS EXISTS:
 *   - validate correctness
 *   - debug correlation logic
 *   - simulate real filesystem behavior without kernel
 *
 *======================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alfred_correlator.h"

/*======================================================================
 * SIMPLE PRINT CALLBACK
 *======================================================================*/

/*
 * This callback is invoked by the engine for each
 * high-level event.
 *
 * In production this could be:
 *   - indexer
 *   - database writer
 *   - network stream
 */
static void on_event(
    const alfred_event_t *ev,
    void *userdata)
{
    (void)userdata;

    printf(
        "[%llu] %-20s src=%s dst=%s pid=%d\n",
        (unsigned long long)ev->seq,
        alfred_event_name(ev->type),
        ev->src_path ? ev->src_path : "-",
        ev->dst_path ? ev->dst_path : "-",
        ev->pid
    );
}

/*======================================================================
 * HELPERS TO CREATE RAW EVENTS
 *======================================================================*/

static alfred_raw_event_t mk_event(
    uint32_t mask,
    const char *path,
    uint32_t cookie,
    pid_t pid)
{
    alfred_raw_event_t e;

    memset(&e, 0, sizeof(e));

    e.ts_ns = 1000000; /* fake monotonic time */
    e.source = ALFRED_SRC_INOTIFY;
    e.mask = mask;
    e.cookie = cookie;
    e.pid = pid;
    e.path = path;

    return e;
}

/*======================================================================
 * MAIN DEMO
 *======================================================================*/

int main(void)
{
    alfred_config_t cfg;
    alfred_engine_t *eng;

    /*==============================================================
     * CONFIGURATION
     *==============================================================*/

    alfred_config_default(&cfg);

    cfg.move_timeout_ms = 200;
    cfg.modify_debounce_ms = 100;
    cfg.create_ready_ms = 300;

    /*==============================================================
     * ENGINE INIT
     *==============================================================*/

    eng = alfred_create(&cfg, on_event, NULL);

    if (!eng) {
        fprintf(stderr, "failed to create engine\n");
        return 1;
    }

    /*==============================================================
     * 1. FILE CREATE + READY SEQUENCE
     *==============================================================*/

    alfred_process(eng,
        &mk_event(ALFRED_RAW_CREATE, "/tmp/a.txt", 0, 100));

    alfred_process(eng,
        &mk_event(ALFRED_RAW_CLOSE_WRITE, "/tmp/a.txt", 0, 100));

    /*==============================================================
     * 2. MODIFY STORM (should be debounced)
     *==============================================================*/

    for (int i = 0; i < 5; i++) {
        alfred_process(eng,
            &mk_event(ALFRED_RAW_MODIFY, "/tmp/a.txt", 0, 100));
    }

    /*==============================================================
     * 3. RENAME SEQUENCE (MOVE_FROM + MOVE_TO)
     *==============================================================*/

    alfred_process(eng,
        &mk_event(ALFRED_RAW_MOVED_FROM, "/tmp/a.txt", 55, 100));

    alfred_process(eng,
        &mk_event(ALFRED_RAW_MOVED_TO, "/home/a.txt", 55, 100));

    /*==============================================================
     * 4. DELETE EVENT
     *==============================================================*/

    alfred_process(eng,
        &mk_event(ALFRED_RAW_DELETE, "/home/a.txt", 0, 100));

    /*==============================================================
     * FLUSH + CLEANUP
     *==============================================================*/

    alfred_flush(eng);
    alfred_destroy(eng);

    return 0;
}
