/**
 * contract.c
 * Summary: Atomic contract process runner.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "priv.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Executes one atomic contract runtime script.
 * @param model Parsed contract model.
 * @param overrides Runtime overrides.
 * @param cfg_path Source contract path.
 * @param output Captured stdout/stderr/exit code.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on runtime failure.
 */
int kc_flow_run_contract(const kc_flow_model *model,
                         const kc_flow_overrides *overrides,
                         const char *cfg_path,
                         kc_flow_run_output *output,
                         char *error,
                         size_t error_size) {
    char cfg_dir[PATH_MAX];
    char workdir[PATH_MAX];
    char stdout_path[] = "/tmp/kc-flow-stdout-XXXXXX";
    char stderr_path[] = "/tmp/kc-flow-stderr-XXXXXX";
    char stdin_path[PATH_MAX];
    int stdout_fd;
    int stderr_fd;
    char *resolved_script;
    char *resolved_exec;
    char *resolved_stdin;
    char *quoted_workdir;
    char *quoted_stdout;
    char *quoted_stderr;
    char *quoted_stdin;
    char *quoted_exec;
    char *command;
    char *env_buffer;
    size_t command_size;
    int status;

    kc_flow_dirname(cfg_path, cfg_dir, sizeof(cfg_dir));
    if (kc_flow_build_path(workdir,
                           sizeof(workdir),
                           cfg_dir,
                           model->runtime_workdir != NULL ?
                           model->runtime_workdir : ".") != 0) {
        snprintf(error, error_size, "Unable to resolve workdir.");
        return -1;
    }

    resolved_script = kc_flow_resolve_template(
        model, overrides, model->runtime_script, error, error_size
    );
    if (resolved_script == NULL) {
        return -1;
    }

    resolved_exec = kc_flow_resolve_template(
        model, overrides, model->runtime_exec, error, error_size
    );
    if (model->runtime_exec != NULL && resolved_exec == NULL) {
        free(resolved_script);
        return -1;
    }

    resolved_stdin = kc_flow_resolve_template(
        model, overrides, model->runtime_stdin, error, error_size
    );
    if (model->runtime_stdin != NULL && resolved_stdin == NULL) {
        free(resolved_script);
        free(resolved_exec);
        return -1;
    }

    stdout_fd = mkstemp(stdout_path);
    stderr_fd = mkstemp(stderr_path);
    if (stdout_fd < 0 || stderr_fd < 0) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        if (stdout_fd >= 0) {
            close(stdout_fd);
            unlink(stdout_path);
        }
        if (stderr_fd >= 0) {
            close(stderr_fd);
            unlink(stderr_path);
        }
        snprintf(error, error_size, "Unable to create temp output files.");
        return -1;
    }
    close(stdout_fd);
    close(stderr_fd);

    stdin_path[0] = '\0';
    if (resolved_stdin != NULL) {
        if (kc_flow_write_temp_file(resolved_stdin,
                                    stdin_path,
                                    sizeof(stdin_path)) != 0) {
            free(resolved_script);
            free(resolved_exec);
            free(resolved_stdin);
            unlink(stdout_path);
            unlink(stderr_path);
            snprintf(error, error_size, "Unable to create stdin file.");
            return -1;
        }
    }

    quoted_workdir = kc_flow_shell_quote(workdir);
    quoted_stdout = kc_flow_shell_quote(stdout_path);
    quoted_stderr = kc_flow_shell_quote(stderr_path);
    quoted_stdin = resolved_stdin != NULL ? kc_flow_shell_quote(stdin_path) : NULL;
    quoted_exec = resolved_exec != NULL ? kc_flow_shell_quote(resolved_exec) : NULL;
    env_buffer = NULL;

    if (quoted_workdir == NULL || quoted_stdout == NULL || quoted_stderr == NULL ||
        (resolved_stdin != NULL && quoted_stdin == NULL) ||
        (resolved_exec != NULL && quoted_exec == NULL)) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        snprintf(error, error_size, "Out of memory while preparing exec.");
        return -1;
    }

    if (kc_flow_build_env_prefix(model, overrides, &env_buffer, error, error_size) != 0) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        unlink(stdout_path);
        unlink(stderr_path);
        return -1;
    }

    command_size = strlen("cd ") + strlen(quoted_workdir) + strlen(" && ") +
                   strlen(env_buffer) + strlen(resolved_script) +
                   strlen(" > ") + strlen(quoted_stdout) +
                   strlen(" 2> ") + strlen(quoted_stderr) + 32;

    if (quoted_exec != NULL) {
        command_size += strlen(quoted_exec) + 1;
    }

    if (quoted_stdin != NULL) {
        command_size += strlen(" < ") + strlen(quoted_stdin);
    }

    command = malloc(command_size);
    if (command == NULL) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        snprintf(error, error_size, "Out of memory while building command.");
        return -1;
    }

    snprintf(command,
             command_size,
             "cd %s && %s%s%s%s%s > %s 2> %s",
             quoted_workdir,
             env_buffer,
             quoted_exec != NULL ? quoted_exec : "",
             quoted_exec != NULL ? " " : "",
             resolved_script,
             quoted_stdin != NULL ? "" : "",
             quoted_stdout,
             quoted_stderr);

    if (quoted_stdin != NULL) {
        size_t current_len = strlen(command);
        snprintf(command + current_len,
                 command_size - current_len,
                 " < %s",
                 quoted_stdin);
    }

    status = system(command);
    if (status == -1) {
        snprintf(error, error_size, "Unable to launch process.");
        free(command);
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        return -1;
    }

    output->stdout_text = kc_flow_read_file_text(stdout_path);
    output->stderr_text = kc_flow_read_file_text(stderr_path);
    output->exit_code = kc_flow_system_exit_code(status);

    unlink(stdout_path);
    unlink(stderr_path);
    if (stdin_path[0] != '\0') {
        unlink(stdin_path);
    }

    free(command);
    free(resolved_script);
    free(resolved_exec);
    free(resolved_stdin);
    free(quoted_workdir);
    free(quoted_stdout);
    free(quoted_stderr);
    free(quoted_stdin);
    free(quoted_exec);
    free(env_buffer);

    if (output->stdout_text == NULL || output->stderr_text == NULL) {
        snprintf(error, error_size, "Unable to collect process output.");
        return -1;
    }

    return 0;
}
