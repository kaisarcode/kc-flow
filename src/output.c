/**
 * output.c
 * Summary: Runtime template resolution and output materialization.
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
 * Frees one run output.
 * @param output Output struct.
 * @return void
 */
void kc_flow_run_output_free(kc_flow_run_output *output) {
    free(output->stdout_text);
    free(output->stderr_text);
    output->stdout_text = NULL;
    output->stderr_text = NULL;
    output->exit_code = 0;
}

/**
 * Collects contract outputs into one override store.
 * @param model Parsed model.
 * @param output Captured output.
 * @param values Output store.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_collect_contract_outputs(
    const kc_flow_model *model,
    const kc_flow_run_output *output,
    kc_flow_overrides *values,
    char *error,
    size_t error_size
) {
    size_t i;

    for (i = 0; i < model->outputs.count; ++i) {
        const char *output_id;
        const char *mode;
        char key[128];
        const char *materialized;

        output_id = kc_flow_lookup_indexed_value(
            model,
            &model->outputs,
            "output.",
            model->outputs.values[i],
            "id"
        );
        if (output_id == NULL) {
            continue;
        }
        mode = kc_flow_lookup_indexed_id_value(
            model,
            &model->bind_output,
            "bind.output.",
            output_id,
            "mode"
        );
        if (mode == NULL) {
            snprintf(error, error_size, "Missing bind.output for %s.", output_id);
            return -1;
        }
        if (strcmp(mode, "stdout") == 0) {
            materialized = output->stdout_text != NULL ? output->stdout_text : "";
        } else if (strcmp(mode, "stderr") == 0) {
            materialized = output->stderr_text != NULL ? output->stderr_text : "";
        } else if (strcmp(mode, "exit_code") == 0) {
            char exit_text[32];

            snprintf(exit_text, sizeof(exit_text), "%d", output->exit_code);
            snprintf(key, sizeof(key), "output.%s", output_id);
            if (kc_flow_overrides_add(values, key, exit_text) != 0) {
                snprintf(error, error_size, "Unable to store output value.");
                return -1;
            }
            continue;
        } else {
            snprintf(error, error_size, "Unsupported bind mode: %s", mode);
            return -1;
        }
        snprintf(key, sizeof(key), "output.%s", output_id);
        if (kc_flow_overrides_add(values, key, materialized) != 0) {
            snprintf(error, error_size, "Unable to store output value.");
            return -1;
        }
    }
    return 0;
}

/**
 * Prints normalized contract outputs.
 * @param model Parsed model.
 * @param output Captured output.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_print_contract_outputs(
    const kc_flow_model *model,
    const kc_flow_run_output *output
) {
    kc_flow_overrides values;
    char error[256];
    size_t i;

    kc_flow_overrides_init(&values);
    if (kc_flow_collect_contract_outputs(model, output, &values, error, sizeof(error)) != 0) {
        kc_flow_overrides_free(&values);
        return -1;
    }
    for (i = 0; i < values.count; ++i) {
        printf("%s=%s\n", values.records[i].key, values.records[i].value);
    }
    kc_flow_overrides_free(&values);
    return 0;
}
