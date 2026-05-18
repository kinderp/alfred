/* ============================================================================
 * include/watch_manager.h
 * ========================================================================== */

#ifndef WATCH_MANAGER_H
#define WATCH_MANAGER_H

#include <stdint.h>

/* forward declaration */
struct app;

int watch_manager_add(struct app *app,
                      const char *path);

int watch_manager_remove(struct app *app,
                         int wd);

int watch_manager_add_recursive(struct app *app,
                                const char *root);

uint32_t watch_manager_default_mask(void);

#endif
