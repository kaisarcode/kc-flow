/**
 * build.c
 * Summary: CLI build/export backend entry points.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "build.h"

#include "chain.h"
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int kc_flow_append(char **buffer, size_t *capacity, size_t *length, const char *text) {
    size_t need;
    char *grown;

    need = *length + strlen(text) + 1;
    if (need > *capacity) {
        size_t next = *capacity;
        while (next < need) {
            next *= 2;
        }
        grown = realloc(*buffer, next);
        if (grown == NULL) {
            return -1;
        }
        *buffer = grown;
        *capacity = next;
    }

    memcpy(*buffer + *length, text, strlen(text));
    *length += strlen(text);
    (*buffer)[*length] = '\0';
    return 0;
}

static int kc_flow_appendf(char **buffer,
                           size_t *capacity,
                           size_t *length,
                           const char *fmt,
                           const char *a,
                           const char *b) {
    char tmp[512];
    int written;

    written = snprintf(tmp, sizeof(tmp), fmt, a, b);
    if (written < 0 || (size_t)written >= sizeof(tmp)) {
        return -1;
    }
    return kc_flow_append(buffer, capacity, length, tmp);
}

static int kc_flow_build_template_to_bash(const char *input,
                                          char **out,
                                          char *error,
                                          size_t error_size) {
    size_t cap = strlen(input) + 64;
    size_t len = 0;
    size_t i;
    char *buf = malloc(cap);

    if (buf == NULL) {
        snprintf(error, error_size, "Out of memory while rendering template.");
        return -1;
    }
    buf[0] = '\0';

    for (i = 0; input[i] != '\0'; ++i) {
        if (input[i] == '<') {
            size_t start = i + 1;
            size_t end = start;
            char ref[128];
            const char *prefix = NULL;

            while (input[end] != '\0' && input[end] != '>') {
                end++;
            }
            if (input[end] != '>') {
                free(buf);
                snprintf(error, error_size, "Unclosed placeholder in template.");
                return -1;
            }
            if (end <= start || (end - start) >= sizeof(ref)) {
                free(buf);
                snprintf(error, error_size, "Invalid placeholder in template.");
                return -1;
            }

            memcpy(ref, input + start, end - start);
            ref[end - start] = '\0';

            if (strncmp(ref, "input.", 6) == 0) {
                prefix = ref + 6;
            } else if (strncmp(ref, "param.", 6) == 0) {
                prefix = ref + 6;
            } else {
                free(buf);
                snprintf(error, error_size, "Unsupported placeholder: <%s>", ref);
                return -1;
            }

            if (kc_flow_appendf(&buf, &cap, &len, "${%s}", prefix, "") != 0) {
                free(buf);
                snprintf(error, error_size, "Out of memory while rendering placeholder.");
                return -1;
            }
            i = end;
            continue;
        }

        {
            char one[2];
            one[0] = input[i];
            one[1] = '\0';
            if (kc_flow_append(&buf, &cap, &len, one) != 0) {
                free(buf);
                snprintf(error, error_size, "Out of memory while rendering template.");
                return -1;
            }
        }
    }

    *out = buf;
    return 0;
}

static int kc_flow_build_contract_cli(const kc_flow_model *model,
                                      const char *path,
                                      char *error,
                                      size_t error_size) {
    size_t i;
    char *script = NULL;
    char cwd[1024];
    char workdir[1024];

    kc_flow_dirname(path, cwd, sizeof(cwd));
    if (kc_flow_build_path(workdir,
                           1024,
                           cwd,
                           model->runtime_workdir != NULL ? model->runtime_workdir : ".") != 0) {
        snprintf(error, error_size, "Unable to resolve runtime.workdir.");
        return -1;
    }

    if (kc_flow_build_template_to_bash(model->runtime_script, &script, error, error_size) != 0) {
        return -1;
    }

    printf("#!/usr/bin/env bash\n");
    printf("set -euo pipefail\n\n");
    printf("# Contract: %s\n", model->id != NULL ? model->id : "unknown");

    for (i = 0; i < model->params.count; ++i) {
        char key[128];
        const char *id;
        const char *def;
        snprintf(key, sizeof(key), "param.%d.id", model->params.values[i]);
        id = kc_flow_model_get(model, key);
        if (id == NULL) {
            continue;
        }
        def = kc_flow_lookup_indexed_id_value(model, &model->params, "param.", id, "default");
        printf("%s=${%s:-%s}\n", id, id, def != NULL ? def : "");
    }

    for (i = 0; i < model->inputs.count; ++i) {
        char key[128];
        const char *id;
        snprintf(key, sizeof(key), "input.%d.id", model->inputs.values[i]);
        id = kc_flow_model_get(model, key);
        if (id != NULL) {
            printf(": \"${%s:?missing input '%s'}\"\n", id, id);
        }
    }

    printf("\n(cd '%s' && ", workdir);
    if (model->runtime_exec != NULL && model->runtime_exec[0] != '\0') {
        char *exec_rendered = NULL;
        if (kc_flow_build_template_to_bash(model->runtime_exec, &exec_rendered, error, error_size) != 0) {
            free(script);
            return -1;
        }
        printf("%s ", exec_rendered);
        free(exec_rendered);
    }
    printf("%s)\n", script);

    free(script);
    return 0;
}

/**
 * Renders one flow into CLI form.
 * @param path Source flow path.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero while renderer is not implemented.
 */
int kc_flow_build_cli(
    const char *path,
    char *error,
    size_t error_size
) {
    kc_flow_model model;

    kc_flow_model_init(&model);
    if (!kc_flow_file_exists(path)) {
        snprintf(error, error_size, "contract or flow file not found: %s", path);
        return -1;
    }

    if (kc_flow_load_file(path, &model, error, error_size) != 0 ||
        kc_flow_validate_model(&model, error, error_size) != 0) {
        kc_flow_model_free(&model);
        return -1;
    }

    if (model.kind == KC_STDIO_FILE_CONTRACT) {
        int rc = kc_flow_build_contract_cli(&model, path, error, error_size);
        kc_flow_model_free(&model);
        return rc;
    }

    {
        int rc = kc_flow_build_flow_cli(&model, path, error, error_size);
        kc_flow_model_free(&model);
        return rc;
    }
}
