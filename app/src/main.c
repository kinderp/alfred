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
#include <stdlib.h>
#include <string.h>

/*
 * alfred_print_usage - write the short informational CLI usage
 * @out: destination stream, normally stdout
 *
 * Informational commands are handled before app_init() so they do not load
 * configuration, initialize loggers, open inotify, create output pipelines, or
 * install watches. This keeps --help safe and side-effect free.
 */
static void alfred_print_usage(FILE *out)
{
    fprintf(out, "Usage: alfred [OPTIONS] PATH...\n");
    fprintf(out, "\n");
    fprintf(out, "Watch one or more directory roots with the current ");
    fprintf(out, "Linux inotify backend.\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --help          Show this help and exit.\n");
    fprintf(out, "  --version       Show version information and exit.\n");
    fprintf(out, "  --check-config  Validate configuration and exit.\n");
    fprintf(out, "\n");
    fprintf(out, "Configuration:\n");
    fprintf(out, "  ALFRED_CONFIG=FILE  Load a key=value configuration file.\n");
}

/*
 * alfred_print_version - write process version information
 * @out: destination stream, normally stdout
 *
 * The executable reports the public core semantic version for v0. A future
 * packaging step can add a separate product/build version if needed.
 */
static void alfred_print_version(FILE *out)
{
    fprintf(out, "alfred %s\n", alfred_version_string());
}

/*
 * alfred_check_config - validate the current configuration-only contract
 * @out: destination stream for success messages, normally stdout
 * @err: destination stream for error messages, normally stderr
 *
 * This command mirrors the lightweight configuration part of app_init() but
 * deliberately stops before workspace/session publication, logger setup,
 * backend/core/output initialization, signal handlers, and watch installation.
 * It validates defaults, optional ALFRED_CONFIG, and optional
 * ALFRED_EVENT_ENGINE with the same helpers used by normal startup.
 *
 * Return: 0 when configuration is valid, 1 when validation fails.
 */
static int alfred_check_config(FILE *out, FILE *err)
{
    config_t config;

    config_defaults(&config);

    const char *config_path = getenv("ALFRED_CONFIG");
    if (config_path != NULL &&
        config_load(&config, config_path) != ERR_OK) {

        fprintf(err, "invalid ALFRED_CONFIG=%s\n", config_path);
        return 1;
    }

    const char *event_engine_env = getenv("ALFRED_EVENT_ENGINE");
    if (event_engine_env != NULL &&
        config_set_event_engine(&config, event_engine_env) != ERR_OK) {

        fprintf(err,
                "invalid ALFRED_EVENT_ENGINE=%s (expected core)\n",
                event_engine_env);
        return 1;
    }

    fprintf(out, "configuration OK\n");
    return 0;
}

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

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        alfred_print_usage(stdout);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        alfred_print_version(stdout);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--check-config") == 0) {
        return alfred_check_config(stdout, stderr);
    }

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
