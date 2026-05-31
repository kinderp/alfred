/* ============================================================================
 * main_demo.c - standalone core correlator demonstration
 *
 * This example feeds synthetic alfred_raw_event_t records directly into the
 * core without using the inotify backend. It is useful for reading and
 * experimenting with the core API in isolation, but it is not the production
 * runtime path.
 *
 * The real application flow is:
 *
 *   inotify_backend.c -> alfred_raw_event_t -> alfred_process()
 *
 * This example starts at the alfred_raw_event_t step.
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alfred_correlator.h"

/* ============================================================================
 * SIMPLE PRINT CALLBACK
 * ========================================================================== */

/*
 * on_event - print one semantic event emitted by the demo engine
 * @ev: semantic event emitted by the core
 * @userdata: unused demo context
 *
 * Real applications would usually replace this with a logger, indexer,
 * database writer, network stream, or test collector.
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

/* ============================================================================
 * HELPERS TO CREATE RAW EVENTS
 * ========================================================================== */

/*
 * mk_event - build a synthetic raw event for the demo
 * @mask: ALFRED_RAW_* flags
 * @path: event path
 * @cookie: move correlation cookie, or 0
 * @pid: synthetic process id
 *
 * The timestamp is intentionally fixed. That keeps the example deterministic,
 * but it also means the demo is not a realistic timing test for debounce or
 * move timeout behavior.
 *
 * Return: initialized raw event value.
 */
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

/* ============================================================================
 * MAIN DEMO
 * ========================================================================== */

/*
 * main - run the standalone core demonstration
 *
 * Return: 0 on success, 1 when the engine cannot be created.
 */
int main(void)
{
    alfred_config_t cfg;
    alfred_engine_t *eng;

    /* ========================================================================
     * CONFIGURATION
     * ====================================================================== */

    alfred_config_default(&cfg);

    cfg.move_timeout_ms = 200;
    cfg.modify_debounce_ms = 100;
    cfg.create_ready_ms = 300;

    /* ========================================================================
     * ENGINE INIT
     * ====================================================================== */

    eng = alfred_create(&cfg, on_event, NULL);

    if (!eng) {
        fprintf(stderr, "failed to create engine\n");
        return 1;
    }

    /* ========================================================================
     * 1. FILE CREATE + READY SEQUENCE
     * ====================================================================== */

    alfred_process(eng,
        &mk_event(ALFRED_RAW_CREATE, "/tmp/a.txt", 0, 100));

    alfred_process(eng,
        &mk_event(ALFRED_RAW_CLOSE_WRITE, "/tmp/a.txt", 0, 100));

    /* ========================================================================
     * 2. MODIFY STORM (should be debounced)
     * ====================================================================== */

    for (int i = 0; i < 5; i++) {
        alfred_process(eng,
            &mk_event(ALFRED_RAW_MODIFY, "/tmp/a.txt", 0, 100));
    }

    /* ========================================================================
     * 3. RENAME SEQUENCE (MOVE_FROM + MOVE_TO)
     * ====================================================================== */

    alfred_process(eng,
        &mk_event(ALFRED_RAW_MOVED_FROM, "/tmp/a.txt", 55, 100));

    alfred_process(eng,
        &mk_event(ALFRED_RAW_MOVED_TO, "/home/a.txt", 55, 100));

    /* ========================================================================
     * 4. DELETE EVENT
     * ====================================================================== */

    alfred_process(eng,
        &mk_event(ALFRED_RAW_DELETE, "/home/a.txt", 0, 100));

    /* ========================================================================
     * FLUSH + CLEANUP
     * ====================================================================== */

    alfred_flush(eng);
    alfred_destroy(eng);

    return 0;
}
