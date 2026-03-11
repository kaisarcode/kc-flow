/**
 * worker.c
 * Summary: Runtime worker dispatch helpers for parallel node execution.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#if !defined(_WIN32)

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Starts one ready node in one worker process.
 * @param model Parsed model.
 * @param cfg Runtime configuration.
 * @param flow_dir Parent directory.
 * @param node_id Logical node id.
 * @param runtime_params Runtime parameters.
 * @param fd_in Runtime input descriptor.
 * @param handle Output worker handle.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_start_flow_node(
    const kc_flow_model *model,
    const kc_flow_runtime_cfg *cfg,
    const char *flow_dir,
    const char *node_id,
    const kc_flow_overrides *runtime_params,
    int fd_in,
    kc_flow_worker_handle *handle,
    char *error,
    size_t error_size
) {
    pid_t pid;
    int output_fd;

    output_fd = kc_flow_create_artifact_fd(error, error_size);
    if (output_fd < 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        kc_flow_release_fd(output_fd);
        snprintf(error, error_size, "Unable to fork worker.");
        return -1;
    }
    if (pid == 0) {
        int child_fd_out;

        child_fd_out = output_fd;
        if (kc_flow_run_node(
                model,
                cfg,
                flow_dir,
                node_id,
                runtime_params,
                fd_in,
                &child_fd_out,
                error,
                error_size
            ) != 0) {
            _exit(1);
        }
        _exit(0);
    }
    handle->pid = (long)pid;
    handle->output_fd = output_fd;
    return 0;
}

/**
 * Waits for one worker process and collects its output descriptor.
 * @param handle Worker handle.
 * @param fd_out Output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_finish_flow_node(
    kc_flow_worker_handle *handle,
    int *fd_out,
    char *error,
    size_t error_size
) {
    int status;

    *fd_out = handle->output_fd;
    if (waitpid((pid_t)handle->pid, &status, 0) < 0) {
        kc_flow_release_fd(handle->output_fd);
        handle->output_fd = -1;
        snprintf(error, error_size, "Unable to wait for worker.");
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        kc_flow_release_fd(handle->output_fd);
        handle->output_fd = -1;
        snprintf(error, error_size, "Worker exited with non-zero status.");
        return -1;
    }
    handle->output_fd = -1;
    return 0;
}

#else

#include <stdio.h>

/**
 * Rejects worker start on unsupported runtime targets.
 * @param model Parsed model.
 * @param cfg Runtime configuration.
 * @param flow_dir Parent directory.
 * @param node_id Logical node id.
 * @param runtime_params Runtime parameters.
 * @param fd_in Runtime input descriptor.
 * @param handle Output worker handle.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int Always non-zero.
 */
int kc_flow_start_flow_node(
    const kc_flow_model *model,
    const kc_flow_runtime_cfg *cfg,
    const char *flow_dir,
    const char *node_id,
    const kc_flow_overrides *runtime_params,
    int fd_in,
    kc_flow_worker_handle *handle,
    char *error,
    size_t error_size
) {
    (void)model;
    (void)cfg;
    (void)flow_dir;
    (void)node_id;
    (void)runtime_params;
    (void)fd_in;
    (void)handle;
    snprintf(error, error_size, "Worker runtime start is not available on this platform.");
    return -1;
}

/**
 * Rejects worker completion on unsupported runtime targets.
 * @param handle Worker handle.
 * @param fd_out Output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int Always non-zero.
 */
int kc_flow_finish_flow_node(
    kc_flow_worker_handle *handle,
    int *fd_out,
    char *error,
    size_t error_size
) {
    (void)handle;
    (void)fd_out;
    snprintf(error, error_size, "Worker runtime finish is not available on this platform.");
    return -1;
}

#endif
