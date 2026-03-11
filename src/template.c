/**
 * template.c
 * Summary: Placeholder resolution and runtime env prefix assembly.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "priv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Resolves one raw placeholder reference.
 * @param model Source model.
 * @param overrides Runtime overrides.
 * @param reference Placeholder reference text.
 * @return const char* Resolved value or NULL if unavailable.
 */
static const char *kc_flow_resolve_reference_raw(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *reference
) {
    const char *field_id;
    const char *override_value;

    if (overrides != NULL) {
        override_value = kc_flow_overrides_get(overrides, reference);
        if (override_value != NULL) {
            return override_value;
        }
    }

    if (strncmp(reference, "param.", 6) == 0) {
        field_id = reference + 6;
        return kc_flow_lookup_indexed_id_value(
            model, &model->params, "param.", field_id, "default"
        );
    }

    if (strncmp(reference, "input.", 6) == 0) {
        return NULL;
    }

    if (strncmp(reference, "output.", 7) == 0) {
        return NULL;
    }

    return NULL;
}

/**
 * Expands placeholders inside one template string.
 * @param model Source model.
 * @param overrides Runtime overrides.
 * @param template_text Template text.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return char* Newly allocated resolved string or NULL on failure.
 */
char *kc_flow_resolve_template(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *template_text,
    char *error,
    size_t error_size
) {
    size_t capacity;
    size_t length;
    size_t i;
    char *result;

    if (template_text == NULL) {
        return NULL;
    }

    capacity = strlen(template_text) + 1;
    result = malloc(capacity);
    if (result == NULL) {
        snprintf(error, error_size, "Out of memory while resolving template.");
        return NULL;
    }

    length = 0;
    for (i = 0; template_text[i] != '\0'; ++i) {
        if (template_text[i] == '<') {
            size_t start;
            size_t end;
            char reference[256];
            size_t ref_len;
            const char *value;
            size_t value_len;

            start = i + 1;
            end = start;
            while (template_text[end] != '\0' && template_text[end] != '>') {
                end++;
            }

            if (template_text[end] != '>') {
                free(result);
                snprintf(error, error_size, "Unclosed placeholder in template.");
                return NULL;
            }

            ref_len = end - start;
            if (ref_len == 0 || ref_len >= sizeof(reference)) {
                free(result);
                snprintf(error, error_size, "Invalid placeholder length.");
                return NULL;
            }

            memcpy(reference, template_text + start, ref_len);
            reference[ref_len] = '\0';

            value = kc_flow_resolve_reference_raw(model, overrides, reference);
            if (value == NULL) {
                free(result);
                snprintf(error, error_size, "Unable to resolve: <%.180s>", reference);
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

        result[length++] = template_text[i];
    }

    result[length] = '\0';
    return result;
}
