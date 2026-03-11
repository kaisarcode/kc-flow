/**
 * process.c
 * Summary: Atomic contract execution runtime.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if !defined(_WIN32)

/**
 * Duplicates one string.
 * @param text Source text.
 * @return char* Heap copy or NULL.
 */
static char *kc_flow_strdup(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }
    len = strlen(text);
    copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}
#endif

#if defined(_WIN32)

/**
 * Reads one file as text.
 * @param path Source path.
 * @return char* Text buffer or NULL.
 */
static char *kc_flow_read_file_text(const char *path) {
    FILE *fp;
    long size;
    char *buffer;
    size_t read_size;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }
    read_size = fread(buffer, 1, (size_t)size, fp);
    buffer[read_size] = '\0';
    fclose(fp);
    return buffer;
}
#endif

#if !defined(_WIN32)

/**
 * Reads one descriptor as text.
 * @param fd Source descriptor.
 * @return char* Text buffer or NULL.
 */
static char *kc_flow_read_fd_text(int fd) {
    char chunk[512];
    char *buffer;
    size_t size;
    size_t capacity;

    capacity = sizeof(chunk);
    buffer = malloc(capacity + 1);
    if (buffer == NULL) {
        return NULL;
    }
    size = 0;
    for (;;) {
        ssize_t read_size;

        read_size = read(fd, chunk, sizeof(chunk));
        if (read_size < 0) {
            if (errno == EINTR) {
                continue;
            }
            free(buffer);
            return NULL;
        }
        if (read_size == 0) {
            break;
        }
        while (size + (size_t)read_size > capacity) {
            char *grown;

            capacity *= 2;
            grown = realloc(buffer, capacity + 1);
            if (grown == NULL) {
                free(buffer);
                return NULL;
            }
            buffer = grown;
        }
        memcpy(buffer + size, chunk, (size_t)read_size);
        size += (size_t)read_size;
    }
    buffer[size] = '\0';
    return buffer;
}
#endif

/**
 * Executes one contract.
 * @param model Parsed model.
 * @param overrides Runtime overrides.
 * @param cfg_path Source path.
 * @param output Captured output.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_contract(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *cfg_path,
    kc_flow_run_output *output,
    char *error,
    size_t error_size
) {
    char cfg_dir[KC_FLOW_MAX_PATH];
    char workdir[KC_FLOW_MAX_PATH];
    char *resolved_script;

    resolved_script = NULL;
    memset(output, 0, sizeof(*output));
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
        char command[KC_FLOW_MAX_PATH * 2];
        int rc;

        snprintf(
            command,
            sizeof(command),
            "cd /d \"%s\" && %s > kc-flow.out 2> kc-flow.err",
            workdir,
            resolved_script
        );
        rc = system(command);
        output->exit_code = rc;
        output->stdout_text = kc_flow_read_file_text("kc-flow.out");
        output->stderr_text = kc_flow_read_file_text("kc-flow.err");
        remove("kc-flow.out");
        remove("kc-flow.err");
    }
#else
    {
        int out_pipe[2];
        int err_pipe[2];
        pid_t pid;
        int status;

        if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
            free(resolved_script);
            snprintf(error, error_size, "Unable to create runtime pipes.");
            return -1;
        }
        pid = fork();
        if (pid < 0) {
            free(resolved_script);
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(err_pipe[0]);
            close(err_pipe[1]);
            snprintf(error, error_size, "Unable to fork runtime process.");
            return -1;
        }
        if (pid == 0) {
            if (chdir(workdir) != 0) {
                _exit(127);
            }
            dup2(out_pipe[1], 1);
            dup2(err_pipe[1], 2);
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(err_pipe[0]);
            close(err_pipe[1]);
            execl("/bin/sh", "sh", "-lc", resolved_script, (char *)NULL);
            _exit(127);
        }
        close(out_pipe[1]);
        close(err_pipe[1]);
        waitpid(pid, &status, 0);
        output->stdout_text = kc_flow_read_fd_text(out_pipe[0]);
        output->stderr_text = kc_flow_read_fd_text(err_pipe[0]);
        close(out_pipe[0]);
        close(err_pipe[0]);
        if (output->stdout_text == NULL) {
            output->stdout_text = kc_flow_strdup("");
        }
        if (output->stderr_text == NULL) {
            output->stderr_text = kc_flow_strdup("");
        }
        output->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
#endif
    free(resolved_script);
    return 0;
}
