/**
 * cycle.c
 * Summary: Flow graph endpoint and cycle validation.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <string.h>

/**
 * Checks whether one node id exists in the current model.
 * @param model Parsed model.
 * @param node_id Logical node id.
 * @return int 1 when present; otherwise 0.
 */
static int kc_flow_has_node_id(const kc_flow_model *model, const char *node_id) {
    size_t i;

    for (i = 0; i < model->nodes.count; ++i) {
        const char *current_id;

        current_id = kc_flow_lookup_indexed_value(
            model,
            &model->nodes,
            "node.",
            model->nodes.values[i],
            "id"
        );
        if (current_id != NULL && strcmp(current_id, node_id) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * Extracts one node id from one endpoint path.
 * @param endpoint Endpoint text.
 * @param kind Expected endpoint marker.
 * @param node_id Output node id.
 * @param size Output buffer size.
 * @return int 1 on success; otherwise 0.
 */
static int kc_flow_extract_node_endpoint(
    const char *endpoint,
    const char *kind,
    char *node_id,
    size_t size
) {
    const char *cursor;
    const char *suffix;
    size_t len;

    if (strncmp(endpoint, "node.", 5) != 0) {
        return 0;
    }
    cursor = endpoint + 5;
    suffix = strstr(cursor, kind);
    if (suffix == NULL || suffix == cursor) {
        return 0;
    }
    len = (size_t)(suffix - cursor);
    if (len >= size) {
        return 0;
    }
    memcpy(node_id, cursor, len);
    node_id[len] = '\0';
    return 1;
}

/**
 * Validates one source or destination endpoint.
 * @param model Parsed model.
 * @param endpoint Endpoint text.
 * @param source Non-zero for source validation.
 * @return int 1 when valid; otherwise 0.
 */
static int kc_flow_validate_endpoint(
    const kc_flow_model *model,
    const char *endpoint,
    int source
) {
    char node_id[128];

    if (endpoint == NULL || endpoint[0] == '\0') {
        return 0;
    }
    if (source) {
        if (strncmp(endpoint, "input.", 6) == 0) {
            return endpoint[6] != '\0';
        }
        if (kc_flow_extract_node_endpoint(endpoint, ".out.", node_id, sizeof(node_id))) {
            return kc_flow_has_node_id(model, node_id);
        }
        return 0;
    }
    if (strncmp(endpoint, "output.", 7) == 0) {
        return endpoint[7] != '\0';
    }
    if (kc_flow_extract_node_endpoint(endpoint, ".in.", node_id, sizeof(node_id))) {
        return kc_flow_has_node_id(model, node_id);
    }
    return 0;
}

/**
 * Validates the current flow graph for accidental cycles.
 * @param model Parsed model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_validate_cycles(
    const kc_flow_model *model,
    char *error,
    size_t error_size
) {
    size_t indegree[KC_FLOW_MAX_INDEXES];
    size_t queue[KC_FLOW_MAX_INDEXES];
    char node_ids[KC_FLOW_MAX_INDEXES][128];
    size_t node_count;
    size_t i;
    size_t head;
    size_t tail;

    if (kc_flow_collect_node_ids(model, node_ids, &node_count, error, error_size) != 0) {
        return -1;
    }
    memset(indegree, 0, sizeof(indegree));
    for (i = 0; i < model->links.count; ++i) {
        const char *from;
        const char *to;
        char from_id[128];
        char to_id[128];
        size_t dst;

        from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "from");
        to = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "to");
        if (!kc_flow_validate_endpoint(model, from, 1) ||
                !kc_flow_validate_endpoint(model, to, 0)) {
            snprintf(error, error_size, "Invalid link endpoint.");
            return -1;
        }
        if (!kc_flow_extract_node_endpoint(from, ".out.", from_id, sizeof(from_id)) ||
                !kc_flow_extract_node_endpoint(to, ".in.", to_id, sizeof(to_id))) {
            continue;
        }
        for (dst = 0; dst < node_count; ++dst) {
            if (strcmp(node_ids[dst], to_id) == 0) {
                indegree[dst]++;
                break;
            }
        }
    }
    head = 0;
    tail = 0;
    for (i = 0; i < node_count; ++i) {
        if (indegree[i] == 0) {
            queue[tail++] = i;
        }
    }
    while (head < tail) {
        size_t src_index;

        src_index = queue[head++];
        for (i = 0; i < model->links.count; ++i) {
            const char *from;
            const char *to;
            char from_id[128];
            char to_id[128];
            size_t dst;

            from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "from");
            to = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "to");
            if (!kc_flow_extract_node_endpoint(from, ".out.", from_id, sizeof(from_id)) ||
                    !kc_flow_extract_node_endpoint(to, ".in.", to_id, sizeof(to_id)) ||
                    strcmp(node_ids[src_index], from_id) != 0) {
                continue;
            }
            for (dst = 0; dst < node_count; ++dst) {
                if (strcmp(node_ids[dst], to_id) == 0) {
                    if (--indegree[dst] == 0) {
                        queue[tail++] = dst;
                    }
                    break;
                }
            }
        }
    }
    if (tail != node_count) {
        snprintf(error, error_size, "Flow contains one cycle.");
        return -1;
    }
    return 0;
}
