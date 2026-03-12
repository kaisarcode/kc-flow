/**
 * event.c
 * Summary: Structured runtime status emission and line encoding for external supervisors.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"
#include "compat.h"

#include <stdio.h>
#include <string.h>

/**
 * Appends one escaped field value.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @param length Current output length.
 * @param text Source text.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_status_append_text(
    char *buffer,
    size_t size,
    size_t *length,
    const char *text
) {
    size_t i;

    for (i = 0; text != NULL && text[i] != '\0'; ++i) {
        const char *escape;
        size_t escape_len;

        escape = NULL;
        escape_len = 0;
        if (text[i] == '\\') {
            escape = "\\\\";
            escape_len = 2;
        } else if (text[i] == '\n') {
            escape = "\\n";
            escape_len = 2;
        } else if (text[i] == '\r') {
            escape = "\\r";
            escape_len = 2;
        } else if (text[i] == '\t') {
            escape = "\\t";
            escape_len = 2;
        } else if (text[i] == ' ') {
            escape = "\\s";
            escape_len = 2;
        } else if (text[i] == '=') {
            escape = "\\e";
            escape_len = 2;
        }
        if (escape != NULL) {
            if (*length + escape_len >= size) {
                return -1;
            }
            memcpy(buffer + *length, escape, escape_len);
            *length += escape_len;
            continue;
        }
        if (*length + 1 >= size) {
            return -1;
        }
        buffer[*length] = text[i];
        (*length)++;
    }
    return 0;
}

/**
 * Appends one optional key-value field.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @param length Current output length.
 * @param key Field key.
 * @param value Field value.
 * @param first Non-zero before the first field.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_status_append_field(
    char *buffer,
    size_t size,
    size_t *length,
    const char *key,
    const char *value,
    int *first
) {
    int rc;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    rc = snprintf(
        buffer + *length,
        size - *length,
        "%s%s=",
        *first ? "" : " ",
        key
    );
    if (rc < 0 || (size_t)rc >= size - *length) {
        return -1;
    }
    *length += (size_t)rc;
    if (kc_flow_status_append_text(buffer, size, length, value) != 0) {
        return -1;
    }
    *first = 0;
    return 0;
}

/**
 * Appends the current process id field.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @param length Current output length.
 * @param first Non-zero before the first field.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_status_append_pid(
    char *buffer,
    size_t size,
    size_t *length,
    int *first
) {
    int rc;

    rc = snprintf(
        buffer + *length,
        size - *length,
        "%spid=%ld",
        *first ? "" : " ",
        kc_flow_platform_pid()
    );
    if (rc < 0 || (size_t)rc >= size - *length) {
        return -1;
    }
    *length += (size_t)rc;
    *first = 0;
    return 0;
}

/**
 * Emits one structured status event.
 * @param fd Destination descriptor.
 * @param event Event name.
 * @param kind Runtime kind.
 * @param id Runtime identifier.
 * @param path Runtime path.
 * @param node Node identifier.
 * @param target_kind Node target kind.
 * @param target_path Node target path.
 * @param status Result status.
 * @param message Error or detail message.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_status_emit(
    int fd,
    const char *event,
    const char *kind,
    const char *id,
    const char *path,
    const char *node,
    const char *target_kind,
    const char *target_path,
    const char *status,
    const char *message
) {
    char buffer[4096];
    size_t length;
    int first;

    if (fd < 0) {
        return 0;
    }
    length = 0;
    first = 1;
    if (kc_flow_status_append_field(buffer, sizeof(buffer), &length, "event", event, &first) != 0 ||
            kc_flow_status_append_pid(buffer, sizeof(buffer), &length, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "kind", kind, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "id", id, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "path", path, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "node", node, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "target_kind", target_kind, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "target_path", target_path, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "status", status, &first) != 0 ||
            kc_flow_status_append_field(buffer, sizeof(buffer), &length, "message", message, &first) != 0) {
        return -1;
    }
    if (length + 1 >= sizeof(buffer)) {
        return -1;
    }
    buffer[length++] = '\n';
    return kc_flow_platform_write_all(fd, buffer, length);
}

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
