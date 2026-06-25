/*
 * test_overflow_raw_jsonl.c - synthetic JSONL golden for IN_Q_OVERFLOW
 *
 * IN_Q_OVERFLOW is not practical to trigger as a stable end-to-end test: it
 * depends on kernel queue pressure and timing. This helper therefore follows
 * the same stable boundary used by the backend bridge test:
 *
 *   struct inotify_event(IN_Q_OVERFLOW, wd=-1)
 *   -> backend_build_overflow_raw()
 *   -> alfred_record_from_raw()
 *   -> alfred_record_format_jsonl()
 *
 * Expected JSONL contract:
 * - layer=normalized_raw
 * - category=filesystem
 * - type=RAW_OVERFLOW
 * - source=ALFRED_SRC_INOTIFY
 * - raw_mask=ALFRED_RAW_OVERFLOW
 * - path=""
 *
 * Meaning:
 * Queue overflow is global to the inotify instance, not to one watched path.
 * Alfred must expose that stream-integrity loss as a raw structured record
 * without inventing a filesystem object path.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>

#include "../../core/include/alfred_record_adapter.h"
#include "../../core/include/alfred_record_jsonl.h"

#include "../../modules/inotify/src/inotify_backend.c"

int main(void)
{
    struct inotify_event ev;
    alfred_raw_event_t raw;
    alfred_record_t record;
    char jsonl[512];

    memset(&ev, 0, sizeof(ev));
    ev.wd = -1;
    ev.mask = IN_Q_OVERFLOW;

    assert(backend_build_overflow_raw(&ev, &raw) == 0);
    assert(raw.source == ALFRED_SRC_INOTIFY);
    assert(raw.mask == ALFRED_RAW_OVERFLOW);
    assert(raw.cookie == 0u);
    assert(raw.path != NULL);
    assert(strcmp(raw.path, "") == 0);

    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert(record.layer == ALFRED_RECORD_LAYER_NORMALIZED_RAW);
    assert(record.category == ALFRED_RECORD_CATEGORY_FILESYSTEM);
    assert(record.type == ALFRED_RECORD_TYPE_RAW_OVERFLOW);
    assert(record.path != NULL);
    assert(strcmp(record.path, "") == 0);

    assert(alfred_record_format_jsonl(&record, jsonl, sizeof(jsonl)) > 0);
    puts(jsonl);

    return 0;
}
