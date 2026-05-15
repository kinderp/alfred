#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

void iso_timestamp(char *buf, size_t len);

int path_join(char *dst,
              size_t len,
              const char *base,
              const char *name);

#endif
