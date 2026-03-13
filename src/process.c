/**
 * process.c
 * Summary: Atomic node exec resolution and process launch handoff.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdlib.h>

/**
 * Runs one node exec in the current scopes.
 * @param flow_path Current flow path.
 * @param command Source command template.
 * @param flow_params Current flow params.
 * @param node_params Current node params.
 * @param fd_in Runtime input descriptor.
 * @param fd_out Runtime output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_exec(
    const char *flow_path,
    const char *command,
    const kc_flow_overrides *flow_params,
    const kc_flow_overrides *node_params,
    int fd_in,
    int fd_out,
    char *error,
    size_t error_size
) {
    char *resolved;
    int rc;

    resolved = kc_flow_resolve_template(command, flow_params, node_params, error, error_size);
    if (resolved == NULL) {
        return -1;
    }
    rc = kc_flow_platform_run_command(
        flow_path,
        resolved,
        flow_params,
        node_params,
        fd_in,
        fd_out,
        error,
        error_size
    );
    free(resolved);
    return rc;
}
