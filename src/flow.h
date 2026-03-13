/**
 * flow.h
 * Summary: Shared model, runtime, and platform declarations for kc-flow.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#ifndef KC_FLOW_H
#define KC_FLOW_H

#include <stddef.h>

#define KC_FLOW_MAX_NODES 256
#define KC_FLOW_MAX_LINKS 256
#define KC_FLOW_MAX_BRANCHES 1024
#define KC_FLOW_MAX_OVERRIDES 256
#define KC_FLOW_MAX_PATH 4096

typedef struct kc_flow_record {
    char *key;
    char *value;
} kc_flow_record;

typedef struct kc_flow_overrides {
    kc_flow_record records[KC_FLOW_MAX_OVERRIDES];
    size_t count;
} kc_flow_overrides;

typedef struct kc_flow_strings {
    char *values[KC_FLOW_MAX_LINKS];
    size_t count;
} kc_flow_strings;

typedef struct kc_flow_node {
    char *ref;
    char *file;
    char *exec;
    kc_flow_overrides params;
    kc_flow_strings links;
} kc_flow_node;

typedef struct kc_flow_model {
    char *id;
    kc_flow_overrides params;
    kc_flow_strings entry_links;
    kc_flow_node nodes[KC_FLOW_MAX_NODES];
    size_t node_count;
} kc_flow_model;

typedef struct kc_flow_runtime_cfg {
    size_t workers;
    int fd_in;
    int fd_out;
    int fd_status;
} kc_flow_runtime_cfg;

typedef struct kc_flow_fd_list {
    int values[KC_FLOW_MAX_BRANCHES];
    size_t count;
} kc_flow_fd_list;

int kc_flow_file_exists(const char *path);
int kc_flow_build_path(char *buffer, size_t size, const char *base, const char *value);
void kc_flow_dirname(const char *path, char *buffer, size_t size);

void kc_flow_overrides_init(kc_flow_overrides *overrides);
void kc_flow_overrides_free(kc_flow_overrides *overrides);
const char *kc_flow_overrides_get(const kc_flow_overrides *overrides, const char *key);
int kc_flow_overrides_add(kc_flow_overrides *overrides, const char *key, const char *value);
int kc_flow_overrides_copy(kc_flow_overrides *dst, const kc_flow_overrides *src);

void kc_flow_strings_init(kc_flow_strings *strings);
void kc_flow_strings_free(kc_flow_strings *strings);
int kc_flow_strings_add(kc_flow_strings *strings, const char *value);

void kc_flow_model_init(kc_flow_model *model);
void kc_flow_model_free(kc_flow_model *model);
const kc_flow_node *kc_flow_model_find_node(const kc_flow_model *model, const char *ref);
int kc_flow_load_file(const char *path, kc_flow_model *model, char *error, size_t error_size);
int kc_flow_validate_model(const kc_flow_model *model, char *error, size_t error_size);
int kc_flow_collect_node_params(
    const kc_flow_model *model,
    const kc_flow_node *node,
    const kc_flow_overrides *flow_params,
    kc_flow_overrides *effective_params,
    char *error,
    size_t error_size
);
char *kc_flow_resolve_template(
    const char *text,
    const kc_flow_overrides *flow_params,
    const kc_flow_overrides *node_params,
    char *error,
    size_t error_size
);

void kc_flow_fd_list_init(kc_flow_fd_list *list);
void kc_flow_fd_list_free(kc_flow_fd_list *list);
int kc_flow_fd_list_add(kc_flow_fd_list *list, int fd);
int kc_flow_fd_list_append(kc_flow_fd_list *dst, kc_flow_fd_list *src);

int kc_flow_create_artifact_fd(char *error, size_t error_size);
int kc_flow_dup_artifact_fd(int fd, char *error, size_t error_size);
int kc_flow_copy_artifact_fd(int src, int dst);
void kc_flow_release_fd(int fd);

int kc_flow_run_exec(
    const char *flow_path,
    const char *command,
    const kc_flow_overrides *flow_params,
    const kc_flow_overrides *node_params,
    int fd_in,
    int fd_out,
    char *error,
    size_t error_size
);
int kc_flow_run_model(
    const kc_flow_model *model,
    const kc_flow_runtime_cfg *cfg,
    const kc_flow_overrides *flow_params,
    const char *path,
    char *error,
    size_t error_size
);
int kc_flow_run_node(
    const kc_flow_model *model,
    const kc_flow_runtime_cfg *cfg,
    const kc_flow_overrides *flow_params,
    const char *flow_path,
    const kc_flow_node *node,
    int input_fd,
    kc_flow_fd_list *outputs,
    char *error,
    size_t error_size
);

long kc_flow_platform_pid(void);
int kc_flow_platform_write_all(int fd, const void *buffer, size_t size);
int kc_flow_platform_rewind_fd(int fd);
int kc_flow_platform_dup_fd(int fd);
void kc_flow_platform_close_fd(int fd);
int kc_flow_platform_open_null_fd(int write_mode);
int kc_flow_platform_create_artifact_fd(char *error, size_t error_size);
int kc_flow_platform_run_command(
    const char *flow_path,
    const char *command,
    const kc_flow_overrides *flow_params,
    const kc_flow_overrides *node_params,
    int fd_in,
    int fd_out,
    char *error,
    size_t error_size
);

int kc_flow_status_emit(
    int fd,
    const char *event,
    const char *kind,
    const char *id,
    const char *path,
    const char *node,
    const char *target_kind,
    const char *target_path,
    const char *status,
    const char *message
);
int kc_flow_status_write_run_event(
    int fd,
    const char *event,
    const char *kind,
    const char *id,
    const char *path,
    const char *status,
    const char *message
);
int kc_flow_status_write_node_event(
    int fd,
    const char *event,
    const char *node,
    const char *target_kind,
    const char *target_path,
    const char *status,
    const char *message
);

#endif
