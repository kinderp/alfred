#ifndef ALFRED_UTILS_H
#define ALFRED_UTILS_H

#include <stddef.h>
#include <stdint.h>

uint64_t alfred_now_ns(void);
uint64_t alfred_ms_to_ns(uint64_t ms);

char *alfred_strdup(const char *src);

uint32_t alfred_hash_u32(uint32_t x);
uint32_t alfred_hash_str(const char *s);

const char *alfred_basename_ptr(const char *path);
void alfred_parent_dir(const char *path, char *out, size_t out_sz);

int alfred_same_parent(const char *a, const char *b);
int alfred_same_basename(const char *a, const char *b);
int alfred_streq(const char *a, const char *b);

const char *alfred_version_string_impl(void);

#endif /* ALFRED_UTILS_H */
