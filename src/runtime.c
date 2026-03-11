/**
 * runtime.c
 * Summary: Runtime output lifecycle and output materialization helpers.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Duplicates one C string.
 * @param text Source text.
 * @return char* Heap copy on success; NULL on error.
 */
static char *kc_flow_strdup(const char *text) {
    size_t len;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    len = strlen(text);
    copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, len + 1);
    return copy;
}

/**
 * Releases buffers captured from one contract run.
 * @param output Captured runtime output.
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
 * Finds one bind.output mapping by logical output id.
 * @param model Parsed contract model.
 * @param output_id Logical output id.
 * @param mode Output binding mode.
 * @param path Output binding path.
 * @return int 0 on success; non-zero if not found.
 */
static int kc_flow_find_output_binding(
    const kc_flow_model *model,
    const char *output_id,
    const char **mode,
    const char **path
) {
    size_t i;
    char key[128];
    const char *current_id;

    for (i = 0; i < model->bind_output.count; ++i) {
        snprintf(
            key,
            sizeof(key),
            "bind.output.%d.id",
            model->bind_output.values[i]
        );
        current_id = kc_flow_model_get(model, key);
        if (current_id != NULL && strcmp(current_id, output_id) == 0) {
            snprintf(
                key,
                sizeof(key),
                "bind.output.%d.mode",
                model->bind_output.values[i]
            );
            *mode = kc_flow_model_get(model, key);
            snprintf(
                key,
                sizeof(key),
                "bind.output.%d.path",
                model->bind_output.values[i]
            );
            *path = kc_flow_model_get(model, key);
            return 0;
        }
    }

    return -1;
}

/**
 * Materializes one bound output value from captured runtime data.
 * @param output_id Logical output id.
 * @param mode Binding mode.
 * @param path Binding path.
 * @param output Captured runtime output.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return char* Heap value on success; NULL on error.
 */
static char *kc_flow_materialize_bound_output(
    const char *output_id,
    const char *mode,
    const char *path,
    const kc_flow_run_output *output,
    char *error,
    size_t error_size
) {
    char buffer[64];

    if (mode == NULL) {
        snprintf(error, error_size, "Missing output binding mode for %s.", output_id);
        return NULL;
    }

    if (strcmp(mode, "stdout") == 0) {
        return kc_flow_strdup(output->stdout_text != NULL ? output->stdout_text : "");
    }

    if (strcmp(mode, "stderr") == 0) {
        return kc_flow_strdup(output->stderr_text != NULL ? output->stderr_text : "");
    }

    if (strcmp(mode, "exit_code") == 0) {
        snprintf(buffer, sizeof(buffer), "%d", output->exit_code);
        return kc_flow_strdup(buffer);
    }

    if (strcmp(mode, "file") == 0) {
        if (path == NULL) {
            snprintf(error, error_size, "Missing file binding path for %s.", output_id);
            return NULL;
        }
        return kc_flow_strdup(path);
    }

    snprintf(error, error_size, "Unsupported bind mode '%s' for %s.", mode, output_id);
    return NULL;
}

/**
 * Collects bound `output.*` values from one contract run.
 * @param model Parsed contract model.
 * @param output Captured runtime output.
 * @param values Output key/value store (output.<id>=...).
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on collection failure.
 */
int kc_flow_collect_contract_outputs(
    const kc_flow_model *model,
    const kc_flow_run_output *output,
    kc_flow_overrides *values,
    char *error,
    size_t error_size
) {
    size_t i;
    char key[128];

    for (i = 0; i < model->outputs.count; ++i) {
        const char *output_id;
        const char *mode;
        const char *path;
        char *materialized;
        char output_key[160];

        snprintf(key, sizeof(key), "output.%d.id", model->outputs.values[i]);
        output_id = kc_flow_model_get(model, key);
        if (output_id == NULL) {
            continue;
        }

        mode = NULL;
        path = NULL;
        if (kc_flow_find_output_binding(model, output_id, &mode, &path) != 0) {
            snprintf(
                error,
                error_size,
                "Missing bind.output entry for output id: %s",
                output_id
            );
            return -1;
        }

        materialized = kc_flow_materialize_bound_output(
            output_id,
            mode,
            path,
            output,
            error,
            error_size
        );
        if (materialized == NULL) {
            return -1;
        }

        snprintf(output_key, sizeof(output_key), "output.%s", output_id);
        if (kc_flow_overrides_add(values, output_key, materialized) != 0) {
            free(materialized);
            snprintf(error, error_size, "Unable to store contract output value.");
            return -1;
        }
        free(materialized);
    }

    return 0;
}

/**
 * Emits normalized bound output lines from one contract run.
 * @param model Parsed contract model.
 * @param output Captured runtime output.
 * @return int 0 on success; non-zero on collection failure.
 */
int kc_flow_print_contract_outputs(
    const kc_flow_model *model,
    const kc_flow_run_output *output
) {
    kc_flow_overrides values;
    size_t i;
    char error[256];

    kc_flow_overrides_init(&values);
    if (kc_flow_collect_contract_outputs(
            model,
            output,
            &values,
            error,
            sizeof(error)
        ) != 0) {
        kc_flow_overrides_free(&values);
        return -1;
    }

    for (i = 0; i < values.count; ++i) {
        printf("%s=%s\n", values.records[i].key, values.records[i].value);
    }

    kc_flow_overrides_free(&values);
    return 0;
}
