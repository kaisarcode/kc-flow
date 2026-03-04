#define _POSIX_C_SOURCE 200809L

#include "model.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

int kc_flow_file_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

void kc_flow_model_init(kc_flow_model *model) {
    memset(model, 0, sizeof(*model));
}

void kc_flow_model_free(kc_flow_model *model) {
    size_t i;

    for (i = 0; i < model->record_count; ++i) {
        free(model->records[i].key);
        free(model->records[i].value);
    }

    kc_flow_model_init(model);
}

void kc_flow_overrides_init(kc_flow_overrides *overrides) {
    memset(overrides, 0, sizeof(*overrides));
}

void kc_flow_overrides_free(kc_flow_overrides *overrides) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        free(overrides->records[i].key);
        free(overrides->records[i].value);
    }

    kc_flow_overrides_init(overrides);
}

const char *kc_flow_overrides_get(const kc_flow_overrides *overrides, const char *key) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        if (strcmp(overrides->records[i].key, key) == 0) {
            return overrides->records[i].value;
        }
    }

    return NULL;
}

int kc_flow_overrides_add(kc_flow_overrides *overrides, const char *key, const char *value) {
    const char *current;

    current = kc_flow_overrides_get(overrides, key);
    if (current != NULL) {
        size_t i;
        for (i = 0; i < overrides->count; ++i) {
            if (strcmp(overrides->records[i].key, key) == 0) {
                char *next_value = kc_flow_strdup(value);
                if (next_value == NULL) {
                    return -1;
                }
                free(overrides->records[i].value);
                overrides->records[i].value = next_value;
                return 0;
            }
        }
    }

    if (overrides->count >= KC_STDIO_MAX_INDEXES) {
        return -1;
    }

    overrides->records[overrides->count].key = kc_flow_strdup(key);
    overrides->records[overrides->count].value = kc_flow_strdup(value);
    if (overrides->records[overrides->count].key == NULL ||
        overrides->records[overrides->count].value == NULL) {
        free(overrides->records[overrides->count].key);
        free(overrides->records[overrides->count].value);
        overrides->records[overrides->count].key = NULL;
        overrides->records[overrides->count].value = NULL;
        return -1;
    }

    overrides->count++;
    return 0;
}

const char *kc_flow_model_get(const kc_flow_model *model, const char *key) {
    size_t i;

    for (i = 0; i < model->record_count; ++i) {
        if (strcmp(model->records[i].key, key) == 0) {
            return model->records[i].value;
        }
    }

    return NULL;
}

static int kc_flow_collect_prefix_index(const char *key, const char *prefix, kc_flow_index_set *set) {
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

    if (kc_flow_model_get(model, "flow.id") != NULL || kc_flow_model_get(model, "flow.name") != NULL) {
        model->kind = KC_STDIO_FILE_FLOW;
        model->id = kc_flow_model_get(model, "flow.id");
        model->name = kc_flow_model_get(model, "flow.name");
    }

    for (i = 0; i < model->record_count; ++i) {
        rc = kc_flow_collect_prefix_index(model->records[i].key, "param.", &model->params);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "input.", &model->inputs);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "output.", &model->outputs);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "runtime.env.", &model->runtime_env);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "bind.output.", &model->bind_output);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "node.", &model->nodes);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "node.param.", &model->node_params);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "link.", &model->links);
        if (rc != 0) {
            return -1;
        }

        rc = kc_flow_collect_prefix_index(model->records[i].key, "expose.", &model->expose);
        if (rc != 0) {
            return -1;
        }
    }

    return 0;
}

static ssize_t kc_flow_getline(char **lineptr, size_t *n, FILE *stream) {
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
        if (buffer == NULL) {
            return -1;
        }
        *lineptr = buffer;
    }

    while ((ch = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            size_t next = (*n < 1024) ? 1024 : (*n * 2);
            char *grown = realloc(buffer, next);
            if (grown == NULL) {
                return -1;
            }
            buffer = grown;
            *lineptr = buffer;
            *n = next;
        }

        buffer[pos++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (pos == 0 && ch == EOF) {
        return -1;
    }

    buffer[pos] = '\0';
    return (ssize_t)pos;
#else
    return getline(lineptr, n, stream);
#endif
}

int kc_flow_load_file(const char *path, kc_flow_model *model, char *error, size_t error_size) {
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

    while ((line_length = kc_flow_getline(&line, &line_capacity, fp)) != -1) {
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
        snprintf(error, error_size, "Unable to collect indexed sections from file: %s", path);
        return -1;
    }

    return 0;
}

static int kc_flow_validate_contract(const kc_flow_model *model, char *error, size_t error_size) {
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

static int kc_flow_validate_flow(const kc_flow_model *model, char *error, size_t error_size) {
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

int kc_flow_validate_model(const kc_flow_model *model, char *error, size_t error_size) {
    if (model->kind == KC_STDIO_FILE_CONTRACT) {
        return kc_flow_validate_contract(model, error, error_size);
    }

    if (model->kind == KC_STDIO_FILE_FLOW) {
        return kc_flow_validate_flow(model, error, error_size);
    }

    snprintf(error, error_size, "Unable to determine file kind.");
    return -1;
}

int kc_flow_build_path(char *buffer, size_t size, const char *base, const char *value) {
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

const char *kc_flow_lookup_indexed_id_value(
    const kc_flow_model *model,
    const kc_flow_index_set *set,
    const char *prefix,
    const char *id,
    const char *field
) {
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
