/**
 * cycle.c
 * Summary: Flow cycle detection over validated link entries.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "link.h"

#include <stdio.h>
#include <string.h>

/**
 * Detects graph cycles across one flow node set.
 * @param model Source model.
 * @param node_ids Dense node id table.
 * @param node_count Number of nodes.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 if acyclic; non-zero if invalid or cyclic.
 */
int kc_flow_detect_cycle(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t node_count,
    char *error,
    size_t error_size
) {
    kc_flow_link_entry links[KC_STDIO_MAX_INDEXES];
    unsigned char edges[KC_STDIO_MAX_INDEXES][KC_STDIO_MAX_INDEXES];
    size_t indegree[KC_STDIO_MAX_INDEXES];
    size_t queue[KC_STDIO_MAX_INDEXES];
    size_t link_count;
    size_t head;
    size_t tail;
    size_t visited;
    size_t i;

    memset(edges, 0, sizeof(edges));
    memset(indegree, 0, sizeof(indegree));

    if (kc_flow_collect_links(
            model,
            node_ids,
            node_count,
            links,
            &link_count,
            error,
            error_size
        ) != 0) {
        return -1;
    }

    for (i = 0; i < link_count; ++i) {
        if (links[i].from.kind == KC_FLOW_ENDPOINT_NODE_OUT &&
                links[i].to.kind == KC_FLOW_ENDPOINT_NODE_IN) {
            size_t from_index;
            size_t to_index;
            int from_idx;
            int to_idx;

            from_idx = -1;
            to_idx = -1;
            for (from_index = 0; from_index < node_count; ++from_index) {
                if (strcmp(node_ids[from_index], links[i].from.node_id) == 0) {
                    from_idx = (int)from_index;
                    break;
                }
            }
            for (to_index = 0; to_index < node_count; ++to_index) {
                if (strcmp(node_ids[to_index], links[i].to.node_id) == 0) {
                    to_idx = (int)to_index;
                    break;
                }
            }

            if (from_idx < 0 || to_idx < 0) {
                snprintf(error, error_size, "Unable to resolve node edge indexes.");
                return -1;
            }

            if (edges[from_idx][to_idx] == 0) {
                edges[from_idx][to_idx] = 1;
                indegree[to_idx]++;
            }
        }
    }

    head = 0;
    tail = 0;
    visited = 0;
    for (i = 0; i < node_count; ++i) {
        if (indegree[i] == 0) {
            queue[tail++] = i;
        }
    }

    while (head < tail) {
        size_t u;
        size_t v;

        u = queue[head++];
        visited++;
        for (v = 0; v < node_count; ++v) {
            if (edges[u][v] != 0) {
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
