/**
 * template.c
 * Summary: Placeholder resolution and runtime env prefix assembly.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "priv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *kc_flow_resolve_reference_raw(const kc_flow_model *model,
                                                  const kc_flow_overrides *overrides,
                                                  const char *reference) {
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

char *kc_flow_resolve_template(const kc_flow_model *model,
                               const kc_flow_overrides *overrides,
                               const char *template_text,
                               char *error,
                               size_t error_size) {
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

static int kc_flow_is_valid_env_key(const char *key) {
    size_t i;

    if (key == NULL || key[0] == '\0') {
        return 0;
    }

    if (!(isalpha((unsigned char)key[0]) || key[0] == '_')) {
        return 0;
    }

    for (i = 1; key[i] != '\0'; ++i) {
        if (!(isalnum((unsigned char)key[i]) || key[i] == '_')) {
            return 0;
        }
    }

    return 1;
}

int kc_flow_build_env_prefix(const kc_flow_model *model,
                             const kc_flow_overrides *overrides,
                             char **out_prefix,
                             char *error,
                             size_t error_size) {
    size_t i;
    char *buffer;
    size_t length;
    size_t capacity;

    buffer = calloc(1, 1);
    if (buffer == NULL) {
        snprintf(error, error_size, "Out of memory while building env prefix.");
        return -1;
    }

    length = 0;
    capacity = 1;

    for (i = 0; i < model->runtime_env.count; ++i) {
        char key_name[128];
        char value_name[128];
        const char *env_key;
        const char *env_value_template;
        char *env_value;
        char *quoted_env_value;
        size_t needed;
        int written;

        snprintf(key_name, sizeof(key_name), "runtime.env.%d.key",
                 model->runtime_env.values[i]);
        snprintf(value_name, sizeof(value_name), "runtime.env.%d.value",
                 model->runtime_env.values[i]);

        env_key = kc_flow_model_get(model, key_name);
        env_value_template = kc_flow_model_get(model, value_name);

        if (env_key == NULL || env_value_template == NULL) {
            continue;
        }

        if (!kc_flow_is_valid_env_key(env_key)) {
            free(buffer);
            snprintf(error, error_size, "Invalid env key: %s", env_key);
            return -1;
        }

        env_value = kc_flow_resolve_template(
            model, overrides, env_value_template, error, error_size
        );
        if (env_value == NULL) {
            free(buffer);
            return -1;
        }

        quoted_env_value = kc_flow_shell_quote(env_value);
        free(env_value);
        if (quoted_env_value == NULL) {
            free(buffer);
            snprintf(error, error_size, "Out of memory while quoting env value.");
            return -1;
        }

        needed = length + strlen(env_key) + strlen(quoted_env_value) + 3;
        if (needed + 1 > capacity) {
            char *grown;
            size_t new_capacity = capacity * 2;
            while (new_capacity < needed + 1) {
                new_capacity *= 2;
            }
            grown = realloc(buffer, new_capacity);
            if (grown == NULL) {
                free(quoted_env_value);
                free(buffer);
                snprintf(error, error_size, "Out of memory while growing env prefix.");
                return -1;
            }
            buffer = grown;
            capacity = new_capacity;
        }

        written = snprintf(buffer + length,
                           capacity - length,
                           "%s=%s ",
                           env_key,
                           quoted_env_value);
        free(quoted_env_value);
        if (written < 0 || (size_t)written >= capacity - length) {
            free(buffer);
            snprintf(error, error_size, "Unable to compose env prefix.");
            return -1;
        }
        length += (size_t)written;
    }

    *out_prefix = buffer;
    return 0;
}
