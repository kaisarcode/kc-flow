/**
 * runtime.c
 * Summary: Model validation and composed flow execution.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <string.h>

/**
 * Matches one destination prefix for one node input.
 * @param endpoint Link destination endpoint.
 * @param node_id Logical node id.
 * @return int 1 when matched; otherwise 0.
 */
static int kc_flow_is_node_input_endpoint(
    const char *endpoint,
    const char *node_id
) {
    size_t node_len;
    if (strncmp(endpoint, "node.", 5) != 0) {
        return 0;
    }
    node_len = strlen(node_id);
    if (strncmp(endpoint + 5, node_id, node_len) != 0) {
        return 0;
    }
    return strncmp(endpoint + 5 + node_len, ".in.", 4) == 0;
}

/**
 * Executes one flow.
 * @param model Parsed model.
 * @param overrides Runtime overrides.
 * @param path Source path.
 * @param outputs Output overrides.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_flow(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *path,
    kc_flow_overrides *outputs,
    char *error,
    size_t error_size
) {
    char node_ids[KC_FLOW_MAX_INDEXES][128];
    unsigned char executed[KC_FLOW_MAX_INDEXES];
    kc_flow_overrides values;
    char flow_dir[KC_FLOW_MAX_PATH];
    size_t node_count;
    size_t completed;
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
    for (i = 0; i < overrides->count; ++i) {
        if (kc_flow_overrides_add(&values, overrides->records[i].key, overrides->records[i].value) != 0) {
            kc_flow_overrides_free(&values);
            snprintf(error, error_size, "Unable to seed runtime values.");
            return -1;
        }
    }
    completed = 0;
    while (completed < node_count) {
        int progressed;
        progressed = 0;
        for (i = 0; i < node_count; ++i) {
            kc_flow_overrides node_inputs;
            kc_flow_overrides node_outputs;
            size_t j;
            int ready;
            if (executed[i]) {
                continue;
            }
            kc_flow_overrides_init(&node_inputs);
            kc_flow_overrides_init(&node_outputs);
            ready = 1;
            for (j = 0; j < model->links.count; ++j) {
                const char *from;
                const char *to;
                char source_key[160];
                const char *source_value;
                const char *input_id;
                from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[j], "from");
                to = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[j], "to");
                if (from == NULL || to == NULL) {
                    continue;
                }
                if (!kc_flow_is_node_input_endpoint(to, node_ids[i])) {
                    continue;
                }
                if (snprintf(source_key, sizeof(source_key), "%s", from) >=
                        (int)sizeof(source_key)) {
                    kc_flow_overrides_free(&node_inputs);
                    kc_flow_overrides_free(&node_outputs);
                    kc_flow_overrides_free(&values);
                    snprintf(error, error_size, "Source endpoint is too long.");
                    return -1;
                }
                source_value = kc_flow_overrides_get(&values, source_key);
                if (source_value == NULL) {
                    ready = 0;
                    break;
                }
                input_id = to + 5 + strlen(node_ids[i]) + 4;
                if (strlen(input_id) + strlen("input.") >= sizeof(source_key)) {
                    kc_flow_overrides_free(&node_inputs);
                    kc_flow_overrides_free(&node_outputs);
                    kc_flow_overrides_free(&values);
                    snprintf(error, error_size, "Input endpoint is too long.");
                    return -1;
                }
                if (snprintf(source_key, sizeof(source_key), "input.%s", input_id) >=
                        (int)sizeof(source_key)) {
                    kc_flow_overrides_free(&node_inputs);
                    kc_flow_overrides_free(&node_outputs);
                    kc_flow_overrides_free(&values);
                    snprintf(error, error_size, "Unable to normalize node input.");
                    return -1;
                }
                if (kc_flow_overrides_add(&node_inputs, source_key, source_value) != 0) {
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
            if (kc_flow_run_node(model, flow_dir, node_ids[i], &node_inputs, &node_outputs, error, error_size) != 0) {
                kc_flow_overrides_free(&node_inputs);
                kc_flow_overrides_free(&node_outputs);
                kc_flow_overrides_free(&values);
                return -1;
            }
            for (j = 0; j < node_outputs.count; ++j) {
                char key[192];
                snprintf(key, sizeof(key), "node.%s.out.%s", node_ids[i], node_outputs.records[j].key + strlen("output."));
                if (kc_flow_overrides_add(&values, key, node_outputs.records[j].value) != 0) {
                    kc_flow_overrides_free(&node_inputs);
                    kc_flow_overrides_free(&node_outputs);
                    kc_flow_overrides_free(&values);
                    snprintf(error, error_size, "Unable to store node output.");
                    return -1;
                }
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
    if (outputs != NULL) {
        for (i = 0; i < model->links.count; ++i) {
            const char *from;
            const char *to;
            const char *value;
            from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "from");
            to = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "to");
            if (from == NULL || to == NULL || strncmp(to, "output.", 7) != 0) {
                continue;
            }
            value = kc_flow_overrides_get(&values, from);
            if (value == NULL || kc_flow_overrides_add(outputs, to, value) != 0) {
                kc_flow_overrides_free(&values);
                snprintf(error, error_size, "Unable to resolve final outputs.");
                return -1;
            }
        }
    }
    kc_flow_overrides_free(&values);
    return 0;
}
