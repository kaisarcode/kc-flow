/**
 * chain.c
 * Summary: Flow-to-bash chain builder.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "chain.h"
#include "link.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int kc_flow_find_node_index(char node_ids[][128], size_t count, const char *node_id) {
    size_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(node_ids[i], node_id) == 0) return (int)i;
    }
    return -1;
}

static int kc_flow_find_node_record(const kc_flow_model *model, const char *node_id) {
    size_t i;
    char key[128];
    for (i = 0; i < model->nodes.count; ++i) {
        const char *id;
        snprintf(key, sizeof(key), "node.%d.id", model->nodes.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL && strcmp(id, node_id) == 0) return model->nodes.values[i];
    }
    return -1;
}

static void kc_flow_sanitize(char *out, size_t out_size, const char *in) {
    size_t i;
    size_t n = 0;
    if (out_size == 0) return;
    for (i = 0; in[i] != '\0' && n + 1 < out_size; ++i) {
        char c = in[i];
        out[n++] = (isalnum((unsigned char)c) || c == '_') ? c : '_';
    }
    if (n == 0) out[n++] = 'x';
    if (isdigit((unsigned char)out[0]) && n + 1 < out_size) {
        memmove(out + 1, out, n);
        out[0] = 'v';
        n++;
    }
    out[n] = '\0';
}

static void kc_flow_print_sq(const char *text) {
    size_t i;
    putchar('\'');
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '\'') fputs("'\\''", stdout);
        else putchar(text[i]);
    }
    putchar('\'');
}

static int kc_flow_toposort(char node_ids[][128],
                            size_t node_count,
                            const kc_flow_link_entry *links,
                            size_t link_count,
                            size_t *order,
                            char *error,
                            size_t error_size) {
    unsigned char edges[KC_STDIO_MAX_INDEXES][KC_STDIO_MAX_INDEXES];
    size_t indegree[KC_STDIO_MAX_INDEXES];
    size_t queue[KC_STDIO_MAX_INDEXES];
    size_t head = 0, tail = 0, out_n = 0, i;

    memset(edges, 0, sizeof(edges));
    memset(indegree, 0, sizeof(indegree));

    for (i = 0; i < link_count; ++i) {
        if (links[i].from.kind == KC_FLOW_ENDPOINT_NODE_OUT &&
            links[i].to.kind == KC_FLOW_ENDPOINT_NODE_IN) {
            int u = kc_flow_find_node_index(node_ids, node_count, links[i].from.node_id);
            int v = kc_flow_find_node_index(node_ids, node_count, links[i].to.node_id);
            if (u < 0 || v < 0) {
                snprintf(error, error_size, "Unable to build topological order.");
                return -1;
            }
            if (!edges[u][v]) {
                edges[u][v] = 1;
                indegree[v]++;
            }
        }
    }

    for (i = 0; i < node_count; ++i) {
        if (indegree[i] == 0) queue[tail++] = i;
    }

    while (head < tail) {
        size_t u = queue[head++];
        size_t v;
        order[out_n++] = u;
        for (v = 0; v < node_count; ++v) {
            if (edges[u][v]) {
                if (indegree[v] > 0) indegree[v]--;
                if (indegree[v] == 0) queue[tail++] = v;
            }
        }
    }

    if (out_n != node_count) {
        snprintf(error, error_size, "Unable to topologically sort flow nodes.");
        return -1;
    }
    return 0;
}
static int kc_flow_compute_depths(char node_ids[][128],
                                  size_t node_count,
                                  const kc_flow_link_entry *links,
                                  size_t link_count,
                                  const size_t *order,
                                  size_t *depth,
                                  size_t *max_depth,
                                  char *error,
                                  size_t error_size) {
    size_t i;
    *max_depth = 0;
    memset(depth, 0, sizeof(size_t) * KC_STDIO_MAX_INDEXES);
    for (i = 0; i < node_count; ++i) {
        size_t u = order[i];
        size_t j;
        for (j = 0; j < link_count; ++j) {
            if (links[j].from.kind == KC_FLOW_ENDPOINT_NODE_OUT &&
                links[j].to.kind == KC_FLOW_ENDPOINT_NODE_IN &&
                strcmp(links[j].from.node_id, node_ids[u]) == 0) {
                int v = kc_flow_find_node_index(node_ids, node_count, links[j].to.node_id);
                if (v < 0) {
                    snprintf(error, error_size, "Unable to compute flow levels.");
                    return -1;
                }
                if (depth[(size_t)v] < depth[u] + 1) depth[(size_t)v] = depth[u] + 1;
                if (*max_depth < depth[(size_t)v]) *max_depth = depth[(size_t)v];
            }
        }
    }
    return 0;
}

/**
 * Builds bash CLI for one composed flow.
 * @param model Parsed flow model.
 * @param path Source flow path.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on build errors.
 */
int kc_flow_build_flow_cli(const kc_flow_model *model,
                           const char *path,
                           char *error,
                           size_t error_size) {
    kc_flow_link_entry links[KC_STDIO_MAX_INDEXES];
    char node_ids[KC_STDIO_MAX_INDEXES][128];
    size_t order[KC_STDIO_MAX_INDEXES];
    size_t depth[KC_STDIO_MAX_INDEXES];
    size_t node_count = 0, link_count = 0, i;
    size_t max_depth = 0;
    char flow_dir[1024];

    kc_flow_dirname(path, flow_dir, sizeof(flow_dir));
    if (kc_flow_collect_node_ids(model, node_ids, &node_count, error, error_size) != 0) return -1;
    if (kc_flow_collect_links(model, node_ids, node_count, links, &link_count, error, error_size) != 0) return -1;
    if (kc_flow_detect_cycle(model, node_ids, node_count, error, error_size) != 0) return -1;
    if (kc_flow_toposort(node_ids, node_count, links, link_count, order, error, error_size) != 0) return -1;
    if (kc_flow_compute_depths(node_ids,
                               node_count,
                               links,
                               link_count,
                               order,
                               depth,
                               &max_depth,
                               error,
                               error_size) != 0) return -1;

    printf("#!/usr/bin/env bash\nset -euo pipefail\n\n");
    printf("KC_FLOW_BIN=${KC_FLOW_BIN:-kc-flow}\ndeclare -A V\n\n");
    printf("TMP_DIR=\"$(mktemp -d)\"\n");
    printf("trap 'rm -rf \"$TMP_DIR\"' EXIT\n\n");

    for (i = 0; i < model->inputs.count; ++i) {
        char key[128];
        const char *id;
        snprintf(key, sizeof(key), "input.%d.id", model->inputs.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL) {
            printf(": \"${%s:?missing input '%s'}\"\n", id, id);
            printf("V[\"input.%s\"]=\"${%s}\"\n", id, id);
        }
    }
    printf("\n");

    for (i = 0; i <= max_depth; ++i) {
        size_t k;
        for (k = 0; k < node_count; ++k) {
            size_t idx = order[k];
            const char *node_id = node_ids[idx];
            int node_record;
            char key[128], node_var[160], contract_path[1024];
            const char *contract_rel;
            size_t j;
            if (depth[idx] != i) continue;

            node_record = kc_flow_find_node_record(model, node_id);
            if (node_record < 0) {
                snprintf(error, error_size, "Unable to resolve node in build flow.");
                return -1;
            }
            snprintf(key, sizeof(key), "node.%d.contract", node_record);
            contract_rel = kc_flow_model_get(model, key);
            if (contract_rel == NULL) {
                snprintf(error, error_size, "Missing node contract path in build flow.");
                return -1;
            }
            if (kc_flow_build_path(contract_path, sizeof(contract_path), flow_dir, contract_rel) != 0) {
                snprintf(error, error_size, "Unable to resolve node path in build flow.");
                return -1;
            }

            kc_flow_sanitize(node_var, sizeof(node_var), node_id);
            printf("echo \"[kc-flow] step node=%s contract=", node_id);
            kc_flow_print_sq(contract_path);
            printf("\" >&2\n");

            printf("\"$KC_FLOW_BIN\" --run ");
            kc_flow_print_sq(contract_path);
            for (j = 0; j < link_count; ++j) {
                if (links[j].to.kind == KC_FLOW_ENDPOINT_NODE_IN && strcmp(links[j].to.node_id, node_id) == 0) {
                    char src_key[192];
                    if (links[j].from.kind == KC_FLOW_ENDPOINT_INPUT) {
                        snprintf(src_key, sizeof(src_key), "input.%s", links[j].from.field_id);
                    } else {
                        snprintf(src_key, sizeof(src_key), "node.%s.out.%s", links[j].from.node_id, links[j].from.field_id);
                    }
                    printf(" --set \"input.%s=${V[\\\"%s\\\"]}\"", links[j].to.field_id, src_key);
                }
            }
            printf(" > \"$TMP_DIR/step_%s.out\" &\n", node_var);
        }
        printf("wait\n");
        for (k = 0; k < node_count; ++k) {
            size_t idx = order[k];
            const char *node_id = node_ids[idx];
            char node_var[160];
            size_t j;
            if (depth[idx] != i) continue;
            kc_flow_sanitize(node_var, sizeof(node_var), node_id);
            for (j = 0; j < link_count; ++j) {
                if (links[j].from.kind == KC_FLOW_ENDPOINT_NODE_OUT && strcmp(links[j].from.node_id, node_id) == 0) {
                    printf("if ! grep -q '^output.%s=' \"$TMP_DIR/step_%s.out\"; then\n",
                           links[j].from.field_id,
                           node_var);
                    printf("  echo \"missing output.%s from node %s\" >&2\n", links[j].from.field_id, node_id);
                    printf("  exit 1\nfi\n");
                    printf("V[\"node.%s.out.%s\"]=\"$(grep '^output.%s=' \"$TMP_DIR/step_%s.out\" | tail -n1 | cut -d= -f2-)\"\n",
                           node_id,
                           links[j].from.field_id,
                           links[j].from.field_id,
                           node_var);
                }
            }
        }
        printf("\n");
    }

    for (i = 0; i < link_count; ++i) {
        if (links[i].to.kind == KC_FLOW_ENDPOINT_OUTPUT) {
            if (links[i].from.kind == KC_FLOW_ENDPOINT_INPUT) {
                printf("V[\"output.%s\"]=\"${V[\\\"input.%s\\\"]}\"\n",
                       links[i].to.field_id,
                       links[i].from.field_id);
            } else {
                printf("V[\"output.%s\"]=\"${V[\\\"node.%s.out.%s\\\"]}\"\n",
                       links[i].to.field_id,
                       links[i].from.node_id,
                       links[i].from.field_id);
            }
        }
    }

    for (i = 0; i < model->outputs.count; ++i) {
        char key[128];
        const char *id;
        snprintf(key, sizeof(key), "output.%d.id", model->outputs.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL) {
            printf("printf 'output.%s=%%s\\n' \"${V[\\\"output.%s\\\"]}\"\n", id, id);
        }
    }
    return 0;
}
