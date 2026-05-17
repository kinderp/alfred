#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h> // uint32_t
void iso_timestamp(char *buf, size_t len);

int path_join(char *dst,
              size_t len,
              const char *base,
              const char *name);

void raw_event_name_from_mask(uint32_t mask,
              char *dest,
              size_t dest_size);

#endif
