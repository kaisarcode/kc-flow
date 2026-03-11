/**
 * process.c
 * Summary: Atomic contract execution runtime with descriptor and env binding.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "flow.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if !defined(_WIN32)

/**
 * Normalizes one parameter id into one environment variable suffix.
 * @param text Source text.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @return void
 */
static void kc_flow_param_env_name(const char *text, char *buffer, size_t size) {
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

#endif

/**
 * Exports one environment variable.
 * @param key Environment key.
 * @param value Environment value.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_set_env(const char *key, const char *value) {
#if defined(_WIN32)
    return _putenv_s(key, value);
#else
    return setenv(key, value, 1);
#endif
}

#if !defined(_WIN32)

/**
 * Exports runtime descriptors and node parameters to the child environment.
 * @param overrides Effective node parameters.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_export_env(
    const kc_flow_overrides *overrides,
    int fd_in,
    int fd_out
) {
    size_t i;
    char value[32];

    if (fd_in >= 0) {
        snprintf(value, sizeof(value), "%d", fd_in);
        if (kc_flow_set_env("KC_FLOW_FD_IN", value) != 0) {
            return -1;
        }
    }
    if (fd_out >= 0) {
        snprintf(value, sizeof(value), "%d", fd_out);
        if (kc_flow_set_env("KC_FLOW_FD_OUT", value) != 0) {
            return -1;
        }
    }
    for (i = 0; i < overrides->count; ++i) {
        char key[160];
        char suffix[128];

        if (strncmp(overrides->records[i].key, "param.", 6) != 0) {
            continue;
        }
        kc_flow_param_env_name(
            overrides->records[i].key + 6,
            suffix,
            sizeof(suffix)
        );
        snprintf(key, sizeof(key), "KC_FLOW_PARAM_%s", suffix);
        if (kc_flow_set_env(key, overrides->records[i].value) != 0) {
            return -1;
        }
    }
    return 0;
}

#endif

/**
 * Opens one null descriptor for child fallback I/O.
 * @param write_mode Non-zero for write access.
 * @return int Descriptor or -1.
 */
static int kc_flow_open_null_fd(int write_mode) {
#if defined(_WIN32)
    return _open("NUL", write_mode ? _O_WRONLY : _O_RDONLY);
#else
    return open("/dev/null", write_mode ? O_WRONLY : O_RDONLY);
#endif
}

/**
 * Executes one contract.
 * @param model Parsed contract.
 * @param overrides Effective parameters.
 * @param cfg_path Source path.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_contract(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *cfg_path,
    int fd_in,
    int fd_out,
    char *error,
    size_t error_size
) {
    char cfg_dir[KC_FLOW_MAX_PATH];
    char workdir[KC_FLOW_MAX_PATH];
    char *resolved_script;

    resolved_script = NULL;
    kc_flow_dirname(cfg_path, cfg_dir, sizeof(cfg_dir));
    if (kc_flow_build_path(
            workdir,
            sizeof(workdir),
            cfg_dir,
            model->runtime_workdir != NULL ? model->runtime_workdir : "."
        ) != 0) {
        snprintf(error, error_size, "Unable to resolve workdir.");
        return -1;
    }
    resolved_script = kc_flow_resolve_template(
        model,
        overrides,
        model->runtime_script,
        error,
        error_size
    );
    if (resolved_script == NULL) {
        return -1;
    }
#if defined(_WIN32)
    {
        int rc;

        (void)fd_in;
        (void)fd_out;
        rc = system(resolved_script);
        free(resolved_script);
        if (rc != 0) {
            snprintf(error, error_size, "Contract exited with non-zero status.");
            return -1;
        }
        return 0;
    }
#else
    {
        pid_t pid;
        int status;

        pid = fork();
        if (pid < 0) {
            free(resolved_script);
            snprintf(error, error_size, "Unable to fork runtime process.");
            return -1;
        }
        if (pid == 0) {
            int input_fd;
            int output_fd;

            if (chdir(workdir) != 0) {
                _exit(127);
            }
            input_fd = fd_in >= 0 ? fd_in : kc_flow_open_null_fd(0);
            output_fd = fd_out >= 0 ? fd_out : kc_flow_open_null_fd(1);
            if (input_fd < 0 || output_fd < 0) {
                _exit(127);
            }
            if (dup2(input_fd, 0) < 0 || dup2(output_fd, 1) < 0) {
                _exit(127);
            }
            if (kc_flow_export_env(overrides, fd_in >= 0 ? 0 : -1, fd_out >= 0 ? 1 : -1) != 0) {
                _exit(127);
            }
            execl("/bin/sh", "sh", "-lc", resolved_script, (char *)NULL);
            _exit(127);
        }
        waitpid(pid, &status, 0);
        free(resolved_script);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            snprintf(error, error_size, "Contract exited with non-zero status.");
            return -1;
        }
    }
#endif
    return 0;
}
