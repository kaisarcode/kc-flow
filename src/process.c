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
    (void)fd_status;
    {
        int rc;

        rc = kc_flow_platform_run_contract(
            workdir,
            resolved_script,
            overrides,
            fd_in,
            fd_out,
            error,
            error_size
        );
        free(resolved_script);
        return rc;
    }
}
