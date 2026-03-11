/**
 * graph.c
 * Summary: Node lookup and nested-node execution for composed flows.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <string.h>

/**
 * Resolves one node record id by node id.
 * @param model Parsed model.
 * @param node_id Logical node id.
 * @return int Record id or -1.
 */
static int kc_flow_find_node_record(const kc_flow_model *model, const char *node_id) {
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
            return model->nodes.values[i];
        }
    }
    return -1;
}

/**
 * Collects dense node ids.
 * @param model Parsed model.
 * @param node_ids Output node ids.
 * @param count Output count.
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

    *count = model->nodes.count;
    if (*count > KC_FLOW_MAX_INDEXES) {
        snprintf(error, error_size, "Too many nodes.");
        return -1;
    }
    for (i = 0; i < model->nodes.count; ++i) {
        const char *node_id;

        node_id = kc_flow_lookup_indexed_value(
            model,
            &model->nodes,
            "node.",
            model->nodes.values[i],
            "id"
        );
        if (node_id == NULL || strlen(node_id) >= sizeof(node_ids[0])) {
            snprintf(error, error_size, "Invalid node id.");
            return -1;
        }
        snprintf(node_ids[i], sizeof(node_ids[0]), "%s", node_id);
    }
    return 0;
}

/**
 * Resolves one node contract or nested flow.
 * @param model Parent model.
 * @param flow_dir Parent directory.
 * @param node_id Logical node id.
 * @param node_inputs Input overrides.
 * @param node_outputs Output overrides.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_node(
    const kc_flow_model *model,
    const char *flow_dir,
    const char *node_id,
    const kc_flow_overrides *node_inputs,
    kc_flow_overrides *node_outputs,
    char *error,
    size_t error_size
) {
    int node_record;
    const char *target;
    char path[KC_FLOW_MAX_PATH];
    kc_flow_model child;

    node_record = kc_flow_find_node_record(model, node_id);
    if (node_record < 0) {
        snprintf(error, error_size, "Unknown node: %s", node_id);
        return -1;
    }
    target = kc_flow_lookup_indexed_value(
        model,
        &model->nodes,
        "node.",
        node_record,
        "contract"
    );
    if (target == NULL || kc_flow_build_path(path, sizeof(path), flow_dir, target) != 0) {
        snprintf(error, error_size, "Invalid node contract path.");
        return -1;
    }
    if (!kc_flow_file_exists(path)) {
        snprintf(error, error_size, "Node contract file not found: %s", path);
        return -1;
    }
    kc_flow_model_init(&child);
    if (kc_flow_load_file(path, &child, error, error_size) != 0 ||
            kc_flow_validate_model(&child, error, error_size) != 0) {
        kc_flow_model_free(&child);
        return -1;
    }
    fprintf(stderr, "[kc-flow] step node=%s contract=%s\n", node_id, path);
    if (child.kind == KC_FLOW_FILE_FLOW) {
        int rc;

        rc = kc_flow_run_flow(&child, node_inputs, path, node_outputs, error, error_size);
        kc_flow_model_free(&child);
        return rc;
    }
    {
        kc_flow_run_output output;
        int rc;

        rc = kc_flow_run_contract(&child, node_inputs, path, &output, error, error_size);
        if (rc == 0 && output.exit_code == 0) {
            rc = kc_flow_collect_contract_outputs(
                &child,
                &output,
                node_outputs,
                error,
                error_size
            );
        } else if (rc == 0) {
            snprintf(error, error_size, "Node contract exited with non-zero status.");
            rc = -1;
        }
        kc_flow_run_output_free(&output);
        kc_flow_model_free(&child);
        return rc;
    }
}
