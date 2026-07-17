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

typedef enum {
    ALFRED_CLI_RUNTIME = 0,
    ALFRED_CLI_HELP,
    ALFRED_CLI_VERSION,
    ALFRED_CLI_CHECK_CONFIG
} alfred_cli_command_t;

typedef struct {
    alfred_cli_command_t command;
    const char *config_path;
    int path_count;
    char **paths;
} alfred_cli_options_t;

static int alfred_parse_args(int argc,
                             char **argv,
                             alfred_cli_options_t *options,
                             FILE *err);
static int alfred_set_exclusive_command(alfred_cli_options_t *options,
                                        alfred_cli_command_t command,
                                        const char *name,
                                        int seen_config,
                                        FILE *err);
static int alfred_report_parse_error(FILE *err,
                                     const char *message,
                                     const char *arg);

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
    fprintf(out, "  -c FILE        Load configuration from FILE.\n");
    fprintf(out, "  --config FILE  Load configuration from FILE.\n");
    fprintf(out, "  --help          Show this help and exit.\n");
    fprintf(out, "  --version       Show version information and exit.\n");
    fprintf(out, "  --check-config  Validate configuration and exit.\n");
    fprintf(out, "  --              Treat following arguments as paths.\n");
    fprintf(out, "\n");
    fprintf(out, "Configuration:\n");
    fprintf(out, "  ALFRED_CONFIG=FILE  Load a key=value configuration file.\n");
    fprintf(out, "  Explicit -c/--config wins over ALFRED_CONFIG.\n");
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
 * @config_path: optional explicit CLI configuration file, or NULL
 *
 * This command mirrors the lightweight configuration part of app_init() but
 * deliberately stops before workspace/session publication, logger setup,
 * backend/core/output initialization, signal handlers, and watch installation.
 * It validates defaults, optional configuration selected by the parser, and
 * optional ALFRED_EVENT_ENGINE with the same helpers used by normal startup.
 * When @config_path is present it wins over ALFRED_CONFIG.
 *
 * Return: 0 when configuration is valid, 1 when validation fails.
 */
static int alfred_check_config(FILE *out, FILE *err, const char *config_path)
{
    config_t config;

    config_defaults(&config);

    if (config_path != NULL) {
        if (config_load(&config, config_path) != ERR_OK) {
            fprintf(err, "invalid --config=%s\n", config_path);
            return 1;
        }
    }
    else {
        const char *env_config_path = getenv("ALFRED_CONFIG");
        if (env_config_path != NULL &&
            config_load(&config, env_config_path) != ERR_OK) {

            fprintf(err, "invalid ALFRED_CONFIG=%s\n", env_config_path);
            return 1;
        }
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
 * alfred_report_parse_error - write one CLI parser error
 * @err: destination stream for error messages
 * @message: human-readable reason
 * @arg: optional argument connected to the reason
 *
 * Parser errors are emitted before app_init(), so they must not create runtime
 * logs. Keep the shape short and stable enough for CLI tests.
 *
 * Return: always -1 for convenient parser propagation.
 */
static int alfred_report_parse_error(FILE *err,
                                     const char *message,
                                     const char *arg)
{
    if (arg != NULL) {
        fprintf(err, "alfred: %s '%s'\n", message, arg);
    }
    else {
        fprintf(err, "alfred: %s\n", message);
    }

    fprintf(err, "Try 'alfred --help' for usage.\n");
    return -1;
}

/*
 * alfred_set_exclusive_command - record a no-runtime CLI command
 * @options: parser result to update
 * @command: requested no-runtime command
 * @name: command text for diagnostics
 * @seen_config: true when -c/--config was already parsed
 * @err: destination stream for parser errors
 *
 * --help and --version are deliberately exclusive: they do not load
 * configuration and do not combine with paths or -c/--config. --check-config is
 * also a no-runtime command, but it may be combined with one explicit config
 * file so users can validate a file before starting Alfred.
 *
 * Return: 0 on success, -1 on parser error.
 */
static int alfred_set_exclusive_command(alfred_cli_options_t *options,
                                        alfred_cli_command_t command,
                                        const char *name,
                                        int seen_config,
                                        FILE *err)
{
    if (options->command != ALFRED_CLI_RUNTIME) {
        return alfred_report_parse_error(err,
                                         "duplicate no-runtime command",
                                         name);
    }

    if ((command == ALFRED_CLI_HELP || command == ALFRED_CLI_VERSION) &&
        seen_config) {
        return alfred_report_parse_error(err,
                                         "option cannot be combined with",
                                         name);
    }

    options->command = command;
    return 0;
}

/*
 * alfred_parse_args - parse the CLI v0 grammar
 * @argc: command-line argument count
 * @argv: command-line argument vector
 * @options: destination parser result
 * @err: destination stream for parse errors
 *
 * The parser only answers process-level questions: which no-runtime command,
 * which optional config file, and which tokens are startup paths. It does not
 * initialize logging, backend, core, output, or policy state.
 *
 * Return: 0 on success, -1 on parser error.
 */
static int alfred_parse_args(int argc,
                             char **argv,
                             alfred_cli_options_t *options,
                             FILE *err)
{
    int seen_config = 0;

    if (options == NULL || argv == NULL) {
        return alfred_report_parse_error(err,
                                         "invalid argument vector",
                                         NULL);
    }

    memset(options, 0, sizeof(*options));
    options->command = ALFRED_CLI_RUNTIME;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--") == 0) {
            if (options->command != ALFRED_CLI_RUNTIME) {
                return alfred_report_parse_error(
                    err,
                    "paths are not allowed for",
                    arg);
            }

            if (i + 1 >= argc) {
                return alfred_report_parse_error(
                    err,
                    "expected at least one path after",
                    arg);
            }

            options->paths = &argv[i + 1];
            options->path_count = argc - i - 1;
            return 0;
        }

        if (strcmp(arg, "-c") == 0 || strcmp(arg, "--config") == 0) {
            if (seen_config) {
                return alfred_report_parse_error(
                    err,
                    "duplicate configuration option",
                    arg);
            }

            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                return alfred_report_parse_error(
                    err,
                    "option requires a value",
                    arg);
            }

            if (options->command == ALFRED_CLI_HELP ||
                options->command == ALFRED_CLI_VERSION) {
                return alfred_report_parse_error(
                    err,
                    "option cannot be combined with",
                    arg);
            }

            options->config_path = argv[i + 1];
            seen_config = 1;
            i++;
            continue;
        }

        if (strcmp(arg, "--help") == 0) {
            if (alfred_set_exclusive_command(options,
                                             ALFRED_CLI_HELP,
                                             arg,
                                             seen_config,
                                             err) != 0) {
                return -1;
            }
            continue;
        }

        if (strcmp(arg, "--version") == 0) {
            if (alfred_set_exclusive_command(options,
                                             ALFRED_CLI_VERSION,
                                             arg,
                                             seen_config,
                                             err) != 0) {
                return -1;
            }
            continue;
        }

        if (strcmp(arg, "--check-config") == 0) {
            if (alfred_set_exclusive_command(options,
                                             ALFRED_CLI_CHECK_CONFIG,
                                             arg,
                                             0,
                                             err) != 0) {
                return -1;
            }
            continue;
        }

        if (strcmp(arg, "--print-config") == 0) {
            return alfred_report_parse_error(err,
                                             "unsupported option",
                                             arg);
        }

        if (arg[0] == '-') {
            return alfred_report_parse_error(err,
                                             "unknown option",
                                             arg);
        }

        if (options->command != ALFRED_CLI_RUNTIME) {
            return alfred_report_parse_error(err,
                                             "paths are not allowed for",
                                             arg);
        }

        options->paths = &argv[i];
        options->path_count = argc - i;

        for (int j = i; j < argc; j++) {
            if (argv[j][0] == '-') {
                return alfred_report_parse_error(
                    err,
                    "unexpected option after path",
                    argv[j]);
            }
        }

        return 0;
    }

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
    alfred_cli_options_t options;

    if (alfred_parse_args(argc, argv, &options, stderr) != 0) {
        return 1;
    }

    if (options.command == ALFRED_CLI_HELP) {
        alfred_print_usage(stdout);
        return 0;
    }

    if (options.command == ALFRED_CLI_VERSION) {
        alfred_print_version(stdout);
        return 0;
    }

    if (options.command == ALFRED_CLI_CHECK_CONFIG) {
        return alfred_check_config(stdout, stderr, options.config_path);
    }

    int rc = app_init_from_paths(&app,
                                 options.path_count,
                                 options.paths,
                                 options.config_path);

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
