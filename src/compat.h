/**
 * compat.h
 * Summary: Minimal platform primitives for kc-flow runtime portability.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_COMPAT_H
#define KC_FLOW_COMPAT_H

#include <stddef.h>

struct kc_flow_overrides;

long kc_flow_platform_pid(void);
int kc_flow_platform_write_all(int fd, const void *buffer, size_t size);
int kc_flow_platform_rewind_fd(int fd);
int kc_flow_platform_dup_fd(int fd);
void kc_flow_platform_close_fd(int fd);
int kc_flow_platform_open_null_fd(int write_mode);
int kc_flow_platform_create_artifact_fd(char *error, size_t error_size);
int kc_flow_platform_run_command(
    const char *flow_path,
    const char *command,
    const struct kc_flow_overrides *flow_params,
    const struct kc_flow_overrides *node_params,
    int fd_in,
    int fd_out,
    char *error,
    size_t error_size
);

#endif
