/**
 * load.c
 * Summary: Key-value flow loading and section indexing.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Duplicates one string.
 * @param text Source text.
 * @return char* Heap copy on success; NULL on failure.
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
 * Trims one line in place.
 * @param text Input text.
 * @return char* Trimmed text pointer.
 */
static char *kc_flow_trim(char *text) {
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    end = text + strlen(text);
    while (end > text &&
            (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return text;
}

/**
 * Reads one logical line from one file.
 * @param fp Source file.
 * @param line Output buffer pointer.
 * @param capacity Output buffer capacity.
 * @return int 1 when one line was read; otherwise 0.
 */
static int kc_flow_read_line(FILE *fp, char **line, size_t *capacity) {
    size_t length;
    int ch;

    if (*line == NULL || *capacity == 0) {
        *capacity = 256;
        *line = malloc(*capacity);
        if (*line == NULL) {
            return 0;
        }
    }
    length = 0;
    while ((ch = fgetc(fp)) != EOF) {
        if (length + 2 > *capacity) {
            char *grown;

            *capacity *= 2;
            grown = realloc(*line, *capacity);
            if (grown == NULL) {
                free(*line);
                *line = NULL;
                *capacity = 0;
                return 0;
            }
            *line = grown;
        }
        (*line)[length++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }
    if (length == 0 && ch == EOF) {
        return 0;
    }
    (*line)[length] = '\0';
    return 1;
}

/**
 * Collects one indexed section into an index set.
 * @param model Parsed model.
 * @param prefix Section prefix.
 * @param out Output set.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_collect_section(
    const kc_flow_model *model,
    const char *prefix,
    kc_flow_index_set *out
) {
    size_t i;
    size_t prefix_len;

    memset(out, 0, sizeof(*out));
    prefix_len = strlen(prefix);
    for (i = 0; i < model->record_count; ++i) {
        int index;
        const char *cursor;
        char *endptr;
        size_t j;

        if (strncmp(model->records[i].key, prefix, prefix_len) != 0) {
            continue;
        }
        cursor = model->records[i].key + prefix_len;
        if (*cursor < '0' || *cursor > '9') {
            continue;
        }
        index = (int)strtol(cursor, &endptr, 10);
        if (endptr == cursor || *endptr != '.') {
            continue;
        }
        for (j = 0; j < out->count; ++j) {
            if (out->values[j] == index) {
                break;
            }
        }
        if (j < out->count) {
            continue;
        }
        if (out->count >= KC_FLOW_MAX_INDEXES) {
            return -1;
        }
        out->values[out->count++] = index;
    }
    return 0;
}

/**
 * Resolves model kind and known sections.
 * @param model Output model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_finalize_model(
    kc_flow_model *model,
    char *error,
    size_t error_size
) {
    model->id = kc_flow_model_get(model, "contract.id");
    model->name = kc_flow_model_get(model, "contract.name");
    model->runtime_script = kc_flow_model_get(model, "runtime.script");
    model->runtime_exec = kc_flow_model_get(model, "runtime.exec");
    model->runtime_workdir = kc_flow_model_get(model, "runtime.workdir");
    model->runtime_stdin = kc_flow_model_get(model, "runtime.stdin");
    if (model->id != NULL) {
        model->kind = KC_FLOW_FILE_CONTRACT;
    } else {
        model->id = kc_flow_model_get(model, "flow.id");
        model->name = kc_flow_model_get(model, "flow.name");
        if (model->id != NULL) {
            model->kind = KC_FLOW_FILE_FLOW;
        }
    }
    if (model->kind == KC_FLOW_FILE_NONE) {
        snprintf(error, error_size, "Unable to determine file kind.");
        return -1;
    }
    if (kc_flow_collect_section(model, "param.", &model->params) != 0 ||
            kc_flow_collect_section(model, "input.", &model->inputs) != 0 ||
            kc_flow_collect_section(model, "output.", &model->outputs) != 0 ||
            kc_flow_collect_section(model, "bind.output.", &model->bind_output) != 0 ||
            kc_flow_collect_section(model, "node.", &model->nodes) != 0 ||
            kc_flow_collect_section(model, "link.", &model->links) != 0) {
        snprintf(error, error_size, "Too many indexed records.");
        return -1;
    }
    return 0;
}

/**
 * Loads one flow or contract file.
 * @param path Source file path.
 * @param model Output model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_load_file(
    const char *path,
    kc_flow_model *model,
    char *error,
    size_t error_size
) {
    FILE *fp;
    char *line;
    size_t capacity;

    kc_flow_model_init(model);
    fp = fopen(path, "r");
    if (fp == NULL) {
        snprintf(error, error_size, "Unable to open file: %s", path);
        return -1;
    }
    line = NULL;
    capacity = 0;
    while (kc_flow_read_line(fp, &line, &capacity)) {
        char *key;
        char *value;
        char *equals;

        key = kc_flow_trim(line);
        if (key[0] == '\0' || key[0] == '#') {
            continue;
        }
        equals = strchr(key, '=');
        if (equals == NULL || equals == key) {
            free(line);
            fclose(fp);
            kc_flow_model_free(model);
            snprintf(error, error_size, "Invalid record: %s", key);
            return -1;
        }
        *equals = '\0';
        value = kc_flow_trim(equals + 1);
        key = kc_flow_trim(key);
        if (model->record_count >= KC_FLOW_MAX_RECORDS) {
            free(line);
            fclose(fp);
            kc_flow_model_free(model);
            snprintf(error, error_size, "Too many records.");
            return -1;
        }
        model->records[model->record_count].key = kc_flow_strdup(key);
        model->records[model->record_count].value = kc_flow_strdup(value);
        if (model->records[model->record_count].key == NULL ||
                model->records[model->record_count].value == NULL) {
            free(line);
            fclose(fp);
            kc_flow_model_free(model);
            snprintf(error, error_size, "Out of memory while loading file.");
            return -1;
        }
        model->record_count++;
    }
    free(line);
    fclose(fp);
    return kc_flow_finalize_model(model, error, error_size);
}
