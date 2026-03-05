/**
 * cycle.c
 * Summary: Flow cycle detection.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "link.h"

#include <stdio.h>
#include <string.h>

static int kc_flow_has_parent_field_id(const kc_flow_model *model,
                                       const kc_flow_index_set *set,
                                       const char *prefix,
                                       const char *field_id) {
    size_t i;
    char key[128];

    for (i = 0; i < set->count; ++i) {
        const char *value;
        snprintf(key, sizeof(key), "%s%d.id", prefix, set->values[i]);
        value = kc_flow_model_get(model, key);
        if (value != NULL && strcmp(value, field_id) == 0) {
            return 1;
        }
    }

    return 0;
}

static int kc_flow_find_node_index(char node_ids[][128],
                                   size_t count,
                                   const char *node_id) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(node_ids[i], node_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int kc_flow_parse_endpoint(const char *text, kc_flow_endpoint *endpoint) {
    memset(endpoint, 0, sizeof(*endpoint));

    if (text == NULL || text[0] == 0) {
        return -1;
    }

    if (strncmp(text, "input.", 6) == 0) {
        const char *field = text + 6;
        if (field[0] == 0 || strlen(field) >= sizeof(endpoint->field_id)) {
            return -1;
        }
        endpoint->kind = KC_FLOW_ENDPOINT_INPUT;
        snprintf(endpoint->field_id, sizeof(endpoint->field_id), "%s", field);
        return 0;
    }

    if (strncmp(text, "output.", 7) == 0) {
        const char *field = text + 7;
        if (field[0] == 0 || strlen(field) >= sizeof(endpoint->field_id)) {
            return -1;
        }
        endpoint->kind = KC_FLOW_ENDPOINT_OUTPUT;
        snprintf(endpoint->field_id, sizeof(endpoint->field_id), "%s", field);
        return 0;
    }

    if (strncmp(text, "node.", 5) == 0) {
        const char *cursor = text + 5;
        const char *dot = strchr(cursor, '.');
        const char *rest;

        if (dot == NULL || dot == cursor) {
            return -1;
        }

        if ((size_t)(dot - cursor) >= sizeof(endpoint->node_id)) {
            return -1;
        }

        memcpy(endpoint->node_id, cursor, (size_t)(dot - cursor));
        endpoint->node_id[dot - cursor] = 0;

        rest = dot + 1;
        if (strncmp(rest, "in.", 3) == 0) {
            const char *field = rest + 3;
            if (field[0] == 0 || strlen(field) >= sizeof(endpoint->field_id)) {
                return -1;
            }
            endpoint->kind = KC_FLOW_ENDPOINT_NODE_IN;
            snprintf(endpoint->field_id, sizeof(endpoint->field_id), "%s", field);
            return 0;
        }

        if (strncmp(rest, "out.", 4) == 0) {
            const char *field = rest + 4;
            if (field[0] == 0 || strlen(field) >= sizeof(endpoint->field_id)) {
                return -1;
            }
            endpoint->kind = KC_FLOW_ENDPOINT_NODE_OUT;
            snprintf(endpoint->field_id, sizeof(endpoint->field_id), "%s", field);
            return 0;
        }
    }

    return -1;
}

static int kc_flow_validate_link_semantics(const kc_flow_model *model,
                                           char node_ids[][128],
                                           size_t node_count,
                                           const kc_flow_endpoint *from,
                                           const kc_flow_endpoint *to,
                                           char *error,
                                           size_t error_size) {
    if (!(from->kind == KC_FLOW_ENDPOINT_INPUT ||
          from->kind == KC_FLOW_ENDPOINT_NODE_OUT)) {
        snprintf(error, error_size, "Invalid link source endpoint.");
        return -1;
    }

    if (!(to->kind == KC_FLOW_ENDPOINT_NODE_IN ||
          to->kind == KC_FLOW_ENDPOINT_OUTPUT)) {
        snprintf(error, error_size, "Invalid link destination endpoint.");
        return -1;
    }

    if (from->kind == KC_FLOW_ENDPOINT_INPUT &&
        !kc_flow_has_parent_field_id(model,
                                     &model->inputs,
                                     "input.",
                                     from->field_id)) {
        snprintf(error,
                 error_size,
                 "Unknown parent input id in link: %s",
                 from->field_id);
        return -1;
    }

    if ((from->kind == KC_FLOW_ENDPOINT_NODE_IN ||
         from->kind == KC_FLOW_ENDPOINT_NODE_OUT) &&
        kc_flow_find_node_index(node_ids, node_count, from->node_id) < 0) {
        snprintf(error,
                 error_size,
                 "Unknown node id in link source: %s",
                 from->node_id);
        return -1;
    }

    if ((to->kind == KC_FLOW_ENDPOINT_NODE_IN ||
         to->kind == KC_FLOW_ENDPOINT_NODE_OUT) &&
        kc_flow_find_node_index(node_ids, node_count, to->node_id) < 0) {
        snprintf(error,
                 error_size,
                 "Unknown node id in link destination: %s",
                 to->node_id);
        return -1;
    }

    return 0;
}

int kc_flow_detect_cycle(const kc_flow_model *model,
                         char node_ids[][128],
                         size_t node_count,
                         char *error,
                         size_t error_size) {
    unsigned char edges[KC_STDIO_MAX_INDEXES][KC_STDIO_MAX_INDEXES];
    size_t indegree[KC_STDIO_MAX_INDEXES];
    size_t queue[KC_STDIO_MAX_INDEXES];
    size_t head = 0;
    size_t tail = 0;
    size_t visited = 0;
    size_t i;

    memset(edges, 0, sizeof(edges));
    memset(indegree, 0, sizeof(indegree));

    for (i = 0; i < model->links.count; ++i) {
        char from_key[128];
        char to_key[128];
        const char *from_value;
        const char *to_value;
        kc_flow_endpoint from_ep;
        kc_flow_endpoint to_ep;

        snprintf(from_key, sizeof(from_key), "link.%d.from", model->links.values[i]);
        snprintf(to_key, sizeof(to_key), "link.%d.to", model->links.values[i]);
        from_value = kc_flow_model_get(model, from_key);
        to_value = kc_flow_model_get(model, to_key);

        if (kc_flow_parse_endpoint(from_value, &from_ep) != 0) {
            snprintf(error, error_size, "Invalid link source endpoint.");
            return -1;
        }
        if (kc_flow_parse_endpoint(to_value, &to_ep) != 0) {
            snprintf(error, error_size, "Invalid link destination endpoint.");
            return -1;
        }

        if (kc_flow_validate_link_semantics(model,
                                            node_ids,
                                            node_count,
                                            &from_ep,
                                            &to_ep,
                                            error,
                                            error_size) != 0) {
            return -1;
        }

        if (from_ep.kind == KC_FLOW_ENDPOINT_NODE_OUT &&
            to_ep.kind == KC_FLOW_ENDPOINT_NODE_IN) {
            int from_idx = kc_flow_find_node_index(
                node_ids, node_count, from_ep.node_id
            );
            int to_idx = kc_flow_find_node_index(
                node_ids, node_count, to_ep.node_id
            );
            if (from_idx < 0 || to_idx < 0) {
                snprintf(error, error_size, "Unable to resolve node edge indexes.");
                return -1;
            }
            if (!edges[from_idx][to_idx]) {
                edges[from_idx][to_idx] = 1;
                indegree[to_idx]++;
            }
        }
    }

    for (i = 0; i < node_count; ++i) {
        if (indegree[i] == 0) {
            queue[tail++] = i;
        }
    }

    while (head < tail) {
        size_t u = queue[head++];
        size_t v;
        visited++;

        for (v = 0; v < node_count; ++v) {
            if (edges[u][v]) {
                if (indegree[v] > 0) {
                    indegree[v]--;
                }
                if (indegree[v] == 0) {
                    queue[tail++] = v;
                }
            }
        }
    }

    if (visited != node_count) {
        snprintf(error, error_size, "Cycle detected in flow links.");
        return -1;
    }

    return 0;
}
