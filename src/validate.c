/**
 * validate.c
 * Summary: Structural validation for parsed flows and contracts.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>

/**
 * Validates one parsed model.
 * @param model Parsed model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_validate_model(
    const kc_flow_model *model,
    char *error,
    size_t error_size
) {
    size_t i;

    if (model->id == NULL || model->name == NULL) {
        snprintf(error, error_size, "Missing id or name.");
        return -1;
    }
    if (model->inputs.count > 1 || model->outputs.count > 1) {
        snprintf(error, error_size, "Only one input and one output are supported.");
        return -1;
    }
    if (model->kind == KC_FLOW_FILE_CONTRACT) {
        if (model->runtime_command == NULL) {
            snprintf(error, error_size, "Missing required key: runtime.command");
            return -1;
        }
        return 0;
    }
    for (i = 0; i < model->nodes.count; ++i) {
        if (kc_flow_lookup_indexed_value(
                model,
                &model->nodes,
                "node.",
                model->nodes.values[i],
                "id"
            ) == NULL ||
                kc_flow_lookup_indexed_value(
                    model,
                    &model->nodes,
                    "node.",
                    model->nodes.values[i],
                    "contract"
                ) == NULL) {
            snprintf(error, error_size, "Missing node id or contract.");
            return -1;
        }
    }
    for (i = 0; i < model->links.count; ++i) {
        if (kc_flow_lookup_indexed_value(
                model,
                &model->links,
                "link.",
                model->links.values[i],
                "from"
            ) == NULL ||
                kc_flow_lookup_indexed_value(
                    model,
                    &model->links,
                    "link.",
                    model->links.values[i],
                    "to"
                ) == NULL) {
            snprintf(error, error_size, "Missing link from or to.");
            return -1;
        }
    }
    return kc_flow_validate_cycles(model, error, error_size);
}
