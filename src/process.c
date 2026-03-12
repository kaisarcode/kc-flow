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
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Executes one contract.
 * @param model Parsed contract.
 * @param overrides Effective parameters.
 * @param cfg_path Source path.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @param fd_status Runtime status descriptor.
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
    int fd_status,
    char *error,
    size_t error_size
) {
    kc_flow_overrides effective_params;
    char *resolved_command;

    (void)cfg_path;
    if (kc_flow_collect_effective_params(
            model,
            overrides,
            &effective_params,
            error,
            error_size
        ) != 0) {
        return -1;
    }
    resolved_command = kc_flow_resolve_template(
        model,
        &effective_params,
        model->runtime_command,
        error,
        error_size
    );
    if (resolved_command == NULL) {
        kc_flow_overrides_free(&effective_params);
        return -1;
    }
    (void)fd_status;
    {
        int rc;

        rc = kc_flow_platform_run_contract(
            cfg_path,
            resolved_command,
            &effective_params,
            fd_in,
            fd_out,
            error,
            error_size
        );
        free(resolved_command);
        kc_flow_overrides_free(&effective_params);
        return rc;
    }
}
