/**
 * flow.h
 * Summary: Shared model, parser, and runtime declarations for kc-flow.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_H
#define KC_FLOW_H

#include <stddef.h>

#define KC_FLOW_MAX_RECORDS 2048
#define KC_FLOW_MAX_INDEXES 256
#define KC_FLOW_MAX_PATH 4096

typedef enum kc_flow_file_kind {
    KC_FLOW_FILE_NONE = 0,
    KC_FLOW_FILE_CONTRACT,
    KC_FLOW_FILE_FLOW
} kc_flow_file_kind;

typedef struct kc_flow_record {
    char *key;
    char *value;
} kc_flow_record;

typedef struct kc_flow_index_set {
    int values[KC_FLOW_MAX_INDEXES];
    size_t count;
} kc_flow_index_set;

typedef struct kc_flow_model {
    kc_flow_file_kind kind;
    kc_flow_record records[KC_FLOW_MAX_RECORDS];
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
    kc_flow_index_set bind_output;
    kc_flow_index_set nodes;
    kc_flow_index_set links;
} kc_flow_model;

typedef struct kc_flow_overrides {
    kc_flow_record records[KC_FLOW_MAX_INDEXES];
    size_t count;
} kc_flow_overrides;

typedef struct kc_flow_run_output {
    char *stdout_text;
    char *stderr_text;
    int exit_code;
} kc_flow_run_output;

int kc_flow_file_exists(const char *path);
void kc_flow_model_init(kc_flow_model *model);
void kc_flow_model_free(kc_flow_model *model);
const char *kc_flow_model_get(const kc_flow_model *model, const char *key);
const char *kc_flow_lookup_indexed_value(
    const kc_flow_model *model,
    const kc_flow_index_set *set,
    const char *prefix,
    int index,
    const char *field
);
const char *kc_flow_lookup_indexed_id_value(
    const kc_flow_model *model,
    const kc_flow_index_set *set,
    const char *prefix,
    const char *id,
    const char *field
);

void kc_flow_overrides_init(kc_flow_overrides *overrides);
void kc_flow_overrides_free(kc_flow_overrides *overrides);
const char *kc_flow_overrides_get(
    const kc_flow_overrides *overrides,
    const char *key
);
int kc_flow_overrides_add(
    kc_flow_overrides *overrides,
    const char *key,
    const char *value
);

int kc_flow_build_path(
    char *buffer,
    size_t size,
    const char *base,
    const char *value
);
void kc_flow_dirname(const char *path, char *buffer, size_t size);

int kc_flow_load_file(
    const char *path,
    kc_flow_model *model,
    char *error,
    size_t error_size
);

int kc_flow_run_contract(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *cfg_path,
    kc_flow_run_output *output,
    char *error,
    size_t error_size
);
char *kc_flow_resolve_template(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *text,
    char *error,
    size_t error_size
);
void kc_flow_run_output_free(kc_flow_run_output *output);
int kc_flow_collect_contract_outputs(
    const kc_flow_model *model,
    const kc_flow_run_output *output,
    kc_flow_overrides *values,
    char *error,
    size_t error_size
);
int kc_flow_print_contract_outputs(
    const kc_flow_model *model,
    const kc_flow_run_output *output
);

int kc_flow_validate_model(
    const kc_flow_model *model,
    char *error,
    size_t error_size
);
int kc_flow_collect_node_ids(
    const kc_flow_model *model,
    char node_ids[][128],
    size_t *count,
    char *error,
    size_t error_size
);
int kc_flow_run_node(
    const kc_flow_model *model,
    const char *flow_dir,
    const char *node_id,
    const kc_flow_overrides *node_inputs,
    kc_flow_overrides *node_outputs,
    char *error,
    size_t error_size
);
int kc_flow_run_flow(
    const kc_flow_model *model,
    const kc_flow_overrides *overrides,
    const char *path,
    kc_flow_overrides *outputs,
    char *error,
    size_t error_size
);

#endif
