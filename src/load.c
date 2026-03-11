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
#include "load_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Loads and parses one contract/flow file.
 * @param path Source file path.
 * @param model Output model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on parse/load errors.
 */
int kc_flow_load_file(
    const char *path,
    kc_flow_model *model,
    char *error,
    size_t error_size
) {
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

        model->records[model->record_count].key = kc_flow_load_strdup(key);
        model->records[model->record_count].value = kc_flow_load_strdup(value);
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
