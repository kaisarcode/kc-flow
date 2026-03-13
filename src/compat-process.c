/**
 * compat-process.c
 * Summary: Process launch and environment binding for kc-flow node execs.
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
 * Normalizes one parameter key into an environment suffix.
 * @param text Source text.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @return void
 */
static void kc_flow_platform_param_env_name(const char *text, char *buffer, size_t size) {
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
 * Exports one scoped parameter family.
 * @param prefix Environment prefix.
 * @param params Parameter store.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_platform_export_params(
    const char *prefix,
    const struct kc_flow_overrides *params
) {
    size_t i;

    for (i = 0; i < params->count; ++i) {
        char key[192];
        char suffix[128];

        kc_flow_platform_param_env_name(params->records[i].key, suffix, sizeof(suffix));
        snprintf(key, sizeof(key), "%s%s", prefix, suffix);
        if (kc_flow_platform_set_env(key, params->records[i].value) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * Exports runtime paths, descriptors, and scope params.
 * @param flow_path Current flow path.
 * @param flow_params Current flow params.
 * @param node_params Current node params.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_platform_export_env(
    const char *flow_path,
    const struct kc_flow_overrides *flow_params,
    const struct kc_flow_overrides *node_params,
    int fd_in,
    int fd_out
) {
    char value[32];
    char dir[KC_FLOW_MAX_PATH];

    if (flow_path != NULL && flow_path[0] != '\0') {
        if (kc_flow_platform_set_env("KC_FLOW_FILE", flow_path) != 0) {
            return -1;
        }
        kc_flow_dirname(flow_path, dir, sizeof(dir));
        if (kc_flow_platform_set_env("KC_FLOW_DIR", dir) != 0) {
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
    if (kc_flow_platform_export_params("KC_FLOW_FLOW_PARAM_", flow_params) != 0 ||
            kc_flow_platform_export_params("KC_FLOW_NODE_PARAM_", node_params) != 0 ||
            kc_flow_platform_export_params("KC_FLOW_PARAM_", node_params) != 0) {
        return -1;
    }
    return 0;
}
#endif

/**
 * Runs one resolved node exec command.
 * @param flow_path Current flow path.
 * @param command Resolved shell command.
 * @param flow_params Current flow params.
 * @param node_params Current node params.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_platform_run_command(
    const char *flow_path,
    const char *command,
    const struct kc_flow_overrides *flow_params,
    const struct kc_flow_overrides *node_params,
    int fd_in,
    int fd_out,
    char *error,
    size_t error_size
) {
#if defined(_WIN32)
    int rc;

    (void)flow_path;
    (void)flow_params;
    (void)node_params;
    (void)fd_in;
    (void)fd_out;
    rc = system(command);
    if (rc != 0) {
        snprintf(error, error_size, "Exec exited with non-zero status.");
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
        if (kc_flow_platform_export_env(flow_path, flow_params, node_params, fd_in, fd_out) != 0) {
            _exit(127);
        }
        execl("/bin/sh", "sh", "-lc", command, (char *)NULL);
        _exit(127);
    }
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        snprintf(error, error_size, "Exec exited with non-zero status.");
        return -1;
    }
    return 0;
#endif
}
