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

static int kc_flow_value_put(kc_flow_overrides *values,
                             const char *key,
                             const char *value,
                             char *error,
                             size_t error_size) {
    if (kc_flow_overrides_add(values, key, value) != 0) {
        snprintf(error, error_size, "Unable to store flow runtime value.");
        return -1;
    }
    return 0;
}

int kc_flow_find_node_record_index(const kc_flow_model *model,
                                   const char *node_id) {
    size_t i;
    char key[128];

    for (i = 0; i < model->nodes.count; ++i) {
        const char *current_id;
        snprintf(key, sizeof(key), "node.%d.id", model->nodes.values[i]);
        current_id = kc_flow_model_get(model, key);
        if (current_id != NULL && strcmp(current_id, node_id) == 0) {
            return model->nodes.values[i];
        }
    }

    return -1;
}

int kc_flow_seed_parent_inputs(const kc_flow_overrides *overrides,
                               kc_flow_overrides *values,
                               char *error,
                               size_t error_size) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        if (strncmp(overrides->records[i].key, "input.", 6) == 0) {
            if (kc_flow_value_put(values,
                                  overrides->records[i].key,
                                  overrides->records[i].value,
                                  error,
                                  error_size) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int kc_flow_run_node(const kc_flow_model *model,
                     const char *flow_dir,
                     const char *node_id,
                     const kc_flow_overrides *node_inputs,
                     kc_flow_overrides *node_outputs,
                     char *error,
                     size_t error_size) {
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

    if (kc_flow_build_path(node_contract_path,
                           sizeof(node_contract_path),
                           flow_dir,
                           node_contract_rel) != 0) {
        snprintf(error, error_size, "Unable to resolve node contract path.");
        return -1;
    }

    if (!kc_flow_file_exists(node_contract_path)) {
        snprintf(error,
                 error_size,
                 "Node contract file not found: %s",
                 node_contract_path);
        return -1;
    }

    kc_flow_model_init(&child);
    if (kc_flow_load_file(node_contract_path, &child, error, error_size) != 0 ||
        kc_flow_validate_model(&child, error, error_size) != 0) {
        kc_flow_model_free(&child);
        return -1;
    }

    if (child.kind == KC_STDIO_FILE_FLOW) {
        int rc = kc_flow_run_flow(
            &child, node_inputs, node_contract_path, node_outputs, error, error_size
        );
        kc_flow_model_free(&child);
        return rc;
    }

    {
        kc_flow_run_output output;
        int rc;

        memset(&output, 0, sizeof(output));
        rc = kc_flow_run_contract(
            &child, node_inputs, node_contract_path, &output, error, error_size
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

int kc_flow_collect_node_outputs(const kc_flow_link_entry *links,
                                 size_t link_count,
                                 const char *node_id,
                                 const kc_flow_overrides *node_outputs,
                                 kc_flow_overrides *values,
                                 char *error,
                                 size_t error_size) {
    size_t j;

    for (j = 0; j < link_count; ++j) {
        char node_output_key[160];
        char parent_value_key[192];
        const char *resolved;

        if (links[j].from.kind != KC_FLOW_ENDPOINT_NODE_OUT ||
            strcmp(links[j].from.node_id, node_id) != 0) {
            continue;
        }

        snprintf(node_output_key,
                 sizeof(node_output_key),
                 "output.%s",
                 links[j].from.field_id);
        resolved = kc_flow_overrides_get(node_outputs, node_output_key);
        if (resolved == NULL) {
            snprintf(error,
                     error_size,
                     "Node %s did not produce output id: %s",
                     node_id,
                     links[j].from.field_id);
            return -1;
        }

        snprintf(parent_value_key,
                 sizeof(parent_value_key),
                 "node.%s.out.%s",
                 node_id,
                 links[j].from.field_id);
        if (kc_flow_value_put(values,
                              parent_value_key,
                              resolved,
                              error,
                              error_size) != 0) {
            return -1;
        }
    }

    return 0;
}

int kc_flow_resolve_final_outputs(const kc_flow_model *model,
                                  const kc_flow_link_entry *links,
                                  size_t link_count,
                                  const kc_flow_overrides *values,
                                  kc_flow_overrides *outputs,
                                  char *error,
                                  size_t error_size) {
    size_t i;
    char key[160];

    if (outputs == NULL) {
        return 0;
    }

    for (i = 0; i < link_count; ++i) {
        const char *resolved;

        if (links[i].to.kind != KC_FLOW_ENDPOINT_OUTPUT) {
            continue;
        }

        if (links[i].from.kind == KC_FLOW_ENDPOINT_INPUT) {
            snprintf(key, sizeof(key), "input.%s", links[i].from.field_id);
        } else {
            snprintf(key,
                     sizeof(key),
                     "node.%s.out.%s",
                     links[i].from.node_id,
                     links[i].from.field_id);
        }
        resolved = kc_flow_overrides_get(values, key);
        if (resolved == NULL) {
            snprintf(error,
                     error_size,
                     "Unable to resolve parent output for output.%s",
                     links[i].to.field_id);
            return -1;
        }

        snprintf(key, sizeof(key), "output.%s", links[i].to.field_id);
        if (kc_flow_value_put(outputs, key, resolved, error, error_size) != 0) {
            return -1;
        }
    }

    for (i = 0; i < model->outputs.count; ++i) {
        const char *output_id;

        snprintf(key, sizeof(key), "output.%d.id", model->outputs.values[i]);
        output_id = kc_flow_model_get(model, key);
        if (output_id == NULL) {
            continue;
        }

        snprintf(key, sizeof(key), "output.%s", output_id);
        if (kc_flow_overrides_get(outputs, key) == NULL) {
            snprintf(error, error_size, "Unresolved flow output id: %s", output_id);
            return -1;
        }
    }

    return 0;
}
