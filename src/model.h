/**
 * model.h
 * Summary: Shared file model, indexes, and contract/flow parsing interfaces.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_MODEL_H
#define KC_FLOW_MODEL_H

#include <stddef.h>

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

typedef struct kc_flow_overrides {
    kc_flow_record records[KC_STDIO_MAX_INDEXES];
    size_t count;
} kc_flow_overrides;

int kc_flow_file_exists(const char *path);
void kc_flow_model_init(kc_flow_model *model);
void kc_flow_model_free(kc_flow_model *model);
const char *kc_flow_model_get(const kc_flow_model *model, const char *key);
int kc_flow_load_file(const char *path, kc_flow_model *model, char *error, size_t error_size);
int kc_flow_validate_model(const kc_flow_model *model, char *error, size_t error_size);

void kc_flow_overrides_init(kc_flow_overrides *overrides);
void kc_flow_overrides_free(kc_flow_overrides *overrides);
const char *kc_flow_overrides_get(const kc_flow_overrides *overrides, const char *key);
int kc_flow_overrides_add(kc_flow_overrides *overrides, const char *key, const char *value);

int kc_flow_build_path(char *buffer, size_t size, const char *base, const char *value);
void kc_flow_dirname(const char *path, char *buffer, size_t size);
const char *kc_flow_lookup_indexed_id_value(
    const kc_flow_model *model,
    const kc_flow_index_set *set,
    const char *prefix,
    const char *id,
    const char *field
);

#endif
