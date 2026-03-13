/**
 * runtime.c
 * Summary: Branch list helpers and top-level flow dispatch.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <string.h>

/**
 * Initializes one fd list.
 * @param list Output list.
 * @return void
 */
void kc_flow_fd_list_init(kc_flow_fd_list *list) {
    memset(list, 0, sizeof(*list));
}

/**
 * Closes and clears one fd list.
 * @param list Owned list.
 * @return void
 */
void kc_flow_fd_list_free(kc_flow_fd_list *list) {
    size_t i;

    for (i = 0; i < list->count; ++i) {
        kc_flow_release_fd(list->values[i]);
    }
    kc_flow_fd_list_init(list);
}

/**
 * Appends one descriptor to one fd list.
 * @param list Destination list.
 * @param fd Descriptor.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_fd_list_add(kc_flow_fd_list *list, int fd) {
    if (list->count >= KC_FLOW_MAX_BRANCHES) {
        return -1;
    }
    list->values[list->count++] = fd;
    return 0;
}

/**
 * Moves descriptors from one list into another.
 * @param dst Destination list.
 * @param src Source list.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_fd_list_append(kc_flow_fd_list *dst, kc_flow_fd_list *src) {
    size_t i;

    for (i = 0; i < src->count; ++i) {
        if (kc_flow_fd_list_add(dst, src->values[i]) != 0) {
            return -1;
        }
    }
    src->count = 0;
    return 0;
}

/**
 * Flushes one list of terminal branch outputs.
 * @param outputs Terminal outputs.
 * @param fd_out Runtime output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_flush_outputs(
    kc_flow_fd_list *outputs,
    int fd_out,
    char *error,
    size_t error_size
) {
    size_t i;

    for (i = 0; i < outputs->count; ++i) {
        if (outputs->values[i] < 0) {
            continue;
        }
        if (kc_flow_platform_rewind_fd(outputs->values[i]) != 0 ||
                kc_flow_copy_artifact_fd(outputs->values[i], fd_out) != 0) {
            snprintf(error, error_size, "Unable to flush branch output.");
            return -1;
        }
    }
    return 0;
}

/**
 * Duplicates one optional input descriptor for an entry branch.
 * @param fd Input descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int Descriptor or -1.
 */
static int kc_flow_branch_dup_input(int fd, char *error, size_t error_size) {
    if (fd < 0) {
        return -1;
    }
    return kc_flow_dup_artifact_fd(fd, error, error_size);
}

/**
 * Runs one parsed flow model.
 * @param model Parsed model.
 * @param cfg Runtime configuration.
 * @param flow_params Runtime flow overrides.
 * @param path Flow source path.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_model(
    const kc_flow_model *model,
    const kc_flow_runtime_cfg *cfg,
    const kc_flow_overrides *flow_params,
    const char *path,
    char *error,
    size_t error_size
) {
    kc_flow_overrides effective_flow_params;
    kc_flow_fd_list outputs;
    size_t i;
    int rc;

    kc_flow_overrides_init(&effective_flow_params);
    kc_flow_fd_list_init(&outputs);
    if (kc_flow_overrides_copy(&effective_flow_params, &model->params) != 0) {
        snprintf(error, error_size, "Unable to copy flow parameters.");
        return -1;
    }
    for (i = 0; flow_params != NULL && i < flow_params->count; ++i) {
        if (kc_flow_overrides_add(
                &effective_flow_params,
                flow_params->records[i].key,
                flow_params->records[i].value
            ) != 0) {
            kc_flow_overrides_free(&effective_flow_params);
            snprintf(error, error_size, "Unable to merge flow parameters.");
            return -1;
        }
    }
    rc = 0;
    for (i = 0; i < model->entry_links.count; ++i) {
        const kc_flow_node *entry;
        int branch_fd;

        entry = kc_flow_model_find_node(model, model->entry_links.values[i]);
        if (entry == NULL) {
            kc_flow_overrides_free(&effective_flow_params);
            snprintf(error, error_size, "Unknown flow link: %s", model->entry_links.values[i]);
            return -1;
        }
        branch_fd = kc_flow_branch_dup_input(cfg != NULL ? cfg->fd_in : -1, error, error_size);
        if ((cfg != NULL ? cfg->fd_in : -1) >= 0 && branch_fd < 0) {
            kc_flow_overrides_free(&effective_flow_params);
            return -1;
        }
        if (kc_flow_run_node(
                model,
                cfg,
                &effective_flow_params,
                path,
                entry,
                branch_fd,
                &outputs,
                error,
                error_size
            ) != 0) {
            kc_flow_release_fd(branch_fd);
            rc = -1;
            break;
        }
    }
    if (rc == 0 && cfg != NULL && cfg->fd_out >= 0) {
        rc = kc_flow_flush_outputs(&outputs, cfg->fd_out, error, error_size);
    }
    kc_flow_fd_list_free(&outputs);
    kc_flow_overrides_free(&effective_flow_params);
    return rc;
}
