/* ============================================================================
 * alfred_record_owned.h - owned copies for Event Model v0 records
 *
 * alfred_record_t is the common record view used by adapters, diagnostics, and
 * synchronous sinks. Many of its string fields are borrowed. This header exposes
 * the minimal ownership bridge needed before records can safely cross an
 * asynchronous queue, dispatcher thread, or delayed writer boundary.
 * ========================================================================== */

#ifndef ALFRED_RECORD_OWNED_H
#define ALFRED_RECORD_OWNED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"

/*
 * alfred_record_clone_owned - deep-copy the string fields of one record
 * @src: borrowed/source record to clone
 * @dst: destination record overwritten by this function on success
 *
 * The function copies the whole scalar record first, then duplicates every
 * currently defined string pointer: backend, filesystem paths, OS error strings,
 * watch state/reason/error, and recovery detail_path. On success, @dst is an
 * alfred_record_t whose string pointers are independent from @src and can live
 * after the producer stack frame or backend buffer expires.
 *
 * This helper is intentionally not wired into the runtime hot path yet. It is a
 * contract and test step for the future queue/dispatcher boundary. Once a record
 * is enqueued or handed to another thread, producers should use an owned copy or
 * a later benchmark-driven storage strategy with the same lifetime guarantees.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
int alfred_record_clone_owned(const alfred_record_t *src,
                              alfred_record_t *dst);

/*
 * alfred_record_destroy_owned - release strings owned by a cloned record
 * @record: record previously initialized by alfred_record_clone_owned()
 *
 * This function frees only the string fields that alfred_record_clone_owned()
 * duplicates, then clears the whole struct to make repeated cleanup patterns
 * easier to audit. It must not be called on a purely borrowed record produced by
 * adapters or diagnostic builders.
 */
void alfred_record_destroy_owned(alfred_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_OWNED_H */
