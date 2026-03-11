/**
 * route.c
 * Summary: Flow transport resolution on top of runtime artifact bookkeeping.
 *
 * Author:  KaisarCode
 * Website: https://kaisarcode.com
 * License: GNU GPL v3.0
 */

#include "flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int kc_flow_artifact_store(kc_flow_overrides *artifacts, const char *key, int fd);
int kc_flow_artifact_load(const kc_flow_overrides *artifacts, const char *key);
int kc_flow_artifact_refcount_load(const kc_flow_overrides *artifacts, const char *endpoint);
int kc_flow_artifact_refcount_store(kc_flow_overrides *artifacts, const char *endpoint, int count);
int kc_flow_artifact_consume(kc_flow_overrides *artifacts, const char *endpoint, int *fd, char *error, size_t error_size);

/**
 * Matches one destination endpoint against one node input.
 * @param endpoint Endpoint text.
 * @param node_id Logical node id.
 * @return int 1 when matched; otherwise 0.
 */
static int kc_flow_is_node_input_endpoint(const char *endpoint, const char *node_id) {
    size_t node_len;

    if (strncmp(endpoint, "node.", 5) != 0) {
        return 0;
    }
    node_len = strlen(node_id);
    if (strncmp(endpoint + 5, node_id, node_len) != 0) {
        return 0;
    }
    return strncmp(endpoint + 5 + node_len, ".in.", 4) == 0;
}

/**
 * Matches one source endpoint against one node output.
 * @param endpoint Endpoint text.
 * @param node_id Logical node id.
 * @return int 1 when matched; otherwise 0.
 */
static int kc_flow_is_node_output_endpoint(const char *endpoint, const char *node_id) {
    size_t node_len;

    if (strncmp(endpoint, "node.", 5) != 0) {
        return 0;
    }
    node_len = strlen(node_id);
    if (strncmp(endpoint + 5, node_id, node_len) != 0) {
        return 0;
    }
    return strncmp(endpoint + 5 + node_len, ".out.", 5) == 0;
}

/**
 * Seeds one root flow input artifact from one runtime descriptor.
 * @param model Parsed model.
 * @param fd_in Runtime input descriptor.
 * @param artifacts Runtime artifact store.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_seed_flow_input(const kc_flow_model *model, int fd_in, kc_flow_overrides *artifacts, char *error, size_t error_size) {
    const char *input_id;
    char key[128];
    int artifact_fd;
    int consumers;
    size_t i;

    if (model->inputs.count == 0) {
        return 0;
    }
    input_id = kc_flow_lookup_indexed_value(model, &model->inputs, "input.", model->inputs.values[0], "id");
    if (input_id == NULL || fd_in < 0) {
        snprintf(error, error_size, "Missing runtime input descriptor.");
        return -1;
    }
    snprintf(key, sizeof(key), "input.%s", input_id);
    consumers = 0;
    for (i = 0; i < model->links.count; ++i) {
        const char *from;

        from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "from");
        if (from != NULL && strcmp(from, key) == 0) {
            consumers++;
        }
    }
    artifact_fd = kc_flow_create_artifact_fd(error, error_size);
    if (artifact_fd < 0) {
        return -1;
    }
    if (kc_flow_copy_artifact_fd(fd_in, artifact_fd) != 0 ||
            kc_flow_artifact_store(artifacts, key, artifact_fd) != 0 ||
            kc_flow_artifact_refcount_store(artifacts, key, consumers) != 0) {
        kc_flow_release_fd(artifact_fd);
        snprintf(error, error_size, "Unable to register flow input.");
        return -1;
    }
    return 0;
}

/**
 * Prepares one node input descriptor when all upstream artifacts are ready.
 * @param model Parsed model.
 * @param artifacts Runtime artifact store.
 * @param node_id Logical node id.
 * @param fd_in Output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 1 when ready; 0 when waiting; -1 on failure.
 */
int kc_flow_prepare_node_input(const kc_flow_model *model, const kc_flow_overrides *artifacts, const char *node_id, int *fd_in, char *error, size_t error_size) {
    size_t i;
    const char *source_endpoint;

    *fd_in = -1;
    source_endpoint = NULL;
    for (i = 0; i < model->links.count; ++i) {
        const char *from;
        const char *to;

        from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "from");
        to = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "to");
        if (from == NULL || to == NULL || !kc_flow_is_node_input_endpoint(to, node_id)) {
            continue;
        }
        if (source_endpoint != NULL) {
            snprintf(error, error_size, "Node input must have one upstream source.");
            return -1;
        }
        if (kc_flow_artifact_load(artifacts, from) < 0) {
            return 0;
        }
        source_endpoint = from;
    }
    if (source_endpoint == NULL) {
        return 1;
    }
    return kc_flow_artifact_consume((kc_flow_overrides *)artifacts, source_endpoint, fd_in, error, error_size) == 0 ? 1 : -1;
}

/**
 * Publishes one node output descriptor to every matching downstream source.
 * @param model Parsed model.
 * @param artifacts Runtime artifact store.
 * @param node_id Logical node id.
 * @param fd_out Output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_publish_node_output(const kc_flow_model *model, kc_flow_overrides *artifacts, const char *node_id, int fd_out, char *error, size_t error_size) {
    size_t i;
    int published;

    if (fd_out < 0) {
        return 0;
    }
    published = 0;
    for (i = 0; i < model->links.count; ++i) {
        const char *from;
        int refcount;

        from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "from");
        if (from == NULL || !kc_flow_is_node_output_endpoint(from, node_id)) {
            continue;
        }
        refcount = kc_flow_artifact_refcount_load(artifacts, from);
        if (refcount == 0 && kc_flow_artifact_store(artifacts, from, fd_out) != 0) {
            snprintf(error, error_size, "Unable to publish node output.");
            return -1;
        }
        if (kc_flow_artifact_refcount_store(artifacts, from, refcount + 1) != 0) {
            snprintf(error, error_size, "Unable to track node output consumers.");
            return -1;
        }
        published = 1;
    }
    if (!published) {
        kc_flow_release_fd(fd_out);
    }
    return 0;
}

/**
 * Resolves the public output descriptor of one flow.
 * @param model Parsed model.
 * @param artifacts Runtime artifact store.
 * @param fd_out Output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_collect_final_output(const kc_flow_model *model, const kc_flow_overrides *artifacts, int *fd_out, char *error, size_t error_size) {
    size_t i;
    const char *source_endpoint;

    *fd_out = -1;
    if (model->outputs.count == 0) {
        return 0;
    }
    source_endpoint = NULL;
    for (i = 0; i < model->links.count; ++i) {
        const char *from;
        const char *to;

        from = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "from");
        to = kc_flow_lookup_indexed_value(model, &model->links, "link.", model->links.values[i], "to");
        if (from == NULL || to == NULL || strncmp(to, "output.", 7) != 0) {
            continue;
        }
        if (source_endpoint != NULL) {
            snprintf(error, error_size, "Flow output must have one upstream source.");
            return -1;
        }
        if (kc_flow_artifact_load(artifacts, from) < 0) {
            snprintf(error, error_size, "Unable to resolve final flow output.");
            return -1;
        }
        source_endpoint = from;
    }
    if (source_endpoint == NULL) {
        snprintf(error, error_size, "Missing final flow output.");
        return -1;
    }
    return kc_flow_artifact_consume((kc_flow_overrides *)artifacts, source_endpoint, fd_out, error, error_size);
}

/**
 * Flushes the public output of one flow into one runtime descriptor.
 * @param model Parsed model.
 * @param artifacts Runtime artifact store.
 * @param fd_out Runtime output descriptor.
 * @param error Error buffer.
 * @param error_size Error buffer size.
 * @return int 0 on success; non-zero on failure.
 */
int kc_flow_flush_flow_output(const kc_flow_model *model, const kc_flow_overrides *artifacts, int fd_out, char *error, size_t error_size) {
    int artifact_fd;

    if (model->outputs.count == 0) {
        return 0;
    }
    if (kc_flow_collect_final_output(model, artifacts, &artifact_fd, error, error_size) != 0) {
        return -1;
    }
    if (kc_flow_copy_artifact_fd(artifact_fd, fd_out) != 0) {
        kc_flow_release_fd(artifact_fd);
        snprintf(error, error_size, "Unable to flush flow output.");
        return -1;
    }
    kc_flow_release_fd(artifact_fd);
    return 0;
}

/**
 * Closes every stored artifact descriptor.
 * @param artifacts Runtime artifact store.
 * @return void
 */
void kc_flow_cleanup_artifacts(kc_flow_overrides *artifacts) {
    size_t i;

    for (i = 0; i < artifacts->count; ++i) {
        int fd;

        if (strncmp(artifacts->records[i].key, "__ref__:", 8) == 0) {
            continue;
        }
        fd = atoi(artifacts->records[i].value);
        if (fd >= 0) {
            kc_flow_release_fd(fd);
        }
    }
}
