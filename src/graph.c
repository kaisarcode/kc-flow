/**
 * graph.c
 * Summary: Composed flow runtime orchestrator.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "graph.h"

#include "flow.h"
#include "link.h"

#include <stdio.h>
#include <string.h>

#define KC_FLOW_PATH_BUFFER 4096

/**
 * Runs one composed flow graph with nested contract/flow dispatch.
 * @param model Parsed flow model.
 * @param overrides Runtime overrides.
 * @param path Source flow path.
 * @param outputs Optional collected `output.*` values.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on execution/validation failure.
 */
int kc_flow_run_flow(const kc_flow_model *model,
                     const kc_flow_overrides *overrides,
                     const char *path,
                     kc_flow_overrides *outputs,
                     char *error,
                     size_t error_size) {
    kc_flow_link_entry links[KC_STDIO_MAX_INDEXES];
    char node_ids[KC_STDIO_MAX_INDEXES][128];
    unsigned char executed[KC_STDIO_MAX_INDEXES];
    kc_flow_overrides values;
    char flow_dir[KC_FLOW_PATH_BUFFER];
    size_t link_count = 0;
    size_t completed = 0;
    size_t node_count = 0;
    size_t i;

    memset(executed, 0, sizeof(executed));
    kc_flow_overrides_init(&values);
    if (outputs != NULL) {
        kc_flow_overrides_init(outputs);
    }

    kc_flow_dirname(path, flow_dir, sizeof(flow_dir));

    if (kc_flow_collect_node_ids(model, node_ids, &node_count, error, error_size) != 0) {
        kc_flow_overrides_free(&values);
        return -1;
    }

    if (kc_flow_detect_cycle(model, node_ids, node_count, error, error_size) != 0) {
        kc_flow_overrides_free(&values);
        return -1;
    }

    if (kc_flow_collect_links(model,
                              node_ids,
                              node_count,
                              links,
                              &link_count,
                              error,
                              error_size) != 0) {
        kc_flow_overrides_free(&values);
        return -1;
    }

    if (kc_flow_seed_parent_inputs(overrides, &values, error, error_size) != 0) {
        kc_flow_overrides_free(&values);
        return -1;
    }

    while (completed < node_count) {
        int progressed = 0;

        for (i = 0; i < node_count; ++i) {
            kc_flow_overrides node_inputs;
            kc_flow_overrides node_outputs;
            size_t j;
            int ready = 1;

            if (executed[i]) {
                continue;
            }

            kc_flow_overrides_init(&node_inputs);
            kc_flow_overrides_init(&node_outputs);

            for (j = 0; j < link_count; ++j) {
                const char *source_value;
                char source_key[160];
                char target_key[160];

                if (links[j].to.kind != KC_FLOW_ENDPOINT_NODE_IN ||
                    strcmp(links[j].to.node_id, node_ids[i]) != 0) {
                    continue;
                }

                if (links[j].from.kind == KC_FLOW_ENDPOINT_INPUT) {
                    snprintf(source_key,
                             sizeof(source_key),
                             "input.%s",
                             links[j].from.field_id);
                } else {
                    snprintf(source_key,
                             sizeof(source_key),
                             "node.%s.out.%s",
                             links[j].from.node_id,
                             links[j].from.field_id);
                }

                source_value = kc_flow_overrides_get(&values, source_key);
                if (source_value == NULL) {
                    ready = 0;
                    break;
                }

                snprintf(target_key,
                         sizeof(target_key),
                         "input.%s",
                         links[j].to.field_id);
                if (kc_flow_overrides_add(&node_inputs, target_key, source_value) != 0) {
                    kc_flow_overrides_free(&node_inputs);
                    kc_flow_overrides_free(&node_outputs);
                    kc_flow_overrides_free(&values);
                    snprintf(error, error_size, "Unable to prepare node inputs.");
                    return -1;
                }
            }

            if (!ready) {
                kc_flow_overrides_free(&node_inputs);
                kc_flow_overrides_free(&node_outputs);
                continue;
            }

            if (kc_flow_run_node(model,
                                 flow_dir,
                                 node_ids[i],
                                 &node_inputs,
                                 &node_outputs,
                                 error,
                                 error_size) != 0) {
                kc_flow_overrides_free(&node_inputs);
                kc_flow_overrides_free(&node_outputs);
                kc_flow_overrides_free(&values);
                return -1;
            }

            if (kc_flow_collect_node_outputs(links,
                                             link_count,
                                             node_ids[i],
                                             &node_outputs,
                                             &values,
                                             error,
                                             error_size) != 0) {
                kc_flow_overrides_free(&node_inputs);
                kc_flow_overrides_free(&node_outputs);
                kc_flow_overrides_free(&values);
                return -1;
            }

            executed[i] = 1;
            completed++;
            progressed = 1;
            kc_flow_overrides_free(&node_inputs);
            kc_flow_overrides_free(&node_outputs);
        }

        if (!progressed) {
            kc_flow_overrides_free(&values);
            snprintf(error, error_size, "Unable to resolve flow dependencies.");
            return -1;
        }
    }

    i = kc_flow_resolve_final_outputs(
        model, links, link_count, &values, outputs, error, error_size
    );
    kc_flow_overrides_free(&values);
    return i;
}
