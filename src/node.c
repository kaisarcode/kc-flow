/**
 * node.c
 * Summary: Node expansion, exec application, and branch fan-out traversal.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * Duplicates one optional input descriptor for a child branch.
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
 * Runs one child flow and yields every terminal branch output.
 * @param cfg Runtime configuration.
 * @param flow_params Effective child flow params.
 * @param path Child flow path.
 * @param input_fd Input descriptor.
 * @param outputs Output branch list.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_run_child_flow(const kc_flow_runtime_cfg *cfg, const kc_flow_overrides *flow_params, const char *path, int input_fd, kc_flow_fd_list *outputs, char *error, size_t error_size) {
    kc_flow_model *child;
    kc_flow_overrides effective_flow_params;
    size_t i;

    child = malloc(sizeof(*child));
    if (child == NULL) {
        snprintf(error, error_size, "Out of memory while loading child flow.");
        return -1;
    }
    kc_flow_model_init(child);
    if (kc_flow_load_file(path, child, error, error_size) != 0) {
        free(child);
        return -1;
    }
    kc_flow_overrides_init(&effective_flow_params);
    if (kc_flow_overrides_copy(&effective_flow_params, &child->params) != 0) {
        kc_flow_model_free(child);
        free(child);
        snprintf(error, error_size, "Unable to copy child flow parameters.");
        return -1;
    }
    for (i = 0; i < flow_params->count; ++i) {
        if (kc_flow_overrides_add(
                &effective_flow_params,
                flow_params->records[i].key,
                flow_params->records[i].value
            ) != 0) {
            kc_flow_overrides_free(&effective_flow_params);
            kc_flow_model_free(child);
            free(child);
            snprintf(error, error_size, "Unable to merge child flow parameters.");
            return -1;
        }
    }
    for (i = 0; i < child->entry_links.count; ++i) {
        const kc_flow_node *entry;
        int branch_fd;
        const char *entry_ref;

        entry_ref = child->entry_links.values[i];
        entry = kc_flow_model_find_node(child, entry_ref);
        if (entry == NULL) {
            kc_flow_overrides_free(&effective_flow_params);
            kc_flow_model_free(child);
            free(child);
            snprintf(error, error_size, "Unknown child flow link: %s", entry_ref);
            return -1;
        }
        branch_fd = kc_flow_branch_dup_input(input_fd, error, error_size);
        if (input_fd >= 0 && branch_fd < 0) {
            kc_flow_overrides_free(&effective_flow_params);
            kc_flow_model_free(child);
            free(child);
            return -1;
        }
        if (kc_flow_run_node(child, cfg, &effective_flow_params, path, entry, branch_fd, outputs, error, error_size) != 0) {
            kc_flow_release_fd(branch_fd);
            kc_flow_overrides_free(&effective_flow_params);
            kc_flow_model_free(child);
            free(child);
            return -1;
        }
    }
    kc_flow_overrides_free(&effective_flow_params);
    kc_flow_model_free(child);
    free(child);
    return 0;
}

/**
 * Applies one node exec to every active branch.
 * @param flow_path Current flow path.
 * @param node Node being executed.
 * @param flow_params Current flow params.
 * @param node_params Current node params.
 * @param active Active input branches.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_run_node_execs(const char *flow_path, const kc_flow_node *node, const kc_flow_overrides *flow_params, const kc_flow_overrides *node_params, kc_flow_fd_list *active, char *error, size_t error_size) {
    kc_flow_fd_list next;
    size_t i;

    kc_flow_fd_list_init(&next);
    for (i = 0; i < active->count; ++i) {
        int output_fd;

        output_fd = kc_flow_create_artifact_fd(error, error_size);
        if (output_fd < 0) {
            kc_flow_fd_list_free(&next);
            return -1;
        }
        if (kc_flow_run_exec(flow_path, node->exec, flow_params, node_params, active->values[i], output_fd, error, error_size) != 0 || kc_flow_fd_list_add(&next, output_fd) != 0) {
            kc_flow_release_fd(output_fd);
            kc_flow_fd_list_free(&next);
            return -1;
        }
        kc_flow_release_fd(active->values[i]);
    }
    active->count = 0;
    if (kc_flow_fd_list_append(active, &next) != 0) {
        kc_flow_fd_list_free(&next);
        snprintf(error, error_size, "Too many active branches.");
        return -1;
    }
    return 0;
}

/**
 * Expands one node file into child branches.
 * @param cfg Runtime configuration.
 * @param flow_path Current flow path.
 * @param node Node being expanded.
 * @param node_params Effective node params.
 * @param active Active branch list.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_run_node_file(const kc_flow_runtime_cfg *cfg, const char *flow_path, const kc_flow_node *node, const kc_flow_overrides *node_params, kc_flow_fd_list *active, char *error, size_t error_size) {
    kc_flow_fd_list next;
    char flow_dir[KC_FLOW_MAX_PATH];
    char child_path[KC_FLOW_MAX_PATH];
    size_t i;

    kc_flow_dirname(flow_path, flow_dir, sizeof(flow_dir));
    if (kc_flow_build_path(child_path, sizeof(child_path), flow_dir, node->file) != 0) {
        snprintf(error, error_size, "Invalid node file path.");
        return -1;
    }
    if (!kc_flow_file_exists(child_path)) {
        snprintf(error, error_size, "Node file not found: %s", child_path);
        return -1;
    }
    kc_flow_fd_list_init(&next);
    for (i = 0; i < active->count; ++i) {
        if (kc_flow_run_child_flow(cfg, node_params, child_path, active->values[i], &next, error, error_size) != 0) {
            kc_flow_fd_list_free(&next);
            return -1;
        }
        kc_flow_release_fd(active->values[i]);
    }
    active->count = 0;
    if (kc_flow_fd_list_append(active, &next) != 0) {
        kc_flow_fd_list_free(&next);
        snprintf(error, error_size, "Too many active branches.");
        return -1;
    }
    return 0;
}

/**
 * Propagates every active branch into linked child nodes.
 * @param model Current flow model.
 * @param cfg Runtime configuration.
 * @param flow_params Current flow params.
 * @param flow_path Current flow path.
 * @param node Source node.
 * @param active Current branch outputs.
 * @param outputs Final output list.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
static int kc_flow_run_node_links(const kc_flow_model *model, const kc_flow_runtime_cfg *cfg, const kc_flow_overrides *flow_params, const char *flow_path, const kc_flow_node *node, kc_flow_fd_list *active, kc_flow_fd_list *outputs, char *error, size_t error_size) {
    kc_flow_fd_list produced;
    size_t i;
    size_t j;

    if (node->links.count == 0) {
        if (kc_flow_fd_list_append(outputs, active) != 0) {
            snprintf(error, error_size, "Too many terminal branches.");
            return -1;
        }
        return 0;
    }
    kc_flow_fd_list_init(&produced);
    for (i = 0; i < active->count; ++i) {
        for (j = 0; j < node->links.count; ++j) {
            const kc_flow_node *child;
            int branch_fd;

            child = kc_flow_model_find_node(model, node->links.values[j]);
            if (child == NULL) {
                kc_flow_fd_list_free(&produced);
                snprintf(error, error_size, "Unknown node link: %s", node->links.values[j]);
                return -1;
            }
            branch_fd = kc_flow_branch_dup_input(active->values[i], error, error_size);
            if (active->values[i] >= 0 && branch_fd < 0) {
                kc_flow_fd_list_free(&produced);
                return -1;
            }
            if (kc_flow_run_node(model, cfg, flow_params, flow_path, child, branch_fd, &produced, error, error_size) != 0) {
                kc_flow_release_fd(branch_fd);
                kc_flow_fd_list_free(&produced);
                return -1;
            }
        }
        kc_flow_release_fd(active->values[i]);
    }
    active->count = 0;
    if (kc_flow_fd_list_append(outputs, &produced) != 0) {
        kc_flow_fd_list_free(&produced);
        snprintf(error, error_size, "Too many produced branches.");
        return -1;
    }
    return 0;
}

/**
 * Runs one node in the current flow scope.
 * @param model Current flow model.
 * @param cfg Runtime configuration.
 * @param flow_params Current flow params.
 * @param flow_path Current flow path.
 * @param node Node to execute.
 * @param input_fd Input descriptor for this branch.
 * @param outputs Final branch output list.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_node(const kc_flow_model *model, const kc_flow_runtime_cfg *cfg, const kc_flow_overrides *flow_params, const char *flow_path, const kc_flow_node *node, int input_fd, kc_flow_fd_list *outputs, char *error, size_t error_size) {
    kc_flow_overrides node_params;
    kc_flow_fd_list active;
    int rc;

    kc_flow_fd_list_init(&active);
    kc_flow_overrides_init(&node_params);
    if (kc_flow_collect_node_params(model, node, flow_params, &node_params, error, error_size) != 0) {
        return -1;
    }
    if (kc_flow_fd_list_add(&active, input_fd) != 0) {
        kc_flow_overrides_free(&node_params);
        snprintf(error, error_size, "Too many active branches.");
        return -1;
    }
    kc_flow_status_write_node_event(cfg != NULL ? cfg->fd_status : -1, "node.started", node->ref, "node", flow_path, NULL, NULL);
    fprintf(stderr, "[kc-flow] step node=%s flow=%s\n", node->ref, flow_path);
    rc = 0;
    if (node->file != NULL && node->file[0] != '\0') {
        rc = kc_flow_run_node_file(cfg, flow_path, node, &node_params, &active, error, error_size);
    }
    if (rc == 0 && node->exec != NULL && node->exec[0] != '\0') {
        rc = kc_flow_run_node_execs(flow_path, node, flow_params, &node_params, &active, error, error_size);
    }
    if (rc == 0) {
        rc = kc_flow_run_node_links(model, cfg, flow_params, flow_path, node, &active, outputs, error, error_size);
    }
    kc_flow_status_write_node_event(cfg != NULL ? cfg->fd_status : -1, "node.finished", node->ref, "node", flow_path, rc == 0 ? "ok" : "error", rc == 0 ? NULL : error);
    kc_flow_fd_list_free(&active);
    kc_flow_overrides_free(&node_params);
    return rc;
}
