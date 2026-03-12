/**
 * graph.c
 * Summary: Node lookup and contract execution for composed flows.
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
 * Resolves one node contract.
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
    const kc_flow_runtime_cfg *cfg,
    const char *flow_dir,
    const char *node_id,
    const kc_flow_overrides *runtime_params,
    int fd_in,
    int *fd_out,
    char *error,
    size_t error_size
) {
    int node_record;
    const char *target;
    char path[KC_FLOW_MAX_PATH];
    kc_flow_model child;
    kc_flow_overrides node_params;
    int child_fd_out;
    int status_fd;

    child_fd_out = *fd_out;
    *fd_out = -1;
    status_fd = cfg != NULL ? cfg->fd_status : -1;
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
    kc_flow_overrides_init(&node_params);
    if (kc_flow_load_file(path, &child, error, error_size) != 0 ||
            kc_flow_collect_node_params(
                model,
                node_record,
                runtime_params,
                &node_params,
                error,
                error_size
            ) != 0 ||
            kc_flow_validate_model(&child, error, error_size) != 0) {
        kc_flow_overrides_free(&node_params);
        kc_flow_model_free(&child);
        return -1;
    }
    if (child.kind != KC_FLOW_FILE_CONTRACT) {
        snprintf(error, error_size, "Node target must be one contract.");
        kc_flow_overrides_free(&node_params);
        kc_flow_model_free(&child);
        return -1;
    }
    if (child.outputs.count > 0 && child_fd_out < 0) {
        child_fd_out = kc_flow_create_artifact_fd(error, error_size);
    } else if (child.outputs.count == 0) {
        child_fd_out = -1;
    }
    if (child.outputs.count > 0 && child_fd_out < 0) {
        kc_flow_overrides_free(&node_params);
        kc_flow_model_free(&child);
        return -1;
    }
    kc_flow_status_write_node_event(
        status_fd,
        "node.started",
        node_id,
        "contract",
        path,
        NULL,
        NULL
    );
    fprintf(stderr, "[kc-flow] step node=%s contract=%s\n", node_id, path);
    {
        int rc;

        rc = kc_flow_run_contract(
            &child,
            &node_params,
            path,
            fd_in,
            child_fd_out,
            status_fd,
            error,
            error_size
        );
        kc_flow_status_write_node_event(
            status_fd,
            "node.finished",
            node_id,
            "contract",
            path,
            rc == 0 ? "ok" : "error",
            rc == 0 ? NULL : error
        );
        if (rc == 0) {
            *fd_out = child_fd_out;
        } else if (child_fd_out >= 0) {
            kc_flow_release_fd(child_fd_out);
        }
        kc_flow_overrides_free(&node_params);
        kc_flow_model_free(&child);
        return rc;
    }
}
