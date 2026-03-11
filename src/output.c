/**
 * output.c
 * Summary: Runtime template resolution and effective node parameter binding.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Resolves one placeholder reference.
 * @param model Parsed model.
 * @param overrides Runtime overrides.
 * @param name Placeholder name.
 * @return const char* Resolved value or NULL.
 */
static const char *kc_flow_resolve_value(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *name
) {
    const char *value;

    value = kc_flow_overrides_get(overrides, name);
    if (value != NULL) {
        return value;
    }
    if (strncmp(name, "param.", 6) == 0) {
        return kc_flow_lookup_indexed_id_value(
            model,
            &model->params,
            "param.",
            name + 6,
            "default"
        );
    }
    return NULL;
}

/**
 * Resolves one template string.
 * @param model Parsed model.
 * @param overrides Runtime overrides.
 * @param text Template text.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return char* Resolved string or NULL.
 */
char *kc_flow_resolve_template(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *text,
    char *error,
    size_t error_size
) {
    size_t i;
    size_t capacity;
    size_t length;
    char *result;

    if (text == NULL) {
        return NULL;
    }
    capacity = strlen(text) + 1;
    result = malloc(capacity);
    if (result == NULL) {
        snprintf(error, error_size, "Out of memory while resolving template.");
        return NULL;
    }
    length = 0;
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '<') {
            char name[256];
            const char *value;
            size_t start;
            size_t end;
            size_t name_len;
            size_t value_len;

            start = i + 1;
            end = start;
            while (text[end] != '\0' && text[end] != '>') {
                end++;
            }
            if (text[end] != '>') {
                free(result);
                snprintf(error, error_size, "Unclosed placeholder.");
                return NULL;
            }
            name_len = end - start;
            if (name_len == 0 || name_len >= sizeof(name)) {
                free(result);
                snprintf(error, error_size, "Invalid placeholder size.");
                return NULL;
            }
            memcpy(name, text + start, name_len);
            name[name_len] = '\0';
            value = kc_flow_resolve_value(model, overrides, name);
            if (value == NULL) {
                free(result);
                snprintf(error, error_size, "Unable to resolve: <%.180s>", name);
                return NULL;
            }
            value_len = strlen(value);
            while (length + value_len + 1 > capacity) {
                char *grown;

                capacity *= 2;
                grown = realloc(result, capacity);
                if (grown == NULL) {
                    free(result);
                    snprintf(error, error_size, "Out of memory while growing template.");
                    return NULL;
                }
                result = grown;
            }
            memcpy(result + length, value, value_len);
            length += value_len;
            i = end;
            continue;
        }
        if (length + 2 > capacity) {
            char *grown;

            capacity *= 2;
            grown = realloc(result, capacity);
            if (grown == NULL) {
                free(result);
                snprintf(error, error_size, "Out of memory while growing template.");
                return NULL;
            }
            result = grown;
        }
        result[length++] = text[i];
    }
    result[length] = '\0';
    return result;
}

/**
 * Collects effective node parameters from runtime and node-local definitions.
 * @param model Parent model.
 * @param node_record Numeric node record id.
 * @param runtime_params Runtime override store.
 * @param node_params Output node parameter store.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_collect_node_params(
    const kc_flow_model *model,
    int node_record,
    const kc_flow_overrides *runtime_params,
    kc_flow_overrides *node_params,
    char *error,
    size_t error_size
) {
    size_t i;
    char prefix[64];

    kc_flow_overrides_init(node_params);
    for (i = 0; i < runtime_params->count; ++i) {
        if (strncmp(runtime_params->records[i].key, "param.", 6) != 0) {
            continue;
        }
        if (kc_flow_overrides_add(
                node_params,
                runtime_params->records[i].key,
                runtime_params->records[i].value
            ) != 0) {
            snprintf(error, error_size, "Unable to seed node parameters.");
            kc_flow_overrides_free(node_params);
            return -1;
        }
    }
    snprintf(prefix, sizeof(prefix), "node.%d.param.", node_record);
    for (i = 0; i < model->record_count; ++i) {
        char key[128];
        char *resolved;

        if (strncmp(model->records[i].key, prefix, strlen(prefix)) != 0) {
            continue;
        }
        snprintf(key, sizeof(key), "param.%s", model->records[i].key + strlen(prefix));
        resolved = kc_flow_resolve_template(
            model,
            runtime_params,
            model->records[i].value,
            error,
            error_size
        );
        if (resolved == NULL ||
                kc_flow_overrides_add(node_params, key, resolved) != 0) {
            free(resolved);
            snprintf(error, error_size, "Unable to store node parameter.");
            kc_flow_overrides_free(node_params);
            return -1;
        }
        free(resolved);
    }
    return 0;
}
