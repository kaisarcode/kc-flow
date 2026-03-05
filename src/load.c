/**
 * load.c
 * Summary: File parsing and indexed section collection.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "model.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static char *kc_flow_trim(char *text) {
    char *end;
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0') {
        return text;
    }
    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return text;
}

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

static int kc_flow_index_set_add(kc_flow_index_set *set, int value) {
    size_t i;

    for (i = 0; i < set->count; ++i) {
        if (set->values[i] == value) {
            return 0;
        }
    }

    if (set->count >= KC_STDIO_MAX_INDEXES) {
        return -1;
    }

    set->values[set->count++] = value;
    return 0;
}

static int kc_flow_collect_prefix_index(const char *key,
                                        const char *prefix,
                                        kc_flow_index_set *set) {
    const char *cursor;
    char *endptr;
    long value;

    if (strncmp(key, prefix, strlen(prefix)) != 0) {
        return 0;
    }

    cursor = key + strlen(prefix);
    if (!isdigit((unsigned char)*cursor)) {
        return 0;
    }

    value = strtol(cursor, &endptr, 10);
    if (endptr == cursor || *endptr != '.') {
        return 0;
    }

    if (value <= 0 || value > 2147483647L) {
        return -1;
    }

    return kc_flow_index_set_add(set, (int)value);
}

static int kc_flow_model_collect(kc_flow_model *model) {
    size_t i;
    int rc;

    model->id = kc_flow_model_get(model, "contract.id");
    model->name = kc_flow_model_get(model, "contract.name");
    model->runtime_script = kc_flow_model_get(model, "runtime.script");
    model->runtime_exec = kc_flow_model_get(model, "runtime.exec");
    model->runtime_workdir = kc_flow_model_get(model, "runtime.workdir");
    model->runtime_stdin = kc_flow_model_get(model, "runtime.stdin");

    if (model->id != NULL || model->name != NULL) {
        model->kind = KC_STDIO_FILE_CONTRACT;
    }

    if (kc_flow_model_get(model, "flow.id") != NULL ||
        kc_flow_model_get(model, "flow.name") != NULL) {
        model->kind = KC_STDIO_FILE_FLOW;
        model->id = kc_flow_model_get(model, "flow.id");
        model->name = kc_flow_model_get(model, "flow.name");
    }

    for (i = 0; i < model->record_count; ++i) {
        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "param.", &model->params
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "input.", &model->inputs
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "output.", &model->outputs
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "runtime.env.", &model->runtime_env
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "bind.output.", &model->bind_output
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "node.", &model->nodes
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "node.param.", &model->node_params
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "link.", &model->links
        );
        if (rc != 0) return -1;

        rc = kc_flow_collect_prefix_index(
            model->records[i].key, "expose.", &model->expose
        );
        if (rc != 0) return -1;
    }

    return 0;
}

static ssize_t kc_flow_getline_portable(char **lineptr,
                                        size_t *n,
                                        FILE *stream) {
#if defined(_WIN32)
    size_t pos = 0;
    int ch;
    char *buffer;

    if (lineptr == NULL || n == NULL || stream == NULL) {
        return -1;
    }

    buffer = *lineptr;
    if (buffer == NULL || *n == 0) {
        *n = 256;
        buffer = malloc(*n);
        if (buffer == NULL) return -1;
        *lineptr = buffer;
    }

    while ((ch = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            size_t next = (*n < 1024) ? 1024 : (*n * 2);
            char *grown = realloc(buffer, next);
            if (grown == NULL) return -1;
            buffer = grown;
            *lineptr = buffer;
            *n = next;
        }
        buffer[pos++] = (char)ch;
        if (ch == '\n') break;
    }

    if (pos == 0 && ch == EOF) return -1;
    buffer[pos] = '\0';
    return (ssize_t)pos;
#else
    return getline(lineptr, n, stream);
#endif
}

/**
 * Loads and parses one contract/flow file.
 * @param path Source file path.
 * @param model Output model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on parse/load errors.
 */
int kc_flow_load_file(const char *path,
                      kc_flow_model *model,
                      char *error,
                      size_t error_size) {
    FILE *fp;
    char *line;
    size_t line_capacity;
    ssize_t line_length;
    size_t line_number;

    fp = fopen(path, "r");
    if (fp == NULL) {
        snprintf(error, error_size, "Unable to open file: %s", path);
        return -1;
    }

    line = NULL;
    line_capacity = 0;
    line_number = 0;

    while ((line_length = kc_flow_getline_portable(&line, &line_capacity, fp)) != -1) {
        char *cursor;
        char *equals;
        char *key;
        char *value;

        line_number++;
        (void)line_length;

        cursor = kc_flow_trim(line);
        if (*cursor == '\0' || *cursor == '#') {
            continue;
        }

        equals = strchr(cursor, '=');
        if (equals == NULL) {
            snprintf(error, error_size, "Invalid record at line %zu", line_number);
            free(line);
            fclose(fp);
            return -1;
        }

        *equals = '\0';
        key = kc_flow_trim(cursor);
        value = kc_flow_trim(equals + 1);

        if (*key == '\0') {
            snprintf(error, error_size, "Empty key at line %zu", line_number);
            free(line);
            fclose(fp);
            return -1;
        }

        if (model->record_count >= KC_STDIO_MAX_RECORDS) {
            snprintf(error, error_size, "Too many records in file: %s", path);
            free(line);
            fclose(fp);
            return -1;
        }

        model->records[model->record_count].key = kc_flow_strdup(key);
        model->records[model->record_count].value = kc_flow_strdup(value);
        if (model->records[model->record_count].key == NULL ||
            model->records[model->record_count].value == NULL) {
            snprintf(error, error_size, "Out of memory while reading file: %s", path);
            free(line);
            fclose(fp);
            return -1;
        }

        model->record_count++;
    }

    free(line);
    fclose(fp);

    if (kc_flow_model_collect(model) != 0) {
        snprintf(error, error_size, "Unable to collect indexed sections: %s", path);
        return -1;
    }

    return 0;
}
