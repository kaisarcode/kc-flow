/**
 * model.c
 * Summary: Core model storage and override map operations.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "model.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
 * Checks whether a regular file exists at the given path.
 * @param path File path.
 * @return int 1 if path exists and is regular; 0 otherwise.
 */
int kc_flow_file_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/**
 * Initializes one model instance to zero state.
 * @param model Output model.
 * @return void
 */
void kc_flow_model_init(kc_flow_model *model) {
    memset(model, 0, sizeof(*model));
}

/**
 * Releases all allocated key/value records in a model.
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
 * Initializes runtime override storage.
 * @param overrides Output override store.
 * @return void
 */
void kc_flow_overrides_init(kc_flow_overrides *overrides) {
    memset(overrides, 0, sizeof(*overrides));
}

/**
 * Releases all allocated runtime override key/value records.
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
 * Reads one override value by key.
 * @param overrides Override store.
 * @param key Lookup key.
 * @return const char* Value if found; NULL otherwise.
 */
const char *kc_flow_overrides_get(const kc_flow_overrides *overrides,
                                  const char *key) {
    size_t i;

    for (i = 0; i < overrides->count; ++i) {
        if (strcmp(overrides->records[i].key, key) == 0) {
            return overrides->records[i].value;
        }
    }

    return NULL;
}

/**
 * Adds or updates one override entry.
 * @param overrides Override store.
 * @param key Override key.
 * @param value Override value.
 * @return int 0 on success; non-zero on memory/capacity errors.
 */
int kc_flow_overrides_add(kc_flow_overrides *overrides,
                          const char *key,
                          const char *value) {
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

/**
 * Returns one model value by raw key.
 * @param model Parsed model.
 * @param key Lookup key.
 * @return const char* Value if found; NULL otherwise.
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
