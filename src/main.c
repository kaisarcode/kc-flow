/**
 * kc-flow
 * Summary: Headless parser and execution entry for contract and flow files.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif
#include <unistd.h>

#define KC_STDIO_MAX_RECORDS 2048
#define KC_STDIO_MAX_INDEXES 256

typedef enum kc_flow_file_kind {
    KC_STDIO_FILE_NONE = 0,
    KC_STDIO_FILE_CONTRACT,
    KC_STDIO_FILE_FLOW
} kc_flow_file_kind;

typedef struct kc_flow_record {
    char *key;
    char *value;
} kc_flow_record;

typedef struct kc_flow_index_set {
    int values[KC_STDIO_MAX_INDEXES];
    size_t count;
} kc_flow_index_set;

typedef struct kc_flow_model {
    kc_flow_file_kind kind;
    kc_flow_record records[KC_STDIO_MAX_RECORDS];
    size_t record_count;
    const char *id;
    const char *name;
    const char *runtime_script;
    const char *runtime_exec;
    const char *runtime_workdir;
    const char *runtime_stdin;
    kc_flow_index_set params;
    kc_flow_index_set inputs;
    kc_flow_index_set outputs;
    kc_flow_index_set runtime_env;
    kc_flow_index_set bind_output;
    kc_flow_index_set nodes;
    kc_flow_index_set node_params;
    kc_flow_index_set links;
    kc_flow_index_set expose;
} kc_flow_model;

typedef struct kc_flow_run_output {
    char *stdout_text;
    char *stderr_text;
    int exit_code;
} kc_flow_run_output;

typedef struct kc_flow_overrides {
    kc_flow_record records[KC_STDIO_MAX_INDEXES];
    size_t count;
} kc_flow_overrides;

static int kc_flow_system_exit_code(int status) {
#if defined(_WIN32)
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

static void kc_flow_help(const char *bin) {
    printf("Commands:\n");
    printf("  schema            Print the current contract and flow direction\n");
    printf("  inspect <file>    Inspect one contract or flow file path\n");
    printf("  --run <file>      Resolve one contract or flow file path for execution\n");
    printf("  --help            Show help\n");
    printf("\n");
    printf("Options:\n");
    printf("  %s schema\n", bin);
    printf("  %s inspect /path/to/file.flow\n", bin);
    printf("  %s --run /path/to/file.flow [--set key=value ...]\n", bin);
}

static int kc_flow_fail(const char *bin, const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    kc_flow_help(bin);
    return 1;
}

static int kc_flow_file_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

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

static void kc_flow_model_init(kc_flow_model *model) {
    memset(model, 0, sizeof(*model));
}

static void kc_flow_model_free(kc_flow_model *model) {
    size_t i;

    for (i = 0; i < model->record_count; ++i) {
        free(model->records[i].key);
        free(model->records[i].value);
    }

    kc_flow_model_init(model);
}

static void kc_flow_run_output_free(kc_flow_run_output *output) {
    free(output->stdout_text);
    free(output->stderr_text);
    output->stdout_text = NULL;
    output->stderr_text = NULL;
    output->exit_code = 0;
}

static void kc_flow_overrides_init(kc_flow_overrides *overrides) {
    memset(overrides, 0, sizeof(*overrides));
}

static void kc_flow_overrides_free(kc_flow_overrides *overrides) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        free(overrides->records[i].key);
        free(overrides->records[i].value);
    }

    kc_flow_overrides_init(overrides);
}

static const char *kc_flow_overrides_get(const kc_flow_overrides *overrides, const char *key) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        if (strcmp(overrides->records[i].key, key) == 0) {
            return overrides->records[i].value;
        }
    }

    return NULL;
}

static int kc_flow_overrides_add(kc_flow_overrides *overrides, const char *key, const char *value) {
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

static const char *kc_flow_model_get(const kc_flow_model *model, const char *key) {
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

static int kc_flow_load_file(const char *path, kc_flow_model *model, char *error, size_t error_size) {
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

static int kc_flow_validate_model(const kc_flow_model *model, char *error, size_t error_size) {
    if (model->kind == KC_STDIO_FILE_CONTRACT) {
        return kc_flow_validate_contract(model, error, error_size);
    }

    if (model->kind == KC_STDIO_FILE_FLOW) {
        return kc_flow_validate_flow(model, error, error_size);
    }

    snprintf(error, error_size, "Unable to determine file kind.");
    return -1;
}

static int kc_flow_build_path(char *buffer, size_t size, const char *base, const char *value) {
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

static void kc_flow_dirname(const char *path, char *buffer, size_t size) {
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

static const char *kc_flow_lookup_indexed_id_value(
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
        return kc_flow_lookup_indexed_id_value(model, &model->params, "param.", field_id, "default");
    }

    if (strncmp(reference, "input.", 6) == 0) {
        return NULL;
    }

    if (strncmp(reference, "output.", 7) == 0) {
        return NULL;
    }

    return NULL;
}

static char *kc_flow_resolve_template(
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
                snprintf(error, error_size, "Invalid placeholder length in template.");
                return NULL;
            }

            memcpy(reference, template_text + start, ref_len);
            reference[ref_len] = '\0';

            value = kc_flow_resolve_reference_raw(model, overrides, reference);
            if (value == NULL) {
                free(result);
                snprintf(error, error_size, "Unable to resolve placeholder: <%.180s>", reference);
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

static char *kc_flow_shell_quote(const char *text) {
    size_t i;
    size_t extra;
    char *quoted;
    size_t length;

    if (text == NULL) {
        return kc_flow_strdup("''");
    }

    extra = 2;
    for (i = 0; text[i] != '\0'; ++i) {
        extra += text[i] == '\'' ? 4 : 1;
    }

    quoted = malloc(extra + 1);
    if (quoted == NULL) {
        return NULL;
    }

    length = 0;
    quoted[length++] = '\'';
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '\'') {
            memcpy(quoted + length, "'\\''", 4);
            length += 4;
        } else {
            quoted[length++] = text[i];
        }
    }
    quoted[length++] = '\'';
    quoted[length] = '\0';

    return quoted;
}

static char *kc_flow_read_file_text(const char *path) {
    FILE *fp;
    long size;
    char *buffer;
    size_t read_size;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)size, fp);
    buffer[read_size] = '\0';
    fclose(fp);
    return buffer;
}

static int kc_flow_write_temp_file(const char *content, char *path_buffer, size_t path_size) {
    int fd;
    FILE *fp;
    char template_path[] = "/tmp/kc-flow-stdin-XXXXXX";

    fd = mkstemp(template_path);
    if (fd < 0) {
        return -1;
    }

    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        unlink(template_path);
        return -1;
    }

    if (content != NULL && fputs(content, fp) == EOF) {
        fclose(fp);
        unlink(template_path);
        return -1;
    }

    if (fclose(fp) != 0) {
        unlink(template_path);
        return -1;
    }

    if (snprintf(path_buffer, path_size, "%s", template_path) >= (int)path_size) {
        unlink(template_path);
        return -1;
    }

    return 0;
}

static int kc_flow_find_output_binding(const kc_flow_model *model, const char *output_id, const char **mode, const char **path) {
    size_t i;
    char key[128];
    const char *current_id;

    for (i = 0; i < model->bind_output.count; ++i) {
        snprintf(key, sizeof(key), "bind.output.%d.id", model->bind_output.values[i]);
        current_id = kc_flow_model_get(model, key);
        if (current_id != NULL && strcmp(current_id, output_id) == 0) {
            snprintf(key, sizeof(key), "bind.output.%d.mode", model->bind_output.values[i]);
            *mode = kc_flow_model_get(model, key);
            snprintf(key, sizeof(key), "bind.output.%d.path", model->bind_output.values[i]);
            *path = kc_flow_model_get(model, key);
            return 0;
        }
    }

    return -1;
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

static int kc_flow_build_env_prefix(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    char **out_prefix,
    char *error,
    size_t error_size
) {
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

        snprintf(key_name, sizeof(key_name), "runtime.env.%d.key", model->runtime_env.values[i]);
        snprintf(value_name, sizeof(value_name), "runtime.env.%d.value", model->runtime_env.values[i]);

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

        env_value = kc_flow_resolve_template(model, overrides, env_value_template, error, error_size);
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

        written = snprintf(buffer + length, capacity - length, "%s=%s ", env_key, quoted_env_value);
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

static int kc_flow_run_contract(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *cfg_path,
    kc_flow_run_output *output,
    char *error,
    size_t error_size
) {
    char cfg_dir[PATH_MAX];
    char workdir[PATH_MAX];
    char stdout_path[] = "/tmp/kc-flow-stdout-XXXXXX";
    char stderr_path[] = "/tmp/kc-flow-stderr-XXXXXX";
    char stdin_path[PATH_MAX];
    int stdout_fd;
    int stderr_fd;
    char *resolved_script;
    char *resolved_exec;
    char *resolved_stdin;
    char *quoted_workdir;
    char *quoted_stdout;
    char *quoted_stderr;
    char *quoted_stdin;
    char *quoted_exec;
    char *command;
    char *env_buffer;
    size_t command_size;
    int status;

    kc_flow_dirname(cfg_path, cfg_dir, sizeof(cfg_dir));
    if (kc_flow_build_path(workdir, sizeof(workdir), cfg_dir, model->runtime_workdir != NULL ? model->runtime_workdir : ".") != 0) {
        snprintf(error, error_size, "Unable to resolve workdir.");
        return -1;
    }

    resolved_script = kc_flow_resolve_template(model, overrides, model->runtime_script, error, error_size);
    if (resolved_script == NULL) {
        return -1;
    }

    resolved_exec = kc_flow_resolve_template(model, overrides, model->runtime_exec, error, error_size);
    if (model->runtime_exec != NULL && resolved_exec == NULL) {
        free(resolved_script);
        return -1;
    }

    resolved_stdin = kc_flow_resolve_template(model, overrides, model->runtime_stdin, error, error_size);
    if (model->runtime_stdin != NULL && resolved_stdin == NULL) {
        free(resolved_script);
        free(resolved_exec);
        return -1;
    }

    stdout_fd = mkstemp(stdout_path);
    stderr_fd = mkstemp(stderr_path);
    if (stdout_fd < 0 || stderr_fd < 0) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        if (stdout_fd >= 0) {
            close(stdout_fd);
            unlink(stdout_path);
        }
        if (stderr_fd >= 0) {
            close(stderr_fd);
            unlink(stderr_path);
        }
        snprintf(error, error_size, "Unable to create temporary output files.");
        return -1;
    }
    close(stdout_fd);
    close(stderr_fd);

    stdin_path[0] = '\0';
    if (resolved_stdin != NULL) {
        if (kc_flow_write_temp_file(resolved_stdin, stdin_path, sizeof(stdin_path)) != 0) {
            free(resolved_script);
            free(resolved_exec);
            free(resolved_stdin);
            unlink(stdout_path);
            unlink(stderr_path);
            snprintf(error, error_size, "Unable to create stdin file.");
            return -1;
        }
    }

    quoted_workdir = kc_flow_shell_quote(workdir);
    quoted_stdout = kc_flow_shell_quote(stdout_path);
    quoted_stderr = kc_flow_shell_quote(stderr_path);
    quoted_stdin = resolved_stdin != NULL ? kc_flow_shell_quote(stdin_path) : NULL;
    quoted_exec = resolved_exec != NULL ? kc_flow_shell_quote(resolved_exec) : NULL;
    env_buffer = NULL;

    if (quoted_workdir == NULL || quoted_stdout == NULL || quoted_stderr == NULL ||
        (resolved_stdin != NULL && quoted_stdin == NULL) ||
        (resolved_exec != NULL && quoted_exec == NULL)) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        snprintf(error, error_size, "Out of memory while preparing execution.");
        return -1;
    }

    if (kc_flow_build_env_prefix(model, overrides, &env_buffer, error, error_size) != 0) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        unlink(stdout_path);
        unlink(stderr_path);
        return -1;
    }

    command_size = strlen("cd ") + strlen(quoted_workdir) + strlen(" && ") +
                   strlen(env_buffer) + strlen(resolved_script) +
                   strlen(" > ") + strlen(quoted_stdout) +
                   strlen(" 2> ") + strlen(quoted_stderr) + 32;

    if (quoted_exec != NULL) {
        command_size += strlen(quoted_exec) + 1;
    }

    if (quoted_stdin != NULL) {
        command_size += strlen(" < ") + strlen(quoted_stdin);
    }

    command = malloc(command_size);
    if (command == NULL) {
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        snprintf(error, error_size, "Out of memory while building command.");
        return -1;
    }

    snprintf(
        command,
        command_size,
        "cd %s && %s%s%s%s%s > %s 2> %s",
        quoted_workdir,
        env_buffer,
        quoted_exec != NULL ? quoted_exec : "",
        quoted_exec != NULL ? " " : "",
        resolved_script,
        quoted_stdin != NULL ? "" : "",
        quoted_stdout,
        quoted_stderr
    );

    if (quoted_stdin != NULL) {
        size_t current_len = strlen(command);
        snprintf(command + current_len, command_size - current_len, " < %s", quoted_stdin);
    }

    status = system(command);
    if (status == -1) {
        snprintf(error, error_size, "Unable to launch process.");
        free(command);
        free(resolved_script);
        free(resolved_exec);
        free(resolved_stdin);
        free(quoted_workdir);
        free(quoted_stdout);
        free(quoted_stderr);
        free(quoted_stdin);
        free(quoted_exec);
        free(env_buffer);
        unlink(stdout_path);
        unlink(stderr_path);
        if (stdin_path[0] != '\0') {
            unlink(stdin_path);
        }
        return -1;
    }

    output->stdout_text = kc_flow_read_file_text(stdout_path);
    output->stderr_text = kc_flow_read_file_text(stderr_path);
    output->exit_code = kc_flow_system_exit_code(status);

    unlink(stdout_path);
    unlink(stderr_path);
    if (stdin_path[0] != '\0') {
        unlink(stdin_path);
    }

    free(command);
    free(resolved_script);
    free(resolved_exec);
    free(resolved_stdin);
    free(quoted_workdir);
    free(quoted_stdout);
    free(quoted_stderr);
    free(quoted_stdin);
    free(quoted_exec);
    free(env_buffer);

    if (output->stdout_text == NULL || output->stderr_text == NULL) {
        snprintf(error, error_size, "Unable to collect process output.");
        return -1;
    }

    return 0;
}

static int kc_flow_print_contract_outputs(const kc_flow_model *model, const kc_flow_run_output *output) {
    size_t i;
    char key[128];

    for (i = 0; i < model->outputs.count; ++i) {
        const char *output_id;
        const char *mode;
        const char *path;

        snprintf(key, sizeof(key), "output.%d.id", model->outputs.values[i]);
        output_id = kc_flow_model_get(model, key);
        if (output_id == NULL) {
            continue;
        }

        mode = NULL;
        path = NULL;
        if (kc_flow_find_output_binding(model, output_id, &mode, &path) != 0 || mode == NULL) {
            continue;
        }

        if (strcmp(mode, "stdout") == 0) {
            printf("output.%s=%s\n", output_id, output->stdout_text);
        } else if (strcmp(mode, "stderr") == 0) {
            printf("output.%s=%s\n", output_id, output->stderr_text);
        } else if (strcmp(mode, "exit_code") == 0) {
            printf("output.%s=%d\n", output_id, output->exit_code);
        } else if (strcmp(mode, "file") == 0 && path != NULL) {
            printf("output.%s=%s\n", output_id, path);
        }
    }

    return 0;
}

static int kc_flow_schema(void) {
    printf("contract: executable unit with params, inputs, outputs, execution reference, output bindings\n");
    printf("flow: composed contract graph with nodes, links, and exposed interface\n");
    printf("view: paired visual metadata stored in .gui.cfg files\n");
    printf("engine: headless resolver and executor over contracts and flows\n");
    printf("gui: compatible visual clients over the same model\n");
    return 0;
}

static int kc_flow_parse_run_overrides(
    int argc,
    char **argv,
    int start_index,
    kc_flow_overrides *overrides,
    char *error,
    size_t error_size
) {
    int i;

    for (i = start_index; i < argc; ++i) {
        const char *assignment;
        const char *equals;
        char key[256];
        size_t key_len;

        if (strcmp(argv[i], "--set") != 0) {
            snprintf(error, error_size, "Unknown run argument: %s", argv[i]);
            return -1;
        }

        if (i + 1 >= argc) {
            snprintf(error, error_size, "run --set requires key=value.");
            return -1;
        }

        assignment = argv[++i];
        equals = strchr(assignment, '=');
        if (equals == NULL || equals == assignment) {
            snprintf(error, error_size, "Invalid --set assignment: %s", assignment);
            return -1;
        }

        key_len = (size_t)(equals - assignment);
        if (key_len >= sizeof(key)) {
            snprintf(error, error_size, "run --set key too long.");
            return -1;
        }

        memcpy(key, assignment, key_len);
        key[key_len] = '\0';

        if (kc_flow_overrides_add(overrides, key, equals + 1) != 0) {
            snprintf(error, error_size, "Unable to register run override.");
            return -1;
        }
    }

    return 0;
}

static int kc_flow_inspect(const char *path) {
    kc_flow_model model;
    char error[256];

    if (!kc_flow_file_exists(path)) {
        fprintf(stderr, "Error: contract or flow file not found: %s\n", path);
        return 1;
    }

    kc_flow_model_init(&model);

    if (kc_flow_load_file(path, &model, error, sizeof(error)) != 0 ||
        kc_flow_validate_model(&model, error, sizeof(error)) != 0) {
        fprintf(stderr, "Error: %s\n", error);
        kc_flow_model_free(&model);
        return 1;
    }

    printf("inspect ok\n");
    printf("path=%s\n", path);
    printf("kind=%s\n", model.kind == KC_STDIO_FILE_CONTRACT ? "contract" : "flow");
    printf("id=%s\n", model.id);
    printf("name=%s\n", model.name);
    printf("params=%zu\n", model.params.count);
    printf("inputs=%zu\n", model.inputs.count);
    printf("outputs=%zu\n", model.outputs.count);

    if (model.kind == KC_STDIO_FILE_CONTRACT) {
        printf("runtime.script=%s\n", model.runtime_script);
        if (model.runtime_exec != NULL) {
            printf("runtime.exec=%s\n", model.runtime_exec);
        }
        if (model.runtime_stdin != NULL) {
            printf("runtime.stdin=%s\n", model.runtime_stdin);
        }
        printf("runtime.env=%zu\n", model.runtime_env.count);
        printf("bind.output=%zu\n", model.bind_output.count);
    } else {
        printf("nodes=%zu\n", model.nodes.count);
        printf("node.param=%zu\n", model.node_params.count);
        printf("links=%zu\n", model.links.count);
        printf("expose=%zu\n", model.expose.count);
    }

    kc_flow_model_free(&model);
    return 0;
}

static int kc_flow_run(const char *path, const kc_flow_overrides *overrides) {
    kc_flow_model model;
    kc_flow_run_output output;
    char error[256];

    if (!kc_flow_file_exists(path)) {
        fprintf(stderr, "Error: contract or flow file not found: %s\n", path);
        return 1;
    }

    kc_flow_model_init(&model);
    memset(&output, 0, sizeof(output));

    if (kc_flow_load_file(path, &model, error, sizeof(error)) != 0 ||
        kc_flow_validate_model(&model, error, sizeof(error)) != 0) {
        fprintf(stderr, "Error: %s\n", error);
        kc_flow_model_free(&model);
        return 1;
    }

    if (model.kind != KC_STDIO_FILE_CONTRACT) {
        fprintf(stderr, "Error: Flow execution is not implemented yet.\n");
        kc_flow_model_free(&model);
        return 1;
    }

    if (kc_flow_run_contract(&model, overrides, path, &output, error, sizeof(error)) != 0) {
        fprintf(stderr, "Error: %s\n", error);
        kc_flow_run_output_free(&output);
        kc_flow_model_free(&model);
        return 1;
    }

    printf("run ok\n");
    printf("path=%s\n", path);
    printf("kind=contract\n");
    printf("id=%s\n", model.id);
    printf("engine=headless\n");
    printf("exit_code=%d\n", output.exit_code);
    kc_flow_print_contract_outputs(&model, &output);

    kc_flow_run_output_free(&output);
    kc_flow_model_free(&model);
    return output.exit_code == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    kc_flow_overrides overrides;
    char run_error[256];

    kc_flow_overrides_init(&overrides);

    if (argc <= 1) {
        kc_flow_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0) {
        kc_flow_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "schema") == 0) {
        if (argc != 2) {
            return kc_flow_fail(argv[0], "schema does not accept extra arguments.");
        }
        return kc_flow_schema();
    }

    if (strcmp(argv[1], "inspect") == 0) {
        if (argc != 3) {
            return kc_flow_fail(argv[0], "inspect requires exactly one file path.");
        }
        return kc_flow_inspect(argv[2]);
    }

    if (strcmp(argv[1], "--run") == 0) {
        if (argc < 3) {
            return kc_flow_fail(argv[0], "--run requires one file path.");
        }
        if (kc_flow_parse_run_overrides(argc, argv, 3, &overrides, run_error, sizeof(run_error)) != 0) {
            kc_flow_overrides_free(&overrides);
            return kc_flow_fail(argv[0], run_error);
        }
        {
            int rc = kc_flow_run(argv[2], &overrides);
            kc_flow_overrides_free(&overrides);
            return rc;
        }
    }

    return kc_flow_fail(argv[0], "Unknown argument.");
}
