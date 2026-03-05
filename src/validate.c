/**
 * validate.c
 * Summary: Model validation and shared path/lookup helpers.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "model.h"

#include <stdio.h>
#include <string.h>

static int kc_flow_validate_contract(const kc_flow_model *model,
                                     char *error,
                                     size_t error_size) {
    if (model->id == NULL) {
        snprintf(error, error_size, "Missing required key: contract.id");
        return -1;
    }

    if (model->name == NULL) {
        snprintf(error, error_size, "Missing required key: contract.name");
        return -1;
    }

    if (model->runtime_script == NULL) {
        snprintf(error, error_size, "Missing required key: runtime.script");
        return -1;
    }

    return 0;
}

static int kc_flow_validate_flow(const kc_flow_model *model,
                                 char *error,
                                 size_t error_size) {
    size_t i;
    char key[128];

    if (model->id == NULL) {
        snprintf(error, error_size, "Missing required key: flow.id");
        return -1;
    }

    if (model->name == NULL) {
        snprintf(error, error_size, "Missing required key: flow.name");
        return -1;
    }

    for (i = 0; i < model->nodes.count; ++i) {
        snprintf(key, sizeof(key), "node.%d.id", model->nodes.values[i]);
        if (kc_flow_model_get(model, key) == NULL) {
            snprintf(error, error_size, "Missing required key: %s", key);
            return -1;
        }

        snprintf(key, sizeof(key), "node.%d.contract", model->nodes.values[i]);
        if (kc_flow_model_get(model, key) == NULL) {
            snprintf(error, error_size, "Missing required key: %s", key);
            return -1;
        }
    }

    for (i = 0; i < model->links.count; ++i) {
        snprintf(key, sizeof(key), "link.%d.from", model->links.values[i]);
        if (kc_flow_model_get(model, key) == NULL) {
            snprintf(error, error_size, "Missing required key: %s", key);
            return -1;
        }

        snprintf(key, sizeof(key), "link.%d.to", model->links.values[i]);
        if (kc_flow_model_get(model, key) == NULL) {
            snprintf(error, error_size, "Missing required key: %s", key);
            return -1;
        }
    }

    return 0;
}

/**
 * Validates required fields for parsed model kind.
 * @param model Parsed model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on validation errors.
 */
int kc_flow_validate_model(const kc_flow_model *model,
                           char *error,
                           size_t error_size) {
    if (model->kind == KC_STDIO_FILE_CONTRACT) {
        return kc_flow_validate_contract(model, error, error_size);
    }

    if (model->kind == KC_STDIO_FILE_FLOW) {
        return kc_flow_validate_flow(model, error, error_size);
    }

    snprintf(error, error_size, "Unable to determine file kind.");
    return -1;
}

/**
 * Builds an absolute or base-relative path.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @param base Base directory.
 * @param value Relative or absolute path.
 * @return int 0 on success; non-zero on invalid/truncated paths.
 */
int kc_flow_build_path(char *buffer,
                       size_t size,
                       const char *base,
                       const char *value) {
    size_t base_len;

    if (value == NULL || *value == '\0') {
        return -1;
    }

    if (value[0] == '/') {
        return snprintf(buffer, size, "%s", value) >= (int)size ? -1 : 0;
    }

    base_len = strlen(base);
    if (base_len == 0) {
        return snprintf(buffer, size, "%s", value) >= (int)size ? -1 : 0;
    }

    return snprintf(buffer, size, "%s/%s", base, value) >= (int)size ? -1 : 0;
}

/**
 * Computes directory portion for a file path.
 * @param path Input file path.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @return void
 */
void kc_flow_dirname(const char *path, char *buffer, size_t size) {
    const char *slash;
    size_t len;

    slash = strrchr(path, '/');
    if (slash == NULL) {
        snprintf(buffer, size, ".");
        return;
    }

    len = (size_t)(slash - path);
    if (len == 0) {
        snprintf(buffer, size, "/");
        return;
    }

    if (len >= size) {
        len = size - 1;
    }

    memcpy(buffer, path, len);
    buffer[len] = '\0';
}

/**
 * Looks up an indexed section value by logical id and field.
 * @param model Parsed model.
 * @param set Index set to scan.
 * @param prefix Section key prefix.
 * @param id Logical id to match.
 * @param field Field name to resolve.
 * @return const char* Value if found; NULL otherwise.
 */
const char *kc_flow_lookup_indexed_id_value(const kc_flow_model *model,
                                            const kc_flow_index_set *set,
                                            const char *prefix,
                                            const char *id,
                                            const char *field) {
    size_t i;
    char key[128];
    const char *current_id;

    for (i = 0; i < set->count; ++i) {
        snprintf(key, sizeof(key), "%s%d.id", prefix, set->values[i]);
        current_id = kc_flow_model_get(model, key);
        if (current_id != NULL && strcmp(current_id, id) == 0) {
            snprintf(key, sizeof(key), "%s%d.%s", prefix, set->values[i], field);
            return kc_flow_model_get(model, key);
        }
    }

    return NULL;
}
