/**
 * load.c
 * Summary: Flat flow file parsing into the in-memory branch model.
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
 * @return char* Heap copy or NULL.
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
 * @param text Mutable text buffer.
 * @return char* Trimmed text.
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
 * Reads one logical line.
 * @param fp Open input file.
 * @param line Output line buffer.
 * @param capacity Output line capacity.
 * @return int 1 on success; otherwise 0.
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
 * Looks up one mutable node by local reference.
 * @param model Parsed model.
 * @param ref Node reference.
 * @return kc_flow_node* Node or NULL.
 */
static kc_flow_node *kc_flow_model_find_node_mut(kc_flow_model *model, const char *ref) {
    size_t i;

    for (i = 0; i < model->node_count; ++i) {
        if (strcmp(model->nodes[i].ref, ref) == 0) {
            return &model->nodes[i];
        }
    }
    return NULL;
}

/**
 * Resolves or creates one node reference.
 * @param model Parsed model.
 * @param ref Node reference.
 * @return kc_flow_node* Node or NULL.
 */
static kc_flow_node *kc_flow_model_ensure_node(kc_flow_model *model, const char *ref) {
    kc_flow_node *node;

    node = kc_flow_model_find_node_mut(model, ref);
    if (node != NULL) {
        return node;
    }
    if (model->node_count >= KC_FLOW_MAX_NODES) {
        return NULL;
    }
    node = &model->nodes[model->node_count++];
    memset(node, 0, sizeof(*node));
    node->ref = kc_flow_strdup(ref);
    if (node->ref == NULL) {
        return NULL;
    }
    return node;
}

/**
 * Parses one node-scoped field.
 * @param model Parsed model.
 * @param key Full source key.
 * @param value Source value.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_parse_node_field(
    kc_flow_model *model,
    const char *key,
    const char *value,
    char *error,
    size_t error_size
) {
    const char *cursor;
    const char *field;
    size_t ref_len;
    char ref[128];
    kc_flow_node *node;

    cursor = key + 5;
    field = strchr(cursor, '.');
    if (field == NULL || field == cursor) {
        snprintf(error, error_size, "Invalid node record: %s", key);
        return -1;
    }
    ref_len = (size_t)(field - cursor);
    if (ref_len >= sizeof(ref)) {
        snprintf(error, error_size, "Node reference too long.");
        return -1;
    }
    memcpy(ref, cursor, ref_len);
    ref[ref_len] = '\0';
    node = kc_flow_model_ensure_node(model, ref);
    if (node == NULL) {
        snprintf(error, error_size, "Unable to allocate node: %s", ref);
        return -1;
    }
    field++;
    if (strcmp(field, "file") == 0) {
        free(node->file);
        node->file = kc_flow_strdup(value);
        return node->file != NULL ? 0 : -1;
    }
    if (strcmp(field, "exec") == 0) {
        free(node->exec);
        node->exec = kc_flow_strdup(value);
        return node->exec != NULL ? 0 : -1;
    }
    if (strcmp(field, "link") == 0) {
        return kc_flow_strings_add(&node->links, value);
    }
    if (strncmp(field, "param.", 6) == 0) {
        return kc_flow_overrides_add(&node->params, field + 6, value);
    }
    snprintf(error, error_size, "Unknown node field: %s", key);
    return -1;
}

/**
 * Loads one flow file.
 * @param path Source file path.
 * @param model Output model.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_load_file(const char *path, kc_flow_model *model, char *error, size_t error_size) {
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
        if (strcmp(key, "flow.id") == 0) {
            free(model->id);
            model->id = kc_flow_strdup(value);
            if (model->id == NULL) {
                free(line);
                fclose(fp);
                kc_flow_model_free(model);
                snprintf(error, error_size, "Out of memory while storing flow id.");
                return -1;
            }
            continue;
        }
        if (strcmp(key, "flow.link") == 0) {
            if (kc_flow_strings_add(&model->entry_links, value) != 0) {
                free(line);
                fclose(fp);
                kc_flow_model_free(model);
                snprintf(error, error_size, "Too many flow links.");
                return -1;
            }
            continue;
        }
        if (strncmp(key, "flow.param.", 11) == 0) {
            if (kc_flow_overrides_add(&model->params, key + 11, value) != 0) {
                free(line);
                fclose(fp);
                kc_flow_model_free(model);
                snprintf(error, error_size, "Unable to store flow parameter.");
                return -1;
            }
            continue;
        }
        if (strncmp(key, "node.", 5) == 0) {
            if (kc_flow_parse_node_field(model, key, value, error, error_size) != 0) {
                free(line);
                fclose(fp);
                kc_flow_model_free(model);
                return -1;
            }
            continue;
        }
        free(line);
        fclose(fp);
        kc_flow_model_free(model);
        snprintf(error, error_size, "Unknown record: %s", key);
        return -1;
    }
    free(line);
    fclose(fp);
    return kc_flow_validate_model(model, error, error_size);
}
