/**
 * main.c
 * Summary: CLI entrypoint for kc-flow branch runtime.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * Detects the default worker count.
 * @return size_t One or more workers.
 */
static size_t kc_flow_default_workers(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? (size_t)info.dwNumberOfProcessors : 1;
#else
    long value;

    value = sysconf(_SC_NPROCESSORS_ONLN);
    return value > 0 ? (size_t)value : 1;
#endif
}

/**
 * Parses one strict worker count.
 * @param text Source text.
 * @param workers Output worker count.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_parse_workers(const char *text, size_t *workers) {
    size_t value;
    const char *cursor;

    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    value = 0;
    cursor = text;
    while (*cursor != '\0') {
        if (*cursor < '0' || *cursor > '9') {
            return -1;
        }
        value = value * 10 + (size_t)(*cursor - '0');
        cursor++;
    }
    if (value == 0) {
        return -1;
    }
    *workers = value;
    return 0;
}

/**
 * Parses one strict descriptor value.
 * @param text Source text.
 * @param fd Output descriptor.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_parse_fd(const char *text, int *fd) {
    int value;
    const char *cursor;

    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    value = 0;
    cursor = text;
    while (*cursor != '\0') {
        if (*cursor < '0' || *cursor > '9') {
            return -1;
        }
        value = value * 10 + (*cursor - '0');
        cursor++;
    }
    *fd = value;
    return 0;
}

/**
 * Normalizes one CLI override key into flow scope.
 * @param source Source key text.
 * @param target Output key buffer.
 * @param size Output key size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_cli_override_key(const char *source, char *target, size_t size) {
    char buffer[256];

    if (strncmp(source, "flow.param.", 11) == 0) {
        if (snprintf(buffer, sizeof(buffer), "%s", source + 11) >= (int)sizeof(buffer)) {
            return -1;
        }
    } else if (strncmp(source, "param.", 6) == 0) {
        if (snprintf(buffer, sizeof(buffer), "%s", source + 6) >= (int)sizeof(buffer)) {
            return -1;
        }
    } else if (snprintf(buffer, sizeof(buffer), "%s", source) >= (int)sizeof(buffer)) {
        return -1;
    }
    if (snprintf(target, size, "%s", buffer) >= (int)size) {
        return -1;
    }
    return 0;
}

/**
 * Prints command help.
 * @param bin Executable name.
 * @return void
 */
static void kc_flow_help(const char *bin) {
    printf(
        "Options:\n"
        "  --run <file>      Execute one flow file\n"
        "  --set key=value   Inject one flow.param override\n"
        "  --workers <n>     Runtime worker count hint\n"
        "  --fd-in <n>       Runtime input descriptor\n"
        "  --fd-out <n>      Runtime output descriptor\n"
        "  --fd-status <n>   Runtime status descriptor\n"
        "  --help            Show help\n\n"
        "Examples:\n"
        "  %s --run /path/to/file.flow\n"
        "  %s --run /path/to/file.flow --set flow.param.hello=Hello\n",
        bin,
        bin
    );
}

/**
 * Fails one CLI invocation.
 * @param bin Executable name.
 * @param message Error message.
 * @return int Always 1.
 */
static int kc_flow_fail(const char *bin, const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    kc_flow_help(bin);
    return 1;
}

/**
 * Program entrypoint.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return int 0 on success; non-zero on failure.
 */
int main(int argc, char **argv) {
    kc_flow_overrides overrides;
    kc_flow_runtime_cfg cfg;
    const char *run_path;
    char error[256];
    int i;

    if (argc <= 1) {
        kc_flow_help(argv[0]);
        return 0;
    }
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) { kc_flow_help(argv[0]); return 0; }
    }
    kc_flow_overrides_init(&overrides);
    cfg.workers = kc_flow_default_workers();
    cfg.fd_in = -1;
    cfg.fd_out = 1;
    cfg.fd_status = -1;
    run_path = NULL;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--run") == 0) {
            if (i + 1 >= argc) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "--run requires a file path.");
            }
            run_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--set") == 0) {
            const char *assignment;
            const char *equals;
            char key[256];
            size_t key_len;

            if (i + 1 >= argc) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "--set requires key=value.");
            }
            assignment = argv[++i];
            equals = strchr(assignment, '=');
            if (equals == NULL || equals == assignment) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Invalid --set assignment.");
            }
            key_len = (size_t)(equals - assignment);
            if (key_len >= sizeof(key)) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "--set key too long.");
            }
            memcpy(key, assignment, key_len);
            key[key_len] = '\0';
            if (kc_flow_cli_override_key(key, key, sizeof(key)) != 0 ||
                    kc_flow_overrides_add(&overrides, key, equals + 1) != 0) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Unable to register override.");
            }
            continue;
        }
        if (strcmp(argv[i], "--workers") == 0) {
            if (i + 1 >= argc || kc_flow_parse_workers(argv[++i], &cfg.workers) != 0) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Invalid value for --workers.");
            }
            continue;
        }
        if (strcmp(argv[i], "--fd-in") == 0) {
            if (i + 1 >= argc || kc_flow_parse_fd(argv[++i], &cfg.fd_in) != 0) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Invalid value for --fd-in.");
            }
            continue;
        }
        if (strcmp(argv[i], "--fd-out") == 0) {
            if (i + 1 >= argc || kc_flow_parse_fd(argv[++i], &cfg.fd_out) != 0) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Invalid value for --fd-out.");
            }
            continue;
        }
        if (strcmp(argv[i], "--fd-status") == 0) {
            if (i + 1 >= argc || kc_flow_parse_fd(argv[++i], &cfg.fd_status) != 0) {
                kc_flow_overrides_free(&overrides);
                return kc_flow_fail(argv[0], "Invalid value for --fd-status.");
            }
            continue;
        }
        kc_flow_overrides_free(&overrides);
        return kc_flow_fail(argv[0], "Unknown argument.");
    }
    if (run_path == NULL) {
        kc_flow_overrides_free(&overrides);
        return kc_flow_fail(argv[0], "Missing --run.");
    }
    {
        kc_flow_model *model;
        int rc;

        if ((model = malloc(sizeof(*model))) == NULL) {
            kc_flow_overrides_free(&overrides);
            return kc_flow_fail(argv[0], "Out of memory.");
        }
        kc_flow_model_init(model);
        if (!kc_flow_file_exists(run_path)) {
            free(model);
            kc_flow_overrides_free(&overrides);
            return kc_flow_fail(argv[0], "flow file not found.");
        }
        if (kc_flow_load_file(run_path, model, error, sizeof(error)) != 0) {
            kc_flow_status_write_run_event(
                cfg.fd_status,
                "run.finished",
                "flow",
                NULL,
                run_path,
                "error",
                error
            );
            fprintf(stderr, "Error: %s\n", error);
            kc_flow_model_free(model);
            free(model);
            kc_flow_overrides_free(&overrides);
            return 1;
        }
        kc_flow_status_write_run_event(cfg.fd_status, "run.started", "flow", model->id, run_path, NULL, NULL);
        rc = kc_flow_run_model(model, &cfg, &overrides, run_path, error, sizeof(error));
        kc_flow_status_write_run_event(cfg.fd_status, "run.finished", "flow", model->id, run_path, rc == 0 ? "ok" : "error", rc == 0 ? NULL : error);
        if (rc != 0) { fprintf(stderr, "Error: %s\n", error); }
        kc_flow_model_free(model);
        free(model);
        kc_flow_overrides_free(&overrides);
        return rc == 0 ? 0 : 1;
    }
}
