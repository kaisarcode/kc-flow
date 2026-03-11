/**
 * load_util.c
 * Summary: Internal parsing helpers for flow and contract file loading.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#define _POSIX_C_SOURCE 200809L

#include "load_util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/**
 * Trims leading and trailing whitespace in one mutable string.
 * @param text Mutable text buffer.
 * @return char* Trimmed view into the same buffer.
 */
char *kc_flow_trim(char *text) {
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

/**
 * Duplicates one C string.
 * @param text Source text.
 * @return char* Heap copy on success; NULL on error.
 */
char *kc_flow_load_strdup(const char *text) {
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
 * Adds one unique numeric index to a set.
 * @param set Target index set.
 * @param value Index value.
 * @return int 0 on success; non-zero on overflow.
 */
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

/**
 * Collects one indexed prefix entry from a raw key.
 * @param key Raw record key.
 * @param prefix Section prefix.
 * @param set Target index set.
 * @return int 0 on success; non-zero on invalid index overflow.
 */
static int kc_flow_collect_prefix_index(
    const char *key,
    const char *prefix,
    kc_flow_index_set *set
) {
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

/**
 * Collects indexed sections and determines file kind metadata.
 * @param model Parsed model.
 * @return int 0 on success; non-zero on invalid indexed data.
 */
int kc_flow_model_collect(kc_flow_model *model) {
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

/**
 * Portable getline wrapper for POSIX and Windows builds.
 * @param lineptr Output line buffer pointer.
 * @param n Buffer capacity.
 * @param stream Source stream.
 * @return ssize_t Line length or -1 on EOF/error.
 */
ssize_t kc_flow_getline_portable(char **lineptr, size_t *n, FILE *stream) {
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
