/* ============================================================================
 * main.c - alfred process entry point
 *
 * The entry point is intentionally small. All runtime ownership and shutdown
 * ordering live in the application layer so startup failures and normal exits
 * use the same lifecycle functions.
 * ============================================================================
 */

#include "app.h"

#include <stdio.h>

/*
 * main - initialize, run, and shut down alfred
 * @argc: command-line argument count
 * @argv: command-line argument vector
 *
 * Delegates all real work to the application lifecycle API. This keeps the
 * process entry point free of subsystem details.
 *
 * Return: 0 on success, 1 on startup failure, or the app_run() return code.
 */
int main(int argc, char **argv)
{
    app_t app;
    int shutdown_rc;

    int rc = app_init(&app, argc, argv);

    if (rc != 0) {
        fprintf(stderr, "startup failed\n");
        (void)app_shutdown(&app);
        return 1;
    }

    rc = app_run(&app);

    shutdown_rc = app_shutdown(&app);

    if (rc != 0) {
        return rc;
    }

    if (shutdown_rc != 0) {
        return 1;
    }

    return 0;
}
