/**
 * flow.c
 * Summary: Flow execution helpers for nodes and value propagation.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "flow.h"
#include "graph.h"

#include <stdio.h>
#include <string.h>

#define KC_FLOW_PATH_BUFFER 4096

/**
 * Executes one node contract or nested flow.
 * @param model Parent flow model.
 * @param flow_dir Parent flow directory.
 * @param node_id Node identifier.
 * @param node_inputs Resolved node inputs.
 * @param node_outputs Output node outputs.
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
    const char *node_contract_rel;
    char node_contract_path[KC_FLOW_PATH_BUFFER];
    kc_flow_model child;

    node_record = kc_flow_find_node_record_index(model, node_id);
    if (node_record < 0) {
        snprintf(error, error_size, "Unable to resolve node record: %s", node_id);
        return -1;
    }

    {
        char key[128];
        snprintf(key, sizeof(key), "node.%d.contract", node_record);
        node_contract_rel = kc_flow_model_get(model, key);
    }
    if (node_contract_rel == NULL) {
        snprintf(error, error_size, "Missing node contract path.");
        return -1;
    }

    if (kc_flow_build_path(
            node_contract_path,
            sizeof(node_contract_path),
            flow_dir,
            node_contract_rel
        ) != 0) {
        snprintf(error, error_size, "Unable to resolve node contract path.");
        return -1;
    }

    if (!kc_flow_file_exists(node_contract_path)) {
        snprintf(
            error,
            error_size,
            "Node contract file not found: %s",
            node_contract_path
        );
        return -1;
    }

    fprintf(
        stderr,
        "[kc-flow] step node=%s contract=%s\n",
        node_id,
        node_contract_path
    );

    kc_flow_model_init(&child);
    if (kc_flow_load_file(node_contract_path, &child, error, error_size) != 0 ||
        kc_flow_validate_model(&child, error, error_size) != 0) {
        kc_flow_model_free(&child);
        return -1;
    }

    if (child.kind == KC_STDIO_FILE_FLOW) {
        int rc;

        rc = kc_flow_run_flow(
            &child,
            node_inputs,
            node_contract_path,
            node_outputs,
            error,
            error_size
        );
        kc_flow_model_free(&child);
        return rc;
    }

    {
        kc_flow_run_output output;
        int rc;

        memset(&output, 0, sizeof(output));
        rc = kc_flow_run_contract(
            &child,
            node_inputs,
            node_contract_path,
            &output,
            error,
            error_size
        );
        if (rc != 0) {
            kc_flow_run_output_free(&output);
            kc_flow_model_free(&child);
            return -1;
        }

        if (output.exit_code != 0) {
            kc_flow_run_output_free(&output);
            kc_flow_model_free(&child);
            snprintf(error, error_size, "Node contract exited with non-zero status.");
            return -1;
        }

        rc = kc_flow_collect_contract_outputs(
            &child, &output, node_outputs, error, error_size
        );
        kc_flow_run_output_free(&output);
        kc_flow_model_free(&child);
        return rc;
    }
}
