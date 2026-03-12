/**
 * compat-process.c
 * Summary: Contract process platform primitives for kc-flow.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "flow.h"
#include "compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

#if !defined(_WIN32)

/**
 * Normalizes one parameter id into one environment suffix.
 * @param text Source text.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @return void
 */
static void kc_flow_platform_param_env_name(
    const char *text,
    char *buffer,
    size_t size
) {
    size_t i;

    for (i = 0; i + 1 < size && text[i] != '\0'; ++i) {
        if (isalnum((unsigned char)text[i])) {
            buffer[i] = (char)toupper((unsigned char)text[i]);
        } else {
            buffer[i] = '_';
        }
    }
    buffer[i] = '\0';
}

/**
 * Exports one environment variable.
 * @param key Environment key.
 * @param value Environment value.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_platform_set_env(const char *key, const char *value) {
    return setenv(key, value, 1);
}

/**
 * Exports one runtime path variable.
 * @param key Environment key.
 * @param value Path value.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_platform_set_path_env(const char *key, const char *value) {
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    return kc_flow_platform_set_env(key, value);
}

/**
 * Exports runtime descriptors, runtime paths, and node parameters.
 * @param cfg_path Source file path.
 * @param overrides Effective node parameters.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_platform_export_env(
    const char *cfg_path,
    const struct kc_flow_overrides *overrides,
    int fd_in,
    int fd_out
) {
    size_t i;
    char value[32];
    char dir[KC_FLOW_MAX_PATH];

    if (kc_flow_platform_set_path_env("KC_FLOW_FILE", cfg_path) != 0) {
        return -1;
    }
    if (cfg_path != NULL && cfg_path[0] != '\0') {
        kc_flow_dirname(cfg_path, dir, sizeof(dir));
        if (kc_flow_platform_set_path_env("KC_FLOW_DIR", dir) != 0) {
            return -1;
        }
    }
    if (fd_in >= 0) {
        snprintf(value, sizeof(value), "%d", fd_in);
        if (kc_flow_platform_set_env("KC_FLOW_FD_IN", value) != 0) {
            return -1;
        }
    }
    if (fd_out >= 0) {
        snprintf(value, sizeof(value), "%d", fd_out);
        if (kc_flow_platform_set_env("KC_FLOW_FD_OUT", value) != 0) {
            return -1;
        }
    }
    for (i = 0; i < overrides->count; ++i) {
        char key[160];
        char suffix[128];

        if (strncmp(overrides->records[i].key, "param.", 6) != 0) {
            continue;
        }
        kc_flow_platform_param_env_name(
            overrides->records[i].key + 6,
            suffix,
            sizeof(suffix)
        );
        snprintf(key, sizeof(key), "KC_FLOW_PARAM_%s", suffix);
        if (kc_flow_platform_set_env(key, overrides->records[i].value) != 0) {
            return -1;
        }
    }
    return 0;
}
#endif

/**
 * Runs one atomic contract command.
 * @param command Resolved shell command.
 * @param overrides Effective node parameters.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_platform_run_contract(
    const char *cfg_path,
    const char *command,
    const struct kc_flow_overrides *overrides,
    int fd_in,
    int fd_out,
    char *error,
    size_t error_size
) {
#if defined(_WIN32)
    int rc;

    (void)cfg_path;
    (void)overrides;
    (void)fd_in;
    (void)fd_out;
    rc = system(command);
    if (rc != 0) {
        snprintf(error, error_size, "Contract exited with non-zero status.");
        return -1;
    }
    return 0;
#else
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        snprintf(error, error_size, "Unable to fork runtime process.");
        return -1;
    }
    if (pid == 0) {
        if (kc_flow_platform_export_env(cfg_path, overrides, fd_in, fd_out) != 0) {
            _exit(127);
        }
        execl("/bin/sh", "sh", "-lc", command, (char *)NULL);
        _exit(127);
    }
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        snprintf(error, error_size, "Contract exited with non-zero status.");
        return -1;
    }
    return 0;
#endif
}
