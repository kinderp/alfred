/* ============================================================================
 * include/watch_manager.h
 * ========================================================================== */

#ifndef WATCH_MANAGER_H
#define WATCH_MANAGER_H

#include <stdint.h>

/* forward declaration */
typedef struct app app_t;

int watch_manager_add(app_t *app,
                      const char *path);

int watch_manager_remove(app_t *app,
                         int wd);

int watch_manager_add_recursive(app_t *app,
                                const char *root);

uint32_t watch_manager_default_mask(void);

#endif
