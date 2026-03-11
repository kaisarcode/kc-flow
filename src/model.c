/**
 * model.c
 * Summary: Shared model storage, overrides, and path helpers.
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
 * Checks whether one regular file exists.
 * @param path File path.
 * @return int 1 if present; otherwise 0.
 */
int kc_flow_file_exists(const char *path) {
    struct stat st;

    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * Initializes one model.
 * @param model Output model.
 * @return void
 */
void kc_flow_model_init(kc_flow_model *model) {
    memset(model, 0, sizeof(*model));
}

/**
 * Frees one model.
 * @param model Model instance.
 * @return void
 */
void kc_flow_model_free(kc_flow_model *model) {
    size_t i;

    for (i = 0; i < model->record_count; ++i) {
        free(model->records[i].key);
        free(model->records[i].value);
    }
    kc_flow_model_init(model);
}

/**
 * Looks up one raw model key.
 * @param model Parsed model.
 * @param key Raw key.
 * @return const char* Value or NULL.
 */
const char *kc_flow_model_get(const kc_flow_model *model, const char *key) {
    size_t i;

    for (i = 0; i < model->record_count; ++i) {
        if (strcmp(model->records[i].key, key) == 0) {
            return model->records[i].value;
        }
    }
    return NULL;
}

/**
 * Looks up one indexed field by numeric record id.
 * @param model Parsed model.
 * @param set Index set.
 * @param prefix Section prefix.
 * @param index Numeric record id.
 * @param field Field name.
 * @return const char* Value or NULL.
 */
const char *kc_flow_lookup_indexed_value(
    const kc_flow_model *model,
    const kc_flow_index_set *set,
    const char *prefix,
    int index,
    const char *field
) {
    size_t i;
    char key[128];

    for (i = 0; i < set->count; ++i) {
        if (set->values[i] == index) {
            snprintf(key, sizeof(key), "%s%d.%s", prefix, index, field);
            return kc_flow_model_get(model, key);
        }
    }
    return NULL;
}

/**
 * Looks up one indexed field by logical id.
 * @param model Parsed model.
 * @param set Index set.
 * @param prefix Section prefix.
 * @param id Logical id.
 * @param field Field name.
 * @return const char* Value or NULL.
 */
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

/**
 * Initializes one override store.
 * @param overrides Output override store.
 * @return void
 */
void kc_flow_overrides_init(kc_flow_overrides *overrides) {
    memset(overrides, 0, sizeof(*overrides));
}

/**
 * Frees one override store.
 * @param overrides Override store.
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
 * @param key Override key.
 * @return const char* Value or NULL.
 */
const char *kc_flow_overrides_get(
    const kc_flow_overrides *overrides,
    const char *key
) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        if (strcmp(overrides->records[i].key, key) == 0) {
            return overrides->records[i].value;
        }
    }
    return NULL;
}

/**
 * Adds or updates one override value.
 * @param overrides Override store.
 * @param key Override key.
 * @param value Override value.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_overrides_add(
    kc_flow_overrides *overrides,
    const char *key,
    const char *value
) {
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
    if (overrides->count >= KC_FLOW_MAX_INDEXES) {
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
 * Builds one absolute or base-relative path.
 * @param buffer Output buffer.
 * @param size Output buffer size.
 * @param base Base directory.
 * @param value Relative or absolute path.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_build_path(
    char *buffer,
    size_t size,
    const char *base,
    const char *value
) {
    size_t base_len;

    if (value == NULL || value[0] == '\0') {
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

/**
 * Computes the directory name for one path.
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
