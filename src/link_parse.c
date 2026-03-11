/**
 * link_parse.c
 * Summary: Link endpoint parsing and semantic validation helpers.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "link.h"

#include <stdio.h>
#include <string.h>

/**
 * Checks whether one parent field id exists in one indexed set.
 * @param model Source model.
 * @param set Indexed section set.
 * @param prefix Field key prefix.
 * @param field_id Field identifier to match.
 * @return int 1 if found; otherwise 0.
 */
static int kc_flow_has_parent_field_id(
    const kc_flow_model *model,
    const kc_flow_index_set *set,
    const char *prefix,
    const char *field_id
) {
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

/**
 * Resolves one node id to its dense graph index.
 * @param node_ids Dense node id table.
 * @param count Number of node ids.
 * @param node_id Node id to resolve.
 * @return int Resolved index or -1 if not found.
 */
static int kc_flow_find_node_index(
    char node_ids[][128],
    size_t count,
    const char *node_id
) {
    size_t i;

    for (i = 0; i < count; ++i) {
        if (strcmp(node_ids[i], node_id) == 0) {
            return (int)i;
        }
    }

    return -1;
}

/**
 * Parses one link endpoint expression.
 * @param text Endpoint text.
 * @param endpoint Output parsed endpoint.
 * @return int 0 on success; non-zero on parse failure.
 */
int kc_flow_parse_link_endpoint(
    const char *text,
    kc_flow_endpoint *endpoint
) {
    memset(endpoint, 0, sizeof(*endpoint));

    if (text == NULL || text[0] == 0) {
        return -1;
    }

    if (strncmp(text, "input.", 6) == 0) {
        const char *field;

        field = text + 6;
        if (field[0] == 0 || strlen(field) >= sizeof(endpoint->field_id)) {
            return -1;
        }
        endpoint->kind = KC_FLOW_ENDPOINT_INPUT;
        snprintf(endpoint->field_id, sizeof(endpoint->field_id), "%s", field);
        return 0;
    }

    if (strncmp(text, "output.", 7) == 0) {
        const char *field;

        field = text + 7;
        if (field[0] == 0 || strlen(field) >= sizeof(endpoint->field_id)) {
            return -1;
        }
        endpoint->kind = KC_FLOW_ENDPOINT_OUTPUT;
        snprintf(endpoint->field_id, sizeof(endpoint->field_id), "%s", field);
        return 0;
    }

    if (strncmp(text, "node.", 5) == 0) {
        const char *cursor;
        const char *dot;
        const char *rest;

        cursor = text + 5;
        dot = strchr(cursor, '.');
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
            const char *field;

            field = rest + 3;
            if (field[0] == 0 || strlen(field) >= sizeof(endpoint->field_id)) {
                return -1;
            }
            endpoint->kind = KC_FLOW_ENDPOINT_NODE_IN;
            snprintf(endpoint->field_id, sizeof(endpoint->field_id), "%s", field);
            return 0;
        }

        if (strncmp(rest, "out.", 4) == 0) {
            const char *field;

            field = rest + 4;
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

/**
 * Validates one parsed link against the current model.
 * @param model Source model.
 * @param node_ids Dense node id table.
 * @param node_count Number of nodes.
 * @param from Parsed source endpoint.
 * @param to Parsed destination endpoint.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on validation failure.
 */
int kc_flow_validate_link_endpoint_pair(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t node_count,
    const kc_flow_endpoint *from,
    const kc_flow_endpoint *to,
    char *error,
    size_t error_size
) {
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
            !kc_flow_has_parent_field_id(
                model,
                &model->inputs,
                "input.",
                from->field_id
            )) {
        snprintf(
            error,
            error_size,
            "Unknown parent input id in link: %s",
            from->field_id
        );
        return -1;
    }

    if ((from->kind == KC_FLOW_ENDPOINT_NODE_IN ||
            from->kind == KC_FLOW_ENDPOINT_NODE_OUT) &&
            kc_flow_find_node_index(node_ids, node_count, from->node_id) < 0) {
        snprintf(
            error,
            error_size,
            "Unknown node id in link source: %s",
            from->node_id
        );
        return -1;
    }

    if ((to->kind == KC_FLOW_ENDPOINT_NODE_IN ||
            to->kind == KC_FLOW_ENDPOINT_NODE_OUT) &&
            kc_flow_find_node_index(node_ids, node_count, to->node_id) < 0) {
        snprintf(
            error,
            error_size,
            "Unknown node id in link destination: %s",
            to->node_id
        );
        return -1;
    }

    return 0;
}
