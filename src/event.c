/**
 * event.c
 * Summary: Structured runtime status emission for external supervisors.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

/**
 * Emits one run lifecycle event.
 * @param fd Destination descriptor.
 * @param event Event name.
 * @param kind Runtime kind.
 * @param id Runtime identifier.
 * @param path Runtime path.
 * @param status Result status.
 * @param message Error or detail message.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_status_write_run_event(
    int fd,
    const char *event,
    const char *kind,
    const char *id,
    const char *path,
    const char *status,
    const char *message
) {
    return kc_flow_status_emit(
        fd,
        event,
        kind,
        id,
        path,
        NULL,
        NULL,
        NULL,
        status,
        message
    );
}

/**
 * Emits one node lifecycle event.
 * @param fd Destination descriptor.
 * @param event Event name.
 * @param node Node identifier.
 * @param target_kind Node target kind.
 * @param target_path Node target path.
 * @param status Result status.
 * @param message Error or detail message.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_status_write_node_event(
    int fd,
    const char *event,
    const char *node,
    const char *target_kind,
    const char *target_path,
    const char *status,
    const char *message
) {
    return kc_flow_status_emit(
        fd,
        event,
        "node",
        NULL,
        NULL,
        node,
        target_kind,
        target_path,
        status,
        message
    );
}
