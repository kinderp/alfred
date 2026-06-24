/* ============================================================================
 * alfred_record_owned.c - owned copies for Event Model v0 records
 * ========================================================================== */

#include "alfred_record_owned.h"

#include <stdlib.h>
#include <string.h>

/*
 * duplicate_optional_string - duplicate a nullable borrowed string
 * @src: borrowed string pointer, or NULL
 *
 * Return: allocated copy, NULL when @src is NULL or allocation fails.
 */
static char *duplicate_optional_string(const char *src)
{
    char *copy;
    size_t len;

    if (src == NULL) {
        return NULL;
    }

    len = strlen(src) + 1u;
    copy = malloc(len);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, src, len);
    return copy;
}

/*
 * clone_string_field - clone one nullable string field
 * @src: borrowed source field
 * @dst: destination field inside the owned record
 *
 * A NULL source is a successful empty clone. A non-NULL source must allocate.
 *
 * Return: 0 on success, -1 on allocation failure.
 */
static int clone_string_field(const char *src, const char **dst)
{
    char *copy;

    if (dst == NULL) {
        return -1;
    }

    *dst = NULL;
    if (src == NULL) {
        return 0;
    }

    copy = duplicate_optional_string(src);
    if (copy == NULL) {
        return -1;
    }

    *dst = copy;
    return 0;
}

void alfred_record_destroy_owned(alfred_record_t *record)
{
    if (record == NULL) {
        return;
    }

    free((void *)record->backend);
    free((void *)record->path);
    free((void *)record->old_path);
    free((void *)record->new_path);
    free((void *)record->os_error.name);
    free((void *)record->os_error.message);
    free((void *)record->watch.state);
    free((void *)record->watch.reason);
    free((void *)record->watch.error);
    free((void *)record->watch.event_mask);
    free((void *)record->watch.event_name);
    free((void *)record->recovery.detail_path);

    memset(record, 0, sizeof(*record));
}

int alfred_record_clone_owned(const alfred_record_t *src,
                              alfred_record_t *dst)
{
    alfred_record_t clone;

    if (src == NULL || dst == NULL) {
        return -1;
    }

    clone = *src;
    clone.backend = NULL;
    clone.path = NULL;
    clone.old_path = NULL;
    clone.new_path = NULL;
    clone.os_error.name = NULL;
    clone.os_error.message = NULL;
    clone.watch.state = NULL;
    clone.watch.reason = NULL;
    clone.watch.error = NULL;
    clone.watch.event_mask = NULL;
    clone.watch.event_name = NULL;
    clone.recovery.detail_path = NULL;

    if (clone_string_field(src->backend, &clone.backend) != 0 ||
        clone_string_field(src->path, &clone.path) != 0 ||
        clone_string_field(src->old_path, &clone.old_path) != 0 ||
        clone_string_field(src->new_path, &clone.new_path) != 0 ||
        clone_string_field(src->os_error.name, &clone.os_error.name) != 0 ||
        clone_string_field(src->os_error.message, &clone.os_error.message) != 0 ||
        clone_string_field(src->watch.state, &clone.watch.state) != 0 ||
        clone_string_field(src->watch.reason, &clone.watch.reason) != 0 ||
        clone_string_field(src->watch.error, &clone.watch.error) != 0 ||
        clone_string_field(src->watch.event_mask,
                           &clone.watch.event_mask) != 0 ||
        clone_string_field(src->watch.event_name,
                           &clone.watch.event_name) != 0 ||
        clone_string_field(src->recovery.detail_path,
                           &clone.recovery.detail_path) != 0) {
        alfred_record_destroy_owned(&clone);
        return -1;
    }

    *dst = clone;
    return 0;
}
