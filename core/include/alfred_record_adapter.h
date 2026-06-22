/* ============================================================================
 * alfred_record_adapter.h - adapters from current core types to records
 *
 * Event Model v0 introduces alfred_record_t as the common structured shape,
 * while the current runtime still exchanges alfred_raw_event_t and
 * alfred_event_t. This header exposes small conversion helpers that let the
 * project adopt records incrementally without changing runtime behavior all at
 * once.
 * ========================================================================== */

#ifndef ALFRED_RECORD_ADAPTER_H
#define ALFRED_RECORD_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_correlator.h"
#include "alfred_record.h"

/*
 * alfred_record_from_raw - convert one current raw event into a v0 record
 * @raw: current backend/core raw event to describe
 * @out: destination record written by this function
 *
 * The output record is a normalized raw filesystem record. It borrows all
 * string memory from @raw, especially raw->path. The caller must therefore keep
 * @raw and its pointed-to storage alive for as long as @out is consumed.
 *
 * The adapter deliberately preserves raw facts instead of promoting them to
 * semantic outcomes. For example ALFRED_RAW_MOVED_FROM becomes
 * ALFRED_RECORD_TYPE_RAW_MOVED_FROM, not FILE_MOVED or DIR_RENAMED.
 *
 * A valid raw record contains exactly one primary ALFRED_RAW_* action bit.
 * ALFRED_RAW_ISDIR is the only accepted qualifier and is preserved in raw_mask
 * instead of changing the record type. Ambiguous masks with multiple action bits
 * are rejected so future producers cannot accidentally depend on priority-based
 * interpretation.
 *
 * Return: 0 on success, -1 on invalid input or unsupported raw mask.
 */
int alfred_record_from_raw(const alfred_raw_event_t *raw,
                           alfred_record_t *out);

/*
 * alfred_record_from_event - convert one semantic core event into a v0 record
 * @event: current semantic core event to describe
 * @out: destination record written by this function
 *
 * The output record is a semantic filesystem record. It borrows all string
 * memory from @event, especially event->src_path and event->dst_path. The
 * caller must keep @event and its pointed-to storage alive while @out is
 * consumed.
 *
 * Return: 0 on success, -1 on invalid input or unsupported event type.
 */
int alfred_record_from_event(const alfred_event_t *event,
                             alfred_record_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_ADAPTER_H */
