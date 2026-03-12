/**
 * runtime.c
 * Summary: Model validation and composed flow execution.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <string.h>

/**
 * Resolves one model kind name.
 * @param model Parsed model.
 * @return const char* Stable kind name.
 */
const char *kc_flow_model_kind_name(const kc_flow_model *model) {
    return model != NULL && model->kind == KC_FLOW_FILE_FLOW ? "flow" : "contract";
}

/**
 * Executes one flow.
 * @param model Parsed model.
 * @param cfg Runtime configuration.
 * @param overrides Runtime parameter overrides.
 * @param path Source path.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_flow(
    const kc_flow_model *model,
    const kc_flow_runtime_cfg *cfg,
    const kc_flow_overrides *overrides,
    const char *path,
    char *error,
    size_t error_size
) {
    size_t batch_index[KC_FLOW_MAX_INDEXES];
    int batch_inputs[KC_FLOW_MAX_INDEXES];
    kc_flow_worker_handle batch_workers[KC_FLOW_MAX_INDEXES];
    char node_ids[KC_FLOW_MAX_INDEXES][128];
    unsigned char executed[KC_FLOW_MAX_INDEXES];
    kc_flow_overrides artifacts;
    char flow_dir[KC_FLOW_MAX_PATH];
    size_t node_count;
    size_t completed;
    size_t i;
    size_t worker_limit;

    memset(executed, 0, sizeof(executed));
    kc_flow_overrides_init(&artifacts);
    worker_limit = cfg != NULL && cfg->workers > 0 ? cfg->workers : 1;
#if defined(_WIN32)
    worker_limit = 1;
#endif
    kc_flow_dirname(path, flow_dir, sizeof(flow_dir));
    if (kc_flow_collect_node_ids(model, node_ids, &node_count, error, error_size) != 0 ||
            kc_flow_seed_flow_input(model, cfg != NULL ? cfg->fd_in : -1, &artifacts, error, error_size) != 0) {
        kc_flow_cleanup_artifacts(&artifacts);
        kc_flow_overrides_free(&artifacts);
        return -1;
    }
    completed = 0;
    while (completed < node_count) {
        int progressed;
        size_t batch_count;

        progressed = 0;
        batch_count = 0;
        for (i = 0; i < node_count; ++i) {
            int input_fd;
            int ready;

            if (executed[i]) {
                continue;
            }
            ready = kc_flow_prepare_node_input(
                model,
                &artifacts,
                node_ids[i],
                &input_fd,
                error,
                error_size
            );
            if (ready < 0) {
                kc_flow_cleanup_artifacts(&artifacts);
                kc_flow_overrides_free(&artifacts);
                return -1;
            }
            if (ready == 0) {
                continue;
            }
            batch_index[batch_count] = i;
            batch_inputs[batch_count] = input_fd;
            batch_count++;
            if (batch_count >= worker_limit) {
                break;
            }
        }
        if (batch_count == 0) {
            kc_flow_cleanup_artifacts(&artifacts);
            kc_flow_overrides_free(&artifacts);
            snprintf(error, error_size, "Unable to resolve flow dependencies.");
            return -1;
        }
        if (worker_limit > 1 && batch_count > 1) {
            size_t j;

            for (j = 0; j < batch_count; ++j) {
                batch_workers[j].pid = -1;
                batch_workers[j].output_fd = -1;
                if (kc_flow_start_flow_node(
                        model,
                        cfg,
                        flow_dir,
                        node_ids[batch_index[j]],
                        overrides,
                        batch_inputs[j],
                        &batch_workers[j],
                        error,
                        error_size
                    ) != 0) {
                    size_t k;

                    for (k = 0; k <= j; ++k) {
                        kc_flow_release_fd(batch_inputs[k]);
                    }
                    kc_flow_cleanup_artifacts(&artifacts);
                    kc_flow_overrides_free(&artifacts);
                    return -1;
                }
                kc_flow_release_fd(batch_inputs[j]);
            }
            for (j = 0; j < batch_count; ++j) {
                int output_fd;

                output_fd = -1;
                if (kc_flow_finish_flow_node(
                        &batch_workers[j],
                        &output_fd,
                        error,
                        error_size
                    ) != 0 ||
                        kc_flow_publish_node_output(
                            model,
                            &artifacts,
                            node_ids[batch_index[j]],
                            output_fd,
                            error,
                            error_size
                        ) != 0) {
                    if (output_fd >= 0) {
                        kc_flow_release_fd(output_fd);
                    }
                    kc_flow_cleanup_artifacts(&artifacts);
                    kc_flow_overrides_free(&artifacts);
                    return -1;
                }
                executed[batch_index[j]] = 1;
                completed++;
                progressed = 1;
            }
        } else {
            size_t j;

            for (j = 0; j < batch_count; ++j) {
                int output_fd;

                output_fd = -1;
                if (kc_flow_run_node(
                        model,
                        cfg,
                        flow_dir,
                        node_ids[batch_index[j]],
                        overrides,
                        batch_inputs[j],
                        &output_fd,
                        error,
                        error_size
                    ) != 0 ||
                        kc_flow_publish_node_output(
                            model,
                            &artifacts,
                            node_ids[batch_index[j]],
                            output_fd,
                            error,
                            error_size
                        ) != 0) {
                    if (batch_inputs[j] >= 0) {
                        kc_flow_release_fd(batch_inputs[j]);
                    }
                    if (output_fd >= 0) {
                        kc_flow_release_fd(output_fd);
                    }
                    kc_flow_cleanup_artifacts(&artifacts);
                    kc_flow_overrides_free(&artifacts);
                    return -1;
                }
                if (batch_inputs[j] >= 0) {
                    kc_flow_release_fd(batch_inputs[j]);
                }
                executed[batch_index[j]] = 1;
                completed++;
                progressed = 1;
            }
        }
        if (!progressed) {
            kc_flow_cleanup_artifacts(&artifacts);
            kc_flow_overrides_free(&artifacts);
            snprintf(error, error_size, "Unable to resolve flow dependencies.");
            return -1;
        }
    }
    if (kc_flow_flush_flow_output(
            model,
            &artifacts,
            cfg != NULL ? cfg->fd_out : -1,
            error,
            error_size
        ) != 0) {
        kc_flow_cleanup_artifacts(&artifacts);
        kc_flow_overrides_free(&artifacts);
        return -1;
    }
    kc_flow_cleanup_artifacts(&artifacts);
    kc_flow_overrides_free(&artifacts);
    return 0;
}

/**
 * Executes one loaded model.
 * @param model Parsed model.
 * @param cfg Runtime configuration.
 * @param overrides Runtime parameter overrides.
 * @param path Source path.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_run_model(
    const kc_flow_model *model,
    const kc_flow_runtime_cfg *cfg,
    const kc_flow_overrides *overrides,
    const char *path,
    char *error,
    size_t error_size
) {
    if (model->kind == KC_FLOW_FILE_FLOW) {
        return kc_flow_run_flow(model, cfg, overrides, path, error, error_size);
    }
    return kc_flow_run_contract(
        model,
        overrides,
        path,
        cfg != NULL ? cfg->fd_in : -1,
        cfg != NULL ? cfg->fd_out : -1,
        cfg != NULL ? cfg->fd_status : -1,
        error,
        error_size
    );
}
