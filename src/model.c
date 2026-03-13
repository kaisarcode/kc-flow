/**
 * model.c
 * Summary: Flow parsing, scope resolution, and structural validation.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
 * Checks whether one regular file exists.
 * @param path File path.
 * @return int 1 if present; otherwise 0.
 */
int kc_flow_file_exists(const char *path) {
    struct stat st;

    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * Builds one absolute or relative path.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @param base Base directory.
 * @param value Relative or absolute path.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_build_path(char *buffer, size_t size, const char *base, const char *value) {
    size_t base_len;

    if (value == NULL || value[0] == '\0') {
        return -1;
    }
    if (value[0] == '/') {
        return snprintf(buffer, size, "%s", value) >= (int)size ? -1 : 0;
    }
    base_len = base != NULL ? strlen(base) : 0;
    if (base_len == 0) {
        return snprintf(buffer, size, "%s", value) >= (int)size ? -1 : 0;
    }
    return snprintf(buffer, size, "%s/%s", base, value) >= (int)size ? -1 : 0;
}

/**
 * Computes the directory portion of one path.
 * @param path Input path.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @return void
 */
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

/**
 * Initializes one override store.
 * @param overrides Output store.
 * @return void
 */
void kc_flow_overrides_init(kc_flow_overrides *overrides) {
    memset(overrides, 0, sizeof(*overrides));
}

/**
 * Frees one override store.
 * @param overrides Store instance.
 * @return void
 */
void kc_flow_overrides_free(kc_flow_overrides *overrides) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        free(overrides->records[i].key);
        free(overrides->records[i].value);
    }
    kc_flow_overrides_init(overrides);
}

/**
 * Looks up one override key.
 * @param overrides Override store.
 * @param key Logical key.
 * @return const char* Value or NULL.
 */
const char *kc_flow_overrides_get(const kc_flow_overrides *overrides, const char *key) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        if (strcmp(overrides->records[i].key, key) == 0) {
            return overrides->records[i].value;
        }
    }
    return NULL;
}

/**
 * Adds or updates one override.
 * @param overrides Override store.
 * @param key Logical key.
 * @param value Logical value.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_overrides_add(kc_flow_overrides *overrides, const char *key, const char *value) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        char *next_value;

        if (strcmp(overrides->records[i].key, key) != 0) {
            continue;
        }
        next_value = kc_flow_strdup(value);
        if (next_value == NULL) {
            return -1;
        }
        free(overrides->records[i].value);
        overrides->records[i].value = next_value;
        return 0;
    }
    if (overrides->count >= KC_FLOW_MAX_OVERRIDES) {
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

/**
 * Copies one override store into another.
 * @param dst Destination store.
 * @param src Source store.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_overrides_copy(kc_flow_overrides *dst, const kc_flow_overrides *src) {
    size_t i;

    kc_flow_overrides_init(dst);
    for (i = 0; i < src->count; ++i) {
        if (kc_flow_overrides_add(dst, src->records[i].key, src->records[i].value) != 0) {
            kc_flow_overrides_free(dst);
            return -1;
        }
    }
    return 0;
}

/**
 * Initializes one string list.
 * @param strings Output list.
 * @return void
 */
void kc_flow_strings_init(kc_flow_strings *strings) {
    memset(strings, 0, sizeof(*strings));
}

/**
 * Frees one string list.
 * @param strings List instance.
 * @return void
 */
void kc_flow_strings_free(kc_flow_strings *strings) {
    size_t i;

    for (i = 0; i < strings->count; ++i) {
        free(strings->values[i]);
    }
    kc_flow_strings_init(strings);
}

/**
 * Appends one string to one list.
 * @param strings Destination list.
 * @param value Source value.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_strings_add(kc_flow_strings *strings, const char *value) {
    if (strings->count >= KC_FLOW_MAX_LINKS) {
        return -1;
    }
    strings->values[strings->count] = kc_flow_strdup(value);
    if (strings->values[strings->count] == NULL) {
        return -1;
    }
    strings->count++;
    return 0;
}

/**
 * Initializes one parsed model.
 * @param model Output model.
 * @return void
 */
void kc_flow_model_init(kc_flow_model *model) {
    memset(model, 0, sizeof(*model));
}

/**
 * Frees one parsed model.
 * @param model Model instance.
 * @return void
 */
void kc_flow_model_free(kc_flow_model *model) {
    size_t i;

    free(model->id);
    kc_flow_overrides_free(&model->params);
    kc_flow_strings_free(&model->entry_links);
    for (i = 0; i < model->node_count; ++i) {
        free(model->nodes[i].ref);
        free(model->nodes[i].file);
        free(model->nodes[i].exec);
        kc_flow_overrides_free(&model->nodes[i].params);
        kc_flow_strings_free(&model->nodes[i].links);
    }
    kc_flow_model_init(model);
}

/**
 * Looks up one node by local reference.
 * @param model Parsed model.
 * @param ref Node reference.
 * @return const kc_flow_node* Node or NULL.
 */
const kc_flow_node *kc_flow_model_find_node(const kc_flow_model *model, const char *ref) {
    size_t i;

    for (i = 0; i < model->node_count; ++i) {
        if (strcmp(model->nodes[i].ref, ref) == 0) {
            return &model->nodes[i];
        }
    }
    return NULL;
}
