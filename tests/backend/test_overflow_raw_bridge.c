/*
 * test_overflow_raw_bridge.c - focused test for IN_Q_OVERFLOW raw bridging
 *
 * IN_Q_OVERFLOW is a global inotify-instance event: the kernel reports it with
 * wd=-1, so there is no watcher-table path to use. This test includes
 * inotify_backend.c directly and exercises the static bridge helper that turns
 * the kernel fact into ALFRED_RAW_OVERFLOW for the core.
 *
 * Expected log contract:
 *
 * raw.log:
 * - RAW_OVERFLOW path= mask=128
 *
 * events log stream:
 * - none; this test stops at the raw backend/core boundary
 *
 * Expected raw event:
 * - source=ALFRED_SRC_INOTIFY
 * - mask=ALFRED_RAW_OVERFLOW
 * - path=""
 *
 * Meaning:
 * Overflow is not tied to a single watched directory. Alfred must still notify
 * the core that the stream is incomplete, but full recovery/resync remains a
 * separate policy.
 */

#include <assert.h>
#include <string.h>
#include <sys/inotify.h>

#include "../../core/include/alfred_record_adapter.h"
#include "../../core/include/alfred_record_text.h"

#include "../../modules/inotify/src/inotify_backend.c"

static void test_overflow_event_builds_global_raw(void)
{
    struct inotify_event ev;
    alfred_raw_event_t raw;
    alfred_record_t record;
    char text[128];

    memset(&ev, 0, sizeof(ev));
    ev.wd = -1;
    ev.mask = IN_Q_OVERFLOW;

    assert(backend_build_overflow_raw(&ev, &raw) == 0);
    assert(raw.source == ALFRED_SRC_INOTIFY);
    assert(raw.mask == ALFRED_RAW_OVERFLOW);
    assert(raw.cookie == 0);
    assert(raw.pid == 0);
    assert(raw.path != NULL);
    assert(strcmp(raw.path, "") == 0);
    assert(raw.ts_ns != 0);

    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert(record.type == ALFRED_RECORD_TYPE_RAW_OVERFLOW);
    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text, "RAW_OVERFLOW path= mask=128") == 0);
}

static void test_non_overflow_event_is_ignored(void)
{
    struct inotify_event ev;
    alfred_raw_event_t raw;

    memset(&ev, 0, sizeof(ev));
    memset(&raw, 0x7f, sizeof(raw));

    ev.wd = 3;
    ev.mask = IN_CREATE;

    assert(backend_build_overflow_raw(&ev, &raw) != 0);
}

int main(void)
{
    test_overflow_event_builds_global_raw();
    test_non_overflow_event_is_ignored();

    return 0;
}
