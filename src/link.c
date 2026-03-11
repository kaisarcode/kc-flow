/**
 * link.c
 * Summary: Endpoint parsing, link validation, and cycle detection.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "link.h"

#include <stdio.h>
#include <string.h>

/**
 * Collects dense node ids from one flow model.
 * @param model Source model.
 * @param node_ids Output dense node id table.
 * @param count Output node count.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_collect_node_ids(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t *count,
    char *error,
    size_t error_size
) {
    size_t i;
    char key[128];

    *count = model->nodes.count;
    if (*count > KC_STDIO_MAX_INDEXES) {
        snprintf(error, error_size, "Too many nodes in flow.");
        return -1;
    }

    for (i = 0; i < model->nodes.count; ++i) {
        const char *value;
        snprintf(key, sizeof(key), "node.%d.id", model->nodes.values[i]);
        value = kc_flow_model_get(model, key);
        if (value == NULL || value[0] == 0) {
            snprintf(error, error_size, "Missing required key: %s", key);
            return -1;
        }
        if (strlen(value) >= sizeof(node_ids[0])) {
            snprintf(error, error_size, "node id too long: %s", value);
            return -1;
        }
        snprintf(node_ids[i], sizeof(node_ids[0]), "%s", value);
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
/**
 * Collects and validates flow links.
 * @param model Source model.
 * @param node_ids Dense node id table.
 * @param node_count Number of nodes.
 * @param links Output link table.
 * @param link_count Output link count.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_collect_links(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t node_count,
    kc_flow_link_entry *links,
    size_t *link_count,
    char *error,
    size_t error_size
) {
    size_t i;

    if (model->links.count > KC_STDIO_MAX_INDEXES) {
        snprintf(error, error_size, "Too many links in flow.");
        return -1;
    }

    *link_count = model->links.count;
    for (i = 0; i < model->links.count; ++i) {
        char from_key[128];
        char to_key[128];
        const char *from_value;
        const char *to_value;

        snprintf(from_key, sizeof(from_key), "link.%d.from", model->links.values[i]);
        snprintf(to_key, sizeof(to_key), "link.%d.to", model->links.values[i]);
        from_value = kc_flow_model_get(model, from_key);
        to_value = kc_flow_model_get(model, to_key);

        if (kc_flow_parse_link_endpoint(from_value, &links[i].from) != 0) {
            snprintf(
                error,
                error_size,
                "Invalid link source endpoint: %s",
                from_value != NULL ? from_value : "(null)"
            );
            return -1;
        }
        if (kc_flow_parse_link_endpoint(to_value, &links[i].to) != 0) {
            snprintf(
                error,
                error_size,
                "Invalid link destination endpoint: %s",
                to_value != NULL ? to_value : "(null)"
            );
            return -1;
        }

        if (kc_flow_validate_link_endpoint_pair(
                model,
                node_ids,
                node_count,
                &links[i].from,
                &links[i].to,
                error,
                error_size
            ) != 0) {
            return -1;
        }
    }

    return 0;
}
